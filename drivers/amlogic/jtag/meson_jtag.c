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
#ifdef CONFIG_MACH_MESON8B
#include <linux/amlogic/meson-secure.h>
#endif

#include "meson_jtag.h"


#define AML_JTAG_NAME		"jtag"

/* store the jtag select globaly */
static int global_select = AMLOGIC_JTAG_DISABLE;

/* whether the jtag select is setup by the boot param
 * jtag select is setup by the boot prior to device tree.
 */
static bool jtag_select_setup;

/* store the params that are setup by the boot param */
static int jtag_select = AMLOGIC_JTAG_DISABLE;
static int jtag_cluster = CLUSTER_DISABLE;

bool is_jtag_disable(void)
{
	if (global_select == AMLOGIC_JTAG_DISABLE)
		return true;
	else
		return false;
}
EXPORT_SYMBOL(is_jtag_disable);

bool is_jtag_apao(void)
{
	if (global_select == AMLOGIC_JTAG_APAO)
		return true;
	else
		return false;
}
EXPORT_SYMBOL(is_jtag_apao);

bool is_jtag_apee(void)
{
	if (global_select == AMLOGIC_JTAG_APEE)
		return true;
	else
		return false;
}
EXPORT_SYMBOL(is_jtag_apee);

static inline char *select_to_name(int select)
{
	switch (select) {
	case AMLOGIC_JTAG_APAO:
		return "apao";
	case AMLOGIC_JTAG_APEE:
		return "apee";
	default:
		return "disable";
	}

}

