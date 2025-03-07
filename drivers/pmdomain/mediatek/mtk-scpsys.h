/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef __MTK_SCPSYS_H__
#define __MTK_SCPSYS_H__

#include <linux/pm_domain.h>

#define MTK_POLL_DELAY_US		10
#define MTK_POLL_TIMEOUT		USEC_PER_SEC

#define MTK_SCPD_ACTIVE_WAKEUP		BIT(0)
#define MTK_SCPD_FWAIT_SRAM		BIT(1)
#define MTK_SCPD_SRAM_ISO		BIT(2)
#define MTK_SCPD_MD_OPS			BIT(3)
#define MTK_SCPD_ALWAYS_ON		BIT(4)
#define MTK_SCPD_APU_OPS		BIT(5)
#define MTK_SCPD_SRAM_SLP		BIT(6)
#define MTK_SCPD_BYPASS_INIT_ON		BIT(7)
#define MTK_SCPD_IS_PWR_CON_ON		BIT(8)
#define MTK_SCPD_L2TCM_SRAM		BIT(9)
#define MTK_SCPD_BYPASS_CLK		BIT(10)
#define MTK_SCPD_HWV_OPS		BIT(11)
#define MTK_SCPD_NON_CPU_RTFF		BIT(12)
#define MTK_SCPD_PEXTP_PHY_RTFF		BIT(13)
#define MTK_SCPD_UFS_RTFF		BIT(14)
#define MTK_SCPD_BYPASS_OFF		BIT(15)
#define MTK_SCPD_RTFF_DELAY		BIT(16)
#define MTK_SCPD_REMOVE_MD_RSTB		BIT(17)
#define MTK_SCPD_MMINFRA_HWV_OPS	BIT(18)
#define MTK_SCPD_IRQ_SAVE		BIT(19)
#define MTK_SCPD_PROFILE		BIT(20)
#define MTK_SCPD_WAIT_VCP		BIT(21)
#define MTK_SCPD_KEEP_DEFAULT_OFF	BIT(22)

#define MAX_CLKS	5
#define MAX_SUBSYS_CLKS 20
#define MAX_STEPS		14
#define MAX_SRAM_STEPS	4
#define MAX_CHILDREN	2

#define _BUS_PROT(_type, _set_ofs, _clr_ofs,			\
		_en_ofs, _sta_ofs, _mask, _ack_mask,		\
		_ignore_clr_ack, _ignore_clk) {				\
		.type = _type,					\
		.set_ofs = _set_ofs,				\
		.clr_ofs = _clr_ofs,				\
		.en_ofs = _en_ofs,				\
		.sta_ofs = _sta_ofs,				\
		.mask = _mask,					\
		.ack_mask = _ack_mask,				\
		.ignore_clr_ack = _ignore_clr_ack,		\
		.ignore_clk = _ignore_clk,		\
	}

#define BUS_PROT(_type, _set_ofs, _clr_ofs,		\
		_en_ofs, _sta_ofs, _mask)		\
		_BUS_PROT(_type, _set_ofs, _clr_ofs,	\
		_en_ofs, _sta_ofs, _mask, _mask, false, false)

#define BUS_PROT_IGN(_type, _set_ofs, _clr_ofs,	\
		_en_ofs, _sta_ofs, _mask)		\
		_BUS_PROT(_type, _set_ofs, _clr_ofs,	\
		_en_ofs, _sta_ofs, _mask, _mask, true, false)

#define BUS_PROT_CON(_type, _set_ofs, _clr_ofs,	\
		_en_ofs, _sta_ofs, _mask, _ack_mask)		\
		_BUS_PROT(_type, 0x0, 0x0,			\
		_en_ofs, _sta_ofs, _mask, _ack_mask, true, false)

#define BUS_PROT_CLK_IGN(_type, _set_ofs, _clr_ofs,	\
		_en_ofs, _sta_ofs, _mask)		\
		_BUS_PROT(_type, _set_ofs, _clr_ofs,	\
		_en_ofs, _sta_ofs, _mask, _mask, true, true)

#define _SRAM_CTRL(_offs, _msk, _ack_msk, _wait_ack) {	\
		.offs = _offs,			\
		.msk = _msk,			\
		.ack_msk = _ack_msk,		\
		.wait_ack = _wait_ack,		\
	}

#define SRAM_NO_ACK(_offs, _msk)	\
		_SRAM_CTRL(_offs, (BIT(_msk)), (BIT(_msk)), false)	\

