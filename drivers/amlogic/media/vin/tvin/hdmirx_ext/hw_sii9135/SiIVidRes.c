/*------------------------------------------------------------------------------
 * Module Name: SiI Video Resolution
 * ---
 * Module Description: Here are video resolution related functions
 *
 * Copyright © 2002-2005, Silicon Image, Inc.  All rights reserved.
 *
 * No part of this work may be reproduced, modified, distributed, transmitted,
 * transcribed, or translated into any language or computer format, in any form
 * or by any means without written permission of: Silicon Image, Inc.,
 * 1060 East Arques Avenue, Sunnyvale, California 94085
 *----------------------------------------------------------------------------
 */

#include "SiIHAL.h"
#include "SiIInfoPkts.h"
#include "SiIVidRes.h"
#include "SiIVidF.h"
#include "SiIGlob.h"
#include "SiIRXDefs.h"
#include "SiIRXIO.h"
#include "SiITrace.h"
#include "SiIDeepColor.h"

/* #ifdef SII_DUMP_UART
 * #include <stdio.h>
 * #endif
 */

#include "../hdmirx_ext_drv.h"

/*------------------------------------------------------------------------------
 * Function Name: abs_diff
 * Function Description: calculates diffrence between two parameters
 *----------------------------------------------------------------------------
 */
static WORD abs_diff(WORD A, WORD B)
{
	WORD Result;

	if (A > B)
		Result = A - B;
	else
		Result = B - A;
	return Result;
}
/*------------------------------------------------------------------------------
 * Function Name: siiGetPixClockFromTables
 * Function Description: Takes Pixel clock from Video Resolution tables
 *
 * Accepts: bIndex
 * Returns: none
 * Globals: byte, Pixel Clock
 *----------------------------------------------------------------------------
 */
BYTE siiGetPixClockFromTables(BYTE bIndex)
{
	return VModeTables[bIndex].PixClk;
}
/*------------------------------------------------------------------------------
 * Function Name: siiCheckSyncDetect
 * Function Description:  checks Sync Detect ( asserted when cirtain number of
 *                        DE counted )
 *----------------------------------------------------------------------------
 */
BOOL siiCheckSyncDetect(void)
{
	BOOL qResult = FALSE;

	if (siiReadByteHDMIRXP0(RX_STATE_ADDR) & RX_BIT_SCDT)
		qResult = TRUE;
	return qResult;
}
/*------------------------------------------------------------------------------
 * Function Name: siiCheckClockDetect
 * Function Description:  checks Clock Detect
 *----------------------------------------------------------------------------
 */
BOOL siiCheckClockDetect(void)
{
	BOOL qResult = FALSE;

	if (siiReadByteHDMIRXP0(RX_STATE_ADDR) & RX_BIT_CLK)
		qResult = TRUE;
	return qResult;
}
/*------------------------------------------------------------------------------
 * Function Name: GetVSyncFreq
 * Function Description: this function culculates Vertical Sync Frequency
 *----------------------------------------------------------------------------
 */
