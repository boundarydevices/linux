/*------------------------------------------------------------------------------
 * Module Name HDMIRX
 * Module Description: this file is used to hadle misc RX functions
 *
 * Copyright © 2002-2005, Silicon Image, Inc.  All rights reserved.
 *
 * No part of this work may be reproduced, modified, distributed, transmitted,
 * transcribed, or translated into any language or computer format, in any form
 * or by any means without written permission of: Silicon Image, Inc.,
 * 1060 East Arques Avenue, Sunnyvale, California 94085
 *----------------------------------------------------------------------------
 */

#include "SiIGlob.h"
#include "SiITrace.h"
#include "SiIRXDefs.h"
#include "SiIRXIO.h"
#include "SiIHDMIRX.h"
#include "SiIHAL.h"
#include "SiIVidIn.h"
#include "SiIInfoPkts.h"
#include "SiITTVideo.h"
#include "SiITTAudio.h"
#include "SiIAudio.h"
#include "SiIDeepColor.h"

#include "../hdmirx_ext_drv.h"

static BOOL bSeparateAFE;

/*------------------------------------------------------------------------------
 * Function Name: siiRXHardwareReset
 * Function Description: This function makes hardware reset of RX,
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiRXHardwareReset(void)
{
	halAssertResetHDMIRXPin();
	halDelayMS(1);
	halReleaseResetHDMIRXPin();
	halDelayMS(5);
}
/*------------------------------------------------------------------------------
 * Function Name: siiSetMasterClock
 * Function Description: Sets Master Clock divider which used for Audio Output
 *
 * Accepts: BYTE
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiSetMasterClock(BYTE bDividerIndex)
{
	BYTE bRegVal;

	bRegVal = siiReadByteHDMIRXP1(RX_FREQ_SVAL_ADDR) & 0x0F;  /*reg 102 */
	siiWriteByteHDMIRXP1(RX_FREQ_SVAL_ADDR, (bRegVal |
		(bDividerIndex << 6) | (bDividerIndex << 4)));
}

/*------------------------------------------------------------------------------
 * Function Name: siiSetHBRMasterClock
 * Function Description: Sets Master Clock divider which used for Audio Output
 *                       Sets the Fs output as Fs input / 4
 * Accepts: BYTE
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiSetHBRFs(BOOL qON)
{
	BYTE bECode = FALSE;
	BYTE bRegVal;
	BYTE bMCLK;

	if (qON) {
		/* reg 102 */
		bMCLK = siiReadByteHDMIRXP1(RX_FREQ_SVAL_ADDR) & 0xF0;
		/* reg 117 */
		bRegVal = siiReadByteHDMIRXP1(RX_PCLK_FS_ADDR) & 0x0F;
		switch (bRegVal) {
		case _192Fs: /* 192K */
			bRegVal = _48Fs; /* 48K */
			break;
		case _768Fs: /* 768k */
			bRegVal = _192Fs; /* 192K */
			break;
		/*not HBR*/
		/*case _176Fs: // 176.4K
		 *	bRegVal = _44Fs; // 44.10K
		 *	break;
		 */
		default: /* YMA 08/17/06 handle invalid values? */
			break;
		}
		siiWriteByteHDMIRXP1(RX_FREQ_SVAL_ADDR, bRegVal | bMCLK);

		/* enable sw manually set Fs //100.1 */
		siiIIC_RX_RWBitsInByteP1(RX_ACR_CTRL1_ADDR,
			RX_BIT_ACR_FSSEL, SET);
	} else {
		siiIIC_RX_RWBitsInByteP1(RX_ACR_CTRL1_ADDR,
			RX_BIT_ACR_FSSEL, CLR);
	}

	siiErrorHandler(bECode);
}

