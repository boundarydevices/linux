// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2019 NXP.
 */

#include <linux/arm-smccc.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>

#include <soc/imx/imx_sip.h>

#define MAX_CLK_NUM	6
#define to_imx8m_pm_domain(_genpd) container_of(_genpd, struct imx8m_pm_domain, genpd)
#define genpd_status_on(genpd)		((genpd)->status == GENPD_STATE_ON)

struct imx8m_pm_domain {
	struct generic_pm_domain genpd;
	struct device *dev;
	u32 domain_index;
	struct clk *clk[MAX_CLK_NUM];
	unsigned int num_clks;
	struct regulator *regulator;

	struct regulator *dvfs_reg;
	struct device_node *opp_np[MAX_CLK_NUM];
	unsigned long u_volt;
	unsigned long idle_uv;
	unsigned long idle_uv_min;
	unsigned long idle_uv_max;
};

enum imx8m_pm_domain_state {
	PD_STATE_OFF,
	PD_STATE_ON,
};

static DEFINE_MUTEX(gpc_pd_mutex);

struct uv_freq {
	unsigned long uv;
	unsigned long uv_min;
	unsigned long uv_max;
	unsigned long freq;
};

static void get_max_uv(struct imx8m_pm_domain *pd, struct uv_freq *uvf)
{
	struct clk **clks = &pd->clk[0];
	int i = 0;
	struct dev_pm_opp *opp;

	if (!pd->num_clks)
		return;

	while (clks[i]) {
		struct uv_freq uv = {
			.uv = 0,
			.uv_min = 0,
			.uv_max = 0,
			.freq = 0,
		};

		if (pd->opp_np[i]) {
			uv.freq = clk_get_rate(clks[i]);
			opp = dev_pm_opp_find_freq_ceil_np(&pd->genpd.dev, pd->opp_np[i],
				&uv.freq);
			if (!IS_ERR(opp) && opp) {
				opp_return_volts(opp, &uv.uv, &uv.uv_min, &uv.uv_max);
				dev_dbg(&pd->genpd.dev, "%s: voltage=%ld freq=%ld, clk=%s\n", __func__,
					uv.uv, uv.freq, __clk_get_name(clks[i]));
				if (uvf->uv < uv.uv)
					*uvf = uv;
			} else {
				dev_err(&pd->genpd.dev, "%s: ceil failed, i=%d, freq=%ld, clk=%s\n", __func__,
					i, uv.freq, __clk_get_name(clks[i]));
			}
		}
		i++;
		if (i >= pd->num_clks)
			break;
	}
}

static void rescan_voltage(struct imx8m_pm_domain *parent, struct generic_pm_domain *skip)
{
	struct generic_pm_domain *genpd = &parent->genpd;
	struct generic_pm_domain *child_pd;
	struct gpd_link *link;
	struct uv_freq u_volt = {
		.uv = 0,
		.uv_min = 0,
		.uv_max = 0,
		.freq = 0,
	};
	int ret;

	if (!parent->dvfs_reg)
		return;

	list_for_each_entry(link, &genpd->parent_links, parent_node) {
		child_pd = link->child;

		if ((child_pd != skip) && genpd_status_on(child_pd))
			get_max_uv(to_imx8m_pm_domain(child_pd), &u_volt);
	}
	get_max_uv(parent, &u_volt);

	if (u_volt.uv && (parent->u_volt != u_volt.uv)) {
		parent->u_volt = u_volt.uv;
		ret = regulator_set_voltage_triplet(parent->dvfs_reg,
				u_volt.uv_min,
				u_volt.uv,
				u_volt.uv_max);
		dev_dbg(&genpd->dev, "%s: voltage=%ld freq=%ld\n", __func__,
				u_volt.uv, u_volt.freq);
	}
}