static WORD GetVSyncFreq(WORD wHtot, WORD wVtot)
{
	BYTE i;
	WORD wXClks = 0;
	DWORD dwXclksPerFrame;
	WORD wVFreq;
	BYTE bRegVal;

	bRegVal = siiReadByteHDMIRXP0(RX_SYS_CTRL1_ADDR);
	RXEXTPR("%s: RX_SYS_CTRL1_ADDR(0x%02x)=0x%02x\n",
		__func__, RX_SYS_CTRL1_ADDR, bRegVal);

	/* FrameXClk = (( Htot * Vtot)* (AVR of 8 Reg128XClk ))/1128 */

	if ((SiI_Ctrl.bDevId == RX_SiI9133) ||
		(SiI_Ctrl.bDevId == RX_SiI9135 ||
		SiI_Ctrl.bDevId == RX_SiI9125) ||
		(SiI_Ctrl.bDevId == RX_SiIIP11)) {
		for (i = 0; i < 8; i++) {
			/* by doing this we will get average XClk */
			wXClks += GetXClockW();
		}
		if (bRegVal & 0x10)
			wXClks <<= 1;
		SiI_Inf.Sync.bFPix = (BYTE)((WORD)
			(SII_XCLOCK_OSC_SCALED2047_FOR_CALK_FPIX / wXClks));
	} else {
		for (i = 0; i < 8; i++) {
			/* by doing this we will get average XClk */
			wXClks += GetXClock();
		}
		if (bRegVal & 0x10)
			wXClks <<= 1;
		SiI_Inf.Sync.bFPix = (BYTE)((WORD)
			(SII_XCLOCK_OSC_SCALED_FOR_CALK_FPIX / wXClks));
	}

	RXEXTPR("%s: wXClks=%d, bFPix=%d\n",
		__func__, wXClks, SiI_Inf.Sync.bFPix);
	dwXclksPerFrame = (DWORD)wHtot * (DWORD)wVtot;

	if ((SiI_Ctrl.bDevId == RX_SiI9133) ||
		(SiI_Ctrl.bDevId == RX_SiI9135 ||
		SiI_Ctrl.bDevId == RX_SiI9125) ||
		(SiI_Ctrl.bDevId == RX_SiIIP11)) {
		RXEXTPR("%s: bDevId siI9135\n", __func__);
		dwXclksPerFrame *= (DWORD)(wXClks >> 3);
		dwXclksPerFrame >>= 11; /* div ( 8 * 2048 ) */
		wVFreq = (WORD)(SII_XCLOCK_OSC_SCALED2047_AND_MUL100 /
			dwXclksPerFrame);
	} else {
		RXEXTPR("%s: bDevId other\n", __func__);
		dwXclksPerFrame *= (DWORD)wXClks;
		dwXclksPerFrame >>= 10; /* div ( 8 * 128 ) */
		wVFreq = (WORD)(SII_XCLOCK_OSC_SCALED_AND_MUL100 /
			dwXclksPerFrame);
	}

	return wVFreq;
}

/*------------------------------------------------------------------------------
 * Function Name: siiGetSyncInfo()
 * Function Description: is used to get H/V Sync and polarity information
 *----------------------------------------------------------------------------
 */
void siiGetSyncInfo(struct SyncInfoType_s *SyncInfo)
{
	BYTE abData[4];

	siiReadBlockHDMIRXP0(RX_HRES_L_ADDR, 4, abData);

	RXEXTPR("%s: RX_HRES_L_ADDR: 0x%02x,0x%02x,0x%02x,0x%02x\n",
		__func__, abData[0], abData[1], abData[2], abData[3]);
	SyncInfo->ClocksPerLine = abData[0] | (abData[1] << 8);
	SyncInfo->HTot = abData[2] | (abData[3] << 8);
	RXEXTPR("%s: ClocksPerLine=%d, HTot=%d, pixel_repl=%d\n",
		__func__, SyncInfo->ClocksPerLine,
		SyncInfo->HTot, SiI_Ctrl.bShadowPixRepl);
	if (SiI_Ctrl.bShadowPixRepl == 0x01)
		SyncInfo->ClocksPerLine <<= 1; /* pixel replicated by 2 clocks*/
	else if (SiI_Ctrl.bShadowPixRepl == 0x02)
		SyncInfo->ClocksPerLine *= 3; /* pixel replicated by 3 clocks*/
	else if (SiI_Ctrl.bShadowPixRepl == 0x03)
		SyncInfo->ClocksPerLine <<= 2; /* pixel replicated by 4 clocks*/
	SyncInfo->VFreq = GetVSyncFreq(SyncInfo->ClocksPerLine, SyncInfo->HTot);
	RXEXTPR("%s: VFreq=%d\n", __func__, SyncInfo->VFreq);
	SyncInfo->RefrTypeVHPol = GetRefrTypeHVPol();

}
/*------------------------------------------------------------------------------
 * Function Name: CompareWithVMTableRef()
 * Function Description:  Compare timing inform with an entry of video
 *                        mode reference table
 *----------------------------------------------------------------------------
 */
