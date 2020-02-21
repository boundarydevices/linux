/* SPDX-License-Identifier: GPL-2.0 */
/*
 * tc358840_regs.h - Toshiba UH2C/D HDMI-CSI bridge registers
 *
 * Copyright (c) 2015, Armin Weiss <weii@zhaw.ch>
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
 * Copyright 2015-2020 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 */

#ifndef __TC358840_REGS__
#define __TC358840_REGS__

/**************************************************
 * Register Addresses
 *************************************************/

/* *** General (16 bit) *** */
#define CHIPID_ADDR				0x0000
#define MASK_CHIPID				0xff00
#define MASK_REVID				0x00ff
#define TC358840_CHIPID				0x4700

#define SYSCTL					0x0002
#define MASK_SLEEP				BIT(0)
#define MASK_I2SDIS				BIT(7)
#define MASK_HDMIRST				BIT(8)
#define MASK_CTXRST				BIT(9)
#define MASK_CECRST				BIT(10)
#define MASK_IRRST				BIT(11)
#define MASK_SPLRST				BIT(12)
#define MASK_ABRST				BIT(14)
#define MASK_RESET_ALL				0x5f80

#define CONFCTL0				0x0004
#define MASK_VTX0EN				BIT(0)
#define MASK_VTX1EN				BIT(1)
#define MASK_AUTOINDEX				BIT(2)
#define MASK_AUDOUTSEL_CSITX0			(0 << 3)
#define MASK_AUDOUTSEL_CSITX1			(1 << 3)
#define MASK_AUDOUTSEL_I2S			(2 << 3)
#define MASK_AUDOUTSEL_TDM			(3 << 3)
#define MASK_ABUFEN				BIT(5)
#define MASK_YCBCRFMT				(3 << 6)
#define MASK_YCBCRFMT_YCBCR444			(0 << 6)
#define MASK_YCBCRFMT_YCBCR422_12		(1 << 6)
#define MASK_YCBCRFMT_VPID2			(2 << 6)
#define MASK_YCBCRFMT_YCBCR422_8		(3 << 6)
#define MASK_I2SDLYOPT				BIT(8)
#define MASK_AUDCHSEL				BIT(9)
#define MASK_AUDCHNUM_8				(0 << 10)
#define MASK_AUDCHNUM_6				(1 << 10)
#define MASK_AUDCHNUM_4				(2 << 10)
#define MASK_AUDCHNUM_2				(3 << 10)
#define MASK_ACLKOPT				BIT(12)
#define MASK_IECEN				BIT(13)
#define MASK_SLMBEN				BIT(14)
#define MASK_TX_MSEL				BIT(15)

#define CONFCTL1				0x0006
#define MASK_TX_OUT_FMT				0x0003
#define MASK_TX_OUT_FMT_RGB888			(0 << 0)
#define MASK_TX_OUT_FMT_RGB666			(2 << 0)
#define MASK_TX_MS_EN				BIT(2)

/* *** Interrupt (16 bit) *** */
#define INTSTATUS				0x0014
#define INTMASK					0x0016
#define MASK_IR_DINT				BIT(0)
#define MASK_IR_EINT				BIT(1)
#define MASK_CEC_RINT				BIT(2)
#define MASK_CEC_TINT				BIT(3)
#define MASK_CEC_EINT				BIT(4)
#define MASK_SYS_INT				BIT(5)
#define MASK_CSITX0_INT				BIT(8)
#define MASK_HDMI_INT				BIT(9)
#define MASK_AMUTE_INT				BIT(10)
#define MASK_CSITX1_INT				BIT(11)
#define MASK_INT_STATUS_MASK_ALL		0x0f3f

/* *** Audio pull up or down (16 bit) *** */
#define I2S_PUDCTL				0x0084
#define MASK_A_SD_3_PULL_UP			BIT(13)
#define MASK_A_SD_3_PULL_DOWN			BIT(12)
#define MASK_A_SD_2_PULL_UP			BIT(11)
#define MASK_A_SD_2_PULL_DOWN			BIT(10)
#define MASK_A_SD_1_PULL_UP			BIT(9)
#define MASK_A_SD_1_PULL_DOWN			BIT(8)
#define MASK_A_SD_0_PULL_UP			BIT(7)
#define MASK_A_SD_0_PULL_DOWN			BIT(6)
#define MASK_A_WFS_PULL_UP			BIT(5)
#define MASK_A_WFS_PULL_DOWN			BIT(4)
#define MASK_A_SCK_PULL_UP			BIT(3)
#define MASK_A_SCK_PULL_DOWN			BIT(2)
#define MASK_A_OSCK_PULL_UP			BIT(1)
#define MASK_A_OSCK_PULL_DOWN			BIT(0)

