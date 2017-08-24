/*------------------------------------------------------------------------------
 * Module Name main
 * Module Description: this file is used to call initalization of MCU, RX API
 *                     call users tasks, including HDMI RX API, processing
 *                     Audio Content Protection (ACP) on user's level.
 *
 * Copyright © 2002-2005, Silicon Image, Inc.  All rights reserved.
 *
 * No part of this work may be reproduced, modified, distributed, transmitted,
 * transcribed, or translated into any language or computer format, in any form
 * or by any means without written permission of: Silicon Image, Inc.,
 * 1060 East Arques Avenue, Sunnyvale, California 94085
 *----------------------------------------------------------------------------
 */

#include <linux/timer.h>

#include "SiIGlob.h"
#include "SiIRX_API.h"
#include "UGlob.h"
/* #include "UInitMCU.h" */
#include "UCfgRX.h"
/* #include "UCom.h" */
/* #include "UKeyboard.h" */
#include "SiIHAL.h"
#include "../platform_iface.h"

#ifdef _STD_C_
#pragma hdrstop
#endif
/*---------------------------------------------------------------------------
 * RX API Global data
 *---------------------------------------------------------------------------
 */
IRAM struct SiI_InfoType_s SiI_Inf;
IRAM struct SiI_CtrlType_s SiI_Ctrl;

IRAM BYTE SiI_bWCode[WRN_BUF_SIZE];
IRAM BYTE SiI_bECode[ERR_BUF_SIZE];

/*---------------------------------------------------------------------------
 * User Global data used for task management
 *---------------------------------------------------------------------------
 */
BOOL qReqTasksProcessing = TRUE;/* FALSE; //  request to process user's tasks */
DWORD wTickCounter;
DWORD wNewTaskTickCounter;
BYTE bNewTaskSlot;

/*---------------------------------------------------------------------------
 * These global data are used in debugger mode
 *---------------------------------------------------------------------------
 */
IRAM BYTE RXBuffer[RX_BUF_SIZE];
BYTE bRxIndex, bCommState, bCommTO;
BOOL qBuffInUse, qFuncExe, qDebugModeOnlyF;

/*------------------------------------------------------------------------------
 * Function Name: userHDMItask
 * Function Description:
 *----------------------------------------------------------------------------
 */
static void userHDMItask(void)
{
#if 1
	BYTE bRX_ChEvents;
	BYTE abRXParameters[8];
	static BOOL qACPStatus = FALSE;
	WORD wAudioSelect, wI2SFormat;

	SiI_RX_DoTasks(&bRX_ChEvents);

	RXEXTDBG("sii9135 %s: bRx_ChEvents = 0x%x\n", __func__, bRX_ChEvents);

	if (bRX_ChEvents & SiI_RX_API_GlobalStatus_Changed) {
		SiI_RX_GetSystemInformation(abRXParameters);
		wAudioSelect = (WORD)(abRXParameters[0] |
			abRXParameters[1] << 8);
		wI2SFormat = (WORD)(abRXParameters[2] | abRXParameters[3] << 8);
		if ((abRXParameters[6] & SiI_RX_GlobalHDMI_ACP) &&
			(!qACPStatus)) {
			/* SPDIF must be disabled */
			qACPStatus = TRUE;
			SiI_RX_GetAudioOutputFormat(abRXParameters);
			/* modify to disable SPDIF */
			wAudioSelect &= (~SiI_RX_AOut_SPDIF);
			SiI_RX_SetAudioOutputFormat(wAudioSelect, wI2SFormat,
				0x00, abRXParameters[5]);
		} else {
			if ((!(abRXParameters[6] & SiI_RX_GlobalHDMI_ACP)) &&
				qACPStatus) {
				qACPStatus = FALSE;
				SiI_RX_GetAudioOutputFormat(abRXParameters);
				/* modify to enable SPDIF */
				wAudioSelect |= SiI_RX_AOut_SPDIF;
				SiI_RX_SetAudioOutputFormat(wAudioSelect,
					wI2SFormat, 0x00, abRXParameters[5]);
			}
		}
	}
#endif
}

/*------------------------------------------------------------------------------
 * Function Name:   main
 * Function Description:
 *	call uinitilizations and infinite loop with user's tasks
 *----------------------------------------------------------------------------
 */

#define SII9135_PUT_INTERVAL    10   /* 10ms, #define HZ 100 */
int sii9135_main(void)
{
	wTickCounter = 0;
	wNewTaskTickCounter = 0;
	bNewTaskSlot = 0;

	/* Amlogic init */
	__plat_timer_init(SysTickTimerISR);
	__plat_timer_set(SII9135_PUT_INTERVAL);

	qDebugModeOnlyF = FALSE;
	/* siiInitializeMCU(); */

	siiRXConfig();

	while (1) {
		/*
		 *if (qReqTasksProcessing) {

		 *	qReqTasksProcessing = FALSE;

		 *	if (!qDebugModeOnlyF) {
		 *		if (bNewTaskSlot == TASK_HDMI_RX_API)
		 *			userHDMItask();
		 *		else if ( bNewTaskSlot == TASK_KEYBOARD )
		 *			;//siiKeyboardHandler();
		 *	}
		 *	if (bNewTaskSlot == TASK_COMM)
		 *		;//siiCommunicationHandler();
		 *}
		 */
		userHDMItask();
		__plat_msleep(200);
	}
	return 0;
}
/*---------------------------------------------------------------------------*/

