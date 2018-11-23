/*
 * drivers/amlogic/media/vin/tvin/tvafe/tvafe_cvd.c
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

/******************************Includes************************************/
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <asm/div64.h>
#include <linux/module.h>

/*#include <mach/am_regs.h>*/

#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#include "../tvin_global.h"
#include "../tvin_format_table.h"
#include "tvafe_regs.h"
#include "tvafe_cvd.h"
#include "tvafe_debug.h"
#include "tvafe_general.h"
#include "../vdin/vdin_regs.h"
#include "../vdin/vdin_ctl.h"
/***************************Local defines**********************************/

#define TVAFE_CVD2_SHIFT_CNT                6
/* cnt*10ms,delay for fmt shift counter */

#define TVAFE_CVD2_NONSTD_DGAIN_MAX         0x500
/* CVD max digital gain for non-std signal */
#define TVAFE_CVD2_NONSTD_CNT_MAX           0x0A
/* CVD max cnt non-std signal */

/* cvd2 function in vsync */
#define CVD_3D_COMB_CHECK_MAX_CNT           100
/* cnt*vs_interval,3D comb error check delay */

#define TVAFE_CVD2_CDTO_ADJ_TH              0x4CEDB3
/* CVD cdto adjust threshold */
#define TVAFE_CVD2_CDTO_ADJ_STEP            3
/* CVD cdto adjust step cdto/2^n */

#define PAL_BURST_MAG_UPPER_LIMIT           0x1C80
#define PAL_BURST_MAG_LOWER_LIMIT           0x1980

#define NTSC_BURST_MAG_UPPER_LIMIT          0x1580
#define NTSC_BURST_MAG_LOWER_LIMIT          0x1280

#define SYNC_HEIGHT_LOWER_LIMIT             0x60
#define SYNC_HEIGHT_UPPER_LIMIT             0xFF

#define SYNC_HEIGHT_ADJ_CNT                 0x2

/* cvd2 function in 10ms timer */
#define TVAFE_CVD2_NORMAL_REG_CHECK_CNT     100  /* n*10ms */

/* zhuangwei, nonstd flag */
#define CVD2_NONSTD_CNT_INC_STEP 1
#define CVD2_NONSTD_CNT_DEC_STEP 1
#define CVD2_NONSTD_CNT_INC_LIMIT 50
#define CVD2_NONSTD_CNT_DEC_LIMIT 1
/* should be large than CVD2_NONSTD_CNT_DEC_STEP */
#define CVD2_NONSTD_FLAG_ON_TH 10
#define CVD2_NONSTD_FLAG_OFF_TH 2

#define SCENE_COLORFUL_TH 0x80000

/* wait for signal stable */
#define FMT_WAIT_CNT 15
#define NTSC_SW_MAXCNT 20
#define NTSC_SW_MIDCNT 40

/*threshold for 4xx or 3xx valid*/
#define CNT_VLD_TH 0x30

#define CVD_REG07_PAL 0x03
#define SYNC_SENSITIVITY true
#define NOISE_JUDGE false
#define PGA_DEFAULT_VAL 0x20
#define TRY_FORMAT_MAX 5

/*0:NORMAL  1:a little sharper 2:sharper 3:even sharper*/
#define CVD2_FILTER_CONFIG_LEVEL 0

/********Local variables*********************************/
static const unsigned int cvd_mem_4f_length[TVIN_SIG_FMT_CVBS_SECAM-
			TVIN_SIG_FMT_CVBS_NTSC_M+1] = {

	0x0000e946, /* TVIN_SIG_FMT_CVBS_NTSC_M, */
	0x0000e946, /* TVIN_SIG_FMT_CVBS_NTSC_443, */
	0x00015a60, /* TVIN_SIG_FMT_CVBS_PAL_I, */
	0x0000e905, /* TVIN_SIG_FMT_CVBS_PAL_M, */
	0x00015a60, /* TVIN_SIG_FMT_CVBS_PAL_60, */
	0x000117d9, /* TVIN_SIG_FMT_CVBS_PAL_CN, */
	0x00015a60, /* TVIN_SIG_FMT_CVBS_SECAM, */
};

static int cnt_dbg_en;
static int force_fmt_flag;
static unsigned int scene_colorful = 1;
static int scene_colorful_old;
static int auto_de_en = 1;
static int lock_cnt;
static unsigned int cvd_reg8a = 0xa;
static int auto_vs_en = 1;
static bool ntsc50_en;

module_param(auto_vs_en, int, 0664);
MODULE_PARM_DESC(auto_vs_en, "auto_vs_en\n");

module_param(auto_de_en, int, 0664);
MODULE_PARM_DESC(auto_de_en, "auto_de_en\n");

module_param(cnt_dbg_en, int, 0664);
MODULE_PARM_DESC(cnt_dbg_en, "cnt_dbg_en\n");

static int cdto_adj_th = TVAFE_CVD2_CDTO_ADJ_TH;
module_param(cdto_adj_th, int, 0664);
MODULE_PARM_DESC(cdto_adj_th, "cvd2_adj_diff_threshold");

static int cdto_adj_step = TVAFE_CVD2_CDTO_ADJ_STEP;
module_param(cdto_adj_step, int, 0664);
MODULE_PARM_DESC(cdto_adj_step, "cvd2_adj_step");

static bool cvd_dbg_en;
module_param(cvd_dbg_en, bool, 0664);
MODULE_PARM_DESC(cvd_dbg_en, "cvd2 debug enable");

static bool cvd_nonstd_dbg_en;
module_param(cvd_nonstd_dbg_en, bool, 0664);
MODULE_PARM_DESC(cvd_nonstd_dbg_en, "cvd2 nonstd debug enable");

static int cvd2_shift_cnt = TVAFE_CVD2_SHIFT_CNT;
module_param(cvd2_shift_cnt, int, 0664);
MODULE_PARM_DESC(cvd2_shift_cnt, "cvd2_shift_cnt");

/*force the fmt for chrome off,for example ntsc pal_i 12*/
static unsigned int config_force_fmt;
module_param(config_force_fmt, uint, 0664);
MODULE_PARM_DESC(config_force_fmt,
		"after try TRY_FORMAT_MAX times ,we will force one fmt");

/*0:normal 1:force nonstandard configure*/
/*2:force don't nonstandard configure*/
static unsigned int  force_nostd = 2;
module_param(force_nostd, uint, 0644);
MODULE_PARM_DESC(force_nostd,
			"fixed nosig problem by removing the nostd config.\n");

/*0x001:enable cdto adj 0x010:enable 3d adj 0x100:enable pga;*/
/*0x1000:enable hs adj,which can instead cdto*/
static unsigned int  cvd_isr_en = 0x1110;
module_param(cvd_isr_en, uint, 0644);
MODULE_PARM_DESC(cvd_isr_en, "cvd_isr_en\n");

static int ignore_pal_nt;
module_param(ignore_pal_nt, int, 0644);
MODULE_PARM_DESC(ignore_pal_nt, "ignore_pal_nt\n");

static int ignore_443_358;
module_param(ignore_443_358, int, 0644);
MODULE_PARM_DESC(ignore_443_358, "ignore_443_358\n");

static unsigned int acd_h_config = 0x8e035e;
module_param(acd_h_config, uint, 0664);
MODULE_PARM_DESC(acd_h_config, "acd_h_config");

/*0x890359 is default setting for pal*/
static unsigned int acd_h = 0x890359;
module_param(acd_h, uint, 0664);
MODULE_PARM_DESC(acd_h, "acd_h");
static unsigned int acd_h_back = 0x890359;

static unsigned int hs_adj_th_level0 = 0x260;
static unsigned int hs_adj_th_level1 = 0x4f0;
static unsigned int hs_adj_th_level2 = 0x770;
static unsigned int hs_adj_th_level3 = 0x9e0;
static unsigned int hs_adj_th_level4 = 0xc50;
/*0.5hz*/
static unsigned int vs_adj_th_level0 = 0xfa;
/*1.5hz*/
static unsigned int vs_adj_th_level1 = 0xee;
/*2.5hz*/
static unsigned int vs_adj_th_level2 = 0xe2;
/*3hz*/
static unsigned int vs_adj_th_level3 = 0xdc;
/*3.5hz*/
static unsigned int vs_adj_th_level4 = 0xd8;
/*-0.5hz*/
static unsigned int vs_adj_th_level00 = 0x6;
/*-1.0hz*/
static unsigned int vs_adj_th_level01 = 0xc;
/*-1.5hz*/
static unsigned int vs_adj_th_level02 = 0x12;
/*-3hz*/
static unsigned int vs_adj_th_level03 = 0x20;
/*-3.5hz*/
static unsigned int vs_adj_th_level04 = 0x28;
static unsigned int cvd_2e = 0x8c;
static unsigned int cvd_2e_l1 = 0x5c;
static unsigned int acd_128 = 0x14;
static unsigned int acd_128_l1 = 0x1f;
static unsigned int try_format_cnt;

static int dg_ave_last = 0x200;
static int pga_step_last = 1;

static bool cvd_pr_flag;
static bool cvd_pr1_chroma_flag;
static bool cvd_pr2_chroma_flag;

/* zhuangwei, nonstd experiment */
static short nonstd_cnt;
static short nonstd_flag;
static unsigned int chroma_sum_pre1;
static unsigned int chroma_sum_pre2;
static unsigned int chroma_sum_pre3;
/* fanghui,noise det to juge some reg setting */
static unsigned int noise1;
static unsigned int noise2;
static unsigned int noise3;
/* test */
static short print_cnt;

unsigned int vbi_mem_start;


void cvd_vbi_mem_set(unsigned int offset, unsigned int size)
{
	W_APB_REG(ACD_REG_2F, offset);
	if (0) {/*(is_meson_txlx_cpu()) {*/
		W_VBI_APB_BIT(ACD_REG_42, size, 0, 24);
		W_VBI_APB_BIT(ACD_REG_42, 1, 31, 1);
	} else
		W_APB_BIT(ACD_REG_21, size,
			AML_VBI_SIZE_BIT, AML_VBI_SIZE_WID);
	W_APB_BIT(ACD_REG_21, DECODER_VBI_START_ADDR,
		AML_VBI_START_ADDR_BIT, AML_VBI_START_ADDR_WID);
}

void cvd_vbi_config(void)
{
	W_APB_REG(CVD2_VBI_CC_START, VBI_START_CC);
	W_APB_REG(CVD2_VBI_WSS_START, 0x54);
	W_VBI_APB_REG(CVD2_VBI_TT_START, VBI_START_TT);
	W_VBI_APB_REG(CVD2_VBI_VPS_START, VBI_START_VPS);
	W_APB_BIT(CVD2_VBI_CONTROL, 1, 0, 1);
	W_APB_REG(CVD2_VSYNC_VBI_LOCKOUT_START, 0x00000000);
	W_APB_REG(CVD2_VSYNC_VBI_LOCKOUT_END, 0x00000025);
	if (cvd_dbg_en)
		tvafe_pr_info("cvd_vbi_config\n");
}

/*
 * tvafe cvd2 memory setting for 3D comb/motion detection/vbi function
 */
static void tvafe_cvd2_memory_init(struct tvafe_cvd2_mem_s *mem,
				enum tvin_sig_fmt_e fmt)
{
	unsigned int cvd2_addr = mem->start >> 4;/*gxtvbb >>4;g9tv >>3*/
	unsigned int motion_offset = DECODER_MOTION_BUFFER_ADDR_OFFSET;
	unsigned int vbi_offset = DECODER_VBI_ADDR_OFFSET;
	unsigned int vbi_start;
	unsigned int vbi_size;

	if ((mem->start == 0) || (mem->size == 0)) {

		if (cvd_dbg_en)
			tvafe_pr_info("%s: cvd2 memory size error!!!\n",
			__func__);
		return;
	}

	if (tvafe_cpu_type() >= CPU_TYPE_GXTVBB) {
		cvd2_addr = mem->start >> 4;
		motion_offset = motion_offset >> 4;
#if defined(CONFIG_AMLOGIC_MEDIA_TVIN_VBI)
	if (vbi_mem_start != 0)
		vbi_start = vbi_mem_start >> 4;
	else
		vbi_start = cvd2_addr + (vbi_offset >> 4);
#else
	vbi_start = cvd2_addr + (vbi_offset >> 4);
#endif
		vbi_size = (DECODER_VBI_SIZE/2) >> 4;
	} else {
		cvd2_addr = mem->start >> 3;
		motion_offset = motion_offset >> 3;
#if defined(CONFIG_AMLOGIC_MEDIA_TVIN_VBI)
	if (vbi_mem_start != 0)
		vbi_start = vbi_mem_start >> 3;
	else
		vbi_start = cvd2_addr + (vbi_offset >> 3);
#else
	vbi_start = cvd2_addr + (vbi_offset >> 3);
#endif
		vbi_size = (DECODER_VBI_SIZE/2) >> 3;
	}
	/* CVD2 mem addr is based on 64bit, system mem is based on 8bit*/
	W_APB_REG(CVD2_REG_96, cvd2_addr);
	W_APB_REG(ACD_REG_30, (cvd2_addr + motion_offset));
	/* 4frame mode memory setting */
	W_APB_BIT(ACD_REG_2A,
		cvd_mem_4f_length[fmt - TVIN_SIG_FMT_CVBS_NTSC_M],
		REG_4F_MOTION_LENGTH_BIT, REG_4F_MOTION_LENGTH_WID);
#if 1
	/* vbi memory setting */
	cvd_vbi_mem_set(vbi_start, vbi_size);
	/*open front lpf for av ring*/
	W_APB_BIT(ACD_REG_26, 1, 8, 1);
#endif

}

/*txl new add adj filter:*/
/*normal filter: 0x3194 = 0x40100160, 0x3195 = 0x50, 0x3196=0x0.*/
/*sharper filter: 0x3194 = 0x40123282, 0x3195 = 0x19dd03a6, 0x96=0x0.*/
/*even sharper filter: 0x3194 = 0x401012da, 0x3195 = 0x2023036c,*/
/*0x3196 = 0xf20815d4*/
static void tvafe_cvd2_filter_config(void)
{
	switch (CVD2_FILTER_CONFIG_LEVEL) {
	case 0:
		W_APB_REG(ACD_REG_94, 0x40100160);
		W_APB_REG(ACD_REG_95, 0x50);
		W_APB_REG(ACD_REG_96, 0x0);
		break;
	case 1:
		W_APB_REG(ACD_REG_94, 0x40126266);
		W_APB_REG(ACD_REG_95, 0x3ad303a1);
		W_APB_REG(ACD_REG_96, 0x080ee9fa);
		break;
	case 2:
		W_APB_REG(ACD_REG_94, 0x40114288);
		W_APB_REG(ACD_REG_95, 0x38f30388);
		W_APB_REG(ACD_REG_96, 0x0416f7e4);
		break;
	case 3:
		W_APB_REG(ACD_REG_94, 0x40123282);
		W_APB_REG(ACD_REG_95, 0x19dd03a6);
		W_APB_REG(ACD_REG_96, 0x0);
		break;
	default:
		W_APB_REG(ACD_REG_94, 0x40100160);
		W_APB_REG(ACD_REG_95, 0x50);
		W_APB_REG(ACD_REG_96, 0x0);
		break;
	}
	if (cvd_dbg_en)
		tvafe_pr_info("%s cvd2 filter config level %d.\n",
				__func__, CVD2_FILTER_CONFIG_LEVEL);

}

/*
 * tvafe cvd2 load Reg talbe
 */
static void tvafe_cvd2_write_mode_reg(struct tvafe_cvd2_s *cvd2,
		struct tvafe_cvd2_mem_s *mem)
{
	unsigned int i = 0;

	/*disable vbi*/
	W_APB_REG(CVD2_VBI_FRAME_CODE_CTL, 0x10);
	W_APB_REG(ACD_REG_22, 0x07080000);
	/* manuel reset vbi */
	W_APB_REG(ACD_REG_22, 0x87080000);

