/*------------------------------------------------------------------------------
 * Module Name: UEEPROM
 *
 * Module Description:  Reading/writing data from EEPROM
 *
 * Copyright © 2002-2005, Silicon Image, Inc.  All rights reserved.
 *
 * No part of this work may be reproduced, modified, distributed, transmitted,
 * transcribed, or translated into any language or computer format, in any form
 * or by any means without written permission of: Silicon Image, Inc.,
 * 1060 East Arques Avenue, Sunnyvale, California 94085
 *----------------------------------------------------------------------------
 */
#include "UEEPROM.h"
#include "SiIHLVIIC.h"

#define EE_SLVADDR_1 0xA0
#define EE_SLVADDR_2 0xA8

static BYTE bEEPROMSlaveAddr;

/*------------------------------------------------------------------------------
 * Function Name: siiFindEEPROM
 * Function Description: Trys to find EEPROM at various slave addresses
 *
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
BYTE siiFindEEPROM(void)
{
	BYTE bError = FALSE;

	if (hlWaitForAck(EE_SLVADDR_1, 100))
		bEEPROMSlaveAddr = EE_SLVADDR_1;
	else if (hlWaitForAck(EE_SLVADDR_2, 100))
		bEEPROMSlaveAddr = EE_SLVADDR_2;
	else
		bError = TRUE;

	return bError;
}
/*------------------------------------------------------------------------------
 * Function Name: siiBlockReadEEPROM
 * Function Description: Reads block of Data from EEPROM
 *
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
BYTE siiBlockReadEEPROM(WORD Addr, BYTE NBytes, BYTE *Data)
{
	BYTE bError;
	struct I2CShortCommandType_s I2CComm;

	I2CComm.SlaveAddr = bEEPROMSlaveAddr;
	I2CComm.Flags = 0;
	I2CComm.NBytes = NBytes;
	I2CComm.RegAddrL = Addr & 0xFF;
	I2CComm.RegAddrH = Addr >> 8;
#ifdef _BIGEEPROM_
	bError = BlockRead_16BAS((struct I2CShortCommandType_s *)&I2CComm,
			Data);
#else
	bError = hlBlockRead_8BAS((struct I2CShortCommandType_s *)&I2CComm,
			Data);
#endif
	return bError;
}
/*------------------------------------------------------------------------------
 * Function Name: siiBlockWriteEEPROM
 * Function Description: Write block of DATA into EEPROM
 *
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */

void siiBlockWriteEEPROM(WORD Addr, BYTE NBytes, BYTE *Data)
{
	struct I2CShortCommandType_s I2CComm;

	I2CComm.SlaveAddr = bEEPROMSlaveAddr;
	I2CComm.Flags = 0;
	I2CComm.NBytes = NBytes;
	I2CComm.RegAddrL = Addr & 0xFF;
	I2CComm.RegAddrH = Addr >> 8;
#ifdef _BIGEEPROM_
	BlockWrite_16BAS((struct I2CShortCommandType_s *)&I2CComm, Data);
#else
	hlBlockWrite_8BAS((struct I2CShortCommandType_s *)&I2CComm, Data);
#endif

	/*hlWaitForAck(bEEPROMSlaveAddr, 100);*/
}
/*----------------------------------------------------------------------*/

