/*
 * sound/soc/amlogic/auge/spdif_hw.c
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

#include "iomap.h"
#include "spdif_hw.h"
#include "ddr_mngr.h"

#include <linux/amlogic/media/sound/aout_notify.h>

/*#define G12A_PTM*/
/*#define __PTM_SPDIF_INTERNAL_LB__*/

unsigned int aml_spdif_ctrl_read(struct aml_audio_controller *actrl,
	int stream, int index)
{
	unsigned int offset, reg;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		offset = EE_AUDIO_SPDIFOUT_B_CTRL0 - EE_AUDIO_SPDIFOUT_CTRL0;
		reg = EE_AUDIO_SPDIFOUT_CTRL0 + offset * index;
	} else {
		reg = EE_AUDIO_SPDIFIN_CTRL0;
	}

	return aml_audiobus_read(actrl, reg);
}

void aml_spdif_ctrl_write(struct aml_audio_controller *actrl,
	int stream, int index, int val)
{
	unsigned int offset, reg;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		offset = EE_AUDIO_SPDIFOUT_B_CTRL0 - EE_AUDIO_SPDIFOUT_CTRL0;
		reg = EE_AUDIO_SPDIFOUT_CTRL0 + offset * index;
	} else {
		reg = EE_AUDIO_SPDIFIN_CTRL0;
	}

	aml_audiobus_write(actrl, reg, val);
}

void aml_spdifin_chnum_en(struct aml_audio_controller *actrl,
	int index, bool is_enable)
{
	unsigned int reg;

	reg = EE_AUDIO_SPDIFIN_CTRL0;
	aml_audiobus_update_bits(actrl, reg, 1 << 26, is_enable << 26);

	pr_info("%s spdifin ctrl0:0x%x\n",
		__func__,
		aml_audiobus_read(actrl, reg));
}

void aml_spdif_enable(
	struct aml_audio_controller *actrl,
	int stream,
	int index,
	bool is_enable)
{
	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		unsigned int offset, reg;

		offset = EE_AUDIO_SPDIFOUT_B_CTRL0 - EE_AUDIO_SPDIFOUT_CTRL0;
		reg = EE_AUDIO_SPDIFOUT_CTRL0 + offset * index;
		aml_audiobus_update_bits(actrl,
			reg, 1<<31, is_enable<<31);
	} else {
		aml_audiobus_update_bits(actrl,
			EE_AUDIO_SPDIFIN_CTRL0, 1<<31, is_enable<<31);
#ifdef __PTM_SPDIF_INTERNAL_LB__
		if (index == 0)
		aml_audiobus_update_bits(actrl,
			EE_AUDIO_SPDIFIN_CTRL0, 0x3<<4, 0x1<<4);
#endif
	}
}

void aml_spdif_mute(
	struct aml_audio_controller *actrl,
	int stream,
	int index,
	bool is_mute)
{
	int mute_lr = 0;

	if (is_mute)
		mute_lr = 0x3;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		unsigned int offset, reg;

		offset = EE_AUDIO_SPDIFOUT_B_CTRL0 - EE_AUDIO_SPDIFOUT_CTRL0;
		reg = EE_AUDIO_SPDIFOUT_CTRL0 + offset * index;
		aml_audiobus_update_bits(actrl,
			reg, 0x3 << 21, mute_lr << 21);
	} else {
		aml_audiobus_update_bits(actrl,
			EE_AUDIO_SPDIFIN_CTRL0, 0x3 << 6, mute_lr << 6);
	}
}

void aml_spdif_arb_config(struct aml_audio_controller *actrl)
{
	/* config ddr arb */
	aml_audiobus_write(actrl, EE_AUDIO_ARB_CTRL, 1<<31|0xff<<0);
}

int aml_spdifin_status_check(struct aml_audio_controller *actrl)
{
	unsigned int val;

	val = aml_audiobus_read(actrl, EE_AUDIO_SPDIFIN_STAT0);

	/* pr_info("\t--- spdif handles status0 %#x\n", val); */
	return val;
}

