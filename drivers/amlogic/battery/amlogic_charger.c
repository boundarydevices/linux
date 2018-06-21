/*
 * drivers/amlogic/battery/amlogic_charger.c
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

struct dummy_charger_info {
	struct device *dev;
	struct power_supply *usb;
	unsigned int online;
};

static int dummy_usb_get_prop(struct power_supply *psy,
				   enum power_supply_property psp,
				   union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = 1;
		break;
	default:
		return -ENODEV;
	}
	return 0;
}


static enum power_supply_property dummy_usb_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_ONLINE,
};

static const struct power_supply_desc dummy_charger_desc = {
	.name       = "usb",
	.type       = POWER_SUPPLY_TYPE_USB,
	.properties = dummy_usb_props,
	.num_properties = ARRAY_SIZE(dummy_usb_props),
	.get_property   = dummy_usb_get_prop,
};

static int dummy_charger_probe(struct platform_device *pdev)
{
	struct dummy_charger_info *info;
	struct device *dev = &pdev->dev;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->dev = dev;
	platform_set_drvdata(pdev, info);

	info->usb = power_supply_register(dev, &dummy_charger_desc,
					 NULL);

	if (IS_ERR(info->usb)) {
		pr_err("\n%ld\n", PTR_ERR(info->usb));
		return PTR_ERR(info->usb);
	}

	info->usb->dev.parent = dev;

	return 0;
}

static int dummy_charger_remove(struct platform_device *pdev)
{
	struct dummy_charger_info *info = platform_get_drvdata(pdev);

	power_supply_unregister(info->usb);
	return 0;
}


static const struct of_device_id meson_dummycharger_dt_match[] = {
	{
		.compatible = "amlogic, dummy-charger",
	},
	{},
};

static struct platform_driver dummy_charger_driver = {
	.driver = {
		   .name = "dummy-charger",
		   .owner = THIS_MODULE,
		   .of_match_table = meson_dummycharger_dt_match,
	},
	.probe = dummy_charger_probe,
	.remove = dummy_charger_remove,

};

module_platform_driver(dummy_charger_driver);

MODULE_DESCRIPTION("Amlogic Dummy Charger driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Shuide Chen<shuide.chen@amlogic.com>");
