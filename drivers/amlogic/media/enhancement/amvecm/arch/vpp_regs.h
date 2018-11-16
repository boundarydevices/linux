/*
 * drivers/amlogic/media/enhancement/amvecm/arch/vpp_regs.h
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

#ifndef VPP_REGS_HEADER_
#define VPP_REGS_HEADER_


#define VPP_DUMMY_DATA 0x1d00
#define VPP_LINE_IN_LENGTH 0x1d01
#define VPP_PIC_IN_HEIGHT 0x1d02
#define VPP_SCALE_COEF_IDX 0x1d03
#define VPP_SCALE_COEF 0x1d04
#define VPP_VSC_REGION12_STARTP 0x1d05
#define VPP_VSC_REGION34_STARTP 0x1d06
#define VPP_VSC_REGION4_ENDP 0x1d07
#define VPP_VSC_START_PHASE_STEP 0x1d08
#define VPP_VSC_REGION0_PHASE_SLOPE 0x1d09
#define VPP_VSC_REGION1_PHASE_SLOPE 0x1d0a
#define VPP_VSC_REGION3_PHASE_SLOPE 0x1d0b
#define VPP_VSC_REGION4_PHASE_SLOPE 0x1d0c
#define VPP_VSC_PHASE_CTRL 0x1d0d
#define VPP_VSC_INI_PHASE 0x1d0e
#define VPP_HSC_REGION12_STARTP 0x1d10
#define VPP_HSC_REGION34_STARTP 0x1d11
#define VPP_HSC_REGION4_ENDP 0x1d12
#define VPP_HSC_START_PHASE_STEP 0x1d13
#define VPP_HSC_REGION0_PHASE_SLOPE 0x1d14
#define VPP_HSC_REGION1_PHASE_SLOPE 0x1d15
#define VPP_HSC_REGION3_PHASE_SLOPE 0x1d16
#define VPP_HSC_REGION4_PHASE_SLOPE 0x1d17
#define VPP_HSC_PHASE_CTRL 0x1d18
#define VPP_SC_MISC 0x1d19
#define VPP_PREBLEND_VD1_H_START_END 0x1d1a
#define VPP_PREBLEND_VD1_V_START_END 0x1d1b
#define VPP_POSTBLEND_VD1_H_START_END 0x1d1c
#define VPP_POSTBLEND_VD1_V_START_END 0x1d1d
#define VPP_BLEND_VD2_H_START_END 0x1d1e
#define VPP_BLEND_VD2_V_START_END 0x1d1f
#define VPP_PREBLEND_H_SIZE 0x1d20
#define VPP_POSTBLEND_H_SIZE 0x1d21
#define VPP_HOLD_LINES 0x1d22
#define VPP_BLEND_ONECOLOR_CTRL 0x1d23
#define VPP_PREBLEND_CURRENT_XY 0x1d24
#define VPP_POSTBLEND_CURRENT_XY 0x1d25
#define VPP_MISC 0x1d26
#define VPP_OFIFO_SIZE 0x1d27
#define VPP_FIFO_STATUS 0x1d28
#define VPP_SMOKE_CTRL 0x1d29
#define VPP_SMOKE1_VAL 0x1d2a
#define VPP_SMOKE2_VAL 0x1d2b
#define VPP_SMOKE3_VAL 0x1d2c
#define VPP_SMOKE1_H_START_END 0x1d2d
#define VPP_SMOKE1_V_START_END 0x1d2e
#define VPP_SMOKE2_H_START_END 0x1d2f
#define VPP_SMOKE2_V_START_END 0x1d30
#define VPP_SMOKE3_H_START_END 0x1d31
#define VPP_SMOKE3_V_START_END 0x1d32
#define VPP_SCO_FIFO_CTRL 0x1d33
#define VPP_HSC_PHASE_CTRL1 0x1d34
#define VPP_HSC_INI_PAT_CTRL 0x1d35
#define VPP_VADJ_CTRL 0x1d40
#define VPP_VADJ1_Y 0x1d41
#define VPP_VADJ1_MA_MB 0x1d42
#define VPP_VADJ1_MC_MD 0x1d43
#define VPP_VADJ2_Y 0x1d44
#define VPP_VADJ2_MA_MB 0x1d45
#define VPP_VADJ2_MC_MD 0x1d46
#define VPP_HSHARP_CTRL 0x1d50
#define VPP_HSHARP_LUMA_THRESH01 0x1d51
#define VPP_HSHARP_LUMA_THRESH23 0x1d52
#define VPP_HSHARP_CHROMA_THRESH01 0x1d53
#define VPP_HSHARP_CHROMA_THRESH23 0x1d54
#define VPP_HSHARP_LUMA_GAIN 0x1d55
#define VPP_HSHARP_CHROMA_GAIN 0x1d56
#define VPP_MATRIX_PROBE_COLOR 0x1d5c
#define VPP_MATRIX_PROBE_COLOR1			0x1dd7
#define VPP_MATRIX_HL_COLOR 0x1d5d
#define VPP_MATRIX_PROBE_POS 0x1d5e
#define VPP_MATRIX_CTRL 0x1d5f
#define VPP_MATRIX_COEF00_01 0x1d60
#define VPP_MATRIX_COEF02_10 0x1d61
#define VPP_MATRIX_COEF11_12 0x1d62
#define VPP_MATRIX_COEF20_21 0x1d63
#define VPP_MATRIX_COEF22 0x1d64
#define VPP_MATRIX_OFFSET0_1 0x1d65
#define VPP_MATRIX_OFFSET2 0x1d66
#define VPP_MATRIX_PRE_OFFSET0_1 0x1d67
#define VPP_MATRIX_PRE_OFFSET2 0x1d68
#define VPP_DUMMY_DATA1 0x1d69
#define VPP_GAINOFF_CTRL0 0x1d6a
#define VPP_GAINOFF_CTRL1 0x1d6b
#define VPP_GAINOFF_CTRL2 0x1d6c
#define VPP_GAINOFF_CTRL3 0x1d6d
#define VPP_GAINOFF_CTRL4 0x1d6e
#define VPP_CHROMA_ADDR_PORT 0x1d70
#define VPP_CHROMA_DATA_PORT 0x1d71
#define VPP_GCLK_CTRL0 0x1d72
#define VPP_GCLK_CTRL1 0x1d73
#define VPP_SC_GCLK_CTRL 0x1d74
#define VPP_MISC1 0x1d76
#define VPP_BLACKEXT_CTRL 0x1d80
#define VPP_DNLP_CTRL_00 0x1d81
#define VPP_DNLP_CTRL_01 0x1d82
#define VPP_DNLP_CTRL_02 0x1d83
#define VPP_DNLP_CTRL_03 0x1d84
#define VPP_DNLP_CTRL_04 0x1d85
#define VPP_DNLP_CTRL_05 0x1d86
#define VPP_DNLP_CTRL_06 0x1d87
#define VPP_DNLP_CTRL_07 0x1d88
#define VPP_DNLP_CTRL_08 0x1d89
#define VPP_DNLP_CTRL_09 0x1d8a
#define VPP_DNLP_CTRL_10 0x1d8b
#define VPP_DNLP_CTRL_11 0x1d8c
#define VPP_DNLP_CTRL_12 0x1d8d
#define VPP_DNLP_CTRL_13 0x1d8e
#define VPP_DNLP_CTRL_14 0x1d8f
#define VPP_DNLP_CTRL_15 0x1d90
#define VPP_SRSHARP0_CTRL 0x1d91
#define VPP_SRSHARP1_CTRL 0x1d92
#define VPP_PEAKING_NLP_1 0x1d93
#define VPP_PEAKING_NLP_2 0x1d94
#define VPP_PEAKING_NLP_3 0x1d95
#define VPP_PEAKING_NLP_4 0x1d96
#define VPP_PEAKING_NLP_5 0x1d97
#define VPP_SHARP_LIMIT 0x1d98
#define VPP_VLTI_CTRL 0x1d99
#define VPP_HLTI_CTRL 0x1d9a
#define VPP_CTI_CTRL 0x1d9b
#define VPP_BLUE_STRETCH_1 0x1d9c
#define VPP_BLUE_STRETCH_2 0x1d9d
#define VPP_BLUE_STRETCH_3 0x1d9e
#define VPP_CCORING_CTRL 0x1da0
#define VPP_VE_ENABLE_CTRL 0x1da1
#define VPP_VE_DEMO_LEFT_TOP_SCREEN_WIDTH 0x1da2
#define VPP_VE_DEMO_CENTER_BAR 0x1da3
#define VPP_VE_H_V_SIZE 0x1da4
#define VPP_PSR_H_V_SIZE 0x1da5
#define VPP_OUT_H_V_SIZE 0x1da5
#define VPP_VDO_MEAS_CTRL 0x1da8
#define VPP_VDO_MEAS_VS_COUNT_HI 0x1da9
#define VPP_VDO_MEAS_VS_COUNT_LO 0x1daa
#define VPP_INPUT_CTRL 0x1dab
#define VPP_CTI_CTRL2 0x1dac
#define VPP_PEAKING_SAT_THD1 0x1dad
#define VPP_PEAKING_SAT_THD2 0x1dae
#define VPP_PEAKING_SAT_THD3 0x1daf
#define VPP_PEAKING_SAT_THD4 0x1db0
#define VPP_PEAKING_SAT_THD5 0x1db1
#define VPP_PEAKING_SAT_THD6 0x1db2
#define VPP_PEAKING_SAT_THD7 0x1db3
#define VPP_PEAKING_SAT_THD8 0x1db4
#define VPP_PEAKING_SAT_THD9 0x1db5
#define VPP_PEAKING_GAIN_ADD1 0x1db6
#define VPP_PEAKING_GAIN_ADD2 0x1db7
#define VPP_PEAKING_DNLP 0x1db8
#define VPP_SHARP_DEMO_WIN_CTRL1 0x1db9
#define VPP_SHARP_DEMO_WIN_CTRL2 0x1dba
#define VPP_FRONT_HLTI_CTRL 0x1dbb
#define VPP_FRONT_CTI_CTRL 0x1dbc
#define VPP_FRONT_CTI_CTRL2 0x1dbd
#define VPP_OSD_VSC_PHASE_STEP 0x1dc0
#define VPP_OSD_VSC_INI_PHASE 0x1dc1
#define VPP_OSD_VSC_CTRL0 0x1dc2
#define VPP_OSD_HSC_PHASE_STEP 0x1dc3
#define VPP_OSD_HSC_INI_PHASE 0x1dc4
#define VPP_OSD_HSC_CTRL0 0x1dc5
#define VPP_OSD_HSC_INI_PAT_CTRL 0x1dc6
#define VPP_OSD_SC_DUMMY_DATA 0x1dc7
#define VPP_OSD_SC_CTRL0 0x1dc8
#define VPP_OSD_SCI_WH_M1 0x1dc9
#define VPP_OSD_SCO_H_START_END 0x1dca
#define VPP_OSD_SCO_V_START_END 0x1dcb
#define VPP_OSD_SCALE_COEF_IDX 0x1dcc
#define VPP_OSD_SCALE_COEF 0x1dcd
#define VPP_INT_LINE_NUM 0x1dce
#define VPP_XVYCC_MISC 0x1dcf

#define VPP_CLIP_MISC0 0x1dd9

#define VPP_MATRIX_CLIP 0x1dde
#define VPP_CLIP_MISC1 0x1dda
#define VPP_MATRIX_COEF13_14 0x1ddb
#define VPP_MATRIX_COEF23_24 0x1ddc
#define VPP_MATRIX_COEF15_25 0x1ddd
#define VPP_MATRIX_CLIP 0x1dde
#define VPP_XVYCC_MISC0 0x1ddf
#define VPP_XVYCC_MISC1 0x1de0
#define VPP_VD1_CLIP_MISC0 0x1de1
#define VPP_VD1_CLIP_MISC1 0x1de2
#define VPP_VD2_CLIP_MISC0 0x1de3
#define VPP_VD2_CLIP_MISC1 0x1de4

/*txlx new add*/
#define VPP_DAT_CONV_PARA0 0x1d94
#define VPP_DAT_CONV_PARA1 0x1d95

