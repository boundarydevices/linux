/*
 * drivers/amlogic/media/video_processor/ppmgr/ppmgr_3d_tv.h
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

#ifndef _PPMGR_3D_TV_H_
#define _PPMGR_3D_TV_H_
extern struct vfq_s q_ready;
extern struct vfq_s q_free;

extern int get_ppmgr_vertical_sample(void);
extern int get_ppmgr_scale_width(void);
extern int get_ppmgr_view_mode(void);
extern u32 index2canvas(u32 index);
extern void ppmgr_vf_put_dec(struct vframe_s *vf);

static int cur_process_type;
extern int ppmgr_cutwin_top;
extern int ppmgr_cutwin_left;
extern struct frame_info_t frame_info;

extern int get_depth(void);
#endif

