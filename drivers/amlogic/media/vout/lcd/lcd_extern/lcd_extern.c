/*
 * drivers/amlogic/media/vout/lcd/lcd_extern/lcd_extern.c
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
#include <linux/amlogic/media/vout/lcd/lcd_unifykey.h>
#include "lcd_extern.h"

static struct device *lcd_extern_dev;
static int lcd_ext_driver_num;
static struct aml_lcd_extern_driver_s *lcd_ext_driver[LCD_EXT_DRIVER_MAX];
static int lcd_extern_add_driver(struct lcd_extern_config_s *extconf);

static unsigned int lcd_ext_key_valid;
static unsigned char lcd_ext_config_load;
static unsigned char lcd_ext_i2c_bus = LCD_EXTERN_I2C_BUS_INVALID;
static unsigned char lcd_ext_i2c_sck_gpio = LCD_EXTERN_GPIO_NUM_MAX;
static unsigned char lcd_ext_i2c_sck_gpio_off = 2;
static unsigned char lcd_ext_i2c_sda_gpio = LCD_EXTERN_GPIO_NUM_MAX;
static unsigned char lcd_ext_i2c_sda_gpio_off = 2;

struct lcd_ext_gpio_s {
	char name[15];
	struct gpio_desc *gpio;
	int probe_flag;
	int register_flag;
};

static struct lcd_ext_gpio_s lcd_extern_gpio[LCD_EXTERN_GPIO_NUM_MAX] = {
	{.probe_flag = 0, .register_flag = 0,},
	{.probe_flag = 0, .register_flag = 0,},
	{.probe_flag = 0, .register_flag = 0,},
	{.probe_flag = 0, .register_flag = 0,},
	{.probe_flag = 0, .register_flag = 0,},
	{.probe_flag = 0, .register_flag = 0,},
};

struct aml_lcd_extern_driver_s *aml_lcd_extern_get_driver(int index)
{
	int i;
	struct aml_lcd_extern_driver_s *ext_driver = NULL;

	if (index >= LCD_EXTERN_INDEX_INVALID) {
		EXTERR("invalid driver index: %d\n", index);
		return NULL;
	}
	for (i = 0; i < lcd_ext_driver_num; i++) {
		if (lcd_ext_driver[i]->config.index == index) {
			ext_driver = lcd_ext_driver[i];
			break;
		}
	}
	if (ext_driver == NULL)
		EXTERR("invalid driver index: %d\n", index);
	return ext_driver;
}

#if 0
static struct aml_lcd_extern_driver_s
	*aml_lcd_extern_get_driver_by_name(char *name)
{
	int i;
	struct aml_lcd_extern_driver_s *ext_driver = NULL;

	for (i = 0; i < lcd_ext_driver_num; i++) {
		if (strcmp(lcd_ext_driver[i]->config.name, name) == 0) {
			ext_driver = lcd_ext_driver[i];
			break;
		}
	}
	if (ext_driver == NULL)
		EXTERR("invalid driver name: %s\n", name);
	return ext_driver;
}
#endif

#ifdef CONFIG_OF
void lcd_extern_gpio_probe(unsigned char index)
{
	struct lcd_ext_gpio_s *ext_gpio;
	const char *str;
	int ret;

	if (index >= LCD_EXTERN_GPIO_NUM_MAX) {
		EXTERR("gpio index %d, exit\n", index);
		return;
	}
	ext_gpio = &lcd_extern_gpio[index];
	if (ext_gpio->probe_flag) {
		if (lcd_debug_print_flag) {
			EXTPR("gpio %s[%d] is already registered\n",
				ext_gpio->name, index);
		}
		return;
	}

	/* get gpio name */
	ret = of_property_read_string_index(lcd_extern_dev->of_node,
		"extern_gpio_names", index, &str);
	if (ret) {
		EXTERR("failed to get extern_gpio_names: %d\n", index);
		str = "unknown";
	}
	strcpy(ext_gpio->name, str);

	/* init gpio flag */
	ext_gpio->probe_flag = 1;
	ext_gpio->register_flag = 0;
}

void lcd_extern_gpio_unregister(int index)
{
	struct lcd_ext_gpio_s *ext_gpio;

	if (index >= LCD_EXTERN_GPIO_NUM_MAX) {
		EXTERR("gpio index %d, exit\n", index);
		return;
	}
	ext_gpio = &lcd_extern_gpio[index];
	if (ext_gpio->probe_flag == 0) {
		if (lcd_debug_print_flag) {
			EXTPR("gpio %s[%d] is already registered\n",
				ext_gpio->name, index);
		}
		return;
	}
	if (ext_gpio->register_flag) {
		EXTPR("%s: gpio %s[%d] is already registered\n",
			__func__, ext_gpio->name, index);
		return;
	}
	if (IS_ERR(ext_gpio->gpio)) {
		EXTERR("register gpio %s[%d]: %p, err: %d\n",
			ext_gpio->name, index, ext_gpio->gpio,
			IS_ERR(ext_gpio->gpio));
		ext_gpio->gpio = NULL;
		return;
	}

	/* release gpio */
	devm_gpiod_put(lcd_extern_dev, ext_gpio->gpio);
	ext_gpio->probe_flag = 0;
	ext_gpio->register_flag = 0;
	if (lcd_debug_print_flag)
		EXTPR("release gpio %s[%d]\n", ext_gpio->name, index);
}

static int lcd_extern_gpio_register(unsigned char index, int init_value)
{
	struct lcd_ext_gpio_s *ext_gpio;
	int value;

	if (index >= LCD_EXTERN_GPIO_NUM_MAX) {
		EXTERR("gpio index %d, exit\n", index);
		return -1;
	}
	ext_gpio = &lcd_extern_gpio[index];
	if (ext_gpio->probe_flag == 0) {
		EXTERR("%s: gpio [%d] is not probed, exit\n", __func__, index);
		return -1;
	}
	if (ext_gpio->register_flag) {
		EXTPR("%s: gpio %s[%d] is already registered\n",
			__func__, ext_gpio->name, index);
		return 0;
	}

	switch (init_value) {
	case LCD_GPIO_OUTPUT_LOW:
		value = GPIOD_OUT_LOW;
		break;
	case LCD_GPIO_OUTPUT_HIGH:
		value = GPIOD_OUT_HIGH;
		break;
	case LCD_GPIO_INPUT:
	default:
		value = GPIOD_IN;
		break;
	}

	/* request gpio */
	ext_gpio->gpio = devm_gpiod_get_index(
		lcd_extern_dev, "extern", index, value);
	if (IS_ERR(ext_gpio->gpio)) {
		EXTERR("register gpio %s[%d]: %p, err: %d\n",
			ext_gpio->name, index, ext_gpio->gpio,
			IS_ERR(ext_gpio->gpio));
		ext_gpio->gpio = NULL;
		return -1;
	}
	ext_gpio->register_flag = 1;
	if (lcd_debug_print_flag) {
		EXTPR("register gpio %s[%d]: %p, init value: %d\n",
			ext_gpio->name, index, ext_gpio->gpio, init_value);
	}

	return 0;
}
#endif

void lcd_extern_gpio_set(unsigned char index, int value)
{
	struct lcd_ext_gpio_s *ext_gpio;

	if (index >= LCD_EXTERN_GPIO_NUM_MAX) {
		EXTERR("gpio index %d, exit\n", index);
		return;
	}
	ext_gpio = &lcd_extern_gpio[index];
	if (ext_gpio->probe_flag == 0) {
		EXTERR("%s: gpio [%d] is not probed, exit\n", __func__, index);
		return;
	}
	if (ext_gpio->register_flag == 0) {
		lcd_extern_gpio_register(index, value);
		return;
	}

	if (IS_ERR_OR_NULL(ext_gpio->gpio)) {
		EXTERR("gpio %s[%d]: %p, err: %ld\n",
			ext_gpio->name, index, ext_gpio->gpio,
			PTR_ERR(ext_gpio->gpio));
		return;
	}

	switch (value) {
	case LCD_GPIO_OUTPUT_LOW:
	case LCD_GPIO_OUTPUT_HIGH:
		gpiod_direction_output(ext_gpio->gpio, value);
		break;
	case LCD_GPIO_INPUT:
	default:
		gpiod_direction_input(ext_gpio->gpio);
		break;
	}
	if (lcd_debug_print_flag) {
		EXTPR("set gpio %s[%d] value: %d\n",
		ext_gpio->name, index, value);
	}
}

unsigned int lcd_extern_gpio_get(unsigned char index)
{
	struct lcd_ext_gpio_s *ext_gpio;

	if (index >= LCD_EXTERN_GPIO_NUM_MAX) {
		EXTERR("gpio index %d, exit\n", index);
		return -1;
	}
	ext_gpio = &lcd_extern_gpio[index];
	if (ext_gpio->probe_flag == 0) {
		EXTERR("%s: gpio [%d] is not probed, exit\n", __func__, index);
		return -1;
	}
	if (ext_gpio->register_flag == 0) {
		EXTERR("%s: gpio %s[%d] is not registered\n",
			__func__, ext_gpio->name, index);
		return -1;
	}
	if (IS_ERR_OR_NULL(ext_gpio->gpio)) {
		EXTERR("gpio %s[%d]: %p, err: %ld\n",
			ext_gpio->name, index, ext_gpio->gpio,
			PTR_ERR(ext_gpio->gpio));
		return -1;
	}

	return gpiod_get_value(ext_gpio->gpio);
}

