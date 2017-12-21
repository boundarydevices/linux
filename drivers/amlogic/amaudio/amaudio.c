/*
 * drivers/amlogic/amaudio/amaudio.c
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

#define pr_fmt(fmt) "amaudio: " fmt

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

/* Amlogic headers */
#include <linux/amlogic/major.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/media/sound/aiu_regs.h>
#include <linux/amlogic/media/sound/audin_regs.h>
#include <linux/amlogic/media/sound/audio_iomap.h>

#include "amaudio.h"

#define AMAUDIO_DEVICE_COUNT    ARRAY_SIZE(amaudio_ports)

static int amaudio_open(struct inode *inode, struct file *file);
static int amaudio_release(struct inode *inode, struct file *file);
static long amaudio_ioctl(struct file *file, unsigned int cmd,
			  unsigned long arg);
static int amaudio_utils_open(struct inode *inode, struct file *file);
static int amaudio_utils_release(struct inode *inode, struct file *file);
static long amaudio_utils_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg);

#ifdef CONFIG_COMPAT
static long amaudio_compat_ioctl(struct file *file, unsigned int cmd,
				 ulong arg);
static long amaudio_compat_utils_ioctl(struct file *file, unsigned int cmd,
				       ulong arg);
#endif

struct amaudio_port_t {
	const char *name;
	struct device *dev;
	const struct file_operations *fops;
};

static unsigned int mute_left_right;
static unsigned int mute_unmute;
static struct class *amaudio_classp;
static struct device *amaudio_dev;

static const struct file_operations amaudio_ctl_fops = {
	.owner = THIS_MODULE,
	.open = amaudio_open,
	.release = amaudio_release,
	.unlocked_ioctl = amaudio_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = amaudio_compat_ioctl,
#endif
};

static const struct file_operations amaudio_utils_fops = {
	.owner = THIS_MODULE,
	.open = amaudio_utils_open,
	.release = amaudio_utils_release,
	.unlocked_ioctl = amaudio_utils_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = amaudio_compat_utils_ioctl,
#endif
};

static struct amaudio_port_t amaudio_ports[] = {
	{
	 .name = "amaudio_ctl",
	 .fops = &amaudio_ctl_fops,
	 },
	{
	 .name = "amaudio_utils",
	 .fops = &amaudio_utils_fops,
	 },
};

static const struct file_operations amaudio_fops = {
	.owner = THIS_MODULE,
	.open = amaudio_open,
	.release = amaudio_release,
	.unlocked_ioctl = amaudio_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = amaudio_compat_ioctl,
#endif
};

static int amaudio_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int amaudio_release(struct inode *inode, struct file *file)
{
	return 0;
}

static long amaudio_ioctl(struct file *file, unsigned int cmd,
			  unsigned long arg)
{
	s32 r = 0;
	u32 reg;

	switch (cmd) {
	case AMAUDIO_IOC_SET_LEFT_MONO:
		pr_info("set audio i2s left mono!\n");
		audio_i2s_swap_left_right(1);
		break;
	case AMAUDIO_IOC_SET_RIGHT_MONO:
		pr_info("set audio i2s right mono!\n");
		audio_i2s_swap_left_right(2);
		break;
	case AMAUDIO_IOC_SET_STEREO:
		pr_info("set audio i2s stereo!\n");
		audio_i2s_swap_left_right(0);
		break;
	case AMAUDIO_IOC_SET_CHANNEL_SWAP:
		pr_info("set audio i2s left/right swap!\n");
		reg = read_i2s_mute_swap_reg();
		if (reg & 0x3)
			audio_i2s_swap_left_right(0);
		else
			audio_i2s_swap_left_right(3);
		break;
	case AMAUDIO_IOC_MUTE_LEFT_RIGHT_CHANNEL:
		pr_info("set audio 958 mute left/right!\n");
		audio_mute_left_right(arg);
		break;
	case AMAUDIO_IOC_MUTE_UNMUTE:
		pr_info("set audio mute/unmute!\n");
		if (arg == 1)
			audio_i2s_mute();
		else if (arg == 0)
			audio_i2s_unmute();
		break;
	default:
		break;

	};
	return r;
}

static int amaudio_utils_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int amaudio_utils_release(struct inode *inode, struct file *file)
{
	return 0;
}

static long amaudio_utils_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	return 0;
}

#ifdef CONFIG_COMPAT

static long amaudio_compat_ioctl(struct file *file, unsigned int cmd, ulong arg)
{
	return amaudio_ioctl(file, cmd, (ulong) compat_ptr(arg));
}

static long amaudio_compat_utils_ioctl(struct file *file,
				       unsigned int cmd, ulong arg)
{
	return amaudio_utils_ioctl(file, cmd, (ulong) compat_ptr(arg));
}

