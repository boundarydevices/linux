/*
 * drivers/amlogic/ddr_tool/dmc_monitor.c
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

#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/irqreturn.h>
#include <linux/module.h>
#include <linux/mm.h>

#include <linux/cpu.h>
#include <linux/smp.h>
#include <linux/kallsyms.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/page_trace.h>
#include <linux/arm-smccc.h>
#include <linux/amlogic/dmc_monitor.h>
#include <linux/amlogic/ddr_port.h>

static struct dmc_monitor *dmc_mon;

unsigned long dmc_rw(unsigned long addr, unsigned long value, int rw)
{
	struct arm_smccc_res smccc;

	arm_smccc_smc(DMC_MON_RW, addr + dmc_mon->io_base,
		      value, rw, 0, 0, 0, 0, &smccc);

	return smccc.a0;
}

static int dev_name_to_id(const char *dev_name)
{
	int i, len;

	for (i = 0; i < dmc_mon->port_num; i++) {
		if (dmc_mon->port[i].port_id >= PORT_MAJOR)
			return -1;
		len = strlen(dmc_mon->port[i].port_name);
		if (!strncmp(dmc_mon->port[i].port_name, dev_name, len))
			break;
	}
	if (i >= dmc_mon->port_num)
		return -1;
	return dmc_mon->port[i].port_id;
}

char *to_ports(int id)
{
	int i;

	for (i = 0; i < dmc_mon->port_num; i++) {
		if (dmc_mon->port[i].port_id == id)
			return dmc_mon->port[i].port_name;
	}
	return NULL;
}

char *to_sub_ports(int mid, int sid, char *id_str)
{
	int i;

	/* 7 is device port id */
	if (mid == 7) {
		for (i = 0; i < dmc_mon->port_num; i++) {
			if (dmc_mon->port[i].port_id == sid + PORT_MAJOR)
				return dmc_mon->port[i].port_name;
		}
	}
	sprintf(id_str, "%2d", sid);

	return id_str;
}

unsigned int get_all_dev_mask(void)
{
	unsigned int ret = 0;
	int i;

	for (i = 0; i < PORT_MAJOR; i++) {
		if (dmc_mon->port[i].port_id >= PORT_MAJOR)
			break;
		ret |= (1 << dmc_mon->port[i].port_id);
	}
	return ret;
}

static unsigned int get_other_dev_mask(void)
{
	unsigned int ret = 0;
	int i;

	for (i = 0; i < PORT_MAJOR; i++) {
		if (dmc_mon->port[i].port_id >= PORT_MAJOR)
			break;

		/*
		 * we don't want id with arm mali and device
		 * because these devices can access all ddr range
		 * and generate value-less report
		 */
		if (strstr(dmc_mon->port[i].port_name, "ARM")  ||
		    strstr(dmc_mon->port[i].port_name, "MALI") ||
		    strstr(dmc_mon->port[i].port_name, "DEVICE"))
			continue;

		ret |= (1 << dmc_mon->port[i].port_id);
	}
	return ret;
}

static size_t dump_reg(char *buf)
{
	size_t sz = 0, i;

	if (dmc_mon->ops && dmc_mon->ops->dump_reg)
		sz += dmc_mon->ops->dump_reg(buf);
	sz += sprintf(buf + sz, "IO_BASE:%lx\n", dmc_mon->io_base);
	sz += sprintf(buf + sz, "RANGE:%lx - %lx\n",
		      dmc_mon->addr_start, dmc_mon->addr_end);
	sz += sprintf(buf + sz, "MONITOR DEVICE:\n");
	for (i = 0; i < sizeof(dmc_mon->device) * 8; i++) {
		if (dmc_mon->device & (1 << i))
			sz += sprintf(buf + sz, "    %s\n", to_ports(i));
	}

	return sz;
}

static irqreturn_t dmc_monitor_irq_handler(int irq, void *dev_instance)
{
	if (dmc_mon->ops && dmc_mon->ops->handle_irq)
		dmc_mon->ops->handle_irq(dmc_mon);

	return IRQ_HANDLED;
}

static void clear_irq_work(struct work_struct *work)
{
	/*
	 * DMC VIOLATION may happen very quickly and irq re-generated
	 * again before CPU leave IRQ mode, once this scenario happened,
	 * DMC protection would not generate IRQ again until we cleared
	 * it manually.
	 * Since no parameters used for irq handler, so we just call IRQ
	 * handler again to save code  size.
	 */
	dmc_monitor_irq_handler(0, NULL);
	schedule_delayed_work(&dmc_mon->work, HZ);
}

int dmc_set_monitor(unsigned long start, unsigned long end,
		    unsigned long dev_mask, int en)
{
	if (!dmc_mon)
		return -EINVAL;

	dmc_mon->addr_start = start;
	dmc_mon->addr_end   = end;
	if (en)
		dmc_mon->device |= dev_mask;
	else
		dmc_mon->device &= ~(dev_mask);
	if (start < end && dmc_mon->ops && dmc_mon->ops->set_montor)
		return dmc_mon->ops->set_montor(dmc_mon);
	return -EINVAL;
}
EXPORT_SYMBOL(dmc_set_monitor);

int dmc_set_monitor_by_name(unsigned long start, unsigned long end,
		    const char *port_name, int en)
{
	long id;

	id = dev_name_to_id(port_name);
	if (id < 0 || id >= BITS_PER_LONG)
		return -EINVAL;

	return dmc_set_monitor(start, end, 1UL << id, en);
}
EXPORT_SYMBOL(dmc_set_monitor_by_name);

