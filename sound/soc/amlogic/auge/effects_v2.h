/*
 * sound/soc/amlogic/auge/effects_v2.h
 *
 * Copyright (C) 2018 Amlogic, Inc. All rights reserved.
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
#ifndef __EFFECTS_V2_H__
#define __EFFECTS_V2_H__

extern bool check_aed_v2(void);
extern int card_add_effect_v2_kcontrols(struct snd_soc_card *card);

#endif
