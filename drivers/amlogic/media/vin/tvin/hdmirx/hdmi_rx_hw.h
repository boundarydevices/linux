/*
 * drivers/amlogic/media/vin/tvin/hdmirx/hdmi_rx_hw.h
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

#ifndef __HDMI_RX_HW_H__
#define __HDMI_RX_HW_H__


/**
 * Bit field mask
 * @param m	width
 * @param n shift
 */
#define MSK(m, n)		(((1 << (m)) - 1) << (n))
/**
 * Bit mask
 * @param n shift
 */
#define _BIT(n)			MSK(1, (n))


#define HHI_GCLK_MPEG0			(0x50 << 2) /* (0xC883C000 + 0x140) */
#define HHI_HDMIRX_CLK_CNTL		0x200 /* (0xC883C000 + 0x200)  */
#define HHI_HDMIRX_AUD_CLK_CNTL	0x204 /* 0x1081 */
#define HHI_AUD_PLL_CNTL		(0xf8 * 4)
#define HHI_AUD_PLL_CNTL2		(0xf9 * 4)
#define HHI_AUD_PLL_CNTL3		(0xfa * 4)
#define HHI_AUD_PLL_CNTL4		(0xfb * 4)
#define HHI_AUD_PLL_CNTL5		(0xfc * 4)
#define HHI_AUD_PLL_CNTL6		(0xfd * 4)
#define HHI_AUD_PLL_CNTL_I		(0xfe * 4) /* audio pll lock bit31 */
#define HHI_ADC_PLL_CNTL4		(0xad * 4)
#define HHI_HDCP22_CLK_CNTL		(0x7c * 4)
#define HHI_GCLK_MPEG2			(0x52 * 4)

/* TXLX */
/* unified_register.h by wujun */
#define HHI_AUDPLL_CLK_OUT_CNTL (0x8c << 2)

#define PREG_PAD_GPIO0_EN_N		(0x0c * 4)
#define PREG_PAD_GPIO0_O		(0x0d * 4)
#define PREG_PAD_GPIO0_I		(0x0e * 4)
#define PERIPHS_PIN_MUX_6		(0x32 * 4)
#define PERIPHS_PIN_MUX_10		(0x36 * 4)
#define PERIPHS_PIN_MUX_11		(0x37 * 4)
#define PAD_PULL_UP_REG2		(0x3c * 4)

#define AUD_RESAMPLE_CTRL0		0x28bf

/** PHY Gen3 Clock measurement lock threshold - default 8*/
#define LOCK_THRES                       0x3f	/* 0x08 */
/** register address: PHY Gen3 clock measurement unit configuration */
#define PHY_CMU_CONFIG			(0x02UL)
/** register address: PHY Gen3 system configuration */
#define PHY_SYSTEM_CONFIG		(0x03UL)
#define PHY_MAINFSM_CTL			(0x05UL)
/** register address: PHY Gen3 main FSM status 1 */
#define PHY_MAINFSM_STATUS1	   (0x09UL)

#define PHY_RESISTOR_CALIBRATION_1 (0x10UL)
#define PHY_MAIN_FSM_OVERRIDE1	(0x07UL)
#define PHY_MAIN_FSM_OVERRIDE2	(0x08UL)

#define PHY_MAIN_BIST_CONTROL	(0x0BUL)
#define PHY_MAIN_BIST_RESULTS	(0x0CUL)

#define PHY_CORESTATUS_CH0		(0x30UL)
#define PHY_CORESTATUS_CH1		(0x50UL)
#define PHY_CORESTATUS_CH2		(0x70UL)

#define PHY_EQCTRL1_CH0			(0x32UL)
#define PHY_EQCTRL1_CH1			(0x52UL)
#define PHY_EQCTRL1_CH2			(0x72UL)

#define PHY_EQLSTATS_CH0		(0x34UL)
#define PHY_EQLSTATS_CH1		(0x54UL)
#define PHY_EQLSTATS_CH2		(0x74UL)

#define PHY_CH0_EQ_CTRL3		(0x3EUL)
#define PHY_CH1_EQ_CTRL3		(0x5EUL)
#define PHY_CH2_EQ_CTRL3		(0x7EUL)

#define PHY_EQCTRL4_CH0			(0x3FUL)
#define PHY_EQCTRL4_CH1			(0x5FUL)
#define PHY_EQCTRL4_CH2			(0x7FUL)

#define PHY_EQCTRL2_CH0			(0x33UL)
#define PHY_EQCTRL2_CH1			(0x53UL)
#define PHY_EQCTRL2_CH2			(0x73UL)

#define PHY_EQLCKST2_CH0		(0x40UL)
#define PHY_EQLCKST2_CH1		(0x60UL)
#define PHY_EQLCKST2_CH2		(0x80UL)

#define PHY_EQSTAT3_CH0			(0x42UL)
#define PHY_EQSTAT3_CH1			(0x62UL)
#define PHY_EQSTAT3_CH2			(0x82UL)

#define PHY_EQCTRL6_CH0		(0x43UL)
#define PHY_EQCTRL6_CH1		(0x63UL)
#define PHY_EQCTRL6_CH2		(0x83UL)

#define OVL_PROT_CTRL				(0x0DUL)
#define PHY_CDR_CTRL_CNT			(0x0EUL)
	#define CLK_RATE_BIT		_BIT(8)
#define PHY_VOLTAGE_LEVEL			(0x22UL)
#define PHY_MPLL_CTRL				(0x24UL)
#define MPLL_DIVIDER_CONTROL		(0x25UL)
#define MPLL_PARAMETERS2                (0x27UL)
#define MPLL_PARAMETERS3                (0x28UL)
#define MPLL_PARAMETERS4                (0x29UL)
#define MPLL_PARAMETERS5                (0x2AUL)
#define MPLL_PARAMETERS6                (0x2BUL)
#define MPLL_PARAMETERS7                (0x2CUL)
#define MPLL_PARAMETERS8                (0x2DUL)
#define MPLL_PARAMETERS9                (0x2EUL)
#define MPLL_PARAMETERS10                (0xC0UL)
#define MPLL_PARAMETERS11                (0xC1UL)
#define MPLL_PARAMETERS12                (0xC2UL)
#define MPLL_PARAMETERS13                (0xC3UL)
#define MPLL_PARAMETERS14                (0xC4UL)
#define MPLL_PARAMETERS15                (0xC5UL)
#define MPLL_PARAMETERS16                (0xC6UL)
#define MPLL_PARAMETERS17               (0xC7UL)
#define MPLL_PARAMETERS18                (0xC8UL)
#define MPLL_PARAMETERS19                (0xC9UL)
#define MPLL_PARAMETERS20                (0xCAUL)
#define MPLL_PARAMETERS21                (0xCBUL)
#define MPLL_PARAMETERS22                (0xCCUL)
#define MPLL_PARAMETERS23                (0xCDUL)


