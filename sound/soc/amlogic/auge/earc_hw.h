/*
 * sound/soc/amlogic/auge/earc_hw.h
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
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
#ifndef __EARC_HW_H__
#define __EARC_HW_H__

#include "regs.h"
#include "iomap.h"

extern void earcrx_cmdc_init(void);
extern void earcrx_dmac_init(void);
extern void earc_arc_init(void);
extern void earc_rx_enable(bool enable);
#endif
