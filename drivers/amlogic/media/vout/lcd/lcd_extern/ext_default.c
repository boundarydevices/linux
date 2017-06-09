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

static struct i2c_client *aml_default_i2c_client;
static struct i2c_client *aml_default_i2c2_client;
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

static int lcd_extern_spi_write(unsigned char *buf, int len)
{
	EXTPR("to do\n");
	return 0;
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
	if (len == LCD_EXTERN_DYNAMIC_LEN) {
		switch (ext_config->type) {
		case LCD_EXTERN_I2C:
			while (i <= LCD_EXTERN_INIT_TABLE_MAX) {
				if (init_table[i] == LCD_EXTERN_INIT_END) {
					break;
				} else if (init_table[i] ==
						LCD_EXTERN_INIT_NONE) {
					/* do nothing, only for delay */
					if (init_table[i+1] > 0)
						mdelay(init_table[i+1]);
					i += 2;
				} else if (init_table[i] ==
						LCD_EXTERN_INIT_GPIO) {
					if (init_table[i+1] < LCD_GPIO_MAX) {
						lcd_extern_gpio_set(
							init_table[i+1],
							init_table[i+2]);
					}
					if (init_table[i+3] > 0)
						mdelay(init_table[i+3]);
					i += 4;
				} else if (init_table[i] ==
						LCD_EXTERN_INIT_CMD) {
					ret = lcd_extern_i2c_write(
						aml_default_i2c_client,
						&init_table[i+2],
						init_table[i+1]-1);
					if (init_table[i+init_table[i+1]+1] > 0)
						mdelay(init_table[i+
							init_table[i+1]+1]);
					i += (init_table[i+1] + 2);
				} else if (init_table[i] ==
						LCD_EXTERN_INIT_CMD2) {
					ret = lcd_extern_i2c_write(
						aml_default_i2c2_client,
						&init_table[i+2],
						init_table[i+1]-1);
					if (init_table[i+init_table[i+1]+1] > 0)
						mdelay(init_table[i+
							init_table[i+1]+1]);
					i += (init_table[i+1] + 2);
				} else {
					EXTERR("%s(%d: %s): type %d invalid\n",
						__func__, ext_config->index,
						ext_config->name,
						ext_config->type);
				}
			}
			break;
		case LCD_EXTERN_SPI:
			while (i <= LCD_EXTERN_INIT_TABLE_MAX) {
				if (init_table[i] == LCD_EXTERN_INIT_END) {
					break;
				} else if (init_table[i] ==
					LCD_EXTERN_INIT_NONE) {
					/* do nothing, only for delay */
					if (init_table[i+1] > 0)
						mdelay(init_table[i+1]);
					i += 2;
				} else if (init_table[i] ==
					LCD_EXTERN_INIT_GPIO) {
					if (init_table[i+1] < LCD_GPIO_MAX) {
						lcd_extern_gpio_set(
							init_table[i+1],
							init_table[i+2]);
					}
					if (init_table[i+3] > 0)
						mdelay(init_table[i+3]);
					i += 4;
				} else if (init_table[i] ==
					LCD_EXTERN_INIT_CMD) {
					ret = lcd_extern_spi_write(
						&init_table[i+2],
						init_table[i+1]-1);
					if (init_table[i+init_table[i+1]+1] > 0)
						mdelay(init_table[i+
							init_table[i+1]+1]);
					i += (init_table[i+1] + 2);
				} else {
					EXTERR("%s(%d: %s): type %d invalid\n",
						__func__, ext_config->index,
						ext_config->name,
						ext_config->type);
				}
			}
			break;
		default:
			EXTERR("%s(%d: %s): extern_type %d is not support\n",
				__func__, ext_config->index,
				ext_config->name, ext_config->type);
			break;
		}
	} else {
		switch (ext_config->type) {
		case LCD_EXTERN_I2C:
			while (i <= LCD_EXTERN_INIT_TABLE_MAX) {
				if (init_table[i] == LCD_EXTERN_INIT_END) {
					break;
				} else if (init_table[i] ==
					LCD_EXTERN_INIT_NONE) {
					/* do nothing, only for delay */
				} else if (init_table[i] ==
					LCD_EXTERN_INIT_GPIO) {
					if (init_table[i+1] < LCD_GPIO_MAX) {
						lcd_extern_gpio_set(
							init_table[i+1],
							init_table[i+2]);
					}
				} else if (init_table[i] ==
						LCD_EXTERN_INIT_CMD) {
					ret = lcd_extern_i2c_write(
						aml_default_i2c_client,
						&init_table[i+1], (len-2));
				} else if (init_table[i] ==
						LCD_EXTERN_INIT_CMD2) {
					ret = lcd_extern_i2c_write(
						aml_default_i2c2_client,
						&init_table[i+1], (len-2));
				} else {
					EXTERR("%s(%d: %s): type %d invalid\n",
						__func__, ext_config->index,
						ext_config->name,
						ext_config->type);
				}
				if (init_table[i+len-1] > 0)
					mdelay(init_table[i+len-1]);
				i += len;
			}
			break;
		case LCD_EXTERN_SPI:
			while (i <= LCD_EXTERN_INIT_TABLE_MAX) {
				if (init_table[i] == LCD_EXTERN_INIT_END) {
					break;
				} else if (init_table[i] ==
						LCD_EXTERN_INIT_NONE) {
					/* do nothing, only for delay */
				} else if (init_table[i] ==
						LCD_EXTERN_INIT_GPIO) {
					if (init_table[i+1] < LCD_GPIO_MAX) {
						lcd_extern_gpio_set(
							init_table[i+1],
							init_table[i+2]);
					}
				} else if (init_table[i] ==
						LCD_EXTERN_INIT_CMD) {
					ret = lcd_extern_spi_write(
						&init_table[i+1], (len-1));
				} else {
					EXTERR("%s(%d: %s): type %d invalid\n",
						__func__, ext_config->index,
						ext_config->name,
						ext_config->type);
				}
				if (init_table[i+len-1] > 0)
					mdelay(init_table[i+len-1]);
				i += len;
			}
			break;
		default:
			EXTERR("%s(%d: %s): extern_type %d is not support\n",
				__func__, ext_config->index,
				ext_config->name, ext_config->type);
			break;
		}
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
	ext_drv->reg_read  = lcd_extern_reg_read;
	ext_drv->reg_write = lcd_extern_reg_write;
	ext_drv->power_on  = lcd_extern_power_on;
	ext_drv->power_off = lcd_extern_power_off;

	return 0;
}

static int aml_default_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		EXTERR("%s: functionality check failed\n", __func__);
	else
		aml_default_i2c_client = client;

	EXTPR("%s OK\n", __func__);
	return 0;
}