void aml_spdifin_clr_irq(struct aml_audio_controller *actrl,
	bool is_all_bits, int clr_bits_val)
{
	if (is_all_bits) {
		aml_audiobus_update_bits(actrl,
				EE_AUDIO_SPDIFIN_CTRL0,
				1 << 26,
				1 << 26);
		aml_audiobus_update_bits(actrl,
				EE_AUDIO_SPDIFIN_CTRL0,
				1 << 26,
				0);
	} else
		aml_audiobus_update_bits(actrl,
				EE_AUDIO_SPDIFIN_CTRL6,
				0xff << 16,
				clr_bits_val << 16);
}

void aml_spdif_fifo_reset(
	struct aml_audio_controller *actrl,
	int stream, int index)
{
	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		/* reset afifo */
		unsigned int offset, reg;

		offset = EE_AUDIO_SPDIFOUT_B_CTRL0 - EE_AUDIO_SPDIFOUT_CTRL0;
		reg = EE_AUDIO_SPDIFOUT_CTRL0 + offset * index;
		aml_audiobus_update_bits(actrl,
				reg, 3<<28, 0);
		aml_audiobus_update_bits(actrl,
				reg, 1<<29, 1<<29);
		aml_audiobus_update_bits(actrl,
				reg, 1<<28, 1<<28);
	} else {
		/* reset afifo */
		aml_audiobus_update_bits(actrl,
				EE_AUDIO_SPDIFIN_CTRL0, 3<<28, 0);
		aml_audiobus_update_bits(actrl,
				EE_AUDIO_SPDIFIN_CTRL0, 1<<29, 1<<29);
		aml_audiobus_update_bits(actrl,
				EE_AUDIO_SPDIFIN_CTRL0, 1<<28, 1<<28);
	}
}

int spdifout_get_frddr_type(int bitwidth)
{
	unsigned int frddr_type = 0;

	switch (bitwidth) {
	case 8:
		frddr_type = 0;
		break;
	case 16:
		frddr_type = 1;
		break;
	case 24:
		frddr_type = 4;
		break;
	case 32:
		frddr_type = 3;
		break;
	default:
		pr_err("runtime format invalid bitwidth: %d\n",
			bitwidth);
		break;
	}

	return frddr_type;
}

