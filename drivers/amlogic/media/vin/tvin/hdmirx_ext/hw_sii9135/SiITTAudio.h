/*------------------------------------------------------------------------------
 * Copyright © 2002-2005, Silicon Image, Inc.  All rights reserved.
 *
 * No part of this work may be reproduced, modified, distributed, transmitted,
 * transcribed, or translated into any language or computer format, in any form
 * or by any means without written permission of: Silicon Image, Inc.,
 * 1060 East Arques Avenue, Sunnyvale, California 94085
 *----------------------------------------------------------------------------
 */

#ifndef _SII_TTAUDIO_
#define _SII_TTAUDIO_
#include "SiITypeDefs.h"



BYTE siiTTAudioHandler(BOOL qTimeOut);
void siiSetSM_ReqAudio(void);
void siiTurnAudioOffAndSetSM_AudioOff(void);
void siiSetGlobalAudioMuteAndSM_Audio(BOOL qOn);
BOOL siiIsAudioHoldMuteState(void);
/* YMA fix by added audio mute API */
void siiSetSM_ReqHoldAudioMuteAndStartMuting(BOOL qOn);


#endif