void lcd_extern_pinmux_set(struct aml_lcd_extern_driver_s *ext_drv, int status)
{
	struct lcd_ext_gpio_s *ext_gpio_sck;
	struct lcd_ext_gpio_s *ext_gpio_sda;

	ext_gpio_sck = &lcd_extern_gpio[ext_drv->config.i2c_sck_gpio];
	ext_gpio_sda = &lcd_extern_gpio[ext_drv->config.i2c_sda_gpio];

	if (lcd_debug_print_flag)
		EXTPR("%s: %d\n", __func__, status);

	if (status) {
		/* release gpio */
		if (ext_drv->pinmux_flag) {
			EXTPR("drv_index %d pinmux is already selected\n",
				ext_drv->config.index);
			return;
		}
		if (ext_drv->config.type == LCD_EXTERN_I2C) {
			if (ext_drv->config.i2c_sck_gpio <
				LCD_EXTERN_GPIO_NUM_MAX)
				lcd_extern_gpio_unregister(
					ext_drv->config.i2c_sck_gpio);
			if (ext_drv->config.i2c_sda_gpio <
				LCD_EXTERN_GPIO_NUM_MAX)
				lcd_extern_gpio_unregister(
					ext_drv->config.i2c_sda_gpio);
		}
		/* request pinmux */
		ext_drv->pin = devm_pinctrl_get_select(lcd_extern_dev,
			"extern_pins");
		if (IS_ERR(ext_drv->pin)) {
			EXTERR("set drv_index %d pinmux error\n",
				ext_drv->config.index);
		} else {
			if (lcd_debug_print_flag) {
				EXTPR("set drv_index %d pinmux ok\n",
					ext_drv->config.index);
			}
		}
		ext_drv->pinmux_flag = 1;
	} else {
		if (ext_drv->pinmux_flag) {
			if (lcd_debug_print_flag)
				EXTPR("release pinmux: %p\n", ext_drv->pin);
			 /* release pinmux */
			if (!IS_ERR(ext_drv->pin))
				devm_pinctrl_put(ext_drv->pin);
			ext_drv->pinmux_flag = 0;
		}
		/* request gpio &  set gpio */
		if (ext_drv->config.type == LCD_EXTERN_I2C) {
			if (ext_drv->config.i2c_sck_gpio <
					LCD_EXTERN_GPIO_NUM_MAX) {
				lcd_extern_gpio_set(
					ext_drv->config.i2c_sck_gpio,
					ext_drv->config.i2c_sck_gpio_off);
			}
			if (ext_drv->config.i2c_sda_gpio <
					LCD_EXTERN_GPIO_NUM_MAX) {
				lcd_extern_gpio_set(
					ext_drv->config.i2c_sda_gpio,
					ext_drv->config.i2c_sda_gpio_off);
			}
		}
	}
}

#ifdef CONFIG_OF
static unsigned char lcd_extern_get_i2c_bus_str(const char *str)
{
	unsigned char i2c_bus;

	if (strncmp(str, "i2c_bus_ao", 10) == 0)
		i2c_bus = AML_I2C_MASTER_AO;
	else if (strncmp(str, "i2c_bus_a", 9) == 0)
		i2c_bus = AML_I2C_MASTER_A;
	else if (strncmp(str, "i2c_bus_b", 9) == 0)
		i2c_bus = AML_I2C_MASTER_B;
	else if (strncmp(str, "i2c_bus_c", 9) == 0)
		i2c_bus = AML_I2C_MASTER_C;
	else if (strncmp(str, "i2c_bus_d", 9) == 0)
		i2c_bus = AML_I2C_MASTER_D;
	else {
		i2c_bus = LCD_EXTERN_I2C_BUS_INVALID;
		EXTERR("invalid i2c_bus: %s\n", str);
	}

	return i2c_bus;
}

struct device_node *aml_lcd_extern_get_dts_child(int index)
{
	char propname[30];
	struct device_node *child;

	sprintf(propname, "extern_%d", index);
	child = of_get_child_by_name(lcd_extern_dev->of_node, propname);
	return child;
}

static int lcd_extern_init_table_dynamic_size_load_dts(
		struct device_node *of_node,
		struct lcd_extern_config_s *extconf, int flag)
{
	unsigned char cmd_size, index, type;
	int i, j, val, max_len, step = 0, ret = 0;
	unsigned char *init_table;
	char propname[20];

	if (flag) {
		init_table = extconf->table_init_on;
		max_len = LCD_EXTERN_INIT_ON_MAX;
		sprintf(propname, "init_on");
	} else {
		init_table = extconf->table_init_off;
		max_len = LCD_EXTERN_INIT_OFF_MAX;
		sprintf(propname, "init_off");
	}

	switch (extconf->type) {
	case LCD_EXTERN_I2C:
	case LCD_EXTERN_SPI:
		i = 0;
		while ((i + 1) < max_len) { /* type & cmd_size detect */
			/* step1: type */
			ret = of_property_read_u32_index(
				of_node, propname, i, &val);
			if (ret) {
				EXTERR("get %s %s type failed, step %d\n",
					extconf->name, propname, step);
				init_table[i] = LCD_EXTERN_INIT_END;
				return -1;
			}
			init_table[i] = (unsigned char)val;
			type = init_table[i];
			if (type == LCD_EXTERN_INIT_END)
				break;
			/* step2: cmd_size */
			ret = of_property_read_u32_index(
				of_node, propname, (i+1), &val);
			if (ret) {
				EXTERR("get %s %s cmd_size failed, step %d\n",
					extconf->name, propname, step);
				init_table[i] = LCD_EXTERN_INIT_END;
				return -1;
			}
			init_table[i+1] = (unsigned char)val;
			cmd_size = init_table[i+1];
			if (cmd_size == 0) {
				i += 2;
				continue;
			}
			if ((i + 2 + cmd_size) > max_len) {
				EXTERR("%s %s cmd_size out of max, step %d\n",
					extconf->name, propname, step);
				init_table[i] = LCD_EXTERN_INIT_END;
				init_table[i+1] = 0;
				break;
			}
			/* step3: data */
			for (j = 0; j < cmd_size; j++) {
				ret = of_property_read_u32_index(
					of_node, propname, (i+2+j), &val);
				if (ret) {
					EXTERR("get %s%s data failed,step%d\n",
						extconf->name, propname, step);
					init_table[i] = LCD_EXTERN_INIT_END;
					return -1;
				}
				init_table[i+2+j] = (unsigned char)val;
			}
			if (type == LCD_EXTERN_INIT_GPIO) {
				/* gpio probe */
				index = init_table[i+2];
				if (index < LCD_EXTERN_GPIO_NUM_MAX)
					lcd_extern_gpio_probe(index);
			}
			step++;
			i += (cmd_size + 2);
		}
		break;
	case LCD_EXTERN_MIPI:
		i = 0;
		while ((i + 1) < max_len) { /* type & cmd_size detect */
			ret = of_property_read_u32_index(
				of_node, propname, i, &val);
			if (ret) {
				EXTERR("get %s %s type failed\n",
					extconf->name, propname);
				init_table[i] = 0xff;
				init_table[i+1] = 0xff;
				return -1;
			}
			init_table[i] = (unsigned char)val;
			type = (unsigned char)val;
			if (type == 0xff) {
				ret = of_property_read_u32_index(
					of_node, propname, (i+1), &val);
				if (ret) {
					EXTERR("get %s %s cmd_size failed\n",
						extconf->name, propname);
					init_table[i] = 0xff;
					init_table[i+1] = 0xff;
					return -1;
				}
				init_table[i+1] = (unsigned char)val;
				cmd_size = init_table[i+1];
				if (cmd_size == 0xff)
					break;
				i += 2;
			} else if (type == 0xf0) {
				ret = of_property_read_u32_index(
					of_node, propname, (i+1), &val);
				if (ret) {
					EXTERR("get %s %s type failed\n",
						extconf->name, propname);
					init_table[i+1] = 0xff;
					return -1;
				}
				/* gpio probe */
				init_table[i+1] = val;
				cmd_size = val;
				if (cmd_size < 3) {
					EXTERR("%s %s invalid cmd_size gpio\n",
						extconf->name, propname);
					return -1;
				}
				if ((i + 2 + cmd_size) >= max_len) {
					EXTERR("%s %s cmd_size out of max\n",
						extconf->name, propname);
					init_table[i] = 0xff;
					init_table[i+1] = 0xff;
					return -1;
				}
				for (j = 0; j < cmd_size; j++) {
					ret = of_property_read_u32_index(
						of_node, propname,
						(i+2+j), &val);
					if (ret) {
						EXTERR("get %s %s failed\n",
							extconf->name,
							propname);
						init_table[i] = 0xff;
						init_table[i+1] = 0xff;
						return -1;
					}
					init_table[i+2+j] = (unsigned char)val;
				}
				index = init_table[i+2];
				if (index < LCD_EXTERN_GPIO_NUM_MAX)
					lcd_extern_gpio_probe(index);
				i += (cmd_size + 2);
			} else {
				ret = of_property_read_u32_index(
					of_node, propname, (i+1), &val);
				init_table[i+1] = val;
				cmd_size = val;
				if (cmd_size == 0) {
					i += 2;
					continue;
				}
				if ((i + 2 + cmd_size) >= max_len) {
					EXTERR("%s %s cmd_size out of max\n",
						extconf->name, propname);
					init_table[i] = 0xff;
					init_table[i+1] = 0xff;
					return -1;
				}
				for (j = 0; j < cmd_size; j++) {
					ret = of_property_read_u32_index(
						of_node, propname,
						(i+2+j), &val);
					if (ret) {
						EXTERR("%s %s failed\n",
							extconf->name,
							propname);
						init_table[i] = 0xff;
						init_table[i+1] = 0xff;
						return -1;
					}
					init_table[i+2+j] = (unsigned char)val;
				}
				i += (cmd_size + 2);
			}
		}
		break;
	default:
		break;
	}

