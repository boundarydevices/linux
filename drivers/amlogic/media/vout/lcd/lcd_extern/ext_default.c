/*
 * drivers/amlogic/media/vout/lcd/lcd_extern/ext_default.c
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

#define LCD_EXTERN_INDEX          0
#define LCD_EXTERN_NAME           "ext_default"

#define LCD_EXTERN_TYPE           LCD_EXTERN_I2C

#define LCD_EXTERN_I2C_ADDR       (0x1c) /* 7bit address */
#define LCD_EXTERN_I2C_ADDR2      (0xff) /* 7bit address */
#define LCD_EXTERN_I2C_BUS        AML_I2C_MASTER_A

#define SPI_GPIO_CS               0 /* index */
#define SPI_GPIO_CLK              1 /* index */
#define SPI_GPIO_DATA             2 /* index */
#define SPI_CLK_FREQ              10000 /* Hz */
#define SPI_CLK_POL               1

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

static void set_lcd_csb(unsigned int v)
{
	lcd_extern_gpio_set(ext_config->spi_gpio_cs, v);
	udelay(ext_config->spi_delay_us);
}

static void set_lcd_scl(unsigned int v)
{
	lcd_extern_gpio_set(ext_config->spi_gpio_clk, v);
	udelay(ext_config->spi_delay_us);
}

static void set_lcd_sda(unsigned int v)
{
	lcd_extern_gpio_set(ext_config->spi_gpio_data, v);
	udelay(ext_config->spi_delay_us);
}

static void spi_gpio_init(void)
{
	set_lcd_csb(1);
	set_lcd_scl(1);
	set_lcd_sda(1);
}

static void spi_gpio_off(void)
{
	set_lcd_sda(0);
	set_lcd_scl(0);
	set_lcd_csb(0);
}

static void spi_write_byte(unsigned char data)
{
	int i;

	for (i = 0; i < 8; i++) {
		set_lcd_scl(0);
		if (data & 0x80)
			set_lcd_sda(1);
		else
			set_lcd_sda(0);
		data <<= 1;
		set_lcd_scl(1);
	}
}

static int lcd_extern_spi_write(unsigned char *buf, int len)
{
	int i;

	if (len < 2) {
		EXTERR("%s: len %d error\n", __func__, len);
		return -1;
	}

	set_lcd_csb(0);

	for (i = 0; i < len; i++)
		spi_write_byte(buf[i]);

	set_lcd_csb(1);
	set_lcd_scl(1);
	set_lcd_sda(1);
	udelay(ext_config->spi_delay_us);

	return 0;
}

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

static int lcd_extern_power_cmd_dynamic_size(unsigned char *init_table,
		int flag)
{
	int i = 0, step = 0, max_len = 0;
	unsigned char type, cmd_size;
	int ret = 0;

	if (flag)
		max_len = LCD_EXTERN_INIT_ON_MAX;
	else
		max_len = LCD_EXTERN_INIT_OFF_MAX;

	switch (ext_config->type) {
	case LCD_EXTERN_I2C:
		while ((i + 2) < max_len) {
			type = init_table[i];
			if (type == LCD_EXTERN_INIT_END)
				break;
			if (lcd_debug_print_flag) {
				EXTPR("%s: step %d: type=0x%02x, cmd_size=%d\n",
					__func__, step,
					init_table[i], init_table[i+1]);
			}
			cmd_size = init_table[i+1];
			if ((i + 2 + cmd_size) > max_len)
				break;

			if (type == LCD_EXTERN_INIT_NONE) {
				if (cmd_size < 1) {
					EXTERR("step %d: invalid cmd_size %d\n",
						step, cmd_size);
					i += (cmd_size + 2);
					step++;
					continue;
				}
				/* do nothing, only for delay */
				if (init_table[i+2] > 0)
					mdelay(init_table[i+2]);
			} else if (type == LCD_EXTERN_INIT_GPIO) {
				if (cmd_size < 3) {
					EXTERR("step %d: invalid cmd_size %d\n",
						step, cmd_size);
					i += (cmd_size + 2);
					step++;
					continue;
				}
				if (init_table[i+2] < LCD_GPIO_MAX) {
					lcd_extern_gpio_set(init_table[i+2],
						init_table[i+3]);
				}
				if (init_table[i+4] > 0)
					mdelay(init_table[i+4]);
			} else if (type == LCD_EXTERN_INIT_CMD) {
				if (i2c_device == NULL) {
					EXTERR("invalid i2c device\n");
					return -1;
				}
				ret = lcd_extern_i2c_write(
					i2c_device->client,
					&init_table[i+2], (cmd_size-1));
				if (init_table[i+cmd_size+1] > 0)
					mdelay(init_table[i+cmd_size+1]);
			} else if (type == LCD_EXTERN_INIT_CMD2) {
				EXTPR("%s not support cmd2\n", __func__);
			} else {
				EXTERR("%s(%d: %s): type %d invalid\n",
					__func__, ext_config->index,
					ext_config->name, ext_config->type);
			}
			i += (cmd_size + 2);
			step++;
		}
		break;
	case LCD_EXTERN_SPI:
		while ((i + 2) < max_len) {
			type = init_table[i];
			if (type == LCD_EXTERN_INIT_END)
				break;
			if (lcd_debug_print_flag) {
				EXTPR("%s: step %d: type=0x%02x, cmd_size=%d\n",
					__func__, step,
					init_table[i], init_table[i+1]);
			}
			cmd_size = init_table[i+1];
			if ((i + 2 + cmd_size) > max_len)
				break;

			if (type == LCD_EXTERN_INIT_NONE) {
				if (cmd_size < 1) {
					EXTERR("step %d: invalid cmd_size %d\n",
						step, cmd_size);
					i += (cmd_size + 2);
					step++;
					continue;
				}
				/* do nothing, only for delay */
				if (init_table[i+2] > 0)
					mdelay(init_table[i+2]);
			} else if (type == LCD_EXTERN_INIT_GPIO) {
				if (cmd_size < 3) {
					EXTERR("step %d: invalid cmd_size %d\n",
						step, cmd_size);
					i += (cmd_size + 2);
					step++;
					continue;
				}
				if (init_table[i+2] < LCD_GPIO_MAX) {
					lcd_extern_gpio_set(init_table[i+2],
						init_table[i+3]);
				}
				if (init_table[i+4] > 0)
					mdelay(init_table[i+4]);
			} else if (type == LCD_EXTERN_INIT_CMD) {
				ret = lcd_extern_spi_write(
					&init_table[i+2], (cmd_size-1));
				if (init_table[i+cmd_size+1] > 0)
					mdelay(init_table[i+cmd_size+1]);
			} else {
				EXTERR("%s(%d: %s): type %d invalid\n",
					__func__, ext_config->index,
					ext_config->name, ext_config->type);
			}
			i += (cmd_size + 2);
			step++;
		}
		break;
	default:
		EXTERR("%s(%d: %s): extern_type %d is not support\n",
			__func__, ext_config->index,
			ext_config->name, ext_config->type);
		break;
	}

	return ret;
}