/* *** Interrupt and MASKs (8 bit) *** */
#define HDMI_INT0				0x8500
#define MASK_KEY				0x01
#define MASK_MISC				0x0002

#define HDMI_INT1				0x8501
#define MASK_SYS				0x01
#define MASK_CLK				0x02
#define MASK_PACKET				0x04
#define MASK_ACBIT				0x08
#define MASK_AUD				0x10
#define MASK_ERR				0x20
#define MASK_HDCP				0x40
#define MASK_GBD				0x80

#define SYS_INT					0x8502
#define SYS_INTM				0x8512
#define MASK_DDC				0x01
#define MASK_TMDS				0x02
#define MASK_DPMBDET				0x04
#define MASK_NOPMBDET				0x08
#define MASK_HDMI				0x10
#define MASK_DVI				0x20
#define MASK_ACRN				0x40
#define MASK_ACR_CTS				0x80

#define CLK_INT					0x8503
#define CLK_INTM				0x8513
#define MASK_TMDSCLK_CHG			0x01
#define MASK_PHYCLK_CHG				0x02
#define MASK_PXCLK_CHG				0x04
#define MASK_DC_CHG				0x08
#define MASK_IN_HV_CHG				0x10
#define MASK_IN_DE_CHG				0x20
#define MASK_OUT_H_CHG				0x40
#define MASK_OUT_DE_CHG				0x80

#define PACKET_INT				0x8504
#define PACKET_INTM				0x8514
#define MASK_PK_AVI				0x01
#define MASK_PK_AUD				0x02
#define MASK_PK_MS				0x04
#define MASK_PK_SPD				0x08
#define MASK_PK_VS				0x10
#define MASK_PK_ACP				0x20
#define MASK_PK_ISRC				0x40
#define MASK_PK_ISRC2				0x80

#define CBIT_INT				0x8505
#define CBIT_INTM				0x8515
#define MASK_CBIT				0x01
#define MASK_CBIT_FS				0x02
#define MASK_CBIT_NLPCM				0x04
#define MASK_AU_HBR				0x08
#define MASK_AU_DSD				0x10
#define MASK_AF_UNLOCK				0x40
#define MASK_AF_LOCK				0x80

#define AUDIO_INT				0x8506
#define AUDIO_INTM				0x8516
#define MASK_BUFINIT_END			0x01
#define MASK_BUF_UNDER				0x02
#define MASK_BUF_NU2				0x04
#define MASK_BUF_NU1				0x08
#define MASK_BUF_CENTER				0x10
#define MASK_BUF_NO1				0x20
#define MASK_BUF_NO2				0x40
#define MASK_BUF_OVER				0x80

#define ERR_INT					0x8507
#define ERR_INTM				0x8517
#define MASK_DC_PPERR				0x01
#define MASK_DC_BUFERR				0x02
#define MASK_DC_DEERR				0x04
#define MASK_DC_NOCD				0x08
#define MASK_NO_AVI				0x10
#define MASK_NO_ACP				0x20
#define MASK_AU_FRAME				0x40
#define MASK_EESS_ERR				0x80

#define HDCP_INT				0x8508
#define HDCP_INTM				0x8518
#define MASK_AN_END				0x01
#define MASK_AKSV_END				0x02
#define MASK_KM_END				0x04
#define MASK_R0_END				0x08
#define MASK_SHA_END				0x10
#define MASK_LINKERR				0x20
#define MASK_AVM_CLR				0x40
#define MASK_AVM_SET				0x80

#define GBD_INT					0x8509
#define GBD_INTM				0x8519
#define MASK_GBD_ON				0x01
#define MASK_GBD_OFF				0x02
#define MASK_P1GBD_DET				0x04
#define MASK_P0GBD_CHG				0x10
#define MASK_P1GBD_CHG				0x20
#define MASK_GBD_ACLR				0x40
#define MASK_GBD_PKERR				0x80

#define MISC_INT				0x850b
#define MISC_INTM				0x851b
#define MASK_AUDIO_MUTE				0x01
#define MASK_SYNC_CHG				0x02
#define MASK_NO_VS				0x04
#define MASK_NO_SPD				0x08
#define MASK_AS_LAYOUT				0x10
#define MASK_VIDEO_COLOR			0x20
#define MASK_AU_HBR_OFF				0x40
#define MASK_AU_DSD_OFF				0x80

