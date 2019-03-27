/*
 * sound/soc/amlogic/auge/audio_clks.h
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
#ifndef __AML_AUDIO_CLKS_H__
#define __AML_AUDIO_CLKS_H__

#include <linux/device.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>

#include <linux/clk-provider.h>

#define CLOCK_GATE(_name, _reg, _bit) \
static struct clk_gate _name = { \
	.reg = (void __iomem *)(_reg), \
	.bit_idx = (_bit), \
	.lock = &aclk_lock, \
	.hw.init = &(struct clk_init_data) { \
		.name = #_name, \
		.ops = &clk_gate_ops, \
		.parent_names = (const char *[]){ "clk81" }, \
		.num_parents = 1, \
		.flags = (CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED), \
	}, \
}

#define CLOCK_COM_MUX(_name, _reg, _mask, _shift) \
static struct clk_mux _name##_mux = { \
	.reg = (void __iomem *)(_reg), \
	.mask = (_mask), \
	.shift = (_shift), \
	.lock = &aclk_lock, \
}
#define CLOCK_COM_DIV(_name, _reg, _shift, _width) \
static struct clk_divider _name##_div = { \
	.reg = (void __iomem *)(_reg), \
	.shift = (_shift), \
	.width = (_width), \
	.lock = &aclk_lock, \
}
#define CLOCK_COM_GATE(_name, _reg, _bit) \
static struct clk_gate _name##_gate = { \
	.reg = (void __iomem *)(_reg), \
	.bit_idx = (_bit), \
	.lock = &aclk_lock, \
}

#define IOMAP_COM_CLK(_name, _iobase) \
do { \
	_name##_mux.reg += (unsigned long)(iobase); \
	_name##_div.reg += (unsigned long)(iobase); \
	_name##_gate.reg += (unsigned long)(iobase); \
} while (0)

#define REGISTER_CLK_COM(_name) \
	clk_register_composite(NULL, #_name, \
			mclk_parent_names, ARRAY_SIZE(mclk_parent_names), \
			&_name##_mux.hw, &clk_mux_ops, \
			&_name##_div.hw, &clk_divider_ops, \
			&_name##_gate.hw, &clk_gate_ops, \
			CLK_SET_RATE_NO_REPARENT)

#define REGISTER_AUDIOCLK_COM(_name) \
	clk_register_composite(NULL, #_name, \
			audioclk_parent_names, \
			ARRAY_SIZE(audioclk_parent_names), \
			&_name##_mux.hw, &clk_mux_ops, \
			&_name##_div.hw, &clk_divider_ops, \
			&_name##_gate.hw, &clk_gate_ops, \
			CLK_SET_RATE_NO_REPARENT)

struct audio_clk_init {
	int clk_num;
	int (*clk_gates)(struct clk **clks, void __iomem *iobase);
	int (*clks)(struct clk **clks, void __iomem *iobase);
};

extern struct audio_clk_init axg_audio_clks_init;
extern struct audio_clk_init g12a_audio_clks_init;
extern struct audio_clk_init tl1_audio_clks_init;
extern struct audio_clk_init sm1_audio_clks_init;
extern struct audio_clk_init tm2_audio_clks_init;

struct clk_chipinfo {
	/* force clock source as oscin(24M) */
	bool force_oscin_fn;
	/* Sclk_ws_inv for tdm; if not, echo for axg tdm
	 * Sclk_ws_inv, for the capture ws sclk; 0: not revert; 1: revert clock;
	 */
	bool sclk_ws_inv;
};
#endif
