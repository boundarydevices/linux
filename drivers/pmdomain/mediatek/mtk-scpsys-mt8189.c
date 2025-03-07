// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Qiqi Wang <qiqi.wang@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/pm_opp.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>

#include <dt-bindings/power/mediatek,mt8189-power.h>

#include "mtk-scpsys.h"

#define SCPSYS_BRINGUP			(0)
#if SCPSYS_BRINGUP
#define default_cap			(MTK_SCPD_BYPASS_OFF)
#else
#define default_cap			(0)
#endif

#define MT8189_TOP_AXI_PROT_EN_MCU_STA_0_CONN	(BIT(1))
#define MT8189_TOP_AXI_PROT_EN_INFRASYS_STA_1_CONN	(BIT(12))
#define MT8189_TOP_AXI_PROT_EN_MCU_STA_0_CONN_2ND	(BIT(0))
#define MT8189_TOP_AXI_PROT_EN_INFRASYS_STA_0_CONN	(BIT(8))
#define MT8189_TOP_AXI_PROT_EN_PERISYS_STA_0_AUDIO	(BIT(6))
#define MT8189_TOP_AXI_PROT_EN_MMSYS_STA_0_ISP_IMG1	(BIT(3))
#define MT8189_TOP_AXI_PROT_EN_MMSYS_STA_1_ISP_IMG1	(BIT(7))
#define MT8189_TOP_AXI_PROT_EN_MMSYS_STA_0_ISP_IPE	(BIT(2))
#define MT8189_TOP_AXI_PROT_EN_MMSYS_STA_1_ISP_IPE	(BIT(8))
#define MT8189_TOP_AXI_PROT_EN_MMSYS_STA_0_VDE0	(BIT(20))
#define MT8189_TOP_AXI_PROT_EN_MMSYS_STA_1_VDE0	(BIT(13))
#define MT8189_TOP_AXI_PROT_EN_MMSYS_STA_0_VEN0	(BIT(12))
#define MT8189_TOP_AXI_PROT_EN_MMSYS_STA_1_VEN0	(BIT(12))
#define MT8189_TOP_AXI_PROT_EN_MMSYS_STA_0_CAM_MAIN	(BIT(30) | BIT(31))
#define MT8189_TOP_AXI_PROT_EN_MMSYS_STA_1_CAM_MAIN	(BIT(9) | BIT(10))
#define MT8189_TOP_AXI_PROT_EN_MMSYS_STA_0_MDP0	(BIT(18))
#define MT8189_TOP_AXI_PROT_EN_MMSYS_STA_0_DISP	(BIT(0) | BIT(1))
#define MT8189_TOP_AXI_PROT_EN_MMSYS_STA_1_MM_INFRA	(BIT(2) |  \
			BIT(3))
#define MT8189_TOP_AXI_PROT_EN_INFRASYS_STA_1_MM_INFRA	(BIT(11))
#define MT8189_TOP_AXI_PROT_EN_MMSYS_STA_1_MM_INFRA_2ND	(BIT(7) |  \
			BIT(8) | BIT(9) |  \
			BIT(10) | BIT(11) |  \
			BIT(12) | BIT(13) |  \
			BIT(14) | BIT(15))
#define MT8189_TOP_AXI_PROT_EN_MMSYS_STA_1_MM_INFRA_CLK_IGN	(BIT(1))
#define MT8189_TOP_AXI_PROT_EN_MMSYS_STA_1_MM_INFRA_2ND_CLK_IGN	(BIT(0))
#define MT8189_TOP_AXI_PROT_EN_INFRASYS_STA_0_MM_INFRA	(BIT(16))
#define MT8189_TOP_AXI_PROT_EN_EMISYS_STA_0_MM_INFRA	(BIT(20) | BIT(21))
#define MT8189_AXI_PROT_EN_SCP_CORE	(BIT(3) | BIT(8) |  \
			BIT(0) | BIT(1))
#define MT8189_VLP_AXI_PROT_EN_SCP_CORE	(BIT(12) | BIT(19))
#define MT8189_AXI_PROT_EN_SCP_CORE_2ND	(BIT(3) | BIT(8) |  \
			BIT(4) | BIT(9) |  \
			BIT(0) | BIT(1))
