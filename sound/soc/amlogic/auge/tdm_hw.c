/*
 * sound/soc/amlogic/auge/tdm_hw.c
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

#include "tdm_hw.h"

void aml_tdm_enable(
	struct aml_audio_controller *actrl,
	int stream, int index,
	bool is_enable)
{
	unsigned int offset, reg;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		pr_info("tdm playback enable\n");

		offset = EE_AUDIO_TDMOUT_B_CTRL0
				- EE_AUDIO_TDMOUT_A_CTRL0;
		reg = EE_AUDIO_TDMOUT_A_CTRL0 + offset * index;
		aml_audiobus_update_bits(actrl, reg, 1<<31, is_enable<<31);
	} else {
		pr_info("tdm capture enable\n");

		offset = EE_AUDIO_TDMIN_B_CTRL
				- EE_AUDIO_TDMIN_A_CTRL;
		reg = EE_AUDIO_TDMIN_A_CTRL + offset * index;
		aml_audiobus_update_bits(actrl, reg, 1<<31, is_enable<<31);
	}

}

void aml_tdm_arb_config(struct aml_audio_controller *actrl)
{
	/* config ddr arb */
	aml_audiobus_write(actrl, EE_AUDIO_ARB_CTRL, 1<<31|0xff<<0);
}

void aml_tdm_fifo_reset(
	struct aml_audio_controller *actrl,
	int stream, int index)
{
	unsigned int reg, offset;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		offset = EE_AUDIO_TDMOUT_B_CTRL0
				- EE_AUDIO_TDMOUT_A_CTRL0;
		reg = EE_AUDIO_TDMOUT_A_CTRL0 + offset * index;
		/* reset afifo */
		aml_audiobus_update_bits(actrl, reg, 3<<28, 0);
		aml_audiobus_update_bits(actrl, reg, 1<<29, 1<<29);
		aml_audiobus_update_bits(actrl, reg, 1<<28, 1<<28);
	} else {
		offset = EE_AUDIO_TDMIN_B_CTRL
				- EE_AUDIO_TDMIN_A_CTRL;
		reg = EE_AUDIO_TDMIN_A_CTRL + offset * index;
		/* reset afifo */
		aml_audiobus_update_bits(actrl, reg, 3<<28, 0);
		aml_audiobus_update_bits(actrl, reg, 1<<29, 1<<29);
		aml_audiobus_update_bits(actrl, reg, 1<<28, 1<<28);
	}

}

void aml_tdm_fifo_ctrl(
	struct aml_audio_controller *actrl,
	int bitwidth, int stream,
	int index)
{
	unsigned int frddr_type, toddr_type;
	unsigned int reg, offset;

	switch (bitwidth) {
	case 8:
		frddr_type = 0;
		toddr_type = 0;
		break;
	case 16:
		frddr_type = 2;
		toddr_type = 2;
		break;
	case 24:
	case 32:
		frddr_type = 4;
		toddr_type = 4;
		break;
	default:
		pr_err("invalid bit_depth: %d\n",
			bitwidth);
		return;
	}

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		pr_info("tdm prepare----playback\n");
		// from ddr, 63bit split into 2 samples
		offset = EE_AUDIO_TDMOUT_B_CTRL1
				- EE_AUDIO_TDMOUT_A_CTRL1;
		reg = EE_AUDIO_TDMOUT_A_CTRL1 + offset * index;
		aml_audiobus_update_bits(actrl, reg,
				0x3<<24|0x1f<<8|0x7<<4,
				index<<24|(bitwidth-1)<<8|frddr_type<<4);
	} else {
		pr_info("tdm prepare----capture\n");
	}

}

