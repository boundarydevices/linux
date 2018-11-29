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
#include <linux/extcon.h>
#include <linux/amlogic/aml_dmc.h>

static struct ddr_bandwidth *aml_db;
struct extcon_dev *ddr_extcon_bandwidth;
static const unsigned int bandwidth_cable[] = {
	EXTCON_TYPE_DISP,
	EXTCON_NONE,
};

static void cal_ddr_usage(struct ddr_bandwidth *db, struct ddr_grant *dg)
{
	u64 mul; /* avoid overflow */
	unsigned long i, cnt, freq = 0;

	if (db->mode == MODE_AUTODETECT) { /* ignore mali bandwidth */
		static int count;
		unsigned int grant = dg->all_grant;

		if (db->mali_port[0] >= 0)
			grant -= dg->channel_grant[0];
		if (db->mali_port[1] >= 0)
			grant -= dg->channel_grant[1];
		if (grant > db->threshold) {
			if (count >= 2) {
				if (db->busy == 0) {
					db->busy = 1;
					schedule_work(&db->work_bandwidth);
				}
			} else
				count++;
		} else if (count > 0) {
			if (count >= 2) {
				db->busy = 0;
				schedule_work(&db->work_bandwidth);
			}
			count = 0;
		}
		return;
	}
	if (db->ops && db->ops->get_freq)
		freq = db->ops->get_freq(db);
	mul  = dg->all_grant;
	mul *= 10000ULL;
	mul /= 16;
	cnt  = db->clock_count;
	do_div(mul, cnt);
	db->total_usage = mul;
	if (freq) {
		/* calculate in KB */
		mul  = dg->all_grant;
		mul *= freq;
		mul /= 1024;
		do_div(mul, cnt);
		db->total_bandwidth = mul;
		for (i = 0; i < db->channels; i++) {
			mul  = dg->channel_grant[i];
			mul *= freq;
			mul /= 1024;
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

static int format_port(char *buf, u64 port_mask)
{
	u64 t;
	int i, size = 0;
	char *name;

	for (i = 0; i < sizeof(u64) * 8; i++) {
		t = 1ULL << i;
		if (port_mask & t) {
			name = find_port_name(i);
			if (!name)
				continue;
			size += sprintf(buf + size, "      %s\n", name);
		}
	}
	return size;
}

static ssize_t ddr_channel_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	int size = 0, i;

	for (i = 0; i < aml_db->channels; i++) {
		size += sprintf(buf + size, "ch %d:%16llx: ports:\n",
				i, aml_db->port[i]);
		size += format_port(buf + size, aml_db->port[i]);
	}

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

	if (ch >= MAX_CHANNEL || ch < 0 ||
	    aml_db->cpu_type < MESON_CPU_MAJOR_ID_GXTVBB ||
	    port > MAX_PORTS) {
		pr_info("invalid channel %d or port %d\n", ch, port);
		return count;
	}

	if (aml_db->ops && aml_db->ops->config_port) {
		aml_db->ops->config_port(aml_db, ch, port);
		if (port < 0) /* clear port set */
			aml_db->port[ch] = 0;
		else
			aml_db->port[ch] |= 1ULL << port;
	}

	return count;
}

static ssize_t busy_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", aml_db->busy);
}

static ssize_t threshold_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n",
		aml_db->threshold / 16 / (aml_db->clock_count / 10000));
}

static ssize_t threshold_store(struct class *cla,
	struct class_attribute *attr, const char *buf, size_t count)
{
	long val = 0;

	if (kstrtoul(buf, 10, &val)) {
		pr_info("invalid input:%s\n", buf);
		return 0;
	}

	if (val > 10000)
		val = 10000;
	aml_db->threshold = val * 16 * (aml_db->clock_count / 10000);
	return count;
}

static ssize_t mode_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	if (aml_db->mode == MODE_DISABLE)
		return sprintf(buf, "0: disable\n");
	else if (aml_db->mode == MODE_ENABLE)
		return sprintf(buf, "1: enable\n");
	else if (aml_db->mode == MODE_AUTODETECT)
		return sprintf(buf, "2: auto detect\n");
	return sprintf(buf, "\n");
}

static ssize_t mode_store(struct class *cla,
	struct class_attribute *attr, const char *buf, size_t count)
{
	long val = 0;

	if (kstrtoul(buf, 10, &val)) {
		pr_info("invalid input:%s\n", buf);
		return 0;
	}
	if ((val > MODE_AUTODETECT) || (val < MODE_DISABLE))
		val = MODE_AUTODETECT;

