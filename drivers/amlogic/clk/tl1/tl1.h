/*
 * drivers/amlogic/clk/tl1/tl1.h
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

#ifndef __TL1_H
#define __TL1_H

/*
 * Clock controller register offsets
 *
 * Register offsets from the data sheet are listed in comment blocks below.
 * Those offsets must be multiplied by 4 before adding them to the base address
 * to get the right value
 */
#define HHI_GP0_PLL_CNTL0		0x40 /* 0x10 offset in datasheet very*/
#define HHI_GP0_PLL_CNTL1		0x44 /* 0x11 offset in datasheet */
#define HHI_GP0_PLL_CNTL2		0x48 /* 0x12 offset in datasheet */
#define HHI_GP0_PLL_CNTL3		0x4c /* 0x13 offset in datasheet */
#define HHI_GP0_PLL_CNTL4		0x50 /* 0x14 offset in datasheet */
#define HHI_GP0_PLL_CNTL5		0x54 /* 0x15 offset in datasheet */
#define HHI_GP0_PLL_CNTL6		0x58 /* 0x16 offset in datasheet */
#define HHI_GP0_PLL_STS			0x5c /* 0x17 offset in datasheet */
#define HHI_GP1_PLL_CNTL0		0x60 /* 0x18 offset in datasheet */
#define HHI_GP1_PLL_CNTL1		0x64 /* 0x19 offset in datasheet */
#define HHI_GP1_PLL_CNTL2		0x68 /* 0x1a offset in datasheet */
#define HHI_GP1_PLL_CNTL3		0x6c /* 0x1b offset in datasheet */
#define HHI_GP1_PLL_CNTL4		0x70 /* 0x1c offset in datasheet */
#define HHI_GP1_PLL_CNTL5		0x74 /* 0x1d offset in datasheet */
#define HHI_GP1_PLL_CNTL6		0x78 /* 0x1e offset in datasheet */
#define HHI_GP1_PLL_STS			0x7c /* 0x1f offset in datasheet very*/

#define HHI_HIFI_PLL_CNTL0		0xd8 /* 0x36 offset in datasheet very*/
#define HHI_HIFI_PLL_CNTL1		0xdc /* 0x37 offset in datasheet */
#define HHI_HIFI_PLL_CNTL2		0xe0 /* 0x38 offset in datasheet */
#define HHI_HIFI_PLL_CNTL4		0xe4 /* 0x39 offset in datasheet */
#define HHI_HIFI_PLL_CNTL5		0xe8 /* 0x3a offset in datasheet */
#define HHI_HIFI_PLL_CNTL6		0xec /* 0x3b offset in datasheet */
#define HHI_HIFI_PLL_STS		0xf0 /* 0x3c offset in datasheet very*/

#define HHI_GCLK_MPEG0			0x140 /* 0x50 offset in datasheet */
#define HHI_GCLK_MPEG1			0x144 /* 0x51 offset in datasheet */
#define HHI_GCLK_MPEG2			0x148 /* 0x52 offset in datasheet */
#define HHI_GCLK_OTHER			0x150 /* 0x54 offset in datasheet */

#define HHI_GCLK_AO			0x154 /* 0x55 offset in datasheet */

#define HHI_VID_CLK_DIV			0x164 /* 0x59 offset in datasheet */
#define HHI_SPICC_HCLK_CNTL		0x168 /* 0x5a offset in datasheet */

#define HHI_MPEG_CLK_CNTL		0x174 /* 0x5d offset in datasheet */
#define HHI_VID_CLK_CNTL		0x17c /* 0x5f offset in datasheet */
#define HHI_TS_CLK_CNTL			0x190 /* 0x64 offset in datasheet1 */
#define HHI_VID_CLK_CNTL2		0x194 /* 0x65 offset in datasheet */
#define HHI_SYS_CPU_CLK_CNTL0		0x19c /* 0x67 offset in datasheet */
#define HHI_AUD_CLK_CNTL3		0x1A4 /* 0x69 offset in datasheet */
#define HHI_AUD_CLK_CNTL4		0x1A8 /* 0x6a offset in datasheet */
#define HHI_MALI_CLK_CNTL		0x1b0 /* 0x6c offset in datasheet */
#define HHI_VPU_CLKC_CNTL		0x1b4 /* 0x6d offset in datasheet1 */
#define HHI_VPU_CLK_CNTL		0x1bC /* 0x6f offset in datasheet1 */
#define HHI_AUDPLL_CLK_OUT_CNTL	0x1E0 /* 0x74 offset in datasheet1 */
#define HHI_VDEC_CLK_CNTL		0x1E0 /* 0x78 offset in datasheet1 */
#define HHI_VDEC2_CLK_CNTL		0x1E4 /* 0x79 offset in datasheet1 */
#define HHI_VDEC3_CLK_CNTL		0x1E8 /* 0x7a offset in datasheet1 */
#define HHI_VDEC4_CLK_CNTL		0x1EC /* 0x7b offset in datasheet1 */
#define HHI_HDCP22_CLK_CNTL		0x1F0 /* 0x7c offset in datasheet1 */
#define HHI_VAPBCLK_CNTL		0x1F4 /* 0x7d offset in datasheet1 */
#define HHI_HDMIRX_CLK_CNTL		0x200 /* 0x80 offset in datasheet1 */
#define HHI_HDMIRX_AUD_CLK_CNTL		0x204 /* 0x81 offset in datasheet1 */
#define HHI_VPU_CLKB_CNTL		0x20C /* 0x83 offset in datasheet1 */

