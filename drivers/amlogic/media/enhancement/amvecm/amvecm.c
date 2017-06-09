/*
 * drivers/amlogic/media/enhancement/amvecm/amvecm.c
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

/* Standard Linux headers */
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
/* #include <linux/amlogic/aml_common.h> */
#include <linux/ctype.h>/* for parse_para_pq */
#include <linux/vmalloc.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/io.h>

#ifdef CONFIG_AMLOGIC_LCD
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#endif

#include "arch/vpp_regs.h"
#include "arch/ve_regs.h"
#include "arch/cm_regs.h"
#include "../../vin/tvin/tvin_global.h"

#include "amve.h"
#include "amcm.h"
#include "amcsc.h"

#define pr_amvecm_dbg(fmt, args...)\
	do {\
		if (debug_amvecm)\
			pr_info("AMVECM: " fmt, ## args);\
	} while (0)
#define pr_amvecm_error(fmt, args...)\
	pr_error("AMVECM: " fmt, ## args)

#define AMVECM_NAME               "amvecm"
#define AMVECM_DRIVER_NAME        "amvecm"
#define AMVECM_MODULE_NAME        "amvecm"
#define AMVECM_DEVICE_NAME        "amvecm"
#define AMVECM_CLASS_NAME         "amvecm"

struct amvecm_dev_s {
	dev_t                       devt;
	struct cdev                 cdev;
	dev_t                       devno;
	struct device               *dev;
	struct class                *clsp;
};

static struct amvecm_dev_s amvecm_dev;

spinlock_t vpp_lcd_gamma_lock;

signed int vd1_brightness = 0, vd1_contrast;

static int hue_pre;  /*-25~25*/
static int saturation_pre;  /*-128~127*/
static int hue_post;  /*-25~25*/
static int saturation_post;  /*-128~127*/
/*contrast add saturation add*/
static int satu_shift_by_con;  /*-128~127*/

static s16 saturation_ma;
static s16 saturation_mb;
static s16 saturation_ma_shift;
static s16 saturation_mb_shift;

unsigned int sr1_reg_val[101];
unsigned int sr1_ret_val[101];
struct vpp_hist_param_s vpp_hist_param;
static unsigned int pre_hist_height, pre_hist_width;
static unsigned int pc_mode = 0xff;
static unsigned int pc_mode_last = 0xff;
static struct hdr_metadata_info_s vpp_hdr_metadata_s;

void __iomem *amvecm_hiu_reg_base;/* = *ioremap(0xc883c000, 0x2000); */

#define HIU_REG_BASE 0xc883c000

static bool debug_amvecm;
module_param(debug_amvecm, bool, 0664);
MODULE_PARM_DESC(debug_amvecm, "\n debug_amvecm\n");

unsigned int vecm_latch_flag;
module_param(vecm_latch_flag, uint, 0664);
MODULE_PARM_DESC(vecm_latch_flag, "\n vecm_latch_flag\n");

unsigned int vpp_demo_latch_flag;
module_param(vpp_demo_latch_flag, uint, 0664);
MODULE_PARM_DESC(vpp_demo_latch_flag, "\n vpp_demo_latch_flag\n");

unsigned int pq_load_en = 1;/* load pq table enable/disable */
module_param(pq_load_en, uint, 0664);
MODULE_PARM_DESC(pq_load_en, "\n pq_load_en\n");

bool gamma_en;  /* wb_gamma_en enable/disable */
module_param(gamma_en, bool, 0664);
MODULE_PARM_DESC(gamma_en, "\n gamma_en\n");

bool wb_en;  /* wb_en enable/disable */
module_param(wb_en, bool, 0664);
MODULE_PARM_DESC(wb_en, "\n wb_en\n");

unsigned int probe_ok;/* probe ok or not */
module_param(probe_ok, uint, 0664);
MODULE_PARM_DESC(probe_ok, "\n probe_ok\n");

static unsigned int sr1_index;/* for sr1 read */
module_param(sr1_index, uint, 0664);
MODULE_PARM_DESC(sr1_index, "\n sr1_index\n");


/* vpp brightness/contrast/saturation/hue */
static int __init amvecm_load_pq_val(char *str)
{
	int i = 0, err = 0;
	char *tk = NULL, *tmp[4];
	long val;

	if (str == NULL) {
		pr_err("[amvecm] pq val error !!!\n");
		return 0;
	}

	for (tk = strsep(&str, ","); tk != NULL; tk = strsep(&str, ",")) {
		tmp[i] = tk;
		err = kstrtol(tmp[i], 10, &val);
		if (err) {
			pr_err("[amvecm] pq string error !!!\n");
			break;
		}
		/* pr_err("[amvecm] pq[%d]: %d\n", i, (int)val[i]); */

		/* only need to get sat/hue value,*/
		/*brightness/contrast can be got from registers */
		if (i == 2)
			saturation_post = (int)val;
		else if (i == 3)
			hue_post = (int)val;
		i++;
	}

	return 0;
}
__setup("pq=", amvecm_load_pq_val);


static void amvecm_size_patch(void)
{
	unsigned int hs, he, vs, ve;

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXTVBB) {
		hs = READ_VPP_REG_BITS(VPP_HSC_REGION12_STARTP, 16, 13);
		he = READ_VPP_REG_BITS(VPP_HSC_REGION4_ENDP, 0, 13);

		vs = READ_VPP_REG_BITS(VPP_VSC_REGION12_STARTP, 16, 13);
		ve = READ_VPP_REG_BITS(VPP_VSC_REGION4_ENDP, 0, 13);
		ve_frame_size_patch(he-hs+1, ve-vs+1);
	}
	hs = READ_VPP_REG_BITS(VPP_POSTBLEND_VD1_H_START_END, 16, 13);
	he = READ_VPP_REG_BITS(VPP_POSTBLEND_VD1_H_START_END, 0, 13);

	vs = READ_VPP_REG_BITS(VPP_POSTBLEND_VD1_V_START_END, 16, 13);
	ve = READ_VPP_REG_BITS(VPP_POSTBLEND_VD1_V_START_END, 0, 13);
	cm2_frame_size_patch(he-hs+1, ve-vs+1);
}

/* video adj1 */
static ssize_t video_adj1_brightness_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	s32 val = 0;

	if (get_cpu_type() <= MESON_CPU_MAJOR_ID_GXTVBB) {
		val = (READ_VPP_REG(VPP_VADJ1_Y) >> 8) & 0x1ff;
		val = (val << 23) >> 23;

		return sprintf(buf, "%d\n", val);
	}
	val = (READ_VPP_REG(VPP_VADJ1_Y) >> 8) & 0x3ff;
	val = (val << 23) >> 23;

	return sprintf(buf, "%d\n", val >> 1);
}

static ssize_t video_adj1_brightness_store(struct class *cla,
			struct class_attribute *attr,
			const char *buf, size_t count)
{
	size_t r;
	int val;

	r = sscanf(buf, "%d\n", &val);
	if ((r != 1) || (val < -255) || (val > 255))
		return -EINVAL;

	if (get_cpu_type() <= MESON_CPU_MAJOR_ID_GXTVBB)
		WRITE_VPP_REG_BITS(VPP_VADJ1_Y, val, 8, 9);
	else
		WRITE_VPP_REG_BITS(VPP_VADJ1_Y, val << 1, 8, 10);

	WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, 1, 0, 1);

	return count;
}

static ssize_t video_adj1_contrast_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n",
			(int)(READ_VPP_REG(VPP_VADJ1_Y) & 0xff) - 0x80);
}

static ssize_t video_adj1_contrast_store(struct class *cla,
			struct class_attribute *attr,
			const char *buf, size_t count)
{
	size_t r;
	int val;

	r = sscanf(buf, "%d\n", &val);
	if ((r != 1) || (val < -127) || (val > 127))
		return -EINVAL;

	val += 0x80;

	WRITE_VPP_REG_BITS(VPP_VADJ1_Y, val, 0, 8);
	WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, 1, 0, 1);

	return count;
}

/* video adj2 */
static ssize_t video_adj2_brightness_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	s32 val = 0;

	if (get_cpu_type() <= MESON_CPU_MAJOR_ID_GXTVBB) {
		val = (READ_VPP_REG(VPP_VADJ2_Y) >> 8) & 0x1ff;
		val = (val << 23) >> 23;

		return sprintf(buf, "%d\n", val);
	}
	val = (READ_VPP_REG(VPP_VADJ2_Y) >> 8) & 0x3ff;
	val = (val << 23) >> 23;

	return sprintf(buf, "%d\n", val >> 1);
}

static ssize_t video_adj2_brightness_store(struct class *cla,
			struct class_attribute *attr,
			const char *buf, size_t count)
{
	size_t r;
	int val;

	r = sscanf(buf, "%d\n", &val);
	if ((r != 1) || (val < -255) || (val > 255))
		return -EINVAL;

	if (get_cpu_type() <= MESON_CPU_MAJOR_ID_GXTVBB)
		WRITE_VPP_REG_BITS(VPP_VADJ2_Y, val, 8, 9);
	else
		WRITE_VPP_REG_BITS(VPP_VADJ2_Y, val << 1, 8, 10);

	WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, 1, 2, 1);

	return count;
}

static ssize_t video_adj2_contrast_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n",
			(int)(READ_VPP_REG(VPP_VADJ2_Y) & 0xff) - 0x80);
}

static ssize_t video_adj2_contrast_store(struct class *cla,
			struct class_attribute *attr,
			const char *buf, size_t count)
{
	size_t r;
	int val;

	r = sscanf(buf, "%d\n", &val);
	if ((r != 1) || (val < -127) || (val > 127))
		return -EINVAL;

	val += 0x80;

	WRITE_VPP_REG_BITS(VPP_VADJ2_Y, val, 0, 8);
	WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, 1, 2, 1);

	return count;
}

static ssize_t amvecm_usage_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	pr_info("Usage:");
	pr_info("brightness_val range:-255~255\n");
	pr_info("contrast_val range:-127~127\n");
	pr_info("saturation_val range:-128~128\n");
	pr_info("hue_val range:-25~25\n");
	pr_info("************video brightness & contrast & saturation_hue adj as flow*************\n");
	pr_info("echo brightness_val > /sys/class/amvecm/brightness1\n");
	pr_info("echo contrast_val > /sys/class/amvecm/contrast1\n");
	pr_info("echo saturation_val hue_val > /sys/class/amvecm/saturation_hue_pre\n");
	pr_info("************after video+osd blender, brightness & contrast & saturation_hue adj as flow*************\n");
	pr_info("echo brightness_val > /sys/class/amvecm/brightness2\n");
	pr_info("echo contrast_val > /sys/class/amvecm/contrast2\n");
	pr_info("echo saturation_val hue_val > /sys/class/amvecm/saturation_hue_post\n");
	return 0;
}

static void parse_param_amvecm(char *buf_orig, char **parm)
{
	char *ps, *token;
	unsigned int n = 0;
	char delim1[3] = " ";
	char delim2[2] = "\n";

	ps = buf_orig;
	strcat(delim1, delim2);
	while (1) {
		token = strsep(&ps, delim1);
		if (token == NULL)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
}
static void amvecm_3d_sync_status(void)
{
	unsigned int sync_h_start, sync_h_end, sync_v_start,
		sync_v_end, sync_polarity,
		sync_out_inv, sync_en;
	if (!is_meson_gxtvbb_cpu()) {
		pr_info("\n chip does not support 3D sync process!!!\n");
		return;
	}
	sync_h_start = READ_VPP_REG_BITS(VPU_VPU_3D_SYNC2, 0, 13);
	sync_h_end = READ_VPP_REG_BITS(VPU_VPU_3D_SYNC2, 16, 13);
	sync_v_start = READ_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 0, 13);
	sync_v_end = READ_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 16, 13);
	sync_polarity = READ_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 29, 1);
	sync_out_inv = READ_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 15, 1);
	sync_en = READ_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 31, 1);
	pr_info("\n current 3d sync state:\n");
	pr_info("sync_h_start:%d\n", sync_h_start);
	pr_info("sync_h_end:%d\n", sync_h_end);
	pr_info("sync_v_start:%d\n", sync_v_start);
	pr_info("sync_v_end:%d\n", sync_v_end);
	pr_info("sync_polarity:%d\n", sync_polarity);
	pr_info("sync_out_inv:%d\n", sync_out_inv);
	pr_info("sync_en:%d\n", sync_en);
	pr_info("sync_3d_black_color:%d\n", sync_3d_black_color);
	pr_info("sync_3d_sync_to_vbo:%d\n", sync_3d_sync_to_vbo);
}
static ssize_t amvecm_3d_sync_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	ssize_t len = 0;

	len += sprintf(buf+len,
		"echo hstart val(D) > /sys/class/amvecm/sync_3d\n");
	len += sprintf(buf+len,
		"echo hend val(D) > /sys/class/amvecm/sync_3d\n");
	len += sprintf(buf+len,
		"echo vstart val(D) > /sys/class/amvecm/sync_3d\n");
	len += sprintf(buf+len,
		"echo vend val(D) > /sys/class/amvecm/sync_3d\n");
	len += sprintf(buf+len,
		"echo pola val(D) > /sys/class/amvecm/sync_3d\n");
	len += sprintf(buf+len,
		"echo inv val(D) > /sys/class/amvecm/sync_3d\n");
	len += sprintf(buf+len,
		"echo black_color val(Hex) > /sys/class/amvecm/sync_3d\n");
	len += sprintf(buf+len,
		"echo sync_to_vx1 val(D) > /sys/class/amvecm/sync_3d\n");
	len += sprintf(buf+len,
		"echo enable > /sys/class/amvecm/sync_3d\n");
	len += sprintf(buf+len,
		"echo disable > /sys/class/amvecm/sync_3d\n");
	len += sprintf(buf+len,
		"echo status > /sys/class/amvecm/sync_3d\n");
	return len;
}

static ssize_t amvecm_3d_sync_store(struct class *cla,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	char *buf_orig, *parm[8] = {NULL};
	long val;

	if (!buf)
		return count;

	if (!is_meson_gxtvbb_cpu()) {
		pr_info("\n chip does not support 3D sync process!!!\n");
		return count;
	}

	buf_orig = kstrdup(buf, GFP_KERNEL);
	parse_param_amvecm(buf_orig, (char **)&parm);
	if (!strncmp(parm[0], "hstart", 6)) {
		if (kstrtol(parm[1], 10, &val) < 0)
			return -EINVAL;
		sync_3d_h_start = val&0x1fff;
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC2, sync_3d_h_start, 0, 13);
	} else if (!strncmp(parm[0], "hend", 4)) {
		if (kstrtol(parm[1], 10, &val) < 0)
			return -EINVAL;
		sync_3d_h_end = val&0x1fff;
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC2, sync_3d_h_end, 16, 13);
	} else if (!strncmp(parm[0], "vstart", 6)) {
		if (kstrtol(parm[1], 10, &val) < 0)
			return -EINVAL;
		sync_3d_v_start = val&0x1fff;
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_v_start, 0, 13);
	} else if (!strncmp(parm[0], "vend", 4)) {
		if (kstrtol(parm[1], 10, &val) < 0)
			return -EINVAL;
		sync_3d_v_end = val&0x1fff;
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_v_end, 16, 13);
	} else if (!strncmp(parm[0], "pola", 4)) {
		if (kstrtol(parm[1], 10, &val) < 0)
			return -EINVAL;
		sync_3d_polarity = val&0x1;
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_polarity, 29, 1);
	} else if (!strncmp(parm[0], "inv", 3)) {
		if (kstrtol(parm[1], 10, &val) < 0)
			return -EINVAL;
		sync_3d_out_inv = val&0x1;
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_out_inv, 15, 1);
	} else if (!strncmp(parm[0], "black_color", 11)) {
		if (kstrtol(parm[1], 16, &val) < 0)
			return -EINVAL;
		sync_3d_black_color = val&0xffffff;
		WRITE_VPP_REG_BITS(VPP_BLEND_ONECOLOR_CTRL,
			sync_3d_black_color, 0, 24);
	} else if (!strncmp(parm[0], "sync_to_vx1", 11)) {
		if (kstrtol(parm[1], 10, &val) < 0)
			return -EINVAL;
		sync_3d_sync_to_vbo = val&0x1;
	} else if (!strncmp(parm[0], "enable", 6)) {
		vecm_latch_flag |= FLAG_3D_SYNC_EN;
	} else if (!strncmp(parm[0], "disable", 7)) {
		vecm_latch_flag |= FLAG_3D_SYNC_DIS;
	} else if (!strncmp(parm[0], "status", 7)) {
		amvecm_3d_sync_status();
	}
	kfree(buf_orig);
	return count;
}

