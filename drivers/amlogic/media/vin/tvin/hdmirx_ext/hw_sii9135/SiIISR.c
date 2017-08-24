/*------------------------------------------------------------------------------
 * Module Name:  SiIISR.c
 * ---
 * Module Description:  serves RX interrupts
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
#include "SiIISR.h"
#include "SiIHAL.h"
#include "SiIRXIO.h"
#include "SiIHDMIRX.h"
#include "SiITTAudio.h"
#include "SiITTVideo.h"
#include "SiIInfoPkts.h"
#include "SiITTInfoPkts.h"
#include "SiIHDCP.h"
#include "SiITTasks.h"
#include "SiIAudio.h"
#include "SiIDeepColor.h"

#include "../hdmirx_ext_drv.h"

/* Declaratins of functions which used only inside the module */
static BOOL CheckAVIHader(void);
static void SetNoAVIIntrrupts(BOOL);
static void ProcessChangeBetween_DVI_HDMI_Modes(void);
static void EnableCheckHDCPFailureWithVSyncRate(BOOL);
static void AVI_And_NoAVI_WorkAround(BYTE *);
static BOOL GotNConsecutiveVSyncHDCPFailure(void);
static BOOL DetectHDCPStuck(void);
static BYTE GotNConsecutiveHDCPStuckInterrupt(void);
static BYTE HDCPErrorHandler(BYTE, BYTE);
static BYTE RXInterruptHandler(void);


/*------------------------------------------------------------------------------
 * Function Name: siiTraceRXInterrupts
 * Function Description: This function reads RX interrupts and used for
 *                       debugging purpose only
 *
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
#ifdef SII_DUMP_UART
void siiTraceRXInterrupts(void)
{
	BYTE i, abIntrData[7];

	/* including main status reg */
	siiReadBlockHDMIRXP0(RX_HDMI_INT_GR1_ADDR - 1, 5, abIntrData);
	siiReadBlockHDMIRXP0(RX_HDMI_INT_GR2_ADDR, 2, &abIntrData[5]);

	printf("\n RX ints:");
	for (i = 0; i < 7; i++)
		printf(" 0x%02X", (int)abIntrData[i]);
}
#endif
/*------------------------------------------------------------------------------
 * Function Name: CheckAVIHader
 * Function Description: checks if Id header byte has data
 *----------------------------------------------------------------------------
 */
static BOOL CheckAVIHader(void)
{
	BOOL qResult = FALSE;

	if (siiReadByteHDMIRXP1(RX_AVI_IF_ADDR) != 0)
		qResult = TRUE;
	return qResult;
}
/*------------------------------------------------------------------------------
 * Function Name: SetNoAVIIntrrupts
 * Function Description: Enable or Disable NoAVI interrupt
 *----------------------------------------------------------------------------
 */
static void SetNoAVIIntrrupts(BOOL qOn)
{
	if (qOn) {
		siiIIC_RX_RWBitsInByteP0(RX_HDMI_INT_ST4_MASK_ADDR,
			RX_BIT_MASK_NO_AVI_INTR, SET);
		SiI_Ctrl.bIgnoreIntr |= qcIgnoreNoAVI;
	} else {
		siiIIC_RX_RWBitsInByteP0(RX_HDMI_INT_ST4_MASK_ADDR,
			RX_BIT_MASK_NO_AVI_INTR, CLR);
		SiI_Ctrl.bIgnoreIntr &= (~qcIgnoreNoAVI);
	}
}
/*------------------------------------------------------------------------------
 * Function Name: AVI_And_NoAVI_WorkAround
 * Function Description: this function is used to resolve conflict situation
 *                        when AVIChange an NoAVI both are set
 *----------------------------------------------------------------------------
 */
