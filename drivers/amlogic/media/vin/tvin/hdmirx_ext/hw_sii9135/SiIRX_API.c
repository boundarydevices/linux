/*------------------------------------------------------------------------------
 * Module Name SiIRX_API
 * Module Description: RX API functions
 *
 * Copyright © 2002-2005, Silicon Image, Inc.  All rights reserved.
 *
 * No part of this work may be reproduced, modified, distributed, transmitted,
 * transcribed, or translated into any language or computer format, in any form
 * or by any means without written permission of: Silicon Image, Inc.,
 * 1060 East Arques Avenue, Sunnyvale, California 94085
 *----------------------------------------------------------------------------
 */
#include "SiIRX_API.h"
#include "SiIGlob.h"
#include "SiIVidF.h"
#include "SiIVidIn.h"
#include "SiIAudio.h"
#include "SiITTAudio.h"
#include "SiITTVideo.h"
#include "SiISysCtrl.h"
#include "SiITTasks.h"
#include "SiIHDMIRX.h"
#include "SiIInfoPkts.h"
#include "SiIDiagnostic.h"
#include "SiIISR.h"
#include "SiIHAL.h"
#include "SiITrace.h"

#include "../hdmirx_ext_drv.h"

/*------------------------------------------------------------------------------
 * System Commands
 *----------------------------------------------------------------------------
 */

/*------------------------------------------------------------------------------
 * Function Name:  SiI_RX_GetAPIInfo
 * ---
 * Function Description:  This function is used to get information about API
 *
 * Accepts: <description of input>
 * Returns: Number of Errors code and/or warnings
 * Globals: <global variables and system state changes>
 *----------------------------------------------------------------------------
 */
BYTE SiI_RX_GetAPI_Info(BYTE *pbRXParameters)
{
	WORD wStartSysTimerTicks;

	siiResetErrorsAndWarnings(); /* Number of Warning Messages = Zero */
	wStartSysTimerTicks = siiGetTicksNumber();

	pbRXParameters[0] = SiI_RX_API_Version;
	pbRXParameters[1] = SiI_RX_API_Revision;
	pbRXParameters[2] = SiI_RX_API_Build;
	pbRXParameters[3] = SiI_RX_API_DiagnosticCommands;
	siiGetRXDeviceInfo(&pbRXParameters[4]);
	pbRXParameters[7] = SII_REQ_TASK_CALL_TIME;

	siiMeasureProcLastAPI_Ticks(wStartSysTimerTicks);
	/* if Result FALSE, no errors and warning messages */
	return siiGetErrorsWarnings();

}

/*------------------------------------------------------------------------------
 * Function Name: SiI_RX_InitializeSystem
 * Function Description: Initialize global data and devices
 *
 * Accepts: none
 * Returns: Number of Errors code and/or warnings
 * Globals: none
 *----------------------------------------------------------------------------
 */
BYTE SiI_RX_InitializeSystem(BYTE *pbSysInit)
{
	WORD wStartSysTimerTicks;

	RXEXTPR("%s\n", __func__);
	siiResetErrorsAndWarnings(); /* Number of Warning Messages = Zero */
	wStartSysTimerTicks = siiGetTicksNumber();

	halInitGPIO_Pins();
	siiInitializeRX(pbSysInit);
	siiSaveRXInitParameters(pbSysInit);
	siiInitilizeSystemData(ON);
	siiTurnAudioOffAndSetSM_AudioOff();
	siiMuteVideoAndSetSM_SyncInChange();

	siiMeasureProcLastAPI_Ticks(wStartSysTimerTicks);
	/* if Result FALSE, no errors and warning messages */
	return siiGetErrorsWarnings();

}

/*------------------------------------------------------------------------------
 * Function Name: SiI_RX_GlobalPower
 * Function Description:
 *
 * Accepts: none
 * Returns: Number of Errors code and/or warnings
 * Globals: none
 *----------------------------------------------------------------------------
 */
BYTE SiI_RX_GlobalPower(BYTE bPowerState)
{
	WORD wStartSysTimerTicks;

	siiResetErrorsAndWarnings(); /* Number of Warning Messages = Zero */
	wStartSysTimerTicks = siiGetTicksNumber();

	siiRX_GlobalPower(bPowerState);

	siiMeasureProcLastAPI_Ticks(wStartSysTimerTicks);
	/* if Result FALSE, no errors and warning messages */
	return siiGetErrorsWarnings();
}