/* *** STATUS *** */
#define SYS_STATUS				0x8520
#define MASK_S_SYNC				0x80
#define MASK_S_AVMUTE				0x40
#define MASK_S_HDCP				0x20
#define MASK_S_HDMI				0x10
#define MASK_S_PHY_SCDT				0x08
#define MASK_S_PHY_PLL				0x04
#define MASK_S_TMDS				0x02
#define MASK_S_DDC5V				0x01

#define VI_STATUS1				0x8522
#define MASK_S_V_GBD				0x08
#define MASK_S_DEEPCOLOR			0x0c
#define MASK_S_V_422				0x02
#define MASK_S_V_INTERLACE			0x01

#define VI_STATUS3				0x8528
#define MASK_S_V_COLOR				0x1f
#define MASK_RGB_FULL				0x00
#define MASK_RGB_LIMITED			0x01
#define MASK_YCBCR601_FULL			0x02
#define MASK_YCBCR601_LIMITED			0x03
#define MASK_OPRGB_FULL				0x04
#define MASK_OPRGB_LIMITED			0x05
#define MASK_YCBCR709_FULL			0x06
#define MASK_YCBCR709_LIMITED			0x07
#define MASK_XVYCC601_FULL			0x0a
#define MASK_XVYCC601_LIMITED			0x0b
#define MASK_XVYCC709_FULL			0x0e
#define MASK_XVYCC709_LIMITED			0x0f
#define MASK_SYCC601_FULL			0x12
#define MASK_SYCC601_LIMITED			0x13
#define MASK_OPYCC601_FULL			0x1a
#define MASK_OPYCC601_LIMITED			0x1b
#define MASK_LIMITED                            0x01

/* *** CSI TX (32 bit) *** */
#define CSITX0_BASE_ADDR			0x0000
#define CSITX1_BASE_ADDR			0x0200

#define CSITX_CLKEN				0x0108
#define MASK_CSITX_EN				BIT(0)

#define PPICLKEN				0x010c
#define MASK_HSTXCLKEN				0x00000001

#define MODECONF				0x0110	/* Not in Ref. v1.5 */
#define MASK_CSI2MODE				BIT(0)
#define MASK_VSYNC_POL_SW			BIT(1)
#define MASK_HSYNC_POL_SW			BIT(2)
#define MASK_DTVALID_POL_SW			BIT(3)
#define MASK_INDMODE				BIT(4)

#define LANEEN					0x0118
#define MASK_LANES				0x00000007
#define MASK_LANE_0_EN				(1 << 0)
#define MASK_LANE_0_1_EN			(2 << 0)
#define MASK_LANE_0_1_2_EN			(3 << 0)
#define MASK_LANE_0_1_2_3_EN			(4 << 0)
#define MASK_LANES				0x00000007
#define MASK_CLANEEN				BIT(4)

#define CSITX_START				0x011c
#define LINEINITCNT				0x0120
#define HSTOCNT					0x0124

#define FUNCMODE				0x0150
#define MASK_CONTCLKMODE			BIT(5)
#define MASK_FORCESTOP				BIT(10)

#define CSITX_INTERNAL_STAT			0x01B0
#define APF_VHIF_IND_STAT			BIT(8)
#define APF_VHIF_SYNC_STAT			BIT(7)
#define PPI_INIT_BUSY				BIT(5)
#define PPI_BYTE_BUSY				BIT(4)
#define PPI_TXESC_BUSY				BIT(3)
#define PPI_CL_BUSY				BIT(2)
#define PPI_DL_BUSY				BIT(1)
#define CSI_STAT				BIT(0)

#define LPTXTIMECNT				0x0254
#define TCLK_HEADERCNT				0x0258
#define TCLK_TRAILCNT				0x025c
#define THS_HEADERCNT				0x0260
#define TWAKEUP					0x0264
#define TCLK_POSTCNT				0x0268
#define THS_TRAILCNT				0x026c
#define HSTXVREGCNT				0x0270

#define HSTXVREGEN				0x0274
#define MASK_D3M_HSTXVREGEN			0x0010
#define MASK_D2M_HSTXVREGEN			0x0008
#define MASK_D1M_HSTXVREGEN			0x0004
#define MASK_D0M_HSTXVREGEN			0x0002
#define MASK_CLM_HSTXVREGEN			0x0001

