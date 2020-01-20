/*
 * Copyright (C) 2016 Freescale Semiconductor, Inc.
 * Copyright 2017 NXP
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/arm-smccc.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/cpumask.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/irqchip.h>
#include <linux/irqchip/arm-gic.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_domain.h>
#include <linux/pm_opp.h>
#include <soc/imx/fsl_sip.h>

#define GPC_MAX_IRQS		(4 * 32)

struct imx_gpc_pm_domain;

struct imx_gpc_pm_domain {
	struct device *dev;
	struct generic_pm_domain pd;
	struct imx_gpc_pm_domain *parent;
	int gpc_domain_id;
	struct clk **clks;
	unsigned int num_clks;
	struct regulator *reg;
	struct regulator *dvfs_reg;
	struct device_node **opp_np;
	unsigned long u_volt;
	unsigned long idle_uv;
	unsigned long idle_uv_min;
	unsigned long idle_uv_max;
};

enum imx_gpc_pm_domain_state {
	GPC_PD_STATE_OFF,
	GPC_PD_STATE_ON,
};

#define to_imx_gpc_pm_domain(_genpd) container_of(_genpd, struct imx_gpc_pm_domain, pd)

static DEFINE_SPINLOCK(gpc_psci_lock);
static DEFINE_MUTEX(gpc_pd_mutex);

static void imx_gpc_psci_irq_unmask(struct irq_data *d)
{
	struct arm_smccc_res res;

	spin_lock(&gpc_psci_lock);
	arm_smccc_smc(FSL_SIP_GPC, FSL_SIP_CONFIG_GPC_UNMASK, d->hwirq,
		      0, 0, 0, 0, 0, &res);
	spin_unlock(&gpc_psci_lock);

	irq_chip_unmask_parent(d);
}

static void imx_gpc_psci_irq_mask(struct irq_data *d)
{
	struct arm_smccc_res res;

	spin_lock(&gpc_psci_lock);
	arm_smccc_smc(FSL_SIP_GPC, FSL_SIP_CONFIG_GPC_MASK, d->hwirq,
		      0, 0, 0, 0, 0, &res);
	spin_unlock(&gpc_psci_lock);

	irq_chip_mask_parent(d);
}
static int imx_gpc_psci_irq_set_wake(struct irq_data *d, unsigned int on)
{
	struct arm_smccc_res res;

	spin_lock(&gpc_psci_lock);
	arm_smccc_smc(FSL_SIP_GPC, FSL_SIP_CONFIG_GPC_SET_WAKE, d->hwirq,
		      on, 0, 0, 0, 0, &res);
	spin_unlock(&gpc_psci_lock);

	return 0;
}

static int imx_gpc_psci_irq_set_affinity(struct irq_data *d,
					 const struct cpumask *dest, bool force)
{
	/* parse the cpu of irq affinity */
	struct arm_smccc_res res;
	int cpu = cpumask_any_and(dest, cpu_online_mask);

	irq_chip_set_affinity_parent(d, dest, force);

	spin_lock(&gpc_psci_lock);
	arm_smccc_smc(FSL_SIP_GPC, 0x4, d->hwirq,
		      cpu, 0, 0, 0, 0, &res);
	spin_unlock(&gpc_psci_lock);

	return 0;
}

static struct irq_chip imx_gpc_psci_chip = {
	.name			= "GPC-PSCI",
	.irq_eoi		= irq_chip_eoi_parent,
	.irq_mask		= imx_gpc_psci_irq_mask,
	.irq_unmask		= imx_gpc_psci_irq_unmask,
	.irq_retrigger		= irq_chip_retrigger_hierarchy,
	.irq_set_wake		= imx_gpc_psci_irq_set_wake,
	.irq_set_affinity	= imx_gpc_psci_irq_set_affinity,
};

