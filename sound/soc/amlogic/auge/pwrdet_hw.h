/*
 * sound/soc/amlogic/auge/pwrdet_hw.h
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
#ifndef __POWER_DETECT_HW_H__
#define __POWER_DETECT_HW_H__
#include <linux/types.h>

#include "regs.h"
#include "iomap.h"

extern void pwrdet_threshold(int shift, int hi_th, int lo_th);

extern void pwrdet_downsample_ctrl(bool dw_en, bool fast_en,
	int dw_sel, int fast_sel);

extern void aml_pwrdet_format_set(int toddr_type, int msb, int lsb);

extern void pwrdet_src_select(bool enable, int src);
#endif
