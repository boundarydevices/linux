/*
 * drivers/amlogic/clk/txlx/txlx.h
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

#ifndef __TXLX_H
#define __TXLX_H

/*
 * Clock controller register offsets
 *
 * Register offsets from the data sheet are listed in comment blocks below.
 * Those offsets must be multiplied by 4 before adding them to the base address
 * to get the right value
 */
/*ok*/
#define HHI_GP0_PLL_CNTL		0x40 /* 0x10 offset in data sheet */
/*new*/
#define HHI_GP1_PLL_CNTL		0x60 /* 0x18 offset in data sheet */
/*not define in spec*/
#define HHI_HIFI_PLL_CNTL		0x80 /* 0x20 offset in data sheet */

#define HHI_PCIE_PLL_CNTL		0xd8 /* 0x36 offset in data sheet */
#define HHI_PCIE_PLL_CNTL1		0xdc /* 0x37 offset in data sheet */
#define HHI_PCIE_PLL_CNTL6	0xf0 /* 0x3c offset in data sheet */

#define HHI_GCLK_MPEG0			0x140 /* 0x50 offset in data sheet */
#define HHI_GCLK_MPEG1			0x144 /* 0x51 offset in data sheet */
#define HHI_GCLK_MPEG2			0x148 /* 0x52 offset in data sheet */
#define HHI_GCLK_OTHER			0x150 /* 0x54 offset in data sheet */

#define HHI_GCLK_AO			0x154 /* 0x55 offset in data sheet */

#define HHI_VID_CLK_DIV			0x164 /* 0x59 offset in data sheet */
#define HHI_SPICC_HCLK_CNTL 0x168 /* 0x5a offset in data sheet */

#define HHI_MPEG_CLK_CNTL		0x174 /* 0x5d offset in data sheet */
#define HHI_AUD_CLK_CNTL		0x178 /* 0x5e offset in data sheet */
#define HHI_VID_CLK_CNTL		0x17c /* 0x5f offset in data sheet */
#define HHI_AUD_CLK_CNTL2		0x190 /* 0x64 offset in data sheet */
#define HHI_VID_CLK_CNTL2		0x194 /* 0x65 offset in data sheet */
#define HHI_SYS_CPU_CLK_CNTL0		0x19c /* 0x67 offset in data sheet */
#define HHI_AUD_CLK_CNTL3		0x1A4 /* 0x69 offset in data sheet */
#define HHI_AUD_CLK_CNTL4		0x1A8 /* 0x6a offset in data sheet */
#define HHI_MALI_CLK_CNTL		0x1b0 /* 0x6c offset in data sheet */
#define HHI_VPU_CLK_CNTL		0x1bC /* 0x6f offset in data sheet */

#define HHI_VDEC_CLK_CNTL		0x1E0 /* 0x78 offset in data sheet */
#define HHI_VDEC2_CLK_CNTL		0x1E4 /* 0x79 offset in data sheet */
#define HHI_VDEC3_CLK_CNTL		0x1E8 /* 0x7a offset in data sheet */
#define HHI_VDEC4_CLK_CNTL		0x1EC /* 0x7b offset in data sheet */
#define HHI_HDCP22_CLK_CNTL		0x1F0 /* 0x7c offset in data sheet */
#define HHI_VAPBCLK_CNTL		0x1F4 /* 0x7d offset in data sheet */
#define HHI_HDMIRX_CLK_CNTL		0x200 /* 0x80 offset in data sheet */
#define HHI_HDMIRX_AUD_CLK_CNTL	0x204 /* 0x81 offset in data sheet */
#define HHI_VPU_CLKB_CNTL		0x20C /* 0x83 offset in data sheet */
#define HHI_ALOCKER_CLK_CNTL	0x234 /* 0x8d offset in data sheet */

#define HHI_VDIN_MEAS_CLK_CNTL		0x250 /* 0x94 offset in data sheet */
#define HHI_PCM_CLK_CNTL		0x258 /* 0x96 offset in data sheet */
#define HHI_NAND_CLK_CNTL		0x25C /* 0x97 offset in data sheet */
#define HHI_SD_EMMC_CLK_CNTL		0x264 /* 0x99 offset in data sheet */

