/*
 * sound/soc/amlogic/auge/pwrdet_hw.c
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
#include "pwrdet_hw.h"

void pwrdet_threshold(int shift, int hi_th, int lo_th)
{
	audiobus_write(EE_AUDIO_POW_DET_TH_HI + shift, hi_th);

	audiobus_write(EE_AUDIO_POW_DET_TH_LO + shift, lo_th);
}

void pwrdet_downsample_ctrl(bool dw_en, bool fast_en, int dw_sel, int fast_sel)
{
	audiobus_update_bits(EE_AUDIO_POW_DET_CTRL0,
		0x1 << 30 | 0xf << 16 | 0x1 << 28 | 0xf << 20,
		dw_en << 30 | dw_sel << 16 | fast_en << 28 | fast_sel);
}

void aml_pwrdet_format_set(int toddr_type, int msb, int lsb)
{
	audiobus_update_bits(EE_AUDIO_POW_DET_CTRL0,
		0x7 << 13 | 0x1f << 8 | 0x1f << 3,
		toddr_type << 13 | msb << 8 | lsb << 3);
}

void pwrdet_src_select(bool enable, int src)
{
	audiobus_update_bits(EE_AUDIO_POW_DET_CTRL0,
		0x1 << 31 | 0x7 << 0,
		enable << 31 | src << 0);
}
