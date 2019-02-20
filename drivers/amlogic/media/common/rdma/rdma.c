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

#define RDMA_NUM  2
static int second_rdma_feature;
static int rdma_num = RDMA_NUM;
static int vsync_rdma_handle[RDMA_NUM];
static int irq_count[RDMA_NUM];
static int enable[RDMA_NUM];
static int cur_enable[RDMA_NUM];
static int pre_enable_[RDMA_NUM];
static int debug_flag[RDMA_NUM];
static int vsync_cfg_count[RDMA_NUM];
static u32 force_rdma_config[RDMA_NUM];
static bool first_config[RDMA_NUM];
static bool rdma_done[RDMA_NUM];

static void vsync_rdma_irq(void *arg);
static void line_n_int_rdma_irq(void *arg);

struct rdma_op_s vsync_rdma_op = {
	vsync_rdma_irq,
	NULL
};
struct rdma_op_s line_n_int_rdma_op = {
	line_n_int_rdma_irq,
	NULL
};

static void set_rdma_trigger_line(void)
{
	int trigger_line;

	switch (aml_read_vcbus(VPU_VIU_VENC_MUX_CTRL) & 0x3) {
	case 0:
		trigger_line = aml_read_vcbus(ENCL_VIDEO_VAVON_ELINE)
			- aml_read_vcbus(ENCL_VIDEO_VSO_BLINE);
		break;
	case 1:
		if ((aml_read_vcbus(ENCI_VIDEO_MODE) & 1) == 0)
			trigger_line = 260; /* 480i */
		else
			trigger_line = 310; /* 576i */
		break;
	case 2:
		if (aml_read_vcbus(ENCP_VIDEO_MODE) & (1 << 12))
			trigger_line = aml_read_vcbus(ENCP_DE_V_END_EVEN);
		else
			trigger_line = aml_read_vcbus(ENCP_VIDEO_VAVON_ELINE)
				- aml_read_vcbus(ENCP_VIDEO_VSO_BLINE);
		break;
	case 3:
		trigger_line = aml_read_vcbus(ENCT_VIDEO_VAVON_ELINE)
			- aml_read_vcbus(ENCT_VIDEO_VSO_BLINE);
		break;
	}
	aml_write_vcbus(VPP_INT_LINE_NUM, trigger_line);
}

void _vsync_rdma_config(int rdma_type)
{
	int iret = 0;
	int enable_ = cur_enable[rdma_type] & 0xf;

	if (vsync_rdma_handle[rdma_type] <= 0)
		return;

	/* first frame not use rdma */
	if (!first_config[rdma_type]) {
		cur_enable[rdma_type] = enable[rdma_type];
		pre_enable_[rdma_type] = enable_;
		first_config[rdma_type] = true;
		rdma_done[rdma_type] = false;
		return;
	}

	/* if rdma mode changed, reset rdma */
	if (pre_enable_[rdma_type] != enable_) {
		rdma_clear(vsync_rdma_handle[rdma_type]);
		force_rdma_config[rdma_type] = 1;
	}

	if (force_rdma_config[rdma_type])
		rdma_done[rdma_type] = true;

	if (enable_ == 1) {
		if (rdma_done[rdma_type])
			iret = rdma_watchdog_setting(0);
		else
			iret = rdma_watchdog_setting(1);
	} else {
		/* not vsync mode */
		iret = rdma_watchdog_setting(0);
		force_rdma_config[rdma_type] = 1;
	}
	rdma_done[rdma_type] = false;
	if (iret)
		force_rdma_config[rdma_type] = 1;

	iret = 0;
	if (force_rdma_config[rdma_type]) {
		if (enable_ == 1) {
			if (rdma_type == VSYNC_RDMA)
				iret = rdma_config(vsync_rdma_handle[rdma_type],
					RDMA_TRIGGER_VSYNC_INPUT);
			else if (rdma_type == LINE_N_INT_RDMA) {
				set_rdma_trigger_line();
				iret = rdma_config(vsync_rdma_handle[rdma_type],
					RDMA_TRIGGER_LINE_INPUT);
			}
			if (iret)
				vsync_cfg_count[rdma_type]++;
		} else if (enable_ == 2)
			/*manually in cur vsync*/
			rdma_config(vsync_rdma_handle[rdma_type],
				RDMA_TRIGGER_MANUAL);
		else if (enable_ == 3)
			;
		else if (enable_ == 4)
			rdma_config(vsync_rdma_handle[rdma_type],
				RDMA_TRIGGER_DEBUG1); /*for debug*/
		else if (enable_ == 5)
			rdma_config(vsync_rdma_handle[rdma_type],
				RDMA_TRIGGER_DEBUG2); /*for debug*/
		else if (enable_ == 6)
			;
		if (!iret)
			force_rdma_config[rdma_type] = 1;
		else
			force_rdma_config[rdma_type] = 0;
	}
	pre_enable_[rdma_type] = enable_;
	cur_enable[rdma_type] = enable[rdma_type];
}

