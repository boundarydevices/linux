/*
 * drivers/amlogic/reg_access/reg_access.c
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
#include <linux/device.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/debugfs.h>
#include <linux/io.h>
#include <linux/uaccess.h>
static struct dentry *debugfs_root;
/* amlogic debug device*/
struct aml_ddev {
	unsigned int cached_reg_addr;
	unsigned int size;
	int (*debugfs_reg_access)(struct aml_ddev *indio_dev,
				 unsigned int reg, unsigned int writeval,
				 unsigned int *readval);
};
int aml_reg_access(struct aml_ddev *indio_dev,
				 unsigned int reg, unsigned int writeval,
				 unsigned int *readval)
{
	void __iomem *vaddr;

	reg = round_down(reg, 0x3);

	vaddr = ioremap(reg, 0x4);

	if (readval)
		*readval = readl(vaddr);
	else
		writel(writeval, vaddr);
	iounmap(vaddr);
	return 0;
}
static struct aml_ddev aml_dev;
static ssize_t paddr_read_file(struct file *file, char __user *userbuf,
				 size_t count, loff_t *ppos)
{
	struct aml_ddev *indio_dev = file->private_data;
	char buf[80];
	unsigned int val = 0;
	ssize_t len;
	int ret;

	ret = indio_dev->debugfs_reg_access(indio_dev,
						  indio_dev->cached_reg_addr,
						  0, &val);
	if (ret)
		pr_err("%s: read failed\n", __func__);

	len = snprintf(buf, sizeof(buf), "[0x%x] = 0x%X\n",
		       indio_dev->cached_reg_addr, val);

	return simple_read_from_buffer(userbuf, count, ppos, buf, len);

}

static ssize_t paddr_write_file(struct file *file, const char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	struct aml_ddev *indio_dev = file->private_data;
	unsigned int reg, val;
	char buf[80];
	int ret;

	count = min_t(size_t, count, (sizeof(buf)-1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;

	ret = sscanf(buf, "%x %x", &reg, &val);

	switch (ret) {
	case 1:
		indio_dev->cached_reg_addr = reg;
		break;
	case 2:
		indio_dev->cached_reg_addr = reg;
		ret = indio_dev->debugfs_reg_access(indio_dev, reg,
							  val, NULL);
		if (ret) {
			pr_err("%s: write failed\n", __func__);
			return ret;
		}
		break;
	default:
		return -EINVAL;
	}

	return count;
}

static const struct file_operations paddr_file_ops = {
	.open		= simple_open,
	.read		= paddr_read_file,
	.write		= paddr_write_file,
};


static ssize_t dump_write_file(struct file *file, const char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct aml_ddev *indio_dev = s->private;
	unsigned int reg, val;
	char buf[80];
	int ret;

	count = min_t(size_t, count, (sizeof(buf)-1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;

	ret = sscanf(buf, "%x %i", &reg, &val);
	switch (ret) {
	case 2:
		indio_dev->cached_reg_addr = reg;
		indio_dev->size = val;
		break;
	default:
		return -EINVAL;
	}

	return count;
}

static int dump_show(struct seq_file *s, void *what)
{
	unsigned int  i = 0, val;
	int ret;
	struct aml_ddev *indio_dev = s->private;

	for (i = 0; i < indio_dev->size ; i++) {
		ret = indio_dev->debugfs_reg_access(indio_dev,
				  indio_dev->cached_reg_addr,
				  0, &val);
		if (ret)
			pr_err("%s: read failed\n", __func__);

		seq_printf(s, "[0x%x] = 0x%X\n",
			   indio_dev->cached_reg_addr, val);
		indio_dev->cached_reg_addr += 4;
	}
	return 0;
}

static int dump_open(struct inode *inode, struct file *file)
{
	return single_open(file, dump_show, inode->i_private);
}


static const struct file_operations dump_file_ops = {
	.open		= dump_open,
	.read		= seq_read,
	.write		= dump_write_file,
	.llseek		= seq_lseek,
	.release	= single_release,
};
static int __init aml_debug_init(void)
{
	debugfs_root = debugfs_create_dir("aml_reg", NULL);
	if (IS_ERR(debugfs_root) || !debugfs_root) {
		pr_warn("failed to create debugfs directory\n");
		debugfs_root = NULL;
		return -1;
	}
	aml_dev.debugfs_reg_access = aml_reg_access;

	debugfs_create_file("paddr", S_IFREG | 0440,
			    debugfs_root, &aml_dev, &paddr_file_ops);
	debugfs_create_file("dump", S_IFREG | 0440,
			    debugfs_root, &aml_dev, &dump_file_ops);
	return 0;
}

static void __exit aml_debug_exit(void)
{
}


module_init(aml_debug_init);
module_exit(aml_debug_exit);

MODULE_DESCRIPTION("Amlogic debug module");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xing Xu <xing.xu@amlogic.com>");

