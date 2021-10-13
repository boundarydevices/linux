/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __SOC_MEDIATEK_MT8365_PM_DOMAINS_H
#define __SOC_MEDIATEK_MT8365_PM_DOMAINS_H

#include "mtk-pm-domains.h"
#include <dt-bindings/power/mt8365-power.h>

/*
 * MT8365 power domain support
 */

static const struct scpsys_domain_data scpsys_domain_data_mt8365[] = {
	[MT8365_POWER_DOMAIN_MM] = {
		.name = "mm",
		.sta_mask = PWR_STATUS_DISP,
		.ctl_offs = 0x30c,
		.pwr_sta_offs = 0x0180,
		.pwr_sta2nd_offs = 0x0184,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_STRICT_BUSP,
		.bp_infracfg = {
			BUS_PROT_WR(BIT(16) | BIT(17), 0x2a8, 0x2ac, 0x258),
			BUS_PROT_WR(BIT(1) | BIT(2) | BIT(10) | BIT(11), 0x2a0, 0x2a4, 0x228),
			BUS_PROT_WAYEN(BIT(6), BIT(24), 0x200, 0x0),
			BUS_PROT_WAYEN(BIT(5), BIT(14), 0x234, 0x28),
			BUS_PROT_WR(BIT(6), 0x2a0, 0x2a4, 0x228),
                },
	},
	[MT8365_POWER_DOMAIN_VENC] = {
		.name = "venc",
		.sta_mask = PWR_STATUS_VENC,
		.ctl_offs = 0x0304,
		.pwr_sta_offs = 0x0180,
		.pwr_sta2nd_offs = 0x0184,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_smi = {
			BUS_PROT_WR(BIT(1), 0x3c4, 0x3c8, 0x3c0),
		},
	},
	[MT8365_POWER_DOMAIN_AUDIO] = {
		.name = "audio",
		.sta_mask = PWR_STATUS_AUDIO,
		.ctl_offs = 0x0314,
		.pwr_sta_offs = 0x0180,
		.pwr_sta2nd_offs = 0x0184,
		.sram_pdn_bits = GENMASK(12, 8),
		.sram_pdn_ack_bits = GENMASK(17, 13),
		.bp_infracfg = {
			BUS_PROT_WR(BIT(27) | BIT(28), 0x2a8, 0x2ac, 0x258),
		},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT8365_POWER_DOMAIN_CONN] = {
		.name = "conn",
		.sta_mask = PWR_STATUS_CONN,
		.ctl_offs = 0x032c,
		.pwr_sta_offs = 0x0180,
		.pwr_sta2nd_offs = 0x0184,
		.sram_pdn_bits = 0,
		.sram_pdn_ack_bits = 0,
		.bp_infracfg = {
			BUS_PROT_WR(BIT(13), 0x2a0, 0x2a4, 0x228),
			BUS_PROT_WR(BIT(18), 0x2a8, 0x2ac, 0x258),
			BUS_PROT_WR(BIT(14), 0x2a0, 0x2a4, 0x228),
			BUS_PROT_WR(BIT(21), 0x2a8, 0x2ac, 0x258),
		},
		.caps = MTK_SCPD_ACTIVE_WAKEUP | MTK_SCPD_KEEP_DEFAULT_OFF,
	},
	[MT8365_POWER_DOMAIN_MFG] = {
		.name = "mfg",
		.sta_mask = PWR_STATUS_MFG,
		.ctl_offs = 0x0338,
		.pwr_sta_offs = 0x0180,
		.pwr_sta2nd_offs = 0x0184,
		.sram_pdn_bits = GENMASK(9, 8),
		.sram_pdn_ack_bits = GENMASK(13, 12),
		.bp_infracfg = {
			BUS_PROT_WR(BIT(25), 0x2a0, 0x2a4, 0x228),
			BUS_PROT_WR(BIT(21) | BIT(22), 0x2a0, 0x2a4, 0x228),
		},
	},
	[MT8365_POWER_DOMAIN_CAM] = {
		.name = "cam",
		.sta_mask = BIT(25),
		.ctl_offs = 0x0344,
		.pwr_sta_offs = 0x0180,
		.pwr_sta2nd_offs = 0x0184,
		.sram_pdn_bits = GENMASK(9, 8),
		.sram_pdn_ack_bits = GENMASK(13, 12),
		.bp_infracfg = {
			BUS_PROT_WR(BIT(19), 0x2a8, 0x2ac, 0x258),
		},
		.bp_smi = {
			BUS_PROT_WR(BIT(2), 0x3c4, 0x3c8, 0x3c0),
		},
	},
	[MT8365_POWER_DOMAIN_VDEC] = {
		.name = "vdec",
		.sta_mask = BIT(31),
		.ctl_offs = 0x0370,
		.pwr_sta_offs = 0x0180,
		.pwr_sta2nd_offs = 0x0184,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_smi = {
			BUS_PROT_WR(BIT(3), 0x3c4, 0x3c8, 0x3c0),
		},
	},
	[MT8365_POWER_DOMAIN_APU] = {
		.name = "apu",
		.sta_mask = BIT(16),
		.ctl_offs = 0x0378,
		.pwr_sta_offs = 0x0180,
		.pwr_sta2nd_offs = 0x0184,
		.sram_pdn_bits = GENMASK(14, 8),
		.sram_pdn_ack_bits = GENMASK(21, 15),
		.bp_infracfg = {
			BUS_PROT_WR(BIT(2) | BIT(20), 0x2a8, 0x2ac, 0x258),
		},
		.bp_smi = {
			BUS_PROT_WR(BIT(4), 0x3c4, 0x3c8, 0x3c0),
		},
	},
	[MT8365_POWER_DOMAIN_DSP] = {
		.name = "dsp",
		.sta_mask = BIT(17),
		.ctl_offs = 0x037C,
		.pwr_sta_offs = 0x0180,
		.pwr_sta2nd_offs = 0x0184,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
		.bp_infracfg = {
			BUS_PROT_WR(BIT(24) | BIT(30) | BIT(31), 0x2a8, 0x2ac, 0x258),
		},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
};

static const struct scpsys_soc_data mt8365_scpsys_data = {
	.domains_data = scpsys_domain_data_mt8365,
	.num_domains = ARRAY_SIZE(scpsys_domain_data_mt8365),
};

#endif /* __SOC_MEDIATEK_MT8365_PM_DOMAINS_H */