	/* reset CVD2 */
	W_APB_BIT(CVD2_RESET_REGISTER, 1, SOFT_RST_BIT, SOFT_RST_WID);

	/* for rf&cvbs source acd table */
	if ((cvd2->vd_port == TVIN_PORT_CVBS3) ||
		(cvd2->vd_port == TVIN_PORT_CVBS0)) {

		for (i = 0; i < (ACD_REG_NUM+1); i++) {
			if (i == 0x21 || i == 0x22 || i == 0x2f)
				continue;
			W_APB_REG(((ACD_BASE_ADD+i)<<2),
					(rf_acd_table[cvd2->config_fmt-
					TVIN_SIG_FMT_CVBS_NTSC_M][i]));
		}


	} else {

		for (i = 0; i < (ACD_REG_NUM+1); i++) {

			if (i == 0x21 || i == 0x22 || i == 0x2f)
				continue;
			W_APB_REG(((ACD_BASE_ADD+i)<<2),
					(cvbs_acd_table[cvd2->config_fmt-
					TVIN_SIG_FMT_CVBS_NTSC_M][i]));
		}

	}
	/*cvd filter config*/
	tvafe_cvd2_filter_config();

	/* load CVD2 reg 0x00~3f (char) */
	for (i = 0; i < CVD_PART1_REG_NUM; i++) {

		W_APB_REG(((CVD_BASE_ADD+CVD_PART1_REG_MIN+i)<<2),
				(cvd_part1_table[cvd2->config_fmt-
				TVIN_SIG_FMT_CVBS_NTSC_M][i]));
	}

	/*setting for txhd snow*/
	if (tvafe_cpu_type() == CPU_TYPE_TXHD ||
		tvafe_cpu_type() == CPU_TYPE_TL1) {
		W_APB_BIT(CVD2_OUTPUT_CONTROL, 3, 5, 2);
		W_APB_REG(ACD_REG_6C, 0x80500000);
	}

	/* load CVD2 reg 0x70~ff (char) */
	for (i = 0; i < CVD_PART2_REG_NUM; i++) {

		W_APB_REG(((CVD_BASE_ADD+CVD_PART2_REG_MIN+i)<<2),
				(cvd_part2_table[cvd2->config_fmt-
				TVIN_SIG_FMT_CVBS_NTSC_M][i]));
	}

	/*patch for Very low probability hanging issue on atv close*/
	/*only appeared in one project,this for reserved debug*/
	if (cvd2->nonstd_detect_dis) {
		/*disable the nonstandard signal detect*/
		W_APB_REG(CVD2_NON_STANDARD_SIGNAL_THRESHOLD, 0x0);
		W_APB_REG(CVD2_REG_B6, 0x0);
	}

	if (((cvd2->vd_port == TVIN_PORT_CVBS1) ||
		(cvd2->vd_port == TVIN_PORT_CVBS2)) &&
		(cvd2->config_fmt == TVIN_SIG_FMT_CVBS_NTSC_M))
		W_APB_REG(CVD2_VSYNC_SIGNAL_THRESHOLD, 0x7d);

	/* reload CVD2 reg 0x87, 0x93, 0x94, 0x95, 0x96, 0xe6, 0xfa (int) */
	W_APB_REG(((CVD_BASE_ADD+CVD_PART3_REG_0)<<2),
		cvd_part3_table[cvd2->config_fmt-TVIN_SIG_FMT_CVBS_NTSC_M][0]);
	W_APB_REG(((CVD_BASE_ADD+CVD_PART3_REG_1)<<2),
		cvd_part3_table[cvd2->config_fmt-TVIN_SIG_FMT_CVBS_NTSC_M][1]);
	W_APB_REG(((CVD_BASE_ADD+CVD_PART3_REG_2)<<2),
		cvd_part3_table[cvd2->config_fmt-TVIN_SIG_FMT_CVBS_NTSC_M][2]);
	W_APB_REG(((CVD_BASE_ADD+CVD_PART3_REG_3)<<2),
		cvd_part3_table[cvd2->config_fmt-TVIN_SIG_FMT_CVBS_NTSC_M][3]);
	W_APB_REG(((CVD_BASE_ADD+CVD_PART3_REG_4)<<2),
		cvd_part3_table[cvd2->config_fmt-TVIN_SIG_FMT_CVBS_NTSC_M][4]);
	W_APB_REG(((CVD_BASE_ADD+CVD_PART3_REG_5)<<2),
		cvd_part3_table[cvd2->config_fmt-TVIN_SIG_FMT_CVBS_NTSC_M][5]);
	W_APB_REG(((CVD_BASE_ADD+CVD_PART3_REG_6)<<2),
		cvd_part3_table[cvd2->config_fmt-TVIN_SIG_FMT_CVBS_NTSC_M][6]);

	/* for tuner picture quality */

	if ((cvd2->vd_port == TVIN_PORT_CVBS3) ||
		(cvd2->vd_port == TVIN_PORT_CVBS0)) {

		W_APB_REG(CVD2_REG_B0, 0xf0);
		W_APB_BIT(CVD2_REG_B2, 0,
			ADAPTIVE_CHROMA_MODE_BIT, ADAPTIVE_CHROMA_MODE_WID);
		W_APB_BIT(CVD2_CONTROL1, 0, CHROMA_BW_LO_BIT, CHROMA_BW_LO_WID);

	} else {
		W_APB_REG(CVD2_VSYNC_NO_SIGNAL_THRESHOLD, 0xf0);
		if (cvd2->config_fmt == TVIN_SIG_FMT_CVBS_PAL_I)
				/*add for chroma state adjust dynamicly*/
			W_APB_REG(CVD2_CHROMA_LOOPFILTER_STATE, cvd_reg8a);
	}
#ifdef TVAFE_CVD2_CC_ENABLE
	W_APB_REG(CVD2_VBI_DATA_TYPE_LINE21, 0x00000011);
	W_APB_REG(CVD2_VSYNC_VBI_LOCKOUT_START, 0x00000000);
	W_APB_REG(CVD2_VSYNC_VBI_LOCKOUT_END, 0x00000025);
	W_APB_REG(CVD2_VSYNC_TIME_CONSTANT, 0x0000000a);
	W_APB_REG(CVD2_VBI_CC_START, 0x00000054);
	W_APB_REG(CVD2_VBI_FRAME_CODE_CTL, 0x11);
	W_APB_REG(ACD_REG_22, 0x82080000); /* manuel reset vbi */
	W_APB_REG(ACD_REG_22, 0x04080000);
	/* vbi reset release, vbi agent enable */
#endif

#if defined(CONFIG_TVIN_TUNER_SI2176)
	if ((cvd2->vd_port == TVIN_PORT_CVBS3) ||
		(cvd2->vd_port == TVIN_PORT_CVBS0)) {

		W_APB_BIT(CVD2_NON_STANDARD_SIGNAL_THRESHOLD, 3,
				HNON_STD_TH_BIT, HNON_STD_TH_WID);
	}
#elif defined(CONFIG_TVIN_TUNER_HTM9AW125)
	if (((cvd2->vd_port == TVIN_PORT_CVBS3) ||
		(cvd2->vd_port == TVIN_PORT_CVBS0)) &&
		(cvd2->config_fmt == TVIN_SIG_FMT_CVBS_NTSC_M)) {

		W_APB_BIT(ACD_REG_1B, 0xc, YCSEP_TEST6F_BIT, YCSEP_TEST6F_WID);
	}
#endif

	/* add for board e04&e08  */
	if (((cvd2->vd_port == TVIN_PORT_CVBS3) ||
		(cvd2->vd_port == TVIN_PORT_CVBS0)) &&
		(CVD_REG07_PAL != 0x03)
		&& (cvd2->config_fmt == TVIN_SIG_FMT_CVBS_PAL_I))
		W_APB_REG(CVD2_OUTPUT_CONTROL, CVD_REG07_PAL);

	/*disable vbi*/
	W_APB_REG(CVD2_VBI_FRAME_CODE_CTL, 0x10);

	/* 3D comb filter buffer assignment */
	tvafe_cvd2_memory_init(mem, cvd2->config_fmt);

#if 1/* TVAFE_CVD2_WSS_ENABLE */
	/* config data type */
	/*line17 for PAL M*/
	W_APB_REG(CVD2_VBI_DATA_TYPE_LINE17, 0xcc);
	/*line23 for PAL B,D,G,H,I,N,CN*/
	W_APB_REG(CVD2_VBI_DATA_TYPE_LINE23, 0xcc);
	/* config wss dto */
	W_APB_REG(CVD2_VBI_WSS_DTO_MSB, 0x20);
	W_APB_REG(CVD2_VBI_WSS_DTO_LSB, 0x66);

	/*0x40[2]=0 Use a fixed vbi threshold 0x30*/
	W_APB_REG(CVD2_VBI_DATA_HLVL, 0x30);

	/* config vbi start line */
	cvd_vbi_config();

	/* be care the polarity bellow!!! */
	if (cvd2->config_fmt != TVIN_SIG_FMT_CVBS_PAL_CN)
		W_APB_BIT(CVD2_VSYNC_TIME_CONSTANT, 0, 7, 1);

	W_APB_REG(ACD_REG_22, 0x04080000);
	/*enable vbi*/
	W_APB_REG(CVD2_VBI_FRAME_CODE_CTL, 0x11);
	pr_info("[tvafe..] %s: enable vbi\n", __func__);
#endif
	/*for palm moonoscope pattern color flash*/
	if (cvd2->config_fmt == TVIN_SIG_FMT_CVBS_PAL_M) {
		W_APB_REG(ACD_REG_22, 0x2020000);
		W_APB_REG(CVD2_NOISE_THRESHOLD, 0xff);
		W_APB_REG(CVD2_NON_STANDARD_SIGNAL_THRESHOLD, 0x20);
	}

	/*set for wipe off vertical stripes*/
	if ((cvd2->vd_port > TVIN_PORT_CVBS0) &&
		(cvd2->vd_port <= TVIN_PORT_CVBS3) &&
		(cvd2->vd_port != TVIN_PORT_CVBS3) &&
		(tvafe_cpu_type() >= CPU_TYPE_TXL))
		W_APB_REG(ACD_REG_25, 0x00e941a8);

	/* enable CVD2 */
	W_APB_BIT(CVD2_RESET_REGISTER, 0, SOFT_RST_BIT, SOFT_RST_WID);
}

/*
 * tvafe cvd2 configure Reg for non-standard signal
 */
static void tvafe_cvd2_non_std_config(struct tvafe_cvd2_s *cvd2)
{
	static unsigned int time_non_count = 50;
	unsigned int noise_read = 0;
	unsigned int noise_strenth = 0;

	noise_read = R_APB_REG(CVD2_SYNC_NOISE_STATUS);
	noise3 = noise2;
	noise2 = noise1;
	noise1 = noise_read;
	noise_strenth = (noise1+(noise2<<1)+noise3)>>2;

	if (time_non_count) {
		time_non_count--;
		return;
	}
	time_non_count = 200;
	if (force_nostd == 3)
		return;
	if ((cvd2->info.non_std_config == cvd2->info.non_std_enable) &&
		(force_nostd&0x2))
		return;
	cvd2->info.non_std_config = cvd2->info.non_std_enable;
	if (cvd2->info.non_std_config && (!(force_nostd&0x1))) {

		if (cvd_nonstd_dbg_en) {
			tvafe_pr_info("%s: config non-std signal reg.\n",
				__func__);
			tvafe_pr_info("%s: noise_strenth=%d.\n",
						__func__, noise_strenth);
		}

#ifdef CONFIG_AM_SI2176
		if ((cvd2->vd_port == TVIN_PORT_CVBS3) ||
			(cvd2->vd_port == TVIN_PORT_CVBS0)) {

			W_APB_BIT(CVD2_NON_STANDARD_SIGNAL_THRESHOLD, 3,
					HNON_STD_TH_BIT, HNON_STD_TH_WID);
			W_APB_BIT(CVD2_VSYNC_SIGNAL_THRESHOLD, 1,
				VS_SIGNAL_AUTO_TH_BIT, VS_SIGNAL_AUTO_TH_WID);
			W_APB_REG(CVD2_NOISE_THRESHOLD, 0x04);
			if (scene_colorful)
				W_APB_REG(CVD2_VSYNC_CNTL, 0x02);
			if (noise_strenth > 48 && NOISE_JUDGE)
				W_APB_BIT(CVD2_H_LOOP_MAXSTATE, 4,
					HSTATE_MAX_BIT, HSTATE_MAX_WID);
			else
				W_APB_BIT(CVD2_H_LOOP_MAXSTATE, 5,
					HSTATE_MAX_BIT, HSTATE_MAX_WID);
			W_APB_BIT(CVD2_ACTIVE_VIDEO_VSTART, 0x14,
					VACTIVE_START_BIT, VACTIVE_START_WID);
			W_APB_BIT(CVD2_ACTIVE_VIDEO_VHEIGHT, 0xe0,
					VACTIVE_HEIGHT_BIT, VACTIVE_HEIGHT_WID);
		} else {
			/* W_APB_BIT(CVD2_VSYNC_SIGNAL_THRESHOLD, 1, */
			/* VS_SIGNAL_AUTO_TH_BIT, VS_SIGNAL_AUTO_TH_WID); */
			W_APB_REG(CVD2_HSYNC_RISING_EDGE_START, 0x25);
			W_APB_BIT(TVFE_CLAMP_INTF, 0x661, 0, 12);
			if (noise_strenth > 48 && NOISE_JUDGE)
				W_APB_BIT(CVD2_H_LOOP_MAXSTATE, 4,
					HSTATE_MAX_BIT, HSTATE_MAX_WID);
			else
				W_APB_BIT(CVD2_H_LOOP_MAXSTATE, 5,
					HSTATE_MAX_BIT, HSTATE_MAX_WID);
			if (scene_colorful)
				W_APB_REG(CVD2_VSYNC_CNTL, 0x02);
			W_APB_REG(CVD2_VSYNC_SIGNAL_THRESHOLD, 0x10);
			W_APB_REG(CVD2_NOISE_THRESHOLD, 0x08);
		}
#else
		W_APB_REG(CVD2_HSYNC_RISING_EDGE_START, 0x25);
		W_APB_BIT(TVFE_CLAMP_INTF, 0x0, 0, 12);
		W_APB_BIT(CVD2_H_LOOP_MAXSTATE, 4,
			HSTATE_MAX_BIT, HSTATE_MAX_WID);
		if ((cvd2->vd_port == TVIN_PORT_CVBS3) ||
			(cvd2->vd_port == TVIN_PORT_CVBS0)) {

			/*config 0 for tuner R840/mxl661*/
			/*si2151 si2159 r842 may need set 1*/
			W_APB_BIT(CVD2_VSYNC_SIGNAL_THRESHOLD, 0,
				VS_SIGNAL_AUTO_TH_BIT, VS_SIGNAL_AUTO_TH_WID);

			/* vsync signal is not good */
			W_APB_REG(CVD2_NOISE_THRESHOLD, 0x00);

		} else {

			if (scene_colorful)
				W_APB_REG(CVD2_VSYNC_CNTL, 0x02);
			W_APB_BIT(CVD2_VSYNC_SIGNAL_THRESHOLD, 1,
				VS_SIGNAL_AUTO_TH_BIT, VS_SIGNAL_AUTO_TH_WID);
			W_APB_BIT(CVD2_H_LOOP_MAXSTATE, 1,
				DISABLE_HFINE_BIT, DISABLE_HFINE_WID);
			W_APB_REG(CVD2_NOISE_THRESHOLD, 0x08);
		}
#endif

	} else {
		if (cvd_nonstd_dbg_en)
			tvafe_pr_info("%s: out of non-std signal.\n",
				__func__);
		W_APB_REG(CVD2_HSYNC_RISING_EDGE_START, 0x6d);
		/*bit 15 dis/enabled by avin detect*/
		W_APB_BIT(TVFE_CLAMP_INTF, 0x666, 0, 12);
		W_APB_BIT(CVD2_H_LOOP_MAXSTATE, 5,
			HSTATE_MAX_BIT, HSTATE_MAX_WID);
		if ((cvd2->vd_port == TVIN_PORT_CVBS3) ||
			(cvd2->vd_port == TVIN_PORT_CVBS0)) {


#ifdef CVD_SI2176_RSSI
			if (cvd_get_rf_strength() < 187 &&
				cvd_get_rf_strength() > 100 &&
				SYNC_SENSITIVITY) {

				W_APB_REG(CVD2_VSYNC_SIGNAL_THRESHOLD, 0xf0);
				W_APB_REG(CVD2_VSYNC_CNTL, 0x2);
				if (cvd_nonstd_dbg_en)
					tvafe_pr_info("%s: out of non-std signal.rssi=%d\n",
					__func__, cvd_get_rf_strength());
			}
#else

			if (R_APB_REG(CVD2_SYNC_NOISE_STATUS) > 48 &&
				SYNC_SENSITIVITY) {
				W_APB_REG(CVD2_VSYNC_SIGNAL_THRESHOLD, 0xf0);
				W_APB_REG(CVD2_VSYNC_CNTL, 0x2);
				if (cvd_nonstd_dbg_en)
					tvafe_pr_info("%s: use the cvd register to judge the rssi.rssi=%u\n",
				__func__, R_APB_REG(CVD2_SYNC_NOISE_STATUS));
			}
#endif
			else{
				W_APB_REG(CVD2_VSYNC_CNTL, 0x01);
				W_APB_BIT(CVD2_VSYNC_SIGNAL_THRESHOLD, 0,
				VS_SIGNAL_AUTO_TH_BIT, VS_SIGNAL_AUTO_TH_WID);
			}
			W_APB_REG(CVD2_NOISE_THRESHOLD, 0x32);
			W_APB_BIT(CVD2_ACTIVE_VIDEO_VSTART, 0x2a,
					VACTIVE_START_BIT, VACTIVE_START_WID);
			W_APB_BIT(CVD2_ACTIVE_VIDEO_VHEIGHT, 0xc0,
					VACTIVE_HEIGHT_BIT, VACTIVE_HEIGHT_WID);

		} else {

			W_APB_BIT(CVD2_VSYNC_SIGNAL_THRESHOLD, 0,
				VS_SIGNAL_AUTO_TH_BIT, VS_SIGNAL_AUTO_TH_WID);
			W_APB_REG(CVD2_VSYNC_CNTL, 0x01);
			W_APB_BIT(CVD2_VSYNC_SIGNAL_THRESHOLD, 0,
				VS_SIGNAL_AUTO_TH_BIT, VS_SIGNAL_AUTO_TH_WID);
			W_APB_BIT(CVD2_H_LOOP_MAXSTATE, 0,
				DISABLE_HFINE_BIT, DISABLE_HFINE_WID);
			W_APB_REG(CVD2_NOISE_THRESHOLD, 0x32);
		}
	}

}