/*------------------------------------------------------------------------------
 * Function Name: SiI_RX_SetVideoInput
 * ---
 * Function Description:  Selects HDMI (DVI) input
 * Accepts: bVideoInputChannels
 * Returns: Number of Errors code and/or warnings
 * Globals: none
 *----------------------------------------------------------------------------
 */
BYTE SiI_RX_SetVideoInput(BYTE bVideoInputChannels)
{
	WORD wStartSysTimerTicks;

	siiResetErrorsAndWarnings(); /* Number of Warning Messages = Zero */
	wStartSysTimerTicks = siiGetTicksNumber();

	SiI_Ctrl.bVidInChannel = bVideoInputChannels; /* YMA added */

	siiChangeVideoInput(bVideoInputChannels);


	siiMeasureProcLastAPI_Ticks(wStartSysTimerTicks);
	return siiGetErrorsWarnings();
}

/*------------------------------------------------------------------------------
 * Function Name: SiI_RX_SetAudioMute
 * Function Description: This command provides a manual mute of
 *                       the Silicon Image HDMI receiver's audio output.
 *                       This command does not change the behavior of any
 *                       device's auto mute functionality
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
BYTE SiI_RX_SetAudioVideoMute(BYTE bAudioVideoMute)
{
	WORD wStartSysTimerTicks;

	siiResetErrorsAndWarnings(); /* Number of Warning Messages = Zero */
	wStartSysTimerTicks = siiGetTicksNumber();

	if (bAudioVideoMute & SiI_RX_AVMuteCtrl_MuteAudio)
		siiSetSM_ReqHoldAudioMuteAndStartMuting(ON);
	else if (siiIsAudioHoldMuteState())
		siiSetSM_ReqHoldAudioMuteAndStartMuting(OFF);

	if (bAudioVideoMute & SiI_RX_AVMuteCtrl_MuteVideo)
		siiSetGlobalVideoMuteAndSM_Video(ON);
	else
		siiSetGlobalVideoMuteAndSM_Video(OFF);


	siiMeasureProcLastAPI_Ticks(wStartSysTimerTicks);
	/* if Result FALSE, no errors and warning messages */
	return siiGetErrorsWarnings();
}

/*------------------------------------------------------------------------------
 * Function Name: SiI_RX_DoTasks
 * Function Description:
 *
 * Accepts: pointer on events changes
 * Returns: Number of Errors code and/or warnings
 * Globals: <global variables and system state changes>
 *----------------------------------------------------------------------------
 */
BYTE SiI_RX_DoTasks(BYTE *pbRXParameters)
{
	/* Slot time is time between last and current DoTasks measured in ms */
	WORD wNumberOfTicks;
	BYTE bTimeFromLastDoTasks;
	BYTE bError;

	siiResetErrorsAndWarnings(); /* Number of Warning Messages = Zero */
	wNumberOfTicks = siiGetTicksNumber();
	RXEXTDBG("sii9135 SiI_Ctrl.sm_bVideo = %d\n", SiI_Ctrl.sm_bVideo);
	if (SiI_Ctrl.sm_bVideo != SiI_RX_VS_GlobalPowerDown) {
		/* if (SiI_Inf.bGlobStatus & SiI_RX_Global_HotPlugDetect) { */
#ifdef SII_USE_RX_PIN_INTERRUPT
		bError = siiRX_PinInterruptHandler();
#else
		bError = siiRX_BitInterruptHandler();
#endif
		/* } */
		if (bError == SiI_EC_HDCP_StuckInterrupt)
			siiReInitRX();

#ifndef FIXED_TASK_CALL_TIME
		bTimeFromLastDoTasks = siiDoTasksTimeDiffrence();
#else
		bTimeFromLastDoTasks = SII_TASK_CALL_TIME;
#endif
		siiTimerTasksHandler(bTimeFromLastDoTasks);
		pbRXParameters[0] = siiGetSMEventChanges();
	}
	siiMeasureProcLastAPI_Ticks(wNumberOfTicks);

	/* if Result FALSE, no errors and warning messages */
	return siiGetErrorsWarnings();
}

