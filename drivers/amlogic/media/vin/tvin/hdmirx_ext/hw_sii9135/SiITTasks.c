/*------------------------------------------------------------------------------
 * Module Name: SiI Timer tasks
 * ---
 * Module Description: Here timer schedueled tasks will be called
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
#include "SiITTasks.h"
#include "SiITTVideo.h"
#include "SiITTAudio.h"
#include "SiITTInfoPkts.h"
#include "SiITTHDCP.h"
#include "SiIISR.h"
#include "SiIHDMIRX.h"
#include "SiISysCtrl.h"
#include "SiITrace.h"
/*------------------------------------------------------------------------------
 * Function Name: siiInitACPCheckTimeOut
 * Function Description: Intitialize ACP Timeout Check
 *
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiInitACPCheckTimeOut(void)
{
	SiI_Ctrl.bInfoFrameTimeOut = 300;
}

/*------------------------------------------------------------------------------
 * Function Name: TTSysTasksHandler
 * Function Description:  This function clears BCH Errors, Polls HDMI Cable
 *                        Status and calls ReInit RX on HDMI Cable Plug In Event
 *----------------------------------------------------------------------------
 */
static void TTSysTasksHandler(void)
{
	SiI_Inf.bHDCPStatus = siiGetHDCPStatus();

	if (siiRX_CheckCableHPD()) {
		if (!(SiI_Inf.bGlobStatus & SiI_RX_Global_OldHotPlugDetect)) {
			SiI_Inf.bGlobStatus |= SiI_RX_Global_OldHotPlugDetect;
			siiReInitRX();

			/* YMA set the terminaion to 3k ohm first and then
			 *after HPD, change it back to normal
			 */
			if (SiI_Ctrl.bDevId == RX_SiI9135 ||
				SiI_Ctrl.bDevId ==  RX_SiI9125) {
				/* YMA others use default */
				if (SiI_Ctrl.bVidInChannel == SiI_RX_VInCh1)
					siiSetNormalTerminationValueCh1(ON);
				else
					siiSetNormalTerminationValueCh2(ON);
			}
		}
		SiI_Inf.bGlobStatus |= SiI_RX_Global_HotPlugDetect;
	} else {
		if (SiI_Ctrl.bDevId == RX_SiI9135 ||
			SiI_Ctrl.bDevId ==  RX_SiI9125) {
			/* YMA others use default */
			if (SiI_Ctrl.bVidInChannel == SiI_RX_VInCh1)
				siiSetNormalTerminationValueCh1(OFF);
			else
				siiSetNormalTerminationValueCh2(OFF);
		}

		SiI_Inf.bGlobStatus &= (~SiI_RX_Global_HotPlugDetect);
		SiI_Inf.bGlobStatus &= (~SiI_RX_Global_OldHotPlugDetect);
	}
}
/*------------------------------------------------------------------------------
 * Function Name: CheckAudioTimeOut()
 * Function Description:
 *----------------------------------------------------------------------------
 */
BOOL CheckAudioTimeOut(WORD wTimeDiff)
{
	BOOL qResult = FALSE;

	if (SiI_Ctrl.wAudioTimeOut) {
		if (SiI_Ctrl.wAudioTimeOut >= wTimeDiff) {
			SiI_Ctrl.wAudioTimeOut -= wTimeDiff;
			if (!SiI_Ctrl.wAudioTimeOut)
				qResult = TRUE;
		} else {
			qResult = TRUE;
			SiI_Ctrl.wAudioTimeOut = 0;
		}
	}
	return qResult;
}
/*------------------------------------------------------------------------------
 * Function Name: CheckVideoTimeOut()
 * Function Description:
 *----------------------------------------------------------------------------
 */
BOOL CheckVideoTimeOut(WORD wTimeDiff)
{
	BOOL qResult = FALSE;

	if (SiI_Ctrl.wVideoTimeOut) {
		if (SiI_Ctrl.wVideoTimeOut >= wTimeDiff) {
			SiI_Ctrl.wVideoTimeOut -= wTimeDiff;
			if (!SiI_Ctrl.wVideoTimeOut)
				qResult = TRUE;
		} else {
			qResult = TRUE;
			SiI_Ctrl.wVideoTimeOut = 0;
		}
	}
	return qResult;
}
/*------------------------------------------------------------------------------
 * Function Name: CheckInfoFrameTimeOut()
 * Function Description:
 *----------------------------------------------------------------------------
 */
BOOL CheckInfoFrameTimeOut(WORD wTimeDiff)
{
	BOOL qResult = FALSE;

	if (SiI_Ctrl.bInfoFrameTimeOut) {
		if ((WORD)SiI_Ctrl.bInfoFrameTimeOut >= wTimeDiff) {
			SiI_Ctrl.bInfoFrameTimeOut -= wTimeDiff;
			if (!SiI_Ctrl.bInfoFrameTimeOut)
				qResult = TRUE;
		} else {
			qResult = TRUE;
		}

	}
	return qResult;
}
/*------------------------------------------------------------------------------
 * Function Name: siiTimerTasksHandler
 * Function Description:
 *----------------------------------------------------------------------------
 */
BYTE siiTimerTasksHandler(BYTE bSlotTime)
{
	static BYTE bTaskSlot = SiI_RX_TT_Video;
	BYTE bECode = FALSE;
	BOOL qAudioTimeOut = FALSE;
	BOOL qVideoTimeOut = FALSE;

	if (CheckVideoTimeOut(bSlotTime)) {
		qVideoTimeOut = TRUE;
		siiTTVideoHandler(qVideoTimeOut);
	}
	if (SiI_Inf.bGlobStatus & SiI_RX_GlobalHDMI_Detected) {
		if (CheckAudioTimeOut(bSlotTime)) {
			qAudioTimeOut = TRUE;
			bECode = siiTTAudioHandler(qAudioTimeOut);
			if (bECode == SiI_EC_FIFO_UnderRunStuck)
				siiReInitRX();
		}
		if (CheckInfoFrameTimeOut(bSlotTime)) {
			siiInitACPCheckTimeOut();
			siiTTInfoFrameHandler();
		}
	}

	switch (bTaskSlot++) {
	case SiI_RX_TT_Video:
		siiTTVideoHandler(0);
		break;
	case SiI_RX_TT_Audio:
		siiTTAudioHandler(0);
		break;
	case SiI_RX_TT_InfoFrmPkt:
		break;
	case SiI_RX_TT_Sys:
		TTSysTasksHandler();
		break;
	}
	if (bTaskSlot == SiI_RX_TT_Last)
		bTaskSlot = SiI_RX_TT_Video;

	return bECode;
}