static void AVI_And_NoAVI_WorkAround(BYTE *bpInfoFramesIntrrupts)
{
	if ((*bpInfoFramesIntrrupts & NoAVI_Mask) &&
		(*bpInfoFramesIntrrupts & AVI_Mask)) {
		if (CheckAVIHader())
			*bpInfoFramesIntrrupts &= (~NoAVI_Mask);
		else
			*bpInfoFramesIntrrupts &= (~AVI_Mask);
	}
	if (*bpInfoFramesIntrrupts & NoAVI_Mask)
		SetNoAVIIntrrupts(OFF);
	if (*bpInfoFramesIntrrupts & AVI_Mask)
		SetNoAVIIntrrupts(ON);

}

/*------------------------------------------------------------------------------
 * Function Name: ProcessChangeBetween_DVI_HDMI_Modes
 * Function Description:  Processing when RX mode changed between HDMI/DVI
 *----------------------------------------------------------------------------
 */
static void ProcessChangeBetween_DVI_HDMI_Modes(void)
{

	if (siiIsHDMI_Mode()) {
		SiI_Inf.bGlobStatus |= SiI_RX_GlobalHDMI_Detected;
		siiSetSM_ReqAudio();
		siiInitACPCheckTimeOut();
	} else {
		SiI_Inf.bGlobStatus &=
			(~(SiI_RX_GlobalHDMI_Detected | SiI_RX_GlobalHDMI_ACP));
		SiI_Inf.bGlobStatus |= SiI_RX_GlobalHDMI_NoAVIPacket;
		SiI_Inf.bNewInfoPkts = 0;
		siiResetACPPacketData();
		SetNoAVIIntrrupts(OFF);
	}
}
#ifdef SII_BUG_PHOEBE_AUTOSW_BUG
/*------------------------------------------------------------------------------
 * Function Name: siiCheckTransitionToDVIModeAndResetHDMIInfoIfConfirmed
 * Function Description:
 *
 * Accepts:
 * Returns:
 * Globals:
 *----------------------------------------------------------------------------
 */

void siiCheckTransitionToDVIModeAndResetHDMIInfoIfConfirmed(void)
{
	if (!siiIsHDMI_Mode()) {
		SiI_Inf.bGlobStatus &=
			(~(SiI_RX_GlobalHDMI_Detected | SiI_RX_GlobalHDMI_ACP));
		SiI_Inf.bGlobStatus |= SiI_RX_GlobalHDMI_NoAVIPacket;
		SiI_Inf.bNewInfoPkts = 0;
		siiTurnAudioOffAndSetSM_AudioOff();
		SetNoAVIIntrrupts(OFF);
		siiClearAVI_Info(&SiI_Inf.AVI);
	}
}
#endif
/*------------------------------------------------------------------------------
 * Function Name: EnableHDCPCheckUsingVSyncInt
 * Function Description: This function enables using VSync Interrupts for
 *                        processing HDCP errors on field to field base or
 *                        disable the mode with switching wait for isolated HDCP
 *                        Failure
 *----------------------------------------------------------------------------
 */
static void EnableCheckHDCPFailureWithVSyncRate(BOOL qOn)
{
	if (qOn) {
		SiI_Ctrl.bHDCPFailFrmCnt = 1;
		/* don't clear HDCP Failure Int if this bit is set */
		SiI_Ctrl.bIgnoreIntr |= qcIgnoreHDCPFail;
		siiIIC_RX_RWBitsInByteP0(RX_HDMI_INT_ST2_MASK_ADDR,
			RX_BIT_VSYNC, SET);
		siiIIC_RX_RWBitsInByteP0(RX_HDMI_INT_ST4_MASK_ADDR,
			RX_BIT_HDCP_FAILED, CLR);
	} else {
		SiI_Ctrl.bHDCPFailFrmCnt = 0;
		SiI_Ctrl.bIgnoreIntr &= (~qcIgnoreHDCPFail);
		siiIIC_RX_RWBitsInByteP0(RX_HDMI_INT_ST2_MASK_ADDR,
			RX_BIT_VSYNC, CLR);
		siiIIC_RX_RWBitsInByteP0(RX_HDMI_INT_ST4_MASK_ADDR,
			RX_BIT_HDCP_FAILED, SET);
	}
}
/*------------------------------------------------------------------------------
 * Function Name: GotNConsecutiveVSyncHDCPFailure
 * Function Description: If HDCP Failure is repeated on frame to frame base
 *                       and reached threshold then function return TRUE
 *                       The function is called if: VSync && HDCPFailureInt
 *----------------------------------------------------------------------------
 */