/* ------------------------------------- */
/* TOP-level wrapper registers addresses */
/* ------------------------------------- */

#define TOP_SW_RESET                     0x000
	#define HDCP22_TMDSCLK_EN		_BIT(3)
#define TOP_CLK_CNTL                     0x001
#define TOP_HPD_PWR5V                    0x002
#define TOP_PORT_SEL                     0x003
#define TOP_EDID_GEN_CNTL                0x004
#define TOP_EDID_ADDR_CEC                0x005
#define TOP_EDID_DATA_CEC_PORT01         0x006
#define TOP_EDID_DATA_CEC_PORT23         0x007
#define TOP_EDID_GEN_STAT                0x008
#define TOP_INTR_MASKN                   0x009
#define TOP_INTR_STAT                    0x00A
#define TOP_INTR_STAT_CLR                0x00B
#define TOP_VID_CNTL                     0x00C
#define TOP_VID_STAT                     0x00D
#define TOP_ACR_CNTL_STAT                0x00E
#define TOP_ACR_AUDFIFO                  0x00F
#define TOP_ARCTX_CNTL                   0x010
#define TOP_METER_HDMI_CNTL              0x011
#define TOP_METER_HDMI_STAT              0x012
#define TOP_VID_CNTL2                    0x013

/* hdmi2.0 new start */
#define TOP_MEM_PD                       0x014
#define TOP_EDID_RAM_OVR0                0x015
#define TOP_EDID_RAM_OVR0_DATA           0x016
#define TOP_EDID_RAM_OVR1                0x017
#define TOP_EDID_RAM_OVR1_DATA           0x018
#define TOP_EDID_RAM_OVR2                0x019
#define TOP_EDID_RAM_OVR2_DATA           0x01a
#define TOP_EDID_RAM_OVR3                0x01b
#define TOP_EDID_RAM_OVR3_DATA           0x01c
#define TOP_EDID_RAM_OVR4                0x01d
#define TOP_EDID_RAM_OVR4_DATA           0x01e
#define TOP_EDID_RAM_OVR5                0x01f
#define TOP_EDID_RAM_OVR5_DATA           0x020
#define TOP_EDID_RAM_OVR6                0x021
#define TOP_EDID_RAM_OVR6_DATA           0x022
#define TOP_EDID_RAM_OVR7                0x023
#define TOP_EDID_RAM_OVR7_DATA           0x024
#define TOP_EDID_GEN_STAT_B              0x025
#define TOP_EDID_GEN_STAT_C              0x026
#define TOP_EDID_GEN_STAT_D              0x027
#define	TOP_ACR_CNTL2					 0x02a
/* Gxtvbb */
#define	TOP_INFILTER_GXTVBB				 0x02b
/* Gxtvbb */

#define	TOP_INFILTER_HDCP				 0x02C
#define	TOP_INFILTER_I2C0				 0x02D
#define	TOP_INFILTER_I2C1				 0x02E
#define	TOP_INFILTER_I2C2				 0x02F
#define	TOP_INFILTER_I2C3				 0x030

#define	TOP_SKP_CNTL_STAT				 0x061
#define	TOP_NONCE_0						 0x062
#define	TOP_NONCE_1						 0x063
#define	TOP_NONCE_2						 0x064
#define	TOP_NONCE_3						 0x065
#define	TOP_PKF_0						 0x066
#define	TOP_PKF_1						 0x067
#define	TOP_PKF_2						 0x068
#define	TOP_PKF_3						 0x069
#define	TOP_DUK_0						 0x06a
#define	TOP_DUK_1						 0x06b
#define	TOP_DUK_2						 0x06c
#define	TOP_DUK_3						 0x06d
#define TOP_NSEC_SCRATCH				 0x06e
#define	TOP_SEC_SCRATCH					 0x06f
#define TOP_DONT_TOUCH0                  0x0fe
#define TOP_DONT_TOUCH1                  0x0ff


/* hdmi2.0 new end */
#define TOP_EDID_OFFSET                  0x200

/*
 * HDMI registers
 */
