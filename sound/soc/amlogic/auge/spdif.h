/*
 * sound/soc/amlogic/auge/spdif.h
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

#ifndef __AML_SPDIF_H__
#define __AML_SPDIF_H__
#include <linux/clk.h>

enum SPDIF_ID {
	SPDIF_A,
	SPDIF_B,
	SPDIF_ID_CNT
};

int spdif_set_audio_clk(enum SPDIF_ID id,
		struct clk *clk_src, int rate, bool same);

#endif
