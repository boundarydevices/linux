/*
 * drivers/amlogic/media/deinterlace/film_mode_fmw/film_fw1.c
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
#include <linux/amlogic/media/vfm/vframe.h>
#include "film_vof_soft.h"
#include "../deinterlace.h"


static int DIweavedetec(struct sFlmSftPar *pPar, int nDif01);
/* Software parameters (registers) */
UINT8 FlmVOFSftInt(struct sFlmSftPar *pPar)
{
	pPar->sFrmDifAvgRat = 16;	/* 0~32 */
	/* The Large Decision should be: (large>average+LgDifThrd) */
	pPar->sFrmDifLgTDif = 4096;
	pPar->sF32StpWgt01 = 15;
	pPar->sF32StpWgt02 = 15;
	/* Dif>Rat*Min  --> Larger */
	pPar->sF32DifLgRat = 16;

	pPar->sFlm2MinAlpha = 32;	/* [0~63] */
	pPar->sFlm2MinBelta = 32;	/* [0~63] */
	pPar->sFlm20ftAlpha = 16;	/* [0~63] */
	pPar->sFlm2LgDifThd = 4096;	/* [0~63] %LgDifThrd */
	pPar->sFlm2LgFlgThd = 8;

	pPar->sF32Dif01A1 = 65;
	pPar->sF32Dif01T1 = 128;
	pPar->sF32Dif01A2 = 60;
	pPar->sF32Dif01T2 = 128;

	pPar->rCmbRwMinCt0 = 8;	/* for film 3-2 */
	pPar->rCmbRwMinCt1 = 7;	/* for film 2-2 */

	pPar->sFlm32NCmb = 8;	/* absolute no combing for film 32 */
	/* pre-processing (t-0), post-processing f(t-mPstDlyPre); No RTL */
	pPar->mPstDlyPre = 1;
	/* pre-processing (t-0), pre-processing f(t+mNxtDlySft);
	 * No RTL,default=1
	 */
	pPar->mNxtDlySft = 1;

	pPar->cmb22_nocmb_num = 30;
	pPar->flm22_en = 1;
	pPar->flm32_en = 1;
	pPar->flm22_flag = 1;
	pPar->flm22_avg_flag = 0;
	pPar->flm2224_flag = 1;
	pPar->flm22_comlev = 22;
	pPar->flm22_comlev1 = 8;
	pPar->flm22_comlev2 = 22;
	pPar->flm22_comnum = 115;
	pPar->flm22_comth = 15;
	pPar->flm22_dif01_avgth = 55;
	pPar->dif01rate = 20;
	pPar->flag_di01th = 0;
	pPar->numthd = 60;
	pPar->flm32_dif02_gap_th = 3;/*suggest from vlsi-yanling*/
	pPar->flm32_luma_th = 90;
	pPar->sF32Dif02M0 = 4096;/* mpeg-4096, cvbs-8192 */
	pPar->sF32Dif02M1 = 4096;

	return 0;
}

/* Outputs:
 *     rCmb32Spcl:  1-bit, 0/1
 *    rPstCYWnd0~4[0]: bgn,
 *     rPstCYWnd0~4[1]: end,
 *     rPstCYWnd0~4[2]: 0-mtn,1-with-buffer,2-ei,3-di,
 *     rFlmPstGCm: 1-bit, global combing (maybe unused)
 *     rFlmSltPre: 1-bit, 0-next field, 1-previous field
 *     rFlmPstMod: 2-bit, 00-no, 01-22, 10-23, 11-other
 * Inputs:
 *     rROFldDif01:  difference f(t) and f(t-1), U32
 *     rROFldDif02:  difference f(t) and f(t-2), U32
 *     rROCmbInf[9]: U32 x 9
 *     nROW: (240 for 480i)
 */

unsigned int pr_pd;
module_param(pr_pd, uint, 0644);
MODULE_PARM_DESC(pr_pd, "/n printk /n");

bool prt_flg;
module_param(prt_flg, bool, 0644);
MODULE_PARM_DESC(prt_flg, "/n prt_flg /n");

char debug_str[512];

/* if flmxx level > flmxx_first_num */
/* flmxx first: even when 2-2 3-2 detected */
/* unsigned int flmxx_first_num = 50; */
/* module_param(flmxx_first_num, uint, 0644); */
/* MODULE_PARM_DESC(flmxx_first_num, */
/* "/n flmxx first: even when 2-2 3-2 detected /n"); */

/* if flmxx level > flmxx_maybe_num */
/* mabye flmxx: when 2-2 3-2 not detected */
unsigned int flmxx_maybe_num = 15;
module_param(flmxx_maybe_num, uint, 0644);
MODULE_PARM_DESC(flmxx_maybe_num,
"/n mabye flmxx: when 2-2 3-2 not detected /n");

int flm32_mim_frms = 6;
module_param(flm32_mim_frms, int, 0644);
MODULE_PARM_DESC(flm32_mim_frms, "flm32_mim_frms");

int flm22_dif01a_flag = 1;
module_param(flm22_dif01a_flag, int, 0644);
MODULE_PARM_DESC(flm22_dif01a_flag, "flm22_dif01a_flag");

int flm22_mim_frms = 60;
module_param(flm22_mim_frms, int, 0644);
MODULE_PARM_DESC(flm22_mim_frms, "flm22_mim_frms");

int flm22_mim_smfrms = 40;
module_param(flm22_mim_smfrms, int, 0644);
MODULE_PARM_DESC(flm22_mim_smfrms, "flm22_mim_smfrms");

int flm32_f2fdif_min0 = 11;
module_param(flm32_f2fdif_min0, int, 0644);
MODULE_PARM_DESC(flm32_f2fdif_min0, "flm32_f2fdif_min0");

int flm32_f2fdif_min1 = 11;
module_param(flm32_f2fdif_min1, int, 0644);
MODULE_PARM_DESC(flm32_f2fdif_min1, "flm32_f2fdif_min1");

int flm32_chk1_rtn = 25;
module_param(flm32_chk1_rtn, int, 0644);
MODULE_PARM_DESC(flm32_chk1_rtn, "flm32_chk1_rtn");

int flm32_ck13_rtn = 8;
module_param(flm32_ck13_rtn, int, 0644);
MODULE_PARM_DESC(flm32_ck13_rtn, "flm32_ck13_rtn");

int flm32_chk2_rtn = 16;
module_param(flm32_chk2_rtn, int, 0644);
MODULE_PARM_DESC(flm32_chk2_rtn, "flm32_chk2_rtn");

int flm32_chk3_rtn = 16;
module_param(flm32_chk3_rtn, int, 0644);
MODULE_PARM_DESC(flm32_chk3_rtn, "flm32_chk3_rtn");

int flm32_dif02_ratio = 8;
module_param(flm32_dif02_ratio, int, 0644);
MODULE_PARM_DESC(flm32_dif02_ratio, "flm32_dif02_ratio");

int flm22_chk20_sml = 6;
module_param(flm22_chk20_sml, int, 0644);
MODULE_PARM_DESC(flm22_chk20_sml, "flm22_chk20_sml");

int flm22_chk21_sml = 6;
module_param(flm22_chk21_sml, int, 0644);
MODULE_PARM_DESC(flm22_chk21_sml, "flm22_chk21_sml");

int flm22_chk21_sm2 = 10;
module_param(flm22_chk21_sm2, int, 0644);
MODULE_PARM_DESC(flm22_chk21_sm2, "flm22_chk21_sm2");

int flm22_lavg_sft = 4;
module_param(flm22_lavg_sft, int, 0644);
MODULE_PARM_DESC(flm22_lavg_sft, "flm22_lavg_sft");

int flm22_lavg_lg = 24;
module_param(flm22_lavg_lg, int, 0644);
MODULE_PARM_DESC(flm22_lavg_lg, "flm22_lavg_lg");

/* dif02 < (size >> sft) => static */
int flm22_stl_sft = 7; /*10*/
module_param(flm22_stl_sft, int, 0644);
MODULE_PARM_DESC(flm22_stl_sft, "flm22_stl_sft");

int flm22_chk5_avg = 50;
module_param(flm22_chk5_avg, int, 0644);
MODULE_PARM_DESC(flm22_chk5_avg, "flm22_chk5_avg");

int flm22_chk6_max = 20;
module_param(flm22_chk6_max, int, 0644);
MODULE_PARM_DESC(flm22_chk6_max, "flm22_chk6_max");

int flm22_anti_chk1 = 61;
module_param(flm22_anti_chk1, int, 0644);
MODULE_PARM_DESC(flm22_anti_chk1, "flm22_anti_chk1");

int flm22_anti_chk3 = 140;
module_param(flm22_anti_chk3, int, 0644);
MODULE_PARM_DESC(flm22_anti_chk3, "flm22_anti_chk3");

int flm22_anti_chk4 = 128;
module_param(flm22_anti_chk4, int, 0644);
MODULE_PARM_DESC(flm22_anti_chk4, "flm22_anti_chk4");

int flm22_anti_ck140 = 32;
module_param(flm22_anti_ck140, int, 0644);
MODULE_PARM_DESC(flm22_anti_ck140, "flm22_anti_ck140");