static void increase_voltage(struct imx8m_pm_domain *pd, struct uv_freq *uvf)
{
	int ret;

	if (!pd->dvfs_reg)
		return;

	if (pd->u_volt < uvf->uv) {
		dev_dbg(&pd->genpd.dev, "%s: was %lduV\n", __func__, pd->u_volt);
		pd->u_volt = uvf->uv;
		ret = regulator_set_voltage_triplet(pd->dvfs_reg,
				uvf->uv_min,
				uvf->uv,
				uvf->uv_max);
		dev_dbg(&pd->genpd.dev, "%s: voltage=%ld %ld %ld freq=%ld\n", __func__,
			uvf->uv_min, uvf->uv, uvf->uv_max, uvf->freq);
	}
}

static void check_voltage(struct generic_pm_domain *genpd)
{
	struct imx8m_pm_domain *pd = to_imx8m_pm_domain(genpd);
	struct gpd_link *link;
	struct uv_freq u_volt = {
		.uv = 0,
		.uv_min = 0,
		.uv_max = 0,
		.freq = 0,
	};

	get_max_uv(pd, &u_volt);
	increase_voltage(pd, &u_volt);

	list_for_each_entry(link, &genpd->child_links, child_node) {
		struct generic_pm_domain *pd_parent = link->parent;
		struct imx8m_pm_domain *parent;

		parent = to_imx8m_pm_domain(pd_parent);
		increase_voltage(parent, &u_volt);
	}
}

static int imx8m_pd_power_on(struct generic_pm_domain *genpd)
{
	struct imx8m_pm_domain *domain = to_imx8m_pm_domain(genpd);
	struct arm_smccc_res res;
	int index, ret = 0;

	/* power on the external supply */
	if (domain->regulator) {
		ret = regulator_enable(domain->regulator);
		if (ret) {
			dev_warn(domain->dev, "failed to power up the reg%d\n", ret);
			return ret;
		}
	}
	check_voltage(genpd);

	if (domain->dvfs_reg) {
		ret = regulator_enable(domain->dvfs_reg);
		if (ret) {
			dev_warn(&genpd->dev, "failed to power up the dvfs-reg(%d)\n", ret);
			return ret;
		}
	}

	/* enable the necessary clks needed by the power domain */
	if (domain->num_clks) {
		for (index = 0; index < domain->num_clks; index++)
			clk_prepare_enable(domain->clk[index]);
	}

	mutex_lock(&gpc_pd_mutex);
	arm_smccc_smc(IMX_SIP_GPC, IMX_SIP_CONFIG_GPC_PM_DOMAIN, domain->domain_index,
		      PD_STATE_ON, 0, 0, 0, 0, &res);
	mutex_unlock(&gpc_pd_mutex);

	return 0;
}

static int imx8m_pd_power_off(struct generic_pm_domain *genpd)
{
	struct imx8m_pm_domain *domain = to_imx8m_pm_domain(genpd);
	struct arm_smccc_res res;
	int index, ret = 0;

	mutex_lock(&gpc_pd_mutex);
	arm_smccc_smc(IMX_SIP_GPC, IMX_SIP_CONFIG_GPC_PM_DOMAIN, domain->domain_index,
		      PD_STATE_OFF, 0, 0, 0, 0, &res);
	mutex_unlock(&gpc_pd_mutex);

	if (domain->dvfs_reg) {
		ret = regulator_disable(domain->dvfs_reg);
		if (ret)
			dev_warn(&genpd->dev, "failed to power off the dvfs-reg(%d)\n", ret);
	}
	if (domain->opp_np[0]) {
		struct gpd_link *link;

		list_for_each_entry(link, &genpd->child_links, child_node) {
			struct generic_pm_domain *pd_parent = link->parent;
			struct imx8m_pm_domain *parent;

			parent = to_imx8m_pm_domain(pd_parent);
			rescan_voltage(parent, genpd);
		}
	}

	if ((domain->u_volt != domain->idle_uv) && domain->dvfs_reg && domain->idle_uv) {
		domain->u_volt = domain->idle_uv;
		ret = regulator_set_voltage_triplet(domain->dvfs_reg,
				domain->idle_uv_min,
				domain->idle_uv,
				domain->idle_uv_max);
		dev_dbg(&genpd->dev, "%s: voltage=%ld\n", __func__, domain->idle_uv);
	}
	/* power off the external supply */
	if (domain->regulator) {
		ret = regulator_disable(domain->regulator);
		if (ret) {
			dev_warn(domain->dev, "failed to power off the reg%d\n", ret);
			return ret;
		}
	}

	/* disable clks when power domain is off */
	if (domain->num_clks) {
		for (index = 0; index < domain->num_clks; index++)
			clk_disable_unprepare(domain->clk[index]);
	}

	return ret;
};

