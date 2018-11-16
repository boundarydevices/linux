/*
 * drivers/amlogic/media/vout/cvbs/cvbs_out_reg.h
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

#ifndef __CVBS_OUT_REG_H__
#define __CVBS_OUT_REG_H__
#include <linux/amlogic/iomap.h>

/* ********************************
 * register offset address define
 * **********************************/
/* base & offset */
#define CVBS_OUT_REG_BASE_CBUS                       (0xc1100000L)
#define CVBS_OUT_REG_BASE_HIU                        (0xc883c000L)
#define CVBS_OUT_REG_BASE_VCBUS                      (0xd0100000L)
#define CVBS_OUT_REG_OFFSET_CBUS(reg)                ((reg << 2))
#define CVBS_OUT_REG_OFFSET_HIU(reg)                 ((reg << 2))
#define CVBS_OUT_REG_OFFSET_VCBUS(reg)               ((reg << 2))

/*********************************
 * HIU:  HHI_CBUS_BASE = 0x10
 **********************************/
#define HHI_GCLK_MPEG0                             0x50
#define HHI_GCLK_MPEG1                             0x51
#define HHI_GCLK_MPEG2                             0x52
#define HHI_GCLK_OTHER                             0x54
	#define VCLK2_VENCI		1
	#define VCLK2_VENCI1	2
	#define GCLK_VENCI_INT	8
	#define DAC_CLK			10

#define HHI_VIID_PLL_CNTL4                         0x46
#define HHI_VIID_PLL_CNTL                          0x47
#define HHI_VIID_PLL_CNTL2                         0x48
#define HHI_VIID_PLL_CNTL3                         0x49
#define HHI_VIID_CLK_DIV                           0x4a
    #define DAC0_CLK_SEL           28
    #define DAC1_CLK_SEL           24
    #define DAC2_CLK_SEL           20
    #define VCLK2_XD_RST           17
    #define VCLK2_XD_EN            16
    #define ENCL_CLK_SEL           12
    #define VCLK2_XD                0
#define HHI_VIID_CLK_CNTL                          0x4b
    #define VCLK2_EN               19
    #define VCLK2_CLK_IN_SEL       16
    #define VCLK2_SOFT_RST         15
    #define VCLK2_DIV12_EN          4
    #define VCLK2_DIV6_EN           3
    #define VCLK2_DIV4_EN           2
    #define VCLK2_DIV2_EN           1
    #define VCLK2_DIV1_EN           0
#define HHI_VIID_DIVIDER_CNTL                      0x4c
    #define DIV_CLK_IN_EN          16
    #define DIV_CLK_SEL            15
    #define DIV_POST_TCNT          12
    #define DIV_LVDS_CLK_EN        11
    #define DIV_LVDS_DIV2          10
    #define DIV_POST_SEL            8
    #define DIV_POST_SOFT_RST       7
    #define DIV_PRE_SEL             4
    #define DIV_PRE_SOFT_RST        3
    #define DIV_POST_RST            1
    #define DIV_PRE_RST             0
#define HHI_VID_CLK_DIV                            0x59
    #define ENCI_CLK_SEL           28
    #define ENCP_CLK_SEL           24
    #define ENCT_CLK_SEL           20
    #define VCLK_XD_RST            17
    #define VCLK_XD_EN             16
    #define ENCL_CLK_SEL           12
    #define VCLK_XD1                8
    #define VCLK_XD0                0
#define HHI_VID_CLK_CNTL                           0x5f
    #define VCLK_EN1               20
    #define VCLK_EN0               19
    #define VCLK_CLK_IN_SEL        16
    #define VCLK_SOFT_RST          15
    #define VCLK_DIV12_EN           4
    #define VCLK_DIV6_EN            3
    #define VCLK_DIV4_EN            2
    #define VCLK_DIV2_EN            1
    #define VCLK_DIV1_EN            0
#define HHI_VID_CLK_CNTL2                          0x65
    #define HDMI_TX_PIXEL_GATE_VCLK  5
    #define VDAC_GATE_VCLK           4
    #define ENCL_GATE_VCLK           3
    #define ENCP_GATE_VCLK           2
    #define ENCT_GATE_VCLK           1
    #define ENCI_GATE_VCLK           0
