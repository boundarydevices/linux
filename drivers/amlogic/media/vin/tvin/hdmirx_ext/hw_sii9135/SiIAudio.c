/*------------------------------------------------------------------------------
 * Module Name: SiIAudio
 * ---
 * Module Description: this module sirves Audio functions
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
#include "SiIAudio.h"
#include "SiITTVideo.h"
#include "SiIRXIO.h"
#include "SiIHAL.h"
#include "SiIRXDefs.h"
#include "SiIHDMIRX.h"
#include "UCfgRX.h"
#include "UGlob.h"
#include "UAudDAC.h"

#ifdef SII_DUMP_UART
#include "../hdmirx_ext_drv.h"
#endif

struct AudioOutputFormatType_s SavedAudioOutputFormat;
/*------------------------------------------------------------------------------
 * Function Name: siiSetAudioOutputFormat
 * Function Description:  This function used to set output audio format
 *
 * Accepts: pointer on struct AudioOutputFormatType_s
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiSetAudioOutputFormat(struct AudioOutputFormatType_s *AudioOutputFormat)
{
	BYTE bRegVal;

	bRegVal = siiReadByteHDMIRXP1(RX_AUDIO_CTRL_ADDR);
	/* Set or clear SPDIF Enable bit */
	if (AudioOutputFormat->wOutputSelect & SiI_RX_AOut_SPDIF)
		bRegVal |= RX_BIT_SPDIF_EN;
	else
		bRegVal &= (~RX_BIT_SPDIF_EN);
	/* Set or clear Smooth Audio muting */
	if (AudioOutputFormat->wOutputSelect & SiI_RX_AOut_SmoothHWMute)
		bRegVal |= RX_SMOOTH_MUTE_EN;
	else
		bRegVal &= (~RX_SMOOTH_MUTE_EN);
	siiWriteByteHDMIRXP1(RX_AUDIO_CTRL_ADDR, bRegVal);

	/* Select SD0-3 */
	bRegVal = siiReadByteHDMIRXP1(RX_I2S_CTRL2_ADDR);
	bRegVal &= 0x0F;
	bRegVal |= ((AudioOutputFormat->wOutputSelect >> 4) & 0xF0);

	siiWriteByteHDMIRXP1(RX_I2S_CTRL2_ADDR, bRegVal);

	/* set I2S bus format */
	bRegVal = AudioOutputFormat->wI2SBusFormat & 0xFF;
	siiWriteByteHDMIRXP1(RX_I2S_CTRL1_ADDR, bRegVal);
	bRegVal = siiReadByteHDMIRXP1(RX_I2S_CTRL2_ADDR);
	bRegVal &= 0xFC;
	bRegVal |= ((AudioOutputFormat->wI2SBusFormat >> 8) & 0x03);
	siiWriteByteHDMIRXP1(RX_I2S_CTRL2_ADDR, bRegVal);
}
/*------------------------------------------------------------------------------
 * Function Name: siiGetAudioOutputFormat
 * Function Description: this function used to get Output Audio Format
 *
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiGetAudioOutputFormat(struct AudioOutputFormatType_s *AudioOutputFormat)
{
	/* BYTE bRegVal; */

	/* YMA change to return the save value instead of read from registers.
	 *the reg value may be not the right mode
	 */

	*AudioOutputFormat = SavedAudioOutputFormat;

#if 0
	AudioOutputFormat->wOutputSelect = SiI_Ctrl.wAudioOutputSelect;

	AudioOutputFormat->wI2SBusFormat = 0;
	AudioOutputFormat->bI2SMap = 0;

	AudioOutputFormat->bI2SMap = siiReadByteHDMIRXP1(RX_I2S_MAP_ADDR);

	bRegVal = siiReadByteHDMIRXP1(RX_I2S_CTRL1_ADDR);
	AudioOutputFormat->wI2SBusFormat = (WORD) bRegVal;


	bRegVal = siiReadByteHDMIRXP1(RX_I2S_CTRL2_ADDR);
	AudioOutputFormat->wOutputSelect |= ((bRegVal & 0xF0) << 4);
	AudioOutputFormat->wI2SBusFormat |= (WORD)((bRegVal & 0x03) << 8);
#endif
	/*
	 * yma for API rev2
	 *actually not need read from the registers because it may be the
	 *	settings for I2S, not DSD/HBR
	 *should return the settings by API set function or from EEPROM
	 *otherwise, the DSD/HBR configurations will be overwritten by
	 *	the I2S settings.
	 */
}

