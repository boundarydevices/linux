/*
 * drivers/amlogic/efuse/efuse_regs.h
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

#ifndef __EFUSE_REG_H
#define __EFUSE_REG_H

#include <linux/io.h>

#define	M8_EFUSE_VERSION_OFFSET			509
#define	M8_EFUSE_VERSION_DATA_LEN		1
#define M8_EFUSE_VERSION_SERIALNUM_V1	20

enum efuse_socchip_type_e {
	EFUSE_SOC_CHIP_M8BABY = 1,
	EFUSE_SOC_CHIP_UNKNOWN,
};

extern void __iomem *efuse_base;

#define P_EFUSE_CNTL0 (unsigned int)(efuse_base + 0x4*0)
#define P_EFUSE_CNTL1 (unsigned int)(efuse_base + 0x4*1)
#define P_EFUSE_CNTL2 (unsigned int)(efuse_base + 0x4*2)
#define P_EFUSE_CNTL3 (unsigned int)(efuse_base + 0x4*3)
#define P_EFUSE_CNTL4 (unsigned int)(efuse_base + 0x4*4)

/* efuse register define*/
/* EFUSE_CNTL0 */

/*
 * EFUSE_CNTL1
 * bit[31-27]
 * bit[26]     AUTO_RD_BUSY
 * bit[25]     AUTO_RD_START
 * bit[24]     AUTO_RD_ENABLE
 * bit[23-16]  BYTE_WR_DATA
 * bit[15]
 * bit[14]     AUTO_WR_BUSY
 * bit[13]     AUTO_WR_START
 * bit[12]     AUTO_WR_ENABLE
 * bit[11]     BYTE_ADDR_SET
 * bit[10]
 * bit[9-0]    BYTE_ADDR
 */
#define CNTL1_PD_ENABLE_BIT					27
#define CNTL1_PD_ENABLE_SIZE					1
#define CNTL1_PD_ENABLE_ON					1
#define CNTL1_PD_ENABLE_OFF				0

#define CNTL1_AUTO_RD_BUSY_BIT              26
#define CNTL1_AUTO_RD_BUSY_SIZE             1

#define CNTL1_AUTO_RD_START_BIT             25
#define CNTL1_AUTO_RD_START_SIZE            1

#define CNTL1_AUTO_RD_ENABLE_BIT            24
#define CNTL1_AUTO_RD_ENABLE_SIZE           1
#define CNTL1_AUTO_RD_ENABLE_ON             1
#define CNTL1_AUTO_RD_ENABLE_OFF            0

#define CNTL1_BYTE_WR_DATA_BIT              16
#define CNTL1_BYTE_WR_DATA_SIZE             8

#define CNTL1_AUTO_WR_BUSY_BIT              14
#define CNTL1_AUTO_WR_BUSY_SIZE             1

#define CNTL1_AUTO_WR_START_BIT             13
#define CNTL1_AUTO_WR_START_SIZE            1
#define CNTL1_AUTO_WR_START_ON              1
#define CNTL1_AUTO_WR_START_OFF             0

#define CNTL1_AUTO_WR_ENABLE_BIT            12
#define CNTL1_AUTO_WR_ENABLE_SIZE           1
#define CNTL1_AUTO_WR_ENABLE_ON             1
#define CNTL1_AUTO_WR_ENABLE_OFF            0

#define CNTL1_BYTE_ADDR_SET_BIT             11
#define CNTL1_BYTE_ADDR_SET_SIZE            1
#define CNTL1_BYTE_ADDR_SET_ON              1
#define CNTL1_BYTE_ADDR_SET_OFF             0

#define CNTL1_BYTE_ADDR_BIT                 0
#define CNTL1_BYTE_ADDR_SIZE                10

/* EFUSE_CNTL2 */

/* EFUSE_CNTL3 */

/*
 * EFUSE_CNTL4
 *
 * bit[31-24]
 * bit[23-16]  RD_CLOCK_HIGH
 * bit[15-11]
 * bit[10]     Encrypt enable
 * bit[9]      Encrypt reset
 * bit[8]      XOR_ROTATE
 * bit[7-0]    XOR
 */
#define CNTL4_ENCRYPT_ENABLE_BIT            10
#define CNTL4_ENCRYPT_ENABLE_SIZE           1
#define CNTL4_ENCRYPT_ENABLE_ON             1
#define CNTL4_ENCRYPT_ENABLE_OFF            0

#define CNTL4_ENCRYPT_RESET_BIT             9
#define CNTL4_ENCRYPT_RESET_SIZE            1
#define CNTL4_ENCRYPT_RESET_ON              1
#define CNTL4_ENCRYPT_RESET_OFF             0

#define CNTL4_XOR_ROTATE_BIT                8
#define CNTL4_XOR_ROTATE_SIZE               1

#define CNTL4_XOR_BIT                       0
#define CNTL4_XOR_SIZE                      8

static inline void aml_set_reg32_bits(uint32_t _reg, const uint32_t _value,
	const uint32_t _start, const uint32_t	_len)
{
	writel(((readl((void *)_reg)
		& ~(((1L << (_len))-1) << (_start)))
		| ((unsigned int)((_value)&((1L<<(_len))-1)) << (_start))),
		(void *)_reg);
}
#endif
