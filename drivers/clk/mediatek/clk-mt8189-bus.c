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

static const struct mtk_gate_regs ifrao0_cg_regs = {
	.set_ofs = 0x80,
	.clr_ofs = 0x84,
	.sta_ofs = 0x90,
};

static const struct mtk_gate_regs ifrao1_cg_regs = {
	.set_ofs = 0x88,
	.clr_ofs = 0x8C,
	.sta_ofs = 0x94,
};

static const struct mtk_gate_regs ifrao2_cg_regs = {
	.set_ofs = 0xA4,
	.clr_ofs = 0xA8,
	.sta_ofs = 0xAC,
};

#define GATE_IFRAO0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ifrao0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IFRAO0_V(_id, _name, _parent) {    \
		.id = _id,              \
		.name = _name,              \
		.parent_name = _parent,         \
	}

#define GATE_IFRAO1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ifrao1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IFRAO1_V(_id, _name, _parent) {    \
		.id = _id,              \
		.name = _name,              \
		.parent_name = _parent,         \
	}

#define GATE_IFRAO2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ifrao2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IFRAO2_V(_id, _name, _parent) {    \
		.id = _id,              \
		.name = _name,              \
		.parent_name = _parent,         \
	}

static const struct mtk_gate ifrao_clks[] = {
	/* IFRAO0 */
	GATE_IFRAO0(CLK_IFRAO_CQ_DMA_FPC, "ifrao_dma",
			"f26m_ck"/* parent */, 28),
	/* IFRAO1 */
	GATE_IFRAO1(CLK_IFRAO_DEBUGSYS, "ifrao_debugsys",
			"axi_ck"/* parent */, 24),
	GATE_IFRAO1(CLK_IFRAO_DBG_TRACE, "ifrao_dbg_trace",
			"axi_ck"/* parent */, 29),
	/* IFRAO2 */
	GATE_IFRAO2(CLK_IFRAO_CQ_DMA, "ifrao_cq_dma",
			"axi_ck"/* parent */, 27),
};

static const struct mtk_clk_desc ifrao_mcd = {
	.clks = ifrao_clks,
	.num_clks = CLK_IFRAO_NR_CLK,
};

static const struct mtk_gate_regs perao0_cg_regs = {
	.set_ofs = 0x24,
	.clr_ofs = 0x28,
	.sta_ofs = 0x10,
};

static const struct mtk_gate_regs perao1_cg_regs = {
	.set_ofs = 0x2C,
	.clr_ofs = 0x30,
	.sta_ofs = 0x14,
};

static const struct mtk_gate_regs perao2_cg_regs = {
	.set_ofs = 0x34,
	.clr_ofs = 0x38,
	.sta_ofs = 0x18,
};

#define GATE_PERAO0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &perao0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_PERAO0_V(_id, _name, _parent) {    \
		.id = _id,              \
		.name = _name,              \
		.parent_name = _parent,         \
	}

#define GATE_PERAO1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &perao1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_PERAO1_V(_id, _name, _parent) {    \
		.id = _id,              \
		.name = _name,              \
		.parent_name = _parent,         \
	}

#define GATE_PERAO2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &perao2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_PERAO2_V(_id, _name, _parent) {    \
		.id = _id,              \
		.name = _name,              \
		.parent_name = _parent,         \
	}

