/*
 * drivers/amlogic/media/osd_ext/osd_clone.h
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

#include <osd/osd_log.h>

#ifdef CONFIG_AMLOGIC_MEDIA_GE2D
#define OSD_EXT_GE2D_CLONE_SUPPORT 1
#endif

#ifdef OSD_EXT_GE2D_CLONE_SUPPORT
extern void osd_ext_clone_set_angle(int angle);
extern void osd_ext_clone_update_pan(int pan);
extern int osd_ext_clone_task_start(void);
extern void osd_ext_clone_task_stop(void);
#else
static inline void osd_ext_clone_set_angle(int angle) {}
static inline void osd_ext_clone_update_pan(int pan) {}
static inline int osd_ext_clone_task_start(void)
{
	osd_log_info("++ osd_ext_clone depends on GE2D module!\n");
	return 0;
}
static inline void osd_ext_clone_task_stop(void)
{
	osd_log_info("-- osd_ext_clone depends on GE2D module!\n");
}
#endif

#endif