#define MT8189_VLP_AXI_PROT_EN_SCP_CORE_2ND	(BIT(11) | BIT(18))
#define MT8189_TOP_AXI_PROT_EN_PERISYS_STA_0_SSUSB	(BIT(7))
#define MT8189_TOP_AXI_PROT_EN_INFRASYS_STA_1_MFG1	(BIT(20))
#define MT8189_TOP_AXI_PROT_EN_MD_STA_0_MFG1	(BIT(0) | BIT(2))
#define MT8189_TOP_AXI_PROT_EN_MD_STA_0_MFG1_2ND	(BIT(4))
#define MT8189_AXI_PROT_EN_MFG1	(BIT(4) | BIT(5))

enum regmap_type {
	INVALID_TYPE = 0,
	IFR_TYPE = 1,
	VLP_TYPE = 2,
	VLPCFG_REG_TYPE = 3,
	EMICFG_AO_MEM_TYPE = 4,
	BUS_TYPE_NUM,
};

static const char *bus_list[BUS_TYPE_NUM] = {
	[IFR_TYPE] = "infra-infracfg-ao-reg-bus",
	[VLP_TYPE] = "vlpcfg-reg-bus",
	[VLPCFG_REG_TYPE] = "vlpcfg-reg-bus",
	[EMICFG_AO_MEM_TYPE] = "emicfg-ao-mem",
};

/*
 * MT8189 power domain support
 */