int flm22_anti_ck141 = 80;
module_param(flm22_anti_ck141, int, 0644);
MODULE_PARM_DESC(flm22_anti_ck141, "flm22_anti_ck141");

int flm22_frmdif_max = 50;
module_param(flm22_frmdif_max, int, 0644);
MODULE_PARM_DESC(flm22_frmdif_max, "flm22_frmdif_max");

int flm22_flddif_max = 100;
module_param(flm22_flddif_max, int, 0644);
MODULE_PARM_DESC(flm22_flddif_max, "flm22_flddif_max");

int flm22_minus_cntmax = 2;
module_param(flm22_minus_cntmax, int, 0644);
MODULE_PARM_DESC(flm22_minus_cntmax, "flm22_minus_cntmax");

static int flagdif01chk = 1;
module_param(flagdif01chk,  int, 0644);
MODULE_PARM_DESC(flagdif01chk, "flagdif01chk");

static int dif01_ratio = 10;
module_param(dif01_ratio,  int, 0644);
MODULE_PARM_DESC(dif01_ratio, "dif01_ratio");


static int flm22_force;

int comsum;

int FlmVOFSftTop(UINT8 *rCmb32Spcl, unsigned short *rPstCYWnd0,
	unsigned short *rPstCYWnd1, unsigned short *rPstCYWnd2,
	unsigned short *rPstCYWnd3, unsigned short *rPstCYWnd4,
	UINT8 *rFlmPstGCm, UINT8 *rFlmSltPre, UINT8 *rFlmPstMod,
	UINT8 *dif01flag, UINT32 *rROFldDif01, UINT32 *rROFrmDif02,
	UINT32 *rROCmbInf, UINT32 glb_frame_mot_num,
	UINT32 glb_field_mot_num, unsigned int *combing_row_num,
	unsigned int *frame_diff_avg, struct sFlmSftPar *pPar, bool reverse,
	struct vframe_s *vf)
{
	static UINT32 DIF01[HISDIFNUM]; /* Last one is global */
	static UINT32 DIF02[HISDIFNUM]; /* Last one is global */
    /* Dif01 of 5th windows used for 2-2 */
	static UINT32 DifW5[HISDIFNUM];

	static struct sFlmDatSt pRDat;
	static int pre22lvl;
	static UINT32 pre_fld_motnum;
	static int modpre;
	static int num;
	static int num32;
	static int flag_pre;
	static int comsumpre;
	static int nS1pre;
	int dif01th = 0;
	int avg_flag = 0;
	int flm22_min = 0;
	int flm22_th = 0;

	int nDIF01[HISDIFNUM];
	int nDIF02[HISDIFNUM];
	/* UINT32 nCb32=0; */
	unsigned int ntmp = 0;
	unsigned int flm22_mim_numb = 0;
	/* int nRCMB[ROWCMBNUM]; */
	int mDly = pPar->mPstDlyPre;
	int mNDly = pPar->mNxtDlySft;
	int flm22 = pPar->flm22_en;
	int flm32 = pPar->flm32_en;
	int flm22_flag = pPar->flm22_flag;
	int flm2224_flag = pPar->flm2224_flag;
	int flm22_comth = pPar->flm22_comth;
	int flm22_avg_flag = pPar->flm22_avg_flag;
	int comdif = 0;
	int dif01avg = 0;

	int nT0 = 0;
	int nT1 = 0;
	int nS0 = 0;
	int nS1 = 0;
	int nMod = 0;
	int difflag = 0;

	/* difference */
	pRDat.rROFrmDif02 = rROFrmDif02;
	/* size of the image */
	pRDat.iHeight = pPar->height; /* field height */
	pRDat.iWidth  = pPar->width;

	prt_flg = 0;
	debug_str[0] = '\0';

	/* Initialization */
	if (pPar->field_count < 3) {
		for (nT1 = 0; nT1 < HISDIFNUM; nT1++) {
			DIF01[nT1] = 0xffffffff;
			DIF02[nT1] = 0xffffffff;
			DifW5[nT1] = 0xffffffff;
		}

		for (nT1 = 0; nT1 < HISDETNUM; nT1++) {
			pRDat.pFlg32[nT1] = 0;
			pRDat.pMod32[nT1] = 0;
			pRDat.mNum32[nT1] = 0;

			pRDat.pFld32[nT1] = 0;
			pRDat.pFrm32[nT1] = 0;
			pRDat.pFrm32t[nT1] = 0;

			pRDat.pFlg22[nT1] = 0;
			pRDat.pMod22[nT1] = 0;
			pRDat.mNum22[nT1] = 0;

			pRDat.pStp22[nT1] = 0;
			pRDat.pSmp22[nT1] = 0;

			/* HISDETNUM hist */
			pRDat.pModXx[nT1] = 0;
			pRDat.pFlgXx[nT1] = 0; /* pre-1, nxt-0 */
			pRDat.pLvlXx[nT1] = 0;  /* mode level */
		}
	} else {
		for (nT1 = 1; nT1 < HISDETNUM; nT1++) {
			pRDat.mNum32[nT1 - 1] = pRDat.mNum32[nT1];
			pRDat.mNum22[nT1 - 1] = pRDat.mNum22[nT1];
		}
	}

	/* -------------------------------------------------------------- */
	nS0 = 0;
	nS1 = 0;
	/* int rROFrmDif02[6]; // Read only */
	/* int rROFldDif01[6]; // Read only */
	for (nT1 = 1; nT1 < HISDIFNUM; nT1++) {
		DIF01[nT1 - 1] = DIF01[nT1];
		DIF02[nT1 - 1] = DIF02[nT1];
		DifW5[nT1-1]   = DifW5[nT1];
	}

	DIF01[HISDIFNUM - 1] = rROFldDif01[0];	/* 5windows+global */
	DIF02[HISDIFNUM - 1] = rROFrmDif02[0];	/* 5windows+global */

	if (pr_pd) {
		sprintf(debug_str, "\nField#%5d: [%4dx%4d]\n",
			pPar->field_count, pPar->height, pPar->width);
		sprintf(debug_str + strlen(debug_str),
		"diff counter: %4d %4d\n",
			glb_field_mot_num, glb_frame_mot_num);
	}

	prt_flg = (pr_pd & 0x1);
	if (prt_flg) {
		sprintf(debug_str + strlen(debug_str), "Dif012=[%u,%u]\n",
			DIF01[HISDIFNUM - 1], DIF02[HISDIFNUM - 1]);

		for (nT1 = 1; nT1 < 6; nT1++)
			sprintf(debug_str + strlen(debug_str),
				"WDif12[%d]=[%u,%u]\n", nT1,
				rROFldDif01[nT1], rROFrmDif02[nT1]);

		for (nT1 = 0; nT1 < ROWCMBLEN; nT1++)
			sprintf(debug_str + strlen(debug_str),
				"rROCmbInf[%d]=%08x\n", nT1,
				rROCmbInf[nT1]);
	}
	if (pr_pd)
		pr_info("%s", debug_str);

	/* --------------------------------------------------------------- */
	/* int nDIF01[HISDIFNUM]; */
	/* int nDIF02[HISDIFNUM]; */
	for (nT1 = 0; nT1 < HISDIFNUM; nT1++) {
		nDIF01[nT1] = DIF01[nT1];
		nDIF02[nT1] = DIF02[nT1];
	}
	/* --------------------------------------------------------- */
	/* Film-Detection */
	nS1 = FlmDetSft(&pRDat, nDIF01, nDIF02, nT0, pPar, vf);

	nS0 = FlmModsDet(&pRDat, rROFldDif01[0], rROFrmDif02[0]);
	/* --------------------------------------------------------- */

	/* for panda 2-2 : flag	1, 3 */
	if (pRDat.pFlg22[HISDETNUM-1] &	0x1) {
		/* diff01 of the 5th window	*/
		/* for panda 2-2: VOF */
		DifW5[HISDIFNUM-1] =
			(16 * rROFldDif01[5]) / (rROFldDif01[0] + 16);
		nT1	= (DifW5[HISDIFNUM-1] << 2);
		if (nS1	> nT1)
			nS1	-= nT1;	/* reset the 5th window	*/

		pre22lvl = nS1;
	} else if (pRDat.pFlg22[HISDETNUM-1])
		nS1	= pre22lvl;
	else
		pre22lvl = 0;

	/* Current f(t-0) with previous */
	/* pFMReg->rCmb32Spcl =0; */
	*rCmb32Spcl = 0;
	if (pRDat.pMod32[HISDETNUM - 1] == 3) {
		nMod = pRDat.pMod32[HISDETNUM - 1];
		/* nT0 = pRDat.pFlg32[HISDETNUM - 1] % 2; */

		if (pRDat.mNum32[HISDETNUM - 1] < 255)	/* maximum */
			pRDat.mNum32[HISDETNUM - 1] += 1;

		/* 3-2 film combing special precessing (A-A-A) */
		if (pRDat.pFlg32[HISDETNUM - 1] == (mNDly % 5))
			*rCmb32Spcl = 1;
	} else if (pRDat.pMod22[HISDETNUM - 1] == 2) {
		nMod = pRDat.pMod22[HISDETNUM - 1];
		/* nT0 = pRDat.pFlg22[HISDETNUM - 1] % 2; */

		if (pRDat.mNum22[HISDETNUM - 1] < 255)	/* maximum */
			pRDat.mNum22[HISDETNUM - 1] += 1;
	} else {
		nMod = 0;

		pRDat.mNum32[HISDETNUM - 1] = 0;
		pRDat.mNum22[HISDETNUM - 1] = 0;
	}

