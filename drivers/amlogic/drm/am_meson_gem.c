/*
 * drivers/amlogic/drm/am_meson_gem.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */
#include <drm/drmP.h>
#include <drm/drm_gem.h>
#include <drm/drm_vma_manager.h>

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/dma-buf.h>
#include <linux/meson_ion.h>
#include <ion/ion.h>

#include "am_meson_gem.h"

#define to_am_meson_gem_obj(x) container_of(x, struct am_meson_gem_object, base)

static int am_meson_gem_alloc_ion_buff(
	struct ion_client *client,
	struct am_meson_gem_object *meson_gem_obj,
	int flags)
{
	struct ion_handle *handle;
	bool bscatter = false;

	if (!client)
		return -EINVAL;

	if (!meson_gem_obj)
		return -EINVAL;

	//check flags to set different ion heap type.
	//if flags is set to 0, need to use ion dma buffer.
	if (((flags & (BO_USE_SCANOUT | BO_USE_CURSOR)) != 0)
		|| (flags == 0)) {
		handle = ion_alloc(client, meson_gem_obj->base.size,
				0, (1 << ION_HEAP_TYPE_DMA), 0);
	}
	else {
		handle = ion_alloc(client, meson_gem_obj->base.size,
					0, (1 << ION_HEAP_TYPE_SYSTEM), 0);
		bscatter = true;
	}

	if (IS_ERR(handle)) {
		DRM_ERROR("%s: FAILED, flags:0x%x.\n",
			__func__, flags);
		return -ENOMEM;
	}

	meson_gem_obj->handle = handle;
	meson_gem_obj->bscatter = bscatter;
	DRM_DEBUG("%s: allocate handle (%p).\n",
		__func__, meson_gem_obj->handle);
	return 0;
}

static void am_meson_gem_free_ion_buf(
	struct drm_device *dev,
	struct am_meson_gem_object *meson_gem_obj)
{
	struct ion_client *client = NULL;

	if (meson_gem_obj->handle) {
		DRM_DEBUG("am_meson_gem_free_ion_buf free handle  (%p).\n",
			meson_gem_obj->handle);
		client = meson_gem_obj->handle->client;
		ion_free(client, meson_gem_obj->handle);
		meson_gem_obj->handle = NULL;
	} else {
		DRM_ERROR("meson_gem_obj handle is null\n");
	}
}

struct am_meson_gem_object *am_meson_gem_object_create(
	struct drm_device *dev,
	unsigned int flags,
	unsigned long size,
	struct ion_client *client)
{
	struct am_meson_gem_object *meson_gem_obj = NULL;
	int ret;

	if (!size) {
		DRM_ERROR("invalid size.\n");
		return ERR_PTR(-EINVAL);
	}

	meson_gem_obj = kzalloc(sizeof(*meson_gem_obj), GFP_KERNEL);
	if (!meson_gem_obj)
		return ERR_PTR(-ENOMEM);

	ret = drm_gem_object_init(dev, &meson_gem_obj->base, size);
	if (ret < 0) {
		DRM_ERROR("failed to initialize gem object\n");
		goto error;
	}

	ret = am_meson_gem_alloc_ion_buff(client, meson_gem_obj, flags);
	if (ret < 0) {
		drm_gem_object_release(&meson_gem_obj->base);
		goto error;
	}

	return meson_gem_obj;

error:
	kfree(meson_gem_obj);
	return ERR_PTR(ret);
}

void am_meson_gem_object_free(struct drm_gem_object *obj)
{
	struct am_meson_gem_object *meson_gem_obj = to_am_meson_gem_obj(obj);

	DRM_DEBUG("am_meson_gem_object_free %p handle count = %d\n",
		meson_gem_obj, obj->handle_count);

	if (obj->import_attach == false)
		am_meson_gem_free_ion_buf(obj->dev, meson_gem_obj);
	else
		DRM_ERROR("Not support import buffer from other driver.\n");

	drm_gem_free_mmap_offset(obj);

	/* release file pointer to gem object. */
	drm_gem_object_release(obj);

	kfree(meson_gem_obj);
	meson_gem_obj = NULL;
}

int am_meson_gem_object_mmap(
	struct am_meson_gem_object *obj,
	struct vm_area_struct *vma)
{
	int ret = 0;
	struct ion_buffer *buffer;

	/*
	 * Clear the VM_PFNMAP flag that was set by drm_gem_mmap(), and set the
	 * vm_pgoff (used as a fake buffer offset by DRM) to 0 as we want to map
	 * the whole buffer.
	 */
	vma->vm_flags &= ~VM_PFNMAP;
	vma->vm_pgoff = 0;

