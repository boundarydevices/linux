/*
 * drivers/amlogic/media/osd/osd_io.c
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

/* Linux Headers */
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/amlogic/iomap.h>
#include <linux/io.h>

/* Local Headers */
#include "osd_log.h"
#include "osd_backup.h"

#define OSDBUS_REG_ADDR(reg) (reg << 2)

struct reg_map_s {
	unsigned int phy_addr;
	unsigned int size;
	void __iomem *vir_addr;
	int flag;
};

static struct reg_map_s osd_reg_map = {
	.phy_addr = 0xff900000,
	.size = 0x10000,
};

int osd_io_remap(int iomap)
{
	int ret = 0;

	if (iomap) {
		if (osd_reg_map.flag)
			return 1;
		osd_reg_map.vir_addr =
			ioremap(osd_reg_map.phy_addr, osd_reg_map.size);
		if (!osd_reg_map.vir_addr) {
			pr_info("failed map phy: 0x%x\n", osd_reg_map.phy_addr);
			ret = 0;
		} else {
			osd_reg_map.flag = 1;
			pr_info("mapped phy: 0x%x\n", osd_reg_map.phy_addr);
			ret = 1;
		}
	} else
		ret = 1;
	return ret;
}

uint32_t osd_cbus_read(uint32_t reg)
{
	uint32_t ret = 0;
	unsigned int addr = 0;

	if (osd_reg_map.flag) {
		addr = OSDBUS_REG_ADDR(reg);
		ret = readl(osd_reg_map.vir_addr + addr);

	} else
		ret = (uint32_t)aml_read_cbus(reg);
	osd_log_dbg3(MODULE_BASE, "%s(0x%x)=0x%x\n", __func__, reg, ret);

	return ret;
};

void osd_cbus_write(uint32_t reg,
				   const uint32_t val)
{
	unsigned int addr = 0;

	if (osd_reg_map.flag) {
		addr = OSDBUS_REG_ADDR(reg);
		writel(val, osd_reg_map.vir_addr + addr);
	} else
		aml_write_cbus(reg, val);
	osd_log_dbg3(MODULE_BASE, "%s(0x%x, 0x%x)\n", __func__, reg, val);
};


uint32_t osd_reg_read(uint32_t reg)
{
	uint32_t ret = 0;
	unsigned int addr = 0;

	/* if (get_backup_reg(reg, &ret) != 0) */
	/* not read from bakcup */
	if (osd_reg_map.flag) {
		addr = OSDBUS_REG_ADDR(reg);
		ret = readl(osd_reg_map.vir_addr + addr);

	} else
		ret = (uint32_t)aml_read_vcbus(reg);
	osd_log_dbg3(MODULE_BASE, "%s(0x%x)=0x%x\n", __func__, reg, ret);

	return ret;
};

void osd_reg_write(uint32_t reg,
				 const uint32_t val)
{
	unsigned int addr = 0;

	if (osd_reg_map.flag) {
		addr = OSDBUS_REG_ADDR(reg);
		writel(val, osd_reg_map.vir_addr + addr);
	} else
		aml_write_vcbus(reg, val);
	update_backup_reg(reg, val);
	osd_log_dbg3(MODULE_BASE, "%s(0x%x, 0x%x)\n", __func__, reg, val);
};

void osd_reg_set_mask(uint32_t reg,
				    const uint32_t mask)
{
	osd_reg_write(reg, (osd_reg_read(reg) | (mask)));
}

void osd_reg_clr_mask(uint32_t reg,
				    const uint32_t mask)
{
	osd_reg_write(reg, (osd_reg_read(reg) & (~(mask))));
}

void osd_reg_set_bits(uint32_t reg,
				    const uint32_t value,
				    const uint32_t start,
				    const uint32_t len)
{
	osd_reg_write(reg, ((osd_reg_read(reg) &
			     ~(((1L << (len)) - 1) << (start))) |
			    (((value) & ((1L << (len)) - 1)) << (start))));
}

