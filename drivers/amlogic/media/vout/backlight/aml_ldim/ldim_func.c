/*
 * drivers/amlogic/media/vout/backlight/aml_ldim/ldim_func.c
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
#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/major.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/moduleparam.h>
#include <linux/timer.h>
/* #include <mach/am_regs.h> */
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/spinlock.h>
#include <linux/amlogic/iomap.h>
#include "ldim_drv.h"
#include "ldim_func.h"
#include "ldim_reg.h"
#include <linux/amlogic/media/vout/lcd/aml_bl.h>
#include <linux/amlogic/media/vout/lcd/aml_ldim.h>

#ifndef MIN
#define MIN(a, b)   ((a < b) ? a:b)
#endif

static int LD_STA1max_Hidx[25] = {
	/*  U12* 25	*/
	0, 480, 960, 1440, 1920, 2400, 2880,
	3360, 3840, 4095, 4095, 4095, 4095,
	4095, 4095, 4095, 4095, 4095, 4095,
	4095, 4095, 4095, 4095, 4095, 4095
};

static int LD_STA1max_Vidx[17] = {
	/* u12x 17	*/
	0, 2160, 4095, 4095, 4095, 4095,
	4095, 4095, 4095, 4095, 4095, 4095,
	4095, 4095, 4095, 4095, 4095
};

static int LD_STA2max_Hidx[25] = {
	/* U12* 25	*/
	0, 480, 960, 1440, 1920, 2400, 2880,
	3360, 3840, 4095, 4095, 4095, 4095,
	4095, 4095, 4095, 4095, 4095, 4095,
	4095, 4095, 4095, 4095, 4095, 4095
};

static int LD_STA2max_Vidx[17] = {
	/* u12x 17	*/
	0, 2160, 4095, 4095, 4095, 4095,
	4095, 4095, 4095, 4095, 4095, 4095,
	4095, 4095, 4095, 4095, 4095
};

static int LD_STAhist_Hidx[25] = {
	/* U12* 25	*/
	0, 480, 960, 1440, 1920, 2400, 2880,
	3360, 3840, 4095, 4095, 4095, 4095,
	4095, 4095, 4095, 4095, 4095, 4095,
	4095, 4095, 4095, 4095, 4095, 4095
};

static int LD_STAhist_Vidx[17] = {
	/*  u12x 17	*/
	0, 2160, 4095, 4095, 4095, 4095,
	4095, 4095, 4095, 4095, 4095, 4095,
	4095, 4095, 4095, 4095, 4095
};

static int LD_BLK_Hidx[33] = {
	/* S14* 33	*/
	-1920, -1440, -960, -480, 0, 480,
	960, 1440, 1920, 2400, 2880, 3360,
	3840, 4320, 4800, 5280, 5760, 6240,
	6720, 7200, 7680, 8160, 8191, 8191,
	8191, 8191, 8191, 8191, 8191, 8191,
	8191, 8191, 8191
};

static int LD_BLK_Vidx[25] = {
	/*  S14* 25 */
	-8192, -8192, -8192, -4320, 0, 4320,
	8191, 8191, 8191, 8191, 8191, 8191,
	8191, 8191, 8191, 8191, 8191, 8191,
	8191, 8191, 8191, 8191, 8191, 8191, 8191
};

static int LD_LUT_Hdg[32] = {
	 /*	u10	*/
	503, 501, 494, 481, 465, 447, 430, 409, 388, 369, 354,
	343, 334, 326, 318, 311, 305, 299, 293, 286, 279, 272,
	266, 261, 257, 252, 245, 235, 226, 218, 214, 213
};

static int LD_LUT_Vdg[32] = {
	 /*	u10	*/
	373, 371, 367, 364, 359, 353, 346, 337, 328, 318, 308,
	297, 286, 274, 261, 247, 232, 218, 204, 191, 180, 169,
	158, 148, 138, 130, 122, 115, 108, 104, 100, 97
};

static int LD_LUT_VHk[32] = {
	 /*	u10	*/
	492, 492, 492, 492, 427, 356, 328, 298, 272, 251, 229,
	206, 191, 175, 162, 151, 144, 139, 131, 127, 119, 110,
	105, 101, 99, 98, 94, 85, 83, 77, 74, 73
};

static int LD_LUT_Hdg1[32] = {
	 /*	u10	*/
	503, 501, 494, 481, 465, 447, 430, 409, 388, 369, 354,
	343, 334, 326, 318, 311, 305, 299, 293, 286, 279, 272,
	266, 261, 257, 252, 245, 235, 226, 218, 214, 213
};

static int LD_LUT_Vdg1[32] = {
	/*	u10	*/
	373, 371, 367, 364, 359, 353, 346, 337, 328, 318, 308,
	297, 286, 274, 261, 247, 232, 218, 204, 191, 180, 169,
	158, 148, 138, 130, 122, 115, 108, 104, 100, 97
};

static int LD_LUT_VHk1[32] = {
	 /*	u10	*/
	492, 492, 492, 492, 427, 356, 328, 298, 272, 251, 229,
	206, 191, 175, 162, 151, 144, 139, 131, 127, 119, 110,
	105, 101, 99, 98, 94, 85, 83, 77, 74, 73
};

static int LD_LUT_VHo_pos[] = {
	104, 75, 50, 20, -15, -25, -25, -25, -25, -25, -25,
	-25, -25, -25, -25, -25, -25, -25, -25, -25, -25,
	-25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25
};

static int LD_LUT_VHo_neg[] = {
	104, 75, 50, 20, -15, -25, -25, -25, -25, -25, -25,
	-25, -25, -25, -25, -25, -25, -25, -25, -25, -25,
	-25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25
};

static int LD_LUT_Hdg1_TXLX[32] = {
	455, 487, 498, 505, 506, 509, 503, 494,
	493, 483, 484, 480, 478, 476, 472, 472,
	468, 465, 459, 449, 448, 439, 436, 432,
	430, 413, 402, 386, 361, 343, 317, 307};
static int LD_LUT_Vdg1_TXLX[32] = {
	485, 483, 474, 465, 451, 435, 406, 381,
	350, 320, 283, 251, 211, 178, 147, 113,
	88, 65, 52, 37, 27, 20, 16, 8,
	3, 2, 0, 0, 0, 0, 0, 0};
static int LD_LUT_VHk1_TXLX[32] = {
	490, 410, 356, 317, 288, 272, 266, 260,
	258, 253, 249, 246, 242, 240, 236, 236,
	232, 229, 226, 224, 224, 222, 221, 221,
	221, 219, 219, 221, 219, 225, 228, 237};
