/*
 * drivers/amlogic/media/deinterlace/film_mode_fmw/vof_soft_top.c
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

int cmb32_blw_wnd = 180; /*192 */
module_param(cmb32_blw_wnd, int, 0644);
MODULE_PARM_DESC(cmb32_blw_wnd, "cmb32_blw_wnd");

static int cmb32_wnd_ext = 11;
module_param(cmb32_wnd_ext, int, 0644);
MODULE_PARM_DESC(cmb32_wnd_ext, "cmb32_wnd_ext");

int cmb32_wnd_tol = 4;
module_param(cmb32_wnd_tol, int, 0644);
MODULE_PARM_DESC(cmb32_wnd_tol, "cmb32_wnd_tol");

int cmb32_frm_nocmb = 40;
module_param(cmb32_frm_nocmb, int, 0644);
MODULE_PARM_DESC(cmb32_frm_nocmb, "cmb32_frm_nocmb");

int cmb32_min02_sft = 7;
module_param(cmb32_min02_sft, int, 0644);
MODULE_PARM_DESC(cmb32_min02_sft, "cmb32_min02_sft");

int cmb32_cmb_tol = 10;
module_param(cmb32_cmb_tol, int, 0644);
MODULE_PARM_DESC(cmb32_cmb_tol, "cmb32_cmb_tol");

int cmb32_avg_dff = 48; /* if avg dif32 > dff>>4 */
module_param(cmb32_avg_dff, int, 0644);
MODULE_PARM_DESC(cmb32_avg_dff, "cmb32_avg_dff");

int cmb32_smfrm_num = 4;
module_param(cmb32_smfrm_num, int, 0644);
MODULE_PARM_DESC(cmb32_smfrm_num, "cmb32_smfrm_num");

int cmb32_nocmb_num = 20;
module_param(cmb32_nocmb_num, int, 0644);
MODULE_PARM_DESC(cmb32_nocmb_num, "cmb32_nocmb_num");

int cmb22_gcmb_rnum = 8;
module_param(cmb22_gcmb_rnum, int, 0644);
MODULE_PARM_DESC(cmb22_gcmb_rnum, "cmb22_gcmb_rnum");

int flmxx_cal_lcmb = 1;
module_param(flmxx_cal_lcmb, int, 0644);
MODULE_PARM_DESC(flmxx_cal_lcmb, "flmxx_cal_lcmb");

/* 15: 8-7 */
/* 12: 3-2-3-2-2 */
/* 10: 6-4 */
/* 10: 5-5 */
/* 10: 2-2-2-4 */
/* 10: 2-3-3-2 */
/* 10: 3-2-3-2 */
/* row * flmxx_no_cmb >> 6 */
static unsigned int flmxx_no_cmb[7] = {8, 32, 8, 8, 8, 32, 32};
static unsigned int flmxx_nn_cmb = 7;
module_param_array(flmxx_no_cmb, uint, &flmxx_nn_cmb, 0664);

/* Get 1-Row combing information, 1bit */
/* iHSCMB[9]; 9x32=288 */
static UINT8 Get1RCmb(UINT32 *iHSCMB, UINT32 iRow, int nRow)
{
	UINT8 nR1 = 0;
	UINT8 nBt = 0;

	if (nRow > 288)
		iRow >>= 1;
	nR1 = ((iRow >> 5) & 0xf);/* iRow/32; 0--8 */
	iHSCMB[nR1] = nR1 > 8 ? 0 : iHSCMB[nR1];
	nBt = (iRow & 0x1f);/* iRow%32 */
	return (iHSCMB[nR1] >> nBt) & 0x1;
}

