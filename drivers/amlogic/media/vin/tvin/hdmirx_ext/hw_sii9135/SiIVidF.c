/*------------------------------------------------------------------------------
 * Module Name: SiIVidF ( Video Format )
 * ---
 * Module Description: Function of this module are used for setting up output
 *                     format of receiver
 *
 * Copyright © 2002-2005, Silicon Image, Inc.  All rights reserved.
 *
 * No part of this work may be reproduced, modified, distributed, transmitted,
 * transcribed, or translated into any language or computer format, in any form
 * or by any means without written permission of: Silicon Image, Inc.,
 * 1060 East Arques Avenue, Sunnyvale, California 94085
 *----------------------------------------------------------------------------
 */

#include "SiIVidF.h"
#include "SiIRXDefs.h"
#include "SiIRXAPIDefs.h"
#include "SiIRXIO.h"
#include "SiIGlob.h"
#include "SiIVidRes.h"
#include "SiIHDMIRX.h"
#include "SiITrace.h"
#include "../hdmirx_ext_drv.h"
/*------------------------------------------------------------------------------
 * Function Name: siiGetVideoFormatData(...)
 * Function Description: This function reads video format data from
 *                       control registers
 *
 * Accepts: pointer on Video Format Data
 * Returns: pointer on Video Format Data
 * Globals: none
 *----------------------------------------------------------------------------
 */
BYTE siiGetVideoFormatData(BYTE *pbVidFormatData)
{
	BYTE bECode = FALSE;

	siiReadBlockHDMIRXP0(RX_VID_FORMAT_BASE_ADDR, 3, pbVidFormatData);
	return bECode;
}
/*-----------------------------------------------------------------------------
 * Function Name: siiSetVideoFormatData(...)
 * Function Description: This function writes video format data into
 *                       control registers
 *
 * Accepts: pointer on Video Format Data
 * Returns: pointer on Video Format Data
 * Globals: none
 *----------------------------------------------------------------------------
 */
BYTE siiSetVideoFormatData(BYTE *pbVidFormatData)
{
	BYTE bECode = FALSE;

	siiWriteBlockHDMIRXP0(RX_VID_FORMAT_BASE_ADDR, 3, pbVidFormatData);
	return bECode;
}
/*------------------------------------------------------------------------------
 * Function Name: siiSetVideoPathColorSpaceDependent
 * Function Description:
 *
 * Accepts:
 * Returns:
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiSetVideoPathColorSpaceDependent(BYTE bVideoPath, BYTE bInputColorSpace)
{
	BYTE bVidFormatData[3];

	siiGetVideoFormatData(bVidFormatData);
	siiPrepVideoPathSelect(bVideoPath, bInputColorSpace, bVidFormatData);
	siiSetVideoFormatData(bVidFormatData);

}

/*------------------------------------------------------------------------------
 * Function Name: Check if Video DAC capable
 * Function Description:
 *----------------------------------------------------------------------------
 */
BOOL CheckVideoDAC_Cap(void)
{
	BOOL qVidedDACCap = FALSE;

	if ((SiI_Ctrl.bDevId == RX_SiI9021) || (SiI_Ctrl.bDevId == RX_SiI9031))
		qVidedDACCap = TRUE;
	return qVidedDACCap;

}
/*------------------------------------------------------------------------------
 * Function Name: CheckVidPathForDevCap
 * Function Description:
 *----------------------------------------------------------------------------
 */
static void CheckVidPathForDevCap(BYTE bVideoPath)
{
	BYTE bWCode = FALSE;

	switch (bVideoPath) {
	case SiI_RX_P_RGB_2PixClk:            /* SiI9011 */
	case SiI_RX_P_YCbCr444_2PixClk:       /* SiI9011 */
	case SiI_RX_P_YCbCr422_16B_2PixClk:   /* SiI9011 */
	case SiI_RX_P_YCbCr422_20B_2PixClk:   /* SiI9011 */
		if (SiI_Ctrl.bDevId != RX_SiI9011)
			bWCode = SiI_WC_ChipNoCap;
		break;
	case SiI_RX_P_RGB_48B:                /* SiI9021/31 */
	case SiI_RX_P_YCbCr444_48B:           /* SiI9021/31 */
		if (SiI_Ctrl.bDevId == RX_SiI9011)
			bWCode = SiI_WC_ChipNoCap;
	}
	siiWarningHandler(bWCode);
}
/*------------------------------------------------------------------------------
 * Function Name: PrepVideoPathCSCAnalogDAC
 * Function Description:  This function sets type of Video DAC (RGB vs. YCbCr)
 *----------------------------------------------------------------------------
 */