	/* Only frame (t-1) */
	/* pFMReg->rFlmPstGCm = 0; */
	*rFlmPstGCm = 0;
	*frame_diff_avg = DIF02[HISDIFNUM-1] / (glb_frame_mot_num + 1);
	/*-----------------*/
	/*force entry pulldown22 to fix image jitter when play DTV*/
	/*3 channels by vlsi-yanling*/
	flm22_min = nDIF01[HISDIFNUM-1] > nDIF01[HISDIFNUM-2]
		? nDIF01[HISDIFNUM-2] : nDIF01[HISDIFNUM-1];
	flm22_th = flm22_min/2;
	avg_flag = abs(nDIF01[HISDIFNUM-1] - nDIF01[HISDIFNUM-2]) > flm22_th
		? 1:0;
	avg_flag =
		(max(nDIF01[HISDIFNUM-1], nDIF01[HISDIFNUM-2]) > (1<<16)
		&& pRDat.iHeight == 288)
		? avg_flag : 0;
	/*-----------------*/
	/* rFlmPstGCm = 1; */
	if (pRDat.pMod32[HISDETNUM - 1 - mDly] == 3) {
		nT0 = pRDat.pFlg32[HISDETNUM - 1 - mDly] % 2;
		*rFlmSltPre = nT0;
		/* Post-processing: film mode,00: global combing,
		 * 01: 2-2 film, 10: 2-3 film, 11:-others
		 */
		 *rFlmPstMod = 2;
		/* param: at least 5 field+5 */
		if (pRDat.mNum32[HISDETNUM - 1] < flm32_mim_frms ||
				flm32 == 0) {
			*rFlmSltPre = 0;
			*rFlmPstMod = 0;
		}
		nS1 = 0;
	} else if (pRDat.pMod22[HISDETNUM - 1 - mDly] == 2) {
		nT0 = pRDat.pFlg22[HISDETNUM - 1 - mDly] % 2;
		*rFlmSltPre = nT0;
		/* Post-processing: film mode,
		 * 00: global combing, 01: 2-2 film, 10: 2-3 film, 11:-others
		 */
		 *rFlmPstMod = 1;

		ntmp = (glb_frame_mot_num + glb_field_mot_num) /
				(pPar->width + 1);
		if (flm22_mim_frms > ntmp +  flm22_mim_smfrms)
			flm22_mim_numb = flm22_mim_frms - ntmp;
		else
			flm22_mim_numb = flm22_mim_smfrms;

		if (pr_pd)
			pr_info("diff02-avg=%4d, flm22_min=%4d,flm22_th=%4d, avg_flag=%d\n",
				*frame_diff_avg, flm22_min, flm22_th, avg_flag);
		if ((*frame_diff_avg > flm22_frmdif_max)
			&& (avg_flag == 0 || flm22_avg_flag == 1)) {
			ntmp = *frame_diff_avg - flm22_frmdif_max;
			if (ntmp > flm22_minus_cntmax)
				ntmp = flm22_minus_cntmax;
			if (pRDat.mNum22[HISDETNUM - 1] > ntmp)
				pRDat.mNum22[HISDETNUM - 1] =
					pRDat.mNum22[HISDETNUM - 1] - ntmp;
			else
				pRDat.mNum22[HISDETNUM - 1] = 0;
		}

		if (DIF01[HISDIFNUM-1] < DIF01[HISDIFNUM-2]) {
			/*ntmp = DIF01[HISDIFNUM-1] / (glb_field_mot_num + 1);*/
			/* min / max */
			ntmp = DIF01[HISDIFNUM-1] / (pre_fld_motnum + 1);
			dif01avg = ntmp;

			if (pr_pd)
				pr_info("diff01-avg=%4d\n", ntmp);
			if ((ntmp > flm22_flddif_max)
				&& (avg_flag == 0 || flm22_avg_flag == 1)) {
				ntmp = ntmp - flm22_flddif_max;

				if (ntmp > flm22_minus_cntmax)
					ntmp = flm22_minus_cntmax;

				if (pRDat.mNum22[HISDETNUM - 1] > ntmp)
					pRDat.mNum22[HISDETNUM - 1] =
					pRDat.mNum22[HISDETNUM - 1] - ntmp;
				else
					pRDat.mNum22[HISDETNUM - 1] = 0;
			}
			comdif = (comsumpre < comsum) ?  (comsum - comsumpre)
				: (comsumpre - comsum);
			if (pr_pd)
				pr_info("comsum=%d, comsumpre=%d, flev=%d\n",
					comsum, comsumpre, nS1);
			if ((comsum < 200) && (comsum > pPar->flm22_comnum)
				&& (comdif < flm22_comth) &&
				flm22_dif01a_flag) {
				if (nS1 < pPar->flm22_comlev)
					nS1 = 0;
				else
					nS1 = nS1 - pPar->flm22_comlev;
			} else if ((dif01avg > pPar->flm22_dif01_avgth)
				&& (avg_flag == 0 || flm22_avg_flag == 1)) {
				if (nS1 < pPar->flm22_comlev)
					nS1 = 0;
				else
					nS1 = nS1 - pPar->flm22_comlev;
			}
			if (pr_pd)
				pr_info("flev=%d\n", nS1);
			comsumpre = comsum;
		} else if (nS1pre < 100) {
			if (nS1 < pPar->flm22_comlev2)
				nS1 = 0;
			else
				nS1 = nS1 - pPar->flm22_comlev2;
		}
		nS1pre = nS1;

		/* param: at least 60 field+4 */
		if (pRDat.mNum22[HISDETNUM - 1] < flm22_mim_numb ||
				flm22 == 0) {
			*rFlmSltPre = 0;
			*rFlmPstMod = 0;
			if (pr_pd)
				pr_info("mNum22(%3d) < %03d => set to mod=0\n",
				pRDat.mNum22[HISDETNUM - 1], flm22_mim_frms);
		}
	} else {
		*rFlmSltPre = 0;
		/* Post-processing: film mode,00: global combing,
		 * 01: 2-2 film, 10: 2-3 film, 11:-others
		 */
		 *rFlmPstMod = 0;
		nS1 = 0;
	}
	if (flm22_force) {
		*rFlmSltPre = nDIF01[HISDIFNUM-1] > nDIF01[HISDIFNUM-2] ? 1 : 0;
		/* Post-processing: film mode,00: global combing,
		 * 01: 2-2 film, 10: 2-3 film, 11:-others
		 */
		*rFlmPstMod = 1;
		nS1 = 135;
	}
	pre_fld_motnum = glb_field_mot_num;

	comsum = VOFSftTop(rFlmPstGCm, rFlmSltPre, rFlmPstMod,
		rPstCYWnd0, rPstCYWnd1, rPstCYWnd2, rPstCYWnd3,
		nMod, rROCmbInf, &pRDat, pPar, pPar->height,
		pPar->width, reverse);
	if (*rFlmPstMod == 1 && *rFlmPstGCm && flm22_flag)
		*rFlmPstMod = 0;

	nT1 = pRDat.pLvlXx[HISDETNUM - 1 - mDly];
	if ((*rFlmPstMod == 0) && (nT1 > flmxx_maybe_num)
		&& (nS0 != 6) &&
		(pRDat.pMod22[HISDETNUM - 1 - mDly] != 2 || flm2224_flag)) {
		*rFlmSltPre = pRDat.pFlgXx[HISDETNUM - 1 - mDly];
		*rFlmPstMod = 4 + pRDat.pModXx[HISDETNUM - 1 - mDly];
		nS1 = pRDat.pLvlXx[HISDETNUM - 1 - mDly];
	}
	*dif01flag = 2;
	if (*rFlmPstMod == 0)
		*dif01flag = DIweavedetec(pPar, rROFldDif01[0]);
	if (num32 > 0 && *rFlmPstMod != 2)
		num32 = num32-1;
	if (pRDat.pFlg32[HISDETNUM - 1 - mDly] == 3) {
		if (DIF01[HISDIFNUM - 2] > DIF01[HISDIFNUM - 1])
			num32 = num32 + 1;
		else if (num32 > 0)
			num32 = num32 - 1;
	}
	if (modpre != *rFlmPstMod && modpre != 0 && *rFlmPstMod != 0 &&
		num32 == 0) {
		flag_pre = 1;
		num = 0;
	} else {
		if (modpre == 0 || *rFlmPstMod == 0)
			num = 0;
		else if (num <= 255)
			num = num + 1;
	}

	if (num > 5 || num32 > 0)
		flag_pre = 0;

