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
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/amlogic/media/vout/lcd/lcd_extern.h>
#include "lcd_extern.h"

#define LCD_EXTERN_NAME           "ext_default"

#define LCD_EXTERN_TYPE           LCD_EXTERN_MAX

static struct lcd_extern_config_s *ext_config;
static struct aml_lcd_extern_i2c_dev_s *i2c0_dev;
static struct aml_lcd_extern_i2c_dev_s *i2c1_dev;


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
	struct aml_lcd_extern_i2c_dev_s *i2c_dev;
	unsigned char tmp;
	int ret = 0;

	tmp = reg;
	switch (ext_config->type) {
	case LCD_EXTERN_I2C:
		if (ext_config->addr_sel)
			i2c_dev = i2c1_dev;
		else
			i2c_dev = i2c0_dev;
		if (i2c_dev == NULL) {
			EXTERR("invalid i2c device\n");
			return -1;
		}
		lcd_extern_i2c_read(i2c_dev->client, &tmp, 1);
		buf[0] = tmp;
		break;
	case LCD_EXTERN_SPI:
		EXTPR("not support\n");
		break;
	default:
		break;
	}

	return ret;
}

static int lcd_extern_reg_write(unsigned char reg, unsigned char value)
{
	struct aml_lcd_extern_i2c_dev_s *i2c_dev;
	unsigned char tmp[2];
	int ret = 0;

	tmp[0] = reg;
	tmp[1] = value;
	switch (ext_config->type) {
	case LCD_EXTERN_I2C:
		if (ext_config->addr_sel)
			i2c_dev = i2c1_dev;
		else
			i2c_dev = i2c0_dev;
		if (i2c_dev == NULL) {
			EXTERR("invalid i2c device\n");
			return -1;
		}
		lcd_extern_i2c_write(i2c_dev->client, tmp, 2);
		break;
	case LCD_EXTERN_SPI:
		lcd_extern_spi_write(tmp, 2);
		break;
	default:
		break;
	}

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
				if (i2c0_dev == NULL) {
					EXTERR("invalid i2c0 device\n");
					return -1;
				}
				ret = lcd_extern_i2c_write(i2c0_dev->client,
					&init_table[i+2], (cmd_size-1));
				if (init_table[i+cmd_size+1] > 0)
					mdelay(init_table[i+cmd_size+1]);
			} else if (type == LCD_EXTERN_INIT_CMD2) {
				if (i2c1_dev == NULL) {
					EXTERR("invalid i2c1 device\n");
					return -1;
				}
				ret = lcd_extern_i2c_write(i2c1_dev->client,
					&init_table[i+2], (cmd_size-1));
				if (init_table[i+cmd_size+1] > 0)
					mdelay(init_table[i+cmd_size+1]);
			} else {
				EXTERR("%s: %s(%d): type %d invalid\n",
					__func__, ext_config->name,
					ext_config->index, ext_config->type);
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
				EXTERR("%s: %s(%d): type %d invalid\n",
					__func__, ext_config->name,
					ext_config->index, ext_config->type);
			}
			i += (cmd_size + 2);
			step++;
		}
		break;
	default:
		EXTERR("%s: %s(%d): extern_type %d is not support\n",
			__func__, ext_config->name,
			ext_config->index, ext_config->type);
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
				if (i2c0_dev == NULL) {
					EXTERR("invalid i2c0 device\n");
					return -1;
				}
				ret = lcd_extern_i2c_write(i2c0_dev->client,
					&init_table[i+1], (cmd_size-2));
			} else if (type == LCD_EXTERN_INIT_CMD2) {
				if (i2c1_dev == NULL) {
					EXTERR("invalid i2c1 device\n");
					return -1;
				}
				ret = lcd_extern_i2c_write(i2c1_dev->client,
					&init_table[i+1], (cmd_size-2));
			} else {
				EXTERR("%s: %s(%d): type %d invalid\n",
					__func__, ext_config->name,
					ext_config->index, ext_config->type);
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
				EXTERR("%s: %s(%d): type %d invalid\n",
					__func__, ext_config->name,
					ext_config->index, ext_config->type);
			}
			if (init_table[i+cmd_size-1] > 0)
				mdelay(init_table[i+cmd_size-1]);
			i += cmd_size;
			step++;
		}
		break;
	default:
		EXTERR("%s: %s(%d): extern_type %d is not support\n",
			__func__, ext_config->name,
			ext_config->index, ext_config->type);
		break;
	}

	return ret;
}

static int lcd_extern_power_ctrl(int flag)
{
	unsigned char *init_table;
	unsigned char cmd_size;
	int ret = 0;

	if (ext_config->type == LCD_EXTERN_SPI)
		spi_gpio_init();

	if (flag)
		init_table = ext_config->table_init_on;
	else
		init_table = ext_config->table_init_off;
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

	if (ext_config->type == LCD_EXTERN_SPI)
		spi_gpio_off();

	EXTPR("%s: %s(%d): %d\n",
		__func__, ext_config->name, ext_config->index, flag);
	return ret;
}

static int lcd_extern_power_on(void)
{
	int ret;

	lcd_extern_pinmux_set(1);
	ret = lcd_extern_power_ctrl(1);
	return ret;
}

static int lcd_extern_power_off(void)
{
	int ret;

	ret = lcd_extern_power_ctrl(0);
	lcd_extern_pinmux_set(0);
	return ret;
}

static int lcd_extern_driver_update(struct aml_lcd_extern_driver_s *ext_drv)
{
	if (ext_drv == NULL) {
		EXTERR("%s driver is null\n", LCD_EXTERN_NAME);
		return -1;
	}

	if (ext_drv->config->type == LCD_EXTERN_SPI) {
		ext_drv->config->spi_delay_us =
			1000000 / ext_drv->config->spi_clk_freq;
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

	ext_config = ext_drv->config;

	switch (ext_config->type) {
	case LCD_EXTERN_I2C:
		if (ext_config->i2c_addr < LCD_EXTERN_I2C_ADDR_INVALID) {
			i2c0_dev = lcd_extern_get_i2c_device(
					ext_config->i2c_addr);
			if (i2c0_dev == NULL) {
				EXTERR("invalid i2c0 device\n");
				return -1;
			}
			EXTPR("get i2c0 device: %s, addr 0x%02x OK\n",
				i2c0_dev->name, i2c0_dev->client->addr);
		}
		if (ext_config->i2c_addr2 < LCD_EXTERN_I2C_ADDR_INVALID) {
			i2c1_dev = lcd_extern_get_i2c_device(
					ext_config->i2c_addr2);
			if (i2c1_dev == NULL) {
				EXTERR("invalid i2c1 device\n");
				i2c0_dev = NULL;
				return -1;
			}
			EXTPR("get i2c1 device: %s, addr 0x%02x OK\n",
				i2c1_dev->name, i2c1_dev->client->addr);
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

int aml_lcd_extern_default_remove(void)
{
	i2c0_dev = NULL;
	i2c1_dev = NULL;
	ext_config = NULL;

	return 0;
}

