/*
 * drivers/amlogic/clk/g12a/g12a.h
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

#ifndef __G12A_H
#define __G12A_H

/*
 * Clock controller register offsets
 *
 * Register offsets from the data sheet are listed in comment blocks below.
 * Those offsets must be multiplied by 4 before adding them to the base address
 * to get the right value
 */

#define HHI_MIPI_CNTL0			0x0 /* 0x0 offset in data sheet */
#define HHI_MIPI_CNTL1			0x4 /* 0x1 offset in data sheet */
#define HHI_MIPI_CNTL2			0x8 /* 0x2 offset in data sheet */

#define HHI_GP0_PLL_CNTL0		0x40 /* 0x10 offset in data sheet */


#define HHI_PCIE_PLL_CNTL0		0x98 /* 0x26 offset in data sheet */
#define HHI_PCIE_PLL_CNTL1		0x9c /* 0x27 offset in data sheet */
//#define HHI_PCIE_PLL_CNTL6	0xf0 /* 0x3c offset in data sheet */

#define HHI_HIFI_PLL_CNTL0		0xD8 /* 0x36 offset in data sheet */

#define HHI_GCLK_MPEG0			0x140 /* 0x50 offset in data sheet */
#define HHI_GCLK_MPEG1			0x144 /* 0x51 offset in data sheet */
#define HHI_GCLK_MPEG2			0x148 /* 0x52 offset in data sheet */
#define HHI_GCLK_OTHER			0x150 /* 0x54 offset in data sheet */

#define HHI_APICALGDC_CNTL		0x168 /* 0x5a offset in data sheet */

#define HHI_MPEG_CLK_CNTL       0x174 /* 0x5d offset in data sheet */
#define HHI_AUD_CLK_CNTL        0x178 /* 0x5e offset in data sheet */
#define HHI_VID_CLK_CNTL        0x17c /* 0x5f offset in data sheet */
#define HHI_TS_CLK_CNTL         0x190 /* 0x64 offset in data sheet */
#define HHI_VID_CLK_CNTL2       0x194 /* 0x65 offset in data sheet */
#define HHI_SYS_CPU_CLK_CNTL0   0x19c /* 0x67 offset in data sheet */

#define HHI_MALI_CLK_CNTL		0x1b0 /* 0x6c offset in data sheet */
#define HHI_VPU_CLKC_CNTL		0x1b4 /* 0x6d offset in data sheet */
#define HHI_VPU_CLK_CNTL		0x1bC /* 0x6f offset in data sheet */

#define HHI_MIPI_ISP_CLK_CNTL   0x1c0 /* 0x70 offset in data sheet */

#define HHI_VIPNANOQ_CLK_CNTL   0x1c8 /* 0x72 offset in data sheet */
#define HHI_HDMI_CLK_CNTL       0x1CC /* 0x73 offset in data sheet */

#define HHI_VDEC_CLK_CNTL		0x1E0 /* 0x78 offset in data sheet */
#define HHI_VDEC2_CLK_CNTL		0x1E4 /* 0x79 offset in data sheet */
#define HHI_VDEC3_CLK_CNTL		0x1E8 /* 0x7a offset in data sheet */
#define HHI_VDEC4_CLK_CNTL		0x1EC /* 0x7b offset in data sheet */
#define HHI_HDCP22_CLK_CNTL		0x1F0 /* 0x7c offset in data sheet */
#define HHI_VAPBCLK_CNTL		0x1F4 /* 0x7d offset in data sheet */

#define HHI_SYS_CPUB_CLK_CNTL	0x208 /* 0x82 offset in data sheet */
#define HHI_VPU_CLKB_CNTL		0x20C /* 0x83 offset in data sheet */
#define HHI_GEN_CLK_CNTL		0x228 /* 0x8a offset in data sheet */


#define HHI_VDIN_MEAS_CLK_CNTL		0x250 /* 0x94 offset in data sheet */
#define HHI_MIPIDSI_PHY_CLK_CNTL	0x254 /* 0x95 offset in data sheet */
#define HHI_NAND_CLK_CNTL		0x25C /* 0x97 offset in data sheet */
#define HHI_SD_EMMC_CLK_CNTL		0x264 /* 0x99 offset in data sheet */

