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

#ifndef _INC_AML_BL_EXTERN_H_
#define _INC_AML_BL_EXTERN_H_

enum bl_extern_type_e {
	BL_EXTERN_I2C = 0,
	BL_EXTERN_SPI,
	BL_EXTERN_MIPI,
	BL_EXTERN_MAX,
};

enum bl_extern_i2c_bus_e {
	BL_EXTERN_I2C_BUS_0 = 0,
	BL_EXTERN_I2C_BUS_1,
	BL_EXTERN_I2C_BUS_2,
	BL_EXTERN_I2C_BUS_3,
	BL_EXTERN_I2C_BUS_4,
	BL_EXTERN_I2C_BUS_MAX,
};
#define BL_EXTERN_I2C_BUS_INVALID   0xff

#define BL_EXTERN_SPI_CLK_FREQ_DFT  10000 /* default 10k */

#define BL_EXTERN_INIT_TABLE_MAX    500

#define BL_EXTERN_INIT_CMD          0x00
#define BL_EXTERN_INIT_NONE         0xf0
#define BL_EXTERN_INIT_END          0xff

#define BL_EXTERN_DYNAMIC_LEN		0xff

#define BL_EXTERN_GPIO_NUM_MAX      6
#define BL_EXTERN_INDEX_INVALID     0xff
#define BL_EXTERN_NAME_LEN_MAX      30
struct bl_extern_config_s {
	unsigned char index;
	char name[BL_EXTERN_NAME_LEN_MAX];
	enum bl_extern_type_e type;
	unsigned char i2c_addr;
	unsigned char i2c_bus;
	unsigned int dim_min;
	unsigned int dim_max;
};

/* global API */
struct aml_bl_extern_driver_s {
	unsigned char status;
	unsigned int brightness;
	int (*power_on)(void);
	int (*power_off)(void);
	int (*set_level)(unsigned int level);
	void (*config_print)(void);
	int (*device_power_on)(void);
	int (*device_power_off)(void);
	int (*device_bri_update)(unsigned int level);
	struct bl_extern_config_s config;
	struct device *dev;
};

extern struct aml_bl_extern_driver_s *aml_bl_extern_get_driver(void);
extern int aml_bl_extern_device_load(int index);

#ifdef CONFIG_AMLOGIC_LCD_TABLET
extern int dsi_write_cmd(unsigned char *payload);
#endif


#endif