int VOFSftTop(UINT8 *rFlmPstGCm, UINT8 *rFlmSltPre, UINT8 *rFlmPstMod,
		UShort *rPstCYWnd0, UShort *rPstCYWnd1,
		UShort *rPstCYWnd2, UShort *rPstCYWnd3, int nMod,
		UINT32 *rROCmbInf, struct sFlmDatSt *pRDat,
		struct sFlmSftPar *pPar, int nROW, int nCOL, bool reverse)
{
	/* HSCMB[hist10][9(32bit)] */
	static UINT32 HSCMB[HISCMBNUM][ROWCMBLEN];
	/* 6-history,10-5Wnd(bgn/end) */
	static int CWND[HISDETNUM][2 * VOFWNDNUM];
	static UINT8 BGN;
	static UINT8 frmNoCmb; /* counter from No combing */
	static UINT8 CmbFlds;  /* counter of combing field */
	static UINT8 NumSmFd;  /* counter for same field */

	int mDly = pPar->mPstDlyPre;
	int flm22_flag = pPar->flm22_flag;
	int cmb22_nocmb_num = pPar->cmb22_nocmb_num;
	int cmb32_blw_wnd_rel = cmb32_blw_wnd;
	/* UINT8 *PREWV = pRDat.pFlg32; or pRDat.pFlg22 */
	/* static int TCNm[HISCMBNUM]; history: the number of combing-rows */
	int *TCNm = pRDat->TCNm;
	static int NWND[HISDETNUM];/* 6-history,the number of combing windows */
	static int WGlb[HISDETNUM];	/* Global combing */
	static unsigned int pFlgXx;
	static UINT8 pCmbXx[PDXX_PT_NUM];

	UINT8 *pFlg32 = pRDat->pFlg32;	/* [HISDETNUM]; //history information */
	UINT8 *pMod32 = pRDat->pMod32;	/* [HISDETNUM]; */
	UINT8 *pFlg22 = pRDat->pFlg22;
	UINT8 *pMod22 = pRDat->pMod22;
	UINT8 *mNum32 = pRDat->mNum32;
	UINT32 *DIF02 = pRDat->rROFrmDif02; /* windows */

	int VOFWnd[2 * VOFWNDNUM]; /* VOF windows 5*(bgn/end) */
	int nCSum = 0;		/* Combine sum */
	int nWCmb = 0; /* Total Window combing */
	int nBCmb = 0; /* combing line of below */

	UINT32 nCb32 = 0;

	UINT32 nRCmbAd[ROWCMBLEN];
	int nDif02Min = ((nROW * nCOL) >> cmb32_min02_sft);
	int nT0 = 0;
	int nT1 = 0;
	int nT2 = 0;
	int nT3 = 0;

	pFlgXx = (((pFlgXx >> 1) << 2) |
			(pRDat->pFlgXx[HISDETNUM - 2] << 1) |
			(pRDat->pFlgXx[HISDETNUM - 1]));
	/* for 1080i */
	if (nROW > 288)
		cmb32_blw_wnd_rel = 70;
	/* Initialization */
	if (BGN == 0) {
		for (nT0 = 0; nT0 < HISCMBNUM; nT0++) {
			/* (288/32) */
			for (nT1 = 0; nT1 < ROWCMBLEN; nT1++) {
				/* 32-bit (all 1) */
				HSCMB[nT0][nT1] = 0xffffffff;
			}
			TCNm[nT0] = 0;
		}

		for (nT0 = 0; nT0 < HISDETNUM; nT0++) {
			NWND[nT0] = 15;
			WGlb[nT0] = 0;
			for (nT1 = 0; nT1 < 2 * VOFWNDNUM; nT1++)
				CWND[nT0][nT1] = 0;
		}

		BGN = 1;
	}

	for (nT0 = 1; nT0 < HISCMBNUM; nT0++) {
		for (nT1 = 0; nT1 < ROWCMBLEN; nT1++)
			HSCMB[nT0 - 1][nT1] = HSCMB[nT0][nT1];
		TCNm[nT0 - 1] = TCNm[nT0];
	}

	/* static int CWND[6][10]; */
    /* 6-history, 10-5Wnd(bgn/end) */
	for (nT0 = 0; nT0 < HISDETNUM - 1; nT0++) {
		for (nT1 = 0; nT1 < 2 * VOFWNDNUM; nT1++)
			CWND[nT0][nT1] = CWND[nT0 + 1][nT1];
	}