static ssize_t amvecm_vlock_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	ssize_t len = 0;

	len += sprintf(buf+len,
		"echo vlock_mode val(0/1/2) > /sys/class/amvecm/vlock\n");
	len += sprintf(buf+len,
		"echo vlock_en val(0/1) > /sys/class/amvecm/vlock\n");
	len += sprintf(buf+len,
		"echo vlock_adapt val(0/1) > /sys/class/amvecm/vlock\n");
	len += sprintf(buf+len,
		"echo vlock_dis_cnt_limit val(D) > /sys/class/amvecm/vlock\n");
	len += sprintf(buf+len,
		"echo vlock_delta_limit_frac val(D) > /sys/class/amvecm/vlock\n");
	len += sprintf(buf+len,
		"echo vlock_delta_limit_m val(D) > /sys/class/amvecm/vlock\n");
	len += sprintf(buf+len,
		"echo vlock_debug val(0x111) > /sys/class/amvecm/vlock\n");
	len += sprintf(buf+len,
		"echo vlock_dynamic_adjust val(0/1) > /sys/class/amvecm/vlock\n");
	len += sprintf(buf+len,
		"echo vlock_dis_cnt_step1_limit val(D) > /sys/class/amvecm/vlock\n");
	len += sprintf(buf+len,
		"echo vlock_cnt_step1_limit val(D) > /sys/class/amvecm/vlock\n");
	len += sprintf(buf+len,
		"echo enable > /sys/class/amvecm/vlock\n");
	len += sprintf(buf+len,
		"echo disable > /sys/class/amvecm/vlock\n");
	len += sprintf(buf+len,
		"echo status > /sys/class/amvecm/vlock\n");
	len += sprintf(buf+len,
		"echo dump_reg > /sys/class/amvecm/vlock\n");
	return len;
}

static ssize_t amvecm_vlock_store(struct class *cla,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	char *buf_orig, *parm[8] = {NULL};
	long val;
	unsigned int temp_val;
	enum vlock_param_e sel = VLOCK_PARAM_MAX;

	if (!buf)
		return count;
	if (!is_meson_gxtvbb_cpu() &&
		!is_meson_gxbb_cpu() &&
		(get_cpu_type() < MESON_CPU_MAJOR_ID_GXL)) {
		pr_info("\n chip does not support vlock process!!!\n");
		return count;
	}

	buf_orig = kstrdup(buf, GFP_KERNEL);
	parse_param_amvecm(buf_orig, (char **)&parm);
	if (!strncmp(parm[0], "vlock_mode", 10)) {
		if (kstrtol(parm[1], 10, &val) < 0)
			return -EINVAL;
		temp_val = val;
		sel = VLOCK_MODE;
	} else if (!strncmp(parm[0], "vlock_en", 8)) {
		if (kstrtol(parm[1], 10, &val) < 0)
			return -EINVAL;
		temp_val = val;
		sel = VLOCK_EN;
	} else if (!strncmp(parm[0], "vlock_adapt", 11)) {
		if (kstrtol(parm[1], 10, &val) < 0)
			return -EINVAL;
		temp_val = val;
		sel = VLOCK_ADAPT;
	} else if (!strncmp(parm[0], "vlock_dis_cnt_limit", 19)) {
		if (kstrtol(parm[1], 10, &val) < 0)
			return -EINVAL;
		temp_val = val;
		sel = VLOCK_DIS_CNT_LIMIT;
	} else if (!strncmp(parm[0], "vlock_delta_limit_frac", 22)) {
		if (kstrtol(parm[1], 10, &val) < 0)
			return -EINVAL;
		temp_val = val;
		sel = VLOCK_DELTA_LIMIT_FRAC;
	} else if (!strncmp(parm[0], "vlock_delta_limit_m", 19)) {
		if (kstrtol(parm[1], 10, &val) < 0)
			return -EINVAL;
		temp_val = val;
		sel = VLOCK_DELTA_LIMIT_M;
	} else if (!strncmp(parm[0], "vlock_debug", 11)) {
		if (kstrtol(parm[1], 16, &val) < 0)
			return -EINVAL;
		temp_val = val;
		sel = VLOCK_DEBUG;
	} else if (!strncmp(parm[0], "vlock_dynamic_adjust", 20)) {
		if (kstrtol(parm[1], 10, &val) < 0)
			return -EINVAL;
		temp_val = val;
		sel = VLOCK_DYNAMIC_ADJUST;
	} else if (!strncmp(parm[0], "vlock_dis_cnt_step1_limit", 25)) {
		if (kstrtol(parm[1], 10, &val) < 0)
			return -EINVAL;
		temp_val = val;
		sel = VLOCK_DIS_CNT_STEP1_LIMIT;
	} else if (!strncmp(parm[0], "vlock_cnt_step1_limit", 21)) {
		if (kstrtol(parm[1], 10, &val) < 0)
			return -EINVAL;
		temp_val = val;
		sel = VLOCK_EN_CNT_STEP1_LIMIT;
	} else if (!strncmp(parm[0], "enable", 6)) {
		vecm_latch_flag |= FLAG_VLOCK_EN;
	} else if (!strncmp(parm[0], "disable", 7)) {
		vecm_latch_flag |= FLAG_VLOCK_DIS;
	} else if (!strncmp(parm[0], "status", 6)) {
		vlock_status();
	} else if (!strncmp(parm[0], "dump_reg", 8)) {
		vlock_reg_dump();
	} else {
		pr_info("unsupport cmd!!\n");
	}
	if (sel < VLOCK_PARAM_MAX)
		vlock_param_set(temp_val, sel);
	kfree(buf_orig);
	return count;
}

/* #endif */

static void vpp_backup_histgram(struct vframe_s *vf)
{
	unsigned int i = 0;

	vpp_hist_param.vpp_hist_pow = vf->prop.hist.hist_pow;
	vpp_hist_param.vpp_luma_sum = vf->prop.hist.vpp_luma_sum;
	vpp_hist_param.vpp_pixel_sum = vf->prop.hist.vpp_pixel_sum;
	for (i = 0; i < 64; i++)
		vpp_hist_param.vpp_histgram[i] = vf->prop.hist.vpp_gamma[i];
}

static void vpp_dump_histgram(void)
{
	uint i;

	pr_info("%s:\n", __func__);
	for (i = 0; i < 64; i++) {
		pr_info("[%d]0x%-8x\t", i, vpp_hist_param.vpp_histgram[i]);
		if ((i+1)%8 == 0)
			pr_info("\n");
	}
}

void vpp_get_hist_en(void)
{
	WRITE_VPP_REG_BITS(VI_HIST_CTRL, 0x1, 11, 3);
	WRITE_VPP_REG_BITS(VI_HIST_CTRL, 0x1, 0, 1);
	WRITE_VPP_REG(VI_HIST_GCLK_CTRL, 0xffffffff);
	WRITE_VPP_REG_BITS(VI_HIST_CTRL, 2, VI_HIST_POW_BIT, VI_HIST_POW_WID);
}