#define VD1_IF0_GEN_REG3 0x1aa7
#define VD2_IF0_GEN_REG3 0x1aa8

/*g12a new add reg*/
#define VD1_AFBCD0_MISC_CTRL 0x1a0a
#define VD2_AFBCD1_MISC_CTRL 0x1a0b
#define DOLBY_PATH_CTRL 0x1a0c
#define WR_BACK_MISC_CTRL 0x1a0d

#define VD1_BLEND_SRC_CTRL 0x1dfb
#define VD2_BLEND_SRC_CTRL 0x1dfc
#define OSD1_BLEND_SRC_CTRL 0x1dfd
#define OSD2_BLEND_SRC_CTRL 0x1dfe

#define G12_VD1_IF0_GEN_REG3 0x3216
#define G12_VD2_IF0_GEN_REG3 0x3236

#define VPP2_MISC 0x1e26
#define VPP2_OFIFO_SIZE 0x1e27
#define VPP2_INT_LINE_NUM 0x1e20
#define VPP2_OFIFO_URG_CTRL 0x1e21


#define VIU_OSD1_BLK0_CFG_W0 0x1a1b
#define VIU_OSD1_MATRIX_CTRL 0x1a90
#define VIU_OSD1_MATRIX_COEF00_01 0x1a91
#define VIU_OSD1_MATRIX_COEF02_10 0x1a92
#define VIU_OSD1_MATRIX_COEF11_12 0x1a93
#define VIU_OSD1_MATRIX_COEF20_21 0x1a94
#define VIU_OSD1_MATRIX_COLMOD_COEF42 0x1a95
#define VIU_OSD1_MATRIX_OFFSET0_1 0x1a96
#define VIU_OSD1_MATRIX_OFFSET2 0x1a97
#define VIU_OSD1_MATRIX_PRE_OFFSET0_1 0x1a98
#define VIU_OSD1_MATRIX_PRE_OFFSET2 0x1a99
#define VIU_OSD1_MATRIX_COEF22_30 0x1a9d
#define VIU_OSD1_MATRIX_COEF31_32 0x1a9e
#define VIU_OSD1_MATRIX_COEF40_41 0x1a9f
#define VIU_OSD1_EOTF_CTL 0x1ad4
#define VIU_OSD1_EOTF_COEF00_01 0x1ad5
#define VIU_OSD1_EOTF_COEF02_10 0x1ad6
#define VIU_OSD1_EOTF_COEF11_12 0x1ad7
#define VIU_OSD1_EOTF_COEF20_21 0x1ad8
#define VIU_OSD1_EOTF_COEF22_RS 0x1ad9
#define VIU_OSD1_EOTF_LUT_ADDR_PORT 0x1ada
#define VIU_OSD1_EOTF_LUT_DATA_PORT 0x1adb
#define VIU_OSD1_OETF_CTL 0x1adc
#define VIU_OSD1_OETF_LUT_ADDR_PORT 0x1add
#define VIU_OSD1_OETF_LUT_DATA_PORT 0x1ade

#define VI_HIST_CTRL  0x2e00
#define VI_HIST_H_START_END  0x2e01
#define VI_HIST_V_START_END  0x2e02
#define VI_HIST_MAX_MIN  0x2e03
#define VI_HIST_SPL_VAL  0x2e04
#define VI_HIST_SPL_PIX_CNT  0x2e05
#define VI_HIST_CHROMA_SUM  0x2e06
#define VI_DNLP_HIST00  0x2e07
#define VI_DNLP_HIST01  0x2e08
#define VI_DNLP_HIST02  0x2e09
#define VI_DNLP_HIST03  0x2e0a
#define VI_DNLP_HIST04  0x2e0b
#define VI_DNLP_HIST05  0x2e0c
#define VI_DNLP_HIST06  0x2e0d
#define VI_DNLP_HIST07  0x2e0e
#define VI_DNLP_HIST08  0x2e0f
#define VI_DNLP_HIST09  0x2e10
#define VI_DNLP_HIST10  0x2e11
#define VI_DNLP_HIST11  0x2e12
#define VI_DNLP_HIST12  0x2e13
#define VI_DNLP_HIST13  0x2e14
#define VI_DNLP_HIST14  0x2e15
#define VI_DNLP_HIST15  0x2e16
#define VI_DNLP_HIST16  0x2e17
#define VI_DNLP_HIST17  0x2e18
#define VI_DNLP_HIST18  0x2e19
#define VI_DNLP_HIST19  0x2e1a
#define VI_DNLP_HIST20  0x2e1b
#define VI_DNLP_HIST21  0x2e1c
#define VI_DNLP_HIST22  0x2e1d
#define VI_DNLP_HIST23  0x2e1e
#define VI_DNLP_HIST24  0x2e1f
#define VI_DNLP_HIST25  0x2e20
#define VI_DNLP_HIST26  0x2e21
#define VI_DNLP_HIST27  0x2e22
#define VI_DNLP_HIST28  0x2e23
#define VI_DNLP_HIST29  0x2e24
#define VI_DNLP_HIST30  0x2e25
#define VI_DNLP_HIST31  0x2e26
#define VI_HIST_PIC_SIZE  0x2e28
#define VI_HIST_BLACK_WHITE_VALUE  0x2e29
#define VI_HIST_GCLK_CTRL  0x2e2a

#define VPP_IN_H_V_SIZE  0x1da6


/* 3D process */
#define VPU_VPU_3D_SYNC1			0x2738
#define VPU_VPU_3D_SYNC2			0x2739
#define VPU_VPU_PWM_V0				0x2730
#define VPU_VIU_VENC_MUX_CTRL       0x271a
#define VIU_MISC_CTRL0				0x1a06
#define VPU_VLOCK_CTRL				0x3000
#define VPU_VLOCK_MISC_CTRL			0x3001
#define VPU_VLOCK_LOOP0_ACCUM_LMT		0x3002
#define VPU_VLOCK_LOOP0_CTRL0			0x3003
#define VPU_VLOCK_LOOP1_CTRL0			0x3004
#define VPU_VLOCK_LOOP1_IMISSYNC_MAX	0x3005
#define VPU_VLOCK_LOOP1_IMISSYNC_MIN	0x3006
#define VPU_VLOCK_OVWRITE_ACCUM0		0x3007
#define VPU_VLOCK_OVWRITE_ACCUM1		0x3008
#define VPU_VLOCK_OUTPUT0_CAPT_LMT		0x3009
#define VPU_VLOCK_OUTPUT0_PLL_LMT		0x300a
#define VPU_VLOCK_OUTPUT1_CAPT_LMT		0x300b
#define VPU_VLOCK_OUTPUT1_PLL_LMT		0x300c
#define VPU_VLOCK_LOOP1_PHSDIF_TGT		0x300d
#define VPU_VLOCK_RO_LOOP0_ACCUM		0x300e
#define VPU_VLOCK_RO_LOOP1_ACCUM		0x300f
#define VPU_VLOCK_OROW_OCOL_MAX			0x3010
#define VPU_VLOCK_RO_VS_I_DIST			0x3011
#define VPU_VLOCK_RO_VS_O_DIST			0x3012
#define VPU_VLOCK_RO_LINE_PIX_ADJ		0x3013
#define VPU_VLOCK_RO_OUTPUT_00_01		0x3014
#define VPU_VLOCK_RO_OUTPUT_10_11		0x3015
#define VPU_VLOCK_MX4096				0x3016
#define VPU_VLOCK_STBDET_WIN0_WIN1		0x3017
#define VPU_VLOCK_STBDET_CLP			0x3018
#define VPU_VLOCK_STBDET_ABS_WIN0		0x3019
#define VPU_VLOCK_STBDET_ABS_WIN1		0x301a
#define VPU_VLOCK_STBDET_SGN_WIN0		0x301b
#define VPU_VLOCK_STBDET_SGN_WIN1		0x301c
#define VPU_VLOCK_ADJ_EN_SYNC_CTRL		0x301d
#define VPU_VLOCK_GCLK_EN				0x301e
#define VPU_VLOCK_LOOP1_ACCUM_LMT		0x301f
#define VPU_VLOCK_RO_M_INT_FRAC			0x3020
#define VPU_VLOCK_RO_LCK_FRM			0x3024

#define XVYCC_VD1_RGB_CTRST			0x3170

#define VIU_EOTF_CTL 0x31d0
#define VIU_EOTF_COEF00_01 0x31d1
#define VIU_EOTF_COEF02_10 0x31d2
#define VIU_EOTF_COEF11_12 0x31d3
#define VIU_EOTF_COEF20_21 0x31d4
#define VIU_EOTF_COEF22_RS 0x31d5
#define VIU_EOTF_LUT_ADDR_PORT  0x31d6
#define VIU_EOTF_LUT_DATA_PORT  0x31d7
#define VIU_EOTF_3X3_OFST_0     0x31d8
#define VIU_EOTF_3X3_OFST_1     0x31d9

/* sharpness */
#define SRSHARP0_PK_FINALGAIN_HP_BP 0x3222
#define SRSHARP0_PK_NR_ENABLE 0x3227
#define SRSHARP0_SHARP_DNLP_EN 0x3245

/*sr0 sr1 ybic cbic*/
#define SRSHARP0_SHARP_SR2_YBIC_HCOEF0 0x3258
#define SRSHARP0_SHARP_SR2_CBIC_HCOEF0 0x325a
#define SRSHARP0_SHARP_SR2_YBIC_VCOEF0 0x325c
#define SRSHARP0_SHARP_SR2_CBIC_VCOEF0 0x325e