	for (nT1 = 0; nT1 < 2 * VOFWNDNUM; nT1++) {
		CWND[HISDETNUM - 1][nT1] = 0;/* f(t-0) vs f(t-1) */
		VOFWnd[nT1] = 0; /* initialization */
	}

	/* nS0 = 0; */
	nCSum = 0;
	for (nT0 = 0; nT0 < ROWCMBLEN; nT0++) {
		nCb32 = rROCmbInf[nT0];
		/* Inf[0]-[31:0], First-[31], Lst-[0] */
		HSCMB[HISCMBNUM - 1][nT0] = nCb32;

		for (nT1 = 0; nT1 < 32; nT1++) {
			nCSum += (nCb32 & 0x1);
			nCb32 = nCb32 >> 1;
		}
	}

	if (nCSum > nROW)
		nCSum = nROW;
	TCNm[HISCMBNUM - 1] = nCSum; /* the number of combing row */

	for (nT0 = 0; nT0 < HISDETNUM-1; nT0++) {
		NWND[nT0] = NWND[nT0+1];
		WGlb[nT0] = WGlb[nT0+1];
	}

	prt_flg = ((pr_pd >> 5) & 0x1);
	if (prt_flg)
		sprintf(debug_str, "#CMB-Dbg\nnCSum=%03d\n", nCSum);

