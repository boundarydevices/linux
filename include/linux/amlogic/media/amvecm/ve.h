/*
 * include/linux/amlogic/media/amvecm/ve.h
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

#ifndef __VE_H
#define __VE_H

/* ******************************************************************* */
/* *** enum definitions ********************************************* */
/* ******************************************************************* */

enum ve_demo_pos_e {
	VE_DEMO_POS_TOP = 0,
	VE_DEMO_POS_BOTTOM,
	VE_DEMO_POS_LEFT,
	VE_DEMO_POS_RIGHT,
};

enum ve_dnlp_rt_e {
	VE_DNLP_RT_0S = 0,
	VE_DNLP_RT_1S = 6,
	VE_DNLP_RT_2S,
	VE_DNLP_RT_4S,
	VE_DNLP_RT_8S,
	VE_DNLP_RT_16S,
	VE_DNLP_RT_32S,
	VE_DNLP_RT_64S,
	VE_DNLP_RT_FREEZE,
};

/* ******************************************************************* */
/* *** struct definitions ********************************************* */
/* ******************************************************************* */

struct ve_bext_s {
	unsigned char en;
	unsigned char start;
	unsigned char slope1;
	unsigned char midpt;
	unsigned char slope2;
};
#if defined(CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM)
struct ve_dnlp_s {
	unsigned int      en;
	unsigned int rt;    /* 0 ~ 255, */
	unsigned int rl;    /* 0 ~  15, 1.0000x ~ 1.9375x, step 0.0625x */
	unsigned int black; /* 0 ~  16, weak ~ strong */
	unsigned int white; /* 0 ~  16, weak ~ strong */
};
struct ve_hist_s {
	ulong sum;
	int width;
	int height;
	int ave;
};
struct ve_dnlp_table_s {
	unsigned int en;
	unsigned int method;
	unsigned int cliprate;
	unsigned int lowrange;
	unsigned int hghrange;
	unsigned int lowalpha;
	unsigned int midalpha;
	unsigned int hghalpha;
	unsigned int adj_level;
	unsigned int new_adj_level;
	unsigned int new_mvreflsh;
	unsigned int new_gmma_rate;
	unsigned int new_lowalpha;
	unsigned int new_hghalpha;
	unsigned int new_sbgnbnd;
	unsigned int new_sendbnd;
	unsigned int new_clashBgn;
	unsigned int new_clashEnd;
	unsigned int new_cliprate;
	unsigned int new_mtdbld_rate;
	unsigned int new_dnlp_pst_gmarat;
	unsigned int dnlp_sel;
	unsigned int dnlp_blk_cctr;/*blk signal add brightness*/
	unsigned int dnlp_brgt_ctrl;
	unsigned int dnlp_brgt_range;
	unsigned int dnlp_brght_add;
	unsigned int dnlp_brght_max;
	unsigned int dnlp_almst_wht;
	unsigned int dnlp_hghbin;/*1*/
	unsigned int dnlp_hghnum;
	unsigned int dnlp_lowbin;
	unsigned int dnlp_lownum;
	unsigned int dnlp_bkgend;
	unsigned int dnlp_bkgert;
	unsigned int dnlp_blkext;
	unsigned int dnlp_whtext;
	unsigned int dnlp_bextmx;
	unsigned int dnlp_wextmx;
	unsigned int dnlp_smhist_ck;
	unsigned int dnlp_glb_crate;/*12*/
	unsigned int dnlp_pstgma_brghtrate;
	unsigned int dnlp_pstgma_brghtrat1;
	unsigned int dnlp_wext_autorat;
	unsigned int dnlp_cliprate_min;
	unsigned int dnlp_adpcrat_lbnd;
	unsigned int dnlp_adpcrat_hbnd;
	unsigned int dnlp_adpmtd_lbnd;
	unsigned int dnlp_adpmtd_hbnd;
	unsigned int dnlp_set_bext;
	unsigned int dnlp_set_wext;
	unsigned int dnlp_satur_rat;
	unsigned int dnlp_satur_max;
	unsigned int blk_prct_rng;
	unsigned int blk_prct_max;
	unsigned int dnlp_lowrange;
	unsigned int dnlp_hghrange;
	unsigned int dnlp_auto_rng;
	unsigned int dnlp_bin0_absmax;
	unsigned int dnlp_bin0_sbtmax;
	unsigned int dnlp_adpalpha_lrate;
	unsigned int dnlp_adpalpha_hrate;
	unsigned int dnlp_lrate00;/*0-64bin curve slope*/
	unsigned int dnlp_lrate02;
	unsigned int dnlp_lrate04;
	unsigned int dnlp_lrate06;
	unsigned int dnlp_lrate08;
	unsigned int dnlp_lrate10;
	unsigned int dnlp_lrate12;
	unsigned int dnlp_lrate14;
	unsigned int dnlp_lrate16;
	unsigned int dnlp_lrate18;
	unsigned int dnlp_lrate20;
	unsigned int dnlp_lrate22;
	unsigned int dnlp_lrate24;
	unsigned int dnlp_lrate26;
	unsigned int dnlp_lrate28;
	unsigned int dnlp_lrate30;
	unsigned int dnlp_lrate32;
	unsigned int dnlp_lrate34;
	unsigned int dnlp_lrate36;
	unsigned int dnlp_lrate38;
	unsigned int dnlp_lrate40;
	unsigned int dnlp_lrate42;
	unsigned int dnlp_lrate44;
	unsigned int dnlp_lrate46;
	unsigned int dnlp_lrate48;
	unsigned int dnlp_lrate50;
	unsigned int dnlp_lrate52;
	unsigned int dnlp_lrate54;
	unsigned int dnlp_lrate56;
	unsigned int dnlp_lrate58;
	unsigned int dnlp_lrate60;
	unsigned int dnlp_lrate62;
};
struct vpp_hist_param_s {
	unsigned int vpp_hist_pow;
	unsigned int vpp_luma_sum;
	unsigned int vpp_pixel_sum;
	unsigned short vpp_histgram[64];
};
#else
struct ve_dnlp_s {
	unsigned char en;
	enum  ve_dnlp_rt_e rt;
	unsigned char gamma[64];
};
#endif
struct ve_hsvs_s {
	unsigned char en;
	unsigned char peak_gain_h1;
	unsigned char peak_gain_h2;
	unsigned char peak_gain_h3;
	unsigned char peak_gain_h4;
	unsigned char peak_gain_h5;
	unsigned char peak_gain_v1;
	unsigned char peak_gain_v2;
	unsigned char peak_gain_v3;
	unsigned char peak_gain_v4;
	unsigned char peak_gain_v5;
	unsigned char peak_gain_v6;
	unsigned char hpeak_slope1;
	unsigned char hpeak_slope2;
	unsigned char hpeak_thr1;
	unsigned char hpeak_thr2;
	unsigned char hpeak_nlp_cor_thr;
	unsigned char hpeak_nlp_gain_pos;
	unsigned char hpeak_nlp_gain_neg;
	unsigned char vpeak_slope1;
	unsigned char vpeak_slope2;
	unsigned char vpeak_thr1;
	unsigned char vpeak_thr2;
	unsigned char vpeak_nlp_cor_thr;
	unsigned char vpeak_nlp_gain_pos;
	unsigned char vpeak_nlp_gain_neg;
	unsigned char speak_slope1;
	unsigned char speak_slope2;
	unsigned char speak_thr1;
	unsigned char speak_thr2;
	unsigned char speak_nlp_cor_thr;
	unsigned char speak_nlp_gain_pos;
	unsigned char speak_nlp_gain_neg;
	unsigned char peak_cor_gain;
	unsigned char peak_cor_thr_l;
	unsigned char peak_cor_thr_h;
	unsigned char vlti_step;
	unsigned char vlti_step2;
	unsigned char vlti_thr;
	unsigned char vlti_gain_pos;
	unsigned char vlti_gain_neg;
	unsigned char vlti_blend_factor;
	unsigned char hlti_step;
	unsigned char hlti_thr;
	unsigned char hlti_gain_pos;
	unsigned char hlti_gain_neg;
	unsigned char hlti_blend_factor;
	unsigned char vlimit_coef_h;
	unsigned char vlimit_coef_l;
	unsigned char hlimit_coef_h;
	unsigned char hlimit_coef_l;
	unsigned char cti_444_422_en;
	unsigned char cti_422_444_en;
	unsigned char cti_blend_factor;
	unsigned char vcti_buf_en;
	unsigned char vcti_buf_mode_c5l;
	unsigned char vcti_filter;
	unsigned char hcti_step;
	unsigned char hcti_step2;
	unsigned char hcti_thr;
	unsigned char hcti_gain;
	unsigned char hcti_mode_median;
};