/*sr0 sr1 lti cti*/
#define SRSHARP0_HCTI_FLT_CLP_DC     0x322e/*bit28*/
#define SRSHARP0_HLTI_FLT_CLP_DC     0x3234
#define SRSHARP0_VLTI_FLT_CON_CLP     0x323a/*bit14*/
#define SRSHARP0_VCTI_FLT_CON_CLP     0x323f

/*sr0 sr1 dejaggy/direction/dering*/
#define SRSHARP0_DEJ_CTRL				0x3264/*bit 0*/
#define SRSHARP0_SR3_DRTLPF_EN			0x3266/*bit 0-2*/
#define SRSHARP0_SR3_DERING_CTRL		0x326b/*bit 28-30*/

/*sr4 add*/
#define SRSHARP0_SR3_DRTLPF_THETA             0x3273
#define SRSHARP0_SATPRT_CTRL                  0x3274
#define SRSHARP0_SATPRT_DIVM                  0x3275
#define SRSHARP0_SATPRT_LMT_RGB               0x3276
#define SRSHARP0_DB_FLT_CTRL                  0x3277
#define SRSHARP0_DB_FLT_YC_THRD               0x3278
#define SRSHARP0_DB_FLT_RANDLUT               0x3279
#define SRSHARP0_DB_FLT_PXI_THRD              0x327a
#define SRSHARP0_DB_FLT_SEED_Y                0x327b
#define SRSHARP0_DB_FLT_SEED_U                0x327c
#define SRSHARP0_DB_FLT_SEED_V                0x327d
#define SRSHARP0_PKGAIN_VSLUMA_LUT_L          0x327e
#define SRSHARP0_PKGAIN_VSLUMA_LUT_H          0x327f
#define SRSHARP0_PKOSHT_VSLUMA_LUT_L          0x3203
#define SRSHARP0_PKOSHT_VSLUMA_LUT_H          0x3204


/*sharpness reg*/
#define SRSHARP1_SHARP_HVSIZE                 0x3280
#define SRSHARP1_SHARP_HVBLANK_NUM            0x3281
#define SRSHARP1_NR_GAUSSIAN_MODE             0x3282
#define SRSHARP1_PK_CON_2CIRHPGAIN_TH_RATE    0x3285
#define SRSHARP1_PK_CON_2CIRHPGAIN_LIMIT      0x3286
#define SRSHARP1_PK_CON_2CIRBPGAIN_TH_RATE    0x3287
#define SRSHARP1_PK_CON_2CIRBPGAIN_LIMIT      0x3288
#define SRSHARP1_PK_CON_2DRTHPGAIN_TH_RATE    0x3289
#define SRSHARP1_PK_CON_2DRTHPGAIN_LIMIT      0x328a
#define SRSHARP1_PK_CON_2DRTBPGAIN_TH_RATE    0x328b
#define SRSHARP1_PK_CON_2DRTBPGAIN_LIMIT      0x328c
#define SRSHARP1_PK_CIRFB_LPF_MODE            0x328d
#define SRSHARP1_PK_DRTFB_LPF_MODE            0x328e
#define SRSHARP1_PK_CIRFB_HP_CORING           0x328f
#define SRSHARP1_PK_CIRFB_BP_CORING           0x3290
#define SRSHARP1_PK_DRTFB_HP_CORING           0x3291
#define SRSHARP1_PK_DRTFB_BP_CORING           0x3292
#define SRSHARP1_PK_CIRFB_BLEND_GAIN          0x3293
#define SRSHARP1_NR_ALPY_SSD_GAIN_OFST        0x3294
#define SRSHARP1_NR_ALP0Y_ERR2CURV_TH_RATE    0x3295
#define SRSHARP1_NR_ALP0Y_ERR2CURV_LIMIT      0x3296
#define SRSHARP1_NR_ALP0C_ERR2CURV_TH_RATE    0x3297
#define SRSHARP1_NR_ALP0C_ERR2CURV_LIMIT      0x3298
#define SRSHARP1_NR_ALP0_MIN_MAX              0x3299
#define SRSHARP1_NR_ALP1_MIERR_CORING         0x329a
#define SRSHARP1_NR_ALP1_ERR2CURV_TH_RATE     0x329b
#define SRSHARP1_NR_ALP1_ERR2CURV_LIMIT       0x329c
#define SRSHARP1_NR_ALP1_MIN_MAX              0x329d
#define SRSHARP1_PK_ALP2_MIERR_CORING         0x329e
#define SRSHARP1_PK_ALP2_ERR2CURV_TH_RATE     0x329f
#define SRSHARP1_PK_ALP2_ERR2CURV_LIMIT       0x32a0
#define SRSHARP1_PK_ALP2_MIN_MAX              0x32a1
#define SRSHARP1_PK_FINALGAIN_HP_BP           0x32a2
#define SRSHARP1_PK_OS_HORZ_CORE_GAIN         0x32a3
#define SRSHARP1_PK_OS_VERT_CORE_GAIN         0x32a4
#define SRSHARP1_PK_OS_ADPT_MISC              0x32a5
#define SRSHARP1_PK_OS_STATIC                 0x32a6
#define SRSHARP1_PK_NR_ENABLE                 0x32a7
#define SRSHARP1_PK_DRT_SAD_MISC              0x32a8
#define SRSHARP1_NR_TI_DNLP_BLEND             0x32a9
#define SRSHARP1_TI_DIR_CORE_ALPHA            0x32aa
#define SRSHARP1_CTI_DIR_ALPHA                0x32ab
#define SRSHARP1_LTI_CTI_DF_GAIN              0x32ac
#define SRSHARP1_LTI_CTI_DIR_AC_DBG           0x32ad
#define SRSHARP1_HCTI_FLT_CLP_DC              0x32ae
#define SRSHARP1_HCTI_BST_GAIN                0x32af
#define SRSHARP1_HCTI_BST_CORE                0x32b0
#define SRSHARP1_HCTI_CON_2_GAIN_0            0x32b1
#define SRSHARP1_HCTI_CON_2_GAIN_1            0x32b2
#define SRSHARP1_HCTI_OS_MARGIN               0x32b3
#define SRSHARP1_HLTI_FLT_CLP_DC              0x32b4
#define SRSHARP1_HLTI_BST_GAIN                0x32b5
#define SRSHARP1_HLTI_BST_CORE                0x32b6
#define SRSHARP1_HLTI_CON_2_GAIN_0            0x32b7
#define SRSHARP1_HLTI_CON_2_GAIN_1            0x32b8
#define SRSHARP1_HLTI_OS_MARGIN               0x32b9
#define SRSHARP1_VLTI_FLT_CON_CLP             0x32ba
#define SRSHARP1_VLTI_BST_GAIN                0x32bb
#define SRSHARP1_VLTI_BST_CORE                0x32bc
#define SRSHARP1_VLTI_CON_2_GAIN_0            0x32bd
#define SRSHARP1_VLTI_CON_2_GAIN_1            0x32be
#define SRSHARP1_VCTI_FLT_CON_CLP             0x32bf
#define SRSHARP1_VCTI_BST_GAIN                0x32c0
#define SRSHARP1_VCTI_BST_CORE                0x32c1
#define SRSHARP1_VCTI_CON_2_GAIN_0            0x32c2
#define SRSHARP1_VCTI_CON_2_GAIN_1            0x32c3
#define SRSHARP1_SHARP_3DLIMIT                0x32c4
#define SRSHARP1_DNLP_EN                      0x32c5
#define SRSHARP1_DNLP_00                      0x32c6
#define SRSHARP1_DNLP_01                      0x32c7
#define SRSHARP1_DNLP_02                      0x32c8
#define SRSHARP1_DNLP_03                      0x32c9
#define SRSHARP1_DNLP_04                      0x32ca
#define SRSHARP1_DNLP_05                      0x32cb
#define SRSHARP1_DNLP_06                      0x32cc
#define SRSHARP1_DNLP_07                      0x32cd
#define SRSHARP1_DNLP_08                      0x32ce
#define SRSHARP1_DNLP_09                      0x32cf
#define SRSHARP1_DNLP_10                      0x32d0
#define SRSHARP1_DNLP_11                      0x32d1
#define SRSHARP1_DNLP_12                      0x32d2
#define SRSHARP1_DNLP_13                      0x32d3
#define SRSHARP1_DNLP_14                      0x32d4
#define SRSHARP1_DNLP_15                      0x32d5
#define SRSHARP1_DEMO_CRTL                    0x32d6
#define SRSHARP1_SHARP_SR2_CTRL               0x32d7
#define SRSHARP1_SHARP_SR2_YBIC_HCOEF0        0x32d8
#define SRSHARP1_SHARP_SR2_YBIC_HCOEF1        0x32d9
#define SRSHARP1_SHARP_SR2_CBIC_HCOEF0        0x32da
#define SRSHARP1_SHARP_SR2_CBIC_HCOEF1        0x32db
#define SRSHARP1_SHARP_SR2_YBIC_VCOEF0        0x32dc
#define SRSHARP1_SHARP_SR2_YBIC_VCOEF1        0x32dd
#define SRSHARP1_SHARP_SR2_CBIC_VCOEF0        0x32de
#define SRSHARP1_SHARP_SR2_CBIC_VCOEF1        0x32df
#define SRSHARP1_SHARP_SR2_MISC               0x32e0
#define SRSHARP1_SR3_SAD_CTRL                 0x32e1
#define SRSHARP1_SR3_PK_CTRL0                 0x32e2
#define SRSHARP1_SR3_PK_CTRL1                 0x32e3
#define SRSHARP1_DEJ_CTRL                     0x32e4
#define SRSHARP1_DEJ_ALPHA                    0x32e5
#define SRSHARP1_SR3_DRTLPF_EN                0x32e6
#define SRSHARP1_SR3_DRTLPF_ALPHA_0           0x32e7
#define SRSHARP1_SR3_DRTLPF_ALPHA_1           0x32e8
#define SRSHARP1_SR3_DRTLPF_ALPHA_2           0x32e9
#define SRSHARP1_SR3_DRTLPF_ALPHA_OFST        0x32ea
#define SRSHARP1_SR3_DERING_CTRL              0x32eb
#define SRSHARP1_SR3_DERING_LUMA2PKGAIN_0TO3  0x32ec
#define SRSHARP1_SR3_DERING_LUMA2PKGAIN_4TO6  0x32ed
#define SRSHARP1_SR3_DERING_LUMA2PKOS_0TO3    0x32ee
#define SRSHARP1_SR3_DERING_LUMA2PKOS_4TO6    0x32ef
#define SRSHARP1_SR3_DERING_GAINVS_MADSAD     0x32f0
#define SRSHARP1_SR3_DERING_GAINVS_VR2MAX     0x32f1
#define SRSHARP1_SR3_DERING_PARAM0            0x32f2
#define SRSHARP1_SR3_DRTLPF_THETA             0x32f3
#define SRSHARP1_SATPRT_CTRL                  0x32f4
#define SRSHARP1_SATPRT_DIVM                  0x32f5
#define SRSHARP1_SATPRT_LMT_RGB               0x32f6
#define SRSHARP1_DB_FLT_CTRL                  0x32f7
#define SRSHARP1_DB_FLT_YC_THRD               0x32f8
#define SRSHARP1_DB_FLT_RANDLUT               0x32f9
#define SRSHARP1_DB_FLT_PXI_THRD              0x32fa
#define SRSHARP1_DB_FLT_SEED_Y                0x32fb
#define SRSHARP1_DB_FLT_SEED_U                0x32fc
#define SRSHARP1_DB_FLT_SEED_V                0x32fd
#define SRSHARP1_PKGAIN_VSLUMA_LUT_L          0x32fe
#define SRSHARP1_PKGAIN_VSLUMA_LUT_H          0x32ff
#define SRSHARP1_PKOSHT_VSLUMA_LUT_L          0x3283
#define SRSHARP1_PKOSHT_VSLUMA_LUT_H          0x3284

