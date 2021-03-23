// SPDX-License-Identifier: GPL-2.0-only
/*
 * linux/drivers/devfreq/governor_passive.c
 *
 * Copyright (C) 2016 Samsung Electronics
 * Author: Chanwoo Choi <cw00.choi@samsung.com>
 * Author: MyungJoo Ham <myungjoo.ham@samsung.com>
 */

#include <linux/module.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/cpumask.h>
#include <linux/device.h>
#include <linux/devfreq.h>
#include <linux/slab.h>
#include "governor.h"

struct devfreq_cpu_state {
	unsigned int curr_freq;
	unsigned int min_freq;
	unsigned int max_freq;
	unsigned int first_cpu;
	struct device *cpu_dev;
	struct opp_table *opp_table;
};

static unsigned long xlate_cpufreq_to_devfreq(struct devfreq_passive_data *data,
					      unsigned int cpu)
{
	unsigned int cpu_min_freq, cpu_max_freq, cpu_curr_freq_khz, cpu_percent;
	unsigned long dev_min_freq, dev_max_freq, dev_max_state;

	struct devfreq_cpu_state *cpu_state = data->cpu_state[cpu];
	struct devfreq *devfreq = (struct devfreq *)data->this;
	unsigned long *dev_freq_table = devfreq->profile->freq_table;
	struct dev_pm_opp *opp = NULL, *p_opp = NULL;
	unsigned long cpu_curr_freq, freq;

	if (!cpu_state || cpu_state->first_cpu != cpu ||
	    !cpu_state->opp_table || !devfreq->opp_table)
		return 0;

	cpu_curr_freq = cpu_state->curr_freq * 1000;
	p_opp = devfreq_recommended_opp(cpu_state->cpu_dev, &cpu_curr_freq, 0);
	if (IS_ERR(p_opp))
		return 0;

	opp = dev_pm_opp_xlate_required_opp(cpu_state->opp_table,
					    devfreq->opp_table, p_opp);
	dev_pm_opp_put(p_opp);

	if (!IS_ERR(opp)) {
		freq = dev_pm_opp_get_freq(opp);
		dev_pm_opp_put(opp);
		goto out;
	}

	/* Use Interpolation if required opps is not available */
	cpu_min_freq = cpu_state->min_freq;
	cpu_max_freq = cpu_state->max_freq;
	cpu_curr_freq_khz = cpu_state->curr_freq;

	if (dev_freq_table) {
		/* Get minimum frequency according to sorting order */
		dev_max_state = dev_freq_table[devfreq->profile->max_state - 1];
		if (dev_freq_table[0] < dev_max_state) {
			dev_min_freq = dev_freq_table[0];
			dev_max_freq = dev_max_state;
		} else {
			dev_min_freq = dev_max_state;
			dev_max_freq = dev_freq_table[0];
		}
	} else {
		dev_min_freq = dev_pm_qos_read_value(devfreq->dev.parent,
						     DEV_PM_QOS_MIN_FREQUENCY);
		dev_max_freq = dev_pm_qos_read_value(devfreq->dev.parent,
						     DEV_PM_QOS_MAX_FREQUENCY);

		if (dev_max_freq <= dev_min_freq)
			return 0;
	}
	cpu_percent = ((cpu_curr_freq_khz - cpu_min_freq) * 100) / cpu_max_freq - cpu_min_freq;
	freq = dev_min_freq + mult_frac(dev_max_freq - dev_min_freq, cpu_percent, 100);

out:
	return freq;
}

static int get_target_freq_with_cpufreq(struct devfreq *devfreq,
					unsigned long *freq)
{
	struct devfreq_passive_data *p_data =
				(struct devfreq_passive_data *)devfreq->data;
	unsigned int cpu;
	unsigned long target_freq = 0;

	for_each_online_cpu(cpu)
		target_freq = max(target_freq,
				  xlate_cpufreq_to_devfreq(p_data, cpu));

	*freq = target_freq;

	return 0;
}

