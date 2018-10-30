/*
 * sound/soc/amlogic/auge/effect_hw_v2.c
 *
 * Copyright (C) 2018 Amlogic, Inc. All rights reserved.
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
#include <linux/kernel.h>

#include "effects_hw_v2.h"
#include "regs.h"
#include "iomap.h"

#include "tdm_hw.h"
#include "spdif_hw.h"

void aed_set_ram_coeff(int len, int *params)
{
	int i, ctrl_v;

	for (i = 0; i < len; i++) {
		ctrl_v = (i << 2) | (0x1 << 1) | (0x1 << 0);
		eqdrc_write(AED_COEF_RAM_DATA, params[i]);
		eqdrc_write(AED_COEF_RAM_CNTL, ctrl_v);
	}
}

void aed_set_multiband_drc_coeff(int len, int *params)
{
	int band_len = len / 3, i, j;
	int offset = AED_MDRC_RMS_COEF10 - AED_MDRC_RMS_COEF00;
	int reg = AED_MDRC_RMS_COEF00;

	for (i = 0; i < 3; i++)
		for (j = 0; j < band_len; j++)
			eqdrc_write(reg + i * offset + j,
				params[i * band_len + j]);

	eqdrc_write(AED_MDRC_THD0, 0xf6000000);
	eqdrc_write(AED_MDRC_K0, 0x20000);
	eqdrc_write(AED_MDRC_OFFSET0, 0x200);
	eqdrc_write(AED_MDRC_LOW_GAIN, 0x40000);

	eqdrc_write(AED_MDRC_THD1, 0xfb000000);
	eqdrc_write(AED_MDRC_K1, 0x26666);
	eqdrc_write(AED_MDRC_OFFSET1, 0x200);
	eqdrc_write(AED_MDRC_MID_GAIN, 0x40000);

	eqdrc_write(AED_MDRC_THD2, 0xf1000000);
	eqdrc_write(AED_MDRC_K2, 0x2cccc);
	eqdrc_write(AED_MDRC_OFFSET2, 0x200);
	eqdrc_write(AED_MDRC_HIGH_GAIN, 0x40000);
}

void aed_set_fullband_drc_coeff(int len, int *params)
{
	int i;

	for (i = 0; i < len; i++)
		eqdrc_write(AED_DRC_RELEASE_COEF00 + i,
			params[i]);

	eqdrc_write(AED_DRC_RMS_COEF0, 0x34ebb);
	eqdrc_write(AED_DRC_RMS_COEF1, 0x7cb145);
	eqdrc_write(AED_DRC_THD0, 0xf7000000);
	eqdrc_write(AED_DRC_THD1, 0xf6000000);
	eqdrc_write(AED_DRC_THD2, 0xec000000);
	eqdrc_write(AED_DRC_THD3, 0xe2000000);
	eqdrc_write(AED_DRC_THD4, 0xce000000);
	eqdrc_write(AED_DRC_K0, 0x20000);
	eqdrc_write(AED_DRC_K1, 0x46666);
	eqdrc_write(AED_DRC_K2, 0x40000);
	eqdrc_write(AED_DRC_K3, 0x39999);
	eqdrc_write(AED_DRC_K4, 0x33333);
	eqdrc_write(AED_DRC_K5, 0x4cccc);
	eqdrc_write(AED_DRC_THD_OUT0, 0xf5e66667);
	eqdrc_write(AED_DRC_THD_OUT1, 0xebe66667);
	eqdrc_write(AED_DRC_THD_OUT2, 0xe2e66667);
	eqdrc_write(AED_DRC_THD_OUT3, 0xd2e66667);
	eqdrc_write(AED_DRC_OFFSET, 0x100);
	eqdrc_write(AED_DRC_LOOPBACK_CNTL, (144 << 0));
}

static void aed_set_mixer_params(void)
{
	eqdrc_write(AED_MIX0_LL, 0x40000);
	eqdrc_write(AED_MIX0_RL, 0x0);
	eqdrc_write(AED_MIX0_LR, 0x0);
	eqdrc_write(AED_MIX0_RR, 0x40000);
	eqdrc_write(AED_CLIP_THD, 0x7fffff);
}

void aed_dc_enable(bool enable)
{
	eqdrc_write(AED_DC_EN, enable << 0);
}

void aed_nd_enable(bool enable)
{
	if (enable) {
		eqdrc_write(AED_ND_LOW_THD, 0x100);
		eqdrc_write(AED_ND_HIGH_THD, 0x200);
		eqdrc_write(AED_ND_CNT_THD, 0x100);
		eqdrc_write(AED_ND_SUM_NUM, 0x200);
		eqdrc_write(AED_ND_CZ_NUM, 0x800);
		eqdrc_write(AED_ND_SUM_THD0, 0x20000);
		eqdrc_write(AED_ND_SUM_THD1, 0x30000);
		eqdrc_write(AED_ND_CZ_THD0, 0x200);
		eqdrc_write(AED_ND_CZ_THD1, 0x100);
		eqdrc_write(AED_ND_COND_CNTL, 0x3f);
		eqdrc_write(AED_ND_RELEASE_COEF0, 0x3263a);
		eqdrc_write(AED_ND_RELEASE_COEF1, 0x7cd9c6);
		eqdrc_write(AED_ND_ATTACK_COEF0, 0x5188);
		eqdrc_write(AED_ND_ATTACK_COEF1, 0x7fae78);
	}

	eqdrc_write(AED_ND_CNTL, (enable << 0)|(3 << 1));
}

void aed_eq_enable(int idx, bool enable)
{
	eqdrc_update_bits(AED_EQ_EN, 0x1 << idx, enable << idx);
	eqdrc_update_bits(AED_EQ_TAP_CNTL, 0x1f << (5 * idx), 10 << (5 * idx));
}

void aed_multiband_drc_enable(bool enable)
{
	eqdrc_write(AED_MDRC_CNTL,
		(1 << 16)     |  /* mdrc_pow_sel */
		(enable << 8) |  /* mdrc_all_en */
		(7 << 3)      |  /* mdrc_rms_mode[2:0] */
		(7 << 0)         /* mdrc_en[2:0] */
	);
}

