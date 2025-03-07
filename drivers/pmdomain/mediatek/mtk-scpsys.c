// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015 Pengutronix, Sascha Hauer <kernel@pengutronix.de>
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
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/soc/mediatek/infracfg.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>

#include "mtk-scpsys.h"

#include <dt-bindings/power/mt2701-power.h>
#include <dt-bindings/power/mt2712-power.h>
#include <dt-bindings/power/mt6797-power.h>
#include <dt-bindings/power/mt7622-power.h>
#include <dt-bindings/power/mt7623a-power.h>
#include <dt-bindings/power/mt8173-power.h>

#define MTK_POLL_DELAY_US   10
#define MTK_POLL_TIMEOUT    USEC_PER_SEC

#define MTK_SCPD_CAPS(_scpd, _x)	((_scpd)->data->caps & (_x))

#define SPM_VDE_PWR_CON			0x0210
#define SPM_MFG_PWR_CON			0x0214
#define SPM_VEN_PWR_CON			0x0230
#define SPM_ISP_PWR_CON			0x0238
#define SPM_DIS_PWR_CON			0x023c
#define SPM_CONN_PWR_CON		0x0280
#define SPM_VEN2_PWR_CON		0x0298
#define SPM_AUDIO_PWR_CON		0x029c	/* MT8173, MT2712 */
#define SPM_BDP_PWR_CON			0x029c	/* MT2701 */
#define SPM_ETH_PWR_CON			0x02a0
#define SPM_HIF_PWR_CON			0x02a4
#define SPM_IFR_MSC_PWR_CON		0x02a8
#define SPM_MFG_2D_PWR_CON		0x02c0
#define SPM_MFG_ASYNC_PWR_CON		0x02c4
#define SPM_USB_PWR_CON			0x02cc
#define SPM_USB2_PWR_CON		0x02d4	/* MT2712 */
#define SPM_ETHSYS_PWR_CON		0x02e0	/* MT7622 */
#define SPM_HIF0_PWR_CON		0x02e4	/* MT7622 */
#define SPM_HIF1_PWR_CON		0x02e8	/* MT7622 */
#define SPM_WB_PWR_CON			0x02ec	/* MT7622 */

#define SPM_PWR_STATUS			0x060c
#define SPM_PWR_STATUS_2ND		0x0610

#define PWR_RST_B_BIT			BIT(0)
#define PWR_ISO_BIT			BIT(1)
#define PWR_ON_BIT			BIT(2)
#define PWR_ON_2ND_BIT			BIT(3)
#define PWR_CLK_DIS_BIT			BIT(4)
#define PWR_SRAM_CLKISO_BIT		BIT(5)
#define PWR_SRAM_ISOINT_B_BIT		BIT(6)
#define PWR_ACK				BIT(30)
#define PWR_ACK_2ND			BIT(31)

#define PWR_STATUS_CONN			BIT(1)
#define PWR_STATUS_DISP			BIT(3)
#define PWR_STATUS_MFG			BIT(4)
#define PWR_STATUS_ISP			BIT(5)
#define PWR_STATUS_VDEC			BIT(7)
#define PWR_STATUS_BDP			BIT(14)
#define PWR_STATUS_ETH			BIT(15)
#define PWR_STATUS_HIF			BIT(16)
#define PWR_STATUS_IFR_MSC		BIT(17)
#define PWR_STATUS_USB2			BIT(19)	/* MT2712 */
#define PWR_STATUS_VENC_LT		BIT(20)
#define PWR_STATUS_VENC			BIT(21)
#define PWR_STATUS_MFG_2D		BIT(22)	/* MT8173 */
#define PWR_STATUS_MFG_ASYNC		BIT(23)	/* MT8173 */
#define PWR_STATUS_AUDIO		BIT(24)	/* MT8173, MT2712 */
#define PWR_STATUS_USB			BIT(25)	/* MT8173, MT2712 */
#define PWR_STATUS_ETHSYS		BIT(24)	/* MT7622 */
#define PWR_STATUS_HIF0			BIT(25)	/* MT7622 */
#define PWR_STATUS_HIF1			BIT(26)	/* MT7622 */
#define PWR_STATUS_WB			BIT(27)	/* MT7622 */

enum regmap_type {
	INVALID_TYPE = 0,
	IFR_TYPE,
	VLP_TYPE,
	BUS_TYPE_NUM,
};

static const char *bus_list[BUS_TYPE_NUM] = {
	[INVALID_TYPE] = "invalid",
	[IFR_TYPE] = "infracfg",
	[VLP_TYPE] = "vlpcfg",
};

static bool scpsys_init_flag;

static int __scpsys_domain_is_on(void __iomem *addr, u32 sta_mask)
{
	u32 status = readl(addr) & sta_mask;

	/*
	 * A domain is on when both status bits are set. If only one is set
	 * return an error. This happens while powering up a domain
	 */

	if (status == sta_mask)
		return true;
	if (!status)
		return false;

	return -EINVAL;
}

static int scpsys_sta_is_on(struct scp_domain *scpd)
{
	struct scp *scp = scpd->scp;

	return __scpsys_domain_is_on(scp->base + scp->ctrl_reg.pwr_sta_offs,
			scpd->data->sta_mask);
}

static int scpsys_sta_2nd_is_on(struct scp_domain *scpd)
{
	struct scp *scp = scpd->scp;

	return __scpsys_domain_is_on(scp->base + scp->ctrl_reg.pwr_sta2nd_offs,
			scpd->data->sta_mask);
}

static int scpsys_pwr_ack_is_on(struct scp_domain *scpd)
{
	struct scp *scp = scpd->scp;

	return __scpsys_domain_is_on(scp->base + scpd->data->ctl_offs,
			PWR_ACK);
}

static int scpsys_pwr_ack_2nd_is_on(struct scp_domain *scpd)
{
	struct scp *scp = scpd->scp;

	return __scpsys_domain_is_on(scp->base + scpd->data->ctl_offs,
			PWR_ACK_2ND);
}

static int scpsys_regulator_is_enabled(struct scp_domain *scpd)
{
	if (!scpd->supply)
		return 0;

	return regulator_is_enabled(scpd->supply);
}

static int scpsys_regulator_enable(struct scp_domain *scpd)
{
	if (!scpd->supply)
		return 0;

	return regulator_enable(scpd->supply);
}

static int scpsys_regulator_disable(struct scp_domain *scpd)
{
	if (!scpd->supply)
		return 0;

	return regulator_disable(scpd->supply);
}

static void scpsys_clk_disable(struct clk *clk[], int max_num)
{
	int i;

	for (i = max_num - 1; i >= 0; i--)
		clk_disable_unprepare(clk[i]);
}

