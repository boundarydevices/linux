/*
 * include/linux/amlogic/display/vinfo.h
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
#include <linux/amlogic/color_fmt.h>

/* the MSB is represent vmode set by vmode_init */
#define	VMODE_INIT_BIT_MASK	0x8000
#define	VMODE_MODE_BIT_MASK	0xff

enum vmode_e {
	VMODE_HDMI,
	VMODE_CVBS,
	VMODE_LCD,
	VMODE_NULL, /* null mode is used as temporary witch mode state */
	VMODE_MAX,
	VMODE_INIT_NULL,
	VMODE_MASK = 0xFF,
};

#define SUPPORT_2020	0x01

/* master_display_info for display device */
struct master_display_info_s {
	u32 present_flag;
	u32 features;			/* feature bits bt2020/2084 */
	u32 primaries[3][2];		/* normalized 50000 in G,B,R order */
	u32 white_point[2];		/* normalized 50000 */
	u32 luminance[2];		/* max/min lumin, normalized 10000 */
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
	struct master_display_info_s master_display_info;
	void *vout_device;
};

#endif /* _VINFO_H_ */