void vpp_get_vframe_hist_info(struct vframe_s *vf)
{
	unsigned int hist_height, hist_width;

	hist_height = READ_VPP_REG_BITS(VPP_IN_H_V_SIZE, 0, 13);
	hist_width = READ_VPP_REG_BITS(VPP_IN_H_V_SIZE, 16, 13);

	if ((hist_height != pre_hist_height) ||
		(hist_width != pre_hist_width)) {
		pre_hist_height = hist_height;
		pre_hist_width = hist_width;
		WRITE_VPP_REG_BITS(VI_HIST_PIC_SIZE, hist_height, 16, 13);
		WRITE_VPP_REG_BITS(VI_HIST_PIC_SIZE, hist_width, 0, 13);
	}
	/* fetch hist info */
	/* vf->prop.hist.luma_sum   = READ_CBUS_REG_BITS(VDIN_HIST_SPL_VAL,*/
	/* HIST_LUMA_SUM_BIT,    HIST_LUMA_SUM_WID   ); */
	vf->prop.hist.hist_pow   = READ_VPP_REG_BITS(VI_HIST_CTRL,
			VI_HIST_POW_BIT, VI_HIST_POW_WID);
	vf->prop.hist.vpp_luma_sum   = READ_VPP_REG(VI_HIST_SPL_VAL);
	/* vf->prop.hist.chroma_sum = READ_CBUS_REG_BITS(VDIN_HIST_CHROMA_SUM,*/
	/* HIST_CHROMA_SUM_BIT,  HIST_CHROMA_SUM_WID ); */
	vf->prop.hist.vpp_chroma_sum = READ_VPP_REG(VI_HIST_CHROMA_SUM);
	vf->prop.hist.vpp_pixel_sum  = READ_VPP_REG_BITS(VI_HIST_SPL_PIX_CNT,
			VI_HIST_PIX_CNT_BIT, VI_HIST_PIX_CNT_WID);
	vf->prop.hist.vpp_height     = READ_VPP_REG_BITS(VI_HIST_PIC_SIZE,
			VI_HIST_PIC_HEIGHT_BIT, VI_HIST_PIC_HEIGHT_WID);
	vf->prop.hist.vpp_width      = READ_VPP_REG_BITS(VI_HIST_PIC_SIZE,
			VI_HIST_PIC_WIDTH_BIT, VI_HIST_PIC_WIDTH_WID);
	vf->prop.hist.vpp_luma_max   = READ_VPP_REG_BITS(VI_HIST_MAX_MIN,
			VI_HIST_MAX_BIT, VI_HIST_MAX_WID);
	vf->prop.hist.vpp_luma_min   = READ_VPP_REG_BITS(VI_HIST_MAX_MIN,
			VI_HIST_MIN_BIT, VI_HIST_MIN_WID);
	vf->prop.hist.vpp_gamma[0]   = READ_VPP_REG_BITS(VI_DNLP_HIST00,
			VI_HIST_ON_BIN_00_BIT, VI_HIST_ON_BIN_00_WID);
	vf->prop.hist.vpp_gamma[1]   = READ_VPP_REG_BITS(VI_DNLP_HIST00,
			VI_HIST_ON_BIN_01_BIT, VI_HIST_ON_BIN_01_WID);
	vf->prop.hist.vpp_gamma[2]   = READ_VPP_REG_BITS(VI_DNLP_HIST01,
			VI_HIST_ON_BIN_02_BIT, VI_HIST_ON_BIN_02_WID);
	vf->prop.hist.vpp_gamma[3]   = READ_VPP_REG_BITS(VI_DNLP_HIST01,
			VI_HIST_ON_BIN_03_BIT, VI_HIST_ON_BIN_03_WID);
	vf->prop.hist.vpp_gamma[4]   = READ_VPP_REG_BITS(VI_DNLP_HIST02,
			VI_HIST_ON_BIN_04_BIT, VI_HIST_ON_BIN_04_WID);
	vf->prop.hist.vpp_gamma[5]   = READ_VPP_REG_BITS(VI_DNLP_HIST02,
			VI_HIST_ON_BIN_05_BIT, VI_HIST_ON_BIN_05_WID);
	vf->prop.hist.vpp_gamma[6]   = READ_VPP_REG_BITS(VI_DNLP_HIST03,
			VI_HIST_ON_BIN_06_BIT, VI_HIST_ON_BIN_06_WID);
	vf->prop.hist.vpp_gamma[7]   = READ_VPP_REG_BITS(VI_DNLP_HIST03,
			VI_HIST_ON_BIN_07_BIT, VI_HIST_ON_BIN_07_WID);
	vf->prop.hist.vpp_gamma[8]   = READ_VPP_REG_BITS(VI_DNLP_HIST04,
			VI_HIST_ON_BIN_08_BIT, VI_HIST_ON_BIN_08_WID);
	vf->prop.hist.vpp_gamma[9]   = READ_VPP_REG_BITS(VI_DNLP_HIST04,
			VI_HIST_ON_BIN_09_BIT, VI_HIST_ON_BIN_09_WID);
	vf->prop.hist.vpp_gamma[10]  = READ_VPP_REG_BITS(VI_DNLP_HIST05,
			VI_HIST_ON_BIN_10_BIT, VI_HIST_ON_BIN_10_WID);
	vf->prop.hist.vpp_gamma[11]  = READ_VPP_REG_BITS(VI_DNLP_HIST05,
			VI_HIST_ON_BIN_11_BIT, VI_HIST_ON_BIN_11_WID);
	vf->prop.hist.vpp_gamma[12]  = READ_VPP_REG_BITS(VI_DNLP_HIST06,
			VI_HIST_ON_BIN_12_BIT, VI_HIST_ON_BIN_12_WID);
	vf->prop.hist.vpp_gamma[13]  = READ_VPP_REG_BITS(VI_DNLP_HIST06,
			VI_HIST_ON_BIN_13_BIT, VI_HIST_ON_BIN_13_WID);
	vf->prop.hist.vpp_gamma[14]  = READ_VPP_REG_BITS(VI_DNLP_HIST07,
			VI_HIST_ON_BIN_14_BIT, VI_HIST_ON_BIN_14_WID);
	vf->prop.hist.vpp_gamma[15]  = READ_VPP_REG_BITS(VI_DNLP_HIST07,
			VI_HIST_ON_BIN_15_BIT, VI_HIST_ON_BIN_15_WID);
	vf->prop.hist.vpp_gamma[16]  = READ_VPP_REG_BITS(VI_DNLP_HIST08,
			VI_HIST_ON_BIN_16_BIT, VI_HIST_ON_BIN_16_WID);
	vf->prop.hist.vpp_gamma[17]  = READ_VPP_REG_BITS(VI_DNLP_HIST08,
			VI_HIST_ON_BIN_17_BIT, VI_HIST_ON_BIN_17_WID);
	vf->prop.hist.vpp_gamma[18]  = READ_VPP_REG_BITS(VI_DNLP_HIST09,
			VI_HIST_ON_BIN_18_BIT, VI_HIST_ON_BIN_18_WID);
	vf->prop.hist.vpp_gamma[19]  = READ_VPP_REG_BITS(VI_DNLP_HIST09,
			VI_HIST_ON_BIN_19_BIT, VI_HIST_ON_BIN_19_WID);
	vf->prop.hist.vpp_gamma[20]  = READ_VPP_REG_BITS(VI_DNLP_HIST10,
			VI_HIST_ON_BIN_20_BIT, VI_HIST_ON_BIN_20_WID);
	vf->prop.hist.vpp_gamma[21]  = READ_VPP_REG_BITS(VI_DNLP_HIST10,
			VI_HIST_ON_BIN_21_BIT, VI_HIST_ON_BIN_21_WID);
	vf->prop.hist.vpp_gamma[22]  = READ_VPP_REG_BITS(VI_DNLP_HIST11,
			VI_HIST_ON_BIN_22_BIT, VI_HIST_ON_BIN_22_WID);
	vf->prop.hist.vpp_gamma[23]  = READ_VPP_REG_BITS(VI_DNLP_HIST11,
			VI_HIST_ON_BIN_23_BIT, VI_HIST_ON_BIN_23_WID);
	vf->prop.hist.vpp_gamma[24]  = READ_VPP_REG_BITS(VI_DNLP_HIST12,
			VI_HIST_ON_BIN_24_BIT, VI_HIST_ON_BIN_24_WID);
	vf->prop.hist.vpp_gamma[25]  = READ_VPP_REG_BITS(VI_DNLP_HIST12,
			VI_HIST_ON_BIN_25_BIT, VI_HIST_ON_BIN_25_WID);
	vf->prop.hist.vpp_gamma[26]  = READ_VPP_REG_BITS(VI_DNLP_HIST13,
			VI_HIST_ON_BIN_26_BIT, VI_HIST_ON_BIN_26_WID);
	vf->prop.hist.vpp_gamma[27]  = READ_VPP_REG_BITS(VI_DNLP_HIST13,
			VI_HIST_ON_BIN_27_BIT, VI_HIST_ON_BIN_27_WID);
	vf->prop.hist.vpp_gamma[28]  = READ_VPP_REG_BITS(VI_DNLP_HIST14,
			VI_HIST_ON_BIN_28_BIT, VI_HIST_ON_BIN_28_WID);
	vf->prop.hist.vpp_gamma[29]  = READ_VPP_REG_BITS(VI_DNLP_HIST14,
			VI_HIST_ON_BIN_29_BIT, VI_HIST_ON_BIN_29_WID);
	vf->prop.hist.vpp_gamma[30]  = READ_VPP_REG_BITS(VI_DNLP_HIST15,
			VI_HIST_ON_BIN_30_BIT, VI_HIST_ON_BIN_30_WID);
	vf->prop.hist.vpp_gamma[31]  = READ_VPP_REG_BITS(VI_DNLP_HIST15,
			VI_HIST_ON_BIN_31_BIT, VI_HIST_ON_BIN_31_WID);
	vf->prop.hist.vpp_gamma[32]  = READ_VPP_REG_BITS(VI_DNLP_HIST16,
			VI_HIST_ON_BIN_32_BIT, VI_HIST_ON_BIN_32_WID);
	vf->prop.hist.vpp_gamma[33]  = READ_VPP_REG_BITS(VI_DNLP_HIST16,
			VI_HIST_ON_BIN_33_BIT, VI_HIST_ON_BIN_33_WID);
	vf->prop.hist.vpp_gamma[34]  = READ_VPP_REG_BITS(VI_DNLP_HIST17,
			VI_HIST_ON_BIN_34_BIT, VI_HIST_ON_BIN_34_WID);
	vf->prop.hist.vpp_gamma[35]  = READ_VPP_REG_BITS(VI_DNLP_HIST17,
			VI_HIST_ON_BIN_35_BIT, VI_HIST_ON_BIN_35_WID);
	vf->prop.hist.vpp_gamma[36]  = READ_VPP_REG_BITS(VI_DNLP_HIST18,
			VI_HIST_ON_BIN_36_BIT, VI_HIST_ON_BIN_36_WID);
	vf->prop.hist.vpp_gamma[37]  = READ_VPP_REG_BITS(VI_DNLP_HIST18,
			VI_HIST_ON_BIN_37_BIT, VI_HIST_ON_BIN_37_WID);
	vf->prop.hist.vpp_gamma[38]  = READ_VPP_REG_BITS(VI_DNLP_HIST19,
			VI_HIST_ON_BIN_38_BIT, VI_HIST_ON_BIN_38_WID);
	vf->prop.hist.vpp_gamma[39]  = READ_VPP_REG_BITS(VI_DNLP_HIST19,
			VI_HIST_ON_BIN_39_BIT, VI_HIST_ON_BIN_39_WID);
	vf->prop.hist.vpp_gamma[40]  = READ_VPP_REG_BITS(VI_DNLP_HIST20,
			VI_HIST_ON_BIN_40_BIT, VI_HIST_ON_BIN_40_WID);
	vf->prop.hist.vpp_gamma[41]  = READ_VPP_REG_BITS(VI_DNLP_HIST20,
			VI_HIST_ON_BIN_41_BIT, VI_HIST_ON_BIN_41_WID);
	vf->prop.hist.vpp_gamma[42]  = READ_VPP_REG_BITS(VI_DNLP_HIST21,
			VI_HIST_ON_BIN_42_BIT, VI_HIST_ON_BIN_42_WID);
	vf->prop.hist.vpp_gamma[43]  = READ_VPP_REG_BITS(VI_DNLP_HIST21,
			VI_HIST_ON_BIN_43_BIT, VI_HIST_ON_BIN_43_WID);
	vf->prop.hist.vpp_gamma[44]  = READ_VPP_REG_BITS(VI_DNLP_HIST22,
			VI_HIST_ON_BIN_44_BIT, VI_HIST_ON_BIN_44_WID);
	vf->prop.hist.vpp_gamma[45]  = READ_VPP_REG_BITS(VI_DNLP_HIST22,
			VI_HIST_ON_BIN_45_BIT, VI_HIST_ON_BIN_45_WID);
	vf->prop.hist.vpp_gamma[46]  = READ_VPP_REG_BITS(VI_DNLP_HIST23,
			VI_HIST_ON_BIN_46_BIT, VI_HIST_ON_BIN_46_WID);
	vf->prop.hist.vpp_gamma[47]  = READ_VPP_REG_BITS(VI_DNLP_HIST23,
			VI_HIST_ON_BIN_47_BIT, VI_HIST_ON_BIN_47_WID);
	vf->prop.hist.vpp_gamma[48]  = READ_VPP_REG_BITS(VI_DNLP_HIST24,
			VI_HIST_ON_BIN_48_BIT, VI_HIST_ON_BIN_48_WID);
	vf->prop.hist.vpp_gamma[49]  = READ_VPP_REG_BITS(VI_DNLP_HIST24,
			VI_HIST_ON_BIN_49_BIT, VI_HIST_ON_BIN_49_WID);
	vf->prop.hist.vpp_gamma[50]  = READ_VPP_REG_BITS(VI_DNLP_HIST25,
			VI_HIST_ON_BIN_50_BIT, VI_HIST_ON_BIN_50_WID);
	vf->prop.hist.vpp_gamma[51]  = READ_VPP_REG_BITS(VI_DNLP_HIST25,
			VI_HIST_ON_BIN_51_BIT, VI_HIST_ON_BIN_51_WID);
	vf->prop.hist.vpp_gamma[52]  = READ_VPP_REG_BITS(VI_DNLP_HIST26,
			VI_HIST_ON_BIN_52_BIT, VI_HIST_ON_BIN_52_WID);
	vf->prop.hist.vpp_gamma[53]  = READ_VPP_REG_BITS(VI_DNLP_HIST26,
			VI_HIST_ON_BIN_53_BIT, VI_HIST_ON_BIN_53_WID);
	vf->prop.hist.vpp_gamma[54]  = READ_VPP_REG_BITS(VI_DNLP_HIST27,
			VI_HIST_ON_BIN_54_BIT, VI_HIST_ON_BIN_54_WID);
	vf->prop.hist.vpp_gamma[55]  = READ_VPP_REG_BITS(VI_DNLP_HIST27,
			VI_HIST_ON_BIN_55_BIT, VI_HIST_ON_BIN_55_WID);
	vf->prop.hist.vpp_gamma[56]  = READ_VPP_REG_BITS(VI_DNLP_HIST28,
			VI_HIST_ON_BIN_56_BIT, VI_HIST_ON_BIN_56_WID);
	vf->prop.hist.vpp_gamma[57]  = READ_VPP_REG_BITS(VI_DNLP_HIST28,
			VI_HIST_ON_BIN_57_BIT, VI_HIST_ON_BIN_57_WID);
	vf->prop.hist.vpp_gamma[58]  = READ_VPP_REG_BITS(VI_DNLP_HIST29,
			VI_HIST_ON_BIN_58_BIT, VI_HIST_ON_BIN_58_WID);
	vf->prop.hist.vpp_gamma[59]  = READ_VPP_REG_BITS(VI_DNLP_HIST29,
			VI_HIST_ON_BIN_59_BIT, VI_HIST_ON_BIN_59_WID);
	vf->prop.hist.vpp_gamma[60]  = READ_VPP_REG_BITS(VI_DNLP_HIST30,
			VI_HIST_ON_BIN_60_BIT, VI_HIST_ON_BIN_60_WID);
	vf->prop.hist.vpp_gamma[61]  = READ_VPP_REG_BITS(VI_DNLP_HIST30,
			VI_HIST_ON_BIN_61_BIT, VI_HIST_ON_BIN_61_WID);
	vf->prop.hist.vpp_gamma[62]  = READ_VPP_REG_BITS(VI_DNLP_HIST31,
			VI_HIST_ON_BIN_62_BIT, VI_HIST_ON_BIN_62_WID);
	vf->prop.hist.vpp_gamma[63]  = READ_VPP_REG_BITS(VI_DNLP_HIST31,
			VI_HIST_ON_BIN_63_BIT, VI_HIST_ON_BIN_63_WID);
}

static void ioctrl_get_hdr_metadata(struct vframe_s *vf)
{
	if (((vf->signal_type >> 16) & 0xff) == 9) {
		if (vf->prop.master_display_colour.present_flag) {

			memcpy(vpp_hdr_metadata_s.primaries,
				vf->prop.master_display_colour.primaries,
				sizeof(u32)*6);
			memcpy(vpp_hdr_metadata_s.white_point,
				vf->prop.master_display_colour.white_point,
				sizeof(u32)*2);
			vpp_hdr_metadata_s.luminance[0] =
				vf->prop.master_display_colour.luminance[0];
			vpp_hdr_metadata_s.luminance[1] =
				vf->prop.master_display_colour.luminance[1];
		} else
			memset(vpp_hdr_metadata_s.primaries, 0,
					10 * sizeof(unsigned int));
	} else
		memset(vpp_hdr_metadata_s.primaries, 0,
				10 * sizeof(unsigned int));
}

void vpp_demo_config(struct vframe_s *vf)
{
	unsigned int reg_value;
	/*dnlp demo config*/
	if (vpp_demo_latch_flag & VPP_DEMO_DNLP_EN) {
		WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 1, 18, 1);
		/*bit14-15   left: 2   right: 3*/
		WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 2, 14, 2);
		reg_value = READ_VPP_REG_BITS(VPP_SRSHARP1_CTRL, 0, 1);
		if (((vf->height > 1080) && (vf->width > 1920)) ||
			(reg_value == 0))
			WRITE_VPP_REG_BITS(VPP_VE_DEMO_LEFT_TOP_SCREEN_WIDTH,
				1920, 0, 12);
		else
			WRITE_VPP_REG_BITS(VPP_VE_DEMO_LEFT_TOP_SCREEN_WIDTH,
				960, 0, 12);
		vpp_demo_latch_flag &= ~VPP_DEMO_DNLP_EN;
	} else if (vpp_demo_latch_flag & VPP_DEMO_DNLP_DIS) {
		WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 0, 18, 1);
		WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 0, 14, 2);
		WRITE_VPP_REG_BITS(VPP_VE_DEMO_LEFT_TOP_SCREEN_WIDTH,
				0xfff, 0, 12);
		vpp_demo_latch_flag &= ~VPP_DEMO_DNLP_DIS;
	}
	/*cm demo config*/
	if (vpp_demo_latch_flag & VPP_DEMO_CM_EN) {
		/*left: 0x1   right: 0x4*/
		WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, 0x20f);
		WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0x1);
		vpp_demo_latch_flag &= ~VPP_DEMO_CM_EN;
	} else if (vpp_demo_latch_flag & VPP_DEMO_CM_DIS) {
		WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, 0x20f);
		WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0x0);
		vpp_demo_latch_flag &= ~VPP_DEMO_CM_DIS;
	}
}

void amvecm_video_latch(void)
{
	pc_mode_process();
	cm_latch_process();
	amvecm_size_patch();
	ve_dnlp_latch_process();
	ve_lcd_gamma_process();
	lvds_freq_process();
/* #if (MESON_CPU_TYPE >= MESON_CPU_TYPE_MESONG9TV) */
	if (0) {
		amvecm_3d_sync_process();
		amvecm_3d_black_process();
	}
/* #endif */
}

void amvecm_on_vs(struct vframe_s *vf)
{
	if ((probe_ok == 0) ||
	(is_meson_gxm_cpu() && is_dolby_vision_on()))
		return;

	if (vf != NULL) {
		/* matrix ajust */
		amvecm_matrix_process(vf);

		amvecm_bricon_process(
			vd1_brightness,
			vd1_contrast + vd1_contrast_offset, vf);

		ioctrl_get_hdr_metadata(vf);
		amvecm_color_process(
			saturation_pre + saturation_offset
			+ satu_shift_by_con,
			hue_pre, vf);

		vpp_demo_config(vf);
	} else
		amvecm_matrix_process(NULL);
	/* vlock processs */
	if ((get_cpu_type() >=
		MESON_CPU_MAJOR_ID_GXBB) && (vf != NULL))
		amve_vlock_process(vf);
	else if ((get_cpu_type() >=
		MESON_CPU_MAJOR_ID_GXBB) && (vf == NULL))
		amve_vlock_resume();

	/* pq latch process */
	amvecm_video_latch();
}
EXPORT_SYMBOL(amvecm_on_vs);


void refresh_on_vs(struct vframe_s *vf)
{
	if (is_meson_gxm_cpu() && is_dolby_vision_on())
		return;
	if (vf != NULL) {
		vpp_get_vframe_hist_info(vf);
		ve_on_vs(vf);
		vpp_backup_histgram(vf);
	}
}
EXPORT_SYMBOL(refresh_on_vs);

static int amvecm_open(struct inode *inode, struct file *file)
{
	struct amvecm_dev_s *devp;
	/* Get the per-device structure that contains this cdev */
	devp = container_of(inode->i_cdev, struct amvecm_dev_s, cdev);
	file->private_data = devp;
	return 0;
}

static int amvecm_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}
static struct am_regs_s amregs_ext;