	nT1 = 0;
	if (pMod32[HISDETNUM - 1] == 3) {
		if (pFlg32[HISDETNUM - 1] & 0x1)
			nT1 = 1;
		else
			nT1 = 2;

		/* TODO: Check here */
		WGlb[HISDETNUM-1] = 0;

		for (nT0 = 0; nT0 < ROWCMBLEN; nT0++)
			nRCmbAd[nT0] = HSCMB[HISCMBNUM - nT1][nT0];

		for (nT1 = nT1 + 1; nT1 <= 5; nT1++) {
			if (pFlg32[HISDETNUM - nT1] & 0x1) {
				for (nT0 = 0; nT0 < ROWCMBLEN; nT0++) {
					nRCmbAd[nT0] =
						(nRCmbAd[nT0] &
						HSCMB[HISCMBNUM - nT1][nT0]);
				}
			}
		}

		if (prt_flg)
			for (nT0 = 0; nT0 < ROWCMBLEN; nT0++)
				sprintf(debug_str + strlen(debug_str),
					"nCmd32([%d])=[%08x]\n",
					nT0, nRCmbAd[nT0]);

		/* return: the number of windows */
		nT1 = VOFDetSub1(VOFWnd, &nCSum, 3, nRCmbAd, nROW, pPar);
		nWCmb = 0;
		nBCmb = 0;
		for (nT0 = 0; nT0 < nT1; nT0++) {
			if (VOFWnd[2*nT0] > (cmb32_blw_wnd_rel * nROW >> 8)) {
				CWND[HISDETNUM-1][2*nT0] =
					VOFWnd[2*nT0] - cmb32_wnd_ext;
				CWND[HISDETNUM-1][2*nT0+1] =
					VOFWnd[2*nT0+1] + cmb32_wnd_ext;
				if (reverse) {
					if (CWND[HISDETNUM-1][2*nT0+1] >=
						nROW - 1)
						CWND[HISDETNUM-1][2*nT0+1]
							= nROW - 1;
					CWND[HISDETNUM-1][2*nT0] = nROW - 1
					- CWND[HISDETNUM-1][2*nT0+1];
					CWND[HISDETNUM-1][2*nT0+1] = nROW
					- 1 - (VOFWnd[2*nT0] - cmb32_wnd_ext);
				}

				nBCmb = VOFWnd[2*nT0+1]-VOFWnd[2*nT0]+1;

				/* patch for MIT32Mix ending vof */
				if (CWND[4][2*nT0] < CWND[5][2*nT0])
					nT2 = CWND[5][2*nT0] - CWND[4][2*nT0];
				else
					nT2 = CWND[4][2*nT0] - CWND[5][2*nT0];

				if (CWND[4][2*nT0+1] < CWND[5][2*nT0+1])
					nT3 = CWND[5][2*nT0+1]
						- CWND[4][2*nT0+1];
				else
					nT3 = CWND[4][2*nT0+1]
						- CWND[5][2*nT0+1];

				if ((nT2 <= cmb32_wnd_tol) &&
					(nT3 <= cmb32_wnd_tol)) {
					if (CWND[4][2*nT0] < CWND[5][2*nT0])
						CWND[5][2*nT0] = CWND[4][2*nT0];

					if (CWND[4][2*nT0+1]
						> CWND[5][2*nT0+1])
						CWND[5][2*nT0+1]
							= CWND[4][2*nT0+1];
				}
				/* patch for MIT32Mix ending vof */
			}
			nWCmb += VOFWnd[2*nT0+1]-VOFWnd[2*nT0]+1;

			if (prt_flg)
				sprintf(debug_str + strlen(debug_str),
				"Wnd32[%d]=[%3d~%3d]\n", nT0,
				VOFWnd[2 * nT0], VOFWnd[2 * nT0 + 1]);
		}

		if (prt_flg)
			sprintf(debug_str + strlen(debug_str),
			"frmNoCm=%02d CmbFld=%02d\n",
			frmNoCmb, CmbFlds);

		/* VOF using last ones */
		if (nBCmb == 0) {
			if ((frmNoCmb > cmb32_frm_nocmb) ||
				(DIF02[0] < nDif02Min))
				CmbFlds = 0;

			if (frmNoCmb < 255)
				frmNoCmb = frmNoCmb+1;
		} else {
			/* noCombing => combing => noCombing */
			if (frmNoCmb >= cmb32_cmb_tol)
				CmbFlds = 0;

			frmNoCmb = 0;

			if (CmbFlds < 255)
				CmbFlds = CmbFlds+1;
		}

		/* parameter setting */
		if ((nBCmb == 0) && (frmNoCmb < cmb32_frm_nocmb) &&
			(CmbFlds > (cmb32_cmb_tol >> 1)) &&
			(DIF02[0] > nDif02Min))
			for (nT0 = 0; nT0 < 2 * VOFWNDNUM; nT0++)
				CWND[HISDETNUM-1][nT0] = CWND[HISDETNUM-2][nT0];
		/* VOF using last ones */

		/* patch for cadence-32 */
		if ((NumSmFd > cmb32_smfrm_num) &&
			(mNum32[HISDETNUM-1] > cmb32_nocmb_num)) {
			for (nT0 = 0; nT0 < 2*VOFWNDNUM; nT0++)
				CWND[HISDETNUM-1][nT0] = 0;

		if (prt_flg)
			sprintf(debug_str + strlen(debug_str),
				"NumSmFd=%03d\n", NumSmFd);
		}
		/* patch for cadence-32 */

		if (prt_flg)
			sprintf(debug_str + strlen(debug_str),
			"nCSum32=%03d, nWndCmb=%03d\n",
			nCSum, nWCmb);
		/*
		 * if ( nCSum > (nROW>>1) ) {
		 *	WGlb[HISDETNUM-1] = 1;
		 * }
		 */

		/* here for vertical moving VOF */
		/* HQV: Mix32- piano+vertical VOF */
		/* here can be set as parameters */
		if (pFlg32[HISDETNUM-1] == 5) {
			/* patch for cadence-32 */
			if (DIF02[0] < nDif02Min) {
				if (NumSmFd < 255)
					NumSmFd = NumSmFd+1;
			} else
				NumSmFd = 0;
			/* patch for cadence-32 */

			/* 256/16 = 16 */
			/* 64/16  = 4 for 8-bit */
			/* vertical vof copyright */
			if (nWCmb < 10)
				nWCmb = 10;

			nT1 = (nWCmb * nCOL * cmb32_avg_dff + 8) >> 4;

			if ((DIF02[0] >= nT1) && (nWCmb > nBCmb+10))
				mNum32[HISDETNUM-1] = 0;

			if (prt_flg)
				sprintf(debug_str + strlen(debug_str),
				"AvgDif=%d (%d > (%d + 10)\n",
				DIF02[0]/nCOL/nWCmb, nWCmb, nBCmb);
		} /* here for vertical moving VOF */
	} else if ((pMod22[HISDETNUM-1] == 2) &&
		(pFlg22[HISDETNUM-1] & 0x1)) {
		if (flm22_flag)
			nT2 = 288 - cmb22_nocmb_num;
		else
			nT2 = ((nROW * cmb22_gcmb_rnum + 8) >> 4);
		if (nCSum > nT2)
			WGlb[HISDETNUM-1] = 1; /*global combing*/
		else
			WGlb[HISDETNUM-1] = 0; /*global combing*/
		if (prt_flg)
			sprintf(debug_str + strlen(debug_str),
					"WGlb22=%d\n", WGlb[HISDETNUM-1]);

		for (nT0 = 0; nT0 < ROWCMBLEN; nT0++)
			nRCmbAd[nT0] = HSCMB[HISCMBNUM-1][nT0];

		for (nT1 = 3; nT1 <= 5; nT1 += 2)
			for (nT0 = 0; nT0 < ROWCMBLEN; nT0++)
				nRCmbAd[nT0] = (nRCmbAd[nT0]
					& HSCMB[HISCMBNUM-nT1][nT0]);

		if (prt_flg)
			for (nT0 = 0; nT0 < ROWCMBLEN; nT0++)
				sprintf(debug_str + strlen(debug_str),
					"nCmd22([%d])=[%08x]\n",
					nT0, nRCmbAd[nT0]);

		/* return: the number of windows */
		nT1 = VOFDetSub1(VOFWnd, &nCSum, 2, nRCmbAd, nROW, pPar);
		for (nT0 = 0; nT0 < nT1; nT0++) {
			CWND[HISDETNUM-1][2*nT0]   = VOFWnd[2*nT0];
			CWND[HISDETNUM-1][2*nT0+1] = VOFWnd[2*nT0+1];
			if (reverse) {
				CWND[HISDETNUM-1][2*nT0] =
					nROW - 1 - VOFWnd[2*nT0+1];
				CWND[HISDETNUM-1][2*nT0+1] =
					nROW - 1 - VOFWnd[2*nT0];
			}
			if (prt_flg)
				sprintf(debug_str + strlen(debug_str),
					"Wnd22[%d]=[%3d~%3d]\n",
					nT0, VOFWnd[2*nT0], VOFWnd[2*nT0+1]);
		}
	}