#define MIPICLKEN				0x02a0
#define MASK_MP_ENABLE				0x00000001
#define MASK_MP_CKEN				0x00000002

#define PLLCONF					0x02ac
#define MASK_LFBREN				BIT(9)
#define MASK_MPLBW				0x00030000
#define MASK_MPLBW_25				(0 << 16)
#define MASK_MPLBW_33				(1 << 16)
#define MASK_MPLBW_50				(2 << 16)
#define MASK_MPLBW_MAX				(3 << 16)
#define MASK_PLL_FBD				0x000000ff
#define SET_PLL_FBD(fbd)			((fbd) & MASK_PLL_FBD)
#define MASK_PLL_FRS				0x00000c00
#define SET_PLL_FRS(frs)			(((frs) << 10) & MASK_PLL_FRS)
#define MASK_PLL_PRD				0x0000f000
#define SET_PLL_PRD(prd)			(((prd) << 12) & \
						  MASK_PLL_PRD)
#define MASK_PLL_LBW				0x00030000
#define SET_PLL_LBW(lbw)			((((lbw) - 1) << 16) & \
						  MASK_PLL_LBW)

/* *** CEC (32 bit) *** */
#define CECHCLK					0x0028	/* 16 bits */
#define MASK_CECHCLK				(0x7ff << 0)

#define CECLCLK					0x002a	/* 16 bits */
#define MASK_CECLCLK				(0x7ff << 0)

#define CECEN					0x0600
#define MASK_CECEN				0x0001

#define CECADD					0x0604
#define CECRST					0x0608
#define MASK_CECRESET				0x0001

#define CECREN					0x060c
#define MASK_CECREN				0x0001

#define CECRCTL1				0x0614
#define MASK_CECACKDIS				BIT(24)
#define MASK_CECHNC				(3 << 20)
#define MASK_CECLNC				(7 << 16)
#define MASK_CECMIN				(7 << 12)
#define MASK_CECMAX				(7 << 8)
#define MASK_CECDAT				(7 << 4)
#define MASK_CECTOUT				(3 << 2)
#define MASK_CECRIHLD				BIT(1)
#define MASK_CECOTH				BIT(0)

#define CECRCTL2				0x0618
#define MASK_CECSWAV3				(7 << 12)
#define MASK_CECSWAV2				(7 << 8)
#define MASK_CECSWAV1				(7 << 4)
#define MASK_CECSWAV0				(7 << 0)

#define CECRCTL3				0x061c
#define MASK_CECWAV3				(7 << 20)
#define MASK_CECWAV2				(7 << 16)
#define MASK_CECWAV1				(7 << 12)
#define MASK_CECWAV0				(7 << 8)
#define MASK_CECACKEI				BIT(4)
#define MASK_CECMINEI				BIT(3)
#define MASK_CECMAXEI				BIT(2)
#define MASK_CECRSTEI				BIT(1)
#define MASK_CECWAVEI				BIT(0)

#define CECTEN					0x0620
#define MASK_CECTBUSY				BIT(1)
#define MASK_CECTEN				BIT(0)

#define CECTCTL					0x0628
#define MASK_CECSTRS				(7 << 20)
#define MASK_CECSPRD				(7 << 16)
#define MASK_CECDTRS				(7 << 12)
#define MASK_CECDPRD				(15 << 8)
#define MASK_CECBRD				BIT(4)
#define MASK_CECFREE				(15 << 0)

#define CECRSTAT				0x062c
#define MASK_CECRIWA				BIT(6)
#define MASK_CECRIOR				BIT(5)
#define MASK_CECRIACK				BIT(4)
#define MASK_CECRIMIN				BIT(3)
#define MASK_CECRIMAX				BIT(2)
#define MASK_CECRISTA				BIT(1)
#define MASK_CECRIEND				BIT(0)

#define CECTSTAT				0x0630
#define MASK_CECTIUR				BIT(4)
#define MASK_CECTIACK				BIT(3)
#define MASK_CECTIAL				BIT(2)
#define MASK_CECTIEND				BIT(1)

#define CECRBUF1				0x0634
#define MASK_CECRACK				BIT(9)
#define MASK_CECEOM				BIT(8)
#define MASK_CECRBYTE				(0xff << 0)

#define CECTBUF1				0x0674
#define MASK_CECTEOM				BIT(8)
#define MASK_CECTBYTE				(0xff << 0)

#define CECRCTR					0x06b4
#define MASK_CECRCTR				(0x1f << 0)

