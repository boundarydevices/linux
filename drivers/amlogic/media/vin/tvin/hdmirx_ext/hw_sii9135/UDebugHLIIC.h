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
/*#include "UCom.h"*/
#include "SiIHLVIIC.h"

#ifndef _SIIDEBUGHLIIC_
#define _SIIDEBUGHLIIC_

#define _BIGEEPROM_
#define IIC_CAPTURED  1
#define IIC_NOACK     2
#define MDDC_CAPTURED 3
#define MDDC_NOACK    4
#define MDDC_FIFO_FULL  5
#define IIC_OK 0

#define BUS_1 1
#define BUS_0 0

#define EE_SLVADDR_1 0xA0
#define EE_SLVADDR_2 0xA8

#define RX_SLV0 0x60
#define RX_SLV1 0x68
#define RX_AFE0 0x64
#define RX_AFE1 0x6C

#define SET 1
#define CLR 0

/*#define _BLOCK_16BA*/
/*#define _BLOCK_8BA*/

#ifdef _BLOCK_16BA
BYTE BlockWrite_16BA(I2CCommandType *I2CCommand);
BYTE BlockRead_16BA(I2CCommandType *I2CCommand);
BYTE BlockRead_16BAS(struct I2CShortCommandType_s *IIC, BYTE *Data);
BYTE BlockWrite_16BAS(struct I2CShortCommandType_s *IIC, BYTE *Data);
#endif
WORD ReadWord_16BA(BYTE SlaveAddr, WORD RegAddr);
BYTE ReadByte_16BA(BYTE SlaveAddr, WORD RegAddr);
void WriteByte_16BA(BYTE SlaveAddr, WORD RegAddr, BYTE Data);
void WriteWord_16BA(BYTE SlaveAddr, WORD RegAddr, WORD Data);

#ifdef _BLOCK_8BA
BYTE BlockRead_8BA(I2CCommandType *I2CCommand);
BYTE BlockWrite_8BA(I2CCommandType *I2CCommand);
BYTE DoRecoverySCLs(void);
#endif


#endif