struct ve_ccor_s {
	unsigned char en;
	unsigned char slope;
	unsigned char thr;
};

struct ve_benh_s {
	unsigned char en;
	unsigned char cb_inc;
	unsigned char cr_inc;
	unsigned char gain_cr;
	unsigned char gain_cb4cr;
	unsigned char luma_h;
	unsigned char err_crp;
	unsigned char err_crn;
	unsigned char err_cbp;
	unsigned char err_cbn;
};

struct ve_cbar_s {
	unsigned char en;
	unsigned char wid;
	unsigned char cr;
	unsigned char cb;
	unsigned char y;
};
struct ve_demo_s {
	unsigned char bext;
	unsigned char dnlp;
	unsigned char hsvs;
	unsigned char ccor;
	unsigned char benh;
	enum  ve_demo_pos_e  pos;
	unsigned long wid;
	struct ve_cbar_s   cbar;
};

struct vdo_meas_s {
	/* ... */
};

struct ve_regmap_s {
	unsigned long reg[43];
};

#define EOTF_LUT_SIZE 33
#define OSD_OETF_LUT_SIZE 41

/********************OSD HDR registers backup********************************/
struct hdr_osd_lut_s {
	uint32_t r_map[33];
	uint32_t g_map[33];
	uint32_t b_map[33];
	uint32_t or_map[41];
	uint32_t og_map[41];
	uint32_t ob_map[41];
};

