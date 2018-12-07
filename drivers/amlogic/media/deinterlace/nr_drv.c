/*
 * drivers/amlogic/media/deinterlace/nr_drv.c
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
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/cpu_version.h>
#include "register.h"
#include "register_nr4.h"
#include "nr_drv.h"
#include "deinterlace.h"

static DNR_PRM_t dnr_param;
static struct NR_PARM_s nr_param;
static bool dnr_pr;
module_param(dnr_pr, bool, 0644);
MODULE_PARM_DESC(dnr_pr, "/n print dnr debug information /n");

static bool dnr_dm_en;
module_param(dnr_dm_en, bool, 0644);
MODULE_PARM_DESC(dnr_dm_en, "/n dnr dm enable debug /n");

static bool dnr_en = true;
module_param_named(dnr_en, dnr_en, bool, 0644);

static unsigned int nr2_en = 0x1;
module_param_named(nr2_en, nr2_en, uint, 0644);

static bool nr_ctrl_reg;

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
			  int prm_gbs_calcmod)
{
	int nMax, nMin;
	int nDif0, nDif1, nDif2;
	int nDif, nRat;
	int nCurGbs;

	nMax = max(max(nGbsStatLR, nGbsStatLL), nGbsStatRR);
	nMin = min(min(nGbsStatLR, nGbsStatLL), nGbsStatRR);

	nDif0 = nMax == 0 ? 0 : ((nMax - nMin) << 9)/nMax;
	nDif0 = min(511, nDif0);

	nDif1 = nGbsStatLR == 0 ? 0 :
	(abs(nGbsStatLR - (nGbsStatLL + nGbsStatRR)/2) << 9)/nGbsStatLR;
	nDif1 = min(511, nDif1);

	nDif2 = nGbsStatLR == 0 ? 0 :
	(abs(nGbsStatLR - max(nGbsStatLL, nGbsStatRR)) << 9)/nGbsStatLR;
	nDif2 = min(511, nDif2);

	if (prm_gbs_ratcalcmod == 0)
		nRat = (nGbsStatLR << 4) / max(prm_gbs_cnt_min, nGbsStatCnt);
	else
		nRat = (nGbsStatDif << 4) / max(prm_gbs_cnt_min, nGbsStatCnt);

	nDif = (prm_gbs_calcmod == 0) ? nDif0 :
(prm_gbs_calcmod == 1 ? nDif1 : nDif2);

	if (nGbsStatLR < max(nGbsStatLL, nGbsStatRR)) {
		if (nGbsStatCnt <= prm_gbs_cnt_min || nRat <= prm_gbs_ratthd[0])
			nCurGbs = 0;
		else if (nRat <= prm_gbs_ratthd[1])
			nCurGbs = 1;
		else if (nRat <= prm_gbs_ratthd[2])
			nCurGbs = 2;
		else
			nCurGbs = 3;
	} else {
		if (nGbsStatCnt <= prm_gbs_cnt_min || nDif <= prm_gbs_difthd[0])
			nCurGbs = 0;
		else if (nDif <= prm_gbs_difthd[1])
			nCurGbs = 1;
		else if (nDif <= prm_gbs_difthd[2])
			nCurGbs = 2;
		else
			nCurGbs = 3;
	}

	/*	*/
	if ((nCurGbs != 0 && 0 == *pGbs) ||
(nCurGbs != 0 && abs(nCurGbs - *pGbs) <= prm_gbs_bsdifthd))
		(*pGbsVldCnt)++;
	else
		*pGbsVldCnt = 0;

	if (*pGbsVldCnt >= prm_gbs_vldcntthd)
		*pGbsVldFlg = 1;
	else
		*pGbsVldFlg = 0;

	*pGbs = nCurGbs;

	/* print debug info. */
	/* printk("GBS info at Field: LR = %6d, LL = %6d, RR = %6d.\n",
	 * nGbsStatLR, nGbsStatLL, nGbsStatRR;
	 */

	return 0;
}

