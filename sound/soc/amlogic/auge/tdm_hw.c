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
#include "iomap.h"

#define MST_CLK_INVERT_PH0_PAD_BCLK       (1 << 0)
#define MST_CLK_INVERT_PH0_PAD_FCLK       (1 << 1)
#define MST_CLK_INVERT_PH1_TDMIN_BCLK     (1 << 2)
#define MST_CLK_INVERT_PH1_TDMIN_FCLK     (1 << 3)
#define MST_CLK_INVERT_PH2_TDMOUT_BCLK    (1 << 4)
#define MST_CLK_INVERT_PH2_TDMOUT_FCLK    (1 << 5)

/*#define G12A_PTM_LB_INTERNAL*/
/*#define TL1_PTM_LB_INTERNAL*/

/* without audio handler, it should be improved */
void aml_tdm_enable(
	struct aml_audio_controller *actrl,
	int stream, int index,
	bool is_enable)
{
	unsigned int offset, reg;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		pr_debug("tdm playback enable\n");

		offset = EE_AUDIO_TDMOUT_B_CTRL0
				- EE_AUDIO_TDMOUT_A_CTRL0;
		reg = EE_AUDIO_TDMOUT_A_CTRL0 + offset * index;
		aml_audiobus_update_bits(actrl, reg, 1<<31, is_enable<<31);
	} else {
		pr_debug("tdm capture enable\n");

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


void tdm_enable(int tdm_index, int is_enable)
{
	unsigned int offset, reg;

	if (tdm_index < 3) {
		pr_info("tdmout is_enable:%d\n", is_enable);

		offset = EE_AUDIO_TDMOUT_B_CTRL0
			- EE_AUDIO_TDMOUT_A_CTRL0;
		reg = EE_AUDIO_TDMOUT_A_CTRL0 + offset * tdm_index;

		audiobus_update_bits(reg, 1<<31, is_enable<<31);
	} else if (tdm_index < 6) {
		pr_info("tdmin is_enable:%d\n", is_enable);

		tdm_index -= 3;
		offset = EE_AUDIO_TDMIN_B_CTRL
			- EE_AUDIO_TDMIN_A_CTRL;
		reg = EE_AUDIO_TDMIN_A_CTRL + offset * tdm_index;

		audiobus_update_bits(reg, 1<<31, is_enable<<31);
	}
}

void tdm_fifo_enable(int tdm_index, int is_enable)
{
	unsigned int reg, offset;

	if (tdm_index < 3) {
		offset = EE_AUDIO_TDMOUT_B_CTRL0
			- EE_AUDIO_TDMOUT_A_CTRL0;
		reg = EE_AUDIO_TDMOUT_A_CTRL0 + offset * tdm_index;

		if (is_enable) {
			audiobus_update_bits(reg, 1<<29, 1<<29);
			audiobus_update_bits(reg, 1<<28, 1<<28);
		} else
			audiobus_update_bits(reg, 3<<28, 0);

	} else if (tdm_index < 6) {
		tdm_index -= 3;
		offset = EE_AUDIO_TDMIN_B_CTRL
				- EE_AUDIO_TDMIN_A_CTRL;
		reg = EE_AUDIO_TDMIN_A_CTRL + offset * tdm_index;

		if (is_enable) {
			audiobus_update_bits(reg, 1<<29, 1<<29);
			audiobus_update_bits(reg, 1<<28, 1<<28);
		} else
			audiobus_update_bits(reg, 3<<28, 0);
	}
}

int tdmout_get_frddr_type(int bitwidth)
{
	unsigned int frddr_type = 0;

	switch (bitwidth) {
	case 8:
		frddr_type = 0;
		break;
	case 16:
		frddr_type = 2;
		break;
	case 24:
	case 32:
		frddr_type = 4;
		break;
	default:
		pr_err("invalid bit_depth: %d\n",
			bitwidth);
		break;
	}

	return frddr_type;
}

void aml_tdm_fifo_ctrl(
	struct aml_audio_controller *actrl,
	int bitwidth, int stream,
	int index, unsigned int fifo_id)
{
	unsigned int frddr_type = tdmout_get_frddr_type(bitwidth);
	unsigned int reg, offset;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		pr_debug("tdm prepare----playback\n");
		// from ddr, 63bit split into 2 samples
		offset = EE_AUDIO_TDMOUT_B_CTRL1
				- EE_AUDIO_TDMOUT_A_CTRL1;
		reg = EE_AUDIO_TDMOUT_A_CTRL1 + offset * index;
		aml_audiobus_update_bits(actrl, reg,
				0x3<<24|0x1f<<8|0x7<<4,
				fifo_id<<24|(bitwidth-1)<<8|frddr_type<<4);
	} else {
		pr_debug("tdm prepare----capture\n");
	}

}