static BOOL GotNConsecutiveVSyncHDCPFailure(void)
{
	BOOL qResult = FALSE;

	if (!DetectHDCPStuck())
		SiI_Ctrl.bHDCPFailFrmCnt++;
	if (SiI_Ctrl.bHDCPFailFrmCnt == cbHDCPFailFrmThres)
		qResult = TRUE;
	return qResult;
}

/*------------------------------------------------------------------------------
 * Function Name: DetectHDCPStuck
 * Function Description:
 *----------------------------------------------------------------------------
 */
static BOOL DetectHDCPStuck(void)
{
	WORD wHDCPErrors;
	BOOL qError = FALSE;

	wHDCPErrors = siiReadWordHDMIRXP0(RX_HDCP_ERR_ADDR);
	if (!wHDCPErrors)
		qError = TRUE;

	return qError;
}
/*------------------------------------------------------------------------------
 * Function Name: GotNConsecutiveVSyncHDCPStuckInterrupt
 * Function Description:
 *----------------------------------------------------------------------------
 */
static BYTE GotNConsecutiveHDCPStuckInterrupt(void)
{
	BYTE bError = FALSE;

	if (DetectHDCPStuck()) {
		SiI_Ctrl.bHDCPStuckConfirmCnt++;
		if (SiI_Ctrl.bHDCPStuckConfirmCnt == cbHDCPStuckConfirmThres)
			bError = SiI_EC_HDCP_StuckInterrupt;
	} else
		SiI_Ctrl.bHDCPStuckConfirmCnt = 0;

	return bError;
}
/*------------------------------------------------------------------------------
 * Function Name: HDCPErrorHandler
 * Function Description:  This function process HDCP errors and requests HDCP
 *                        re-authentication if it reached threshold
 *----------------------------------------------------------------------------
 */
static BYTE HDCPErrorHandler(BYTE bHDCPFailInt, BYTE bVSyncInt)
{
	BYTE bError = FALSE;

	if (SiI_Ctrl.bIgnoreIntr & qcIgnoreHDCPFail) {
		if (bVSyncInt) {
			if (bHDCPFailInt) {
				siiClearBCHCounter();
				bError = GotNConsecutiveHDCPStuckInterrupt();
				if (GotNConsecutiveVSyncHDCPFailure()) {
#ifdef SII_DUMP_UART
					printf("\n N HDCP Conseq failures!!");
#endif
					/* Clearing Ri will be notification for
					 *host to start
					 */
					ReqReAuthenticationRX();
					EnableCheckHDCPFailureWithVSyncRate(
						OFF);
				}
			} else {
				EnableCheckHDCPFailureWithVSyncRate(OFF);
			}
		}
	} else {
		if (bHDCPFailInt) {
			siiClearBCHCounter();
			bError = GotNConsecutiveHDCPStuckInterrupt();
			EnableCheckHDCPFailureWithVSyncRate(ON);
		}
	}
	return bError;
}

/*------------------------------------------------------------------------------
 * Function Name: HandleDeepColorError
 * Function Description:  Just clear the Deep Color error bit for now
 *----------------------------------------------------------------------------
 */
static void HandleDeepColorError(void)
{
	siiWriteByteHDMIRXP0(RX_HDMI_INT_ST6_ADDR, RX_BIT_DC_ERROR);
}

/*------------------------------------------------------------------------------
 * Function Name: RXInterruptHandler
 * Function Description: reading, clearing and dispatching RX interrupts
 *----------------------------------------------------------------------------
 */
