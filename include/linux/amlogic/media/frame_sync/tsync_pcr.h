/*
 * include/linux/amlogic/media/frame_sync/tsync_pcr.h
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

#ifndef TSYNC_PCR_H
#define TSYNC_PCR_H
#include <linux/amlogic/media/frame_sync/tsync.h>
extern void tsync_pcr_avevent_locked(enum avevent_e event, u32 param);

extern int tsync_pcr_start(void);

extern void tsync_pcr_stop(void);

extern int tsync_pcr_set_apts(unsigned int pts);

extern int get_vsync_pts_inc_mode(void);

int tsync_pcr_init(void);
void tsync_pcr_exit(void);
extern int tsync_pcr_demux_pcr_used(void);

#endif
