/*
 * drivers/amlogic/audiodsp/dsp_mailbox.h
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

#ifndef DSP_IRQ_HEADER
#define  DSP_IRQ_HEADER
#include "audiodsp_module.h"
int audiodsp_init_mailbox(struct audiodsp_priv *priv);
int audiodsp_release_mailbox(struct audiodsp_priv *priv);
int dsp_mailbox_send(struct audiodsp_priv *priv, int overwrite, int num,
		      int cmd, const char *data, int len);
int audiodsp_get_audioinfo(struct audiodsp_priv *priv);

#if 0
#define pre_read_mailbox(reg)	\
	dma_cache_inv((unsigned long)reg, sizeof(*reg))
#define after_change_mailbox(reg)	\
	dma_cache_wback((unsigned long)reg, sizeof(*reg))
#else				/*  */
#define pre_read_mailbox(reg)
#define after_change_mailbox(reg)
#endif				/*  */
enum DSP_CMD {
	DSP_CMD_SET_EQ_PRESET = 0,
	DSP_CMD_SET_EQ_CUSTOMIZE,
	DSP_CMD_GET_EQ_VALUE_SETS,
	DSP_CMD_SET_SRS_SURROUND,
	DSP_CMD_SET_SRS_TRUBASS,
	DSP_CMD_SET_SRS_DIALOG_CLARITY,
	DSP_CMD_SET_HDMI_SR,
	DSP_CMD_SET_SRS_TRUBASS_GAIN,
	DSP_CMD_SET_SRS_DIALOG_CLARITY_GAIN,
	DSP_CMD_SET_REINIT_FILTER,
	DSP_CMD_COUNT
};

extern unsigned int IEC958_bpf;
extern unsigned int IEC958_brst;
extern unsigned int IEC958_length;
extern unsigned int IEC958_padsize;
extern unsigned int IEC958_mode;
extern unsigned int IEC958_syncword1;
extern unsigned int IEC958_syncword2;
extern unsigned int IEC958_syncword3;
extern unsigned int IEC958_syncword1_mask;
extern unsigned int IEC958_syncword2_mask;
extern unsigned int IEC958_syncword3_mask;
extern unsigned int IEC958_chstat0_l;
extern unsigned int IEC958_chstat0_r;
extern unsigned int IEC958_chstat1_l;
extern unsigned int IEC958_chstat1_r;
extern unsigned int IEC958_mode_raw;
extern unsigned int IEC958_mode_codec;
extern void set_pcminfo_data(void *pcm_encoded_info);

#define VDEC_ASSIST_MBOX1_IRQ_REG		0x0074
#define VDEC_ASSIST_MBOX1_CLR_REG		0x0075
#define VDEC_ASSIST_MBOX1_FIQ_SEL		0x0077
#define VDEC_ASSIST_MBOX2_IRQ_REG		0x0078
#define VDEC_ASSIST_MBOX2_MASK			0x007a
#define VDEC_ASSIST_MBOX2_FIQ_SEL		0x007b

#endif				/*  */
