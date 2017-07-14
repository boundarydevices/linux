/*
 * drivers/amlogic/drm/meson_dmabuf.h
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

#ifndef _MESON_DRM_DMABUF_H_
#define _MESON_DRM_DMABUF_H_

struct dma_buf *meson_dmabuf_prime_export(struct drm_device *drm_dev,
				struct drm_gem_object *obj, int flags);

struct drm_gem_object *meson_dmabuf_prime_import(struct drm_device *drm_dev,
						struct dma_buf *dma_buf);

#endif /* MESON_DRM_DMABUF_H */