/*------------------------------------------------------------------------------
 * Function Name: siiSetDSDHBRAudioOutputFormat
 * Function Description:  This function used to set DSD/HBR output audio format
 *
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
static void siiSetDSDHBRAudioOutputFormat(BOOL qON)
{
	/* struct AudioOutputFormatType_s AudioOutputFormat; */

	if (qON) {
		if (SiI_Inf.AudioStatus.bRepresentation ==
			SiI_RX_AudioRepr_DSD) {
			/* YMA 2 set DSD output format as set from API */
			if (SavedAudioOutputFormat.bDSDHBRFormat &
				SiI_RX_AOut_DSD_WS16Bit) {
				siiIIC_RX_RWBitsInByteP1(RX_I2S_CTRL1_ADDR,
					RX_BIT_Aout_WordSize, SET);
			}
			if (SavedAudioOutputFormat.bDSDHBRFormat &
				SiI_RX_AOut_DSD_SENeg) {
				siiIIC_RX_RWBitsInByteP1(RX_I2S_CTRL1_ADDR,
					RX_BIT_Aout_ClockEdge, SET);
			}
		} else if (SiI_Inf.AudioStatus.bRepresentation ==
			SiI_RX_AudioRepr_HBR) {
			if (SavedAudioOutputFormat.bDSDHBRFormat &
				SiI_RX_AOut_HBRA_WS16Bit) {
				siiIIC_RX_RWBitsInByteP1(RX_I2S_CTRL1_ADDR,
					RX_BIT_Aout_WordSize, SET);
			}
			if (SavedAudioOutputFormat.bDSDHBRFormat &
				SiI_RX_AOut_HBRA_SENeg) {
				siiIIC_RX_RWBitsInByteP1(RX_I2S_CTRL1_ADDR,
					RX_BIT_Aout_ClockEdge, SET);
			}
		}
	} else { /* YMA restore SiI_RX_AudioRepr_PCM; */
		/* not get the parameter from */
		/* siiGetAudioOutputFormat(&AudioOutputFormat); */
		siiSetAudioOutputFormat(&SavedAudioOutputFormat);
	}
}

/*------------------------------------------------------------------------------
 * Function Name: ChangeDSDAudioStreamHandler
 * Function Description:
 *
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiChangeDSDAudioStreamHandler(void)
{
	/* BYTE bNewAudioRepresentation; */

	if (siiReadByteHDMIRXP1(RX_AUDP_STAT_ADDR) & BIT_DSD_STATUS) {
		SiI_Inf.AudioStatus.bRepresentation = SiI_RX_AudioRepr_DSD;
		/* YMA DSD MCLK is bit 3,2 of the parameter */
		siiSetMasterClock((SiI_Ctrl.bRXInitPrm0 >> 2) & SelectMClock);
		siiSetDSDHBRAudioOutputFormat(ON);
	} else {
		SiI_Inf.AudioStatus.bRepresentation = SiI_RX_AudioRepr_PCM;
		siiSetMasterClock(SiI_Ctrl.bRXInitPrm0 & SelectMClock);
		siiSetDSDHBRAudioOutputFormat(OFF);
	}
}

/*------------------------------------------------------------------------------
 * Function Name: ChangeHBRAudioStreamHandler
 * Function Description:
 *
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiChangeHBRAudioStreamHandler(void)
{
	if (siiReadByteHDMIRXP1(RX_AUDP_STAT_ADDR) & RX_BIT_HBRA_STATUS) {
		siiSetHBRFs(ON);
		SiI_Inf.AudioStatus.bRepresentation = SiI_RX_AudioRepr_HBR;
		/* YMA HBR  MCLK is bit 5,4 of the parameter */
		siiSetMasterClock((SiI_Ctrl.bRXInitPrm0 >> 4) & SelectMClock);
		siiSetDSDHBRAudioOutputFormat(ON);
	} else {
		siiSetHBRFs(OFF);
		SiI_Inf.AudioStatus.bRepresentation = SiI_RX_AudioRepr_PCM;
		siiSetMasterClock(SiI_Ctrl.bRXInitPrm0 & SelectMClock);
		siiSetDSDHBRAudioOutputFormat(OFF);
	}
}

/*-----------------------------------------------------------------------------
 * This table represents operational CTS ranges
 * To get Real CTS Value, * 10, it was done to reduce data size
 *---------------------------------------------------------------------------
 */
ROM const struct CTSLimitsType_s CTSLimits[4] = {
	{1666, 16666},   /* FPix TMDS range 25-50mHz */
	{3333, 28666},   /* FPix TMDS range 50-86mHz */
	{5733, 41666},   /* FPix TMDS range 86-125mHz */
	{8333, 55000},   /* FPix TMDS range 125-165mHz */
};
/*------------------------------------------------------------------------------
 * Function Name: siiClearCTSChangeInterruprt
 * Function Description:   This functio clears CTS change interrupts in order
 *                        to use later information about stability of CTS values
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiClearCTSChangeInterruprt(void)
{
	siiWriteByteHDMIRXP0(RX_HDMI_INT_ST1_ADDR, RX_BIT_CTS_CHANGED);
}

/*------------------------------------------------------------------------------
 * Function Name: CheckCTSChanged
 * Function Description:  it used to detect if CTS value has been changed
 *----------------------------------------------------------------------------
 */