static long amvecm_ioctl(struct file *file,
		unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	void __user *argp;

	pr_amvecm_dbg("[amvecm..] %s: cmd_nr = 0x%x\n",
			__func__, _IOC_NR(cmd));

	if (probe_ok == 0)
		return ret;
	switch (cmd) {
	case AMVECM_IOC_LOAD_REG:
		if (pq_load_en == 0) {
			ret = -EBUSY;
			pr_amvecm_dbg("[amvecm..] pq ioctl function disabled !!\n");
			return ret;
		}

		if ((vecm_latch_flag & FLAG_REG_MAP0) &&
			(vecm_latch_flag & FLAG_REG_MAP1) &&
			(vecm_latch_flag & FLAG_REG_MAP2) &&
			(vecm_latch_flag & FLAG_REG_MAP3) &&
			(vecm_latch_flag & FLAG_REG_MAP4) &&
			(vecm_latch_flag & FLAG_REG_MAP5)) {
			ret = -EBUSY;
			pr_amvecm_dbg("load regs error: loading regs, please wait\n");
			break;
		}
		if (copy_from_user(&amregs_ext,
				(void __user *)arg, sizeof(struct am_regs_s))) {
			pr_amvecm_dbg("0x%x load reg errors: can't get buffer length\n",
					FLAG_REG_MAP0);
			ret = -EFAULT;
		} else
			ret = cm_load_reg(&amregs_ext);
		break;
	case AMVECM_IOC_VE_DNLP_EN:
		vecm_latch_flag |= FLAG_VE_DNLP_EN;
		break;
	case AMVECM_IOC_VE_DNLP_DIS:
		vecm_latch_flag |= FLAG_VE_DNLP_DIS;
		break;
	case AMVECM_IOC_VE_DNLP:
		if (copy_from_user(&am_ve_dnlp,
				(void __user *)arg,
				sizeof(struct ve_dnlp_s)))
			ret = -EFAULT;
		else
			ve_dnlp_param_update();
		break;
	case AMVECM_IOC_VE_NEW_DNLP:
		if (copy_from_user(&am_ve_new_dnlp,
				(void __user *)arg,
				sizeof(struct ve_dnlp_table_s)))
			ret = -EFAULT;
		else
			ve_new_dnlp_param_update();
		break;
	case AMVECM_IOC_G_HIST_AVG:
		argp = (void __user *)arg;
		if ((video_ve_hist.height == 0) || (video_ve_hist.width == 0))
			ret = -EFAULT;
		else if (copy_to_user(argp,
				&video_ve_hist,
				sizeof(struct ve_hist_s)))
			ret = -EFAULT;
		break;
	case AMVECM_IOC_G_HIST_BIN:
		argp = (void __user *)arg;
		if (vpp_hist_param.vpp_pixel_sum == 0)
			ret = -EFAULT;
		else if (copy_to_user(argp, &vpp_hist_param,
					sizeof(struct vpp_hist_param_s)))
			ret = -EFAULT;
		break;
	case AMVECM_IOC_G_HDR_METADATA:
		argp = (void __user *)arg;
		if (copy_to_user(argp, &vpp_hdr_metadata_s,
					sizeof(struct hdr_metadata_info_s)))
			ret = -EFAULT;
		break;
	/**********************************************************************/
	/*gamma ioctl*/
	/**********************************************************************/
	case AMVECM_IOC_GAMMA_TABLE_EN:
		if (!gamma_en)
			return -EINVAL;

		vecm_latch_flag |= FLAG_GAMMA_TABLE_EN;
		break;
	case AMVECM_IOC_GAMMA_TABLE_DIS:
		if (!gamma_en)
			return -EINVAL;

		vecm_latch_flag |= FLAG_GAMMA_TABLE_DIS;
		break;
	case AMVECM_IOC_GAMMA_TABLE_R:
		if (!gamma_en)
			return -EINVAL;

		if (copy_from_user(&video_gamma_table_r,
				(void __user *)arg,
				sizeof(struct tcon_gamma_table_s)))
			ret = -EFAULT;
		else
			vecm_latch_flag |= FLAG_GAMMA_TABLE_R;
		break;
	case AMVECM_IOC_GAMMA_TABLE_G:
		if (!gamma_en)
			return -EINVAL;

		if (copy_from_user(&video_gamma_table_g,
				(void __user *)arg,
				sizeof(struct tcon_gamma_table_s)))
			ret = -EFAULT;
		else
			vecm_latch_flag |= FLAG_GAMMA_TABLE_G;
		break;
	case AMVECM_IOC_GAMMA_TABLE_B:
		if (!gamma_en)
			return -EINVAL;

		if (copy_from_user(&video_gamma_table_b,
				(void __user *)arg,
				sizeof(struct tcon_gamma_table_s)))
			ret = -EFAULT;
		else
			vecm_latch_flag |= FLAG_GAMMA_TABLE_B;
		break;
	case AMVECM_IOC_S_RGB_OGO:
		if (!wb_en)
			return -EINVAL;

		if (copy_from_user(&video_rgb_ogo,
				(void __user *)arg,
				sizeof(struct tcon_rgb_ogo_s)))
			ret = -EFAULT;
		else
			ve_ogo_param_update();
		break;
	case AMVECM_IOC_G_RGB_OGO:
		if (!wb_en)
			return -EINVAL;

		if (copy_to_user((void __user *)arg,
				&video_rgb_ogo, sizeof(struct tcon_rgb_ogo_s)))
			ret = -EFAULT;

		break;
	/*VLOCK*/
	case AMVECM_IOC_VLOCK_EN:
		vecm_latch_flag |= FLAG_VLOCK_EN;
		break;
	case AMVECM_IOC_VLOCK_DIS:
		vecm_latch_flag |= FLAG_VLOCK_DIS;
		break;
	/*3D-SYNC*/
	case AMVECM_IOC_3D_SYNC_EN:
		vecm_latch_flag |= FLAG_3D_SYNC_EN;
		break;
	case AMVECM_IOC_3D_SYNC_DIS:
		vecm_latch_flag |= FLAG_3D_SYNC_DIS;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}
#ifdef CONFIG_COMPAT
static long amvecm_compat_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	unsigned long ret;

	arg = (unsigned long)compat_ptr(arg);
	ret = amvecm_ioctl(file, cmd, arg);
	return ret;
}
#endif
static ssize_t amvecm_dnlp_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x\n",
			(am_ve_dnlp.en << 28) | (am_ve_dnlp.rt << 24) |
			(am_ve_dnlp.rl << 16) | (am_ve_dnlp.black << 8) |
			(am_ve_dnlp.white << 0));
}
/* [   28] en    0~1 */
/* [27:20] rt    0~16 */
/* [19:16] rl-1  0~15 */
/* [15: 8] black 0~16 */
/* [ 7: 0] white 0~16 */
static ssize_t amvecm_dnlp_store(struct class *cla,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	size_t r;
	s32 val;

	r = sscanf(buf, "0x%x", &val);
	if ((r != 1) || (vecm_latch_flag & FLAG_VE_DNLP))
		return -EINVAL;
	am_ve_dnlp.en    = (val & 0xf0000000) >> 28;
	am_ve_dnlp.rt    =  (val & 0x0f000000) >> 24;
	am_ve_dnlp.rl    = (val & 0x00ff0000) >> 16;
	am_ve_dnlp.black =  (val & 0x0000ff00) >>  8;
	am_ve_dnlp.white = (val & 0x000000ff) >>  0;
	if (am_ve_dnlp.en >  1)
		am_ve_dnlp.en    =  1;
	if (am_ve_dnlp.rl > 64)
		am_ve_dnlp.rl    = 64;
	if (am_ve_dnlp.black > 16)
		am_ve_dnlp.black = 16;
	if (am_ve_dnlp.white > 16)
		am_ve_dnlp.white = 16;
	vecm_latch_flag |= FLAG_VE_DNLP;
	return count;
}

static ssize_t amvecm_brightness_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", vd1_brightness);
}

static ssize_t amvecm_brightness_store(struct class *cla,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	size_t r;
	int val;

	r = sscanf(buf, "%d\n", &val);
	if ((r != 1) || (val < -1024) || (val > 1024))
		return -EINVAL;

	vd1_brightness = val;
	/*vecm_latch_flag |= FLAG_BRI_CON;*/
	vecm_latch_flag |= FLAG_VADJ1_BRI;
	return count;
}

static ssize_t amvecm_contrast_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", vd1_contrast);
}

static ssize_t amvecm_contrast_store(struct class *cla,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	size_t r;
	int val;

	r = sscanf(buf, "%d\n", &val);
	if ((r != 1) || (val < -1024) || (val > 1024))
		return -EINVAL;

	vd1_contrast = val;
	/*vecm_latch_flag |= FLAG_BRI_CON;*/
	vecm_latch_flag |= FLAG_VADJ1_CON;

	if (val > 0)
		satu_shift_by_con = val >> 3;
	else
		satu_shift_by_con = 0;
	vecm_latch_flag |= FLAG_VADJ1_COLOR;
	return count;
}

static ssize_t amvecm_saturation_hue_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x\n", READ_VPP_REG(VPP_VADJ1_MA_MB));
}

static ssize_t amvecm_saturation_hue_store(struct class *cla,
		struct class_attribute *attr, const char *buf, size_t count)
{
	size_t r;
	s32 mab = 0;
	s16 mc = 0, md = 0;
	s16 ma, mb;

	r = sscanf(buf, "0x%x", &mab);
	if ((r != 1) || (mab&0xfc00fc00))
		return -EINVAL;
	ma = (s16)((mab << 6) >> 22);
	mb = (s16)((mab << 22) >> 22);

	saturation_ma = ma - 0x100;
	saturation_mb = mb;

	ma += saturation_ma_shift;
	mb += saturation_mb_shift;
	mab =  ((ma & 0x3ff) << 16) | (mb & 0x3ff);
	WRITE_VPP_REG(VPP_VADJ1_MA_MB, mab);
	mc = (s16)((mab<<22)>>22); /* mc = -mb */
	mc = 0 - mc;
	if (mc > 511)
		mc = 511;
	if (mc <  -512)
		mc = -512;
	md = (s16)((mab<<6)>>22);  /* md =  ma; */
	mab = ((mc&0x3ff)<<16)|(md&0x3ff);
	WRITE_VPP_REG(VPP_VADJ1_MC_MD, mab);
	WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, 1, 0, 1);
	pr_amvecm_dbg("%s set video_saturation_hue OK!!!\n", __func__);
	return count;
}

static int parse_para_pq(const char *para, int para_num, int *result)
{
	char *token = NULL;
	char *params, *params_base;
	int *out = result;
	int len = 0, count = 0;
	int res = 0;

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
		if (len == 0)
			break;
		if (!token || kstrtoint(token, 0, &res) < 0)
			break;
		len = strlen(token);
		*out++ = res;
		count++;
	} while ((token) && (count < para_num) && (len > 0));

	kfree(params_base);
	return count;
}

void vpp_vd_adj1_saturation_hue(signed int sat_val,
	signed int hue_val, struct vframe_s *vf)
{
	int i, ma, mb, mab, mc, md;
	int hue_cos[] = {
			/*0~12*/
		256, 256, 256, 255, 255, 254, 253, 252, 251, 250, 248, 247, 245,
		/*13~25*/
		243, 241, 239, 237, 234, 231, 229, 226, 223, 220, 216, 213, 209
	};
	int hue_sin[] = {
		/*-25~-13*/
		-147, -142, -137, -132, -126, -121, -115, -109, -104,
		 -98,  -92,  -86,  -80,  -74,  -68,  -62,  -56,  -50,
		 -44,  -38,  -31,  -25, -19, -13,  -6,      /*-12~-1*/
		0,  /*0*/
		 /*1~12*/
		6,   13,   19,	25,   31,   38,   44,	50,   56,  62,
		68,  74,   80,   86,   92,	98,  104,  109,  115,  121,
		126,  132, 137, 142, 147 /*13~25*/
	};

	i = (hue_val > 0) ? hue_val : -hue_val;
	ma = (hue_cos[i]*(sat_val + 128)) >> 7;
	mb = (hue_sin[25+hue_val]*(sat_val + 128)) >> 7;
	saturation_ma_shift = ma - 0x100;
	saturation_mb_shift = mb;

	ma += saturation_ma;
	mb += saturation_mb;
	if (ma > 511)
		ma = 511;
	if (ma < -512)
		ma = -512;
	if (mb > 511)
		mb = 511;
	if (mb < -512)
		mb = -512;
	mab =  ((ma & 0x3ff) << 16) | (mb & 0x3ff);
	pr_info("\n[amvideo..] saturation_pre:%d hue_pre:%d mab:%x\n",
			sat_val, hue_val, mab);
	WRITE_VPP_REG(VPP_VADJ1_MA_MB, mab);
	mc = (s16)((mab<<22)>>22); /* mc = -mb */
	mc = 0 - mc;
	if (mc > 511)
		mc = 511;
	if (mc < -512)
		mc = -512;
	md = (s16)((mab<<6)>>22);  /* md =	ma; */
	mab = ((mc&0x3ff)<<16)|(md&0x3ff);
	WRITE_VPP_REG(VPP_VADJ1_MC_MD, mab);
	WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, 1, 0, 1);
};

static ssize_t amvecm_saturation_hue_pre_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	return snprintf(buf, 20, "%d %d\n", saturation_pre, hue_pre);
}

static ssize_t amvecm_saturation_hue_pre_store(struct class *cla,
		struct class_attribute *attr, const char *buf, size_t count)
{
	int parsed[2];

	if (likely(parse_para_pq(buf, 2, parsed) != 2))
		return -EINVAL;

	if ((parsed[0] < -128) || (parsed[0] > 128) ||
		(parsed[1] < -25) || (parsed[1] > 25)) {
		return -EINVAL;
	}
	saturation_pre = parsed[0];
	hue_pre = parsed[1];
	vecm_latch_flag |= FLAG_VADJ1_COLOR;

	return count;
}

static ssize_t amvecm_saturation_hue_post_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	return snprintf(buf, 20, "%d %d\n", saturation_post, hue_post);
}

static ssize_t amvecm_saturation_hue_post_store(struct class *cla,
		struct class_attribute *attr, const char *buf, size_t count)
{
	int parsed[2];
	int i, ma, mb, mab, mc, md;
	int hue_cos[] = {
		/*0~12*/
		256, 256, 256, 255, 255, 254, 253, 252, 251, 250,
		248, 247, 245, 243, 241, 239, 237, 234, 231, 229,
		226, 223, 220, 216, 213, 209  /*13~25*/
	};
	int hue_sin[] = {
		-147, -142, -137, -132, -126, -121, -115, -109, -104,
		-98, -92, -86, -80, /*-25~-13*/-74,  -68,  -62,  -56,
		-50,  -44,  -38,  -31,  -25, -19, -13,  -6, /*-12~-1*/
		0, /*0*/
		6,   13,   19,	25,   31,   38,   44,	50,   56,
		62,	68,  74,      /*1~12*/	80,   86,   92,	98,  104,
		109,  115,  121,  126,  132, 137, 142, 147 /*13~25*/
	};
	if (likely(parse_para_pq(buf, 2, parsed) != 2))
		return -EINVAL;

	if ((parsed[0] < -128) ||
		(parsed[0] > 128) ||
		(parsed[1] < -25) ||
		(parsed[1] > 25)) {
		return -EINVAL;
	}
	saturation_post = parsed[0];
	hue_post = parsed[1];
	i = (hue_post > 0) ? hue_post : -hue_post;
	ma = (hue_cos[i]*(saturation_post + 128)) >> 7;
	mb = (hue_sin[25+hue_post]*(saturation_post + 128)) >> 7;
	if (ma > 511)
		ma = 511;
	if (ma < -512)
		ma = -512;
	if (mb > 511)
		mb = 511;
	if (mb < -512)
		mb = -512;
	mab =  ((ma & 0x3ff) << 16) | (mb & 0x3ff);
	pr_info("\n[amvideo..] saturation_post:%d hue_post:%d mab:%x\n",
			saturation_post, hue_post, mab);
	WRITE_VPP_REG(VPP_VADJ2_MA_MB, mab);
	mc = (s16)((mab<<22)>>22); /* mc = -mb */
	mc = 0 - mc;
	if (mc > 511)
		mc = 511;
	if (mc < -512)
		mc = -512;
	md = (s16)((mab<<6)>>22);  /* md =	ma; */
	mab = ((mc&0x3ff)<<16)|(md&0x3ff);
	WRITE_VPP_REG(VPP_VADJ2_MC_MD, mab);
	WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, 1, 2, 1);
	return count;
}

static ssize_t amvecm_cm2_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	pr_info("Usage:");
	pr_info(" echo wm addr data0 data1 data2 data3 data4 ");
	pr_info("> /sys/class/amvecm/cm2\n");
	pr_info(" echo rm addr > /sys/class/amvecm/cm2\n");
	return 0;
}