static int imx_gpc_psci_domain_translate(struct irq_domain *d,
				    struct irq_fwspec *fwspec,
				    unsigned long *hwirq,
				    unsigned int *type)
{
	if (is_of_node(fwspec->fwnode)) {
		if (fwspec->param_count != 3)
			return -EINVAL;

		/* No PPI should point to this domain */
		if (fwspec->param[0] != 0)
			return -EINVAL;

		*hwirq = fwspec->param[1];
		*type = fwspec->param[2];
		return 0;
	}

	return -EINVAL;
}

static int imx_gpc_psci_domain_alloc(struct irq_domain *domain,
				  unsigned int irq,
				  unsigned int nr_irqs, void *data)
{
	struct irq_fwspec *fwspec = data;
	struct irq_fwspec parent_fwspec;
	irq_hw_number_t hwirq;
	int i;

	if (fwspec->param_count != 3)
		return -EINVAL;	/* Not GIC compliant */
	if (fwspec->param[0] != 0)
		return -EINVAL;	/* No PPI should point to this domain */

	hwirq = fwspec->param[1];
	if (hwirq >= GPC_MAX_IRQS)
		return -EINVAL;	/* Can't deal with this */

	for (i = 0; i < nr_irqs; i++)
		irq_domain_set_hwirq_and_chip(domain, irq + i, hwirq + i,
					      &imx_gpc_psci_chip, NULL);

	parent_fwspec = *fwspec;
	parent_fwspec.fwnode = domain->parent->fwnode;

	return irq_domain_alloc_irqs_parent(domain, irq, nr_irqs,
					    &parent_fwspec);
}

static struct irq_domain_ops imx_gpc_psci_domain_ops = {
	.translate = imx_gpc_psci_domain_translate,
	.alloc	= imx_gpc_psci_domain_alloc,
	.free	= irq_domain_free_irqs_common,
};

static int __init imx_gpc_psci_init(struct device_node *node,
			       struct device_node *parent)
{
	struct irq_domain *parent_domain, *domain;

	if (!parent) {
		pr_err("%s: no parent, giving up\n", node->full_name);
		return -ENODEV;
	}

	parent_domain = irq_find_host(parent);
	if (!parent_domain) {
		pr_err("%s: unable to obtain parent domain\n", node->full_name);
		return -ENXIO;
	}

	domain = irq_domain_add_hierarchy(parent_domain, 0, GPC_MAX_IRQS,
					  node, &imx_gpc_psci_domain_ops,
					  NULL);
	if (!domain)
		return -ENOMEM;

	return 0;
}
IRQCHIP_DECLARE(imx_gpc_psci, "fsl,imx8mq-gpc", imx_gpc_psci_init);

#define genpd_status_on(genpd)		((genpd)->status == GPD_STATE_ACTIVE)

struct uv_freq {
	unsigned long uv;
	unsigned long uv_min;
	unsigned long uv_max;
	unsigned long freq;
};

static void get_max_uv(struct imx_gpc_pm_domain *pd, struct uv_freq *uvf)
{
	struct clk **clks = pd->clks;
	struct device_node **opp_np = pd->opp_np;
	int i = 0;
	struct dev_pm_opp *opp;

	if (!opp_np || !clks || !pd->num_clks)
		return;

	while (clks[i] && opp_np[i]) {
		struct uv_freq uv = {
			.uv = 0,
			.uv_min = 0,
			.uv_max = 0,
			.freq = 0,
		};

		uv.freq = clk_get_rate(clks[i]);
		opp = dev_pm_opp_find_freq_ceil_np(pd->dev, opp_np[i],
				&uv.freq);
		if (opp) {
			opp_return_volts(opp, &uv.uv, &uv.uv_min, &uv.uv_max);
			pr_debug("%s: voltage=%ld freq=%ld, clk=%s\n", __func__,
					uv.uv, uv.freq, __clk_get_name(clks[i]));
			if (uvf->uv < uv.uv)
				*uvf = uv;
		}
		i++;
		if (i >= pd->num_clks)
			break;
	}
}

