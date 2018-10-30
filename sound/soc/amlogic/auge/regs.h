/*
 * sound/soc/amlogic/auge/regs.h
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

#ifndef __AML_REGS_H_
#define __AML_REGS_H_

enum clk_sel {
	MASTER_A,
	MASTER_B,
	MASTER_C,
	MASTER_D,
	MASTER_E,
	MASTER_F,
	SLAVE_A,
	SLAVE_B,
	SLAVE_C,
	SLAVE_D,
	SLAVE_E,
	SLAVE_F,
	SLAVE_G,
	SLAVE_H,
	SLAVE_I,
	SLAVE_J
};

#define AUD_ADDR_OFFSET(addr)              ((addr) << 2)

/*
 *  PDM - Registers
 */
#define PDM_CTRL                           0x00
#define PDM_HCIC_CTRL1                     0x01
#define PDM_HCIC_CTRL2                     0x02
#define PDM_F1_CTRL                        0x03
#define PDM_F2_CTRL                        0x04
#define PDM_F3_CTRL                        0x05
#define PDM_HPF_CTRL                       0x06
#define PDM_CHAN_CTRL                      0x07
#define PDM_CHAN_CTRL1                     0x08
#define PDM_COEFF_ADDR                     0x09
#define PDM_COEFF_DATA                     0x0A
#define PDM_CLKG_CTRL                      0x0B
#define PDM_STS                            0x0C
#define PDM_MUTE_VALUE                     0x0D
#define PDM_MASK_NUM                       0x0E

/*
 *	AUDIO CLOCK, MST PAD,
 */
#define EE_AUDIO_CLK_GATE_EN0              0x000
#define EE_AUDIO_CLK_GATE_EN1              0x001
#define EE_AUDIO_MCLK_A_CTRL(offset)       (0x001 + offset)
#define EE_AUDIO_MCLK_B_CTRL(offset)       (0x002 + offset)
#define EE_AUDIO_MCLK_C_CTRL(offset)       (0x003 + offset)
#define EE_AUDIO_MCLK_D_CTRL(offset)       (0x004 + offset)
#define EE_AUDIO_MCLK_E_CTRL(offset)       (0x005 + offset)
#define EE_AUDIO_MCLK_F_CTRL(offset)       (0x006 + offset)
#define EE_AUDIO_MST_PAD_CTRL0(offset)     (0x007 + offset)
#define EE_AUDIO_MST_PAD_CTRL1(offset)     (0x008 + offset)
#define EE_AUDIO_SW_RESET0(offset)         (0x009 + offset)
#define EE_AUDIO_SW_RESET1                 0x00b
#define EE_AUDIO_CLK81_CTRL                0x00c
#define EE_AUDIO_CLK81_EN                  0x00d


#define EE_AUDIO_MST_A_SCLK_CTRL0          0x010
#define EE_AUDIO_MST_A_SCLK_CTRL1          0x011
#define EE_AUDIO_MST_B_SCLK_CTRL0          0x012
#define EE_AUDIO_MST_B_SCLK_CTRL1          0x013
#define EE_AUDIO_MST_C_SCLK_CTRL0          0x014
#define EE_AUDIO_MST_C_SCLK_CTRL1          0x015
#define EE_AUDIO_MST_D_SCLK_CTRL0          0x016
#define EE_AUDIO_MST_D_SCLK_CTRL1          0x017
#define EE_AUDIO_MST_E_SCLK_CTRL0          0x018
#define EE_AUDIO_MST_E_SCLK_CTRL1          0x019
#define EE_AUDIO_MST_F_SCLK_CTRL0          0x01a
#define EE_AUDIO_MST_F_SCLK_CTRL1          0x01b

#define EE_AUDIO_CLK_TDMIN_A_CTRL          0x020
#define EE_AUDIO_CLK_TDMIN_B_CTRL          0x021
#define EE_AUDIO_CLK_TDMIN_C_CTRL          0x022
#define EE_AUDIO_CLK_TDMIN_LB_CTRL         0x023
#define EE_AUDIO_CLK_TDMOUT_A_CTRL         0x024
#define EE_AUDIO_CLK_TDMOUT_B_CTRL         0x025
#define EE_AUDIO_CLK_TDMOUT_C_CTRL         0x026
#define EE_AUDIO_CLK_SPDIFIN_CTRL          0x027
#define EE_AUDIO_CLK_SPDIFOUT_CTRL         0x028
#define EE_AUDIO_CLK_RESAMPLEA_CTRL        0x029
#define EE_AUDIO_CLK_LOCKER_CTRL           0x02a
#define EE_AUDIO_CLK_PDMIN_CTRL0           0x02b
#define EE_AUDIO_CLK_PDMIN_CTRL1           0x02c
#define EE_AUDIO_CLK_SPDIFOUT_B_CTRL       0x02d
#define EE_AUDIO_CLK_RESAMPLEB_CTRL        0x02e
#define EE_AUDIO_CLK_SPDIFIN_LB_CTRL       0x02f
#define EE_AUDIO_CLK_EQDRC_CTRL0           0x030
#define EE_AUDIO_VAD_CLK_CTRL              0x031

/*
 *	AUDIO TODDR
 */
#define EE_AUDIO_TODDR_A_CTRL0             0x040
#define EE_AUDIO_TODDR_A_CTRL1             0x041
#define EE_AUDIO_TODDR_A_START_ADDR        0x042
#define EE_AUDIO_TODDR_A_FINISH_ADDR       0x043
#define EE_AUDIO_TODDR_A_INT_ADDR          0x044
#define EE_AUDIO_TODDR_A_STATUS1           0x045
#define EE_AUDIO_TODDR_A_STATUS2           0x046
#define EE_AUDIO_TODDR_A_START_ADDRB       0x047
#define EE_AUDIO_TODDR_A_FINISH_ADDRB      0x048
#define EE_AUDIO_TODDR_A_INIT_ADDR         0x049
#define EE_AUDIO_TODDR_A_CTRL2             0x04a

