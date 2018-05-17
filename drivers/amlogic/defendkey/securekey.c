/*
 * drivers/amlogic/defendkey/securekey.c
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

#include <linux/types.h>
#include <linux/slab.h>
/* #include <asm/compiler.h> */
#include <linux/amlogic/iomap.h>
#include "securekey.h"

#ifdef CONFIG_ARM64
#define __asmeq(x, y)  ".ifnc " x "," y " ; .err ; .endif\n\t"

int aml_is_secure_set(void)
{
	int ret;

	/*AO_SEC_SD_CFG10: bit4 secure boot enable*/
	ret = (aml_read_aobus(0x228)>>4)&0x1;

	return ret;
}

long get_sharemem_info(unsigned long function_id)
{
	asm volatile(
		__asmeq("%0", "x0")
		"smc    #0\n"
		: "+r" (function_id));

	return function_id;
}

unsigned long aml_sec_boot_check(unsigned long nType,
	unsigned long pBuffer,
	unsigned long nLength,
	unsigned long nOption)
{
	uint64_t ret = 1;

	register uint64_t x0 asm("x0");
	register uint64_t x1 asm("x1");
	register uint64_t x2 asm("x2");
	register uint64_t x3 asm("x3");
	register uint64_t x4 asm("x4");

	asm __volatile__("" : : : "memory");

	x0 = AML_DATA_PROCESS;
	x1 = nType;
	x2 = pBuffer;
	x3 = nLength;
	x4 = nOption;

	do {
		asm volatile(
			__asmeq("%0", "x0")
			__asmeq("%1", "x0")
			__asmeq("%2", "x1")
			__asmeq("%3", "x2")
			__asmeq("%4", "x3")
			__asmeq("%5", "x4")
		    "smc #0\n"
		    : "=r"(x0)
		    : "r"(x0), "r"(x1), "r"(x2), "r"(x3), "r"(x4));
	} while (0);

	ret = x0;

	return ret;

}
#endif
