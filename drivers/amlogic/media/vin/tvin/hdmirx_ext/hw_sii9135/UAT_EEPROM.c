/*------------------------------------------------------------------------------
 * Module Name: AT_EEPROM
 *
 * Module Description:  this low level driver for reading/writing
 *                      Atmel AT89C51ID2 EEPROM
 *
 * Copyright © 2005 Silicon Image, Inc.
 * All rights reserved.
 *----------------------------------------------------------------------------
 */
#include "UAT_EEPROM.h"

/*------------------------------------------------------------------------------
 * Function Name: siiReadByteInternEEPROM
 * Function Description:  This function reads one byte of Internal EEPROM of
 *                        AT89C51ID2 Internal EEPROM uses XDATA adressing of
 *                        8051, when EEPROM XDATA is selected RAM XDATA is not
 *                        accessible. Input parameters are should be converted
 *                        to DATA segment before switching to EEPROM XDATA
 *                        segment
 * Accepts: wAddr, address of EEPROM location
 * Returns: pointer on byte which was received form EEPROM
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiReadByteInternEEPROM(WORD wAddr, BYTE *pbData)
{
#ifdef USE_InternEEPROM
	BYTE *abEEPROM;
	WORD wDAddr;
	BYTE bDData;

	wDAddr = wAddr;

	halDisableMCUInterrupts();
	halIntEEPROM_Enable();

	abEEPROM = (BYTE *)wAddr;

	while (halIsIntEEPROM_Busy())
		;

	bDData = *abEEPROM;

	halIntEEPROM_Disable();
	halEnableMCUInterrupts();
	*pbData = bDData;
#endif
}
/*------------------------------------------------------------------------------
 * Function Name: siiWriteByteInternEEPROM
 * Function Description:  This function writes one byte of Internal EEPROM of
 *                        AT89C51ID2 Internal EEPROM uses XDATA adressing of
 *                        8051, when EEPROM XDATA is selected RAM XDATA is not
 *                        accessible. Input parameters are should be converted
 *                        to DATA segment before switching to EEPROM XDATA
 *                        segment
 * Accepts: wAddr, address of EEPROM location
 *                 pointer on byte which will be written into EEPROM
 * Returns:  none
 * Globals: none
 *----------------------------------------------------------------------------
 */

void siiWriteByteInternEEPROM(WORD wAddr, BYTE bData)
{
#ifdef USE_InternEEPROM
	BYTE xdata *abEEPROM;

	WORD data wDAddr;
	BYTE bDData;

	wDAddr = wAddr;
	bDData = bData;

	halDisableMCUInterrupts();
	halIntEEPROM_Enable();

	abEEPROM = wAddr;

	while (halIsIntEEPROM_Busy())
		;

	*abEEPROM = bDData;

	halIntEEPROM_Disable();
	halEnableMCUInterrupts();
#endif
}

