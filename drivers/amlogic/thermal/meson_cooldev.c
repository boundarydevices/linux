/*
 * drivers/amlogic/thermal/meson_cooldev.c
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

#include <linux/slab.h>
#include <linux/types.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/printk.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/cpufreq.h>
#include <linux/cpu_cooling.h>
#include <linux/amlogic/cpucore_cooling.h>
#include <linux/amlogic/gpucore_cooling.h>
#include <linux/amlogic/gpu_cooling.h>
#include <linux/cpu.h>

enum cluster_type {
	CLUSTER_BIG = 0,
	CLUSTER_LITTLE,
	NUM_CLUSTERS
};

enum cool_dev_type {
	COOL_DEV_TYPE_CPU_FREQ = 0,
	COOL_DEV_TYPE_CPU_CORE,
	COOL_DEV_TYPE_GPU_FREQ,
	COOL_DEV_TYPE_GPU_CORE,
	COOL_DEV_TYPE_MAX
};

struct cool_dev {
	int min_state;
	int coeff;
	int gpupp;
	int cluster_id;
	char *device_type;
	struct device_node *np;
	struct thermal_cooling_device *cooling_dev;
};

struct meson_cooldev {
	int chip_trimmed;
	int cool_dev_num;
	int min_exist;
	struct mutex lock;
	struct cpumask mask[NUM_CLUSTERS];
	struct cool_dev *cool_devs;
	struct thermal_zone_device    *tzd;
};

static struct meson_cooldev *meson_gcooldev;

static int get_cool_dev_type(char *type)
{
	if (!strcmp(type, "cpufreq"))
		return COOL_DEV_TYPE_CPU_FREQ;
	if (!strcmp(type, "cpucore"))
		return COOL_DEV_TYPE_CPU_CORE;
	if (!strcmp(type, "gpufreq"))
		return COOL_DEV_TYPE_GPU_FREQ;
	if (!strcmp(type, "gpucore"))
		return COOL_DEV_TYPE_GPU_CORE;
	return COOL_DEV_TYPE_MAX;
}

static struct cool_dev *get_cool_dev_by_node(struct platform_device *pdev,
						struct device_node *np)
{
	struct meson_cooldev *mcooldev = platform_get_drvdata(pdev);
	int i;
	struct cool_dev *dev;

	if (!np)
		return NULL;
	for (i = 0; i < mcooldev->cool_dev_num; i++) {
		dev = &mcooldev->cool_devs[i];
		if (dev->np == np)
			return dev;
	}
	return NULL;
}

static struct cool_dev *get_gcool_dev_by_node(struct meson_cooldev *mgcooldev,
						struct device_node *np)
{
	int i;
	struct cool_dev *dev;

	if (!meson_gcooldev) {
		pr_info("meson_gcooldev is null, no set min status\n");
		return NULL;
	}
	if (!np)
		return NULL;
	for (i = 0; i < mgcooldev->cool_dev_num; i++) {
		dev = &mgcooldev->cool_devs[i];
		if (dev->np == np)
			return dev;
	}
	return NULL;
}


static int meson_set_min_status(struct thermal_cooling_device *cdev,
				unsigned long min_state)
{
	struct device_node *tzdnp, *child, *coolmap, *gchild;
	struct thermal_zone_device *tzd = ERR_PTR(-ENODEV);
	struct device_node *np = cdev->np;
	int err = 0;

	tzdnp = of_find_node_by_name(NULL, "thermal-zones");
	if (!tzdnp)
		goto end;
	for_each_available_child_of_node(tzdnp, child) {
		coolmap = of_find_node_by_name(child, "cooling-maps");
		for_each_available_child_of_node(coolmap, gchild) {
			struct of_phandle_args cooling_spec;
			int ret;

			ret = of_parse_phandle_with_args(
					gchild,
					"cooling-device",
					"#cooling-cells",
					0,
					&cooling_spec);
			if (ret < 0) {
				pr_err("missing cooling_device property\n");
				goto end;
			}
			if (cooling_spec.np == np) {
				int i;

				tzd =
				thermal_zone_get_zone_by_name(child->name);
				pr_info("find tzd id: %d\n", tzd->id);
				for (i = 0; i < tzd->trips; i++)
					thermal_set_upper(tzd,
							cdev, i, min_state);
				err = 1;
			}
		}
	}
end:
	return err;
}

int meson_gcooldev_min_update(struct thermal_cooling_device *cdev)
{
	struct gpufreq_cooling_device *gf_cdev;
	struct gpucore_cooling_device *gc_cdev;
	struct cool_dev *cool;
	long min_state;
	int ret;

	cool = get_gcool_dev_by_node(meson_gcooldev, cdev->np);
	if (!cool)
		return -ENODEV;

	if (cool->cooling_dev == NULL)
		cool->cooling_dev = cdev;

	if (cool->min_state == 0)
		return 0;

	switch (get_cool_dev_type(cool->device_type)) {
	case COOL_DEV_TYPE_GPU_CORE:
		gc_cdev = (struct gpucore_cooling_device *)cdev->devdata;
		cdev->ops->get_max_state(cdev, &min_state);
		min_state = min_state - cool->min_state;
		break;

	case COOL_DEV_TYPE_GPU_FREQ:
		gf_cdev = (struct gpufreq_cooling_device *)cdev->devdata;
		min_state = gf_cdev->get_gpu_freq_level(cool->min_state);
		break;

	default:
		pr_info("can not find cool devices type\n");
		return -EINVAL;
	}

	ret = meson_set_min_status(cdev, min_state);
	if (!ret)
		pr_info("meson_cdev set min sussces\n");
	return 0;
}
EXPORT_SYMBOL(meson_gcooldev_min_update);

int meson_cooldev_min_update(struct platform_device *pdev, int index)
{
	struct meson_cooldev *mcooldev = platform_get_drvdata(pdev);
	struct cool_dev *cool = &mcooldev->cool_devs[index];
	struct thermal_cooling_device *cdev = cool->cooling_dev;
	struct gpufreq_cooling_device *gf_cdev;
	struct gpucore_cooling_device *gc_cdev;
	long min_state;
	int ret;
	int cpu, c_id;

	cool = get_cool_dev_by_node(pdev, cdev->np);
	if (!cool)
		return -ENODEV;

	if (!cdev)
		return -ENODEV;

	if (cool->min_state == 0)
		return 0;

	switch (get_cool_dev_type(cool->device_type)) {
	case COOL_DEV_TYPE_CPU_CORE:
		/* TODO: cluster ID */
		cool->cooling_dev->ops->get_max_state(cdev, &min_state);
		min_state = min_state - cool->min_state;
		break;

	case COOL_DEV_TYPE_CPU_FREQ:
		for_each_possible_cpu(cpu) {
			if (mc_capable())
				c_id = topology_physical_package_id(cpu);
			else
				c_id = 0; /* force cluster 0 if no MC */
			if (c_id == cool->cluster_id)
				break;
		}
		min_state = cpufreq_cooling_get_level(cpu, cool->min_state);
		break;

	case COOL_DEV_TYPE_GPU_CORE:
		gc_cdev = (struct gpucore_cooling_device *)cdev->devdata;
		cdev->ops->get_max_state(cdev, &min_state);
		min_state = min_state - cool->min_state;
		break;

	case COOL_DEV_TYPE_GPU_FREQ:
		gf_cdev = (struct gpufreq_cooling_device *)cdev->devdata;
		min_state = gf_cdev->get_gpu_freq_level(cool->min_state);
		break;

	default:
		return -EINVAL;
	}
	ret = meson_set_min_status(cdev, min_state);
	if (!ret)
		pr_info("meson_cdev set min sussces\n");
	return 0;
}
EXPORT_SYMBOL(meson_cooldev_min_update);


