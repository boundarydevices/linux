/*
 * drivers/amlogic/media/vout/lcd/lcd_extern/mipi_default.c
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
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/amlogic/media/vout/lcd/lcd_extern.h>
#include "lcd_extern.h"


#define LCD_EXTERN_INDEX		0
#define LCD_EXTERN_NAME			"mipi_default"
#define LCD_EXTERN_TYPE			LCD_EXTERN_MIPI

/******************** mipi command ********************/
/**format:  data_type, num, data**/
/*special: data_type=0xff, num<0xff means delay ms, num=0xff means ending.*/
static unsigned char mipi_init_on_table[] = {
	0x05, 1,  0x11,
	0xff, 200,
	0x05, 1,  0x29,
	0xff, 20,
	0xFF, 0xFF,   /* ending flag */
};

static unsigned char mipi_init_off_table[] = {
	0x05, 1, 0x28, /* display off */
	0xFF, 10,      /* delay 10ms */
	0x05, 1, 0x10, /* sleep in */
	0xFF, 150,      /* delay 150ms */
	0xFF, 0xFF,   /* ending flag */
};

static int lcd_extern_driver_update(struct aml_lcd_extern_driver_s *ext_drv)
{
	if (ext_drv == NULL) {
		EXTERR("%s driver is null\n", LCD_EXTERN_NAME);
		return -1;
	}

	if (ext_drv->config->type == LCD_EXTERN_MAX) { //default for no dt
		ext_drv->config->index = LCD_EXTERN_INDEX;
		ext_drv->config->type = LCD_EXTERN_TYPE;
		strcpy(ext_drv->config->name, LCD_EXTERN_NAME);

		ext_drv->config->table_init_on = &mipi_init_on_table[0];
		ext_drv->config->table_init_off = &mipi_init_off_table[0];
	}


	return 0;
}

int aml_lcd_extern_mipi_default_get_default_index(void)
{
	return LCD_EXTERN_INDEX;
}

int aml_lcd_extern_mipi_default_probe(
	struct aml_lcd_extern_driver_s *ext_drv)
{
	int ret = 0;

	ret = lcd_extern_driver_update(ext_drv);

	if (lcd_debug_print_flag)
		EXTPR("%s: %d\n", __func__, ret);
	return ret;
}
