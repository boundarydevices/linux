/*
 * drivers/amlogic/media/osd_ext/osd_hw.h
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

#ifndef _OSD_HW_H_
#define _OSD_HW_H_

#include <osd/osd.h>

#define REG_OFFSET 0x20
#define	OSD_RELATIVE_BITS 0x333f0
extern void osd_ext_set_color_key_hw(u32 index, u32 bpp, u32 colorkey);
extern void osd_ext_srckey_enable_hw(u32 index, u8 enable);
extern void osd_ext_set_gbl_alpha_hw(u32 index, u32 gbl_alpha);
extern u32 osd_ext_get_gbl_alpha_hw(u32 index);
extern void osd_ext_setup(struct osd_ctl_s *osd_ext_ctl,
			  u32 xoffset,
			  u32 yoffset,
			  u32 xres,
			  u32 yres,
			  u32 xres_virtual,
			  u32 yres_virtual,
			  u32 disp_start_x,
			  u32 disp_start_y,
			  u32 disp_end_x,
			  u32 disp_end_y,
			  u32 fbmem,
			  const struct color_bit_define_s *color,
			  int index);
extern void osd_ext_update_disp_axis_hw(
	u32 index,
	u32 display_h_start,
	u32 display_h_end,
	u32 display_v_start,
	u32 display_v_end,
	u32 xoffset,
	u32 yoffset,
	u32 mode_change);
extern void osd_ext_set_order_hw(u32 index, u32 order);
extern u32 osd_ext_get_order_hw(u32 index);
extern void osd_ext_set_free_scale_enable_hw(u32 index, u32 enable);
extern void osd_ext_get_free_scale_enable_hw(u32 index,
		u32 *free_scale_enable);
extern void osd_ext_get_free_scale_width_hw(u32 index, u32 *free_scale_width);
extern void osd_ext_get_free_scale_height_hw(u32 index,
		u32 *free_scale_height);
extern void osd_ext_get_free_scale_axis_hw(u32 index, s32 *x0, s32 *y0, s32 *x1,
		s32 *y1);
extern void osd_ext_set_free_scale_axis_hw(u32 index, s32 x0, s32 y0, s32 x1,
		s32 y1);
extern void osd_ext_get_scale_axis_hw(u32 index, s32 *x0, s32 *y0, s32 *x1,
				      s32 *y1);
extern void osd_ext_set_scale_axis_hw(u32 index, s32 x0, s32 y0, s32 x1,
				      s32 y1);
extern void osd_ext_get_window_axis_hw(u32 index, s32 *x0, s32 *y0, s32 *x1,
				       s32 *y1);
extern void osd_ext_set_window_axis_hw(u32 index, s32 x0, s32 y0, s32 x1,
				       s32 y1);
extern void osd_ext_get_free_scale_mode_hw(u32 index, u32 *freescale_mode);
extern void osd_ext_set_free_scale_mode_hw(u32 index, u32 freescale_mode);
extern void osd_ext_set_debug_hw(u32 index, u32 flag);
extern void osd_ext_get_block_windows_hw(u32 index, u32 *windows);
extern void osd_ext_set_block_windows_hw(u32 index, u32 *windows);
extern void osd_ext_get_block_mode_hw(u32 index, u32 *mode);
extern void osd_ext_set_block_mode_hw(u32 index, u32 mode);
extern void osd_ext_enable_3d_mode_hw(int index, int enable);
extern void osd_ext_set_2x_scale_hw(u32 index, u16 h_scale_enable,
				    u16 v_scale_enable);
extern void osd_ext_setpal_hw(u32 index, unsigned int regno,
				unsigned int red, unsigned int green,
				unsigned int blue, unsigned int transp);
extern void osd_ext_enable_hw(u32 index, int enable);
extern void osd_ext_pan_display_hw(u32 index, unsigned int xoffset,
				   unsigned int yoffset);
extern int  osd_ext_sync_request(u32 index, u32 yres, u32 xoffset, u32 yoffset,
				 s32 in_fence_fd);
extern s32  osd_ext_wait_vsync_event(void);
extern void osd_ext_suspend_hw(void);
extern void osd_ext_resume_hw(void);
extern void osd_ext_init_hw(u32  logo_loaded);
extern void osd_ext_get_angle_hw(u32 index, u32 *angle);
extern void osd_ext_set_angle_hw(u32 index, u32 angle);
extern void osd_ext_get_clone_hw(u32 index, u32 *clone);
extern void osd_ext_set_clone_hw(u32 index, u32 clone);
extern void osd_ext_get_info(u32 index, u32 *addr, u32 *width, u32 *height);

#endif