/*
 * tvafe cvd2 reset pga setting
 */
inline void tvafe_cvd2_reset_pga(void)
{
	/* reset pga value */
	W_APB_BIT(TVFE_VAFE_CTRL1, PGA_DEFAULT_VAL,
		VAFE_PGA_GAIN_BIT, VAFE_PGA_GAIN_WID);

	/*reset pga parameter*/
	pga_step_last = 1;
	dg_ave_last = 0x200;
}

#ifdef TVAFE_SET_CVBS_CDTO_EN
/* tvafe cvd2 read cdto setting from Reg*/
static unsigned int tvafe_cvd2_get_cdto(void)
{
	unsigned int cdto = 0;

	cdto = (R_APB_BIT(CVD2_CHROMA_DTO_INCREMENT_29_24,
		CDTO_INC_29_24_BIT, CDTO_INC_29_24_WID) & 0x0000003f)<<24;
	cdto += (R_APB_BIT(CVD2_CHROMA_DTO_INCREMENT_23_16,
		CDTO_INC_23_16_BIT, CDTO_INC_23_16_WID) & 0x000000ff)<<16;
	cdto += (R_APB_BIT(CVD2_CHROMA_DTO_INCREMENT_15_8,
		CDTO_INC_15_8_BIT, CDTO_INC_15_8_WID) & 0x000000ff)<<8;
	cdto += (R_APB_BIT(CVD2_CHROMA_DTO_INCREMENT_7_0,
		CDTO_INC_7_0_BIT, CDTO_INC_7_0_WID) & 0x000000ff);
	return cdto;
}

/*tvafe cvd2 write cdto value to Reg*/
static void tvafe_cvd2_set_cdto(unsigned int cdto)
{
	W_APB_REG(CVD2_CHROMA_DTO_INCREMENT_29_24, (cdto >> 24) & 0x0000003f);
	W_APB_REG(CVD2_CHROMA_DTO_INCREMENT_23_16, (cdto >> 16) & 0x000000ff);
	W_APB_REG(CVD2_CHROMA_DTO_INCREMENT_15_8,  (cdto >>  8) & 0x000000ff);
	W_APB_REG(CVD2_CHROMA_DTO_INCREMENT_7_0,   (cdto >>  0) & 0x000000ff);
}

/*set default cdto */
void tvafe_cvd2_set_default_cdto(struct tvafe_cvd2_s *cvd2)
{
	if (!cvd2) {

		tvafe_pr_info("%s cvd2 null error.\n", __func__);
		return;
	}
		switch (cvd2->config_fmt) {

		case TVIN_SIG_FMT_CVBS_NTSC_M:
			if (tvafe_cvd2_get_cdto() != CVD2_CHROMA_DTO_NTSC_M)
				tvafe_cvd2_set_cdto(CVD2_CHROMA_DTO_NTSC_M);
			break;
		case TVIN_SIG_FMT_CVBS_NTSC_443:
			if (tvafe_cvd2_get_cdto() != CVD2_CHROMA_DTO_NTSC_443)
				tvafe_cvd2_set_cdto(CVD2_CHROMA_DTO_NTSC_443);
			break;
		case TVIN_SIG_FMT_CVBS_PAL_I:
			if (tvafe_cvd2_get_cdto() != CVD2_CHROMA_DTO_PAL_I)
				tvafe_cvd2_set_cdto(CVD2_CHROMA_DTO_PAL_I);
			break;
		case TVIN_SIG_FMT_CVBS_PAL_M:
			if (tvafe_cvd2_get_cdto() != CVD2_CHROMA_DTO_PAL_M)
				tvafe_cvd2_set_cdto(CVD2_CHROMA_DTO_PAL_M);
			break;
		case TVIN_SIG_FMT_CVBS_PAL_60:
			if (tvafe_cvd2_get_cdto() != CVD2_CHROMA_DTO_PAL_60)
				tvafe_cvd2_set_cdto(CVD2_CHROMA_DTO_PAL_60);
			break;
		case TVIN_SIG_FMT_CVBS_PAL_CN:
			if (tvafe_cvd2_get_cdto() != CVD2_CHROMA_DTO_PAL_CN)
				tvafe_cvd2_set_cdto(CVD2_CHROMA_DTO_PAL_CN);
			break;
		case TVIN_SIG_FMT_CVBS_SECAM:
			if (tvafe_cvd2_get_cdto() != CVD2_CHROMA_DTO_SECAM)
				tvafe_cvd2_set_cdto(CVD2_CHROMA_DTO_SECAM);
			break;
		default:
			break;
	}
	if (cvd_dbg_en)
		tvafe_pr_info("%s set cdto to default fmt %s.\n",
				__func__, tvin_sig_fmt_str(cvd2->config_fmt));
}
#endif

/*
 * tvafe cvd2 write Reg table by different format
 */
inline void tvafe_cvd2_try_format(struct tvafe_cvd2_s *cvd2,
			struct tvafe_cvd2_mem_s *mem, enum tvin_sig_fmt_e fmt)
{
	/* check format validation */
	if ((fmt < TVIN_SIG_FMT_CVBS_NTSC_M) ||
		(fmt > TVIN_SIG_FMT_CVBS_NTSC_50)) {

		if (cvd_dbg_en)
			tvafe_pr_err("%s: cvd2 try format error!!!\n",
			__func__);
		return;
	}

	if (fmt != cvd2->config_fmt) {
		lock_cnt = 0;
		tvafe_pr_info("%s: try new fmt:%s\n",
				__func__, tvin_sig_fmt_str(fmt));
		cvd2->config_fmt = fmt;
		tvafe_cvd2_write_mode_reg(cvd2, mem);
		/* init variable */
		memset(&cvd2->info, 0, sizeof(struct tvafe_cvd2_info_s));
	}
}

/*
 * tvafe cvd2 get signal status from Reg
 */