#define HHI_VID_DIVIDER_CNTL                       0x66
#define HHI_VID_PLL_CLK_DIV                        0x68
#define HHI_HDMI_CLK_CNTL                          0x73
#define HHI_EDP_APB_CLK_CNTL                       0x7b
#define HHI_HDMI_AFC_CNTL                          0x7f
#define HHI_EDP_APB_CLK_CNTL_M8M2                  0x82
#define HHI_EDP_TX_PHY_CNTL0                       0x9c
#define HHI_EDP_TX_PHY_CNTL1                       0x9d
/* m8b */
#define HHI_VID_PLL_CNTL                           0xc8
#define HHI_VID_PLL_CNTL2                          0xc9
#define HHI_VID_PLL_CNTL3                          0xca
#define HHI_VID_PLL_CNTL4                          0xcb
#define HHI_VID_PLL_CNTL5                          0xcc
#define HHI_VID_PLL_CNTL6                          0xcd
/* g9tv */
#define HHI_HDMI_PLL_CNTL                          0xc8
#define HHI_HDMI_PLL_CNTL2                         0xc9
#define HHI_HDMI_PLL_CNTL3                         0xca
#define HHI_HDMI_PLL_CNTL4                         0xcb
#define HHI_HDMI_PLL_CNTL5                         0xcc
#define HHI_HDMI_PLL_CNTL6                         0xcd
/*G12A*/
#define HHI_HDMI_PLL_CNTL7                         0xce

/* TL1 */
#define HHI_TCON_PLL_CNTL0                         0x020
#define HHI_TCON_PLL_CNTL1                         0x021
#define HHI_TCON_PLL_CNTL2                         0x022
#define HHI_TCON_PLL_CNTL3                         0x023
#define HHI_TCON_PLL_CNTL4                         0x0df

#define HHI_GP0_PLL_CNTL0						   0x10
#define HHI_GP0_PLL_CNTL1						   0x11
#define HHI_GP0_PLL_CNTL2						   0x12
#define HHI_GP0_PLL_CNTL3						   0x13
#define HHI_GP0_PLL_CNTL4						   0x14
#define HHI_GP0_PLL_CNTL5						   0x15
#define HHI_GP0_PLL_CNTL6						   0x16

#define HHI_DSI_LVDS_EDP_CNTL0                     0xd1
#define HHI_DSI_LVDS_EDP_CNTL1                     0xd2
#define HHI_DIF_CSI_PHY_CNTL0                      0xd8
#define HHI_DIF_CSI_PHY_CNTL1                      0xd9
#define HHI_DIF_CSI_PHY_CNTL2                      0xda
#define HHI_DIF_CSI_PHY_CNTL3                      0xdb
#define HHI_DIF_CSI_PHY_CNTL4                      0xdc
#define HHI_DIF_CSI_PHY_CNTL5                      0xdd
#define HHI_LVDS_TX_PHY_CNTL0                      0xde
#define HHI_LVDS_TX_PHY_CNTL1                      0xdf
#define HHI_VID2_PLL_CNTL                          0xe0
#define HHI_VID2_PLL_CNTL2                         0xe1
#define HHI_VID2_PLL_CNTL3                         0xe2
#define HHI_VID2_PLL_CNTL4                         0xe3
#define HHI_VID2_PLL_CNTL5                         0xe4
#define HHI_VID2_PLL_CNTL6                         0xe5
#define HHI_VID_LOCK_CLK_CNTL                      0xf2

#define HHI_VDAC_CNTL0                             0xbd
#define HHI_VDAC_CNTL1                             0xbe
#define HHI_VDAC_CNTL0_G12A                        0xbb
#define HHI_VDAC_CNTL1_G12A                        0xbc

/*********************************
 * HIU:  GXBB
 **********************************/
#if 0
#define HHI_VIID_CLK_DIV                           0x4a
#define HHI_VIID_CLK_CNTL                          0x4b
#define HHI_VIID_DIVIDER_CNTL                      0x4c
#define HHI_VID_CLK_DIV                            0x59
#define HHI_VID_CLK_CNTL                           0x5f
#define HHI_VID_CLK_CNTL2                          0x65
#define HHI_VID_DIVIDER_CNTL                       0x66
#define HHI_VID_PLL_CLK_DIV                        0x68
#define HHI_EDP_APB_CLK_CNTL                       0x82
#define HHI_EDP_TX_PHY_CNTL0                       0x9c
#define HHI_EDP_TX_PHY_CNTL1                       0x9d

#define HHI_HDMI_PLL_CNTL                          0xc8
#define HHI_HDMI_PLL_CNTL2                         0xc9
#define HHI_HDMI_PLL_CNTL3                         0xca
#define HHI_HDMI_PLL_CNTL4                         0xcb
#define HHI_HDMI_PLL_CNTL5                         0xcc
#define HHI_HDMI_PLL_CNTL6                         0xcd
#define HHI_HDMI_PLL_CNTL_I                        0xce
#define HHI_HDMI_PLL_CNTL7                         0xcf