void aml_spdif_fifo_ctrl(
	struct aml_audio_controller *actrl,
	int bitwidth,
	int stream,
	int index,
	unsigned int fifo_id)
{
	unsigned int toddr_type;
	unsigned int frddr_type = spdifout_get_frddr_type(bitwidth);

	switch (bitwidth) {
	case 8:
		toddr_type = 0;
		break;
	case 16:
		toddr_type = 1;
		break;
	case 24:
		toddr_type = 4;
		break;
	case 32:
		toddr_type = 3;
		break;
	default:
		pr_err("runtime format invalid bitwidth: %d\n",
			bitwidth);
		return;
	}

	pr_info("%s, bit depth:%d, frddr type:%d, toddr:type:%d\n",
		__func__,
		bitwidth,
		frddr_type,
		toddr_type);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		unsigned int offset, reg;

		/* mask lane 0 L/R channels */
		offset = EE_AUDIO_SPDIFOUT_B_CTRL0 - EE_AUDIO_SPDIFOUT_CTRL0;
		reg = EE_AUDIO_SPDIFOUT_CTRL0 + offset * index;
		aml_audiobus_update_bits(actrl,
			reg,
			0x1<<29|0x1<<28|0x3<<21|0x1<<20|0x1<<19|0xff<<4,
			1<<29|1<<28|0x0<<21|0<<20|0<<19|0x3<<4);

		offset = EE_AUDIO_SPDIFOUT_B_CTRL1 - EE_AUDIO_SPDIFOUT_CTRL1;
		reg = EE_AUDIO_SPDIFOUT_CTRL1 + offset * index;
		aml_audiobus_update_bits(actrl,
			reg,
			0x3 << 24 | 0x1f << 8 | 0x7 << 4,
			fifo_id << 24 | (bitwidth - 1) << 8 | frddr_type<<4);

		offset = EE_AUDIO_SPDIFOUT_B_SWAP - EE_AUDIO_SPDIFOUT_SWAP;
		reg = EE_AUDIO_SPDIFOUT_SWAP + offset * index;
		aml_audiobus_write(actrl,
			reg,
			1<<4);
	} else {
		unsigned int spdifin_clk = 500000000;

		/* sysclk/rate/32(bit)/2(ch)/2(bmc) */
		unsigned int counter_32k  = (spdifin_clk / (32000  * 64));
		unsigned int counter_44k  = (spdifin_clk / (44100  * 64));
		unsigned int counter_48k  = (spdifin_clk / (48000  * 64));
		unsigned int counter_88k  = (spdifin_clk / (88200  * 64));
		unsigned int counter_96k  = (spdifin_clk / (96000  * 64));
		unsigned int counter_176k = (spdifin_clk / (176400 * 64));
		unsigned int counter_192k = (spdifin_clk / (192000 * 64));
		unsigned int mode0_th = 3 * (counter_32k + counter_44k) >> 1;
		unsigned int mode1_th = 3 * (counter_44k + counter_48k) >> 1;
		unsigned int mode2_th = 3 * (counter_48k + counter_88k) >> 1;
		unsigned int mode3_th = 3 * (counter_88k + counter_96k) >> 1;
		unsigned int mode4_th = 3 * (counter_96k + counter_176k) >> 1;
		unsigned int mode5_th = 3 * (counter_176k + counter_192k) >> 1;
		unsigned int mode0_timer = counter_32k >> 1;
		unsigned int mode1_timer = counter_44k >> 1;
		unsigned int mode2_timer = counter_48k >> 1;
		unsigned int mode3_timer = counter_88k >> 1;
		unsigned int mode4_timer = counter_96k >> 1;
		unsigned int mode5_timer = (counter_176k >> 1);
		unsigned int mode6_timer = (counter_192k >> 1);

		aml_audiobus_write(actrl,
			EE_AUDIO_SPDIFIN_CTRL1,
			0xff << 20 | (spdifin_clk / 10000) << 0);

		aml_audiobus_write(actrl,
			EE_AUDIO_SPDIFIN_CTRL2,
			mode0_th << 20 |
			mode1_th << 10 |
			mode2_th << 0);

		aml_audiobus_write(actrl,
			EE_AUDIO_SPDIFIN_CTRL3,
			mode3_th << 20 |
			mode4_th << 10 |
			mode5_th << 0);

		aml_audiobus_write(actrl,
			EE_AUDIO_SPDIFIN_CTRL4,
			(mode0_timer << 24) |
			(mode1_timer << 16) |
			(mode2_timer << 8)  |
			(mode3_timer << 0)
			);

		aml_audiobus_write(actrl,
			EE_AUDIO_SPDIFIN_CTRL5,
			(mode4_timer << 24) |
			(mode5_timer << 16) |
			(mode6_timer << 8)
			);

		aml_audiobus_update_bits(actrl,
			EE_AUDIO_SPDIFIN_CTRL0,
			0x1 << 25 | 0x1 << 24 | 0xfff << 12,
			0x1 << 25 | 0x0 << 24 | 0xff << 12);
	}

}

int spdifin_get_mode(void)
{
	int mode_val = audiobus_read(EE_AUDIO_SPDIFIN_STAT0);

	mode_val >>= 28;
	mode_val &= 0x7;

	return mode_val;
}

int spdif_get_channel_status(int reg)
{
	return audiobus_read(reg);
}

void spdifin_set_channel_status(int ch, int bits)
{
	int ch_status_sel = (ch << 3 | bits) & 0xf;

	/*which channel status would be got*/
	audiobus_update_bits(EE_AUDIO_SPDIFIN_CTRL0,
		0xf << 8,
		ch_status_sel << 8);
}

void aml_spdifout_select_aed(bool enable, int spdifout_id)
{
	unsigned int offset, reg;

	/* select eq_drc output */
	offset = EE_AUDIO_SPDIFOUT_B_CTRL1 - EE_AUDIO_SPDIFOUT_CTRL1;
	reg = EE_AUDIO_SPDIFOUT_CTRL1 + offset * spdifout_id;
	audiobus_update_bits(reg, 0x1 << 21, enable << 21);
}