void tvafe_cvd2_get_signal_status(struct tvafe_cvd2_s *cvd2)
{
	int data = 0;

	data = R_APB_REG(CVD2_STATUS_REGISTER1);
	cvd2->hw_data[cvd2->hw_data_cur].no_sig =
				(bool)((data & 0x01) >> NO_SIGNAL_BIT);
	cvd2->hw_data[cvd2->hw_data_cur].h_lock =
				(bool)((data & 0x02) >> HLOCK_BIT);
	cvd2->hw_data[cvd2->hw_data_cur].v_lock =
				(bool)((data & 0x04) >> VLOCK_BIT);
	cvd2->hw_data[cvd2->hw_data_cur].chroma_lock =
				(bool)((data & 0x08) >> CHROMALOCK_BIT);

	data = R_APB_REG(CVD2_STATUS_REGISTER2);
	cvd2->hw_data[cvd2->hw_data_cur].h_nonstd =
				(bool)((data & 0x02) >> HNON_STD_BIT);
	cvd2->hw_data[cvd2->hw_data_cur].v_nonstd =
				(bool)((data & 0x04) >> VNON_STD_BIT);
	cvd2->hw_data[cvd2->hw_data_cur].no_color_burst =
				(bool)((data & 0x08) >> BKNWT_DETECTED_BIT);
	cvd2->hw_data[cvd2->hw_data_cur].comb3d_off =
				(bool)((data & 0x10) >> STATUS_COMB3D_OFF_BIT);

	data = R_APB_REG(CVD2_STATUS_REGISTER3);
	cvd2->hw_data[cvd2->hw_data_cur].pal =
				(bool)((data & 0x01) >> PAL_DETECTED_BIT);
	cvd2->hw_data[cvd2->hw_data_cur].secam =
				(bool)((data & 0x02) >> SECAM_DETECTED_BIT);
	cvd2->hw_data[cvd2->hw_data_cur].line625 =
				(bool)((data & 0x04) >> LINES625_DETECTED_BIT);
	cvd2->hw_data[cvd2->hw_data_cur].noisy =
				(bool)((data & 0x08) >> NOISY_BIT);
	cvd2->hw_data[cvd2->hw_data_cur].vcr =
				(bool)((data & 0x10) >> VCR_BIT);
	cvd2->hw_data[cvd2->hw_data_cur].vcrtrick =
				(bool)((data & 0x20) >> VCR_TRICK_BIT);
	cvd2->hw_data[cvd2->hw_data_cur].vcrff =
				(bool)((data & 0x40) >> VCR_FF_BIT);
	cvd2->hw_data[cvd2->hw_data_cur].vcrrew =
				(bool)((data & 0x80) >> VCR_REW_BIT);

	cvd2->hw_data[cvd2->hw_data_cur].cordic =
			R_APB_BIT(CVD2_CORDIC_FREQUENCY_STATUS,
			STATUS_CORDIQ_FRERQ_BIT, STATUS_CORDIQ_FRERQ_WID);

	/* need the average of 3 fields ? */
	data = R_APB_REG(ACD_REG_83);
	cvd2->hw_data[cvd2->hw_data_cur].acc4xx_cnt =
			(unsigned char)(data >> RO_BD_ACC4XX_CNT_BIT);
	cvd2->hw_data[cvd2->hw_data_cur].acc425_cnt =
			(unsigned char)(data >> RO_BD_ACC425_CNT_BIT);
	cvd2->hw_data[cvd2->hw_data_cur].acc3xx_cnt =
			(unsigned char)(data >> RO_BD_ACC3XX_CNT_BIT);
	cvd2->hw_data[cvd2->hw_data_cur].acc358_cnt =
			(unsigned char)(data >> RO_BD_ACC358_CNT_BIT);
	data = cvd2->hw_data[0].acc4xx_cnt + cvd2->hw_data[1].acc4xx_cnt +
			cvd2->hw_data[2].acc4xx_cnt;
	cvd2->hw.acc4xx_cnt = data / 3;
	data = cvd2->hw_data[0].acc425_cnt + cvd2->hw_data[1].acc425_cnt +
			cvd2->hw_data[2].acc425_cnt;
	cvd2->hw.acc425_cnt = data / 3;
	data = cvd2->hw_data[0].acc3xx_cnt + cvd2->hw_data[1].acc3xx_cnt +
			cvd2->hw_data[2].acc3xx_cnt;
	cvd2->hw.acc3xx_cnt = data / 3;
	data = cvd2->hw_data[0].acc358_cnt + cvd2->hw_data[1].acc358_cnt +
			cvd2->hw_data[2].acc358_cnt;
	cvd2->hw.acc358_cnt = data / 3;
	data = R_APB_REG(ACD_REG_84);
	cvd2->hw_data[cvd2->hw_data_cur].secam_detected =
			(bool)((data & 0x1) >> RO_BD_SECAM_DETECTED_BIT);
	cvd2->hw.secam_phase = (bool)((data & 0x2) >> RO_DBDR_PHASE_BIT);
	if (cvd2->hw_data[0].secam_detected &&
		cvd2->hw_data[1].secam_detected &&
		cvd2->hw_data[2].secam_detected)
		cvd2->hw.secam_detected = true;
	if (!cvd2->hw_data[0].secam_detected &&
		!cvd2->hw_data[1].secam_detected &&
		!cvd2->hw_data[2].secam_detected)
		cvd2->hw.secam_detected = false;

	if (cnt_dbg_en) {

		tvafe_pr_info("[%d]:cvd2->hw.acc3xx_cnt =%d,cvd2->hw.acc4xx_cnt=%d,acc425_cnt=%d\n",
			__LINE__,
		cvd2->hw.acc3xx_cnt, cvd2->hw.acc4xx_cnt, cvd2->hw.acc425_cnt);
		tvafe_pr_info("[%d]:cvd2->hw.fsc_358=%d,cvd2->hw.fsc_425=%d,cvd2->hw.fsc_443 =%d\n",
			__LINE__,
		cvd2->hw.fsc_358, cvd2->hw.fsc_425, cvd2->hw.fsc_443);
		}
	if (cvd2->hw.acc3xx_cnt > CNT_VLD_TH) {

		if (cvd2->hw.acc358_cnt >
			(cvd2->hw.acc3xx_cnt - (cvd2->hw.acc3xx_cnt>>2))) {

			cvd2->hw.fsc_358 = true;
			cvd2->hw.fsc_425 = false;
			cvd2->hw.fsc_443 = false;

		} else if (cvd2->hw.acc358_cnt < (cvd2->hw.acc3xx_cnt<<1)/5) {
			cvd2->hw.fsc_358 = false;
			if (cvd2->hw.acc4xx_cnt > CNT_VLD_TH) {
				cvd2->hw.fsc_425 = false;
				cvd2->hw.fsc_443 = true;
			} else if (cvd2->hw.acc4xx_cnt  < 40) {
				cvd2->hw.fsc_425 = false;
				cvd2->hw.fsc_443 = false;
			}
		}

	} else if (cvd2->hw.acc3xx_cnt < 40) {

		cvd2->hw.fsc_358 = false;
		cvd2->hw.fsc_425 = false;
		cvd2->hw.fsc_443 = false;
		cvd2->hw.no_color_burst = true;
	}
	if (++ cvd2->hw_data_cur >= 3)
		cvd2->hw_data_cur = 0;
	if (cnt_dbg_en)
		tvafe_pr_info("[%d]:cvd2->hw.fsc_358=%d,cvd2->hw.fsc_425=%d,cvd2->hw.fsc_443 =%d\n",
		__LINE__, cvd2->hw.fsc_358,
		cvd2->hw.fsc_425, cvd2->hw.fsc_443);

#ifdef TVAFE_CVD2_NOT_TRUST_NOSIG
#else
	if (cvd2->hw_data[0].no_sig && cvd2->hw_data[1].no_sig &&
		cvd2->hw_data[2].no_sig)
		cvd2->hw.no_sig = true;
	if (!cvd2->hw_data[0].no_sig && !cvd2->hw_data[1].no_sig &&
		!cvd2->hw_data[2].no_sig)
		cvd2->hw.no_sig = false;
#endif
	if ((cvd2->hw_data[0].h_lock || cvd2->hw_data[1].h_lock) &&
		cvd2->hw_data[2].h_lock)
		cvd2->hw.h_lock = true;
	else if (cvd2->hw_data[0].h_lock &&
		(cvd2->hw_data[1].h_lock || cvd2->hw_data[2].h_lock))
		cvd2->hw.h_lock = true;
	if (!cvd2->hw_data[0].h_lock && !cvd2->hw_data[1].h_lock &&
		!cvd2->hw_data[2].h_lock)
		cvd2->hw.h_lock = false;

	if (cvd2->hw_data[0].v_lock && cvd2->hw_data[1].v_lock &&
		cvd2->hw_data[2].v_lock)
		cvd2->hw.v_lock = true;
	if (!cvd2->hw_data[0].v_lock && !cvd2->hw_data[1].v_lock &&
		!cvd2->hw_data[2].v_lock)
		cvd2->hw.v_lock = false;

	if (cvd2->hw_data[0].h_nonstd && cvd2->hw_data[1].h_nonstd &&
		cvd2->hw_data[2].h_nonstd)
		cvd2->hw.h_nonstd = true;
	if (!cvd2->hw_data[0].h_nonstd && !cvd2->hw_data[1].h_nonstd &&
		!cvd2->hw_data[2].h_nonstd)
		cvd2->hw.h_nonstd = false;

	if (cvd2->hw_data[0].v_nonstd && cvd2->hw_data[1].v_nonstd &&
		cvd2->hw_data[2].v_nonstd)
		cvd2->hw.v_nonstd = true;
	else if (cvd2->hw_data[0].v_nonstd ||
		(cvd2->hw_data[1].v_nonstd && cvd2->hw_data[2].v_nonstd))
		cvd2->hw.v_nonstd = true;
	if (!cvd2->hw_data[0].v_nonstd && !cvd2->hw_data[1].v_nonstd &&
		!cvd2->hw_data[2].v_nonstd)
		cvd2->hw.v_nonstd = false;

	if (cvd2->hw_data[0].no_color_burst &&
		cvd2->hw_data[1].no_color_burst &&
		cvd2->hw_data[2].no_color_burst)
		cvd2->hw.no_color_burst = true;
	if (!cvd2->hw_data[0].no_color_burst &&
		!cvd2->hw_data[1].no_color_burst &&
		!cvd2->hw_data[2].no_color_burst)
		cvd2->hw.no_color_burst = false;

	if (cvd2->hw_data[0].comb3d_off && cvd2->hw_data[1].comb3d_off &&
		cvd2->hw_data[2].comb3d_off)
		cvd2->hw.comb3d_off = true;
	if (!cvd2->hw_data[0].comb3d_off && !cvd2->hw_data[1].comb3d_off &&
		!cvd2->hw_data[2].comb3d_off)
		cvd2->hw.comb3d_off = false;

	if (cvd2->hw_data[0].chroma_lock &&
		(cvd2->hw_data[1].chroma_lock || cvd2->hw_data[2].chroma_lock))
		cvd2->hw.chroma_lock = true;
	else if ((cvd2->hw_data[0].chroma_lock ||
		cvd2->hw_data[1].chroma_lock) &&
		cvd2->hw_data[2].chroma_lock)
		cvd2->hw.chroma_lock = true;
	if (!cvd2->hw_data[0].chroma_lock && !cvd2->hw_data[1].chroma_lock &&
		!cvd2->hw_data[2].chroma_lock)
		cvd2->hw.chroma_lock = false;

	if ((cvd2->hw_data[0].pal || cvd2->hw_data[1].pal) &&
		cvd2->hw_data[2].pal)
		cvd2->hw.pal = true;
	else if (cvd2->hw_data[0].pal &&
		(cvd2->hw_data[1].pal || cvd2->hw_data[2].pal))
		cvd2->hw.pal = true;
	else if ((cvd2->hw_data[0].pal || cvd2->hw_data[2].pal) &&
		cvd2->hw_data[1].pal)
		cvd2->hw.pal = true;
	if (!cvd2->hw_data[0].pal && !cvd2->hw_data[1].pal &&
		!cvd2->hw_data[2].pal)
		cvd2->hw.pal = false;

	if (cvd2->hw_data[0].secam && cvd2->hw_data[1].secam &&
		cvd2->hw_data[2].secam)
		cvd2->hw.secam = true;
	if (!cvd2->hw_data[0].secam && !cvd2->hw_data[1].secam &&
		!cvd2->hw_data[2].secam)
		cvd2->hw.secam = false;

	if (cvd2->hw_data[0].line625 && cvd2->hw_data[1].line625 &&
		cvd2->hw_data[2].line625)
		cvd2->hw.line625 = true;
	if (!cvd2->hw_data[0].line625 && !cvd2->hw_data[1].line625 &&
		!cvd2->hw_data[2].line625)
		cvd2->hw.line625 = false;

	if (cvd2->hw_data[0].noisy && cvd2->hw_data[1].noisy &&
		cvd2->hw_data[2].noisy)
		cvd2->hw.noisy = true;
	if (!cvd2->hw_data[0].noisy && !cvd2->hw_data[1].noisy &&
		!cvd2->hw_data[2].noisy)
		cvd2->hw.noisy = false;

	if (cvd2->hw_data[0].vcr && cvd2->hw_data[1].vcr &&
		cvd2->hw_data[2].vcr)
		cvd2->hw.vcr = true;
	if (!cvd2->hw_data[0].vcr && !cvd2->hw_data[1].vcr &&
		!cvd2->hw_data[2].vcr)
		cvd2->hw.vcr = false;

	if (cvd2->hw_data[0].vcrtrick && cvd2->hw_data[1].vcrtrick &&
		cvd2->hw_data[2].vcrtrick)
		cvd2->hw.vcrtrick = true;
	if (!cvd2->hw_data[0].vcrtrick && !cvd2->hw_data[1].vcrtrick &&
		!cvd2->hw_data[2].vcrtrick)
		cvd2->hw.vcrtrick = false;

	if (cvd2->hw_data[0].vcrff && cvd2->hw_data[1].vcrff &&
		cvd2->hw_data[2].vcrff)
		cvd2->hw.vcrff = true;
	if (!cvd2->hw_data[0].vcrff && !cvd2->hw_data[1].vcrff &&
		!cvd2->hw_data[2].vcrff)
		cvd2->hw.vcrff = false;

	if (cvd2->hw_data[0].vcrrew && cvd2->hw_data[1].vcrrew &&
		cvd2->hw_data[2].vcrrew)
		cvd2->hw.vcrrew = true;
	if (!cvd2->hw_data[0].vcrrew && !cvd2->hw_data[1].vcrrew &&
		!cvd2->hw_data[2].vcrrew)
		cvd2->hw.vcrrew = false;
#ifdef TVAFE_CVD2_NOT_TRUST_NOSIG
/* while tv channel switch, avoid black screen */
	if (!cvd2->hw.h_lock)
		cvd2->hw.no_sig = true;
	if (cvd2->hw.h_lock)
		cvd2->hw.no_sig = false;
#endif

	data  = 0;
	data += (int)cvd2->hw_data[0].cordic;
	data += (int)cvd2->hw_data[1].cordic;
	data += (int)cvd2->hw_data[2].cordic;
	if (cvd2->hw_data[0].cordic & 0x80)

		data -= 256;

	if (cvd2->hw_data[1].cordic & 0x80)

		data -= 256;

	if (cvd2->hw_data[2].cordic & 0x80)

		data -= 256;

	data /= 3;
	cvd2->hw.cordic  = (unsigned char)(data & 0xff);
}

/*tvafe cvd2 get cvd2 signal lock status*/
enum tvafe_cvbs_video_e tvafe_cvd2_get_lock_status(
					struct tvafe_cvd2_s *cvd2)
{
	enum tvafe_cvbs_video_e cvbs_lock_status = TVAFE_CVBS_VIDEO_HV_UNLOCKED;

	if (!cvd2->hw.h_lock && !cvd2->hw.v_lock)
		cvbs_lock_status = TVAFE_CVBS_VIDEO_HV_UNLOCKED;
	else if (cvd2->hw.h_lock && cvd2->hw.v_lock) {
		cvbs_lock_status = TVAFE_CVBS_VIDEO_HV_LOCKED;
		lock_cnt++;
	} else if (cvd2->hw.h_lock) {
		cvbs_lock_status = TVAFE_CVBS_VIDEO_H_LOCKED;
		lock_cnt++;
	} else if (cvd2->hw.v_lock) {
		cvbs_lock_status = TVAFE_CVBS_VIDEO_V_LOCKED;
		lock_cnt++;
	}
	if (lock_cnt >= 1)
		cvbs_lock_status = TVAFE_CVBS_VIDEO_HV_LOCKED;
	return cvbs_lock_status;
}

/*
 * tvafe cvd2 get cvd2 tv format
 */

int tvafe_cvd2_get_atv_format(void)
{
	int format;

	format = R_APB_REG(CVD2_STATUS_REGISTER3)&0x7;
	return format;
}
EXPORT_SYMBOL(tvafe_cvd2_get_atv_format);

int tvafe_cvd2_get_hv_lock(void)
{
	int lock_status;

	lock_status = R_APB_REG(CVD2_STATUS_REGISTER1)&0x6;
	return lock_status;
}
EXPORT_SYMBOL(tvafe_cvd2_get_hv_lock);

/*
 * tvafe cvd2 non-standard signal detection
 */
static void tvafe_cvd2_non_std_signal_det(
				struct tvafe_cvd2_s *cvd2)
{
	unsigned short dgain = 0;
	unsigned long chroma_sum_filt_tmp = 0;
	unsigned long chroma_sum_filt = 0;
	unsigned long chroma_sum_in = 0;


	chroma_sum_in = rd_bits(0, VDIN_HIST_CHROMA_SUM,
				HIST_CHROMA_SUM_BIT,  HIST_CHROMA_SUM_WID);
	chroma_sum_pre3 = chroma_sum_pre2;
	chroma_sum_pre2 = chroma_sum_pre1;
	chroma_sum_pre1 = chroma_sum_in;

	chroma_sum_filt_tmp = (chroma_sum_pre3 +
				(chroma_sum_pre2 << 1) + chroma_sum_pre1) >> 2;
	chroma_sum_filt = chroma_sum_filt_tmp;

	if (chroma_sum_filt >= SCENE_COLORFUL_TH)
		scene_colorful = 1;
	else
		scene_colorful = 0;

	if (print_cnt == 0x50)
		print_cnt = 0;
	else
		print_cnt = print_cnt + 1;

	if (print_cnt == 0x28) {
		if (cvd_nonstd_dbg_en)
			tvafe_pr_info("%s: scene_colorful = %d, chroma_sum_filt = %ld\n",
			__func__, scene_colorful, chroma_sum_filt);
	}

	if ((cvd2->hw.h_nonstd | (cvd2->hw.v_nonstd && scene_colorful)) &&
		(nonstd_cnt < CVD2_NONSTD_CNT_INC_LIMIT)) {

		nonstd_cnt = nonstd_cnt + CVD2_NONSTD_CNT_INC_STEP;

	} else if ((!cvd2->hw.h_nonstd) && (!cvd2->hw.v_nonstd) &&
			(nonstd_cnt >= CVD2_NONSTD_CNT_DEC_LIMIT)) {

		nonstd_cnt = nonstd_cnt - CVD2_NONSTD_CNT_DEC_STEP;

	}

	if (nonstd_cnt <= CVD2_NONSTD_FLAG_OFF_TH)
		nonstd_flag = 0;
	else if (nonstd_cnt >= CVD2_NONSTD_FLAG_ON_TH)
		nonstd_flag = 1;

	if ((cvd2->config_fmt == TVIN_SIG_FMT_CVBS_PAL_I) && cvd2->hw.line625) {

		dgain = R_APB_BIT(CVD2_AGC_GAIN_STATUS_7_0,
				AGC_GAIN_7_0_BIT, AGC_GAIN_7_0_WID);
		dgain |= R_APB_BIT(CVD2_AGC_GAIN_STATUS_11_8,
				AGC_GAIN_11_8_BIT, AGC_GAIN_11_8_WID)<<8;
		if ((dgain >= TVAFE_CVD2_NONSTD_DGAIN_MAX) ||
				cvd2->hw.h_nonstd || nonstd_flag){

			cvd2->info.non_std_enable = 1;

		} else {

			cvd2->info.non_std_enable = 0;
		}
	}
}

/*
 * tvafe cvd2 signal unstable
 */
static bool tvafe_cvd2_sig_unstable(struct tvafe_cvd2_s *cvd2)
{
	bool ret = false;
#if 1
	if (cvd2->hw.no_sig)
		ret = true;
#else
	if (cvd2->vd_port == TVIN_PORT_CVBS0) {

		if (cvd2->hw.no_sig)/* || !cvd2->hw.h_lock) */
			ret = true;

	} else {

		if (cvd2->hw.no_sig || !cvd2->hw.h_lock || !cvd2->hw.v_lock)
			ret = true;
	}
#endif
	return ret;
}

/*
 * tvafe cvd2 checkt current format match condition
 */