static void aml_clk_set_tdmout_by_id(
		struct aml_audio_controller *actrl,
		unsigned int tdm_index,
		unsigned int sclk_sel,
		unsigned int lrclk_sel,
		bool sclk_ws_inv,
		bool is_master,
		bool binv)
{
	unsigned int val_sclk_ws_inv = 0;
	unsigned int reg = EE_AUDIO_CLK_TDMOUT_A_CTRL + tdm_index;

	/* This is just a copy from previous setting. WHY??? */
	val_sclk_ws_inv = sclk_ws_inv && is_master;
	if (val_sclk_ws_inv)
		aml_audiobus_update_bits(actrl, reg,
			0x3<<30|1<<28|0xf<<24|0xf<<20,
			0x3<<30|val_sclk_ws_inv<<28|
			sclk_sel<<24|lrclk_sel<<20);
	else
		aml_audiobus_update_bits(actrl, reg,
			0x3<<30|1<<29|0xf<<24|0xf<<20,
			0x3<<30|binv<<29|
			sclk_sel<<24|lrclk_sel<<20);
}

static void aml_clk_set_tdmin_by_id(
		struct aml_audio_controller *actrl,
		unsigned int tdm_index,
		unsigned int sclk_sel,
		unsigned int lrclk_sel)
{
	unsigned int reg =
		EE_AUDIO_CLK_TDMIN_A_CTRL + tdm_index;
	aml_audiobus_update_bits(actrl,
		reg,
		0xff<<20,
		sclk_sel<<24|lrclk_sel<<20);
}

static void aml_tdmout_invert_lrclk(
		struct aml_audio_controller *actrl,
		unsigned int tdm_index,
		bool finv)
{
	unsigned int off_set =
		EE_AUDIO_TDMOUT_B_CTRL1 - EE_AUDIO_TDMOUT_A_CTRL1;
	unsigned int reg_out =
		EE_AUDIO_TDMOUT_A_CTRL1 + off_set * tdm_index;
	aml_audiobus_update_bits(actrl,
		reg_out, 0x1<<28, finv<<28);
}

static void aml_tdmout_bclk_skew(
		struct aml_audio_controller *actrl,
		unsigned int tdm_index,
		unsigned int bclkout_skew)
{
	unsigned int off_set =
		EE_AUDIO_TDMOUT_B_CTRL0 - EE_AUDIO_TDMOUT_A_CTRL0;
	unsigned int reg_out =
		EE_AUDIO_TDMOUT_A_CTRL0 + off_set * tdm_index;
	aml_audiobus_update_bits(actrl,
		reg_out, 0x1f<<15, bclkout_skew<<15);
}

