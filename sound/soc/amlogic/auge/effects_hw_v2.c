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

void aed_set_ram_coeff(int add, int len, unsigned int *params)
{
	int i, ctrl_v;
	unsigned int *p = params;

	for (i = 0; i < len; i++, p++) {
		ctrl_v = ((add+i) << 2) | (0x1 << 1) | (0x1 << 0);
		eqdrc_write(AED_COEF_RAM_DATA, *p);
		eqdrc_write(AED_COEF_RAM_CNTL, ctrl_v);
	}
}

void aed_get_ram_coeff(int add, int len, unsigned int *params)
{
	int i, ctrl_v;
	unsigned int *p = params;

	for (i = 0; i < len; i++, p++) {
		ctrl_v = ((add+i) << 2) | (0x0 << 1) | (0x1 << 0);
		eqdrc_write(AED_COEF_RAM_CNTL, ctrl_v);
		*p = eqdrc_read(AED_COEF_RAM_DATA);
		//pr_info("%s, params[%d] = %8.8x\n", __func__, i, *p);
	}
}

void aed_set_multiband_drc_coeff(int band, unsigned int *params)
{
	int i;
	int offset = AED_MDRC_RMS_COEF10 - AED_MDRC_RMS_COEF00;
	int reg = AED_MDRC_RMS_COEF00;

	for (i = 0; i < offset; i++) {
		eqdrc_write(reg + band * offset + i,
			params[band * offset + i]);
	}
}

void aed_get_multiband_drc_coeff(int band, unsigned int *params)
{
	int i;
	int offset = AED_MDRC_RMS_COEF10 - AED_MDRC_RMS_COEF00;
	int reg = AED_MDRC_RMS_COEF00;

	for (i = 0; i < offset; i++) {
		*(params + band * offset + i) =
			eqdrc_read(reg + band * offset + i);
	}
}

void aed_set_fullband_drc_coeff(int group, unsigned int *params)
{
	unsigned int *p = params;

	if (group == 0) {
		eqdrc_write(AED_DRC_RELEASE_COEF00, *p++);
		eqdrc_write(AED_DRC_RELEASE_COEF01, *p++);
		eqdrc_write(AED_DRC_ATTACK_COEF00, *p++);
		eqdrc_write(AED_DRC_ATTACK_COEF01, *p++);
		eqdrc_write(AED_DRC_THD0, *p++);
		eqdrc_write(AED_DRC_K0, *p++);
	} else if (group == 1) {
		eqdrc_write(AED_DRC_RELEASE_COEF10, *p++);
		eqdrc_write(AED_DRC_RELEASE_COEF11, *p++);
		eqdrc_write(AED_DRC_ATTACK_COEF10, *p++);
		eqdrc_write(AED_DRC_ATTACK_COEF11, *p++);
		eqdrc_write(AED_DRC_THD1, *p++);
		eqdrc_write(AED_DRC_K2, *p++);
		eqdrc_write(AED_DRC_THD_OUT0, eqdrc_read(AED_DRC_THD1));
	} else if (group == 2) {
		eqdrc_write(AED_DRC_RMS_COEF0, *p++);
		eqdrc_write(AED_DRC_RMS_COEF1, *p++);
		eqdrc_write(AED_DRC_LOOPBACK_CNTL, *p++);
		/*THD_OUT0 = THD1; K1 = 1.0*/
		eqdrc_write(AED_DRC_THD_OUT0, eqdrc_read(AED_DRC_THD1));
		eqdrc_write(AED_DRC_K1, 0x40000);
	}
}