struct bus_prot {
	u32 type;
	u32 set_ofs;
	u32 clr_ofs;
	u32 en_ofs;
	u32 sta_ofs;
	u32 mask;
	u32 ack_mask;
	bool ignore_clr_ack;
	bool ignore_clk;
};

struct sram_ctl {
	u32 offs;
	u32 msk;
	u32 ack_msk;
	bool wait_ack;
};

/**
 * struct scp_domain_data - scp domain data for power on/off flow
 * @name: The domain name.
 * @sta_mask: The mask for power on/off status bit.
 * @ctl_offs: The offset for main power control register.
 * @sram_pdn_bits: The mask for sram power control bits.
 * @sram_pdn_ack_bits: The mask for sram power control acked bits.
 * @sram_slp_bits: The mask for sram sleep control bits.
 * @sram_slp_ack_bits: The mask for sram sleep control acked bits.
 * @basic_clk_name: The basic clocks required by this power domain.
 * @subsys_clk_prefix: The prefix name of the clocks need to be enabled
 *                     before releasing bus protection.
 * @caps: The flag for active wake-up action.
 * @bp_table: The mask table for multiple step bus protection.
 * @sram_offs_table: The offset table for L2TCM SRAM control.
 */
struct scp_domain_data {
	const char *name;
	const char *hwv_comp;
	u32 sta_mask;
	int ctl_offs;
	u32 hwv_done_ofs;
	u32 hwv_ofs;
	u32 hwv_set_ofs;
	u32 hwv_clr_ofs;
	u32 hwv_en_ofs;
	u32 hwv_set_sta_ofs;
	u32 hwv_clr_sta_ofs;
	u8 hwv_shift;
	u8 ipi_shift;
	u32 sram_pdn_bits;
	u32 sram_pdn_ack_bits;
	u32 sram_slp_bits;
	u32 sram_slp_ack_bits;
	int extb_iso_offs;
	u32 extb_iso_bits;
	const char *basic_clk_name[MAX_CLKS];
	const char *subsys_clk_prefix;
	u32 caps;
	struct bus_prot bp_table[MAX_STEPS];
	struct sram_ctl sram_table[MAX_SRAM_STEPS];
	u32 child[MAX_CHILDREN];
	spinlock_t *lock;
};

struct scp;

struct scp_domain {
	struct generic_pm_domain genpd;
	struct scp *scp;
	struct clk *clk[MAX_CLKS];
	struct clk *subsys_clk[MAX_SUBSYS_CLKS];
	const struct scp_domain_data *data;
	struct regulator *supply;
	struct regmap *hwv_regmap;
	bool rtff_flag;
	int boot_status;
	bool is_on;
};

struct scp_ctrl_reg {
	int pwr_sta_offs;
	int pwr_sta2nd_offs;
};

struct scp {
	struct scp_domain *domains;
	struct genpd_onecell_data pd_data;
	struct device *dev;
	void __iomem *base;
	struct regmap **bus_regmap;
	struct regmap *hwv_regmap;
	struct scp_ctrl_reg ctrl_reg;
	int num_bus_type;
	bool bus_prot_reg_update;
};

struct scp_subdomain {
	int origin;
	int subdomain;
};

struct scp_soc_data {
	const struct scp_domain_data *domains;
	int num_domains;
	const struct scp_subdomain *subdomains;
	int num_subdomains;
	const struct scp_ctrl_reg regs;
	bool bus_prot_reg_update;
};

struct scp_event_data {
	int event_type;
	int domain_id;
	struct generic_pm_domain *genpd;
};

enum scp_event_type {
	MTK_SCPSYS_PSTATE,
};

struct apu_callbacks {
	int (*apu_power_on)(void);
	int (*apu_power_off)(void);
};

/* register new apu_callbacks and return previous apu_callbacks. */
extern void register_apu_callback(struct apu_callbacks *apucb);

int register_scpsys_notifier(struct notifier_block *nb);
int unregister_scpsys_notifier(struct notifier_block *nb);

struct scp *init_scp(struct platform_device *pdev,
			const struct scp_domain_data *scp_domain_data, int num,
			const struct scp_ctrl_reg *scp_ctrl_reg,
			bool bus_prot_reg_update,
			const char *bus_list[],
			unsigned int type_num);

int mtk_register_power_domains(struct platform_device *pdev,
				struct scp *scp, int num);

int __mminfra_hwv_power_ctrl(struct scp_domain *scpd, struct regmap *regmap,
			 struct device *dev, const char *name, bool onoff);
#endif /* __MTK_SCPSYS_H__ */