#ifdef DNR_HV_SHIFT
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
			 int nCol)
{
	int i = 0;

	int nCurHbOfst = 0;
	int nRat0 = 0, nRat1 = 0;

	int nMax1 = 0;
	int nMax2 = 0;
	int nMaxIdx = 0;

	/* get 2 maximum, move to RTL part */
	nMax1 = nMax2 = 0;
	for (i = 0; i < 8; i++) {
		if (nHbOfStatCnt[i] > nMax1) {
			nMax2 = nMax1;
			nMax1 = nHbOfStatCnt[i];
			nMaxIdx = i;
		} else if (nHbOfStatCnt[i] > nMax2) {
			nMax2 = nHbOfStatCnt[i];
		}
	} /* i */

	/* decide if offset valid */
	nCurHbOfst = -1;
	nRat0 = 256*nMax1/((nXed - nXst)/8)/nRow;
	nRat1 = 128*nMax1/max(nMax2, prm_hbof_minthd);
	if (nRat0 >= prm_hbof_ratthd0 && nRat1 >= prm_hbof_ratthd1)
		nCurHbOfst = (nMaxIdx+1)%8;

	if (nCurHbOfst == *pHbOfst)
		(*pHbOfVldCnt)++;
	else
		*pHbOfVldCnt = 0;

	if (*pHbOfVldCnt >= prm_hbof_vldcntthd)
		*pHbOfVldFlg = 1;
	else
		*pHbOfVldFlg = 0;

	*pHbOfst = (nCurHbOfst == -1) ? 0 : nCurHbOfst;

	/* print for debug
	 * printk("Hoff info at Field: ");
	 * for ( i = 0; i < 32; i++ ) {
	 * printk("%5d, ",  nHbOfStatCnt[i]);
	 * }
	 */
	if (dnr_pr) {
		pr_dbg("Max1 = %5d, Max2 = %5d, MaxIdx = %5d, Rat0 = %5d,Rat1 = %5d.\n",
				nMax1, nMax2, nMaxIdx, nRat0, nRat1);
		pr_dbg("CurHbOfst = %5d, HbOfVldFlg = %d, HbOfVldCnt = %d.\n",
				nCurHbOfst, *pHbOfVldFlg, *pHbOfVldCnt);
	}

	return 0;
}
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
			 int nCol)
{
	int i = 0;

	int nCurVbOfst = 0;
	int nRat0 = 0, nRat1 = 0;

	int nMax1 = 0;
	int nMax2 = 0;
	int nMaxIdx = 0;

	/* get 2 maximum, move to RTL part */
	nMax1 = nMax2 = 0;
	for (i = 0; i < 8; i++) {
		if (nVbOfStatCnt[i] > nMax1) {
			nMax2 = nMax1;
			nMax1 = nVbOfStatCnt[i];
			nMaxIdx = i;
		}
	else if (nVbOfStatCnt[i] > nMax2) {
		nMax2 = nVbOfStatCnt[i];
		}
	}

	/* decide if offset valid */
	nCurVbOfst = -1;
	nRat0 = 256*nMax1/((nYed - nYst)/8)/nCol;
	nRat1 = 128*nMax1/max(nMax2, prm_vbof_minthd);
	if (nRat0 >= prm_vbof_ratthd0 && nRat1 >= prm_vbof_ratthd1)
		nCurVbOfst = (nMaxIdx+1)%8;

	if (nCurVbOfst == *pVbOfst)
		(*pVbOfVldCnt)++;
	else
		*pVbOfVldCnt = 0;

	if (*pVbOfVldCnt >= prm_vbof_vldcntthd)
		*pVbOfVldFlg = 1;
	else
		*pVbOfVldFlg = 0;

	*pVbOfst = (nCurVbOfst == -1) ? 0 : nCurVbOfst;

	/* print for debug
	 * printk("Voff info at Field: ");
	 * for ( i = 0; i < 32; i++ ) {
	 * printk("%5d, ",  nVbOfStatCnt[i]);
	 * }//i
	 * //printk("Max1 = %5d, Max2 = %5d, MaxIdx = %5d, Rat0 = %5d,
	 * Rat1 = %5d, CurVbOfst = %5d, VbOfVldFlg = %d, VbOfVldCnt = %d.\n"
	 * nMax1, nMax2, nMaxIdx, nRat0, nRat1, nCurVbOfst, *pVbOfVldFlg,
	 * *pVbOfVldCnt);
	 */
	return 0;
}
#endif
static void dnr_config(struct DNR_PARM_s *dnr_parm_p,
		unsigned short width, unsigned short height)
{
	unsigned short border_offset = dnr_parm_p->dnr_stat_coef;

	DI_Wr(DNR_HVSIZE, width<<16|height);
	DI_Wr(DNR_STAT_X_START_END, (((border_offset<<3)&0x3fff) << 16)
		|((width-((border_offset<<3)+1))&0x3fff));
	DI_Wr(DNR_STAT_Y_START_END, (((border_offset<<3)&0x3fff) << 16)
		|((height-((border_offset<<3)+1))&0x3fff));
	DI_Wr(DNR_DM_CTRL, Rd(DNR_DM_CTRL)|(1 << 11));
	DI_Wr_reg_bits(DNR_CTRL, dnr_en?1:0, 16, 1);
	/* dm for sd, hd will slower */
	if (is_meson_tl1_cpu() || is_meson_tm2_cpu())
		DI_Wr(DNR_CTRL, 0x1df00 | (0x03 << 18)); //5 line
	else
		DI_Wr(DNR_CTRL, 0x1df00);
	if (is_meson_gxlx_cpu()) {
		/* disable chroma dm according to baozheng */
		DI_Wr_reg_bits(DNR_DM_CTRL, 0, 8, 1);
		DI_Wr(DNR_CTRL, 0x1dd00);
	} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX)) {
		/*disable */
		if (width > 1280) {
			DI_Wr_reg_bits(DNR_DM_CTRL, 0, 8, 1);
			/* disable dm for 1080 which will cause pre timeout*/
			DI_Wr_reg_bits(DNR_DM_CTRL, 0, 9, 1);
		} else {
			DI_Wr_reg_bits(DNR_DM_CTRL, 1, 8, 1);
			DI_Wr_reg_bits(DNR_DM_CTRL, dnr_dm_en, 9, 1);
		}
	} else {
		if (width >= 1920)
			DI_Wr_reg_bits(DNR_DM_CTRL, 0, 9, 1);
		else
			DI_Wr_reg_bits(DNR_DM_CTRL, dnr_dm_en, 9, 1);
	}
}
static void nr4_config(struct NR4_PARM_s *nr4_parm_p,
		unsigned short width, unsigned short height)
{
	unsigned short val = 0;

	val = nr4_parm_p->border_offset;
	/* luma enhancement*/
	DI_Wr(NR4_MCNR_LUMA_STAT_LIMTX, (val<<16) | (width-val-1));
	DI_Wr(NR4_MCNR_LUMA_STAT_LIMTY, (val<<16) | (height-val-1));
	/* noise meter */
	DI_Wr(NR4_NM_X_CFG, (val<<16) | (width-val-1));
	DI_Wr(NR4_NM_Y_CFG, (val<<16) | (height-val-1));
	/* enable nr4 */
	DI_Wr_reg_bits(NR4_TOP_CTRL, 1, 16, 1);
	DI_Wr_reg_bits(NR4_TOP_CTRL, 1, 18, 1);
	DI_Wr_reg_bits(NR4_TOP_CTRL, 1, 3, 1);
	DI_Wr_reg_bits(NR4_TOP_CTRL, 1, 5, 1);
	nr4_parm_p->width = width - val - val - 1;
	nr4_parm_p->height = height - val - val - 1;
}
/*
 * line buffer ctrl such as 5 lines or 3 lines
 * yuv444 or yuv422
 */
static void linebuffer_config(unsigned short width)
{
	unsigned short val = 0;
	unsigned short line5_444 = 1368, line5_422 = 2052;
	unsigned short line3_444 = 2736;

	if (is_meson_txhd_cpu()) {
		line5_444 = 640;
		line5_422 = 960;
		line3_444 = 1280;
	}
	if (width <= line5_444)
		val = 3;
	else if (width <= line5_422)
		val = 1;
	else if (width <= line3_444)
		val = 2;
	else
		val = 0;
	/* line buffer no gate clock */
	DI_Wr_reg_bits(LBUF_TOP_CTRL, 0, 20, 6);
	DI_Wr_reg_bits(LBUF_TOP_CTRL, val, 16, 2);
}

static void nr2_config(unsigned short width, unsigned short height)
{
	if (is_meson_txlx_cpu() || is_meson_g12a_cpu() ||
		is_meson_g12b_cpu() || is_meson_tl1_cpu() ||
		is_meson_sm1_cpu() || is_meson_tm2_cpu()) {
		DI_Wr_reg_bits(NR4_TOP_CTRL, nr2_en, 2, 1);
		DI_Wr_reg_bits(NR4_TOP_CTRL, nr2_en, 15, 1);
		DI_Wr_reg_bits(NR4_TOP_CTRL, nr2_en, 17, 1);
	} else {
		/*set max height to disable nfram cnt in cue*/
		if (is_meson_gxlx_cpu())
			DI_Wr(NR2_FRM_SIZE, (0xfff<<16)|width);
		else
			DI_Wr(NR2_FRM_SIZE, (height<<16)|width);
		DI_Wr_reg_bits(NR2_SW_EN, nr2_en, 4, 1);
	}
}

static bool cue_en = true;
module_param_named(cue_en, cue_en, bool, 0664);
/*
 * workaround for nframe count
 * indicating error field type in cue
 */
static void cue_config(struct CUE_PARM_s *pcue_parm, unsigned short field_type)
{
	pcue_parm->field_count = 8;
	pcue_parm->frame_count = 8;
	if (field_type != VIDTYPE_PROGRESSIVE) {
		DI_Wr_reg_bits(NR2_CUE_PRG_DIF, 0, 20, 1);
		DI_Wr_reg_bits(DI_NR_CTRL0, 0, 26, 1);
		/* cur row mode avoid seek error */
		Wr_reg_bits(NR2_CUE_MODE, 5, 0, 4);
	} else {
		DI_Wr_reg_bits(NR2_CUE_PRG_DIF, 1, 20, 1);
		/* disable cue for progressive issue */
		DI_Wr_reg_bits(DI_NR_CTRL0, 0, 26, 1);
	}
}

void nr_all_config(unsigned short width, unsigned short height,
	unsigned short field_type)
{
	nr_param.width = width;
	nr_param.height = height;
	nr_param.frame_count = 0;
	nr_param.prog_flag = field_type?false:true;
	nr2_config(width, height);
	dnr_config(nr_param.pdnr_parm, width, height);

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXLX))
		cue_config(nr_param.pcue_parm, field_type);
	if (is_meson_txlx_cpu() || is_meson_g12a_cpu() ||
		is_meson_g12b_cpu() || is_meson_tl1_cpu() ||
		is_meson_sm1_cpu() || is_meson_tm2_cpu()) {
		linebuffer_config(width);
		nr4_config(nr_param.pnr4_parm, width, height);
	}
	if (is_meson_txhd_cpu())
		linebuffer_config(width);
}

