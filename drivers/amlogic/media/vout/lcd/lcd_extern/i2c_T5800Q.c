/*
 * drivers/amlogic/media/vout/lcd/lcd_extern/i2c_T5800Q.c
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

#define LCD_EXTERN_NAME			"i2c_T5800Q"

static struct i2c_client *aml_T5800Q_i2c_client;
static struct lcd_extern_config_s *ext_config;

#define LCD_EXTERN_CMD_SIZE        9
static unsigned char init_on_table[] = {
	0x00, 0x20, 0x01, 0x02, 0x00, 0x40, 0xFF, 0x00, 0x00,
	0x00, 0x80, 0x02, 0x00, 0x40, 0x62, 0x51, 0x73, 0x00,
	0x00, 0x61, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0xC1, 0x05, 0x0F, 0x00, 0x08, 0x70, 0x00, 0x00,
	0x00, 0x13, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x3D, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0xED, 0x0D, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x23, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0A,  /* delay 10ms */
	0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* ending */
};

static unsigned char init_off_table[] = {
	0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* ending */
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
#if 0
static int lcd_extern_i2c_read(struct i2c_client *i2client,
		unsigned char *buff, unsigned int len)
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
		}
	};

	ret = i2c_transfer(i2client->adapter, msgs, 2);
	if (ret < 0)
		EXTERR("i2c read failed [addr 0x%02x]\n", i2client->addr);

	return ret;
}
#endif

static int lcd_extern_reg_read(unsigned char reg, unsigned char *buf)
{
	int ret = 0;

	return ret;
}

static int lcd_extern_reg_write(unsigned char reg, unsigned char value)
{
	int ret = 0;

	return ret;
}

static int lcd_extern_power_cmd(unsigned char *init_table)
{
	int i = 0, len;
	int ret = 0;

	len = ext_config->cmd_size;
	if (len < 1) {
		EXTERR("%s: cmd_size %d is invalid\n", __func__, len);
		return -1;
	}

	while (i <= LCD_EXTERN_INIT_TABLE_MAX) {
		if (init_table[i] == LCD_EXTERN_INIT_END) {
			break;
		} else if (init_table[i] == LCD_EXTERN_INIT_NONE) {
			/* do nothing, only for delay */
		} else if (init_table[i] == LCD_EXTERN_INIT_GPIO) {
			if (init_table[i+1] < LCD_GPIO_MAX) {
				lcd_extern_gpio_set(init_table[i+1],
					init_table[i+2]);
			}
		} else if (init_table[i] == LCD_EXTERN_INIT_CMD) {
			ret = lcd_extern_i2c_write(aml_T5800Q_i2c_client,
				&init_table[i+1], (len-2));
		} else {
			EXTERR("%s(%d: %s): power_type %d is invalid\n",
				__func__, ext_config->index,
				ext_config->name, ext_config->type);
		}
		if (init_table[i+len-1] > 0)
			mdelay(init_table[i+len-1]);
		i += len;
	}

	return ret;
}

static int lcd_extern_power_ctrl(int flag)
{
	int ret = 0;

	if (flag)
		ret = lcd_extern_power_cmd(ext_config->table_init_on);
	else
		ret = lcd_extern_power_cmd(ext_config->table_init_off);

	EXTPR("%s(%d: %s): %d\n",
		__func__, ext_config->index, ext_config->name, flag);
	return ret;
}

static int lcd_extern_power_on(void)
{
	int ret;

	ret = lcd_extern_power_ctrl(1);
	return ret;
}

static int lcd_extern_power_off(void)
{
	int ret;

	ret = lcd_extern_power_ctrl(0);
	return ret;
}