static int get_target_freq_with_devfreq(struct devfreq *devfreq,
					unsigned long *freq)
{
	struct devfreq_passive_data *p_data
			= (struct devfreq_passive_data *)devfreq->data;
	struct devfreq *parent_devfreq = (struct devfreq *)p_data->parent;
	unsigned long child_freq = ULONG_MAX;
	struct dev_pm_opp *opp, *p_opp;
	int i, count;

	/*
	 * If the parent and passive devfreq device uses the OPP table,
	 * get the next frequency by using the OPP table.
	 */

	/*
	 * - parent devfreq device uses the governors except for passive.
	 * - passive devfreq device uses the passive governor.
	 *
	 * Each devfreq has the OPP table. After deciding the new frequency
	 * from the governor of parent devfreq device, the passive governor
	 * need to get the index of new frequency on OPP table of parent
	 * device. And then the index is used for getting the suitable
	 * new frequency for passive devfreq device.
	 */
	if (!devfreq->profile || !devfreq->profile->freq_table
		|| devfreq->profile->max_state <= 0)
		return -EINVAL;

	/*
	 * The passive governor have to get the correct frequency from OPP
	 * list of parent device. Because in this case, *freq is temporary
	 * value which is decided by ondemand governor.
	 */
	if (devfreq->opp_table && parent_devfreq->opp_table) {
		p_opp = devfreq_recommended_opp(parent_devfreq->dev.parent,
						freq, 0);
		if (IS_ERR(p_opp))
			return PTR_ERR(p_opp);

		opp = dev_pm_opp_xlate_required_opp(parent_devfreq->opp_table,
						    devfreq->opp_table, p_opp);
		dev_pm_opp_put(p_opp);

		if (IS_ERR(opp))
			goto no_required_opp;

		*freq = dev_pm_opp_get_freq(opp);
		dev_pm_opp_put(opp);

		return 0;
	}

no_required_opp:
	/*
	 * Get the OPP table's index of decided frequency by governor
	 * of parent device.
	 */
	for (i = 0; i < parent_devfreq->profile->max_state; i++)
		if (parent_devfreq->profile->freq_table[i] == *freq)
			break;

	if (i == parent_devfreq->profile->max_state)
		return -EINVAL;

	/* Get the suitable frequency by using index of parent device. */
	if (i < devfreq->profile->max_state) {
		child_freq = devfreq->profile->freq_table[i];
	} else {
		count = devfreq->profile->max_state;
		child_freq = devfreq->profile->freq_table[count - 1];
	}

	/* Return the suitable frequency for passive device. */
	*freq = child_freq;

	return 0;
}

static int devfreq_passive_get_target_freq(struct devfreq *devfreq,
					   unsigned long *freq)
{
	struct devfreq_passive_data *p_data =
		(struct devfreq_passive_data *)devfreq->data;
	int ret;

	/*
	 * If the devfreq device with passive governor has the specific method
	 * to determine the next frequency, should use the get_target_freq()
	 * of struct devfreq_passive_data.
	 */
	if (p_data->get_target_freq)
		return p_data->get_target_freq(devfreq, freq);

	switch (p_data->parent_type) {
	case DEVFREQ_PARENT_DEV:
		ret = get_target_freq_with_devfreq(devfreq, freq);
		break;
	case CPUFREQ_PARENT_DEV:
		ret = get_target_freq_with_cpufreq(devfreq, freq);
		break;
	default:
		ret = -EINVAL;
		dev_err(&devfreq->dev, "Invalid parent type\n");
		break;
	}

	return ret;
}

static int devfreq_passive_notifier_call(struct notifier_block *nb,
				unsigned long event, void *ptr)
{
	struct devfreq_passive_data *data
			= container_of(nb, struct devfreq_passive_data, nb);
	struct devfreq *devfreq = (struct devfreq *)data->this;
	struct devfreq *parent = (struct devfreq *)data->parent;
	struct devfreq_freqs *freqs = (struct devfreq_freqs *)ptr;
	unsigned long freq = freqs->new;
	int ret = 0;

	mutex_lock_nested(&devfreq->lock, SINGLE_DEPTH_NESTING);
	switch (event) {
	case DEVFREQ_PRECHANGE:
		if (parent->previous_freq > freq)
			ret = devfreq_update_target(devfreq, freq);

		break;
	case DEVFREQ_POSTCHANGE:
		if (parent->previous_freq < freq)
			ret = devfreq_update_target(devfreq, freq);
		break;
	}
	mutex_unlock(&devfreq->lock);

	if (ret < 0)
		dev_warn(&devfreq->dev,
			"failed to update devfreq using passive governor\n");

	return NOTIFY_DONE;
}

