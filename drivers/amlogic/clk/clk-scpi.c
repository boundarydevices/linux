/*
 * drivers/amlogic/clk/clk-scpi.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/amlogic/scpi_protocol.h>


struct scpi_clk {
	u32 id;
	const char *name;
	struct clk_hw hw;
	struct scpi_dvfs_info *opps;
	unsigned long rate_min;
	unsigned long rate_max;
};

static struct platform_device *cpufreq_dev;
#define to_scpi_clk(clk) container_of(clk, struct scpi_clk, hw)

static unsigned long scpi_clk_recalc_rate(struct clk_hw *hw,
					  unsigned long parent_rate)
{
	struct scpi_clk *clk = to_scpi_clk(hw);

	return scpi_clk_get_val(clk->id);
}

static long scpi_clk_round_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long *parent_rate)
{
	struct scpi_clk *clk = to_scpi_clk(hw);

	if (clk->rate_min && rate < clk->rate_min)
		rate = clk->rate_min;
	if (clk->rate_max && rate > clk->rate_max)
		rate = clk->rate_max;

	return rate;
}

static int scpi_clk_set_rate(struct clk_hw *hw, unsigned long rate,
			     unsigned long parent_rate)
{
	struct scpi_clk *clk = to_scpi_clk(hw);

	return scpi_clk_set_val(clk->id, rate);
}

static const struct clk_ops scpi_clk_ops = {
	.recalc_rate = scpi_clk_recalc_rate,
	.round_rate = scpi_clk_round_rate,
	.set_rate = scpi_clk_set_rate,
};

/* find closest match to given frequency in OPP table */
static int __scpi_dvfs_round_rate(struct scpi_clk *clk, unsigned long rate)
{
	int idx, max_opp = clk->opps->count;
	struct scpi_opp_entry *opp = clk->opps->opp;
	u32 fmin = 0, fmax = ~0, ftmp;

	for (idx = 0; idx < max_opp; idx++, opp++) {
		ftmp = opp->freq_hz;
		if (ftmp >= (u32)rate) {
			if (ftmp <= fmax)
				fmax = ftmp;
		} else {
			if (ftmp >= fmin)
				fmin = ftmp;
		}
	}
	if (fmax != ~0)
		return fmax;
	else
		return fmin;
}

static unsigned long scpi_dvfs_recalc_rate(struct clk_hw *hw,
					   unsigned long parent_rate)
{
	struct scpi_clk *clk = to_scpi_clk(hw);
	int idx = scpi_dvfs_get_idx(clk->id);
	struct scpi_opp_entry *opp = clk->opps->opp;

	if (idx < 0)
		return 0;
	else
		return opp[idx].freq_hz;
}

static long scpi_dvfs_round_rate(struct clk_hw *hw, unsigned long rate,
				 unsigned long *parent_rate)
{
	struct scpi_clk *clk = to_scpi_clk(hw);

	return __scpi_dvfs_round_rate(clk, rate);
}

static int __scpi_find_dvfs_index(struct scpi_clk *clk, unsigned long rate)
{
	int idx, max_opp = clk->opps->count;
	struct scpi_opp_entry *opp = clk->opps->opp;

	for (idx = 0; idx < max_opp; idx++, opp++)
		if (opp->freq_hz == (u32)rate)
			break;
	return (idx == max_opp) ? -EINVAL : idx;
}

static int scpi_dvfs_set_rate(struct clk_hw *hw, unsigned long rate,
			      unsigned long parent_rate)
{
	struct scpi_clk *clk = to_scpi_clk(hw);
	int ret = __scpi_find_dvfs_index(clk, rate);

	if (ret < 0)
		return ret;
	else
		return scpi_dvfs_set_idx(clk->id, (u8)ret);
}

static const struct clk_ops scpi_dvfs_ops = {
	.recalc_rate = scpi_dvfs_recalc_rate,
	.round_rate = scpi_dvfs_round_rate,
	.set_rate = scpi_dvfs_set_rate,
};

static struct clk *
scpi_dvfs_ops_init(struct device *dev, struct device_node *np,
		   struct scpi_clk *sclk)
{
	struct clk_init_data init;
	struct scpi_dvfs_info *opps;

	init.name = sclk->name;
	init.flags = 0;
	init.num_parents = 0;
	init.ops = &scpi_dvfs_ops;
	sclk->hw.init = &init;

	opps = scpi_dvfs_get_opps(sclk->id);
	if (IS_ERR(opps))
		return (struct clk *)opps;