void aml_tdm_set_format(
	struct aml_audio_controller *actrl,
	struct pcm_setting *p_config,
	unsigned int clk_sel,
	unsigned int index,
	unsigned int fmt)
{
	unsigned int binv, finv, id;
	unsigned int valb, valf;
	unsigned int reg_in, reg_out, off_set;
	int bclkin_skew, bclkout_skew;
	int master_mode;

	id = index;

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		valb = SLAVE_A + id;
		valf = SLAVE_A;
		master_mode = 0;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		valb = MASTER_A + clk_sel;
		valf = MASTER_A + clk_sel;
		master_mode = 1;
		break;
	default:
		return;
	}

	//TODO: clk tree
	reg_out = EE_AUDIO_CLK_TDMOUT_A_CTRL + id;
	reg_in = EE_AUDIO_CLK_TDMIN_A_CTRL + id;
	aml_audiobus_update_bits(actrl,
		reg_out,
		0xff<<20,
		valb<<24|valf<<20);
	aml_audiobus_update_bits(actrl,
		reg_in,
		0xff<<20,
		valb<<24|valf<<20);

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		if (master_mode) {
			bclkout_skew = 1;
			bclkin_skew = 3;
		} else {
			bclkout_skew = 2;
			bclkin_skew = 3;
		}
		break;
	case SND_SOC_DAIFMT_DSP_A:
		if (master_mode) {
			bclkout_skew = 1;
			bclkin_skew = 4;
		} else {
			bclkout_skew = 2;
			bclkin_skew = 3;
		}
		break;
	case SND_SOC_DAIFMT_LEFT_J:
	case SND_SOC_DAIFMT_DSP_B:
		//TODO: need test
		bclkout_skew = 2;
		bclkin_skew = 2;
		break;
	default:
		return;
	}

	p_config->pcm_mode = fmt & SND_SOC_DAIFMT_FORMAT_MASK;

	/* set lrclk/bclk invertion */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_IB_IF:
		/* Invert both clocks */
		binv = 1;
		finv = 1;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		/* Invert bit clock */
		binv = 1;
		finv = 0;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		/* Invert frame clock */
		binv = 0;
		finv = 1;
		break;
	case SND_SOC_DAIFMT_NB_NF:
		/* normal cases */
		binv = 0;
		finv = 0;
		break;
	default:
		return;
	}

	pr_info("master_mode(%d), binv(%d), finv(%d) out_skew(%d), in_skew(%d)\n",
			master_mode, binv, finv, bclkout_skew, bclkin_skew);

	/* TDM out */
	reg_out = EE_AUDIO_CLK_TDMOUT_A_CTRL + id;
	aml_audiobus_update_bits(actrl, reg_out,
		0x3<<30|0x1<<29, 0x3<<30/*|binv<<29*/);
	// sclk_ph0 (pad) invert
	off_set = EE_AUDIO_MST_B_SCLK_CTRL1 - EE_AUDIO_MST_A_SCLK_CTRL1;
	reg_out = EE_AUDIO_MST_A_SCLK_CTRL1 + off_set * id;
	aml_audiobus_update_bits(actrl, reg_out, 0x3f, binv);

	off_set = EE_AUDIO_TDMOUT_B_CTRL0 - EE_AUDIO_TDMOUT_A_CTRL0;
	reg_out = EE_AUDIO_TDMOUT_A_CTRL0 + off_set * id;
	aml_audiobus_update_bits(actrl, reg_out, 0x1f<<15, bclkout_skew<<15);

	off_set = EE_AUDIO_TDMOUT_B_CTRL1 - EE_AUDIO_TDMOUT_A_CTRL1;
	reg_out = EE_AUDIO_TDMOUT_A_CTRL1 + off_set * id;
	aml_audiobus_update_bits(actrl, reg_out, 0x1<<28, finv<<28);

	/* TDM in */
	reg_in = EE_AUDIO_CLK_TDMIN_A_CTRL + id;
	aml_audiobus_update_bits(actrl, reg_in,
				0x3<<30|0x1<<29, 0x3<<30|binv<<29);

	off_set = EE_AUDIO_TDMIN_B_CTRL - EE_AUDIO_TDMIN_A_CTRL;
	reg_in = EE_AUDIO_TDMIN_A_CTRL + off_set * id;
	if (p_config->pcm_mode == SND_SOC_DAIFMT_I2S)
		aml_audiobus_update_bits(actrl, reg_in,
			1<<30|3<<26|0x1<<25|0x7<<16,
			1<<30|3<<26|1<<25|bclkin_skew<<16);
	else
		aml_audiobus_update_bits(actrl, reg_in,
			3<<26|0x7<<16, 3<<26|bclkin_skew<<16);

}

