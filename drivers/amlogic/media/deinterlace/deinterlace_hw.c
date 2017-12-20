/*
 * drivers/amlogic/media/deinterlace/deinterlace_hw.c
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

#include <linux/string.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/module.h>

#include <linux/amlogic/media/vpu/vpu.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include "deinterlace_hw.h"
#include "register.h"
#ifdef DET3D
#include "detect3d.h"
#endif

static unsigned short mcen_mode = 1;
MODULE_PARM_DESC(mcen_mode, "\n mcen mode\n");
module_param(mcen_mode, ushort, 0664);
static unsigned short mcuv_en = 1;
MODULE_PARM_DESC(mcuv_en, "\n blend mcuv enable\n");
module_param(mcuv_en, ushort, 0664);
static unsigned short mcdebug_mode;
MODULE_PARM_DESC(mcdebug_mode, "\n mcdi mcdebugmode\n");
module_param(mcdebug_mode, ushort, 0664);

static unsigned short debug_blend_mode_ctrl = 0xff;
MODULE_PARM_DESC(debug_blend_mode_ctrl, "\n debug blend mode ctrl\n");
module_param(debug_blend_mode_ctrl, ushort, 0664);
/*
 * 0: use vframe->bitdepth,
 * 8: froce to 8 bit mode.
 * 10: froce to 10 bit mode and enable nr 10 bit.
 */

static unsigned int pq_load_dbg;
module_param_named(pq_load_dbg, pq_load_dbg, uint, 0644);

#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
extern u32 VSYNC_RD_MPEG_REG(u32 adr);
#endif
static void set_di_inp_fmt_more(
		unsigned int repeat_l0_en,
		int hz_yc_ratio,	/* 2bit */
		int hz_ini_phase,	/* 4bit */
		int vfmt_en,
		int vt_yc_ratio,	/* 2bit */
		int vt_ini_phase,	/* 4bit */
		int y_length,
		int c_length,
		int hz_rpt		/* 1bit */
	);

static void set_di_inp_mif(struct DI_MIF_s  *mif, int urgent, int hold_line);

static void set_di_mem_fmt_more(
		int hfmt_en,
		int hz_yc_ratio,	/* 2bit */
		int hz_ini_phase,	/* 4bit */
		int vfmt_en,
		int vt_yc_ratio,	/* 2bit */
		int vt_ini_phase,	/* 4bit */
		int y_length,
		int c_length,
		int hz_rpt	/* 1bit */
	);

static void set_di_mem_mif(struct DI_MIF_s *mif, int urgent, int hold_line);

static void set_di_if0_fmt_more(
		int hfmt_en,
		int hz_yc_ratio,		/* 2bit */
		int hz_ini_phase,		/* 4bit */
		int vfmt_en,
		int vt_yc_ratio,		/* 2bit */
		int vt_ini_phase,		/* 4bit */
		int y_length,
		int c_length,
		int hz_rpt				  /* 1bit */
	);

static void set_di_if1_fmt_more(
		int hfmt_en,
		int hz_yc_ratio,	/* 2bit */
		int hz_ini_phase,	/* 4bit */
		int vfmt_en,
		int vt_yc_ratio,	/* 2bit */
		int vt_ini_phase,	/* 4bit */
		int y_length,
		int c_length,
		int hz_rpt		/* 1bit */
	);

static void set_di_if1_mif(struct DI_MIF_s *mif, int urgent,
	int hold_line, int vskip_cnt);

static void set_di_chan2_mif(struct DI_MIF_s *mif, int urgent, int hold_line);

static void set_di_if0_mif(struct DI_MIF_s *mif, int urgent,
	int hold_line, int vskip_cnt, int wr_en);

static void ma_di_init(void)
{
	/* 420->422 chrome difference is large motion is large,flick */
	DI_Wr(DI_MTN_1_CTRL4, 0x01800880);
	DI_Wr(DI_MTN_1_CTRL7, 0x0a800480);
	/* mtn setting */
	DI_Wr(DI_MTN_1_CTRL1, 0xa0202015);
	/* invert chan2 field num */
	DI_Wr(DI_MTN_CTRL1, (1 << 17) | 2);
		/* no invert chan2 field num from gxlx*/
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXLX))
		DI_Wr(DI_MTN_CTRL1, 2);

}

static void ei_hw_init(void)
{
	/* ei setting */
	DI_Wr(DI_EI_CTRL0, 0x00ff0100);
	DI_Wr(DI_EI_CTRL1, 0x5a0a0f2d);
	DI_Wr(DI_EI_CTRL2, 0x050a0a5d);
	DI_Wr(DI_EI_CTRL3, 0x80000013);
	if (is_meson_txlx_cpu()) {
		DI_Wr_reg_bits(DI_EI_DRT_CTRL, 1, 30, 1);
		DI_Wr_reg_bits(DI_EI_DRT_CTRL, 1, 31, 1);
	} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXLX)) {
		DI_Wr_reg_bits(DI_EI_DRT_CTRL_GXLX, 1, 30, 1);
		DI_Wr_reg_bits(DI_EI_DRT_CTRL_GXLX, 1, 31, 1);
	}

}

static void mc_di_param_init(void)
{
	DI_Wr(MCDI_CHK_EDGE_GAIN_OFFST, 0x4f6124);
	DI_Wr(MCDI_LMV_RT, 0x7455);
	DI_Wr(MCDI_LMV_GAINTHD, 0x6014d409);
	DI_Wr(MCDI_REL_DET_LPF_MSK_22_30, 0x0a010001);
	DI_Wr(MCDI_REL_DET_LPF_MSK_31_34, 0x01010101);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL))
		DI_Wr_reg_bits(MCDI_REF_MV_NUM, 2, 0, 2);
}

void init_field_mode(unsigned short height)
{
	DI_Wr(DIPD_COMB_CTRL0, 0x02400210);
	DI_Wr(DIPD_COMB_CTRL1, 0x88080808);
	DI_Wr(DIPD_COMB_CTRL2, 0x41041008);
	DI_Wr(DIPD_COMB_CTRL3, 0x00008053);
	DI_Wr(DIPD_COMB_CTRL4, 0x20070002);
	if (height > 288)
		DI_Wr(DIPD_COMB_CTRL5, 0x04041020);
	else
		DI_Wr(DIPD_COMB_CTRL5, 0x04040804);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXLX))
		DI_Wr(DIPD_COMB_CTRL6, 0x00107064);
}
void mc_pre_mv_irq(void)
{
	unsigned int val1;

	val1 = RDMA_RD(MCDI_RO_PD_22_FLG);
	RDMA_WR(MCDI_PD_22_CHK_FLG_CNT, val1);

	val1 = RDMA_RD_BITS(MCDI_RO_HIGH_VERT_FRQ_FLG, 0, 1);
	RDMA_WR_BITS(MCDI_FIELD_HVF_PRDX_CNT, val1, 0, 1);
	val1 = RDMA_RD_BITS(MCDI_RO_HIGH_VERT_FRQ_FLG, 1, 2);
	RDMA_WR_BITS(MCDI_FIELD_HVF_PRDX_CNT, val1, 2, 2);
	val1 = RDMA_RD_BITS(MCDI_RO_HIGH_VERT_FRQ_FLG, 8, 8);
	RDMA_WR_BITS(MCDI_FIELD_HVF_PRDX_CNT, val1, 8, 8);
	val1 = RDMA_RD_BITS(MCDI_RO_MOTION_PARADOX_FLG, 0, 16);
	RDMA_WR_BITS(MCDI_FIELD_HVF_PRDX_CNT, val1, 16, 16);

	val1 = RDMA_RD_BITS(MCDI_RO_RPT_MV, 0, 6);
	if (val1 == 32) {
		val1 = 0;
		RDMA_WR_BITS(MCDI_CTRL_MODE, 0, 15, 1);
	} else {
		RDMA_WR_BITS(MCDI_CTRL_MODE, 1, 15, 1);
	}

	RDMA_WR_BITS(MCDI_FIELD_MV, val1, 8, 6);

	val1 = RDMA_RD_BITS(MCDI_RO_GMV_LOCK_FLG, 0, 1);
	RDMA_WR_BITS(MCDI_FIELD_MV, val1, 14, 1);

	val1 = RDMA_RD_BITS(MCDI_RO_GMV_LOCK_FLG, 8, 8);
	RDMA_WR_BITS(MCDI_FIELD_MV, val1, 16, 8);

	val1 = RDMA_RD_BITS(MCDI_RO_GMV_LOCK_FLG, 2, 6);
	if (val1 == 32) {
		val1 = 0;
		RDMA_WR_BITS(MCDI_CTRL_MODE, 0, 14, 1);
	} else {
		RDMA_WR_BITS(MCDI_CTRL_MODE, 1, 14, 1);
	}
	RDMA_WR_BITS(MCDI_FIELD_MV, val1, 0, 6);

	val1 = RDMA_RD(MCDI_FIELD_LUMA_AVG_SUM_1);
	RDMA_WR(MCDI_FIELD_LUMA_AVG_SUM_0, val1);

	val1 = RDMA_RD(MCDI_RO_FLD_LUMA_AVG_SUM);
	RDMA_WR(MCDI_FIELD_LUMA_AVG_SUM_1, val1);

}

static void lmvs_init(struct mcinfo_lmv_s *lmvs, unsigned short lmv)
{
		lmvs->lock_flag = (lmv>>14) & 3;
		lmvs->lmv = (lmv>>8)&63;
		lmvs->lmv = lmvs->lmv > 32 ? (32 - lmvs->lmv) : lmvs->lmv;
		lmvs->lock_cnt = (lmv & 255);
}

static bool lmv_lock_win_en;
module_param_named(lmv_lock_win_en, lmv_lock_win_en, bool, 0644);

void calc_lmv_init(void)
{
	if (lmv_lock_win_en) {
		RDMA_WR_BITS(MCDI_REL_DET_LMV_DIF_CHK, 3, 12, 2);
		RDMA_WR_BITS(MCDI_LMVLCKSTEXT_1, 3, 30, 2);
	} else {
		RDMA_WR_BITS(MCDI_REL_DET_LMV_DIF_CHK, 0, 12, 2);
		RDMA_WR_BITS(MCDI_LMVLCKSTEXT_1, 0, 30, 2);
	}
}

static short lmv_dist = 5;
module_param_named(lmv_dist, lmv_dist, short, 0644);

static unsigned short pr_mcinfo_cnt;
module_param_named(pr_mcinfo_cnt, pr_mcinfo_cnt, ushort, 0644);
static struct mcinfo_lmv_s lines_mv[540];
static short offset_lmv = 100;
module_param_named(offset_lmv, offset_lmv, short, 0644);

void calc_lmv_base_mcinfo(unsigned int vf_height, unsigned long mcinfo_adr)
{
	unsigned short i, top_str, bot_str, top_end, bot_end, j = 0;
	unsigned short *mcinfo_vadr = NULL, lck_num;
	unsigned short flg_m1 = 0, flg_i = 0, nLmvLckSt = 0;
	unsigned short lmv_lckstext[3] = {0, 0, 0}, nLmvLckEd;
	unsigned short lmv_lckedext[3] = {0, 0, 0}, nLmvLckNum;

	mcinfo_vadr = (unsigned short *)phys_to_virt(mcinfo_adr);
	if (!lmv_lock_win_en)
		return;
	for (i = 0; i < (vf_height>>1); i++) {
		lmvs_init(&lines_mv[i], *(mcinfo_vadr+i));
		j = i + (vf_height>>1);
		lmvs_init(&lines_mv[j], *(mcinfo_vadr+i+272));
		if (pr_mcinfo_cnt && j < (vf_height - 10) &&
			j > (vf_height - offset_lmv)) {
			pr_info("MCINFO[%u]=0x%x\t", j,
				*(mcinfo_vadr + i + 272));
			if (i%16 == 0)
				pr_info("\n");
		}
	}
	pr_mcinfo_cnt ? pr_mcinfo_cnt-- : (pr_mcinfo_cnt = 0);
	top_str = 0;
	top_end = offset_lmv;
	i = top_str;
	j = 0;
	lck_num = Rd_reg_bits(MCDI_LMVLCKSTEXT_1, 16, 12);

	while (i < top_end) {
		flg_m1 = (i == top_str) ? 0 :
			(lines_mv[i-1].lock_flag > 0);
		flg_i  = (i == top_end - 1) ? 0 :
			lines_mv[i].lock_flag > 0;
		if (!flg_m1 && flg_i) {
			nLmvLckSt = (j == 0) ? i : ((i < (lmv_lckedext[j-1] +
				lmv_dist)) ? lmv_lckstext[j-1] : i);
			j = (nLmvLckSt != i) ? (j - 1) : j;
		} else if (flg_m1 && !flg_i) {
			nLmvLckEd = i;
			nLmvLckNum = (nLmvLckEd - nLmvLckSt + 1);
			if (nLmvLckNum >= lck_num) {
				lmv_lckstext[j] = nLmvLckSt;
				lmv_lckedext[j] = nLmvLckEd;
				j++;
			}
		}
		i++;
		if (j > 2)
			break;
	}

	bot_str = vf_height - offset_lmv - 1;
	bot_end = vf_height;
	i = bot_str;
	while (i < bot_end && j < 3) {
		flg_m1 = (i == bot_str) ? 0 :
			(lines_mv[i-1].lock_flag > 0);
		flg_i  = (i == bot_end - 1) ? 0 :
			lines_mv[i].lock_flag > 0;
		if (!flg_m1 && flg_i) {
			nLmvLckSt = (j == 0) ? i : ((i < (lmv_lckedext[j-1] +
				lmv_dist)) ? lmv_lckstext[j-1] : i);
			j = (nLmvLckSt != i) ? (j - 1) : j;
		} else if (flg_m1 && !flg_i) {
			nLmvLckEd = i;
			nLmvLckNum = (nLmvLckEd - nLmvLckSt + 1);
			if (nLmvLckNum >= lck_num) {
				lmv_lckstext[j] = nLmvLckSt;
				lmv_lckedext[j] = nLmvLckEd;
				j++;
			}
		}
		i++;
		if (j > 2)
			break;
	}

	Wr(MCDI_LMVLCKSTEXT_0, lmv_lckstext[1]<<16 | lmv_lckstext[0]);
	Wr_reg_bits(MCDI_LMVLCKSTEXT_1, lmv_lckstext[2], 0, 12);
	Wr(MCDI_LMVLCKEDEXT_0, lmv_lckedext[1]<<16 | lmv_lckedext[0]);
	Wr(MCDI_LMVLCKEDEXT_1, lmv_lckedext[2]);
}

