/*------------------------------------------------------------------------------
 * Module Name: SiITTVideo
 *
 * Module Description:
 *
 * Copyright © 2002-2005, Silicon Image, Inc.  All rights reserved.
 *
 * No part of this work may be reproduced, modified, distributed, transmitted,
 * transcribed, or translated into any language or computer format, in any form
 * or by any means without written permission of: Silicon Image, Inc.,
 * 1060 East Arques Avenue, Sunnyvale, California 94085
 *----------------------------------------------------------------------------
 */

#include "SiIVidRes.h"
#include "SiIVidF.h"
#include "SiIGlob.h"
#include "SiITTVideo.h"
#include "SiITrace.h"
#include "SiIHDMIRX.h"
#include "SiIAudio.h"
#include "SiIVidIn.h"
#include "SiIISR.h"
#include "SiITTAudio.h"

#include "../hdmirx_ext_drv.h"

/*------------------------------------------------------------------------------
 * Function Name: IsVideoOn
 * Function Description: checks video state machine, state is used when res
 *                       change interrupt is ignorred
 * Accepts: none
 * Returns: BOOL, true if Video is on
 * Globals: SiI_Ctrl.sm_bVideo
 *----------------------------------------------------------------------------
 */
BOOL siiIsVideoOn(void)
{
	BOOL qResult = FALSE;

	if ((SiI_Ctrl.sm_bVideo == SiI_RX_VS_VideoOn) ||
		(SiI_Ctrl.sm_bVideo == SiI_RX_VS_VMNotDetected))
		qResult = TRUE;
	return qResult;
}
/*------------------------------------------------------------------------------
 * Function Name: siiSMCheckReqVideoOn
 * Function Description: checks video state machine, state is used when res
 *                       change interrupt is ignorred
 * Accepts: none
 * Returns: BOOL
 * Globals: Read only, SiI_Ctrl.sm_bVideo
 *----------------------------------------------------------------------------
 */

BOOL siiSMCheckReqVideoOn(void)
{
	BOOL qResult = FALSE;

	if (SiI_Ctrl.sm_bVideo == SiI_RX_VS_ReqVideoOn)
		qResult = TRUE;
	return qResult;

}
/*------------------------------------------------------------------------------
 * Function Name:  siiSetSM_ReqGlobalPowerDown
 * Function Description:   Sets SiI_RX_VS_ReqGlobalPowerDown and when
 *                         SiI_RX_VS_GlobalPowerDown will be asserted
 *----------------------------------------------------------------------------
 */
void siiSetSM_ReqGlobalPowerDown(void)
{
	SiI_Ctrl.sm_bVideo = SiI_RX_VS_ReqGlobalPowerDown;
	SiI_Ctrl.wVideoTimeOut = 32;

}
/*------------------------------------------------------------------------------
 * Function Name:  siiSetSM_ReqPowerDown
 * Function Description:   Sets SiI_RX_VS_ReqPowerDown and when
 *                         SiI_RX_VS_PowerDown will be asserted
 *----------------------------------------------------------------------------
 */
void siiSetSM_ReqPowerDown(void)
{
	SiI_Ctrl.sm_bVideo = SiI_RX_VS_ReqPowerDown;
	SiI_Ctrl.wVideoTimeOut = 1200;

}
/*------------------------------------------------------------------------------
 * Function Name:
 * Function Description:
 *----------------------------------------------------------------------------
 */
void siiSetPowerDownModeAndSetSM_PowerDown(void)
{
	SiI_Ctrl.sm_bVideo = SiI_RX_VS_PowerDown;
	siiRX_PowerDown(ON);
	SiI_Ctrl.wVideoTimeOut = 200;
}
/*-----------------------------------------------------------------------------
 * Function Name:
 * Function Description:
 *----------------------------------------------------------------------------
 */
void siiSetSM_ReqVidInChange(void)
{
	SiI_Ctrl.sm_bVideo = SiI_RX_VS_ReqVidInChange;
	SiI_Ctrl.wVideoTimeOut = 64;
}
/*------------------------------------------------------------------------------
 * Function Name: siiMuteVideoAndSetSM_SyncInChange
 * Function Description: Mutes Video, change Video state machine to SyncInCange,
 *                       and sets Time after which sync information will be read
 * Accepts: none
 * Returns: none
 * Globals: SiI_Ctrl.sm_bVideo modified
 *----------------------------------------------------------------------------
 */