static BYTE RXInterruptHandler(void)
{
	BYTE abIntrData[4];
	BYTE bError = FALSE;

	RXEXTDBG("sii9135 %s:\n", __func__);
	/* Handling second group interrupts */

	/* YMA NOTE R 0x7B, 0x7C */
	siiReadBlockHDMIRXP0(RX_HDMI_INT_GR2_ADDR, 2, abIntrData);

	/* YMA should be bug, AAC is not in abIntrData[1] */
	/* This interrupt has been processed on polling base */
	/* abIntrData[1] &= (~( RX_BIT_ACP_PACKET | RX_BIT_AAC_DONE)); */

	/* This interrupt has been processed on polling base */
	abIntrData[1] &= (~RX_BIT_ACP_PACKET);
	siiWriteBlockHDMIRXP0(RX_HDMI_INT_GR2_ADDR, 2, abIntrData);

	if ((abIntrData[0] & RX_BIT_AAC_DONE) &&
		(!(SiI_Ctrl.bIgnoreIntr & qcIgnoreAAC))) {
		siiTurnAudioOffAndSetSM_AudioOff();
		siiWriteByteHDMIRXP0(RX_HDMI_INT_GR2_ADDR, RX_BIT_AAC_DONE);
#ifdef SII_DUMP_UART
		printf("\nAAC");
#endif
	}

	if ((abIntrData[0] & RX_BIT_HRES_CHANGE) ||
		(abIntrData[0] & RX_BIT_VRES_CHANGE)) {
		/* it is caused a false interrupt, by pix replication change,
		 *before unmute we well recheck
		 */
		/* if video really wasn't changed */
		if (!siiSMCheckReqVideoOn())
			siiMuteVideoAndSetSM_SyncInChange();
	}

	if (abIntrData[1] & BIT_DSD_ON) {
		SiI_Ctrl.sm_bAudio = SiI_RX_AS_ReqAudio;
		siiChangeDSDAudioStreamHandler();
	}

	/* 9135 only */
	if ((SiI_Ctrl.bDevId == RX_SiI9135 || SiI_Ctrl.bDevId == RX_SiI9125) &&
		(abIntrData[1] & BIT_HBRA_ON_CHANGED)) {
		SiI_Ctrl.sm_bAudio = SiI_RX_AS_ReqAudio;
		siiChangeHBRAudioStreamHandler();
	}

	if (abIntrData[1] & RX_BIT_DC_ERROR)
		HandleDeepColorError();

	/* Handling first group interrupts */

	/* Copy Interrupt Events into RAM */
	siiReadBlockHDMIRXP0(RX_HDMI_INT_GR1_ADDR, 4, abIntrData);
	/* This interrupt has been processed on polling base */
	abIntrData[0] &= (~RX_BIT_CTS_CHANGED);
	/* This interrupt has been processed on polling base */
	abIntrData[1] &= (~(RX_BIT_GOT_AUDIO_PKT | RX_BIT_GOT_CTS_PKT));

	if (SiI_Ctrl.bIgnoreIntr & qcIgnoreHDCPFail) {
		if (!(abIntrData[1] & RX_BIT_VSYNC)) {
			/*this interrupt shouldn't be cleared because no VSync*/
			abIntrData[3] &= (~RX_BIT_HDCP_FAILED);
		}
	}

	/* Clear Interrupts */
	siiWriteBlockHDMIRXP0(RX_HDMI_INT_GR1_ADDR, 4, abIntrData);

	if (abIntrData[1] & RX_BIT_SCDT_CHANGE) {
		EnableCheckHDCPFailureWithVSyncRate(OFF);
		siiMuteVideoAndSetSM_SyncInChange();
		if ((SiI_Ctrl.bDevId == RX_SiI9021) ||
			(SiI_Ctrl.bDevId == RX_SiI9031)) {
			/* SiI9031/21 cannot auto-mute when no SCDT */
			siiTurnAudioOffAndSetSM_AudioOff();
		}
		siiSetAFEClockDelay();
#ifdef SII_BUG_PHOEBE_AUTOSW_BUG
		if ((SiI_Ctrl.bDevId == RX_SiI9023) ||
			(SiI_Ctrl.bDevId == RX_SiI9033)) {
			siiTurnAudioOffAndSetSM_AudioOff();
			siiSetAutoSWReset(ON);
		}
#endif
	}

	if (abIntrData[1] & RX_BIT_HDMI_CHANGED)
		ProcessChangeBetween_DVI_HDMI_Modes();

	if (SiI_Inf.bGlobStatus & SiI_RX_GlobalHDMI_Detected) {
		/* YMA use HDCP start is better
		 *if (abIntrData[0] & RX_BIT_HDCP_DONE) {
		 */
		if (abIntrData[0] & RX_BIT_HDCP_START)
			EnableCheckHDCPFailureWithVSyncRate(OFF);

		if ((abIntrData[3] & RX_BIT_HDCP_FAILED) ||
			(SiI_Ctrl.bIgnoreIntr & qcIgnoreHDCPFail)) {
			bError = HDCPErrorHandler(
				abIntrData[3] & RX_BIT_HDCP_FAILED,
				abIntrData[1] & RX_BIT_VSYNC);
		}

		if ((abIntrData[2] & ST3_INFO_MASK) ||
			(abIntrData[3] & ST4_INFO_MASK)) {
			if ((abIntrData[3] & RX_BIT_MASK_NO_AVI_INTR) &&
				(SiI_Ctrl.bIgnoreIntr & qcIgnoreNoAVI)) {
				abIntrData[2] |= NoAVI_Mask;
			} else {
				abIntrData[2] &= (~NoAVI_Mask);
			}

			AVI_And_NoAVI_WorkAround(&abIntrData[2]);
			siiProcessInfoFrameIntrrupts(abIntrData[2]);
		}

		if ((abIntrData[2] & RX_BIT_NEW_CP_PACKET) &&
			((SiI_Ctrl.bDevId == RX_SiI9133) ||
			(SiI_Ctrl.bDevId == RX_SiI9135 ||
			SiI_Ctrl.bDevId == RX_SiI9125))) {
			siiSetDeepColorMode();
		}
	}

	return bError;
}

