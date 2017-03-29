/*
 * drivers/amlogic/audiodsp/spdif_module.h
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

#ifndef __SPDIF_MODULE_H__
#define __SPDIF_MODULE_H__
struct _iec958_data_info {
	unsigned int iec958_hw_start;
	unsigned int iec958_hw_rd_offset;
	unsigned int iec958_wr_offset;
	unsigned int iec958_buffer_size;
};

#define AUDIO_SPDIF_GET_958_BUF_SIZE	_IOR('s', 0x01, unsigned long)
#define AUDIO_SPDIF_GET_958_BUF_CONTENT	_IOR('s', 0x02, unsigned long)
#define AUDIO_SPDIF_GET_958_BUF_SPACE	_IOR('s', 0x03, unsigned long)
#define AUDIO_SPDIF_GET_958_BUF_RD_OFFSET	_IOR('s', 0x04, unsigned long)
#define AUDIO_SPDIF_GET_958_ENABLE_STATUS	_IOR('s', 0x05, unsigned long)
#define AUDIO_SPDIF_GET_I2S_ENABLE_STATUS	_IOR('s', 0x06, unsigned long)
#define AUDIO_SPDIF_SET_958_ENABLE		_IOW('s', 0x07, unsigned long)
#define AUDIO_SPDIF_SET_958_INIT_PREPARE	_IOW('s', 0x08, unsigned long)
#define AUDIO_SPDIF_SET_958_WR_OFFSET	_IOW('s', 0x09, unsigned long)
static int audio_spdif_open(struct inode *inode, struct file *file);
static int audio_spdif_release(struct inode *inode, struct file *file);
static long audio_spdif_ioctl(struct file *file, unsigned int cmd,
			unsigned long args);
static ssize_t audio_spdif_write(struct file *file,
				const char __user *userbuf, size_t len,
				loff_t *off);
static ssize_t audio_spdif_read(struct file *filp, char __user *buffer,
				size_t length, loff_t *offset);
static int audio_spdif_mmap(struct file *file, struct vm_area_struct *vma);
extern void aml_alsa_hw_reprepare(void);
extern void audio_enable_output(int flag);
extern unsigned int IEC958_mode_raw;
extern unsigned int IEC958_mode_codec;

#endif				/*  */