static int scpsys_clk_enable(struct clk *clk[], int max_num)
{
	int i, ret = 0;

	for (i = 0; i < max_num && clk[i]; i++) {
		ret = clk_prepare_enable(clk[i]);
		if (ret) {
			scpsys_clk_disable(clk, i);
			break;
		}
	}

	return ret;
}

static int scpsys_sram_on(struct scp_domain *scpd, void __iomem *ctl_addr,
			u32 set_bits, u32 ack_bits, bool wait_ack)
{
	u32 ack_mask, ack_sta;
	u32 val;
	int tmp;
	int ret = 0;

	if (MTK_SCPD_CAPS(scpd, MTK_SCPD_SRAM_SLP)) {
		ack_mask = ack_bits;
		ack_sta = ack_mask;
		val = readl(ctl_addr) | set_bits;
	} else {
		ack_mask = ack_bits;
		ack_sta = 0;
		val = readl(ctl_addr) & ~set_bits;
	}

	writel(val, ctl_addr);

	/* Either wait until SRAM_PDN_ACK all 0 or have a force wait */
	if (MTK_SCPD_CAPS(scpd, MTK_SCPD_FWAIT_SRAM)) {
		/*
		 * Currently, MTK_SCPD_FWAIT_SRAM is necessary only for
		 * MT7622_POWER_DOMAIN_WB and thus just a trivial setup
		 * is applied here.
		 */
		usleep_range(12000, 12100);
	} else {
		/* Either wait until SRAM_PDN_ACK all 1 or 0 */
		if (wait_ack) {
			ret = readl_poll_timeout_atomic(ctl_addr, tmp,
				(tmp & ack_mask) == ack_sta,
				MTK_POLL_DELAY_US, MTK_POLL_TIMEOUT);
			if (ret < 0)
				return ret;
		}
	}

	return ret;
}

static int scpsys_sram_off(struct scp_domain *scpd, void __iomem *ctl_addr,
			u32 set_bits, u32 ack_bits, bool wait_ack)
{
	u32 val;
	u32 ack_mask, ack_sta;
	int tmp;
	int ret = 0;

	if (MTK_SCPD_CAPS(scpd, MTK_SCPD_SRAM_SLP)) {
		ack_mask = ack_bits;
		ack_sta = 0;
		val = readl(ctl_addr) & ~set_bits;
	} else {
		ack_mask = ack_bits;
		ack_sta = ack_mask;
		val = readl(ctl_addr) | set_bits;
	}
	writel(val, ctl_addr);

	/* Either wait until SRAM_PDN_ACK all 1 or 0 */
	if (wait_ack)
		ret = readl_poll_timeout_atomic(ctl_addr, tmp,
				(tmp & ack_mask) == ack_sta,
				MTK_POLL_DELAY_US, MTK_POLL_TIMEOUT);

	return ret;
}

static int scpsys_sram_enable(struct scp_domain *scpd, void __iomem *ctl_addr)
{
	u32 val;
	int ret = 0;

	if (MTK_SCPD_CAPS(scpd, MTK_SCPD_SRAM_SLP))
		ret = scpsys_sram_on(scpd, ctl_addr, scpd->data->sram_slp_bits,
				scpd->data->sram_slp_ack_bits, true);
	else
		ret = scpsys_sram_on(scpd, ctl_addr, scpd->data->sram_pdn_bits,
				scpd->data->sram_pdn_ack_bits, true);

	if (MTK_SCPD_CAPS(scpd, MTK_SCPD_SRAM_ISO)) {
		val = readl(ctl_addr) | PWR_SRAM_ISOINT_B_BIT;
		writel(val, ctl_addr);
		udelay(1);
		val &= ~PWR_SRAM_CLKISO_BIT;
		writel(val, ctl_addr);
	}

	return ret;
}

static int scpsys_sram_disable(struct scp_domain *scpd, void __iomem *ctl_addr)
{
	u32 val;
	int ret = 0;

	if (MTK_SCPD_CAPS(scpd, MTK_SCPD_SRAM_ISO)) {
		val = readl(ctl_addr) | PWR_SRAM_CLKISO_BIT;
		writel(val, ctl_addr);
		val &= ~PWR_SRAM_ISOINT_B_BIT;
		writel(val, ctl_addr);
		udelay(1);
	}

	if (MTK_SCPD_CAPS(scpd, MTK_SCPD_SRAM_SLP))
		ret = scpsys_sram_off(scpd, ctl_addr, scpd->data->sram_slp_bits,
				scpd->data->sram_slp_ack_bits, true);
	else
		ret = scpsys_sram_off(scpd, ctl_addr, scpd->data->sram_pdn_bits,
				scpd->data->sram_pdn_ack_bits, true);

	return ret;
}

static int set_bus_protection(struct regmap *map, struct bus_prot *bp)
{
	u32 val = 0;
	u32 set_ofs = bp->set_ofs;
	u32 en_ofs = bp->en_ofs;
	u32 sta_ofs = bp->sta_ofs;
	u32 mask = bp->mask;
	u32 ack_mask = bp->ack_mask;
	int retry = 0;
	int ret = 0;

	while (retry <= 10) {
		if (set_ofs)
			regmap_write(map, set_ofs, mask);
		else
			regmap_update_bits(map, en_ofs, mask, mask);

		/* check bus protect enable setting */
		regmap_read(map, en_ofs, &val);
		if ((val & mask) == mask)
			break;

		retry++;
	}

	ret = regmap_read_poll_timeout_atomic(map, sta_ofs,
			val, (val & ack_mask) == ack_mask,
			MTK_POLL_DELAY_US, MTK_POLL_TIMEOUT);

	if (ret < 0) {
		pr_err("%s val=0x%x, mask=0x%x, (val & mask)=0x%x\n",
			__func__, val, ack_mask, (val & ack_mask));
	}

	return ret;
}

static int clear_bus_protection(struct regmap *map, struct bus_prot *bp)
{
	u32 val = 0;
	u32 clr_ofs = bp->clr_ofs;
	u32 en_ofs = bp->en_ofs;
	u32 sta_ofs = bp->sta_ofs;
	u32 mask = bp->mask;
	u32 ack_mask = bp->ack_mask;
	bool ignore_ack = bp->ignore_clr_ack;
	int ret = 0;

	if (clr_ofs)
		regmap_write(map, clr_ofs, mask);
	else
		regmap_update_bits(map, en_ofs, mask, 0);

	if (ignore_ack)
		return 0;

	ret = regmap_read_poll_timeout_atomic(map, sta_ofs,
			val, !(val & ack_mask),
			MTK_POLL_DELAY_US, MTK_POLL_TIMEOUT);

	if (ret < 0) {
		pr_err("%s val=0x%x, mask=0x%x, (val & mask)=0x%x\n",
			__func__, val, ack_mask, (val & ack_mask));
	}
	return ret;
}

