/*
 * drivers/amlogic/media/vout/backlight/aml_ldim/ldim_reg.h
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

#define LDIM_BL_ADDR_PORT				0x144e
#define LDIM_BL_DATA_PORT				0x144f
#define ASSIST_SPARE8_REG1			0x1f58

/*gxtvbb new add*/
#define LDIM_STTS_CTRL0					0x1ac1
#define LDIM_STTS_GCLK_CTRL0						0x1ac0
#define LDIM_STTS_WIDTHM1_HEIGHTM1			0x1ac2
#define LDIM_STTS_MATRIX_COEF00_01			0x1ac3
#define LDIM_STTS_MATRIX_COEF02_10			0x1ac4
#define LDIM_STTS_MATRIX_COEF11_12			0x1ac5
#define LDIM_STTS_MATRIX_COEF20_21			0x1ac6
#define LDIM_STTS_MATRIX_COEF22					0x1ac7
#define LDIM_STTS_MATRIX_OFFSET0_1			0x1ac8
#define LDIM_STTS_MATRIX_OFFSET2				0x1ac9
#define LDIM_STTS_MATRIX_PRE_OFFSET0_1	0x1aca
#define LDIM_STTS_MATRIX_PRE_OFFSET2		0x1acb
#define LDIM_STTS_MATRIX_HL_COLOR				0x1acc
#define LDIM_STTS_MATRIX_PROBE_POS			0x1acd
#define LDIM_STTS_MATRIX_PROBE_COLOR		0x1ace
#define LDIM_STTS_HIST_REGION_IDX				0x1ad0
#define LDIM_STTS_HIST_SET_REGION				0x1ad1
#define LDIM_STTS_HIST_READ_REGION			0x1ad2
#define LDIM_STTS_HIST_START_RD_REGION  0x1ad3

#define VDIN0_HIST_CTRL			0x1230

static inline void W_APB_BIT(unsigned int reg,
				    unsigned int value,
				    unsigned int start,
				    unsigned int  len)
{
	aml_write_vcbus(reg, ((aml_read_vcbus(reg) &
			     ~(((1L << (len)) - 1) << (start))) |
			    (((value) & ((1L << (len)) - 1)) << (start))));
}



