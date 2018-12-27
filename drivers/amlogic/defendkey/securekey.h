/*
 * drivers/amlogic/defendkey/securekey.h
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

#if (defined CONFIG_ARM64) || (defined CONFIG_ARM64_A32)
#define AML_D_P_UPGRADE_CHECK   (0x80)
#define AML_D_P_IMG_DECRYPT     (0x40)
#define AML_D_Q_IMG_SIG_HDR_SIZE	 (0x100)
#define AML_DATA_PROCESS		(0x820000FF)
#define GET_SHARE_MEM_INPUT_BASE	0x82000020

long get_sharemem_info(unsigned long function_id);

int aml_is_secure_set(void);
unsigned long aml_sec_boot_check(unsigned long nType,
	unsigned long pBuffer,
	unsigned long nLength,
	unsigned long nOption);
#endif
