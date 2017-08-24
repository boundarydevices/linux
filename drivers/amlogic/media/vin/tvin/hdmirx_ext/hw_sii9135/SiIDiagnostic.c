#include "SiIDiagnostic.h"
/*------------------------------------------------------------------------------
 * Module Name: Diagnostic
 *
 * Module Description: contains a collection of diagnostic functions
 *
 * Copyright © 2002-2005, Silicon Image, Inc.  All rights reserved.
 *
 * No part of this work may be reproduced, modified, distributed, transmitted,
 * transcribed, or translated into any language or computer format, in any form
 * or by any means without written permission of: Silicon Image, Inc.,
 * 1060 East Arques Avenue, Sunnyvale, California 94085
 *----------------------------------------------------------------------------
 */

/*------------------------------------------------------------------------------
 * Function Name: siiGetNCTS
 * Function Description: Reads N/CTS packet data from RX registers
 *
 * Accepts: none
 * Returns: pointer on string of bytes
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiGetNCTS(BYTE *pbNCTS)
{
	siiReadBlockHDMIRXP1(RX_HW_N_VAL_ADDR, 3, pbNCTS);
	pbNCTS[3] = 0;

	siiReadBlockHDMIRXP1(RX_HW_CTS_ADDR, 3, &pbNCTS[4]);
	pbNCTS[7] = 0;
}

/*------------------------------------------------------------------------------
 * Function Name: siiGetABKSV
 * Function Description:  Reads AKSV and BKSV vectors from RX registers
 *
 * Accepts: none
 * Returns: pointer on string of bytes
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiGetABKSV(BYTE *pbABKSV)
{
	siiReadBlockHDMIRXP0(RX_AKSV_ADDR, 5, pbABKSV);
	siiReadBlockHDMIRXP0(RX_BKSV_ADDR, 5, &pbABKSV[5]);
}

