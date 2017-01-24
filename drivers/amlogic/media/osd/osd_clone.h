/*
 * drivers/amlogic/media/osd/osd_clone.h
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

#ifndef _OSD_CLONE_H_
#define _OSD_CLONE_H_

#include "osd_log.h"

#ifdef CONFIG_AMLOGIC_MEDIA_GE2D
#define OSD_GE2D_CLONE_SUPPORT 1
#endif

#ifdef OSD_GE2D_CLONE_SUPPORT
extern void osd_clone_set_virtual_yres(u32 osd1_yres, u32 osd2_yres);
extern void osd_clone_get_virtual_yres(u32 *osd2_yres);
extern void osd_clone_set_angle(int angle);
extern void osd_clone_update_pan(int buffer_number);
extern int osd_clone_task_start(void);
extern void osd_clone_task_stop(void);
#else
static inline void osd_clone_set_virtual_yres(u32 osd1_yres, u32 osd2_yres) {}
static inline void osd_clone_get_virtual_yres(u32 *osd2_yres) {}
static inline void osd_clone_set_angle(int angle) {}
static inline void osd_clone_update_pan(int buffer_number) {}
static inline int osd_clone_task_start(void)
{
	osd_log_info("++ osd_clone depends on GE2D module!\n");
	return 0;
}
static inline void osd_clone_task_stop(void)
{
	osd_log_info("-- osd_clone depends on GE2D module!\n");
}
#endif

#endif