/*------------------------------------------------------------------------------
 * Function Name: siiIsHDMI_Mode
 * Function Description: this function checks if RX in HDMI mode
 *
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
BOOL siiIsHDMI_Mode(void)
{
	BOOL qResult = FALSE;

	if (siiReadByteHDMIRXP1(RX_AUDP_STAT_ADDR) & RX_BIT_HDMI_EN_MASK)
		qResult = TRUE;
	return qResult;
}
/*------------------------------------------------------------------------------
 * Function Name:  siiGetRXDeviceInfo
 * Function Description: reads RX Dev virsion/revision data from RX registers
 *
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiGetRXDeviceInfo(BYTE *pbChipVer)
{
	siiReadBlockHDMIRXP0(RX_DEV_IDL_ADDR, 3, pbChipVer);
}

/*------------------------------------------------------------------------------
 * Function Name: siiRX_PowerDown
 * Function Description:  sets or claer main power down mode
 *
 * Accepts: bPowerDown
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiRX_PowerDown(BYTE bPowerDown)
{
	if (bPowerDown == ON) {
		siiIIC_RX_RWBitsInByteP0(RX_SYS_CTRL1_ADDR, RX_BIT_PD_ALL, CLR);
		if (bSeparateAFE == PRESENT) {
			siiIIC_RX_RWBitsInByteU0(RX_SYS_CTRL1_ADDR,
				RX_BIT_PD_ALL, CLR);
		}
	} else {
		siiIIC_RX_RWBitsInByteP0(RX_SYS_CTRL1_ADDR, RX_BIT_PD_ALL, SET);
		if (bSeparateAFE == PRESENT) {
			siiIIC_RX_RWBitsInByteU0(RX_SYS_CTRL1_ADDR,
				RX_BIT_PD_ALL, SET);
		}
	}
}

/*------------------------------------------------------------------------------
 * Function Name: siiRX_DisableTMDSCores
 * Function Description: Sets global power down mode
 *
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiRX_DisableTMDSCores(void)
{
	siiIIC_RX_RWBitsInByteP0(RX_SYS_SW_SWTCHC_ADDR,
		(RX_BIT_RX0_EN | RX_BIT_DDC0_EN |
		RX_BIT_RX1_EN | RX_BIT_DDC1_EN), CLR);
	if (bSeparateAFE == PRESENT) {
		siiIIC_RX_RWBitsInByteU0(RX_SYS_SW_SWTCHC_ADDR,
			RX_BIT_RX0_EN, CLR);
	}
}

/*------------------------------------------------------------------------------
 * Function Name: sii_SetVideoOutputPowerDown
 * Function Description:
 *
 * Accepts: bPowerState
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
void sii_SetVideoOutputPowerDown(BYTE bVideoOutputPowerDown)
{
	switch (bVideoOutputPowerDown) {

	case SiI_RX_VidOutPD_NoPD:
		siiIIC_RX_RWBitsInByteP1(RX_PD_SYS_ADDR,
			RX_BIT_NPD_VIDEO_DAC, SET);
		siiIIC_RX_RWBitsInByteP1(RX_PD_SYS2_ADDR,
			RX_BIT_NPD_DIGITAL_OUTPUTS, SET);
		break;
	case SiI_RX_VidOutPD_Analog:
		siiIIC_RX_RWBitsInByteP1(RX_PD_SYS_ADDR,
			RX_BIT_NPD_VIDEO_DAC, CLR);
		siiIIC_RX_RWBitsInByteP1(RX_PD_SYS2_ADDR,
			RX_BIT_NPD_DIGITAL_OUTPUTS, SET);
		break;
	case SiI_RX_VidOutPD_AnalogAndDigital:
		siiIIC_RX_RWBitsInByteP1(RX_PD_SYS_ADDR,
			RX_BIT_NPD_VIDEO_DAC, CLR);
		siiIIC_RX_RWBitsInByteP1(RX_PD_SYS2_ADDR,
			RX_BIT_NPD_DIGITAL_OUTPUTS, CLR);
		break;
	case SiI_RX_VidOutPD_Digital:
		siiIIC_RX_RWBitsInByteP1(RX_PD_SYS_ADDR,
			RX_BIT_NPD_VIDEO_DAC, SET);
		siiIIC_RX_RWBitsInByteP1(RX_PD_SYS2_ADDR,
			RX_BIT_NPD_DIGITAL_OUTPUTS, CLR);
		break;
	}
}
/*------------------------------------------------------------------------------
 * Function Name: siiRX_GlobalPower
 * Function Description: Sets or takes from most saving power down mode
 *
 * Accepts: bPowerState
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiRX_GlobalPower(BYTE bPowerState)
{
	if (bPowerState == SiI_RX_Power_Off) {
		siiSetSM_ReqGlobalPowerDown();
		siiSetAudioMuteEvent();
	} else if (SiI_Ctrl.sm_bVideo != SiI_RX_VS_VideoOn) {
		siiChangeVideoInput(SiI_Ctrl.bVidInChannel);
	}
}
/*------------------------------------------------------------------------------
 * Function Name: siiRX_CheckCableHPD
 * Function Description: Checks if HDMI cable has hot plug
 *
 * Accepts: none
 * Returns: BOOL hot plug state
 * Globals: none
 *----------------------------------------------------------------------------
 */
