/*
 * drivers/amlogic/audiodsp/dsp_codec.h
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

#ifndef DSP_CODEC_CTL_H
#define DSP_CODEC_CTL_H

#include "dsp_control.h"
#include "codec_message.h"
int dsp_codec_start(struct audiodsp_priv *priv);
int dsp_codec_stop(struct audiodsp_priv *priv);
int dsp_codec_get_bufer_data_len(struct audiodsp_priv *priv);
static inline unsigned long dsp_codec_get_rd_addr(struct audiodsp_priv *priv)
{
	return ARC_2_ARM_ADDR_SWAP(DSP_RD(DSP_DECODE_OUT_RD_ADDR));
};

static inline unsigned long dsp_codec_get_wd_addr(struct audiodsp_priv *priv)
{
	return ARC_2_ARM_ADDR_SWAP(DSP_RD(DSP_DECODE_OUT_WD_ADDR));
};

unsigned long dsp_codec_inc_rd_addr(struct audiodsp_priv *priv, int size);
u32 dsp_codec_get_current_pts(struct audiodsp_priv *priv);

#endif				/*  */
