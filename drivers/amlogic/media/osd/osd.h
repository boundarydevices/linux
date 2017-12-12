/*
 * drivers/amlogic/media/osd/osd.h
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

#ifndef _OSD_H_
#define _OSD_H_

enum color_index_e {
	COLOR_INDEX_02_PAL4    = 2,  /* 0 */
	COLOR_INDEX_04_PAL16   = 4, /* 0 */
	COLOR_INDEX_08_PAL256 = 8,
	COLOR_INDEX_16_655 = 9,
	COLOR_INDEX_16_844 = 10,
	COLOR_INDEX_16_6442 = 11,
	COLOR_INDEX_16_4444_R = 12,
	COLOR_INDEX_16_4642_R = 13,
	COLOR_INDEX_16_1555_A = 14,
	COLOR_INDEX_16_4444_A = 15,
	COLOR_INDEX_16_565 = 16,

	COLOR_INDEX_24_6666_A = 19,
	COLOR_INDEX_24_6666_R = 20,
	COLOR_INDEX_24_8565 = 21,
	COLOR_INDEX_24_5658 = 22,
	COLOR_INDEX_24_888_B = 23,
	COLOR_INDEX_24_RGB = 24,

	COLOR_INDEX_32_BGRX = 25,
	COLOR_INDEX_32_XBGR = 26,
	COLOR_INDEX_32_RGBX = 27,
	COLOR_INDEX_32_XRGB = 28,

	COLOR_INDEX_32_BGRA = 29,
	COLOR_INDEX_32_ABGR = 30,
	COLOR_INDEX_32_RGBA = 31,
	COLOR_INDEX_32_ARGB = 32,

	COLOR_INDEX_YUV_422 = 33,
};

#define VPP_OSD2_PREBLEND           (1 << 17)
#define VPP_OSD1_PREBLEND           (1 << 16)
#define VPP_VD2_PREBLEND            (1 << 15)
#define VPP_VD1_PREBLEND            (1 << 14)
#define VPP_OSD2_POSTBLEND          (1 << 13)
#define VPP_OSD1_POSTBLEND          (1 << 12)
#define VPP_VD2_POSTBLEND           (1 << 11)
#define VPP_VD1_POSTBLEND           (1 << 10)
#define VPP_POSTBLEND_EN            (1 << 7)
#define VPP_PRE_FG_OSD2             (1 << 5)
#define VPP_PREBLEND_EN             (1 << 6)
#define VPP_POST_FG_OSD2            (1 << 4)

/* OSD device ioctl definition */
#define FBIOPUT_OSD_SRCKEY_ENABLE        0x46fa
#define FBIOPUT_OSD_SRCCOLORKEY          0x46fb
#define FBIOGET_OSD_DMABUF               0x46fc
#define FBIOPUT_OSD_SET_GBL_ALPHA        0x4500
#define FBIOGET_OSD_GET_GBL_ALPHA        0x4501
#define FBIOPUT_OSD_2X_SCALE             0x4502
#define FBIOPUT_OSD_ENABLE_3D_MODE       0x4503
#define FBIOPUT_OSD_FREE_SCALE_ENABLE    0x4504
#define FBIOPUT_OSD_FREE_SCALE_WIDTH     0x4505
#define FBIOPUT_OSD_FREE_SCALE_HEIGHT    0x4506
#define FBIOPUT_OSD_ORDER                0x4507
#define FBIOGET_OSD_ORDER                0x4508
#define FBIOGET_OSD_SCALE_AXIS           0x4509
#define FBIOPUT_OSD_SCALE_AXIS           0x450a
#define FBIOGET_OSD_BLOCK_WINDOWS        0x450b
#define FBIOPUT_OSD_BLOCK_WINDOWS        0x450c
#define FBIOGET_OSD_BLOCK_MODE           0x450d
#define FBIOPUT_OSD_BLOCK_MODE           0x450e
#define FBIOGET_OSD_FREE_SCALE_AXIS      0x450f
#define FBIOPUT_OSD_FREE_SCALE_AXIS      0x4510
#define FBIOPUT_OSD_FREE_SCALE_MODE      0x4511
#define FBIOGET_OSD_WINDOW_AXIS          0x4512
#define FBIOPUT_OSD_WINDOW_AXIS          0x4513
#define FBIOGET_OSD_FLUSH_RATE           0x4514
#define FBIOPUT_OSD_REVERSE              0x4515
#define FBIOPUT_OSD_ROTATE_ON            0x4516
#define FBIOPUT_OSD_ROTATE_ANGLE         0x4517
#define FBIOPUT_OSD_SYNC_ADD             0x4518
#define FBIOPUT_OSD_SYNC_RENDER_ADD      0x4519

