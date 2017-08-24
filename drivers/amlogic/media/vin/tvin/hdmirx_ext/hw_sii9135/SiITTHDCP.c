/*------------------------------------------------------------------------------
 * Module Name:  SiITTHDCP
 *
 * Module Description:  Executes Timer HDCP tasks
 *
 * Copyright © 2002-2005, Silicon Image, Inc.  All rights reserved.
 *
 * No part of this work may be reproduced, modified, distributed, transmitted,
 * transcribed, or translated into any language or computer format, in any form
 * or by any means without written permission of: Silicon Image, Inc.,
 * 1060 East Arques Avenue, Sunnyvale, California 94085
 *----------------------------------------------------------------------------
 */
#include "SiIGlob.h"
#include "SiITTHDCP.h"
#include "SiITrace.h"
#include "SiIHDCP.h"
#include "SiIRXDefs.h"


/*------------------------------------------------------------------------------
 * Function Name: siiGetHDCPStatus
 * Function Description: This function reads HDCP status
 *
 * Accepts: none
 * Returns: BYTE of HDCP status
 * Globals: none
 *----------------------------------------------------------------------------
 */
BYTE siiGetHDCPStatus(void)
{
	BYTE bStatus;

	if (siiCheckHDCPDecrypting())
		bStatus = SiI_RX_HDCP_Decrypted;
	else
		bStatus = SiI_RX_HDCP_NotDecrypted;
	return bStatus;

}

