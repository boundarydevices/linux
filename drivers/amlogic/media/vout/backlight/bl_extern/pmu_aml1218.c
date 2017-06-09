/*
 * drivers/amlogic/media/vout/backlight/bl_extern/pmu_aml1218.c
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
#include <linux/i2c.h>
#include <linux/amlogic/i2c-amlogic.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/sysctl.h>
#include <linux/uaccess.h>
#include <linux/amlogic/media/vout/lcd/aml_bl_extern.h>
#ifdef CONFIG_AMLOGIC_BOARD_HAS_PMU
#include <linux/amlogic/aml_pmu_common.h>
#endif

static struct bl_extern_config_t *bl_ext_config;


#ifdef BL_EXT_DEBUG_INFO
#define DBG_PRINT(...)		pr_info(__VA_ARGS__)
#else
#define DBG_PRINT(...)
#endif

#define BL_EXTERN_NAME			"bl_pmu_aml1218"

static int bl_extern_set_level(unsigned int level)
{
#ifdef CONFIG_AMLOGIC_BOARD_HAS_PMU
	struct aml_pmu_driver *pmu_driver;
	unsigned char temp;
#endif
	int ret = 0;

	if (bl_ext_config == NULL) {
		pr_err("no %s driver\n", BL_EXTERN_NAME);
		return -1;
	}
	get_bl_ext_level(bl_ext_config);
	level = bl_ext_config->dim_min -
		((level - bl_ext_config->level_min) *
			(bl_ext_config->dim_min - bl_ext_config->dim_max)) /
			(bl_ext_config->level_max - bl_ext_config->level_min);
	level &= 0x1f;
#ifdef CONFIG_AMLOGIC_BOARD_HAS_PMU
	pmu_driver = aml_pmu_get_driver();
	if (pmu_driver == NULL) {
		pr_err("no pmu driver\n");
	} else {
		if ((pmu_driver->pmu_reg_write) && (pmu_driver->pmu_reg_read)) {
			ret = pmu_driver->pmu_reg_read(0x005f, &temp);
			temp &= ~(0x3f << 2);
			temp |= (level << 2);
			ret = pmu_driver->pmu_reg_write(0x005f, temp);
		} else {
			pr_err("no pmu_reg_read/write\n");
			return -1;
		}
	}
#endif
	return ret;
}

static int bl_extern_power_on(void)
{
#ifdef CONFIG_AMLOGIC_BOARD_HAS_PMU
	struct aml_pmu_driver *pmu_driver;
	unsigned char temp;
#endif
	int ret = 0;

#ifdef CONFIG_AMLOGIC_BOARD_HAS_PMU
	pmu_driver = aml_pmu_get_driver();
	if (pmu_driver == NULL) {
		pr_err("no pmu driver\n");
	} else {
		if ((pmu_driver->pmu_reg_write) && (pmu_driver->pmu_reg_read)) {
			ret = pmu_driver->pmu_reg_read(0x005e, &temp);
			temp |= (1 << 7);
			ret = pmu_driver->pmu_reg_write(0x005e, temp);
		} else {
			pr_err("no pmu_reg_read/write\n");
			return -1;
		}
	}
#endif
	if (bl_ext_config->gpio_used > 0) {
		if (bl_ext_config->gpio_on == 2)
			bl_extern_gpio_input(bl_ext_config->gpio);
		else
			bl_extern_gpio_output(bl_ext_config->gpio,
							bl_ext_config->gpio_on);
	}
	pr_info("%s\n", __func__);
	return ret;
}

static int bl_extern_power_off(void)
{
#ifdef CONFIG_AMLOGIC_BOARD_HAS_PMU
	struct aml_pmu_driver *pmu_driver;
	unsigned char temp;
#endif
	int ret = 0;

	if (bl_ext_config->gpio_used > 0) {
		if (bl_ext_config->gpio_off == 2)
			bl_extern_gpio_input(bl_ext_config->gpio);
		else
			bl_extern_gpio_output(bl_ext_config->gpio,
						bl_ext_config->gpio_off);
	}
#ifdef CONFIG_AMLOGIC_BOARD_HAS_PMU
	pmu_driver = aml_pmu_get_driver();
	if (pmu_driver == NULL) {
		pr_err("no pmu driver\n");
	}  else {
		if ((pmu_driver->pmu_reg_write) && (pmu_driver->pmu_reg_read)) {
			ret = pmu_driver->pmu_reg_read(0x005e, &temp);
			temp &= ~(1 << 7);
			ret = pmu_driver->pmu_reg_write(0x005e, temp);
		}  else {
			pr_err("no pmu_reg_read/write\n");
			return -1;
		}
	}
#endif
	pr_info("%s\n", __func__);
	return ret;
}

static ssize_t bl_extern_debug(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
#ifdef CONFIG_AMLOGIC_BOARD_HAS_PMU
	struct aml_pmu_driver *pmu_driver;
	unsigned char temp;
	unsigned int t[2];
#endif
	int ret = 0;

#ifdef CONFIG_AMLOGIC_BOARD_HAS_PMU
	pmu_driver = aml_pmu_get_driver();
	if (pmu_driver == NULL) {
		pr_err("no pmu driver\n");
		return -EINVAL;
	}

	switch (buf[0]) {
	case 'r':
		ret = sscanf(buf, "r %x", &t[0]);
		if ((pmu_driver->pmu_reg_write) && (pmu_driver->pmu_reg_read)) {
			ret = pmu_driver->pmu_reg_read(t[0], &temp);
			pr_info("read pmu reg: 0x%x=0x%x\n", t[0], temp);
		}
		break;
	case 'w':
		ret = sscanf(buf, "w %x %x", &t[0], &t[1]);
		if ((pmu_driver->pmu_reg_write) && (pmu_driver->pmu_reg_read)) {
			ret = pmu_driver->pmu_reg_write(t[0], t[1]);
			ret = pmu_driver->pmu_reg_read(t[0], &temp);
			pr_info("write pmu reg 0x%x: 0x%x, readback: 0x%x\n",
							t[0], t[1], temp);
		}
		break;
	default:
		pr_err("wrong format of command.\n");
		break;
	}
#endif
	if (ret != 1 || ret != 2)
		return -EINVAL;
	return count;
}

static struct class_attribute bl_extern_debug_class_attrs[] = {
	__ATTR(debug, 0644, NULL, bl_extern_debug),
	__ATTR_NULL
};

static struct class bl_extern_debug_class = {
	.name = "bl_ext",
	.class_attrs = bl_extern_debug_class_attrs,
};

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

	if (bl_ext_cfg->dim_min > 0x1f)
		bl_ext_cfg->dim_min = 0x1f;
	if (bl_ext_cfg->dim_max > 0x1f)
		bl_ext_cfg->dim_max = 0x1f;

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

static int aml_aml1218_probe(struct platform_device *pdev)
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

	ret = class_register(&bl_extern_debug_class);
	if (ret)
		pr_err("class register bl_extern_debug_class fail!\n");

	pr_err("%s ok\n", __func__);
	return ret;

bl_extern_probe_failed:
	kfree(bl_ext_config);
	bl_ext_config = NULL;
	return -1;
}

static int aml_aml1218_remove(struct platform_device *pdev)
{
	kfree(pdev->dev.platform_data);
	return 0;
}

#ifdef CONFIG_USE_OF
static const struct of_device_id aml_aml1218_dt_match[] = {
	{
		.compatible = "amlogic, bl_pmu_aml1218",
	},
	{},
};
#else
#define aml_aml1218_dt_match NULL
#endif

static struct platform_driver aml_aml1218_driver = {
	.probe  = aml_aml1218_probe,
	.remove = aml_aml1218_remove,
	.driver = {
		.name  = BL_EXTERN_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_USE_OF
		.of_match_table = aml_aml1218_dt_match,
#endif
	},
};

static int __init aml_aml1218_init(void)
{
	int ret;

	DBG_PRINT("%s\n", __func__);

	ret = platform_driver_register(&aml_aml1218_driver);
	if (ret) {
		pr_err("[error] %s failed to register bl extern driver\n",
								__func__);
		return -ENODEV;
	}
	return ret;
}

static void __exit aml_aml1218_exit(void)
{
	platform_driver_unregister(&aml_aml1218_driver);
}

rootfs_initcall(aml_aml1218_init);
module_exit(aml_aml1218_exit);

MODULE_AUTHOR("AMLOGIC");
MODULE_DESCRIPTION("BL Extern driver for aml1218");
MODULE_LICENSE("GPL");
