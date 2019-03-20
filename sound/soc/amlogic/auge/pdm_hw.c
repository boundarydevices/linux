/*
 * sound/soc/amlogic/auge/pdm_hw.c
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

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/string.h>
/*#include <linux/delay.h>*/

#include "pdm_hw.h"
#include "regs.h"
#include "iomap.h"
#include "pdm_hw_coeff.c"

void pdm_enable(int is_enable)
{
	if (is_enable) {
		aml_pdm_update_bits(
			PDM_CTRL,
			0x1 << 31,
			is_enable << 31);
	} else {
		aml_pdm_update_bits(
			PDM_CTRL,
			0x1 << 31 | 0x1 << 16,
			0 << 31 | 0 << 16);

		/*udelay(1000);*/
	}
}

void pdm_fifo_reset(void)
{
	/* PDM Asynchronous FIFO soft reset.
	 * write 1 to soft reset AFIFO
	 */
	aml_pdm_update_bits(
		PDM_CTRL,
		0x1 << 16,
		0 << 16);

	aml_pdm_update_bits(
		PDM_CTRL,
		0x1 << 16,
		0x1 << 16);
}

void aml_pdm_ctrl(struct pdm_info *info)
{
	int mode, i, ch_mask = 0, sample_count;
	int pdm_chs, lane_chs = 0;

	if (!info)
		return;

	/* sameple count */
	if (info->dclk_idx == 1)
		sample_count = 38;
	else if (info->dclk_idx == 2)
		sample_count = 48;
	else
		sample_count = 18;

	if (info->bitdepth == 32)
		mode = 0;
	else
		mode = 1;

	/* update pdm channels for loopback */
	pdm_chs = info->channels;
	if (info->channels > PDM_CHANNELS_MAX)
		pdm_chs = PDM_CHANNELS_MAX;

	if (pdm_chs > info->lane_masks * 2)
		pr_warn("capturing channels more than lanes carried\n");

	/* each lanes carries two channels */
	for (i = 0; i < PDM_LANE_MAX; i++)
		if ((1 << i) & info->lane_masks) {
			ch_mask |= (1 << (2 * i));
			lane_chs += 1;
			if (lane_chs >= info->channels)
				break;
			ch_mask |= (1 << (2 * i + 1));
			lane_chs += 1;
			if (lane_chs >= info->channels)
				break;
		}

	pr_info("%s, lane mask:0x%x, channels:%d, channels mask:0x%x, bypass:%d\n",
		__func__,
		info->lane_masks,
		info->channels,
		ch_mask,
		info->bypass);

	aml_pdm_write(PDM_CLKG_CTRL, 1);

	/* must be sure that clk and pdm is enable */
	aml_pdm_update_bits(PDM_CTRL,
				(0x7 << 28 | 0xff << 8 | 0xff << 0),
				/*(1 << 31) |*/
				/* invert the PDM_DCLK or not */
				(0 << 30) |
				/* output mode:  1: 24bits. 0: 32 bits */
				(mode << 29) |
				/* bypass mode.
				 * 1: bypass all filter. 0: normal mode.
				 */
				(info->bypass << 28) |
				/* PDM channel reset. */
				(ch_mask << 8) |
				/* PDM channel enable */
				(ch_mask << 0)
				);

	aml_pdm_write(PDM_CHAN_CTRL, ((sample_count << 24) |
					(sample_count << 16) |
					(sample_count << 8) |
					(sample_count << 0)
		));
	aml_pdm_write(PDM_CHAN_CTRL1, ((sample_count << 24) |
					(sample_count << 16) |
					(sample_count << 8) |
					(sample_count << 0)
		));
}

void aml_pdm_arb_config(struct aml_audio_controller *actrl)
{
	/* config ddr arb */
	aml_audiobus_write(actrl, EE_AUDIO_ARB_CTRL, 1<<31|0xff<<0);
}