	if (prt_flg)
		pr_info("%s", debug_str);

	if ((pRDat->pLvlXx[HISDETNUM - 1] > flmxx_cal_lcmb) &&
		(pRDat->pModXx[HISDETNUM - 1] != 6)) {
		if ((pFlgXx & 0x1) || (DIF02[0] < nDif02Min))
			nT1 = 1;
		else
			nT1 = 2;

		if (prt_flg)
			sprintf(debug_str, "ModXx=%d, LvlXx=%d, FlgXx=%08x\n",
			pRDat->pModXx[HISDETNUM - 1],
			pRDat->pLvlXx[HISDETNUM - 1],
			pFlgXx);

		for (nT0 = 0; nT0 < ROWCMBLEN; nT0++)
			nRCmbAd[nT0] = HSCMB[HISCMBNUM - nT1][nT0];

		for (nT1 = nT1 + 1; nT1 <= 10; nT1++) {
			if ((pFlgXx >> (nT1 - 1)) & 0x1) {
				for (nT0 = 0; nT0 < ROWCMBLEN; nT0++) {
					nRCmbAd[nT0] =
						(nRCmbAd[nT0] &
						HSCMB[HISCMBNUM - nT1][nT0]);
				}
			}
		}

		if (prt_flg)
			for (nT0 = 0; nT0 < ROWCMBLEN; nT0++)
				sprintf(debug_str + strlen(debug_str),
				"nCmdXx([%d])=[%08x]\n",
				nT0, nRCmbAd[nT0]);

		nT0 = 4 + pRDat->pModXx[HISDETNUM - 1];
		/* return: the number of windows */
		nT1 = VOFDetSub1(VOFWnd, &nCSum, nT0, nRCmbAd, nROW, pPar);

		nWCmb = 0;
		for (nT0 = 0; nT0 < nT1; nT0++) {
			CWND[HISDETNUM-1][2*nT0]   = VOFWnd[2*nT0];
			CWND[HISDETNUM-1][2*nT0+1] = VOFWnd[2*nT0+1];
			if (reverse) {
				CWND[HISDETNUM-1][2*nT0] =
					nROW - 1 - VOFWnd[2*nT0+1];
				CWND[HISDETNUM-1][2*nT0+1] =
					nROW - 1 - VOFWnd[2*nT0];
			}
			if (prt_flg)
				sprintf(debug_str + strlen(debug_str),
				"WndXx[%d]=[%3d~%3d]\n",
				nT0, VOFWnd[2*nT0], VOFWnd[2*nT0+1]);

			nWCmb += VOFWnd[2*nT0+1]-VOFWnd[2*nT0]+1;
		}

		nT1 = pRDat->pModXx[HISDETNUM - 1];
		for (nT0 = 0; nT0 < HISDETNUM; nT0++) {
			if ((nT0 == nT1) &&
			(nWCmb < ((flmxx_no_cmb[nT1] * nROW) >> 6)))
				pCmbXx[nT0] = pCmbXx[nT0] + 1;
			else
				pCmbXx[nT0] = 0;
		}

		if (prt_flg)
			sprintf(debug_str + strlen(debug_str),
			"nCSum=%3d, nWCmb=%3d, pCmbXx=%d\n",
			nCSum, nWCmb, pCmbXx[nT1]);

		nT1 = pRDat->pLvlXx[HISDETNUM - 1] + pCmbXx[nT1];

		if (nT1 > 255)
			nT1 = 255;
		pRDat->pLvlXx[HISDETNUM - 1] = nT1;

		if (prt_flg)
			pr_info("%s", debug_str);
	}