static int register_cool_dev(struct platform_device *pdev, int index)
{
	struct meson_cooldev *mcooldev = platform_get_drvdata(pdev);
	struct cool_dev *cool = &mcooldev->cool_devs[index];
	struct cpumask *mask;
	int id = cool->cluster_id;

	pr_info("meson_cdev index: %d\n", index);
	switch (get_cool_dev_type(cool->device_type)) {
	case COOL_DEV_TYPE_CPU_CORE:
		cool->cooling_dev = cpucore_cooling_register(cool->np,
							     cool->cluster_id);
		break;

	case COOL_DEV_TYPE_CPU_FREQ:
		mask = &mcooldev->mask[id];
		cool->cooling_dev = of_cpufreq_power_cooling_register(cool->np,
							mask,
							cool->coeff,
							NULL);
		break;
	/* GPU is KO, just save these parameters */
	case COOL_DEV_TYPE_GPU_FREQ:
		save_gpu_cool_para(cool->coeff, cool->np, cool->gpupp);
		return 0;

	case COOL_DEV_TYPE_GPU_CORE:
		save_gpucore_thermal_para(cool->np);
		return 0;

	default:
		pr_err("thermal: unknown type:%s\n", cool->device_type);
		return -EINVAL;
	}

	if (IS_ERR(cool->cooling_dev)) {
		pr_err("thermal: register %s failed\n", cool->device_type);
		cool->cooling_dev = NULL;
		return -EINVAL;
	}
	return 0;
}

