/*
 * drivers/amlogic/media/osd/osd_io.h
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

#ifndef _OSD_IO_H_
#define _OSD_IO_H_
#include <linux/amlogic/iomap.h>
/* Local Headers */
#include "osd_log.h"

int osd_io_remap(int iomap);
uint32_t osd_cbus_read(uint32_t reg);
void osd_cbus_write(uint32_t reg,
				   const uint32_t val);
uint32_t osd_reg_read(uint32_t reg);
void osd_reg_write(uint32_t reg,
				 const uint32_t val);
void osd_reg_set_mask(uint32_t reg,
				    const uint32_t mask);
void osd_reg_clr_mask(uint32_t reg,
				    const uint32_t mask);
void osd_reg_set_bits(uint32_t reg,
				    const uint32_t value,
				    const uint32_t start,
				    const uint32_t len);

u32 VSYNCOSD_RD_MPEG_REG(u32 reg);
int VSYNCOSD_WR_MPEG_REG(u32 reg, u32 val);
int VSYNCOSD_WR_MPEG_REG_BITS(u32 reg, u32 val, u32 start, u32 len);
int VSYNCOSD_SET_MPEG_REG_MASK(u32 reg, u32 mask);
int VSYNCOSD_CLR_MPEG_REG_MASK(u32 reg, u32 mask);

int VSYNCOSD_IRQ_WR_MPEG_REG(u32 reg, u32 val);
#if 0
#define VSYNCOSD_RD_MPEG_REG(reg) osd_reg_read(reg)
#define VSYNCOSD_WR_MPEG_REG(reg, val) osd_reg_write(reg, val)
#define VSYNCOSD_WR_MPEG_REG_BITS(reg, val, start, len) \
	osd_reg_set_bits(reg, val, start, len)
#define VSYNCOSD_SET_MPEG_REG_MASK(reg, mask) osd_reg_set_mask(reg, mask)
#define VSYNCOSD_CLR_MPEG_REG_MASK(reg, mask) osd_reg_clr_mask(reg, mask)

#define VSYNCOSD_IRQ_WR_MPEG_REG(reg, val) osd_reg_write(reg, val)
#endif

#endif