/*------------------------------------------------------------------------------
 * Function Name: SiI_RX_GetSystemInformation
 * Function Description:
 *
 * Accepts: none
 * Returns: nNumber of Errors code and/or warnings
 * Globals: none
 *----------------------------------------------------------------------------
 */
BYTE SiI_RX_GetSystemInformation(BYTE *pbRXParameters)
{
	WORD wStartSysTimerTicks;

	siiResetErrorsAndWarnings(); /* Number of Warning Messages = Zero */
	wStartSysTimerTicks = siiGetTicksNumber();

	pbRXParameters[0] = SiI_Ctrl.sm_bVideo;    /* is BYTE bVideoState */
	pbRXParameters[1] = SiI_Ctrl.sm_bAudio;    /* is BYTE bAudioState */
	if (siiCheckIfVideoInputResolutionReady()) {
		/* is BYTE bModeId */
		pbRXParameters[2] = siiGetVideoResId(SiI_Inf.bVResId);
	} else {
		if (siiCheckIfVideoOutOfRangeOrVMNotDetected())
			pbRXParameters[2] = SiI_ResNotDetected;
		else
			pbRXParameters[2] = SiI_ResNoInfo;
	}
	/* is BYTE bInputColorSpace */
	pbRXParameters[3] = SiI_Inf.AVI.bInputColorSpace;
			/* is BYTE bColorimetry */
	pbRXParameters[4] = SiI_Inf.AVI.bColorimetry/* ; */
			/* is  upper 4 bits is Color Depth */
			| (SiI_Inf.AVI.bInputColorDepth << 4);
	/* is BYTE bHDCPStatus */
	pbRXParameters[5] = SiI_Inf.bHDCPStatus;
	/* is BYTE bGlobStatus */
	pbRXParameters[6] = SiI_Inf.bGlobStatus;
	pbRXParameters[7] = SiI_Inf.bNewInfoPkts;

	siiMeasureProcLastAPI_Ticks(wStartSysTimerTicks);
	/* if Result FALSE, no errors and warning messages */
	return siiGetErrorsWarnings();
}

/*------------------------------------------------------------------------------
 * Packet Commands
 *----------------------------------------------------------------------------
 */

/*------------------------------------------------------------------------------
 * Function Name:  SiI_RX_GetInfoFrame
 * Function Description:  Get the current Info Frame received by
 *                        the Silicon Image HDMI receiver.
 *
 * Accepts: bInfoFrameType
 * Returns: BYTE *pbRXParameters pointer on Info Frame data
 * Globals: none
 *----------------------------------------------------------------------------
 */

BYTE SiI_RX_GetPackets(BYTE bInfoPacketType, BYTE *pbRXParameters)
{
	WORD wStartSysTimerTicks;

	siiResetErrorsAndWarnings(); /* Number of Warning Messages = Zero */
	wStartSysTimerTicks = siiGetTicksNumber();

	siiGetInfoPacket(bInfoPacketType, pbRXParameters);
	siiClearNewPacketEvent(bInfoPacketType);

	siiMeasureProcLastAPI_Ticks(wStartSysTimerTicks);
	/* if Result FALSE, no errors and warning messages */
	return siiGetErrorsWarnings();
}

/*------------------------------------------------------------------------------
 * Video Commands
 *----------------------------------------------------------------------------
 */

/*------------------------------------------------------------------------------
 * Function Name: SiI_RX_SetVideoOutputFormat
 * ---
 * Function Description: Sets Video Output Format of SiI RX, function sets
 *                       static part of Video Path, dynamic part will be
 *                       complete after detection of video resolution
 * Accepts: parameters to set Video Output Format
 * Returns: Number of Errors code and/or warnings
 * Globals: sm_bVideo, bVideoPath, bInputColorSpace
 *----------------------------------------------------------------------------
 */

