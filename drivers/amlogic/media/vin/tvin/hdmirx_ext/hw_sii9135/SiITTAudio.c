/*------------------------------------------------------------------------------
 * Module Name: SiITTAudio
 *
 * Module Description:    Serves Audio Timeing evevts and audio state machine
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
#include "SiITTAudio.h"
#include "SiITTVideo.h" /* need to sync with video */
#include "SiIAudio.h"
#include "SiITrace.h"
#include "SiIHAL.h"

#ifdef SII_DUMP_UART
#include "SiIISR.h"
#include "../hdmirx_ext_drv.h"
#endif

/*------------------------------------------------------------------------------
 * Function Name: siiIsAudioHoldMuteState
 * Function Description:  This function checks state machine, if state
 *                        SiI_RX_AS_HoldAudioMute then return true
 *
 * Accepts: none
 * Returns: BOOL
 * Globals: none
 *----------------------------------------------------------------------------
 */
BOOL siiIsAudioHoldMuteState(void)
{
	BOOL qResult = FALSE;

	if (SiI_Ctrl.sm_bAudio == SiI_RX_AS_HoldAudioMute)
		qResult = TRUE;
	return qResult;
}

/*------------------------------------------------------------------------------
 * Function Name: SetTimeOutOandSM_ReqWaitVideoOn
 * Function Description:  assign this state and time out in order to
 *                        sync Audio with Video
 *
 * Accepts: none
 * Returns: none
 * Globals: SiI_Ctrl.sm_bAudio, SiI_Ctrl.wAudioTimeOut
 *----------------------------------------------------------------------------
 */
static void SetTimeOutandSM_ReqWaitVideoOn(void)
{
	SiI_Ctrl.sm_bAudio = SiI_RX_AS_WaitVideoOn;
	SiI_Ctrl.wAudioTimeOut = 16;
}
/*------------------------------------------------------------------------------
 * Function Name: siiIsAudioStatusReady
 * Function Description:  This function checks state machine, if state AudioOn
 *                        then return true
 *
 * Accepts: none
 * Returns: BOOL
 * Globals: none
 *----------------------------------------------------------------------------
 */
BOOL siiIsAudioStatusReady(void)
{
	BOOL qResult = FALSE;

	if (SiI_Ctrl.sm_bAudio == SiI_RX_AS_AudioOn)
		qResult = TRUE;
	return qResult;
}

/*------------------------------------------------------------------------------
 * Function Name: siiSetSM_ReqAudio
 * Function Description:  asserts  SiI_RX_AS_ReqAudio, this state starts check
 *                         Audio conditions exception asserted
 *                         SiI_RX_AS_HoldAudioMute state
 *
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiSetSM_ReqAudio(void)
{
	/* Audio mute which is asserted by API command can be
	 * can be de-asserted only by command
	 */
	if (SiI_Ctrl.sm_bAudio != SiI_RX_AS_HoldAudioMute) {
		SiI_Ctrl.sm_bAudio = SiI_RX_AS_ReqAudio;
		SiI_Ctrl.wAudioTimeOut = 5;
	}
}
/*------------------------------------------------------------------------------
 * Function Name: SetSM_HoldAudioMute
 * Function Description:  asserts and deaserts SiI_RX_AS_HoldAudioMute
 *                        only by this function iI_RX_AS_HoldAudioMute
 *                        can be deasserted
 *----------------------------------------------------------------------------
 */
static void SetSM_HoldAudioMute(BOOL qOn)
{
	if (qOn) {
		SiI_Ctrl.sm_bAudio = SiI_RX_AS_HoldAudioMute;
		SiI_Ctrl.wAudioTimeOut = 0;
	} else {
		SiI_Ctrl.sm_bAudio = SiI_RX_AS_ReqAudio;
		SiI_Ctrl.wAudioTimeOut = 5;
	}

}
/*------------------------------------------------------------------------------
 * Function Name: SetSM_AudioOff
 * Function Description:   this function sets SiI_RX_AS_AudioOff, exception
 *                         asserted SiI_RX_AS_HoldAudioMute state
 *----------------------------------------------------------------------------
 */
static void SetSM_AudioOff(void)
{
	/* Audio mute which is asserted by API command can be
	 * can be de-asserted only by command
	 */
	if (SiI_Ctrl.sm_bAudio != SiI_RX_AS_HoldAudioMute) {
		SiI_Ctrl.sm_bAudio = SiI_RX_AS_AudioOff;
		/* # ms being quiet, (not to attepmt bring up Audio) */
		SiI_Ctrl.wAudioTimeOut = 200;
	}
}
/*------------------------------------------------------------------------------
 * Function Name: siiTurnAudioOffAndSetSM_AudioOff
 * Function Description: This function calls turning Audio Off, and changes
 *                       state of the state machine
 *
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiTurnAudioOffAndSetSM_AudioOff(void)
{
	/* siiSetAutoFIFOReset(OFF); */
	siiTurningAudio(OFF);
	SetSM_AudioOff();
}
/*------------------------------------------------------------------------------
 * Function Name: siiSetGlobalAudioMuteAndSM_Audio
 * Function Description: Asserts or Deasserts Audio Mute
 *
 * Accepts: BOOL
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiSetGlobalAudioMuteAndSM_Audio(BOOL qOn)
{
	if (qOn) {
		siiSetAudioMuteEvent();
		SetSM_HoldAudioMute(ON);
	} else {
		SetSM_HoldAudioMute(OFF);
	}

}
/*------------------------------------------------------------------------------
 * Function Name: siiSetSM_ReqHoldAudioMute
 * Function Description:  This function emolliates waiting for Soft Muting,
 *            it should be used to wait when audio soft mute is ramping down
 * Accepts: none
 * Returns: none
 *----------------------------------------------------------------------------
 */

