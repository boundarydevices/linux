// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/soc/mediatek/gzvm_drv.h>

static struct gzvm_driver gzvm_drv = {
	.drv_version = {
		.major = GZVM_DRV_MAJOR_VERSION,
		.minor = GZVM_DRV_MINOR_VERSION,
		.sub = 0,
	},
};

/**
 * gzvm_err_to_errno() - Convert geniezone return value to standard errno
 *
 * @err: Return value from geniezone function return
 *
 * Return: Standard errno
 */
int gzvm_err_to_errno(unsigned long err)
{
	int gz_err = (int)err;

	switch (gz_err) {
	case 0:
		return 0;
	case ERR_NO_MEMORY:
		return -ENOMEM;
	case ERR_NOT_SUPPORTED:
		fallthrough;
	case ERR_NOT_IMPLEMENTED:
		return -EOPNOTSUPP;
	case ERR_FAULT:
		return -EFAULT;
	default:
		break;
	}

	return -EINVAL;
}

static int gzvm_dev_open(struct inode *inode, struct file *file)
{
	/*
	 * Reference count to prevent this module is unload without destroying
	 * VM
	 */
	try_module_get(THIS_MODULE);
	return 0;
}

static int gzvm_dev_release(struct inode *inode, struct file *file)
{
	module_put(THIS_MODULE);
	return 0;
}

static const struct file_operations gzvm_chardev_ops = {
	.llseek		= noop_llseek,
	.open		= gzvm_dev_open,
	.release	= gzvm_dev_release,
};

static struct miscdevice gzvm_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = KBUILD_MODNAME,
	.fops = &gzvm_chardev_ops,
};

static int gzvm_drv_probe(struct platform_device *pdev)
{
	if (gzvm_arch_probe(gzvm_drv.drv_version, &gzvm_drv.hyp_version) != 0) {
		dev_err(&pdev->dev, "Not found available conduit\n");
		return -ENODEV;
	}

	pr_debug("Found GenieZone hypervisor version %u.%u.%llu\n",
		 gzvm_drv.hyp_version.major, gzvm_drv.hyp_version.minor,
		 gzvm_drv.hyp_version.sub);

	return misc_register(&gzvm_dev);
}

static void gzvm_drv_remove(struct platform_device *pdev)
{
	misc_deregister(&gzvm_dev);
}

static const struct of_device_id gzvm_of_match[] = {
	{ .compatible = "mediatek,geniezone" },
	{/* sentinel */},
};

static struct platform_driver gzvm_driver = {
	.probe = gzvm_drv_probe,
	.remove = gzvm_drv_remove,
	.driver = {
		.name = KBUILD_MODNAME,
		.of_match_table = gzvm_of_match,
	},
};

module_platform_driver(gzvm_driver);

MODULE_DEVICE_TABLE(of, gzvm_of_match);
MODULE_AUTHOR("MediaTek");
MODULE_DESCRIPTION("GenieZone interface for VMM");
MODULE_LICENSE("GPL");
