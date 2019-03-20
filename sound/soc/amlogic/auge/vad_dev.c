/*
 * sound/soc/amlogic/auge/vad_dev.c
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
#define DEBUG

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/device.h>

#include <linux/uaccess.h>
#include <sound/asound.h>

#include <linux/amlogic/major.h>

#include "vad.h"

#define DRV_NAME "vad"

#define IOCTL_READI_SUSPENDED_FRAMES	_IOR('Z', 0x0, struct snd_xferi)

static bool readable;

bool vad_is_trunk_data_readable(void)
{
	return readable;
}

void vad_set_trunk_data_readable(bool en)
{
	readable = en;
}

static ssize_t readable_show(struct class *cla, struct class_attribute *attr,
			      char *buf)
{
	return sprintf(buf, "%d\n", readable);
}

static struct class_attribute vad_attrs[] = {
	__ATTR_RO(readable),

	__ATTR_NULL
};

static struct class vad_class = {
	.name = DRV_NAME,
	.class_attrs = vad_attrs,
};

static int vad_open(struct inode *inode, struct file *file)
{
	pr_info("%s\n", __func__);

	return 0;
}

static long vad_unlocked_ioctl(
	struct file *file,
	unsigned int cmd,
	unsigned long arg)
{
	if (cmd == IOCTL_READI_SUSPENDED_FRAMES) {
		struct snd_xferi xferi;
		struct snd_xferi __user *_xferi =
			(struct snd_xferi __user *)arg;
		int result;

		if (!vad_is_trunk_data_readable())
			return 0;

		if (put_user(0, &_xferi->result))
			return 0;
		if (copy_from_user(&xferi, _xferi, sizeof(xferi)))
			return 0;

		result = vad_transfer_chunk_data(
					(unsigned long)xferi.buf,
					xferi.frames);

		__put_user(result, &_xferi->result);

		pr_debug("VAD resume trunk data, frames:%lu, result:%d\n",
			xferi.frames,
			result);

		/* if audio data is read, waiting for next time */
		vad_set_trunk_data_readable(false);

		return result;
	}

	return -ENOTTY;
}


#ifdef CONFIG_COMPAT
static long vad_ioctl_compat(
	struct file *file,
	unsigned int cmd,
	unsigned long arg)
{
	pr_info("%s\n", __func__);
	return -ENOIOCTLCMD;
}
#else
#define vad_ioctl_compat    NULL
#endif

static const struct file_operations vad_fops = {
	.owner          = THIS_MODULE,
	.open           = vad_open,
	.unlocked_ioctl = vad_unlocked_ioctl,
	.compat_ioctl   = vad_ioctl_compat,
};

static int __init vad_init(void)
{
	struct device *vad_dev;
	struct class *p_vad_class;
	int ret = 0;

	ret = register_chrdev(VAD_MAJOR, DRV_NAME, &vad_fops);
	if (ret) {
		pr_err("Can't register char devie for " DRV_NAME "\n");
		goto err0;
	}

	ret = class_register(&vad_class);
	if (ret < 0) {
		pr_err("Create vad class failed\n");
		ret = -EEXIST;
		goto err1;
	}

	p_vad_class = &vad_class;
	vad_dev = device_create(p_vad_class,
				NULL, MKDEV(VAD_MAJOR, 0),
				NULL, DRV_NAME);
	if (vad_dev == NULL) {
		ret = -EEXIST;
		goto err2;
	}

	pr_info("Register %s\n", DRV_NAME);

	return 0;
err2:
	class_destroy(p_vad_class);
err1:
	unregister_chrdev(VAD_MAJOR, DRV_NAME);
err0:
	return ret;
}

static void __exit vad_exit(void)
{
	unregister_chrdev(VAD_MAJOR, DRV_NAME);
}

module_init(vad_init);
module_exit(vad_exit);
