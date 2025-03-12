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

static const struct mtk_gate_regs imgsys1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_IMGSYS1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &imgsys1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
		.flags = CLK_OPS_PARENT_ENABLE,		\
	}

#define GATE_IMGSYS1_V(_id, _name, _parent) {    \
		.id = _id,              \
		.name = _name,              \
		.parent_name = _parent,         \
	}

static const struct mtk_gate imgsys1_clks[] = {
	GATE_IMGSYS1(CLK_IMGSYS1_LARB9, "imgsys1_larb9",
			"img1_ck"/* parent */, 0),
	GATE_IMGSYS1(CLK_IMGSYS1_LARB11, "imgsys1_larb11",
			"img1_ck"/* parent */, 1),
	GATE_IMGSYS1(CLK_IMGSYS1_DIP, "imgsys1_dip",
			"img1_ck"/* parent */, 2),
	GATE_IMGSYS1(CLK_IMGSYS1_GALS, "imgsys1_gals",
			"img1_ck"/* parent */, 12),
};

static const struct mtk_clk_desc imgsys1_mcd = {
	.clks = imgsys1_clks,
	.num_clks = CLK_IMGSYS1_NR_CLK,
};

static const struct mtk_gate_regs imgsys2_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_IMGSYS2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &imgsys2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
		.flags = CLK_OPS_PARENT_ENABLE,		\
	}

#define GATE_IMGSYS2_V(_id, _name, _parent) {    \
		.id = _id,              \
		.name = _name,              \
		.parent_name = _parent,         \
	}

static const struct mtk_gate imgsys2_clks[] = {
	GATE_IMGSYS2(CLK_IMGSYS2_LARB9, "imgsys2_larb9",
			"img1_ck"/* parent */, 0),
	GATE_IMGSYS2(CLK_IMGSYS2_LARB11, "imgsys2_larb11",
			"img1_ck"/* parent */, 1),
	GATE_IMGSYS2(CLK_IMGSYS2_MFB, "imgsys2_mfb",
			"img1_ck"/* parent */, 6),
	GATE_IMGSYS2(CLK_IMGSYS2_WPE, "imgsys2_wpe",
			"img1_ck"/* parent */, 7),
	GATE_IMGSYS2(CLK_IMGSYS2_MSS, "imgsys2_mss",
			"img1_ck"/* parent */, 8),
	GATE_IMGSYS2(CLK_IMGSYS2_GALS, "imgsys2_gals",
			"img1_ck"/* parent */, 12),
};

static const struct mtk_clk_desc imgsys2_mcd = {
	.clks = imgsys2_clks,
	.num_clks = CLK_IMGSYS2_NR_CLK,
};

static const struct mtk_gate_regs ipe_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_IPE(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ipe_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
		.flags = CLK_OPS_PARENT_ENABLE,		\
	}

#define GATE_IPE_V(_id, _name, _parent) {    \
		.id = _id,              \
		.name = _name,              \
		.parent_name = _parent,         \
	}

static const struct mtk_gate ipe_clks[] = {
	GATE_IPE(CLK_IPE_LARB19, "ipe_larb19",
			"ipe_ck"/* parent */, 0),
	GATE_IPE(CLK_IPE_LARB20, "ipe_larb20",
			"ipe_ck"/* parent */, 1),
	GATE_IPE(CLK_IPE_SMI_SUBCOM, "ipe_smi_subcom",
			"ipe_ck"/* parent */, 2),
	GATE_IPE(CLK_IPE_FD, "ipe_fd",
			"ipe_ck"/* parent */, 3),
	GATE_IPE(CLK_IPE_FE, "ipe_fe",
			"ipe_ck"/* parent */, 4),
	GATE_IPE(CLK_IPE_RSC, "ipe_rsc",
			"ipe_ck"/* parent */, 5),
	GATE_IPE(CLK_IPESYS_GALS, "ipesys_gals",
			"ipe_ck"/* parent */, 8),
};

static const struct mtk_clk_desc ipe_mcd = {
	.clks = ipe_clks,
	.num_clks = CLK_IPE_NR_CLK,
};

static const struct of_device_id of_match_clk_mt8189_img[] = {
	{
		.compatible = "mediatek,mt8189-imgsys1",
		.data = &imgsys1_mcd,
	}, {
		.compatible = "mediatek,mt8189-imgsys2",
		.data = &imgsys2_mcd,
	}, {
		.compatible = "mediatek,mt8189-ipesys",
		.data = &ipe_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt8189_img_grp_probe(struct platform_device *pdev)
{
	int r;

	r = mtk_clk_simple_probe(pdev);
	if (r)
		dev_info(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

	return r;
}

static struct platform_driver clk_mt8189_img_drv = {
	.probe = clk_mt8189_img_grp_probe,
	.driver = {
		.name = "clk-mt8189-img",
		.of_match_table = of_match_clk_mt8189_img,
	},
};

module_platform_driver(clk_mt8189_img_drv);
MODULE_LICENSE("GPL");
