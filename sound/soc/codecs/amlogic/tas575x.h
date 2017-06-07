/*
 * sound/soc/codecs/amlogic/tas575x.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * Author: Shuai Li <shuai.li@amlogic.com>
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

#ifndef __TAS575X_H__
#define __TAS575X_H__

/* Register Address Map */
#define TAS575X_RESET			1
#define TAS575X_STANDBY			2
#define TAS575X_MUTE			3
#define TAS575X_PLL				4
#define TAS575X_DOUT			7
#define TAS575X_GPIO			8
#define TAS575X_CLK				9
#define TAS575X_MASTR_CLK		12
#define TAS575X_PLL_REF			13
#define TAS575X_CLK_SRC			14
#define TAS575X_GPIO_SRC		18
#define TAS575X_SYNC_REQ		19
#define TAS575X_PLL_DIV_P		20
#define TAS575X_PLL_DIV_J		21
#define TAS575X_PLL_DIV_D		22
#define TAS575X_PLL_DIV_R		24
#define TAS575X_DSP_DIV			27
#define TAS575X_DAC_DIV			28
#define TAS575X_NCP_DIV			29
#define TAS575X_OSR_DIV			30
#define TAS575X_SCLK_DIV		32
#define TAS575X_LRCLK_DIV		33
#define TAS575X_16X_INTERP		34
#define TAS575X_DSP_CLK_CYCLM	35
#define TAS575X_DSP_CLK_CYCLL	36
#define TAS575X_IGNORES			37
#define TAS575X_I2S_FMT			40
#define TAS575X_I2S_SHIFT		41
#define TAS575X_DATA_PATH		42
#define TAS575X_DSP_PROG_SEL	43
#define TAS575X_CLK_DET_PRD		44
#define TAS575X_MUTE_TIME		59
#define TAS575X_DIG_VOL_CTRL	60
#define TAS575X_CH_B_DIG_VOL	61
#define TAS575X_CH_A_DIG_VOL	62
#define TAS575X_VOL_NORM_RAMP	63
#define TAS575X_VOL_EMRG_RAMP	64
#define TAS575X_AUTO_MUTE_CTRL	65
#define TAS575X_GPIO1_SEL		82
#define TAS575X_GPIO0_SEL		83
#define TAS575X_GPIO2_SEL		85
#define TAS575X_GPIO_CTRL		86
#define TAS575X_GPIO_INV		87
#define TAS575X_CHANL_OVFLOW	90
#define TAS575X_MCLK_DET		91
#define TAS575X_SCLK_DET_MSB	92
#define TAS575X_SCLK_DET_LSB	93
#define TAS575X_CLK_DET_STAT	94
#define TAS575X_CLK_STAT		95
#define TAS575X_ANLG_MUTE_STAT	108
#define TAS575X_SHORT_STAT		109
#define TAS575X_SPK_MUTE_STAT	114
#define TAS575X_FS_SPD_MODE		115
#define TAS575X_PWR_STAT		117
#define TAS575X_GPIO_STAT		119
#define TAS575X_AUTO_MUTE		120
#define TAS575X_DAC_MODE		121

#endif /* __TAS575X_H__ */
