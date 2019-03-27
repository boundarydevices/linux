/*
 * sound/soc/amlogic/auge/loopback_hw.c
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
#define DEBUG

#include "linux/kernel.h"

#include "loopback_hw.h"
#include "regs.h"
#include "iomap.h"


void tdminlb_set_clk(int datatlb_src, int sclk_div, int ratio, bool enable)
{
	unsigned int bclk_sel, fsclk_sel;
	unsigned int tdmin_src;

	/* config for external codec */
	if (datatlb_src >= 3) {
		unsigned int clk_id = datatlb_src - 3;
		unsigned int offset, reg;
		unsigned int fsclk_hi;

		fsclk_hi = ratio / 2;
		bclk_sel = clk_id;
		fsclk_sel = clk_id;

		/*sclk, lrclk*/
		offset = EE_AUDIO_MST_B_SCLK_CTRL0 - EE_AUDIO_MST_A_SCLK_CTRL0;
		reg = EE_AUDIO_MST_A_SCLK_CTRL0 + offset * clk_id;
		audiobus_update_bits(reg,
			0x3 << 30 | 0x3ff << 20 | 0x3ff<<10 | 0x3ff,
			(enable ? 0x3 : 0x0) << 30
			| sclk_div << 20 | fsclk_hi << 10
			| ratio);


		tdmin_src = datatlb_src - 3;
	} else
		tdmin_src = datatlb_src;

	audiobus_update_bits(
		EE_AUDIO_CLK_TDMIN_LB_CTRL,
		0x3 << 30 | 1 << 29 | 0xf << 24 | 0xf << 20,
		(enable ? 0x3 : 0x0) << 30 |
		1 << 29 | tdmin_src << 24 | tdmin_src << 20
	);
}

void tdminlb_enable(int tdm_index, int is_enable)
{
	audiobus_update_bits(
		EE_AUDIO_TDMIN_LB_CTRL,
		0x1 << 31,
		is_enable << 31);
}

void tdminlb_set_format(int i2s_fmt)
{
	audiobus_update_bits(EE_AUDIO_TDMIN_LB_CTRL,
		0x1 << 30,
		i2s_fmt << 30 /* 0:tdm mode; 1: i2s mode; */
	);
}

void tdminlb_set_ctrl(int src)
{
	audiobus_update_bits(
		EE_AUDIO_TDMIN_LB_CTRL,
		0xf << 20 | 0x7 << 16 | 0x1f << 0,
		src << 20 |  /* in src */
		3   << 16 |  /* skew */
		31  << 0     /* bit width */
	);
}

void tdminlb_fifo_enable(int is_enable)
{
	if (is_enable) {
		audiobus_update_bits(EE_AUDIO_TDMIN_LB_CTRL, 1<<29, 1<<29);
		audiobus_update_bits(EE_AUDIO_TDMIN_LB_CTRL, 1<<28, 1<<28);
	} else
		audiobus_update_bits(EE_AUDIO_TDMIN_LB_CTRL, 3<<28, 0);
}

static void tdminlb_set_lane_mask(int lane, int mask)
{
	audiobus_write(EE_AUDIO_TDMIN_LB_MASK0 + lane, mask);
}

void tdminlb_set_lanemask_and_chswap(int swap, int lane_mask)
{
	unsigned int mask;
	unsigned int i;

	pr_debug("tdmin_lb lane swap:0x%x mask:0x%x\n", swap, lane_mask);

	mask = 0x3; /* i2s format */

	/* channel swap */
	audiobus_write(EE_AUDIO_TDMIN_LB_SWAP0, swap);

	/* lane mask */
	for (i = 0; i < 4; i++)
		if ((1 << i) & lane_mask)
			tdminlb_set_lane_mask(i, mask);
}

void tdminlb_set_src(int src)
{
	audiobus_update_bits(EE_AUDIO_TDMIN_LB_CTRL, 0xf << 20, src);
}

void lb_set_datain_src(int id, int src)
{
	int offset = EE_AUDIO_LB_B_CTRL0 - EE_AUDIO_LB_A_CTRL0;
	int reg = EE_AUDIO_LB_A_CTRL0 + offset * id;

	audiobus_update_bits(reg, 0x7 << 0, src);
}

void lb_set_datain_cfg(int id, struct data_cfg *datain_cfg)
{
	int offset = EE_AUDIO_LB_B_CTRL0 - EE_AUDIO_LB_A_CTRL0;
	int reg = EE_AUDIO_LB_A_CTRL0 + offset * id;

	if (!datain_cfg)
		return;

	if (datain_cfg->ch_ctrl_switch) {
		audiobus_update_bits(reg,
			1 << 29 | 0x7 << 13 | 0x1f << 8 | 0x1f << 3 | 0x7 << 0,
			datain_cfg->ext_signed << 29 |
			datain_cfg->type       << 13 |
			datain_cfg->m          << 8  |
			datain_cfg->n          << 3  |
			datain_cfg->src        << 0
		);

		/* channel and mask */
		offset = EE_AUDIO_LB_B_CTRL2 - EE_AUDIO_LB_A_CTRL2;
		reg = EE_AUDIO_LB_A_CTRL2 + offset * id;
		audiobus_write(reg,
			(datain_cfg->chnum - 1) << 16 |
			datain_cfg->chmask      << 0
		);
	} else
		audiobus_update_bits(reg,
			1 << 29 | 0x7 << 24 | 0xff << 16 |
			0x7 << 13 | 0x1f << 8 | 0x1f << 3 |
			0x7 << 0,
			datain_cfg->ext_signed  << 29 |
			(datain_cfg->chnum - 1) << 24 |
			datain_cfg->chmask      << 16 |
			datain_cfg->type        << 13 |
			datain_cfg->m           << 8  |
			datain_cfg->n           << 3  |
			datain_cfg->src         << 0
		);
}

void lb_set_datalb_cfg(int id, struct data_cfg *datalb_cfg)
{
	int offset = EE_AUDIO_LB_B_CTRL1 - EE_AUDIO_LB_A_CTRL1;
	int reg = EE_AUDIO_LB_A_CTRL1 + offset * id;

	if (!datalb_cfg)
		return;

	if (datalb_cfg->ch_ctrl_switch) {
		audiobus_update_bits(reg,
			0x1 << 29 | 0x7 << 13 | 0x1f << 8
			| 0x1f << 3 | 0x1 << 1,
			datalb_cfg->ext_signed << 29 |
			datalb_cfg->type       << 13 |
			datalb_cfg->m          << 8  |
			datalb_cfg->n          << 3  |
			datalb_cfg->datalb_src << 0
		);

		/* channel and mask */
		offset = EE_AUDIO_LB_B_CTRL3 - EE_AUDIO_LB_A_CTRL3;
		reg = EE_AUDIO_LB_A_CTRL3 + offset * id;
		audiobus_write(reg,
			(datalb_cfg->chnum - 1) << 16 |
			datalb_cfg->chmask	<< 0
		);
	} else
		audiobus_write(reg,
			datalb_cfg->ext_signed   << 29 |
			(datalb_cfg->chnum - 1)  << 24 |
			datalb_cfg->chmask       << 16 |
			datalb_cfg->type         << 13 |
			datalb_cfg->m            << 8  |
			datalb_cfg->n            << 3
		);
}

void lb_enable(int id, bool enable)
{
	int offset = EE_AUDIO_LB_B_CTRL0 - EE_AUDIO_LB_A_CTRL0;
	int reg = EE_AUDIO_LB_A_CTRL0 + offset * id;

	audiobus_update_bits(reg, 0x1 << 31, enable << 31);
}