void siiMuteVideoAndSetSM_SyncInChange(void)
{
	if (SiI_Ctrl.sm_bVideo != SiI_RX_VS_GlobalPowerDown) {
		siiMuteVideo(ON);
		siiRX_PowerDown(OFF);
		SiI_Ctrl.wVideoTimeOut = 100;
		SiI_Ctrl.sm_bVideo = SiI_RX_VS_SyncInChange;
	}

}

/*------------------------------------------------------------------------------
 * Function Name: siiCheckOverrangeConditions
 * Function Description: This function used to check sync overrange conditions
 * Accepts: none
 * Returns: Error Code
 * Globals: SiI_Inf:sm_bVideo modified
 *----------------------------------------------------------------------------
 */
/*******************************************************************************
 *->SiI_RX_VS_SyncStable
 *	Check Overrange Conditions
 *		-> SiI_RX_VS_ReqVMDetection
 *		-> SiI_RX_VS_SyncOutOfRange
 *
 ******************************************************************************
 */
void CheckOutOfRangeConditions(void)
{
	if (siiCheckOutOfRangeConditions(&SiI_Inf.Sync))
		SiI_Ctrl.sm_bVideo = SiI_RX_VS_SyncOutOfRange;
	else {
		SiI_Ctrl.sm_bVideo = SiI_RX_VS_ReqVMDetection;
		SiI_Ctrl.wVideoTimeOut = 10;
	}
}
/*------------------------------------------------------------------------------
 * Function Name: siiGetPixClock()
 * Function Description:  This function checks if video resolution detected,
 *                        if it wasn't detected then return Zero
 * Accepts: void
 * Returns: bPixClock
 * Globals: read only; SiI_Ctrl.sm_bVideo
 *----------------------------------------------------------------------------
 */
BYTE siiGetPixClock(void)
{
	BYTE bPixClock;

	if ((SiI_Ctrl.sm_bVideo == SiI_RX_VS_VMDetected) ||
		(SiI_Ctrl.sm_bVideo == SiI_RX_VS_VideoOn) ||
		(SiI_Ctrl.sm_bVideo == SiI_RX_VS_ReqVideoOn)) {
		bPixClock = siiGetPixClockFromTables(SiI_Inf.bVResId);
	} else
		bPixClock = 0;
	return bPixClock;
}
#ifdef SII_NO_RESOLUTION_DETECTION
/*------------------------------------------------------------------------------
 * Function Name:
 * Function Description:
 *
 * Accepts: void
 * Returns: qResult
 * Globals: read only; SiI_Ctrl.sm_bVideo
 *----------------------------------------------------------------------------
 */
BOOL siiCheckIfVideoOutOfRangeOrVMNotDetected(void)
{
	BOOL qResult = FALSE;

	if ((SiI_Ctrl.sm_bVideo == SiI_RX_VS_SyncOutOfRange) ||
		(SiI_Ctrl.sm_bVideo == SiI_RX_VS_VMNotDetected))
		qResult = TRUE;

	return qResult;
}
#endif
/*------------------------------------------------------------------------------
 * Function Name:
 * Function Description:
 *
 * Accepts: void
 * Returns: qResult
 * Globals: read only; SiI_Ctrl.sm_bVideo
 *----------------------------------------------------------------------------
 */
BOOL siiCheckIfVideoInputResolutionReady(void)
{
	BOOL qResult = FALSE;

	if ((SiI_Ctrl.sm_bVideo == SiI_RX_VS_VMDetected) ||
		(SiI_Ctrl.sm_bVideo == SiI_RX_VS_VideoOn) ||
		(SiI_Ctrl.sm_bVideo == SiI_RX_VS_ReqVideoOn))
		qResult = TRUE;

	return qResult;
}
/*------------------------------------------------------------------------------
 * Function Name: siiVideoResolutionDetection
 * Function Description: This function is used for Video Resolution detection
 * Accepts: none
 * Returns: Error Code
 * Globals: SiI_Inf:sm_bVideo, bVResId modified
 *----------------------------------------------------------------------------
 */
