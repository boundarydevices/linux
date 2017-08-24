/*------------------------------------------------------------------------------
 * Copyright © 2002-2005, Silicon Image, Inc.  All rights reserved.
 *
 * No part of this work may be reproduced, modified, distributed, transmitted,
 * transcribed, or translated into any language or computer format, in any form
 * or by any means without written permission of: Silicon Image, Inc.,
 * 1060 East Arques Avenue, Sunnyvale, California 94085
 *----------------------------------------------------------------------------
 */

#ifndef SII_HDMIRX
#define SII_HDMIRX

#define RX_SLV0 0x60
#define RX_SLV1 0x68
#define RX_AFE0 0x64
#define RX_AFE1 0x6C

/*-----------------------------------------------------
 * Sys. registers
 *---------------------------------------------------
 */
#define RX_VND_IDL_ADDR     0x00
#define RX_VND_IDH_ADDR     0x01
#define RX_DEV_IDL_ADDR     0x02
#define RX_DEV_IDH_ADDR     0x03
#define RX_DEV_REV_ADDR     0x04
#define RX_SWRST_ADDR       0x05

#define RX_BIT_SW_AUTO      0x10

#define RX_SWRST_ADDR2      0x07    /* YMA 9125/35 only */
#define RX_BIT_AUDIOFIFO_AUTO    0x40  /* YMA 9135 only, auto audio FIFO rst */
#define RX_BIT_DCFIFO_RST        0x80  /* YMA 9135 only, Deep color FIFO rst */


#define RX_STATE_ADDR       0x06

#define RX_BIT_CABLE_HPD    0x08
#define RX_BIT_SCDT 0x01
#define RX_BIT_CLK  0x02

#define RX_SYS_CTRL1_ADDR   0x08

#define RX_BIT_INVERT_OUTPUT_VID_CLK 0x02
#define RX_BIT_VID_BUS_24BIT    0x04

#define RX_SYS_SW_SWTCHC_ADDR    0x09

#define RX_BIT_RX0_EN       0x01
#define RX_BIT_RX1_EN       0x02
#define RX_BIT_DDC0_EN      0x10
#define RX_BIT_DDC1_EN      0x20

#define RX_BIT_AUDIO_FIFO_RESET 0x02
#define RX_BIT_ACR_RESET        0x04

#define RX_BIT_PD_ALL 0x01
#define RX_BIT_2PIX_MODE 0x10/* 0x08 */

#define RX_MSK_CLR_IN_OUT_DIV 0x0F

#define RX_OUTPUT_DIV_BY_1 0x00
#define RX_OUTPUT_DIV_BY_2 0x40
#define RX_OUTPUT_DIV_BY_4 0xC0

/* PD Output */

#define RX_PD_SYS_ADDR              0x3F
#define RX_BIT_NPD_VIDEO_DAC         0x10

#define RX_PD_SYS2_ADDR             0x3E

#define RX_FIFO_DIFF_ADDR 0x39
#define RX_BIT_NPD_DIGITAL_OUTPUTS   0x04

/* STATE MASK */

#define  RX_STATE_SCDT_MASK 0x01

/*-----------------------------------------------------
 * Analog Front End
 *----------------------------------------------------
 */
#define RX_AFE_DDR_CONF_ADDR     0x0A
#define RX_AFE_DDR_DSA_ADDR      0x0B

/*-----------------------------------------------------
 * HDCP registers
 *---------------------------------------------------
 */
#define RX_RI_PRIME_ADDR 0x1F
#define RX_HDCP_CTRL_ADDR  0x31

#define RX_BIT_TRASH_RI 0x80

#define RX_HDCP_STAT_ADDR  0x32

#define RX_BIT_HDCP_DECRYPTING 0x20

#define RX_AKSV_ADDR 0x21
#define RX_BKSV_ADDR 0x1A
#define RX_CAP_MASK 0x08


/*-----------------------------------------------------
 * Interrupt registers
 *---------------------------------------------------
 */
#define RX_INT_ADDR 0x71
#define RX_INT_ST2_ADDR 0x72
#define RX_INT_ST4_ADDR 0x74
#define RX_INT_MASK_ADDR 0x75

