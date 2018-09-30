/*
 * drivers/amlogic/clk/axg/axg_clk-pll.c
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
 * In the most basic form, a Meson PLL is composed as follows:
 *
 *                     PLL
 *      +------------------------------+
 *      |                              |
 * in -----[ /N ]---[ *M ]---[ >>OD ]----->> out
 *      |         ^        ^           |
 *      +------------------------------+
 *                |        |
 *               FREF     VCO
 *
 * out = (in * M / N) >> OD
 */

#include <linux/clk-provider.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/amlogic/cpu_version.h>
#include <dt-bindings/clock/amlogic,axg-clkc.h>

#if (defined CONFIG_ARM64) || (defined CONFIG_ARM64_A32)
#include "../clkc.h"
#else
#include "m8b/clkc.h"
#endif

#define MESON_PLL_RESET				BIT(29)
#define MESON_PLL_ENABLE			BIT(30)
#define MESON_PLL_LOCK				BIT(31)

/* GXL TXL */
#define GXL_GP0_CNTL1 0xc084a000
#define GXL_GP0_CNTL2 0xb75020be
#define GXL_GP0_CNTL3 0x0a59a288
#define GXL_GP0_CNTL4 0xc000004d
#define GXL_GP0_CNTL5 0x00078000
/* AXG */
#define AXG_MIPI_CNTL0_ENABLE   BIT(29)
#define AXG_MIPI_CNTL0_BANDGAP  BIT(26)
#define AXG_PCIE_PLL_CNTL 0x400106c8
#define AXG_PCIE_PLL_CNTL1 0x0084a2aa
#define AXG_PCIE_PLL_CNTL2 0xb75020be
#define AXG_PCIE_PLL_CNTL3 0x0a47488e
#define AXG_PCIE_PLL_CNTL4 0xc000004d
#define AXG_PCIE_PLL_CNTL5 0x00078000
#define AXG_PCIE_PLL_CNTL6 0x002323c6

#define AXG_HIFI_PLL_CNTL1 0xc084b000
#define AXG_HIFI_PLL_CNTL2 0xb75020be
#define AXG_HIFI_PLL_CNTL3 0x0a6a3a88
#define AXG_HIFI_PLL_CNTL4 0xc000004d
#define AXG_HIFI_PLL_CNTL5 0x000581eb

#define to_meson_clk_pll(_hw) container_of(_hw, struct meson_clk_pll, hw)

static unsigned long meson_axg_pll_recalc_rate(struct clk_hw *hw,
						unsigned long parent_rate)
{
	struct meson_clk_pll *pll = to_meson_clk_pll(hw);
	struct parm *p;
	u64 parent_rate_mhz = parent_rate;
	unsigned long rate_mhz;
	u16 n, m, frac = 0, od, od2 = 0;
	u32 reg;
	u64 tmp64;

	p = &pll->n;
	reg = readl(pll->base + p->reg_off);
	n = PARM_GET(p->width, p->shift, reg);

	p = &pll->m;
	reg = readl(pll->base + p->reg_off);
	m = PARM_GET(p->width, p->shift, reg);

	p = &pll->od;
	reg = readl(pll->base + p->reg_off);
	od = PARM_GET(p->width, p->shift, reg);

	p = &pll->od2;
	if (p->width) {
		reg = readl(pll->base + p->reg_off);
		od2 = PARM_GET(p->width, p->shift, reg);
	}

	p = &pll->frac;

	if (p->width) {
		reg = readl(pll->base + p->reg_off);
		frac = PARM_GET(p->width, p->shift, reg);
		tmp64 = parent_rate_mhz * m + (parent_rate_mhz * frac >> 12);
		do_div(tmp64, n);
		rate_mhz = (unsigned long)tmp64;
		rate_mhz = rate_mhz >> od >> od2;
	} else {
		tmp64 = parent_rate_mhz * m;
		do_div(tmp64, n);
		rate_mhz = tmp64 >> od >> od2;
	}
	return rate_mhz;
}

static long meson_axg_pll_round_rate(struct clk_hw *hw, unsigned long rate,
				     unsigned long *parent_rate)
{
	struct meson_clk_pll *pll = to_meson_clk_pll(hw);
	const struct pll_rate_table *rate_table = pll->rate_table;
	int i;

	for (i = 0; i < pll->rate_count; i++) {
		if (rate <= rate_table[i].rate)
			return rate_table[i].rate;
	}

	/* else return the smallest value */
	return rate_table[0].rate;
}

static const struct pll_rate_table *meson_axg_get_pll_settings
	(struct meson_clk_pll *pll, unsigned long rate)
{
	const struct pll_rate_table *rate_table = pll->rate_table;
	int i;

	for (i = 0; i < pll->rate_count; i++) {
		if (rate == rate_table[i].rate)
			return &rate_table[i];
	}
	return NULL;
}