#define HHI_DSI_LVDS_EDP_CNTL0                     0xd1
#define HHI_DSI_LVDS_EDP_CNTL1                     0xd2
#define HHI_DIF_CSI_PHY_CNTL0                      0xd8
#define HHI_DIF_CSI_PHY_CNTL1                      0xd9
#define HHI_DIF_CSI_PHY_CNTL2                      0xda
#define HHI_DIF_CSI_PHY_CNTL3                      0xdb
#define HHI_DIF_CSI_PHY_CNTL4                      0xdc
#define HHI_DIF_CSI_PHY_CNTL5                      0xdd
#define HHI_LVDS_TX_PHY_CNTL0                      0xde
#define HHI_LVDS_TX_PHY_CNTL1                      0xdf

#define HHI_VID2_PLL_CNTL                          0xe0
#define HHI_VID2_PLL_CNTL2                         0xe1
#define HHI_VID2_PLL_CNTL3                         0xe2
#define HHI_VID2_PLL_CNTL4                         0xe3
#define HHI_VID2_PLL_CNTL5                         0xe4
#define HHI_VID2_PLL_CNTL_I                        0xe5
#define HHI_VID_LOCK_CLK_CNTL                      0xf2
#else
#define CVBS_OUT_HIU_REG_GX(addr)                    (addr & 0xff)
#endif

/*********************************
 * Global control:  RESET_CBUS_BASE = 0x11
 **********************************/
#define VERSION_CTRL                               0x1100
#define RESET0_REGISTER                            0x1101
#define RESET1_REGISTER                            0x1102
#define RESET2_REGISTER                            0x1103
#define RESET3_REGISTER                            0x1104
#define RESET4_REGISTER                            0x1105
#define RESET5_REGISTER                            0x1106
#define RESET6_REGISTER                            0x1107
#define RESET7_REGISTER                            0x1108
#define RESET0_MASK                                0x1110
#define RESET1_MASK                                0x1111
#define RESET2_MASK                                0x1112
#define RESET3_MASK                                0x1113
#define RESET4_MASK                                0x1114
#define RESET5_MASK                                0x1115
#define RESET6_MASK                                0x1116
#define CRT_MASK                                   0x1117
#define RESET7_MASK                                0x1118

/*********************************
 * Video Interface:  VENC_VCBUS_BASE = 0x1b
 *********************************/
#define VENC_INTCTRL                               0x1b6e

/*********************************
 * ENCI
 **********************************/
#define ENCI_VIDEO_MODE                            0x1b00
#define ENCI_VIDEO_MODE_ADV                        0x1b01
#define ENCI_VIDEO_FSC_ADJ                         0x1b02
#define ENCI_VIDEO_BRIGHT                          0x1b03
#define ENCI_VIDEO_CONT                            0x1b04
#define ENCI_VIDEO_SAT                             0x1b05
#define ENCI_VIDEO_HUE                             0x1b06
#define ENCI_VIDEO_SCH                             0x1b07
#define ENCI_SYNC_MODE                             0x1b08
#define ENCI_SYNC_CTRL                             0x1b09
#define ENCI_SYNC_HSO_BEGIN                        0x1b0a
#define ENCI_SYNC_HSO_END                          0x1b0b
#define ENCI_SYNC_VSO_EVN                          5
#define ENCI_SYNC_VSO_ODD                          0x1b0d
#define ENCI_SYNC_VSO_EVNLN                        0x1b0e
#define ENCI_SYNC_VSO_ODDLN                        0x1b0f
#define ENCI_SYNC_HOFFST                           0x1b10
#define ENCI_SYNC_VOFFST                           0x1b11
#define ENCI_SYNC_ADJ                              0x1b12
#define ENCI_RGB_SETTING                           0x1b13