#endif

static ssize_t show_audio_channels_mask(struct class *class,
					struct class_attribute *attr, char *buf)
{
	ssize_t ret = 0;

	ret =
		sprintf(buf,
		 "echo l/r/s/c to /sys/class/amaudio/audio_channels_mask\n"
	    "file to mask audio output.\n l : left channel mono output.\n"
	    "r : right channel mono output.\n s : stereo output.\n"
	    "c : swap left and right channels.\n");

	return ret;
}

static ssize_t store_audio_channels_mask(struct class *class,
					 struct class_attribute *attr,
					 const char *buf, size_t count)
{
	u32 reg;

	switch (buf[0]) {
	case 'l':
		audio_i2s_swap_left_right(1);
		break;

	case 'r':
		audio_i2s_swap_left_right(2);
		break;

	case 's':
		audio_i2s_swap_left_right(0);
		break;

	case 'c':
		reg = read_i2s_mute_swap_reg();
		if (reg & 0x3)
			audio_i2s_swap_left_right(0);
		else
			audio_i2s_swap_left_right(3);
		break;

	default:
		pr_info("unknown command!\n");
	}

	return count;
}

static ssize_t dac_mute_const_show(struct class *cla,
				   struct class_attribute *attr, char *buf)
{
	char *pbuf = buf;

	pbuf += sprintf(pbuf, "dac mute const val  0x%x\n", dac_mute_const);
	return pbuf - buf;
}

static ssize_t dac_mute_const_store(struct class *class,
				    struct class_attribute *attr,
				    const char *buf, size_t count)
{
	int val = dac_mute_const;

	if (buf[0] && kstrtoint(buf, 16, &val))
		return -EINVAL;

	if (val == 0 || val == 0x800000)
		dac_mute_const = val;
	pr_info("dac mute const val set to 0x%x\n", val);
	return count;
}

static ssize_t output_enable_show(struct class *class,
				  struct class_attribute *attr, char *buf)
{
/*
 * why limit this condition of iec958 buffer size bigger than 128,
 * because we begin to use a 128 bytes zero playback mode
 * of 958 output when alsa driver is not called by user space to
 * avoid some noise.This mode should must separate this case
 * with normal playback case to avoid one risk:
 * when EOF,the last three seconds this is no audio pcm decoder
 * to output.the zero playback mode is triggered,
 * this cause the player has no chance to  trigger the exit condition
 */
	unsigned int iec958_size =
	    aml_aiu_read(AIU_MEM_IEC958_END_PTR) -
	    aml_aiu_read(AIU_MEM_IEC958_START_PTR);
	/* normal spdif buffer MUST NOT less than 512 bytes */
	return sprintf(buf, "%d\n", (if_audio_out_enable() ||
			(if_958_audio_out_enable() && iec958_size > 512)));
}


enum {
	I2SIN_MASTER_MODE = 0,
	I2SIN_SLAVE_MODE = 1 << 0,
	SPDIFIN_MODE = 1 << 1,
};
static ssize_t record_type_store(struct class *class,
				 struct class_attribute *attr, const char *buf,
				 size_t count)
{
	if (buf[0] == '0')
		audioin_mode = I2SIN_MASTER_MODE;
	else if (buf[0] == '1')
		audioin_mode = I2SIN_SLAVE_MODE;
	else if (buf[0] == '2')
		audioin_mode = SPDIFIN_MODE;

	return count;
}

static ssize_t record_type_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	if (audioin_mode == I2SIN_MASTER_MODE) {	/* mic */
		return sprintf(buf, "i2s in master mode for built in mic\n");
	} else if (audioin_mode == SPDIFIN_MODE) {	/* spdif in mode */
		return sprintf(buf, "spdif in mode\n");
	} else if (audioin_mode == I2SIN_SLAVE_MODE) {	/* i2s in slave */
		return sprintf(buf, "i2s in slave mode\n");
	} else {
		return sprintf(buf, "audioin_mode can't match mode\n");
	}
}

static int dtsm6_stream_type = -1;
static unsigned int dtsm6_apre_cnt;
static unsigned int dtsm6_apre_sel;
static unsigned int dtsm6_apre_assets_sel;
static unsigned int dtsm6_mulasset_hint;
static char dtsm6_apres_assets_Array[32] = { 0 };

