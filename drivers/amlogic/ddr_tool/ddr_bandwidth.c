/*
 * drivers/amlogic/ddr_tool/ddr_bandwidth.c
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
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/aml_ddr_bandwidth.h>
#include <linux/io.h>
#include <linux/slab.h>

static struct ddr_bandwidth *aml_db;

static void cal_ddr_usage(struct ddr_bandwidth *db, struct ddr_grant *dg)
{
	u64 mul; /* avoid overflow */
	unsigned long i, cnt, freq = 0;

	if (db->ops && db->ops->get_freq)
		freq = db->ops->get_freq(db);
	mul  = (dg->all_grant * 10000ULL) / 16;	/* scale up to keep precision*/
	cnt  = db->clock_count;
	do_div(mul, cnt);
	db->total_usage = mul;
	if (freq) {
		/* calculate in KB */
		mul = (dg->all_grant * freq) / 1024;
		do_div(mul, cnt);
		db->total_bandwidth = mul;
		for (i = 0; i < db->channels; i++) {
			mul = (dg->channel_grant[i] * freq) / 1024;
			do_div(mul, cnt);
			db->bandwidth[i] = mul;
		}
	}
}

static irqreturn_t dmc_irq_handler(int irq, void *dev_instance)
{
	struct ddr_bandwidth *db;
	struct ddr_grant dg = {0};

	db = (struct ddr_bandwidth *)dev_instance;
	if (db->ops && db->ops->handle_irq) {
		if (!db->ops->handle_irq(db, &dg))
			cal_ddr_usage(db, &dg);
	}
	return IRQ_HANDLED;
}

unsigned int aml_get_ddr_usage(void)
{
	unsigned int ret = 0;

	if (aml_db)
		ret = aml_db->total_usage;

	return ret;
}
EXPORT_SYMBOL(aml_get_ddr_usage);

static char *find_port_name(int id)
{
	int i;

	if (!aml_db->real_ports || !aml_db->port_desc)
		return NULL;

	for (i = 0; i < aml_db->real_ports; i++) {
		if (aml_db->port_desc[i].port_id == id)
			return aml_db->port_desc[i].port_name;
	}
	return NULL;
}

static ssize_t ddr_channel_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	int size = 0, i;

	for (i = 0; i < aml_db->channels; i++)
		size += sprintf(buf + size, "ch %d:%3d, %s\n",
				i, aml_db->port[i],
				find_port_name(aml_db->port[i]));

	return size;
}

static ssize_t ddr_channel_store(struct class *cla,
	struct class_attribute *attr, const char *buf, size_t count)
{
	int ch = 0, port = 0;

	if (sscanf(buf, "%d:%d", &ch, &port) < 2) {
		pr_info("invalid input:%s\n", buf);
		return count;
	}

	if (ch >= MAX_CHANNEL ||
	    (ch && aml_db->cpu_type < MESON_CPU_MAJOR_ID_GXTVBB) ||
	    port > MAX_PORTS) {
		pr_info("invalid channel %d or port %d\n", ch, port);
		return count;
	}

	if (aml_db->ops && aml_db->ops->config_port) {
		aml_db->ops->config_port(aml_db, ch, port);
		aml_db->port[ch] = port;
	}

	return count;
}

static ssize_t clock_count_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", aml_db->clock_count);
}

static ssize_t clock_count_store(struct class *cla,
	struct class_attribute *attr, const char *buf, size_t count)
{
	long val = 0;

	if (kstrtoul(buf, 10, &val)) {
		pr_info("invalid input:%s\n", buf);
		return 0;
	}
	aml_db->clock_count = val;
	if (aml_db->ops && aml_db->ops->init)
		aml_db->ops->init(aml_db);

	return count;
}

static ssize_t bandwidth_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	size_t s = 0, i;
	int percent, rem;
#define BANDWIDTH_PREFIX "Total bandwidth:%8d KB/s, usage:%2d.%02d%%\n"

	percent = aml_db->total_usage / 100;
	rem     = aml_db->total_usage % 100;
	s      += sprintf(buf + s, BANDWIDTH_PREFIX,
			aml_db->total_bandwidth, percent, rem);

	for (i = 0; i < aml_db->channels; i++) {
		s += sprintf(buf + s, "Channel %ld, bandwidth:%8d KB/s\n",
				i, aml_db->bandwidth[i]);
	}
	return s;
}

static ssize_t freq_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	unsigned long clk = 0;

	if (aml_db->ops && aml_db->ops->get_freq)
		clk = aml_db->ops->get_freq(aml_db);
	return sprintf(buf, "%ld MHz\n", clk / 1000000);
}

#if DDR_BANDWIDTH_DEBUG
static ssize_t dump_reg_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	int s = 0;

	if (aml_db->ops && aml_db->ops->dump_reg)
		return aml_db->ops->dump_reg(aml_db, buf);

	return s;
}
#endif

static ssize_t cpu_type_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	int cpu_type;

	cpu_type = aml_db->cpu_type;
	return sprintf(buf, "%x\n", cpu_type);
}

static ssize_t name_of_ports_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	int i, s = 0;

	if (!aml_db->real_ports || !aml_db->port_desc)
		return -EINVAL;

	for (i = 0; i < aml_db->real_ports; i++) {
		s += sprintf(buf + s, "%2d, %s\n",
			     aml_db->port_desc[i].port_id,
			     aml_db->port_desc[i].port_name);
	}

	return s;
}

static struct class_attribute aml_ddr_tool_attr[] = {
	__ATTR(port, 0664, ddr_channel_show, ddr_channel_store),
	__ATTR(irq_clock, 0664, clock_count_show, clock_count_store),
	__ATTR_RO(bandwidth),
	__ATTR_RO(freq),
	__ATTR_RO(cpu_type),
	__ATTR_RO(name_of_ports),
#if DDR_BANDWIDTH_DEBUG
	__ATTR_RO(dump_reg),
#endif
	__ATTR_NULL
};

