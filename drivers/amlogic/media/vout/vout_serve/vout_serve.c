/*
 * drivers/amlogic/media/vout/vout_serve/vout_serve.c
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
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/ctype.h>
#include <linux/of.h>
#include <linux/major.h>
#include <linux/uaccess.h>

/* Amlogic Headers */
#include <linux/amlogic/media/vout/vout_notify.h>

/* Local Headers */
#include "vout_serve.h"

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
static struct early_suspend early_suspend;
static int early_suspend_flag;
#endif
#ifdef CONFIG_SCREEN_ON_EARLY
static int early_resume_flag;
#endif


#define VOUT_CLASS_NAME "display"
#define	MAX_NUMBER_PARA 10

static struct class *vout_class;
static DEFINE_MUTEX(vout_mutex);
static char vout_mode_uboot[64] __nosavedata;
static char vout_mode[64] __nosavedata;
#ifdef CONFIG_AMLOGIC_FB
static char vout_axis[64] __nosavedata;
#endif
static u32 vout_init_vmode = VMODE_INIT_NULL;
static int uboot_display;

void update_vout_mode(char *name)
{
	snprintf(vout_mode, 60, "%s", name);
}
EXPORT_SYMBOL(update_vout_mode);

char *get_vout_mode_internal(void)
{
	return vout_mode;
}
EXPORT_SYMBOL(get_vout_mode_internal);

char *get_vout_mode_uboot(void)
{
	return vout_mode_uboot;
}
EXPORT_SYMBOL(get_vout_mode_uboot);

static char local_name[32] = {0};

static int set_vout_mode(char *name)
{
	enum vmode_e mode;
	int ret = 0;

	VOUTPR("vmode set to %s\n", name);

	mode = validate_vmode(name);
	if (mode == VMODE_MAX) {
		VOUTERR("no matched vout mode\n");
		return -1;
	}

	if (strcmp(name, local_name) == 0) {
		VOUTPR("don't set the same mode as current\n");
		return -1;
	}

	memset(local_name, 0, sizeof(local_name));
	strcpy(local_name, name);

	vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE_PRE, &mode);
	ret = set_current_vmode(mode);
	if (ret)
		VOUTERR("new mode %s set error\n", name);
	else
		VOUTPR("new mode %s set ok\n", name);
	vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE, &mode);

	return ret;
}

static int set_vout_init_mode(void)
{
	enum vmode_e vmode;
	int ret = 0;

	vout_init_vmode = validate_vmode(vout_mode_uboot);
	if (vout_init_vmode >= VMODE_MAX) {
		VOUTERR("no matched vout_init mode %s\n",
			vout_mode_uboot);
		return -1;
	}

	if (uboot_display)
		vmode = vout_init_vmode | VMODE_INIT_BIT_MASK;
	else
		vmode = vout_init_vmode;

	memset(local_name, 0, sizeof(local_name));
	strcpy(local_name, vout_mode_uboot);
	ret = set_current_vmode(vmode);
	VOUTPR("init mode %s\n", vout_mode_uboot);

	return ret;
}

#ifdef CONFIG_AMLOGIC_FB
static int parse_para(const char *para, int para_num, int *result)
{
	char *token = NULL;
	char *params, *params_base;
	int *out = result;
	int len = 0, count = 0;
	int res = 0;
	int ret = 0;

	if (!para)
		return 0;

	params = kstrdup(para, GFP_KERNEL);
	params_base = params;
	token = params;
	len = strlen(token);
	do {
		token = strsep(&params, " ");
		while (token && (isspace(*token)
				|| !isgraph(*token)) && len) {
			token++;
			len--;
		}
		if ((!token) || (*token == '\n') || (len == 0))
			break;
		ret = kstrtoint(token, 0, &res);
		if (ret < 0)
			break;
		len = strlen(token);
		*out++ = res;
		count++;
	} while ((token) && (count < para_num) && (len > 0));

	kfree(params_base);
	return count;
}