void aml_tdm_set_format(
	struct aml_audio_controller *actrl,
	struct pcm_setting *p_config,
	unsigned int clk_sel,
	unsigned int index,
	unsigned int fmt,
	unsigned int capture_active,
	unsigned int playback_active)
{
	unsigned int binv, finv, id;
	unsigned int valb, valf;
	unsigned int reg_in, reg_out, off_set;
	int bclkin_skew, bclkout_skew;
	int master_mode;
	unsigned int clkctl = 0;

	id = index;

	binv = 0;
	finv = 0;

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		valb = SLAVE_A + id;
		valf = SLAVE_A + id;
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
	aml_clk_set_tdmin_by_id(actrl, id, valb, valf);

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		if (p_config->sclk_ws_inv) {
			if (master_mode)
				bclkout_skew = 2;
			else
				bclkout_skew = 3;
		} else
			bclkout_skew = 1;
		bclkin_skew = 3;

		clkctl |= MST_CLK_INVERT_PH0_PAD_FCLK
			| MST_CLK_INVERT_PH2_TDMOUT_FCLK;
		finv = 1;

		if (master_mode) {
			clkctl |= MST_CLK_INVERT_PH0_PAD_BCLK;
			if (capture_active)
				binv |= 1;
		} else {
			if (playback_active)
				binv |= 1;
		}

		break;
	case SND_SOC_DAIFMT_DSP_A:
		/*
		 * Frame high, 1clk before data, one bit for frame sync,
		 * frame sync starts one serial clock cycle earlier,
		 * that is, together with the last bit of the previous
		 * data word.
		 */
		if (p_config->sclk_ws_inv) {
			if (master_mode)
				bclkout_skew = 2;
			else
				bclkout_skew = 3;
		} else
			bclkout_skew = 1;
		bclkin_skew = 3;

		if (capture_active)
			binv |= 1;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
	case SND_SOC_DAIFMT_DSP_B:
		/*
		 * Frame high, one bit for frame sync,
		 * frame sync asserts with the first bit of the frame.
		 */
		if (p_config->sclk_ws_inv) {
			if (master_mode)
				bclkout_skew = 3;
			else
				bclkout_skew = 4;
		} else
			bclkout_skew = 2;
		bclkin_skew = 2;

		if (capture_active)
			binv |= 1;
		break;
	default:
		return;
	}

	p_config->pcm_mode = fmt & SND_SOC_DAIFMT_FORMAT_MASK;

	pr_debug("pad clk ctl value:%x\n", clkctl);
	/* set lrclk/bclk invertion */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_IB_IF:
		/* Invert both clocks */
		if (!master_mode)
			binv ^= 1;

		finv |= 1;
		clkctl ^= MST_CLK_INVERT_PH0_PAD_BCLK;
		clkctl ^= MST_CLK_INVERT_PH0_PAD_FCLK;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		/* Invert bit clock */
		if (!master_mode)
			binv ^= 1;
		clkctl ^= MST_CLK_INVERT_PH0_PAD_BCLK;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		/* Invert frame clock */
		finv ^= 1;
		clkctl ^= MST_CLK_INVERT_PH0_PAD_FCLK;
		break;
	case SND_SOC_DAIFMT_NB_NF:
		/* normal cases */
		break;
	default:
		return;
	}
	pr_debug("sclk_ph0 (pad) clk ctl set:%x\n", clkctl);
	/* clk ctrl: delay line and invert clk */
	/*clkctl |= 0x88880000;*/

	if (master_mode) {
		off_set = EE_AUDIO_MST_B_SCLK_CTRL1 - EE_AUDIO_MST_A_SCLK_CTRL1;
		reg_out = EE_AUDIO_MST_A_SCLK_CTRL1 + off_set * id;

		aml_audiobus_update_bits(actrl, reg_out, 0x3f, clkctl);
	}

	pr_info("master_mode(%d), binv(%d), finv(%d) out_skew(%d), in_skew(%d)\n",
			master_mode, binv, finv, bclkout_skew, bclkin_skew);

	/* TDM out */
	if (playback_active) {
		aml_clk_set_tdmout_by_id(actrl,
			id, valb, valf,
			p_config->sclk_ws_inv, master_mode, binv);
		aml_tdmout_invert_lrclk(actrl, id, finv);
		aml_tdmout_bclk_skew(actrl, id, bclkout_skew);
	}

	/* TDM in */
	if (capture_active) {
		reg_in = EE_AUDIO_CLK_TDMIN_A_CTRL + id;
		aml_audiobus_update_bits(actrl, reg_in,
			0x3<<30, 0x3<<30);

		if (master_mode)
			aml_audiobus_update_bits(actrl, reg_in,
				0x1<<29, binv<<29);

		off_set = EE_AUDIO_TDMIN_B_CTRL - EE_AUDIO_TDMIN_A_CTRL;
		reg_in = EE_AUDIO_TDMIN_A_CTRL + off_set * id;
		aml_audiobus_update_bits(actrl, reg_in,
			3<<26|0x7<<16, 3<<26|bclkin_skew<<16);

		aml_audiobus_update_bits(actrl, reg_in,
			0x1<<25, finv<<25);

		if (p_config->pcm_mode == SND_SOC_DAIFMT_I2S)
			aml_audiobus_update_bits(actrl, reg_in,
				1<<30,
				1<<30);
	}
}