static const struct scp_domain_data scp_domain_mt8189_spm_data[] = {
	[MT8189_POWER_DOMAIN_CONN] = {
		.name = "conn",
		.ctl_offs = 0xE04,
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C94, 0x0C98, 0x0C90, 0x0C9C,
				MT8189_TOP_AXI_PROT_EN_MCU_STA_0_CONN),
			BUS_PROT_IGN(IFR_TYPE, 0x0C54, 0x0C58, 0x0C50, 0x0C5C,
				MT8189_TOP_AXI_PROT_EN_INFRASYS_STA_1_CONN),
			BUS_PROT_IGN(IFR_TYPE, 0x0C94, 0x0C98, 0x0C90, 0x0C9C,
				MT8189_TOP_AXI_PROT_EN_MCU_STA_0_CONN_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0C44, 0x0C48, 0x0C40, 0x0C4C,
				MT8189_TOP_AXI_PROT_EN_INFRASYS_STA_0_CONN),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT8189_POWER_DOMAIN_AUDIO] = {
		.name = "audio",
		.ctl_offs = 0xE18,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"audio"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C84, 0x0C88, 0x0C80, 0x0C8C,
				MT8189_TOP_AXI_PROT_EN_PERISYS_STA_0_AUDIO),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT8189_POWER_DOMAIN_ADSP_TOP_DORMANT] = {
		.name = "adsp-top-dormant",
		.ctl_offs = 0xE1C,
		.sram_slp_bits = GENMASK(9, 9),
		.sram_slp_ack_bits = GENMASK(13, 13),
		.basic_clk_name = {"vadsp"},
		.subsys_clk_prefix = "vadsp-top",
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_SRAM_SLP | MTK_SCPD_IS_PWR_CON_ON
				| MTK_SCPD_ACTIVE_WAKEUP | MTK_SCPD_KEEP_DEFAULT_OFF | default_cap,
	},
	[MT8189_POWER_DOMAIN_ADSP_INFRA] = {
		.name = "adsp-infra",
		.ctl_offs = 0xE20,
		.basic_clk_name = {"vadsp"},
		.subsys_clk_prefix = "vadsp",
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_KEEP_DEFAULT_OFF | default_cap,
	},
	[MT8189_POWER_DOMAIN_ADSP_AO] = {
		.name = "adsp-ao",
		.ctl_offs = 0xE24,
		.basic_clk_name = {"vadsp"},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT8189_POWER_DOMAIN_ISP_IMG1] = {
		.name = "isp-img1",
		.ctl_offs = 0xE28,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"img1"},
		.subsys_clk_prefix = "img1",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT8189_TOP_AXI_PROT_EN_MMSYS_STA_0_ISP_IMG1),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT8189_TOP_AXI_PROT_EN_MMSYS_STA_1_ISP_IMG1),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_KEEP_DEFAULT_OFF | default_cap,
	},
	[MT8189_POWER_DOMAIN_ISP_IMG2] = {
		.name = "isp-img2",
		.ctl_offs = 0xE2C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"img1"},
		.subsys_clk_prefix = "img2",
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_KEEP_DEFAULT_OFF | default_cap,
	},
	[MT8189_POWER_DOMAIN_ISP_IPE] = {
		.name = "isp-ipe",
		.ctl_offs = 0xE30,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"ipe"},
		.subsys_clk_prefix = "ipe",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT8189_TOP_AXI_PROT_EN_MMSYS_STA_0_ISP_IPE),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT8189_TOP_AXI_PROT_EN_MMSYS_STA_1_ISP_IPE),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_KEEP_DEFAULT_OFF | default_cap,
	},
	[MT8189_POWER_DOMAIN_VDE0] = {
		.name = "vde0",
		.ctl_offs = 0xE38,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"vdec"},
		.subsys_clk_prefix = "vdec",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT8189_TOP_AXI_PROT_EN_MMSYS_STA_0_VDE0),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT8189_TOP_AXI_PROT_EN_MMSYS_STA_1_VDE0),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT8189_POWER_DOMAIN_VEN0] = {
		.name = "ven0",
		.ctl_offs = 0xE40,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"venc"},
		.subsys_clk_prefix = "venc",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT8189_TOP_AXI_PROT_EN_MMSYS_STA_0_VEN0),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT8189_TOP_AXI_PROT_EN_MMSYS_STA_1_VEN0),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT8189_POWER_DOMAIN_CAM_MAIN] = {
		.name = "cam-main",
		.ctl_offs = 0xE48,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"cam"},
		.subsys_clk_prefix = "cam_main",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT8189_TOP_AXI_PROT_EN_MMSYS_STA_0_CAM_MAIN),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT8189_TOP_AXI_PROT_EN_MMSYS_STA_1_CAM_MAIN),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_KEEP_DEFAULT_OFF | default_cap,
	},
	[MT8189_POWER_DOMAIN_CAM_SUBA] = {
		.name = "cam-suba",
		.ctl_offs = 0xE50,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"cam"},
		.subsys_clk_prefix = "cam_suba",
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_KEEP_DEFAULT_OFF | default_cap,
	},
	[MT8189_POWER_DOMAIN_CAM_SUBB] = {
		.name = "cam-subb",
		.ctl_offs = 0xE54,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"cam"},
		.subsys_clk_prefix = "cam_subb",
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_KEEP_DEFAULT_OFF | default_cap,
	},
	[MT8189_POWER_DOMAIN_MDP0] = {
		.name = "mdp0",
		.ctl_offs = 0xE68,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"mdp"},
		.subsys_clk_prefix = "mdp0",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT8189_TOP_AXI_PROT_EN_MMSYS_STA_0_MDP0),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT8189_POWER_DOMAIN_DISP] = {
		.name = "disp",
		.ctl_offs = 0xE70,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"disp"},
		.subsys_clk_prefix = "disp",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT8189_TOP_AXI_PROT_EN_MMSYS_STA_0_DISP),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT8189_POWER_DOMAIN_MM_INFRA] = {
		.name = "mm-infra",
		.ctl_offs = 0xE78,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"mm_infra"},
		.subsys_clk_prefix = "mm_infra",
		.bp_table = {
			BUS_PROT(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT8189_TOP_AXI_PROT_EN_MMSYS_STA_1_MM_INFRA),
			BUS_PROT(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT8189_TOP_AXI_PROT_EN_MMSYS_STA_1_MM_INFRA_2ND),
			BUS_PROT_CLK_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT8189_TOP_AXI_PROT_EN_MMSYS_STA_1_MM_INFRA_CLK_IGN),
			BUS_PROT_CLK_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT8189_TOP_AXI_PROT_EN_MMSYS_STA_1_MM_INFRA_2ND_CLK_IGN),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT8189_POWER_DOMAIN_DP_TX] = {
		.name = "dp-tx",
		.ctl_offs = 0xE80,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_KEEP_DEFAULT_OFF | default_cap,
	},
	[MT8189_POWER_DOMAIN_SCP_CORE] = {
		.name = "scp-core",
		.ctl_offs = 0xE84,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_CON(VLPCFG_REG_TYPE, 0x091C, 0x091C, 0x091C, 0x091C,
				MT8189_AXI_PROT_EN_SCP_CORE,
				MT8189_AXI_PROT_EN_SCP_CORE),
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT8189_VLP_AXI_PROT_EN_SCP_CORE),
			BUS_PROT_CON(VLPCFG_REG_TYPE, 0x091C, 0x091C, 0x091C, 0x091C,
				MT8189_AXI_PROT_EN_SCP_CORE_2ND,
				MT8189_AXI_PROT_EN_SCP_CORE_2ND),
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT8189_VLP_AXI_PROT_EN_SCP_CORE_2ND),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_SRAM_SLP
				| MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT8189_POWER_DOMAIN_CSI_RX] = {
		.name = "csi-rx",
		.ctl_offs = 0xE9C,
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_KEEP_DEFAULT_OFF | default_cap,
	},
	[MT8189_POWER_DOMAIN_SSUSB] = {
		.name = "ssusb",
		.ctl_offs = 0xEA8,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C84, 0x0C88, 0x0C80, 0x0C8C,
				MT8189_TOP_AXI_PROT_EN_PERISYS_STA_0_SSUSB),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_ACTIVE_WAKEUP |
				MTK_SCPD_KEEP_DEFAULT_OFF | default_cap,
	},
	[MT8189_POWER_DOMAIN_MFG0] = {
		.name = "mfg0",
		.ctl_offs = 0xEB4,
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_KEEP_DEFAULT_OFF | default_cap,
	},
	[MT8189_POWER_DOMAIN_MFG1] = {
		.name = "mfg1",
		.ctl_offs = 0xEB8,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"mfg1"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C54, 0x0C58, 0x0C50, 0x0C5C,
				MT8189_TOP_AXI_PROT_EN_INFRASYS_STA_1_MFG1),
			BUS_PROT_IGN(IFR_TYPE, 0x0CA4, 0x0CA8, 0x0CA0, 0x0CAC,
				MT8189_TOP_AXI_PROT_EN_MD_STA_0_MFG1),
			BUS_PROT_IGN(IFR_TYPE, 0x0CA4, 0x0CA8, 0x0CA0, 0x0CAC,
				MT8189_TOP_AXI_PROT_EN_MD_STA_0_MFG1_2ND),
			BUS_PROT_IGN(EMICFG_AO_MEM_TYPE, 0x0084, 0x0088, 0x0080, 0x008C,
				MT8189_AXI_PROT_EN_MFG1),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_KEEP_DEFAULT_OFF | default_cap,
	},
	[MT8189_POWER_DOMAIN_MFG2] = {
		.name = "mfg2",
		.ctl_offs = 0xEBC,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_KEEP_DEFAULT_OFF | default_cap,
	},
	[MT8189_POWER_DOMAIN_MFG3] = {
		.name = "mfg3",
		.ctl_offs = 0xEC0,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_KEEP_DEFAULT_OFF | default_cap,
	},
	[MT8189_POWER_DOMAIN_EDP_TX_DORMANT] = {
		.name = "edp-tx-dormant",
		.ctl_offs = 0xF70,
		.sram_slp_bits = GENMASK(9, 9),
		.sram_slp_ack_bits = 0,
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_SRAM_SLP | MTK_SCPD_IS_PWR_CON_ON
				| MTK_SCPD_KEEP_DEFAULT_OFF | default_cap,
	},
	[MT8189_POWER_DOMAIN_PCIE] = {
		.name = "pcie",
		.ctl_offs = 0xF74,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_KEEP_DEFAULT_OFF | default_cap,
	},
	[MT8189_POWER_DOMAIN_PCIE_PHY] = {
		.name = "pcie-phy",
		.ctl_offs = 0xF78,
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_KEEP_DEFAULT_OFF | MTK_SCPD_BYPASS_INIT_ON
			| default_cap,
	},
};

