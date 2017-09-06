/*
 * drivers/amlogic/efuse/efuse.c
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

#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/amlogic/secmon.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include "efuse.h"
#include "efuse_regs.h"

#define EFUSE_MODULE_NAME   "efuse"
#define EFUSE_DRIVER_NAME	"efuse"
#define EFUSE_DEVICE_NAME   "efuse"
#define EFUSE_CLASS_NAME    "efuse"
#define EFUSE_IS_OPEN           (0x01)

struct efuse_dev_t {
	struct cdev cdev;
	unsigned int flags;
};

static struct efuse_dev_t *efuse_devp;
/* static struct class *efuse_clsp; */
static dev_t efuse_devno;
void __iomem *efuse_base;
struct clk *efuse_clk;

static int efuse_open(struct inode *inode, struct file *file)
{
	int ret = 0;
	struct efuse_dev_t *devp;

	devp = container_of(inode->i_cdev, struct efuse_dev_t, cdev);
	file->private_data = devp;

	return ret;
}

static int efuse_release(struct inode *inode, struct file *file)
{
	int ret = 0;
	struct efuse_dev_t *devp;
	unsigned long efuse_status = 0;

	devp = file->private_data;
	efuse_status &= ~EFUSE_IS_OPEN;
	return ret;
}

loff_t efuse_llseek(struct file *filp, loff_t off, int whence)
{
	loff_t newpos;

	switch (whence) {
	case 0: /* SEEK_SET */
		newpos = off;
		break;

	case 1: /* SEEK_CUR */
		newpos = filp->f_pos + off;
		break;

	case 2: /* SEEK_END */
		newpos = EFUSE_BYTES + off;
		break;

	default: /* can't happen */
		return -EINVAL;
	}

	if (newpos < 0)
		return -EINVAL;
	filp->f_pos = newpos;
		return newpos;
}

static long efuse_unlocked_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	void __user		*argp = (void __user *)arg;
	struct efuseinfo_item_t info;
	int			ret;


	switch (cmd) {
	case EFUSE_INFO_GET:
		ret = copy_from_user(&info, argp, sizeof(info));
		if (ret != 0) {
			pr_err("%s:%d,copy_from_user fail\n",
				__func__, __LINE__);
			return ret;
		}

		if (efuse_getinfo_byTitle(info.title, &info) < 0)
			return  -EFAULT;

		ret = copy_to_user(argp, &info, sizeof(info));
		if (ret != 0) {
			pr_err("%s:%d,copy_to_user fail\n",
				__func__, __LINE__);
			return ret;
		}

		break;

	default:
		return -ENOTTY;
	}
	return 0;
}

static ssize_t efuse_read(struct file *file, char __user *buf,
	size_t count, loff_t *ppos)
{
	int ret;
	int local_count = 0;
	unsigned char *local_buf = kcalloc(count, sizeof(char), GFP_KERNEL);

	if (!local_buf) {
		/* pr_info("memory not enough\n"); */
		return -ENOMEM;
	}

	local_count = efuse_read_item(local_buf, count, ppos);
	if (local_count < 0) {
		ret =  -EFAULT;
		goto error_exit;
	}

	if (copy_to_user((void *)buf, (void *)local_buf, local_count)) {
		ret =  -EFAULT;
		goto error_exit;
	}
	ret = local_count;

error_exit:
	/*if (local_buf)*/
		kfree(local_buf);
	return ret;
}

static ssize_t efuse_write(struct file *file,
	const char __user *buf, size_t count, loff_t *ppos)
{
	unsigned int  pos = (unsigned int)*ppos;
	int ret, size;
	unsigned char *contents = NULL;

	if (pos >= EFUSE_BYTES)
		return 0;       /* Past EOF */
	if (count > EFUSE_BYTES - pos)
		count = EFUSE_BYTES - pos;
	if (count > EFUSE_BYTES)
		return -EFAULT;

	ret = check_if_efused(pos, count);
	if (ret) {
		pr_info("check if has been efused failed\n");
		if (ret == 1)
			return -EROFS;
		else if (ret < 0)
			return ret;
	}

	contents = kzalloc(sizeof(unsigned char)*EFUSE_BYTES, GFP_KERNEL);
	if (!contents) {
		/* pr_info("memory not enough\n"); */
		return -ENOMEM;
	}
	size = sizeof(contents);
	memset(contents, 0, size);
	if (copy_from_user(contents, buf, count)) {
		/*if (contents)*/
		kfree(contents);
		return -EFAULT;
	}

	if (efuse_write_item(contents, count, ppos) < 0) {
		kfree(contents);
		return -EFAULT;
	}

	kfree(contents);
	return count;
}

