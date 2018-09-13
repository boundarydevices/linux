/*
 * drivers/amlogic/atv_demod/atv_demod_access.h
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

#ifndef __ATV_DEMOD_ACCESS_H__
#define __ATV_DEMOD_ACCESS_H__

#include <linux/types.h>


extern int amlatvdemod_reg_read(unsigned int reg, unsigned int *val);
extern int amlatvdemod_reg_write(unsigned int reg, unsigned int val);
extern int atvaudiodem_reg_read(unsigned int reg, unsigned int *val);
extern int atvaudiodem_reg_write(unsigned int reg, unsigned int val);
extern int amlatvdemod_hiu_reg_read(unsigned int reg, unsigned int *val);
extern int amlatvdemod_hiu_reg_write(unsigned int reg, unsigned int val);
extern int amlatvdemod_periphs_reg_read(unsigned int reg, unsigned int *val);
extern int amlatvdemod_periphs_reg_write(unsigned int reg, unsigned int val);

extern void atv_dmd_wr_reg(unsigned char block, unsigned char reg,
		unsigned long data);
extern unsigned long atv_dmd_rd_reg(unsigned char block, unsigned char reg);
extern unsigned long atv_dmd_rd_byte(unsigned long block_address,
		unsigned long reg_addr);
extern unsigned long atv_dmd_rd_word(unsigned long block_address,
		unsigned long reg_addr);
extern unsigned long atv_dmd_rd_long(unsigned long block_address,
		unsigned long reg_addr);
extern void atv_dmd_wr_long(unsigned long block_address,
		unsigned long reg_addr, unsigned long data);
extern void atv_dmd_wr_word(unsigned long block_address,
		unsigned long reg_addr, unsigned long data);
extern void atv_dmd_wr_byte(unsigned long block_address,
		unsigned long reg_addr, unsigned long data);


static inline uint32_t R_ATVDEMOD_REG(uint32_t reg)
{
	uint32_t val = 0;

	amlatvdemod_reg_read(reg, &val);
	return val;
}

static inline void W_ATVDEMOD_REG(uint32_t reg, const uint32_t val)
{
	amlatvdemod_reg_write(reg, val);
}

static inline void W_ATVDEMOD_BIT(uint32_t reg, const uint32_t value,
		const uint32_t start, const uint32_t len)
{
	W_ATVDEMOD_REG(reg, ((R_ATVDEMOD_REG(reg) &
			~(((1L << (len)) - 1) << (start))) |
			(((value) & ((1L << (len)) - 1)) << (start))));
}

static inline uint32_t R_ATVDEMOD_BIT(uint32_t reg, const uint32_t start,
		const uint32_t len)
{
	uint32_t val = 0;

	val = ((R_ATVDEMOD_REG(reg) >> (start)) & ((1L << (len)) - 1));

	return val;
}

static inline uint32_t R_HIU_REG(uint32_t reg)
{
	uint32_t val = 0;

	amlatvdemod_hiu_reg_read(reg, &val);

	return val;
}

static inline void W_HIU_REG(uint32_t reg, const uint32_t val)
{
	amlatvdemod_hiu_reg_write(reg, val);
}

static inline void W_HIU_BIT(uint32_t reg, const uint32_t value,
		const uint32_t start, const uint32_t len)
{
	W_HIU_REG(reg, ((R_HIU_REG(reg) &
			~(((1L << (len)) - 1) << (start))) |
			(((value) & ((1L << (len)) - 1)) << (start))));
}

static inline uint32_t R_HIU_BIT(uint32_t reg, const uint32_t start,
		const uint32_t len)
{
	uint32_t val = 0;

	val = ((R_HIU_REG(reg) >> (start)) & ((1L << (len)) - 1));

	return val;
}

static inline uint32_t R_AUDDEMOD_REG(uint32_t reg)
{
	uint32_t val = 0;

	atvaudiodem_reg_read(reg << 2, &val);
	return val;
}

static inline void W_AUDDEMOD_REG(uint32_t reg,
		const uint32_t val)
{
	atvaudiodem_reg_write(reg << 2, val);
}

static inline void W_AUDDEMOD_BIT(uint32_t reg, const uint32_t value,
		const uint32_t start, const uint32_t len)
{
	W_AUDDEMOD_REG(reg, ((R_AUDDEMOD_REG(reg) &
			~(((1L << (len)) - 1) << (start))) |
			(((value) & ((1L << (len)) - 1)) << (start))));
}

static inline uint32_t R_AUDDEMOD_BIT(uint32_t reg, const uint32_t start,
		const uint32_t len)
{
	uint32_t val = 0;

	val = ((R_AUDDEMOD_REG(reg) >> (start)) & ((1L << (len)) - 1));

	return val;
}

#endif /* __ATV_DEMOD_ACCESS_H__ */