static int aml_default_i2c_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id aml_default_i2c_id[] = {
	{LCD_EXTERN_NAME, 0},
	{ }
};
/* MODULE_DEVICE_TABLE(i2c, aml_T5800Q_id); */

static struct i2c_driver aml_default_i2c_driver = {
	.probe    = aml_default_i2c_probe,
	.remove   = aml_default_i2c_remove,
	.id_table = aml_default_i2c_id,
	.driver = {
		.name = LCD_EXTERN_NAME,
		.owner = THIS_MODULE,
	},
};

int aml_lcd_extern_default_probe(struct aml_lcd_extern_driver_s *ext_drv)
{
	struct i2c_board_info i2c_info;
	struct i2c_adapter *adapter;
	struct i2c_client *i2c_client;
	int ret = 0;

	ext_config = &ext_drv->config;

	switch (ext_drv->config.type) {
	case LCD_EXTERN_I2C:
		aml_default_i2c_client = NULL;
		aml_default_i2c2_client = NULL;
		memset(&i2c_info, 0, sizeof(i2c_info));
		adapter = i2c_get_adapter(ext_drv->config.i2c_bus);
		if (!adapter) {
			EXTERR("%s failed to get i2c adapter\n",
				ext_drv->config.name);
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
			EXTERR("%s failed to new i2c device\n",
				ext_drv->config.name);
		} else {
			if (lcd_debug_print_flag) {
				EXTPR("%s new i2c device succeed\n",
					ext_drv->config.name);
			}
		}

		if (!aml_default_i2c_client) {
			ret = i2c_add_driver(&aml_default_i2c_driver);
			if (ret) {
				EXTERR("%s add i2c_driver failed\n",
					ext_drv->config.name);
				return -1;
			}
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

