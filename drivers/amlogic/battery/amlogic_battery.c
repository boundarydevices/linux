/*
 * drivers/amlogic/battery/amlogic_battery.c
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
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/power_supply.h>

struct dummy_battery_info {
	struct device *dev;
	struct power_supply *battery;
	struct mutex lock;
	unsigned int present;
};

static int dummy_batt_get_prop(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	static int counter = 1;

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = 100;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = 4200;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		val->intval = 4100;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = 1100;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		val->intval = counter++;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		val->intval = 10000000;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = POWER_SUPPLY_STATUS_CHARGING;
		break;
	default:
		return -ENODEV;
	}
	return 0;
}

static int dummy_batt_set_prop(struct power_supply *psy,
			enum power_supply_property psp,
			const union power_supply_propval *val)
{
		return -EPERM;
}

static enum power_supply_property dummy_batt_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL,
};

static const struct power_supply_desc dummy_battery_desc = {
	.name           = "Battery",
	.type           = POWER_SUPPLY_TYPE_BATTERY,
	.properties     = dummy_batt_props,
	.num_properties     = ARRAY_SIZE(dummy_batt_props),
	.get_property       = dummy_batt_get_prop,
	.set_property       = dummy_batt_set_prop,
};

static int dummy_battery_probe(struct platform_device *pdev)
{
	struct dummy_battery_info *info;
	struct device *dev = &pdev->dev;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->dev = dev;

	mutex_init(&info->lock);
	platform_set_drvdata(pdev, info);

	info->battery = devm_power_supply_register(dev,
						   &dummy_battery_desc,
						   NULL);
	if (IS_ERR(info->battery)) {
		pr_err("\n%ld\n", PTR_ERR(info->battery));
		return PTR_ERR(info->battery);
	}

	info->battery->dev.parent = dev;

	return 0;
}

static const struct of_device_id meson_dummybattery_dt_match[] = {
	{
		.compatible = "amlogic, dummy-battery",
	},
	{},
};

static struct platform_driver dummy_battery_driver = {
	.driver = {
		   .name = "dummy-battery",
		   .owner = THIS_MODULE,
		   .of_match_table = meson_dummybattery_dt_match,
	},
	.probe = dummy_battery_probe,
};

module_platform_driver(dummy_battery_driver);

MODULE_DESCRIPTION("Amlogic Dummy Battery driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Shuide Chen<shuide.chen@amlogic.com>");