	return 0;
}

static int lcd_extern_init_table_fixed_size_load_dts(
		struct device_node *of_node,
		struct lcd_extern_config_s *extconf, int flag)
{
	unsigned char cmd_size, index;
	int i, j, val, max_len, step = 0, ret = 0;
	unsigned char *init_table;
	char propname[20];

	cmd_size = extconf->cmd_size;
	if (flag) {
		init_table = extconf->table_init_on;
		max_len = LCD_EXTERN_INIT_ON_MAX;
		sprintf(propname, "init_on");
	} else {
		init_table = extconf->table_init_off;
		max_len = LCD_EXTERN_INIT_OFF_MAX;
		sprintf(propname, "init_off");
	}

	i = 0;
	while (i < max_len) { /* group detect */
		if ((i + cmd_size) >= max_len) {
			EXTERR("%s %s cmd_size out of max\n",
				extconf->name, propname);
			init_table[i] = LCD_EXTERN_INIT_END;
			break;
		}
		for (j = 0; j < cmd_size; j++) {
			ret = of_property_read_u32_index(
				of_node, propname, (i+j), &val);
			if (ret) {
				EXTERR("get %s %s failed, step %d\n",
					extconf->name, propname, step);
				init_table[i] = LCD_EXTERN_INIT_END;
				return -1;
			}
			init_table[i+j] = (unsigned char)val;
		}
		if (init_table[i] == LCD_EXTERN_INIT_END) {
			break;
		} else if (init_table[i] == LCD_EXTERN_INIT_GPIO) {
			/* gpio probe */
			index = init_table[i+1];
			if (index < LCD_EXTERN_GPIO_NUM_MAX)
				lcd_extern_gpio_probe(index);
		}
		step++;
		i += cmd_size;
	}

	return 0;
}

static int lcd_extern_get_config_dts(struct device_node *of_node,
		struct lcd_extern_config_s *extconf)
{
	int ret;
	int val;
	const char *str;

	extconf->index = LCD_EXTERN_INDEX_INVALID;
	extconf->type = LCD_EXTERN_MAX;
	extconf->status = 0;
	extconf->table_init_loaded = 0;

	ret = of_property_read_u32(of_node, "index", &val);
	if (ret) {
		EXTERR("get index failed, exit\n");
		return -1;
	}
	extconf->index = (unsigned char)val;
	if (extconf->index == LCD_EXTERN_INDEX_INVALID) {
		if (lcd_debug_print_flag)
			EXTPR("index %d is invalid\n", extconf->index);
		return -1;
	}

	ret = of_property_read_string(of_node, "status", &str);
	if (ret) {
		EXTERR("get index %d status failed\n", extconf->index);
		return -1;
	}
	if (lcd_debug_print_flag)
		EXTPR("index %d status = %s\n", extconf->index, str);
	if (strncmp(str, "okay", 2) == 0)
		extconf->status = 1;
	else
		return -1;

	ret = of_property_read_string(of_node, "extern_name", &str);
	if (ret) {
		str = "none";
		EXTERR("get extern_name failed\n");
	}
	strncpy(extconf->name, str, LCD_EXTERN_NAME_LEN_MAX);
	/* ensure string ending */
	extconf->name[LCD_EXTERN_NAME_LEN_MAX-1] = '\0';
	EXTPR("load config: %s[%d]\n", extconf->name, extconf->index);

	ret = of_property_read_u32(of_node, "type", &extconf->type);
	if (ret) {
		extconf->type = LCD_EXTERN_MAX;
		EXTERR("get type failed, exit\n");
		return -1;
	}

	switch (extconf->type) {
	case LCD_EXTERN_I2C:
		if (lcd_ext_i2c_bus == LCD_EXTERN_I2C_BUS_INVALID) {
			ret = of_property_read_string(of_node, "i2c_bus", &str);
			if (ret) {
				EXTERR("get %s i2c_bus failed, exit\n",
					extconf->name);
				extconf->i2c_bus = LCD_EXTERN_I2C_BUS_INVALID;
				return -1;
			}
			extconf->i2c_bus = lcd_extern_get_i2c_bus_str(str);
		} else
			extconf->i2c_bus = lcd_ext_i2c_bus;
		if (lcd_debug_print_flag) {
			EXTPR("%s i2c_bus=%d\n",
				extconf->name, extconf->i2c_bus);
		}
		ret = of_property_read_u32(of_node, "i2c_address", &val);
		if (ret) {
			EXTERR("get %s i2c_address failed, exit\n",
				extconf->name);
			extconf->i2c_addr = 0xff;
			return -1;
		}
		extconf->i2c_addr = (unsigned char)val;
		if (lcd_debug_print_flag) {
			EXTPR("%s i2c_address=0x%02x\n",
				extconf->name, extconf->i2c_addr);
		}
		ret = of_property_read_u32(of_node, "i2c_second_address", &val);
		if (ret) {
			EXTPR("get %s i2c_second_address failed\n",
				extconf->name);
			extconf->i2c_addr2 = 0xff;
		} else {
			extconf->i2c_addr2 = (unsigned char)val;
		}
		if (lcd_debug_print_flag) {
			EXTPR("%s i2c_second_address=0x%02x\n",
				extconf->name, extconf->i2c_addr2);
		}

		extconf->i2c_sck_gpio = lcd_ext_i2c_sck_gpio;
		extconf->i2c_sck_gpio_off = lcd_ext_i2c_sck_gpio_off;
		extconf->i2c_sda_gpio = lcd_ext_i2c_sda_gpio;
		extconf->i2c_sda_gpio_off = lcd_ext_i2c_sda_gpio_off;
		if ((lcd_ext_i2c_sck_gpio < LCD_EXTERN_GPIO_NUM_MAX) ||
			(lcd_ext_i2c_sda_gpio < LCD_EXTERN_GPIO_NUM_MAX)) {
			EXTPR("find i2c_gpio_off config\n");
			lcd_extern_gpio_probe(extconf->i2c_sck_gpio);
			lcd_extern_gpio_probe(extconf->i2c_sda_gpio);
		}

		ret = of_property_read_u32(of_node, "cmd_size", &val);
		if (ret) {
			if (extconf->index == 0) {
				EXTERR("get %s cmd_size failed\n",
					extconf->name);
			} else {
				EXTPR("%s no cmd_size\n", extconf->name);
			}
			extconf->cmd_size = 0;
		} else {
			extconf->cmd_size = (unsigned char)val;
		}
		if (lcd_debug_print_flag) {
			EXTPR("%s cmd_size=%d\n",
				extconf->name, extconf->cmd_size);
		}
		if (extconf->cmd_size <= 1) {
			if (extconf->index == 0) {
				EXTERR("cmd_size %d is invalid\n",
					extconf->cmd_size);
			}
			break;
		}

		if (extconf->cmd_size == LCD_EXTERN_CMD_SIZE_DYNAMIC) {
			ret = lcd_extern_init_table_dynamic_size_load_dts(
				of_node, extconf, 1);
			if (ret)
				break;
			ret = lcd_extern_init_table_dynamic_size_load_dts(
				of_node, extconf, 0);
		} else {
			ret = lcd_extern_init_table_fixed_size_load_dts(
				of_node, extconf, 1);
			if (ret)
				break;
			ret = lcd_extern_init_table_fixed_size_load_dts(
				of_node, extconf, 0);
		}
		if (ret == 0)
			extconf->table_init_loaded = 1;
		break;
	case LCD_EXTERN_SPI:
		ret = of_property_read_u32(of_node, "gpio_spi_cs", &val);
		if (ret) {
			EXTERR("get %s gpio_spi_cs failed, exit\n",
				extconf->name);
			extconf->spi_gpio_cs = LCD_EXTERN_GPIO_NUM_MAX;
			return -1;
		}
		extconf->spi_gpio_cs = val;
		lcd_extern_gpio_probe(val);
		if (lcd_debug_print_flag)
			EXTPR("spi_gpio_cs: %d\n", extconf->spi_gpio_cs);
		ret = of_property_read_u32(of_node, "gpio_spi_clk", &val);
		if (ret) {
			EXTERR("get %s gpio_spi_clk failed, exit\n",
				extconf->name);
			extconf->spi_gpio_clk = LCD_EXTERN_GPIO_NUM_MAX;
			return -1;
		}
		extconf->spi_gpio_clk = val;
		lcd_extern_gpio_probe(val);
		if (lcd_debug_print_flag)
			EXTPR("spi_gpio_clk: %d\n", extconf->spi_gpio_clk);
		ret = of_property_read_u32(of_node, "gpio_spi_data", &val);
		if (ret) {
			EXTERR("get %s gpio_spi_data failed, exit\n",
				extconf->name);
			extconf->spi_gpio_data = LCD_EXTERN_GPIO_NUM_MAX;
			return -1;
		}
		extconf->spi_gpio_data = val;
		lcd_extern_gpio_probe(val);
		if (lcd_debug_print_flag)
			EXTPR("spi_gpio_data: %d\n", extconf->spi_gpio_data);
		ret = of_property_read_u32(of_node, "spi_clk_freq", &val);
		if (ret) {
			EXTERR("get %s spi_clk_freq failed, default to %dHz\n",
				extconf->name, LCD_EXTERN_SPI_CLK_FREQ_DFT);
			extconf->spi_clk_freq = LCD_EXTERN_SPI_CLK_FREQ_DFT;
		} else {
			extconf->spi_clk_freq = val;
			if (lcd_debug_print_flag) {
				EXTPR("spi_clk_freq: %dHz\n",
					extconf->spi_clk_freq);
			}
		}
		ret = of_property_read_u32(of_node, "spi_clk_pol", &val);
		if (ret) {
			EXTERR("get %s spi_clk_pol failed, default to 1\n",
				extconf->name);
			extconf->spi_clk_pol = 1;
		} else {
			extconf->spi_clk_pol = (unsigned char)val;
			if (lcd_debug_print_flag) {
				EXTPR("spi_clk_pol: %dHz\n",
					extconf->spi_clk_pol);
			}
		}
		ret = of_property_read_u32(of_node, "cmd_size", &val);
		if (ret) {
			if (extconf->index == 0) {
				EXTERR("get %s cmd_size failed\n",
					extconf->name);
			} else {
				EXTPR("%s no cmd_size\n", extconf->name);
			}
			extconf->cmd_size = 0;
		} else {
			extconf->cmd_size = (unsigned char)val;
		}
		if (lcd_debug_print_flag) {
			EXTPR("%s cmd_size=%d\n",
				extconf->name, extconf->cmd_size);
		}
		if (extconf->cmd_size <= 1) {
			if (extconf->index == 0) {
				EXTERR("cmd_size %d is invalid\n",
					extconf->cmd_size);
			}
			break;
		}

		if (extconf->cmd_size == LCD_EXTERN_CMD_SIZE_DYNAMIC) {
			ret = lcd_extern_init_table_dynamic_size_load_dts(
				of_node, extconf, 1);
			if (ret)
				break;
			ret = lcd_extern_init_table_dynamic_size_load_dts(
				of_node, extconf, 0);
		} else {
			ret = lcd_extern_init_table_fixed_size_load_dts(
				of_node, extconf, 1);
			if (ret)
				break;
			ret = lcd_extern_init_table_fixed_size_load_dts(
				of_node, extconf, 0);
		}
		if (ret == 0)
			extconf->table_init_loaded = 1;
		break;
	case LCD_EXTERN_MIPI:
		ret = of_property_read_u32(of_node, "cmd_size", &val);
		if (ret) {
			EXTPR("%s no cmd_size\n", extconf->name);
			extconf->cmd_size = 0;
		} else {
			extconf->cmd_size = (unsigned char)val;
		}
		if (lcd_debug_print_flag) {
			EXTPR("%s(%d) cmd_size=%d\n",
				extconf->name, extconf->index,
				extconf->cmd_size);
		}
		if (extconf->cmd_size != LCD_EXTERN_CMD_SIZE_DYNAMIC) {
			EXTERR("cmd_size %d is invalid\n",
				extconf->cmd_size);
			break;
		}
		ret = lcd_extern_init_table_dynamic_size_load_dts(
			of_node, extconf, 1);
		if (ret)
			break;
		ret = lcd_extern_init_table_dynamic_size_load_dts(
			of_node, extconf, 0);
		if (ret == 0)
			extconf->table_init_loaded = 1;
		break;
	default:
		break;
	}