struct hdr_osd_reg_s {
	uint32_t viu_osd1_matrix_ctrl; /* 0x1a90 */
	uint32_t viu_osd1_matrix_coef00_01; /* 0x1a91 */
	uint32_t viu_osd1_matrix_coef02_10; /* 0x1a92 */
	uint32_t viu_osd1_matrix_coef11_12; /* 0x1a93 */
	uint32_t viu_osd1_matrix_coef20_21; /* 0x1a94 */
	uint32_t viu_osd1_matrix_colmod_coef42; /* 0x1a95 */
	uint32_t viu_osd1_matrix_offset0_1; /* 0x1a96 */
	uint32_t viu_osd1_matrix_offset2; /* 0x1a97 */
	uint32_t viu_osd1_matrix_pre_offset0_1; /* 0x1a98 */
	uint32_t viu_osd1_matrix_pre_offset2; /* 0x1a99 */
	uint32_t viu_osd1_matrix_coef22_30; /* 0x1a9d */
	uint32_t viu_osd1_matrix_coef31_32; /* 0x1a9e */
	uint32_t viu_osd1_matrix_coef40_41; /* 0x1a9f */
	uint32_t viu_osd1_eotf_ctl; /* 0x1ad4 */
	uint32_t viu_osd1_eotf_coef00_01; /* 0x1ad5 */
	uint32_t viu_osd1_eotf_coef02_10; /* 0x1ad6 */
	uint32_t viu_osd1_eotf_coef11_12; /* 0x1ad7 */
	uint32_t viu_osd1_eotf_coef20_21; /* 0x1ad8 */
	uint32_t viu_osd1_eotf_coef22_rs; /* 0x1ad9 */
	uint32_t VIU_OSD1_EOTF_3X3_OFST_0; /* 0x1aa0*/
	uint32_t VIU_OSD1_EOTF_3X3_OFST_1; /* 0x1aa1*/
	uint32_t viu_osd1_oetf_ctl; /* 0x1adc */
	struct hdr_osd_lut_s lut_val;
	/* -1: invalid, 0: not shadow, >1: delay count */
	int32_t shadow_mode;
};

extern struct hdr_osd_reg_s hdr_osd_reg;
/***********************OSD HDR registers*******************************/


/* ******************************************************************* */
/* *** MACRO definitions ********** */
/* ******************************************************************* */

/* ******************************************************************* */
/* *** FUNCTION definitions ********** */
/* ******************************************************************* */

#endif  /* _VE_H */
