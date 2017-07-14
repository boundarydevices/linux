/*
 * drivers/amlogic/drm/meson_dmabuf.c
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
#include <linux/dma-buf.h>
#include <ion/ion.h>

#include "meson_gem.h"
#include "meson_dmabuf.h"

struct dma_buf *meson_dmabuf_prime_export(struct drm_device *drm_dev,
				struct drm_gem_object *obj, int flags)
{
	struct meson_gem_object *meson_gem_obj;
	struct ion_handle *handle;

	meson_gem_obj = container_of(obj, struct meson_gem_object, base);
	handle = meson_gem_obj->handle;

	if (!handle)
		return ERR_PTR(-EINVAL);

	return ion_share_dma_buf(handle->client, handle);
}

struct drm_gem_object *meson_dmabuf_prime_import(struct drm_device *drm_dev,
						struct dma_buf *dma_buf)
{
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	struct meson_gem_object *meson_gem_obj;
	int ret;

	attach = dma_buf_attach(dma_buf, drm_dev->dev);
	if (IS_ERR(attach))
		return ERR_PTR(-EINVAL);

	get_dma_buf(dma_buf);

	sgt = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	if (IS_ERR_OR_NULL(sgt)) {
		ret = PTR_ERR(sgt);
		goto err_buf_detach;
	}

	meson_gem_obj = meson_drm_gem_init(drm_dev, dma_buf->size);
	if (!meson_gem_obj) {
		ret = -ENOMEM;
		goto err_unmap_attach;
	}

	meson_gem_obj->sgt = sgt;
	meson_gem_obj->base.import_attach = attach;

	return &meson_gem_obj->base;

err_unmap_attach:
	dma_buf_unmap_attachment(attach, sgt, DMA_BIDIRECTIONAL);
err_buf_detach:
	dma_buf_detach(dma_buf, attach);
	dma_buf_put(dma_buf);

	return ERR_PTR(ret);
}
