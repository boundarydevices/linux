/*
 * drivers/amlogic/media/osd/osd_antiflicker.h
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

#ifndef _OSD_ANTIFLICKER_H_
#define _OSD_ANTIFLICKER_H_

#include "osd_log.h"

#ifdef CONFIG_AMLOGIC_MEDIA_GE2D
#define OSD_GE2D_ANTIFLICKER_SUPPORT 1
#endif

#ifdef OSD_GE2D_ANTIFLICKER_SUPPORT
extern void osd_antiflicker_update_pan(u32 yoffset, u32 yres);
extern int osd_antiflicker_task_start(void);
extern void osd_antiflicker_task_stop(void);
extern void osd_antiflicker_enable(u32 enable);
#else
static inline void osd_antiflicker_enable(u32 enable) {}
static inline void osd_antiflicker_update_pan(u32 yoffset, u32 yres) {}
static inline int osd_antiflicker_task_start(void)
{
	osd_log_info("++ osd_antiflicker depends on GE2D module!\n");
	return 0;
}
static inline void osd_antiflicker_task_stop(void)
{
	osd_log_info("-- osd_antiflicker depends on GE2D module!\n");
}
#endif

#endif

