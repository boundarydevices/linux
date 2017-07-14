/*
 * drivers/amlogic/drm/meson_gem.h
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

#ifndef __MESON_GEM_H
#define __MESON_GEM_H
#include  <drm/drm_gem.h>
#include <ion/ion_priv.h>

struct drm_meson_file_private {
	struct ion_client *client;
};

struct meson_gem_object {
	struct drm_gem_object base;
	struct ion_handle *handle;
	struct sg_table *sgt;
	unsigned long size;
	unsigned int flags;
};

/* create memory region for drm */
int meson_drm_gem_dumb_create(struct drm_file *file_priv,
		 struct drm_device *dev,
		 struct drm_mode_create_dumb *args);

/* map memory region for drm to user space. */
int meson_drm_gem_dumb_map_offset(struct drm_file *file_priv,
		 struct drm_device *dev, uint32_t handle,
		 uint64_t *offset);

/* free gem object. */
void meson_drm_gem_free_object(struct drm_gem_object *gem_obj);

struct meson_gem_object *meson_drm_gem_init(struct drm_device *dev,
		 unsigned long size);

int meson_drm_gem_dumb_destroy(struct drm_file *file,
		 struct drm_device *dev,
		 uint32_t handle);

int meson_drm_gem_open(struct drm_device *dev, struct drm_file *file);
void meson_drm_gem_close(struct drm_device *dev, struct drm_file *file);

#endif /* __MESON_GEM_H */