	if (DIF01[HISDIFNUM - 2] < DIF01[HISDIFNUM - 1])
		difflag = 1;
	else
		difflag = 0;

	dif01th = (DIF01[HISDIFNUM - 2] + DIF01[HISDIFNUM - 1]) / dif01_ratio;

	if (abs(DIF01[HISDIFNUM - 2] - DIF01[HISDIFNUM - 1]) > dif01th &&
		flag_pre && flagdif01chk)
		*rFlmSltPre = difflag;
	modpre = *rFlmPstMod;

	*combing_row_num = pRDat.TCNm[HISCMBNUM - 1];
	pPar->field_count++;
	if (pPar->field_count == 0x7fffffff)
		pPar->field_count = 3;

	return nS1;
}

/* Film Detection Software implementation */
/* nDif01: Field Difference */
/* nDif02: Frame Difference */
/* WND: The index of Window */
int FlmDetSft(struct sFlmDatSt *pRDat, int *nDif01, int *nDif02,
	      int WND, struct sFlmSftPar *pPar, struct vframe_s *vf)
{
	int nT0 = 0;

	/* 3-2 */
	Flm32DetSft(pRDat, nDif02, nDif01, pPar, vf);

	/* Film2-2 Detection */
	/* debug0304 */
	nT0 = Flm22DetSft(pRDat, nDif02, nDif01, pPar);
	/* ---------------------------------------- */
	prt_flg = 0;
	return nT0;
}

/* pFlm02[0:nLEN-1] : recursive, 0-2 dif */
/* pFlm01[0:nLEN-1] : recursive, 0-1 dif */
int Flm32DetSft(struct sFlmDatSt *pRDat, int *nDif02,
		int *nDif01, struct sFlmSftPar *pPar, struct vframe_s *vf)
{
	int sFrmDifAvgRat = pPar->sFrmDifAvgRat;	/* 16;  //0~32 */
	/*  The Large Decision should be: (large>average+LgDifThrd) */
	int sFrmDifLgTDif = pPar->sFrmDifLgTDif; /* 4096 */
	int sF32StpWgt01 = pPar->sF32StpWgt01;	/* 15; */
	int sF32StpWgt02 = pPar->sF32StpWgt02;	/* 15; */
	int sF32DifLgRat = pPar->sF32DifLgRat;	/* 16; Dif>Rat*Min-->Larger */
	/* int sF32DifSmRat = 16;  //Dif*Rat<Max  --> Smaller */

	UINT8 *pFlm02 = pRDat->pFrm32;
	UINT8 *pFlm02t = pRDat->pFrm32t;
	UINT8 *pFlm01 = pRDat->pFld32;
	UINT8 *pFlg32 = pRDat->pFlg32;

	static UINT8 sFld32[6] = { 0, 0, 0, 0, 0, 0 };

	UINT8 FIX02[2][5] = { {2, 3, 4, 5, 1}, {4, 5, 1, 2, 3} };

	int nT0 = 0;
	int nT1 = 0;
	int nT2 = 0;

	int nMn = nDif02[HISDIFNUM - 1];
	int nMx = nDif02[HISDIFNUM - 1];
	int nSM = nDif02[HISDIFNUM - 1];
	UINT8 nFlg01[6] = { 0, 0, 0, 0, 0, 0 };
	UINT8 nFlg02[6] = { 0, 0, 0, 0, 0, 0 };
	UINT8 nFlg12[6] = { 0, 0, 0, 0, 0, 0 };

	int nAV10 = 0;
	int nAV11 = 0;
	int nAV12 = 0;
	int nAV1 = 0;
	int nSTP = 0;
	UINT8 nMIX = 0;

	int nFlgChk1 = 0;
	int nFlgChk2 = 0;
	int nFlgChk3 = 0; /* for Mit32VHLine */

	int luma_avg = 0;
	int flm32_dif02_gap = 0;

	int flm32_luma_th = pPar->flm32_luma_th; // APL th
	int flm32_dif02_gap_th = pPar->flm32_dif02_gap_th;

	/* ============================================= */
	/*patch for dark scenes don't into pulldown32 by vlsi yanling*/
	if (vf == NULL || !IS_VDIN_SRC(vf->source_type))
		luma_avg = flm32_luma_th;
	else {
		if (vf->prop.hist.pixel_sum > 0)
			luma_avg = vf->prop.hist.luma_sum /
				vf->prop.hist.pixel_sum;
		else
			luma_avg = flm32_luma_th;
	}
	if (luma_avg < flm32_luma_th)
		flm32_dif02_gap = flm32_dif02_gap_th * 2;
	else
		flm32_dif02_gap = flm32_dif02_gap_th;
	/*---------------------------------*/

	prt_flg = ((pr_pd >> 2) & 0x1);
	if (prt_flg)
		sprintf(debug_str, "#Dbg32:\n");

	/* ---------------------------------- */
	/* Get min/max from the last fives */
	for (nT0 = 2; nT0 <= 5; nT0++) {
		nT1 = nDif02[HISDIFNUM - nT0];
		nSM += nT1;

		if (nT1 < nMn)
			nMn = nT1;

		if (nT1 > nMx)
			nMx = nT1;
	}
	nAV10 = ((nSM - nMx + 2) >> 2);
	nAV12 = ((nSM + 3 * nMn + 4) >> 3);

	nSM = 0;
	nT2 = 0;
	for (nT0 = 1; nT0 <= 5; nT0++) {
		nT1 = nDif02[HISDIFNUM - nT0];
		if (nT1 >= nAV10) {
			nT2 += 1;
			nSM += nT1;
		}
	}
	nAV11 = (nSM - nMx + nT2 / 2) / (nT2 - 1);

	nAV1 = (sFrmDifAvgRat * nAV11 + (32 - sFrmDifAvgRat) * nAV12);
	nAV1 = ((nAV1 + 16) >> 5);

	/* for Mit32VHLine */
	if (nDif02[HISDIFNUM-1] > nDif02[HISDIFNUM-6])
		nFlgChk1 = nDif02[HISDIFNUM-1] - nDif02[HISDIFNUM-6];
	else
		nFlgChk1 = nDif02[HISDIFNUM-6] - nDif02[HISDIFNUM-1];

	/* if (pFlg32[HISDETNUM-1] == 4) { */
	/* B-B A-A-A X-Y-Z */
	/* ---------=>Sceen changed */
	if (pFlg32[HISDETNUM-1] == 4 || pFlg32[HISDETNUM-1] == 5) {
		nFlgChk3 = nFlgChk1;
		for (nT0 = 2; nT0 <= 5; nT0++) {
			nFlgChk2 = nDif02[HISDIFNUM-nT0]
					- nDif02[HISDIFNUM-nT0-5];
			if (nFlgChk2 < 0)
				nFlgChk2 = -nFlgChk2;

			/* 5-loop: maximum */
			if (nFlgChk2 > nFlgChk3)
				nFlgChk3 = nFlgChk2;
		}
		nFlgChk3 = 16 * nFlgChk3 / nAV1;
	} else
		nFlgChk3 = 255;
	/* for Mit32VHLine */

	if (pFlg32[HISDETNUM - 1] == 2 || pFlg32[HISDETNUM - 1] == 4
		|| pFlg32[HISDETNUM - 1] == 3) {
		/* ========================================== */
		if (nDif02[HISDIFNUM - 2] > nDif02[HISDIFNUM - 7])
			nFlgChk2 =
			    nDif02[HISDIFNUM - 2] - nDif02[HISDIFNUM - 7];
		else
			nFlgChk2 =
			    nDif02[HISDIFNUM - 7] - nDif02[HISDIFNUM - 2];

		if (nFlgChk1 > nFlgChk2)
			nFlgChk2 = nFlgChk1 - nFlgChk2;
		else
			nFlgChk2 = nFlgChk2 - nFlgChk1;

		nFlgChk2 = 16 * nFlgChk2 / nAV1;
		/* ============================================ */

		/* please check the DI-skateboard */
		/* the next should be 1 and 3 */
		/* dif02(flg=2 vs 1) almost same */
		/* dif02(flg=4 vs 3) almost same */
		if (nDif02[HISDIFNUM - 1] > nDif02[HISDIFNUM - 2])
			nFlgChk1 =
			    nDif02[HISDIFNUM - 1] - nDif02[HISDIFNUM - 2];
		else
			nFlgChk1 =
			    nDif02[HISDIFNUM - 2] - nDif02[HISDIFNUM - 1];

		nFlgChk1 = 16 * nFlgChk1 / nAV1;

		if (prt_flg)
			sprintf(debug_str + strlen(debug_str),
				"nFlgChk1/2/3=(%2d,%2d,%02d)\n",
			    nFlgChk1, nFlgChk2, nFlgChk3);
	} else {
		nFlgChk1 = 0;
		nFlgChk2 = 0;
	}

	nT2 = 5 * nDif02[HISDIFNUM - 1] / (nMn + sFrmDifLgTDif + 1);
	nT2 = nT2>>1;
	if (nMn <= (1 << flm32_f2fdif_min0)) {
		nSTP = nT2;
	} else {
		nSTP = flm32_dif02_ratio * (nDif02[HISDIFNUM - 1] - nMn) +
				(nAV1 - nMn + sFrmDifLgTDif) / 2;
		nSTP = nSTP / (nAV1 - nMn + sFrmDifLgTDif);

		/* ======================== */
		/* patch for DI1 3:2, [ 16 16 9 16 0] */
		if (nT2 > nSTP)
			nSTP = nT2;
		/* ======================== */
	}

	if (nSTP > 16)
		nSTP = 16;
		/*patch for dark scenes don't into pulldown32 by vlsi yanling*/
	if (((nMx + nMn/2) / (nMn + 1)) < flm32_dif02_gap)
		nSTP = 0;
	/*---------------*/
	for (nT0 = 1; nT0 < HISDETNUM; nT0++) {
		pFlm02[nT0 - 1] = pFlm02[nT0];
		pFlm02t[nT0 - 1] = pFlm02t[nT0];
	}

	if (nDif02[HISDIFNUM - 1] > (nMn + sFrmDifLgTDif) * sF32DifLgRat) {
		pFlm02t[HISDETNUM - 1] =
		    nDif02[HISDIFNUM - 1] / (nMn + sFrmDifLgTDif);
	} else {
		pFlm02t[HISDETNUM - 1] = nSTP;
	}

	pFlm02[HISDETNUM - 1] = nSTP;

	/* -------------------------------- */
	nMn = pFlm02[HISDETNUM - 1];
	nMIX = 5;
	for (nT0 = 0; nT0 < 6; nT0++) {
		nFlg02[5 - nT0] = pFlm02[HISDETNUM - 1 - nT0];
		if (nFlg02[5 - nT0] < nMn && nT0 <= 4) {
			nMn = nFlg02[5 - nT0];
			nMIX = 5 - nT0;
		}
	}
	nFlg02[nMIX] = 16 - nFlg02[nMIX];
	if (nMIX == 5)
		nFlg02[0] = 16 - nFlg02[0];

	/* -------------------------------------------- */
	/* field difference */
	/* pFlm01 */
	/* length of pFlm01/nDif01: [0:5]; */
	/* iDx: index of minimum dif02 ([0:5] */
	Cal32Flm01(pFlm01, nDif01, nMIX, pPar);
	for (nT0 = 0; nT0 < 6; nT0++) {
		if ((nT0 == FIX02[0][nMIX - 1]) || (nT0 == FIX02[1][nMIX - 1]))
			nFlg01[nT0] = pFlm01[HISDETNUM - 6 + nT0];
		else
			nFlg01[nT0] = 16 - pFlm01[HISDETNUM - 6 + nT0];
	}

	if (FIX02[0][nMIX - 1] == 5 || FIX02[1][nMIX - 1] == 5)
		nFlg01[0] = pFlm01[HISDETNUM - 6];

	/* A-A-A B-B C-C-C D-D E-E-E */
	/* 0-1-1 0-0 0-1-1 0-0 */
	for (nT0 = 0; nT0 < 6; nT0++) {

		if (nMIX == 1 && nT0 == 5) {
			nSTP =
			    sF32StpWgt02 * nFlg02[nT0] + (16 -
							  sF32StpWgt02) *
			    nFlg01[nT0];
		} else if (nMIX == 5 && nT0 == 0) {
			nSTP =
			    sF32StpWgt02 * nFlg02[nT0] + (16 -
							  sF32StpWgt02) *
			    nFlg01[nT0];
		} else if (nT0 == nMIX || nT0 == nMIX - 1) {
			nSTP =
			    sF32StpWgt02 * nFlg02[nT0] + (16 -
							  sF32StpWgt02) *
			    nFlg01[nT0];
		} else {
			nSTP =
			    sF32StpWgt01 * nFlg01[nT0] + (16 -
							  sF32StpWgt02) *
			    nFlg02[nT0];
		}

		nFlg12[nT0] = ((nSTP + 8) >> 4);
	}
	/* -------------------------------------------- */

	/* -------------------------------------------- */
	Flm32DetSub1(pRDat, nFlg12, pFlm02t, nFlg01, nFlg02, nMIX);
	/* -------------------------------------------- */

	/* 150213-patch */
	nSTP = pRDat->pFrm32[HISDETNUM - 6 + nMIX];
	for (nT1 = 5; nT1 > 0; nT1--) {
		if (nT1 != nMIX)
			nSTP += (16 - pRDat->pFrm32[HISDETNUM - 6 + nT1]);
	}

	if (nMn < (1 << flm32_f2fdif_min1) && nSTP <= 2) {
		nSTP = 0;
		for (nT1 = 4; nT1 >= 0; nT1--) {
			if (sFld32[nT1] >= pRDat->pFld32[HISDETNUM - 6 + nT1]) {
				nSTP +=
				    (sFld32[nT1] -
				     pRDat->pFld32[HISDETNUM - 6 + nT1]);
			} else {
				nSTP +=
				    (pRDat->pFld32[HISDETNUM - 6 + nT1] -
				     sFld32[nT1]);
			}
		}

		if (nSTP <= 2) {
			pRDat->pMod32[HISDETNUM - 1] = 3;
			pRDat->pFlg32[HISDETNUM - 1] = nMIX;
		}
	}

	for (nT1 = 1; nT1 <= 5; nT1++)
		sFld32[nT1 - 1] = sFld32[nT1];

	sFld32[5] = pRDat->pFld32[HISDETNUM - 5];
	/* -------------------------------------------- */

	/* ============================================= */
	/* please check the DI-skateboard */
	/* the next should be 1 and 3 */
	/* dif02(flg=2 vs 1) almost same */
	/* dif02(flg=4 vs 3) almost same */
	/* nFlgChk3: for Mit32VHLine */
	/* last: for sceen change */
	if (((nFlgChk1 > flm32_chk1_rtn) &&
		(nFlgChk3 > flm32_ck13_rtn))
		|| (nFlgChk2 > flm32_chk2_rtn)
		|| ((pFlg32[HISDETNUM-1] == 4) &&
		(nFlgChk3 > flm32_chk3_rtn))) {
		pRDat->pMod32[HISDETNUM - 1] = 0;
		pRDat->pFlg32[HISDETNUM - 1] = 0;

		if (prt_flg)
			sprintf(debug_str + strlen(debug_str),
			"Reg: ck1=%d, ck13=%d, ck2=%d, ck3=%d\n",
			flm32_chk1_rtn, flm32_ck13_rtn,
			flm32_chk2_rtn, flm32_chk3_rtn);
	}
	/* ============================================= */

	if (prt_flg) {
		sprintf(debug_str + strlen(debug_str),
			"Mod=%d, Flg=%d, Num=%3d\n",
			pRDat->pMod32[HISDETNUM - 1],
			pRDat->pFlg32[HISDETNUM - 1],
			pRDat->mNum32[HISDETNUM - 1]);
		pr_info("%s", debug_str);
	}

	return 0;
}