void vsync_rdma_config(void)
{
	_vsync_rdma_config(VSYNC_RDMA);
	if (second_rdma_feature &&
		is_meson_g12b_revb())
		_vsync_rdma_config(LINE_N_INT_RDMA);
}

EXPORT_SYMBOL(vsync_rdma_config);

void _vsync_rdma_config_pre(int rdma_type)
{
	int enable_ = cur_enable[rdma_type] & 0xf;

	if (vsync_rdma_handle[rdma_type] == 0)
		return;
	if (enable_ == 3)/*manually in next vsync*/
		rdma_config(vsync_rdma_handle[rdma_type], 0);
	else if (enable_ == 6)
		rdma_config(vsync_rdma_handle[rdma_type], 0x101); /*for debug*/
}

void vsync_rdma_config_pre(void)
{
	_vsync_rdma_config_pre(VSYNC_RDMA);
	if (second_rdma_feature &&
		is_meson_g12b_revb())
		_vsync_rdma_config_pre(LINE_N_INT_RDMA);
}
EXPORT_SYMBOL(vsync_rdma_config_pre);

static void vsync_rdma_irq(void *arg)
{
	int iret;
	int enable_ = cur_enable[VSYNC_RDMA] & 0xf;

	if (enable_ == 1) {
		/*triggered by next vsync*/
		iret = rdma_config(vsync_rdma_handle[VSYNC_RDMA],
			RDMA_TRIGGER_VSYNC_INPUT);
		if (iret)
			vsync_cfg_count[VSYNC_RDMA]++;
	} else
		iret = rdma_config(vsync_rdma_handle[VSYNC_RDMA], 0);
	pre_enable_[VSYNC_RDMA] = enable_;

	if ((!iret) || (enable_ != 1))
		force_rdma_config[VSYNC_RDMA] = 1;
	else
		force_rdma_config[VSYNC_RDMA] = 0;
	rdma_done[VSYNC_RDMA] = true;
	irq_count[VSYNC_RDMA]++;
	return;
}

static void line_n_int_rdma_irq(void *arg)
{
	int iret;
	int enable_ = cur_enable[LINE_N_INT_RDMA] & 0xf;

	if (enable_ == 1) {
		/*triggered by next vsync*/
		//set_rdma_trigger_line();
		iret = rdma_config(vsync_rdma_handle[LINE_N_INT_RDMA],
			RDMA_TRIGGER_LINE_INPUT);
		if (iret)
			vsync_cfg_count[LINE_N_INT_RDMA]++;
	} else
		iret = rdma_config(vsync_rdma_handle[LINE_N_INT_RDMA], 0);
	pre_enable_[LINE_N_INT_RDMA] = enable_;

	if ((!iret) || (enable_ != 1))
		force_rdma_config[LINE_N_INT_RDMA] = 1;
	else
		force_rdma_config[LINE_N_INT_RDMA] = 0;
	rdma_done[VSYNC_RDMA] = true;
	irq_count[VSYNC_RDMA]++;
}

