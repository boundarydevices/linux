/*
 * drivers/amlogic/drm/am_meson_gem.h
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

#ifndef __AM_MESON_GEM_H
#define __AM_MESON_GEM_H
#include  <drm/drm_gem.h>
#include <ion/ion_priv.h>
#include "meson_drv.h"

struct am_meson_gem_object {
	struct drm_gem_object base;
	unsigned int flags;

	/*for buffer create from ion heap */
	struct ion_handle *handle;
	bool bscatter;
	/*for buffer imported. */
	struct sg_table *sgt;

};

/* GEM MANAGER CREATE*/
int am_meson_gem_create(struct meson_drm  *drmdrv);

void am_meson_gem_cleanup(struct meson_drm  *drmdrv);

int am_meson_gem_mmap(
	struct file *filp,
	struct vm_area_struct *vma);

/* GEM DUMB OPERATIONS */
int am_meson_gem_dumb_create(
	struct drm_file *file_priv,
	struct drm_device *dev,
	struct drm_mode_create_dumb *args);

int am_meson_gem_dumb_destroy(
	struct drm_file *file,
	struct drm_device *dev,
	uint32_t handle);

int am_meson_gem_dumb_map_offset(
	struct drm_file *file_priv,
	struct drm_device *dev,
	uint32_t handle,
	uint64_t *offset);

/* GEM OBJECT OPERATIONS */
struct am_meson_gem_object *am_meson_gem_object_create(
		struct drm_device *dev, unsigned int flags,
		unsigned long size, struct ion_client *client);

void am_meson_gem_object_free(struct drm_gem_object *gem_obj);

int am_meson_gem_object_mmap(
	struct am_meson_gem_object *obj,
	struct vm_area_struct *vma);

int am_meson_gem_object_get_phyaddr(
	struct meson_drm *drm,
	struct am_meson_gem_object *meson_gem);

/* GEM PRIME OPERATIONS */
struct drm_gem_object *am_meson_gem_prime_import(
	struct drm_device *drm_dev,
	struct dma_buf *dma_buf);

struct dma_buf *am_meson_gem_prime_export(
	struct drm_device *drm_dev,
	struct drm_gem_object *obj,
	int flags);

#endif /* __AM_MESON_GEM_H */
