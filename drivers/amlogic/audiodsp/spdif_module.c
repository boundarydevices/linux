/*
 * drivers/amlogic/audiodsp/spdif_module.c
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

#define pr_fmt(fmt) "audio_dsp: " fmt

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include <linux/amlogic/amports/dsp_register.h>
#include <linux/amlogic/sound/aiu_regs.h>
#include <linux/amlogic/iomap.h>

#include "spdif_module.h"

#define DEVICE_NAME    "audio_spdif"
static int device_opened;
static int major_spdif;
static struct class *class_spdif;
static struct device *dev_spdif;
static struct mutex mutex_spdif;
static unsigned int iec958_wr_offset;

static inline int if_audio_output_iec958_enable(void)
{
	return aml_read_cbus(AIU_MEM_IEC958_CONTROL) && (0x3 << 1);
}

static inline int if_audio_output_i2s_enable(void)
{
	return aml_read_cbus(AIU_MEM_I2S_CONTROL) && (0x3 << 1);
}

static inline void audio_output_iec958_enable(unsigned int flag)
{
	if (flag) {
		aml_write_cbus(AIU_958_FORCE_LEFT, 0);
		aml_cbus_update_bits(AIU_958_DCU_FF_CTRL, 1, 1);
		aml_cbus_update_bits(AIU_MEM_IEC958_CONTROL, 0x3 << 1,
				0x3 << 1);
	} else {
		aml_write_cbus(AIU_958_DCU_FF_CTRL, 0);
		aml_cbus_update_bits(AIU_MEM_IEC958_CONTROL, 0x3 << 1, 0);
	}
}

static int audio_spdif_open(struct inode *inode, struct file *file)
{
	if (device_opened) {
		pr_info("error,audio_spdif device busy\n");
		return -EBUSY;
	}
	device_opened++;
	try_module_get(THIS_MODULE);
	return 0;
}

static int audio_spdif_release(struct inode *inode, struct file *file)
{
	/* audio_enable_output(0); */
	/* audio_output_iec958_enable(0); */
	aml_alsa_hw_reprepare();
	device_opened--;
	module_put(THIS_MODULE);
	IEC958_mode_codec = 0;
	return 0;
}

static long audio_spdif_ioctl(struct file *file, unsigned int cmd,
			unsigned long args)
{
	int err = 0;
	int tmp = 0;

	mutex_lock(&mutex_spdif);
	switch (cmd) {
	case AUDIO_SPDIF_GET_958_BUF_RD_OFFSET:
		tmp =
		aml_read_cbus(AIU_MEM_IEC958_RD_PTR) -
		aml_read_cbus(AIU_MEM_IEC958_START_PTR);
		put_user(tmp, (__s32 __user *) args);
		break;
	case AUDIO_SPDIF_GET_958_BUF_SIZE:
		/* iec958_info.iec958_buffer_size; */
		tmp = aml_read_cbus(AIU_MEM_IEC958_END_PTR) -
			aml_read_cbus(AIU_MEM_IEC958_START_PTR) + 64;
		if (tmp == 64)
			tmp = 0;
		if (aml_read_cbus(AIU_MEM_IEC958_START_PTR) ==
		aml_read_cbus(AIU_MEM_I2S_START_PTR)) {
			tmp = tmp * 4;
		}
		put_user(tmp, (__s32 __user *) args);
		break;
	case AUDIO_SPDIF_GET_958_ENABLE_STATUS:
		put_user(if_audio_output_iec958_enable(),
			(__s32 __user *) args);
		break;
	case AUDIO_SPDIF_GET_I2S_ENABLE_STATUS:
		put_user(if_audio_output_i2s_enable(), (__s32 __user *) args);
		break;
	case AUDIO_SPDIF_SET_958_ENABLE:

		/* IEC958_mode_raw = 1; */
		/* audio_enable_output(1); */
		audio_output_iec958_enable(args);
		break;
	case AUDIO_SPDIF_SET_958_INIT_PREPARE:
		IEC958_mode_codec = 3;	/* dts pcm raw */
		DSP_WD(DSP_IEC958_INIT_READY_INFO, 0x12345678);
		aml_alsa_hw_reprepare();
		DSP_WD(DSP_IEC958_INIT_READY_INFO, 0);
		break;
	case AUDIO_SPDIF_SET_958_WR_OFFSET:
		get_user(iec958_wr_offset, (__u32 __user *) args);
		break;
	default:
		pr_info("audio spdif: cmd not implemented\n");
		break;
	}
	mutex_unlock(&mutex_spdif);
	return err;
}

static ssize_t audio_spdif_write(struct file *file,
				const char __user *userbuf, size_t len,
				loff_t *off)
{
	char *wr_ptr;
	unsigned long wr_addr;
	dma_addr_t buf_map;

	wr_addr = aml_read_cbus(AIU_MEM_IEC958_START_PTR) + iec958_wr_offset;
	wr_ptr = (char *)phys_to_virt(wr_addr);
	if (copy_from_user((void *)wr_ptr, (void *)userbuf, len) != 0) {
		pr_info("audio spdif: copy from user failed\n");
		return -EINVAL;
	}
	buf_map = dma_map_single(NULL, (void *)wr_ptr, len, DMA_TO_DEVICE);
	dma_unmap_single(NULL, buf_map, len, DMA_TO_DEVICE);
	return 0;
}