static const struct scp_subdomain scp_subdomain_mt8189_spm[] = {
	{MT8189_POWER_DOMAIN_ADSP_AO, MT8189_POWER_DOMAIN_ADSP_INFRA},
	{MT8189_POWER_DOMAIN_ADSP_INFRA, MT8189_POWER_DOMAIN_ADSP_TOP_DORMANT},
	{MT8189_POWER_DOMAIN_MM_INFRA, MT8189_POWER_DOMAIN_ISP_IMG1},
	{MT8189_POWER_DOMAIN_ISP_IMG1, MT8189_POWER_DOMAIN_ISP_IMG2},
	{MT8189_POWER_DOMAIN_MM_INFRA, MT8189_POWER_DOMAIN_ISP_IPE},
	{MT8189_POWER_DOMAIN_MM_INFRA, MT8189_POWER_DOMAIN_VDE0},
	{MT8189_POWER_DOMAIN_MM_INFRA, MT8189_POWER_DOMAIN_VEN0},
	{MT8189_POWER_DOMAIN_MM_INFRA, MT8189_POWER_DOMAIN_CAM_MAIN},
	{MT8189_POWER_DOMAIN_CAM_MAIN, MT8189_POWER_DOMAIN_CAM_SUBA},
	{MT8189_POWER_DOMAIN_CAM_MAIN, MT8189_POWER_DOMAIN_CAM_SUBB},
	{MT8189_POWER_DOMAIN_MM_INFRA, MT8189_POWER_DOMAIN_MDP0},
	{MT8189_POWER_DOMAIN_MM_INFRA, MT8189_POWER_DOMAIN_DISP},
	{MT8189_POWER_DOMAIN_DISP, MT8189_POWER_DOMAIN_DP_TX},
	{MT8189_POWER_DOMAIN_MFG0, MT8189_POWER_DOMAIN_MFG1},
	{MT8189_POWER_DOMAIN_MFG1, MT8189_POWER_DOMAIN_MFG2},
	{MT8189_POWER_DOMAIN_MFG1, MT8189_POWER_DOMAIN_MFG3},
	{MT8189_POWER_DOMAIN_DP_TX, MT8189_POWER_DOMAIN_EDP_TX_DORMANT},
	{MT8189_POWER_DOMAIN_PCIE, MT8189_POWER_DOMAIN_PCIE_PHY},
};

