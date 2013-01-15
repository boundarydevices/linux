/*
 * Copyright (C) 2010-2013 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

/*
 * A driver for the Freescale Semiconductor i.MXC CPUfreq module.
 * The CPUFREQ driver is for controlling CPU frequency. It allows you to change
 * the CPU clock speed on the fly.
 */

#include <linux/module.h>
#include <linux/cpufreq.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <mach/hardware.h>
#include <mach/clock.h>
#include <linux/of_device.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>

#define CLK32_FREQ	32768
#define NANOSECOND	(1000 * 1000 * 1000)

static int cpu_freq_khz_min;
static int cpu_freq_khz_max;

static struct clk *cpu_clk, *pll1_sys, *pll1_sw, *step, *pll2_pfd2_396m;
static struct cpufreq_frequency_table *imx_freq_table;

static int cpu_op_nr;
static struct cpu_op *cpu_op_tbl;
static struct regulator *arm_regulator, *soc_regulator, *pu_regulator;

static int set_cpu_freq(int freq)
{
	int ret = 0;
	int pll_rate = 0;
	int org_cpu_rate;
	int cpu_volt = 0;
	int soc_volt = 0;
	int pu_volt = 0;
	int i;

	org_cpu_rate = clk_get_rate(cpu_clk);
	if (org_cpu_rate == freq)
		return ret;
	for (i = 0; i < cpu_op_nr; i++) {
		if (freq == cpu_op_tbl[i].cpu_rate) {
			cpu_volt = cpu_op_tbl[i].cpu_voltage;
			soc_volt = cpu_op_tbl[i].soc_voltage;
			pu_volt = cpu_op_tbl[i].pu_voltage;
			pll_rate = cpu_op_tbl[i].pll_rate;
		}
	}

	if (cpu_volt == 0)
		return ret;
	if (freq > org_cpu_rate) {
		if (!IS_ERR(soc_regulator)) {
			ret = regulator_set_voltage(soc_regulator,
				soc_volt, soc_volt);
			if (ret < 0) {
				printk(KERN_DEBUG "COULD NOT SET SOC VOLTAGE!!!!\n");
				return ret;
			}
		}
		if (!IS_ERR(pu_regulator)) {
			ret = regulator_set_voltage(pu_regulator,
				pu_volt, pu_volt);
			if (ret < 0) {
				printk(KERN_DEBUG "COULD NOT SET PU VOLTAGE!!!!\n");
				return ret;
			}
		}
		if (!IS_ERR(arm_regulator)) {
			ret = regulator_set_voltage(arm_regulator,
				cpu_volt, cpu_volt);
			if (ret < 0) {
				printk(KERN_DEBUG "COULD NOT SET CPU VOLTAGE!!!!\n");
				return ret;
			}
		}
		udelay(50);
	}
	if (freq > clk_get_rate(pll2_pfd2_396m)) {
		/* enable pfd396m */
		clk_prepare(pll2_pfd2_396m);
		clk_enable(pll2_pfd2_396m);
		/* move pll1_sw clk to step */
		clk_set_parent(pll1_sw, step);
		/* setp pll1_sys rate */
		clk_set_rate(pll1_sys, pll_rate);
		if (org_cpu_rate <= clk_get_rate(pll2_pfd2_396m)) {
			/* enable pll1_sys */
			clk_prepare(pll1_sys);
			clk_enable(pll1_sys);
		}
		/* move pll1_sw clk to pll1_sys */
		clk_set_parent(pll1_sw, pll1_sys);
		/* disable pfd396m */
		clk_disable(pll2_pfd2_396m);
		clk_unprepare(pll2_pfd2_396m);
	} else {
		/* enable pfd396m */
		clk_prepare(pll2_pfd2_396m);
		clk_enable(pll2_pfd2_396m);
		/* move pll1_sw clk to step */
		clk_set_parent(pll1_sw, step);
		/* disable pll1_sys */
		clk_disable(pll1_sys);
		clk_unprepare(pll1_sys);
	}
	/* set arm divider */
	clk_set_rate(cpu_clk, freq);

	if (ret != 0) {
		printk(KERN_DEBUG "cannot set CPU clock rate\n");
		return ret;
	}
	if (freq < org_cpu_rate) {
		if (!IS_ERR(arm_regulator)) {
			ret = regulator_set_voltage(arm_regulator,
				cpu_volt, cpu_volt);
			if (ret < 0) {
				printk(KERN_DEBUG "COULD NOT SET CPU VOLTAGE!!!!\n");
				return ret;
			}
		}
		if (!IS_ERR(soc_regulator)) {
			ret = regulator_set_voltage(soc_regulator,
				soc_volt, soc_volt);
			if (ret < 0) {
				printk(KERN_DEBUG "COULD NOT SET SOC VOLTAGE!!!!\n");
				return ret;
			}
		}
		if (!IS_ERR(pu_regulator)) {
			ret = regulator_set_voltage(pu_regulator,
				pu_volt, pu_volt);
			if (ret < 0) {
				printk(KERN_DEBUG "COULD NOT SET PU VOLTAGE!!!!\n");
				return ret;
			}
		}
		udelay(50);
	}
	return ret;
}

