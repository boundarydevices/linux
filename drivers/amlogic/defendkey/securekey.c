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
#include <linux/arm-smccc.h>

#if (defined CONFIG_ARM64) || (defined CONFIG_ARM64_A32)

int aml_is_secure_set(void)
{
	int ret;

	/*AO_SEC_SD_CFG10: bit4 secure boot enable*/
	ret = (aml_read_aobus(0x228)>>4)&0x1;

	return ret;
}

long get_sharemem_info(unsigned long function_id)
{
	struct arm_smccc_res res;

	arm_smccc_smc((unsigned long)function_id, 0, 0, 0, 0, 0, 0, 0, &res);

	return res.a0;
}

unsigned long aml_sec_boot_check(unsigned long nType,
	unsigned long pBuffer,
	unsigned long nLength,
	unsigned long nOption)
{
	struct arm_smccc_res res;

	asm __volatile__("" : : : "memory");

	do {
		arm_smccc_smc((unsigned long)AML_DATA_PROCESS,
					(unsigned long)nType,
					(unsigned long)pBuffer,
					(unsigned long)nLength,
					(unsigned long)nOption,
					0, 0, 0, &res);
	} while (0);

	return res.a0;

}
#endif