static int parse_cool_device(struct platform_device *pdev)
{
	struct meson_cooldev *mcooldev = platform_get_drvdata(pdev);
	struct device_node *np = pdev->dev.of_node;
	int i, temp, ret = 0;
	struct cool_dev *cool;
	struct device_node *node, *child;
	const char *str;

	child = of_get_child_by_name(np, "cooling_devices");
	if (child == NULL) {
		pr_err("meson cooldev: can't found cooling_devices\n");
		return -EINVAL;
	}
	mcooldev->cool_dev_num = of_get_child_count(child);
	i = sizeof(struct cool_dev) * mcooldev->cool_dev_num;
	mcooldev->cool_devs = kzalloc(i, GFP_KERNEL);
	if (mcooldev->cool_devs == NULL) {
		pr_err("meson cooldev: alloc mem failed\n");
		return -ENOMEM;
	}

	child = of_get_next_child(child, NULL);
	for (i = 0; i < mcooldev->cool_dev_num; i++) {
		cool = &mcooldev->cool_devs[i];
		if (child == NULL)
			break;
		if (of_property_read_u32(child, "min_state", &temp))
			pr_err("thermal: read min_state failed\n");
		else
			cool->min_state = temp;

		if (of_property_read_u32(child, "dyn_coeff", &temp))
			pr_err("thermal: read dyn_coeff failed\n");
		else
			cool->coeff = temp;

		if (of_property_read_u32(child, "gpu_pp", &temp))
			pr_err("thermal: read gpupp failed\n");
		else
			cool->gpupp = temp;

		if (of_property_read_u32(child, "cluster_id", &temp))
			pr_err("thermal: read cluster_id failed\n");
		else
			cool->cluster_id = temp;

		if (of_property_read_string(child, "device_type", &str))
			pr_err("thermal: read device_type failed\n");
		else
			cool->device_type = (char *)str;

		if (of_property_read_string(child, "node_name", &str))
			pr_err("thermal: read node_name failed\n");
		else {
			node = of_find_node_by_name(NULL, str);
			if (!node)
				pr_err("thermal: can't find node\n");
			cool->np = node;
		}
		if (cool->np)
			ret += register_cool_dev(pdev, i);
		child = of_get_next_child(np, child);
	}
	return ret;
}

static int meson_cooldev_probe(struct platform_device *pdev)
{
	int cpu, i, c_id;
	struct cool_dev *cool;
	struct meson_cooldev *mcooldev;
	struct cpufreq_policy *policy;

	pr_info("meson_cdev probe\n");
	mcooldev = devm_kzalloc(&pdev->dev, sizeof(struct meson_cooldev),
					GFP_KERNEL);
	if (!mcooldev)
		return -ENOMEM;
	platform_set_drvdata(pdev, mcooldev);
	mutex_init(&mcooldev->lock);

	policy = cpufreq_cpu_get(0);
	if (!policy || !policy->freq_table) {
		dev_info(&pdev->dev,
			"Frequency policy not init. Deferring probe...\n");
		return -EPROBE_DEFER;
	}

	for_each_possible_cpu(cpu) {
		if (mc_capable())
			c_id = topology_physical_package_id(cpu);
		else
			c_id = CLUSTER_BIG;	/* Always cluster 0 if no mc */
		if (c_id > NUM_CLUSTERS) {
			pr_err("Cluster id: %d > %d\n", c_id, NUM_CLUSTERS);
			return -EINVAL;
		}
		cpumask_set_cpu(cpu, &mcooldev->mask[c_id]);
	}

	if (parse_cool_device(pdev))
		pr_info("meson_cdev one or more cooldev register fail\n");

	/* update min state for each device */
	for (i = 0; i < mcooldev->cool_dev_num; i++) {
		cool = &mcooldev->cool_devs[i];
		if (cool->cooling_dev)
			meson_cooldev_min_update(pdev, i);
	}
	/*save pdev for mali ko api*/
	meson_gcooldev = platform_get_drvdata(pdev);

	pr_info("meson_cdev probe done\n");
	return 0;
}

static int meson_cooldev_remove(struct platform_device *pdev)
{
	struct meson_cooldev *mcooldev = platform_get_drvdata(pdev);
	devm_kfree(&pdev->dev, mcooldev);
	return 0;
}

static const struct of_device_id meson_cooldev_of_match[] = {
	{ .compatible = "amlogic, meson-cooldev" },
	{},
};

static struct platform_driver meson_cooldev_platdrv = {
	.driver = {
		.name		= "meson-cooldev",
		.owner		= THIS_MODULE,
		.of_match_table = meson_cooldev_of_match,
	},
	.probe	= meson_cooldev_probe,
	.remove	= meson_cooldev_remove,
};

static int __init meson_cooldev_platdrv_init(void)
{
	return platform_driver_register(&(meson_cooldev_platdrv));
}
late_initcall(meson_cooldev_platdrv_init);