	prt_flg = ((pr_pd >> 1) & 0x1);
	if (prt_flg)
		sprintf(debug_str, "#Pre-VOF:\n");

	/* film-mode: pMod22[5-mDly] or pMod32[5-mDly] */
	if ((*rFlmPstMod == 1) || (*rFlmPstMod == 2)) {
		/* weaver with pre-field */
		if (*rFlmSltPre == 1) {
			/* Interpolation method:0-mtn,1-with-buffer,2-ei,3-di */
			rPstCYWnd0[0] = CWND[HISDETNUM - 1 - mDly][0];/* bgn */
			rPstCYWnd0[1] = CWND[HISDETNUM - 1 - mDly][1];/* end */
			rPstCYWnd0[2] = 3;/* 0-mtn,1-with-buffer,2-ei,3-di */

			rPstCYWnd1[0] = CWND[HISDETNUM - 1 - mDly][2];/* bgn */
			rPstCYWnd1[1] = CWND[HISDETNUM - 1 - mDly][3];/* end */
			rPstCYWnd1[2] = 3;/* 0-mtn,1-with-buffer,2-ei,3-di */

			rPstCYWnd2[0] = CWND[HISDETNUM - 1 - mDly][4];/* bgn */
			rPstCYWnd2[1] = CWND[HISDETNUM - 1 - mDly][5];/* end */
			rPstCYWnd2[2] = 3;/* 0-mtn,1-with-buffer,2-ei,3-di */

			rPstCYWnd3[0] = CWND[HISDETNUM - 1 - mDly][6];/* bgn */
			rPstCYWnd3[1] = CWND[HISDETNUM - 1 - mDly][7];/* end */
			rPstCYWnd3[2] = 3;/* 0-mtn,1-with-buffer,2-ei,3-di */

			/* pFMReg->rFlmPstGCm = WGlb[5-mDly]; */
			*rFlmPstGCm = WGlb[HISDETNUM - 1 - mDly];

			if (prt_flg) {
				sprintf(debug_str + strlen(debug_str),
					"rFlmPstGCm-5=%d\n", *rFlmPstGCm);
				sprintf(debug_str + strlen(debug_str),
					"pFlg32=%d, pFlg22=%d\n",
					pFlg32[HISDETNUM - 1 - mDly],
					pFlg22[HISDETNUM - 1 - mDly]);
			}
		} else {
			/* weaver with nxt-field */
			/* Interpolation method: 0-EI,1-MTN,2-MA,3-Weaver */
			rPstCYWnd0[0] = CWND[HISDETNUM - mDly][0];/* bgn */
			rPstCYWnd0[1] = CWND[HISDETNUM - mDly][1];/* end */
			rPstCYWnd0[2] = 3;/* 0-mtn,1-with-buffer,2-ei,3-di */
			rPstCYWnd1[0] = CWND[HISDETNUM - mDly][2];/* bgn */
			rPstCYWnd1[1] = CWND[HISDETNUM - mDly][3];/* end */
			rPstCYWnd1[2] = 3;/* 0-mtn,1-with-buffer,2-ei,3-di */
			rPstCYWnd2[0] = CWND[HISDETNUM - mDly][4];/* bgn */
			rPstCYWnd2[1] = CWND[HISDETNUM - mDly][5];/* end */
			rPstCYWnd2[2] = 3;/* 0-mtn,1-with-buffer,2-ei,3-di */
			rPstCYWnd3[0] = CWND[HISDETNUM - mDly][6];/* bgn */
			rPstCYWnd3[1] = CWND[HISDETNUM - mDly][7];/* end */
			rPstCYWnd3[2] = 3;/* 0-mtn,1-with-buffer,2-ei,3-di */
			*rFlmPstGCm = WGlb[HISDETNUM - mDly];
			if (prt_flg) {
				sprintf(debug_str + strlen(debug_str),
					"rFlmPstGCm-6=%d\n", *rFlmPstGCm);
				sprintf(debug_str + strlen(debug_str),
					"pFlg32=%d, pFlg22=%d\n",
					pFlg32[HISDETNUM - 1 - mDly],
					pFlg22[HISDETNUM - 1 - mDly]);
			}
		}
	} else {
		rPstCYWnd0[0] = 0;	/* bgn */
		rPstCYWnd0[1] = 0;	/* end */
		rPstCYWnd0[2] = 3;	/* 0-mtn,1-with-buffer,2-ei,3-di */
		rPstCYWnd1[0] = 0;	/* bgn */
		rPstCYWnd1[1] = 0;	/* end */
		rPstCYWnd1[2] = 3;	/* 0-mtn,1-with-buffer,2-ei,3-di */
		rPstCYWnd2[0] = 0;	/* bgn */
		rPstCYWnd2[1] = 0;	/* end */
		rPstCYWnd2[2] = 3;	/* 0-mtn,1-with-buffer,2-ei,3-di */
		rPstCYWnd3[0] = 0;	/* bgn */
		rPstCYWnd3[1] = 0;	/* end */
		rPstCYWnd3[2] = 3;	/* 0-mtn,1-with-buffer,2-ei,3-di */
		*rFlmPstGCm = 1;

		if (prt_flg)
			sprintf(debug_str + strlen(debug_str),
			"rFlmPstGCm-1=%d\n", *rFlmPstGCm);
	}