#define FB_IOC_MAGIC   'O'
#define FBIOPUT_OSD_CURSOR	\
	_IOWR(FB_IOC_MAGIC, 0x0,  struct fb_cursor_user)
/* OSD color definition */
#define KEYCOLOR_FLAG_TARGET  1
#define KEYCOLOR_FLAG_ONHOLD  2
#define KEYCOLOR_FLAG_CURRENT 4

#define HW_OSD_COUNT 2
/* OSD block definition */
#define HW_OSD_BLOCK_COUNT 4
#define HW_OSD_BLOCK_REG_COUNT (HW_OSD_BLOCK_COUNT*2)
#define HW_OSD_BLOCK_ENABLE_MASK        0x000F
#define HW_OSD_BLOCK_ENABLE_0           0x0001 /* osd blk0 enable */
#define HW_OSD_BLOCK_ENABLE_1           0x0002 /* osd blk1 enable */
#define HW_OSD_BLOCK_ENABLE_2           0x0004 /* osd blk2 enable */
#define HW_OSD_BLOCK_ENABLE_3           0x0008 /* osd blk3 enable */
#define HW_OSD_BLOCK_LAYOUT_MASK        0xFFFF0000
#define HW_OSD_BLOCK_LAYOUT_HORIZONTAL  0x00010000
#define HW_OSD_BLOCK_LAYOUT_VERTICAL    0x00020000
#define HW_OSD_BLOCK_LAYOUT_GRID        0x00030000
#define HW_OSD_BLOCK_LAYOUT_CUSTOMER    0xFFFF0000

#define OSD_LEFT 0
#define OSD_RIGHT 1
#define OSD_ORDER_01 1
#define OSD_ORDER_10 2
#define OSD_GLOBAL_ALPHA_DEF 0x100
#define OSD_DATA_BIG_ENDIAN 0
#define OSD_DATA_LITTLE_ENDIAN 1
#define OSD_TC_ALPHA_ENABLE_DEF 0  /* disable tc_alpha */

#define INT_VIU_VSYNC 30 /* 35 */
#define INT_VIU2_VSYNC 45
#define INT_RDMA 121

#define OSD_MAX_BUF_NUM 3  /* fence relative */

enum osd_index_e {
	OSD1 = 0,
	OSD2
};

enum osd_enable_e {
	DISABLE = 0,
	ENABLE
};

enum scan_mode_e {
	SCAN_MODE_INTERLACE,
	SCAN_MODE_PROGRESSIVE
};

struct color_bit_define_s {
	enum color_index_e color_index;
	u8 hw_colormat;
	u8 hw_blkmode;

	u8 red_offset;
	u8 red_length;
	u8 red_msb_right;

	u8 green_offset;
	u8 green_length;
	u8 green_msb_right;

	u8 blue_offset;
	u8 blue_length;
	u8 blue_msb_right;

	u8 transp_offset;
	u8 transp_length;
	u8 transp_msb_right;

	u8 color_type;
	u8 bpp;
};

struct osd_ctl_s {
	u32 xres_virtual;
	u32 yres_virtual;
	u32 xres;
	u32 yres;
	u32 disp_start_x; /* coordinate of screen */
	u32 disp_start_y;
	u32 disp_end_x;
	u32 disp_end_y;
	u32 addr;
	u32 index;
};

struct osd_info_s {
	u32 index;
	u32 osd_reverse;
};

struct para_osd_info_s {
	char *name;
	u32 info;
	u32 prev_idx;
	u32 next_idx;
	u32 cur_group_start;
	u32 cur_group_end;
};

enum osd_dev_e {
	DEV_OSD0 = 0,
	DEV_OSD1,
	DEV_ALL,
	DEV_MAX
};

enum reverse_info_e {
	REVERSE_FALSE = 0,
	REVERSE_TRUE,
	REVERSE_X,
	REVERSE_Y,
	REVERSE_MAX
};

enum hw_reg_index_e {
	OSD_COLOR_MODE = 0,
	OSD_ENABLE,
	OSD_COLOR_KEY,
	OSD_COLOR_KEY_ENABLE,
	OSD_GBL_ALPHA,
	OSD_CHANGE_ORDER,
	OSD_FREESCALE_COEF,
	DISP_GEOMETRY,
	DISP_SCALE_ENABLE,
	DISP_FREESCALE_ENABLE,
	DISP_OSD_REVERSE,
	DISP_OSD_ROTATE,
	OSD_FIFO,
	HW_REG_INDEX_MAX
};

struct pandata_s {
	s32 x_start;
	s32 x_end;
	s32 y_start;
	s32 y_end;
};

struct fb_geometry_s {
	u32 width;  /* in byte unit */
	u32 height;
	u32 canvas_idx;
	u32 addr;
	u32 xres;
	u32 yres;
};

struct osd_scale_s {
	u16 h_enable;
	u16 v_enable;
};

