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

extern void aml_spdif_enable(
	struct aml_audio_controller *actrl,
	int stream,
	bool is_enable);

extern void aml_spdif_arb_config(struct aml_audio_controller *actrl);

extern void aml_spdifin_status_check(
	struct aml_audio_controller *actrl);

extern void aml_spdif_fifo_reset(
	struct aml_audio_controller *actrl,
	int stream);

extern void aml_spdif_fifo_ctrl(
	struct aml_audio_controller *actrl,
	int bitwidth,
	int stream,
	unsigned int fifo_id);

extern int spdifin_get_mode(void);

extern int spdif_get_channel_status(int reg);

extern void spdifin_set_channel_status(int ch, int bits);
#endif