static void PrepVideoPathCSCAnalogDAC(BYTE bVideoPath,
		BYTE *pbVideoOutputFormat)
{
	pbVideoOutputFormat[1] &= (~RX_BIT_RGBA_OUT);
	pbVideoOutputFormat[2] |= RX_BIT_EN_INS_CSYNC;
	switch (bVideoPath) {
	case SiI_RX_P_RGB: /* Clear YCbCr Converter, DownScale, UpScale */
	case SiI_RX_P_RGB_2PixClk:            /* SiI9011 */
	case SiI_RX_P_RGB_48B:                /* SiI9021/31 */
		pbVideoOutputFormat[1] |= RX_BIT_RGBA_OUT;
		break;
	}

}
/*******************************************************************************
 *Transmitted color Space is ActiveColorSpace
 *
 *InputColorSpace == SiI_RX_ICP_RGB | InputColorSpace == SiI_RX_ICP_YCbCr444 |
 *    InputColorSpace == SiI_RX_ICP_YCbCr422
 *
 *In      Out   [4A:2]VID_M [4A:1]VID_M   [4A:3]VID_M [49:2]VID_M2  [49:0]VID_M2
 *Color   Color  UpScale Bit DownScale Bit RGB->YCbCr  YCbCr->RGB    RGBAout
 *Space   Space  422->444    444->422
 *
 *RGB      RGB       0          0            0            0            1
 *         444       0          0            1            0            0
 *         422       0          1            1            0            0
 *
 *444      RGB       0          0            0            1            1
 *         444       0          0            0            0            0
 *         422       0          1            0            0            0
 *
 *422      RGB       1          0            0            1            1
 *         444       1          0            0            0            0
 *         422       0          0            0            0            0
 ******************************************************************************
 */
/*------------------------------------------------------------------------------
 * Function Name: PrepVideoPathForRGBInput()
 * Function Description: Preparing Video Path's bits of Video Output Format
 *                       structure for RGB Input Color Space
 *  function sets Color Spase converters and Up or Down Sampling
 *----------------------------------------------------------------------------
 */