	if (prt_flg) {
		sprintf(debug_str + strlen(debug_str),
		"rPstCYWnd=(%d, %d), (%d, %d), (%d, %d), (%d, %d)\n\n",
			rPstCYWnd0[0], rPstCYWnd0[1],
			rPstCYWnd1[0], rPstCYWnd1[1],
			rPstCYWnd2[0], rPstCYWnd2[1],
			rPstCYWnd3[0], rPstCYWnd3[1]);

		pr_info("%s", debug_str);
	}

	return nCSum;
}

/* int *PREWV:5*2 */
/* nCNum: total rows of combing */
int VOFDetSub1(int *VOFWnd, int *nCNum, int nMod, UINT32 *nRCmb, int nROW,
	       struct sFlmSftPar *pPar)
{
	int rCmbRwMinCt0 = 0; /* 8; //for film 3-2 */
	int rCmbRwMinCt1 = 0; /* =7; //for film 2-2 */
	/* int rCmbRwMaxStp=1; //fill in the hole */
	int rCmbRwMinCt = 0;
	int nCSUM = 0;	/* Combing sum (nCSUM>rCmbRwMinCt0) */
	int nMIN = 0;
	int nT0 = 0;
	int nT1 = 0;
	int nT2 = 0;
	int nCNM = 0;
	int nBgn = 0;
	int nEnd = 0;
	int fEND = 0;
	int pIDx[VOFWNDNUM + 1][2];	/* Maximum-5windows */
	int nIDx = 0;