static unsigned int dtsm6_HPS_hint;
static ssize_t store_debug(struct class *class, struct class_attribute *attr,
			   const char *buf, size_t count)
{
	if (strncmp(buf, "chstatus_set", 12) == 0) {
		aml_aiu_write(AIU_958_VALID_CTRL, 0);
		aml_aiu_write(AIU_958_CHSTAT_L0, 0x1900);
		aml_aiu_write(AIU_958_CHSTAT_R0, 0x1900);
	} else if (strncmp(buf, "chstatus_off", 12) == 0) {
		aml_aiu_write(AIU_958_VALID_CTRL, 3);
		aml_aiu_write(AIU_958_CHSTAT_L0, 0x1902);
		aml_aiu_write(AIU_958_CHSTAT_R0, 0x1902);
	} else if (strncmp(buf, "dtsm6_stream_type_set", 21) == 0) {
		if (kstrtoint(buf + 21, 10, &dtsm6_stream_type))
			return -EINVAL;
	} else if (strncmp(buf, "dtsm6_apre_cnt_set", 18) == 0) {
		if (kstrtoint(buf + 18, 10, &dtsm6_apre_cnt))
			return -EINVAL;
	} else if (strncmp(buf, "dtsm6_apre_sel_set", 18) == 0) {
		if (kstrtoint(buf + 18, 10, &dtsm6_apre_sel))
			return -EINVAL;
	} else if (strncmp(buf, "dtsm6_apres_assets_set", 22) == 0) {
		if (dtsm6_apre_cnt > 32) {
			pr_info("[%s %d]unvalid dtsm6_apre_cnt/%d\n", __func__,
				__LINE__, dtsm6_apre_cnt);
		} else {
			memcpy(dtsm6_apres_assets_Array, buf + 22,
			       dtsm6_apre_cnt);
		}
	} else if (strncmp(buf, "dtsm6_apre_assets_sel_set", 25) == 0) {
		if (kstrtoint(buf + 25, 10, &dtsm6_apre_assets_sel))
			return -EINVAL;
	} else if (strncmp(buf, "dtsm6_mulasset_hint", 19) == 0) {
		if (kstrtoint(buf + 19, 10, &dtsm6_mulasset_hint))
			return -EINVAL;
	} else if (strncmp(buf, "dtsm6_clear_info", 16) == 0) {
		dtsm6_stream_type = -1;
		dtsm6_apre_cnt = 0;
		dtsm6_apre_sel = 0;
		dtsm6_apre_assets_sel = 0;
		dtsm6_mulasset_hint = 0;
		dtsm6_HPS_hint = 0;
		memset(dtsm6_apres_assets_Array, 0,
		       sizeof(dtsm6_apres_assets_Array));
	} else if (strncmp(buf, "dtsm6_hps_hint", 14) == 0) {
		if (kstrtoint(buf + 14, 10, &dtsm6_HPS_hint))
			return -EINVAL;
	}
	return count;
}

static ssize_t show_debug(struct class *class, struct class_attribute *attr,
			  char *buf)
{
	int pos = 0;

	pos += sprintf(buf + pos, "dtsM6:StreamType%d\n", dtsm6_stream_type);
	pos += sprintf(buf + pos, "ApreCnt%d\n", dtsm6_apre_cnt);
	pos += sprintf(buf + pos, "ApreSel%d\n", dtsm6_apre_sel);
	pos += sprintf(buf + pos, "ApreAssetSel%d\n", dtsm6_apre_assets_sel);
	pos += sprintf(buf + pos, "MulAssetHint%d\n", dtsm6_mulasset_hint);
	pos += sprintf(buf + pos, "HPSHint%d\n", dtsm6_HPS_hint);
	pos += sprintf(buf + pos, "ApresAssetsArray");
	memcpy(buf + pos, dtsm6_apres_assets_Array,
	       sizeof(dtsm6_apres_assets_Array));
	pos += sizeof(dtsm6_apres_assets_Array);
	return pos;
}

static ssize_t show_mute_left_right(struct class *class,
				    struct class_attribute *attr, char *buf)
{
	ssize_t ret = 0;

	ret =
	    sprintf(buf, "echo l/r/s/c to /sys/class/amaudio/mute_left_right\n"
	    "file to mute left or right channel\n1: mute left channel\n"
	    "0: mute right channel\nmute_left_right: %d\n", mute_left_right);

	return ret;
}

static ssize_t store_mute_left_right(struct class *class,
				     struct class_attribute *attr,
				     const char *buf, size_t count)
{
	switch (buf[0]) {
	case '1':
		audio_mute_left_right(1);
		break;

	case '0':
		audio_mute_left_right(0);
		break;

	default:
		pr_info("unknown command!\n");
	}

	return count;
}

static ssize_t show_mute_unmute(struct class *class,
				struct class_attribute *attr, char *buf)
{
	ssize_t ret = 0;

	ret =
	    sprintf(buf, " 1: mute, 0:unmute: mute_unmute:%d,\n", mute_unmute);

	return ret;
}

