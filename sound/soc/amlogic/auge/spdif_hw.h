/*
 * sound/soc/amlogic/auge/spdif_hw.h
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

#ifndef __AML_SPDIF_HW_H__
#define __AML_SPDIF_HW_H__
#include "audio_io.h"
#include "regs.h"

#include <linux/amlogic/media/sound/spdif_info.h>

extern unsigned int aml_spdif_ctrl_read(struct aml_audio_controller *actrl,
	int stream, int index);
extern void aml_spdif_ctrl_write(struct aml_audio_controller *actrl,
	int stream, int index, int val);
extern void aml_spdifin_chnum_en(struct aml_audio_controller *actrl,
	int index, bool is_enable);
extern void aml_spdif_enable(
	struct aml_audio_controller *actrl,
	int stream,
	int index,
	bool is_enable);

extern void aml_spdif_mute(
	struct aml_audio_controller *actrl,
	int stream,
	int index,
	bool is_mute);

extern void aml_spdif_arb_config(struct aml_audio_controller *actrl);

extern int aml_spdifin_status_check(
	struct aml_audio_controller *actrl);
extern void aml_spdifin_clr_irq(struct aml_audio_controller *actrl,
	bool is_all_bits, int clr_bits_val);

extern void aml_spdif_fifo_reset(
	struct aml_audio_controller *actrl,
	int stream, int index);

extern int spdifout_get_frddr_type(int bitwidth);

extern void aml_spdif_fifo_ctrl(
	struct aml_audio_controller *actrl,
	int bitwidth,
	int stream,
	int index,
	unsigned int fifo_id);

extern int spdifin_get_mode(void);

extern int spdif_get_channel_status(int reg);

extern void spdifin_set_channel_status(int ch, int bits);

extern void aml_spdifout_select_aed(bool enable, int spdifout_id);

extern void aml_spdifout_get_aed_info(int spdifout_id,
	int *bitwidth, int *frddrtype);

extern void spdifout_to_hdmitx_ctrl(int spdif_index);

extern void spdifout_samesource_set(int spdif_index, int fifo_id,
	int bitwidth, bool is_enable);
extern void spdifout_enable(int spdif_id, bool is_enable);

extern int spdifin_get_sample_rate(void);

extern int spdifin_get_ch_status0to31(void);

extern int spdifin_get_audio_type(void);

extern void spdif_set_channel_status_info(
	struct iec958_chsts *chsts, int spdif_id);

extern void spdifout_play_with_zerodata(unsigned int spdif_id);
extern void spdifout_play_with_zerodata_free(unsigned int spdif_id);
#endif