static const struct file_operations efuse_fops = {
	.owner      = THIS_MODULE,
	.llseek     = efuse_llseek,
	.open       = efuse_open,
	.release    = efuse_release,
	.read       = efuse_read,
	.write      = efuse_write,
	.unlocked_ioctl      = efuse_unlocked_ioctl,
};

/* Sysfs Files */
ssize_t efuse_attr_store(char *name, const char *buf, size_t count)
{
#ifndef EFUSE_READ_ONLY
	char *local_buf;
	ssize_t ret;
	int i;
	const unsigned char *s;
	struct efuseinfo_item_t info;
	unsigned int uint_val;

	if (efuse_getinfo_byTitle(name, &info) < 0) {
		pr_err("%s is not found\n", name);
		return -EFAULT;
	}

	if (count < info.data_len) {
		pr_info("%s:item: %s: size: %d too few arguments to store\n",
			__func__, name, info.data_len);
		count = info.data_len + 1;
	}
	if (check_if_efused(info.offset, info.data_len)) {

		pr_err("%s error!data_verify failed!\n", __func__);
		return -1;
	}

	local_buf = kzalloc(sizeof(char)*(count), GFP_KERNEL);

	memcpy(local_buf, buf, (strlen(buf) < info.data_len)
		? strlen(buf):count);

	s = local_buf;

	strim(local_buf);

	/* only support separated with whitespace or colon
	 * eg. A1:B2:C3 or D4 E5 F6 in hexadecimal
	 */
	if ((strstr(s, ":") != NULL) || (strstr(s, " ") != NULL)) {
		for (i = 0; i < info.data_len; i++) {
			while ((!strncmp(s, ":", 1)) ||
				!strncmp(s, " ", 1))
			s++;
			if (!*s) {
				local_buf[i] = 0;
				continue;
			}
				ret = sscanf(s, "%x", &uint_val);
				if (ret < 0) {
					pr_err("ERROR: efuse get user data fail!\n");
					goto error_exit;
				} else {
					local_buf[i] = uint_val;
					s += 2;
				}

				pr_debug("local_buf[%d]: 0x%x\n",
					i, local_buf[i]);
			}
	} else {/* only support separated with whitespace */
		for (i = 0; i < info.data_len; i++) {
			uint_val = 0;
			if (!*s) {
				local_buf[i] = 0;
				continue;
			}
			ret = sscanf(s, "%x", &uint_val);

			if (ret < 0) {
				pr_err("ERROR: efuse get user data fail!\n");
				goto error_exit;
			} else {
				local_buf[i] = uint_val;
				s += 2;
			}
			pr_debug("local_buf[%d]: 0x%x\n",
				i, local_buf[i]);
			}
	}

	ret = efuse_write_item(local_buf, info.data_len,
		(loff_t *)&(info.offset));
	if (ret == -1) {
		pr_err("ERROR: efuse write user data fail!\n");
		goto error_exit;
	}
	if (ret != info.data_len)
		pr_err("ERROR: write %zd byte(s) not %d byte(s) data\n",
			ret, info.data_len);
	pr_info("efuse write %zd data OK\n", ret);

error_exit:
	kfree(local_buf);
	return ret;
#else
	pr_err("no permission to write!!\n");
	return -1;
#endif
}

