/*
 * include/linux/amlogic/media/vfm/vframe.h
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

#ifndef VFRAME_H
#define VFRAME_H

#include <linux/types.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/atomic.h>
#include <linux/amlogic/iomap.h>

#define VIDTYPE_PROGRESSIVE             0x0
#define VIDTYPE_INTERLACE_TOP           0x1
#define VIDTYPE_INTERLACE_BOTTOM        0x3
#define VIDTYPE_TYPEMASK                0x7
#define VIDTYPE_INTERLACE               0x1
#define VIDTYPE_INTERLACE_FIRST         0x8
#define VIDTYPE_MVC                     0x10
#define VIDTYPE_NO_VIDEO_ENABLE         0x20
#define VIDTYPE_VIU_422                 0x800
#define VIDTYPE_VIU_FIELD               0x1000
#define VIDTYPE_VIU_SINGLE_PLANE        0x2000
#define VIDTYPE_VIU_444                 0x4000
#define VIDTYPE_VIU_NV21                0x8000
#define VIDTYPE_VSCALE_DISABLE          0x10000
#define VIDTYPE_CANVAS_TOGGLE           0x20000
#define VIDTYPE_PRE_INTERLACE           0x40000
#define VIDTYPE_HIGHRUN                 0x80000
#define VIDTYPE_COMPRESS                0x100000
#define VIDTYPE_PIC		        0x200000
#define VIDTYPE_SCATTER                 0x400000
#define VIDTYPE_VD2						0x800000
#define VIDTYPE_COMPRESS_LOSS		0x1000000

#define DISP_RATIO_FORCECONFIG          0x80000000
#define DISP_RATIO_FORCE_NORMALWIDE     0x40000000
#define DISP_RATIO_FORCE_FULL_STRETCH   0x20000000
#define DISP_RATIO_ADAPTED_PICMODE   0x10000000
#define DISP_RATIO_INFOFRAME_AVAIL   0x08000000
#define DISP_RATIO_CTRL_MASK            0x00000003
#define DISP_RATIO_NO_KEEPRATIO         0x00000000
#define DISP_RATIO_KEEPRATIO            0x00000001
#define DISP_RATIO_PORTRAIT_MODE        0x00000004

#define DISP_RATIO_ASPECT_RATIO_MASK    0x0003ff00
#define DISP_RATIO_ASPECT_RATIO_BIT     8
#define DISP_RATIO_ASPECT_RATIO_MAX     0x3ff

#define TB_DETECT_MASK    0x00000040
#define TB_DETECT_MASK_BIT     6
#define TB_DETECT_NONE          0
#define TB_DETECT_INVERT       1
#define TB_DETECT_NC               0
#define TB_DETECT_TFF             1
#define TB_DETECT_BFF             2
#define TB_DETECT_TBF             3

#define VFRAME_FLAG_NO_DISCONTINUE      1
#define VFRAME_FLAG_SWITCHING_FENSE     2
#define VFRAME_FLAG_HIGH_BANDWIDTH	4
#define VFRAME_FLAG_ERROR_RECOVERY		8
#define VFRAME_FLAG_SYNCFRAME			0x10
#define VFRAME_FLAG_GAME_MODE		0x20
#define VFRAME_FLAG_EMPTY_FRAME_V4L		0x80

enum pixel_aspect_ratio_e {
	PIXEL_ASPECT_RATIO_1_1,
	PIXEL_ASPECT_RATIO_8_9,
	PIXEL_ASPECT_RATIO_16_15,
};

/*
 * If pixel_sum[21:0] == 0, then all Histogram information are invalid
 */
struct vframe_hist_s {
	unsigned int hist_pow;
	unsigned int luma_sum;
	unsigned int chroma_sum;
	unsigned int pixel_sum;	/* [31:30] POW [21:0] PIXEL_SUM */
	unsigned int height;
	unsigned int width;
	unsigned char luma_max;
	unsigned char luma_min;
	unsigned short gamma[64];
	unsigned int vpp_luma_sum;	/*vpp hist info */
	unsigned int vpp_chroma_sum;
	unsigned int vpp_pixel_sum;
	unsigned int vpp_height;
	unsigned int vpp_width;
	unsigned char vpp_luma_max;
	unsigned char vpp_luma_min;
	unsigned short vpp_gamma[64];
#ifdef AML_LOCAL_DIMMING
	unsigned int ldim_max[100];
#endif
} /*vframe_hist_t */;