static int reg_LD_LUT_Hdg_TXLX[8][32] = {
	{254, 248, 239, 226, 211, 194, 176, 156,
		137, 119, 101, 85, 70, 57, 45, 36,
		28, 21, 16, 12, 9, 6, 4, 3,
		2, 1, 1, 1, 0, 0, 0, 0},
	{254, 248, 239, 226, 211, 194, 176, 156,
		137, 119, 101, 85, 70, 57, 45, 36,
		28, 21, 16, 12, 9, 6, 4, 3,
		2, 1, 1, 1, 0, 0, 0, 0},
	{254, 248, 239, 226, 211, 194, 176, 156,
		137, 119, 101, 85, 70, 57, 45, 36,
		28, 21, 16, 12, 9, 6, 4, 3,
		2, 1, 1, 1, 0, 0, 0, 0},
	{254, 248, 239, 226, 211, 194, 176, 156,
		137, 119, 101, 85, 70, 57, 45, 36,
		28, 21, 16, 12, 9, 6, 4, 3,
		2, 1, 1, 1, 0, 0, 0, 0},
	{254, 248, 239, 226, 211, 194, 176, 156,
		137, 119, 101, 85, 70, 57, 45, 36,
		28, 21, 16, 12, 9, 6, 4, 3,
		2, 1, 1, 1, 0, 0, 0, 0},
	{254, 248, 239, 226, 211, 194, 176, 156,
		137, 119, 101, 85, 70, 57, 45, 36,
		28, 21, 16, 12, 9, 6, 4, 3,
		2, 1, 1, 1, 0, 0, 0, 0},
	{254, 248, 239, 226, 211, 194, 176, 156,
		137, 119, 101, 85, 70, 57, 45, 36,
		28, 21, 16, 12, 9, 6, 4, 3,
		2, 1, 1, 1, 0, 0, 0, 0},
	{254, 248, 239, 226, 211, 194, 176, 156,
		137, 119, 101, 85, 70, 57, 45, 36,
		28, 21, 16, 12, 9, 6, 4, 3,
		2, 1, 1, 1, 0, 0, 0, 0},
};
static int reg_LD_LUT_Vdg_TXLX[8][32] = {
	{254, 248, 239, 226, 211, 194, 176, 156,
		137, 119, 101, 85, 70, 57, 45, 36,
		28, 21, 16, 12, 9, 6, 4, 3,
		2, 1, 1, 1, 0, 0, 0, 0},
	{254, 248, 239, 226, 211, 194, 176, 156,
		137, 119, 101, 85, 70, 57, 45, 36,
		28, 21, 16, 12, 9, 6, 4, 3,
		2, 1, 1, 1, 0, 0, 0, 0},
	{254, 248, 239, 226, 211, 194, 176, 156,
		137, 119, 101, 85, 70, 57, 45, 36,
		28, 21, 16, 12, 9, 6, 4, 3,
		2, 1, 1, 1, 0, 0, 0, 0},
	{254, 248, 239, 226, 211, 194, 176, 156,
		137, 119, 101, 85, 70, 57, 45, 36,
		28, 21, 16, 12, 9, 6, 4, 3,
		2, 1, 1, 1, 0, 0, 0, 0},
	{254, 248, 239, 226, 211, 194, 176, 156,
		137, 119, 101, 85, 70, 57, 45, 36,
		28, 21, 16, 12, 9, 6, 4, 3,
		2, 1, 1, 1, 0, 0, 0, 0},
	{254, 248, 239, 226, 211, 194, 176, 156,
		137, 119, 101, 85, 70, 57, 45, 36,
		28, 21, 16, 12, 9, 6, 4, 3,
		2, 1, 1, 1, 0, 0, 0, 0},
	{254, 248, 239, 226, 211, 194, 176, 156,
		137, 119, 101, 85, 70, 57, 45, 36,
		28, 21, 16, 12, 9, 6, 4, 3,
		2, 1, 1, 1, 0, 0, 0, 0},
	{254, 248, 239, 226, 211, 194, 176, 156,
		137, 119, 101, 85, 70, 57, 45, 36,
		28, 21, 16, 12, 9, 6, 4, 3,
		2, 1, 1, 1, 0, 0, 0, 0},
};
static int reg_LD_LUT_VHk_TXLX[8][32] = {
	{128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128},
	{128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128},
	{128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128},
	{128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128},
	{128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128},
	{128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128},
	{128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128},
	{128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128},
};
static int reg_LD_LUT_VHo_pos_TXLX[32] = {0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int reg_LD_LUT_VHo_neg_TXLX[32] = {0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int reg_LD_LUT_Hdg_LEXT_TXLX[8] = {
	260, 260, 260, 260, 260, 260, 260, 260};
static int reg_LD_LUT_Vdg_LEXT_TXLX[8] = {
	260, 260, 260, 260, 260, 260, 260, 260};
static int reg_LD_LUT_VHk_LEXT_TXLX[8] = {
	128, 128, 128, 128, 128, 128, 128, 128};


/*	public function	*/
int ldim_round(int ix, int ib)
{
	int ld_rst = 0;

	if (ix == 0)
		ld_rst = 0;
	else if (ix > 0)
		ld_rst = (ix + ib / 2) / ib;
	else
		ld_rst = (ix - ib / 2) / ib;

	return ld_rst;
}

/***** local dimming stts functions begin *****/
/*hist mode: 0: comp0 hist only, 1: Max(comp0,1,2) for hist,
 *2: the hist of all comp0,1,2 are calculated
 */
/*lpf_en   1: 1,2,1 filter on before finding max& hist*/
/*rd_idx_auto_inc_mode 0: no self increase, 1: read index increase after
 *read a 25/48 block, 2: increases every read and lock sub-idx
 */
/*one_ram_en 1: one ram mode;  0:double ram mode*/
void ldim_stts_en(unsigned int resolution, unsigned int pix_drop_mode,
	unsigned int eol_en, unsigned int hist_mode, unsigned int lpf_en,
	unsigned int rd_idx_auto_inc_mode, unsigned int one_ram_en)
{
	int data32;

	Wr(LDIM_STTS_GCLK_CTRL0, 0x0);
	Wr(LDIM_STTS_WIDTHM1_HEIGHTM1, resolution);

	data32 = 0x80000000 | ((pix_drop_mode & 0x3) << 29);
	data32 = data32 | ((eol_en & 0x1) << 28);
	data32 = data32 | ((one_ram_en & 0x1) << 27);
	data32 = data32 | ((hist_mode & 0x3) << 22);
	data32 = data32 | ((lpf_en & 0x1) << 21);
	data32 = data32 | ((rd_idx_auto_inc_mode & 0xff) << 14);
	Wr(LDIM_STTS_HIST_REGION_IDX, data32);
}

void ldim_set_region(unsigned int resolution, unsigned int blk_height,
	unsigned int blk_width, unsigned int row_start, unsigned int col_start,
	unsigned int blk_hnum)
{
	unsigned int hend0, hend1, hend2, hend3, hend4, hend5,
			hend6, hend7, hend8, hend9, hend10, hend11, hend12,
			hend13, hend14, hend15, hend16, hend17, hend18,
			hend19, hend20, hend21, hend22, hend23;
	unsigned int vend0, vend1, vend2, vend3, vend4, vend5,
			vend6, vend7, vend8, vend9, vend10, vend11,
			vend12, vend13, vend14, vend15;
	unsigned int data32, k, h_index[24], v_index[16];

	if (resolution == 0) {
		h_index[0] = col_start + blk_width - 1;
		for (k = 1; k < 24; k++) {
			h_index[k] = h_index[k-1] + blk_width;
			if (h_index[k] > 4095)
				h_index[k] = 4095; /* clip U12 */
		}
		v_index[0] = row_start + blk_height - 1;
		for (k = 1; k < 16; k++) {
			v_index[k] = v_index[k-1] + blk_height;
			if (v_index[k] > 4095)
				v_index[k] = 4095; /* clip U12 */
		}
		hend0 = h_index[0];/*col_start + blk_width - 1;*/
		hend1 = h_index[1];/*hend0 + blk_width;*/
		hend2 = h_index[2];/*hend1 + blk_width;*/
		hend3 = h_index[3];/*hend2 + blk_width;*/
		hend4 = h_index[4];/*hend3 + blk_width;*/
		hend5 = h_index[5];/*hend4 + blk_width;*/
		hend6 = h_index[6];/*hend5 + blk_width;*/
		hend7 = h_index[7];/*hend6 + blk_width;*/
		hend8 = h_index[8];/*hend7 + blk_width;*/
		hend9 = h_index[9];/*hend8 + blk_width;*/
		hend10 = h_index[10];/*hend9 + blk_width ;*/
		hend11 = h_index[11];/*hend10 + blk_width;*/
		hend12 = h_index[12];/*hend11 + blk_width;*/
		hend13 = h_index[13];/*hend12 + blk_width;*/
		hend14 = h_index[14];/*hend13 + blk_width;*/
		hend15 = h_index[15];/*hend14 + blk_width;*/
		hend16 = h_index[16];/*hend15 + blk_width;*/
		hend17 = h_index[17];/*hend16 + blk_width;*/
		hend18 = h_index[18];/*hend17 + blk_width;*/
		hend19 = h_index[19];/*hend18 + blk_width;*/
		hend20 = h_index[20];/*hend19 + blk_width ;*/
		hend21 = h_index[21];/*hend20 + blk_width;*/
		hend22 = h_index[22];/*hend21 + blk_width;*/
		hend23 = h_index[23];/*hend22 + blk_width;*/
		vend0 = v_index[0];/*row_start + blk_height - 1;*/
		vend1 = v_index[1];/*vend0 + blk_height;*/
		vend2 = v_index[2];/*vend1 + blk_height;*/
		vend3 = v_index[3];/*vend2 + blk_height;*/
		vend4 = v_index[4];/*vend3 + blk_height;*/
		vend5 = v_index[5];/*vend4 + blk_height;*/
		vend6 = v_index[6];/*vend5 + blk_height;*/
		vend7 = v_index[7];/*vend6 + blk_height;*/
		vend8 = v_index[8];/*vend7 + blk_height;*/
		vend9 = v_index[9];/*vend8 + blk_height;*/
		vend10 = v_index[10];/*vend9 +  blk_height;*/
		vend11 = v_index[11];/*vend10 + blk_height;*/
		vend12 = v_index[12];/*vend11 + blk_height;*/
		vend13 = v_index[13];/*vend12 + blk_height;*/
		vend14 = v_index[14];/*vend13 + blk_height;*/
		vend15 = v_index[15];/*vend14 + blk_height;*/

		data32 = Rd(LDIM_STTS_HIST_REGION_IDX);
		Wr(LDIM_STTS_HIST_REGION_IDX, 0xffe0ffff & data32);
		Wr(LDIM_STTS_HIST_SET_REGION, ((((row_start & 0x1fff) << 16)
			& 0xffff0000) | (col_start & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((hend1 & 0x1fff) << 16)
			| (hend0 & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((vend1 & 0x1fff) << 16)
			| (vend0 & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((hend3 & 0x1fff) << 16)
			| (hend2 & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((vend3 & 0x1fff) << 16)
			| (vend2 & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((hend5 & 0x1fff) << 16)
			| (hend4 & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((vend5 & 0x1fff) << 16)
			| (vend4 & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((hend7 & 0x1fff) << 16)
			| (hend6 & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((vend7 & 0x1fff) << 16)
			| (vend6 & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((hend9 & 0x1fff) << 16)
			| (hend8 & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((vend9 & 0x1fff) << 16)
			| (vend8 & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((hend11 & 0x1fff) << 16)
			| (hend10 & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((vend11 & 0x1fff) << 16)
			| (vend10 & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((hend13 & 0x1fff) << 16)
			| (hend12 & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((vend13 & 0x1fff) << 16)
			| (vend12 & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((hend15 & 0x1fff) << 16)
			| (hend14 & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((vend15 & 0x1fff) << 16)
			| (vend14 & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((hend17 & 0x1fff) << 16)
			| (hend16 & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((hend19 & 0x1fff) << 16)
			| (hend18 & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((hend21 & 0x1fff) << 16)
			| (hend20 & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((hend23 & 0x1fff) << 16)
			| (hend22 & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, blk_hnum); /*h region number*/
		Wr(LDIM_STTS_HIST_SET_REGION, 0); /*line_n_int_num*/
	} else if (resolution == 1) {
		data32 = Rd(LDIM_STTS_HIST_REGION_IDX);
		Wr(LDIM_STTS_HIST_REGION_IDX, 0xfff0ffff & data32);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x0010010);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x1000080);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x0800040);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x2000180);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x10000c0);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x3000280);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x1800140);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4000380);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x20001c0);
		/*Wr(LDIM_STTS_HIST_SET_REGION, 0x4ff0480);*/
		/*Wr(LDIM_STTS_HIST_SET_REGION, 0x2cf0260);*/
	} else if (resolution == 2) {
		data32 = Rd(LDIM_STTS_HIST_REGION_IDX);
		Wr(LDIM_STTS_HIST_REGION_IDX, 0xfff0ffff & data32);

		Wr(LDIM_STTS_HIST_SET_REGION, 0x0000000);/*hv00*/
		Wr(LDIM_STTS_HIST_SET_REGION, 0x17f00bf);/*h01*/
		Wr(LDIM_STTS_HIST_SET_REGION, 0x0d7006b);/*v01*/
		Wr(LDIM_STTS_HIST_SET_REGION, 0x2ff023f);/*h23*/
		Wr(LDIM_STTS_HIST_SET_REGION, 0x1af0143);/*v23*/
		Wr(LDIM_STTS_HIST_SET_REGION, 0x47f03bf);/*h45*/
		Wr(LDIM_STTS_HIST_SET_REGION, 0x287021b);/*v45*/
		Wr(LDIM_STTS_HIST_SET_REGION, 0x5ff053f);/*h67*/
		Wr(LDIM_STTS_HIST_SET_REGION, 0x35f02f3);/*v67*/
		Wr(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/*h89*/
		Wr(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/*v89*/
		Wr(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/*h1011*/
		Wr(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/*v1011*/
		Wr(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/*h1213*/
		Wr(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/*v1213*/
		Wr(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/*h1415*/
		Wr(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/*v1415*/
		Wr(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/*h1617*/
		Wr(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/*h1819*/
		Wr(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/*h2021*/
		Wr(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/*h2223*/
	} else if (resolution == 3) {
		data32 = Rd(LDIM_STTS_HIST_REGION_IDX);
		Wr(LDIM_STTS_HIST_REGION_IDX, 0xfff0ffff & data32);

		Wr(LDIM_STTS_HIST_SET_REGION, 0x0000000);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x1df00ef);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x10d0086);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x3bf02cf);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x21b0194);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x59f04af);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x32902a2);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x77f068f);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x43703b0);
		/*Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);*/
		/*Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);*/
	} else if (resolution == 4) { /* 5x6 */
		data32 = Rd(LDIM_STTS_HIST_REGION_IDX);
		Wr(LDIM_STTS_HIST_REGION_IDX, 0xfff0ffff & data32);

		Wr(LDIM_STTS_HIST_SET_REGION, 0x0040001);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x27f0136);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x1af00d7);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4ff03bf);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x35f0287);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x77f063f);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4380437);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		/*Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);*/
		/*Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);*/
	} else if (resolution == 5) { /* 8x2 */
		data32 = Rd(LDIM_STTS_HIST_REGION_IDX);
		Wr(LDIM_STTS_HIST_REGION_IDX, 0xfff0ffff & data32);

		Wr(LDIM_STTS_HIST_SET_REGION, 0x0030002);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x31f02bb);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x0940031);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x233012b);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x30b0243);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x42d03d3);
		/*Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);*/
		/*Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);*/
	} else if (resolution == 6) { /* 2x1 */
		data32 = Rd(LDIM_STTS_HIST_REGION_IDX);
		Wr(LDIM_STTS_HIST_REGION_IDX, 0xfff0ffff & data32);

		Wr(LDIM_STTS_HIST_SET_REGION, 0x0030002);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x78002bb);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x0940031);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		/*Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);*/
		/*Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);*/
	} else if (resolution == 7) { /* 2x2 */
		data32 = Rd(LDIM_STTS_HIST_REGION_IDX);
		Wr(LDIM_STTS_HIST_REGION_IDX, 0xfff0ffff & data32);

		Wr(LDIM_STTS_HIST_SET_REGION, 0x0000000);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x77f03bf);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x437021b);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		/*Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);*/
		/*Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);*/
	} else if (resolution == 8) { /* 3x5 */
		data32 = Rd(LDIM_STTS_HIST_REGION_IDX);
		Wr(LDIM_STTS_HIST_REGION_IDX, 0xfff0ffff & data32);

		Wr(LDIM_STTS_HIST_SET_REGION, 0x0000000);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x2ff017f);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x2cf0167);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x5ff047f);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4380437);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x780077f);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		/*Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);*/
		/*Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);*/
	} else if (resolution == 9) { /* 4x3 */
		data32 = Rd(LDIM_STTS_HIST_REGION_IDX);
		Wr(LDIM_STTS_HIST_REGION_IDX, 0xfff0ffff & data32);

		Wr(LDIM_STTS_HIST_SET_REGION, 0x0010001);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4560333);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x2220180);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x7800666);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4000338);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		/*Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);*/
		/*Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);*/
	} else if (resolution == 10) { /* 6x8 */
		data32 = Rd(LDIM_STTS_HIST_REGION_IDX);
		Wr(LDIM_STTS_HIST_REGION_IDX, 0xfff0ffff & data32);

		Wr(LDIM_STTS_HIST_SET_REGION, 0x0010001);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x2430167);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x2220180);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4000350);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4000338);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x6000510);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4370410);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x77f0700);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		/*Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);*/
		/*Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);*/
	}
}

