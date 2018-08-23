/*
 * include/linux/amlogic/media/vout/lcd/lcd_extern.h
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

#ifndef _INC_AML_LCD_EXTERN_H_
#define _INC_AML_LCD_EXTERN_H_

enum lcd_extern_type_e {
	LCD_EXTERN_I2C = 0,
	LCD_EXTERN_SPI,
	LCD_EXTERN_MIPI,
	LCD_EXTERN_MAX,
};

#define LCD_EXTERN_I2C_BUS_0          0
#define LCD_EXTERN_I2C_BUS_1          1
#define LCD_EXTERN_I2C_BUS_2          2
#define LCD_EXTERN_I2C_BUS_3          3
#define LCD_EXTERN_I2C_BUS_4          4
#define LCD_EXTERN_I2C_BUS_MAX        0xff

#define LCD_EXTERN_I2C_BUS_INVALID    0xff
#define LCD_EXTERN_I2C_ADDR_INVALID   0xff

#define LCD_EXTERN_SPI_CLK_FREQ_DFT   10000 /* default 10k */

#define LCD_EXTERN_INIT_ON_MAX        3000
#define LCD_EXTERN_INIT_OFF_MAX       100

#define LCD_EXTERN_INIT_CMD           0x00
#define LCD_EXTERN_INIT_CMD2          0x01  /* only for special i2c device */
#define LCD_EXTERN_INIT_NONE          0x10
#define LCD_EXTERN_INIT_GPIO          0xf0
#define LCD_EXTERN_INIT_END           0xff

#define LCD_EXTERN_CMD_SIZE_DYNAMIC   0xff
#define LCD_EXTERN_DYNAMIC_SIZE_INDEX 1

#define LCD_EXTERN_GPIO_NUM_MAX       6
#define LCD_EXTERN_INDEX_INVALID      0xff
#define LCD_EXTERN_NAME_LEN_MAX       30
struct lcd_extern_config_s {
	unsigned char index;
	char name[LCD_EXTERN_NAME_LEN_MAX];
	enum lcd_extern_type_e type;
	unsigned char status;
	unsigned char pinmux_valid;
	unsigned char pinmux_gpio_off;
	unsigned char key_valid;
	unsigned char addr_sel; /* internal used */

	unsigned char i2c_addr;
	unsigned char i2c_addr2;
	unsigned char i2c_bus;
	unsigned char i2c_sck_gpio;
	unsigned char i2c_sda_gpio;

	unsigned char spi_gpio_cs;
	unsigned char spi_gpio_clk;
	unsigned char spi_gpio_data;
	unsigned int spi_clk_freq;
	unsigned int spi_delay_us;
	unsigned char spi_clk_pol;

	unsigned char cmd_size;
	unsigned char *table_init_on;
	unsigned char *table_init_off;
	unsigned char table_init_loaded; /* internal use */
	unsigned int table_init_on_cnt; /* internal use */
};

/* global API */
struct aml_lcd_extern_driver_s {
	struct lcd_extern_config_s *config;
	int (*reg_read)(unsigned char reg, unsigned char *buf);
	int (*reg_write)(unsigned char reg, unsigned char value);
	int (*power_on)(void);
	int (*power_off)(void);
	struct pinctrl *pin;
	unsigned int pinmux_flag;
};

#define LCD_EXT_I2C_DEV_MAX    10
struct aml_lcd_extern_i2c_dev_s {
	char name[30];
	struct i2c_client *client;
};

extern struct aml_lcd_extern_driver_s *aml_lcd_extern_get_driver(int index);

#endif

