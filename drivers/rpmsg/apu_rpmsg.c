// SPDX-License-Identifier: GPL-2.0
//
// Copyright 2020 BayLibre SAS

#include <asm/cacheflush.h>

#include <linux/cdev.h>
#include <linux/dma-buf.h>
#include <linux/dma-map-ops.h>
#include <linux/dma-mapping.h>
#include <linux/iommu.h>
#include <linux/iova.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/remoteproc.h>
#include <linux/rpmsg.h>
#include <linux/slab.h>
#include <linux/types.h>

#include <drm/apu_drm.h>

#include "rpmsg_internal.h"

#define APU_RPMSG_SERVICE_MT8183 "rpmsg-mt8183-apu0"

struct rpmsg_apu {
	struct apu_core *core;
	struct rpmsg_device *rpdev;
};

static int apu_rpmsg_callback(struct rpmsg_device *rpdev, void *data, int count,
			      void *priv, u32 addr)
{
	struct rpmsg_apu *apu = dev_get_drvdata(&rpdev->dev);
	struct apu_core *apu_core = apu->core;

	return apu_drm_callback(apu_core, data, count);
}

static int apu_rpmsg_send(struct apu_core *apu_core, void *data, int len)
{
	struct rpmsg_apu *apu = apu_drm_priv(apu_core);
	struct rpmsg_device *rpdev = apu->rpdev;

	return rpmsg_send(rpdev->ept, data, len);
}

static struct apu_drm_ops apu_rpmsg_ops = {
	.send = apu_rpmsg_send,
};

static int apu_init_iovad(struct rproc *rproc, struct rpmsg_apu *apu)
{
	struct resource_table *table;
	struct fw_rsc_carveout *rsc;
	int i;

	if (!rproc->table_ptr) {
		dev_err(&apu->rpdev->dev,
			"No resource_table: has the firmware been loaded ?\n");
		return -ENODEV;
	}

	table = rproc->table_ptr;
	for (i = 0; i < table->num; i++) {
		int offset = table->offset[i];
		struct fw_rsc_hdr *hdr = (void *)table + offset;

		if (hdr->type != RSC_CARVEOUT)
			continue;

		rsc = (void *)hdr + sizeof(*hdr);
		if (apu_drm_reserve_iova(apu->core, rsc->da, rsc->len)) {
			dev_err(&apu->rpdev->dev,
				"failed to reserve iova\n");
			return -ENOMEM;
		}
	}

	return 0;
}

static struct rproc *apu_get_rproc(struct rpmsg_device *rpdev)
{
	/*
	 * To work, the APU RPMsg driver need to get the rproc device.
	 * Currently, we only use virtio so we could use that to find the
	 * remoteproc parent.
	 */
	if (!rpdev->dev.parent && rpdev->dev.parent->bus) {
		dev_err(&rpdev->dev, "invalid rpmsg device\n");
		return ERR_PTR(-EINVAL);
	}

	if (strcmp(rpdev->dev.parent->bus->name, "virtio")) {
		dev_err(&rpdev->dev, "unsupported bus\n");
		return ERR_PTR(-EINVAL);
	}

	return vdev_to_rproc(dev_to_virtio(rpdev->dev.parent));
}

static int apu_rpmsg_probe(struct rpmsg_device *rpdev)
{
	struct rpmsg_apu *apu;
	struct rproc *rproc;
	int ret;

	apu = devm_kzalloc(&rpdev->dev, sizeof(*apu), GFP_KERNEL);
	if (!apu)
		return -ENOMEM;
	apu->rpdev = rpdev;

	rproc = apu_get_rproc(rpdev);
	if (IS_ERR_OR_NULL(rproc))
		return PTR_ERR(rproc);

	/* Make device dma capable by inheriting from parent's capabilities */
	set_dma_ops(&rpdev->dev, get_dma_ops(rproc->dev.parent));

	ret = dma_coerce_mask_and_coherent(&rpdev->dev,
					   dma_get_mask(rproc->dev.parent));
	if (ret)
		goto err_put_device;

	rpdev->dev.iommu_group = rproc->dev.parent->iommu_group;

	apu->core = apu_drm_register_core(rproc, &apu_rpmsg_ops, apu);
	if (!apu->core) {
		ret = -ENODEV;
		goto err_put_device;
	}

	ret = apu_init_iovad(rproc, apu);

	dev_set_drvdata(&rpdev->dev, apu);

	return ret;

err_put_device:
	devm_kfree(&rpdev->dev, apu);

	return ret;
}

static void apu_rpmsg_remove(struct rpmsg_device *rpdev)
{
	struct rpmsg_apu *apu = dev_get_drvdata(&rpdev->dev);

	apu_drm_unregister_core(apu);
	devm_kfree(&rpdev->dev, apu);
}

static const struct rpmsg_device_id apu_rpmsg_match[] = {
	{ APU_RPMSG_SERVICE_MT8183 },
	{}
};

static struct rpmsg_driver apu_rpmsg_driver = {
	.probe = apu_rpmsg_probe,
	.remove = apu_rpmsg_remove,
	.callback = apu_rpmsg_callback,
	.id_table = apu_rpmsg_match,
	.drv  = {
		.name  = "apu_rpmsg",
	},
};

static int __init apu_rpmsg_init(void)
{
	return register_rpmsg_driver(&apu_rpmsg_driver);
}
arch_initcall(apu_rpmsg_init);

static void __exit apu_rpmsg_exit(void)
{
	unregister_rpmsg_driver(&apu_rpmsg_driver);
}
module_exit(apu_rpmsg_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("APU RPMSG driver");