/*
 * If bottom == 0 or right == 0, then all Blackbar information are invalid
 */
struct vframe_bbar_s {
	unsigned short top;
	unsigned short bottom;
	unsigned short left;
	unsigned short right;
} /*vfame_bbar_t */;

/*
 * If vsin == 0, then all Measurement information are invalid
 */
struct vframe_meas_s {
	/* float frin; frame Rate of Video Input in the unit of Hz */
	unsigned int vs_span_cnt;
	unsigned long long vs_cnt;
	unsigned int hs_cnt0;
	unsigned int hs_cnt1;
	unsigned int hs_cnt2;
	unsigned int hs_cnt3;
	unsigned int vs_cycle;
	unsigned int vs_stamp;
} /*vframe_meas_t */;

struct vframe_view_s {
	int start_x;
	int start_y;
	unsigned int width;
	unsigned int height;
} /*vframe_view_t */;

#define SEI_PicTiming         1
#define SEI_MasteringDisplayColorVolume 137
#define SEI_ContentLightLevel 144
struct vframe_content_light_level_s {
	u32 present_flag;
	u32 max_content;
	u32 max_pic_average;
}; /* content_light_level from SEI */

struct vframe_master_display_colour_s {
	u32 present_flag;
	u32 primaries[3][2];
	u32 white_point[2];
	u32 luminance[2];
	struct vframe_content_light_level_s
		content_light_level;
}; /* master_display_colour_info_volume from SEI */

struct vframe_hdr_plus_sei_s {
	u16 present_flag;
	u16 itu_t_t35_country_code;
	u16 itu_t_t35_terminal_provider_code;
	u16 itu_t_t35_terminal_provider_oriented_code;
	u16 application_identifier;
	u16 application_version;
	/*num_windows max is 3*/
	u16 num_windows;
	/*windows xy*/
	u16 window_upper_left_corner_x[3];
	u16 window_upper_left_corner_y[3];
	u16 window_lower_right_corner_x[3];
	u16 window_lower_right_corner_y[3];
	u16 center_of_ellipse_x[3];
	u16 center_of_ellipse_y[3];
	u16 rotation_angle[3];
	u16 semimajor_axis_internal_ellipse[3];
	u16 semimajor_axis_external_ellipse[3];
	u16 semiminor_axis_external_ellipse[3];
	u16 overlap_process_option[3];
	/*target luminance*/
	u32 tgt_sys_disp_max_lumi;
	u16 tgt_sys_disp_act_pk_lumi_flag;
	u16 num_rows_tgt_sys_disp_act_pk_lumi;
	u16 num_cols_tgt_sys_disp_act_pk_lumi;
	u16 tgt_sys_disp_act_pk_lumi[25][25];

	/*num_windows max is 3, e.g maxscl[num_windows][i];*/
	u32 maxscl[3][3];
	u32 average_maxrgb[3];
	u16 num_distribution_maxrgb_percentiles[3];
	u16 distribution_maxrgb_percentages[3][15];
	u32 distribution_maxrgb_percentiles[3][15];
	u16 fraction_bright_pixels[3];

	u16 mast_disp_act_pk_lumi_flag;
	u16 num_rows_mast_disp_act_pk_lumi;
	u16 num_cols_mast_disp_act_pk_lumi;
	u16 mast_disp_act_pk_lumi[25][25];
	/*num_windows max is 3, e.g knee_point_x[num_windows]*/
	u16 tone_mapping_flag[3];
	u16 knee_point_x[3];
	u16 knee_point_y[3];
	u16 num_bezier_curve_anchors[3];
	u16 bezier_curve_anchors[3][15];
	u16 color_saturation_mapping_flag[3];
	u16 color_saturation_weight[3];
};
/* vframe properties */
struct vframe_prop_s {
	struct vframe_hist_s hist;
	struct vframe_bbar_s bbar;
	struct vframe_meas_s meas;
	struct vframe_master_display_colour_s
	 master_display_colour;
} /*vframe_prop_t */;