	return 0;
}
#endif


static unsigned char lcd_extern_get_i2c_bus_unifykey(unsigned char val)
{
	if (lcd_ext_i2c_bus == LCD_EXTERN_I2C_BUS_INVALID)
		EXTERR("get i2c_bus failed\n");
	if (lcd_debug_print_flag)
		EXTPR("i2c_bus=%d\n", lcd_ext_i2c_bus);

	return lcd_ext_i2c_bus;
}

static int lcd_extern_init_table_dynamic_size_load_unifykey(
		struct lcd_extern_config_s *extconf, unsigned char *p,
		int key_len, int len, int flag)
{
	unsigned char cmd_size = 0;
	unsigned char index;
	int i, j, max_len, ret = 0;
	unsigned char *init_table, *buf;
	char propname[20];

	if (flag) {
		init_table = extconf->table_init_on;
		max_len = LCD_EXTERN_INIT_ON_MAX;
		sprintf(propname, "init_on");
		buf = p;
	} else {
		init_table = extconf->table_init_off;
		max_len = LCD_EXTERN_INIT_OFF_MAX;
		sprintf(propname, "init_off");
		buf = p + extconf->table_init_on_cnt;
	}
	switch (extconf->type) {
	case LCD_EXTERN_I2C:
	case LCD_EXTERN_SPI:
		i = 0;
		while ((i + 1) < max_len) { /* type & cmd_size detect */
			/* step1: type */
			len += 1;
			ret = lcd_unifykey_len_check(key_len, len);
			if (ret) {
				EXTERR("get %s %s failed\n",
					extconf->name, propname);
				init_table[i] = LCD_EXTERN_INIT_END;
				return -1;
			}
			init_table[i] = *(buf + LCD_UKEY_EXT_INIT + i);
			if (init_table[i] == LCD_EXTERN_INIT_END)
				break;

			/* step2: cmd_size */
			len += 1;
			ret = lcd_unifykey_len_check(key_len, len);
			if (ret) {
				EXTERR("get %s %s failed\n",
					extconf->name, propname);
				init_table[i] = LCD_EXTERN_INIT_END;
				init_table[i+1] = 0;
				return -1;
			}
			init_table[i+1] = *(buf + LCD_UKEY_EXT_INIT + i + 1);
			cmd_size = init_table[i+1];
			if (cmd_size == 0) {
				i += 2;
				continue;
			}
			if ((i + 2 + cmd_size) > max_len) {
				EXTERR("%s %s cmd_size out of max\n",
					extconf->name, propname);
				init_table[i] = LCD_EXTERN_INIT_END;
				init_table[i+1] = 0;
				return -1;
			}

			/* step3: data */
			len += cmd_size;
			ret = lcd_unifykey_len_check(key_len, len);
			if (ret) {
				EXTERR("get %s %s failed\n",
					extconf->name, propname);
				init_table[i] = LCD_EXTERN_INIT_END;
				for (j = 0; j < cmd_size; j++)
					init_table[i+2+j] = 0x0;
				return -1;
			}
			for (j = 0; j < cmd_size; j++) {
				init_table[i+2+j] =
					*(buf + LCD_UKEY_EXT_INIT + i + 2 + j);
			}
			if (init_table[i] == LCD_EXTERN_INIT_END) {
				break;
			} else if (init_table[i] == LCD_EXTERN_INIT_GPIO) {
				/* gpio probe */
				index = init_table[i+1];
				if (index < LCD_EXTERN_GPIO_NUM_MAX)
					lcd_extern_gpio_probe(index);
			}
			i += (cmd_size + 2);
		}
		if (flag)
			extconf->table_init_on_cnt = i + 2;
		break;
	case LCD_EXTERN_MIPI:
		i = 0;
		while ((i + 1) < max_len) { /* type & cmd_size detect */
			/* step1: type */
			len += 1;
			ret = lcd_unifykey_len_check(key_len, len);
			if (ret) {
				EXTERR("get %s %s failed\n",
					extconf->name, propname);
				init_table[i] = 0xff;
				init_table[i+1] = 0xff;
				return -1;
			}
			init_table[i] = *(buf + LCD_UKEY_EXT_INIT + i);
			if (init_table[i] == 0xff) {
				len += 1;
				ret = lcd_unifykey_len_check(key_len, len);
				if (ret) {
					EXTERR("get %s %s failed\n",
						extconf->name, propname);
					init_table[i+1] = 0xff;
					init_table[i+1] = 0xff;
					return -1;
				}
				init_table[i+1] =
					*(buf + LCD_UKEY_EXT_INIT + i + 1);
				cmd_size = init_table[i+1];
				if (cmd_size == 0xff)
					break;
				i += 2;
			} else if (init_table[i] == 0xf0) {
				len += 1;
				ret = lcd_unifykey_len_check(key_len, len);
				if (ret) {
					EXTERR("get %s %s failed\n",
						extconf->name, propname);
					init_table[i+1] = 0xff;
					init_table[i+1] = 0xff;
					return -1;
				}
				init_table[i+1] =
					*(buf + LCD_UKEY_EXT_INIT + i + 1);
				/* gpio probe */
				cmd_size = *(buf + LCD_UKEY_EXT_INIT + i + 1);
				if (cmd_size < 3) {
					EXTERR("%s %s wrong cmd_size %d gpio\n",
						extconf->name, propname,
						cmd_size);
					return -1;
				}
				if ((i + 2 + cmd_size) >= max_len) {
					EXTERR("%s %s cmd_size out of max\n",
						extconf->name, propname);
					init_table[i] = 0xff;
					init_table[i+1] = 0xff;
					return -1;
				}
				len += cmd_size;
				ret = lcd_unifykey_len_check(key_len, len);
				if (ret) {
					EXTERR("get %s %s failed\n",
						extconf->name, propname);
					init_table[i] = LCD_EXTERN_INIT_END;
					for (j = 0; j < cmd_size; j++)
						init_table[i+2+j] = 0x0;
					return -1;
				}
				for (j = 0; j < cmd_size; j++) {
					init_table[i+2+j] = *(buf +
						LCD_UKEY_EXT_INIT + i + 2 + j);
				}
				index = init_table[i+2];
				if (index < LCD_EXTERN_GPIO_NUM_MAX)
					lcd_extern_gpio_probe(index);
				i += (cmd_size + 2);
			} else {
				len += 1;
				ret = lcd_unifykey_len_check(key_len, len);
				if (ret) {
					EXTERR("get %s %s failed\n",
						extconf->name, propname);
					init_table[i+1] = 0xff;
					return -1;
				}
				init_table[i+1] = *(buf + LCD_UKEY_EXT_INIT
					+ i + 1);
				cmd_size = *(buf + LCD_UKEY_EXT_INIT + i + 1);
				if (cmd_size == 0) {
					i += 2;
					continue;
				}
				if ((i + 2 + cmd_size) >= max_len) {
					EXTERR("%s %s cmd_size out of max\n",
						extconf->name, propname);
					init_table[i] = 0xff;
					init_table[i+1] = 0xff;
					return -1;
				}
				len += cmd_size;
				ret = lcd_unifykey_len_check(key_len, len);
				if (ret) {
					EXTERR("get %s %s failed\n",
						extconf->name, propname);
					init_table[i] = LCD_EXTERN_INIT_END;
					for (j = 0; j < cmd_size; j++)
						init_table[i+2+j] = 0xff;
					return -1;
				}
				for (j = 0; j < cmd_size; j++) {
					init_table[i+2+j] = *(buf +
						LCD_UKEY_EXT_INIT + i + 2 + j);
				}
				i += (cmd_size + 2);
			}
		}
		if (flag)
			extconf->table_init_on_cnt = i + 2;
		break;
	default:
		break;
	}

	return 0;
}