static BOOL CheckCTSChanged(void)
{
	BOOL qResult = FALSE;

	if (siiReadByteHDMIRXP0(RX_HDMI_INT_ST1_ADDR) & RX_BIT_CTS_CHANGED)
		qResult = TRUE;
	return qResult;
}
/*------------------------------------------------------------------------------
 * Function Name: AudioFIFO_Reset
 * Function Description: Makes Audio FIFO reset
 *----------------------------------------------------------------------------
 */
void siiAudioFIFO_Reset(void)
{
	siiIIC_RX_RWBitsInByteP0(RX_SWRST_ADDR, RX_BIT_AUDIO_FIFO_RESET, SET);
	siiIIC_RX_RWBitsInByteP0(RX_SWRST_ADDR, RX_BIT_AUDIO_FIFO_RESET, CLR);
}
/*------------------------------------------------------------------------------
 * Function Name: ACR_Reset
 * Function Description: Makes Audio clock regeneration (ACR) reset
 *----------------------------------------------------------------------------
 */
static void ACR_Reset(void)
{
	siiIIC_RX_RWBitsInByteP0(RX_SWRST_ADDR, RX_BIT_ACR_RESET, SET);
	siiIIC_RX_RWBitsInByteP0(RX_SWRST_ADDR, RX_BIT_ACR_RESET, CLR);
}
/*------------------------------------------------------------------------------
 * Function Name: siiAudioMute
 * Function Description:  mutes audio
 *
 * Accepts: qOn
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiAudioMute(BOOL qOn)
{
	if (qOn) {
		siiIIC_RX_RWBitsInByteP1(RX_AUDP_MUTE_ADDR,
			RX_BIT_AUDIO_MUTE, SET);
		SiI_Inf.bGlobStatus |= SiI_RX_GlobalHDMI_AMute;
	} else {
		siiIIC_RX_RWBitsInByteP1(RX_AUDP_MUTE_ADDR,
			RX_BIT_AUDIO_MUTE, CLR);
		SiI_Inf.bGlobStatus &= (~SiI_RX_GlobalHDMI_AMute);
	}
}

#ifdef SII_ANALOG_DIG_AUDIO_MAX
/*------------------------------------------------------------------------------
 * Function Name: siiSetAnalogAudioMux
 * Function Description:  Used in DVI mode for Analog Audio input selection
 *                        control of MUX through MCU general purpose output pins
 * Accepts: bChannel
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */

void siiSetAnalogAudioMux(BYTE bChannel)
{
	BYTE bPCB_Id;

	bPCB_Id = siiGetPCB_Id();

	if (bChannel == SiI_RX_VInCh1) {   /* select MuxAnalog Channel A */
		/*
		 *YMA 10/4/06 remove wilma special
		 *   if (( bPCB_Id == SiI_CP9133 )||( bPCB_Id == SiI_CP9135 )) {
		 */

		if (bPCB_Id == SiI_CP9133) {
			halAudioSetAltA();
			halAudioClearAltB();
		} else {
			halAudioSetA();
			halAudioClearB();
		}
	} else if (bChannel == SiI_RX_VInCh2) { /* select MuxAnalog Channel B */
		/* YMA 10/4/06 remove wilma special
		 *   if (( bPCB_Id == SiI_CP9133 )||( bPCB_Id == SiI_CP9135 )) {
		 */

		if (bPCB_Id == SiI_CP9133) {
			halAudioClearAltA();
			halAudioSetAltB();
		} else {
			halAudioClearA();
			halAudioSetB();
		}
	}
}
/*------------------------------------------------------------------------------
 * Function Name: siiSetDigitalAudioMux(
 * Function Description:  Used in HDMI mode for Didital Audio input selection
 *
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */

void siiSetDigitalAudioMux(void)
{
	BYTE bPCB_Id;

	bPCB_Id = siiGetPCB_Id();
	/*
	 *YMA 10/4/06 remove wilma special
	 *   if (( bPCB_Id == SiI_CP9133 )||( bPCB_Id == SiI_CP9135 )) {
	 */

	if (bPCB_Id == SiI_CP9133) {
		halAudioClearAltA();
		halAudioClearAltB();
	} else {
		halAudioClearA();
		halAudioClearB();
	}
}
#endif /* end SII_ANALOG_DIG_AUDIO_MAX */
/*------------------------------------------------------------------------------
 * Function Name: AudioExceptionsControl
 * Function Description:
 *	for switching between Automatic and Manual Audio control
 *----------------------------------------------------------------------------
 */
