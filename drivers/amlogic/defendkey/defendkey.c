/*
 * drivers/amlogic/defendkey/defendkey.c
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
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/scatterlist.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/dma-contiguous.h>
#include <linux/dma-mapping.h>
#include <linux/cma.h>
#include <linux/io-mapping.h>
#include <linux/sysfs.h>
#include <asm/cacheflush.h>
#include "securekey.h"
#include <linux/memblock.h>
#include <linux/random.h>

#define DEFENDKEY_DEVICE_NAME	"defendkey"
#define DEFENDKEY_CLASS_NAME "defendkey"
void __iomem *mem_base_virt;
unsigned long mem_size;
//unsigned long random_virt;

struct defendkey_mem {
	unsigned long	base;
	unsigned long	size;
};

struct defendkey_mem defendkey_rmem;

#define CMD_SECURE_CHECK _IO('d', 0x01)
#define CMD_DECRYPT_DTB  _IO('d', 0x02)
#define DEFENDKEY_LIMIT_ADDR	0x0F000000

enum e_defendkey_type {
	e_upgrade_check = 0,
	e_decrypt_dtb = 1,
	e_decrypt_dtb_success = 2,
};

enum ret_defendkey {
	ret_fail = 0,
	ret_success = 1,
	ret_error = -1,
};

struct defendkey_dev_t {
	struct cdev cdev;
	dev_t  devno;
};

static struct defendkey_dev_t *defendkey_devp;
static enum e_defendkey_type decrypt_dtb;

static int defendkey_open(struct inode *inode, struct file *file)
{
	struct defendkey_dev_t *devp;

	devp = container_of(inode->i_cdev, struct defendkey_dev_t, cdev);
	file->private_data = devp;
	return 0;
}
static int defendkey_release(struct inode *inode, struct file *file)
{
	return 0;
}

static loff_t defendkey_llseek(struct file *filp, loff_t off, int whence)
{
	return 0;
}

static long defendkey_unlocked_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	unsigned long ret = 0;

	switch (cmd) {
	case CMD_SECURE_CHECK:
		ret = aml_is_secure_set();
		break;
	case CMD_DECRYPT_DTB:
		if (arg == 1)
			decrypt_dtb = 1;
		else if (arg == 0)
			decrypt_dtb = 0;
		else {
			return -EINVAL;
			pr_info("set defendkey decrypt_dtb fail,invalid value\n");
		}
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long defendkey_compat_ioctl(struct file *filp,
			      unsigned int cmd, unsigned long args)
{
	unsigned long ret;

	args = (unsigned long)compat_ptr(args);
	ret = defendkey_unlocked_ioctl(filp, cmd, args);

	return ret;
}
#endif


static ssize_t defendkey_read(struct file *file,
	char __user *buf, size_t count, loff_t *ppos)
{
	ssize_t ret_value = ret_error;
	int ret = -EINVAL;
	unsigned long mem_base_phy, check_offset;

	if (decrypt_dtb == e_decrypt_dtb_success) {
		mem_base_phy = virt_to_phys(mem_base_virt);

		check_offset = aml_sec_boot_check(AML_D_Q_IMG_SIG_HDR_SIZE,
			mem_base_phy, mem_size, 0);
		if (AML_D_Q_IMG_SIG_HDR_SIZE == (check_offset & 0xFFFF))
			check_offset = (check_offset >> 16) & 0xFFFF;
		else
			check_offset = 0;

		if (mem_size < count) {
			pr_err("%s:data size overflow!\n", __func__);
			ret_value = ret_fail;
			goto exit;
		}

		ret = copy_to_user((void __user *)buf,
			(const void *)(mem_base_virt + check_offset), count);
		if (ret) {
			pr_err("%s:copy_to_user fail! ret:%d\n",
				__func__, ret);
			ret_value = ret_fail;
		} else {
			pr_info("%s: copy data to user successfully!\n",
				__func__);
			ret_value = ret_success;
		}

		decrypt_dtb = e_upgrade_check;
	} else
		ret_value = ret_error;

exit:
	return ret_value;
}

static ssize_t defendkey_write(struct file *file,
	const char __user *buf, size_t count, loff_t *ppos)
{
	ssize_t ret_value = ret_error;
	int ret = -EINVAL;

	unsigned long mem_base_phy, copy_base, copy_size;
	uint64_t option = 0, random = 0, option_random = 0;
	int i;
	struct cpumask task_cpumask;

	cpumask_copy(&task_cpumask, &current->cpus_allowed);
	set_cpus_allowed_ptr(current, cpumask_of(0));

	mem_base_phy = defendkey_rmem.base;
	mem_base_virt = phys_to_virt(mem_base_phy);

	if (!mem_base_phy || !mem_size) {
		pr_err("bad secure check memory!\nmem_base_phy:%lx,size:%lx\n",
			mem_base_phy, mem_size);
		ret =  -EFAULT;
		goto exit;
	}
	pr_info("defendkey: mem_base_phy:%lx mem_size:%lx mem_base_virt:%p\n",
		mem_base_phy, mem_size, mem_base_virt);

	random = get_random_long();

#ifdef	CONFIG_ARM64_A32
	option_random = (random << 8);
#endif

	option_random |= (random << 32);

	for (i = 0; i <= count/mem_size; i++) {
		copy_size = mem_size;
		copy_base = (unsigned long)buf+i*mem_size;
		if ((i+1)*mem_size > count)
			copy_size = count - mem_size*i;
		ret = copy_from_user(mem_base_virt,
			(const void __user *)copy_base, copy_size);
		if (ret) {
			pr_err("defendkey:copy_from_user fail! ret:%d\n", ret);
			ret =  -EFAULT;
			goto exit;
		}
		//__dma_flush_area((const void *)mem_base_virt, copy_size);

		if (i == 0) {
			option = 1;
			if (count <= mem_size) {
				/*just transfer data to BL31 one time*/
				option = 1|2|4;
			}
			option |= option_random;
		} else if ((i > 0) && (i < (count/mem_size))) {
			option = 2|option_random;
			if ((count%mem_size == 0) &&
				(i == (count/mem_size - 1)))
				option = 4|option_random;
		} else if (i == (count/mem_size)) {
			if (count%mem_size != 0)
				option = 4|option_random;
			else
				break;
		}
		pr_info("defendkey:%d: copy_size:0x%lx, option:0x%llx\n",
			__LINE__, copy_size, option);
		pr_info("decrypt_dtb: %d\n", decrypt_dtb);
		if (e_decrypt_dtb == decrypt_dtb)
			ret = aml_sec_boot_check(AML_D_P_IMG_DECRYPT,
				mem_base_phy, copy_size, 0); /*option: 0: dtb*/
		else if (e_upgrade_check == decrypt_dtb)
			ret = aml_sec_boot_check(AML_D_P_UPGRADE_CHECK,
				mem_base_phy, copy_size, option);
		else {
			ret = -1;
			pr_err("%s: decrypt_dtb: %d,", __func__, decrypt_dtb);
			pr_err("not for upgrade check and decrypt_dtb\n");
		}
		if (ret) {
			ret_value = ret_fail;
			pr_err("defendkey: aml_sec_boot_check error,");
			pr_err(" %s:%d: ret %d %s\n", __func__, __LINE__,
				ret, decrypt_dtb ? ": for decrypt dtb":"\n");
			goto exit;
		}
	}

	if (!ret) {
		if (decrypt_dtb) {
			decrypt_dtb = e_decrypt_dtb_success;
			pr_info("defendkey: aml_sec_boot_check decrypt dtb ok!\n");
		} else {
			pr_info("defendkey: aml_sec_boot_check ok!\n");
		}
		ret_value = ret_success;
	}