#define HHI_MPLL_CNTL0			0x278 /* 0x9e offset in data sheet */
#define HHI_MPLL_CNTL1			0x27C /* 0x9f offset in data sheet */
#define HHI_MPLL_CNTL2			0x280 /* 0xa0 offset in data sheet */
#define HHI_MPLL_CNTL3			0x284 /* 0xa1 offset in data sheet */
#define HHI_MPLL_CNTL4			0x288 /* 0xa2 offset in data sheet */
#define HHI_MPLL_CNTL5			0x28c /* 0xa3 offset in data sheet */
#define HHI_MPLL_CNTL6			0x290 /* 0xa4 offset in data sheet */
#define HHI_MPLL_CNTL7			0x294 /* 0xa5 offset in data sheet */
#define HHI_MPLL_CNTL8			0x298 /* 0xa6 offset in data sheet */

#define HHI_FIX_PLL_CNTL0		0x2A0 /* 0xa8 offset in data sheet */
#define HHI_FIX_PLL_CNTL1		0x2A4 /* 0xa9 offset in data sheet */

#if 0
#define HHI_MPLL3_CNTL0			0x2E0 /* 0xb8 offset in data sheet */
#define HHI_MPLL3_CNTL1			0x2E4 /* 0xb9 offset in data sheet */
#define HHI_PLL_TOP_MISC		0x2E8 /* 0xba offset in data sheet */
#endif
#define HHI_SYS_PLL_CNTL0		0x2f4 /* 0xbd offset in data sheet */
#define HHI_SYS_PLL_CNTL1		0x2f8 /* 0xbe offset in data sheet */
#define HHI_SYS_PLL_CNTL2		0x2fc /* 0xbf offset in data sheet */
#define HHI_SYS_PLL_CNTL3		0x300 /* 0xc0 offset in data sheet */
#define HHI_SYS_PLL_CNTL4		0x304 /* 0xc1 offset in data sheet */
#define HHI_SYS_PLL_CNTL5		0x308 /* 0xc2 offset in data sheet */
#define HHI_SYS_PLL_CNTL6		0x30c /* 0xc3 offset in data sheet */

/* For G12B only */
#define HHI_MIPI_CSI_PHY_CLK_CNTL 0x340 /* 0xd0 offset in data sheet */
#define HHI_SYS1_PLL_CNTL0        0x380 /* 0xe0 offset in data sheet */
#define HHI_SYS1_PLL_CNTL1        0x384 /* 0xe1 offset in data sheet */
#define HHI_SYS1_PLL_CNTL2        0x388 /* 0xe2 offset in data sheet */
#define HHI_SYS1_PLL_CNTL3        0x38c /* 0xe3 offset in data sheet */
#define HHI_SYS1_PLL_CNTL4        0x390 /* 0xe4 offset in data sheet */
#define HHI_SYS1_PLL_CNTL5        0x394 /* 0xe5 offset in data sheet */
#define HHI_SYS1_PLL_CNTL6        0x398 /* 0xe6 offset in data sheet */
/*****************/

#define HHI_SPICC_CLK_CNTL      0x3dc /* 0xf7 offset in data sheet */
/* AO registers*/
#define AO_RTI_PWR_CNTL_REG0 0x10 /* 0x4 offset in data sheet */
#define AO_SAR_CLK		0x90 /* 0x24 offset in data sheet */

/*G12A: pll: FDCO: 3G~6G FDCO = 24*(M+frac)/N
 *N: recommend is 1
 *clk_out = FDCO >> OD
 */