#define HHI_VDIN_MEAS_CLK_CNTL		0x250 /* 0x94 offset in datasheet1 */
#define HHI_NAND_CLK_CNTL		0x25C /* 0x97 offset in datasheet1*/
#define HHI_SD_EMMC_CLK_CNTL		0x264 /* 0x99 offset in datasheet1*/
#define HHI_TCON_CLK_CNTL		0x270 /* 0x9c offset in datasheet1*/

#define HHI_MPLL_CNTL0			0x278 /* 0x9e offset in datasheet very*/
#define HHI_MPLL_CNTL1			0x27c /* 0x9f offset in datasheet */
#define HHI_MPLL_CNTL2			0x280 /* 0xa0 offset in datasheet */
#define HHI_MPLL_CNTL3			0x284 /* 0xa1 offset in datasheet */
#define HHI_MPLL_CNTL4			0x288 /* 0xa2 offset in datasheet */
#define HHI_MPLL_CNTL5			0x28c /* 0xa3 offset in datasheet */
#define HHI_MPLL_CNTL6			0x290 /* 0xa4 offset in datasheet */
#define HHI_MPLL_CNTL7			0x294 /* 0xa5 offset in datasheet */
#define HHI_MPLL_CNTL8			0x298 /* 0xa6 offset in datasheet */
#define HHI_MPLL_STS			0x29c /* 0xa7 offset in datasheet very*/

#define HHI_FIX_PLL_CNTL0		0x2a0 /* 0xa8 offset in datasheet very*/
#define HHI_FIX_PLL_CNTL1		0x2a4 /* 0xa9 offset in datasheet */
#define HHI_FIX_PLL_CNTL2		0x2a8 /* 0xaa offset in datasheet */
#define HHI_FIX_PLL_CNTL3		0x2ac /* 0xab offset in datasheet */
#define HHI_FIX_PLL_CNTL4		0x2b0 /* 0xac offset in datasheet */
#define HHI_FIX_PLL_CNTL5		0x2b4 /* 0xad offset in datasheet */
#define HHI_FIX_PLL_CNTL6		0x2b8 /* 0xae offset in datasheet */
#define HHI_FIX_PLL_STS			0x2bc /* 0xaf offset in datasheet very*/

#define HHI_MPLL3_CNTL0			0x2E0 /* 0xb8 offset in datasheet */
#define HHI_MPLL3_CNTL1			0x2E4 /* 0xb9 offset in datasheet */
#define HHI_PLL_TOP_MISC		0x2E8 /* 0xba offset in datasheet */

#define HHI_ADC_PLL_CNTL		0x2A8 /* 0xaa offset in datasheet */
#define HHI_ADC_PLL_CNTL2		0x2AC /* 0xab offset in datasheet */
#define HHI_ADC_PLL_CNTL3		0x2B0 /* 0xac offset in datasheet */
#define HHI_ADC_PLL_CNTL4		0x2B4 /* 0xad offset in datasheet */

#define HHI_SYS_PLL_CNTL0		0x2f4 /* 0xbd offset in datasheet */
#define HHI_SYS_PLL_CNTL1		0x2f8 /* 0xbe offset in datasheet */
#define HHI_SYS_PLL_CNTL2		0x2fc /* 0xbf offset in datasheet */
#define HHI_SYS_PLL_CNTL3		0x300 /* 0xc0 offset in datasheet */
#define HHI_SYS_PLL_CNTL4		0x304 /* 0xc1 offset in datasheet */
#define HHI_SYS_PLL_CNTL5		0x308 /* 0xc2 offset in datasheet */
#define HHI_SYS_PLL_CNTL6		0x30c /* 0xc3 offset in datasheet */

