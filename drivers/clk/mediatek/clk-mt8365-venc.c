// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/clk-provider.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt8365-clk.h>

static const struct mtk_gate_regs venc_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_VENC(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &venc_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

static const struct mtk_gate venc_clks[] = {
	/* VENC */
	GATE_VENC(CLK_VENC, "venc_fvenc_ck", "mm_sel", 4),
	GATE_VENC(CLK_VENC_JPGENC, "venc_jpgenc_ck", "mm_sel", 8),
};

static int clk_mt8365_venc_probe(struct platform_device *pdev)
{
	struct clk_hw_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

	clk_data = mtk_alloc_clk_data(CLK_VENC_NR_CLK);

	mtk_clk_register_gates(node, venc_clks, ARRAY_SIZE(venc_clks),
			       clk_data);

	r = of_clk_add_hw_provider(node, of_clk_hw_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

	return r;
}

static const struct of_device_id of_match_clk_mt8365_venc[] = {
	{ .compatible = "mediatek,mt8365-vencsys", },
	{}
};

static struct platform_driver clk_mt8365_venc_drv = {
	.probe = clk_mt8365_venc_probe,
	.driver = {
		.name = "clk-mt8365-venc",
		.of_match_table = of_match_clk_mt8365_venc,
	},
};

builtin_platform_driver(clk_mt8365_venc_drv);