static void rescan_voltage(struct imx_gpc_pm_domain *parent, struct generic_pm_domain *skip)
{
	struct generic_pm_domain *genpd = &parent->pd;
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

	list_for_each_entry(link, &genpd->master_links, master_node) {
		child_pd = link->slave;

		if ((child_pd != skip) && genpd_status_on(child_pd))
			get_max_uv(to_imx_gpc_pm_domain(child_pd), &u_volt);
	}
	get_max_uv(parent, &u_volt);

	if (u_volt.uv && (parent->u_volt != u_volt.uv)) {
		parent->u_volt = u_volt.uv;
		ret = regulator_set_voltage_triplet(parent->dvfs_reg,
				u_volt.uv_min,
				u_volt.uv,
				u_volt.uv_max);
		pr_info("%s: voltage=%ld freq=%ld %s\n", __func__,
				u_volt.uv, u_volt.freq, genpd->name);
	}
}

static void check_voltage(struct imx_gpc_pm_domain *pd)
{
	struct imx_gpc_pm_domain *parent = pd->parent;
	int ret;
	struct uv_freq u_volt = {
		.uv = 0,
		.uv_min = 0,
		.uv_max = 0,
		.freq = 0,
	};

	if (!parent)
		parent = pd;
	if (!parent->dvfs_reg)
		return;

	get_max_uv(pd, &u_volt);

	if (parent->u_volt < u_volt.uv) {
		parent->u_volt = u_volt.uv;
		ret = regulator_set_voltage_triplet(parent->dvfs_reg,
				u_volt.uv_min,
				u_volt.uv,
				u_volt.uv_max);
		pr_info("%s: voltage=%ld %ld %ld freq=%ld %s\n", __func__,
			u_volt.uv_min, u_volt.uv, u_volt.uv_max, u_volt.freq,
			parent->pd.name);
	}
}

static int imx_gpc_pd_power_on(struct generic_pm_domain *domain)
{
	struct imx_gpc_pm_domain *pd = to_imx_gpc_pm_domain(domain);
	struct arm_smccc_res res;
	int index, ret = 0;

	/* power on the external supply */
	if (pd->reg) {
		ret = regulator_enable(pd->reg);
		if (ret) {
			dev_warn(pd->dev, "failed to power up the reg%d\n", ret);
			return ret;
		}
	}
	check_voltage(pd);

	if (pd->dvfs_reg) {
		ret = regulator_enable(pd->dvfs_reg);
		if (ret) {
			dev_warn(pd->dev, "failed to power up the dvfs-reg(%d)\n", ret);
			return ret;
		}
	}

	/* enable the necessary clks needed by the power domain */
	if (pd->num_clks) {
		for (index = 0; index < pd->num_clks; index++)
			clk_prepare_enable(pd->clks[index]);
	}

	if (pd->gpc_domain_id >= 0) {
		mutex_lock(&gpc_pd_mutex);
		arm_smccc_smc(FSL_SIP_GPC, FSL_SIP_CONFIG_GPC_PM_DOMAIN, pd->gpc_domain_id,
				GPC_PD_STATE_ON, 0, 0, 0, 0, &res);
		mutex_unlock(&gpc_pd_mutex);
	}
	return 0;
}

static int imx_gpc_pd_power_off(struct generic_pm_domain *domain)
{
	struct imx_gpc_pm_domain *pd = to_imx_gpc_pm_domain(domain);
	struct arm_smccc_res res;
	int index, ret = 0;

	if (pd->gpc_domain_id >= 0) {
		mutex_lock(&gpc_pd_mutex);
		arm_smccc_smc(FSL_SIP_GPC, FSL_SIP_CONFIG_GPC_PM_DOMAIN, pd->gpc_domain_id,
				GPC_PD_STATE_OFF, 0, 0, 0, 0, &res);
		mutex_unlock(&gpc_pd_mutex);
	}

	/* disable the necessary clks when power domain on finished */
	if (pd->num_clks) {
		for (index = 0; index < pd->num_clks; index++)
			clk_disable_unprepare(pd->clks[index]);
	}

	if (pd->dvfs_reg) {
		ret = regulator_disable(pd->dvfs_reg);
		if (ret)
			dev_warn(pd->dev, "failed to power off the dvfs-reg(%d)\n", ret);
	}
	if (pd->opp_np && pd->parent)
		rescan_voltage(pd->parent, domain);

	if ((pd->u_volt != pd->idle_uv) && pd->dvfs_reg && pd->idle_uv) {
		pd->u_volt = pd->idle_uv;
		ret = regulator_set_voltage_triplet(pd->dvfs_reg,
				pd->idle_uv_min,
				pd->idle_uv,
				pd->idle_uv_max);
		pr_info("%s: voltage=%ld %s\n", __func__, pd->idle_uv,
				pd->pd.name);

	}

	/* power off the external supply */
	if (pd->reg) {
		ret = regulator_disable(pd->reg);
		if (ret) {
			dev_warn(pd->dev, "failed to power off the reg%d\n", ret);
			return ret;
		}
	}
	return ret;
};