static void PrepVideoPathForRGBInput(BYTE bVideoPath, BYTE *pbVideoOutputFormat)
{
/* pbVideoOutputFormat:
 *                       |    7   |   6    |   5        |       4         |
 *                                 3           |    2      |   1    |   0      |
 * bit[0] 0x48 VID_CTRL  |InvVSync|InvHSync|CSyncOnVSync|CSyncOnHSync     |
 *                             enSOG           |YCbCr2RGB  |BitMode |RGB2YCbCr |
 * bit[1] 0x49 VID_MODE2 |Res     |Res     |Res         |LE Pol           |
 *                             En YCbCr2RGB Rng|EnYCbCr2RGB|En Ped. |DAC RGB   |
 * bit[2] 0x4A VID_MODE  |SyncCode|MuxYC   |Dither      |En RGB2YCbCr Rng |
 *                             En RGB2YCbCr    |UpSmpl     |DownSmpl|EnImsCSync|
 */

	pbVideoOutputFormat[2] &= (~(RX_BIT_444to422 | RX_BIT_422to444 |
		RX_BIT_DITHER | RX_BIT_MUX_YC | RX_BIT_RGBToYCbCr));
	pbVideoOutputFormat[1] &= (~RX_BIT_YCbCrToRGB);

	pbVideoOutputFormat[0] &= (~RX_BIT_EXT_BIT_MODE);

	switch (bVideoPath) {
	case SiI_RX_P_RGB: /* Clear YCbCr Converter, DownScale, UpScale */
	case SiI_RX_P_RGB_2PixClk:            /* SiI9011 */
	case SiI_RX_P_RGB_48B:                /* SiI9021/31 */
		break;
	case SiI_RX_P_YCbCr444: /* Set YCbCr Converter */
	case SiI_RX_P_YCbCr444_2PixClk:       /* SiI9011 */
	case SiI_RX_P_YCbCr444_48B:           /* SiI9021/31 */
		pbVideoOutputFormat[2] |= RX_BIT_RGBToYCbCr;
		break;
		/* Set YCbCr Converter, set DownScale
		 * 4:2:2 mode is used with digital output only
		 */
	case SiI_RX_P_YCbCr422_MUX8B:
		/* YCbCr 422 muxed 8 bit, used with digital output only */
		pbVideoOutputFormat[2] |= (RX_BIT_RGBToYCbCr | RX_BIT_444to422 |
			RX_BIT_MUX_YC | RX_BIT_DITHER);
		break;
	case SiI_RX_P_YCbCr422_MUX10B:
		/* YCbCr 422 muxed 10 bit, used with digital output only */
		pbVideoOutputFormat[2] |= (RX_BIT_RGBToYCbCr | RX_BIT_444to422 |
			RX_BIT_MUX_YC);
		break;
	case SiI_RX_P_YCbCr422_16B:
		pbVideoOutputFormat[2] |= (RX_BIT_RGBToYCbCr | RX_BIT_444to422 |
			RX_BIT_DITHER);
		break;
	case SiI_RX_P_YCbCr422:
	case SiI_RX_P_YCbCr422_20B:
		pbVideoOutputFormat[2] |= (RX_BIT_RGBToYCbCr | RX_BIT_444to422);
		break;
	case SiI_RX_P_YCbCr422_16B_2PixClk:   /* SiI9011 */
		pbVideoOutputFormat[2] |= (RX_BIT_RGBToYCbCr | RX_BIT_444to422 |
			RX_BIT_DITHER);
		break;
	case SiI_RX_P_YCbCr422_20B_2PixClk:   /* SiI9011 */
		pbVideoOutputFormat[2] |= (RX_BIT_RGBToYCbCr | RX_BIT_444to422);
		break;
	}
}
/*------------------------------------------------------------------------------
 * Function Name: PrepVideoPathForYCbCr422Input()
 * Function Description: Preparing Video Path for YCbCr 422 Input Color Space
 *----------------------------------------------------------------------------
 */
static void PrepVideoPathForYCbCr422Input(BYTE bVideoPath,
		BYTE *pbVideoOutputFormat)
{
/* pbVideoOutputFormat:
 *                       |    7   |   6    |   5        |       4         |
 *                                3           |    2      |   1    |   0      |
 * bit[0] 0x48 VID_CTRL  |InvVSync|InvHSync|CSyncOnVSync|CSyncOnHSync     |
 *                            enSOG           |YCbCr2RGB  |BitMode |RGB2YCbCr |
 * bit[1] 0x49 VID_MODE2 |Res     |Res     |Res         |LE Pol           |
 *                            En YCbCr2RGB Rng|EnYCbCr2RGB|En Ped. |DAC RGB   |
 * bit[2] 0x4A VID_MODE  |SyncCode|MuxYC   |Dither      |En RGB2YCbCr Rng |
 *                            En RGB2YCbCr    |UpSmpl     |DownSmpl|EnImsCSync|
 */
	pbVideoOutputFormat[2] &= (~(RX_BIT_444to422 | RX_BIT_422to444 |
		RX_BIT_DITHER | RX_BIT_MUX_YC | RX_BIT_RGBToYCbCr));
	pbVideoOutputFormat[1] &= (~RX_BIT_YCbCrToRGB);

	/* YMA changed as JAPAN customer reported */
	/* pbVideoOutputFormat[0]&=( ~RX_BIT_EXT_BIT_MODE ); */
	pbVideoOutputFormat[0] |= RX_BIT_EXT_BIT_MODE;

	switch (bVideoPath) {
	/* Clear YCbCr Converter, DownScale, UpScale */
	case SiI_RX_P_RGB:
	case SiI_RX_P_RGB_2PixClk:            /* SiI9011 */
	case SiI_RX_P_RGB_48B:                /* SiI9021/31 */
		pbVideoOutputFormat[2] |= RX_BIT_422to444;
		pbVideoOutputFormat[1] |= RX_BIT_YCbCrToRGB;
		break;
	/* Set YCbCr Converter, */
	case SiI_RX_P_YCbCr444:
	case SiI_RX_P_YCbCr444_2PixClk:       /* SiI9011 */
	case SiI_RX_P_YCbCr444_48B:           /* SiI9021/31 */
		pbVideoOutputFormat[2] |= RX_BIT_422to444;
		break;
	/* Set YCbCr Converter, set DownScale
	 * 4:2:2 mode is used with digital output only
	 */
	case SiI_RX_P_YCbCr422_MUX8B:
		/* YCbCr 422 muxed 8 bit, used with digital output only */
		pbVideoOutputFormat[1] |= (RX_BIT_EN_PEDESTAL);
		pbVideoOutputFormat[2] |= (RX_BIT_MUX_YC | RX_BIT_DITHER);
		break;
	case SiI_RX_P_YCbCr422_MUX10B:
		/* YCbCr 422 muxed 10 bit, used with digital output only */
		pbVideoOutputFormat[1] |= (RX_BIT_EN_PEDESTAL);
		pbVideoOutputFormat[2] |= RX_BIT_MUX_YC;
		break;
	case SiI_RX_P_YCbCr422_16B:
		pbVideoOutputFormat[2] |= RX_BIT_DITHER;
		break;
	case SiI_RX_P_YCbCr422:
	case SiI_RX_P_YCbCr422_20B:
		break;
	case SiI_RX_P_YCbCr422_16B_2PixClk:   /* SiI9011 */
		pbVideoOutputFormat[2] |= RX_BIT_DITHER;
		break;
	case SiI_RX_P_YCbCr422_20B_2PixClk:   /* SiI9011 */
		break;
	}
}
/*------------------------------------------------------------------------------
 * Function Name: PrepVideoPathForYCbCr444Input()
 * Function Description: Preparing Video Path for YCbCr 444 Input Color Space
 *----------------------------------------------------------------------------
 */