void aed_get_fullband_drc_coeff(int len, unsigned int *params)
{
	unsigned int *p = params;

	*p++ = eqdrc_read(AED_DRC_RELEASE_COEF00);
	*p++ = eqdrc_read(AED_DRC_RELEASE_COEF01);
	*p++ = eqdrc_read(AED_DRC_ATTACK_COEF00);
	*p++ = eqdrc_read(AED_DRC_ATTACK_COEF01);
	*p++ = eqdrc_read(AED_DRC_THD0);
	*p++ = eqdrc_read(AED_DRC_K0);

	*p++ = eqdrc_read(AED_DRC_RELEASE_COEF10);
	*p++ = eqdrc_read(AED_DRC_RELEASE_COEF11);
	*p++ = eqdrc_read(AED_DRC_ATTACK_COEF10);
	*p++ = eqdrc_read(AED_DRC_ATTACK_COEF11);
	*p++ = eqdrc_read(AED_DRC_THD1);
	*p++ = eqdrc_read(AED_DRC_K2);

	*p++ = eqdrc_read(AED_DRC_RMS_COEF0);
	*p++ = eqdrc_read(AED_DRC_RMS_COEF1);
	*p++ = eqdrc_read(AED_DRC_LOOPBACK_CNTL);
	*p++ = eqdrc_read(AED_DRC_THD_OUT0);
	*p++ = eqdrc_read(AED_DRC_K1);
}

void aed_set_mixer_params(void)
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
}

void aed_eq_taps(unsigned int eq1_taps)
{
	if (eq1_taps > 20) {
		pr_err("Error EQ1_Tap = %d\n", eq1_taps);
		return;
	}
	eqdrc_update_bits(AED_EQ_TAP_CNTL, 0x1f, eq1_taps);
	eqdrc_update_bits(AED_EQ_TAP_CNTL, 0x1f << 5, (20 - eq1_taps) << 5);
}

void aed_set_multiband_drc_param(void)
{
	eqdrc_update_bits(AED_MDRC_CNTL,
		(0x1 << 16) | (0x7 << 3) | (0x7 << 0),
		(0x0 << 16) | (0x7 << 3) | (0x7 << 0));
}

void aed_set_fullband_drc_param(int tap)
{
	eqdrc_update_bits(AED_DRC_CNTL,
		(0x7 << 3), (tap << 3));
}

void aed_set_multiband_drc_enable(bool enable)
{
	eqdrc_update_bits(AED_MDRC_CNTL,
		(0x1 << 8), (enable << 8));
}

void aed_set_fullband_drc_enable(bool enable)
{
	eqdrc_update_bits(AED_DRC_CNTL,
		(0x1 << 0), (enable << 0));
}

void aed_set_volume(
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
	eqdrc_write(AED_EQ_VOLUME_SLEW_CNT, 0x2); /*40ms from -120dB~0dB*/
	eqdrc_write(AED_MUTE, 0);
}

void aed_set_lane_and_channels(int lane_mask, int ch_mask)
{
	int ch_num = 0, i = 0;
	int val = ch_mask & 0xff;

	eqdrc_update_bits(AED_TOP_CTL,
		0xff << 18 | 0xf << 14,
		ch_mask << 18 | lane_mask << 14);

	for (i = 0; i < 8; i++) {
		if ((val & 0x1) == 0x1)
			ch_num++;
		val >>= 1;
	}

	eqdrc_update_bits(AED_TOP_REQ_CTL,
		0xff << 12, (ch_num - 1) << 12);
}

void aed_set_ctrl(bool enable, int sel, enum frddr_dest module)
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
	if (module <= TDMOUT_C && module >= TDMOUT_A) {
		/* TDMOUT A/B/C */
		aml_tdmout_select_aed(enable, module);
	} else {
		/* SPDIFOUT A/B */
		aml_spdifout_select_aed(enable, module - SPDIFOUT_A);
	}

}

void aed_set_format(int msb, enum ddr_types frddr_type, enum ddr_num source)
{
	eqdrc_update_bits(AED_TOP_CTL,
		0x7 << 11 | 0x1f << 6 | 0x3 << 4,
		frddr_type << 11 | msb << 6 | source << 4);
}

void aed_enable(bool enable)
{
	if (enable) {
		eqdrc_write(AED_ED_CNTL, 0x1);
		eqdrc_write(AED_ED_CNTL, 0x0);

		eqdrc_update_bits(AED_TOP_CTL, 0x1 << 1, 0x1 << 1);
		eqdrc_update_bits(AED_TOP_CTL, 0x1 << 2, 0x1 << 2);
	} else
		eqdrc_update_bits(AED_TOP_CTL, 0x3 << 1, 0x0 << 1);

	eqdrc_update_bits(AED_TOP_CTL, 0x1 << 0, enable << 0);

	/* start en */
	if (enable)
		eqdrc_update_bits(AED_TOP_CTL, 0x1 << 31, 0x1 << 31);
}

