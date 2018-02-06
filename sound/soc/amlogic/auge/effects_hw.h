/*
 * sound/soc/amlogic/auge/effects_hw.h
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
#ifndef __AML_AUDIO_EFFECTS_HW_H__
#define __AML_AUDIO_EFFECTS_HW_H__
#include <linux/types.h>
#include <linux/errno.h>

#include "regs.h"
#include "iomap.h"

extern int DRC0_enable(int enable, int thd0, int k0);
extern int init_EQ_DRC_module(void);
extern int set_internal_EQ_volume(
	unsigned int master_volume,
	unsigned int channel_1_volume,
	unsigned int channel_2_volume);

extern void aed_req_sel(bool enable, int sel, int req_module);
extern int aed_get_req_sel(int sel);
extern void aed_set_eq(int enable, int params_len, unsigned int *params);
extern void aed_set_drc(int enable,
	int drc_len, unsigned int *drc_params,
	int drc_tko_len, unsigned int *drc_tko_params);
extern int aml_aed_format_set(int frddr_dst);
extern void aed_src_select(bool enable, int frddr_dst, int fifo_id);
extern void aed_set_lane(int lane_mask);
extern void aed_set_channel(int channel_mask);
#endif