static BYTE CompareWithVMTableRef(BYTE Index, struct SyncInfoType_s *SyncInform)
{
	BOOL qResult = FALSE;

	if (VModeTables[Index].Tag.RefrTypeVHPol == SyncInform->RefrTypeVHPol) {
		if (abs_diff(VModeTables[Index].Tag.VFreq, SyncInform->VFreq) <
			V_TOLERANCE) {
			if (abs_diff(VModeTables[Index].Tag.Total.Lines,
				SyncInform->HTot) < LINES_TOLERANCE) {
				if (abs_diff(
					VModeTables[Index].Tag.Total.Pixels,
					SyncInform->ClocksPerLine) <
					PIXELS_TOLERANCE) {
					qResult = TRUE;
				}
			}
		}
	}
	return qResult;

}
/*------------------------------------------------------------------------------
 * Function Name:  ompareWithVMTableRefIgnorPol()
 * Function Description:  Compare timing inform with an entry of video
 *                        mode reference table, but polarity information
 *                        is ignored
 *----------------------------------------------------------------------------
 */
static BYTE CompareWithVMTableRefIgnorPol(BYTE Index,
		struct SyncInfoType_s *SyncInform)
{
	BOOL qResult = FALSE;

	if ((VModeTables[Index].Tag.RefrTypeVHPol & INTERLACE_MASK) ==
		(SyncInform->RefrTypeVHPol & INTERLACE_MASK)) {
		if (abs_diff(VModeTables[Index].Tag.VFreq, SyncInform->VFreq) <
			V_TOLERANCE) {
			if (abs_diff(VModeTables[Index].Tag.Total.Lines,
				SyncInform->HTot) < LINES_TOLERANCE) {
				if (abs_diff(
					VModeTables[Index].Tag.Total.Pixels,
					SyncInform->ClocksPerLine) <
					PIXELS_TOLERANCE) {
					qResult = TRUE;
				}
			}
		}
	}
	return qResult;

}
/*------------------------------------------------------------------------------
 * Function Name: siiVideoModeDetection
 * Function Description: This function used for search video resolution modes
 *                       through reference tables
 *----------------------------------------------------------------------------
 */

/* This function used for search video modes */
BYTE siiVideoModeDetection(BYTE *pbIndex, struct SyncInfoType_s *SyncInf)
{
	BYTE bResult = FALSE;

	for ((*pbIndex) = 0; (*pbIndex) < NMODES; (*pbIndex)++) {
		if (CompareWithVMTableRef(*pbIndex, SyncInf)) {
			bResult = 1;
			break;
		}
	}
	if ((*pbIndex) == NMODES) {
		/* Preset mode not found, search ignoring polarity then: */
		for ((*pbIndex) = 0; (*pbIndex) < NMODES; (*pbIndex)++) {
			if (CompareWithVMTableRefIgnorPol(*pbIndex, SyncInf)) {
				bResult  = 2;
				break;
			}
		}
	}
	return bResult;
}

/*------------------------------------------------------------------------------
 * Function Name: CheckOutOfRangeConditions
 * Function Description: This function is used to detect of sync data overrange
 *                       conditions
 *----------------------------------------------------------------------------
 */

