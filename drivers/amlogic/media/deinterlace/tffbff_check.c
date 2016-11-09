/*
 * drivers/amlogic/media/deinterlace/tffbff_check.c
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

#include <linux/module.h>
#include <linux/types.h>
#include "deinterlace.h"
#include "register.h"

/* define the length of history */
#define TBFF_DLEN 20

/* parameters */
/* valid data: only pixel_num > size*rat >> 8 */
/* u8: [0,255] */
static int tbff_pixel_minnum = 16;

/* x3 / row > rat */
/* => x3 > (row * rat >> 5) */
/* u10: [0 ~ 1023] => [0, 16] */
static int tbff_x3_minrow = 64;

/* m1/p1/m2/p2 changing 0/1/0/1... as 2-2 mode */
/* (max)/ min > (rat): define it max, else min */
/* u8: [0,31] */
static int tbff_mp_lgrat[4] = {16, 16, 16, 16};

/* tbff check length */
/* [0, 31] */
static int tbff_chk_len[4] = {4, 4, 4, 4};

static int calc_tbff_pixel_minnum = 16;
static int calc_tbff_x3_minrow = 64;
static int calc_tbff_mp_lgrat[4] = {16, 16, 16, 16};

static int nTmp0;
static bool tbff_pr, tffbff_en = true;
static int mode_count;

void tbff_init(void)
{
	int i = 0;

	mode_count = 0;
	calc_tbff_pixel_minnum = tbff_pixel_minnum;
	calc_tbff_x3_minrow = tbff_x3_minrow;
	for (i = 0; i < 4; i++)
		calc_tbff_mp_lgrat[i] = tbff_mp_lgrat[i];
}
static int tbff_get_rate(int t0, int t1)
{
	int nmax = t1;
	int nmin = t0;
	int nrst = 0;

	if (t0 > t1) {
		nmax = t0;
		nmin = t1;
	}

	nrst = (8 * nmax) + (nmin >> 1);
	nrst /= (nmin+1);
	if (nrst > 31)
		nrst = 31;

	if (nmax == t1) /* current is small */
		nrst = 0;

	return nrst;
}

static int tbff_get_minrate(int *pLen)
{
	int iT0 = 0;
	int aTLn[4] = {0, 0, 0, 0};
	int nrst = 255;

	int nP0 = pLen[TBFF_DLEN - 1];
	int nP1 = pLen[TBFF_DLEN - 2];

	if (nP0 < nP1) {
		for (iT0 = 0; iT0 < 4; (iT0 += 2)) {
			aTLn[iT0] = 31 - pLen[TBFF_DLEN - iT0 - 1];
			aTLn[iT0 + 1] = pLen[TBFF_DLEN - iT0 - 2];
		}
	} else {
		for (iT0 = 0; iT0 < 4; (iT0 += 2)) {
			aTLn[iT0] = pLen[TBFF_DLEN - iT0 - 1];
			aTLn[iT0 + 1] = 31 - pLen[TBFF_DLEN - iT0 - 2];
		}
	}

	for (iT0 = 0; iT0 < 4; iT0++) {
		if (aTLn[iT0] < nrst)
			nrst = aTLn[iT0];
	}

	return nrst;
}

/* update param according to detection count */
static void calc_tbff_param(int mode_cnt, int step_max)
{
	int delt_num, delt_row, delt_lgrat, i;

	delt_num = tbff_pixel_minnum - 4;
	delt_row = tbff_x3_minrow;
	delt_lgrat = tbff_mp_lgrat[0] - 6;

	calc_tbff_pixel_minnum =
		tbff_pixel_minnum - (delt_num * mode_cnt)/step_max;
	calc_tbff_x3_minrow = tbff_x3_minrow - (delt_row * mode_cnt)/step_max;
	calc_tbff_mp_lgrat[0] =
		tbff_mp_lgrat[0] - (delt_lgrat * mode_cnt)/step_max;

	for (i = 1; i < 4; i++)
		calc_tbff_mp_lgrat[i] = calc_tbff_mp_lgrat[0];
}
/* TFF / BFF error check */
/* polar3[0]: num of pixels */
/* polar3[1]: smooth motion vector */
/* polar3[2]: m1 */
/* polar3[3]: p1 */
/* polar3[4]: m2 */
/* polar3[5]: p2 */
/* polar3[6]: x3 */