/* length of pFlm01/nDif01: [0:5]; */
/* iDx: index of minimum dif02 ([0:5] */
int Cal32Flm01(UINT8 *pFlm01, int *nDif01, int iDx,
		struct sFlmSftPar *pPar)
{
	int sF32Dif01A1 = pPar->sF32Dif01A1;	/* 65; */
	int sF32Dif01T1 = pPar->sF32Dif01T1;	/* 128; */

	int sF32Dif01A2 = pPar->sF32Dif01A2;	/* 65; */
	int sF32Dif01T2 = pPar->sF32Dif01T2;	/* 128; */

	int dDif05[5]; /* patch for MIT32-Line */

	int nT0 = 0;
	int nT1 = 0;
	/* int nT2=0; */
	int nSP = 0;

	int CP = nDif01[HISDIFNUM - 1];	/* Last */
	int PP = nDif01[HISDIFNUM - 2];	/* Prev */

	int nMn = ((CP < PP) ? CP : PP);
	int nMx = ((CP > PP) ? CP : PP);

	for (nT0 = 0; nT0 < HISDETNUM - 1; nT0++)
		pFlm01[nT0] = pFlm01[nT0 + 1];

	nSP = nDif01[HISDIFNUM-1];
	for (nT0 = 0; nT0 < 5; nT0++) {
		if (nDif01[HISDIFNUM-1-nT0] >= nDif01[HISDIFNUM-6-nT0])
			dDif05[nT0] = nDif01[HISDIFNUM-1-nT0]
				- nDif01[HISDIFNUM-6-nT0];
		else
			dDif05[nT0] = nDif01[HISDIFNUM-6-nT0]
				- nDif01[HISDIFNUM-1-nT0];

		if (nDif01[HISDIFNUM-1-nT0] < nSP)
			nSP = nDif01[HISDIFNUM-1-nT0];
	}

	for (nT0 = 0; nT0 < 5; nT0++)
		dDif05[nT0] = 8 * dDif05[nT0] / (nSP + 1024);