static int find_lut16(unsigned int val, int *pLut)
{
	int idx_L, shft, dist;
	int left, right, norm;
	int res;

	if (val < 2) {
		idx_L = 0;
		dist  = val - 0;
		shft = 1;
	} else if (val < 4) {
		idx_L = 1;
		dist  = val - 2;
		shft = 1;
	} else if (val < 8) {
		idx_L = 2;
		dist  = val - 4;
		shft = 2;
	} else if (val < 16) {
		idx_L = 3;
		dist  = val - 8;
		shft = 3;
	} else if (val < 32) {
		idx_L = 4;
		dist  = val - 16;
		shft = 4;
	} else if (val < 48) {
		idx_L = 5;
		dist  = val - 32;
		shft = 4;
	} else if (val < 64) {
		idx_L = 6;
		dist  = val - 48;
		shft = 4;
	} else if (val < 80) {
		idx_L = 7;
		dist  = val - 64;
		shft = 4;
	} else if (val < 96) {
		idx_L = 8;
		dist  = val - 80;
		shft = 4;
	} else if (val < 112) {
		idx_L = 9;
		dist = val - 96;
		shft = 4;
	} else if (val < 128) {
		idx_L = 10;
		dist = val - 112;
		shft = 4;
	} else if (val < 160) {
		idx_L = 11;
		dist  = val - 128;
		shft = 5;
	} else if (val < 192) {
		idx_L = 12;
		dist = val - 160;
		shft = 5;
	} else if (val < 224) {
		idx_L = 13;
		dist = val - 192;
		shft = 5;
	} else {
		idx_L = 14;
		dist = val - 224;
		shft = 5;
	}
	left = pLut[idx_L];
	right = pLut[idx_L+1];
	norm = (1<<shft);
	res = ((left*(norm-dist) + right*dist + (norm>>1))>>shft);
	return res;
}
static void noise_meter_process(struct NR4_PARM_s *nr4_param_p,
		unsigned int field_cnt)
{
	unsigned int val1 = 0, val2 = 0, field_sad = 0, field_var = 0;
	int val = 0;

	val1 = Rd(NR4_RO_NM_SAD_CNT);
	val2 = Rd(NR4_RO_NM_SAD_SUM);

	field_sad = (val1 == 0 ? 0 : (val2 + (val1>>1)) / val1);

	val1 = Rd(NR4_RO_NM_VAR_SCNT);
	val2 = Rd(NR4_RO_NM_VAR_SUM);
	field_var = (val1 == 0 ? 0 : (val2 + (val1>>1)) / val1);
	/*
	 *  field sad based global noise level to gain,
	 *  maybe improved further
	 */
	if (nr4_param_p->sw_nr4_sad2gain_en == 1) {
		val2 = (field_sad<<2) < 255 ? (field_sad<<2) : 255;
		val1 = find_lut16(val2, &nr4_param_p->sw_nr4_sad2gain_lut[0]);
	} else
		val1 = 64;
	DI_Wr_reg_bits(NR4_MCNR_MV_CTRL_REG, val1, 4, 8);
	/*add for TL1------*/
	if (nr4_param_p->sw_nr4_noise_ctrl_dm_en == 1) {
		if (nr4_param_p->sw_nr4_noise_sel == 0) {
			val2 = val1 >= nr4_param_p->sw_nr4_noise_thd ? 1 : 0;
		} else {
			val2 =
			field_sad >= nr4_param_p->sw_nr4_noise_thd ? 1 : 0;
		}

		DI_Wr_reg_bits(DNR_DM_NR_BLND, val2, 24, 1);
	}
	/*------------------*/
	/* scene change processing */
	nr4_param_p->sw_nr4_scene_change_flg[0] =
		nr4_param_p->sw_nr4_scene_change_flg[1];
	nr4_param_p->sw_nr4_scene_change_flg[1] =
		nr4_param_p->sw_nr4_scene_change_flg[2];
	val = field_sad - nr4_param_p->sw_nr4_field_sad[1];
	if (field_cnt > 2
		&& (val > nr4_param_p->sw_nr4_scene_change_thd)) {
		nr4_param_p->sw_nr4_scene_change_flg[2] = 1;
		if (nr4_param_p->nr4_debug) {
			pr_info("NR4 current field_sad=%d, sad[1]=%d, val=%d",
			field_sad, nr4_param_p->sw_nr4_field_sad[1], val);
		}
	} else
		nr4_param_p->sw_nr4_scene_change_flg[2] = 0;
	if (nr4_param_p->sw_nr4_scene_change_flg[1] ||
		nr4_param_p->sw_nr4_scene_change_flg[2])
		DI_Wr_reg_bits(NR4_TOP_CTRL, 1, 0, 1);
	else
		DI_Wr_reg_bits(NR4_TOP_CTRL, 0, 0, 1);

	/*fot TL1 **************/
	if (nr4_param_p->sw_dm_scene_change_en == 1) {
		val = field_sad >= nr4_param_p->sw_nr4_scene_change_thd2
			&& nr4_param_p->sw_nr4_field_sad[1]
				>= nr4_param_p->sw_nr4_scene_change_thd2;

		DI_Wr_reg_bits(DNR_DM_CTRL, val, 12, 1);
	}
	/***********************/
	nr4_param_p->sw_nr4_field_sad[0] = nr4_param_p->sw_nr4_field_sad[1];
	nr4_param_p->sw_nr4_field_sad[1] = field_sad;
}
static void luma_enhancement_process(struct NR4_PARM_s *nr4_param_p,
		unsigned int field_cnt)
{
	unsigned int reg_val = 0, tmp1 = 0;

	tmp1 = nr4_param_p->width * nr4_param_p->height;
	if (field_cnt <= 2) {
		reg_val = (Rd(NR4_MCNR_RO_U_SUM) + (tmp1>>1))/tmp1;
		DI_Wr_reg_bits(NR4_MCNR_LUMAPRE_CAL_PRAM, reg_val, 8, 8);
		reg_val = (Rd(NR4_MCNR_RO_V_SUM) + (tmp1>>1))/tmp1;
		DI_Wr_reg_bits(NR4_MCNR_LUMAPRE_CAL_PRAM, reg_val, 0, 8);
	} else {
		reg_val = Rd_reg_bits(NR4_MCNR_LUMACUR_CAL_PRAM, 8, 8);
		DI_Wr_reg_bits(NR4_MCNR_LUMAPRE_CAL_PRAM, reg_val, 8, 8);
		reg_val = Rd_reg_bits(NR4_MCNR_LUMACUR_CAL_PRAM, 0, 8);
		DI_Wr_reg_bits(NR4_MCNR_LUMACUR_CAL_PRAM, reg_val, 0, 8);
	}
	reg_val = Rd_reg_bits(NR4_MCNR_LUMACUR_CAL_PRAM, 24, 2);
	DI_Wr_reg_bits(NR4_MCNR_LUMAPRE_CAL_PRAM, reg_val, 24, 2);
	reg_val = Rd_reg_bits(NR4_MCNR_LUMACUR_CAL_PRAM, 16, 2);
	DI_Wr_reg_bits(NR4_MCNR_LUMAPRE_CAL_PRAM, reg_val, 16, 2);
	reg_val = (Rd(NR4_MCNR_RO_U_SUM) + (tmp1>>1))/tmp1;
	DI_Wr_reg_bits(NR4_MCNR_LUMACUR_CAL_PRAM, reg_val, 8, 8);
	reg_val = (Rd(NR4_MCNR_RO_V_SUM) + (tmp1>>1))/tmp1;
	DI_Wr_reg_bits(NR4_MCNR_LUMACUR_CAL_PRAM, reg_val, 0, 8);
	reg_val = SGN2(Rd(NR4_MCNR_RO_GRDU_SUM));
	DI_Wr_reg_bits(NR4_MCNR_LUMACUR_CAL_PRAM, reg_val, 24, 2);
	reg_val = SGN2(Rd(NR4_MCNR_RO_GRDV_SUM));
	DI_Wr_reg_bits(NR4_MCNR_LUMACUR_CAL_PRAM, reg_val, 16, 2);
}
static void dnr_process(struct DNR_PARM_s *pDnrPrm)
{
	static int ro_gbs_stat_lr = 0, ro_gbs_stat_ll = 0, ro_gbs_stat_rr = 0,
	ro_gbs_stat_dif = 0, ro_gbs_stat_cnt = 0;
	/* int reg_dnr_stat_xst=0,reg_dnr_stat_xed=0,
	 * reg_dnr_stat_yst=0,reg_dnr_stat_yed=0;
	 */
#ifdef DNR_HV_SHIFT
	int ro_hbof_stat_cnt[32], ro_vbof_stat_cnt[32], i = 0;
#endif
	int ll, lr;

	if (is_meson_tl1_cpu() || is_meson_tm2_cpu()) {
		ll = Rd(DNR_RO_GBS_STAT_LR);
		lr = Rd(DNR_RO_GBS_STAT_LL);
	} else {
		ll = Rd(DNR_RO_GBS_STAT_LL);
		lr = Rd(DNR_RO_GBS_STAT_LR);

	}
	if (ro_gbs_stat_lr != lr ||
		ro_gbs_stat_ll != ll ||
		ro_gbs_stat_rr != Rd(DNR_RO_GBS_STAT_RR) ||
		ro_gbs_stat_dif != Rd(DNR_RO_GBS_STAT_DIF) ||
		ro_gbs_stat_cnt != Rd(DNR_RO_GBS_STAT_CNT)) {

		ro_gbs_stat_lr = lr;
		ro_gbs_stat_ll = ll;
		ro_gbs_stat_rr = Rd(DNR_RO_GBS_STAT_RR);
		ro_gbs_stat_dif = Rd(DNR_RO_GBS_STAT_DIF);
		ro_gbs_stat_cnt = Rd(DNR_RO_GBS_STAT_CNT);
	} else {
		return;
	}

	global_bs_calc_sw(&pDnrPrm->sw_gbs_vld_cnt,
			  &pDnrPrm->sw_gbs_vld_flg,
			  &pDnrPrm->sw_gbs,
			  ro_gbs_stat_lr,
			  ro_gbs_stat_ll,
			  ro_gbs_stat_rr,
			  ro_gbs_stat_dif,
			  ro_gbs_stat_cnt,
			  pDnrPrm->prm_gbs_vldcntthd, /* prm below */
			  pDnrPrm->prm_gbs_cnt_min,
			  pDnrPrm->prm_gbs_ratcalcmod,
			  pDnrPrm->prm_gbs_ratthd,
			  pDnrPrm->prm_gbs_difthd,
			  pDnrPrm->prm_gbs_bsdifthd,
			  pDnrPrm->prm_gbs_calcmod);
#ifdef DNR_HV_SHIFT
	for (i = 0; i < 32; i++)
		ro_hbof_stat_cnt[i] = Rd(DNR_RO_HBOF_STAT_CNT_0+i);
	for (i = 0; i < 32; i++)
		ro_vbof_stat_cnt[i] = Rd(DNR_RO_VBOF_STAT_CNT_0+i);
		hor_blk_ofst_calc_sw(&pDnrPrm->sw_hbof_vld_cnt,
			 &pDnrPrm->sw_hbof_vld_flg,
			 &pDnrPrm->sw_hbof,
			 ro_hbof_stat_cnt,
			 0,
			 nCol-1,
			 pDnrPrm->prm_hbof_minthd,
			 pDnrPrm->prm_hbof_ratthd0,
			 pDnrPrm->prm_hbof_ratthd1,
			 pDnrPrm->prm_hbof_vldcntthd,
			 nRow,
			 nCol);

	ver_blk_ofst_calc_sw(&pDnrPrm->sw_vbof_vld_cnt,
			 &pDnrPrm->sw_vbof_vld_flg,
			 &pDnrPrm->sw_vbof,
			 ro_vbof_stat_cnt,
			 0,
			 nRow-1,
			 pDnrPrm->prm_vbof_minthd,
			 pDnrPrm->prm_vbof_ratthd0,
			 pDnrPrm->prm_vbof_ratthd1,
			 pDnrPrm->prm_vbof_vldcntthd,
			 nRow,
			 nCol);
#endif
	/* update hardware registers */
	if (pDnrPrm->prm_sw_gbs_ctrl == 0) {
		DI_Wr(DNR_GBS,
			(pDnrPrm->sw_gbs_vld_flg == 1)?pDnrPrm->sw_gbs : 0);
	} else if (pDnrPrm->prm_sw_gbs_ctrl == 1) {
		DI_Wr_reg_bits(DNR_BLK_OFFST,
		(pDnrPrm->sw_hbof_vld_flg == 1)?pDnrPrm->sw_hbof:0, 4, 3);
		DI_Wr(DNR_GBS, (pDnrPrm->sw_hbof_vld_flg == 1 &&
		pDnrPrm->sw_gbs_vld_flg == 1)?pDnrPrm->sw_gbs:0);
	} else if (pDnrPrm->prm_sw_gbs_ctrl == 2) {
		DI_Wr_reg_bits(DNR_BLK_OFFST,
		(pDnrPrm->sw_vbof_vld_flg == 1)?pDnrPrm->sw_vbof:0, 0, 3);
		DI_Wr(DNR_GBS, (pDnrPrm->sw_vbof_vld_flg == 1 &&
			pDnrPrm->sw_gbs_vld_flg == 1)?pDnrPrm->sw_gbs:0);
	} else if (pDnrPrm->prm_sw_gbs_ctrl == 1) {
		DI_Wr_reg_bits(DNR_BLK_OFFST,
		pDnrPrm->sw_hbof_vld_flg == 1 ? pDnrPrm->sw_hbof : 0, 4, 3);
		DI_Wr_reg_bits(DNR_BLK_OFFST,
		pDnrPrm->sw_vbof_vld_flg == 1 ? pDnrPrm->sw_vbof : 0, 0, 3);
		DI_Wr(DNR_GBS, (pDnrPrm->sw_hbof_vld_flg == 1 &&
			pDnrPrm->sw_vbof_vld_flg == 1 &&
			pDnrPrm->sw_gbs_vld_flg == 1)?pDnrPrm->sw_gbs:0);
	}
}

