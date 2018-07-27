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

#include <linux/amlogic/iomap.h>
#include <linux/amlogic/media/utils/vdec_reg.h>

#define LDIM_BL_ADDR_PORT			0x144e
#define LDIM_BL_DATA_PORT			0x144f
#define ASSIST_SPARE8_REG1			0x1f58

/*gxtvbb new add*/
#define LDIM_STTS_CTRL0				0x1ac1
#define LDIM_STTS_GCLK_CTRL0			0x1ac0
#define LDIM_STTS_WIDTHM1_HEIGHTM1		0x1ac2
#define LDIM_STTS_MATRIX_COEF00_01		0x1ac3
#define LDIM_STTS_MATRIX_COEF02_10		0x1ac4
#define LDIM_STTS_MATRIX_COEF11_12		0x1ac5
#define LDIM_STTS_MATRIX_COEF20_21		0x1ac6
#define LDIM_STTS_MATRIX_COEF22			0x1ac7
#define LDIM_STTS_MATRIX_OFFSET0_1		0x1ac8
#define LDIM_STTS_MATRIX_OFFSET2		0x1ac9
#define LDIM_STTS_MATRIX_PRE_OFFSET0_1		0x1aca
#define LDIM_STTS_MATRIX_PRE_OFFSET2		0x1acb
#define LDIM_STTS_MATRIX_HL_COLOR		0x1acc
#define LDIM_STTS_MATRIX_PROBE_POS		0x1acd
#define LDIM_STTS_MATRIX_PROBE_COLOR		0x1ace
#define LDIM_STTS_HIST_REGION_IDX		0x1ad0
#define LDIM_STTS_HIST_SET_REGION		0x1ad1
#define LDIM_STTS_HIST_READ_REGION		0x1ad2
#define LDIM_STTS_HIST_START_RD_REGION		0x1ad3

#define VDIN0_HIST_CTRL				0x1230


/*** GXTVBB & TXLX common use register*/
/* each base has 16 address space */
#define REG_LD_CFG_BASE           0x00
#define REG_LD_RGB_IDX_BASE       0x10
#define REG_LD_RGB_LUT_BASE       0x2000
#define REG_LD_MATRIX_BASE        0x3000
/* LD_CFG_BASE */
#define REG_LD_FRM_SIZE           0x0
#define REG_LD_RGB_MOD            0x1
#define REG_LD_BLK_HVNUM          0x2
#define REG_LD_HVGAIN             0x3
#define REG_LD_BKLIT_VLD          0x4
#define REG_LD_BKLIT_PARAM        0x5
#define REG_LD_LIT_GAIN_COMP      0x7
#define REG_LD_FRM_RST_POS        0x8
#define REG_LD_FRM_BL_START_POS   0x9
#define REG_LD_MISC_CTRL0         0xa
#define REG_LD_FRM_HBLAN_VHOLS    0xb
#define REG_LD_XLUT_DEMO_ROI_XPOS 0xc
#define REG_LD_XLUT_DEMO_ROI_YPOS 0xd
#define REG_LD_XLUT_DEMO_ROI_CTRL 0xe


/* each base has 16 address space */
/******  GXTVBB ******/
#define REG_LD_G_IDX_BASE          0x20
#define REG_LD_B_IDX_BASE          0x30
#define REG_LD_RGB_NRMW_BASE       0x40
#define REG_LD_LUT_HDG_BASE        0x50
#define REG_LD_LUT_VHK_BASE        0x60
#define REG_LD_LUT_VDG_BASE        0x70
#define REG_LD_BLK_HIDX_BASE       0x80
#define REG_LD_BLK_VIDX_BASE       0xa0
#define REG_LD_LUT_VHK_NEGPOS_BASE 0xc0
#define REG_LD_LUT_VHO_NEGPOS_BASE 0xd0
#define REG_LD_LUT_HHK_BASE        0xe0
#define REG_LD_REFLECT_DGR_BASE    0xf0
/* LD_CFG_BASE */
#define REG_LD_LUT_XDG_LEXT        0x6



/* each base has 16 address space */
/******  TXLX  ******/
#define REG_LD_RGB_NRMW_BASE_TXLX       0x20
#define REG_LD_BLK_HIDX_BASE_TXLX       0x30
#define REG_LD_BLK_VIDX_BASE_TXLX       0x50
#define REG_LD_LUT_VHK_NEGPOS_BASE_TXLX 0x60
#define REG_LD_LUT_VHO_NEGPOS_BASE_TXLX 0x70
#define REG_LD_LUT_HHK_BASE_TXLX        0x80
#define REG_LD_REFLECT_DGR_BASE_TXLX    0x90
#define REG_LD_LUT_LEXT_BASE_TXLX       0xa0
#define REG_LD_LUT_HDG_BASE_TXLX        0x100
#define REG_LD_LUT_VDG_BASE_TXLX        0x180
#define REG_LD_LUT_VHK_BASE_TXLX        0x200
#define REG_LD_LUT_ID_BASE_TXLX         0x300
/* LD_CFG_BASE */
#define REG_LD_BLMAT_RAM_MISC           0xf

/* #define LDIM_STTS_HIST_REGION_IDX      0x1aa0 */
#define LOCAL_DIM_STATISTIC_EN_BIT          31
#define LOCAL_DIM_STATISTIC_EN_WID           1
#define EOL_EN_BIT                          28
#define EOL_EN_WID                           1

