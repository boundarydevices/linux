/*
 * Generic big.LITTLE CPUFreq Interface driver
 *
 * It provides necessary ops to arm_big_little cpufreq driver and gets
 * Frequency information from Device Tree. Freq table in DT must be in KHz.
 *
 * Copyright (C) 2013 Linaro.
 * Viresh Kumar <viresh.kumar@linaro.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

//#define DEBUG  0

#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/device.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/pm_opp.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/clk.h>
#include <linux/cpumask.h>
#include <linux/clk-provider.h>
#include <linux/cpu_cooling.h>
#include <linux/mutex.h>
#include <linux/of_platform.h>
#include <linux/topology.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/regulator/driver.h>

#include "arm_big_little.h"
#include "../regulator/internal.h"

/* Currently we support only two clusters */
#define A15_CLUSTER	0
#define A7_CLUSTER	1
#define MAX_CLUSTERS	2

#ifdef CONFIG_BL_SWITCHER
#include <asm/bL_switcher.h>
static bool bL_switching_enabled;
#define is_bL_switching_enabled()	bL_switching_enabled
#define set_switching_enabled(x)	(bL_switching_enabled = (x))
#else
#define is_bL_switching_enabled()	false
#define set_switching_enabled(x)	do { } while (0)
#define bL_switch_request(...)		do { } while (0)
#define bL_switcher_put_enabled()	do { } while (0)
#define bL_switcher_get_enabled()	do { } while (0)
#endif

#define ACTUAL_FREQ(cluster, freq)  ((cluster == A7_CLUSTER) ? freq << 1 : freq)
#define VIRT_FREQ(cluster, freq)    ((cluster == A7_CLUSTER) ? freq >> 1 : freq)

/*core power supply*/
#define CORE_SUPPLY "cpu"

/* Core Clocks */
#define CORE_CLK	"core_clk"
#define LOW_FREQ_CLK_PARENT	"low_freq_clk_parent"
#define HIGH_FREQ_CLK_PARENT	"high_freq_clk_parent"

static struct thermal_cooling_device *cdev[MAX_CLUSTERS];
static struct cpufreq_arm_bL_ops *arm_bL_ops;
static struct clk *clk[MAX_CLUSTERS];
static struct cpufreq_frequency_table *freq_table[MAX_CLUSTERS + 1];
static atomic_t cluster_usage[MAX_CLUSTERS + 1];

static unsigned int clk_big_min;	/* (Big) clock frequencies */
static unsigned int clk_little_max;	/* Maximum clock frequency (Little) */

/* Default voltage_tolerance */
#define DEF_VOLT_TOL		0

/*mid rate for set parent,Khz*/
static unsigned int mid_rate = (1000*1000);
static unsigned int gap_rate = (10*1000*1000);

struct meson_cpufreq_driver_data {
	struct device *cpu_dev;
	struct regulator *reg;
	/* voltage tolerance in percentage */
	unsigned int volt_tol;
	struct clk *high_freq_clk_p;
	struct clk *low_freq_clk_p;
};


static DEFINE_PER_CPU(unsigned int, physical_cluster);
static DEFINE_PER_CPU(unsigned int, cpu_last_req_freq);

static struct mutex cluster_lock[MAX_CLUSTERS];

static inline int raw_cpu_to_cluster(int cpu)
{
	return topology_physical_package_id(cpu);
}

static inline int cpu_to_cluster(int cpu)
{
	return is_bL_switching_enabled() ?
		MAX_CLUSTERS : raw_cpu_to_cluster(cpu);
}

static int dt_get_transition_latency(struct device *cpu_dev)
{
	struct device_node *np;
	u32 transition_latency = CPUFREQ_ETERNAL;

	np = of_node_get(cpu_dev->of_node);
	if (!np) {
		pr_info("Failed to find cpu node. Use CPUFREQ_ETERNAL transition latency\n");
		return CPUFREQ_ETERNAL;
	}

	of_property_read_u32(np, "clock-latency", &transition_latency);
	of_node_put(np);

	pr_debug("%s: clock-latency: %d\n", __func__, transition_latency);
	return transition_latency;
}

static unsigned int find_cluster_maxfreq(int cluster)
{
	int j;
	u32 max_freq = 0, cpu_freq;

	for_each_online_cpu(j) {
		cpu_freq = per_cpu(cpu_last_req_freq, j);

		if ((cluster == per_cpu(physical_cluster, j)) &&
				(max_freq < cpu_freq))
			max_freq = cpu_freq;
	}

	pr_debug("%s: cluster: %d, max freq: %d\n", __func__, cluster,
			max_freq);

	return max_freq;
}