static int scpsys_bus_protect_disable(struct scp_domain *scpd, unsigned int index)
{
	struct scp *scp = scpd->scp;
	const struct bus_prot *bp_table = scpd->data->bp_table;
	unsigned long flags = 0;
	int ret = 0;
	int i;

	if (scpd->data->lock)
		spin_lock_irqsave(scpd->data->lock, flags);

	for (i = index; i >= 0; i--) {
		struct regmap *map;
		struct bus_prot bp = bp_table[i];

		if (bp.type > INVALID_TYPE && bp.type < scp->num_bus_type)
			map = scp->bus_regmap[bp.type];
		else
			continue;

		if (!map)
			continue;
		if (bp.ignore_clk)
			continue;

		if (index != (MAX_STEPS - 1)) {
			unsigned int val = 0, val2 = 0;

			/* reserve bus register status */
			regmap_read(map, bp.en_ofs, &val);
			regmap_read(map, bp.sta_ofs, &val2);
			pr_notice("[%d] bus en: 0x%08x, sta: 0x%08x before restore\n",
					i, val, val2);
			/* restore bus protect setting */
			clear_bus_protection(map, &bp);
		} else {
			ret = clear_bus_protection(map, &bp);

			if (ret)
				goto ERR;
		}
	}

ERR:
	if (scpd->data->lock)
		spin_unlock_irqrestore(scpd->data->lock, flags);

	return ret;
}

static int scpsys_bus_protect_disable_ignore_clock(struct scp_domain *scpd, unsigned int index)
{
	struct scp *scp = scpd->scp;
	const struct bus_prot *bp_table = scpd->data->bp_table;
	unsigned long flags = 0;
	int ret = 0;
	int i;

	if (scpd->data->lock)
		spin_lock_irqsave(scpd->data->lock, flags);

	for (i = index; i >= 0; i--) {
		struct regmap *map;
		struct bus_prot bp = bp_table[i];

		if (bp.type > INVALID_TYPE && bp.type < scp->num_bus_type)
			map = scp->bus_regmap[bp.type];
		else
			continue;

		if (!map)
			continue;
		if (!bp.ignore_clk)
			continue;

		if (index != (MAX_STEPS - 1)) {
			unsigned int val = 0, val2 = 0;

			/* reserve bus register status */
			regmap_read(map, bp.en_ofs, &val);
			regmap_read(map, bp.sta_ofs, &val2);
			pr_notice("[%d] bus en: 0x%08x, sta: 0x%08x before restore\n",
					i, val, val2);
			/* restore bus protect setting */
			clear_bus_protection(map, &bp);
		} else {
			ret = clear_bus_protection(map, &bp);

			if (ret)
				goto ERR;
		}
	}

ERR:
	if (scpd->data->lock)
		spin_unlock_irqrestore(scpd->data->lock, flags);

	return ret;
}

static int scpsys_bus_protect_enable(struct scp_domain *scpd)
{
	struct scp *scp = scpd->scp;
	const struct bus_prot *bp_table = scpd->data->bp_table;
	unsigned long flags = 0;
	int ret = 0;
	int i;

	if (scpd->data->lock)
		spin_lock_irqsave(scpd->data->lock, flags);

	for (i = 0; i < MAX_STEPS; i++) {
		struct regmap *map;
		struct bus_prot bp = bp_table[i];

		if (bp.type > INVALID_TYPE && bp.type < scp->num_bus_type)
			map = scp->bus_regmap[bp.type];
		else
			continue;
		if (!map)
			continue;
		if (bp.ignore_clk)
			continue;

		ret = set_bus_protection(map, &bp);

		if (ret) {
			if (scpd->data->lock)
				spin_unlock_irqrestore(scpd->data->lock, flags);
			scpsys_bus_protect_disable(scpd, i);
			return ret;
		}
	}

	if (scpd->data->lock)
		spin_unlock_irqrestore(scpd->data->lock, flags);

	return ret;
}

static int scpsys_bus_protect_enable_ignore_clock(struct scp_domain *scpd)
{
	struct scp *scp = scpd->scp;
	const struct bus_prot *bp_table = scpd->data->bp_table;
	unsigned long flags = 0;
	int ret = 0;
	int i;

	if (scpd->data->lock)
		spin_lock_irqsave(scpd->data->lock, flags);

	for (i = 0; i < MAX_STEPS; i++) {
		struct regmap *map;
		struct bus_prot bp = bp_table[i];

		if (bp.type > INVALID_TYPE && bp.type < scp->num_bus_type)
			map = scp->bus_regmap[bp.type];
		else
			continue;
		if (!map)
			continue;
		if (!bp.ignore_clk)
			continue;

		ret = set_bus_protection(map, &bp);

		if (ret) {
			if (scpd->data->lock)
				spin_unlock_irqrestore(scpd->data->lock, flags);
			scpsys_bus_protect_disable(scpd, i);
			return ret;
		}
	}

	if (scpd->data->lock)
		spin_unlock_irqrestore(scpd->data->lock, flags);

	return ret;
}

