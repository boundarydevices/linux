/*
 * drivers/amlogic/drm/am_meson_fb.h
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

#ifndef __AM_MESON_FB_H
#define __AM_MESON_FB_H
#include <drm/drmP.h>
#include <drm/drm_gem.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_modeset_helper.h>

#include "am_meson_gem.h"

struct am_meson_fb {
	struct drm_framebuffer base;
	struct am_meson_gem_object *bufp;
};

struct drm_framebuffer *am_meson_fb_create(struct drm_device *dev,
				     struct drm_file *file_priv,
				     const struct drm_mode_fb_cmd2 *mode_cmd);
struct drm_framebuffer *am_meson_drm_framebuffer_init(
				     struct drm_device *dev,
				     struct drm_mode_fb_cmd2 *mode_cmd,
				     struct drm_gem_object *obj);
#endif