static unsigned int clk_get_cpu_rate(unsigned int cpu)
{
	u32 cur_cluster = per_cpu(physical_cluster, cpu);
	u32 rate = clk_get_rate(clk[cur_cluster]) / 1000;

	/* For switcher we use virtual A7 clock rates */
	if (is_bL_switching_enabled())
		rate = VIRT_FREQ(cur_cluster, rate);

	pr_debug("%s: cpu: %d, cluster: %d, freq: %u\n", __func__, cpu,
			cur_cluster, rate);

	return rate;
}

static unsigned int meson_bL_cpufreq_get_rate(unsigned int cpu)
{
	if (is_bL_switching_enabled()) {
		pr_debug("%s: freq: %d\n", __func__, per_cpu(cpu_last_req_freq,
					cpu));

		return per_cpu(cpu_last_req_freq, cpu);
	} else {
		return clk_get_cpu_rate(cpu);
	}
}

static unsigned int meson_bL_cpufreq_set_rate(struct cpufreq_policy *policy,
	u32 old_cluster, u32 new_cluster, u32 rate)
{
	struct clk  *low_freq_clk_p, *high_freq_clk_p;
	struct meson_cpufreq_driver_data *cpufreq_data;
	u32 new_rate, prev_rate;
	int ret, cpu = 0;
	bool bLs = is_bL_switching_enabled();

	cpu = policy->cpu;
	cpufreq_data = policy->driver_data;
	high_freq_clk_p = cpufreq_data->high_freq_clk_p;
	low_freq_clk_p = cpufreq_data->low_freq_clk_p;

#ifdef CONFIG_AMLOGIC_COMMON_CLK_SCPI
	/* MARK: cluster0 and cluster share the same scpi lock,
	 * and don't send scpi command at the same time
	 */
	mutex_lock(&cluster_lock[0]);
#else
	mutex_lock(&cluster_lock[new_cluster]);
#endif

	if (bLs) {
		prev_rate = per_cpu(cpu_last_req_freq, cpu);
		per_cpu(cpu_last_req_freq, cpu) = rate;
		per_cpu(physical_cluster, cpu) = new_cluster;

		new_rate = find_cluster_maxfreq(new_cluster);
		new_rate = ACTUAL_FREQ(new_cluster, new_rate);
	} else {
		new_rate = rate;
	}

	pr_debug("%s: cpu: %d, old cluster: %d, new cluster: %d, freq: %d\n",
			__func__, cpu, old_cluster, new_cluster, new_rate);

	if (new_rate > mid_rate) {
		if (__clk_get_enable_count(high_freq_clk_p) == 0) {
			ret = clk_prepare_enable(high_freq_clk_p);
			if (ret) {
				pr_err("%s: CPU%d clk_prepare_enable failed\n",
					__func__, policy->cpu);
				return ret;
			}
		}

		ret = clk_set_parent(clk[new_cluster], low_freq_clk_p);
		if (ret) {
			pr_err("%s: error in setting low_freq_clk_p as parent\n",
				__func__);
			return ret;
		}

		ret = clk_set_rate(high_freq_clk_p, new_rate * 1000);
		if (ret) {
			pr_err("%s: error in setting low_freq_clk_p rate!\n",
				__func__);
			return ret;
		}

		ret = clk_set_parent(clk[new_cluster], high_freq_clk_p);
		if (ret) {
			pr_err("%s: error in setting high_freq_clk_p as parent\n",
				__func__);
			return ret;
		}
	} else {
		ret = clk_set_rate(low_freq_clk_p, new_rate * 1000);
		if (ret) {
			pr_err("%s: error in setting low_freq_clk_p rate!\n",
				__func__);
			return ret;
		}

		ret = clk_set_parent(clk[new_cluster], low_freq_clk_p);
		if (ret) {
			pr_err("%s: error in setting low_freq_clk_p rate!\n",
				__func__);
			return ret;
		}

		if (__clk_get_enable_count(high_freq_clk_p) >= 1)
			clk_disable_unprepare(high_freq_clk_p);
	}

	if (!ret) {
		/*
		 * FIXME: clk_set_rate hasn't returned an error here however it
		 * may be that clk_change_rate failed due to hardware or
		 * firmware issues and wasn't able to report that due to the
		 * current design of the clk core layer. To work around this
		 * problem we will read back the clock rate and check it is
		 * correct. This needs to be removed once clk core is fixed.
		 */
		if (abs(clk_get_rate(clk[new_cluster]) - new_rate * 1000)
				> gap_rate)
			ret = -EIO;
	}

	if (WARN_ON(ret)) {
		pr_err("clk_set_rate failed: %d, new cluster: %d\n", ret,
				new_cluster);
		if (bLs) {
			per_cpu(cpu_last_req_freq, cpu) = prev_rate;
			per_cpu(physical_cluster, cpu) = old_cluster;
		}

#ifdef CONFIG_AMLOGIC_COMMON_CLK_SCPI
		mutex_unlock(&cluster_lock[0]);
#else
		mutex_unlock(&cluster_lock[new_cluster]);
#endif

		return ret;
	}

#ifdef CONFIG_AMLOGIC_COMMON_CLK_SCPI
	mutex_unlock(&cluster_lock[0]);
#else
	mutex_unlock(&cluster_lock[new_cluster]);
#endif

	/* Recalc freq for old cluster when switching clusters */
	if (old_cluster != new_cluster) {
		pr_debug("%s: cpu: %d, old cluster: %d, new cluster: %d\n",
				__func__, cpu, old_cluster, new_cluster);

		/* Switch cluster */
		bL_switch_request(cpu, new_cluster);

#ifdef CONFIG_AMLOGIC_COMMON_CLK_SCPI
		mutex_lock(&cluster_lock[0]);
#else
		mutex_lock(&cluster_lock[new_cluster]);
#endif

		/* Set freq of old cluster if there are cpus left on it */
		new_rate = find_cluster_maxfreq(old_cluster);
		new_rate = ACTUAL_FREQ(old_cluster, new_rate);

		if (new_rate) {
			pr_debug("%s: Updating rate of old cluster: %d, to freq: %d\n",
					__func__, old_cluster, new_rate);

			if (clk_set_rate(clk[old_cluster], new_rate * 1000))
				pr_err("%s: clk_set_rate failed: %d, old cluster: %d\n",
						__func__, ret, old_cluster);
		}
#ifdef CONFIG_AMLOGIC_COMMON_CLK_SCPI
		mutex_unlock(&cluster_lock[0]);
#else
		mutex_unlock(&cluster_lock[new_cluster]);
#endif
	}

	return 0;
}

