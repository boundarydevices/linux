/*
 * drivers/amlogic/clk/sm1/sm1.h
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

#ifndef __SM1_H
#define __SM1_H

/*
 * Clock controller register offsets
 *
 * Register offsets from the data sheet are listed in comment blocks below.
 * Those offsets must be multiplied by 4 before adding them to the base address
 * to get the right value
 */

#define HHI_GP1_PLL_CNTL0		0x60 /* 0x18 offset in data sheet */
#define HHI_SYS_CPU_CLK_CNTL5		0x21C /* 0x87 offset in data sheet */
#define HHI_SYS_CPU_CLK_CNTL6		0x220 /* 0x88 offset in data sheet */

#endif /* __SM1_H */