static unsigned int invalid_val_cnt;
void ldim_stts_read_region(unsigned int nrow, unsigned int ncol)
{
	unsigned int i, j, k;
	unsigned int data32;
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();

	if (invalid_val_cnt > 0xfffffff)
		invalid_val_cnt = 0;

	Wr(LDIM_STTS_HIST_REGION_IDX, Rd(LDIM_STTS_HIST_REGION_IDX)
		& 0xffffc000);
	data32 = Rd(LDIM_STTS_HIST_START_RD_REGION);

	for (i = 0; i < nrow; i++) {
		for (j = 0; j < ncol; j++) {
			data32 = Rd(LDIM_STTS_HIST_START_RD_REGION);
			for (k = 0; k < 17; k++) {
				if (k == 16) {
					data32 = Rd(LDIM_STTS_HIST_READ_REGION);
					ldim_drv->max_rgb[i * ncol + j]
						= data32;
				} else {
					data32 = Rd(LDIM_STTS_HIST_READ_REGION);
					ldim_drv->hist_matrix[i * ncol * 16 +
						j * 16 + k] = data32;
				}
				if (!(data32 & 0x40000000))
					invalid_val_cnt++;
			}
		}
	}
}

/* VDIN_MATRIX_YUV601_RGB */
/* -16	   1.164  0	 1.596	    0 */
/* -128     1.164 -0.391 -0.813      0 */
/* -128     1.164  2.018  0	     0 */
/*{0x07c00600, 0x00000600, 0x04a80000, 0x066204a8, 0x1e701cbf, 0x04a80812,
 *	0x00000000, 0x00000000, 0x00000000,},
 */
void ldim_set_matrix_ycbcr2rgb(void)
{
	Wr_reg_bits(LDIM_STTS_CTRL0, 1, 2, 1);  /* enable matrix */

	Wr(LDIM_STTS_MATRIX_PRE_OFFSET0_1, 0x07c00600);
	Wr(LDIM_STTS_MATRIX_PRE_OFFSET2, 0x00000600);
	Wr(LDIM_STTS_MATRIX_COEF00_01, 0x04a80000);
	Wr(LDIM_STTS_MATRIX_COEF02_10, 0x066204a8);
	Wr(LDIM_STTS_MATRIX_COEF11_12, 0x1e701cbf);
	Wr(LDIM_STTS_MATRIX_COEF20_21, 0x04a80812);
	Wr(LDIM_STTS_MATRIX_COEF22, 0x00000000);
	Wr(LDIM_STTS_MATRIX_OFFSET0_1, 0x00000000);
	Wr(LDIM_STTS_MATRIX_OFFSET2, 0x00000000);
}

void ldim_set_matrix_rgb2ycbcr(int mode)
{
	Wr_reg_bits(LDIM_STTS_CTRL0, 1, 2, 1);
	if (mode == 0) {/*ycbcr not full range, 601 conversion*/
		Wr(LDIM_STTS_MATRIX_PRE_OFFSET0_1, 0x0);
		Wr(LDIM_STTS_MATRIX_PRE_OFFSET2, 0x0);
		/*	0.257     0.504   0.098	*/
		/*	-0.148    -0.291  0.439	*/
		/*	0.439     -0.368 -0.071	*/
		Wr(LDIM_STTS_MATRIX_COEF00_01, (0x107 << 16) | 0x204);
		Wr(LDIM_STTS_MATRIX_COEF02_10, (0x64 << 16) | 0x1f68);
		Wr(LDIM_STTS_MATRIX_COEF11_12, (0x1ed6 << 16) | 0x1c2);
		Wr(LDIM_STTS_MATRIX_COEF20_21, (0x1c2 << 16) | 0x1e87);
		Wr(LDIM_STTS_MATRIX_COEF22, 0x1fb7);

		Wr(LDIM_STTS_MATRIX_OFFSET2, 0x0200);
	} else if (mode == 1) {/*ycbcr full range, 601 conversion*/
		/* todo */
	}
}

int Round(int iX, int iB)
{
	int Rst = 0;

	if (iX == 0)
		Rst = 0;
	else if (iX > 0)
		Rst = (iX + iB / 2) / iB;
	else
		Rst = (iX - iB / 2) / iB;
	return Rst;
}

void LD_IntialData(int pData[], int size, int val)
{
	int i;

	for (i = 0; i < size; i++)
		pData[i] = val;
}

int LD_GetBLMtxAvg(int pMtx[], int size, int mode)
{
	int i;
	int da;

	if (mode == 0)
		da = 0;
	else if (mode == 1) {
		da = 0;
		for (i = 0; i < size; i++)
			da += pMtx[i];
		da = da / size;
	} else if (mode == 2) {
		da = pMtx[0];
		for (i = 1; i < size; i++) {
			if (da > pMtx[i])
				da = pMtx[i];
		}
	} else {
		da = pMtx[0];
		for (i = 1; i < size; i++) {
			if (da < pMtx[i])
				da = pMtx[i];
		}
	}
	return da;
}

void LD_MtxInv(int *oDat, int *iDat, int nRow, int nCol)
{
	int nT1 = 0;
	int nT2 = 0;
	int nY = 0;
	int nX = 0;

	for (nY = 0; nY < nRow; nY++) {
		for (nX = 0; nX < nCol; nX++) {
			nT1 = nY * nCol + nX;
			nT2 = nX * nRow + nY;
			oDat[nT2] = iDat[nT1];
		}
	}
}

static int Remap_lut[16][32] = {
	{
		128, 258, 387, 517, 646, 776, 905, 1034,
		1163, 1291, 1420, 1548, 1676, 1804, 1932, 2059,
		2187, 2314, 2441, 2569, 2696, 2823, 2950, 3077,
		3204, 3331, 3458, 3586, 3713, 3840, 3968, 4095,
	},
	{
		114,  230,  345,  461,  577,  693,  808,  923,
		1038, 1153, 1267, 1382, 1496, 1610, 1724, 1844,
		1987, 2130, 2272, 2415, 2557, 2700, 2842, 2985,
		3127, 3270, 3412, 3555, 3697, 3840, 3981, 4095,
	},
	{
		114,  230,  345,  461,  577,  692,  808,  923,
		1038, 1152, 1267, 1381, 1496, 1610, 1731, 1874,
		2017, 2160, 2303, 2445, 2588, 2731, 2873, 3015,
		3158, 3300, 3443, 3585, 3728, 3868, 3981, 4095,
	},
	{
		115,  230,  346,  462,  579,  694,  810,  926,
		1041, 1155, 1270, 1384, 1499, 1637, 1781, 1923,
		2066, 2209, 2351, 2494, 2636, 2778, 2921, 3063,
		3205, 3347, 3490, 3632, 3754, 3868, 3982, 4095,
	},
	{
		119,  243,  367,  493,  620,  744,  867,  988,
		1106, 1224, 1340, 1456, 1596, 1739, 1881, 2021,
		2161, 2300, 2438, 2576, 2713, 2850, 2987, 3124,
		3261, 3399, 3536, 3652, 3761, 3871, 3982, 4095,
	},
	{
		116,  240,  364,  490,  619,  742,  863,  982,
		1096, 1208, 1319, 1469, 1620, 1769, 1917, 2063,
		2208, 2352, 2494, 2636, 2777, 2918, 3059, 3200,
		3341, 3482, 3589, 3687, 3786, 3887, 3990, 4095,
	},
	{
		114,  237,  361,  487,  615,  737,  857,  973,
		1085, 1221, 1380, 1536, 1691, 1844, 1994, 2144,
		2291, 2438, 2583, 2727, 2870, 3014, 3157, 3300,
		3424, 3518, 3610, 3704, 3798, 3894, 3994, 4095,
	},
	{
		113,  238,  363,  490,  620,  744,  863,  978,
		1091, 1256, 1419, 1579, 1737, 1893, 2046, 2198,
		2348, 2496, 2642, 2788, 2933, 3077, 3222, 3366,
		3459, 3547, 3633, 3722, 3811, 3903, 3997, 4095,
	},
	{
		120,  255,  390,  528,  669,  801,  928, 1050,
		1194, 1356, 1515, 1671, 1825, 1975, 2123, 2268,
		2412, 2553, 2693, 2832, 2970, 3107, 3244, 3361,
		3448, 3536, 3623, 3713, 3804, 3897, 3994, 4095,
	},
	{
		134,  292,  449,  611,  775,  924, 1063, 1193,
		1340, 1503, 1661, 1814, 1963, 2108, 2249, 2387,
		2522, 2654, 2785, 2914, 3042, 3169, 3297, 3405,
		3484, 3565, 3644, 3727, 3813, 3901, 3996, 4095,
	},
	{
		160,  358,  550,  746,  943, 1113, 1266, 1406,
		1552, 1708, 1858, 2000, 2138, 2269, 2397, 2520,
		2640, 2757, 2872, 2985, 3098, 3211, 3325, 3422,
		3496, 3572, 3647, 3727, 3811, 3898, 3993, 4095,
	},
	{
		201,  456,  693,  929, 1162, 1354, 1520, 1671,
		1822, 1963, 2095, 2220, 2339, 2451, 2559, 2662,
		2762, 2859, 2955, 3049, 3143, 3239, 3335, 3407,
		3479, 3556, 3630, 3712, 3797, 3887, 3987, 4095,
	},
	{
		243,  546,  815, 1079, 1334, 1536, 1707, 1857,
		1996, 2125, 2245, 2357, 2462, 2561, 2655, 2746,
		2833, 2917, 3000, 3083, 3166, 3250, 3336, 3405,
		3476, 3551, 3624, 3706, 3791, 3882, 3984, 4095,
	},
	{
		280,  622,  914, 1195, 1465, 1672, 1844, 1993,
		2122, 2240, 2349, 2450, 2545, 2634, 2718, 2798,
		2875, 2950, 3023, 3097, 3171, 3247, 3325, 3393,
		3464, 3540, 3614, 3697, 3784, 3876, 3981, 4095,
	},
	{
		308,  675,  979, 1268, 1544, 1752, 1922, 2067,
		2191, 2304, 2408, 2505, 2595, 2679, 2758, 2834,
		2907, 2978, 3048, 3118, 3189, 3262, 3338, 3404,
		3473, 3546, 3618, 3700, 3785, 3877, 3981, 4095,
	},
	{
		342,  738, 1058, 1360, 1648, 1862, 2037, 2184,
		2295, 2396, 2488, 2573, 2651, 2724, 2792, 2857,
		2919, 2979, 3038, 3098, 3158, 3221, 3287, 3357,
		3431, 3510, 3588, 3674, 3766, 3864, 3975, 4095,
	},
};