static int meson_axg_pll_wait_lock(struct meson_clk_pll *pll,
				   struct parm *p_n)
{
	int delay = 24000000;
	u32 reg;

	while (delay > 0) {
		reg = readl(pll->base + p_n->reg_off);

		if (reg & MESON_PLL_LOCK)
			return 0;
		delay--;
	}
	return -ETIMEDOUT;
}

static int meson_axg_pll_set_rate(struct clk_hw *hw, unsigned long rate,
				  unsigned long parent_rate)
{
	struct meson_clk_pll *pll = to_meson_clk_pll(hw);
	struct parm *p;
	const struct pll_rate_table *rate_set;
	unsigned long old_rate;
	unsigned int tmp;
	int ret = 0;
	u32 reg;
	unsigned long flags = 0;

	if (parent_rate == 0 || rate == 0)
		return -EINVAL;

	old_rate = rate;

	rate_set = meson_axg_get_pll_settings(pll, rate);
	if (!rate_set)
		return -EINVAL;

	p = &pll->n;

	if (pll->lock)
		spin_lock_irqsave(pll->lock, flags);

	if (readl(pll->base + p->reg_off) & MESON_PLL_ENABLE) {
		old_rate = meson_axg_pll_recalc_rate(hw,
			clk_get_rate(clk_get_parent(hw->clk)));
		old_rate = meson_axg_pll_round_rate(hw, old_rate, NULL);

		if (old_rate == rate) {
			if (pll->lock)
				spin_unlock_irqrestore(pll->lock, flags);
			return ret;
		}
	}

	if (!strcmp(clk_hw_get_name(hw), "gp0_pll")
		|| !strcmp(clk_hw_get_name(hw), "hifi_pll")
		|| !strcmp(clk_hw_get_name(hw), "pcie_pll")) {
		void *cntlbase = pll->base + p->reg_off;

		if (!strcmp(clk_hw_get_name(hw), "pcie_pll")) {
			writel(AXG_PCIE_PLL_CNTL,
					cntlbase + (unsigned long)(0*4));
			writel(AXG_PCIE_PLL_CNTL1,
					cntlbase + (unsigned long)(1*4));
			writel(AXG_PCIE_PLL_CNTL2,
					cntlbase + (unsigned long)(2*4));
			writel(AXG_PCIE_PLL_CNTL3,
					cntlbase + (unsigned long)(3*4));
			writel(AXG_PCIE_PLL_CNTL4,
					cntlbase + (unsigned long)(4*4));
			writel(AXG_PCIE_PLL_CNTL5,
					cntlbase + (unsigned long)(5*4));
			writel(AXG_PCIE_PLL_CNTL6,
					cntlbase + (unsigned long)(6*4));
		} else if (!strcmp(clk_hw_get_name(hw), "hifi_pll")) {
			writel(AXG_HIFI_PLL_CNTL1,
					cntlbase + (unsigned long)(6*4));
			writel(AXG_HIFI_PLL_CNTL2,
					cntlbase + (unsigned long)(1*4));
			writel(AXG_HIFI_PLL_CNTL3,
					cntlbase + (unsigned long)(2*4));
			writel(AXG_HIFI_PLL_CNTL4,
					cntlbase + (unsigned long)(3*4));
			writel(AXG_HIFI_PLL_CNTL5,
					cntlbase + (unsigned long)(4*4));
		} else {
			writel(GXL_GP0_CNTL1,
					cntlbase + (unsigned long)(6*4));
			writel(GXL_GP0_CNTL2,
					cntlbase + (unsigned long)(1*4));
			writel(GXL_GP0_CNTL3,
					cntlbase + (unsigned long)(2*4));
			writel(GXL_GP0_CNTL4,
					cntlbase + (unsigned long)(3*4));
			writel(GXL_GP0_CNTL5,
					cntlbase + (unsigned long)(4*4));
		}

		reg = readl(pll->base + p->reg_off);
		writel(((reg | (MESON_PLL_ENABLE)) &
			(~MESON_PLL_RESET)), pll->base + p->reg_off);
	}

	reg = readl(pll->base + p->reg_off);

	tmp = rate_set->n;
	reg = PARM_SET(p->width, p->shift, reg, tmp);
	writel(reg, pll->base + p->reg_off);

	p = &pll->m;
	reg = readl(pll->base + p->reg_off);
	tmp = rate_set->m;
	reg = PARM_SET(p->width, p->shift, reg, tmp);
	writel(reg, pll->base + p->reg_off);

	p = &pll->od;
	reg = readl(pll->base + p->reg_off);
	tmp = rate_set->od;
	reg = PARM_SET(p->width, p->shift, reg, tmp);
	writel(reg, pll->base + p->reg_off);

	p = &pll->od2;
	if (p->width) {
		reg = readl(pll->base + p->reg_off);
		tmp = rate_set->od2;
		reg = PARM_SET(p->width, p->shift, reg, tmp);
		writel(reg, pll->base + p->reg_off);
	}

	p = &pll->frac;
	if (p->width) {
		reg = readl(pll->base + p->reg_off);
		tmp = rate_set->frac;
		reg = PARM_SET(p->width, p->shift, reg, tmp);
		writel(reg, pll->base + p->reg_off);
	}

	p = &pll->n;

	/* PLL reset */
	reg = readl(pll->base + p->reg_off);
	writel(reg | MESON_PLL_RESET, pll->base + p->reg_off);
	udelay(10);
	writel(reg & (~MESON_PLL_RESET), pll->base + p->reg_off);

	ret = meson_axg_pll_wait_lock(pll, p);

	if (pll->lock)
		spin_unlock_irqrestore(pll->lock, flags);

	if (ret) {
		pr_warn("%s: pll did not lock, trying to lock rate %lu again\n",
			__func__, rate);
		meson_axg_pll_set_rate(hw, rate, parent_rate);
	}

	return ret;
}