static int scpsys_power_on(struct generic_pm_domain *genpd)
{
	struct scp_domain *scpd = container_of(genpd, struct scp_domain, genpd);
	struct scp *scp = scpd->scp;
	void __iomem *ctl_addr = scp->base + scpd->data->ctl_offs;
	u32 val;
	int ret, tmp;

	if (MTK_SCPD_CAPS(scpd, MTK_SCPD_KEEP_DEFAULT_OFF) && !scpd->boot_status)
		return 0;

	ret = scpsys_regulator_enable(scpd);
	if (ret < 0)
		goto err_regulator;

	ret = scpsys_clk_enable(scpd->clk, MAX_CLKS);
	if (ret)
		goto err_clk;

	/* subsys power on */
	val = readl(ctl_addr);
	val |= PWR_ON_BIT;
	writel(val, ctl_addr);
	/* wait until PWR_ACK = 1 */
	if (MTK_SCPD_CAPS(scpd, MTK_SCPD_IS_PWR_CON_ON))
		ret = readx_poll_timeout_atomic(scpsys_pwr_ack_is_on, scpd, tmp, tmp > 0,
				MTK_POLL_DELAY_US, MTK_POLL_TIMEOUT);
	else
		ret = readx_poll_timeout_atomic(scpsys_sta_is_on, scpd, tmp, tmp > 0,
				MTK_POLL_DELAY_US, MTK_POLL_TIMEOUT);
	if (ret < 0)
		goto err_pwr_ack;

	udelay(50);

	/* subsys 2nd power on */
	val |= PWR_ON_2ND_BIT;
	writel(val, ctl_addr);
	/* wait until PWR_ACK = 1 */
	if (MTK_SCPD_CAPS(scpd, MTK_SCPD_IS_PWR_CON_ON))
		ret = readx_poll_timeout_atomic(scpsys_pwr_ack_2nd_is_on, scpd, tmp, tmp > 0,
				MTK_POLL_DELAY_US, MTK_POLL_TIMEOUT);
	else
		ret = readx_poll_timeout_atomic(scpsys_sta_2nd_is_on, scpd, tmp, tmp > 0,
				MTK_POLL_DELAY_US, MTK_POLL_TIMEOUT);
	if (ret < 0)
		goto err_pwr_ack;

	val &= ~PWR_CLK_DIS_BIT;
	writel(val, ctl_addr);

	val &= ~PWR_ISO_BIT;
	writel(val, ctl_addr);

	val |= PWR_RST_B_BIT;
	writel(val, ctl_addr);

	ret = scpsys_bus_protect_disable_ignore_clock(scpd, MAX_STEPS - 1);
	if (ret < 0)
		goto err_pwr_ack;

	if (!MTK_SCPD_CAPS(scpd, MTK_SCPD_BYPASS_CLK)) {
		ret = scpsys_clk_enable(scpd->subsys_clk, MAX_SUBSYS_CLKS);
		if (ret < 0)
			goto err_pwr_ack;
	}

	ret = scpsys_sram_enable(scpd, ctl_addr);
	if (ret < 0)
		goto err_sram;

	ret = scpsys_bus_protect_disable(scpd, MAX_STEPS - 1);
	if (ret < 0)
		goto err_sram;

	scpd->is_on = true;

	return 0;

err_sram:
	dev_err(scp->dev, "Failed to power on sram/bus %s(%d)\n", genpd->name, ret);
err_pwr_ack:
	dev_err(scp->dev, "Failed to power on mtcmos %s(%d)\n", genpd->name, ret);
err_clk:
	val = scpsys_regulator_is_enabled(scpd);
	dev_err(scp->dev, "Failed to enable clk %s(%d %d)\n", genpd->name, ret, val);
err_regulator:
	dev_err(scp->dev, "Failed to power on regulator %s(%d)\n", genpd->name, ret);

	return ret;
}

static int scpsys_power_off(struct generic_pm_domain *genpd)
{
	struct scp_domain *scpd = container_of(genpd, struct scp_domain, genpd);
	struct scp *scp = scpd->scp;
	void __iomem *ctl_addr = scp->base + scpd->data->ctl_offs;
	u32 val;
	int ret, tmp;

	if (MTK_SCPD_CAPS(scpd, MTK_SCPD_BYPASS_OFF)) {
		dev_err(scp->dev, "bypass power off %s for bringup\n", genpd->name);
		return 0;
	}

	ret = scpsys_bus_protect_enable(scpd);
	if (ret < 0)
		goto out;

	ret = scpsys_sram_disable(scpd, ctl_addr);
	if (ret < 0)
		goto out;

	if (!MTK_SCPD_CAPS(scpd, MTK_SCPD_BYPASS_CLK))
		scpsys_clk_disable(scpd->subsys_clk, MAX_SUBSYS_CLKS);

	ret = scpsys_bus_protect_enable_ignore_clock(scpd);
	if (ret < 0)
		goto out;

	val = readl(ctl_addr);
	val |= PWR_ISO_BIT;
	writel(val, ctl_addr);

	val &= ~PWR_RST_B_BIT;
	writel(val, ctl_addr);

	val |= PWR_CLK_DIS_BIT;
	writel(val, ctl_addr);

	val &= ~PWR_ON_BIT;
	writel(val, ctl_addr);

	/* wait until PWR_ACK = 1 */
	if (MTK_SCPD_CAPS(scpd, MTK_SCPD_IS_PWR_CON_ON))
		ret = readx_poll_timeout_atomic(scpsys_pwr_ack_is_on, scpd, tmp, tmp == 0,
				MTK_POLL_DELAY_US, MTK_POLL_TIMEOUT);
	else
		ret = readx_poll_timeout_atomic(scpsys_sta_is_on, scpd, tmp, tmp == 0,
				MTK_POLL_DELAY_US, MTK_POLL_TIMEOUT);
	if (ret < 0)
		goto out;

	/* subsys 2nd power off */
	val &= ~PWR_ON_2ND_BIT;
	writel(val, ctl_addr);

	/* wait until PWR_ACK = 0 */
	if (MTK_SCPD_CAPS(scpd, MTK_SCPD_IS_PWR_CON_ON))
		ret = readx_poll_timeout_atomic(scpsys_pwr_ack_2nd_is_on, scpd, tmp, tmp == 0,
				MTK_POLL_DELAY_US, MTK_POLL_TIMEOUT);
	else
		ret = readx_poll_timeout_atomic(scpsys_sta_2nd_is_on, scpd, tmp, tmp == 0,
				MTK_POLL_DELAY_US, MTK_POLL_TIMEOUT);
	if (ret < 0)
		goto out;

	scpsys_clk_disable(scpd->clk, MAX_CLKS);

	ret = scpsys_regulator_disable(scpd);
	if (ret < 0)
		goto out;

	scpd->is_on = false;

	return 0;

out:
	val = scpsys_regulator_is_enabled(scpd);
	dev_err(scp->dev, "Failed to power off domain %s(%d %d)\n", genpd->name, ret, val);

	return ret;
}

static int init_subsys_clks(struct platform_device *pdev,
		const char *prefix, struct clk **clk)
{
	struct device_node *node = pdev->dev.of_node;
	u32 prefix_len, sub_clk_cnt = 0;
	struct property *prop;
	const char *clk_name;

	if (!node) {
		dev_notice(&pdev->dev, "Cannot find scpsys node: %ld\n",
			PTR_ERR(node));
		return PTR_ERR(node);
	}

	prefix_len = strlen(prefix);

	of_property_for_each_string(node, "clock-names", prop, clk_name) {
		if (!strncmp(clk_name, prefix, prefix_len) &&
				(clk_name[prefix_len] == '-')) {
			if (sub_clk_cnt >= MAX_SUBSYS_CLKS) {
				dev_notice(&pdev->dev,
					"subsys clk out of range %d\n",
					sub_clk_cnt);
				return -EINVAL;
			}

			clk[sub_clk_cnt] = devm_clk_get(&pdev->dev,
						clk_name);

			if (IS_ERR(clk[sub_clk_cnt])) {
				dev_notice(&pdev->dev,
					"Subsys clk get fail %ld\n",
					PTR_ERR(clk[sub_clk_cnt]));
				return PTR_ERR(clk[sub_clk_cnt]);
			}
			sub_clk_cnt++;
		}
	}

	return sub_clk_cnt;
}

static int init_basic_clks(struct platform_device *pdev, struct clk **clk,
			const char * const *name)
{
	int i;

	for (i = 0; i < MAX_CLKS && name[i]; i++) {
		clk[i] = devm_clk_get(&pdev->dev, name[i]);

		if (IS_ERR(clk[i])) {
			dev_notice(&pdev->dev,
				"get basic clk %s fail %ld\n",
				name[i], PTR_ERR(clk[i]));
			return PTR_ERR(clk[i]);
		}
	}

	return 0;
}