#ifdef SII_USE_RX_PIN_INTERRUPT
/*------------------------------------------------------------------------------
 * Function Name: siiRX_PinInterruptHandler
 * Function Description: Checks interrupt pin, and call RX interrupt handler if
 *                       pin is active (negative)
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
BYTE siiRX_PinInterruptHandler(void)
{
	BYTE bError = FALSE;

	if (!halReadRXInt_Pin())
		bError = RXInterruptHandler();
	return bError;
}
#else
/*------------------------------------------------------------------------------
 * Function Name: CheckRXInt_Bit
 * Function Description: checks RX global interrupt bit, used when polling RX
 *                       interrupts
 *----------------------------------------------------------------------------
 */
static BOOL CheckRXInt_Bit(void)
{
	BOOL qResult = FALSE;
	BYTE value;

	value = siiReadByteHDMIRXP0(RX_INTR_STATE_ADDR);
	RXEXTDBG("sii9135 %s: value = 0x%x\n", __func__, value);
	if (value & RX_BIT_GLOBAL_INTR)
		qResult = TRUE;

	return qResult;
}
/*------------------------------------------------------------------------------
 * Function Name: siiRX_BitInterruptHandler
 * Function Description: Checks global interrupt bit, and call RX interrupt
 *                       handler if bit is One
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
BYTE siiRX_BitInterruptHandler(void)
{
	if (CheckRXInt_Bit())
		return RXInterruptHandler();

	return FALSE;
}

#endif /* end SII_USE_RX_PIN_INTERRUPT */

