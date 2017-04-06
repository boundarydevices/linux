/*
 * drivers/amlogic/power/aml_pmu_of_common.c
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

#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/pinctrl/consumer.h>
#include <linux/delay.h>

#define AML_I2C_BUS_AO		0
#define AML_I2C_BUS_A		1
#define AML_I2C_BUS_B		2

#define DEBUG_TREE		0
#define DEBUG_PARSE		0
#define DBG(format, args...) pr_info("%s, "format, __func__, ##args)

static int aml_pmus_probe(struct platform_device *pdev)
{
	struct device_node	*pmu_node = pdev->dev.of_node;
	struct device_node	*child;
	struct i2c_board_info   board_info;
	struct i2c_adapter	*adapter;
	struct i2c_client	*client;
	int	err;
	int	addr;
	int	bus_type = -1;
	const  char *str;

	for_each_child_of_node(pmu_node, child) {
		/* register exist pmu */
		pr_info("%s, child name:%s\n", __func__, child->name);
		err = of_property_read_string(child, "i2c_bus", &str);
		if (err) {
			pr_info("get 'i2c_bus' failed, ret:%d\n", err);
			continue;
		}
		pr_info("%s, i2c_bus:%s\n", __func__, str);
		if (!strncmp(str, "i2c_bus_ao", 10))
			bus_type = AML_I2C_BUS_AO;
		else if (!strncmp(str, "i2c_bus_b", 9))
			bus_type = AML_I2C_BUS_B;
		else if (!strncmp(str, "i2c_bus_a", 9))
			bus_type = AML_I2C_BUS_A;
		else
			bus_type = AML_I2C_BUS_AO;
		err = of_property_read_string(child, "status", &str);
		if (err) {
			pr_info("get 'status' failed, ret:%d\n", err);
			continue;
		}
		if (strcmp(str, "okay") && strcmp(str, "ok")) {
			/* status is not OK, do not probe it */
			pr_info("device %s status is %s, stop probe it\n",
				 child->name, str);
			continue;
		}
		err = of_property_read_u32(child, "slave_address", &addr);
		if (err) {
			pr_info("get 'slave_address' failed, ret:%d\n", err);
			continue;
		}
		memset(&board_info, 0, sizeof(board_info));
		adapter = i2c_get_adapter(bus_type);
		if (!adapter) {
			pr_info("wrong i2c adapter:%d\n", bus_type);
			continue;
		}
		err = of_property_read_string(child, "compatible", &str);
		if (err) {
			pr_info("get 'compatible' failed, ret:%d\n", err);
			continue;
		}
		strncpy(board_info.type, str, I2C_NAME_SIZE);
		board_info.addr = addr;
		board_info.of_node = child;	 /* for device driver */
		board_info.irq = irq_of_parse_and_map(child, 0);
		client = i2c_new_device(adapter, &board_info);
		if (!client) {
			pr_info("%s, allocate i2c_client failed\n", __func__);
			continue;
		}
		pr_info("%s: adapter:%d, addr:0x%x, node name:%s, type:%s\n",
			 "Allocate new i2c device",
			 bus_type, addr, child->name, str);
	}
	return 0;
}

static int aml_pmus_remove(struct platform_device *pdev)
{
	/* nothing to do */
	return 0;
}

static const struct of_device_id aml_pmu_dt_match[] = {
	{
		.compatible = "amlogic, aml_pmu_prober",
	},
	{}
};

static  struct platform_driver aml_pmu_prober = {
	.probe	 = aml_pmus_probe,
	.remove	 = aml_pmus_remove,
	.driver	 = {
		.name   = "aml_pmu_prober",
		.owner  = THIS_MODULE,
		.of_match_table = aml_pmu_dt_match,
	},
};

static int __init aml_pmu_probe_init(void)
{
	int ret;

	pr_info("call %s in\n", __func__);
	ret = platform_driver_register(&aml_pmu_prober);
	return ret;
}

static void __exit aml_pmu_probe_exit(void)
{
	platform_driver_unregister(&aml_pmu_prober);
}

fs_initcall(aml_pmu_probe_init);
module_exit(aml_pmu_probe_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Amlogic pmu common driver");