	sclk->opps = opps;

	return devm_clk_register(dev, &sclk->hw);
}

static struct clk *
scpi_clk_ops_init(struct device *dev, struct device_node *np,
		  struct scpi_clk *sclk)
{
	struct clk_init_data init;
	u32 range[2];
	int ret;

	init.name = sclk->name;
	init.flags = 0;
	init.num_parents = 0;
	init.ops = &scpi_clk_ops;
	sclk->hw.init = &init;

	ret = of_property_read_u32_array(np, "frequency-range", range,
					 ARRAY_SIZE(range));
	if (ret)
		return ERR_PTR(ret);
	sclk->rate_min = range[0];
	sclk->rate_max = range[1];

	return devm_clk_register(dev, &sclk->hw);
}

static int scpi_clk_setup(struct device *dev, struct device_node *np,
			  const void *data)
{
	struct clk * (*setup_ops)(struct device *, struct device_node *,
				 struct scpi_clk *) = data;
	struct clk_onecell_data *clk_data;
	struct clk **clks;
	int count;
	int idx;

	count = of_property_count_strings(np, "clock-output-names");
	if (count < 0) {
		dev_err(dev, "%s: invalid clock output count\n", np->name);
		return -EINVAL;
	}

	clk_data = devm_kmalloc(dev, sizeof(*clk_data), GFP_KERNEL);
	if (!clk_data)
		return -ENOMEM;

	clks = devm_kmalloc(dev, count * sizeof(*clks), GFP_KERNEL);
	if (!clks)
		return -ENOMEM;

	for (idx = 0; idx < count; idx++) {
		struct scpi_clk *sclk;
		u32 val;

		sclk = devm_kzalloc(dev, sizeof(*sclk), GFP_KERNEL);
		if (!sclk)
			return -ENOMEM;

		if (of_property_read_string_index(np, "clock-output-names",
						  idx, &sclk->name)) {
			dev_err(dev, "invalid clock name @ %s\n", np->name);
			return -EINVAL;
		}

		if (of_property_read_u32_index(np, "clock-indices",
					       idx, &val)) {
			dev_err(dev, "invalid clock index @ %s\n", np->name);
			return -EINVAL;
		}

		sclk->id = val;

		clks[idx] = setup_ops(dev, np, sclk);
		if (IS_ERR(clks[idx])) {
			dev_err(dev, "failed to register clock '%s'\n",
				sclk->name);
			return PTR_ERR(clks[idx]);
		}

		dev_dbg(dev, "Registered clock '%s'\n", sclk->name);
	}

	clk_data->clks = clks;
	clk_data->clk_num = count;
	of_clk_add_provider(np, of_clk_src_onecell_get, clk_data);

	return 0;
}

static const struct of_device_id clk_match[] = {
	{ .compatible = "arm, scpi-clk-indexed", .data = scpi_dvfs_ops_init, },
	{ .compatible = "arm, scpi-clk-range", .data = &scpi_clk_ops_init, },
	{}
};

static int scpi_clk_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node, *child;
	const struct of_device_id *match;
	int ret;

	for_each_child_of_node(np, child) {
		match = of_match_node(clk_match, child);
		if (!match)
			continue;
		ret = scpi_clk_setup(dev, child, match->data);
		if (ret)
			return ret;
	}
	/* Add the virtual cpufreq device */
	cpufreq_dev = platform_device_register_simple("scpi-cpufreq",
						      -1, NULL, 0);
	if (!cpufreq_dev)
		pr_warn("unable to register cpufreq device");

	return 0;
}

static const struct of_device_id scpi_clk_ids[] = {
	{ .compatible = "arm, scpi-clks", },
	{}
};

static struct platform_driver scpi_clk_driver = {
	.driver	= {
		.name = "aml_scpi_clocks",
		.of_match_table = scpi_clk_ids,
	},
	.probe = scpi_clk_probe,
};

static int __init scpi_clk_init(void)
{
	return platform_driver_register(&scpi_clk_driver);
}
postcore_initcall(scpi_clk_init);

static void __exit scpi_clk_exit(void)
{
	platform_driver_unregister(&scpi_clk_driver);
}
module_exit(scpi_clk_exit);

MODULE_AUTHOR("Sudeep Holla <sudeep.holla@arm.com>");
MODULE_DESCRIPTION("ARM SCPI clock driver");
MODULE_LICENSE("GPL");
