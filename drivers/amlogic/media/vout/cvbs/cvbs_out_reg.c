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
#include <linux/amlogic/iomap.h>
#include "cvbs_out_reg.h"

/* ********************************
 * register access api
 * **********************************/
unsigned int cvbs_out_reg_read(unsigned int _reg)
{
	return aml_read_vcbus(_reg);
};

void cvbs_out_reg_write(unsigned int _reg, unsigned int _value)
{
	return aml_write_vcbus(_reg, _value);
};

void cvbs_out_reg_setb(unsigned int reg, unsigned int value,
		unsigned int _start, unsigned int _len)
{
	aml_write_vcbus(reg, ((aml_read_vcbus(reg) &
		(~(((1L << _len)-1) << _start))) |
		((value & ((1L << _len)-1)) << _start)));
}

unsigned int cvbs_out_reg_getb(unsigned int reg,
		unsigned int _start, unsigned int _len)
{
	return (aml_read_vcbus(reg) >> _start) & ((1L << _len)-1);
}

void cvbs_out_reg_set_mask(unsigned int reg, unsigned int _mask)
{
	aml_write_vcbus(reg, (aml_read_vcbus(reg) | (_mask)));
}

void cvbs_out_reg_clr_mask(unsigned int reg, unsigned int _mask)
{
	aml_write_vcbus(reg, (aml_read_vcbus(reg) & (~(_mask))));
}

unsigned int cvbs_out_hiu_read(unsigned int _reg)
{
	return aml_read_hiubus(_reg);
};

void cvbs_out_hiu_write(unsigned int _reg, unsigned int _value)
{
	return aml_write_hiubus(_reg, _value);
};

void cvbs_out_hiu_setb(unsigned int _reg, unsigned int _value,
		unsigned int _start, unsigned int _len)
{
	aml_write_hiubus(_reg, ((aml_read_hiubus(_reg) &
		(~(((1L << _len)-1) << _start))) |
		((_value & ((1L << _len)-1)) << _start)));
}

unsigned int cvbs_out_hiu_getb(unsigned int _reg,
		unsigned int _start, unsigned int _len)
{
	return (aml_read_hiubus(_reg) >> (_start)) & ((1L << (_len)) - 1);
}

void cvbs_out_hiu_set_mask(unsigned int _reg, unsigned int _mask)
{
	aml_write_hiubus(_reg, (aml_read_hiubus(_reg) | (_mask)));
}

void cvbs_out_hiu_clr_mask(unsigned int _reg, unsigned int _mask)
{
	aml_write_hiubus(_reg, (aml_read_hiubus(_reg) & (~(_mask))));
}