	if (obj->base.import_attach) {
		DRM_ERROR("Not support import buffer from other driver.\n");
	} else {
		buffer = obj->handle->buffer;

		if (!buffer->heap->ops->map_user) {
			DRM_ERROR("%s:heap does not define map to userspace\n",
			       __func__);
			ret = -EINVAL;
		} else {

			if (!(buffer->flags & ION_FLAG_CACHED))
				vma->vm_page_prot =
					pgprot_writecombine(vma->vm_page_prot);

			mutex_lock(&buffer->lock);
			/* now map it to userspace */
			ret = buffer->heap->ops->map_user(
						buffer->heap, buffer, vma);
			mutex_unlock(&buffer->lock);
		}
	}

	if (ret) {
		DRM_ERROR("%s: failure mapping buffer to userspace (%d)\n",
		       __func__, ret);
		drm_gem_vm_close(vma);
	}

	return ret;
}

int am_meson_gem_mmap(
	struct file *filp,
	struct vm_area_struct *vma)
{
	struct drm_gem_object *obj;
	struct am_meson_gem_object *meson_gem_obj;
	int ret;

	ret = drm_gem_mmap(filp, vma);
	if (ret)
		return ret;

	obj = vma->vm_private_data;
	meson_gem_obj = to_am_meson_gem_obj(obj);
	DRM_DEBUG("am_meson_gem_mmap %p.\n", meson_gem_obj);

	ret = am_meson_gem_object_mmap(meson_gem_obj, vma);

	return ret;
}

int am_meson_gem_object_get_phyaddr(
	struct meson_drm *drm,
	struct am_meson_gem_object *meson_gem)
{
	int addr;
	size_t len;

	if (!meson_gem->handle) {
		DRM_INFO("%s handle null\n", __func__);
		return -1;
	}

	ion_phys(drm->gem_client, meson_gem->handle,
						(ion_phys_addr_t *)&addr, &len);

	return addr;
}
EXPORT_SYMBOL(am_meson_gem_object_get_phyaddr);

int am_meson_gem_dumb_create(
	struct drm_file *file_priv,
	struct drm_device *dev,
	struct drm_mode_create_dumb *args)
{
	int ret = 0;
	struct am_meson_gem_object *meson_gem_obj;
	struct meson_drm *drmdrv = dev->dev_private;
	struct ion_client *client = (struct ion_client *)drmdrv->gem_client;
	int min_pitch = DIV_ROUND_UP(args->width * args->bpp, 8);

	args->pitch = ALIGN(min_pitch, 64);
	if (args->size < args->pitch * args->height)
		args->size = args->pitch * args->height;

	args->size = round_up(args->size, PAGE_SIZE);

	meson_gem_obj = am_meson_gem_object_create(
					dev, args->flags, args->size, client);
	if (IS_ERR(meson_gem_obj))
		return PTR_ERR(meson_gem_obj);

	/*
	 * allocate a id of idr table where the obj is registered
	 * and handle has the id what user can see.
	 */
	ret = drm_gem_handle_create(file_priv,
			&meson_gem_obj->base, &args->handle);
	/* drop reference from allocate - handle holds it now. */
	drm_gem_object_unreference_unlocked(&meson_gem_obj->base);
	if (ret) {
		DRM_ERROR("%s: create dumb handle failed %d\n",
			__func__, ret);
		return ret;
	}

	DRM_DEBUG("%s: create dumb %p  with gem handle (0x%x)\n",
		__func__, meson_gem_obj, args->handle);
	return 0;
}

int am_meson_gem_dumb_destroy(
	struct drm_file *file,
	struct drm_device *dev,
	uint32_t handle)
{
	DRM_DEBUG("%s: destroy dumb with handle (0x%x)\n", __func__, handle);
	drm_gem_handle_delete(file, handle);
	return 0;
}

int am_meson_gem_dumb_map_offset(
	struct drm_file *file_priv,
	struct drm_device *dev,
	uint32_t handle,
	uint64_t *offset)
{
	struct drm_gem_object *obj;
	int ret = 0;

	mutex_lock(&dev->struct_mutex);

	/*
	 * get offset of memory allocated for drm framebuffer.
	 * - this callback would be called by user application
	 *	with DRM_IOCTL_MODE_MAP_DUMB command.
	 */
	obj = drm_gem_object_lookup(file_priv, handle);
	if (!obj) {
		DRM_ERROR("failed to lookup gem object.\n");
		ret = -EINVAL;
		goto unlock;
	}

	ret = drm_gem_create_mmap_offset(obj);
	if (ret)
		goto out;

	*offset = drm_vma_node_offset_addr(&obj->vma_node);
	DRM_DEBUG("offset = 0x%lx\n", (unsigned long)*offset);

out:
	drm_gem_object_unreference(obj);
unlock:
	mutex_unlock(&dev->struct_mutex);
	return ret;
}

int am_meson_gem_create_ioctl(
	struct drm_device *dev,
	void *data,
	struct drm_file *file_priv)
{
	struct am_meson_gem_object *meson_gem_obj;
	struct meson_drm *drmdrv = dev->dev_private;
	struct ion_client *client = (struct ion_client *)drmdrv->gem_client;
	struct drm_meson_gem_create *args = data;
	int ret = 0;