static ssize_t amvecm_cm2_store(struct class *cls,
		 struct class_attribute *attr,
		 const char *buffer, size_t count)
{
	int n = 0;
	char *buf_orig, *ps, *token;
	char *parm[7];
	u32 addr;
	int data[5] = {0};
	unsigned int addr_port = VPP_CHROMA_ADDR_PORT;/* 0x1d70; */
	unsigned int data_port = VPP_CHROMA_DATA_PORT;/* 0x1d71; */
	long val;
	char delim1[3] = " ";
	char delim2[2] = "\n";

	buf_orig = kstrdup(buffer, GFP_KERNEL);
	ps = buf_orig;
	strcat(delim1, delim2);
	while (1) {
		token = strsep(&ps, delim1);
		if (token == NULL)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}

	if ((parm[0][0] == 'w') && parm[0][1] == 'm') {
		if (n != 7) {
			pr_info("read: invalid parameter\n");
			pr_info("please: cat /sys/class/amvecm/cm2\n");
			kfree(buf_orig);
			return count;
		}
		if (kstrtol(parm[1], 16, &val) < 0)
			return -EINVAL;
		addr = val;
		addr = addr - addr%8;
		if (kstrtol(parm[2], 16, &val) < 0)
			return -EINVAL;
		data[0] = val;
		if (kstrtol(parm[3], 16, &val) < 0)
			return -EINVAL;
		data[1] = val;
		if (kstrtol(parm[4], 16, &val) < 0)
			return -EINVAL;
		data[2] = val;
		if (kstrtol(parm[5], 16, &val) < 0)
			return -EINVAL;
		data[3] = val;
		if (kstrtol(parm[6], 16, &val) < 0)
			return -EINVAL;
		data[4] = val;
		WRITE_VPP_REG(addr_port, addr);
		WRITE_VPP_REG(data_port, data[0]);
		WRITE_VPP_REG(addr_port, addr + 1);
		WRITE_VPP_REG(data_port, data[1]);
		WRITE_VPP_REG(addr_port, addr + 2);
		WRITE_VPP_REG(data_port, data[2]);
		WRITE_VPP_REG(addr_port, addr + 3);
		WRITE_VPP_REG(data_port, data[3]);
		WRITE_VPP_REG(addr_port, addr + 4);
		WRITE_VPP_REG(data_port, data[4]);
		pr_info("wm: [0x%x] <-- 0x0\n", addr);
	} else if ((parm[0][0] == 'r') && parm[0][1] == 'm') {
		if (n != 2) {
			pr_info("read: invalid parameter\n");
			pr_info("please: cat /sys/class/amvecm/cm2\n");
			kfree(buf_orig);
			return count;
		}
		if (kstrtol(parm[1], 16, &val) < 0)
			return -EINVAL;
		addr = val;
		addr = addr - addr%8;
		WRITE_VPP_REG(addr_port, addr);
		data[0] = READ_VPP_REG(data_port);
		data[0] = READ_VPP_REG(data_port);
		data[0] = READ_VPP_REG(data_port);
		WRITE_VPP_REG(addr_port, addr+1);
		data[1] = READ_VPP_REG(data_port);
		data[1] = READ_VPP_REG(data_port);
		data[1] = READ_VPP_REG(data_port);
		WRITE_VPP_REG(addr_port, addr+2);
		data[2] = READ_VPP_REG(data_port);
		data[2] = READ_VPP_REG(data_port);
		data[2] = READ_VPP_REG(data_port);
		WRITE_VPP_REG(addr_port, addr+3);
		data[3] = READ_VPP_REG(data_port);
		data[3] = READ_VPP_REG(data_port);
		data[3] = READ_VPP_REG(data_port);
		WRITE_VPP_REG(addr_port, addr+4);
		data[4] = READ_VPP_REG(data_port);
		data[4] = READ_VPP_REG(data_port);
		data[4] = READ_VPP_REG(data_port);
		pr_info("rm:[0x%x]-->[0x%x][0x%x][0x%x][0x%x][0x%x]\n",
				addr, data[0], data[1],
				data[2], data[3], data[4]);
	} else {
		pr_info("invalid command\n");
		pr_info("please: cat /sys/class/amvecm/bit");
	}
	kfree(buf_orig);
	return count;
}

static ssize_t amvecm_gamma_show(struct class *cls,
			struct class_attribute *attr,
			char *buf)
{
	pr_info("Usage:");
	pr_info("	echo sgr|sgg|sgb xxx...xx > /sys/class/amvecm/gamma\n");
	pr_info("Notes:");
	pr_info("	if the string xxx......xx is less than 256*3,");
	pr_info("	then the remaining will be set value 0\n");
	pr_info("	if the string xxx......xx is more than 256*3, ");
	pr_info("	then the remaining will be ignored\n");
	return 0;
}

static ssize_t amvecm_gamma_store(struct class *cls,
			struct class_attribute *attr,
			const char *buffer, size_t count)
{

	int n = 0;
	char *buf_orig, *ps, *token;
	char *parm[4];
	unsigned short *gammaR, *gammaG, *gammaB;
	unsigned int gamma_count;
	char gamma[4];
	int i = 0;
	long val;
	char delim1[3] = " ";
	char delim2[2] = "\n";

	/* to avoid the bellow warning message while compiling:
	 * warning: the frame size of 1576 bytes is larger than 1024 bytes
	 */
	gammaR = kmalloc(256 * sizeof(unsigned short), GFP_KERNEL);
	gammaG = kmalloc(256 * sizeof(unsigned short), GFP_KERNEL);
	gammaB = kmalloc(256 * sizeof(unsigned short), GFP_KERNEL);

	buf_orig = kstrdup(buffer, GFP_KERNEL);
	ps = buf_orig;
	strcat(delim1, delim2);
	while (1) {
		token = strsep(&ps, delim1);
		if (token == NULL)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}

	if ((parm[0][0] == 's') && (parm[0][1] == 'g')) {
		memset(gammaR, 0, 256 * sizeof(unsigned short));
		gamma_count = (strlen(parm[1]) + 2) / 3;
		if (gamma_count > 256)
			gamma_count = 256;

		for (i = 0; i < gamma_count; ++i) {
			gamma[0] = parm[1][3 * i + 0];
			gamma[1] = parm[1][3 * i + 1];
			gamma[2] = parm[1][3 * i + 2];
			gamma[3] = '\0';
			if (kstrtol(gamma, 16, &val) < 0)
				return -EINVAL;
			gammaR[i] = val;

		}

		switch (parm[0][2]) {
		case 'r':
			vpp_set_lcd_gamma_table(gammaR, H_SEL_R);
			break;

		case 'g':
			vpp_set_lcd_gamma_table(gammaR, H_SEL_G);
			break;

		case 'b':
			vpp_set_lcd_gamma_table(gammaR, H_SEL_B);
			break;
		default:
			break;
		}
	} else {
		pr_info("invalid command\n");
		pr_info("please: cat /sys/class/amvecm/gamma");

	}
	kfree(buf_orig);
	kfree(gammaR);
	kfree(gammaG);
	kfree(gammaB);
	return count;
}

static ssize_t set_gamma_pattern_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	pr_info("	echo r g b > /sys/class/amvecm/gamma_pattern\n");
	pr_info("	r g b should be hex\n");
	return 0;
}

static ssize_t set_gamma_pattern_store(struct class *cls,
			struct class_attribute *attr,
			const char *buffer, size_t count)
{
	unsigned short r_val[256], g_val[256], b_val[256];
	int n = 0;
	char *buf_orig, *ps, *token;
	char *parm[3];
	unsigned int gamma[3];
	long val, i;
	char deliml[3] = " ";
	char delim2[2] = "\n";

	buf_orig = kstrdup(buffer, GFP_KERNEL);
	ps = buf_orig;
	strcat(deliml, delim2);
	while (1) {
		token = strsep(&ps, deliml);
		if (token == NULL)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
	if (kstrtol(parm[0], 16, &val) < 0)
		return -EINVAL;
	gamma[0] = val << 2;

	if (kstrtol(parm[1], 16, &val) < 0)
		return -EINVAL;
	gamma[1] = val << 2;

	if (kstrtol(parm[2], 16, &val) < 0)
		return -EINVAL;
	gamma[2] = val << 2;

	for (i = 0; i < 256; i++) {
		r_val[i] = gamma[0];
		g_val[i] = gamma[1];
		b_val[i] = gamma[2];
	}

	vpp_set_lcd_gamma_table(r_val, H_SEL_R);

	vpp_set_lcd_gamma_table(g_val, H_SEL_G);

	vpp_set_lcd_gamma_table(b_val, H_SEL_B);
	return count;

}

void white_balance_adjust(int sel, int value)
{
	switch (sel) {
	/*0: en*/
	/*1: pre r   2: pre g   3: pre b*/
	/*4: gain r  5: gain g  6: gain b*/
	/*7: post r  8: post g  9: post b*/
	case 0:
		video_rgb_ogo.en = value;
		break;
	case 1:
		video_rgb_ogo.r_pre_offset = value;
		break;
	case 2:
		video_rgb_ogo.g_pre_offset = value;
		break;
	case 3:
		video_rgb_ogo.b_pre_offset = value;
		break;
	case 4:
		video_rgb_ogo.r_gain = value;
		break;
	case 5:
		video_rgb_ogo.g_gain = value;
		break;
	case 6:
		video_rgb_ogo.b_gain = value;
		break;
	case 7:
		video_rgb_ogo.r_post_offset = value;
		break;
	case 8:
		video_rgb_ogo.g_post_offset = value;
		break;
	case 9:
		video_rgb_ogo.b_post_offset = value;
		break;
	default:
		break;
	}
	ve_ogo_param_update();
}

static ssize_t amvecm_wb_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	pr_info("read:	echo r gain_r > /sys/class/amvecm/wb\n");
	pr_info("read:	echo r pre_r > /sys/class/amvecm/wb\n");
	pr_info("read:	echo r post_r > /sys/class/amvecm/wb\n");
	pr_info("write:	echo gain_r value > /sys/class/amvecm/wb\n");
	pr_info("write:	echo preofst_r value > /sys/class/amvecm/wb\n");
	pr_info("write:	echo postofst_r value > /sys/class/amvecm/wb\n");
	return 0;
}

static ssize_t amvecm_wb_store(struct class *cls,
			struct class_attribute *attr,
			const char *buffer, size_t count)
{
	char *buf_orig, *parm[8] = {NULL};
	long value;

	if (!buffer)
		return count;
	buf_orig = kstrdup(buffer, GFP_KERNEL);
	parse_param_amvecm(buf_orig, (char **)&parm);

	if (!strncmp(parm[0], "r", 1)) {
		if (!strncmp(parm[1], "pre_r", 5))
			pr_info("\t Pre_R = %d\n", video_rgb_ogo.r_pre_offset);
		else if (!strncmp(parm[1], "pre_g", 5))
			pr_info("\t Pre_G = %d\n", video_rgb_ogo.g_pre_offset);
		else if (!strncmp(parm[1], "pre_b", 5))
			pr_info("\t Pre_B = %d\n", video_rgb_ogo.b_pre_offset);
		else if (!strncmp(parm[1], "gain_r", 6))
			pr_info("\t Gain_R = %d\n", video_rgb_ogo.r_gain);
		else if (!strncmp(parm[1], "gain_g", 6))
			pr_info("\t Gain_G = %d\n", video_rgb_ogo.g_gain);
		else if (!strncmp(parm[1], "gain_b", 6))
			pr_info("\t Gain_B = %d\n", video_rgb_ogo.b_gain);
		else if (!strncmp(parm[1], "post_r", 6))
			pr_info("\t Post_R = %d\n",
				video_rgb_ogo.r_post_offset);
		else if (!strncmp(parm[1], "post_g", 6))
			pr_info("\t Post_G = %d\n",
				video_rgb_ogo.g_post_offset);
		else if (!strncmp(parm[1], "post_b", 6))
			pr_info("\t Post_B = %d\n",
				video_rgb_ogo.b_post_offset);
		else if (!strncmp(parm[1], "en", 2))
			pr_info("\t En = %d\n", video_rgb_ogo.en);
	} else {
		if (kstrtol(parm[1], 10, &value) < 0)
			return -EINVAL;
		if (!strncmp(parm[0], "wb_en", 5)) {
			white_balance_adjust(0, value);
			pr_info("\t set wb en\n");
		} else if (!strncmp(parm[0], "preofst_r", 9)) {
			if ((value > 1023) || (value < -1024))
				pr_info("\t preofst r over range\n");
			else {
				white_balance_adjust(1, value);
				pr_info("\t set wb preofst r\n");
			}
		} else if (!strncmp(parm[0], "preofst_g", 9)) {
			if ((value > 1023) || (value < -1024))
				pr_info("\t preofst g over range\n");
			else {
				white_balance_adjust(2, value);
				pr_info("\t set wb preofst g\n");
			}
		} else if (!strncmp(parm[0], "preofst_b", 9)) {
			if ((value > 1023) || (value < -1024))
				pr_info("\t preofst b over range\n");
			else {
				white_balance_adjust(3, value);
				pr_info("\t set wb preofst b\n");
			}
		} else if (!strncmp(parm[0], "gain_r", 6)) {
			if ((value > 2047) || (value < 0))
				pr_info("\t gain r over range\n");
			else {
				white_balance_adjust(4, value);
				pr_info("\t set wb gain r\n");
			}
		} else if (!strncmp(parm[0], "gain_g", 6)) {
			if ((value > 2047) || (value < 0))
				pr_info("\t gain g over range\n");
			else {
				white_balance_adjust(5, value);
				pr_info("\t set wb gain g\n");
			}
		} else if (!strncmp(parm[0], "gain_b", 6)) {
			if ((value > 2047) || (value < 0))
				pr_info("\t gain b over range\n");
			else {
				white_balance_adjust(6, value);
				pr_info("\t set wb gain b\n");
			}
		} else if (!strncmp(parm[0], "postofst_r", 10)) {
			if ((value > 1023) || (value < -1024))
				pr_info("\t postofst r over range\n");
			else {
				white_balance_adjust(7, value);
				pr_info("\t set wb postofst r\n");
			}
		} else if (!strncmp(parm[0], "postofst_g", 10)) {
			if ((value > 1023) || (value < -1024))
				pr_info("\t postofst g over range\n");
			else {
				white_balance_adjust(8, value);
				pr_info("\t set wb postofst g\n");
			}
		} else if (!strncmp(parm[0], "postofst_b", 10)) {
			if ((value > 1023) || (value < -1024))
				pr_info("\t postofst b over range\n");
			else {
				white_balance_adjust(9, value);
				pr_info("\t set wb postofst b\n");
			}
		}
	}

	kfree(buf_orig);
	return count;
}

static ssize_t set_hdr_289lut_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	int i;

	for (i = 0; i < 289; i++) {
		pr_info("0x%-8x\t", lut_289_mapping[i]);
		if ((i + 1) % 8 == 0)
			pr_info("\n");
	}
	return 0;
}
static ssize_t set_hdr_289lut_store(struct class *cls,
			struct class_attribute *attr,
			const char *buffer, size_t count)
{
	int n = 0;
	char *buf_orig, *ps, *token;
	char *parm[4];
	unsigned short *Hdr289lut;
	unsigned int gamma_count;
	char gamma[4];
	int i = 0;
	long val;
	char deliml[3] = " ";
	char delim2[2] = "\n";

	Hdr289lut = kmalloc(289 * sizeof(unsigned short), GFP_KERNEL);

	buf_orig = kstrdup(buffer, GFP_KERNEL);
	ps = buf_orig;
	strcat(deliml, delim2);
	while (1) {
		token = strsep(&ps, deliml);
		if (token == NULL)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}

	memset(Hdr289lut, 0, 289 * sizeof(unsigned short));
	gamma_count = (strlen(parm[0]) + 2) / 3;
	if (gamma_count > 289)
		gamma_count = 289;

	for (i = 0; i < gamma_count; ++i) {
		gamma[0] = parm[0][3 * i + 0];
		gamma[1] = parm[0][3 * i + 1];
		gamma[2] = parm[0][3 * i + 2];
		gamma[3] = '\0';
		if (kstrtol(gamma, 16, &val) < 0)
			return -EINVAL;
		Hdr289lut[i] = val;
	}

	for (i = 0; i < gamma_count; i++)
		lut_289_mapping[i] = Hdr289lut[i];

	kfree(buf_orig);
	kfree(Hdr289lut);
	return count;

}