static int mtk_pd_get_regmap(struct platform_device *pdev, struct regmap **regmap,
			const char *name)
{
	*regmap = syscon_regmap_lookup_by_phandle(pdev->dev.of_node, name);
	if (PTR_ERR(*regmap) == -ENODEV) {
		dev_notice(&pdev->dev, "%s regmap is null(%ld)\n",
				name, PTR_ERR(*regmap));

		*regmap = NULL;
	} else if (IS_ERR(*regmap)) {
		dev_notice(&pdev->dev, "Cannot find %s controller: %ld\n",
				name, PTR_ERR(*regmap));

		return PTR_ERR(*regmap);
	}

	return 0;
}

struct scp *init_scp(struct platform_device *pdev,
			const struct scp_domain_data *scp_domain_data, int num,
			const struct scp_ctrl_reg *scp_ctrl_reg,
			bool bus_prot_reg_update,
			const char *bus_list[],
			unsigned int type_num)
{
	struct genpd_onecell_data *pd_data;
	struct resource *res;
	int i, ret;
	struct scp *scp;

	scp = devm_kzalloc(&pdev->dev, sizeof(*scp), GFP_KERNEL);
	if (!scp)
		return ERR_PTR(-ENOMEM);

	scp->ctrl_reg.pwr_sta_offs = scp_ctrl_reg->pwr_sta_offs;
	scp->ctrl_reg.pwr_sta2nd_offs = scp_ctrl_reg->pwr_sta2nd_offs;

	scp->bus_prot_reg_update = bus_prot_reg_update;

	scp->dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	scp->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(scp->base))
		return ERR_CAST(scp->base);

	scp->domains = devm_kcalloc(&pdev->dev,
				num, sizeof(*scp->domains), GFP_KERNEL);
	if (!scp->domains)
		return ERR_PTR(-ENOMEM);

	pd_data = &scp->pd_data;

	pd_data->domains = devm_kcalloc(&pdev->dev,
			num, sizeof(*pd_data->domains), GFP_KERNEL);
	if (!pd_data->domains)
		return ERR_PTR(-ENOMEM);

	scp->num_bus_type = type_num;
	scp->bus_regmap = devm_kcalloc(&pdev->dev,
			type_num, sizeof(*scp->bus_regmap), GFP_KERNEL);
	if (!scp->bus_regmap)
		return ERR_PTR(-ENOMEM);

	/* get bus prot regmap from dts node, 0 means invalid bus type */
	for (i = 1; i < type_num; i++) {
		ret = mtk_pd_get_regmap(pdev, &scp->bus_regmap[i], bus_list[i]);
		if (ret)
			return ERR_PTR(ret);
	}

	for (i = 0; i < num; i++) {
		struct scp_domain *scpd = &scp->domains[i];
		const struct scp_domain_data *data = &scp_domain_data[i];

		scpd->supply = devm_regulator_get_optional(&pdev->dev, data->name);
		if (IS_ERR(scpd->supply)) {
			if (PTR_ERR(scpd->supply) == -ENODEV)
				scpd->supply = NULL;
			else
				return ERR_CAST(scpd->supply);
		}
	}

	pd_data->num_domains = num;

	for (i = 0; i < num; i++) {
		struct scp_domain *scpd = &scp->domains[i];
		struct generic_pm_domain *genpd = &scpd->genpd;
		const struct scp_domain_data *data = &scp_domain_data[i];

		pd_data->domains[i] = genpd;
		scpd->scp = scp;

		scpd->data = data;

		ret = init_basic_clks(pdev, scpd->clk, data->basic_clk_name);
		if (ret)
			return ERR_PTR(ret);

		if (data->subsys_clk_prefix) {
			ret = init_subsys_clks(pdev,
					data->subsys_clk_prefix,
					scpd->subsys_clk);
			if (ret < 0) {
				dev_notice(&pdev->dev,
					"%s: subsys clk unavailable\n",
					data->name);
				return ERR_PTR(ret);
			}
		}

		genpd->name = data->name;
		genpd->power_off = scpsys_power_off;
		genpd->power_on = scpsys_power_on;
		if (MTK_SCPD_CAPS(scpd, MTK_SCPD_ACTIVE_WAKEUP))
			genpd->flags |= GENPD_FLAG_ACTIVE_WAKEUP;
		if (MTK_SCPD_CAPS(scpd, MTK_SCPD_ALWAYS_ON))
			genpd->flags |= GENPD_FLAG_ALWAYS_ON;
	}

	return scp;
}
EXPORT_SYMBOL_GPL(init_scp);

int mtk_register_power_domains(struct platform_device *pdev,
				struct scp *scp, int num)
{
	struct genpd_onecell_data *pd_data;
	int i, ret = 0;

	scpsys_init_flag = true;
	for (i = 0; i < num; i++) {
		struct scp_domain *scpd = &scp->domains[i];
		struct generic_pm_domain *genpd = &scpd->genpd;
		bool on;

		/*
		 * Initially turn on all domains to make the domains usable
		 * with !CONFIG_PM and to get the hardware in sync with the
		 * software.  The unused domains will be switched off during
		 * late_init time.
		 */
		if (MTK_SCPD_CAPS(scpd, MTK_SCPD_KEEP_DEFAULT_OFF)) {
			if (MTK_SCPD_CAPS(scpd, MTK_SCPD_IS_PWR_CON_ON))
				scpd->boot_status =
					scpsys_pwr_ack_is_on(scpd) &&
					scpsys_pwr_ack_2nd_is_on(scpd);
			else
				scpd->boot_status = scpsys_sta_is_on(scpd) &&
						    scpsys_sta_2nd_is_on(scpd);

			if (scpd->boot_status)
				on = !WARN_ON(genpd->power_on(genpd) < 0);
			else
				on = false;
		} else if (MTK_SCPD_CAPS(scpd, MTK_SCPD_BYPASS_INIT_ON)) {
			on = false;
		} else {
			on = !WARN_ON(genpd->power_on(genpd) < 0);
		}

		pm_genpd_init(genpd, NULL, !on);
	}

	scpsys_init_flag = false;

	/*
	 * We are not allowed to fail here since there is no way to unregister
	 * a power domain. Once registered above we have to keep the domains
	 * valid.
	 */

	pd_data = &scp->pd_data;

	ret = of_genpd_add_provider_onecell(pdev->dev.of_node, pd_data);
	if (ret)
		dev_err(&pdev->dev, "Failed to add OF provider: %d\n", ret);

	return ret;
}
EXPORT_SYMBOL(mtk_register_power_domains);