/*ve dither*/
#define VPP_VE_DITHER_CTRL		0x3120

/* TL1 */
/*offset 0x1000*/
#define HHI_TCON_PLL_CNTL0                         0x20
#define HHI_TCON_PLL_CNTL1                         0x21
#define HHI_HDMI_PLL_VLOCK_CNTL                         0xd1

/* for pll bug */

#define HHI_HDMI_PLL_CNTL			    0xc8
#define HHI_HDMI_PLL_CNTL2			    0xc9
#define HHI_VID_LOCK_CLK_CNTL			0xf2
#define HHI_HDMI_PLL_CNTL6			    0xcd

/* for vlock enc mode adjust begin */
#define ENCL_VIDEO_MAX_LNCNT            0x1cbb
#define ENCL_VIDEO_MAX_PXCNT 0x1cb0
#define ENCL_MAX_LINE_SWITCH_POINT		0x1cc8
#define ENCL_VIDEO_MODE 0x1ca7
#define ENCL_VIDEO_MODE_ADV 0x1ca8

#define ENCP_VIDEO_MAX_LNCNT 0x1bae
#define ENCP_VIDEO_MAX_PXCNT 0x1b97
#define ENCP_VIDEO_MODE 0x1b8d
#define ENCP_MAX_LINE_SWITCH_POINT 0x1c0f

#define ENCT_VIDEO_MAX_LNCNT 0x1c7b
#define ENCT_VIDEO_MAX_PXCNT 0x1c70
#define ENCT_VIDEO_MODE 0x1c67
#define ENCT_MAX_LINE_SWITCH_POINT 0x1c88
/* for vlock enc mode adjust end */

#define VDIN_MEAS_VS_COUNT_LO 0x125c
/*for vlock*/
/*after GXL new add CNTL1,same with CNTL2 on G9TV/GXTVBB*/
#define HHI_HDMI_PLL_CNTL1			    0xc9
/*after GXL CNTL5[bit3] is same with CNTL6[bit20] on G9TV/GXTVBB*/
#define HHI_HDMI_PLL_CNTL5			    0xcd


/* #define VI_HIST_CTRL                             0x2e00 */
/* the total pixels = VDIN_HISTXX*(2^(VDIN_HIST_POW+3)) */
#define VI_HIST_POW_BIT                    5
#define VI_HIST_POW_WID                    3
/* Histgram range: 0: full picture, 1: histgram window*/
/* defined by VDIN_HIST_H_START_END & VDIN_HIST_V_START_END */
#define VI_HIST_WIN_EN_BIT                 1
#define VI_HIST_WIN_EN_WID                 1
/* Histgram readback: 0: disable, 1: enable */
#define VI_HIST_RD_EN_BIT                  0
#define VI_HIST_RD_EN_WID                  1

/* #define VDIN_HIST_H_START_END                   0x1231 */
#define VI_HIST_HSTART_BIT                 16
#define VI_HIST_HSTART_WID                 13
#define VI_HIST_HEND_BIT                   0
#define VI_HIST_HEND_WID                   13

/* #define VDIN_HIST_V_START_END                   0x1232 */
#define VI_HIST_VSTART_BIT                 16
#define VI_HIST_VSTART_WID                 13
#define VI_HIST_VEND_BIT                   0
#define VI_HIST_VEND_WID                   13

/* #define VDIN_HIST_MAX_MIN                       0x1233 */
#define VI_HIST_MAX_BIT                    8
#define VI_HIST_MAX_WID                    8
#define VI_HIST_MIN_BIT                    0
#define VI_HIST_MIN_WID                    8

/* #define VDIN_HIST_SPL_VAL                       0x1234 */
#define VI_HIST_LUMA_SUM_BIT               0
#define VI_HIST_LUMA_SUM_WID               32

 /* the total calculated pixels */
/* #define VDIN_HIST_SPL_PIX_CNT                   0x1235 */
#define VI_HIST_PIX_CNT_BIT                0
#define VI_HIST_PIX_CNT_WID                22

/* the total chroma value */
/* #define VDIN_HIST_CHROMA_SUM                    0x1236 */
#define VI_HIST_CHROMA_SUM_BIT             0
#define VI_HIST_CHROMA_SUM_WID             32

/* #define VDIN_DNLP_HIST00                        0x1237 */
#define VI_HIST_ON_BIN_01_BIT              16
#define VI_HIST_ON_BIN_01_WID              16
#define VI_HIST_ON_BIN_00_BIT              0
#define VI_HIST_ON_BIN_00_WID              16

/* #define VDIN_DNLP_HIST01                        0x1238 */
#define VI_HIST_ON_BIN_03_BIT              16
#define VI_HIST_ON_BIN_03_WID              16
#define VI_HIST_ON_BIN_02_BIT              0
#define VI_HIST_ON_BIN_02_WID              16

/* #define VDIN_DNLP_HIST02                        0x1239 */
#define VI_HIST_ON_BIN_05_BIT              16
#define VI_HIST_ON_BIN_05_WID              16
#define VI_HIST_ON_BIN_04_BIT              0
#define VI_HIST_ON_BIN_04_WID              16

/* #define VDIN_DNLP_HIST03                        0x123a */
#define VI_HIST_ON_BIN_07_BIT              16
#define VI_HIST_ON_BIN_07_WID              16
#define VI_HIST_ON_BIN_06_BIT              0
#define VI_HIST_ON_BIN_06_WID              16

/* #define VDIN_DNLP_HIST04                        0x123b */
#define VI_HIST_ON_BIN_09_BIT              16
#define VI_HIST_ON_BIN_09_WID              16
#define VI_HIST_ON_BIN_08_BIT              0
#define VI_HIST_ON_BIN_08_WID              16

/* #define VDIN_DNLP_HIST05                        0x123c */
#define VI_HIST_ON_BIN_11_BIT              16
#define VI_HIST_ON_BIN_11_WID              16
#define VI_HIST_ON_BIN_10_BIT              0
#define VI_HIST_ON_BIN_10_WID              16

/* #define VDIN_DNLP_HIST06                        0x123d */
#define VI_HIST_ON_BIN_13_BIT              16
#define VI_HIST_ON_BIN_13_WID              16
#define VI_HIST_ON_BIN_12_BIT              0
#define VI_HIST_ON_BIN_12_WID              16

/* #define VDIN_DNLP_HIST07                        0x123e */
#define VI_HIST_ON_BIN_15_BIT              16
#define VI_HIST_ON_BIN_15_WID              16
#define VI_HIST_ON_BIN_14_BIT              0
#define VI_HIST_ON_BIN_14_WID              16

/* #define VDIN_DNLP_HIST08                        0x123f */
#define VI_HIST_ON_BIN_17_BIT              16
#define VI_HIST_ON_BIN_17_WID              16
#define VI_HIST_ON_BIN_16_BIT              0
#define VI_HIST_ON_BIN_16_WID              16

/* #define VDIN_DNLP_HIST09                        0x1240 */
#define VI_HIST_ON_BIN_19_BIT              16
#define VI_HIST_ON_BIN_19_WID              16
#define VI_HIST_ON_BIN_18_BIT              0
#define VI_HIST_ON_BIN_18_WID              16

/* #define VDIN_DNLP_HIST10                        0x1241 */
#define VI_HIST_ON_BIN_21_BIT              16
#define VI_HIST_ON_BIN_21_WID              16
#define VI_HIST_ON_BIN_20_BIT              0
#define VI_HIST_ON_BIN_20_WID              16

/* #define VDIN_DNLP_HIST11                        0x1242 */
#define VI_HIST_ON_BIN_23_BIT              16
#define VI_HIST_ON_BIN_23_WID              16
#define VI_HIST_ON_BIN_22_BIT              0
#define VI_HIST_ON_BIN_22_WID              16

/* #define VDIN_DNLP_HIST12                        0x1243 */
#define VI_HIST_ON_BIN_25_BIT              16
#define VI_HIST_ON_BIN_25_WID              16
#define VI_HIST_ON_BIN_24_BIT              0
#define VI_HIST_ON_BIN_24_WID              16

/* #define VDIN_DNLP_HIST13                        0x1244 */
#define VI_HIST_ON_BIN_27_BIT              16
#define VI_HIST_ON_BIN_27_WID              16
#define VI_HIST_ON_BIN_26_BIT              0
#define VI_HIST_ON_BIN_26_WID              16

/* #define VDIN_DNLP_HIST14                        0x1245 */
#define VI_HIST_ON_BIN_29_BIT              16
#define VI_HIST_ON_BIN_29_WID              16
#define VI_HIST_ON_BIN_28_BIT              0
#define VI_HIST_ON_BIN_28_WID              16

/* #define VDIN_DNLP_HIST15                        0x1246 */
#define VI_HIST_ON_BIN_31_BIT              16
#define VI_HIST_ON_BIN_31_WID              16
#define VI_HIST_ON_BIN_30_BIT              0
#define VI_HIST_ON_BIN_30_WID              16

/* #define VDIN_DNLP_HIST16                        0x1247 */
#define VI_HIST_ON_BIN_33_BIT              16
#define VI_HIST_ON_BIN_33_WID              16
#define VI_HIST_ON_BIN_32_BIT              0
#define VI_HIST_ON_BIN_32_WID              16

/* #define VDIN_DNLP_HIST17                        0x1248 */
#define VI_HIST_ON_BIN_35_BIT              16
#define VI_HIST_ON_BIN_35_WID              16
#define VI_HIST_ON_BIN_34_BIT              0
#define VI_HIST_ON_BIN_34_WID              16

/* #define VDIN_DNLP_HIST18                        0x1249 */
#define VI_HIST_ON_BIN_37_BIT              16
#define VI_HIST_ON_BIN_37_WID              16
#define VI_HIST_ON_BIN_36_BIT              0
#define VI_HIST_ON_BIN_36_WID              16

/* #define VDIN_DNLP_HIST19                        0x124a */
#define VI_HIST_ON_BIN_39_BIT              16
#define VI_HIST_ON_BIN_39_WID              16
#define VI_HIST_ON_BIN_38_BIT              0
#define VI_HIST_ON_BIN_38_WID              16

