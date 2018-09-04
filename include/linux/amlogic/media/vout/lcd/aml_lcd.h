/*
 * include/linux/amlogic/media/vout/lcd/aml_lcd.h
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

#ifndef _INC_AML_LCD_COMMON_H_
#define _INC_AML_LCD_COMMON_H_


#define LCD_EXT_I2C_BUS_0          0  /* A */
#define LCD_EXT_I2C_BUS_1          1  /* B */
#define LCD_EXT_I2C_BUS_2          2  /* C */
#define LCD_EXT_I2C_BUS_3          3  /* D */
#define LCD_EXT_I2C_BUS_4          4  /* AO */
#define LCD_EXT_I2C_BUS_MAX        0xff

#define LCD_EXT_I2C_BUS_INVALID    0xff
#define LCD_EXT_I2C_ADDR_INVALID   0xff
#define LCD_EXT_GPIO_INVALID       0xff

#define LCD_EXT_SPI_CLK_FREQ_DFT   10 /* unit: KHz */

/*******************************************/
/*        LCD EXT CMD                      */
/*******************************************/
#define LCD_EXT_CMD_TYPE_CMD_DELAY    0x00
#define LCD_EXT_CMD_TYPE_CMD2_DELAY   0x01  /* for i2c device 2nd addr */
#define LCD_EXT_CMD_TYPE_NONE         0x10
#define LCD_EXT_CMD_TYPE_CMD          0xc0
#define LCD_EXT_CMD_TYPE_CMD2         0xc1  /* for i2c device 2nd addr */
#define LCD_EXT_CMD_TYPE_GPIO         0xf0
#define LCD_EXT_CMD_TYPE_CHECK        0xfc
#define LCD_EXT_CMD_TYPE_DELAY        0xfd
#define LCD_EXT_CMD_TYPE_END          0xff

#define LCD_EXT_CMD_SIZE_DYNAMIC      0xff
#define LCD_EXT_DYNAMIC_SIZE_INDEX    1


#endif

