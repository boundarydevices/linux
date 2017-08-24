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

struct CTSLimitsType_s {
	WORD Min;
	WORD Max;
};

struct AudioOutputFormatType_s {
	WORD wOutputSelect;
	WORD wI2SBusFormat;
	BYTE bI2SMap;
	BYTE bDSDHBRFormat;
};

extern struct AudioOutputFormatType_s SavedAudioOutputFormat;
#define siiSetAudioMuteEvent() siiAudioFIFO_Reset()
#define GetFIFO_DiffPointer() siiReadByteHDMIRXP1(RX_FIFO_DIFF_ADDR)

void siiAudioFIFO_Reset(void);
BOOL siiCheckAudio_IfOK_InitACR(void);
BOOL siiPrepareTurningAudioOn(void);
void siiTurningAudio(BOOL qOn);
void siiSetAnalogAudioMux(BYTE bChannel);
void siiSetDigitalAudioMux(void);
void siiClearGotCTSAudioPacketsIterrupts(void);
void siiClearCTSChangeInterruprt(void);
void siiSetAudioOutputFormat(struct AudioOutputFormatType_s *AudioOutputFormat);
void siiGetAudioOutputFormat(struct AudioOutputFormatType_s *AudioOutputFormat);
void siiSaveInputAudioStatus(void);
BOOL siiIsAudioStatusReady(void);
void siiSetAutoFIFOReset(BOOL qOn);
void siiAudioMute(BOOL qOn);

void siiChangeDSDAudioStreamHandler(void);
void siiChangeHBRAudioStreamHandler(void);