BYTE SiI_RX_SetVideoOutputFormat(BYTE  bOutputVideoPathSelect,
				BYTE  bOutputSyncSelect,
				BYTE  bOutputSyncCtrl,
				BYTE  bOutputVideoCtrl)
{
	WORD wStartSysTimerTicks;
	BYTE abVidFormatData[3];

	siiResetErrorsAndWarnings(); /* Number of Warning Messages = Zero */
	wStartSysTimerTicks = siiGetTicksNumber();

	abVidFormatData[0] = abVidFormatData[1] = abVidFormatData[2] = 0;
	/* YMA restore the configuration, or outpout depth lost */
	siiGetVideoFormatData(abVidFormatData);

	siiMuteVideoAndSetSM_SyncInChange();

	siiPrepVideoPathSelect(bOutputVideoPathSelect,
		SiI_Inf.AVI.bInputColorSpace, abVidFormatData);
	siiPrepSyncSelect(bOutputSyncSelect, abVidFormatData);
	siiPrepSyncCtrl(bOutputSyncCtrl, abVidFormatData);
	siiPrepVideoCtrl(bOutputVideoCtrl, abVidFormatData);
	/* Update Video format data to  Vid. Mode/Ctrl regs. */
	siiSetVideoFormatData(abVidFormatData);

	/* later will be used in Vid. Res. Dependent functions */
	SiI_Ctrl.VideoF.bOutputVideoPath = bOutputVideoPathSelect;
	SiI_Ctrl.VideoF.bOutputSyncSelect = bOutputSyncSelect;
	SiI_Ctrl.VideoF.bOutputSyncCtrl = bOutputSyncCtrl;
	SiI_Ctrl.VideoF.bOutputVideoCtrl = bOutputVideoCtrl;


	siiMeasureProcLastAPI_Ticks(wStartSysTimerTicks);
	/* if Result FALSE, no errors and warning messages */
	return siiGetErrorsWarnings();

}

/*------------------------------------------------------------------------------
 * Function Name: SiI_RX_GetVideoOutputFormat
 * Function Description: Gets Video Output Format
 *
 * Accepts: none
 * Returns: Number of Errors code and/or warnings
 * Globals: none
 *----------------------------------------------------------------------------
 */
BYTE SiI_RX_GetVideoOutputFormat(BYTE *pbRXParameters)
{
	WORD wStartSysTimerTicks;

	siiResetErrorsAndWarnings(); /* Number of Warning Messages = Zero */
	wStartSysTimerTicks = siiGetTicksNumber();

	/* later will be used in Vid. Res. Dependent functions */
	pbRXParameters[0] = SiI_Ctrl.VideoF.bOutputVideoPath;
	pbRXParameters[1] = SiI_Ctrl.VideoF.bOutputSyncSelect;
	pbRXParameters[2] = SiI_Ctrl.VideoF.bOutputSyncCtrl;
	pbRXParameters[3] = SiI_Ctrl.VideoF.bOutputVideoCtrl;

	siiMeasureProcLastAPI_Ticks(wStartSysTimerTicks);
	/* if Result FALSE, no errors and warning messages */
	return siiGetErrorsWarnings();
}

/*------------------------------------------------------------------------------
 * Function Name: SiI_RX_GetInputVideoResolution
 * Function Description:
 *
 * Accepts: none
 * Returns: Number of Errors code and/or warnings
 * Globals: none
 *----------------------------------------------------------------------------
 */
BYTE SiI_RX_GetVideoInputResolution(BYTE *pbRXParameters)
{
	WORD wStartSysTimerTicks;

	siiResetErrorsAndWarnings(); /* Number of Warning Messages = Zero */
	wStartSysTimerTicks = siiGetTicksNumber();
	if (siiCheckIfVideoInputResolutionReady())
		siiGetVideoInputResolution(SiI_Inf.bVResId, pbRXParameters);
	else
		pbRXParameters[0] = SiI_ResNoInfo;
#ifdef SII_NO_RESOLUTION_DETECTION
	if (siiCheckIfVideoOutOfRangeOrVMNotDetected())
		siiGetVideoInputResolutionromRegisters(pbRXParameters);
#endif
	siiMeasureProcLastAPI_Ticks(wStartSysTimerTicks);
	/* if Result FALSE, no errors and warning messages */
	return siiGetErrorsWarnings();
}

/*------------------------------------------------------------------------------
 * Audio Commands
 *----------------------------------------------------------------------------
 */

