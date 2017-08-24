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
/* #include "UCom.h" */
#ifndef _HLVIIC_
#define _HLVIIC_

#define IIC_CAPTURED  1
#define IIC_NOACK     2
#define MDDC_CAPTURED 3
#define MDDC_NOACK    4
#define MDDC_FIFO_FULL  5
#define IIC_OK 0
#define IIC_ERROR 6

#define RX_SLV0 0x60
#define RX_SLV1 0x68
#define RX_AFE0 0x64
#define RX_AFE1 0x6C

#define SET 1
#define CLR 0

#define MDDC_BUS 2



struct I2CShortCommandType_s {

	BYTE SlaveAddr;
	BYTE Flags;
	BYTE NBytes;
	BYTE RegAddrL;
	BYTE RegAddrH;

};


BYTE hlBlockRead_8BAS(struct I2CShortCommandType_s *IIC, BYTE *Data);
BYTE hlBlockWrite_8BAS(struct I2CShortCommandType_s *IIC, BYTE *Data);
BYTE hlReadByte_8BA(BYTE SlaveAddr, BYTE RegAddr);
WORD hlReadWord_8BA(BYTE SlaveAddr, BYTE RegAddr);
void hlWriteWord_8BA(BYTE SlaveAddr, BYTE RegAddr, WORD Data);
void hlWriteByte_8BA(BYTE SlaveAddr, BYTE RegAddr, BYTE Data);
BOOL hlWaitForAck(BYTE bSlvAddr, WORD wTimeOut);
BYTE hlWriteAddr_8BA(BYTE bdata1, BYTE bdata2);




#endif