static int cpufreq_passive_notifier_call(struct notifier_block *nb,
					 unsigned long event, void *ptr)
{
	struct devfreq_passive_data *data =
			container_of(nb, struct devfreq_passive_data, nb);
	struct devfreq *devfreq = (struct devfreq *)data->this;
	struct devfreq_cpu_state *cpu_state;
	struct cpufreq_freqs *cpu_freq = ptr;
	unsigned int curr_freq;
	int ret;

	if (event != CPUFREQ_POSTCHANGE || !cpu_freq ||
	    !data->cpu_state[cpu_freq->policy->cpu])
		return 0;

	cpu_state = data->cpu_state[cpu_freq->policy->cpu];
	if (cpu_state->curr_freq == cpu_freq->new)
		return 0;

	/* Backup current freq and pre-update cpu state freq*/
	curr_freq = cpu_state->curr_freq;
	cpu_state->curr_freq = cpu_freq->new;

	mutex_lock(&devfreq->lock);
	ret = update_devfreq(devfreq);
	mutex_unlock(&devfreq->lock);
	if (ret) {
		cpu_state->curr_freq = curr_freq;
		dev_err(&devfreq->dev, "Couldn't update the frequency.\n");
		return ret;
	}

	return 0;
}

static int cpufreq_passive_register(struct devfreq_passive_data **p_data)
{
	struct devfreq_passive_data *data = *p_data;
	struct devfreq *devfreq = (struct devfreq *)data->this;
	struct device *dev = devfreq->dev.parent;
	struct opp_table *opp_table = NULL;
	struct devfreq_cpu_state *cpu_state;
	struct cpufreq_policy *policy;
	struct device *cpu_dev;
	unsigned int cpu;
	int ret;

	cpus_read_lock();

	data->nb.notifier_call = cpufreq_passive_notifier_call;
	ret = cpufreq_register_notifier(&data->nb,
					CPUFREQ_TRANSITION_NOTIFIER);
	if (ret) {
		dev_err(dev, "Couldn't register cpufreq notifier.\n");
		data->nb.notifier_call = NULL;
		goto out;
	}

	/* Populate devfreq_cpu_state */
	for_each_online_cpu(cpu) {
		if (data->cpu_state[cpu])
			continue;

		policy = cpufreq_cpu_get(cpu);
		if (!policy) {
			ret = -EINVAL;
			goto out;
		} else if (PTR_ERR(policy) == -EPROBE_DEFER) {
			ret = -EPROBE_DEFER;
			goto out;
		} else if (IS_ERR(policy)) {
			ret = PTR_ERR(policy);
			dev_err(dev, "Couldn't get the cpufreq_poliy.\n");
			goto out;
		}

		cpu_state = kzalloc(sizeof(*cpu_state), GFP_KERNEL);
		if (!cpu_state) {
			ret = -ENOMEM;
			goto out;
		}

		cpu_dev = get_cpu_device(cpu);
		if (!cpu_dev) {
			dev_err(dev, "Couldn't get cpu device.\n");
			ret = -ENODEV;
			goto out;
		}

		opp_table = dev_pm_opp_get_opp_table(cpu_dev);
		if (IS_ERR(devfreq->opp_table)) {
			ret = PTR_ERR(opp_table);
			goto out;
		}

		cpu_state->cpu_dev = cpu_dev;
		cpu_state->opp_table = opp_table;
		cpu_state->first_cpu = cpumask_first(policy->related_cpus);
		cpu_state->curr_freq = policy->cur;
		cpu_state->min_freq = policy->cpuinfo.min_freq;
		cpu_state->max_freq = policy->cpuinfo.max_freq;
		data->cpu_state[cpu] = cpu_state;

		cpufreq_cpu_put(policy);
	}

out:
	cpus_read_unlock();
	if (ret)
		return ret;

	/* Update devfreq */
	mutex_lock(&devfreq->lock);
	ret = update_devfreq(devfreq);
	mutex_unlock(&devfreq->lock);
	if (ret)
		dev_err(dev, "Couldn't update the frequency.\n");

	return ret;
}

