/*
 * drivers/amlogic/cpufreq/m8b_cpufreq.c
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

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/cpu.h>

#include <linux/pm_opp.h>
#include <linux/module.h>
#include <linux/of.h>

#define FIX_PLL_TABLE_CNT	8

struct meson_cpufreq {
	int fixpll_target;
	int opp_size;
	int *opp_table;
	struct cpufreq_frequency_table *table;
	struct clk *armclk;
	struct clk *sysclk;
};

static struct meson_cpufreq *mfreq;

static void build_fixpll_freqtable(void *data, int target,
				   int *opp, int size,
				   struct device *dev)
{
	int i = 0, j, v, ret;
	int ratio[FIX_PLL_TABLE_CNT] = {1, 2, 4, 8, 10, 12, 14, 16};
	struct cpufreq_frequency_table *table;

	if (!data)
		return;

	table = data;
	for (i = 0; i < FIX_PLL_TABLE_CNT; i++) {
		table[i].driver_data = i;
		table[i].frequency = target * ratio[i] / 16;
		v = 0;
		for (j = 0; j < size / 2; j++) {
			if (opp[j * 2] >= table[i].frequency) {
				v = opp[j * 2 + 1];
				break;
			}
		}
		if (v) {
			ret = dev_pm_opp_add(dev, table[i].frequency * 1000, v);
			pr_debug("cpu freq:%d, voltage:%d, ret:%d\n",
				table[i].frequency, v, ret);
		} else
			pr_err("no opp for cpu freq:%d\n", table[i].frequency);
	}
	table[i].frequency = CPUFREQ_TABLE_END;
}

static DEFINE_MUTEX(meson_cpufreq_mutex);

static int meson_cpufreq_verify(struct cpufreq_policy *policy)
{
	struct cpufreq_frequency_table *freq_table;

	freq_table = policy->freq_table;
	if (freq_table)
		return cpufreq_frequency_table_verify(policy, freq_table);

	if (policy->cpu)
		return -EINVAL;

	cpufreq_verify_within_limits(policy, policy->cpuinfo.min_freq,
				     policy->cpuinfo.max_freq);

	policy->min = clk_round_rate(mfreq->armclk, policy->min * 1000) / 1000;
	policy->max = clk_round_rate(mfreq->armclk, policy->max * 1000) / 1000;
	cpufreq_verify_within_limits(policy, policy->cpuinfo.min_freq,
				     policy->cpuinfo.max_freq);
	return 0;
}

static int meson_cpufreq_target_locked(struct cpufreq_policy *policy,
				       unsigned int target_freq,
				       unsigned int relation)
{
	struct cpufreq_freqs freqs;
	int cpu = policy ? policy->cpu : 0;
	int ret = -EINVAL;
	struct cpufreq_frequency_table *freq_table;

	if (cpu > (num_possible_cpus() - 1)) {
		pr_err("cpu %d set target freq error\n", cpu);
		return ret;
	}

	freq_table = policy->freq_table;
	if (IS_ERR_OR_NULL(freq_table)) {
		pr_err("can't get freq_table\n");
		return ret;
	}

	/* Ensure desired rate is within allowed range.  Some govenors
	 * (ondemand) will just pass target_freq=0 to get the minimum.
	 */
	if (policy) {
		if (target_freq < policy->min)
			target_freq = policy->min;
		if (target_freq > policy->max)
			target_freq = policy->max;
	}

	/* sys pll is not locked to target */
	if (!((unsigned long)(mfreq->sysclk) & 0x03)) {
		/* armclk will also be updated by updating sysclk*/
		freqs.cpu = cpu;
		freqs.old = policy->cur;
		freqs.new = mfreq->fixpll_target;

		cpufreq_freq_transition_begin(policy, &freqs);
		ret = clk_set_rate(mfreq->sysclk, mfreq->fixpll_target * 1000);
		cpufreq_freq_transition_end(policy, &freqs, ret);
		if (ret) {
			pr_err("lock sys pll to target failed\n");
			return -EINVAL;
		}
		mfreq->sysclk = (void *)((unsigned long)(mfreq->sysclk) | 0x03);
	}

	ret = cpufreq_frequency_table_target(policy, target_freq,
					     CPUFREQ_RELATION_H);
	if (ret >= 0)
		target_freq = freq_table[ret].frequency;
	else {
		pr_err("can't find %d in freq table:%d\n", target_freq, ret);
		return ret;
	}

	freqs.old = clk_get_rate(mfreq->armclk) / 1000;
	freqs.new = clk_round_rate(mfreq->armclk, target_freq * 1000) / 1000;
	freqs.cpu = cpu;

	if (freqs.old == freqs.new) {
		pr_info("freq equal, new:%d\n", freqs.new);
		return ret;
	}


	pr_debug("cpufreq-meson: CPU%d transition: %u --> %u\n",
		freqs.cpu, freqs.old, freqs.new);
	cpufreq_freq_transition_begin(policy, &freqs);
	ret = clk_set_rate(mfreq->armclk, freqs.new * 1000);
	cpufreq_freq_transition_end(policy, &freqs, ret);
	pr_debug("cpufreq-meson: end\n");
	return ret;
}