	if (iDx == 5) {
		/* Last three */
		if (nDif01[HISDIFNUM - 3] > nMx)
			nMx = nDif01[HISDIFNUM - 3];

		if (nDif01[HISDIFNUM - 3] < nMn)
			nMn = nDif01[HISDIFNUM - 3];

		nSP = 16 * (CP - nMn) + (nMx - nMn) / 2;
		nSP = nSP / (nMx - nMn + 1);
		if (nSP > 16)
			nSP = 16;

		pFlm01[HISDETNUM - 1] = nSP;
	} else if (iDx == 4) {
		nT0 = sF32Dif01T1 + ((CP * sF32Dif01A1 + 32) >> 6); /* x/64 */
		nT1 = sF32Dif01T1 + ((PP * sF32Dif01A1 + 32) >> 6); /* x/64 */

		if (nT0 <= PP) {
			pFlm01[HISDETNUM - 1] = 0;
			pFlm01[HISDETNUM - 2] = 16;
		} else if (nT1 <= CP) {
			pFlm01[HISDETNUM - 2] = 0;
			pFlm01[HISDETNUM - 1] = 16;
		} else {
			pFlm01[HISDETNUM - 2] = 8;	/* overlap */
			pFlm01[HISDETNUM - 1] = 8;	/* overlap */
		}
	} else {
		nT0 = sF32Dif01T2 + ((CP * sF32Dif01A2 + 32) >> 6); /* x/64 */
		nT1 = sF32Dif01T2 + ((PP * sF32Dif01A2 + 32) >> 6); /* x/64 */

		if (nT0 <= PP)
			pFlm01[HISDETNUM - 1] = 0;
		else if (nT1 <= CP)
			pFlm01[HISDETNUM - 1] = 16;
		else
			pFlm01[HISDETNUM - 1] = 8;	/* overlap */
	}

	nSP = dDif05[0];
	for (nT0 = 1; nT0 < 5; nT0++) {
		if (nSP < dDif05[nT0])
			nSP = dDif05[nT0]; /* maximum */
	}
	if (nSP <= 3) {
		if (iDx == 2) {
			pFlm01[HISDETNUM-1] = 16;
			pFlm01[HISDETNUM-2] = 0;
		} else if (iDx == 1) {
			pFlm01[HISDETNUM-1] = 0;
			pFlm01[HISDETNUM-2] = 16;
		}
	}

	return 0;
}

/* length: [0:5] */
/* MIX: [1~5] */
int Flm32DetSub1(struct sFlmDatSt *pRDat, UINT8 *nFlg12, UINT8 *pFlm02t,
		 UINT8 *nFlg01, UINT8 *nFlg02, UINT8 MIX)
{
	UINT8 *pFlg = pRDat->pFlg32;
	UINT8 *pMod = pRDat->pMod32;

	int CFg = pFlg[HISDETNUM - 1];
	int RFlg[5] = { 5, 1, 2, 3, 4 };
	int nT0 = 0;
	int CNT = 0;

	UINT8 MN0 = nFlg12[5];
	UINT8 MN1 = nFlg01[5];
	UINT8 MN2 = nFlg02[5];

	int ID0 = 0;
	int ID1 = 0;

	for (nT0 = 5; nT0 >= 0; nT0--) {
		if (pMod[nT0] == 3)
			CNT++;
		else
			break;
	}

	for (nT0 = 0; nT0 < HISDETNUM - 1; nT0++) {
		pMod[nT0] = pMod[nT0 + 1];
		pFlg[nT0] = pFlg[nT0 + 1];
	}

	for (nT0 = 0; nT0 < 5; nT0++) {
		if (nFlg12[nT0] < MN0)
			MN0 = nFlg12[nT0];

		if (nFlg01[nT0] < MN1)
			MN1 = nFlg01[nT0];

		if (nFlg02[nT0] < MN2)
			MN2 = nFlg02[nT0];
	}

	if (CFg == 0 && MN0 >= 10 && MN1 >= 10 && MN2 >= 10) {
		pMod[HISDETNUM - 1] = 3;
		pFlg[HISDETNUM - 1] = MIX;
	} else if (CFg != 0 && RFlg[CFg - 1] == MIX && MN0 >= 8) {
		pMod[HISDETNUM - 1] = 3;
		pFlg[HISDETNUM - 1] = MIX;
		if (CNT <= 2 && (MN1 <= 8 || MN2 <= 8))
			pFlg[HISDETNUM - 1] = 0;
	} else {
		pMod[HISDETNUM - 1] = 0;
		pFlg[HISDETNUM - 1] = 0;

		MN0 = pFlm02t[HISDETNUM - 1];
		MN1 = pFlm02t[HISDETNUM - 1];
		ID0 = 5;
		ID1 = 5;
		for (nT0 = 4; nT0 >= 1; nT0--) {
			if (pFlm02t[HISDETNUM - 6 + nT0] < MN0) {
				MN1 = MN0;
				ID1 = ID0;
				MN0 = pFlm02t[HISDETNUM - 6 + nT0];
				ID0 = nT0;
			} else if (pFlm02t[HISDETNUM - 6 + nT0] < MN1) {
				MN1 = pFlm02t[HISDETNUM - 6 + nT0];
				ID1 = nT0;
			}
		}

		MIX = ID0;
		if (ID0 == 5 && ID1 == 5) {
			if (pFlm02t[HISDETNUM - 6] > MN0)
				MN0 = pFlm02t[HISDETNUM - 6];

			CNT = 0;
			for (nT0 = 1; nT0 <= 4; nT0++) {
				if (pFlm02t[HISDETNUM - 6 + nT0] >
				    (MN0 + 2) * 32) {
					CNT++;
				}
			}

			if (CNT == 4) {
				if (CFg == 0) {
					pMod[HISDETNUM - 1] = 3;
					pFlg[HISDETNUM - 1] = MIX;
				} else if (RFlg[CFg - 1] == MIX) {
					pMod[HISDETNUM - 1] = 3;
					pFlg[HISDETNUM - 1] = MIX;
				}
			}
		} else if (MN1 > (MN0 + 2) * 32) {
			/* All >64 */
			if (ID0 != 5) {
				if (CFg == 0) {
					pMod[HISDETNUM - 1] = 3;
					pFlg[HISDETNUM - 1] = MIX;
				} else if (RFlg[CFg - 1] == MIX) {
					pMod[HISDETNUM - 1] = 3;
					pFlg[HISDETNUM - 1] = MIX;
				}
			}	/* if(ID0!=5) */
		}		/* if(MN1>(MN0+2)*32) */
	}

	return 0;
}

