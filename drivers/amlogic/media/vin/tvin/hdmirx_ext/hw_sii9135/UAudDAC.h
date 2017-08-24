/******************************************************************************/
/*  Copyright (c) 2002-2005, Silicon Image, Inc.  All rights reserved.        */
/*  No part of this work may be reproduced, modified, distributed,            */
/*  transmitted, transcribed, or translated into any language or computer     */
/*  format, in any form or by any means without written permission of:        */
/*  Silicon Image, Inc., 1060 East Arques Avenue, Sunnyvale, California 94085 */
/******************************************************************************/
#ifndef _SII_AUDDAC_
#define _SII_AUDDAC_

#include "SiITypeDefs.h"
#include "SiIHLVIIC.h"

#define siiWriteByteAudDAC(ADDR, DATA) hlWriteByte_8BA(0x26, ADDR, DATA)

#define AUDDAC_CTRL1_ADDR    0x00
#define AUDDAC_CTRL2_ADDR    0x01
#define AUDDAC_SPEED_PD_ADDR 0x02
#define AUDDAC_CTRL3_ADDR    0x0A

#define AUDDAC_RST 0x00
#define AUDDAC_NORM_OP 0x4F

#define ADAC_DSD_MODE 0x18
#define ADAC_PCM_MODE 0x00


void WakeUpAudioDAC(void);
void PowerDownAudioDAC(void);
void halSetAudioDACMode(BYTE bMode);

#endif