static int meson_cpufreq_target(struct cpufreq_policy *policy,
				unsigned int target_freq,
				unsigned int relation)
{
	int ret;
	/* TODO: set max freq for sys pll */

	mutex_lock(&meson_cpufreq_mutex);
	ret = meson_cpufreq_target_locked(policy, target_freq, relation);
	mutex_unlock(&meson_cpufreq_mutex);

	return ret;
}

static unsigned int meson_cpufreq_get(unsigned int cpu)
{
	unsigned long rate;

	if (cpu > (num_possible_cpus() - 1)) {
		pr_err("cpu %d on current thread error\n", cpu);
		return 0;
	}
	rate = clk_get_rate(mfreq->armclk) / 1000;
	return rate;
}

static ssize_t opp_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	int size = 0;
	struct dev_pm_opp *opp;
	unsigned long freq = 0, voltage;

	while (1) {
		opp = dev_pm_opp_find_freq_ceil(dev, &freq);
		if (IS_ERR_OR_NULL(opp)) {
			pr_err("can't found opp for freq:%ld\n", freq);
			break;
		}
		freq = dev_pm_opp_get_freq(opp);
		voltage = dev_pm_opp_get_voltage(opp);
		size += sprintf(buf + size, "%10ld  %7ld\n", freq, voltage);
		freq += 1;	/* increase for next freq */
	}
	return size;
}
static DEVICE_ATTR(opp, 0444, opp_show, NULL);

static int meson_cpufreq_init(struct cpufreq_policy *policy)
{
	struct device *cpu_dev;
	int ret;

	cpu_dev  = get_cpu_device(policy->cpu);
	if (policy->cpu != 0)
		return -EINVAL;

	if (policy->cpu > (num_possible_cpus() - 1)) {
		pr_err("cpu %d on current thread error\n", policy->cpu);
		return -1;
	}

	cpumask_setall(policy->cpus);
	/* build and add opp table */
	build_fixpll_freqtable(mfreq->table,
			       mfreq->fixpll_target, mfreq->opp_table,
			       mfreq->opp_size, cpu_dev);

	if (cpufreq_table_validate_and_show(policy, mfreq->table)) {
		pr_err("invalid freq table\n");
		return -EINVAL;
	}

	ret = device_create_file(cpu_dev, &dev_attr_opp);
	if (ret < 0)
		dev_err(cpu_dev, "create opp sysfs failed\n");

	kfree(mfreq->opp_table);
	mfreq->opp_table = 0;
	return 0;
}

static struct freq_attr *meson_cpufreq_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL,
};

static int meson_cpufreq_suspend(struct cpufreq_policy *policy)
{
	return 0;
}