/* #define VDIN_DNLP_HIST20                        0x124b */
#define VI_HIST_ON_BIN_41_BIT              16
#define VI_HIST_ON_BIN_41_WID              16
#define VI_HIST_ON_BIN_40_BIT              0
#define VI_HIST_ON_BIN_40_WID              16

/* #define VDIN_DNLP_HIST21                        0x124c */
#define VI_HIST_ON_BIN_43_BIT              16
#define VI_HIST_ON_BIN_43_WID              16
#define VI_HIST_ON_BIN_42_BIT              0
#define VI_HIST_ON_BIN_42_WID              16

/* #define VDIN_DNLP_HIST22                        0x124d */
#define VI_HIST_ON_BIN_45_BIT              16
#define VI_HIST_ON_BIN_45_WID              16
#define VI_HIST_ON_BIN_44_BIT              0
#define VI_HIST_ON_BIN_44_WID              16

/* #define VDIN_DNLP_HIST23                        0x124e */
#define VI_HIST_ON_BIN_47_BIT              16
#define VI_HIST_ON_BIN_47_WID              16
#define VI_HIST_ON_BIN_46_BIT              0
#define VI_HIST_ON_BIN_46_WID              16

/* #define VDIN_DNLP_HIST24                        0x124f */
#define VI_HIST_ON_BIN_49_BIT              16
#define VI_HIST_ON_BIN_49_WID              16
#define VI_HIST_ON_BIN_48_BIT              0
#define VI_HIST_ON_BIN_48_WID              16

/* #define VDIN_DNLP_HIST25                        0x1250 */
#define VI_HIST_ON_BIN_51_BIT              16
#define VI_HIST_ON_BIN_51_WID              16
#define VI_HIST_ON_BIN_50_BIT              0
#define VI_HIST_ON_BIN_50_WID              16

/* #define VDIN_DNLP_HIST26                        0x1251 */
#define VI_HIST_ON_BIN_53_BIT              16
#define VI_HIST_ON_BIN_53_WID              16
#define VI_HIST_ON_BIN_52_BIT              0
#define VI_HIST_ON_BIN_52_WID              16

/* #define VDIN_DNLP_HIST27                        0x1252 */
#define VI_HIST_ON_BIN_55_BIT              16
#define VI_HIST_ON_BIN_55_WID              16
#define VI_HIST_ON_BIN_54_BIT              0
#define VI_HIST_ON_BIN_54_WID              16

/* #define VDIN_DNLP_HIST28                        0x1253 */
#define VI_HIST_ON_BIN_57_BIT              16
#define VI_HIST_ON_BIN_57_WID              16
#define VI_HIST_ON_BIN_56_BIT              0
#define VI_HIST_ON_BIN_56_WID              16

/* #define VDIN_DNLP_HIST29                        0x1254 */
#define VI_HIST_ON_BIN_59_BIT              16
#define VI_HIST_ON_BIN_59_WID              16
#define VI_HIST_ON_BIN_58_BIT              0
#define VI_HIST_ON_BIN_58_WID              16

/* #define VDIN_DNLP_HIST30                        0x1255 */
#define VI_HIST_ON_BIN_61_BIT              16
#define VI_HIST_ON_BIN_61_WID              16
#define VI_HIST_ON_BIN_60_BIT              0
#define VI_HIST_ON_BIN_60_WID              16

/* #define VDIN_DNLP_HIST31                        0x1256 */
#define VI_HIST_ON_BIN_63_BIT              16
#define VI_HIST_ON_BIN_63_WID              16
#define VI_HIST_ON_BIN_62_BIT              0
#define VI_HIST_ON_BIN_62_WID              16

/* #define VDIN_HIST_PIC_SIZE                   0x2e28 */
#define VI_HIST_PIC_HEIGHT_BIT                 16
#define VI_HIST_PIC_HEIGHT_WID                 13
#define VI_HIST_PIC_WIDTH_BIT                   0
#define VI_HIST_PIC_WIDTH_WID                   13

/*G12A Matrix reg*/
#define VPP_VD1_MATRIX_COEF00_01	    0x3290
#define VPP_VD1_MATRIX_COEF02_10	    0x3291
#define VPP_VD1_MATRIX_COEF11_12	    0x3292
#define VPP_VD1_MATRIX_COEF20_21	    0x3293
#define VPP_VD1_MATRIX_COEF22	        0x3294
#define VPP_VD1_MATRIX_COEF13_14	    0x3295
#define VPP_VD1_MATRIX_COEF23_24	    0x3296
#define VPP_VD1_MATRIX_COEF15_25	    0x3297
#define VPP_VD1_MATRIX_CLIP	            0x3298
#define VPP_VD1_MATRIX_OFFSET0_1	    0x3299
#define VPP_VD1_MATRIX_OFFSET2	        0x329a
#define VPP_VD1_MATRIX_PRE_OFFSET0_1	0x329b
#define VPP_VD1_MATRIX_PRE_OFFSET2	    0x329c
#define VPP_VD1_MATRIX_EN_CTRL	        0x329d

#define VPP_POST_MATRIX_COEF00_01	    0x32b0
#define VPP_POST_MATRIX_COEF02_10	    0x32b1
#define VPP_POST_MATRIX_COEF11_12	    0x32b2
#define VPP_POST_MATRIX_COEF20_21	    0x32b3
#define VPP_POST_MATRIX_COEF22	        0x32b4
#define VPP_POST_MATRIX_COEF13_14	    0x32b5
#define VPP_POST_MATRIX_COEF23_24	    0x32b6
#define VPP_POST_MATRIX_COEF15_25	    0x32b7
#define VPP_POST_MATRIX_CLIP	        0x32b8
#define VPP_POST_MATRIX_OFFSET0_1	    0x32b9
#define VPP_POST_MATRIX_OFFSET2	        0x32ba
#define VPP_POST_MATRIX_PRE_OFFSET0_1	0x32bb
#define VPP_POST_MATRIX_PRE_OFFSET2	    0x32bc
#define VPP_POST_MATRIX_EN_CTRL	        0x32bd

#define VPP_POST_MATRIX_SAT				0x32c1

#define VPP_POST2_MATRIX_COEF00_01	    0x39a0
#define VPP_POST2_MATRIX_COEF02_10	    0x39a1
#define VPP_POST2_MATRIX_COEF11_12	    0x39a2
#define VPP_POST2_MATRIX_COEF20_21	    0x39a3
#define VPP_POST2_MATRIX_COEF22	        0x39a4
#define VPP_POST2_MATRIX_COEF13_14	    0x39a5
#define VPP_POST2_MATRIX_COEF23_24	    0x39a6
#define VPP_POST2_MATRIX_COEF15_25	    0x39a7
#define VPP_POST2_MATRIX_CLIP	        0x39a8
#define VPP_POST2_MATRIX_OFFSET0_1	    0x39a9
#define VPP_POST2_MATRIX_OFFSET2	    0x39aa
#define VPP_POST2_MATRIX_PRE_OFFSET0_1	0x39ab
#define VPP_POST2_MATRIX_PRE_OFFSET2	0x39ac
#define VPP_POST2_MATRIX_EN_CTRL	    0x39ad

#define VPP_LUT3D_CTRL					0x39d0
#define VPP_LUT3D_CBUS2RAM_CTRL			0x39d1
#define VPP_LUT3D_RAM_ADDR				0x39d2
#define VPP_LUT3D_RAM_DATA				0x39d3

#define ENCL_VIDEO_EN		0x1ca0

/*TL1 VADJ1&VADJ2*/
#define VPP_VADJ1_MISC 0x3280
#define VPP_VADJ1_BLACK_VAL 0x3281
#define VPP_VADJ1_Y_2 0x3282
#define VPP_VADJ1_MA_MB_2 0x3283
#define VPP_VADJ1_MC_MD_2 0x3284

#define VPP_VADJ2_MISC 0x32a0
#define VPP_VADJ2_BLACK_VAL 0x32a1
#define VPP_VADJ2_Y_2 0x32a2
#define VPP_VADJ2_MA_MB_2 0x32a3
#define VPP_VADJ2_MC_MD_2 0x32a4

/*TL1 add cm hist reg*/
#define XVYCC_YSCP_REG 0x21c
#define XVYCC_USCP_REG 0x21d
#define XVYCC_VSCP_REG 0x21e
#define LUMA_ADJ0_REG	0x21f
#define LUMA_ADJ1_REG	0x220
#define STA_WIN_XYXY0_REG	0x221
#define STA_WIN_XYXY1_REG	0x222
#define STA_CFG_REG	0x223
#define STA_SAT_HIST0_REG	0x224
#define STA_SAT_HIST1_REG	0x225

#define RO_CM_HUE_HIST_BIN0	0x226
#define RO_CM_HUE_HIST_BIN1	0x227
#define RO_CM_HUE_HIST_BIN2	0x228
#define RO_CM_HUE_HIST_BIN3	0x229
#define RO_CM_HUE_HIST_BIN4	0x22a
#define RO_CM_HUE_HIST_BIN5	0x22b
#define RO_CM_HUE_HIST_BIN6	0x22c
#define RO_CM_HUE_HIST_BIN7	0x22d
#define RO_CM_HUE_HIST_BIN8	0x22e
#define RO_CM_HUE_HIST_BIN9	0x22f
#define RO_CM_HUE_HIST_BIN10	0x230
#define RO_CM_HUE_HIST_BIN11	0x231
#define RO_CM_HUE_HIST_BIN12	0x232
#define RO_CM_HUE_HIST_BIN13	0x233
#define RO_CM_HUE_HIST_BIN14	0x234
#define RO_CM_HUE_HIST_BIN15	0x235
#define RO_CM_HUE_HIST_BIN16	0x236
#define RO_CM_HUE_HIST_BIN17	0x237
#define RO_CM_HUE_HIST_BIN18	0x238
#define RO_CM_HUE_HIST_BIN19	0x239
#define RO_CM_HUE_HIST_BIN20	0x23a
#define RO_CM_HUE_HIST_BIN21	0x23b
#define RO_CM_HUE_HIST_BIN22	0x23c
#define RO_CM_HUE_HIST_BIN23	0x23d
#define RO_CM_HUE_HIST_BIN24	0x23e
#define RO_CM_HUE_HIST_BIN25	0x23f
#define RO_CM_HUE_HIST_BIN26	0x240
#define RO_CM_HUE_HIST_BIN27	0x241
#define RO_CM_HUE_HIST_BIN28	0x242
#define RO_CM_HUE_HIST_BIN29	0x243
#define RO_CM_HUE_HIST_BIN30	0x244
#define RO_CM_HUE_HIST_BIN31	0x245