#define EE_AUDIO_TODDR_B_CTRL0             0x050
#define EE_AUDIO_TODDR_B_CTRL1             0x051
#define EE_AUDIO_TODDR_B_START_ADDR        0x052
#define EE_AUDIO_TODDR_B_FINISH_ADDR       0x053
#define EE_AUDIO_TODDR_B_INT_ADDR          0x054
#define EE_AUDIO_TODDR_B_STATUS1           0x055
#define EE_AUDIO_TODDR_B_STATUS2           0x056
#define EE_AUDIO_TODDR_B_START_ADDRB       0x057
#define EE_AUDIO_TODDR_B_FINISH_ADDRB      0x058
#define EE_AUDIO_TODDR_B_INIT_ADDR         0x059
#define EE_AUDIO_TODDR_B_CTRL2             0x05a

#define EE_AUDIO_TODDR_C_CTRL0             0x060
#define EE_AUDIO_TODDR_C_CTRL1             0x061
#define EE_AUDIO_TODDR_C_START_ADDR        0x062
#define EE_AUDIO_TODDR_C_FINISH_ADDR       0x063
#define EE_AUDIO_TODDR_C_INT_ADDR          0x064
#define EE_AUDIO_TODDR_C_STATUS1           0x065
#define EE_AUDIO_TODDR_C_STATUS2           0x066
#define EE_AUDIO_TODDR_C_START_ADDRB       0x067
#define EE_AUDIO_TODDR_C_FINISH_ADDRB      0x068
#define EE_AUDIO_TODDR_C_INIT_ADDR         0x069
#define EE_AUDIO_TODDR_C_CTRL2             0x06a

/*
 *	AUDIO FRDDR
 */
#define EE_AUDIO_FRDDR_A_CTRL0             0x070
#define EE_AUDIO_FRDDR_A_CTRL1             0x071
#define EE_AUDIO_FRDDR_A_START_ADDR        0x072
#define EE_AUDIO_FRDDR_A_FINISH_ADDR       0x073
#define EE_AUDIO_FRDDR_A_INT_ADDR          0x074
#define EE_AUDIO_FRDDR_A_STATUS1           0x075
#define EE_AUDIO_FRDDR_A_STATUS2           0x076
#define EE_AUDIO_FRDDR_A_START_ADDRB       0x077
#define EE_AUDIO_FRDDR_A_FINISH_ADDRB      0x078
#define EE_AUDIO_FRDDR_A_INIT_ADDR         0x079
#define EE_AUDIO_FRDDR_A_CTRL2             0x07a

#define EE_AUDIO_FRDDR_B_CTRL0             0x080
#define EE_AUDIO_FRDDR_B_CTRL1             0x081
#define EE_AUDIO_FRDDR_B_START_ADDR        0x082
#define EE_AUDIO_FRDDR_B_FINISH_ADDR       0x083
#define EE_AUDIO_FRDDR_B_INT_ADDR          0x084
#define EE_AUDIO_FRDDR_B_STATUS1           0x085
#define EE_AUDIO_FRDDR_B_STATUS2           0x086
#define EE_AUDIO_FRDDR_B_START_ADDRB       0x087
#define EE_AUDIO_FRDDR_B_FINISH_ADDRB      0x088
#define EE_AUDIO_FRDDR_B_INIT_ADDR         0x089
#define EE_AUDIO_FRDDR_B_CTRL2             0x08a

#define EE_AUDIO_FRDDR_C_CTRL0             0x090
#define EE_AUDIO_FRDDR_C_CTRL1             0x091
#define EE_AUDIO_FRDDR_C_START_ADDR        0x092
#define EE_AUDIO_FRDDR_C_FINISH_ADDR       0x093
#define EE_AUDIO_FRDDR_C_INT_ADDR          0x094
#define EE_AUDIO_FRDDR_C_STATUS1           0x095
#define EE_AUDIO_FRDDR_C_STATUS2           0x096
#define EE_AUDIO_FRDDR_C_START_ADDRB       0x097
#define EE_AUDIO_FRDDR_C_FINISH_ADDRB      0x098
#define EE_AUDIO_FRDDR_C_INIT_ADDR         0x099
#define EE_AUDIO_FRDDR_C_CTRL2             0x09a

/*
 *	AUDIO ARB,
 */
#define EE_AUDIO_ARB_CTRL                  0x0a0

/*
 *	AUDIO TDM
 */
#define EE_AUDIO_LB_CTRL0                  0x0b0
#define EE_AUDIO_LB_CTRL1                  0x0b1
#define EE_AUDIO_DAT_ID0                   0x0b2
#define EE_AUDIO_DAT_ID1                   0x0b3
#define EE_AUDIO_LB_ID0                    0x0b4
#define EE_AUDIO_LB_ID1                    0x0b5
#define EE_AUDIO_LB_STS                    0x0b6

#define EE_AUDIO_TDMIN_A_CTRL              0x0c0
#define EE_AUDIO_TDMIN_A_SWAP              0x0c1
#define EE_AUDIO_TDMIN_A_MASK0             0x0c2
#define EE_AUDIO_TDMIN_A_MASK1             0x0c3
#define EE_AUDIO_TDMIN_A_MASK2             0x0c4
#define EE_AUDIO_TDMIN_A_MASK3             0x0c5
#define EE_AUDIO_TDMIN_A_STAT              0x0c6
#define EE_AUDIO_TDMIN_A_MUTE_VAL          0x0c7
#define EE_AUDIO_TDMIN_A_MUTE0             0x0c8
#define EE_AUDIO_TDMIN_A_MUTE1             0x0c9
#define EE_AUDIO_TDMIN_A_MUTE2             0x0ca
#define EE_AUDIO_TDMIN_A_MUTE3             0x0cb