static int meson_regulator_set_volate(struct regulator *regulator, int old_uv,
			int new_uv, int tol_uv)
{
	int cur, to, vol_cnt = 0;
	int ret = 0;
	int temp_uv = 0;
	struct regulator_dev *rdev = regulator->rdev;

	cur = regulator_map_voltage_iterate(rdev, old_uv, old_uv + tol_uv);
	to = regulator_map_voltage_iterate(rdev, new_uv, new_uv + tol_uv);
	vol_cnt = regulator_count_voltages(regulator);
	pr_debug("%s:old_uv:%d,cur:%d----->new_uv:%d,to:%d,vol_cnt=%d\n",
		__func__, old_uv, cur, new_uv, to, vol_cnt);

	if (to >= vol_cnt)
		to = vol_cnt - 1;

	if (cur < 0 || cur >= vol_cnt) {
		temp_uv = regulator_list_voltage(regulator, to);
		ret = regulator_set_voltage_tol(regulator, temp_uv, temp_uv
					+ tol_uv);
		udelay(200);
		return ret;
	}

	while (cur != to) {
		/* adjust to target voltage step by step */
		if (cur < to) {
			if (cur < to - 3)
				cur += 3;
			else
				cur = to;
		} else {
			if (cur > to + 3)
				cur -= 3;
			else
				cur = to;
		}
		temp_uv = regulator_list_voltage(regulator, cur);
		ret = regulator_set_voltage_tol(regulator, temp_uv,
			temp_uv + tol_uv);

		pr_debug("%s:temp_uv:%d, cur:%d, change_cur_uv:%d\n", __func__,
			temp_uv, cur, regulator_get_voltage(regulator));
		udelay(200);
	}
	return ret;

}