static int lcd_extern_init_table_fixed_size_load_unifykey(
		struct lcd_extern_config_s *extconf, unsigned char *p,
		int key_len, int len, int flag)
{
	unsigned char cmd_size;
	unsigned char index;
	int i, j, max_len, ret = 0;
	unsigned char *init_table, *buf;
	char propname[20];

	cmd_size = extconf->cmd_size;
	if (flag) {
		init_table = extconf->table_init_on;
		max_len = LCD_EXTERN_INIT_ON_MAX;
		sprintf(propname, "init_on");
		buf = p;
	} else {
		init_table = extconf->table_init_off;
		max_len = LCD_EXTERN_INIT_OFF_MAX;
		sprintf(propname, "init_off");
		buf = p + extconf->table_init_on_cnt;
	}

	i = 0;
	while (i < max_len) {
		if ((i + cmd_size) >= max_len) {
			EXTERR("%s %s cmd_size out of max\n",
				extconf->name, propname);
			init_table[i] = LCD_EXTERN_INIT_END;
			return -1;
		}
		len += cmd_size;
		ret = lcd_unifykey_len_check(key_len, len);
		if (ret) {
			EXTERR("get %s %s failed\n",
				extconf->name, propname);
			init_table[i] = LCD_EXTERN_INIT_END;
			return -1;
		}
		for (j = 0; j < cmd_size; j++)
			init_table[i+j] = *(buf +
				LCD_UKEY_EXT_INIT + i + j);
		if (init_table[i] == LCD_EXTERN_INIT_END) {
			break;
		} else if (init_table[i] == LCD_EXTERN_INIT_GPIO) {
			/* gpio probe */
			index = init_table[i+1];
			if (index < LCD_EXTERN_GPIO_NUM_MAX)
				lcd_extern_gpio_probe(index);
		}
		i += cmd_size;
	}
	if (flag)
		extconf->table_init_on_cnt = i + cmd_size;

	return 0;
}

static int lcd_extern_get_config_unifykey(struct lcd_extern_config_s *extconf)
{
	unsigned char *para, *p;
	int key_len, len;
	const char *str;
	struct aml_lcd_unifykey_header_s ext_header;
	int ret;

	extconf->index = LCD_EXTERN_INDEX_INVALID;
	extconf->type = LCD_EXTERN_MAX;
	extconf->table_init_loaded = 0;

	para = kmalloc((sizeof(unsigned char) * LCD_UKEY_LCD_EXT_SIZE),
		GFP_KERNEL);
	if (!para) {
		EXTERR("%s: Not enough memory\n", __func__);
		return -1;
	}
	key_len = LCD_UKEY_LCD_EXT_SIZE;
	memset(para, 0, (sizeof(unsigned char) * key_len));
	ret = lcd_unifykey_get("lcd_extern", para, &key_len);
	if (ret) {
		kfree(para);
		return -1;
	}

	/* check lcd_extern unifykey length */
	len = 10 + 33 + 10;
	ret = lcd_unifykey_len_check(key_len, len);
	if (ret) {
		EXTERR("unifykey length is not correct\n");
		kfree(para);
		return -1;
	}

	/* header: 10byte */
	lcd_unifykey_header_check(para, &ext_header);
	if (lcd_debug_print_flag) {
		EXTPR("unifykey header:\n");
		EXTPR("crc32             = 0x%08x\n", ext_header.crc32);
		EXTPR("data_len          = %d\n", ext_header.data_len);
		EXTPR("version           = 0x%04x\n", ext_header.version);
		EXTPR("reserved          = 0x%04x\n", ext_header.reserved);
	}

	/* basic: 33byte */
	p = para;
	str = (const char *)(p + LCD_UKEY_HEAD_SIZE);
	strncpy(extconf->name, str, LCD_EXTERN_NAME_LEN_MAX);
	/* ensure string ending */
	extconf->name[LCD_EXTERN_NAME_LEN_MAX-1] = '\0';
	extconf->index = *(p + LCD_UKEY_EXT_INDEX);
	extconf->type = *(p + LCD_UKEY_EXT_TYPE);
	extconf->status = *(p + LCD_UKEY_EXT_STATUS);

	if (extconf->index == LCD_EXTERN_INDEX_INVALID) {
		if (lcd_debug_print_flag)
			EXTPR("index %d is invalid\n", extconf->index);
		kfree(para);
		return -1;
	}

	/* type: 10byte */
	switch (extconf->type) {
	case LCD_EXTERN_I2C:
		extconf->i2c_addr = *(p + LCD_UKEY_EXT_TYPE_VAL_0);
		extconf->i2c_addr2 = *(p + LCD_UKEY_EXT_TYPE_VAL_1);
		extconf->i2c_bus = lcd_extern_get_i2c_bus_unifykey(
			*(p + LCD_UKEY_EXT_TYPE_VAL_2));
		extconf->i2c_sck_gpio = lcd_ext_i2c_sck_gpio;
		extconf->i2c_sck_gpio_off = lcd_ext_i2c_sck_gpio_off;
		extconf->i2c_sda_gpio = lcd_ext_i2c_sda_gpio;
		extconf->i2c_sda_gpio_off = lcd_ext_i2c_sda_gpio_off;
		if ((lcd_ext_i2c_sck_gpio < LCD_EXTERN_GPIO_NUM_MAX) ||
			(lcd_ext_i2c_sda_gpio < LCD_EXTERN_GPIO_NUM_MAX)) {
			EXTPR("find i2c_gpio_off config\n");
			lcd_extern_gpio_probe(extconf->i2c_sck_gpio);
			lcd_extern_gpio_probe(extconf->i2c_sda_gpio);
		}
		extconf->cmd_size = *(p + LCD_UKEY_EXT_TYPE_VAL_3);

		/* init */
		if (extconf->cmd_size <= 1) {
			EXTERR("cmd_size %d is invalid\n", extconf->cmd_size);
			break;
		}
		if (extconf->cmd_size == LCD_EXTERN_CMD_SIZE_DYNAMIC) {
			ret = lcd_extern_init_table_dynamic_size_load_unifykey(
				extconf, p, key_len, len, 1);
			if (ret)
				break;
			ret = lcd_extern_init_table_dynamic_size_load_unifykey(
				extconf, p, key_len, len, 0);
		} else {
			ret = lcd_extern_init_table_fixed_size_load_unifykey(
				extconf, p, key_len, len, 1);
			if (ret)
				break;
			ret = lcd_extern_init_table_fixed_size_load_unifykey(
				extconf, p, key_len, len, 0);
		}
		if (ret == 0)
			extconf->table_init_loaded = 1;
		break;
	case LCD_EXTERN_SPI:
		extconf->spi_gpio_cs = *(p + LCD_UKEY_EXT_TYPE_VAL_0);
		lcd_extern_gpio_probe(*(p + LCD_UKEY_EXT_TYPE_VAL_0));
		extconf->spi_gpio_clk = *(p + LCD_UKEY_EXT_TYPE_VAL_1);
		lcd_extern_gpio_probe(*(p + LCD_UKEY_EXT_TYPE_VAL_1));
		extconf->spi_gpio_data = *(p + LCD_UKEY_EXT_TYPE_VAL_2);
		lcd_extern_gpio_probe(*(p + LCD_UKEY_EXT_TYPE_VAL_2));
		extconf->spi_clk_freq = (*(p + LCD_UKEY_EXT_TYPE_VAL_3) |
			((*(p + LCD_UKEY_EXT_TYPE_VAL_3 + 1)) << 8) |
			((*(p + LCD_UKEY_EXT_TYPE_VAL_3 + 2)) << 16) |
			((*(p + LCD_UKEY_EXT_TYPE_VAL_3 + 3)) << 24));
		extconf->spi_clk_pol = *(p + LCD_UKEY_EXT_TYPE_VAL_7);
		extconf->cmd_size = *(p + LCD_UKEY_EXT_TYPE_VAL_8);

		/* init */
		if (extconf->cmd_size <= 1) {
			EXTERR("cmd_size %d is invalid\n", extconf->cmd_size);
			break;
		}
		if (extconf->cmd_size == LCD_EXTERN_CMD_SIZE_DYNAMIC) {
			ret = lcd_extern_init_table_dynamic_size_load_unifykey(
				extconf, p, key_len, len, 1);
			if (ret)
				break;
			ret = lcd_extern_init_table_dynamic_size_load_unifykey(
				extconf, p, key_len, len, 0);
		} else {
			ret = lcd_extern_init_table_fixed_size_load_unifykey(
				extconf, p, key_len, len, 1);
			if (ret)
				break;
			ret = lcd_extern_init_table_fixed_size_load_unifykey(
				extconf, p, key_len, len, 0);
		}
		if (ret == 0)
			extconf->table_init_loaded = 1;
		break;
	case LCD_EXTERN_MIPI:
		extconf->cmd_size = *(p + LCD_UKEY_EXT_TYPE_VAL_0);
		if (lcd_debug_print_flag) {
			EXTPR("%s cmd_size=%d\n",
				extconf->name, extconf->cmd_size);
		}
		if (extconf->cmd_size != LCD_EXTERN_CMD_SIZE_DYNAMIC) {
			EXTERR("cmd_size %d is invalid\n", extconf->cmd_size);
			break;
		}
		ret = lcd_extern_init_table_dynamic_size_load_unifykey(
			extconf, p, key_len, len, 1);
		if (ret)
			break;
		ret = lcd_extern_init_table_dynamic_size_load_unifykey(
			extconf, p, key_len, len, 0);
		if (ret == 0)
			extconf->table_init_loaded = 1;
		break;
	default:
		break;
	}

	kfree(para);
	return 0;
}

