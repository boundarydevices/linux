/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _VGA_TABLE_H_
#define _VGA_TABLE_H_

extern const u8 bHdtvTimings;
extern const u8 bVgaTimings;
extern const u8 bUserVgaTimings;
extern const u8 bAllTimings;

extern const u8 bUserVgaTimingBegin;

#define USERMODE_TIMING 8
#define USERMODE_TIMING 8
#define HDTV_TIMING_NUM 112

#define ALL_TIMING_NUM (sizeof(VGATIMING_TABLE) / sizeof(struct VGAMODE))

u16 Get_VGAMODE_IHF(u8 mode);
u8 Get_VGAMODE_IVF(u8 mode);
u16 Get_VGAMODE_ICLK(u8 mode);
u16 Get_VGAMODE_IHTOTAL(u8 mode);
u16 Get_VGAMODE_IVTOTAL(u8 mode);
u16 Get_VGAMODE_IPH_STA(u8 mode);
u16 Get_VGAMODE_IPH_SYNCW(u8 mode);
u16 Get_VGAMODE_IPH_WID(u8 mode);
u16 Get_VGAMODE_IPH_BP(u8 mode);
u8 Get_VGAMODE_IPV_STA(u8 mode);
u16 Get_VGAMODE_IPV_LEN(u8 mode);
u16 Get_VGAMODE_COMBINE(u8 mode);
u8 Get_VGAMODE_OverSample(u8 mode);
u16 Get_VGAMODE_ID(u8 mode);

#define Get_VGAMODE_HSyncWidthChk(bMode)                                       \
	((Get_VGAMODE_COMBINE(bMode) >> 1) & 0x01)
#define Get_VGAMODE_AmbiguousH(bMode) ((Get_VGAMODE_COMBINE(bMode) >> 2) & 0x01)
#define Get_VGAMODE_VPol(bMode) ((Get_VGAMODE_COMBINE(bMode) >> 3) & 0x01)
#define Get_VGAMODE_HPol(bMode) ((Get_VGAMODE_COMBINE(bMode) >> 4) & 0x01)
#define Get_VGAMODE_VSyncWidthChk(bMode)                                       \
	((Get_VGAMODE_COMBINE(bMode) >> 5) & 0x01)
#define Get_VGAMODE_INTERLACE(bMode) ((Get_VGAMODE_COMBINE(bMode) >> 6) & 0x01)
#define Get_VGAMODE_PolChk(bMode) ((Get_VGAMODE_COMBINE(bMode) >> 7) & 0x01)
#define Get_HDMIMODE_SupportVideo(bMode)                                       \
	((Get_VGAMODE_COMBINE(bMode) >> 8) & 0x01)
#define Get_VGAMODE_VgaDisabled(bMode)                                         \
	((Get_VGAMODE_COMBINE(bMode) >> 9) & 0x01)
#define Get_VGAMODE_YpbprDisabled(bMode)                                       \
	((Get_VGAMODE_COMBINE(bMode) >> 10) & 0x01)

#define HDTV_SEARCH_START 1
#define HDTV_SEARCH_END (bHdtvTimings - 1)

#define VGA_SEARCH_START (bHdtvTimings)

#define DVI_SEARCH_START (bHdtvTimings)
#define DVI_SEARCH_END (bHdtvTimings + bVgaTimings - 1)

#define MAX_TIMING_FORMAT (bAllTimings)

#define fgIsUserModeTiming(bMode)                                              \
	(((bMode) >= bUserVgaTimingBegin) && ((bMode) < bAllTimings))

#define fgIsVgaTiming(bMode)                                                   \
	((((bMode) >= DVI_SEARCH_START) && ((bMode) <= DVI_SEARCH_END)) ||     \
		((bMode) == MODE_DE_MODE) || fgIsUserModeTiming(bMode))

#define fgIsVideoTiming(bMode)                                                 \
	(((bMode) >= HDTV_SEARCH_START) && ((bMode) <= HDTV_SEARCH_END))

#define fgIsValidTiming(bMode)                                                 \
	(fgIsVgaTiming(bMode) || fgIsVideoTiming(bMode) ||                     \
		fgIsUserModeTiming(bMode))

#define PROGRESSIVE 0
#define INTERLACE 1

#endif
