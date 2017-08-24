/*------------------------------------------------------------------------------
 * Module Name: Trace
 * ---
 * Module Description: Used to provide debugging Information
 *
 * Copyright © 2002-2005, Silicon Image, Inc.  All rights reserved.
 *
 * No part of this work may be reproduced, modified, distributed, transmitted,
 * transcribed, or translated into any language or computer format, in any form
 * or by any means without written permission of: Silicon Image, Inc.,
 * 1060 East Arques Avenue, Sunnyvale, California 94085
 *----------------------------------------------------------------------------
 */
#include "SiITrace.h"
#include "SiIGlob.h"

/*-----------------------------------------------------------------------------
 * Function Name: siiErrorHandler
 * Function Description:  Fill in buffer error structure
 * Accepts: none
 * Returns: none
 * Globals: structure SiI_Inf.Error
 *----------------------------------------------------------------------------
 */
void siiErrorHandler(BYTE bErrorCode)
{
	if (bErrorCode) {
		SiI_bECode[SiI_bECode[0] + 1] = bErrorCode;
		if (SiI_bECode[0] < ERR_BUF_SIZE)
			SiI_bECode[0]++;
	}
}
/*------------------------------------------------------------------------------
 * Function Name: siiWarningHandler
 * Function Description:  Fill in buffer with warning messages
 * Accepts: none
 * Returns: none
 * Globals: SiI_bWCode[16]
 *----------------------------------------------------------------------------
 */
void siiWarningHandler(BYTE bWrnCode)
{
	if (bWrnCode) {
		SiI_bWCode[SiI_bWCode[0] + 1] = bWrnCode;
		if (SiI_bWCode[0] < (WRN_BUF_SIZE - 1))
			SiI_bWCode[0]++;
	}
}
/*------------------------------------------------------------------------------
 * Function Name: siiResetErrorsAndWarnings
 * Function Description:  Fill in buffer with warning messages
 * Accepts: none
 * Returns: none
 * Globals: SiI_bWCode[0]
 *----------------------------------------------------------------------------
 */
void siiResetErrorsAndWarnings(void)
{
	BYTE i;

	for (i = 0; i < WRN_BUF_SIZE; i++)
		SiI_bWCode[i] = 0;
	for (i = 0; i < ERR_BUF_SIZE; i++)
		SiI_bECode[i] = 0;

}

/*------------------------------------------------------------------------------
 * Function Name: siiGetErrorsWarnings
 * Function Description:  Fill in buffer error structure
 * Accepts: none
 * Returns: none
 * Globals: structure SiI_Inf.Error
 *----------------------------------------------------------------------------
 */
BYTE siiGetErrorsWarnings(void)
{
	return (SiI_bECode[0] << 4) | SiI_bWCode[0];
}

/*------------------------------------------------------------------------------
 * Function Name: siiGetErrorsData
 * Function Description:
 * Accepts:  pbErrData
 * Returns:  number of errors
 * Globals: SiI_bWCode
 *----------------------------------------------------------------------------
 */
BYTE siiGetErrorsData(BYTE *pbErrData)
{
	BYTE i;

	for (i = 0; i < SiI_bECode[0]; i++)
		pbErrData[i] = SiI_bECode[1 + i];

	return SiI_bECode[0];

}

/*------------------------------------------------------------------------------
 * Function Name: siiGetWarningData
 * Function Description:
 * Accepts: pbWrnData
 * Returns: number of warnings
 * Globals: SiI_bECode
 *----------------------------------------------------------------------------
 */

BYTE siiGetWarningData(BYTE *pbWrnData)
{
	BYTE i;

	for (i = 0; i < SiI_bWCode[0]; i++)
		pbWrnData[i] = SiI_bWCode[1 + i];

	return SiI_bWCode[0];
}



