/*
 * sound/soc/amlogic/meson/i2s_dai.h
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

#ifndef __AML_I2S_DAI_H__
#define __AML_I2S_DAI_H__

struct aml_i2s {
	struct clk *clk_mpll;
	struct clk *clk_mclk;
	int old_samplerate;
	bool disable_clk_suspend;
	int audin_fifo_src;
	int i2s_pos_sync;
	int clk_data_pos;
};
#endif
