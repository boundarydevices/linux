/*
 * drivers/amlogic/clk/clkc.h
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

#include <linux/bitops.h>

#define CLK_PARENT_ALTERNATE BIT(5)
#define PMASK(width)			GENMASK(width - 1, 0)
#define SETPMASK(width, shift)		GENMASK(shift + width - 1, shift)
#define CLRPMASK(width, shift)		(~SETPMASK(width, shift))

#define PARM_GET(width, shift, reg)					\
	(((reg) & SETPMASK(width, shift)) >> (shift))
#define PARM_SET(width, shift, reg, val)				\
	(((reg) & CLRPMASK(width, shift)) | (val << (shift)))

#define MESON_PARM_APPLICABLE(p)		(!!((p)->width))

#define PNAME(x) \
static const char *x[]

struct parm {
	u16	reg_off;
	u8	shift;
	u8	width;
};

struct parm_fclk {
	u8	shift;
	u8	width;
	u32	mask;
};

struct pll_rate_table {
	unsigned long	rate;
	u16		m;
	u16		n;
	u16		od;
	u16		od2;
	u16		frac;
};

struct fclk_rate_table {
	unsigned long rate;
	u16 premux;
	u16 postmux;
	u16 mux_div;
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

#define FCLK_PLL_RATE(_r, _premux, _postmux, _mux_div)			\
	{								\
		.rate		= (_r),					\
		.premux		= (_premux),				\
		.postmux	= (_postmux),				\
		.mux_div	= (_mux_div),				\
	}

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
	struct clk_mux mux;
	const struct clk_ops *ops;

	struct clk *parent;
	void __iomem *base;
	struct notifier_block clk_nb;
	u16 reg_off;
	spinlock_t				*lock;
};

int meson_clk_cpu_notifier_cb(struct notifier_block *nb, unsigned long event,
		void *data);

struct meson_clk_mpll {
	struct clk_hw hw;
	void __iomem *base;
	struct parm sdm;
	struct parm n2;
	/* FIXME ssen gate control? */
	u8 sdm_en;
	u8 en_dds;
	u16 top_misc_reg; /*after txlx*/
	u16 top_misc_bit;
	u16 mpll_cntl0_reg;
	spinlock_t *lock;
};

#define MESON_GATE(_name, _reg, _bit)					\
struct clk_gate _name = {						\
	.reg = (void __iomem *) _reg,				\
	.bit_idx = (_bit),						\
	.lock = &clk_lock,						\
	.hw.init = &(struct clk_init_data) {		\
		.name = #_name,					\
		.ops = &clk_gate_ops,					\
		.parent_names = (const char *[]){ "clk81" },		\
		.num_parents = 1,					\
		.flags = (CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED),	\
	},								\
}


/*mux/div/gate macro*/
#define GATE(_name, _reg, _bit, _pname, _flags)			\
struct clk_gate _name = {				\
	.reg = (void __iomem *) _reg,		\
	.bit_idx = (_bit),					\
	.lock = &clk_lock,					\
	.hw.init = &(struct clk_init_data) {\
		.name = #_name,					\
		.ops = &clk_gate_ops,			\
		.parent_names = (const char *[]){ _pname },	\
		.num_parents = 1,			\
		.flags = (_flags),\
	},		\
}

#define DIV(_name, _reg, _shift, _width, _pname, _flags)	\
struct clk_divider _name = {			\
	.reg = (void __iomem *) _reg,		\
	.shift = _shift,					\
	.width = _width,					\
	.lock = &clk_lock,					\
	.hw.init = &(struct clk_init_data) {\
		.name = #_name,					\
		.ops = &clk_divider_ops,		\
		.parent_names = (const char *[]){ _pname },	\
		.num_parents = 1,			\
		.flags = (_flags),\
	},		\
}