static int lcd_extern_power_cmd_fixed_size(unsigned char *init_table, int flag)
{
	int i = 0, step = 0, max_len = 0;
	unsigned char type, cmd_size;
	int ret = 0;

	if (flag)
		max_len = LCD_EXTERN_INIT_ON_MAX;
	else
		max_len = LCD_EXTERN_INIT_OFF_MAX;

	cmd_size = ext_config->cmd_size;
	switch (ext_config->type) {
	case LCD_EXTERN_I2C:
		while ((i + cmd_size) <= max_len) {
			type = init_table[i];
			if (type == LCD_EXTERN_INIT_END)
				break;
			if (lcd_debug_print_flag) {
				EXTPR("%s: step %d: type=0x%02x, cmd_size=%d\n",
					__func__, step, type, cmd_size);
			}
			if (type == LCD_EXTERN_INIT_NONE) {
				/* do nothing, only for delay */
			} else if (type == LCD_EXTERN_INIT_GPIO) {
				if (init_table[i+1] < LCD_GPIO_MAX) {
					lcd_extern_gpio_set(init_table[i+1],
						init_table[i+2]);
				}
			} else if (type == LCD_EXTERN_INIT_CMD) {
				if (i2c_device == NULL) {
					EXTERR("invalid i2c device\n");
					return -1;
				}
				ret = lcd_extern_i2c_write(
					i2c_device->client,
					&init_table[i+1], (cmd_size-2));
			} else if (type == LCD_EXTERN_INIT_CMD2) {
				EXTPR("%s not support cmd2\n", __func__);
			} else {
				EXTERR("%s(%d: %s): type %d invalid\n",
					__func__, ext_config->index,
					ext_config->name, ext_config->type);
			}
			if (init_table[i+cmd_size-1] > 0)
				mdelay(init_table[i+cmd_size-1]);
			i += cmd_size;
			step++;
		}
		break;
	case LCD_EXTERN_SPI:
		while ((i + cmd_size) <= max_len) {
			type = init_table[i];
			if (type == LCD_EXTERN_INIT_END)
				break;
			if (lcd_debug_print_flag) {
				EXTPR("%s: step %d: type=0x%02x, cmd_size=%d\n",
					__func__, step, type, cmd_size);
			}
			if (type == LCD_EXTERN_INIT_NONE) {
				/* do nothing, only for delay */
			} else if (type == LCD_EXTERN_INIT_GPIO) {
				if (init_table[i+1] < LCD_GPIO_MAX) {
					lcd_extern_gpio_set(init_table[i+1],
						init_table[i+2]);
				}
			} else if (type == LCD_EXTERN_INIT_CMD) {
				ret = lcd_extern_spi_write(&init_table[i+1],
					(cmd_size-2));
			} else {
				EXTERR("%s(%d: %s): type %d invalid\n",
					__func__, ext_config->index,
					ext_config->name, ext_config->type);
			}
			if (init_table[i+cmd_size-1] > 0)
				mdelay(init_table[i+cmd_size-1]);
			i += cmd_size;
			step++;
		}
		break;
	default:
		EXTERR("%s(%d: %s): extern_type %d is not support\n",
			__func__, ext_config->index,
			ext_config->name, ext_config->type);
		break;
	}

	return ret;
}

