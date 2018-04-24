/*
 * drivers/amlogic/media/deinterlace/nr_downscale.c
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

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/dma-contiguous.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include "register.h"
#include "nr_downscale.h"

static struct nr_ds_s nrds_dev;

static void nr_ds_hw_init(unsigned int width, unsigned int height)
{
	unsigned char h_step = 0, v_step = 0;
	unsigned int width_out, height_out;

	width_out = NR_DS_WIDTH;
	height_out = NR_DS_HEIGHT;

	h_step = width/width_out;
	v_step = height/height_out;

	RDMA_WR_BITS(VIUB_MISC_CTRL0, 3, 5, 2);   //Switch MIF to NR_DS
	RDMA_WR_BITS(NR_DS_BUF_SIZE_REG, width_out, 0, 8);  // config dsbuf_ocol
	RDMA_WR_BITS(NR_DS_BUF_SIZE_REG, height_out, 8, 8); // config dsbuf_orow

	RDMA_WR_BITS(NRDSWR_X, (width_out-1), 0, 13);
	RDMA_WR_BITS(NRDSWR_Y, (height_out-1), 0, 13);

	RDMA_WR_BITS(NRDSWR_CAN_SIZE, (height_out-1), 0, 13);
	RDMA_WR_BITS(NRDSWR_CAN_SIZE, (width_out-1), 16, 13);
	/* little endian */
	RDMA_WR_BITS(NRDSWR_CAN_SIZE, 1, 13, 1);

	RDMA_WR_BITS(NR_DS_CTRL, v_step, 16, 6);
	RDMA_WR_BITS(NR_DS_CTRL, h_step, 24, 6);
}

/*
 * init nr ds buffer
 */
void nr_ds_buf_init(unsigned int cma_flag, unsigned long mem_start,
	struct device *dev)
{
	unsigned int i = 0;

	if (cma_flag == 0) {
		nrds_dev.nrds_addr = mem_start;
	} else {
		nrds_dev.nrds_pages = dma_alloc_from_contiguous(dev,
			NR_DS_PAGE_NUM, 0);
		if (nrds_dev.nrds_pages)
			nrds_dev.nrds_addr = page_to_phys(nrds_dev.nrds_pages);
		else
			pr_err("DI: alloc nr ds mem error.\n");
	}
	for (i = 0; i < NR_DS_BUF_NUM; i++)
		nrds_dev.buf[i] = nrds_dev.nrds_addr + (NR_DS_BUF_SIZE*i);
	nrds_dev.cur_buf_idx = 0;

}

void nr_ds_buf_uninit(unsigned int cma_flag, struct device *dev)
{
	unsigned int i = 0;

	if (cma_flag == 0) {
		nrds_dev.nrds_addr = 0;
	} else {
		if (nrds_dev.nrds_pages) {
			dma_release_from_contiguous(dev,
				nrds_dev.nrds_pages,
				NR_DS_PAGE_NUM);
			nrds_dev.nrds_addr = 0;
			nrds_dev.nrds_pages = NULL;
		} else
			pr_info("DI: no release nr ds mem.\n");
	}
	for (i = 0; i < NR_DS_BUF_NUM; i++)
		nrds_dev.buf[i] = 0;
	nrds_dev.cur_buf_idx = 0;
}
/*
 * hw config, alloc canvas
 */
void nr_ds_init(unsigned int width, unsigned int height)
{
	nr_ds_hw_init(width, height);
	nrds_dev.field_num = 0;

	if (nrds_dev.canvas_idx != 0)
		return;

	if (canvas_pool_alloc_canvas_table("nr_ds",
		&nrds_dev.canvas_idx, 1, CANVAS_MAP_TYPE_1)) {
		pr_err("%s alloc nrds canvas error.\n", __func__);
		return;
	}
	pr_info("%s alloc nrds canvas %u.\n",
		__func__, nrds_dev.canvas_idx);
}

/*
 * config nr ds mif, switch buffer
 */
void nr_ds_mif_config(void)
{
	unsigned long mem_addr = 0;

	mem_addr = nrds_dev.buf[nrds_dev.cur_buf_idx];
	canvas_config(nrds_dev.canvas_idx, mem_addr,
		NR_DS_WIDTH, NR_DS_HEIGHT, 0, 0);
	RDMA_WR_BITS(NRDSWR_CTRL,
		nrds_dev.canvas_idx, 0, 8);
	nr_ds_hw_ctrl(true);
}

/*
 * enable/disable nr ds mif&hw
 */
void nr_ds_hw_ctrl(bool enable)
{
	RDMA_WR_BITS(VIUB_MISC_CTRL0, enable?3:2, 5, 2);   //Switch MIF to NR_DS
	RDMA_WR_BITS(NRDSWR_CTRL, enable?1:0, 12, 1);
	RDMA_WR_BITS(NR_DS_CTRL, enable?1:0, 30, 1);
}

/*
 * process in irq
 */
void nr_ds_irq(void)
{
	nr_ds_hw_ctrl(false);
	nrds_dev.field_num++;
	nrds_dev.cur_buf_idx++;
	if (nrds_dev.cur_buf_idx >= NR_DS_BUF_NUM)
		nrds_dev.cur_buf_idx = 0;
}
/*
 * get buf addr&size for dump
 */
void get_nr_ds_buf(unsigned long *addr, unsigned long *size)
{
	*addr = nrds_dev.nrds_addr;
	*size =	NR_DS_BUF_SIZE;
	pr_info("%s addr 0x%lx, size 0x%lx.\n",
		__func__, *addr, *size);
}
/*
 * 0x37f9 ~ 0x37fc 0x3740 ~ 0x3743 8 regs
 */
void dump_nrds_reg(unsigned int base_addr)
{
	unsigned int i = 0x37f9;

	pr_info("-----nrds reg start-----\n");
	pr_info("[0x%x][0x%x]=0x%x\n",
		base_addr + (0x2006 << 2), i, RDMA_RD(0x2006));
	for (i = 0x37f9; i < 0x37fd; i++)
		pr_info("[0x%x][0x%x]=0x%x\n",
	base_addr + (i << 2), i, RDMA_RD(i));
	for (i = 0x3740; i < 0x3744; i++)
		pr_info("[0x%x][0x%x]=0x%x\n",
	base_addr + (i << 2), i, RDMA_RD(i));
	pr_info("-----nrds reg end-----\n");
}
