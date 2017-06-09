/*
 * include/linux/amlogic/media/vout/lcd/aml_bl_extern.h
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

#ifndef __AMLOGIC_BL_EXTERN_H_
#define __AMLOGIC_BL_EXTERN_H_

#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/pinctrl/consumer.h>

enum bl_extern_type_e {
	BL_EXTERN_I2C = 0,
	BL_EXTERN_SPI,
	BL_EXTERN_OTHER,
	BL_EXTERN_MAX,
};

struct bl_extern_config_s {
	const char *name;
	enum bl_extern_type_e type;
	struct gpio_desc *gpio;
	unsigned char gpio_on;
	unsigned char gpio_off;

	int i2c_addr;
	int i2c_bus;
	struct gpio_desc *spi_cs;
	struct gpio_desc *spi_clk;
	struct gpio_desc *spi_data;
	unsigned int dim_min;
	unsigned int dim_max;
	/* unsigned int level_min; */
	/* unsigned int level_max; */
};

/*******global API******/
struct aml_bl_extern_driver_t {
	const char *name;
	enum bl_extern_type_e type;
	int (*power_on)(void);
	int (*power_off)(void);
	int (*set_level)(unsigned int level);
};

#define BL_EXTERN_DRIVER		"bl_extern"


#define bl_extern_gpio_free(gpio)	gpiod_free(gpio, BL_EXTERN_DRIVER)
#define bl_extern_gpio_input(gpio)        gpiod_direction_input(gpio)
#define bl_extern_gpio_output(gpio, val)  gpiod_direction_output(gpio, val)
#define bl_extern_gpio_get_value(gpio)              gpiod_get_value(gpio)
#define bl_extern_gpio_set_value(gpio, val)         gpiod_set_value(gpio, val)


extern struct aml_bl_extern_driver_t *aml_bl_extern_get_driver(void);
extern int bl_extern_driver_check(void);
extern int get_bl_extern_dt_data(struct device dev,
					struct bl_extern_config_s *pdata);

extern void get_bl_ext_level(struct bl_extern_config_s *bl_ext_cfg);

#endif