#define RX_INT_CNTRL_ADDR 0x79
#define RX_INT_MASK_ST5_ADDR 0x7D
/* reg 0x71 bit defenition */

#define RX_BIT_CTS_CHANGED 0x80
#define RX_BIT_N_CHANGED 0x40
#define RX_BIT_ECC_ERR 0x04
#define RX_BIT_HDCP_DONE 0x01
#define RX_BIT_HDCP_START 0x02

#define RX_BIT_FIFO_UNDERRUN 0x01
#define RX_BIT_FIFO_OVERRUN 0x02
#define RX_BIT_CTS_DROPPED   0x08
#define RX_BIT_CTS_REUSED    0x04

/* reg 0x72 bit defenition */
#define RX_BIT_HDMI_CHANGED  0x80
#define RX_BIT_SCDT_CHANGED   0x08

/*-----------------------------------------------------
 *  ECC error threshold, aux. reg. for ECC int. audio path
 *---------------------------------------------------
 */
#define RX_ECC_CTRL_ADDR 0xBB
#define RX_BIT_CLR_BCH_COUNTER 0x01

#define RX_ECC_BCH_THRESHOLD 0xBC

#define RX_ECC_THRES_ADDR 0x7C
#define RX_ECC_THRES_PCMREPR 0x01
#define RX_ECC_THRES_CSTREAMREPR 0x03
#define RX_HDCP_ERR_ADDR 0xCD
/*-----------------------------------------------------
 * TMDS PLL registers
 *---------------------------------------------------
 */


/* Audio PLL registers */
/*---------------------------------------------------*/
#define RX_APLL_CTRL1_ADDR 0x88
#define RX_PLL_IOPPC_ADDR 0x8C
#define RX_PLL_CGC_ADDR 0x8D
/*-----------------------------------------------------
 * Mail Box registers
 *---------------------------------------------------
 */

/*-----------------------------------------------------
 * ACR registers
 *---------------------------------------------------
 */
#define RX_ACR_CTRL1_ADDR 0x00
#define RX_FREQ_SVAL_ADDR 0x02

#define RX_HW_N_ADDR 0x06
#define RX_HW_CTS_ADDR 0x0C

#define RX_BIT_ACR_INIT 0x01
#define RX_BIT_ACR_FSSEL 0x02

/*-----------------------------------------------------
 * Audio out registers
 *---------------------------------------------------
 */
#define RX_I2S_CTRL1_ADDR 0x26
#define RX_I2S_CTRL2_ADDR 0x27
#define RX_I2S_MAP_ADDR 0x28
#define RX_AUDIO_CTRL_ADDR 0x29

#define RX_I2S_MAP_DEFAULT 0xE4
#define RX_SMOOTH_MUTE_EN 0x20

#define RX_BIT_SPDIF_EN 0x01
#define RX_BIT_I2S_MODE 0x04
#define RX_BIT_PASS_SPDIF_ERR 0x10

#define RX_BIT_Aout_WordSize 0x20  /* YMA 2 added for API Rev2 */
#define RX_BIT_Aout_ClockEdge 0x40
#define RX_BIT_SD0_EN 0x10
#define RX_BIT_SD1_EN 0x20
#define RX_BIT_SD2_EN 0x40
#define RX_BIT_SD3_EN 0x80

#define RX_BITS_SD0_SD3_EN 0xF0

#define RX_I2S_DISABLE 0x0F
/*-----------------------------------------------------
 * Audio path registers
 *---------------------------------------------------
 */
#define RX_CH_STATUS1_ADDR 0x2A

#define RX_BIT_AUDIO_SAMPLE_NPCM 0x02

#define RX_CH_STATUS4_ADDR 0x30
#define RX_CH_STATUS5_ADDR 0x31

#define RX_AUDP_STAT_ADDR 0x34

/* reg address P1, 3D */
#define RX_BIT_LAYOUT_1 0x08
#define RX_BIT_DSD_STATUS 0x20
#define RX_BIT_HBRA_STATUS 0x40  /* 9135 and after only */
#define RX_BIT_HDMI_EN_MASK 0x02