/*------------------------------------------------------------------------------
 * Function Name: SiI_RX_SetAudioOutputFormat
 * Function Description: Set the Silicon image HDMI receivers audio
 *                       output format.
 *                       Both audio interfaces, SPDIF and I2S can
 *                       be selected simultaneously.
 *                       The Smooth HW Mute can also be enabled regardless of
 *                       the audio interfaces combination selected.
 *
 *                       The BYTE bI2SMap is a reserved parameter in
 *                       the API specification.
 *                       It is being used to set up the I2S mapping,
 *                       the API does not support configurations other than
 *                       the default.
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
BYTE SiI_RX_SetAudioOutputFormat(WORD wAudioOutputSelect,
				WORD wI2SBusFormat,
				BYTE bI2SMap,
				BYTE bDSDHBRFormat)
{
	/* YMA 2 added for API rev2 */
	WORD wStartSysTimerTicks;
	struct AudioOutputFormatType_s AudioOutputFormat;

	siiResetErrorsAndWarnings(); /* Number of Warning Messages = Zero */
	wStartSysTimerTicks = siiGetTicksNumber();

	siiTurnAudioOffAndSetSM_AudioOff();

	AudioOutputFormat.wOutputSelect =
		SiI_Ctrl.wAudioOutputSelect = wAudioOutputSelect;
	AudioOutputFormat.wI2SBusFormat = wI2SBusFormat;
	AudioOutputFormat.bI2SMap = bI2SMap;
	AudioOutputFormat.bDSDHBRFormat = bDSDHBRFormat;
	siiSetAudioOutputFormat(&AudioOutputFormat);

	/* YMA 2 add to save the configuration for
	 *later restore when HBR/DSD -> PCM
	 */
	SavedAudioOutputFormat = AudioOutputFormat;
	siiMeasureProcLastAPI_Ticks(wStartSysTimerTicks);
	/* if Result FALSE, no errors and warning messages */
	return siiGetErrorsWarnings();
}

/*------------------------------------------------------------------------------
 * Function Name: SiI_RX_GetAudioOutputFormat
 * Function Description:
 *
 * Accepts: none
 * Returns: pbRXParameters[0] is BYTE bAudioOutputControl,
 *          pbRXParameters[1] is WORD wI2SBusFormat
 *          pbRXParameters[4] is BYTE bI2SMap
 *			pbRXParameters[5] is BYTE bDSDHBRFormat
 * Globals: none
 *----------------------------------------------------------------------------
 */
BYTE SiI_RX_GetAudioOutputFormat(BYTE *pbRXParameters)
{
	WORD wStartSysTimerTicks;
	/* struct AudioOutputFormatType_s AudioOutputFormat; */

	siiResetErrorsAndWarnings(); /* Number of Warning Messages = Zero */
	wStartSysTimerTicks = siiGetTicksNumber();
	/* YMA change to return the save value instead of read from registers.
	 *the reg value may be not the right mode
	 */

	/* siiGetAudioOutputFormat(&AudioOutputFormat); */

	pbRXParameters[0] = (BYTE)(SavedAudioOutputFormat.wOutputSelect & 0xFF);
	pbRXParameters[1] =
		(BYTE)((SavedAudioOutputFormat.wOutputSelect >> 8) & 0xFF);
	pbRXParameters[2] = (BYTE)(SavedAudioOutputFormat.wI2SBusFormat & 0xFF);
	pbRXParameters[3] =
		(BYTE)((SavedAudioOutputFormat.wI2SBusFormat >> 8) & 0xFF);
	pbRXParameters[4] = SavedAudioOutputFormat.bI2SMap;
	pbRXParameters[5] = SavedAudioOutputFormat.bDSDHBRFormat;

	siiMeasureProcLastAPI_Ticks(wStartSysTimerTicks);
	/* if Result FALSE, no errors and warning messages */
	return siiGetErrorsWarnings();
}