#define HHI_HDMI_PLL_CNTL		0x320 /* 0xc8 offset in datasheet */
#define HHI_HDMI_PLL_CNTL2		0x324 /* 0xc9 offset in datasheet */
#define HHI_HDMI_PLL_CNTL3		0x328 /* 0xca offset in datasheet */
#define HHI_HDMI_PLL_CNTL4		0x32C /* 0xcb offset in datasheet */
#define HHI_HDMI_PLL_CNTL5		0x330 /* 0xcc offset in datasheet */
#define HHI_VID_LOCK_CLK_CNTL		0x3c8 /* 0xf2 offset in datasheet1 */
#define HHI_BT656_CLK_CNTL		0x3d4 /* 0xf5 offset in datasheet1 */
#define HHI_SPICC_CLK_CNTL		0x3dc /* 0xf7 offset in datasheet1 */

/* AO registers*/
#define AO_RTI_PWR_CNTL_REG0		0x10 /* 0x4 offset in datasheet1*/
#define AO_CLK_GATE0			0x4c /* 0x13 offset in datasheet1 */
#define AO_CLK_GATE0_SP			0x50 /* 0x14 offset in datasheet1 */
#define AO_SAR_CLK			0x90 /* 0x24 offset in datasheet1 */

/*tl1: pll: FDCO: 3G~6G FDCO = 24*(M+frac)/N
 *N: recommend is 1
 *clk_out = FDCO >> OD
 */