static const struct mtk_gate perao_clks[] = {
	/* PERAO0 */
	GATE_PERAO0(CLK_PERAO_UART0, "perao_uart0",
			"uart_ck"/* parent */, 0),
	GATE_PERAO0(CLK_PERAO_UART1, "perao_uart1",
			"uart_ck"/* parent */, 1),
	GATE_PERAO0(CLK_PERAO_UART2, "perao_uart2",
			"uart_ck"/* parent */, 2),
	GATE_PERAO0(CLK_PERAO_UART3, "perao_uart3",
			"uart_ck"/* parent */, 3),
	GATE_PERAO0(CLK_PERAO_PWM_H, "perao_pwm_h",
			"axi_peri_ck"/* parent */, 4),
	GATE_PERAO0(CLK_PERAO_PWM_B, "perao_pwm_b",
			"pwm_ck"/* parent */, 5),
	GATE_PERAO0(CLK_PERAO_PWM_FB1, "perao_pwm_fb1",
			"pwm_ck"/* parent */, 6),
	GATE_PERAO0(CLK_PERAO_PWM_FB2, "perao_pwm_fb2",
			"pwm_ck"/* parent */, 7),
	GATE_PERAO0(CLK_PERAO_PWM_FB3, "perao_pwm_fb3",
			"pwm_ck"/* parent */, 8),
	GATE_PERAO0(CLK_PERAO_PWM_FB4, "perao_pwm_fb4",
			"pwm_ck"/* parent */, 9),
	GATE_PERAO0(CLK_PERAO_DISP_PWM0, "perao_disp_pwm0",
			"disp_pwm_ck"/* parent */, 10),
	GATE_PERAO0(CLK_PERAO_DISP_PWM1, "perao_disp_pwm1",
			"disp_pwm_ck"/* parent */, 11),
	GATE_PERAO0(CLK_PERAO_SPI0_B, "perao_spi0_b",
			"spi0_ck"/* parent */, 12),
	GATE_PERAO0(CLK_PERAO_SPI1_B, "perao_spi1_b",
			"spi1_ck"/* parent */, 13),
	GATE_PERAO0(CLK_PERAO_SPI2_B, "perao_spi2_b",
			"spi2_ck"/* parent */, 14),
	GATE_PERAO0(CLK_PERAO_SPI3_B, "perao_spi3_b",
			"spi3_ck"/* parent */, 15),
	GATE_PERAO0(CLK_PERAO_SPI4_B, "perao_spi4_b",
			"spi4_ck"/* parent */, 16),
	GATE_PERAO0(CLK_PERAO_SPI5_B, "perao_spi5_b",
			"spi5_ck"/* parent */, 17),
	GATE_PERAO0(CLK_PERAO_SPI0_H, "perao_spi0_h",
			"axi_peri_ck"/* parent */, 18),
	GATE_PERAO0(CLK_PERAO_SPI1_H, "perao_spi1_h",
			"axi_peri_ck"/* parent */, 19),
	GATE_PERAO0(CLK_PERAO_SPI2_H, "perao_spi2_h",
			"axi_peri_ck"/* parent */, 20),
	GATE_PERAO0(CLK_PERAO_SPI3_H, "perao_spi3_h",
			"axi_peri_ck"/* parent */, 21),
	GATE_PERAO0(CLK_PERAO_SPI4_H, "perao_spi4_h",
			"axi_peri_ck"/* parent */, 22),
	GATE_PERAO0(CLK_PERAO_SPI5_H, "perao_spi5_h",
			"axi_peri_ck"/* parent */, 23),
	GATE_PERAO0(CLK_PERAO_AXI, "perao_axi",
			"mem_sub_peri_ck"/* parent */, 24),
	GATE_PERAO0(CLK_PERAO_AHB_APB, "perao_ahb_apb",
			"axi_peri_ck"/* parent */, 25),
	GATE_PERAO0(CLK_PERAO_TL, "perao_tl",
			"pcie_mac_tl_ck"/* parent */, 26),
	GATE_PERAO0(CLK_PERAO_REF, "perao_ref",
			"f26m_ck"/* parent */, 27),
	GATE_PERAO0(CLK_PERAO_I2C, "perao_i2c",
			"axi_peri_ck"/* parent */, 28),
	GATE_PERAO0(CLK_PERAO_DMA_B, "perao_dma_b",
			"axi_peri_ck"/* parent */, 29),
	/* PERAO1 */
	GATE_PERAO1(CLK_PERAO_SSUSB0_REF, "perao_ssusb0_ref",
			"f26m_ck"/* parent */, 1),
	GATE_PERAO1(CLK_PERAO_SSUSB0_FRMCNT, "perao_ssusb0_frmcnt",
			"ssusb_frmcnt_p0"/* parent */, 2),
	GATE_PERAO1(CLK_PERAO_SSUSB0_SYS, "perao_ssusb0_sys",
			"usb_p0_ck"/* parent */, 4),
	GATE_PERAO1(CLK_PERAO_SSUSB0_XHCI, "perao_ssusb0_xhci",
			"ssusb_xhci_p0_ck"/* parent */, 5),
	GATE_PERAO1(CLK_PERAO_SSUSB0_F, "perao_ssusb0_f",
			"axi_peri_ck"/* parent */, 6),
	GATE_PERAO1(CLK_PERAO_SSUSB0_H, "perao_ssusb0_h",
			"axi_peri_ck"/* parent */, 7),
	GATE_PERAO1(CLK_PERAO_SSUSB1_REF, "perao_ssusb1_ref",
			"f26m_ck"/* parent */, 8),
	GATE_PERAO1(CLK_PERAO_SSUSB1_FRMCNT, "perao_ssusb1_frmcnt",
			"ssusb_frmcnt_p1"/* parent */, 9),
	GATE_PERAO1(CLK_PERAO_SSUSB1_SYS, "perao_ssusb1_sys",
			"usb_p1_ck"/* parent */, 11),
	GATE_PERAO1(CLK_PERAO_SSUSB1_XHCI, "perao_ssusb1_xhci",
			"ssusb_xhci_p1_ck"/* parent */, 12),
	GATE_PERAO1(CLK_PERAO_SSUSB1_F, "perao_ssusb1_f",
			"axi_peri_ck"/* parent */, 13),
	GATE_PERAO1(CLK_PERAO_SSUSB1_H, "perao_ssusb1_h",
			"axi_peri_ck"/* parent */, 14),
	GATE_PERAO1(CLK_PERAO_SSUSB2_REF, "perao_ssusb2_ref",
			"f26m_ck"/* parent */, 15),
	GATE_PERAO1(CLK_PERAO_SSUSB2_FRMCNT, "perao_ssusb2_frmcnt",
			"ssusb_frmcnt_p2"/* parent */, 16),
	GATE_PERAO1(CLK_PERAO_SSUSB2_SYS, "perao_ssusb2_sys",
			"usb_p2_ck"/* parent */, 18),
	GATE_PERAO1(CLK_PERAO_SSUSB2_XHCI, "perao_ssusb2_xhci",
			"ssusb_xhci_p2_ck"/* parent */, 19),
	GATE_PERAO1(CLK_PERAO_SSUSB2_F, "perao_ssusb2_f",
			"axi_peri_ck"/* parent */, 20),
	GATE_PERAO1(CLK_PERAO_SSUSB2_H, "perao_ssusb2_h",
			"axi_peri_ck"/* parent */, 21),
	GATE_PERAO1(CLK_PERAO_SSUSB3_REF, "perao_ssusb3_ref",
			"f26m_ck"/* parent */, 23),
	GATE_PERAO1(CLK_PERAO_SSUSB3_FRMCNT, "perao_ssusb3_frmcnt",
			"ssusb_frmcnt_p3"/* parent */, 24),
	GATE_PERAO1(CLK_PERAO_SSUSB3_SYS, "perao_ssusb3_sys",
			"usb_p3_ck"/* parent */, 26),
	GATE_PERAO1(CLK_PERAO_SSUSB3_XHCI, "perao_ssusb3_xhci",
			"ssusb_xhci_p3_ck"/* parent */, 27),
	GATE_PERAO1(CLK_PERAO_SSUSB3_F, "perao_ssusb3_f",
			"axi_peri_ck"/* parent */, 28),
	GATE_PERAO1(CLK_PERAO_SSUSB3_H, "perao_ssusb3_h",
			"axi_peri_ck"/* parent */, 29),
	/* PERAO2 */
	GATE_PERAO2(CLK_PERAO_SSUSB4_REF, "perao_ssusb4_ref",
			"f26m_ck"/* parent */, 0),
	GATE_PERAO2(CLK_PERAO_SSUSB4_FRMCNT, "perao_ssusb4_frmcnt",
			"ssusb_frmcnt_p4"/* parent */, 1),
	GATE_PERAO2(CLK_PERAO_SSUSB4_SYS, "perao_ssusb4_sys",
			"usb_p4_ck"/* parent */, 3),
	GATE_PERAO2(CLK_PERAO_SSUSB4_XHCI, "perao_ssusb4_xhci",
			"ssusb_xhci_p4_ck"/* parent */, 4),
	GATE_PERAO2(CLK_PERAO_SSUSB4_F, "perao_ssusb4_f",
			"axi_peri_ck"/* parent */, 5),
	GATE_PERAO2(CLK_PERAO_SSUSB4_H, "perao_ssusb4_h",
			"axi_peri_ck"/* parent */, 6),
	GATE_PERAO2(CLK_PERAO_MSDC0, "perao_msdc0",
			"msdc50_0_ck"/* parent */, 7),
	GATE_PERAO2(CLK_PERAO_MSDC0_H, "perao_msdc0_h",
			"msdc5hclk_ck"/* parent */, 8),
	GATE_PERAO2(CLK_PERAO_MSDC0_FAES, "perao_msdc0_faes",
			"aes_msdcfde_ck"/* parent */, 9),
	GATE_PERAO2(CLK_PERAO_MSDC0_MST_F, "perao_msdc0_mst_f",
			"axi_peri_ck"/* parent */, 10),
	GATE_PERAO2(CLK_PERAO_MSDC0_SLV_H, "perao_msdc0_slv_h",
			"axi_peri_ck"/* parent */, 11),
	GATE_PERAO2(CLK_PERAO_MSDC1, "perao_msdc1",
			"msdc30_1_ck"/* parent */, 12),
	GATE_PERAO2(CLK_PERAO_MSDC1_H, "perao_msdc1_h",
			"msdc30_1_h_ck"/* parent */, 13),
	GATE_PERAO2(CLK_PERAO_MSDC1_MST_F, "perao_msdc1_mst_f",
			"axi_peri_ck"/* parent */, 14),
	GATE_PERAO2(CLK_PERAO_MSDC1_SLV_H, "perao_msdc1_slv_h",
			"axi_peri_ck"/* parent */, 15),
	GATE_PERAO2(CLK_PERAO_MSDC2, "perao_msdc2",
			"msdc30_2_ck"/* parent */, 16),
	GATE_PERAO2(CLK_PERAO_MSDC2_H, "perao_msdc2_h",
			"msdc30_2_h_ck"/* parent */, 17),
	GATE_PERAO2(CLK_PERAO_MSDC2_MST_F, "perao_msdc2_mst_f",
			"axi_peri_ck"/* parent */, 18),
	GATE_PERAO2(CLK_PERAO_MSDC2_SLV_H, "perao_msdc2_slv_h",
			"axi_peri_ck"/* parent */, 19),
	GATE_PERAO2(CLK_PERAO_SFLASH, "perao_sflash",
			"sflash_ck"/* parent */, 20),
	GATE_PERAO2(CLK_PERAO_SFLASH_F, "perao_sflash_f",
			"axi_peri_ck"/* parent */, 21),
	GATE_PERAO2(CLK_PERAO_SFLASH_H, "perao_sflash_h",
			"axi_peri_ck"/* parent */, 22),
	GATE_PERAO2(CLK_PERAO_SFLASH_P, "perao_sflash_p",
			"axi_peri_ck"/* parent */, 23),
	GATE_PERAO2(CLK_PERAO_AUDIO0, "perao_audio0",
			"axi_peri_ck"/* parent */, 24),
	GATE_PERAO2(CLK_PERAO_AUDIO1, "perao_audio1",
			"axi_peri_ck"/* parent */, 25),
	GATE_PERAO2(CLK_PERAO_AUDIO2, "perao_audio2",
			"aud_intbus_ck"/* parent */, 26),
	GATE_PERAO2(CLK_PERAO_AUXADC_26M, "perao_auxadc_26m",
			"f26m_ck"/* parent */, 27),
};

static const struct mtk_clk_desc perao_mcd = {
	.clks = perao_clks,
	.num_clks = CLK_PERAO_NR_CLK,
};

static const struct of_device_id of_match_clk_mt8189_bus[] = {
	{
		.compatible = "mediatek,mt8189-infra_ao",
		.data = &ifrao_mcd,
	}, {
		.compatible = "mediatek,mt8189-peri_ao",
		.data = &perao_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt8189_bus_grp_probe(struct platform_device *pdev)
{
	int r;

	r = mtk_clk_simple_probe(pdev);
	if (r)
		dev_info(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

	return r;
}

static struct platform_driver clk_mt8189_bus_drv = {
	.probe = clk_mt8189_bus_grp_probe,
	.driver = {
		.name = "clk-mt8189-bus",
		.of_match_table = of_match_clk_mt8189_bus,
	},
};

module_platform_driver(clk_mt8189_bus_drv);
MODULE_LICENSE("GPL");