void di_hw_init(bool pd_enable, bool mc_enable)
{
	unsigned short fifo_size_vpp = 0xc0;
	unsigned short fifo_size_di = 0xc0;
	switch_vpu_clk_gate_vmod(VPU_VPU_CLKB, VPU_CLK_GATE_ON);
	if (is_meson_txlx_cpu() || is_meson_txhd_cpu())
		di_top_gate_control(true, true);
	else if (is_meson_gxl_cpu()	|| is_meson_gxm_cpu()
		|| is_meson_gxlx_cpu())
		DI_Wr(DI_CLKG_CTRL, 0xffff0001);
	else
		DI_Wr(DI_CLKG_CTRL, 0x1); /* di no clock gate */

	if (is_meson_txlx_cpu() ||
		is_meson_gxlx_cpu() ||
		is_meson_txhd_cpu()) {
		/* vpp fifo max size on txl :128*3=384[0x180] */
		/* di fifo max size on txl :96*3=288[0x120] */
		fifo_size_vpp = 0x180;
		fifo_size_di = 0x120;
	}
	/* enable di all arb */
	DI_Wr_reg_bits(DI_ARB_CTRL, 0xf0f, 0, 16);
	DI_Wr(VD1_IF0_LUMA_FIFO_SIZE, fifo_size_vpp);
	DI_Wr(VD2_IF0_LUMA_FIFO_SIZE, fifo_size_vpp);
	/* 1a83 is vd2_if0_luma_fifo_size */
	DI_Wr(DI_INP_LUMA_FIFO_SIZE,	fifo_size_di);
	/* 17d8 is DI_INP_luma_fifo_size */
	DI_Wr(DI_MEM_LUMA_FIFO_SIZE,	fifo_size_di);
	/* 17e5 is DI_MEM_luma_fifo_size */
	DI_Wr(DI_IF1_LUMA_FIFO_SIZE,	fifo_size_di);
	/* 17f2 is  DI_IF1_luma_fifo_size */
	DI_Wr(DI_IF2_LUMA_FIFO_SIZE,	fifo_size_di);
	/* 201a is if2 fifo size */
	DI_Wr(DI_CHAN2_LUMA_FIFO_SIZE, fifo_size_di);
	/* 17b3 is DI_chan2_luma_fifo_size */
	if (is_meson_txlx_cpu() ||
		is_meson_txhd_cpu()) {
		di_pre_gate_control(true, true);
		di_post_gate_control(true);
	}
	DI_Wr(DI_PRE_HOLD, (1 << 31) | (31 << 16) | 31);

	ma_di_init();
	ei_hw_init();
	nr_hw_init();
	if (pd_enable)
		init_field_mode(288);

	if (mc_enable)
		mc_di_param_init();
	if (is_meson_txlx_cpu() ||
		is_meson_txhd_cpu()) {
		di_pre_gate_control(false, true);
		di_post_gate_control(false);
		di_top_gate_control(false, false);
	} else if (is_meson_gxlx_cpu()) {
		/* di clock div enable for pq load */
		DI_Wr(DI_CLKG_CTRL, 0x80000000);
	} else {
		DI_Wr(DI_CLKG_CTRL, 0x2); /* di clock gate all */
	}
	switch_vpu_clk_gate_vmod(VPU_VPU_CLKB,
		VPU_CLK_GATE_OFF);

}

void di_hw_uninit(void)
{
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX))
		nr_gate_control(false);
}

/* config di pre bit mode */
static void pre_bit_mode_config(unsigned char inp,
	unsigned char mem, unsigned char chan2, unsigned char nrwr)
{
	if (!cpu_after_eq(MESON_CPU_MAJOR_ID_GXTVBB))
		return;

	RDMA_WR_BITS(DI_INP_GEN_REG3, inp&0x3, 8, 2);
	RDMA_WR_BITS(DI_MEM_GEN_REG3, mem&0x3, 8, 2);
	RDMA_WR_BITS(DI_CHAN2_GEN_REG3, chan2&0x3, 8, 2);
	RDMA_WR_BITS(DI_NRWR_Y, nrwr&0x1, 14, 1);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL) && ((nrwr&0x3) == 0x3))
		RDMA_WR_BITS(DI_NRWR_CTRL, 0x3, 22, 2);
}

void enable_di_pre_aml(
	struct DI_MIF_s		   *di_inp_mif,
	struct DI_MIF_s		   *di_mem_mif,
	struct DI_MIF_s		   *di_chan2_mif,
	struct DI_SIM_MIF_s    *di_nrwr_mif,
	struct DI_SIM_MIF_s    *di_mtnwr_mif,
	struct DI_SIM_MIF_s    *di_contp2rd_mif,
	struct DI_SIM_MIF_s    *di_contprd_mif,
	struct DI_SIM_MIF_s    *di_contwr_mif,
	int nr_en, int mtn_en, int pd32_check_en, int pd22_check_en,
	int hist_check_en, int pre_field_num, int pre_vdin_link,
	int hold_line, int urgent)
{
	int hist_check_only = 0;

	pd32_check_en = 1; /* for progressive luma detection */

	hist_check_only = hist_check_en && !nr_en && !mtn_en &&
			!pd22_check_en && !pd32_check_en;

	if (nr_en | mtn_en | pd22_check_en || pd32_check_en) {
		set_di_mem_mif(di_mem_mif, urgent, hold_line);
		set_di_inp_mif(di_inp_mif, urgent, hold_line);
	}

	if (pd22_check_en || hist_check_only)
		set_di_chan2_mif(di_chan2_mif, urgent, hold_line);
	/* set nr wr mif interface. */
	if (nr_en) {
		RDMA_WR_BITS(DI_NRWR_X, di_nrwr_mif->end_x, 0, 14);
		RDMA_WR_BITS(DI_NRWR_X,	di_nrwr_mif->start_x, 16, 14);
		RDMA_WR_BITS(DI_NRWR_Y, di_nrwr_mif->start_y, 16, 13);
		RDMA_WR_BITS(DI_NRWR_Y, di_nrwr_mif->end_y, 0, 13);
		RDMA_WR_BITS(DI_NRWR_Y, 3, 30, 2);
		RDMA_WR(DI_NRWR_CTRL, di_nrwr_mif->canvas_num|
			(urgent<<16)|
			2<<26 |/*burst_lim 1->2 2->4*/
			1<<30); /* urgent bit 16 */
	}

	pre_bit_mode_config(di_inp_mif->bit_mode,
				di_mem_mif->bit_mode,
				di_chan2_mif->bit_mode,
				di_nrwr_mif->bit_mode);
	/* motion wr mif. */
	if (mtn_en) {
		RDMA_WR(DI_CONTWR_X, (di_contwr_mif->start_x << 16)|
			(di_contwr_mif->end_x));
		RDMA_WR(DI_CONTWR_Y, (di_contwr_mif->start_y << 16)|
			(di_contwr_mif->end_y));
		RDMA_WR(DI_CONTWR_CTRL, di_contwr_mif->canvas_num|
			(urgent << 8));/* urgent. */
		RDMA_WR(DI_CONTPRD_X, (di_contprd_mif->start_x << 16)|
			(di_contprd_mif->end_x));
		RDMA_WR(DI_CONTPRD_Y, (di_contprd_mif->start_y << 16)|
			(di_contprd_mif->end_y));
		RDMA_WR(DI_CONTP2RD_X, (di_contp2rd_mif->start_x << 16)|
			(di_contp2rd_mif->end_x));
		RDMA_WR(DI_CONTP2RD_Y, (di_contp2rd_mif->start_y << 16)|
			(di_contp2rd_mif->end_y));
		RDMA_WR(DI_CONTRD_CTRL, (di_contprd_mif->canvas_num << 8)|
						  (urgent << 16)|/* urgent */
						   di_contp2rd_mif->canvas_num);
		/* current field mtn canvas index. */
		RDMA_WR(DI_MTNWR_X, (di_mtnwr_mif->start_x << 16)|
			(di_mtnwr_mif->end_x));
		RDMA_WR(DI_MTNWR_Y, (di_mtnwr_mif->start_y << 16)|
			(di_mtnwr_mif->end_y));
		RDMA_WR(DI_MTNWR_CTRL, di_mtnwr_mif->canvas_num|
						(urgent << 8));	/* urgent. */
	}

	RDMA_WR(DI_PRE_CTRL, nr_en |			/* NR enable */
					(mtn_en << 1) |	/* MTN_EN */
			(pd32_check_en << 2)|/* check 3:2 pulldown */
			(pd22_check_en << 3)|/* check 2:2 pulldown */
					(1 << 4) |
/* 2:2 check mid pixel come from next field after MTN. */
			(hist_check_en << 5)|/* hist check enable */
			(1 << 6)|/* hist check  use chan2. */
					((!nr_en) << 7)|
/* hist check use data before noise reduction. */
	((pd22_check_en || hist_check_only) << 8)|
	/* chan 2 enable for 2:2 pull down check.*/
		(pd22_check_en << 9) |/* line buffer 2 enable */
					(0 << 10) |	/* pre drop first. */
					(0 << 11) | /* di pre repeat */
					(0 << 12) | /* pre viu link */
			(pre_vdin_link << 13) |
			(pre_vdin_link << 14) |/* pre go line link */
			(hold_line << 16)|/* pre hold line number */
					(1 << 22)|/* MTN after NR. */
			(pre_field_num << 29)/* pre field number.*/
				   );

}
void enable_afbc_input(struct vframe_s *vf)
{
	unsigned int r, u, v;

	if (vf->type & VIDTYPE_COMPRESS) {
		r = (3 << 24) |
		    (17 << 16) |
		    (1 << 14) | /*burst1 1*/
		    vf->bitdepth;

		if ((vf->type & VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_TOP)
			r |= 0x44;
		else if ((vf->type & VIDTYPE_TYPEMASK) ==
					VIDTYPE_INTERLACE_BOTTOM)
			r |= 0x88;

		RDMA_WR(AFBC_MODE, r);
		RDMA_WR(AFBC_ENABLE, 0x1700);
		RDMA_WR(AFBC_CONV_CTRL, 0x100);
		u = (vf->bitdepth >> (BITDEPTH_U_SHIFT)) & 0x3;
		v = (vf->bitdepth >> (BITDEPTH_V_SHIFT)) & 0x3;
		RDMA_WR(AFBC_DEC_DEF_COLOR,
			0x3FF00000 | /*Y,bit20+*/
			0x80 << (u + 10) |
			0x80 << v);
		/* chroma formatter */
		RDMA_WR(AFBC_VD_CFMT_CTRL,
			(1<<21)|/* HFORMATTER_YC_RATIO_2_1 */
			(1<<20)|/* HFORMATTER_EN */
			(1<<16)|/* VFORMATTER_RPTLINE0_EN */
			(0x8 << 1)|/* VFORMATTER_PHASE_BIT */
			1);/* VFORMATTER_EN */

		RDMA_WR(AFBC_VD_CFMT_W,
			(vf->width << 16) | (vf->width/2));

		RDMA_WR(AFBC_MIF_HOR_SCOPE,
			(0 << 16) | ((vf->width - 1)>>5));

		RDMA_WR(AFBC_PIXEL_HOR_SCOPE,
			(0 << 16) | (vf->width - 1));
		RDMA_WR(AFBC_VD_CFMT_H, vf->height);

		RDMA_WR(AFBC_MIF_VER_SCOPE,
		    (0 << 16) | ((vf->height-1)>>2));

		RDMA_WR(AFBC_PIXEL_VER_SCOPE,
			0 << 16 | (vf->height-1));
		RDMA_WR(AFBC_SIZE_IN, vf->height | vf->width << 16);
		RDMA_WR(AFBC_HEAD_BADDR, vf->canvas0Addr>>4);
		RDMA_WR(AFBC_BODY_BADDR, vf->canvas1Addr>>4);
		/* disable inp memory */
		RDMA_WR_BITS(DI_INP_GEN_REG, 0, 0, 1);
		/* afbc to di enable */
		if (Rd_reg_bits(VIU_MISC_CTRL0, 19, 1) != 1)
			RDMA_WR_BITS(VIU_MISC_CTRL0, 1, 19, 1);
		/* DI inp(current data) switch to AFBC */
		RDMA_WR_BITS(VIUB_MISC_CTRL0, 1, 16, 1);
	} else {
		RDMA_WR(AFBC_ENABLE, 0);
		/* afbc to vpp(replace vd1) enable */
		if (Rd_reg_bits(VIU_MISC_CTRL0, 19, 1) != 0)
			RDMA_WR_BITS(VIU_MISC_CTRL0, 0, 19, 1);
		/* DI inp(current data) switch to memory */
		RDMA_WR_BITS(VIUB_MISC_CTRL0, 0, 16, 1);
	}
}

void enable_mc_di_pre(struct DI_MC_MIF_s *di_mcinford_mif,
	struct DI_MC_MIF_s *di_mcinfowr_mif,
	struct DI_MC_MIF_s *di_mcvecwr_mif, int urgent)
{
	RDMA_WR(MCDI_MCVECWR_X, di_mcvecwr_mif->size_x);
	RDMA_WR(MCDI_MCVECWR_Y, di_mcvecwr_mif->size_y);
	RDMA_WR(MCDI_MCINFOWR_X, di_mcinfowr_mif->size_x);
	RDMA_WR(MCDI_MCINFOWR_Y, di_mcinfowr_mif->size_y);

	RDMA_WR(MCDI_MCINFORD_X, di_mcinford_mif->size_x);
	RDMA_WR(MCDI_MCINFORD_Y, di_mcinford_mif->size_y);
	RDMA_WR(MCDI_MCVECWR_CANVAS_SIZE,
		(di_mcvecwr_mif->size_x<<16)+di_mcvecwr_mif->size_y);
	RDMA_WR(MCDI_MCINFOWR_CANVAS_SIZE,
		(di_mcinfowr_mif->size_x<<16)+di_mcinfowr_mif->size_y);
	RDMA_WR(MCDI_MCINFORD_CANVAS_SIZE,
		(di_mcinford_mif->size_x<<16)+di_mcinford_mif->size_y);

	/* DI_Wr(MCDI_MOTINEN,1<<1);	//enable motin refinement */

	RDMA_WR(MCDI_MCVECWR_CTRL, di_mcvecwr_mif->canvas_num |
			(0<<14) |	 /* sync latch en */
			(urgent<<8) |   /* urgent */
			(1<<12) |	 /* enable reset by frame rst */
			(0x4031<<16));
	RDMA_WR(MCDI_MCINFOWR_CTRL, di_mcinfowr_mif->canvas_num |
			(0<<14) |	 /* sync latch en */
			(urgent<<8) |	 /* urgent */
			(1<<12) |	 /* enable reset by frame rst */
			(0x4042<<16));
	RDMA_WR(MCDI_MCINFORD_CTRL, di_mcinford_mif->canvas_num |
			(0<<10) |	 /* sync latch en */
			(urgent<<8) |   /* urgent */
			(1<<9)  |		/* enable reset by frame rst */
			(0x42<<16));
}

void enable_mc_di_post(struct DI_MC_MIF_s *di_mcvecrd_mif,
	int urgent, bool reverse, int invert_mv)
{
	di_mcvecrd_mif->size_y =
			(di_mcvecrd_mif->end_y - di_mcvecrd_mif->start_y + 1);
	DI_VSYNC_WR_MPEG_REG(MCDI_MCVECRD_X, (reverse?1:0)<<30 |
			di_mcvecrd_mif->start_x << 16 |
			(di_mcvecrd_mif->size_x+di_mcvecrd_mif->start_x));
	DI_VSYNC_WR_MPEG_REG(MCDI_MCVECRD_Y, (reverse?1:0)<<30 |
			di_mcvecrd_mif->start_y << 16 |
			di_mcvecrd_mif->end_y);
	DI_VSYNC_WR_MPEG_REG(MCDI_MCVECRD_CANVAS_SIZE,
			(di_mcvecrd_mif->size_x << 16) |
			di_mcvecrd_mif->size_y);
	DI_VSYNC_WR_MPEG_REG(MCDI_MCVECRD_CTRL, di_mcvecrd_mif->canvas_num |
						(urgent<<8)|/* urgent */
						(1<<9)|/* canvas enable */
						(0 << 10) |
						(0x31<<16));
	DI_VSYNC_WR_MPEG_REG_BITS(MCDI_MC_CRTL, di_mcvecrd_mif->vecrd_offset,
		12, 3);
	if (di_mcvecrd_mif->blend_en)
		DI_VSYNC_WR_MPEG_REG_BITS(MCDI_MC_CRTL, mcen_mode, 0, 2);
	else
		DI_VSYNC_WR_MPEG_REG_BITS(MCDI_MC_CRTL, 0, 0, 2);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL)) {
		DI_VSYNC_WR_MPEG_REG_BITS(MCDI_MC_CRTL, mcuv_en, 10, 1);
		DI_VSYNC_WR_MPEG_REG_BITS(MCDI_MC_CRTL, 1, 11, 1);
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXLX)) {
			DI_VSYNC_WR_MPEG_REG_BITS(MCDI_MC_CRTL,
				invert_mv, 17, 1);
			DI_VSYNC_WR_MPEG_REG_BITS(MCDI_MC_CRTL,
				3, 18, 2);
		}
	} else
		DI_VSYNC_WR_MPEG_REG_BITS(MCDI_MC_CRTL, mcuv_en, 9, 1);
	DI_VSYNC_WR_MPEG_REG_BITS(MCDI_MC_CRTL, mcdebug_mode, 2, 3);
}



