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
	audiobus_write(
		EE_AUDIO_LB_CTRL0,
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

void datalb_ctrl(int lb_src, int bitdepth)
{
	//tdmin lb,  same as tdm out
	audiobus_update_bits(
		EE_AUDIO_CLK_TDMIN_LB_CTRL,
		0x3 << 30 | 0 << 29 | 0xf << 24 | 0xf << 20,
		0x3 << 30 | 0 << 29 | 2 << 24 | 2 << 20
		);
	//tdmin ctrl, from tdmout
	//reset
	audiobus_update_bits(EE_AUDIO_TDMIN_LB_CTRL, 3<<28, 0);
	audiobus_update_bits(EE_AUDIO_TDMIN_LB_CTRL, 1<<29, 1<<29);
	audiobus_update_bits(EE_AUDIO_TDMIN_LB_CTRL, 1<<28, 1<<28);

	audiobus_write(
		EE_AUDIO_TDMIN_LB_CTRL,
		1 << 31 |
		0 << 30 | /*0:tdm mode; 1: i2s mode;*/
		1 << 29 |
		1 << 28 |
		lb_src << 20|
		4 << 16|
		(bitdepth - 1) << 0
		);
	audiobus_write(
		EE_AUDIO_TDMIN_LB_MASK0,
		0xff);

}

void lb_enable_ex(int mode, bool is_enable)
{
	audiobus_update_bits(
		EE_AUDIO_LB_CTRL0,
		0x3 << 30,
		is_enable << 31 |
		mode << 30
		);
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

void lb_enable(bool is_enable)
{
	if (is_enable)
		audiobus_update_bits(
			EE_AUDIO_LB_CTRL0,
			0x1           << 31,
			is_enable     << 31
			);
	else
		audiobus_write(
			EE_AUDIO_LB_CTRL0,
			0);
}

void lb_set_mode(int mode)
{
	audiobus_update_bits(
		EE_AUDIO_LB_CTRL0,
		0x1           << 30,
		mode          << 30
		);
}