#define HHI_MPLL_CNTL			0x280 /* 0xa0 offset in data sheet */
#define HHI_MPLL_CNTL2			0x284 /* 0xa1 offset in data sheet */
#define HHI_MPLL_CNTL3			0x288 /* 0xa2 offset in data sheet */
#define HHI_MPLL_CNTL4			0x28C /* 0xa3 offset in data sheet */
#define HHI_MPLL_CNTL5			0x290 /* 0xa4 offset in data sheet */
#define HHI_MPLL_CNTL6			0x294 /* 0xa5 offset in data sheet */
#define HHI_MPLL_CNTL7			0x298 /* MP0, 0xa6 offset */
#define HHI_MPLL_CNTL8			0x29C /* MP1, 0xa7 offset */
#define HHI_MPLL_CNTL9			0x2A0 /* MP2, 0xa8 offset */
#define HHI_MPLL_CNTL10			0x2A4 /* MP2, 0xa9 offset */

#define HHI_MPLL3_CNTL0			0x2E0 /* 0xb8 offset in data sheet */
#define HHI_MPLL3_CNTL1			0x2E4 /* 0xb9 offset in data sheet */
#define HHI_PLL_TOP_MISC		0x2E8 /* 0xba offset in data sheet */

#define HHI_ADC_PLL_CNTL		0x2A8 /* 0xaa offset in data sheet */
#define HHI_ADC_PLL_CNTL2		0x2AC /* 0xab offset in data sheet */
#define HHI_ADC_PLL_CNTL3		0x2B0 /* 0xac offset in data sheet */
#define HHI_ADC_PLL_CNTL4		0x2B4 /* 0xad offset in data sheet */

#define HHI_SYS_PLL_CNTL		0x300 /* 0xc0 offset in data sheet */
#define HHI_SYS_PLL_CNTL2		0x304 /* 0xc1 offset in data sheet */
#define HHI_SYS_PLL_CNTL3		0x308 /* 0xc2 offset in data sheet */
#define HHI_SYS_PLL_CNTL4		0x30c /* 0xc3 offset in data sheet */
#define HHI_SYS_PLL_CNTL5		0x310 /* 0xc4 offset in data sheet */

#define HHI_HDMI_PLL_CNTL		0x320 /* 0xc8 offset in data sheet */
#define HHI_HDMI_PLL_CNTL2		0x324 /* 0xc9 offset in data sheet */
#define HHI_HDMI_PLL_CNTL3		0x328 /* 0xca offset in data sheet */
#define HHI_HDMI_PLL_CNTL4		0x32C /* 0xcb offset in data sheet */
#define HHI_HDMI_PLL_CNTL5		0x330 /* 0xcc offset in data sheet */

/* AO registers*/
#define AO_RTI_PWR_CNTL_REG0 0x10 /* 0x4 offset in data sheet */
#define AO_SAR_CLK		0x90 /* 0x24 offset in data sheet */