/** Register address: setup control */
#define DWC_HDMI_SETUP_CTRL      (0x000UL)
/** Hot plug detect signaled */
#define		HOT_PLUG_DETECT			_BIT(0)
/** Register address: override control */
#define DWC_HDMI_OVR_CTRL        (0x004UL)
/** Register address: timer control */
#define DWC_HDMI_TIMEDWC_CTRL     (0x008UL)
/** Register address: resistor override */
#define DWC_HDMI_RES_OVR         (0x010UL)
/** Register address: resistor status */
#define DWC_HDMI_RES_STS         (0x014UL)
/** Register address: TMDS PLL control */
#define DWC_HDMI_PLL_CTRL        (0x018UL)
/** Register address: TMDS PLL frequency range */
#define DWC_HDMI_PLL_FRQSET1     (0x01CUL)
/** Register address: TMDS PLL frequency range */
#define DWC_HDMI_PLL_FRQSET2     (0x020UL)
/** Register address: TMDS PLL PCP and ICP range */
#define DWC_HDMI_PLL_PAR1        (0x024UL)
/** Register address: TMDS PLL PCP and ICP range */
#define DWC_HDMI_PLL_PAR2        (0x028UL)
/** Register address: TMDS PLL KOSC and CCOLF range */
#define DWC_HDMI_PLL_PAR3        (0x02CUL)
/** Register address: PLL post lock filter */
#define DWC_HDMI_PLL_LCK_STS     (0x030UL)
/** Register address: PLL clock control */
#define DWC_HDMI_CLK_CTRL        (0x034UL)
/** Register address: PCB diversity control */
#define DWC_HDMI_PCB_CTRL        (0x038UL)
/** Input selector */
#define		INPUT_SELECT			_BIT(16)
/** Register address: phase control */
#define DWC_HDMI_PHS_CTRL        (0x040UL)
/** Register address: used phases */
#define DWC_HDMI_PHS_USD         (0x044UL)
/** Register address: miscellaneous operations control */
#define DWC_HDMI_MISC_CTRL       (0x048UL)
/** Register address: EQ offset calibration */
#define DWC_HDMI_EQOFF_CTRL      (0x04CUL)
/** Register address: EQ gain control */
#define DWC_HDMI_EQGAIN_CTRL     (0x050UL)
/** Register address: EQ status */
#define DWC_HDMI_EQCAL_STS       (0x054UL)
/** Register address: EQ results */
#define DWC_HDMI_EQRESULT        (0x058UL)
/** Register address: EQ measurement control */
#define DWC_HDMI_EQ_MEAS_CTRL    (0x05CUL)
/** Register address: HDMI mode recover */
#define DWC_HDMI_MODE_RECOVER    (0x080UL)
/** Register address: HDMI error protection */
#define DWC_HDMI_ERROR_PROTECT  (0x084UL)
/** Register address: validation and production test */
#define DWC_HDMI_ERD_STS         (0x088UL)
/** Register address: video output sync signals control */
#define DWC_HDMI_SYNC_CTRL       (0x090UL)
/** VS polarity adjustment */
#define		VS_POL_ADJ_MODE			MSK(2, 3)
/** HS polarity adjustment automatic */
#define		VS_POL_ADJ_AUTO			(2)
/** HS polarity adjustment */
#define		HS_POL_ADJ_MODE			MSK(2, 1)
/** HS polarity adjustment automatic inversion */
#define		HS_POL_ADJ_AUTO			(2)
/** Register address: clock measurement */
#define DWC_HDMI_CKM_EVLTM       (0x094UL)
/** Evaluation period */
#define		EVAL_TIME				MSK(12, 4)
/** active wait period for TMDS stabilisation */
#define		TMDS_STABLE_TIMEOUT			(30)
/** Register address: legal clock count */
#define DWC_HDMI_CKM_F           (0x098UL)
/** Maximum count for legal count */
#define		CKM_MAXFREQ					MSK(16, 16)
/** Minimum count for legal count */
#define		MINFREQ					MSK(16, 0)
/** Register address: measured clock results */
#define DWC_HDMI_CKM_RESULT      (0x09CUL)
/** Measured clock is stable */
#define		CLOCK_IN_RANGE			_BIT(17)
/** Measured clock rate in bits */
#define		CLKRATE					MSK(16, 0)
/** Register address: sub-sampling control */
#define DWC_HDMI_RESMPL_CTRL     (0x0A4UL)
/** Register address: deep color mode control */
#define DWC_HDMI_DCM_CTRL        (0x0A8UL)
/** Register address: video output mute configuration */
#define DWC_HDMI_VM_CFG_CH_0_1   (0x0B0UL)
/** Register address: video output mute configuration */
#define DWC_HDMI_VM_CFG_CH2      (0x0B4UL)
/** Register address: spare */
#define DWC_HDMI_SPARE           (0x0B8UL)
/** Register address: HDMI status */
#define DWC_HDMI_STS             (0x0BCUL)
/** Current deep color mode */
#define		DCM_CURRENT_MODE		MSK(4, 28)
/** Deep color mode, 24 bit */
#define		DCM_CURRENT_MODE_24b	(4)
/** Deep color mode, 30 bit */
#define		DCM_CURRENT_MODE_30b	(5)
/** Deep color mode, 36 bit */
#define		DCM_CURRENT_MODE_36b	(6)
/** Deep color mode, 48 bit */
#define		DCM_CURRENT_MODE_48b	(7)

/* HDMI 2.0 feature registers */
/* bit0-1  scramble ctrl */
#define DWC_HDMI20_CONTROL				0x0800
#define DWC_SCDC_I2CCONFIG				0x0804
#define DWC_SCDC_CONFIG					0x0808
#define DWC_CHLOCK_CONFIG				0x080C
#define DWC_HDCP22_CONTROL				0x081C
#define DWC_SCDC_REGS0                  0x0820
#define DWC_SCDC_REGS1                  0x0824
#define DWC_SCDC_REGS2                  0x0828
#define DWC_SCDC_REGS3                  0x082C
#define DWC_SCDC_MANSPEC0               0x0840
#define DWC_SCDC_MANSPEC1               0x0844
#define DWC_SCDC_MANSPEC2               0x0848
#define DWC_SCDC_MANSPEC3               0x084C
#define DWC_SCDC_MANSPEC4               0x0850
#define DWC_SCDC_WRDATA0                0x0860
#define DWC_SCDC_WRDATA1                0x0864
#define DWC_SCDC_WRDATA2                0x0868
#define DWC_SCDC_WRDATA3                0x086C
#define DWC_SCDC_WRDATA4                0x0870
#define DWC_SCDC_WRDATA5                0x0874
#define DWC_SCDC_WRDATA6                0x0878
#define DWC_SCDC_WRDATA7                0x087C
#define DWC_HDMI20_STATUS               0x08E0
#define DWC_HDCP22_STATUS               0x08FC

#define DWC_PDEC_DRM_HB				     0x4c0
#define DWC_PDEC_DRM_PAYLOAD0			 0x4c4
#define DWC_PDEC_DRM_PAYLOAD1			 0x4c8
#define DWC_PDEC_DRM_PAYLOAD2			 0x4cc
#define DWC_PDEC_DRM_PAYLOAD3			 0x4d0
#define DWC_PDEC_DRM_PAYLOAD4			 0x4d4
#define DWC_PDEC_DRM_PAYLOAD5			 0x4d8
#define DWC_PDEC_DRM_PAYLOAD6			 0x4dc

/*
 * hdcp register
 */
/** Register address: HDMI status */
#define DWC_HDCP_DBG             (0x0E0UL)
/*
 * Video Mode registers
 */
/** Register address: video mode control */
#define DWC_MD_HCTRL1            (0x140UL)
/** Register address: video mode control */
#define DWC_MD_HCTRL2            (0x144UL)
/** Register address: horizontal sync */
#define DWC_MD_HT0               (0x148UL)
/** Register address: horizontal offset */
#define DWC_MD_HT1               (0x14CUL)
/** Horizontal total length */
#define		HTOT_PIX				MSK(16, 16)
/** Horizontal offset length */
#define		HOFS_PIX				MSK(16, 0)
/** Register address: horizontal active length */
#define DWC_MD_HACT_PX           (0x150UL)
/** Horizontal active length */
#define		HACT_PIX				MSK(16, 0)
/** Register address: horizontal active time */
#define DWC_MD_HACT_PXA          (0x154UL)
/** Register address: vertical control */
#define DWC_MD_VCTRL             (0x158UL)
/** Register address: vertical timing - sync pulse duration */
#define DWC_MD_VSC               (0x15CUL)
/** Register address: vertical timing - frame duration */
#define DWC_MD_VTC               (0x160UL)
/** Frame duration */
#define		VTOT_CLK				(~0)
/** Register address: vertical offset length */
#define DWC_MD_VOL               (0x164UL)
/** Vertical offset length */
#define		VOFS_LIN				MSK(16, 0)
/** Register address: vertical active length */
#define DWC_MD_VAL               (0x168UL)
/** Vertical active length */
#define		VACT_LIN				MSK(16, 0)
/** Register address: vertical timing */
#define DWC_MD_VTH               (0x16CUL)
/** Register address: vertical total length */
#define DWC_MD_VTL               (0x170UL)
/** Vertical total length */
#define		VTOT_LIN				MSK(16, 0)
/** Register address: skew measurement trigger */
#define DWC_MD_IL_CTRL           (0x174UL)
/** Register address: VS and HS skew */
#define DWC_MD_IL_SKEW           (0x178UL)
/** Register address: V&H skew and filed detection */
#define DWC_MD_IL_POL            (0x17CUL)
/** Register address: video mode status */
#define DWC_MD_STS               (0x180UL)
/** Interlace active status */
#define		ILACE					_BIT(3)
/*
 * Audio registers
 */
