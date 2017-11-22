/*
 * drivers/amlogic/media/deinterlace/film_mode_fmw/flm_mod_xx.c
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

#include <linux/kernel.h>
#include <linux/module.h>
#include "film_vof_soft.h"

/* dif02 < (size >> sft) => static */
int flm2224_stl_sft = 7; /*10*/
module_param(flm2224_stl_sft, int, 0644);
MODULE_PARM_DESC(flm2224_stl_sft, "flm2224_stl_sft");

int aMax02[15]; /* maximum 4 */
int aXId02[15];
int aMin02[15]; /* minimum 4 */
int aNId02[15];
int aXMI02[15];

int GetMaxNIdx(int *nMax4, int *nXId4, int N, int *nQt01, int nLen)
{
	int nT1 = 0;
	int nT3 = 0;
	int nT4 = 0;
	int nTmp = 0;

	for (nT1 = 0; nT1 <	N; nT1++) {
		nMax4[nT1] = 0;
		nXId4[nT1] = 0;
	}

	for (nT1 = 0; nT1 <	nLen; nT1++) {
		nTmp = nQt01[nT1];

		/* maximum */
		for (nT3 = 0; nT3 < N; nT3++) {
			if (nTmp > nMax4[nT3]) {
				for (nT4 = 3; nT4 >= nT3+1; nT4--) {
					nMax4[nT4] = nMax4[nT4-1];
					nXId4[nT4] = nXId4[nT4-1];
				}
				nMax4[nT3] = nTmp;
				nXId4[nT3] = nT1;
				break;
			}
		}
	}
	return 0;
}

int GetMinNIdx(int *nMax4, int *nXId4, int N, int *nQt01, int nLen)
{
	int nT1 = 0;
	int nT3 = 0;
	int nT4 = 0;
	int nTmp = 0;

	for (nT1 = 0; nT1 <	N; nT1++) {
		nMax4[nT1] = 17;
		nXId4[nT1] = 0;
	}

	for (nT1 = 0; nT1 <	nLen; nT1++) {
		nTmp = nQt01[nT1];

		/* minimum */
		for (nT3 = 0; nT3 < N; nT3++) {
			if (nTmp < nMax4[nT3]) {
				for (nT4 = 3; nT4 >= nT3+1; nT4--) {
					nMax4[nT4] = nMax4[nT4-1];
					nXId4[nT4] = nXId4[nT4-1];
				}
				nMax4[nT3] = nTmp;
				nXId4[nT3] = nT1;
				break;
			}
		}
	}
	return 0;
}