static bool tvafe_cvd2_condition_shift(struct tvafe_cvd2_s *cvd2)
{
	bool ret = false;

	/* check non standard signal, ignore SECAM/525 mode */
	if (!tvafe_cvd2_sig_unstable(cvd2))
		tvafe_cvd2_non_std_signal_det(cvd2);

	if (cvd2->manual_fmt)
		return false;

	if (tvafe_cvd2_sig_unstable(cvd2)) {

		if (cvd_dbg_en)
			tvafe_pr_info("%s: sig unstable, nosig:%d,h-lock:%d,v-lock:%d\n",
			__func__,
			cvd2->hw.no_sig, cvd2->hw.h_lock, cvd2->hw.v_lock);
		return true;
	}

	/* check line flag */
		switch (cvd2->config_fmt) {

		case TVIN_SIG_FMT_CVBS_PAL_I:
		case TVIN_SIG_FMT_CVBS_PAL_CN:
		case TVIN_SIG_FMT_CVBS_SECAM:
		case TVIN_SIG_FMT_CVBS_NTSC_50:
			if (!cvd2->hw.line625) {

				ret = true;
				cvd2->fmt_loop_cnt = 0;
				if (cvd_dbg_en)
					tvafe_pr_info("%s: reset fmt try cnt 525 line\n",
					__func__);
			}
			break;
		case TVIN_SIG_FMT_CVBS_PAL_M:
		case TVIN_SIG_FMT_CVBS_NTSC_443:
		case TVIN_SIG_FMT_CVBS_PAL_60:
		case TVIN_SIG_FMT_CVBS_NTSC_M:
			if (cvd2->hw.line625) {

				ret = true;
				cvd2->fmt_loop_cnt = 0;
				if (cvd_dbg_en)
					tvafe_pr_info("%s: reset fmt try cnt 625 line\n",
					__func__);
			}
			break;
		default:
			break;
	}
	if (ret) {

		if (cvd_dbg_en)
			tvafe_pr_info("%s: line625 error!!!!\n", __func__);
		return true;
	}
	if (cvd2->hw.no_color_burst) {

		/* for SECAM format, set PAL_I */
		if (cvd2->config_fmt != TVIN_SIG_FMT_CVBS_SECAM) {
			/* set default fmt */

			if (!cvd_pr_flag && cvd_dbg_en) {

				cvd_pr_flag = true;
				tvafe_pr_info("%s: no-color-burst, do not change mode.\n",
					__func__);
			}
			return false;
		}
	}
	/* ignore pal flag because of cdto adjustment */
	if ((cvd2->info.non_std_worst || cvd2->hw.h_nonstd) &&
			(cvd2->config_fmt == TVIN_SIG_FMT_CVBS_PAL_I)) {

		if (!cvd_pr_flag && cvd_dbg_en) {

			cvd_pr_flag = true;
			tvafe_pr_info("%s: if adj cdto or h-nonstd, ignore mode change.\n",
				__func__);
		}
		return false;
	}

	/* for ntsc-m pal-m switch bug */
	if ((!cvd2->hw.chroma_lock) &&
		(cvd2->config_fmt == TVIN_SIG_FMT_CVBS_NTSC_M)) {

		if (cvd2->info.ntsc_switch_cnt++ >= NTSC_SW_MAXCNT)

			cvd2->info.ntsc_switch_cnt = 0;


		if (cvd2->info.ntsc_switch_cnt <= NTSC_SW_MIDCNT) {

			if (R_APB_BIT(CVD2_CHROMA_DTO_INCREMENT_23_16,
			CDTO_INC_23_16_BIT, CDTO_INC_23_16_WID) != 0x2e){

				W_APB_BIT(CVD2_CHROMA_DTO_INCREMENT_23_16, 0x2e,
					CDTO_INC_23_16_BIT, CDTO_INC_23_16_WID);
				W_APB_BIT(CVD2_PAL_DETECTION_THRESHOLD, 0x40,
					PAL_DET_TH_BIT, PAL_DET_TH_WID);
				W_APB_BIT(CVD2_CONTROL0, 0x00,
					COLOUR_MODE_BIT, COLOUR_MODE_WID);
				W_APB_BIT(CVD2_COMB_FILTER_CONFIG, 0x2,
					PALSW_LVL_BIT, PALSW_LVL_WID);
				W_APB_BIT(CVD2_COMB_LOCK_CONFIG, 0x7,
			LOSE_CHROMALOCK_LVL_BIT, LOSE_CHROMALOCK_LVL_WID);
				W_APB_BIT(CVD2_PHASE_OFFSE_RANGE, 0x20,
			PHASE_OFFSET_RANGE_BIT, PHASE_OFFSET_RANGE_WID);
			}
			if (!cvd_pr1_chroma_flag && cvd_dbg_en) {

				cvd_pr1_chroma_flag = true;
				cvd_pr2_chroma_flag = false;
				tvafe_pr_info("%s: change cdto to ntsc-m\n",
					__func__);
			}

		} else {

			if (R_APB_BIT(CVD2_CHROMA_DTO_INCREMENT_23_16,
				CDTO_INC_23_16_BIT, CDTO_INC_23_16_WID) !=
				0x23){

				W_APB_BIT(CVD2_CHROMA_DTO_INCREMENT_23_16, 0x23,
					CDTO_INC_23_16_BIT, CDTO_INC_23_16_WID);
				W_APB_BIT(CVD2_PAL_DETECTION_THRESHOLD, 0x1f,
					PAL_DET_TH_BIT, PAL_DET_TH_WID);
				W_APB_BIT(CVD2_CONTROL0, 0x02,
					COLOUR_MODE_BIT, COLOUR_MODE_WID);
				W_APB_BIT(CVD2_COMB_FILTER_CONFIG, 3,
					PALSW_LVL_BIT, PALSW_LVL_WID);
				W_APB_BIT(CVD2_COMB_LOCK_CONFIG, 2,
					LOSE_CHROMALOCK_LVL_BIT,
					LOSE_CHROMALOCK_LVL_WID);
				W_APB_BIT(CVD2_PHASE_OFFSE_RANGE, 0x15,
					PHASE_OFFSET_RANGE_BIT,
					PHASE_OFFSET_RANGE_WID);
			}
			if (!cvd_pr2_chroma_flag && cvd_dbg_en) {

				cvd_pr2_chroma_flag = true;
				cvd_pr1_chroma_flag = false;
				tvafe_pr_info("%s: change cdto to pal-m\n",
					__func__);
			}
		}
	}
	if (((cvd2->vd_port == TVIN_PORT_CVBS3) ||
		(cvd2->vd_port == TVIN_PORT_CVBS0)) &&
		force_fmt_flag) {
		if (cvd_dbg_en)
			tvafe_pr_info("[%s]:ignore the pal/358/443 flag and return\n",
			__func__);
		return false;
	}
	if (ignore_pal_nt)

		return false;

	/* check pal/secam flag */
		switch (cvd2->config_fmt) {

		case TVIN_SIG_FMT_CVBS_PAL_I:
		case TVIN_SIG_FMT_CVBS_PAL_CN:
		case TVIN_SIG_FMT_CVBS_PAL_60:
		case TVIN_SIG_FMT_CVBS_PAL_M:
			if (!cvd2->hw.pal)
				ret = true;
			break;
		case TVIN_SIG_FMT_CVBS_SECAM:
			if (!cvd2->hw.secam || !cvd2->hw.secam_detected)
				ret = true;
			break;
		case TVIN_SIG_FMT_CVBS_NTSC_443:
		case TVIN_SIG_FMT_CVBS_NTSC_M:
		case TVIN_SIG_FMT_CVBS_NTSC_50:
			if (cvd2->hw.pal)
				ret = true;
			break;
		default:
			break;
	}
	if (ignore_443_358) {

		if (ret)
			return true;
		else
			return false;
	}
	/*check 358/443*/
		switch (cvd2->config_fmt) {

		case TVIN_SIG_FMT_CVBS_PAL_CN:
		case TVIN_SIG_FMT_CVBS_PAL_M:
		case TVIN_SIG_FMT_CVBS_NTSC_M:
		case TVIN_SIG_FMT_CVBS_NTSC_50:
			if (!cvd2->hw.fsc_358 && cvd2->hw.fsc_443)
				ret = true;
			break;
		case TVIN_SIG_FMT_CVBS_PAL_I:
		case TVIN_SIG_FMT_CVBS_PAL_60:
		case TVIN_SIG_FMT_CVBS_NTSC_443:
			if (cvd2->hw.fsc_358 && !cvd2->hw.fsc_443)
				ret = true;
			break;
		default:
			break;
	}
	if (ret) {

		if (cvd_dbg_en)
			tvafe_pr_info("%s: pal is %d,secam flag is %d, changed.\n",
			__func__, cvd2->hw.pal, cvd2->hw.secam);
		return true;

	} else
		return false;
}

/*due to some cvd falg is invalid,we must force fmt after reach to max-cnt*/
static void cvd_force_config_fmt(struct tvafe_cvd2_s *cvd2,
			struct tvafe_cvd2_mem_s *mem, int config_force_fmt)
{
	/* force to secam */
	if ((cvd2->hw.line625 && (cvd2->hw.secam_detected || cvd2->hw.secam)) ||
		config_force_fmt == 1) {
		tvafe_cvd2_try_format(cvd2, mem, TVIN_SIG_FMT_CVBS_SECAM);
		tvafe_pr_info("[%s]:force the fmt to TVIN_SIG_FMT_CVBS_SECAM\n",
			__func__);
	}
	/* force to ntscm */
	else if ((!cvd2->hw.line625 && !cvd2->hw.pal) ||
		config_force_fmt == 2) {
		tvafe_cvd2_try_format(cvd2, mem, TVIN_SIG_FMT_CVBS_NTSC_M);
		tvafe_pr_info("[%s]:force the fmt to TVIN_SIG_FMT_CVBS_NTSC_M\n",
			__func__);
	}
	/* force to palm */
	else if ((!cvd2->hw.line625 && cvd2->hw.pal) || config_force_fmt == 3) {
		tvafe_cvd2_try_format(cvd2, mem, TVIN_SIG_FMT_CVBS_PAL_M);
		tvafe_pr_info("[%s]:force the fmt to TVIN_SIG_FMT_CVBS_PAL_M\n",
			__func__);
	}
	/* force to pali */
	else if ((cvd2->hw.line625) || config_force_fmt == 4) {
		tvafe_cvd2_try_format(cvd2, mem, TVIN_SIG_FMT_CVBS_PAL_I);
		tvafe_pr_info("[%s]:force the fmt to TVIN_SIG_FMT_CVBS_PAL_I\n",
			__func__);
	}
	return;

}



/*
 * tvafe cvd2 search video format function
 */