void dmc_monitor_disable(void)
{
	if (dmc_mon->ops && dmc_mon->ops->disable)
		return dmc_mon->ops->disable(dmc_mon);
}
EXPORT_SYMBOL(dmc_monitor_disable);

static ssize_t protect_range_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%08lx - %08lx\n",
		       dmc_mon->addr_start, dmc_mon->addr_end);
}

static ssize_t protect_range_store(struct class *cla,
	struct class_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned long start, end;

	ret = sscanf(buf, "%lx %lx", &start, &end);
	if (ret != 2) {
		pr_info("%s, bad input:%s\n", __func__, buf);
		return count;
	}
	dmc_set_monitor(start, end, dmc_mon->device, 1);
	return count;
}

static ssize_t dev_store(struct class *cla,
	struct class_attribute *attr, const char *buf, size_t count)
{
	int i;

	if (!strncmp(buf, "none", 4)) {
		dmc_monitor_disable();
		return count;
	}
	if (!strncmp(buf, "all", 3))
		dmc_mon->device = get_all_dev_mask();
	else if (!strncmp(buf, "other", 5))
		dmc_mon->device = get_other_dev_mask();
	else {
		i = dev_name_to_id(buf);
		if (i < 0) {
			pr_info("bad device:%s\n", buf);
			return -EINVAL;
		}
		dmc_mon->device |= (1 << i);
	}
	dmc_set_monitor(dmc_mon->addr_start, dmc_mon->addr_end,
			dmc_mon->device, 1);

	return count;
}

static ssize_t dev_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	int i, s = 0;

	s += sprintf(buf + s, "supported device:\n");
	for (i = 0; i < dmc_mon->port_num; i++) {
		if (dmc_mon->port[i].port_id >= PORT_MAJOR)
			break;
		s += sprintf(buf + s, "%2d:%s\n",
			dmc_mon->port[i].port_id,
			dmc_mon->port[i].port_name);
	}
	return s;
}

static ssize_t dump_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	return dump_reg(buf);
}

static struct class_attribute dmc_monitor_attr[] = {
	__ATTR(range, 0664, protect_range_show, protect_range_store),
	__ATTR(device, 0664, dev_show, dev_store),
	__ATTR_RO(dump),
	__ATTR_NULL
};

static struct class dmc_monitor_class = {
	.name = "dmc_monitor",
	.class_attrs = dmc_monitor_attr,
};

static int dmc_monitor_probe(struct platform_device *pdev)
{
	int r = 0, irq, ports;
	unsigned int io;
	struct device_node *node = pdev->dev.of_node;
	struct ddr_port_desc *desc = NULL;

	pr_info("%s\n", __func__);
	r = get_cpu_type();
	dmc_mon = kzalloc(sizeof(struct dmc_monitor), GFP_KERNEL);
	if (!dmc_mon)
		return -ENOMEM;

	ports = ddr_find_port_desc(r, &desc);
	if (ports < 0) {
		pr_info("can't get port desc\n");
		goto inval;
	}
	dmc_mon->chip = r;
	dmc_mon->port_num = ports;
	dmc_mon->port = desc;
	if (dmc_mon->chip >= MESON_CPU_MAJOR_ID_G12A)
		dmc_mon->ops = &g12_dmc_mon_ops;
	else
		dmc_mon->ops = &gx_dmc_mon_ops;

	r = of_property_read_u32(node, "reg_base", &io);
	if (r < 0) {
		pr_info("can't find iobase\n");
		goto inval;
	}

	dmc_mon->io_base = io;

	irq = of_irq_get(node, 0);
	r = request_irq(irq, dmc_monitor_irq_handler,
			IRQF_SHARED, "dmc_monitor", pdev);
	if (r < 0) {
		pr_info("request irq failed:%d, r:%d\n", irq, r);
		goto inval;
	}
	r = class_register(&dmc_monitor_class);
	if (r) {
		pr_err("regist dmc_monitor_class failed\n");
		goto inval;
	}
	INIT_DELAYED_WORK(&dmc_mon->work, clear_irq_work);
	schedule_delayed_work(&dmc_mon->work, HZ);

	return 0;
inval:
	kfree(dmc_mon);
	dmc_mon = NULL;
	return -EINVAL;
}

static int dmc_monitor_remove(struct platform_device *pdev)
{
	cancel_delayed_work_sync(&dmc_mon->work);
	class_unregister(&dmc_monitor_class);
	kfree(dmc_mon);
	dmc_mon = NULL;
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id dmc_monitor_match[] = {
	{
		.compatible = "amlogic, dmc_monitor",
	},
	{}
};
#endif

static struct platform_driver dmc_monitor_driver = {
	.driver = {
		.name  = "dmc_monitor",
		.owner = THIS_MODULE,
	#ifdef CONFIG_OF
		.of_match_table = dmc_monitor_match,
	#endif
	},
	.probe = dmc_monitor_probe,
	.remove = dmc_monitor_remove,
};

static int __init dmc_monitor_init(void)
{
	int ret;

	ret = platform_driver_register(&dmc_monitor_driver);
	return ret;
}

static void __exit dmc_monitor_exit(void)
{
	platform_driver_unregister(&dmc_monitor_driver);
}

module_init(dmc_monitor_init);
module_exit(dmc_monitor_exit);
MODULE_DESCRIPTION("amlogic dmc monitor driver");
MODULE_LICENSE("GPL");
