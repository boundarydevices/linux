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
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>

/* the MSB is represent vmode set by vmode_init */
#define	VMODE_INIT_BIT_MASK	0x8000
#define	VMODE_MODE_BIT_MASK	0xff

enum vmode_e {
	VMODE_480I = 0,
	VMODE_480I_RPT,
	VMODE_480CVBS,
	VMODE_480P,
	VMODE_480P_RPT,
	VMODE_576I,
	VMODE_576I_RPT,
	VMODE_576CVBS,
	VMODE_576P,
	VMODE_576P_RPT,
	VMODE_720P,
	VMODE_720P_50HZ,
	VMODE_768P,
	VMODE_768P_50HZ,
	VMODE_1080I,
	VMODE_1080I_50HZ,
	VMODE_1080P,
	VMODE_1080P_30HZ,
	VMODE_1080P_50HZ,
	VMODE_1080P_25HZ,
	VMODE_1080P_24HZ,
	VMODE_4K2K_30HZ,
	VMODE_4K2K_25HZ,
	VMODE_4K2K_24HZ,
	VMODE_4K2K_SMPTE,
	VMODE_4K2K_SMPTE_25HZ,
	VMODE_4K2K_SMPTE_30HZ,
	VMODE_4K2K_SMPTE_50HZ,
	VMODE_4K2K_SMPTE_50HZ_Y420,
	VMODE_4K2K_SMPTE_60HZ,
	VMODE_4K2K_SMPTE_60HZ_Y420,
	VMODE_4K2K_50HZ_Y420_10BIT,
	VMODE_4K2K_60HZ_Y420_10BIT,
	VMODE_4K2K_50HZ_Y422_10BIT,
	VMODE_4K2K_60HZ_Y422_10BIT,
	VMODE_4K2K_24HZ_Y444_10BIT,
	VMODE_4K2K_25HZ_Y444_10BIT,
	VMODE_4K2K_30HZ_Y444_10BIT,
	VMODE_4K2K_FAKE_5G,
	VMODE_4K2K_60HZ,
	VMODE_4K2K_60HZ_Y420,
	VMODE_4K2K_50HZ,
	VMODE_4K2K_50HZ_Y420,
	VMODE_4K2K_5G,
	VMODE_4K1K_120HZ,
	VMODE_4K1K_120HZ_Y420,
	VMODE_4K1K_100HZ,
	VMODE_4K1K_100HZ_Y420,
	VMODE_4K05K_240HZ,
	VMODE_4K05K_240HZ_Y420,
	VMODE_4K05K_200HZ,
	VMODE_4K05K_200HZ_Y420,
	VMODE_VGA,
	VMODE_SVGA,
	VMODE_XGA,
	VMODE_SXGA,
	VMODE_WSXGA,
	VMODE_FHDVGA,
	VMODE_LCD,
	VMODE_NULL,	/* null mode is used as temporary witch mode state */
	VMODE_MAX,
	VMODE_INIT_NULL,
	VMODE_MASK = 0xFF,
};

enum tvmode_e {
	TVMODE_480I = 0,
	TVMODE_480I_RPT,
	TVMODE_480CVBS,
	TVMODE_480P,
	TVMODE_480P_RPT,
	TVMODE_576I,
	TVMODE_576I_RPT,
	TVMODE_576CVBS,
	TVMODE_576P,
	TVMODE_576P_RPT,
	TVMODE_720P,
	TVMODE_720P_50HZ,
	TVMODE_768P,
	TVMODE_768P_50HZ,
	TVMODE_1080I,
	TVMODE_1080I_50HZ,
	TVMODE_1080P,
	TVMODE_1080P_30HZ,
	TVMODE_1080P_50HZ,
	TVMODE_1080P_25HZ,
	TVMODE_1080P_24HZ,
	TVMODE_4K2K_30HZ,
	TVMODE_4K2K_25HZ,
	TVMODE_4K2K_24HZ,
	TVMODE_4K2K_SMPTE,
	TVMODE_4K2K_SMPTE_25HZ,
	TVMODE_4K2K_SMPTE_30HZ,
	TVMODE_4K2K_SMPTE_50HZ,
	TVMODE_4K2K_SMPTE_50HZ_Y420,
	TVMODE_4K2K_SMPTE_60HZ,
	TVMODE_4K2K_SMPTE_60HZ_Y420,
	TVMODE_4K2K_50HZ_Y420_10BIT,
	TVMODE_4K2K_60HZ_Y420_10BIT,
	TVMODE_4K2K_50HZ_Y422_10BIT,
	TVMODE_4K2K_60HZ_Y422_10BIT,
	TVMODE_4K2K_24HZ_Y444_10BIT,
	TVMODE_4K2K_25HZ_Y444_10BIT,
	TVMODE_4K2K_30HZ_Y444_10BIT,
	TVMODE_4K2K_FAKE_5G,
	TVMODE_4K2K_60HZ,
	TVMODE_4K2K_60HZ_Y420,
	TVMODE_4K2K_50HZ,
	TVMODE_4K2K_50HZ_Y420,
	TVMODE_4K2K_5G,
	TVMODE_4K1K_120HZ,
	TVMODE_4K1K_120HZ_Y420,
	TVMODE_4K1K_100HZ,
	TVMODE_4K1K_100HZ_Y420,
	TVMODE_4K05K_240HZ,
	TVMODE_4K05K_240HZ_Y420,
	TVMODE_4K05K_200HZ,
	TVMODE_4K05K_200HZ_Y420,
	TVMODE_VGA,
	TVMODE_SVGA,
	TVMODE_XGA,
	TVMODE_SXGA,
	TVMODE_WSXGA,
	TVMODE_FHDVGA,
	TVMODE_NULL,	/* null mode is used as temporary witch mode state */
	TVMODE_MAX
};