#define RO_CM_SAT_HIST_BIN0	0x246
#define RO_CM_SAT_HIST_BIN1	0x247
#define RO_CM_SAT_HIST_BIN2	0x248
#define RO_CM_SAT_HIST_BIN3	0x249
#define RO_CM_SAT_HIST_BIN4	0x24a
#define RO_CM_SAT_HIST_BIN5	0x24b
#define RO_CM_SAT_HIST_BIN6	0x24c
#define RO_CM_SAT_HIST_BIN7	0x24d
#define RO_CM_SAT_HIST_BIN8	0x24e
#define RO_CM_SAT_HIST_BIN9	0x24f
#define RO_CM_SAT_HIST_BIN10	0x250
#define RO_CM_SAT_HIST_BIN11	0x251
#define RO_CM_SAT_HIST_BIN12	0x252
#define RO_CM_SAT_HIST_BIN13	0x253
#define RO_CM_SAT_HIST_BIN14	0x254
#define RO_CM_SAT_HIST_BIN15	0x255
#define RO_CM_SAT_HIST_BIN16	0x256
#define RO_CM_SAT_HIST_BIN17	0x257
#define RO_CM_SAT_HIST_BIN18	0x258
#define RO_CM_SAT_HIST_BIN19	0x259
#define RO_CM_SAT_HIST_BIN20	0x25a
#define RO_CM_SAT_HIST_BIN21	0x25b
#define RO_CM_SAT_HIST_BIN22	0x25c
#define RO_CM_SAT_HIST_BIN23	0x25d
#define RO_CM_SAT_HIST_BIN24	0x25e
#define RO_CM_SAT_HIST_BIN25	0x25f
#define RO_CM_SAT_HIST_BIN26	0x260
#define RO_CM_SAT_HIST_BIN27	0x261
#define RO_CM_SAT_HIST_BIN28	0x262
#define RO_CM_SAT_HIST_BIN29	0x263
#define RO_CM_SAT_HIST_BIN30	0x264
#define RO_CM_SAT_HIST_BIN31	0x265

#define RO_CM_BLK_BIN	0x266
#define RO_CM_BRT_BIN	0x267
/*TL1 cm HIST reg end*/

