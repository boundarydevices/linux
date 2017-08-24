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

#ifdef _8051_
#include "UMCUDefs.h"
#endif

#if 1

#define halAudioSetA()
#define halAudioSetB()
#define halAudioClearA()
#define halAudioClearB()
#define halAudioSetAltA()
#define halAudioSetAltB()
#define halAudioClearAltA()
#define halAudioClearAltB()

#define halClearHardwareAudioMute()
#define halSetHardwareAudioMute()
#define halWakeUpAudioDAC()
#define halPowerDownAudioDAC()

#define halReadRXInt_Pin() 0
#define halGetHPD1Pin() 0
#define halGetHPD2Pin() 0
#define halSetHPD1Pin()
#define halSetHPD2Pin()
#define halClearHPD1Pin()
#define halClearHPD2Pin()

#define halSetSDAPin()
#define halSetSCLPin()
#define halClearSDAPin()
#define halClearSCLPin()
#define halGetSDAPin() 0
#define halGetSCLPin() 0

#define halAssertResetHDMIRXPin()
#define halReleaseResetHDMIRXPin()

#elif defined(_8051_)

#define halAudioSetA()                 (AudioSel0 = 1)
#define halAudioSetB()                 (AudioSel1 = 1)
#define halAudioClearA()               (AudioSel0 = 0)
#define halAudioClearB()               (AudioSel1 = 0)

#define halAudioSetAltA()              (AudioSelAlt0 = 1)
#define halAudioSetAltB()              (AudioSelAlt1 = 1)
#define halAudioClearAltA()            (AudioSelAlt0 = 0)
#define halAudioClearAltB()            (AudioSelAlt1 = 0)

#define halClearHardwareAudioMute()    (MUTE = 0)
#define halSetHardwareAudioMute()      (MUTE = 1)

#define halWakeUpAudioDAC()            (PDN = 1)
#define halPowerDownAudioDAC()         (PDN = 0)

#define halAssertResetHDMIRXPin()      (Reset_HDMIRX = 0)
#define halReleaseResetHDMIRXPin()     (Reset_HDMIRX = 1)

#define halReadRXInt_Pin()             (RXInt_Pin)
#define halGetHPD1Pin()                (HPCh1Ctrl_Pin)
#define halGetHPD2Pin()                (HPCh2Ctrl_Pin)
#define halSetHPD1Pin()                (HPCh1Ctrl_Pin = 1)
#define halSetHPD2Pin()                (HPCh2Ctrl_Pin = 1)
#define halClearHPD1Pin()              (HPCh1Ctrl_Pin = 0)
#define halClearHPD2Pin()              (HPCh2Ctrl_Pin = 0)

#define halSetSDAPin()     (SDA = 1)
#define halSetSCLPin()     (SCL = 1)
#define halClearSDAPin()   (SDA = 0)
#define halClearSCLPin()   (SCL = 0)
#define halGetSDAPin()     SDA
#define halGetSCLPin()     SCL

#define halGetPortSel()					PSEL_Pin

#endif

void halInitGPIO_Pins(void);
void halDelayMS(BYTE MS);
WORD siiGetTicksNumber(void);

void SysTickTimerISR(void);


