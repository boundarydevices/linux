/*
 * drivers/amlogic/media/vout/hdmitx/hdmi_tx_20/hw/txlx_reg.h
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

#ifndef __TXLX_REG_H__
#define __TXLX_REG_H__
#include "reg_ops.h"

#define RESET0_REGISTER 0x01
#define P_RESET0_REGISTER RESET_CBUS_REG_ADDR(RESET0_REGISTER)
#define RESET2_REGISTER 0x03
#define P_RESET2_REGISTER RESET_CBUS_REG_ADDR(RESET2_REGISTER)

#define ISA_DEBUG_REG0 0x3c00
#define P_ISA_DEBUG_REG0 CBUS_REG_ADDR(ISA_DEBUG_REG0)

#endif