/*TL1 SHARPNESS REG*/
#define SHARP0_SHARP_HVSIZE     0x3e00
#define SHARP0_SHARP_HVBLANK_NUM     0x3e01
#define SHARP0_NR_GAUSSIAN_MODE     0x3e02
#define SHARP0_PK_CON_2CIRHPGAIN_TH_RATE     0x3e05
#define SHARP0_PK_CON_2CIRHPGAIN_LIMIT     0x3e06
#define SHARP0_PK_CON_2CIRBPGAIN_TH_RATE     0x3e07
#define SHARP0_PK_CON_2CIRBPGAIN_LIMIT     0x3e08
#define SHARP0_PK_CON_2DRTHPGAIN_TH_RATE     0x3e09
#define SHARP0_PK_CON_2DRTHPGAIN_LIMIT     0x3e0a
#define SHARP0_PK_CON_2DRTBPGAIN_TH_RATE     0x3e0b
#define SHARP0_PK_CON_2DRTBPGAIN_LIMIT     0x3e0c
#define SHARP0_PK_CIRFB_LPF_MODE     0x3e0d
#define SHARP0_PK_DRTFB_LPF_MODE     0x3e0e
#define SHARP0_PK_CIRFB_HP_CORING     0x3e0f
#define SHARP0_PK_CIRFB_BP_CORING     0x3e10
#define SHARP0_PK_DRTFB_HP_CORING     0x3e11
#define SHARP0_PK_DRTFB_BP_CORING     0x3e12
#define SHARP0_PK_CIRFB_BLEND_GAIN     0x3e13
#define SHARP0_NR_ALPY_SSD_GAIN_OFST     0x3e14
#define SHARP0_NR_ALP0Y_ERR2CURV_TH_RATE     0x3e15
#define SHARP0_NR_ALP0Y_ERR2CURV_LIMIT     0x3e16
#define SHARP0_NR_ALP0C_ERR2CURV_TH_RATE     0x3e17
#define SHARP0_NR_ALP0C_ERR2CURV_LIMIT     0x3e18
#define SHARP0_NR_ALP0_MIN_MAX     0x3e19
#define SHARP0_NR_ALP1_MIERR_CORING     0x3e1a
#define SHARP0_NR_ALP1_ERR2CURV_TH_RATE     0x3e1b
#define SHARP0_NR_ALP1_ERR2CURV_LIMIT     0x3e1c
#define SHARP0_NR_ALP1_MIN_MAX     0x3e1d
#define SHARP0_PK_ALP2_MIERR_CORING     0x3e1e
#define SHARP0_PK_ALP2_ERR2CURV_TH_RATE     0x3e1f
#define SHARP0_PK_ALP2_ERR2CURV_LIMIT     0x3e20
#define SHARP0_PK_ALP2_MIN_MAX     0x3e21
#define SHARP0_PK_FINALGAIN_HP_BP     0x3e22
#define SHARP0_PK_OS_HORZ_CORE_GAIN     0x3e23
#define SHARP0_PK_OS_VERT_CORE_GAIN     0x3e24
#define SHARP0_PK_OS_ADPT_MISC     0x3e25
#define SHARP0_PK_OS_STATIC     0x3e26
#define SHARP0_PK_NR_ENABLE     0x3e27
#define SHARP0_PK_DRT_SAD_MISC     0x3e28
#define SHARP0_NR_TI_DNLP_BLEND     0x3e29
#define SHARP0_TI_DIR_CORE_ALPHA     0x3e2a
#define SHARP0_CTI_DIR_ALPHA     0x3e2b
#define SHARP0_LTI_CTI_DF_GAIN     0x3e2c
#define SHARP0_LTI_CTI_DIR_AC_DBG     0x3e2d
#define SHARP0_HCTI_FLT_CLP_DC     0x3e2e
#define SHARP0_HCTI_BST_GAIN     0x3e2f
#define SHARP0_HCTI_BST_CORE     0x3e30
#define SHARP0_HCTI_CON_2_GAIN_0     0x3e31
#define SHARP0_HCTI_CON_2_GAIN_1     0x3e32
#define SHARP0_HCTI_OS_MARGIN     0x3e33
#define SHARP0_HLTI_FLT_CLP_DC     0x3e34
#define SHARP0_HLTI_BST_GAIN     0x3e35
#define SHARP0_HLTI_BST_CORE     0x3e36
#define SHARP0_HLTI_CON_2_GAIN_0     0x3e37
#define SHARP0_HLTI_CON_2_GAIN_1     0x3e38
#define SHARP0_HLTI_OS_MARGIN     0x3e39
#define SHARP0_VLTI_FLT_CON_CLP     0x3e3a
#define SHARP0_VLTI_BST_GAIN     0x3e3b
#define SHARP0_VLTI_BST_CORE     0x3e3c
#define SHARP0_VLTI_CON_2_GAIN_0     0x3e3d
#define SHARP0_VLTI_CON_2_GAIN_1     0x3e3e
#define SHARP0_VCTI_FLT_CON_CLP     0x3e3f
#define SHARP0_VCTI_BST_GAIN     0x3e40
#define SHARP0_VCTI_BST_CORE     0x3e41
#define SHARP0_VCTI_CON_2_GAIN_0     0x3e42
#define SHARP0_VCTI_CON_2_GAIN_1     0x3e43
#define SHARP0_SHARP_3DLIMIT     0x3e44
#define SHARP0_DNLP_EN     0x3e45
#define SHARP0_DEMO_CRTL     0x3e56
#define SHARP0_SHARP_SR2_CTRL    0x3e57
#define SHARP0_SHARP_SR2_YBIC_HCOEF0    0x3e58
#define SHARP0_SHARP_SR2_YBIC_HCOEF1    0x3e59
#define SHARP0_SHARP_SR2_CBIC_HCOEF0    0x3e5a
#define SHARP0_SHARP_SR2_CBIC_HCOEF1    0x3e5b
#define SHARP0_SHARP_SR2_YBIC_VCOEF0    0x3e5c
#define SHARP0_SHARP_SR2_YBIC_VCOEF1    0x3e5d
#define SHARP0_SHARP_SR2_CBIC_VCOEF0    0x3e5e
#define SHARP0_SHARP_SR2_CBIC_VCOEF1    0x3e5f
#define SHARP0_SHARP_SR2_MISC    0x3e60
#define SHARP0_SR3_SAD_CTRL     0x3e61
#define SHARP0_SR3_PK_CTRL0     0x3e62
#define SHARP0_SR3_PK_CTRL1     0x3e63
#define SHARP0_DEJ_CTRL     0x3e64
#define SHARP0_DEJ_ALPHA     0x3e65
#define SHARP0_SR3_DRTLPF_EN     0x3e66
#define SHARP0_SR3_DRTLPF_ALPHA_0    0x3e67
#define SHARP0_SR3_DRTLPF_ALPHA_1    0x3e68
#define SHARP0_SR3_DRTLPF_ALPHA_2    0x3e69
#define SHARP0_SR3_DRTLPF_ALPHA_OFST     0x3e6a
#define SHARP0_SR3_DERING_CTRL     0x3e6b
#define SHARP0_SR3_DERING_LUMA2PKGAIN_0TO3     0x3e6c
#define SHARP0_SR3_DERING_LUMA2PKGAIN_4TO6     0x3e6d
#define SHARP0_SR3_DERING_LUMA2PKOS_0TO3     0x3e6e
#define SHARP0_SR3_DERING_LUMA2PKOS_4TO6     0x3e6f
#define SHARP0_SR3_DERING_GAINVS_MADSAD     0x3e70
#define SHARP0_SR3_DERING_GAINVS_VR2MAX     0x3e71
#define SHARP0_SR3_DERING_PARAM0     0x3e72
#define SHARP0_SR3_DRTLPF_THETA    0x3e73
#define SHARP0_SATPRT_CTRL     0x3e74
#define SHARP0_SATPRT_DIVM     0x3e75
#define SHARP0_DB_FLT_CTRL                           0x3e77
#define SHARP0_DB_FLT_YC_THRD     0x3e78
#define SHARP0_DB_FLT_RANDLUT     0x3e79
#define SHARP0_DB_FLT_PXI_THRD     0x3e7a
#define SHARP0_DB_FLT_SEED_Y     0x3e7b
#define SHARP0_DB_FLT_SEED_U     0x3e7c
#define SHARP0_DB_FLT_SEED_V     0x3e7d
#define SHARP0_PKGAIN_VSLUMA_LUT_L     0x3e7e
#define SHARP0_PKGAIN_VSLUMA_LUT_H     0x3e7f
#define SHARP0_PKOSHT_VSLUMA_LUT_L     0x3e80
#define SHARP0_PKOSHT_VSLUMA_LUT_H     0x3e81
#define SHARP0_SATPRT_LMT_RGB1     0x3e82
#define SHARP0_SATPRT_LMT_RGB2     0x3e83
#define SHARP0_SHARP_GATE_CLK_CTRL_0     0x3e84
#define SHARP0_SHARP_GATE_CLK_CTRL_1     0x3e85
#define SHARP0_SHARP_GATE_CLK_CTRL_2     0x3e86
#define SHARP0_SHARP_GATE_CLK_CTRL_3     0x3e87
#define SHARP0_SHARP_DPS_CTRL     0x3e88
#define SHARP0_DNLP_00     0x3e90
#define SHARP0_DNLP_01     0x3e91
#define SHARP0_DNLP_02     0x3e92
#define SHARP0_DNLP_03     0x3e93
#define SHARP0_DNLP_04     0x3e94
#define SHARP0_DNLP_05     0x3e95
#define SHARP0_DNLP_06     0x3e96
#define SHARP0_DNLP_07     0x3e97
#define SHARP0_DNLP_08     0x3e98
#define SHARP0_DNLP_09     0x3e99
#define SHARP0_DNLP_10     0x3e9a
#define SHARP0_DNLP_11     0x3e9b
#define SHARP0_DNLP_12     0x3e9c
#define SHARP0_DNLP_13     0x3e9d
#define SHARP0_DNLP_14     0x3e9e
#define SHARP0_DNLP_15     0x3e9f
#define SHARP0_DNLP_16     0x3ea0
#define SHARP0_DNLP_17     0x3ea1
#define SHARP0_DNLP_18    0x3ea2
#define SHARP0_DNLP_19     0x3ea3
#define SHARP0_DNLP_20     0x3ea4
#define SHARP0_DNLP_21     0x3ea5
#define SHARP0_DNLP_22     0x3ea6
#define SHARP0_DNLP_23     0x3ea7
#define SHARP0_DNLP_24     0x3ea8
#define SHARP0_DNLP_25     0x3ea9
#define SHARP0_DNLP_26     0x3eaa
#define SHARP0_DNLP_27     0x3eab
#define SHARP0_DNLP_28     0x3eac
#define SHARP0_DNLP_29     0x3ead
#define SHARP0_DNLP_30     0x3eae
#define SHARP0_DNLP_31    0x3eaf
#define SHARP0_SHARP_SYNC_CTRL     0x3eb0
#define SHARP1_SHARP_HVSIZE     0x3f00
#define SHARP1_SHARP_HVBLANK_NUM     0x3f01
#define SHARP1_NR_GAUSSIAN_MODE     0x3f02
#define SHARP1_PK_CON_2CIRHPGAIN_TH_RATE     0x3f05
#define SHARP1_PK_CON_2CIRHPGAIN_LIMIT     0x3f06
#define SHARP1_PK_CON_2CIRBPGAIN_TH_RATE     0x3f07
#define SHARP1_PK_CON_2CIRBPGAIN_LIMIT     0x3f08
#define SHARP1_PK_CON_2DRTHPGAIN_TH_RATE     0x3f09
#define SHARP1_PK_CON_2DRTHPGAIN_LIMIT     0x3f0a
#define SHARP1_PK_CON_2DRTBPGAIN_TH_RATE     0x3f0b
#define SHARP1_PK_CON_2DRTBPGAIN_LIMIT     0x3f0c
#define SHARP1_PK_CIRFB_LPF_MODE     0x3f0d
#define SHARP1_PK_DRTFB_LPF_MODE     0x3f0e
#define SHARP1_PK_CIRFB_HP_CORING     0x3f0f
#define SHARP1_PK_CIRFB_BP_CORING     0x3f10
#define SHARP1_PK_DRTFB_HP_CORING     0x3f11
#define SHARP1_PK_DRTFB_BP_CORING     0x3f12
#define SHARP1_PK_CIRFB_BLEND_GAIN     0x3f13
#define SHARP1_NR_ALPY_SSD_GAIN_OFST     0x3f14
#define SHARP1_NR_ALP0Y_ERR2CURV_TH_RATE     0x3f15
#define SHARP1_NR_ALP0Y_ERR2CURV_LIMIT     0x3f16
#define SHARP1_NR_ALP0C_ERR2CURV_TH_RATE     0x3f17
#define SHARP1_NR_ALP0C_ERR2CURV_LIMIT     0x3f18
#define SHARP1_NR_ALP0_MIN_MAX     0x3f19
#define SHARP1_NR_ALP1_MIERR_CORING     0x3f1a
#define SHARP1_NR_ALP1_ERR2CURV_TH_RATE     0x3f1b
#define SHARP1_NR_ALP1_ERR2CURV_LIMIT     0x3f1c
#define SHARP1_NR_ALP1_MIN_MAX     0x3f1d
#define SHARP1_PK_ALP2_MIERR_CORING     0x3f1e
#define SHARP1_PK_ALP2_ERR2CURV_TH_RATE     0x3f1f
#define SHARP1_PK_ALP2_ERR2CURV_LIMIT     0x3f20
#define SHARP1_PK_ALP2_MIN_MAX     0x3f21
#define SHARP1_PK_FINALGAIN_HP_BP     0x3f22
#define SHARP1_PK_OS_HORZ_CORE_GAIN     0x3f23
#define SHARP1_PK_OS_VERT_CORE_GAIN     0x3f24
#define SHARP1_PK_OS_ADPT_MISC     0x3f25
#define SHARP1_PK_OS_STATIC     0x3f26
#define SHARP1_PK_NR_ENABLE     0x3f27
#define SHARP1_PK_DRT_SAD_MISC     0x3f28
#define SHARP1_NR_TI_DNLP_BLEND     0x3f29
#define SHARP1_TI_DIR_CORE_ALPHA     0x3f2a
#define SHARP1_CTI_DIR_ALPHA     0x3f2b
#define SHARP1_LTI_CTI_DF_GAIN     0x3f2c
#define SHARP1_LTI_CTI_DIR_AC_DBG     0x3f2d
#define SHARP1_HCTI_FLT_CLP_DC     0x3f2e
#define SHARP1_HCTI_BST_GAIN     0x3f2f
#define SHARP1_HCTI_BST_CORE     0x3f30
#define SHARP1_HCTI_CON_2_GAIN_0     0x3f31
#define SHARP1_HCTI_CON_2_GAIN_1     0x3f32
#define SHARP1_HCTI_OS_MARGIN     0x3f33
#define SHARP1_HLTI_FLT_CLP_DC     0x3f34
#define SHARP1_HLTI_BST_GAIN     0x3f35
#define SHARP1_HLTI_BST_CORE     0x3f36
#define SHARP1_HLTI_CON_2_GAIN_0     0x3f37
#define SHARP1_HLTI_CON_2_GAIN_1     0x3f38
#define SHARP1_HLTI_OS_MARGIN     0x3f39
#define SHARP1_VLTI_FLT_CON_CLP     0x3f3a
#define SHARP1_VLTI_BST_GAIN     0x3f3b
#define SHARP1_VLTI_BST_CORE     0x3f3c
#define SHARP1_VLTI_CON_2_GAIN_0     0x3f3d
#define SHARP1_VLTI_CON_2_GAIN_1     0x3f3e
#define SHARP1_VCTI_FLT_CON_CLP     0x3f3f
#define SHARP1_VCTI_BST_GAIN     0x3f40
#define SHARP1_VCTI_BST_CORE     0x3f41
#define SHARP1_VCTI_CON_2_GAIN_0     0x3f42
#define SHARP1_VCTI_CON_2_GAIN_1     0x3f43
#define SHARP1_SHARP_3DLIMIT     0x3f44
#define SHARP1_DNLP_EN     0x3f45
#define SHARP1_DEMO_CRTL     0x3f56
#define SHARP1_SHARP_SR2_CTRL    0x3f57
#define SHARP1_SHARP_SR2_YBIC_HCOEF0    0x3f58
#define SHARP1_SHARP_SR2_YBIC_HCOEF1    0x3f59
#define SHARP1_SHARP_SR2_CBIC_HCOEF0    0x3f5a
#define SHARP1_SHARP_SR2_CBIC_HCOEF1    0x3f5b
#define SHARP1_SHARP_SR2_YBIC_VCOEF0    0x3f5c
#define SHARP1_SHARP_SR2_YBIC_VCOEF1    0x3f5d
#define SHARP1_SHARP_SR2_CBIC_VCOEF0    0x3f5e
#define SHARP1_SHARP_SR2_CBIC_VCOEF1    0x3f5f
#define SHARP1_SHARP_SR2_MISC    0x3f60
#define SHARP1_SR3_SAD_CTRL     0x3f61
#define SHARP1_SR3_PK_CTRL0     0x3f62
#define SHARP1_SR3_PK_CTRL1     0x3f63
#define SHARP1_DEJ_CTRL     0x3f64
#define SHARP1_DEJ_ALPHA     0x3f65
#define SHARP1_SR3_DRTLPF_EN     0x3f66
#define SHARP1_SR3_DRTLPF_ALPHA_0    0x3f67
#define SHARP1_SR3_DRTLPF_ALPHA_1    0x3f68
#define SHARP1_SR3_DRTLPF_ALPHA_2    0x3f69
#define SHARP1_SR3_DRTLPF_ALPHA_OFST     0x3f6a
#define SHARP1_SR3_DERING_CTRL     0x3f6b
#define SHARP1_SR3_DERING_LUMA2PKGAIN_0TO3     0x3f6c
#define SHARP1_SR3_DERING_LUMA2PKGAIN_4TO6     0x3f6d
#define SHARP1_SR3_DERING_LUMA2PKOS_0TO3     0x3f6e
#define SHARP1_SR3_DERING_LUMA2PKOS_4TO6     0x3f6f
#define SHARP1_SR3_DERING_GAINVS_MADSAD     0x3f70
#define SHARP1_SR3_DERING_GAINVS_VR2MAX     0x3f71
#define SHARP1_SR3_DERING_PARAM0     0x3f72
#define SHARP1_SR3_DRTLPF_THETA    0x3f73
#define SHARP1_SATPRT_CTRL     0x3f74
#define SHARP1_SATPRT_DIVM     0x3f75
#define SHARP1_DB_FLT_CTRL                           0x3f77
#define SHARP1_DB_FLT_YC_THRD     0x3f78
#define SHARP1_DB_FLT_RANDLUT     0x3f79
#define SHARP1_DB_FLT_PXI_THRD     0x3f7a
#define SHARP1_DB_FLT_SEED_Y     0x3f7b
#define SHARP1_DB_FLT_SEED_U     0x3f7c
#define SHARP1_DB_FLT_SEED_V     0x3f7d
#define SHARP1_PKGAIN_VSLUMA_LUT_L     0x3f7e
#define SHARP1_PKGAIN_VSLUMA_LUT_H     0x3f7f
#define SHARP1_PKOSHT_VSLUMA_LUT_L     0x3f80
#define SHARP1_PKOSHT_VSLUMA_LUT_H     0x3f81
#define SHARP1_SATPRT_LMT_RGB1     0x3f82
#define SHARP1_SATPRT_LMT_RGB2     0x3f83
#define SHARP1_SHARP_GATE_CLK_CTRL_0     0x3f84
#define SHARP1_SHARP_GATE_CLK_CTRL_1     0x3f85
#define SHARP1_SHARP_GATE_CLK_CTRL_2     0x3f86
#define SHARP1_SHARP_GATE_CLK_CTRL_3     0x3f87
#define SHARP1_SHARP_DPS_CTRL     0x3f88
#define SHARP1_DNLP_00     0x3f90
#define SHARP1_DNLP_01     0x3f91
#define SHARP1_DNLP_02     0x3f92
#define SHARP1_DNLP_03     0x3f93
#define SHARP1_DNLP_04     0x3f94
#define SHARP1_DNLP_05     0x3f95
#define SHARP1_DNLP_06     0x3f96
#define SHARP1_DNLP_07     0x3f97
#define SHARP1_DNLP_08     0x3f98
#define SHARP1_DNLP_09     0x3f99
#define SHARP1_DNLP_10     0x3f9a
#define SHARP1_DNLP_11     0x3f9b
#define SHARP1_DNLP_12     0x3f9c
#define SHARP1_DNLP_13     0x3f9d
#define SHARP1_DNLP_14     0x3f9e
#define SHARP1_DNLP_15     0x3f9f
#define SHARP1_DNLP_16     0x3fa0
#define SHARP1_DNLP_17     0x3fa1
#define SHARP1_DNLP_18    0x3fa2
#define SHARP1_DNLP_19     0x3fa3
#define SHARP1_DNLP_20     0x3fa4
#define SHARP1_DNLP_21     0x3fa5
#define SHARP1_DNLP_22     0x3fa6
#define SHARP1_DNLP_23     0x3fa7
#define SHARP1_DNLP_24     0x3fa8
#define SHARP1_DNLP_25     0x3fa9
#define SHARP1_DNLP_26     0x3faa
#define SHARP1_DNLP_27     0x3fab
#define SHARP1_DNLP_28     0x3fac
#define SHARP1_DNLP_29     0x3fad
#define SHARP1_DNLP_30     0x3fae
#define SHARP1_DNLP_31    0x3faf
#define SHARP1_SHARP_SYNC_CTRL     0x3fb0
/*TL1 SHARPNESS REG END*/