BOOL siiCheckOutOfRangeConditions(struct SyncInfoType_s *SyncInfo)
{
	WORD wHFreq;
	BOOL qOutOfRangeF = TRUE;

	wHFreq = (WORD)(((DWORD) SyncInfo->VFreq * SyncInfo->HTot) / 1000);
	if (SyncInfo->RefrTypeVHPol & INTERLACE_MASK) {
		/* Interlace Video Mode */
		if ((wHFreq < INTERLACE_HFREQ_HIGH_LIMIT) &&
			(wHFreq > INTERLACE_HFREQ_LOW_LIMIT)) {
			if ((SyncInfo->VFreq < INTERLACE_VFREQ_HIGH_LIMIT) &&
				(SyncInfo->VFreq > INTERLACE_VFREQ_LOW_LIMIT)) {
				qOutOfRangeF = FALSE;  /* sync in range */
			}
		}
#ifdef SII_DUMP_UART
		printf("\n--HFreq = %i VFreq = %i ScanPol=%02X--\n",
			wHFreq, SyncInfo->VFreq, (int)SyncInfo->RefrTypeVHPol);
#endif
	} else { /* Progressive Video Mode */
		if ((wHFreq < PROGRESSIVE_HFREQ_HIGH_LIMIT) &&
			(wHFreq > PROGRESSIVE_HFREQ_LOW_LIMIT)) {
			if ((SyncInfo->VFreq < PROGRESSIVE_VFREQ_HIGH_LIMIT) &&
				(SyncInfo->VFreq >
				PROGRESSIVE_VFREQ_LOW_LIMIT)){
				qOutOfRangeF = FALSE;  /* sync in range */
			}
#ifdef SII_DUMP_UART
			printf("\n--HFreq = %i VFreq = %i ScanPol=%02X--\n",
				wHFreq, SyncInfo->VFreq,
				(int)SyncInfo->RefrTypeVHPol);
#endif
		}
	}
	return qOutOfRangeF;
}
/*------------------------------------------------------------------------------
 * Function Name: GetColorimetryFormVidRes
 * Function Description: this function gets Colorimetry information from video
 *                       input resolution
 *----------------------------------------------------------------------------
 */

static BYTE GetColorimetryFormVidRes(BYTE bVidResId)
{
	BYTE bColorimetry;

	bColorimetry = SiI_RX_ITU_601;
	if (VModeTables[bVidResId].Res.V >= 720)
		bColorimetry = SiI_RX_ITU_709;
	return bColorimetry;
}
/*------------------------------------------------------------------------------
 * Function Name: siiGetPixReplicationFormVidRes
 * Function Description: this function gets Pixel Replication Rate from entry
 *                       of detected video resolution
 *----------------------------------------------------------------------------
 */
static BYTE siiGetPixReplicationFormVidRes(BYTE VidResId)
{
	return VModeTables[VidResId].PixRepl;
}
/*------------------------------------------------------------------------------
 * Function Name: siiGetPixReplicationFormVidRes
 * Function Description: this function gets Pixel Replication Rate from entry
 *                       of detected video resolution
 *----------------------------------------------------------------------------
 */
static BYTE siiIsInterlaceMode(BYTE VidResId)
{
	BYTE tmp;

	tmp = (VModeTables[VidResId].Tag.RefrTypeVHPol & SiI_InterlaceMask);
	return tmp;
}
/*------------------------------------------------------------------------------
 * Function Name: siiSetVideoBlankLevel()
 * Function Description: this function restores blank level which dependes on
 *                       Input Color Space and Input Video resolution
 *----------------------------------------------------------------------------
 */

static BYTE SetVideoBlankLevel(BYTE bVidResId)
{
	BYTE bEWCode = FALSE;

	BYTE bBlankDataCh[3];

	bBlankDataCh[0] = 0;     /* blank levels for PC modes */
	bBlankDataCh[1] = 0;
	bBlankDataCh[2] = 0;

	if (SiI_Inf.AVI.bInputColorSpace == SiI_RX_ICP_RGB) {
		if ((bVidResId < PC_BASE) && (bVidResId != VGA)) { /* RGB CE */
			bBlankDataCh[0] = 0x10;
			bBlankDataCh[1] = 0x10;
			bBlankDataCh[2] = 0x10;
		}
	} else if (SiI_Inf.AVI.bInputColorSpace == SiI_RX_ICP_YCbCr444) {
		/* YCbCr 4:4:4 */
		bBlankDataCh[0] = 0x80;
		bBlankDataCh[1] = 0x10;
		bBlankDataCh[2] = 0x80;
	} else if (SiI_Inf.AVI.bInputColorSpace == SiI_RX_ICP_YCbCr422) {
		/* YCbCr 4:2:2 */
		bBlankDataCh[0] = 0x00;
		bBlankDataCh[1] = 0x10;
		bBlankDataCh[2] = 0x80;
	} else
		bEWCode = SiI_EC_InColorSpace;

	siiWriteBlockHDMIRXP0(RX_BLANKDATA_CH0_ADDR, 3, bBlankDataCh);
	siiWarningHandler(bEWCode);
	return bEWCode;

}
/*------------------------------------------------------------------------------
 * Function Name:  siiSetOutputVideoFilter
 * Function Description: this function selects type of Output Video Filter
 *                       which depends from Video Resolution
 *----------------------------------------------------------------------------
 */