static void PrepVideoPathForYCbCr444Input(BYTE bVideoPath,
		BYTE *pbVideoOutputFormat)
{
/* pbVideoOutputFormat:
 *                       |    7   |   6    |   5        |       4         |
 *                                3           |    2      |   1    |   0      |
 * bit[0] 0x48 VID_CTRL  |InvVSync|InvHSync|CSyncOnVSync|CSyncOnHSync     |
 *                            enSOG           |YCbCr2RGB  |BitMode |RGB2YCbCr |
 * bit[1] 0x49 VID_MODE2 |Res     |Res     |Res         |LE Pol           |
 *                            En YCbCr2RGB Rng|EnYCbCr2RGB|En Ped. |DAC RGB   |
 * bit[2] 0x4A VID_MODE  |SyncCode|MuxYC   |Dither      |En RGB2YCbCr Rng |
 *                            En RGB2YCbCr    |UpSmpl     |DownSmpl|EnImsCSync|
 */
	pbVideoOutputFormat[2] &= (~(RX_BIT_422to444 | RX_BIT_DITHER |
		RX_BIT_MUX_YC | RX_BIT_RGBToYCbCr));
	pbVideoOutputFormat[1] &= (~RX_BIT_YCbCrToRGB);
	pbVideoOutputFormat[0] &= (~RX_BIT_EXT_BIT_MODE);


	switch (bVideoPath) {
	/* Clear YCbCr Converter, DownScale, UpScale */
	case SiI_RX_P_RGB:
	case SiI_RX_P_RGB_2PixClk:            /* SiI9011 */
	case SiI_RX_P_RGB_48B:                /* SiI9021/31 */
		pbVideoOutputFormat[1] |= RX_BIT_YCbCrToRGB;
		break;
	/* Set YCbCr Converter, */
	case SiI_RX_P_YCbCr444:
	case SiI_RX_P_YCbCr444_2PixClk:       /* SiI9011 */
	case SiI_RX_P_YCbCr444_48B:           /* SiI9021/31 */
		break;
	/* Set YCbCr Converter, set DownScale
	 * 4:2:2 mode is used with digital output only
	 */
	case SiI_RX_P_YCbCr422_20B:
	case SiI_RX_P_YCbCr422:
		pbVideoOutputFormat[2] |= RX_BIT_444to422;
		break;
	case SiI_RX_P_YCbCr422_MUX8B:
		/* YCbCr 422 muxed 8 bit, used with digital output only */
		pbVideoOutputFormat[1] |= (RX_BIT_EN_PEDESTAL);
		pbVideoOutputFormat[2] |= (RX_BIT_444to422 | RX_BIT_MUX_YC |
			RX_BIT_DITHER);
		break;
	case SiI_RX_P_YCbCr422_MUX10B:
		/* YCbCr 422 muxed 10 bit, used with digital output only */
		pbVideoOutputFormat[1] |= (RX_BIT_EN_PEDESTAL);
		pbVideoOutputFormat[2] |= (RX_BIT_444to422 | RX_BIT_MUX_YC);
		break;
	case SiI_RX_P_YCbCr422_16B:
		pbVideoOutputFormat[2] |= (RX_BIT_444to422 | RX_BIT_DITHER);
		break;
	case SiI_RX_P_YCbCr422_16B_2PixClk:   /* SiI9011 */
		pbVideoOutputFormat[2] |= (RX_BIT_444to422 | RX_BIT_DITHER);
		break;
	case SiI_RX_P_YCbCr422_20B_2PixClk:   /* SiI9011 */
		break;
	}
}