/* Set clock frequency */
static int meson_bL_cpufreq_set_target(struct cpufreq_policy *policy,
		unsigned int index)
{
	struct dev_pm_opp *opp;
	u32 cpu = policy->cpu, cur_cluster, new_cluster, actual_cluster;
	unsigned long int freq_new, freq_old;
	unsigned int volt_new = 0, volt_old = 0, volt_tol = 0;
	struct meson_cpufreq_driver_data *cpufreq_data;
	struct device *cpu_dev;
	struct regulator *cpu_reg;
	int ret = 0;

	if (!policy) {
		pr_err("invalid policy, returning\n");
		return -ENODEV;
	}
	cpufreq_data = policy->driver_data;
	cpu_dev = cpufreq_data->cpu_dev;
	cpu_reg = cpufreq_data->reg;
	cur_cluster = cpu_to_cluster(cpu);
	new_cluster = actual_cluster = per_cpu(physical_cluster, cpu);

	pr_debug("setting target for cpu %d, index =%d\n", policy->cpu, index);

	freq_new = freq_table[cur_cluster][index].frequency*1000;

	if (!IS_ERR(cpu_reg)) {
		rcu_read_lock();
		opp = dev_pm_opp_find_freq_ceil(cpu_dev, &freq_new);
		if (IS_ERR(opp)) {
			rcu_read_unlock();
			pr_err("failed to find OPP for %lu Khz\n",
					freq_new / 1000);
			return PTR_ERR(opp);
		}
		volt_new = dev_pm_opp_get_voltage(opp);
		rcu_read_unlock();
		volt_old = regulator_get_voltage(cpu_reg);
		volt_tol = volt_new * cpufreq_data->volt_tol / 100;
		pr_debug("Found OPP: %lu kHz, %u, tolerance: %u\n",
			freq_new / 1000, volt_new, volt_tol);
	}

	if (is_bL_switching_enabled()) {
		per_cpu(cpu_last_req_freq, policy->cpu) =
			clk_get_cpu_rate(policy->cpu);
		if ((actual_cluster == A15_CLUSTER) &&
				(freq_new < clk_big_min)) {
			new_cluster = A7_CLUSTER;
		} else if ((actual_cluster == A7_CLUSTER) &&
				(freq_new > clk_little_max)) {
			new_cluster = A15_CLUSTER;
		}
	} else
		freq_old = clk_get_rate(clk[cur_cluster]);

	pr_debug("Scalling from %lu MHz, %u mV,cur_cluster_id:%u, --> %lu MHz, %u mV,new_cluster_id:%u\n",
		freq_old / 1000000, (volt_old > 0) ? volt_old / 1000 : -1,
		cur_cluster,
		freq_new / 1000000, volt_new ? volt_new / 1000 : -1,
		new_cluster);

	/*cpufreq up,change voltage before frequency*/
	if (freq_new > freq_old) {
		ret = meson_regulator_set_volate(cpu_reg, volt_old,
			volt_new, volt_tol);
		if (ret) {
			pr_err("failed to scale voltage %u %u up: %d\n",
				volt_new, volt_tol, ret);
			return ret;
		}
	}

	/*scale clock frequency*/
	ret = meson_bL_cpufreq_set_rate(policy, actual_cluster, new_cluster,
					freq_new / 1000);
	if (ret) {
		pr_err("failed to set clock %luMhz rate: %d\n",
					freq_new / 1000000, ret);
		if ((volt_old > 0) && (freq_new > freq_old)) {
			pr_debug("scaling to old voltage %u\n", volt_old);
			meson_regulator_set_volate(cpu_reg, volt_old, volt_old,
				volt_tol);
		}
		return ret;
	}
	/*cpufreq down,change voltage after frequency*/
	if (freq_new < freq_old) {
		ret = meson_regulator_set_volate(cpu_reg, volt_old,
			volt_new, volt_tol);
		if (ret) {
			pr_err("failed to scale volt %u %u down: %d\n",
				volt_new, volt_tol, ret);
			meson_bL_cpufreq_set_rate(policy, actual_cluster,
				actual_cluster,	freq_old / 1000);
		}
	}

	pr_debug("After transition, new lk rate %luMhz, volt %dmV\n",
		clk_get_rate(clk[cur_cluster]) / 1000000,
		regulator_get_voltage(cpu_reg) / 1000);
	return ret;
}


static inline u32 get_table_count(struct cpufreq_frequency_table *table)
{
	int count;

	for (count = 0; table[count].frequency != CPUFREQ_TABLE_END; count++)
		;

	return count;
}

/* get the minimum frequency in the cpufreq_frequency_table */
static inline u32 get_table_min(struct cpufreq_frequency_table *table)
{
	struct cpufreq_frequency_table *pos;
	uint32_t min_freq = ~0;

	cpufreq_for_each_entry(pos, table)
		if (pos->frequency < min_freq)
			min_freq = pos->frequency;
	return min_freq;
}

/* get the maximum frequency in the cpufreq_frequency_table */
static inline u32 get_table_max(struct cpufreq_frequency_table *table)
{
	struct cpufreq_frequency_table *pos;
	uint32_t max_freq = 0;

	cpufreq_for_each_entry(pos, table)
		if (pos->frequency > max_freq)
			max_freq = pos->frequency;
	return max_freq;
}

static int merge_cluster_tables(void)
{
	int i, j, k = 0, count = 1;
	struct cpufreq_frequency_table *table;

	for (i = 0; i < MAX_CLUSTERS; i++)
		count += get_table_count(freq_table[i]);

	table = kcalloc(count, sizeof(*table), GFP_KERNEL);
	if (!table)
		return -ENOMEM;

	freq_table[MAX_CLUSTERS] = table;

	/* Add in reverse order to get freqs in increasing order */
	for (i = MAX_CLUSTERS - 1; i >= 0; i--) {
		for (j = 0; freq_table[i][j].frequency != CPUFREQ_TABLE_END;
				j++) {
			table[k].frequency = VIRT_FREQ(i,
					freq_table[i][j].frequency);
			pr_debug("%s: index: %d, freq: %d\n", __func__, k,
					table[k].frequency);
			k++;
		}
	}

	table[k].driver_data = k;
	table[k].frequency = CPUFREQ_TABLE_END;

	pr_debug("%s: End, table: %p, count: %d\n", __func__, table, k);

	return 0;
}