static bool invert_cue_phase;
module_param_named(invert_cue_phase, invert_cue_phase, bool, 0644);

static unsigned int cue_pr_cnt;
module_param_named(cue_pr_cnt, cue_pr_cnt, uint, 0644);

static bool cue_glb_mot_check_en = true;
module_param_named(cue_glb_mot_check_en, cue_glb_mot_check_en, bool, 0644);

static void cue_process_irq(void)
{

	int pre_field_num = 0, cue_invert = 0;

	if (is_meson_gxlx_cpu()) {
		pre_field_num = Rd_reg_bits(DI_PRE_CTRL, 29, 1);
		if (invert_cue_phase)
			cue_invert = (pre_field_num?3:0);
		else
			cue_invert = (pre_field_num?0:3);
		if (cue_pr_cnt > 0) {
			pr_info("[DI]: chan2 field num %d, cue_invert %d.\n",
				pre_field_num, cue_invert);
			cue_pr_cnt--;
		}
		Wr_reg_bits(NR2_CUE_MODE, cue_invert, 10, 2);
	}
	if (!nr_param.prog_flag) {
		if (nr_param.frame_count > 1 && cue_glb_mot_check_en)
			DI_Wr_reg_bits(DI_NR_CTRL0, cue_en?1:0, 26, 1);
	}
	if (nr_param.frame_count == 5)
		Wr_reg_bits(NR2_CUE_MODE, 7, 0, 4);
}
void cue_int(void)
{
	/*confirm with vlsi-liuyanling, G12a cue must be disabled*/
	if (is_meson_g12a_cpu()) {
		cue_en = false;
		cue_glb_mot_check_en = false;
	}
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12B)) {
		if (cue_en)
			Wr_reg_bits(NR2_CUE_MODE, 3, 10, 2);
	}
}
static bool glb_fieldck_en = true;
module_param_named(glb_fieldck_en, glb_fieldck_en, bool, 0644);