static int imx8m_pd_clk_init(struct device_node *np,
			     struct imx_gpc_pm_domain *domain)
{
	struct property *pp;
	struct clk **clks;
	int index;

	pp = of_find_property(np, "clocks", NULL);
	if (pp)
		domain->num_clks = pp->length / 8;
	else
		domain->num_clks = 0;

	if (domain->num_clks) {
		clks = kcalloc(domain->num_clks, sizeof(*clks), GFP_KERNEL);
		if (!clks) {
			domain->num_clks = 0;
			domain->clks = NULL;
			return -ENOMEM;
		}

		domain->clks = clks;
	}

	for (index = 0; index < domain->num_clks; index++) {
		clks[index] = of_clk_get(np, index);
		if (IS_ERR(clks[index])) {
			for (index = 0; index < domain->num_clks; index++) {
				if (!IS_ERR(clks[index]))
					clk_put(clks[index]);
			}

			domain->num_clks = 0;
			domain->clks = NULL;
			kfree(clks);
			pr_warn("imx8m domain clock init failed\n");
			return -ENODEV;
		}
	}

	return 0;
}

static int imx8m_add_subdomain(struct device_node *parent,
			       struct generic_pm_domain *parent_pd,
			       struct device *dev)
{
	struct device_node *child_node;
	struct imx_gpc_pm_domain *child_domain;
	int ret = 0;
	struct device_node **opp_np;

	/* add each of the child domain of parent */
	for_each_child_of_node(parent, child_node) {
		if (!of_device_is_available(child_node))
			continue;

		child_domain = kzalloc(sizeof(*child_domain), GFP_KERNEL);
		if (!child_domain)
			return -ENOMEM;
		child_domain->dev = dev;

		ret = of_property_read_string(child_node, "domain-name",
					      &child_domain->pd.name);
		if (ret)
			goto exit;

		ret = of_property_read_u32(child_node, "domain-id",
					   &child_domain->gpc_domain_id);
		if (ret)
			goto exit;
		pr_debug("%s: 0x%x\n", __func__, child_domain->gpc_domain_id);

		child_domain->pd.power_off = imx_gpc_pd_power_off;
		child_domain->pd.power_on = imx_gpc_pd_power_on;
		/* no reg for subdomains */
		child_domain->reg = NULL;
		child_domain->parent = to_imx_gpc_pm_domain(parent_pd);

		imx8m_pd_clk_init(child_node, child_domain);

		if (child_domain->num_clks && of_find_property(child_node, "operating-points-v2", NULL)) {
			opp_np = kcalloc(child_domain->num_clks, sizeof(*opp_np), GFP_KERNEL);
			ret = dev_pm_opp_of_add_table_np(dev, child_node,
					opp_np, child_domain->num_clks);
			child_domain->opp_np = opp_np;
			if (ret && (ret != -ENODEV))
				return ret;
		}

		/* power domains as off at boot */
		pm_genpd_init(&child_domain->pd, NULL, true);

		/* add subdomain of parent power domain */
		pm_genpd_add_subdomain(parent_pd, &child_domain->pd);

		ret = of_genpd_add_provider_simple(child_node,
						 &child_domain->pd);
		if (ret)
			pr_err("failed to add subdomain\n");
	}

	return 0;
exit:
	kfree(child_domain);
	return ret;
};

