/*
 * drivers/amlogic/media/dtv_demod/include/addr_atsc_cntr.h
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

#ifndef __ADDR_ATSC_CNTR_H__
#define __ADDR_ATSC_CNTR_H__

#include "addr_atsc_cntr_bit.h"

#define MOD_ATSC_CNTR 0x0
#define  ATSC_CNTR_ADDR(x) (MOD_ATSC_CNTR+(x))

#define  ATSC_CNTR_REG_0X20                  ATSC_CNTR_ADDR(0x20)
#define  ATSC_CNTR_REG_0X21                  ATSC_CNTR_ADDR(0x21)
#define  ATSC_CNTR_REG_0X22                  ATSC_CNTR_ADDR(0x22)
#define  ATSC_CNTR_REG_0X23                  ATSC_CNTR_ADDR(0x23)
#define  ATSC_CNTR_REG_0X24                  ATSC_CNTR_ADDR(0x24)
#define  ATSC_CNTR_REG_0X29                  ATSC_CNTR_ADDR(0x29)
#define  ATSC_CNTR_REG_0X2A                  ATSC_CNTR_ADDR(0x2a)
#define  ATSC_CNTR_REG_0X2B                  ATSC_CNTR_ADDR(0x2b)
#define  ATSC_CNTR_REG_0X2C                  ATSC_CNTR_ADDR(0x2c)
#define  ATSC_CNTR_REG_0X2D                  ATSC_CNTR_ADDR(0x2d)
#define  ATSC_CNTR_REG_0X2E                  ATSC_CNTR_ADDR(0x2e)

#endif
