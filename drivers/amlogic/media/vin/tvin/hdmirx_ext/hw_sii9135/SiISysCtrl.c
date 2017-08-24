/*------------------------------------------------------------------------------
 * Module Name
 * ---
 * Module Description…
 *
 * Copyright © 2002-2005, Silicon Image, Inc.  All rights reserved.
 *
 * No part of this work may be reproduced, modified, distributed, transmitted,
 * transcribed, or translated into any language or computer format, in any form
 * or by any means without written permission of: Silicon Image, Inc.,
 * 1060 East Arques Avenue, Sunnyvale, California 94085
 *----------------------------------------------------------------------------
 */

#include "SiISysCtrl.h"
#include "SiIGlob.h"
#include "SiIHAL.h"
#include "SiIRXDefs.h"
#include "SiIRXAPIDefs.h"
#include "SiITrace.h"
#include "SiIHDMIRX.h"
#include "SiIAudio.h"
#include "SiIVidF.h"
#include "SiITTInfoPkts.h"
#include "UAudDAC.h"
/*------------------------------------------------------------------------------
 * Function Name: siiSaveRXInitParameters()
 * Function Description:
 *
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiSaveRXInitParameters(BYTE *pbParameter)
{
	SiI_Ctrl.bRXInitPrm0 = pbParameter[0];
	SiI_Inf.AVI.bOutputColorDepth = pbParameter[1]; /* YMA fix */
	/* SiI_Ctrl.bVidInChannel = pbParameter[1]; //YMA wrong info saved
	 * SiI_Ctrl.bVidInChannel = pbParameter[2];
	 */

}
/*------------------------------------------------------------------------------
 * Function Name: siiGetRXInitParameters()
 * Function Description:
 *
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiGetRXInitParameters(BYTE *pbParameter)
{
	pbParameter[0] = SiI_Ctrl.bRXInitPrm0; /* FPGA and MClock */
	pbParameter[1] = SiI_Inf.AVI.bOutputColorDepth; /*YMA fix */
	/* pbParameter[1] = SiI_Ctrl.bVidInChannel;     // Video input id
	 * pbParameter[2] = SiI_Ctrl.bVidInChannel;     // Video input id
	 */

}
/*------------------------------------------------------------------------------
 * Function Name: siiInitilizeSystemData
 * Function Description:
 *
 * Accepts: BOOL Key for initialization System data
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
BYTE siiInitilizeSystemData(BOOL qDoFromScratch)
{
	BYTE bEWCode = FALSE;

	if (qDoFromScratch) {
		SiI_Ctrl.wAudioOutputSelect = SiI_RX_AOut_Default;
		SiI_Ctrl.sm_bAudio = SiI_RX_AS_AudioOff;
		halWakeUpAudioDAC();
		PowerDownAudioDAC();
		SiI_Inf.bGlobStatus = SiI_RX_GlobalHDMI_NoAVIPacket |
			SiI_RX_Global_OldHotPlugDetect;
	} else {
		/* clear these these flags and retain others */
		SiI_Inf.bGlobStatus = ~(SiI_RX_Global_HotPlugDetect |
			SiI_RX_GlobalHDMI_Detected | SiI_RX_GlobalHDMI_ACP |
			SiI_RX_GlobalHDMI_NoAVIPacket);

	}
	siiResetACPPacketData();
	SiI_Ctrl.sm_bVideo = SiI_RX_VS_PowerDown;
	SiI_Ctrl.wVideoTimeOut = 20; /* YMA PD issue fix */

	SiI_Ctrl.bShadowPixRepl = 0;
	SiI_Ctrl.bIgnoreIntr = 0;
	SiI_Ctrl.bHDCPFailFrmCnt = 0;
	SiI_Ctrl.bHDCPStuckConfirmCnt = 0;

	SiI_Inf.AVI.bInputColorDepth = SiI_RX_CD_24BPP;

	SiI_Inf.bVResId = 0;
	SiI_Inf.AVI.bAVI_State = SiI_RX_NoAVI;
	SiI_Inf.AVI.bColorimetry = SiI_RX_ColorimetryNoInfo;
	SiI_Inf.AVI.bInputColorSpace = SiI_RX_ICP_RGB;
	SiI_Inf.AVI.bPixRepl = 0;
	SiI_Inf.bNewInfoPkts = 0;

	return bEWCode;
}

/*------------------------------------------------------------------------------
 * Function Name: siiConvertTicksInMS
 * Function Description: Converts System timer ticks in milliseconds
 *
 * Accepts: wTicks
 * Returns: wMS
 * Globals: none
 *----------------------------------------------------------------------------
 */
WORD siiConvertTicksInMS(WORD wTicks)
{
	WORD wMS;
	DWORD dwUS;

	dwUS = wTicks * SII_SYS_TICK_TIME; /* SII_SYS_TICK_TIME in US */
	wMS = dwUS / 1000;
	return wMS;

}

#ifndef FIXED_TASK_CALL_TIME
/*------------------------------------------------------------------------------
 * Function Name:  siiDoTasksTimeDiffrence
 * Function Description:   Function is called in DoTasks
 *
 * Accepts:
 * Returns:  TimeDifference
 * Globals:
 *----------------------------------------------------------------------------
 */
