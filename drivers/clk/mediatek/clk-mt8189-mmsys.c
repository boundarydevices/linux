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

static const struct mtk_gate_regs mm0_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs mm1_cg_regs = {
	.set_ofs = 0x114,
	.clr_ofs = 0x118,
	.sta_ofs = 0x110,
};

#define GATE_MM0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
		.flags = CLK_OPS_PARENT_ENABLE,	\
	}

#define GATE_MM0_V(_id, _name, _parent) {    \
		.id = _id,              \
		.name = _name,              \
		.parent_name = _parent,         \
	}

#define GATE_MM1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
		.flags = CLK_OPS_PARENT_ENABLE,	\
	}

#define GATE_MM1_V(_id, _name, _parent) {    \
		.id = _id,              \
		.name = _name,              \
		.parent_name = _parent,         \
	}

static const struct mtk_gate mm_clks[] = {
	/* MM0 */
	GATE_MM0(CLK_MM_DISP_OVL0_4L, "mm_disp_ovl0_4l",
			"disp0_ck"/* parent */, 0),
	GATE_MM0(CLK_MM_DISP_OVL1_4L, "mm_disp_ovl1_4l",
			"disp0_ck"/* parent */, 1),
	GATE_MM0(CLK_MM_VPP_RSZ0, "mm_vpp_rsz0",
			"disp0_ck"/* parent */, 2),
	GATE_MM0(CLK_MM_VPP_RSZ1, "mm_vpp_rsz1",
			"disp0_ck"/* parent */, 3),
	GATE_MM0(CLK_MM_DISP_RDMA0, "mm_disp_rdma0",
			"disp0_ck"/* parent */, 4),
	GATE_MM0(CLK_MM_DISP_RDMA1, "mm_disp_rdma1",
			"disp0_ck"/* parent */, 5),
	GATE_MM0(CLK_MM_DISP_COLOR0, "mm_disp_color0",
			"disp0_ck"/* parent */, 6),
	GATE_MM0(CLK_MM_DISP_COLOR1, "mm_disp_color1",
			"disp0_ck"/* parent */, 7),
	GATE_MM0(CLK_MM_DISP_CCORR0, "mm_disp_ccorr0",
			"disp0_ck"/* parent */, 8),
	GATE_MM0(CLK_MM_DISP_CCORR1, "mm_disp_ccorr1",
			"disp0_ck"/* parent */, 9),
	GATE_MM0(CLK_MM_DISP_CCORR2, "mm_disp_ccorr2",
			"disp0_ck"/* parent */, 10),
	GATE_MM0(CLK_MM_DISP_CCORR3, "mm_disp_ccorr3",
			"disp0_ck"/* parent */, 11),
	GATE_MM0(CLK_MM_DISP_AAL0, "mm_disp_aal0",
			"disp0_ck"/* parent */, 12),
	GATE_MM0(CLK_MM_DISP_AAL1, "mm_disp_aal1",
			"disp0_ck"/* parent */, 13),
	GATE_MM0(CLK_MM_DISP_GAMMA0, "mm_disp_gamma0",
			"disp0_ck"/* parent */, 14),
	GATE_MM0(CLK_MM_DISP_GAMMA1, "mm_disp_gamma1",
			"disp0_ck"/* parent */, 15),
	GATE_MM0(CLK_MM_DISP_DITHER0, "mm_disp_dither0",
			"disp0_ck"/* parent */, 16),
	GATE_MM0(CLK_MM_DISP_DITHER1, "mm_disp_dither1",
			"disp0_ck"/* parent */, 17),
	GATE_MM0(CLK_MM_DISP_DSC_WRAP0, "mm_disp_dsc_wrap0",
			"disp0_ck"/* parent */, 18),
	GATE_MM0(CLK_MM_VPP_MERGE0, "mm_vpp_merge0",
			"disp0_ck"/* parent */, 19),
	GATE_MM0(CLK_MMSYS_0_DISP_DVO, "mmsys_0_disp_dvo",
			"disp0_ck"/* parent */, 20),
	GATE_MM0(CLK_MMSYS_0_DISP_DSI0, "mmsys_0_CLK0",
			"disp0_ck"/* parent */, 21),
	GATE_MM0(CLK_MM_DP_INTF0, "mm_dp_intf0",
			"disp0_ck"/* parent */, 22),
	GATE_MM0(CLK_MM_DPI0, "mm_dpi0",
			"disp0_ck"/* parent */, 23),
	GATE_MM0(CLK_MM_DISP_WDMA0, "mm_disp_wdma0",
			"disp0_ck"/* parent */, 24),
	GATE_MM0(CLK_MM_DISP_WDMA1, "mm_disp_wdma1",
			"disp0_ck"/* parent */, 25),
	GATE_MM0(CLK_MM_DISP_FAKE_ENG0, "mm_disp_fake_eng0",
			"disp0_ck"/* parent */, 26),
	GATE_MM0(CLK_MM_DISP_FAKE_ENG1, "mm_disp_fake_eng1",
			"disp0_ck"/* parent */, 27),
	GATE_MM0(CLK_MM_SMI_LARB, "mm_smi_larb",
			"disp0_ck"/* parent */, 28),
	GATE_MM0(CLK_MM_DISP_MUTEX0, "mm_disp_mutex0",
			"disp0_ck"/* parent */, 29),
	GATE_MM0(CLK_MM_DIPSYS_CONFIG, "mm_dipsys_config",
			"disp0_ck"/* parent */, 30),
	GATE_MM0(CLK_MM_DUMMY, "mm_dummy",
			"disp0_ck"/* parent */, 31),
	/* MM1 */
	GATE_MM1(CLK_MMSYS_1_DISP_DSI0, "mmsys_1_CLK0",
			"dsi_occ_ck"/* parent */, 0),
	GATE_MM1(CLK_MMSYS_1_LVDS_ENCODER, "mmsys_1_lvds_encoder",
			"pll_dpix_ck"/* parent */, 1),
	GATE_MM1(CLK_MMSYS_1_DPI0, "mmsys_1_dpi0",
			"pll_dpix_ck"/* parent */, 2),
	GATE_MM1(CLK_MMSYS_1_DISP_DVO, "mmsys_1_disp_dvo",
			"edp_ck"/* parent */, 3),
	GATE_MM1(CLK_MM_DP_INTF, "mm_dp_intf",
			"dp_ck"/* parent */, 4),
	GATE_MM1(CLK_MMSYS_1_LVDS_ENCODER_CTS, "mmsys_1_lvds_encoder_cts",
			"vdstx_dg_cts_ck"/* parent */, 5),
	GATE_MM1(CLK_MMSYS_1_DISP_DVO_AVT, "mmsys_1_disp_dvo_avt",
			"edp_favt_ck"/* parent */, 6),
};

