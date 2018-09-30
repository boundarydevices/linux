/*
 * drivers/amlogic/clk/clk-cpu-fclk-composite.c
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

/*
 * CPU clock path:
 *
 *                           +-[/N]-----|3|
 *             MUX2  +--[/3]-+----------|2| MUX1
 * [sys_pll]---|1|   |--[/2]------------|1|-|1|
 *             | |---+------------------|0| | |----- [a5_clk]
 *          +--|0|                          | |
 * [xtal]---+-------------------------------|0|
 *
 *
 *
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>

#define MESON_CPU_CLK_CNTL		0x00
#define MESON_CPU_CLK_CNTL1		0x40

#define MESON_POST_MUX0		BIT(2)
#define MESON_DYN_MUX			BIT(10)
#define MESON_FINAL_MUX			BIT(11)
#define MESON_POST_MUX1		BIT(18)
#define MESON_DYN_ENABLE	BIT(26)

#define MESON_N_WIDTH			9
#define MESON_N_SHIFT			20
#define MESON_SEL_WIDTH			2
#define MESON_SEL_SHIFT			2

#include "clkc.h"

#define to_clk_mux_divider(_hw) \
		container_of(_hw, struct meson_cpu_mux_divider, hw)
static unsigned int gap_rate = (10*1000*1000);

/* GX series, control cpu clk in firmware,
 * kernel do not know when freq will change
 */

static const struct fclk_rate_table *meson_fclk_get_pll_settings
	(struct meson_cpu_mux_divider *pll, unsigned long rate)
{
	const struct fclk_rate_table *rate_table = pll->rate_table;
	int i;

	for (i = 0; i < pll->rate_count; i++) {
		if (abs(rate-rate_table[i].rate) < gap_rate)
			return &rate_table[i];
	}
	return NULL;
}

static u8 meson_fclk_cpu_get_parent(struct clk_hw *hw)
{
	struct meson_cpu_mux_divider *mux_divider =
		to_clk_mux_divider(hw);
	int num_parents = clk_hw_get_num_parents(hw);
	u32  val, final_dyn_mask, premux_mask;
	u8 final_dyn_shift,  premux_shift;

	final_dyn_mask = mux_divider->cpu_fclk_p.mask;
	final_dyn_shift = mux_divider->cpu_fclk_p.shift;
	val = clk_readl(mux_divider->reg);

	if ((val >> final_dyn_shift) & final_dyn_mask) {
		premux_mask = mux_divider->cpu_fclk_p10.mask;
		premux_shift = mux_divider->cpu_fclk_p10.shift;
	} else {
		premux_mask = mux_divider->cpu_fclk_p00.mask;
		premux_shift = mux_divider->cpu_fclk_p00.shift;
	}

	val = clk_readl(mux_divider->reg) >> premux_shift;
	val &= premux_mask;


	if (val >= num_parents)
		return -EINVAL;

	if (mux_divider->table) {
		int i;

		for (i = 0; i < num_parents; i++)
			if (mux_divider->table[i] == val)
				return i;
	}
	return val;

}

static int meson_fclk_cpu_set_parent(struct clk_hw *hw, u8 index)
{
	struct meson_cpu_mux_divider *mux_divider =
		to_clk_mux_divider(hw);
	u32  val, final_dyn_mask, premux_mask;
	u8 final_dyn_shift, premux_shift;
	unsigned long flags = 0;

	final_dyn_mask = mux_divider->cpu_fclk_p.mask;
	final_dyn_shift = mux_divider->cpu_fclk_p.shift;
	val = clk_readl(mux_divider->reg);
	if ((val >> final_dyn_shift) & final_dyn_mask) {
		premux_mask = mux_divider->cpu_fclk_p00.mask;
		premux_shift = mux_divider->cpu_fclk_p00.shift;
	} else {
		premux_mask = mux_divider->cpu_fclk_p10.mask;
		premux_shift = mux_divider->cpu_fclk_p10.shift;
	}

	if (mux_divider->table) {
		index = mux_divider->table[index];
	} else {
		if (mux_divider->flags & CLK_MUX_INDEX_BIT)
			index = (1 << ffs(index));

		if (mux_divider->flags & CLK_MUX_INDEX_ONE)
			index++;
	}

	if (mux_divider->lock)
		spin_lock_irqsave(mux_divider->lock, flags);
	else
		__acquire(mux_divider->lock);

	if (mux_divider->flags & CLK_MUX_HIWORD_MASK) {
		val = premux_mask << (premux_shift + 16);
	} else {
		val = clk_readl(mux_divider->reg);
		val &= ~(premux_mask << premux_shift);
	}

	val |= index << premux_shift;
	clk_writel(val, mux_divider->reg);

	if (mux_divider->lock)
		spin_unlock_irqrestore(mux_divider->lock, flags);
	else
		__release(mux_divider->lock);

	return 0;
}