static void _put_cluster_clk_and_freq_table(struct device *cpu_dev,
					    const struct cpumask *cpumask)
{
	u32 cluster = raw_cpu_to_cluster(cpu_dev->id);

	if (!freq_table[cluster])
		return;

	clk_put(clk[cluster]);
	dev_pm_opp_free_cpufreq_table(cpu_dev, &freq_table[cluster]);
	if (arm_bL_ops->free_opp_table)
		arm_bL_ops->free_opp_table(cpumask);
	dev_dbg(cpu_dev, "%s: cluster: %d\n", __func__, cluster);
}

static void put_cluster_clk_and_freq_table(struct device *cpu_dev,
					   const struct cpumask *cpumask)
{
	u32 cluster = cpu_to_cluster(cpu_dev->id);
	int i;

	if (atomic_dec_return(&cluster_usage[cluster]))
		return;

	if (cluster < MAX_CLUSTERS)
		return _put_cluster_clk_and_freq_table(cpu_dev, cpumask);

	for_each_present_cpu(i) {
		struct device *cdev = get_cpu_device(i);

		if (!cdev) {
			pr_err("%s: failed to get cpu%d device\n", __func__, i);
			return;
		}

		_put_cluster_clk_and_freq_table(cdev, cpumask);
	}

	/* free virtual table */
	kfree(freq_table[cluster]);
}


static int _get_cluster_clk_and_freq_table(struct device *cpu_dev,
					   const struct cpumask *cpumask)
{
	u32 cluster = raw_cpu_to_cluster(cpu_dev->id);
	int ret;

	if (freq_table[cluster])
		return 0;

	ret = arm_bL_ops->init_opp_table(cpumask);
	if (ret) {
		dev_err(cpu_dev, "%s: init_opp_table failed, cpu: %d, err: %d\n",
				__func__, cpu_dev->id, ret);
		goto out;
	}

	ret = dev_pm_opp_init_cpufreq_table(cpu_dev, &freq_table[cluster]);
	if (ret) {
		dev_err(cpu_dev, "%s: failed to init cpufreq table, cpu: %d, err: %d\n",
				__func__, cpu_dev->id, ret);
		goto free_opp_table;
	}

	clk[cluster] = of_clk_get_by_name(of_node_get(cpu_dev->of_node),
		CORE_CLK);
	if (!IS_ERR(clk[cluster])) {
		dev_dbg(cpu_dev, "%s: clk: %p & freq table: %p, cluster: %d\n",
				__func__, clk[cluster], freq_table[cluster],
				cluster);
		return 0;
	}

	dev_err(cpu_dev, "%s: Failed to get clk for cpu: %d, cluster: %d\n",
			__func__, cpu_dev->id, cluster);

	ret = PTR_ERR(clk[cluster]);
	dev_pm_opp_free_cpufreq_table(cpu_dev, &freq_table[cluster]);

free_opp_table:
	if (arm_bL_ops->free_opp_table)
		arm_bL_ops->free_opp_table(cpumask);
out:
	dev_err(cpu_dev, "%s: Failed to get data for cluster: %d\n", __func__,
			cluster);
	return ret;
}

static int get_cluster_clk_and_freq_table(struct device *cpu_dev,
					  const struct cpumask *cpumask)
{
	u32 cluster = cpu_to_cluster(cpu_dev->id);
	int i, ret;

	if (atomic_inc_return(&cluster_usage[cluster]) != 1)
		return 0;

	if (cluster < MAX_CLUSTERS) {
		ret = _get_cluster_clk_and_freq_table(cpu_dev, cpumask);
		if (ret)
			atomic_dec(&cluster_usage[cluster]);
		return ret;
	}

	/*
	 * Get data for all clusters and fill virtual cluster with a merge of
	 * both
	 */

	for_each_present_cpu(i) {
		struct device *cdev = get_cpu_device(i);

		if (!cdev) {
			pr_err("%s: failed to get cpu%d device\n", __func__, i);
			return -ENODEV;
		}

		ret = _get_cluster_clk_and_freq_table(cdev, cpumask);
		if (ret)
			goto put_clusters;
	}

	ret = merge_cluster_tables();
	if (ret)
		goto put_clusters;

	/* Assuming 2 cluster, set clk_big_min and clk_little_max */
	clk_big_min = get_table_min(freq_table[0]);
	clk_little_max = VIRT_FREQ(1, get_table_max(freq_table[1]));

	pr_debug("%s: cluster: %d, clk_big_min: %d, clk_little_max: %d\n",
			__func__, cluster, clk_big_min, clk_little_max);

	return 0;

put_clusters:
	for_each_present_cpu(i) {
		struct device *cdev = get_cpu_device(i);

		if (!cdev) {
			pr_err("%s: failed to get cpu%d device\n", __func__, i);
			return -ENODEV;
		}

		_put_cluster_clk_and_freq_table(cdev, cpumask);
	}

	atomic_dec(&cluster_usage[cluster]);

	return ret;
}


