/*
 * drivers/amlogic/media/vout/backlight/bl_extern/i2c_lp8556.c
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

static struct bl_extern_config_t *bl_ext_config;

static struct i2c_client *aml_lp8556_i2c_client;


#ifdef BL_EXT_DEBUG_INFO
#define DBG_PRINT(...)		pr_info(__VA_ARGS__)
#else
#define DBG_PRINT(...)
#endif

#define BL_EXTERN_NAME			"bl_i2c_lp8556"
static unsigned int bl_status = 1;
static unsigned int bl_level;

static unsigned char i2c_init_table[][2] = {
	{0xa1, 0x76}, /* hight bit(8~11)(0~0X66e set backlight) */
	{0xa0, 0x66},  /* low bit(0~7)  20mA */
	{0x16, 0x1F}, /* 5channel LED enable 0x1F */
	{0xa9, 0xA0}, /* VBOOST_MAX 25V */
	{0x9e, 0x12},
	{0xa2, 0x23},
	{0x01, 0x05}, /* 0x03 pwm+I2c set brightness,0x5 I2c set brightness */
	{0xff, 0xff}, /* ending flag */
};

static int aml_i2c_write(struct i2c_client *i2client,
				unsigned char *buff, unsigned int len)
{
	int res = 0;
	struct i2c_msg msg[] = {
		{
			.addr = i2client->addr,
			.flags = 0,
			.len = len,
			.buf = buff,
		}
	};

	res = i2c_transfer(i2client->adapter, msg, 1);
	if (res < 0)
		pr_err("%s: i2c transfer failed [addr 0x%02x]\n",
						__func__, i2client->addr);

	return res;
}
#if 0
static int aml_i2c_read(struct i2c_client *i2client,
				unsigned char *buff, unsigned int len)
{
	int res = 0;
	struct i2c_msg msgs[] = {
		{
			.addr = i2client->addr,
			.flags = 0,
			.len = 1,
			.buf = buff,
		},
		{
			.addr = i2client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = buff,
		}
	};
	res = i2c_transfer(i2client->adapter, msgs, 2);
	if (res < 0)
		pr_err("%s: i2c transfer failed [addr 0x%02x]\n",
						__func__, i2client->addr);
	return res;
}
#endif

static int bl_extern_set_level(unsigned int level)
{
	unsigned char tData[3];
	int ret = 0;
	struct aml_bl_drv_s *bl_drv = aml_bl_get_driver();
	unsigned int level_max, level_min;
	unsigned int dim_max, dim_min;

	bl_level = level;

	if (bl_ext_config == NULL) {
		pr_err("no %s driver\n", BL_EXTERN_NAME);
		return -1;
	}

	if (bl_drv == NULL)
		return -1;
	level_max = bl_drv->bconf->level_max;
	level_min = bl_drv->bconf->level_min;
	dim_max = bl_ext_config->dim_max;
	dim_min = bl_ext_config->dim_min;
	level = dim_min - ((level - level_min) * (dim_min - dim_max)) /
			(level_max - level_min);
	level &= 0xff;

	if (bl_status) {
		tData[0] = 0x0;
		tData[1] = level;
		ret = aml_i2c_write(aml_lp8556_i2c_client, tData, 2);
	}
	return ret;
}

static int bl_extern_power_on(void)
{
	unsigned char tData[3];
	int i = 0, ending_flag = 0;
	int ret = 0;

	if (bl_ext_config->gpio) {
		if (bl_ext_config->gpio_on == 2) {
			bl_gpio_input(bl_ext_config->gpio);
		} else {
			bl_gpio_output(bl_ext_config->gpio,
					bl_ext_config->gpio_on);
		}
	}

	while (ending_flag == 0) {
		if (i2c_init_table[i][0] == 0xff) {
			if (i2c_init_table[i][1] == 0xff)
				ending_flag = 1;
			else
				mdelay(i2c_init_table[i][1]);
		} else {
			tData[0] = i2c_init_table[i][0];
			tData[1] = i2c_init_table[i][1];
			ret = aml_i2c_write(aml_lp8556_i2c_client, tData, 2);
		}
		i++;
	}
	bl_status = 1;
	bl_extern_set_level(bl_level);
	BLPR("%s\n", __func__);
	return ret;
}

static int bl_extern_power_off(void)
{
	bl_status = 0;
	if (bl_ext_config->gpio) {
		if (bl_ext_config->gpio_off == 2) {
			bl_gpio_input(bl_ext_config->gpio);
		} else {
			bl_gpio_output(bl_ext_config->gpio,
					bl_ext_config->gpio_off);
		}
	}
	BLPR("%s\n", __func__);
	return 0;
}

static int get_bl_extern_config(struct device dev,
		struct bl_extern_config_t *bl_ext_cfg)
{
	int ret = 0;
	struct aml_bl_extern_driver_t *bl_ext;