static void AudioExceptionsControl(BOOL qOn)
{
	if (qOn) {
		siiIIC_RX_RWBitsInByteP0(RX_AEC_CTRL_ADDR, RX_BIT_AEC_EN, SET);
		siiIIC_RX_RWBitsInByteP0(RX_INT_MASK_ST5_ADDR,
			RX_BIT_AAC_DONE, SET);
		SiI_Ctrl.bIgnoreIntr &= (~qcIgnoreAAC);
	} else {
		siiIIC_RX_RWBitsInByteP0(RX_AEC_CTRL_ADDR, RX_BIT_AEC_EN, CLR);
		siiIIC_RX_RWBitsInByteP0(RX_INT_MASK_ST5_ADDR,
			RX_BIT_AAC_DONE, CLR);
		SiI_Ctrl.bIgnoreIntr |= qcIgnoreAAC;
	}
}
/*------------------------------------------------------------------------------
 * Function Name: ACRInit
 * Function Description:
 *	This function makes initialisation of Audio Clock Regeneration (ACR)
 *----------------------------------------------------------------------------
 */
static void ACRInit(void)
{
	siiIIC_RX_RWBitsInByteP1(RX_ACR_CTRL1_ADDR, RX_BIT_ACR_INIT, SET);
}
/*------------------------------------------------------------------------------
 * Function Name: ClearFIFOOverUnderRunInterrupts
 * Function Description: clears the interrupts, which indicate FIFO operation
 *----------------------------------------------------------------------------
 */
static void ClearFIFOOverUnderRunInterrupts(void)
{
	siiWriteByteHDMIRXP0(RX_HDMI_INT_ST4_ADDR,
		RX_BIT_FIFO_UNDERRUN | RX_BIT_FIFO_OVERRUN);
}
/*------------------------------------------------------------------------------
 * Function Name: CheckFIFOOverUnderRunInterrupts
 * Function Description:
 *	used to detect the interrupt, getting of these interrupt,
 *                       tells that FIFO has problems
 *----------------------------------------------------------------------------
 */
static BOOL CheckFIFOOverUnderRunInterrupts(void)
{
	BOOL qResult = FALSE;

	if (siiReadByteHDMIRXP0(RX_HDMI_INT_ST4_ADDR) &
		(RX_BIT_FIFO_UNDERRUN | RX_BIT_FIFO_OVERRUN)) {
		qResult = TRUE;
	}
	return  qResult;
}

/*------------------------------------------------------------------------------
 * Function Name: CheckGotAudioPacketInterrupt
 * Function Description: indicates presence of Audio Packets
 *----------------------------------------------------------------------------
 */
static BOOL CheckGotAudioPacketInterrupt(void)
{
	BOOL qResult = FALSE;

	if (siiReadByteHDMIRXP0(RX_HDMI_INT_ST2_ADDR) & RX_BIT_GOT_AUDIO_PKT)
		qResult = TRUE;
	return  qResult;
}

/*------------------------------------------------------------------------------
 * Function Name: CheckGotCTSPacketInterrupt
 * Function Description:
	indicates presence of CTS Packets, these packets used for
 *                        Audio Clock Regeneration
 *----------------------------------------------------------------------------
 */
static BOOL CheckGotCTSPacketInterrupt(void)
{
	BOOL qResult = FALSE;

	if (siiReadByteHDMIRXP0(RX_HDMI_INT_ST2_ADDR) & RX_BIT_GOT_CTS_PKT)
		qResult = TRUE;
	return qResult;
}
/*------------------------------------------------------------------------------
 * Function Name: siiClearGotCTSAudioPacketsIterrupts
 * Function Description:  clears CTS and Audio packets interrupts
 *                        system checks later if packets are received
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiClearGotCTSAudioPacketsIterrupts(void)
{
	siiWriteByteHDMIRXP0(RX_HDMI_INT_ST2_ADDR,
		(RX_BIT_GOT_AUDIO_PKT | RX_BIT_GOT_CTS_PKT));
}

/*------------------------------------------------------------------------------
 * Function Name: GotCTSPackets
 * Function Description: received CTS packets, used for Audio Clock Regeneration
 *----------------------------------------------------------------------------
 */
static BOOL GotCTSPackets(void)
{
	return CheckGotCTSPacketInterrupt();
}
/*------------------------------------------------------------------------------
 * Function Name: GotAudioPackets
 * Function Description: received Audio packets
 *----------------------------------------------------------------------------
 */
static BOOL GotAudioPackets(void)
{
	return CheckGotAudioPacketInterrupt();
}
/*------------------------------------------------------------------------------
 * Function Name: GetCTS
 * Function Description: This function gets Cycle Timing Stamp (CTS)
 *                       the result is divided by 10 to simlify math
 *----------------------------------------------------------------------------
 */
static WORD GetCTS(void)
{
	WORD CTS_L;
	DWORD CTS_H;

	CTS_L = siiReadWordHDMIRXP1(RX_HW_CTS_ADDR);
	CTS_H = siiReadWordHDMIRXP1(RX_HW_CTS_ADDR + 2);
	CTS_H <<= 16;
	return (WORD)((CTS_H | CTS_L) / 10);
}
/*------------------------------------------------------------------------------
 * Function Name: IsCTSInRange
 * Function Description: Check if CTS stamp in the operation range
 *----------------------------------------------------------------------------
 */
