// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Qiqi Wang <qiqi.wang@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include <dt-bindings/clock/mediatek,mt8189-clk.h>

#include "clk-mtk.h"
#include "clk-gate.h"

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs impe_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

#define GATE_IMPE(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &impe_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IMPE_V(_id, _name, _parent) {    \
		.id = _id,              \
		.name = _name,              \
		.parent_name = _parent,         \
	}

static const struct mtk_gate impe_clks[] = {
	GATE_IMPE(CLK_IMPE_I2C0, "impe_i2c0",
			"i2c_ck"/* parent */, 0),
	GATE_IMPE(CLK_IMPE_I2C1, "impe_i2c1",
			"i2c_ck"/* parent */, 1),
};

static const struct mtk_clk_desc impe_mcd = {
	.clks = impe_clks,
	.num_clks = CLK_IMPE_NR_CLK,
};

static const struct mtk_gate_regs impen_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

#define GATE_IMPEN(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &impen_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IMPEN_V(_id, _name, _parent) {    \
		.id = _id,              \
		.name = _name,              \
		.parent_name = _parent,         \
	}

static const struct mtk_gate impen_clks[] = {
	GATE_IMPEN(CLK_IMPEN_I2C7, "impen_i2c7",
			"i2c_ck"/* parent */, 0),
	GATE_IMPEN(CLK_IMPEN_I2C8, "impen_i2c8",
			"i2c_ck"/* parent */, 1),
};

static const struct mtk_clk_desc impen_mcd = {
	.clks = impen_clks,
	.num_clks = CLK_IMPEN_NR_CLK,
};

static const struct mtk_gate_regs imps_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

#define GATE_IMPS(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &imps_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IMPS_V(_id, _name, _parent) {    \
		.id = _id,              \
		.name = _name,              \
		.parent_name = _parent,         \
	}

static const struct mtk_gate imps_clks[] = {
	GATE_IMPS(CLK_IMPS_I2C3, "imps_i2c3",
			"i2c_ck"/* parent */, 0),
	GATE_IMPS(CLK_IMPS_I2C4, "imps_i2c4",
			"i2c_ck"/* parent */, 1),
	GATE_IMPS(CLK_IMPS_I2C5, "imps_i2c5",
			"i2c_ck"/* parent */, 2),
	GATE_IMPS(CLK_IMPS_I2C6, "imps_i2c6",
			"i2c_ck"/* parent */, 3),
};

static const struct mtk_clk_desc imps_mcd = {
	.clks = imps_clks,
	.num_clks = CLK_IMPS_NR_CLK,
};

static const struct mtk_gate_regs impws_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

#define GATE_IMPWS(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &impws_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IMPWS_V(_id, _name, _parent) {    \
		.id = _id,              \
		.name = _name,              \
		.parent_name = _parent,         \
	}

static const struct mtk_gate impws_clks[] = {
	GATE_IMPWS(CLK_IMPWS_I2C2, "impws_i2c2",
			"i2c_ck"/* parent */, 0),
};

static const struct mtk_clk_desc impws_mcd = {
	.clks = impws_clks,
	.num_clks = CLK_IMPWS_NR_CLK,
};

static const struct of_device_id of_match_clk_mt8189_iic[] = {
	{
		.compatible = "mediatek,mt8189-iic_wrap_e",
		.data = &impe_mcd,
	}, {
		.compatible = "mediatek,mt8189-iic_wrap_en",
		.data = &impen_mcd,
	}, {
		.compatible = "mediatek,mt8189-iic_wrap_s",
		.data = &imps_mcd,
	}, {
		.compatible = "mediatek,mt8189-iic_wrap_ws",
		.data = &impws_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt8189_iic_grp_probe(struct platform_device *pdev)
{
	int r;

	r = mtk_clk_simple_probe(pdev);
	if (r)
		dev_info(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

	return r;
}

static struct platform_driver clk_mt8189_iic_drv = {
	.probe = clk_mt8189_iic_grp_probe,
	.driver = {
		.name = "clk-mt8189-iic",
		.of_match_table = of_match_clk_mt8189_iic,
	},
};

module_platform_driver(clk_mt8189_iic_drv);
MODULE_LICENSE("GPL");