void adaptive_cue_adjust(unsigned int frame_diff, unsigned int field_diff)
{
	struct CUE_PARM_s *pcue_parm = nr_param.pcue_parm;
	unsigned int mask1, mask2;

	if (is_meson_tl1_cpu() || is_meson_tm2_cpu()) {
		/*value from VLSI(yanling.liu) 2018-12-07: */
		mask1 = 0x50332;
		mask2 = 0x00054357;
	} else { /*ori value*/
		mask1 = 0x50323;
		mask2 = 0x00054375;
	}

	if (frame_diff > pcue_parm->glb_mot_framethr) {
		pcue_parm->frame_count = pcue_parm->frame_count > 0 ?
			(pcue_parm->frame_count - 1) : 0;
	} else if (pcue_parm->frame_count < pcue_parm->glb_mot_fieldnum) {
		pcue_parm->frame_count = pcue_parm->frame_count + 1;
	}
	if (glb_fieldck_en) {
		if (field_diff < pcue_parm->glb_mot_fieldthr)
			pcue_parm->field_count = pcue_parm->field_count + 1;
		else if (pcue_parm->field_count < pcue_parm->glb_mot_fieldnum) {
			pcue_parm->field_count = pcue_parm->field_count > 0 ?
				(pcue_parm->field_count - 1) : 0;
		}
	}

	if (cue_glb_mot_check_en) {
		if (pcue_parm->frame_count > (pcue_parm->glb_mot_fieldnum - 6))
			cue_en = true;
		else
			cue_en = false;
		/* for clockfuliness clip */
		if (pcue_parm->field_count >
				(pcue_parm->glb_mot_fieldnum - 6)) {
			Wr(NR2_CUE_MODE, mask1|(Rd(NR2_CUE_MODE)&0xc00));
			Wr(NR2_CUE_CON_MOT_TH, 0x03010e01);
		} else {
			Wr(NR2_CUE_MODE, mask2|(Rd(NR2_CUE_MODE)&0xc00));
			Wr(NR2_CUE_CON_MOT_TH, 0xa03c8c3c);
		}
	}
}

/*
 * insert nr ctrl regs into ctrl table
 */
bool set_nr_ctrl_reg_table(unsigned int addr, unsigned int value)
{
	unsigned int i = 0;
	struct NR_CTRL_REGS_s *pnr_regs = NULL;

	pnr_regs = nr_param.pnr_regs;
	for (i = 0; i < NR_CTRL_REG_NUM; i++) {
		if (pnr_regs->regs[i].addr == addr) {
			pnr_regs->regs[i].addr = addr;
			pnr_regs->regs[i].value = value;
			atomic_set(&pnr_regs->regs[i].load_flag, 1);
			if (nr_ctrl_reg)
				pr_info("NR_CTRL_REG[0x%x]=[0x%x].\n",
					addr, value);
			return true;
		}
	}
	return false;
}

/* load nr related ctrl regs */
static void nr_ctrl_reg_load(struct NR_CTRL_REGS_s *pnr_regs)
{
	unsigned int i = 0;

	for (i = 0; i < pnr_regs->reg_num; i++) {
		if (atomic_read(&pnr_regs->regs[i].load_flag)) {
			DI_Wr(pnr_regs->regs[i].addr,
				pnr_regs->regs[i].value);
			atomic_set(&pnr_regs->regs[i].load_flag, 0);
			if (nr_ctrl_reg) {
				pr_info("LOAD NR[0x%x]=[0x%x]\n",
					pnr_regs->regs[i].addr,
					pnr_regs->regs[i].value);
			}
		}
	}
}

