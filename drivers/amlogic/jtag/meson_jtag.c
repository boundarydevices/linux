/*
 * drivers/amlogic/jtag/meson_jtag.c
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

#define pr_fmt(fmt)	"jtag: " fmt

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/pinctrl/consumer.h>
#include <linux/arm-smccc.h>
#include <linux/psci.h>
#include <uapi/linux/psci.h>

#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/jtag.h>
#include <linux/amlogic/mmc_notify.h>

#include "meson_jtag.h"


#define AML_JTAG_NAME		"jtag"

static bool jtag_select_setup;

static int jtag_select = AMLOGIC_JTAG_DISABLE;
static int jtag_cluster = CLUSTER_DISABLE;

bool is_jtag_disable(void)
{
	if (jtag_select == AMLOGIC_JTAG_DISABLE)
		return true;
	else
		return false;
}
EXPORT_SYMBOL(is_jtag_disable);

bool is_jtag_apao(void)
{
	if (jtag_select == AMLOGIC_JTAG_AO)
		return true;
	else
		return false;
}
EXPORT_SYMBOL(is_jtag_apao);

bool is_jtag_apee(void)
{
	if (jtag_select == AMLOGIC_JTAG_EE)
		return true;
	else
		return false;
}
EXPORT_SYMBOL(is_jtag_apee);

static void aml_jtag_option_parse(struct aml_jtag_dev *jdev, const char *s)
{
	char *cluster;
	unsigned long value;
	int ret;

	if (!strncmp(s, "disable", 7))
		jdev->select = AMLOGIC_JTAG_DISABLE;
	else if (!strncmp(s, "apao", 4))
		jdev->select = AMLOGIC_JTAG_AO;
	else if (!strncmp(s, "apee", 4))
		jdev->select = AMLOGIC_JTAG_EE;
	else
		pr_info("unknown select: %s", s);

	jtag_select = jdev->select;
	pr_info("jtag select %d\n", jdev->select);

	cluster = strchr(s, ',');
	if (cluster != NULL) {
		cluster++;
		ret = kstrtoul(cluster, 10, &value);
		if (ret) {
			pr_err("invalid cluster index %d\n", jdev->cluster);
			jdev->cluster = CLUSTER_DISABLE;
		} else {
			jdev->cluster = value;
		}
	} else {
		jdev->cluster = CLUSTER_DISABLE;
	}

	jtag_cluster = jdev->cluster;
	pr_info("cluster index %d\n", jdev->cluster);
}

static int __init setup_jtag(char *p)
{
	char *cluster;
	unsigned long value;
	int ret;

	jtag_select_setup = true;

	if (!strncmp(p, "disable", 7))
		jtag_select = AMLOGIC_JTAG_DISABLE;
	else if (!strncmp(p, "apao", 4))
		jtag_select = AMLOGIC_JTAG_AO;
	else if (!strncmp(p, "apee", 4))
		jtag_select = AMLOGIC_JTAG_EE;
	else
		jtag_select = AMLOGIC_JTAG_DISABLE;

	pr_info("jtag select %d\n", jtag_select);

	cluster = strchr(p, ',');
	if (cluster != NULL) {
		cluster++;
		ret = kstrtoul(cluster, 10, &value);
		if (ret) {
			pr_err("invalid cluster index %d\n", jtag_cluster);
			jtag_cluster = CLUSTER_DISABLE;
		} else {
			jtag_cluster = value;
		}
		pr_info("cluster index %d\n", jtag_cluster);
	}

	return 0;
}

/*
 * jtag=[apao|apee]
 * jtag=[apao|apee]{,[0|1]}
 *
 * [apao|apee]: jtag domain
 * [0|1]: cluster index
 */
__setup("jtag=", setup_jtag);


/*
 * clean other pinmux except jtag register.
 * jtag register will be setup by aml_set_jtag_state().
 *
 *@return: 0 success, other failed
 *
 */
static int aml_jtag_pinmux_apao(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;

	int count, i, ret;
	int gpio;

	count = of_gpio_named_count(np, "jtagao-gpios");

	for (i = 0; i < count; i++) {
		gpio = of_get_named_gpio(dev->of_node, "jtagao-gpios", i);
		if (!gpio_is_valid(gpio)) {
			dev_err(dev, "gpio %d is not valid", gpio);
			return -ENOENT;
		}

		ret = devm_gpio_request(dev, gpio, "jtagao");
		if (ret) {
			dev_err(dev, "can't request gpio %d", gpio);
			return -ENOENT;
		}
	}

	return 0;
}