/* Film2-2 Detection */
int Flm22DetSft(struct sFlmDatSt *pRDat, int *nDif02,
			int *nDif01, struct sFlmSftPar *pPar)
{
	UINT8 *pFlg = pRDat->pFlg22;
	UINT8 *pMod = pRDat->pMod22;

	UINT8 *pStp = pRDat->pStp22;
	UINT8 *pSmp = pRDat->pSmp22;
	UINT8 *mNum22 = pRDat->mNum22;

	int sFlm2MinAlpha = pPar->sFlm2MinAlpha;	/* 32; // [0~63] */
	int sFlm2MinBelta = pPar->sFlm2MinBelta;	/* 32; // [0~63] */
	int sFlm20ftAlpha = pPar->sFlm20ftAlpha;	/* 16; // [0~63] */
	int sFlm2LgDifThd = pPar->sFlm2LgDifThd;	/* 4096; */
	int sFlm2LgFlgThd = pPar->sFlm2LgFlgThd;	/* 8; */
	int flm22_flag = pPar->flm22_flag;
	int flm22_comlev = pPar->flm22_comlev;
	int flm22_comlev1 = pPar->flm22_comlev1;
	int flm22_comnum = pPar->flm22_comnum;

	int dif_flag;
	int flm22_min;
	int flm22_th;

	int cFlg = pFlg[HISDETNUM - 1];
	int rFlg[4] = { 2, 3, 4, 1 };

	int nT0 = 0;
	int nT1 = 0;
	int CNT0 = 0;
	int CNT1 = 0;

	int nMn = nDif01[HISDIFNUM - 1];
	int nMx = nDif01[HISDIFNUM - 1];

	int nSM20 = nDif01[HISDIFNUM - 1];
	int nSM21 = 0;
	int nSM22 = 0;
	int nL21 = 0;
	int nL22 = 0;
	int Mx56 = 0;
	int Mn56 = 0;

	int nAV20 = 0;
	int nAV21 = 0;
	int nAV22 = 0;
	int nOfst = 0;
	int tMgn = 0;
	int BtMn = 0;
	static int num22;

	int FdTg[6];

	int nFlgChk1 = 0; /* chk1 */
	int nFlgCk20 = 0; /* chk2-0 */
	int nFlgCk21 = 0; /* chk2-1 */
	int nFlgChk3 = 0; /* chk3 */
	int nFlgChk4 = 0; /* chk4 */
	int nFlgChk5 = 0; /* chk5 */
	int nFlgChk6 = 0; /* dif02-small */

	static UINT8 nCk20Cnt;
	static UINT8 nCk21Cnt;
	/* check 2-2: for panda sequence */
	/* static UINT8 nCk22Flg[HISDETNUM]; */
	/* static UINT8 nCk22Cnt; */

	/* size of image */
	int iWidth  = pRDat->iWidth;
	int iHeight = pRDat->iHeight;
	int nFlm22Lvl = 0;
	int nSIZE = iWidth * iHeight + 1;

	prt_flg = ((pr_pd >> 3) & 0x1);
	if (prt_flg)
		sprintf(debug_str, "#Dbg22:\n");

	for (nT0 = 0; nT0 < HISDETNUM - 1; nT0++) {
		pFlg[nT0] = pFlg[nT0 + 1];
		pMod[nT0] = pMod[nT0 + 1];
		pStp[nT0] = pStp[nT0 + 1];
		pSmp[nT0] = pSmp[nT0 + 1];
		/* nCk22Flg[nT0] = nCk22Flg[nT0+1]; */
	}

	/* ========== check1/3 2-2 mode  ========== */
	/* |dif02(t-1) - dif02(t-0)| => should be small */
	/* |dif01(t-1) - (dif01(t)+dif02(t))| => should be small */
	nFlgChk1 = 0;
	nFlgChk3 = 0;
	nAV20 = 0;
	if (pFlg[HISDETNUM-1] == 0) {
		nFlgChk1 = 255;
		nFlgChk3 = 255;
		nFlgChk4 = 0;
		/* nCk22Cnt = 0; */
	} else if (pFlg[HISDETNUM-1] == 2
			|| pFlg[HISDETNUM-1] == 4) {
		for (nT0 = 1; nT0 <= 7; nT0 = nT0+2) {
			if (nDif02[HISDIFNUM-nT0] > nDif02[HISDIFNUM-nT0-1]) {
				nOfst = nDif02[HISDIFNUM-nT0]
					- nDif02[HISDIFNUM-nT0-1];
				nAV20 = nAV20 + nDif02[HISDIFNUM-nT0-1];
			} else {
				nOfst = nDif02[HISDIFNUM-nT0-1]
					- nDif02[HISDIFNUM-nT0];
				nAV20 = nAV20 + nDif02[HISDIFNUM-nT0];
			}

			/* maximum */
			if (nOfst > nFlgChk1)
				nFlgChk1 = nOfst;

			tMgn = nDif02[HISDIFNUM-nT0]+nDif01[HISDIFNUM-nT0];
			if (tMgn > nDif01[HISDIFNUM-nT0-1])
				BtMn = tMgn - nDif01[HISDIFNUM-nT0-1];
			else
				BtMn = nDif01[HISDIFNUM-nT0-1] - tMgn;

			if (BtMn > nFlgChk3)
				nFlgChk3 = BtMn;

		}
		nAV20 = nAV20>>2;
		nFlgChk1 = 16*nFlgChk1/(nAV20+1024);
		nFlgChk3 = 16*nFlgChk3/(nAV20+1024);
		if (nAV20 < (nSIZE >> flm22_stl_sft))
			mNum22[HISDETNUM-1] = 0; /* static sequence */

		if (prt_flg)
			sprintf(debug_str + strlen(debug_str),
			"nAV20(%04d) < (%04d)\n",
			nAV20, (nSIZE >> flm22_stl_sft));
	} else {
		nFlgChk1 = 0;
		nFlgChk3 = 0;

		for (nT0 = 1; nT0 <= 8; nT0 = nT0 + 2) {
			if (nDif02[HISDIFNUM-nT0] > nDif01[HISDIFNUM-nT0])
				nFlgChk4++;
		}
	}
	/* ========== check1/3 2-2 mode  ========== */

	for (nT0 = 2; nT0 <= 4; nT0++) {
		nT1 = nDif01[HISDIFNUM - nT0];

		nSM20 += nT1;

		if (nMx < nT1)
			nMx = nT1;

		if (nMn > nT1)
			nMn = nT1;
	}
	/* sum(0~5), last-6 */
	nSM20 += (nDif01[HISDIFNUM - 5] + nDif01[HISDIFNUM - 6]);
	nAV20 = (nSM20 - nMx + 2) / 5;

	nFlgChk6 = 0;
	for (nT0 = 1; nT0 <= 6; nT0++) {
		nT1 = nDif01[HISDIFNUM - nT0];
		if (nT1 >= nAV20) {
			nL21 += 1;
			nSM21 += nT1;
		}

		if (nT1 <= nAV20) {
			nL22 += 1;
			nSM22 += nT1;
		}
		nFlgChk6 += (nDif02[HISDIFNUM-nT0] >> 8);
	}
	nFlgChk6 = nFlgChk6 / 6;

	nAV21 = (nSM21 + nL21 / 2) / nL21;	/* High average */
	nAV22 = (nSM22 + nL22 / 2) / nL22;	/* Low average */
	nOfst = nAV21 - nAV22;

	if (prt_flg)
		sprintf(debug_str + strlen(debug_str),
		"LAvg=%04d\n", (nAV22/nSIZE));

	if (nAV22 > (nSIZE << 3))
		mNum22[HISDETNUM - 1] = 0;
	else if (nAV22 > (nSIZE * flm22_lavg_lg >> 3))
		mNum22[HISDETNUM - 1] = 0;

	/* ========== check2 2-2 mode  ========== */
	/* |dif01(t-0) - dif01(t-2)| => should be small */
	/* |dif01(t-0) - dif01(t-4)| => should be small */
	nFlgCk20 = 0;
	nFlgCk21 = 0;
	for (nT0 = 1; nT0 <= 4; nT0++) {
		nOfst = nDif01[HISDIFNUM-nT0] - nDif01[HISDIFNUM-nT0-2];
		if (nOfst < 0)
			nOfst = -nOfst;
		if (nOfst > nFlgCk20) /* maximum */
			nFlgCk20 = nOfst;

		nOfst = nDif01[HISDIFNUM-nT0] - nDif01[HISDIFNUM-nT0-4];
		if (nOfst < 0)
			nOfst = -nOfst;
		if (nOfst > nFlgCk21) /* maximum */
			nFlgCk21 = nOfst;
	}
	nFlgCk20 = 16*nFlgCk20/(nAV22+1024);
	nFlgCk21 = 16*nFlgCk21/(nAV22+1024);

	if (nFlgCk20 < flm22_chk20_sml) {
		if (nCk20Cnt < 255)
			nCk20Cnt++;
	} else
		nCk20Cnt = 0;

	if (nFlgCk21 < flm22_chk21_sml) {
		if (nCk21Cnt < 255)
			nCk21Cnt++;
	} else
		nCk21Cnt = 0;

	/* ========== check2 2-2 mode  ========== */

	/* ------------------------------ */
	/* Max or min of (5/6) */
	if (nDif01[HISDIFNUM - 1] > nDif01[HISDIFNUM - 2]) {
		Mx56 = nDif01[HISDIFNUM - 1];
		Mn56 = nDif01[HISDIFNUM - 2];
	} else {
		Mx56 = nDif01[HISDIFNUM - 2];
		Mn56 = nDif01[HISDIFNUM - 1];
	}
	/* ------------------------------ */

	CNT0 = 0;
	for (nT0 = 5; nT0 >= 0; nT0--) {
		if (pMod[HISDETNUM - 6 + nT0] == 2)
			CNT0++;
		else
			break;
	}

	if (CNT0 >= 1) {
		sFlm20ftAlpha = sFlm20ftAlpha - ((CNT0 * 25 + 8) >> 4);
		sFlm2MinBelta = sFlm2MinBelta - ((CNT0 * 36 + 8) >> 4);
	}

	/* water girl: part-2 */
	if (nCk21Cnt < flm22_chk21_sm2) {
		nT0 = sFlm2MinAlpha*Mn56+sFlm20ftAlpha*nOfst;
		tMgn = ((nT0+16)>>5);

		nT1 = sFlm2MinBelta*Mn56;
		BtMn = ((nT1+32)>>6);

		/* ----------------------------------- */
		/* int *pStp = pRDat->pStp22; */
		/* int *pSmp = pRDat->pSmp22; */
		nT0 = 16*(Mx56-tMgn) + (BtMn+sFlm2LgDifThd)/2;
		nT1 = nT0/(BtMn+sFlm2LgDifThd);
		if (nT1 > 16)
			nT1 = 16;
		else if (nT1 < 0)
			nT1 = 0;
	} else {
		nT1 = 16;
	}

	pStp[HISDETNUM - 1] = nT1;
	if (Mx56 == nDif01[HISDIFNUM - 1])
		pSmp[HISDETNUM - 1] = 1;
	else
		pSmp[HISDETNUM - 1] = 0;

	/* ------------------------------------ */
	CNT0 = 0;
	CNT1 = 0;
	for (nT0 = 0; nT0 < 6; nT0++) {
		if (pStp[HISDETNUM - 6 + nT0] >= sFlm2LgFlgThd) {
			FdTg[CNT0] = nT0;
			CNT0++;
		}
		CNT1 += pSmp[HISDETNUM - 6 + nT0];
	}

	nT0 = 0;
	nT1 = 0;
	nFlgChk5 = 0;
	if (CNT0 == 6 && CNT1 == 3) {
		if (pSmp[HISDETNUM - 6] && pSmp[HISDETNUM - 4] &&
			pSmp[HISDETNUM - 2]) {
			nT1 = 2;
			if (cFlg != 0) {
				if (cFlg % 2 == 0)
					nT0 = rFlg[cFlg - 1];
			} else {
				nT0 = 1;
			}
		} else if (pSmp[HISDETNUM - 5] && pSmp[HISDETNUM - 3] &&
			pSmp[HISDETNUM - 1]) {	/* All 1 */
			nT1 = 2;
			if (cFlg != 0) {
				if (cFlg % 2 == 1)
					nT0 = rFlg[cFlg - 1];
			} else {
				nT0 = 4;
			}
		}

		/* --------------------------------------- */
		/* patch for toilet paper */
		/* Low average avg>(totoal*x) x>1 */
		/* tMgn = (nAV22 * 64) >> 8; */
		/* if(tMgn > 720*240) */
		/* if(tMgn > iWidth*iHeight*32) */ /*toilet paper*/
		/* parameter */
		nOfst = nAV21 - nAV22;
		if (nAV22 > (nSIZE << flm22_lavg_sft)) /* low average */
			nFlgChk5 = 16;
		else if ((nAV22 << 4) > (nSIZE * flm22_chk5_avg))
			nFlgChk5 = nAV22 / nSIZE;
		else if (nOfst < ((nAV22 * 46) >> 7))
			nFlgChk5 = (nAV22 << 2) / (nOfst + 32);

		if (nFlgChk5 > 32)
			nFlgChk5 = 32;
		/* --------------------------------------- */

		nL22 = (nSIZE >> 9) + 1;
		if (nFlgChk6 < nL22)
			nFlgChk6 = nL22 / (nFlgChk6 + 1);
		else if (nFlgChk6 > (nL22 << 1))
			nFlgChk6 = nFlgChk6 / nL22;
		else
			nFlgChk6 = 0;

		if (nFlgChk6 > flm22_chk6_max)
			nFlgChk6 = flm22_chk6_max;
	}
	pFlg[HISDETNUM - 1] = nT0;
	pMod[HISDETNUM - 1] = nT1;
	/* ----------------------------------- */

	/* for panda */
	/* check bug */
	/*
	 * if (pFlg[HISDETNUM-1] & 0x1) {
	 *	nCk22Flg[HISDETNUM-1] = nT0;

	 *	if (nT0 == 0)
	 *		nCk22Cnt = nCk22Cnt+1;
	 *	else
	 *		nCk22Cnt = 0;

	 *	if (nCk22Cnt > 254)
	 *		nCk22Cnt = 254;
	 * }
	 */

	/* debug 2-2 mode */
	/* if(pr_pd && (nT0 != 0) && pFlg[HISDETNUM-1]!=0) { */
	if (prt_flg && (pFlg[HISDETNUM-1] & 0x1)) {
		if (nT0 != 0) {
			sprintf(debug_str + strlen(debug_str),
				"nCk1/3/4=(%2d,%2d,%2d)\n",
				nFlgChk1, nFlgChk3, nFlgChk4);
			sprintf(debug_str + strlen(debug_str),
				"nCk20/1Cnt=(%2d,%2d)\n",
				nCk20Cnt, nCk21Cnt);
		}
	}

	/* ========== check2 2-2 mode  ========== */
	nFlm22Lvl	= (mNum22[HISDETNUM-1] >> 2);
	if (nFlm22Lvl	> 64)
		nFlm22Lvl	= 64;

	/* panda */
	/* if (pFlg[HISDETNUM-1] && nCk22Cnt < 20) */
	/*	nFlm22Lvl = nFlm22Lvl + nCk22Cnt - 20; */

	/* 2-2 but with combing: force dejaggies */
	/* return information */
	if (nFlgChk1 < flm22_anti_chk1) {
		nT1 = ((flm22_anti_chk1 - nFlgChk1) >> 2);
		nFlm22Lvl += nT1;
	}

	nT1 = ((nCk20Cnt > nCk21Cnt) ? nCk20Cnt : nCk21Cnt);
	if (nT1 > 128)
		nT1 = 128;
	nT1 = ((nT1 + 4) >> 3);
	nFlm22Lvl += nT1;

	if (nFlgChk3 < flm22_anti_chk3) {
		nT1 = ((flm22_anti_chk3 - nFlgChk3) >> 3);
		nFlm22Lvl += nT1;
	}

	if (nFlgChk4 < flm22_anti_chk4) {
		nT1 = ((flm22_anti_chk4 - nFlgChk4) >> 3);
		nFlm22Lvl += nT1;
	}

	/* for sony-mp3 */
	if (flm22_anti_ck141 < flm22_anti_ck140)
		flm22_anti_ck141 = flm22_anti_ck140;
	nT0 = flm22_anti_ck141 - flm22_anti_ck140;

	if ((nFlgChk1 > flm22_anti_ck140) ||
		(nFlgChk4 > flm22_anti_ck141)) {
		if (nFlgChk1 > (nFlgChk4 + nT0))
			nT1 = nFlgChk1 - flm22_anti_ck140;
		else
			nT1 = nFlgChk4 - flm22_anti_ck141;

		if (nT1 > 128)
			nT1 = 128;

		nT1 = (nT1>>2);

		nFlm22Lvl -= nT1;
	}
	/* ---------------------- */
	/*DI:PQ patch fix 480i error into pulldown22(by yanling)*/
	flm22_min = nDif01[HISDIFNUM-1] > nDif01[HISDIFNUM-2]
		? nDif01[HISDIFNUM-2] : nDif01[HISDIFNUM-1];
	flm22_th = flm22_min/2;
	dif_flag = abs(nDif01[HISDIFNUM-1]-nDif01[HISDIFNUM-2])
		> flm22_th ? 1:0;
	dif_flag =
		max(nDif01[HISDIFNUM-1], nDif01[HISDIFNUM-2]) > (1<<16) ?
		dif_flag : 0;
	if (flm22_flag && (dif_flag || pPar->flm22_avg_flag)) {
	/* ---------------------- */
		if (pFlg[HISDETNUM-1] == 3
				|| pFlg[HISDETNUM-1] == 1) {
			if (comsum > flm22_comnum) {
				if (num22 < 30)
					num22 = num22 + 1;
				else
					nFlm22Lvl = nFlm22Lvl + flm22_comlev;
			} else {
				num22 = 0;
				nFlm22Lvl = nFlm22Lvl - flm22_comlev;
			}
			/* if(prt_flg)
			 * pr_info("nFlm22Lvl = %d, comsum=%d,num22=%d,"
			 * "flm22_comnum=%d,flm22_flag=%d\n",
			 * nFlm22Lvl,comsum,num22,flm22_comnum,flm22_flag);
			 */
		}
		if (nFlgCk20 < flm22_chk20_sml)
			nFlm22Lvl = nFlm22Lvl + flm22_comlev1 - nFlgCk20;
		if (nFlgCk21 < flm22_chk21_sml)
			nFlm22Lvl = nFlm22Lvl + flm22_comlev1 - nFlgCk21;
		if (prt_flg) {
			pr_info("nFlm22Lvl=%d, nFlgCk20=%d, nFlgCk21=%d,flm22_min=%d,flm22_th=%d\n",
				nFlm22Lvl, nFlgCk20, nFlgCk21,
				flm22_min, flm22_th);
		}
	}
	/* for sony-mp3 */

	nFlm22Lvl -= nFlgChk5;
	nFlm22Lvl -= nFlgChk6;

	if (nFlm22Lvl < 0)
		nFlm22Lvl = 0;

	if (prt_flg) {
		sprintf(debug_str + strlen(debug_str),
			"Mod=%d, Flg=%d, Num=%3d, Lvl=%3d\n",
			pMod[HISDETNUM - 1], pFlg[HISDETNUM - 1],
			mNum22[HISDETNUM-1], nFlm22Lvl);

		pr_info("%s", debug_str);
	}

	return nFlm22Lvl;
}
static int DIweavedetec(struct sFlmSftPar *pPar, int nDif01)
{
	int dif01th = 0;
	int dif01rate = pPar->dif01rate;
	int flag_di01th = pPar->flag_di01th;
	int numthd = pPar->numthd;
	static int numdif;
	static int predifflag = 2;
	static int predif01;
	static int difflag;

	dif01th	= (predif01+nDif01)/dif01rate;
	if (abs(predif01 - nDif01) < dif01th && flag_di01th)
		difflag = 2;
	else {
		if (pr_pd)
			pr_info("predif01=%d,dif01=%d,predifflag=%d\n",
				predif01, nDif01, predifflag);
		if (predif01 < nDif01)
			difflag = 1;
		else
			difflag = 0;
		if (difflag^predifflag) {
			if (numdif <= 255)
				numdif = numdif + 1;
			predifflag = difflag;
		} else if (numdif > numthd) {
			numdif = 0;
			difflag = difflag^1;
			predifflag = difflag;
		} else {
			predifflag = difflag;
			difflag = 2;
		}
		if (pr_pd)
			pr_info("difflag=%d\n", difflag);
	}
	if (pr_pd)
		pr_info("difflag=%d\n", difflag);
	predif01 = nDif01;
	return difflag;
}