static BOOL IsCTSInRange(void)
{
	BYTE bPixClk;
	WORD wCTS;
	BOOL qResult = FALSE;

	wCTS = GetCTS();
	bPixClk = siiGetPixClock();
	if (bPixClk) {
		if (bPixClk < 50) {
			if ((CTSLimits[0].Min < wCTS) &&
				(CTSLimits[0].Max > wCTS)) {
				qResult = TRUE;
			}
		} else if (bPixClk  < 86) {
			if ((CTSLimits[1].Min < wCTS) &&
				(CTSLimits[1].Max > wCTS)) {
				qResult = TRUE;
			}
		} else if (bPixClk < 125) {
			if ((CTSLimits[2].Min < wCTS) &&
				(CTSLimits[2].Max > wCTS)) {
				qResult = TRUE;
			}
		} else if (bPixClk < 165) {
			if ((CTSLimits[3].Min < wCTS) &&
				(CTSLimits[3].Max > wCTS)) {
				qResult = TRUE;
			}
		}
	} else {
		/* This check goes when video resolution is not detected yet,
		 * so less strict conditions for test.
		 */
		if ((CTSLimits[0].Min < wCTS) && (CTSLimits[3].Max > wCTS))
			qResult = TRUE;
	}
	return qResult;
}
/*------------------------------------------------------------------------------
 * Function Name: ClearCTS_DroppedReusedInterrupts
 * Function Description:
 *	clears CTS dropped or reused interrupts
 *      these interrupts used to check stability of receiving CTS packets
 *----------------------------------------------------------------------------
 */
static void ClearCTS_DroppedReusedInterrupts(void)
{
	siiWriteByteHDMIRXP0(RX_HDMI_INT_ST4_ADDR,
		RX_BIT_CTS_DROPPED | RX_BIT_CTS_REUSED);
}
/*------------------------------------------------------------------------------
 * Function Name: GotCTS_DroppedReusedInterrupts
 * Function Description: check presence of CTS dropped or reused interrupts
 *----------------------------------------------------------------------------
 */
static BOOL GotCTS_DroppedReusedInterrupts(void)
{
	BOOL qResult = FALSE;

	if (siiReadByteHDMIRXP0(RX_HDMI_INT_ST4_ADDR) &
		(RX_BIT_CTS_DROPPED | RX_BIT_CTS_REUSED)) {
		qResult = TRUE;
	}
	return qResult;
}
/*------------------------------------------------------------------------------
 * Function Name: IsAudioStable()
 * Function Description:
 *	About stable audio clock we judge how stable CTS packets are coming
 *                        If CTS dropped, then CTS packets coming too often
 *                        If CTS reused, then CTS packets coming too seldom
 *----------------------------------------------------------------------------
 */
static BOOL GotAudioStable(void)
{
	BOOL qResult = FALSE;

	if (!GotCTS_DroppedReusedInterrupts())
		qResult = TRUE;
	return qResult;
}
/*------------------------------------------------------------------------------
 * Function Name: GotPLL_UnlockInterrupt
 * Function Description: this function is used if PLL is unlock
 *----------------------------------------------------------------------------
 */
static BOOL GotPLL_UnlockInterrupt(void)
{
	BOOL qResult = FALSE;

	if (siiReadByteHDMIRXP0(RX_HDMI_INT_ST1_ADDR) & RX_BIT_ACR_PLL_UNLOCK)
		qResult = TRUE;
	return qResult;
}

/*------------------------------------------------------------------------------
 * Function Name:  ClearPLL_UnlockInterrupt
 * Function Description: clears PLL unlock interrupt
 *----------------------------------------------------------------------------
 */
static void ClearPLL_UnlockInterrupt(void)
{
	siiWriteByteHDMIRXP0(RX_HDMI_INT_ST1_ADDR, RX_BIT_ACR_PLL_UNLOCK);
}
/*------------------------------------------------------------------------------
 * Function Name: GotPLL_Unlocked
 * Function Description:
 *	Check if Audio PLL is still unlock. Function clears PLL unlock
 *                       interrupt and checks for new interrupt
 *----------------------------------------------------------------------------
 */
static BOOL GotPLL_Unlocked(void)
{
	BOOL qResult = FALSE;

	ClearPLL_UnlockInterrupt();
	if (GotPLL_UnlockInterrupt())
		qResult = TRUE;
	return qResult;
}

#ifdef SII_DUMP_UART
/*------------------------------------------------------------------------------
 * Function Name:  PrintN
 * Function Description:
 *
 *----------------------------------------------------------------------------
 */
static void PrintN(void)
{
	WORD wN_L;
	DWORD dwN_H;

	wN_L = siiReadWordHDMIRXP1(RX_HW_N_ADDR);
	dwN_H = siiReadWordHDMIRXP1(RX_HW_N_ADDR + 2);
	dwN_H <<= 16;
	dwN_H |= wN_L;
	printf(" N = %d ", (int)dwN_H);

}
/*------------------------------------------------------------------------------
 * Function Name:  PrintCTS
 * Function Description:
 *
 *----------------------------------------------------------------------------
 */
