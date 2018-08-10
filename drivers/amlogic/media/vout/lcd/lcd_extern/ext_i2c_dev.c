/*
 * drivers/amlogic/media/vout/lcd/lcd_extern/ext_i2c_dev.c
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

static struct aml_lcd_extern_i2c_dev_s *i2c_dev[LCD_EXT_I2C_DEV_MAX];
static unsigned int i2c_dev_cnt;

struct aml_lcd_extern_i2c_dev_s *lcd_extern_get_i2c_device(unsigned char addr)
{
	struct aml_lcd_extern_i2c_dev_s *i2c_device = NULL;
	int i;

	/*pr_info("%s: addr=0x%02x\n", __func__, addr);*/
	for (i = 0; i < i2c_dev_cnt; i++) {
		if (i2c_dev[i] == NULL)
			break;
		if (i2c_dev[i]->client->addr == addr) {
			i2c_device = i2c_dev[i];
			break;
		}
	}
	return i2c_device;
}

int lcd_extern_i2c_write(struct i2c_client *i2client,
		unsigned char *buff, unsigned int len)
{
	struct i2c_msg msg;
	int ret = 0;

	if (i2client == NULL) {
		EXTERR("i2client is null\n");
		return -1;
	}

	msg.addr = i2client->addr;
	msg.flags = 0;
	msg.len = len;
	msg.buf = buff;

	ret = i2c_transfer(i2client->adapter, &msg, 1);
	if (ret < 0)
		EXTERR("i2c write failed [addr 0x%02x]\n", i2client->addr);

	return ret;
}

int lcd_extern_i2c_read(struct i2c_client *i2client,
		unsigned char *buff, unsigned int len)
{
	struct i2c_msg msgs[2];
	int ret = 0;

	if (i2client == NULL) {
		EXTERR("i2client is null\n");
		return -1;
	}

	msgs[0].addr = i2client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = buff;
	msgs[1].addr = i2client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = buff;

	ret = i2c_transfer(i2client->adapter, msgs, 2);
	if (ret < 0) {
		EXTERR("%s: i2c transfer failed [addr 0x%02x]\n",
			__func__, i2client->addr);
	}

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
		strcpy(i2c_device->name, "lcd_ext_i2c0");
	} else {
		strncpy(i2c_device->name, str, 30);
		i2c_device->name[29] = '\0';
	}

	return 0;
}

static int lcd_extern_i2c_dev_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	if (i2c_dev_cnt >= LCD_EXT_I2C_DEV_MAX) {
		EXTERR("i2c_dev_cnt reach max\n");
		return -ENODEV;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		EXTERR("i2c0_dev check functionality failed");
		return -ENODEV;
	}

	i2c_dev[i2c_dev_cnt] = kzalloc(sizeof(struct aml_lcd_extern_i2c_dev_s),
		GFP_KERNEL);
	if (!i2c_dev[i2c_dev_cnt]) {
		EXTERR("i2c0_dev %d driver malloc error\n", i2c_dev_cnt);
		return -ENOMEM;
	}

	i2c_dev[i2c_dev_cnt]->client = client;
	lcd_extern_i2c_config_from_dts(&client->dev, i2c_dev[i2c_dev_cnt]);
	EXTPR("i2c_dev probe: %s address 0x%02x OK",
		i2c_dev[i2c_dev_cnt]->name,
		i2c_dev[i2c_dev_cnt]->client->addr);

	i2c_dev_cnt++;

	return 0;
}

static int lcd_extern_i2c_dev_remove(struct i2c_client *client)
{
	int i;

	if (i2c_dev_cnt == 0)
		return 0;

	for (i = 0; i < i2c_dev_cnt; i++) {
		kfree(i2c_dev[i]);
		i2c_dev[i] = NULL;
	}
	i2c_dev_cnt = 0;

	return 0;
}

static const struct i2c_device_id lcd_extern_i2c_dev_id[] = {
	{"lcd_ext_i2c", 0},
	{}
};

#ifdef CONFIG_OF
static const struct of_device_id lcd_extern_i2c_dev_dt_match[] = {
	{
		.compatible = "lcd_ext, i2c",
	},
	{},
};
#endif

static struct i2c_driver lcd_extern_i2c_dev_driver = {
	.probe    = lcd_extern_i2c_dev_probe,
	.remove   = lcd_extern_i2c_dev_remove,
	.id_table = lcd_extern_i2c_dev_id,
	.driver = {
		.name  = "lcd_ext_i2c",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = lcd_extern_i2c_dev_dt_match,
#endif
	},
};

static int __init aml_lcd_extern_i2c_dev_init(void)
{
	int ret;

	if (lcd_debug_print_flag)
		EXTPR("%s\n", __func__);

	i2c_dev_cnt = 0;

	ret = i2c_add_driver(&lcd_extern_i2c_dev_driver);
	if (ret) {
		EXTERR("i2c0_dev driver register failed\n");
		return -ENODEV;
	}

	return ret;
}

static void __exit aml_lcd_extern_i2c_dev_exit(void)
{
	i2c_del_driver(&lcd_extern_i2c_dev_driver);
}


module_init(aml_lcd_extern_i2c_dev_init);
module_exit(aml_lcd_extern_i2c_dev_exit);

MODULE_AUTHOR("AMLOGIC");
MODULE_DESCRIPTION("lcd extern i2c device driver");
MODULE_LICENSE("GPL");

