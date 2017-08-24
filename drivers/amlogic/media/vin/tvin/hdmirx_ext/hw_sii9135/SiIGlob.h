/*------------------------------------------------------------------------------
 * Copyright © 2002-2005, Silicon Image, Inc.  All rights reserved.
 *
 * No part of this work may be reproduced, modified, distributed, transmitted,
 * transcribed, or translated into any language or computer format, in any form
 * or by any means without written permission of: Silicon Image, Inc.,
 * 1060 East Arques Avenue, Sunnyvale, California 94085
 *----------------------------------------------------------------------------
 */
#ifndef _SII_GLOB_
#define _SII_GLOB_

#include "SiITypeDefs.h"
#include "SiIVidRes.h"

#define SII_ADAC_PD_DELAY 0

/* RX API Information */
enum SiI_RX_API_Info {
	SiI_RX_API_Version = 0x01,
	SiI_RX_API_Revision = 0x02,/* increased */
	SiI_RX_API_Build = 0x3,/* YMA 3 */
	SiI_RX_API_DiagnosticCommands = TRUE,
};

struct SysTimerType_s {
	WORD wProcLastAPI_Ticks;
	WORD wProcLastDoTasks_Ticks;
};

struct VideoFormatType_s {
	BYTE bOutputVideoPath;
	BYTE bOutputSyncSelect;
	BYTE bOutputSyncCtrl;
	BYTE bOutputVideoCtrl;
};
/*
 *typedef struct {
 *	BYTE bInputColorDepth;
 *	BYTE bOutputColorDepth;
 *} DeepColorType;
 */
/* move to AVIType (sync as Vlad's new API code) */

struct SiI_CtrlType_s {
	BYTE sm_bVideo;
	BYTE sm_bAudio;
	BYTE bDevId;
	BYTE bRXInitPrm0;
	BYTE bVidInChannel;
	WORD wAudioTimeOut;
	WORD wVideoTimeOut;
	WORD bInfoFrameTimeOut;
	BYTE bVideoPath;
	WORD wAudioOutputSelect;
	BYTE bShadowPixRepl;
	BYTE bIgnoreIntr;
	BYTE bHDCPFailFrmCnt;
	BYTE bHDCPStuckConfirmCnt;
	struct VideoFormatType_s VideoF;
	struct SysTimerType_s SysTimer;
	/* DeepColorType DC_Info;
	 * move to AVIType (sync as Vlad's new API code)
	 */
};

struct ErrorType_s {
	BYTE bAudio;
	BYTE bIIC;
};

struct AudioStatusType_s {
	BYTE bRepresentation;         /* Compressed, PCM, DSD */
	BYTE bAccuracyAndFs;
	BYTE bLength;
	BYTE bNumberChannels;
};

struct AVIType_s {
	BYTE bAVI_State;
	BYTE bInputColorSpace;
	BYTE bColorimetry;
	BYTE bPixRepl;
	BYTE bInputColorDepth;  /* deep color API support */
	BYTE bOutputColorDepth; /* Output color depth API support */
};

struct SiI_InfoType_s {
	BYTE bVResId;
	struct AVIType_s AVI;
	BYTE bGlobStatus;
	BYTE bHDCPStatus;
	BYTE bNewInfoPkts;
	struct AudioStatusType_s AudioStatus;
	struct SyncInfoType_s Sync;
	BYTE bAudioErr;
	BYTE bIIC_Err;
};

extern IRAM BYTE SiI_bWCode[WRN_BUF_SIZE];
extern IRAM BYTE SiI_bECode[ERR_BUF_SIZE];

extern IRAM struct SiI_InfoType_s SiI_Inf;
extern IRAM struct SiI_CtrlType_s SiI_Ctrl;


#endif /* end _SII_GLOB_ */

