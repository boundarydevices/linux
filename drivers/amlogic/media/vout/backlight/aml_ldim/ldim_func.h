/*
 * drivers/amlogic/media/vout/backlight/aml_ldim/ldim_func.h
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

#include <linux/cdev.h>
#include <linux/amlogic/iomap.h>

#define Wr_reg_bits(adr, val, start, len)  \
		aml_vcbus_update_bits(adr, ((1<<len)-1)<<start, val<<start)
/*	#define Rd_reg_bits(adr, start, len)  \
 *		(aml_read_vcbus(adr)&(((1<<len)-1)<<start))
 */
#define Rd_reg_bits(adr, start, len)  \
		((aml_read_vcbus(adr)>>start)&((1<<len)-1))

extern int  ldim_round(int ix, int ib);
extern void ldim_stts_en(unsigned int resolution, unsigned int pix_drop_mode,
	unsigned int eol_en, unsigned int hist_mode, unsigned int lpf_en,
	unsigned int rd_idx_auto_inc_mode, unsigned int one_ram_en);
extern void ldim_set_region(unsigned int resolution, unsigned int blk_height,
	unsigned int blk_width, unsigned int row_start, unsigned int col_start,
	unsigned int blk_hnum);
extern void LD_ConLDReg(struct LDReg *Reg);
extern void ld_fw_cfg_once(struct LDReg *nPRM);
extern void ldim_stts_read_region(unsigned int nrow, unsigned int ncol);

extern void ldim_set_matrix_ycbcr2rgb(void);
extern void ldim_set_matrix_rgb2ycbcr(int mode);

