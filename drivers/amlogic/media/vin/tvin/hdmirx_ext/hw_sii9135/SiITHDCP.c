/*------------------------------------------------------------------------------
 * Module Name: SiITHDCP
 *
 * Module Description: services time polling HDCP functions
 *
 *------------------------------------------------------------------------------
 * Copyright © 2002-2005, Silicon Image, Inc.  All rights reserved.
 *
 * No part of this work may be reproduced, modified, distributed, transmitted,
 * transcribed, or translated into any language or computer format, in any form
 * or by any means without written permission of: Silicon Image, Inc.,
 * 1060 East Arques Avenue, Sunnyvale, California 94085
 *----------------------------------------------------------------------------
 */
/*#include <stdio.h>*/
#include "SiIGlob.h"
#include "SiITHDCP.h"
#include "SiITrace.h"
#include "SiIRXIO.h"
#include "SiIRXDefs.h"

/*------------------------------------------------------------------------------
 * Function Name:
 * Function Description:
 *----------------------------------------------------------------------------
 */
static BOOL CheckHDCPDecypting(void)
{
	BOOL qResult = FALSE;

	if (siiReadByteHDMIRXP0(RX_HDCP_STAT_ADDR) & RX_BIT_HDCP_DECRYPTING)
		qResult = TRUE;
	return  qResult;
}
/*------------------------------------------------------------------------------
 * Function Name:
 * Function Description:
 *
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
BYTE siiGetHDCPStatus(void)
{
	BYTE bStatus;

	if (CheckHDCPDecypting())
		bStatus = SiI_RX_HDCP_Decrypted;
	else
		bStatus = SiI_RX_HDCP_NotDecrypted;
	return bStatus;

}