/** Register address: audio mode control */
#define DWC_AUD_CTRL             (0x200UL)
#define DWC_AUD_HBR_ENABLE	_BIT(8)
/** Register address: audio PLL control */
#define DWC_AUD_PLL_CTRL         (0x208UL)
/** Register address: audio PLL lock */
/* #define DWC_AUD_PLL_LOCK         (0x20CUL) */
/** Register address: DDS audio clock control */
/* #define DWC_AUD_PLL_RESET        (0x210UL) */
/** Register address: audio clock control */
#define DWC_AUD_CLK_CTRL         (0x214UL)
/** Register address: ASP sync intervals */
#define DWC_AUD_CLK_MASP         (0x218UL)
/** Register address: audio sync interval */
#define DWC_AUD_CLK_MAUD         (0x21CUL)
/** Register address: sync interval reset */
#define DWC_AUD_FILT_CTRL1       (0x220UL)
/** Register address: phase filter control */
#define DWC_AUD_FILT_CTRL2       (0x224UL)
/** Register address: manual CTS control */
#define DWC_AUD_CTS_MAN          (0x228UL)
/** Register address: manual N control */
#define DWC_AUD_N_MAN            (0x22CUL)
/** Register address: audio clock status */
#define DWC_AUD_CLK_STS          (0x23CUL)
/** Register address: audio FIFO control */
#define DWC_AUD_FIFO_CTRL        (0x240UL)
/** Audio FIFO reset */
#define AFIF_INIT                 _BIT(0)
#define AFIF_SUBPACKETS           _BIT(16)
/** Register address: audio FIFO threshold */
#define DWC_AUD_FIFO_TH          (0x244UL)
/** Register address: audio FIFO fill */
#define DWC_AUD_FIFO_FILL_S      (0x248UL)
/** Register address: audio FIFO fill minimum/maximum */
#define DWC_AUD_FIFO_CLDWC_MM     (0x24CUL)
/** Register address: audio FIFO fill status */
#define DWC_AUD_FIFO_FILLSTS     (0x250UL)
/** Register address: audio output interface configuration */
#define DWC_AUD_CHEXTR_CTRL     (0x254UL)
#define AUD_CH_MAP_CFG MSK(5, 2)
/** Register address: audio mute control */
#define DWC_AUD_MUTE_CTRL        (0x258UL)
/** Manual/automatic audio mute control */
#define		AUD_MUTE_SEL			MSK(2, 5)
/** Force unmute (overrules all) */
#define		AUD_MUTE_FORCE_UN		(0)
/** Automatic mute when FIFO thresholds are exceeded */
#define		AUD_MUTE_FIFO_TH		(1)
/** Automatic mute when FIFO over/underflows */
#define		AUD_MUTE_FIFO_FL		(2)
/** Force mute (overrules all) */
#define		AUD_MUTE_FORCE			(3)
/** Register address: serial audio output control */
#define DWC_AUD_SAO_CTRL         (0x260UL)
/** Register address: parallel audio output control */
#define DWC_AUD_PAO_CTRL         (0x264UL)
/** Register address: audio FIFO status */
#define DWC_AUD_FIFO_STS         (0x27CUL)
	#define OVERFL_STS		_BIT(4)
	#define UNDERFL_STS		_BIT(3)

#define DWC_AUDPLL_GEN_CTS       (0x280UL)
#define DWC_AUDPLL_GEN_N         (0x284UL)

/** Register address: lock detector threshold */
#define DWC_CI_PLLAUDIO_5        (0x28CUL)
/** Register address: test mode selection */
#define DWC_CI_PLLAUDIO_4        (0x290UL)
/** Register address: bypass divider control */
#define DWC_CI_PLLAUDIO_3        (0x294UL)
/** Register address: monitoring */
#define DWC_CI_PLLAUDIO_2        (0x298UL)
/** Register address: control */
#define DWC_CI_PLLAUDIO_1        (0x29CUL)
/** Register address: SNPS PHY GEN3 control */
#define PDDQ_DW	_BIT(1)
#define PHY_RST	_BIT(0)
#define DWC_SNPS_PHYG3_CTRL	(0x2C0UL)
/** Register address:  I2C Master: */
/** slave address - starting version 1.30a */
#define DWC_I2CM_PHYG3_SLAVE		(0x2C4UL)
/** Register address: I2C Master: */
/** register address - starting version 1.30a */
#define DWC_I2CM_PHYG3_ADDRESS		(0x2C8UL)
/** Register address: I2C Master: */
/** data to write to slave-starting version 1.30a*/
#define DWC_I2CM_PHYG3_DATAO		(0x2CCUL)
/** Register address: I2C Master: */
/** data read from slave-starting version 1.30a */
#define DWC_I2CM_PHYG3_DATAI		(0x2D0UL)
/** Register address: I2C Master: */
/** operation RD/WR - starting version 1.30a */
#define DWC_I2CM_PHYG3_OPERATION		(0x2D4UL)
/** Register address: I2C Master: */
/** SS/HS mode - starting version 1.30a */
#define DWC_I2CM_PHYG3_MODE			(0x2D8UL)
/** Register address: I2C Master: */
/** soft reset - starting version 1.30a */
#define DWC_I2CM_PHYG3_SOFTRST		(0x2DCUL)
/** Register address: I2C Master: */
/** ss mode counters  - starting version 1.30a */
#define DWC_I2CM_PHYG3_SS_CNTS		(0x2E0UL)
/** Register address:I2C Master:  */
/** hs mode counters  - starting version 1.30a */
#define DWC_I2CM_PHYG3_FS_HCNT		(0x2E4UL)
/*
 * Packet Decoder and FIFO Control registers
 */
/** Register address: packet decoder and FIFO control */
#define DWC_PDEC_CTRL            (0x300UL)
/** Packet FIFO store filter enable */
#define PFIFO_STORE_FILTER_EN	_BIT(31)

