/*
 * include/linux/amlogic/jtag.h
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

#ifndef __INCLUDE_AMLOGIC_JTAG_H
#define __INCLUDE_AMLOGIC_JTAG_H

#include <linux/types.h>

#define AMLOGIC_JTAG_STATE_ON		0
#define AMLOGIC_JTAG_STATE_OFF		1

#define AMLOGIC_JTAG_DISABLE		(-1)
#define AMLOGIC_JTAG_APAO			2
#define AMLOGIC_JTAG_APEE			3

#define AMLOGIC_JTAG_ON			0x82000040
#define AMLOGIC_JTAG_OFF		0x82000041


#ifdef CONFIG_AMLOGIC_JTAG_MESON
extern bool is_jtag_disable(void);
extern bool is_jtag_apao(void);
extern bool is_jtag_apee(void);
#else
static inline bool is_jtag_disable(void) { return true; }
static inline bool is_jtag_apao(void) { return false; }
static inline bool is_jtag_apee(void) { return false; }
#endif

#endif
