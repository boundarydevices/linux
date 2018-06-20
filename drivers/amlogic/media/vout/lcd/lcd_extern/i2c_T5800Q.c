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

static struct lcd_extern_config_s *ext_config;
static struct aml_lcd_extern_i2c_dev_s *i2c_device;

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

static int lcd_extern_power_cmd(unsigned char *init_table, int flag)
{
	int i = 0, max_len = 0, step = 0;
	unsigned char cmd_size;
	int ret = 0;

	cmd_size = ext_config->cmd_size;
	if (cmd_size < 1) {
		EXTERR("%s: cmd_size %d is invalid\n", __func__, cmd_size);
		return -1;
	}
	if (cmd_size == LCD_EXTERN_CMD_SIZE_DYNAMIC) {
		EXTPR("%s: cmd_size dynamic length is not support\n", __func__);
		return -1;
	}
	if (init_table == NULL) {
		EXTERR("%s: init_table %d is NULL\n", __func__, flag);
		return -1;
	}

	if (flag)
		max_len = LCD_EXTERN_INIT_ON_MAX;
	else
		max_len = LCD_EXTERN_INIT_OFF_MAX;

	while ((i + cmd_size) <= max_len) {
		if (init_table[i] == LCD_EXTERN_INIT_END)
			break;
		if (lcd_debug_print_flag) {
			EXTPR("%s: step %d: type=0x%02x, cmd_size=%d\n",
				__func__, step, init_table[i], cmd_size);
		}
		if (init_table[i] == LCD_EXTERN_INIT_NONE) {
			/* do nothing, only for delay */
		} else if (init_table[i] == LCD_EXTERN_INIT_GPIO) {
			if (init_table[i+1] < LCD_GPIO_MAX) {
				lcd_extern_gpio_set(init_table[i+1],
					init_table[i+2]);
			}
		} else if (init_table[i] == LCD_EXTERN_INIT_CMD) {
			if (i2c_device == NULL) {
				EXTERR("invalid i2c device\n");
				return -1;
			}
			ret = lcd_extern_i2c_write(i2c_device->client,
				&init_table[i+1], (cmd_size-2));
		} else {
			EXTERR("%s(%d: %s): power_type %d is invalid\n",
				__func__, ext_config->index,
				ext_config->name, ext_config->type);
		}
		if (init_table[i+cmd_size-1] > 0)
			mdelay(init_table[i+cmd_size-1]);
		step++;
		i += cmd_size;
	}

	return ret;
}

static int lcd_extern_power_ctrl(int flag)
{
	int ret = 0;

	if (flag)
		ret = lcd_extern_power_cmd(ext_config->table_init_on, 1);
	else
		ret = lcd_extern_power_cmd(ext_config->table_init_off, 0);

	EXTPR("%s(%d: %s): %d\n",
		__func__, ext_config->index, ext_config->name, flag);
	return ret;
}

static int lcd_extern_power_on(struct aml_lcd_extern_driver_s *ext_drv)
{
	int ret;

	lcd_extern_pinmux_set(ext_drv, 1);
	ret = lcd_extern_power_ctrl(1);
	return ret;
}

static int lcd_extern_power_off(struct aml_lcd_extern_driver_s *ext_drv)
{
	int ret;

	lcd_extern_pinmux_set(ext_drv, 0);
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
	ext_drv->power_on  = lcd_extern_power_on;
	ext_drv->power_off = lcd_extern_power_off;

	return 0;
}

int aml_lcd_extern_i2c_T5800Q_probe(struct aml_lcd_extern_driver_s *ext_drv)
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
	EXTPR("%s: %d\n", __func__, ret);
	return ret;
}

int aml_lcd_extern_i2c_T5800Q_remove(void)
{
	return 0;
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
		str = "lcd_extern_i2c_T5800Q";
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
	//i2c_set_clientdata(client, NULL);

	return 0;
}

static const struct i2c_device_id aml_lcd_extern_i2c_id[] = {
	{"i2c_T5800Q", 0},
	{}
};

#ifdef CONFIG_OF
static const struct of_device_id aml_lcd_extern_i2c_dt_match[] = {
	{
		.compatible = "amlogic, lcd_i2c_T5800Q",
	},
	{},
};
#endif

static struct i2c_driver aml_lcd_extern_i2c_driver = {
	.probe  = aml_lcd_extern_i2c_probe,
	.remove = aml_lcd_extern_i2c_remove,
	.id_table   = aml_lcd_extern_i2c_id,
	.driver = {
		.name  = "i2c_T5800Q",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = aml_lcd_extern_i2c_dt_match,
#endif
	},
};

static int __init aml_lcd_extern_i2c_init(void)
{
	int ret;

	//if (lcd_debug_print_flag)
	EXTPR("%s\n", __func__);

	ret = i2c_add_driver(&aml_lcd_extern_i2c_driver);
	if (ret) {
		EXTERR("driver register failed\n");
		return -ENODEV;
	}
	return ret;
}

static void __exit aml_lcd_extern_i2c_exit(void)
{
	i2c_del_driver(&aml_lcd_extern_i2c_driver);
}


module_init(aml_lcd_extern_i2c_init);
module_exit(aml_lcd_extern_i2c_exit);

//module_i2c_driver(aml_lcd_extern_i2c_driver);

MODULE_AUTHOR("AMLOGIC");
MODULE_DESCRIPTION("lcd extern driver");
MODULE_LICENSE("GPL");