#define ENCI_DE_H_BEGIN                            0x1b16
#define ENCI_DE_H_END                              0x1b17
#define ENCI_DE_V_BEGIN_EVEN                       0x1b18
#define ENCI_DE_V_END_EVEN                         0x1b19
#define ENCI_DE_V_BEGIN_ODD                        0x1b1a
#define ENCI_DE_V_END_ODD                          0x1b1b
#define ENCI_VBI_SETTING                           0x1b20
#define ENCI_VBI_CCDT_EVN                          0x1b21
#define ENCI_VBI_CCDT_ODD                          0x1b22
#define ENCI_VBI_CC525_LN                          0x1b23
#define ENCI_VBI_CC625_LN                          0x1b24
#define ENCI_VBI_WSSDT                             0x1b25
#define ENCI_VBI_WSS_LN                            0x1b26
#define ENCI_VBI_CGMSDT_L                          0x1b27
#define ENCI_VBI_CGMSDT_H                          0x1b28
#define ENCI_VBI_CGMS_LN                           0x1b29
#define ENCI_VBI_TTX_HTIME                         0x1b2a
#define ENCI_VBI_TTX_LN                            0x1b2b
#define ENCI_VBI_TTXDT0                            0x1b2c
#define ENCI_VBI_TTXDT1                            0x1b2d
#define ENCI_VBI_TTXDT2                            0x1b2e
#define ENCI_VBI_TTXDT3                            0x1b2f
#define ENCI_MACV_N0                               0x1b30
#define ENCI_MACV_N1                               0x1b31
#define ENCI_MACV_N2                               0x1b32
#define ENCI_MACV_N3                               0x1b33
#define ENCI_MACV_N4                               0x1b34
#define ENCI_MACV_N5                               0x1b35
#define ENCI_MACV_N6                               0x1b36
#define ENCI_MACV_N7                               0x1b37
#define ENCI_MACV_N8                               0x1b38
#define ENCI_MACV_N9                               0x1b39
#define ENCI_MACV_N10                              0x1b3a
#define ENCI_MACV_N11                              0x1b3b
#define ENCI_MACV_N12                              0x1b3c
#define ENCI_MACV_N13                              0x1b3d
#define ENCI_MACV_N14                              0x1b3e
#define ENCI_MACV_N15                              0x1b3f
#define ENCI_MACV_N16                              0x1b40
#define ENCI_MACV_N17                              0x1b41
#define ENCI_MACV_N18                              0x1b42
#define ENCI_MACV_N19                              0x1b43
#define ENCI_MACV_N20                              0x1b44
#define ENCI_MACV_N21                              0x1b45
#define ENCI_MACV_N22                              0x1b46

#define ENCI_DBG_PX_RST                            0x1b48
#define ENCI_DBG_FLDLN_RST                         0x1b49
#define ENCI_DBG_PX_INT                            0x1b4a
#define ENCI_DBG_FLDLN_INT                         0x1b4b
#define ENCI_DBG_MAXPX                             0x1b4c
#define ENCI_DBG_MAXLN                             0x1b4d
#define ENCI_MACV_MAX_AMP                          0x1b50
#define ENCI_MACV_PULSE_LO                         0x1b51
#define ENCI_MACV_PULSE_HI                         0x1b52
#define ENCI_MACV_BKP_MAX                          0x1b53
#define ENCI_CFILT_CTRL                            0x1b54
#define ENCI_CFILT7                                0x1b55
#define ENCI_YC_DELAY                              0x1b56
#define ENCI_VIDEO_EN                              0x1b57

#define ENCI_DVI_HSO_BEGIN                         0x1c00
#define ENCI_DVI_HSO_END                           0x1c01
#define ENCI_DVI_VSO_BLINE_EVN                     0x1c02
#define ENCI_DVI_VSO_BLINE_ODD                     0x1c03
#define ENCI_DVI_VSO_ELINE_EVN                     0x1c04
#define ENCI_DVI_VSO_ELINE_ODD                     0x1c05
#define ENCI_DVI_VSO_BEGIN_EVN                     0x1c06
#define ENCI_DVI_VSO_BEGIN_ODD                     0x1c07
#define ENCI_DVI_VSO_END_EVN                       0x1c08
#define ENCI_DVI_VSO_END_ODD                       0x1c09

#define ENCI_CFILT_CTRL2                           0x1c0a
#define ENCI_DACSEL_0                              0x1c0b
#define ENCI_DACSEL_1                              0x1c0c
#define ENCP_DACSEL_0                              0x1c0d
#define ENCP_DACSEL_1                              0x1c0e
#define ENCP_MAX_LINE_SWITCH_POINT                 0x1c0f
#define ENCI_TST_EN                                0x1c10
#define ENCI_TST_MDSEL                             0x1c11
#define ENCI_TST_Y                                 0x1c12
#define ENCI_TST_CB                                0x1c13
#define ENCI_TST_CR                                0x1c14
#define ENCI_TST_CLRBAR_STRT                       0x1c15
#define ENCI_TST_CLRBAR_WIDTH                      0x1c16
#define ENCI_TST_VDCNT_STSET                       0x1c17

#define ENCI_VFIFO2VD_CTL                          0x1c18

#define ENCI_VFIFO2VD_PIXEL_START                  0x1c19

#define ENCI_VFIFO2VD_PIXEL_END                    0x1c1a

#define ENCI_VFIFO2VD_LINE_TOP_START               0x1c1b

