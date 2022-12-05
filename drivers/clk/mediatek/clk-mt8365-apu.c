// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/clk-provider.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt8365-clk.h>

static const struct mtk_gate_regs apu_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_APU(_id, _name, _parent, _shift) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
		.regs = &apu_cg_regs,		\
		.shift = _shift,		\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate apu_clks[] = {
	GATE_APU(CLK_APU_AHB, "apu_ahb", "ifr_apu_axi", 5),
	GATE_APU(CLK_APU_EDMA, "apu_edma", "apu_sel", 4),
	GATE_APU(CLK_APU_IF_CK, "apu_if_ck", "apu_if_sel", 3),
	GATE_APU(CLK_APU_JTAG, "apu_jtag", "clk26m_ck", 2),
	GATE_APU(CLK_APU_AXI, "apu_axi", "apu_sel", 1),
	GATE_APU(CLK_APU_IPU_CK, "apu_ck", "apu_sel", 0),
};

static int clk_mt8365_apu_probe(struct platform_device *pdev)
{
	struct clk_hw_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

	clk_data = mtk_alloc_clk_data(CLK_APU_NR_CLK);

	mtk_clk_register_gates(node, apu_clks, ARRAY_SIZE(apu_clks),
			clk_data);

	r = of_clk_add_hw_provider(node, of_clk_hw_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

	return r;
}

static const struct of_device_id of_match_clk_mt8365_apu[] = {
	{ .compatible = "mediatek,mt8365-apu", },
	{}
};

static struct platform_driver clk_mt8365_apu_drv = {
	.probe = clk_mt8365_apu_probe,
	.driver = {
		.name = "clk-mt8365-apu",
		.of_match_table = of_match_clk_mt8365_apu,
	},
};

builtin_platform_driver(clk_mt8365_apu_drv);
