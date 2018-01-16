/*
 * drivers/amlogic/drm/am_meson_lcd.h
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

#ifndef __AM_DRM_LCD_H
#define __AM_DRM_LCD_H

#include "meson_drv.h"

extern int am_drm_lcd_register(struct drm_device *drm);
extern int am_drm_lcd_unregister(struct drm_device *drm);

#endif

