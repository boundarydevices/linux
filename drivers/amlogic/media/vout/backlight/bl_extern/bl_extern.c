/*
 * drivers/amlogic/media/vout/backlight/bl_extern/bl_extern.c
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
#include <linux/amlogic/media/vout/lcd/aml_bl_extern.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>

#ifdef BL_EXT_DEBUG_INFO
#define DBG_PRINT(...)		pr_info(__VA_ARGS__)
#else
#define DBG_PRINT(...)
#endif

static struct aml_bl_extern_driver_t bl_ext_driver = {
	.type = BL_EXTERN_MAX,
	.name = NULL,
	.power_on = NULL,
	.power_off = NULL,
	.set_level = NULL,

};

struct aml_bl_extern_driver_t *aml_bl_extern_get_driver(void)
{
	return &bl_ext_driver;
}

int bl_extern_driver_check(void)
{
	struct aml_bl_extern_driver_t *bl_ext;

	bl_ext = aml_bl_extern_get_driver();
	if (bl_ext) {
		if (bl_ext->type < BL_EXTERN_MAX) {
			pr_err("[warning]: bl_extern has already exist (%s)\n",
								bl_ext->name);
			return -1;
		}
	} else {
		pr_err("get bl_extern_driver failed\n");
	}
	return 0;
}

int get_bl_extern_dt_data(struct device dev, struct bl_extern_config_t *pdata)
{
	int ret;
	struct device_node *of_node = dev.of_node;
	u32 bl_para[2];
	const char *str;
	struct gpio_desc *bl_extern_gpio;

	ret = of_property_read_string(of_node, "dev_name",
					(const char **)&pdata->name);
	if (ret) {
		pdata->name = "aml_bl_extern";
		pr_err("warning: get dev_name failed\n");
	}

	ret = of_property_read_u32(of_node, "type", &pdata->type);
	if (ret) {
		pdata->type = BL_EXTERN_MAX;
		pr_err("%s warning: get type failed, exit\n", pdata->name);
		return -1;
	}
	pdata->gpio_used = 0;
	ret = of_property_read_string_index(of_node,
						"gpio_enable_on_off", 0, &str);
	if (ret) {
		pr_warn("%s warning: get gpio_enable failed\n", pdata->name);
	} else {
		if (strncmp(str, "G", 1) == 0) {
			pdata->gpio_used = 1;
			bl_extern_gpio = gpiod_get(&dev, str);
			if (bl_extern_gpio) {
				pr_warn("%s warning:failed to alloc gpio (%s)\n",
							pdata->name, str);
			}
			pdata->gpio = bl_extern_gpio;
		}
		DBG_PRINT("%s: gpio_enable %s\n",
				pdata->name, ((pdata->gpio_used) ? str:"none"));
	}
	ret = of_property_read_string_index(of_node,
					"gpio_enable_on_off", 1, &str);
	if (ret) {
		pr_warn("%s warning: get gpio_enable_on failed\n", pdata->name);
	} else {
		if (strncmp(str, "2", 1) == 0)
			pdata->gpio_on = LCD_POWER_GPIO_INPUT;
		else if (strncmp(str, "0", 1) == 0)
			pdata->gpio_on = LCD_POWER_GPIO_OUTPUT_LOW;
		else
			pdata->gpio_on = LCD_POWER_GPIO_OUTPUT_HIGH;
	}
	ret = of_property_read_string_index(of_node,
					"gpio_enable_on_off", 2, &str);
	if (ret) {
		pr_warn("%s warning:get gpio_enable_off failed\n", pdata->name);
	} else {
		if (strncmp(str, "2", 1) == 0)
			pdata->gpio_off = LCD_POWER_GPIO_INPUT;
		else if (strncmp(str, "1", 1) == 0)
			pdata->gpio_off = LCD_POWER_GPIO_OUTPUT_HIGH;
		else
			pdata->gpio_off = LCD_POWER_GPIO_OUTPUT_LOW;
	}
	DBG_PRINT("%s: gpio_on = %d,", pdata->name, pdata->gpio_on);
	DBG_PRINT("gpio_off = %d\n", pdata->gpio_off);
	switch (pdata->type) {
	case BL_EXTERN_I2C:
		ret = of_property_read_u32(of_node, "i2c_address",
							&pdata->i2c_addr);
		if (ret) {
			pr_warn("%s warning: get i2c_address failed\n",
								pdata->name);
			pdata->i2c_addr = 0;
		}
		DBG_PRINT("%s: i2c_address=", pdata->name);
		DBG_PRINT("0x%02x\n", pdata->i2c_addr);
		ret = of_property_read_string(of_node, "i2c_bus", &str);
		if (ret) {
			pr_warn("%s warning: get i2c_bus failed,", pdata->name);
			pr_warn("use default i2c bus\n");
			pdata->i2c_bus = AML_I2C_MASTER_A;
		} else {
			if (strncmp(str, "i2c_bus_a", 9) == 0)
				pdata->i2c_bus = AML_I2C_MASTER_A;
			else if (strncmp(str, "i2c_bus_b", 9) == 0)
				pdata->i2c_bus = AML_I2C_MASTER_B;
			else if (strncmp(str, "i2c_bus_c", 9) == 0)
				pdata->i2c_bus = AML_I2C_MASTER_C;
			else if (strncmp(str, "i2c_bus_d", 9) == 0)
				pdata->i2c_bus = AML_I2C_MASTER_D;
			else if (strncmp(str, "i2c_bus_ao", 10) == 0)
				pdata->i2c_bus = AML_I2C_MASTER_AO;
			else
				pdata->i2c_bus = AML_I2C_MASTER_A;
		}
		DBG_PRINT("%s: i2c_bus=%s[%d]\n", pdata->name,
						str, pdata->i2c_bus);
		break;
	case BL_EXTERN_SPI:
		ret = of_property_read_string(of_node, "gpio_spi_cs", &str);
		if (ret) {
			pr_warn("%s warning: get spi gpio_spi_cs failed\n",
								pdata->name);
			pdata->spi_cs = NULL;
		} else {
			bl_extern_gpio = gpiod_get(&dev, str);
			if (bl_extern_gpio) {
				pdata->spi_cs = bl_extern_gpio;
				DBG_PRINT("spi_cs gpio = %s\n", str);
			} else {
				pr_warn("%s warning:failed to alloc gpio (%s)\n",
							pdata->name, str);
				pdata->spi_cs = NULL;
			}
		}
		ret = of_property_read_string(of_node, "gpio_spi_clk", &str);
		if (ret) {
			pr_warn("%s warning: get spi gpio_spi_clk failed\n",
								pdata->name);
			pdata->spi_clk = NULL;
		} else {
			bl_extern_gpio = gpiod_get(&dev, str);
			if (bl_extern_gpio) {
				pdata->spi_clk = bl_extern_gpio;
				DBG_PRINT("spi_cs gpio = %s\n", str);
			} else {
				pdata->spi_clk = NULL;
				pr_warn("%s warning:failed to alloc gpio (%s)\n",
							pdata->name, str);
			}
		}
		ret = of_property_read_string(of_node, "gpio_spi_data", &str);
		if (ret) {
			pr_warn("%s warning: get spi gpio_spi_data failed\n",
								pdata->name);
			pdata->spi_data = NULL;
		}  else {
			bl_extern_gpio = gpiod_get(&dev, str);
			if (bl_extern_gpio) {
				pdata->spi_data = bl_extern_gpio;
				DBG_PRINT("spi_cs gpio = %s\n", str);
			} else {
				pdata->spi_data = NULL;
				pr_warn("%s warning:failed to alloc gpio (%s)\n",
							pdata->name, str);
			}
		}
		break;
	case BL_EXTERN_OTHER:
		break;
	default:
		break;
	}
	ret = of_property_read_u32_array(of_node, "dim_max_min",
							&bl_para[0], 2);
	if (ret) {
		pr_warn("%s warning: get dim_max_min failed\n", pdata->name);
		pdata->dim_max = 0;
		pdata->dim_min = 0;
	}  else {
		pdata->dim_max = bl_para[0];
		pdata->dim_min = bl_para[1];
	}
	DBG_PRINT("%s dim_min = %d, dim_max = %d\n", pdata->name,
			pdata->dim_min, pdata->dim_max);
	return 0;
}
