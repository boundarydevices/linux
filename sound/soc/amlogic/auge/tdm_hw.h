/*
 * sound/soc/amlogic/auge/tdm_hw.h
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

#ifndef __AML_TDM_HW_H__
#define __AML_TDM_HW_H__

#include "audio_io.h"
#include "regs.h"

//	TODO: fix me, now based by tl1
enum tdmin_src {
	PAD_TDMINA_DIN = 0,
	PAD_TDMINB_DIN = 1,
	PAD_TDMINC_DIN = 2,
	PAD_TDMINA_D = 4,
	PAD_TDMINB_D = 5,
	PAD_TDMINC_D = 6,
	HDMIRX_I2S = 7,
	ACODEC_ADC = 8,
	TDMOUTA = 13,
	TDMOUTB = 14,
	TDMOUTC = 15,
};

struct pcm_setting {
	unsigned int pcm_mode;
	unsigned int sysclk;
	unsigned int sysclk_bclk_ratio;
	unsigned int bclk;
	unsigned int bclk_lrclk_ratio;
	unsigned int lrclk;
	unsigned int tx_mask;
	unsigned int rx_mask;
	unsigned int slots;
	unsigned int slot_width;
	unsigned int pcm_width;
	unsigned int lane_mask_out;
	unsigned int lane_mask_in;
	/* lane oe (out pad) mask */
	unsigned int lane_oe_mask_out;
	unsigned int lane_oe_mask_in;
	/* lane in selected from out, for intrenal loopback */
	unsigned int lane_lb_mask_in;

	/* eco or sclk_ws_inv */
	bool sclk_ws_inv;
};

extern void aml_tdm_enable(
	struct aml_audio_controller *actrl,
	int stream, int index,
	bool is_enable);

extern void aml_tdm_arb_config(
	struct aml_audio_controller *actrl);

extern void aml_tdm_fifo_reset(
	struct aml_audio_controller *actrl,
	int stream, int index);

extern int tdmout_get_frddr_type(int bitwidth);

extern void aml_tdm_fifo_ctrl(
	struct aml_audio_controller *actrl,
	int bitwidth, int stream,
	int index, unsigned int fifo_id);

extern void aml_tdm_set_format(
	struct aml_audio_controller *actrl,
	struct pcm_setting *p_config,
	unsigned int clk_sel,
	unsigned int index,
	unsigned int fmt,
	unsigned int capture_active,
	unsigned int playback_active);

extern void aml_tdm_set_slot_out(
	struct aml_audio_controller *actrl,
	int index, int slots, int slot_width,
	int force_oe, int oe_val);

extern void aml_tdm_set_slot_in(
	struct aml_audio_controller *actrl,
	int index, int in_src, int slot_width);

extern void tdm_update_slot_in(
	struct aml_audio_controller *actrl,
	int index, int in_src);

extern void aml_tdm_set_channel_mask(
	struct aml_audio_controller *actrl,
	int stream, int index, int lanes, int mask);

extern void aml_tdm_set_lane_channel_swap(
	struct aml_audio_controller *actrl,
	int stream, int index, int swap);

extern void aml_tdm_set_bclk_ratio(
	struct aml_audio_controller *actrl,
	int clk_sel, int lrclk_hi, int bclk_ratio);

extern void aml_tdm_set_lrclkdiv(
	struct aml_audio_controller *actrl,
	int clk_sel, int ratio);

extern void tdm_enable(int tdm_index, int is_enable);

extern void tdm_fifo_enable(int tdm_index, int is_enable);

extern void aml_tdmout_select_aed(bool enable, int tdmout_id);

extern void aml_tdmout_get_aed_info(int tdmout_id,
	int *bitwidth, int *frddrtype);

extern void aml_tdm_clk_pad_select(
	struct aml_audio_controller *actrl,
	int mpad, int mclk_sel,
	int tdm_index, int clk_sel);

extern void i2s_to_hdmitx_ctrl(int tdm_index);
#endif
