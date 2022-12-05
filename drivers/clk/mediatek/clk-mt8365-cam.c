// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/clk-provider.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt8365-clk.h>

static const struct mtk_gate_regs cam_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAM(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate cam_clks[] = {
	GATE_CAM(CLK_CAM_LARB2, "cam_larb2", "mm_sel", 0),
	GATE_CAM(CLK_CAM, "cam", "mm_sel", 6),
	GATE_CAM(CLK_CAMTG, "camtg", "mm_sel", 7),
	GATE_CAM(CLK_CAM_SENIF, "cam_senif", "mm_sel", 8),
	GATE_CAM(CLK_CAMSV0, "camsv0", "mm_sel", 9),
	GATE_CAM(CLK_CAMSV1, "camsv1", "mm_sel", 10),
	GATE_CAM(CLK_CAM_FDVT, "cam_fdvt", "mm_sel", 11),
	GATE_CAM(CLK_CAM_WPE, "cam_wpe", "mm_sel", 12),
};

static int clk_mt8365_cam_probe(struct platform_device *pdev)
{
	struct clk_hw_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

	clk_data = mtk_alloc_clk_data(CLK_CAM_NR_CLK);

	mtk_clk_register_gates(node, cam_clks, ARRAY_SIZE(cam_clks), clk_data);

	r = of_clk_add_hw_provider(node, of_clk_hw_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

	return r;
}

static const struct of_device_id of_match_clk_mt8365_cam[] = {
	{ .compatible = "mediatek,mt8365-imgsys", },
	{}
};

static struct platform_driver clk_mt8365_cam_drv = {
	.probe = clk_mt8365_cam_probe,
	.driver = {
		.name = "clk-mt8365-cam",
		.of_match_table = of_match_clk_mt8365_cam,
	},
};

builtin_platform_driver(clk_mt8365_cam_drv);
