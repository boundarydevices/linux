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


/*#define CONFIG_RDMA_IN_RDMAIRQ*/
/*#define CONFIG_RDMA_IN_TASK*/

#define RDMA_TABLE_SIZE                    (8 * (PAGE_SIZE))

static int vsync_rdma_handle;

static int irq_count;

static int enable;

static int enable_mask = 0x400ff;

static int pre_enable_;

static int debug_flag;

static int vsync_cfg_count;

#define RDMA_VSYNC_INPUT_TRIG		0x1
static bool vsync_rdma_config_delay_flag;

static void vsync_rdma_irq(void *arg);

struct rdma_op_s vsync_rdma_op = {
	vsync_rdma_irq,
	NULL
};

static struct semaphore  rdma_sema;
struct task_struct *rdma_task;
static unsigned int rdma_config_flag;

static unsigned char rdma_start_flag;


static int rdma_task_handle(void *data)
{
	int ret = 0;

	while (1) {
		ret = down_interruptible(&rdma_sema);
		if (debug_flag & 2)
			pr_info("%s: %x\r\n", __func__, rdma_config_flag);
		if (rdma_config_flag == 1) {
			rdma_config_flag = 0;
			if (rdma_config(vsync_rdma_handle,
				RDMA_VSYNC_INPUT_TRIG) != 1){
				rdma_config_flag = 2;
				/*
				 *fail or rdma table empty,
				 *there is no rdma irq
				 */
			}
		}
		if (rdma_start_flag) {
			if (vsync_rdma_handle <= 0)
				vsync_rdma_handle =
				rdma_register(&vsync_rdma_op,
				NULL, RDMA_TABLE_SIZE);
			rdma_start_flag = 0;
		}
	}
	return 0;
}


void vsync_rdma_config(void)
{
	int enable_ = ((enable & enable_mask) | (enable_mask >> 8)) & 0xff;

	if (vsync_rdma_handle == 0)
		return;

	if (pre_enable_ != enable_) {
		if (((enable_mask >> 17) & 0x1) == 0)
			rdma_clear(vsync_rdma_handle);
		vsync_rdma_config_delay_flag = false;
	}
	if (enable == 1)
		rdma_watchdog_setting(1);
	else
		rdma_watchdog_setting(0);
	if (enable_ == 1) {
#ifdef CONFIG_RDMA_IN_TASK
		if (debug_flag & 2) {
			pr_info("%s: %d : %d :\r\n", __func__,
			rdma_config_flag, pre_enable_);
		}
		if ((rdma_config_flag == 2) || (pre_enable_ != enable)) {
			rdma_config_flag = 1;
			up(&rdma_sema);
		}

#elif (defined CONFIG_RDMA_IN_RDMAIRQ)
		if (pre_enable_ != enable_)
			rdma_config(vsync_rdma_handle, RDMA_VSYNC_INPUT_TRIG);
#else
		rdma_config(vsync_rdma_handle, RDMA_VSYNC_INPUT_TRIG);
		vsync_cfg_count++;
#endif
	} else if (enable_ == 2)
		rdma_config(vsync_rdma_handle,
			RDMA_TRIGGER_MANUAL); /*manually in cur vsync*/
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

	pre_enable_ = enable_;
}
EXPORT_SYMBOL(vsync_rdma_config);

void vsync_rdma_config_pre(void)
{
	int enable_ = ((enable&enable_mask)|(enable_mask>>8))&0xff;

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
#ifdef CONFIG_RDMA_IN_TASK
	int enable_ = ((enable&enable_mask) | (enable_mask >> 8)) & 0xff;

	if (enable_ == 1) {
		rdma_config_flag = 1;
		up(&rdma_sema);
	} else
		rdma_config(vsync_rdma_handle, 0);

#elif (defined CONFIG_RDMA_IN_RDMAIRQ)
	int enable_ = ((enable&enable_mask) | (enable_mask >> 8)) & 0xff;

	if (enable_ == 1)
		rdma_config(vsync_rdma_handle,
		RDMA_VSYNC_INPUT_TRIG); /*triggered by next vsync*/
	else
		rdma_config(vsync_rdma_handle, 0);
#endif
	irq_count++;
}

MODULE_PARM_DESC(enable, "\n enable\n");
module_param(enable, uint, 0664);

MODULE_PARM_DESC(enable_mask, "\n enable_mask\n");
module_param(enable_mask, uint, 0664);

MODULE_PARM_DESC(irq_count, "\n irq_count\n");
module_param(irq_count, uint, 0664);



MODULE_PARM_DESC(debug_flag, "\n debug_flag\n");
module_param(debug_flag, uint, 0664);


MODULE_PARM_DESC(vsync_cfg_count, "\n vsync_cfg_count\n");
module_param(vsync_cfg_count, uint, 0664);

u32 VSYNC_RD_MPEG_REG(u32 adr)
{
	int enable_ = ((enable&enable_mask) | (enable_mask >> 8)) & 0xff;
	u32 read_val = Rd(adr);

	if ((enable_ != 0) && (vsync_rdma_handle > 0))
		read_val = rdma_read_reg(vsync_rdma_handle, adr);

	return read_val;
}
EXPORT_SYMBOL(VSYNC_RD_MPEG_REG);

int VSYNC_WR_MPEG_REG(u32 adr, u32 val)
{
	int enable_ = ((enable & enable_mask) | (enable_mask >> 8)) & 0xff;

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
	int enable_ = ((enable & enable_mask) | (enable_mask >> 8)) & 0xff;

	if ((enable_ != 0) && (vsync_rdma_handle > 0)) {
		rdma_write_reg_bits(vsync_rdma_handle, adr, val, start, len);
	}	else {
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
	int enable_ = ((enable & enable_mask) | (enable_mask >> 8)) & 0xff;

		return (enable_ != 0) && (((enable_mask >> 19) & 0x1) == 0);
}
EXPORT_SYMBOL(is_vsync_rdma_enable);

void start_rdma(void)
{
	if (vsync_rdma_handle <= 0) {
		rdma_start_flag = 1;
		up(&rdma_sema);
	}
}
EXPORT_SYMBOL(start_rdma);

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
	WRITE_VCBUS_REG(VPU_VDISP_ASYNC_HOLD_CTRL, 0x18101810);
	WRITE_VCBUS_REG(VPU_VPUARB2_ASYNC_HOLD_CTRL, 0x18101810);

	enable = 1;

	sema_init(&rdma_sema, 1);
	kthread_run(rdma_task_handle, NULL, "kthread_h265");
	return 0;
}

module_init(rdma_init);
