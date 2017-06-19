/*
 * include/linux/amlogic/media/utils/aformat.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef AFORMAT_H
#define AFORMAT_H

enum aformat_e {
	AFORMAT_MPEG = 0,
	AFORMAT_PCM_S16LE = 1,
	AFORMAT_AAC = 2,
	AFORMAT_AC3 = 3,
	AFORMAT_ALAW = 4,
	AFORMAT_MULAW = 5,
	AFORMAT_DTS = 6,
	AFORMAT_PCM_S16BE = 7,
	AFORMAT_FLAC = 8,
	AFORMAT_COOK = 9,
	AFORMAT_PCM_U8 = 10,
	AFORMAT_ADPCM = 11,
	AFORMAT_AMR = 12,
	AFORMAT_RAAC = 13,
	AFORMAT_WMA = 14,
	AFORMAT_WMAPRO = 15,
	AFORMAT_PCM_BLURAY = 16,
	AFORMAT_ALAC = 17,
	AFORMAT_VORBIS = 18,
	AFORMAT_AAC_LATM = 19,
	AFORMAT_APE = 20,
	AFORMAT_EAC3 = 21,
	AFORMAT_PCM_WIFIDISPLAY = 22,
	AFORMAT_TRUEHD = 25,
	/* AFORMAT_MPEG-->mp3,AFORMAT_MPEG1-->mp1,AFROMAT_MPEG2-->mp2 */
	AFORMAT_MPEG1 = 26,
	AFORMAT_MPEG2 = 27,
	AFORMAT_WMAVOI = 28,
	AFORMAT_WMALOSSLESS = 29,
	AFORMAT_PCM_S24LE = 30,
	AFORMAT_UNSUPPORT = 31,
	AFORMAT_MAX = 32
};

#endif /* AFORMAT_H */
