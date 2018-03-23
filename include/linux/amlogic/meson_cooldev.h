/*
 * include/linux/amlogic/meson_cooldev.h
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

#ifndef __MESON_COOLDEV_H__
#define __MESON_COOLDEV_H__

#ifndef mc_capable
#define mc_capable()		0
#endif

struct thermal_cooling_device;
extern int meson_gcooldev_min_update(struct thermal_cooling_device *cdev);
#endif