static int lcd_extern_get_config(void)
{
	struct device_node *child;
	struct lcd_extern_config_s extconf;
	unsigned char *lcd_ext_init_on_table;
	unsigned char *lcd_ext_init_off_table;
	unsigned int extern_para[5];
	const char *str;
	int load_id = 0;
	int ret;

	if (lcd_extern_dev->of_node == NULL) {
		EXTERR("no lcd_extern of_node exist\n");
		return -1;
	}

	lcd_ext_init_on_table =
		kmalloc(sizeof(unsigned char) * LCD_EXTERN_INIT_ON_MAX,
			GFP_KERNEL);
	if (lcd_ext_init_on_table == NULL) {
		EXTERR("failed to alloc default init table\n");
		return -1;
	}
	lcd_ext_init_off_table =
		kmalloc(sizeof(unsigned char) * LCD_EXTERN_INIT_OFF_MAX,
			GFP_KERNEL);
	if (lcd_ext_init_off_table == NULL) {
		EXTERR("failed to alloc default init table\n");
		kfree(lcd_ext_init_on_table);
		return -1;
	}
	lcd_ext_init_on_table[0] = LCD_EXTERN_INIT_END;
	lcd_ext_init_off_table[0] = LCD_EXTERN_INIT_END;

	ret = of_property_read_string(lcd_extern_dev->of_node,
			"i2c_bus", &str);
	if (ret)
		lcd_ext_i2c_bus = LCD_EXTERN_I2C_BUS_INVALID;
	else
		lcd_ext_i2c_bus = lcd_extern_get_i2c_bus_str(str);
	if (lcd_debug_print_flag)
		EXTPR("i2c_bus=%s[%d]\n", str, lcd_ext_i2c_bus);

	ret = of_property_read_u32_array(lcd_extern_dev->of_node,
		"i2c_gpio_off", &extern_para[0], 4);
	if (ret) {
		lcd_ext_i2c_sck_gpio = LCD_EXTERN_GPIO_NUM_MAX;
		lcd_ext_i2c_sck_gpio_off = 2;
		lcd_ext_i2c_sda_gpio = LCD_EXTERN_GPIO_NUM_MAX;
		lcd_ext_i2c_sda_gpio_off = 2;
	} else {
		lcd_ext_i2c_sck_gpio = extern_para[0];
		lcd_ext_i2c_sck_gpio_off = extern_para[1];
		lcd_ext_i2c_sda_gpio = extern_para[2];
		lcd_ext_i2c_sda_gpio_off = extern_para[3];
	}

	ret = of_property_read_u32(lcd_extern_dev->of_node,
			"key_valid", &lcd_ext_key_valid);
	if (ret) {
		if (lcd_debug_print_flag)
			EXTPR("failed to get key_valid\n");
		lcd_ext_key_valid = 0;
	}
	EXTPR("key_valid: %d\n", lcd_ext_key_valid);

	if (lcd_ext_key_valid) {
		ret = lcd_unifykey_check("lcd_extern");
		if (ret < 0)
			load_id = 0;
		else
			load_id = 1;
	}
	memset(&extconf, 0, sizeof(struct lcd_extern_config_s));
	extconf.table_init_on = lcd_ext_init_on_table;
	extconf.table_init_off = lcd_ext_init_off_table;
	if (load_id) {
		EXTPR("%s from unifykey\n", __func__);
		lcd_ext_config_load = 1;
		ret = lcd_extern_get_config_unifykey(&extconf);
		if (ret == 0)
			lcd_extern_add_driver(&extconf);
	} else {
#ifdef CONFIG_OF
		EXTPR("%s from dts\n", __func__);
		lcd_ext_config_load = 0;
		for_each_child_of_node(lcd_extern_dev->of_node, child) {
			ret = lcd_extern_get_config_dts(child, &extconf);
			if (ret == 0)
				lcd_extern_add_driver(&extconf);
		}
#endif
	}

	kfree(lcd_ext_init_on_table);
	kfree(lcd_ext_init_off_table);
	return 0;
}

static int lcd_extern_add_i2c(struct aml_lcd_extern_driver_s *ext_drv)
{
	int ret = 0;

	if (strcmp(ext_drv->config.name, "ext_default") == 0) {
#ifdef LCD_EXTERN_DEFAULT_ENABLE
		ret = aml_lcd_extern_default_probe(ext_drv);
#endif
	} else if (strcmp(ext_drv->config.name, "i2c_T5800Q") == 0) {
#ifdef CONFIG_AMLOGIC_LCD_EXTERN_I2C_T5800Q
		ret = aml_lcd_extern_i2c_T5800Q_probe(ext_drv);
#endif
	} else if (strcmp(ext_drv->config.name, "i2c_tc101") == 0) {
#ifdef CONFIG_AMLOGIC_LCD_EXTERN_I2C_TC101
		ret = aml_lcd_extern_i2c_tc101_probe(ext_drv);
#endif
	} else if (strcmp(ext_drv->config.name, "i2c_anx6345") == 0) {
#ifdef CONFIG_AMLOGIC_LCD_EXTERN_I2C_ANX6345
		ret = aml_lcd_extern_i2c_anx6345_probe(ext_drv);
#endif
	} else if (strcmp(ext_drv->config.name, "i2c_DLPC3439") == 0) {
#ifdef CONFIG_AMLOGIC_LCD_EXTERN_I2C_DLPC3439
		ret = aml_lcd_extern_i2c_DLPC3439_probe(ext_drv);
#endif
	} else {
		EXTERR("invalid driver name: %s\n", ext_drv->config.name);
		ret = -1;
	}
	return ret;
}

static int lcd_extern_add_spi(struct aml_lcd_extern_driver_s *ext_drv)
{
	int ret = 0;

	if (strcmp(ext_drv->config.name, "ext_default") == 0) {
#ifdef LCD_EXTERN_DEFAULT_ENABLE
		ret = aml_lcd_extern_default_probe(ext_drv);
#endif
	} else if (strcmp(ext_drv->config.name, "spi_LD070WS2") == 0) {
#ifdef CONFIG_AMLOGIC_LCD_EXTERN_SPI_LD070WS2
		ret = aml_lcd_extern_spi_LD070WS2_probe(ext_drv);
#endif
	} else {
		EXTERR("invalid driver name: %s\n", ext_drv->config.name);
		ret = -1;
	}
	return ret;
}