static int imx8m_pd_get_clocks(struct imx8m_pm_domain *domain)
{
	int i, ret;

	if (domain->genpd.flags & GENPD_FLAG_PM_PD_CLK)
		return 0;

	for (i = 0; ; i++) {
		struct clk *clk = of_clk_get(domain->dev->of_node, i);
		if (IS_ERR(clk))
			break;
		if (i >= MAX_CLK_NUM) {
			dev_err(domain->dev, "more than %d clocks\n",
				MAX_CLK_NUM);
			ret = -EINVAL;
			goto clk_err;
		}
		domain->clk[i] = clk;
	}
	domain->num_clks = i;

	return 0;

clk_err:
	while (i--)
		clk_put(domain->clk[i]);

	return ret;
}

static void imx8m_pd_put_clocks(struct imx8m_pm_domain *domain)
{
	int i;

	if (domain->genpd.flags & GENPD_FLAG_PM_PD_CLK)
		return;

	for (i = domain->num_clks - 1; i >= 0; i--)
		clk_put(domain->clk[i]);
}

static const struct of_device_id imx8m_pm_domain_ids[] = {
	{.compatible = "fsl,imx8m-pm-domain"},
	{},
};

static unsigned char idle_uv[] = "idle-microvolt";

static int imx8m_pm_domain_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct imx8m_pm_domain *domain;
	struct of_phandle_args parent, child;
	struct regulator *regulator, *dvfs_reg;
	int vcount;
	u32 microvolt[3];
	int ret;

	if (!of_parse_phandle_with_args(np, "parent-domains",
			"#power-domain-cells", 0, &parent)) {
		struct generic_pm_domain *genpd = of_genpd_get_from_provider(&parent);
		of_node_put(parent.np);
		if (IS_ERR(genpd))
			return -EPROBE_DEFER;
	}

	domain = devm_kzalloc(dev, sizeof(*domain), GFP_KERNEL);
	if (!domain)
		return -ENOMEM;

	child.np = np;
	domain->dev = dev;

	ret = of_property_read_string(np, "domain-name", &domain->genpd.name);
	if (ret) {
		dev_err(dev, "failed to get the domain name\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "domain-index", &domain->domain_index);
	if (ret) {
		dev_err(dev, "failed to get the domain index\n");
		return -EINVAL;
	}

	regulator = devm_regulator_get_optional(dev, "power");
	if (IS_ERR(regulator)) {
		if (PTR_ERR(regulator) != -ENODEV) {
			return dev_err_probe(dev, PTR_ERR(regulator),
					"failed to get domain's regulator\n");
		}
		regulator = NULL;
	}

	dvfs_reg = devm_regulator_get_optional(dev, "dvfs");
	if (IS_ERR(dvfs_reg)) {
		if (PTR_ERR(dvfs_reg) != -ENODEV) {
			return dev_err_probe(dev, PTR_ERR(dvfs_reg),
					     "Failed to get dvfs regulator\n");
		}
		dvfs_reg = NULL;
	}
	domain->regulator = regulator;
	domain->dvfs_reg = dvfs_reg;

	if (of_machine_is_compatible("fsl,imx8mp"))
		domain->genpd.flags |= GENPD_FLAG_PM_PD_CLK;

	ret = imx8m_pd_get_clocks(domain);
	if (ret) {
		return dev_err_probe(dev, ret,
				"failed to get domain's clocks\n");
	}

	domain->genpd.power_off = imx8m_pd_power_off;
	domain->genpd.power_on = imx8m_pd_power_on;
	if (of_property_read_bool(np, "active-wakeup"))
		domain->genpd.flags |= GENPD_FLAG_ACTIVE_WAKEUP;
	if (of_property_read_bool(np, "rpm-always-on"))
		domain->genpd.flags |= GENPD_FLAG_RPM_ALWAYS_ON;

	pm_genpd_init(&domain->genpd, NULL, !(domain->genpd.flags & GENPD_FLAG_RPM_ALWAYS_ON));

	ret = pm_genpd_of_add_clks(&domain->genpd, dev);
	if (ret) {
		pm_genpd_remove(&domain->genpd);
		return ret;
	}

	vcount = of_property_count_u32_elems(np, idle_uv);
	if ((vcount == 1) || (vcount == 3)) {
		ret = of_property_read_u32_array(np, idle_uv, microvolt, vcount);
		if (ret) {
			dev_err(dev, "%s: error parsing %s: %d\n", __func__, idle_uv, ret);
			ret = -EINVAL;
			goto free_clks;
		}
		if (vcount == 1) {
			domain->idle_uv = microvolt[0];
			domain->idle_uv_min = domain->idle_uv;
			domain->idle_uv_max = domain->idle_uv;
		} else {
			domain->idle_uv = microvolt[0];
			domain->idle_uv_min = microvolt[1];
			domain->idle_uv_max = microvolt[2];
		}
	}

	if (domain->num_clks && of_find_property(np, "operating-points-v2", NULL)) {
		ret = dev_pm_opp_of_add_table_indexed_np(&domain->genpd.dev, 0,
				false, np, domain->opp_np, domain->num_clks);
		if (ret && (ret != -ENODEV))
			goto free_clks;
	}

	ret = of_genpd_add_provider_simple(np, &domain->genpd);
	if (ret) {
		dev_err(dev, "failed to add the domain provider\n");
		goto rm_genpd;
	}

	/* add it as subdomain if necessary */
	if (!of_parse_phandle_with_args(np, "parent-domains",
			"#power-domain-cells", 0, &parent)) {
		ret = of_genpd_add_subdomain(&parent, &child);
		of_node_put(parent.np);

		if (ret < 0) {
			dev_dbg(dev, "failed to add the subdomain: %s: %d",
				domain->genpd.name, ret);
			of_genpd_del_provider(np);
			ret = -EPROBE_DEFER;
			goto rm_genpd;
		}
	}

	return 0;
rm_genpd:
	pm_genpd_remove(&domain->genpd);
free_clks:
	imx8m_pd_put_clocks(domain);
	return ret;
}

static struct platform_driver imx8m_pm_domain_driver = {
	.driver = {
		.name	= "imx8m_pm_domain",
		.owner	= THIS_MODULE,
		.of_match_table = imx8m_pm_domain_ids,
	},
	.probe = imx8m_pm_domain_probe,
};

static int __init imx8m_pm_domain_driver_init(void)
{
	return platform_driver_register(&imx8m_pm_domain_driver);
}
subsys_initcall(imx8m_pm_domain_driver_init);

static void __exit imx8m_pm_domain_driver_exit(void)
{
	platform_driver_unregister(&imx8m_pm_domain_driver);
}
module_exit(imx8m_pm_domain_driver_exit);

MODULE_AUTHOR("NXP");
MODULE_DESCRIPTION("NXP i.MX8M power domain driver");
MODULE_LICENSE("GPL v2");