#define RX_HDMI_CRIT1_ADDR 0x35
#define RX_AUDP_MUTE_ADDR 0x37
#define RX_MUTE_DIV_ADDR 0x2D
#define RX_HDMI_CRIT2_ADDR 0x38
#define RX_HDMI_EN_MASK 0x02

#define BIT_DSD_STATUS 0x20

#define RX_SPDIF_CH_STATUS 0x2A

#define RX_AUDIO_SAMPLE_REPRTYPE 0x02
#define RX_AUDIO_SWAP_ADDR 0x2E
#define BIT_DSD_USES_I2S_SPDIF_BUSES 0x02

#define RX_BIT_MCLK_EN 0x08
#define RX_BIT_VIDEO_MUTE 0x01
#define RX_BIT_AUDIO_MUTE 0x02
/*---------------------------------------------------*/
#define RX_AEC_CTRL_ADDR 0xB5

#define RX_BIT_AEC_EN   0x01

#define RX_AEC1_ADDR 0xB6
#define RX_AEC2_ADDR 0xB7
#define RX_AEC3_ADDR 0xB8




/*-----------------------------------------------------
 * Video registers
 *---------------------------------------------------
 */
#define RX_HRES_L_ADDR 0x3A
#define RX_HRES_H_ADDR 0x3B
#define RX_VRES_L_ADDR 0x3C
#define RX_VRES_H_ADDR 0x3D

#define RX_VID_FORMAT_BASE_ADDR 0x48
#define RX_VID_CTRL_ADDR 0x48
#define RX_VID_MODE_ADDR 0x4A
#define RX_VID_MODE2_ADDR 0x49

/* Defines the video blanking value for channel 0 */
#define RX_BLANKDATA_CH0_ADDR 0x4B
/* Defines the video blanking value for channel 1 */
#define RX_BLANKDATA_CH1_ADDR 0x4C
/* Defines the video blanking value for channel 2 */
#define RX_BLANKDATA_CH2_ADDR 0x4D

#define RX_RES_ACTIVE_ADDR 0x4E
#define RX_DE_PIX1_ADDR 0x4E         /* Number pixels per DE (read only) */
#define RX_DE_PIX2_ADDR 0x4F
#define RX_DE_LINE1_ADDR 0x50  /* Number active lines (read only) */
#define RX_DE_LINE2_ADDR 0x51
#define RX_VID_STAT_ADDR 0x55
#define RX_VID_XCNT_ADDR 0x6F
/*--------------------------------------------------------------------*/
/* CSync Interlace regs */
#define RX_FIELD2BACKPORCH_ADDR 0x54

#define RX_CAPHALFLINE_ADDR  0x57
#define RX_PREEQPULSCNT_ADDR 0x5A
#define RX_SERRATEDCNT_ADDR  0x5D
#define RX_POSTEQPULSECNT_ADDR    0x5E

/*---------------------------------------------------------------------*/
/*   VIDEO MODE BITS  [0x4A] */

#define RX_BIT_EN_INS_CSYNC 0x01
#define RX_BIT_444to422   0x02
#define RX_BIT_422to444   0x04
#define RX_BIT_CSC        0x08
#define RX_BIT_RGBToYCbCr 0x08
#define RX_BIT_16_235_RANGE   0x10
#define RX_BIT_DITHER     0x20
#define RX_BIT_MUX_YC     0x40
#define RX_BIT_INS_SAVEAV 0x80


/*---------------------------------------------------------------------*/
/*   VIDEO MODE2 BITS [0x49] */
#define RX_BIT_RGBA_OUT        0x01
#define RX_BIT_EN_PEDESTAL     0x02
#define RX_BIT_YCbCrToRGB      0x04
#define RX_BIT_YCbCrRangeToRGB 0x08
#define RX_BIT_LATCH_POLARITY  0x10

