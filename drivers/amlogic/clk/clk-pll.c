/*
 * drivers/amlogic/clk/clk-pll.c
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
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/amlogic/cpu_version.h>

#if (defined CONFIG_ARM64) || (defined CONFIG_ARM64_A32)
#include "clkc.h"
#else
#include "m8b/clkc.h"
#endif

#define MESON_PLL_RESET				BIT(29)
#define MESON_PLL_ENABLE			BIT(30)
#define MESON_PLL_LOCK				BIT(31)

/* GXBB GXTVBB */
#define GXBB_GP0_CNTL2 0x69c80000
#define GXBB_GP0_CNTL3 0x0a674a21
#define GXBB_GP0_CNTL4 0xc000000d

/* GXL TXL */
#define GXL_GP0_CNTL1 0xc084a000
#define GXL_GP0_CNTL2 0xb75020be
#define GXL_GP0_CNTL3 0x0a59a288
#define GXL_GP0_CNTL4 0xc000004d
#define GXL_GP0_CNTL5 0x00078000

/* TXLX */
/* CNTL2-5 the same to GXL*/
#define TXLL_GP0_CNTL5 0x00058000


#define to_meson_clk_pll(_hw) container_of(_hw, struct meson_clk_pll, hw)

static unsigned long meson_clk_pll_recalc_rate(struct clk_hw *hw,
						unsigned long parent_rate)
{
	struct meson_clk_pll *pll = to_meson_clk_pll(hw);
	struct parm *p;
	unsigned long parent_rate_mhz = parent_rate / 1000000;
	unsigned long rate_mhz;
	u16 n, m, frac = 0, od, od2 = 0;
	u32 reg;

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
		rate_mhz = (parent_rate_mhz * m +
				(parent_rate_mhz * frac >> 12)) * 2 / n;
		rate_mhz = rate_mhz >> od >> od2;
	} else
		rate_mhz = (parent_rate_mhz * m / n) >> od >> od2;

	return rate_mhz * 1000000;
}

static long meson_clk_pll_round_rate(struct clk_hw *hw, unsigned long rate,
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

static const struct pll_rate_table *meson_clk_get_pll_settings
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

static int meson_clk_pll_wait_lock(struct meson_clk_pll *pll,
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

static int meson_clk_pll_set_rate(struct clk_hw *hw, unsigned long rate,
				  unsigned long parent_rate)
{
	struct meson_clk_pll *pll = to_meson_clk_pll(hw);
	struct parm *p;
	const struct pll_rate_table *rate_set;
	unsigned long old_rate;
	unsigned int tmp;
	int ret = 0;
	u32 reg;

	if (parent_rate == 0 || rate == 0)
		return -EINVAL;

	old_rate = rate;

	rate_set = meson_clk_get_pll_settings(pll, rate);
	if (!rate_set)
		return -EINVAL;

	p = &pll->n;

	if (!strcmp(clk_hw_get_name(hw), "gp0_pll")) {
		void *cntlbase = pll->base + p->reg_off;

		if ((get_cpu_type() == MESON_CPU_MAJOR_ID_GXBB) ||
			(get_cpu_type() == MESON_CPU_MAJOR_ID_GXTVBB)) {
			writel(GXBB_GP0_CNTL2,
					cntlbase + (unsigned long)(1*4));
			writel(GXBB_GP0_CNTL3,
					cntlbase + (unsigned long)(2*4));
			writel(GXBB_GP0_CNTL4,
					cntlbase + (unsigned long)(3*4));
		} else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXL) {
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

			reg = readl(pll->base + p->reg_off);
			writel(((reg | (MESON_PLL_ENABLE)) &
				(~MESON_PLL_RESET)), pll->base + p->reg_off);
		} else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_TXLX) {
			writel(GXL_GP0_CNTL1,
					cntlbase + (unsigned long)(6*4));
			writel(GXL_GP0_CNTL2,
					cntlbase + (unsigned long)(1*4));
			writel(GXL_GP0_CNTL3,
					cntlbase + (unsigned long)(2*4));
			writel(GXL_GP0_CNTL4,
					cntlbase + (unsigned long)(3*4));
			writel(TXLL_GP0_CNTL5,
					cntlbase + (unsigned long)(4*4));

			reg = readl(pll->base + p->reg_off);
			writel(((reg | (MESON_PLL_ENABLE)) &
				(~MESON_PLL_RESET)), pll->base + p->reg_off);
		}

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

	ret = meson_clk_pll_wait_lock(pll, p);
	if (ret) {
		pr_warn("%s: pll did not lock, trying to restore old rate %lu\n",
			__func__, old_rate);
		meson_clk_pll_set_rate(hw, old_rate, parent_rate);
	}

	return ret;
}

const struct clk_ops meson_clk_pll_ops = {
	.recalc_rate	= meson_clk_pll_recalc_rate,
	.round_rate	= meson_clk_pll_round_rate,
	.set_rate	= meson_clk_pll_set_rate,
};

const struct clk_ops meson_clk_pll_ro_ops = {
	.recalc_rate	= meson_clk_pll_recalc_rate,
};