/*------------------------------------------------------------------------------
 * Function Name: siiPrepVideoPathSelect()
 * Function Description:  prepares Video Format Data for selected Video Format
 *
 * Accepts: BYTE Video Path Select, pointer on Video Format Data
 * Returns: Warning Byte, pointer on Video Format Data, Video Path Select Data
 *          can be modified
 * Globals: bInputColorSpace (read only)
 *----------------------------------------------------------------------------
 */
BYTE siiPrepVideoPathSelect(BYTE bVideoPathSelect, BYTE bInputColorSpace,
		BYTE *pbVidFormatData)
{
	BYTE bECode = FALSE;

	RXEXTPR("%s: bInputColorSpace=%d\n", __func__, bInputColorSpace);
	if (CheckVideoDAC_Cap()) {
		PrepVideoPathCSCAnalogDAC(bVideoPathSelect & SiI_RX_P_Mask,
			pbVidFormatData);
	}
	if (bInputColorSpace == SiI_RX_ICP_RGB) {
		RXEXTPR("%s: bInputColorSpac as RGB\n", __func__);
		PrepVideoPathForRGBInput(bVideoPathSelect & SiI_RX_P_Mask,
			pbVidFormatData);
	} else if (bInputColorSpace == SiI_RX_ICP_YCbCr422) {
		RXEXTPR("%s: bInputColorSpace as YUV422\n", __func__);
		PrepVideoPathForYCbCr422Input(bVideoPathSelect & SiI_RX_P_Mask,
			pbVidFormatData);
	} else if (bInputColorSpace == SiI_RX_ICP_YCbCr444) {
		RXEXTPR("%s: bInputColorSpace as YUV444\n", __func__);
		PrepVideoPathForYCbCr444Input(bVideoPathSelect & SiI_RX_P_Mask,
			pbVidFormatData);
	} else {
		RXEXTERR("%s: bInputColorSpace error\n", __func__);
		bECode = SiI_EC_InColorSpace;
	}

	CheckVidPathForDevCap(bVideoPathSelect);

	return bECode;
}
/*------------------------------------------------------------------------------
 * Function Name: siiPrepSyncSelect()
 * Function Description: prepares Sync Select data
 *
 * Accepts: BYTE Sync Select, pointer on Video Format Data
 * Returns: Warning byte, pointer on Video Format Data, sync select data can
 *          be modified
 * Globals: none
 *----------------------------------------------------------------------------
 */