/** Packet FIFO store packet */
#define PFIFO_DRM_EN		_BIT(29)/*type:0x87*/
#define PFIFO_AMP_EN		_BIT(28)/*type:0x0D*/
#define PFIFO_NTSCVBI_EN	_BIT(27)/*type:0x86*/
#define PFIFO_MPEGS_EN		_BIT(26)/*type:0x85*/
#define PFIFO_AUD_EN		_BIT(25)/*type:0x84*/
#define PFIFO_SPD_EN		_BIT(24)/*type:0x83*/
#define PFIFO_AVI_EN		_BIT(23)/*type:0x82*/
#define PFIFO_VS_EN			_BIT(22)/*type:0x81*/
#define PFIFO_GMT_EN		_BIT(21)/*type:0x0A*/
#define PFIFO_ISRC2_EN		_BIT(20)/*type:0x06*/
#define PFIFO_ISRC1_EN		_BIT(19)/*type:0x05*/
#define PFIFO_ACP_EN		_BIT(18)/*type:0x04*/
#define PFIFO_GCP_EN		_BIT(17)/*type:0x03*/
#define PFIFO_ACR_EN		_BIT(16)/*type:0x01*/

#define		GCP_GLOBAVMUTE			_BIT(15)
/** Packet FIFO clear min/max information */
#define		PD_FIFO_FILL_INFO_CLR	_BIT(8)
/** Packet FIFO skip one packet */
#define		PD_FIFO_SKIP			_BIT(6)
/** Packet FIFO clear */
#define		PD_FIFO_CLR				_BIT(5)
/** Packet FIFO write enable */
#define		PD_FIFO_WE				_BIT(4)
/** Packet error detection/correction enable */
#define		PDEC_BCH_EN				_BIT(0)
/** Register address: packet decoder and FIFO configuration */
#define DWC_PDEC_FIFO_CFG        (0x304UL)
/** Register address: packet decoder and FIFO status */
#define DWC_PDEC_FIFO_STS        (0x308UL)
/** Register address: packet decoder and FIFO byte data */
#define DWC_PDEC_FIFO_DATA       (0x30CUL)
/** Register address: packet decoder and FIFO debug control */
#define DWC_PDEC_DBG_CTRL        (0x310UL)/*968 966 txl define*/
#define DWC_PDEC_AUDDET_CTRL     (0x310UL)/*txlx define*/
/** Register address: packet decoder and FIFO measured timing gap */
#define DWC_PDEC_DBG_TMAX        (0x314UL)
/** Register address: CTS loop */
#define DWC_PDEC_DBG_CTS         (0x318UL)
/** Register address: ACP frequency count */
#define DWC_PDEC_DBG_ACP         (0x31CUL)
/** Register address: signal errors in data island packet */
#define DWC_PDEC_DBG_ERDWC_CORR   (0x320UL)
/** Register address: packet decoder and FIFO status */
#define DWC_PDEC_FIFO_STS1        (0x324UL)
/** Register address: CTS reset measurement control */
#define DWC_PDEC_ACRM_CTRL       (0x330UL)
/** Register address: maximum CTS div N value */
#define DWC_PDEC_ACRM_MAX        (0x334UL)
/** Register address: minimum CTS div N value */
#define DWC_PDEC_ACRM_MIN        (0x338UL)
#define DWC_PDEC_ERR_FILTER			(0x33CUL)
/** Register address: audio sub packet control */
#define DWC_PDEC_ASP_CTRL        (0x340UL)
/** Automatic mute all video channels */
#define		AUTO_VMUTE				_BIT(6)
/** Automatic mute audio sub packets */
#define		AUTO_SPFLAT_MUTE		MSK(4, 2)
/** Register address: audio sub packet errors */
#define DWC_PDEC_ASP_ERR         (0x344UL)
/** Register address: packet decoder status, see packet interrupts */
#define PD_NEW_ENTRY	MSK(1, 8)
#define PD_TH_START		MSK(1, 2)
#define PD_AUD_LAYOUT	_BIT(11)
#define DWC_PDEC_STS             (0x360UL)
/** Register address: Packet Decoder Audio Status*/
#define DWC_PDEC_AUD_STS         (0x364UL)
#define AUDS_RCV					MSK(1, 0)
#define AUDS_HBR_RCV			_BIT(3)
/** Register address: general control packet AV mute */
#define DWC_PDEC_GCP_AVMUTE      (0x380UL)
/** Register address: audio clock regeneration */
#define DWC_PDEC_ACR_CTS        (0x390UL)
/** Audio clock regeneration, CTS parameter */
#define		CTS_DECODED				MSK(20, 0)
/** Register address: audio clock regeneration */
#define DWC_PDEC_ACR_N		(0x394UL)
/** Audio clock regeneration, N parameter */
#define		N_DECODED				MSK(20, 0)
/** Register address: auxiliary video information info frame */
#define DWC_PDEC_AVI_HB		(0x3A0UL)
/** PR3-0, pixel repetition factor */
#define		PIX_REP_FACTOR			MSK(4, 24)
/** Q1-0, YUV quantization range */
#define		YUV_QUANT_RANGE			MSK(2, 30)
/** Register address: auxiliary video information info frame */
#define DWC_PDEC_AVI_PB		(0x3A4UL)
/** VIC6-0, video mode identification code */
#define		VID_IDENT_CODE			MSK(8, 24)
/** ITC, IT content */
#define		IT_CONTENT				_BIT(23)
/** EC2-0, extended colorimetry */
#define		EXT_COLORIMETRY			MSK(3, 20)
/** Q1-0, RGB quantization range */
#define		RGB_QUANT_RANGE			MSK(2, 18)
/** SC1-0, non-uniform scaling information */
#define		NON_UNIF_SCALE			MSK(2, 16)
/** C1-0, colorimetry information */
#define		COLORIMETRY				MSK(2, 14)
/** M1-0, picture aspect ratio */
#define		PIC_ASPECT_RATIO		MSK(2, 12)
/** R3-0, active format aspect ratio */
#define		ACT_ASPECT_RATIO		MSK(4, 8)
/** Y1-0, video format */
#define		VIDEO_FORMAT			MSK(2, 5)
/** A0, active format information present */
#define		ACT_INFO_PRESENT		_BIT(4)
/** B1-0, bar valid information */
#define		BAR_INFO_VALID			MSK(2, 2)
/** S1-0, scan information from packet extraction */
#define		SCAN_INFO				MSK(2, 0)
/** Register address: auxiliary video information info frame */
#define DWC_PDEC_AVI_TBB		(0x3A8UL)
/** Line number to start of bottom bar */
#define		LIN_ST_BOT_BAR			MSK(16, 16)
/** Line number to end of top bar */
#define		LIN_END_TOP_BAR			MSK(16, 0)
/** Register address: auxiliary video information info frame */
#define DWC_PDEC_AVI_LRB		(0x3ACUL)
/** Pixel number of start right bar */
#define		PIX_ST_RIG_BAR			MSK(16, 16)
/** Pixel number of end left bar */
#define		PIX_END_LEF_BAR			MSK(16, 0)
/** Register addr: special audio layout control for multi-channel audio */
#define DWC_PDEC_AIF_CTRL	(0x3C0UL)
/** Register address: audio info frame */
#define DWC_PDEC_AIF_HB		(0x3C4UL)
/** Register address: audio info frame */
#define DWC_PDEC_AIF_PB0		(0x3C8UL)
/** CA7-0, channel/speaker allocation */
#define		CH_SPEAK_ALLOC			MSK(8, 24)
/** CTX, coding extension */
#define		AIF_DATA_BYTE_3			MSK(8, 16)
/** SF2-0, sample frequency */
#define		SAMPLE_FREQ				MSK(3, 10)
/** SS1-0, sample size */
#define		SAMPLE_SIZE				MSK(2, 8)
/** CT3-0, coding type */
#define		CODING_TYPE				MSK(4, 4)
/** CC2-0, channel count */
#define		CHANNEL_COUNT			MSK(3, 0)
/** Register address: audio info frame */
#define DWC_PDEC_AIF_PB1		(0x3CCUL)
/** DM_INH, down-mix inhibit */
#define		DWNMIX_INHIBIT			_BIT(7)
/** LSV3-0, level shift value */
#define		LEVEL_SHIFT_VAL			MSK(4, 3)
/** Register address: gamut sequence number */
#define DWC_PDEC_GMD_HB		(0x3D0UL)
/** Register address: gamut meta data */
#define DWC_PDEC_GMD_PB		(0x3D4UL)

