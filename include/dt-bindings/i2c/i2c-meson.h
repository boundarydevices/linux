/*
 * include/dt-bindings/i2c/i2c-meson.h
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

#ifndef _DT_BINDINGS_I2C_MESON_H
#define _DT_BINDINGS_I2C_MESON_H

/*MESON_I2C_MASTER0 - MESON_I2C_MASTERA
 *MESON_I2C_MASTER0 - MESON_I2C_MASTERB
 *MESON_I2C_MASTER0 - MESON_I2C_MASTERC
 *MESON_I2C_MASTER0 - MESON_I2C_MASTERD
 *we can see i2c-A/B/C/D rename to i2c0/1/2/3
 */

#define		MESON_I2C_MASTER0   0
#define		MESON_I2C_MASTER1   1
#define		MESON_I2C_MASTER2   2
#define		MESON_I2C_MASTER3   3
#define		MESON_I2C_MASTERAO  4

#endif