BOOL siiRX_CheckCableHPD(void)
{
	BOOL qResult = FALSE;

	/* YMA NOTE /R0x06/bit3/ 5V  ower detect */
	if (siiReadByteHDMIRXP0(RX_STATE_ADDR) & RX_BIT_CABLE_HPD)
		qResult = TRUE;
	return qResult;
}
/*------------------------------------------------------------------------------
 * Function Name: siiClearBCHCounter
 * Function Description: clears BCH counter. The counter accomulates BCH errors
 *                       if counter won't be clear it can cause HDCP interrupts
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiClearBCHCounter(void)
{
	siiIIC_RX_RWBitsInByteP0(RX_ECC_CTRL_ADDR, RX_BIT_CLR_BCH_COUNTER, SET);
}
/*------------------------------------------------------------------------------
 * Function Name: InitAECRegisters
 * Function Description: Initialization of Audio Exception registers
 *                       which used for auto-muting
 *----------------------------------------------------------------------------
 */
static void InitAECRegisters(void)
{
	BYTE abData[3];

	abData[0] = 0xC1;
	abData[1] = 0x87;
	abData[2] = 0x01;
	siiWriteBlockHDMIRXP0(RX_AEC1_ADDR, 3, abData);
}
/*------------------------------------------------------------------------------
 * Function Name: siiRX_InitializeInterrupts
 * Function Description: Initialize RX interrupts
 *                       (used with polling RX interrupt pin)
 *----------------------------------------------------------------------------
 */
static void RX_InitializeInterrupts(void)
{
	BYTE abData[4];

	abData[0] = 0x42;  /* N change, used in repeater configuration */
			   /* HDCP Start */
	abData[1] = 0x88;  /* HDMI Change SCDT int */
	abData[2] = 0x9F;  /* infoFrame interrupts enable */
	abData[3] = 0x40;  /* HDCP Failed */
	siiWriteBlockHDMIRXP0(RX_HDMI_INT_GR1_MASK_ADDR, 4, abData);
	abData[0] = 0x58;  /* ACC done enable,  VRes Change, HRes Change */
	abData[1] = 0x00;  /* Aud Ch Status Ready */
	siiWriteBlockHDMIRXP0(RX_HDMI_INT_GR2_MASK_ADDR, 2, abData);
	siiWriteByteHDMIRXP0(RX_INFO_FRAME_CTRL_ADDR, RX_BIT_NEW_ACP);
}
/*------------------------------------------------------------------------------
 * Function Name: InitSoftwareResetReg
 * Function Description: Initialize software reset register
 *----------------------------------------------------------------------------
 */
void InitSoftwareResetReg(void)
{
#ifndef SII_BUG_BETTY_PORT1_BUG
	/* when no SCDT software reset is applied */
	siiWriteByteHDMIRXP0(RX_SWRST_ADDR, RX_BIT_SW_AUTO);
#endif

	if (bSeparateAFE == PRESENT) {
		/* when no SCDT software reset is applied */
		siiWriteByteAFEU0(RX_SWRST_ADDR, RX_BIT_SW_AUTO);

		/* IP requires HDCP reset at start */
		siiIIC_RX_RWBitsInByteP0(RX_SWRST_ADDR, 0x08, SET);
		siiIIC_RX_RWBitsInByteP0(RX_SWRST_ADDR, 0x08, CLR);
	}
}

