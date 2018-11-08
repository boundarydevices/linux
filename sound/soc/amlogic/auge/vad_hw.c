/*
 * sound/soc/amlogic/auge/vad_hw.c
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
 */

#include "vad_hw.h"


void vad_set_ram_coeff(int len, int *params)
{
	int i, ctrl_v;

	for (i = 0; i < len; i++) {
		ctrl_v = 0x1 << 31 | (i << 0);
		vad_write(VAD_LUT_WR, params[i]);
		vad_write(VAD_LUT_CTRL, ctrl_v);
	}
}

/* parameters for downsample and emphasis filter */
void vad_set_de_params(int len, int *params)
{
	int i;

	for (i = 0; i < len; i++)
		vad_write(VAD_FIR_CTRL + i, params[i]);
}

/* Power detection */
void vad_set_pwd(void)
{
	/* frame for 32 ms */
	vad_write(VAD_FRAME_CTRL0,
		0x2 << 30 |
		0x1 << 24 |
		0x1 << 16);

	vad_write(VAD_FRAME_CTRL1, 0x00000d65);
	vad_write(VAD_FRAME_CTRL2, 0xd00103ff);
}

void vad_set_cep(void)
{
	vad_write(VAD_CEP_CTRL0, 0x11050000);
	vad_write(VAD_CEP_CTRL1, 0x0000001b);
	vad_write(VAD_CEP_CTRL2, 0xc001fd);
	vad_write(VAD_CEP_CTRL3, 0x137f0000);
	vad_write(VAD_CEP_CTRL4, 0x186d0000);
	vad_write(VAD_CEP_CTRL5, 0xfd00f61);
	vad_write(VAD_DEC_CTRL, 0x10030001);
}

void vad_set_src(int src)
{
	audiobus_update_bits(EE_AUDIO_TOVAD_CTRL0,
		0x7 << 12,
		src << 12);
}

void vad_set_in(void)
{
	/* two channel enable */
	vad_write(VAD_IN_SEL0, 0x00000001);
	vad_write(VAD_IN_SEL1, 0x00000002);

	vad_write(VAD_TO_DDR, 0xa0000719);
}

void vad_set_enable(bool enable)
{
	audiobus_update_bits(EE_AUDIO_TOVAD_CTRL0,
		0x1 << 31 | 0x1 << 30,
		enable << 31 | 0x1 << 30);

	if (enable) {
		vad_write(VAD_TOP_CTRL0, 0x7ff);
		vad_write(VAD_TOP_CTRL0, 0x0);

		vad_write(VAD_TOP_CTRL1, 0xff);
		vad_write(VAD_TOP_CTRL1, 0x0);

		vad_update_bits(VAD_TOP_CTRL0,
			0xfff << 20,
			1 << 31 | /* vad_en */
			1 << 30 | /* dec_fir_en */
			1 << 29 | /* pre_emp_en */
			1 << 28 | /* pre_ram_en */
			1 << 27 | /* frame_his_en */
			1 << 23 | /* ceps_ceps_en */
			1 << 22 | /* ceps_spec_en */
			0 << 20   /* two_channel_en */
		);
	} else {
		vad_write(VAD_TOP_CTRL0, 0x0);

		vad_write(VAD_TOP_CTRL1, 0x0);
	}
}
