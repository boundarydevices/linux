/*------------------------------------------------------------------------------
 * Module Name: SiIInfoPkts
 *
 * Module Description: Processing Info Frame/Packets Interrupts
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
#include "SiIRXIO.h"
#include "SiITTVideo.h"
#include "SiIRXAPIDefs.h"
#include "SiITrace.h"
#include "SiIGlob.h"
#include "../hdmirx_ext_drv.h"

/*------------------------------------------------------------------------------
 * Function Name: Check_CheckSum
 * Function Description: checks Info Frame Check Sum
 *----------------------------------------------------------------------------
 */
static BOOL Check_CheckSum(BYTE bTotInfoFrameLength, BYTE *pbInfoData)
{
	BOOL qResult = FALSE;
	BYTE i, bCheckSum = 0;

	for (i = 0; i < bTotInfoFrameLength; i++)
		bCheckSum += pbInfoData[i];
	if (!bCheckSum)
		qResult = TRUE;

	return qResult;

}
/*------------------------------------------------------------------------------
 * Function Name: GetPacketTypeAndAddr
 * Function Description: used to get Info Frame/Packet type and address
 *----------------------------------------------------------------------------
 */
static struct InfoFrPktType_s GetPacketTypeAndAddr(BYTE bMaskedIntrrupt)
{
	struct InfoFrPktType_s InfoFrmPkt;
	/*YMA revised as by Japan customer
	 *
	 *switch (bMaskedIntrrupt) {
	 *
	 *case AVI_Mask:      InfoFrmPkt.bAddr = RX_AVI_IF_ADDR;
	 *	break;
	 *case SPD_Mask:      InfoFrmPkt.bAddr = RX_SPD_IF_ADDR;
	 *	break;
	 *case Audio_Mask:    InfoFrmPkt.bAddr = RX_AUD_IF_ADDR;
	 *	break;
	 *case MPEG_Mask:     InfoFrmPkt.bAddr = RX_MPEG_IF_ADDR;
	 *	break;
	 *case Unknown_Mask:  InfoFrmPkt.bAddr = RX_UNKNOWN_IF_ADDR;
	 *	break;
	 *case ACP_Mask:      InfoFrmPkt.bAddr = RX_ACP_IP_ADDR;
	 *	break;
	 *default:   InfoFrmPkt.bType = 0;
	 *}
	 *InfoFrmPkt.bType = siiReadByteHDMIRXP1( InfoFrmPkt.bAddr );
	 *return InfoFrmPkt;
	 */
	InfoFrmPkt.bType = 0;
	InfoFrmPkt.bAddr = 0;

	switch (bMaskedIntrrupt) {
	case AVI_Mask:
		InfoFrmPkt.bAddr = RX_AVI_IF_ADDR;
		break;
	case SPD_Mask:
		InfoFrmPkt.bAddr = RX_SPD_IF_ADDR;
		break;
	case Audio_Mask:
		InfoFrmPkt.bAddr = RX_AUD_IF_ADDR;
		break;
	case MPEG_Mask:
		InfoFrmPkt.bAddr = RX_MPEG_IF_ADDR;
		break;
	case Unknown_Mask:
		InfoFrmPkt.bAddr = RX_UNKNOWN_IF_ADDR;
		break;
	case ACP_Mask:
		InfoFrmPkt.bAddr = RX_ACP_IP_ADDR;
		break;
	}
	if (InfoFrmPkt.bAddr)
		InfoFrmPkt.bType = siiReadByteHDMIRXP1(InfoFrmPkt.bAddr);
	return InfoFrmPkt;

}
/*------------------------------------------------------------------------------
 * Function Name: CheckForInfoFrameLength
 * Function Description: check if Info Frame length in normal range
 *----------------------------------------------------------------------------
 */
