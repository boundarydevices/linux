// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2017,2018,2021 NXP
 * Copyright 2019 Purism SPC
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/rational.h>
#include <linux/regmap.h>

/* DPHY registers */
#define DPHY_PD_DPHY			0x00
#define DPHY_M_PRG_HS_PREPARE		0x04
#define DPHY_MC_PRG_HS_PREPARE		0x08
#define DPHY_M_PRG_HS_ZERO		0x0c
#define DPHY_MC_PRG_HS_ZERO		0x10
#define DPHY_M_PRG_HS_TRAIL		0x14
#define DPHY_MC_PRG_HS_TRAIL		0x18
#define DPHY_REG_BYPASS_PLL		0x4C

#define MBPS(x) ((x) * 1000000)

#define DATA_RATE_MAX_SPEED MBPS(1500)
#define DATA_RATE_MIN_SPEED MBPS(80)

#define PLL_LOCK_SLEEP 10
#define PLL_LOCK_TIMEOUT 1000

#define CN_BUF	0xcb7a89c0
#define CO_BUF	0x63
#define CM(x)	(				  \
		((x) <	32) ? 0xe0 | ((x) - 16) : \
		((x) <	64) ? 0xc0 | ((x) - 32) : \
		((x) < 128) ? 0x80 | ((x) - 64) : \
		((x) - 128))
#define CN(x)	(((x) == 1) ? 0x1f : (((CN_BUF) >> ((x) - 1)) & 0x1f))
#define CO(x)	((CO_BUF) >> (8 - (x)) & 0x03)

/* PHY power on is active low */
#define PWR_ON	0
#define PWR_OFF	1

/* not available register */
#define REG_NA	0xff

enum mixel_dphy_devtype {
	MIXEL_IMX8MQ,
	MIXEL_IMX8QM,
	MIXEL_IMX8QX,
	MIXEL_IMX8ULP,
};

struct mixel_dphy_devdata {
	u8 reg_mc_prg_rxhs_settle;
	u8 reg_m_prg_rxhs_settle;
	u8 reg_pd_pll;
	u8 reg_tst;
	u8 reg_cn;
	u8 reg_cm;
	u8 reg_co;
	u8 reg_lock;
	u8 reg_lock_byp;
	u8 reg_tx_rcal;
	u8 reg_auto_pd_en;
	u8 reg_rxlprp;
	u8 reg_rxcdrp;
	u8 reg_rxhs_settle;
};

static const struct mixel_dphy_devdata mixel_dphy_devdata[] = {
	[MIXEL_IMX8MQ] = {
		.reg_mc_prg_rxhs_settle = REG_NA,
		.reg_m_prg_rxhs_settle = REG_NA,
		.reg_pd_pll = 0x1c,
		.reg_tst = 0x20,
		.reg_cn = 0x24,
		.reg_cm = 0x28,
		.reg_co = 0x2c,
		.reg_lock = 0x30,
		.reg_lock_byp = 0x34,
		.reg_tx_rcal = 0x38,
		.reg_auto_pd_en = 0x3c,
		.reg_rxlprp = 0x40,
		.reg_rxcdrp = 0x44,
		.reg_rxhs_settle = 0x48,
	},
	[MIXEL_IMX8QM] = {
		.reg_mc_prg_rxhs_settle = REG_NA,
		.reg_m_prg_rxhs_settle = REG_NA,
		.reg_pd_pll = 0x1c,
		.reg_tst = 0x20,
		.reg_cn = 0x24,
		.reg_cm = 0x28,
		.reg_co = 0x2c,
		.reg_lock = 0x30,
		.reg_lock_byp = 0x34,
		.reg_tx_rcal = 0x00,
		.reg_auto_pd_en = 0x38,
		.reg_rxlprp = 0x3c,
		.reg_rxcdrp = 0x40,
		.reg_rxhs_settle = 0x44,
	},
	[MIXEL_IMX8QX] = {
		.reg_mc_prg_rxhs_settle = REG_NA,
		.reg_m_prg_rxhs_settle = REG_NA,
		.reg_pd_pll = 0x1c,
		.reg_tst = 0x20,
		.reg_cn = 0x24,
		.reg_cm = 0x28,
		.reg_co = 0x2c,
		.reg_lock = 0x30,
		.reg_lock_byp = 0x34,
		.reg_tx_rcal = 0x00,
		.reg_auto_pd_en = 0x38,
		.reg_rxlprp = 0x3c,
		.reg_rxcdrp = 0x40,
		.reg_rxhs_settle = 0x44,
	},
	[MIXEL_IMX8ULP] = {
		.reg_mc_prg_rxhs_settle = 0x1c,
		.reg_m_prg_rxhs_settle = 0x20,
		.reg_pd_pll = 0x24,
		.reg_tst = 0x28,
		.reg_cn = 0x2c,
		.reg_cm = 0x30,
		.reg_co = 0x34,
		.reg_lock = 0x38,
		.reg_lock_byp = 0x3c,
		.reg_tx_rcal = 0x00,
		.reg_auto_pd_en = 0x40,
		.reg_rxlprp = 0x44,
		.reg_rxcdrp = 0x48,
		.reg_rxhs_settle = REG_NA,
	},
};