static int aml_jtag_pinmux_apee(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;

	int count, i, ret;
	int gpio;

	count = of_gpio_named_count(np, "jtagee-gpios");

	for (i = 0; i < count; i++) {
		gpio = of_get_named_gpio(dev->of_node, "jtagee-gpios", i);
		if (!gpio_is_valid(gpio)) {
			dev_err(dev, "gpio %d is not valid", gpio);
			return -ENOENT;
		}

		ret = devm_gpio_request(dev, gpio, "jtagee");
		if (ret) {
			dev_err(dev, "can't request gpio %d", gpio);
			return -ENOENT;
		}
	}

	return 0;
}


static void aml_jtag_pinctrl(struct aml_jtag_dev *jdev)
{
	switch (jdev->select) {
	case AMLOGIC_JTAG_AO:
		aml_jtag_pinmux_apao(jdev->pdev);
		break;
	case AMLOGIC_JTAG_EE:
		aml_jtag_pinmux_apee(jdev->pdev);
		break;
	default:
		break;
	}
}

#ifdef CONFIG_MACH_MESON8B

static int aml_jtag_select(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct aml_jtag_dev *jdev = platform_get_drvdata(pdev);
	unsigned int sel = jdev->select;
	u32 val;

	jdev->base = of_iomap(np, 0);
	if (!jdev->base) {
		dev_err(dev, "failed to iomap regs");
		return -ENODEV;
	}

	switch (sel) {
	case AMLOGIC_JTAG_DISABLE:
		writel_relaxed(0x0, jdev->base);
		break;
	case AMLOGIC_JTAG_AO:
		val = readl_relaxed(jdev->base);
		val &= ~0x3FF;
		val |= (2 << 0) | (1 << 8);
		writel_relaxed(val, jdev->base);
		break;
	case AMLOGIC_JTAG_EE:
		val = readl_relaxed(jdev->base);
		val &= ~0x3FF;
		val |= (2 << 4) | (2 << 8);
		writel_relaxed(val, jdev->base);
		break;
	default:
		writel_relaxed(0x0, jdev->base);
		break;
	}

	return 0;
}

#else

static unsigned long __invoke_psci_fn_smc(unsigned long function_id,
			unsigned long arg0, unsigned long arg1,
			unsigned long arg2)
{
	struct arm_smccc_res res;

	arm_smccc_smc(function_id, arg0, arg1, arg2, 0, 0, 0, 0, &res);
	return res.a0;
}


/*
 * setup jtag on/off, and setup ao/ee jtag
 *
 * @state: must be JTAG_STATE_ON/JTAG_STATE_OFF
 * @select: mest be JTAG_DISABLE/JTAG_A53_AO/JTAG_A53_EE
 */
void aml_set_jtag_state(unsigned int state, unsigned int select)
{
	uint64_t command;

	if (state == AMLOGIC_JTAG_STATE_ON)
		command = AMLOGIC_JTAG_ON;
	else
		command = AMLOGIC_JTAG_OFF;
	asm __volatile__("" : : : "memory");

	__invoke_psci_fn_smc(command, select, 0, 0);
}

static int aml_jtag_select(struct platform_device *pdev)
{
	struct aml_jtag_dev *jdev = platform_get_drvdata(pdev);
	unsigned int sel = jdev->select;

	if (jdev->cluster != CLUSTER_DISABLE)
		sel |= jdev->cluster << CLUSTER_BIT;

	pr_info("set state %u\n", sel);

	set_cpus_allowed_ptr(current, cpumask_of(0));
	aml_set_jtag_state(AMLOGIC_JTAG_STATE_ON, sel);
	set_cpus_allowed_ptr(current, cpu_all_mask);

	return 0;
}

#endif

static void aml_jtag_setup(struct aml_jtag_dev *jdev)
{
	aml_jtag_pinctrl(jdev);
	aml_jtag_select(jdev->pdev);
}

static ssize_t jtag_select_show(struct class *cls,
			struct class_attribute *attr, char *buf)
{
	unsigned int len = 0;

	len += sprintf(buf + len, "usage:\n");
	len += sprintf(buf + len, "    echo [apao|apee] > select\n");
	len += sprintf(buf + len, "    echo [apao|apee]{,[0|1]} > select\n");
	return len;
}


static ssize_t jtag_select_store(struct class *cls,
			 struct class_attribute *attr,
			 const char *buffer, size_t count)
{
	struct aml_jtag_dev *jdev;

	jdev = container_of(cls, struct aml_jtag_dev, cls);
	aml_jtag_option_parse(jdev, buffer);
	aml_jtag_setup(jdev);

	return count;
}