static void PrintCTS(void)
{
	WORD CTS_L;
	DWORD CTS_H;

	CTS_L = siiReadWordHDMIRXP1(RX_HW_CTS_ADDR);
	CTS_H = siiReadWordHDMIRXP1(RX_HW_CTS_ADDR + 2);

/*    CTS_H <<= 16;
 *    CTS_H |= CTS_L;
 *    printf(" CTS = % ", (int) CTS_H );  //fixed in build 010118.
 */
	printf(" CTS = 0x%x%x\n", (int)CTS_H, (int)CTS_L);
}
/*------------------------------------------------------------------------------
 * Function Name: PrintFIFO_Diff()
 * Function Description:
 *
 *----------------------------------------------------------------------------
 */
static void PrintFIFO_Diff(void)
{
	BYTE bRegVal;

	bRegVal = GetFIFO_DiffPointer();
	printf(" FIFO diff = 0x%X ", (int)bRegVal);
}

#endif
/*------------------------------------------------------------------------------
 * Function Name: ResetFIFO_AndCheckItsOperation
 * Function Description: If no FIFO under run or over run interrupts after
 *                       Audio FIFO restet, then FIFO is working fine
 *----------------------------------------------------------------------------
 */
BYTE ResetFIFO_AndCheckItsOperation(void)
{
	BYTE bError = FALSE;
	BOOL qFIFO_OverUnderRun = TRUE;
	BYTE bTry = 3;
	BYTE bFIFO_DiffPointer;

	if ((SiI_Ctrl.bDevId ==  RX_SiI9021) ||
		(SiI_Ctrl.bDevId ==  RX_SiI9031)) {
		/* !!!!!!!!! wait when FIFO UnderRun and OverRun are gone */
		do {
			siiAudioFIFO_Reset();
			ClearFIFOOverUnderRunInterrupts();
			halDelayMS(1);
			qFIFO_OverUnderRun = CheckFIFOOverUnderRunInterrupts();
			if (qFIFO_OverUnderRun)
				break;
		} while (--bTry);
	} else {
		siiAudioFIFO_Reset();
		ClearFIFOOverUnderRunInterrupts();

		halDelayMS(1);

		qFIFO_OverUnderRun = CheckFIFOOverUnderRunInterrupts();
	}
	if (qFIFO_OverUnderRun) {
		bFIFO_DiffPointer = GetFIFO_DiffPointer();
		/*
		 *YMA change for Rx FIFO size 32
		 *if ((bFIFO_DiffPointer < 2) || (bFIFO_DiffPointer > 16))
		 */
		if ((bFIFO_DiffPointer < 2) || (bFIFO_DiffPointer > 0x19))
			bError = SiI_EC_FIFO_ResetFailure;
		else
			bError = SiI_EC_FIFO_UnderRunStuck;
	}
#ifdef SII_DUMP_UART
	PrintFIFO_Diff();
	PrintN();
	PrintCTS();
#endif
	return bError;
}
/*------------------------------------------------------------------------------
 * Function Name: SetAudioClock
 * Function Description: Makes Master Clock Enable
 *----------------------------------------------------------------------------
 */
static void SetAudioClock(void)
{
	siiIIC_RX_RWBitsInByteP1(RX_I2S_CTRL2_ADDR, RX_BIT_MCLK_EN, SET);
}
/*------------------------------------------------------------------------------
 * Function Name: ClearAudioClock
 * Function Description: Makes Master Clock Disable
 *----------------------------------------------------------------------------
 */
static void ClearAudioClock(void)
{
	siiIIC_RX_RWBitsInByteP1(RX_I2S_CTRL2_ADDR, RX_BIT_MCLK_EN, CLR);
}
/*------------------------------------------------------------------------------
 * Function Name: MaskUnusedAudioOutputs
 * Function Description: disable unused channels in order to prevent audio noise
 *----------------------------------------------------------------------------
 */
static void MaskUnusedAudioOutputs(BYTE *pbRegister)
{
	BYTE bMask;

	if ((siiReadByteHDMIRXP1(RX_AUDP_STAT_ADDR) & RX_BIT_LAYOUT_1)
	/* YMA 2 HBR mode may get layout 0, but needs to enable all SDx */
		|| (SiI_Inf.AudioStatus.bRepresentation ==
			SiI_RX_AudioRepr_HBR)) {
		bMask = 0xFF;
	} else {
		bMask = 0x1F;
	}
	*pbRegister &=  bMask;
}

/*------------------------------------------------------------------------------
 * Function Name: PrepareAACOn
 * Function Description: prepare Automatic Audio Control On
 *----------------------------------------------------------------------------
 */
