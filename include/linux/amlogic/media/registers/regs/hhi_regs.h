/*
 * include/linux/amlogic/media/registers/regs/hhi_regs.h
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

#ifndef HHI_REGS_HEADER_
#define HHI_REGS_HEADER_

#define HHI_GCLK_MPEG0    0x1050
#define HHI_GCLK_MPEG1    0x1051
#define HHI_GCLK_MPEG2    0x1052
#define HHI_GCLK_OTHER    0x1054
#define HHI_GCLK_AO       0x1055


#define HHI_VID_CLK_DIV 0x1059
#define HHI_MPEG_CLK_CNTL 0x105d
#define HHI_AUD_CLK_CNTL 0x105e
#define HHI_VID_CLK_CNTL 0x105f
#define HHI_AUD_CLK_CNTL2 0x1064
/*add from M8m2*/
#define HHI_VID_CLK_CNTL2 0x1065
/**/
#define HHI_VID_DIVIDER_CNTL 0x1066
#define HHI_SYS_CPU_CLK_CNTL 0x1067
#define HHI_MALI_CLK_CNTL 0x106c
#define HHI_MIPI_PHY_CLK_CNTL 0x106e
#define HHI_VPU_CLK_CNTL 0x106f
#define HHI_OTHER_PLL_CNTL 0x1070
#define HHI_OTHER_PLL_CNTL2 0x1071
#define HHI_OTHER_PLL_CNTL3 0x1072
#define HHI_HDMI_CLK_CNTL 0x1073
#define HHI_DEMOD_CLK_CNTL 0x1074
#define HHI_SATA_CLK_CNTL 0x1075
#define HHI_ETH_CLK_CNTL 0x1076
#define HHI_CLK_DOUBLE_CNTL 0x1077
#define HHI_VDEC_CLK_CNTL 0x1078
#define HHI_VDEC2_CLK_CNTL 0x1079
/*add from M8M2*/
#define HHI_VDEC3_CLK_CNTL 0x107a
#define HHI_VDEC4_CLK_CNTL 0x107b

/* add from GXM */
#define HHI_WAVE420L_CLK_CNTL 0x109a

/* add from g12a */
#define HHI_WAVE420L_CLK_CNTL2 0x109b

#endif