void aml_spdifout_get_aed_info(int spdifout_id,
	int *bitwidth, int *frddrtype)
{
	unsigned int reg, offset, val;

	offset = EE_AUDIO_SPDIFOUT_B_CTRL1
			- EE_AUDIO_SPDIFOUT_CTRL1;
	reg = EE_AUDIO_SPDIFOUT_CTRL1 + offset * spdifout_id;

	val = audiobus_read(reg);
	if (bitwidth)
		*bitwidth = (val >> 8) & 0x1f;
	if (frddrtype)
		*frddrtype = (val >> 4) & 0x7;
}

/* spdifout to hdmix ctrl
 * allow spdif out data to hdmitx
 */
void spdifout_to_hdmitx_ctrl(int spdif_index)
{
	audiobus_write(EE_AUDIO_TOHDMITX_CTRL0,
		1 << 31
		| 1 << 3 /* spdif_clk_cap_inv */
		| 0 << 2 /* spdif_clk_inv */
		| spdif_index << 1 /* spdif_out */
		| spdif_index << 0 /* spdif_clk */
	);
}

void spdifout_clk_ctrl(int spdif_id, bool is_enable)
{
	unsigned int offset, reg;

	offset = EE_AUDIO_CLK_SPDIFOUT_B_CTRL - EE_AUDIO_CLK_SPDIFOUT_CTRL;
	reg = EE_AUDIO_CLK_SPDIFOUT_CTRL + offset * spdif_id;

	/* select : mpll 0, 24m, so spdif clk:6m */
	audiobus_write(reg, is_enable << 31 | 0x0 << 24 | 0x3 << 0);
}

void spdifout_fifo_ctrl(int spdif_id, int fifo_id, int bitwidth)
{
	unsigned int frddr_type = spdifout_get_frddr_type(bitwidth);
	unsigned int offset, reg;

	pr_info("spdif_%s fifo ctrl, frddr:%d type:%d, %d bits\n",
		(spdif_id == 0) ? "a":"b",
		fifo_id,
		frddr_type,
		bitwidth);

	/* mask lane 0 L/R channels */
	offset = EE_AUDIO_SPDIFOUT_B_CTRL0 - EE_AUDIO_SPDIFOUT_CTRL0;
	reg = EE_AUDIO_SPDIFOUT_CTRL0 + offset * spdif_id;
	audiobus_update_bits(reg,
		0x3<<21|0x1<<20|0x1<<19|0xff<<4,
		0x0<<21|0<<20|0<<19|0x3<<4);

	offset = EE_AUDIO_SPDIFOUT_B_CTRL1 - EE_AUDIO_SPDIFOUT_CTRL1;
	reg = EE_AUDIO_SPDIFOUT_CTRL1 + offset * spdif_id;
	audiobus_update_bits(reg,
		0x3 << 24 | 0x1f << 8 | 0x7 << 4,
		fifo_id << 24 | (bitwidth - 1) << 8 | frddr_type<<4);

	offset = EE_AUDIO_SPDIFOUT_B_SWAP - EE_AUDIO_SPDIFOUT_SWAP;
	reg = EE_AUDIO_SPDIFOUT_SWAP + offset * spdif_id;
	audiobus_write(reg, 1<<4);

	/* reset afifo */
	offset = EE_AUDIO_SPDIFOUT_B_CTRL0 - EE_AUDIO_SPDIFOUT_CTRL0;
	reg = EE_AUDIO_SPDIFOUT_CTRL0 + offset * spdif_id;
	audiobus_update_bits(reg, 3<<28, 0);
	audiobus_update_bits(reg, 1<<29, 1<<29);
	audiobus_update_bits(reg, 1<<28, 1<<28);
}

static bool spdifout_is_enable(int spdif_id)
{
	unsigned int offset, reg, val;

	offset = EE_AUDIO_SPDIFOUT_B_CTRL0 - EE_AUDIO_SPDIFOUT_CTRL0;
	reg = EE_AUDIO_SPDIFOUT_CTRL0 + offset * spdif_id;
	val = audiobus_read(reg);

	return ((val >> 31) == 1);
}