static ssize_t amvecm_set_post_matrix_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x\n", (int)(READ_VPP_REG(VPP_MATRIX_CTRL)));
}
static ssize_t amvecm_set_post_matrix_store(struct class *cla,
			struct class_attribute *attr,
			const char *buf, size_t count)
{
	size_t r;
	int val;

	r = sscanf(buf, "0x%x", &val);
	if ((r != 1)  || (val & 0xffff0000))
		return -EINVAL;

	WRITE_VPP_REG(VPP_MATRIX_CTRL, val);
	return count;
}

static ssize_t amvecm_post_matrix_pos_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x\n",
			(int)(READ_VPP_REG(VPP_MATRIX_PROBE_POS)));
}
static ssize_t amvecm_post_matrix_pos_store(struct class *cla,
			struct class_attribute *attr,
			const char *buf, size_t count)
{
	size_t r;
	int val;

	r = sscanf(buf, "0x%x", &val);
	if ((r != 1)  || (val & 0xf000f000))
		return -EINVAL;

	WRITE_VPP_REG(VPP_MATRIX_PROBE_POS, val);
	return count;
}

static ssize_t amvecm_post_matrix_data_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	int len = 0, val1 = 0, val2 = 0;

	val1 = READ_VPP_REG(VPP_MATRIX_PROBE_COLOR);
/* #if (MESON_CPU_TYPE >= MESON_CPU_TYPE_MESONG9TV) */
	val2 = READ_VPP_REG(VPP_MATRIX_PROBE_COLOR1);
/* #endif */
	len += sprintf(buf+len, "VPP_MATRIX_PROBE_COLOR %x\n", val1);
	len += sprintf(buf+len, "VPP_MATRIX_PROBE_COLOR %x\n", val2);
	return len;
}

static ssize_t amvecm_post_matrix_data_store(struct class *cla,
			struct class_attribute *attr,
			const char *buf, size_t count)
{
	return 0;
}

static ssize_t amvecm_sr1_reg_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	unsigned int addr;

	addr = ((sr1_index+0x3280) << 2) | 0xd0100000;
	return sprintf(buf, "0x%x = 0x%x\n",
			addr, sr1_ret_val[sr1_index]);
}

static ssize_t amvecm_sr1_reg_store(struct class *cla,
			struct class_attribute *attr,
			const char *buf, size_t count)
{
	size_t r;
	unsigned int addr, off_addr = 0;

	r = sscanf(buf, "0x%x", &addr);
	addr = (addr&0xffff) >> 2;
	if ((r != 1)  || (addr > 0x32e4) || (addr < 0x3280))
		return -EINVAL;
	off_addr = addr - 0x3280;
	sr1_index = off_addr;
	sr1_ret_val[off_addr] = sr1_reg_val[off_addr];

	return count;

}

static ssize_t amvecm_write_sr1_reg_val_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t amvecm_write_sr1_reg_val_store(struct class *cla,
			struct class_attribute *attr,
			const char *buf, size_t count)
{
	size_t r;
	unsigned int val;

	r = sscanf(buf, "0x%x", &val);
	if (r != 1)
		return -EINVAL;
	sr1_reg_val[sr1_index] = val;

	return count;

}

static ssize_t amvecm_dump_reg_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	unsigned int addr;
	unsigned int value;

	pr_info("----dump sharpness0 reg----\n");
	for (addr = 0x3200;
		addr <= 0x3264; addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
				(0xd0100000+(addr<<2)), addr,
				READ_VPP_REG(addr));
	if (is_meson_txl_cpu()) {
		for (addr = 0x3265;
			addr <= 0x3272; addr++)
			pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
					(0xd0100000+(addr<<2)), addr,
					READ_VPP_REG(addr));
	}
	pr_info("----dump sharpness1 reg----\n");
	for (addr = (0x3200+0x80);
		addr <= (0x3264+0x80); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
				(0xd0100000+(addr<<2)), addr,
				READ_VPP_REG(addr));
	if (is_meson_txl_cpu()) {
		for (addr = (0x3265+0x80);
			addr <= (0x3272+0x80); addr++)
			pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
					(0xd0100000+(addr<<2)), addr,
					READ_VPP_REG(addr));
	}

	pr_info("----dump cm reg----\n");
	for (addr = 0x200; addr <= 0x21e; addr++) {
		WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, addr);
		value = READ_VPP_REG(VPP_CHROMA_DATA_PORT);
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
				addr, addr,
				value);
	}
	for (addr = 0x100; addr <= 0x1fc; addr++) {
		WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, addr);
		value = READ_VPP_REG(VPP_CHROMA_DATA_PORT);
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
				addr, addr,
				value);
	}

	pr_info("----dump vd1 IF0 reg----\n");
	for (addr = (0x1a50);
		addr <= (0x1a69); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
				(0xd0100000+(addr<<2)), addr,
				READ_VPP_REG(addr));
	pr_info("----dump vpp1 part1 reg----\n");
	for (addr = (0x1d00);
		addr <= (0x1d6e); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
				(0xd0100000+(addr<<2)), addr,
				READ_VPP_REG(addr));

	pr_info("----dump vpp1 part2 reg----\n");
	for (addr = (0x1d72);
		addr <= (0x1de4); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
				(0xd0100000+(addr<<2)), addr,
				READ_VPP_REG(addr));

	pr_info("----dump ndr reg----\n");
	for (addr = (0x2d00);
		addr <= (0x2d78); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
				(0xd0100000+(addr<<2)), addr,
				READ_VPP_REG(addr));
	pr_info("----dump nr3 reg----\n");
	for (addr = (0x2ff0);
		addr <= (0x2ff6); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
				(0xd0100000+(addr<<2)), addr,
				READ_VPP_REG(addr));
	pr_info("----dump vlock reg----\n");
	for (addr = (0x3000);
		addr <= (0x3020); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
				(0xd0100000+(addr<<2)), addr,
				READ_VPP_REG(addr));
	pr_info("----dump super scaler0 reg----\n");
	for (addr = (0x3100);
		addr <= (0x3115); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
				(0xd0100000+(addr<<2)), addr,
				READ_VPP_REG(addr));
	pr_info("----dump super scaler1 reg----\n");
	for (addr = (0x3118);
		addr <= (0x312e); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
				(0xd0100000+(addr<<2)), addr,
				READ_VPP_REG(addr));
	pr_info("----dump xvycc reg----\n");
	for (addr = (0x3158);
		addr <= (0x3179); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
				(0xd0100000+(addr<<2)), addr,
				READ_VPP_REG(addr));
	pr_info("----dump reg done----\n");
	return 0;
}
static ssize_t amvecm_dump_reg_store(struct class *cla,
			struct class_attribute *attr,
			const char *buf, size_t count)
{
	return 0;
}
static ssize_t amvecm_dump_vpp_hist_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	vpp_dump_histgram();
	return 0;
}

static ssize_t amvecm_dump_vpp_hist_store(struct class *cla,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	return 0;
}

static ssize_t amvecm_hdr_dbg_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	int ret;

	ret = amvecm_hdr_dbg(0);

	return 0;
}

static ssize_t amvecm_hdr_dbg_store(struct class *cla,
			struct class_attribute *attr,
			const char *buf, size_t count)
{
	return 0;
}

static ssize_t amvecm_hdr_reg_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	int ret;

	ret = amvecm_hdr_dbg(1);

	return 0;
}

static ssize_t amvecm_hdr_reg_store(struct class *cla,
			struct class_attribute *attr,
			const char *buf, size_t count)
{
	return 0;
}

static ssize_t amvecm_pc_mode_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	pr_info("pc:echo 0x0 > /sys/class/amvecm/pc_mode\n");
	pr_info("other:echo 0x1 > /sys/class/amvecm/pc_mode\n");
	pr_info("pc_mode:%d,pc_mode_last:%d\n", pc_mode, pc_mode_last);
	return 0;
}

static ssize_t amvecm_pc_mode_store(struct class *cla,
			struct class_attribute *attr,
			const char *buf, size_t count)
{
	size_t r;
	int val;

	r = sscanf(buf, "%x\n", &val);
	if ((r != 1))
		return -EINVAL;

	if (val == 1) {
		pc_mode = 1;
		pc_mode_last = 0xff;
	} else if (val == 0) {
		pc_mode = 0;
		pc_mode_last = 0xff;
	}

	return count;
}

void pc_mode_process(void)
{
	unsigned int reg_val;

	if ((pc_mode == 1) && (pc_mode != pc_mode_last)) {
		/* open dnlp clock gate */
		dnlp_en = 1;
		ve_enable_dnlp();
		/* open cm clock gate */
		cm_en = 1;
		amcm_enable();
			/* sharpness on */
		WRITE_VPP_REG_BITS(
			SRSHARP0_SHARP_PK_NR_ENABLE,
			1, 1, 1);
		WRITE_VPP_REG_BITS(
			SRSHARP1_SHARP_PK_NR_ENABLE,
			1, 1, 1);
		reg_val = READ_VPP_REG(SRSHARP0_HCTI_FLT_CLP_DC);
		WRITE_VPP_REG(SRSHARP0_HCTI_FLT_CLP_DC,
				reg_val | 0x10000000);
		WRITE_VPP_REG(SRSHARP1_HCTI_FLT_CLP_DC,
				reg_val | 0x10000000);

		reg_val = READ_VPP_REG(SRSHARP0_HLTI_FLT_CLP_DC);
		WRITE_VPP_REG(SRSHARP0_HLTI_FLT_CLP_DC,
				reg_val | 0x10000000);
		WRITE_VPP_REG(SRSHARP1_HLTI_FLT_CLP_DC,
				reg_val | 0x10000000);

		reg_val = READ_VPP_REG(SRSHARP0_VLTI_FLT_CON_CLP);
		WRITE_VPP_REG(SRSHARP0_VLTI_FLT_CON_CLP,
				reg_val | 0x4000);
		WRITE_VPP_REG(SRSHARP1_VLTI_FLT_CON_CLP,
				reg_val | 0x4000);

		reg_val = READ_VPP_REG(SRSHARP0_VCTI_FLT_CON_CLP);
		WRITE_VPP_REG(SRSHARP0_VCTI_FLT_CON_CLP,
				reg_val | 0x4000);
		WRITE_VPP_REG(SRSHARP1_VCTI_FLT_CON_CLP,
				reg_val | 0x4000);

		if (is_meson_txl_cpu()) {
			WRITE_VPP_REG_BITS(SRSHARP0_DEJ_CTRL, 1, 0, 1);
			WRITE_VPP_REG_BITS(SRSHARP0_SR3_DRTLPF_EN, 7, 0, 3);
			WRITE_VPP_REG_BITS(SRSHARP0_SR3_DERING_CTRL, 1, 28, 3);

			WRITE_VPP_REG_BITS(SRSHARP1_DEJ_CTRL, 1, 0, 1);
			WRITE_VPP_REG_BITS(SRSHARP1_SR3_DRTLPF_EN, 7, 0, 3);
			WRITE_VPP_REG_BITS(SRSHARP1_SR3_DERING_CTRL, 1, 28, 3);
		}
		WRITE_VPP_REG(VPP_VADJ_CTRL, 0xd);
		pc_mode_last = pc_mode;
	} else if ((pc_mode == 0) && (pc_mode != pc_mode_last)) {
		dnlp_en = 0;
		ve_disable_dnlp();
		cm_en = 0;
		amcm_disable();

		WRITE_VPP_REG_BITS(
			SRSHARP0_SHARP_PK_NR_ENABLE,
			0, 1, 1);
		WRITE_VPP_REG_BITS(
			SRSHARP1_SHARP_PK_NR_ENABLE,
			0, 1, 1);
		reg_val = READ_VPP_REG(SRSHARP0_HCTI_FLT_CLP_DC);
		WRITE_VPP_REG(SRSHARP0_HCTI_FLT_CLP_DC,
				reg_val & 0xefffffff);
		WRITE_VPP_REG(SRSHARP1_HCTI_FLT_CLP_DC,
				reg_val & 0xefffffff);

		reg_val = READ_VPP_REG(SRSHARP0_HLTI_FLT_CLP_DC);
		WRITE_VPP_REG(SRSHARP0_HLTI_FLT_CLP_DC,
				reg_val & 0xefffffff);
		WRITE_VPP_REG(SRSHARP1_HLTI_FLT_CLP_DC,
				reg_val & 0xefffffff);

		reg_val = READ_VPP_REG(SRSHARP0_VLTI_FLT_CON_CLP);
		WRITE_VPP_REG(SRSHARP0_VLTI_FLT_CON_CLP,
				reg_val & 0xffffbfff);
		WRITE_VPP_REG(SRSHARP1_VLTI_FLT_CON_CLP,
				reg_val & 0xffffbfff);

		reg_val = READ_VPP_REG(SRSHARP0_VCTI_FLT_CON_CLP);
		WRITE_VPP_REG(SRSHARP0_VCTI_FLT_CON_CLP,
				reg_val & 0xffffbfff);
		WRITE_VPP_REG(SRSHARP1_VCTI_FLT_CON_CLP,
				reg_val & 0xffffbfff);

		if (is_meson_txl_cpu()) {
			WRITE_VPP_REG_BITS(SRSHARP0_DEJ_CTRL, 0, 0, 1);
			WRITE_VPP_REG_BITS(SRSHARP0_SR3_DRTLPF_EN, 0, 0, 3);
			WRITE_VPP_REG_BITS(SRSHARP0_SR3_DERING_CTRL, 0, 28, 3);

			WRITE_VPP_REG_BITS(SRSHARP1_DEJ_CTRL, 0, 0, 1);
			WRITE_VPP_REG_BITS(SRSHARP1_SR3_DRTLPF_EN, 0, 0, 3);
			WRITE_VPP_REG_BITS(SRSHARP1_SR3_DERING_CTRL, 0, 28, 3);
		}
		WRITE_VPP_REG(VPP_VADJ_CTRL, 0x0);
		pc_mode_last = pc_mode;
	}
}

static ssize_t amvecm_vpp_demo_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t amvecm_vpp_demo_store(struct class *cla,
			struct class_attribute *attr,
			const char *buf, size_t count)
{
	size_t r;
	int val;

	r = sscanf(buf, "%x\n", &val);
	if ((r != 1))
		return -EINVAL;

	if (val & VPP_DEMO_CM_EN)
		vpp_demo_latch_flag |= VPP_DEMO_CM_EN;
	else if (val & VPP_DEMO_CM_DIS)
		vpp_demo_latch_flag |= VPP_DEMO_CM_DIS;

	if (val & VPP_DEMO_DNLP_EN)
		vpp_demo_latch_flag |= VPP_DEMO_DNLP_EN;
	else if (val & VPP_DEMO_DNLP_DIS)
		vpp_demo_latch_flag |= VPP_DEMO_DNLP_DIS;

	return count;
}

