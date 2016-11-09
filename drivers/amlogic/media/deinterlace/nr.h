/*
 * drivers/amlogic/media/deinterlace/nr.h
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

#ifndef _DNR_H
#define _DNR_H

struct dnr_param_s {
	char *name;
	int *addr;
};

#define dnr_param_t struct dnr_param_s

struct DNR_PRM_s {
	int prm_sw_gbs_ctrl;
	int prm_gbs_vldcntthd;
	int prm_gbs_cnt_min;
	int prm_gbs_ratcalcmod;
	int prm_gbs_ratthd[3];
	int prm_gbs_difthd[3];
	int prm_gbs_bsdifthd;
	int prm_gbs_calcmod;
	int sw_gbs;
	int sw_gbs_vld_flg;
	int sw_gbs_vld_cnt;
	int prm_hbof_minthd;
	int prm_hbof_ratthd0;
	int prm_hbof_ratthd1;
	int prm_hbof_vldcntthd;
	int sw_hbof;
	int sw_hbof_vld_flg;
	int sw_hbof_vld_cnt;
	int prm_vbof_minthd;
	int prm_vbof_ratthd0;
	int prm_vbof_ratthd1;
	int prm_vbof_vldcntthd;
	int sw_vbof;
	int sw_vbof_vld_flg;
	int sw_vbof_vld_cnt;
};/* used for software */
#define DNR_PRM_t struct DNR_PRM_s
/* software parameters initialization£¬ initializing before used */
void nr_init(struct device *dev);
void di_nr_init(void);
int global_bs_calc_sw(int *pGbsVldCnt,
		      int *pGbsVldFlg,
		      int *pGbs,
		      int nGbsStatLR,
		      int nGbsStatLL,
		      int nGbsStatRR,
		      int nGbsStatDif,
		      int nGbsStatCnt,
		      int prm_gbs_vldcntthd, /* prm below */
		      int prm_gbs_cnt_min,
		      int prm_gbs_ratcalcmod,
		      int prm_gbs_ratthd[3],
		      int prm_gbs_difthd[3],
		      int prm_gbs_bsdifthd,
		      int prm_gbs_calcmod);

int hor_blk_ofst_calc_sw(int *pHbOfVldCnt,
			 int *pHbOfVldFlg,
			 int *pHbOfst,
			 int nHbOfStatCnt[32],
			 int nXst,
			 int nXed,
			 int prm_hbof_minthd,
			 int prm_hbof_ratthd0,
			 int prm_hbof_ratthd1,
			 int prm_hbof_vldcntthd,
			 int nRow,
			 int nCol);

int hor_blk_ofst_calc_sw(int *pHbOfVldCnt,
			 int *pHbOfVldFlg,
			 int *pHbOfst,
			 int nHbOfStatCnt[32],
			 int nXst,
			 int nXed,
			 int prm_hbof_minthd,
			 int prm_hbof_ratthd0,
			 int prm_hbof_ratthd1,
			 int prm_hbof_vldcntthd,
			 int nRow,
			 int nCol);

int ver_blk_ofst_calc_sw(int *pVbOfVldCnt,
			 int *pVbOfVldFlg,
			 int *pVbOfst,
			 int nVbOfStatCnt[32],
			 int nYst,
			 int nYed,
			 int prm_vbof_minthd,
			 int prm_vbof_ratthd0,
			 int prm_vbof_ratthd1,
			 int prm_vbof_vldcntthd,
			 int nRow,
			 int nCol);
void run_dnr_in_irq(unsigned short nCol, unsigned short nRow);
#endif