static void PrepareAACOn(void)
{
	BYTE bRegVal;

	bRegVal = siiReadByteHDMIRXP1(RX_I2S_CTRL2_ADDR);
	bRegVal |= (BYTE)((SiI_Ctrl.wAudioOutputSelect >> 4) & 0xF3);
	MaskUnusedAudioOutputs(&bRegVal);
	/* Enable SDX (Reg (p1)27) */
	siiWriteByteHDMIRXP1(RX_I2S_CTRL2_ADDR, bRegVal);

	/* Enable SCK, WS, SPDIF, ignor VUCP errors (p1)2 */
	bRegVal = siiReadByteHDMIRXP1(RX_AUDIO_CTRL_ADDR);
	if (SiI_Ctrl.wAudioOutputSelect & SiI_RX_AOut_SPDIF)
		bRegVal |= RX_BIT_SPDIF_EN;
	else
		bRegVal &= (~RX_BIT_SPDIF_EN);
	if (SiI_Ctrl.wAudioOutputSelect & SiI_RX_AOut_I2S)
		bRegVal |= RX_BIT_I2S_MODE;
	else
		bRegVal &= (~RX_BIT_I2S_MODE);
	if (SiI_Ctrl.wAudioOutputSelect & SiI_RX_AOut_SmoothHWMute)
		bRegVal |= RX_SMOOTH_MUTE_EN;
	else
		bRegVal &= (~RX_SMOOTH_MUTE_EN);
	bRegVal |= RX_BIT_PASS_SPDIF_ERR;
	siiWriteByteHDMIRXP1(RX_AUDIO_CTRL_ADDR, bRegVal);
}
/*------------------------------------------------------------------------------
 * Function Name: IsAudioReady
 * Function Description: check for Audio Error conditions
 *----------------------------------------------------------------------------
 */
static BOOL IsAudioReady(void)
{
	BOOL qResult = FALSE;

	ClearCTS_DroppedReusedInterrupts();

	if (!GotAudioPackets())
		SiI_Inf.bAudioErr = SiI_EC_NoAudioPackets;
	else if (!GotCTSPackets())
		SiI_Inf.bAudioErr = SiI_EC_NoCTS_Packets;
	else if (!IsCTSInRange())
		SiI_Inf.bAudioErr = SiI_EC_CTS_OutOfRange;
	else if (!GotAudioStable())
		SiI_Inf.bAudioErr = SiI_WC_CTS_Irregular;
	else {
		SiI_Inf.bAudioErr = SiI_EC_NoAudioErrors;
		qResult = TRUE;
	}
	return qResult;
}
/*------------------------------------------------------------------------------
 * Function Name:  siiCheckAudio_IfOK_InitACR
 * Function Description: Checks Audio Ready and makes ACR Reaset
 *                       and Initialization
 *
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
BOOL siiCheckAudio_IfOK_InitACR(void)
{
	BOOL qResult = FALSE;

	siiClearBCHCounter();

	if (IsAudioReady())
		qResult = TRUE;
	ACR_Reset();
	ACRInit();

	return qResult;
}
/*------------------------------------------------------------------------------
 * Function Name: CheckPLLUnLockAndReCheckAudio
 * Function Description: checks audio PLL state, and another audio conditions
 *----------------------------------------------------------------------------
 */
BOOL CheckPLLUnLockAndReCheckAudio(void)
{
	BOOL qResult = FALSE;

	if (GotPLL_Unlocked())
		SiI_Inf.bAudioErr = SiI_EC_PLL_Unlock;
	else if (IsAudioReady())
		qResult = TRUE;
	return qResult;
}
/*------------------------------------------------------------------------------
 * Function Name: SaveInputAudioStatus
 * Function Description:  saving input Audio stream info into
 *                        SiI_Inf.AudioStatus
 * VG this function will be modified for DSD and other new audio streams
 *----------------------------------------------------------------------------
 */
