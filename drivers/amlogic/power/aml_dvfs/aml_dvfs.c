/*
 * drivers/amlogic/power/aml_dvfs/aml_dvfs.c
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

#include <linux/cpufreq.h>
#include <linux/amlogic/aml_dvfs.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/cpu.h>
#include <linux/pm_opp.h>

#define DVFS_DBG(format, args...) \
	({if (1) pr_info("[DVFS]"format, ##args); })

#define DVFS_WARN(format, args...) \
	({if (1) pr_info("[DVFS]"format, ##args); })

#define DEBUG_DVFS		0

DEFINE_MUTEX(driver_mutex);
/*
 * @id: id of dvfs source
 * @driver: voltage scale driver for this source
 * @table: dvfs table
 */
struct aml_dvfs_master {
	unsigned int			id;
	struct aml_dvfs_driver		*driver;
	struct mutex			mutex;
	struct list_head		list;
};

LIST_HEAD(__aml_dvfs_list);

int aml_dvfs_register_driver(struct aml_dvfs_driver *driver)
{
	struct list_head *element;
	struct aml_dvfs_master *master = NULL, *target = NULL;

	if (driver == NULL) {
		DVFS_DBG("%s, NULL input of driver\n", __func__);
		return -EINVAL;
	}
	mutex_lock(&driver_mutex);
	list_for_each(element, &__aml_dvfs_list) {
		master = list_entry(element, struct aml_dvfs_master, list);
		if (!master)
			continue;

		if (driver->id_mask & master->id) {
			/* driver support for this dvfs source */
			target = master;
			break;
		}
	}
	if (!target)
		return -ENODEV;

	if (target->driver) {
		DVFS_DBG("%s, source id %x has driver %s, reject %s\n",
			 __func__, target->id, target->driver->name,
			 driver->name);
		return -EINVAL;
	}
	target->driver = driver;
	DVFS_DBG("%s, %s regist success, mask:%x, source id:%x\n",
		 __func__, driver->name,
		 driver->id_mask, target->id);
	mutex_unlock(&driver_mutex);

	return 0;
}
EXPORT_SYMBOL_GPL(aml_dvfs_register_driver);

int aml_dvfs_unregister_driver(struct aml_dvfs_driver *driver)
{
	struct list_head *element;
	struct aml_dvfs_master *master;
	int ok = 0;

	if (driver == NULL)
		return -EINVAL;

	mutex_lock(&driver_mutex);
	list_for_each(element, &__aml_dvfs_list) {
		master = list_entry(element, struct aml_dvfs_master, list);
		if (master && master->driver == driver) {
			DVFS_DBG("%s, driver %s unregist success\n",
				 __func__, master->driver->name);
			master->driver = NULL;
			ok = 1;
		}
	}
	mutex_unlock(&driver_mutex);
	if (!ok)
		DVFS_DBG("%s, driver %s not found\n", __func__, driver->name);

	return 0;
}
EXPORT_SYMBOL_GPL(aml_dvfs_unregister_driver);

static int aml_dvfs_do_voltage_change(struct aml_dvfs_master *master,
				      uint32_t new_freq,
				      uint32_t old_freq,
				      uint32_t uv,
				      uint32_t flags)
{
	uint32_t id = master->id;
	uint32_t curr_voltage = 0;
	int ret = 0;

	if (master->driver == NULL) {
		DVFS_WARN("%s, no dvfs driver\n", __func__);
		goto error;
	}
	/*
	 * update voltage
	 */
	if ((flags == AML_DVFS_FREQ_PRECHANGE  && new_freq >= old_freq) ||
	    (flags == AML_DVFS_FREQ_POSTCHANGE && new_freq <= old_freq)) {
		if (master->driver->get_voltage) {
			master->driver->get_voltage(id, &curr_voltage);
			/* in range, do not change */
			if (curr_voltage == uv) {
			#if DEBUG_DVFS
				DVFS_WARN("%s, voltage %d is in [%d, %d]\n",
					  __func__, curr_voltage,
					  uv, uv);
			#endif
				goto ok;
			}
		}
		if (master->driver->set_voltage) {
		#if DEBUG_DVFS
			DVFS_WARN("%s, freq [%u -> %u], voltage [%u -> %u]\n",
				  __func__, old_freq, new_freq,
				  curr_voltage, uv);
		#endif
			ret = master->driver->set_voltage(id, uv, uv);
		#if DEBUG_DVFS
			DVFS_WARN("%s, set voltage finished\n", __func__);
		#endif
		}
	}
ok:
	return ret;
error:
	return -EINVAL;
}

static int aml_dvfs_freq_change(unsigned int id,
				unsigned int new_freq,
				unsigned int old_freq,
				unsigned int uv,
				unsigned int flag)
{
	struct aml_dvfs_master  *m = NULL;
	struct list_head	*element;
	int ret = 0;

	list_for_each(element, &__aml_dvfs_list) {
		m = list_entry(element, struct aml_dvfs_master, list);
		if (m->id != id)
			break;
	}
	mutex_lock(&m->mutex);
	ret = aml_dvfs_do_voltage_change(m, new_freq, old_freq, uv, flag);
	mutex_unlock(&m->mutex);
	return ret;
}