static const struct pll_rate_table sys_pll_rate_table[] = {
	PLL_RATE(24000000, 56, 1, 2),
	PLL_RATE(48000000, 64, 1, 2),
	PLL_RATE(72000000, 72, 1, 2),
	PLL_RATE(96000000, 64, 1, 2),
	PLL_RATE(120000000, 80, 1, 2),
	PLL_RATE(144000000, 96, 1, 2),
	PLL_RATE(168000000, 56, 1, 1),
	PLL_RATE(192000000, 64, 1, 1),
	PLL_RATE(216000000, 72, 1, 1),
	PLL_RATE(240000000, 80, 1, 1),
	PLL_RATE(264000000, 88, 1, 1),
	PLL_RATE(288000000, 96, 1, 1),
	PLL_RATE(312000000, 52, 1, 2),
	PLL_RATE(336000000, 56, 1, 2),
	PLL_RATE(360000000, 60, 1, 2),
	PLL_RATE(384000000, 64, 1, 2),
	PLL_RATE(408000000, 68, 1, 2),
	PLL_RATE(432000000, 72, 1, 2),
	PLL_RATE(456000000, 76, 1, 2),
	PLL_RATE(480000000, 80, 1, 2),
	PLL_RATE(504000000, 84, 1, 2),
	PLL_RATE(528000000, 88, 1, 2),
	PLL_RATE(552000000, 92, 1, 2),
	PLL_RATE(576000000, 96, 1, 2),
	PLL_RATE(600000000, 50, 1, 1),
	PLL_RATE(624000000, 52, 1, 1),
	PLL_RATE(648000000, 54, 1, 1),
	PLL_RATE(672000000, 56, 1, 1),
	PLL_RATE(696000000, 58, 1, 1),
	PLL_RATE(720000000, 60, 1, 1),
	PLL_RATE(744000000, 62, 1, 1),
	PLL_RATE(768000000, 64, 1, 1),
	PLL_RATE(792000000, 66, 1, 1),
	PLL_RATE(816000000, 68, 1, 1),
	PLL_RATE(840000000, 70, 1, 1),
	PLL_RATE(864000000, 72, 1, 1),
	PLL_RATE(888000000, 74, 1, 1),
	PLL_RATE(912000000, 76, 1, 1),
	PLL_RATE(936000000, 78, 1, 1),
	PLL_RATE(960000000, 80, 1, 1),
	PLL_RATE(984000000, 82, 1, 1),
	PLL_RATE(1008000000, 84, 1, 1),
	PLL_RATE(1032000000, 86, 1, 1),
	PLL_RATE(1056000000, 88, 1, 1),
	PLL_RATE(1080000000, 90, 1, 1),
	PLL_RATE(1104000000, 92, 1, 1),
	PLL_RATE(1128000000, 94, 1, 1),
	PLL_RATE(1152000000, 96, 1, 1),
	PLL_RATE(1176000000, 98, 1, 1),
	PLL_RATE(1200000000, 50, 1, 0),
	PLL_RATE(1224000000, 51, 1, 0),
	PLL_RATE(1248000000, 52, 1, 0),
	PLL_RATE(1272000000, 53, 1, 0),
	PLL_RATE(1296000000, 54, 1, 0),
	PLL_RATE(1320000000, 55, 1, 0),
	PLL_RATE(1344000000, 56, 1, 0),
	PLL_RATE(1368000000, 57, 1, 0),
	PLL_RATE(1392000000, 58, 1, 0),
	PLL_RATE(1416000000, 59, 1, 0),
	PLL_RATE(1440000000, 60, 1, 0),
	PLL_RATE(1464000000, 61, 1, 0),
	PLL_RATE(1488000000, 62, 1, 0),
	PLL_RATE(1512000000, 63, 1, 0),
	PLL_RATE(1536000000, 64, 1, 0),
	PLL_RATE(1560000000, 65, 1, 0),
	PLL_RATE(1584000000, 66, 1, 0),
	PLL_RATE(1608000000, 67, 1, 0),
	PLL_RATE(1632000000, 68, 1, 0),
	PLL_RATE(1656000000, 68, 1, 0),
	PLL_RATE(1680000000, 68, 1, 0),
	PLL_RATE(1704000000, 68, 1, 0),
	PLL_RATE(1728000000, 69, 1, 0),
	PLL_RATE(1752000000, 69, 1, 0),
	PLL_RATE(1776000000, 69, 1, 0),
	PLL_RATE(1800000000, 69, 1, 0),
	PLL_RATE(1824000000, 70, 1, 0),
	PLL_RATE(1848000000, 70, 1, 0),
	PLL_RATE(1872000000, 70, 1, 0),
	PLL_RATE(1896000000, 70, 1, 0),
	PLL_RATE(1920000000, 71, 1, 0),
	PLL_RATE(1944000000, 71, 1, 0),
	PLL_RATE(1968000000, 71, 1, 0),
	PLL_RATE(1992000000, 71, 1, 0),
	PLL_RATE(2016000000, 72, 1, 0),
	PLL_RATE(2040000000, 72, 1, 0),
	PLL_RATE(2064000000, 72, 1, 0),
	PLL_RATE(2088000000, 72, 1, 0),
	PLL_RATE(2112000000, 73, 1, 0),
	{ /* sentinel */ },
};

/*TXLX: gp0_pll: Fdco: 0.96G~1.632G Fdco = 24*(M+frac)/N
 *N: recommend is 1
 *gp0_clk_out = Fdco /Fod
 *od=0 Fod=1 od=1 Fod=2 od=2 Fod=4 od=3 Fod=4
 */