static const struct pll_rate_table tl1_pll_rate_table[] = {
	PLL_RATE(24000000ULL,	128, 1, 7), /*DCO=3072M*/
	PLL_RATE(48000000ULL,	128, 1, 6), /*DCO=3072M*/
	PLL_RATE(96000000ULL,	128, 1, 5), /*DCO=3072M*/
	PLL_RATE(192000000ULL,  128, 1, 4), /*DCO=3072M*/
	PLL_RATE(312000000ULL,  208, 1, 4), /*DCO=4992M*/
	PLL_RATE(408000000ULL,  136, 1, 3), /*DCO=3264M*/
	PLL_RATE(600000000ULL,  200, 1, 3), /*DCO=4800M*/
	PLL_RATE(696000000ULL,  232, 1, 3), /*DCO=5568M*/
	PLL_RATE(792000000ULL,  132, 1, 2), /*DCO=3168M*/
	PLL_RATE(846000000ULL,  141, 1, 2), /*DCO=3384M*/
	PLL_RATE(912000000ULL,  152, 1, 2), /*DCO=3648M*/
	PLL_RATE(1008000000ULL, 168, 1, 2), /*DCO=4032M*/
	PLL_RATE(1104000000ULL, 184, 1, 2), /*DCO=4416M*/
	PLL_RATE(1200000000ULL, 200, 1, 2), /*DCO=4800M*/
	PLL_RATE(1296000000ULL, 216, 1, 2), /*DCO=5184M*/
	PLL_RATE(1302000000ULL, 217, 1, 2), /*DCO=5208M*/
	PLL_RATE(1398000000ULL, 233, 1, 2), /*DCO=5592M*/
	PLL_RATE(1494000000ULL, 249, 1, 2), /*DCO=5976M*/
	PLL_RATE(1512000000ULL, 126, 1, 1), /*DCO=3024M*/
	PLL_RATE(1608000000ULL, 134, 1, 1), /*DCO=3216M*/
	PLL_RATE(1704000000ULL, 142, 1, 1), /*DCO=3408M*/
	PLL_RATE(1800000000ULL, 150, 1, 1), /*DCO=3600M*/
	PLL_RATE(1896000000ULL, 158, 1, 1), /*DCO=3792M*/
	PLL_RATE(1908000000ULL, 159, 1, 1), /*DCO=3816M*/
	PLL_RATE(1920000000ULL, 160, 1, 1), /*DCO=3840M*/
	PLL_RATE(2004000000ULL, 167, 1, 1), /*DCO=4008M*/
	PLL_RATE(2016000000ULL, 168, 1, 1), /*DCO=4032M*/
	PLL_RATE(2100000000ULL, 175, 1, 1), /*DCO=4200M*/
	PLL_RATE(2196000000ULL, 183, 1, 1), /*DCO=4392M*/
	PLL_RATE(2208000000ULL, 184, 1, 1), /*DCO=4416M*/
	PLL_RATE(2292000000ULL, 191, 1, 1), /*DCO=4584M*/
	PLL_RATE(2304000000ULL, 192, 1, 1), /*DCO=4608M*/
	PLL_RATE(2400000000ULL, 200, 1, 1), /*DCO=4800M*/
	PLL_RATE(2496000000ULL, 208, 1, 1), /*DCO=4992M*/
	PLL_RATE(2592000000ULL, 216, 1, 1), /*DCO=5184M*/
	PLL_RATE(2700000000ULL, 225, 1, 1), /*DCO=5400M*/
	PLL_RATE(2796000000ULL, 233, 1, 1), /*DCO=5592M*/
	PLL_RATE(2892000000ULL, 241, 1, 1), /*DCO=5784M*/
	PLL_RATE(3000000000ULL, 125, 1, 0), /*DCO=3000M*/
	PLL_RATE(3096000000ULL, 129, 1, 0), /*DCO=3096M*/
	PLL_RATE(3192000000ULL, 133, 1, 0), /*DCO=3192M*/
	PLL_RATE(3288000000ULL, 137, 1, 0), /*DCO=3288M*/
	PLL_RATE(3408000000ULL, 142, 1, 0), /*DCO=3408M*/
	PLL_RATE(3504000000ULL, 146, 1, 0), /*DCO=3504M*/
	PLL_RATE(3600000000ULL, 150, 1, 0), /*DCO=3600M*/
	PLL_RATE(3696000000ULL, 154, 1, 0), /*DCO=3696M*/
	PLL_RATE(3792000000ULL, 158, 1, 0), /*DCO=3792M*/
	PLL_RATE(3888000000ULL, 162, 1, 0), /*DCO=3888M*/
	PLL_RATE(4008000000ULL, 167, 1, 0), /*DCO=4008M*/
	PLL_RATE(4104000000ULL, 171, 1, 0), /*DCO=4104M*/
	PLL_RATE(4200000000ULL, 175, 1, 0), /*DCO=4200M*/
	PLL_RATE(4296000000ULL, 179, 1, 0), /*DCO=4296M*/
	PLL_RATE(4392000000ULL, 183, 1, 0), /*DCO=4392M*/
	PLL_RATE(4488000000ULL, 187, 1, 0), /*DCO=4488M*/
	PLL_RATE(4608000000ULL, 192, 1, 0), /*DCO=4608M*/
	PLL_RATE(4704000000ULL, 196, 1, 0), /*DCO=4704M*/
	PLL_RATE(4800000000ULL, 200, 1, 0), /*DCO=4800M*/
	PLL_RATE(4896000000ULL, 204, 1, 0), /*DCO=4896M*/
	PLL_RATE(4992000000ULL, 208, 1, 0), /*DCO=4992M*/
	PLL_RATE(5088000000ULL, 212, 1, 0), /*DCO=5088M*/
	PLL_RATE(5208000000ULL, 217, 1, 0), /*DCO=5208M*/
	PLL_RATE(5304000000ULL, 221, 1, 0), /*DCO=5304M*/
	PLL_RATE(5400000000ULL, 225, 1, 0), /*DCO=5400M*/
	PLL_RATE(5496000000ULL, 229, 1, 0), /*DCO=5496M*/
	PLL_RATE(5592000000ULL, 233, 1, 0), /*DCO=5592M*/
	PLL_RATE(5688000000ULL, 237, 1, 0), /*DCO=5688M*/
	PLL_RATE(5808000000ULL, 242, 1, 0), /*DCO=5808M*/
	PLL_RATE(5904000000ULL, 246, 1, 0), /*DCO=5904M*/
	PLL_RATE(6000000000ULL, 250, 1, 0), /*DCO=6000M*/

	{ /* sentinel */ },

};

/*fix pll rate table*/
static const struct fclk_rate_table fclk_pll_rate_table[] = {
	FCLK_PLL_RATE(50000000, 1, 1, 19),
	FCLK_PLL_RATE(100000000, 1, 1, 9),
	FCLK_PLL_RATE(167000000, 2, 1, 3),
	FCLK_PLL_RATE(200000000, 1, 1, 4),
	FCLK_PLL_RATE(250000000, 1, 1, 3),
	FCLK_PLL_RATE(333000000, 2, 1, 1),
	FCLK_PLL_RATE(500000000, 1, 1, 1),
	FCLK_PLL_RATE(667000000, 2, 0, 0),
	FCLK_PLL_RATE(1000000000, 1, 0, 0),
};

#endif /* __TL1_H */
