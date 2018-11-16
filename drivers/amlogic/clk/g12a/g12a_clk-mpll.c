/*
 * drivers/amlogic/clk/g12a/g12a_clk-mpll.c
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
 * MultiPhase Locked Loops are outputs from a PLL with additional frequency
 * scaling capabilities. MPLL rates are calculated as:
 *
 * f(N2_integer, SDM_IN ) = 2.0G/(N2_integer + SDM_IN/16384)
 */

#include <linux/clk-provider.h>
#include <linux/delay.h>

#include "../clkc.h"
/* #undef pr_debug */
/* #define pr_debug pr_info */
#define SDM_MAX 16384ULL
#define MAX_RATE	500000000
#define MIN_RATE	3920000

#define G12A_MPLL_CNTL0 0x00000543
#define G12A_MPLL_CNTL2 0x40000033


#define to_meson_clk_mpll(_hw) container_of(_hw, struct meson_clk_mpll, hw)

static unsigned long mpll_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct meson_clk_mpll *mpll = to_meson_clk_mpll(hw);
	struct parm *p;
	unsigned long rate = 0;
	unsigned long reg, sdm, n2;
	u64 rate64 = parent_rate;

	p = &mpll->sdm;
	reg = readl(mpll->base + p->reg_off);
	sdm = PARM_GET(p->width, p->shift, reg);

	p = &mpll->n2;
	reg = readl(mpll->base + p->reg_off);
	n2 = PARM_GET(p->width, p->shift, reg);

	if (n2 == 0 && sdm == 0) {
		rate = 0;
	} else {
		rate64 = rate64 * SDM_MAX;
		do_div(rate64, ((SDM_MAX * n2) + sdm));
		rate = rate64;
	}

	return rate;
}

static long meson_clk_pll_round_rate(struct clk_hw *hw, unsigned long rate,
				     unsigned long *parent_rate)
{
	unsigned long rate_val = rate;

	if (rate_val < MIN_RATE)
		rate = MIN_RATE;
	if (rate_val > MAX_RATE)
		rate = MAX_RATE;

	return rate_val;
}


static int mpll_set_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long parent_rate)
{
	struct meson_clk_mpll *mpll = to_meson_clk_mpll(hw);
	struct parm *p;
	unsigned long reg, sdm, n2;
	unsigned long flags = 0;
	uint64_t rate64 = parent_rate;

	if ((rate > MAX_RATE) || (rate < MIN_RATE)) {
		pr_err("Err: can not set rate to %lu!\n", rate);
		pr_err("Range[3920000 - 500000000]\n");
		return -1;
	}

	if (mpll->lock)
		spin_lock_irqsave(mpll->lock, flags);

	/* calculate new n2 and sdm */
	do_div(rate64, rate);
	n2 = rate64;

	rate64 = (parent_rate - n2 * rate) * SDM_MAX + rate - 1;
	do_div(rate64, rate);
	sdm = rate64;
	if (sdm >= SDM_MAX)
		sdm = SDM_MAX - 1;

	pr_debug("%s: sdm: %lu n2: %lu rate: %lu parent_rate: %lu\n",
		__func__, sdm, n2, rate, parent_rate);

	writel(G12A_MPLL_CNTL0, mpll->base + mpll->mpll_cntl0_reg);

	p = &mpll->sdm;
	writel(G12A_MPLL_CNTL2, mpll->base + p->reg_off + (unsigned long)(1*4));
	reg = readl(mpll->base + p->reg_off);
	reg = PARM_SET(p->width, p->shift, reg, sdm);
	p = &mpll->n2;
	reg = PARM_SET(p->width, p->shift, reg, n2);
	reg = PARM_SET(1, mpll->sdm_en, reg, 1);
	reg = PARM_SET(1, mpll->en_dds, reg, 1);
	writel(reg, mpll->base + p->reg_off);

	if (mpll->lock)
		spin_unlock_irqrestore(mpll->lock, flags);

	return 0;
}

static int mpll_enable(struct clk_hw *hw)
{
	struct meson_clk_mpll *mpll = to_meson_clk_mpll(hw);
	struct parm *p = &mpll->sdm;
	unsigned long reg;
	unsigned long flags = 0;

	if (mpll->lock)
		spin_lock_irqsave(mpll->lock, flags);

	reg = readl(mpll->base + p->reg_off);
	reg = PARM_SET(1, mpll->sdm_en, reg, 1);
	reg = PARM_SET(1, mpll->en_dds, reg, 1);
	writel(reg, mpll->base + p->reg_off);

	if (mpll->lock)
		spin_unlock_irqrestore(mpll->lock, flags);

	return 0;
}

static void mpll_disable(struct clk_hw *hw)
{
	struct meson_clk_mpll *mpll = to_meson_clk_mpll(hw);
	struct parm *p = &mpll->sdm;
	unsigned long reg;
	unsigned long flags = 0;

	if (mpll->lock)
		spin_lock_irqsave(mpll->lock, flags);

	reg = readl(mpll->base + p->reg_off);
	reg = PARM_SET(1, mpll->sdm_en, reg, 0);
	reg = PARM_SET(1, mpll->en_dds, reg, 0);
	writel(reg, mpll->base + p->reg_off);

	if (mpll->lock)
		spin_unlock_irqrestore(mpll->lock, flags);
}

const struct clk_ops meson_g12a_mpll_ops = {
	.recalc_rate = mpll_recalc_rate,
	.round_rate	= meson_clk_pll_round_rate,
	.set_rate = mpll_set_rate,
	.enable = mpll_enable,
	.disable = mpll_disable,
};

const struct clk_ops meson_g12a_mpll_ro_ops = {
	.recalc_rate = mpll_recalc_rate,
};