#define ENCI_VFIFO2VD_LINE_TOP_END                 0x1c1c

#define ENCI_VFIFO2VD_LINE_BOT_START               0x1c1d

#define ENCI_VFIFO2VD_LINE_BOT_END                 0x1c1e
#define ENCI_VFIFO2VD_CTL2                         0x1c1f

#define ENCP_DVI_HSO_BEGIN                         0x1c30
#define ENCP_DVI_HSO_END                           0x1c31
#define ENCP_DVI_VSO_BLINE_EVN                     0x1c32
#define ENCP_DVI_VSO_BLINE_ODD                     0x1c33
#define ENCP_DVI_VSO_ELINE_EVN                     0x1c34
#define ENCP_DVI_VSO_ELINE_ODD                     0x1c35
#define ENCP_DVI_VSO_BEGIN_EVN                     0x1c36
#define ENCP_DVI_VSO_BEGIN_ODD                     0x1c37
#define ENCP_DVI_VSO_END_EVN                       0x1c38
#define ENCP_DVI_VSO_END_ODD                       0x1c39
#define ENCP_DE_H_BEGIN                            0x1c3a
#define ENCP_DE_H_END                              0x1c3b
#define ENCP_DE_V_BEGIN_EVEN                       0x1c3c
#define ENCP_DE_V_END_EVEN                         0x1c3d
#define ENCP_DE_V_BEGIN_ODD                        0x1c3e
#define ENCP_DE_V_END_ODD                          0x1c3f

#define ENCI_SYNC_LINE_LENGTH                      0x1c40

#define ENCI_SYNC_PIXEL_EN                         0x1c41

#define ENCI_SYNC_TO_LINE_EN                       0x1c42

#define ENCI_SYNC_TO_PIXEL                         0x1c43

#define ENCP_SYNC_LINE_LENGTH                      0x1c44

#define ENCP_SYNC_PIXEL_EN                         0x1c45

#define ENCP_SYNC_TO_LINE_EN                       0x1c46

#define ENCP_SYNC_TO_PIXEL                         0x1c47

#define ENCT_SYNC_LINE_LENGTH                      0x1c48

#define ENCT_SYNC_PIXEL_EN                         0x1c49

#define ENCT_SYNC_TO_LINE_EN                       0x1c4a

#define ENCT_SYNC_TO_PIXEL                         0x1c4b

#define ENCL_SYNC_LINE_LENGTH                      0x1c4c

#define ENCL_SYNC_PIXEL_EN                         0x1c4d

#define ENCL_SYNC_TO_LINE_EN                       0x1c4e

#define ENCL_SYNC_TO_PIXEL                         0x1c4f

#define ENCP_VFIFO2VD_CTL2                         0x1c50

#define VENC_DVI_SETTING_MORE                      0x1c51
#define VENC_VDAC_DAC4_FILT_CTRL0                  0x1c54
#define VENC_VDAC_DAC4_FILT_CTRL1                  0x1c55
#define VENC_VDAC_DAC5_FILT_CTRL0                  0x1c56
#define VENC_VDAC_DAC5_FILT_CTRL1                  0x1c57

#define VENC_VDAC_DAC0_FILT_CTRL0                  0x1c58

#define VENC_VDAC_DAC0_FILT_CTRL1                  0x1c59

#define VENC_VDAC_DAC1_FILT_CTRL0                  0x1c5a

#define VENC_VDAC_DAC1_FILT_CTRL1                  0x1c5b

#define VENC_VDAC_DAC2_FILT_CTRL0                  0x1c5c

#define VENC_VDAC_DAC2_FILT_CTRL1                  0x1c5d

#define VENC_VDAC_DAC3_FILT_CTRL0                  0x1c5e

#define VENC_VDAC_DAC3_FILT_CTRL1                  0x1c5f


#define ENCP_VFIFO2VD_CTL                          0x1b58

#define ENCP_VFIFO2VD_PIXEL_START                  0x1b59

#define ENCP_VFIFO2VD_PIXEL_END                    0x1b5a

#define ENCP_VFIFO2VD_LINE_TOP_START               0x1b5b

#define ENCP_VFIFO2VD_LINE_TOP_END                 0x1b5c

#define ENCP_VFIFO2VD_LINE_BOT_START               0x1b5d

#define ENCP_VFIFO2VD_LINE_BOT_END                 0x1b5e

#define VENC_SYNC_ROUTE                            0x1b60
#define VENC_VIDEO_EXSRC                           0x1b61
#define VENC_DVI_SETTING                           0x1b62
#define VENC_C656_CTRL                             0x1b63
#define VENC_UPSAMPLE_CTRL0                        0x1b64
#define VENC_UPSAMPLE_CTRL1                        0x1b65
#define VENC_UPSAMPLE_CTRL2                        0x1b66

