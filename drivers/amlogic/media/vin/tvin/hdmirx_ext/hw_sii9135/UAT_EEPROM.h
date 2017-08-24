#ifndef SII_AT_EEPROM
#define SII_AT_EEPROM

#include "SiITypeDefs.h"
/*#include "AT89C51XD2.h" */

#define halDisableMCUInterrupts()
#define halEnableMCUInterrupts()
#define halIsIntEEPROM_Busy()
#define halIntEEPROM_Enable()
#define halIntEEPROM_Disable()

void siiWriteByteInternEEPROM(WORD wAddr, BYTE bData);
void siiReadByteInternEEPROM(WORD wAddr, BYTE *pbData);

#endif