#ifdef _OUTPUT_VFILTER_
/* TThis function controlls extrnal Video Filter, if it present */

static BYTE SetOutputVideoFilter(BYTE bVidResId)
{
	BYTE bEWCode = FALSE;

	bVidResId = 0;
	SiI_Inf.Sync
	return bEWCode;
}
#endif /* _OUTPUT_VFILTER_ */
/*------------------------------------------------------------------------------
 * Function Name: CompareNewOldSync()
 * Function Description:  This function is used to detect that resolution has
 *                        stated to change
 *----------------------------------------------------------------------------
 */
BOOL siiCompareNewOldSync(struct SyncInfoType_s *OldSyncInfo,
		struct SyncInfoType_s *NewSyncInfo)
{
	BOOL qResult = FALSE;

	if ((abs_diff(OldSyncInfo->HTot, NewSyncInfo->HTot) <= LINES_TOLERANCE)
		&& (abs_diff(OldSyncInfo->ClocksPerLine,
		NewSyncInfo->ClocksPerLine) <= PIXELS_TOLERANCE)) {
		qResult = TRUE;
	}

#ifdef SII_DUMP_UART
	if (!qResult) {
		printf("--Old HTot= %i ClocksPerLine = %i\n",
			OldSyncInfo->HTot, OldSyncInfo->ClocksPerLine);
		printf("--New HTot= %i ClocksPerLine = %i\n",
			NewSyncInfo->HTot, NewSyncInfo->ClocksPerLine);
	}
#endif
	return qResult;
}
/*------------------------------------------------------------------------------
 * Function Name: ConfigInterlaceCSync( )
 * Function Description: Sets parameters which used for generation Composite
 *                       sync for particular Interlace Modes
 *----------------------------------------------------------------------------
 */

static void ConfigInterlaceCSync(BYTE bVidResId)
{
	siiWriteWordHDMIRXP0(RX_CAPHALFLINE_ADDR,
		InterlaceCSync[bVidResId].CapHalfLineCnt);
	siiWriteWordHDMIRXP0(RX_PREEQPULSCNT_ADDR,
		InterlaceCSync[bVidResId].PreEqPulseCnt);
	siiWriteWordHDMIRXP0(RX_SERRATEDCNT_ADDR,
		InterlaceCSync[bVidResId].SerratedCnt);
	siiWriteWordHDMIRXP0(RX_POSTEQPULSECNT_ADDR,
		InterlaceCSync[bVidResId].PostEqPulseCnt);

}
/*------------------------------------------------------------------------------
 * Function Name: siiConfigureSyncOnYForInretlaceModes()
 * Function Description: this functio is used to select which table should be
 *                       used for generation of Sync for Interlace Modes
 *----------------------------------------------------------------------------
 */

static void ConfigureSyncOnYForInretlaceModes(BYTE bVidResId)
{
	if (VModeTables[bVidResId].Res.V == 480) {
		ConfigInterlaceCSync(0);
	} else if (VModeTables[bVidResId].Res.V == 576) {
		ConfigInterlaceCSync(1);
	} else if (VModeTables[bVidResId].Res.V == 1080) {
		ConfigInterlaceCSync(2);
	} else {
#ifdef SII_DUMP_UART
		printf("Interlace Output is not supported\n");
#endif
	}
}
/*------------------------------------------------------------------------------
 * Function Name:           siiSetColorimetry()
 * Function Description:    sets colorimetry formula whith used with color space
 *                          converter (BT601 vs. BT709)
 *----------------------------------------------------------------------------
 */