/* 15: 8-7 */
/* 12: 3-2-3-2-2 */
/* 10: 6-4 */
/* 10: 5-5 */
/* 10: 2-2-2-4 */
/* 10: 2-3-3-2 */
/* 10: 3-2-3-2 */
/* pulldown pattern number */
int	FlmModsDet(struct sFlmDatSt *pRDat, int nDif01, int nDif02)
{
	int iWidth  = pRDat->iWidth;
	int iHeight = pRDat->iHeight;
	int iSIZE = (iWidth * iHeight);
	int iMxDif  = (iSIZE >> 6);
	int nPrtLog[PDXX_PT_NUM] = {87, 32322, 64, 55, 2224, 2332, 3232};

	/* HISDETNUM hist */
	UINT8 *pModXx = pRDat->pModXx;
	UINT8 *pFlgXx = pRDat->pFlgXx; /* pre-1, nxt-0 */
	UINT8 *pLvlXx = pRDat->pLvlXx;  /* mode level */

	static unsigned int sModFlg01[PDXX_PT_NUM]; /* flags */
	static unsigned int sModFlg02[PDXX_PT_NUM]; /* flags */
	static int nModCnt[PDXX_PT_NUM]; /* mode counter */

	unsigned int tModFlg01[PDXX_PT_NUM]; /* current flags */
	unsigned int tModFlg02[PDXX_PT_NUM]; /* current flags */

	int nMxMn[PDXX_PT_NUM][2] = { {2, 4}, {-2, -2}, {2, 4},
	{2, 4}, {4, -2}, {4, -2}, {4, -2} };

	int nModLvl[PDXX_PT_NUM] = {0, 0, 0, 0, 0, 0, 0}; /* mode level */
	int nQt01[15];
	int nQt02[15];

	int aMax01[15]; /* maximum 4 */
	int aXId01[15];
	int aMin01[15]; /* minimum 4 */
	int aNId01[15];
	int aXMI01[15];

	int	nT0	= 0;
	int	nT1	= 0;
	int	nT2	= 0;
	int	nT3	= 0;
	int	nT4	= 0;

	int	tT0	= 0;
	int	tT1	= 0;
	int	tT2	= 0;
	int	tT3	= 0;

	unsigned int uT01 = 0;
	unsigned int uT02 = 0;
	unsigned int uT03 = 0;

	int	nS01[PDXX_PT_NUM]	= {0, 0, 0,	0, 0, 0, 0};
	int	nS02[PDXX_PT_NUM]	= {0, 0, 0,	0, 0, 0, 0};
	int	nStp[PDXX_PT_NUM]	= {15, 12, 10, 10, 10, 10};

	static int pDif01[30];
	static int pDif02[30];

	int nLen1 = 0;
	int nLen2 = 0;

	int	nMin01 = 0;
	int	nMax01 = 0;
	int	nMin02 = 0;
	int	nMax02 = 0;
	int tModLvl = 0;

	iMxDif = (iMxDif >> 4) + 1;

	prt_flg = ((pr_pd >> 4) & 0x1);
	if (prt_flg)
		sprintf(debug_str, "#DbgXx:\n");


	for (nT0 = 1; nT0 < HISDETNUM; nT0++) {
		pFlgXx[nT0 - 1] = pFlgXx[nT0];
		pLvlXx[nT0 - 1] = pLvlXx[nT0];
		pModXx[nT0 - 1] = pModXx[nT0];
	}

	for	(nT0 = 0; nT0 <	29;	nT0++) {
		pDif01[nT0]	= pDif01[nT0 + 1];
		pDif02[nT0]	= pDif02[nT0 + 1];
	}
	pDif01[29] = (nDif01 >> 6);
	pDif02[29] = (nDif02 >> 6);

	for	(nT0 = 0; nT0 <	3; nT0++) {
		nT2	= nStp[nT0];

		nT3	= pDif01[29];
		nT4	= pDif02[29];

		nMin01 = nT3;
		nMax01 = nT3;
		nMin02 = nT4;
		nMax02 = nT4;

		nT3 = pDif01[29] - pDif01[29 - nT2];
		nT4 = pDif02[29] - pDif02[29 - nT2];
		if (nT3 < 0)
			nT3 = -nT3;
		if (nT4 < 0)
			nT4 = -nT4;
		nS01[nT0] = nT3;
		nS02[nT0] = nT4;

		/* nS01, nS02: sum of difference */
		for	(nT1 = 1; nT1 <	nT2; nT1++)	{
			nT3	= pDif01[29	- nT1];
			nT4	= pDif02[29	- nT1];

			if (nT3 > nMax01)
				nMax01 = nT3;
			if (nT3 < nMin01)
				nMin01 = nT3;

			if (nT4 > nMax02)
				nMax02 = nT4;
			if (nT4 < nMin02)
				nMin02 = nT4;

			/* diff max */
			nT3 = pDif01[29	- nT1] - pDif01[29 - nT1 - nT2];
			nT4	= pDif02[29	- nT1] - pDif02[29 - nT1 - nT2];
			if (nT3 < 0)
				nT3 = -nT3;
			if (nT4 < 0)
				nT4 = -nT4;
			if (nT3 > nS01[nT0])
				nS01[nT0] = nT3;
			if (nT4 > nS02[nT0])
				nS02[nT0] = nT4;
		}

		for (nT1 = 0; nT1 <	nT2; nT1++)	{
			nT3 = pDif01[29 - nT1] - nMin01;
			nT4 = nMax01 - nMin01 + 32;
			nT3 = (16 * nT3) + (nT4 / 2);
			nQt01[nT1] = (nT3 / nT4);

			nT3 = pDif02[29 - nT1] - nMin02;
			nT4 = nMax02 - nMin02 + 32;
			nT3 = (16 * nT3) + (nT4 / 2);
			nQt02[nT1] = (nT3 / nT4);
		}

		if (nT0 == 2)
			tT2 = PDXX_PT_NUM - 2;
		else
			tT2 = 1;

		for (tT1 = 0; tT1 < tT2; tT1++) {
			tT0	= nT0 + tT1;

			tModLvl = ((nModCnt[tT0] + 2) >> 2);
			if (tModLvl > 64)
				tModLvl = 64;

			if (nS01[nT0] > nMax01)
				nT3 = 8;
			else {
				nT3 = (nS01[nT0] << 3);
				nT3 = nT3 + (nMax01 >> 1);
				nT3 = nT3 / (nMax01 + 1);
				if (nT3 > 8)
					nT3 = 8;
			}
			tModLvl -= nT3;

			if (nS02[nT0] > nMax02)
				nT4 = 8;
			else {
				nT4 = (nS02[nT0] << 3);
				nT4 = nT4 + (nMax02 >> 1);
				nT4 = nT4 / (nMax02 + 1);
				if (nT4 > 8)
					nT4 = 8;
			}
			tModLvl -= nT4;

			if (nMxMn[tT0][0] > 0) {
				nLen1 = nMxMn[tT0][0];
				GetMaxNIdx(aMax01, aXId01, nLen1, nQt01, nT2);

				nT4 = 0;
				for (tT3 = 0; tT3 < nLen1; tT3++) {
					aXMI01[tT3] = aXId01[tT3];
					nT4 += (16 - aMax01[tT3]);
				}
				nT4 /= nLen1;
				tModLvl -= nT4;
			} else {
				nLen1 = -nMxMn[tT0][0];
				GetMinNIdx(aMin01, aNId01, nLen1, nQt01, nT2);

				nT4 = 0;
				for (tT3 = 0; tT3 < nLen1; tT3++) {
					aXMI01[tT3] = aNId01[tT3];
					nT4 += aMin01[tT3];
				}
				nT4 /= nLen1;
				tModLvl -= nT4;
			}

			if (nMxMn[tT0][1] > 0) {
				nLen2 = nMxMn[tT0][1];
				GetMaxNIdx(aMax02, aXId02, nLen2, nQt02, nT2);

				nT4 = 0;
				for (tT3 = 0; tT3 < nLen2; tT3++) {
					aXMI02[tT3] = aXId02[tT3];
					nT4 += (16 - aMax02[tT3]);
				}
				nT4 /= nLen2;
				tModLvl -= nT4;
			} else {
				nLen2 = -nMxMn[tT0][1];
				GetMinNIdx(aMin02, aNId02, nLen2, nQt02, nT2);

				nT3 = 0;
				nT4 = 0;
				for (tT3 = 0; tT3 < nLen2; tT3++) {
					aXMI02[tT3] = aNId02[tT3];
					nT4 += aMin02[tT3];

					nT3 = pDif02[29 - aNId02[tT3]];
					if (nT3 > iMxDif)
						nT4 += 8;
					else {
						nT3 = (nT3 << 3) +
							(iMxDif >> 1);
						nT3 /= (iMxDif + 1);
						nT4 += nT3;
					}
				}
				nT4 /= (2 * nLen2);
				tModLvl -= nT4;
			}

			tModFlg01[tT0] = 0;
			for (tT3 = 0; tT3 < nLen1; tT3++)
				tModFlg01[tT0] |= (1 << aXMI01[tT3]);

			tModFlg02[tT0] = 0;
			for (tT3 = 0; tT3 < nLen2; tT3++)
				tModFlg02[tT0] |= (1 << aXMI02[tT3]);

			uT03 = (1 << nT2) - 1;
			tModFlg01[tT0] &= uT03;
			tModFlg02[tT0] &= uT03;

			uT01 = (sModFlg01[tT0] << 1);
			uT01 |= (uT01 >> nT2);
			uT01 &= uT03;

			uT02 = (sModFlg02[tT0] << 1);
			uT02 |= (uT02 >> nT2);
			uT02 &= uT03;

			/* minimum check */
			nLen2 = 0;
			if (tT0 == 0) { /* 8-7 */
				nLen2 = 11;
			} else if (tT0 == 2) { /* 6-4 */
				nLen2 = 6;
			} else if (tT0 == 3) { /* 5-5 */
				nLen2 = 6;
			}

			if (nLen2 > 0) {
				GetMinNIdx(aMin02, aNId02, nLen2, nQt02, nT2);
				nT4 = 0;
				for (tT3 = 0; tT3 < nLen2; tT3++) {
					nT3 = pDif02[29 - aNId02[tT3]];
					if (nT3 > iMxDif)
						nT4 += 8;
					else {
						nT3 = (nT3 << 3)
							+ (iMxDif >> 1);
						nT3 /= (iMxDif + 1);
						nT4 += nT3;
					}
				}
				nT4 /= nLen2;
				tModLvl -= nT4;
			}

			if (nMin02 > iMxDif) {
				tModLvl -= 16;
					nModCnt[tT0] = 0;
			} else {
				nT4 = (nMin02 << 4);
				nT4 = nT4 / iMxDif;
				tModLvl -= nT4;
			}

			/*
			 * nT4 = (nMin02 << 6) + (iMxDif >> 1);
			 * nT4 = nT4 / iMxDif;
			 * if (nModCnt[tT0] > nT4)
			 *	nModCnt[tT0] -= nT4;
			 * else
			 *	nModCnt[tT0] = 0;
			 */


			/* Distance between maximum-2 dif01*/
			if (aXMI01[1] > aXMI01[0])
				nT3 = aXMI01[1] - aXMI01[0];
			else
				nT3 = aXMI01[0] - aXMI01[1];

			/* Distance between minimium-2 dif02 */
			if (aXMI02[1] > aXMI02[0])
				nT4 = aXMI02[1] - aXMI02[0];
			else
				nT4 = aXMI02[0] - aXMI02[1];


			if ((uT01 == tModFlg01[tT0]) &&
				(uT02 == tModFlg02[tT0]) &&
				(nT3 > 0) && (uT01 > 0) &&
				(uT02 > 0)) {
				if (tT0 == 0) {
					if (nT3 == 7 || nT3 == 8) {
						nModCnt[tT0] += 1;
						tModLvl += 2;
					} else
						nModCnt[tT0] = 0;
				} else if (tT0 == 2) {
					if (nT3 == 4 || nT3 == 6) {
						nModCnt[tT0] += 1;
						tModLvl += 2;
					} else
						nModCnt[tT0] = 0;
				} else if (tT0 == 3) {
					if (nT3 == 5) {
						nModCnt[tT0] += 1;
						tModLvl += 2;
					} else
						nModCnt[tT0] = 0;
				}
			}

			if ((uT02 == tModFlg02[tT0]) &&
				(nT4 > 0) && (uT02 > 0)) {
				if ((uT01 == tModFlg01[tT0]) && (uT01 > 0))
					tModLvl += 1;

				if (tT0 == 1) {
					if (nT4 == 5 || nT4 == 7) {
						nModCnt[tT0] += 1;
						tModLvl += 1;
					} else
						nModCnt[tT0] = 0;
				}  else if (tT0 == 4) {
					if (nT4 == 1 || nT4 == 9) {
						nModCnt[tT0] += 1;
						tModLvl += 1;
					} else
						nModCnt[tT0] = 0;
				} else if (tT0 == 5) {
					if (nT4 == 3 || nT4 == 7) {
						nModCnt[tT0] += 1;
						tModLvl += 1;
					} else
						nModCnt[tT0] = 0;
				} else if (tT0 == 6) {
					if (nT4 == 5) {
						nModCnt[tT0] += 1;
						tModLvl += 1;
					} else
						nModCnt[tT0] = 0;
				}
			}

			if (nModCnt[tT0] > 254)
				nModCnt[tT0] = 254;

			if (tModLvl < 0)
				tModLvl = 0;

			nModLvl[tT0] = tModLvl;

			sModFlg01[tT0] = tModFlg01[tT0];
			sModFlg02[tT0] = tModFlg02[tT0];
		} /* 2-3-4-5*/
	}

	tModLvl = nModLvl[0];
	nT1 = 0;
	for	(nT0 = 1; nT0 <	PDXX_PT_NUM; nT0++) {
		if (nModLvl[nT0] > tModLvl) {
			tModLvl = nModLvl[nT0];
			nT1 = nT0;
		}
	}

	if (prt_flg)
		sprintf(debug_str + strlen(debug_str),
			"nMax02(10)=%4d < (%4d)\n",
			nMax02, (iSIZE >> flm2224_stl_sft));

	if ((nT1 == 4) &&
		(nMax02 <= (iSIZE >> flm2224_stl_sft)))
		nModCnt[nT1] = 0;

	pModXx[HISDETNUM - 1] = nT1;
	pLvlXx[HISDETNUM - 1] = tModLvl;
	pFlgXx[HISDETNUM - 1] = (pDif01[29] < pDif01[28]);

	/* recheck */
	if ((pFlgXx[HISDETNUM - 2] == 0) &&
		(pDif01[29] > pDif01[28]))
		pFlgXx[HISDETNUM - 2] = 1;

	if (prt_flg && tModLvl > 0) {
		sprintf(debug_str + strlen(debug_str),
			"#FM%5d detected ct(%3d) lvl(%2d)\n",
			nPrtLog[nT1], nModCnt[nT1], nModLvl[nT1]);
		if (pDif01[29] < pDif01[28])
			sprintf(debug_str + strlen(debug_str),
				"#Pre: A<-A\n");
		else
			sprintf(debug_str + strlen(debug_str),
				"#Nxt: A B->\n");
	}

	if (prt_flg) {
		sprintf(debug_str + strlen(debug_str),
			"Mod=%5d, Flg=%d, Lvl=%3d\n",
			nT1, pFlgXx[HISDETNUM - 1],
			tModLvl);
		pr_info("%s", debug_str);
	}

	return nT1;
}