static int cpufreq_passive_unregister(struct devfreq_passive_data **p_data)
{
	struct devfreq_passive_data *data = *p_data;
	struct devfreq_cpu_state *cpu_state;
	int cpu;

	if (data->nb.notifier_call)
		cpufreq_unregister_notifier(&data->nb,
					    CPUFREQ_TRANSITION_NOTIFIER);

	for_each_possible_cpu(cpu) {
		cpu_state = data->cpu_state[cpu];
		if (cpu_state) {
			if (cpu_state->opp_table)
				dev_pm_opp_put_opp_table(cpu_state->opp_table);
			kfree(cpu_state);
			cpu_state = NULL;
		}
	}

	return 0;
}

int register_parent_dev_notifier(struct devfreq_passive_data **p_data)
{
	struct notifier_block *nb = &(*p_data)->nb;
	int ret = 0;

	switch ((*p_data)->parent_type) {
	case DEVFREQ_PARENT_DEV:
		nb->notifier_call = devfreq_passive_notifier_call;
		ret = devfreq_register_notifier((struct devfreq *)(*p_data)->parent, nb,
						DEVFREQ_TRANSITION_NOTIFIER);
		break;
	case CPUFREQ_PARENT_DEV:
		ret = cpufreq_passive_register(p_data);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

int unregister_parent_dev_notifier(struct devfreq_passive_data **p_data)
{
	int ret = 0;

	switch ((*p_data)->parent_type) {
	case DEVFREQ_PARENT_DEV:
		WARN_ON(devfreq_unregister_notifier((struct devfreq *)(*p_data)->parent,
						    &(*p_data)->nb,
						    DEVFREQ_TRANSITION_NOTIFIER));
		break;
	case CPUFREQ_PARENT_DEV:
		cpufreq_passive_unregister(p_data);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int devfreq_passive_event_handler(struct devfreq *devfreq,
				unsigned int event, void *data)
{
	struct devfreq_passive_data *p_data
			= (struct devfreq_passive_data *)devfreq->data;
	struct devfreq *parent = (struct devfreq *)p_data->parent;
	int ret = 0;

	if (p_data->parent_type == DEVFREQ_PARENT_DEV && !parent)
		return -EPROBE_DEFER;

	switch (event) {
	case DEVFREQ_GOV_START:
		if (!p_data->this)
			p_data->this = devfreq;

		ret = register_parent_dev_notifier(&p_data);
		break;

	case DEVFREQ_GOV_STOP:
		ret = unregister_parent_dev_notifier(&p_data);
		break;
	default:
		break;
	}

	return ret;
}

static struct devfreq_governor devfreq_passive = {
	.name = DEVFREQ_GOV_PASSIVE,
	.flags = DEVFREQ_GOV_FLAG_IMMUTABLE,
	.get_target_freq = devfreq_passive_get_target_freq,
	.event_handler = devfreq_passive_event_handler,
};

static int __init devfreq_passive_init(void)
{
	return devfreq_add_governor(&devfreq_passive);
}
subsys_initcall(devfreq_passive_init);

static void __exit devfreq_passive_exit(void)
{
	int ret;

	ret = devfreq_remove_governor(&devfreq_passive);
	if (ret)
		pr_err("%s: failed remove governor %d\n", __func__, ret);
}
module_exit(devfreq_passive_exit);

MODULE_AUTHOR("Chanwoo Choi <cw00.choi@samsung.com>");
MODULE_AUTHOR("MyungJoo Ham <myungjoo.ham@samsung.com>");
MODULE_DESCRIPTION("DEVFREQ Passive governor");
MODULE_LICENSE("GPL v2");