static int meson_axg_pll_enable(struct clk_hw *hw)
{
	struct meson_clk_pll *pll = to_meson_clk_pll(hw);
	struct parm *p;
	int ret = 0;
	unsigned long flags = 0;
	unsigned long first_set = 1;
	struct clk *parent;
	unsigned long rate;

	p = &pll->n;

	if (pll->lock)
		spin_lock_irqsave(pll->lock, flags);

	if (readl(pll->base + p->reg_off) & MESON_PLL_ENABLE) {
		if (pll->lock)
			spin_unlock_irqrestore(pll->lock, flags);
		return ret;
	}

	if (!strcmp(clk_hw_get_name(hw), "gp0_pll")
		|| !strcmp(clk_hw_get_name(hw), "hifi_pll")
		|| !strcmp(clk_hw_get_name(hw), "pcie_pll")) {
		void *cntlbase = pll->base + p->reg_off;

		if (!strcmp(clk_hw_get_name(hw), "pcie_pll")) {
			if (readl(cntlbase + (unsigned long)(6*4))
						== AXG_PCIE_PLL_CNTL6)
				first_set = 0;
		} else if (!strcmp(clk_hw_get_name(hw), "hifi_pll")) {
			if (readl(cntlbase + (unsigned long)(4*4))
						== AXG_HIFI_PLL_CNTL5)
				first_set = 0;
		} else {
			if (readl(cntlbase + (unsigned long)(4*4))
						== GXL_GP0_CNTL5)
				first_set = 0;
		}
	}

	parent = clk_get_parent(hw->clk);

	/*First init, just set minimal rate.*/
	if (first_set)
		rate = pll->rate_table[0].rate;
	else {
		rate = meson_axg_pll_recalc_rate(hw, clk_get_rate(parent));
		rate = meson_axg_pll_round_rate(hw, rate, NULL);
	}

	if (pll->lock)
		spin_unlock_irqrestore(pll->lock, flags);

	ret = meson_axg_pll_set_rate(hw, rate, clk_get_rate(parent));

	return ret;
}

static void meson_axg_pll_disable(struct clk_hw *hw)
{
	struct meson_clk_pll *pll = to_meson_clk_pll(hw);
	struct parm *p = &pll->n;
	unsigned long flags = 0;

	if (!strcmp(clk_hw_get_name(hw), "gp0_pll")
			|| !strcmp(clk_hw_get_name(hw), "hifi_pll")
			|| !strcmp(clk_hw_get_name(hw), "pcie_pll")) {
		if (pll->lock)
			spin_lock_irqsave(pll->lock, flags);

		writel(readl(pll->base + p->reg_off) & (~MESON_PLL_ENABLE),
			pll->base + p->reg_off);

		if (pll->lock)
			spin_unlock_irqrestore(pll->lock, flags);
	}

}

const struct clk_ops meson_axg_pll_ops = {
	.recalc_rate	= meson_axg_pll_recalc_rate,
	.round_rate	= meson_axg_pll_round_rate,
	.set_rate	= meson_axg_pll_set_rate,
	.enable		= meson_axg_pll_enable,
	.disable	= meson_axg_pll_disable,
};

const struct clk_ops meson_axg_pll_ro_ops = {
	.recalc_rate	= meson_axg_pll_recalc_rate,
};