struct mixel_dphy_cfg {
	/* DPHY PLL parameters */
	u32 cm;
	u32 cn;
	u32 co;
	/* DPHY register values */
	u8 mc_prg_hs_prepare;
	u8 m_prg_hs_prepare;
	u8 mc_prg_hs_zero;
	u8 m_prg_hs_zero;
	u8 mc_prg_hs_trail;
	u8 m_prg_hs_trail;
	u8 rxhs_settle;
};

struct mixel_dphy_priv {
	struct mixel_dphy_cfg cfg;
	struct regmap *regmap;
	struct clk *phy_ref_clk;
	struct clk_hw	dsi_clk;
	unsigned long	frequency;
	const struct mixel_dphy_devdata *devdata;
};

static const struct regmap_config mixel_dphy_regmap_config = {
	.reg_bits = 8,
	.val_bits = 32,
	.reg_stride = 4,
	.max_register = DPHY_REG_BYPASS_PLL,
	.name = "mipi-dphy",
};

static int phy_write(struct phy *phy, u32 value, unsigned int reg)
{
	struct mixel_dphy_priv *priv = phy_get_drvdata(phy);
	int ret;

	ret = regmap_write(priv->regmap, reg, value);
	if (ret < 0)
		dev_err(&phy->dev, "Failed to write DPHY reg %d: %d\n", reg,
			ret);
	return ret;
}

#ifdef CONFIG_COMMON_CLK
#define dsi_clk_to_data(_hw) container_of(_hw, struct mixel_dphy_priv, dsi_clk)

static unsigned long mixel_dsi_clk_recalc_rate(struct clk_hw *hw,
					    unsigned long parent_rate)
{
	return dsi_clk_to_data(hw)->frequency;
}

static long mixel_dsi_clk_round_rate(struct clk_hw *hw, unsigned long rate,
				  unsigned long *prate)
{
	return dsi_clk_to_data(hw)->frequency;
}

static int mixel_dsi_clk_set_rate(struct clk_hw *hw, unsigned long rate,
			       unsigned long parent_rate)
{
	return 0;
}

static int mixel_dsi_clk_prepare(struct clk_hw *hw)
{
	return 0;
}

static void mixel_dsi_clk_unprepare(struct clk_hw *hw)
{
	return;
}

static int mixel_dsi_clk_is_prepared(struct clk_hw *hw)
{
	struct mixel_dphy_priv *priv = dsi_clk_to_data(hw);

	if (priv->cfg.cm < 16 || priv->cfg.cm > 255 ||
	    priv->cfg.cn < 1 || priv->cfg.cn > 32 ||
	    priv->cfg.co < 1 || priv->cfg.co > 8)
		return 0;
	return 1;
}

static const struct clk_ops mixel_dsi_clk_ops = {
	.prepare = mixel_dsi_clk_prepare,
	.unprepare = mixel_dsi_clk_unprepare,
	.is_prepared = mixel_dsi_clk_is_prepared,
	.recalc_rate = mixel_dsi_clk_recalc_rate,
	.round_rate = mixel_dsi_clk_round_rate,
	.set_rate = mixel_dsi_clk_set_rate,
};

static struct clk *mixel_dsi_clk_register_clk(struct mixel_dphy_priv *priv, struct device *dev)
{
	struct clk *clk;
	struct clk_init_data init;

