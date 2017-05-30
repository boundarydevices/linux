/*
 * sound/soc/amlogic/meson/audio_hw_pcm.h
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

#ifndef __AML_PCM_HW_H__
#define __AML_PCM_HW_H__

#include "sound/asound.h"
#include <sound/pcm.h>

#include "audio_hw.h"

void aml_set_pcm_format(int pcm_mode);
void pcm_in_enable(struct snd_pcm_substream *substream, int flag);
void pcm_in_set_buf(unsigned int addr, unsigned int size);
int  pcm_in_is_enable(void);
unsigned int pcm_in_rd_ptr(void);
unsigned int pcm_in_wr_ptr(void);
unsigned int pcm_in_set_rd_ptr(unsigned int value);
unsigned int pcm_in_fifo_int(void);

void pcm_out_enable(struct snd_pcm_substream *substream, int flag);
void pcm_out_mute(int flag);
void pcm_out_set_buf(unsigned int addr, unsigned int size);
int  pcm_out_is_enable(void);
int  pcm_out_is_mute(void);
unsigned int pcm_out_rd_ptr(void);
unsigned int pcm_out_wr_ptr(void);
unsigned int pcm_out_set_wr_ptr(unsigned int value);

void pcm_master_in_enable(struct snd_pcm_substream *substream, int flag);
void pcm_master_out_enable(struct snd_pcm_substream *substream, int flag);
#endif