void aml_update_tdmin_skew(struct aml_audio_controller *actrl,
	 int idx, int skew)
{
	unsigned int reg_in, off_set;

	off_set = EE_AUDIO_TDMIN_B_CTRL - EE_AUDIO_TDMIN_A_CTRL;
	reg_in = EE_AUDIO_TDMIN_A_CTRL + off_set * idx;
	aml_audiobus_update_bits(actrl, reg_in,
		0x7 << 16, skew << 16);
}

void aml_update_tdmin_rev_ws(struct aml_audio_controller *actrl,
	 int idx, int is_rev)
{
	unsigned int reg_in, off_set;

	off_set = EE_AUDIO_TDMIN_B_CTRL - EE_AUDIO_TDMIN_A_CTRL;
	reg_in = EE_AUDIO_TDMIN_A_CTRL + off_set * idx;
	aml_audiobus_update_bits(actrl, reg_in,
		0x1 << 25, is_rev << 25);
}

void aml_tdm_set_slot_out(
	struct aml_audio_controller *actrl,
	int index, int slots, int slot_width,
	int force_oe, int oe_val)
{
	unsigned int reg, offset;

	offset = EE_AUDIO_TDMOUT_B_CTRL0 - EE_AUDIO_TDMOUT_A_CTRL0;
	reg = EE_AUDIO_TDMOUT_A_CTRL0 + offset * index;
	aml_audiobus_update_bits(actrl, reg,
				0x3ff, ((slots - 1) << 5) | (slot_width - 1));

	if (force_oe) {
		aml_audiobus_update_bits(actrl, reg, 0xf << 24, force_oe << 24);

		/* force oe val, in or out */
		if (oe_val) {
			reg = EE_AUDIO_TDMOUT_A_CTRL1 + offset * index;
			aml_audiobus_update_bits(actrl, reg,
				0xf << 0, oe_val << 0);
		}
	}
}

void aml_tdm_set_slot_in(
	struct aml_audio_controller *actrl,
	int index, int in_src, int slot_width)
{
	unsigned int reg, offset;

	offset = EE_AUDIO_TDMIN_B_CTRL - EE_AUDIO_TDMIN_A_CTRL;
	reg = EE_AUDIO_TDMIN_A_CTRL + offset * index;

#if defined(G12A_PTM_LB_INTERNAL)
	if (index == 0) /*TODO: ptm, tdma dsp_a lb*/
		aml_audiobus_update_bits(actrl, reg,
			0xf<<20|0x1f, 6<<20|(slot_width-1));
	if (index == 1) /*TODO: ptm, tdmb i2s lb*/
		aml_audiobus_update_bits(actrl, reg,
			0xf<<20|0x1f, 7<<20|(slot_width-1));
	else
#elif defined(TL1_PTM_LB_INTERNAL)
if (index == 0) /*TODO: ptm, tdma dsp_a lb*/
	aml_audiobus_update_bits(actrl, reg,
		0xf<<20|0x1f, 13<<20|(slot_width-1));
else if (index == 1) /*TODO: ptm, tdmb i2s lb*/
	aml_audiobus_update_bits(actrl, reg,
		0xf<<20|0x1f, 14<<20|(slot_width-1));
else
#endif
	aml_audiobus_update_bits(actrl, reg,
		0xf << 20 | 0x1f, in_src << 20 | (slot_width-1));
}