static int mxc_verify_speed(struct cpufreq_policy *policy)
{
	return 0;
	if (policy->cpu != 0)
		return -EINVAL;

	return cpufreq_frequency_table_verify(policy, imx_freq_table);
}

static unsigned int mxc_get_speed(unsigned int cpu)
{
	if (cpu)
		return 0;
	return clk_get_rate(cpu_clk) / 1000;
}

static int mxc_set_target(struct cpufreq_policy *policy,
			  unsigned int target_freq, unsigned int relation)
{
	struct cpufreq_freqs freqs;
	int freq_Hz;
	int ret = 0;
	unsigned int index;

	cpufreq_frequency_table_target(policy, imx_freq_table,
			target_freq, relation, &index);
	freq_Hz = imx_freq_table[index].frequency * 1000;

	freqs.old = clk_get_rate(cpu_clk) / 1000;
	freqs.new = freq_Hz / 1000;
	freqs.cpu = 0;
	freqs.flags = 0;
	cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);

	ret = set_cpu_freq(freq_Hz);

	cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);

	return ret;
}

static int mxc_cpufreq_init(struct cpufreq_policy *policy)
{
	int ret;
	int i;
	struct device_node *np;

	if (policy->cpu != 0)
		return -EINVAL;

	np = of_find_node_by_name(NULL, "cpufreq-setpoint");
	if (!np) {
		printk(KERN_ERR "%s: failed to find device tree data!\n",
			__func__);
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "setpoint-number", &cpu_op_nr);
	if (ret) {
		printk(KERN_ERR "%s: failed to get setpoint number!\n",
			__func__);
		return -EINVAL;
	}

	cpu_op_tbl = kzalloc(sizeof(*cpu_op_tbl) * cpu_op_nr, GFP_KERNEL);
	if (!cpu_op_tbl) {
		ret = -ENOMEM;
		goto err6;
	}

	i = 0;
	for_each_compatible_node(np, NULL, "setpoint-table") {
		if (of_property_read_u32(np, "pll_rate",
			&(cpu_op_tbl[i].pll_rate)))
			continue;
		if (of_property_read_u32(np, "cpu_rate",
			&(cpu_op_tbl[i].cpu_rate)))
			continue;
		if (of_property_read_u32(np, "cpu_podf",
			&(cpu_op_tbl[i].cpu_podf)))
			continue;
		if (of_property_read_u32(np, "pu_voltage",
			&(cpu_op_tbl[i].pu_voltage)))
			continue;
		if (of_property_read_u32(np, "soc_voltage",
			&(cpu_op_tbl[i].soc_voltage)))
			continue;
		if (of_property_read_u32(np, "cpu_voltage",
			&(cpu_op_tbl[i].cpu_voltage)))
			continue;
		i++;
	}

	printk(KERN_INFO "cpufreq support %d setpoint:\n", cpu_op_nr);
	for (i = 0; i < cpu_op_nr; i++) {
		printk(KERN_INFO "%d, %d, %d, %d, %d, %d\n",
			cpu_op_tbl[i].pll_rate, cpu_op_tbl[i].cpu_rate,
			cpu_op_tbl[i].cpu_podf, cpu_op_tbl[i].pu_voltage,
			cpu_op_tbl[i].soc_voltage,
			cpu_op_tbl[i].cpu_voltage);
	}

	pll1_sys = clk_get(NULL, "pll1_sys");
	if (IS_ERR(pll1_sys)) {
		printk(KERN_ERR "%s: failed to get pll1_sys\n", __func__);
		ret = -EINVAL;
		goto err5;
	}
	/* prepare and enable pll1 clock to make use count right */
	clk_prepare(pll1_sys);
	clk_enable(pll1_sys);

	pll1_sw = clk_get(NULL, "pll1_sw");
	if (IS_ERR(pll1_sw)) {
		printk(KERN_ERR "%s: failed to get pll1_sw\n", __func__);
		ret = -EINVAL;
		goto err4;
	}

	step = clk_get(NULL, "step");
	if (IS_ERR(pll1_sw)) {
		printk(KERN_ERR "%s: failed to get step\n", __func__);
		ret = -EINVAL;
		goto err3;
	}

	pll2_pfd2_396m = clk_get(NULL, "pll2_pfd2_396m");
	if (IS_ERR(pll1_sw)) {
		printk(KERN_ERR "%s: failed to get pll2_pfd2_396m\n",
			__func__);
		ret = -EINVAL;
		goto err2;
	}

	cpu_clk = clk_get(NULL, "arm");
	if (IS_ERR(cpu_clk)) {
		printk(KERN_ERR "%s: failed to get cpu clock\n",
			__func__);
		ret = -EINVAL;
		goto err1;
	}

	arm_regulator = regulator_get(NULL, "cpu");
	if (IS_ERR(arm_regulator)) {
		printk(KERN_ERR "%s: failed to get arm regulator\n",
			__func__);
		ret = -EINVAL;
		goto err1;
	}
	soc_regulator = regulator_get(NULL, "vddsoc");
	if (IS_ERR(soc_regulator)) {
		printk(KERN_ERR "%s: failed to get soc regulator\n",
			__func__);
		ret = -EINVAL;
		goto err1;
	}
	pu_regulator = regulator_get(NULL, "vddpu");
	if (IS_ERR(pu_regulator)) {
		printk(KERN_ERR "%s: failed to get pu regulator\n",
			__func__);
		ret = -EINVAL;
		goto err1;
	}

	cpu_freq_khz_min = cpu_op_tbl[0].cpu_rate / 1000;
	cpu_freq_khz_max = cpu_op_tbl[0].cpu_rate / 1000;

	imx_freq_table = kmalloc(
		sizeof(struct cpufreq_frequency_table) * (cpu_op_nr + 1),
			GFP_KERNEL);
	if (!imx_freq_table) {
		ret = -ENOMEM;
		goto err0;
	}

	for (i = 0; i < cpu_op_nr; i++) {
		imx_freq_table[i].index = i;
		imx_freq_table[i].frequency = cpu_op_tbl[i].cpu_rate / 1000;

		if ((cpu_op_tbl[i].cpu_rate / 1000) < cpu_freq_khz_min)
			cpu_freq_khz_min = cpu_op_tbl[i].cpu_rate / 1000;

		if ((cpu_op_tbl[i].cpu_rate / 1000) > cpu_freq_khz_max)
			cpu_freq_khz_max = cpu_op_tbl[i].cpu_rate / 1000;
	}

	imx_freq_table[i].index = i;
	imx_freq_table[i].frequency = CPUFREQ_TABLE_END;

	policy->cur = clk_get_rate(cpu_clk) / 1000;
	policy->min = policy->cpuinfo.min_freq = cpu_freq_khz_min;
	policy->max = policy->cpuinfo.max_freq = cpu_freq_khz_max;

	/* Manual states, that PLL stabilizes in two CLK32 periods */
	policy->cpuinfo.transition_latency = 2 * NANOSECOND / CLK32_FREQ;

	ret = cpufreq_frequency_table_cpuinfo(policy, imx_freq_table);

	if (ret < 0) {
		printk(KERN_ERR "%s: failed to register i.MXC CPUfreq with\
			error code %d\n", __func__, ret);
		ret = -ENOMEM;
		goto err0;
	}

	cpufreq_frequency_table_get_attr(imx_freq_table, policy->cpu);
	return 0;
err0:
	kfree(imx_freq_table);
err1:
	clk_put(cpu_clk);
err2:
	clk_put(pll2_pfd2_396m);
err3:
	clk_put(step);
err4:
	clk_put(pll1_sw);
err5:
	clk_put(pll1_sys);
err6:
	kfree(cpu_op_tbl);
	return ret;
}

static int mxc_cpufreq_exit(struct cpufreq_policy *policy)
{
	cpufreq_frequency_table_put_attr(policy->cpu);

	set_cpu_freq(cpu_freq_khz_max * 1000);
	clk_put(cpu_clk);
	kfree(imx_freq_table);
	return 0;
}

static struct cpufreq_driver mxc_driver = {
	.flags = CPUFREQ_STICKY,
	.verify = mxc_verify_speed,
	.target = mxc_set_target,
	.get = mxc_get_speed,
	.init = mxc_cpufreq_init,
	.exit = mxc_cpufreq_exit,
	.name = "imx",
};

static int __devinit mxc_cpufreq_driver_init(void)
{
	return cpufreq_register_driver(&mxc_driver);
}

static void mxc_cpufreq_driver_exit(void)
{
	cpufreq_unregister_driver(&mxc_driver);
}

module_init(mxc_cpufreq_driver_init);
module_exit(mxc_cpufreq_driver_exit);

MODULE_AUTHOR("Freescale Semiconductor Inc. Yong Shen <yong.shen@linaro.org>");
MODULE_DESCRIPTION("CPUfreq driver for i.MX");
MODULE_LICENSE("GPL");
