/******************************************************************************/
/*  Copyright (c) 2002-2005, Silicon Image, Inc.  All rights reserved.        */
/*  No part of this work may be reproduced, modified, distributed,            */
/*  transmitted, transcribed, or translated into any language or computer     */
/*  format, in any form or by any means without written permission of:        */
/*   Silicon Image, Inc., 1060 East Arques Avenue, Sunnyvale, California 94085*/
/******************************************************************************/
/*------------------------------------------------------------------------------
 * Module Name: UAudDAC
 *
 * Module Description:  this low level driver for Audio DAC Control
 *
 *----------------------------------------------------------------------------
 */
#include "SiIRXAPIDefs.h"
#include "UAudDAC.h"
#include "SiIHAL.h"

/*------------------------------------------------------------------------------
 * Function Name: WakeUpAudioDAC
 * Function Description: Wake up Audio DAC
 * Accepts:
 * Returns:
 * Globals:
 *--------------------------------------------------------
 */
void WakeUpAudioDAC(void)
{
	siiWriteByteAudDAC(AUDDAC_SPEED_PD_ADDR, AUDDAC_NORM_OP);
}
/*------------------------------------------------------------------------------
 * Function Name: PowerDownAudioDAC
 * Function Description: Wake up Audio DAC
 * Accepts:
 * Returns:
 * Globals:
 *--------------------------------------------------------
 */
void PowerDownAudioDAC(void)
{
	siiWriteByteAudDAC(AUDDAC_SPEED_PD_ADDR, AUDDAC_RST);
}
/*------------------------------------------------------------------------------
 * Function Name: halSetAudioDACMode
 * Function Description: Set Audio DAC modes DSD vs.PCM
 * Accepts:
 * Returns:
 * Globals:
 *--------------------------------------------------------
 */
void halSetAudioDACMode(BYTE bMode)
{
	/* put in reset to be safe */
	siiWriteByteAudDAC(AUDDAC_SPEED_PD_ADDR, AUDDAC_RST);
	halDelayMS(1); /* YAM wait for old data to flush */
	if (bMode == SiI_RX_AudioRepr_DSD)
		siiWriteByteAudDAC(AUDDAC_CTRL3_ADDR, ADAC_DSD_MODE);
	else
		siiWriteByteAudDAC(AUDDAC_CTRL3_ADDR, ADAC_PCM_MODE);
}