	init.name = "mixel-dsi-clk";
	init.ops = &mixel_dsi_clk_ops;
	init.flags = 0;
	init.parent_names = NULL;
	init.num_parents = 0;
	priv->dsi_clk.init = &init;

	/* optional override of the clockname */
	of_property_read_string(dev->of_node, "clock-output-names", &init.name);

	/* register the clock */
	clk = clk_register(dev, &priv->dsi_clk);
	if (!IS_ERR(clk))
		of_clk_add_provider(dev->of_node, of_clk_src_simple_get, clk);

	return clk;
}
#endif

static int mixel_dphy_config_from_opts(struct phy *phy,
	       struct phy_configure_opts_mipi_dphy *dphy_opts,
	       struct mixel_dphy_cfg *cfg)
{
	struct mixel_dphy_priv *priv = dev_get_drvdata(phy->dev.parent);
	unsigned long ref_clk = clk_get_rate(priv->phy_ref_clk);
	u32 lp_t;
	unsigned long numerator, denominator;
	unsigned max_d = 256;
	unsigned long bit_clk;
	unsigned long long tmp;
	u32 n;
	int i = 0;

	bit_clk = dphy_opts->hs_clk_rate;
	if (bit_clk > DATA_RATE_MAX_SPEED ||
	    bit_clk < DATA_RATE_MIN_SPEED)
		return -EINVAL;

	/* CM ranges between 16 and 255 */
	/* CN ranges between 1 and 32 */
	/* CO is power of 2: 1, 2, 4, 8 */
	do {
		numerator = bit_clk << i;
		denominator = ref_clk;
		rational_best_ratio_bigger(&numerator, &denominator, 255, max_d >> i);
		denominator <<= i;
		i++;
	} while ((denominator >> __ffs(denominator)) > 32);

	if (!numerator || !denominator) {
		dev_err(&phy->dev, "Invalid %ld/%ld for %ld/%ld\n",
			numerator, denominator,
			bit_clk, ref_clk);
		return -EINVAL;
	}

	while ((numerator < 16) && (denominator <= 128)) {
		numerator <<= 1;
		denominator <<= 1;
	}
	/*
	 * CM ranges between 16 and 255
	 * CN ranges between 1 and 32
	 * CO is power of 2: 1, 2, 4, 8
	 */
	i = __ffs(denominator);
	if (i > 3)
		i = 3;
	cfg->cn = denominator >> i;
	cfg->co = 1 << i;
	cfg->cm = numerator;

	if (cfg->cm < 16 || cfg->cm > 255 ||
	    cfg->cn < 1 || cfg->cn > 32 ||
	    cfg->co < 1 || cfg->co > 8) {
		dev_err(&phy->dev, "Invalid CM/CN/CO values: %u/%u/%u\n",
			cfg->cm, cfg->cn, cfg->co);
		dev_err(&phy->dev, "for hs_clk/ref_clk=%ld/%ld ~ %ld/%ld\n",
			bit_clk, ref_clk,
			numerator, denominator);
		return -EINVAL;
	}

	dphy_opts->hs_clk_rate = ref_clk * cfg->cm / (cfg->co * cfg->cn);
	dev_dbg(&phy->dev, "hs_clk(%ld)/ref_clk=%ld/%ld ~ %ld/%ld\n",
			dphy_opts->hs_clk_rate, bit_clk, ref_clk,
			numerator, denominator);

	/* LP clock period */
	tmp = 1000000000000LL;
	do_div(tmp, dphy_opts->lp_clk_rate); /* ps */
	if (tmp > ULONG_MAX)
		return -EINVAL;

	lp_t = tmp;
	dev_dbg(&phy->dev, "LP clock %lu, period: %u ps\n",
		dphy_opts->lp_clk_rate, lp_t);

	/* hs_prepare: in lp clock periods */
	if (2 * dphy_opts->hs_prepare > 5 * lp_t) {
		dev_err(&phy->dev,
			"hs_prepare (%u) > 2.5 * lp clock period (%u)\n",
			dphy_opts->hs_prepare, lp_t);
		return -EINVAL;
	}
	/* 00: lp_t, 01: 1.5 * lp_t, 10: 2 * lp_t, 11: 2.5 * lp_t */
	tmp = 2 * (dphy_opts->hs_prepare) + lp_t - 1;
	do_div(tmp, lp_t);
	if (tmp > 2) {
		n = tmp - 2;
		if (n > 3)
			n = 3;
	} else {
		n = 0;
	}
	cfg->m_prg_hs_prepare = n;