exit:
	set_cpus_allowed_ptr(current, &task_cpumask);
	return ret_value;
}

static ssize_t version_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{

	return sprintf(buf, "version:2.00\n");
}

static ssize_t secure_check_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	ssize_t n = 0;
	int ret;

	ret = aml_is_secure_set();
	if (ret < 0)
		n = sprintf(buf, "fail");
	else if (ret == 0)
		n = sprintf(buf, "raw");
	else if (ret > 0)
		n = sprintf(buf, "encrypt");

	return n;
}

static ssize_t secure_verify_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t decrypt_dtb_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	/* show lock state. */
	return sprintf(buf, "%d\n", decrypt_dtb);
}

static ssize_t decrypt_dtb_store(struct class *cla,
	struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned int len;
	/* check '\n' and del */
	if (buf[count - 1] == '\n')
		len = count - 1;
	else
		len = count;

	if (!strncmp(buf, "1", len)) {
		decrypt_dtb = 1;
	} else if (!strncmp(buf, "0", len))
		decrypt_dtb = 0;
	else {
		pr_info("set defendkey decrypt_dtb fail,invalid value\n");
	}

	return count;
}

static const struct file_operations defendkey_fops = {
	.owner      = THIS_MODULE,
	.llseek     = defendkey_llseek,
	.open       = defendkey_open,
	.release    = defendkey_release,
	.read       = defendkey_read,
	.write      = defendkey_write,
	.unlocked_ioctl      = defendkey_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = defendkey_compat_ioctl,
#endif
};

static struct class_attribute defendkey_class_attrs[] = {
	__ATTR_RO(version),
	__ATTR_RO(secure_check),
	__ATTR_RO(secure_verify),
	__ATTR(decrypt_dtb, (0700), decrypt_dtb_show, decrypt_dtb_store),
	__ATTR_NULL
};
static struct class defendkey_class = {
	.name = DEFENDKEY_CLASS_NAME,
	.class_attrs = defendkey_class_attrs,
};

