/*
 * sound/soc/amlogic/auge/sharebuffer.h
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
#ifndef __AML_AUDIO_SHAREBUFFER_H__
#define __AML_AUDIO_SHAREBUFFER_H__

extern int sharebuffer_prepare(struct snd_pcm_substream *substream,
	void *pfrddr, int samesource_sel, int lane_i2s);
extern int sharebuffer_free(struct snd_pcm_substream *substream,
		void *pfrddr, int samesource_sel);
extern int sharebuffer_trigger(int cmd, int samesource_sel, bool reenable);

extern void sharebuffer_get_mclk_fs_ratio(int samesource_sel,
	int *pll_mclk_ratio, int *mclk_fs_ratio);
#endif
