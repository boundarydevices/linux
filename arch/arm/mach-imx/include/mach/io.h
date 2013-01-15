/*
 * arch/arm/mach-imx/include/mach/io.h
 *
 * Copyright (C) 2013 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __MACH_IMX_IO_H
#define __MACH_IMX_IO_H

#define IO_SPACE_LIMIT ((resource_size_t)0x010fffff)

#define __io(a)		__typesafe_io((a) & IO_SPACE_LIMIT)

#endif
