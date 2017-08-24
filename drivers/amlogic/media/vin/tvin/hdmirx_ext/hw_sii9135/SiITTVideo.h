/*------------------------------------------------------------------------------
 * Copyright © 2002-2005, Silicon Image, Inc.  All rights reserved.
 *
 * No part of this work may be reproduced, modified, distributed, transmitted,
 * transcribed, or translated into any language or computer format, in any form
 * or by any means without written permission of: Silicon Image, Inc.,
 * 1060 East Arques Avenue, Sunnyvale, California 94085
 *----------------------------------------------------------------------------
 */


#ifndef _SII_TTVIDEO_
#define _SII_TTVIDEO_
#include "SiITypeDefs.h"

#define SYNC_CONFIRM_THRESHOLD 3

BYTE siiTTVideoHandler(BOOL qTimeOut);
void siiMuteVideoAndSetSM_SyncInChange(void);
BOOL siiSMCheckReqVideoOn(void);
BYTE siiGetPixClock(void);
BOOL siiCheckIfVideoInputResolutionReady(void);
BOOL siiCheckIfVideoOutOfRangeOrVMNotDetected(void);
void siiSetSM_ReqVidInChange(void);
void siiSetSM_ReqGlobalPowerDown(void);
BOOL siiIsVideoOn(void);

void siiSetGlobalVideoMuteAndSM_Video(BOOL qOn);
#endif