#define SUPPORT_2020	0x01

/* master_display_info for display device */
struct master_display_info_s {
	u32 present_flag;
	u32 features;		/* feature bits bt2020/2084 */
	u32 primaries[3][2];	/* normalized 50000 in G,B,R order */
	u32 white_point[2];	/* normalized 50000 */
	u32 luminance[2];	/* max/min lumin, normalized 10000 */
};

struct hdr_info {
	u32 hdr_support;	/* RX EDID hdr support types */
	u32 lumi_max;		/* RX EDID Lumi Max value */
	u32 lumi_avg;		/* RX EDID Lumi Avg value */
	u32 lumi_min;		/* RX EDID Lumi Min value */
};

/*
 *Refer ot DolbyVision 2.6, Page 35
 * For Explicit Switch Signaling Methods using.
 */
enum eotf_type {
	EOTF_T_NULL = 0,
	EOTF_T_DOLBYVISION,
	EOTF_T_HDR10,
	EOTF_T_SDR,
	EOTF_T_MAX,
};

/* Dolby Version support information */

/* Refer to DV Spec issue 2.6 Page 11 and 14 */
struct dv_info {
	uint32_t ieeeoui;
	uint8_t ver;		/* 0 or 1 */
	uint8_t sup_yuv422_12bit:1; /* if as 0, then support RGB tunnel mode */
	uint8_t sup_2160p60hz:1;	/* if as 0, then support 2160p30hz */
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
	u32 sync_duration_num;
	u32 sync_duration_den;
	u32 screen_real_width;
	u32 screen_real_height;
	u32 video_clk;
	enum tvin_color_fmt_e viu_color_fmt;
	struct hdr_info hdr_info;
	struct master_display_info_s
	 master_display_info;
	const struct dv_info *dv_info;
	/* update hdmitx hdr packet, if data is NULL, disalbe packet */
	void (*fresh_tx_hdr_pkt)(struct master_display_info_s *data);
	/* tunnel_mode: 1: tunneling mode, RGB 8bit  0: YCbCr422 12bit mode */
	void (*fresh_tx_vsif_pkt)(enum eotf_type type, uint8_t tunnel_mode);
};

struct disp_rect_s {
	int x;
	int y;
	int w;
	int h;
};

struct reg_s {
	unsigned int reg;
	unsigned int val;
};

struct tvregs_set_t {
	enum tvmode_e tvmode;
	const struct reg_s *clk_reg_setting;
	const struct reg_s *enc_reg_setting;
};

struct tvinfo_s {
	enum tvmode_e tvmode;
	uint xres;
	uint yres;
	const char *id;
};

#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
enum fine_tune_mode_e {
	KEEP_HPLL,
	UP_HPLL,
	DOWN_HPLL,
};
#endif

extern enum vmode_e vmode_name_to_mode(const char *p);
extern struct vinfo_s *get_invalid_vinfo(void);
extern const char *vmode_mode_to_name(enum vmode_e vmode);
extern struct vinfo_hdr *get_rx_hdr_info(void);

#endif /* _VINFO_H_ */