#ifdef SII_BUG_PHOEBE_AUTOSW_BUG
/*------------------------------------------------------------------------------
 * Function Name: SetAutoSWReset
 * Function Description:  this function enables or disables Auto Software reset
 * Accepts: BOOL qOn
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiSetAutoSWReset(BOOL qOn)
{
	if ((SiI_Ctrl.bDevId == RX_SiI9023) ||
		(SiI_Ctrl.bDevId == RX_SiI9033)) {
		if (qOn) {
			siiIIC_RX_RWBitsInByteP0(RX_SWRST_ADDR,
				RX_BIT_SW_AUTO, SET);
		} else {
			siiIIC_RX_RWBitsInByteP0(RX_SWRST_ADDR,
				RX_BIT_SW_AUTO, CLR);
		}
	}
}
#endif
/*------------------------------------------------------------------------------
 * Function Name: GetRXDevId
 * Function Description: used for detection onboard HDMI RX
 *----------------------------------------------------------------------------
 */
BYTE GetRXDevId(void)
{
	BYTE abDevInfo[3];
	WORD wDevId;
	BYTE bDevId = 0;

	siiGetRXDeviceInfo(abDevInfo);
	wDevId = abDevInfo[0] | (abDevInfo[1] << 8);
	switch (wDevId) {
	case SiI9993:
		bDevId = RX_SiI9993;
		break;
	case SiI9031:
		bDevId = RX_SiI9031;
		break;
	case SiI9021:
		bDevId = RX_SiI9021;
		break;
	case SiI9023:
		bDevId = RX_SiI9023;
		break;
	case SiI9033:
		bDevId = RX_SiI9033;
		break;
	case SiI9011:
		bDevId = RX_SiI9011;
		break;
	case SiI9051:
		bDevId = RX_SiI9051;
		break;
	case SiI9133:
		bDevId = RX_SiI9133;
		break;
	case SiI9135:
		bDevId = RX_SiI9135;
		break;
	case SiI9125:
		bDevId = RX_SiI9125;
		break;
	default:
		bDevId = RX_Unknown;
	}
	RXEXTPR("%s: abDevInfo=0x%02x,0x%02x, wDevId=0x%04x, bDevId=%d\n",
		__func__, abDevInfo[0], abDevInfo[1], wDevId, bDevId);
	return bDevId;
}

/*------------------------------------------------------------------------------
 * Function Name: siiSetAFEClockDelay
 * Function Description: Set output clock depay for Analog Front End
 *----------------------------------------------------------------------------
 */

void siiSetAFEClockDelay(void)
{
#if 0
	BYTE bClkCount;
	BYTE bDDRdelay;
	BYTE abData[2] = {0x00, 0x60};

	if (bSeparateAFE == PRESENT) {
		bClkCount = siiReadByteAFEU0(0x11);
		printf("Count: 0x%02x\n", (int)bClkCount);

		if (bClkCount < 0x40)
			bDDRdelay = abData[0];
		else
			bDDRdelay = abData[1];

		siiWriteByteAFEU0(RX_AFE_DDR_DSA_ADDR, bDDRdelay);
		printf("DDRdelay: 0x%02x\n", (int)bDDRdelay);
	}
#endif
}

/*------------------------------------------------------------------------------
 * Function Name: siiInitAFE
 * Function Description: Initialize the Analog Front End
 *----------------------------------------------------------------------------
 */
void siiInitAFE(void)
{
	siiWriteByteAFEU0(RX_AFE_DDR_CONF_ADDR, 0x01);
	siiWriteByteAFEU0(RX_AFE_DDR_DSA_ADDR, 0x00);
	bSeparateAFE = TRUE;
}

/*------------------------------------------------------------------------------
 * Function Name: siiConfigureTerminationValue
 * Function Description:
 *----------------------------------------------------------------------------
 */
void siiSetNormalTerminationValueCh1(BOOL qOn)
{
	BYTE bRegVal;
	/* YMA set the terminaion to 3k ohm first and then after HPD,
	 *change it back to normal
	 */
	bRegVal = siiReadByteHDMIRXP0(RX_TMDS_TERMCTRL_ADDR);
	if (qOn) {
		/* YMA clear bit 0,1 for ch0 */
		siiWriteByteHDMIRXP0(RX_TMDS_TERMCTRL_ADDR, bRegVal & 0xFC);
	} else {
		/* YMA set bit 0,1 for ch0 */
		siiWriteByteHDMIRXP0(RX_TMDS_TERMCTRL_ADDR, bRegVal | 0x03);
	}
}
/*------------------------------------------------------------------------------
 * Function Name: siiSetNormalTerminationValueCh2
 * Function Description:
 *----------------------------------------------------------------------------
 */
