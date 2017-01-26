/*
 * drivers/amlogic/clk/clk-mpll.c
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

#include "clkc.h"
/* #undef pr_debug */
/* #define pr_debug pr_info */
#define SDM_MAX 16384

#define to_meson_clk_mpll(_hw) container_of(_hw, struct meson_clk_mpll, hw)

static unsigned long mpll_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct meson_clk_mpll *mpll = to_meson_clk_mpll(hw);
	struct parm *p;
	unsigned long rate = 0;
	unsigned long reg, sdm, n2;

	p = &mpll->sdm;
	reg = readl(mpll->base + p->reg_off);
	sdm = PARM_GET(p->width, p->shift, reg);

	p = &mpll->n2;
	reg = readl(mpll->base + p->reg_off);
	n2 = PARM_GET(p->width, p->shift, reg);

	rate = (parent_rate * SDM_MAX) / ((SDM_MAX * n2) + sdm);

	return rate;
}

static long meson_clk_pll_round_rate(struct clk_hw *hw, unsigned long rate,
				     unsigned long *parent_rate)
{
	return rate;
}


static int mpll_set_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long parent_rate)
{
	struct meson_clk_mpll *mpll = to_meson_clk_mpll(hw);
	struct parm *p;
	unsigned long old_rate = 0;
	unsigned long reg, old_sdm, old_n2, sdm, n2;
	unsigned long flags = 0;

	if ((rate > 500000000) || (rate < 250000000)) {
		pr_err("Err: can not set rate to %lu!\n", rate);
		pr_err("Range[250000000 - 500000000]\n");
		return -1;
	}

	if (mpll->lock)
		spin_lock_irqsave(mpll->lock, flags);

	p = &mpll->sdm;
	reg = readl(mpll->base + p->reg_off);
	old_sdm = PARM_GET(p->width, p->shift, reg);

	p = &mpll->n2;
	reg = readl(mpll->base + p->reg_off);
	old_n2 = PARM_GET(p->width, p->shift, reg);

	old_rate = (parent_rate * SDM_MAX) / ((SDM_MAX * old_n2) + old_sdm);
	pr_debug("%s: old_sdm: %lu old_n2: %lu old_rate: %lu\n", __func__,
		old_sdm, old_n2, old_rate);

	if (old_rate == rate)
		return 0;

	/* calculate new n2 and sdm */
	n2 = parent_rate / rate;
	sdm = DIV_ROUND_UP((parent_rate - n2 * rate) * SDM_MAX, rate);
	pr_debug("%s: sdm: %lu n2: %lu rate: %lu\n", __func__, sdm, n2, rate);

	if (old_n2 != n2 || old_sdm != sdm) {
		p = &mpll->sdm;
		reg = readl(mpll->base + p->reg_off);
		reg = PARM_SET(p->width, p->shift, reg, sdm);
		p = &mpll->n2;
		reg = PARM_SET(p->width, p->shift, reg, n2);
		reg = PARM_SET(1, mpll->sdm_en, reg, 1);
		reg = PARM_SET(1, mpll->en_dss, reg, 1);
		if (!strcmp(clk_hw_get_name(hw), "mpll3"))
			/* MPLL_CNTL10 bit14 should be set together
			 * with MPLL3_CNTL0 bit0
			 */
			writel(readl(mpll->base + p->reg_off - 0x3c) |
				0x1<<14, mpll->base + p->reg_off - 0x3c);
		writel(reg, mpll->base + p->reg_off);
		udelay(100);
	}

	if (mpll->lock)
		spin_unlock_irqrestore(mpll->lock, flags);

	return 0;
}

const struct clk_ops meson_clk_mpll_ops = {
	.recalc_rate = mpll_recalc_rate,
	.round_rate	= meson_clk_pll_round_rate,
	.set_rate = mpll_set_rate,
};

const struct clk_ops meson_clk_mpll_ro_ops = {
	.recalc_rate = mpll_recalc_rate,
};
