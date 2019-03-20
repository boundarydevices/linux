/*
 * drivers/amlogic/clk/g12a/g12a_clk-pll.c
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
#include <dt-bindings/clock/amlogic,g12a-clkc.h>

#include "../clkc.h"

#define MESON_PLL_RESET				BIT(29)
#define MESON_PLL_ENABLE			BIT(28)
#define MESON_PLL_LOCK				BIT(31)

/* G12A */

//#define G12A_PCIE_PLL_CNTL 0x400106c8
#define G12A_PCIE_PLL_CNTL0_0  0x28060464
#define G12A_PCIE_PLL_CNTL0_1  0x38060464
#define G12A_PCIE_PLL_CNTL0_2  0x3c060464
#define G12A_PCIE_PLL_CNTL0_3  0x1c060464
#define G12A_PCIE_PLL_CNTL1  0x00000000
#define G12A_PCIE_PLL_CNTL2  0x00001100
#define G12A_PCIE_PLL_CNTL2_ 0x00001000
#define G12A_PCIE_PLL_CNTL3  0x10058e00
#define G12A_PCIE_PLL_CNTL4  0x000100c0
#define G12A_PCIE_PLL_CNTL4_ 0x008100c0
#define G12A_PCIE_PLL_CNTL5  0x68000048
#define G12A_PCIE_PLL_CNTL5_ 0x68000068

#define G12B_PCIE_PLL_CNTL0_0  0x28060464
#define G12B_PCIE_PLL_CNTL0_1  0x38060464
#define G12B_PCIE_PLL_CNTL0_2  0x3c060464
#define G12B_PCIE_PLL_CNTL0_3  0x1c060464
#define G12B_PCIE_PLL_CNTL1  0x00000000
#define G12B_PCIE_PLL_CNTL2  0x00001100
#define G12B_PCIE_PLL_CNTL2_ 0x00001000
#define G12B_PCIE_PLL_CNTL3  0x10058e00
#define G12B_PCIE_PLL_CNTL4  0x000100c0
#define G12B_PCIE_PLL_CNTL4_ 0x008100c0
#define G12B_PCIE_PLL_CNTL5  0x68000048
#define G12B_PCIE_PLL_CNTL5_ 0x68000068

#define G12A_SYS_PLL_CNTL1 0x00000000
#define G12A_SYS_PLL_CNTL2 0x00000000
#define G12A_SYS_PLL_CNTL3 0x48681c00
#define G12A_SYS_PLL_CNTL4 0x88770290
#define G12A_SYS_PLL_CNTL5 0x39272000

#define G12A_SYS1_PLL_CNTL1 0x00000000
#define G12A_SYS1_PLL_CNTL2 0x00000000
#define G12A_SYS1_PLL_CNTL3 0x48681c00
#define G12A_SYS1_PLL_CNTL4 0x88770290
#define G12A_SYS1_PLL_CNTL5 0x39272000

#define G12A_GP0_PLL_CNTL1 0x00000000
#define G12A_GP0_PLL_CNTL2 0x00000000
#define G12A_GP0_PLL_CNTL3 0x48681c00
#define G12A_GP0_PLL_CNTL4 0x33771290
#define G12A_GP0_PLL_CNTL5 0x39272000

#define G12A_HIFI_PLL_CNTL1 0x00000000
#define G12A_HIFI_PLL_CNTL2 0x00000000
#define G12A_HIFI_PLL_CNTL3 0x6a285c00
#define G12A_HIFI_PLL_CNTL4 0x65771290
#define G12A_HIFI_PLL_CNTL5 0x39272000

#define G12A_PLL_CNTL6 0x56540000

#define to_meson_clk_pll(_hw) container_of(_hw, struct meson_clk_pll, hw)

static unsigned long meson_g12a_pll_recalc_rate(struct clk_hw *hw,
						unsigned long parent_rate)
{
	struct meson_clk_pll *pll = to_meson_clk_pll(hw);
	struct parm *p;
	u64 parent_rate_mhz = parent_rate;
	unsigned long rate_mhz;
	u16 n, m, od, od2 = 0;
	u32 reg, frac = 0;
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

	if (p->width >= 2) {
		reg = readl(pll->base + p->reg_off);
		frac = PARM_GET(p->width - 1, p->shift, reg);

		if (reg & (1 << (p->width - 1))) {
			tmp64 = (parent_rate_mhz * m -
				((parent_rate_mhz * frac)
				>> (p->width - 2)));
			do_div(tmp64, n);
			rate_mhz = (unsigned long)tmp64;
		} else {
			tmp64 = (parent_rate_mhz * m +
				((parent_rate_mhz * frac)
				>> (p->width - 2)));
			do_div(tmp64, n);
			rate_mhz = (unsigned long)tmp64;
		}

		if (!strcmp(clk_hw_get_name(hw), "pcie_pll"))
			rate_mhz = rate_mhz/4/od;
		else
			rate_mhz = rate_mhz >> od;
	} else {
		if (!strcmp(clk_hw_get_name(hw), "pcie_pll")) {
			tmp64 = parent_rate_mhz * m;
			do_div(tmp64, n * 4 * od);
			rate_mhz = (unsigned long)tmp64;
		} else {
			tmp64 = parent_rate_mhz * m;
			do_div(tmp64, n);
			rate_mhz = tmp64 >> od >> od2;
		}
	}

	return rate_mhz;
}