#define EE_AUDIO_TDMIN_B_CTRL              0x0d0
#define EE_AUDIO_TDMIN_B_SWAP              0x0d1
#define EE_AUDIO_TDMIN_B_MASK0             0x0d2
#define EE_AUDIO_TDMIN_B_MASK1             0x0d3
#define EE_AUDIO_TDMIN_B_MASK2             0x0d4
#define EE_AUDIO_TDMIN_B_MASK3             0x0d5
#define EE_AUDIO_TDMIN_B_STAT              0x0d6
#define EE_AUDIO_TDMIN_B_MUTE_VAL          0x0d7
#define EE_AUDIO_TDMIN_B_MUTE0             0x0d8
#define EE_AUDIO_TDMIN_B_MUTE1             0x0d9
#define EE_AUDIO_TDMIN_B_MUTE2             0x0da
#define EE_AUDIO_TDMIN_B_MUTE3             0x0db

#define EE_AUDIO_TDMIN_C_CTRL              0x0e0
#define EE_AUDIO_TDMIN_C_SWAP              0x0e1
#define EE_AUDIO_TDMIN_C_MASK0             0x0e2
#define EE_AUDIO_TDMIN_C_MASK1             0x0e3
#define EE_AUDIO_TDMIN_C_MASK2             0x0e4
#define EE_AUDIO_TDMIN_C_MASK3             0x0e5
#define EE_AUDIO_TDMIN_C_STAT              0x0e6
#define EE_AUDIO_TDMIN_C_MUTE_VAL          0x0e7
#define EE_AUDIO_TDMIN_C_MUTE0             0x0e8
#define EE_AUDIO_TDMIN_C_MUTE1             0x0e9
#define EE_AUDIO_TDMIN_C_MUTE2             0x0ea
#define EE_AUDIO_TDMIN_C_MUTE3             0x0eb

#define EE_AUDIO_TDMIN_LB_CTRL             0x0f0
#define EE_AUDIO_TDMIN_LB_SWAP             0x0f1
#define EE_AUDIO_TDMIN_LB_MASK0            0x0f2
#define EE_AUDIO_TDMIN_LB_MASK1            0x0f3
#define EE_AUDIO_TDMIN_LB_MASK2            0x0f4
#define EE_AUDIO_TDMIN_LB_MASK3            0x0f5
#define EE_AUDIO_TDMIN_LB_STAT             0x0f6
#define EE_AUDIO_TDMIN_LB_MUTE_VAL         0x0f7
#define EE_AUDIO_TDMIN_LB_MUTE0            0x0f8
#define EE_AUDIO_TDMIN_LB_MUTE1            0x0f9
#define EE_AUDIO_TDMIN_LB_MUTE2            0x0fa
#define EE_AUDIO_TDMIN_LB_MUTE3            0x0fb

/*
 *	AUDIO OUTPUT
 */
#define EE_AUDIO_SPDIFIN_CTRL0             0x100
#define EE_AUDIO_SPDIFIN_CTRL1             0x101
#define EE_AUDIO_SPDIFIN_CTRL2             0x102
#define EE_AUDIO_SPDIFIN_CTRL3             0x103
#define EE_AUDIO_SPDIFIN_CTRL4             0x104
#define EE_AUDIO_SPDIFIN_CTRL5             0x105
#define EE_AUDIO_SPDIFIN_CTRL6             0x106
#define EE_AUDIO_SPDIFIN_STAT0             0x107
#define EE_AUDIO_SPDIFIN_STAT1             0x108
#define EE_AUDIO_SPDIFIN_STAT2             0x109
#define EE_AUDIO_SPDIFIN_MUTE_VAL          0x10a

#define EE_AUDIO_RESAMPLEA_CTRL0           0x110
#define EE_AUDIO_RESAMPLEA_CTRL1           0x111
#define EE_AUDIO_RESAMPLEA_CTRL2           0x112
#define EE_AUDIO_RESAMPLEA_CTRL3           0x113
#define EE_AUDIO_RESAMPLEA_COEF0           0x114
#define EE_AUDIO_RESAMPLEA_COEF1           0x115
#define EE_AUDIO_RESAMPLEA_COEF2           0x116
#define EE_AUDIO_RESAMPLEA_COEF3           0x117
#define EE_AUDIO_RESAMPLEA_COEF4           0x118
#define EE_AUDIO_RESAMPLEA_STATUS1         0x119

#define EE_AUDIO_RESAMPLEB_CTRL0           0x1e0
#define EE_AUDIO_RESAMPLEB_CTRL1           0x1e1
#define EE_AUDIO_RESAMPLEB_CTRL2           0x1e2
#define EE_AUDIO_RESAMPLEB_CTRL3           0x1e3
#define EE_AUDIO_RESAMPLEB_COEF0           0x1e4
#define EE_AUDIO_RESAMPLEB_COEF1           0x1e5
#define EE_AUDIO_RESAMPLEB_COEF2           0x1e6
#define EE_AUDIO_RESAMPLEB_COEF3           0x1e7
#define EE_AUDIO_RESAMPLEB_COEF4           0x1e8
#define EE_AUDIO_RESAMPLEB_STATUS1         0x1e9

#define EE_AUDIO_SPDIFOUT_STAT             0x120
#define EE_AUDIO_SPDIFOUT_GAIN0            0x121
#define EE_AUDIO_SPDIFOUT_GAIN1            0x122
#define EE_AUDIO_SPDIFOUT_CTRL0            0x123
#define EE_AUDIO_SPDIFOUT_CTRL1            0x124
#define EE_AUDIO_SPDIFOUT_PREAMB           0x125
#define EE_AUDIO_SPDIFOUT_SWAP             0x126
#define EE_AUDIO_SPDIFOUT_CHSTS0           0x127
#define EE_AUDIO_SPDIFOUT_CHSTS1           0x128
#define EE_AUDIO_SPDIFOUT_CHSTS2           0x129
#define EE_AUDIO_SPDIFOUT_CHSTS3           0x12a
#define EE_AUDIO_SPDIFOUT_CHSTS4           0x12b
#define EE_AUDIO_SPDIFOUT_CHSTS5           0x12c
#define EE_AUDIO_SPDIFOUT_CHSTS6           0x12d
#define EE_AUDIO_SPDIFOUT_CHSTS7           0x12e
#define EE_AUDIO_SPDIFOUT_CHSTS8           0x12f
#define EE_AUDIO_SPDIFOUT_CHSTS9           0x130
#define EE_AUDIO_SPDIFOUT_CHSTSA           0x131
#define EE_AUDIO_SPDIFOUT_CHSTSB           0x132
#define EE_AUDIO_SPDIFOUT_MUTE_VAL         0x133