/*******************************************************************************
 *-> SiI_RX_VS_ReqVMDetection
 *	Video Resolution Detection
 *		-> SiI_RX_VS_VMDetected
 *		-> SiI_RX_VS_VMNotDetected ( unknown resolution )
 ******************************************************************************
 */
static void VideoResolutionDetection(void)
{
	BYTE bSearchResult;

	bSearchResult = siiVideoModeDetection(&SiI_Inf.bVResId, &SiI_Inf.Sync);
	if (!bSearchResult) {  /* Mode is not detected */
		SiI_Ctrl.sm_bVideo = SiI_RX_VS_VMNotDetected;
		/* siiMuteVideo(OFF); //YMA change */
		siiMuteVideo(ON);
	} else
		SiI_Ctrl.sm_bVideo = SiI_RX_VS_VMDetected;
	SiI_Ctrl.wVideoTimeOut = 10;
	/* #ifdef SII_DUMP_UART */
	siiPrintVModeParameters(SiI_Inf.bVResId, bSearchResult);
	/* #endif */

}
/*------------------------------------------------------------------------------
 * Function Name: PowerDownProcessing
 * Function Description:
 *----------------------------------------------------------------------------
 */
static void PowerDownProcessing(void)
{
	siiRX_PowerDown(OFF); /* SCDT doesn't work in PD */
	/* Wilma has the SCDT doesn't work in PD bug */
	/*
	 *  if ( siiCheckClockDetect() ){
	 *	siiMuteVideoAndSetSM_SyncInChange();
	 *  }
	 */
	if ((siiCheckSyncDetect()) ||
		((SiI_Ctrl.bDevId == RX_SiI9133) && (siiCheckClockDetect())))
		siiMuteVideoAndSetSM_SyncInChange();
	else
		siiSetPowerDownModeAndSetSM_PowerDown();
}

/*------------------------------------------------------------------------------
 * Function Name: SyncInChangeProcessing
 * Function Description:
 *----------------------------------------------------------------------------
 */
static void SyncInChangeProcessing(void)
{
	if (siiCheckSyncDetect()) {
		SiI_Ctrl.sm_bVideo = SiI_RX_VS_SyncStable;
		SiI_Ctrl.wVideoTimeOut = 10;
		siiGetSyncInfo(&SiI_Inf.Sync);
#ifdef SII_BUG_PHOEBE_AUTOSW_BUG
		if ((SiI_Ctrl.bDevId == RX_SiI9023) ||
			(SiI_Ctrl.bDevId == RX_SiI9033)) {
			siiSetAutoSWReset(OFF);
			siiTurnAudioOffAndSetSM_AudioOff();
		}
#endif
	} else {
		siiSetSM_ReqPowerDown();
	}
}
/*------------------------------------------------------------------------------
 * Function Name: siiSetResDepVideoPath
 * Function Description: This function is used to set Res Dependent Video path
 * Accepts: none
 * Returns: Error Code
 * Globals: SiI_Inf:sm_bVideo, bVResId modified
 *----------------------------------------------------------------------------
 */
/*******************************************************************************
 *-> SiI_RX_VS_VMDetected
 *	Set Video Dependent Video Path
 *		-> SiI_RX_VS_VideoOn
 *		-> SiI_RX_VS_SyncInChange
 *******************************************************************************
 */
static void SetResDepVideoPath(void)
{
	if (!(SiI_Inf.bGlobStatus & SiI_RX_GlobalHDMI_Detected)) {
		siiSetAnalogAudioMux(SiI_Ctrl.bVidInChannel);
		if (SiI_Inf.AVI.bInputColorSpace != SiI_RX_ICP_RGB) {
			SiI_Inf.AVI.bInputColorSpace = SiI_RX_ICP_RGB;
			siiSetVideoPathColorSpaceDependent(
				SiI_Ctrl.VideoF.bOutputVideoPath,
				SiI_Inf.AVI.bInputColorSpace);
				/* As color space has changed, it's need
				 *a call to config. Video Path non res.
				 *dependent Vid. Path
				 */
		}
	} else
		siiSetDigitalAudioMux();
	siiSetVidResDependentVidOutputFormat(SiI_Inf.bVResId,
		SiI_Ctrl.VideoF.bOutputVideoPath, SiI_Inf.AVI.bAVI_State);
	SiI_Ctrl.sm_bVideo = SiI_RX_VS_ReqVideoOn;
	SiI_Ctrl.wVideoTimeOut = 32;

}
/*------------------------------------------------------------------------------
 * Function Name:
 * Function Description:
 *----------------------------------------------------------------------------
 */