u32 VSYNC_RD_MPEG_REG(u32 adr)
{
	int enable_ = cur_enable[VSYNC_RDMA] & 0xf;

	u32 read_val = Rd(adr);

	if ((enable_ != 0) && (vsync_rdma_handle[VSYNC_RDMA] > 0))
		read_val = rdma_read_reg(vsync_rdma_handle[VSYNC_RDMA], adr);

	return read_val;
}
EXPORT_SYMBOL(VSYNC_RD_MPEG_REG);

int VSYNC_WR_MPEG_REG(u32 adr, u32 val)
{
	int enable_ = cur_enable[VSYNC_RDMA] & 0xf;

	if ((enable_ != 0) && (vsync_rdma_handle[VSYNC_RDMA] > 0)) {
		rdma_write_reg(vsync_rdma_handle[VSYNC_RDMA], adr, val);
	} else {
		Wr(adr, val);
		if (debug_flag[VSYNC_RDMA] & 1)
			pr_info("VSYNC_WR(%x)<=%x\n", adr, val);
	}
	return 0;
}
EXPORT_SYMBOL(VSYNC_WR_MPEG_REG);

int VSYNC_WR_MPEG_REG_BITS(u32 adr, u32 val, u32 start, u32 len)
{
	int enable_ = cur_enable[VSYNC_RDMA] & 0xf;

	if ((enable_ != 0) && (vsync_rdma_handle[VSYNC_RDMA] > 0)) {
		rdma_write_reg_bits(vsync_rdma_handle[VSYNC_RDMA],
			adr, val, start, len);
	} else {
		u32 read_val = Rd(adr);
		u32 write_val = (read_val & ~(((1L<<(len))-1)<<(start)))
			|((unsigned int)(val) << (start));
		Wr(adr, write_val);
		if (debug_flag[VSYNC_RDMA] & 1)
			pr_info("VSYNC_WR(%x)<=%x\n", adr, write_val);
	}
	return 0;
}
EXPORT_SYMBOL(VSYNC_WR_MPEG_REG_BITS);

u32 _VSYNC_RD_MPEG_REG(u32 adr)
{
	u32 read_val = 0;

	if (second_rdma_feature && is_meson_g12b_revb()) {
		int enable_ = cur_enable[LINE_N_INT_RDMA] & 0xf;

		read_val = Rd(adr);

		if ((enable_ != 0) &&
			(vsync_rdma_handle[LINE_N_INT_RDMA] > 0))
			read_val = rdma_read_reg
				(vsync_rdma_handle[LINE_N_INT_RDMA], adr);
	} else {
		read_val = VSYNC_RD_MPEG_REG(adr);
	}
	return read_val;
}
EXPORT_SYMBOL(_VSYNC_RD_MPEG_REG);

int _VSYNC_WR_MPEG_REG(u32 adr, u32 val)
{
	if (second_rdma_feature && is_meson_g12b_revb()) {
		int enable_ = cur_enable[LINE_N_INT_RDMA] & 0xf;

		if ((enable_ != 0) &&
			(vsync_rdma_handle[LINE_N_INT_RDMA] > 0)) {
			rdma_write_reg
				(vsync_rdma_handle[LINE_N_INT_RDMA], adr, val);
		} else {
			Wr(adr, val);
			if (debug_flag[LINE_N_INT_RDMA] & 1)
				pr_info("VSYNC_WR(%x)<=%x\n", adr, val);
		}
	} else {
		VSYNC_WR_MPEG_REG(adr, val);
	}
	return 0;
}
EXPORT_SYMBOL(_VSYNC_WR_MPEG_REG);

