/*
 * sound/soc/amlogic/auge/vad.h
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

#ifndef __VAD_H__
#define __VAD_H__

#include "ddr_mngr.h"

enum vad_src {
	VAD_SRC_TDMIN_A,
	VAD_SRC_TDMIN_B,
	VAD_SRC_TDMIN_C,
	VAD_SRC_SPDIFIN,
	VAD_SRC_PDMIN,
	VAD_SRC_LOOPBACK_B,
	VAD_SRC_TDMIN_LB,
	VAD_SRC_LOOPBACK_A,
};

extern void vad_update_buffer(int isvad);
extern int vad_transfer_chunk_data(unsigned long data, int frames);

extern bool vad_tdm_is_running(int tdm_idx);
extern bool vad_pdm_is_running(void);

extern void vad_enable(bool enable);
extern void vad_set_toddr_info(struct toddr *to);

extern void vad_set_trunk_data_readable(bool en);

extern int card_add_vad_kcontrols(struct snd_soc_card *card);

#endif
