/*
 * drivers/amlogic/media/dtv_demod/include/addr_dtmb_front.h
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

#ifndef __ADDR_DTMB_FRONT_H__
#define __ADDR_DTMB_FRONT_H__

/*#include "addr_dtmb_top.h"*/

#define  DTMB_FRONT_ADDR(x) (DTMB_DEMOD_BASE + (x << 2))

#define  DTMB_FRONT_AFIFO_ADC                 DTMB_FRONT_ADDR(0x20)
#define  DTMB_FRONT_AGC_CONFIG1               DTMB_FRONT_ADDR(0x21)
#define  DTMB_FRONT_AGC_CONFIG2               DTMB_FRONT_ADDR(0x22)
#define  DTMB_FRONT_AGC_CONFIG3               DTMB_FRONT_ADDR(0x23)
#define  DTMB_FRONT_AGC_CONFIG4               DTMB_FRONT_ADDR(0x24)
#define  DTMB_FRONT_DDC_BYPASS                DTMB_FRONT_ADDR(0x25)
#define  DTMB_FRONT_DC_HOLD                   DTMB_FRONT_ADDR(0x28)
#define  DTMB_FRONT_DAGC_TARGET_POWER         DTMB_FRONT_ADDR(0x29)
#define  DTMB_FRONT_ACF_BYPASS                DTMB_FRONT_ADDR(0x2a)
#define  DTMB_FRONT_COEF_SET1                 DTMB_FRONT_ADDR(0x2b)
#define  DTMB_FRONT_COEF_SET2                 DTMB_FRONT_ADDR(0x2c)
#define  DTMB_FRONT_COEF_SET3                 DTMB_FRONT_ADDR(0x2d)
#define  DTMB_FRONT_COEF_SET4                 DTMB_FRONT_ADDR(0x2e)
#define  DTMB_FRONT_COEF_SET5                 DTMB_FRONT_ADDR(0x2f)
#define  DTMB_FRONT_COEF_SET6                 DTMB_FRONT_ADDR(0x30)
#define  DTMB_FRONT_COEF_SET7                 DTMB_FRONT_ADDR(0x31)
#define  DTMB_FRONT_COEF_SET8                 DTMB_FRONT_ADDR(0x32)
#define  DTMB_FRONT_COEF_SET9                 DTMB_FRONT_ADDR(0x33)
#define  DTMB_FRONT_COEF_SET10                DTMB_FRONT_ADDR(0x34)
#define  DTMB_FRONT_COEF_SET11                DTMB_FRONT_ADDR(0x35)
#define  DTMB_FRONT_COEF_SET12                DTMB_FRONT_ADDR(0x36)
#define  DTMB_FRONT_COEF_SET13                DTMB_FRONT_ADDR(0x37)
#define  DTMB_FRONT_COEF_SET14                DTMB_FRONT_ADDR(0x38)
#define  DTMB_FRONT_COEF_SET15                DTMB_FRONT_ADDR(0x39)
#define  DTMB_FRONT_COEF_SET16                DTMB_FRONT_ADDR(0x3a)
#define  DTMB_FRONT_COEF_SET17                DTMB_FRONT_ADDR(0x3b)
#define  DTMB_FRONT_COEF_SET18                DTMB_FRONT_ADDR(0x3c)
#define  DTMB_FRONT_COEF_SET19                DTMB_FRONT_ADDR(0x3d)
#define  DTMB_FRONT_SRC_CONFIG1               DTMB_FRONT_ADDR(0x3e)
#define  DTMB_FRONT_SRC_CONFIG2               DTMB_FRONT_ADDR(0x3f)
#define  DTMB_FRONT_SFIFO_OUT_LEN             DTMB_FRONT_ADDR(0x40)
#define  DTMB_FRONT_DAGC_GAIN                 DTMB_FRONT_ADDR(0x41)
#define  DTMB_FRONT_IQIB_STEP                 DTMB_FRONT_ADDR(0x42)
#define  DTMB_FRONT_IQIB_CONFIG               DTMB_FRONT_ADDR(0x43)
#define  DTMB_FRONT_ST_CONFIG                 DTMB_FRONT_ADDR(0x44)
#define  DTMB_FRONT_ST_FREQ                   DTMB_FRONT_ADDR(0x45)
#define  DTMB_FRONT_46_CONFIG                   DTMB_FRONT_ADDR(0x46)
#define  DTMB_FRONT_47_CONFIG                   DTMB_FRONT_ADDR(0x47)
#define  DTMB_FRONT_DEBUG_CFG                  DTMB_FRONT_ADDR(0x48)
#define  DTMB_FRONT_MEM_ADDR                  DTMB_FRONT_ADDR(0x49)
#define  DTMB_FRONT_19_CONFIG                   DTMB_FRONT_ADDR(0x19)
#define  DTMB_FRONT_4d_CONFIG                   DTMB_FRONT_ADDR(0x4d)

#endif