	if (val == MODE_AUTODETECT && aml_db->ops && aml_db->ops->config_port) {
		if (aml_db->mali_port[0] >= 0) {
			aml_db->port[0] = (1ULL << aml_db->mali_port[0]);
			aml_db->ops->config_port(aml_db, 0, aml_db->port[0]);
		}
		if (aml_db->mali_port[1] >= 0) {
			aml_db->port[1] = (1ULL << aml_db->mali_port[1]);
			aml_db->ops->config_port(aml_db, 1, aml_db->port[1]);
		}
	}
	if ((aml_db->mode == MODE_DISABLE) && (val != MODE_DISABLE)) {
		int r = request_irq(aml_db->irq_num, dmc_irq_handler,
				IRQF_SHARED, "ddr_bandwidth", (void *)aml_db);
		if (r < 0) {
			pr_info("ddrbandwidth request irq failed\n");
			return count;
		}

		if (aml_db->ops->init)
			aml_db->ops->init(aml_db);
	} else if ((aml_db->mode != MODE_DISABLE) && (val == MODE_DISABLE)) {
		free_irq(aml_db->irq_num, (void *)aml_db);
		aml_db->total_usage = 0;
		aml_db->total_bandwidth = 0;
		aml_db->busy = 0;
	}
	aml_db->mode = val;

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
	aml_db->threshold /= (aml_db->clock_count / 10000);
	aml_db->threshold *= (val / 10000);
	aml_db->clock_count = val;
	if (aml_db->ops && aml_db->ops->init)
		aml_db->ops->init(aml_db);
	return count;
}

static ssize_t bandwidth_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	size_t s = 0;
	int percent, rem, i;
#define BANDWIDTH_PREFIX "Total bandwidth: %8d KB/s, usage: %2d.%02d%%\n"

	if (aml_db->mode != MODE_ENABLE)
		return sprintf(buf, "set mode to enable(1) first.\n");

	percent = aml_db->total_usage / 100;
	rem     = aml_db->total_usage % 100;
	s      += sprintf(buf + s, BANDWIDTH_PREFIX,
			aml_db->total_bandwidth, percent, rem);

	for (i = 0; i < aml_db->channels; i++) {
		s += sprintf(buf + s, "ch:%d port bit:%16llx: %8d KB/s\n",
			     i, aml_db->port[i], aml_db->bandwidth[i]);
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

void dmc_set_urgent(unsigned int port, unsigned int type)
{
	unsigned int val = 0, addr = 0;

	if (aml_db->cpu_type < MESON_CPU_MAJOR_ID_G12A) {
		unsigned int port_reg[16] = {
			DMC_AXI0_CHAN_CTRL, DMC_AXI1_CHAN_CTRL,
			DMC_AXI2_CHAN_CTRL, DMC_AXI3_CHAN_CTRL,
			DMC_AXI4_CHAN_CTRL, DMC_AXI5_CHAN_CTRL,
			DMC_AXI6_CHAN_CTRL, DMC_AXI7_CHAN_CTRL,
			DMC_AM0_CHAN_CTRL, DMC_AM1_CHAN_CTRL,
			DMC_AM2_CHAN_CTRL, DMC_AM3_CHAN_CTRL,
			DMC_AM4_CHAN_CTRL, DMC_AM5_CHAN_CTRL,
			DMC_AM6_CHAN_CTRL, DMC_AM7_CHAN_CTRL,};

		if (port >= 16)
			return;
		addr = port_reg[port];
	} else {
		unsigned int port_reg[24] = {
			DMC_AXI0_G12_CHAN_CTRL, DMC_AXI1_G12_CHAN_CTRL,
			DMC_AXI2_G12_CHAN_CTRL, DMC_AXI3_G12_CHAN_CTRL,
			DMC_AXI4_G12_CHAN_CTRL, DMC_AXI5_G12_CHAN_CTRL,
			DMC_AXI6_G12_CHAN_CTRL, DMC_AXI7_G12_CHAN_CTRL,
			DMC_AXI8_G12_CHAN_CTRL, DMC_AXI9_G12_CHAN_CTRL,
			DMC_AXI10_G12_CHAN_CTRL, DMC_AXI1_G12_CHAN_CTRL,
			DMC_AXI12_G12_CHAN_CTRL, 0, 0, 0,
			DMC_AM0_G12_CHAN_CTRL, DMC_AM1_G12_CHAN_CTRL,
			DMC_AM2_G12_CHAN_CTRL, DMC_AM3_G12_CHAN_CTRL,
			DMC_AM4_G12_CHAN_CTRL, DMC_AM5_G12_CHAN_CTRL,
			DMC_AM6_G12_CHAN_CTRL, DMC_AM7_G12_CHAN_CTRL,};

		if ((port >= 24) || (port_reg[port] == 0))
			return;
		addr = port_reg[port];
	}

	/**
	 *bit 18. force this channel all request to be super urgent request.
	 *bit 17. force this channel all request to be urgent request.
	 *bit 16. force this channel all request to be non urgent request.
	 */
	val = readl(aml_db->ddr_reg + addr);
	val &= (~(0x7 << 16));
	val |= ((type & 0x7) << 16);
	writel(val, aml_db->ddr_reg + addr);
}
EXPORT_SYMBOL(dmc_set_urgent);

static ssize_t urgent_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	int i, s = 0;

	if (!aml_db->real_ports || !aml_db->port_desc)
		return -EINVAL;

	s += sprintf(buf + s, "echo port val > /sys/class/aml_ddr/urgent\n"
			"val:\n\t1: non urgent;\n\t2: urgent;\n\t4: super urgent;\n"
			"port:   (hex integer)\n");
	for (i = 0; i < aml_db->real_ports; i++) {
		if (aml_db->port_desc[i].port_id >= 24)
			break;
		s += sprintf(buf + s, "\tbit%d: %s\n",
				aml_db->port_desc[i].port_id,
				aml_db->port_desc[i].port_name);
	}

	return s;
}