void aml_update_tdmin_src(
	struct aml_audio_controller *actrl,
	int index, int in_src)
{
	unsigned int reg, offset;

	offset = EE_AUDIO_TDMIN_B_CTRL - EE_AUDIO_TDMIN_A_CTRL;
	reg = EE_AUDIO_TDMIN_A_CTRL + offset * index;

	aml_audiobus_update_bits(actrl, reg,
		0xf << 20, in_src << 20);
}

void tdmin_set_chnum_en(
	struct aml_audio_controller *actrl,
	int index, bool enable)
{
	unsigned int reg, offset;

	offset = EE_AUDIO_TDMIN_B_CTRL - EE_AUDIO_TDMIN_A_CTRL;
	reg = EE_AUDIO_TDMIN_A_CTRL + offset * index;

	aml_audiobus_update_bits(actrl, reg,
		0x1 << 6, enable << 6);
}

void aml_tdm_set_channel_mask(
	struct aml_audio_controller *actrl,
	int stream, int index, int lane, int mask)
{
	unsigned int offset, reg;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (lane >= LANE_MAX1) {
			offset = EE_AUDIO_TDMOUT_B_MASK4
				- EE_AUDIO_TDMOUT_A_MASK4;
			reg = EE_AUDIO_TDMOUT_A_MASK4 + offset * index;
			lane -= LANE_MAX1;
		} else {
			offset = EE_AUDIO_TDMOUT_B_MASK0
				- EE_AUDIO_TDMOUT_A_MASK0;
			reg = EE_AUDIO_TDMOUT_A_MASK0 + offset * index;
		}
	} else {
		if (lane >= LANE_MAX1) {
			offset = EE_AUDIO_TDMIN_B_MASK4
				- EE_AUDIO_TDMIN_A_MASK4;
			reg = EE_AUDIO_TDMIN_A_MASK4 + offset * index;
			lane -= LANE_MAX1;
		} else {
			offset = EE_AUDIO_TDMIN_B_MASK0
				- EE_AUDIO_TDMIN_A_MASK0;
			reg = EE_AUDIO_TDMIN_A_MASK0 + offset * index;
		}
	}

	aml_audiobus_write(actrl, reg + lane, mask);
}

void aml_tdm_set_lane_channel_swap(
	struct aml_audio_controller *actrl,
	int stream, int index, int swap0, int swap1)
{
	unsigned int offset, reg;

	pr_debug("\t %s swap0 = %#x, swap1 = %#x\n",
		(stream == SNDRV_PCM_STREAM_PLAYBACK) ? "tdmout" : "tdmin",
		swap0,
		swap1);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		offset = EE_AUDIO_TDMOUT_B_SWAP0
			- EE_AUDIO_TDMOUT_A_SWAP0;
		reg = EE_AUDIO_TDMOUT_A_SWAP0 + offset * index;
		aml_audiobus_write(actrl, reg, swap0);

		if (swap1) {
			offset = EE_AUDIO_TDMOUT_B_SWAP1
				- EE_AUDIO_TDMOUT_A_SWAP1;
			reg = EE_AUDIO_TDMOUT_A_SWAP1 + offset * index;
			aml_audiobus_write(actrl, reg, swap1);
		}
	} else {
		offset = EE_AUDIO_TDMIN_B_SWAP0
			- EE_AUDIO_TDMIN_A_SWAP0;
		reg = EE_AUDIO_TDMIN_A_SWAP0 + offset * index;
		aml_audiobus_write(actrl, reg, swap0);

		if (swap1) {
			offset = EE_AUDIO_TDMIN_B_SWAP1
				- EE_AUDIO_TDMIN_A_SWAP1;
			reg = EE_AUDIO_TDMIN_A_SWAP1 + offset * index;
			aml_audiobus_write(actrl, reg, swap1);
		}
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

	pr_debug("aml_dai_set_clkdiv, clksel(%d), ratio(%d)\n",
			clk_sel, ratio);

	reg = EE_AUDIO_MST_A_SCLK_CTRL0 + reg_step * clk_sel;

	aml_audiobus_update_bits(actrl, reg,
		(3 << 30)|(0x3ff << 20),
		(3 << 30)|(ratio << 20));
}