struct osd_3d_mode_s {
	struct osd_scale_s origin_scale;
	u16 enable;
	u16 left_right;
	u16 l_start;
	u16 l_end;
	u16 r_start;
	u16 r_end;
};

struct osd_rotate_s {
	u32 on_off;
	u32 angle;
};

struct osd_fence_map_s {
	struct list_head list;

	u32 fb_index;
	u32 buf_num;
	u32 xoffset;
	u32 yoffset;
	u32 yres;
	s32 in_fd;
	s32 out_fd;
	u32 ext_addr;
	u32 format;
	u32 width;
	u32 height;
	u32 op;
	u32 compose_type;
	u32 dst_x;
	u32 dst_y;
	u32 dst_w;
	u32 dst_h;
	int byte_stride;
	int pxiel_stride;
	u32 reserve;
	struct fence *in_fence;
};

struct afbcd_data_s {
	u32 enable;
	u32 phy_addr;
	u32 addr[OSD_MAX_BUF_NUM];
	u32 frame_width;
	u32 frame_height;
	u32 conv_lbuf_len;
};

typedef void (*update_func_t)(void);
struct hw_list_s {
	struct list_head list;
	update_func_t update_func;
};

struct hw_para_s {
	struct pandata_s pandata[HW_OSD_COUNT];
	struct pandata_s dispdata[HW_OSD_COUNT];
	struct pandata_s dispdata_backup[HW_OSD_COUNT];
	struct pandata_s scaledata[HW_OSD_COUNT];
	struct pandata_s free_src_data[HW_OSD_COUNT];
	struct pandata_s free_dst_data[HW_OSD_COUNT];
	struct pandata_s free_src_data_backup[HW_OSD_COUNT];
	struct pandata_s free_dst_data_backup[HW_OSD_COUNT];
	/* struct pandata_s rotation_pandata[HW_OSD_COUNT]; */
	struct pandata_s cursor_dispdata[HW_OSD_COUNT];
	u32 buffer_alloc[HW_OSD_COUNT];

	u32 gbl_alpha[HW_OSD_COUNT];
	u32 color_key[HW_OSD_COUNT];
	u32 color_key_enable[HW_OSD_COUNT];
	u32 enable[HW_OSD_COUNT];
	u32 reg_status_save;
#ifdef FIQ_VSYNC
	bridge_item_t fiq_handle_item;
#endif
	struct osd_scale_s scale[HW_OSD_COUNT];
	struct osd_scale_s free_scale[HW_OSD_COUNT];
	struct osd_scale_s free_scale_backup[HW_OSD_COUNT];
	u32 free_scale_enable[HW_OSD_COUNT];
	u32 free_scale_enable_backup[HW_OSD_COUNT];
	struct fb_geometry_s fb_gem[HW_OSD_COUNT];
	const struct color_bit_define_s *color_info[HW_OSD_COUNT];
	const struct color_bit_define_s *color_backup[HW_OSD_COUNT];
	u32 scan_mode[HW_OSD_COUNT];
	u32 order;
	struct osd_3d_mode_s mode_3d[HW_OSD_COUNT];
	u32 updated[HW_OSD_COUNT];
	/* u32 block_windows[HW_OSD_COUNT][HW_OSD_BLOCK_REG_COUNT]; */
	u32 block_mode[HW_OSD_COUNT];
	u32 free_scale_mode[HW_OSD_COUNT];
	u32 free_scale_mode_backup[HW_OSD_COUNT];
	u32 osd_reverse[HW_OSD_COUNT];
	/* struct osd_rotate_s rotate[HW_OSD_COUNT]; */
	struct hw_list_s reg[HW_OSD_COUNT][HW_REG_INDEX_MAX];
	u32 field_out_en;
	u32 scale_workaround;
	u32 fb_for_4k2k;
	u32 antiflicker_mode;
	u32 angle[HW_OSD_COUNT];
	u32 clone[HW_OSD_COUNT];
	u32 bot_type;
	u32 hw_reset_flag;
	struct afbcd_data_s osd_afbcd[HW_OSD_COUNT];
	u32 urgent[HW_OSD_COUNT];
	u32 osd_fifo[HW_OSD_COUNT];
	u32 osd_deband_enable;
	u32 osd_fps;
	u32 osd_fps_start;
	u32 osd_display_debug;
	char __iomem *screen_base[HW_OSD_COUNT];
	u32 screen_size[HW_OSD_COUNT];
	char __iomem *screen_base_backup[HW_OSD_COUNT];
	u32 screen_size_backup[HW_OSD_COUNT];
	u32 osd_clear[HW_OSD_COUNT];
	u32 vinfo_width;
	u32 vinfo_height;
	u32 fb_drvier_probe;
};


#endif /* _OSD_H_ */
