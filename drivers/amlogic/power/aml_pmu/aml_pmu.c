/*
 * drivers/amlogic/power/aml_pmu/aml_pmu.c
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/amlogic/aml_pmu.h>

#ifdef CONFIG_AMLOGIC_M8B_DVFS
#include <linux/amlogic/aml_dvfs.h>
#endif

#ifdef CONFIG_AMLOGIC_1218
struct i2c_client *g_aml1218_client;
#endif
static const struct i2c_device_id aml_pmu_id_table[] = {
#ifdef CONFIG_AMLOGIC_1218
	{ AML1218_DRIVER_NAME, 2},
#endif
	{},
};
MODULE_DEVICE_TABLE(i2c, aml_pmu_id_table);

#ifdef CONFIG_OF
#define DEBUG_TREE	0
#define DEBUG_PARSE	0
#define DBG(format, args...) pr_info("[AML_PMU]%s, "format, __func__, ##args)

static struct i2c_device_id *find_id_by_name(const struct i2c_device_id *l,
					     const char *name)
{
	while (l->name && l->name[0]) {
		pr_info("table name:%s, name:%s\n", l->name, name);
		if (!strcmp(l->name, name))
			return (struct i2c_device_id *)l;
		l++;
	}
	return NULL;
}
#endif /* CONFIG_OF */

#if defined(CONFIG_AMLOGIC_M8B_DVFS) && defined(CONFIG_AMLOGIC_1218)
static int aml_1218_convert_id_to_dcdc(uint32_t id)
{
	int dcdc = 0;

	switch (id) {
	case AML_DVFS_ID_VCCK:
		dcdc = 4;
		break;

	case AML_DVFS_ID_VDDEE:
		dcdc = 2;
		break;

	case AML_DVFS_ID_DDR:
		dcdc = 3;
		break;

	default:
		break;
	}
	return dcdc;
}

static int aml1218_set_voltage(uint32_t id, uint32_t min_uV, uint32_t max_uV)
{
	int dcdc = aml_1218_convert_id_to_dcdc(id);
	uint32_t vol = 0;

	if (min_uV > max_uV)
		return -1;
	vol = (min_uV + max_uV) / 2;
	if (dcdc >= 1 && dcdc <= 4)
		return aml1218_set_dcdc_voltage(dcdc, vol);
	return -EINVAL;
}

static int aml1218_get_voltage(uint32_t id, uint32_t *uV)
{
	int dcdc = aml_1218_convert_id_to_dcdc(id);

	if (dcdc >= 1 && dcdc <= 4)
		return aml1218_get_dcdc_voltage(dcdc, uV);
	return -EINVAL;
}

struct aml_dvfs_driver aml1218_dvfs_driver = {
	.name	     = "aml1218-dvfs",
	.id_mask     = (AML_DVFS_ID_VCCK | AML_DVFS_ID_VDDEE | AML_DVFS_ID_DDR),
	.set_voltage = aml1218_set_voltage,
	.get_voltage = aml1218_get_voltage,
};
#endif

static int aml_pmu_check_device(struct i2c_client *client)
{
	int ret;
	uint8_t buf[2] = {};
	struct i2c_msg msg[] = {
		{
			.addr  = client->addr & 0xff,
			.flags = 0,
			.len   = sizeof(buf),
			.buf   = buf,
		},
		{
			.addr  = client->addr & 0xff,
			.flags = I2C_M_RD,
			.len   = 1,
			.buf   = &buf[1],
		}
	};

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret < 0) {
		DBG("%s: i2c transfer for %x failed, ret:%d\n",
		    __func__, client->addr, ret);
		return ret;
	}
	return 0;
}

static int aml_pmu_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
#ifdef CONFIG_OF
	const char *subtype;
	struct device_node *node;
	struct  i2c_device_id *type = NULL;
	int     ret;

	if (aml_pmu_check_device(client))
		return -ENODEV;
	node = client->dev.of_node;
	ret = of_property_read_string(node, "sub_type", &subtype);
	if (ret) {
		pr_err("can't find subtype\n");
		return -ENODEV;
	}

	type = find_id_by_name(aml_pmu_id_table, subtype);
	if (!type) { /* sub type is not supported */
		DBG("sub_type of '%s' is not match, abort\n", subtype);
		goto out_free_chip;
	}

#ifdef CONFIG_AMLOGIC_1218
	if (type->driver_data == 2) {
		g_aml1218_client = client;
#if defined(CONFIG_AMLOGIC_M8B_DVFS) && defined(CONFIG_AMLOGIC_1218)
		aml_dvfs_register_driver(&aml1218_dvfs_driver);
#endif
	}
#endif

out_free_chip:
#endif  /* CONFIG_OF */
	return 0;
}

static int aml_pmu_remove(struct i2c_client *client)
{
	struct platform_device *pdev = i2c_get_clientdata(client);

#ifdef CONFIG_AMLOGIC_1218
	g_aml1218_client = NULL;
#if defined(CONFIG_AMLOGIC_M8B_DVFS) && defined(CONFIG_AMLOGIC_1218)
	aml_dvfs_unregister_driver(&aml1218_dvfs_driver);
#endif
#endif
	platform_device_del(pdev);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id amlogic_pmu_match_id = {
	.compatible = "amlogic, amlogic_pmu",
};
#endif

static struct i2c_driver aml_pmu_driver = {
	.driver	= {
		.name	= "amlogic_pmu",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = &amlogic_pmu_match_id,
#endif
	},
	.probe		= aml_pmu_probe,
	.remove		= aml_pmu_remove,
	.id_table	= aml_pmu_id_table,
};

static int __init aml_pmu_init(void)
{
	pr_info("%s, %d\n", __func__, __LINE__);
	return i2c_add_driver(&aml_pmu_driver);
}
subsys_initcall(aml_pmu_init);

static void __exit aml_pmu_exit(void)
{
	i2c_del_driver(&aml_pmu_driver);
}
module_exit(aml_pmu_exit);

MODULE_DESCRIPTION("Amlogic PMU device driver");
MODULE_AUTHOR("tao.zeng@amlogic.com");
MODULE_LICENSE("GPL");