static int lcd_extern_add_mipi(struct aml_lcd_extern_driver_s *ext_drv)
{
	int ret = 0;

	if (strcmp(ext_drv->config.name, "mipi_default") == 0) {
		ret = aml_lcd_extern_mipi_default_probe(ext_drv);
	} else if (strcmp(ext_drv->config.name, "mipi_N070ICN") == 0) {
#ifdef CONFIG_AMLOGIC_LCD_EXTERN_MIPI_N070ICN
		ret = aml_lcd_extern_mipi_N070ICN_probe(ext_drv);
#endif
	} else if (strcmp(ext_drv->config.name, "mipi_KD080D13") == 0) {
#ifdef CONFIG_AMLOGIC_LCD_EXTERN_MIPI_KD080D13
		ret = aml_lcd_extern_mipi_KD080D13_probe(ext_drv);
#endif
	} else if (strcmp(ext_drv->config.name, "mipi_TV070WSM") == 0) {
#ifdef CONFIG_AMLOGIC_LCD_EXTERN_MIPI_TV070WSM
		ret = aml_lcd_extern_mipi_TV070WSM_probe(ext_drv);
#endif
	} else if (strcmp(ext_drv->config.name, "mipi_ST7701") == 0) {
#ifdef CONFIG_AMLOGIC_LCD_EXTERN_MIPI_ST7701
		ret = aml_lcd_extern_mipi_st7701_probe(ext_drv);
#endif
	} else if (strcmp(ext_drv->config.name, "mipi_P070ACB") == 0) {
#ifdef CONFIG_AMLOGIC_LCD_EXTERN_MIPI_P070ACB
		ret = aml_lcd_extern_mipi_p070acb_probe(ext_drv);
#endif
	} else if (strcmp(ext_drv->config.name, "mipi_TL050FHV02CT") == 0) {
#ifdef CONFIG_AMLOGIC_LCD_EXTERN_MIPI_TL050FHV02CT
		ret = aml_lcd_extern_mipi_tl050fhv02ct_probe(ext_drv);
#endif
	} else {
		EXTERR("invalid driver name: %s\n", ext_drv->config.name);
		ret = -1;
	}
	return ret;
}

static int lcd_extern_add_invalid(struct aml_lcd_extern_driver_s *ext_drv)
{
	return -1;
}

static int lcd_extern_add_driver(struct lcd_extern_config_s *extconf)
{
	struct aml_lcd_extern_driver_s *ext_drv;
	int i;
	int ret = 0;

	if (lcd_ext_driver_num >= LCD_EXT_DRIVER_MAX) {
		EXTERR("driver num is out of support\n");
		return -1;
	}
	if (extconf->status == 0) {
		EXTERR("driver %s[%d] status is disabled\n",
			extconf->name, extconf->index);
		return -1;
	}

	i = lcd_ext_driver_num;
	lcd_ext_driver[i] =
		kmalloc(sizeof(struct aml_lcd_extern_driver_s), GFP_KERNEL);
	if (lcd_ext_driver[i] == NULL) {
		EXTERR("failed to alloc driver %s[%d], not enough memory\n",
			extconf->name, extconf->index);
		return -1;
	}
	lcd_ext_driver[i]->pinmux_flag = 0;
	ext_drv = lcd_ext_driver[i];
	/* fill config parameters */
	ext_drv->config.index = extconf->index;
	strcpy(ext_drv->config.name, extconf->name);
	ext_drv->config.type = extconf->type;
	ext_drv->config.status = extconf->status;
	ext_drv->config.cmd_size = extconf->cmd_size;
	ext_drv->config.table_init_on = NULL;
	ext_drv->config.table_init_off = NULL;
	ext_drv->config.table_init_loaded = extconf->table_init_loaded;
	ext_drv->config.table_init_on_cnt = extconf->table_init_on_cnt;
	if (ext_drv->config.table_init_loaded) {
		ext_drv->config.table_init_on =
			kmalloc(sizeof(unsigned char) * LCD_EXTERN_INIT_ON_MAX,
				GFP_KERNEL);
		if (ext_drv->config.table_init_on == NULL) {
			EXTERR("failed to alloc driver %s[%d] init table\n",
				extconf->name, extconf->index);
			return -1;
		}
		ext_drv->config.table_init_off =
			kmalloc(sizeof(unsigned char) * LCD_EXTERN_INIT_OFF_MAX,
				GFP_KERNEL);
		if (ext_drv->config.table_init_off == NULL) {
			EXTERR("failed to alloc driver %s[%d] init table\n",
				extconf->name, extconf->index);
			return -1;
		}
		memcpy(ext_drv->config.table_init_on, extconf->table_init_on,
			LCD_EXTERN_INIT_ON_MAX);
		memcpy(ext_drv->config.table_init_off, extconf->table_init_off,
			LCD_EXTERN_INIT_OFF_MAX);
	}

	/* fill config parameters by different type */
	switch (ext_drv->config.type) {
	case LCD_EXTERN_I2C:
		ext_drv->config.i2c_addr = extconf->i2c_addr;
		ext_drv->config.i2c_addr2 = extconf->i2c_addr2;
		ext_drv->config.i2c_bus = extconf->i2c_bus;
		ext_drv->config.i2c_sck_gpio = extconf->i2c_sck_gpio;
		ext_drv->config.i2c_sck_gpio_off = extconf->i2c_sck_gpio_off;
		ext_drv->config.i2c_sda_gpio = extconf->i2c_sda_gpio;
		ext_drv->config.i2c_sda_gpio_off = extconf->i2c_sda_gpio_off;
		ret = lcd_extern_add_i2c(ext_drv);
		break;
	case LCD_EXTERN_SPI:
		ext_drv->config.spi_gpio_cs = extconf->spi_gpio_cs;
		ext_drv->config.spi_gpio_clk = extconf->spi_gpio_clk;
		ext_drv->config.spi_gpio_data = extconf->spi_gpio_data;
		ext_drv->config.spi_clk_freq = extconf->spi_clk_freq;
		ext_drv->config.spi_clk_pol = extconf->spi_clk_pol;
		ret = lcd_extern_add_spi(ext_drv);
		break;
	case LCD_EXTERN_MIPI:
		ret = lcd_extern_add_mipi(ext_drv);
		break;
	default:
		ret = lcd_extern_add_invalid(ext_drv);
		EXTERR("don't support type %d\n", ext_drv->config.type);
		break;
	}
	if (ret) {
		EXTERR("add driver failed\n");
		kfree(lcd_ext_driver[i]->config.table_init_on);
		kfree(lcd_ext_driver[i]->config.table_init_off);
		kfree(lcd_ext_driver[i]);
		lcd_ext_driver[i] = NULL;
		return -1;
	}
	lcd_ext_driver_num++;
	EXTPR("add driver %s(%d)\n",
		ext_drv->config.name, ext_drv->config.index);
	return 0;
}

/* *********************************************************
 * debug function
 * *********************************************************
 */
#define EXT_LEN_MAX   2000
static void lcd_extern_init_table_dynamic_size_print(
		struct lcd_extern_config_s *econf, int flag)
{
	int i, j, k, max_len;
	unsigned char cmd_size;
	char *str;
	unsigned char *init_table;

	str = kmalloc(EXT_LEN_MAX*sizeof(char), GFP_KERNEL);
	if (str == NULL) {
		EXTERR("lcd_extern_dynamic_size str malloc error\n");
		return;
	}
	if (flag) {
		pr_info("power on:\n");
		init_table = econf->table_init_on;
		max_len = LCD_EXTERN_INIT_ON_MAX;
	} else {
		pr_info("power off:\n");
		init_table = econf->table_init_off;
		max_len = LCD_EXTERN_INIT_OFF_MAX;
	}
	if (init_table == NULL) {
		EXTERR("init_table %d is NULL\n", flag);
		kfree(str);
		return;
	}

	i = 0;
	switch (econf->type) {
	case LCD_EXTERN_I2C:
	case LCD_EXTERN_SPI:
		while ((i + 1) < max_len) {
			if (init_table[i] == LCD_EXTERN_INIT_END) {
				pr_info("  0x%02x,%d,\n",
					init_table[i], init_table[i+1]);
				break;
			}

			cmd_size = init_table[i+1];
			k = snprintf(str, EXT_LEN_MAX, "  0x%02x,%d,",
				init_table[i], cmd_size);
			if (cmd_size > 0) {
				for (j = 0; j < cmd_size; j++) {
					k += snprintf(str+k, EXT_LEN_MAX,
						"0x%02x,",
						init_table[i+2+j]);
				}
			}
			pr_info("%s\n", str);
			i += (cmd_size + 2);
		}
		break;
	case LCD_EXTERN_MIPI:
		while ((i + 1) < max_len) {
			if (init_table[i] == 0xff) { /* ctrl flag */
				cmd_size = 0;
				if (init_table[i+1] == 0xff) {
					pr_info("  0x%02x,0x%02x,\n",
						init_table[i], init_table[i+1]);
					break;
				}
				pr_info("  0x%02x,%d,\n",
					init_table[i], init_table[i+1]);
			} else if (init_table[i] == 0xf0) { /* gpio */
				cmd_size = init_table[i+DSI_CMD_SIZE_INDEX];
				k = snprintf(str, EXT_LEN_MAX, "  0x%02x,%d,",
					init_table[i], cmd_size);
				for (j = 0; j < cmd_size; j++) {
					k += snprintf(str+k, EXT_LEN_MAX,
						"%d,", init_table[i+2+j]);
				}
				pr_info("%s\n", str);
			} else if ((init_table[i] & 0xf) == 0x0) {
				pr_info("  init_%s wrong data_type: 0x%02x\n",
					flag ? "on" : "off", init_table[i]);
				break;
			} else {
				cmd_size = init_table[i+DSI_CMD_SIZE_INDEX];
				k = snprintf(str, EXT_LEN_MAX, "  0x%02x,%d,",
					init_table[i], cmd_size);
				for (j = 0; j < cmd_size; j++) {
					k += snprintf(str+k, EXT_LEN_MAX,
						"0x%02x,",
						init_table[i+2+j]);
				}
				pr_info("%s\n", str);
			}
			i += (cmd_size + 2);
		}
		break;
	default:
		break;
	}
	kfree(str);
}