#define TCON_INVERT_CTL                            0x1b67
#define VENC_VIDEO_PROG_MODE                       0x1b68

#define VENC_ENCI_LINE                             0x1b69
#define VENC_ENCI_PIXEL                            0x1b6a
#define VENC_ENCP_LINE                             0x1b6b
#define VENC_ENCP_PIXEL                            0x1b6c

#define VENC_STATA                                 0x1b6d

#define VENC_INTCTRL                               0x1b6e
#define VENC_INTFLAG                               0x1b6f

#define VENC_VIDEO_TST_EN                          0x1b70
#define VENC_VIDEO_TST_MDSEL                       0x1b71
#define VENC_VIDEO_TST_Y                           0x1b72
#define VENC_VIDEO_TST_CB                          0x1b73
#define VENC_VIDEO_TST_CR                          0x1b74
#define VENC_VIDEO_TST_CLRBAR_STRT                 0x1b75
#define VENC_VIDEO_TST_CLRBAR_WIDTH                0x1b76
#define VENC_VIDEO_TST_VDCNT_STSET                 0x1b77

#define VENC_VDAC_DACSEL0                          0x1b78
#define VENC_VDAC_DACSEL1                          0x1b79
#define VENC_VDAC_DACSEL2                          0x1b7a
#define VENC_VDAC_DACSEL3                          0x1b7b
#define VENC_VDAC_DACSEL4                          0x1b7c
#define VENC_VDAC_DACSEL5                          0x1b7d
#define VENC_VDAC_SETTING                          0x1b7e
#define VENC_VDAC_TST_VAL                          0x1b7f
#define VENC_VDAC_DAC0_GAINCTRL                    0x1bf0
#define VENC_VDAC_DAC0_OFFSET                      0x1bf1
#define VENC_VDAC_DAC1_GAINCTRL                    0x1bf2
#define VENC_VDAC_DAC1_OFFSET                      0x1bf3
#define VENC_VDAC_DAC2_GAINCTRL                    0x1bf4
#define VENC_VDAC_DAC2_OFFSET                      0x1bf5
#define VENC_VDAC_DAC3_GAINCTRL                    0x1bf6
#define VENC_VDAC_DAC3_OFFSET                      0x1bf7
#define VENC_VDAC_DAC4_GAINCTRL                    0x1bf8
#define VENC_VDAC_DAC4_OFFSET                      0x1bf9
#define VENC_VDAC_DAC5_GAINCTRL                    0x1bfa
#define VENC_VDAC_DAC5_OFFSET                      0x1bfb
#define VENC_VDAC_FIFO_CTRL                        0x1bfc
#define ENCL_TCON_INVERT_CTL                       0x1bfd

/*********************************
 *ENCI
 *********************************
 */
#define ENCP_VIDEO_EN                              0x1b80
#define ENCP_VIDEO_SYNC_MODE                       0x1b81
#define ENCP_MACV_EN                               0x1b82
#define ENCP_VIDEO_Y_SCL                           0x1b83
#define ENCP_VIDEO_PB_SCL                          0x1b84
#define ENCP_VIDEO_PR_SCL                          0x1b85
#define ENCP_VIDEO_SYNC_SCL                        0x1b86
#define ENCP_VIDEO_MACV_SCL                        0x1b87
#define ENCP_VIDEO_Y_OFFST                         0x1b88
#define ENCP_VIDEO_PB_OFFST                        0x1b89
#define ENCP_VIDEO_PR_OFFST                        0x1b8a
#define ENCP_VIDEO_SYNC_OFFST                      0x1b8b
#define ENCP_VIDEO_MACV_OFFST                      0x1b8c

#define ENCP_VIDEO_MODE                            0x1b8d
#define ENCP_VIDEO_MODE_ADV                        0x1b8e

#define ENCP_DBG_PX_RST                            0x1b90
#define ENCP_DBG_LN_RST                            0x1b91
#define ENCP_DBG_PX_INT                            0x1b92
#define ENCP_DBG_LN_INT                            0x1b93

