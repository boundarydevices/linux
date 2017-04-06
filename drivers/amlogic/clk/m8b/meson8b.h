/*
 * drivers/amlogic/clk/m8b/meson8b.h
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

#ifndef __MESON8B_H
#define __MESON8B_H

/*
 * Clock controller register offsets
 *
 * Register offsets from the HardKernel[0] data sheet are listed in comment
 * blocks below. Those offsets must be multiplied by 4 before adding them to
 * the base address to get the right value
 *
 * [0] http://dn.odroid.com/S805/Datasheet/S805_Datasheet%20V0.8%2020150126.pdf
 */
#define HHI_GCLK_MPEG0			0x140 /* 0x50 offset in data sheet */
#define HHI_GCLK_MPEG1			0x144 /* 0x51 offset in data sheet */
#define HHI_GCLK_MPEG2			0x148 /* 0x52 offset in data sheet */
#define HHI_GCLK_OTHER			0x150 /* 0x54 offset in data sheet */
#define HHI_GCLK_AO			0x154 /* 0x55 offset in data sheet */
#define HHI_SYS_CPU_CLK_CNTL1		0x15c /* 0x57 offset in data sheet */
#define HHI_MPEG_CLK_CNTL		0x174 /* 0x5d offset in data sheet */

#define HHI_AUD_CLK_CNTL		0x178 /* 0x5e offset in data sheet */

#define HHI_AUD_CLK_CNTL2		0x190 /* 0x64 offset in data sheet */
#define HHI_MALI_CLK_CNTL		0x1b0 /* 0x6c offset in data sheet */
#define HHI_VPU_CLK_CNTL		0x1bC /* 0x6f offset in data sheet */

#define HHI_VDEC_CLK_CNTL		0x1E0 /* 0x78 offset in data sheet */
#define HHI_VDEC2_CLK_CNTL		0x1E4 /* 0x79 offset in data sheet */
#define HHI_VDEC3_CLK_CNTL		0x1E8 /* 0x7a offset in data sheet */
#define HHI_VDEC4_CLK_CNTL		0x1EC /* 0x7b offset in data sheet */

#define HHI_VDIN_MEAS_CLK_CNTL		0x250 /* 0x94 offset in data sheet */
#define HHI_PCM_CLK_CNTL		0x258 /* 0x96 offset in data sheet */
#define HHI_NAND_CLK_CNTL		0x25C /* 0x97 offset in data sheet */
#define HHI_SD_EMMC_CLK_CNTL		0x264 /* 0x99 offset in data sheet */
 /*
  * MPLL register offeset taken from the S905 datasheet. Vendor kernel source
  * confirm these are the same for the S805.
  */
#define HHI_MPLL_CNTL	0x280 /* 0xa0 offset in data sheet */
#define HHI_MPLL_CNTL2	0x284 /* 0xa1 offset in data sheet */
#define HHI_MPLL_CNTL3	0x288 /* 0xa2 offset in data sheet */
#define HHI_MPLL_CNTL4	0x28C /* 0xa3 offset in data sheet */
#define HHI_MPLL_CNTL5	0x290 /* 0xa4 offset in data sheet */
#define HHI_MPLL_CNTL6	0x294 /* 0xa5 offset in data sheet */
#define HHI_MPLL_CNTL7	0x298 /* 0xa6 offset in data sheet */
#define HHI_MPLL_CNTL8	0x29C /* 0xa7 offset in data sheet */
#define HHI_MPLL_CNTL9	0x2A0 /* 0xa8 offset in data sheet */
#define HHI_MPLL_CNTL10	0x2A4 /* 0xa9 offset in data sheet */

#define HHI_SYS_PLL_CNTL		0x300 /* 0xc0 offset in data sheet */
#define HHI_VID_PLL_CNTL		0x320 /* 0xc8 offset in data sheet */
/* include the CLKIDs that have been made part of the stable DT binding */
#include <dt-bindings/clock/meson8b-clkc.h>

#endif /* __MESON8B_H */
