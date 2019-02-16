/*
 * drivers/amlogic/media/video_sink/video_priv.h
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

#ifndef VIDEO_PRIV_HEADER_HH
#define VIDEO_PRIV_HEADER_HH

#include <linux/amlogic/media/video_sink/vpp.h>

#define DEBUG_FLAG_BLACKOUT     0x1
#define DEBUG_FLAG_PRINT_TOGGLE_FRAME 0x2
#define DEBUG_FLAG_PRINT_RDMA                0x4
#define DEBUG_FLAG_LOG_RDMA_LINE_MAX         0x100
#define DEBUG_FLAG_TOGGLE_SKIP_KEEP_CURRENT  0x10000
#define DEBUG_FLAG_TOGGLE_FRAME_PER_VSYNC    0x20000
#define DEBUG_FLAG_RDMA_WAIT_1		     0x40000
#define DEBUG_FLAG_VSYNC_DONONE                0x80000
#define DEBUG_FLAG_GOFIELD_MANUL             0x100000
#define DEBUG_FLAG_LATENCY             0x200000
#define DEBUG_FLAG_PTS_TRACE            0x400000
#define DEBUG_FLAG_FRAME_DETECT            0x800000

/*for video.c's static int debug_flag;*/

#define VOUT_TYPE_TOP_FIELD 0
#define VOUT_TYPE_BOT_FIELD 1
#define VOUT_TYPE_PROG      2

#define VIDEO_DISABLE_NONE    0
#define VIDEO_DISABLE_NORMAL  1
#define VIDEO_DISABLE_FORNEXT 2

#define VIDEO_NOTIFY_TRICK_WAIT   0x01
#define VIDEO_NOTIFY_PROVIDER_GET 0x02
#define VIDEO_NOTIFY_PROVIDER_PUT 0x04
#define VIDEO_NOTIFY_FRAME_WAIT   0x08
#define VIDEO_NOTIFY_POS_CHANGED  0x10

struct video_dev_s {
	int vpp_off;
	int viu_off;
};

struct mif_pos_s {
	u32 id;
	u32 vd_reg_offt;
	u32 afbc_reg_offt;

	/* frame original size */
	u32 src_w;
	u32 src_h;

	/* mif start - end lines */
	u32 start_x_lines;
	u32 end_x_lines;
	u32 start_y_lines;
	u32 end_y_lines;

	/* left and right eye position, skip flag. */
	/* And if non 3d case, left eye = right eye */
	u32 l_hs_luma;
	u32 l_he_luma;
	u32 l_hs_chrm;
	u32 l_he_chrm;
	u32 r_hs_luma;
	u32 r_he_luma;
	u32 r_hs_chrm;
	u32 r_he_chrm;
	u32 h_skip;
	u32 l_vs_luma;
	u32 l_ve_luma;
	u32 l_vs_chrm;
	u32 l_ve_chrm;
	u32 r_vs_luma;
	u32 r_ve_luma;
	u32 r_vs_chrm;
	u32 r_ve_chrm;
	u32 v_skip;

	bool reverse;

	bool skip_afbc;
};

struct scaler_setting_s {
	u32 id;
	u32 misc_reg_offt;

	bool sc_h_enable;
	bool sc_v_enable;
	bool sc_top_enable;

	u32 vinfo_width;
	u32 vinfo_height;
	/* u32 VPP_pic_in_height_; */
	/* u32 VPP_line_in_length_; */

	struct vpp_frame_par_s *frame_par;
};

struct blend_setting_s {
	u32 id;
	u32 misc_reg_offt;

	u32 layer_alpha;

	u32 preblend_h_start;
	u32 preblend_h_end;
	u32 preblend_v_start;
	u32 preblend_v_end;

	u32 preblend_h_size;

	u32 postblend_h_start;
	u32 postblend_h_end;
	u32 postblend_v_start;
	u32 postblend_v_end;

	u32 postblend_h_size;

	struct vpp_frame_par_s *frame_par;
};

void safe_disble_videolayer(void);
void update_cur_dispbuf(void *buf);

/*for video related files only.*/
void video_module_lock(void);
void video_module_unlock(void);
struct video_dev_s *get_video_cur_dev(void);
struct vframe_s *get_cur_dispbuf(void);

int get_video_debug_flags(void);
int _video_set_disable(u32 val);
u32 get_video_enabled(void);
struct device *get_video_device(void);

#ifdef CONFIG_AMLOGIC_MEDIA_VIDEOCAPTURE
int ext_frame_capture_poll(int endflags);
#endif

extern u32 disp_canvas_index[2][6];
#endif
/*VIDEO_PRIV_HEADER_HH*/