void aml_tdmout_select_aed(bool enable, int tdmout_id)
{
	unsigned int reg, offset;

	/* select eq_drc output */
	offset = EE_AUDIO_TDMOUT_B_CTRL1
			- EE_AUDIO_TDMOUT_A_CTRL1;
	reg = EE_AUDIO_TDMOUT_A_CTRL1 + offset * tdmout_id;
	audiobus_update_bits(reg, 0x1 << 31, enable << 31);
}

void aml_tdmout_get_aed_info(int tdmout_id,
	int *bitwidth, int *frddrtype)
{
	unsigned int reg, offset, val;

	offset = EE_AUDIO_TDMOUT_B_CTRL1
			- EE_AUDIO_TDMOUT_A_CTRL1;
	reg = EE_AUDIO_TDMOUT_A_CTRL1 + offset * tdmout_id;

	val = audiobus_read(reg);
	if (bitwidth)
		*bitwidth = (val >> 8) & 0x1f;
	if (frddrtype)
		*frddrtype = (val >> 4) & 0x7;
}

void aml_tdmout_enable_gain(int tdmout_id, int en)
{
	unsigned int reg, offset;

	offset = EE_AUDIO_TDMOUT_B_CTRL1
			- EE_AUDIO_TDMOUT_A_CTRL1;
	reg = EE_AUDIO_TDMOUT_A_CTRL1 + offset * tdmout_id;
	audiobus_update_bits(reg, 0x1 << 26, !!en << 26);
}

void aml_tdm_clk_pad_select(
	struct aml_audio_controller *actrl,
	int mpad, int mpad_offset, int mclk_sel,
	int tdm_index, int clk_sel)
{
	unsigned int reg, mask_offset, val_offset;

	if (mpad >= 0) {
		switch (mpad) {
		case 0:
			mask_offset = 0x7 << 0;
			val_offset = mclk_sel << 0;
			break;
		case 1:
			mask_offset = 0x7 << 4;
			val_offset = mclk_sel << 4;
			break;
		default:
			mask_offset = 0;
			val_offset = 0;
			pr_info("unknown tdm mpad:%d\n", mpad);
			break;
		}

		reg = EE_AUDIO_MST_PAD_CTRL0(mpad_offset);
		if (actrl)
			aml_audiobus_update_bits(actrl, reg,
				mask_offset, val_offset);
		else
			audiobus_update_bits(reg,
				mask_offset, val_offset);
	} else
		pr_warn("mclk is not configured\n");

	reg = EE_AUDIO_MST_PAD_CTRL1(mpad_offset);
	switch (tdm_index) {
	case 0:
		mask_offset = 0x7 << 16 | 0x7 << 0;
		val_offset = clk_sel << 16 | clk_sel << 0;
		break;
	case 1:
		mask_offset = 0x7 << 20 | 0x7 << 4;
		val_offset = clk_sel << 20 | clk_sel << 4;
		break;
	case 2:
		mask_offset = 0x7 << 24 | 0x7 << 8;
		val_offset = clk_sel << 24 | clk_sel << 8;
		break;
	default:
		pr_err("unknown mclk pad, tdm index:%d\n", tdm_index);
		return;
	}
	if (actrl)
		aml_audiobus_update_bits(actrl, reg,
			mask_offset, val_offset);
	else
		audiobus_update_bits(reg,
			mask_offset, val_offset);
}