static int lcd_extern_driver_update(struct aml_lcd_extern_driver_s *ext_drv)
{
	if (ext_drv == NULL) {
		EXTERR("%s driver is null\n", LCD_EXTERN_NAME);
		return -1;
	}

	if (ext_drv->config.table_init_loaded == 0) {
		ext_drv->config.table_init_on = init_on_table;
		ext_drv->config.table_init_off = init_off_table;
	}
	ext_drv->reg_read  = lcd_extern_reg_read;
	ext_drv->reg_write = lcd_extern_reg_write;
	ext_drv->power_on  = lcd_extern_power_on;
	ext_drv->power_off = lcd_extern_power_off;

	return 0;
}

/* debug function */
static const char *lcd_extern_debug_usage_str = {
"Usage:\n"
"    echo <reg> <> ... <> > write ; T5800Q i2c command write, 7 parameters without address\n"
};

static ssize_t lcd_extern_debug_help(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", lcd_extern_debug_usage_str);
}

static ssize_t lcd_extern_debug_write(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned int ret;
	unsigned int temp[8];
	unsigned char data[8];
	int i;

	memset(temp, 0, (sizeof(unsigned int) * 8));
	ret = sscanf(buf, "%x %x %x %x %x %x %x",
		&temp[0], &temp[1], &temp[2], &temp[3],
		&temp[4], &temp[5], &temp[6]);
	EXTPR("T5800Q i2c write:\n");
	for (i = 0; i < 7; i++) {
		data[i] = (unsigned char)temp[i];
		pr_info("0x%02x ", data[i]);
	}
	pr_info("\n");
	lcd_extern_i2c_write(aml_T5800Q_i2c_client, data, 7);

	if (ret != 1 || ret != 2)
		return -EINVAL;

	return count;
}

static struct class_attribute lcd_extern_class_attrs[] = {
	__ATTR(write, 0644,
		lcd_extern_debug_help, lcd_extern_debug_write),
};

static struct class *debug_class;
static int creat_lcd_extern_class(void)
{
	int i;

	debug_class = class_create(THIS_MODULE, LCD_EXTERN_NAME);
	if (IS_ERR(debug_class)) {
		EXTERR("create debug class failed\n");
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(lcd_extern_class_attrs); i++) {
		if (class_create_file(debug_class,
			&lcd_extern_class_attrs[i])) {
			EXTERR("create debug attribute %s failed\n",
				lcd_extern_class_attrs[i].attr.name);
		}
	}

	return 0;
}

#if 0
static int remove_lcd_extern_class(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(lcd_extern_class_attrs); i++)
		class_remove_file(debug_class, &lcd_extern_class_attrs[i]);

	class_destroy(debug_class);
	debug_class = NULL;

	return 0;
}
#endif
/* ********************************************************* */

static int aml_T5800Q_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		EXTERR("%s: functionality check failed\n", __func__);
	else
		aml_T5800Q_i2c_client = client;

	EXTPR("%s OK\n", __func__);
	return 0;
}

static int aml_T5800Q_i2c_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id aml_T5800Q_i2c_id[] = {
	{LCD_EXTERN_NAME, 0},
	{ }
};
/* MODULE_DEVICE_TABLE(i2c, aml_T5800Q_id); */

static struct i2c_driver aml_T5800Q_i2c_driver = {
	.probe    = aml_T5800Q_i2c_probe,
	.remove   = aml_T5800Q_i2c_remove,
	.id_table = aml_T5800Q_i2c_id,
	.driver = {
		.name = LCD_EXTERN_NAME,
		.owner = THIS_MODULE,
	},
};

int aml_lcd_extern_i2c_T5800Q_probe(struct aml_lcd_extern_driver_s *ext_drv)
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

	if (!aml_T5800Q_i2c_client) {
		ret = i2c_add_driver(&aml_T5800Q_i2c_driver);
		if (ret) {
			EXTERR("%s add i2c_driver failed\n",
				ext_drv->config.name);
			return -1;
		}
	}

	ret = lcd_extern_driver_update(ext_drv);
	creat_lcd_extern_class();

	if (lcd_debug_print_flag)
		EXTPR("%s: %d\n", __func__, ret);
	return ret;
}