/*---------------------------------------------------------------------*/
/* VIDEO CONTROL BITS [0x48] */
#define RX_BIT_BT709                       0x01
#define RX_BIT_BT709_RGBCONV               0x01
#define RX_BIT_EXT_BIT_MODE                0x02
#define RX_BIT_BT709_YCbCrCONV             0x04
#define RX_BIT_INSERT_CSYNC_ON_AOUT        0x08
#define RX_BIT_INSERT_SOGorCSYNC_ON_AOUT   0x08
#define RX_BIT_ENCSYNC_ON_HSYNC            0x10
#define RX_BIT_ENCSYNC_ON_VSYNC            0x20
#define RX_BIT_INVERT_HSYNC                0x40
#define RX_BIT_INVERT_VSYNC                0x80

#define RX_MASK_CLR_SYNC_CTRL              0xF8

/*-----------------------------------------------------
 * Test registers
 *---------------------------------------------------
 */
#define RX_DBGCORE_ADDR 0x3A
#define RX_PD_SYS2_ADDR 0x3E

#define RX_BIT_PD_QE 0x08
/*-----------------------------------------------------
 * CEA-861
 *---------------------------------------------------
 */

/*-----------------------------------------------------
 * Software resets
 *---------------------------------------------------
 */
#define RX_SW_RST 0x01
#define RX_FIFO_RST 0x02
#define RX_ACR_RST 0x04
#define RX_HDCP_RST 0x10
/*-----------------------------------------------------
 * Deep Color Registers
 *---------------------------------------------------
 */

/* #define RX_VID_MODE2_ADDR 0x49 */
#define RX_DITHER_MODE_MASK     0xC0
    #define RX_DITHER_8BITS         0x00
    #define RX_DITHER_10BITS        0x40
    #define RX_DITHER_12BITS        0x80


#define RX_DC_STATUS_ADDR       0x61
    #define RX_DC_PIXELDEPTH_MASK   0x03
    #define RX_DC_8BPP_VAL          0x00
    #define RX_DC_10BPP_VAL         0x01
    #define RX_DC_12BPP_VAL         0x02

/* #define RX_TMDS_ECTRL_ADDR    0x81 */
    #define RX_DC_CLK_CTRL_MASK     0x0F
    #define RX_DC_CLK_8BPP_1X       0x00
    #define RX_DC_CLK_8BPP_2X       0x02
    #define RX_DC_CLK_10BPP_1X      0x04
    #define RX_DC_CLK_12BPP_1X      0x05
    #define RX_DC_CLK_10BPP_2X      0x06
    #define RX_DC_CLK_12BPP_2X      0x07

/*-----------------------------------------------------
 * Interrupt registers
 *---------------------------------------------------
 */
#define RX_INTR_STATE_ADDR 0x70
#define RX_BIT_GLOBAL_INTR 0x01

#define RX_HDMI_INT_ADDR 0x71
#define RX_HDMI_INT_GR1_ADDR 0x71
#define RX_HDMI_INT_GR2_ADDR 0x7B
#define RX_HDMI_INT_ST1_ADDR 0x71
#define RX_HDMI_INT_ST2_ADDR 0x72
#define RX_HDMI_INT_ST3_ADDR 0x73
#define RX_HDMI_INT_ST4_ADDR 0x74

#define RX_INFO_FRAME_CTRL_ADDR 0x7A

#define RX_HDMI_INT_ST5_ADDR 0x7B
#define RX_HDMI_INT_ST6_ADDR 0x7C

#define RX_HDMI_INT_MASK_ADDR 0x75
#define RX_HDMI_INT_GR1_MASK_ADDR 0x75
#define RX_HDMI_INT_ST1_MASK_ADDR 0x75
#define RX_HDMI_INT_ST2_MASK_ADDR 0x76
#define RX_HDMI_INT_ST3_MASK_ADDR 0x77
#define RX_HDMI_INT_ST4_MASK_ADDR 0x78

#define RX_BIT_MASK_NO_AVI_INTR 0x10

#define RX_HDMI_INT_MASK_ADDR 0x75
#define RX_HDMI_INT_GR2_MASK_ADDR 0x7D

#define RX_INT_CNTRL_ADDR 0x79