void spdifout_enable(int spdif_id, bool is_enable)
{
	unsigned int offset, reg;

	pr_info("spdif_%s is set to %s\n",
		(spdif_id == 0) ? "a":"b",
		is_enable ? "enable":"disable");

	offset = EE_AUDIO_SPDIFOUT_B_CTRL0 - EE_AUDIO_SPDIFOUT_CTRL0;
	reg = EE_AUDIO_SPDIFOUT_CTRL0 + offset * spdif_id;

	if (!is_enable) {
		/* share buffer, spdif should be active, so mute it */
		audiobus_update_bits(reg, 0x3 << 21, 0x3 << 21);
		return;
	}

	audiobus_update_bits(reg, 1<<31, is_enable<<31);
}

void spdifout_samesource_set(int spdif_index, int fifo_id,
	int bitwidth, bool is_enable)
{
	int spdif_id;

	if (spdif_index == 1)
		spdif_id = 1;
	else
		spdif_id = 0;

	/* clk for spdif_b is always on */
	/*if (!spdif_id)*/
		spdifout_clk_ctrl(spdif_id, /*is_enable*/true);

	if (is_enable)
		spdifout_fifo_ctrl(spdif_id, fifo_id, bitwidth);
}

int spdifin_get_sample_rate(void)
{
	unsigned int val;

	val = audiobus_read(EE_AUDIO_SPDIFIN_STAT0);

	/* NA when check min width of two edges */
	if (((val >> 18) & 0x3ff) == 0x3ff)
		return 0x7;

	return (val >> 28) & 0x7;
}

static int spdifin_get_channel_status(int sel)
{
	unsigned int val;

	/* set ch_status_sel to channel status */
	audiobus_update_bits(EE_AUDIO_SPDIFIN_CTRL0, 0xf << 8, sel << 8);

	val = audiobus_read(EE_AUDIO_SPDIFIN_STAT1);

	return val;
}

int spdifin_get_ch_status0to31(void)
{
	return spdifin_get_channel_status(0x0);
}

int spdifin_get_audio_type(void)
{
	unsigned int val;

	/* set ch_status_sel to read Pc */
	val = spdifin_get_channel_status(0x6);

	return (val >> 16) & 0xff;
}

void spdif_set_channel_status_info(
	struct iec958_chsts *chsts, int spdif_id)
{
	unsigned int offset, reg;

	/* "ch status" = reg_chsts0~B */
	offset = EE_AUDIO_SPDIFOUT_B_CTRL0 - EE_AUDIO_SPDIFOUT_CTRL0;
	reg = EE_AUDIO_SPDIFOUT_CTRL0 + offset * spdif_id;
	audiobus_update_bits(reg, 0x1 << 24, 0x0 << 24);

	offset = EE_AUDIO_SPDIFOUT_B_CHSTS0 - EE_AUDIO_SPDIFOUT_CHSTS0;
	reg = EE_AUDIO_SPDIFOUT_CHSTS0 + offset * spdif_id;
	audiobus_write(reg, chsts->chstat1_l << 16 | chsts->chstat0_l);

	offset = EE_AUDIO_SPDIFOUT_B_CHSTS1 - EE_AUDIO_SPDIFOUT_CHSTS1;
	reg = EE_AUDIO_SPDIFOUT_CHSTS1 + offset * spdif_id;
	audiobus_write(reg, chsts->chstat1_l << 16 | chsts->chstat0_l);

	offset = EE_AUDIO_SPDIFOUT_B_CHSTS2 - EE_AUDIO_SPDIFOUT_CHSTS2;
	reg = EE_AUDIO_SPDIFOUT_CHSTS2 + offset * spdif_id;
	audiobus_write(reg, chsts->chstat1_l << 16 | chsts->chstat0_l);

	offset = EE_AUDIO_SPDIFOUT_B_CHSTS3 - EE_AUDIO_SPDIFOUT_CHSTS3;
	reg = EE_AUDIO_SPDIFOUT_CHSTS3 + offset * spdif_id;
	audiobus_write(reg, chsts->chstat1_l << 16 | chsts->chstat0_l);

	offset = EE_AUDIO_SPDIFOUT_B_CHSTS4 - EE_AUDIO_SPDIFOUT_CHSTS4;
	reg = EE_AUDIO_SPDIFOUT_CHSTS4 + offset * spdif_id;
	audiobus_write(reg, chsts->chstat1_l << 16 | chsts->chstat0_l);

