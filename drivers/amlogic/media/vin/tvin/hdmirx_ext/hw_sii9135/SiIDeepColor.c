/*------------------------------------------------------------------------------
 * Module Name SiIDeepColor
 * Module Description: this file is used to handle Deep Color functions
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
#include "SiIRXIO.h"
#include "SiIRXDefs.h"
#include "SiIDeepColor.h"
#include "SiIVidF.h"

/*------------------------------------------------------------------------------
 * Function Name: VideoFIFO_Reset
 * Function Description: Makes Video FIFO reset
 *----------------------------------------------------------------------------
 */
void siiVideoFIFO_Reset(void)
{
	siiIIC_RX_RWBitsInByteP0(RX_SWRST_ADDR2, RX_BIT_DCFIFO_RST, SET);
	siiIIC_RX_RWBitsInByteP0(RX_SWRST_ADDR2, RX_BIT_DCFIFO_RST, CLR);

}

/*------------------------------------------------------------------------------
 * Function Name: siiCheckSupportDeepColorMode
 * Function Description: This function chechs Deep Color mode support
 *
 * Accepts: TRUE/FALSE
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
BOOL siiCheckSupportDeepColorMode(void)
{
	BOOL qResult = FALSE;

	if ((SiI_Ctrl.bDevId == RX_SiI9133)
		|| (SiI_Ctrl.bDevId == RX_SiI9125)
		|| (SiI_Ctrl.bDevId == RX_SiI9135)
		/* || (SiI_Ctrl[bHDMIDev].bDevId == RX_SiI8500) */
		) {
		qResult = TRUE;
	}
	return qResult;
}

/*------------------------------------------------------------------------------
 * Function Name: siiResetDeepColorMode
 * Function Description: This function sets TMDS registers respecting input
 *                       color color depth
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiResetDeepColorMode(void)
{
	/* reset video FIFO feature only in 9135 and/or after */
	if (SiI_Ctrl.bDevId != RX_SiI9133)
		siiVideoFIFO_Reset(); /* reset and flush    video FIFO */
	siiIIC_RX_RMW_ByteP0(RX_TMDS_ECTRL_ADDR,
		RX_DC_CLK_CTRL_MASK, RX_DC_CLK_8BPP_1X);
	SiI_Inf.AVI.bInputColorDepth = SiI_RX_CD_24BPP;
	/* bRegOldColorDepthStat = SiI_RX_CD_24BPP; */
}
/*------------------------------------------------------------------------------
 * Function Name: SiI_RX_SetOutputColorDepth
 * Function Description:
 *
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
BYTE SiI_RX_SetOutputColorDepth(BYTE bOutputColorDepth)
{
	BYTE bError = FALSE;

	switch (bOutputColorDepth) {
	case SiI_RX_CD_24BPP:
		siiIIC_RX_RMW_ByteP0(RX_VID_MODE2_ADDR,
			RX_DITHER_MODE_MASK, RX_DITHER_8BITS);
		break;
	case SiI_RX_CD_30BPP:
		siiIIC_RX_RMW_ByteP0(RX_VID_MODE2_ADDR,
			RX_DITHER_MODE_MASK, RX_DITHER_10BITS);
		break;
	case SiI_RX_CD_36BPP:
		siiIIC_RX_RMW_ByteP0(RX_VID_MODE2_ADDR,
			RX_DITHER_MODE_MASK, RX_DITHER_12BITS);
		break;
	default:
		bError = SiI_EC_UnsupportedColorDepth;
	}

	return bError;
}

/*------------------------------------------------------------------------------
 * Function Name: siiSetDeepColorMode
 * Function Description: This function sets TMDS registers respecting
 *                       input color color depth
 * Accepts: none
 * Returns: Error status
 * Globals: none
 *----------------------------------------------------------------------------
 */

BYTE siiSetDeepColorMode(void)
{
	BYTE bColorDepth;
	BYTE bError = FALSE;

	if (siiCheckSupportDeepColorMode()) {
		/* Read incoming pixel depth from
		 *latest General Control Packet
		 */
		bColorDepth = siiReadByteHDMIRXP0(RX_DC_STATUS_ADDR) &
				RX_DC_PIXELDEPTH_MASK;

		/* If it differs from the current setting */
		if (bColorDepth != SiI_Inf.AVI.bInputColorDepth) {

			siiMuteVideo(ON);

			/* reset video FIFO feature only in 9135 and/or after */
			if (SiI_Ctrl.bDevId == RX_SiI9135 ||
				SiI_Ctrl.bDevId == RX_SiI9125) {
				/* reset and flush    video FIFO */
				siiVideoFIFO_Reset();
			}
			/* Update the current setting */
			SiI_Inf.AVI.bInputColorDepth = bColorDepth;

			switch (bColorDepth) { /* Update the value in hardware*/
			case RX_DC_8BPP_VAL:
				siiIIC_RX_RMW_ByteP0(RX_TMDS_ECTRL_ADDR,
					RX_DC_CLK_CTRL_MASK, RX_DC_CLK_8BPP_1X);
				break;
			case RX_DC_10BPP_VAL:
				siiIIC_RX_RMW_ByteP0(RX_TMDS_ECTRL_ADDR,
					RX_DC_CLK_CTRL_MASK,
					RX_DC_CLK_10BPP_1X);
				break;
			case RX_DC_12BPP_VAL:
				siiIIC_RX_RMW_ByteP0(RX_TMDS_ECTRL_ADDR,
					RX_DC_CLK_CTRL_MASK,
					RX_DC_CLK_12BPP_1X);
				break;
			default:
				bError = SiI_EC_UnsupportedColorDepth;
			}

			siiMuteVideo(OFF);
		}
	}
	return bError;
}