static const struct pll_rate_table g12a_pll_rate_table[] = {
	PLL_RATE(24000000,  128, 1, 7), /*DCO=3072M*/
	PLL_RATE(48000000,  128, 1, 6), /*DCO=3072M*/
	PLL_RATE(96000000,  128, 1, 5), /*DCO=3072M*/
	PLL_RATE(192000000,  128, 1, 4), /*DCO=3072M*/
	PLL_RATE(312000000,  208, 1, 4), /*DCO=4992M*/
	PLL_RATE(408000000,  136, 1, 3), /*DCO=3264M*/
	PLL_RATE(600000000,  200, 1, 3), /*DCO=4800M*/
	PLL_RATE(696000000,  232, 1, 3), /*DCO=5568M*/
	PLL_RATE(792000000,  132, 1, 2), /*DCO=3168M*/
	PLL_RATE(846000000,  141, 1, 2), /*DCO=3384M*/
	PLL_RATE(912000000,  152, 1, 2), /*DCO=3648M*/
	PLL_RATE(1008000000, 168, 1, 2), /*DCO=4032M*/
	PLL_RATE(1104000000, 184, 1, 2), /*DCO=4416M*/
	PLL_RATE(1200000000, 200, 1, 2), /*DCO=4800M*/
	PLL_RATE(1296000000, 216, 1, 2), /*DCO=5184M*/
	PLL_RATE(1398000000, 233, 1, 2), /*DCO=5592M*/
	PLL_RATE(1494000000, 249, 1, 2), /*DCO=5976M*/
	PLL_RATE(1512000000, 126, 1, 1), /*DCO=3024M*/
	PLL_RATE(1608000000, 134, 1, 1), /*DCO=3216M*/
	PLL_RATE(1704000000, 142, 1, 1), /*DCO=3408M*/
	PLL_RATE(1800000000, 150, 1, 1), /*DCO=3600M*/
	PLL_RATE(1896000000, 158, 1, 1), /*DCO=3792M*/
	PLL_RATE(1908000000, 159, 1, 1), /*DCO=3816M*/
	PLL_RATE(1920000000, 160, 1, 1), /*DCO=3840M*/
	PLL_RATE(2016000000, 168, 1, 1), /*DCO=4032M*/
	PLL_RATE(2100000000, 175, 1, 1), /*DCO=4200M*/
	PLL_RATE(2196000000, 183, 1, 1), /*DCO=4392M*/
	PLL_RATE(2292000000, 191, 1, 1), /*DCO=4584M*/
	PLL_RATE(2400000000, 200, 1, 1), /*DCO=4800M*/
	PLL_RATE(2496000000, 208, 1, 1), /*DCO=4992M*/
	PLL_RATE(2592000000, 216, 1, 1), /*DCO=5184M*/
	PLL_RATE(2700000000, 225, 1, 1), /*DCO=5400M*/
	PLL_RATE(2796000000, 233, 1, 1), /*DCO=5592M*/
	PLL_RATE(2892000000, 241, 1, 1), /*DCO=5784M*/
	PLL_RATE(3000000000, 125, 1, 0), /*DCO=3000M*/
	PLL_RATE(3096000000, 129, 1, 0), /*DCO=3096M*/
	PLL_RATE(3192000000, 133, 1, 0), /*DCO=3192M*/
	PLL_RATE(3288000000, 137, 1, 0), /*DCO=3288M*/
	PLL_RATE(3408000000, 142, 1, 0), /*DCO=3408M*/
	PLL_RATE(3504000000, 146, 1, 0), /*DCO=3504M*/
	PLL_RATE(3600000000, 150, 1, 0), /*DCO=3600M*/
	PLL_RATE(3696000000, 154, 1, 0), /*DCO=3696M*/
	PLL_RATE(3792000000, 158, 1, 0), /*DCO=3792M*/
	PLL_RATE(3888000000, 162, 1, 0), /*DCO=3888M*/
	PLL_RATE(4008000000, 167, 1, 0), /*DCO=4008M*/
	PLL_RATE(4104000000, 171, 1, 0), /*DCO=4104M*/
	PLL_RATE(4200000000, 175, 1, 0), /*DCO=4200M*/
	PLL_RATE(4296000000, 179, 1, 0), /*DCO=4296M*/
	PLL_RATE(4392000000, 183, 1, 0), /*DCO=4392M*/
	PLL_RATE(4488000000, 187, 1, 0), /*DCO=4488M*/
	PLL_RATE(4608000000, 192, 1, 0), /*DCO=4608M*/
	PLL_RATE(4704000000, 196, 1, 0), /*DCO=4704M*/
	PLL_RATE(4800000000, 200, 1, 0), /*DCO=4800M*/
	PLL_RATE(4896000000, 204, 1, 0), /*DCO=4896M*/
	PLL_RATE(4992000000, 208, 1, 0), /*DCO=4992M*/
	PLL_RATE(5088000000, 212, 1, 0), /*DCO=5088M*/
	PLL_RATE(5208000000, 217, 1, 0), /*DCO=5208M*/
	PLL_RATE(5304000000, 221, 1, 0), /*DCO=5304M*/
	PLL_RATE(5400000000, 225, 1, 0), /*DCO=5400M*/
	PLL_RATE(5496000000, 229, 1, 0), /*DCO=5496M*/
	PLL_RATE(5592000000, 233, 1, 0), /*DCO=5592M*/
	PLL_RATE(5688000000, 237, 1, 0), /*DCO=5688M*/
	PLL_RATE(5808000000, 242, 1, 0), /*DCO=5808M*/
	PLL_RATE(5904000000, 246, 1, 0), /*DCO=5904M*/
	PLL_RATE(6000000000, 250, 1, 0), /*DCO=6000M*/

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

/*PCIE clk_out = 24M*m/2/2/OD*/
static const struct pll_rate_table g12a_pcie_pll_rate_table[] = {
	PLL_RATE(100000000, 150, 0, 9),
	{ /* sentinel */ },
};
#endif /* __G12A_H */