static BYTE SetColorimetry(BYTE bColorimetry)
{
	BYTE bEWCode = FALSE;

	if (bColorimetry == SiI_RX_ITU_601) {
		siiIIC_RX_RWBitsInByteP0(RX_VID_CTRL_ADDR,
			RX_BIT_BT709_RGBCONV | RX_BIT_BT709_YCbCrCONV, CLR);
	} else if (bColorimetry == SiI_RX_ITU_709) {
		siiIIC_RX_RWBitsInByteP0(RX_VID_CTRL_ADDR,
			RX_BIT_BT709_RGBCONV | RX_BIT_BT709_YCbCrCONV, SET);
	} else {
		bEWCode = SiI_WC_Colorimetry;
	}
	return bEWCode;
}
/*------------------------------------------------------------------------------
 * Function Name: siiGetVideoResId
 * Function Description: provides short resolution info for user layer
 *----------------------------------------------------------------------------
 */
BYTE siiGetVideoResId(BYTE bVidResIndex)
{
	BYTE bVideoResId;
	BYTE bVIC;

	if (bVidResIndex < PC_BASE) {
		bVideoResId = VModeTables[bVidResIndex].ModeId.Mode_C1;

		if (VModeTables[bVidResIndex].ModeId.Mode_C2) {
			if (SiI_Inf.AVI.bAVI_State == SiI_RX_GotAVI) {
				/* get Video Id Code form Info Frame Packet */
				bVIC = siiGetVIC();
				if ((bVIC == bVideoResId) ||
					(bVIC == bVideoResId + 1))
					bVideoResId = bVIC;
				else
					bVideoResId |= SiI_CE_orNextRes;
			} else
				bVideoResId |= SiI_CE_orNextRes;
		}
		bVideoResId |= SiI_CE_Res;
	} else {
		bVideoResId = bVidResIndex - PC_BASE;
	}
	return bVideoResId;
}

/*------------------------------------------------------------------------------
 * Function Name: ConvertInterlaceFieldsIntoFrames
 * Function Description:
 *----------------------------------------------------------------------------
 */
static WORD ConvNLinesInFieldIntoNLinesInFrame(WORD bLines)
{
	if (SiI_Inf.Sync.RefrTypeVHPol & INTERLACE_MASK) {
		if (bLines & 0x0001) { /* if odd */
			bLines += bLines;
		} else {
			/* calculate total number of lines in frame */
			bLines += (bLines + 1);
		}
	}
	return bLines;
}
/*------------------------------------------------------------------------------
 * Function Name: ConvertClocksToPixels
 * Function Description: For pixel replicated modes it's need to convert clocks
 *                       to pixels for not replicated modes clocks equal
 *                       to pixels
 *----------------------------------------------------------------------------
 */
static void ConvertClocksToPixels(WORD *wpPixClocks)
{
	if (SiI_Ctrl.bShadowPixRepl == 0x01)
		*wpPixClocks >>= 1;
	else if (SiI_Ctrl.bShadowPixRepl == 0x02)
		*wpPixClocks /= 3;
	else if (SiI_Ctrl.bShadowPixRepl == 0x03)
		*wpPixClocks >>= 2;
}

#ifdef SII_NO_RESOLUTION_DETECTION

