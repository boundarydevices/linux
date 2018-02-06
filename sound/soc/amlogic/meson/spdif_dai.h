/*
 * sound/soc/amlogic/meson/spdif_dai.h
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

#ifndef _AML_SPDIF_DAI_H
#define _AML_SPDIF_DAI_H

extern unsigned int IEC958_mode_codec;

/*
 * special call by the audiodsp,add these code,
 * as there are three cases for 958 s/pdif output
 * 1)NONE-PCM  raw output ,only available when ac3/dts audio,
 * when raw output mode is selected by user.
 * 2)PCM  output for  all audio, when pcm mode is selected by user .
 * 3)PCM  output for audios except ac3/dts,
 * when raw output mode is selected by user
 */
void aml_hw_iec958_init(struct snd_pcm_substream *substream, int samesrc);
int aml_set_spdif_clk(unsigned long rate, bool src_i2s);
void aml_spdif_play(int samesrc);
#endif  /* _AML_SPDIF_DAI_H */