static struct class_attribute aml_jtag_attrs[] = {
	__ATTR(select,  0644, jtag_select_show, jtag_select_store),
	__ATTR_NULL,
};


static int jtag_notify_callback(struct notifier_block *block,
			unsigned long event, void *data)
{
	struct aml_jtag_dev *jdev;

	jdev = container_of(block, struct aml_jtag_dev, notifier);


	/* @todo need mmc driver to implement notify calling */
	pr_info("%s %lu\n", __func__, event);
	switch (event) {
	case MMC_EVENT_JTAG_IN:
		jdev->select = AMLOGIC_JTAG_EE;
		jtag_select = jdev->select;
		break;

	case MMC_EVENT_JTAG_OUT:
		jdev->select = AMLOGIC_JTAG_AO;
		jtag_select = jdev->select;
		break;

	default:
		return NOTIFY_DONE;
	}

	aml_jtag_setup(jdev);

	return NOTIFY_OK;
}


static int aml_jtag_dt_parse(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct aml_jtag_dev *jdev = platform_get_drvdata(pdev);
	const char *tmp;
	int ret;

	/* if jtag= param is setup, set select with jtag= param */
	if (jtag_select_setup) {
		jdev->select = jtag_select;
		jdev->cluster = jtag_cluster;
		return 0;
	}

	/* otherwise set select with dt */
	ret = of_property_read_string(np, "select", &tmp);
	if (ret < 0) {
		pr_err("select not configured\n");
		return -EINVAL;
	}
	pr_info("select is configured %s\n", tmp);

	if (!strcmp(tmp, "disable"))
		jdev->select = AMLOGIC_JTAG_DISABLE;
	else if (!strcmp(tmp, "apao"))
		jdev->select = AMLOGIC_JTAG_AO;
	else if (!strcmp(tmp, "apee"))
		jdev->select = AMLOGIC_JTAG_EE;
	else
		dev_warn(dev, "unknown select: %s", tmp);

	return 0;
}



static int aml_jtag_probe(struct platform_device *pdev)
{
	struct aml_jtag_dev *jdev;
	int ret;

	/* kzalloc device */
	jdev = kzalloc(sizeof(struct aml_jtag_dev), GFP_KERNEL);

	/* set driver data */
	jdev->pdev = pdev;
	platform_set_drvdata(pdev, jdev);

	aml_jtag_dt_parse(pdev);

	/* create class attributes */
	jdev->cls.name = AML_JTAG_NAME;
	jdev->cls.owner = THIS_MODULE;
	jdev->cls.class_attrs = aml_jtag_attrs;
	class_register(&jdev->cls);

	/* register mmc notify */
	jdev->notifier.notifier_call = jtag_notify_callback;
	ret = mmc_register_client(&jdev->notifier);

	/* setup jtag */
	aml_jtag_setup(jdev);

	pr_info("module probed ok\n");
	return 0;
}


static int __exit aml_jtag_remove(struct platform_device *pdev)
{
	struct aml_jtag_dev *jdev = platform_get_drvdata(pdev);

	class_unregister(&jdev->cls);
	platform_set_drvdata(pdev, NULL);
	kfree(jdev);

	pr_info("module removed ok\n");
	return 0;
}


static const struct of_device_id aml_jtag_dt_match[] = {
	{
		.compatible = "amlogic, jtag",
	},
	{},
};


static struct platform_driver aml_jtag_driver = {
	.driver = {
		.name = AML_JTAG_NAME,
		.owner = THIS_MODULE,
		.of_match_table = aml_jtag_dt_match,
	},
	.probe = aml_jtag_probe,
	.remove = __exit_p(aml_jtag_remove),
};


static int __init aml_jtag_init(void)
{
	pr_info("module init\n");
	if (platform_driver_register(&aml_jtag_driver)) {
		pr_err("failed to register driver\n");
		return -ENODEV;
	}

	return 0;
}

/* Jtag will be setuped before device_initcall that most driver used.
 * But jtag should be after pinmux.
 * That means we must use some initcall between arch_initcall
 * and device_initcall.
 */
fs_initcall(aml_jtag_init);

static void __exit aml_jtag_exit(void)
{
	pr_info("module exit\n");
	platform_driver_unregister(&aml_jtag_driver);
}

module_exit(aml_jtag_exit);

MODULE_DESCRIPTION("Meson JTAG Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Amlogic, Inc.");

