/*
 * sound/soc/amlogic/auge/resample_hw.h
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
#ifndef __AML_AUDIO_RESAMPLE_HW_H__
#define __AML_AUDIO_RESAMPLE_HW_H__

#include "ddr_mngr.h"

enum samplerate_index {
	RATE_OFF,
	RATE_32K,
	RATE_44K,
	RATE_48K,
	RATE_88K,
	RATE_96K,
	RATE_176K,
	RATE_192K,
};

extern void resample_enable(enum resample_idx id, bool enable);
extern int resample_init(enum resample_idx id, int input_sr);
extern int resample_disable(enum resample_idx id);
extern int resample_set_hw_param(enum resample_idx id,
		enum samplerate_index rate_index);
extern void resample_src_select(int src);
extern void resample_src_select_ab(enum resample_idx id, enum toddr_src src);
extern void resample_format_set(enum resample_idx id, int ch_num, int bits);

extern int resample_ctrl_read(enum resample_idx id);
extern void resample_ctrl_write(enum resample_idx id, int value);
int resample_set_hw_pause_thd(enum resample_idx id, unsigned int thd);

#endif