static int Remap_lut2[16][16] = {};
void LD_LUTInit(struct LDReg *Reg)
{
	int i, j, k, t;
	struct aml_bl_drv_s *bl_drv = aml_bl_get_driver();

	switch (bl_drv->data->chip_type) {
	case BL_CHIP_TXLX:
		for (i = 0; i < 16; i++) {
			for (j = 0; j < 16; j++)
				Remap_lut2[i][j] = Remap_lut[i][j * 2] |
					(Remap_lut[i][j * 2 + 1] << 16);
		}
		/* Emulate the FW to set the LUTs */
		for (k = 0; k < 16; k++) {
			/*set the LUT to be inverse of the Lit_value,*/
			/* lit_idx distribute equal space, set by FW */
			Reg->X_idx[0][k] = 4095 - 256 * k;
			Reg->X_nrm[0][k] = 8;
			for (t = 0; t < 16; t++) {
				Reg->X_lut2[0][k][t] = Remap_lut2[k][t];
				Reg->X_lut2[1][k][t] = Remap_lut2[k][t];
				Reg->X_lut2[2][k][t] = Remap_lut2[k][t];
			}
		}
		break;
	case BL_CHIP_GXTVBB:
			/* Emulate the FW to set the LUTs */
		for (k = 0; k < 16; k++) {
			/*set the LUT to be inverse of the Lit_value,*/
			/* lit_idx distribute equal space, set by FW */
			Reg->X_idx[0][k] = 4095 - 256 * k;
			Reg->X_nrm[0][k] = 8;
			for (t = 0; t < 32; t++) {
				Reg->X_lut[0][k][t] = Remap_lut[k][t];
				Reg->X_lut[1][k][t] = Remap_lut[k][t];
				Reg->X_lut[2][k][t] = Remap_lut[k][t];
			}
		}
		break;
	default:
		break;
	}
}

