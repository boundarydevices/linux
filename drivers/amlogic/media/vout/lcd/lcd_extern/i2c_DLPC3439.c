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

#define LCD_EXTERN_I2C_ADDR       (0x1b) /* 7bit address */
#define LCD_EXTERN_I2C_ADDR2      (0xff) /* 7bit address */
#define LCD_EXTERN_I2C_BUS        LCD_EXTERN_I2C_BUS_2

static struct lcd_extern_config_s *ext_config;
static struct aml_lcd_extern_i2c_dev_s *i2c_dev;


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

static int lcd_extern_power_on(void)
{
	int ret = 0;

	lcd_extern_pinmux_set(1);

	if (i2c_dev == NULL) {
		EXTERR("invalid i2c device\n");
		return -1;
	}
	lcd_extern_i2c_write(i2c_dev->client, data_1, 9);
	lcd_extern_i2c_write(i2c_dev->client, data_2, 5);
	lcd_extern_i2c_write(i2c_dev->client, data_3, 5);
	lcd_extern_i2c_write(i2c_dev->client, data_4, 2);
	lcd_extern_i2c_write(i2c_dev->client, data_5, 2);

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

	if (ext_drv == NULL) {
		EXTERR("%s: driver is null\n", LCD_EXTERN_NAME);
		return -1;
	}

	ext_drv->power_on  = lcd_extern_power_on;
	ext_drv->power_off = lcd_extern_power_off;

	return ret;
}

int aml_lcd_extern_i2c_DLPC3439_probe(struct aml_lcd_extern_driver_s *ext_drv)
{
	int ret = 0;

	ext_config = ext_drv->config;

	i2c_dev = lcd_extern_get_i2c_device(ext_config->i2c_addr);
	if (i2c_dev == NULL) {
		EXTERR("invalid i2c device\n");
		return -1;
	}
	EXTPR("get i2c device: %s, addr 0x%02x OK\n",
		i2c_dev->name, i2c_dev->client->addr);

	ret = lcd_extern_driver_update(ext_drv);

	if (lcd_debug_print_flag)
		EXTPR("%s: %d\n", __func__, ret);
	return ret;
}

int aml_lcd_extern_i2c_DLPC3439_remove(void)
{
	i2c_dev = NULL;
	ext_config = NULL;

	return 0;
}

