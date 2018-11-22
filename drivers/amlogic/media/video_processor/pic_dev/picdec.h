/*
 * drivers/amlogic/media/video_processor/pic_dev/picdec.h
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

#ifndef _PICDEC_INCLUDE__
#define _PICDEC_INCLUDE__

#include <linux/interrupt.h>
/* #include <mach/am_regs.h> */
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/fb.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/sysfs.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/io-mapping.h>
#include <linux/ctype.h>
#include <linux/of.h>
#include <linux/sizes.h>
#include <linux/dma-mapping.h>
#include <linux/of_fdt.h>
#include <linux/dma-contiguous.h>
/**************************************************************
 *
 *                         macro define
 *
 **************************************************************
 */

struct picdec_device_s {
	char name[20];
	struct platform_device *pdev;
	int task_running;
	int dump;
	char *dump_path;
	unsigned int open_count;
	int major;
	unsigned int dbg_enable;
	struct class *cla;
	struct device *dev;
	resource_size_t buffer_start;
	unsigned int buffer_size;
	resource_size_t assit_buf_start;
	const struct vinfo_s *vinfo;
	int disp_width;
	int disp_height;
	int origin_width;
	int origin_height;
	int frame_render;
	int frame_post;
	int target_width;
	int target_height;
	int p2p_mode;
	int output_format_mode;
	struct ge2d_context_s *context;
	int cur_index;
	int use_reserved;
	struct page *cma_pages;
	struct io_mapping *mapping;
	void  __iomem *vir_addr;
	int cma_mode;
};

struct source_input_s {
	char *input;
	int frame_width;
	int frame_height;
	int format;
	int rotate;
};

#ifdef CONFIG_COMPAT
struct compat_source_input_s {
	compat_uptr_t input;
	int frame_width;
	int frame_height;
	int format;
	int rotate;
};

#define PICDEC_IOC_FRAME_RENDER32 _IOW(PICDEC_IOC_MAGIC, 0x00, \
struct compat_source_input_s)

#endif

struct picdec_private_s {
	struct ge2d_context_s *context;
	unsigned long phyaddr;
	unsigned int buf_len;
};

#define PICDEC_IOC_MAGIC  'P'
#define PICDEC_IOC_FRAME_RENDER     _IOW(PICDEC_IOC_MAGIC, 0x00, \
struct source_input_s)
#define PICDEC_IOC_FRAME_POST     _IOW(PICDEC_IOC_MAGIC, 0X01, unsigned int)
#define PICDEC_IOC_CONFIG_FRAME  _IOW(PICDEC_IOC_MAGIC, 0X02, unsigned int)

void stop_picdec_task(void);
int picdec_buffer_init(void);
void get_picdec_buf_info(resource_size_t *start, unsigned int *size,
						 struct io_mapping **mapping);
int picdec_fill_buffer(struct vframe_s *vf, struct ge2d_context_s *context,
	struct config_para_ex_s *ge2d_config);
extern void set_freerun_mode(int mode);
int picdec_cma_buf_init(void);
int picdec_cma_buf_uninit(void);
extern int start_picdec_task(void);
extern int start_picdec_simulate_task(void);
#endif				/* _PICDEC_INCLUDE__ */