BYTE siiDoTasksTimeDiffrence(void)
{
	WORD wTicksNumber;
	WORD wTicksDiff;

	wTicksNumber = siiGetTicksNumber();

	if (wTicksNumber >= SiI_Ctrl.SysTimer.wProcLastDoTasks_Ticks) {
		wTicksDiff =
			wTicksNumber - SiI_Ctrl.SysTimer.wProcLastDoTasks_Ticks;
	} else {
		wTicksDiff =
			SiI_Ctrl.SysTimer.wProcLastDoTasks_Ticks - wTicksNumber;
	}

	SiI_Ctrl.SysTimer.wProcLastDoTasks_Ticks = wTicksNumber;

	return siiConvertTicksInMS(wTicksDiff);

}
#endif
/*------------------------------------------------------------------------------
 * Function Name: siiMeasureProcLastAPI_Ticks
 * Function Description:  This function is used for time measuring, time
 *                        differnce shouldn't exceed 0xFFFF ticks
 * Accepts: none
 * Returns: Number ticks spent for execution of task
 * Globals: SiI_Ctrl.SysTimer.wProcLastAPI_Ticks
 *----------------------------------------------------------------------------
 */
void siiMeasureProcLastAPI_Ticks(WORD wStartTimeInTicks)
{
	WORD wNewTimeInTicks;

	wNewTimeInTicks = siiGetTicksNumber();

	if (wNewTimeInTicks > wStartTimeInTicks) {  /* check for roll over */
		SiI_Ctrl.SysTimer.wProcLastAPI_Ticks =
			wNewTimeInTicks - wStartTimeInTicks;
	} else {
		SiI_Ctrl.SysTimer.wProcLastAPI_Ticks =
			(0xFFFF - wNewTimeInTicks) + wStartTimeInTicks;
	}
}

/*------------------------------------------------------------------------------
 * Function Name: siiDiagnostic_GetAPI_ExeTime
 * Function Description: this function takes time for the last executed API
 *                       and convert it from ticks to ms
 * Accepts: none
 * Returns: BYTE * pbAPI_ExeTime
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiDiagnostic_GetAPI_ExeTime(BYTE *pbAPI_ExeTime)
{
	WORD wExeTime;

	wExeTime = siiConvertTicksInMS(SiI_Ctrl.SysTimer.wProcLastAPI_Ticks);
	pbAPI_ExeTime[0] = (BYTE)(wExeTime & 0x00FF);
	pbAPI_ExeTime[1] = (BYTE)((wExeTime & 0xFF00) >> 8);

}

/*------------------------------------------------------------------------------
 * Function Name: siiGetEventChenges
 * Function Description:  Reports about main changes in system
 *
 * Accepts: none
 * Returns: BYTE, change events
 * Globals: none
 *----------------------------------------------------------------------------
 */
BYTE siiGetSMEventChanges(void)
{
	BYTE bResult = 0;
	/* values should be same as initalized in siiInitilizeSystemData */
	static BYTE sm_bVideo = SiI_RX_VS_PowerDown;
	static BYTE sm_bAudio = SiI_RX_AS_AudioOff;
	static BYTE bHDCPStatus;
	static BYTE bGlobStatus;

	if (sm_bVideo != SiI_Ctrl.sm_bVideo) {
		bResult |= SiI_RX_API_VideoSM_Changed;
		sm_bVideo = SiI_Ctrl.sm_bVideo;
	}
	if (sm_bAudio != SiI_Ctrl.sm_bAudio) {
		bResult |= SiI_RX_API_AudioSM_Changed;
		sm_bAudio = SiI_Ctrl.sm_bAudio;
	}
	if (SiI_Inf.bNewInfoPkts)
		bResult |= SiI_RX_API_InfoPacket_Changed;
	if (bHDCPStatus != SiI_Inf.bHDCPStatus) {
		bResult |= SiI_RX_API_HDCPStatus_Changed;
		bHDCPStatus = SiI_Inf.bHDCPStatus;
	}
	if (bGlobStatus != SiI_Inf.bGlobStatus) {
		bGlobStatus = SiI_Inf.bGlobStatus;
		bResult |= SiI_RX_API_GlobalStatus_Changed;
	}

	return bResult;

}

/*------------------------------------------------------------------------------
 * Function Name: siiReInitRX
 * Function Description:  This function is used to Re-inialize HDMI RX
 *                        after Hardware reset
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiReInitRX(void)
{
	BYTE abData[3];
	struct AudioOutputFormatType_s AudioOutputFormat;

	/* Gets MClock divider and active TMDS core */
	siiGetRXInitParameters(abData);
	/* Get Audio Format Setting, some setting stored in RX regs. so it's
	 * Need to do before hardware reset
	 */
	siiGetAudioOutputFormat(&AudioOutputFormat);

	siiInitializeRX(abData); /*in this function  RX Hardware Reset is used*/
	siiInitilizeSystemData(OFF); /* Intilize System Data with Key OFF */
	/* with OFF key Video/Audio Format cfg. data is not modified
	 * Now it's need to restore Aideo Output Format
	 */
	/* Restore RX Audio Format Configuration */
	siiSetAudioOutputFormat(&AudioOutputFormat);

	/* Set Video Format static settings */
	/* Restore RX Video Format Configuration */
	abData[0] = abData[1] = abData[2] = 0;
	/* YMA restore the configuration, or outpout depth lost */
	siiGetVideoFormatData(abData);
	siiPrepSyncSelect(SiI_Ctrl.VideoF.bOutputSyncSelect, abData);
	siiPrepSyncCtrl(SiI_Ctrl.VideoF.bOutputSyncCtrl, abData);
	siiPrepVideoCtrl(SiI_Ctrl.VideoF.bOutputVideoCtrl, abData);
	/* Update RX Video format cfg. at Vid. Mode/Ctrl regs. */
	siiSetVideoFormatData(abData);

}


