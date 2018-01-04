/*
 * drivers/amlogic/media/deinterlace/nr_downscale.h
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

#ifndef _NR_DS_H
#define _NR_DS_H

#define NR_DS_WIDTH	128
#define NR_DS_HEIGHT	96
#define NR_DS_BUF_SIZE	(96<<7)
#define NR_DS_BUF_NUM	6
#define NR_DS_MEM_SIZE (NR_DS_BUF_SIZE * NR_DS_BUF_NUM)
#define NR_DS_PAGE_NUM (NR_DS_MEM_SIZE>>PAGE_SHIFT)

struct nr_ds_s {
	unsigned int field_num;
	unsigned long nrds_addr;
	struct page	*nrds_pages;
	unsigned int canvas_idx;
	unsigned char cur_buf_idx;
	unsigned long buf[NR_DS_BUF_NUM];
};
void nr_ds_buf_init(unsigned int cma_flag, unsigned long mem_start,
	struct device *dev);
void nr_ds_buf_uninit(unsigned int cma_flag, struct device *dev);
void nr_ds_init(unsigned int width, unsigned int height);
void nr_ds_mif_config(void);
void nr_ds_hw_ctrl(bool enable);
void nr_ds_irq(void);
void get_nr_ds_buf(unsigned long *addr, unsigned long *size);
void dump_nrds_reg(unsigned int base_addr);
#endif
