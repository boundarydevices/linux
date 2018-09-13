/*
 * drivers/amlogic/atv_demod/atvauddemod_func.h
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

#ifndef __ATVAUDDEMOD_H_
#define __ATVAUDDEMOD_H_

#include "aud_demod_reg.h"

extern unsigned int signal_audmode;

extern uint32_t adec_rd_reg(uint32_t addr);
extern void adec_wr_reg(uint32_t reg, uint32_t val);
extern int aml_atvdemod_get_btsc_sap_mode(void);

void set_outputmode(uint32_t standard, uint32_t outmode);
void aud_demod_clk_gate(int on);
void configure_adec(int Audio_mode);
void adec_soft_reset(void);
void audio_thd_init(void);
void audio_thd_det(void);
void set_nicam_outputmode(uint32_t outmode);
void set_a2_eiaj_outputmode(uint32_t outmode);
void set_btsc_outputmode(uint32_t outmode);
void update_nicam_mode(int *nicam_flag, int *nicam_mono_flag,
		int *nicam_stereo_flag, int *nicam_dual_flag);
void update_btsc_mode(int auto_en, int *stereo_flag, int *sap_flag);
void update_a2_eiaj_mode(int auto_en, int *stereo_flag, int *dual_flag);

void set_output_left_right_exchange(unsigned int ch);

#endif /* __ATVAUDDEMOD_H_ */
