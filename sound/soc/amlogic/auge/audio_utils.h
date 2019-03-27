/*
 * sound/soc/amlogic/auge/audio_utils.h
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

#ifndef __AML_AUDIO_UTILS_H__
#define __AML_AUDIO_UTILS_H__

#include <sound/soc.h>

extern int snd_card_add_kcontrols(struct snd_soc_card *card);

extern void audio_locker_set(int enable);

extern int audio_locker_get(void);

extern void fratv_enable(bool enable);
extern void fratv_src_select(int src);

extern void cec_arc_enable(int src, bool enable);
#endif
