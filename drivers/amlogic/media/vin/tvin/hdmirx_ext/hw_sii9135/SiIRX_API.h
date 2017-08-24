/*------------------------------------------------------------------------------
 * Copyright © 2002-2005, Silicon Image, Inc.  All rights reserved.
 *
 * No part of this work may be reproduced, modified, distributed, transmitted,
 * transcribed, or translated into any language or computer format, in any form
 * or by any means without written permission of: Silicon Image, Inc.,
 * 1060 East Arques Avenue, Sunnyvale, California 94085
 *----------------------------------------------------------------------------
 */

#include "SiITypeDefs.h"
#include "SiIRXAPIDefs.h"
#include "SiIGlob.h"

/*------------------------------------------------------------------------------
 * System Commands
 *----------------------------------------------------------------------------
 */
BYTE SiI_RX_GetAPI_Info(BYTE *pbRXParameters);
BYTE SiI_RX_InitializeSystem(BYTE *pbSysInit);
BYTE SiI_RX_GlobalPower(BYTE bPowerState);
BYTE SiI_RX_SetVideoInput(BYTE bVideoInputChannels);
BYTE SiI_RX_SetAudioVideoMute(BYTE bAudioVideoMute);
BYTE SiI_RX_DoTasks(BYTE *pbRXParameters);
BYTE SiI_RX_GetSystemInformation(BYTE *pbRXParameters);

/*------------------------------------------------------------------------------
 * Packet Commands
 *----------------------------------------------------------------------------
 */
BYTE SiI_RX_GetPackets(BYTE bInfoPacketType, BYTE *pbRXParameters);

/*------------------------------------------------------------------------------
 * Video Commands
 *----------------------------------------------------------------------------
 */
BYTE SiI_RX_SetVideoOutputFormat(BYTE  bOutputVideoPathSelect,
		BYTE  bOutputSyncSelect, BYTE  bOutputSyncCtrl,
		BYTE  bOutputVideoCtrl);
BYTE SiI_RX_GetVideoOutputFormat(BYTE *pbRXParameters);
BYTE SiI_RX_GetVideoInputResolution(BYTE *pbRXParameters);

/*------------------------------------------------------------------------------
 * Audio Commands
 *----------------------------------------------------------------------------
 */
BYTE SiI_RX_SetAudioOutputFormat(WORD wAudioOutputSelect, WORD wI2SBusFormat,
		BYTE bI2SMap, BYTE bDSDHBRFormat);
BYTE SiI_RX_GetAudioOutputFormat(BYTE *pbRXParameters);
BYTE SiI_RX_GetAudioInputStatus(BYTE *pbRXParameters);

/*------------------------------------------------------------------------------
 * Common Commands
 *----------------------------------------------------------------------------
 */
BYTE SiI_GetWarnings(BYTE *pbRXParameters);
BYTE SiI_GetErrors(BYTE *pbRXParameters);

/*------------------------------------------------------------------------------
 * Diagnostics Commands
 *----------------------------------------------------------------------------
 */
BYTE SiI_Diagnostic_GetNCTS(BYTE *pbRXParameters);
BYTE SiI_Diagnostic_GetABKSV(BYTE *pbRXParameters);
BYTE SiI_Diagnostic_GetAPIExeTime(BYTE *pbRXParameters);

/* Command has been removed from API 1.0 specification
 *BYTE SiI_RX_GetTasksSchedule(BYTE *bdata);
 */



