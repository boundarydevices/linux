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

#include "osd_log.h"
#include "osd_backup.h"

static inline uint32_t osd_cbus_read(uint32_t reg)
{
	uint32_t ret = 0;

	ret = (uint32_t)aml_read_cbus(reg);
	osd_log_dbg3("%s(0x%x)=0x%x\n", __func__, reg, ret);

	return ret;
};

static inline void osd_cbus_write(uint32_t reg,
				   const uint32_t val)
{
	uint32_t ret = 0;

	aml_write_cbus(reg, val);
	ret = aml_read_cbus(reg);
	osd_log_dbg3("%s(0x%x, 0x%x)=0x%x\n", __func__, reg, val, ret);
};


static inline uint32_t osd_reg_read(uint32_t reg)
{
	uint32_t ret = 0;

	/* if (get_backup_reg(reg, &ret) != 0) */
	/* not read from bakcup */
	ret = (uint32_t)aml_read_vcbus(reg);
	osd_log_dbg3("%s(0x%x)=0x%x\n", __func__, reg, ret);

	return ret;
};

static inline void osd_reg_write(uint32_t reg,
				 const uint32_t val)
{
	aml_write_vcbus(reg, val);
	update_backup_reg(reg, val);
	osd_log_dbg3("%s(0x%x, 0x%x)\n", __func__, reg, val);
};

static inline void osd_reg_set_mask(uint32_t reg,
				    const uint32_t mask)
{
	osd_reg_write(reg, (osd_reg_read(reg) | (mask)));
}

static inline void osd_reg_clr_mask(uint32_t reg,
				    const uint32_t mask)
{
	osd_reg_write(reg, (osd_reg_read(reg) & (~(mask))));
}

static inline void osd_reg_set_bits(uint32_t reg,
				    const uint32_t value,
				    const uint32_t start,
				    const uint32_t len)
{
	osd_reg_write(reg, ((osd_reg_read(reg) &
			     ~(((1L << (len)) - 1) << (start))) |
			    (((value) & ((1L << (len)) - 1)) << (start))));
}

#ifdef CONFIG_AMLOGIC_MEDIA_FB_OSD_VSYNC_RDMA
u32 VSYNCOSD_RD_MPEG_REG(u32 reg);
int VSYNCOSD_WR_MPEG_REG(u32 reg, u32 val);
int VSYNCOSD_WR_MPEG_REG_BITS(u32 reg, u32 val, u32 start, u32 len);
int VSYNCOSD_SET_MPEG_REG_MASK(u32 reg, u32 mask);
int VSYNCOSD_CLR_MPEG_REG_MASK(u32 reg, u32 mask);

int VSYNCOSD_IRQ_WR_MPEG_REG(u32 reg, u32 val);
#else
#define VSYNCOSD_RD_MPEG_REG(reg) osd_reg_read(reg)
#define VSYNCOSD_WR_MPEG_REG(reg, val) osd_reg_write(reg, val)
#define VSYNCOSD_WR_MPEG_REG_BITS(reg, val, start, len) \
	osd_reg_set_bits(reg, val, start, len)
#define VSYNCOSD_SET_MPEG_REG_MASK(reg, mask) osd_reg_set_mask(reg, mask)
#define VSYNCOSD_CLR_MPEG_REG_MASK(reg, mask) osd_reg_clr_mask(reg, mask)

#define VSYNCOSD_IRQ_WR_MPEG_REG(reg, val) osd_reg_write(reg, val)
#endif

#endif