/* Per-CPU initialization */
static int meson_bL_cpufreq_init(struct cpufreq_policy *policy)
{
	u32 cur_cluster = cpu_to_cluster(policy->cpu);
	struct dev_pm_opp *opp;
	struct device *cpu_dev;
	struct device_node *np;
	struct regulator *cpu_reg = NULL;
	struct meson_cpufreq_driver_data *cpufreq_data;
	struct clk *low_freq_clk_p, *high_freq_clk_p = NULL;
	unsigned int volt_new = 0, volt_old = 0, volt_tol = 0;
	unsigned long freq_hz = 0;
	int cpu = 0;
	int ret = 0;

	if (!policy) {
		pr_err("invalid cpufreq_policy\n");
		return -ENODEV;
	}

	cpu = policy->cpu;
	cpu_dev = get_cpu_device(cpu);
	if (IS_ERR(cpu_dev)) {
		pr_err("%s: failed to get cpu%d device\n", __func__,
				policy->cpu);
		return -ENODEV;
	}

	np = of_node_get(cpu_dev->of_node);
	if (!np) {
		pr_err("ERROR failed to find cpu%d node\n", cpu);
		return -ENOENT;
	}

	cpufreq_data = kzalloc(sizeof(*cpufreq_data), GFP_KERNEL);
	if (IS_ERR(cpufreq_data)) {
		pr_err("%s: failed to alloc cpufreq data!\n", __func__);
		return -ENOMEM;
		goto free_np;
	}

	low_freq_clk_p = of_clk_get_by_name(np, LOW_FREQ_CLK_PARENT);
	if (IS_ERR(low_freq_clk_p)) {
		pr_err("%s: Failed to get low parent for cpu: %d, cluster: %d\n",
			__func__, cpu_dev->id, cur_cluster);
		goto free_clk;
	}

	high_freq_clk_p = of_clk_get_by_name(np, HIGH_FREQ_CLK_PARENT);
	if (IS_ERR(high_freq_clk_p)) {
		pr_err("%s: Failed to get high parent for cpu: %d,cluster: %d\n",
			__func__, cpu_dev->id, cur_cluster);
		goto free_mem;
	}

	cpu_reg = devm_regulator_get(cpu_dev, CORE_SUPPLY);
	if (IS_ERR(cpu_reg)) {
		pr_err("%s:failed to get regulator, %ld\n", __func__,
			PTR_ERR(cpu_reg));
		ret = PTR_ERR(cpu_reg);
		goto free_clk;
	}

	if (of_property_read_u32(np, "voltage-tolerance", &volt_tol))
		volt_tol = DEF_VOLT_TOL;
	pr_info("value of voltage_tolerance %u\n", volt_tol);

	if (cur_cluster < MAX_CLUSTERS) {
		int cpu;

		cpumask_copy(policy->cpus, topology_core_cpumask(policy->cpu));
		for_each_cpu(cpu, policy->cpus)
			per_cpu(physical_cluster, cpu) = cur_cluster;
	} else {
		/* Assumption: during init, we are always running on A15 */
		per_cpu(physical_cluster, policy->cpu) = A15_CLUSTER;
	}

	ret = get_cluster_clk_and_freq_table(cpu_dev, policy->cpus);

	if (ret)
		return ret;

	ret = cpufreq_table_validate_and_show(policy, freq_table[cur_cluster]);
	if (ret) {
		dev_err(cpu_dev, "CPU %d, cluster: %d invalid freq table\n",
			policy->cpu, cur_cluster);
		put_cluster_clk_and_freq_table(cpu_dev, policy->cpus);
		return ret;
	}

	if (arm_bL_ops->get_transition_latency)
		policy->cpuinfo.transition_latency =
			arm_bL_ops->get_transition_latency(cpu_dev);
	else
		policy->cpuinfo.transition_latency = CPUFREQ_ETERNAL;

	cpufreq_data->cpu_dev = cpu_dev;
	cpufreq_data->low_freq_clk_p = low_freq_clk_p;
	cpufreq_data->high_freq_clk_p = high_freq_clk_p;
	cpufreq_data->reg = cpu_reg;
	cpufreq_data->volt_tol = volt_tol;
	policy->driver_data = cpufreq_data;
	policy->suspend_freq = get_table_max(freq_table[0]);

	if (is_bL_switching_enabled())
		per_cpu(cpu_last_req_freq, policy->cpu) =
					clk_get_cpu_rate(policy->cpu);
	else
		policy->cur = clk_get_rate(clk[cur_cluster]) / 1000;

	/*
	 * if uboot default cpufreq larger than freq_table's max,
	 * it will set freq_table's max.
	 */
	if (policy->cur > policy->suspend_freq)
		freq_hz = policy->suspend_freq*1000;
	else
		freq_hz =  policy->cur*1000;

	opp = dev_pm_opp_find_freq_ceil(cpu_dev, &freq_hz);
	volt_new = dev_pm_opp_get_voltage(opp);
	volt_old = regulator_get_voltage(cpu_reg);
	volt_tol = volt_new * cpufreq_data->volt_tol / 100;
	ret = meson_regulator_set_volate(cpu_reg, volt_old, volt_new, volt_tol);

	dev_info(cpu_dev, "%s: CPU %d initialized\n", __func__, policy->cpu);

	goto free_np;

free_clk:
		if (!IS_ERR(low_freq_clk_p))
			clk_put(low_freq_clk_p);
		if (!IS_ERR(high_freq_clk_p))
			clk_put(high_freq_clk_p);
free_mem:
		kfree(cpufreq_data);
free_np:
	if (!np)
		of_node_put(np);
	return ret;
}