BYTE siiPrepSyncSelect(BYTE bSyncSelect, BYTE *pbVidFormatData)
{
	BYTE bECode = FALSE;
	BYTE bWCode = FALSE;
/* pbVideoOutputFormat:
 *                       |    7   |   6    |   5        |       4         |
 *                                3           |    2      |   1    |   0      |
 * bit[0] 0x48 VID_CTRL  |InvVSync|InvHSync|CSyncOnVSync|CSyncOnHSync     |
 *                            enSOG           |YCbCr2RGB  |BitMode |RGB2YCbCr |
 * bit[1] 0x49 VID_MODE2 |Res     |Res     |Res         |LE Pol           |
 *                            En YCbCr2RGB Rng|EnYCbCr2RGB|En Ped. |DAC RGB   |
 * bit[2] 0x4A VID_MODE  |SyncCode|MuxYC   |Dither      |En RGB2YCbCr Rng |
 *                            En RGB2YCbCr    |UpSmpl     |DownSmpl|EnImsCSync|
 */

	pbVidFormatData[0] &= (~(RX_BIT_ENCSYNC_ON_HSYNC |
		RX_BIT_ENCSYNC_ON_VSYNC | RX_BIT_INSERT_CSYNC_ON_AOUT));
	pbVidFormatData[2] &= (~RX_BIT_INS_SAVEAV);
	switch (bSyncSelect) {
	case SiI_RX_SS_SeparateSync:
		break;
	case SiI_RX_SS_CompOnH:
		pbVidFormatData[0] |= RX_BIT_ENCSYNC_ON_HSYNC;
		break;
	case SiI_RX_SS_CompOnV:
		pbVidFormatData[0] |= RX_BIT_ENCSYNC_ON_VSYNC;
		break;
	case SiI_RX_SS_EmbSync:
		pbVidFormatData[2] |= RX_BIT_INS_SAVEAV;
		break;
	case SiI_RX_AVC_SOGY:
		pbVidFormatData[0] |= RX_BIT_INSERT_CSYNC_ON_AOUT;
		if (SiI_Ctrl.bDevId == RX_SiI9011)
			bWCode = SiI_WC_ChipNoCap;
		break;
	}
	siiWarningHandler(bWCode);

	return bECode;
}
/*------------------------------------------------------------------------------
 * Function Name: siiPrepSyncCtrl()
 * Function Description: parepares control of sync
 *
 * Accepts: byte Sync. control, and Video Format Data
 * Returns: Warning byte, pointer on Video Format Data, sync control data can
 *          be modified
 * Globals: none
 *----------------------------------------------------------------------------
 */
BYTE siiPrepSyncCtrl(BYTE bSyncCtrl, BYTE *pbVidFormatData)
{
	BYTE bECode = FALSE;
/* pbVidOutputFormat:
 *                       |    7   |   6    |   5        |       4         |
 *                                3           |    2      |   1    |   0      |
 * bit[0] 0x48 VID_CTRL  |InvVSync|InvHSync|CSyncOnVSync|CSyncOnHSync     |
 *                            enSOG           |YCbCr2RGB  |BitMode |RGB2YCbCr |
 * bit[1] 0x49 VID_MODE2 |Res     |Res     |Res         |LE Pol           |
 *                            En YCbCr2RGB Rng|EnYCbCr2RGB|En Ped. |DAC RGB   |
 * bit[2] 0x4A VID_MODE  |SyncCode|MuxYC   |Dither      |En RGB2YCbCr Rng |
 *                            En RGB2YCbCr    |UpSmpl     |DownSmpl|EnImsCSync|
 */
	pbVidFormatData[0] &= (~(RX_BIT_INVERT_HSYNC | RX_BIT_INVERT_VSYNC));
	if (bSyncCtrl & SiI_RX_SC_InvH)
		pbVidFormatData[0] |= RX_BIT_INVERT_HSYNC;
	if (bSyncCtrl & SiI_RX_SC_InvV)
		pbVidFormatData[0] |= RX_BIT_INVERT_VSYNC;

	return bECode;
}
/*------------------------------------------------------------------------------
 * Function Name: siiPrepVideoCtrl()
 * Function Description: Sets Enable or Disable pedestal
 *
 * Accepts: pointer on Video Format Data
 * Returns: Warning code, if wrong parameter, Modified Video Format Data
 * Globals: none
 *----------------------------------------------------------------------------
 */
BYTE siiPrepVideoCtrl(BYTE bVideoOutCtrl, BYTE *pbVidFormatData)
{
	BYTE bECode = FALSE;
	BYTE bWCode = FALSE;
/* pbVidOutputFormat:
 *                       |    7   |   6    |   5        |       4         |
 *                                3           |    2      |   1    |   0      |
 * bit[0] 0x48 VID_CTRL  |InvVSync|InvHSync|CSyncOnVSync|CSyncOnHSync     |
 *                            enSOG           |YCbCr2RGB  |BitMode |RGB2YCbCr |
 * bit[1] 0x49 VID_MODE2 |Res     |Res     |Res         |LE Pol           |
 *                            En YCbCr2RGB Rng|EnYCbCr2RGB|En Ped. |DAC RGB   |
 * bit[2] 0x4A VID_MODE  |SyncCode|MuxYC   |Dither      |En RGB2YCbCr Rng |
 *                            En RGB2YCbCr    |UpSmpl     |DownSmpl|EnImsCSync|
 */

	if (bVideoOutCtrl == SiI_RX_AVC_Pedestal)
		pbVidFormatData[1] |= RX_BIT_EN_PEDESTAL;
	else if (bVideoOutCtrl == SiI_RX_AVC_NoPedestal)
		pbVidFormatData[1] &= (~RX_BIT_EN_PEDESTAL);

	if ((SiI_Ctrl.bDevId == RX_SiI9011) &&
		(bVideoOutCtrl != SiI_RX_AVC_Digital_Output))
		bWCode = SiI_WC_ChipNoCap;

	if (bVideoOutCtrl == SiI_RX_AVC_Digital_Output)
		sii_SetVideoOutputPowerDown(SiI_RX_VidOutPD_Analog);
	else
		sii_SetVideoOutputPowerDown(SiI_RX_VidOutPD_Digital);

	siiWarningHandler(bWCode);
	return bECode;
}

