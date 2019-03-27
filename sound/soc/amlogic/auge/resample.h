/*
 * sound/soc/amlogic/auge/resample.h
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
#ifndef __AML_AUDIO_RESAMPLE_H__
#define __AML_AUDIO_RESAMPLE_H__

#include "resample_hw.h"

extern int card_add_resample_kcontrols(struct snd_soc_card *card);

extern int resample_set(enum resample_idx id, enum samplerate_index index);

extern int get_resample_module_num(void);

int set_resample_source(enum resample_idx id, enum toddr_src src);

int resample_set_inner_rate(enum resample_idx id);

struct audioresample *get_audioresample(enum resample_idx id);

#endif