static BOOL CheckForInfoFrameLength(BYTE bInfoFrameLenth)
{
	BOOL qResult = FALSE;

	if ((bInfoFrameLenth >= IF_MIN_LENGTH) &&
		(bInfoFrameLenth <= IF_MAX_LENGTH))
		qResult = TRUE;
	return qResult;

}
/*------------------------------------------------------------------------------
 * Function Name:  siiClearNewPacketEvent
 * Function Description: After reading Packet through API interface, it's need
 *                       to clear that Event of packet. Otherwise top layer
 *                       would read it again
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiClearNewPacketEvent(BYTE bPaketType)
{
	switch (bPaketType) {
	case AVI_Type:
		SiI_Inf.bNewInfoPkts &= (~SiI_RX_NewInfo_AVI);
		break;
	case SPD_Type:
		SiI_Inf.bNewInfoPkts &= (~SiI_RX_NewInfo_SPD);
		break;
	case Audio_Type:
		SiI_Inf.bNewInfoPkts &= (~SiI_RX_NewInfo_AUD);
		break;
	case ISRC1_Type:
		SiI_Inf.bNewInfoPkts &= (~SiI_RX_NewInfo_ISRC1);
		break;
	case ISRC2_Type:
		SiI_Inf.bNewInfoPkts &= (~SiI_RX_NewInfo_ISRC2);
		break;
	case ACP_Type:
		SiI_Inf.bNewInfoPkts &= (~SiI_RX_NewInfo_ACP);
		break;
	}
}
/*------------------------------------------------------------------------------
 * Function Name: GetInfoFramesOrPakets
 * Function Description: gets Info Frames or Packets,
 *                       if Info Frame check for its length and check sum
 *----------------------------------------------------------------------------
 */
static BYTE GetInfoFramesOrPakets(struct InfoFrPktType_s InfoFrPkt,
		BYTE *pbInfoData)
{
	BYTE bECode = FALSE, bMaxNBytes = 0;
	BOOL qInfoFrame = TRUE;

	switch (InfoFrPkt.bType) {
	case AVI_Type:
		bMaxNBytes = 19;
		break;
	case MPEG_Type:
	case SPD_Type:
		bMaxNBytes = 31;
		break;
	case Audio_Type:
		bMaxNBytes = 14;
		break;
	case ISRC1_Type:
	case ISRC2_Type:
	case ACP_Type:
		qInfoFrame = FALSE; bMaxNBytes = 31;
		break;
	}

	siiReadBlockHDMIRXP1(InfoFrPkt.bAddr, bMaxNBytes, pbInfoData);

	if (qInfoFrame) {
		if (CheckForInfoFrameLength(pbInfoData[IF_LENGTH_INDEX])) {
			if (!Check_CheckSum(
				pbInfoData[IF_LENGTH_INDEX] + IF_HEADER_LENGTH,
				pbInfoData))
				bECode = SiI_EC_InfoFrameWrongCheckSum;
		} else
			bECode = SiI_EC_InfoFrameWrongLength;
	}

	return bECode;
	/* TODO think how differentiate from another errors
	 *( because getting these errors is a source fault)
	 */
}
/*------------------------------------------------------------------------------
 * Function Name: GetInfoPacketAddress
 * Function Description:  returns address in SiI RX where Packet data
 *                        were placed
 *----------------------------------------------------------------------------
 */
BYTE GetInfoPacketAddress(BYTE bType)
{
	BYTE bAddr;

	switch (bType) {
	case AVI_Type:
		bAddr = RX_AVI_IF_ADDR;
		break;
	case SPD_Type:
		bAddr = RX_SPD_IF_ADDR;
		break;
	case Audio_Type:
		bAddr = RX_AUD_IF_ADDR;
		break;
	case ISRC1_Type:
		bAddr = RX_MPEG_IF_ADDR;
		break;
	case ISRC2_Type:
		bAddr = RX_UNKNOWN_IF_ADDR;
		break;
	case ACP_Type:
		bAddr = RX_ACP_IP_ADDR;
		break;
	default:
		bAddr = 0;
	}
	return bAddr;
}
/*------------------------------------------------------------------------------
 * Function Name: siiGetInfoPacket
 * Function Description: get packet from
 *
 * Accepts: BYTE InfoPacketType
 * Returns: BYTE *, InfoPacketData
 * Globals: none
 *----------------------------------------------------------------------------
 */
