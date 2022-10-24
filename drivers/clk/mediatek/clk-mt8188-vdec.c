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

static const struct mtk_gate_regs vde0_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x4,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs vde1_cg_regs = {
	.set_ofs = 0x200,
	.clr_ofs = 0x204,
	.sta_ofs = 0x200,
};

static const struct mtk_gate_regs vde2_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0xc,
	.sta_ofs = 0x8,
};

#define GATE_VDE0(_id, _name, _parent, _shift)			\
	GATE_MTK(_id, _name, _parent, &vde0_cg_regs, _shift, &mtk_clk_gate_ops_setclr_inv)

#define GATE_VDE1(_id, _name, _parent, _shift)			\
	GATE_MTK(_id, _name, _parent, &vde1_cg_regs, _shift, &mtk_clk_gate_ops_setclr_inv)

#define GATE_VDE2(_id, _name, _parent, _shift)			\
	GATE_MTK(_id, _name, _parent, &vde2_cg_regs, _shift, &mtk_clk_gate_ops_setclr_inv)

static const struct mtk_gate vde1_clks[] = {
	/* VDE1_0 */
	GATE_VDE0(CLK_VDE1_SOC_VDEC, "vde1_soc_vdec", "top_vdec", 0),
	GATE_VDE0(CLK_VDE1_SOC_VDEC_ACTIVE, "vde1_soc_vdec_active", "top_vdec", 4),
	GATE_VDE0(CLK_VDE1_SOC_VDEC_ENG, "vde1_soc_vdec_eng", "top_vdec", 8),
	/* VDE1_1 */
	GATE_VDE1(CLK_VDE1_SOC_LAT, "vde1_soc_lat", "top_vdec", 0),
	GATE_VDE1(CLK_VDE1_SOC_LAT_ACTIVE, "vde1_soc_lat_active", "top_vdec", 4),
	GATE_VDE1(CLK_VDE1_SOC_LAT_ENG, "vde1_soc_lat_eng", "top_vdec", 8),
	/* VDE12 */
	GATE_VDE2(CLK_VDE1_SOC_LARB1, "vde1_soc_larb1", "top_vdec", 0),
};

static const struct mtk_gate vde2_clks[] = {
	/* VDE2_0 */
	GATE_VDE0(CLK_VDE2_VDEC, "vde2_vdec", "top_vdec", 0),
	GATE_VDE0(CLK_VDE2_VDEC_ACTIVE, "vde2_vdec_active", "top_vdec", 4),
	GATE_VDE0(CLK_VDE2_VDEC_ENG, "vde2_vdec_eng", "top_vdec", 8),
	/* VDE2_1 */
	GATE_VDE1(CLK_VDE2_LAT, "vde2_lat", "top_vdec", 0),
	/* VDE2_2 */
	GATE_VDE2(CLK_VDE2_LARB1, "vde2_larb1", "top_vdec", 0),
};

static const struct mtk_clk_desc vde1_desc = {
	.clks = vde1_clks,
	.num_clks = ARRAY_SIZE(vde1_clks),
};

static const struct mtk_clk_desc vde2_desc = {
	.clks = vde2_clks,
	.num_clks = ARRAY_SIZE(vde2_clks),
};

static const struct of_device_id of_match_clk_mt8188_vde[] = {
	{
		.compatible = "mediatek,mt8188-vdecsys_soc",
		.data = &vde1_desc,
	}, {
		.compatible = "mediatek,mt8188-vdecsys",
		.data = &vde2_desc,
	}, {
		/* sentinel */
	}
};

static struct platform_driver clk_mt8188_vde_drv = {
	.probe = mtk_clk_simple_probe,
	.remove = mtk_clk_simple_remove,
	.driver = {
		.name = "clk-mt8188-vde",
		.of_match_table = of_match_clk_mt8188_vde,
	},
};

builtin_platform_driver(clk_mt8188_vde_drv);
MODULE_LICENSE("GPL");
