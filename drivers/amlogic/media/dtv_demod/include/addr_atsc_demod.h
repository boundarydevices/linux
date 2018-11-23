/*
 * drivers/amlogic/media/dtv_demod/include/addr_atsc_demod.h
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

#ifndef __ADDR_ATSC_DEMOD_H__
#define __ADDR_ATSC_DEMOD_H__
#include "addr_atsc_demod_bit.h"

#define MOD_ATSC_DEMOD  0x0
#define  ATSC_DEMOD_ADDR(x) (MOD_ATSC_DEMOD+(x))

#define  ATSC_DEMOD_REG_0X52                  ATSC_DEMOD_ADDR(0x52)
#define  ATSC_DEMOD_REG_0X53                  ATSC_DEMOD_ADDR(0x53)
#define  ATSC_DEMOD_REG_0X54                  ATSC_DEMOD_ADDR(0x54)
#define  ATSC_DEMOD_REG_0X55                  ATSC_DEMOD_ADDR(0x55)
#define  ATSC_DEMOD_REG_0X56                  ATSC_DEMOD_ADDR(0x56)
#define  ATSC_DEMOD_REG_0X58                  ATSC_DEMOD_ADDR(0x58)
#define  ATSC_DEMOD_REG_0X59                  ATSC_DEMOD_ADDR(0x59)
#define  ATSC_DEMOD_REG_0X5A                  ATSC_DEMOD_ADDR(0x5a)
#define  ATSC_DEMOD_REG_0X5B                  ATSC_DEMOD_ADDR(0x5b)
#define  ATSC_DEMOD_REG_0X5C                  ATSC_DEMOD_ADDR(0x5c)
#define  ATSC_DEMOD_REG_0X5D                  ATSC_DEMOD_ADDR(0x5d)
#define  ATSC_DEMOD_REG_0X5E                  ATSC_DEMOD_ADDR(0x5e)
#define  ATSC_DEMOD_REG_0X5F                  ATSC_DEMOD_ADDR(0x5f)
#define  ATSC_DEMOD_REG_0X60                  ATSC_DEMOD_ADDR(0x60)
#define  ATSC_DEMOD_REG_0X61                  ATSC_DEMOD_ADDR(0x61)
#define  ATSC_DEMOD_REG_0X62                  ATSC_DEMOD_ADDR(0x62)
#define  ATSC_DEMOD_REG_0X63                  ATSC_DEMOD_ADDR(0x63)
#define  ATSC_DEMOD_REG_0X64                  ATSC_DEMOD_ADDR(0x64)
#define  ATSC_DEMOD_REG_0X65                  ATSC_DEMOD_ADDR(0x65)
#define  ATSC_DEMOD_REG_0X66                  ATSC_DEMOD_ADDR(0x66)
#define  ATSC_DEMOD_REG_0X67                  ATSC_DEMOD_ADDR(0x67)
#define  ATSC_DEMOD_REG_0X68                  ATSC_DEMOD_ADDR(0x68)
#define  ATSC_DEMOD_REG_0X69                  ATSC_DEMOD_ADDR(0x69)
#define  ATSC_DEMOD_REG_0X6A                  ATSC_DEMOD_ADDR(0x6a)
#define  ATSC_DEMOD_REG_0X6B                  ATSC_DEMOD_ADDR(0x6b)
#define  ATSC_DEMOD_REG_0X6C                  ATSC_DEMOD_ADDR(0x6c)
#define  ATSC_DEMOD_REG_0X6D                  ATSC_DEMOD_ADDR(0x6d)
#define  ATSC_DEMOD_REG_0X6E                  ATSC_DEMOD_ADDR(0x6e)
#define  ATSC_DEMOD_REG_0X70                  ATSC_DEMOD_ADDR(0x70)
#define  ATSC_DEMOD_REG_0X71                  ATSC_DEMOD_ADDR(0x71)
#define  ATSC_DEMOD_REG_0X72                  ATSC_DEMOD_ADDR(0x72)
#define  ATSC_DEMOD_REG_0X73                  ATSC_DEMOD_ADDR(0x73)
#define  ATSC_DEMOD_REG_0X74                  ATSC_DEMOD_ADDR(0x74)
#define  ATSC_DEMOD_REG_0X75                  ATSC_DEMOD_ADDR(0x75)
#define  ATSC_DEMOD_REG_0X76                  ATSC_DEMOD_ADDR(0x76)
#define  ATSC_DEMOD_REG_0X77                  ATSC_DEMOD_ADDR(0x77)
#define  ATSC_DEMOD_REG_0X78                  ATSC_DEMOD_ADDR(0x78)
#define  ATSC_DEMOD_REG_0X79                  ATSC_DEMOD_ADDR(0x79)
#define  ATSC_DEMOD_REG_0X7A                  ATSC_DEMOD_ADDR(0x7a)
#define  ATSC_DEMOD_REG_0X7B                  ATSC_DEMOD_ADDR(0x7b)
#define  ATSC_DEMOD_REG_0X7C                  ATSC_DEMOD_ADDR(0x7c)
#define  ATSC_DEMOD_REG_0X7D                  ATSC_DEMOD_ADDR(0x7d)
#define  ATSC_DEMOD_REG_0X7E                  ATSC_DEMOD_ADDR(0x7e)
#define  ATSC_DEMOD_REG_0X7F                  ATSC_DEMOD_ADDR(0x7f)
#define  ATSC_DEMOD_REG_0X80                  ATSC_DEMOD_ADDR(0x80)
#define  ATSC_DEMOD_REG_0X81                  ATSC_DEMOD_ADDR(0x81)
#define  ATSC_DEMOD_REG_0X82                  ATSC_DEMOD_ADDR(0x82)
#define  ATSC_DEMOD_REG_0X83                  ATSC_DEMOD_ADDR(0x83)
#define  ATSC_DEMOD_REG_0X84                  ATSC_DEMOD_ADDR(0x84)
#define  ATSC_DEMOD_REG_0X85                  ATSC_DEMOD_ADDR(0x85)
#define  ATSC_DEMOD_REG_0X86                  ATSC_DEMOD_ADDR(0x86)
#define  ATSC_DEMOD_REG_0X87                  ATSC_DEMOD_ADDR(0x87)
#define  ATSC_DEMOD_REG_0X88                  ATSC_DEMOD_ADDR(0x88)

#endif
