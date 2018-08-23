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


#define BL_EXTERN_NAME			"i2c_lp8556"
#define BL_EXTERN_TYPE			BL_EXTERN_I2C

static struct bl_extern_config_s *ext_config;
static struct aml_bl_extern_i2c_dev_s *i2c_dev;

#define BL_EXTERN_CMD_SIZE	4
static unsigned char init_on_table[] = {
	0x00, 0xa2, 0x20, 0x00,
	0x00, 0xa5, 0x54, 0x00,
	0x00, 0x00, 0xff, 0x00,
	0x00, 0x01, 0x05, 0x00,
	0x00, 0xa2, 0x20, 0x00,
	0x00, 0xa5, 0x54, 0x00,
	0x00, 0xa1, 0xb7, 0x00,
	0x00, 0xa0, 0xff, 0x00,
	0x00, 0x00, 0x80, 0x00,
	0xff, 0x00, 0x00, 0x00, /*ending*/
};

static unsigned char init_off_table[] = {
	0xff, 0x00, 0x00, 0x00, /*ending*/
};

static int i2c_lp8556_power_cmd(unsigned char *init_table)
{
	int i = 0, len;
	int ret = 0;


	BLEX("%s\n", __func__);
	if (ext_config == NULL) {
		BLEXERR("invalid ext_config\n");
		return -1;
	}
	if (i2c_dev == NULL) {
		BLEXERR("invalid i2c device\n");
		return -1;
	}

	len = BL_EXTERN_CMD_SIZE;
	while (i <= BL_EXTERN_INIT_TABLE_MAX) {
		if (init_table[i] == BL_EXTERN_INIT_END) {
			break;
		} else if (init_table[i] == BL_EXTERN_INIT_NONE) {
			/* do nothing, only for delay */
		} else if (init_table[i] == BL_EXTERN_INIT_CMD) {
			ret = bl_extern_i2c_write(i2c_dev->client,
				&init_table[i+1], (len-2));
		} else {
			BLEXERR("%s: %s(%d): power_type %d is invalid\n",
				__func__, ext_config->name,
				ext_config->index, ext_config->type);
		}
		if (init_table[i+len-1] > 0)
			mdelay(init_table[i+len-1]);
		i += len;
	}

	return ret;
}

static int i2c_lp8556_power_ctrl(int flag)
{
	int ret = 0;

	BLEX("%s\n", __func__);

	if (ext_config == NULL) {
		BLEXERR("invalid ext_config\n");
		return -1;
	}

	if (flag)
		ret = i2c_lp8556_power_cmd(init_on_table);
	else
		ret = i2c_lp8556_power_cmd(init_off_table);

	BLEX("%s: %s(%d): %d\n",
		__func__, ext_config->name, ext_config->index, flag);
	return ret;
}

static int i2c_lp8556_power_on(void)
{
	int ret;

	BLEX("%s\n", __func__);

	ret = i2c_lp8556_power_ctrl(1);

	return ret;
}

static int i2c_lp8556_power_off(void)
{
	int ret;

	ret = i2c_lp8556_power_ctrl(0);
	return ret;
}

static int i2c_lp8556_set_level(unsigned int level)
{
	unsigned char tData[3];
	int ret = 0;

	if (i2c_dev == NULL) {
		BLEXERR("invalid i2c device\n");
		return -1;
	}

	tData[0] = 0x0;
	tData[1] = level & 0xff;
	ret = bl_extern_i2c_write(i2c_dev->client, tData, 2);
	return ret;
}

static int i2c_lp8556_update(void)
{
	struct aml_bl_extern_driver_s *bl_extern = aml_bl_extern_get_driver();

	if (bl_extern == NULL) {
		BLEXERR("%s driver is null\n", BL_EXTERN_NAME);
		return -1;
	}

	ext_config = &bl_extern->config;
	i2c_dev = aml_bl_extern_i2c_get_dev();

	bl_extern->device_power_on = i2c_lp8556_power_on;
	bl_extern->device_power_off = i2c_lp8556_power_off;
	bl_extern->device_bri_update = i2c_lp8556_set_level;

	return 0;
}

int i2c_lp8556_probe(void)
{
	int ret = 0;

	ret = i2c_lp8556_update();

	BLEX("%s: %d\n", __func__, ret);
	return ret;
}

int i2c_lp8556_remove(void)
{
	return 0;
}

