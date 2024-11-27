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

static const struct mtk_gate_regs mfg_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_MFG(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mfg_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MFG_V(_id, _name, _parent) {    \
		.id = _id,              \
		.name = _name,              \
		.parent_name = _parent,         \
	}

static const struct mtk_gate mfg_clks[] = {
	GATE_MFG(CLK_MFG_BG3D, "mfg_bg3d",
			"mfg_sel_mfgpll"/* parent */, 0),
};

static const struct mtk_clk_desc mfg_mcd = {
	.clks = mfg_clks,
	.num_clks = CLK_MFG_NR_CLK,
};

static const struct of_device_id of_match_clk_mt8189_mfg[] = {
	{
		.compatible = "mediatek,mt8189-mfgsys",
		.data = &mfg_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt8189_mfg_grp_probe(struct platform_device *pdev)
{
	int r;

	r = mtk_clk_simple_probe(pdev);
	if (r)
		dev_info(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

	return r;
}

static struct platform_driver clk_mt8189_mfg_drv = {
	.probe = clk_mt8189_mfg_grp_probe,
	.driver = {
		.name = "clk-mt8189-mfg",
		.of_match_table = of_match_clk_mt8189_mfg,
	},
};

module_platform_driver(clk_mt8189_mfg_drv);
MODULE_LICENSE("GPL");