#define ENCP_VIDEO_YFP1_HTIME                      0x1b94
#define ENCP_VIDEO_YFP2_HTIME                      0x1b95
#define ENCP_VIDEO_YC_DLY                          0x1b96
#define ENCP_VIDEO_MAX_PXCNT                       0x1b97
#define ENCP_VIDEO_HSPULS_BEGIN                    0x1b98
#define ENCP_VIDEO_HSPULS_END                      0x1b99
#define ENCP_VIDEO_HSPULS_SWITCH                   0x1b9a
#define ENCP_VIDEO_VSPULS_BEGIN                    0x1b9b
#define ENCP_VIDEO_VSPULS_END                      0x1b9c
#define ENCP_VIDEO_VSPULS_BLINE                    0x1b9d
#define ENCP_VIDEO_VSPULS_ELINE                    0x1b9e
#define ENCP_VIDEO_EQPULS_BEGIN                    0x1b9f
#define ENCP_VIDEO_EQPULS_END                      0x1ba0
#define ENCP_VIDEO_EQPULS_BLINE                    0x1ba1
#define ENCP_VIDEO_EQPULS_ELINE                    0x1ba2
#define ENCP_VIDEO_HAVON_END                       0x1ba3
#define ENCP_VIDEO_HAVON_BEGIN                     0x1ba4
#define ENCP_VIDEO_VAVON_ELINE                     0x1baf
#define ENCP_VIDEO_VAVON_BLINE                     0x1ba6
#define ENCP_VIDEO_HSO_BEGIN                       0x1ba7
#define ENCP_VIDEO_HSO_END                         0x1ba8
#define ENCP_VIDEO_VSO_BEGIN                       0x1ba9
#define ENCP_VIDEO_VSO_END                         0x1baa
#define ENCP_VIDEO_VSO_BLINE                       0x1bab
#define ENCP_VIDEO_VSO_ELINE                       0x1bac
#define ENCP_VIDEO_SYNC_WAVE_CURVE                 0x1bad
#define ENCP_VIDEO_MAX_LNCNT                       0x1bae
#define ENCP_VIDEO_SY_VAL                          0x1bb0
#define ENCP_VIDEO_SY2_VAL                         0x1bb1
#define ENCP_VIDEO_BLANKY_VAL                      0x1bb2
#define ENCP_VIDEO_BLANKPB_VAL                     0x1bb3
#define ENCP_VIDEO_BLANKPR_VAL                     0x1bb4
#define ENCP_VIDEO_HOFFST                          0x1bb5
#define ENCP_VIDEO_VOFFST                          0x1bb6
#define ENCP_VIDEO_RGB_CTRL                        0x1bb7
#define ENCP_VIDEO_FILT_CTRL                       0x1bb8
#define ENCP_VIDEO_OFLD_VPEQ_OFST                  0x1bb9
#define ENCP_VIDEO_OFLD_VOAV_OFST                  0x1bba
#define ENCP_VIDEO_MATRIX_CB                       0x1bbb
#define ENCP_VIDEO_MATRIX_CR                       0x1bbc
#define ENCP_VIDEO_RGBIN_CTRL                      0x1bbd

#define ENCP_MACV_BLANKY_VAL                       0x1bc0
#define ENCP_MACV_MAXY_VAL                         0x1bc1
#define ENCP_MACV_1ST_PSSYNC_STRT                  0x1bc2
#define ENCP_MACV_PSSYNC_STRT                      0x1bc3
#define ENCP_MACV_AGC_STRT                         0x1bc4
#define ENCP_MACV_AGC_END                          0x1bc5
#define ENCP_MACV_WAVE_END                         0x1bc6
#define ENCP_MACV_STRTLINE                         0x1bc7
#define ENCP_MACV_ENDLINE                          0x1bc8
#define ENCP_MACV_TS_CNT_MAX_L                     0x1bc9
#define ENCP_MACV_TS_CNT_MAX_H                     0x1bca
#define ENCP_MACV_TIME_DOWN                        0x1bcb
#define ENCP_MACV_TIME_LO                          0x1bcc
#define ENCP_MACV_TIME_UP                          0x1bcd
#define ENCP_MACV_TIME_RST                         0x1bce

#define ENCP_VBI_CTRL                              0x1bd0
#define ENCP_VBI_SETTING                           0x1bd1
#define ENCP_VBI_BEGIN                             0x1bd2
#define ENCP_VBI_WIDTH                             0x1bd3
#define ENCP_VBI_HVAL                              0x1bd4
#define ENCP_VBI_DATA0                             0x1bd5
#define ENCP_VBI_DATA1                             0x1bd6

/*********************************
 *Video post-processing:  VPP_VCBUS_BASE = 0x1d
 *********************************
 */
#define VPP2_MISC                                  0x1926

#define VPP_MISC                                   0x1d26

#define VPP2_POSTBLEND_H_SIZE                      0x1921
#define VPP_POSTBLEND_H_SIZE                       0x1d21
/*Bit 3	minus black level enable for vadj2
 *Bit 2	Video adjustment enable for vadj2
 *Bit 1	minus black level enable for vadj1
 *Bit 0	Video adjustment enable for vadj1
 */