static ssize_t urgent_store(struct class *cla,
	struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned int port = 0, val, i;

	if (sscanf(buf, "%x %d", &port, &val) != 2) {
		pr_info("invalid input:%s\n", buf);
		return -EINVAL;
	}
	for (i = 0; i < 24; i++) {
		if (port & 1)
			dmc_set_urgent(i, val);
		port >>= 1;
	}
	return count;
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
	__ATTR(urgent, 0664, urgent_show, urgent_store),
	__ATTR(threshold, 0664, threshold_show, threshold_store),
	__ATTR(mode, 0664, mode_show, mode_store),
	__ATTR_RO(busy),
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

static void bandwidth_work_func(struct work_struct *work)
{
	extcon_set_state_sync(ddr_extcon_bandwidth, EXTCON_TYPE_DISP,
		(aml_db->busy == 1) ? true : false);
}

void ddr_extcon_register(struct platform_device *pdev)
{
	struct extcon_dev *edev;
	int ret;

	edev = extcon_dev_allocate(bandwidth_cable);
	if (IS_ERR(edev)) {
		pr_info("failed to allocate ddr extcon bandwidth\n");
		return;
	}
	edev->dev.parent = &pdev->dev;
	edev->name = "ddr_extcon_bandwidth";
	dev_set_name(&edev->dev, "bandwidth");
	ret = extcon_dev_register(edev);
	if (ret < 0) {
		pr_info("failed to register ddr extcon bandwidth\n");
		return;
	}
	ddr_extcon_bandwidth = edev;

	INIT_WORK(&aml_db->work_bandwidth, bandwidth_work_func);
}

static void ddr_extcon_free(void)
{
	extcon_dev_free(ddr_extcon_bandwidth);
	ddr_extcon_bandwidth = NULL;
}

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
	if (aml_db->cpu_type < MESON_CPU_MAJOR_ID_GXTVBB) {
		aml_db->channels = 1;
		aml_db->mali_port[0] = 2;
		aml_db->mali_port[1] = -1;
	} else {
		aml_db->channels = 4;
		if ((aml_db->cpu_type == MESON_CPU_MAJOR_ID_GXM)
			|| (aml_db->cpu_type >= MESON_CPU_MAJOR_ID_G12A)) {
			aml_db->mali_port[0] = 1; /* port1: mali */
			aml_db->mali_port[1] = -1;
		} else if (aml_db->cpu_type == MESON_CPU_MAJOR_ID_AXG) {
			aml_db->mali_port[0] = -1; /* no mali */
			aml_db->mali_port[1] = -1;
		} else {
			aml_db->mali_port[0] = 1; /* port1: mali0 */
			aml_db->mali_port[1] = 2; /* port2: mali1 */
		}
	}

	/* find and configure port description */
	pcnt = ddr_find_port_desc(aml_db->cpu_type, &desc);
	if (pcnt < 0)
		pr_err("can't find port descriptor,cpu:%d\n", aml_db->cpu_type);
	else {
		aml_db->port_desc = desc;
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
#endif
	aml_db->clock_count = DEFAULT_CLK_CNT;
	aml_db->mode = MODE_DISABLE;
	aml_db->threshold = DEFAULT_THRESHOLD * 16 *
			(aml_db->clock_count / 10000);
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

	if (!aml_db->ops->config_port)
		goto inval;

	r = class_register(&aml_ddr_class);
	if (r)
		pr_info("%s, class regist failed\n", __func__);

	ddr_extcon_register(pdev);
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
		iounmap(aml_db->ddr_reg);
		iounmap(aml_db->pll_reg);
		kfree(aml_db);
		aml_db = NULL;
	}
	ddr_extcon_free();

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id aml_ddr_bandwidth_dt_match[] = {
	{
		.compatible = "amlogic, ddr-bandwidth",
	},
	{}
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