static void ConLDReg_GXTVBB(struct LDReg *Reg)
{
	unsigned int T = 0;
	unsigned int Vnum = 0;
	unsigned int Hnum = 0;
	unsigned int BSIZE = 0;

	/* General registers; */
	Reg->reg_LD_pic_RowMax = 2160;/* setting default */
	Reg->reg_LD_pic_ColMax = 3840;
	LD_IntialData(Reg->reg_LD_pic_YUVsum, 3, 0);
	/* only output u16*3, (internal ACC will be u32x3)*/
	LD_IntialData(Reg->reg_LD_pic_RGBsum, 3, 0);

	/* set same region division for statistics */
	Reg->reg_LD_STA_Vnum  = 8;
	Reg->reg_LD_STA_Hnum  = 8;

	/*Image Statistic options */
	Reg->reg_LD_BLK_Vnum = 1;/*u5: Maximum to BLKVMAX */
	Reg->reg_LD_BLK_Hnum  = 8;/*u5: Maximum to BLKHMAX */

	Reg->reg_LD_STA1max_LPF = 1;
	/*u1: STA1max statistics on [1 2 1]/4 filtered results */
	Reg->reg_LD_STA2max_LPF = 1;
	/*u1: STA2max statistics on [1 2 1]/4 filtered results*/
	Reg->reg_LD_STAhist_LPF  = 1;
	/*u1: STAhist statistics on [1 2 1]/4 filtered results*/
	Reg->reg_LD_STA1max_Hdlt = 0;
	/*u2: (2^x) extra pixels into Max calculation*/
	Reg->reg_LD_STA1max_Vdlt = 0;
	/*u4: extra pixels into Max calculation vertically*/
	Reg->reg_LD_STA2max_Hdlt = 0;
	/*u2: (2^x) extra pixels into Max calculation*/
	Reg->reg_LD_STA2max_Vdlt = 0;
	/*u4: extra pixels into Max calculation vertically*/
	Reg->reg_LD_STAhist_mode = 3;
	/*u3: histogram statistics on XX separately 20bits*16bins:
	 *	0:R-only,1:G-only 2:B-only 3:Y-only; 4:MAX(R,G,B),5/6/7:R&G&B
	 */
	Reg->reg_LD_STAhist_pix_drop_mode = 0;/*u2 */
	for (T = 0; T < LD_STA_LEN_H; T++) {
		Reg->reg_LD_STA1max_Hidx[T] = LD_STA1max_Hidx[T];/*U12* 25*/
		Reg->reg_LD_STA2max_Hidx[T] = LD_STA2max_Hidx[T];/*U12* 25*/
		Reg->reg_LD_STAhist_Hidx[T] = LD_STAhist_Hidx[T];/*U12* 25*/
	}
	for (T = 0; T < LD_STA_LEN_V; T++) {
		Reg->reg_LD_STA1max_Vidx[T] = LD_STA1max_Vidx[T];/*u12x 17*/
		Reg->reg_LD_STA2max_Vidx[T] = LD_STA2max_Vidx[T];/*u12x 17*/
		Reg->reg_LD_STAhist_Vidx[T] = LD_STAhist_Vidx[T];/*u12x 17*/
	}

	/******	FBC3 fw_hw_alg_frm	*******/
	Reg->reg_ldfw_BLmax = 4095;       /*maximum BL value*/
	Reg->reg_ldfw_blk_norm = 128;
	/*u8: normalization gain for blk number,
	 *	1/blk_num= norm>>(rs+8), norm = (1<<(rs+8))/blk_num
	 */

	Reg->reg_ldfw_blk_norm_rs = 2;
	/*u3: 0~7,  1/blk_num= norm>>(rs+8)*/

	for (T = 0; T < 8; T++)
		Reg->reg_ldfw_sta_hdg_weight[T] = 64;

	Reg->reg_ldfw_sta_max_mode = 3;
	/* u2: maximum selection for components:
	 *	0: r_max, 1: g_max, 2: b_max; 3: max(r,g,b)
	 */

	Reg->reg_ldfw_sta_max_hist_mode = 0;
	/* u2: mode of reference max/hist mode:
	 *	0: MIN(max, hist), 1: MAX(max, hist) 2: (max+hist)/2,
	 *	3: (max(a,b)*3 + min(a,b))/4
	 */

	Reg->reg_ldfw_hist_valid_rate = 64;
	/* u8, norm to 512 as "1", if hist_matrix[i]>(rate*histavg)>>9 */

	Reg->reg_ldfw_hist_valid_ofst = 63;/* u8, hist valid bin upward offset*/
	Reg->reg_ldfw_sedglit_RL = 1;/*u1: single edge lit right/bottom mode*/

	Reg->reg_ldfw_sf_thrd = 1600;
	/*u12: threshold of difference to enable the sf;*/

	Reg->reg_ldfw_boost_gain = 64;
	/* u8: boost gain for the region that is
	 *	larger than the average, norm to 16 as "1"
	 */

	Reg->reg_ldfw_tf_alpha_rate = 16;
	/*u8: rate to SFB_BL_matrix from last frame difference;*/

	Reg->reg_ldfw_tf_alpha_ofst = 32;
	/* u8: ofset to alpha SFB_BL_matrix from last frame difference;*/

	Reg->reg_ldfw_tf_disable_th = 255;
	/* u8: 4x is the threshod to disable tf to the alpha
	 *	(SFB_BL_matrix from last frame difference;
	 */

	Reg->reg_ldfw_blest_acmode = 1;
	/* u3: 0: est on BLmatrix; 1: est on (BL-DC);
	 *	2: est on (BL-MIN); 3: est on (BL-MAX) 4: 2048; 5:1024
	 */

	Reg->reg_ldfw_sf_enable = 0;
	/* u1: enable signal for spatial filter on the tbl_matrix */

	Reg->reg_ldfw_boost_enable = 0;
	/* u1: enable signal for Boost filter on the tbl_matrix */

	Reg->ro_ldfw_bl_matrix_avg = 0;
	/* u12: read-only register for bl_matrix */

	/*---------------------Setting BL_matrix initial value
	 *		(will be updated frame to frame in FW)
	 */
	Vnum = Reg->reg_LD_BLK_Vnum;
	Hnum = Reg->reg_LD_BLK_Hnum;
	BSIZE = Vnum*Hnum;
	/*Initialization */
	LD_IntialData(Reg->BL_matrix, BSIZE, 4095);

	/* BackLight Modeling control register setting*/
	Reg->reg_LD_BackLit_Xtlk = 1;
	/* u1: 0 no block to block Xtalk model needed;	 1: Xtalk model needed*/
	Reg->reg_LD_BackLit_mode = 1;
	/*u2: 0- LEFT/RIGHT Edge Lit; 1- Top/Bot Edge Lit; 2 - DirectLit modeled
	 *	H/V independent; 3- DirectLit modeled HV Circle distribution
	 */
	Reg->reg_LD_Reflect_Hnum = 3;
	/*u3: numbers of band reflection considered in Horizontal
	 *		direction; 0~4
	 */
	Reg->reg_LD_Reflect_Vnum = 0;
	/*u3: numbers of band reflection considered in Horizontal
	 *		direction; 0~4
	 */
	Reg->reg_LD_BkLit_curmod = 0;
	/*u1: 0: H/V separately, 1 Circle distribution*/
	Reg->reg_LD_BkLUT_Intmod = 1;
	/*u1: 0: linear interpolation, 1 cubical interpolation*/
	Reg->reg_LD_BkLit_Intmod = 1;
	/*u1: 0: linear interpolation, 1 cubical interpolation*/
	Reg->reg_LD_BkLit_LPFmod = 7;
	/*	u3: 0: no LPF, 1:[1 14 1]/16;2:[1 6 1]/8; 3: [1 2 1]/4;
	 *		4:[9 14 9]/32  5/6/7: [5 6 5]/16;
	 */
	Reg->reg_LD_BkLit_Celnum = 121;
	/*u8:0:1920~61####((Reg->reg_LD_pic_ColMax+1)/32)+1;*/
	Reg->reg_BL_matrix_AVG = 4095;
	/*	u12: DC of whole picture BL to be subtract from BL_matrix
	 *	during modeling (Set by FW daynamically)
	 */
	Reg->reg_BL_matrix_Compensate = 4095;
	/*	u12: DC of whole picture BL to be compensated back to
	 *	Litfull after the model (Set by FW dynamically);
	 */
	LD_IntialData(Reg->reg_LD_Reflect_Hdgr, 20, 32);
	/*20*u6: cells 1~20 for H Gains of different dist of Left/Right;*/
	LD_IntialData(Reg->reg_LD_Reflect_Vdgr, 20, 32);
	/*20*u6: cells 1~20 for V Gains of different dist of Top/Bot; */
	LD_IntialData(Reg->reg_LD_Reflect_Xdgr, 4, 32);/*  4*u6: */

	Reg->reg_LD_Vgain	= 256;/* u12 */
	Reg->reg_LD_Hgain	= 242;/* u12 */
	Reg->reg_LD_Litgain = 256;/* u12 */
	Reg->reg_LD_Litshft = 3;
	/* u3	right shif of bits for the all Lit's sum */
	LD_IntialData(Reg->reg_LD_BkLit_valid, 32, 1);
	/*u1x32: valid bits for the 32 cell Bklit to contribut to current
	 *	position (refer to the backlit padding pattern)
	 *	region division index 1 2 3 4 5(0) 6(1) 7(2) 8(3) 9(4)
	 * 10(5)11(6)12(7)13(8) 14(9)15(10) 16   17   18	19
	 */
	for (T = 0; T < LD_BLK_LEN_H; T++)
		Reg->reg_LD_BLK_Hidx[T] = LD_BLK_Hidx[T];/* S14* BLK_LEN_H */
	for (T = 0; T < LD_BLK_LEN_V; T++)
		Reg->reg_LD_BLK_Vidx[T] = LD_BLK_Vidx[T];/* S14x BLK_LEN_V */
	for (T = 0; T < LD_LUT_LEN; T++) {
		Reg->reg_LD_LUT_Hdg[T] = LD_LUT_Hdg[T];
		Reg->reg_LD_LUT_Vdg[T] = LD_LUT_Vdg[T];
		Reg->reg_LD_LUT_VHk[T] = LD_LUT_VHk[T];
	}
	/* set the VHk_pos and VHk_neg value ,normalized to
	 *	128 as "1" 20150428
	 */
	for (T = 0; T < 32; T++) {
		Reg->reg_LD_LUT_VHk_pos[T] = 128;/* vdist>=0 */
		Reg->reg_LD_LUT_VHk_neg[T] = 128;/* vdist<0 */
		Reg->reg_LD_LUT_HHk[T] = 128;/* hdist gain */
		Reg->reg_LD_LUT_VHo_pos[T] = 0;/* vdist>=0 */
		Reg->reg_LD_LUT_VHo_neg[T] = 0;/*  vdist<0 */
	}
	Reg->reg_LD_LUT_VHo_LS = 0;
	Reg->reg_LD_LUT_Hdg_LEXT = 505;
	/* 2*(nPRM->reg_LD_LUT_Hdg[0]) - (nPRM->reg_LD_LUT_Hdg[1]); */
	Reg->reg_LD_LUT_Vdg_LEXT = 372;
	/* 2*(nPRM->reg_LD_LUT_Vdg[0]) - (nPRM->reg_LD_LUT_Vdg[1]); */
	Reg->reg_LD_LUT_VHk_LEXT = 492;
	/* 2*(nPRM->reg_LD_LUT_VHk[0]) - (nPRM->reg_LD_LUT_VHk[1]); */
	/* set the demo window */
	Reg->reg_LD_xlut_demo_roi_xstart = (Reg->reg_LD_pic_ColMax/4);
	     /* u14 start col index of the region of interest */
	Reg->reg_LD_xlut_demo_roi_xend = (Reg->reg_LD_pic_ColMax*3/4);
	  /* u14 end col index of the region of interest */
	Reg->reg_LD_xlut_demo_roi_ystart = (Reg->reg_LD_pic_RowMax/4);
	     /* u14 start row index of the region of interest */
	Reg->reg_LD_xlut_demo_roi_yend = (Reg->reg_LD_pic_RowMax*3/4);
	   /*  u14 end row index of the region of interest */
	Reg->reg_LD_xlut_iroi_enable = 1;
	     /*  u1: enable rgb LUT remapping inside regon of interest:
	      *	0: no rgb remapping; 1: enable rgb remapping
	      */
	Reg->reg_LD_xlut_oroi_enable = 1;
	    /* u1: enable rgb LUT remapping outside regon of interest:
	     * 0: no rgb remapping; 1: enable rgb remapping
	     */

	/*  Registers used in LD_RGB_LUT for RGB remaping */
	Reg->reg_LD_RGBmapping_demo = 0;
	/* u2: 0 no demo mode 1: display BL_fulpel on RGB */
	Reg->reg_LD_X_LUT_interp_mode[0] = 1;
	 /* U1 0: using linear interpolation between to neighbour LUT;
	  *	1: use the nearest LUT results
	  */
	Reg->reg_LD_X_LUT_interp_mode[1] = 1;
	 /*  U1 0: using linear interpolation between to neighbour LUT;
	  *	1: use the nearest LUT results
	  */
	Reg->reg_LD_X_LUT_interp_mode[2] = 1;
	 /* U1 0: using linear interpolation between to neighbour LUT;
	  *	1: use the nearest LUT results
	  */
	LD_LUTInit(Reg);
	/* only do the Lit modleing on the AC part */
	Reg->fw_LD_BLEst_ACmode = 0;
	/* u2: 0: est on BLmatrix; 1: est on (BL-DC);
	 *	2: est on (BL-MIN); 3: est on (BL-MAX)
	 */
}