	offset = EE_AUDIO_SPDIFOUT_B_CHSTS5 - EE_AUDIO_SPDIFOUT_CHSTS5;
	reg = EE_AUDIO_SPDIFOUT_CHSTS5 + offset * spdif_id;
	audiobus_write(reg, chsts->chstat1_l << 16 | chsts->chstat0_l);

	offset = EE_AUDIO_SPDIFOUT_B_CHSTS6 - EE_AUDIO_SPDIFOUT_CHSTS6;
	reg = EE_AUDIO_SPDIFOUT_CHSTS6 + offset * spdif_id;
	audiobus_write(reg, chsts->chstat1_r << 16 | chsts->chstat0_r);

	offset = EE_AUDIO_SPDIFOUT_B_CHSTS7 - EE_AUDIO_SPDIFOUT_CHSTS7;
	reg = EE_AUDIO_SPDIFOUT_CHSTS7 + offset * spdif_id;
	audiobus_write(reg, chsts->chstat1_r << 16 | chsts->chstat0_r);

	offset = EE_AUDIO_SPDIFOUT_B_CHSTS8 - EE_AUDIO_SPDIFOUT_CHSTS8;
	reg = EE_AUDIO_SPDIFOUT_CHSTS8 + offset * spdif_id;
	audiobus_write(reg, chsts->chstat1_r << 16 | chsts->chstat0_r);

	offset = EE_AUDIO_SPDIFOUT_B_CHSTS9 - EE_AUDIO_SPDIFOUT_CHSTS9;
	reg = EE_AUDIO_SPDIFOUT_CHSTS9 + offset * spdif_id;
	audiobus_write(reg, chsts->chstat1_r << 16 | chsts->chstat0_r);

	offset = EE_AUDIO_SPDIFOUT_B_CHSTSA - EE_AUDIO_SPDIFOUT_CHSTSA;
	reg = EE_AUDIO_SPDIFOUT_CHSTSA + offset * spdif_id;
	audiobus_write(reg, chsts->chstat1_r << 16 | chsts->chstat0_r);

	offset = EE_AUDIO_SPDIFOUT_B_CHSTSB - EE_AUDIO_SPDIFOUT_CHSTSB;
	reg = EE_AUDIO_SPDIFOUT_CHSTSB + offset * spdif_id;
	audiobus_write(reg, chsts->chstat1_r << 16 | chsts->chstat0_r);
}

void spdifout_play_with_zerodata(unsigned int spdif_id)
{
	pr_info("%s, spdif id:%d enable:%d\n",
		__func__,
		spdif_id,
		spdifout_is_enable(spdif_id));

	if (!spdifout_is_enable(spdif_id)) {
		unsigned int frddr_index = 0;
		unsigned int bitwidth = 32;
		unsigned int sample_rate = 48000;
		unsigned int src0_sel = 4; /* spdif b */
		struct iec958_chsts chsts;
		struct snd_pcm_substream substream;
		struct snd_pcm_runtime runtime;

		substream.runtime = &runtime;
		runtime.rate = sample_rate;
		runtime.format = SNDRV_PCM_FORMAT_S16_LE;
		runtime.channels = 2;
		runtime.sample_bits = 16;

		/* check whether fix to spdif a */
		if (spdif_id == 0)
			src0_sel = 3;

		/* spdif clk */
		spdifout_clk_ctrl(spdif_id, true);
		/* spdif to hdmitx */
		spdifout_to_hdmitx_ctrl(spdif_id);

		/* spdif ctrl */
		spdifout_fifo_ctrl(spdif_id, frddr_index, bitwidth);

		/* channel status info */
		spdif_get_channel_status_info(&chsts, sample_rate);
		spdif_set_channel_status_info(&chsts, spdif_id);

		/* notify hdmitx audio */
		aout_notifier_call_chain(0x1, &substream);

		/* init frddr to output zero data. */
		frddr_init_without_mngr(frddr_index, src0_sel);

		/* spdif enable */
		spdifout_enable(spdif_id, true);
	}
}

void spdifout_play_with_zerodata_free(unsigned int spdif_id)
{
	pr_info("%s, spdif id:%d\n",
		__func__,
		spdif_id);

	/* free frddr, then frddr in mngr */
	frddr_deinit_without_mngr(spdif_id);
}