static const struct mtk_clk_desc mm_mcd = {
	.clks = mm_clks,
	.num_clks = CLK_MM_NR_CLK,
};

static const struct mtk_gate_regs gce_d_cg_regs = {
	.set_ofs = 0xF0,
	.clr_ofs = 0xF0,
	.sta_ofs = 0xF0,
};

#define GATE_GCE_D(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &gce_d_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
		.flags = CLK_OPS_PARENT_ENABLE,	\
	}

#define GATE_GCE_D_V(_id, _name, _parent) {    \
		.id = _id,              \
		.name = _name,              \
		.parent_name = _parent,         \
	}

static const struct mtk_gate gce_d_clks[] = {
	GATE_GCE_D(CLK_GCE_D_TOP, "gce_d_top",
			"mminfra_gce_d"/* parent */, 16),
};

static const struct mtk_clk_desc gce_d_mcd = {
	.clks = gce_d_clks,
	.num_clks = CLK_GCE_D_NR_CLK,
};

static const struct mtk_gate_regs gce_m_cg_regs = {
	.set_ofs = 0xF0,
	.clr_ofs = 0xF0,
	.sta_ofs = 0xF0,
};

#define GATE_GCE_M(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &gce_m_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
		.flags = CLK_OPS_PARENT_ENABLE,		\
	}