static int aml_dvfs_cpufreq_nb_func(struct notifier_block *nb,
				    unsigned long action, void *data)
{
	struct cpufreq_freqs *freqs = data;
	struct device *dev;
	struct dev_pm_opp *opp_new, *opp_old;
	unsigned long voltage, freq_new, freq_old;
	int cpu;

	cpu = freqs->cpu;
	dev = get_cpu_device(cpu);
	if (!dev) {
		pr_info("%s, %d\n", __func__, __LINE__);
		return -EINVAL;
	}

	freq_new = freqs->new * 1000;
	freq_old = freqs->old * 1000;
	opp_new  = dev_pm_opp_find_freq_ceil(dev, &freq_new);
	opp_old  = dev_pm_opp_find_freq_ceil(dev, &freq_old);

	if (IS_ERR_OR_NULL(opp_new) || IS_ERR_OR_NULL(opp_old)) {
		pr_info("%s, %d\n", __func__, __LINE__);
		return -EINVAL;
	}
	freq_new = dev_pm_opp_get_freq(opp_new);
	freq_old = dev_pm_opp_get_freq(opp_old);
	voltage = dev_pm_opp_get_voltage(opp_new);
	switch (action) {
	case CPUFREQ_PRECHANGE:
		aml_dvfs_freq_change(AML_DVFS_ID_VCCK, freq_new,
				     freq_old, voltage,
				     AML_DVFS_FREQ_PRECHANGE);
		break;

	case CPUFREQ_POSTCHANGE:
		aml_dvfs_freq_change(AML_DVFS_ID_VCCK, freq_new,
				     freq_old, voltage,
				     AML_DVFS_FREQ_POSTCHANGE);
		break;

	default:
		break;
	}
	return 0;
}

static struct notifier_block aml_cpufreq_nb;

static int aml_dummy_set_voltage(uint32_t id, uint32_t min_uV, uint32_t max_uV)
{
	return 0;
}

struct aml_dvfs_driver aml_dummy_dvfs_driver = {
	.name		 = "aml-dumy-dvfs",
	.id_mask	 = 0,
	.set_voltage = aml_dummy_set_voltage,
	.get_voltage = NULL,
};

static int aml_dvfs_init_for_master(struct aml_dvfs_master *master)
{
	int ret = 0;

	mutex_init(&master->mutex);
	return ret;
}

static int aml_dvfs_probe(struct platform_device *pdev)
{
	struct device_node		*dvfs_node = pdev->dev.of_node;
	struct device_node		*child;
	struct aml_dvfs_master	*master;
	int   err;
	int   id = 0;

	for_each_child_of_node(dvfs_node, child) {
		DVFS_DBG("%s, child name:%s\n", __func__, child->name);
		/* read dvfs id */
		err = of_property_read_u32(child, "dvfs_id", &id);
		if (err) {
			DVFS_DBG("%s, get 'dvfs_id' failed\n", __func__);
			continue;
		}
		master = kzalloc(sizeof(*master), GFP_KERNEL);
		if (master == NULL) {
			DVFS_DBG("%s, allocate memory failed\n", __func__);
			return -ENOMEM;
		}
		master->id = id;
		/* get table count */
		list_add_tail(&master->list, &__aml_dvfs_list);
		if (aml_dvfs_init_for_master(master))
			return -EINVAL;
	}

	aml_cpufreq_nb.notifier_call = aml_dvfs_cpufreq_nb_func;
	err = cpufreq_register_notifier(&aml_cpufreq_nb,
					CPUFREQ_TRANSITION_NOTIFIER);

	return err;
}

static int aml_dvfs_remove(struct platform_device *pdev)
{
	struct list_head *element;
	struct aml_dvfs_master *master;

	cpufreq_unregister_notifier(&aml_cpufreq_nb,
				    CPUFREQ_TRANSITION_NOTIFIER);
	list_for_each(element, &__aml_dvfs_list) {
		master = list_entry(element, struct aml_dvfs_master, list);
		kfree(master);
	}
	return 0;
}

static const struct of_device_id aml_dvfs_dt_match[] = {
	{
		.compatible = "amlogic, amlogic-dvfs",
	},
	{}
};

static  struct platform_driver aml_dvfs_prober = {
	.probe   = aml_dvfs_probe,
	.remove  = aml_dvfs_remove,
	.driver  = {
		.name			= "amlogic-dvfs",
		.owner			= THIS_MODULE,
		.of_match_table = aml_dvfs_dt_match,
	},
};

static int __init aml_dvfs_init(void)
{
	int ret;

	pr_info("call %s in\n", __func__);
	ret = platform_driver_register(&aml_dvfs_prober);
	return ret;
}

static void __exit aml_dvfs_exit(void)
{
	platform_driver_unregister(&aml_dvfs_prober);
}

subsys_initcall(aml_dvfs_init);
module_exit(aml_dvfs_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Amlogic DVFS interface driver");