static void dump_vpp_size_info(void)
{
	unsigned int vpp_input_h, vpp_input_v,
		pps_input_length, pps_input_height,
		pps_output_hs, pps_output_he, pps_output_vs, pps_output_ve,
		vd1_preblend_hs, vd1_preblend_he,
		vd1_preblend_vs, vd1_preblend_ve,
		vd2_preblend_hs, vd2_preblend_he,
		vd2_preblend_vs, vd2_preblend_ve,
		prelend_input_hsize,
		vd1_postblend_hs, vd1_postblend_he,
		vd1_postblend_vs, vd1_postblend_ve,
		postblend_hsize,
		ve_hsize, ve_vsize, psr_hsize, psr_vsize,
		cm_hsize, cm_vsize;
	vpp_input_h = READ_VPP_REG_BITS(VPP_IN_H_V_SIZE, 16, 13);
	vpp_input_v = READ_VPP_REG_BITS(VPP_IN_H_V_SIZE, 0, 13);
	pps_input_length = READ_VPP_REG_BITS(VPP_LINE_IN_LENGTH, 0, 13);
	pps_input_height = READ_VPP_REG_BITS(VPP_PIC_IN_HEIGHT, 0, 13);
	pps_output_hs = READ_VPP_REG_BITS(VPP_HSC_REGION12_STARTP, 16, 13);
	pps_output_he = READ_VPP_REG_BITS(VPP_HSC_REGION4_ENDP, 0, 13);
	pps_output_vs = READ_VPP_REG_BITS(VPP_VSC_REGION12_STARTP, 16, 13);
	pps_output_ve = READ_VPP_REG_BITS(VPP_VSC_REGION4_ENDP, 0, 13);
	vd1_preblend_he = READ_VPP_REG_BITS(VPP_PREBLEND_VD1_H_START_END,
		0, 13);
	vd1_preblend_hs = READ_VPP_REG_BITS(VPP_PREBLEND_VD1_H_START_END,
		16, 13);
	vd1_preblend_ve = READ_VPP_REG_BITS(VPP_PREBLEND_VD1_V_START_END,
		0, 13);
	vd1_preblend_vs = READ_VPP_REG_BITS(VPP_PREBLEND_VD1_V_START_END,
		16, 13);
	vd2_preblend_he = READ_VPP_REG_BITS(VPP_BLEND_VD2_H_START_END, 0, 13);
	vd2_preblend_hs = READ_VPP_REG_BITS(VPP_BLEND_VD2_H_START_END, 16, 13);
	vd2_preblend_ve = READ_VPP_REG_BITS(VPP_BLEND_VD2_V_START_END, 0, 13);
	vd2_preblend_vs = READ_VPP_REG_BITS(VPP_BLEND_VD2_V_START_END, 16, 13);
	prelend_input_hsize = READ_VPP_REG_BITS(VPP_PREBLEND_H_SIZE, 0, 13);
	vd1_postblend_he = READ_VPP_REG_BITS(VPP_POSTBLEND_VD1_H_START_END,
		0, 13);
	vd1_postblend_hs = READ_VPP_REG_BITS(VPP_POSTBLEND_VD1_H_START_END,
		16, 13);
	vd1_postblend_ve = READ_VPP_REG_BITS(VPP_POSTBLEND_VD1_V_START_END,
		0, 13);
	vd1_postblend_vs = READ_VPP_REG_BITS(VPP_POSTBLEND_VD1_V_START_END,
		16, 13);
	postblend_hsize = READ_VPP_REG_BITS(VPP_POSTBLEND_H_SIZE, 0, 13);
	ve_hsize = READ_VPP_REG_BITS(VPP_VE_H_V_SIZE, 16, 13);
	ve_vsize = READ_VPP_REG_BITS(VPP_VE_H_V_SIZE, 0, 13);
	psr_hsize = READ_VPP_REG_BITS(VPP_PSR_H_V_SIZE, 16, 13);
	psr_vsize = READ_VPP_REG_BITS(VPP_PSR_H_V_SIZE, 0, 13);
	WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, 0x205);
	cm_hsize = READ_VPP_REG(VPP_CHROMA_DATA_PORT);
	cm_vsize = (cm_hsize >> 16) & 0xffff;
	cm_hsize = cm_hsize & 0xffff;
	pr_info("\n vpp size info:\n");
	pr_info("vpp_input_h:%d, vpp_input_v:%d\n"
		"pps_input_length:%d, pps_input_height:%d\n"
		"pps_output_hs:%d, pps_output_he:%d\n"
		"pps_output_vs:%d, pps_output_ve:%d\n"
		"vd1_preblend_hs:%d, vd1_preblend_he:%d\n"
		"vd1_preblend_vs:%d, vd1_preblend_ve:%d\n"
		"vd2_preblend_hs:%d, vd2_preblend_he:%d\n"
		"vd2_preblend_vs:%d, vd2_preblend_ve:%d\n"
		"prelend_input_hsize:%d\n"
		"vd1_postblend_hs:%d, vd1_postblend_he:%d\n"
		"vd1_postblend_vs:%d, vd1_postblend_ve:%d\n"
		"postblend_hsize:%d\n"
		"ve_hsize:%d, ve_vsize:%d\n"
		"psr_hsize:%d, psr_vsize:%d\n"
		"cm_hsize:%d, cm_vsize:%d\n",
		vpp_input_h, vpp_input_v,
		pps_input_length, pps_input_height,
		pps_output_hs, pps_output_he,
		pps_output_vs, pps_output_ve,
		vd1_preblend_hs, vd1_preblend_he,
		vd1_preblend_vs, vd1_preblend_ve,
		vd2_preblend_hs, vd2_preblend_he,
		vd2_preblend_vs, vd2_preblend_ve,
		prelend_input_hsize,
		vd1_postblend_hs, vd1_postblend_he,
		vd1_postblend_vs, vd1_postblend_ve,
		postblend_hsize,
		ve_hsize, ve_vsize,
		psr_hsize, psr_vsize,
		cm_hsize, cm_vsize);
}

static void vpp_sr3_enhance_enable(unsigned int enable)
{
	/*0x00: core 0 disable*/
	/*0x01: core 0 enable*/
	/*0x10: core 1 disable*/
	/*0x11: core 1 enable*/

	if (enable == 0x00) {
		WRITE_VPP_REG_BITS(SRSHARP0_DEJ_CTRL, 0, 0, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_SR3_DRTLPF_EN, 0, 0, 3);
		WRITE_VPP_REG_BITS(SRSHARP0_SR3_DERING_CTRL, 0, 28, 3);
	} else if (enable == 0x01) {
		WRITE_VPP_REG_BITS(SRSHARP0_DEJ_CTRL, 1, 0, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_SR3_DRTLPF_EN, 7, 0, 3);
		WRITE_VPP_REG_BITS(SRSHARP0_SR3_DERING_CTRL, 1, 28, 3);
	} else if (enable == 0x10) {
		WRITE_VPP_REG_BITS(SRSHARP1_DEJ_CTRL, 0, 0, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_SR3_DRTLPF_EN, 0, 0, 3);
		WRITE_VPP_REG_BITS(SRSHARP1_SR3_DERING_CTRL, 0, 28, 3);
	} else if (enable == 0x11) {
		WRITE_VPP_REG_BITS(SRSHARP1_DEJ_CTRL, 1, 0, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_SR3_DRTLPF_EN, 7, 0, 3);
		WRITE_VPP_REG_BITS(SRSHARP1_SR3_DERING_CTRL, 1, 28, 3);
	}
}

static void amvecm_wb_enable(int enable)
{
	if (enable) {
		wb_en = 1;
		WRITE_VPP_REG_BITS(VPP_GAINOFF_CTRL0, 1, 31, 1);
	} else {
		wb_en = 0;
		WRITE_VPP_REG_BITS(VPP_GAINOFF_CTRL0, 0, 31, 1);
	}
}

static void amvecm_sharpness_debug(int enable)
{
	/*0:peaking enable   1:peaking disable*/
	/*2:lti/cti enable   3:lti/cti disable*/
	switch (enable) {
	case 0:
		WRITE_VPP_REG_BITS(SRSHARP0_SHARP_PK_NR_ENABLE, 1, 1, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_SHARP_PK_NR_ENABLE, 1, 1, 1);
		break;
	case 1:
		WRITE_VPP_REG_BITS(SRSHARP0_SHARP_PK_NR_ENABLE, 0, 1, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_SHARP_PK_NR_ENABLE, 0, 1, 1);
		break;
	case 2:
		WRITE_VPP_REG_BITS(SRSHARP0_HCTI_FLT_CLP_DC, 1, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_HLTI_FLT_CLP_DC, 1, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_VLTI_FLT_CON_CLP, 1, 14, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_VCTI_FLT_CON_CLP, 1, 14, 1);

		WRITE_VPP_REG_BITS(SRSHARP1_HCTI_FLT_CLP_DC, 1, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_HLTI_FLT_CLP_DC, 1, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_VLTI_FLT_CON_CLP, 1, 14, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_VCTI_FLT_CON_CLP, 1, 14, 1);
		break;
	case 3:
		WRITE_VPP_REG_BITS(SRSHARP0_HCTI_FLT_CLP_DC, 0, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_HLTI_FLT_CLP_DC, 0, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_VLTI_FLT_CON_CLP, 0, 14, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_VCTI_FLT_CON_CLP, 0, 14, 1);

		WRITE_VPP_REG_BITS(SRSHARP1_HCTI_FLT_CLP_DC, 0, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_HLTI_FLT_CLP_DC, 0, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_VLTI_FLT_CON_CLP, 0, 14, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_VCTI_FLT_CON_CLP, 0, 14, 1);
		break;
	default:
		break;
	}
}

static void amvecm_pq_enable(int enable)
{
	if (enable) {
		vecm_latch_flag |= FLAG_VE_DNLP_EN;

		amcm_enable();

		WRITE_VPP_REG_BITS(SRSHARP0_SHARP_PK_NR_ENABLE, 1, 1, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_SHARP_PK_NR_ENABLE, 1, 1, 1);

		WRITE_VPP_REG_BITS(SRSHARP0_HCTI_FLT_CLP_DC, 1, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_HLTI_FLT_CLP_DC, 1, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_VLTI_FLT_CON_CLP, 1, 14, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_VCTI_FLT_CON_CLP, 1, 14, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_HCTI_FLT_CLP_DC, 1, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_HLTI_FLT_CLP_DC, 1, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_VLTI_FLT_CON_CLP, 1, 14, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_VCTI_FLT_CON_CLP, 1, 14, 1);

		WRITE_VPP_REG_BITS(VPP_GAINOFF_CTRL0, 1, 31, 1);

		vecm_latch_flag |= FLAG_GAMMA_TABLE_EN;

		WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, 1, 0, 1);
	} else {
		vecm_latch_flag |= FLAG_VE_DNLP_DIS;

		amcm_disable();

		WRITE_VPP_REG_BITS(SRSHARP0_SHARP_PK_NR_ENABLE, 0, 1, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_SHARP_PK_NR_ENABLE, 0, 1, 1);

		WRITE_VPP_REG_BITS(SRSHARP0_HCTI_FLT_CLP_DC, 0, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_HLTI_FLT_CLP_DC, 0, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_VLTI_FLT_CON_CLP, 0, 14, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_VCTI_FLT_CON_CLP, 0, 14, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_HCTI_FLT_CLP_DC, 0, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_HLTI_FLT_CLP_DC, 0, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_VLTI_FLT_CON_CLP, 0, 14, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_VCTI_FLT_CON_CLP, 0, 14, 1);

		WRITE_VPP_REG_BITS(VPP_GAINOFF_CTRL0, 0, 31, 1);

		vecm_latch_flag |= FLAG_GAMMA_TABLE_DIS;

		WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, 0, 0, 1);
	}
}

static const char *amvecm_debug_usage_str = {
	"Usage:\n"
	"echo vpp_size > /sys/class/amvecm/debug ; get vpp size config\n"
};
static ssize_t amvecm_debug_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", amvecm_debug_usage_str);
}
static ssize_t amvecm_debug_store(struct class *cla,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	char *buf_orig, *parm[8] = {NULL};

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	parse_param_amvecm(buf_orig, (char **)&parm);
	if (!strncmp(parm[0], "vpp_size", 8))
		dump_vpp_size_info();
	else if (!strncmp(parm[0], "4k_enhance", 10)) {
		if (!strncmp(parm[1], "core0", 5)) {
			if (!strncmp(parm[2], "00", 2)) {
				vpp_sr3_enhance_enable(0x0);
				pr_info("disable core0 sr3 dering/dejaggy/direction\n");
			} else if (!strncmp(parm[2], "01", 2)) {
				vpp_sr3_enhance_enable(0x1);
				pr_info("enable core0 sr3 dering/dejaggy/direction\n");
			}
		} else if (!strncmp(parm[1], "core1", 2)) {
			if (!strncmp(parm[2], "10", 2)) {
				vpp_sr3_enhance_enable(0x10);
				pr_info("disable core1 sr3 dering/dejaggy/direction\n");
			} else if (!strncmp(parm[2], "11", 2)) {
				vpp_sr3_enhance_enable(0x11);
				pr_info("enable core1 sr3 dering/dejaggy/direction\n");
			}
		}
	} else if (!strncmp(parm[0], "wb", 2)) {
		if (!strncmp(parm[1], "enable", 6)) {
			amvecm_wb_enable(1);
			pr_info("enable wb\n");
		} else if (!strncmp(parm[1], "disable", 7)) {
			amvecm_wb_enable(0);
			pr_info("disable wb\n");
		}
	} else if (!strncmp(parm[0], "gamma", 5)) {
		if (!strncmp(parm[1], "enable", 6)) {
			vecm_latch_flag |= FLAG_GAMMA_TABLE_EN;	/* gamma off */
			pr_info("enable gamma\n");
		} else if (!strncmp(parm[1], "disable", 7)) {
			vecm_latch_flag |= FLAG_GAMMA_TABLE_DIS;/* gamma off */
			pr_info("disable gamma\n");
		}
	} else if (!strncmp(parm[0], "sr", 2)) {
		if (!strncmp(parm[1], "peaking_en", 10)) {
			amvecm_sharpness_debug(0);
			pr_info("enable peaking\n");
		} else if (!strncmp(parm[1], "peaking_dis", 11)) {
			amvecm_sharpness_debug(1);
			pr_info("disable peaking\n");
		} else if (!strncmp(parm[1], "lcti_en", 7)) {
			amvecm_sharpness_debug(2);
			pr_info("enable lti cti\n");
		} else if (!strncmp(parm[1], "lcti_dis", 8)) {
			amvecm_sharpness_debug(3);
			pr_info("disable lti cti\n");
		}
	} else if (!strncmp(parm[0], "cm", 2)) {
		if (!strncmp(parm[1], "enable", 6)) {
			amcm_enable();
			pr_info("enable cm\n");
		} else if (!strncmp(parm[1], "disable", 7)) {
			amcm_disable();
			pr_info("disable cm\n");
		}
	} else if (!strncmp(parm[0], "dnlp", 4)) {
		if (!strncmp(parm[1], "enable", 6)) {
			ve_enable_dnlp();
			pr_info("enable dnlp\n");
		} else if (!strncmp(parm[1], "disable", 7)) {
			ve_disable_dnlp();
			pr_info("disable dnlp\n");
		}
	} else if (!strncmp(parm[0], "vpp_pq", 6)) {
		if (!strncmp(parm[1], "enable", 6)) {
			amvecm_pq_enable(1);
			pr_info("enable vpp_pq\n");
		} else if (!strncmp(parm[1], "disable", 7)) {
			amvecm_pq_enable(0);
			pr_info("disable vpp_pq\n");
		}
	}

	kfree(buf_orig);
	return count;
}

/* supported mode: IPT_TUNNEL/HDR10/SDR10 */
static const int dv_mode_table[6] = {
	5, /*DOLBY_VISION_OUTPUT_MODE_BYPASS*/
	0, /*DOLBY_VISION_OUTPUT_MODE_IPT*/
	1, /*DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL*/
	2, /*DOLBY_VISION_OUTPUT_MODE_HDR10*/
	3, /*DOLBY_VISION_OUTPUT_MODE_SDR10*/
	4, /*DOLBY_VISION_OUTPUT_MODE_SDR8*/
};

