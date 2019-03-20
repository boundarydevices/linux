/*
 * drivers/amlogic/media/common/ge2d/ge2d_io.h
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

#ifndef _GE2D_IO_H_
#define _GE2D_IO_H_

#include <linux/io.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/registers/regs/ao_regs.h>

#include "ge2d_log.h"

#define GE2DBUS_REG_ADDR(reg) (((reg - 0x1800) << 2))
extern unsigned int ge2d_dump_reg_cnt;
extern unsigned int ge2d_dump_reg_enable;
extern void __iomem *ge2d_reg_map;

struct reg_map_s {
	unsigned int phy_addr;
	unsigned int size;
	void __iomem *vir_addr;
	int flag;
};

static struct reg_map_s reg_map = {
	.phy_addr = 0xd0160000,
	.size = 0x10000,
};

static int check_map_flag(unsigned int addr)
{
	int ret = 0;

	if (reg_map.flag)
		return 1;

	if (ge2d_reg_map) {
		reg_map.vir_addr = ge2d_reg_map;
		reg_map.flag = 1;
		ret = 1;
	} else {
		reg_map.vir_addr = ioremap(reg_map.phy_addr, reg_map.size);
		if (!reg_map.vir_addr) {
			pr_info("failed map phy: 0x%x\n", addr);
			ret = 0;
		} else {
			reg_map.flag = 1;
			ge2d_log_dbg("mapped phy: 0x%x\n", reg_map.phy_addr);
			ret = 1;
		}
	}
	return ret;
}

static uint32_t ge2d_reg_read(unsigned int reg)
{
	unsigned int addr = 0;
	unsigned int val = 0;

	if (get_cpu_type() < MESON_CPU_MAJOR_ID_GXBB)
		return (uint32_t)aml_read_cbus(reg);

	addr = GE2DBUS_REG_ADDR(reg);
	if (check_map_flag(addr))
		val = readl(reg_map.vir_addr + addr);

	ge2d_log_dbg2("read(0x%x)=0x%x\n", reg_map.phy_addr + addr, val);

	return val;
}

static void ge2d_reg_write(unsigned int reg, unsigned int val)
{
	unsigned int addr = 0;

	if (get_cpu_type() < MESON_CPU_MAJOR_ID_GXBB) {
		aml_write_cbus(reg, val);
		return;
	}

	addr = GE2DBUS_REG_ADDR(reg);
	if (check_map_flag(addr)) {
		writel(val, reg_map.vir_addr + addr);
		/* ret = readl(reg_map.vir_addr + addr); */
	}
	if (ge2d_dump_reg_enable && (ge2d_dump_reg_cnt > 0)) {
		ge2d_log_info("write(0x%x) = 0x%x\n",
				reg, val);
		ge2d_dump_reg_cnt--;
	}
}

static inline uint32_t ge2d_vcbus_read(uint32_t reg)
{
	return (uint32_t)aml_read_vcbus(reg);
};

static inline uint32_t ge2d_reg_get_bits(uint32_t reg,
		const uint32_t start,
		const uint32_t len)
{
	uint32_t val;

	val = (ge2d_reg_read(reg) >> (start)) & ((1L << (len)) - 1);
	return val;
}

static inline void ge2d_reg_set_bits(uint32_t reg,
				      const uint32_t value,
				      const uint32_t start,
				      const uint32_t len)
{
	ge2d_reg_write(reg, ((ge2d_reg_read(reg) &
			       ~(((1L << (len)) - 1) << (start))) |
			      (((value) & ((1L << (len)) - 1)) << (start))));
}

static void ge2d_hiu_setb(unsigned int _reg, unsigned int _value,
		unsigned int _start, unsigned int _len)
{
	aml_write_hiubus(_reg, ((aml_read_hiubus(_reg) &
			~(((1L << (_len))-1) << (_start))) |
			(((_value)&((1L<<(_len))-1)) << (_start))));
}

static void ge2d_ao_setb(unsigned int _reg, unsigned int _value,
		unsigned int _start, unsigned int _len)
{
	aml_write_aobus(_reg, ((aml_read_aobus(_reg) &
			~(((1L << (_len))-1) << (_start))) |
			(((_value)&((1L<<(_len))-1)) << (_start))));
}

static void ge2d_c_setb(unsigned int _reg, unsigned int _value,
		unsigned int _start, unsigned int _len)
{
	aml_write_cbus(_reg, ((aml_read_cbus(_reg) &
			~(((1L << (_len))-1) << (_start))) |
			(((_value)&((1L<<(_len))-1)) << (_start))));
}

static inline void ge2d_set_bus_bits(unsigned int bus_type,
			unsigned int reg, unsigned int val,
			unsigned int start, unsigned int len)
{
	switch (bus_type) {
	case CBUS_BASE:
		ge2d_c_setb(reg, val, start, len);
	break;
	case AOBUS_BASE:
		ge2d_ao_setb(reg, val, start, len);
	break;
	case HIUBUS_BASE:
		ge2d_hiu_setb(reg, val, start, len);
	break;
	default:
		ge2d_log_err("unsupported bus type\n");
	break;
	}
}
#endif
