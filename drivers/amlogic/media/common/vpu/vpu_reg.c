/*
 * drivers/amlogic/media/common/vpu/vpu_reg.c
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/amlogic/cpu_version.h>
#include "vpu_reg.h"
#include "vpu.h"

/* ********************************
 * register access api
 * *********************************
 */

#define VPU_MAP_HIUBUS    0
#define VPU_MAP_VCBUS     1

struct reg_map_s {
	unsigned int base_addr;
	unsigned int size;
	void __iomem *p;
	int flag;
};

static struct reg_map_s *vpu_map;
static int vpu_map_num;

static struct reg_map_s vpu_reg_maps[] = {
	{ /* HIU */
		.base_addr = 0xc883c000,
		.size = 0x400,
	},
	{ /* VCBUS */
		.base_addr = 0xd0100000,
		.size = 0xa000,
	},
};

int vpu_ioremap(void)
{
	int i;
	int ret = 0;

	vpu_map = vpu_reg_maps;
	vpu_map_num = ARRAY_SIZE(vpu_reg_maps);

	for (i = 0; i < vpu_map_num; i++) {
		vpu_map[i].p = ioremap(vpu_map[i].base_addr, vpu_map[i].size);
		if (vpu_map[i].p == NULL) {
			vpu_map[i].flag = 0;
			VPUERR("VPU reg map failed: 0x%x\n",
				vpu_map[i].base_addr);
			ret = -1;
		} else {
			vpu_map[i].flag = 1;
#if 0
			VPUPR("VPU reg mapped: 0x%x -> %p\n",
				vpu_map[i].base_addr, vpu_map[i].p);
#endif
		}
	}
	return ret;
}

static int vpu_ioremap_check(int n)
{
	if (vpu_map == NULL)
		return -1;
	if (n >= vpu_map_num)
		return -1;

	if (vpu_map[n].flag == 0) {
		VPUERR("reg 0x%x mapped error\n", vpu_map[n].base_addr);
		return -1;
	}
	return 0;
}

static void __iomem *vpu_hiu_reg_check(unsigned int _reg)
{
	void __iomem *p = NULL;
	int reg_bus;
	unsigned int reg_offset;

	reg_bus = VPU_MAP_HIUBUS;
	if (vpu_ioremap_check(reg_bus))
		return NULL;

	reg_offset = REG_OFFSET_HIU(_reg);
	if (reg_offset > vpu_map[reg_bus].size) {
		VPUERR("invalid reg offset: 0x%02x\n", _reg);
		return NULL;
	}
	p = vpu_map[reg_bus].p + reg_offset;

	return p;
}

static void __iomem *vpu_vcbus_reg_check(unsigned int _reg)
{
	void __iomem *p = NULL;
	int reg_bus;
	unsigned int reg_offset;

	reg_bus = VPU_MAP_VCBUS;
	if (vpu_ioremap_check(reg_bus))
		return NULL;

	reg_offset = REG_OFFSET_VCBUS(_reg);
	if (reg_offset > vpu_map[reg_bus].size) {
		VPUERR("invalid reg offset: 0x%04x\n", _reg);
		return NULL;
	}
	p = vpu_map[reg_bus].p + reg_offset;

	return p;
}

unsigned int vpu_hiu_read(unsigned int _reg)
{
	void __iomem *p;

	p = vpu_hiu_reg_check(_reg);
	if (p)
		return readl(p);
	else
		return 0;
};

void vpu_hiu_write(unsigned int _reg, unsigned int _value)
{
	void __iomem *p;

	p = vpu_hiu_reg_check(_reg);
	if (p)
		writel(_value, p);
};

void vpu_hiu_setb(unsigned int _reg, unsigned int _value,
		unsigned int _start, unsigned int _len)
{
	vpu_hiu_write(_reg, ((vpu_hiu_read(_reg) &
			~(((1L << (_len))-1) << (_start))) |
			(((_value)&((1L<<(_len))-1)) << (_start))));
}

unsigned int vpu_hiu_getb(unsigned int _reg,
		unsigned int _start, unsigned int _len)
{
	return (vpu_hiu_read(_reg) >> (_start)) & ((1L << (_len)) - 1);
}

void vpu_hiu_set_mask(unsigned int _reg, unsigned int _mask)
{
	vpu_hiu_write(_reg, (vpu_hiu_read(_reg) | (_mask)));
}

void vpu_hiu_clr_mask(unsigned int _reg, unsigned int _mask)
{
	vpu_hiu_write(_reg, (vpu_hiu_read(_reg) & (~(_mask))));
}

unsigned int vpu_vcbus_read(unsigned int _reg)
{
	void __iomem *p;

	p = vpu_vcbus_reg_check(_reg);
	if (p)
		return readl(p);
	else
		return 0;
};

void vpu_vcbus_write(unsigned int _reg, unsigned int _value)
{
	void __iomem *p;

	p = vpu_vcbus_reg_check(_reg);
	if (p)
		writel(_value, p);
};

void vpu_vcbus_setb(unsigned int _reg, unsigned int _value,
		unsigned int _start, unsigned int _len)
{
	vpu_vcbus_write(_reg, ((vpu_vcbus_read(_reg) &
			~(((1L << (_len))-1) << (_start))) |
			(((_value)&((1L<<(_len))-1)) << (_start))));
}

unsigned int vpu_vcbus_getb(unsigned int _reg,
		unsigned int _start, unsigned int _len)
{
	return (vpu_vcbus_read(_reg) >> (_start)) & ((1L << (_len)) - 1);
}

void vpu_vcbus_set_mask(unsigned int _reg, unsigned int _mask)
{
	vpu_vcbus_write(_reg, (vpu_vcbus_read(_reg) | (_mask)));
}

void vpu_vcbus_clr_mask(unsigned int _reg, unsigned int _mask)
{
	vpu_vcbus_write(_reg, (vpu_vcbus_read(_reg) & (~(_mask))));
}

