/*------------------------------------------------------------------------------
 * Copyright © 2002-2005, Silicon Image, Inc.  All rights reserved.
 *
 * No part of this work may be reproduced, modified, distributed, transmitted,
 * transcribed, or translated into any language or computer format, in any form
 * or by any means without written permission of: Silicon Image, Inc.,
 * 1060 East Arques Avenue, Sunnyvale, California 94085
 *----------------------------------------------------------------------------
 */

#ifndef _SII_VID_RES_
#define _SII_VID_RES_

#include "SiITypeDefs.h"
#include "SiIRXAPIDefs.h"

struct SyncInfoType_s {

	WORD ClocksPerLine;
	WORD HTot;
	WORD VFreq;
	BYTE RefrTypeVHPol;
	BYTE bFPix;

};


/*------------------------------------------------------------------*/
struct ModeIdType_s {
	BYTE Mode_C1;
	BYTE Mode_C2;
	BYTE SubMode;
};
/*------------------------------------------------------------------*/
struct PxlLnTotalType_s {
	WORD Pixels;
	WORD Lines;
};
/*------------------------------------------------------------------*/
struct HVPositionType_s {
	WORD H;
	WORD V;
};
/*------------------------------------------------------------------*/
struct HVResolutionType_s {
	WORD H;
	WORD V;
};
/*------------------------------------------------------------------*/
struct TagType_s {
	BYTE RefrTypeVHPol;
	WORD VFreq;
	struct PxlLnTotalType_s Total;
};
/*------------------------------------------------------------------*/
struct _656Type_s {
	BYTE IntAdjMode;
	WORD HLength;
	WORD VLength;
	WORD Top;
	WORD Dly;
	WORD HBit2HSync;
	WORD VBit2VSync;
	WORD Field2Offset;

};
/*------------------------------------------------------------------*/
struct VModeInfoType_s {
	struct ModeIdType_s ModeId;
	BYTE PixClk;
	struct TagType_s Tag;
	struct HVResolutionType_s Res;
	BYTE PixRepl;
};

/*------------------------------------------------------------------*/
struct InterlaceCSyncType_s {

	WORD CapHalfLineCnt;
	WORD PreEqPulseCnt;
	WORD SerratedCnt;
	WORD PostEqPulseCnt;
	BYTE Field2BackPorch;

};

/*                  Aspect ratio */
#define _4     0  /* 4:3 */
#define _4or16 1  /* 4:3 or 16:9 */
#define _16    2  /* 16:9 */

#define NSM    0 /* no sub. mode */


#define VGA 0

#ifdef SII_PCMODES

#ifdef SII_861C_MODES
#define PC_BASE (28 + 15)
#define NMODES (PC_BASE + 54)        /* CEA 861C + PC Video Modes */

#else
#define PC_BASE 28
#define NMODES (PC_BASE + 54)        /* CEA 861B + PC Video Modes */

#endif

#else        /* CEA modes only */

#ifndef SII_861C_MODES    /* only CEA 861B are included */

#define PC_BASE 28
#define NMODES 28           /* CEA 861 B Video Modes */

#endif

#ifdef SII_861C_MODES

#define PC_BASE (28 + 15)
#define NMODES PC_BASE          /* CEA 861 C Video Modes */
#endif

#endif

#define _480i_60Hz      4
#define _576i_50Hz      19


#define ProgrVPosHPos 0x03
#define ProgrVNegHPos 0x02
#define ProgrVPosHNeg 0x01
#define ProgrVNegHNeg 0x00
#define InterlaceVPosHPos 0x07
#define InterlaceVNegHPos 0x06
#define InterlaceVPosHNeg 0x05
#define InterlaceVNegHNeg 0x04
#define SiI_InterlaceMask 0x04

#ifdef SII_861C_MODES

