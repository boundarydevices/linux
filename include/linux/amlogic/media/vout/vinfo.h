/*
 * include/linux/amlogic/media/vout/vinfo.h
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

#ifndef _VINFO_H_
#define _VINFO_H_
#include <linux/amlogic/media/vfm/video_common.h>

/* the MSB is represent vmode set by vmode_init */
#define	VMODE_INIT_BIT_MASK	0x8000
#define	VMODE_MODE_BIT_MASK	0xff

enum vmode_e {
	VMODE_HDMI = 0,
	VMODE_CVBS,
	VMODE_LCD,
	VMODE_NULL, /* null mode is used as temporary witch mode state */
	VMODE_MAX,
	VMODE_INIT_NULL,
	VMODE_MASK = 0xFF,
};

enum viu_mux_e {
	VIU_MUX_ENCI = 0,
	VIU_MUX_ENCP,
	VIU_MUX_ENCL,
	VIU_MUX_MAX,
};

#define SUPPORT_2020	0x01

/* master_display_info for display device */
struct master_display_info_s {
	u32 present_flag;
	u32 features;			/* feature bits bt2020/2084 */
	u32 primaries[3][2];		/* normalized 50000 in G,B,R order */
	u32 white_point[2];		/* normalized 50000 */
	u32 luminance[2];		/* max/min lumin, normalized 10000 */
	u32 max_content;		/* Maximum Content Light Level */
	u32 max_frame_average;	/* Maximum Frame-average Light Level */
};
struct hdr_info {
	u32 hdr_support; /* RX EDID hdr support types */
	u32 lumi_max; /* RX EDID Lumi Max value */
	u32 lumi_avg; /* RX EDID Lumi Avg value */
	u32 lumi_min; /* RX EDID Lumi Min value */
};
enum eotf_type {
	EOTF_T_NULL = 0,
	EOTF_T_DOLBYVISION,
	EOTF_T_HDR10,
	EOTF_T_SDR,
	EOTF_T_MAX,
};
struct dv_info {
	uint32_t ieeeoui;
	uint8_t ver; /* 0 or 1 */
	uint8_t sup_yuv422_12bit:1; /* if as 0, then support RGB tunnel mode */
	uint8_t sup_2160p60hz:1; /* if as 0, then support 2160p30hz */
	uint8_t sup_global_dimming:1;
	uint8_t colorimetry:1;
	union {
		struct {
			uint16_t chrom_red_primary_x;
			uint16_t chrom_red_primary_y;
			uint16_t chrom_green_primary_x;
			uint16_t chrom_green_primary_y;
			uint16_t chrom_blue_primary_x;
			uint16_t chrom_blue_primary_y;
			uint16_t chrom_white_primary_x;
			uint16_t chrom_white_primary_y;
			uint16_t target_min_pq;
			uint16_t target_max_pq;
			uint8_t dm_major_ver;
			uint8_t dm_minor_ver;
		} ver0;
		struct {
			uint8_t dm_version;
			uint8_t target_max_lum;
			uint8_t target_min_lum;
			uint8_t chrom_red_primary_x;
			uint8_t chrom_red_primary_y;
			uint8_t chrom_green_primary_x;
			uint8_t chrom_green_primary_y;
			uint8_t chrom_blue_primary_x;
			uint8_t chrom_blue_primary_y;
		} ver1;
	} vers;
};

struct vinfo_s {
	char *name;
	enum vmode_e mode;
	char ext_name[32];
	u32 width;
	u32 height;
	u32 field_height;
	u32 aspect_ratio_num;
	u32 aspect_ratio_den;
	u32 screen_real_width;
	u32 screen_real_height;
	u32 sync_duration_num;
	u32 sync_duration_den;
	u32 video_clk;
	enum color_fmt_e viu_color_fmt;
	enum viu_mux_e viu_mux;
	struct master_display_info_s master_display_info;
	struct hdr_info hdr_info;
	const struct dv_info *dv_info;
	void (*fresh_tx_hdr_pkt)(struct master_display_info_s *data);
	void (*fresh_tx_vsif_pkt)(enum eotf_type type, uint8_t tunnel_mode);
	void *vout_device;
};

#ifdef CONFIG_AMLOGIC_MEDIA_FB
struct disp_rect_s {
	int x;
	int y;
	int w;
	int h;
};
#endif
#endif /* _VINFO_H_ */
