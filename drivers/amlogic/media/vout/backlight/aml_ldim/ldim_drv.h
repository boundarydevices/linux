/*
 * drivers/amlogic/media/vout/backlight/aml_ldim/ldim_drv.h
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

#ifndef _AML_LDIM_DRV_H_
#define _AML_LDIM_DRV_H_
#include <linux/amlogic/media/vout/lcd/ldim_alg.h>

/*20180629: initial version */
/*20180725: new pwm control flow support */
/*20180730: algorithm clear up */
#define LDIM_DRV_VER    "20180730"

extern unsigned char ldim_debug_print;

extern int LD_remap_lut[16][32];

#define AML_LDIM_MODULE_NAME "aml_ldim"
#define AML_LDIM_DRIVER_NAME "aml_ldim"
#define AML_LDIM_DEVICE_NAME "aml_ldim"
#define AML_LDIM_CLASS_NAME  "aml_ldim"

/*========================================*/


extern int  ldim_round(int ix, int ib);
extern void ldim_stts_en(unsigned int resolution, unsigned int pix_drop_mode,
	unsigned int eol_en, unsigned int hist_mode, unsigned int lpf_en,
	unsigned int rd_idx_auto_inc_mode, unsigned int one_ram_en);
extern void ldim_set_region(unsigned int resolution, unsigned int blk_height,
	unsigned int blk_width, unsigned int row_start, unsigned int col_start,
	unsigned int blk_hnum);
extern void LD_ConLDReg(struct LDReg_s *Reg);
extern void ld_fw_cfg_once(struct LDReg_s *nPRM);
extern void ldim_stts_read_region(unsigned int nrow, unsigned int ncol);

extern void ldim_set_matrix_ycbcr2rgb(void);
extern void ldim_set_matrix_rgb2ycbcr(int mode);

#endif
