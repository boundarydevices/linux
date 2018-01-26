/*
 * drivers/amlogic/media/deinterlace/film_mode_fmw/film_vof_soft.h
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

#ifndef _FLMVOFSFT_H_
#define _FLMVOFSFT_H_
#include <linux/kernel.h>
/* Film Detection and VOF detection Software implementation */
/* Designer: Xin.Hu@amlogic.com */
/* Date: 12/06/13 */

/* Difference Windows Number (Last one is the global/total dif) */
/* 5wind+global */
/* #define DIFWNDNUM 6 */
#define HISDIFNUM 10
#define HISCMBNUM 10
/* detection history information */
#define HISDETNUM 6
/* The number of VOF window */
#define VOFWNDNUM 4

#define PDXX_PT_NUM 7

/* 288Row, 1bit/row -> (288/32)=9 */
#define ROWCMBNUM 288
#define ROWCMBLEN 9

#ifndef UINT32
#define UINT32 unsigned int
#endif

#ifndef UINT8
#define UINT8 unsigned char
#endif

#ifndef UShort
#define UShort unsigned short
#endif

extern uint pr_pd;
extern bool prt_flg;
extern char debug_str[];

/* Software: Film Detection and VOF parameters */
struct sFlmDatSt {
	UINT8 pFlg32[HISDETNUM]; /* history information */
	UINT8 pMod32[HISDETNUM];
	UINT8 mNum32[HISDETNUM];

	UINT8 pFld32[HISDETNUM];
	UINT8 pFrm32[HISDETNUM];
	UINT8 pFrm32t[HISDETNUM]; /* history information: spacial processing */

	UINT8 pFlg22[HISDETNUM];
	UINT8 pMod22[HISDETNUM];
	UINT8 mNum22[HISDETNUM];

	UINT8 pStp22[HISDETNUM];
	UINT8 pSmp22[HISDETNUM];

	UINT8 pModXx[HISDETNUM]; /* mode */
	UINT8 pFlgXx[HISDETNUM]; /* pre-1, nxt-0 */
	UINT8 pLvlXx[HISDETNUM]; /* mode level */

	int TCNm[HISCMBNUM];/* history: the number of combing-rows */

	UINT32 *rROFrmDif02;

	/* size of the image */
	int iHeight;
	int iWidth;
};

/* Software parameters */
struct sFlmSftPar {
	/* software */
	int sFrmDifAvgRat;	/* 16;  //0~32 */
	/* 4096; //The Large Decision should be: (large>average+LgDifThrd) */
	int sFrmDifLgTDif;
	int sF32StpWgt01;	/* 15; */
	int sF32StpWgt02;	/* 15; */
	int sF32DifLgRat;	/* 16;  //Dif>Rat*Min  --> Larger */

	int sFlm2MinAlpha;	/* = 32; // [0~63] */
	int sFlm2MinBelta;	/* = 32; // [0~63] */
	int sFlm20ftAlpha;	/* = 16; // [0~63] */
	int sFlm2LgDifThd;	/* = 4096; // [0~63] %LgDifThrd */
	int sFlm2LgFlgThd;	/* = 8; */

	int sF32Dif01A1;	/* 65; */
	int sF32Dif01T1;	/* 128; */
	int sF32Dif01A2;	/* 65; */
	int sF32Dif01T2;	/* 128; */

	int rCmbRwMinCt0;	/* 8; //for film 3-2 */
	int rCmbRwMinCt1;	/* =7; //for film 2-2 */

	UINT8 sFlm32NCmb;	/* absolute no combing for film 32 */

	/* pre-processing (t-0), post-processing f(t-mPstDlyPre); // No RTL */
	int mPstDlyPre;
	/* pre-processing (t-0), pre-processing f(t+mNxtDlySft); default=1 */
	int mNxtDlySft;
	int cmb22_nocmb_num;
	int flm22_en;
	int flm32_en;
	int flm22_flag;
	int flm2224_flag;
	int flm22_comlev;
	int flm22_comlev1;
	int flm22_comlev2;
	int flm22_comnum;
	int flm22_comth;
	int flm22_dif01_avgth;
	int dif01rate;
	int flag_di01th;
	int numthd;