static void tvafe_cvd2_search_video_mode(struct tvafe_cvd2_s *cvd2,
			struct tvafe_cvd2_mem_s *mem)
{
	unsigned int shift_cnt = 0;
	/* execute manual mode */
	if ((cvd2->manual_fmt) && (cvd2->config_fmt != cvd2->manual_fmt) &&
		(cvd2->config_fmt != TVIN_SIG_FMT_NULL)) {
		tvafe_cvd2_try_format(cvd2, mem, cvd2->manual_fmt);
	}
	/* state-machine */
	if (cvd2->info.state == TVAFE_CVD2_STATE_INIT) {
		/* wait for signal setup */
		if (tvafe_cvd2_sig_unstable(cvd2)) {
			cvd2->info.state_cnt = 0;
			if (!cvd_pr_flag && cvd_dbg_en) {
				cvd_pr_flag = true;
				tvafe_pr_info("%s: sig unstable,nosig:%d,h-lock:%d,v-lock:%d.\n",
				__func__, cvd2->hw.no_sig, cvd2->hw.h_lock,
				cvd2->hw.v_lock);
			}
		}
		/* wait for signal stable */
		else if (++cvd2->info.state_cnt <= FMT_WAIT_CNT)
			return;
		force_fmt_flag = 0;
		cvd_pr_flag = false;
		cvd2->info.state_cnt = 0;
		/* manual mode =>*/
		/*go directly to the manual format */
		if (cvd2->manual_fmt) {
			try_format_cnt = 0;
			cvd2->info.state = TVAFE_CVD2_STATE_FIND;
			if (cvd_dbg_en)
				tvafe_pr_info("%s: manual fmt is:%s,do not need try other format!!!\n",
				__func__, tvin_sig_fmt_str(cvd2->manual_fmt));
			return;
		}
		/* auto mode */
		if (cvd_dbg_en)
			tvafe_pr_info("%s: switch to fmt:%s,hnon:%d,vnon:%d,c-lk:%d,pal:%d,secam:%d,h-lk:%d,v-lk:%d,fsc358:%d,fsc425:%d,fsc443:%d,secam detected %d,line625:%d\n",
			__func__, tvin_sig_fmt_str(cvd2->config_fmt),
			cvd2->hw.h_nonstd, cvd2->hw.v_nonstd,
			cvd2->hw.chroma_lock, cvd2->hw.pal,
			cvd2->hw.secam, cvd2->hw.h_lock,
			cvd2->hw.v_lock, cvd2->hw.fsc_358,
			cvd2->hw.fsc_425, cvd2->hw.fsc_443,
			cvd2->hw.secam_detected, cvd2->hw.line625);
		if (cvd_dbg_en)
			tvafe_pr_info("acc4xx_cnt = %d,acc425_cnt = %d,acc3xx_cnt = %d,acc358_cnt = %d secam_detected:%d\n",
			cvd2->hw_data[cvd2->hw_data_cur].acc4xx_cnt,
			cvd2->hw_data[cvd2->hw_data_cur].acc425_cnt,
			cvd2->hw_data[cvd2->hw_data_cur].acc3xx_cnt,
			cvd2->hw_data[cvd2->hw_data_cur].acc358_cnt,
			cvd2->hw_data[cvd2->hw_data_cur].secam_detected);
			/* force mode:due to some*/
			/*signal is hard to check out */
		if (++try_format_cnt == TRY_FORMAT_MAX) {
			cvd_force_config_fmt(cvd2, mem, config_force_fmt);
			return;
		} else if (try_format_cnt > TRY_FORMAT_MAX) {
			cvd2->info.state = TVAFE_CVD2_STATE_FIND;
			force_fmt_flag = 1;
			return;
		}
		switch (cvd2->config_fmt) {
		case TVIN_SIG_FMT_CVBS_PAL_I:
			if (cvd2->hw.line625) {
				if (cvd2->hw.secam_detected ||
					cvd2->hw.secam)
					tvafe_cvd2_try_format(cvd2, mem,
					TVIN_SIG_FMT_CVBS_SECAM);
				else if (cvd2->hw.fsc_443 && cvd2->hw.pal) {
					/* 625 +	*/
					/*cordic_match =>*/
					/*confirm PAL_I */
					cvd2->info.state =
					TVAFE_CVD2_STATE_FIND;
				} else if (cvd2->hw.fsc_358) {
					/* 625 + 358 => */
					/*try PAL_CN */
					tvafe_cvd2_try_format(cvd2, mem,
					TVIN_SIG_FMT_CVBS_PAL_CN);
				}
			} else {
				/* 525 lines */
				if (cvd_dbg_en)
					tvafe_pr_info("%s dismatch pal_i line625 %d!!!and the  fsc358 %d,pal %d,fsc_443:%d",
					__func__, cvd2->hw.line625,
					cvd2->hw.fsc_358, cvd2->hw.pal,
					cvd2->hw.fsc_443);
				tvafe_cvd2_try_format(cvd2, mem,
				TVIN_SIG_FMT_CVBS_PAL_M);
				if (cvd_dbg_en)
					tvafe_pr_info("%sdismatch pal_i and after try other format: line625 %d!!!and the  fsc358 %d,pal %d,fsc_443:%d",
					__func__, cvd2->hw.line625,
					cvd2->hw.fsc_358,
					cvd2->hw.pal, cvd2->hw.fsc_443);
			}
			break;
		case TVIN_SIG_FMT_CVBS_PAL_CN:
			if (cvd2->hw.line625 && cvd2->hw.fsc_358 &&
			cvd2->hw.pal){
				/* line625+brust358+pal*/
				/*-> pal_cn */
				cvd2->info.state = TVAFE_CVD2_STATE_FIND;
			} else if (((cvd2->vd_port == TVIN_PORT_CVBS1) ||
				(cvd2->vd_port == TVIN_PORT_CVBS2) || ntsc50_en)
				&& (cvd2->hw.line625 &&
				cvd2->hw.fsc_358 &&
				!cvd2->hw.pal &&
				!cvd2->hw.fsc_443 &&
				!cvd2->hw.secam))
				tvafe_cvd2_try_format(cvd2, mem,
					TVIN_SIG_FMT_CVBS_NTSC_50);
			else {
				if (cvd_dbg_en)
					tvafe_pr_info("%s dismatch pal_cn line625 %d, fsc358 %d,pal %d",
					__func__, cvd2->hw.line625,
					cvd2->hw.fsc_358, cvd2->hw.pal);
				if (cvd2->hw.line625)
					tvafe_cvd2_try_format(cvd2, mem,
					TVIN_SIG_FMT_CVBS_PAL_I);
				else if (!cvd2->hw.line625 &&
				cvd2->hw.fsc_358 &&
				cvd2->hw.pal)
					tvafe_cvd2_try_format(cvd2, mem,
					TVIN_SIG_FMT_CVBS_PAL_M);
				else if (!cvd2->hw.line625 &&
				cvd2->hw.fsc_443 &&
				!cvd2->hw.pal)
					tvafe_cvd2_try_format(cvd2, mem,
					TVIN_SIG_FMT_CVBS_NTSC_443);
				}
			break;
		case TVIN_SIG_FMT_CVBS_NTSC_50:
			if (cvd2->hw.line625 && cvd2->hw.fsc_358 &&
			cvd2->hw.pal)
				tvafe_cvd2_try_format(cvd2, mem,
						TVIN_SIG_FMT_CVBS_PAL_CN);
			else if (cvd2->hw.line625 &&
				cvd2->hw.fsc_358 &&
				!cvd2->hw.pal &&
				!cvd2->hw.fsc_443 &&
				!cvd2->hw.secam)
				cvd2->info.state = TVAFE_CVD2_STATE_FIND;
			else if (cvd2->hw.line625)
				tvafe_cvd2_try_format(cvd2, mem,
					TVIN_SIG_FMT_CVBS_PAL_I);
			else if (!cvd2->hw.line625 &&
			cvd2->hw.fsc_358 &&
			cvd2->hw.pal)
				tvafe_cvd2_try_format(cvd2, mem,
				TVIN_SIG_FMT_CVBS_PAL_M);
			else if (!cvd2->hw.line625 &&
			cvd2->hw.fsc_443 &&
			!cvd2->hw.pal)
				tvafe_cvd2_try_format(cvd2, mem,
				TVIN_SIG_FMT_CVBS_NTSC_443);
			break;
		case TVIN_SIG_FMT_CVBS_SECAM:
			if (cvd2->hw.line625 && cvd2->hw.secam_detected &&
			cvd2->hw.secam){
				/* 625 + secam =>*/
				/*confirm SECAM */
				cvd2->info.state = TVAFE_CVD2_STATE_FIND;
			} else {
			if (cvd_dbg_en)
				tvafe_pr_info("%s dismatch secam line625 %d, secam_detected %d",
				__func__, cvd2->hw.line625,
				cvd2->hw.secam_detected);
			if (cvd2->hw.line625)
				tvafe_cvd2_try_format(cvd2, mem,
				TVIN_SIG_FMT_CVBS_PAL_I);
			else if (!cvd2->hw.line625 && cvd2->hw.fsc_358 &&
			cvd2->hw.pal)
				tvafe_cvd2_try_format(cvd2, mem,
				TVIN_SIG_FMT_CVBS_PAL_M);
			else if (!cvd2->hw.line625 && cvd2->hw.fsc_443 &&
			!cvd2->hw.pal)
				tvafe_cvd2_try_format(cvd2, mem,
				TVIN_SIG_FMT_CVBS_NTSC_443);
			}
			break;
		case TVIN_SIG_FMT_CVBS_PAL_M:
			if (!cvd2->hw.line625 && cvd2->hw.fsc_358 &&
			cvd2->hw.pal && cvd2->hw.chroma_lock){
				/* line525 + 358 + pal */
				/* => confirm PAL_M */
				cvd2->info.state = TVAFE_CVD2_STATE_FIND;
			} else {
			if (cvd_dbg_en)
				tvafe_pr_info("%s dismatch pal m line625 %d, fsc358 %d,pal %d",
				__func__, cvd2->hw.line625,
				cvd2->hw.fsc_358, cvd2->hw.pal);
			if (cvd2->hw.line625 && cvd2->hw.fsc_358 &&
			cvd2->hw.pal)
				tvafe_cvd2_try_format(cvd2, mem,
				TVIN_SIG_FMT_CVBS_PAL_CN);
			else if (cvd2->hw.line625)
				tvafe_cvd2_try_format(cvd2, mem,
				TVIN_SIG_FMT_CVBS_PAL_I);
			else if (!cvd2->hw.line625 && cvd2->hw.fsc_443 &&
			cvd2->hw.pal)
				tvafe_cvd2_try_format(cvd2, mem,
				TVIN_SIG_FMT_CVBS_PAL_60);
			else if (!cvd2->hw.line625 && cvd2->hw.fsc_358 &&
				!cvd2->hw.pal)
				tvafe_cvd2_try_format(cvd2, mem,
				TVIN_SIG_FMT_CVBS_NTSC_M);
			else if (!cvd2->hw.line625 && cvd2->hw.fsc_443 &&
				!cvd2->hw.pal)
				tvafe_cvd2_try_format(cvd2, mem,
				TVIN_SIG_FMT_CVBS_NTSC_443);
			}
			break;
		case TVIN_SIG_FMT_CVBS_NTSC_M:
			if (!cvd2->hw.line625 && cvd2->hw.fsc_358 &&
				!cvd2->hw.pal && cvd2->hw.chroma_lock){
				/* line525 + 358 =>*/
				/*confirm NTSC_M */
				cvd2->info.state = TVAFE_CVD2_STATE_FIND;
			} else {
				if (cvd_dbg_en)
					tvafe_pr_info("%s dismatch ntsc m line625 %d, fsc358 %d,pal %d",
					__func__, cvd2->hw.line625,
					cvd2->hw.fsc_358, cvd2->hw.pal);
				tvafe_cvd2_try_format(cvd2, mem,
				TVIN_SIG_FMT_CVBS_PAL_I);
			}
			break;
		case TVIN_SIG_FMT_CVBS_PAL_60:
			if (!cvd2->hw.line625 && cvd2->hw.fsc_443 &&
				cvd2->hw.pal)
				/* 525 + 443 + pal => */
				/*confirm PAL_60 */
				cvd2->info.state = TVAFE_CVD2_STATE_FIND;
			else{
				/* set default to pal i */
				if (cvd_dbg_en)
					tvafe_pr_info("%s dismatch pal 60 line625 %d, fsc443 %d,pal %d",
					__func__, cvd2->hw.line625,
					cvd2->hw.fsc_443, cvd2->hw.pal);
				tvafe_cvd2_try_format(cvd2, mem,
				TVIN_SIG_FMT_CVBS_PAL_I);
			}
			break;
		case TVIN_SIG_FMT_CVBS_NTSC_443:
			if (!cvd2->hw.line625 && cvd2->hw.fsc_443 &&
				!cvd2->hw.pal)
				/* 525 + 443 => */
				/*confirm NTSC_443 */
				cvd2->info.state =
				TVAFE_CVD2_STATE_FIND;
			else{
				/* set default to pal i */
				if (cvd_dbg_en)
					tvafe_pr_info("%s dismatch NTSC_443 line625 %d, fsc443 %d,pal %d",
					__func__, cvd2->hw.line625,
					cvd2->hw.fsc_443, cvd2->hw.pal);
				if (!cvd2->hw.line625 &&
				cvd2->hw.fsc_443 && cvd2->hw.pal)
					tvafe_cvd2_try_format(cvd2, mem,
					TVIN_SIG_FMT_CVBS_PAL_60);
				else if (!cvd2->hw.line625) {
					cvd_force_config_fmt(cvd2, mem,
					config_force_fmt);
					try_format_cnt =
						TRY_FORMAT_MAX+1;
				} else
					tvafe_cvd2_try_format(cvd2, mem,
					TVIN_SIG_FMT_CVBS_PAL_I);
			}
			break;
		default:
			break;
		}
		if (cvd_dbg_en)
			tvafe_pr_info("%s: current fmt is:%s\n",
			__func__, tvin_sig_fmt_str(cvd2->config_fmt));
	} else if (cvd2->info.state == TVAFE_CVD2_STATE_FIND) {
		/* manual mode => go directly to the manual format */
		try_format_cnt = 0;
		if (tvafe_cvd2_condition_shift(cvd2)) {
			shift_cnt = cvd2_shift_cnt;
			if (cvd2->info.non_std_enable)
				shift_cnt = cvd2_shift_cnt*10;
			/* if no color burst,*/
			/*pal flag can not be trusted */
			if (cvd2->info.fmt_shift_cnt++ > shift_cnt) {
				tvafe_cvd2_try_format(
				cvd2, mem, TVIN_SIG_FMT_CVBS_PAL_I);
				cvd2->info.state = TVAFE_CVD2_STATE_INIT;
				cvd2->info.ntsc_switch_cnt = 0;
				try_format_cnt = 0;
				cvd_pr_flag = false;
			}
		}
		/* non-standard signal config */
		tvafe_cvd2_non_std_config(cvd2);
	}
}


#ifdef TVAFE_CVD2_AUTO_DE_ENABLE
static void tvafe_cvd2_auto_de(struct tvafe_cvd2_s *cvd2)
{
	struct tvafe_cvd2_lines_s *lines = &cvd2->info.vlines;
	unsigned int i = 0, l_ave = 0, l_max = 0, l_min = 0xff, tmp = 0;

	if (!cvd2->hw.line625 || (cvd2->config_fmt != TVIN_SIG_FMT_CVBS_PAL_I))
		return;
	lines->val[0] = lines->val[1];
	lines->val[1] = lines->val[2];
	lines->val[2] = lines->val[3];
	lines->val[3] = R_APB_REG(CVD2_REG_E6);
	for (i = 0; i < 4; i++) {
		if (l_max < lines->val[i])
			l_max = lines->val[i];
		if (l_min > lines->val[i])
			l_min = lines->val[i];
		l_ave += lines->val[i];
	}
	if (lines->check_cnt++ == TVAFE_CVD2_AUTO_DE_CHECK_CNT) {
		lines->check_cnt = 0;
		/* if (cvd_dbg_en) */
	/* tvafe_pr_info("%s: check lines every 100*10ms\n", __func__); */
		l_ave = (l_ave - l_max - l_min + 1) >> 1;
		/* get the average value */
		if (l_ave > TVAFE_CVD2_AUTO_DE_TH) {
			tmp = (0xff - l_ave + 1) >> 2;
			/* avoid overflow */
			if (tmp > TVAFE_CVD2_PAL_DE_START)
				tmp = TVAFE_CVD2_PAL_DE_START;
			if (lines->de_offset != tmp || scene_colorful_old) {
				lines->de_offset = tmp;
				tmp = ((TVAFE_CVD2_PAL_DE_START -
					lines->de_offset) << 16) |
					(288 + TVAFE_CVD2_PAL_DE_START -
					lines->de_offset);
				W_APB_REG(ACD_REG_2E, tmp);
				scene_colorful_old = 0;
				if (cvd_dbg_en)
					tvafe_pr_info("%s: vlines:%d, de_offset:%d tmp:%x\n",
				__func__, l_ave, lines->de_offset, tmp);
			}
		} else {
			if (lines->de_offset > 0) {
				tmp = ((TVAFE_CVD2_PAL_DE_START -
					lines->de_offset) << 16) |
					(288 + TVAFE_CVD2_PAL_DE_START -
					lines->de_offset);
				W_APB_REG(ACD_REG_2E, tmp);
				scene_colorful_old = 0;
				if (cvd_dbg_en)
					tvafe_pr_info("%s: vlines:%d, de_offset:%d tmp:%x\n",
					__func__, l_ave, lines->de_offset, tmp);
				lines->de_offset--;
			}
		}
	}
}
/* vlis advice new add @20170329 */
static void tvafe_cvd2_adj_vs(struct tvafe_cvd2_s *cvd2)
{
	struct tvafe_cvd2_lines_s *lines = &cvd2->info.vlines;
	unsigned int i = 0, l_ave = 0, l_max = 0, l_min = 0xff;

	if (!cvd2->hw.line625 ||
		((cvd2->config_fmt != TVIN_SIG_FMT_CVBS_PAL_I) &&
		(cvd2->config_fmt != TVIN_SIG_FMT_CVBS_NTSC_M)))
		return;
	if (auto_de_en == 0) {
		lines->val[0] = lines->val[1];
		lines->val[1] = lines->val[2];
		lines->val[2] = lines->val[3];
		lines->val[3] = R_APB_REG(CVD2_REG_E6);
	}
	for (i = 0; i < 4; i++) {
		if (l_max < lines->val[i])
			l_max = lines->val[i];
		if (l_min > lines->val[i])
			l_min = lines->val[i];
		l_ave += lines->val[i];
	}
	if (lines->check_cnt++ == TVAFE_CVD2_AUTO_DE_CHECK_CNT)
		lines->check_cnt = TVAFE_CVD2_AUTO_DE_CHECK_CNT;
	if (lines->check_cnt == TVAFE_CVD2_AUTO_DE_CHECK_CNT) {
		l_ave = (l_ave - l_max - l_min + 1) >> 1;
		if (l_ave > TVAFE_CVD2_AUTO_VS_TH) {
			cvd2->info.vs_adj_en = 1;
			/*vlsi test result*/
			/*0x3 for test colobar pattern*/
			if (cvd2->info.hs_adj_en == 0)
				W_APB_REG(CVD2_CHROMA_LOOPFILTER_STATE, 0x3);
			else
				W_APB_REG(CVD2_CHROMA_LOOPFILTER_STATE, 0xa);
		} else {
			cvd2->info.vs_adj_en = 0;
			if (R_APB_REG(CVD2_CHROMA_LOOPFILTER_STATE) != 0xa)
				W_APB_REG(CVD2_CHROMA_LOOPFILTER_STATE, 0xa);
		}
		/* get the average value */
		if ((l_ave > vs_adj_th_level0) || (l_ave < vs_adj_th_level00))
			cvd2->info.vs_adj_level = 0;
		else if ((l_ave > vs_adj_th_level1) ||
			(l_ave < vs_adj_th_level01))
			cvd2->info.vs_adj_level = 1;
		else if ((l_ave > vs_adj_th_level2) ||
			(l_ave < vs_adj_th_level02))
			cvd2->info.vs_adj_level = 2;
		else if ((l_ave > vs_adj_th_level3) ||
			(l_ave < vs_adj_th_level03))
			cvd2->info.vs_adj_level = 3;
		else if ((l_ave > vs_adj_th_level4) ||
			(l_ave < vs_adj_th_level04))
			cvd2->info.vs_adj_level = 4;
		else
			cvd2->info.vs_adj_level = 0xff;
	}
}
#endif
/*set default de according to config fmt*/
void tvafe_cvd2_set_default_de(struct tvafe_cvd2_s *cvd2)
{
	if (scene_colorful_old == 1)
		return;
#ifdef TVAFE_CVD2_AUTO_DE_ENABLE
	if (!cvd2) {

		tvafe_pr_info("%s error.\n", __func__);
		return;
	}
	/*write default de to register*/
	W_APB_REG(ACD_REG_2E,  (rf_acd_table[cvd2->config_fmt-
				TVIN_SIG_FMT_CVBS_NTSC_M][0x2e]));
	if (cvd_dbg_en)
		tvafe_pr_info("%s set default de %s.\n",
				__func__, tvin_sig_fmt_str(cvd2->config_fmt));
	scene_colorful_old = 1;
#endif
}

/*
 * tvafe cvd2 init if no signal input
 */
static void tvafe_cvd2_reinit(struct tvafe_cvd2_s *cvd2)
{
	if (cvd2->cvd2_init_en)
		return;

#ifdef TVAFE_SET_CVBS_CDTO_EN
	if ((tvafe_cvd2_get_cdto() != CVD2_CHROMA_DTO_PAL_I) &&
			(cvd2->config_fmt == TVIN_SIG_FMT_CVBS_PAL_I)) {

		tvafe_cvd2_set_cdto(CVD2_CHROMA_DTO_PAL_I);
		if (cvd_dbg_en)
			tvafe_pr_info("%s: set default cdto.\n", __func__);

	}
	#endif
	/* reset pga value */
#ifdef TVAFE_SET_CVBS_PGA_EN
	tvafe_cvd2_reset_pga();
#endif
	/* init variable */
	memset(&cvd2->info, 0, sizeof(struct tvafe_cvd2_info_s));
	cvd2->cvd2_init_en = true;

	if (cvd_dbg_en)
		tvafe_pr_info("%s: reinit cvd2.\n", __func__);
}

/*
 * tvafe cvd2 signal status
 */