static ssize_t audio_spdif_read(struct file *filp, char __user *buffer,
			size_t length, loff_t *offset)
{
	pr_info("audio spdif: read operation isn't supported.\n");
	return -EINVAL;
}

static int audio_spdif_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long off = vma->vm_pgoff << PAGE_SHIFT;
	unsigned int vm_size = vma->vm_end - vma->vm_start;

	if (vm_size == 0) {
		pr_info("audio spdif:vm_size 0\n");
		return -EAGAIN;
	}
	/* mapping the 958 dma buffer to user space to write */
	off = aml_read_cbus(AIU_MEM_IEC958_START_PTR);

	/*|VM_MAYWRITE|VM_MAYSHARE */
	vma->vm_flags |=
		VM_DONTEXPAND | VM_DONTDUMP | VM_IO;
	if (remap_pfn_range(vma, vma->vm_start,
		off >> PAGE_SHIFT,
	    vma->vm_end - vma->vm_start,
	    vma->vm_page_prot)) {
		pr_info("audio spdif : failed remap_pfn_range\n");
		return -EAGAIN;
	}
	pr_info("audio spdif: mmap finished\n");
	pr_info("audio spdif: 958 dma buf:py addr 0x%x,vir addr 0x%x\n",
	aml_read_cbus(AIU_MEM_IEC958_START_PTR), (unsigned int)
		phys_to_virt(aml_read_cbus(AIU_MEM_IEC958_START_PTR)));
	return 0;
}

static ssize_t audio_spdif_ptr_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	ssize_t ret = 0;

	ret = sprintf(buf, "iec958 buf runtime info:\n"
		"  iec958 rd ptr :\t%x\n"
		"  iec958 wr ptr :\t%x\n",
		aml_read_cbus(AIU_MEM_IEC958_RD_PTR),
		(aml_read_cbus(AIU_MEM_IEC958_START_PTR) +
			iec958_wr_offset));
	return ret;
}

static ssize_t audio_spdif_buf_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	unsigned int *ptr =
		(unsigned int *)phys_to_virt(
				aml_read_cbus(AIU_MEM_IEC958_RD_PTR) +
				iec958_wr_offset);
	ret =
		sprintf(buf,
		"iec958 buf  info:\n"
		" iec958 wr ptr val:\t[%x][%x][%x][%x]\n", ptr[0], ptr[1],
		ptr[2], ptr[3]);
	return ret;
}

static struct class_attribute audio_spdif_attrs[] = {
	__ATTR_RO(audio_spdif_ptr),
	__ATTR_RO(audio_spdif_buf),
	__ATTR_NULL
};

static const struct file_operations fops_spdif = {
	.read = audio_spdif_read,
	.unlocked_ioctl = audio_spdif_ioctl,
	.write = audio_spdif_write,
	.open = audio_spdif_open,
	.mmap = audio_spdif_mmap,
	.release = audio_spdif_release
};

static void create_audio_spdif_attrs(struct class *class)
{
	int i = 0, ret;

	for (i = 0; audio_spdif_attrs[i].attr.name; i++)
		ret = class_create_file(class, &audio_spdif_attrs[i]);
}

static int __init audio_spdif_init_module(void)
{
	void *ptr_err;

	major_spdif = register_chrdev(0, DEVICE_NAME, &fops_spdif);
	if (major_spdif < 0) {
		pr_info("Registering spdif char device %s failed with %d\n",
			DEVICE_NAME, major_spdif);
		return major_spdif;
	}
	class_spdif = class_create(THIS_MODULE, DEVICE_NAME);
	ptr_err = class_spdif;
	if (IS_ERR(ptr_err))
		goto err0;

	create_audio_spdif_attrs(class_spdif);
	dev_spdif =
		device_create(class_spdif, NULL, MKDEV(major_spdif, 0), NULL,
			DEVICE_NAME);
	ptr_err = dev_spdif;
	if (IS_ERR(ptr_err))
		goto err1;

	mutex_init(&mutex_spdif);
	pr_info("amlogic audio spdif interface device init!\n");
	return 0;

#if 0
err2:
	device_destroy(class_spdif, MKDEV(major_spdif, 0));
#endif				/*  */
err1:
	class_destroy(class_spdif);
err0:
	unregister_chrdev(major_spdif, DEVICE_NAME);
	return PTR_ERR(ptr_err);
}

static void __exit audio_spdif_exit_module(void)
{
	device_destroy(class_spdif, MKDEV(major_spdif, 0));
	class_destroy(class_spdif);
	unregister_chrdev(major_spdif, DEVICE_NAME);
}

module_init(audio_spdif_init_module);
module_exit(audio_spdif_exit_module);
MODULE_DESCRIPTION("AMLOGIC IEC958 output interface driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("jian.xu <jian.xu@amlogic.com>");