static const struct scp_soc_data mt8189_spm_data = {
	.domains = scp_domain_mt8189_spm_data,
	.num_domains = MT8189_SPM_POWER_DOMAIN_NR,
	.subdomains = scp_subdomain_mt8189_spm,
	.num_subdomains = ARRAY_SIZE(scp_subdomain_mt8189_spm),
	.regs = {
		.pwr_sta_offs = 0xF40,
		.pwr_sta2nd_offs = 0xF44,
	}
};

/*
 * scpsys driver init
 */

static const struct of_device_id of_scpsys_match_tbl[] = {
	{
		.compatible = "mediatek,mt8189-scpsys",
		.data = &mt8189_spm_data,
	}, {
		/* sentinel */
	}
};

static int mt8189_scpsys_probe(struct platform_device *pdev)
{
	const struct scp_subdomain *sd;
	const struct scp_soc_data *soc;
	struct scp *scp;
	struct genpd_onecell_data *pd_data;
	int i, ret;

	soc = of_device_get_match_data(&pdev->dev);
	if (!soc)
		return -EINVAL;

	scp = init_scp(pdev, soc->domains, soc->num_domains, &soc->regs,
		       soc->bus_prot_reg_update, bus_list, BUS_TYPE_NUM);
	if (IS_ERR(scp))
		return PTR_ERR(scp);

	ret = mtk_register_power_domains(pdev, scp, soc->num_domains);
	if (ret)
		return ret;

	pd_data = &scp->pd_data;

	for (i = 0, sd = soc->subdomains; i < soc->num_subdomains; i++, sd++) {
		ret = pm_genpd_add_subdomain(pd_data->domains[sd->origin],
					     pd_data->domains[sd->subdomain]);
		if (ret && IS_ENABLED(CONFIG_PM)) {
			dev_info(&pdev->dev,
				 "Failed to add subdomain %s->%s: %d\n",
				 pd_data->domains[sd->origin]->name,
				 pd_data->domains[sd->subdomain]->name, ret);
			return ret;
		}
	}
	return 0;
}

static struct platform_driver mt8189_scpsys_drv = {
	.probe = mt8189_scpsys_probe,
	.driver = {
		.name = "mtk-scpsys-mt8189",
		.suppress_bind_attrs = true,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(of_scpsys_match_tbl),
	},
};

module_platform_driver(mt8189_scpsys_drv);
MODULE_LICENSE("GPL");
