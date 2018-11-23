/*
 * drivers/amlogic/media/dtv_demod/include/addr_atsc_eq.h
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

#ifndef __ADDR_ATSC_EQ_H__
#define __ADDR_ATSC_EQ_H__
#include "addr_atsc_eq_bit.h"

#define MOD_ATSC_EQ 0x0
#define  ATSC_EQ_ADDR(x) (MOD_ATSC_EQ+(x))

#define  ATSC_EQ_REG_0X90                  ATSC_EQ_ADDR(0x90)
#define  ATSC_EQ_REG_0X91                  ATSC_EQ_ADDR(0x91)
#define  ATSC_EQ_REG_0X92                  ATSC_EQ_ADDR(0x92)
#define  ATSC_EQ_REG_0X93                  ATSC_EQ_ADDR(0x93)
#define  ATSC_EQ_REG_0X94                  ATSC_EQ_ADDR(0x94)
#define  ATSC_EQ_REG_0X95                  ATSC_EQ_ADDR(0x95)
#define  ATSC_EQ_REG_0X96                  ATSC_EQ_ADDR(0x96)
#define  ATSC_EQ_REG_0X97                  ATSC_EQ_ADDR(0x97)
#define  ATSC_EQ_REG_0X98                  ATSC_EQ_ADDR(0x98)
#define  ATSC_EQ_REG_0X99                  ATSC_EQ_ADDR(0x99)
#define  ATSC_EQ_REG_0X9A                  ATSC_EQ_ADDR(0x9a)
#define  ATSC_EQ_REG_0X9B                  ATSC_EQ_ADDR(0x9b)
#define  ATSC_EQ_REG_0X9C                  ATSC_EQ_ADDR(0x9c)
#define  ATSC_EQ_REG_0X9D                  ATSC_EQ_ADDR(0x9d)
#define  ATSC_EQ_REG_0X9E                  ATSC_EQ_ADDR(0x9e)
#define  ATSC_EQ_REG_0X9F                  ATSC_EQ_ADDR(0x9f)
#define  ATSC_EQ_REG_0XA0                  ATSC_EQ_ADDR(0xa0)
#define  ATSC_EQ_REG_0XA1                  ATSC_EQ_ADDR(0xa1)
#define  ATSC_EQ_REG_0XA2                  ATSC_EQ_ADDR(0xa2)
#define  ATSC_EQ_REG_0XA3                  ATSC_EQ_ADDR(0xa3)
#define  ATSC_EQ_REG_0XA4                  ATSC_EQ_ADDR(0xa4)
#define  ATSC_EQ_REG_0XA5                  ATSC_EQ_ADDR(0xa5)
#define  ATSC_EQ_REG_0XA6                  ATSC_EQ_ADDR(0xa6)
#define  ATSC_EQ_REG_0XA7                  ATSC_EQ_ADDR(0xa7)
#define  ATSC_EQ_REG_0XA8                  ATSC_EQ_ADDR(0xa8)
#define  ATSC_EQ_REG_0XA9                  ATSC_EQ_ADDR(0xa9)
#define  ATSC_EQ_REG_0XAA                  ATSC_EQ_ADDR(0xaa)
#define  ATSC_EQ_REG_0XAB                  ATSC_EQ_ADDR(0xab)
#define  ATSC_EQ_REG_0XAC                  ATSC_EQ_ADDR(0xac)
#define  ATSC_EQ_REG_0XAD                  ATSC_EQ_ADDR(0xad)
#define  ATSC_EQ_REG_0XAE                  ATSC_EQ_ADDR(0xae)
#define  ATSC_EQ_REG_0XAF                  ATSC_EQ_ADDR(0xaf)
#define  ATSC_EQ_REG_0XB0                  ATSC_EQ_ADDR(0xb0)
#define  ATSC_EQ_REG_0XB1                  ATSC_EQ_ADDR(0xb1)
#define  ATSC_EQ_REG_0XB2                  ATSC_EQ_ADDR(0xb2)
#define  ATSC_EQ_REG_0XB3                  ATSC_EQ_ADDR(0xb3)
#define  ATSC_EQ_REG_0XB4                  ATSC_EQ_ADDR(0xb4)
#define  ATSC_EQ_REG_0XB5                  ATSC_EQ_ADDR(0xb5)
#define  ATSC_EQ_REG_0XB6                  ATSC_EQ_ADDR(0xb6)
#define  ATSC_EQ_REG_0XB7                  ATSC_EQ_ADDR(0xb7)
#define  ATSC_EQ_REG_0XB8                  ATSC_EQ_ADDR(0xb8)
#define  ATSC_EQ_REG_0XB9                  ATSC_EQ_ADDR(0xb9)
#define  ATSC_EQ_REG_0XBA                  ATSC_EQ_ADDR(0xba)
#define  ATSC_EQ_REG_0XBB                  ATSC_EQ_ADDR(0xbb)
#define  ATSC_EQ_REG_0XBC                  ATSC_EQ_ADDR(0xbc)
#define  ATSC_EQ_REG_0XBD                  ATSC_EQ_ADDR(0xbd)
#define  ATSC_EQ_REG_0XBE                  ATSC_EQ_ADDR(0xbe)
#define  ATSC_EQ_REG_0XBF                  ATSC_EQ_ADDR(0xbf)
#define  ATSC_EQ_REG_0XC0                  ATSC_EQ_ADDR(0xc0)
#define  ATSC_EQ_REG_0XC1                  ATSC_EQ_ADDR(0xc1)
#define  ATSC_EQ_REG_0XC2                  ATSC_EQ_ADDR(0xc2)
#define  ATSC_EQ_REG_0XC3                  ATSC_EQ_ADDR(0xc3)
#define  ATSC_EQ_REG_0XC4                  ATSC_EQ_ADDR(0xc4)
#define  ATSC_EQ_REG_0XC5                  ATSC_EQ_ADDR(0xc5)
#define  ATSC_EQ_REG_0XC6                  ATSC_EQ_ADDR(0xc6)
#define  ATSC_EQ_REG_0XC7                  ATSC_EQ_ADDR(0xc7)
#define  ATSC_EQ_REG_0XC8                  ATSC_EQ_ADDR(0xc8)
#define  ATSC_EQ_REG_0XC9                  ATSC_EQ_ADDR(0xc9)
#define  ATSC_EQ_REG_0XCA                  ATSC_EQ_ADDR(0xca)
#define  ATSC_EQ_REG_0XCB                  ATSC_EQ_ADDR(0xcb)
#define  ATSC_EQ_REG_0XCC                  ATSC_EQ_ADDR(0xcc)
#define  ATSC_EQ_REG_0XCD                  ATSC_EQ_ADDR(0xcd)
#define  ATSC_EQ_REG_0XD0                  ATSC_EQ_ADDR(0xd0)
#define  ATSC_EQ_REG_0XD1                  ATSC_EQ_ADDR(0xd1)
#define  ATSC_EQ_REG_0XD2                  ATSC_EQ_ADDR(0xd2)
#define  ATSC_EQ_REG_0XD3                  ATSC_EQ_ADDR(0xd3)
#define  ATSC_EQ_REG_0XD4                  ATSC_EQ_ADDR(0xd4)
#define  ATSC_EQ_REG_0XD5                  ATSC_EQ_ADDR(0xd5)
#define  ATSC_EQ_REG_0XD6                  ATSC_EQ_ADDR(0xd6)
#define  ATSC_EQ_REG_0XD7                  ATSC_EQ_ADDR(0xd7)
#define  ATSC_EQ_REG_0XD8                  ATSC_EQ_ADDR(0xd8)
#define  ATSC_EQ_REG_0XD9                  ATSC_EQ_ADDR(0xd9)

#endif