void nr_process_in_irq(void)
{
	nr_param.frame_count++;
	nr_ctrl_reg_load(nr_param.pnr_regs);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXLX))
		cue_process_irq();
	if (dnr_en)
		dnr_process(&dnr_param);
	if (is_meson_txlx_cpu() || is_meson_g12a_cpu()
		|| is_meson_g12a_cpu() || is_meson_tl1_cpu() ||
		is_meson_sm1_cpu() || is_meson_tm2_cpu()) {
		noise_meter_process(nr_param.pnr4_parm, nr_param.frame_count);
		luma_enhancement_process(nr_param.pnr4_parm,
				nr_param.frame_count);
	}
}
static void parse_cmd_params(char *buf_orig, char **parm)
{
	char *ps, *token;
	char delim1[3] = " ";
	char delim2[2] = "\n";
	unsigned int n = 0;

	ps = buf_orig;
	strcat(delim1, delim2);
	while (1) {
		token = strsep(&ps, delim1);
		if (token == NULL)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
}

static dnr_param_t dnr_params[] = {
	{"prm_sw_gbs_ctrl", &(dnr_param.prm_sw_gbs_ctrl)},
	{"prm_gbs_vldcntthd", &(dnr_param.prm_gbs_vldcntthd)},
	{"prm_gbs_cnt_min", &(dnr_param.prm_gbs_cnt_min)},
	{"prm_gbs_ratcalcmod", &(dnr_param.prm_gbs_ratcalcmod)},
	{"prm_gbs_ratthd[0]", &(dnr_param.prm_gbs_ratthd[0])},
	{"prm_gbs_ratthd[1]", &(dnr_param.prm_gbs_ratthd[1])},
	{"prm_gbs_ratthd[2]", &(dnr_param.prm_gbs_ratthd[2])},
	{"prm_gbs_difthd[0]", &(dnr_param.prm_gbs_difthd[0])},
	{"prm_gbs_difthd[1]", &(dnr_param.prm_gbs_difthd[1])},
	{"prm_gbs_difthd[2]", &(dnr_param.prm_gbs_difthd[2])},
	{"prm_gbs_bsdifthd", &(dnr_param.prm_gbs_bsdifthd)},
	{"prm_gbs_calcmod", &(dnr_param.prm_gbs_calcmod)},
	{"sw_gbs", &(dnr_param.sw_gbs)},
	{"sw_gbs_vld_flg", &(dnr_param.sw_gbs_vld_flg)},
	{"sw_gbs_vld_cnt", &(dnr_param.sw_gbs_vld_cnt)},
	{"prm_hbof_minthd", &(dnr_param.prm_hbof_minthd)},
	{"prm_hbof_ratthd0", &(dnr_param.prm_hbof_ratthd0)},
	{"prm_hbof_ratthd1", &(dnr_param.prm_hbof_ratthd1)},
	{"prm_hbof_vldcntthd", &(dnr_param.prm_hbof_vldcntthd)},
	{"sw_hbof", &(dnr_param.sw_hbof)},
	{"sw_hbof_vld_flg", &(dnr_param.sw_hbof_vld_flg)},
	{"sw_hbof_vld_cnt", &(dnr_param.sw_hbof_vld_cnt)},
	{"prm_vbof_minthd", &(dnr_param.prm_vbof_minthd)},
	{"prm_vbof_ratthd0", &(dnr_param.prm_vbof_ratthd0)},
	{"prm_vbof_ratthd1", &(dnr_param.prm_vbof_ratthd1)},
	{"prm_vbof_vldcntthd", &(dnr_param.prm_vbof_vldcntthd)},
	{"sw_vbof", &(dnr_param.sw_vbof)},
	{"sw_vbof_vld_flg", &(dnr_param.sw_vbof_vld_flg)},
	{"sw_vbof_vld_cnt", &(dnr_param.sw_vbof_vld_cnt)},
	{"dnr_stat_coef", &(dnr_param.dnr_stat_coef)},
	{"", NULL}
};
static ssize_t dnr_param_store(struct device *dev,
				struct device_attribute *attr,
				const char *buff, size_t count)
{
	int i = 0, value = 0;
	char *parm[2] = {NULL}, *buf_orig;

	buf_orig = kstrdup(buff, GFP_KERNEL);
	parse_cmd_params(buf_orig, (char **)(&parm));
	for (i = 0; dnr_params[i].addr; i++) {
		if (!strcmp(parm[0], dnr_params[i].name)) {
			value = kstrtol(parm[1], 10, NULL);
			*(dnr_params[i].addr) = value;
			pr_dbg("%s=%d.\n", dnr_params[i].name,
			*(dnr_params[i].addr));
		}
	}

	kfree(buf_orig);
	return count;
}

static ssize_t dnr_param_show(struct device *dev,
				struct device_attribute *attr,
				char *buff)
{
	ssize_t len = 0;
	int i = 0;

	for (i = 0; dnr_params[i].addr; i++)
		len += sprintf(buff+len, "%s=%d.\n",
		dnr_params[i].name, *(dnr_params[i].addr));
	return len;
}

static nr4_param_t nr4_params[NR4_PARAMS_NUM];
static void nr4_params_init(struct NR4_PARM_s *nr4_parm_p)
{
	nr4_params[0].name = "prm_nr4_srch_stp";
	nr4_params[0].addr = &(nr4_parm_p->prm_nr4_srch_stp);
	nr4_params[1].name = "sw_nr4_field_sad[0]";
	nr4_params[1].addr = &(nr4_parm_p->sw_nr4_field_sad[0]);
	nr4_params[2].name = "sw_nr4_field_sad[1]";
	nr4_params[2].addr = &(nr4_parm_p->sw_nr4_field_sad[1]);
	nr4_params[3].name = "sw_nr4_scene_change_thd";
	nr4_params[3].addr = &(nr4_parm_p->sw_nr4_scene_change_thd);
	nr4_params[4].name = "sw_nr4_scene_change_flg[0]";
	nr4_params[4].addr = &(nr4_parm_p->sw_nr4_scene_change_flg[0]);
	nr4_params[5].name = "sw_nr4_scene_change_flg[1]";
	nr4_params[5].addr = &(nr4_parm_p->sw_nr4_scene_change_flg[1]);
	nr4_params[6].name = "sw_nr4_scene_change_flg[2]";
	nr4_params[6].addr = &(nr4_parm_p->sw_nr4_scene_change_flg[2]);
	nr4_params[7].name = "sw_nr4_sad2gain_en";
	nr4_params[7].addr = &(nr4_parm_p->sw_nr4_sad2gain_en);
	nr4_params[8].name = "sw_nr4_sad2gain_lut[0]";
	nr4_params[8].addr = &(nr4_parm_p->sw_nr4_sad2gain_lut[0]);
	nr4_params[9].name = "sw_nr4_sad2gain_lut[1]";
	nr4_params[9].addr = &(nr4_parm_p->sw_nr4_sad2gain_lut[1]);
	nr4_params[10].name = "sw_nr4_sad2gain_lut[2]";
	nr4_params[10].addr = &(nr4_parm_p->sw_nr4_sad2gain_lut[2]);
	nr4_params[11].name = "sw_nr4_sad2gain_lut[3]";
	nr4_params[11].addr = &(nr4_parm_p->sw_nr4_sad2gain_lut[3]);
	nr4_params[12].name = "sw_nr4_sad2gain_lut[4]";
	nr4_params[12].addr = &(nr4_parm_p->sw_nr4_sad2gain_lut[4]);
	nr4_params[13].name = "sw_nr4_sad2gain_lut[5]";
	nr4_params[13].addr = &(nr4_parm_p->sw_nr4_sad2gain_lut[5]);
	nr4_params[14].name = "sw_nr4_sad2gain_lut[6]";
	nr4_params[14].addr = &(nr4_parm_p->sw_nr4_sad2gain_lut[6]);
	nr4_params[15].name = "sw_nr4_sad2gain_lut[7]";
	nr4_params[15].addr = &(nr4_parm_p->sw_nr4_sad2gain_lut[7]);
	nr4_params[16].name = "sw_nr4_sad2gain_lut[8]";
	nr4_params[16].addr = &(nr4_parm_p->sw_nr4_sad2gain_lut[8]);
	nr4_params[17].name = "sw_nr4_sad2gain_lut[9]";
	nr4_params[17].addr = &(nr4_parm_p->sw_nr4_sad2gain_lut[9]);
	nr4_params[18].name = "sw_nr4_sad2gain_lut[10]";
	nr4_params[18].addr = &(nr4_parm_p->sw_nr4_sad2gain_lut[10]);
	nr4_params[19].name = "sw_nr4_sad2gain_lut11]";
	nr4_params[19].addr = &(nr4_parm_p->sw_nr4_sad2gain_lut[11]);
	nr4_params[20].name = "sw_nr4_sad2gain_lut[12]";
	nr4_params[20].addr = &(nr4_parm_p->sw_nr4_sad2gain_lut[12]);
	nr4_params[21].name = "sw_nr4_sad2gain_lut13]";
	nr4_params[21].addr = &(nr4_parm_p->sw_nr4_sad2gain_lut[13]);
	nr4_params[22].name = "sw_nr4_sad2gain_lut[14]";
	nr4_params[22].addr = &(nr4_parm_p->sw_nr4_sad2gain_lut[14]);
	nr4_params[23].name = "sw_nr4_sad2gain_lut[15]";
	nr4_params[23].addr = &(nr4_parm_p->sw_nr4_sad2gain_lut[15]);
	nr4_params[24].name = "nr4_debug";
	nr4_params[24].addr = &(nr4_parm_p->nr4_debug);

	nr4_params[25].name = "sw_nr4_noise_thd";
	nr4_params[25].addr = &(nr4_parm_p->sw_nr4_noise_thd);
	nr4_params[26].name = "sw_nr4_noise_sel";
	nr4_params[26].addr = &(nr4_parm_p->sw_nr4_noise_sel);
	nr4_params[27].name = "sw_nr4_noise_ctrl_dm_en";
	nr4_params[27].addr = &(nr4_parm_p->sw_nr4_noise_ctrl_dm_en);
	nr4_params[28].name = "sw_nr4_scene_change_thd2";
	nr4_params[28].addr = &(nr4_parm_p->sw_nr4_scene_change_thd2);
	nr4_params[29].name = "sw_dm_scene_change_en";
	nr4_params[29].addr = &(nr4_parm_p->sw_dm_scene_change_en);

};

static ssize_t nr4_param_store(struct device *dev,
				struct device_attribute *attr,
				const char *buff, size_t count)
{
	long i = 0, value = 0;
	char *parm[2] = {NULL}, *buf_orig;

	buf_orig = kstrdup(buff, GFP_KERNEL);
	parse_cmd_params(buf_orig, (char **)(&parm));
	for (i = 0; i < NR4_PARAMS_NUM; i++) {
		if (IS_ERR_OR_NULL(nr4_params[i].name) ||
		    IS_ERR_OR_NULL(nr4_params[i].addr))
			continue;
		if (!strcmp(parm[0], nr4_params[i].name)) {
			if (parm[1]) {
				if (kstrtol(parm[1], 10, &value) < 0)
					pr_err("DI: input value error.\n");
				*(nr4_params[i].addr) = value;
			}
			pr_info(" %d\n",    *(nr4_params[i].addr));
		}
	}

	kfree(buf_orig);
	return count;
}

static ssize_t nr4_param_show(struct device *dev,
				struct device_attribute *attr,
				char *buff)
{
	ssize_t len = 0;
	int i = 0;

	for (i = 0; i < NR4_PARAMS_NUM; i++) {
		if (IS_ERR_OR_NULL(nr4_params[i].name) ||
		    IS_ERR_OR_NULL(nr4_params[i].addr))
			continue;
		len += sprintf(buff+len, "%s=%d.\n",
		nr4_params[i].name, *(nr4_params[i].addr));
	}

	return len;
}

static DEVICE_ATTR(nr4_param, 0664, nr4_param_show, nr4_param_store);

static void nr4_param_init(struct NR4_PARM_s *nr4_parm_p)
{
	int k = 0;

	nr4_parm_p->border_offset = 8;
	nr4_parm_p->nr4_debug = 0;
	nr4_parm_p->prm_nr4_srch_stp = 1;
	nr4_parm_p->sw_nr4_field_sad[0] = 0;
	nr4_parm_p->sw_nr4_field_sad[1] = 0;
	nr4_parm_p->sw_nr4_scene_change_thd = 20;
	for (k = 0; k < 3; k++)
		nr4_parm_p->sw_nr4_scene_change_flg[k] = 0;
	nr4_parm_p->sw_nr4_sad2gain_en   =   1;
	nr4_parm_p->sw_nr4_sad2gain_lut[0] =   1;
	nr4_parm_p->sw_nr4_sad2gain_lut[1] =   2;
	nr4_parm_p->sw_nr4_sad2gain_lut[2] =   2;
	nr4_parm_p->sw_nr4_sad2gain_lut[3] =   4;
	nr4_parm_p->sw_nr4_sad2gain_lut[4] =   8;
	nr4_parm_p->sw_nr4_sad2gain_lut[5] =  16;
	nr4_parm_p->sw_nr4_sad2gain_lut[6] =  32;
	nr4_parm_p->sw_nr4_sad2gain_lut[7] =  63;
	nr4_parm_p->sw_nr4_sad2gain_lut[8] =  67;
	nr4_parm_p->sw_nr4_sad2gain_lut[9] = 104;
	nr4_parm_p->sw_nr4_sad2gain_lut[10] = 48;
	nr4_parm_p->sw_nr4_sad2gain_lut[11] = 32;
	nr4_parm_p->sw_nr4_sad2gain_lut[12] = 20;
	nr4_parm_p->sw_nr4_sad2gain_lut[13] = 16;
	nr4_parm_p->sw_nr4_sad2gain_lut[14] = 14;
	nr4_parm_p->sw_nr4_sad2gain_lut[15] = 9;

	nr4_parm_p->sw_nr4_noise_thd = 32;
	nr4_parm_p->sw_nr4_noise_sel = 0;
	nr4_parm_p->sw_nr4_noise_ctrl_dm_en = 0;
	nr4_parm_p->sw_nr4_scene_change_thd2 = 80;
	nr4_parm_p->sw_dm_scene_change_en = 0;
}

static void cue_param_init(struct CUE_PARM_s *cue_parm_p)
{
	cue_parm_p->glb_mot_framethr = 1000;
	cue_parm_p->glb_mot_fieldnum = 20;
	cue_parm_p->glb_mot_fieldthr = 10;
	cue_parm_p->field_count = 8;
	cue_parm_p->frame_count = 8;
}
static int dnr_prm_init(DNR_PRM_t *pPrm)
{
	pPrm->prm_sw_gbs_ctrl = 0;
	/*
	 * 0: update gbs, 1: update hoffst & gbs,
	 * 2: update voffst & gbs, 3: update all (hoffst & voffst & gbs).
	 */

	pPrm->prm_gbs_vldcntthd  =	4;
	pPrm->prm_gbs_cnt_min	 = 32;
	pPrm->prm_gbs_ratcalcmod =	1;/* 0: use LR, 1: use Dif */
	pPrm->prm_gbs_ratthd[0]  = 40;
	pPrm->prm_gbs_ratthd[1]  = 80;
	pPrm->prm_gbs_ratthd[2]  = 120;
	pPrm->prm_gbs_difthd[0]  = 25;
	pPrm->prm_gbs_difthd[1]  = 75;
	pPrm->prm_gbs_difthd[2]  = 125;
	pPrm->prm_gbs_bsdifthd	 = 1;
	pPrm->prm_gbs_calcmod	 = 1; /* 0:dif0, 1:dif1, 2: dif2 */

	pPrm->sw_gbs = 0;
	pPrm->sw_gbs_vld_flg = 0;
	pPrm->sw_gbs_vld_cnt = 0;

	pPrm->prm_hbof_minthd	 = 32;
	pPrm->prm_hbof_ratthd0	 = 150;
	pPrm->prm_hbof_ratthd1	 = 150;
	pPrm->prm_hbof_vldcntthd = 4;
	pPrm->sw_hbof			 = 0;
	pPrm->sw_hbof_vld_flg	 = 0;
	pPrm->sw_hbof_vld_cnt	 = 0;

	pPrm->prm_vbof_minthd	 = 32;
	pPrm->prm_vbof_ratthd0	 = 150;
	pPrm->prm_vbof_ratthd1	 = 120;
	pPrm->prm_vbof_vldcntthd = 4;
	pPrm->sw_vbof		  = 0;
	pPrm->sw_vbof_vld_flg = 0;
	pPrm->sw_vbof_vld_cnt = 0;
	pPrm->dnr_stat_coef = 3;
	return 0;
}

static DEVICE_ATTR(dnr_param, 0664, dnr_param_show, dnr_param_store);

static void nr_all_ctrl(bool enable)
{
	unsigned char value = 0;

	value = enable ? 1 : 0;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX)) {
		/* nr4 nr2*/
		DI_Wr_reg_bits(NR4_TOP_CTRL, value, 18, 1);
		DI_Wr_reg_bits(NR4_TOP_CTRL, value, 2, 1);
	} else {
		DI_Wr_reg_bits(NR2_SW_EN, value, 4, 1);
	}
	DI_Wr_reg_bits(DNR_CTRL, value, 16, 1);

}
static ssize_t nr_dbg_store(struct device *dev,
				struct device_attribute *attr,
				const char *buff, size_t count)
{
	char *parm[2] = {NULL}, *buf_orig;

	buf_orig = kstrdup(buff, GFP_KERNEL);
	parse_cmd_params(buf_orig, (char **)(&parm));
	if (!strcmp(parm[0], "disable"))
		nr_all_ctrl(false);
	else if (!strcmp(parm[0], "enable"))
		nr_all_ctrl(true);

	kfree(buf_orig);
	return count;
}