	meson_gem_obj = am_meson_gem_object_create(
					dev, args->flags, args->size, client);
	if (IS_ERR(meson_gem_obj))
		return PTR_ERR(meson_gem_obj);

	/*
	 * allocate a id of idr table where the obj is registered
	 * and handle has the id what user can see.
	 */
	ret = drm_gem_handle_create(file_priv,
			&meson_gem_obj->base, &args->handle);
	/* drop reference from allocate - handle holds it now. */
	drm_gem_object_unreference_unlocked(&meson_gem_obj->base);
	if (ret) {
		DRM_ERROR("%s: create dumb handle failed %d\n",
			__func__, ret);
		return ret;
	}

	DRM_DEBUG("%s: create dumb %p  with gem handle (0x%x)\n",
		__func__, meson_gem_obj, args->handle);
	return 0;
}


int am_meson_gem_create(struct meson_drm  *drmdrv)
{
	drmdrv->gem_client = meson_ion_client_create(-1, "meson-gem");
	if (!drmdrv->gem_client) {
		DRM_ERROR("open ion client error\n");
		return -EFAULT;
	}

	DRM_DEBUG("open ion client: %p\n", drmdrv->gem_client);
	return 0;
}

void am_meson_gem_cleanup(struct meson_drm  *drmdrv)
{
	struct ion_client *gem_ion_client = drmdrv->gem_client;

	if (gem_ion_client) {
		DRM_DEBUG(" destroy ion client: %p\n", gem_ion_client);
		ion_client_destroy(gem_ion_client);
	}
}

struct sg_table *am_meson_gem_prime_get_sg_table(
	struct drm_gem_object *obj)
{
	struct am_meson_gem_object *meson_gem_obj;
	struct sg_table *dst_table = NULL;
	struct scatterlist *dst_sg = NULL;
	struct sg_table *src_table = NULL;
	struct scatterlist *src_sg = NULL;
	int ret, i;

	meson_gem_obj = to_am_meson_gem_obj(obj);
	DRM_DEBUG("am_meson_gem_prime_get_sg_table %p.\n", meson_gem_obj);

	if (meson_gem_obj->base.import_attach == false) {
		src_table = meson_gem_obj->handle->buffer->sg_table;
		dst_table = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
		if (!dst_table) {
			ret = -ENOMEM;
			return ERR_PTR(ret);
		}

		ret = sg_alloc_table(dst_table, src_table->nents, GFP_KERNEL);
		if (ret) {
			kfree(dst_table);
			return ERR_PTR(ret);
		}

		dst_sg = dst_table->sgl;
		src_sg = src_table->sgl;
		for (i = 0; i < src_table->nents; i++) {
			sg_set_page(dst_sg, sg_page(src_sg), src_sg->length, 0);
			dst_sg = sg_next(dst_sg);
			src_sg = sg_next(src_sg);
		}
		return dst_table;
	}
	else {
		DRM_ERROR("Not support import buffer from other driver.\n");
		return NULL;
	}
}

struct drm_gem_object *am_meson_gem_prime_import_sg_table(
	struct drm_device *dev,
	struct dma_buf_attachment *attach,
	struct sg_table *sgt)
{
	struct am_meson_gem_object *meson_gem_obj;
	int ret;

	meson_gem_obj = kzalloc(sizeof(*meson_gem_obj), GFP_KERNEL);
	if (!meson_gem_obj)
		return ERR_PTR(-ENOMEM);

	ret = drm_gem_object_init(dev,
			&meson_gem_obj->base,
			attach->dmabuf->size);
	if (ret < 0) {
		DRM_ERROR("failed to initialize gem object\n");
		kfree(meson_gem_obj);
		return  ERR_PTR(-ENOMEM);
	}

	DRM_DEBUG("%s: %p, sg_table %p\n", __func__, meson_gem_obj, sgt);
	/*meson_gem_obj->sgt = sgt;*/
	return &meson_gem_obj->base;
}

void *am_meson_gem_prime_vmap(struct drm_gem_object *obj)
{
	DRM_DEBUG("am_meson_gem_prime_vmap %p.\n", obj);

	return NULL;
}

void am_meson_gem_prime_vunmap(
	struct drm_gem_object *obj,
	void *vaddr)
{
	DRM_DEBUG("am_meson_gem_prime_vunmap nothing to do.\n");
}

int am_meson_gem_prime_mmap(
	struct drm_gem_object *obj,
	struct vm_area_struct *vma)
{
	struct am_meson_gem_object *meson_gem_obj;
	int ret;

	ret = drm_gem_mmap_obj(obj, obj->size, vma);
	if (ret < 0)
		return ret;

	meson_gem_obj = to_am_meson_gem_obj(obj);
	DRM_DEBUG("am_meson_gem_prime_mmap %p.\n", meson_gem_obj);

	return am_meson_gem_object_mmap(meson_gem_obj, vma);
}