inline bool tvafe_cvd2_no_sig(struct tvafe_cvd2_s *cvd2,
			struct tvafe_cvd2_mem_s *mem)
{
	static bool ret;
	static int time_flag;

	tvafe_cvd2_get_signal_status(cvd2);

	/*TVAFE register status need more time to be stable.*/
	/*for double time delay.*/
	time_flag++;
	if (time_flag%2 != 0)
		return ret;

	/* get signal status from HW */

	/* search video mode */
	tvafe_cvd2_search_video_mode(cvd2, mem);

	/* init if no signal input */
	if (cvd2->hw.no_sig) {

		ret = true;
		tvafe_cvd2_reinit(cvd2);

	} else {
		ret = false;
		cvd2->cvd2_init_en = false;
#ifdef TVAFE_CVD2_AUTO_DE_ENABLE
	if (((!scene_colorful) && auto_de_en) || auto_vs_en) {
		if (auto_de_en)
			tvafe_cvd2_auto_de(cvd2);
		if (auto_vs_en)
			tvafe_cvd2_adj_vs(cvd2);
	} else
		tvafe_cvd2_set_default_de(cvd2);
#endif
	}
	if (ret && try_format_cnt) {

		try_format_cnt = 0;
		if (cvd_dbg_en)
			tvafe_pr_info("%s: initialize try_format_cnt to zero.\n",
					__func__);
	}
	return ret;
}

/*
 * tvafe cvd2 mode change status
 */
inline bool tvafe_cvd2_fmt_chg(struct tvafe_cvd2_s *cvd2)
{
	if (cvd2->info.state == TVAFE_CVD2_STATE_FIND)
		return false;
	else
		return true;
}

/*
 * tvafe cvd2 find the configured format
 */
inline enum tvin_sig_fmt_e tvafe_cvd2_get_format(
		struct tvafe_cvd2_s *cvd2)
{
	if (cvd2->info.state == TVAFE_CVD2_STATE_FIND)
		return cvd2->config_fmt;
	else
		return TVIN_SIG_FMT_NULL;
}

#ifdef TVAFE_SET_CVBS_PGA_EN
/*
 * tvafe cvd2 pag ajustment in vsync interval
 */
inline void tvafe_cvd2_adj_pga(struct tvafe_cvd2_s *cvd2)
{
	unsigned short dg_max = 0, dg_min = 0xffff, dg_ave = 0, i = 0, pga = 0;
	unsigned int tmp = 0;
	unsigned int step = 0;
	unsigned int delta_dg = 0;

	if ((cvd_isr_en & 0x100) == 0)
		return;

	cvd2->info.dgain[0] = cvd2->info.dgain[1];
	cvd2->info.dgain[1] = cvd2->info.dgain[2];
	cvd2->info.dgain[2] = cvd2->info.dgain[3];
	cvd2->info.dgain[3] = R_APB_BIT(CVD2_AGC_GAIN_STATUS_7_0,
			AGC_GAIN_7_0_BIT, AGC_GAIN_7_0_WID);
	cvd2->info.dgain[3] |= R_APB_BIT(CVD2_AGC_GAIN_STATUS_11_8,
			AGC_GAIN_11_8_BIT, AGC_GAIN_11_8_WID)<<8;
	for (i = 0; i < 4; i++) {

		if (dg_max < cvd2->info.dgain[i])
			dg_max = cvd2->info.dgain[i];
		if (dg_min > cvd2->info.dgain[i])
			dg_min = cvd2->info.dgain[i];
		dg_ave += cvd2->info.dgain[i];
	}
	if (++cvd2->info.dgain_cnt >=
		(TVAFE_SET_CVBS_PGA_START + pga_step_last))
		cvd2->info.dgain_cnt = TVAFE_SET_CVBS_PGA_START;
	else
		return;
	if (cvd2->info.dgain_cnt == TVAFE_SET_CVBS_PGA_START) {
		cvd2->info.dgain_cnt = 0;
		dg_ave = (dg_ave - dg_max - dg_min + 1) >> 1;
		pga = R_APB_BIT(TVFE_VAFE_CTRL1, VAFE_PGA_GAIN_BIT,
			VAFE_PGA_GAIN_WID);
		delta_dg = abs(dg_ave - (signed short)dg_ave_last);
		if (((dg_ave >= CVD2_DGAIN_LIMITL) &&
			(dg_ave <= CVD2_DGAIN_LIMITH)) &&
			(delta_dg < CVD2_DGAIN_WINDOW*2)) {
			return;
		} else if ((dg_ave < CVD2_DGAIN_LIMITL) && (pga == 0)) {
			return;
		} else if ((dg_ave > CVD2_DGAIN_LIMITH) &&
			(pga >= (255+97))) {
			return;
		}
		if (cvd_dbg_en)
			tvafe_pr_info("%s: dg_ave_last:0x%x dg_ave:0x%x. pga 0x%x.\n",
			__func__, dg_ave_last, dg_ave, pga);
		dg_ave_last = dg_ave;

		tmp = abs(dg_ave - (signed short)CVD2_DGAIN_MIDDLE);
		if (tmp > CVD2_DGAIN_MIDDLE)
			step = 16;
		else if (tmp > (CVD2_DGAIN_MIDDLE >> 1))
			step = 5;
		else if (tmp > (CVD2_DGAIN_MIDDLE >> 2))
			step = 2;
		else
			step = 1;
		if ((delta_dg > CVD2_DGAIN_MIDDLE) ||
			((delta_dg == 0) &&
			(tmp > (CVD2_DGAIN_MIDDLE >> 2))))
			step = PGA_DELTA_VAL;
		if (dg_ave > CVD2_DGAIN_LIMITH) {
			pga += step;
			if (pga >= 255)  /* set max value */
				pga = 255;
		} else {
			if (pga < step)  /* set min value */
				pga = 0;
			else
				pga -= step;
		}
		if (pga < 2)
			pga = 2;
		if (pga != R_APB_BIT(TVFE_VAFE_CTRL1,
			VAFE_PGA_GAIN_BIT, VAFE_PGA_GAIN_WID)){
			if (cvd_dbg_en)
				tvafe_pr_info("%s: set pag:0x%x. current dgain 0x%x.\n",
					__func__, pga, cvd2->info.dgain[3]);
			W_APB_BIT(TVFE_VAFE_CTRL1, pga,
			VAFE_PGA_GAIN_BIT, VAFE_PGA_GAIN_WID);
			if (cvd_dbg_en)
				tvafe_pr_info("%s: pga_step_last:0x%x step:0x%x.\n",
					__func__, pga_step_last, step);
		}
		pga_step_last = step;
	}
}
#endif

#ifdef TVAFE_SET_CVBS_CDTO_EN
/*
 * tvafe cvd2 cdto tune in vsync interval
 */
static void tvafe_cvd2_cdto_tune(unsigned int cur, unsigned int dest)
{
	unsigned int diff = 0, step = 0;

	diff = (unsigned int)abs((signed int)cur - (signed int)dest);

	if (diff == 0)

		return;


	if ((diff > (diff>>1)) && (diff > (0x1<<cdto_adj_step)))
		step = diff >> cdto_adj_step;
	else
		step = 1;

	if (cur > dest)
		cur -= step;
	else
		cur += step;

	W_APB_REG(CVD2_CHROMA_DTO_INCREMENT_29_24, (cur >> 24) & 0x0000003f);
	W_APB_REG(CVD2_CHROMA_DTO_INCREMENT_23_16, (cur >> 16) & 0x000000ff);
	W_APB_REG(CVD2_CHROMA_DTO_INCREMENT_15_8,  (cur >>  8) & 0x000000ff);
	W_APB_REG(CVD2_CHROMA_DTO_INCREMENT_7_0,   (cur >>  0) & 0x000000ff);

}
inline void tvafe_cvd2_adj_hs(struct tvafe_cvd2_s *cvd2,
			unsigned int hcnt64)
{
	unsigned int hcnt64_max, hcnt64_min, temp, delta;
	unsigned int diff, hcnt64_ave, i;
	unsigned int hcnt64_standard = 0;

	if (tvafe_cpu_type() >= CPU_TYPE_GXTVBB) {
		if (cvd2->config_fmt == TVIN_SIG_FMT_CVBS_PAL_I)
			hcnt64_standard = 0x31380;
		else if (cvd2->config_fmt == TVIN_SIG_FMT_CVBS_NTSC_M)
			hcnt64_standard = 0x30e0e;
	} else
		hcnt64_standard = 0x17a00;

	if ((cvd_isr_en & 0x1000) == 0)
		return;

	/* only for pal-i adjusment */
	if ((cvd2->config_fmt != TVIN_SIG_FMT_CVBS_PAL_I) &&
		(cvd2->config_fmt != TVIN_SIG_FMT_CVBS_NTSC_M))
		return;

	cvd2->info.hcnt64[0] = cvd2->info.hcnt64[1];
	cvd2->info.hcnt64[1] = cvd2->info.hcnt64[2];
	cvd2->info.hcnt64[2] = cvd2->info.hcnt64[3];
	cvd2->info.hcnt64[3] = hcnt64;
	hcnt64_ave = 0;
	hcnt64_max = 0;
	hcnt64_min = 0xffffffff;
	for (i = 0; i < 4; i++) {
		if (hcnt64_max < cvd2->info.hcnt64[i])
			hcnt64_max = cvd2->info.hcnt64[i];
		if (hcnt64_min > cvd2->info.hcnt64[i])
			hcnt64_min = cvd2->info.hcnt64[i];
		hcnt64_ave += cvd2->info.hcnt64[i];
	}
	if (++cvd2->info.hcnt64_cnt >= 300)
		cvd2->info.hcnt64_cnt = 300;
	if (cvd2->info.hcnt64_cnt == 300) {
		hcnt64_ave = (hcnt64_ave - hcnt64_max - hcnt64_min + 1) >> 1;
		if (hcnt64_ave == 0)  /* to avoid kernel crash */
			return;
		diff = abs(hcnt64_ave - hcnt64_standard);
		if (diff > hs_adj_th_level0) {
			if (R_APB_REG(CVD2_YC_SEPARATION_CONTROL) != 0x11)
				W_APB_REG(CVD2_YC_SEPARATION_CONTROL, 0x11);
			if (R_APB_REG(CVD2_H_LOOP_MAXSTATE) != 0xc)
				W_APB_REG(CVD2_H_LOOP_MAXSTATE, 0xc);
			if (R_APB_REG(CVD2_REG_87) != 0xc0)
				W_APB_REG(CVD2_REG_87, 0xc0);
			cvd2->info.hs_adj_en = 1;
			if (diff > hs_adj_th_level4)
				cvd2->info.hs_adj_level = 4;
			else if (diff > hs_adj_th_level3)
				cvd2->info.hs_adj_level = 3;
			else if (diff > hs_adj_th_level2)
				cvd2->info.hs_adj_level = 2;
			else if (diff > hs_adj_th_level1)
				cvd2->info.hs_adj_level = 1;
			else
				cvd2->info.hs_adj_level = 0;
			if (hcnt64_ave > hcnt64_standard)
				cvd2->info.hs_adj_dir = 1;
			else
				cvd2->info.hs_adj_dir = 0;
			/*@20170420 vlsi adjust new add,optimize for display*/
			if (cvd2->info.hs_adj_dir == 1) {
				/* 0x2e: 0x5c is test result, 0x9c is default */
				temp = (cvd_2e - cvd_2e_l1) *
					cvd2->info.hs_adj_level;
				delta = temp / 4;
				temp = cvd_2e - delta;
				W_APB_BIT(CVD2_ACTIVE_VIDEO_HSTART, temp,
					HACTIVE_START_BIT, HACTIVE_START_WID);
				/* 0x12d */
				temp = delta << 16;
				temp = temp | delta;
				temp = acd_h_back - temp;
				W_APB_REG(ACD_REG_2D, temp);
				acd_h = temp;
			} else {
				/*0x128*/
				temp = (acd_128_l1 - acd_128) *
					cvd2->info.hs_adj_level;
				temp = temp / 4;
				temp = temp + acd_128;
				W_APB_BIT(ACD_REG_28, temp, 16, 5);
				/*0x166 bit31*/
				if (cvd2->info.hs_adj_level > 0)
					W_APB_BIT(ACD_REG_66, 0,
						AML_2DCOMB_EN_BIT,
						AML_2DCOMB_EN_WID);
				/*0x12d, 0x94 is test result, 0x88 is default*/
				temp = (0x94 - 0x88) * cvd2->info.hs_adj_level;
				delta = temp / 4;
				temp = delta << 16;
				temp = temp | delta;
				temp = acd_h_back + temp;
				W_APB_REG(ACD_REG_2D, temp);
				acd_h = temp;
			}
		} else {
			if (R_APB_REG(CVD2_YC_SEPARATION_CONTROL) != 0x12)
				W_APB_REG(CVD2_YC_SEPARATION_CONTROL, 0x12);
			if (R_APB_REG(CVD2_H_LOOP_MAXSTATE) != 0xd)
				W_APB_REG(CVD2_H_LOOP_MAXSTATE, 0xd);
			if (R_APB_REG(CVD2_REG_87) != 0x0)
				W_APB_REG(CVD2_REG_87, 0x0);
			W_APB_REG(ACD_REG_2D, acd_h_back);
			W_APB_BIT(CVD2_ACTIVE_VIDEO_HSTART, cvd_2e,
					HACTIVE_START_BIT, HACTIVE_START_WID);
			W_APB_BIT(ACD_REG_28, acd_128, 16, 5);
			cvd2->info.hs_adj_en = 0;
			cvd2->info.hs_adj_level = 0;
			acd_h = acd_h_back;
		}
	}
}

inline void tvafe_cvd2_adj_hs_ntsc(struct tvafe_cvd2_s *cvd2,
			unsigned int hcnt64)
{
	unsigned int hcnt64_max, hcnt64_min;
	unsigned int diff, hcnt64_ave, i, hcnt64_standard;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXTVBB))
		hcnt64_standard = 0x30e0e;
	else
		hcnt64_standard = 0x17a00;

	if ((cvd_isr_en & 0x1000) == 0)
		return;

	/* only for ntsc-m adjusment */
	if (cvd2->config_fmt != TVIN_SIG_FMT_CVBS_NTSC_M)
		return;

	cvd2->info.hcnt64[0] = cvd2->info.hcnt64[1];
	cvd2->info.hcnt64[1] = cvd2->info.hcnt64[2];
	cvd2->info.hcnt64[2] = cvd2->info.hcnt64[3];
	cvd2->info.hcnt64[3] = hcnt64;
	hcnt64_ave = 0;
	hcnt64_max = 0;
	hcnt64_min = 0xffffffff;
	for (i = 0; i < 4; i++) {
		if (hcnt64_max < cvd2->info.hcnt64[i])
			hcnt64_max = cvd2->info.hcnt64[i];
		if (hcnt64_min > cvd2->info.hcnt64[i])
			hcnt64_min = cvd2->info.hcnt64[i];
		hcnt64_ave += cvd2->info.hcnt64[i];
	}
	if (++cvd2->info.hcnt64_cnt >= 300)
		cvd2->info.hcnt64_cnt = 300;
	if (cvd2->info.hcnt64_cnt == 300) {
		hcnt64_ave = (hcnt64_ave - hcnt64_max - hcnt64_min + 1) >> 1;
		if (hcnt64_ave == 0)  /* to avoid kernel crash */
			return;
		diff = abs(hcnt64_ave - hcnt64_standard);
		if (diff > hs_adj_th_level0) {
			cvd2->info.hs_adj_en = 1;
			if (diff > hs_adj_th_level4)
				cvd2->info.hs_adj_level = 4;
			else if (diff > hs_adj_th_level3)
				cvd2->info.hs_adj_level = 3;
			else if (diff > hs_adj_th_level2)
				cvd2->info.hs_adj_level = 2;
			else if (diff > hs_adj_th_level1)
				cvd2->info.hs_adj_level = 1;
			else
				cvd2->info.hs_adj_level = 0;
			if (hcnt64_ave > hcnt64_standard)
				cvd2->info.hs_adj_dir = 1;
			else
				cvd2->info.hs_adj_dir = 0;
		} else {
			cvd2->info.hs_adj_en = 0;
			cvd2->info.hs_adj_level = 0;
		}
	}
}