static void aml_jtag_option_parse(struct aml_jtag_dev *jdev, const char *s)
{
	char *cluster;
	unsigned long value;
	int ret;

	if (!strncmp(s, "disable", 7))
		jdev->select = AMLOGIC_JTAG_DISABLE;
	else if (!strncmp(s, "apao", 4))
		jdev->select = AMLOGIC_JTAG_APAO;
	else if (!strncmp(s, "apee", 4))
		jdev->select = AMLOGIC_JTAG_APEE;
	else
		pr_err("unknown select: %s", s);

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
		jtag_select = AMLOGIC_JTAG_APAO;
	else if (!strncmp(p, "apee", 4))
		jtag_select = AMLOGIC_JTAG_APEE;
	else
		jtag_select = AMLOGIC_JTAG_DISABLE;

	pr_info("jtag select %s\n", select_to_name(jtag_select));

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
 * request gpios for jtag apao.
 *
 * @return: 0 success, other failed
 *
 */
static int aml_jtag_apao_request_gpios(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct aml_jtag_dev *jdev = platform_get_drvdata(pdev);
	const unsigned int *gpios = jdev->ao_gpios;

	int ngpio, i, ret;

	ngpio = jdev->ao_ngpios;

	for (i = 0; i < ngpio; i++) {
		ret = devm_gpio_request(dev, gpios[i], "apao");
		if (ret) {
			pr_err("can't request gpio %d", gpios[i]);
			return -ENOENT;
		}
		pr_info("request gpio %d  for apao\n", gpios[i]);
	}

	return 0;
}

static int aml_jtag_apao_free_gpios(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct aml_jtag_dev *jdev = platform_get_drvdata(pdev);
	const unsigned int *gpios = jdev->ao_gpios;

	int ngpio, i;

	ngpio = jdev->ao_ngpios;

	for (i = 0; i < ngpio; i++)
		devm_gpio_free(dev, gpios[i]);

	return 0;
}


/*
 * request gpios for jtag apee.
 *
 * @return: 0 success, other failed
 *
 */
static int aml_jtag_apee_request_gpios(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct aml_jtag_dev *jdev = platform_get_drvdata(pdev);
	const unsigned int *gpios = jdev->ee_gpios;

	int ngpio, i, ret;

	ngpio = jdev->ee_ngpios;

	for (i = 0; i < ngpio; i++) {
		ret = devm_gpio_request(dev, gpios[i], "apee");
		if (ret) {
			pr_err("can't request gpio %d", gpios[i]);
			return -ENOENT;
		}
		pr_info("request gpio %d for apee\n", gpios[i]);
	}

	return 0;
}

static int aml_jtag_apee_free_gpios(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct aml_jtag_dev *jdev = platform_get_drvdata(pdev);
	const unsigned int *gpios = jdev->ee_gpios;

	int ngpio, i;

	ngpio = jdev->ee_ngpios;

	for (i = 0; i < ngpio; i++)
		devm_gpio_free(dev, gpios[i]);

	return 0;
}


#ifdef CONFIG_MACH_MESON8B

static int aml_jtag_select_tee(struct platform_device *pdev)
{
	struct aml_jtag_dev *jdev = platform_get_drvdata(pdev);
	uint32_t select = jdev->select;

	pr_info("set state %u\n", select);
	set_cpus_allowed_ptr(current, cpumask_of(0));
	switch (select) {
	case AMLOGIC_JTAG_DISABLE:
		meson_secure_jtag_disable();
		break;
	case AMLOGIC_JTAG_APAO:
		meson_secure_jtag_apao();
		break;
	case AMLOGIC_JTAG_APEE:
		meson_secure_jtag_apao();
		break;

	default:
		writel_relaxed(0x0, jdev->base);
		break;
	}
	set_cpus_allowed_ptr(current, cpu_all_mask);

	return 0;
}

static int aml_jtag_select_ree(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct aml_jtag_dev *jdev = platform_get_drvdata(pdev);
	unsigned int sel = jdev->select;
	u32 val;

	jdev->base = of_iomap(np, 0);
	if (!jdev->base) {
		pr_err("failed to iomap regs");
		return -ENODEV;
	}

	switch (sel) {
	case AMLOGIC_JTAG_DISABLE:
		writel_relaxed(0x0, jdev->base);
		break;
	case AMLOGIC_JTAG_APAO:
		val = readl_relaxed(jdev->base);
		val &= ~0x3FF;
		val |= (2 << 0) | (1 << 8);
		writel_relaxed(val, jdev->base);
		break;
	case AMLOGIC_JTAG_APEE:
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

static int aml_jtag_select(struct platform_device *pdev)
{
	if (meson_secure_enabled())
		aml_jtag_select_tee(pdev);
	else
		aml_jtag_select_ree(pdev);

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
	unsigned int select = jdev->select;
	unsigned int state = AMLOGIC_JTAG_STATE_OFF;

	if (select != AMLOGIC_JTAG_DISABLE)
		state = AMLOGIC_JTAG_STATE_ON;

	if (jdev->cluster != CLUSTER_DISABLE)
		select |= jdev->cluster << CLUSTER_BIT;

	pr_info("set state %u\n", select);

	set_cpus_allowed_ptr(current, cpumask_of(0));
	aml_set_jtag_state(state, select);
	set_cpus_allowed_ptr(current, cpu_all_mask);

	return 0;
}

#endif

static void aml_jtag_setup(struct aml_jtag_dev *jdev)
{
	struct platform_device *pdev = jdev->pdev;
	unsigned int old_select = jdev->old_select;
	unsigned int select = jdev->select;

	if (old_select == select)
		return;

	/* free gpios */
	switch (old_select) {
	case AMLOGIC_JTAG_APAO:
		aml_jtag_apao_free_gpios(pdev);
		break;
	case AMLOGIC_JTAG_APEE:
		aml_jtag_apee_free_gpios(pdev);
		break;
	default:
		break;
	}

	/* free gpios */
	switch (select) {
	case AMLOGIC_JTAG_APAO:
		aml_jtag_apao_request_gpios(pdev);
		break;
	case AMLOGIC_JTAG_APEE:
		aml_jtag_apee_request_gpios(pdev);
		break;
	default:
		break;
	}

	aml_jtag_select(jdev->pdev);

	jdev->old_select = select;

}

static ssize_t jtag_select_show(struct class *cls,
			struct class_attribute *attr, char *buf)
{
	unsigned int len = 0;

	len += sprintf(buf + len, "current select: %s\n\n",
			select_to_name(global_select));
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

	/* save to global */
	global_select = jdev->select;

	aml_jtag_setup(jdev);

	return count;
}

static struct class_attribute aml_jtag_attrs[] = {
	__ATTR(select,  0644, jtag_select_show, jtag_select_store),
	__ATTR_NULL,
};


static int aml_jtag_dt_parse(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct aml_jtag_dev *jdev = platform_get_drvdata(pdev);
	const char *tmp;

	int ao_ngpios, ee_ngpios;
	unsigned int *ao_gpios, *ee_gpios;
	int ret, i, gpio;


	ao_ngpios = of_gpio_named_count(np, "jtagao-gpios");
	if (ao_ngpios <= 0) {
		pr_err("ao gpios not specified\n");
		return -EINVAL;
	}

	ee_ngpios = of_gpio_named_count(np, "jtagee-gpios");
	if (ee_ngpios <= 0) {
		pr_err("ee gpios not specified\n");
		return -EINVAL;
	}

	jdev->ao_ngpios = ao_ngpios;
	jdev->ee_ngpios = ee_ngpios;

	ao_gpios = devm_kzalloc(dev, sizeof(unsigned int) * ao_ngpios,
				GFP_KERNEL);
	ee_gpios = devm_kzalloc(dev, sizeof(unsigned int) * ee_ngpios,
				GFP_KERNEL);
	if (!ao_gpios || !ee_gpios) {
		pr_err("failed to allocate memory for gpios\n");
		return -ENOMEM;
	}

	for (i = 0; i < ao_ngpios; i++) {
		gpio = of_get_named_gpio(dev->of_node, "jtagao-gpios", i);
		if (!gpio_is_valid(gpio)) {
			pr_err("gpio %d is not valid", gpio);
			return -EINVAL;
		}
		ao_gpios[i] = gpio;
	}

	for (i = 0; i < ee_ngpios; i++) {
		gpio = of_get_named_gpio(dev->of_node, "jtagee-gpios", i);
		if (!gpio_is_valid(gpio)) {
			pr_err("gpio %d is not valid", gpio);
			return -EINVAL;
		}
		ee_gpios[i] = gpio;
	}

	jdev->ao_gpios = ao_gpios;
	jdev->ee_gpios = ee_gpios;

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
		jdev->select = AMLOGIC_JTAG_APAO;
	else if (!strcmp(tmp, "apee"))
		jdev->select = AMLOGIC_JTAG_APEE;
	else
		pr_err("unknown select: %s", tmp);

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

	ret = aml_jtag_dt_parse(pdev);
	if (ret)
		return -EINVAL;

	jdev->old_select = AMLOGIC_JTAG_DISABLE;
	/* if jtag= param is setup, use select with jtag= param */
	if (jtag_select_setup) {
		jdev->select = jtag_select;
		jdev->cluster = jtag_cluster;
		pr_info("select is replaced by boot param\n");
	}

	/* save to global */
	global_select = jdev->select;

	/* create class attributes */
	jdev->cls.name = AML_JTAG_NAME;
	jdev->cls.owner = THIS_MODULE;
	jdev->cls.class_attrs = aml_jtag_attrs;
	ret = class_register(&jdev->cls);
	if (ret) {
		pr_err("couldn't register sysfs class\n");
		return ret;
	}

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