/*
 * MT2701 power domain support
 */

static const struct scp_domain_data scp_domain_data_mt2701[] = {
	[MT2701_POWER_DOMAIN_CONN] = {
		.name = "conn",
		.sta_mask = PWR_STATUS_CONN,
		.ctl_offs = SPM_CONN_PWR_CON,
		.bp_table = {
			BUS_PROT(IFR_TYPE, 0, 0, 0x220, 0x228,
				MT2701_TOP_AXI_PROT_EN_CONN_M |
				MT2701_TOP_AXI_PROT_EN_CONN_S),
		},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT2701_POWER_DOMAIN_DISP] = {
		.name = "disp",
		.sta_mask = PWR_STATUS_DISP,
		.ctl_offs = SPM_DIS_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.basic_clk_name = {"mm"},
		.bp_table = {
			BUS_PROT(IFR_TYPE, 0, 0, 0x220, 0x228,
				MT2701_TOP_AXI_PROT_EN_MM_M0),
		},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT2701_POWER_DOMAIN_MFG] = {
		.name = "mfg",
		.sta_mask = PWR_STATUS_MFG,
		.ctl_offs = SPM_MFG_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"mfg"},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT2701_POWER_DOMAIN_VDEC] = {
		.name = "vdec",
		.sta_mask = PWR_STATUS_VDEC,
		.ctl_offs = SPM_VDE_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"mm"},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT2701_POWER_DOMAIN_ISP] = {
		.name = "isp",
		.sta_mask = PWR_STATUS_ISP,
		.ctl_offs = SPM_ISP_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(13, 12),
		.basic_clk_name = {"mm"},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT2701_POWER_DOMAIN_BDP] = {
		.name = "bdp",
		.sta_mask = PWR_STATUS_BDP,
		.ctl_offs = SPM_BDP_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT2701_POWER_DOMAIN_ETH] = {
		.name = "eth",
		.sta_mask = PWR_STATUS_ETH,
		.ctl_offs = SPM_ETH_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
		.basic_clk_name = {"ethif"},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT2701_POWER_DOMAIN_HIF] = {
		.name = "hif",
		.sta_mask = PWR_STATUS_HIF,
		.ctl_offs = SPM_HIF_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
		.basic_clk_name = {"ethif"},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT2701_POWER_DOMAIN_IFR_MSC] = {
		.name = "ifr_msc",
		.sta_mask = PWR_STATUS_IFR_MSC,
		.ctl_offs = SPM_IFR_MSC_PWR_CON,
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
};

/*
 * MT2712 power domain support
 */
static const struct scp_domain_data scp_domain_data_mt2712[] = {
	[MT2712_POWER_DOMAIN_MM] = {
		.name = "mm",
		.sta_mask = PWR_STATUS_DISP,
		.ctl_offs = SPM_DIS_PWR_CON,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"mm"},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT2712_POWER_DOMAIN_VDEC] = {
		.name = "vdec",
		.sta_mask = PWR_STATUS_VDEC,
		.ctl_offs = SPM_VDE_PWR_CON,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"mm", "vdec"},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT2712_POWER_DOMAIN_VENC] = {
		.name = "venc",
		.sta_mask = PWR_STATUS_VENC,
		.ctl_offs = SPM_VEN_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
		.basic_clk_name = {"mm", "venc", "jpgdec"},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT2712_POWER_DOMAIN_ISP] = {
		.name = "isp",
		.sta_mask = PWR_STATUS_ISP,
		.ctl_offs = SPM_ISP_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(13, 12),
		.basic_clk_name = {"mm"},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT2712_POWER_DOMAIN_AUDIO] = {
		.name = "audio",
		.sta_mask = PWR_STATUS_AUDIO,
		.ctl_offs = SPM_AUDIO_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
		.basic_clk_name = {"audio"},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT2712_POWER_DOMAIN_USB] = {
		.name = "usb",
		.sta_mask = PWR_STATUS_USB,
		.ctl_offs = SPM_USB_PWR_CON,
		.sram_pdn_bits = GENMASK(10, 8),
		.sram_pdn_ack_bits = GENMASK(14, 12),
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT2712_POWER_DOMAIN_USB2] = {
		.name = "usb2",
		.sta_mask = PWR_STATUS_USB2,
		.ctl_offs = SPM_USB2_PWR_CON,
		.sram_pdn_bits = GENMASK(10, 8),
		.sram_pdn_ack_bits = GENMASK(14, 12),
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT2712_POWER_DOMAIN_MFG] = {
		.name = "mfg",
		.sta_mask = PWR_STATUS_MFG,
		.ctl_offs = SPM_MFG_PWR_CON,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(16, 16),
		.basic_clk_name = {"mfg"},
		.bp_table = {
			BUS_PROT(IFR_TYPE, 0x260, 0x264, 0x220, 0x228,
				BIT(14) | BIT(21) | BIT(23)),
		},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT2712_POWER_DOMAIN_MFG_SC1] = {
		.name = "mfg_sc1",
		.sta_mask = BIT(22),
		.ctl_offs = 0x02c0,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(16, 16),
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT2712_POWER_DOMAIN_MFG_SC2] = {
		.name = "mfg_sc2",
		.sta_mask = BIT(23),
		.ctl_offs = 0x02c4,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(16, 16),
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT2712_POWER_DOMAIN_MFG_SC3] = {
		.name = "mfg_sc3",
		.sta_mask = BIT(30),
		.ctl_offs = 0x01f8,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(16, 16),
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
};

static const struct scp_subdomain scp_subdomain_mt2712[] = {
	{MT2712_POWER_DOMAIN_MM, MT2712_POWER_DOMAIN_VDEC},
	{MT2712_POWER_DOMAIN_MM, MT2712_POWER_DOMAIN_VENC},
	{MT2712_POWER_DOMAIN_MM, MT2712_POWER_DOMAIN_ISP},
	{MT2712_POWER_DOMAIN_MFG, MT2712_POWER_DOMAIN_MFG_SC1},
	{MT2712_POWER_DOMAIN_MFG_SC1, MT2712_POWER_DOMAIN_MFG_SC2},
	{MT2712_POWER_DOMAIN_MFG_SC2, MT2712_POWER_DOMAIN_MFG_SC3},
};

/*
 * MT6797 power domain support
 */