#define EE_AUDIO_TDMOUT_A_CTRL0            0x140
#define EE_AUDIO_TDMOUT_A_CTRL1            0x141
#define EE_AUDIO_TDMOUT_A_SWAP             0x142
#define EE_AUDIO_TDMOUT_A_MASK0            0x143
#define EE_AUDIO_TDMOUT_A_MASK1            0x144
#define EE_AUDIO_TDMOUT_A_MASK2            0x145
#define EE_AUDIO_TDMOUT_A_MASK3            0x146
#define EE_AUDIO_TDMOUT_A_STAT             0x147
#define EE_AUDIO_TDMOUT_A_GAIN0            0x148
#define EE_AUDIO_TDMOUT_A_GAIN1            0x149
#define EE_AUDIO_TDMOUT_A_MUTE_VAL         0x14a
#define EE_AUDIO_TDMOUT_A_MUTE0            0x14b
#define EE_AUDIO_TDMOUT_A_MUTE1            0x14c
#define EE_AUDIO_TDMOUT_A_MUTE2            0x14d
#define EE_AUDIO_TDMOUT_A_MUTE3            0x14e
#define EE_AUDIO_TDMOUT_A_MASK_VAL         0x14f

#define EE_AUDIO_TDMOUT_B_CTRL0            0x150
#define EE_AUDIO_TDMOUT_B_CTRL1            0x151
#define EE_AUDIO_TDMOUT_B_SWAP             0x152
#define EE_AUDIO_TDMOUT_B_MASK0            0x153
#define EE_AUDIO_TDMOUT_B_MASK1            0x154
#define EE_AUDIO_TDMOUT_B_MASK2            0x155
#define EE_AUDIO_TDMOUT_B_MASK3            0x156
#define EE_AUDIO_TDMOUT_B_STAT             0x157
#define EE_AUDIO_TDMOUT_B_GAIN0            0x158
#define EE_AUDIO_TDMOUT_B_GAIN1            0x159
#define EE_AUDIO_TDMOUT_B_MUTE_VAL         0x15a
#define EE_AUDIO_TDMOUT_B_MUTE0            0x15b
#define EE_AUDIO_TDMOUT_B_MUTE1            0x15c
#define EE_AUDIO_TDMOUT_B_MUTE2            0x15d
#define EE_AUDIO_TDMOUT_B_MUTE3            0x15e
#define EE_AUDIO_TDMOUT_B_MASK_VAL         0x15f

#define EE_AUDIO_TDMOUT_C_CTRL0            0x160
#define EE_AUDIO_TDMOUT_C_CTRL1            0x161
#define EE_AUDIO_TDMOUT_C_SWAP             0x162
#define EE_AUDIO_TDMOUT_C_MASK0            0x163
#define EE_AUDIO_TDMOUT_C_MASK1            0x164
#define EE_AUDIO_TDMOUT_C_MASK2            0x165
#define EE_AUDIO_TDMOUT_C_MASK3            0x166
#define EE_AUDIO_TDMOUT_C_STAT             0x167
#define EE_AUDIO_TDMOUT_C_GAIN0            0x168
#define EE_AUDIO_TDMOUT_C_GAIN1            0x169
#define EE_AUDIO_TDMOUT_C_MUTE_VAL         0x16a
#define EE_AUDIO_TDMOUT_C_MUTE0            0x16b
#define EE_AUDIO_TDMOUT_C_MUTE1            0x16c
#define EE_AUDIO_TDMOUT_C_MUTE2            0x16d
#define EE_AUDIO_TDMOUT_C_MUTE3            0x16e
#define EE_AUDIO_TDMOUT_C_MASK_VAL         0x16f

/*
 *	AUDIO POWER DETECT
 */
#define EE_AUDIO_POW_DET_CTRL0             0x180
#define EE_AUDIO_POW_DET_TH_HI             0x181
#define EE_AUDIO_POW_DET_TH_LO             0x182
#define EE_AUDIO_POW_DET_VALUE             0x183
#define EE_AUDIO_SECURITY_CTRL             0x193

/*
 *	AUDIO SPDIF_B
 */
#define EE_AUDIO_SPDIFOUT_B_STAT           0x1a0
#define EE_AUDIO_SPDIFOUT_B_GAIN0          0x1a1
#define EE_AUDIO_SPDIFOUT_B_GAIN1          0x1a2
#define EE_AUDIO_SPDIFOUT_B_CTRL0          0x1a3
#define EE_AUDIO_SPDIFOUT_B_CTRL1          0x1a4
#define EE_AUDIO_SPDIFOUT_B_PREAMB         0x1a5
#define EE_AUDIO_SPDIFOUT_B_SWAP           0x1a6
#define EE_AUDIO_SPDIFOUT_B_CHSTS0         0x1a7
#define EE_AUDIO_SPDIFOUT_B_CHSTS1         0x1a8
#define EE_AUDIO_SPDIFOUT_B_CHSTS2         0x1a9
#define EE_AUDIO_SPDIFOUT_B_CHSTS3         0x1aa
#define EE_AUDIO_SPDIFOUT_B_CHSTS4         0x1ab
#define EE_AUDIO_SPDIFOUT_B_CHSTS5         0x1ac
#define EE_AUDIO_SPDIFOUT_B_CHSTS6         0x1ad
#define EE_AUDIO_SPDIFOUT_B_CHSTS7         0x1ae
#define EE_AUDIO_SPDIFOUT_B_CHSTS8         0x1af
#define EE_AUDIO_SPDIFOUT_B_CHSTS9         0x1b0
#define EE_AUDIO_SPDIFOUT_B_CHSTSA         0x1b1
#define EE_AUDIO_SPDIFOUT_B_CHSTSB         0x1b2
#define EE_AUDIO_SPDIFOUT_B_MUTE_VAL       0x1b3

/*
 *	AUDIO LOCKER
 */
