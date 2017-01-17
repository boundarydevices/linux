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
#include <linux/amlogic/media/old_cpu_version.h>
#include "vpu_reg.h"
#include "vpu.h"

/* ********************************
 * register access api
 * *********************************
 */

struct reg_map_s {
	unsigned int base_addr;
	unsigned int size;
	void __iomem *p;
	int flag;
};

static struct reg_map_s vpu_reg_maps[] = {
	{ /* CBUS */
		.base_addr = 0xc1100000,
		.size = 0x10000,
	},
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

	for (i = 0; i < ARRAY_SIZE(vpu_reg_maps); i++) {
		vpu_reg_maps[i].p = ioremap(vpu_reg_maps[i].base_addr,
					vpu_reg_maps[i].size);
		if (vpu_reg_maps[i].p == NULL) {
			vpu_reg_maps[i].flag = 0;
			VPUERR("VPU reg map failed: 0x%x\n",
				vpu_reg_maps[i].base_addr);
			ret = -1;
		} else {
			vpu_reg_maps[i].flag = 1;
#if 0
			VPUPR("VPU reg mapped: 0x%x -> %p\n",
				vpu_reg_maps[i].base_addr,
				vpu_reg_maps[i].p);
#endif
		}
	}
	return ret;
}

unsigned int vpu_hiu_read(unsigned int _reg)
{
	void __iomem *p;

	p = vpu_reg_maps[1].p + REG_OFFSET_HIU(_reg);
	return readl(p);
};

void vpu_hiu_write(unsigned int _reg, unsigned int _value)
{
	void __iomem *p;

	p = vpu_reg_maps[1].p + REG_OFFSET_HIU(_reg);
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

unsigned int vpu_cbus_read(unsigned int _reg)
{
	void __iomem *p;

	p = vpu_reg_maps[0].p + REG_OFFSET_CBUS(_reg);
	return readl(p);
};

void vpu_cbus_write(unsigned int _reg, unsigned int _value)
{
	void __iomem *p;

	p = vpu_reg_maps[0].p + REG_OFFSET_CBUS(_reg);
	writel(_value, p);
};

void vpu_cbus_setb(unsigned int _reg, unsigned int _value,
		unsigned int _start, unsigned int _len)
{
	vpu_cbus_write(_reg, ((vpu_cbus_read(_reg) &
			~(((1L << (_len))-1) << (_start))) |
			(((_value)&((1L<<(_len))-1)) << (_start))));
}

unsigned int vpu_cbus_getb(unsigned int _reg,
		unsigned int _start, unsigned int _len)
{
	return (vpu_cbus_read(_reg) >> (_start)) & ((1L << (_len)) - 1);
}

void vpu_cbus_set_mask(unsigned int _reg, unsigned int _mask)
{
	vpu_cbus_write(_reg, (vpu_cbus_read(_reg) | (_mask)));
}

void vpu_cbus_clr_mask(unsigned int _reg, unsigned int _mask)
{
	vpu_cbus_write(_reg, (vpu_cbus_read(_reg) & (~(_mask))));
}

unsigned int vpu_vcbus_read(unsigned int _reg)
{
	void __iomem *p;

	p = vpu_reg_maps[2].p + REG_OFFSET_VCBUS(_reg);
	return readl(p);
};

void vpu_vcbus_write(unsigned int _reg, unsigned int _value)
{
	void __iomem *p;

	p = vpu_reg_maps[2].p + REG_OFFSET_VCBUS(_reg);
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

