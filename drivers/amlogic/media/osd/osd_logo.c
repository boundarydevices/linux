/*
 * drivers/amlogic/media/osd/osd_logo.c
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


/* Linux Headers */
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/delay.h>

/* Amlogic Headers */
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_module.h>


/* Local Headers */
#include "osd_hw.h"
#include "osd_log.h"
#include "osd.h"


#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

static DEFINE_MUTEX(logo_lock);

struct para_pair_s {
	char *name;
	int value;
};


static struct para_pair_s logo_args[] = {
	{"osd0", LOGO_DEV_OSD0},
	{"osd1", LOGO_DEV_OSD1},
	{"viu2_osd0", LOGO_DEV_VIU2_OSD0},
	{"debug", LOGO_DEBUG},
	{"loaded", LOGO_LOADED},
};


struct logo_info_s {
	int index;
	u32 vmode;
	u32 debug;
	u32 loaded;
	u32 fb_width;
	u32 fb_height;
} logo_info = {
	.index = -1,
	.vmode = VMODE_MAX,
	.debug = 0,
	.loaded = 0,
	.fb_width = 1920,
	.fb_height = 1080,
};

static int get_value_by_name(char *name, struct para_pair_s *pair, u32 cnt)
{
	u32 i = 0;
	int found = -1;

	for (i = 0; i < cnt; i++, pair++) {
		if (strcmp(pair->name, name) == 0) {
			found = pair->value;
			break;
		}
	}

	return found;
}

static int logo_info_init(char *para)
{
	u32 count = 0;
	int value = -1;

	count = sizeof(logo_args) / sizeof(logo_args[0]);
	value = get_value_by_name(para, logo_args, count);
	if (value >= 0) {
		switch (value) {
		case LOGO_DEV_OSD0:
			logo_info.index = LOGO_DEV_OSD0;
			break;
		case LOGO_DEV_OSD1:
			logo_info.index = LOGO_DEV_OSD1;
			break;
		case LOGO_DEV_VIU2_OSD0:
			logo_info.index = LOGO_DEV_VIU2_OSD0;
			break;
		case LOGO_DEBUG:
			logo_info.debug = 1;
			break;
		case LOGO_LOADED:
			logo_info.loaded = 1;
			break;
		default:
			break;
		}
		return 0;
	}

	return 0;
}

static int str2lower(char *str)
{
	while (*str != '\0') {
		*str = tolower(*str);
		str++;
	}
	return 0;
}

static int __init logo_setup(char *str)
{
	char *ptr = str;
	char sep[2];
	char *option;
	int count = 5;
	char find = 0;

	if (str == NULL)
		return -EINVAL;

	do {
		/* search for a delimiter */
		if (!isalpha(*ptr) && !isdigit(*ptr) && (*ptr != '_')) {
			find = 1;
			break;
		}
	} while (*++ptr != '\0');

	if (!find)
		return -EINVAL;

	logo_info.index = -1;
	logo_info.debug = 0;
	logo_info.loaded = 0;
	logo_info.vmode = VMODE_MAX;

	sep[0] = *ptr;
	sep[1] = '\0';
	while ((count--) && (option = strsep(&str, sep))) {
		pr_info("%s\n", option);
		str2lower(option);
		logo_info_init(option);
	}
	return 0;
}

static int __init get_logo_width(char *str)
{
	int ret;

	ret = kstrtoint(str, 0, &logo_info.fb_width);
	pr_info("logo_info.fb_width=%d\n", logo_info.fb_width);
	return 0;
}

static int __init get_logo_height(char *str)
{
	int ret;

	ret = kstrtoint(str, 0, &logo_info.fb_height);
	pr_info("logo_info.fb_height=%d\n", logo_info.fb_height);
	return 0;
}

int set_osd_logo_freescaler(void)
{
	const struct vinfo_s *vinfo;
	u32 index = logo_info.index;
	s32 src_x_start = 0, src_x_end = 0;
	s32 src_y_start = 0, src_y_end = 0;
	s32 dst_x_start = 0, dst_x_end = 0;
	s32 dst_y_start = 0, dst_y_end = 0;
	s32 target_x_end = 0, target_y_end = 0;

	if (logo_info.loaded == 0)
		return 0;

	if (osd_get_logo_index() != logo_info.index) {
		pr_info("logo changed, return!\n");
		return -1;
	}

	if ((osd_hw.osd_meson_dev.osd_ver == OSD_SIMPLE) && (index >= 1))
		return -1;

	if (osd_get_position_from_reg(
		index,
		&src_x_start, &src_x_end,
		&src_y_start, &src_y_end,
		&dst_x_start, &dst_x_end,
		&dst_y_start, &dst_y_end))
		return -1;

	vinfo = get_current_vinfo();
	if (vinfo) {
		target_x_end = vinfo->width - 1;
		target_y_end = vinfo->height - 1;
	} else {
		target_x_end = 1919;
		target_y_end = 1079;
	}
	if ((src_x_start == 0)
		&& (src_x_end == (logo_info.fb_width - 1))
		&& (src_y_start == 0)
		&& (src_y_end == (logo_info.fb_height - 1))
		&& (dst_x_start == 0)
		&& (dst_x_end == target_x_end)
		&& (dst_y_start == 0)
		&& (dst_y_end == target_y_end))
		return 0;

	if (vinfo)
		pr_info("outputmode changed to %s, reset osd%d, (%d, %d, %d, %d) -> (%d, %d, %d, %d)\n",
			vinfo->name, index,
			dst_x_start, dst_y_start, dst_x_end, dst_y_end,
			0, 0, target_x_end, target_y_end);
	else
		pr_info("outputmode changed to NULL, reset osd%d, (%d, %d, %d, %d) -> (%d, %d, %d, %d)\n",
			index,
			dst_x_start, dst_y_start, dst_x_end, dst_y_end,
			0, 0, target_x_end, target_y_end);

	osd_set_free_scale_mode_hw(index, 1);
	osd_set_free_scale_enable_hw(index, 0);
	osd_set_free_scale_axis_hw(index, 0, 0,
		logo_info.fb_width - 1, logo_info.fb_height - 1);
	osd_update_disp_axis_hw(index, 0, logo_info.fb_width - 1,
		0, logo_info.fb_height - 1, 0, 0, 0);
	osd_set_window_axis_hw(index, 0, 0, target_x_end, target_y_end);
	osd_set_free_scale_enable_hw(index, 0x10001);
	osd_enable_hw(index, 1);
	return 0;
}

int get_logo_loaded(void)
{
	return logo_info.loaded;
}

void set_logo_loaded(void)
{
	logo_info.loaded = 0;
}

__setup("logo=", logo_setup);

__setup("fb_width=", get_logo_width);

__setup("fb_height=", get_logo_height);

int logo_work_init(void)
{
	if (logo_info.loaded == 0)
		return -1;
	osd_set_logo_index(logo_info.index);
	return 0;
}