static unsigned long meson_fclk_cpu_recalc_rate(struct clk_hw *hw,
					       unsigned long parent_rate)
{
	struct meson_cpu_mux_divider *mux_divider =
		to_clk_mux_divider(hw);
	struct parm_fclk *p_premux, *p_postmux, *p_div;
	unsigned long rate;
	u32  val, final_dyn_mask, div;
	u8 final_dyn_shift;

	final_dyn_mask = mux_divider->cpu_fclk_p.mask;
	final_dyn_shift = mux_divider->cpu_fclk_p.shift;
	val = readl(mux_divider->reg);

	if ((val >> final_dyn_shift) & final_dyn_mask) {
		p_premux = &mux_divider->cpu_fclk_p10;
		p_postmux = &mux_divider->cpu_fclk_p1;
		p_div = &mux_divider->cpu_fclk_p11;
	} else {
		p_premux = &mux_divider->cpu_fclk_p00;
		p_postmux = &mux_divider->cpu_fclk_p0;
		p_div = &mux_divider->cpu_fclk_p01;
	}

	div = PARM_GET(p_div->width, p_div->shift, val);
	rate = parent_rate / (div + 1);

	return rate;
}

static int meson_fclk_divider_set_rate(struct clk_hw *hw, unsigned long rate,
					unsigned long parent_rate)
{
	struct meson_cpu_mux_divider *mux_divider =
		to_clk_mux_divider(hw);
	const struct fclk_rate_table *rate_set;
	struct parm_fclk *p_premux, *p_postmux, *p_div;
	u32  val, final_dyn_mask;
	u8 final_dyn_shift;
	unsigned long old_rate;
	unsigned int tmp;
	unsigned long flags = 0;

	if (parent_rate == 0 || rate == 0)
		return -EINVAL;

	final_dyn_mask = mux_divider->cpu_fclk_p.mask;
	final_dyn_shift = mux_divider->cpu_fclk_p.shift;
	val = readl(mux_divider->reg);

	if ((val >> final_dyn_shift) & final_dyn_mask) {
		p_premux = &mux_divider->cpu_fclk_p00;
		p_postmux = &mux_divider->cpu_fclk_p0;
		p_div = &mux_divider->cpu_fclk_p01;
	} else {
		p_premux = &mux_divider->cpu_fclk_p10;
		p_postmux = &mux_divider->cpu_fclk_p1;
		p_div = &mux_divider->cpu_fclk_p11;
	}

	old_rate = rate;
	rate_set = meson_fclk_get_pll_settings(mux_divider, rate);
	if (!rate_set)
		return -EINVAL;

	if (mux_divider->lock)
		spin_lock_irqsave(mux_divider->lock, flags);
	else
		__acquire(mux_divider->lock);
	writel((val | MESON_DYN_ENABLE), mux_divider->reg);
	/*set mux_divider clk divider*/
	tmp = rate_set->mux_div;
	val = PARM_SET(p_div->width, p_div->shift, val, tmp);
	writel(val, mux_divider->reg);
	/*set mux_divider postmux*/
	tmp = rate_set->postmux;
	val = PARM_SET(p_postmux->width, p_postmux->shift, val, tmp);
	writel(val, mux_divider->reg);
	/*set mux_divider final dyn*/
	val = readl(mux_divider->reg);
	if ((val >> final_dyn_shift) & final_dyn_mask)
		val &= ~(1 << final_dyn_shift);
	else
		val |= (1 << final_dyn_shift);

	writel(val, mux_divider->reg);
	if (mux_divider->lock)
		spin_unlock_irqrestore(mux_divider->lock, flags);
	else
		__release(mux_divider->lock);

	return 0;
}

int meson_fclk_mux_divider_determine_rate(struct clk_hw *hw,
			     struct clk_rate_request *req)
{
	struct clk_hw *best_parent = NULL;
	unsigned long best = 0;
	struct clk_rate_request parent_req = *req;
	struct meson_cpu_mux_divider *mux_divider =
		to_clk_mux_divider(hw);
	const struct fclk_rate_table *rate_set;
	u32 premux;

	rate_set = meson_fclk_get_pll_settings(mux_divider, req->rate);
	if (!rate_set)
		return -EINVAL;

	premux = rate_set->premux;
	best_parent = clk_hw_get_parent_by_index(hw, premux);

	if (!best_parent)
		return -EINVAL;

	best = clk_hw_get_rate(best_parent);

	if (best != parent_req.rate)
		meson_fclk_cpu_set_parent(hw, premux);

	if (best_parent)
		req->best_parent_hw = best_parent;

	req->best_parent_rate = best;
	return 0;

}

const struct clk_ops meson_fclk_cpu_ops = {
	.determine_rate	= meson_fclk_mux_divider_determine_rate,
	.recalc_rate	= meson_fclk_cpu_recalc_rate,
	.get_parent	= meson_fclk_cpu_get_parent,
	.set_parent	= meson_fclk_cpu_set_parent,
	.set_rate	= meson_fclk_divider_set_rate,
};

