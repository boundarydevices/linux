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

static const struct mtk_gate_regs ufscfg_ao_reg_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0xC,
	.sta_ofs = 0x4,
};

#define GATE_UFSCFG_AO_REG(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ufscfg_ao_reg_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_UFSCFG_AO_REG_V(_id, _name, _parent) {    \
		.id = _id,              \
		.name = _name,              \
		.parent_name = _parent,         \
	}

static const struct mtk_gate ufscfg_ao_reg_clks[] = {
	GATE_UFSCFG_AO_REG(CLK_UFSCFG_AO_REG_UNIPRO_TX_SYM, "ufscfg_ao_unipro_tx_sym",
			"f26m_ck"/* parent */, 1),
	GATE_UFSCFG_AO_REG(CLK_UFSCFG_AO_REG_UNIPRO_RX_SYM0, "ufscfg_ao_unipro_rx_sym0",
			"f26m_ck"/* parent */, 2),
	GATE_UFSCFG_AO_REG(CLK_UFSCFG_AO_REG_UNIPRO_RX_SYM1, "ufscfg_ao_unipro_rx_sym1",
			"f26m_ck"/* parent */, 3),
	GATE_UFSCFG_AO_REG(CLK_UFSCFG_AO_REG_UNIPRO_SYS, "ufscfg_ao_unipro_sys",
			"ufs_ck"/* parent */, 4),
	GATE_UFSCFG_AO_REG(CLK_UFSCFG_AO_REG_U_SAP_CFG, "ufscfg_ao_u_sap_cfg",
			"f26m_ck"/* parent */, 5),
	GATE_UFSCFG_AO_REG(CLK_UFSCFG_AO_REG_U_PHY_TOP_AHB_S_BUS, "ufscfg_ao_u_phy_ahb_s_bus",
			"axi_u_ck"/* parent */, 6),
};

static const struct mtk_clk_desc ufscfg_ao_reg_mcd = {
	.clks = ufscfg_ao_reg_clks,
	.num_clks = CLK_UFSCFG_AO_REG_NR_CLK,
};

static const struct mtk_gate_regs ufscfg_pdn_reg_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0xC,
	.sta_ofs = 0x4,
};

#define GATE_UFSCFG_PDN_REG(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ufscfg_pdn_reg_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_UFSCFG_PDN_REG_V(_id, _name, _parent) {    \
		.id = _id,              \
		.name = _name,              \
		.parent_name = _parent,         \
	}

static const struct mtk_gate ufscfg_pdn_reg_clks[] = {
	GATE_UFSCFG_PDN_REG(CLK_UFSCFG_REG_UFSHCI_UFS, "ufscfg_ufshci_ufs",
			"ufs_ck"/* parent */, 0),
	GATE_UFSCFG_PDN_REG(CLK_UFSCFG_REG_UFSHCI_AES, "ufscfg_ufshci_aes",
			"aes_ufsfde_ck"/* parent */, 1),
	GATE_UFSCFG_PDN_REG(CLK_UFSCFG_REG_UFSHCI_U_AHB, "ufscfg_ufshci_u_ahb",
			"axi_u_ck"/* parent */, 3),
	GATE_UFSCFG_PDN_REG(CLK_UFSCFG_REG_UFSHCI_U_AXI, "ufscfg_ufshci_u_axi",
			"mem_sub_u_ck"/* parent */, 5),
};

static const struct mtk_clk_desc ufscfg_pdn_reg_mcd = {
	.clks = ufscfg_pdn_reg_clks,
	.num_clks = CLK_UFSCFG_PDN_REG_NR_CLK,
};

static const struct of_device_id of_match_clk_mt8189_ufs[] = {
	{
		.compatible = "mediatek,mt8189-ufscfg_ao",
		.data = &ufscfg_ao_reg_mcd,
	}, {
		.compatible = "mediatek,mt8189-ufscfg_pdn",
		.data = &ufscfg_pdn_reg_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt8189_ufs_grp_probe(struct platform_device *pdev)
{
	int r;

	r = mtk_clk_simple_probe(pdev);
	if (r)
		dev_info(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

	return r;
}

static struct platform_driver clk_mt8189_ufs_drv = {
	.probe = clk_mt8189_ufs_grp_probe,
	.driver = {
		.name = "clk-mt8189-ufs",
		.of_match_table = of_match_clk_mt8189_ufs,
	},
};

module_platform_driver(clk_mt8189_ufs_drv);
MODULE_LICENSE("GPL");
