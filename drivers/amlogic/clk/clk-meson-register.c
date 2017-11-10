/*
 * drivers/amlogic/clk/clk-meson-register.c
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

#include <linux/clk-provider.h>
#include "clkc.h"

/*register composite clock*/
void meson_clk_register_composite(struct clk **soc_clks,
				struct meson_composite *composite,
				unsigned int length)
{
	int i = 0;

	for (i = 0; i < length; i++) {
		soc_clks[composite[i].composite_id] = clk_register_composite(
		NULL, composite[i].name, composite[i].parent_names,
		composite[i].num_parents,
		composite[i].mux_hw, &clk_mux_ops,
		composite[i].rate_hw, &clk_divider_ops,
		composite[i].gate_hw, &clk_gate_ops, composite[i].flags);
		if (IS_ERR(soc_clks[composite[i].composite_id]))
			pr_err("%s: %s error\n", __func__, composite[i].name);
	}
}

/*register single clock*/
void meson_hw_clk_register(struct clk **soc_clks,
				struct meson_hw *m,
				unsigned int length)
{
	int i = 0;

	for (i = 0; i < length; i++) {
		soc_clks[m[i].hw_id] = clk_register(NULL, m[i].hw);
		WARN_ON(soc_clks[m[i].hw_id]);
	}
}
