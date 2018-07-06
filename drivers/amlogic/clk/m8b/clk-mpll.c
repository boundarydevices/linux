/*
 * drivers/amlogic/clk/m8b/clk-mpll.c
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
#include "clkc.h"

#define SDM_DEN 16384
#define SDM_MIN 1
#define SDM_MAX 16383
#define N2_MIN	4
#define N2_MAX	511

#define to_meson_clk_mpll(_hw) container_of(_hw, struct meson_clk_mpll, hw)

static unsigned long rate_from_params(u64 parent_rate,
				      unsigned long sdm,
				      unsigned long n2)
{
	return DIV_ROUND_UP_ULL(((uint64_t)parent_rate * SDM_DEN),
							((SDM_DEN * n2) + sdm));
}

static void params_from_rate(unsigned long requested_rate,
			     unsigned long parent_rate,
			     unsigned long *sdm,
			     unsigned long *n2)
{
	uint64_t div = parent_rate;
	uint64_t rem = do_div(div, requested_rate);

	if (div < N2_MIN) {
		*n2 = N2_MIN;
		*sdm = SDM_MIN;
	} else if (div > N2_MAX) {
		*n2 = N2_MAX;
		*sdm = SDM_MAX;
	} else {
		*n2 = div;
		*sdm = DIV_ROUND_UP_ULL(rem * SDM_DEN, requested_rate);
		if (*sdm < SDM_MIN)
			*sdm = SDM_MIN;
		else if (*sdm > SDM_MAX)
			*sdm = SDM_MAX;
	}
}

static unsigned long mpll_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct meson_clk_mpll *mpll = to_meson_clk_mpll(hw);
	struct parm *p;
	unsigned long reg, sdm, n2;

	p = &mpll->sdm;
	reg = readl(mpll->base + p->reg_off);
	sdm = PARM_GET(p->width, p->shift, reg);

	p = &mpll->n2;
	reg = readl(mpll->base + p->reg_off);
	n2 = PARM_GET(p->width, p->shift, reg);

	if ((!sdm) || (!n2))
		return 0;
	else
		return rate_from_params(parent_rate, sdm, n2);
}

static long mpll_round_rate(struct clk_hw *hw,
			    unsigned long rate,
			    unsigned long *parent_rate)
{
	unsigned long sdm, n2;

	params_from_rate(rate, *parent_rate, &sdm, &n2);
	return rate_from_params(*parent_rate, sdm, n2);
}

static int mpll_set_rate(struct clk_hw *hw,
			 unsigned long rate,
			 unsigned long parent_rate)
{
	struct meson_clk_mpll *mpll = to_meson_clk_mpll(hw);
	struct parm *p;
	unsigned long reg, sdm, n2;
	unsigned long flags = 0;

	params_from_rate(rate, parent_rate, &sdm, &n2);

	if (mpll->lock)
		spin_lock_irqsave(mpll->lock, flags);
	else
		__acquire(mpll->lock);

	p = &mpll->sdm;
	reg = readl(mpll->base + p->reg_off);
	reg = PARM_SET(p->width, p->shift, reg, sdm);
	writel(reg, mpll->base + p->reg_off);

	p = &mpll->sdm_en;
	reg = readl(mpll->base + p->reg_off);
	reg = PARM_SET(p->width, p->shift, reg, 1);
	writel(reg, mpll->base + p->reg_off);

	p = &mpll->n2;
	reg = readl(mpll->base + p->reg_off);
	reg = PARM_SET(p->width, p->shift, reg, n2);
	writel(reg, mpll->base + p->reg_off);

	if (mpll->lock)
		spin_unlock_irqrestore(mpll->lock, flags);
	else
		__release(mpll->lock);

	return 0;
}

static void mpll_enable_core(struct clk_hw *hw, int enable)
{
	struct meson_clk_mpll *mpll = to_meson_clk_mpll(hw);
	struct parm *p;
	unsigned long reg;
	unsigned long flags = 0;

	if (mpll->lock)
		spin_lock_irqsave(mpll->lock, flags);
	else
		__acquire(mpll->lock);

	p = &mpll->en;
	reg = readl(mpll->base + p->reg_off);
	reg = PARM_SET(p->width, p->shift, reg, enable ? 1 : 0);
	writel(reg, mpll->base + p->reg_off);

	if (mpll->lock)
		spin_unlock_irqrestore(mpll->lock, flags);
	else
		__release(mpll->lock);
}


static int mpll_enable(struct clk_hw *hw)
{
	mpll_enable_core(hw, 1);

	return 0;
}

static void mpll_disable(struct clk_hw *hw)
{
	mpll_enable_core(hw, 0);
}

static int mpll_is_enabled(struct clk_hw *hw)
{
	struct meson_clk_mpll *mpll = to_meson_clk_mpll(hw);
	struct parm *p;
	unsigned long reg;
	int en;

	p = &mpll->en;
	reg = readl(mpll->base + p->reg_off);
	en = PARM_GET(p->width, p->shift, reg);

	return en;
}

const struct clk_ops meson_clk_mpll_ro_ops = {
	.recalc_rate	= mpll_recalc_rate,
	.round_rate	= mpll_round_rate,
	.is_enabled	= mpll_is_enabled,
};

const struct clk_ops meson_clk_mpll_ops = {
	.recalc_rate	= mpll_recalc_rate,
	.round_rate	= mpll_round_rate,
	.set_rate	= mpll_set_rate,
	.enable		= mpll_enable,
	.disable	= mpll_disable,
	.is_enabled	= mpll_is_enabled,
};