#define EE_AUDIO_TORAM_CTRL0               0x1c0
#define EE_AUDIO_TORAM_CTRL1               0x1c1
#define EE_AUDIO_TORAM_START_ADDR          0x1c2
#define EE_AUDIO_TORAM_FINISH_ADDR         0x1c3
#define EE_AUDIO_TORAM_INT_ADDR            0x1c4
#define EE_AUDIO_TORAM_STATUS1             0x1c5
#define EE_AUDIO_TORAM_STATUS2             0x1c6
#define EE_AUDIO_TORAM_INIT_ADDR           0x1c7

/*
 *	HIU, AUDIO CODEC RESET
 */
#define EE_RESET1                          0x002

/*
 *	HIU, ARC
 */
#define HHI_HDMIRX_ARC_CNTL                0xe8

/*
 *	AUDIO MUX CONTROLS
 */
#define EE_AUDIO_TOACODEC_CTRL0            0x1d0
#define EE_AUDIO_TOHDMITX_CTRL0            0x1d1
#define EE_AUDIO_TOVAD_CTRL0               0x1d2
#define EE_AUDIO_FRATV_CTRL0               0x1d3

#define EE_AUDIO_SPDIFIN_LB_CTRL0          0x1f0
#define EE_AUDIO_SPDIFIN_LB_CTRL1          0x1f1
#define EE_AUDIO_SPDIFIN_LB_CTRL6          0x1f6
#define EE_AUDIO_SPDIFIN_LB_STAT0          0x1f7
#define EE_AUDIO_SPDIFIN_LB_STAT1          0x1f8
#define EE_AUDIO_SPDIFIN_LB_MUTE_VAL       0x1fa

#define EE_AUDIO_FRHDMIRX_CTRL0            0x200
#define EE_AUDIO_FRHDMIRX_CTRL1            0x201
#define EE_AUDIO_FRHDMIRX_CTRL2            0x202
#define EE_AUDIO_FRHDMIRX_CTRL3            0x203
#define EE_AUDIO_FRHDMIRX_CTRL4            0x204
#define EE_AUDIO_FRHDMIRX_CTRL5            0x205
#define EE_AUDIO_FRHDMIRX_STAT0            0x20a
#define EE_AUDIO_FRHDMIRX_STAT1            0x20b

#define EE_AUDIO_TODDR_D_CTRL0             0x210
#define EE_AUDIO_TODDR_D_CTRL1             0x211
#define EE_AUDIO_TODDR_D_START_ADDR        0x212
#define EE_AUDIO_TODDR_D_FINISH_ADDR       0x213
#define EE_AUDIO_TODDR_D_INT_ADDR          0x214
#define EE_AUDIO_TODDR_D_STATUS1           0x215
#define EE_AUDIO_TODDR_D_STATUS2           0x216
#define EE_AUDIO_TODDR_D_START_ADDRB       0x217
#define EE_AUDIO_TODDR_D_FINISH_ADDRB      0x218
#define EE_AUDIO_TODDR_D_INIT_ADDR         0x219
#define EE_AUDIO_TODDR_D_CTRL2             0x21a

#define EE_AUDIO_FRDDR_D_CTRL0             0x220
#define EE_AUDIO_FRDDR_D_CTRL1             0x221
#define EE_AUDIO_FRDDR_D_START_ADDR        0x222
#define EE_AUDIO_FRDDR_D_FINISH_ADDR       0x223
#define EE_AUDIO_FRDDR_D_INT_ADDR          0x224
#define EE_AUDIO_FRDDR_D_STATUS1           0x225
#define EE_AUDIO_FRDDR_D_STATUS2           0x226
#define EE_AUDIO_FRDDR_D_START_ADDRB       0x227
#define EE_AUDIO_FRDDR_D_FINISH_ADDRB      0x228
#define EE_AUDIO_FRDDR_D_INIT_ADDR         0x229
#define EE_AUDIO_FRDDR_D_CTRL2             0x22a

#define EE_AUDIO_LB_B_CTRL0                0x230
#define EE_AUDIO_LB_B_CTRL1                0x231
#define EE_AUDIO_LB_B_CTRL2                0x232
#define EE_AUDIO_LB_B_CTRL3                0x233
#define EE_AUDIO_LB_B_DAT_CH_ID0           0x234
#define EE_AUDIO_LB_B_DAT_CH_ID1           0x235
#define EE_AUDIO_LB_B_DAT_CH_ID2           0x236
#define EE_AUDIO_LB_B_DAT_CH_ID3           0x237
#define EE_AUDIO_LB_B_LB_CH_ID0            0x238
#define EE_AUDIO_LB_B_LB_CH_ID1            0x239
#define EE_AUDIO_LB_B_LB_CH_ID2            0x23a
#define EE_AUDIO_LB_B_LB_CH_ID3            0x23b
#define EE_AUDIO_LB_B_STS                  0x23c

/*
 *	AUDIO LOCKER
 */
#define AUD_LOCK_EN                        0x000
#define AUD_LOCK_SW_RESET                  0x001
#define AUD_LOCK_SW_LATCH                  0x002
#define AUD_LOCK_HW_LATCH                  0x003
#define AUD_LOCK_REFCLK_SRC                0x004
#define AUD_LOCK_REFCLK_LAT_INT            0x005
#define AUD_LOCK_IMCLK_LAT_INT             0x006
#define AUD_LOCK_OMCLK_LAT_INT             0x007
#define AUD_LOCK_REFCLK_DS_INT             0x008
#define AUD_LOCK_IMCLK_DS_INT              0x009
#define AUD_LOCK_OMCLK_DS_INT              0x00a
#define AUD_LOCK_INT_CLR                   0x00b
#define AUD_LOCK_GCLK_CTRL                 0x00c
#define AUD_LOCK_INT_CTRL                  0x00d
#define RO_REF2IMCLK_CNT_L                 0x010
#define RO_REF2IMCLK_CNT_H                 0x011
#define RO_REF2OMCLK_CNT_L                 0x012
#define RO_REF2OMCLK_CNT_H                 0x013
#define RO_IMCLK2REF_CNT_L                 0x014
#define RO_IMCLK2REF_CNT_H                 0x015
#define RO_OMCLK2REF_CNT_L                 0x016
#define RO_OMCLK2REF_CNT_H                 0x017
#define RO_REFCLK_PKG_CNT                  0x018
#define RO_IMCLK_PKG_CNT                   0x019
#define RO_OMCLK_PKG_CNT                   0x01a
#define RO_AUD_LOCK_INT_STATUS             0x01b

