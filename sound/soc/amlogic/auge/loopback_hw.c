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
#include <sound/soc.h>

#include "regs.h"
#include "loopback_hw.h"
#include "iomap.h"

void datain_config(struct data_in *datain)
{
	audiobus_update_bits(
		EE_AUDIO_LB_CTRL0,
		1 << 29 | 0x7 << 24 | 0xff << 16 |
		0x7 << 13 | 0x1f << 8 | 0x1f << 3 |
		0x7 << 0,
		datain->config->ext_signed  << 29 |
		(datain->config->chnum - 1) << 24 |
		datain->config->chmask      << 16 |
		datain->ddrdata->combined_type << 13 |
		datain->ddrdata->msb        << 8  |
		datain->ddrdata->lsb        << 3  |
		datain->ddrdata->src        << 0
		);
}

void datalb_config(struct data_lb *datalb)
{
	audiobus_write(
		EE_AUDIO_LB_CTRL1,
		datalb->config->ext_signed  << 29 |
		(datalb->config->chnum - 1) << 24 |
		datalb->config->chmask      << 16 |
		datalb->ddr_type            << 13 |
		datalb->msb                 << 8  |
		datalb->lsb                 << 3
		);
}

void datalb_ctrl(struct loopback_cfg *lb_cfg)
{
	int id = lb_cfg->datalb_src;
	int offset = 0;
	int reg, reg_base;

	audiobus_update_bits(
		EE_AUDIO_TDMIN_LB_CTRL,
		0xf << 28 | 0xf << 20 | 0x7 << 16 | 0x1f << 0,
		1 << 31 |
		/*0:tdm mode; 1: i2s mode;*/
		1 << 30 |
		1 << 29 |
		1 << 28 |
		lb_cfg->datalb_src << 20 |
		3 << 16|
		31 << 0
		);

	if (id >= 0 && id <= 2) {
		/* tdmout_a, tdmout_b, tdmout_c */
		reg_base = EE_AUDIO_TDMOUT_A_SWAP0;
		offset = EE_AUDIO_TDMOUT_B_SWAP0 - EE_AUDIO_TDMOUT_A_SWAP0;
	} else if (id < 6) {
		/*lb_cfg->datalb_src for pad tdm in,
		 *pad from tdmin_a, tdmin_b, tdmin_c
		 */
		/* id offset from tdmin_a */
		id -= 3;

		reg_base = EE_AUDIO_TDMIN_A_CTRL;
		offset = EE_AUDIO_TDMIN_B_CTRL - EE_AUDIO_TDMIN_A_CTRL;
		reg = reg_base + offset * id;
		audiobus_update_bits(reg, 3<<28, 0);
		audiobus_update_bits(reg, 1<<29, 1<<29);
		audiobus_update_bits(reg, 1<<28, 1<<28);

		/* just assume lb from tdm in is i2s mode */
		audiobus_update_bits(
			reg,
			0xf << 28 | 0xf << 20 | 0x7 << 16 | 0x1f << 0,
			1 << 31 |
			/* 0:tdm mode; 1: i2s mode */
			1 << 30 |
			1 << 29 |
			1 << 28 |
			id << 20 |
			3 << 16|
			31 << 0
			);

		pr_info("reg:0x%x, EE_AUDIO_TDMIN_A_CTRL:0x%x\n",
			reg,
			audiobus_read(EE_AUDIO_TDMIN_A_CTRL));
		/* swap */
		reg += 1;
		audiobus_write(
			reg,
			lb_cfg->datalb_chswap);

		/* mask 0 */
		reg += 1;
		audiobus_write(
			reg,
			lb_cfg->datalb_chmask);
		reg_base = EE_AUDIO_TDMIN_A_SWAP0;
		offset = EE_AUDIO_TDMIN_B_SWAP0 - EE_AUDIO_TDMIN_A_SWAP0;
	} else {
		pr_err("unsupport datalb_src\n");
		return;
	}

	if (lb_cfg->datain_datalb_total > 8) {
		audiobus_write(
				EE_AUDIO_TDMIN_LB_SWAP0,
				lb_cfg->datalb_chswap);

		audiobus_write(EE_AUDIO_TDMIN_LB_MASK0, 3);
		audiobus_write(EE_AUDIO_TDMIN_LB_MASK1, 3);
		audiobus_write(EE_AUDIO_TDMIN_LB_MASK2, 3);
		audiobus_write(EE_AUDIO_TDMIN_LB_MASK3, 3);
	} else {
		/*swap same as tdmout */
		reg = reg_base + offset * id;
		audiobus_write(EE_AUDIO_TDMIN_LB_SWAP0,
				audiobus_read(reg));

		/*mask same as datalb*/
		/* mask 0 */
		reg += 1;
		audiobus_write(
				EE_AUDIO_TDMIN_LB_MASK0,
				audiobus_read(reg));

		/* mask 1 */
		reg += 1;
		audiobus_write(
				EE_AUDIO_TDMIN_LB_MASK1,
				audiobus_read(reg));

		/* mask 2 */
		reg += 1;
		audiobus_write(
				EE_AUDIO_TDMIN_LB_MASK2,
				audiobus_read(reg));

		/* mask 3 */
		reg += 1;
		audiobus_write(
				EE_AUDIO_TDMIN_LB_MASK3,
				audiobus_read(reg));
	}
}

void lb_mode(int mode)
{
// TODO:
	return;
#if 0
	audiobus_update_bits(
		EE_AUDIO_LB_CTRL0,
		0x1 << 30,
		mode << 30
		);
#endif
}

static void tdmin_lb_clk_enalbe(int tdm_src, int is_enable)
{
	if (tdm_src <= 2)
		audiobus_update_bits(
			EE_AUDIO_CLK_TDMIN_LB_CTRL,
			0x3 << 30 | 1 << 29 | 0xf << 24 | 0xf << 20,
			0x3 << 30 | 1 << 29 | tdm_src << 24 | tdm_src << 20
			);
	else
		pr_warn_once("pad from tdmin_a, tdmin_b, tdmin_c needs clks\n");
}

void tdmin_lb_enable(int tdm_index, int is_enable)
{
	if (is_enable)
		tdmin_lb_clk_enalbe(tdm_index, is_enable);

	audiobus_update_bits(
		EE_AUDIO_TDMIN_LB_CTRL,
		0x1 << 31,
		is_enable << 31);
}

void tdmin_lb_fifo_enable(int is_enable)
{
	if (is_enable) {
		audiobus_update_bits(EE_AUDIO_TDMIN_LB_CTRL, 1<<29, 1<<29);
		audiobus_update_bits(EE_AUDIO_TDMIN_LB_CTRL, 1<<28, 1<<28);
	} else
		audiobus_update_bits(EE_AUDIO_TDMIN_LB_CTRL, 3<<28, 0);
}


void lb_set_datain_src(int src)
{
}

void lb_set_tdminlb_src(int src)
{
}

void lb_set_tdminlb_enable(bool is_enable)
{
}

int lb_is_enable(void)
{
	return (audiobus_read(EE_AUDIO_LB_CTRL0) & 0x80000000) >> 31;
}

void lb_enable(bool is_enable)
{
	audiobus_update_bits(
		EE_AUDIO_LB_CTRL0,
		0x1           << 31,
		is_enable     << 31
		);
}

void lb_set_mode(int mode)
{
	audiobus_update_bits(
		EE_AUDIO_LB_CTRL0,
		0x1           << 30,
		mode          << 30
		);
}

