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

/*#define G12A_PTM*/
/*#define G12A_PTM_LB_INTERNAL*/

void aml_spdif_enable(
	struct aml_audio_controller *actrl,
	int stream,
	int index,
	bool is_enable)
{
	pr_info("spdif stream :%d is_enable:%d\n", stream, is_enable);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		unsigned int offset, reg;

		offset = EE_AUDIO_SPDIFOUT_B_CTRL0 - EE_AUDIO_SPDIFOUT_CTRL0;
		reg = EE_AUDIO_SPDIFOUT_CTRL0 + offset * index;
		aml_audiobus_update_bits(actrl,
			reg, 1<<31, is_enable<<31);
	} else {
		aml_audiobus_update_bits(actrl,
			EE_AUDIO_SPDIFIN_CTRL0, 1<<31, is_enable<<31);
#ifdef G12A_PTM_LB_INTERNAL
		if (index == 0)
		aml_audiobus_update_bits(actrl,
			EE_AUDIO_SPDIFIN_CTRL0, 0x3<<4, 0x1<<4);
#endif
	}
}

void aml_spdif_arb_config(struct aml_audio_controller *actrl)
{
	/* config ddr arb */
	aml_audiobus_write(actrl, EE_AUDIO_ARB_CTRL, 1<<31|0xff<<0);
}

void aml_spdifin_status_check(struct aml_audio_controller *actrl)
{
	unsigned int val;

	val = aml_audiobus_read(actrl,
		EE_AUDIO_SPDIFIN_STAT0);

	/* pr_info("\t--- spdif handles status0 %#x\n", val); */

	aml_audiobus_update_bits(actrl,
			EE_AUDIO_SPDIFIN_CTRL0,
			1<<26,
			1<<26);
	aml_audiobus_update_bits(actrl,
			EE_AUDIO_SPDIFIN_CTRL0,
			1<<26,
			0);
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

void aml_spdif_fifo_ctrl(
	struct aml_audio_controller *actrl,
	int bitwidth,
	int stream,
	int index,
	unsigned int fifo_id)
{
	unsigned int frddr_type, toddr_type;

	switch (bitwidth) {
	case 8:
		frddr_type = 0;
		toddr_type = 0;
		break;
	case 16:
		frddr_type = 1;
		toddr_type = 1;
		break;
	case 24:
		frddr_type = 4;
		toddr_type = 4;
		break;
	case 32:
		frddr_type = 3;
		toddr_type = 3;
		break;
	default:
		pr_err("runtime format invalid bitwidth: %d\n",
			bitwidth);
		return;
	}

	pr_info("%s, bit depth:%d, frddr type:%d, toddr:type:%d\n",
	__func__, bitwidth, frddr_type, toddr_type);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		unsigned int offset, reg;

		/* mask lane 0 L/R channels */
		offset = EE_AUDIO_SPDIFOUT_B_CTRL0 - EE_AUDIO_SPDIFOUT_CTRL0;
		reg = EE_AUDIO_SPDIFOUT_CTRL0 + offset * index;
		aml_audiobus_update_bits(actrl,
			reg,
			0x1<<29|0x1<<28|0x1<<20|0x1<<19|0xff<<4,
			1<<29|1<<28|0<<20|0<<19|0x3<<4);

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
		unsigned int lsb;

		if (bitwidth <= 24)
			lsb = 28 - bitwidth;
		else
			lsb = 4;

		// 250M
#ifdef G12A_PTM
		aml_audiobus_write(actrl,
			EE_AUDIO_SPDIFIN_CTRL1,
			25000 << 0);
#else
		aml_audiobus_write(actrl,
			EE_AUDIO_SPDIFIN_CTRL1,
			0xff << 20 | 25000 << 0);
#endif
		aml_audiobus_write(actrl,
			EE_AUDIO_SPDIFIN_CTRL2,
			140 << 20 | 100 << 10 | 86 << 0);

		aml_audiobus_write(actrl,
			EE_AUDIO_SPDIFIN_CTRL3,
			83 << 20 | 60 << 10 | 30 << 0);

		aml_audiobus_write(actrl,
			EE_AUDIO_SPDIFIN_CTRL4,
			(81<<24) | /* reg_sample_mode0_timer */
			(61<<16) | /* reg_sample_mode1_timer */
			(44<<8) | /* reg_sample_mode2_timer*/
			(42<<0)
			);

#ifdef G12A_PTM
		aml_audiobus_write(actrl,
			EE_AUDIO_SPDIFIN_CTRL5,
			(40<<24) |
			(20<<16) |
			(10<<8) |
			(0<<0)
			);
#else
		aml_audiobus_write(actrl,
			EE_AUDIO_SPDIFIN_CTRL5,
			(40<<24) | /* reg_sample_mode4_timer	  = 5[31:24]; */
			(20<<16) | /* reg_sample_mode5_timer	  = 5[23:16]; */
			(9<<8) |  /* reg_sample_mode6_timer   = 5[15:8]; */
			(0<<0)	   /* reg_sample_mode7_timer	  = 5[7:0]; */
			);
#endif

		aml_audiobus_update_bits(actrl,
			EE_AUDIO_SPDIFIN_CTRL0,
			0x3<<24|1<<12,
			3<<24|1<<12);
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
	audiobus_update_bits(reg, 0x1 << 31, enable << 31);
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

void spdifoutb_to_hdmitx_ctrl(void)
{
	audiobus_write(EE_AUDIO_TOHDMITX_CTRL0,
		1 << 31
		| 1 << 3 /* spdif_clk_cap_inv */
		| 0 << 2 /* spdif_clk_inv */
		| 1 << 1 /* spdif_out_b */
		| 1 << 0 /* spdif_clk_b */
	);
}