static void set_di_inp_fmt_more(unsigned int repeat_l0_en,
				int hz_yc_ratio,		/* 2bit */
				int hz_ini_phase,		/* 4bit */
				int vfmt_en,
				int vt_yc_ratio,		/* 2bit */
				int vt_ini_phase,		/* 4bit */
				int y_length,
				int c_length,
				int hz_rpt	/* 1bit */
		)
{
	int hfmt_en = 1, nrpt_phase0_en = 0;
	int vt_phase_step = (16 >> vt_yc_ratio);

	RDMA_WR(DI_INP_FMT_CTRL, (hz_rpt << 28)	|/* hz rpt pixel */
				  (hz_ini_phase << 24)	|/* hz ini phase */
				  (0 << 23)	|/* repeat p0 enable */
				  (hz_yc_ratio << 21)	|/* hz yc ratio */
				  (hfmt_en << 20)		|/* hz enable */
				 (nrpt_phase0_en << 17)|/* nrpt_phase0 enable */
				  (repeat_l0_en << 16)|/* repeat l0 enable */
				  (0 << 12)		|/* skip line num */
				  (vt_ini_phase << 8)	|/* vt ini phase */
				  (vt_phase_step << 1)|/* vt phase step (3.4) */
				  (vfmt_en << 0)		/* vt enable */
					);

	RDMA_WR(DI_INP_FMT_W, (y_length << 16) |	/* hz format width */
					 (c_length << 0)/* vt format width */
					);
}

static void set_di_inp_mif(struct DI_MIF_s *mif, int urgent, int hold_line)
{
	unsigned int bytes_per_pixel;
	unsigned int demux_mode;
	unsigned int chro_rpt_lastl_ctrl, vfmt_rpt_first = 0;
	unsigned int luma0_rpt_loop_start;
	unsigned int luma0_rpt_loop_end;
	unsigned int luma0_rpt_loop_pat;
	unsigned int chroma0_rpt_loop_start;
	unsigned int chroma0_rpt_loop_end;
	unsigned int chroma0_rpt_loop_pat;
	unsigned int vt_ini_phase = 0;
	unsigned int reset_on_gofield;

	if (mif->set_separate_en != 0 && mif->src_field_mode == 1) {
		chro_rpt_lastl_ctrl = 1;
		luma0_rpt_loop_start = 1;
		luma0_rpt_loop_end = 1;
		chroma0_rpt_loop_start = mif->src_prog?0:1;
		chroma0_rpt_loop_end = mif->src_prog?0:1;
		luma0_rpt_loop_pat = 0x80;
		chroma0_rpt_loop_pat = mif->src_prog?0:0x80;

		vfmt_rpt_first = 1;
		if (mif->output_field_num == 0)
			vt_ini_phase = 0xe;
		else
			vt_ini_phase = 0xa;

		if (mif->src_prog) {
			if (mif->output_field_num == 0) {
				vt_ini_phase = 0xc;
			} else {
				vt_ini_phase = 0x4;
				vfmt_rpt_first = 0;
			}
		}

	} else if (mif->set_separate_en != 0 && mif->src_field_mode == 0) {
		chro_rpt_lastl_ctrl = 1;
		luma0_rpt_loop_start = 0;
		luma0_rpt_loop_end = 0;
		chroma0_rpt_loop_start = 0;
		chroma0_rpt_loop_end = 0;
		luma0_rpt_loop_pat = 0x0;
		chroma0_rpt_loop_pat = 0x0;
	} else if (mif->set_separate_en == 0 && mif->src_field_mode == 1) {
		chro_rpt_lastl_ctrl = 1;
		luma0_rpt_loop_start = 1;
		luma0_rpt_loop_end = 1;
		chroma0_rpt_loop_start = 1;
		chroma0_rpt_loop_end = 1;
		luma0_rpt_loop_pat = 0x80;
		chroma0_rpt_loop_pat = 0x80;
	} else {
		chro_rpt_lastl_ctrl = 0;
		luma0_rpt_loop_start = 0;
		luma0_rpt_loop_end = 0;
		chroma0_rpt_loop_start = 0;
		chroma0_rpt_loop_end = 0;
		luma0_rpt_loop_pat = 0x00;
		chroma0_rpt_loop_pat = 0x00;
	}


	bytes_per_pixel = mif->set_separate_en ? 0 : (mif->video_mode ? 2 : 1);
	demux_mode = mif->video_mode;


	/* ---------------------- */
	/* General register */
	/* ---------------------- */
	reset_on_gofield = (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXBB)?0:1;
	RDMA_WR(DI_INP_GEN_REG, (reset_on_gofield << 29) |
				(urgent << 28)	|/* chroma urgent bit */
				(urgent << 27)	|/* luma urgent bit. */
				(1 << 25)		|/* no dummy data. */
				(hold_line << 19)	|/* hold lines */
				(1 << 18)	|/* push dummy pixel */
				(demux_mode << 16)	|/* demux_mode */
				(bytes_per_pixel << 14)		|
				(1 << 12)	|/*burst_size_cr*/
				(1 << 10)	|/*burst_size_cb*/
				(3 << 8)	|/*burst_size_y*/
				(chro_rpt_lastl_ctrl << 6)	|
				((mif->set_separate_en != 0) << 1) |
				(0 << 0)/* cntl_enable */
	  );
	if (mif->set_separate_en == 2) {
		/* Enable NV12 Display */
		RDMA_WR_BITS(DI_INP_GEN_REG2, 1, 0, 1);
	} else {
		RDMA_WR_BITS(DI_INP_GEN_REG2, 0, 0, 1);
	}

	RDMA_WR(DI_INP_CANVAS0, (mif->canvas0_addr2 << 16) |
				/* cntl_canvas0_addr2 */
				(mif->canvas0_addr1 << 8) |
				/* cntl_canvas0_addr1 */
				(mif->canvas0_addr0 << 0)
				/* cntl_canvas0_addr0 */
	);

	/* ---------------------- */
	/* Picture 0 X/Y start,end */
	/* ---------------------- */
	RDMA_WR(DI_INP_LUMA_X0, (mif->luma_x_end0 << 16) |
			/* cntl_luma_x_end0 */
			(mif->luma_x_start0 << 0)/* cntl_luma_x_start0 */
		);
	RDMA_WR(DI_INP_LUMA_Y0, (mif->luma_y_end0 << 16) |
			/* cntl_luma_y_end0 */
			(mif->luma_y_start0 << 0) /* cntl_luma_y_start0 */
		);
	RDMA_WR(DI_INP_CHROMA_X0, (mif->chroma_x_end0 << 16) |
			(mif->chroma_x_start0 << 0)
		);
	RDMA_WR(DI_INP_CHROMA_Y0, (mif->chroma_y_end0 << 16) |
		   (mif->chroma_y_start0 << 0)
		);

	/* ---------------------- */
	/* Repeat or skip */
	/* ---------------------- */
	RDMA_WR(DI_INP_RPT_LOOP, (0 << 28) |
					(0 << 24) |
					(0 << 20) |
					(0 << 16) |
					(chroma0_rpt_loop_start << 12) |
					(chroma0_rpt_loop_end << 8)	|
					(luma0_rpt_loop_start << 4)	|
					(luma0_rpt_loop_end << 0)
		);

	RDMA_WR(DI_INP_LUMA0_RPT_PAT, luma0_rpt_loop_pat);
	RDMA_WR(DI_INP_CHROMA0_RPT_PAT, chroma0_rpt_loop_pat);

	/* Dummy pixel value */
	RDMA_WR(DI_INP_DUMMY_PIXEL, 0x00808000);
	if ((mif->set_separate_en != 0)) {/* 4:2:0 block mode.*/
		set_di_inp_fmt_more(
						vfmt_rpt_first,/* hfmt_en */
						1,/* hz_yc_ratio */
						0,/* hz_ini_phase */
						1,/* vfmt_en */
		mif->src_prog?0:1,/* vt_yc_ratio */
						vt_ini_phase,/* vt_ini_phase */
		mif->luma_x_end0 - mif->luma_x_start0 + 1,
						/* y_length */
		mif->chroma_x_end0 - mif->chroma_x_start0 + 1,
						/* c length */
						0);	/* hz repeat. */
	} else {
		set_di_inp_fmt_more(
						vfmt_rpt_first,	/* hfmt_en */
						1,	/* hz_yc_ratio */
						0,	/* hz_ini_phase */
						0,	/* vfmt_en */
						0,	/* vt_yc_ratio */
						0,	/* vt_ini_phase */
	mif->luma_x_end0 - mif->luma_x_start0 + 1,
	((mif->luma_x_end0 >> 1) - (mif->luma_x_start0>>1) + 1),
						0); /* hz repeat. */
	}
}

static void set_di_mem_fmt_more(int hfmt_en,
				int hz_yc_ratio,	/* 2bit */
				int hz_ini_phase,	/* 4bit */
				int vfmt_en,
				int vt_yc_ratio,	/* 2bit */
				int vt_ini_phase,	/* 4bit */
				int y_length,
				int c_length,
				int hz_rpt	/* 1bit */
		)
{
	int vt_phase_step = (16 >> vt_yc_ratio);

	RDMA_WR(DI_MEM_FMT_CTRL,
				(hz_rpt << 28)	| /* hz rpt pixel */
			(hz_ini_phase << 24) | /* hz ini phase */
				(0 << 23)	| /* repeat p0 enable */
			(hz_yc_ratio << 21)	| /* hz yc ratio */
				(hfmt_en << 20)	| /* hz enable */
				(1 << 17)	| /* nrpt_phase0 enable */
				(0 << 16)	| /* repeat l0 enable */
				(0 << 12)	| /* skip line num */
			(vt_ini_phase << 8)	| /* vt ini phase */
			(vt_phase_step << 1) | /* vt phase step (3.4) */
			(vfmt_en << 0)		/* vt enable */
				);

	RDMA_WR(DI_MEM_FMT_W, (y_length << 16) |	/* hz format width */
					(c_length << 0)	/* vt format width */
				);
}

