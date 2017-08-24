/*------------------------------------------------------------------------------
 * Copyright © 2002-2005, Silicon Image, Inc.  All rights reserved.
 *
 * No part of this work may be reproduced, modified, distributed, transmitted,
 * transcribed, or translated into any language or computer format, in any form
 * or by any means without written permission of: Silicon Image, Inc.,
 * 1060 East Arques Avenue, Sunnyvale, California 94085
 *----------------------------------------------------------------------------
 */

#include "SiITypeDefs.h"
#include "SiIGlob.h"

#define AVI_MASK 0x01
#define SP_MASK 0x02
#define AUD_MASK 0x04
#define MPEG_MASK 0x08
#define UNKNOWN_MASK 0x10
#define CP_MASK  0x80

/* ---------------------------------- */
#define RX_AVI_IF_ADDR 0x40
#define RX_SPD_IF_ADDR  0x60
#define RX_AUD_IF_ADDR 0x80
#define RX_MPEG_IF_ADDR 0xA0
#define RX_UNKNOWN_IF_ADDR 0xC0
#define RX_ACP_IP_ADDR 0xE0

#define BIT_CP_AVI_MUTE_SET    0x01
#define BIT_CP_AVI_MUTE_CLEAR  0x10

#define RX_SPD_DEC_ADDR        0x7F
#define RX_MPEG_DEC_ADDR       0xBF
#define RX_ACP_DEC_ADDR        0xFF
#define RX_ACP_INFO_PKT_ADDR   0xE0

#define IF_HEADER_LENGTH 4
#define IF_LENGTH_INDEX 2
#define IF_MAX_LENGTH 27
#define IF_MIN_LENGTH 10

/* ----------------------------------- */
enum SiI_InfoInterruptMask {

	AVI_Mask =      0x01,
	SPD_Mask =      0x02,
	Audio_Mask =    0x04,
	MPEG_Mask =     0x08,
	Unknown_Mask =  0x10,
	NoAVI_Mask =    0x20,
	ACP_Mask =      0x40,
	CP_Mask =       0x80
};


enum SiI_InfoFramePackets {

	AVI_Type = 0x82,
	SPD_Type = 0x83,
	Audio_Type = 0x84,
	MPEG_Type = 0x85,
	ISRC1_Type = 0x05,
	ISRC2_Type = 0x06,
	ACP_Type = 0x04

};

struct InfoFrPktType_s {

	BYTE bType;
	BYTE bAddr;

};

enum AVI_Encoding {

	ColorSpaceMask = 0x60,
	ColorimetryMask = 0xC0,
	PixReplicationMask = 0x0F

};

enum ColorSpace {

	RGB = 0x00,
	YCbCr422 = 0x20,
	YCbCr444 = 0x40,

};

enum Colorimetry {

	NoInfo,
	ITU601 = 0x40,
	ITU709 = 0x80,

};

void siiProcessInfoFrameIntrrupts(BYTE bInfoFramesIntrrupts);
void siiGetInfoPacket(BYTE bInfoPacketType, BYTE *pbInfoPacket);
void siiClearNewPacketEvent(BYTE bPaketType);
BYTE siiGetVIC(void);
void siiClearAVI_Info(struct AVIType_s *AVI);