#define OSD_COUNT 2
static void set_vout_axis(char *para)
{
	static struct disp_rect_s disp_rect[OSD_COUNT];
	char count = OSD_COUNT * 4;
	int *pt = &disp_rect[0].x;
	int parsed[MAX_NUMBER_PARA] = {};

	/* parse window para */
	if (parse_para(para, 8, parsed) >= 4)
		memcpy(pt, parsed, sizeof(struct disp_rect_s) * OSD_COUNT);
	if ((count >= 4) && (count < 8))
		disp_rect[1] = disp_rect[0];

	VOUTPR("osd0=> x:%d,y:%d,w:%d,h:%d\nosd1=> x:%d,y:%d,w:%d,h:%d\n",
			*pt, *(pt + 1), *(pt + 2), *(pt + 3),
			*(pt + 4), *(pt + 5), *(pt + 6), *(pt + 7));
	vout_notifier_call_chain(VOUT_EVENT_OSD_DISP_AXIS, &disp_rect[0]);
}
#endif

static ssize_t vout_mode_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	int ret = 0;

	ret = snprintf(buf, 64, "%s\n", vout_mode);

	return ret;
}

static ssize_t vout_mode_store(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	char mode[64];

	mutex_lock(&vout_mutex);
	snprintf(mode, 64, "%s", buf);
	if (set_vout_mode(mode) == 0)
		strcpy(vout_mode, mode);
	mutex_unlock(&vout_mutex);
	return count;
}

#ifdef CONFIG_AMLOGIC_FB
static ssize_t vout_axis_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	int ret = 0;

	ret = snprintf(buf, 64, "%s\n", vout_axis);
	return ret;
}

static ssize_t vout_axis_store(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	mutex_lock(&vout_mutex);
	snprintf(vout_axis, 64, "%s", buf);
	set_vout_axis(vout_axis);
	mutex_unlock(&vout_mutex);
	return count;
}
#endif

static ssize_t vout_fr_policy_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	int policy;
	int ret = 0;

	policy = get_vframe_rate_policy();
	ret = sprintf(buf, "%d\n", policy);

	return ret;
}

static ssize_t vout_fr_policy_store(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	int policy;
	int ret = 0;

	mutex_lock(&vout_mutex);
	ret = kstrtoint(buf, 10, &policy);
	if (ret == 1) {
		ret = set_vframe_rate_policy(policy);
		if (ret)
			pr_info("%s: %d failed\n", __func__, policy);
	} else {
		pr_info("%s: invalid data\n", __func__);
		return -EINVAL;
	}
	mutex_unlock(&vout_mutex);
	return count;
}

static ssize_t vout_vinfo_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	const struct vinfo_s *info = NULL;
	ssize_t len = 0;

	info = get_current_vinfo();
	if (info == NULL)
		return sprintf(buf, "current vinfo is null\n");

	len = sprintf(buf, "current vinfo:\n"
		"    name:                  %s\n"
		"    mode:                  %d\n"
		"    width:                 %d\n"
		"    height:                %d\n"
		"    field_height:          %d\n"
		"    aspect_ratio_num:      %d\n"
		"    aspect_ratio_den:      %d\n"
		"    sync_duration_num:     %d\n"
		"    sync_duration_den:     %d\n"
		"    screen_real_width:     %d\n"
		"    screen_real_height:    %d\n"
		"    video_clk:             %d\n"
		"    viu_color_fmt:         %d\n"
		"    viu_mux:               %d\n\n",
		info->name, info->mode,
		info->width, info->height, info->field_height,
		info->aspect_ratio_num, info->aspect_ratio_den,
		info->sync_duration_num, info->sync_duration_den,
		info->screen_real_width, info->screen_real_height,
		info->video_clk, info->viu_color_fmt, info->viu_mux);
	len += sprintf(buf+len, "hdr_info:\n"
		"    present_flag          %d\n"
		"    features              0x%x\n"
		"    primaries             0x%x, 0x%x\n"
		"                          0x%x, 0x%x\n"
		"                          0x%x, 0x%x\n"
		"    white_point           0x%x, 0x%x\n"
		"    luminance             %d, %d\n\n",
		info->master_display_info.present_flag,
		info->master_display_info.features,
		info->master_display_info.primaries[0][0],
		info->master_display_info.primaries[0][1],
		info->master_display_info.primaries[1][0],
		info->master_display_info.primaries[1][1],
		info->master_display_info.primaries[2][0],
		info->master_display_info.primaries[2][1],
		info->master_display_info.white_point[0],
		info->master_display_info.white_point[1],
		info->master_display_info.luminance[0],
		info->master_display_info.luminance[1]);
	return len;
}

