// SPDX-License-Identifier: GPL-2.0-only
//
// Copyright (c) 2022 MediaTek Inc.
// Author: Garmin Chang <garmin.chang@mediatek.com>

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <dt-bindings/clock/mediatek,mt8188-clk.h>

#include "clk-gate.h"
#include "clk-mtk.h"

static const struct mtk_gate_regs ven1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_VEN1(_id, _name, _parent, _shift)			\
	GATE_MTK(_id, _name, _parent, &ven1_cg_regs, _shift, &mtk_clk_gate_ops_setclr_inv)

static const struct mtk_gate ven1_clks[] = {
	GATE_VEN1(CLK_VEN1_CKE0_LARB, "ven1_cke0_larb", "top_venc", 0),
	GATE_VEN1(CLK_VEN1_CKE1_VENC, "ven1_cke1_venc", "top_venc", 4),
	GATE_VEN1(CLK_VEN1_CKE2_JPGENC, "ven1_cke2_jpgenc", "top_venc", 8),
	GATE_VEN1(CLK_VEN1_CKE3_JPGDEC, "ven1_cke3_jpgdec", "top_venc", 12),
	GATE_VEN1(CLK_VEN1_CKE4_JPGDEC_C1, "ven1_cke4_jpgdec_c1", "top_venc", 16),
	GATE_VEN1(CLK_VEN1_CKE5_GALS, "ven1_cke5_gals", "top_venc", 28),
	GATE_VEN1(CLK_VEN1_CKE6_GALS_SRAM, "ven1_cke6_gals_sram", "top_venc", 31),
};

static const struct mtk_clk_desc ven1_desc = {
	.clks = ven1_clks,
	.num_clks = ARRAY_SIZE(ven1_clks),
};

static const struct of_device_id of_match_clk_mt8188_ven1[] = {
	{
		.compatible = "mediatek,mt8188-vencsys",
		.data = &ven1_desc,
	}, {
		/* sentinel */
	}
};

static struct platform_driver clk_mt8188_ven1_drv = {
	.probe = mtk_clk_simple_probe,
	.remove = mtk_clk_simple_remove,
	.driver = {
		.name = "clk-mt8188-ven1",
		.of_match_table = of_match_clk_mt8188_ven1,
	},
};

builtin_platform_driver(clk_mt8188_ven1_drv);
MODULE_LICENSE("GPL");