static ssize_t store_mute_unmute(struct class *class,
				 struct class_attribute *attr, const char *buf,
				 size_t count)
{
	switch (buf[0]) {
	case '1':
		audio_i2s_mute();
		break;

	case '0':
		audio_i2s_unmute();
		break;

	default:
		pr_info("unknown command!\n");
	}

	return count;
}
static ssize_t dts_enable_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	unsigned int val;

	val = aml_read_aobus(0x228);
	val = (val>>14)&1;

	return sprintf(buf, "0x%x\n", val);
}
static ssize_t dolby_enable_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	unsigned int val;

	val = aml_read_aobus(0x228);
	val = (val>>16)&1;

	return sprintf(buf, "0x%x\n", val);
}
static struct class_attribute amaudio_attrs[] = {
	__ATTR(audio_channels_mask, 0664,
	       show_audio_channels_mask, store_audio_channels_mask),
	__ATTR(dac_mute_const, 0644,
	       dac_mute_const_show, dac_mute_const_store),
	__ATTR(record_type, 0644,
	       record_type_show, record_type_store),
	__ATTR(debug, 0664,
	       show_debug, store_debug),
	__ATTR(mute_left_right, 0644,
	       show_mute_left_right, store_mute_left_right),
	__ATTR(mute_unmute, 0644,
	       show_mute_unmute, store_mute_unmute),
	__ATTR_RO(output_enable),
	__ATTR_RO(dts_enable),
	__ATTR_RO(dolby_enable),
	__ATTR_NULL,
};

static struct class amaudio_class = {
	.name = AMAUDIO_CLASS_NAME,
	.class_attrs = amaudio_attrs,
};

static int __init amaudio_init(void)
{
	int ret = 0;
	int i = 0;
	struct amaudio_port_t *ap;

	pr_info("amaudio: driver %s init!\n", AMAUDIO_DRIVER_NAME);

	ret =
	    register_chrdev(AMAUDIO_MAJOR, AMAUDIO_DRIVER_NAME, &amaudio_fops);
	if (ret < 0) {
		pr_err("Can't register %s divece ", AMAUDIO_DRIVER_NAME);
		ret = -ENODEV;
		goto err;
	}

	amaudio_classp = class_create(THIS_MODULE, AMAUDIO_DEVICE_NAME);
	if (IS_ERR(amaudio_classp)) {
		ret = PTR_ERR(amaudio_classp);
		goto err1;
	}

	ap = &amaudio_ports[0];
	for (i = 0; i < AMAUDIO_DEVICE_COUNT; ap++, i++) {
		ap->dev = device_create(amaudio_classp, NULL,
					MKDEV(AMAUDIO_MAJOR, i), NULL,
					amaudio_ports[i].name);
		if (IS_ERR(ap->dev)) {
			pr_err
			    ("amaudio: failed to create amaudio device node\n");
			goto err2;
		}
	}

	ret = class_register(&amaudio_class);
	if (ret) {
		pr_err("amaudio class create fail.\n");
		goto err3;
	}

	amaudio_dev =
	    device_create(&amaudio_class, NULL, MKDEV(AMAUDIO_MAJOR, 10), NULL,
			  AMAUDIO_CLASS_NAME);
	if (IS_ERR(amaudio_dev)) {
		pr_err("amaudio creat device fail.\n");
		goto err4;
	}

	pr_info("%s - amaudio: driver %s succuess!\n",
		__func__, AMAUDIO_DRIVER_NAME);
	return 0;

 err4:
	device_destroy(&amaudio_class, MKDEV(AMAUDIO_MAJOR, 10));
 err3:
	class_unregister(&amaudio_class);
 err2:
	for (ap = &amaudio_ports[0], i = 0; i < AMAUDIO_DEVICE_COUNT; ap++, i++)
		device_destroy(amaudio_classp, MKDEV(AMAUDIO_MAJOR, i));
 err1:
	class_destroy(amaudio_classp);
 err:
	unregister_chrdev(AMAUDIO_MAJOR, AMAUDIO_DRIVER_NAME);
	return ret;

}

static void __exit amaudio_exit(void)
{
	int i = 0;
	struct amaudio_port_t *ap;

	device_destroy(&amaudio_class, MKDEV(AMAUDIO_MAJOR, 10));
	class_unregister(&amaudio_class);
	for (ap = &amaudio_ports[0], i = 0; i < AMAUDIO_DEVICE_COUNT; ap++, i++)
		device_destroy(amaudio_classp, MKDEV(AMAUDIO_MAJOR, i));
	class_destroy(amaudio_classp);
	unregister_chrdev(AMAUDIO_MAJOR, AMAUDIO_DRIVER_NAME);
}

module_init(amaudio_init);
module_exit(amaudio_exit);

MODULE_DESCRIPTION("AMLOGIC Audio Control Interface driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Amlogic Inc.");
MODULE_VERSION("1.0.0");