void siiGetInfoPacket(BYTE bInfoPacketType, BYTE *pbInfoPacket)
{
	BYTE bInfoPacketAddress;
	struct InfoFrPktType_s InfoFrmPkt;

	bInfoPacketAddress = GetInfoPacketAddress(bInfoPacketType);
	if (bInfoPacketAddress) {
		InfoFrmPkt.bType = bInfoPacketType;
		InfoFrmPkt.bAddr = bInfoPacketAddress;
		GetInfoFramesOrPakets(InfoFrmPkt, pbInfoPacket);
	}

}
/*------------------------------------------------------------------------------
 * Function Name: siiGetVIC
 * Function Description: get VIC code from AVI packet
 *
 * Accepts: void
 * Returns: BYTE
 * Globals: none
 *----------------------------------------------------------------------------
 */
BYTE siiGetVIC(void)
{
	return siiReadByteHDMIRXP1(RX_AVI_IF_ADDR + 7);
}
/*------------------------------------------------------------------------------
 * Function Name:  SetAVI_Info
 * Function Description:  fill in AVI information in SiI_Inf global strucute
 *----------------------------------------------------------------------------
 */
static void SetAVI_Info(struct AVIType_s *AVI, BYTE *Data)
{
	BYTE bAVI;

	AVI->bAVI_State = SiI_RX_GotAVI;

	SiI_Inf.bGlobStatus &= (~SiI_RX_GlobalHDMI_NoAVIPacket);

	bAVI = Data[IF_HEADER_LENGTH] & ColorSpaceMask;
	if (bAVI == RGB)
		AVI->bInputColorSpace = SiI_RX_ICP_RGB;
	else if (bAVI == (BYTE)YCbCr422)
		AVI->bInputColorSpace = SiI_RX_ICP_YCbCr422;
	else if (bAVI == (BYTE)YCbCr444)
		AVI->bInputColorSpace = SiI_RX_ICP_YCbCr444;

	bAVI = Data[IF_HEADER_LENGTH + 1] & ColorimetryMask;
	if (bAVI == NoInfo)
		AVI->bColorimetry = SiI_RX_ColorimetryNoInfo;
	else if (bAVI == (BYTE)ITU601)
		AVI->bColorimetry = SiI_RX_ITU_601;
	else if (bAVI == (BYTE)ITU709)
		AVI->bColorimetry = SiI_RX_ITU_709;

	AVI->bPixRepl = Data[IF_HEADER_LENGTH + 4] & PixReplicationMask;


}
/*------------------------------------------------------------------------------
 * Function Name: ClearAVI_Info
 * Function Description: write default AVI information in SiI_Inf global
 *                       strucute
 *----------------------------------------------------------------------------
 */
void siiClearAVI_Info(struct AVIType_s *AVI)
{
	SiI_Inf.bGlobStatus |= SiI_RX_GlobalHDMI_NoAVIPacket;
	SiI_Inf.bNewInfoPkts &= (~SiI_RX_NewInfo_AVI);
	AVI->bAVI_State = SiI_RX_NoAVI;
	AVI->bInputColorSpace = SiI_RX_ICP_RGB;
	AVI->bColorimetry = SiI_RX_ColorimetryNoInfo;
	AVI->bPixRepl = 0;

}
/*------------------------------------------------------------------------------
 * Function Name: ProcessAVIorNoAVI
 * Function Description: processing AVI and NoAVI interrupts
 *----------------------------------------------------------------------------
 */