static ssize_t nr_dbg_show(struct device *dev,
				struct device_attribute *attr,
				char *buff)
{
	ssize_t len = 0;

	len += sprintf(buff+len,
		"echo disable/enable to disable/enable nr(nr2/nr4/dnr).\n");
	len += sprintf(buff+len,
		"NR4_TOP_CTRL=0x%x DNR_CTRL=0x%x DI_NR_CTRL0=0x%x\n",
		Rd(NR4_TOP_CTRL), Rd(DNR_CTRL), Rd(DI_NR_CTRL0));

	return len;
}


static DEVICE_ATTR(nr_debug, 0664, nr_dbg_show, nr_dbg_store);

void nr_hw_init(void)
{

	nr_gate_control(true);
	if (is_meson_tl1_cpu() || is_meson_tm2_cpu())
		DI_Wr(DNR_CTRL, 0x1df00|(0x03<<18));//5 line
	else
		DI_Wr(DNR_CTRL, 0x1df00);
	DI_Wr(NR3_MODE, 0x3);
	DI_Wr(NR3_COOP_PARA, 0x28ff00);
	DI_Wr(NR3_CNOOP_GAIN, 0x881900);
	DI_Wr(NR3_YMOT_PARA, 0x0c0a1e);
	DI_Wr(NR3_CMOT_PARA, 0x08140f);
	DI_Wr(NR3_SUREMOT_YGAIN, 0x100c4014);
	DI_Wr(NR3_SUREMOT_CGAIN, 0x22264014);
	nr_gate_control(false);
}
void nr_gate_control(bool gate)
{
	if (!is_meson_txlx_cpu() && !is_meson_g12a_cpu()
		&& !is_meson_g12b_cpu() && !is_meson_sm1_cpu()
		&& !is_meson_tl1_cpu() && !is_meson_tm2_cpu())
		return;
	if (gate) {
		/* enable nr auto gate */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL2, 0, 0, 2);
		/* enable dnr auto gate */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL2, 0, 8, 2);
		/* enable nr/dm blend auto gate */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL2, 0, 10, 2);
		/* enable nr internal snr gate */
		DI_Wr_reg_bits(NR4_TOP_CTRL, 0, 30, 2);
		/* enable nr internal tnr gate */
		DI_Wr_reg_bits(NR4_TOP_CTRL, 0, 28, 2);
		/* enable nr internal mcnr gate */
		DI_Wr_reg_bits(NR4_TOP_CTRL, 0, 26, 2);
		/* enable nr internal cfr gate */
		DI_Wr_reg_bits(NR4_TOP_CTRL, 0, 24, 2);
		/* enable nr internal det_polar gate */
		DI_Wr_reg_bits(NR4_TOP_CTRL, 0, 22, 2);
		/* enable nr internal 3ddet gate */
		DI_Wr_reg_bits(NR4_TOP_CTRL, 0, 20, 2);
	} else {
		/* enable nr auto gate */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL2, 1, 0, 2);
		/* enable dnr auto gate */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL2, 1, 8, 2);
		/* disable nr/dm blend auto gate */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL2, 1, 10, 2);
		/* disable nr internal snr gate */
		DI_Wr_reg_bits(NR4_TOP_CTRL, 1, 30, 2);
		/* disable nr internal tnr gate */
		DI_Wr_reg_bits(NR4_TOP_CTRL, 1, 28, 2);
		/* disable nr internal mcnr gate */
		DI_Wr_reg_bits(NR4_TOP_CTRL, 1, 26, 2);
		/* disable nr internal cfr gate */
		DI_Wr_reg_bits(NR4_TOP_CTRL, 1, 24, 2);
		/* disable nr internal det_polar gate */
		DI_Wr_reg_bits(NR4_TOP_CTRL, 1, 22, 2);
		/* disable nr internal 3ddet gate */
		DI_Wr_reg_bits(NR4_TOP_CTRL, 1, 20, 2);
	}
}
/*
 * set ctrl reg address need load in irq
 */