static void ConLDReg_TXLX(struct LDReg *Reg)
{
	int i, j;
	unsigned int T = 0;
	unsigned int Vnum = 0;
	unsigned int Hnum = 0;
	unsigned int BSIZE = 0;

	/* General registers; */
	Reg->reg_LD_pic_RowMax = 2160;/* setting default */
	Reg->reg_LD_pic_ColMax = 3840;
	LD_IntialData(Reg->reg_LD_pic_YUVsum, 3, 0);
	/* only output u16*3, (internal ACC will be u32x3)*/
	LD_IntialData(Reg->reg_LD_pic_RGBsum, 3, 0);

	/* set same region division for statistics */
	Reg->reg_LD_STA_Vnum  = 1;
	Reg->reg_LD_STA_Hnum  = 8;

	/*Image Statistic options */
	Reg->reg_LD_BLK_Vnum = 1;/*u5: Maximum to BLKVMAX */
	Reg->reg_LD_BLK_Hnum  = 8;/*u5: Maximum to BLKHMAX */

	Reg->reg_LD_STA1max_LPF = 1;
	/*u1: STA1max statistics on [1 2 1]/4 filtered results */
	Reg->reg_LD_STA2max_LPF = 1;
	/*u1: STA2max statistics on [1 2 1]/4 filtered results*/
	Reg->reg_LD_STAhist_LPF  = 1;
	/*u1: STAhist statistics on [1 2 1]/4 filtered results*/
	Reg->reg_LD_STA1max_Hdlt = 0;
	/*u2: (2^x) extra pixels into Max calculation*/
	Reg->reg_LD_STA1max_Vdlt = 0;
	/*u4: extra pixels into Max calculation vertically*/
	Reg->reg_LD_STA2max_Hdlt = 0;
	/*u2: (2^x) extra pixels into Max calculation*/
	Reg->reg_LD_STA2max_Vdlt = 0;
	/*u4: extra pixels into Max calculation vertically*/
	Reg->reg_LD_STAhist_mode = 3;
	/* u3: histogram statistics on XX separately 20bits*16bins:
	 *	0:R-only,1:G-only 2:B-only 3:Y-only; 4:MAX(R,G,B),5/6/7:R&G&B
	 */

	/******	FBC3 fw_hw_alg_frm	*******/
	Reg->reg_ldfw_blest_acmode = 0;
	/* u3: 0: est on BLmatrix; 1: est on (BL-DC);
	 *	2: est on (BL-MIN); 3: est on (BL-MAX) 4: 2048; 5:1024
	 */

	Reg->reg_ldfw_blk_norm = 128;
	/* u8: normalization gain for blk number,
	 *	1/blk_num= norm>>(rs+8), norm = (1<<(rs+8))/blk_num
	 */

	Reg->reg_ldfw_blk_norm_rs = 2;
	/*u3: 0~7,  1/blk_num= norm>>(rs+8)*/

	Reg->reg_ldfw_BLmax = 4095;       /*maximum BL value*/

	Reg->reg_ldfw_boost_enable = 1;
	/* u1: enable signal for Boost filter on the tbl_matrix */

	Reg->reg_ldfw_boost_gain = 64;
	/* u8: boost gain for the region that is
	 *	larger than the average, norm to 16 as "1"
	 */

	Reg->reg_ldfw_enable = 1;

	Reg->reg_ldfw_hist_valid_ofst = 63;/* u8, hist valid bin upward offset*/

	Reg->reg_ldfw_hist_valid_rate = 64;
	/* u8, norm to 512 as "1", if hist_matrix[i]>(rate*histavg)>>9 */

	Reg->reg_ldfw_sedglit_RL = 1;/*u1: single edge lit right/bottom mode*/

	Reg->reg_ldfw_sf_enable = 1;
	/* u1: enable signal for spatial filter on the tbl_matrix */

	Reg->reg_ldfw_sf_thrd = 1600;
	/*u12: threshold of difference to enable the sf;*/

	Reg->reg_ldfw_sta_hdg_vflt = 1;

	for (T = 0; T < 8; T++)
		Reg->reg_ldfw_sta_hdg_weight[T] = 64;

	Reg->reg_ldfw_sta_max_hist_mode = 0;
	/* u2: mode of reference max/hist mode:
	 *	0: MIN(max, hist), 1: MAX(max, hist) 2: (max+hist)/2,
	 *	3: (max(a,b)*3 + min(a,b))/4
	 */

	Reg->reg_ldfw_sta_max_mode = 3;
	/* u2: maximum selection for components:
	 *	0: r_max, 1: g_max, 2: b_max; 3: max(r,g,b)
	 */

	Reg->reg_ldfw_sta_norm         = 128;
	Reg->reg_ldfw_sta_norm_rs      = 5;

	Reg->reg_ldfw_tf_alpha_ofst = 32;
	/* u8: ofset to alpha SFB_BL_matrix from last frame difference;*/

	Reg->reg_ldfw_tf_alpha_rate = 16;
	/*u8: rate to SFB_BL_matrix from last frame difference;*/

	Reg->reg_ldfw_tf_disable_th = 255;
	/* u8: 4x is the threshod to disable tf to the alpha
	 *	(SFB_BL_matrix from last frame difference;
	 */

	Reg->reg_ldfw_tf_enable = 1;

	Vnum = Reg->reg_LD_BLK_Vnum;
	Hnum = Reg->reg_LD_BLK_Hnum;
	BSIZE = Vnum*Hnum;
	/*Initialization */
	LD_IntialData(Reg->BL_matrix, BSIZE, 4095);

	/* BackLight Modeling control register setting*/
	Reg->reg_LD_BackLit_Xtlk = 1;
	/* u1: 0 no block to block Xtalk model needed;	 1: Xtalk model needed*/
	Reg->reg_LD_BackLit_mode = 1;
	/* u2: 0- LEFT/RIGHT Edge Lit;
	 *	1- Top/Bot Edge Lit; 2 - DirectLit modeled
	 *	H/V independent; 3- DirectLit modeled HV Circle distribution
	 */
	Reg->reg_LD_Reflect_Hnum = 3;
	/*u3: numbers of band reflection considered in Horizontal
	 *		direction; 0~4
	 */
	Reg->reg_LD_Reflect_Vnum = 0;
	/*u3: numbers of band reflection considered in Horizontal
	 *		direction; 0~4
	 */
	Reg->reg_LD_BkLit_curmod = 0;
	/*u1: 0: H/V separately, 1 Circle distribution*/
	Reg->reg_LD_BkLUT_Intmod = 1;
	/*u1: 0: linear interpolation, 1 cubical interpolation*/
	Reg->reg_LD_BkLit_Intmod = 1;
	/*u1: 0: linear interpolation, 1 cubical interpolation*/
	Reg->reg_LD_BkLit_LPFmod = 7;
	/*u3: 0: no LPF, 1:[1 14 1]/16;2:[1 6 1]/8; 3: [1 2 1]/4;
	 *		4:[9 14 9]/32  5/6/7: [5 6 5]/16;
	 */
	Reg->reg_LD_BkLit_Celnum = 121;
	/*u8:0:1920~61####((Reg->reg_LD_pic_ColMax+1)/32)+1;*/
	Reg->reg_BL_matrix_AVG = 3167;
	/* u12: DC of whole picture BL to be subtract from BL_matrix
	 *	during modeling (Set by FW daynamically)
	 */
	Reg->reg_BL_matrix_Compensate = 3167;
	/*	u12: DC of whole picture BL to be compensated back to
	 *	Litfull after the model (Set by FW dynamically);
	 */
	LD_IntialData(Reg->reg_LD_Reflect_Hdgr, 20, 32);
	/*20*u6: cells 1~20 for H Gains of different dist of Left/Right;*/
	LD_IntialData(Reg->reg_LD_Reflect_Vdgr, 20, 32);
	/*20*u6: cells 1~20 for V Gains of different dist of Top/Bot; */
	LD_IntialData(Reg->reg_LD_Reflect_Xdgr, 4, 32);/*  4*u6: */

	Reg->reg_LD_Vgain	= 256;/* u12 */
	Reg->reg_LD_Hgain	= 128;/* u12 */
	Reg->reg_LD_Litgain = 230;/* u12 */
	Reg->reg_LD_Litshft = 3;
	/* u3	right shif of bits for the all Lit's sum */
	LD_IntialData(Reg->reg_LD_BkLit_valid, 32, 1);
	/*u1x32: valid bits for the 32 cell Bklit to contribut to current
	 *	position (refer to the backlit padding pattern)
	 * region division index  1  2	3	4 5(0) 6(1) 7(2) 8(3) 9(4)
	 *	10(5)11(6)12(7)13(8) 14(9)15(10) 16   17   18	19
	 */
	for (T = 0; T < LD_BLK_LEN_H; T++)
		Reg->reg_LD_BLK_Hidx[T] = LD_BLK_Hidx[T];/* S14* BLK_LEN_H */
	for (T = 0; T < LD_BLK_LEN_V; T++)
		Reg->reg_LD_BLK_Vidx[T] = LD_BLK_Vidx[T];/* S14x BLK_LEN_V */

	for (j = 0; j < 8; j++) {
		for (i = 0; i < 32; i++) {
			Reg->reg_LD_LUT_Hdg_TXLX[j][i] =
				reg_LD_LUT_Hdg_TXLX[j][i];
		}
	}

	for (j = 0; j < 8; j++) {
		for (i = 0; i < 32; i++) {
			Reg->reg_LD_LUT_Vdg_TXLX[j][i] =
				reg_LD_LUT_Vdg_TXLX[j][i];
		}
	}

	for (j = 0; j < 8; j++) {
		for (i = 0; i < 32; i++) {
			Reg->reg_LD_LUT_VHk_TXLX[j][i] =
				reg_LD_LUT_VHk_TXLX[j][i];
		}
	}
	/* led LUT only choose fist one */
	for (i = 0; i < 16*24; i++)
		Reg->reg_LD_LUT_Id[i]  = 0;
	/* set the VHk_pos and VHk_neg value ,normalized to
	 *	128 as "1" 20150428
	 */
	for (T = 0; T < 32; T++) {
		Reg->reg_LD_LUT_VHk_pos[T] = 128;/* vdist>=0 */
		Reg->reg_LD_LUT_VHk_neg[T] = 128;/* vdist<0 */
		Reg->reg_LD_LUT_HHk[T] = 128;/* hdist gain */
		Reg->reg_LD_LUT_VHo_pos[T] = reg_LD_LUT_VHo_pos_TXLX[T];
		Reg->reg_LD_LUT_VHo_neg[T] = reg_LD_LUT_VHo_neg_TXLX[T];
	}
	Reg->reg_LD_LUT_VHo_LS = 0;

	for (i = 0; i < 8; i++) {
		Reg->reg_LD_LUT_Hdg_LEXT_TXLX[i] = reg_LD_LUT_Hdg_LEXT_TXLX[i];
		Reg->reg_LD_LUT_Vdg_LEXT_TXLX[i] = reg_LD_LUT_Vdg_LEXT_TXLX[i];
		Reg->reg_LD_LUT_VHk_LEXT_TXLX[i] = reg_LD_LUT_VHk_LEXT_TXLX[i];
	}

	/* set the demo window */
	Reg->reg_LD_xlut_demo_roi_xstart = (Reg->reg_LD_pic_ColMax / 4);
	     /* u14 start col index of the region of interest */
	Reg->reg_LD_xlut_demo_roi_xend = (Reg->reg_LD_pic_ColMax * 3 / 4);
	  /* u14 end col index of the region of interest */
	Reg->reg_LD_xlut_demo_roi_ystart = (Reg->reg_LD_pic_RowMax / 4);
	     /* u14 start row index of the region of interest */
	Reg->reg_LD_xlut_demo_roi_yend = (Reg->reg_LD_pic_RowMax * 3 / 4);
	   /*  u14 end row index of the region of interest */
	Reg->reg_LD_xlut_iroi_enable = 1;
	     /*  u1: enable rgb LUT remapping inside regon of interest:
	      *	0: no rgb remapping; 1: enable rgb remapping
	      */
	Reg->reg_LD_xlut_oroi_enable = 1;
	    /* u1: enable rgb LUT remapping outside regon of interest:
	     * 0: no rgb remapping; 1: enable rgb remapping
	     */
	/*  Registers used in LD_RGB_LUT for RGB remaping */
	Reg->reg_LD_RGBmapping_demo = 0;
	/* u2: 0 no demo mode 1: display BL_fulpel on RGB */
	Reg->reg_LD_X_LUT_interp_mode[0] = 0;
	 /* U1 0: using linear interpolation between to neighbour LUT;
	  *	1: use the nearest LUT results
	  */
	Reg->reg_LD_X_LUT_interp_mode[1] = 0;
	 /*  U1 0: using linear interpolation between to neighbour LUT;
	  *	1: use the nearest LUT results
	  */
	Reg->reg_LD_X_LUT_interp_mode[2] = 0;
	 /* U1 0: using linear interpolation between to neighbour LUT;
	  *	 1: use the nearest LUT results
	  */
	LD_LUTInit(Reg);
}


#if 1
void LD_ConLDReg(struct LDReg *Reg)
{
	struct aml_bl_drv_s *bl_drv = aml_bl_get_driver();

	switch (bl_drv->data->chip_type) {
	case BL_CHIP_TXLX:
		ConLDReg_TXLX(Reg);
		break;
	case BL_CHIP_GXTVBB:
		ConLDReg_GXTVBB(Reg);
		break;
	default:
		break;
	}
}

#define LD_ONESIDE    1
	/* 0: left/top side,
	 *	1: right/bot side, others: non-one-side
	 */
