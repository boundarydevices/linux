/*
 * sound/soc/amlogic/auge/frhdmirx_hw.c
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
#include <linux/types.h>
#include <linux/kernel.h>

#include "frhdmirx_hw.h"
#include "regs.h"
#include "iomap.h"

void frhdmirx_enable(bool enable)
{
	if (enable) {
		audiobus_update_bits(EE_AUDIO_FRHDMIRX_CTRL0,
			0x1 << 29,
			0x1 << 29);
		audiobus_update_bits(EE_AUDIO_FRHDMIRX_CTRL0,
			0x1 << 28,
			0x1 << 28);
	} else
		audiobus_update_bits(EE_AUDIO_FRHDMIRX_CTRL0,
			0x3 << 28,
			0x0 << 28);

	audiobus_update_bits(EE_AUDIO_FRHDMIRX_CTRL0, 0x1 << 31, enable << 31);
}

/* source select
 * 0: select spdif lane;
 * 1: select PAO mode;
 */
void frhdmirx_src_select(int src)
{
	audiobus_update_bits(EE_AUDIO_FRHDMIRX_CTRL0,
		0x1 << 23,
		(bool)src << 23);
}

static void frhdmirx_enable_irq_bits(int channels, int src)
{
	int lane, int_bits = 0, i;

	if (channels % 2)
		lane = channels / 2 + 1;
	else
		lane = channels / 2;

	/* interrupt bits */
	if (src) { /* PAO mode */
		int_bits = (
			0x1 << INT_PAO_PAPB_MASK | /* find papb */
			0x1 << INT_PAO_PCPD_MASK   /* find pcpd changed */
			);
	} else { /* SPDIF Lane */
		int lane_irq_bits = (0x1 << 7 | /* lane: find papb */
			0x1 << 6 | /* lane: find papb */
			0x1 << 5 | /* lane: find nonpcm to pcm */
			0x1 << 4 | /* lane: find pcpd changed */
			0x1 << 3 | /* lane: find ch status changed */
			0x1 << 1 /* lane: find parity error */
			);

		for (i = 0; i < lane; i++)
			int_bits |= (lane_irq_bits << i);
	}
	audiobus_write(EE_AUDIO_FRHDMIRX_CTRL2, int_bits);
}

void frhdmirx_clr_irq_bits(int channels, int src)
{
	int lane, int_clr_mask = 0, i;

	if (channels % 2)
		lane = channels / 2 + 1;
	else
		lane = channels / 2;

	/* interrupt bits */
	if (src) { /* PAO mode */
		int_clr_mask = (
			0x1 << INT_PAO_PAPB_MASK | /* find papb */
			0x1 << INT_PAO_PCPD_MASK   /* find pcpd changed */
			);
	} else { /* SPDIF Lane */
		int lane_irq_bits = (0x1 << 7 | /* lane: find papb */
			0x1 << 6 | /* lane: find valid changed; */
			0x1 << 5 | /* lane: find nonpcm to pcm */
			0x1 << 4 | /* lane: find pcpd changed */
			0x1 << 3 | /* lane: find ch status changed */
			0x1 << 1 /* lane: find parity error */
			);

		for (i = 0; i < lane; i++)
			int_clr_mask |= (lane_irq_bits << i);
	}
	audiobus_write(EE_AUDIO_FRHDMIRX_CTRL3, ~int_clr_mask);
	audiobus_write(EE_AUDIO_FRHDMIRX_CTRL3, int_clr_mask);
	audiobus_write(EE_AUDIO_FRHDMIRX_CTRL4, ~int_clr_mask);
	audiobus_write(EE_AUDIO_FRHDMIRX_CTRL4, int_clr_mask);
}

void frhdmirx_ctrl(int channels, int src)
{
	int lane, lane_mask = 0, i;

	/* PAO mode */
	if (src) {
		audiobus_write(EE_AUDIO_FRHDMIRX_CTRL0,
			0x1 << 22 | /* capture input by fall edge*/
			0x1 << 8  | /* start detect PAPB */
			0x4 << 4    /* chan status sel: pao pc/pd value */
			);
	} else {
		if (channels % 2)
			lane = channels / 2 + 1;
		else
			lane = channels / 2;

		for (i = 0; i < lane; i++)
			lane_mask |= (1 << i);

		audiobus_update_bits(EE_AUDIO_FRHDMIRX_CTRL0,
			0x1 << 30 | 0xf << 24 | 0x1 << 22 | 0x3 << 11,
			0x1 << 30 | /* chnum_sel */
			lane_mask << 24 | /* chnum_sel */
			0x1 << 22 | /* clk_inv */
			0x0 << 11  /* req_sel, Sync 4 spdifin by which */
			);
	}
	/* nonpcm2pcm_th */
	audiobus_write(EE_AUDIO_FRHDMIRX_CTRL1, 0xff << 20);

	/* enable irq bits */
	frhdmirx_enable_irq_bits(channels, src);
}

void frhdmirx_clr_PAO_irq_bits(void)
{
	audiobus_update_bits(EE_AUDIO_FRHDMIRX_CTRL4,
		0x1 << INT_PAO_PAPB_MASK | 0x1 << INT_PAO_PCPD_MASK,
		0x1 << INT_PAO_PAPB_MASK | 0x1 << INT_PAO_PCPD_MASK);

	audiobus_update_bits(EE_AUDIO_FRHDMIRX_CTRL4,
		0x1 << INT_PAO_PAPB_MASK | 0x1 << INT_PAO_PCPD_MASK,
		0x0 << INT_PAO_PAPB_MASK | 0x0 << INT_PAO_PCPD_MASK);
}

unsigned int frhdmirx_get_ch_status0to31(void)
{
	return (unsigned int)audiobus_read(EE_AUDIO_FRHDMIRX_STAT0);
}

unsigned int frhdmirx_get_chan_status_pc(void)
{
	unsigned int val;

	val = audiobus_read(EE_AUDIO_FRHDMIRX_STAT1);
	return (val >> 16) & 0xff;
}
