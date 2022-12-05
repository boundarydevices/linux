// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/clk-provider.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt8365-clk.h>

static const struct mtk_gate_regs mfg0_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs mfg1_cg_regs = {
	.set_ofs = 0x280,
	.clr_ofs = 0x280,
	.sta_ofs = 0x280,
};

#define GATE_MFG0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mfg0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MFG1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mfg1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

static const struct mtk_gate mfg_clks[] = {
	/* MFG0 */
	GATE_MFG0(CLK_MFG_BG3D, "mfg_bg3d", "mfg_sel", 0),
	/* MFG1 */
	GATE_MFG1(CLK_MFG_MBIST_DIAG, "mfg_mbist_diag", "mbist_diag_sel", 24),
};

static int clk_mt8365_mfg_probe(struct platform_device *pdev)
{
	struct clk_hw_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

	clk_data = mtk_alloc_clk_data(CLK_MFG_NR_CLK);

	mtk_clk_register_gates(node, mfg_clks, ARRAY_SIZE(mfg_clks), clk_data);

	r = of_clk_add_hw_provider(node, of_clk_hw_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

	return r;
}

static const struct of_device_id of_match_clk_mt8365_mfg[] = {
	{ .compatible = "mediatek,mt8365-mfgcfg", },
	{}
};

static struct platform_driver clk_mt8365_mfg_drv = {
	.probe = clk_mt8365_mfg_probe,
	.driver = {
		.name = "clk-mt8365-mfg",
		.of_match_table = of_match_clk_mt8365_mfg,
	},
};

builtin_platform_driver(clk_mt8365_mfg_drv);