static void CheckIfResUnMuteVideoIfOK(void)
{
	struct SyncInfoType_s SyncInfo;

	/* in case if Pix Replication was changed
	 * need to check sync info wasn't changed
	 * res change interrupt is ignorred for this
	 * state of Video state machine
	 */
	siiGetSyncInfo(&SyncInfo);
	if (siiCompareNewOldSync(&SiI_Inf.Sync, &SyncInfo)) {
		siiMuteVideo(OFF);
		SiI_Ctrl.sm_bVideo = SiI_RX_VS_VideoOn;
		/* TBD: This User call should be removed -
		 * Needed for Oscar board only
		 */
		/* InitTX(); */
	} else
		siiMuteVideoAndSetSM_SyncInChange();
}
/*------------------------------------------------------------------------------
 * Function Name: siiSetGlobalVideoMuteAndSM_Video
 * Function Description:
 *----------------------------------------------------------------------------
 */

void siiSetGlobalVideoMuteAndSM_Video(BOOL qOn)
{
	if (qOn) {
		siiMuteVideo(ON);
		SiI_Ctrl.sm_bVideo = SiI_RX_VS_HoldVideoMute;
	} else {
		siiMuteVideo(OFF);
		SiI_Ctrl.sm_bVideo = SiI_RX_VS_VideoOn;
	}

}
/*------------------------------------------------------------------------------
 * Function Name:
 * Function Description:
 *----------------------------------------------------------------------------
 */

BYTE siiTTVideoHandler(BOOL qTimeOut)
{
	BYTE bECode = FALSE;

	RXEXTDBG("%s: sm_bVideo=%d\n", __func__, SiI_Ctrl.sm_bVideo);
	switch (SiI_Ctrl.sm_bVideo) {
	case SiI_RX_VS_ReqPowerDown:
	if (qTimeOut) {
		siiTurnAudioOffAndSetSM_AudioOff();
		siiSetPowerDownModeAndSetSM_PowerDown();
#ifdef SII_DUMP_UART
		printf("\nAudioOff\n");
		printf("PowerDown\n");
#endif
		}
		break;
	case SiI_RX_VS_PowerDown:
		if (qTimeOut)
			PowerDownProcessing();
		break;
	case SiI_RX_VS_SyncInChange:
		if (qTimeOut)
			SyncInChangeProcessing();
		break;
	case SiI_RX_VS_SyncStable:
#ifdef SII_BUG_PHOEBE_AUTOSW_BUG
		if ((SiI_Ctrl.bDevId == RX_SiI9023) ||
			(SiI_Ctrl.bDevId == RX_SiI9033)) {
			siiCheckTransitionToDVIModeAndResetHDMIInfoIfConfirmed
				();
		}
#endif
		CheckOutOfRangeConditions();
		break;
	case SiI_RX_VS_ReqVMDetection:
		VideoResolutionDetection();
		break;
	case SiI_RX_VS_VMDetected:
		SetResDepVideoPath();
		break;
	case SiI_RX_VS_ReqVideoOn:
		CheckIfResUnMuteVideoIfOK();
		break;
	case SiI_RX_VS_ReqVidInChange:
		if (qTimeOut)
			siiSetVideoInput(SiI_Ctrl.bVidInChannel);
		break;
	case SiI_RX_VS_ReqGlobalPowerDown:
		siiMuteVideo(ON);
		siiRX_DisableTMDSCores();
		siiRX_PowerDown(ON);
		SiI_Ctrl.sm_bVideo = SiI_RX_VS_GlobalPowerDown;
		break;
	case SiI_RX_VS_HoldVideoMute: /* do nothing */
		break;
	}
	return bECode;

}

