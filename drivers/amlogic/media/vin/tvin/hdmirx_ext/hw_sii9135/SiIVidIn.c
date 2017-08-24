/*------------------------------------------------------------------------------
 * Module Name:  VidIn
 * ---
 * Module Description: This module use for switchn between HDMI (DVI Inputs)
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
#include "SiIVidIn.h"
#include "SiIRXIO.h"
#include "SiITrace.h"
#include "SiIRXAPIDefs.h"
#include "SiIRXDefs.h"
#include "SiITTVideo.h"
#include "SiITTAudio.h"
#include "SiIHDCP.h"
#include "SiIHAL.h"
#include "SiIHDMIRX.h"
#include "SiISysCtrl.h"
#include "SiIAudio.h"

/*------------------------------------------------------------------------------
 * Function Name: siiChangeHPD
 * Function Description:  This function enables Hot Plug Detect (HPD) on Active
 *                        HDMI Input and disables on passive one
 *----------------------------------------------------------------------------
 */

static void ChangeHPD(BYTE SelectedChannel)
{
	if (SelectedChannel == SiI_RX_VInCh1) {
		halClearHPD1Pin();  /* YMA NOTE clr P2^4 */
		halSetHPD2Pin();
		if (SiI_Ctrl.bDevId == RX_SiI9135 ||
			SiI_Ctrl.bDevId ==  RX_SiI9125)
			siiSetNormalTerminationValueCh2(OFF);
	} else if (SelectedChannel == SiI_RX_VInCh2) {
		halClearHPD2Pin();
		halSetHPD1Pin();
		if (SiI_Ctrl.bDevId == RX_SiI9135 ||
			SiI_Ctrl.bDevId ==  RX_SiI9125)
			siiSetNormalTerminationValueCh1(OFF);
	}
}

/*------------------------------------------------------------------------------
 * Function Name: ChangeTMDSCoreAndDDC()
 * Function Description:  Function makes disable both TMDS cores and DDC,
 *                        and makes enable for active Video Input Channel
 *----------------------------------------------------------------------------
 */
static BYTE ChangeTMDSCoreAndDDC(BYTE bVideoInputChannels)
{
	BYTE bECode = FALSE;

	siiIIC_RX_RWBitsInByteP0(RX_SYS_SW_SWTCHC_ADDR,
		(RX_BIT_RX0_EN | RX_BIT_DDC0_EN | RX_BIT_RX1_EN |
		RX_BIT_DDC1_EN), CLR);

	if (bVideoInputChannels == SiI_RX_VInCh1) {
		siiIIC_RX_RWBitsInByteP0(RX_SYS_SW_SWTCHC_ADDR,
			RX_BIT_RX0_EN | RX_BIT_DDC0_EN, SET);
	} else {
		siiIIC_RX_RWBitsInByteP0(RX_SYS_SW_SWTCHC_ADDR,
			RX_BIT_RX1_EN | RX_BIT_DDC1_EN, SET);
	}

	return bECode;
}
/*------------------------------------------------------------------------------
 * Function Name: siiSetVideoInput
 * Function Description:  This function is used to Select new HDMI Input
 *
 * Accepts: bVideoInputChannel
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiSetVideoInput(BYTE bVideoInputChannel)
{
	siiMuteVideoAndSetSM_SyncInChange();

	siiReInitRX();

	ChangeTMDSCoreAndDDC(bVideoInputChannel);    /* re-init RX */

	ChangeHPD(bVideoInputChannel);
	SiI_Ctrl.bVidInChannel = bVideoInputChannel;
	/* Clearing Ri will be notification for host to start */
	ReqReAuthenticationRX();
}

/*------------------------------------------------------------------------------
 * Function Name: siiInitVideoInput
 * Function Description:  This function is used to Select new HDMI Input
 *
 * Accepts: bVideoInputChannel
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
BYTE siiInitVideoInput(BYTE bVideoInputChannel)
{
	ChangeTMDSCoreAndDDC(bVideoInputChannel);    /* re-init RX */

	ChangeHPD(bVideoInputChannel);
	SiI_Ctrl.bVidInChannel = bVideoInputChannel;
	/* Clearing Ri will be notification for host to start */
	ReqReAuthenticationRX();

	return siiGetErrorsWarnings();
}

/*-----------------------------------------------------------------------------
 * Function Name: siiChangeVideoInput
 * Function Description:
 *
 * Accepts: bVideoInputChannel
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiChangeVideoInput(BYTE bVideoInputChannel)
{
	if ((SiI_Ctrl.bDevId == RX_SiI9011) &&
		(bVideoInputChannel != SiI_RX_VInCh1)) {
		siiWarningHandler(SiI_WC_ChipNoCap);
	} else {
		/* YMA set already
		 *SiI_Ctrl.bVidInChannel = bVideoInputChannel;
		 */
		if (siiIsVideoOn() &&
			(SiI_Inf.bGlobStatus & SiI_RX_GlobalHDMI_Detected)) {
			/* Gives time for Audio soft muting */
			siiSetSM_ReqVidInChange();
			siiSetAudioMuteEvent();
		} else {
			siiSetVideoInput(SiI_Ctrl.bVidInChannel);
		}
	}

}

