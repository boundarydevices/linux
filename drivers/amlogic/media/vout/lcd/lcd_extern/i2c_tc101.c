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

#define LCD_EXTERN_NAME	          "i2c_tc101"

#define LCD_EXTERN_I2C_ADDR       (0x7e) /* 7bit address */
#define LCD_EXTERN_I2C_ADDR2      (0xff) /* 7bit address */
#define LCD_EXTERN_I2C_BUS        LCD_EXTERN_I2C_BUS_2

static struct lcd_extern_config_s *ext_config;
static struct aml_lcd_extern_i2c_dev_s *i2c0_dev;
static struct aml_lcd_extern_i2c_dev_s *i2c1_dev;

static unsigned char init_on_table[][3] = {
	/* {0xff, 0xff, 20},//delay mark(20ms) */
	{0xf8, 0x30, 0xb2},
	{0xf8, 0x33, 0xc2},
	{0xf8, 0x31, 0xf0},
	{0xf8, 0x40, 0x80},
	{0xf8, 0x81, 0xec},
	{0xff, 0xff, 0xff},/* end mark */
};

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

static int lcd_extern_power_on(void)
{
	unsigned char tData[4];
	int i = 0, ending_flag = 0;
	int ret = 0;

	lcd_extern_pinmux_set(1);

	if (i2c0_dev == NULL) {
		EXTERR("invalid i2c device\n");
		return -1;
	}
	while (ending_flag == 0) {
		if ((init_on_table[i][0] == 0xff) &&
			(init_on_table[i][1] == 0xff)) { /* special mark */
			if (init_on_table[i][2] == 0xff) /* ending flag */
				ending_flag = 1;
			else /* delay flag */
				mdelay(init_on_table[i][2]);
		} else {
			tData[0] = init_on_table[i][0];
			tData[1] = init_on_table[i][1];
			tData[2] = init_on_table[i][2];
			lcd_extern_i2c_write(i2c0_dev->client, tData, 3);
		}
		i++;
	}
	EXTPR("%s\n", __func__);
	return ret;
}

static int lcd_extern_power_off(void)
{
	int ret = 0;

	lcd_extern_pinmux_set(0);
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

int aml_lcd_extern_i2c_tc101_probe(struct aml_lcd_extern_driver_s *ext_drv)
{
	int ret = 0;

	ext_config = ext_drv->config;

	i2c0_dev = lcd_extern_get_i2c_device(ext_config->i2c_addr);
	if (i2c0_dev == NULL) {
		EXTERR("invalid i2c0 device\n");
		return -1;
	}
	EXTPR("get i2c0 device: %s, addr 0x%02x OK\n",
		i2c0_dev->name, i2c0_dev->client->addr);

	i2c1_dev = lcd_extern_get_i2c_device(ext_config->i2c_addr2);
	if (i2c1_dev == NULL) {
		EXTERR("invalid i2c1 device\n");
		i2c0_dev = NULL;
		return -1;
	}
	EXTPR("get i2c1 device: %s, addr 0x%02x OK\n",
		i2c1_dev->name, i2c1_dev->client->addr);

	ret = lcd_extern_driver_update(ext_drv);

	if (lcd_debug_print_flag)
		EXTPR("%s: %d\n", __func__, ret);
	return ret;
}

int aml_lcd_extern_i2c_tc101_remove(void)
{
	i2c0_dev = NULL;
	i2c1_dev = NULL;
	ext_config = NULL;

	return 0;
}
