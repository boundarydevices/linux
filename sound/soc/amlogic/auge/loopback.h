/*
 * sound/soc/amlogic/auge/loopback.h
 *
 * Copyright (C) 2018 Amlogic, Inc. All rights reserved.
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
#ifndef __AML_AUDIO_LOOPBACK_H__
#define __AML_AUDIO_LOOPBACK_H__
#include <sound/soc.h>
#include <sound/tlv.h>

/* datain src
 * [4]: pdmin;
 * [3]: spdifin;
 * [2]: tdmin_c;
 * [1]: tdmin_b;
 * [0]: tdmin_a;
 */
enum datain_src {
	DATAIN_TDMA = 0,
	DATAIN_TDMB,
	DATAIN_TDMC,
	DATAIN_SPDIF,
	DATAIN_PDM,
	DATAIN_LOOPBACK,
};

/* datalb src
 * /tdmin_lb src
 *     [0]: tdmoutA
 *     [1]: tdmoutB
 *     [2]: tdmoutC
 *     [3]: PAD_tdminA
 *     [4]: PAD_tdminB
 *     [5]: PAD_tdminC
 * /spdifin_lb src
 *     spdifout_a
 *     spdifout_b
 */
enum datalb_src {
	TDMINLB_TDMOUTA = 0,
	TDMINLB_TDMOUTB,
	TDMINLB_TDMOUTC,
	TDMINLB_PAD_TDMINA,
	TDMINLB_PAD_TDMINB,
	TDMINLB_PAD_TDMINC,

	TDMINLB_PAD_TDMINA_D,
	TDMINLB_PAD_TDMINB_D,
	TDMINLB_PAD_TDMINC_D,

	SPDIFINLB_SPDIFOUTA,
	SPDIFINLB_SPDIFOUTB,
};
#endif
