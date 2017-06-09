/*
 * drivers/amlogic/media/vout/lcd/lcd_extern/i2c_tc101.c
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

static struct lcd_extern_config_s *ext_config;
static struct i2c_client *aml_tc101_i2c_client;

#define LCD_EXTERN_NAME			"lcd_i2c_tc101"

static unsigned char i2c_init_table[][3] = {
	/* {0xff, 0xff, 20},//delay mark(20ms) */
	{0xf8, 0x30, 0xb2},
	{0xf8, 0x33, 0xc2},
	{0xf8, 0x31, 0xf0},
	{0xf8, 0x40, 0x80},
	{0xf8, 0x81, 0xec},
	{0xff, 0xff, 0xff},/* end mark */
};

static int aml_i2c_write(struct i2c_client *i2client, unsigned char *buff,
		unsigned int len)
{
	int ret = 0;
	struct i2c_msg msg[] = {
		{
			.addr = i2client->addr,
			.flags = 0,
			.len = len,
			.buf = buff,
		},
	};

	ret = i2c_transfer(i2client->adapter, msg, 1);
	if (ret < 0) {
		EXTERR("%s: i2c transfer failed [addr 0x%02x]\n",
			__func__, i2client->addr);
	}

	return ret;
}
#if 0
static int aml_i2c_read(struct i2c_client *i2client, unsigned char *buff,
		unsigned int len)
{
	int ret = 0;
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
		},
	};

	ret = i2c_transfer(i2client->adapter, msgs, 2);
	if (ret < 0) {
		EXTERR("%s: i2c transfer failed [addr 0x%02x]\n",
			__func__, i2client->addr);
	}

	return ret;
}

static int i2c_reg_read(unsigned char reg, unsigned char *buf)
{
	int ret = 0;

	return ret;
}

static int i2c_reg_write(unsigned char reg, unsigned char value)
{
	int ret = 0;

	return ret;
}
#endif

static int lcd_extern_power_on(void)
{
	unsigned char tData[4];
	int i = 0, ending_flag = 0;
	int ret = 0;

	while (ending_flag == 0) {
		if ((i2c_init_table[i][0] == 0xff) &&
			(i2c_init_table[i][1] == 0xff)) { /* special mark */
			if (i2c_init_table[i][2] == 0xff) /* ending flag */
				ending_flag = 1;
			else /* delay flag */
				mdelay(i2c_init_table[i][2]);
		} else {
			tData[0] = i2c_init_table[i][0];
			tData[1] = i2c_init_table[i][1];
			tData[2] = i2c_init_table[i][2];
			aml_i2c_write(aml_tc101_i2c_client, tData, 3);
		}
		i++;
	}
	EXTPR("%s\n", __func__);
	return ret;
}

static int lcd_extern_power_off(void)
{
	int ret = 0;

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

static int aml_tc101_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		EXTERR("%s: functionality check failed\n", __func__);
	else
		aml_tc101_i2c_client = client;

	EXTPR("%s OK\n", __func__);
	return 0;
}

static int aml_tc101_i2c_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id aml_tc101_i2c_id[] = {
	{LCD_EXTERN_NAME, 0},
	{ }
};
/* MODULE_DEVICE_TABLE(i2c, aml_tc101_id); */

static struct i2c_driver aml_tc101_i2c_driver = {
	.probe    = aml_tc101_i2c_probe,
	.remove   = aml_tc101_i2c_remove,
	.id_table = aml_tc101_i2c_id,
	.driver = {
		.name = LCD_EXTERN_NAME,
		.owner = THIS_MODULE,
	},
};

int aml_lcd_extern_i2c_tc101_probe(struct aml_lcd_extern_driver_s *ext_drv)
{
	struct i2c_board_info i2c_info;
	struct i2c_adapter *adapter;
	struct i2c_client *i2c_client;
	int ret = 0;

	ext_config = &ext_drv->config;
	memset(&i2c_info, 0, sizeof(i2c_info));

	adapter = i2c_get_adapter(ext_drv->config.i2c_bus);
	if (!adapter) {
		EXTERR("%s failed to get i2c adapter\n", ext_drv->config.name);
		return -1;
	}

	strncpy(i2c_info.type, ext_drv->config.name, I2C_NAME_SIZE);
	i2c_info.addr = ext_drv->config.i2c_addr;
	i2c_info.platform_data = &ext_drv->config;
	i2c_info.flags = 0;
	if (i2c_info.addr > 0x7f) {
		EXTERR("%s invalid i2c address: 0x%02x\n",
			ext_drv->config.name, ext_drv->config.i2c_addr);
		return -1;
	}
	i2c_client = i2c_new_device(adapter, &i2c_info);
	if (!i2c_client) {
		EXTERR("%s failed to new i2c device\n", ext_drv->config.name);
	} else {
		if (lcd_debug_print_flag) {
			EXTPR("%s new i2c device succeed\n",
				ext_drv->config.name);
		}
	}

	if (!aml_tc101_i2c_client) {
		ret = i2c_add_driver(&aml_tc101_i2c_driver);
		if (ret) {
			EXTERR("%s add i2c_driver failed\n",
				ext_drv->config.name);
			return -1;
		}
	}

	ret = lcd_extern_driver_update(ext_drv);

	if (lcd_debug_print_flag)
		EXTPR("%s: %d\n", __func__, ret);
	return ret;
}