#define GATE_GCE_M_V(_id, _name, _parent) {    \
		.id = _id,              \
		.name = _name,              \
		.parent_name = _parent,         \
	}

static const struct mtk_gate gce_m_clks[] = {
	GATE_GCE_M(CLK_GCE_M_TOP, "gce_m_top",
			"mminfra_gce_m"/* parent */, 16),
};

static const struct mtk_clk_desc gce_m_mcd = {
	.clks = gce_m_clks,
	.num_clks = CLK_GCE_M_NR_CLK,
};

static const struct mtk_gate_regs mminfra_config0_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs mminfra_config1_cg_regs = {
	.set_ofs = 0x114,
	.clr_ofs = 0x118,
	.sta_ofs = 0x110,
};

#define GATE_MMINFRA_CONFIG0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mminfra_config0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
		.flags = CLK_OPS_PARENT_ENABLE,		\
	}

#define GATE_MMINFRA_CONFIG0_V(_id, _name, _parent) {    \
		.id = _id,              \
		.name = _name,              \
		.parent_name = _parent,         \
	}

#define GATE_MMINFRA_CONFIG1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mminfra_config1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
		.flags = CLK_OPS_PARENT_ENABLE,		\
	}

#define GATE_MMINFRA_CONFIG1_V(_id, _name, _parent) {    \
		.id = _id,              \
		.name = _name,              \
		.parent_name = _parent,         \
	}

static const struct mtk_gate mminfra_config_clks[] = {
	/* MMINFRA_CONFIG0 */
	GATE_MMINFRA_CONFIG0(CLK_MMINFRA_GCE_D, "mminfra_gce_d",
			"mminfra_ck"/* parent */, 0),
	GATE_MMINFRA_CONFIG0(CLK_MMINFRA_GCE_M, "mminfra_gce_m",
			"mminfra_ck"/* parent */, 1),
	GATE_MMINFRA_CONFIG0(CLK_MMINFRA_SMI, "mminfra_smi",
			"mminfra_ck"/* parent */, 2),
	/* MMINFRA_CONFIG1 */
	GATE_MMINFRA_CONFIG1(CLK_MMINFRA_GCE_26M, "mminfra_gce_26m",
			"mminfra_ck"/* parent */, 17),
};

static const struct mtk_clk_desc mminfra_config_mcd = {
	.clks = mminfra_config_clks,
	.num_clks = CLK_MMINFRA_CONFIG_NR_CLK,
};

static const struct of_device_id of_match_clk_mt8189_mmsys[] = {
	{
		.compatible = "mediatek,mt8189-dispsys",
		.data = &mm_mcd,
	}, {
		.compatible = "mediatek,mt8189-gce_d",
		.data = &gce_d_mcd,
	}, {
		.compatible = "mediatek,mt8189-gce_m",
		.data = &gce_m_mcd,
	}, {
		.compatible = "mediatek,mt8189-mm_infra",
		.data = &mminfra_config_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt8189_mmsys_grp_probe(struct platform_device *pdev)
{
	int r;

	r = mtk_clk_simple_probe(pdev);
	if (r)
		dev_info(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

	return r;
}

static struct platform_driver clk_mt8189_mmsys_drv = {
	.probe = clk_mt8189_mmsys_grp_probe,
	.driver = {
		.name = "clk-mt8189-mmsys",
		.of_match_table = of_match_clk_mt8189_mmsys,
	},
};

module_platform_driver(clk_mt8189_mmsys_drv);
MODULE_LICENSE("GPL");