/*------------------------------------------------------------------------------
 * Function Name: Configure2PixPerClockMode()
 * Function Description: Function turns on or off 2 Pixels per clock mode
 *----------------------------------------------------------------------------
 */
static void Configure2PixPerClockMode(BOOL On)
{
	if (On) {
		RXEXTPR("%s: on\n", __func__);
		siiIIC_RX_RWBitsInByteP0(RX_SYS_CTRL1_ADDR,
			RX_BIT_2PIX_MODE, SET);
		/* siiIIC_RX_RWBitsInByteP1(RX_PD_SYS2_ADDR,
		 *	RX_BIT_PD_QE, SET);
		 */
	} else {
		RXEXTPR("%s: off\n", __func__);
		siiIIC_RX_RWBitsInByteP0(RX_SYS_CTRL1_ADDR,
			RX_BIT_2PIX_MODE, CLR);
		/* siiIIC_RX_RWBitsInByteP1(RX_PD_SYS2_ADDR,
		 *	RX_BIT_PD_QE, CLR);
		 */
	}

}

/*------------------------------------------------------------------------------
 * Function Name: SetInputOutputDivider
 * Function Description: Function Sets Input and Output pixel clock divider
 *----------------------------------------------------------------------------
 */
static void SetInputOutputDivider(BOOL qMode48BitOr2PixClk,
		BOOL qRestoreOrigPixClock, BYTE PixRepl, BOOL mux_mode)
{
	BYTE bRegVal;

	SiI_Ctrl.bShadowPixRepl = PixRepl;
	bRegVal = siiReadByteHDMIRXP0(RX_SYS_CTRL1_ADDR);
	RXEXTPR("%s: RX_SYS_CTRL1_ADDR(0x%02x)=0x%02x\n",
		__func__, RX_SYS_CTRL1_ADDR, bRegVal);

	/* clear bits of input and output devider */
	bRegVal &= RX_MSK_CLR_IN_OUT_DIV;

	/* Set bits for input divider equal Pixel Replication */
	bRegVal |= (PixRepl << 4);

	/* in YCbCr422Mux mode output clock should be doubled */
	if (mux_mode) {
		PixRepl += 1;
		PixRepl >>= 1; /* divide by 2 */
		if (PixRepl)
			PixRepl -= 1;
	}

	if ((!qRestoreOrigPixClock) && qMode48BitOr2PixClk) {
		bRegVal |= RX_OUTPUT_DIV_BY_2;
	} else if (qRestoreOrigPixClock && (!qMode48BitOr2PixClk)) {
		/*Set bits for output divider to restore original pixel clock*/
		bRegVal |= (PixRepl << 6);
	} else if (qRestoreOrigPixClock && qMode48BitOr2PixClk) {
		if (PixRepl == SiI_PixRepl1)
			bRegVal |= RX_OUTPUT_DIV_BY_2;
		else if (PixRepl == SiI_PixRepl2)
			bRegVal |= RX_OUTPUT_DIV_BY_4;
		else
			siiWarningHandler(SiI_WC_ChipNoCap);
	} else {
		if ((PixRepl == 0) && mux_mode)
			bRegVal |= 0x10;
	}

	siiWriteByteHDMIRXP0(RX_SYS_CTRL1_ADDR, bRegVal);
	RXEXTPR("%s: RX_SYS_CTRL1_ADDR(0x%02x)=0x%02x\n",
		__func__, RX_SYS_CTRL1_ADDR, bRegVal);

}
/*------------------------------------------------------------------------------
 * Function Name: siiSetVidResDependentVidOutputVormat
 * Function Description: Functions sets Video Resolution dependent parameters:
 * Pix Replication Divider, Output Divider
 * Accepts: BYTE Pixel Replication Rate, BYTE Video Path Select
 * Returns: Warning code, if wrong parameter
 * Globals: none
 *----------------------------------------------------------------------------
 */