void siiSetSM_ReqHoldAudioMuteAndStartMuting(BOOL qOn)
{
	if (qOn) {
		siiAudioMute(ON);
		SiI_Ctrl.sm_bAudio = SiI_RX_AS_ReqHoldAudioMute;
		SiI_Ctrl.wAudioTimeOut = 50; /* delays for 50 ms */
	} else {
		SetSM_HoldAudioMute(OFF);
	}

}
/*------------------------------------------------------------------------------
 * Function Name: CheckAudio_IfOK_InitACR
 * Function Description: If Audio is OK and ACR init., then set time after which
 * CTS change interrupt can be cleared (used for STS stability check later)
 *----------------------------------------------------------------------------
 */
static void CheckAudio_IfOK_InitACR(void)
{
	siiClearCTSChangeInterruprt();
	if (siiCheckAudio_IfOK_InitACR()) {
		SiI_Ctrl.wAudioTimeOut = 4;
		SiI_Ctrl.sm_bAudio = SiI_RX_AS_StartCheckCTS_Stable;
	} else {
		SetSM_AudioOff();
	}

}
/*------------------------------------------------------------------------------
 * Function Name: StartCheckCTS_Stable
 * Function Description: Call clear CTS change interrupt,
 *                       change state machine for waiting PLL Lock
 *----------------------------------------------------------------------------
 */
static void StartCheckCTS_Stable(void)
{
	siiClearCTSChangeInterruprt();
	SiI_Ctrl.sm_bAudio = SiI_RX_AS_WaitAudioPPLLock;
	SiI_Ctrl.wAudioTimeOut = 150;  /* Set wait # ms for PLL Locking */

}
/*------------------------------------------------------------------------------
 * Function Name: PrepareTurningAudioOn
 * Function Description: If Audio prepered for turning on (waked up Audio DAC),
 *                       hten set delay when Audio DAC will be ready
 *----------------------------------------------------------------------------
 */
static void PrepareTurningAudioOn(void)
{
	if (siiPrepareTurningAudioOn()) {
		SiI_Ctrl.sm_bAudio = SiI_RX_AS_WaitAudioDACReady;
		/* Set wait # ms, when Audio DAC will be ready */
		SiI_Ctrl.wAudioTimeOut = SiI_RX_DelayAudio_WaitDACWakingUp;
	} else {
		SiI_Ctrl.sm_bAudio = SiI_RX_AS_AudioOff;
		/* # ms being quiet, (not to attepmt bring up Audio) */
		SiI_Ctrl.wAudioTimeOut = 200;
	}
}
/*------------------------------------------------------------------------------
 * Function Name: TurningAudioOn
 * Function Description: call teurning Audio On and change state machine
 *                       to Audio On
 *----------------------------------------------------------------------------
 */
static void TurningAudioOn_SetSM_WaitStatusLock(void)
{
	siiTurningAudio(ON);
	SiI_Ctrl.sm_bAudio = SiI_RX_AS_AudioOnWaitLockStatus;
	/* # ms being quiet, (not to attepmt bring up Audio) */
	SiI_Ctrl.wAudioTimeOut = 700;
}
/*------------------------------------------------------------------------------
 * Function Name:
 * Function Description:
 *----------------------------------------------------------------------------
 */
static void LockAudioStatusSetSM_AudioOn(void)
{
	siiSaveInputAudioStatus();
	SiI_Ctrl.sm_bAudio = SiI_RX_AS_AudioOn;
}

/*------------------------------------------------------------------------------
 * Function Name:   siiTTAudioHandler
 * Function Description:  Calls processing of Audio Timing Events
 *
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
BYTE siiTTAudioHandler(BOOL qTimeOut)
{
	BYTE bECode = FALSE;

	switch (SiI_Ctrl.sm_bAudio) {
	case SiI_RX_AS_ReqAudio:
		if  (qTimeOut)
			CheckAudio_IfOK_InitACR();
		break;
	case SiI_RX_AS_StartCheckCTS_Stable:
		if (qTimeOut) {
			StartCheckCTS_Stable();
			siiClearGotCTSAudioPacketsIterrupts();
		}
		break;
	case SiI_RX_AS_WaitAudioPPLLock:
		if (qTimeOut)
			PrepareTurningAudioOn();
		break;
	case SiI_RX_AS_AudioOff:
		if (qTimeOut) {
			siiSetSM_ReqAudio();
			siiClearGotCTSAudioPacketsIterrupts();
		}
		break;
	case SiI_RX_AS_WaitAudioDACReady:
		if (qTimeOut) {
			if (siiIsVideoOn()) {
#ifdef SII_DUMP_UART
				siiTraceRXInterrupts();
#endif
				TurningAudioOn_SetSM_WaitStatusLock();
#ifdef SII_DUMP_UART
				printf(" Aud On ");
#endif
			} else
				SetTimeOutandSM_ReqWaitVideoOn();
		}
		break;
	case SiI_RX_AS_AudioOnWaitLockStatus:
		if (qTimeOut)
			LockAudioStatusSetSM_AudioOn();
		break;
	case SiI_RX_AS_HoldAudioMute:
		/* Do nothing, a global Audio Mute is asserted! */
		break;
	case SiI_RX_AS_WaitVideoOn:
		if (qTimeOut) {
			if (siiIsVideoOn())
				TurningAudioOn_SetSM_WaitStatusLock();
			else
				SetTimeOutandSM_ReqWaitVideoOn();
		}
		break;
	case SiI_RX_AS_ReqHoldAudioMute:
		if (qTimeOut) {
			SetSM_HoldAudioMute(ON);
			siiTurnAudioOffAndSetSM_AudioOff();
		}
		break;
	}
	bECode = SiI_Inf.bAudioErr;
	return bECode;

}

