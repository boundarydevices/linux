/*
 * drivers/amlogic/media/vout/cvbs/cvbs_out_reg.c
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
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/amlogic/cpu_version.h>
#include "cvbs_out_reg.h"

struct reg_map_s {
	unsigned int base_addr;
	unsigned int size;
	void __iomem *p;
	int flag;
};

#define CVBS_OUT_MAP_VCBUS     0
#define CVBS_OUT_MAP_HIUBUS    1
static struct reg_map_s cvbs_out_reg_maps[] = {
	{ /* VCBUS */
		.base_addr = 0xd0100000,
		.size = 0x10000,
		.flag = 0,
	},
	{ /* HIU */
		.base_addr = 0xc883c000,
		.size = 0x400,
		.flag = 0,
	},
};

int cvbs_out_ioremap(void)
{
	int i;
	int ret = 0;

	for (i = 0; i < 2; i++) {
		cvbs_out_reg_maps[i].p = ioremap(cvbs_out_reg_maps[i].base_addr,
					cvbs_out_reg_maps[i].size);
		if (cvbs_out_reg_maps[i].p == NULL) {
			cvbs_out_reg_maps[i].flag = 0;
			pr_info("cvbs_out reg map failed: 0x%x\n",
				cvbs_out_reg_maps[i].base_addr);
			ret = -1;
		} else {
			cvbs_out_reg_maps[i].flag = 1;
#if 0
			pr_info("cvbs_out reg mapped: 0x%x -> %p\n",
			cvbs_out_reg_maps[i].base_addr, cvbs_out_reg_maps[i].p);
#endif
		}
	}
	return ret;
}

static int check_cvbs_out_ioremap(int n)
{
	if (cvbs_out_reg_maps[n].flag == 0) {
		pr_info("cvbs_out reg 0x%x mapped error\n",
			cvbs_out_reg_maps[n].base_addr);
		return -1;
	}
	return 0;
}

/* ********************************
 * register access api
 * **********************************/
static inline void __iomem *check_cvbs_out_reg(unsigned int _reg)
{
	void __iomem *p;
	int reg_bus;

	reg_bus = CVBS_OUT_MAP_VCBUS;
	if (check_cvbs_out_ioremap(reg_bus))
		return NULL;

	p = cvbs_out_reg_maps[reg_bus].p +
		CVBS_OUT_REG_OFFSET_VCBUS(_reg);
	return p;
}

unsigned int cvbs_out_reg_read(unsigned int _reg)
{
	void __iomem *p;

	p = check_cvbs_out_reg(_reg);
	if (p)
		return readl(p);
	else
		return -1;
};

void cvbs_out_reg_write(unsigned int _reg, unsigned int _value)
{
	void __iomem *p;

	p = check_cvbs_out_reg(_reg);
	if (p)
		writel(_value, p);
};

void cvbs_out_reg_setb(unsigned int reg, unsigned int value,
		unsigned int _start, unsigned int _len)
{
	cvbs_out_reg_write(reg, ((cvbs_out_reg_read(reg) &
		(~(((1L << _len)-1) << _start))) |
		((value & ((1L << _len)-1)) << _start)));
}

unsigned int cvbs_out_reg_getb(unsigned int reg,
		unsigned int _start, unsigned int _len)
{
	return (cvbs_out_reg_read(reg) >> _start) & ((1L << _len)-1);
}

void cvbs_out_reg_set_mask(unsigned int reg, unsigned int _mask)
{
	cvbs_out_reg_write(reg, (cvbs_out_reg_read(reg) | (_mask)));
}

void cvbs_out_reg_clr_mask(unsigned int reg, unsigned int _mask)
{
	cvbs_out_reg_write(reg, (cvbs_out_reg_read(reg) & (~(_mask))));
}

/* hiu_bus */
static inline void __iomem *check_cvbs_out_hiu_reg(unsigned int _reg)
{
	void __iomem *p;
	int reg_bus;

	/*(get_cpu_type() >= MESON_CPU_MAJOR_ID_GXBB)*/
	reg_bus = CVBS_OUT_MAP_HIUBUS;
	if (check_cvbs_out_ioremap(reg_bus))
		return NULL;

	_reg = CVBS_OUT_HIU_REG_GX(_reg);
	p = cvbs_out_reg_maps[reg_bus].p +
			CVBS_OUT_REG_OFFSET_HIU(_reg);
	return p;
}

unsigned int cvbs_out_hiu_read(unsigned int _reg)
{
	void __iomem *p;

	p = check_cvbs_out_hiu_reg(_reg);
	if (p)
		return readl(p);
	else
		return -1;
};

void cvbs_out_hiu_write(unsigned int _reg, unsigned int _value)
{
	void __iomem *p;

	p = check_cvbs_out_hiu_reg(_reg);
	if (p)
		writel(_value, p);
};

void cvbs_out_hiu_setb(unsigned int _reg, unsigned int _value,
		unsigned int _start, unsigned int _len)
{
	cvbs_out_hiu_write(_reg, ((cvbs_out_hiu_read(_reg) &
		(~(((1L << _len)-1) << _start))) |
		((_value & ((1L << _len)-1)) << _start)));
}

unsigned int cvbs_out_hiu_getb(unsigned int _reg,
		unsigned int _start, unsigned int _len)
{
	return (cvbs_out_hiu_read(_reg) >> (_start)) & ((1L << (_len)) - 1);
}

void cvbs_out_hiu_set_mask(unsigned int _reg, unsigned int _mask)
{
	cvbs_out_hiu_write(_reg, (cvbs_out_hiu_read(_reg) | (_mask)));
}

void cvbs_out_hiu_clr_mask(unsigned int _reg, unsigned int _mask)
{
	cvbs_out_hiu_write(_reg, (cvbs_out_hiu_read(_reg) & (~(_mask))));
}

