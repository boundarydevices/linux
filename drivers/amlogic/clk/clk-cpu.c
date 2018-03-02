/*
 * drivers/amlogic/clk/clk-cpu.c
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

#define MESON_CPU_CLK_CNTL1		0x00
#define MESON_CPU_CLK_CNTL		0x40

#define MESON_POST_MUX0		BIT(2)
#define MESON_DYN_MUX			BIT(10)
#define MESON_FINAL_MUX			BIT(11)
#define MESON_POST_MUX1		BIT(18)

#define MESON_N_WIDTH			9
#define MESON_N_SHIFT			20
#define MESON_SEL_WIDTH			2
#define MESON_SEL_SHIFT			2
#define MID_RATE		(1000*1000*1000)
#include "clkc.h"

#define to_meson_clk_cpu_nb(_nb) container_of(_nb, struct meson_clk_cpu, clk_nb)
#define to_clk_mux(_hw) container_of(_hw, struct clk_mux, hw)

/* GX series, control cpu clk in firmware,
 * kernel do not know when freq will change
 */
static u8 meson_clk_cpu_get_parent(struct clk_hw *hw)
{
	struct clk_mux *mux = to_clk_mux(hw);
	int num_parents = clk_hw_get_num_parents(hw);
	u32 val;

	val = clk_readl(mux->reg) >> mux->shift;
	val &= mux->mask;

	if (val >= num_parents)
		return -EINVAL;

	if (mux->table) {
		int i;

		for (i = 0; i < num_parents; i++)
			if (mux->table[i] == val)
				return i;
		return -EINVAL;
	}

	return val;
}

static int meson_clk_cpu_set_parent(struct clk_hw *hw, u8 index)
{
	/* To Do*/
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

static unsigned long meson_clk_cpu_recalc_rate(struct clk_hw *hw,
					       unsigned long parent_rate)
{
	struct clk_hw *parent_hw;
	u32 index = 0;
	unsigned long new_parent_rate;

	index = meson_clk_cpu_get_parent(hw);
	parent_hw = clk_hw_get_parent_by_index(hw, index);
	new_parent_rate = clk_hw_get_rate(parent_hw);
	if (new_parent_rate != parent_rate) {
		/*parent is changed by firmware, we need update parent*/
		clk_set_parent(hw->clk, parent_hw->clk);
	}

	return new_parent_rate;
}

/* FIXME MUX1 & MUX2 should be struct clk_hw objects */
static int meson_clk_cpu_pre_rate_change(struct meson_clk_cpu *clk_cpu,
					 struct clk_notifier_data *ndata)
{
	u32 cpu_clk_cntl;

	if (ndata->new_rate > MID_RATE) {
		/* switch final mux to fix pll */
		cpu_clk_cntl = readl(clk_cpu->base + clk_cpu->reg_off
				+ MESON_CPU_CLK_CNTL);
		cpu_clk_cntl &= ~MESON_FINAL_MUX;
		writel(cpu_clk_cntl, clk_cpu->base + clk_cpu->reg_off
				+ MESON_CPU_CLK_CNTL);
		udelay(100);
	}
	return 0;
}

/* FIXME MUX1 & MUX2 should be struct clk_hw objects */
static int meson_clk_cpu_post_rate_change(struct meson_clk_cpu *clk_cpu,
					  struct clk_notifier_data *ndata)
{
	u32 cpu_clk_cntl;

	if (ndata->new_rate > MID_RATE) {
		/* switch final mux to sys pll */
		cpu_clk_cntl = readl(clk_cpu->base + clk_cpu->reg_off
				+ MESON_CPU_CLK_CNTL);
		cpu_clk_cntl |= MESON_FINAL_MUX;
		writel(cpu_clk_cntl, clk_cpu->base + clk_cpu->reg_off
				+ MESON_CPU_CLK_CNTL);
		udelay(100);
	}

	return 0;
}

/*
 * This clock notifier is called when the frequency of the of the parent
 * PLL clock is to be changed. We use the xtal input as temporary parent
 * while the PLL frequency is stabilized.
 */
int meson_clk_cpu_notifier_cb(struct notifier_block *nb,
				     unsigned long event, void *data)
{
	struct clk_notifier_data *ndata = data;
	struct meson_clk_cpu *clk_cpu = to_meson_clk_cpu_nb(nb);
	int ret = 0;

	if (event == PRE_RATE_CHANGE)
		ret = meson_clk_cpu_pre_rate_change(clk_cpu, ndata);
	else if (event == POST_RATE_CHANGE)
		ret = meson_clk_cpu_post_rate_change(clk_cpu, ndata);

	return notifier_from_errno(ret);
}

const struct clk_ops meson_clk_cpu_ops = {
	.recalc_rate = meson_clk_cpu_recalc_rate,
	.get_parent = meson_clk_cpu_get_parent,
	.set_parent = meson_clk_cpu_set_parent,
};
