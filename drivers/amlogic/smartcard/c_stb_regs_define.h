/*
 * drivers/amlogic/smartcard/c_stb_regs_define.h
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

#ifndef __MACH_MESON8_REG_ADDR_H_
#define __MACH_MESON8_REG_ADDR_H_
#include <linux/amlogic/iomap.h>
#define CBUS_REG_ADDR(_r) aml_read_cbus(_r)
#define SMARTCARD_REG0 0x2110
#define P_SMARTCARD_REG0                CBUS_REG_ADDR(SMARTCARD_REG0)
#define SMARTCARD_REG1 0x2111
#define P_SMARTCARD_REG1                CBUS_REG_ADDR(SMARTCARD_REG1)
#define SMARTCARD_REG2 0x2112
#define P_SMARTCARD_REG2                CBUS_REG_ADDR(SMARTCARD_REG2)
#define SMARTCARD_STATUS 0x2113
#define P_SMARTCARD_STATUS              CBUS_REG_ADDR(SMARTCARD_STATUS)
#define SMARTCARD_INTR 0x2114
#define P_SMARTCARD_INTR                CBUS_REG_ADDR(SMARTCARD_INTR)
#define SMARTCARD_REG5 0x2115
#define P_SMARTCARD_REG5                CBUS_REG_ADDR(SMARTCARD_REG5)
#define SMARTCARD_REG6 0x2116
#define P_SMARTCARD_REG6                CBUS_REG_ADDR(SMARTCARD_REG6)
#define SMARTCARD_FIFO 0x2117
#define P_SMARTCARD_FIFO                CBUS_REG_ADDR(SMARTCARD_FIFO)
#define SMARTCARD_REG8 0x2118
#define P_SMARTCARD_REG8                CBUS_REG_ADDR(SMARTCARD_REG8)
#endif
