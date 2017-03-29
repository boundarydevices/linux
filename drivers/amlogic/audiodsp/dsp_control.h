/*
 * drivers/amlogic/audiodsp/dsp_control.h
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

#ifndef DSP_CONTROL_HEADER
#define DSP_CONTROL_HEADER

#include <asm/cacheflush.h>
#include "audiodsp_module.h"
#include "dsp_microcode.h"
/*#include <asm/system.h>*/
#include <linux/dma-mapping.h>

void halt_dsp(struct audiodsp_priv *priv);
void reset_dsp(struct audiodsp_priv *priv);
int dsp_start(struct audiodsp_priv *priv, struct audiodsp_microcode *mcode);
int dsp_stop(struct audiodsp_priv *priv);
int dsp_check_status(struct audiodsp_priv *priv);

extern unsigned int IEC958_mode_raw;
extern unsigned int IEC958_mode_codec;
extern unsigned int audioin_mode;

#define AUDIODSP_RESET
#endif				/*  */