/*------------------------------------------------------------------------------
 * Function Name: SiI_RX_GetAudioInputStatus
 * Function Description:
 *
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
BYTE SiI_RX_GetAudioInputStatus(BYTE *pbRXParameters)
{
	WORD wStartSysTimerTicks;

	siiResetErrorsAndWarnings(); /* Number of Warning Messages = Zero */
	wStartSysTimerTicks = siiGetTicksNumber();
	pbRXParameters[0] = SiI_Inf.bAudioErr;
	if (siiIsAudioStatusReady()) {
		/* Compressed, PCM, DSD */
		pbRXParameters[1] =  SiI_Inf.AudioStatus.bRepresentation;
		pbRXParameters[2] =  SiI_Inf.AudioStatus.bAccuracyAndFs;
		pbRXParameters[3] =  SiI_Inf.AudioStatus.bLength;
		pbRXParameters[4] =  SiI_Inf.AudioStatus.bNumberChannels;
	} else {
		pbRXParameters[1] = 0xFF;
		pbRXParameters[2] = 0xFF;
		pbRXParameters[3] = 0xFF;
		pbRXParameters[4] = 0xFF;
	}
	siiMeasureProcLastAPI_Ticks(wStartSysTimerTicks);
	/* if Result FALSE, no errors and warning messages */
	return siiGetErrorsWarnings();
}


/*------------------------------------------------------------------------------
 * Common Commands
 *----------------------------------------------------------------------------
 */

/*------------------------------------------------------------------------------
 * Function Name:
 * Function Description:
 *
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
BYTE SiI_GetWarnings(BYTE *pbRXParameters)
{
	WORD wStartSysTimerTicks;
	BYTE bNumberWrn;

	wStartSysTimerTicks = siiGetTicksNumber();

	bNumberWrn = siiGetWarningData(pbRXParameters);

	siiMeasureProcLastAPI_Ticks(wStartSysTimerTicks);
	/* if Result FALSE, no errors and warning messages */
	return bNumberWrn;

}

/*------------------------------------------------------------------------------
 * Function Name:
 * Function Description:
 *
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
BYTE SiI_GetErrors(BYTE *pbRXParameters)
{
	WORD wStartSysTimerTicks;
	BYTE bNumberErr;

	wStartSysTimerTicks = siiGetTicksNumber();

	bNumberErr = siiGetErrorsData(pbRXParameters);

	siiMeasureProcLastAPI_Ticks(wStartSysTimerTicks);
	/* if Result FALSE, no errors and warning messages */
	return bNumberErr;
}

/*------------------------------------------------------------------------------
 * Diagnostics Commands
 *----------------------------------------------------------------------------
 */

/*------------------------------------------------------------------------------
 * Function Name: SiI_Diagnostic_GetNCTS
 * Function Description:   Get information about N/CTS packets, these packets
 *                         are sent by TX and used for Audio Clock Regeneration
 *                         (ACR)
 * Accepts: none
 * Returns: pointer on requested data
 * Globals: none
 *----------------------------------------------------------------------------
 */
BYTE SiI_Diagnostic_GetNCTS(BYTE *pbRXParameters)
{
	siiResetErrorsAndWarnings(); /* Number of Warning Messages = Zero */
	siiGetNCTS(pbRXParameters);
	/* if Result FALSE, no errors and warning messages */
	return siiGetErrorsWarnings();
}

/*------------------------------------------------------------------------------
 * Function Name: SiI_Diagnostic_GetABKSV
 * Function Description:   Get AKSV and BKSV
 *                        (for more information see HDCP specefication)
 *
 * Accepts: none
 * Returns: pointer on requested data
 * Globals: none
 *----------------------------------------------------------------------------
 */
BYTE SiI_Diagnostic_GetABKSV(BYTE *pbRXParameters)
{
	siiResetErrorsAndWarnings(); /* Number of Warning Messages = Zero */
	siiGetABKSV(pbRXParameters);
	/* if Result FALSE, no errors and warning messages */
	return siiGetErrorsWarnings();
}

/*------------------------------------------------------------------------------
 * Function Name:
 * Function Description:
 *
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
BYTE SiI_Diagnostic_GetAPIExeTime(BYTE *pbRXParameters)
{
	siiResetErrorsAndWarnings(); /* Number of Warning Messages = Zero */
	siiDiagnostic_GetAPI_ExeTime(pbRXParameters);
	/* if Result FALSE, no errors and warning messages */
	return siiGetErrorsWarnings();
}

