/*
 * drivers/amlogic/ddr_tool/ddr_window.c
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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/amlogic/cpu_version.h>

static unsigned int reg_base;
static void __iomem *map;

static unsigned int reg_base_debug;
static void __iomem *map_debug;

static unsigned int reg_base_debug_a;
static void __iomem *map_debug_a;

static ssize_t window_reg_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	unsigned int val;

	if (reg_base == 0) {
		pr_err("please set register base first\n");
		return -EINVAL;
	}
	map = NULL;
	map = (void *)ioremap(reg_base, 4);
	pr_err("map:%p, base:%x\n", map, reg_base);
	if (map) {
		val = readl(map);
		iounmap(map);
		return sprintf(buf, "0x%08x\n", val);
	}

	return 0;
}

static ssize_t window_reg_store(struct class *cla,
	struct class_attribute *attr, const char *buf, size_t count)
{
	int ret = -1, i;
	unsigned int val;
	char *arg[2] = {}, *para, *buf_work, *p;

	map = NULL;
	buf_work = kstrdup(buf, GFP_KERNEL);
	p = buf_work;
	for (i = 0; i < 2; i++) {
		para = strsep(&p, " ");
		if (para == NULL)
			break;

		arg[i] = para;
	}
	if (i < 1 || i > 2) {
		ret = 1;
		goto error;
	}

	ret = kstrtouint((const char *)arg[0], 16, &reg_base);
	if (i == 2) {
		ret = kstrtouint((const char *)arg[1], 16, &val);
		map = (void *)ioremap(reg_base, 4);
		if (map) {
			writel(val, map);
			iounmap(map);
			pr_err("set 0x%08x to %08x\n", reg_base, val);
		}
	} else {
		pr_err("set reg base to 0x%08x\n", reg_base);
	}
error:
	kfree(buf_work);
	if (ret)
		pr_err("error\n");

	return count;
}

static ssize_t reg_debug_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{


	unsigned int val;

	if (reg_base_debug == 0) {
		pr_err("please set register base first\n");
		return -EINVAL;
	}
	map_debug = NULL;
	map_debug = (void *)ioremap(reg_base_debug, 4);
	pr_err("map:%p, base:%x\n", map_debug, reg_base_debug);
	if (map_debug) {
		val = readl(map_debug);
		iounmap(map_debug);
		return sprintf(buf, "0x%08x\n", val);
	}

	return 0;
}

static ssize_t reg_debug_store(struct class *cla,
	struct class_attribute *attr, const char *buf, size_t count)
{

	int ret = -1, i;
	unsigned int val;
	char *arg[2] = {}, *para, *buf_work, *p;

	map_debug = NULL;
	buf_work = kstrdup(buf, GFP_KERNEL);
	p = buf_work;
	for (i = 0; i < 2; i++) {
		para = strsep(&p, " ");
		if (para == NULL)
			break;

		arg[i] = para;
	}
	if (i < 1 || i > 2) {
		ret = 1;
		goto error;
	}

	ret = kstrtouint((const char *)arg[0], 16, &reg_base_debug);
	if (i == 2) {
		ret = kstrtouint((const char *)arg[1], 16, &val);
		map_debug = (void *)ioremap(reg_base_debug, 4);
		if (map_debug) {
			writel(val, map_debug);
			iounmap(map_debug);
			pr_err("set 0x%08x to %08x\n", reg_base_debug, val);
		}
	} else {
		pr_err("set reg base to 0x%08x\n", reg_base_debug);
	}
error:
	kfree(buf_work);
	if (ret)
		pr_err("error\n");
	return count;
}

static ssize_t reg_debug_a_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	unsigned int val;

	if (reg_base_debug_a == 0) {
		pr_err("please set register base first\n");
		return -EINVAL;
	}
	map_debug_a = NULL;
	map_debug_a = (void *)ioremap(reg_base_debug_a, 4);
	pr_err("map:%p, base:%x\n", map_debug_a, reg_base_debug_a);
	if (map_debug_a) {
		val = readl(map_debug_a);
		iounmap(map_debug_a);
		return sprintf(buf, "0x%08x\n", val);
	}

	return 0;
}




static ssize_t reg_debug_a_store(struct class *cla,
	struct class_attribute *attr, const char *buf, size_t count)
{

	int ret = -1, i;
	unsigned int val;
	char *arg[2] = {}, *para, *buf_work, *p;

	map_debug_a = NULL;
	buf_work = kstrdup(buf, GFP_KERNEL);
	p = buf_work;
	for (i = 0; i < 2; i++) {
		para = strsep(&p, " ");
		if (para == NULL)
			break;
		arg[i] = para;
	}

	if (i < 1 || i > 2) {
		ret = 1;
		goto error;
	}

	ret = kstrtouint((const char *)arg[0], 16, &reg_base_debug_a);
	if (i == 2) {
		ret = kstrtouint((const char *)arg[1], 16, &val);
		map_debug_a = (void *)ioremap(reg_base_debug_a, 4);
		if (map_debug_a) {
			writel(val, map_debug_a);
			iounmap(map_debug_a);
			pr_err("set 0x%08x to %08x\n", reg_base_debug_a, val);
		}
	} else {
		pr_err("set reg base to 0x%08x\n", reg_base_debug_a);
	}
error:
	kfree(buf_work);
	if (ret)
		pr_err("error\n");

	return count;
}

static ssize_t cpu_type_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	int cpu_type;

	cpu_type = get_meson_cpu_version(0);
	return sprintf(buf, "%x\n", cpu_type);
}

static struct class_attribute ddr_window_attr[] = {
	__ATTR(window_reg, 0664, window_reg_show, window_reg_store),
	__ATTR(reg_debug, 0664, reg_debug_show, reg_debug_store),
	__ATTR(reg_debug_a, 0664, reg_debug_a_show, reg_debug_a_store),
	__ATTR_RO(cpu_type),
	__ATTR_NULL
};

static struct class ddr_window_class = {
	.name = "ddr_window",
	.class_attrs = ddr_window_attr,
};

static int ddr_window_probe(struct platform_device *pdev)
{
	int r;

	r = class_register(&ddr_window_class);
	if (r) {
		pr_err("%s, class regist failed\n", __func__);
		return -EINVAL;
	}
	reg_base = 0;
	return 0;
}

static int ddr_window_remove(struct platform_device *pdev)
{
	class_unregister(&ddr_window_class);
	return 0;
}

static struct platform_driver ddr_window = {
	.driver = {
		.name = "ddr_window",
		.owner = THIS_MODULE,
	},
	.probe  = ddr_window_probe,
	.remove = ddr_window_remove,
};

static int __init ddr_window_init(void)
{
	struct platform_device *pdev;
	int ret;

	pdev = platform_device_alloc("ddr_window", 0);
	if (!pdev) {
		pr_err("%s, alloc pdev failed\n", __func__);
		return -EINVAL;
	}
	ret = platform_device_add(pdev);
	if (ret) {
		pr_err("%s, add pdev failed\n", __func__);
		platform_device_del(pdev);
		return -EINVAL;
	}
	return platform_driver_register(&ddr_window);
}

static void __exit ddr_window_exit(void)
{
	platform_driver_unregister(&ddr_window);
}

module_init(ddr_window_init);
module_exit(ddr_window_exit);
MODULE_LICENSE("GPL v2");
