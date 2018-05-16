/*
 * drivers/amlogic/clk/clk-mux.c
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

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/string.h>

#if (defined CONFIG_ARM64) || (defined CONFIG_ARM64_A32)
#include "clkc.h"
#else
#include "m8b/clkc.h"
#endif

#define to_clk_mux(_hw) container_of(_hw, struct clk_mux, hw)

static u8 meson_clk_mux_get_parent(struct clk_hw *hw)
{
	struct clk_mux *mux = to_clk_mux(hw);
	int num_parents = clk_hw_get_num_parents(hw);
	u32 val;

	/*
	 * FIXME need a mux-specific flag to determine if val is bitwise or
	 * numeric. e.g. sys_clkin_ck's clksel field is 3 bits wide, but ranges
	 * from 0x1 to 0x7 (index starts at one)
	 * OTOH, pmd_trace_clk_mux_ck uses a separate bit for each clock, so
	 * val = 0x4 really means "bit 2, index starts at bit 0"
	 */
	val = clk_readl(mux->reg) >> mux->shift;
	val &= mux->mask;

	if (mux->table) {
		int i;

		for (i = 0; i < num_parents; i++)
			if (mux->table[i] == val)
				return i;
		return -EINVAL;
	}

	if (val && (mux->flags & CLK_MUX_INDEX_BIT))
		val = ffs(val) - 1;

	if (val && (mux->flags & CLK_MUX_INDEX_ONE))
		val--;

	if (val >= num_parents)
		return -EINVAL;

	return val;
}

static int meson_clk_mux_set_parent(struct clk_hw *hw, u8 index)
{
	struct clk_mux *mux = to_clk_mux(hw);
	u32 val;
	unsigned long flags = 0;

	if (mux->table) {
		index = mux->table[index];
	} else {
		if (mux->flags & CLK_MUX_INDEX_BIT)
			index = (1 << ffs(index));

		if (mux->flags & CLK_MUX_INDEX_ONE)
			index++;
	}

	if (mux->lock)
		spin_lock_irqsave(mux->lock, flags);
	else
		__acquire(mux->lock);

	if (mux->flags & CLK_MUX_HIWORD_MASK) {
		val = mux->mask << (mux->shift + 16);
	} else {
		val = clk_readl(mux->reg);
		val &= ~(mux->mask << mux->shift);
	}

	val |= index << mux->shift;
	clk_writel(val, mux->reg);

	if (mux->lock)
		spin_unlock_irqrestore(mux->lock, flags);
	else
		__release(mux->lock);

	return 0;
}

static unsigned long meson_clk_mux_recalc_rate(struct clk_hw *hw,
					       unsigned long parent_rate)
{
	struct clk_hw *parent_hw;
	u32 index = 0;
	unsigned long new_parent_rate;

	index = meson_clk_mux_get_parent(hw);

	parent_hw = clk_hw_get_parent_by_index(hw, index);
	new_parent_rate = clk_hw_get_rate(parent_hw);
	if (new_parent_rate != parent_rate)
		clk_set_parent(hw->clk, parent_hw->clk);

	return new_parent_rate;
}

int meson_clk_mux_determine_rate(struct clk_hw *hw,
			     struct clk_rate_request *req)
{
	struct clk_hw *parent, *best_parent = NULL;
	int i, num_parents, ret;
	unsigned long best = 0;
	struct clk_rate_request parent_req = *req;
	struct clk_mux *mux = to_clk_mux(hw);

	num_parents = clk_hw_get_num_parents(hw);

	if ((num_parents == 2) && (mux->flags == CLK_PARENT_ALTERNATE)) {
		i = meson_clk_mux_get_parent(hw);
		i = (i + 1) % 2;

		best_parent = clk_hw_get_parent_by_index(hw, i);
		best = clk_hw_get_rate(best_parent);
		if (best != parent_req.rate) {
			ret = clk_set_rate(best_parent->clk, parent_req.rate);
			if (ret)
				pr_err("Fail! Can not set to %lu, cur rate: %lu\n",
				   parent_req.rate, best);
			else {
				best = clk_hw_get_rate(best_parent);
				pr_debug("success set parent %s rate to %lu\n",
					clk_hw_get_name(best_parent), best);
				if (!(clk_hw_get_flags(hw) &
						CLK_SET_RATE_UNGATE)) {
					clk_prepare(best_parent->clk);
					clk_enable(best_parent->clk);
				}
			}
		}
	} else {
		for (i = 0; i < num_parents; i++) {
			parent = clk_hw_get_parent_by_index(hw, i);
			if (!parent)
				continue;

			if (mux->flags & CLK_SET_RATE_PARENT) {
				parent_req = *req;
				ret = __clk_determine_rate(parent, &parent_req);
				if (ret)
					continue;
			} else {
				parent_req.rate = clk_hw_get_rate(parent);
			}
		}
	}

	if (!best_parent)
		return -EINVAL;

	if (best_parent)
		req->best_parent_hw = best_parent;

	req->best_parent_rate = best;
	req->rate = best;

	return 0;
}

const struct clk_ops meson_clk_mux_ops = {
	.get_parent = meson_clk_mux_get_parent,
	.set_parent = meson_clk_mux_set_parent,
	.determine_rate = meson_clk_mux_determine_rate,
	.recalc_rate = meson_clk_mux_recalc_rate,
};

const struct clk_ops meson_clk_mux_ro_ops = {
	.get_parent = meson_clk_mux_get_parent,
};