	/* clk_prepare: in lp clock periods */
	if (2 * dphy_opts->clk_prepare > 3 * lp_t) {
		dev_err(&phy->dev,
			"clk_prepare (%u) > 1.5 * lp clock period (%u)\n",
			dphy_opts->clk_prepare, lp_t);
		return -EINVAL;
	}
	/* 00: lp_t, 01: 1.5 * lp_t */
	cfg->mc_prg_hs_prepare = dphy_opts->clk_prepare > lp_t ? 1 : 0;

	/* hs_zero: formula from NXP BSP */
	n = (144 * (bit_clk / 1000000) - 47500) / 10000;
	cfg->m_prg_hs_zero = n < 1 ? 1 : n;

	/* clk_zero: formula from NXP BSP */
	n = (34 * (bit_clk / 1000000) - 2500) / 1000;
	cfg->mc_prg_hs_zero = n < 1 ? 1 : n;

	/* clk_trail, hs_trail: formula from NXP BSP */
	n = (103 * (bit_clk / 1000000) + 10000) / 10000;
	if (n > 15)
		n = 15;
	if (n < 1)
		n = 1;
	cfg->m_prg_hs_trail = n;
	cfg->mc_prg_hs_trail = n;

	/* rxhs_settle: formula from NXP BSP */
	if (priv->devdata->reg_rxhs_settle != REG_NA) {
		if (bit_clk < MBPS(80))
			cfg->rxhs_settle = 0x0d;
		else if (bit_clk < MBPS(90))
			cfg->rxhs_settle = 0x0c;
		else if (bit_clk < MBPS(125))
			cfg->rxhs_settle = 0x0b;
		else if (bit_clk < MBPS(150))
			cfg->rxhs_settle = 0x0a;
		else if (bit_clk < MBPS(225))
			cfg->rxhs_settle = 0x09;
		else if (bit_clk < MBPS(500))
			cfg->rxhs_settle = 0x08;
		else
			cfg->rxhs_settle = 0x07;
	} else if (priv->devdata->reg_m_prg_rxhs_settle != REG_NA) {
		if (bit_clk < MBPS(80))
			cfg->rxhs_settle = 0x01;
		else if (bit_clk < MBPS(250))
			cfg->rxhs_settle = 0x06;
		else if (bit_clk < MBPS(500))
			cfg->rxhs_settle = 0x08;
		else
			cfg->rxhs_settle = 0x0a;
	}

	dev_dbg(&phy->dev, "phy_config: %u %u %u %u %u %u %u\n",
		cfg->m_prg_hs_prepare, cfg->mc_prg_hs_prepare,
		cfg->m_prg_hs_zero, cfg->mc_prg_hs_zero,
		cfg->m_prg_hs_trail, cfg->mc_prg_hs_trail,
		cfg->rxhs_settle);

	return 0;
}

static void mixel_phy_set_hs_timings(struct phy *phy)
{
	struct mixel_dphy_priv *priv = phy_get_drvdata(phy);

	phy_write(phy, priv->cfg.m_prg_hs_prepare, DPHY_M_PRG_HS_PREPARE);
	phy_write(phy, priv->cfg.mc_prg_hs_prepare, DPHY_MC_PRG_HS_PREPARE);
	phy_write(phy, priv->cfg.m_prg_hs_zero, DPHY_M_PRG_HS_ZERO);
	phy_write(phy, priv->cfg.mc_prg_hs_zero, DPHY_MC_PRG_HS_ZERO);
	phy_write(phy, priv->cfg.m_prg_hs_trail, DPHY_M_PRG_HS_TRAIL);
	phy_write(phy, priv->cfg.mc_prg_hs_trail, DPHY_MC_PRG_HS_TRAIL);
	if (priv->devdata->reg_rxhs_settle != REG_NA)
		phy_write(phy, priv->cfg.rxhs_settle,
			  priv->devdata->reg_rxhs_settle);
	else if (priv->devdata->reg_m_prg_rxhs_settle != REG_NA)
		phy_write(phy, priv->cfg.rxhs_settle,
			  priv->devdata->reg_m_prg_rxhs_settle);

	if (priv->devdata->reg_mc_prg_rxhs_settle != REG_NA)
		phy_write(phy, 0x10, priv->devdata->reg_mc_prg_rxhs_settle);
}