#define CECIMSK					0x06c0
#define MASK_CECTIM				BIT(1)
#define MASK_CECRIM				BIT(0)

#define CECICLR					0x06cc
#define MASK_CECTICLR				BIT(1)
#define MASK_CECRICLR				BIT(0)

/* *** Split Control (16 bit) *** */
#define SPLITTX0_CTRL				0x5000
#define SPLITTX1_CTRL				0x5080
#define MASK_LCD_CSEL				0x0001
#define MASK_IFEN				0x0002
#define MASK_SPBP				0x0100

#define SPLITTX0_SPLIT				0x500c
#define SPLITTX1_SPLIT				0x508c
#define MASK_FPXV				0x0fff
/* NOTE: Only available for TX0 */
#define MASK_TX1SEL				0x4000
/* NOTE: Only available for TX0 */
#define MASK_EHW				0x8000

/* *** HDMI PHY (8 bit) *** */
#define PHY_CTL					0x8410
#define MASK_POWERCTL				BIT(0)

#define PHY_CTL2				0x8412
#define MASK_PHY_FREE_RUN			BIT(5)

#define PHY_ENB					0x8413
#define MASK_ENABLE_PHY				0x01

#define PHY_RST					0x8414
#define MASK_RESET_CTRL				0x01	/* Reset active low */

#define EQ_BYPS					0x8420
#define MASK_EQ_BYPS				0x01

#define APPL_CTL				0x84f0
#define MASK_APLL_ON				0x01
#define MASK_APLL_CPCTL				0x30
#define MASK_APLL_CPCTL_HIZ			0x00
#define MASK_APLL_CPCTL_LFIX			0x10
#define MASK_APLL_CPCTL_HFIX			0x20
#define MASK_APLL_CPCTL_NORMAL			0x30

#define DDCIO_CTL				0x84f4
#define MASK_DDC_PWR_ON				BIT(0)

/** *** HDMI Clock (8 bit) *** */
#define AU_STATUS0				0x8523
#define MASK_S_A_SAMPLE				0x01

#define SYS_FREQ0				0x8540
#define SYS_FREQ1				0x8541
#define LOCK_REF_FREQA				0x8630
#define LOCK_REF_FREQB				0x8631
#define LOCK_REF_FREQC				0x8632

#define FS_SET					0x8621
#define MASK_FS					0x0f

#define CLK_STATUS				0x8526
#define MASK_S_V_HPOL				BIT(3)
#define MASK_S_V_VPOL				BIT(2)

#define NCO_F0_MOD				0x8670
#define MASK_NCO_F0_MOD_42MHZ			0x00
#define MASK_NCO_F0_MOD_REG			0x02

#define NCO_48F0A				0x8671
#define NCO_48F0B				0x8672
#define NCO_48F0C				0x8673
#define NCO_48F0D				0x8674

#define NCO_44F0A				0x8675
#define NCO_44F0B				0x8676
#define NCO_44F0C				0x8677
#define NCO_44F0D				0x8678

#define SCLK_CSC0				0x8a0c
#define SCLK_CSC1				0x8a0d

#define HDCP_MODE				0x8560
#define MASK_MODE_RST_TN			0x20
#define MASK_LINE_REKEY				0x10
#define MASK_AUTO_CLR				0x04
#define MASK_MANUAL_AUTHENTICATION              0x02 /* Not in REF_01 */

#define HDCP_CMD                0x8561
#define MASK_SHAS                   0x08
#define MASK_CALPARAM               0x04
#define MASK_CALKM                  0x02

#define HDCP_BKSV               0x85ec
#define MASK_LOAD_BKSV              0x01

/* *** VI *** */
#define VI_MODE					0x8570
#define MASK_RGB_DVI				0x08
#define MASK_RGB_HDMI				0x04
#define MASK_VI_MODE				0x01

#define DE_HSIZE_LO				0x8582
#define DE_HSIZE_HI				0x8583
#define DE_VSIZE_LO				0x858c
#define DE_VSIZE_HI				0x858d

#define IN_HSIZE_LO				0x858e
#define IN_HSIZE_HI				0x858f

#define IN_VSIZE_LO				0x8590
#define IN_VSIZE_HI				0x8591

#define HV_CLR					0x8593
#define MASK_HV_HOLD				0x80
#define MASK_HV_DEV_CLR				0x20
#define MASK_HV_DEH_CLR				0x10
#define MASK_HV_V_CLR				0x02
#define MASK_HV_H_CLR				0x01

