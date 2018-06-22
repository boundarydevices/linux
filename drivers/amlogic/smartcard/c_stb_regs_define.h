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
#define SMARTCARD_REG_BASE	smc_get_reg_base()
#define SMARTCARD_REG0 (SMARTCARD_REG_BASE + 0x0)
#define P_SMARTCARD_REG0                CBUS_REG_ADDR(SMARTCARD_REG0)
#define SMARTCARD_REG1 (SMARTCARD_REG_BASE + 0x1)
#define P_SMARTCARD_REG1                CBUS_REG_ADDR(SMARTCARD_REG1)
#define SMARTCARD_REG2 (SMARTCARD_REG_BASE + 0x2)
#define P_SMARTCARD_REG2                CBUS_REG_ADDR(SMARTCARD_REG2)
#define SMARTCARD_STATUS (SMARTCARD_REG_BASE + 0x3)
#define P_SMARTCARD_STATUS              CBUS_REG_ADDR(SMARTCARD_STATUS)
#define SMARTCARD_INTR (SMARTCARD_REG_BASE + 0x4)
#define P_SMARTCARD_INTR                CBUS_REG_ADDR(SMARTCARD_INTR)
#define SMARTCARD_REG5 (SMARTCARD_REG_BASE + 0x5)
#define P_SMARTCARD_REG5                CBUS_REG_ADDR(SMARTCARD_REG5)
#define SMARTCARD_REG6 (SMARTCARD_REG_BASE + 0x6)
#define P_SMARTCARD_REG6                CBUS_REG_ADDR(SMARTCARD_REG6)
#define SMARTCARD_FIFO (SMARTCARD_REG_BASE + 0x7)
#define P_SMARTCARD_FIFO                CBUS_REG_ADDR(SMARTCARD_FIFO)
#define SMARTCARD_REG8 (SMARTCARD_REG_BASE + 0x8)
#define P_SMARTCARD_REG8                CBUS_REG_ADDR(SMARTCARD_REG8)
#endif