static int mixel_dphy_set_pll_params(struct phy *phy)
{
	struct mixel_dphy_priv *priv = dev_get_drvdata(phy->dev.parent);

	if (priv->cfg.cm < 16 || priv->cfg.cm > 255 ||
	    priv->cfg.cn < 1 || priv->cfg.cn > 32 ||
	    priv->cfg.co < 1 || priv->cfg.co > 8) {
		dev_err(&phy->dev, "Invalid CM/CN/CO values! (%u/%u/%u)\n",
			priv->cfg.cm, priv->cfg.cn, priv->cfg.co);
		return -EINVAL;
	}
	dev_dbg(&phy->dev, "Using CM:%u CN:%u CO:%u\n",
		priv->cfg.cm, priv->cfg.cn, priv->cfg.co);
	phy_write(phy, CM(priv->cfg.cm), priv->devdata->reg_cm);
	phy_write(phy, CN(priv->cfg.cn), priv->devdata->reg_cn);
	phy_write(phy, CO(priv->cfg.co), priv->devdata->reg_co);
	return 0;
}

static int mixel_dphy_configure(struct phy *phy, union phy_configure_opts *opts)
{
	struct mixel_dphy_priv *priv = phy_get_drvdata(phy);
	struct mixel_dphy_cfg cfg = { 0 };
	int ret;
	unsigned long ref_clk;

	ret = mixel_dphy_config_from_opts(phy, &opts->mipi_dphy, &cfg);
	if (ret)
		return ret;

	/* Update the configuration */
	memcpy(&priv->cfg, &cfg, sizeof(struct mixel_dphy_cfg));

	ref_clk = clk_get_rate(priv->phy_ref_clk);
	/* Divided by 2 because mipi output clock is DDR */
	priv->frequency = opts->mipi_dphy.hs_clk_rate >> 1;
	if (priv->dsi_clk.clk)
		clk_set_rate(priv->dsi_clk.clk, priv->frequency);
	dev_info(&phy->dev, "%s:%ld ref_clk=%ld, cm=%d, co=%d cn=%d\n",
		__func__, priv->frequency, ref_clk, cfg.cm, cfg.co, cfg.cn);

	phy_write(phy, 0x00, priv->devdata->reg_lock_byp);
	phy_write(phy, 0x01, priv->devdata->reg_tx_rcal);
	phy_write(phy, 0x00, priv->devdata->reg_auto_pd_en);
	phy_write(phy, 0x02, priv->devdata->reg_rxlprp);
	phy_write(phy, 0x02, priv->devdata->reg_rxcdrp);
	phy_write(phy, 0x25, priv->devdata->reg_tst);

	mixel_phy_set_hs_timings(phy);
	ret = mixel_dphy_set_pll_params(phy);
	if (ret < 0)
		return ret;

	return 0;
}

static int mixel_dphy_validate(struct phy *phy, enum phy_mode mode, int submode,
			       union phy_configure_opts *opts)
{
	struct mixel_dphy_cfg cfg = { 0 };

	if (mode != PHY_MODE_MIPI_DPHY)
		return -EINVAL;

	return mixel_dphy_config_from_opts(phy, &opts->mipi_dphy, &cfg);
}

static int mixel_dphy_init(struct phy *phy)
{
	struct mixel_dphy_priv *priv = phy_get_drvdata(phy);

	phy_write(phy, PWR_OFF, priv->devdata->reg_pd_pll);
	phy_write(phy, PWR_OFF, DPHY_PD_DPHY);

	return 0;
}

static int mixel_dphy_exit(struct phy *phy)
{
	struct mixel_dphy_priv *priv = phy_get_drvdata(phy);

	phy_write(phy, 0, priv->devdata->reg_cm);
	phy_write(phy, 0, priv->devdata->reg_cn);
	phy_write(phy, 0, priv->devdata->reg_co);

	return 0;
}

