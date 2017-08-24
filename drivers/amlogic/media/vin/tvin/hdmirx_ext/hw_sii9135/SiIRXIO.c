/*------------------------------------------------------------------------------
 * Module Name: SiIRXIO
 *
 * Module Description:  Reading and writing in SiI RX
 *
 * Copyright © 2002-2005, Silicon Image, Inc.  All rights reserved.
 *
 * No part of this work may be reproduced, modified, distributed, transmitted,
 * transcribed, or translated into any language or computer format, in any form
 * or by any means without written permission of: Silicon Image, Inc.,
 * 1060 East Arques Avenue, Sunnyvale, California 94085
 *----------------------------------------------------------------------------
 */

#include "SiIRXIO.h"
/*------------------------------------------------------------------------------
 * Function Name:  siiReadBlockHDMIRXP0
 * Function Description: Reads block of data from HDMI RX (page 0)
 *
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiReadBlockHDMIRXP0(BYTE bAddr, BYTE bNBytes, BYTE *pbData)
{
	struct I2CShortCommandType_s I2CComm;

	I2CComm.SlaveAddr = RX_SLV0;
	I2CComm.Flags = 0;
	I2CComm.NBytes = bNBytes;
	I2CComm.RegAddrL = bAddr;
	hlBlockRead_8BAS((struct I2CShortCommandType_s *)&I2CComm, pbData);
}
/*------------------------------------------------------------------------------
 * Function Name: siiReadBlockHDMIRXP1
 * Function Description: Reads block of data from HDMI RX (page 1)
 *
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiReadBlockHDMIRXP1(BYTE bAddr, BYTE bNBytes, BYTE *pbData)
{
	struct I2CShortCommandType_s I2CComm;

	I2CComm.SlaveAddr = RX_SLV1;
	I2CComm.Flags = 0;
	I2CComm.NBytes = bNBytes;
	I2CComm.RegAddrL = bAddr;
	hlBlockRead_8BAS((struct I2CShortCommandType_s *)&I2CComm, pbData);
}

/*------------------------------------------------------------------------------
 * Function Name:  siiWriteBlockHDMIRXP0
 * Function Description: Writes block of data from HDMI RX (page 0)
 *
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiWriteBlockHDMIRXP0(BYTE bAddr, BYTE bNBytes, BYTE *pbData)
{
	struct I2CShortCommandType_s I2CComm;

	I2CComm.SlaveAddr = RX_SLV0;
	I2CComm.Flags = 0;
	I2CComm.NBytes = bNBytes;
	I2CComm.RegAddrL = bAddr;
	hlBlockWrite_8BAS((struct I2CShortCommandType_s *)&I2CComm, pbData);
}

/*------------------------------------------------------------------------------
 * Function Name: siiModifyBits
 * Function Description:  this function reads byte from i2c device, modifys bit
 *                        in the byte, then writes it back
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiModifyBits(BYTE bSlaveAddr, BYTE bRegAddr, BYTE bModVal, BOOL qSet)
{
	BYTE bRegVal;

	bRegVal = hlReadByte_8BA(bSlaveAddr, bRegAddr);
	if (qSet)
		bRegVal |= bModVal;
	else
		bRegVal &= (~bModVal);
	hlWriteByte_8BA(bSlaveAddr, bRegAddr, bRegVal);
}

/*------------------------------------------------------------------------------
 * Function Name: siiReadModWriteByte
 * Function Description: Reads byte at bRegAddr from i2c device at bSlaveAddr
 *			  Clears any bits set in bBitmask
 *			  Sets any bits set in bNewValue
 *			  Writes byte back to bRegAddr i2c device at bSlaveAddr
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiReadModWriteByte(BYTE bSlaveAddr, BYTE bRegAddr,
		BYTE bBitMask, BYTE bNewValue)
{

	/*BYTE bRegVal;
	 *
	 *	bRegVal = hlReadByte_8BA ( bSlaveAddr, bRegAddr );
	 *	bRegVal &= ~bBitMask;
	 *	bRegVal |= bNewValue;
	 *	hlWriteByte_8BA ( bSlaveAddr, bRegAddr, bRegVal );
	 */

	hlWriteByte_8BA(bSlaveAddr, bRegAddr,
		(hlReadByte_8BA(bSlaveAddr, bRegAddr) & ~bBitMask) | bNewValue);
}

/*------------------------------------------------------------------------------
 * Function Name:  siiWriteBlockHDMIRXP1
 * Function Description: Writes block of data from HDMI RX (page 1)
 *
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
#ifdef _UNUSED_FUNC_
void siiWriteBlockHDMIRXP1(BYTE bAddr, BYTE bNBytes, BYTE *pbData)
{
	struct I2CShortCommandType_s I2CComm;

	I2CComm.Bus = BUS_1;
	I2CComm.SlaveAddr = RX_SLV1;
	I2CComm.Flags = 0;
	I2CComm.NBytes = bNBytes;
	I2CComm.RegAddrL = bAddr;
	hlBlockWrite_8BAS((struct I2CShortCommandType_s *)&I2CComm, pbData);
}
#endif /* end _UNUSED_FUNC_ */