static int meson_bL_cpufreq_exit(struct cpufreq_policy *policy)
{
	struct device *cpu_dev;
	struct sprd_cpufreq_driver_data *cpufreq_data;
	int cur_cluster = cpu_to_cluster(policy->cpu);

	cpufreq_data = policy->driver_data;
	if (cpufreq_data == NULL)
		return 0;

	if (cur_cluster < MAX_CLUSTERS) {
		cpufreq_cooling_unregister(cdev[cur_cluster]);
		cdev[cur_cluster] = NULL;
	}

	cpu_dev = get_cpu_device(policy->cpu);
	if (!cpu_dev) {
		pr_err("%s: failed to get cpu%d device\n", __func__,
				policy->cpu);
		return -ENODEV;
	}

	put_cluster_clk_and_freq_table(cpu_dev, policy->related_cpus);
	dev_dbg(cpu_dev, "%s: Exited, cpu: %d\n", __func__, policy->cpu);
	kfree(cpufreq_data);

	return 0;
}

static int meson_cpufreq_suspend(struct cpufreq_policy *policy)
{

		return cpufreq_generic_suspend(policy);
}

static int meson_cpufreq_resume(struct cpufreq_policy *policy)
{
	return cpufreq_generic_suspend(policy);

}

static struct cpufreq_driver meson_cpufreq_driver = {
	.name			= "arm-big-little",
	.flags			= CPUFREQ_STICKY |
					CPUFREQ_HAVE_GOVERNOR_PER_POLICY |
					CPUFREQ_NEED_INITIAL_FREQ_CHECK,
	.verify			= cpufreq_generic_frequency_table_verify,
	.target_index	= meson_bL_cpufreq_set_target,
	.get			= meson_bL_cpufreq_get_rate,
	.init			= meson_bL_cpufreq_init,
	.exit			= meson_bL_cpufreq_exit,
	.attr			= cpufreq_generic_attr,
	.suspend		= meson_cpufreq_suspend,
	.resume			= meson_cpufreq_resume,
};

#ifdef CONFIG_BL_SWITCHER
static int bL_cpufreq_switcher_notifier(struct notifier_block *nfb,
					unsigned long action, void *_arg)
{
	pr_debug("%s: action: %ld\n", __func__, action);

	switch (action) {
	case BL_NOTIFY_PRE_ENABLE:
	case BL_NOTIFY_PRE_DISABLE:
		cpufreq_unregister_driver(&bL_cpufreq_driver);
		break;

	case BL_NOTIFY_POST_ENABLE:
		set_switching_enabled(true);
		cpufreq_register_driver(&bL_cpufreq_driver);
		break;

	case BL_NOTIFY_POST_DISABLE:
		set_switching_enabled(false);
		cpufreq_register_driver(&bL_cpufreq_driver);
		break;

	default:
		return NOTIFY_DONE;
	}

	return NOTIFY_OK;
}

static struct notifier_block bL_switcher_notifier = {
	.notifier_call = bL_cpufreq_switcher_notifier,
};