void siiSetNormalTerminationValueCh2(BOOL qOn)
{
	BYTE bRegVal;
	/* YMA set the terminaion to 3k ohm first and then after HPD,
	 *change it back to normal
	 */
	bRegVal = siiReadByteHDMIRXP0(RX_TMDS_TERMCTRL_ADDR);
	if (qOn) {
		/* YMA clear bit 5,6 for ch2 */
		siiWriteByteHDMIRXP0(RX_TMDS_TERMCTRL_ADDR, bRegVal&0x9F);
	} else {
		/* YMA set bit 5,6 for ch2 */
		siiWriteByteHDMIRXP0(RX_TMDS_TERMCTRL_ADDR, bRegVal|0x60);
	}
}
/*------------------------------------------------------------------------------
 * Function Name: siiInitializeRX
 * Function Description: Initialize RX
 *
 * Accepts: pointer on data which are used for inialization
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
BOOL siiInitializeRX(BYTE *pbInit)
{
	BOOL qResult = FALSE;
	BOOL qFPGA  = FALSE;
	BOOL qUrsula = FALSE;
	BYTE bRegVal;
	WORD wTimeOut = 500;

	siiRXHardwareReset();
	/* YMA the block below takes too long time for !qUrsula devices */
	/* move to after devID is detected. */
	/* if (siiWaitForAckAFE(wTimeOut)) {
	 *	siiInitAFE();
	 *	wTimeOut = 15000;
	 *	qUrsula = TRUE;
	 *}
	 */
	if (pbInit[0] & SiI_RX_FPGA) {
		qFPGA = TRUE;
		wTimeOut = 15000;
	}

	if (siiWaitForAckHDMIRX(wTimeOut)) {
		InitSoftwareResetReg();
		SiI_Ctrl.bDevId = GetRXDevId();

		if ((SiI_Ctrl.bDevId != RX_SiI9135) &&
			(SiI_Ctrl.bDevId != RX_SiI9125)) {
			if (siiWaitForAckAFE(wTimeOut)) {
				siiInitAFE();
				wTimeOut = 15000;
				qUrsula = TRUE;
			}
		}
		if (qFPGA && (!qUrsula)) {
			siiWriteByteHDMIRXP1(RX_ACR_CTRL3_ADDR,
				RX_CTS_THRESHOLD | RX_BIT_MCLK_LOOPBACK | 0x01);
		} else {
			siiWriteByteHDMIRXP1(RX_ACR_CTRL3_ADDR,
				RX_CTS_THRESHOLD | RX_BIT_MCLK_LOOPBACK);
		}

		siiWriteByteHDMIRXP1(RX_LK_WIN_SVAL_ADDR, 0x0F);
		siiWriteByteHDMIRXP1(RX_LK_THRESHOLD_ADDR, 0x20);
		siiWriteByteHDMIRXP1(RX_LK_THRESHOLD_ADDR + 1, 0x00);
		/* siiWrite threshold for PLL unlock interrupt */
		siiWriteByteHDMIRXP1(RX_LK_THRESHOLD_ADDR + 2, 0x00);

		if (pbInit[0] & SiI_RX_InvertOutputVidClock) {
			/* set Video bus width and Video data edge */
			siiWriteByteHDMIRXP0(RX_SYS_CTRL1_ADDR,
				RX_BIT_VID_BUS_24BIT |
				RX_BIT_INVERT_OUTPUT_VID_CLK);
		} else {
			siiWriteByteHDMIRXP0(RX_SYS_CTRL1_ADDR,
				RX_BIT_VID_BUS_24BIT);
		}
		/* YMA removed since actually no bit for it, changed to #define
		 *  if (pbInit[0] & SiI_RX_DSD_Uses_I2S_SPDIF_Buses)
		 */
#ifdef SiI_RX_DSD_Uses_I2S_SPDIF_Buses
		siiIIC_RX_RWBitsInByteP1(RX_AUDIO_SWAP_ADDR,
			BIT_DSD_USES_I2S_SPDIF_BUSES, SET);
#endif
		/* SCL edge, if need customize SPDIF parameters */
		siiWriteByteHDMIRXP1(RX_I2S_CTRL1_ADDR, 0x40);
		siiWriteByteHDMIRXP1(RX_HDMI_CRIT1_ADDR, 0x06);
		siiWriteByteHDMIRXP1(RX_HDMI_CRIT2_ADDR, 0x0C);

		/* Set Interrupt polarity (Neg.) */
		siiWriteByteHDMIRXP0(RX_INT_CNTRL_ADDR, 0x06);
		/* AudioVideo Mute ON */
		siiWriteByteHDMIRXP1(RX_AUDP_MUTE_ADDR, 0x03);

		/* Audio PLL setting */
		siiWriteByteHDMIRXP0(RX_APLL_POLE_ADDR, 0x88);
		/* Audio PLL setting, improved PLL lock time */
		siiWriteByteHDMIRXP0(RX_APLL_CLIP_ADDR, 0x16);

		/* YMA CP9135/25 reference board has 5ohm external resistor,
		 *	so the TMDS termination control should set to
		 *	45 ohm  50 ohm instead of the default value 50 ohm
		 */
		/* 9125/35 has only the TQFP package,
		 * so this applies to all the devices have 9125/35 device ID
		 */

		/* YMA removed.
		 * set the terminaion to 3k ohm first and then after HPD,
		 * change it back to normal
		 */
		/* if (SiI_Ctrl.bDevId == RX_SiI9135 ||
		 *	SiI_Ctrl.bDevId ==  RX_SiI9125) {
		 *		siiWriteByteHDMIRXP0(RX_TMDS_TERMCTRL_ADDR,
		 *			0x0c); //45 ohm
		 * }
		 */

		if ((SiI_Ctrl.bDevId == RX_SiI9133) ||
			(SiI_Ctrl.bDevId == RX_SiI9135 ||
			SiI_Ctrl.bDevId ==  RX_SiI9125) ||
			(SiI_Ctrl.bDevId == RX_SiIIP11)) {
			/* set equlizer register value,
			 *optimized for different length of
			 */
			siiWriteByteHDMIRXP0(RX_TMDS_ECTRL_ADDR, 0x00);
		} else {
			/* set equlizer register value,
			 *optimized for different length of
			 */
			siiWriteByteHDMIRXP0(RX_TMDS_ECTRL_ADDR, 0xC3);
		}
		/* no need to process MPEG as data is screwed there */
		siiWriteByteHDMIRXP1(RX_MPEG_DEC_ADDR, ISRC1_Type);

		/* ramp slope for Soft Audio Mute */
		siiWriteByteHDMIRXP1(RX_MUTE_DIV_ADDR, 0x02);
		siiSetMasterClock(SelectMClock & pbInit[0]);

		InitAECRegisters();
		RX_InitializeInterrupts();

		bRegVal = siiReadByteHDMIRXP0(RX_SYS_SW_SWTCHC_ADDR);
		if ((SiI_Ctrl.bVidInChannel == SiI_RX_VInCh1) ||
			(SiI_Ctrl.bDevId == RX_SiI9011)) {
			siiWriteByteHDMIRXP0(RX_SYS_SW_SWTCHC_ADDR,
				bRegVal | 0x11);
		} else {
			siiWriteByteHDMIRXP0(RX_SYS_SW_SWTCHC_ADDR,
				bRegVal | 0x22);
		}

		if (bSeparateAFE == PRESENT)
			siiWriteByteAFEU0(RX_SYS_SW_SWTCHC_ADDR, 0x01);

		siiWriteByteHDMIRXP0(RX_ECC_BCH_THRESHOLD, 0x02);

		siiClearBCHCounter();
		qResult = TRUE;
		if (siiCheckSupportDeepColorMode()) {
			if (SiI_RX_SetOutputColorDepth(pbInit[1]))
				qResult = FALSE;
		}

	} else {
		siiErrorHandler(SiI_EC_NoAckIIC);
	}
	return qResult;
}