static void set_di_chan2_fmt_more(int hfmt_en,
				int hz_yc_ratio,/* 2bit */
				int hz_ini_phase,/* 4bit */
				int vfmt_en,
				int vt_yc_ratio,/* 2bit */
				int vt_ini_phase,/* 4bit */
				int y_length,
				int c_length,
				int hz_rpt	/* 1bit */
		)
{
	int vt_phase_step = (16 >> vt_yc_ratio);

	RDMA_WR(DI_CHAN2_FMT_CTRL, (hz_rpt << 28) | /* hz rpt pixel */
				(hz_ini_phase << 24) | /* hz ini phase */
				(0 << 23) | /* repeat p0 enable */
				(hz_yc_ratio << 21) | /* hz yc ratio */
				(hfmt_en << 20) | /* hz enable */
				(1 << 17) | /* nrpt_phase0 enable */
				(0 << 16) | /* repeat l0 enable */
				(0 << 12) | /* skip line num */
				(vt_ini_phase << 8)	 | /* vt ini phase */
				(vt_phase_step << 1) | /* vt phase step (3.4) */
				(vfmt_en << 0)		/* vt enable */
				);

	RDMA_WR(DI_CHAN2_FMT_W, (y_length << 16) | /* hz format width */
					(c_length << 0)	/* vt format width */
				);
}

static void set_di_mem_mif(struct DI_MIF_s *mif, int urgent, int hold_line)
{
	unsigned int bytes_per_pixel;
	unsigned int demux_mode;
	unsigned int chro_rpt_lastl_ctrl;
	unsigned int luma0_rpt_loop_start;
	unsigned int luma0_rpt_loop_end;
	unsigned int luma0_rpt_loop_pat;
	unsigned int chroma0_rpt_loop_start;
	unsigned int chroma0_rpt_loop_end;
	unsigned int chroma0_rpt_loop_pat;
	unsigned int reset_on_gofield;

	if (mif->set_separate_en != 0 && mif->src_field_mode == 1) {
		chro_rpt_lastl_ctrl = 1;
		luma0_rpt_loop_start = 1;
		luma0_rpt_loop_end = 1;
		chroma0_rpt_loop_start = 1;
		chroma0_rpt_loop_end = 1;
		luma0_rpt_loop_pat = 0x80;
		chroma0_rpt_loop_pat = 0x80;
	} else if (mif->set_separate_en != 0 && mif->src_field_mode == 0) {
		chro_rpt_lastl_ctrl = 1;
		luma0_rpt_loop_start = 0;
		luma0_rpt_loop_end = 0;
		chroma0_rpt_loop_start = 0;
		chroma0_rpt_loop_end = 0;
		luma0_rpt_loop_pat = 0x0;
		chroma0_rpt_loop_pat = 0x0;
	} else if (mif->set_separate_en == 0 && mif->src_field_mode == 1) {
		chro_rpt_lastl_ctrl = 1;
		luma0_rpt_loop_start = 1;
		luma0_rpt_loop_end = 1;
		chroma0_rpt_loop_start = 0;
		chroma0_rpt_loop_end = 0;
		luma0_rpt_loop_pat = 0x80;
		chroma0_rpt_loop_pat = 0x00;
	} else {
		chro_rpt_lastl_ctrl = 0;
		luma0_rpt_loop_start = 0;
		luma0_rpt_loop_end = 0;
		chroma0_rpt_loop_start = 0;
		chroma0_rpt_loop_end = 0;
		luma0_rpt_loop_pat = 0x00;
		chroma0_rpt_loop_pat = 0x00;
	}

	bytes_per_pixel = mif->set_separate_en ? 0 : (mif->video_mode ? 2 : 1);
	demux_mode = mif->video_mode;


	/* ---------------------- */
	/* General register */
	/* ---------------------- */
	reset_on_gofield = (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXBB)?0:1;
	RDMA_WR(DI_MEM_GEN_REG, (reset_on_gofield << 29) |
						/* reset on go field */
				(urgent << 28)	| /* urgent bit. */
				(urgent << 27)	| /* urgent bit. */
				(1 << 25)		| /* no dummy data. */
				(hold_line << 19) |	/* hold lines */
				(1 << 18) |	/* push dummy pixel */
				(demux_mode << 16) | /* demux_mode */
				(bytes_per_pixel << 14)	|
				(1 << 12)	|/*burst_size_cr*/
				(1 << 10)	|/*burst_size_cb*/
				(3 << 8)	|/*burst_size_y*/
				(chro_rpt_lastl_ctrl << 6)	|
				((mif->set_separate_en != 0) << 1)|
				(0 << 0)	/* cntl_enable */
	  );
	if (mif->set_separate_en == 2) {
		/* Enable NV12 Display */
		RDMA_WR_BITS(DI_MEM_GEN_REG2, 1, 0, 1);
	} else {
		RDMA_WR_BITS(DI_MEM_GEN_REG2, 0, 0, 1);
	}
	/* ---------------------- */
	/* Canvas */
	/* ---------------------- */
	RDMA_WR(DI_MEM_CANVAS0, (mif->canvas0_addr2 << 16) |
		/* cntl_canvas0_addr2 */
(mif->canvas0_addr1 << 8) | (mif->canvas0_addr0 << 0));

	/* ---------------------- */
	/* Picture 0 X/Y start,end */
	/* ---------------------- */
	RDMA_WR(DI_MEM_LUMA_X0, (mif->luma_x_end0 << 16) |
(mif->luma_x_start0 << 0) /* cntl_luma_x_start0 */
		);
	RDMA_WR(DI_MEM_LUMA_Y0, (mif->luma_y_end0 << 16) |
(mif->luma_y_start0 << 0) /* cntl_luma_y_start0 */
		);
	RDMA_WR(DI_MEM_CHROMA_X0, (mif->chroma_x_end0 << 16) |
				(mif->chroma_x_start0 << 0)
		);
	RDMA_WR(DI_MEM_CHROMA_Y0, (mif->chroma_y_end0 << 16) |
				(mif->chroma_y_start0 << 0)
		);

	/* ---------------------- */
	/* Repeat or skip */
	/* ---------------------- */
	RDMA_WR(DI_MEM_RPT_LOOP, (0 << 28) |
					(0	<< 24) |
					(0	<< 20) |
					(0	  << 16) |
					(chroma0_rpt_loop_start << 12) |
					(chroma0_rpt_loop_end << 8) |
					(luma0_rpt_loop_start << 4) |
					(luma0_rpt_loop_end << 0)
		);

	RDMA_WR(DI_MEM_LUMA0_RPT_PAT, luma0_rpt_loop_pat);
	RDMA_WR(DI_MEM_CHROMA0_RPT_PAT, chroma0_rpt_loop_pat);

	/* Dummy pixel value */
	RDMA_WR(DI_MEM_DUMMY_PIXEL, 0x00808000);
	if ((mif->set_separate_en != 0)) {/* 4:2:0 block mode.*/
		set_di_mem_fmt_more(
						1,	/* hfmt_en */
						1,	/* hz_yc_ratio */
						0,  /* hz_ini_phase */
						1,	/* vfmt_en */
						1,	/* vt_yc_ratio */
						0,	/* vt_ini_phase */
			mif->luma_x_end0 - mif->luma_x_start0 + 1,
						/* y_length */
			mif->chroma_x_end0 - mif->chroma_x_start0 + 1,
						/* c length */
						0);	/* hz repeat. */
	} else {
		set_di_mem_fmt_more(1,	/* hfmt_en */
		1,	/* hz_yc_ratio */
		0,  /* hz_ini_phase */
		0,	/* vfmt_en */
		0,	/* vt_yc_ratio */
		0,	/* vt_ini_phase */
		mif->luma_x_end0 - mif->luma_x_start0 + 1,
		((mif->luma_x_end0 >> 1) - (mif->luma_x_start0>>1) + 1),
		0);	/* hz repeat. */
	}
}

static void set_di_if0_fmt_more(int hfmt_en,
				int hz_yc_ratio,		/* 2bit */
				int hz_ini_phase,		/* 4bit */
				int vfmt_en,
				int vt_yc_ratio,		/* 2bit */
				int vt_ini_phase,		/* 4bit */
				int y_length,
				int c_length,
				int hz_rpt		        /* 1bit */
				)
{
	int vt_phase_step = (16 >> vt_yc_ratio);

	DI_VSYNC_WR_MPEG_REG(VIU_VD1_FMT_CTRL,
(hz_rpt << 28)	|		    /* hz rpt pixel */
(hz_ini_phase << 24)	|		    /* hz ini phase */
(0 << 23)		|		    /* repeat p0 enable */
(hz_yc_ratio << 21)	|		    /* hz yc ratio */
(hfmt_en << 20)	|		    /* hz enable */
(1 << 17)		|		    /* nrpt_phase0 enable */
(0 << 16)		|		    /* repeat l0 enable */
(0 << 12)		|		    /* skip line num */
(vt_ini_phase << 8)	|		    /* vt ini phase */
(vt_phase_step << 1)	|		    /* vt phase step (3.4) */
(vfmt_en << 0)		            /* vt enable */
					);

	DI_VSYNC_WR_MPEG_REG(VIU_VD1_FMT_W,
		(y_length << 16) |		/* hz format width */
		(c_length << 0)			/* vt format width */
					);
}

static void set_di_if1_fmt_more(int hfmt_en,
				int hz_yc_ratio,/* 2bit */
				int hz_ini_phase,/* 4bit */
				int vfmt_en,
				int vt_yc_ratio,/* 2bit */
				int vt_ini_phase,/* 4bit */
				int y_length,
				int c_length,
				int hz_rpt	/* 1bit */
				)
{
	int vt_phase_step = (16 >> vt_yc_ratio);

	DI_VSYNC_WR_MPEG_REG(DI_IF1_FMT_CTRL,
	(hz_rpt << 28)|/* hz rpt pixel */
	(hz_ini_phase << 24)|/* hz ini phase */
	(0 << 23)|/* repeat p0 enable */
	(hz_yc_ratio << 21)|/* hz yc ratio */
	(hfmt_en << 20)|/* hz enable */
	(1 << 17)|/* nrpt_phase0 enable */
	(0 << 16)|/* repeat l0 enable */
	(0 << 12)|/* skip line num */
	(vt_ini_phase << 8)|/* vt ini phase */
	(vt_phase_step << 1)|/* vt phase step (3.4) */
	(vfmt_en << 0) /* vt enable */
					);

	DI_VSYNC_WR_MPEG_REG(DI_IF1_FMT_W, (y_length << 16) |
							 (c_length << 0));
}
static void set_di_if2_fmt_more(int hfmt_en,
				int hz_yc_ratio,/* 2bit */
				int hz_ini_phase,/* 4bit */
				int vfmt_en,
				int vt_yc_ratio,/* 2bit */
				int vt_ini_phase,/* 4bit */
				int y_length,
				int c_length,
				int hz_rpt	/* 1bit */
				)
{
	int vt_phase_step = (16 >> vt_yc_ratio);

	DI_VSYNC_WR_MPEG_REG(DI_IF2_FMT_CTRL,
	(hz_rpt << 28)|/* hz rpt pixel */
	(hz_ini_phase << 24)|/* hz ini phase */
	(0 << 23)|/* repeat p0 enable */
	(hz_yc_ratio << 21)|/* hz yc ratio */
	(hfmt_en << 20)|/* hz enable */
	(1 << 17)|/* nrpt_phase0 enable */
	(0 << 16)|/* repeat l0 enable */
	(0 << 12)|/* skip line num */
	(vt_ini_phase << 8)|/* vt ini phase */
	(vt_phase_step << 1)|/* vt phase step (3.4) */
	(vfmt_en << 0) /* vt enable */
					);

	DI_VSYNC_WR_MPEG_REG(DI_IF2_FMT_W, (y_length << 16) |
							 (c_length << 0));
}

static const u32 vpat[] = {0, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf};

static void set_di_if2_mif(struct DI_MIF_s *mif, int urgent,
	int hold_line, int vskip_cnt)
{
	unsigned int bytes_per_pixel, demux_mode;
	unsigned int pat, loop = 0, chro_rpt_lastl_ctrl = 0;

	if (mif->set_separate_en == 1) {
		pat = vpat[(vskip_cnt<<1)+1];
		/*top*/
		if (mif->src_field_mode == 0) {
			chro_rpt_lastl_ctrl = 1;
			loop = 0x11;
			pat <<= 4;
		}
	} else {
		loop = 0;
		pat = vpat[vskip_cnt];
	}

	bytes_per_pixel = mif->set_separate_en ? 0 : (mif->video_mode ? 2 : 1);
	demux_mode = mif->video_mode;


	/* ---------------------- */
	/* General register */
	/* ---------------------- */

	DI_VSYNC_WR_MPEG_REG(DI_IF2_GEN_REG, (0 << 29) | /* reset on go field */
			(urgent << 28)	|/* urgent */
			(urgent << 27)	|/* luma urgent */
			(1 << 25)|/* no dummy data. */
			(hold_line << 19)|/* hold lines */
			(1 << 18)|/* push dummy pixel */
			(demux_mode << 16)|/* demux_mode */
			(bytes_per_pixel << 14)|
			(1 << 12)|/*burst_size_cr*/
			(1 << 10)|/*burst_size_cb*/
			(3 << 8)|/*burst_size_y*/
			(chro_rpt_lastl_ctrl << 6)|
			((mif->set_separate_en != 0) << 1)|
			(1 << 0)/* cntl_enable */
		);
	/* post bit mode config, if0 config in video.c
	 * DI_VSYNC_WR_MPEG_REG_BITS(DI_IF2_GEN_REG3, mif->bit_mode, 8, 2);
	 */
	/* ---------------------- */
	/* Canvas */
	/* ---------------------- */
	DI_VSYNC_WR_MPEG_REG(DI_IF2_CANVAS0, (mif->canvas0_addr2 << 16) |
		(mif->canvas0_addr1 << 8) | (mif->canvas0_addr0 << 0));

	/* ---------------------- */
	/* Picture 0 X/Y start,end */
	/* ---------------------- */
	DI_VSYNC_WR_MPEG_REG(DI_IF2_LUMA_X0, (mif->luma_x_end0 << 16) |
		(mif->luma_x_start0 << 0));
	DI_VSYNC_WR_MPEG_REG(DI_IF2_LUMA_Y0, (mif->luma_y_end0 << 16) |
		(mif->luma_y_start0 << 0));
	DI_VSYNC_WR_MPEG_REG(DI_IF2_CHROMA_X0, (mif->chroma_x_end0 << 16) |
		(mif->chroma_x_start0 << 0));
	DI_VSYNC_WR_MPEG_REG(DI_IF2_CHROMA_Y0, (mif->chroma_y_end0 << 16) |
		(mif->chroma_y_start0 << 0));

	/* ---------------------- */
	/* Repeat or skip */
	/* ---------------------- */
	DI_VSYNC_WR_MPEG_REG(DI_IF2_RPT_LOOP, (loop << 24) |
							   (loop << 16) |
							   (loop << 8) |
							   (loop << 0)
					 );

	DI_VSYNC_WR_MPEG_REG(DI_IF2_LUMA0_RPT_PAT, pat);
	DI_VSYNC_WR_MPEG_REG(DI_IF2_CHROMA0_RPT_PAT, pat);

	/* Dummy pixel value */
	DI_VSYNC_WR_MPEG_REG(DI_IF2_DUMMY_PIXEL, 0x00808000);
	if (mif->set_separate_en != 0) { /* 4:2:0 block mode. */
		set_di_if2_fmt_more(1, /* hfmt_en */
		1,/* hz_yc_ratio */
		0,/* hz_ini_phase */
		1,	/* vfmt_en */
		1, /* vt_yc_ratio */
		0, /* vt_ini_phase */
		mif->luma_x_end0 - mif->luma_x_start0 + 1,
		mif->chroma_x_end0 - mif->chroma_x_start0 + 1,
							 0); /* hz repeat. */
	} else {
		set_di_if2_fmt_more(1,	/* hfmt_en */
		1, /* hz_yc_ratio */
		0, /* hz_ini_phase */
		0,	/* vfmt_en */
		0,	/* vt_yc_ratio */
		0, /* vt_ini_phase */
			mif->luma_x_end0 - mif->luma_x_start0 + 1,
			((mif->luma_x_end0 >> 1) - (mif->luma_x_start0>>1) + 1),
							 0); /* hz repeat */
	}
}