static int meson_cpufreq_register_notifier(void)
{
	return bL_switcher_register_notifier(&bL_switcher_notifier);
}

static int meson_cpufreq_unregister_notifier(void)
{
	return bL_switcher_unregister_notifier(&bL_switcher_notifier);
}
#else
static int meson_cpufreq_register_notifier(void) { return 0; }
static int meson_cpufreq_unregister_notifier(void) { return 0; }
#endif

int meson_cpufreq_register(struct cpufreq_arm_bL_ops *ops)
{
	int ret, i;

	if (arm_bL_ops) {
		pr_debug("%s: Already registered: %s, exiting\n", __func__,
				arm_bL_ops->name);
		return -EBUSY;
	}

	if (!ops || !strlen(ops->name) || !ops->init_opp_table) {
		pr_err("%s: Invalid arm_bL_ops, exiting\n", __func__);
		return -ENODEV;
	}

	arm_bL_ops = ops;
	set_switching_enabled(bL_switcher_get_enabled());

	for (i = 0; i < MAX_CLUSTERS; i++)
		mutex_init(&cluster_lock[i]);

	ret = cpufreq_register_driver(&meson_cpufreq_driver);
	if (ret) {
		pr_err("%s: Failed registering platform driver: %s, err: %d\n",
				__func__, ops->name, ret);
		arm_bL_ops = NULL;
	} else {
		ret = meson_cpufreq_register_notifier();
		if (ret) {
			cpufreq_unregister_driver(&meson_cpufreq_driver);
			arm_bL_ops = NULL;
		} else {
			pr_err("%s: Registered platform driver: %s\n",
					__func__, ops->name);
		}
	}

	bL_switcher_put_enabled();
	return ret;
}
EXPORT_SYMBOL_GPL(meson_cpufreq_register);

void meson_cpufreq_unregister(struct cpufreq_arm_bL_ops *ops)
{
	if (arm_bL_ops != ops) {
		pr_err("%s: Registered with: %s, can't unregister, exiting\n",
				__func__, arm_bL_ops->name);
		return;
	}

	bL_switcher_get_enabled();
	meson_cpufreq_unregister_notifier();
	cpufreq_unregister_driver(&meson_cpufreq_driver);
	bL_switcher_put_enabled();
	pr_info("%s: Un-registered platform driver: %s\n", __func__,
			arm_bL_ops->name);
	arm_bL_ops = NULL;
}
EXPORT_SYMBOL_GPL(meson_cpufreq_unregister);


static struct cpufreq_arm_bL_ops meson_dt_bL_ops = {
	.name	= "meson_dt-bl",
	.get_transition_latency = dt_get_transition_latency,
	.init_opp_table = dev_pm_opp_of_cpumask_add_table,
	.free_opp_table = dev_pm_opp_of_cpumask_remove_table,
};

static int meson_cpufreq_probe(struct platform_device *pdev)
{
	struct device *cpu_dev;
	struct device_node *np;
	struct regulator *cpu_reg = NULL;
	unsigned int cpu = 0;

	cpu_dev = get_cpu_device(cpu);
	if (!cpu_dev) {
		pr_err("failed to get cpu%d device\n", cpu);
		return -ENODEV;
	}

	np = of_node_get(cpu_dev->of_node);
	if (!np) {
		pr_err("failed to find cpu node\n");
		of_node_put(np);
		return -ENODEV;
	}

	cpu_reg = devm_regulator_get(cpu_dev, CORE_SUPPLY);
	if (IS_ERR(cpu_reg)) {
		pr_err("failed  in regulator getting %ld\n",
				PTR_ERR(cpu_reg));
		devm_regulator_put(cpu_reg);
		return PTR_ERR(cpu_reg);
	}

	return meson_cpufreq_register(&meson_dt_bL_ops);
}

static int meson_cpufreq_remove(struct platform_device *pdev)
{
	meson_cpufreq_unregister(&meson_dt_bL_ops);
	return 0;
}

static const struct of_device_id amlogic_cpufreq_meson_dt_match[] = {
	{	.compatible = "amlogic, cpufreq-meson",
	},
	{},
};

static struct platform_driver meson_cpufreq_platdrv = {
	.driver = {
		.name	= "cpufreq-meson",
		.owner  = THIS_MODULE,
		.of_match_table = amlogic_cpufreq_meson_dt_match,
	},
	.probe		= meson_cpufreq_probe,
	.remove		= meson_cpufreq_remove,
};
module_platform_driver(meson_cpufreq_platdrv);

MODULE_AUTHOR("Amlogic cpufreq driver owner");
MODULE_DESCRIPTION("Generic ARM big LITTLE cpufreq driver via DT");
MODULE_LICENSE("GPL v2");
