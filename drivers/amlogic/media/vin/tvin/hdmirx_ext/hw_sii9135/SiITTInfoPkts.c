/*------------------------------------------------------------------------------
 * Module Name: SiITTInfoPkts
 *
 * Module Description: time based Info handling
 *
 * Copyright © 2002-2005, Silicon Image, Inc.  All rights reserved.
 *
 * No part of this work may be reproduced, modified, distributed, transmitted,
 * transcribed, or translated into any language or computer format, in any form
 * or by any means without written permission of: Silicon Image, Inc.,
 * 1060 East Arques Avenue, Sunnyvale, California 94085
 *----------------------------------------------------------------------------
 */

#include "SiIInfoPkts.h"
#include "SiIGlob.h"
#include "SiIRXIO.h"
#include "SiIRXDefs.h"
#include "SiITrace.h"

BYTE abOldInfoPkt[2] = { 0x00, 0x00 };

/*------------------------------------------------------------------------------
 * Function Name:  siiResetACPPacketData
 * Function Description:  Clears data in ACP packet
 *
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiResetACPPacketData(void)
{
	abOldInfoPkt[0] = abOldInfoPkt[1] = 0;
}

/*------------------------------------------------------------------------------
 * Function Name: CheckACPInterruptAndClearIfSet
 * Function Description: checking Audio Code Protection Packet interrupt
 *----------------------------------------------------------------------------
 */
static BOOL CheckACPInterruptAndClearIfSet(void)
{
	BOOL qResult = FALSE;

	if (siiReadByteHDMIRXP0(RX_HDMI_INT_ST6_ADDR) & RX_BIT_ACP_PACKET) {
		siiWriteByteHDMIRXP0(RX_HDMI_INT_ST6_ADDR, RX_BIT_ACP_PACKET);
		qResult = TRUE;
	}
	return qResult;

}
/*------------------------------------------------------------------------------
 * Function Name: InfoACPInterruptHandler
 * Function Description: checks ACP status
 *----------------------------------------------------------------------------
 */
void InfoACPInterruptHandler(void)
{ /* SiI_RX_NewInfo_ACP */
	BYTE abInfoPkt[2];

	siiReadBlockHDMIRXP1(RX_ACP_INFO_PKT_ADDR, 2, abInfoPkt);

	if (abInfoPkt[0] == ACP_Type) {
		/* call ACP processing */
		if (abInfoPkt[1] > 1) {
			/* Audio is protected */
			SiI_Inf.bGlobStatus |= SiI_RX_GlobalHDMI_ACP;
		} else {
			/* Flag repported on upper layer,
			 * Enable Digital Output
			 */
			SiI_Inf.bGlobStatus &= (~SiI_RX_GlobalHDMI_ACP);
		}
		if ((abInfoPkt[0] != abOldInfoPkt[0]) ||
			(abInfoPkt[1] != abOldInfoPkt[1])) {
			abOldInfoPkt[0] = abInfoPkt[0];
			abOldInfoPkt[1] = abInfoPkt[1];
			SiI_Inf.bNewInfoPkts |= SiI_RX_NewInfo_ACP;
		}
	}
}


/*------------------------------------------------------------------------------
 * Function Name:  siiTTInfoFrameHandler
 * Function Description: switches between Info decode addresses in order
 *                       to be able receive all types of Info, call checking
 *                       ACP status
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiTTInfoFrameHandler(void)
{
	if (CheckACPInterruptAndClearIfSet()) {
		InfoACPInterruptHandler();  /* checks ACP packet */
	} else {
		SiI_Inf.bGlobStatus &= (~SiI_RX_GlobalHDMI_ACP);
		SiI_Inf.bNewInfoPkts &= (~SiI_RX_NewInfo_ACP);
	}

}