static void set_di_if1_mif(struct DI_MIF_s *mif, int urgent,
	int hold_line, int vskip_cnt)
{
	unsigned int bytes_per_pixel, demux_mode;
	unsigned int pat, loop = 0, chro_rpt_lastl_ctrl = 0;

	if (mif->set_separate_en == 1) {
		pat = vpat[(vskip_cnt<<1)+1];
		/*top*/
		if (mif->src_field_mode == 0) {
			chro_rpt_lastl_ctrl = 1;
			loop = 0x11;
			pat <<= 4;
		}
	} else {
		loop = 0;
		pat = vpat[vskip_cnt];
	}

	bytes_per_pixel = mif->set_separate_en ? 0 : (mif->video_mode ? 2 : 1);
	demux_mode = mif->video_mode;


	/* ---------------------- */
	/* General register */
	/* ---------------------- */

	DI_VSYNC_WR_MPEG_REG(DI_IF1_GEN_REG, (0 << 29) | /* reset on go field */
			(urgent << 28)	|/* urgent */
			(urgent << 27)	|/* luma urgent */
			(1 << 25)|/* no dummy data. */
			(hold_line << 19)|/* hold lines */
			(1 << 18)|/* push dummy pixel */
			(demux_mode << 16)|/* demux_mode */
			(bytes_per_pixel << 14)|
			(1 << 12)|/*burst_size_cr*/
			(1 << 10)|/*burst_size_cb*/
			(3 << 8)|/*burst_size_y*/
			(chro_rpt_lastl_ctrl << 6)|
			((mif->set_separate_en != 0) << 1)|
			(1 << 0)/* cntl_enable */
		);
	/* ---------------------- */
	/* Canvas */
	/* ---------------------- */
	DI_VSYNC_WR_MPEG_REG(DI_IF1_CANVAS0, (mif->canvas0_addr2 << 16) |
(mif->canvas0_addr1 << 8) | (mif->canvas0_addr0 << 0));

	/* ---------------------- */
	/* Picture 0 X/Y start,end */
	/* ---------------------- */
	DI_VSYNC_WR_MPEG_REG(DI_IF1_LUMA_X0, (mif->luma_x_end0 << 16) |
(mif->luma_x_start0 << 0));
	DI_VSYNC_WR_MPEG_REG(DI_IF1_LUMA_Y0, (mif->luma_y_end0 << 16) |
(mif->luma_y_start0 << 0));
	DI_VSYNC_WR_MPEG_REG(DI_IF1_CHROMA_X0, (mif->chroma_x_end0 << 16) |
(mif->chroma_x_start0 << 0));
	DI_VSYNC_WR_MPEG_REG(DI_IF1_CHROMA_Y0, (mif->chroma_y_end0 << 16) |
(mif->chroma_y_start0 << 0));

	/* ---------------------- */
	/* Repeat or skip */
	/* ---------------------- */
	DI_VSYNC_WR_MPEG_REG(DI_IF1_RPT_LOOP, (loop << 24) |
							   (loop << 16) |
							   (loop << 8) |
							   (loop << 0)
					 );

	DI_VSYNC_WR_MPEG_REG(DI_IF1_LUMA0_RPT_PAT, pat);
	DI_VSYNC_WR_MPEG_REG(DI_IF1_CHROMA0_RPT_PAT, pat);

	/* Dummy pixel value */
	DI_VSYNC_WR_MPEG_REG(DI_IF1_DUMMY_PIXEL, 0x00808000);
	if (mif->set_separate_en != 0) { /* 4:2:0 block mode. */
		set_di_if1_fmt_more(1, /* hfmt_en */
		1,/* hz_yc_ratio */
		0,/* hz_ini_phase */
		1,	/* vfmt_en */
		1, /* vt_yc_ratio */
		0, /* vt_ini_phase */
		mif->luma_x_end0 - mif->luma_x_start0 + 1,
		mif->chroma_x_end0 - mif->chroma_x_start0 + 1,
							 0); /* hz repeat. */
	} else {
		set_di_if1_fmt_more(1,	/* hfmt_en */
		1, /* hz_yc_ratio */
		0, /* hz_ini_phase */
		0,	/* vfmt_en */
		0,	/* vt_yc_ratio */
		0, /* vt_ini_phase */
			mif->luma_x_end0 - mif->luma_x_start0 + 1,
			((mif->luma_x_end0 >> 1) - (mif->luma_x_start0>>1) + 1),
							 0); /* hz repeat */
	}
}

static void set_di_chan2_mif(struct DI_MIF_s *mif, int urgent, int hold_line)
{
	unsigned int bytes_per_pixel;
	unsigned int demux_mode;
	unsigned int chro_rpt_lastl_ctrl;
	unsigned int luma0_rpt_loop_start;
	unsigned int luma0_rpt_loop_end;
	unsigned int luma0_rpt_loop_pat;
	unsigned int chroma0_rpt_loop_start;
	unsigned int chroma0_rpt_loop_end;
	unsigned int chroma0_rpt_loop_pat;
	unsigned int reset_on_gofield;
	if (mif->set_separate_en != 0 && mif->src_field_mode == 1) {
		chro_rpt_lastl_ctrl = 1;
		luma0_rpt_loop_start = 1;
		luma0_rpt_loop_end = 1;
		chroma0_rpt_loop_start = 1;
		chroma0_rpt_loop_end = 1;
		luma0_rpt_loop_pat = 0x80;
		chroma0_rpt_loop_pat = 0x80;
	} else if (mif->set_separate_en != 0 && mif->src_field_mode == 0) {
		chro_rpt_lastl_ctrl = 1;
		luma0_rpt_loop_start = 0;
		luma0_rpt_loop_end = 0;
		chroma0_rpt_loop_start = 0;
		chroma0_rpt_loop_end = 0;
		luma0_rpt_loop_pat = 0x0;
		chroma0_rpt_loop_pat = 0x0;
	} else if (mif->set_separate_en == 0 && mif->src_field_mode == 1) {
		chro_rpt_lastl_ctrl = 1;
		luma0_rpt_loop_start = 1;
		luma0_rpt_loop_end = 1;
		chroma0_rpt_loop_start = 0;
		chroma0_rpt_loop_end = 0;
		luma0_rpt_loop_pat = 0x80;
		chroma0_rpt_loop_pat = 0x00;
	} else {
		chro_rpt_lastl_ctrl = 0;
		luma0_rpt_loop_start = 0;
		luma0_rpt_loop_end = 0;
		chroma0_rpt_loop_start = 0;
		chroma0_rpt_loop_end = 0;
		luma0_rpt_loop_pat = 0x00;
		chroma0_rpt_loop_pat = 0x00;
	}

	bytes_per_pixel = mif->set_separate_en ? 0 : (mif->video_mode ? 2 : 1);
	demux_mode = mif->video_mode;


	/* ---------------------- */
	/* General register */
	/* ---------------------- */
	reset_on_gofield = (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXBB)?0:1;
	RDMA_WR(DI_CHAN2_GEN_REG, (reset_on_gofield << 29) |
				(urgent << 28) | /* urgent */
				(urgent << 27) | /* luma urgent */
				(1 << 25) |	/* no dummy data. */
				(hold_line << 19) |	/* hold lines */
				(1 << 18) |/* push dummy pixel */
				(demux_mode << 16) |
				(bytes_per_pixel << 14)	|
				(1 << 12)	|/*burst_size_cr*/
				(1 << 10)	|/*burst_size_cb*/
				(3 << 8)	|/*burst_size_y*/
				(chro_rpt_lastl_ctrl << 6)	|
				((mif->set_separate_en != 0) << 1)|
				(0 << 0)	/* cntl_enable */
	  );
	/* ---------------------- */
	/* Canvas */
	/* ---------------------- */
	if (mif->set_separate_en == 2) {
		/* Enable NV12 Display */
		RDMA_WR_BITS(DI_CHAN2_GEN_REG2, 1, 0, 1);
	} else {
		RDMA_WR_BITS(DI_CHAN2_GEN_REG2, 0, 0, 1);
	}
	RDMA_WR(DI_CHAN2_CANVAS0, (mif->canvas0_addr2 << 16) |
				(mif->canvas0_addr1 << 8) |
				(mif->canvas0_addr0 << 0));
	/* ---------------------- */
	/* Picture 0 X/Y start,end */
	/* ---------------------- */
	RDMA_WR(DI_CHAN2_LUMA_X0, (mif->luma_x_end0 << 16) |
	/* cntl_luma_x_end0 */
(mif->luma_x_start0 << 0));
	RDMA_WR(DI_CHAN2_LUMA_Y0, (mif->luma_y_end0 << 16) |
				(mif->luma_y_start0 << 0));
	RDMA_WR(DI_CHAN2_CHROMA_X0, (mif->chroma_x_end0 << 16) |
				(mif->chroma_x_start0 << 0));
	RDMA_WR(DI_CHAN2_CHROMA_Y0, (mif->chroma_y_end0 << 16) |
				(mif->chroma_y_start0 << 0));

	/* ---------------------- */
	/* Repeat or skip */
	/* ---------------------- */
	RDMA_WR(DI_CHAN2_RPT_LOOP, (0 << 28) |
							(0	 << 24) |
							(0	 << 20) |
							(0	 << 16) |
							(0	 << 12) |
							(0	 << 8)	|
						(luma0_rpt_loop_start << 4) |
						(luma0_rpt_loop_end << 0)
	);

	RDMA_WR(DI_CHAN2_LUMA0_RPT_PAT, luma0_rpt_loop_pat);

	/* Dummy pixel value */
	RDMA_WR(DI_CHAN2_DUMMY_PIXEL, 0x00808000);

	if ((mif->set_separate_en != 0)) {	/* 4:2:0 block mode. */
		set_di_chan2_fmt_more(
						1,	/* hfmt_en */
						1,	/* hz_yc_ratio */
						0,	/* hz_ini_phase */
						1,	/* vfmt_en */
						1,	/* vt_yc_ratio */
						0,	/* vt_ini_phase */
		mif->luma_x_end0 - mif->luma_x_start0 + 1,	/* y_length */
		mif->chroma_x_end0 - mif->chroma_x_start0 + 1,/* c length */
						0);	/* hz repeat. */
	} else {
		set_di_chan2_fmt_more(
						1,	/* hfmt_en */
						1,	/* hz_yc_ratio */
						0,	/* hz_ini_phase */
						0,	/* vfmt_en */
						0,	/* vt_yc_ratio */
						0,	/* vt_ini_phase */
		mif->luma_x_end0 - mif->luma_x_start0 + 1,	/* y_length */
		((mif->luma_x_end0 >> 1) - (mif->luma_x_start0>>1) + 1),
						0);	/* hz repeat. */
	}

}