static unsigned char idle_uv[] = "idle-microvolt";

static int imx_gpc_pm_domain_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct imx_gpc_pm_domain *imx_pm_domain;
	int vcount;
	u32 microvolt[3];
	int ret = 0;

	if (!np) {
		dev_err(dev, "power domain device tree node not found\n");
		return -ENODEV;
	}

	imx_pm_domain = devm_kzalloc(dev, sizeof(*imx_pm_domain), GFP_KERNEL);
	if (!imx_pm_domain)
		return -ENOMEM;
	imx_pm_domain->dev = dev;

	ret = of_property_read_string(np, "domain-name", &imx_pm_domain->pd.name);
	if (ret) {
		dev_err(dev, "get domain name failed\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "domain-id", &imx_pm_domain->gpc_domain_id);
	if (ret) {
		dev_err(dev, "get domain id failed\n");
		return -EINVAL;
	}
	pr_debug("%s: 0x%x\n", __func__, imx_pm_domain->gpc_domain_id);

	imx_pm_domain->reg = devm_regulator_get_optional(dev, "power");
	if (IS_ERR(imx_pm_domain->reg)) {
		if (PTR_ERR(imx_pm_domain->reg) == -EPROBE_DEFER)
			return -EPROBE_DEFER;

		imx_pm_domain->reg = NULL;
	}

	imx_pm_domain->dvfs_reg = devm_regulator_get_optional(dev, "dvfs");
	if (IS_ERR(imx_pm_domain->dvfs_reg)) {
		if (PTR_ERR(imx_pm_domain->dvfs_reg) == -EPROBE_DEFER)
			return -EPROBE_DEFER;
		imx_pm_domain->dvfs_reg = NULL;
	}

	vcount = of_property_count_u32_elems(np, idle_uv);
	if ((vcount == 1) || (vcount == 3)) {
		ret = of_property_read_u32_array(np, idle_uv, microvolt, vcount);
		if (ret) {
			dev_err(dev, "%s: error parsing %s: %d\n", __func__, idle_uv, ret);
			return -EINVAL;
		}
		if (vcount == 1) {
			imx_pm_domain->idle_uv = microvolt[0];
			imx_pm_domain->idle_uv_min = imx_pm_domain->idle_uv;
			imx_pm_domain->idle_uv_max = imx_pm_domain->idle_uv;
		} else {
			imx_pm_domain->idle_uv = microvolt[0];
			imx_pm_domain->idle_uv_min = microvolt[1];
			imx_pm_domain->idle_uv_max = microvolt[2];
		}
	}

	imx8m_pd_clk_init(np, imx_pm_domain);

	imx_pm_domain->pd.power_off = imx_gpc_pd_power_off;
	imx_pm_domain->pd.power_on = imx_gpc_pd_power_on;
	/* all power domains as off at boot */
	pm_genpd_init(&imx_pm_domain->pd, NULL, true);

	ret = of_genpd_add_provider_simple(np,
				 &imx_pm_domain->pd);

	/* add subdomain */
	ret = imx8m_add_subdomain(np, &imx_pm_domain->pd, dev);
	if (ret)
		dev_warn(dev, "please check the child power domain init\n");

	return 0;
}

static const struct of_device_id imx_gpc_pm_domain_ids[] = {
	{.compatible = "fsl,imx8mq-pm-domain"},
	{.compatible = "fsl,imx8mm-pm-domain"},
	{.compatible = "fsl,imx8mn-pm-domain"},
	{},
};

static struct platform_driver imx_gpc_pm_domain_driver = {
	.driver = {
		.name	= "imx8m_gpc_pm_domain",
		.owner	= THIS_MODULE,
		.of_match_table = imx_gpc_pm_domain_ids,
	},
	.probe = imx_gpc_pm_domain_probe,
};

module_platform_driver(imx_gpc_pm_domain_driver);

MODULE_AUTHOR("NXP");
MODULE_DESCRIPTION("NXP i.MX8M GPC power domain driver");
MODULE_LICENSE("GPL v2");