void ld_fw_cfg_once(struct LDReg *nPRM)
{
	int k, dlt, j, i;
	int hofst = 4;
	int vofst = 4;
	int Hnrm = 256;/* Hgain norm (256: for Hlen==2048) */
	int Vnrm = 256;/* Vgain norm (256: for Vlen==2048),related to VHk */

	int drt_LD_LUT_dg[] = {254, 248, 239, 226, 211, 194, 176, 156, 137,
				119, 101, 85, 70, 57, 45, 36, 28, 21, 16,
				12, 9, 6, 4, 3, 2, 1, 1, 1, 0, 0, 0, 0};

	/* demo mode to show the Lit map */
	nPRM->reg_LD_RGBmapping_demo = 0;

	/*  set Reg->reg_LD_BkLit_Celnum */
	nPRM->reg_LD_BkLit_Celnum = (nPRM->reg_LD_pic_ColMax + 63) / 32;

	/* set same region division for statistics */
	/*nPRM->reg_LD_STA_Vnum  = nPRM->reg_LD_BLK_Vnum;*/
	/*nPRM->reg_LD_STA_Hnum  = nPRM->reg_LD_BLK_Hnum;*/

	/* STA1max_Hidx */
	nPRM->reg_LD_STA1max_Hidx[0] = 0;
	for (k = 1; k < LD_STA_LEN_H; k++) {
		nPRM->reg_LD_STA1max_Hidx[k] = ((nPRM->reg_LD_pic_ColMax +
			(nPRM->reg_LD_STA_Hnum) - 1) /
			(nPRM->reg_LD_STA_Hnum)) * k;
		if (nPRM->reg_LD_STA1max_Hidx[k] > 4095)
			nPRM->reg_LD_STA1max_Hidx[k] = 4095;/* clip U12 */
	}
	/* STA1max_Vidx */
	nPRM->reg_LD_STA1max_Vidx[0] = 0;
	for (k = 1; k < LD_STA_LEN_V; k++) {
		nPRM->reg_LD_STA1max_Vidx[k] = ((nPRM->reg_LD_pic_RowMax +
			(nPRM->reg_LD_STA_Vnum) - 1) /
			(nPRM->reg_LD_STA_Vnum)) * k;
		if (nPRM->reg_LD_STA1max_Vidx[k] > 4095)
			nPRM->reg_LD_STA1max_Vidx[k] = 4095;/* clip to U12 */
	}
	/* config LD_STA2max_H/Vidx/LD_STAhist_H/Vidx */
	for (k = 0; k < LD_STA_LEN_H; k++) {
		nPRM->reg_LD_STA2max_Hidx[k] = nPRM->reg_LD_STA1max_Hidx[k];
		nPRM->reg_LD_STAhist_Hidx[k] = nPRM->reg_LD_STA1max_Hidx[k];
	}
	for (k = 0; k < LD_STA_LEN_V; k++) {
		nPRM->reg_LD_STA2max_Vidx[k] = nPRM->reg_LD_STA1max_Vidx[k];
		nPRM->reg_LD_STAhist_Vidx[k] = nPRM->reg_LD_STA1max_Vidx[k];
	}
	if (nPRM->reg_LD_BackLit_mode == 0) {/* Left/right EdgeLit */
		/*  set reflect num */
		nPRM->reg_LD_Reflect_Hnum = 0;
		/* u3: numbers of band reflection considered in Horizontal
		 *	direction; 0~4
		 */
		nPRM->reg_LD_Reflect_Vnum = 3;
		/* u3: numbers of band reflection considered in Horizontal
		 *	direction; 0~4
		 */
		/* config reg_LD_BLK_Hidx */
		for (k = 0; k < LD_BLK_LEN_H; k++) {
			dlt = nPRM->reg_LD_pic_ColMax /
				(nPRM->reg_LD_BLK_Hnum) * 2;
#if (LD_ONESIDE == 1) /* bot/right one side */
			nPRM->reg_LD_BLK_Hidx[k] = 0 + (k-hofst) * dlt;
#else
			nPRM->reg_LD_BLK_Hidx[k] = (-1) * (dlt / 2) +
				(k - hofst) * dlt;
#endif
			nPRM->reg_LD_BLK_Hidx[k] = (nPRM->reg_LD_BLK_Hidx[k] >
				8191) ? 8191 : ((nPRM->reg_LD_BLK_Hidx[k] <
				(-8192)) ? (-8192) :
				(nPRM->reg_LD_BLK_Hidx[k]));/* Clip to S14 */
		}
		/*  config reg_LD_BLK_Vidx */
		for (k = 0; k < LD_BLK_LEN_V; k++) {
			dlt = (nPRM->reg_LD_pic_RowMax) /
				(nPRM->reg_LD_BLK_Vnum);
			nPRM->reg_LD_BLK_Vidx[k] = 0 + (k - vofst) * dlt;
			nPRM->reg_LD_BLK_Vidx[k] = (nPRM->reg_LD_BLK_Vidx[k] >
				8191) ? 8191 : ((nPRM->reg_LD_BLK_Vidx[k] <
				(-8192)) ? (-8192) :
				(nPRM->reg_LD_BLK_Vidx[k]));/*Clip to S14*/
		}
		/*  configure  Hgain/Vgain */
		nPRM->reg_LD_Hgain = (Hnrm*2048 / (nPRM->reg_LD_pic_ColMax));
		nPRM->reg_LD_Vgain = (Vnrm*2048 / (nPRM->reg_LD_pic_RowMax));
		nPRM->reg_LD_Hgain = (nPRM->reg_LD_Hgain >
			4095)  ? 4095 : (nPRM->reg_LD_Hgain);
		nPRM->reg_LD_Vgain = (nPRM->reg_LD_Vgain >
			4095)  ? 4095 : (nPRM->reg_LD_Vgain);

		/* if one side led, set the Hdg/Vdg/VHk differently */
		if (nPRM->reg_LD_BLK_Hnum == 1) {
			for (k = 0; k < LD_LUT_LEN; k++) {
				nPRM->reg_LD_LUT_Hdg[k] = LD_LUT_Hdg1[k];
				nPRM->reg_LD_LUT_Vdg[k] = LD_LUT_Vdg1[k];
				nPRM->reg_LD_LUT_VHk[k] = LD_LUT_VHk1[k];
				nPRM->reg_LD_LUT_VHo_neg[k] = LD_LUT_VHo_neg[k];
				nPRM->reg_LD_LUT_VHo_pos[k] = LD_LUT_VHo_pos[k];
			}
			nPRM->reg_LD_LUT_Hdg_LEXT = 2 * nPRM->reg_LD_LUT_Hdg[0]
				- nPRM->reg_LD_LUT_Hdg[1];
			nPRM->reg_LD_LUT_Vdg_LEXT = 2 * nPRM->reg_LD_LUT_Vdg[0]
				- nPRM->reg_LD_LUT_Vdg[1];
			nPRM->reg_LD_LUT_VHk_LEXT = 2 * nPRM->reg_LD_LUT_VHk[0]
				- nPRM->reg_LD_LUT_VHk[1];
			nPRM->reg_LD_Litgain = 230;
			/* u12 will be adjust according to pannel */
			/* specially for TXLX config of one
			 * side led mode(mode=1)--kite1023
			 */
			for (k = 0; k < LD_LUT_LEN; k++) {
				/* group 2 as one led lut--kite1023 */
				nPRM->reg_LD_LUT_Hdg_TXLX[1][k] =
					LD_LUT_Hdg1_TXLX[k];
				nPRM->reg_LD_LUT_Vdg_TXLX[1][k] =
					LD_LUT_Vdg1_TXLX[k];
				nPRM->reg_LD_LUT_VHk_TXLX[1][k] =
					LD_LUT_VHk1_TXLX[k];
				/* same with gxtvbb,default is ok */
				nPRM->reg_LD_LUT_VHo_neg[k] = LD_LUT_VHo_neg[k];
				nPRM->reg_LD_LUT_VHo_pos[k] = LD_LUT_VHo_pos[k];
			}
			for (j = 0; j < 8; j++) {
				nPRM->reg_LD_LUT_Hdg_LEXT_TXLX[j] =
					2*(nPRM->reg_LD_LUT_Hdg_TXLX[1][0]) -
					(nPRM->reg_LD_LUT_Hdg_TXLX[1][1]);
				nPRM->reg_LD_LUT_Vdg_LEXT_TXLX[j] =
					2*(nPRM->reg_LD_LUT_Vdg_TXLX[1][0]) -
					(nPRM->reg_LD_LUT_Vdg_TXLX[1][1]);
				nPRM->reg_LD_LUT_VHk_LEXT_TXLX[j] =
					2*(nPRM->reg_LD_LUT_VHk_TXLX[1][0]) -
					(nPRM->reg_LD_LUT_VHk_TXLX[1][1]);
			}
			nPRM->reg_LD_Litgain = 230;
			/* led LUT only choose group 2--kite1023 */
			for (i = 0; i < 16*24; i++)
				nPRM->reg_LD_LUT_Id[i]  = 1;
		}
	} else if (nPRM->reg_LD_BackLit_mode == 1) {/* Top/Bot EdgeLit */
		/* set reflect num */
		nPRM->reg_LD_Reflect_Hnum = 3;
		/* u3: numbers of band reflection considered in Horizontal
		 *	direction; 0~4
		 */
		nPRM->reg_LD_Reflect_Vnum = 0;
		/* u3: numbers of band reflection considered in Horizontal
		 *	direction; 0~4
		 */
		/* config reg_LD_BLK_Hidx */
		for (k = 0; k < LD_BLK_LEN_H; k++) {
			dlt = nPRM->reg_LD_pic_ColMax / nPRM->reg_LD_BLK_Hnum;
			nPRM->reg_LD_BLK_Hidx[k] = 0 + (k - hofst) * dlt;
			nPRM->reg_LD_BLK_Hidx[k] = (nPRM->reg_LD_BLK_Hidx[k] >
				8191) ? 8191 : ((nPRM->reg_LD_BLK_Hidx[k] <
				(-8192)) ? (-8192) :
				(nPRM->reg_LD_BLK_Hidx[k]));/*Clip to S14*/
		}
		/* config reg_LD_BLK_Vidx */
		for (k = 0; k < LD_BLK_LEN_V; k++) {
			dlt = nPRM->reg_LD_pic_RowMax /
				(nPRM->reg_LD_BLK_Vnum) * 2;
#if (LD_ONESIDE == 1) /*  bot/right one side */
			nPRM->reg_LD_BLK_Vidx[k] = 0 + (k-vofst) * dlt;
#else
			nPRM->reg_LD_BLK_Vidx[k] = (-1) * (dlt / 2) +
				(k - vofst) * dlt;
#endif
			nPRM->reg_LD_BLK_Vidx[k] = (nPRM->reg_LD_BLK_Vidx[k] >
				8191) ? 8191 : ((nPRM->reg_LD_BLK_Vidx[k] <
				(-8192)) ? (-8192) :
				(nPRM->reg_LD_BLK_Vidx[k])); /* Clip to S14*/
		}
		/*  configure  Hgain/Vgain */
		nPRM->reg_LD_Hgain = (Hnrm*2048 / (nPRM->reg_LD_pic_RowMax));
		nPRM->reg_LD_Vgain = (Vnrm*2048 / (nPRM->reg_LD_pic_ColMax));
		nPRM->reg_LD_Hgain = (nPRM->reg_LD_Hgain >
			4095) ? 4095 : (nPRM->reg_LD_Hgain);
		nPRM->reg_LD_Vgain = (nPRM->reg_LD_Vgain >
			4095) ? 4095 : (nPRM->reg_LD_Vgain);

		/* if one side led, set the Hdg/Vdg/VHk differently */
		if (nPRM->reg_LD_BLK_Vnum == 1) {
			for (k = 0; k < LD_LUT_LEN; k++) {
				nPRM->reg_LD_LUT_Hdg[k] = LD_LUT_Hdg1[k];
				nPRM->reg_LD_LUT_Vdg[k] = LD_LUT_Vdg1[k];
				nPRM->reg_LD_LUT_VHk[k] = LD_LUT_VHk1[k];
				nPRM->reg_LD_LUT_VHo_neg[k] = LD_LUT_VHo_neg[k];
				nPRM->reg_LD_LUT_VHo_pos[k] = LD_LUT_VHo_pos[k];
			}
			nPRM->reg_LD_LUT_Hdg_LEXT = 2 * nPRM->reg_LD_LUT_Hdg[0]
				- nPRM->reg_LD_LUT_Hdg[1];
			nPRM->reg_LD_LUT_Vdg_LEXT = 2 * nPRM->reg_LD_LUT_Vdg[0]
				- nPRM->reg_LD_LUT_Vdg[1];
			nPRM->reg_LD_LUT_VHk_LEXT = 2 * nPRM->reg_LD_LUT_VHk[0]
				- nPRM->reg_LD_LUT_VHk[1];
			/* specially for TXLX config--kite1023 */
			for (k = 0; k < LD_LUT_LEN; k++) {
				/* group 2 as one led lut--kite1023 */
				nPRM->reg_LD_LUT_Hdg_TXLX[1][k] =
					LD_LUT_Hdg1_TXLX[k];
				nPRM->reg_LD_LUT_Vdg_TXLX[1][k] =
					LD_LUT_Vdg1_TXLX[k];
				nPRM->reg_LD_LUT_VHk_TXLX[1][k] =
					LD_LUT_VHk1_TXLX[k];
				/* same with gxtvbb,default is ok */
				nPRM->reg_LD_LUT_VHo_neg[k] = LD_LUT_VHo_neg[k];
				nPRM->reg_LD_LUT_VHo_pos[k] = LD_LUT_VHo_pos[k];
			}
			 /* 8 led lut */
			for (j = 0; j < 8; j++) {
				nPRM->reg_LD_LUT_Hdg_LEXT_TXLX[j] =
					2*(nPRM->reg_LD_LUT_Hdg_TXLX[1][0]) -
					(nPRM->reg_LD_LUT_Hdg_TXLX[1][1]);
				nPRM->reg_LD_LUT_Vdg_LEXT_TXLX[j] =
					2*(nPRM->reg_LD_LUT_Vdg_TXLX[1][0]) -
					(nPRM->reg_LD_LUT_Vdg_TXLX[1][1]);
				nPRM->reg_LD_LUT_VHk_LEXT_TXLX[j] =
					2*(nPRM->reg_LD_LUT_VHk_TXLX[1][0]) -
					(nPRM->reg_LD_LUT_VHk_TXLX[1][1]);
			}
			/*led LUT only choose group */
			for (i = 0; i < 16*24; i++)
				nPRM->reg_LD_LUT_Id[i]  = 1;
			nPRM->reg_LD_Litgain = 256;
		}
	} else {/* DirectLit */
		/*  set reflect num */
		nPRM->reg_LD_Reflect_Hnum = 2;
		/* u3: numbers of band reflection considered in Horizontal
		 *	direction; 0~4
		 */
		nPRM->reg_LD_Reflect_Vnum = 2;
		/* u3: numbers of band reflection considered in Horizontal
		 *	direction; 0~4
		 */
		/* config reg_LD_BLK_Hidx */
		for (k = 0; k < LD_BLK_LEN_H; k++) {
			dlt = nPRM->reg_LD_pic_ColMax / nPRM->reg_LD_BLK_Hnum;
			nPRM->reg_LD_BLK_Hidx[k] = 0 + (k - hofst) * dlt;
			nPRM->reg_LD_BLK_Hidx[k] = (nPRM->reg_LD_BLK_Hidx[k] >
				8191) ? 8191 : ((nPRM->reg_LD_BLK_Hidx[k] <
				(-8192)) ? (-8192) :
				(nPRM->reg_LD_BLK_Hidx[k]));/*Clip to S14*/
		}
		/* config reg_LD_BLK_Vidx */
		for (k = 0; k < LD_BLK_LEN_V; k++) {
			dlt = nPRM->reg_LD_pic_RowMax / nPRM->reg_LD_BLK_Vnum;
			nPRM->reg_LD_BLK_Vidx[k] = 0 + (k - vofst) * dlt;
			nPRM->reg_LD_BLK_Vidx[k] = (nPRM->reg_LD_BLK_Vidx[k] >
				8191) ? 8191 : ((nPRM->reg_LD_BLK_Vidx[k] <
				(-8192)) ? (-8192) :
				(nPRM->reg_LD_BLK_Vidx[k]));/*Clip to S14*/
		}
		/* configure  Hgain/Vgain */
		nPRM->reg_LD_Hgain = ((nPRM->reg_LD_BLK_Hnum) * 73 * 2048 /
			(nPRM->reg_LD_pic_ColMax));
		nPRM->reg_LD_Vgain = ((nPRM->reg_LD_BLK_Vnum) * 73 * 2048 /
			(nPRM->reg_LD_pic_RowMax));
		nPRM->reg_LD_Hgain = (nPRM->reg_LD_Hgain >
			4095) ? 4095 : (nPRM->reg_LD_Hgain);
		nPRM->reg_LD_Vgain = (nPRM->reg_LD_Vgain >
			4095) ? 4095 : (nPRM->reg_LD_Vgain);

		/*  configure */
		for (k = 0; k < LD_LUT_LEN; k++) {
			nPRM->reg_LD_LUT_Hdg[k] = drt_LD_LUT_dg[k];
			nPRM->reg_LD_LUT_Vdg[k] = drt_LD_LUT_dg[k];
			nPRM->reg_LD_LUT_VHk[k] = 128;
			/*config once to make sure */
			nPRM->reg_LD_LUT_VHk_neg[k] = 128;
			nPRM->reg_LD_LUT_VHk_pos[k] = 128;
			/* nPRM->reg_LD_LUT_VHo_neg =
			 * nPRM->reg_LD_LUT_VHo_neg_TXLX
			 *	register same with gxtvbb
			 */
			nPRM->reg_LD_LUT_VHo_neg[k] = 0;
			/* nPRM->reg_LD_LUT_VHo_pos_TXLX */
			nPRM->reg_LD_LUT_VHo_pos[k] = 0;
		}
		/* just for confirm--kite1023 */
		for (j = 0; j < 8; j++) {
			for (k = 0; k < LD_LUT_LEN; k++) {
				nPRM->reg_LD_LUT_Hdg_TXLX[j][k] =
					reg_LD_LUT_Hdg_TXLX[j][k];
				nPRM->reg_LD_LUT_Vdg_TXLX[j][k] =
					reg_LD_LUT_Vdg_TXLX[j][k];
				nPRM->reg_LD_LUT_VHk_TXLX[j][k] =
					reg_LD_LUT_VHk_TXLX[j][k];
			}
		}
		nPRM->reg_LD_LUT_VHo_LS = 0;
		nPRM->reg_LD_LUT_Hdg_LEXT = 2 * (nPRM->reg_LD_LUT_Hdg[0])
			- (nPRM->reg_LD_LUT_Hdg[1]);
		nPRM->reg_LD_LUT_Vdg_LEXT = 2 * (nPRM->reg_LD_LUT_Vdg[0])
			- (nPRM->reg_LD_LUT_Vdg[1]);
		nPRM->reg_LD_LUT_VHk_LEXT = 2 * (nPRM->reg_LD_LUT_VHk[0])
			- (nPRM->reg_LD_LUT_VHk[1]);

		/*led LUT only choose fist one-- make confirm --kite1023 */
		for (i = 0; i < 16*24; i++)
			nPRM->reg_LD_LUT_Id[i]  = 0;
		for (j = 0; j < 8; j++) { /*8 led lut */
			nPRM->reg_LD_LUT_Hdg_LEXT_TXLX[j] =
				2*(nPRM->reg_LD_LUT_Hdg_TXLX[0][0]) -
				(nPRM->reg_LD_LUT_Hdg_TXLX[0][1]);/*260*/
			nPRM->reg_LD_LUT_Vdg_LEXT_TXLX[j] =
				2*(nPRM->reg_LD_LUT_Vdg_TXLX[0][0]) -
				(nPRM->reg_LD_LUT_Vdg_TXLX[0][1]);/*260*/
			nPRM->reg_LD_LUT_VHk_LEXT_TXLX[j] =
				2*(nPRM->reg_LD_LUT_VHk_TXLX[0][0]) -
				(nPRM->reg_LD_LUT_VHk_TXLX[0][1]);/*128*/
		}
		nPRM->reg_LD_Litgain = 256;
		nPRM->reg_LD_BkLit_curmod = 0;
	}
	/* set one time nPRM here */
	nPRM->reg_LD_STA1max_LPF = 1;
	/* u1: STA1max statistics on [1 2 1]/4 filtered results */
	nPRM->reg_LD_STA2max_LPF = 1;
	/* u1: STA2max statistics on [1 2 1]/4 filtered results */
	nPRM->reg_LD_STAhist_LPF = 1;
	/*u1: STAhist statistics on [1 2 1]/4 filtered results */
	nPRM->reg_LD_X_LUT_interp_mode[0] = 0;
	nPRM->reg_LD_X_LUT_interp_mode[1] = 0;
	nPRM->reg_LD_X_LUT_interp_mode[2] = 0;
	/* set the demo window*/
	nPRM->reg_LD_xlut_demo_roi_xstart = (nPRM->reg_LD_pic_ColMax / 4);
	/* u14 start col index of the region of interest */
	nPRM->reg_LD_xlut_demo_roi_xend   = (nPRM->reg_LD_pic_ColMax * 3 / 4);
	/* u14 end col index of the region of interest */
	nPRM->reg_LD_xlut_demo_roi_ystart = (nPRM->reg_LD_pic_RowMax / 4);
	/* u14 start row index of the region of interest */
	nPRM->reg_LD_xlut_demo_roi_yend  = (nPRM->reg_LD_pic_RowMax * 3 / 4);
	/* u14 end row index of the region of interest */
	nPRM->reg_LD_xlut_iroi_enable = 1;
	/* u1: enable rgb LUT remapping inside regon of interest:
	 *	 0: no rgb remapping; 1: enable rgb remapping
	 */
	nPRM->reg_LD_xlut_oroi_enable = 1;
	/* u1: enable rgb LUT remapping outside regon of interest:
	 *	 0: no rgb remapping; 1: enable rgb remapping
	 */
}
#endif