#define FV_CNT_LO				0x85c1	/* Undocumented */
#define FV_CNT_HI				0x85c2	/* Undocumented */

#define HV_STATUS				0x85c5
#define MASK_M_HV_STATUS			0x30
#define MASK_S_H_RANG				0x02
#define MASK_S_V_RANG				0x01

#define FH_MIN0					0x85ca /* Not in REF_01 */
#define FH_MIN1					0x85cb /* Not in REF_01 */
#define FH_MAX0					0x85cc /* Not in REF_01 */
#define FH_MAX1					0x85cd /* Not in REF_01 */

#define HV_RST					0x85cf /* Not in REF_01 */
#define MASK_H_PI_RST				0x20
#define MASK_V_PI_RST				0x10

/* *** EDID (8 bit) *** */
#define EDID_MODE				0x85e0
#define MASK_DIRECT				0x00
#define MASK_RAM_DDC2B				BIT(0)
#define MASK_RAM_EDDC				BIT(1)
#define MASK_EDID_MODE_ALL			0x03

#define EDID_LEN1				0x85e3
#define EDID_LEN2				0x85e4

#define EDID_RAM				0x8c00

/* *** HDCP *** */
#define BKSV					0x8800

#define BCAPS					0x8840
#define MASK_HDMI_RSVD				0x80
#define MASK_REPEATER				0x40
#define MASK_READY				0x20
#define MASK_FASTI2C				0x10
#define MASK_1_1_FEA				0x02
#define MASK_FAST_REAU				0x01

#define BSTATUS0				0x8841
#define MASK_MAX_DEVEXCED			0x80U
#define MASK_DEVICE_COUNT			0x7fU

#define BSTATUS1				0x8842
#define MASK_VPRIME_RESET			0x20
#define MASK_HDMI_MODE				0x10
#define MASK_MAX_EXCED				0x08
#define MASK_CASCADE_DEPTH			0x07

#define KSVFIFO					0x8843

/* *** Video Output Format (8 bit) *** */
#define VOUT_FMT				0x8a00
#define MASK_OUTFMT_444_RGB			(0 << 0)
#define MASK_OUTFMT_422				(1 << 0)
#define MASK_OUTFMT_THROUGH			(2 << 0)
#define MASK_422FMT_NORMAL			(0 << 4)
#define MASK_422FMT_HDMITHROUGH			(1 << 4)

#define VOUT_FIL				0x8a01
#define MASK_422FIL				0x07
#define MASK_422FIL_2_TAP			(0 << 0)
#define MASK_422FIL_3_TAP			(1 << 0)
#define MASK_422FIL_NO_FILTER			(2 << 0)
#define MASK_422FIL_2_TAP_444			(3 << 0)
#define MASK_422FIL_3_TAP_444			(4 << 0)
#define MASK_422FIL_2_TAP_444_CSC		(5 << 0)
#define MASK_422FIL_3_TAP_444_CSC		(6 << 0)
#define MASK_444FIL				0x10
#define MASK_444FIL_REPEAT			(0 << 4)
#define MASK_444FIL_2_TAP			(1 << 4)

#define VOUT_SYNC0				0x8a02
#define MASK_MODE_2				(2 << 0)
#define MASK_MODE_3				(3 << 0)
#define MASK_M3_HSIZE				0x30
#define MASK_M3_VSIZE				0xc0

#define VOUT_CSC				0x8a08
#define MASK_CSC_MODE				0x03
#define MASK_CSC_MODE_OFF			(0 << 0)
#define MASK_CSC_MODE_BUILTIN			(1 << 0)
#define MASK_CSC_MODE_AUTO			(2 << 0)
#define MASK_CSC_MODE_HOST			(3 << 0)
#define MASK_COLOR				0x70
#define MASK_COLOR_RGB_FULL			(0 << 4)
#define MASK_COLOR_RGB_LIMITED			(1 << 4)
#define MASK_COLOR_601_YCBCR_FULL		(2 << 4)
#define MASK_COLOR_601_YCBCR_LIMITED		(3 << 4)
#define MASK_COLOR_709_YCBCR_FULL		(4 << 4)
#define MASK_COLOR_709_YCBCR_LIMITED		(5 << 4)
#define MASK_COLOR_FULL_TO_LIMITED		(6 << 4)
#define MASK_COLOR_LIMITED_TO_FULL		(7 << 4)

/* *** HDMI Audio RefClk (8 bit) *** */
#define FORCE_MUTE				0x8600
#define MASK_FORCE_DMUTE			BIT(0)
#define MASK_FORCE_AMUTE			BIT(4)