void siiSetVidResDependentVidPath(BYTE bPixRepl, BYTE bVideoPath)
{
	BOOL qRestoreOrgPixClock = TRUE;
	BYTE bRegVal;

	if (bVideoPath & SiI_RX_P_S_PassReplicatedPixels)
		qRestoreOrgPixClock = FALSE;
	switch (bVideoPath) {
	case SiI_RX_P_RGB_2PixClk:            /* SiI9011 */
	case SiI_RX_P_YCbCr444_2PixClk:       /* SiI9011 */
	case SiI_RX_P_YCbCr422_16B_2PixClk:   /* SiI9011 */
	case SiI_RX_P_YCbCr422_20B_2PixClk:   /* SiI9011 */
		if (SiI_Ctrl.bDevId == RX_SiI9011) {
			Configure2PixPerClockMode(ON);
			SetInputOutputDivider(ON/*On 48bit(LE) or 2Pix*/,
				qRestoreOrgPixClock, bPixRepl, false);
		} else
			siiWarningHandler(SiI_WC_ChipNoCap);
		break;
	case SiI_RX_P_YCbCr422_MUX8B: /* RX_SiI9135 */
	case SiI_RX_P_YCbCr422_MUX10B:
		if (SiI_Ctrl.bDevId == RX_SiI9135) {
			/* Configure2PixPerClockMode(ON); */
			SetInputOutputDivider(OFF/*On 48bit(LE) or 2Pix*/,
				FALSE, bPixRepl, true);
			if ((0 == bPixRepl || 1 == bPixRepl)) {
				/* ITU 601 YC MUX */
				siiWriteByteHDMIRXP0(0x5F, 0xE0);
				/* siiWriteByteHDMIRXP0(0x81, 0x02); */
				bRegVal = siiReadByteHDMIRXP0(0x81);
				bRegVal = (bRegVal & ~(0xf)) | 0x02;
				siiWriteByteHDMIRXP0(0x81, bRegVal);
				RXEXTPR("%s: write 0x81\n", __func__);
			}
			/* modified by zhangyue for debug YC Mux end */
			RXEXTPR("%s: bPixRepl=%d\n", __func__, bPixRepl);
		} else {
			siiWarningHandler(SiI_WC_ChipNoCap);
		}
		break;
	case SiI_RX_P_RGB_48B:                /* SiI9021/31 */
	case SiI_RX_P_YCbCr444_48B:           /* SiI9021/31 */
		SetInputOutputDivider(ON/*On 48bit(LE) or 2Pix*/,
			qRestoreOrgPixClock, bPixRepl, false);
		if (SiI_Ctrl.bDevId == RX_SiI9011)
			siiWarningHandler(SiI_WC_ChipNoCap);
		break;
	default:
		SetInputOutputDivider(OFF/*Off 48bit (LE) and 2Pix*/,
			qRestoreOrgPixClock, bPixRepl, false);
		if (SiI_Ctrl.bDevId == RX_SiI9011)
			Configure2PixPerClockMode(OFF);
		break;
	}
}

/*------------------------------------------------------------------------------
 * Function Name:   siiMuteVideo
 * Function Description: Reads Audio Path, modifys Video Mute bit
 *
 * Accepts: BYTE Video Mute Control
 * Returns: Err or Warning code
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiMuteVideo(BYTE On)
{
	if (On) {
		siiIIC_RX_RWBitsInByteP1(RX_AUDP_MUTE_ADDR,
			RX_BIT_VIDEO_MUTE, SET);
		SiI_Inf.bGlobStatus |= SiI_RX_Global_VMute;
	} else {
		siiIIC_RX_RWBitsInByteP1(RX_AUDP_MUTE_ADDR,
			RX_BIT_VIDEO_MUTE, CLR);
		SiI_Inf.bGlobStatus &= (~SiI_RX_Global_VMute);
	}
}

