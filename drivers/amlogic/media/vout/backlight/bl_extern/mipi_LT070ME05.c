/*
 * drivers/amlogic/media/vout/backlight/bl_extern/mipi_LT070ME05.c
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
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/sysctl.h>
#include <linux/uaccess.h>
#include <linux/amlogic/media/vout/lcd/aml_bl_extern.h>

#ifdef CONFIG_LCD_IF_MIPI_VALID
static struct bl_extern_config_t *bl_ext_config;

#ifdef BL_EXT_DEBUG_INFO
#define DBG_PRINT(...)		pr_info(__VA_ARGS__)
#else
#define DBG_PRINT(...)
#endif

#define BL_EXTERN_NAME			"bl_mipi_LT070ME05"
static unsigned int bl_status = 1;
static unsigned int bl_level;

/******************** mipi command ********************
 * format:  data_type, num, data....
 * special: data_type=0xff, num<0xff means delay ms, num=0xff means ending.
 */
static int bl_extern_set_level(unsigned int level)
{
	unsigned char payload[] = {0x15, 2, 0x51, 0xe6, 0xff, 0xff};

	bl_level = level;

	if (bl_ext_config == NULL) {
		pr_err("no %s driver\n", BL_EXTERN_NAME);
		return -1;
	}
	get_bl_ext_level(bl_ext_config);
	level = bl_ext_config->dim_min -
		((level - bl_ext_config->level_min) *
			(bl_ext_config->dim_min - bl_ext_config->dim_max)) /
			(bl_ext_config->level_max - bl_ext_config->level_min);
	level &= 0xff;

	if (bl_status) {
		payload[3] = level;
		dsi_write_cmd(payload);
	}
	return 0;
}

static int bl_extern_power_on(void)
{
	if (bl_ext_config->gpio_used > 0) {
		if (bl_ext_config->gpio_on == 2) {
			bl_extern_gpio_input(bl_ext_config->gpio);
		} else {
			bl_extern_gpio_output(bl_ext_config->gpio,
							bl_ext_config->gpio_on);
		}
	}
	bl_status = 1;
	bl_extern_set_level(bl_level);
	pr_info("%s\n", __func__);
	return 0;
}

static int bl_extern_power_off(void)
{
	if (bl_ext_config->gpio_used > 0) {
		if (bl_ext_config->gpio_off == 2) {
			bl_extern_gpio_input(bl_ext_config->gpio);
		} else {
			bl_extern_gpio_output(bl_ext_config->gpio,
						bl_ext_config->gpio_off);
		}
	}
	pr_info("%s\n", __func__);
	return 0;
}

static int get_bl_extern_config(struct device dev,
				struct bl_extern_config_t *bl_ext_cfg)
{
	int ret = 0;
	struct aml_bl_extern_driver_t *bl_ext;

	ret = get_bl_extern_dt_data(dev, bl_ext_cfg);
	if (ret) {
		pr_err("[error] %s: failed to get dt data\n", BL_EXTERN_NAME);
		return ret;
	}

	if (bl_ext_cfg->dim_min > 0xff)
		bl_ext_cfg->dim_min = 0xff;
	if (bl_ext_cfg->dim_max > 0xff)
		bl_ext_cfg->dim_max = 0xff;

	bl_ext = aml_bl_extern_get_driver();
	if (bl_ext) {
		bl_ext->type      = bl_ext_cfg->type;
		bl_ext->name      = bl_ext_cfg->name;
		bl_ext->power_on  = bl_extern_power_on;
		bl_ext->power_off = bl_extern_power_off;
		bl_ext->set_level = bl_extern_set_level;
	} else {
		pr_err("[error] %s get bl_extern_driver failed\n",
							bl_ext_cfg->name);
		ret = -1;
	}
	return ret;
}

static int aml_LT070ME05_probe(struct platform_device *pdev)
{
	int ret = 0;

	if (bl_extern_driver_check())
		return -1;
	if (bl_ext_config == NULL)
		bl_ext_config = kzalloc(sizeof(*bl_ext_config), GFP_KERNEL);
	if (bl_ext_config == NULL) {
		pr_err("[error] %s probe: failed to alloc data\n",
								BL_EXTERN_NAME);
		return -1;
	}

	pdev->dev.platform_data = bl_ext_config;
	ret = get_bl_extern_config(pdev->dev, bl_ext_config);
	if (ret)
		goto bl_extern_probe_failed;

	pr_info("%s ok\n", __func__);
	return ret;

bl_extern_probe_failed:
	kfree(bl_ext_config);
	bl_ext_config = NULL;
	return -1;
}

static int aml_LT070ME05_remove(struct platform_device *pdev)
{
	kfree(pdev->dev.platform_data);
	return 0;
}

#ifdef CONFIG_USE_OF
static const struct of_device_id aml_LT070ME05_dt_match[] = {
	{
		.compatible = "amlogic, bl_mipi_LT070ME05",
	},
	{},
};
#else
#define aml_LT070ME05_dt_match NULL
#endif

static struct platform_driver aml_LT070ME05_driver = {
	.probe  = aml_LT070ME05_probe,
	.remove = aml_LT070ME05_remove,
	.driver = {
		.name  = BL_EXTERN_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_USE_OF
		.of_match_table = aml_LT070ME05_dt_match,
#endif
	},
};

static int __init aml_LT070ME05_init(void)
{
	int ret;

	DBG_PRINT("%s\n", __func__);

	ret = platform_driver_register(&aml_LT070ME05_driver);
	if (ret) {
		pr_err("[error] %s failed to register bl extern driver\n",
								__func__);
		return -ENODEV;
	}
	return ret;
}

static void __exit aml_LT070ME05_exit(void)
{
	platform_driver_unregister(&aml_LT070ME05_driver);
}

rootfs_initcall(aml_LT070ME05_init);
module_exit(aml_LT070ME05_exit);

MODULE_AUTHOR("AMLOGIC");
MODULE_DESCRIPTION("BL Extern driver for LT070ME05");
MODULE_LICENSE("GPL");

#endif