static void set_di_if0_mif(struct DI_MIF_s *mif, int urgent, int hold_line,
	int vskip_cnt, int post_write_en)
{
	unsigned int pat, loop = 0;
	unsigned int bytes_per_pixel, demux_mode;

	if (mif->set_separate_en == 1) {
		pat = vpat[(vskip_cnt<<1)+1];
		if (mif->src_field_mode == 0) {/* top */
			loop = 0x11;
			pat <<= 4;
		}
	} else {
		loop = 0;
	pat = vpat[vskip_cnt];

	if (post_write_en) {
		bytes_per_pixel =
			mif->set_separate_en ? 0 : (mif->video_mode ? 2 : 1);
		demux_mode = mif->video_mode;
		DI_VSYNC_WR_MPEG_REG(VD1_IF0_GEN_REG,
(0 << 29) | /* reset on go field */
(urgent << 28)		|	/* urgent */
(urgent << 27)		|	/* luma urgent */
(1 << 25)		|	/* no dummy data. */
(hold_line << 19)	|	/* hold lines */
(1 << 18)		|	/* push dummy pixel */
(demux_mode << 16)	|	/* demux_mode */
(bytes_per_pixel << 14)	|
(1 << 12)	|
(1 << 10)	|
(3 << 8)	|
(0 << 6)	|
((mif->set_separate_en != 0) << 1)	|
(1 << 0)			/* cntl_enable */
		);
	}
	/* ---------------------- */
	/* Canvas */
	/* ---------------------- */
	DI_VSYNC_WR_MPEG_REG(VD1_IF0_CANVAS0, (mif->canvas0_addr2 << 16)|
(mif->canvas0_addr1 << 8)|(mif->canvas0_addr0 << 0));

	/* ---------------------- */
	/* Picture 0 X/Y start,end */
	/* ---------------------- */
	DI_VSYNC_WR_MPEG_REG(VD1_IF0_LUMA_X0, (mif->luma_x_end0 << 16) |
(mif->luma_x_start0 << 0));
	DI_VSYNC_WR_MPEG_REG(VD1_IF0_LUMA_Y0, (mif->luma_y_end0 << 16) |
(mif->luma_y_start0 << 0));
	DI_VSYNC_WR_MPEG_REG(VD1_IF0_CHROMA_X0, (mif->chroma_x_end0 << 16) |
(mif->chroma_x_start0 << 0));
	DI_VSYNC_WR_MPEG_REG(VD1_IF0_CHROMA_Y0, (mif->chroma_y_end0 << 16) |
(mif->chroma_y_start0 << 0));

	}
	/* ---------------------- */
	/* Repeat or skip */
	/* ---------------------- */
	DI_VSYNC_WR_MPEG_REG(VD1_IF0_RPT_LOOP,
				   (loop << 24) |
				   (loop << 16)   |
				   (loop << 8) |
				   (loop << 0));
	DI_VSYNC_WR_MPEG_REG(VD1_IF0_LUMA0_RPT_PAT,   pat);
	DI_VSYNC_WR_MPEG_REG(VD1_IF0_CHROMA0_RPT_PAT, pat);

	if (post_write_en) {
		/* 4:2:0 block mode. */
		if (mif->set_separate_en != 0) {
			set_di_if0_fmt_more(
			1, /* hfmt_en */
			1,	/* hz_yc_ratio */
			0,	/* hz_ini_phase */
			1,	/* vfmt_en */
			1, /* vt_yc_ratio */
			0, /* vt_ini_phase */
mif->luma_x_end0 - mif->luma_x_start0 + 1, /* y_length */
mif->chroma_x_end0 - mif->chroma_x_start0 + 1, /* c length */
			0); /* hz repeat. */
		} else {
			set_di_if0_fmt_more(
	1,	/* hfmt_en */
	1,	/* hz_yc_ratio */
	0,  /* hz_ini_phase */
	0,	/* vfmt_en */
	0,	/* vt_yc_ratio */
	0,  /* vt_ini_phase */
	mif->luma_x_end0 - mif->luma_x_start0 + 1, /* y_length */
	((mif->luma_x_end0>>1) - (mif->luma_x_start0>>1) + 1), /* c length */
	0); /* hz repeat */
		}
	}
}

void initial_di_post_2(int hsize_post, int vsize_post,
	int hold_line, bool post_write_en)
{
	DI_VSYNC_WR_MPEG_REG(DI_POST_SIZE,
(hsize_post - 1) | ((vsize_post - 1) << 16));

	/* if post size < MIN_POST_WIDTH, force old ei */
	if (hsize_post < MIN_POST_WIDTH)
		DI_VSYNC_WR_MPEG_REG_BITS(DI_EI_CTRL3, 0, 31, 1);
	else
		DI_VSYNC_WR_MPEG_REG_BITS(DI_EI_CTRL3, 1, 31, 1);

	/* DI_VSYNC_WR_MPEG_REG(DI_BLEND_REG0_Y, (vsize_post>>2)-1 ); */
	DI_VSYNC_WR_MPEG_REG(DI_BLEND_REG0_Y, (vsize_post-1));
	DI_VSYNC_WR_MPEG_REG(DI_BLEND_REG1_Y,
		((vsize_post>>2)<<16) | (2*(vsize_post>>2)-1));
	DI_VSYNC_WR_MPEG_REG(DI_BLEND_REG2_Y,
		((2*(vsize_post>>2))<<16) | (3*(vsize_post>>2)-1));
	DI_VSYNC_WR_MPEG_REG(DI_BLEND_REG3_Y,
		((3*(vsize_post>>2))<<16) | (vsize_post-1));
	DI_VSYNC_WR_MPEG_REG(DI_BLEND_REG0_X, (hsize_post-1));
	DI_VSYNC_WR_MPEG_REG(DI_BLEND_REG1_X, (hsize_post-1));
	DI_VSYNC_WR_MPEG_REG(DI_BLEND_REG2_X, (hsize_post-1));
	DI_VSYNC_WR_MPEG_REG(DI_BLEND_REG3_X, (hsize_post-1));

	DI_VSYNC_WR_MPEG_REG(DI_POST_CTRL, (0 << 0) |
					  (0 << 1)	|
					  (0 << 2)	|
					  (0 << 3)	|
					  (0 << 4)	|
					  (0 << 5)	|
					  (0 << 6)	|
					  (0 << 7)	|
					  (1 << 8)	|
					  (0 << 9)	|
					  (0 << 10) |
					  (0 << 11) |
					  (0 << 12) |
					  (hold_line << 16) |
					  (0 << 29) |
					  (0x3 << 30)
		);
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXBB) {
		/* enable ma,disable if0 to vpp */
		if ((VSYNC_RD_MPEG_REG(VIU_MISC_CTRL0) & 0x50000) != 0x50000) {
			DI_VSYNC_WR_MPEG_REG_BITS(VIU_MISC_CTRL0, 5, 16, 3);
			if (post_write_en)
				DI_VSYNC_WR_MPEG_REG_BITS(VIU_MISC_CTRL0,
					1, 28, 1);
		}
	}
}

static void post_bit_mode_config(unsigned char if0,
	unsigned char if1, unsigned char if2, unsigned char post_wr)
{
	if (!cpu_after_eq(MESON_CPU_MAJOR_ID_GXTVBB))
		return;

	DI_Wr_reg_bits(VD1_IF0_GEN_REG3, if0&0x3, 8, 2);
	DI_Wr_reg_bits(DI_IF1_GEN_REG3, if1&0x3, 8, 2);
	DI_Wr_reg_bits(DI_IF2_GEN_REG3, if2&0x3, 8, 2);
	DI_Wr_reg_bits(DI_DIWR_Y, post_wr&0x1, 14, 1);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL) && ((post_wr&0x3) == 0x3))
		DI_Wr_reg_bits(DI_DIWR_CTRL, 0x3, 22, 2);
}

static unsigned int pldn_ctrl_rflsh = 1;
module_param(pldn_ctrl_rflsh, uint, 0644);
MODULE_PARM_DESC(pldn_ctrl_rflsh, "/n post blend reflesh./n");

void di_post_switch_buffer(
	struct DI_MIF_s		   *di_buf0_mif,
	struct DI_MIF_s		   *di_buf1_mif,
	struct DI_MIF_s		   *di_buf2_mif,
	struct DI_SIM_MIF_s    *di_diwr_mif,
	struct DI_SIM_MIF_s    *di_mtnprd_mif,
	struct DI_MC_MIF_s	   *di_mcvecrd_mif,
	int ei_en, int blend_en, int blend_mtn_en, int blend_mode,
	int di_vpp_en, int di_ddr_en,
	int post_field_num, int hold_line, int urgent,
	int invert_mv, bool pd_enable, bool mc_enable,
	int vskip_cnt
)
{
	int ei_only, buf1_en;

	ei_only = ei_en && !blend_en && (di_vpp_en || di_ddr_en);
	buf1_en =  (!ei_only && (di_ddr_en || di_vpp_en));

	if ((VSYNC_RD_MPEG_REG(VIU_MISC_CTRL0) & 0x50000) != 0x50000)
		DI_VSYNC_WR_MPEG_REG_BITS(VIU_MISC_CTRL0, 5, 16, 3);
	if (di_ddr_en)
		DI_VSYNC_WR_MPEG_REG_BITS(VIU_MISC_CTRL0,
			1, 28, 1);

	if (ei_en || di_vpp_en || di_ddr_en)
		set_di_if0_mif(di_buf0_mif, urgent,
			hold_line, vskip_cnt, di_ddr_en);

	if (!ei_only && (di_ddr_en || di_vpp_en)) {
		DI_VSYNC_WR_MPEG_REG(DI_IF1_CANVAS0,
			(di_buf1_mif->canvas0_addr2 << 16) |
			(di_buf1_mif->canvas0_addr1 << 8) |
			(di_buf1_mif->canvas0_addr0 << 0));
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL))
			DI_VSYNC_WR_MPEG_REG(DI_IF2_CANVAS0,
				(di_buf2_mif->canvas0_addr2 << 16) |
				(di_buf2_mif->canvas0_addr1 << 8) |
				(di_buf2_mif->canvas0_addr0 << 0));
	}

	/* motion for current display field. */
	if (blend_mtn_en) {
		DI_VSYNC_WR_MPEG_REG(DI_MTNRD_CTRL,
(di_mtnprd_mif->canvas_num << 8) | (urgent << 16)
	 ); /* current field mtn canvas index. */

	}

	if (di_ddr_en) {
		DI_VSYNC_WR_MPEG_REG(DI_DIWR_CTRL,
			di_diwr_mif->canvas_num |
				(urgent << 16)	|
				(2 << 26)		|
				(di_ddr_en << 30));
		post_bit_mode_config(di_buf0_mif->bit_mode,
			di_buf1_mif->bit_mode,
			di_buf2_mif->bit_mode,
			di_diwr_mif->bit_mode);
	}
	if ((pldn_ctrl_rflsh == 1) && pd_enable) {
		DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_CTRL, blend_en, 31, 1);
		DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_CTRL, 7, 22, 3);
		DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_CTRL, blend_mode, 20, 2);
		if (debug_blend_mode_ctrl != 0xff)
			DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_CTRL,
				debug_blend_mode_ctrl&0x3, 20, 2);
	} else
		DI_VSYNC_WR_MPEG_REG(DI_BLEND_CTRL, Rd(DI_BLEND_CTRL)|
			(blend_en<<31) | (blend_mode<<20) | 0x1c0001f);

	if (mc_enable) {
		DI_VSYNC_WR_MPEG_REG(MCDI_MCVECRD_CTRL,
	(Rd(MCDI_MCVECRD_CTRL) & 0xffffff00) |
	(1<<9) |	  /* canvas enable */
	di_mcvecrd_mif->canvas_num |  /* canvas index. */
	(urgent << 8));
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXLX))
			DI_VSYNC_WR_MPEG_REG_BITS(MCDI_MC_CRTL,
			invert_mv, 17, 1);/* invert mv */
		DI_VSYNC_WR_MPEG_REG_BITS(MCDI_MC_CRTL,
			di_mcvecrd_mif->vecrd_offset, 12, 3);
		if (di_mcvecrd_mif->blend_en)
			DI_VSYNC_WR_MPEG_REG_BITS(MCDI_MC_CRTL,
				mcen_mode, 0, 2);
		else
			DI_VSYNC_WR_MPEG_REG_BITS(MCDI_MC_CRTL,
					0, 0, 2);
	}
	if (!is_meson_txlx_cpu())
		invert_mv = 0;

	DI_VSYNC_WR_MPEG_REG(DI_POST_CTRL,
		((ei_en|blend_en) << 0) | /* line buf 0 enable */
		((blend_mode == 1?1:0) << 1)  |
		(ei_en << 2) |			/* ei  enable */
		(blend_mtn_en << 3) |	/* mtn line buffer enable */
		(blend_mtn_en  << 4) |/* mtnp read mif enable */
		(blend_en << 5) |
		(1 << 6) |		/* di mux output enable */
		(di_ddr_en << 7) |/* di write to SDRAM enable.*/
		(di_vpp_en << 8) |/* di to VPP enable. */
		(0 << 9) |		/* mif0 to VPP enable. */
		(0 << 10) |		/* post drop first. */
		(0 << 11) |
		(di_vpp_en << 12) | /* post viu link */
		(invert_mv << 14) |
		(hold_line << 16) | /* post hold line number */
		(post_field_num << 29) |	/* post field number. */
		(0x3 << 30)	/* post soft rst  post frame rst. */
	);
	if (di_ddr_en && mc_enable)
		DI_VSYNC_WR_MPEG_REG_BITS(MCDI_MCVECRD_CTRL, 1, 9, 1);
}