static const char dv_mode_str[6][12] = {
	"IPT",
	"IPT_TUNNEL",
	"HDR10",
	"SDR10",
	"SDR8",
	"BYPASS"
};

static ssize_t amvecm_dv_mode_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	pr_info("usage: echo mode > /sys/class/amvecm/dv_mode\n");
	pr_info("\tDOLBY_VISION_OUTPUT_MODE_BYPASS		0\n");
	pr_info("\tDOLBY_VISION_OUTPUT_MODE_IPT			1\n");
	pr_info("\tDOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL	2\n");
	pr_info("\tDOLBY_VISION_OUTPUT_MODE_HDR10		3\n");
	pr_info("\tDOLBY_VISION_OUTPUT_MODE_SDR10		4\n");
	pr_info("\tDOLBY_VISION_OUTPUT_MODE_SDR8		5\n");
	if (is_meson_gxm_cpu() && is_dolby_vision_enable())
		pr_info("current dv_mode = %s\n",
			dv_mode_str[get_dolby_vision_mode()]);
	else
		pr_info("current dv_mode = off\n");
	return 0;
}

static ssize_t amvecm_dv_mode_store(struct class *cla,
			struct class_attribute *attr,
			const char *buf, size_t count)
{
	size_t r;
	int val;

	if (is_meson_gxm_cpu()) {
		r = sscanf(buf, "0x%x", &val);
		if ((r != 1))
			return -EINVAL;
		if ((val >= 0) && (val < 6))
			set_dolby_vision_mode(dv_mode_table[val]);
		else if (val & 0x200)
			dolby_vision_dump_struct();
		else if (val & 0x70)
			dolby_vision_dump_setting(val);
	}
	return count;
}

/* #if (MESON_CPU_TYPE == MESON_CPU_TYPE_MESONG9TV) */
void init_sharpness(void)
{
	/*probe close sr0 peaking for switch on video*/
	WRITE_VPP_REG_BITS(VPP_SRSHARP0_CTRL, 1, 0, 1);
	/*WRITE_VPP_REG_BITS(VPP_SRSHARP1_CTRL, 1,0,1);*/
	WRITE_VPP_REG_BITS(SRSHARP0_SHARP_PK_NR_ENABLE, 0, 1, 1);

	WRITE_VPP_REG_BITS(VPP_SRSHARP1_CTRL, 1, 0, 1);

	if (is_meson_txl_cpu()) {
		WRITE_VPP_REG_BITS(SRSHARP1_PK_FINALGAIN_HP_BP, 2, 16, 2);

		/*sr0 sr1 chroma filter bypass*/
		WRITE_VPP_REG(SRSHARP0_SHARP_SR2_CBIC_HCOEF0, 0x4000);
		WRITE_VPP_REG(SRSHARP0_SHARP_SR2_CBIC_VCOEF0, 0x4000);
		WRITE_VPP_REG(SRSHARP1_SHARP_SR2_CBIC_HCOEF0, 0x4000);
		WRITE_VPP_REG(SRSHARP1_SHARP_SR2_CBIC_VCOEF0, 0x4000);
	}
}
/* #endif*/

static void amvecm_gamma_init(bool en)
{
	unsigned int i;
	unsigned short data[256];

	if (en) {
		WRITE_VPP_REG_BITS(L_GAMMA_CNTL_PORT,
				0, GAMMA_EN, 1);

		for (i = 0; i < 256; i++)
			data[i] = i << 2;
		init_write_gamma_table(
					data,
					H_SEL_R);
		init_write_gamma_table(
					data,
					H_SEL_G);
		init_write_gamma_table(
					data,
					H_SEL_B);
	}
}
static void amvecm_wb_init(bool en)
{
	if (en) {
		WRITE_VPP_REG(VPP_GAINOFF_CTRL0,
			(1024 << 16) | 1024);
		WRITE_VPP_REG(VPP_GAINOFF_CTRL1,
			(1024 << 16));
	}

	WRITE_VPP_REG_BITS(VPP_GAINOFF_CTRL0, en, 31, 1);
}

static struct class_attribute amvecm_class_attrs[] = {
	__ATTR(debug, 0644,
		amvecm_debug_show, amvecm_debug_store),
	__ATTR(dnlp, 0644,
		amvecm_dnlp_show, amvecm_dnlp_store),
	__ATTR(brightness, 0644,
		amvecm_brightness_show, amvecm_brightness_store),
	__ATTR(contrast, 0644,
		amvecm_contrast_show, amvecm_contrast_store),
	__ATTR(saturation_hue, 0644,
		amvecm_saturation_hue_show,
		amvecm_saturation_hue_store),
	__ATTR(saturation_hue_pre, 0644,
		amvecm_saturation_hue_pre_show,
		amvecm_saturation_hue_pre_store),
	__ATTR(saturation_hue_post, 0644,
		amvecm_saturation_hue_post_show,
		amvecm_saturation_hue_post_store),
	__ATTR(cm2, 0644,
		amvecm_cm2_show,
		amvecm_cm2_store),
	__ATTR(gamma, 0644,
		amvecm_gamma_show,
		amvecm_gamma_store),
	__ATTR(wb, 0644,
		amvecm_wb_show,
		amvecm_wb_store),
	__ATTR(brightness1, 0644,
		video_adj1_brightness_show,
		video_adj1_brightness_store),
	__ATTR(contrast1, 0644,
		video_adj1_contrast_show, video_adj1_contrast_store),
	__ATTR(brightness2, 0644,
		video_adj2_brightness_show, video_adj2_brightness_store),
	__ATTR(contrast2, 0644,
		video_adj2_contrast_show, video_adj2_contrast_store),
	__ATTR(help, 0644,
		amvecm_usage_show, NULL),
/* #if (MESON_CPU_TYPE >= MESON_CPU_TYPE_MESONG9TV) */
	__ATTR(sync_3d, 0644,
		amvecm_3d_sync_show,
		amvecm_3d_sync_store),
	__ATTR(vlock, 0644,
		amvecm_vlock_show,
		amvecm_vlock_store),
	__ATTR(matrix_set, 0644,
		amvecm_set_post_matrix_show, amvecm_set_post_matrix_store),
	__ATTR(matrix_pos, 0644,
		amvecm_post_matrix_pos_show, amvecm_post_matrix_pos_store),
	__ATTR(matrix_data, 0644,
		amvecm_post_matrix_data_show, amvecm_post_matrix_data_store),
	__ATTR(dump_reg, 0644,
		amvecm_dump_reg_show, amvecm_dump_reg_store),
	__ATTR(sr1_reg, 0644,
		amvecm_sr1_reg_show, amvecm_sr1_reg_store),
	__ATTR(write_sr1_reg_val, 0644,
		amvecm_write_sr1_reg_val_show, amvecm_write_sr1_reg_val_store),
	__ATTR(dump_vpp_hist, 0644,
		amvecm_dump_vpp_hist_show, amvecm_dump_vpp_hist_store),
	__ATTR(hdr_dbg, 0644,
			amvecm_hdr_dbg_show, amvecm_hdr_dbg_store),
	__ATTR(hdr_reg, 0644,
			amvecm_hdr_reg_show, amvecm_hdr_reg_store),
	__ATTR(gamma_pattern, 0644,
		set_gamma_pattern_show, set_gamma_pattern_store),
	__ATTR(pc_mode, 0644,
		amvecm_pc_mode_show, amvecm_pc_mode_store),
	__ATTR(set_hdr_289lut, 0644,
		set_hdr_289lut_show, set_hdr_289lut_store),
	__ATTR(vpp_demo, 0644,
		amvecm_vpp_demo_show, amvecm_vpp_demo_store),
	__ATTR(dv_mode, 0644,
		amvecm_dv_mode_show, amvecm_dv_mode_store),
	__ATTR_NULL
};

static const struct file_operations amvecm_fops = {
	.owner   = THIS_MODULE,
	.open    = amvecm_open,
	.release = amvecm_release,
	.unlocked_ioctl   = amvecm_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = amvecm_compat_ioctl,
#endif
};
static void aml_vecm_dt_parse(struct platform_device *pdev)
{
	struct device_node *node;
	unsigned int val;
	int ret;

	node = pdev->dev.of_node;
	/* get integer value */
	if (node) {
		ret = of_property_read_u32(node, "gamma_en", &val);
		if (ret)
			pr_info("Can't find  gamma_en.\n");
		else
			gamma_en = val;
		ret = of_property_read_u32(node, "wb_en", &val);
		if (ret)
			pr_info("Can't find  wb_en.\n");
		else
			wb_en = val;
		ret = of_property_read_u32(node, "cm_en", &val);
		if (ret)
			pr_info("Can't find  cm_en.\n");
		else
			cm_en = val;
	}
	/* init module status */
	amvecm_wb_init(wb_en);
	amvecm_gamma_init(gamma_en);
	if (!is_dolby_vision_enable())
		WRITE_VPP_REG_BITS(VPP_MISC, 1, 28, 1);
	if (cm_en)
		amcm_enable();
	else
		amcm_disable();
	/* WRITE_VPP_REG_BITS(VPP_MISC, cm_en, 28, 1); */
}

#ifdef CONFIG_AMLOGIC_LCD
static int aml_lcd_gamma_notifier(struct notifier_block *nb,
		unsigned long event, void *data)
{
	if ((event & LCD_EVENT_GAMMA_UPDATE) == 0)
		return NOTIFY_DONE;

#if 0
	vpp_set_lcd_gamma_table(video_gamma_table_r.data, H_SEL_R);
	vpp_set_lcd_gamma_table(video_gamma_table_g.data, H_SEL_G);
	vpp_set_lcd_gamma_table(video_gamma_table_b.data, H_SEL_B);
#else
	vecm_latch_flag |= FLAG_GAMMA_TABLE_R;
	vecm_latch_flag |= FLAG_GAMMA_TABLE_G;
	vecm_latch_flag |= FLAG_GAMMA_TABLE_B;
#endif

	return NOTIFY_OK;
}

static struct notifier_block aml_lcd_gamma_nb = {
	.notifier_call = aml_lcd_gamma_notifier,
};
#endif
static int aml_vecm_probe(struct platform_device *pdev)
{
	int ret = 0;
	int i = 0;

	struct amvecm_dev_s *devp = &amvecm_dev;

	memset(devp, 0, (sizeof(struct amvecm_dev_s)));
	pr_info("\n VECM probe start\n");
	ret = alloc_chrdev_region(&devp->devno, 0, 1, AMVECM_NAME);
	if (ret < 0)
		goto fail_alloc_region;

	devp->clsp = class_create(THIS_MODULE, AMVECM_CLASS_NAME);
	if (IS_ERR(devp->clsp)) {
		ret = PTR_ERR(devp->clsp);
		goto fail_create_class;
	}
	for (i = 0; amvecm_class_attrs[i].attr.name; i++) {
		if (class_create_file(devp->clsp, &amvecm_class_attrs[i]) < 0)
			goto fail_class_create_file;
	}
	cdev_init(&devp->cdev, &amvecm_fops);
	devp->cdev.owner = THIS_MODULE;
	ret = cdev_add(&devp->cdev, devp->devno, 1);
	if (ret)
		goto fail_add_cdev;

	devp->dev = device_create(devp->clsp, NULL, devp->devno,
			NULL, AMVECM_NAME);
	if (IS_ERR(devp->dev)) {
		ret = PTR_ERR(devp->dev);
		goto fail_create_device;
	}

	spin_lock_init(&vpp_lcd_gamma_lock);
#ifdef CONFIG_AMLOGIC_LCD
	ret = aml_lcd_notifier_register(&aml_lcd_gamma_nb);
	if (ret)
		pr_info("register aml_lcd_gamma_notifier failed\n");
#endif

	amvecm_hiu_reg_base = ioremap(HIU_REG_BASE, 0x2000);

	/* #if (MESON_CPU_TYPE == MESON_CPU_TYPE_MESONG9TV) */
	if (is_meson_gxtvbb_cpu() || is_meson_txl_cpu())
		init_sharpness();
	/* #endif */
	vpp_get_hist_en();

	memset(&vpp_hist_param.vpp_histgram[0],
		0, sizeof(unsigned short) * 64);
	/* box sdr_mode:auto, tv sdr_mode:off */
	/* disable contrast and saturation adjustment for HDR on TV */
	/* disable SDR to HDR convert on TV */
	if (is_meson_gxl_cpu() || is_meson_gxm_cpu()) {
		sdr_mode = 0;
		hdr_flag = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3);
	} else {
		sdr_mode = 0;
		hdr_flag = (1 << 0) | (1 << 1) | (0 << 2) | (0 << 3);
	}
	aml_vecm_dt_parse(pdev);
	if (is_meson_gxm_cpu())
		dolby_vision_init_receiver();
	probe_ok = 1;
	pr_info("%s: ok\n", __func__);
	return 0;

fail_create_device:
	pr_info("[amvecm.] : amvecm device create error.\n");
	cdev_del(&devp->cdev);
fail_add_cdev:
	pr_info("[amvecm.] : amvecm add device error.\n");
	kfree(devp);
fail_class_create_file:
	pr_info("[amvecm.] : amvecm class create file error.\n");
	for (i = 0; amvecm_class_attrs[i].attr.name; i++) {
		class_remove_file(devp->clsp,
		&amvecm_class_attrs[i]);
	}
	class_destroy(devp->clsp);
fail_create_class:
	pr_info("[amvecm.] : amvecm class create error.\n");
	unregister_chrdev_region(devp->devno, 1);
fail_alloc_region:
	pr_info("[amvecm.] : amvecm alloc error.\n");
	pr_info("[amvecm.] : amvecm_init.\n");
	return ret;
}

static int __exit aml_vecm_remove(struct platform_device *pdev)
{
	struct amvecm_dev_s *devp = &amvecm_dev;

	device_destroy(devp->clsp, devp->devno);
	cdev_del(&devp->cdev);
	class_destroy(devp->clsp);
	unregister_chrdev_region(devp->devno, 1);
	kfree(devp);
#ifdef CONFIG_AMLOGIC_LCD
	aml_lcd_notifier_unregister(&aml_lcd_gamma_nb);
#endif
	probe_ok = 0;
	pr_info("[amvecm.] : amvecm_exit.\n");
	return 0;
}

#ifdef CONFIG_PM
static int amvecm_drv_suspend(struct platform_device *pdev,
		pm_message_t state)
{
	if (probe_ok == 1)
		probe_ok = 0;
	pr_info("amvecm: suspend module\n");
	return 0;
}

static int amvecm_drv_resume(struct platform_device *pdev)
{
	if (probe_ok == 0)
		probe_ok = 1;

	pr_info("amvecm: resume module\n");
	return 0;
}
#endif


static const struct of_device_id aml_vecm_dt_match[] = {
	{
		.compatible = "amlogic, vecm",
	},
	{},
};

static struct platform_driver aml_vecm_driver = {
	.driver = {
		.name = "aml_vecm",
		.owner = THIS_MODULE,
		.of_match_table = aml_vecm_dt_match,
	},
	.probe = aml_vecm_probe,
	.remove = __exit_p(aml_vecm_remove),
#ifdef CONFIG_PM
	.suspend    = amvecm_drv_suspend,
	.resume     = amvecm_drv_resume,
#endif

};

static int __init aml_vecm_init(void)
{
	pr_info("module init\n");
	/* remap the hiu bus */
	if (platform_driver_register(&aml_vecm_driver)) {
		pr_err("failed to register bl driver module\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit aml_vecm_exit(void)
{
	pr_info("module exit\n");
	platform_driver_unregister(&aml_vecm_driver);
}

module_init(aml_vecm_init);
module_exit(aml_vecm_exit);

MODULE_DESCRIPTION("AMLOGIC amvecm driver");
MODULE_LICENSE("GPL");

