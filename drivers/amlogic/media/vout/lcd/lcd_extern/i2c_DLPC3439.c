/*
 * drivers/amlogic/media/vout/lcd/lcd_extern/i2c_DLPC3439.c
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
#include <linux/amlogic/i2c-amlogic.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/amlogic/media/vout/lcd/lcd_extern.h>
#include "lcd_extern.h"

#define LCD_EXTERN_NAME			"i2c_DLPC3439"

static struct lcd_extern_config_s *ext_config;
static struct aml_lcd_extern_i2c_dev_s *i2c_device;


	/* Write: ImageCrop: 1920x1080
	 * W 36 10 00 00 00 00 80 07 38 04
	 */
static unsigned char data_1[] = {0x10, 0x00, 0x00, 0x00, 0x00,
		0x80, 0x07, 0x38, 0x04};
	/* Write: DisplaySize: 1920x1080
	 * W 36 12 80 07 38 04
	 */
static unsigned char data_2[] = {0x12, 0x80, 0x07, 0x38, 0x04};
	/* Write: InputImageSize: 1920x1080
	 * W 36 2e 80 07 38 04
	 */
static unsigned char data_3[] = {0x2e, 0x80, 0x07, 0x38, 0x04};
	/* Write: InputSourceSelect; 0 = External Video Port
	 * W 36 05 00
	 */
static unsigned char data_4[] = {0x05, 0x00};
	/* Write: VideoSourceFormatSelect: 0x43=RGB888
	 * W 36 07 43
	 */
static unsigned char data_5[] = {0x07, 0x43};

static int lcd_extern_i2c_write(struct i2c_client *i2client,
		unsigned char *buff, unsigned int len)
{
	int ret = 0;
	struct i2c_msg msg[] = {
		{
			.addr = i2client->addr,
			.flags = 0,
			.len = len,
			.buf = buff,
		}
	};

	ret = i2c_transfer(i2client->adapter, msg, 1);
	if (ret < 0)
		EXTERR("i2c write failed [addr 0x%02x]\n", i2client->addr);

	return ret;
}

static int lcd_extern_power_on(struct aml_lcd_extern_driver_s *ext_drv)
{
	int ret = 0;

	lcd_extern_pinmux_set(ext_drv, 1);

	if (i2c_device == NULL) {
		EXTERR("invalid i2c device\n");
		return -1;
	}
	lcd_extern_i2c_write(i2c_device->client, data_1, 9);
	lcd_extern_i2c_write(i2c_device->client, data_2, 5);
	lcd_extern_i2c_write(i2c_device->client, data_3, 5);
	lcd_extern_i2c_write(i2c_device->client, data_4, 2);
	lcd_extern_i2c_write(i2c_device->client, data_5, 2);

	EXTPR("%s\n", __func__);
	return ret;
}

static int lcd_extern_power_off(struct aml_lcd_extern_driver_s *ext_drv)
{
	int ret = 0;

	lcd_extern_pinmux_set(ext_drv, 0);

	return ret;
}

static int lcd_extern_driver_update(struct aml_lcd_extern_driver_s *ext_drv)
{
	int ret = 0;

	if (ext_drv) {
		ext_drv->power_on  = lcd_extern_power_on;
		ext_drv->power_off = lcd_extern_power_off;
	} else {
		EXTERR("%s driver is null\n", LCD_EXTERN_NAME);
		ret = -1;
	}

	return ret;
}

int aml_lcd_extern_i2c_DLPC3439_probe(struct aml_lcd_extern_driver_s *ext_drv)
{
	int ret = 0;

	ext_config = &ext_drv->config;
	if (i2c_device == NULL) {
		EXTERR("invalid i2c device\n");
		return -1;
	}
	if (ext_drv->config.i2c_addr != i2c_device->client->addr) {
		EXTERR("invalid i2c addr\n");
		return -1;
	}

	ret = lcd_extern_driver_update(ext_drv);

	if (lcd_debug_print_flag)
		EXTPR("%s: %d\n", __func__, ret);
	return ret;
}

static int lcd_extern_i2c_config_from_dts(struct device *dev,
	struct aml_lcd_extern_i2c_dev_s *i2c_device)
{
	int ret;
	struct device_node *np = dev->of_node;
	const char *str;

	ret = of_property_read_string(np, "dev_name", &str);
	if (ret) {
		EXTERR("failed to get dev_i2c_name\n");
		str = "lcd_extern_i2c_DLPC3439";
	}
	strcpy(i2c_device->name, str);

	return 0;
}

static int aml_lcd_extern_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		EXTERR("I2C check functionality failed.");
		return -ENODEV;
	}

	i2c_device = kzalloc(sizeof(struct aml_lcd_extern_i2c_dev_s),
		GFP_KERNEL);
	if (!i2c_device) {
		EXTERR("driver malloc error\n");
		return -ENOMEM;
	}
	i2c_device->client = client;
	lcd_extern_i2c_config_from_dts(&client->dev, i2c_device);
	EXTPR("I2C %s Address: 0x%02x", i2c_device->name,
		i2c_device->client->addr);

	return 0;
}

static int aml_lcd_extern_i2c_remove(struct i2c_client *client)
{
	kfree(i2c_device);
	i2c_set_clientdata(client, NULL);

	return 0;
}

static const struct i2c_device_id aml_lcd_extern_i2c_id[] = {
	{"i2c_DLPC3439", 0},
	{}
};

#ifdef CONFIG_OF
static const struct of_device_id aml_lcd_extern_i2c_dt_match[] = {
	{
		.compatible = "amlogic, lcd_i2c_DLPC3439",
	},
	{},
};
#endif

static struct i2c_driver aml_lcd_extern_i2c_driver = {
	.probe  = aml_lcd_extern_i2c_probe,
	.remove = aml_lcd_extern_i2c_remove,
	.id_table   = aml_lcd_extern_i2c_id,
	.driver = {
		.name  = "i2c_DLPC3439",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = aml_lcd_extern_i2c_dt_match,
#endif
	},
};

module_i2c_driver(aml_lcd_extern_i2c_driver);

MODULE_AUTHOR("AMLOGIC");
MODULE_DESCRIPTION("lcd extern driver");
MODULE_LICENSE("GPL");