#define AUTO_CMD0				0x8602
#define MASK_AUTO_MUTE7				0x80
#define MASK_AUTO_MUTE6				0x40
#define MASK_AUTO_MUTE5				0x20
#define MASK_AUTO_MUTE4				0x10
#define MASK_AUTO_MUTE3				0x08
#define MASK_AUTO_MUTE2				0x04
#define MASK_AUTO_MUTE1				0x02
#define MASK_AUTO_MUTE0				0x01

#define AUTO_CMD1				0x8603
#define MASK_AUTO_MUTE10			0x04
#define MASK_AUTO_MUTE9				0x02
#define MASK_AUTO_MUTE8				0x01

#define AUTO_CMD2				0x8604
#define MASK_AUTO_PLAY3				0x08
#define MASK_AUTO_PLAY2				0x04

#define BUFINIT_START				0x8606
#define SET_BUFINIT_START_MS(milliseconds)	((milliseconds) / 100)

#define FS_MUTE					0x8607
#define MASK_FS_ELSE_MUTE			0x80
#define MASK_FS22_MUTE				0x40
#define MASK_FS24_MUTE				0x20
#define MASK_FS88_MUTE				0x10
#define MASK_FS96_MUTE				0x08
#define MASK_FS176_MUTE				0x04
#define MASK_FS192_MUTE				0x02
#define MASK_FS_NO_MUTE				0x01

#define FS_IMODE				0x8620
#define MASK_NLPCM_HMODE			0x40
#define MASK_NLPCM_SMODE			0x20
#define MASK_NLPCM_IMODE			0x10
#define MASK_FS_HMODE				0x08
#define MASK_FS_AMODE				0x04
#define MASK_FS_SMODE				0x02
#define MASK_FS_IMODE				0x01

#define ACR_MODE				0x8640
#define MASK_ACR_LOAD				0x10
#define MASK_N_MODE				0x04
#define MASK_CTS_MODE				0x01

#define ACR_MDF0				0x8641
#define MASK_ACR_L2MDF				0x70
#define MASK_ACR_L2MDF_0_PPM			0x00
#define MASK_ACR_L2MDF_61_PPM			0x10
#define MASK_ACR_L2MDF_122_PPM			0x20
#define MASK_ACR_L2MDF_244_PPM			0x30
#define MASK_ACR_L2MDF_488_PPM			0x40
#define MASK_ACR_L2MDF_976_PPM			0x50
#define MASK_ACR_L2MDF_1976_PPM			0x60
#define MASK_ACR_L2MDF_3906_PPM			0x70
#define MASK_ACR_L1MDF				0x07
#define MASK_ACR_L1MDF_0_PPM			0x00
#define MASK_ACR_L1MDF_61_PPM			0x01
#define MASK_ACR_L1MDF_122_PPM			0x02
#define MASK_ACR_L1MDF_244_PPM			0x03
#define MASK_ACR_L1MDF_488_PPM			0x04
#define MASK_ACR_L1MDF_976_PPM			0x05
#define MASK_ACR_L1MDF_1976_PPM			0x06
#define MASK_ACR_L1MDF_3906_PPM			0x07

#define ACR_MDF1				0x8642
#define MASK_ACR_L3MDF				0x07
#define MASK_ACR_L3MDF_0_PPM			0x00
#define MASK_ACR_L3MDF_61_PPM			0x01
#define MASK_ACR_L3MDF_122_PPM			0x02
#define MASK_ACR_L3MDF_244_PPM			0x03
#define MASK_ACR_L3MDF_488_PPM			0x04
#define MASK_ACR_L3MDF_976_PPM			0x05
#define MASK_ACR_L3MDF_1976_PPM			0x06
#define MASK_ACR_L3MDF_3906_PPM			0x07

#define SDO_MODE1				0x8652
#define MASK_SDO_BIT_LENG			0x70
#define MASK_SDO_FMT				0x03
#define MASK_SDO_FMT_RIGHT			0x00
#define MASK_SDO_FMT_LEFT			0x01
#define MASK_SDO_FMT_I2S			0x02

#define DIV_MODE				0x8665 /* Not in REF_01 */
#define MASK_DIV_DLY				0xf0
#define SET_DIV_DLY_MS(milliseconds) \
	((((milliseconds) / 100) << 4) & MASK_DIV_DLY)
#define MASK_DIV_MODE				0x01

#define HDMIAUDIO_MODE				0x8680