/*
 * EQ DRC, G12X means g12a, g12b
 */
#define AED_EQ_CH1_COEF00                  0x00
#define AED_EQ_CH1_COEF01                  0x01
#define AED_EQ_CH1_COEF02                  0x02
#define AED_EQ_CH1_COEF03                  0x03
#define AED_EQ_CH1_COEF04                  0x04
#define AED_EQ_CH1_COEF10                  0x05
#define AED_EQ_CH1_COEF11                  0x06
#define AED_EQ_CH1_COEF12                  0x07
#define AED_EQ_CH1_COEF13                  0x08
#define AED_EQ_CH1_COEF14                  0x09
#define AED_EQ_CH1_COEF20                  0x0a
#define AED_EQ_CH1_COEF21                  0x0b
#define AED_EQ_CH1_COEF22                  0x0c
#define AED_EQ_CH1_COEF23                  0x0d
#define AED_EQ_CH1_COEF24                  0x0e
#define AED_EQ_CH1_COEF30                  0x0f
#define AED_EQ_CH1_COEF31                  0x10
#define AED_EQ_CH1_COEF32                  0x11
#define AED_EQ_CH1_COEF33                  0x12
#define AED_EQ_CH1_COEF34                  0x13
#define AED_EQ_CH1_COEF40                  0x14
#define AED_EQ_CH1_COEF41                  0x15
#define AED_EQ_CH1_COEF42                  0x16
#define AED_EQ_CH1_COEF43                  0x17
#define AED_EQ_CH1_COEF44                  0x18
#define AED_EQ_CH1_COEF50                  0x19
#define AED_EQ_CH1_COEF51                  0x1a
#define AED_EQ_CH1_COEF52                  0x1b
#define AED_EQ_CH1_COEF53                  0x1c
#define AED_EQ_CH1_COEF54                  0x1d
#define AED_EQ_CH1_COEF60                  0x1e
#define AED_EQ_CH1_COEF61                  0x1f
#define AED_EQ_CH1_COEF62                  0x20
#define AED_EQ_CH1_COEF63                  0x21
#define AED_EQ_CH1_COEF64                  0x22
#define AED_EQ_CH1_COEF70                  0x23
#define AED_EQ_CH1_COEF71                  0x24
#define AED_EQ_CH1_COEF72                  0x25
#define AED_EQ_CH1_COEF73                  0x26
#define AED_EQ_CH1_COEF74                  0x27
#define AED_EQ_CH1_COEF80                  0x28
#define AED_EQ_CH1_COEF81                  0x29
#define AED_EQ_CH1_COEF82                  0x2a
#define AED_EQ_CH1_COEF83                  0x2b
#define AED_EQ_CH1_COEF84                  0x2c
#define AED_EQ_CH1_COEF90                  0x2d
#define AED_EQ_CH1_COEF91                  0x2e
#define AED_EQ_CH1_COEF92                  0x2f
#define AED_EQ_CH1_COEF93                  0x30
#define AED_EQ_CH1_COEF94                  0x31
#define AED_EQ_CH2_COEF00                  0x32
#define AED_EQ_CH2_COEF01                  0x33
#define AED_EQ_CH2_COEF02                  0x34
#define AED_EQ_CH2_COEF03                  0x35
#define AED_EQ_CH2_COEF04                  0x36
#define AED_EQ_CH2_COEF10                  0x37
#define AED_EQ_CH2_COEF11                  0x38
#define AED_EQ_CH2_COEF12                  0x39
#define AED_EQ_CH2_COEF13                  0x3a
#define AED_EQ_CH2_COEF14                  0x3b
#define AED_EQ_CH2_COEF20                  0x3c
#define AED_EQ_CH2_COEF21                  0x3d
#define AED_EQ_CH2_COEF22                  0x3e
#define AED_EQ_CH2_COEF23                  0x3f
#define AED_EQ_CH2_COEF24                  0x40
#define AED_EQ_CH2_COEF30                  0x41
#define AED_EQ_CH2_COEF31                  0x42
#define AED_EQ_CH2_COEF32                  0x43
#define AED_EQ_CH2_COEF33                  0x44
#define AED_EQ_CH2_COEF34                  0x45
#define AED_EQ_CH2_COEF40                  0x46
#define AED_EQ_CH2_COEF41                  0x47
#define AED_EQ_CH2_COEF42                  0x48
#define AED_EQ_CH2_COEF43                  0x49
#define AED_EQ_CH2_COEF44                  0x4a
#define AED_EQ_CH2_COEF50                  0x4b
#define AED_EQ_CH2_COEF51                  0x4c
#define AED_EQ_CH2_COEF52                  0x4d
#define AED_EQ_CH2_COEF53                  0x4e
#define AED_EQ_CH2_COEF54                  0x4f
#define AED_EQ_CH2_COEF60                  0x50
#define AED_EQ_CH2_COEF61                  0x51
#define AED_EQ_CH2_COEF62                  0x52
#define AED_EQ_CH2_COEF63                  0x53
#define AED_EQ_CH2_COEF64                  0x54
#define AED_EQ_CH2_COEF70                  0x55
#define AED_EQ_CH2_COEF71                  0x56
#define AED_EQ_CH2_COEF72                  0x57
#define AED_EQ_CH2_COEF73                  0x58
#define AED_EQ_CH2_COEF74                  0x59
#define AED_EQ_CH2_COEF80                  0x5a
#define AED_EQ_CH2_COEF81                  0x5b
#define AED_EQ_CH2_COEF82                  0x5c
#define AED_EQ_CH2_COEF83                  0x5d
#define AED_EQ_CH2_COEF84                  0x5e
#define AED_EQ_CH2_COEF90                  0x5f
#define AED_EQ_CH2_COEF91                  0x60
#define AED_EQ_CH2_COEF92                  0x61
#define AED_EQ_CH2_COEF93                  0x62
#define AED_EQ_CH2_COEF94                  0x63
#define AED_EQ_EN_G12X                     0x64
#define AED_EQ_VOLUME_G12X                 0x65
#define AED_EQ_VOLUME_SLEW_CNT_G12X        0x66
#define AED_MUTE_G12X                      0x67
#define AED_DRC_EN                         0x68
#define AED_DRC_AE                         0x69
#define AED_DRC_AA                         0x6a
#define AED_DRC_AD                         0x6b
#define AED_DRC_AE_1M                      0x6c
#define AED_DRC_AA_1M                      0x6d
#define AED_DRC_AD_1M                      0x6e
#define AED_DRC_OFFSET0                    0x6f
#define AED_DRC_OFFSET1                    0x70
#define AED_DRC_THD0_G12X                  0x71
#define AED_DRC_THD1_G12X                  0x72
#define AED_DRC_K0_G12X                    0x73
#define AED_DRC_K1_G12X                    0x74
#define AED_CLIP_THD_G12X                  0x75
#define AED_NG_THD0                        0x76
#define AED_NG_THD1                        0x77
#define AED_NG_CNT_THD                     0x78
#define AED_NG_CTL                         0x79
#define AED_ED_CTL                         0x7a
#define AED_DEBUG0                         0x7b
#define AED_DEBUG1                         0x7c
#define AED_DEBUG2                         0x7d
#define AED_DEBUG3                         0x7e
#define AED_DEBUG4                         0x7f
#define AED_DEBUG5                         0x80
#define AED_DEBUG6                         0x81
#define AED_DRC_AA_H                       0x82
#define AED_DRC_AD_H                       0x83
#define AED_DRC_AA_1M_H                    0x84
#define AED_DRC_AD_1M_H                    0x85
#define AED_NG_CNT                         0x86
#define AED_NG_STEP                        0x87