/*------------------------------------------------------------------------------
 * Function Name: siiGetVideoInputResolutionFromRegisters
 * Function Description:  Using this function use gets extended
 *                        information about detected video resolution
 * Accepts: BYTE, resolution index in table
 * Returns: BYTE *, pointer on resolution data
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiGetVideoInputResolutionromRegisters(BYTE *pbVideoResInfo)
{
	WORD wAux;
	BYTE abData[4];

	pbVideoResInfo[0] = SiI_ResNotDetected;
	/* added for new API */
	pbVideoResInfo[1] = (SiI_Inf.AVI.bPixRepl << 4);
	pbVideoResInfo[1] |= SiI_Inf.Sync.RefrTypeVHPol;
	pbVideoResInfo[2] = SiI_Inf.Sync.bFPix; /* GetFPixClock(); */
	pbVideoResInfo[3] = (BYTE)(SiI_Inf.Sync.VFreq & 0x00FF);
	pbVideoResInfo[4] = (BYTE)(SiI_Inf.Sync.VFreq >> 8);

	siiReadBlockHDMIRXP0(RX_RES_ACTIVE_ADDR, 4, abData);

	wAux = abData[0] | (abData[1] << 8);
	ConvertClocksToPixels(&wAux);
	pbVideoResInfo[5] = (BYTE)(wAux & 0x00FF);
	pbVideoResInfo[6] = (BYTE)((wAux >> 8) & 0x000F);

	wAux = ConvNLinesInFieldIntoNLinesInFrame(abData[2] | (abData[3] << 8));

	pbVideoResInfo[7] = (BYTE)(wAux & 0x00FF);
	pbVideoResInfo[8] = (BYTE)((wAux >> 8) & 0x000F);

	wAux = SiI_Inf.Sync.ClocksPerLine;
	ConvertClocksToPixels(&wAux);
	pbVideoResInfo[9] = (BYTE)(wAux & 0x00FF);
	pbVideoResInfo[10] = (BYTE)((wAux >> 8) & 0x000F);

	wAux = ConvNLinesInFieldIntoNLinesInFrame(SiI_Inf.Sync.HTot);

	pbVideoResInfo[11] = (BYTE)(wAux  & 0x00FF);
	pbVideoResInfo[12] = (BYTE)((wAux >> 8) & 0x000F);
}
#endif
/*------------------------------------------------------------------------------
 * Function Name: siiGetVideoInputResolution
 * Function Description:  Using this function use gets extended
 *                        information about detected video resolution
 * Accepts: BYTE, resolution index in table
 * Returns: BYTE *, pointer on resolution data
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiGetVideoInputResolution(BYTE bVidResIndex, BYTE *pbVideoResInfo)
{
	WORD wAux;

	pbVideoResInfo[0] = siiGetVideoResId(bVidResIndex);
	/* YMA changed to support API rev2
	 *    pbVideoResInfo[1] = SiI_Inf.Sync.RefrTypeVHPol;
	 */
	pbVideoResInfo[1] = (SiI_Inf.AVI.bPixRepl << 4);
	pbVideoResInfo[1] |= SiI_Inf.Sync.RefrTypeVHPol;

	pbVideoResInfo[2] = siiGetPixClockFromTables(bVidResIndex);

	pbVideoResInfo[3] = (BYTE)
		(VModeTables[bVidResIndex].Tag.VFreq & 0x00FF);
	pbVideoResInfo[4] = (BYTE)
		((VModeTables[bVidResIndex].Tag.VFreq >> 8) & 0x00FF);

	wAux = VModeTables[bVidResIndex].Res.H;
	/* for pixel replicated resolutions take out replicated pixels */
	ConvertClocksToPixels(&wAux);
	pbVideoResInfo[5] = (BYTE)(wAux & 0x00FF); /* active number of pixels */
	pbVideoResInfo[6] = (BYTE)((wAux >> 8) & 0x000F);

	pbVideoResInfo[7] = (BYTE)(VModeTables[bVidResIndex].Res.V  & 0x00FF);
	pbVideoResInfo[8] = (BYTE)
		((VModeTables[bVidResIndex].Res.V >> 8) & 0x000F);

	wAux = VModeTables[bVidResIndex].Tag.Total.Pixels;
	/* for pixel replicated resolutions take out replicated pixels */
	ConvertClocksToPixels(&wAux);
	pbVideoResInfo[9] = (BYTE)(wAux & 0x00FF); /* total number of pixels */
	pbVideoResInfo[10] = (BYTE)((wAux >> 8) & 0x000F);

	wAux = ConvNLinesInFieldIntoNLinesInFrame(
		VModeTables[bVidResIndex].Tag.Total.Lines);

	pbVideoResInfo[11] = (BYTE)(wAux & 0x00FF);
	pbVideoResInfo[12] = (BYTE)((wAux >> 8) & 0x000F);

}
/*------------------------------------------------------------------------------
 * Function Name: siiSetVidResDependentVidOutputVormat
 * Function Description: Functions sets Video Resolution dependent parameters:
 *                       Pix Replication Divider, Output Divider,
 *                       Color Converter Type (BT601 vs. BT701)
 * Accepts: Video Resolution Id, Video Path Select, AVI State
 * Returns: Error code byte
 * Globals: bColorimetry, bPixRepl
 *----------------------------------------------------------------------------
 */