ssize_t efuse_attr_show(char *name, char *buf)
{
	char *local_buf;
	ssize_t ret;
	int i;
	struct efuseinfo_item_t info;

	if (efuse_getinfo_byTitle(name, &info) < 0) {
		pr_err("%s is not found\n", name);
		return -EFAULT;
	}

	local_buf = kzalloc(sizeof(char)*(info.data_len), GFP_KERNEL);
	memset(local_buf, 0, info.data_len);

	ret = efuse_read_item(local_buf, info.data_len,
		(loff_t *)&(info.offset));
	if (ret == -1) {
		pr_err("ERROR: efuse read user data fail!\n");
		goto error_exit;
	}
	if (ret != info.data_len)
		pr_err("ERROR: read %zd byte(s) not %d byte(s) data\n",
			ret, info.data_len);

	for (i = 0; i < info.data_len; i++)
		buf[i] = local_buf[i];

error_exit:
	kfree(local_buf);
	return ret;
}

#define  DEFINE_EFUE_SHOW_ATTR(name)	\
	static ssize_t show_##name(struct class *cla, \
					  struct class_attribute *attr,	\
						char *buf)	\
	{	\
		ssize_t ret;	\
		int i = 0; \
		\
		ret = efuse_attr_show(#name, buf); \
		if (ret > 0) {\
		pr_info("efuse read data\n"); \
		for (; i < ret; i++) \
			pr_info("%02x%s", buf[i], ((i+1)%16 == 0)?"\n":":"); \
		pr_info("\n"); \
		} \
		return ret; \
	}

DEFINE_EFUE_SHOW_ATTR(version)
DEFINE_EFUE_SHOW_ATTR(mac)
DEFINE_EFUE_SHOW_ATTR(mac_bt)
DEFINE_EFUE_SHOW_ATTR(mac_wifi)
DEFINE_EFUE_SHOW_ATTR(usid)

#define  DEFINE_EFUE_STORE_ATTR(name)	\
	static ssize_t store_##name(struct class *cla, \
					  struct class_attribute *attr,	\
						const char *buf,	\
						size_t count)	\
	{	\
		ssize_t ret;	\
		\
		ret = efuse_attr_store(#name, buf, count); \
		return ret; \
	}
#ifdef CONFIG_AMLOGIC_EFUSE_WRITE_VERSION_PERMIT
DEFINE_EFUE_STORE_ATTR(version)
#endif
//DEFINE_EFUE_STORE_ATTR(mac)
//DEFINE_EFUE_STORE_ATTR(mac_bt)
//DEFINE_EFUE_STORE_ATTR(mac_wifi)
DEFINE_EFUE_STORE_ATTR(usid)

static struct class_attribute efuse_class_attrs[] = {
#ifdef CONFIG_AMLOGIC_EFUSE_WRITE_VERSION_PERMIT
	__ATTR(version, 0700, show_version, store_version),
#else
	__ATTR(version, 0500, show_version, NULL),
#endif
	__ATTR(mac, 0500, show_mac, NULL),
	__ATTR(mac_bt, 0500, show_mac_bt, NULL),
	__ATTR(mac_wifi, 0500, show_mac_wifi, NULL),
	__ATTR(usid, 0700, show_usid, store_usid),
	__ATTR_NULL

};

static struct class efuse_class = {
	.name = EFUSE_CLASS_NAME,
	.class_attrs = efuse_class_attrs,
};

static int efuse_probe(struct platform_device *pdev)
{
	int ret;
	struct device *devp;
	struct device_node *np = pdev->dev.of_node;

	ret = alloc_chrdev_region(&efuse_devno, 0, 1, EFUSE_DEVICE_NAME);
	if (ret < 0) {
		dev_err(&pdev->dev, "efuse: failed to allocate major number\n");
		ret = -ENODEV;
		goto out;
	}

	ret = class_register(&efuse_class);
	if (ret)
		goto error1;

	efuse_devp = kmalloc(sizeof(struct efuse_dev_t), GFP_KERNEL);
	if (!efuse_devp) {
		/* dev_err(&pdev->dev, "efuse: failed to allocate memory\n"); */
		ret = -ENOMEM;
		goto error2;
	}

	efuse_base = of_iomap(pdev->dev.of_node, 0);
	if (!efuse_base) {
		pr_err("%s: Unable to map efuse base\n", __func__);
		ret = -ENXIO;
		goto error3;
	}
	/* connect the file operations with cdev */
	cdev_init(&efuse_devp->cdev, &efuse_fops);
	efuse_devp->cdev.owner = THIS_MODULE;
	/* connect the major/minor number to the cdev */
	ret = cdev_add(&efuse_devp->cdev, efuse_devno, 1);
	if (ret) {
		dev_err(&pdev->dev, "efuse: failed to add device\n");
		goto error4;
	}

	devp = device_create(&efuse_class, NULL, efuse_devno, NULL, "efuse");
	if (IS_ERR(devp)) {
		dev_err(&pdev->dev, "efuse: failed to create device node\n");
		ret = PTR_ERR(devp);
		goto error5;
	}
	dev_dbg(&pdev->dev, "device %s created\n", EFUSE_DEVICE_NAME);

	if (pdev->dev.of_node) {
		of_node_get(np);
#if 0
		ret = of_property_read_u32(np, "plat-pos", &pos);
		if (ret) {
			dev_err(&pdev->dev, "please config plat-pos item\n");
			return -1;
		}
		ret = of_property_read_u32(np, "plat-count", &count);
		if (ret) {
			dev_err(&pdev->dev, "please config plat-count item\n");
			return -1;
		}
		ret = of_property_read_u32(np, "usid-min", &usid_min);
		if (ret) {
			dev_err(&pdev->dev, "please config usid-min item\n");
			return -1;
		}
		ret = of_property_read_u32(np, "usid-max", &usid_max);
		if (ret) {
			dev_err(&pdev->dev, "please config usid-max item\n");
			return -1;
		}
#endif
		/* todo reserved for user id <usid-min ~ usid max> */
	}
	/* open clk gate HHI_GCLK_MPEG0 bit62*/
	efuse_clk = devm_clk_get(&pdev->dev, "efuse_clk");
	if (IS_ERR(efuse_clk))
		dev_err(&pdev->dev, " open efuse clk gate error!!\n");
	else{
		ret = clk_prepare_enable(efuse_clk);
		if (ret)
			dev_err(&pdev->dev, "enable efuse clk gate error!!\n");
	}

	dev_info(&pdev->dev, "probe ok!\n");
	return 0;

error5:
	cdev_del(&efuse_devp->cdev);
error4:
	kfree(efuse_devp);
error3:
	iounmap(efuse_base);
error2:
	/* class_destroy(efuse_clsp); */
	class_unregister(&efuse_class);
error1:
	unregister_chrdev_region(efuse_devno, 1);
out:
	return ret;
}

static int efuse_remove(struct platform_device *pdev)
{
	unregister_chrdev_region(efuse_devno, 1);
	/* device_destroy(efuse_clsp, efuse_devno); */
	device_destroy(&efuse_class, efuse_devno);
	cdev_del(&efuse_devp->cdev);
	kfree(efuse_devp);
	iounmap(efuse_base);
	/* class_destroy(efuse_clsp); */
	class_unregister(&efuse_class);
	return 0;
}

static const struct of_device_id amlogic_efuse_dt_match[] = {
	{	.compatible = "amlogic, efuse",
	},
	{},
};

static struct platform_driver efuse_driver = {
	.probe = efuse_probe,
	.remove = efuse_remove,
	.driver = {
		.name = EFUSE_DEVICE_NAME,
		.of_match_table = amlogic_efuse_dt_match,
	.owner = THIS_MODULE,
	},
};

static int __init efuse_init(void)
{
	int ret = -1;

	ret = platform_driver_register(&efuse_driver);
	if (ret != 0) {
		pr_err("failed to register efuse driver, error %d\n", ret);
		return -ENODEV;
	}

	return ret;
}

static void __exit efuse_exit(void)
{
	platform_driver_unregister(&efuse_driver);
}

module_init(efuse_init);
module_exit(efuse_exit);

MODULE_DESCRIPTION("AMLOGIC eFuse driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yun Cai <yun.cai@amlogic.com>");