/* Vendor Specific Info Frame */
#define DWC_PDEC_VSI_ST0         (0x3E0UL)
#define IEEE_REG_ID         MSK(24, 0)

#define DWC_PDEC_VSI_ST1         (0x3E4UL)
#define H3D_STRUCTURE       MSK(4, 16)
#define H3D_EXT_DATA        MSK(4, 20)
#define HDMI_VIDEO_FORMAT   MSK(3, 5)
#define VSI_LENGTH	    MSK(5, 0)

#define DWC_PDEC_VSI_PLAYLOAD0 (0x368UL)
#define DWC_PDEC_VSI_PLAYLOAD1 (0x36CUL)
#define DWC_PDEC_VSI_PLAYLOAD2 (0x370UL)
#define DWC_PDEC_VSI_PLAYLOAD3 (0x374UL)
#define DWC_PDEC_VSI_PLAYLOAD4 (0x378UL)
#define DWC_PDEC_VSI_PLAYLOAD5 (0x37CUL)

#define DWC_PDEC_VSI_PB0 (0x3e8UL)
#define DWC_PDEC_VSI_PB1 (0x3ecUL)
#define DWC_PDEC_VSI_PB2 (0x3f0UL)
#define DWC_PDEC_VSI_PB3 (0x3f4UL)
#define DWC_PDEC_VSI_PB4 (0x3f8UL)
#define DWC_PDEC_VSI_PB5 (0x3fcUL)

#define DWC_PDEC_AMP_HB			(0x480UL)
#define	DWC_PDEC_AMP_PB0		(0x484UL)
#define	DWC_PDEC_AMP_PB1		(0x488UL)
#define	DWC_PDEC_AMP_PB2		(0x48cUL)
#define	DWC_PDEC_AMP_PB3		(0x490UL)
#define	DWC_PDEC_AMP_PB4		(0x494UL)
#define	DWC_PDEC_AMP_PB5		(0x498UL)
#define	DWC_PDEC_AMP_PB6		(0x49cUL)

#define DWC_PDEC_NTSCVBI_HB		(0x4a0UL)
#define	DWC_PDEC_NTSCVBI_PB0	(0x4a4UL)
#define	DWC_PDEC_NTSCVBI_PB1	(0x4a8UL)
#define	DWC_PDEC_NTSCVBI_PB2	(0x4acUL)
#define	DWC_PDEC_NTSCVBI_PB3	(0x4b0UL)
#define	DWC_PDEC_NTSCVBI_PB4	(0x4b4UL)
#define	DWC_PDEC_NTSCVBI_PB5	(0x4b8UL)
#define	DWC_PDEC_NTSCVBI_PB6	(0x4bcUL)

/*
 * DTL Interface registers
 */
/** Register address: dummy register for testing */
#define DWC_DUMMY_IP_REG		(0xF00UL)
/*
 * Packet Decoder Interrupt registers
 */
/** Register address: packet decoder interrupt clear enable */
#define DWC_PDEC_IEN_CLR		(0xF78UL)
/** Register address: packet decoder interrupt set enable */
#define DWC_PDEC_IEN_SET		(0xF7CUL)
/** Register address: packet decoder interrupt status */
#define DWC_PDEC_ISTS		(0xF80UL)
/** Register address: packet decoder interrupt enable */
#define DWC_PDEC_IEN		(0xF84UL)
/** Register address: packet decoder interrupt clear status */
#define DWC_PDEC_ICLR		(0xF88UL)
/** Register address: packet decoder interrupt set status */
#define DWC_PDEC_ISET		(0xF8CUL)
/** Drm set entry txlx*/
#define		DRM_CKS_CHG_TXLX			_BIT(31)
#define		DRM_RCV_EN_TXLX				_BIT(30)
/** DVI detection status */
#define		DVIDET					_BIT(28)
/** Vendor Specific Info frame changed */
#define		VSI_CKS_CHG				_BIT(27)

/** AIF checksum changed */
#define		AIF_CKS_CHG				_BIT(25)
/** AVI checksum changed */
#define		AVI_CKS_CHG				_BIT(24)
/** GCP AVMUTE changed */
#define		GCP_AV_MUTE_CHG			_BIT(21)

#define		GCP_RCV					_BIT(16)
/** Vendor Specific Info frame rcv*/
#define		VSI_RCV				_BIT(15)
/** Drm set entry */
#define		DRM_CKS_CHG				_BIT(10)
#define	DRM_RCV_EN				_BIT(9)
/** Packet FIFO new entry */
#define		PD_FIFO_NEW_ENTRY		_BIT(8)
/** Packet FIFO overflow */
#define		PD_FIFO_OVERFL			_BIT(4)
/** Packet FIFO underflow */
#define		PD_FIFO_UNDERFL			_BIT(3)
#define		PD_FIFO_START_PASS		_BIT(2)
/*
 * Audio Clock Interrupt registers
 */