BYTE siiSetVidResDependentVidOutputFormat(BYTE bVidResId,
		BYTE bVideoPathSelect, BYTE bAVI_State)
{
	BYTE bECode = FALSE;

	/* PC modes and CE have different blank levels */
	SetVideoBlankLevel(SiI_Inf.bVResId);
	if ((bAVI_State == SiI_RX_NoAVI) ||
		(SiI_Inf.AVI.bColorimetry == SiI_RX_ColorimetryNoInfo)) {
		SiI_Inf.AVI.bColorimetry = GetColorimetryFormVidRes(bVidResId);
	}
	SetColorimetry(SiI_Inf.AVI.bColorimetry);

	siiSetVideoPathColorSpaceDependent(SiI_Ctrl.VideoF.bOutputVideoPath,
		SiI_Inf.AVI.bInputColorSpace);

	if (SiI_Inf.AVI.bAVI_State == SiI_RX_NoAVI) {
		SiI_Inf.AVI.bPixRepl =
			siiGetPixReplicationFormVidRes(bVidResId);
	}
	if (!bECode) {
		siiSetVidResDependentVidPath(SiI_Inf.AVI.bPixRepl,
			bVideoPathSelect);     /* TODO name */

		if ((SiI_Ctrl.VideoF.bOutputSyncSelect == SiI_RX_AVC_SOGY) &&
			siiIsInterlaceMode(bVidResId)) {
			/* YMA changed by vk for in new API */
			if ((SiI_Ctrl.bDevId == RX_SiI9021) ||
				(SiI_Ctrl.bDevId == RX_SiI9031))
				ConfigureSyncOnYForInretlaceModes(bVidResId);
			else
				siiWarningHandler(SiI_WC_ChipNoCap);
		}
	}
	return bECode;
}

/*------------------------------------------------------------------------------
 * Function Name: PrintVModeParameters()
 * Function Description: This function prints out found video mode information,
 *                       used for debugging purpose
 *----------------------------------------------------------------------------
 */
/* #ifdef SII_DUMP_UART */
void siiPrintVModeParameters(BYTE Index, BYTE SearchRes)
{
	char str[200];
	int len = 0;

	if (SearchRes) {
		len += sprintf(str+len, "sii9135 vmode:\n");

		if (SearchRes == 2) {
			len += sprintf(str+len,
				"Video Mode has polarity mismatch\n");
		}
		if (SiI_Inf.bGlobStatus & SiI_RX_GlobalHDMI_Detected) {
			len += sprintf(str+len, "HDMI MODE\n");
		} else {
			len += sprintf(str+len, "DVI MODE ");
			len += sprintf(str+len,
				"v. mode detected, AVI will be ignored\n");
		}
		if (Index < PC_BASE)
			len += sprintf(str+len, "CE Space:");
		else
			len += sprintf(str+len, "PC Space:");
		len += sprintf(str+len, " Mode ROM-%i",
			(int)VModeTables[Index].ModeId.Mode_C1);
		if (VModeTables[Index].ModeId.Mode_C2) {
			len += sprintf(str+len, " or %i",
				(int)VModeTables[Index].ModeId.Mode_C2);
		}
		if (VModeTables[Index].ModeId.SubMode) {
			len += sprintf(str+len, " Sub mode: %i",
				VModeTables[Index].ModeId.SubMode);
		}
		len += sprintf(str+len, " Resolution: %i x %i@%i\n",
			VModeTables[Index].Res.H, VModeTables[Index].Res.V,
			VModeTables[Index].Tag.VFreq);
		RXEXTPR("%s: %s\n", __func__, str);
	}
}
/* #endif */