	UINT32 sF32Dif02M0;	/* mpeg-4096, cvbs-8192 */
	UINT32 sF32Dif02M1;	/* mpeg-4096, cvbs-8192 */
	unsigned int field_count;
	unsigned short width;
	unsigned short height;
};

struct FlmModReg_t {
	UINT32 rROFrmDif02[6];	/* Read only */
	UINT32 rROFldDif01[6];	/* Read only */
	UINT32 rROCmbInf[9];/* Inf[0]-[31:0], First-[31], Lst-[0] (Only 0/1) */
};

struct FlmDectRes {
	UINT8 rCmb32Spcl;
	UINT8 rFlmPstGCm;
	UINT8 rFlmSltPre;
	UINT8 rFlmPstMod;
	UINT8 dif01flag;
	UShort rPstCYWnds[5][3];
	UShort rF22Flag;
};

UINT8 FlmVOFSftInt(struct sFlmSftPar *pPar);
int FlmModsDet(struct sFlmDatSt *pRDat, int nDif01, int nDif02);

/*  */
int FlmVOFSftTop(UINT8 *rCmb32Spcl, unsigned short *rPstCYWnd0,
	unsigned short *rPstCYWnd1, unsigned short *rPstCYWnd2,
	unsigned short *rPstCYWnd3, unsigned short *rPstCYWnd4,
	UINT8 *rFlmPstGCm, UINT8 *rFlmSltPre, UINT8 *rFlmPstMod,
	UINT8 *dif01flag, UINT32 *rROFldDif01, UINT32 *rROFrmDif02,
	UINT32 *rROCmbInf, UINT32 glb_frame_mot_num,
	UINT32 glb_field_mot_num, unsigned int *cmb_row_num,
	unsigned int *frame_diff_avg, struct sFlmSftPar *pPar, bool reverse);

/* length of pFlm01/nDif01: [0:5]; */
/* iDx: index of minimum dif02 ([0:5] */
int Cal32Flm01(UINT8 *pFlm01, int *nDif01, int iDx, struct sFlmSftPar *pPar);

/* Film Detection Software implementation */
/* nDif01: Fild Difference */
/* nDif02: Frame Difference */
/* WND: The index of Window */
int FlmDetSft(struct sFlmDatSt *pRDat, int *nDif01, int *nDif02, int WND,
	      struct sFlmSftPar *pPar);

int VOFDetSub1(int *PREWV, int *nCNum, int nMod, UINT32 *nRCmb, int nROW,
	       struct sFlmSftPar *pPar);

/* Video on Film Detection Software implementaion */
int VOFDetSft(int *VOFWnd, int *nCNum, int *nGCmb,
	      UINT32 HSCMB[HISCMBNUM][ROWCMBLEN], int nMod, UINT8 *PREWV,
	      int nROW, struct sFlmSftPar *pPar);

/*  */
int Flm32DetSft(struct sFlmDatSt *pRDat, int *nDif02, int *nDif01,
		struct sFlmSftPar *pPar);

/* Film2-2 Detection */
int Flm22DetSft(struct sFlmDatSt *pRDat, int *nDif02,
			int *nDif01, struct sFlmSftPar *pPar);

/* length: [0:5] */
/* MIX: [1~5] */
int Flm32DetSub1(struct sFlmDatSt *pRDat, UINT8 *nFlg12, UINT8 *pFlm02t,
		 UINT8 *nFlg01, UINT8 *nFlg02, UINT8 MIX);
int VOFSftTop(UINT8 *rFlmPstGCm, UINT8 *rFlmSltPre, UINT8 *rFlmPstMod,
	      UShort *rPstCYWnd0, UShort *rPstCYWnd1, UShort *rPstCYWnd2,
	      UShort *rPstCYWnd3, int nMod, UINT32 *rROCmbInf,
	      struct sFlmDatSt *pRDat, struct sFlmSftPar *pPar,
	      int nROW, int nCOL, bool reverse);
#endif