static void lcd_extern_init_table_fixed_size_print(
		struct lcd_extern_config_s *econf, int flag)
{
	int i, j, k, max_len;
	unsigned char cmd_size;
	char *str;
	unsigned char *init_table;

	str = kmalloc(EXT_LEN_MAX*sizeof(char), GFP_KERNEL);
	if (str == NULL) {
		EXTERR("lcd_extern_fixed_size str malloc error\n");
		return;
	}
	cmd_size = econf->cmd_size;
	if (flag) {
		pr_info("power on:\n");
		init_table = econf->table_init_on;
		max_len = LCD_EXTERN_INIT_ON_MAX;
	} else {
		pr_info("power off:\n");
		init_table = econf->table_init_off;
		max_len = LCD_EXTERN_INIT_OFF_MAX;
	}
	if (init_table == NULL) {
		EXTERR("init_table %d is NULL\n", flag);
		kfree(str);
		return;
	}

	i = 0;
	while (i < max_len) {
		k = snprintf(str, EXT_LEN_MAX, " ");
		for (j = 0; j < cmd_size; j++) {
			k += snprintf(str+k, EXT_LEN_MAX, " 0x%02x",
				init_table[i+j]);
		}
		pr_info("%s\n", str);

		if (init_table[i] == LCD_EXTERN_INIT_END)
			break;
		i += cmd_size;
	}
	kfree(str);
}

static void lcd_extern_config_dump(struct aml_lcd_extern_driver_s *ext_drv)
{
	struct lcd_extern_config_s *econf;

	if (ext_drv == NULL)
		return;

	econf = &ext_drv->config;
	EXTPR("driver %s(%d) info:\n", econf->name, econf->index);
	pr_info("status:          %d\n", econf->status);
	switch (econf->type) {
	case LCD_EXTERN_I2C:
		pr_info("type:            i2c(%d)\n", econf->type);
		pr_info("cmd_size:        %d\n"
			"i2c_addr:        0x%02x\n"
			"i2c_addr2:       0x%02x\n"
			"i2c_bus:         %d\n"
			"table_loaded:    %d\n",
			econf->cmd_size, econf->i2c_addr,
			econf->i2c_addr2, econf->i2c_bus,
			econf->table_init_loaded);
		if (econf->cmd_size == 0)
			break;
		if (econf->cmd_size == LCD_EXTERN_CMD_SIZE_DYNAMIC) {
			lcd_extern_init_table_dynamic_size_print(econf, 1);
			lcd_extern_init_table_dynamic_size_print(econf, 0);
		} else {
			lcd_extern_init_table_fixed_size_print(econf, 1);
			lcd_extern_init_table_fixed_size_print(econf, 0);
		}
		break;
	case LCD_EXTERN_SPI:
		pr_info("type:            spi(%d)\n", econf->type);
		pr_info("cmd_size:        %d\n"
			"spi_gpio_cs:     %d\n"
			"spi_gpio_clk:    %d\n"
			"spi_gpio_data:   %d\n"
			"spi_clk_freq:    %dHz\n"
			"spi_delay_us:    %d\n"
			"spi_clk_pol:     %d\n"
			"table_loaded:    %d\n",
			econf->cmd_size, econf->spi_gpio_cs,
			econf->spi_gpio_clk, econf->spi_gpio_data,
			econf->spi_clk_freq, econf->spi_delay_us,
			econf->spi_clk_pol, econf->table_init_loaded);
		if (econf->cmd_size == 0)
			break;
		if (econf->cmd_size == LCD_EXTERN_CMD_SIZE_DYNAMIC) {
			lcd_extern_init_table_dynamic_size_print(econf, 1);
			lcd_extern_init_table_dynamic_size_print(econf, 0);
		} else {
			lcd_extern_init_table_fixed_size_print(econf, 1);
			lcd_extern_init_table_fixed_size_print(econf, 0);
		}
		break;
	case LCD_EXTERN_MIPI:
		pr_info("type:            mipi(%d)\n", econf->type);
		pr_info("cmd_size:        %d\n", econf->cmd_size);
		if (econf->cmd_size != LCD_EXTERN_CMD_SIZE_DYNAMIC)
			break;
		lcd_extern_init_table_dynamic_size_print(econf, 1);
		lcd_extern_init_table_dynamic_size_print(econf, 0);
		break;
	default:
		pr_info("not support extern_type\n");
		break;
	}
	pr_info("\n");
}

static const char *lcd_extern_debug_usage_str = {
"Usage:\n"
"    echo index <n> > info ; dump specified index driver config\n"
"    echo all > info ; dump all driver config\n"
};

static ssize_t lcd_extern_debug_help(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", lcd_extern_debug_usage_str);
}

static ssize_t lcd_extern_info_dump(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned int ret = 0;
	int i, index;
	struct aml_lcd_extern_driver_s *ext_drv;

	index = LCD_EXTERN_INDEX_INVALID;
	switch (buf[0]) {
	case 'i':
		ret = sscanf(buf, "index %d", &index);
		if (ret == 1) {
			ext_drv = aml_lcd_extern_get_driver(index);
			lcd_extern_config_dump(ext_drv);
		} else {
			EXTERR("invalid parameters\n");
		}
		break;
	case 'a':
		for (i = 0; i < lcd_ext_driver_num; i++)
			lcd_extern_config_dump(lcd_ext_driver[i]);
		break;
	default:
		EXTERR("invalid command\n");
		break;
	}

	return count;
}

static ssize_t lcd_extern_debug_key_valid_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", lcd_ext_key_valid);
}

static ssize_t lcd_extern_debug_config_load_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", lcd_ext_config_load);
}

static const char *lcd_extern_debug_test_usage_str = {
"Usage:\n"
"    echo <index> <on/off> > test ; test power on/off for index extern device\n"
"        <on/off>: 1 for power on, 0 for power off\n"
};

static ssize_t lcd_extern_debug_test_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", lcd_extern_debug_test_usage_str);
}

static ssize_t lcd_extern_debug_test_store(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned int ret;
	int index, flag = 0;
	struct aml_lcd_extern_driver_s *ext_drv;

	index = LCD_EXTERN_INDEX_INVALID;
	ret = sscanf(buf, "%d %d", &index, &flag);
	ext_drv = aml_lcd_extern_get_driver(index);
	if (ext_drv) {
		if (flag) {
			if (ext_drv->power_on)
				ext_drv->power_on(ext_drv);
		} else {
			if (ext_drv->power_off)
				ext_drv->power_off(ext_drv);
		}
	}

	return count;
}

static struct class_attribute lcd_extern_class_attrs[] = {
	__ATTR(info, 0644,
		lcd_extern_debug_help, lcd_extern_info_dump),
	__ATTR(key_valid,   0444,
		 lcd_extern_debug_key_valid_show, NULL),
	__ATTR(config_load, 0444,
		lcd_extern_debug_config_load_show, NULL),
	__ATTR(test, 0644,
		lcd_extern_debug_test_show, lcd_extern_debug_test_store),
};

static struct class *debug_class;
static int creat_lcd_extern_class(void)
{
	int i;

	debug_class = class_create(THIS_MODULE, "lcd_ext");
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

static int remove_lcd_extern_class(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(lcd_extern_class_attrs); i++)
		class_remove_file(debug_class, &lcd_extern_class_attrs[i]);

	class_destroy(debug_class);
	debug_class = NULL;

	return 0;
}
/* ********************************************************* */

static int aml_lcd_extern_probe(struct platform_device *pdev)
{
	lcd_extern_dev = &pdev->dev;
	lcd_ext_driver_num = 0;
	lcd_extern_get_config(); /* also add ext_driver */

	creat_lcd_extern_class();

	EXTPR("%s ok\n", __func__);
	return 0;
}

static int aml_lcd_extern_remove(struct platform_device *pdev)
{
	int i;

	remove_lcd_extern_class();
	for (i = 0; i < lcd_ext_driver_num; i++) {
		kfree(lcd_ext_driver[i]->config.table_init_on);
		kfree(lcd_ext_driver[i]->config.table_init_off);
		kfree(lcd_ext_driver[i]);
		lcd_ext_driver[i] = NULL;
	}
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id aml_lcd_extern_dt_match[] = {
	{
		.compatible = "amlogic, lcd_extern",
	},
	{},
};
#endif

static struct platform_driver aml_lcd_extern_driver = {
	.probe  = aml_lcd_extern_probe,
	.remove = aml_lcd_extern_remove,
	.driver = {
		.name  = "lcd_extern",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = aml_lcd_extern_dt_match,
#endif
	},
};

static int __init aml_lcd_extern_init(void)
{
	int ret;

	if (lcd_debug_print_flag)
		EXTPR("%s\n", __func__);

	ret = platform_driver_register(&aml_lcd_extern_driver);
	if (ret) {
		EXTERR("driver register failed\n");
		return -ENODEV;
	}
	return ret;
}

static void __exit aml_lcd_extern_exit(void)
{
	platform_driver_unregister(&aml_lcd_extern_driver);
}

late_initcall(aml_lcd_extern_init);
module_exit(aml_lcd_extern_exit);

MODULE_AUTHOR("AMLOGIC");
MODULE_DESCRIPTION("LCD extern driver");
MODULE_LICENSE("GPL");