#define VPP_VADJ_CTRL                              0x1d40
/*Bit 16:8  brightness, signed value
 *Bit 7:0	contrast, unsigned value, contrast from  0 <= contrast <2
 */
#define VPP_VADJ1_Y                                0x1d41
/*cb' = cb*ma + cr*mb
 *cr' = cb*mc + cr*md
 *all are bit 9:0, signed value, -2 < ma/mb/mc/md < 2
 */
#define VPP_VADJ1_MA_MB                            0x1d42
#define VPP_VADJ1_MC_MD                            0x1d43
/*Bit 16:8  brightness, signed value
 *Bit 7:0   contrast, unsigned value, contrast from  0 <= contrast <2
 */
#define VPP_VADJ2_Y                                0x1d44
/*cb' = cb*ma + cr*mb
 *cr' = cb*mc + cr*md
 *all are bit 9:0, signed value, -2 < ma/mb/mc/md < 2
 */
#define VPP_VADJ2_MA_MB                            0x1d45
#define VPP_VADJ2_MC_MD                            0x1d46

#define PERIPHS_PIN_MUX_0                          0x202c
/*********************************
 *VPU:  VPU_VCBUS_BASE = 0x27
 **********************************/
/* [31:11] Reserved.
 * [10: 8] cntl_viu_vdin_sel_data. Select VIU to VDIN data path,
 *   must clear it first before changing the path selection:
 *          3'b000=Disable VIU to VDIN path;
 *          3'b001=Enable VIU of ENC_I domain to VDIN;
 *          3'b010=Enable VIU of ENC_P domain to VDIN;
 *          3'b100=Enable VIU of ENC_T domain to VDIN;
 * [ 6: 4] cntl_viu_vdin_sel_clk. Select which clock to VDIN path,
 *   must clear it first before changing the clock:
 *          3'b000=Disable VIU to VDIN clock;
 *          3'b001=Select encI clock to VDIN;
 *          3'b010=Select encP clock to VDIN;
 *          3'b100=Select encT clock to VDIN;
 * [ 3: 2] cntl_viu2_sel_venc. Select which one of the encI/P/T
 *   that VIU2 connects to:
 *         0=ENCL, 1=ENCI, 2=ENCP, 3=ENCT.
 * [ 1: 0] cntl_viu1_sel_venc. Select which one of the encI/P/T
 *   that VIU1 connects to:
 *0=ENCL, 1=ENCI, 2=ENCP, 3=ENCT.
 */
#define VPU_VIU_VENC_MUX_CTRL                      0x271a

/*Bit  6 RW, gclk_mpeg_vpu_misc
 *Bit  5 RW, gclk_mpeg_venc_l_top
 *Bit  4 RW, gclk_mpeg_vencl_int
 *Bit  3 RW, gclk_mpeg_vencp_int
 *Bit  2 RW, gclk_mpeg_vi2_top
 *Bit  1 RW, gclk_mpeg_vi_top
 *Bit  0 RW, gclk_mpeg_venc_p_top
 */
#define VPU_CLK_GATE                               0x2723

#define VPU_VLOCK_CTRL                             0x3000
#define VPU_VLOCK_ADJ_EN_SYNC_CTRL                 0x301d
#define VPU_VLOCK_GCLK_EN                          0x301e
/* ******************************** */

extern unsigned int cvbs_out_reg_read(unsigned int _reg);
extern void cvbs_out_reg_write(unsigned int _reg, unsigned int _value);
extern void cvbs_out_reg_setb(unsigned int reg, unsigned int value,
		unsigned int _start, unsigned int _len);
extern unsigned int cvbs_out_reg_getb(unsigned int reg,
		unsigned int _start, unsigned int _len);
extern void cvbs_out_reg_set_mask(unsigned int reg, unsigned int _mask);
extern void cvbs_out_reg_clr_mask(unsigned int reg, unsigned int _mask);

extern unsigned int cvbs_out_hiu_read(unsigned int _reg);
extern void cvbs_out_hiu_write(unsigned int _reg, unsigned int _value);
extern void cvbs_out_hiu_setb(unsigned int _reg, unsigned int _value,
		unsigned int _start, unsigned int _len);
extern unsigned int cvbs_out_hiu_getb(unsigned int _reg,
		unsigned int _start, unsigned int _len);
extern void cvbs_out_hiu_set_mask(unsigned int _reg, unsigned int _mask);
extern void cvbs_out_hiu_clr_mask(unsigned int _reg, unsigned int _mask);

#endif
