/*
 * drivers/amlogic/drm/am_meson_fbdev.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _MESON_DRM_FBDEV_H
#define _MESON_DRM_FBDEV_H

#ifdef CONFIG_DRM_MESON_EMULATE_FBDEV
int meson_drm_fbdev_init(struct drm_device *dev);
void meson_drm_fbdev_fini(struct drm_device *dev);
#endif

#endif /* _MESON_DRM_FBDEV_H */
