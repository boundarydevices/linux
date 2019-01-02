/*
 * include/linux/amlogic/media/camera/flashlight.h
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

#ifndef _VIDEO_AMLOGIC_FLASHLIGHT_INCLUDE_
#define _VIDEO_AMLOGIC_FLASHLIGHT_INCLUDE_
struct aml_plat_flashlight_data_s {
	void (*flashlight_on)(void);
	void (*flashlight_off)(void);
};

enum aml_plat_flashlight_status_s {
	FLASHLIGHT_AUTO = 0,
	FLASHLIGHT_ON,
	FLASHLIGHT_OFF,
	FLASHLIGHT_TORCH,
	FLASHLIGHT_RED_EYE,
};

#endif

