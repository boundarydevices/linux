/*
 * drivers/amlogic/input/saradc/saradc_reg.h
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

#ifndef __SARADC_REG_H__
#define __SARADC_REG_H__

#define SARADC_REG0 (0<<2)
#define SARADC_CH_LIST (1<<2)
#define SARADC_AVG_CNTL (2<<2)
#define SARADC_REG3 (3<<2)
#define SARADC_DELAY (4<<2)
#define SARADC_LAST_RD (5<<2)
#define SARADC_FIFO_RD (6<<2)
#define SARADC_AUX_SW (7<<2)
#define SARADC_CH10_SW (8<<2)
#define SARADC_DETECT_IDLE_SW (9<<2)
#define SARADC_DELTA_10 (10<<2)
#define SARADC_REG11 (11<<2)

#define SAMPLE_ENGINE_EN    bits_desc(SARADC_REG0, 0, 1)
#define START_SAMPLE        bits_desc(SARADC_REG0, 2, 1)
#define STOP_SAMPLE         bits_desc(SARADC_REG0, 14, 1)
#define FIFO_COUNT          bits_desc(SARADC_REG0, 21, 5)
#define SAMPLE_BUSY         bits_desc(SARADC_REG0, 28, 1)
#define AVG_BUSY            bits_desc(SARADC_REG0, 29, 1)
#define DELTA_BUSY          bits_desc(SARADC_REG0, 30, 1)
#define ALL_BUSY            bits_desc(SARADC_REG0, 28, 3)
#define CLK_DIV             bits_desc(SARADC_REG3, 10, 1)
#define ADC_EN              bits_desc(SARADC_REG3, 21, 1)
#define CAL_CNTL            bits_desc(SARADC_REG3, 23, 3)
#define FLAG_INITIALIZED    bits_desc(SARADC_REG3, 28, 1) /* for bl30 */
#define CLK_EN              bits_desc(SARADC_REG3, 30, 1)
#define FLAG_BUSY_KERNEL    bits_desc(SARADC_DELAY, 14, 1) /* for bl30 */
#define FLAG_BUSY_BL30      bits_desc(SARADC_DELAY, 15, 1) /* for bl30 */
#define IDLE_MUX            bits_desc(SARADC_DETECT_IDLE_SW, 7, 3)
#define DETECT_MUX          bits_desc(SARADC_DETECT_IDLE_SW, 23, 3)
#define BANDGAP_EN          bits_desc(SARADC_REG11, 13, 1)
/* saradc clock register */
#define REGC_CLK_DIV        bits_desc(0, 0, 8)
#define REGC_CLK_EN         bits_desc(0, 8, 1)
#define REGC_CLK_SRC        bits_desc(0, 9, 3)

#define FIFO_MAX 32

#endif