#define AED_TOP_CTL_G12X                   0x88
#define AED_TOP_REQ_CTL_G12X               0x89

/*
 * EQ DRC, New ARCH, from tl1
 */
#define AED_COEF_RAM_CNTL                  0x00
#define AED_COEF_RAM_DATA                  0x01
#define AED_EQ_EN                          0x02
#define AED_EQ_TAP_CNTL                    0x03
#define AED_EQ_VOLUME                      0x04
#define AED_EQ_VOLUME_SLEW_CNT             0x05
#define AED_MUTE                           0x06
#define AED_DRC_CNTL                       0x07
#define AED_DRC_RMS_COEF0                  0x08
#define AED_DRC_RMS_COEF1                  0x09
#define AED_DRC_THD0                       0x0a
#define AED_DRC_THD1                       0x0b
#define AED_DRC_THD2                       0x0c
#define AED_DRC_THD3                       0x0d
#define AED_DRC_THD4                       0x0e
#define AED_DRC_K0                         0x0f
#define AED_DRC_K1                         0x10
#define AED_DRC_K2                         0x11
#define AED_DRC_K3                         0x12
#define AED_DRC_K4                         0x13
#define AED_DRC_K5                         0x14
#define AED_DRC_THD_OUT0                   0x15
#define AED_DRC_THD_OUT1                   0x16
#define AED_DRC_THD_OUT2                   0x17
#define AED_DRC_THD_OUT3                   0x18
#define AED_DRC_OFFSET                     0x19
#define AED_DRC_RELEASE_COEF00             0x1a
#define AED_DRC_RELEASE_COEF01             0x1b
#define AED_DRC_RELEASE_COEF10             0x1c
#define AED_DRC_RELEASE_COEF11             0x1d
#define AED_DRC_RELEASE_COEF20             0x1e
#define AED_DRC_RELEASE_COEF21             0x1f
#define AED_DRC_RELEASE_COEF30             0x20
#define AED_DRC_RELEASE_COEF31             0x21
#define AED_DRC_RELEASE_COEF40             0x22
#define AED_DRC_RELEASE_COEF41             0x23
#define AED_DRC_RELEASE_COEF50             0x24
#define AED_DRC_RELEASE_COEF51             0x25
#define AED_DRC_ATTACK_COEF00              0x26
#define AED_DRC_ATTACK_COEF01              0x27
#define AED_DRC_ATTACK_COEF10              0x28
#define AED_DRC_ATTACK_COEF11              0x29
#define AED_DRC_ATTACK_COEF20              0x2a
#define AED_DRC_ATTACK_COEF21              0x2b
#define AED_DRC_ATTACK_COEF30              0x2c
#define AED_DRC_ATTACK_COEF31              0x2d
#define AED_DRC_ATTACK_COEF40              0x2e
#define AED_DRC_ATTACK_COEF41              0x2f
#define AED_DRC_ATTACK_COEF50              0x30
#define AED_DRC_ATTACK_COEF51              0x31
#define AED_DRC_LOOPBACK_CNTL              0x32
#define AED_MDRC_CNTL                      0x33
#define AED_MDRC_RMS_COEF00                0x34
#define AED_MDRC_RMS_COEF01                0x35
#define AED_MDRC_RELEASE_COEF00            0x36
#define AED_MDRC_RELEASE_COEF01            0x37
#define AED_MDRC_ATTACK_COEF00             0x38
#define AED_MDRC_ATTACK_COEF01             0x39
#define AED_MDRC_THD0                      0x3a
#define AED_MDRC_K0                        0x3b
#define AED_MDRC_LOW_GAIN                  0x3c
#define AED_MDRC_OFFSET0                   0x3d
#define AED_MDRC_RMS_COEF10                0x3e
#define AED_MDRC_RMS_COEF11                0x3f
#define AED_MDRC_RELEASE_COEF10            0x40
#define AED_MDRC_RELEASE_COEF11            0x41
#define AED_MDRC_ATTACK_COEF10             0x42
#define AED_MDRC_ATTACK_COEF11             0x43
#define AED_MDRC_THD1                      0x44
#define AED_MDRC_K1                        0x45
#define AED_MDRC_OFFSET1                   0x46
#define AED_MDRC_MID_GAIN                  0x47
#define AED_MDRC_RMS_COEF20                0x48
#define AED_MDRC_RMS_COEF21                0x49
#define AED_MDRC_RELEASE_COEF20            0x4a
#define AED_MDRC_RELEASE_COEF21            0x4b
#define AED_MDRC_ATTACK_COEF20             0x4c
#define AED_MDRC_ATTACK_COEF21             0x4d
#define AED_MDRC_THD2                      0x4e
#define AED_MDRC_K2                        0x4f
#define AED_MDRC_OFFSET2                   0x50
#define AED_MDRC_HIGH_GAIN                 0x51
#define AED_ED_CNTL                        0x52
#define AED_DC_EN                          0x53
#define AED_ND_LOW_THD                     0x54
#define AED_ND_HIGH_THD                    0x55
#define AED_ND_CNT_THD                     0x56
#define AED_ND_SUM_NUM                     0x57
#define AED_ND_CZ_NUM                      0x58
#define AED_ND_SUM_THD0                    0x59
#define AED_ND_SUM_THD1                    0x5a
#define AED_ND_CZ_THD0                     0x5b
#define AED_ND_CZ_THD1                     0x5c
#define AED_ND_COND_CNTL                   0x5d
#define AED_ND_RELEASE_COEF0               0x5e
#define AED_ND_RELEASE_COEF1               0x5f
#define AED_ND_ATTACK_COEF0                0x60
#define AED_ND_ATTACK_COEF1                0x61
#define AED_ND_CNTL                        0x62
#define AED_MIX0_LL                        0x63
#define AED_MIX0_RL                        0x64
#define AED_MIX0_LR                        0x65
#define AED_MIX0_RR                        0x66
#define AED_CLIP_THD                       0x67
#define AED_CH1_ND_SUM_OUT                 0x68
#define AED_CH2_ND_SUM_OUT                 0x69
#define AED_CH1_ND_CZ_OUT                  0x6a
#define AED_CH2_ND_CZ_OUT                  0x6b
#define AED_NOISE_STATUS                   0x6c
#define AED_POW_CURRENT_S0                 0x6d
#define AED_POW_CURRENT_S1                 0x6e
#define AED_POW_CURRENT_S2                 0x6f
#define AED_POW_OUT0                       0x70
#define AED_POW_OUT1                       0x71
#define AED_POW_OUT2                       0x72
#define AED_POW_ADJ_INDEX0                 0x73
#define AED_POW_ADJ_INDEX1                 0x74
#define AED_POW_ADJ_INDEX2                 0x75
#define AED_DRC_GAIN_INDEX0                0x76
#define AED_DRC_GAIN_INDEX1                0x77
#define AED_DRC_GAIN_INDEX2                0x78
#define AED_CH1_VOLUME_STATE               0x79
#define AED_CH2_VOLUME_STATE               0x7a
#define AED_CH1_VOLUME_GAIN                0x7b
#define AED_CH2_VOLUME_GAIN                0x7c
#define AED_FULL_POW_CURRENT               0x7d
#define AED_FULL_POW_OUT                   0x7e
#define AED_FULL_POW_ADJ                   0x7f
#define AED_FULL_DRC_GAIN                  0x80
#define AED_MASTER_VOLUME_STATE            0x81
#define AED_MASTER_VOLUME_GAIN             0x82

