/*
 * drivers/amlogic/drm/meson_gem.c
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
#include <ion/ion.h>
#include <linux/meson_ion.h>

#include "meson_drv.h"
#include "meson_gem.h"


struct meson_gem_object *meson_drm_gem_init(struct drm_device *dev,
		unsigned long size)
{
	struct meson_gem_object *meson_gem_obj;
	struct drm_gem_object *obj;
	int ret;

	meson_gem_obj = kzalloc(sizeof(*meson_gem_obj), GFP_KERNEL);
	if (!meson_gem_obj)
		return NULL;

	meson_gem_obj->size = size;
	obj = &meson_gem_obj->base;

	ret = drm_gem_object_init(dev, obj, size);
	if (ret < 0) {
		DRM_ERROR("failed to initialize gem object\n");
		kfree(meson_gem_obj);
		return NULL;
	}

	DRM_DEBUG("created file object = 0x%lx\n", (unsigned long)obj->filp);

	return meson_gem_obj;
}

static int meson_drm_alloc_buf(struct ion_client *client,
		struct meson_gem_object *meson_gem_obj, int flags)
{
	if (!client)
		return -EINVAL;

	if (!meson_gem_obj)
		return -EINVAL;

	meson_gem_obj->handle = ion_alloc(client,
			meson_gem_obj->size,
			0,
			(1 << ION_HEAP_TYPE_SYSTEM),
			0);

	if (!meson_gem_obj->handle)
		return -ENOMEM;

	return 0;
}

static struct meson_gem_object *meson_drm_gem_create(struct drm_device *dev,
		unsigned int flags,
		unsigned long size,
		struct ion_client *client)
{
	struct meson_gem_object *meson_gem_obj;
	int ret;

	if (!size) {
		DRM_ERROR("invalid size.\n");
		return ERR_PTR(-EINVAL);
	}

	size = round_up(size, PAGE_SIZE);
	meson_gem_obj = meson_drm_gem_init(dev, size);
	if (!meson_gem_obj) {
		ret = -ENOMEM;
		return ERR_PTR(ret);
	}

	ret = meson_drm_alloc_buf(client, meson_gem_obj, flags);
	if (ret < 0)
		goto err_gem_fini;

	return meson_gem_obj;

err_gem_fini:
	drm_gem_object_release(&meson_gem_obj->base);
	kfree(meson_gem_obj);
	return ERR_PTR(ret);
}

static int meson_drm_gem_handle_create(struct drm_gem_object *obj,
		struct drm_file *file_priv,
		unsigned int *handle)
{
	int ret;

	/*
	 * allocate a id of idr table where the obj is registered
	 * and handle has the id what user can see.
	 */
	ret = drm_gem_handle_create(file_priv, obj, handle);
	if (ret)
		return ret;

	DRM_DEBUG("gem handle = 0x%x\n", *handle);

	/* drop reference from allocate - handle holds it now. */
	drm_gem_object_unreference_unlocked(obj);

	return 0;
}

static void meson_drm_free_buf(struct drm_device *dev,
		struct meson_gem_object *meson_gem_obj)
{
	struct ion_client *client = NULL;

	if (meson_gem_obj->handle) {
		client = meson_gem_obj->handle->client;
		ion_free(client, meson_gem_obj->handle);
	} else {
		DRM_ERROR("meson_gem_obj handle is null\n");
	}
}

void meson_drm_gem_destroy(struct meson_gem_object *meson_gem_obj)
{
	struct drm_gem_object *obj;

	obj = &meson_gem_obj->base;

	DRM_DEBUG("handle count = %d\n", obj->handle_count);

	/*
	 * do not release memory region from exporter.
	 *
	 * the region will be released by exporter
	 * once dmabuf's refcount becomes 0.
	 */
	if (obj->import_attach)
		goto out;

	meson_drm_free_buf(obj->dev, meson_gem_obj);

out:

	drm_gem_free_mmap_offset(obj);

	/* release file pointer to gem object. */
	drm_gem_object_release(obj);

	kfree(meson_gem_obj);
	meson_gem_obj = NULL;
}

int meson_drm_gem_dumb_create(struct drm_file *file_priv,
		struct drm_device *dev, struct drm_mode_create_dumb *args)
{
	int ret = 0;
	struct meson_gem_object *meson_gem_obj;
	struct drm_meson_file_private *f = file_priv->driver_priv;

	struct ion_client *client = (struct ion_client *) f->client;
	int min_pitch = DIV_ROUND_UP(args->width * args->bpp, 8);

	args->pitch = ALIGN(min_pitch, 64);
	if (args->size < args->pitch * args->height)
		args->size = args->pitch * args->height;

	meson_gem_obj = meson_drm_gem_create(dev, 0, args->size, client);

	if (IS_ERR(meson_gem_obj))
		return PTR_ERR(meson_gem_obj);

	ret = meson_drm_gem_handle_create(&meson_gem_obj->base, file_priv,
			&args->handle);
	if (ret) {
		meson_drm_gem_destroy(meson_gem_obj);
		return ret;
	}

	return 0;
}

int meson_drm_gem_dumb_map_offset(struct drm_file *file_priv,
		struct drm_device *dev, uint32_t handle,
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

void meson_drm_gem_free_object(struct drm_gem_object *obj)
{
	struct meson_gem_object *meson_gem_obj;

	meson_gem_obj = container_of(obj, struct meson_gem_object, base);

	if (obj->import_attach)
		drm_prime_gem_destroy(obj, meson_gem_obj->sgt);

	meson_drm_gem_destroy(meson_gem_obj);
}

int meson_drm_gem_dumb_destroy(struct drm_file *file,
		struct drm_device *dev,
		uint32_t handle)
{
	struct drm_gem_object *obj;

	spin_lock(&file->table_lock);
	obj = idr_find(&file->object_idr, handle);
	if (obj == NULL) {
		spin_unlock(&file->table_lock);
		return -EINVAL;
	}
	spin_unlock(&file->table_lock);

	drm_gem_handle_delete(file, handle);

	/*
	 * we still have a reference to dma_buf because the ion_handle,
	 * when gem obj handle count become zero and refcount is 1.
	 * It will not release when dma_buf_ops.release is called.
	 * we have to drop it here.
	 * drop the reference on the export fd holds
	 */

	if (obj->handle_count == 0 &&
			atomic_read(&obj->refcount.refcount) == 1)
		drm_gem_object_unreference_unlocked(obj);

	return 0;
}

int meson_drm_gem_open(struct drm_device *dev, struct drm_file *file)
{
	struct drm_meson_file_private *file_priv;

	file_priv = kzalloc(sizeof(*file_priv), GFP_KERNEL);
	if (!file_priv) {
		DRM_ERROR("drm_meson file private alloc fail\n");
		return -ENOMEM;
	}

	file_priv->client = meson_ion_client_create(-1, "meson-gem");
	if (!file_priv->client) {
		DRM_ERROR("open ion client error\n");
		return -EFAULT;
	}

	DRM_DEBUG("open ion client: %p\n", file_priv->client);
	file->driver_priv = (void *) file_priv;
	return 0;
}

void meson_drm_gem_close(struct drm_device *dev, struct drm_file *file)
{
	struct drm_meson_file_private *file_priv = file->driver_priv;
	struct ion_client *gem_ion_client = file_priv->client;

	if (gem_ion_client) {
		DRM_DEBUG(" destroy ion client: %p\n", gem_ion_client);
		ion_client_destroy(gem_ion_client);
	}
}