static struct class_attribute vout_class_attrs[] = {
	__ATTR(mode,      0644, vout_mode_show, vout_mode_store),
#ifdef CONFIG_AMLOGIC_FB
	__ATTR(axis,      0644, vout_axis_show, vout_axis_store),
#endif
	__ATTR(fr_policy, 0644,
		vout_fr_policy_show, vout_fr_policy_store),
	__ATTR(vinfo,     0644, vout_vinfo_show, NULL),
};

static int vout_create_attr(void)
{
	int i;
	int ret = 0;
	struct vinfo_s *init_mode;

	/* create vout class */
	vout_class = class_create(THIS_MODULE, VOUT_CLASS_NAME);
	if (IS_ERR(vout_class)) {
		VOUTERR("create vout class fail\n");
		return -1;
	}

	/* create vout class attr files */
	for (i = 0; i < ARRAY_SIZE(vout_class_attrs); i++) {
		if (class_create_file(vout_class, &vout_class_attrs[i])) {
			VOUTERR("create attribute %s fail\n",
				vout_class_attrs[i].attr.name);
		}
	}

	/* init /sys/class/display/mode */
	init_mode = get_current_vinfo();
	if (init_mode)
		strcpy(vout_mode, init_mode->name);

	return ret;
}

static int vout_remove_attr(void)
{
	int i;

	if (vout_class == NULL)
		return 0;

	for (i = 0; i < ARRAY_SIZE(vout_class_attrs); i++)
		class_remove_file(vout_class, &vout_class_attrs[i]);

	class_destroy(vout_class);
	vout_class = NULL;

	return 0;
}

#ifdef CONFIG_PM
static int aml_vout_suspend(struct platform_device *pdev, pm_message_t state)
{
#ifdef CONFIG_HAS_EARLYSUSPEND

	if (early_suspend_flag)
		return 0;

#endif
	vout_suspend();
	return 0;
}

static int aml_vout_resume(struct platform_device *pdev)
{
#ifdef CONFIG_SCREEN_ON_EARLY

	if (early_resume_flag) {
		early_resume_flag = 0;
		return 0;
	}

#endif
#ifdef CONFIG_HAS_EARLYSUSPEND

	if (early_suspend_flag)
		return 0;

#endif
	vout_resume();
	return 0;
}
#endif

#ifdef CONFIG_HIBERNATION
static int aml_vout_freeze(struct device *dev)
{
	return 0;
}

static int aml_vout_thaw(struct device *dev)
{
	return 0;
}

static int aml_vout_restore(struct device *dev)
{
	enum vmode_e mode;

	mode = get_current_vmode();
	vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE, &mode);

	return 0;
}
static int aml_vout_pm_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	return aml_vout_suspend(pdev, PMSG_SUSPEND);
}
static int aml_vout_pm_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	return aml_vout_resume(pdev);
}
#endif

#ifdef CONFIG_SCREEN_ON_EARLY
void resume_vout_early(void)
{
#ifdef CONFIG_HAS_EARLYSUSPEND
	early_suspend_flag = 0;
	early_resume_flag = 1;
	vout_resume();
#endif
}
EXPORT_SYMBOL(resume_vout_early);
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void aml_vout_early_suspend(struct early_suspend *h)
{
	if (early_suspend_flag)
		return;

	vout_suspend();
	early_suspend_flag = 1;
}

static void aml_vout_late_resume(struct early_suspend *h)
{
	if (!early_suspend_flag)
		return;

	early_suspend_flag = 0;
	vout_resume();
}
#endif