int _VSYNC_WR_MPEG_REG_BITS(u32 adr, u32 val, u32 start, u32 len)
{
	if (second_rdma_feature && is_meson_g12b_revb()) {
		int enable_ = cur_enable[LINE_N_INT_RDMA] & 0xf;

		if ((enable_ != 0) &&
			(vsync_rdma_handle[LINE_N_INT_RDMA] > 0)) {
			rdma_write_reg_bits
				(vsync_rdma_handle[LINE_N_INT_RDMA],
				adr, val, start, len);
		} else {
			u32 read_val = Rd(adr);
			u32 write_val = (read_val & ~(((1L<<(len))-1)<<(start)))
				|((unsigned int)(val) << (start));
			Wr(adr, write_val);
			if (debug_flag[LINE_N_INT_RDMA] & 1)
				pr_info("VSYNC_WR(%x)<=%x\n", adr, write_val);
		}
	} else {
		VSYNC_WR_MPEG_REG_BITS(adr, val, start, len);
	}
	return 0;
}
EXPORT_SYMBOL(_VSYNC_WR_MPEG_REG_BITS);


bool is_vsync_rdma_enable(void)
{
	bool ret;
	int enable_ = cur_enable[VSYNC_RDMA] & 0xf;

	ret = (enable_ != 0);
	return ret;
}
EXPORT_SYMBOL(is_vsync_rdma_enable);

void enable_rdma_log(int flag)
{
	if (flag) {
		debug_flag[VSYNC_RDMA] |= 0x1;
		debug_flag[LINE_N_INT_RDMA] |= 0x1;
	} else {
		debug_flag[VSYNC_RDMA] &= (~0x1);
		debug_flag[LINE_N_INT_RDMA] &= (~0x1);
	}
}
EXPORT_SYMBOL(enable_rdma_log);

void enable_rdma(int enable_flag)
{
	enable[VSYNC_RDMA] = enable_flag;
	enable[LINE_N_INT_RDMA] = enable_flag;
}
EXPORT_SYMBOL(enable_rdma);

struct rdma_op_s *get_rdma_ops(int rdma_type)
{
	if (rdma_type == VSYNC_RDMA)
		return &vsync_rdma_op;
	else if (rdma_type == LINE_N_INT_RDMA)
		return &line_n_int_rdma_op;
	else
		return NULL;
}

void set_rdma_handle(int rdma_type, int handle)
{
	vsync_rdma_handle[rdma_type] = handle;
	pr_info("%s video rdma handle = %d.\n", __func__,
		vsync_rdma_handle[rdma_type]);

}

u32 is_line_n_rdma_enable(void)
{
	return second_rdma_feature;
}

static int  __init rdma_init(void)

{
	second_rdma_feature = 0;
	if (is_meson_g12b_revb())
		second_rdma_feature = 1;

	cur_enable[VSYNC_RDMA] = 0;
	enable[VSYNC_RDMA] = 1;
	force_rdma_config[VSYNC_RDMA] = 1;

	if (second_rdma_feature) {
		cur_enable[LINE_N_INT_RDMA] = 0;
		enable[LINE_N_INT_RDMA] = 1;
		force_rdma_config[LINE_N_INT_RDMA] = 1;
	}

	return 0;
}

module_init(rdma_init);

MODULE_PARM_DESC(second_rdma_feature,
	"\n second_rdma_feature enable\n");
module_param(second_rdma_feature, uint, 0664);

MODULE_PARM_DESC(enable,
	"\n vsync rdma enable\n");
module_param_array(enable, uint, &rdma_num, 0664);

MODULE_PARM_DESC(irq_count,
	"\n vsync rdma irq_count\n");
module_param_array(irq_count, uint, &rdma_num, 0664);

MODULE_PARM_DESC(debug_flag,
	"\n vsync rdma debug_flag\n");
module_param_array(debug_flag, uint, &rdma_num, 0664);

MODULE_PARM_DESC(vsync_cfg_count,
	"\n vsync rdma vsync_cfg_count\n");
module_param_array(vsync_cfg_count, uint, &rdma_num, 0664);

MODULE_PARM_DESC(force_rdma_config,
	"\n vsync rdma force_rdma_config\n");
module_param_array(force_rdma_config, uint, &rdma_num, 0664);