static const struct scp_domain_data scp_domain_data_mt6797[] = {
	[MT6797_POWER_DOMAIN_VDEC] = {
		.name = "vdec",
		.sta_mask = BIT(7),
		.ctl_offs = 0x300,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"vdec"},
	},
	[MT6797_POWER_DOMAIN_VENC] = {
		.name = "venc",
		.sta_mask = BIT(21),
		.ctl_offs = 0x304,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
	},
	[MT6797_POWER_DOMAIN_ISP] = {
		.name = "isp",
		.sta_mask = BIT(5),
		.ctl_offs = 0x308,
		.sram_pdn_bits = GENMASK(9, 8),
		.sram_pdn_ack_bits = GENMASK(13, 12),
	},
	[MT6797_POWER_DOMAIN_MM] = {
		.name = "mm",
		.sta_mask = BIT(3),
		.ctl_offs = 0x30C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"mm"},
		.bp_table = {
			BUS_PROT(IFR_TYPE, 0, 0, 0x220, 0x228,
				BIT(1) | BIT(2)),
		},
	},
	[MT6797_POWER_DOMAIN_AUDIO] = {
		.name = "audio",
		.sta_mask = BIT(24),
		.ctl_offs = 0x314,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
	},
	[MT6797_POWER_DOMAIN_MFG_ASYNC] = {
		.name = "mfg_async",
		.sta_mask = BIT(13),
		.ctl_offs = 0x334,
		.sram_pdn_bits = 0,
		.sram_pdn_ack_bits = 0,
		.basic_clk_name = {"mfg"},
	},
	[MT6797_POWER_DOMAIN_MJC] = {
		.name = "mjc",
		.sta_mask = BIT(20),
		.ctl_offs = 0x310,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
	},
};

#define SPM_PWR_STATUS_MT6797		0x0180
#define SPM_PWR_STATUS_2ND_MT6797	0x0184

static const struct scp_subdomain scp_subdomain_mt6797[] = {
	{MT6797_POWER_DOMAIN_MM, MT6797_POWER_DOMAIN_VDEC},
	{MT6797_POWER_DOMAIN_MM, MT6797_POWER_DOMAIN_ISP},
	{MT6797_POWER_DOMAIN_MM, MT6797_POWER_DOMAIN_VENC},
	{MT6797_POWER_DOMAIN_MM, MT6797_POWER_DOMAIN_MJC},
};

/*
 * MT7622 power domain support
 */

static const struct scp_domain_data scp_domain_data_mt7622[] = {
	[MT7622_POWER_DOMAIN_ETHSYS] = {
		.name = "ethsys",
		.sta_mask = PWR_STATUS_ETHSYS,
		.ctl_offs = SPM_ETHSYS_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
		.bp_table = {
			BUS_PROT(IFR_TYPE, 0, 0, 0x220, 0x228,
				MT7622_TOP_AXI_PROT_EN_ETHSYS),
		},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT7622_POWER_DOMAIN_HIF0] = {
		.name = "hif0",
		.sta_mask = PWR_STATUS_HIF0,
		.ctl_offs = SPM_HIF0_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
		.basic_clk_name = {"hif_sel"},
		.bp_table = {
			BUS_PROT(IFR_TYPE, 0, 0, 0x220, 0x228,
				MT7622_TOP_AXI_PROT_EN_HIF0),
		},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT7622_POWER_DOMAIN_HIF1] = {
		.name = "hif1",
		.sta_mask = PWR_STATUS_HIF1,
		.ctl_offs = SPM_HIF1_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
		.basic_clk_name = {"hif_sel"},
		.bp_table = {
			BUS_PROT(IFR_TYPE, 0, 0, 0x220, 0x228,
				MT7622_TOP_AXI_PROT_EN_HIF1),
		},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT7622_POWER_DOMAIN_WB] = {
		.name = "wb",
		.sta_mask = PWR_STATUS_WB,
		.ctl_offs = SPM_WB_PWR_CON,
		.sram_pdn_bits = 0,
		.sram_pdn_ack_bits = 0,
		.bp_table = {
			BUS_PROT(IFR_TYPE, 0, 0, 0x220, 0x228,
				MT7622_TOP_AXI_PROT_EN_WB),
		},
		.caps = MTK_SCPD_ACTIVE_WAKEUP | MTK_SCPD_FWAIT_SRAM,
	},
};

/*
 * MT7623A power domain support
 */

static const struct scp_domain_data scp_domain_data_mt7623a[] = {
	[MT7623A_POWER_DOMAIN_CONN] = {
		.name = "conn",
		.sta_mask = PWR_STATUS_CONN,
		.ctl_offs = SPM_CONN_PWR_CON,
		.bp_table = {
			BUS_PROT(IFR_TYPE, 0, 0, 0x220, 0x228,
				MT2701_TOP_AXI_PROT_EN_CONN_M |
				MT2701_TOP_AXI_PROT_EN_CONN_S),
		},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT7623A_POWER_DOMAIN_ETH] = {
		.name = "eth",
		.sta_mask = PWR_STATUS_ETH,
		.ctl_offs = SPM_ETH_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
		.basic_clk_name = {"ethif"},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT7623A_POWER_DOMAIN_HIF] = {
		.name = "hif",
		.sta_mask = PWR_STATUS_HIF,
		.ctl_offs = SPM_HIF_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
		.basic_clk_name = {"ethif"},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT7623A_POWER_DOMAIN_IFR_MSC] = {
		.name = "ifr_msc",
		.sta_mask = PWR_STATUS_IFR_MSC,
		.ctl_offs = SPM_IFR_MSC_PWR_CON,
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
};

/*
 * MT8173 power domain support
 */

static const struct scp_domain_data scp_domain_data_mt8173[] = {
	[MT8173_POWER_DOMAIN_VDEC] = {
		.name = "vdec",
		.sta_mask = PWR_STATUS_VDEC,
		.ctl_offs = SPM_VDE_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"mm"},
	},
	[MT8173_POWER_DOMAIN_VENC] = {
		.name = "venc",
		.sta_mask = PWR_STATUS_VENC,
		.ctl_offs = SPM_VEN_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
		.basic_clk_name = {"mm", "venc"},
	},
	[MT8173_POWER_DOMAIN_ISP] = {
		.name = "isp",
		.sta_mask = PWR_STATUS_ISP,
		.ctl_offs = SPM_ISP_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(13, 12),
		.basic_clk_name = {"mm"},
	},
	[MT8173_POWER_DOMAIN_MM] = {
		.name = "mm",
		.sta_mask = PWR_STATUS_DISP,
		.ctl_offs = SPM_DIS_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"mm"},
		.bp_table = {
			BUS_PROT(IFR_TYPE, 0, 0, 0x220, 0x228,
				MT8173_TOP_AXI_PROT_EN_MM_M0 |
				MT8173_TOP_AXI_PROT_EN_MM_M1),
		},
	},
	[MT8173_POWER_DOMAIN_VENC_LT] = {
		.name = "venc_lt",
		.sta_mask = PWR_STATUS_VENC_LT,
		.ctl_offs = SPM_VEN2_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
		.basic_clk_name = {"mm", "venc_lt"},
	},
	[MT8173_POWER_DOMAIN_AUDIO] = {
		.name = "audio",
		.sta_mask = PWR_STATUS_AUDIO,
		.ctl_offs = SPM_AUDIO_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
	},
	[MT8173_POWER_DOMAIN_USB] = {
		.name = "usb",
		.sta_mask = PWR_STATUS_USB,
		.ctl_offs = SPM_USB_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT8173_POWER_DOMAIN_MFG_ASYNC] = {
		.name = "mfg_async",
		.sta_mask = PWR_STATUS_MFG_ASYNC,
		.ctl_offs = SPM_MFG_ASYNC_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = 0,
		.basic_clk_name = {"mfg"},
	},
	[MT8173_POWER_DOMAIN_MFG_2D] = {
		.name = "mfg_2d",
		.sta_mask = PWR_STATUS_MFG_2D,
		.ctl_offs = SPM_MFG_2D_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(13, 12),
	},
	[MT8173_POWER_DOMAIN_MFG] = {
		.name = "mfg",
		.sta_mask = PWR_STATUS_MFG,
		.ctl_offs = SPM_MFG_PWR_CON,
		.sram_pdn_bits = GENMASK(13, 8),
		.sram_pdn_ack_bits = GENMASK(21, 16),
		.bp_table = {
			BUS_PROT(IFR_TYPE, 0, 0, 0x220, 0x228,
				MT8173_TOP_AXI_PROT_EN_MFG_S |
				MT8173_TOP_AXI_PROT_EN_MFG_M0 |
				MT8173_TOP_AXI_PROT_EN_MFG_M1 |
				MT8173_TOP_AXI_PROT_EN_MFG_SNOOP_OUT),
		},
	},
};