#define AED_TOP_CTL                        0x83
#define AED_TOP_REQ_CTL                    0x84


/*
 * VAD, Voice activity detection
 */
#define VAD_TOP_CTRL0                      0x000
#define VAD_TOP_CTRL1                      0x001
#define VAD_TOP_CTRL2                      0x002
#define VAD_FIR_CTRL	                   0x003
#define VAD_FIR_EMP                        0x004
#define VAD_FIR_COEF0                      0x005
#define VAD_FIR_COEF1                      0x006
#define VAD_FIR_COEF2                      0x007
#define VAD_FIR_COEF3                      0x008
#define VAD_FIR_COEF4                      0x009
#define VAD_FIR_COEF5                      0x00a
#define VAD_FIR_COEF6                      0x00b
#define VAD_FIR_COEF7                      0x00c
#define VAD_FIR_COEF8                      0x00d
#define VAD_FIR_COEF9                      0x00e
#define VAD_FIR_COEF10                     0x00f
#define VAD_FIR_COEF11                     0x010
#define VAD_FIR_COEF12                     0x011
#define VAD_FRAME_CTRL0                    0x012
#define VAD_FRAME_CTRL1                    0x013
#define VAD_FRAME_CTRL2                    0x014
#define VAD_CEP_CTRL0                      0x015
#define VAD_CEP_CTRL1                      0x016
#define VAD_CEP_CTRL2                      0x017
#define VAD_CEP_CTRL3                      0x018
#define VAD_CEP_CTRL4                      0x019
#define VAD_CEP_CTRL5                      0x01a
#define VAD_DEC_CTRL                       0x01b
#define VAD_TOP_STS0                       0x01c
#define VAD_TOP_STS1                       0x01d
#define VAD_TOP_STS2                       0x01e
#define VAD_FIR_STS0                       0x01f
#define VAD_FIR_STS1                       0x020
#define VAD_POW_STS0                       0x021
#define VAD_POW_STS1                       0x022
#define VAD_POW_STS2                       0x023
#define VAD_FFT_STS0                       0x024
#define VAD_FFT_STS1                       0x025
#define VAD_SPE_STS0                       0x026
#define VAD_SPE_STS1                       0x027
#define VAD_SPE_STS2                       0x028
#define VAD_SPE_STS3                       0x029
#define VAD_DEC_STS0                       0x02a
#define VAD_DEC_STS1                       0x02b
#define VAD_LUT_CTRL                       0x02c
#define VAD_LUT_WR                         0x02d
#define VAD_LUT_RD                         0x02e
#define VAD_IN_SEL0                        0x02f
#define VAD_IN_SEL1                        0x030
#define VAD_TO_DDR                         0x031

#endif