#define H_TOLERANCE 50      /* +- 500 Hz */
#define V_TOLERANCE 400     /* +- 4.0 Hz */
#define LINES_TOLERANCE     15
#define PIXELS_TOLERANCE    4
#define CLK_TOLERANCE       5
#define CHIP_OSC_CLK_MUL128 (3625216000) /* *1000 MHz */


#define INTERLACE_VFREQ_LOW_LIMIT       2500    /* 25 Hz */
#define PROGRESSIVE_VFREQ_LOW_LIMIT     2300    /* 23 Hz */
#define INTERLACE_HFREQ_LOW_LIMIT       1500    /* 15 kHz */
#define PROGRESSIVE_HFREQ_LOW_LIMIT     1500    /* 15 kHz */
#define INTERLACE_VFREQ_HIGH_LIMIT      25000   /* 250 Hz */
#define PROGRESSIVE_VFREQ_HIGH_LIMIT    25000   /* 250 Hz */
#define INTERLACE_HFREQ_HIGH_LIMIT      7500    /* 75 kHz */
#define PROGRESSIVE_HFREQ_HIGH_LIMIT    15000    /* 150 kHz */
#define INTERLACE_MASK 0x04

#else

#define H_TOLERANCE 50      /* +- 500 Hz */
#define V_TOLERANCE 400     /* +- 4.0 Hz */
#define LINES_TOLERANCE     15
#define PIXELS_TOLERANCE    4
#define CLK_TOLERANCE       5
#define CHIP_OSC_CLK_MUL128 (3625216000) /* *1000 MHz */


#define INTERLACE_VFREQ_LOW_LIMIT       2500    /* 25 Hz */
#define PROGRESSIVE_VFREQ_LOW_LIMIT     2300    /* 23 Hz */
#define INTERLACE_HFREQ_LOW_LIMIT       1500    /* 15 kHz */
#define PROGRESSIVE_HFREQ_LOW_LIMIT     1500    /* 15 kHz */
#define INTERLACE_VFREQ_HIGH_LIMIT      6500    /* 65 Hz */
#define PROGRESSIVE_VFREQ_HIGH_LIMIT    10500   /* 105 Hz */
#define INTERLACE_HFREQ_HIGH_LIMIT      5500    /* 55 kHz */
#define PROGRESSIVE_HFREQ_HIGH_LIMIT    9700    /* 97 kHz */
#define INTERLACE_MASK 0x04

#endif

#define GetRefrTypeHVPol()  siiReadByteHDMIRXP0(RX_VID_STAT_ADDR)
#define GetXClock()         siiReadByteHDMIRXP0(RX_VID_XCNT_ADDR)
#define GetXClockW()        siiReadWordHDMIRXP0(RX_VID_XCNT_ADDR - 1)

extern ROM const struct VModeInfoType_s VModeTables[NMODES];
extern ROM const struct InterlaceCSyncType_s InterlaceCSync[3];


BYTE siiVideoModeDetection(BYTE *pbIndex, struct SyncInfoType_s *SyncInf);
BOOL siiCheckOutOfRangeConditions(struct SyncInfoType_s *SyncInfo);
BYTE siiSetVidResDependentVidOutputFormat(BYTE bVidResId,
		BYTE bVideoPathSelect, BYTE bAVI_State);
void siiGetSyncInfo(struct SyncInfoType_s *SyncInfo);
BOOL siiCheckSyncDetect(void);
BOOL siiCheckClockDetect(void);
BYTE siiGetPixClockFromTables(BYTE bIndex);
BOOL siiCompareNewOldSync(struct SyncInfoType_s *OldSyncInfo,
		struct SyncInfoType_s *NewSyncInfo);
void siiPrintVModeParameters(BYTE Index, BYTE SearchRes);
void siiGetVideoInputResolution(BYTE bVidResIndex, BYTE *pbVideoResInfo);
void siiGetVideoInputResolutionromRegisters(BYTE *pbVideoResInfo);
BYTE siiGetVideoResId(BYTE bVidResIndex);

#endif

