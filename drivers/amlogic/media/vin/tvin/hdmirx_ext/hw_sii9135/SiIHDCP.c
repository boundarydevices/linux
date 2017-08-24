/*------------------------------------------------------------------------------
 * Module Name SiIHDCP
 * Module Description:  HDCP functions
 *
 * Copyright © 2002-2005, Silicon Image, Inc.  All rights reserved.
 *
 * No part of this work may be reproduced, modified, distributed, transmitted,
 * transcribed, or translated into any language or computer format, in any form
 * or by any means without written permission of: Silicon Image, Inc.,
 * 1060 East Arques Avenue, Sunnyvale, California 94085
 *----------------------------------------------------------------------------
 */

#include "SiIHDCP.h"
#include "SiIRXIO.h"
#include "SiIRXDefs.h"
#include "SiIRXAPIDefs.h"
/*------------------------------------------------------------------------------
 * Function Name: ReqReAuthenticationRX
 * Function Description: clears Ri, used to notify upstream about HDCP failure
 *
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
void ReqReAuthenticationRX(void)
{
	siiIIC_RX_RWBitsInByteP0(RX_HDCP_CTRL_ADDR, RX_BIT_TRASH_RI, SET);
	/* this bit will be cleared automaticlly on authentication event */

}
/*-----------------------------------------------------------------------------
 * Function Name: siiCheckHDCPDecrypting
 * Function Description: checks if HDMI has been decrypted  (used by HDCP)
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
BOOL siiCheckHDCPDecrypting(void)
{
	BOOL qResult = FALSE;

	if (siiReadByteHDMIRXP0(RX_HDCP_STAT_ADDR) & RX_BIT_HDCP_DECRYPTING)
		qResult = TRUE;
	return  qResult;
}

