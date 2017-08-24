/*------------------------------------------------------------------------------
 * Module Name: SiIVRTables.c
 *
 * Module Description: contains a Video Resolution Tables which used
 *                     for Video Resolution Detections
 *
 * Copyright © 2002-2005, Silicon Image, Inc.  All rights reserved.
 *
 * No part of this work may be reproduced, modified, distributed, transmitted,
 * transcribed, or translated into any language or computer format, in any form
 * or by any means without written permission of: Silicon Image, Inc.,
 * 1060 East Arques Avenue, Sunnyvale, California 94085
 *----------------------------------------------------------------------------
 */

#include "SiITypeDefs.h"
#include "SiIVRTables.h"

#define WIDE_PULSE 0x80


/*-----------------------------------------------------------------------------
 * This table is used for detection of active video mode,
 * first part of table is CEA861B
 * second part of it for PC modes
 *----------------------------------------------------------------------------
 */
ROM const struct VModeInfoType_s VModeTables[NMODES] = {
	/*{{M.Id SubM},{PixClk},{RefTVPolHPol,RefrRt},{HTot,VTot},{HRes VRes}}*/
	{{ 1,  0, NSM},  25, {ProgrVNegHNeg,      6000, { 800,  525},},
		{ 640,  480}, 0}, /* 0 Format 1   // 640x480p@60(59,94) */
	{{ 2,  3, NSM},  27, {ProgrVNegHNeg,      6000, { 858,  525},},
		{ 720,  480}, 0}, /*  1 Format 2,3   // 720x480p@60(59,94) */
	{{ 4,  0, NSM},  74, {ProgrVPosHPos,      6000, {1650,  750},},
		{1280,  720}, 0}, /*  2 Format 4     // 1280x720p@60(59,94) */
	{{ 5,  0, NSM},  74, {InterlaceVPosHPos,  6000, {2200,  562},},
		{1920, 1080}, 0}, /*  3 Format 5     // 1920x1080i@60(59,94) */
	{{ 6,  7, NSM},  27, {InterlaceVNegHNeg,  6000, {1716,  262},},
		{1440,  480}, 1}, /* 4 Format 6,7 // 720(1440)x480i@60(59,94) */
	{{ 8,  9,   1},  27, {ProgrVNegHNeg,      6000, {1716,  262},},
		{1440,  240}, 1}, /* 5 Format 8,9 // 720(1440)x240p@60(59,94) */
	{{ 8,  9,   2},  27, {ProgrVNegHNeg,      6000, {1716,  263},},
		{1440,  240}, 1}, /* 6 Format 8,9 // 720(1440)x240p@60(59,94) */
	{{10, 11, NSM},  54, {InterlaceVNegHNeg,  6000, {3432,  262},},
		{2880,  480}, 1}, /*  7 Format 10,11 // (2880)x480i@60(59,94) */
	{{12, 13,   1},  54, {ProgrVNegHNeg,      6000, {3432,  262},},
		{2880,  240}, 3}, /*  8 Format 12,13 // (2880)x240p@60(59,94) */
	{{12, 13,   2},  54, {ProgrVNegHNeg,      6000, {3432,  263},},
		{2880,  240}, 3}, /*  9 Format 12,13 // (2880)x240p@60(59,94) */
	{{14, 15, NSM},  54, {ProgrVNegHNeg,      6000, {1716,  525},},
		{1440,  480}, 0}, /* 10 Format 14,15 // 1440x480p@60 (59,94) */
	{{16,  0, NSM}, 146, {ProgrVPosHPos,      6000, {2200, 1125},},
		{1920, 1080}, 0}, /* 11 Format 16    // 1920x1080p@60 (59,94) */
	{{17, 18, NSM},  27, {ProgrVNegHNeg,      5000, { 864,  625},},
		{ 720,  576}, 0}, /* 12 Format 17,18 // 720x576p@50 */
	{{19,  0, NSM},  74, {ProgrVPosHPos,      5000, {1980,  750},},
		{1280,  720}, 0}, /* 13 Format 19    // 1280x720p@50 */
	{{20,  0, NSM},  74, {InterlaceVPosHPos,  5000, {2640,  562},},
		{1920, 1080}, 0}, /* 14 Format 20    // 1920x1080i@50 */
	{{21, 22, NSM},  27, {InterlaceVNegHNeg,  5000, {1728,  312},},
		{1440,  576}, 1}, /* 15 Format 21,22 // 720(1440)x576i@50 */
	{{23, 24,   1},  27, {ProgrVNegHNeg,      5000, {1728,  312},},
		{1440,  288}, 1}, /* 16 Format 23,24 // 720(1440)x288p@50 */
	{{23, 24,   2},  27, {ProgrVNegHNeg,      5000, {1728,  313},},
		{1440,  288}, 1}, /* 17 Format 23,24 // 720(1440)x288p@50 */
	{{23, 24,   3},  27, {ProgrVNegHNeg,      5000, {1728,  314},},
		{1440,  288}, 1}, /* 18 Format 23,24 // 720(1440)x288p@50 */
	{{25, 26, NSM},  54, {InterlaceVNegHNeg,  5000, {3456,  312},},
		{2880,  576}, 1}, /* 19 Format 25,26 // (2880)x576i@50 */
	{{27, 28,   1},  54, {ProgrVNegHNeg,      5000, {3456,  312},},
		{2880,  288}, 3}, /* 20 Format 27,28 // (2880)x288p@50 */
	{{27, 28,   2},  54, {ProgrVNegHNeg,      5000, {3456,  313},},
		{2880,  288}, 3}, /* 21 Format 27,28 // (2880)x288p@50 */
	{{27, 28,   3},  54, {ProgrVNegHNeg,      5000, {3456,  314},},
		{2880,  288}, 3}, /* 22 Format 27,28 // (2880)x288p@50 */
	{{29, 30, NSM},  54, {ProgrVPosHNeg,      5000, {1728,  625},},
		{1440,  576}, 0}, /* 23 Format 29,30 //  1440x576p@50 */
	{{31,  0, NSM}, 148, {ProgrVPosHPos,      5000, {2640, 1125},},
		{1920, 1080}, 0}, /* 24 Format 31    //  1920x1080p@50 */
	{{32,  0, NSM},  74, {ProgrVPosHPos,      2400, {2750, 1125},},
		{1920, 1080}, 0}, /* 25 Format 32    //  1920x1080p@24 */
	{{33,  0, NSM},  74, {ProgrVPosHPos,      2500, {2640, 1125},},
		{1920, 1080}, 0}, /* 26 Format 33    //  1920x1080p@25 */
	{{34,  0, NSM},  74, {ProgrVPosHPos,      3000, {2200, 1125},},
		{1920, 1080}, 0}, /* 27 Format 34    //  1920x1080p@30 */

#ifdef SII_861C_MODES /* 15 enteries */
	{{35, 36, NSM}, 108, {ProgrVNegHNeg,      6000, {3432,  525},},
		{2880,  480}, 1}, /* 28 Format 35,36   2280(1440,720)x480p@60 */
	{{37, 38, NSM}, 108, {ProgrVNegHNeg,      5000, {3456,  625},},
		{2880,  576}, 1}, /* 29 Format 37,38   2280(1440,720)x525p@50 */
	{{39,  0, NSM},  72, {InterlaceVNegHPos,  5000, {2304,  625},},
		{1920, 1080}, 0}, /* 30 Format 39      1920 x 1080i @ 50 */
	{{40,  0, NSM}, 148, {InterlaceVPosHPos, 10000, {2640,  562},},
		{1920, 1080}, 0}, /* 31 Format 40      1920 x 1080i @ 100 */
	{{41,  0, NSM}, 148, {ProgrVPosHPos,     10000, {1980,  750},},
		{1280,  720}, 0}, /* 32 Format 41      1280 x 720p @ 100 */
	{{42, 43, NSM},  54, {ProgrVNegHNeg,     10000, { 864,  625},},
		{ 720,  576}, 0}, /* 33 Format 42,43    720 x 576p @ 100 */
	{{44, 45, NSM},  54, {ProgrVNegHNeg,     10000, {1728,  312},},
		{ 720,  576}, 1}, /* 34 Format 44,45    720 x 576i @ 100 */
	{{46,  0, NSM}, 148, {InterlaceVPosHPos, 12000, {2200,  562},},
		{1920, 1080}, 0}, /* 35 Format 46      1920 x 1080i @ 120 */
	{{47,  0, NSM}, 148, {ProgrVPosHPos,     12000, {1650,  750},},
		{1280,  720}, 0}, /* 36 Format 47      1280 x 720p @ 120 */
	{{48, 49, NSM},  54, {ProgrVNegHNeg,     12000, { 858,  525},},
		{ 720,  480}, 0}, /* 37 Format 48,49    720 x 480p @ 120 */
	{{50, 51, NSM},  54, {InterlaceVNegHNeg, 12000, {1716,  262},},
		{ 720,  480}, 1}, /* 38 Format 50,51    720 x 480i @ 120 */
	{{52, 53, NSM}, 108, {ProgrVPosHPos,     20000, { 864,  625},},
		{ 720,  576}, 0}, /* 39 Format 52,53    720 x 576p @ 200 */
	{{54, 55, NSM}, 108, {InterlaceVNegHNeg, 20000, {1728,  312},},
		{ 720,  576}, 0}, /* 40 Format 54,55    720 x 576i @ 200 */
	{{56, 57, NSM}, 108, {ProgrVNegHNeg,     24000, { 858,  525},},
		{ 720,  480}, 0}, /* 41 Format 56,57    720 x 480p @ 240 */
	{{58, 59, NSM}, 108, {InterlaceVNegHNeg, 24000, {1716,  262},},
		{ 720,  480}, 1}, /* 42 Format 58,59    720 x 480i @ 240 */

#endif

/* {{M.Id SubM},{PixClk},{RefTVPolHPol, RefrRt},{HTot,VTot},{HRes VRes}} */
#ifdef SII_PCMODES
	{{PC_BASE,      0, NSM},  25, {ProgrVNegHPos,     7009, { 800,  449},},
		{ 640,  350}, 0}, /*  640 x  350 @ 70.09 */
	{{PC_BASE +  1, 0, NSM},  31, {ProgrVNegHPos,     8508, { 832,  445},},
		{ 640,  350}, 0}, /*  640 x  350 @ 85.08 */
	{{PC_BASE +  2, 0, NSM},  25, {ProgrVPosHNeg,     7009, { 800,  449},},
		{ 640,  400}, 0}, /*  640 x  400 @ 70.09 */
	{{PC_BASE +  3, 0, NSM},  31, {ProgrVPosHNeg,     8508, { 832,  445},},
		{ 640,  400}, 0}, /*  640 x  400 @ 85.08 */
	{{PC_BASE +  4, 0, NSM},  28, {ProgrVPosHNeg,     7008, { 900,  449},},
		{ 720,  400}, 0}, /*  720 x  400 @ 70.08 */
	{{PC_BASE +  5, 0, NSM},  35, {ProgrVPosHNeg,     8504, { 936,  446},},
		{ 720,  400}, 0}, /*  720 x  400 @ 85.04 */
	{{PC_BASE +  6, 0, NSM},  25, {ProgrVNegHNeg,     5994, { 800,  525},},
		{ 640,  480}, 0}, /*  640 x  480 @ 59.94 */
	{{PC_BASE +  7, 0, NSM},  30, {ProgrVNegHNeg,     6667, { 864,  525},},
		{ 640,  480}, 0}, /*  640 x  480 @ 66.67 */
	{{PC_BASE +  8, 0, NSM},  31, {ProgrVNegHNeg,     7280, { 832,  520},},
		{ 640,  480}, 0}, /*  640 x  480 @ 72.80 */
	{{PC_BASE +  9, 0, NSM},  31, {ProgrVNegHNeg,     7500, { 840,  500},},
		{ 640,  480}, 0}, /*  640 x  480 @ 75.00 */
	{{PC_BASE + 10, 0, NSM},  36, {ProgrVNegHNeg,     8500, { 832,  509},},
		{ 640,  480}, 0}, /*  640 x  480 @ 85.00 */
	{{PC_BASE + 11, 0, NSM},  38, {ProgrVNegHNeg,     9003, { 832,  510},},
		{ 640,  480}, 0}, /*  640 x  480 @ 90.03 */
	{{PC_BASE + 12, 0, NSM},  45, {ProgrVNegHNeg,    10004, { 864,  530},},
		{ 640,  480}, 0}, /*  640 x  480 @ 100.04 */
	{{PC_BASE + 13, 0, NSM},  55, {ProgrVNegHNeg,    12000, { 864,  534},},
		{ 640,  480}, 0}, /*  640 x  480 @ 120 */
	{{PC_BASE + 14, 0, NSM},  36, {ProgrVPosHPos,     5625, {1024,  625},},
		{ 800,  600}, 0}, /*  800 x  600 @ 56.25 */
	{{PC_BASE + 15, 0, NSM},  40, {ProgrVPosHPos,     6031, {1056,  628},},
		{ 800,  600}, 0}, /*  800 x  600 @ 60.31 */
	{{PC_BASE + 16, 0, NSM},  50, {ProgrVPosHPos,     7219, {1040,  666},},
		{ 800,  600}, 0}, /*  800 x  600 @ 72.19 */
	{{PC_BASE + 17, 0, NSM},  50, {ProgrVPosHPos,     7500, {1056,  625},},
		{ 800,  600}, 0}, /*  800 x  600 @ 75 */
	{{PC_BASE + 18, 0, NSM},  56, {ProgrVPosHPos,     8506, {1048,  631},},
		{ 800,  600}, 0}, /*  800 x  600 @ 85.06 */
	{{PC_BASE + 19, 0, NSM},  57, {ProgrVNegHNeg,     9000, {1024,  622},},
		{ 800,  600}, 0}, /*  800 x  600 @ 90 */
	{{PC_BASE + 20, 0, NSM},  70, {ProgrVNegHNeg,    10000, {1088,  640},},
		{ 800,  600}, 0}, /*  800 x  600 @ 100 */
	{{PC_BASE + 21, 0, NSM},  57, {ProgrVNegHNeg,     7455, {1152,  667},},
		{ 832,  624}, 0}, /*  832 x  624 @ 75 */
	{{PC_BASE + 22, 0, NSM},  45, {InterlaceVPosHPos, 4348, {1264,  817},},
		{1024,  768}, 0}, /* 1024 x  768 @ 43.48 interlaced */
	{{PC_BASE + 23, 0, NSM},  65, {ProgrVNegHNeg,     6000, {1344,  806},},
		{1024,  768}, 0}, /* 1024 x  768 @ 60 */
	{{PC_BASE + 24, 0, NSM},  75, {ProgrVNegHNeg,     7007, {1328,  806},},
		{1024,  768}, 0}, /* 1024 x  768 @ 70.07 */
	{{PC_BASE + 25, 0, NSM},  79, {ProgrVPosHPos,     7503, {1312,  800},},
		{1024,  768}, 0}, /* 1024 x  768 @ 75.03 */
	{{PC_BASE + 26, 0, NSM},  94, {ProgrVPosHPos,     8500, {1376,  808},},
		{1024,  768}, 0}, /* 1024 x  768 @ 85 */
	{{PC_BASE + 27, 0, NSM},  80, {ProgrVPosHPos,     6005, {1472,  905},},
		{1152,  864}, 0}, /* 1152 x  864 @ 60.05 */
	{{PC_BASE + 28, 0, NSM},  94, {ProgrVPosHPos,     7002, {1472,  914},},
		{1152,  864}, 0}, /* 1152 x  864 @ 70.02 */
	{{PC_BASE + 29, 0, NSM}, 108, {ProgrVPosHPos,     7500, {1600,  900},},
		{1152,  864}, 0}, /* 1152 x  864 @ 75 */
	{{PC_BASE + 30, 0, NSM}, 108, {ProgrVPosHPos,     6001, {1696, 1066},},
		{1280, 1024}, 0}, /* 1280 x 1024 @ 60.01 */
	{{PC_BASE + 31, 0, NSM}, 108, {ProgrVPosHPos,     6002, {1688, 1066},},
		{1280, 1024}, 0}, /* 1280 x 1024 @ 60.02 */
	{{PC_BASE + 32, 0, NSM}, 135, {ProgrVPosHPos,     7502, {1688, 1066},},
		{1280, 1024}, 0}, /* 1280 x 1024 @ 75.02 */
	{{PC_BASE + 33, 0, NSM}, 157, {ProgrVPosHPos,     8502, {1728, 1072},},
		{1280, 1024}, 0}, /* 1280 x 1024 @ 85.02 */
	{{PC_BASE + 34, 0, NSM}, 135, {InterlaceVPosHPos, 4804, {2160, 1301},},
		{1600, 1200}, 0}, /* 1600 x 1200 @ 48.04 interlaced */
	{{PC_BASE + 35, 0, NSM}, 158, {ProgrVPosHPos,     6000, {2112, 1250},},
		{1600, 1200}, 0}, /* 1600 x 1200 @ 60 */
	{{PC_BASE + 36, 0, NSM}, 121, {ProgrVPosHPos,     8506, {1568,  911},},
		{1152,  864}, 0}, /* 1152 x  864 @ 85 */
	{{PC_BASE + 37, 0, NSM}, 100, {ProgrVNegHNeg,     7506, {1456,  915},},
		{1152,  870}, 0}, /* 1152 x  870 @ 75.06 */
	{{PC_BASE + 38, 0, NSM}, 108, {ProgrVPosHPos,     6000, {1800, 1000},},
		{1280,  960}, 0}, /* 1280 x  960 @ 60 */
	{{PC_BASE + 39, 0, NSM}, 129, {ProgrVPosHPos,     7500, {1728, 1000},},
		{1280,  960}, 0}, /* 1280 x  960 @ 75 */
	{{PC_BASE + 40, 0, NSM}, 148, {ProgrVPosHPos,     8500, {1728, 1011},},
		{1280,  960}, 0}, /* 1280 x  960 @ 85 */
	{{PC_BASE + 41, 0, NSM},  78, {InterlaceVPosHPos, 4344, {1696, 1069},},
		{1280, 1024}, 0}, /* 1280 x 1024 @ 43.44 interlaced */
	{{PC_BASE + 42, 0, NSM}, 162, {ProgrVPosHPos,     6000, {2160, 1250},},
		{1600, 1200}, 0}, /* 1600 x 1200 @ 60 */
	{{PC_BASE + 43, 0, NSM}, 173, {ProgrVPosHPos,     6553, {2112, 1250},},
		{1600, 1200}, 0}, /* 1600 x 1200 @ 65.53 */
	{{PC_BASE + 44, 0, NSM}, 175, {ProgrVPosHPos,     6500, {2154, 1250},},
		{1600, 1200}, 0}, /* 1600 x 1200 @ 65 */
	{{PC_BASE + 45, 0, NSM}, 185, {ProgrVPosHPos,     7000, {2160, 1250},},
		{1600, 1200}, 0}, /* 1600 x 1200 @ 70 */
	{{PC_BASE + 46, 0, NSM}, 202, {ProgrVPosHPos,     7500, {2160, 1250},},
		{1600, 1200}, 0}, /* 1600 x 1200 @ 75 */
	{{PC_BASE + 47, 0, NSM}, 216, {ProgrVPosHPos,     8000, {2160, 1250},},
		{1600, 1200}, 0}, /* 1600 x 1200 @ 80 */
	{{PC_BASE + 48, 0, NSM}, 229, {ProgrVPosHPos,     8500, {2160, 1250},},
		{1600, 1200}, 0}, /* 1600 x 1200 @ 85 */
	{{PC_BASE + 49, 0, NSM}, 232, {ProgrVPosHPos,     8500, {2176, 1258},},
		{1600, 1200}, 0}, /* 1600 x 1200 @ 85 */

	/*  Wide resolutions */

	{{PC_BASE + 50, 0, NSM},  33, {ProgrVPosHPos,     6000, { 517, 1235},},
		{ 848,  480}, 0}, /*  848 x  480 WVGA */
	{{PC_BASE + 51, 0, NSM},  68, {ProgrVNegHPos,     6000, {1440,  790},},
		{1280,  768}, 0}, /* 1280 x  768 WXGA  reduced blanking */
	{{PC_BASE + 52, 0, NSM},  85, {ProgrVPosHPos,     6000, {1792,  795},},
		{1360,  768}, 0}, /* 1360 x  768 WXGA  reduced blanking */
	{{PC_BASE + 53, 0, NSM}, 154, {ProgrVNegHPos,     6000, {2080, 1235},},
		{1920, 1200}, 0}, /* 1920 x 1200 WUXGA  reduced blanking */

#endif
};

/*-----------------------------------------------------------------------------
 * This tables are used for programming Sync on Y(Green) for interlace modes
 *----------------------------------------------------------------------------
 */
ROM const struct InterlaceCSyncType_s InterlaceCSync[3] = {
	{507, 6, 6, 6, 0},               /* 480i */
	{610, 5, 5, 5, 0},               /* 576i */
	{1113, 0, 10, 2 | WIDE_PULSE, 0},  /* 1080i */
};