static int lcd_extern_power_cmd(unsigned char *init_table, int flag)
{
	unsigned char cmd_size;
	int ret = 0;

	cmd_size = ext_config->cmd_size;
	if (cmd_size < 1) {
		EXTERR("%s: cmd_size %d is invalid\n", __func__, cmd_size);
		return -1;
	}
	if (init_table == NULL) {
		EXTERR("%s: init_table %d is NULL\n", __func__, flag);
		return -1;
	}

	if (cmd_size == LCD_EXTERN_CMD_SIZE_DYNAMIC)
		ret = lcd_extern_power_cmd_dynamic_size(init_table, flag);
	else
		ret = lcd_extern_power_cmd_fixed_size(init_table, flag);

	return ret;
}

static int lcd_extern_power_ctrl(int flag)
{
	int ret = 0;

	if (ext_config->type == LCD_EXTERN_SPI)
		spi_gpio_init();

	if (flag)
		ret = lcd_extern_power_cmd(ext_config->table_init_on, 1);
	else
		ret = lcd_extern_power_cmd(ext_config->table_init_off, 0);

	if (ext_config->type == LCD_EXTERN_SPI)
		spi_gpio_off();

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

	if (ext_drv->config.type == LCD_EXTERN_MAX) { /* default for no dts */
		ext_drv->config.index = LCD_EXTERN_INDEX;
		ext_drv->config.type = LCD_EXTERN_TYPE;
		strcpy(ext_drv->config.name, LCD_EXTERN_NAME);
		ext_drv->config.cmd_size = LCD_EXTERN_CMD_SIZE;
		switch (ext_drv->config.type) {
		case LCD_EXTERN_I2C:
			ext_drv->config.i2c_addr = LCD_EXTERN_I2C_ADDR;
			ext_drv->config.i2c_addr2 = LCD_EXTERN_I2C_ADDR2;
			ext_drv->config.i2c_bus = LCD_EXTERN_I2C_BUS;
			break;
		case LCD_EXTERN_SPI:
			ext_drv->config.spi_gpio_cs = SPI_GPIO_CS;
			ext_drv->config.spi_gpio_clk = SPI_GPIO_CLK;
			ext_drv->config.spi_gpio_data = SPI_GPIO_DATA;
			ext_drv->config.spi_clk_freq = SPI_CLK_FREQ;
			ext_drv->config.spi_clk_pol = SPI_CLK_POL;
			break;
		default:
			break;
		}
	}
	if (ext_drv->config.table_init_loaded == 0) {
		ext_drv->config.table_init_on  = init_on_table;
		ext_drv->config.table_init_off = init_off_table;
	}
	if (ext_drv->config.type == LCD_EXTERN_SPI) {
		ext_drv->config.spi_delay_us =
			1000000 / ext_drv->config.spi_clk_freq;
	}

	ext_drv->reg_read  = lcd_extern_reg_read;
	ext_drv->reg_write = lcd_extern_reg_write;
	ext_drv->power_on  = lcd_extern_power_on;
	ext_drv->power_off = lcd_extern_power_off;

	return 0;
}

int aml_lcd_extern_default_probe(struct aml_lcd_extern_driver_s *ext_drv)
{
	int ret = 0;

	ext_config = &ext_drv->config;

	switch (ext_drv->config.type) {
	case LCD_EXTERN_I2C:
		if (i2c_device == NULL) {
			EXTERR("invalid i2c device\n");
			return -1;
		}
		if (ext_drv->config.i2c_addr != i2c_device->client->addr) {
			EXTERR("invalid i2c addr\n");
			return -1;
		}
		break;
	case LCD_EXTERN_SPI:
		break;
	default:
		break;
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
		str = "lcd_extern_i2c_default";
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
	{"ext_default", 0},
	{}
};

#ifdef CONFIG_OF
static const struct of_device_id aml_lcd_extern_i2c_dt_match[] = {
	{
		.compatible = "amlogic, lcd_ext_default",
	},
	{},
};
#endif

static struct i2c_driver aml_lcd_extern_i2c_driver = {
	.probe  = aml_lcd_extern_i2c_probe,
	.remove = aml_lcd_extern_i2c_remove,
	.id_table   = aml_lcd_extern_i2c_id,
	.driver = {
		.name  = "ext_default",
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

