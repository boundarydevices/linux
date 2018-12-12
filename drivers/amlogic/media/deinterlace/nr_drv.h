/*
 * drivers/amlogic/media/deinterlace/nr_drv.h
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
#include <linux/atomic.h>

struct nr_param_s {
	char *name;
	int *addr;
};

#define NR4_PARAMS_NUM (30)	//25
#define dnr_param_t struct nr_param_s
#define nr4_param_t struct nr_param_s

struct DNR_PARM_s {
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
	int dnr_stat_coef;
};/* used for software */
#define DNR_PRM_t struct DNR_PARM_s
struct NR4_PARM_s {
	int prm_nr4_srch_stp;
	int sw_nr4_field_sad[2];
	int sw_nr4_scene_change_thd;
	int sw_nr4_scene_change_flg[3];
	int sw_nr4_sad2gain_en;
	int sw_nr4_sad2gain_lut[16];
	int nr4_debug;
	unsigned short width;
	unsigned short height;
	unsigned short border_offset;

	unsigned int sw_nr4_noise_thd;//u8
	/*u1, 0: use nr4 global gain, 1: use field sad;*/
	unsigned int sw_nr4_noise_sel;
	unsigned int sw_nr4_noise_ctrl_dm_en;//u1

	/*u8, threshold for scene change*/
	unsigned int sw_nr4_scene_change_thd2;
	/*u1, enable dm scene change check*/
	unsigned int sw_dm_scene_change_en;
};

struct CUE_PARM_s {
	int glb_mot_framethr;
	int glb_mot_fieldnum;
	int glb_mot_fieldthr;
	int field_count;
	int frame_count;
};

#define	NR_CTRL_REG_NUM	6
/* nr ctrl reg addr,value, flag*/
struct NR_CTRL_REG_s {
	unsigned int addr;
	unsigned int value;
	atomic_t load_flag;
};

struct NR_CTRL_REGS_s {
	struct NR_CTRL_REG_s regs[NR_CTRL_REG_NUM];
	unsigned int reg_num;
};

struct NR_PARM_s {
	unsigned short width;
	unsigned short height;
	unsigned short frame_count;
	bool           prog_flag;
	struct DNR_PARM_s *pdnr_parm;
	struct NR4_PARM_s *pnr4_parm;
	struct CUE_PARM_s *pcue_parm;
	struct NR_CTRL_REGS_s *pnr_regs;
};
#ifndef SGN2
#define SGN2(x) ((x) > 0 ? 1 : ((x) < 0 ? -1 : 0))
#endif


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
/* software parameters initialization before used */
void adaptive_cue_adjust(unsigned int frame_diff, unsigned int field_diff);
void nr_hw_init(void);
void nr_gate_control(bool gate);
void nr_drv_init(struct device *dev);
void nr_drv_uninit(struct device *dev);
void nr_process_in_irq(void);
void nr_all_config(unsigned short nCol, unsigned short nRow,
	unsigned short type);
bool set_nr_ctrl_reg_table(unsigned int addr, unsigned int value);

extern void cue_int(void);

#endif

