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

enum lcd_extern_i2c_bus_e {
	LCD_EXTERN_I2C_BUS_AO = 0,
	LCD_EXTERN_I2C_BUS_A,
	LCD_EXTERN_I2C_BUS_B,
	LCD_EXTERN_I2C_BUS_C,
	LCD_EXTERN_I2C_BUS_D,
	LCD_EXTERN_I2C_BUS_MAX,
};
#define LCD_EXTERN_I2C_BUS_INVALID    0xff

#define LCD_EXTERN_SPI_CLK_FREQ_DFT   10000 /* default 10k */

#define LCD_EXTERN_INIT_ON_MAX        500
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
	unsigned char i2c_addr;
	unsigned char i2c_addr2;
	unsigned char i2c_bus;
	unsigned char spi_gpio_cs;
	unsigned char spi_gpio_clk;
	unsigned char spi_gpio_data;
	unsigned int spi_clk_freq;
	unsigned int spi_delay_us;
	unsigned char spi_clk_pol;
	unsigned char cmd_size;
	unsigned char table_init_loaded; /* internal use */
	unsigned char *table_init_on;
	unsigned char *table_init_off;
};

/* global API */
#define LCD_EXT_DRIVER_MAX    10
struct aml_lcd_extern_driver_s {
	struct lcd_extern_config_s config;
	int (*reg_read)(unsigned char reg, unsigned char *buf);
	int (*reg_write)(unsigned char reg, unsigned char value);
	int (*power_on)(void);
	int (*power_off)(void);
};

extern struct aml_lcd_extern_driver_s *aml_lcd_extern_get_driver(int index);

#endif

