/*
 * drivers/amlogic/clk/m8b/clkc.h
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

#ifndef __CLKC_H
#define __CLKC_H

#include <linux/hrtimer.h>

#define CLK_PARENT_ALTERNATE BIT(5)
#define PMASK(width)			GENMASK(width - 1, 0)
#define SETPMASK(width, shift)		GENMASK(shift + width - 1, shift)
#define CLRPMASK(width, shift)		(~SETPMASK(width, shift))

#define PARM_GET(width, shift, reg)					\
	(((reg) & SETPMASK(width, shift)) >> (shift))
#define PARM_SET(width, shift, reg, val)				\
	(((reg) & CLRPMASK(width, shift)) | ((val) << (shift)))

#define MESON_PARM_APPLICABLE(p)		(!!((p)->width))

struct parm {
	u16	reg_off;
	u8	shift;
	u8	width;
};

struct pll_rate_table {
	u64		rate;
	u16		m;
	u16		n;
	u16		od;
	u16		od2;
	u16		frac;
};

#define PLL_RATE(_r, _m, _n, _od)					\
	{								\
		.rate		= (_r),					\
		.m		= (_m),					\
		.n		= (_n),					\
		.od		= (_od),				\
	}								\

#define PLL_FRAC_RATE(_r, _m, _n, _od, _od2, _frac)			\
	{								\
		.rate		= (_r),					\
		.m		= (_m),					\
		.n		= (_n),					\
		.od		= (_od),				\
		.od2		= (_od2),				\
		.frac		= (_frac),				\
	}								\

struct meson_clk_pll {
	struct clk_hw hw;
	void __iomem *base;
	struct parm m;
	struct parm n;
	struct parm frac;
	struct parm od;
	struct parm od2;
	const struct pll_rate_table *rate_table;
	unsigned int rate_count;
	spinlock_t *lock;
};

#define to_meson_clk_pll(_hw) container_of(_hw, struct meson_clk_pll, hw)

struct meson_clk_cpu {
	struct clk_hw hw;
	void __iomem *base;
	u16 reg_off;
	struct notifier_block clk_nb;
	const struct clk_div_table *div_table;
};

int meson_clk_cpu_notifier_cb(struct notifier_block *nb, unsigned long event,
		void *data);

struct meson_clk_mpll {
	struct clk_hw hw;
	void __iomem *base;
	struct parm sdm;
	struct parm sdm_en;
	struct parm n2;
	struct parm en;
	spinlock_t *lock;
};

#define MESON_GATE(_name, _reg, _bit)					\
struct clk_gate _name = { 						\
	.reg = (void __iomem *) _reg, 					\
	.bit_idx = (_bit), 						\
	.lock = &clk_lock,						\
	.hw.init = &(struct clk_init_data) { 				\
		.name = #_name,					\
		.ops = &clk_gate_ops,					\
		.parent_names = (const char *[]){ "clk81" },		\
		.num_parents = 1,					\
		.flags = (CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED), 	\
	},								\
};

/* clk_ops */
extern const struct clk_ops meson_clk_pll_ro_ops;
extern const struct clk_ops meson_clk_pll_ops;
extern const struct clk_ops meson_clk_cpu_ops;
extern const struct clk_ops meson_clk_mpll_ro_ops;
extern const struct clk_ops meson_clk_mpll_ops;
extern const struct clk_ops meson_clk_mux_ops;
extern spinlock_t clk_lock;
extern void __iomem *clk_base;
extern struct clk **clks;
extern struct hrtimer cpu_hrtimer;
void amlogic_init_store(void);
void amlogic_init_gpu(void);
void amlogic_init_media(void);
void amlogic_init_misc(void);
enum hrtimer_restart virtual_clock_work(struct hrtimer *hrtimer);
#endif /* __CLKC_H */
