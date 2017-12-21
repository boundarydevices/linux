/*
 * sound/soc/amlogic/meson/pcm_dai.h
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

#ifndef AML_DAI_H
#define AML_DAI_H

struct aml_pcm {
	struct clk *clk_mpll;
	struct clk *clk_pcm_mclk;
	struct clk *clk_pcm_sync;
	int pcm_mode;
};
#endif