/* config for hcic, lpf1,2,3, hpf */
static void aml_pdm_filters_config(int osr,
	int lpf1_len, int lpf2_len, int lpf3_len)
{
	int32_t hcic_dn_rate;
	int32_t hcic_tap_num;
	int32_t hcic_gain;
	int32_t hcic_shift;
	int32_t f1_tap_num;
	int32_t f2_tap_num;
	int32_t f3_tap_num;
	int32_t f1_rnd_mode;
	int32_t f2_rnd_mode;
	int32_t f3_rnd_mode;
	int32_t f1_ds_rate;
	int32_t f2_ds_rate;
	int32_t f3_ds_rate;
	int32_t hpf_en;
	int32_t hpf_shift_step;
	int32_t hpf_out_factor;
	int32_t pdm_out_mode;

	switch (osr) {
	case 32:
		hcic_dn_rate = 0x4;
		hcic_gain	 = 0x80;
		hcic_shift	 = 0xe;
		break;
	case 40:
		hcic_dn_rate = 0x5;
		hcic_gain	 = 0x6b;
		hcic_shift	 = 0x10;
		break;
	case 48:
		hcic_dn_rate = 0x6;
		hcic_gain	 = 0x78;
		hcic_shift	 = 0x12;
		break;
	case 56:
		hcic_dn_rate = 0x7;
		hcic_gain	 = 0x51;
		hcic_shift	 = 0x13;
		break;
	case 64:
		hcic_dn_rate = 0x0008;
		hcic_gain	 = 0x80;
		hcic_shift	 = 0x15;
		break;
	case 96:
		hcic_dn_rate = 0x000c;
		hcic_gain	 = 0x78;
		hcic_shift	 = 0x19;
		break;
	case 128:
		hcic_dn_rate = 0x0010;
		hcic_gain	 = 0x80;
		hcic_shift	 = 0x1c;
		break;
	case 192:
		hcic_dn_rate = 0x0018;
		hcic_gain	 = 0x78;
		hcic_shift	 = 0x20;
		break;
	case 384:
		hcic_dn_rate = 0x0030;
		hcic_gain	 = 0x78;
		hcic_shift	 = 0x27;
		break;
	default:
		pr_info("Not support osr:%d, translate to :192\n", osr);
		hcic_dn_rate = 0x0018;
		hcic_gain	 = 0x78;
		hcic_shift	 = 0x20;
		break;
	}

	/* TODO: fixed hcic_shift 'cause of Dmic */
	if (pdm_hcic_shift_gain)
		hcic_shift -= 0x4;


	hcic_tap_num = 0x0007;
	f1_tap_num	 = lpf1_len;
	f2_tap_num	 = lpf2_len;
	f3_tap_num	 = lpf3_len;
	hpf_shift_step = 0xd;
	hpf_en		 = 1;
	pdm_out_mode	 = 0;
	hpf_out_factor = 0x8000;
	f1_rnd_mode  = 1;
	f2_rnd_mode  = 0;
	f3_rnd_mode  = 1;
	f1_ds_rate	 = 2;
	f2_ds_rate	 = 2;
	f3_ds_rate	 = 2;

	/* hcic */
	aml_pdm_write(PDM_HCIC_CTRL1,
		(0x80000000 |
		hcic_tap_num |
		(hcic_dn_rate << 4) |
		(hcic_gain << 16) |
		(hcic_shift << 24))
		);

	/* lpf */
	aml_pdm_write(PDM_F1_CTRL,
		(0x80000000 |
		f1_tap_num |
		(2 << 12) |
		(f1_rnd_mode << 16))
		);
	aml_pdm_write(PDM_F2_CTRL,
		(0x80000000 |
		f2_tap_num |
		(2 << 12) |
		(f2_rnd_mode << 16))
		);
	aml_pdm_write(PDM_F3_CTRL,
		(0x80000000 |
		f3_tap_num |
		(2 << 12) |
		(f3_rnd_mode << 16))
		);

	/* hpf */
	aml_pdm_write(PDM_HPF_CTRL,
		(hpf_out_factor |
		(hpf_shift_step << 16) |
		(hpf_en << 31))
		);

}

/* coefficent for LPF1,2,3 */
static void aml_pdm_LPF_coeff(
	int lpf1_len, const int *lpf1_coeff,
	int lpf2_len, const int *lpf2_coeff,
	int lpf3_len, const int *lpf3_coeff)
{
	int i;
	int32_t data;
	int32_t data_tmp;

	aml_pdm_write(PDM_COEFF_ADDR, 0);
	for (i = 0;
		i < lpf1_len;
		i++)
		aml_pdm_write(PDM_COEFF_DATA, lpf1_coeff[i]);
	for (i = 0;
		i < lpf2_len;
		i++)
		aml_pdm_write(PDM_COEFF_DATA, lpf2_coeff[i]);
	for (i = 0;
		i < lpf3_len;
		i++)
		aml_pdm_write(PDM_COEFF_DATA, lpf3_coeff[i]);

	aml_pdm_write(PDM_COEFF_ADDR, 0);
	for (i = 0; i < lpf1_len; i++) {
		data = aml_pdm_read(PDM_COEFF_DATA);
		data_tmp = lpf1_coeff[i];
		if (data != data_tmp) {
			pr_info("FAILED coeff data verified wrong!\n");
			pr_info("Coeff = %x\n", data);
			pr_info("DDR COEFF = %x\n", data_tmp);
		}
	}
	for (i = 0; i < lpf2_len; i++) {
		data = aml_pdm_read(PDM_COEFF_DATA);
		data_tmp = lpf2_coeff[i];
		if (data != data_tmp) {
			pr_info("FAILED coeff data verified wrong!\n");
			pr_info("Coeff = %x\n", data);
			pr_info("DDR COEFF = %x\n", data_tmp);
		}
	}
	for (i = 0; i < lpf3_len; i++) {
		data = aml_pdm_read(PDM_COEFF_DATA);
		data_tmp = lpf3_coeff[i];
		if (data != data_tmp) {
			pr_info("FAILED coeff data verified wrong!\n");
			pr_info("Coeff = %x\n", data);
			pr_info("DDR COEFF = %x\n", data_tmp);
		}
	}

}

