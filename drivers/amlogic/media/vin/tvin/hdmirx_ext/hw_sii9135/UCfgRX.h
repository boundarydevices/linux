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
#include "UGlob.h"


#define SII_EEPROM_ID_ADDR 0x00

#define RX_API_ID_L 0x20
#define RX_API_ID_H 0x93

#define SII_RX_INIT_SYS_ADDR    0x10
#define SII_RX_VIDEO_INPUT      0x18
#define SII_RX_VIDEO_OUTPUT_F   0x20
#define SII_RX_AUDIO_OUTPUT_F   0x30

BYTE siiGetPCB_Id(void);
BYTE siiRXConfig(void);