#define MUX(_name, _reg, _mask, _shift, _pname, _flags)		\
struct clk_mux _name = {				\
	.reg = (void __iomem *) _reg,		\
	.mask = _mask,						\
	.shift = _shift,					\
	.hw.init = &(struct clk_init_data) {\
		.name = #_name,					\
		.ops = &clk_mux_ops,			\
		.parent_names =  _pname,		\
		.num_parents = ARRAY_SIZE(_pname),	\
		.flags = (_flags),\
	},		\
}

#define MESON_MUX(_name, _reg, _mask, _shift, _pname, _flags)\
struct clk_mux _name = {				\
	.reg = (void __iomem *) _reg,		\
	.mask = _mask,						\
	.shift = _shift,					\
	.flags = CLK_PARENT_ALTERNATE,		\
	.hw.init = &(struct clk_init_data) {\
		.name = #_name,					\
		.ops = &meson_clk_mux_ops,		\
		.parent_names =  _pname,		\
		.num_parents = ARRAY_SIZE(_pname),	\
		.flags = (_flags),\
	},		\
}

/*composite clock*/
struct meson_composite {
	unsigned int composite_id;
	const char		*name;
	const char * const *parent_names;
	int num_parents;
	struct clk_hw *mux_hw;
	struct clk_hw *rate_hw;
	struct clk_hw *gate_hw;
	unsigned long flags;
};

/*cpu mux and divider*/
struct meson_cpu_mux_divider {
	struct clk_hw hw;
	void __iomem	*reg;
	u32	*table;
	struct parm_fclk cpu_fclk_p00;
	struct parm_fclk cpu_fclk_p0;
	struct parm_fclk cpu_fclk_p10;
	struct parm_fclk cpu_fclk_p1;
	struct parm_fclk cpu_fclk_p;
	struct parm_fclk cpu_fclk_p01;
	struct parm_fclk cpu_fclk_p11;
	const struct fclk_rate_table *rate_table;
	unsigned int rate_count;
	spinlock_t *lock;
	unsigned long flags;
};

/*single clock,mux/div/gate*/
struct meson_hw {
	unsigned int hw_id;
	struct clk_hw *hw;
};

/* clk_ops */
extern const struct clk_ops meson_clk_pll_ro_ops;
extern const struct clk_ops meson_clk_pll_ops;
extern const struct clk_ops meson_clk_cpu_ops;
extern const struct clk_ops meson_fclk_cpu_ops;
extern const struct clk_ops meson_clk_mpll_ro_ops;
extern const struct clk_ops meson_clk_mpll_ops;
extern const struct clk_ops meson_clk_mux_ops;
extern const struct clk_ops meson_axg_pll_ro_ops;
extern const struct clk_ops meson_axg_pll_ops;
extern const struct clk_ops meson_g12a_pll_ro_ops;
extern const struct clk_ops meson_g12a_pll_ops;
extern const struct clk_ops meson_g12a_pcie_pll_ops;
extern const struct clk_ops meson_g12a_mpll_ro_ops;
extern const struct clk_ops meson_g12a_mpll_ops;

extern void meson_clk_register_composite(struct clk **soc_clks,
			struct meson_composite *composite,
			unsigned int length);
extern void meson_hw_clk_register(struct clk **soc_clks,
				struct meson_hw *m,
				unsigned int length);

extern spinlock_t clk_lock;
extern void __iomem *clk_base;
extern struct clk **clks;
void amlogic_init_sdemmc(void);
void amlogic_init_gpu(void);
void amlogic_init_media(void);
void amlogic_init_misc(void);
void axg_amlogic_init_sdemmc(void);
void axg_amlogic_init_media(void);
void axg_amlogic_init_misc(void);

/*txlx*/
void meson_txlx_sdemmc_init(void);
void meson_txlx_media_init(void);
void meson_init_gpu(void);

void meson_g12a_sdemmc_init(void);
void meson_g12a_media_init(void);
void meson_g12a_gpu_init(void);
void meson_g12a_misc_init(void);

#endif /* __CLKC_H */