	rCmbRwMinCt = rCmbRwMinCt1;
	if (nMod == 3)
		rCmbRwMinCt = rCmbRwMinCt0;
	if (IS_ERR(pPar)) {
		pr_err("%s:%d pPar = 0x%p.\n",
			__func__, __LINE__, pPar);
		return -1;
	}
	rCmbRwMinCt0 = pPar->rCmbRwMinCt0;	/* 8; //for film 3-2 */
	rCmbRwMinCt1 = pPar->rCmbRwMinCt1;	/* =7; //for film 2-2 */

	for (nT0 = 0; (nT0 < nROW) && (nIDx <= VOFWNDNUM); nT0++) {
		fEND = 0;
		nT1 = nROW - 1 - nT0;

		/* if(nRCmb[nT0]==1) */
		if (Get1RCmb(nRCmb, nT1, nROW)) {
			nCSUM += 1;	/* Total */
			if (nT0 == 0)
				nBgn = nT0;
			else if (nT0 == nROW - 1) {
				if (Get1RCmb(nRCmb, 1, nROW)) {
					/* at least (2-Row combing) */
					nEnd = nT0;
					fEND = 1;
				}
			} else if (!Get1RCmb(nRCmb, nT1 + 1, nROW)) {
				/* (nRCmb[nT0-1]==0) */
				nBgn = nT0;
			}
		} else {
			/* nRCmb[nT0]==0 */
			if (nT0 != 0 && Get1RCmb(nRCmb, nT1 + 1, nROW)) {
				nEnd = nT0;	/* nT0-1 */
				fEND = 1;
			}
		}

		if (fEND == 0)
			continue;

		nCNM = nEnd - nBgn + 1;
		if (nCNM > rCmbRwMinCt || nEnd == nROW - 1) {
			if (nIDx == VOFWNDNUM) {
				nMIN = nCNM;
				nT2 = VOFWNDNUM;
				for (nT1 = 0; nT1 < VOFWNDNUM; nT1++)
					if ((pIDx[nT1][1] -
					pIDx[nT1][0] + 1) < nMIN) {
						nMIN = pIDx[nT1][1]
						- pIDx[nT1][0] + 1;
						nT2 = nT1;
					}
				if (nT2 != VOFWNDNUM) {
					pIDx[nT2][0] = nBgn;
					pIDx[nT2][1] = nEnd;
				}
			} else {
				pIDx[nIDx][0] = nBgn;
				pIDx[nIDx][1] = nEnd;
				nIDx += 1;
			}
		}
	}
	*nCNum = nCSUM;
	for (nT0 = 0; nT0 < nIDx; nT0++) {
		VOFWnd[2 * nT0] = pIDx[nT0][0];/* nBgn */
		VOFWnd[2 * nT0 + 1] = pIDx[nT0][1];/* nEnd */
	}
	return nIDx;
}