void aml_tdm_set_slot(
	struct aml_audio_controller *actrl,
	int slots, int slot_width, int index)
{
	unsigned int reg, offset;

	offset = EE_AUDIO_TDMOUT_B_CTRL0 - EE_AUDIO_TDMOUT_A_CTRL0;
	reg = EE_AUDIO_TDMOUT_A_CTRL0 + offset * index;
	aml_audiobus_update_bits(actrl, reg,
				0x3ff, ((slots - 1) << 5) | (slot_width - 1));

	offset = EE_AUDIO_TDMIN_B_CTRL - EE_AUDIO_TDMIN_A_CTRL;
	reg = EE_AUDIO_TDMIN_A_CTRL + offset * index;
	aml_audiobus_update_bits(actrl, reg,
		0xf<<20|0x7<<16|0x1f, index<<20|(slot_width-1));
}

void aml_tdm_set_channel_mask(
	struct aml_audio_controller *actrl,
	int stream, int index, int lane, int mask)
{
	unsigned int offset, reg;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		offset = EE_AUDIO_TDMOUT_B_MASK0 - EE_AUDIO_TDMOUT_A_MASK0;
		reg = EE_AUDIO_TDMOUT_A_MASK0 + offset * index;
	} else {
		offset = EE_AUDIO_TDMIN_B_MASK0 - EE_AUDIO_TDMIN_A_MASK0;
		reg = EE_AUDIO_TDMIN_A_MASK0 + offset * index;
	}

	aml_audiobus_write(actrl, reg + lane, mask);
}

void aml_tdm_set_lane_channel_swap(
	struct aml_audio_controller *actrl,
	int stream, int index, int swap)
{
	unsigned int offset, reg;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		// set lanes mask acordingly
		offset = EE_AUDIO_TDMOUT_B_MASK0 - EE_AUDIO_TDMOUT_A_MASK0;
		reg = EE_AUDIO_TDMOUT_A_MASK0 + offset * index;

		pr_info("\ttdmout swap val = %#x\n", swap);
		offset = EE_AUDIO_TDMOUT_B_SWAP - EE_AUDIO_TDMOUT_A_SWAP;
		reg = EE_AUDIO_TDMOUT_A_SWAP + offset * index;
		aml_audiobus_write(actrl, reg, swap);
	} else {
		offset = EE_AUDIO_TDMIN_B_MASK0 - EE_AUDIO_TDMIN_A_MASK0;
		reg = EE_AUDIO_TDMIN_A_MASK0 + offset * index;

		pr_info("\ttdmin swap val = %#x\n", swap);
		offset = EE_AUDIO_TDMIN_B_SWAP - EE_AUDIO_TDMIN_A_SWAP;
		reg = EE_AUDIO_TDMIN_A_SWAP + offset * index;
		aml_audiobus_write(actrl, reg, swap);
	}
}

void aml_tdm_set_bclk_ratio(
	struct aml_audio_controller *actrl,
	int clk_sel, int lrclk_hi, int bclk_ratio)
{
	unsigned int reg, reg_step = 2;

	reg = EE_AUDIO_MST_A_SCLK_CTRL0 + reg_step * clk_sel;

	aml_audiobus_update_bits(actrl, reg,
				(3 << 30)|0x3ff<<10|0x3ff,
				(3 << 30)|lrclk_hi<<10|bclk_ratio);
}

void aml_tdm_set_lrclkdiv(
	struct aml_audio_controller *actrl,
	int clk_sel, int ratio)
{
	unsigned int reg, reg_step = 2;

	pr_info("aml_dai_set_clkdiv, clksel(%d), ratio(%d)\n",
			clk_sel, ratio);

	reg = EE_AUDIO_MST_A_SCLK_CTRL0 + reg_step * clk_sel;

	aml_audiobus_update_bits(actrl, reg,
		(3 << 30)|(0x3ff << 20),
		(3 << 30)|(ratio << 20));
}