static void ProcessAVIorNoAVI(BYTE bInfoFramesIntrrupts)
{
	BYTE abInfoData[19];
	BYTE bECode;
	struct InfoFrPktType_s InfoFrmPkt;

	if (bInfoFramesIntrrupts & AVI_Mask) {
		InfoFrmPkt = GetPacketTypeAndAddr(
			bInfoFramesIntrrupts & AVI_Mask);
		if (InfoFrmPkt.bType) {
			bECode = GetInfoFramesOrPakets(InfoFrmPkt, abInfoData);
			if (!bECode) {
				SetAVI_Info(&SiI_Inf.AVI, abInfoData);
				/* Request Program Video path */
				siiMuteVideoAndSetSM_SyncInChange();
				/* Set Sys Information Flag */
				SiI_Inf.bNewInfoPkts |= SiI_RX_NewInfo_AVI;
#ifdef SII_DUMP_UART
				printf("\nNew AVI\n");
#endif
			}
		}
	}
	if (bInfoFramesIntrrupts &  NoAVI_Mask) {
		siiClearAVI_Info(&SiI_Inf.AVI);
		siiMuteVideoAndSetSM_SyncInChange();
		SiI_Inf.bNewInfoPkts &= (~SiI_RX_NewInfo_AVI);
	}
}
/*------------------------------------------------------------------------------
 * Function Name: ProcessAudioInfoFrame
 * Function Description: processing Audio Info Frame
 *----------------------------------------------------------------------------
 */
static void ProcessAudioInfoFrame(BYTE bInfoFramesIntrrupts)
{
	BYTE bECode;
	BYTE abInfoData[14];
	struct InfoFrPktType_s InfoFrmPkt;

	InfoFrmPkt = GetPacketTypeAndAddr(bInfoFramesIntrrupts & AUD_MASK);
	if (InfoFrmPkt.bType) {
		bECode = GetInfoFramesOrPakets(InfoFrmPkt, abInfoData);
		if (!bECode) {
			/* Set Sys Information Flag */
			SiI_Inf.bNewInfoPkts |= SiI_RX_NewInfo_AUD;
			/* TODO Program input audio status */
		}
	}
}
/*------------------------------------------------------------------------------
 * Function Name: Check InfoPacket Buffer
 * Function Description: This function checks info packet buffer,
 * sets corresponding flag in SiI_Inf.bNewInfoPkts
 *----------------------------------------------------------------------------
 */
static void CheckIPBuffer(BYTE bMaskedIPIntrrupt)
{
	struct InfoFrPktType_s InfoFrmPkt;

	InfoFrmPkt = GetPacketTypeAndAddr(bMaskedIPIntrrupt);
	if (InfoFrmPkt.bType == SPD_Type)
		SiI_Inf.bNewInfoPkts |= SiI_RX_NewInfo_SPD;
	else if (InfoFrmPkt.bType == ISRC1_Type)
		SiI_Inf.bNewInfoPkts |= SiI_RX_NewInfo_ISRC1;
	else if (InfoFrmPkt.bType == ISRC2_Type)
		SiI_Inf.bNewInfoPkts |= SiI_RX_NewInfo_ISRC2;
	else
		SiI_Inf.bNewInfoPkts |= SiI_RX_NewInfo_UNREQ;
}

/*------------------------------------------------------------------------------
 * Function Name:  siiProcessInfoFrameIntrrupts
 * Function Description: processing Info Frame and Info Packet Interrupts
 *
 * Accepts: none
 * Returns: none
 * Globals: SiI_Inf.AVI structure can be modified
 *----------------------------------------------------------------------------
 */
void siiProcessInfoFrameIntrrupts(BYTE bInfoFramesIntrrupts)
{
	if ((bInfoFramesIntrrupts & NoAVI_Mask) ||
		(bInfoFramesIntrrupts & AVI_Mask)) {
		/* only AVI in AVI IF Buffere */
		ProcessAVIorNoAVI(bInfoFramesIntrrupts);
	}
	if (bInfoFramesIntrrupts & Audio_Mask) {
		/* only Audio in AVI IF Buffere */
		ProcessAudioInfoFrame(bInfoFramesIntrrupts);
	}
	if (bInfoFramesIntrrupts & SPD_Mask)
		CheckIPBuffer(bInfoFramesIntrrupts & SPD_Mask);

	if (bInfoFramesIntrrupts & MPEG_Mask)
		CheckIPBuffer(bInfoFramesIntrrupts & MPEG_Mask);

	if (bInfoFramesIntrrupts & Unknown_Mask)
		CheckIPBuffer(bInfoFramesIntrrupts & Unknown_Mask);

}

