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

static const struct mtk_gate_regs vdec_core0_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x4,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs vdec_core1_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0xC,
	.sta_ofs = 0x8,
};

#define GATE_VDEC_CORE0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vdec_core0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv_counted,\
	}

#define GATE_VDEC_CORE0_V(_id, _name, _parent) {    \
		.id = _id,              \
		.name = _name,              \
		.parent_name = _parent,         \
	}

#define GATE_VDEC_CORE1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vdec_core1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv_counted,\
	}

#define GATE_VDEC_CORE1_V(_id, _name, _parent) {    \
		.id = _id,              \
		.name = _name,              \
		.parent_name = _parent,         \
	}

static const struct mtk_gate vdec_core_clks[] = {
	/* VDEC_CORE0 */
	GATE_VDEC_CORE0(CLK_VDEC_CORE_VDEC_CKEN, "vdec_core_vdec_cken",
			"vdec_ck"/* parent */, 0),
	GATE_VDEC_CORE0(CLK_VDEC_CORE_VDEC_ACTIVE, "vdec_core_vdec_active",
			"vdec_ck"/* parent */, 4),
	/* VDEC_CORE1 */
	GATE_VDEC_CORE1(CLK_VDEC_CORE_LARB_CKEN, "vdec_core_larb_cken",
			"vdec_ck"/* parent */, 0),
};

static const struct mtk_clk_desc vdec_core_mcd = {
	.clks = vdec_core_clks,
	.num_clks = CLK_VDEC_CORE_NR_CLK,
};

static const struct mtk_gate_regs ven1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_VEN1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ven1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv_counted,\
	}

#define GATE_VEN1_V(_id, _name, _parent) {    \
		.id = _id,              \
		.name = _name,              \
		.parent_name = _parent,         \
	}

static const struct mtk_gate ven1_clks[] = {
	GATE_VEN1(CLK_VEN1_CKE0_LARB, "ven1_larb",
			"venc_ck"/* parent */, 0),
	GATE_VEN1(CLK_VEN1_CKE1_VENC, "ven1_venc",
			"venc_ck"/* parent */, 4),
	GATE_VEN1(CLK_VEN1_CKE2_JPGENC, "ven1_jpgenc",
			"venc_ck"/* parent */, 8),
	GATE_VEN1(CLK_VEN1_CKE3_JPGDEC, "ven1_jpgdec",
			"venc_ck"/* parent */, 12),
	GATE_VEN1(CLK_VEN1_CKE4_JPGDEC_C1, "ven1_jpgdec_c1",
			"venc_ck"/* parent */, 16),
	GATE_VEN1(CLK_VEN1_CKE5_GALS, "ven1_gals",
			"venc_ck"/* parent */, 28),
	GATE_VEN1(CLK_VEN1_CKE6_GALS_SRAM, "ven1_gals_sram",
			"venc_ck"/* parent */, 31),
};

static const struct mtk_clk_desc ven1_mcd = {
	.clks = ven1_clks,
	.num_clks = CLK_VEN1_NR_CLK,
};

static const struct of_device_id of_match_clk_mt8189_vcodec[] = {
	{
		.compatible = "mediatek,mt8189-vdec_core",
		.data = &vdec_core_mcd,
	}, {
		.compatible = "mediatek,mt8189-venc",
		.data = &ven1_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt8189_vcodec_grp_probe(struct platform_device *pdev)
{
	int r;

	r = mtk_clk_simple_probe(pdev);
	if (r)
		dev_info(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

	return r;
}

static struct platform_driver clk_mt8189_vcodec_drv = {
	.probe = clk_mt8189_vcodec_grp_probe,
	.driver = {
		.name = "clk-mt8189-vcodec",
		.of_match_table = of_match_clk_mt8189_vcodec,
	},
};

module_platform_driver(clk_mt8189_vcodec_drv);
MODULE_LICENSE("GPL");
