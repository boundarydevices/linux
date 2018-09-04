/*
 * drivers/amlogic/media/vout/backlight/bl_extern/mipi_lt070me05.c
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
#include <linux/of.h>
#include <linux/amlogic/media/vout/lcd/aml_bl_extern.h>
#include "bl_extern.h"


#define BL_EXTERN_NAME			"mipi_lt070me05"
#define BL_EXTERN_TYPE			BL_EXTERN_MIPI

static int mipi_lt070me05_power_on(void)
{
	return 0;
}

static int mipi_lt070me05_power_off(void)
{
	return 0;
}

/******************** mipi command ********************/
/* format:  data_type, cmd_size, data.... */
/*	data_type=0xff,
 *		0 < cmd_size < 0xff means delay ms,
 *		cmd_size=0 or 0xff means ending.
 *	data_type=0xf0, for gpio control
 *		data0=gpio_index, data1=gpio_value.
 *		data0=gpio_index, data1=gpio_value, data2=delay.
 *	data_type=0xfd, for delay ms
 *		data0=delay, data_1=delay, ..., data_n=delay.
 */
static int mipi_lt070me05_set_level(unsigned int level)
{
	unsigned char payload[] = {0x15, 2, 0x51, 0xe6, 0xff, 0xff};

	payload[3] = level & 0xff;
#ifdef CONFIG_AMLOGIC_LCD_TABLET
	dsi_write_cmd(&payload[0]);
#endif

	return 0;
}

static int mipi_lt070me05_update(void)
{
	struct aml_bl_extern_driver_s *bl_extern = aml_bl_extern_get_driver();

	if (bl_extern == NULL) {
		BLEXERR("%s driver is null\n", BL_EXTERN_NAME);
		return -1;
	}

	bl_extern->device_power_on = mipi_lt070me05_power_on;
	bl_extern->device_power_off = mipi_lt070me05_power_off;
	bl_extern->device_bri_update = mipi_lt070me05_set_level;

	return 0;
}

int mipi_lt070me05_probe(void)
{
	int ret = 0;

	ret = mipi_lt070me05_update();

	BLEX("%s: %d\n", __func__, ret);

	return ret;
}


int mipi_lt070me05_remove(void)
{
	return 0;
}

