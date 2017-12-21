/*
 * sound/soc/amlogic/meson/i2s2_dai.h
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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

#ifndef __AML_I2S2_DAI_H__
#define __AML_I2S2_DAI_H__

struct aml_i2s2 {
	struct clk *clk_mpll;
	struct clk *clk_mclk;
	struct clk *clk_audin_mpll;
	struct clk *clk_audin_mclk;
	struct clk *clk_audin_sclk;
	struct clk *clk_audin_lrclk;
	int last_audin_samplerate;
	int last_bitwidth;
	int old_samplerate;
	int audin_fifo_src;
};
#endif
