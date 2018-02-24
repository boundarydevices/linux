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
#include <linux/amlogic/iomap.h>
#include "vpu_reg.h"
#include "vpu.h"

/* ********************************
 * register access api
 * *********************************
 */

unsigned int vpu_hiu_read(unsigned int _reg)
{
	return aml_read_hiubus(_reg);
};

void vpu_hiu_write(unsigned int _reg, unsigned int _value)
{
	aml_write_hiubus(_reg, _value);
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
	return aml_read_vcbus(_reg);
};

void vpu_vcbus_write(unsigned int _reg, unsigned int _value)
{
	aml_write_vcbus(_reg, _value);
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

unsigned int vpu_ao_read(unsigned int _reg)
{
	return aml_read_aobus(_reg);
};

void vpu_ao_write(unsigned int _reg, unsigned int _value)
{
	aml_write_aobus(_reg, _value);
};

void vpu_ao_setb(unsigned int _reg, unsigned int _value,
		unsigned int _start, unsigned int _len)
{
	vpu_ao_write(_reg, ((vpu_ao_read(_reg) &
			~(((1L << (_len))-1) << (_start))) |
			(((_value)&((1L<<(_len))-1)) << (_start))));
}

unsigned int vpu_cbus_read(unsigned int _reg)
{
	return aml_read_cbus(_reg);
};

void vpu_cbus_write(unsigned int _reg, unsigned int _value)
{
	aml_write_cbus(_reg, _value);
};

void vpu_cbus_setb(unsigned int _reg, unsigned int _value,
		unsigned int _start, unsigned int _len)
{
	vpu_cbus_write(_reg, ((vpu_cbus_read(_reg) &
			~(((1L << (_len))-1) << (_start))) |
			(((_value)&((1L<<(_len))-1)) << (_start))));
}

void vpu_cbus_set_mask(unsigned int _reg, unsigned int _mask)
{
	vpu_cbus_write(_reg, (vpu_cbus_read(_reg) | (_mask)));
}

void vpu_cbus_clr_mask(unsigned int _reg, unsigned int _mask)
{
	vpu_cbus_write(_reg, (vpu_cbus_read(_reg) & (~(_mask))));
}

