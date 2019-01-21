/*
 * sound/soc/amlogic/auge/pdm_hw.h
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

#ifndef __AML_PDM_HW_H__
#define __AML_PDM_HW_H__
#include "audio_io.h"

#define PDM_CHANNELS_MAX		8 /* 8ch pdm in */
#define PDM_LANE_MAX            4 /* 4 data pins, for 8ch maxs*/

struct pdm_info {
	int bitdepth;
	int channels;
	int lane_masks;

	int dclk_idx;  /* mapping for dclk value */
	int bypass;    /* bypass all filter, capture raw data */
};

extern void aml_pdm_ctrl(struct pdm_info *info);

extern void aml_pdm_arb_config(struct aml_audio_controller *actrl);

extern int aml_pmd_set_HPF_filter_parameters(void *array);

extern void aml_pdm_filter_ctrl(int osr, int set);

extern void pdm_enable(int is_enable);

extern void pdm_fifo_reset(void);

extern int pdm_get_mute_value(void);
extern void pdm_set_mute_value(int val);
extern int pdm_get_mute_channel(void);
extern void pdm_set_mute_channel(int mute_chmask);

extern void pdm_init_truncate_data(int freq);

extern int pdm_hcic_shift_gain;
extern int pdm_dclk;

#endif /*__AML_PDM_HW_H__*/