void aed_fullband_drc_enable(bool enable)
{
	eqdrc_write(AED_DRC_CNTL,
		(5 << 3) |  /* drc_tap */
		(enable << 0)   /* drc_en */
	);
}

void aed_set_EQ_volume(
	unsigned int master_vol,
	unsigned int Lch_vol,
	unsigned int Rch_vol)
{
	eqdrc_write(AED_EQ_VOLUME,
		(0 << 30) |          /* volume step: 0.125dB */
		(master_vol << 16) | /* master volume: 0dB */
		(Rch_vol << 8) |     /* channel 2 volume: 0dB */
		(Lch_vol << 0)       /* channel 1 volume: 0dB */
	);
	eqdrc_write(AED_EQ_VOLUME_SLEW_CNT, 0x40);
	eqdrc_write(AED_MUTE, 0);
}

void aed_set_lane_and_channels(int lane_mask, int ch_mask)
{
	eqdrc_update_bits(AED_TOP_CTL,
		0xff << 18 | 0xf << 14,
		ch_mask << 18 | lane_mask << 14);
}

void aed_set_ctrl(bool enable, int sel, int module)
{
	int mask = 0, val = 0;

	switch (sel) {
	case 0: /* REQ_SEL0 */
		mask = 0xf << 0;
		val = 0x1 << 3 | module << 0;
		break;
	case 1: /* REQ_SEL1 */
		mask = 0xf << 4;
		val = 0x1 << 7 | module << 4;
		break;
	case 2: /* REQ_SEL2 */
		mask = 0xf << 8;
		val = 0x1 << 11 | module << 8;
		break;
	default:
		pr_err("unknown AED req_sel:%d, module:%d\n",
			sel, module);
		return;
	}
	eqdrc_update_bits(AED_TOP_REQ_CTL, mask, val);

	/* Effect Module */
	if (module >= 3) {
		/* SPDIFOUT A/B */
		aml_spdifout_select_aed(enable, module - 3);
	} else if (module < 3 && module >= 0) {
		/* TDMOUT A/B/C */
		aml_tdmout_select_aed(enable, module);
	} else
		pr_err("unknown module:%d for AED\n", module);

}

void aed_set_format(int msb, int frddr_type)
{
	eqdrc_update_bits(AED_TOP_CTL,
		0x7 << 11 | 0x1f << 6,
		frddr_type << 11 | msb << 6);
}

void aed_enable(bool enable, int frddr_dst, int fifo_id)
{
	if (enable) {
		eqdrc_write(AED_ED_CNTL, 0x1);
		eqdrc_write(AED_ED_CNTL, 0x0);

		eqdrc_update_bits(AED_TOP_CTL, 0x1 << 1, 0x1 << 1);
		eqdrc_update_bits(AED_TOP_CTL, 0x1 << 2, 0x1 << 2);

		aed_set_mixer_params();
	} else
		eqdrc_update_bits(AED_TOP_CTL, 0x3 << 1, 0x0 << 1);

	eqdrc_update_bits(AED_TOP_CTL, 0x1 << 0, enable << 0);

	/* start en */
	if (enable)
		eqdrc_update_bits(AED_TOP_CTL, 0x1 << 31, 0x1 << 31);
}