static const struct scp_subdomain scp_subdomain_mt8173[] = {
	{MT8173_POWER_DOMAIN_MFG_ASYNC, MT8173_POWER_DOMAIN_MFG_2D},
	{MT8173_POWER_DOMAIN_MFG_2D, MT8173_POWER_DOMAIN_MFG},
};

static const struct scp_soc_data mt2701_data = {
	.domains = scp_domain_data_mt2701,
	.num_domains = ARRAY_SIZE(scp_domain_data_mt2701),
	.regs = {
		.pwr_sta_offs = SPM_PWR_STATUS,
		.pwr_sta2nd_offs = SPM_PWR_STATUS_2ND
	},
	.bus_prot_reg_update = true,
};

static const struct scp_soc_data mt2712_data = {
	.domains = scp_domain_data_mt2712,
	.num_domains = ARRAY_SIZE(scp_domain_data_mt2712),
	.subdomains = scp_subdomain_mt2712,
	.num_subdomains = ARRAY_SIZE(scp_subdomain_mt2712),
	.regs = {
		.pwr_sta_offs = SPM_PWR_STATUS,
		.pwr_sta2nd_offs = SPM_PWR_STATUS_2ND
	},
	.bus_prot_reg_update = false,
};

static const struct scp_soc_data mt6797_data = {
	.domains = scp_domain_data_mt6797,
	.num_domains = ARRAY_SIZE(scp_domain_data_mt6797),
	.subdomains = scp_subdomain_mt6797,
	.num_subdomains = ARRAY_SIZE(scp_subdomain_mt6797),
	.regs = {
		.pwr_sta_offs = SPM_PWR_STATUS_MT6797,
		.pwr_sta2nd_offs = SPM_PWR_STATUS_2ND_MT6797
	},
	.bus_prot_reg_update = true,
};

static const struct scp_soc_data mt7622_data = {
	.domains = scp_domain_data_mt7622,
	.num_domains = ARRAY_SIZE(scp_domain_data_mt7622),
	.regs = {
		.pwr_sta_offs = SPM_PWR_STATUS,
		.pwr_sta2nd_offs = SPM_PWR_STATUS_2ND
	},
	.bus_prot_reg_update = true,
};

static const struct scp_soc_data mt7623a_data = {
	.domains = scp_domain_data_mt7623a,
	.num_domains = ARRAY_SIZE(scp_domain_data_mt7623a),
	.regs = {
		.pwr_sta_offs = SPM_PWR_STATUS,
		.pwr_sta2nd_offs = SPM_PWR_STATUS_2ND
	},
	.bus_prot_reg_update = true,
};

static const struct scp_soc_data mt8173_data = {
	.domains = scp_domain_data_mt8173,
	.num_domains = ARRAY_SIZE(scp_domain_data_mt8173),
	.subdomains = scp_subdomain_mt8173,
	.num_subdomains = ARRAY_SIZE(scp_subdomain_mt8173),
	.regs = {
		.pwr_sta_offs = SPM_PWR_STATUS,
		.pwr_sta2nd_offs = SPM_PWR_STATUS_2ND
	},
	.bus_prot_reg_update = true,
};

/*
 * scpsys driver init
 */

static const struct of_device_id of_scpsys_match_tbl[] = {
	{
		.compatible = "mediatek,mt2701-scpsys",
		.data = &mt2701_data,
	}, {
		.compatible = "mediatek,mt2712-scpsys",
		.data = &mt2712_data,
	}, {
		.compatible = "mediatek,mt6797-scpsys",
		.data = &mt6797_data,
	}, {
		.compatible = "mediatek,mt7622-scpsys",
		.data = &mt7622_data,
	}, {
		.compatible = "mediatek,mt7623a-scpsys",
		.data = &mt7623a_data,
	}, {
		.compatible = "mediatek,mt8173-scpsys",
		.data = &mt8173_data,
	}, {
		/* sentinel */
	}
};

static int scpsys_probe(struct platform_device *pdev)
{
	const struct scp_subdomain *sd;
	const struct scp_soc_data *soc;
	struct scp *scp;
	struct genpd_onecell_data *pd_data;
	int i, ret;

	soc = of_device_get_match_data(&pdev->dev);

	scp = init_scp(pdev, soc->domains, soc->num_domains, &soc->regs,
			soc->bus_prot_reg_update, bus_list, BUS_TYPE_NUM);
	if (IS_ERR(scp))
		return PTR_ERR(scp);

	mtk_register_power_domains(pdev, scp, soc->num_domains);

	pd_data = &scp->pd_data;

	for (i = 0, sd = soc->subdomains; i < soc->num_subdomains; i++, sd++) {
		ret = pm_genpd_add_subdomain(pd_data->domains[sd->origin],
					     pd_data->domains[sd->subdomain]);
		if (ret && IS_ENABLED(CONFIG_PM))
			dev_err(&pdev->dev, "Failed to add subdomain: %d\n",
				ret);
	}

	return 0;
}

static struct platform_driver scpsys_drv = {
	.probe = scpsys_probe,
	.driver = {
		.name = "mtk-scpsys",
		.suppress_bind_attrs = true,
		.owner = THIS_MODULE,
		.of_match_table = of_scpsys_match_tbl,
	},
};
builtin_platform_driver(scpsys_drv);
