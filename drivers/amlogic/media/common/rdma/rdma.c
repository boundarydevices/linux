/*
 * drivers/amlogic/media/common/rdma/rdma.c
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

#define INFO_PREFIX "video_rdma"

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/dma-mapping.h>

#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/ctype.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/clk.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include "rdma.h"
#include <linux/amlogic/media/utils/vdec_reg.h>
#include <linux/amlogic/media/rdma/rdma_mgr.h>

#define Wr(adr, val) WRITE_VCBUS_REG(adr, val)
#define Rd(adr)    READ_VCBUS_REG(adr)
#define Wr_reg_bits(adr, val, start, len) \
			WRITE_VCBUS_REG_BITS(adr, val, start, len)

#define RDMA_TABLE_SIZE                    (8 * (PAGE_SIZE))
static int vsync_rdma_handle;
static int irq_count;
static int enable;
static int cur_enable;
static int pre_enable_;
static int debug_flag;
static int vsync_cfg_count;
static u32 force_rdma_config;
static bool first_config;
static bool rdma_done;

static void vsync_rdma_irq(void *arg);

struct rdma_op_s vsync_rdma_op = {
	vsync_rdma_irq,
	NULL
};

void vsync_rdma_config(void)
{
	int iret = 0;
	int enable_ = cur_enable & 0xf;

	if (vsync_rdma_handle <= 0)
		return;

	/* first frame not use rdma */
	if (!first_config) {
		cur_enable = enable;
		pre_enable_ = enable_;
		first_config = true;
		rdma_done = false;
		return;
	}

	/* if rdma mode changed, reset rdma */
	if (pre_enable_ != enable_) {
		rdma_clear(vsync_rdma_handle);
		force_rdma_config = 1;
	}

	if (force_rdma_config)
		rdma_done = true;

	if (enable_ == 1) {
		if (rdma_done)
			iret = rdma_watchdog_setting(0);
		else
			iret = rdma_watchdog_setting(1);
	} else {
		/* not vsync mode */
		iret = rdma_watchdog_setting(0);
		force_rdma_config = 1;
	}
	rdma_done = false;
	if (iret)
		force_rdma_config = 1;

	iret = 0;
	if (force_rdma_config) {
		if (enable_ == 1) {
			iret = rdma_config(vsync_rdma_handle,
				RDMA_TRIGGER_VSYNC_INPUT);
			if (iret)
				vsync_cfg_count++;
		} else if (enable_ == 2)
			/*manually in cur vsync*/
			rdma_config(vsync_rdma_handle,
				RDMA_TRIGGER_MANUAL);
		else if (enable_ == 3)
			;
		else if (enable_ == 4)
			rdma_config(vsync_rdma_handle,
				RDMA_TRIGGER_DEBUG1); /*for debug*/
		else if (enable_ == 5)
			rdma_config(vsync_rdma_handle,
				RDMA_TRIGGER_DEBUG2); /*for debug*/
		else if (enable_ == 6)
			;
		if (!iret)
			force_rdma_config = 1;
		else
			force_rdma_config = 0;
	}
	pre_enable_ = enable_;
	cur_enable = enable;
}
EXPORT_SYMBOL(vsync_rdma_config);

void vsync_rdma_config_pre(void)
{
	int enable_ = cur_enable & 0xf;

	if (vsync_rdma_handle == 0)
		return;
	if (enable_ == 3)/*manually in next vsync*/
		rdma_config(vsync_rdma_handle, 0);
	else if (enable_ == 6)
		rdma_config(vsync_rdma_handle, 0x101); /*for debug*/
}
EXPORT_SYMBOL(vsync_rdma_config_pre);

static void vsync_rdma_irq(void *arg)
{
	int iret;
	int enable_ = cur_enable & 0xf;

	if (enable_ == 1) {
		/*triggered by next vsync*/
		iret = rdma_config(vsync_rdma_handle,
			RDMA_TRIGGER_VSYNC_INPUT);
		if (iret)
			vsync_cfg_count++;
	} else
		iret = rdma_config(vsync_rdma_handle, 0);
	pre_enable_ = enable_;

	if ((!iret) || (enable_ != 1))
		force_rdma_config = 1;
	else
		force_rdma_config = 0;
	rdma_done = true;
	irq_count++;
	return;
}

u32 VSYNC_RD_MPEG_REG(u32 adr)
{
	int enable_ = cur_enable & 0xf;

	u32 read_val = Rd(adr);

	if ((enable_ != 0) && (vsync_rdma_handle > 0))
		read_val = rdma_read_reg(vsync_rdma_handle, adr);

	return read_val;
}
EXPORT_SYMBOL(VSYNC_RD_MPEG_REG);

int VSYNC_WR_MPEG_REG(u32 adr, u32 val)
{
	int enable_ = cur_enable & 0xf;

	if ((enable_ != 0) && (vsync_rdma_handle > 0)) {
		rdma_write_reg(vsync_rdma_handle, adr, val);
	} else {
		Wr(adr, val);
		if (debug_flag & 1)
			pr_info("VSYNC_WR(%x)<=%x\n", adr, val);
	}
	return 0;
}
EXPORT_SYMBOL(VSYNC_WR_MPEG_REG);

int VSYNC_WR_MPEG_REG_BITS(u32 adr, u32 val, u32 start, u32 len)
{
	int enable_ = cur_enable & 0xf;

	if ((enable_ != 0) && (vsync_rdma_handle > 0)) {
		rdma_write_reg_bits(vsync_rdma_handle, adr, val, start, len);
	} else {
		u32 read_val = Rd(adr);
		u32 write_val = (read_val & ~(((1L<<(len))-1)<<(start)))
			|((unsigned int)(val) << (start));
		Wr(adr, write_val);
		if (debug_flag & 1)
			pr_info("VSYNC_WR(%x)<=%x\n", adr, write_val);
	}
	return 0;
}
EXPORT_SYMBOL(VSYNC_WR_MPEG_REG_BITS);


bool is_vsync_rdma_enable(void)
{
	bool ret;
	int enable_ = cur_enable & 0xf;

	ret = (enable_ != 0);
	return ret;
}
EXPORT_SYMBOL(is_vsync_rdma_enable);

void enable_rdma_log(int flag)
{
	if (flag)
		debug_flag |= 0x1;
	else
		debug_flag &= (~0x1);
}
EXPORT_SYMBOL(enable_rdma_log);

void enable_rdma(int enable_flag)
{
	enable = enable_flag;
}
EXPORT_SYMBOL(enable_rdma);

static int  __init rdma_init(void)

{
	vsync_rdma_handle =
		rdma_register(&vsync_rdma_op,
		NULL, RDMA_TABLE_SIZE);
	pr_info("%s video rdma handle = %d.\n", __func__,
		vsync_rdma_handle);
	cur_enable = 0;
	enable = 1;
	force_rdma_config = 1;
	return 0;
}

module_init(rdma_init);

MODULE_PARM_DESC(enable, "\n enable\n");
module_param(enable, uint, 0664);

MODULE_PARM_DESC(irq_count, "\n irq_count\n");
module_param(irq_count, uint, 0664);

MODULE_PARM_DESC(debug_flag, "\n debug_flag\n");
module_param(debug_flag, uint, 0664);

MODULE_PARM_DESC(vsync_cfg_count, "\n vsync_cfg_count\n");
module_param(vsync_cfg_count, uint, 0664);

MODULE_PARM_DESC(force_rdma_config, "\n force_rdma_config\n");
module_param(force_rdma_config, uint, 0664);