static const struct pll_rate_table txlx_gp0_pll_rate_table[] = {
	PLL_RATE(240000000, 40, 1, 2),
	PLL_RATE(246000000, 41, 1, 2),
	PLL_RATE(252000000, 42, 1, 2),
	PLL_RATE(258000000, 43, 1, 2),
	PLL_RATE(264000000, 44, 1, 2),
	PLL_RATE(270000000, 45, 1, 2),
	PLL_RATE(276000000, 46, 1, 2),
	PLL_RATE(282000000, 47, 1, 2),
	PLL_RATE(288000000, 48, 1, 2),
	PLL_RATE(294000000, 49, 1, 2),
	PLL_RATE(300000000, 50, 1, 2),
	PLL_RATE(306000000, 51, 1, 2),
	PLL_RATE(312000000, 52, 1, 2),
	PLL_RATE(318000000, 53, 1, 2),
	PLL_RATE(324000000, 54, 1, 2),
	PLL_RATE(330000000, 55, 1, 2),
	PLL_RATE(336000000, 56, 1, 2),
	PLL_RATE(342000000, 57, 1, 2),
	PLL_RATE(348000000, 58, 1, 2),
	PLL_RATE(354000000, 59, 1, 2),
	PLL_RATE(360000000, 60, 1, 2),
	PLL_RATE(366000000, 61, 1, 2),
	PLL_RATE(372000000, 62, 1, 2),
	PLL_RATE(378000000, 63, 1, 2),
	PLL_RATE(384000000, 64, 1, 2),
	PLL_RATE(390000000, 65, 1, 3),
	PLL_RATE(396000000, 66, 1, 3),
	PLL_RATE(402000000, 67, 1, 3),
	PLL_RATE(408000000, 68, 1, 3),
	PLL_RATE(480000000, 40, 1, 1),
	PLL_RATE(492000000, 41, 1, 1),
	PLL_RATE(504000000, 42, 1, 1),
	PLL_RATE(516000000, 43, 1, 1),
	PLL_RATE(528000000, 44, 1, 1),
	PLL_RATE(540000000, 45, 1, 1),
	PLL_RATE(552000000, 46, 1, 1),
	PLL_RATE(564000000, 47, 1, 1),
	PLL_RATE(576000000, 48, 1, 1),
	PLL_RATE(588000000, 49, 1, 1),
	PLL_RATE(600000000, 50, 1, 1),
	PLL_RATE(612000000, 51, 1, 1),
	PLL_RATE(624000000, 52, 1, 1),
	PLL_RATE(636000000, 53, 1, 1),
	PLL_RATE(648000000, 54, 1, 1),
	PLL_RATE(660000000, 55, 1, 1),
	PLL_RATE(672000000, 56, 1, 1),
	PLL_RATE(684000000, 57, 1, 1),
	PLL_RATE(696000000, 58, 1, 1),
	PLL_RATE(708000000, 59, 1, 1),
	PLL_RATE(720000000, 60, 1, 1),
	PLL_RATE(732000000, 61, 1, 1),
	PLL_RATE(744000000, 62, 1, 1),
	PLL_RATE(756000000, 63, 1, 1),
	PLL_RATE(768000000, 64, 1, 1),
	PLL_RATE(780000000, 65, 1, 1),
	PLL_RATE(792000000, 66, 1, 1),
	PLL_RATE(804000000, 67, 1, 1),
	PLL_RATE(816000000, 68, 1, 1),
	PLL_RATE(960000000, 40, 1, 0),
	PLL_RATE(984000000, 41, 1, 0),
	PLL_RATE(1008000000, 42, 1, 0),
	PLL_RATE(1032000000, 43, 1, 0),
	PLL_RATE(1056000000, 44, 1, 0),
	PLL_RATE(1080000000, 45, 1, 0),
	PLL_RATE(1104000000, 46, 1, 0),
	PLL_RATE(1128000000, 47, 1, 0),
	PLL_RATE(1152000000, 48, 1, 0),
	PLL_RATE(1176000000, 49, 1, 0),
	PLL_RATE(1200000000, 50, 1, 0),
	PLL_RATE(1224000000, 51, 1, 0),
	PLL_RATE(1248000000, 52, 1, 0),
	PLL_RATE(1272000000, 53, 1, 0),
	PLL_RATE(1296000000, 54, 1, 0),
	PLL_RATE(1320000000, 55, 1, 0),
	PLL_RATE(1344000000, 56, 1, 0),
	PLL_RATE(1368000000, 57, 1, 0),
	PLL_RATE(1392000000, 58, 1, 0),
	PLL_RATE(1416000000, 59, 1, 0),
	PLL_RATE(1440000000, 60, 1, 0),
	PLL_RATE(1464000000, 61, 1, 0),
	PLL_RATE(1488000000, 62, 1, 0),
	PLL_RATE(1512000000, 63, 1, 0),
	PLL_RATE(1536000000, 64, 1, 0),
	PLL_RATE(1560000000, 65, 1, 0),
	PLL_RATE(1584000000, 66, 1, 0),
	PLL_RATE(1608000000, 67, 1, 0),
	PLL_RATE(1632000000, 68, 1, 0),
	{ /* sentinel */ },
};

#endif /* __TXLX_H */