void aml_pdm_filter_ctrl(int osr, int mode)
{
	int lpf1_len, lpf2_len, lpf3_len;
	const int *lpf1_coeff, *lpf2_coeff, *lpf3_coeff;

	pr_info("%s, osr:%d, mode:%d\n",
		__func__, osr, mode);

	/* select LPF coefficent
	 * For filter 1 and filter 3,
	 * it's only relative with coefficent mode
	 * For filter 2,
	 * it's only relative with osr and hcic stage number
	 */

	/* mode and its latency
	 *	mode	|	latency
	 *	0		|	47.7
	 *	1		|	38.7
	 *	2		|	26
	 *	3		|	20.4
	 *	4		|	15
	 */

	switch (osr) {
	case 64:
		lpf2_coeff = lpf2_osr64;
		break;
	case 96:
		lpf2_coeff = lpf2_osr96;
		break;
	case 128:
		lpf2_coeff = lpf2_osr128;
		break;
	case 192:
		lpf2_coeff = lpf2_osr192;
		break;
	case 256:
		lpf2_coeff = lpf2_osr256;
		break;
	case 384:
		lpf2_coeff = lpf2_osr384;
		break;
	case 32:
	case 40:
	case 48:
	case 56:
	default:
		pr_info("osr :%d , lpf2 uses default parameters with osr64\n",
			osr);
		lpf2_coeff = lpf2_osr64;
		break;
	}

	switch (mode) {
	case 0:
		lpf1_len = 110;
		lpf2_len = 33;
		lpf3_len = 147;
		lpf1_coeff = lpf1_mode0;
		lpf3_coeff = lpf3_mode0;
		break;
	case 1:
		lpf1_len = 87;
		lpf2_len = 33;
		lpf3_len = 117;
		lpf1_coeff = lpf1_mode1;
		lpf3_coeff = lpf3_mode1;
		break;
	case 2:
		lpf1_len = 87;
		lpf2_len = 33;
		lpf3_len = 66;
		lpf1_coeff = lpf1_mode2;
		lpf3_coeff = lpf3_mode2;
		break;
	case 3:
		lpf1_len = 65;
		lpf2_len = 33;
		lpf3_len = 49;
		lpf1_coeff = lpf1_mode3;
		lpf3_coeff = lpf3_mode3;
		break;
	case 4:
		lpf1_len = 43;
		lpf2_len = 33;
		lpf3_len = 32;
		lpf1_coeff = lpf1_mode4;
		lpf3_coeff = lpf3_mode4;
		break;
	default:
		pr_info("default mode 1, osr 64\n");
		lpf1_len = 87;
		lpf2_len = 33;
		lpf3_len = 117;
		lpf1_coeff = lpf1_mode1;
		lpf3_coeff = lpf3_mode1;
		break;
	}

	/* config filter */
	aml_pdm_filters_config(osr,
		lpf1_len,
		lpf2_len,
		lpf3_len);

	aml_pdm_LPF_coeff(
		lpf1_len, lpf1_coeff,
		lpf2_len, lpf2_coeff,
		lpf3_len, lpf3_coeff
		);

}

int pdm_get_mute_value(void)
{
	return aml_pdm_read(PDM_MUTE_VALUE);
}

void pdm_set_mute_value(int val)
{
	aml_pdm_write(PDM_MUTE_VALUE, val);
}

int pdm_get_mute_channel(void)
{
	int val = aml_pdm_read(PDM_CTRL);

	return (val & (0xff << 20));
}

void pdm_set_mute_channel(int mute_chmask)
{
	int mute_en = 0;

	if (mute_chmask)
		mute_en = 1;

	aml_pdm_update_bits(PDM_CTRL,
		(0xff << 20 | 0x1 << 17),
		(mute_chmask << 20 | mute_en << 17));
}

void pdm_init_truncate_data(int freq)
{
	int mask_val;

	/* assume mask 1.05ms */
	mask_val = ((freq / 1000) * 1050) / 1000 - 1;

	aml_pdm_write(PDM_MASK_NUM, mask_val);
}

void pdm_train_en(bool en)
{
	aml_pdm_update_bits(PDM_CTRL,
		0x1 << 19,
		en << 19);
}

void pdm_train_clr(void)
{
	aml_pdm_update_bits(PDM_CTRL,
		0x1 << 18,
		0x1 << 18);
}

int pdm_train_sts(void)
{
	int val = aml_pdm_read(PDM_STS);

	return ((val >> 4) & 0xff);
}
