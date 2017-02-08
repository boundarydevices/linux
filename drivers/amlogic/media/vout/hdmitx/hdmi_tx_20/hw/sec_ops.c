/*
 * drivers/amlogic/media/vout/hdmitx/hdmi_tx_20/hw/sec_ops.c
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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include "mach_reg.h"

#define __asmeq(x, y)  ".ifnc " x "," y " ; .err ; .endif\n\t"

void sec_reg_write(unsigned int *addr, unsigned int value)
{
	register long x0 asm("x0") = 0x82000019;
	register long x1 asm("x1") = (unsigned long)addr;
	register long x2 asm("x2") = value;
	asm volatile(
		__asmeq("%0", "x0")
		__asmeq("%1", "x1")
		__asmeq("%2", "x2")
		"smc #0\n"
		: : "r"(x0), "r"(x1), "r"(x2)
	);
}

unsigned int sec_reg_read(unsigned int *addr)
{
	register long x0 asm("x0") = 0x82000018;
	register long x1 asm("x1") = (unsigned long)addr;
	asm volatile(
		__asmeq("%0", "x0")
		__asmeq("%1", "x1")
		"smc #0\n"
		: "+r"(x0) : "r"(x1)
	);
	return (unsigned int)(x0&0xffffffff);
}