void siiSaveInputAudioStatus(void)
{
	SiI_Inf.AudioStatus.bRepresentation =
		SiI_Inf.AudioStatus.bAccuracyAndFs =
			SiI_Inf.AudioStatus.bLength = 0;
	SiI_Inf.AudioStatus.bNumberChannels = 0;
	/* get audio sample representation */
	if (siiReadByteHDMIRXP1(RX_CH_STATUS1_ADDR) &
		RX_BIT_AUDIO_SAMPLE_NPCM) {
		SiI_Inf.AudioStatus.bRepresentation =
			SiI_RX_AudioRepr_Compressed;
	} else {
		SiI_Inf.AudioStatus.bRepresentation = SiI_RX_AudioRepr_PCM;
	}
	if ((SiI_Ctrl.bDevId ==  RX_SiI9033 || SiI_Ctrl.bDevId ==  RX_SiI9133 ||
		SiI_Ctrl.bDevId == RX_SiI9135 || SiI_Ctrl.bDevId == RX_SiI9125)
		&& (siiReadByteHDMIRXP1(RX_AUDP_STAT_ADDR) &
			RX_BIT_DSD_STATUS)) {
		SiI_Inf.AudioStatus.bRepresentation = SiI_RX_AudioRepr_DSD;
	}

	/* get Fs/Length */
	if ((SiI_Inf.AudioStatus.bRepresentation == SiI_RX_AudioRepr_PCM) ||
		(SiI_Inf.AudioStatus.bRepresentation ==
			SiI_RX_AudioRepr_Compressed)) {
		SiI_Inf.AudioStatus.bAccuracyAndFs |=
			siiReadByteHDMIRXP1(RX_CH_STATUS4_ADDR);
		SiI_Inf.AudioStatus.bLength =
			(siiReadByteHDMIRXP1(RX_CH_STATUS5_ADDR) & 0x0F);
	}

	/* bNumberChannels is copied from Audio InfoFrame if it's not Zero */
	if (SiI_Inf.AudioStatus.bNumberChannels == SiI_RX_NumAudCh_Unknown) {
		if (siiReadByteHDMIRXP1(RX_AUDP_STAT_ADDR) & RX_BIT_LAYOUT_1) {
			SiI_Inf.AudioStatus.bNumberChannels =
				SiI_RX_NumAudCh_UnknownMulti;
		} else
			SiI_Inf.AudioStatus.bNumberChannels = SiI_RX_NumAudCh_2;
	}
}

/*------------------------------------------------------------------------------
 * Function Name: siiSetAutoFIFOReset
 * Function Description:  this function enables or disables Auto FIFO reset
 *----------------------------------------------------------------------------
 */
void siiSetAutoFIFOReset(BOOL qOn)
{
	if ((SiI_Ctrl.bDevId == RX_SiI9125) ||
		(SiI_Ctrl.bDevId == RX_SiI9135)) {
		if (qOn) {
			/* YMA 9135 only */
			siiIIC_RX_RWBitsInByteP0(RX_SWRST_ADDR2,
				RX_BIT_AUDIOFIFO_AUTO, SET);
		} else {
			siiIIC_RX_RWBitsInByteP0(RX_SWRST_ADDR2,
				RX_BIT_AUDIOFIFO_AUTO, CLR);
		}
	}
}

/*------------------------------------------------------------------------------
 * Function Name: siiPrepareTurningAudioOn
 * Function Description: check audio and preparing for turning on
 *
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
BOOL siiPrepareTurningAudioOn(void)
{
	BOOL qResult = FALSE;

	/*
	 *YMA removed: it is done in ISR, should not be called again here
	 *since the intr is cleared in ISR already
	 */
	/* siiChangeAudioStreamHandler(); */
	if (siiReadByteHDMIRXP1(RX_AUDP_STAT_ADDR) & BIT_DSD_STATUS)
		halSetAudioDACMode(SiI_RX_AudioRepr_DSD);
	else
		halSetAudioDACMode(SiI_RX_AudioRepr_PCM);
	siiSetDigitalAudioMux();
	if (!CheckCTSChanged()) {
		if (CheckPLLUnLockAndReCheckAudio()) {
			SetAudioClock();
			/* MJ: Moved outside if to fix
			 *I2S DSD Audio FIFO Bug 3056
			 */
			PrepareAACOn();
			SiI_Inf.bAudioErr = ResetFIFO_AndCheckItsOperation();
			if (!SiI_Inf.bAudioErr) {
				/* PrepareAACOn(); */
				AudioExceptionsControl(ON);

				 WakeUpAudioDAC();
				/* halSetAudioDACMode(
				 *	SiI_Inf.AudioStatus.bRepresentation);
				 */
				qResult = TRUE;
			} else {
				ClearAudioClock();
			}
		}
	} else
		SiI_Inf.bAudioErr = SiI_EC_CTS_Changed;
	return qResult;
}
/*------------------------------------------------------------------------------
 * Function Name: DisableAudioOutputs
 * Function Description: Disable SPDIF/I2S
 *----------------------------------------------------------------------------
 */
void DisableAudioOutputs(void)
{
	/* Disable SCK, SPDIF, Enable I2S/DSD */
	siiWriteByteHDMIRXP1(RX_AUDIO_CTRL_ADDR, 0x18);
	siiIIC_RX_RWBitsInByteP1(RX_I2S_CTRL2_ADDR, RX_BITS_SD0_SD3_EN, CLR);
}
/*------------------------------------------------------------------------------
 * Function Name: siiTurningAudio
 * Function Description: turning Audio On/Off
 *
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiTurningAudio(BOOL qOn)
{
	if (qOn) {
		PowerDownAudioDAC();
		siiAudioMute(OFF);
		WakeUpAudioDAC();
		halClearHardwareAudioMute();
	} else {
		AudioExceptionsControl(OFF);
		halSetHardwareAudioMute();
		PowerDownAudioDAC();
		siiAudioMute(ON);
		DisableAudioOutputs();
	}
}