void enable_di_post_2(
	struct DI_MIF_s		   *di_buf0_mif,
	struct DI_MIF_s		   *di_buf1_mif,
	struct DI_MIF_s		   *di_buf2_mif,
	struct DI_SIM_MIF_s    *di_diwr_mif,
	struct DI_SIM_MIF_s    *di_mtnprd_mif,
	int ei_en, int blend_en, int blend_mtn_en, int blend_mode,
	int di_vpp_en, int di_ddr_en, int post_field_num,
	int hold_line, int urgent, int invert_mv,
	int vskip_cnt
)
{
	int ei_only;
	int buf1_en;

	ei_only = ei_en && !blend_en && (di_vpp_en || di_ddr_en);
	buf1_en =  (!ei_only && (di_ddr_en || di_vpp_en));

	if (ei_en || di_vpp_en || di_ddr_en)
		set_di_if0_mif(di_buf0_mif, di_vpp_en,
			hold_line, vskip_cnt, di_ddr_en);

	/* if (!ei_only && (di_ddr_en || di_vpp_en)) */
		set_di_if1_mif(di_buf1_mif, di_vpp_en, hold_line, vskip_cnt);
		if (is_meson_txlx_cpu() ||
			is_meson_gxlx_cpu() ||
			is_meson_txhd_cpu())
			set_di_if2_mif(di_buf2_mif,
				di_vpp_en, hold_line, vskip_cnt);

	/* printk("%s: ei_only %d,buf1_en %d,ei_en %d,di_vpp_en %d,
	 * di_ddr_en %d,blend_mtn_en %d,blend_mode %d.\n",
	 * __func__,ei_only,buf1_en,ei_en,di_vpp_en,di_ddr_en,
	 * blend_mtn_en,blend_mode);
	 */
	/* motion for current display field. */
	DI_VSYNC_WR_MPEG_REG(DI_MTNPRD_X,
(di_mtnprd_mif->start_x << 16) | (di_mtnprd_mif->end_x));
	DI_VSYNC_WR_MPEG_REG(DI_MTNPRD_Y,
(di_mtnprd_mif->start_y << 16) | (di_mtnprd_mif->end_y));
	if (blend_mtn_en) {
		DI_VSYNC_WR_MPEG_REG(DI_MTNRD_CTRL,
(di_mtnprd_mif->canvas_num << 8) | (urgent << 16)
	 ); /* current field mtn canvas index */
	}

	if (di_ddr_en) {
		DI_VSYNC_WR_MPEG_REG(DI_DIWR_X,
(di_diwr_mif->start_x << 16) | (di_diwr_mif->end_x));
		DI_VSYNC_WR_MPEG_REG(DI_DIWR_Y, (3 << 30) |
(di_diwr_mif->start_y << 16) | (di_diwr_mif->end_y));
		DI_VSYNC_WR_MPEG_REG(DI_DIWR_CTRL,
			di_diwr_mif->canvas_num|
			(urgent << 16) |
			(2 << 26) |
			(di_ddr_en << 30));
		post_bit_mode_config(di_buf0_mif->bit_mode,
			di_buf1_mif->bit_mode,
			di_buf2_mif->bit_mode,
			di_diwr_mif->bit_mode);
	}

	DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_CTRL, 7, 22, 3);
	DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_CTRL,
		blend_en&0x1, 31, 1);
	DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_CTRL,
		blend_mode&0x3, 20, 2);
	if (debug_blend_mode_ctrl != 0xff)
		DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_CTRL,
			debug_blend_mode_ctrl&0x3, 20, 2);
	if (!is_meson_txlx_cpu())
		invert_mv = 0;

	DI_VSYNC_WR_MPEG_REG(DI_POST_CTRL,
((ei_en | blend_en) << 0) |	/* line buffer 0 enable */
((blend_mode == 1?1:0) << 1)  |
(ei_en << 2) | /* ei  enable */
(blend_mtn_en << 3) |	/* mtn line buffer enable */
(blend_mtn_en  << 4) |/* mtnp read mif enable */
(blend_en << 5) |
(1 << 6) |/* di mux output enable */
(di_ddr_en << 7) |	/* di write to SDRAM enable. */
(di_vpp_en << 8) |	/* di to VPP enable. */
(0 << 9) |	/* mif0 to VPP enable. */
(0 << 10) |	/* post drop first. */
(0 << 11) |
(di_vpp_en << 12) |	/* post viu link */
(invert_mv << 14) |	/* invert mv */
(hold_line << 16) |	/* post hold line number */
(post_field_num << 29) |	/* post field number. */
(0x3 << 30)
/* post soft rst  post frame rst. */
		);
}

void disable_post_deinterlace_2(void)
{
	DI_VSYNC_WR_MPEG_REG(DI_POST_CTRL, 0x3 << 30);
	DI_VSYNC_WR_MPEG_REG(DI_POST_SIZE, (32-1) | ((128-1) << 16));
	DI_VSYNC_WR_MPEG_REG(DI_IF1_GEN_REG, 0x3 << 30);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL))
		DI_VSYNC_WR_MPEG_REG(DI_IF2_GEN_REG, 0x3 << 30);
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXBB) {
		/* disable ma,enable if0 to vpp,enable afbc to vpp */
		if ((VSYNC_RD_MPEG_REG(VIU_MISC_CTRL0) & 0x50000) != 0)
			DI_VSYNC_WR_MPEG_REG_BITS(VIU_MISC_CTRL0, 0, 16, 4);
		/* DI inp(current data) switch to memory */
		DI_VSYNC_WR_MPEG_REG_BITS(VIUB_MISC_CTRL0, 0, 16, 1);
	}
	/* DI_VSYNC_WR_MPEG_REG(DI_IF1_GEN_REG,
	 * Rd(DI_IF1_GEN_REG) & 0xfffffffe);
	 */
}

void enable_di_post_mif(enum gate_mode_e mode)
{
	unsigned char gate = 0;

	switch (mode) {
	case GATE_OFF:
		gate = 1;
		break;
	case GATE_ON:
		gate = 2;
		break;
	case GATE_AUTO:
		gate = 0;
		break;
	default:
		gate = 0;
	}
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX)) {
		/* enable if1 external gate freerun hw issue */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL1, ((gate == 0)?2:gate), 2, 2);
		/* enable if2 external gate freerun hw issue */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL1, ((gate == 0)?2:gate), 4, 2);
		/* enable di wr external gate */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL1, gate, 6, 2);
		/* enable mtn rd external gate */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL1, gate, 8, 2);
		/* enable mv rd external gate */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL1, gate, 10, 2);
	}
}

void di_hw_disable(bool mc_enable)
{
	enable_di_pre_mif(false, mc_enable);
	DI_Wr(DI_POST_SIZE, (32-1) | ((128-1) << 16));
	DI_Wr_reg_bits(DI_IF1_GEN_REG, 0, 0, 1);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL))
		DI_Wr_reg_bits(DI_IF2_GEN_REG, 0, 0, 1);
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXBB) {
		/* disable ma,enable if0 to vpp,enable afbc to vpp */
		if (Rd_reg_bits(VIU_MISC_CTRL0, 16, 4) != 0)
			DI_Wr_reg_bits(VIU_MISC_CTRL0, 0, 16, 4);
		/* DI inp(current data) switch to memory */
		DI_Wr_reg_bits(VIUB_MISC_CTRL0, 0, 16, 1);
	}
}
/*
 * old pulldown windows share below ctrl
 * registers
 * with new pulldown windows
 */
void film_mode_win_config(unsigned int width, unsigned int height)
{
	unsigned int win0_start_x, win0_end_x, win0_start_y, win0_end_y;
	unsigned int win1_start_x, win1_end_x, win1_start_y, win1_end_y;
	unsigned int win2_start_x, win2_end_x, win2_start_y, win2_end_y;
	unsigned int win3_start_x, win3_end_x, win3_start_y, win3_end_y;
	unsigned int win4_start_x, win4_end_x, win4_start_y, win4_end_y;

	win0_start_x = win1_start_x = win2_start_x = 0;
	win3_start_x = win4_start_x = 0;
	win0_end_x = win1_end_x = win2_end_x = width-1;
	win3_end_x = win4_end_x = width-1;
	win0_start_y = 0;
	win1_start_y = (height>>3); /* 1/8 */
	win0_end_y = win1_start_y - 1;
	win2_start_y = win1_start_y + (height>>2); /* 1/4 */
	win1_end_y = win2_start_y - 1;
	win3_start_y = win2_start_y + (height>>2); /* 1/4 */
	win2_end_y = win3_start_y - 1;
	win4_start_y = win3_start_y + (height>>2); /* 1/4 */
	win3_end_y = win4_start_y - 1;
	win4_end_y = win4_start_y + (height>>3) - 1; /* 1/8 */

	RDMA_WR(DI_MC_REG0_X, (win0_start_x << 16) | win0_end_x);
	RDMA_WR(DI_MC_REG0_Y, (win0_start_y << 16) | win0_end_y);
	RDMA_WR(DI_MC_REG1_X, (win1_start_x << 16) | win1_end_x);
	RDMA_WR(DI_MC_REG1_Y, (win1_start_y << 16) | win1_end_y);
	RDMA_WR(DI_MC_REG2_X, (win2_start_x << 16) | win2_end_x);
	RDMA_WR(DI_MC_REG2_Y, (win2_start_y << 16) | win2_end_y);
	RDMA_WR(DI_MC_REG3_X, (win3_start_x << 16) | win3_end_x);
	RDMA_WR(DI_MC_REG3_Y, (win3_start_y << 16) | win3_end_y);
	RDMA_WR(DI_MC_REG4_X, (win4_start_x << 16) | win4_end_x);
	RDMA_WR(DI_MC_REG4_Y, (win4_start_y << 16) | win4_end_y);
}

/*
 * old pulldown detction module, global field diff/num & frame
 * diff/numm and 5 window included
 */
void read_pulldown_info(unsigned int *glb_frm_mot_num,
	unsigned int *glb_fid_mot_num)
{
	/*
	 * addr will increase by 1 automatically
	 */
	DI_Wr(DI_INFO_ADDR, 1);
	*glb_frm_mot_num = Rd(DI_INFO_DATA);
	DI_Wr(DI_INFO_ADDR, 4);
	*glb_fid_mot_num = Rd(DI_INFO_DATA);
}

void read_new_pulldown_info(struct FlmModReg_t *pFMReg)
{
	int i = 0;

	for (i = 0; i < 6; i++) {
		pFMReg->rROFrmDif02[i] = Rd(DIPD_RO_COMB_0+i);
		pFMReg->rROFldDif01[i] = Rd(DIPD_RO_COMB_6+i);
	}

	/* pFMReg->rROFrmDif02[0] = Rd(DIPD_RO_COMB_0); */
	/* pFMReg->rROFldDif01[0] = Rd(DIPD_RO_COMB_6); */

	for (i = 0; i < 9; i++)
		pFMReg->rROCmbInf[i] = Rd(DIPD_RO_COMB_12+i);
}

void di_post_read_reverse(bool reverse, bool mc_enable)
{
	if (reverse) {
		DI_Wr_reg_bits(DI_IF1_GEN_REG2, 3, 2, 2);
		DI_Wr_reg_bits(VD1_IF0_GEN_REG2, 0xf, 2, 4);
		DI_Wr_reg_bits(VD2_IF0_GEN_REG2, 0xf, 2, 4);
		if (mc_enable) {
			/* motion vector read reverse*/
			DI_Wr_reg_bits(MCDI_MCVECRD_X, 1, 30, 1);
			DI_Wr_reg_bits(MCDI_MCVECRD_Y, 1, 30, 1);
			DI_Wr_reg_bits(MCDI_MC_CRTL, 0, 8, 1);
		}
	} else {
		DI_Wr_reg_bits(DI_IF1_GEN_REG2,  0, 2, 2);
		DI_Wr_reg_bits(VD1_IF0_GEN_REG2, 0, 2, 4);
		DI_Wr_reg_bits(VD2_IF0_GEN_REG2, 0, 2, 4);
		if (mc_enable) {
			DI_Wr_reg_bits(MCDI_MCVECRD_X, 0, 30, 1);
			DI_Wr_reg_bits(MCDI_MCVECRD_Y, 0, 30, 1);
			DI_Wr_reg_bits(MCDI_MC_CRTL, 1, 8, 1);
		}
	}
}

static bool if2_disable;
module_param_named(if2_disable, if2_disable, bool, 0644);

static unsigned short pre_flag = 2;
module_param_named(pre_flag, pre_flag, ushort, 0644);
void di_post_read_reverse_irq(bool reverse, unsigned char mc_pre_flag,
	bool mc_enable)
{
	unsigned short flag_val = 1;

	mc_pre_flag = if2_disable?1:mc_pre_flag;
	if (reverse) {
		DI_VSYNC_WR_MPEG_REG_BITS(DI_IF1_GEN_REG2,    3, 2, 2);
		DI_VSYNC_WR_MPEG_REG_BITS(VD1_IF0_GEN_REG2, 0xf, 2, 4);
		DI_VSYNC_WR_MPEG_REG_BITS(VD2_IF0_GEN_REG2, 0xf, 2, 4);
		DI_VSYNC_WR_MPEG_REG_BITS(DI_MTNRD_CTRL, 0xf, 17, 4);
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL))
			DI_VSYNC_WR_MPEG_REG_BITS(DI_IF2_GEN_REG2,  3, 2, 2);
		if (mc_enable) {
			/* motion vector read reverse*/
			DI_VSYNC_WR_MPEG_REG_BITS(MCDI_MCVECRD_X, 1, 30, 1);
			DI_VSYNC_WR_MPEG_REG_BITS(MCDI_MCVECRD_Y, 1, 30, 1);
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL)) {
				if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX)) {
					DI_VSYNC_WR_MPEG_REG_BITS(MCDI_MC_CRTL,
						pre_flag, 8, 2);
					flag_val = (pre_flag != 2) ? 0 : 1;
				} else {
					DI_VSYNC_WR_MPEG_REG_BITS(MCDI_MC_CRTL,
						mc_pre_flag, 8, 2);
					flag_val = (mc_pre_flag != 2) ? 0 : 1;
				}
				DI_VSYNC_WR_MPEG_REG_BITS(
					MCDI_MC_CRTL, flag_val, 11, 1);
				/* disable if2 for wave if1 case,
				 *disable mc for pq issue
				 */
				if (if2_disable) {
					DI_VSYNC_WR_MPEG_REG_BITS(
						MCDI_MC_CRTL, 0, 11, 1);
					DI_VSYNC_WR_MPEG_REG_BITS(
						DI_IF2_GEN_REG, 0, 0, 1);
					if (cpu_after_eq(
						MESON_CPU_MAJOR_ID_GXLX))
						DI_VSYNC_WR_MPEG_REG_BITS(
							MCDI_MC_CRTL, 0, 18, 1);
				}
			} else
				DI_VSYNC_WR_MPEG_REG_BITS(MCDI_MC_CRTL,
					mc_pre_flag, 8, 1);
		}
	} else {
		DI_VSYNC_WR_MPEG_REG_BITS(DI_IF1_GEN_REG2,  0, 2, 2);
		DI_VSYNC_WR_MPEG_REG_BITS(VD1_IF0_GEN_REG2, 0, 2, 4);
		DI_VSYNC_WR_MPEG_REG_BITS(VD2_IF0_GEN_REG2, 0, 2, 4);
		DI_VSYNC_WR_MPEG_REG_BITS(DI_MTNRD_CTRL, 0, 17, 4);
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL))
			DI_VSYNC_WR_MPEG_REG_BITS(DI_IF2_GEN_REG2, 0, 2, 2);
		if (mc_enable) {
			DI_VSYNC_WR_MPEG_REG_BITS(MCDI_MCVECRD_X, 0, 30, 1);
			DI_VSYNC_WR_MPEG_REG_BITS(MCDI_MCVECRD_Y, 0, 30, 1);
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL)) {
				if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX)) {
					DI_VSYNC_WR_MPEG_REG_BITS(MCDI_MC_CRTL,
						pre_flag, 8, 2);
					flag_val = (pre_flag != 2) ? 0 : 1;
				} else {
					DI_VSYNC_WR_MPEG_REG_BITS(MCDI_MC_CRTL,
						mc_pre_flag, 8, 2);
					flag_val = (mc_pre_flag != 2) ? 0 : 1;
				}
				DI_VSYNC_WR_MPEG_REG_BITS(
					MCDI_MC_CRTL, flag_val, 11, 1);
				/* disable if2 for wave if1 case */
				if (if2_disable) {
					DI_VSYNC_WR_MPEG_REG_BITS(
						MCDI_MC_CRTL, 0, 11, 1);
					DI_VSYNC_WR_MPEG_REG_BITS(
						DI_IF2_GEN_REG, 0, 0, 1);
					if (cpu_after_eq(
						MESON_CPU_MAJOR_ID_GXLX))
						DI_VSYNC_WR_MPEG_REG_BITS(
							MCDI_MC_CRTL, 0, 18, 1);
				}
			} else
				DI_VSYNC_WR_MPEG_REG_BITS(MCDI_MC_CRTL,
					mc_pre_flag, 8, 1);
		}
	}
}