/** Register address: audio clock and cec interrupt clear enable */
#define DWC_AUD_CEC_IEN_CLR	(0xF90UL)
/** Register address: audio clock and cec interrupt set enable */
#define DWC_AUD_CEC_IEN_SET	(0xF94UL)
/** Register address: audio clock and cec interrupt status */
#define DWC_AUD_CEC_ISTS		(0xF98UL)
/** Register address: audio clock and cec interrupt enable */
#define DWC_AUD_CEC_IEN		(0xF9CUL)
/** Register address: audio clock and cec interrupt clear status */
#define DWC_AUD_CEC_ICLR		(0xFA0UL)
/** Register address: audio clock and cec interrupt set status */
#define DWC_AUD_CEC_ISET		(0xFA4UL)
/*
 * Audio FIFO Interrupt registers
 */
/** Register address: audio FIFO interrupt clear enable */
#define DWC_AUD_FIFO_IEN_CLR	(0xFA8UL)
/** Register address: audio FIFO interrupt set enable */
#define DWC_AUD_FIFO_IEN_SET	(0xFACUL)
/** Register address: audio FIFO interrupt status */
#define DWC_AUD_FIFO_ISTS	(0xFB0UL)
/** Register address: audio FIFO interrupt enable */
#define DWC_AUD_FIFO_IEN		(0xFB4UL)
/** Register address: audio FIFO interrupt clear status */
#define DWC_AUD_FIFO_ICLR	(0xFB8UL)
/** Register address: audio FIFO interrupt set status */
#define DWC_AUD_FIFO_ISET	(0xFBCUL)
/** Audio FIFO overflow interrupt */
#define	OVERFL		_BIT(4)
/** Audio FIFO underflow interrupt */
#define	UNDERFL		_BIT(3)
/*
 * Mode Detection Interrupt registers
 */
/** Register address: mode detection interrupt clear enable */
#define DWC_MD_IEN_CLR		(0xFC0UL)
/** Register address: mode detection interrupt set enable */
#define DWC_MD_IEN_SET		(0xFC4UL)
/** Register address: mode detection interrupt status */
#define DWC_MD_ISTS		(0xFC8UL)
/** Register address: mode detection interrupt enable */
#define DWC_MD_IEN		(0xFCCUL)
/** Register address: mode detection interrupt clear status */
#define DWC_MD_ICLR		(0xFD0UL)
/** Register address: mode detection interrupt set status */
#define DWC_MD_ISET		(0xFD4UL)
/** Video mode interrupts */
#define	VIDEO_MODE		(MSK(3, 9)|MSK(2, 6)|_BIT(3))
/*
 * HDMI Interrupt registers
 */
/** Register address: HDMI interrupt clear enable */
#define DWC_HDMI_IEN_CLR		(0xFD8UL)
/** Register address: HDMI interrupt set enable */
#define DWC_HDMI_IEN_SET		(0xFDCUL)
/** Register address: HDMI interrupt status */
#define DWC_HDMI_ISTS		(0xFE0UL)
/** Register address: HDMI interrupt enable */
#define DWC_HDMI_IEN		(0xFE4UL)
/** Register address: HDMI interrupt clear status */
#define DWC_HDMI_ICLR		(0xFE8UL)
/** Register address: HDMI interrupt set status */
#define DWC_HDMI_ISET		(0xFECUL)
/** AKSV receive interrupt */
#define		AKSV_RCV				_BIT(25)
#define	SCDC_TMDS_CFG_CHG		_BIT(19)
/** Deep color mode change interrupt */
#define		DCM_CURRENT_MODE_CHG	_BIT(16)
#define		CTL3			_BIT(13)
#define		CTL2			_BIT(12)
#define		CTL1			_BIT(11)
#define		CTL0			_BIT(10)

/** Clock change interrupt */
#define		CLK_CHANGE				_BIT(6)
#define		PLL_LCK_CHG				_BIT(5)

#define DWC_HDMI2_IEN_CLR		(0xf60UL)
#define DWC_HDMI2_IEN_SET		(0xF64UL)
#define DWC_HDMI2_ISTS			(0xF68UL)
#define DWC_HDMI2_IEN			(0xF6CUL)
#define DWC_HDMI2_ICLR			(0xF70UL)

/*
 * DMI registers
 */
/** Register address: DMI software reset */
#define DWC_DMI_SW_RST          (0xFF0UL)
#define		IAUDIOCLK_DOMAIN_RESET	_BIT(4)
/** Register address: DMI disable interface */
#define DWC_DMI_DISABLE_IF      (0xFF4UL)
/** Register address: DMI module ID */
#define DWC_DMI_MODULE_ID       (0xFFCUL)

/*
 * HDCP registers
 */
/** Register address: control */
#define DWC_HDCP_CTRL			(0x0C0UL)
/** HDCP enable */
#define		HDCP_ENABLE		_BIT(24)
/** HDCP key decryption */
#define		KEY_DECRYPT_ENABLE		_BIT(1)
/** HDCP activation */
#define		ENCRIPTION_ENABLE				_BIT(0)
/** Register address: configuration */
#define DWC_HDCP_SETTINGS		(0x0C4UL)
/*fast mode*/
#define HDCP_FAST_MODE			_BIT(12)
#define HDCP_BCAPS				_BIT(13)
/** Register address: key description seed */
#define DWC_HDCP_SEED			(0x0C8UL)
/** Register address: receiver key selection */
#define DWC_HDCP_BKSV1			(0x0CCUL)
/** Register address: receiver key selection */
#define DWC_HDCP_BKSV0			(0x0D0UL)
/** Register address: key index */
#define DWC_HDCP_KIDX			(0x0D4UL)
/** Register address: encrypted key */
#define DWC_HDCP_KEY1			(0x0D8UL)
/** Register address: encrypted key */
#define DWC_HDCP_KEY0			(0x0DCUL)
/** Register address: debug */
#define DWC_HDCP_DBG				(0x0E0UL)
/** Register address: transmitter key selection vector */
#define DWC_HDCP_AKSV1			(0x0E4UL)
/** Register address: transmitter key selection vector */
#define DWC_HDCP_AKSV0			(0x0E8UL)
/** Register address: session random number */
#define DWC_HDCP_AN1				(0x0ECUL)
/** Register address: session random number */
#define DWC_HDCP_AN0			(0x0F0UL)
/** Register address: EESS, WOO */
#define DWC_HDCP_EESS_WOO		(0x0F4UL)
/** Register address: key set writing status */
#define DWC_HDCP_STS				(0x0FCUL)
/** HDCP encrypted status */
#define ENCRYPTED_STATUS			_BIT(9)
/** HDCP key set writing status */
#define		HDCP_KEY_WR_OK_STS		_BIT(0)
/** Register address: repeater KSV list control */
#define	DWC_HDCP_RPT_CTRL		(0x600UL)
/** KSV list key set writing status */
#define		KSV_HOLD				_BIT(6)
/** KSV list waiting status */
#define		WAITING_KSV				_BIT(5)
/** V` waiting status */
#define		FIFO_READY				_BIT(4)
/** Repeater capability */
#define		REPEATER				_BIT(3)
/** KSV list ready */
#define		KSVLIST_READY			_BIT(2)
#define		KSVLIST_TIMEOUT			_BIT(1)
#define		KSVLIST_LOSTAUTH		_BIT(0)
/** Register address: repeater status */
#define	DWC_HDCP_RPT_BSTATUS		(0x604UL)
/** Topology error indicator */
#define		MAX_CASCADE_EXCEEDED	_BIT(11)
/** Repeater cascade depth */
#define		DEPTH					MSK(3, 8)
/** Topology error indicator */
#define		MAX_DEVS_EXCEEDED		_BIT(7)
/** Attached downstream device count */
#define		DEVICE_COUNT			MSK(7, 0)
/** Register address: repeater KSV FIFO control */
#define	DWC_HDCP_RPT_KSVFIFOCTRL	(0x608UL)
/** Register address: repeater KSV FIFO */
#define	DWC_HDCP_RPT_KSVFIFO1	(0x60CUL)
/** Register address: repeater KSV FIFO */
#define	DWC_HDCP_RPT_KSVFIFO0	(0x610UL)

