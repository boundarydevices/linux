// SPDX-License-Identifier: GPL-2.0
//
// Copyright 2020 BayLibre SAS

#include <linux/dma-map-ops.h>
#include <linux/dma-mapping.h>
#include <linux/iommu.h>
#include <linux/iova.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/remoteproc.h>

#include <drm/apu_drm.h>
#include <drm/drm_drv.h>
#include <drm/drm_gem_cma_helper.h>
#include <drm/drm_probe_helper.h>

#include <uapi/drm/apu_drm.h>

#include "apu_internal.h"

static LIST_HEAD(apu_devices);

static const struct drm_ioctl_desc ioctls[] = {
	DRM_IOCTL_DEF_DRV(APU_GEM_NEW, ioctl_gem_new,
			  DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(APU_GEM_QUEUE, ioctl_gem_queue,
			  DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(APU_GEM_DEQUEUE, ioctl_gem_dequeue,
			  DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(APU_GEM_IOMMU_MAP, ioctl_gem_iommu_map,
			  DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(APU_GEM_IOMMU_UNMAP, ioctl_gem_iommu_unmap,
			  DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(APU_STATE, ioctl_apu_state,
			  DRM_RENDER_ALLOW),
};

DEFINE_DRM_GEM_CMA_FOPS(apu_drm_ops);

static struct drm_driver apu_drm_driver = {
	.driver_features = DRIVER_GEM | DRIVER_SYNCOBJ,
	.name = "drm_apu",
	.desc = "APU DRM driver",
	.date = "20210319",
	.major = 1,
	.minor = 0,
	.patchlevel = 0,
	.ioctls = ioctls,
	.num_ioctls = ARRAY_SIZE(ioctls),
	.fops = &apu_drm_ops,
	DRM_GEM_CMA_DRIVER_OPS_WITH_DUMB_CREATE(drm_gem_cma_dumb_create),
};

void *apu_drm_priv(struct apu_core *apu_core)
{
	return apu_core->priv;
}
EXPORT_SYMBOL_GPL(apu_drm_priv);

int apu_drm_reserve_iova(struct apu_core *apu_core, u64 start, u64 size)
{
	struct apu_drm *apu_drm = apu_core->apu_drm;
	struct iova *iova;

	iova = reserve_iova(&apu_drm->iovad, PHYS_PFN(start),
			    PHYS_PFN(start + size));
	if (!iova)
		return -ENOMEM;

	return 0;
}
EXPORT_SYMBOL_GPL(apu_drm_reserve_iova);

static int apu_drm_init_first_core(struct apu_drm *apu_drm,
				   struct apu_core *apu_core)
{
	struct drm_device *drm;
	struct device *parent;
	u64 mask;

	drm = apu_drm->drm;
	parent = apu_core->rproc->dev.parent;
	drm->dev->iommu_group = parent->iommu_group;
	apu_drm->domain = iommu_get_domain_for_dev(parent);
	set_dma_ops(drm->dev, get_dma_ops(parent));
	mask = dma_get_mask(parent);
	return dma_coerce_mask_and_coherent(drm->dev, mask);
}

struct apu_core *apu_drm_register_core(struct rproc *rproc,
				       struct apu_drm_ops *ops, void *priv)
{
	struct apu_drm *apu_drm;
	struct apu_core *apu_core;
	int ret;

	list_for_each_entry(apu_drm, &apu_devices, node) {
		list_for_each_entry(apu_core, &apu_drm->apu_cores, node) {
			if (apu_core->rproc == rproc) {
				ret =
				    apu_drm_init_first_core(apu_drm, apu_core);
				apu_core->dev = &rproc->dev;
				apu_core->priv = priv;
				apu_core->ops = ops;

				ret = apu_drm_job_init(apu_core);
				if (ret)
					return NULL;

				return apu_core;
			}
		}
	}

	return NULL;
}
EXPORT_SYMBOL_GPL(apu_drm_register_core);

int apu_drm_unregister_core(void *priv)
{
	struct apu_drm *apu_drm;
	struct apu_core *apu_core;

	list_for_each_entry(apu_drm, &apu_devices, node) {
		list_for_each_entry(apu_core, &apu_drm->apu_cores, node) {
			if (apu_core->priv == priv) {
				apu_sched_fini(apu_core);
				apu_core->priv = NULL;
				apu_core->ops = NULL;
			}
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(apu_drm_unregister_core);

#ifdef CONFIG_OF
static const struct of_device_id apu_platform_of_match[] = {
	{ .compatible = "mediatek,apu-drm", },
	{ },
};

MODULE_DEVICE_TABLE(of, apu_platform_of_match);
#endif

static int apu_platform_probe(struct platform_device *pdev)
{
	struct drm_device *drm;
	struct apu_drm *apu_drm;
	struct of_phandle_iterator it;
	int index = 0;
	u64 iova[2];
	int ret;

	apu_drm = devm_kzalloc(&pdev->dev, sizeof(*apu_drm), GFP_KERNEL);
	if (!apu_drm)
		return -ENOMEM;
	INIT_LIST_HEAD(&apu_drm->apu_cores);

	of_phandle_iterator_init(&it, pdev->dev.of_node, "remoteproc", NULL, 0);
	while (of_phandle_iterator_next(&it) == 0) {
		struct rproc *rproc = rproc_get_by_phandle(it.phandle);
		struct apu_core *apu_core;

		if (!rproc)
			return -EPROBE_DEFER;

		apu_core = devm_kzalloc(&pdev->dev, sizeof(*apu_core),
					GFP_KERNEL);
		if (!apu_core)
			return -ENOMEM;

		apu_core->rproc = rproc;
		apu_core->device_id = index++;
		apu_core->apu_drm = apu_drm;
		spin_lock_init(&apu_core->ctx_lock);
		INIT_LIST_HEAD(&apu_core->requests);
		list_add(&apu_core->node, &apu_drm->apu_cores);
	}

	if (of_property_read_variable_u64_array(pdev->dev.of_node, "iova",
						iova, ARRAY_SIZE(iova),
						ARRAY_SIZE(iova)) !=
	    ARRAY_SIZE(iova))
		return -EINVAL;

	init_iova_domain(&apu_drm->iovad, PAGE_SIZE, PHYS_PFN(iova[0]));
	apu_drm->iova_limit_pfn = PHYS_PFN(iova[0] + iova[1]) - 1;

	drm = drm_dev_alloc(&apu_drm_driver, &pdev->dev);
	if (IS_ERR(drm)) {
		ret = PTR_ERR(drm);
		return ret;
	}

	ret = drm_dev_register(drm, 0);
	if (ret) {
		drm_dev_put(drm);
		return ret;
	}

	drm->dev_private = apu_drm;
	apu_drm->drm = drm;
	apu_drm->dev = &pdev->dev;

	platform_set_drvdata(pdev, drm);

	list_add(&apu_drm->node, &apu_devices);

	return 0;
}

static int apu_platform_remove(struct platform_device *pdev)
{
	struct drm_device *drm;

	drm = platform_get_drvdata(pdev);

	drm_dev_unregister(drm);
	drm_dev_put(drm);

	return 0;
}

static struct platform_driver apu_platform_driver = {
	.probe = apu_platform_probe,
	.remove = apu_platform_remove,
	.driver = {
		   .name = "apu_drm",
		   .of_match_table = of_match_ptr(apu_platform_of_match),
	},
};

module_platform_driver(apu_platform_driver);