static int mixel_dphy_power_on(struct phy *phy)
{
	struct mixel_dphy_priv *priv = phy_get_drvdata(phy);
	u32 locked;
	int ret;

	ret = clk_prepare_enable(priv->phy_ref_clk);
	if (ret < 0)
		return ret;

	phy_write(phy, PWR_ON, priv->devdata->reg_pd_pll);
	ret = regmap_read_poll_timeout(priv->regmap, priv->devdata->reg_lock,
				       locked, locked, PLL_LOCK_SLEEP,
				       PLL_LOCK_TIMEOUT);
	if (ret < 0) {
		dev_err(&phy->dev, "Could not get DPHY lock (%d)!\n", ret);
		goto clock_disable;
	}
	phy_write(phy, PWR_ON, DPHY_PD_DPHY);

	return 0;
clock_disable:
	clk_disable_unprepare(priv->phy_ref_clk);
	return ret;
}

static int mixel_dphy_power_off(struct phy *phy)
{
	struct mixel_dphy_priv *priv = phy_get_drvdata(phy);

	phy_write(phy, PWR_OFF, priv->devdata->reg_pd_pll);
	phy_write(phy, PWR_OFF, DPHY_PD_DPHY);

	clk_disable_unprepare(priv->phy_ref_clk);

	return 0;
}

static const struct phy_ops mixel_dphy_phy_ops = {
	.init = mixel_dphy_init,
	.exit = mixel_dphy_exit,
	.power_on = mixel_dphy_power_on,
	.power_off = mixel_dphy_power_off,
	.configure = mixel_dphy_configure,
	.validate = mixel_dphy_validate,
	.owner = THIS_MODULE,
};

static const struct of_device_id mixel_dphy_of_match[] = {
	{ .compatible = "fsl,imx8mq-mipi-dphy",
	  .data = &mixel_dphy_devdata[MIXEL_IMX8MQ] },
	{ .compatible = "fsl,imx8qm-mipi-dphy",
	  .data = &mixel_dphy_devdata[MIXEL_IMX8QM] },
	{ .compatible = "fsl,imx8qx-mipi-dphy",
	  .data = &mixel_dphy_devdata[MIXEL_IMX8QX] },
	{ .compatible = "fsl,imx8ulp-mipi-dphy",
	  .data = &mixel_dphy_devdata[MIXEL_IMX8ULP] },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, mixel_dphy_of_match);

static int mixel_dphy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct phy_provider *phy_provider;
	struct mixel_dphy_priv *priv;
	struct phy *phy;
	void __iomem *base;

	if (!np)
		return -ENODEV;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->devdata = of_device_get_match_data(&pdev->dev);
	if (!priv->devdata)
		return -EINVAL;

	base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(base))
		return PTR_ERR(base);

	priv->regmap = devm_regmap_init_mmio(&pdev->dev, base,
					     &mixel_dphy_regmap_config);
	if (IS_ERR(priv->regmap)) {
		dev_err(dev, "Couldn't create the DPHY regmap\n");
		return PTR_ERR(priv->regmap);
	}

	priv->phy_ref_clk = devm_clk_get(&pdev->dev, "phy_ref");
	if (IS_ERR(priv->phy_ref_clk)) {
		dev_err(dev, "No phy_ref clock found\n");
		return PTR_ERR(priv->phy_ref_clk);
	}
	dev_dbg(dev, "phy_ref clock rate: %lu\n",
		clk_get_rate(priv->phy_ref_clk));

	dev_set_drvdata(dev, priv);

	phy = devm_phy_create(dev, np, &mixel_dphy_phy_ops);
	if (IS_ERR(phy)) {
		dev_err(dev, "Failed to create phy %ld\n", PTR_ERR(phy));
		return PTR_ERR(phy);
	}
	phy_set_drvdata(phy, priv);

	phy_provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);
#ifdef CONFIG_COMMON_CLK
	mixel_dsi_clk_register_clk(priv, dev);
#endif

	return PTR_ERR_OR_ZERO(phy_provider);
}

static struct platform_driver mixel_dphy_driver = {
	.probe	= mixel_dphy_probe,
	.driver = {
		.name = "mixel-mipi-dphy",
		.of_match_table	= mixel_dphy_of_match,
	}
};
module_platform_driver(mixel_dphy_driver);

MODULE_AUTHOR("NXP Semiconductor");
MODULE_DESCRIPTION("Mixel MIPI-DSI PHY driver");
MODULE_LICENSE("GPL");