/*LC register begin*/
#define SRSHARP1_LC_INPUT_MUX     0x3fb1
#define SRSHARP1_LC_TOP_CTRL     0x3fc0
#define SRSHARP1_LC_HV_NUM     0x3fc1
#define SRSHARP1_LC_SAT_LUT_0_1     0x3fc2
#define SRSHARP1_LC_SAT_LUT_2_3     0x3fc3
#define SRSHARP1_LC_SAT_LUT_4_5     0x3fc4
#define SRSHARP1_LC_SAT_LUT_6_7     0x3fc5
#define SRSHARP1_LC_SAT_LUT_8_9     0x3fc6
#define SRSHARP1_LC_SAT_LUT_10_11     0x3fc7
#define SRSHARP1_LC_SAT_LUT_12_13     0x3fc8
#define SRSHARP1_LC_SAT_LUT_14_15     0x3fc9
#define SRSHARP1_LC_SAT_LUT_16_17     0x3fca
#define SRSHARP1_LC_SAT_LUT_18_19     0x3fcb
#define SRSHARP1_LC_SAT_LUT_20_21     0x3fcc
#define SRSHARP1_LC_SAT_LUT_22_23     0x3fcd
#define SRSHARP1_LC_SAT_LUT_24_25     0x3fce
#define SRSHARP1_LC_SAT_LUT_26_27     0x3fcf
#define SRSHARP1_LC_SAT_LUT_28_29     0x3fd0
#define SRSHARP1_LC_SAT_LUT_30_31     0x3fd1
#define SRSHARP1_LC_SAT_LUT_32_33     0x3fd2
#define SRSHARP1_LC_SAT_LUT_34_35     0x3fd3
#define SRSHARP1_LC_SAT_LUT_36_37     0x3fd4
#define SRSHARP1_LC_SAT_LUT_38_39     0x3fd5
#define SRSHARP1_LC_SAT_LUT_40_41     0x3fd6
#define SRSHARP1_LC_SAT_LUT_42_43     0x3fd7
#define SRSHARP1_LC_SAT_LUT_44_45     0x3fd8
#define SRSHARP1_LC_SAT_LUT_46_47     0x3fd9
#define SRSHARP1_LC_SAT_LUT_48_49     0x3fda
#define SRSHARP1_LC_SAT_LUT_50_51     0x3fdb
#define SRSHARP1_LC_SAT_LUT_52_53     0x3fdc
#define SRSHARP1_LC_SAT_LUT_54_55     0x3fdd
#define SRSHARP1_LC_SAT_LUT_56_57     0x3fde
#define SRSHARP1_LC_SAT_LUT_58_59     0x3fdf
#define SRSHARP1_LC_SAT_LUT_60_61     0x3fe0
#define SRSHARP1_LC_SAT_LUT_62     0x3fe1
#define SRSHARP1_LC_CURVE_BLK_HIDX_0_1     0x3fe2
#define SRSHARP1_LC_CURVE_BLK_HIDX_2_3     0x3fe3
#define SRSHARP1_LC_CURVE_BLK_HIDX_4_5     0x3fe4
#define SRSHARP1_LC_CURVE_BLK_HIDX_6_7     0x3fe5
#define SRSHARP1_LC_CURVE_BLK_HIDX_8_9     0x3fe6
#define SRSHARP1_LC_CURVE_BLK_HIDX_10_11     0x3fe7
#define SRSHARP1_LC_CURVE_BLK_HIDX_12     0x3fe8
#define SRSHARP1_LC_CURVE_BLK_VIDX_0_1     0x3fe9
#define SRSHARP1_LC_CURVE_BLK_VIDX_2_3     0x3fea
#define SRSHARP1_LC_CURVE_BLK_VIDX_4_5     0x3feb
#define SRSHARP1_LC_CURVE_BLK_VIDX_6_7     0x3fec
#define SRSHARP1_LC_CURVE_BLK_VIDX_8     0x3fed
#define SRSHARP1_LC_YUV2RGB_MAT_0_1    0x3fee
#define SRSHARP1_LC_YUV2RGB_MAT_2_3    0x3fef
#define SRSHARP1_LC_YUV2RGB_MAT_4_5    0x3ff0
#define SRSHARP1_LC_YUV2RGB_MAT_6_7    0x3ff1
#define SRSHARP1_LC_YUV2RGB_MAT_8    0x3ff2
#define SRSHARP1_LC_RGB2YUV_MAT_0_1    0x3ff3
#define SRSHARP1_LC_RGB2YUV_MAT_2_3    0x3ff4
#define SRSHARP1_LC_RGB2YUV_MAT_4_5    0x3ff5
#define SRSHARP1_LC_RGB2YUV_MAT_6_7    0x3ff6
#define SRSHARP1_LC_RGB2YUV_MAT_8    0x3ff7
#define SRSHARP1_LC_YUV2RGB_OFST    0x3ff8
#define SRSHARP1_LC_YUV2RGB_CLIP    0x3ff9
#define SRSHARP1_LC_RGB2YUV_OFST    0x3ffa
#define SRSHARP1_LC_RGB2YUV_CLIP    0x3ffb
#define SRSHARP1_LC_MAP_RAM_CTRL    0x3ffc
#define SRSHARP1_LC_MAP_RAM_ADDR    0x3ffd
#define SRSHARP1_LC_MAP_RAM_DATA    0x3ffe

#define LC_CURVE_CTRL     0x4000
#define LC_CURVE_HV_NUM     0x4001
#define LC_CURVE_LMT_RAT     0x4002
#define LC_CURVE_CONTRAST_LH     0x4003
#define LC_CURVE_CONTRAST__LMT_LH     0x4004
#define LC_CURVE_CONTRAST_SCL_LH     0x4005
#define LC_CURVE_CONTRAST_BVN_LH     0x4006
#define LC_CURVE_MISC0     0x4007
#define LC_CURVE_YPKBV_RAT     0x4008
#define LC_CURVE_YPKBV_SLP_LMT     0x4009
#define LC_CURVE_YMINVAL_LMT_0_1     0x400a
#define LC_CURVE_YMINVAL_LMT_2_3     0x400b
#define LC_CURVE_YMINVAL_LMT_4_5     0x400c
#define LC_CURVE_YMINVAL_LMT_6_7     0x400d
#define LC_CURVE_YMINVAL_LMT_8_9     0x400e
#define LC_CURVE_YMINVAL_LMT_10_11     0x400f
#define LC_CURVE_YPKBV_YMAXVAL_LMT_0_1     0x4010
#define LC_CURVE_YPKBV_YMAXVAL_LMT_2_3     0x4011
#define LC_CURVE_YPKBV_YMAXVAL_LMT_4_5     0x4012
#define LC_CURVE_YPKBV_YMAXVAL_LMT_6_7     0x4013
#define LC_CURVE_YPKBV_YMAXVAL_LMT_8_9     0x4014
#define LC_CURVE_YPKBV_YMAXVAL_LMT_10_11     0x4015
#define LC_CURVE_HISTVLD_THRD     0x4016
#define LC_CURVE_BB_MUTE_THRD     0x4017
#define LC_CURVE_INT_STATUS     0x4018
#define LC_CURVE_RAM_CTRL    0x4020
#define LC_CURVE_RAM_ADDR    0x4021
#define LC_CURVE_RAM_DATA    0x4022

#define LC_STTS_GCLK_CTRL0     0x4028
#define LC_STTS_CTRL0     0x4029
#define LC_STTS_WIDTHM1_HEIGHTM1     0x402a
#define LC_STTS_MATRIX_COEF00_01     0x402b
#define LC_STTS_MATRIX_COEF02_10     0x402c
#define LC_STTS_MATRIX_COEF11_12     0x402d
#define LC_STTS_MATRIX_COEF20_21     0x402e
#define LC_STTS_MATRIX_COEF22     0x402f
#define LC_STTS_MATRIX_OFFSET0_1     0x4030
#define LC_STTS_MATRIX_OFFSET2     0x4031
#define LC_STTS_MATRIX_PRE_OFFSET0_1     0x4032
#define LC_STTS_MATRIX_PRE_OFFSET2     0x4033
#define LC_STTS_MATRIX_HL_COLOR     0x4034
#define LC_STTS_MATRIX_PROBE_POS     0x4035
#define LC_STTS_MATRIX_PROBE_COLOR     0x4036
#define LC_STTS_HIST_REGION_IDX     0x4037
#define LC_STTS_HIST_SET_REGION     0x4038
#define LC_STTS_HIST_READ_REGION     0x4039
#define LC_STTS_HIST_START_RD_REGION     0x403a
#define LC_STTS_WHITE_INFO     0x403b
#define LC_STTS_BLACK_INFO     0x403c
/*LC register end*/
#endif

