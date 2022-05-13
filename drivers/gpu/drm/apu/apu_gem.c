// SPDX-License-Identifier: GPL-2.0
//
// Copyright 2020 BayLibre SAS

#include <asm/cacheflush.h>

#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/highmem.h>
#include <linux/iommu.h>
#include <linux/iova.h>
#include <linux/mm.h>
#include <linux/swap.h>

#include <drm/drm_drv.h>
#include <drm/drm_gem_cma_helper.h>

#include <uapi/drm/apu_drm.h>

#include "apu_internal.h"

struct drm_gem_object *apu_gem_create_object(struct drm_device *dev,
					     size_t size)
{
	struct drm_gem_cma_object *cma_obj;

	cma_obj = drm_gem_cma_create(dev, size);
	if (!cma_obj)
		return NULL;

	return &cma_obj->base;
}

int ioctl_gem_new(struct drm_device *dev, void *data,
		  struct drm_file *file_priv)
{
	struct drm_apu_gem_new *args = data;
	struct drm_gem_cma_object *cma_obj;
	struct apu_gem_object *apu_obj;
	struct drm_gem_object *gem_obj;
	int ret;

	cma_obj = drm_gem_cma_create(dev, args->size);
	if (IS_ERR(cma_obj))
		return PTR_ERR(cma_obj);

	gem_obj = &cma_obj->base;
	apu_obj = to_apu_bo(gem_obj);

	/*
	 * Save the size of buffer expected by application instead of the
	 * aligned one.
	 */
	apu_obj->size = args->size;
	apu_obj->offset = 0;
	apu_obj->iommu_refcount = 0;
	mutex_init(&apu_obj->mutex);

	ret = drm_gem_handle_create(file_priv, gem_obj, &args->handle);
	drm_gem_object_put(gem_obj);
	if (ret) {
		drm_gem_cma_free(cma_obj);
		return ret;
	}
	args->offset = drm_vma_node_offset_addr(&gem_obj->vma_node);

	return 0;
}

void apu_bo_iommu_unmap(struct apu_drm *apu_drm, struct apu_gem_object *obj)
{
	int iova_pfn;
	int i;

	if (!obj->iommu_sgt)
		return;

	mutex_lock(&obj->mutex);
	obj->iommu_refcount--;
	if (obj->iommu_refcount) {
		mutex_unlock(&obj->mutex);
		return;
	}

	iova_pfn = PHYS_PFN(obj->iova);
	for (i = 0; i < obj->iommu_sgt->nents; i++) {
		iommu_unmap(apu_drm->domain, PFN_PHYS(iova_pfn),
			    PAGE_ALIGN(obj->iommu_sgt->sgl[i].length));
		iova_pfn += PHYS_PFN(PAGE_ALIGN(obj->iommu_sgt->sgl[i].length));
	}

	sg_free_table(obj->iommu_sgt);
	kfree(obj->iommu_sgt);

	free_iova(&apu_drm->iovad, PHYS_PFN(obj->iova));
	mutex_unlock(&obj->mutex);
}

static struct sg_table *apu_get_sg_table(struct drm_gem_object *obj)
{
	if (obj->funcs)
		return obj->funcs->get_sg_table(obj);
	return NULL;
}

int apu_bo_iommu_map(struct apu_drm *apu_drm, struct drm_gem_object *obj)
{
	struct apu_gem_object *apu_obj = to_apu_bo(obj);
	struct scatterlist *sgl;
	phys_addr_t phys;
	int total_buf_space;
	int iova_pfn;
	int iova;
	int ret;
	int i;

	mutex_lock(&apu_obj->mutex);
	apu_obj->iommu_refcount++;
	if (apu_obj->iommu_refcount != 1) {
		mutex_unlock(&apu_obj->mutex);
		return 0;
	}

	apu_obj->iommu_sgt = apu_get_sg_table(obj);
	if (IS_ERR(apu_obj->iommu_sgt)) {
		mutex_unlock(&apu_obj->mutex);
		return PTR_ERR(apu_obj->iommu_sgt);
	}

	total_buf_space = obj->size;
	iova_pfn = alloc_iova_fast(&apu_drm->iovad,
				   total_buf_space >> PAGE_SHIFT,
				   apu_drm->iova_limit_pfn, true);
	apu_obj->iova = PFN_PHYS(iova_pfn);

	if (!iova_pfn) {
		dev_err(apu_drm->dev, "Failed to allocate iova address\n");
		mutex_unlock(&apu_obj->mutex);
		return -ENOMEM;
	}

	iova = apu_obj->iova;
	sgl = apu_obj->iommu_sgt->sgl;
	for (i = 0; i < apu_obj->iommu_sgt->nents; i++) {
		phys = page_to_phys(sg_page(&sgl[i]));
		ret =
		    iommu_map(apu_drm->domain, PFN_PHYS(iova_pfn), phys,
			      PAGE_ALIGN(sgl[i].length),
			      IOMMU_READ | IOMMU_WRITE);
		if (ret) {
			dev_err(apu_drm->dev, "Failed to iommu map\n");
			free_iova(&apu_drm->iovad, iova_pfn);
			mutex_unlock(&apu_obj->mutex);
			return ret;
		}
		iova += sgl[i].offset + sgl[i].length;
		iova_pfn += PHYS_PFN(PAGE_ALIGN(sgl[i].length));
	}
	mutex_unlock(&apu_obj->mutex);

	return 0;
}

int ioctl_gem_iommu_map(struct drm_device *dev, void *data,
			struct drm_file *file_priv)
{
	struct apu_drm *apu_drm = dev->dev_private;
	struct drm_apu_gem_iommu_map *args = data;
	struct drm_gem_object **bos;
	void __user *bo_handles;
	int ret;
	int i;

	u64 *das = kvmalloc_array(args->bo_handle_count,
				  sizeof(u64), GFP_KERNEL);
	if (!das)
		return -ENOMEM;

	bo_handles = (void __user *)(uintptr_t) args->bo_handles;
	ret = drm_gem_objects_lookup(file_priv, bo_handles,
				     args->bo_handle_count, &bos);
	if (ret) {
		kvfree(das);
		return ret;
	}

	for (i = 0; i < args->bo_handle_count; i++) {
		ret = apu_bo_iommu_map(apu_drm, bos[i]);
		if (ret) {
			/* TODO: handle error */
			break;
		}
		das[i] = to_apu_bo(bos[i])->iova + to_apu_bo(bos[i])->offset;
	}

	if (copy_to_user((void *)args->bo_device_addresses, das,
			 args->bo_handle_count * sizeof(u64))) {
		ret = -EFAULT;
		DRM_DEBUG("Failed to copy device addresses\n");
		goto out;
	}

out:
	kvfree(das);
	kvfree(bos);

	return 0;
}

int ioctl_gem_iommu_unmap(struct drm_device *dev, void *data,
			  struct drm_file *file_priv)
{
	struct apu_drm *apu_drm = dev->dev_private;
	struct drm_apu_gem_iommu_map *args = data;
	struct drm_gem_object **bos;
	void __user *bo_handles;
	int ret;
	int i;

	bo_handles = (void __user *)(uintptr_t) args->bo_handles;
	ret = drm_gem_objects_lookup(file_priv, bo_handles,
				     args->bo_handle_count, &bos);
	if (ret)
		return ret;

	for (i = 0; i < args->bo_handle_count; i++)
		apu_bo_iommu_unmap(apu_drm, to_apu_bo(bos[i]));

	kvfree(bos);

	return 0;
}