/* reg 0x71 bit defenitions */

#define RX_BIT_HDCP_START   0x02
#define RX_BIT_HDCP_DONE    0x01
#define RX_BIT_ECC_ERR      0x04
#define RX_BIT_ACR_PLL_UNLOCK  0x10
#define RX_BIT_N_CHANGED    0x40
#define RX_BIT_CTS_CHANGED  0x80

/* reg 0x72 bit defenitions */

#define RX_BIT_GOT_AUDIO_PKT 0x02
#define RX_BIT_SCDT_CHANGE   0x08

#define RX_BIT_GOT_CTS_PKT   0x04
#define RX_BIT_VSYNC         0x40
#define RX_BIT_HDMI_CHANGED  0x80

/* reg 0x73 bit defenitions */

#define RX_BIT_NEW_CP_PACKET 0x80

/* reg 0x74 bit defenitions */

#define RX_BIT_FIFO_UNDERRUN 0x01
#define RX_BIT_FIFO_OVERRUN  0x02
#define RX_BIT_CTS_DROPPED   0x08
#define RX_BIT_CTS_REUSED    0x04
#define RX_BIT_HDCP_FAILED   0x40

/* reg 0x7A bit defenitions */

#define RX_BIT_NEW_AVI 0x01
#define RX_BIT_NEW_SPD 0x02
#define RX_BIT_NEW_AUD 0x04
#define RX_BIT_NEW_MPEG 0x08
#define RX_BIT_NEW_UNREQ 0x10
#define RX_BIT_NEW_ACP 0x20

/* reg 0x7B */
#define RX_BIT_INTERLACE_CHANGE 0x02
#define RX_BIT_AAC_DONE 0x40
#define RX_BIT_AUDIO_LINK_ERR 0x20
#define RX_BIT_HRES_CHANGE 0x08
#define RX_BIT_VRES_CHANGE 0x10

/* reg 0x7C */
#define RX_BIT_DC_ERROR   0x80
#define RX_BIT_ACP_PACKET 0x04
#define BIT_AUD_CH_STATUS_READY 0x20
#define BIT_DSD_ON              0x02
#define BIT_HBRA_ON_CHANGED     0x10

#define ST3_INFO_MASK 0x1F
#define ST4_INFO_MASK 0x10
/*-----------------------------------------------------
 * TMDS PLL registers
 *---------------------------------------------------
 */
#define RX_TMDS_ECTRL_ADDR 0x81
#define RX_TMDS_TERMCTRL_ADDR 0x82
/*-----------------------------------------------------
 * Audio PLL registers
 *---------------------------------------------------
 */
#define RX_APLL_POLE_ADDR 0x88
#define RX_APLL_CLIP_ADDR 0x89
#define RX_PLL_IOPPC_ADDR 0x8C
#define RX_PLL_CGC_ADDR 0x8D

#define RX_LK_WIN_SVAL_ADDR 0x13
#define RX_LK_THRESHOLD_ADDR 0x14

/*-----------------------------------------------------
 * Mail Box registers
 *---------------------------------------------------
 */

/*-----------------------------------------------------
 * ACR registers
 *---------------------------------------------------
 */
#define RX_CTS_THRESHOLD 0x50 /* 6-3 Bits */

#define RX_ACR_CTRL1_ADDR 0x00
#define RX_FREQ_SVAL_ADDR 0x02
#define RX_HW_N_VAL_ADDR  0x06
#define RX_HW_CTS_ADDR 0x0C
#define RX_PCLK_FS_ADDR 0x17
#define RX_ACR_CTRL3_ADDR 0x18

#define RX_BIT_MCLK_LOOPBACK 0x04
#define RX_BIT_ACR_INIT 0x01

/*-----------------------------------------------------
 * Audio Exception control  registers
 *---------------------------------------------------
 */
#define RX_AEC_CTRL_ADDR 0xB5
#define RX_AEC1_ADDR 0xB6
#define RX_AEC2_ADDR 0xB7
#define RX_AEC3_ADDR 0xB8
/*---------------------------------------------------*/



#endif