int tff_bff_check(int nROW, int nCOL)
{
	int iT0 = 0, iT1 = 0, nTmp1 = 0;
	int nTmp2 = 0, tbfalse = 0, nSIZE = nROW * nCOL;
    /* int nMin0 = 0; */
	unsigned int uTp0 = 0;

	int polar3[7];
     /* top/bot field first error */
	static int pPolar3[7]; /* previous polar3 */
	static int sDat[4][TBFF_DLEN]; /* history infor */
	static unsigned int stc_pp3[4];

	for (iT0 = 0; iT0 < 7; iT0++)
		polar3[iT0] = Rd_reg_bits(NR2_RO_POLAR3_NUMOFPIX+iT0, 0, 24);

	if (tbff_pr)
		pr_info("polar3=%8d %8d %8d %8d %8d %8d %8d.\n",
				polar3[0], polar3[1], polar3[2], polar3[3],
				polar3[4], polar3[5], polar3[6]);
	/* history */
	for (iT0 = 0; iT0 < 4; iT0++) {
		for (iT1 = 0; iT1 < (TBFF_DLEN - 1); iT1++)
			sDat[iT0][iT1] = sDat[iT0][iT1 + 1];

		stc_pp3[iT0] = (stc_pp3[iT0] << 1);
		sDat[iT0][TBFF_DLEN - 1] = 0;
	}

	/* only valid pixel is enough */
	nTmp0 = ((nSIZE * calc_tbff_pixel_minnum + 128) >> 8);
	if (polar3[0] > nTmp0) {
		/* only x3 pixel is enough */
		nTmp1 = ((nROW * calc_tbff_x3_minrow + 16) >> 5);
		if (polar3[6] > nTmp1) {
			/* nMin0 = 255; */
			for (iT1 = 0; iT1 < 4; iT1++) {
				nTmp2 =
			tbff_get_rate(polar3[iT1 + 2], pPolar3[iT1 + 2]);
				sDat[iT1][TBFF_DLEN - 1] = nTmp2;
			}
		}
	}

	for (iT1 = 0; iT1 < 4; iT1++) {
		nTmp2 = tbff_get_minrate(sDat[iT1]);
		if (nTmp2 >= calc_tbff_mp_lgrat[iT1])
			stc_pp3[iT1] |= 0x1;
	}

	for (iT1 = 0; iT1 < 4; iT1++) {
		uTp0 = stc_pp3[iT1];
		nTmp0 = 0;
		for (iT0 = 0; iT0 < 31; iT0++) {
			if (uTp0 & 0x1)
				nTmp0++;
			else
				break;
			uTp0 = (uTp0 >> 1);
		}

		if (nTmp0 >= tbff_chk_len[iT1])
			tbfalse += 1;
	}

	/* m1/p1/m2/p2 all 2-2 mode */
	/* return (tbfalse==4); */
	nTmp0 = (tbfalse == 4);

	if (nTmp0 == 1)
		nTmp0 += (polar3[2] < pPolar3[2]);
	if (nTmp0 != 0)
		mode_count = mode_count > 32 ? 32 : (mode_count+1);
	else
		mode_count = mode_count > 0 ? (mode_count-1) : 0;
	calc_tbff_param(mode_count, 32);
	for (iT0 = 0; iT0 < 7; iT0++)
		pPolar3[iT0] = polar3[iT0];
	if (tbff_pr)
		pr_info("num %d, x3 %d, lgrat %d, result is %d.\n",
			calc_tbff_pixel_minnum, calc_tbff_x3_minrow,
			calc_tbff_mp_lgrat[0], nTmp0);

	return tffbff_en?nTmp0:0;
}
module_param_named(tffbff_en, tffbff_en, bool, 0664);
module_param_named(tbff_pixel_minnum, tbff_pixel_minnum, int, 0664);
module_param_named(tbff_x3_minrow, tbff_x3_minrow, int, 0664);
module_param_named(tbff_pr, tbff_pr, bool, 0664);