void i2s_to_hdmitx_ctrl(int tdm_index)
{
	audiobus_write(EE_AUDIO_TOHDMITX_CTRL0,
		1 << 31
		| tdm_index << 12 /* dat_sel */
		| tdm_index << 8 /* lrclk_sel */
		| 1 << 7 /* Bclk_cap_inv */
		| 0 << 6 /* Bclk_o_inv */
		| tdm_index << 4 /* Bclk_sel */
	);
}

void aml_tdm_mute_playback(
		struct aml_audio_controller *actrl,
		int tdm_index,
		bool mute,
		int lane_cnt)
{
	unsigned int offset, reg;
	unsigned int mute_mask = 0xffffffff;
	unsigned int mute_val = 0;
	int i = 0;

	if (mute)
		mute_val = 0xffffffff;

	offset = EE_AUDIO_TDMOUT_B_MUTE0
			- EE_AUDIO_TDMOUT_A_MUTE0;
	reg = EE_AUDIO_TDMOUT_A_MUTE0 + offset * tdm_index;
	for (i = 0; i < LANE_MAX1; i++)
		aml_audiobus_update_bits(actrl, reg + i, mute_mask, mute_val);

	if (lane_cnt > LANE_MAX1) {
		offset = EE_AUDIO_TDMOUT_B_MUTE4
				- EE_AUDIO_TDMOUT_A_MUTE4;
		reg = EE_AUDIO_TDMOUT_A_MUTE4 + offset * tdm_index;
		for (i = 0; i < LANE_MAX1; i++)
			aml_audiobus_update_bits(actrl, reg + i,
					mute_mask, mute_val);
	}
}

void aml_tdm_mute_capture(
		struct aml_audio_controller *actrl,
		int tdm_index,
		bool mute,
		int lane_cnt)
{
	unsigned int offset, reg;
	unsigned int mute_mask = 0xffffffff;
	unsigned int mute_val = 0;
	int i = 0;

	if (mute)
		mute_val = 0xffffffff;

	offset = EE_AUDIO_TDMIN_B_MUTE0
			- EE_AUDIO_TDMIN_A_MUTE0;
	reg = EE_AUDIO_TDMIN_A_MUTE0 + offset * tdm_index;
	for (i = 0; i < LANE_MAX1; i++)
		aml_audiobus_update_bits(actrl, reg + i,
				mute_mask, mute_val);

	if (lane_cnt > LANE_MAX1) {
		offset = EE_AUDIO_TDMIN_B_MUTE4
				- EE_AUDIO_TDMIN_A_MUTE4;
		reg = EE_AUDIO_TDMIN_A_MUTE4 + offset * tdm_index;
		for (i = 0; i < LANE_MAX1; i++)
			aml_audiobus_update_bits(actrl, reg + i,
					mute_mask, mute_val);
	}
}

void aml_tdm_out_reset(unsigned int tdm_id, int offset)
{
	unsigned int reg = 0, val = 0;

	if ((offset != 0) && (offset != 1)) {
		pr_err("%s(), invalid offset = %d\n", __func__, offset);
		return;
	}

	if (tdm_id == 0) {
		reg = EE_AUDIO_SW_RESET0(offset);
		val = REG_BIT_RESET_TDMOUTA;
	} else if (tdm_id == 1) {
		reg = EE_AUDIO_SW_RESET0(offset);
		val = REG_BIT_RESET_TDMOUTB;
	} else if (tdm_id == 2) {
		reg = EE_AUDIO_SW_RESET0(offset);
		val = REG_BIT_RESET_TDMOUTC;
	} else {
		pr_err("invalid tdmout id %d\n", tdm_id);
		return;
	}
	audiobus_update_bits(reg, val, val);
	audiobus_update_bits(reg, val, 0);
}
