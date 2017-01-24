/*
 * drivers/amlogic/media/logo/logo.c
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

/* Amlogic Headers */
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/hdmi_tx/hdmi_tx_module.h>

/* Local Headers */
#include <osd/osd_hw.h>


#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#define LOGO_DEV_OSD0 0x0
#define LOGO_DEV_OSD1 0x1
#define LOGO_DEBUG    0x1001
#define LOGO_LOADED   0x1002

static enum vmode_e hdmimode = VMODE_MAX;
static enum vmode_e cvbsmode = VMODE_MAX;
static enum vmode_e last_mode = VMODE_MAX;
static u32 vout_init_vmode = VMODE_INIT_NULL;

struct delayed_work logo_work;
static DEFINE_MUTEX(logo_lock);

struct para_pair_s {
	char *name;
	int value;
};

static struct para_pair_s logo_args[] = {
	{"osd0", LOGO_DEV_OSD0},
	{"osd1", LOGO_DEV_OSD1},
	{"debug", LOGO_DEBUG},
	{"loaded", LOGO_LOADED},
};

struct logo_info_s {
	int index;
	u32 vmode;
	u32 debug;
	u32 loaded;
} logo_info = {
	.index = -1,
	.vmode = VMODE_MAX,
	.debug = 0,
	.loaded = 0,
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

int set_osd_freescaler(int index, enum vmode_e new_mode)
{
	const struct vinfo_s *vinfo;

	osd_set_free_scale_mode_hw(index, 1);
	osd_set_free_scale_enable_hw(index, 0);

	osd_set_free_scale_axis_hw(index, 0, 0, 1919, 1079);
	osd_update_disp_axis_hw(index, 0, 1919, 0, 1079, 0, 0, 0);
	vinfo = get_current_vinfo();
	if (vinfo) {
		pr_info("outputmode changed to %s, reset osd%d scaler\n",
			vinfo->name, index);
		osd_set_window_axis_hw(index, 0, 0,
			(vinfo->width - 1), (vinfo->height - 1));
	} else {
		osd_set_window_axis_hw(index, 0, 0, 1919, 1079);
		pr_info("error: vinfo is NULL\n");
	}

	if (osd_get_logo_index() != logo_info.index) {
		pr_info("logo changed, return");
		return -1;
	}
	osd_set_free_scale_enable_hw(index, 0x10001);
	osd_enable_hw(index, 1);
	pr_info("finish");
	return 0;
}

enum vmode_e get_logo_vmode(void)
{
	return vout_init_vmode;
}

static int set_logo_vmode(enum vmode_e mode)
{
	int ret = 0;

	if (mode == VMODE_INIT_NULL)
		return -1;

	vout_init_vmode = mode;
	set_current_vmode(mode);

	return ret;
}

static int refresh_mode_and_logo(bool first)
{
	enum vmode_e cur_mode = VMODE_MAX;
	int hdp_state = 0;

#ifdef AMLOGIC_HDMI_TX
	hdp_state = get_hpd_state();
#endif

	if (!first && osd_get_logo_index() != logo_info.index)
		return -1;

	if (hdp_state)
		cur_mode = hdmimode;
	else
		cur_mode = cvbsmode;

	if (first) {
		last_mode = get_logo_vmode();

		if (logo_info.index >= 0) {
			osd_set_logo_index(logo_info.index);
			/* osd_init_hw(logo_info.loaded); */
		}
	}

	if (cur_mode >= VMODE_MAX) /* not box platform */
		return -1;
	if (cur_mode != last_mode) {
		pr_info("mode chang\n");
		if (logo_info.index >= 0)
			osd_enable_hw(logo_info.index, 0);
		set_logo_vmode(cur_mode);
		pr_info("set vmode: %s\n",
			get_current_vinfo()->name);
		last_mode = cur_mode;
		vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE, &cur_mode);
		if (logo_info.index >= 0)
			set_osd_freescaler(logo_info.index, cur_mode);
	}

	return 0;
}

void aml_logo_work(struct work_struct *work)
{
	mutex_lock(&logo_lock);

	refresh_mode_and_logo(false);

	mutex_unlock(&logo_lock);

	if (osd_get_logo_index() == logo_info.index)
		schedule_delayed_work(&logo_work, 1*HZ/2);

}

static int logo_info_init(char *para)
{
	u32 count = 0;
	int value = -1;

	count = ARRAY_SIZE(logo_args) / sizeof(logo_args[0]);
	value = get_value_by_name(para, logo_args, count);
	if (value >= 0) {
		switch (value) {
		case LOGO_DEV_OSD0:
			logo_info.index = LOGO_DEV_OSD0;
			break;
		case LOGO_DEV_OSD1:
			logo_info.index = LOGO_DEV_OSD1;
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
		if (!isalpha(*ptr) && !isdigit(*ptr)) {
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
__setup("logo=", logo_setup);

static int __init get_hdmi_mode(char *str)
{
	hdmimode = get_current_vmode();
	/* hdmimode = vmode_name_to_mode(str); */

	pr_info("get hdmimode: %s\n", str);
	return 1;
}
__setup("hdmimode=", get_hdmi_mode);

static int __init get_cvbs_mode(char *str)
{
	if (strncmp("480", str, 3) == 0)
		cvbsmode = VMODE_480CVBS;
	else if (strncmp("576", str, 3) == 0)
		cvbsmode = VMODE_576CVBS;
	else if (strncmp("nocvbs", str, 6) == 0)
		cvbsmode = hdmimode;
	pr_info("get cvbsmode: %s\n", str);
	return 1;
}
__setup("cvbsmode=", get_cvbs_mode);

static int __init logo_init(void)
{
	int ret = 0;

	pr_info("%s\n", __func__);

	if (logo_info.loaded == 0)
		return 0;

	refresh_mode_and_logo(true);

	INIT_DELAYED_WORK(&logo_work, aml_logo_work);
	schedule_delayed_work(&logo_work, 1*HZ/2);

	return ret;
}
subsys_initcall(logo_init);