static int __init early_defendkey_para(char *buf)
{
	int ret;

	if (!buf)
		return -EINVAL;

	ret = sscanf(buf, "%lx,%lx",
		&defendkey_rmem.base, &defendkey_rmem.size);
	if (ret != 2) {
		pr_err("invalid boot args \"defendkey\"\n");
		return -EINVAL;
	}

	if (defendkey_rmem.base > DEFENDKEY_LIMIT_ADDR) {
		pr_err("defendkey reserved memory base overflow!\n");
		return -EINVAL;
	}

	pr_info("%s, base:%lx, size:%lx\n",
		__func__, defendkey_rmem.base, defendkey_rmem.size);

	ret = memblock_reserve(defendkey_rmem.base,
		PAGE_ALIGN(defendkey_rmem.size));
	if (ret < 0) {
		pr_info("reserve memblock %lx - %lx failed\n",
			defendkey_rmem.base,
			defendkey_rmem.base + PAGE_ALIGN(defendkey_rmem.size));
		return -EINVAL;
	}

	return 0;
}
early_param("defendkey", early_defendkey_para);

static int aml_defendkey_probe(struct platform_device *pdev)
{
	int ret =  -1;
	u64 val64;

	struct device *devp;

	defendkey_devp = devm_kzalloc(&(pdev->dev),
		sizeof(struct defendkey_dev_t), GFP_KERNEL);
	if (!defendkey_devp) {
		ret = -ENOMEM;
		dev_err(&pdev->dev, "defendkey: failed to allocate memory\n ");
		goto out;
	}

	ret = of_property_read_u64(pdev->dev.of_node, "mem_size", &val64);
	if (ret) {
		dev_err(&pdev->dev, "please config mem_size in dts\n");
		goto error1;
	}

	mem_size = val64;
	if (mem_size > defendkey_rmem.size) {
		dev_err(&pdev->dev, "Reserved memory is not enough!\n");
		ret = -EINVAL;
		goto error1;
	}

	ret = alloc_chrdev_region(&defendkey_devp->devno, 0, 1,
		DEFENDKEY_DEVICE_NAME);
	if (ret < 0) {
		dev_err(&pdev->dev, "defendkey: failed to allocate major number\n ");
		ret = -ENODEV;
		goto error1;
	}
	dev_info(&pdev->dev, "defendkey_devno:%x\n", defendkey_devp->devno);
	ret = class_register(&defendkey_class);
	if (ret) {
		dev_err(&pdev->dev, "defendkey: failed to register class\n ");
		goto error2;
	}

	/* connect the file operations with cdev */
	cdev_init(&defendkey_devp->cdev, &defendkey_fops);
	defendkey_devp->cdev.owner = THIS_MODULE;
	/* connect the major/minor number to the cdev */
	ret = cdev_add(&defendkey_devp->cdev, defendkey_devp->devno, 1);
	if (ret) {
		dev_err(&pdev->dev, "defendkey: failed to add device\n");
		goto error3;
	}
	devp = device_create(&defendkey_class, NULL, defendkey_devp->devno,
		NULL, DEFENDKEY_DEVICE_NAME);
	if (IS_ERR(devp)) {
		dev_err(&pdev->dev, "defendkey: failed to create device node\n");
		ret = PTR_ERR(devp);
		goto error4;
	}


	dev_info(&pdev->dev, "defendkey: device %s created ok\n",
			DEFENDKEY_DEVICE_NAME);
	return 0;

error4:
	cdev_del(&defendkey_devp->cdev);
error3:
	class_unregister(&defendkey_class);
error2:
	unregister_chrdev_region(defendkey_devp->devno, 1);
error1:
	devm_kfree(&(pdev->dev), defendkey_devp);
out:
	return ret;
}

static int aml_defendkey_remove(struct platform_device *pdev)
{
	unregister_chrdev_region(defendkey_devp->devno, 1);
	device_destroy(&defendkey_class, defendkey_devp->devno);
	cdev_del(&defendkey_devp->cdev);
	class_unregister(&defendkey_class);
	return 0;
}

static const struct of_device_id meson_defendkey_dt_match[] = {
	{	.compatible = "amlogic, defendkey",
	},
	{},
};

static struct platform_driver aml_defendkey_driver = {
	.probe = aml_defendkey_probe,
	.remove = aml_defendkey_remove,
	.driver = {
		.name = DEFENDKEY_DEVICE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = meson_defendkey_dt_match,
	},
};

module_platform_driver(aml_defendkey_driver);


MODULE_DESCRIPTION("AMLOGIC defendkey driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("benlong zhou <benlong.zhou@amlogic.com>");