/*****************************************************************
 **
 **	vout driver interface
 **
 ******************************************************************/
static int aml_vout_probe(struct platform_device *pdev)
{
	int ret = -1;

#ifdef CONFIG_HAS_EARLYSUSPEND
	early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	early_suspend.suspend = aml_vout_early_suspend;
	early_suspend.resume = aml_vout_late_resume;
	register_early_suspend(&early_suspend);
#endif

	set_vout_init_mode();

	vout_class = NULL;
	ret = vout_create_attr();

	if (ret == 0)
		VOUTPR("create vout attribute OK\n");
	else
		VOUTERR("create vout attribute FAILED\n");

	VOUTPR("%s OK\n", __func__);
	return ret;
}

static int aml_vout_remove(struct platform_device *pdev)
{
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&early_suspend);
#endif
	vout_remove_attr();

	return 0;
}

static const struct of_device_id aml_vout_dt_match[] = {
	{ .compatible = "amlogic, vout",},
	{ },
};

#ifdef CONFIG_HIBERNATION
const struct dev_pm_ops vout_pm = {
	.freeze		= aml_vout_freeze,
	.thaw		= aml_vout_thaw,
	.restore	= aml_vout_restore,
	.suspend	= aml_vout_pm_suspend,
	.resume		= aml_vout_pm_resume,
};
#endif

static struct platform_driver vout_driver = {
	.probe     = aml_vout_probe,
	.remove    = aml_vout_remove,
#ifdef CONFIG_PM
	.suspend   = aml_vout_suspend,
	.resume    = aml_vout_resume,
#endif
	.driver = {
		.name = "vout",
		.of_match_table = aml_vout_dt_match,
#ifdef CONFIG_HIBERNATION
		.pm = &vout_pm,
#endif
	},
};

static int __init vout_init_module(void)
{
	int ret = 0;

	/*VOUTPR("%s\n", __func__);*/

	if (platform_driver_register(&vout_driver)) {
		VOUTERR("failed to register VOUT driver\n");
		ret = -ENODEV;
	}

	return ret;
}

static __exit void vout_exit_module(void)
{
	/*VOUTPR("%s\n", __func__);*/
	platform_driver_unregister(&vout_driver);
}

subsys_initcall(vout_init_module);
module_exit(vout_exit_module);

static int str2lower(char *str)
{
	while (*str != '\0') {
		*str = tolower(*str);
		str++;
	}
	return 0;
}

static void vout_init_mode_parse(char *str)
{
	/* detect vout mode */
	if (strlen(str) <= 1) {
		VOUTERR("%s: %s\n", __func__, str);
		return;
	}

	/* detect uboot display */
	if (strncmp(str, "en", 2) == 0) { /* enable */
		uboot_display = 1;
		VOUTPR("%s: %d\n", str, uboot_display);
		return;
	} else if (strncmp(str, "dis", 3) == 0) { /* disable */
		uboot_display = 0;
		VOUTPR("%s: %d\n", str, uboot_display);
		return;
	}

	/*
	 * just save the vmode_name,
	 * convert to vmode when vout sever registered
	 */
	strcpy(vout_mode_uboot, str);
	VOUTPR("%s\n", str);

	return;

	VOUTERR("%s: %s\n", __func__, str);
}

static int __init get_vout_init_mode(char *str)
{
	char *ptr = str;
	char sep[2];
	char *option;
	int count = 3;
	char find = 0;

	/* init void vout_mode_uboot name */
	memset(vout_mode_uboot, 0, sizeof(vout_mode_uboot));

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

	sep[0] = *ptr;
	sep[1] = '\0';
	while ((count--) && (option = strsep(&str, sep))) {
		/* VOUTPR("%s\n", option); */
		str2lower(option);
		vout_init_mode_parse(option);
	}

	return 0;
}
__setup("vout=", get_vout_init_mode);

MODULE_AUTHOR("Platform-BJ <platform.bj@amlogic.com>");
MODULE_DESCRIPTION("VOUT Server Module");
MODULE_LICENSE("GPL");