static int meson_cpufreq_resume(struct cpufreq_policy *policy)
{
	return 0;
}

static struct cpufreq_driver meson_cpufreq_driver = {
	.flags   = CPUFREQ_STICKY,
	.verify  = meson_cpufreq_verify,
	.target  = meson_cpufreq_target,
	.get     = meson_cpufreq_get,
	.init    = meson_cpufreq_init,
	.name    = "meson_cpufreq",
	.attr    = meson_cpufreq_attr,
	.suspend = meson_cpufreq_suspend,
	.resume  = meson_cpufreq_resume
};

static int meson_cpufreq_probe(struct platform_device *pdev)
{
	int err = 0;
	int target, size = 0;
	struct device_node *node;
	struct property *prop;
	struct cpufreq_frequency_table *table = NULL;
	unsigned int *opp = NULL;

	node = pdev->dev.of_node;
	if (!node)
		return -EINVAL;

	mfreq = kzalloc(sizeof(struct meson_cpufreq), GFP_KERNEL);
	if (!mfreq)
		return -ENOMEM;

	err = of_property_read_u32(node, "fixpll_target", &target);
	if (err)
		target = 1536000;
	else
		pr_info("%s:SYSPLL fix to target:%u\n", __func__, target);

	/* find fix pll target */
	mfreq->fixpll_target = target;
	table = kzalloc(sizeof(*table) * FIX_PLL_TABLE_CNT, GFP_KERNEL);
	if (!table) {
		pr_err("%s, alloc freq table failed\n", __func__);
		return -ENOMEM;
	}
	mfreq->table = table;

	/* find opp table */
	prop = of_find_property(node, "opp_table", &size);
	if (IS_ERR_OR_NULL(prop)) {
		pr_err("%s, can't find opp table\n", __func__);
		goto out;
	}
	opp = kzalloc(size, GFP_KERNEL);
	if (!opp)
		goto out;

	size /= sizeof(int);
	err = of_property_read_u32_array(node, "opp_table", opp, size);
	if (err < 0) {
		pr_err("%s, read opp_table failed, size:%d\n", __func__, size);
		goto out;
	}
	mfreq->opp_size = size;
	mfreq->opp_table = opp;

	mfreq->armclk = devm_clk_get(&pdev->dev, "cpu_clk");
	if (IS_ERR_OR_NULL(mfreq->armclk)) {
		pr_err("Unable to get ARM clock\n");
		goto out;
	}
	mfreq->sysclk = devm_clk_get(&pdev->dev, "sys_clk");
	if (IS_ERR_OR_NULL(mfreq->sysclk)) {
		pr_err("Unable to get sys clock\n");
		goto out;
	}
	err = clk_prepare_enable(mfreq->sysclk);
	if (err) {
		pr_err("enable sysclk failed\n");
		goto out;
	}

	return cpufreq_register_driver(&meson_cpufreq_driver);

out:
	kfree(table);
	kfree(opp);
	return -EINVAL;
}


static int __exit meson_cpufreq_remove(struct platform_device *pdev)
{
	kfree(mfreq->table);
	return cpufreq_unregister_driver(&meson_cpufreq_driver);
}

#ifdef CONFIG_OF
static const struct of_device_id amlogic_cpufreq_meson_dt_match[] = {
	{
		.compatible = "amlogic, cpufreq-meson",
	},
	{},
};
#else
#define amlogic_cpufreq_meson_dt_match NULL
#endif


static struct platform_driver meson_cpufreq_parent_driver = {
	.driver = {
		.name   = "cpufreq-meson",
		.owner  = THIS_MODULE,
		.of_match_table = amlogic_cpufreq_meson_dt_match,
	},
	.probe  = meson_cpufreq_probe,
	.remove = meson_cpufreq_remove,
};

static int __init meson_cpufreq_parent_init(void)
{
	return platform_driver_register(&meson_cpufreq_parent_driver);
}
late_initcall(meson_cpufreq_parent_init);

