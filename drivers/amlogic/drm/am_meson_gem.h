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
#include <drm/drm_gem.h>
#include <linux/amlogic/meson_drm.h>
#include <ion/ion_priv.h>

#include "am_meson_drv.h"

struct am_meson_gem_object {
	struct drm_gem_object base;
	unsigned int flags;

	/*for buffer create from ion heap */
	struct ion_handle *handle;
	bool bscatter;
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

int am_meson_gem_create_ioctl(
	struct drm_device *dev,
	void *data,
	struct drm_file *file_priv);

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

extern int am_meson_gem_object_get_phyaddr(
	struct meson_drm *drm,
	struct am_meson_gem_object *meson_gem);

/* GEM PRIME OPERATIONS */
struct sg_table *am_meson_gem_prime_get_sg_table(
	struct drm_gem_object *obj);

struct drm_gem_object *am_meson_gem_prime_import_sg_table(
	struct drm_device *dev,
	struct dma_buf_attachment *attach,
	struct sg_table *sgt);

void *am_meson_gem_prime_vmap(
	struct drm_gem_object *obj);

void am_meson_gem_prime_vunmap(
	struct drm_gem_object *obj,
	void *vaddr);

int am_meson_gem_prime_mmap(
	struct drm_gem_object *obj,
	struct vm_area_struct *vma);

#endif /* __AM_MESON_GEM_H */
