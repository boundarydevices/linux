/*
 * sound/soc/amlogic/auge/effect_hw.c
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
#include <linux/delay.h>

#include "effects_hw.h"
#include "tdm_hw.h"
#include "spdif_hw.h"

int DRC0_enable(int enable, int thd0, int k0)
{
	if (enable == 1) {
		eqdrc_write(AED_DRC_THD0_G12X, thd0/*aml_drc_tko_table[2]*/);
		eqdrc_write(AED_DRC_K0_G12X, k0/*aml_drc_tko_table[4]*/);
	} else {
		eqdrc_write(AED_DRC_THD0_G12X, 0xbf000000);
		eqdrc_write(AED_DRC_K0_G12X, 0x40000);
	}

	return 0;
}

int init_EQ_DRC_module(void)
{
	eqdrc_write(AED_ED_CTL, 1); /* soft reset*/
	msleep(20);
	eqdrc_write(AED_ED_CTL, 0); /* soft reset*/

	eqdrc_write(AED_NG_CTL, (3 << 30)); /* disable noise gate*/

	return 0;
}

int set_internal_EQ_volume(
	unsigned int master_volume,
	unsigned int channel_1_volume,
	unsigned int channel_2_volume)
{
	eqdrc_write(AED_EQ_VOLUME_G12X, (0 << 30) /* volume step: 0.125dB*/
			| (master_volume << 16) /* master volume: 0dB*/
			| (channel_1_volume << 8) /* channel 1 volume: 0dB*/
			| (channel_2_volume << 0) /* channel 2 volume: 0dB*/
			);
	eqdrc_write(AED_EQ_VOLUME_SLEW_CNT_G12X, 0x40);
	eqdrc_write(AED_MUTE_G12X, 0);

	return 0;
}

void aed_req_sel(bool enable, int sel, int req_module)
{
	int mask_offset, val_offset;

	switch (sel) {
	case 0: /* REQ_SEL0 */
		mask_offset = 0x1 << 3 | 0x7 << 0;
		val_offset = enable << 3 | req_module << 0;
		break;
	case 1: /* REQ_SEL1 */
		mask_offset = 0x1 << 7 | 0x7 << 4;
		val_offset = enable << 7 | req_module << 4;
		break;
	case 2: /* REQ_SEL2 */
		mask_offset = 0x1 << 11 | 0x7 << 8;
		val_offset = enable << 11 | req_module << 8;
		break;
	default:
		pr_err("unknown AED req_sel:%d\n", sel);
		return;
	}

	eqdrc_update_bits(AED_TOP_REQ_CTL_G12X, mask_offset, val_offset);
}

/* get eq/drc module */
int aed_get_req_sel(int sel)
{
	int val = eqdrc_read(AED_TOP_REQ_CTL_G12X);
	int mask_off;

	switch (sel) {
	case 0: /* REQ_SEL0 */
		mask_off = 0;
		break;
	case 1: /* REQ_SEL1 */
		mask_off = 4;
		break;
	case 2: /* REQ_SEL2 */
		mask_off = 8;
		break;
	default:
		pr_err("unknown AED req_sel:%d\n", sel);
		return -EINVAL;
	}

	return (val >> mask_off) & 0x7;
}

void aed_set_eq(int enable, int params_len, unsigned int *params)
{
	if (enable) {
		unsigned int *param_ptr = &params[0];
		int i;

		for (i = 0; i < params_len; i++) {
			eqdrc_write(AED_EQ_CH1_COEF00 + i, *param_ptr);
			/*pr_info("EQ value[%d]: 0x%x\n", i, *param_ptr);*/
			param_ptr++;
		}
	}

	eqdrc_update_bits(AED_EQ_EN_G12X, 1, enable);
}

void aed_set_drc(int enable, int drc_len, unsigned int *drc_params,
	int drc_tko_len, unsigned int *drc_tko_params)
{
	if (enable) {
		unsigned int *param_ptr;
		int i;

		param_ptr = &drc_params[0];
		for (i = 0; i < drc_len; i++) {
			eqdrc_write(AED_DRC_AE + i, *param_ptr);
			/* pr_info("DRC table value[%d]: 0x%x\n",
			 * i, *param_ptr);
			 */
			param_ptr++;
		}

		param_ptr = &drc_tko_params[0];
		for (i = 0; i < drc_tko_len; i++) {
			eqdrc_write(AED_DRC_OFFSET0 + i, *param_ptr);
			/*pr_info("DRC tko value[%d]: 0x%x\n", i, *param_ptr);*/
			param_ptr++;
		}
	}
	eqdrc_update_bits(AED_DRC_EN, 1, enable);
}

int aml_aed_format_set(int frddr_dst)
{
	int width = -1, frddr_type = -1;

	if (frddr_dst >= 3) {
		/* SPDIFOUT A/B */
		aml_spdifout_get_aed_info(frddr_dst - 3, &width, &frddr_type);
	} else if (frddr_dst < 3 && frddr_dst >= 0) {
		/* TDMOUT A/B/C */
		aml_tdmout_get_aed_info(frddr_dst, &width, &frddr_type);
	} else
		pr_err("unknown function for AED\n");

	if (width < 0 || frddr_type < 0) {
		pr_err(" Fetch wrong info for AED\n");

		return -EINVAL;
	}

	eqdrc_update_bits(AED_TOP_CTL_G12X, 0x7 << 11 | 0x1f << 6,
		frddr_type << 11 | width << 6);

	return 0;
}

void aed_src_select(bool enable, int frddr_dst, int fifo_id)
{
	/* Effect Module */
	if (frddr_dst >= 3) {
		/* SPDIFOUT A/B */
		aml_spdifout_select_aed(enable, frddr_dst - 3);
	} else if (frddr_dst < 3 && frddr_dst >= 0) {
		/* TDMOUT A/B/C */
		aml_tdmout_select_aed(enable, frddr_dst);
	} else
		pr_err("unknown function for AED\n");

	/* AED module, req */
	aed_req_sel(enable, 0, frddr_dst);

	/* AED module, sel & enable */
	eqdrc_update_bits(AED_TOP_CTL_G12X,
		0x3 << 4 | 0x1 << 0,
		fifo_id << 4 | enable << 0);
}

void aed_set_lane(int lane_mask)
{
	eqdrc_update_bits(AED_TOP_CTL_G12X, 0xf << 14, lane_mask << 14);
}

void aed_set_channel(int channel_mask)
{
	eqdrc_update_bits(AED_TOP_CTL_G12X, 0xff << 18, channel_mask << 18);
}