	ret = get_bl_extern_dt_data(dev, bl_ext_cfg);
	if (ret) {
		BLPR("error %s: failed to get dt data\n", BL_EXTERN_NAME);
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
		BLPR("error %s get bl_extern_driver failed\n",
			bl_ext_cfg->name);
		ret = -1;
	}
	return ret;
}

static int aml_lp8556_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		BLPR("error %s: functionality check failed\n",
								__func__);
	else
		aml_lp8556_i2c_client = client;
	BLPR("%s OK\n", __func__);

	return 0;
}

static int aml_lp8556_i2c_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id aml_lp8556_i2c_id[] = {
	{BL_EXTERN_NAME, 0},
	{ }
};

static struct i2c_driver aml_lp8556_i2c_driver = {
	.probe    = aml_lp8556_i2c_probe,
	.remove   = aml_lp8556_i2c_remove,
	.id_table = aml_lp8556_i2c_id,
	.driver = {
		.name = BL_EXTERN_NAME,
		.owner = THIS_MODULE,
	},
};

static int aml_lp8556_probe(struct platform_device *pdev)
{
	struct i2c_board_info i2c_info;
	struct i2c_adapter *adapter;
	struct i2c_client *i2c_client;
	int ret = 0;

	if (bl_extern_driver_check())
		return -1;
	if (bl_ext_config == NULL)
		bl_ext_config = kzalloc(sizeof(*bl_ext_config), GFP_KERNEL);
	if (bl_ext_config == NULL) {
		BLPR("error %s probe: failed to alloc data\n", BL_EXTERN_NAME);
		return -1;
	}

	pdev->dev.platform_data = bl_ext_config;
	ret = get_bl_extern_config(pdev->dev, bl_ext_config);
	if (ret)
		goto bl_extern_probe_failed;

	memset(&i2c_info, 0, sizeof(i2c_info));
	adapter = i2c_get_adapter(bl_ext_config->i2c_bus);
	if (!adapter) {
		BLPR("error %s£ºfailed get i2c adapter\n", BL_EXTERN_NAME);
		goto bl_extern_probe_failed;
	}

	strncpy(i2c_info.type, bl_ext_config->name, I2C_NAME_SIZE);
	i2c_info.addr = bl_ext_config->i2c_addr;
	i2c_info.platform_data = bl_ext_config;
	i2c_info.flags = 0;
	if (i2c_info.addr > 0x7f)
		i2c_info.flags = 0x10;
	i2c_client = i2c_new_device(adapter, &i2c_info);
	if (!i2c_client) {
		BLPR("error %s :failed new i2c device\n", BL_EXTERN_NAME);
		goto bl_extern_probe_failed;
	} else{
		DBG_PRINT("error %s: new i2c device succeed\n",
		((struct bl_extern_data_t *)(bl_ext_config))->name);
	}

	if (!aml_lp8556_i2c_client) {
		ret = i2c_add_driver(&aml_lp8556_i2c_driver);
		if (ret) {
			BLPR("error %s probe: add i2c_driver failed\n",
								BL_EXTERN_NAME);
			goto bl_extern_probe_failed;
		}
	}
	BLPR("%s ok\n", __func__);
	return ret;

bl_extern_probe_failed:
	kfree(bl_ext_config);
	bl_ext_config = NULL;

	return -1;
}

static int aml_lp8556_remove(struct platform_device *pdev)
{
	kfree(pdev->dev.platform_data);
	return 0;
}

#ifdef CONFIG_USE_OF
static const struct of_device_id aml_lp8556_dt_match[] = {
	{
		.compatible = "amlogic, bl_i2c_lp8556",
	},
	{},
};
#else
#define aml_lp8556_dt_match NULL
#endif

static struct platform_driver aml_lp8556_driver = {
	.probe  = aml_lp8556_probe,
	.remove = aml_lp8556_remove,
	.driver = {
		.name  = BL_EXTERN_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_USE_OF
		.of_match_table = aml_lp8556_dt_match,
#endif
	},
};

static int __init aml_lp8556_init(void)
{
	int ret;

	BLPR("%s\n", __func__);
	ret = platform_driver_register(&aml_lp8556_driver);
	if (ret) {
		BLPR("error %s failed to register bl extern driver\n",
			__func__);
		return -ENODEV;
	}
	return ret;
}

static void __exit aml_lp8556_exit(void)
{
	platform_driver_unregister(&aml_lp8556_driver);
}

module_init(aml_lp8556_init);
module_exit(aml_lp8556_exit);

MODULE_AUTHOR("AMLOGIC");
MODULE_DESCRIPTION("BL Extern driver for LP8556");
MODULE_LICENSE("GPL");