/*
 * tvafe cvd2 cdto adjustment in vsync interval
 */
inline void tvafe_cvd2_adj_cdto(struct tvafe_cvd2_s *cvd2,
			unsigned int hcnt64)
{
	unsigned int hcnt64_max = 0, hcnt64_min = 0xffffffff,
				hcnt64_ave = 0, i = 0;
	unsigned int cur_cdto = 0, diff = 0;
	u64 cal_cdto = 0;


	if ((cvd_isr_en & 0x001) == 0)
		return;

	/* only for pal-i adjusment */
	if (cvd2->config_fmt != TVIN_SIG_FMT_CVBS_PAL_I)
		return;

	cvd2->info.hcnt64[0] = cvd2->info.hcnt64[1];
	cvd2->info.hcnt64[1] = cvd2->info.hcnt64[2];
	cvd2->info.hcnt64[2] = cvd2->info.hcnt64[3];
	cvd2->info.hcnt64[3] = hcnt64;
	for (i = 0; i < 4; i++) {
		if (hcnt64_max < cvd2->info.hcnt64[i])
			hcnt64_max = cvd2->info.hcnt64[i];
		if (hcnt64_min > cvd2->info.hcnt64[i])
			hcnt64_min = cvd2->info.hcnt64[i];
		hcnt64_ave += cvd2->info.hcnt64[i];
	}
	if (++cvd2->info.hcnt64_cnt >=
		TVAFE_SET_CVBS_CDTO_START + TVAFE_SET_CVBS_CDTO_STEP){

		cvd2->info.hcnt64_cnt = TVAFE_SET_CVBS_CDTO_START;
	}
	if (cvd2->info.hcnt64_cnt == TVAFE_SET_CVBS_CDTO_START) {

		hcnt64_ave = (hcnt64_ave - hcnt64_max -
					hcnt64_min + 1) >> CDTO_FILTER_FACTOR;
		if (hcnt64_ave == 0)  /* to avoid kernel crash */
			return;
		cal_cdto = CVD2_CHROMA_DTO_PAL_I;
		cal_cdto *= HS_CNT_STANDARD;
		do_div(cal_cdto, hcnt64_ave);

		cur_cdto = tvafe_cvd2_get_cdto();
		diff = (unsigned int)abs((signed int)cal_cdto -
				(signed int)CVD2_CHROMA_DTO_PAL_I);

		if (diff < cdto_adj_th) {

			/* reset cdto to default value */
			if (cur_cdto != CVD2_CHROMA_DTO_PAL_I)
				tvafe_cvd2_cdto_tune(cur_cdto,
					(unsigned int)CVD2_CHROMA_DTO_PAL_I);
			cvd2->info.non_std_worst = 0;
			return;
		}
		cvd2->info.non_std_worst = 1;

		if (cvd_dbg_en)
			tvafe_pr_info("%s: adj cdto from:0x%x to:0x%x\n",
					__func__, (u32)cur_cdto, (u32)cal_cdto);
		tvafe_cvd2_cdto_tune(cur_cdto, (unsigned int)cal_cdto);
	}
}
#endif

#ifdef SYNC_HEIGHT_AUTO_TUNING
/*
 * tvafe cvd2 sync height ajustment for picture quality in vsync interval
 */
static inline void tvafe_cvd2_sync_hight_tune(
		struct tvafe_cvd2_s *cvd2)
{
	int burst_mag = 0;
	int burst_mag_16msb = 0, burst_mag_16lsb = 0;
	unsigned int reg_sync_height = 0;
	int burst_mag_upper_limitation = 0;
	int burst_mag_lower_limitation = 0;
	unsigned int std_sync_height = 0xdd;
	unsigned int cur_div_result = 0;
	unsigned int mult_result = 0;
	unsigned int final_contrast = 0;
	unsigned int reg_contrast_default = 0;

	if ((cvd2->config_fmt == TVIN_SIG_FMT_CVBS_NTSC_M) ||
		(cvd2->config_fmt == TVIN_SIG_FMT_CVBS_PAL_I)) {
		/* try to detect AVin NTSCM/PALI */
		if (cvd2->config_fmt ==
			TVIN_SIG_FMT_CVBS_NTSC_M) {

			burst_mag_upper_limitation =
		NTSC_BURST_MAG_UPPER_LIMIT & 0xffff;
			burst_mag_lower_limitation =
		NTSC_BURST_MAG_LOWER_LIMIT & 0xffff;
			reg_contrast_default = 0x7b;

		} else if (cvd2->config_fmt ==
		TVIN_SIG_FMT_CVBS_PAL_I) {

			burst_mag_upper_limitation =
		PAL_BURST_MAG_UPPER_LIMIT & 0xffff;
			burst_mag_lower_limitation =
		PAL_BURST_MAG_LOWER_LIMIT & 0xffff;
			reg_contrast_default = 0x7d;
		}

		burst_mag_16msb = R_APB_REG(CVD2_STATUS_BURST_MAGNITUDE_LSB);
		burst_mag_16lsb = R_APB_REG(CVD2_STATUS_BURST_MAGNITUDE_MSB);
		burst_mag = ((burst_mag_16msb&0xff) << 8) |
				(burst_mag_16lsb&0xff);
		if (burst_mag > burst_mag_upper_limitation) {

			reg_sync_height = R_APB_REG(CVD2_LUMA_AGC_VALUE);
			if (reg_sync_height > SYNC_HEIGHT_LOWER_LIMIT) {

				reg_sync_height = reg_sync_height - 1;
				W_APB_REG(CVD2_LUMA_AGC_VALUE,
					reg_sync_height&0xff);

				cur_div_result = std_sync_height << 16;
				do_div(cur_div_result, reg_sync_height);
				mult_result = cur_div_result *
					(reg_contrast_default&0xff);
				final_contrast = (mult_result + 0x8000) >> 16;
				if (final_contrast > 0xff)
					W_APB_REG(CVD2_LUMA_CONTRAST_ADJUSTMENT,
					0xff);
				else if (final_contrast > 0x50)
					W_APB_REG(CVD2_LUMA_CONTRAST_ADJUSTMENT,
							final_contrast&0xff);
			}

		} else if (burst_mag < burst_mag_lower_limitation) {

			reg_sync_height = R_APB_REG(CVD2_LUMA_AGC_VALUE);
			if (reg_sync_height < SYNC_HEIGHT_UPPER_LIMIT) {

				reg_sync_height = reg_sync_height + 1;
				W_APB_REG(CVD2_LUMA_AGC_VALUE,
					reg_sync_height&0xff);
				cur_div_result = std_sync_height << 16;
				do_div(cur_div_result, reg_sync_height);
				mult_result = cur_div_result *
					(reg_contrast_default&0xff);
				final_contrast = (mult_result + 0x8000) >> 16;
				if (final_contrast > 0xff)
					W_APB_REG(CVD2_LUMA_CONTRAST_ADJUSTMENT,
						0xff);
				else if (final_contrast > 0x50)
					W_APB_REG(CVD2_LUMA_CONTRAST_ADJUSTMENT,
							final_contrast&0xff);
			}
		}
	}
}
#endif

/*
 * tvafe cvd2 3d comb error checking in vsync interval
 */
inline void tvafe_cvd2_check_3d_comb(struct tvafe_cvd2_s *cvd2)
{
	unsigned int cvd2_3d_status = R_APB_REG(CVD2_REG_95);

	if ((cvd_isr_en & 0x010) == 0)
		return;

#ifdef SYNC_HEIGHT_AUTO_TUNING
	tvafe_cvd2_sync_hight_tune(cvd2);
#endif

	if (cvd2->info.comb_check_cnt++ > CVD_3D_COMB_CHECK_MAX_CNT)


		cvd2->info.comb_check_cnt = 0;

	if (cvd2_3d_status & 0x1ffff) {

		W_APB_BIT(CVD2_REG_B2, 1, COMB2D_ONLY_BIT, COMB2D_ONLY_WID);
		W_APB_BIT(CVD2_REG_B2, 0, COMB2D_ONLY_BIT, COMB2D_ONLY_WID);
		/* if (cvd_dbg_en) */
		/* tvafe_pr_info("%s: reset 3d comb  sts:0x%x\n", */
		/*__func__, cvd2_3d_status); */
	}
}

/* tvafe cvd2 set reset*/
/* vbi disable*/
/* vbi disagent*/
/* 3dcomb disagent*/
/* ddr rw disable*/
void tvafe_cvd2_hold_rst(void)
{
	pr_info("[tvafe..] %s.\n", __func__);

	W_APB_BIT(ACD_REG_22, 1, 25, 1);
	W_APB_BIT(CVD2_VBI_FRAME_CODE_CTL, 0, 0, 1);
	W_APB_BIT(CVD2_REG_B2, 1, 7, 1);
	msleep(30);
	W_APB_BIT(ACD_REG_22, 1, 24, 1);
	W_APB_BIT(ACD_REG_5C, 0, 0, 1);
	W_APB_BIT(CVD2_RESET_REGISTER, 1, SOFT_RST_BIT, SOFT_RST_WID);
}

void tvafe_cvd2_set_reg8a(unsigned int v)
{
	cvd_reg8a = v;
	W_APB_REG(CVD2_CHROMA_LOOPFILTER_STATE, cvd_reg8a);
}

void tvafe_cvd2_rf_ntsc50_en(bool v)
{
	ntsc50_en = v;
}

void tvafe_snow_config(unsigned int onoff)
{
	if (tvafe_snow_function_flag == 0 ||
		tvafe_cpu_type() == CPU_TYPE_TXHD ||
		tvafe_cpu_type() == CPU_TYPE_TL1)
		return;
	if (onoff)
		W_APB_BIT(CVD2_OUTPUT_CONTROL, 3, BLUE_MODE_BIT, BLUE_MODE_WID);
	else
		W_APB_BIT(CVD2_OUTPUT_CONTROL, 0, BLUE_MODE_BIT, BLUE_MODE_WID);
}

void tvafe_snow_config_clamp(unsigned int onoff)
{
	if (tvafe_cpu_type() == CPU_TYPE_TXHD ||
		tvafe_cpu_type() == CPU_TYPE_TL1) {
		if (onoff)
			vdin_adjust_tvafesnow_brightness();
		return;
	}
	if (tvafe_snow_function_flag == 0)
		return;
	if (onoff)
		W_APB_BIT(TVFE_ATV_DMD_CLP_CTRL, 0, 20, 1);
	else
		W_APB_BIT(TVFE_ATV_DMD_CLP_CTRL, 1, 20, 1);
}
/*only for pal-i*/
void tvafe_snow_config_acd(void)
{
	if (tvafe_snow_function_flag == 0)
		return;
	/*0x8e035e is debug test result*/
	if (acd_h_config)
		W_APB_REG(ACD_REG_2D, acd_h_config);
	acd_h = acd_h_back;
}
/*only for pal-i*/
void tvafe_snow_config_acd_resume(void)
{
	if (tvafe_snow_function_flag == 0)
		return;
	/*@todo,0x880358 must be same with cvbs_acd_table/rf_acd_table*/
	if (R_APB_REG(ACD_REG_2D) != acd_h)
		W_APB_REG(ACD_REG_2D, acd_h);
}

enum tvin_aspect_ratio_e tvafe_cvd2_get_wss(void)
{
	unsigned int full_format = 0;
	enum tvin_aspect_ratio_e aspect_ratio = TVIN_ASPECT_NULL;

	full_format = R_APB_REG(CVD2_VBI_WSS_DATA1);

	if (full_format == TVIN_AR_14x9_LB_CENTER_VAL)
		aspect_ratio = TVIN_ASPECT_14x9_LB_CENTER;
	else if (full_format == TVIN_AR_14x9_LB_TOP_VAL)
		aspect_ratio = TVIN_ASPECT_14x9_LB_TOP;
	else if (full_format == TVIN_AR_16x9_LB_TOP_VAL)
		aspect_ratio = TVIN_ASPECT_16x9_LB_TOP;
	else if (full_format == TVIN_AR_16x9_FULL_VAL)
		aspect_ratio = TVIN_ASPECT_16x9_FULL;
	else if (full_format == TVIN_AR_4x3_FULL_VAL)
		aspect_ratio = TVIN_ASPECT_4x3_FULL;
	else if (full_format == TVIN_AR_16x9_LB_CENTER_VAL)
		aspect_ratio = TVIN_ASPECT_16x9_LB_CENTER;
	else if (full_format == TVIN_AR_16x9_LB_CENTER1_VAL)
		aspect_ratio = TVIN_ASPECT_16x9_LB_CENTER;
	else if (full_format == TVIN_AR_14x9_FULL_VAL)
		aspect_ratio = TVIN_ASPECT_14x9_FULL;
	else
		aspect_ratio = TVIN_ASPECT_NULL;

	return aspect_ratio;
}

/*only for develop debug*/
#ifdef TVAFE_CVD_DEBUG
module_param(hs_adj_th_level0, uint, 0664);
MODULE_PARM_DESC(hs_adj_th_level0, "hs_adj_th_level0");

module_param(hs_adj_th_level1, uint, 0664);
MODULE_PARM_DESC(hs_adj_th_level1, "hs_adj_th_level1");

module_param(hs_adj_th_level2, uint, 0664);
MODULE_PARM_DESC(hs_adj_th_level2, "hs_adj_th_level2");

module_param(hs_adj_th_level3, uint, 0664);
MODULE_PARM_DESC(hs_adj_th_level3, "hs_adj_th_level3");

module_param(hs_adj_th_level4, uint, 0664);
MODULE_PARM_DESC(hs_adj_th_level4, "hs_adj_th_level4");

module_param(vs_adj_th_level0, uint, 0664);
MODULE_PARM_DESC(vs_adj_th_level0, "vs_adj_th_level0");

module_param(vs_adj_th_level1, uint, 0664);
MODULE_PARM_DESC(vs_adj_th_level1, "vs_adj_th_level1");

module_param(vs_adj_th_level2, uint, 0664);
MODULE_PARM_DESC(vs_adj_th_level2, "vs_adj_th_level2");

module_param(vs_adj_th_level3, uint, 0664);
MODULE_PARM_DESC(vs_adj_th_level3, "vs_adj_th_level3");

module_param(vs_adj_th_level4, uint, 0664);
MODULE_PARM_DESC(vs_adj_th_level4, "vs_adj_th_level4");

module_param(vs_adj_th_level00, uint, 0664);
MODULE_PARM_DESC(vs_adj_th_level00, "vs_adj_th_level00");

module_param(vs_adj_th_level01, uint, 0664);
MODULE_PARM_DESC(vs_adj_th_level01, "vs_adj_th_level01");

module_param(vs_adj_th_level02, uint, 0664);
MODULE_PARM_DESC(vs_adj_th_level02, "vs_adj_th_level02");

module_param(vs_adj_th_level03, uint, 0664);
MODULE_PARM_DESC(vs_adj_th_level03, "vs_adj_th_level03");

module_param(vs_adj_th_level04, uint, 0664);
MODULE_PARM_DESC(vs_adj_th_level04, "vs_adj_th_level04");

module_param(cvd_2e, uint, 0664);
MODULE_PARM_DESC(cvd_2e, "cvd_2e");

module_param(cvd_2e_l1, uint, 0664);
MODULE_PARM_DESC(cvd_2e_l1, "cvd_2e_l1");

module_param(acd_128, uint, 0664);
MODULE_PARM_DESC(acd_128, "acd_128");

module_param(acd_128_l1, uint, 0664);
MODULE_PARM_DESC(acd_128_l1, "acd_128_l1");
#endif