/* *** HDMI General (16 bit) *** */
#define DDC_CTL					0x8543
#define MASK_DDC_ACTION				0x04
#define MASK_DDC5V_MODE				0x03
#define MASK_DDC5V_MODE_0MS			0x00
#define MASK_DDC5V_MODE_50MS			0x01
#define MASK_DDC5V_MODE_100MS			0x02
#define MASK_DDC5V_MODE_200MS			0x03

#define HPD_CTL					0x8544
#define MASK_HPD_OUT0				BIT(0)
#define MASK_HPD_CTL0				BIT(4)

#define INIT_END				0x854a
#define MASK_INIT_END				0x01

/* *** Video Mute *** */
#define VI_MUTE					0x857f
#define MASK_AUTO_MUTE				0xc0
#define MASK_VI_MUTE				0x10
#define MASK_VI_BLACK				0x01

/* *** Info Frame *** */
#define PK_INT_MODE				0x8709
#define MASK_ISRC2_INT_MODE			0x80
#define MASK_ISRC_INT_MODE			0x40
#define MASK_ACP_INT_MODE			0x20
#define MASK_VS_INT_MODE			0x10
#define MASK_SPD_INT_MODE			0x08
#define MASK_MS_INT_MODE			0x04
#define MASK_AUD_INT_MODE			0x02
#define MASK_AVI_INT_MODE			0x01

#define NO_PKT_LIMIT				0x870b
#define MASK_NO_ACP_LIMIT			0xf0
#define SET_NO_ACP_LIMIT_MS(milliseconds) \
	((((milliseconds) / 80) << 4) & MASK_NO_ACP_LIMIT)

#define NO_PKT_CLR				0x870c
#define MASK_NO_VS_CLR				0x40
#define MASK_NO_SPD_CLR				0x20
#define MASK_NO_ACP_CLR				0x10
#define MASK_NO_AVI_CLR1			0x02
#define MASK_NO_AVI_CLR0			0x01

#define ERR_PK_LIMIT				0x870d
#define NO_PKT_LIMIT2				0x870e
#define PK_AVI_0HEAD				0x8710
#define PK_AVI_1HEAD				0x8711
#define PK_AVI_2HEAD				0x8712
#define PK_AVI_0BYTE				0x8713
#define PK_AVI_1BYTE				0x8714
#define PK_AVI_2BYTE				0x8715
#define PK_AVI_3BYTE				0x8716
#define PK_AVI_4BYTE				0x8717
#define PK_AVI_5BYTE				0x8718
#define PK_AVI_6BYTE				0x8719
#define PK_AVI_7BYTE				0x871a
#define PK_AVI_8BYTE				0x871b
#define PK_AVI_9BYTE				0x871c
#define PK_AVI_10BYTE				0x871d
#define PK_AVI_11BYTE				0x871e
#define PK_AVI_12BYTE				0x871f
#define PK_AVI_13BYTE				0x8720
#define PK_AVI_14BYTE				0x8721
#define PK_AVI_15BYTE				0x8722
#define PK_AVI_16BYTE				0x8723

#define PK_AUD_0HEAD				0x8730

#define PK_SPD_0HEAD				0x8750

#define PK_VS_0HEAD				0x8770
#define PK_VS_0BYTE				0x8773
#define PK_VS_1BYTE				0x8774
#define PK_VS_2BYTE				0x8775
#define PK_VS_3BYTE				0x8776
#define PK_VS_4BYTE				0x8777
#define PK_VS_5BYTE				0x8778

#define NO_GDB_LIMIT				0x9007

/* *** Color Bar (16 bit) *** */
#define CB_CTL					0x7000
#define MASK_CB_EN				0x01
#define MASK_CB_TYPE				(0x03 << 1)
#define MASK_CB_TYPE_COLOR_BAR			(0x02 << 1)
#define MASK_CB_TYPE_COLOR_CHECKERS		(0x03 << 1)
#define MASK_CB_CSEL				(0x01 << 3)
#define MASK_CB_CSEL_CSI_TX0			0x00
#define MASK_CB_CSEL_CSI_TX1			(0x01 << 3)
#define CB_HSW					0x7008
#define CB_VSW					0x700a
#define CB_HTOTAL				0x700c
#define CB_VTOTAL				0x700e
#define CB_HACT					0x7010
#define CB_VACT					0x7012
#define CB_HSTART				0x7014
#define CB_VSTART				0x7016

#endif  /* __TC358840_REGS__ */