/*
 * ESM registers
 */

/** HPI Register */
#define HPI_REG_IRQ_STATUS				0x24
#define IRQ_STATUS_UPDATE_BIT			_BIT(3)
#define HPI_REG_EXCEPTION_STATUS		0x60
#define EXCEPTION_CODE					MSK(8, 1)
#define AUD_PLL_THRESHOLD	1000000

#define TMDS_CLK_MIN			(24000UL)
#define TMDS_CLK_MAX			(340000UL)

extern unsigned int hdmirx_addr_port;
extern unsigned int hdmirx_data_port;
extern unsigned int hdmirx_ctrl_port;
extern int acr_mode;
extern int force_clk_rate;
extern int auto_aclk_mute;
extern int aud_avmute_en;
extern int aud_mute_sel;
extern int pdec_ists_en;
extern int pd_fifo_start_cnt;
extern int md_ists_en;
extern int eq_ref_voltage;
extern int aud_ch_map;

extern void wr_reg_hhi(unsigned int offset, unsigned int val);
extern unsigned int rd_reg_hhi(unsigned int offset);
extern unsigned int rd_reg(enum map_addr_module_e module,
		unsigned int reg_addr);
extern void wr_reg(enum map_addr_module_e module,
		unsigned int reg_addr,
		unsigned int val);
extern void hdmirx_wr_top(unsigned int addr, unsigned int data);
extern unsigned int hdmirx_rd_top(unsigned int addr);
extern void hdmirx_wr_dwc(unsigned int addr, unsigned int data);
extern unsigned int hdmirx_rd_dwc(unsigned int addr);
extern unsigned int hdmirx_rd_bits_dwc(unsigned int addr,
	unsigned int mask);
extern void hdmirx_wr_bits_dwc(unsigned int addr,
	unsigned int mask, unsigned int value);
extern unsigned int hdmirx_wr_phy(unsigned int add,
	unsigned int data);
extern unsigned int hdmirx_rd_phy(unsigned int addr);
extern void hdmirx_wr_bits_top(unsigned int addr,
	unsigned int mask, unsigned int value);
extern unsigned int rx_get_bits(unsigned int data,
	unsigned int mask);
extern unsigned int rx_set_bits(unsigned int data,
	unsigned int mask, unsigned int value);
extern unsigned int hdmirx_get_tmds_clock(void);
extern unsigned int hdmirx_get_pixel_clock(void);
extern unsigned int hdmirx_get_audio_clock(void);
extern unsigned int hdmirx_get_esm_clock(void);
extern unsigned int hdmirx_get_mpll_div_clk(void);
extern unsigned int hdmirx_get_clock(int index);
extern unsigned int meson_clk_measure(unsigned int clk_mux);

/* hdcp22 */
extern void rx_hdcp22_wr_reg(unsigned int addr, unsigned int data);
extern unsigned int rx_hdcp22_rd_reg(unsigned int addr);
extern unsigned int rx_hdcp22_rd_bits_reg(unsigned int addr,
	unsigned int mask);
extern void rx_hdcp22_wr_bits_reg(unsigned int addr,
	unsigned int mask, unsigned int value);
extern void rx_hdcp22_wr_top(unsigned int addr, unsigned int data);
extern unsigned int rx_hdcp22_rd_top(unsigned int addr);
extern unsigned int rx_hdcp22_rd(unsigned int addr);
extern void rx_sec_reg_write(unsigned int *addr, unsigned int value);
extern void rx_sec_reg_write(unsigned int *addr, unsigned int value);
extern unsigned int rx_sec_reg_read(unsigned int *addr);
extern unsigned int sec_top_read(unsigned int *addr);
extern void sec_top_write(unsigned int *addr, unsigned int value);
extern void rx_esm_tmdsclk_en(bool en);
extern int hdcp22_on;
extern bool hdcp22_kill_esm;
extern bool hpd_to_esm;
extern void hdcp22_clk_en(bool en);
extern void hdmirx_hdcp22_esm_rst(void);
extern unsigned int rx_sec_set_duk(void);
extern void hdmirx_hdcp22_init(void);
extern void hdcp22_suspend(void);
extern void hdcp22_resume(void);
extern void hdmirx_hdcp22_hpd(bool value);
extern void esm_set_reset(bool reset);
extern void esm_set_stable(bool stable);
extern void rx_hpd_to_esm_handle(struct work_struct *work);


extern unsigned int hdmirx_packet_fifo_rst(void);
extern unsigned int hdmirx_audio_fifo_rst(void);
extern void hdmirx_phy_init(void);
extern void hdmirx_hw_config(void);
extern void hdmirx_hw_probe(void);
extern void rx_hdcp_init(void);
extern void hdmirx_phy_pddq(unsigned int enable);
extern void rx_get_video_info(void);
extern void hdmirx_set_video_mute(bool mute);
extern void hdmirx_config_video(void);
extern void hdmirx_config_audio(void);
extern void rx_get_audinfo(struct aud_info_s *audio_info);
extern bool rx_clkrate_monitor(void);

extern unsigned char rx_get_hdcp14_sts(void);
extern unsigned int rx_hdcp22_rd_reg_bits(unsigned int addr, unsigned int mask);
extern int rx_get_aud_pll_err_sts(void);
extern void rx_force_hpd_cfg(uint8_t hpd_level);
#endif