/* 0: 17 pix, 1: 9 pix, 2: 5 pix, 3: 3 pix, 4: 0 pix */
#define HOVLP_NUM_SEL_BIT                   21
#define HOVLP_NUM_SEL_WID                    2
#define LPF_BEFORE_STATISTIC_EN_BIT         20
#define LPF_BEFORE_STATISTIC_EN_WID          1
#define BLK_HV_POS_IDXS_BIT                 16
#define BLK_HV_POS_IDXS_WID                  4
#define RD_INDEX_INC_MODE_BIT               14
#define RD_INDEX_INC_MODE_WID                2
#define REGION_RD_SUB_INDEX_BIT              8
#define REGION_RD_SUB_INDEX_WID              4
#define REGION_RD_INDEX_BIT                  0
#define REGION_RD_INDEX_WID                  7


#ifndef CONFIG_AMLOGIC_MEDIA_RDMA
#define LDIM_VSYNC_RDMA      0
#else
#define LDIM_VSYNC_RDMA      0
#endif

#define Wr_reg_bits(adr, val, start, len)  \
		aml_vcbus_update_bits(adr, ((1<<len)-1)<<start, val<<start)
#define Rd_reg_bits(adr, start, len)  \
		((aml_read_vcbus(adr)>>start)&((1<<len)-1))

#define Wr_reg(reg, val)    aml_write_vcbus(reg, val)
#define Rd_reg(reg)         aml_read_vcbus(reg)


static inline void LDIM_WR_32Bits(unsigned int addr, unsigned int data)
{
#if (LDIM_VSYNC_RDMA == 1)
	VSYNC_WR_MPEG_REG(LDIM_BL_ADDR_PORT, addr);
	VSYNC_WR_MPEG_REG(LDIM_BL_DATA_PORT, data);
#else
	aml_write_vcbus(LDIM_BL_ADDR_PORT, addr);
	aml_write_vcbus(LDIM_BL_DATA_PORT, data);
#endif
}

static inline unsigned int LDIM_RD_32Bits(unsigned int addr)
{
	aml_write_vcbus(LDIM_BL_ADDR_PORT, addr);
	return aml_read_vcbus(LDIM_BL_DATA_PORT);
}

static inline void LDIM_wr_reg_bits(unsigned int addr, unsigned int val,
				unsigned int start, unsigned int len)
{
	unsigned int data;

	data = LDIM_RD_32Bits(addr);
	data = (data & (~((1 << len) - 1)<<start))  |
		((val & ((1 << len) - 1)) << start);
	LDIM_WR_32Bits(addr, data);
}

static inline void LDIM_WR_BASE_LUT(unsigned int base, unsigned int *pData,
				unsigned int size_t, unsigned int len)
{
	unsigned int i;
	unsigned int addr, data;
	unsigned int mask, subCnt;
	unsigned int cnt;

	addr   = base;/* (base<<4); */
	mask   = (1 << size_t)-1;
	subCnt = 32 / size_t;
	cnt  = 0;
	data = 0;

	aml_write_vcbus(LDIM_BL_ADDR_PORT, addr);

	for (i = 0; i < len; i++) {
		data = data | ((pData[i] & mask) << (size_t *cnt));
		cnt++;
		if (cnt == subCnt) {
			aml_write_vcbus(LDIM_BL_DATA_PORT, data);
			data = 0;
			cnt = 0;
			addr++;
		}
	}
	if (cnt != 0)
		aml_write_vcbus(LDIM_BL_DATA_PORT, data);
}

static inline void LDIM_RD_BASE_LUT(unsigned int base, unsigned int *pData,
				unsigned int size_t, unsigned int len)
{
	unsigned int i;
	unsigned int addr, data;
	unsigned int mask, subCnt;
	unsigned int cnt;

	addr   = base;/* (base<<4); */
	mask   = (1 << size_t)-1;
	subCnt = 32 / size_t;
	cnt  = 0;
	data = 0;

	aml_write_vcbus(LDIM_BL_ADDR_PORT, addr);

	for (i = 0; i < len; i++) {
		cnt++;
		if (cnt == subCnt) {
			data = aml_read_vcbus(LDIM_BL_DATA_PORT);
			pData[i-1] = data & mask;
			pData[i] = mask & (data >> size_t);
			data = 0;
			cnt = 0;
			addr++;
		}
	}
	if (cnt != 0)
		data = aml_read_vcbus(LDIM_BL_DATA_PORT);
}

static inline void LDIM_RD_BASE_LUT_2(unsigned int base, unsigned int *pData,
				unsigned int size_t, unsigned int len)
{
	unsigned int i;
	unsigned int addr, data;
	unsigned int mask, subCnt;
	unsigned int cnt;

	addr   = base;/* (base<<4); */
	mask   = (1 << size_t)-1;
	subCnt = 2;
	cnt  = 0;
	data = 0;

	aml_write_vcbus(LDIM_BL_ADDR_PORT, addr);

	for (i = 0; i < len; i++) {
		cnt++;
		if (cnt == subCnt) {
			data = aml_read_vcbus(LDIM_BL_DATA_PORT);
			pData[i-1] = data & mask;
			pData[i] = mask & (data >> size_t);
			data = 0;
			cnt = 0;
			addr++;
		}
	}
	if (cnt != 0)
		data = aml_read_vcbus(LDIM_BL_DATA_PORT);
}

static inline void LDIM_WR_BASE_LUT_DRT(int base, int *pData, int len)
{
	int i;
	int addr;

	addr  = base;/*(base<<4)*/
#if (LDIM_VSYNC_RDMA == 1)
	VSYNC_WR_MPEG_REG(LDIM_BL_ADDR_PORT, addr);
	for (i = 0; i < len; i++)
		VSYNC_WR_MPEG_REG(LDIM_BL_DATA_PORT, pData[i]);
#else
	aml_write_vcbus(LDIM_BL_ADDR_PORT, addr);
	for (i = 0; i < len; i++)
		aml_write_vcbus(LDIM_BL_DATA_PORT, pData[i]);
#endif
}

