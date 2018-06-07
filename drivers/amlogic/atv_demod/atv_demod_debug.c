/*
 * drivers/amlogic/atv_demod/atv_demod_debug.c
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

#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>

#include "atv_demod_debug.h"


#if !defined(AML_ATVDEMOD_DEBUGFS)

#undef u32
#define u32 uint

#undef u64
#define u64 ulong

#endif

/* name, mode, parent, type, value */
/* u32, bool, u64 type add here for debugfs */
#define DEBUG_FS_CREATE_NODES(dentry)\
{\
	DEBUGFS_CREATE_NODE(reg_23cf, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(btsc_sap_mode, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(afc_limit, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(aml_timer_en, 0640, dentry, bool)\
	DEBUGFS_CREATE_NODE(timer_delay, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(timer_delay2, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(timer_delay3, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(afc_wave_cnt, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(atvdemod_scan_mode, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(tuner_status_cnt, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(slow_mode, 0640, dentry, bool)\
	DEBUGFS_CREATE_NODE(broad_std, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(aud_std, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(aud_mode, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(aud_auto, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(over_threshold, 0640, dentry, u64)\
	DEBUGFS_CREATE_NODE(input_amplitude, 0640, dentry, u64)\
	DEBUGFS_CREATE_NODE(audio_det_en, 0640, dentry, bool)\
	DEBUGFS_CREATE_NODE(non_std_en, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(non_std_onoff, 0640, dentry, bool)\
	DEBUGFS_CREATE_NODE(non_std_times, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(atv_video_gain, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(carrier_amplif_val, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(extra_input_fil_val, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(audio_det_mode, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(aud_dmd_jilinTV, 0640, dentry, bool)\
	DEBUGFS_CREATE_NODE(if_freq, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(if_inv, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(ademod_debug_en, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(btsc_detect_delay, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(nicam_detect_delay, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(signal_audmode, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(audio_thd_threshold1, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(gde_curve, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(sound_format, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(freq_hz_cvrt, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(atvdemod_debug_en, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(atvdemod_agc_pinmux, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(atvdemod_afc_range, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(atvdemod_afc_offset, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(atvdemod_timer_en, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(atvdemod_afc_en, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(atvdemod_monitor_en, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(audio_thd_en, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(pwm_kp, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(reg_dbg_en, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(audio_gain_val, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(audio_a2_threshold, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(audio_a2_delay, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(audio_nicam_delay, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(audio_a2_auto, 0640, dentry, u32)\
	DEBUGFS_CREATE_NODE(audio_a2_power_threshold, 0640, dentry, u32)\
}


/* name, mode, parent, data, fops, type */
/* int type add here for debugfs */
#define DEBUG_FS_CREATE_FILES(dentry, fops)\
{\
	DEBUGFS_CREATE_FILE(afc_offset, 0640, dentry, fops, int)\
	DEBUGFS_CREATE_FILE(non_std_thld_4c_h, 0640, dentry, fops, int)\
	DEBUGFS_CREATE_FILE(non_std_thld_4c_l, 0640, dentry, fops, int)\
	DEBUGFS_CREATE_FILE(non_std_thld_54_h, 0640, dentry, fops, int)\
	DEBUGFS_CREATE_FILE(non_std_thld_54_l, 0640, dentry, fops, int)\
	DEBUGFS_CREATE_FILE(sum1_thd_h, 0640, dentry, fops, int)\
	DEBUGFS_CREATE_FILE(sum1_thd_l, 0640, dentry, fops, int)\
	DEBUGFS_CREATE_FILE(sum2_thd_h, 0640, dentry, fops, int)\
	DEBUGFS_CREATE_FILE(sum2_thd_l, 0640, dentry, fops, int)\
	DEBUGFS_CREATE_FILE(afc_default, 0640, dentry, fops, int)\
}


#if defined(AML_ATVDEMOD_DEBUGFS)
static struct dentry *debugfs_root;

DEBUGFS_DENTRY_DEFINE(afc_offset);
DEBUGFS_DENTRY_DEFINE(non_std_thld_4c_h);
DEBUGFS_DENTRY_DEFINE(non_std_thld_4c_l);
DEBUGFS_DENTRY_DEFINE(non_std_thld_54_h);
DEBUGFS_DENTRY_DEFINE(non_std_thld_54_l);
DEBUGFS_DENTRY_DEFINE(sum1_thd_h);
DEBUGFS_DENTRY_DEFINE(sum1_thd_l);
DEBUGFS_DENTRY_DEFINE(sum2_thd_h);
DEBUGFS_DENTRY_DEFINE(sum2_thd_l);
DEBUGFS_DENTRY_DEFINE(afc_default);

struct dentry_value *debugfs_dentry[] = {
	DEBUGFS_DENTRY_VALUE(afc_offset),
	DEBUGFS_DENTRY_VALUE(non_std_thld_4c_h),
	DEBUGFS_DENTRY_VALUE(non_std_thld_4c_l),
	DEBUGFS_DENTRY_VALUE(non_std_thld_54_h),
	DEBUGFS_DENTRY_VALUE(non_std_thld_54_l),
	DEBUGFS_DENTRY_VALUE(sum1_thd_h),
	DEBUGFS_DENTRY_VALUE(sum1_thd_l),
	DEBUGFS_DENTRY_VALUE(sum2_thd_h),
	DEBUGFS_DENTRY_VALUE(sum2_thd_l),
	DEBUGFS_DENTRY_VALUE(afc_default),
};

static int debugfs_open(struct inode *node, struct file *file)
{
	return 0;
}

static ssize_t debugfs_read(struct file *file, char __user *userbuf,
		size_t count, loff_t *ppos)
{
	struct dentry *dent = file->f_path.dentry;
	int *val = NULL;
	int i = 0;
	char buf[20] = { 0 };
	int len = ARRAY_SIZE(debugfs_dentry);

	for (i = 0; i < len; ++i) {
		if (debugfs_dentry[i]->dentry == dent) {
			val = debugfs_dentry[i]->value;
			break;
		}
	}

	if (val == NULL)
		return -EINVAL;

	len = snprintf(buf, sizeof(buf), "%d\n", *val);

	return simple_read_from_buffer(userbuf, count, ppos, buf, len);
}

static ssize_t debugfs_write(struct file *file, const char __user *userbuf,
		size_t count, loff_t *ppos)
{
	struct dentry *dent = file->f_path.dentry;
	int val = 0;
	int i = 0;
	char buf[20] = { 0 };
	int len = ARRAY_SIZE(debugfs_dentry);

	count = min_t(size_t, count, (sizeof(buf) - 1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;

	/*i = sscanf(buf, "%d", &val);*/
	i = kstrtoint(buf, 0, &val);
	if (i == 0) {
		for (i = 0; i < len; ++i) {
			if (debugfs_dentry[i]->dentry == dent) {
				*(debugfs_dentry[i]->value) = val;
				break;
			}
		}
	} else
		return -EINVAL;

	return count;
}

static const struct file_operations dfs_fops = {
	.open  = debugfs_open,
	.read  = debugfs_read,
	.write = debugfs_write,
};
#else
DEBUG_FS_CREATE_NODES(NULL);
DEBUG_FS_CREATE_FILES(NULL, NULL);
#endif

int aml_atvdemod_create_debugfs(const char *name)
{
#if defined(AML_ATVDEMOD_DEBUGFS)
	debugfs_root = debugfs_create_dir(name, NULL);

	if (IS_ERR(debugfs_root) || !debugfs_root) {
		pr_warn("failed to create debugfs directory\n");
		debugfs_root = NULL;
		return -1;
	}

	DEBUG_FS_CREATE_NODES(debugfs_root);
	DEBUG_FS_CREATE_FILES(debugfs_root, dfs_fops);
#endif

	return 0;
}

void aml_atvdemod_remove_debugfs(void)
{
#if defined(AML_ATVDEMOD_DEBUGFS)
	if (debugfs_root != NULL) {
		debugfs_remove_recursive(debugfs_root);
		debugfs_root = NULL;
	}
#endif
}