struct vdisplay_info_s {
	u32 frame_hd_start_lines_;
	u32 frame_hd_end_lines_;
	u32 frame_vd_start_lines_;
	u32 frame_vd_end_lines_;
	u32 display_vsc_startp;
	u32 display_vsc_endp;
	u32 display_hsc_startp;
	u32 display_hsc_endp;
	u32 screen_vd_v_start_;
	u32 screen_vd_v_end_;
	u32 screen_vd_h_start_;
	u32 screen_vd_h_end_;
};

enum vframe_source_type_e {
	VFRAME_SOURCE_TYPE_OTHERS = 0,
	VFRAME_SOURCE_TYPE_TUNER,
	VFRAME_SOURCE_TYPE_CVBS,
	VFRAME_SOURCE_TYPE_COMP,
	VFRAME_SOURCE_TYPE_HDMI,
	VFRAME_SOURCE_TYPE_PPMGR,
	VFRAME_SOURCE_TYPE_OSD,
};

enum vframe_source_mode_e {
	VFRAME_SOURCE_MODE_OTHERS = 0,
	VFRAME_SOURCE_MODE_PAL,
	VFRAME_SOURCE_MODE_NTSC,
	VFRAME_SOURCE_MODE_SECAM,
};
enum vframe_secam_phase_e {
	VFRAME_PHASE_DB = 0,
	VFRAME_PHASE_DR,
};
enum vframe_disp_mode_e {
	VFRAME_DISP_MODE_NULL = 0,
	VFRAME_DISP_MODE_UNKNOWN,
	VFRAME_DISP_MODE_SKIP,
	VFRAME_DISP_MODE_OK,
};

enum pic_mode_provider_e {
	PIC_MODE_PROVIDER_DB = 0,
	PIC_MODE_PROVIDER_WSS,
	PIC_MODE_UNKNOWN,
};

struct vframe_pic_mode_s {
	int hs;
	int he;
	int vs;
	int ve;
	u32 screen_mode;
	u32 custom_ar;
	u32 AFD_enable;
	enum pic_mode_provider_e provider;
};

#define BITDEPTH_Y_SHIFT 8
#define BITDEPTH_Y8    (0 << BITDEPTH_Y_SHIFT)
#define BITDEPTH_Y9    (1 << BITDEPTH_Y_SHIFT)
#define BITDEPTH_Y10   (2 << BITDEPTH_Y_SHIFT)
#define BITDEPTH_Y12   (3 << BITDEPTH_Y_SHIFT)
#define BITDEPTH_YMASK (3 << BITDEPTH_Y_SHIFT)

#define BITDEPTH_U_SHIFT 10
#define BITDEPTH_U8    (0 << BITDEPTH_U_SHIFT)
#define BITDEPTH_U9    (1 << BITDEPTH_U_SHIFT)
#define BITDEPTH_U10   (2 << BITDEPTH_U_SHIFT)
#define BITDEPTH_U12   (3 << BITDEPTH_U_SHIFT)
#define BITDEPTH_UMASK (3 << BITDEPTH_U_SHIFT)

#define BITDEPTH_V_SHIFT 12
#define BITDEPTH_V8    (0 << BITDEPTH_V_SHIFT)
#define BITDEPTH_V9    (1 << BITDEPTH_V_SHIFT)
#define BITDEPTH_V10   (2 << BITDEPTH_V_SHIFT)
#define BITDEPTH_V12   (3 << BITDEPTH_V_SHIFT)
#define BITDEPTH_VMASK (3 << BITDEPTH_V_SHIFT)