static long meson_g12a_pll_round_rate(struct clk_hw *hw, unsigned long rate,
				     unsigned long *parent_rate)
{
	struct meson_clk_pll *pll = to_meson_clk_pll(hw);
	const struct pll_rate_table *rate_table = pll->rate_table;
	int i;
	u64 ret_rate = 0;

	for (i = 0; i < pll->rate_count; i++) {
		if (rate <= rate_table[i].rate) {
			ret_rate = rate_table[i].rate;
			if (!strcmp(clk_hw_get_name(hw), "sys_pll")
				|| !strcmp(clk_hw_get_name(hw), "sys1_pll"))
				do_div(ret_rate, 1000);
			return ret_rate;
		}
	}

	/* else return the smallest value */
	ret_rate = rate_table[0].rate;
	if (!strcmp(clk_hw_get_name(hw), "sys_pll"))
		do_div(ret_rate, 1000);
	return ret_rate;
}

static const struct pll_rate_table *meson_g12a_get_pll_settings
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

static int meson_g12a_pll_wait_lock(struct meson_clk_pll *pll,
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

static int meson_g12a_pll_set_rate(struct clk_hw *hw, unsigned long rate,
				  unsigned long parent_rate)
{
	struct meson_clk_pll *pll = to_meson_clk_pll(hw);
	struct parm *p;
	const struct pll_rate_table *rate_set;
	unsigned long old_rate;
	unsigned long tmp;
	int ret = 0;
	u32 reg;
	unsigned long flags = 0;
	void *cntlbase;

	if (parent_rate == 0 || rate == 0)
		return -EINVAL;
	if (!strcmp(clk_hw_get_name(hw), "sys_pll")
		|| !strcmp(clk_hw_get_name(hw), "sys1_pll"))
		rate *= 1000;

	old_rate = rate;

	rate_set = meson_g12a_get_pll_settings(pll, rate);
	if (!rate_set)
		return -EINVAL;

	p = &pll->n;

	if (pll->lock)
		spin_lock_irqsave(pll->lock, flags);

	if (readl(pll->base + p->reg_off) & MESON_PLL_ENABLE) {
		old_rate = meson_g12a_pll_recalc_rate(hw, parent_rate);
		old_rate = meson_g12a_pll_round_rate(hw, old_rate, NULL);

		if (old_rate == rate) {
			if (pll->lock)
				spin_unlock_irqrestore(pll->lock, flags);
			return ret;
		}
	}

	cntlbase = pll->base + p->reg_off;

	if (!strcmp(clk_hw_get_name(hw), "pcie_pll")) {
		if ((get_cpu_type() == MESON_CPU_MAJOR_ID_G12A) ||
			((get_cpu_type() == MESON_CPU_MAJOR_ID_SM1))) {
			writel(G12A_PCIE_PLL_CNTL0_0,
				cntlbase + (unsigned long)(0*4));
			writel(G12A_PCIE_PLL_CNTL0_1,
				cntlbase + (unsigned long)(0*4));
			writel(G12A_PCIE_PLL_CNTL1,
				cntlbase + (unsigned long)(1*4));
			writel(G12A_PCIE_PLL_CNTL2,
				cntlbase + (unsigned long)(2*4));
			writel(G12A_PCIE_PLL_CNTL3,
				cntlbase + (unsigned long)(3*4));
			writel(G12A_PCIE_PLL_CNTL4,
				cntlbase + (unsigned long)(4*4));
			writel(G12A_PCIE_PLL_CNTL5,
				cntlbase + (unsigned long)(5*4));
			writel(G12A_PCIE_PLL_CNTL5_,
				cntlbase + (unsigned long)(5*4));
			udelay(20);
			writel(G12A_PCIE_PLL_CNTL4_,
				cntlbase + (unsigned long)(4*4));
			udelay(10);
			/*set pcie_apll_afc_start bit*/
			writel(G12A_PCIE_PLL_CNTL0_2,
				cntlbase + (unsigned long)(0*4));
			writel(G12A_PCIE_PLL_CNTL0_3,
				cntlbase + (unsigned long)(0*4));
			udelay(10);
			writel(G12A_PCIE_PLL_CNTL2_,
				cntlbase + (unsigned long)(2*4));
		} else if (get_cpu_type() == MESON_CPU_MAJOR_ID_G12B) {
			writel(G12B_PCIE_PLL_CNTL0_0,
				cntlbase + (unsigned long)(0*4));
			writel(G12B_PCIE_PLL_CNTL0_1,
				cntlbase + (unsigned long)(0*4));
			writel(G12B_PCIE_PLL_CNTL1,
				cntlbase + (unsigned long)(1*4));
			writel(G12B_PCIE_PLL_CNTL2,
				cntlbase + (unsigned long)(2*4));
			writel(G12B_PCIE_PLL_CNTL3,
				cntlbase + (unsigned long)(3*4));
			writel(G12B_PCIE_PLL_CNTL4,
				cntlbase + (unsigned long)(4*4));
			writel(G12B_PCIE_PLL_CNTL5,
				cntlbase + (unsigned long)(5*4));
			writel(G12B_PCIE_PLL_CNTL5_,
				cntlbase + (unsigned long)(5*4));
			udelay(20);
			writel(G12B_PCIE_PLL_CNTL4_,
				cntlbase + (unsigned long)(4*4));
			udelay(10);
			/*set pcie_apll_afc_start bit*/
			writel(G12B_PCIE_PLL_CNTL0_2,
				cntlbase + (unsigned long)(0*4));
			writel(G12B_PCIE_PLL_CNTL0_3,
				cntlbase + (unsigned long)(0*4));
			udelay(10);
			writel(G12B_PCIE_PLL_CNTL2_,
				cntlbase + (unsigned long)(2*4));
		}
		goto OUT;
	} else if (!strcmp(clk_hw_get_name(hw), "sys_pll")) {
		writel((readl(cntlbase) | MESON_PLL_RESET)
			& (~MESON_PLL_ENABLE), cntlbase);
		writel(G12A_SYS_PLL_CNTL1,
				cntlbase + (unsigned long)(1*4));
		writel(G12A_SYS_PLL_CNTL2,
				cntlbase + (unsigned long)(2*4));
		writel(G12A_SYS_PLL_CNTL3,
				cntlbase + (unsigned long)(3*4));
		writel(G12A_SYS_PLL_CNTL4,
				cntlbase + (unsigned long)(4*4));
		writel(G12A_SYS_PLL_CNTL5,
				cntlbase + (unsigned long)(5*4));
		writel(G12A_PLL_CNTL6,
				cntlbase + (unsigned long)(6*4));
		udelay(10);
	} else if (!strcmp(clk_hw_get_name(hw), "sys1_pll")) {
		writel((readl(cntlbase) | MESON_PLL_RESET)
			& (~MESON_PLL_ENABLE), cntlbase);
		writel(G12A_SYS1_PLL_CNTL1,
				cntlbase + (unsigned long)(1*4));
		writel(G12A_SYS1_PLL_CNTL2,
				cntlbase + (unsigned long)(2*4));
		writel(G12A_SYS1_PLL_CNTL3,
				cntlbase + (unsigned long)(3*4));
		writel(G12A_SYS1_PLL_CNTL4,
				cntlbase + (unsigned long)(4*4));
		writel(G12A_SYS1_PLL_CNTL5,
				cntlbase + (unsigned long)(5*4));
		writel(G12A_PLL_CNTL6,
				cntlbase + (unsigned long)(6*4));
		udelay(10);
	} else if (!strcmp(clk_hw_get_name(hw), "gp0_pll")) {
		writel((readl(cntlbase) | MESON_PLL_RESET)
			& (~MESON_PLL_ENABLE), cntlbase);
		writel(G12A_GP0_PLL_CNTL1,
				cntlbase + (unsigned long)(1*4));
		writel(G12A_GP0_PLL_CNTL2,
				cntlbase + (unsigned long)(2*4));
		writel(G12A_GP0_PLL_CNTL3,
				cntlbase + (unsigned long)(3*4));
		writel(G12A_GP0_PLL_CNTL4,
				cntlbase + (unsigned long)(4*4));
		writel(G12A_GP0_PLL_CNTL5,
				cntlbase + (unsigned long)(5*4));
		writel(G12A_PLL_CNTL6,
				cntlbase + (unsigned long)(6*4));
		udelay(10);
	} else if (!strcmp(clk_hw_get_name(hw), "hifi_pll")) {
		writel((readl(cntlbase) | MESON_PLL_RESET)
			& (~MESON_PLL_ENABLE), cntlbase);
		writel(G12A_GP0_PLL_CNTL1,
				cntlbase + (unsigned long)(1*4));
		writel(G12A_GP0_PLL_CNTL2,
				cntlbase + (unsigned long)(2*4));
		writel(G12A_GP0_PLL_CNTL3,
				cntlbase + (unsigned long)(3*4));
		writel(G12A_GP0_PLL_CNTL4,
				cntlbase + (unsigned long)(4*4));
		writel(G12A_GP0_PLL_CNTL5,
				cntlbase + (unsigned long)(5*4));
		writel(G12A_PLL_CNTL6,
				cntlbase + (unsigned long)(6*4));
		udelay(10);
	} else {
		pr_err("%s: %s pll not found!!!\n",
			__func__, clk_hw_get_name(hw));
		return -EINVAL;
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
	/*check OD width*/
	if (rate_set->od >> p->width) {
		ret = -EINVAL;
		pr_warn("%s: OD width is wrong at rate %lu !!\n",
			__func__, rate);
		goto OUT;
	}
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
	writel(readl(pll->base + p->reg_off) | MESON_PLL_ENABLE,
		pll->base + p->reg_off);
	udelay(50);
	writel(readl(pll->base + p->reg_off) & (~MESON_PLL_RESET),
		pll->base + p->reg_off);
	ret = meson_g12a_pll_wait_lock(pll, p);

OUT:
	if (pll->lock)
		spin_unlock_irqrestore(pll->lock, flags);

	if (ret) {
		pr_warn("%s: pll did not lock, trying to lock rate %lu again\n",
			__func__, rate);
		meson_g12a_pll_set_rate(hw, rate, parent_rate);
	}

	return ret;
}

static int meson_g12a_pll_enable(struct clk_hw *hw)
{
	struct meson_clk_pll *pll = to_meson_clk_pll(hw);
	struct parm *p;
	int ret = 0;
	unsigned long flags = 0;
	unsigned long first_set = 1;
	struct clk_hw *parent;
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
		|| !strcmp(clk_hw_get_name(hw), "pcie_pll")
		|| !strcmp(clk_hw_get_name(hw), "sys_pll")
		|| !strcmp(clk_hw_get_name(hw), "sys1_pll")) {
		void *cntlbase = pll->base + p->reg_off;

		if (!strcmp(clk_hw_get_name(hw), "pcie_pll")) {
			if (readl(cntlbase + (unsigned long)(3*4))
						== G12A_PCIE_PLL_CNTL3)
				first_set = 0;
		} else {
			if (readl(cntlbase + (unsigned long)(6*4))
						== G12A_PLL_CNTL6)
				first_set = 0;
		}
	}

	parent = clk_hw_get_parent(hw);

	/*First init, just set minimal rate.*/
	if (first_set)
		rate = pll->rate_table[0].rate;
	else {
		rate = meson_g12a_pll_recalc_rate(hw, clk_hw_get_rate(parent));
		rate = meson_g12a_pll_round_rate(hw, rate, NULL);
	}

	if (pll->lock)
		spin_unlock_irqrestore(pll->lock, flags);

	ret = meson_g12a_pll_set_rate(hw, rate, clk_hw_get_rate(parent));

	return ret;
}

static void meson_g12a_pll_disable(struct clk_hw *hw)
{
	struct meson_clk_pll *pll = to_meson_clk_pll(hw);
	struct parm *p = &pll->n;
	unsigned long flags = 0;

	if (pll->lock)
		spin_lock_irqsave(pll->lock, flags);

	writel(readl(pll->base + p->reg_off) | (MESON_PLL_RESET),
		pll->base + p->reg_off);
	writel(readl(pll->base + p->reg_off) & (~MESON_PLL_ENABLE),
		pll->base + p->reg_off);

	if (!strcmp(clk_hw_get_name(hw), "pcie_pll")) {
		writel(0x20000060,
		       pll->base + p->reg_off + 0x14);
	}

	if (pll->lock)
		spin_unlock_irqrestore(pll->lock, flags);
}

const struct clk_ops meson_g12a_pll_ops = {
	.recalc_rate	= meson_g12a_pll_recalc_rate,
	.round_rate	= meson_g12a_pll_round_rate,
	.set_rate	= meson_g12a_pll_set_rate,
	.enable		= meson_g12a_pll_enable,
	.disable	= meson_g12a_pll_disable,
};

const struct clk_ops meson_g12a_pcie_pll_ops = {
	.recalc_rate	= meson_g12a_pll_recalc_rate,
	.enable		= meson_g12a_pll_enable,
	.disable	= meson_g12a_pll_disable,
};

const struct clk_ops meson_g12a_pll_ro_ops = {
	.recalc_rate	= meson_g12a_pll_recalc_rate,
};