static struct class aml_ddr_class = {
	.name = "aml_ddr",
	.class_attrs = aml_ddr_tool_attr,
};

/*
 *    ddr_bandwidth_probe only executes before the init process starts
 * to run, so add __ref to indicate it is okay to call __init function
 * ddr_find_port_desc
 */
static int __ref ddr_bandwidth_probe(struct platform_device *pdev)
{
	int r = 0;
#ifdef CONFIG_OF
	struct device_node *node = pdev->dev.of_node;
	const char *irq_name = NULL;
	/*struct pinctrl *p;*/
	struct resource *res;
	resource_size_t *base;
#endif
	struct ddr_port_desc *desc = NULL;
	int pcnt;

	aml_db = kzalloc(sizeof(struct ddr_bandwidth), GFP_KERNEL);
	if (!aml_db)
		return -ENOMEM;

	aml_db->cpu_type = get_meson_cpu_version(0);
	pr_info("chip type:0x%x\n", aml_db->cpu_type);
	if (aml_db->cpu_type < MESON_CPU_MAJOR_ID_M8B) {
		pr_info("unsupport chip type:%d\n", aml_db->cpu_type);
		goto inval;
	}

	/* set channel */
	if (aml_db->cpu_type < MESON_CPU_MAJOR_ID_GXTVBB)
		aml_db->channels = 1;
	else
		aml_db->channels = 4;

	/* find and configure port description */
	pcnt = ddr_find_port_desc(aml_db->cpu_type, &desc);
	if (pcnt < 0)
		pr_err("can't find port descriptor,cpu:%d\n", aml_db->cpu_type);
	else {
		aml_db->port_desc = kcalloc(pcnt, sizeof(*desc), GFP_KERNEL);
		if (!aml_db->port_desc)
			goto inval;
		pr_info("port count:%d, desc:%p\n", pcnt, aml_db->port_desc);
		memcpy(aml_db->port_desc, desc, sizeof(*desc) * pcnt);
		aml_db->real_ports = pcnt;
	}

#ifdef CONFIG_OF
	/* resource 0 for ddr register base */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res) {
		base = ioremap(res->start, res->end - res->start);
		aml_db->ddr_reg = (void *)base;
	} else {
		pr_err("can't get ddr reg base\n");
		goto inval;
	}

	/* resource 1 for pll register base */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (res) {
		base = ioremap(res->start, res->end - res->start);
		aml_db->pll_reg = (void *)base;
	} else {
		pr_err("can't get ddr reg base\n");
		goto inval;
	}

	aml_db->irq_num = of_irq_get(node, 0);
	if (of_get_property(node, "interrupt-names", NULL)) {
		r = of_property_read_string(node, "interrupt-names", &irq_name);
		if (!r) {
			r = request_irq(aml_db->irq_num, dmc_irq_handler,
					IRQF_SHARED, irq_name, (void *)aml_db);
		}
	}
	if (r < 0) {
		pr_info("request irq failed:%d\n", aml_db->irq_num);
		goto inval;
	}
#endif
	aml_db->clock_count = DEFAULT_CLK_CNT;
	if (aml_db->cpu_type <= MESON_CPU_MAJOR_ID_GXTVBB)
		aml_db->ops = &gx_ddr_bw_ops;
	else if ((aml_db->cpu_type <= MESON_CPU_MAJOR_ID_TXHD) &&
		 (aml_db->cpu_type >= MESON_CPU_MAJOR_ID_GXL))
		aml_db->ops = &gxl_ddr_bw_ops;
	else if (aml_db->cpu_type >= MESON_CPU_MAJOR_ID_G12A)
		aml_db->ops = &g12_ddr_bw_ops;
	else {
		pr_err("%s, can't find ops for cpu type:%d\n",
			__func__, aml_db->cpu_type);
		goto inval;
	}

	if (aml_db->ops->init)
		aml_db->ops->init(aml_db);
	r = class_register(&aml_ddr_class);
	if (r)
		pr_info("%s, class regist failed\n", __func__);
	return 0;
inval:
	kfree(aml_db->port_desc);
	kfree(aml_db);
	aml_db = NULL;

	return -EINVAL;
}

static int ddr_bandwidth_remove(struct platform_device *pdev)
{
	if (aml_db) {
		class_destroy(&aml_ddr_class);
		free_irq(aml_db->irq_num, aml_db);
		kfree(aml_db->port_desc);
		kfree(aml_db);
		iounmap(aml_db->ddr_reg);
		iounmap(aml_db->pll_reg);
		aml_db = NULL;
	}

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id aml_ddr_bandwidth_dt_match[] = {
	{
		.compatible = "amlogic, ddr-bandwidth",
	}
};
#endif

static struct platform_driver ddr_bandwidth_driver = {
	.driver = {
		.name = "amlogic, ddr-bandwidth",
		.owner = THIS_MODULE,
	#ifdef CONFIG_OF
		.of_match_table = aml_ddr_bandwidth_dt_match,
	#endif
	},
	.probe  = ddr_bandwidth_probe,
	.remove = ddr_bandwidth_remove,
};

static int __init ddr_bandwidth_init(void)
{
	return platform_driver_register(&ddr_bandwidth_driver);
}

static void __exit ddr_bandwidth_exit(void)
{
	platform_driver_unregister(&ddr_bandwidth_driver);
}

subsys_initcall(ddr_bandwidth_init);
module_exit(ddr_bandwidth_exit);
MODULE_LICENSE("GPL v2");