#define BITDEPTH_MASK (BITDEPTH_YMASK | BITDEPTH_UMASK | BITDEPTH_VMASK)
#define BITDEPTH_SAVING_MODE	0x1
#define FULL_PACK_422_MODE		0x2
struct vframe_s {
	u32 index;
	u32 index_disp;
	u32 omx_index;
	u32 type;
	u32 type_backup;
	u32 type_original;
	u32 blend_mode;
	u32 duration;
	u32 duration_pulldown;
	u32 pts;
	u64 pts_us64;
	bool next_vf_pts_valid;
	u32 next_vf_pts;
	u32 disp_pts;
	u64 disp_pts_us64;
	u64 timestamp;
	u32 flag;

	u32 canvas0Addr;
	u32 canvas1Addr;
	u32 compHeadAddr;
	u32 compBodyAddr;

	u32 plane_num;
	struct canvas_config_s canvas0_config[3];
	struct canvas_config_s canvas1_config[3];

	u32 bufWidth;
	u32 width;
	u32 height;
	u32 compWidth;
	u32 compHeight;
	u32 ratio_control;
	u32 bitdepth;
	u32 signal_type;
/*
 *	   bit 29: present_flag
 *	   bit 28-26: video_format
 *	   "component", "PAL", "NTSC", "SECAM",
 *	   "MAC", "unspecified"
 *	   bit 25: range "limited", "full_range"
 *	   bit 24: color_description_present_flag
 *	   bit 23-16: color_primaries
 *	   "unknown", "bt709", "undef", "bt601",
 *	   "bt470m", "bt470bg", "smpte170m", "smpte240m",
 *	   "film", "bt2020"
 *	   bit 15-8: transfer_characteristic
 *	   "unknown", "bt709", "undef", "bt601",
 *	   "bt470m", "bt470bg", "smpte170m", "smpte240m",
 *	   "linear", "log100", "log316", "iec61966-2-4",
 *	   "bt1361e", "iec61966-2-1", "bt2020-10", "bt2020-12",
 *	   "smpte-st-2084", "smpte-st-428"
 *	   bit 7-0: matrix_coefficient
 *	   "GBR", "bt709", "undef", "bt601",
 *	   "fcc", "bt470bg", "smpte170m", "smpte240m",
 *	   "YCgCo", "bt2020nc", "bt2020c"
 */
	u32 orientation;
	u32 video_angle;
	enum vframe_source_type_e source_type;
	enum vframe_secam_phase_e phase;
	enum vframe_source_mode_e source_mode;
	enum tvin_sig_fmt_e sig_fmt;

	enum tvin_trans_fmt trans_fmt;
	struct vframe_view_s left_eye;
	struct vframe_view_s right_eye;
	u32 mode_3d_enable;

	/* vframe extension */
	int (*early_process_fun)(void *arg, struct vframe_s *vf);
	int (*process_fun)(void *arg, unsigned int zoom_start_x_lines,
		unsigned int zoom_end_x_lines,
		unsigned int zoom_start_y_lines,
		unsigned int zoom_end_y_lines, struct vframe_s *vf);
	void *private_data;
#if 1
	/* vframe properties */
	struct vframe_prop_s prop;
#endif
	struct list_head list;
	struct tvafe_vga_parm_s vga_parm;
	/* pixel aspect ratio */
	enum pixel_aspect_ratio_e pixel_ratio;
	u64 ready_jiffies64;	/* ready from decode on  jiffies_64 */
	long long ready_clock[5];/*ns*/
	long long ready_clock_hist[2];/*ns*/
	atomic_t use_cnt;
	u32 frame_dirty;
	/*
	 *prog_proc_config:
	 *1: process p from decoder as filed;
	 *0: process p from decoder as frame
	 */
	u32 prog_proc_config;
	/* used for indicate current video is motion or static */
	int combing_cur_lev;

	/*
	 *for vframe's memory,
	 * used by memory owner.
	 */
	void *mem_handle;
	/*for MMU H265/VP9 compress header*/
	void *mem_head_handle;
	struct vframe_pic_mode_s pic_mode;

	unsigned long v4l_mem_handle;
} /*vframe_t */;

#if 0
struct vframe_prop_s *vdin_get_vframe_prop(u32 index);
#endif
int get_curren_frame_para(int *top, int *left, int *bottom, int *right);

u8 is_vpp_postblend(void);

void pause_video(unsigned char pause_flag);
#endif /* VFRAME_H */