static void nr_ctrl_regs_init(struct NR_CTRL_REGS_s *pnr_regs)
{
	unsigned int i = 0;

	pnr_regs->regs[0].addr = NR4_TOP_CTRL;
	pnr_regs->regs[1].addr = NR_DB_FLT_CTRL;
	pnr_regs->regs[2].addr = DNR_DM_CTRL;
	pnr_regs->regs[3].addr = DI_NR_CTRL0;
	pnr_regs->regs[4].addr = DNR_CTRL;
	pnr_regs->regs[5].addr = NR2_CUE_PRG_DIF;
	pnr_regs->reg_num = NR_CTRL_REG_NUM;
	for (i = 0; i < pnr_regs->reg_num; i++) {
		pnr_regs->regs[i].value = 0;
		atomic_set(&pnr_regs->regs[i].load_flag, 0);
	}
}

void nr_drv_uninit(struct device *dev)
{
	if (nr_param.pnr4_parm) {
		vfree(nr_param.pnr4_parm);
		nr_param.pnr4_parm = NULL;
	}
	if (nr_param.pnr_regs) {
		vfree(nr_param.pnr_regs);
		nr_param.pnr_regs = NULL;
	}
	if (nr_param.pcue_parm) {
		vfree(nr_param.pcue_parm);
		nr_param.pcue_parm = NULL;
	}

	device_remove_file(dev, &dev_attr_nr4_param);
	device_remove_file(dev, &dev_attr_dnr_param);
	device_remove_file(dev, &dev_attr_nr_debug);
}
void nr_drv_init(struct device *dev)
{
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX)) {
		nr_param.pnr4_parm = vmalloc(sizeof(struct NR4_PARM_s));
		if (IS_ERR(nr_param.pnr4_parm))
			pr_err("%s allocate nr4 parm error.\n", __func__);
		else {
			nr4_params_init(nr_param.pnr4_parm);
			nr4_param_init(nr_param.pnr4_parm);
			device_create_file(dev, &dev_attr_nr4_param);
		}
	}
	nr_param.pnr_regs = vmalloc(sizeof(struct NR_CTRL_REGS_s));
	if (IS_ERR(nr_param.pnr_regs))
		pr_err("%s allocate ctrl regs error.\n", __func__);
	else
		nr_ctrl_regs_init(nr_param.pnr_regs);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXLX)) {
		nr_param.pcue_parm = vmalloc(sizeof(struct CUE_PARM_s));
		if (IS_ERR(nr_param.pcue_parm))
			pr_err("%s allocate cue parm error.\n", __func__);
		else
			cue_param_init(nr_param.pcue_parm);
	}
	dnr_prm_init(&dnr_param);
	nr_param.pdnr_parm = &dnr_param;
	device_create_file(dev, &dev_attr_dnr_param);
	device_create_file(dev, &dev_attr_nr_debug);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXBB))
		dnr_dm_en = true;
	else
		dnr_dm_en = false;
}