void diwr_set_power_control(unsigned char enable)
{
	switch_vpu_mem_pd_vmod(VPU_VIU_VD1,
		enable?VPU_MEM_POWER_ON:VPU_MEM_POWER_DOWN);
	switch_vpu_mem_pd_vmod(VPU_DI_POST,
		enable?VPU_MEM_POWER_ON:VPU_MEM_POWER_DOWN);
}

void di_top_gate_control(bool top_en, bool mc_en)
{
	if (top_en) {
		/* enable clkb input */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL0, 1, 0, 1);
		DI_Wr_reg_bits(VIUB_GCLK_CTRL0, 1, 15, 1);
		/* enable slow clk */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL0, mc_en?1:0, 10, 1);
		/* enable di arb */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL1, 0, 0, 2);
	} else {
		/* disable clkb input */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL0, 0, 0, 1);
		DI_Wr_reg_bits(VIUB_GCLK_CTRL0, 0, 15, 1);
		/* disable slow clk */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL0, 0, 10, 1);
		/* disable di arb */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL1, 1, 0, 2);
	}
}

void di_pre_gate_control(bool gate, bool mc_enable)
{
	if (gate) {
		/* enable ma pre clk */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL0, 1, 8, 1);
		/* enable mc clk */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL0, 1, 11, 1);
		/* enable pd clk gate */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL2, 0, 2, 2);
		/* enable motion clk gate */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL2, 0, 4, 2);
		/* enable deband clk gate freerun for hw issue */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL2, 2, 6, 2);
		/* enable input mif external gate */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL1, 0, 16, 2);
		/* enable mem mif external gate */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL1, 0, 18, 2);
		/* enable chan2 mif external gate */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL1, 0, 20, 2);
		/* enable nr wr mif external gate */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL1, 0, 22, 2);
		/* enable mtn wr mif external gate */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL1, 0, 24, 2);
		if (mc_enable) {
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXHD))
				DI_Wr_reg_bits(VIUB_GCLK_CTRL2, 0, 12, 2);
			else
				/* enable me clk always run vlsi issue */
				DI_Wr_reg_bits(VIUB_GCLK_CTRL2, 3, 12, 2);
			/*
			 * enable mc pre mv(wr) mcinfo w/r
			 * mif external gate
			 */
			DI_Wr_reg_bits(VIUB_GCLK_CTRL1,
					0, 26, 2);
		}
		/* cowork with auto gate to config reg */
		DI_Wr_reg_bits(DI_PRE_CTRL, 3, 2, 2);
	} else {
		/* disable ma pre clk */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL0, 0, 8, 1);
		/* disable mc clk */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL0, 0, 11, 1);
		/* disable pd clk gate */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL2, 1, 2, 2);
		/* disable motion clk gate */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL2, 1, 4, 2);
		/* disable deband clk gate freerun for hw issue */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL2, 1, 6, 2);
		/* disable input mif external gate */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL1, 1, 16, 2);
		/* disable mem mif external gate */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL1, 1, 18, 2);
		/* disable chan2 mif external gate */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL1, 1, 20, 2);
		/* disable nr wr mif external gate */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL1, 1, 22, 2);
		/* disable mtn wr mif external gate */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL1, 1, 24, 2);
		if (mc_enable) {
			/* disable mc pre mv(wr) mcinfo
			 * w/r mif external gate
			 */
			DI_Wr_reg_bits(VIUB_GCLK_CTRL1,
					1, 26, 2);
			/* disable me clk gate */
			DI_Wr_reg_bits(VIUB_GCLK_CTRL2, 1, 12, 2);
		}
	}

}
void di_post_gate_control(bool gate)
{
	if (gate) {
		/* enable clk post div */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL0, 1, 12, 1);
		/* enable post line buf/fifo/mux clk */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL0, 1, 9, 1);
		/* enable blend1 clk gate */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL3, 0, 0, 2);
		/* enable ei clk gate */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL3, 0, 2, 2);
		/* enable ei_0 clk gate */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL3, 0, 4, 2);
	} else {
		/* disable clk post div */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL0, 0, 12, 1);
		/* disable post line buf/fifo/mux clk */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL0, 0, 9, 1);
		/* disable blend1 clk gate */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL3, 1, 0, 2);
		/* disable ei clk gate */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL3, 1, 2, 2);
		/* disable ei_0 clk gate */
		DI_Wr_reg_bits(VIUB_GCLK_CTRL3, 1, 4, 2);
	}

}
static bool pre_mif_gate;
void enable_di_pre_mif(bool en, bool mc_enable)
{
	if (en) {
		/* enable input mif*/
		DI_Wr(DI_CHAN2_GEN_REG, Rd(DI_CHAN2_GEN_REG) | 0x1);
		DI_Wr(DI_MEM_GEN_REG, Rd(DI_MEM_GEN_REG) | 0x1);
		DI_Wr(DI_INP_GEN_REG, Rd(DI_INP_GEN_REG) | 0x1);
		/* nrwr no clk gate en=0 */
		RDMA_WR_BITS(DI_NRWR_CTRL, 0, 24, 1);
		if (mc_enable) {
			/* gate clk */
			RDMA_WR_BITS(MCDI_MCVECWR_CTRL, 0, 9, 1);
			/* gate clk */
			RDMA_WR_BITS(MCDI_MCINFOWR_CTRL, 0, 9, 1);
			/* mcinfo rd req en =1 */
			RDMA_WR_BITS(MCDI_MCINFORD_CTRL, 1, 9, 1);
			/* mv wr req en =1 */
			RDMA_WR_BITS(MCDI_MCVECWR_CTRL, 1, 12, 1);
			/* mcinfo wr req en =1 */
			RDMA_WR_BITS(MCDI_MCINFOWR_CTRL, 1, 12, 1);
		}
		/* enable di nr/mtn/mv mif */
		/* RDMA_WR(VPU_WRARB_REQEN_SLV_L1C1, 0x3f); */
	} else {
		if (pre_mif_gate)
			return;
		/* nrwr no clk gate en=1 */
		RDMA_WR_BITS(DI_NRWR_CTRL, 1, 24, 1);
		/* nr wr req en =0 */
		RDMA_WR_BITS(DI_PRE_CTRL, 0, 0, 1);
		/* mtn wr req en =0 */
		RDMA_WR_BITS(DI_PRE_CTRL, 0, 1, 1);
		/* cont wr req en =0 */
		RDMA_WR_BITS(DI_MTN_1_CTRL1, 0, 31, 1);
		if (mc_enable) {
			/* no gate clk */
			RDMA_WR_BITS(MCDI_MCVECWR_CTRL, 1, 9, 1);
			/* no gate clk */
			RDMA_WR_BITS(MCDI_MCINFOWR_CTRL, 1, 9, 1);
			/* mcvec wr req en =0 */
			RDMA_WR_BITS(MCDI_MCVECWR_CTRL, 0, 12, 1);
			/* mcinfo wr req en =0 */
			RDMA_WR_BITS(MCDI_MCINFOWR_CTRL, 0, 12, 1);
			/* mcinfo rd req en = 0 */
			RDMA_WR_BITS(MCDI_MCINFORD_CTRL, 0, 9, 1);
		}
		/* disable nr cont mtn mv minfo mif */
		/* RDMA_WR(VPU_WRARB_REQEN_SLV_L1C1, 0x2b); */
		/* disable cont rd */
		DI_Wr(DI_PRE_CTRL, Rd(DI_PRE_CTRL) & ~(1 << 25));
		/* disable input mif*/
		DI_Wr(DI_CHAN2_GEN_REG, Rd(DI_CHAN2_GEN_REG) & ~0x1);
		DI_Wr(DI_MEM_GEN_REG, Rd(DI_MEM_GEN_REG) & ~0x1);
		DI_Wr(DI_INP_GEN_REG, Rd(DI_INP_GEN_REG) & ~0x1);
	}
}

void combing_pd22_window_config(unsigned int width, unsigned int height)
{
	unsigned short y1 = 39, y2 = height - 41;

	if (height >= 540) {
		y1 = 79;
		y2 = height - 81;
	}
	if (!cpu_after_eq(MESON_CPU_MAJOR_ID_GXLX)) {
		DI_Wr_reg_bits(DECOMB_WIND00, 0, 16, 13);/* dcomb x0 */
		DI_Wr_reg_bits(DECOMB_WIND00, (width-1), 0, 13);/* dcomb x1 */
		DI_Wr_reg_bits(DECOMB_WIND01, 0, 16, 13);/* dcomb y0 */
		DI_Wr_reg_bits(DECOMB_WIND01, y1, 0, 13);/* dcomb y1 */
		DI_Wr_reg_bits(DECOMB_WIND10, 0, 16, 13);/* dcomb x0 */
		DI_Wr_reg_bits(DECOMB_WIND10, (width-1), 0, 13);/* dcomb x1 */
		DI_Wr_reg_bits(DECOMB_WIND11, (y1+1), 16, 13);/* dcomb y0 */
		DI_Wr_reg_bits(DECOMB_WIND11, y2, 0, 13);/* dcomb y1 */
	}
	DI_Wr_reg_bits(MCDI_PD_22_CHK_WND0_X, 0, 0, 13);/* pd22 x0 */
	DI_Wr_reg_bits(MCDI_PD_22_CHK_WND0_X, (width-1), 16, 13);/* pd22 x1 */
	DI_Wr_reg_bits(MCDI_PD_22_CHK_WND0_Y, 0, 0, 13);/* pd22 y0 */
	DI_Wr_reg_bits(MCDI_PD_22_CHK_WND0_Y, y1, 16, 13);/* pd y1 */

	DI_Wr_reg_bits(MCDI_PD_22_CHK_WND1_X, 0, 0, 13);/* pd x0 */
	DI_Wr_reg_bits(MCDI_PD_22_CHK_WND1_X, (width-1), 16, 13);/* pd x1 */
	DI_Wr_reg_bits(MCDI_PD_22_CHK_WND1_Y, (y1+1), 0, 13);/* pd y0 */
	DI_Wr_reg_bits(MCDI_PD_22_CHK_WND1_Y, y2, 16, 13);/* pd y2 */
}

void pulldown_vof_win_config(struct pulldown_detected_s *wins)
{

	DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_REG0_Y,
		wins->regs[0].win_vs, 17, 12);
	DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_REG0_Y,
		wins->regs[0].win_ve, 1, 12);

	DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_REG1_Y,
		wins->regs[1].win_vs, 17, 12);
	DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_REG1_Y,
		wins->regs[1].win_ve, 1, 12);

	DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_REG2_Y,
		wins->regs[2].win_vs, 17, 12);
	DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_REG2_Y,
		wins->regs[2].win_ve, 1, 12);

	DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_REG3_Y,
		wins->regs[3].win_vs, 17, 12);
	DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_REG3_Y,
		wins->regs[3].win_ve, 1, 12);

	DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_CTRL,
		(wins->regs[0].win_ve > wins->regs[0].win_vs)
		? 1 : 0, 16, 1);
	DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_CTRL,
		wins->regs[0].blend_mode, 8, 2);

	DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_CTRL,
		(wins->regs[1].win_ve > wins->regs[1].win_vs)
		? 1 : 0, 17, 1);
	DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_CTRL,
		wins->regs[1].blend_mode, 10, 2);

	DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_CTRL,
		(wins->regs[2].win_ve > wins->regs[2].win_vs)
		? 1 : 0, 18, 1);
	DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_CTRL,
		wins->regs[2].blend_mode, 12, 2);

	DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_CTRL,
		(wins->regs[3].win_ve > wins->regs[3].win_vs)
		? 1 : 0, 19, 1);
	DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_CTRL,
		wins->regs[3].blend_mode, 14, 2);
}

void di_load_regs(struct di_pq_parm_s *di_pq_ptr)
{
	unsigned int i = 0, addr = 0, value = 0, mask = 0, len;
	struct am_reg_s *regs_p;

	if (pq_load_dbg == 1)
		return;
	if (pq_load_dbg == 2)
		pr_info("[DI]%s hw load 0x%x pq table len %u.\n",
			__func__, di_pq_ptr->pq_parm.table_name,
			di_pq_ptr->pq_parm.table_len);
	if (PTR_RET(di_pq_ptr->regs)) {
		pr_err("[DI] table ptr error.\n");
		return;
	}
	regs_p = (struct am_reg_s *)di_pq_ptr->regs;
	len = di_pq_ptr->pq_parm.table_len;
	for (i = 0; i < len; i++) {
		addr = regs_p->addr;
		value = regs_p->val;
		mask = regs_p->mask;
		if (pq_load_dbg == 2)
			pr_info("[%u][0x%x] = [0x%x]&[0x%x]\n",
				i, addr, value, mask);
		if (regs_p->mask != 0xffffffff) {
			value = ((Rd(addr) & (~(mask))) |
				(value & mask));
		}
		regs_p++;
		DI_Wr(addr, value);
		if (pq_load_dbg == 2)
			pr_info("[%u][0x%x] = [0x%x]\n", i, addr, value);
	}
}

#ifdef DEBUG_SUPPORT
module_param_named(pre_mif_gate, pre_mif_gate, bool, 0644);
#endif
