/*
 * drivers/amlogic/media/vin/tvin/hdmirx/hdmirx_esm_mem.c
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

#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mman.h>
#include <linux/random.h>
#include <linux/init.h>
#include <linux/raw.h>
#include <linux/tty.h>
#include <linux/capability.h>
#include <linux/ptrace.h>
#include <linux/device.h>
#include <linux/highmem.h>
#include <linux/backing-dev.h>
#include <linux/splice.h>
#include <linux/pfn.h>
#include <linux/export.h>
#include <linux/io.h>
#include <linux/aio.h>

#include <linux/uaccess.h>

#include "hdcp_main.h"
#include "hdmirx_drv.h"

#if 0
#define DEVPORT_MINOR	4
#define ESMMEM_MAJOR		64
static inline unsigned long size_inside_page(unsigned long start,
					     unsigned long size)
{
	unsigned long sz;

	sz = PAGE_SIZE - (start & (PAGE_SIZE - 1));

	return min(sz, size);
}

void __weak unxlate_dev_mem_ptr(unsigned long phys, void *addr)
{
}

/*
 * This function reads the *physical* memory.
 * The f_pos points directly to thememory location.
 */
static ssize_t read_esmmem(struct file *file, char __user *buf,
			size_t count, loff_t *ppos)
{
	/* phys_addr_t p = *ppos; */
	static int flag = 1;

	phys_addr_t p = (loff_t)esm_data_base_addr;
	ssize_t read, sz;
	char *ptr;

	if (flag == 1) {
		rx_pr("phys addr = %d\n", p);
		flag = 0;
	}
	read = 0;

	while (count > 0) {
		unsigned long remaining;
		int my_map = 0;

		sz = size_inside_page(p, count);

		/*
		 * On ia64 if a page has been mapped somewhere as uncached, then
		 * it must also be accessed uncached by the kernel or data
		 * corruption may occur.
		 */
		ptr = xlate_dev_mem_ptr(p);
		if (!ptr) {
			ptr = ioremap(p, sz);
			my_map = 1;
		}
		if (!ptr)
			return -EFAULT;

		remaining = copy_to_user(buf, ptr, sz);
		if (!my_map) {
			unxlate_dev_mem_ptr(p, ptr);
		} else {
			iounmap(ptr);
			my_map = 0;
			ptr = NULL;
		}
		if (remaining)
			return -EFAULT;

		buf += sz;
		p += sz;
		count -= sz;
		read += sz;
	}

	/* ppos += read; */
	esm_data_base_addr += read;
	return read;
}

/*
 * The memory devices use the full 32/64 bits of the offset, and so we
 * cannot check against negative addresses: they are ok. The return
 * value is weird,though, in that case (0).
 * also note that seeking relative to the "end of file" isn't supported:
 * it has no meaning, so it returns -EINVAL.
 */
static loff_t memory_lseek(struct file *file, loff_t offset, int orig)
{
	loff_t ret;

	mutex_lock(&file_inode(file)->i_mutex);
	switch (orig) {
	case SEEK_CUR:
		offset += file->f_pos;
	case SEEK_SET:
		/* to avoid userland mistaking f_pos=-9 as -EBADF=-9 */
		if (IS_ERR_VALUE((unsigned long long)offset)) {
			ret = -EOVERFLOW;
			break;
		}
		file->f_pos = offset;
		ret = file->f_pos;
		force_successful_syscall_return();
		break;
	default:
		ret = -EINVAL;
	}
	mutex_unlock(&file_inode(file)->i_mutex);
	return ret;
}
static const struct file_operations esmmem_fops = {
	.llseek		= memory_lseek,
	.read		= read_esmmem,
};

static const struct memdev {
	const char *name;
	umode_t mode;
	const struct file_operations *fops;
	struct backing_dev_info *dev_info;
} esmdevlist[] = {
	 [1] = { "esmmem", 0, &esmmem_fops, &directly_mappable_cdev_bdi },
};

static int memory_open(struct inode *inode, struct file *filp)
{
	int minor;
	const struct memdev *dev;

	minor = iminor(inode);
	if (minor >= ARRAY_SIZE(esmdevlist))
		return -ENXIO;

	dev = &esmdevlist[minor];
	if (!dev->fops)
		return -ENXIO;

	filp->f_op = dev->fops;
	if (dev->dev_info)
		filp->f_mapping->backing_dev_info = dev->dev_info;

	/* Is /dev/mem or /dev/kmem ? */
	if (dev->dev_info == &directly_mappable_cdev_bdi)
		filp->f_mode |= FMODE_UNSIGNED_OFFSET;

	if (dev->fops->open)
		return dev->fops->open(inode, filp);

	return 0;
}

static const struct file_operations memory_fops = {
	.open = memory_open,
	.llseek = noop_llseek,
};

static char *mem_devnode(struct device *dev, umode_t *mode)
{
	if (mode && esmdevlist[MINOR(dev->devt)].mode)
		*mode = esmdevlist[MINOR(dev->devt)].mode;
	return NULL;
}

static struct class *mem_class;

int hdmirx_dev_init(void)
{
	int minor;

	pr_info("esmmem Initializing...\n");
	if (register_chrdev(ESMMEM_MAJOR, "esmmem", &memory_fops))
		rx_pr("unable to get major %d for esm memory devs\n",
			ESMMEM_MAJOR);

	mem_class = class_create(THIS_MODULE, "esmmem");
	if (IS_ERR(mem_class))
		return PTR_ERR(mem_class);
	mem_class->devnode = mem_devnode;
	for (minor = 1; minor < ARRAY_SIZE(esmdevlist); minor++) {
		if (!esmdevlist[minor].name)
			continue;
		/*
		 * Create /dev/port?
		 */
		if ((minor == DEVPORT_MINOR) && !arch_has_dev_port())
			continue;
		device_create(mem_class, NULL,
				MKDEV(ESMMEM_MAJOR, minor),
			      NULL, esmdevlist[minor].name);
	}

	return 0;
}
#endif
/* fs_initcall(hdmirx_dev_init); */
