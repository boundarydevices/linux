/*
 * drivers/amlogic/media/vin/tvin/hdmirx_ext/hdmirx_ext_reg.h
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

#ifndef __AML_HDMIRX_EXT_REG_H__
#define __AML_HDMIRX_EXT_REG_H__
#include <linux/amlogic/iomap.h>

#define VDIN_ASFIFO_CTRL2                    0x120f
#define DVIN_FRONT_END_CTRL                  0x12e0
#define DVIN_HS_LEAD_VS_ODD                  0x12e1
#define DVIN_ACTIVE_START_PIX                0x12e2
#define DVIN_ACTIVE_START_LINE               0x12e3
#define DVIN_DISPLAY_SIZE                    0x12e4
#define DVIN_CTRL_STAT                       0x12e5


static inline unsigned int rx_ext_vcbus_read(unsigned int reg)
{
	return aml_read_vcbus(reg);
};

static inline void rx_ext_vcbus_write(unsigned int reg, unsigned int value)
{
	aml_write_vcbus(reg, value);
};

static inline void rx_ext_vcbus_setb(unsigned int reg, unsigned int value,
		unsigned int _start, unsigned int _len)
{
	rx_ext_vcbus_write(reg, ((rx_ext_vcbus_read(reg) &
				(~(((1L << _len)-1) << _start))) |
				((value & ((1L << _len)-1)) << _start)));
}

#endif

