/*
 * drivers/amlogic/media/enhancement/amvecm/amve.c
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
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
/* #include <mach/am_regs.h> */
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/amvecm/ve.h>
/* #include <linux/amlogic/aml_common.h> */
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include "arch/vpp_regs.h"
#include "arch/ve_regs.h"
#include "amve.h"
#include "amve_gamma_table.h"
#include "amvecm_vlock_regmap.h"
#include <linux/io.h>

#define pr_amve_dbg(fmt, args...)\
	do {\
		if (dnlp_debug&0x1)\
			pr_info("AMVE: " fmt, ## args);\
	} while (0)\
/* #define pr_amve_error(fmt, args...) */
/* printk(KERN_##(KERN_INFO) "AMVECM: " fmt, ## args) */

#define GAMMA_RETRY        1000

/* 0: Invalid */
/* 1: Valid */
/* 2: Updated in 2D mode */
/* 3: Updated in 3D mode */
unsigned long flags;

/* #if (MESON_CPU_TYPE>=MESON_CPU_TYPE_MESONG9TV) */
#define NEW_DNLP_IN_SHARPNESS 2
#define NEW_DNLP_IN_VPP 1

unsigned int dnlp_sel = NEW_DNLP_IN_SHARPNESS;
module_param(dnlp_sel, int, 0664);
MODULE_PARM_DESC(dnlp_sel, "dnlp_sel");
/* #endif */

struct ve_hist_s video_ve_hist;

static unsigned char ve_dnlp_tgt[64];
bool ve_en;
unsigned int ve_dnlp_white_factor;
unsigned int ve_dnlp_rt;
unsigned int ve_dnlp_rl;
unsigned int ve_dnlp_black;
unsigned int ve_dnlp_white;
unsigned int ve_dnlp_luma_sum;
static ulong ve_dnlp_lpf[64], ve_dnlp_reg[16];
static ulong ve_dnlp_reg_def[16] = {
	0x0b070400,	0x1915120e,	0x2723201c,	0x35312e2a,
	0x47423d38,	0x5b56514c,	0x6f6a6560,	0x837e7974,
	0x97928d88,	0xaba6a19c,	0xbfbab5b0,	0xcfccc9c4,
	0xdad7d5d2,	0xe6e3e0dd,	0xf2efece9,	0xfdfaf7f4
};

/*static bool frame_lock_nosm = 1;*/
static int ve_dnlp_waist_h = 128;
static int ve_dnlp_waist_l = 128;
static int ve_dnlp_ankle = 16;
static int ve_dnlp_strength = 255;
unsigned int vpp_log[128][10];
struct ve_dnlp_s am_ve_dnlp;
struct ve_dnlp_table_s am_ve_new_dnlp;
#if 0
static unsigned int lock_range_50hz_fast =  7; /* <= 14 */
static unsigned int lock_range_50hz_slow =  7; /* <= 14 */
static unsigned int lock_range_60hz_fast =  5; /* <=  4 */
static unsigned int lock_range_60hz_slow =  2; /* <= 10 */
#endif
struct tcon_gamma_table_s video_gamma_table_r;
struct tcon_gamma_table_s video_gamma_table_g;
struct tcon_gamma_table_s video_gamma_table_b;
struct tcon_gamma_table_s video_gamma_table_r_adj;
struct tcon_gamma_table_s video_gamma_table_g_adj;
struct tcon_gamma_table_s video_gamma_table_b_adj;
struct tcon_rgb_ogo_s video_rgb_ogo = {
	0, /* wb enable */
	0, /* -1024~1023, r_pre_offset */
	0, /* -1024~1023, g_pre_offset */
	0, /* -1024~1023, b_pre_offset */
	1024, /* 0~2047, r_gain */
	1024, /* 0~2047, g_gain */
	1024, /* 0~2047, b_gain */
	0, /* -1024~1023, r_post_offset */
	0, /* -1024~1023, g_post_offset */
	0  /* -1024~1023, b_post_offset */
};

#define FLAG_LVDS_FREQ_SW1       (1 <<  6)


int amvecm_hiu_reg_read(unsigned int reg, unsigned int *val)
{
	*val = readl(amvecm_hiu_reg_base+((reg - 0x1000)<<2));
	return 0;
}

int amvecm_hiu_reg_write(unsigned int reg, unsigned int val)
{
	writel(val, (amvecm_hiu_reg_base+((reg - 0x1000)<<2)));
	return 0;
}
static int amvecm_hiu_reg_write_bits(unsigned int reg, unsigned int value,
		unsigned int start, unsigned int len)
{
	unsigned int rd_val;

	amvecm_hiu_reg_read(reg, &rd_val);
	amvecm_hiu_reg_write(reg, ((rd_val &
	     ~(((1L << (len)) - 1) << (start))) |
	    (((value) & ((1L << (len)) - 1)) << (start))));
	return 0;
}


module_param(ve_dnlp_waist_h, int, 0664);
MODULE_PARM_DESC(ve_dnlp_waist_h, "ve_dnlp_waist_h");
module_param(ve_dnlp_waist_l, int, 0664);
MODULE_PARM_DESC(ve_dnlp_waist_l, "ve_dnlp_waist_l");
module_param(ve_dnlp_ankle, int, 0664);
MODULE_PARM_DESC(ve_dnlp_ankle, "ve_dnlp_ankle");
module_param(ve_dnlp_strength, int, 0664);
MODULE_PARM_DESC(ve_dnlp_strength, "ve_dnlp_strength");

/*static int debug_add_curve_en;*/
int glb_scurve[65];
int glb_clash_curve[65];
/*static int glb_scurve_bld_rate;*/
/*static int glb_clash_curve_bld_rate;*/
int glb_pst_gamma[65];
/*static int glb_pst_gamma_bld_rate;*/

static int debug_add_curve_en;
module_param(debug_add_curve_en, int, 0664);
MODULE_PARM_DESC(debug_add_curve_en, "debug_add_curve_en");

static int ve_usr_defined_test_s_mode;
module_param(ve_usr_defined_test_s_mode, int, 0664);
MODULE_PARM_DESC(ve_usr_defined_test_s_mode, "ve_usr_defined_test_s_mode");

static int ve_usr_defined_test_c_mode;
module_param(ve_usr_defined_test_c_mode, int, 0664);
MODULE_PARM_DESC(ve_usr_defined_test_c_mode, "ve_usr_defined_test_c_mode");

static int ve_usr_defined_test_g_mode;
module_param(ve_usr_defined_test_g_mode, int, 0664);
MODULE_PARM_DESC(ve_usr_defined_test_g_mode, "ve_usr_defined_test_g_mode");

static int glb_scurve_bld_rate;
module_param(glb_scurve_bld_rate, int, 0664);
MODULE_PARM_DESC(glb_scurve_bld_rate, "glb_scurve_bld_rate");

static int glb_clash_curve_bld_rate;
module_param(glb_clash_curve_bld_rate, int, 0664);
MODULE_PARM_DESC(glb_clash_curve_bld_rate, "glb_clash_curve_bld_rate");

static int glb_pst_gamma_bld_rate;
module_param(glb_pst_gamma_bld_rate, int, 0664);
MODULE_PARM_DESC(glb_pst_gamma_bld_rate, "glb_pst_gamma_bld_rate");

static int dnlp_respond;
module_param(dnlp_respond, int, 0664);
MODULE_PARM_DESC(dnlp_respond, "dnlp_respond");

static int dnlp_debug;
module_param(dnlp_debug, int, 0664);
MODULE_PARM_DESC(dnlp_debug, "dnlp_debug");

static int ve_dnlp_method;
module_param(ve_dnlp_method, int, 0664);
MODULE_PARM_DESC(ve_dnlp_method, "ve_dnlp_method");

static int ve_dnlp_cliprate = 6;
module_param(ve_dnlp_cliprate, int, 0664);
MODULE_PARM_DESC(ve_dnlp_cliprate, "ve_dnlp_cliprate");

static int ve_dnlp_lowrange = 18;
module_param(ve_dnlp_lowrange, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lowrange, "ve_dnlp_lowrange");

static int ve_dnlp_hghrange = 18;
module_param(ve_dnlp_hghrange, int, 0664);
MODULE_PARM_DESC(ve_dnlp_hghrange, "ve_dnlp_hghrange");

static int ve_dnlp_auto_rng;
module_param(ve_dnlp_auto_rng, int, 0664);
MODULE_PARM_DESC(ve_dnlp_auto_rng, "ve_dnlp_auto_rng");

static int ve_dnlp_lowalpha = 24;
module_param(ve_dnlp_lowalpha, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lowalpha, "ve_dnlp_lowalpha");

static int ve_dnlp_midalpha = 24;
module_param(ve_dnlp_midalpha, int, 0664);
MODULE_PARM_DESC(ve_dnlp_midalpha, "ve_dnlp_midalpha");

static int ve_dnlp_hghalpha = 24;
module_param(ve_dnlp_hghalpha, int, 0664);
MODULE_PARM_DESC(ve_dnlp_hghalpha, "ve_dnlp_hghalpha");

static int dnlp_adj_level = 6;
module_param(dnlp_adj_level, int, 0664);
MODULE_PARM_DESC(dnlp_adj_level, "dnlp_adj_level");

int dnlp_en;/* 0:disabel;1:enable */
module_param(dnlp_en, int, 0664);
MODULE_PARM_DESC(dnlp_en, "\n enable or disable dnlp\n");
static int dnlp_status = 1;/* 0:done;1:todo */

int dnlp_en_2;/* 0:disabel;1:enable */
module_param(dnlp_en_2, int, 0664);
MODULE_PARM_DESC(dnlp_en_2, "\n enable or disable dnlp\n");

static int frame_lock_freq;
module_param(frame_lock_freq, int, 0664);
MODULE_PARM_DESC(frame_lock_freq, "frame_lock_50");

static int video_rgb_ogo_mode_sw;
module_param(video_rgb_ogo_mode_sw, int, 0664);
MODULE_PARM_DESC(video_rgb_ogo_mode_sw,
		"enable/disable video_rgb_ogo_mode_sw");

int video_rgb_ogo_xvy_mtx;
module_param(video_rgb_ogo_xvy_mtx, int, 0664);
MODULE_PARM_DESC(video_rgb_ogo_xvy_mtx,
		"enable/disable video_rgb_ogo_xvy_mtx");

int video_rgb_ogo_xvy_mtx_latch = 1;

static int ve_dnlp_adj_level = 6;
module_param(ve_dnlp_adj_level, int, 0664);
MODULE_PARM_DESC(ve_dnlp_adj_level,
		"ve_dnlp_adj_level");

/* new dnlp start */
/* larger -> slower */
static int ve_dnlp_mvreflsh = 6;
module_param(ve_dnlp_mvreflsh, int, 0664);
MODULE_PARM_DESC(ve_dnlp_mvreflsh,
		"ve_dnlp_mvreflsh");

static int ve_dnlp_gmma_rate = 82;
module_param(ve_dnlp_gmma_rate, int, 0664);
MODULE_PARM_DESC(ve_dnlp_gmma_rate,
		"ve_dnlp_gmma_rate");

static int ve_dnlp_lowalpha_new = 40;
module_param(ve_dnlp_lowalpha_new, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lowalpha_new,
		"ve_dnlp_lowalpha_new");

static int ve_dnlp_hghalpha_new = 28;
module_param(ve_dnlp_hghalpha_new, int, 0664);
MODULE_PARM_DESC(ve_dnlp_hghalpha_new,
		"ve_dnlp_hghalpha_new");

static int ve_dnlp_cliprate_new = 36;
module_param(ve_dnlp_cliprate_new, int, 0664);
MODULE_PARM_DESC(ve_dnlp_cliprate_new,
		"ve_dnlp_cliprate_new");

int ve_dnlp_sbgnbnd;
module_param(ve_dnlp_sbgnbnd, int, 0664);
MODULE_PARM_DESC(ve_dnlp_sbgnbnd, "ve_dnlp_sbgnbnd");

static int ve_dnlp_sendbnd;
module_param(ve_dnlp_sendbnd, int, 0664);
MODULE_PARM_DESC(ve_dnlp_sendbnd, "ve_dnlp_sendbnd");

int ve_dnlp_clashBgn;
module_param(ve_dnlp_clashBgn, int, 0664);
MODULE_PARM_DESC(ve_dnlp_clashBgn, "ve_dnlp_clashBgn");

static int ve_dnlp_clashEnd = 15;
module_param(ve_dnlp_clashEnd, int, 0664);
MODULE_PARM_DESC(ve_dnlp_clashEnd, "ve_dnlp_clashEnd");

static int ve_mtdbld_rate = 53;
module_param(ve_mtdbld_rate, int, 0664);
MODULE_PARM_DESC(ve_mtdbld_rate, "ve_mtdbld_rate");

static int ve_dnlp_pst_gmarat = 65;
module_param(ve_dnlp_pst_gmarat, int, 0664);
MODULE_PARM_DESC(ve_dnlp_pst_gmarat, "ve_dnlp_pst_gmarat");

/*dnlp method = 3, use this flag or no use*/
bool ve_dnlp_respond_flag;
module_param(ve_dnlp_respond_flag, bool, 0664);
MODULE_PARM_DESC(ve_dnlp_respond_flag,
		"ve_dnlp_respond_flag");

/*dnlp method = 3, check the same histogram*/
bool ve_dnlp_smhist_ck;
module_param(ve_dnlp_smhist_ck, bool, 0664);
MODULE_PARM_DESC(ve_dnlp_smhist_ck,
		"ve_dnlp_smhist_ck");

bool ve_dnlp_dbg_adjavg;
module_param(ve_dnlp_dbg_adjavg, bool, 0664);
MODULE_PARM_DESC(ve_dnlp_dbg_adjavg,
		"ve_dnlp_dbg_adjavg");

int ve_dnlp_dbg_i2r = 255;
module_param(ve_dnlp_dbg_i2r, int, 0664);
MODULE_PARM_DESC(ve_dnlp_dbg_i2r,
		"ve_dnlp_dbg_i2r");

/* light -pattern 1 */
int ve_dnlp_slow_end = 2;
module_param(ve_dnlp_slow_end, int, 0664);
MODULE_PARM_DESC(ve_dnlp_slow_end,
		"ve_dnlp_slow_end");

/*concentration*/
static int ve_dnlp_blk_cctr = 8;
module_param(ve_dnlp_blk_cctr, int, 0664);
MODULE_PARM_DESC(ve_dnlp_blk_cctr, "dnlp low luma concenration");

/*the center to be brighter*/
static int ve_dnlp_brgt_ctrl = 48;
module_param(ve_dnlp_brgt_ctrl, int, 0664);
MODULE_PARM_DESC(ve_dnlp_brgt_ctrl, "dnlp center to be brighter");

/*brighter range*/
static int ve_dnlp_brgt_range = 16;
module_param(ve_dnlp_brgt_range, int, 0664);
MODULE_PARM_DESC(ve_dnlp_brgt_range, "dnlp brighter range");

/*yout=yin+ve_dnlp_brght_add*/
/* 32 => 0, brght_add range = [0,64] => [-32,+32] */
static int ve_dnlp_brght_add = 32;
module_param(ve_dnlp_brght_add, int, 0664);
MODULE_PARM_DESC(ve_dnlp_brght_add, "dnlp brightness up absolute");

/*yout=yin+ve_dnlp_brght_add + ve_dnlp_brght_max*rate*/
static int ve_dnlp_brght_max;
module_param(ve_dnlp_brght_max, int, 0664);
MODULE_PARM_DESC(ve_dnlp_brght_max, "dnlp brightness up maximum");

/* define the black or white scence */
static int ve_dnlp_almst_wht = 63;
module_param(ve_dnlp_almst_wht, int, 0664);
MODULE_PARM_DESC(ve_dnlp_almst_wht, "define the white scence");

/* global setting clip rate */
static bool ve_dnlp_glb_crate = 1;
module_param(ve_dnlp_glb_crate, bool, 0664);
MODULE_PARM_DESC(ve_dnlp_glb_crate, "global clash rate");

/* define it high bins: hist > hghbin*tAvg/64 */
static int ve_dnlp_hghbin = 51;
module_param(ve_dnlp_hghbin, int, 0664);
MODULE_PARM_DESC(ve_dnlp_hghbin, "high bins");

/* the number of bins with the largest histogram */
static int ve_dnlp_hghnum = 4;
module_param(ve_dnlp_hghnum, int, 0664);
MODULE_PARM_DESC(ve_dnlp_hghnum, "the number of high bins");

/* define it low bins: hist < lowbin*tAvg/64 */
static int ve_dnlp_lowbin = 4;
module_param(ve_dnlp_lowbin, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lowbin, "define it low bins");

/* the number of bins with the lowest histogram */
static int ve_dnlp_lownum = 4;
module_param(ve_dnlp_lownum, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lownum, "the number of low bins");

/* black gamma end point setting */
static int ve_dnlp_bkgend;
module_param(ve_dnlp_bkgend, int, 0664);
MODULE_PARM_DESC(ve_dnlp_bkgend, "black gamma end point setting");

/* black gamma end point rate */
static int ve_dnlp_bkgert = 64;
module_param(ve_dnlp_bkgert, int, 0664);
MODULE_PARM_DESC(ve_dnlp_bkgert, "black gamma end point rate");

static int ve_dnlp_pstgma_brghtrate = 8;
module_param(ve_dnlp_pstgma_brghtrate, int, 0664);
MODULE_PARM_DESC(ve_dnlp_pstgma_brghtrate, "ve_dnlp_pstgma_brghtrate");

static int ve_dnlp_pstgma_brghtrat1 = 15;
module_param(ve_dnlp_pstgma_brghtrat1, int, 0664);
MODULE_PARM_DESC(ve_dnlp_pstgma_brghtrat1, "ve_dnlp_pstgma_brghtrat1");

/* black extension when sum(hist) <  (ext*tAvg>>6) */
static int ve_dnlp_blkext = 4;
module_param(ve_dnlp_blkext, int, 0664);
MODULE_PARM_DESC(ve_dnlp_blkext, "black extension: sum()");

/* white extension when sum(hist) < (ext*tAvg>>6) */
static int ve_dnlp_whtext = 4;
module_param(ve_dnlp_whtext, int, 0664);
MODULE_PARM_DESC(ve_dnlp_whtext, "white extension: sum()");

/* black extension maximum bins */
static int ve_dnlp_bextmx = 4;
module_param(ve_dnlp_bextmx, int, 0664);
MODULE_PARM_DESC(ve_dnlp_bextmx, "black extension bins");

/* white extension maximum bins */
static int ve_dnlp_wextmx = 32;
module_param(ve_dnlp_wextmx, int, 0664);
MODULE_PARM_DESC(ve_dnlp_wextmx, "white extension bins");

static int ve_dnlp_wext_autorat = 16;
module_param(ve_dnlp_wext_autorat, int, 0664);
MODULE_PARM_DESC(ve_dnlp_wext_autorat, "ve_dnlp_wext_autorat");

/* adpative mtdrate low band */
/* default=0 */
static int ve_dnlp_adpmtd_lbnd = 19;
module_param(ve_dnlp_adpmtd_lbnd, int, 0664);
MODULE_PARM_DESC(ve_dnlp_adpmtd_lbnd, "ve_dnlp_adpmtd_lbnd");

/* adpative mtdrate high band */
/* default=0 */
static int ve_dnlp_adpmtd_hbnd = 20;
module_param(ve_dnlp_adpmtd_hbnd, int, 0664);
MODULE_PARM_DESC(ve_dnlp_adpmtd_hbnd, "ve_dnlp_adpmtd_hbnd");

static int ve_dnlp_adpalpha_lrate = 32;
module_param(ve_dnlp_adpalpha_lrate, int, 0664);
MODULE_PARM_DESC(ve_dnlp_adpalpha_lrate, "ve_dnlp_adpalpha_lrate");

static int ve_dnlp_adpalpha_hrate = 32;
module_param(ve_dnlp_adpalpha_hrate, int, 0664);
MODULE_PARM_DESC(ve_dnlp_adpalpha_hrate, "ve_dnlp_adpalpha_hrate");

static int ve_dnlp_set_bext;
module_param(ve_dnlp_set_bext, int, 0664);
MODULE_PARM_DESC(ve_dnlp_set_bext, "ve_dnlp_set_bext");

static int ve_dnlp_set_wext;
module_param(ve_dnlp_set_wext, int, 0664);
MODULE_PARM_DESC(ve_dnlp_set_wext, "ve_dnlp_set_wext");

static int ve_dnlp_satur_rat = 30;
module_param(ve_dnlp_satur_rat, int, 0664);
MODULE_PARM_DESC(ve_dnlp_satur_rat, "ve_dnlp_satur_rat");

static int ve_dnlp_satur_max = 40;
module_param(ve_dnlp_satur_max, int, 0664);
MODULE_PARM_DESC(ve_dnlp_satur_max, "ve_dnlp_satur_max");

static int ve_blk_prct_rng = 2;
module_param(ve_blk_prct_rng, int, 0664);
MODULE_PARM_DESC(ve_blk_prct_rng, "ve_blk_prct_rng");

static int ve_blk_prct_max = 16;
module_param(ve_blk_prct_max, int, 0664);
MODULE_PARM_DESC(ve_blk_prct_max, "ve_blk_prct_max");

static unsigned int ve_dnlp_set_saturtn;
module_param(ve_dnlp_set_saturtn, uint, 0664);
MODULE_PARM_DESC(ve_dnlp_set_saturtn, "ve_dnlp_set_saturtn");

static int ve_dnlp_bin0_absmax = 16;
module_param(ve_dnlp_bin0_absmax, int, 0664);
MODULE_PARM_DESC(ve_dnlp_bin0_absmax, "ve_dnlp_bin0_absmax");

static int ve_dnlp_bin0_sbtmax = 128;
module_param(ve_dnlp_bin0_sbtmax, int, 0664);
MODULE_PARM_DESC(ve_dnlp_bin0_sbtmax, "ve_dnlp_bin0_sbtmax");

static int ve_dnlp_lrate00 = 32;
module_param(ve_dnlp_lrate00, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lrate00, "Clash Local Clip rate 00");

static int ve_dnlp_lrate02 = 33;
module_param(ve_dnlp_lrate02, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lrate02, "Clash Local Clip rate 02");

static int ve_dnlp_lrate04 = 34;
module_param(ve_dnlp_lrate04, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lrate04, "Clash Local Clip rate 04");

static int ve_dnlp_lrate06 = 35;
module_param(ve_dnlp_lrate06, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lrate06, "Clash Local Clip rate 06");

static int ve_dnlp_lrate08 = 36;
module_param(ve_dnlp_lrate08, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lrate08, "Clash Local Clip rate 08");

static int ve_dnlp_lrate10 = 37;
module_param(ve_dnlp_lrate10, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lrate10, "Clash Local Clip rate 10");

static int ve_dnlp_lrate12 = 38;
module_param(ve_dnlp_lrate12, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lrate12, "Clash Local Clip rate 12");

static int ve_dnlp_lrate14 = 39;
module_param(ve_dnlp_lrate14, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lrate14, "Clash Local Clip rate 14");

static int ve_dnlp_lrate16 = 40;
module_param(ve_dnlp_lrate16, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lrate16, "Clash Local Clip rate 16");

static int ve_dnlp_lrate18 = 41;
module_param(ve_dnlp_lrate18, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lrate18, "Clash Local Clip rate 18");

static int ve_dnlp_lrate20 = 42;
module_param(ve_dnlp_lrate20, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lrate20, "Clash Local Clip rate 20");

static int ve_dnlp_lrate22 = 43;
module_param(ve_dnlp_lrate22, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lrate22, "Clash Local Clip rate 22");

static int ve_dnlp_lrate24 = 44;
module_param(ve_dnlp_lrate24, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lrate24, "Clash Local Clip rate 24");

static int ve_dnlp_lrate26 = 45;
module_param(ve_dnlp_lrate26, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lrate26, "Clash Local Clip rate 26");

static int ve_dnlp_lrate28 = 46;
module_param(ve_dnlp_lrate28, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lrate28, "Clash Local Clip rate 28");

static int ve_dnlp_lrate30 = 47;
module_param(ve_dnlp_lrate30, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lrate30, "Clash Local Clip rate 30");

static int ve_dnlp_lrate32 = 48;
module_param(ve_dnlp_lrate32, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lrate32, "Clash Local Clip rate 32");

static int ve_dnlp_lrate34 = 49;
module_param(ve_dnlp_lrate34, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lrate34, "Clash Local Clip rate 34");

static int ve_dnlp_lrate36 = 50;
module_param(ve_dnlp_lrate36, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lrate36, "Clash Local Clip rate 36");

static int ve_dnlp_lrate38 = 51;
module_param(ve_dnlp_lrate38, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lrate38, "Clash Local Clip rate 38");

static int ve_dnlp_lrate40 = 52;
module_param(ve_dnlp_lrate40, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lrate40, "Clash Local Clip rate 40");

static int ve_dnlp_lrate42 = 53;
module_param(ve_dnlp_lrate42, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lrate42, "Clash Local Clip rate 42");

static int ve_dnlp_lrate44 = 54;
module_param(ve_dnlp_lrate44, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lrate44, "Clash Local Clip rate 44");

static int ve_dnlp_lrate46 = 55;
module_param(ve_dnlp_lrate46, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lrate46, "Clash Local Clip rate 46");

static int ve_dnlp_lrate48 = 56;
module_param(ve_dnlp_lrate48, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lrate48, "Clash Local Clip rate 48");

static int ve_dnlp_lrate50 = 57;
module_param(ve_dnlp_lrate50, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lrate50, "Clash Local Clip rate 50");

static int ve_dnlp_lrate52 = 58;
module_param(ve_dnlp_lrate52, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lrate52, "Clash Local Clip rate 52");

static int ve_dnlp_lrate54 = 59;
module_param(ve_dnlp_lrate54, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lrate54, "Clash Local Clip rate 54");

static int ve_dnlp_lrate56 = 60;
module_param(ve_dnlp_lrate56, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lrate56, "Clash Local Clip rate 56");

static int ve_dnlp_lrate58 = 61;
module_param(ve_dnlp_lrate58, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lrate58, "Clash Local Clip rate 58");

static int ve_dnlp_lrate60 = 62;
module_param(ve_dnlp_lrate60, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lrate60, "Clash Local Clip rate 60");

static int ve_dnlp_lrate62 = 63;
module_param(ve_dnlp_lrate62, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lrate62, "Clash Local Clip rate 62");
/* Local clip rate */

/*the maximum bins > x/256*/
static int ve_dnlp_lgst_bin = 100;
module_param(ve_dnlp_lgst_bin, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lgst_bin, "dnlp: define it maximum bin");

/*two maximum bins' distance*/
static int ve_dnlp_lgst_dst = 30;
module_param(ve_dnlp_lgst_dst, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lgst_dst, "dnlp: two maximum bins' distance");

/* hd /fhd maybe set different value */
static int ve_dnlp_pavg_btsft = 5;
module_param(ve_dnlp_pavg_btsft, int, 0664);
MODULE_PARM_DESC(ve_dnlp_pavg_btsft, "ve_dnlp_pavg_btsft");

/* limit range */
static bool ve_dnlp_limit_rng = 1;
module_param(ve_dnlp_limit_rng, bool, 0664);
MODULE_PARM_DESC(ve_dnlp_limit_rng, "input limit range");

static bool ve_dnlp_range_det;
module_param(ve_dnlp_range_det, bool, 0664);
MODULE_PARM_DESC(ve_dnlp_range_det,
"input limit or full range detection");

/* scene change: avg - dif shif */
static int ve_dnlp_schg_sft = 1;
module_param(ve_dnlp_schg_sft, int, 0664);
MODULE_PARM_DESC(ve_dnlp_schg_sft,
		"ve_dnlp_schg_sft");

/* curveblend minimmum level */
static int ve_dnlp_cuvbld_min = 2;
module_param(ve_dnlp_cuvbld_min, int, 0664);
MODULE_PARM_DESC(ve_dnlp_cuvbld_min,
		"ve_dnlp_cuvbld_min");

/* curveblend minimmum level */
static int ve_dnlp_cuvbld_max = 17;
module_param(ve_dnlp_cuvbld_max, int, 0664);
MODULE_PARM_DESC(ve_dnlp_cuvbld_max,
		"ve_dnlp_cuvbld_max");

/* debug difference level */
static int ve_dnlp_dbg_diflvl;
module_param(ve_dnlp_dbg_diflvl, int, 0664);
MODULE_PARM_DESC(ve_dnlp_dbg_diflvl,
		"ve_dnlp_dbg_diflvl");

/* output the mapping curve */
static int ve_dnlp_dbg_map;
module_param(ve_dnlp_dbg_map, int, 0664);
MODULE_PARM_DESC(ve_dnlp_dbg_map,
		"ve_dnlp_dbg_map");

static int ve_dnlp_scv_dbg;
module_param(ve_dnlp_scv_dbg, int, 0664);
MODULE_PARM_DESC(ve_dnlp_scv_dbg,
		"ve_dnlp_scv_dbg");

static int ve_dnlp_cliprate_min = 19;
module_param(ve_dnlp_cliprate_min, int, 0664);
MODULE_PARM_DESC(ve_dnlp_cliprate_min,
		"ve_dnlp_cliprate_min");

static int ve_dnlp_adpcrat_lbnd = 10;
module_param(ve_dnlp_adpcrat_lbnd, int, 0664);
MODULE_PARM_DESC(ve_dnlp_adpcrat_lbnd,
		"ve_dnlp_adpcrat_lbnd");

static int ve_dnlp_adpcrat_hbnd = 20;
module_param(ve_dnlp_adpcrat_hbnd, int, 0664);
MODULE_PARM_DESC(ve_dnlp_adpcrat_hbnd,
		"ve_dnlp_adpcrat_hbnd");

/* print log once */
static int ve_dnlp_ponce = 1;
module_param(ve_dnlp_ponce, int, 0664);
MODULE_PARM_DESC(ve_dnlp_ponce, "ve_dnlp_ponce");

/* global variable */
unsigned int iRgnBgn; /* i>=iRgnBgn */
unsigned int iRgnEnd = 64;/* i<iRgnEnd */

unsigned int dnlp_printk;
module_param(dnlp_printk, uint, 0664);
MODULE_PARM_DESC(dnlp_printk, "dnlp_printk");
/*new dnlp end */

static bool hist_sel = 1; /*1->vpp , 0->vdin*/
module_param(hist_sel, bool, 0664);
MODULE_PARM_DESC(hist_sel, "hist_sel");

static unsigned int assist_cnt;/* ASSIST_SPARE8_REG1; */
static unsigned int assist_cnt2;/* ASSIST_SPARE8_REG2; */

/* video lock */
/* 0:enc;1:pll;2:manual pll */
unsigned int vlock_mode = VLOCK_MODE_MANUAL_PLL;
unsigned int vlock_en = 1;

/*0:only support 50->50;60->60;24->24;30->30;*/
/*1:support 24/30/50/60/100/120 mix,such as 50->60;*/
static unsigned int vlock_adapt;
static unsigned int vlock_dis_cnt_limit = 2;
static unsigned int vlock_delta_limit = 2;
/*vlock_debug:bit0:disable info;bit1:format change info;bit2:force reset*/
static unsigned int vlock_debug;
static unsigned int vlock_dynamic_adjust = 1;

static unsigned int vlock_sync_limit_flag;
static unsigned int vlock_state = VLOCK_STATE_NULL;/*1/2/3:vlock step*/
static enum vmode_e pre_vmode = VMODE_INIT_NULL;
static enum vframe_source_type_e pre_source_type =
		VFRAME_SOURCE_TYPE_OTHERS;
static enum vframe_source_mode_e pre_source_mode =
		VFRAME_SOURCE_MODE_OTHERS;
static unsigned int pre_input_freq;
static unsigned int pre_output_freq;
static unsigned int vlock_dis_cnt;
static char pre_vout_mode[64];
static bool vlock_vmode_changed;
static unsigned int pre_hiu_reg_m;
static unsigned int pre_hiu_reg_frac;
static unsigned int vlock_dis_cnt_no_vf;
static unsigned int vlock_dis_cnt_no_vf_limit = 5;

static unsigned int vlock_log_cnt;/*cnt base: vlock_log_s*/
static unsigned int vlock_log_size = 60;/*size base: vlock_log_s*/
static unsigned int vlock_log_delta_frac = 100;
static unsigned int vlock_log_delta_ivcnt = 0xff;
static unsigned int vlock_log_delta_ovcnt = 0xff;
static unsigned int vlock_log_delta_vcnt = 0xff;
static unsigned int vlock_log_last_ivcnt;
static unsigned int vlock_log_last_ovcnt;
static unsigned int vlock_log_delta_m;
static unsigned int vlock_log_delta_en;
module_param(vlock_log_size, uint, 0664);
MODULE_PARM_DESC(vlock_log_size, "\n vlock_log_size\n");
module_param(vlock_log_cnt, uint, 0664);
MODULE_PARM_DESC(vlock_log_cnt, "\n vlock_log_cnt\n");
module_param(vlock_log_delta_frac, uint, 0664);
MODULE_PARM_DESC(vlock_log_delta_frac, "\n vlock_log_delta_frac\n");
module_param(vlock_log_delta_m, uint, 0664);
MODULE_PARM_DESC(vlock_log_delta_m, "\n vlock_log_delta_m\n");
module_param(vlock_log_delta_en, uint, 0664);
MODULE_PARM_DESC(vlock_log_delta_en, "\n vlock_log_delta_en\n");

module_param(vlock_log_delta_ivcnt, uint, 0664);
MODULE_PARM_DESC(vlock_log_delta_ivcnt, "\n vlock_log_delta_ivcnt\n");

module_param(vlock_log_delta_ovcnt, uint, 0664);
MODULE_PARM_DESC(vlock_log_delta_ovcnt, "\n vlock_log_delta_ovcnt\n");

module_param(vlock_log_delta_vcnt, uint, 0664);
MODULE_PARM_DESC(vlock_log_delta_vcnt, "\n vlock_log_delta_vcnt\n");
static unsigned int vlock_log_en;
/* 3d sync parts begin */
unsigned int sync_3d_h_start;
unsigned int sync_3d_h_end;
unsigned int sync_3d_v_start = 10;
unsigned int sync_3d_v_end = 20;
unsigned int sync_3d_polarity;
unsigned int sync_3d_out_inv;
unsigned int sync_3d_black_color = 0x008080;/* yuv black */
/* 3d sync to v by one enable/disable */
unsigned int sync_3d_sync_to_vbo;
/* 3d sync parts end */

unsigned int contrast_adj_sel;/*0:vdj1, 1:vd1 mtx rgb contrast*/
module_param(contrast_adj_sel, uint, 0664);
MODULE_PARM_DESC(contrast_adj_sel, "\n contrast_adj_sel\n");

/*gxlx adaptive sr level*/
static unsigned int sr_adapt_level;
module_param(sr_adapt_level, uint, 0664);
MODULE_PARM_DESC(sr_adapt_level, "\n sr_adapt_level\n");

/* *********************************************************************** */
/* *** VPP_FIQ-oriented functions **************************************** */
/* *********************************************************************** */
static void ve_dnlp_add_cm(unsigned int value)
{
	unsigned int reg_value;

	VSYNC_WR_MPEG_REG(VPP_CHROMA_ADDR_PORT, 0x207);
	reg_value = VSYNC_RD_MPEG_REG(VPP_CHROMA_DATA_PORT);
	reg_value = (reg_value & 0xf000ffff) | (value << 16);
	VSYNC_WR_MPEG_REG(VPP_CHROMA_ADDR_PORT, 0x207);
	VSYNC_WR_MPEG_REG(VPP_CHROMA_DATA_PORT, reg_value);
}

static void ve_hist_gamma_tgt(struct vframe_s *vf)
{
	int ave_luma;
	struct vframe_prop_s *p = &vf->prop;

	video_ve_hist.sum    = p->hist.vpp_luma_sum;
	video_ve_hist.width  = p->hist.vpp_width;
	video_ve_hist.height = p->hist.vpp_height;

	video_ve_hist.ave =
		video_ve_hist.sum/(video_ve_hist.height*
				video_ve_hist.width);
	if ((vf->source_type == VFRAME_SOURCE_TYPE_OTHERS) &&
		(is_meson_gxtvbb_cpu())) {
		ave_luma = video_ve_hist.ave;
		ave_luma = (ave_luma - 16) < 0 ? 0 : (ave_luma - 16);
		video_ve_hist.ave = ave_luma*255/(235-16);
		if (video_ve_hist.ave > 255)
			video_ve_hist.ave = 255;
	}
}

static void ve_dnlp_calculate_tgt_ext(struct vframe_s *vf)
{
	struct vframe_prop_s *p = &vf->prop;
	static unsigned int sum_b = 0, sum_c;
	ulong i = 0, j = 0, sum = 0, ave = 0, ankle = 0,
			waist = 0, peak = 0, start = 0;
	ulong qty_h = 0, qty_l = 0, ratio_h = 0, ratio_l = 0;
	ulong div1  = 0, div2  = 0, step_h  = 0, step_l  = 0;
	ulong data[55];
	bool  flag[55], previous_state_high = false;
	/* READ_VPP_REG(ASSIST_SPARE8_REG1); */
	unsigned int cnt = assist_cnt;
	/* old historic luma sum */
	sum_b = sum_c;
	sum_c = ve_dnlp_luma_sum;
	/* new historic luma sum */
	ve_dnlp_luma_sum = p->hist.luma_sum;
	pr_amve_dbg("ve_dnlp_luma_sum=%x,sum_b=%x,sum_c=%x\n",
			ve_dnlp_luma_sum, sum_b, sum_c);
	/* picture mode: freeze dnlp curve */
	if (dnlp_respond) {
		/* new luma sum is 0, something is wrong, freeze dnlp curve */
		if (!ve_dnlp_luma_sum)
			return;
	} else {
		/* new luma sum is 0, something is wrong, freeze dnlp curve */
		if ((!ve_dnlp_luma_sum) ||
			/* new luma sum is closed to old one (1 +/- 1/64),*/
			 /* picture mode, freeze curve */
			((ve_dnlp_luma_sum <
					sum_b + (sum_b >> dnlp_adj_level)) &&
			(ve_dnlp_luma_sum >
					sum_b - (sum_b >> dnlp_adj_level))))
			return;
	}
	/* calculate ave (55 times of ave & data[] for accuracy) */
	/* ave    22-bit */
	/* data[] 22-bit */
	ave = 0;
	for (i = 0; i < 55; i++) {
		data[i]  = (ulong)p->hist.gamma[i + 4];
		ave     += data[i];
		data[i] *= 55;
		flag[i]  = false;
	}
	/* calculate ankle */
	/* ankle 22-bit */
	/* waist 22-bit */
	/* qty_h 6-bit */
	ankle = (ave * ve_dnlp_ankle + 128) >> 8;
	/* scan data[] to find out waist pulses */
	qty_h = 0;
	previous_state_high = false;
	for (i = 0; i < 55; i++) {
		if (data[i] >= ankle) {
			/* ankle pulses start */
			if (!previous_state_high) {
				previous_state_high = true;
				start = i;
				peak = 0;
			}
			/* normal maintenance */
			if (peak < data[i])
				peak = data[i];
		} else {
			/* ankle pulses end + 1 */
			if (previous_state_high) {
				previous_state_high = false;
				/* calculate waist of high area pulses */
				if (peak >= ave)
					waist =	((peak - ankle) *
						ve_dnlp_waist_h + 128) >> 8;
				/* calculate waist of high area pulses */
				else
					waist =	((peak - ankle) *
						ve_dnlp_waist_l + 128) >> 8;
				/* find out waist pulses */
				for (j = start; j < i; j++) {
					if (data[j] >= waist) {
						flag[j] = true;
						qty_h++;
					}
				}
			}
		}
	}

	/* calculate ratio_h and ratio_l */
	/* (div2 = 512*H*L times of value for accuracy) */
	/* averaged duty > 1/3 */
	/* qty_l 6-bit */
	/* div1 20-bit */
	/* div2 21-bit */
	/* ratio_h 22-bit */
	/* ratio_l 21-bit */
	qty_l =  55 - qty_h;
	if ((!qty_h) || (!qty_l)) {
		for (i = 5; i <= 58; i++)
			ve_dnlp_tgt[i] = i << 2;
	} else {
		div1  = 256 * qty_h * qty_l;
		div2  = 512 * qty_h * qty_l;
		if (qty_h > 18) {
			/* [1.0 ~ 2.0) */
			ratio_h = div2 + ve_dnlp_strength * qty_l * qty_l;
			 /* [0.5 ~ 1.0] */
			ratio_l = div2 - ve_dnlp_strength * qty_h * qty_l;
		}
		/* averaged duty < 1/3 */
		/* [1.0 ~ 2.0] */
		ratio_h = div2 + (ve_dnlp_strength << 1) * qty_h * qty_l;
		/* (0.5 ~ 1.0] */
		ratio_l = div2 - (ve_dnlp_strength << 1) * qty_h * qty_h;
		/* distribute ratio_h & ratio_l to */
		/* ve_dnlp_tgt[5] ~ ve_dnlp_tgt[58] */
		/* sum 29-bit */
		/* step_h 24-bit */
		/* step_l 23-bit */
		sum = div2 << 4; /* start from 16 */
		step_h = ratio_h << 2;
		step_l = ratio_l << 2;
		for (i = 5; i <= 58; i++) {
			/* high phase */
			if (flag[i - 5])
				sum += step_h;
			/* low  phase */
			else
				sum += step_l;
			ve_dnlp_tgt[i] = (sum + div1) / div2;
		}
		if (cnt) {
			for (i = 0; i < 64; i++)
				pr_amve_dbg(" ve_dnlp_tgte[%ld]=%d\n",
						i, ve_dnlp_tgt[i]);
			/* WRITE_VPP_REG(ASSIST_SPARE8_REG1, 0); */
			assist_cnt = 0;
		}
	}
}

void GetWgtLst(ulong *iHst, ulong tAvg, ulong nLen, ulong alpha)
{
	ulong iMax = 0;
	ulong iMin = 0;
	ulong iPxl = 0;
	ulong iT = 0;

	for (iT = 0; iT < nLen; iT++) {
		iPxl = iHst[iT];
		if (iPxl > tAvg) {
			iMax = iPxl;
			iMin = tAvg;
		} else {
			iMax = tAvg;
			iMin = iPxl;
		}
		if (alpha < 16) {
			iPxl = ((16-alpha)*iMin+8)>>4;
			iPxl += alpha*iMin;
		} else if (alpha < 32) {
			iPxl = (32-alpha)*iMin;
			iPxl += (alpha-16)*iMax;
		} else {
			iPxl = (48-alpha)+4*(alpha-32);
			iPxl *= iMax;
		}
		iPxl = (iPxl+8)>>4;
		iHst[iT] = iPxl < 1 ? 1 : iPxl;
	}
}

static void ve_dnlp_calculate_tgtx(struct vframe_s *vf)
{
	struct vframe_prop_s *p = &vf->prop;
	ulong iHst[64];
	ulong oHst[64];
	static unsigned int sum_b = 0, sum_c;
	ulong i = 0, j = 0, sum = 0, max = 0;
	ulong cLmt = 0, nStp = 0, stp = 0, uLmt = 0;
	long nExc = 0;
	unsigned int cnt = assist_cnt;/* READ_VPP_REG(ASSIST_SPARE8_REG1); */
	unsigned int cnt2 = assist_cnt2;/* READ_VPP_REG(ASSIST_SPARE8_REG2); */
	unsigned int clip_rate = ve_dnlp_cliprate; /* 8bit */
	unsigned int low_range = ve_dnlp_lowrange;/* 18; //6bit [0-54] */
	unsigned int hgh_range = ve_dnlp_hghrange;/* 18; //6bit [0-54] */
	unsigned int low_alpha = ve_dnlp_lowalpha;/* 24; //6bit [0--48] */
	unsigned int mid_alpha = ve_dnlp_midalpha;/* 24; //6bit [0--48] */
	unsigned int hgh_alpha = ve_dnlp_hghalpha;/* 24; //6bit [0--48] */
	/* ------------------------------------------------- */
	ulong tAvg = 0;
	ulong nPnt = 0;
	ulong mRng = 0;
	/* old historic luma sum */
	sum_b = sum_c;
	sum_c = ve_dnlp_luma_sum;
	/* new historic luma sum */
	ve_dnlp_luma_sum = p->hist.luma_sum;
	pr_amve_dbg("ve_dnlp_luma_sum=%x,sum_b=%x,sum_c=%x\n",
			ve_dnlp_luma_sum, sum_b, sum_c);
	/* picture mode: freeze dnlp curve */
	if (dnlp_respond) {
		/* new luma sum is 0, something is wrong, freeze dnlp curve */
		if (!ve_dnlp_luma_sum)
			return;
	} else {
		/* new luma sum is 0, something is wrong, freeze dnlp curve */
		if ((!ve_dnlp_luma_sum) ||
			/* new luma sum is closed to old one (1 +/- 1/64), */
			/* picture mode, freeze curve */
			((ve_dnlp_luma_sum <
					sum_b + (sum_b >> dnlp_adj_level)) &&
			(ve_dnlp_luma_sum >
					sum_b - (sum_b >> dnlp_adj_level))))
			return;
	}
	/* 64 bins, max, ave */
	for (i = 0; i < 64; i++) {
		iHst[i] = (ulong)p->hist.gamma[i];
		if (i >= 4 && i <= 58) {
			oHst[i] = iHst[i];
			if (max < iHst[i])
				max = iHst[i];
			sum += iHst[i];
		} else {
			oHst[i] = 0;
		}
	}
	cLmt = (clip_rate*sum)>>8;
	tAvg = sum/55;
	/* invalid histgram: freeze dnlp curve */
	if (max <= 55)
		return;
	/* get 1st 4 points */
	for (i = 4; i <= 58; i++) {
		if (iHst[i] > cLmt)
			nExc += (iHst[i]-cLmt);
	}
	nStp = (nExc+28)/55;
	uLmt = cLmt-nStp;
	if (cnt2) {
		pr_amve_dbg(" ve_dnlp_tgtx:cLmt=%ld,nStp=%ld,uLmt=%ld\n",
				cLmt, nStp, uLmt);
		/* WRITE_VPP_REG(ASSIST_SPARE8_REG2, 0); */
		assist_cnt2 = 0;
	}
	if (clip_rate <= 4 || tAvg <= 2) {
		cLmt = (sum+28)/55;
		sum = cLmt*55;
		for (i = 4; i <= 58; i++)
			oHst[i] = cLmt;
	} else if (nStp != 0) {
		for (i = 4; i <= 58; i++) {
			if (iHst[i] >= cLmt)
				oHst[i] = cLmt;
			else {
				if (iHst[i] > uLmt) {
					oHst[i] = cLmt;
					nExc -= cLmt-iHst[i];
				} else {
					oHst[i] = iHst[i]+nStp;
					nExc -= nStp;
				}
				if (nExc < 0)
					nExc = 0;
			}
		}
		j = 4;
		while (nExc > 0) {
			if (nExc >= 55) {
				nStp = 1;
				stp = nExc/55;
			} else {
				nStp = 55/nExc;
				stp = 1;
			}
			for (i = j; i <= 58; i += nStp) {
				if (oHst[i] < cLmt) {
					oHst[i] += stp;
					nExc -= stp;
				}
				if (nExc <= 0)
					break;
			}
			j += 1;
			if (j > 58)
				break;
		}
	}
	if (low_range == 0 && hgh_range == 0)
		nPnt = 0;
	else {
		if (low_range == 0 || hgh_range == 0) {
			nPnt = 1;
			mRng = (hgh_range > low_range ? hgh_range : low_range);
		} else if (low_range+hgh_range >= 54) {
			nPnt = 1;
			mRng = (hgh_range < low_range ? hgh_range : low_range);
		} else
			nPnt = 2;
	}
	if (nPnt == 0 && low_alpha >= 16 && low_alpha <= 32) {
		sum = 0;
		for (i = 5; i <= 59; i++) {
			j = oHst[i]*(32-low_alpha)+tAvg*(low_alpha-16);
			j = (j+8)>>4;
			oHst[i] = j;
			sum += j;
		}
	} else if (nPnt == 1) {
		GetWgtLst(oHst+4, tAvg, mRng, low_alpha);
		GetWgtLst(oHst+4+mRng, tAvg, 54-mRng, hgh_alpha);
	} else if (nPnt == 2) {
		mRng = 55-(low_range+hgh_range);
		GetWgtLst(oHst+4, tAvg, low_range, low_alpha);
		GetWgtLst(oHst+4+low_range, tAvg, mRng, mid_alpha);
		GetWgtLst(oHst+4+mRng+low_range, tAvg, hgh_range, hgh_alpha);
	}
	sum = 0;
	for (i = 4; i <= 58; i++) {
		if (oHst[i] > cLmt)
			oHst[i] = cLmt;
		sum += oHst[i];
	}
	nStp = 0;
	/* sum -= oHst[4]; */
	for (i = 5; i <= 59; i++) {
		nStp += oHst[i-1];
		/* nStp += oHst[i]; */
		j = (236-16)*nStp;
		j += (sum>>1);
		j /= sum;
		ve_dnlp_tgt[i] = j + 16;
	}
	if (cnt) {
		for (i = 0; i < 64; i++)
			pr_amve_dbg(" ve_dnlp_tgtx[%ld]=%d\n",
					i, ve_dnlp_tgt[i]);
		/* WRITE_VPP_REG(ASSIST_SPARE8_REG1, 0); */
		assist_cnt = 0;
	}
}

static unsigned int pre_1_gamma[65];
static unsigned int pre_0_gamma[65];

/* more 2-bit */
static unsigned int pst_0_gamma[65];

bool dnlp_scn_chg; /* scene change */
int dnlp_bld_lvl; /* blend level */
int RBASE = 1;

/*rGmIn[0:64]   ==>0:4:256, gamma*/
/*rGmOt[0:pwdth]==>0-0, 0~1024*/
void GetSubCurve(unsigned int *rGmOt,
		unsigned int *rGmIn, unsigned int pwdth)
{
	int nT0 = 0;
	unsigned int BASE = 64;

	unsigned int plft = 0;
	unsigned int prto = 0;
	unsigned int rst = 0;

	unsigned int idx1 = 0;
	unsigned int idx2 = 0;

	if (pwdth == 0)
		pwdth = 1;

	for (nT0 = 0; nT0 <= pwdth; nT0++) {
		plft = nT0*64/pwdth;
		prto = (BASE*(nT0*BASE-plft*pwdth) + pwdth/2)/pwdth;

		idx1 = plft;
		idx2 = plft+1;
		if (idx1 > 64)
			idx1 = 64;
		if (idx2 > 64)
			idx2 = 64;

		rst = rGmIn[idx1]*(BASE-prto) + rGmIn[idx2]*prto;
		rst = (rst + BASE/2)*4*pwdth/BASE;
		/* rst = ((rst + 128)>>8); */
		rst = ((rst + 32)>>6);

		if (nT0 == 0)
			rst = rGmIn[0];
		if (rst > (pwdth << 4))
			rst = (pwdth << 4);

		rGmOt[nT0] = rst;
	}
}

/*rGmOt[0:64]*/
/*rGmIn[0:64]*/
/* 0~1024 */
void GetGmBlkCvs(unsigned int *rGmOt, unsigned int *rGmIn,
		unsigned int Bgn, unsigned int End)
{
	static unsigned int pgmma0[65];
	int nT0 = 0;
	int pwdth = End - Bgn; /* 64 */
	int pLst[65];
	int i = 0;
	int nTmp0 = 0;

	if (!ve_dnlp_luma_sum) {
		for (nT0 = 0; nT0 < 65; nT0++)
			pgmma0[nT0] = (nT0 << 4); /* 0 ~1024 */
	}

	GetSubCurve(pLst, rGmIn, pwdth); /*0~1024*/

	for (nT0 = 0; nT0 < 65; nT0++) {
		if (nT0 < Bgn)
			rGmOt[nT0] = (nT0 << 4);
		else if (nT0 >= End)
			rGmOt[nT0] = (nT0 << 4);
		else {
			if (ve_dnlp_pst_gmarat > 64)
				rGmOt[nT0] = (Bgn << 4) +
				(1024 - pLst[64 + Bgn - nT0]);
			else
				rGmOt[nT0] = (Bgn << 4) + pLst[nT0 - Bgn];
		}
	}

	if (!dnlp_scn_chg)
		for (i = 0; i < 65; i++) {
			nTmp0 = dnlp_bld_lvl * rGmOt[i] + (RBASE >> 1);
			nTmp0 = nTmp0 + (RBASE - dnlp_bld_lvl) * pgmma0[i];
			nTmp0 = (nTmp0 >> ve_dnlp_mvreflsh); /* 0 ~1024 */

			rGmOt[i] = nTmp0;
		}

	for (i = 0; i < 65; i++)
		pgmma0[i] = rGmOt[i];
}


/*rGmOt[0:64]*/
/*rGmIn[0:64]*/
/* 0 ~1024 */
void GetGmCurves2(unsigned int *rGmOt, unsigned int *rGmIn,
		unsigned int pval, unsigned int BgnBnd, unsigned int EndBnd)
{
	int nT0 = 0;
	/*unsigned int rst=0;*/
	int pwdth = 0;
	unsigned int pLst[65];
	int nT1 = 0;
	bool prt_flg = ((dnlp_printk >> 11) & 0x1);

	if (pval >= ve_dnlp_almst_wht) {
		for (nT0 = 0; nT0 < 65; nT0++)
			rGmOt[nT0] = rGmIn[nT0] << 2;
		return;
	}

	if (BgnBnd > 10)
		BgnBnd = 10;
	if (EndBnd > 10)
		EndBnd = 10;

	if (pval < ve_dnlp_slow_end)
		pval = ve_dnlp_slow_end;

	if (pval <= BgnBnd)
		pval = BgnBnd + 1;

	/* fuck here */
	if (pval + EndBnd > 64)
		pval = 63 - EndBnd;

	for (nT0 = 0; nT0 < 65; nT0++)
		rGmOt[nT0] = (nT0<<4); /* 0~1024 */

	if (pval > BgnBnd) {
		pwdth = pval - BgnBnd;
		GetSubCurve(pLst, rGmIn, pwdth); /* 0~1024 */
		for (nT0 = BgnBnd; nT0 <= pval; nT0++) {
			nT1 = pLst[nT0 - BgnBnd] + (BgnBnd<<4);
			rGmOt[nT0] = nT1;
		}
	}

	if ((pval + EndBnd) < 64) {
		pwdth = 64 - pval - EndBnd;
		GetSubCurve(pLst, rGmIn, pwdth);
		for (nT0 = pval; nT0 <= 64 - EndBnd; nT0++) {
			nT1 = (1024 - (EndBnd<<4)
				- pLst[pwdth - (nT0 - pval)]);
			rGmOt[nT0] = nT1;  /* 0~1024 */
		}
	}

	if (prt_flg) {
		pr_info("#GmCvs2: %d %d %d\n", BgnBnd, pval, EndBnd);
		for (nT1 = 0; nT1 < 65; nT1++)
			pr_info("[%02d] => %4d\n", nT1, rGmOt[nT1]);
		pr_info("\n");
	}
}

void GetGmCurves(unsigned int *rGmOt, unsigned int *rGmIn,
		unsigned int lsft_avg, unsigned int BgnBnd, unsigned int EndBnd)
{
	int pval1 = (lsft_avg >> (2 + ve_dnlp_pavg_btsft));
	int pval2 = pval1 + 1;
	int BASE  = (1 << (2 + ve_dnlp_pavg_btsft));
	int nRzn = lsft_avg - (pval1 << (2 + ve_dnlp_pavg_btsft));
	unsigned int pLst1[65];
	unsigned int pLst2[65];
	int i = 0;

	if (pval2 > ve_dnlp_almst_wht)
		pval2 = ve_dnlp_almst_wht;

	GetGmCurves2(pLst1, rGmIn, pval1, BgnBnd, EndBnd);
	GetGmCurves2(pLst2, rGmIn, pval2, BgnBnd, EndBnd);

	for (i = 0; i < 65; i++) {
		pval2 = pLst1[i] * (BASE - nRzn) + pLst2[i] * nRzn;

		pval2 = (pval2 + (BASE >> 1));
		pval2 =  (pval2 >> (2 + ve_dnlp_pavg_btsft));

		if (pval2 < 0)
			pval2 = 0;
		else if (pval2 > 1023)
			pval2 = 1023;
		rGmOt[i] = pval2;
	}
}

unsigned int cal_hist_avg(unsigned int pval)
{
	static unsigned int ppval;

	if (!dnlp_scn_chg) {
		pval = dnlp_bld_lvl * pval + (RBASE >> 1);
		pval = pval + (RBASE - dnlp_bld_lvl) * ppval;
		pval = (pval >> ve_dnlp_mvreflsh);
	}
	ppval = pval;

	return pval;
}

/* history */
unsigned int cal_hst_shft_avg(unsigned int pval)
{
	static unsigned int ppval;

	if (!dnlp_scn_chg) {
		pval = dnlp_bld_lvl * pval + (RBASE >> 1);
		pval = pval + (RBASE - dnlp_bld_lvl) * ppval;
		pval = (pval >> ve_dnlp_mvreflsh);
	}
	ppval = pval;

	return pval;
}

/* mtdrate: mtdrate in the mid-tone*/
/* luma_avg: 0~64 */
static int dnlp_adp_mtdrate(int mtdrate, int luma_avg)
{
	int np = 0;
	int nt = mtdrate;

	if (ve_dnlp_adpmtd_lbnd > 0 &&
		luma_avg < ve_dnlp_adpmtd_lbnd) {
		nt = mtdrate * luma_avg + (ve_dnlp_adpmtd_lbnd >> 1);
		nt /= ve_dnlp_adpmtd_lbnd;

		np = 4 * (ve_dnlp_adpmtd_lbnd - luma_avg);
		if (np < mtdrate)
			np = mtdrate - np;
		else
			np = 0;

		if (np > nt)
			nt = np;
	} else if (ve_dnlp_adpmtd_hbnd > 0 &&
		(luma_avg > 64 - ve_dnlp_adpmtd_hbnd)) {
		nt = mtdrate * (64 - luma_avg);
		nt += (ve_dnlp_adpmtd_hbnd >> 1);
		nt /= ve_dnlp_adpmtd_hbnd;

		np = luma_avg - (64 - ve_dnlp_adpmtd_hbnd);
		np = 4 * np;
		if (np < mtdrate)
			np = mtdrate - np;
		else
			np = 0;

		if (np > nt)
			nt = np;
	}

	return nt;
}

unsigned int AdjHistAvg(unsigned int pval, unsigned int ihstEnd)
{
	unsigned int pEXT = 224;
	unsigned int pMid = 128;
	unsigned int pMAX = 236;

	if (ihstEnd > 59)
		pMAX = ihstEnd << 2;

	if (pval > pMid) {
		pval = pMid + (pMAX - pMid)*(pval - pMid)/(pEXT - pMid);
		if (pval > pMAX)
			pval = pMAX;
	}

	return pval;
}

int blk_wht_extsn(int *blk_wht_ext, unsigned int *iHst,
	int hstSum, unsigned int luma_avg)
{
	int tAvg = ((hstSum + 32) >> 6);
	int nStp = 0;
	int nT0 = 0;
	int nT1 = 0;
	int i = 0;
	int dnlp_wextmx = ve_dnlp_wextmx;

	/* black / white extension */
	static int pblk_wht_ext[2];

	/* black extension */
	nStp = tAvg * ve_dnlp_blkext + 32;
	nStp = (nStp >> 6);
	blk_wht_ext[0] = 0;
	nT0 = 0;
	for (i = 0; i < (iRgnBgn + ve_dnlp_bextmx); i++) {
		nT0 += iHst[i];
		if (nT0 > nStp)
			break;
		else if (i >= iRgnBgn)
			blk_wht_ext[0] = ((i + 1 - iRgnBgn) << 4);
	}

	/* white extension */
	nStp = tAvg * ve_dnlp_whtext + 32;
	nStp = (nStp >> 6);

	if (ve_dnlp_wext_autorat > 0)
		dnlp_wextmx = iRgnEnd - 1;
	else
		dnlp_wextmx = ve_dnlp_wextmx;

	nT0 = 0;
	nT1 = 0;
	for (i = 0; i < dnlp_wextmx; i++) {
		nT0 += iHst[iRgnEnd - 1 - i];
		if (nT0 > nStp)
			break;
		else if (i >= (64 - iRgnEnd))
			nT1 = (i + iRgnEnd - 63);
	}

	if (ve_dnlp_wext_autorat > 0) {
		if (luma_avg > 32) /* baby face */
			nT1 = 0;
		else {
			nT1 = (32 - luma_avg) * nT1 * ve_dnlp_wext_autorat;
			nT1 = ((nT1 + 512) >> 10);
			if (nT1 > ve_dnlp_wextmx)
				nT1 = ve_dnlp_wextmx;
		}
	}
	blk_wht_ext[1] = (nT1 << 4);

	if (!dnlp_scn_chg) {
		nT0 = dnlp_bld_lvl * blk_wht_ext[0] + (RBASE >> 1);
		nT0 = nT0 + (RBASE - dnlp_bld_lvl) * pblk_wht_ext[0];
		blk_wht_ext[0] = (nT0 >> ve_dnlp_mvreflsh);

		nT1 = dnlp_bld_lvl * blk_wht_ext[1] + (RBASE >> 1);
		nT1 = nT1 + (RBASE - dnlp_bld_lvl) * pblk_wht_ext[1];
		blk_wht_ext[1] = (nT1 >> ve_dnlp_mvreflsh);
	}

	pblk_wht_ext[0] = blk_wht_ext[0];
	pblk_wht_ext[1] = blk_wht_ext[1];

	return 0;
}

static unsigned int dnlp_adp_cliprate(unsigned int clip_rate,
	unsigned int clip_rmin, unsigned int luma_avg)
{
	unsigned int nt = clip_rate;

	if (luma_avg < ve_dnlp_adpcrat_lbnd) {
		nt = clip_rate * (ve_dnlp_adpcrat_lbnd - luma_avg) +
			luma_avg * clip_rmin;

		nt = (nt << 4) + (ve_dnlp_adpcrat_lbnd >> 1);
		nt /= ve_dnlp_adpcrat_lbnd;
	} else if (luma_avg > 64 - ve_dnlp_adpcrat_hbnd) {
		nt = clip_rmin * (64 - luma_avg) +
			clip_rate * (luma_avg + ve_dnlp_adpcrat_hbnd - 64);
		nt += (nt << 4) + (ve_dnlp_adpcrat_hbnd >> 1);
		nt /= ve_dnlp_adpcrat_hbnd;
	} else
		nt = (clip_rmin << 4);

	return nt;
}

int old_dnlp_lrate[32];

/*iHst[0:63]: [0,4)->iHst[0], [252,256)->iHst[63]*/
/*oMap[0:64]:0:16:1024*/
void clash_fun(unsigned int *oMap, unsigned int *iHst,
		unsigned int hstBgn, unsigned int hstEnd)
{
	unsigned int i = 0, j = 0;
	unsigned int tmax = 0;
	unsigned int tsum = 0;
	unsigned int oHst[64];
	unsigned int cLmt = 0;
	unsigned int tLen = (hstEnd - hstBgn);
	unsigned int tAvg = 0;
	unsigned int lAvg4 = 0;
	unsigned int lAvg1 = 0;
	unsigned int nStp = 0;
	/*unsigned int uLmt = 0;*/
	/*unsigned int stp = 0;*/
	unsigned int idx[64];
	unsigned int tHst[64];
	unsigned int nT0 = 0;
	unsigned int clip_rate = ve_dnlp_cliprate_new;
	unsigned int clip_rmin  = ve_dnlp_cliprate_min;
	unsigned int adp_crate = clip_rate;

	int nNum = 0;
	int nExc = 0;
	int tExc = 0;
	bool prt_flg = 0;

	/* local limit 64-bin*/
	unsigned int lcl_lmt[64];

	if (clip_rmin > clip_rate)
		clip_rmin = clip_rate;
	if (ve_dnlp_adpcrat_lbnd < 2)
		ve_dnlp_adpcrat_lbnd = 2;
	else if (ve_dnlp_adpcrat_lbnd > 30)
		ve_dnlp_adpcrat_lbnd = 30;

	if (ve_dnlp_adpcrat_hbnd < 2)
		ve_dnlp_adpcrat_hbnd = 2;
	else if (ve_dnlp_adpcrat_hbnd > 30)
		ve_dnlp_adpcrat_hbnd = 30;

	if (hstBgn > 16)
		hstBgn = 16;

	if (hstEnd > 64)
		hstEnd = 64;
	else if (hstEnd < 48)
		hstEnd = 48;

	oMap[64] = 1024; /* 0~1024 */
	/*64 bins, max, ave*/
	lAvg4 = 0;
	lAvg1 = 0;
	for (i = 0; i < 64; i++) {
		oHst[i] = iHst[i];
		oMap[i] = (i << 4); /* 0~1024 */

		tHst[i] = iHst[i];
		idx[i] = i;

		if (i >= hstBgn && i <= hstEnd-1) {
			if (tmax < iHst[i])
				tmax = iHst[i];
			tsum += iHst[i];
			lAvg4 += (iHst[i] * i);
		} else {
			oHst[i] = 0;
		}

		lcl_lmt[i] = old_dnlp_lrate[(i>>1)];
	}
	lAvg4 = (lAvg4 << 2) + tsum / 2;
	lAvg4 = lAvg4 / tsum;
	lAvg1 = (lAvg4 + 2) >> 2;

	/* << 4 */
	adp_crate = dnlp_adp_cliprate(clip_rate, clip_rmin, lAvg1);

	prt_flg = ((dnlp_printk >> 9) & 0x1);
	if (prt_flg)
		pr_info("#CL: Range[%02d ~ %02d] lAvg4=%d(%d), (crate << 4)=%d\n",
			hstBgn, hstEnd, lAvg4, lAvg1, adp_crate);

	for (i = 1; i <= 61; i += 2)
		lcl_lmt[i] = ((lcl_lmt[i-1] + lcl_lmt[i+1] + 1)>>1);

	if (hstEnd <= hstBgn)
		return;

	cLmt = (adp_crate * tsum) >> 2;
	cLmt = (cLmt + 2048) >> 12;

	tAvg = (tsum + tLen/2)/tLen;

	/* sort histogram */
	for (i = hstBgn; i < hstEnd; i++) {
		for (j = hstBgn; j < (hstEnd - i - 1); j++) {
			if (tHst[j] < tHst[j+1]) {
				nExc = tHst[j];
				tHst[j] = tHst[j+1];
				tHst[j+1] = nExc;

				nNum = idx[j];
				idx[j] = idx[j+1];
				idx[j+1] = nNum;
			}
		}
	}

	/* local clip rate */
	if (ve_dnlp_glb_crate == 0) {
		for (i = 0; i < 64; i++)
			lcl_lmt[i] = ((lcl_lmt[i]*cLmt+32) >> 6);
	} else {
		for (i = 0; i < 64; i++)
			lcl_lmt[i] = cLmt;
	}
	/* the largest bins should be improved */
	nStp = tAvg * ve_dnlp_hghbin + 32;
	nStp = (nStp >> 6);
	for (j = 0; j < ve_dnlp_hghnum; j++) {
		i = idx[j];

		if (lcl_lmt[i] < nStp)
			lcl_lmt[i] = nStp;
	}

	/* the lowest bins */
	nStp = tAvg * ve_dnlp_lowbin + 32;
	nStp = (nStp >> 6);
	for (j = 0; j < ve_dnlp_lownum; j++) {
		i = idx[tLen - 1 - j];

		/* max */
		nT0 = (nStp > tHst[i]) ? nStp : tHst[i];

		if (lcl_lmt[i] > nT0)
			lcl_lmt[i] = nT0;
	}

	/* black protect */
	nStp = tAvg * ve_blk_prct_max + 4;
	nStp = (nStp >> 3);
	for (i = 0; i < ve_blk_prct_rng; i++) {
		nT0 = (ve_blk_prct_rng - i);
		if (iHst[i] > nStp)
			nT0 = nT0 * (iHst[i] - nStp);
		else
			nT0 = 0;

		if (lcl_lmt[i] > (nT0 + tAvg))
			nT0 = lcl_lmt[i] - nT0;
		else
			nT0 = tAvg;

		if (nT0 < tAvg)
			nT0 = tAvg;

		if (prt_flg)
			pr_info("clmt[%d]: %4d -> %4d\n",
				i, lcl_lmt[i], nT0);
	} /* black protect */

	nExc = 0;
	nNum = 0;
	for (i = hstBgn; i < hstEnd; i++) {
		if (iHst[i] > lcl_lmt[i]) {
			nExc += (iHst[i] - lcl_lmt[i]);
			oHst[i] = lcl_lmt[i];
		} else {
			nNum++;
			oHst[i] = iHst[i];
		}
	}

	if (clip_rate <= 8 || tAvg <= 2) {
		cLmt = (tsum + tLen/2)/tLen;
		tsum = cLmt*tLen;
		for (i = hstBgn; i < hstEnd; i++)
			oHst[i] = cLmt;

	} else {
		while ((nNum > 0) && (nExc > 0)) {
			nStp = (nExc+nNum/2)/nNum;

			tExc = nExc;

			for (j = 0; j < 64; j++) {
				i = idx[j];
				if ((i < hstBgn) || (i > hstEnd-1))
					continue;

				if ((oHst[i] + nStp) < lcl_lmt[i]) {
					oHst[i] = oHst[i] + nStp;
					nExc = nExc - nStp;
				} else if (lcl_lmt[i] > oHst[i]) {
					oHst[i] = lcl_lmt[i];
					nExc = nExc - (lcl_lmt[i] - oHst[i]);
					nNum = nNum - 1;
				}

				if ((nNum <= 0) || (nExc <= 0))
					break;
			}

			if (nExc == tExc)
				break;
		} /* end while */

		if (nNum == 0 && tExc > 0 && nExc > tLen) {
			nStp = (nExc + tLen/2)/tLen;
			for (i = hstBgn; i < hstEnd; i++)
				oHst[i] = oHst[i] + nStp;
		}
	}

	/*hstBgn:hstEnd-1*/
	tsum = 0;
	for (i = hstBgn; i < hstEnd; i++) {
		/*if(oHst[i]>cLmt)*/
		/*oHst[i] = cLmt;*/
		tsum += oHst[i];
	}

	nStp = 0;
	/*sum -= oHst[4];*/
	for (i = hstBgn; i < hstEnd; i++) {
		nStp += oHst[i];

		j = ((hstEnd - hstBgn)*nStp << 4);
		j += (tsum>>1);
		j /= tsum;
		oMap[i+1] = j + (hstBgn << 4);
	}

	if (debug_add_curve_en)	{
		for (i = 0; i < 65; i++) {
			oMap[i] = ((128 - glb_clash_curve_bld_rate) * oMap[i] +
			glb_clash_curve_bld_rate * (glb_clash_curve[i] << 2) +
			64) >> 7;
		}
	}


	if (prt_flg)
		for (i = hstBgn; i < hstEnd; i++)
			pr_info("#CL: [%02d: %5d]: %4d => %4d]\n",
				i, iHst[i], i << 4, oMap[i]);
}

/*xhu*/
int old_dnlp_mvreflsh;
int old_dnlp_gmma_rate;
int old_dnlp_lowalpha_new;
int old_dnlp_hghalpha_new;
int old_dnlp_sbgnbnd;
int old_dnlp_sendbnd;
int old_dnlp_cliprate_new;
int old_dnlp_clashBgn;
int old_dnlp_clashEnd;
int old_mtdbld_rate;
int old_dnlp_pst_gmarat;
int old_dnlp_blk_cctr;
int old_dnlp_brgt_ctrl;
int old_dnlp_brgt_range;
int old_dnlp_brght_add;
int old_dnlp_brght_max;
int old_dnlp_lgst_bin;
int old_dnlp_lgst_dst;
int old_dnlp_almst_wht;
int old_dnlp_hghbin;
int old_dnlp_hghnum;
int old_dnlp_lowbin;
int old_dnlp_lownum;
int old_dnlp_bkgend;
int old_dnlp_bkgert;
int old_dnlp_pstgma_brghtrate;
int old_dnlp_pstgma_brghtrat1;
int old_dnlp_blkext;
int old_dnlp_whtext;
int old_dnlp_bextmx;
int old_dnlp_wextmx;
int old_dnlp_wext_autorat;
int old_dnlp_lavg_cum;
int old_dnlp_schg_sft;
bool old_dnlp_smhist_ck;
bool old_dnlp_glb_crate;
int old_dnlp_cuvbld_min;
int old_dnlp_cuvbld_max;
int old_dnlp_dbg_map;
bool old_dnlp_dbg_adjavg;
int old_dnlp_dbg_i2r;
int old_dnlp_slow_end;
int old_dnlp_pavg_btsft;
int old_dnlp_dbg0331;
int old_dnlp_cliprate_min;
int old_dnlp_adpcrat_lbnd;
int old_dnlp_adpcrat_hbnd;
int old_dnlp_adpmtd_lbnd;
int	old_dnlp_adpmtd_hbnd;
int	old_dnlp_set_bext;
int	old_dnlp_set_wext;

int old_dnlp_satur_rat;
int old_dnlp_satur_max;
int	old_blk_prct_rng;
int	old_blk_prct_max;

int old_dnlp_lowrange;
int old_dnlp_hghrange;
int old_dnlp_auto_rng;

int old_dnlp_bin0_absmax;
int old_dnlp_bin0_sbtmax;

int old_dnlp_adpalpha_lrate;
int old_dnlp_adpalpha_hrate;

static int cal_brght_plus(int luma_avg4, int low_lavg4)
{
	int avg_dif = 0;
	int dif_rat = 0;

	int low_rng = 0;
	int low_rat = 0;

	int dnlp_brightness = 0;
	static int pbrtness;

	if (luma_avg4 > low_lavg4)
		avg_dif = luma_avg4 - low_lavg4;

	if (avg_dif < ve_dnlp_blk_cctr)
		dif_rat = ve_dnlp_blk_cctr - avg_dif;

	if (luma_avg4 > ve_dnlp_brgt_ctrl)
		low_rng = luma_avg4 - ve_dnlp_brgt_ctrl;
	else
		low_rng = ve_dnlp_brgt_ctrl - luma_avg4;

	if (low_rng < ve_dnlp_brgt_range)
		low_rat = ve_dnlp_brgt_range - low_rng;

	/* <<2 */
	dnlp_brightness  = (ve_dnlp_brght_max*dif_rat*low_rat + 16)>>5;
	/* add=32 => add 0 */
	dnlp_brightness += ((ve_dnlp_brght_add - 32) << 2);

	if (!dnlp_scn_chg) {
		dnlp_brightness = dnlp_bld_lvl * dnlp_brightness + (RBASE >> 1);
		dnlp_brightness = dnlp_brightness +
				(RBASE - dnlp_bld_lvl) * pbrtness;
		dnlp_brightness = (dnlp_brightness >> ve_dnlp_mvreflsh);
	}
	pbrtness = dnlp_brightness;

	return dnlp_brightness; /* 0 ~ 1024 */
}

int gma_scurve0[65]; /* gamma0 s-curve */
int gma_scurve1[65]; /* gamma1 s-curve */
int gma_scurvet[65]; /* gmma0+gamm1 s-curve */
int clash_curve[65]; /* clash curve */
int clsh_scvbld[65]; /* clash + s-curve blend */

int blk_gma_crv[65]; /* black gamma curve */
int blk_gma_bld[65]; /* blending with black gamma */

int blkwht_ebld[65]; /* black white extension */

/*xhu*/
/* only for debug */
static unsigned int premap0[64];

static int pcurves[8][64];

static void clash_blend(void)
{
	int i = 0;
	int nTmp0 = 0;
	static unsigned int pgmma[65];

	if (!ve_dnlp_luma_sum) {
		for (i = 0; i < 65; i++)
			pgmma[i] = (i << 4); /* 0 ~1024 */
	}

	if (!dnlp_scn_chg && ((ve_dnlp_dbg_i2r >> 3) & 0x1))
		for (i = 0; i < 65; i++) {
			nTmp0 = dnlp_bld_lvl * clash_curve[i] + (RBASE >> 1);
			nTmp0 = nTmp0 + (RBASE - dnlp_bld_lvl) * pgmma[i];
			nTmp0 = (nTmp0 >> ve_dnlp_mvreflsh);
			clash_curve[i] = nTmp0;
		}

	for (i = 0; i < 65; i++)
		pgmma[i] = clash_curve[i];
}

int curve_rfrsh_chk(int hstSum, int rbase)
{
	static unsigned int tLumAvg[30];
	static unsigned int tAvgDif[30];
	bool prt_flg = 0;
	int lSby = 0;
	int bld_lvl = 0;
	int i = 0;

	for (i = 0; i < 29; i++) {
		tLumAvg[i] = tLumAvg[i+1];
		tAvgDif[i] = tAvgDif[i+1];
	}

	tLumAvg[29] = (ve_dnlp_luma_sum + (hstSum >> 1)) / hstSum;
	tLumAvg[29] = ((tLumAvg[29] + 4) >> 3);
	tAvgDif[29] = (tLumAvg[29] > tLumAvg[28]) ?
		(tLumAvg[29] - tLumAvg[28]) : (tLumAvg[28] - tLumAvg[29]);

	/* prt_flg = ((dnlp_printk >> 7) & 0x1); */
	prt_flg = (dnlp_printk & 0x1);

	lSby = 0;
	for (i = 0; i < 8; i++)
		lSby = lSby + tAvgDif[28 - i];
	lSby = ((lSby + 4) >> 3);

	if (tAvgDif[29] > tAvgDif[28])
		bld_lvl = tAvgDif[29] - tAvgDif[28];
	else
		bld_lvl = tAvgDif[28] - tAvgDif[29];

	bld_lvl = (bld_lvl << ve_dnlp_schg_sft);

	if (prt_flg)
		pr_info("bld_lvl=%02d\n", bld_lvl);

	/* play station: return with black scene intersection */
	if (tAvgDif[29] > bld_lvl)
		bld_lvl = tAvgDif[29];

	if (bld_lvl > rbase)
		bld_lvl = rbase;
	else if (bld_lvl < ve_dnlp_cuvbld_min)
		bld_lvl = ve_dnlp_cuvbld_min;
	else if (bld_lvl > ve_dnlp_cuvbld_max)
		bld_lvl = ve_dnlp_cuvbld_max;

	if (prt_flg) {
		pr_info("bld_lvl=%02d, lSby=%02d\n",
			bld_lvl, lSby);

		for (i = 0; i < 10; i++)
			pr_info("tLumAvg[%d]: = %d\n",
			i, tLumAvg[29 - i]);

		for (i = 0; i < 10; i++)
			pr_info("tAvgDif[%d]: = %d\n",
			i, tAvgDif[29 - i]);
	}
	return bld_lvl;
}

static void dnlp3_param_refrsh(void)
{
	if (dnlp_respond) {
		if ((old_dnlp_mvreflsh != ve_dnlp_mvreflsh) ||
			(old_dnlp_gmma_rate != ve_dnlp_gmma_rate) ||
			(old_dnlp_lowalpha_new != ve_dnlp_lowalpha_new) ||
			(old_dnlp_hghalpha_new != ve_dnlp_hghalpha_new) ||
			(old_dnlp_sbgnbnd != ve_dnlp_sbgnbnd) ||
			(old_dnlp_sendbnd != ve_dnlp_sendbnd) ||
			(old_dnlp_cliprate_new != ve_dnlp_cliprate_new) ||
			(old_dnlp_clashBgn != ve_dnlp_clashBgn) ||
			(old_dnlp_clashEnd != ve_dnlp_clashEnd) ||
			(old_mtdbld_rate != ve_mtdbld_rate) ||
			(old_dnlp_pst_gmarat != ve_dnlp_pst_gmarat) ||
			(old_dnlp_blk_cctr != ve_dnlp_blk_cctr) ||
			(old_dnlp_brgt_ctrl != ve_dnlp_brgt_ctrl) ||
			(old_dnlp_brgt_range != ve_dnlp_brgt_range) ||
			(old_dnlp_brght_add != ve_dnlp_brght_add) ||
			(old_dnlp_brght_max != ve_dnlp_brght_max) ||
			(old_dnlp_lgst_bin != ve_dnlp_lgst_bin) ||
			(old_dnlp_lgst_dst != ve_dnlp_lgst_dst) ||
			(old_dnlp_almst_wht != ve_dnlp_almst_wht) ||
			(old_dnlp_glb_crate != ve_dnlp_glb_crate) ||
			(old_dnlp_lrate[0] != ve_dnlp_lrate00)	||
			(old_dnlp_lrate[1] != ve_dnlp_lrate02)	||
			(old_dnlp_lrate[2] != ve_dnlp_lrate04) ||
			(old_dnlp_lrate[3] != ve_dnlp_lrate06) ||
			(old_dnlp_lrate[4] != ve_dnlp_lrate08) ||
			(old_dnlp_lrate[5] != ve_dnlp_lrate10) ||
			(old_dnlp_lrate[6] != ve_dnlp_lrate12) ||
			(old_dnlp_lrate[7] != ve_dnlp_lrate14) ||
			(old_dnlp_lrate[8] != ve_dnlp_lrate16) ||
			(old_dnlp_lrate[9] != ve_dnlp_lrate18) ||
			(old_dnlp_lrate[10] != ve_dnlp_lrate20) ||
			(old_dnlp_lrate[11] != ve_dnlp_lrate22) ||
			(old_dnlp_lrate[12] != ve_dnlp_lrate24) ||
			(old_dnlp_lrate[13] != ve_dnlp_lrate26) ||
			(old_dnlp_lrate[14] != ve_dnlp_lrate28) ||
			(old_dnlp_lrate[15] != ve_dnlp_lrate30) ||
			(old_dnlp_lrate[16] != ve_dnlp_lrate32) ||
			(old_dnlp_lrate[17] != ve_dnlp_lrate34) ||
			(old_dnlp_lrate[18] != ve_dnlp_lrate36) ||
			(old_dnlp_lrate[19] != ve_dnlp_lrate38) ||
			(old_dnlp_lrate[20] != ve_dnlp_lrate40) ||
			(old_dnlp_lrate[21] != ve_dnlp_lrate42) ||
			(old_dnlp_lrate[22] != ve_dnlp_lrate44) ||
			(old_dnlp_lrate[23] != ve_dnlp_lrate46) ||
			(old_dnlp_lrate[24] != ve_dnlp_lrate48) ||
			(old_dnlp_lrate[25] != ve_dnlp_lrate50) ||
			(old_dnlp_lrate[26] != ve_dnlp_lrate52) ||
			(old_dnlp_lrate[27] != ve_dnlp_lrate54) ||
			(old_dnlp_lrate[28] != ve_dnlp_lrate56) ||
			(old_dnlp_lrate[29] != ve_dnlp_lrate58) ||
			(old_dnlp_lrate[30] != ve_dnlp_lrate60) ||
			(old_dnlp_lrate[31] != ve_dnlp_lrate62) ||
			(old_dnlp_hghbin != ve_dnlp_hghbin) ||
			(old_dnlp_hghnum != ve_dnlp_hghnum) ||
			(old_dnlp_lowbin != ve_dnlp_lowbin) ||
			(old_dnlp_lownum != ve_dnlp_lownum) ||
			(old_dnlp_bkgend != ve_dnlp_bkgend) ||
			(old_dnlp_bkgert != ve_dnlp_bkgert) ||
			(old_dnlp_pstgma_brghtrate !=
				ve_dnlp_pstgma_brghtrate) ||
			(old_dnlp_pstgma_brghtrat1 !=
				ve_dnlp_pstgma_brghtrat1) ||
			(old_dnlp_blkext != ve_dnlp_blkext) ||
			(old_dnlp_whtext != ve_dnlp_whtext) ||
			(old_dnlp_bextmx != ve_dnlp_bextmx) ||
			(old_dnlp_wextmx != ve_dnlp_wextmx) ||
			(old_dnlp_wext_autorat != ve_dnlp_wext_autorat) ||
			(old_dnlp_schg_sft != ve_dnlp_schg_sft) ||
			(old_dnlp_smhist_ck != ve_dnlp_smhist_ck) ||
			(old_dnlp_cuvbld_min != ve_dnlp_cuvbld_min) ||
			(old_dnlp_cuvbld_max != ve_dnlp_cuvbld_max) ||
			(old_dnlp_dbg_map != ve_dnlp_dbg_map) ||
			(old_dnlp_dbg_adjavg != ve_dnlp_dbg_adjavg) ||
			(old_dnlp_dbg_i2r != ve_dnlp_dbg_i2r) ||
			(old_dnlp_slow_end != ve_dnlp_slow_end) ||
			(old_dnlp_pavg_btsft != ve_dnlp_pavg_btsft) ||
			(old_dnlp_cliprate_min != ve_dnlp_cliprate_min) ||
			(old_dnlp_adpcrat_lbnd != ve_dnlp_adpcrat_lbnd) ||
			(old_dnlp_adpcrat_hbnd != ve_dnlp_adpcrat_hbnd) ||
			(old_dnlp_adpmtd_lbnd != ve_dnlp_adpmtd_lbnd) ||
			(old_dnlp_adpmtd_hbnd != ve_dnlp_adpmtd_hbnd) ||
			(old_dnlp_set_bext != ve_dnlp_set_bext) ||
			(old_dnlp_set_wext != ve_dnlp_set_wext) ||
			(old_dnlp_satur_rat != ve_dnlp_satur_rat) ||
			(old_dnlp_satur_max != ve_dnlp_satur_max) ||
			(old_blk_prct_rng != ve_blk_prct_rng) ||
			(old_blk_prct_max != ve_blk_prct_max) ||
			(old_dnlp_lowrange != ve_dnlp_lowrange) ||
			(old_dnlp_hghrange != ve_dnlp_hghrange) ||
			(old_dnlp_auto_rng != ve_dnlp_auto_rng) ||
			(old_dnlp_bin0_absmax != ve_dnlp_bin0_absmax) ||
			(old_dnlp_bin0_sbtmax != ve_dnlp_bin0_sbtmax) ||
			(old_dnlp_adpalpha_lrate != ve_dnlp_adpalpha_lrate) ||
			(old_dnlp_adpalpha_hrate != ve_dnlp_adpalpha_hrate))
			ve_dnlp_respond_flag = 1;
		else
			ve_dnlp_respond_flag = 0;
	}

	old_dnlp_mvreflsh = ve_dnlp_mvreflsh;
	old_dnlp_gmma_rate = ve_dnlp_gmma_rate;
	old_dnlp_lowalpha_new = ve_dnlp_lowalpha_new;
	old_dnlp_hghalpha_new = ve_dnlp_hghalpha_new;
	old_dnlp_sbgnbnd = ve_dnlp_sbgnbnd;
	old_dnlp_sendbnd = ve_dnlp_sendbnd;
	old_dnlp_cliprate_new = ve_dnlp_cliprate_new;
	old_dnlp_clashBgn = ve_dnlp_clashBgn;
	old_dnlp_clashEnd = ve_dnlp_clashEnd;
	old_mtdbld_rate = ve_mtdbld_rate;
	old_dnlp_pst_gmarat = ve_dnlp_pst_gmarat;
	old_dnlp_blk_cctr = ve_dnlp_blk_cctr;
	old_dnlp_brgt_ctrl = ve_dnlp_brgt_ctrl;
	old_dnlp_brgt_range = ve_dnlp_brgt_range;
	old_dnlp_brght_add = ve_dnlp_brght_add;
	old_dnlp_brght_max = ve_dnlp_brght_max;
	old_dnlp_lgst_bin = ve_dnlp_lgst_bin;
	old_dnlp_lgst_dst = ve_dnlp_lgst_dst;
	old_dnlp_almst_wht = ve_dnlp_almst_wht;
	old_dnlp_glb_crate = ve_dnlp_glb_crate;
	old_dnlp_lrate[0] = ve_dnlp_lrate00;
	old_dnlp_lrate[1] = ve_dnlp_lrate02;
	old_dnlp_lrate[2] = ve_dnlp_lrate04;
	old_dnlp_lrate[3] = ve_dnlp_lrate06;
	old_dnlp_lrate[4] = ve_dnlp_lrate08;
	old_dnlp_lrate[5] = ve_dnlp_lrate10;
	old_dnlp_lrate[6] = ve_dnlp_lrate12;
	old_dnlp_lrate[7] = ve_dnlp_lrate14;
	old_dnlp_lrate[8] = ve_dnlp_lrate16;
	old_dnlp_lrate[9] = ve_dnlp_lrate18;
	old_dnlp_lrate[10] = ve_dnlp_lrate20;
	old_dnlp_lrate[11] = ve_dnlp_lrate22;
	old_dnlp_lrate[12] = ve_dnlp_lrate24;
	old_dnlp_lrate[13] = ve_dnlp_lrate26;
	old_dnlp_lrate[14] = ve_dnlp_lrate28;
	old_dnlp_lrate[15] = ve_dnlp_lrate30;
	old_dnlp_lrate[16] = ve_dnlp_lrate32;
	old_dnlp_lrate[17] = ve_dnlp_lrate34;
	old_dnlp_lrate[18] = ve_dnlp_lrate36;
	old_dnlp_lrate[19] = ve_dnlp_lrate38;
	old_dnlp_lrate[20] = ve_dnlp_lrate40;
	old_dnlp_lrate[21] = ve_dnlp_lrate42;
	old_dnlp_lrate[22] = ve_dnlp_lrate44;
	old_dnlp_lrate[23] = ve_dnlp_lrate46;
	old_dnlp_lrate[24] = ve_dnlp_lrate48;
	old_dnlp_lrate[25] = ve_dnlp_lrate50;
	old_dnlp_lrate[26] = ve_dnlp_lrate52;
	old_dnlp_lrate[27] = ve_dnlp_lrate54;
	old_dnlp_lrate[28] = ve_dnlp_lrate56;
	old_dnlp_lrate[29] = ve_dnlp_lrate58;
	old_dnlp_lrate[30] = ve_dnlp_lrate60;
	old_dnlp_lrate[31] = ve_dnlp_lrate62;
	old_dnlp_hghbin = ve_dnlp_hghbin;
	old_dnlp_hghnum = ve_dnlp_hghnum;
	old_dnlp_lowbin = ve_dnlp_lowbin;
	old_dnlp_lownum = ve_dnlp_lownum;

	old_dnlp_bkgend = ve_dnlp_bkgend;
	old_dnlp_bkgert = ve_dnlp_bkgert;
	old_dnlp_pstgma_brghtrate = ve_dnlp_pstgma_brghtrate;
	old_dnlp_pstgma_brghtrat1 = ve_dnlp_pstgma_brghtrat1;

	old_dnlp_blkext = ve_dnlp_blkext;
	old_dnlp_whtext = ve_dnlp_whtext;
	old_dnlp_bextmx = ve_dnlp_bextmx;
	old_dnlp_wextmx = ve_dnlp_wextmx;
	old_dnlp_wext_autorat = ve_dnlp_wext_autorat;

	old_dnlp_schg_sft = ve_dnlp_schg_sft;

	old_dnlp_smhist_ck = ve_dnlp_smhist_ck;
	old_dnlp_cuvbld_min = ve_dnlp_cuvbld_min;
	old_dnlp_cuvbld_max = ve_dnlp_cuvbld_max;
	old_dnlp_dbg_map = ve_dnlp_dbg_map;
	old_dnlp_dbg_adjavg = ve_dnlp_dbg_adjavg;
	old_dnlp_dbg_i2r = ve_dnlp_dbg_i2r;
	old_dnlp_slow_end = ve_dnlp_slow_end;
	old_dnlp_pavg_btsft = ve_dnlp_pavg_btsft;
	old_dnlp_pavg_btsft = ve_dnlp_pavg_btsft;
	old_dnlp_cliprate_min = ve_dnlp_cliprate_min;
	old_dnlp_adpcrat_lbnd = ve_dnlp_adpcrat_lbnd;
	old_dnlp_adpcrat_hbnd = ve_dnlp_adpcrat_hbnd;

	old_dnlp_adpmtd_lbnd = ve_dnlp_adpmtd_lbnd;
	old_dnlp_adpmtd_hbnd = ve_dnlp_adpmtd_hbnd;
	old_dnlp_set_bext = ve_dnlp_set_bext;
	old_dnlp_set_wext = ve_dnlp_set_wext;

	old_dnlp_satur_rat = ve_dnlp_satur_rat;
	old_dnlp_satur_max = ve_dnlp_satur_max;
	old_blk_prct_rng = ve_blk_prct_rng;
	old_blk_prct_max = ve_blk_prct_max;

	old_dnlp_lowrange = ve_dnlp_lowrange;
	old_dnlp_hghrange = ve_dnlp_hghrange;
	old_dnlp_auto_rng = ve_dnlp_auto_rng;

	old_dnlp_bin0_absmax = ve_dnlp_bin0_absmax;
	old_dnlp_bin0_sbtmax = ve_dnlp_bin0_sbtmax;
	old_dnlp_adpalpha_hrate = ve_dnlp_adpalpha_hrate;
	old_dnlp_adpalpha_lrate = ve_dnlp_adpalpha_lrate;
}

static void dnlp_rfrsh_subgmma(void)
{
	int i = 0;
	static unsigned int pgmma0[65]; /* 0~4096*/
	static unsigned int pgmma1[65];

	if (!ve_dnlp_luma_sum) {
		for (i = 0; i < 65; i++) {
			pgmma0[i] = (i << 6); /* 0 ~4096 */
			pgmma1[i] = (i << 6); /* 0 ~4096 */
		}
	}

	if (!dnlp_scn_chg)
		for (i = 0; i < 65; i++) {
			gma_scurve0[i] = dnlp_bld_lvl *
					(gma_scurve0[i] << 2) + (RBASE >> 1);
			gma_scurve1[i] = dnlp_bld_lvl *
					(gma_scurve1[i] << 2) + (RBASE >> 1);

			gma_scurve0[i] = gma_scurve0[i] +
					(RBASE - dnlp_bld_lvl) * pgmma0[i];
			gma_scurve1[i] = gma_scurve1[i] +
					(RBASE - dnlp_bld_lvl) * pgmma1[i];

			gma_scurve0[i] = (gma_scurve0[i] >> ve_dnlp_mvreflsh);
			gma_scurve1[i] = (gma_scurve1[i] >> ve_dnlp_mvreflsh);

			pgmma0[i] = gma_scurve0[i]; /* 0~ 4095 */
			pgmma1[i] = gma_scurve1[i]; /* 0~ 4095 */

			gma_scurve0[i] = (gma_scurve0[i] + 2) >> 2; /* 1023 */
			gma_scurve1[i] = (gma_scurve1[i] + 2) >> 2; /* 1023 */
		}
	else
		for (i = 0; i < 65; i++) {
			pgmma0[i] = (gma_scurve0[i] << 2);
			pgmma1[i] = (gma_scurve1[i] << 2);
		}
}

static void dnlp_inhist_lpf(void)
{
	int i = 0;
	int nTmp = 0;
	static unsigned int pgmma0[65];
	static unsigned int luma_sum;

	if (!dnlp_scn_chg && (ve_dnlp_dbg_i2r & 0x1)) {
		for (i = 0; i < 65; i++) {
			nTmp = dnlp_bld_lvl * pre_0_gamma[i] + (RBASE >> 1);
			nTmp = nTmp + (RBASE - dnlp_bld_lvl) * pgmma0[i];
			nTmp = (nTmp >> ve_dnlp_mvreflsh);
			pre_0_gamma[i] = nTmp;
		}

		nTmp = dnlp_bld_lvl * ve_dnlp_luma_sum + (RBASE >> 1);
		nTmp = nTmp + (RBASE - dnlp_bld_lvl) * luma_sum;
		nTmp = (nTmp >> ve_dnlp_mvreflsh);
		ve_dnlp_luma_sum = nTmp;
	}

	for (i = 0; i < 65; i++)
		pgmma0[i] = pre_0_gamma[i];
	luma_sum = ve_dnlp_luma_sum;
}


/*0 ~ 65*/
static void dnlp_gmma_cuvs(unsigned int gmma_rate,
	unsigned int low_alpha, unsigned int hgh_alpha,
	unsigned int lsft_avg)
{
	int i = 0;
	int nTmp = 0;
	unsigned int luma_avg4 = (lsft_avg >> ve_dnlp_pavg_btsft);

	static unsigned int pgmma[65];
	bool prt_flg = ((dnlp_printk >> 10) & 0x1);

	if (!ve_dnlp_luma_sum) {
		for (i = 0; i < 65; i++)
			pgmma[i] = (i << 6); /* 0 ~4096 */
	}

	/* refresh sub gamma */
	if ((ve_dnlp_dbg_i2r >> 1) & 0x1)
		dnlp_rfrsh_subgmma();

	if (prt_flg)
		pr_info("gmma_cuvs: %3d %3d %3d %3d\n",
			gmma_rate, low_alpha, hgh_alpha, luma_avg4);

	for (i = 0; i < 65; i++) {
		nTmp = (((256 - gmma_rate)*gma_scurve0[i] +
			gma_scurve1[i]*gmma_rate + 128) >> 8); /* 0 ~1023 */

		if (nTmp <= (luma_avg4<<2))
			nTmp = (nTmp*(64 - low_alpha) +
				(low_alpha*i<<4) + 8)>>4; /*4096*/
		else
			nTmp = (nTmp*(64 - hgh_alpha) +
				(hgh_alpha*i<<4) + 8)>>4;

		if (debug_add_curve_en)	{
			nTmp = ((128 - glb_scurve_bld_rate) * nTmp +
			glb_scurve_bld_rate * (glb_scurve[i] << 4) + 64) >> 7;
		}

		if (nTmp < 0)
			nTmp = 0;
		else if (nTmp > 4095)
			nTmp = 4095;
		gma_scurvet[i] = nTmp;

		if (prt_flg)
			pr_info("gmma_cuvs: [%02d] %4d %4d => %4d\n",
				i, gma_scurve0[i], gma_scurve1[i],
				gma_scurvet[i]);
	}

	if (!dnlp_scn_chg && ((ve_dnlp_dbg_i2r >> 2) & 0x1))
		for (i = 0; i < 65; i++) {
			nTmp = dnlp_bld_lvl * gma_scurvet[i] + (RBASE >> 1);
			nTmp = nTmp + (RBASE - dnlp_bld_lvl) * pgmma[i];
			nTmp = (nTmp >> ve_dnlp_mvreflsh);

			gma_scurvet[i] = nTmp; /* 4095 */
		}

	for (i = 0; i < 65; i++)
		pgmma[i] = gma_scurvet[i]; /* 4095 */

	for (i = 0; i < 65; i++)
		gma_scurvet[i] = ((gma_scurvet[i] + 2) >> 2); /*1023*/
}

/* clsh_scvbld = clash_curve + gma_scurvet */
static void dnlp_clsh_sbld(unsigned int mtdbld_rate)
{
	int i = 0;
	int nTmp0 = 0;

	static unsigned int pgmma[65];

	if (!ve_dnlp_luma_sum) {
		for (i = 0; i < 65; i++)
			pgmma[i] = (i << 4); /* 0 ~1024 */
	}

	for (i = 0; i < 65; i++) {
		nTmp0 = gma_scurvet[i]; /* 0 ~1024 */
		nTmp0 = nTmp0*mtdbld_rate + clash_curve[i]*(64 - mtdbld_rate);
		nTmp0 = (nTmp0 + 32)>>6; /* 0~1024 */
		clsh_scvbld[i] = nTmp0;
	}

	if (!dnlp_scn_chg && ((ve_dnlp_dbg_i2r >> 4) & 0x1))
		for (i = 0; i < 65; i++) {
			nTmp0 = dnlp_bld_lvl * clsh_scvbld[i] + (RBASE >> 1);
			nTmp0 = nTmp0 + (RBASE - dnlp_bld_lvl) * pgmma[i];
			nTmp0 = (nTmp0 >> ve_dnlp_mvreflsh);

			clsh_scvbld[i] = nTmp0;
		}

	for (i = 0; i < 65; i++)
		pgmma[i] = clsh_scvbld[i]; /* 1023 */
}

/* blk_gma_rat[64] */
/* blk_gma_bld = blk_gma_crv + clsh_scvbld */
static void dnlp_blkgma_bld(unsigned int *blk_gma_rat)
{
	int nT1 = 0;
	int nTmp0 = 0;
	int i = 0;
	static unsigned int pgmma[65];

	if (!ve_dnlp_luma_sum) {
		for (i = 0; i < 65; i++)
			pgmma[i] = (i << 4); /* 0 ~1024 */
	}

	for (i = 0; i < 64; i++) {
		nT1 = blk_gma_rat[i];
		nTmp0 = clsh_scvbld[i];

		nTmp0 = blk_gma_crv[i]*nT1 + nTmp0*(64 - nT1);
		nTmp0 = (nTmp0+32)>>6; /* 0~1024 */
		blk_gma_bld[i] = nTmp0;

		if (debug_add_curve_en)	{
			blk_gma_bld[i] =
			((128 - glb_pst_gamma_bld_rate) * blk_gma_bld[i] +
			glb_pst_gamma_bld_rate *
			(glb_pst_gamma[i] << 2) + 64) >> 7;
		}

		if ((dnlp_printk >> 2) & 0x1)
			pr_info("sc%04d, gm%04d * rat%04d => %04d\n",
				clsh_scvbld[i],  blk_gma_crv[i], nT1, nTmp0);
	}
	blk_gma_bld[64] = 1023;

	if (!dnlp_scn_chg && ((ve_dnlp_dbg_i2r >> 5) & 0x1))
		for (i = 0; i < 65; i++) {
			nTmp0 = dnlp_bld_lvl * blk_gma_bld[i] + (RBASE >> 1);
			nTmp0 = nTmp0 + (RBASE - dnlp_bld_lvl) * pgmma[i];
			nTmp0 = (nTmp0 >> ve_dnlp_mvreflsh);

			blk_gma_bld[i] = nTmp0;
		}

	for (i = 0; i < 65; i++)
		pgmma[i] = blk_gma_bld[i]; /* 1023 */
}

/* blkwht_ebld = blk_gma_bld + extension */
static void dnlp_blkwht_bld(int *blk_wht_ext, int bright,
	unsigned int luma_avg4, unsigned int luma_avg,
	unsigned int iRgnBgn, unsigned int iRgnEnd)
{
	int nT0 = 0;
	int nT1 = 0;
	int nTmp0 = 0;
	int i = 0;
	static unsigned int pgmma[65];

	if (!ve_dnlp_luma_sum) {
		for (i = 0; i < 65; i++)
			pgmma[i] = (i << 4); /* 0 ~1024 */
	}

	/* black / white extension */
	for (i = 0; i < 64; i++) {
		nTmp0 = blk_gma_bld[i];
		if ((luma_avg4 > (iRgnBgn << 2)) && (i <= luma_avg)) {
			nT0 = (luma_avg4 - 4*iRgnBgn);
			nT1 = blk_wht_ext[0] * (luma_avg4 - 4*i);
			nT1 += (nT0 >> 1);
			nT1 = nT1 / nT0;

			nT0 = nTmp0 - nT1;
		} else if ((luma_avg4 < 4*(iRgnEnd - 1)) && (i >= luma_avg)) {
			nT0 = 4*(iRgnEnd - 1) - luma_avg4;
			nT1 = blk_wht_ext[1] * (4*i - luma_avg4) + (nT0 >> 1);
			nT1 = nT1 / nT0;

			nT0 = nTmp0 + nT1;
		} else
			nT0 = nTmp0;


		/* nTmp += dnlp_brightness; */
		nT0 = bright + nT0;
		if (nT0 < 0)
			nTmp0 = 0;
		else if (nT0 > 1023)
			nTmp0 = 1023;
		else
			nTmp0 = nT0;

		blkwht_ebld[i] = nT0;
	}
	blkwht_ebld[0]  = 0;
	blkwht_ebld[64] = 1023;

	if (!dnlp_scn_chg && ((ve_dnlp_dbg_i2r >> 6) & 0x1))
		for (i = 0; i < 65; i++) {
			nTmp0 = dnlp_bld_lvl * blkwht_ebld[i] + (RBASE >> 1);
			nTmp0 = nTmp0 + (RBASE - dnlp_bld_lvl) * pgmma[i];
			nTmp0 = (nTmp0 >> ve_dnlp_mvreflsh);

			blkwht_ebld[i] = nTmp0;
		}

	for (i = 0; i < 65; i++)
		pgmma[i] = blkwht_ebld[i]; /* 1023 */
}

static void dnlp_params_hist(unsigned int *gmma_rate,
	unsigned int *low_alpha, unsigned int *hgh_alpha,
	unsigned int *mtdbld_rate,
	unsigned int luma_avg, unsigned int luma_avg4)
{
	static unsigned int pgmma0[4][7];
	int nTmp = 0;
	int i = 0;
	int trate = *gmma_rate;
	int tlowa = *low_alpha;
	int thgha = *hgh_alpha;
	int tmrat = *mtdbld_rate;

	nTmp = (luma_avg > 31) ? luma_avg-31 : 31-luma_avg;
	nTmp = (32 - nTmp + 2) >> 2;

	trate = trate + nTmp;
	if (trate > 255)
		trate = 255;

	if (luma_avg4 <= 32)
		tlowa = tlowa + (32 - luma_avg4);

	if (luma_avg4 >= 224) {
		if (tlowa < (luma_avg4 - 224))
			tlowa = 0;
		else
			tlowa = tlowa - (luma_avg4 - 224);
	}

	if (!dnlp_scn_chg) {
		for (i = 0; i < 7; i++) {
			trate += pgmma0[0][i];
			tlowa += pgmma0[1][i];
			thgha += pgmma0[2][i];
			tmrat += pgmma0[3][i];
		}
		trate = ((trate + 4)>>3);
		tlowa = ((tlowa + 4)>>3);
		thgha = ((thgha + 4)>>3);
		tmrat = ((tmrat + 4)>>3);

		for (i = 0; i < 6; i++) {
			pgmma0[0][i] = pgmma0[0][i + 1];
			pgmma0[1][i] = pgmma0[1][i + 1];
			pgmma0[2][i] = pgmma0[2][i + 1];
			pgmma0[3][i] = pgmma0[3][i + 1];
		}
		pgmma0[0][6] = trate;
		pgmma0[1][6] = tlowa;
		pgmma0[2][6] = thgha;
		pgmma0[3][6] = tmrat;
	} else
		for (i = 0; i < 7; i++) {
			pgmma0[0][i] = trate;
			pgmma0[1][i] = tlowa;
			pgmma0[2][i] = thgha;
			pgmma0[3][i] = tmrat;
		}
	*gmma_rate = trate;
	*low_alpha = tlowa;
	*hgh_alpha = thgha;
	*mtdbld_rate = tmrat;
}

/* black bord detection, and histogram clipping */
static void dnlp_refine_bin0(int hstSum)
{
	static unsigned int tmp_sum[7];
	unsigned int nTmp = 0;
	unsigned int nTmp0 = 0;
	unsigned int nsum = 0;
	int j = 0;

	nTmp = (hstSum * ve_dnlp_bin0_absmax + 128) >> 8;
	nTmp0 = pre_0_gamma[1] + pre_0_gamma[2];
	if (nTmp0 > nTmp)
		nTmp = nTmp0;

	if (pre_0_gamma[0] > nTmp) {
		if (pre_0_gamma[1] > nTmp)
			nTmp = pre_0_gamma[1];
		if (pre_0_gamma[2] > nTmp)
			nTmp = pre_0_gamma[2];

		nsum = pre_0_gamma[0] - nTmp;

		nTmp = (hstSum * ve_dnlp_bin0_sbtmax + 128) >> 8;
		if (nsum > nTmp)
			nsum = nTmp;
	}

	if (!dnlp_scn_chg) {
		for (j = 0; j < 7; j++)
			nsum += tmp_sum[j];
		nsum = ((nsum + 4) >> 3);

		for (j = 0; j < 6; j++)
			tmp_sum[j] = tmp_sum[j + 1];
		tmp_sum[6] = nsum;
	} else {
		for (j = 0; j < 7; j++)
			tmp_sum[j] = nsum;
	}

	if (dnlp_printk & 0x1)
		pr_info("Bin0 Refine: -%4d\n", nsum);

	if (nsum >= pre_0_gamma[0])
		pre_0_gamma[0] = 0;
	else
		pre_0_gamma[0] = pre_0_gamma[0] - nsum;
}

static void dnlp_adp_alpharate(unsigned int *lmh_avg,
	unsigned int *low_alpha, unsigned int *hgh_alpha,
	unsigned int *pst_gmarat,
	unsigned int dnlp_lowrange, unsigned int dnlp_hghrange)
{
	int nTmp = 0;
	int ndif = 0;
	int nlap = 0;
	int nbrt0 = 0;
	int nbrt1 = 0;

	if ((dnlp_lowrange + lmh_avg[3]) < 64) { /* decrease low alpha */
		nTmp = 64 - (dnlp_lowrange + lmh_avg[3]);
		nTmp = (ve_dnlp_adpalpha_lrate * nTmp + 16) >> 5;
		if (*low_alpha < nTmp)
			*low_alpha = 0;
		else
			*low_alpha = *low_alpha - nTmp;

		if (dnlp_printk)
			pr_info("low alpha-- (%3d) -> %2d\n", nTmp, *low_alpha);
	} else if (lmh_avg[3] > 64) { /* increase low alpha */
		ndif = lmh_avg[3] - 64;
		nlap = (ve_dnlp_adpalpha_lrate * ndif + 16) >> 5;
		if ((nlap + *low_alpha) > 64)
			*low_alpha = 64;
		else
			*low_alpha += nlap;

		if (lmh_avg[4] < 16) {
			nbrt0 = ve_dnlp_pstgma_brghtrat1 * (16 - lmh_avg[4]);
			nbrt0 = (nbrt0 + 8) >> 4;
		}
		nbrt1 = (ve_dnlp_pstgma_brghtrate * ndif + 16) >> 6;

		nTmp = nbrt0 + nbrt1;

		if ((*pst_gmarat + nTmp) > 64)
			*pst_gmarat = 64;
		else
			*pst_gmarat += nTmp;

		if (dnlp_printk)
			pr_info("low alpha++ (%3d) -> %2d pstgma(+%2d +%2d)(%2d)\n",
			nlap, *low_alpha, nbrt0, nbrt1, *pst_gmarat);
	}

	if (lmh_avg[2] < 64 - dnlp_hghrange) { /* decrease hgh alpha */
		nTmp = 64 - dnlp_hghrange - lmh_avg[2];
		nTmp = (ve_dnlp_adpalpha_hrate * nTmp + 16) >> 5;
		if (*hgh_alpha < nTmp)
			*hgh_alpha = 0;
		else
			*hgh_alpha = *hgh_alpha - nTmp;
		if (dnlp_printk)
			pr_info("hgh alpha-- (%3d) -> %2d\n", nTmp, *hgh_alpha);
	} else if (lmh_avg[2] > 63) { /* increase hgh alpha */
		nTmp = lmh_avg[2] - 63;
		nTmp = (ve_dnlp_adpalpha_hrate * nTmp + 16) >> 5;
		if ((nTmp + *hgh_alpha) > 64)
			*hgh_alpha = 64;
		else
			*hgh_alpha += nTmp;

		if (dnlp_printk)
			pr_info("hgh alpha++ (%3d) -> %2d\n", nTmp, *hgh_alpha);
	}

}

static int PreTstDat[28];
static int CrtTstDat[28];

static void dnlp_tgt_sort(void)
{
	int i = 0;
	int j = 0;
	unsigned char t = 0;
	int chk = 0;
	/* unsigned char ve_dnlp_tgt[64]; */
	for (j = 0; j < 63; j++) {
		chk = 0;
		for (i = 0; i < (63 - i); i++) {
			if (ve_dnlp_tgt[i] > ve_dnlp_tgt[i+1]) {
				t = ve_dnlp_tgt[i];
				ve_dnlp_tgt[i] = ve_dnlp_tgt[i+1];
				ve_dnlp_tgt[i+1] = t;
				chk = chk+1;
			}
		}
		if (chk == 0)
			break;
	}
}

static void ve_dnlp_calculate_tgtx_new(struct vframe_s *vf)
{
	struct vframe_prop_s *p = &vf->prop;

	int hstSum = 0;

	/* Black gamma rate: global -> auto local */
	unsigned int blk_gma_rat[64];

	static unsigned int nTstCnt;

	int i = 0;
	/*static unsigned int sum_b;*/
	unsigned int sum = 0, max = 0;
	unsigned int nTmp = 0;
	int nT0 = 0, nT1 = 0;
	int nTmp0 = 0;

	int dnlp_brightness = 0;
	unsigned int mMaxLst[4];
	unsigned int mMaxIdx[4];
	int blk_wht_ext[2] = {0, 0};
	static unsigned int pre_stur;
	unsigned int dnlp_auto_rng = 0;

	/*u4[0-8] smooth moving,reflesh the curve,0-refresh one frame*/
	/*u8larger-->near to gamma1.8, smaller->gamma1.2 [0-256]dft60*/
	unsigned int gmma_rate = (unsigned int) ve_dnlp_gmma_rate;
	/* u6[0-64]dft20*/
	unsigned int low_alpha = (unsigned int) ve_dnlp_lowalpha_new;

	/* u6[0-64]dft28*/
	unsigned int hgh_alpha = (unsigned int) ve_dnlp_hghalpha_new;

	/*u4s-curve begin band [0-16]dft0*/
	unsigned int sBgnBnd = (unsigned int) ve_dnlp_sbgnbnd;

	/*u4s-curve begin band [0-16]dft5*/
	unsigned int sEndBnd = (unsigned int) ve_dnlp_sendbnd;

	/*u4clash hist begin point [0-16] dft0*/
	unsigned int clashBgn = (unsigned int) ve_dnlp_clashBgn;

	/*u4 clash hist end point [0~15] dft10*/
	unsigned int clashEnd = (unsigned int) ve_dnlp_clashEnd+49;

	/*please add the parameters*/
	/*u6method blending rate (0~64) dft32*/
	unsigned int mtdbld_rate = (unsigned int) ve_mtdbld_rate;

	/*u6 dft32*/
	unsigned int dnlp_pst_gmarat = (unsigned int) ve_dnlp_pst_gmarat;

	unsigned int dnlp_lowrange = (unsigned int) ve_dnlp_lowrange;
	unsigned int dnlp_hghrange = (unsigned int) ve_dnlp_hghrange;
	unsigned int dnlp_bkgert   = (unsigned int) ve_dnlp_bkgert;

	/*-------------------------------------------------*/
	unsigned int lsft_avg = 0; /*luma shift average */
	unsigned int luma_avg = 0;
	unsigned int luma_avg4 = 0;
	unsigned int low_lavg4 = 0; /*low luma average*/
	unsigned int lmh_avg[5] = {0, 0, 0, 0, 0};
	/* low/mid/hgh tone average */

	unsigned int ihstBgn = 0;
	unsigned int ihstEnd = 0;
	bool prt_flg = 0;

	unsigned int rGm1p2[] = {0, 2, 4, 7, 9, 12, 15,
					18, 21, 24, 28, 31, 34,
					38, 41, 45, 49, 52, 56,
					60, 63, 67, 71, 75, 79,
					83, 87, 91, 95, 99, 103,
					107, 111, 116, 120, 124,
					128, 133, 137, 141, 146,
					150, 154, 159, 163, 168,
					172, 177, 181, 186, 190,
					195, 200, 204, 209, 213,
					218, 223, 227, 232, 237,
					242, 246, 251, 255};

	/* 2.0 for full range */
	unsigned int rGm1p8[] = {0, 0, 0, 1, 1, 2, 2, 3,
	    4, 5, 6, 8, 9, 11, 12, 14,
	    16, 18, 20, 23, 25, 28, 30, 33,
		36, 39, 42, 46, 49, 53, 56, 60,
		64, 68, 72, 77, 81, 86, 90, 95,
		100, 105, 110, 116, 121, 127, 132, 138,
		144, 150, 156, 163,
		169, 176, 182, 189,
		196, 203, 210, 218,
		225, 233, 240, 248, 255};

	if (ve_dnlp_mvreflsh < 1)
		ve_dnlp_mvreflsh = 1;

	RBASE = (1 << ve_dnlp_mvreflsh);

	/* parameters refresh */
	dnlp3_param_refrsh();
	dnlp_scn_chg = 0;

	if (low_alpha > 64)
		low_alpha = 64;
	if (hgh_alpha > 64)
		hgh_alpha = 64;
	if (clashBgn > 16)
		clashBgn = 16;
	if (clashEnd > 64)
		clashEnd = 64;
	if (clashEnd < 49)
		clashEnd = 49;

	/* old historic luma sum*/
	/*sum_b = ve_dnlp_luma_sum;*/
	/* new historic luma sum*/
	if (hist_sel)
		ve_dnlp_luma_sum = p->hist.vpp_luma_sum;
	else
		ve_dnlp_luma_sum = p->hist.luma_sum;

	/* counter the calling function */
	nTstCnt++;
	if (nTstCnt > 240)
		nTstCnt = 0;

	nT0 = 0; /* counter the same histogram */
	hstSum = 0;
	for (i = 0; i < 64; i++) {
		pre_1_gamma[i] = pre_0_gamma[i];
		if (hist_sel)
			pre_0_gamma[i] = (unsigned int)p->hist.vpp_gamma[i];
		else
			pre_0_gamma[i] = (unsigned int)p->hist.gamma[i];

		/* counter the same histogram */
		if (pre_1_gamma[i] == pre_0_gamma[i])
			nT0++;
		else if (pre_1_gamma[i] > pre_0_gamma[i])
			nT1 = (pre_1_gamma[i] - pre_0_gamma[i]);
		else
			nT1 = (pre_0_gamma[i] - pre_1_gamma[i]);

		hstSum += pre_0_gamma[i];
	}

	if (dnlp_printk)
		pr_info("\nRflsh%03d: %02d same bins hstSum(%d)\n",
			nTstCnt, nT0, hstSum);

	if (!ve_dnlp_luma_sum) {
		/*new luma sum is 0,something is wrong,freeze dnlp curve*/
		dnlp_scn_chg = 1;
		return;
	}

	for (i = 0; i < 28; i++)
		PreTstDat[i] = CrtTstDat[i];

	dnlp_bld_lvl = curve_rfrsh_chk(hstSum, RBASE);
	CrtTstDat[0] = dnlp_bld_lvl;

	if (ve_dnlp_respond_flag) {
		dnlp_bld_lvl = RBASE;
		dnlp_scn_chg = 1;
	} else if (dnlp_bld_lvl >= RBASE) {
		dnlp_bld_lvl = RBASE;
		dnlp_scn_chg = 1;
	}

	CrtTstDat[1] = dnlp_bld_lvl;

	/* black bord detection, and histogram clipping */
	dnlp_refine_bin0(hstSum);

	/* histogram and luma_sum filters */
	dnlp_inhist_lpf();

	hstSum = 0;
	for (i = 0; i < 64; i++) {
		if (pre_0_gamma[i] != 0) {
			if (ihstBgn == 0 && pre_0_gamma[0] == 0)
				ihstBgn = i;
			if (ihstEnd != 64)
				ihstEnd = i+1;
		}
		clash_curve[i] = (i<<4); /* 0~1024 */

		hstSum += pre_0_gamma[i];
	}
	clash_curve[64] = 1024;

	if (ve_dnlp_limit_rng) {
		iRgnBgn = 4; /* i=ihstBgn, i<ihstEnd */
		iRgnEnd = 59;
	} else {
		iRgnBgn = 0;
		iRgnEnd = 64;
	}
	CrtTstDat[6] = iRgnBgn;
	CrtTstDat[7] = iRgnEnd;

	if (ve_dnlp_range_det) {
		if (ihstBgn <= 4)
			iRgnBgn = ihstBgn;

		if (ihstEnd >= 59)
			iRgnEnd = ihstEnd;
	}
	CrtTstDat[8] = iRgnBgn;
	CrtTstDat[9] = iRgnEnd;

	/* all the same */
	if (nT0 == 64 && ve_dnlp_smhist_ck && (!ve_dnlp_respond_flag))
		return;

	sum = 0;
	max = 0;
	luma_avg = 0;

	/*Get the maximum4*/
	mMaxLst[0] = 0;
	mMaxLst[1] = 0;
	mMaxLst[2] = 0;
	mMaxLst[3] = 0;
	mMaxIdx[0] = 0;
	mMaxIdx[1] = 0;
	mMaxIdx[2] = 0;
	mMaxIdx[3] = 0;
	nT0 = 0;
	for (i = iRgnBgn; i < iRgnEnd; i++) {
		nTmp = pre_0_gamma[i];

		sum += nTmp;

		if (max < nTmp)
			max = nTmp;

		/*lower extension [0-63]*/
		nTmp0 = nTmp*i;
		luma_avg += nTmp0;

		if (i == 31)
			low_lavg4 = luma_avg; /*low luma average*/

		/*Get the maximum4*/
		for (nT0 = 0; nT0 < 4; nT0++) {
			if (nTmp >= mMaxLst[nT0]) {
				for (nT1 = 3; nT1 >= nT0+1; nT1--) {
					mMaxLst[nT1] = mMaxLst[nT1-1];
					mMaxIdx[nT1] = mMaxIdx[nT1-1];
				}

				mMaxLst[nT0] = nTmp;
				mMaxIdx[nT0] = i;
				break;
			}
		}
	}

	if (dnlp_printk & 0x1) {
		pr_info("#Bins:  Pre-hist => Crt-Hist\n");
		for (i = 0; i < 64; i++)
			pr_info("[%03d,%03d): %05d => %05d\n",
					4*i, 4*(i+1), pre_1_gamma[i],
					pre_0_gamma[i]);
	    /* new historic luma sum*/
		pr_info("luma s=%x, hist-sum=%d max=%d\n",
			ve_dnlp_luma_sum, sum, max);
	}

	/*invalid histgram: freeze dnlp curve*/
	if ((max <= 55 || sum == 0) && (!ve_dnlp_respond_flag))
		return;

	lsft_avg = (luma_avg << (2 + ve_dnlp_pavg_btsft));
	lsft_avg = (lsft_avg + (sum >> 1)) / sum;
	luma_avg4 = (lsft_avg >> ve_dnlp_pavg_btsft);
	luma_avg  = (luma_avg4>>2);

	if (mMaxIdx[0] == 0)
		nTmp = (mMaxIdx[1] * 2) + mMaxIdx[2] + mMaxIdx[3];
	else {
		if (mMaxIdx[1] == 0)
			nTmp = mMaxIdx[0] + mMaxIdx[2] + mMaxIdx[3];
		else if (mMaxIdx[2] == 0)
			nTmp = mMaxIdx[0] + mMaxIdx[1] + mMaxIdx[3];
		else if (mMaxIdx[3] == 0)
			nTmp = mMaxIdx[0] + mMaxIdx[1] + mMaxIdx[2];
		else
			nTmp = mMaxIdx[1] + mMaxIdx[2] + mMaxIdx[3];

		nTmp += mMaxIdx[0];
	}
	lmh_avg[4] = nTmp;

	prt_flg = (dnlp_printk & 0x1);
	if (prt_flg) {
		pr_info("Max: %04d(%d) > %04d(%d) > %04d(%d) > %04d(%d) => %d\n",
		mMaxLst[0], mMaxIdx[0], mMaxLst[1], mMaxIdx[1],
		mMaxLst[2], mMaxIdx[2], mMaxLst[3], mMaxIdx[3], nTmp);
		pr_info("%d => %d (%d)\n", luma_avg, lsft_avg, luma_avg4);
	}

	CrtTstDat[10] = lsft_avg;
	CrtTstDat[11] = luma_avg4;

	low_lavg4 = 4*low_lavg4/sum;
	if (dnlp_printk)
		pr_info("[avg1]= (%02d %02d) (%4d) (%3d, %3d)\n",
			luma_avg, luma_avg4, lsft_avg,
			gmma_rate, low_alpha);

	dnlp_brightness = cal_brght_plus(luma_avg4, low_lavg4);

	CrtTstDat[12] = low_lavg4;
	CrtTstDat[14] = dnlp_brightness;

	/*150918 for 32-step luma pattern*/
	if (ve_dnlp_dbg_adjavg)
		luma_avg4 = AdjHistAvg(luma_avg4, ihstEnd);

	luma_avg4 = cal_hist_avg(luma_avg4);
	luma_avg = (luma_avg4>>2);

	if (luma_avg < ve_dnlp_auto_rng)
		dnlp_auto_rng = luma_avg;
	else if ((luma_avg + ve_dnlp_auto_rng) > 64)
		dnlp_auto_rng = 64 - luma_avg;
	else
		dnlp_auto_rng = ve_dnlp_auto_rng;

	if (dnlp_auto_rng < 2)
		dnlp_auto_rng = 2;
	else if (dnlp_auto_rng > 10)
		dnlp_auto_rng = 10;

	if (ve_dnlp_auto_rng > 0) {
		if (luma_avg <= dnlp_auto_rng + 2) {
			dnlp_lowrange = 2;
			dnlp_hghrange = 64 - (luma_avg + dnlp_auto_rng);
		} else if (luma_avg >= 61 - dnlp_auto_rng) {
			dnlp_lowrange = luma_avg - dnlp_auto_rng;
			dnlp_hghrange = 2;
		} else {
			dnlp_lowrange = luma_avg - dnlp_auto_rng;
			dnlp_hghrange = (63 - (luma_avg + dnlp_auto_rng));
		}
	} else {
		dnlp_lowrange = ve_dnlp_lowrange;
		dnlp_hghrange = ve_dnlp_hghrange;
	}
	if (dnlp_lowrange > 31)
		dnlp_lowrange = 31;
	else if (dnlp_lowrange < 2)
		dnlp_lowrange = 2;
	if (dnlp_hghrange > 31)
		dnlp_hghrange = 31;
	else if (dnlp_hghrange < 2)
		dnlp_hghrange = 2;

	for (i = iRgnBgn; i < iRgnEnd; i++) {
		nTmp0 = pre_0_gamma[i] * i;

		if (i < dnlp_lowrange) {/* low tone */
			lmh_avg[0] += nTmp0;
			lmh_avg[3] += pre_0_gamma[i] * (64 - i);
		} else if (i > (63 - dnlp_hghrange))  /* hgh tone */
			lmh_avg[2] += nTmp0;
		else /* mid tone */
			lmh_avg[1] += nTmp0;
	}

	/* low/mid/high tone average */
	lmh_avg[0] = (lmh_avg[0] << 6) / dnlp_lowrange;
	lmh_avg[3] = (lmh_avg[3] << 6) / dnlp_lowrange;
	lmh_avg[1] = (lmh_avg[1] << 6) / (64 - dnlp_lowrange - dnlp_hghrange);
	lmh_avg[2] = (lmh_avg[2] << 6) / dnlp_hghrange;

	lmh_avg[0] = (lmh_avg[0] + (sum >> 1)) / (sum + 1);
	lmh_avg[3] = (lmh_avg[3] + (sum >> 1)) / (sum + 1);
	lmh_avg[1] = (lmh_avg[1] + (sum >> 1)) / (sum + 1);
	lmh_avg[2] = (lmh_avg[2] + (sum >> 1)) / (sum + 1);
	/*lmh_avg[3] = 64 - lmh_avg[3];*/

	/* adaptive method rate */
	if (dnlp_printk)
		pr_info("Adp Mtd/PostGm Rate: %d %d gm(%d)",
			luma_avg, mtdbld_rate, dnlp_pst_gmarat);

	mtdbld_rate = dnlp_adp_mtdrate(mtdbld_rate, luma_avg);

	/* post gamma rate: global -> auto local */
	if (ve_dnlp_pst_gmarat > 64)
		dnlp_pst_gmarat = ve_dnlp_pst_gmarat - 64;
	else
		dnlp_pst_gmarat = 64 - ve_dnlp_pst_gmarat;

	dnlp_adp_alpharate(lmh_avg, &low_alpha, &hgh_alpha,
		&dnlp_pst_gmarat, dnlp_lowrange, dnlp_hghrange);

	if (ve_dnlp_pst_gmarat < 64) /* no adp dnlp_pst_gmarat */
		dnlp_pst_gmarat = 64 - ve_dnlp_pst_gmarat;

	if (dnlp_printk) {
		pr_info("mtdbld: %2d (%2d %2d) => %2d (%2d %2d)\n",
			ve_mtdbld_rate, ve_dnlp_lowalpha_new,
			ve_dnlp_hghalpha_new,
			mtdbld_rate, low_alpha, hgh_alpha);
		pr_info("avg: l[ 0~%2d]=%2d(%2d) m=%2d h[%2d~63]=%2d\n",
			dnlp_lowrange - 1, lmh_avg[0],
			64 - lmh_avg[3], lmh_avg[1],
			64 - dnlp_hghrange, lmh_avg[2]);
	}

	CrtTstDat[2] = clashBgn;
	CrtTstDat[3] = clashEnd;
	clash_fun(clash_curve, pre_0_gamma, clashBgn, clashEnd); /* 0~1024 */
	clash_blend();

	blk_wht_extsn(blk_wht_ext, pre_0_gamma, hstSum, luma_avg); /* 0~1024 */
	CrtTstDat[4] = blk_wht_ext[0];
	CrtTstDat[5] = blk_wht_ext[1];

	if (dnlp_printk)
		pr_info("BlkWhtExt: (%d %d), bldlvl=%02d\n",
			blk_wht_ext[0], blk_wht_ext[1], dnlp_bld_lvl);

	/*patch for black+white stripe*/
	if (mMaxIdx[1] > mMaxIdx[0]) {
		nT0 = mMaxIdx[0];
		nT1 = 63 - mMaxIdx[1];
	} else {
		nT0 = mMaxIdx[1];
		nT1 = 63 - mMaxIdx[0];
	}
	nTmp = (nT0 < nT1) ? nT0 : nT1;
	nTmp = (nTmp > 16) ? 16 : nTmp;

	if ((mMaxLst[1] > (ve_dnlp_lgst_bin*sum>>8)) &&
		((mMaxIdx[1] > (mMaxIdx[0] + ve_dnlp_lgst_dst)) ||
		(mMaxIdx[0] > (mMaxIdx[1] + ve_dnlp_lgst_dst)))) {
		gmma_rate += (nTmp*(255 - gmma_rate)>>4);
		low_alpha -= (low_alpha*nTmp>>4);
		hgh_alpha -= (hgh_alpha*nTmp>>4);
		mtdbld_rate += (nTmp*(64 - mtdbld_rate)>>4);

		if (dnlp_printk)
			pr_info("special case: %d %d %d %d\n",
				gmma_rate, low_alpha, hgh_alpha, mtdbld_rate);
	}
	/* print the parameters */

	lsft_avg  = cal_hst_shft_avg(lsft_avg);
	dnlp_params_hist(&gmma_rate, &low_alpha, &hgh_alpha,
		&mtdbld_rate, luma_avg, luma_avg4);

	if (dnlp_printk & 0x1)
		pr_info("[avg2]= (%3d %3d) (%3d %3d) (%3d %3d)\n",
			luma_avg, luma_avg4, gmma_rate, mtdbld_rate,
			low_alpha, hgh_alpha);

	CrtTstDat[15] = luma_avg4;
	CrtTstDat[16] = luma_avg;
	CrtTstDat[17] = lsft_avg;
	CrtTstDat[18] = gmma_rate;
	CrtTstDat[19] = low_alpha;

	prt_flg = (dnlp_printk & 0x1);
	if (prt_flg) {
		pr_info("Rflsh-check: %03u\n", nTstCnt);
		pr_info("BldLvl= %02d\n", dnlp_bld_lvl);
	}

	if (dnlp_bkgert > dnlp_pst_gmarat)
		dnlp_bkgert = dnlp_pst_gmarat;

	for (i = 0; i < 64; i++) {
		nT1 = dnlp_pst_gmarat;

		if (i > ve_dnlp_bkgend)
			nT1 = dnlp_bkgert;
		else if (ve_dnlp_bkgend > 0) {
			nT1 = dnlp_pst_gmarat - dnlp_bkgert;
			nT1 = nT1 * i + (ve_dnlp_bkgend >> 1);
			nT1 = nT1 / ve_dnlp_bkgend;
			nT1 = dnlp_pst_gmarat - nT1;
		}

		if (ve_dnlp_limit_rng && (i <= 4)) {
			nT1 = i * dnlp_pst_gmarat;
			nT1 = ((nT1 + 2) >> 2);
		}

		if (nT1 < 0)
			nT1 = 0;
		else if (nT1 > 64)
			nT1 = 64;

		blk_gma_rat[i] = nT1;
	}

	/* 0~1024 */
	if (dnlp_printk & 0x1)
		pr_info("[avg3]= %02d %02d => %4d\n",
			luma_avg, luma_avg4, lsft_avg);

	if (ve_dnlp_scv_dbg != 0) {
		nTmp0 = lsft_avg + 16 * ve_dnlp_scv_dbg;
		if (nTmp0 < 0)
			lsft_avg = 0;
		else
			lsft_avg = nTmp0;
	}
	GetGmCurves(gma_scurve0, rGm1p2, lsft_avg, sBgnBnd, sEndBnd);
	GetGmCurves(gma_scurve1, rGm1p8, lsft_avg, sBgnBnd, sEndBnd);
	GetGmBlkCvs(blk_gma_crv, rGm1p8, sBgnBnd, iRgnEnd);

	CrtTstDat[20] = sBgnBnd;
	CrtTstDat[21] = sEndBnd;
	CrtTstDat[22] = lsft_avg;
	CrtTstDat[23] = iRgnEnd;

	prt_flg = (dnlp_printk & 0x1);
	if (prt_flg) {
		pr_info("paramets: %d %d %d %d %d\n",
			luma_avg, gmma_rate, low_alpha, hgh_alpha, mtdbld_rate);
	}

	/*=========================================================*/
	if (dnlp_printk & 0x1) {
		pr_info("dnlp blend curve:\n");
		pr_info("[luma_sum] = %d\n", ve_dnlp_luma_sum);
	}

	dnlp_gmma_cuvs(gmma_rate, low_alpha, hgh_alpha, lsft_avg);
	CrtTstDat[24] = gmma_rate;
	CrtTstDat[25] = low_alpha;
	CrtTstDat[26] = hgh_alpha;
	CrtTstDat[27] = lsft_avg;

	for (i = 0; i < 64; i++) {
		pcurves[0][i] = gma_scurve0[i]; /* gamma0 s-curve */
		pcurves[1][i] = gma_scurve1[i]; /* gamma1 s-curve */
		pcurves[2][i] = gma_scurvet[i]; /* gmma0+gamm1 s-curve */
		pcurves[3][i] = clash_curve[i]; /* clash curve */
		pcurves[4][i] = clsh_scvbld[i]; /* clash + s-curve blend */
		pcurves[5][i] = blk_gma_crv[i]; /* black gamma curve */
		pcurves[6][i] = blk_gma_bld[i]; /* blending with black gamma */
		pcurves[7][i] = blkwht_ebld[i]; /* black white extension */
	}

	/* clsh_scvbld = clash_curve + gma_scurvet */
	dnlp_clsh_sbld(mtdbld_rate); /* clash + s-curve */

	/* blk_gma_bld = blk_gma_crv + clsh_scvbld */
	dnlp_blkgma_bld(blk_gma_rat);

	/* blkwht_ebld = blk_gma_bld + extension */
	blk_wht_ext[0] +=  (ve_dnlp_set_bext << 4);
	blk_wht_ext[1] +=  (ve_dnlp_set_wext << 4);
	dnlp_blkwht_bld(blk_wht_ext, dnlp_brightness,
		luma_avg4, luma_avg, iRgnBgn, iRgnEnd);

	if (prt_flg) {
		pr_info("blk/wht ext: [%d %d] + %d\n",
			blk_wht_ext[0], blk_wht_ext[1],
			dnlp_brightness);
	}

	prt_flg = ((dnlp_printk >> 2) & 0x1);
	for (i = 0; i < 64; i++) {
		premap0[i] = ve_dnlp_tgt[i];

		if (ve_dnlp_dbg_map == 1)
			nTmp0 = gma_scurve0[i];
		else if (ve_dnlp_dbg_map == 2)
			nTmp0 = gma_scurve1[i];
		else if (ve_dnlp_dbg_map == 3)
			nTmp0 = gma_scurvet[i];
		else if (ve_dnlp_dbg_map == 4)
			nTmp0 = clash_curve[i];
		else if (ve_dnlp_dbg_map == 5)
			nTmp0 = clsh_scvbld[i];
		else if (ve_dnlp_dbg_map == 6)
			nTmp0 = blk_gma_crv[i];
		else if (ve_dnlp_dbg_map == 7)
			nTmp0 = blk_gma_bld[i];
		else if (ve_dnlp_dbg_map == 8)
			nTmp0 = blkwht_ebld[i]; /* 1023 */
		else
			nTmp0 = blkwht_ebld[i];

		nTmp0 = dnlp_bld_lvl * nTmp0 + (RBASE >> 1); /* 1024 */
		nTmp0 = nTmp0 + (RBASE - dnlp_bld_lvl) * pst_0_gamma[i];
		nTmp0 = (nTmp0 >> ve_dnlp_mvreflsh); /* 0~4096 */

		if (nTmp0 < 0)
			nTmp0 = 0;
		else if (nTmp0 > 1023)
			nTmp0 = 1023;

		pst_0_gamma[i] = nTmp0;

		nTmp0 = ((nTmp0 + 2) >> 2);

		if (nTmp0 > 255)
			nTmp0 = 255;
		else if (nTmp0 < 0)
			nTmp0 = 0;

		if (prt_flg)
			pr_info("[%02d]: (%4d %4d)%4d c%4d cs%4d gm%4d-%4d %4d => %3d\n",
				i, gma_scurve0[i], gma_scurve1[i],
				gma_scurvet[i], clash_curve[i],
				clsh_scvbld[i], blk_gma_crv[i],
				blk_gma_bld[i], blkwht_ebld[i],
				nTmp0);

		ve_dnlp_tgt[i] = nTmp0;
	}

	/* 0~255 sort */
	dnlp_tgt_sort();

	prt_flg = ((dnlp_printk >> 3) & 0x1);
	if (prt_flg) {
		for (i = 0; i < 64; i++) {
			nT0 = ve_dnlp_tgt[i] - 4*i;
			pr_info("%02d: %03d=>%03d (%3d)\n",
				i, 4*i, ve_dnlp_tgt[i], nT0);
		}
		pr_info("\n");
	}

	nT0 = 0;
	nT1 = 0;
	for (i = 1; i < 64; i++) {
		if (ve_dnlp_tgt[i] > 4*i) {
			nT0 += (ve_dnlp_tgt[i] - 4*i) * (65 - i);
			nT1 += (65 - i);
		}
	}
	nTmp0 = nT0 * ve_dnlp_satur_rat + (nT1 >> 1);
	nTmp0 = nTmp0 / (nT1 + 1);
	nTmp0 = ((nTmp0 + 4) >> 3);

	nTmp =  (ve_dnlp_satur_max << 3);
	if (nTmp0 < nTmp)
		nTmp = nTmp0;

	if (dnlp_printk)
		pr_info("#Statu: pre(%3d) => %5d / %3d => %3d cur(%3d)\n",
			pre_stur, nT0, nT1, nTmp0, nTmp);

	if (ve_dnlp_set_saturtn == 0) {
		if (nTmp != pre_stur) {
			ve_dnlp_add_cm(nTmp + 512);
			pre_stur = nTmp;
		}
	} else {
		if (pre_stur != ve_dnlp_set_saturtn) {
			if (ve_dnlp_set_saturtn < 512)
				ve_dnlp_add_cm(ve_dnlp_set_saturtn + 512);
			else
				ve_dnlp_add_cm(ve_dnlp_set_saturtn);
			pre_stur = ve_dnlp_set_saturtn;
		}
	}

	if (dnlp_printk)
		pr_info("#Dbg: [%02d < %02d(%03d) < %02d] %03d\n",
			iRgnBgn, luma_avg, luma_avg4,
			iRgnEnd, dnlp_brightness);

	nT0 = 0;
	prt_flg = ((dnlp_printk >> 3) & 0x1);
	if (prt_flg) {
		for (i = 0; i < 64; i++)
			nT0 += ((premap0[i] > ve_dnlp_tgt[i]) ?
					(premap0[i] - ve_dnlp_tgt[i]) :
					(ve_dnlp_tgt[i] - premap0[i]));

		pr_info("map dif= %d\n", nT0);

		if (nT0 > ve_dnlp_dbg_diflvl) {
			pr_info("#mrate=%02d brt=%02d\n",
				mtdbld_rate, dnlp_brightness);
			for (i = 0; i < 64; i++)
				if (premap0[i] != ve_dnlp_tgt[i])
					pr_info("[%02d]%5d=>%5d:(%4d=>%4d)\n",
						i,
						pre_1_gamma[i], pre_0_gamma[i],
						premap0[i], ve_dnlp_tgt[i]);

			for (i = 0; i < 28; i++)
				if (PreTstDat[i] != CrtTstDat[i])
					pr_info("[%02d] %5d=>%5d\n",
						i, PreTstDat[i], CrtTstDat[i]);
			pr_info("\n");

			if (ve_dnlp_ponce >= 2)
				ve_dnlp_ponce--;
			else
				ve_dnlp_ponce = 1;
		}
	}

	prt_flg = ((dnlp_printk >> 8) & 0x1);
	if (ve_dnlp_dbg_map && prt_flg)
		for (i = 0; i < 64; i++)
			pr_info("[%02d] %5d=>%5d\n",
				i, pre_0_gamma[i], ve_dnlp_tgt[i]);

	/* print debug log once */
	if (ve_dnlp_ponce == 1 && dnlp_printk)
		dnlp_printk = 0;

}

static void ve_dnlp_calculate_tgt(struct vframe_s *vf)
{
	struct vframe_prop_s *p = &vf->prop;
	ulong data[5];
	static unsigned int sum_b = 0, sum_c;
	ulong i = 0, j = 0, ave = 0, max = 0, div = 0;
	unsigned int cnt = assist_cnt;/* READ_VPP_REG(ASSIST_SPARE8_REG1); */
	/* old historic luma sum */
	sum_b = sum_c;
	sum_c = ve_dnlp_luma_sum;
	/* new historic luma sum */
	ve_dnlp_luma_sum = p->hist.luma_sum;
	pr_amve_dbg("ve_dnlp_luma_sum=%x,sum_b=%x,sum_c=%x\n",
			ve_dnlp_luma_sum, sum_b, sum_c);
	/* picture mode: freeze dnlp curve */
	if (dnlp_respond) {
		/* new luma sum is 0, something is wrong, freeze dnlp curve */
		if (!ve_dnlp_luma_sum)
			return;
	} else {
		/* new luma sum is 0, something is wrong, freeze dnlp curve */
		if ((!ve_dnlp_luma_sum) ||
			/* new luma sum is closed to old one (1 +/- 1/64), */
			/* picture mode, freeze curve *//* 5 *//* 5 */
			((ve_dnlp_luma_sum <
					sum_b + (sum_b >> dnlp_adj_level)) &&
			(ve_dnlp_luma_sum >
					sum_b - (sum_b >> dnlp_adj_level))))
			return;
	}
	/* get 5 regions */
	for (i = 0; i < 5; i++) {
		j = 4 + 11 * i;
		data[i] = (ulong)p->hist.gamma[j] +
		(ulong)p->hist.gamma[j +  1] +
		(ulong)p->hist.gamma[j +  2] +
		(ulong)p->hist.gamma[j +  3] +
		(ulong)p->hist.gamma[j +  4] +
		(ulong)p->hist.gamma[j +  5] +
		(ulong)p->hist.gamma[j +  6] +
		(ulong)p->hist.gamma[j +  7] +
		(ulong)p->hist.gamma[j +  8] +
		(ulong)p->hist.gamma[j +  9] +
		(ulong)p->hist.gamma[j + 10];
	}
	/* get max, ave, div */
	for (i = 0; i < 5; i++) {
		if (max < data[i])
			max = data[i];
		ave += data[i];
		data[i] *= 5;
	}
	max *= 5;
	div = (max - ave > ave) ? max - ave : ave;
	/* invalid histgram: freeze dnlp curve */
	if (!max)
		return;
	/* get 1st 4 points */
	for (i = 0; i < 4; i++) {
		if (data[i] > ave)
			data[i] = 64 + (((data[i] - ave) << 1) + div) *
			ve_dnlp_rl / (div << 1);
		else if (data[i] < ave)
			data[i] = 64 - (((ave - data[i]) << 1) + div) *
			ve_dnlp_rl / (div << 1);
		else
			data[i] = 64;
		ve_dnlp_tgt[4 + 11 * (i + 1)] = ve_dnlp_tgt[4 + 11 * i] +
			((44 * data[i] + 32) >> 6);
	}
	/* fill in region 0 with black extension */
	data[0] = ve_dnlp_black;
	if (data[0] > 16)
		data[0] = 16;
	data[0] =
		(ve_dnlp_tgt[15] - ve_dnlp_tgt[4]) * (16 - data[0]);
	for (j = 1; j <= 6; j++)
		ve_dnlp_tgt[4 + j] =
			ve_dnlp_tgt[4] + (data[0] * j + 88) / 176;
	data[0] = (ve_dnlp_tgt[15] - ve_dnlp_tgt[10]) << 1;
	for (j = 1; j <= 4; j++)
		ve_dnlp_tgt[10 + j] =
			ve_dnlp_tgt[10] + (data[0] * j + 5) / 10;
	/* fill in regions 1~3 */
	for (i = 1; i <= 3; i++) {
		data[i] =
		(ve_dnlp_tgt[11 * i + 15] - ve_dnlp_tgt[11 * i + 4]) << 1;
		for (j = 1; j <= 10; j++)
			ve_dnlp_tgt[11 * i + 4 + j] = ve_dnlp_tgt[11 * i + 4] +
			(data[i] * j + 11) / 22;
	}
	/* fill in region 4 with white extension */
	data[4] /= 20;
	data[4] = (ve_dnlp_white *
			((ave << 4) - data[4] * ve_dnlp_white_factor)
			+ (ave << 3)) / (ave << 4);
	if (data[4] > 16)
		data[4] = 16;
	data[4] = (ve_dnlp_tgt[59] - ve_dnlp_tgt[48]) * (16 - data[4]);
	for (j = 1; j <= 6; j++)
		ve_dnlp_tgt[59 - j] = ve_dnlp_tgt[59] -
		(data[4] * j + 88) / 176;
	data[4] = (ve_dnlp_tgt[53] - ve_dnlp_tgt[48]) << 1;
	for (j = 1; j <= 4; j++)
		ve_dnlp_tgt[53 - j] = ve_dnlp_tgt[53] - (data[4] * j + 5) / 10;
	if (cnt) {
		for (i = 0; i < 64; i++)
			pr_amve_dbg(" ve_dnlp_tgt[%ld]=%d\n",
					i, ve_dnlp_tgt[i]);
		/* WRITE_VPP_REG(ASSIST_SPARE8_REG1, 0); */
		assist_cnt = 0;
	}
}
 /* lpf[0] is always 0 & no need calculation */
static void ve_dnlp_calculate_lpf(void)
{
	ulong i = 0;

	for (i = 0; i < 64; i++)
		ve_dnlp_lpf[i] = ve_dnlp_lpf[i] -
		(ve_dnlp_lpf[i] >> ve_dnlp_rt) + ve_dnlp_tgt[i];
}

static void ve_dnlp_calculate_reg(void)
{
	ulong i = 0, j = 0, cur = 0, data = 0,
			offset = ve_dnlp_rt ? (1 << (ve_dnlp_rt - 1)) : 0;
	for (i = 0; i < 16; i++) {
		ve_dnlp_reg[i] = 0;
		cur = i << 2;
		for (j = 0; j < 4; j++) {
			data = (ve_dnlp_lpf[cur + j] + offset) >> ve_dnlp_rt;
			if (data > 255)
				data = 255;
			ve_dnlp_reg[i] |= data << (j << 3);
		}
	}
}

static void ve_dnlp_load_reg(void)
{
	if  (dnlp_sel == NEW_DNLP_IN_SHARPNESS) {
		if (is_meson_gxlx_cpu()) {
			WRITE_VPP_REG(SRSHARP1_DNLP_00, ve_dnlp_reg[0]);
			WRITE_VPP_REG(SRSHARP1_DNLP_01, ve_dnlp_reg[1]);
			WRITE_VPP_REG(SRSHARP1_DNLP_02, ve_dnlp_reg[2]);
			WRITE_VPP_REG(SRSHARP1_DNLP_03, ve_dnlp_reg[3]);
			WRITE_VPP_REG(SRSHARP1_DNLP_04, ve_dnlp_reg[4]);
			WRITE_VPP_REG(SRSHARP1_DNLP_05, ve_dnlp_reg[5]);
			WRITE_VPP_REG(SRSHARP1_DNLP_06, ve_dnlp_reg[6]);
			WRITE_VPP_REG(SRSHARP1_DNLP_07, ve_dnlp_reg[7]);
			WRITE_VPP_REG(SRSHARP1_DNLP_08, ve_dnlp_reg[8]);
			WRITE_VPP_REG(SRSHARP1_DNLP_09, ve_dnlp_reg[9]);
			WRITE_VPP_REG(SRSHARP1_DNLP_10, ve_dnlp_reg[10]);
			WRITE_VPP_REG(SRSHARP1_DNLP_11, ve_dnlp_reg[11]);
			WRITE_VPP_REG(SRSHARP1_DNLP_12, ve_dnlp_reg[12]);
			WRITE_VPP_REG(SRSHARP1_DNLP_13, ve_dnlp_reg[13]);
			WRITE_VPP_REG(SRSHARP1_DNLP_14, ve_dnlp_reg[14]);
			WRITE_VPP_REG(SRSHARP1_DNLP_15, ve_dnlp_reg[15]);
		} else {
			WRITE_VPP_REG(SRSHARP0_DNLP_00, ve_dnlp_reg[0]);
			WRITE_VPP_REG(SRSHARP0_DNLP_01, ve_dnlp_reg[1]);
			WRITE_VPP_REG(SRSHARP0_DNLP_02, ve_dnlp_reg[2]);
			WRITE_VPP_REG(SRSHARP0_DNLP_03, ve_dnlp_reg[3]);
			WRITE_VPP_REG(SRSHARP0_DNLP_04, ve_dnlp_reg[4]);
			WRITE_VPP_REG(SRSHARP0_DNLP_05, ve_dnlp_reg[5]);
			WRITE_VPP_REG(SRSHARP0_DNLP_06, ve_dnlp_reg[6]);
			WRITE_VPP_REG(SRSHARP0_DNLP_07, ve_dnlp_reg[7]);
			WRITE_VPP_REG(SRSHARP0_DNLP_08, ve_dnlp_reg[8]);
			WRITE_VPP_REG(SRSHARP0_DNLP_09, ve_dnlp_reg[9]);
			WRITE_VPP_REG(SRSHARP0_DNLP_10, ve_dnlp_reg[10]);
			WRITE_VPP_REG(SRSHARP0_DNLP_11, ve_dnlp_reg[11]);
			WRITE_VPP_REG(SRSHARP0_DNLP_12, ve_dnlp_reg[12]);
			WRITE_VPP_REG(SRSHARP0_DNLP_13, ve_dnlp_reg[13]);
			WRITE_VPP_REG(SRSHARP0_DNLP_14, ve_dnlp_reg[14]);
			WRITE_VPP_REG(SRSHARP0_DNLP_15, ve_dnlp_reg[15]);
		}
	} else {
		/* #endif */
		WRITE_VPP_REG(VPP_DNLP_CTRL_00, ve_dnlp_reg[0]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_01, ve_dnlp_reg[1]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_02, ve_dnlp_reg[2]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_03, ve_dnlp_reg[3]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_04, ve_dnlp_reg[4]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_05, ve_dnlp_reg[5]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_06, ve_dnlp_reg[6]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_07, ve_dnlp_reg[7]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_08, ve_dnlp_reg[8]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_09, ve_dnlp_reg[9]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_10, ve_dnlp_reg[10]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_11, ve_dnlp_reg[11]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_12, ve_dnlp_reg[12]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_13, ve_dnlp_reg[13]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_14, ve_dnlp_reg[14]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_15, ve_dnlp_reg[15]);
	}
}

static void ve_dnlp_load_def_reg(void)
{
	if  (dnlp_sel == NEW_DNLP_IN_SHARPNESS) {
		if (is_meson_gxlx_cpu()) {
			WRITE_VPP_REG(SRSHARP1_DNLP_00, ve_dnlp_reg[0]);
			WRITE_VPP_REG(SRSHARP1_DNLP_01, ve_dnlp_reg[1]);
			WRITE_VPP_REG(SRSHARP1_DNLP_02, ve_dnlp_reg[2]);
			WRITE_VPP_REG(SRSHARP1_DNLP_03, ve_dnlp_reg[3]);
			WRITE_VPP_REG(SRSHARP1_DNLP_04, ve_dnlp_reg[4]);
			WRITE_VPP_REG(SRSHARP1_DNLP_05, ve_dnlp_reg[5]);
			WRITE_VPP_REG(SRSHARP1_DNLP_06, ve_dnlp_reg[6]);
			WRITE_VPP_REG(SRSHARP1_DNLP_07, ve_dnlp_reg[7]);
			WRITE_VPP_REG(SRSHARP1_DNLP_08, ve_dnlp_reg[8]);
			WRITE_VPP_REG(SRSHARP1_DNLP_09, ve_dnlp_reg[9]);
			WRITE_VPP_REG(SRSHARP1_DNLP_10, ve_dnlp_reg[10]);
			WRITE_VPP_REG(SRSHARP1_DNLP_11, ve_dnlp_reg[11]);
			WRITE_VPP_REG(SRSHARP1_DNLP_12, ve_dnlp_reg[12]);
			WRITE_VPP_REG(SRSHARP1_DNLP_13, ve_dnlp_reg[13]);
			WRITE_VPP_REG(SRSHARP1_DNLP_14, ve_dnlp_reg[14]);
			WRITE_VPP_REG(SRSHARP1_DNLP_15, ve_dnlp_reg[15]);
		} else {
			WRITE_VPP_REG(SRSHARP0_DNLP_00, ve_dnlp_reg[0]);
			WRITE_VPP_REG(SRSHARP0_DNLP_01, ve_dnlp_reg[1]);
			WRITE_VPP_REG(SRSHARP0_DNLP_02, ve_dnlp_reg[2]);
			WRITE_VPP_REG(SRSHARP0_DNLP_03, ve_dnlp_reg[3]);
			WRITE_VPP_REG(SRSHARP0_DNLP_04, ve_dnlp_reg[4]);
			WRITE_VPP_REG(SRSHARP0_DNLP_05, ve_dnlp_reg[5]);
			WRITE_VPP_REG(SRSHARP0_DNLP_06, ve_dnlp_reg[6]);
			WRITE_VPP_REG(SRSHARP0_DNLP_07, ve_dnlp_reg[7]);
			WRITE_VPP_REG(SRSHARP0_DNLP_08, ve_dnlp_reg[8]);
			WRITE_VPP_REG(SRSHARP0_DNLP_09, ve_dnlp_reg[9]);
			WRITE_VPP_REG(SRSHARP0_DNLP_10, ve_dnlp_reg[10]);
			WRITE_VPP_REG(SRSHARP0_DNLP_11, ve_dnlp_reg[11]);
			WRITE_VPP_REG(SRSHARP0_DNLP_12, ve_dnlp_reg[12]);
			WRITE_VPP_REG(SRSHARP0_DNLP_13, ve_dnlp_reg[13]);
			WRITE_VPP_REG(SRSHARP0_DNLP_14, ve_dnlp_reg[14]);
			WRITE_VPP_REG(SRSHARP0_DNLP_15, ve_dnlp_reg[15]);
		}
	} else {
		WRITE_VPP_REG(VPP_DNLP_CTRL_00, ve_dnlp_reg_def[0]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_01, ve_dnlp_reg_def[1]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_02, ve_dnlp_reg_def[2]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_03, ve_dnlp_reg_def[3]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_04, ve_dnlp_reg_def[4]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_05, ve_dnlp_reg_def[5]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_06, ve_dnlp_reg_def[6]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_07, ve_dnlp_reg_def[7]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_08, ve_dnlp_reg_def[8]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_09, ve_dnlp_reg_def[9]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_10, ve_dnlp_reg_def[10]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_11, ve_dnlp_reg_def[11]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_12, ve_dnlp_reg_def[12]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_13, ve_dnlp_reg_def[13]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_14, ve_dnlp_reg_def[14]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_15, ve_dnlp_reg_def[15]);
	}
}

void ve_on_vs(struct vframe_s *vf)
{

	if (dnlp_en_2 || ve_en) {
		/* calculate dnlp target data */
		if (ve_dnlp_method == 0)
			ve_dnlp_calculate_tgt(vf);
		else if (ve_dnlp_method == 1)
			ve_dnlp_calculate_tgtx(vf);
		else if (ve_dnlp_method == 2)
			ve_dnlp_calculate_tgt_ext(vf);
		else if (ve_dnlp_method == 3)
			ve_dnlp_calculate_tgtx_new(vf);
		else
			ve_dnlp_calculate_tgt(vf);
		/* calculate dnlp low-pass-filter data */
		ve_dnlp_calculate_lpf();
		/* calculate dnlp reg data */
		ve_dnlp_calculate_reg();
		/* load dnlp reg data */
		ve_dnlp_load_reg();
	}
	ve_hist_gamma_tgt(vf);

	/* sharpness process */
	sharpness_process(vf);

	/* */
#if 0
	/* comment for duration algorithm is not based on panel vsync */
	if (vf->prop.meas.vs_cycle && !frame_lock_nosm) {
		if ((vecm_latch_flag & FLAG_LVDS_FREQ_SW1) &&
		(vf->duration >= 1920 - 19) && (vf->duration <= 1920 + 19))
			vpp_phase_lock_on_vs(vf->prop.meas.vs_cycle,
				vf->prop.meas.vs_stamp,
				true,
				lock_range_50hz_fast,
				lock_range_50hz_slow);
		if ((!(vecm_latch_flag & FLAG_LVDS_FREQ_SW1)) &&
		(vf->duration >= 1600 - 5) && (vf->duration <= 1600 + 13))
			vpp_phase_lock_on_vs(vf->prop.meas.vs_cycle,
				vf->prop.meas.vs_stamp,
				false,
				lock_range_60hz_fast,
				lock_range_60hz_slow);
	}
#endif
}

/* *********************************************************************** */
/* *** IOCTL-oriented functions ****************************************** */
/* *********************************************************************** */

void vpp_enable_lcd_gamma_table(void)
{
	WRITE_VPP_REG_BITS(L_GAMMA_CNTL_PORT, 1, GAMMA_EN, 1);
}

void vpp_disable_lcd_gamma_table(void)
{
	WRITE_VPP_REG_BITS(L_GAMMA_CNTL_PORT, 0, GAMMA_EN, 1);
}

void vpp_set_lcd_gamma_table(u16 *data, u32 rgb_mask)
{
	int i;
	int cnt = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&vpp_lcd_gamma_lock, flags);

	WRITE_VPP_REG_BITS(L_GAMMA_CNTL_PORT,
				0, GAMMA_EN, 1);

	while (!(READ_VPP_REG(L_GAMMA_CNTL_PORT) & (0x1 << ADR_RDY))) {
		udelay(10);
		if (cnt++ > GAMMA_RETRY)
			break;
	}
	cnt = 0;
	WRITE_VPP_REG(L_GAMMA_ADDR_PORT, (0x1 << H_AUTO_INC) |
				    (0x1 << rgb_mask)   |
				    (0x0 << HADR));
	for (i = 0; i < 256; i++) {
		while (!(READ_VPP_REG(L_GAMMA_CNTL_PORT) & (0x1 << WR_RDY))) {
			udelay(10);
			if (cnt++ > GAMMA_RETRY)
				break;
		}
		cnt = 0;
		WRITE_VPP_REG(L_GAMMA_DATA_PORT, data[i]);
	}
	while (!(READ_VPP_REG(L_GAMMA_CNTL_PORT) & (0x1 << ADR_RDY))) {
		udelay(10);
		if (cnt++ > GAMMA_RETRY)
			break;
	}
	WRITE_VPP_REG(L_GAMMA_ADDR_PORT, (0x1 << H_AUTO_INC) |
				    (0x1 << rgb_mask)   |
				    (0x23 << HADR));

	WRITE_VPP_REG_BITS(L_GAMMA_CNTL_PORT,
					gamma_en, GAMMA_EN, 1);

	spin_unlock_irqrestore(&vpp_lcd_gamma_lock, flags);
}

void amve_write_gamma_table(u16 *data, u32 rgb_mask)
{
	int i;
	int cnt = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&vpp_lcd_gamma_lock, flags);

	while (!(READ_VPP_REG(L_GAMMA_CNTL_PORT) & (0x1 << ADR_RDY))) {
		udelay(10);
		if (cnt++ > GAMMA_RETRY)
			break;
	}
	cnt = 0;
	WRITE_VPP_REG(L_GAMMA_ADDR_PORT, (0x1 << H_AUTO_INC) |
				    (0x1 << rgb_mask)   |
				    (0x0 << HADR));
	for (i = 0; i < 256; i++) {
		while (!(READ_VPP_REG(L_GAMMA_CNTL_PORT) & (0x1 << WR_RDY))) {
			udelay(10);
			if (cnt++ > GAMMA_RETRY)
				break;
		}
		cnt = 0;
		WRITE_VPP_REG(L_GAMMA_DATA_PORT, data[i]);
	}
	while (!(READ_VPP_REG(L_GAMMA_CNTL_PORT) & (0x1 << ADR_RDY))) {
		udelay(10);
		if (cnt++ > GAMMA_RETRY)
			break;
	}
	WRITE_VPP_REG(L_GAMMA_ADDR_PORT, (0x1 << H_AUTO_INC) |
				    (0x1 << rgb_mask)   |
				    (0x23 << HADR));

	spin_unlock_irqrestore(&vpp_lcd_gamma_lock, flags);
}

#define COEFF_NORM(a) ((int)((((a) * 2048.0) + 1) / 2))
#define MATRIX_5x3_COEF_SIZE 24

static int RGB709_to_YUV709l_coeff[MATRIX_5x3_COEF_SIZE] = {
	0, 0, 0, /* pre offset */
	COEFF_NORM(0.181873),	COEFF_NORM(0.611831),	COEFF_NORM(0.061765),
	COEFF_NORM(-0.100251),	COEFF_NORM(-0.337249),	COEFF_NORM(0.437500),
	COEFF_NORM(0.437500),	COEFF_NORM(-0.397384),	COEFF_NORM(-0.040116),
	0, 0, 0, /* 10'/11'/12' */
	0, 0, 0, /* 20'/21'/22' */
	64, 512, 512, /* offset */
	0, 0, 0 /* mode, right_shift, clip_en */
};

static int bypass_coeff[MATRIX_5x3_COEF_SIZE] = {
	0, 0, 0, /* pre offset */
	COEFF_NORM(1.0),	COEFF_NORM(0.0),	COEFF_NORM(0.0),
	COEFF_NORM(0.0),	COEFF_NORM(1.0),	COEFF_NORM(0.0),
	COEFF_NORM(0.0),	COEFF_NORM(0.0),	COEFF_NORM(1.0),
	0, 0, 0, /* 10'/11'/12' */
	0, 0, 0, /* 20'/21'/22' */
	0, 0, 0, /* offset */
	0, 0, 0 /* mode, right_shift, clip_en */
};

void vpp_set_rgb_ogo(struct tcon_rgb_ogo_s *p)
{
	int m[24];
	int i;
	/* write to registers */
	if (video_rgb_ogo_xvy_mtx) {
		if (video_rgb_ogo_xvy_mtx_latch & MTX_BYPASS_RGB_OGO) {
			memcpy(m, bypass_coeff, sizeof(int) * 24);
			video_rgb_ogo_xvy_mtx_latch &= ~MTX_BYPASS_RGB_OGO;
		} else if (video_rgb_ogo_xvy_mtx_latch & MTX_RGB2YUVL_RGB_OGO) {
			memcpy(m, RGB709_to_YUV709l_coeff, sizeof(int) * 24);
			video_rgb_ogo_xvy_mtx_latch &= ~MTX_RGB2YUVL_RGB_OGO;
		} else
			memcpy(m, bypass_coeff, sizeof(int) * 24);

		m[3] = p->r_gain * m[3] / COEFF_NORM(1.0);
		m[4] = p->r_gain * m[4] / COEFF_NORM(1.0);
		m[5] = p->r_gain * m[5] / COEFF_NORM(1.0);
		m[6] = p->g_gain * m[6] / COEFF_NORM(1.0);
		m[7] = p->g_gain * m[7] / COEFF_NORM(1.0);
		m[8] = p->g_gain * m[8] / COEFF_NORM(1.0);
		m[9] = p->b_gain * m[9] / COEFF_NORM(1.0);
		m[10] = p->b_gain * m[10] / COEFF_NORM(1.0);
		m[11] = p->b_gain * m[11] / COEFF_NORM(1.0);

		m[18] = (p->r_pre_offset + m[18] + 1024)
			* p->r_gain / COEFF_NORM(1.0)
			- p->r_gain + p->r_post_offset;
		m[19] = (p->g_pre_offset + m[19] + 1024)
			* p->g_gain / COEFF_NORM(1.0)
			- p->g_gain + p->g_post_offset;
		m[20] = (p->b_pre_offset + m[20] + 1024)
			* p->b_gain / COEFF_NORM(1.0)
			- p->b_gain + p->b_post_offset;

		for (i = 18; i < 21; i++) {
			if (m[i] > 1023)
				m[i] = 1023;
			if (m[i] < -1024)
				m[i] = -1024;
		}

		WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, p->en, 6, 1);
		WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, 3, 8, 2);

		WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET0_1,
			((m[0] & 0xfff) << 16)
			| (m[1] & 0xfff));
		WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET2,
			m[2] & 0xfff);
		WRITE_VPP_REG(VPP_MATRIX_COEF00_01,
			((m[3] & 0x1fff) << 16)
			| (m[4] & 0x1fff));
		WRITE_VPP_REG(VPP_MATRIX_COEF02_10,
			((m[5]	& 0x1fff) << 16)
			| (m[6] & 0x1fff));
		WRITE_VPP_REG(VPP_MATRIX_COEF11_12,
			((m[7] & 0x1fff) << 16)
			| (m[8] & 0x1fff));
		WRITE_VPP_REG(VPP_MATRIX_COEF20_21,
			((m[9] & 0x1fff) << 16)
			| (m[10] & 0x1fff));
		WRITE_VPP_REG(VPP_MATRIX_COEF22,
			m[11] & 0x1fff);
		if (m[21]) {
			WRITE_VPP_REG(VPP_MATRIX_COEF13_14,
				((m[12] & 0x1fff) << 16)
				| (m[13] & 0x1fff));
			WRITE_VPP_REG(VPP_MATRIX_COEF15_25,
				((m[14] & 0x1fff) << 16)
				| (m[17] & 0x1fff));
			WRITE_VPP_REG(VPP_MATRIX_COEF23_24,
				((m[15] & 0x1fff) << 16)
				| (m[16] & 0x1fff));
		}
		WRITE_VPP_REG(VPP_MATRIX_OFFSET0_1,
			((m[18] & 0xfff) << 16)
			| (m[19] & 0xfff));
		WRITE_VPP_REG(VPP_MATRIX_OFFSET2,
			m[20] & 0xfff);
		WRITE_VPP_REG_BITS(VPP_MATRIX_CLIP,
			m[21], 3, 2);
		WRITE_VPP_REG_BITS(VPP_MATRIX_CLIP,
			m[22], 5, 3);
	} else {
		WRITE_VPP_REG(VPP_GAINOFF_CTRL0,
				((p->en << 31) & 0x80000000) |
				((p->r_gain << 16) & 0x07ff0000) |
				((p->g_gain <<  0) & 0x000007ff));
		WRITE_VPP_REG(VPP_GAINOFF_CTRL1,
				((p->b_gain << 16) & 0x07ff0000) |
				((p->r_post_offset <<  0) & 0x000007ff));
		WRITE_VPP_REG(VPP_GAINOFF_CTRL2,
				((p->g_post_offset << 16) & 0x07ff0000) |
				((p->b_post_offset <<  0) & 0x000007ff));
		WRITE_VPP_REG(VPP_GAINOFF_CTRL3,
				((p->r_pre_offset  << 16) & 0x07ff0000) |
				((p->g_pre_offset  <<  0) & 0x000007ff));
		WRITE_VPP_REG(VPP_GAINOFF_CTRL4,
				((p->b_pre_offset  <<  0) & 0x000007ff));
	}
}

void ve_enable_dnlp(void)
{
	ve_en = 1;
/* #ifdef NEW_DNLP_IN_SHARPNESS */
/* if(dnlp_sel == NEW_DNLP_IN_SHARPNESS){ */
	if (is_meson_gxtvbb_cpu() &&
		(dnlp_sel == NEW_DNLP_IN_SHARPNESS))
		WRITE_VPP_REG_BITS(SRSHARP0_DNLP_EN, 1, 0, 1);
	else
		/* #endif */
		WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL,
				1, DNLP_EN_BIT, DNLP_EN_WID);
}

void ve_disable_dnlp(void)
{
	ve_en = 0;
/* #ifdef NEW_DNLP_IN_SHARPNESS */
/* if(dnlp_sel == NEW_DNLP_IN_SHARPNESS){ */
	if (is_meson_gxtvbb_cpu() &&
		(dnlp_sel == NEW_DNLP_IN_SHARPNESS))
		WRITE_VPP_REG_BITS(SRSHARP0_DNLP_EN, 0, 0, 1);
	else
/* #endif */
/* { */
		WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL,
				0, DNLP_EN_BIT, DNLP_EN_WID);
/* } */
}

void ve_set_dnlp(struct ve_dnlp_s *p)
{
	ulong i = 0;
	/* get command parameters */
	ve_en                = p->en;
	ve_dnlp_white_factor = (p->rt >> 4) & 0xf;
	ve_dnlp_rt           = p->rt & 0xf;
	ve_dnlp_rl           = p->rl;
	ve_dnlp_black        = p->black;
	ve_dnlp_white        = p->white;
	if (ve_en) {
		/* clear historic luma sum */
		ve_dnlp_luma_sum = 0;
		/* init tgt & lpf */
		for (i = 0; i < 64; i++) {
			ve_dnlp_tgt[i] = i << 2;
			ve_dnlp_lpf[i] = ve_dnlp_tgt[i] << ve_dnlp_rt;
		}
		/* calculate dnlp reg data */
		ve_dnlp_calculate_reg();
		/* load dnlp reg data */
		ve_dnlp_load_reg();
		/* enable dnlp */
		ve_enable_dnlp();
	} else {
		/* disable dnlp */
		ve_disable_dnlp();
	}
}

void ve_set_dnlp_2(void)
{
	ulong i = 0;
	/* get command parameters */
	if (is_meson_gxbb_cpu()) {
		ve_dnlp_method		 = 1;
		ve_dnlp_cliprate	 = 6;
		ve_dnlp_hghrange	 = 14;
		ve_dnlp_lowrange	 = 18;
		ve_dnlp_hghalpha	 = 26;
		ve_dnlp_midalpha	 = 28;
		ve_dnlp_lowalpha	 = 18;
	}
	/* clear historic luma sum */
	ve_dnlp_luma_sum = 0;
	/* init tgt & lpf */
	for (i = 0; i < 64; i++) {
		ve_dnlp_tgt[i] = i << 2;
		ve_dnlp_lpf[i] = ve_dnlp_tgt[i] << ve_dnlp_rt;
	}
	/* calculate dnlp reg data */
	ve_dnlp_calculate_reg();
	/* load dnlp reg data */
	/*ve_dnlp_load_reg();*/
	ve_dnlp_load_def_reg();
}

void ve_set_new_dnlp(struct ve_dnlp_table_s *p)
{
	ulong i = 0;
	/* get command parameters */
	ve_en                = p->en;
	ve_dnlp_method       = p->method;
	ve_dnlp_cliprate     = p->cliprate;
	ve_dnlp_hghrange     = p->hghrange;
	ve_dnlp_lowrange     = p->lowrange;
	ve_dnlp_hghalpha     = p->hghalpha;
	ve_dnlp_midalpha     = p->midalpha;
	ve_dnlp_lowalpha     = p->lowalpha;

	ve_dnlp_mvreflsh  = p->new_mvreflsh;
	ve_dnlp_gmma_rate = p->new_gmma_rate;
	ve_dnlp_lowalpha_new  = p->new_lowalpha;
	ve_dnlp_hghalpha_new  = p->new_hghalpha;
	ve_dnlp_sbgnbnd   = p->new_sbgnbnd;
	ve_dnlp_sendbnd   = p->new_sendbnd;
	ve_dnlp_cliprate_new  = p->new_cliprate;
	ve_dnlp_clashBgn  = p->new_clashBgn;
	ve_dnlp_clashEnd  = p->new_clashEnd;
	ve_mtdbld_rate    = p->new_mtdbld_rate;
	ve_dnlp_pst_gmarat    = p->new_dnlp_pst_gmarat;

	dnlp_sel = p->dnlp_sel;
	ve_dnlp_blk_cctr = p->dnlp_blk_cctr;
	ve_dnlp_brgt_ctrl = p->dnlp_brgt_ctrl;
	ve_dnlp_brgt_range = p->dnlp_brgt_range;
	ve_dnlp_brght_add = p->dnlp_brght_add;
	ve_dnlp_brght_max = p->dnlp_brght_max;
	ve_dnlp_almst_wht = p->dnlp_almst_wht;

	ve_dnlp_hghbin = p->dnlp_hghbin;
	ve_dnlp_hghnum = p->dnlp_hghnum;
	ve_dnlp_lowbin = p->dnlp_lowbin;
	ve_dnlp_lownum = p->dnlp_lownum;
	ve_dnlp_bkgend = p->dnlp_bkgend;
	ve_dnlp_bkgert = p->dnlp_bkgert;
	ve_dnlp_blkext = p->dnlp_blkext;
	ve_dnlp_whtext = p->dnlp_whtext;
	ve_dnlp_bextmx = p->dnlp_bextmx;
	ve_dnlp_wextmx = p->dnlp_wextmx;
	ve_dnlp_smhist_ck = p->dnlp_smhist_ck;
	ve_dnlp_glb_crate = p->dnlp_glb_crate;

	ve_dnlp_pstgma_brghtrate = p->dnlp_pstgma_brghtrate;
	ve_dnlp_pstgma_brghtrat1 = p->dnlp_pstgma_brghtrat1;
	ve_dnlp_wext_autorat = p->dnlp_wext_autorat;
	ve_dnlp_cliprate_min = p->dnlp_cliprate_min;
	ve_dnlp_adpcrat_lbnd = p->dnlp_adpcrat_lbnd;
	ve_dnlp_adpcrat_hbnd = p->dnlp_adpcrat_hbnd;
	ve_dnlp_adpmtd_lbnd = p->dnlp_adpmtd_lbnd;
	ve_dnlp_adpmtd_hbnd = p->dnlp_adpmtd_hbnd;
	ve_dnlp_set_bext = p->dnlp_set_bext;
	ve_dnlp_set_wext = p->dnlp_set_wext;
	ve_dnlp_satur_rat = p->dnlp_satur_rat;
	ve_dnlp_satur_max = p->dnlp_satur_max;
	ve_blk_prct_rng = p->blk_prct_rng;
	ve_blk_prct_max = p->blk_prct_max;
	ve_dnlp_lowrange = p->dnlp_lowrange;
	ve_dnlp_hghrange = p->dnlp_hghrange;
	ve_dnlp_auto_rng = p->dnlp_auto_rng;
	ve_dnlp_bin0_absmax = p->dnlp_bin0_absmax;
	ve_dnlp_bin0_sbtmax = p->dnlp_bin0_sbtmax;
	ve_dnlp_adpalpha_lrate = p->dnlp_adpalpha_lrate;
	ve_dnlp_adpalpha_hrate = p->dnlp_adpalpha_hrate;

	ve_dnlp_lrate00 = p->dnlp_lrate00;
	ve_dnlp_lrate02 = p->dnlp_lrate02;
	ve_dnlp_lrate04 = p->dnlp_lrate04;
	ve_dnlp_lrate06 = p->dnlp_lrate06;
	ve_dnlp_lrate08 = p->dnlp_lrate08;
	ve_dnlp_lrate10 = p->dnlp_lrate10;
	ve_dnlp_lrate12 = p->dnlp_lrate12;
	ve_dnlp_lrate14 = p->dnlp_lrate14;
	ve_dnlp_lrate16 = p->dnlp_lrate16;
	ve_dnlp_lrate18 = p->dnlp_lrate18;
	ve_dnlp_lrate20 = p->dnlp_lrate20;
	ve_dnlp_lrate22 = p->dnlp_lrate22;
	ve_dnlp_lrate24 = p->dnlp_lrate24;
	ve_dnlp_lrate26 = p->dnlp_lrate26;
	ve_dnlp_lrate28 = p->dnlp_lrate28;
	ve_dnlp_lrate30 = p->dnlp_lrate30;
	ve_dnlp_lrate32 = p->dnlp_lrate32;
	ve_dnlp_lrate34 = p->dnlp_lrate34;
	ve_dnlp_lrate36 = p->dnlp_lrate36;
	ve_dnlp_lrate38 = p->dnlp_lrate38;
	ve_dnlp_lrate40 = p->dnlp_lrate40;
	ve_dnlp_lrate42 = p->dnlp_lrate42;
	ve_dnlp_lrate44 = p->dnlp_lrate44;
	ve_dnlp_lrate46 = p->dnlp_lrate46;
	ve_dnlp_lrate48 = p->dnlp_lrate48;
	ve_dnlp_lrate50 = p->dnlp_lrate50;
	ve_dnlp_lrate52 = p->dnlp_lrate52;
	ve_dnlp_lrate54 = p->dnlp_lrate54;
	ve_dnlp_lrate56 = p->dnlp_lrate56;
	ve_dnlp_lrate58 = p->dnlp_lrate58;
	ve_dnlp_lrate60 = p->dnlp_lrate60;
	ve_dnlp_lrate62 = p->dnlp_lrate62;

	if (ve_en) {
		/* clear historic luma sum */
		ve_dnlp_luma_sum = 0;
		/* init tgt & lpf */
		for (i = 0; i < 64; i++) {
			ve_dnlp_tgt[i] = i << 2;
			ve_dnlp_lpf[i] = ve_dnlp_tgt[i] << ve_dnlp_rt;
		}
		/* calculate dnlp reg data */
		ve_dnlp_calculate_reg();
		/* load dnlp reg data */
		ve_dnlp_load_reg();
		/* enable dnlp */
		ve_enable_dnlp();
	} else {
		/* disable dnlp */
		ve_disable_dnlp();
	}
}

unsigned int ve_get_vs_cnt(void)
{
	return READ_VPP_REG(VPP_VDO_MEAS_VS_COUNT_LO);
}

void vpp_phase_lock_on_vs(unsigned int cycle,
			  unsigned int stamp,
			  bool         lock50,
			  unsigned int range_fast,
			  unsigned int range_slow)
{
	unsigned int vtotal_ori = READ_VPP_REG(ENCL_VIDEO_MAX_LNCNT);
	unsigned int vtotal     = lock50 ? 1349 : 1124;
	unsigned int stamp_in   = READ_VPP_REG(VDIN_MEAS_VS_COUNT_LO);
	unsigned int stamp_out  = ve_get_vs_cnt();
	unsigned int phase      = 0;
	unsigned int cnt = assist_cnt;/* READ_VPP_REG(ASSIST_SPARE8_REG1); */
	int step = 0, i = 0;
	/* get phase */
	if (stamp_out < stamp)
		phase = 0xffffffff - stamp + stamp_out + 1;
	else
		phase = stamp_out - stamp;
	while (phase >= cycle)
		phase -= cycle;
	/* 225~315 degree => tune fast panel output */
	if ((phase > ((cycle * 5) >> 3)) && (phase < ((cycle * 7) >> 3))) {
		vtotal -= range_slow;
		step = 1;
	} else if ((phase > (cycle >> 3)) && (phase < ((cycle * 3) >> 3))) {
		/* 45~135 degree => tune slow panel output */
		vtotal += range_slow;
		step = -1;
	} else if (phase >= ((cycle * 7) >> 3)) {
		/* 315~360 degree => tune fast panel output */
		vtotal -= range_fast;
		step = + 2;
	} else if (phase <= (cycle >> 3)) {
		/* 0~45 degree => tune slow panel output */
		vtotal += range_fast;
		step = -2;
	} else {/* 135~225 degree => keep still */
		vtotal = vtotal_ori;
		step = 0;
	}
	if (vtotal != vtotal_ori)
		WRITE_VPP_REG(ENCL_VIDEO_MAX_LNCNT, vtotal);
	if (cnt) {
		cnt--;
		/* WRITE_VPP_REG(ASSIST_SPARE8_REG1, cnt); */
		assist_cnt = cnt;
		if (cnt) {
			vpp_log[cnt][0] = stamp;
			vpp_log[cnt][1] = stamp_in;
			vpp_log[cnt][2] = stamp_out;
			vpp_log[cnt][3] = cycle;
			vpp_log[cnt][4] = phase;
			vpp_log[cnt][5] = vtotal;
			vpp_log[cnt][6] = step;
		} else {
			for (i = 127; i > 0; i--) {
				pr_amve_dbg("Ti=%10u Tio=%10u To=%10u CY=%6u ",
					vpp_log[i][0],
					vpp_log[i][1],
					vpp_log[i][2],
					vpp_log[i][3]);
				pr_amve_dbg("PH =%10u Vt=%4u S=%2d\n",
					vpp_log[i][4],
					vpp_log[i][5],
					vpp_log[i][6]);
			}
		}
	}

}

void ve_frame_size_patch(unsigned int width, unsigned int height)
{
	unsigned int vpp_size = height|(width << 16);

	if (READ_VPP_REG(VPP_VE_H_V_SIZE) != vpp_size)
		WRITE_VPP_REG(VPP_VE_H_V_SIZE, vpp_size);
}

void ve_dnlp_latch_process(void)
{
	if (vecm_latch_flag & FLAG_VE_DNLP) {
		vecm_latch_flag &= ~FLAG_VE_DNLP;
		ve_set_dnlp(&am_ve_dnlp);
	}
	if (vecm_latch_flag & FLAG_VE_NEW_DNLP) {
		vecm_latch_flag &= ~FLAG_VE_NEW_DNLP;
		ve_set_new_dnlp(&am_ve_new_dnlp);
	}
	if (vecm_latch_flag & FLAG_VE_DNLP_EN) {
		vecm_latch_flag &= ~FLAG_VE_DNLP_EN;
		ve_enable_dnlp();
		pr_amve_dbg("\n[amve..] set vpp_enable_dnlp OK!!!\n");
	}
	if (vecm_latch_flag & FLAG_VE_DNLP_DIS) {
		vecm_latch_flag &= ~FLAG_VE_DNLP_DIS;
		ve_disable_dnlp();
		pr_amve_dbg("\n[amve..] set vpp_disable_dnlp OK!!!\n");
	}
	if (dnlp_en && dnlp_status) {
		dnlp_status = 0;
		ve_set_dnlp_2();
		ve_enable_dnlp();
		pr_amve_dbg("\n[amve..] set vpp_enable_dnlp OK!!!\n");
	} else if (dnlp_en == 0) {
		dnlp_status = 1;
		ve_disable_dnlp();
		pr_amve_dbg("\n[amve..] set vpp_disable_dnlp OK!!!\n");
	}
}

void ve_lcd_gamma_process(void)
{
	if (vecm_latch_flag & FLAG_GAMMA_TABLE_EN) {
		vecm_latch_flag &= ~FLAG_GAMMA_TABLE_EN;
		vpp_enable_lcd_gamma_table();
		pr_amve_dbg("\n[amve..] set vpp_enable_lcd_gamma_table OK!!!\n");
	}
	if (vecm_latch_flag & FLAG_GAMMA_TABLE_DIS) {
		vecm_latch_flag &= ~FLAG_GAMMA_TABLE_DIS;
		vpp_disable_lcd_gamma_table();
		pr_amve_dbg("\n[amve..] set vpp_disable_lcd_gamma_table OK!!!\n");
	}
	if (vecm_latch_flag & FLAG_GAMMA_TABLE_R) {
		vecm_latch_flag &= ~FLAG_GAMMA_TABLE_R;
		vpp_set_lcd_gamma_table(video_gamma_table_r.data, H_SEL_R);
		pr_amve_dbg("\n[amve..] set vpp_set_lcd_gamma_table OK!!!\n");
	}
	if (vecm_latch_flag & FLAG_GAMMA_TABLE_G) {
		vecm_latch_flag &= ~FLAG_GAMMA_TABLE_G;
		vpp_set_lcd_gamma_table(video_gamma_table_g.data, H_SEL_G);
		pr_amve_dbg("\n[amve..] set vpp_set_lcd_gamma_table OK!!!\n");
	}
	if (vecm_latch_flag & FLAG_GAMMA_TABLE_B) {
		vecm_latch_flag &= ~FLAG_GAMMA_TABLE_B;
		vpp_set_lcd_gamma_table(video_gamma_table_b.data, H_SEL_B);
		pr_amve_dbg("\n[amve..] set vpp_set_lcd_gamma_table OK!!!\n");
	}
	if (vecm_latch_flag & FLAG_RGB_OGO) {
		vecm_latch_flag &= ~FLAG_RGB_OGO;
		if (video_rgb_ogo_mode_sw) {
			if (video_rgb_ogo.en) {
				vpp_set_lcd_gamma_table(
						video_gamma_table_r_adj.data,
						H_SEL_R);
				vpp_set_lcd_gamma_table(
						video_gamma_table_g_adj.data,
						H_SEL_G);
				vpp_set_lcd_gamma_table(
						video_gamma_table_b_adj.data,
						H_SEL_B);
			} else {
				vpp_set_lcd_gamma_table(
						video_gamma_table_r.data,
						H_SEL_R);
				vpp_set_lcd_gamma_table(
						video_gamma_table_g.data,
						H_SEL_G);
				vpp_set_lcd_gamma_table(
						video_gamma_table_b.data,
						H_SEL_B);
			}
			pr_amve_dbg("\n[amve..] set vpp_set_lcd_gamma_table OK!!!\n");
		} else {
			vpp_set_rgb_ogo(&video_rgb_ogo);
			pr_amve_dbg("\n[amve..] set vpp_set_rgb_ogo OK!!!\n");
		}
	}
}
void lvds_freq_process(void)
{
/* #if ((MESON_CPU_TYPE==MESON_CPU_TYPE_MESON6TV)|| */
/* (MESON_CPU_TYPE==MESON_CPU_TYPE_MESON6TVD)) */
/*  lvds freq 50Hz/60Hz */
/* if (frame_lock_freq == 1){//50 hz */
/* // panel freq is 60Hz => change back to 50Hz */
/* if (READ_VPP_REG(ENCP_VIDEO_MAX_LNCNT) < 1237) */
/* (1124 + 1349 +1) / 2 */
/* WRITE_VPP_REG(ENCP_VIDEO_MAX_LNCNT, 1349); */
/* } */
/* else if (frame_lock_freq == 2){//60 hz */
/* // panel freq is 50Hz => change back to 60Hz */
/* if(READ_VPP_REG(ENCP_VIDEO_MAX_LNCNT) >= 1237) */
/* (1124 + 1349 + 1) / 2 */
/* WRITE_VPP_REG(ENCP_VIDEO_MAX_LNCNT, 1124); */
/* } */
/* else if (frame_lock_freq == 0){ */
/*  lvds freq 50Hz/60Hz */
/* if (vecm_latch_flag & FLAG_LVDS_FREQ_SW){  //50 hz */
/* // panel freq is 60Hz => change back to 50Hz */
/* if (READ_VPP_REG(ENCP_VIDEO_MAX_LNCNT) < 1237) */
/* (1124 + 1349 +1) / 2 */
/* WRITE_VPP_REG(ENCP_VIDEO_MAX_LNCNT, 1349); */
/* }else{	 //60 hz */
/* // panel freq is 50Hz => change back to 60Hz */
/* if (READ_VPP_REG(ENCP_VIDEO_MAX_LNCNT) >= 1237) */
/* (1124 + 1349 + 1) / 2 */
/* WRITE_VPP_REG(ENCP_VIDEO_MAX_LNCNT, 1124); */
/* } */
/* } */
/* #endif */
}

void ve_dnlp_param_update(void)
{
	if (am_ve_dnlp.en    >  1)
		am_ve_dnlp.en    =  1;
	if (am_ve_dnlp.black > 16)
		am_ve_dnlp.black = 16;
	if (am_ve_dnlp.white > 16)
		am_ve_dnlp.white = 16;
	vecm_latch_flag |= FLAG_VE_DNLP;
}

void ve_new_dnlp_param_update(void)
{
	if (am_ve_new_dnlp.en > 1)
		am_ve_new_dnlp.en = 1;
	if (am_ve_new_dnlp.cliprate > 256)
		am_ve_new_dnlp.cliprate = 256;
	if (am_ve_new_dnlp.lowrange > 54)
		am_ve_new_dnlp.lowrange = 54;
	if (am_ve_new_dnlp.hghrange > 54)
		am_ve_new_dnlp.hghrange = 54;
	if (am_ve_new_dnlp.lowalpha > 48)
		am_ve_new_dnlp.lowalpha = 48;
	if (am_ve_new_dnlp.midalpha > 48)
		am_ve_new_dnlp.midalpha = 48;
	if (am_ve_new_dnlp.hghalpha > 48)
		am_ve_new_dnlp.hghalpha = 48;

	if (am_ve_new_dnlp.dnlp_blk_cctr > 64)
		am_ve_new_dnlp.dnlp_blk_cctr = 64;
	if (am_ve_new_dnlp.dnlp_brgt_ctrl > 64)
		am_ve_new_dnlp.dnlp_brgt_ctrl = 64;
	if (am_ve_new_dnlp.dnlp_brgt_range > 64)
		am_ve_new_dnlp.dnlp_brgt_range = 64;
	if (am_ve_new_dnlp.dnlp_brght_add > 64)
		am_ve_new_dnlp.dnlp_brght_add = 64;
	if (am_ve_new_dnlp.dnlp_brght_max > 64)
		am_ve_new_dnlp.dnlp_brght_max = 64;
	if (am_ve_new_dnlp.dnlp_almst_wht > 64)
		am_ve_new_dnlp.dnlp_almst_wht = 64;

	if (am_ve_new_dnlp.dnlp_hghbin > 64)
		am_ve_new_dnlp.dnlp_hghbin = 64;
	if (am_ve_new_dnlp.dnlp_hghnum > 64)
		am_ve_new_dnlp.dnlp_hghnum = 64;
	if (am_ve_new_dnlp.dnlp_lowbin > 64)
		am_ve_new_dnlp.dnlp_lowbin = 64;
	if (am_ve_new_dnlp.dnlp_lownum > 64)
		am_ve_new_dnlp.dnlp_lownum = 64;
	if (am_ve_new_dnlp.dnlp_bkgend > 64)
		am_ve_new_dnlp.dnlp_bkgend = 64;
	if (am_ve_new_dnlp.dnlp_bkgert > 64)
		am_ve_new_dnlp.dnlp_bkgert = 64;
	if (am_ve_new_dnlp.dnlp_blkext > 64)
		am_ve_new_dnlp.dnlp_blkext = 64;
	if (am_ve_new_dnlp.dnlp_whtext > 64)
		am_ve_new_dnlp.dnlp_whtext = 64;
	if (am_ve_new_dnlp.dnlp_bextmx > 64)
		am_ve_new_dnlp.dnlp_bextmx = 64;
	if (am_ve_new_dnlp.dnlp_wextmx > 64)
		am_ve_new_dnlp.dnlp_wextmx = 64;
	if (am_ve_new_dnlp.dnlp_smhist_ck > 1)
		am_ve_new_dnlp.dnlp_smhist_ck = 1;
	if (am_ve_new_dnlp.dnlp_glb_crate > 1)
		am_ve_new_dnlp.dnlp_glb_crate = 1;

	if (am_ve_new_dnlp.dnlp_lrate00 > 64)
		am_ve_new_dnlp.dnlp_lrate00 = 64;
	if (am_ve_new_dnlp.dnlp_lrate02 > 64)
		am_ve_new_dnlp.dnlp_lrate02 = 64;
	if (am_ve_new_dnlp.dnlp_lrate04 > 64)
		am_ve_new_dnlp.dnlp_lrate04 = 64;
	if (am_ve_new_dnlp.dnlp_lrate06 > 64)
		am_ve_new_dnlp.dnlp_lrate06 = 64;
	if (am_ve_new_dnlp.dnlp_lrate08 > 64)
		am_ve_new_dnlp.dnlp_lrate08 = 64;
	if (am_ve_new_dnlp.dnlp_lrate10 > 64)
		am_ve_new_dnlp.dnlp_lrate10 = 64;
	if (am_ve_new_dnlp.dnlp_lrate12 > 64)
		am_ve_new_dnlp.dnlp_lrate12 = 64;
	if (am_ve_new_dnlp.dnlp_lrate14 > 64)
		am_ve_new_dnlp.dnlp_lrate14 = 64;
	if (am_ve_new_dnlp.dnlp_lrate16 > 64)
		am_ve_new_dnlp.dnlp_lrate16 = 64;
	if (am_ve_new_dnlp.dnlp_lrate18 > 64)
		am_ve_new_dnlp.dnlp_lrate18 = 64;
	if (am_ve_new_dnlp.dnlp_lrate20 > 64)
		am_ve_new_dnlp.dnlp_lrate20 = 64;
	if (am_ve_new_dnlp.dnlp_lrate22 > 64)
		am_ve_new_dnlp.dnlp_lrate22 = 64;
	if (am_ve_new_dnlp.dnlp_lrate24 > 64)
		am_ve_new_dnlp.dnlp_lrate24 = 64;
	if (am_ve_new_dnlp.dnlp_lrate26 > 64)
		am_ve_new_dnlp.dnlp_lrate26 = 64;
	if (am_ve_new_dnlp.dnlp_lrate28 > 64)
		am_ve_new_dnlp.dnlp_lrate28 = 64;
	if (am_ve_new_dnlp.dnlp_lrate30 > 64)
		am_ve_new_dnlp.dnlp_lrate30 = 64;
	if (am_ve_new_dnlp.dnlp_lrate32 > 64)
		am_ve_new_dnlp.dnlp_lrate32 = 64;
	if (am_ve_new_dnlp.dnlp_lrate34 > 64)
		am_ve_new_dnlp.dnlp_lrate34 = 64;
	if (am_ve_new_dnlp.dnlp_lrate36 > 64)
		am_ve_new_dnlp.dnlp_lrate36 = 64;
	if (am_ve_new_dnlp.dnlp_lrate38 > 64)
		am_ve_new_dnlp.dnlp_lrate38 = 64;
	if (am_ve_new_dnlp.dnlp_lrate40 > 64)
		am_ve_new_dnlp.dnlp_lrate40 = 64;
	if (am_ve_new_dnlp.dnlp_lrate42 > 64)
		am_ve_new_dnlp.dnlp_lrate42 = 64;
	if (am_ve_new_dnlp.dnlp_lrate44 > 64)
		am_ve_new_dnlp.dnlp_lrate44 = 64;
	if (am_ve_new_dnlp.dnlp_lrate46 > 64)
		am_ve_new_dnlp.dnlp_lrate46 = 64;
	if (am_ve_new_dnlp.dnlp_lrate48 > 64)
		am_ve_new_dnlp.dnlp_lrate48 = 64;
	if (am_ve_new_dnlp.dnlp_lrate50 > 64)
		am_ve_new_dnlp.dnlp_lrate50 = 64;
	if (am_ve_new_dnlp.dnlp_lrate52 > 64)
		am_ve_new_dnlp.dnlp_lrate52 = 64;
	if (am_ve_new_dnlp.dnlp_lrate54 > 64)
		am_ve_new_dnlp.dnlp_lrate54 = 64;
	if (am_ve_new_dnlp.dnlp_lrate56 > 64)
		am_ve_new_dnlp.dnlp_lrate56 = 64;
	if (am_ve_new_dnlp.dnlp_lrate58 > 64)
		am_ve_new_dnlp.dnlp_lrate58 = 64;
	if (am_ve_new_dnlp.dnlp_lrate60 > 64)
		am_ve_new_dnlp.dnlp_lrate60 = 64;
	if (am_ve_new_dnlp.dnlp_lrate62 > 64)
		am_ve_new_dnlp.dnlp_lrate62 = 64;

	vecm_latch_flag |= FLAG_VE_NEW_DNLP;
}



static void video_data_limitation(int *val)
{
	if (*val > 1023)
		*val = 1023;
	if (*val < 0)
		*val = 0;
}

static void video_lookup(struct tcon_gamma_table_s *tbl, int *val)
{
	unsigned int idx = (*val) >> 2, mod = (*val) & 3;

	if (idx < 255)
		*val = tbl->data[idx] +
		(((tbl->data[idx + 1] - tbl->data[idx]) * mod + 2) >> 2);
	else
		*val = tbl->data[idx] +
		(((1023 - tbl->data[idx]) * mod + 2) >> 2);
}

static void video_set_rgb_ogo(void)
{
	int i = 0, r = 0, g = 0, b = 0;

	for (i = 0; i < 256; i++) {
		r = video_curve_2d2.data[i];
		g = video_curve_2d2.data[i];
		b = video_curve_2d2.data[i];
		/* Pre_offset */
		r += video_rgb_ogo.r_pre_offset;
		g += video_rgb_ogo.g_pre_offset;
		b += video_rgb_ogo.b_pre_offset;
		video_data_limitation(&r);
		video_data_limitation(&g);
		video_data_limitation(&b);
		/* Gain */
		r  *= video_rgb_ogo.r_gain;
		r >>= 10;
		g  *= video_rgb_ogo.g_gain;
		g >>= 10;
		b  *= video_rgb_ogo.b_gain;
		b >>= 10;
		video_data_limitation(&r);
		video_data_limitation(&g);
		video_data_limitation(&b);
		/* Post_offset */
		r += video_rgb_ogo.r_post_offset;
		g += video_rgb_ogo.g_post_offset;
		b += video_rgb_ogo.b_post_offset;
		video_data_limitation(&r);
		video_data_limitation(&g);
		video_data_limitation(&b);
		video_lookup(&video_curve_2d2_inv, &r);
		video_lookup(&video_curve_2d2_inv, &g);
		video_lookup(&video_curve_2d2_inv, &b);
		/* Get gamma_ogo = curve_2d2_inv_ogo * gamma */
		video_lookup(&video_gamma_table_r, &r);
		video_lookup(&video_gamma_table_g, &g);
		video_lookup(&video_gamma_table_b, &b);
		/* Save gamma_ogo */
		video_gamma_table_r_adj.data[i] = r;
		video_gamma_table_g_adj.data[i] = g;
		video_gamma_table_b_adj.data[i] = b;
	}
}

void ve_ogo_param_update(void)
{
	if (video_rgb_ogo.en > 1)
		video_rgb_ogo.en = 1;
	if (video_rgb_ogo.r_pre_offset > 1023)
		video_rgb_ogo.r_pre_offset = 1023;
	if (video_rgb_ogo.r_pre_offset < -1024)
		video_rgb_ogo.r_pre_offset = -1024;
	if (video_rgb_ogo.g_pre_offset > 1023)
		video_rgb_ogo.g_pre_offset = 1023;
	if (video_rgb_ogo.g_pre_offset < -1024)
		video_rgb_ogo.g_pre_offset = -1024;
	if (video_rgb_ogo.b_pre_offset > 1023)
		video_rgb_ogo.b_pre_offset = 1023;
	if (video_rgb_ogo.b_pre_offset < -1024)
		video_rgb_ogo.b_pre_offset = -1024;
	if (video_rgb_ogo.r_gain > 2047)
		video_rgb_ogo.r_gain = 2047;
	if (video_rgb_ogo.r_gain < 0)
		video_rgb_ogo.r_gain = 0;
	if (video_rgb_ogo.g_gain > 2047)
		video_rgb_ogo.g_gain = 2047;
	if (video_rgb_ogo.g_gain < 0)
		video_rgb_ogo.g_gain = 0;
	if (video_rgb_ogo.b_gain > 2047)
		video_rgb_ogo.b_gain = 2047;
	if (video_rgb_ogo.b_gain < 0)
		video_rgb_ogo.b_gain = 0;
	if (video_rgb_ogo.r_post_offset > 1023)
		video_rgb_ogo.r_post_offset = 1023;
	if (video_rgb_ogo.r_post_offset < -1024)
		video_rgb_ogo.r_post_offset = -1024;
	if (video_rgb_ogo.g_post_offset > 1023)
		video_rgb_ogo.g_post_offset = 1023;
	if (video_rgb_ogo.g_post_offset < -1024)
		video_rgb_ogo.g_post_offset = -1024;
	if (video_rgb_ogo.b_post_offset > 1023)
		video_rgb_ogo.b_post_offset = 1023;
	if (video_rgb_ogo.b_post_offset < -1024)
		video_rgb_ogo.b_post_offset = -1024;
	if (video_rgb_ogo_mode_sw)
		video_set_rgb_ogo();

	vecm_latch_flag |= FLAG_RGB_OGO;
}

static unsigned int vlock_check_input_hz(struct vframe_s *vf)
{
	unsigned int ret_hz = 0;
	unsigned int duration = vf->duration;

	if ((vf->source_type != VFRAME_SOURCE_TYPE_CVBS) &&
		(vf->source_type != VFRAME_SOURCE_TYPE_HDMI))
		ret_hz = 0;
	else if (vf->source_type == VFRAME_SOURCE_TYPE_HDMI) {
		if (duration != 0)
			ret_hz = (96000 + duration/2)/duration;
	} else if (vf->source_type == VFRAME_SOURCE_TYPE_CVBS) {
		if (vf->source_mode == VFRAME_SOURCE_MODE_NTSC)
			ret_hz = 60;
		else if ((vf->source_mode == VFRAME_SOURCE_MODE_PAL) ||
			(vf->source_mode == VFRAME_SOURCE_MODE_SECAM))
			ret_hz = 50;
		else
			ret_hz = 0;
	}
	return ret_hz;
}

static unsigned int vlock_check_output_hz(unsigned int sync_duration_num)
{
	unsigned int ret_hz = 0;

	switch (sync_duration_num) {
	case 24:
		ret_hz = 24;
		break;
	case 30:
		ret_hz = 30;
		break;
	case 50:
		ret_hz = 50;
		break;
	case 60:
		ret_hz = 60;
		break;
	case 100:
		ret_hz = 100;
		break;
	case 120:
		ret_hz = 120;
		break;
	default:
		ret_hz = 0;
		break;
	}
	return ret_hz;
}
static void vlock_enable(bool enable)
{
	unsigned int tmp_value;

	amvecm_hiu_reg_read(HHI_HDMI_PLL_CNTL6, &tmp_value);
	if (is_meson_gxtvbb_cpu()) {
		if (vlock_mode == VLOCK_MODE_MANUAL_PLL) {
			amvecm_hiu_reg_write_bits(HHI_HDMI_PLL_CNTL6, 0, 20, 1);
			if (is_meson_gxtvbb_cpu() &&
				(((tmp_value >> 21) & 0x3) != 2))
				amvecm_hiu_reg_write_bits(HHI_HDMI_PLL_CNTL6,
					2, 21, 2);
		} else
			amvecm_hiu_reg_write_bits(HHI_HDMI_PLL_CNTL6,
				enable, 20, 1);
	} else if (is_meson_txl_cpu() || is_meson_txlx_cpu()) {
		if (vlock_mode == VLOCK_MODE_MANUAL_PLL)
			amvecm_hiu_reg_write_bits(HHI_HDMI_PLL_CNTL5, 0, 3, 1);
		else
			amvecm_hiu_reg_write_bits(HHI_HDMI_PLL_CNTL5,
				enable, 3, 1);
	}
}
static void vlock_setting(struct vframe_s *vf,
		unsigned int input_hz, unsigned int output_hz)
{
	unsigned int freq_hz = 0, hiu_reg_value_2_addr = HHI_HDMI_PLL_CNTL2;
	unsigned int reg_value = 0, hiu_reg_value, hiu_reg_value_2;
	unsigned int hiu_m_val, hiu_frac_val;

	amvecm_hiu_reg_write(HHI_VID_LOCK_CLK_CNTL, 0x80);
	if (vlock_mode == VLOCK_MODE_ENC) {
		am_set_regmap(&vlock_enc_lcd720x480);
		/* VLOCK_CNTL_EN disable */
		vlock_enable(0);
		/* disable to adjust pll */
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 0, 29, 1);
		/* CFG_VID_LOCK_ADJ_EN enable */
		WRITE_VPP_REG_BITS(ENCL_MAX_LINE_SWITCH_POINT, 1, 13, 1);
		/* enable to adjust pll */
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 1, 30, 1);
		/*clear accum1 value*/
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 1, 2, 1);
		/*clear accum0 value*/
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 1, 5, 1);
	}
	if ((vlock_mode == VLOCK_MODE_AUTO_PLL) ||
		(vlock_mode == VLOCK_MODE_MANUAL_PLL)) {
		/* av pal in,1080p60 hdmi out as default */
		if ((vlock_debug & 0x1000) == 0)
			am_set_regmap(&vlock_pll_in50hz_out60hz);
		/*set input & output freq*/
		/*bit0~7:input freq*/
		/*bit8~15:output freq*/
		freq_hz = input_hz | (output_hz << 8);
		WRITE_VPP_REG_BITS(VPU_VLOCK_MISC_CTRL, freq_hz, 0, 16);
		/*Ifrm_cnt_mod:0x3001(bit23~16);*/
		/*(output_freq/input_freq)*Ifrm_cnt_mod must be integer*/
		if (vlock_adapt == 0)
			WRITE_VPP_REG_BITS(VPU_VLOCK_MISC_CTRL, 1, 16, 8);
		else
			WRITE_VPP_REG_BITS(VPU_VLOCK_MISC_CTRL,
				input_hz, 16, 8);
		/*set PLL M_INT;PLL M_frac*/
		/* WRITE_VPP_REG_BITS(VPU_VLOCK_MX4096, */
		/* READ_CBUS_REG_BITS(HHI_HDMI_PLL_CNTL,0,9),12,9); */
		amvecm_hiu_reg_read(HHI_HDMI_PLL_CNTL, &hiu_reg_value);
		amvecm_hiu_reg_read(hiu_reg_value_2_addr,
			&hiu_reg_value_2);
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXTVBB)) {
			hiu_m_val = hiu_reg_value & 0x1FF;
			hiu_frac_val = hiu_reg_value_2 & 0x3FF;
			if (hiu_reg_value_2 & 0x800) {
				hiu_m_val -= 1;
				if (hiu_reg_value_2 & 0x400)
					hiu_m_val -= 1;
				hiu_frac_val = 0x400 -
					((~(hiu_frac_val - 1)) & 0x3ff);
			} else if (hiu_reg_value_2 & 0x400) {
				hiu_m_val += 1;
			}
			reg_value = (hiu_m_val << 12)
				+ (hiu_frac_val << 2);
		}
		WRITE_VPP_REG_BITS(VPU_VLOCK_MX4096, reg_value, 0, 21);

		/* vlock module output goes to which module */
		switch (READ_VPP_REG_BITS(VPU_VIU_VENC_MUX_CTRL, 0, 2)) {
		case 0:/* ENCL */
			WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 0, 26, 2);
			break;
		case 1:/* ENCI */
			WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 2, 26, 2);
			break;
		case 2: /* ENCP */
			WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 1, 26, 2);
			break;
		default:
			break;
		}
		/*enable vlock to adj pll*/
		/* CFG_VID_LOCK_ADJ_EN disable */
		WRITE_VPP_REG_BITS(ENCL_MAX_LINE_SWITCH_POINT, 0, 13, 1);
		/* disable to adjust pll */
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 0, 30, 1);
		/* VLOCK_CNTL_EN enable */
		vlock_enable(1);
		/* enable to adjust pll */
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 1, 29, 1);
	}
	if ((vf->source_type == VFRAME_SOURCE_TYPE_TUNER) ||
		(vf->source_type == VFRAME_SOURCE_TYPE_CVBS))
		/* Input Vsync source select from tv-decoder */
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 2, 16, 3);
	else if (vf->source_type == VFRAME_SOURCE_TYPE_HDMI)
		/* Input Vsync source select from hdmi-rx */
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 1, 16, 3);
	WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 1, 31, 1);
}
void vlock_vmode_check(void)
{
	const struct vinfo_s *vinfo;
	unsigned int tmp_value, hiu_reg_addr;
	char cur_vout_mode[64];

	if (vlock_en == 0)
		return;

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXL)
		hiu_reg_addr = HHI_HDMI_PLL_CNTL1;
	else
		hiu_reg_addr = HHI_HDMI_PLL_CNTL2;

	vinfo = get_current_vinfo();
	vlock_vmode_changed = 0;
	memset(cur_vout_mode, 0, sizeof(cur_vout_mode));
	strcpy(cur_vout_mode, vinfo->name);
	if (strcmp(cur_vout_mode, pre_vout_mode) != 0) {
		amvecm_hiu_reg_read(hiu_reg_addr, &tmp_value);
		pre_hiu_reg_frac = tmp_value & 0xfff;
		amvecm_hiu_reg_read(HHI_HDMI_PLL_CNTL, &tmp_value);
		pre_hiu_reg_m = tmp_value & 0x1ff;
		if (vlock_debug & 0x10)
			pr_info("[%s]:vout mode changed:%s==>%s\n",
				__func__, pre_vout_mode, cur_vout_mode);
		memset(pre_vout_mode, 0, sizeof(pre_vout_mode));
		strcpy(pre_vout_mode, cur_vout_mode);
		vlock_vmode_changed = 1;
	}
}
static void vlock_disable_step1(void)
{
	unsigned int m_reg_value, tmp_value;
	unsigned int hiu_reg_addr;

	/* VLOCK_CNTL_EN disable */
	vlock_enable(0);
	vlock_vmode_check();
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXL)
		hiu_reg_addr = HHI_HDMI_PLL_CNTL1;
	else
		hiu_reg_addr = HHI_HDMI_PLL_CNTL2;

	amvecm_hiu_reg_read(hiu_reg_addr, &tmp_value);
	m_reg_value = tmp_value & 0xfff;
	if ((m_reg_value != pre_hiu_reg_frac) &&
		(pre_hiu_reg_m != 0)) {
		tmp_value = (tmp_value & 0xfffff000) |
			(pre_hiu_reg_frac & 0xfff);
		amvecm_hiu_reg_write(hiu_reg_addr, tmp_value);
	}
	amvecm_hiu_reg_read(HHI_HDMI_PLL_CNTL, &tmp_value);
	m_reg_value = tmp_value & 0x1ff;
	if ((m_reg_value != pre_hiu_reg_m) &&
		(pre_hiu_reg_m != 0)) {
		tmp_value = (tmp_value & 0xfffffe00) |
			(pre_hiu_reg_m & 0x1ff);
		amvecm_hiu_reg_write(HHI_HDMI_PLL_CNTL, tmp_value);
	}
	vlock_dis_cnt = vlock_dis_cnt_limit;
	memset(pre_vout_mode, 0, sizeof(pre_vout_mode));
	pre_vmode = VMODE_INIT_NULL;
	pre_source_type = VFRAME_SOURCE_TYPE_OTHERS;
	pre_source_mode = VFRAME_SOURCE_MODE_OTHERS;
	pre_input_freq = 0;
	pre_output_freq = 0;
	vlock_state = VLOCK_STATE_DISABLE_STEP1_DONE;
}

static void vlock_disable_step2(void)
{
	unsigned int temp_val;
	/* need delay to disable follow regs(vlsi suggest!!!) */
	if (vlock_dis_cnt > 0)
		vlock_dis_cnt--;
	if (vlock_dis_cnt == 0) {
		/* disable to adjust pll */
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 0, 29, 1);
		/* CFG_VID_LOCK_ADJ_EN disable */
		WRITE_VPP_REG_BITS(ENCL_MAX_LINE_SWITCH_POINT,
				0, 13, 1);
		/* disable to adjust pll */
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 0, 30, 1);
		/* disable vid_lock_en */
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 0, 31, 1);
		vlock_state = VLOCK_STATE_DISABLE_STEP2_DONE;
		amvecm_hiu_reg_read(HHI_HDMI_PLL_CNTL6, &temp_val);
		if (is_meson_gxtvbb_cpu() && (((temp_val >> 21) & 0x3) != 0))
			amvecm_hiu_reg_write_bits(HHI_HDMI_PLL_CNTL6, 0, 21, 2);
	}
}
static void vlock_enable_step1(struct vframe_s *vf, struct vinfo_s *vinfo,
	unsigned int input_hz, unsigned int output_hz)
{
	vlock_setting(vf, input_hz, output_hz);
	if (vlock_debug & 0x10) {
		pr_info("%s:vmode/source_type/source_mode/input_freq/output_freq:\n",
			__func__);
		pr_info("\t%d/%d/%d/%d/%d=>%d/%d/%d/%d/%d\n",
			pre_vmode, pre_source_type, pre_source_mode,
			pre_input_freq, pre_output_freq,
			vinfo->mode, vf->source_type, vf->source_mode,
			input_hz, output_hz);
	}
	pre_vmode = vinfo->mode;
	pre_source_type = vf->source_type;
	pre_source_mode = vf->source_mode;
	pre_input_freq = input_hz;
	pre_output_freq = output_hz;
	vlock_sync_limit_flag = 0;
	vlock_vmode_changed = 0;
	vlock_dis_cnt = 0;
	vlock_state = VLOCK_STATE_ENABLE_STEP1_DONE;
	vlock_log_cnt = 0;
}
#define VLOCK_REG_NUM	33
struct vlock_log_s {
	unsigned int pll_m;
	unsigned int pll_frac;
	unsigned int vlock_regs[VLOCK_REG_NUM];
};
struct vlock_log_s *vlock_log;

void vlock_log_start(void)
{
	unsigned int size_mem;

	size_mem = vlock_log_size * sizeof(struct vlock_log_s);
	vlock_log = kzalloc(size_mem, GFP_KERNEL);

	if (vlock_log == NULL) {
		kfree(vlock_log);
		return;
	}

	vlock_log_en = 1;
	pr_info("%s done\n", __func__);
}
void vlock_log_stop(void)
{
	if (vlock_log != NULL)
		kfree(vlock_log);
	vlock_log_en = 0;
	pr_info("%s done\n", __func__);
}
void vlock_log_print(void)
{
	unsigned int i, j;

	for (i = 0; i < vlock_log_size; i++) {
		pr_info("\n*******[%d]pll_m:0x%x,pll_frac:0x%x*******\n",
			i, vlock_log[i].pll_m, vlock_log[i].pll_frac);
		for (j = 0; j < VLOCK_REG_NUM;) {
			if ((j%8 == 0) && ((j + 7) < VLOCK_REG_NUM)) {
				pr_info("0x%08x\t0x%08x\t0x%08x\t0x%08x\t0x%08x\t0x%08x\t0x%08x\t0x%08x\n",
					vlock_log[i].vlock_regs[j],
					vlock_log[i].vlock_regs[j+1],
					vlock_log[i].vlock_regs[j+2],
					vlock_log[i].vlock_regs[j+3],
					vlock_log[i].vlock_regs[j+4],
					vlock_log[i].vlock_regs[j+5],
					vlock_log[i].vlock_regs[j+6],
					vlock_log[i].vlock_regs[j+7]);
				j += 8;
			} else {
				pr_info("0x%08x\t",
					vlock_log[i].vlock_regs[j]);
				j++;
			}

		}
	}
	pr_info("%s done\n", __func__);
}

static void vlock_enable_step3(void)
{
	unsigned int m_reg_value, tmp_value, abs_val;
	unsigned int hiu_reg_addr, i;

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXL)
		hiu_reg_addr = HHI_HDMI_PLL_CNTL1;
	else
		hiu_reg_addr = HHI_HDMI_PLL_CNTL2;

	/*vs_i*/
	tmp_value = READ_VPP_REG(VPU_VLOCK_RO_VS_I_DIST);
	abs_val = abs(vlock_log_last_ivcnt - tmp_value);
	if ((abs_val > vlock_log_delta_ivcnt) &&
		(vlock_log_delta_en & (1 << 0)))
		pr_info("%s: abs_ivcnt over 0x%x:0x%x(last:0x%x,cur:0x%x)\n",
			__func__, vlock_log_delta_ivcnt,
			abs_val, vlock_log_last_ivcnt, tmp_value);
	vlock_log_last_ivcnt = tmp_value;
	/*vs_o*/
	tmp_value = READ_VPP_REG(VPU_VLOCK_RO_VS_O_DIST);
	abs_val = abs(vlock_log_last_ovcnt - tmp_value);
	if ((abs_val > vlock_log_delta_ovcnt) &&
		(vlock_log_delta_en & (1 << 1)))
		pr_info("%s: abs_ovcnt over 0x%x:0x%x(last:0x%x,cur:0x%x)\n",
			__func__, vlock_log_delta_ovcnt,
			abs_val, vlock_log_last_ivcnt, tmp_value);
	vlock_log_last_ovcnt = tmp_value;
	/*delta_vs*/
	abs_val = abs(vlock_log_last_ovcnt - vlock_log_last_ivcnt);
	if ((abs_val > vlock_log_delta_vcnt) && (vlock_log_delta_en & (1 << 2)))
		pr_info("%s: abs_vcnt over 0x%x:0x%x(ivcnt:0x%x,ovcnt:0x%x)\n",
			__func__, vlock_log_delta_vcnt,
			abs_val, vlock_log_last_ivcnt, vlock_log_last_ovcnt);

	m_reg_value = READ_VPP_REG(VPU_VLOCK_RO_M_INT_FRAC);
	if (vlock_log_en && (vlock_log_cnt < vlock_log_size)) {
		vlock_log[vlock_log_cnt].pll_frac = (m_reg_value & 0xfff) >> 2;
		vlock_log[vlock_log_cnt].pll_m = (m_reg_value >> 16) & 0x1ff;
		for (i = 0; i < VLOCK_REG_NUM; i++)
			vlock_log[vlock_log_cnt].vlock_regs[i] =
				READ_VPP_REG(0x3000 + i);
		vlock_log_cnt++;
	}
	if (m_reg_value == 0) {
		vlock_state = VLOCK_STATE_ENABLE_FORCE_RESET;
		if (vlock_debug & 0x100)
			pr_info("%s:vlock work abnormal! force reset vlock\n",
				__func__);
		return;
	}
	/*vlsi suggest config:don't enable load signal,*/
	/*on gxtvbb this load signal will effect SSG,*/
	/*which may result in flashes black*/
	amvecm_hiu_reg_read(HHI_HDMI_PLL_CNTL6, &tmp_value);
	if (is_meson_gxtvbb_cpu() && (((tmp_value >> 21) & 0x3) != 2))
		amvecm_hiu_reg_write_bits(HHI_HDMI_PLL_CNTL6, 2, 21, 2);
	/*frac*/
	amvecm_hiu_reg_read(hiu_reg_addr, &tmp_value);
	abs_val = abs(((m_reg_value & 0xfff) >> 2) - (tmp_value & 0xfff));
	if ((abs_val > vlock_log_delta_frac) &&
		(vlock_log_delta_en & (1 << 3)))
		pr_info("vlock frac delta:%d(0x%x,0x%x)\n",
			abs_val, ((m_reg_value & 0xfff) >> 2),
			(tmp_value & 0xfff));
	if (abs_val > vlock_delta_limit) {
		tmp_value = (tmp_value & 0xfffff000) |
			((m_reg_value & 0xfff) >> 2);
		amvecm_hiu_reg_write(hiu_reg_addr, tmp_value);
	}
	/*m*/
	amvecm_hiu_reg_read(HHI_HDMI_PLL_CNTL, &tmp_value);
	abs_val = abs(((m_reg_value >> 16) & 0x1ff) - (tmp_value & 0x1ff));
	if ((abs_val > vlock_log_delta_m) && (vlock_log_delta_en & (1 << 4)))
		pr_info("vlock m delta:%d(0x%x,0x%x)\n",
			abs_val, ((m_reg_value >> 16) & 0x1ff),
			(tmp_value & 0x1ff));
	if (((m_reg_value >> 16) & 0x1ff) != (tmp_value & 0x1ff)) {
		tmp_value = (tmp_value & 0xfffffe00) |
			((m_reg_value >> 16) & 0x1ff);
		amvecm_hiu_reg_write(HHI_HDMI_PLL_CNTL, tmp_value);
	}
}
/* won't change this function internal seqence,*/
/* if really need change,please be carefull */
void amve_vlock_process(struct vframe_s *vf)
{
	struct vinfo_s *vinfo;
	unsigned int input_hz, output_hz, input_vs_cnt;

	if (vecm_latch_flag & FLAG_VLOCK_DIS) {
		vlock_disable_step1();
		vlock_en = 0;
		vecm_latch_flag &= ~FLAG_VLOCK_DIS;
		if (vlock_debug & 0x1)
			pr_info("[%s]disable vlock module!!!\n", __func__);
		return;
	}
	vinfo = get_current_vinfo();
	input_hz = vlock_check_input_hz(vf);
	output_hz = vlock_check_output_hz(vinfo->sync_duration_num);
	vlock_dis_cnt_no_vf = 0;
	if (vecm_latch_flag & FLAG_VLOCK_EN) {
		vlock_enable_step1(vf, vinfo, input_hz, output_hz);
		vlock_en = 1;
		vecm_latch_flag &= ~FLAG_VLOCK_EN;
	}
	if (vlock_state == VLOCK_STATE_DISABLE_STEP1_DONE) {
		vlock_disable_step2();
		return;
	}
	if (vlock_en == 1) {
		if (((input_hz != output_hz) && (vlock_adapt == 0)) ||
			(input_hz == 0) || (output_hz == 0) ||
			(((vf->type_original & VIDTYPE_TYPEMASK)
				!= VIDTYPE_PROGRESSIVE) &&
				is_meson_txlx_package_962E())) {
			if ((vlock_state != VLOCK_STATE_DISABLE_STEP2_DONE) &&
				(vlock_state != VLOCK_STATE_NULL))
				vlock_disable_step1();
			if (vlock_debug & 0x1)
				pr_info("[%s]auto disable vlock module for no support case!!!\n",
					__func__);
			return;
		}
		vlock_vmode_check();
		if ((vinfo->mode != pre_vmode) ||
			(vf->source_type != pre_source_type) ||
			(vf->source_mode != pre_source_mode) ||
			(input_hz != pre_input_freq) ||
			(output_hz != pre_output_freq) ||
			vlock_vmode_changed ||
			(vlock_state == VLOCK_STATE_ENABLE_FORCE_RESET))
			vlock_enable_step1(vf, vinfo, input_hz, output_hz);
		if ((vlock_sync_limit_flag < 10) &&
			(vlock_state >= VLOCK_STATE_ENABLE_STEP1_DONE))
			vlock_sync_limit_flag++;
		if ((vlock_sync_limit_flag == 5) &&
			(vlock_state == VLOCK_STATE_ENABLE_STEP1_DONE)) {
			/*input_vs_cnt =*/
			/*READ_VPP_REG_BITS(VPU_VLOCK_RO_VS_I_DIST,*/
			/*	0, 28);*/
			input_vs_cnt = XTAL_VLOCK_CLOCK/input_hz;
			WRITE_VPP_REG(VPU_VLOCK_LOOP1_IMISSYNC_MAX,
					input_vs_cnt*125/100);
			WRITE_VPP_REG(VPU_VLOCK_LOOP1_IMISSYNC_MIN,
					input_vs_cnt*70/100);
			/*cal accum1 value*/
			WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 0, 2, 1);
			/*cal accum0 value*/
			WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 0, 5, 1);
			vlock_state = VLOCK_STATE_ENABLE_STEP2_DONE;
		} else if (vlock_dynamic_adjust &&
			(vlock_sync_limit_flag > 5) &&
			(vlock_state == VLOCK_STATE_ENABLE_STEP2_DONE) &&
			(cpu_after_eq(MESON_CPU_MAJOR_ID_GXTVBB)) &&
			(vlock_mode == VLOCK_MODE_MANUAL_PLL)) {
			vlock_enable_step3();
		}
	}
}

void amve_vlock_resume(void)
{
	if ((vlock_en == 0) || (vlock_state ==
		VLOCK_STATE_DISABLE_STEP2_DONE) ||
		(vlock_state == VLOCK_STATE_NULL))
		return;
	if (vlock_state == VLOCK_STATE_DISABLE_STEP1_DONE) {
		vlock_disable_step2();
		return;
	}
	vlock_dis_cnt_no_vf++;
	if ((vlock_state != VLOCK_STATE_DISABLE_STEP2_DONE) &&
		(vlock_dis_cnt_no_vf > vlock_dis_cnt_no_vf_limit)) {
		vlock_disable_step1();
		vlock_dis_cnt_no_vf = 0;
		if (vlock_debug & 0x1)
			pr_info("[%s]auto disable vlock module for no vframe & run disable step1.!!!\n",
				__func__);
	}
	if (vlock_debug & 0x1)
		pr_info("[%s]auto disable vlock module for no vframe!!!\n",
			__func__);
	if (vlock_dynamic_adjust &&
		(vlock_sync_limit_flag > 5) &&
		(vlock_state == VLOCK_STATE_ENABLE_STEP2_DONE) &&
		(cpu_after_eq(MESON_CPU_MAJOR_ID_GXTVBB)) &&
		(vlock_mode == VLOCK_MODE_MANUAL_PLL))
		vlock_enable_step3();
}

void vlock_param_set(unsigned int val, enum vlock_param_e sel)
{
	switch (sel) {
	case VLOCK_EN:
		vlock_en = val;
		break;
	case VLOCK_ADAPT:
		vlock_adapt = val;
		break;
	case VLOCK_MODE:
		vlock_mode = val;
		break;
	case VLOCK_DIS_CNT_LIMIT:
		vlock_dis_cnt_limit = val;
		break;
	case VLOCK_DELTA_LIMIT:
		vlock_delta_limit = val;
		break;
	case VLOCK_DEBUG:
		vlock_debug = val;
		break;
	case VLOCK_DYNAMIC_ADJUST:
		vlock_dynamic_adjust = val;
		break;
	case VLOCK_DIS_CNT_NO_VF_LIMIT:
		vlock_dis_cnt_no_vf_limit = val;
		break;
	default:
		pr_info("%s:unknown vlock param:%d\n", __func__, sel);
		break;
	}

}
void vlock_status(void)
{
	pr_info("\n current vlock parameters status:\n");
	pr_info("vlock_mode:%d\n", vlock_mode);
	pr_info("vlock_en:%d\n", vlock_en);
	pr_info("vlock_adapt:%d\n", vlock_adapt);
	pr_info("vlock_dis_cnt_limit:%d\n", vlock_dis_cnt_limit);
	pr_info("vlock_delta_limit:%d\n", vlock_delta_limit);
	pr_info("vlock_debug:0x%x\n", vlock_debug);
	pr_info("vlock_dynamic_adjust:%d\n", vlock_dynamic_adjust);
	pr_info("vlock_state:%d\n", vlock_state);
	pr_info("vlock_sync_limit_flag:%d\n", vlock_sync_limit_flag);
	pr_info("pre_vmode:%d\n", pre_vmode);
	pr_info("pre_hiu_reg_m:0x%x\n", pre_hiu_reg_m);
	pr_info("pre_hiu_reg_frac:0x%x\n", pre_hiu_reg_frac);
	pr_info("vlock_dis_cnt:%d\n", vlock_dis_cnt);
	pr_info("pre_vout_mode:%s\n", pre_vout_mode);
	pr_info("vlock_dis_cnt_no_vf:%d\n", vlock_dis_cnt_no_vf);
	pr_info("vlock_dis_cnt_no_vf_limit:%d\n", vlock_dis_cnt_no_vf_limit);
}
void vlock_reg_dump(void)
{
	unsigned int addr;

	pr_info("----dump vlock reg----\n");
	for (addr = (0x3000); addr <= (0x3020); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
			(0xd0100000+(addr<<2)), addr,
			READ_VPP_REG(addr));
}
/*video lock end*/

/* sharpness process begin */
void sharpness_process(struct vframe_s *vf)
{
	return;
}
/* sharpness process end */

/*for gxbbtv rgb contrast adj in vd1 matrix */
void vpp_vd1_mtx_rgb_contrast(signed int cont_val, struct vframe_s *vf)
{
	unsigned int vd1_contrast;
	unsigned int con_minus_value, rgb_con_en;

	if ((cont_val > 1023) || (cont_val < -1024))
		return;
	cont_val = cont_val + 1024;
	/*close rgb contrast protect*/
	WRITE_VPP_REG_BITS(XVYCC_VD1_RGB_CTRST, 0, 0, 1);
	/*VPP_VADJ_CTRL bit 1 on for rgb contrast adj*/
	rgb_con_en = READ_VPP_REG_BITS(XVYCC_VD1_RGB_CTRST, 1, 1);
	if (!rgb_con_en)
		WRITE_VPP_REG_BITS(XVYCC_VD1_RGB_CTRST, 1, 1, 1);

	/*select full or limit range setting*/
	con_minus_value = READ_VPP_REG_BITS(XVYCC_VD1_RGB_CTRST, 4, 10);
	if (vf->source_type == VFRAME_SOURCE_TYPE_OTHERS) {
		if (con_minus_value != 64)
			WRITE_VPP_REG_BITS(XVYCC_VD1_RGB_CTRST, 64, 4, 10);
	} else {
		if (con_minus_value != 0)
			WRITE_VPP_REG_BITS(XVYCC_VD1_RGB_CTRST, 0, 4, 10);
	}

	vd1_contrast = (READ_VPP_REG(XVYCC_VD1_RGB_CTRST) & 0xf000ffff) |
					(cont_val << 16);

	WRITE_VPP_REG(XVYCC_VD1_RGB_CTRST, vd1_contrast);
}

/*for gxbbtv contrast adj in vadj1*/
void vpp_vd_adj1_contrast(signed int cont_val, struct vframe_s *vf)
{
	unsigned int vd1_contrast;
	unsigned int vdj1_ctl;

	if ((cont_val > 1023) || (cont_val < -1024))
		return;
	cont_val = ((cont_val + 1024) >> 3);
	/*VPP_VADJ_CTRL bit 1 off for contrast adj*/
	vdj1_ctl = READ_VPP_REG_BITS(VPP_VADJ_CTRL, 1, 1);
	if (is_meson_gxtvbb_cpu()) {
		if (vf->source_type == VFRAME_SOURCE_TYPE_OTHERS) {
			if (!vdj1_ctl)
				WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, 1, 1, 1);
		} else {
			if (vdj1_ctl)
				WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, 0, 1, 1);
		}
	}
	if (get_cpu_type() > MESON_CPU_MAJOR_ID_GXTVBB) {
		vd1_contrast = (READ_VPP_REG(VPP_VADJ1_Y) & 0x3ff00) |
						(cont_val << 0);
	} else {
		vd1_contrast = (READ_VPP_REG(VPP_VADJ1_Y) & 0x1ff00) |
						(cont_val << 0);
	}
	WRITE_VPP_REG(VPP_VADJ1_Y, vd1_contrast);
}

void vpp_vd_adj1_brightness(signed int bri_val, struct vframe_s *vf)
{
	signed int vd1_brightness;

	signed int ao0 =  -64;
	signed int ao1 = -512;
	signed int ao2 = -512;
	unsigned int a01 =    0, a_2 =    0;
	/* enable vd0_csc */

	unsigned int ori = READ_VPP_REG(VPP_MATRIX_CTRL) | 0x00000020;
	/* point to vd0_csc */
	unsigned int ctl = (ori & 0xfffffcff) | 0x00000100;

	if (bri_val > 1023 || bri_val < -1024)
		return;

	if (get_cpu_type() > MESON_CPU_MAJOR_ID_GXTVBB) {
		bri_val = bri_val >> 1;
		vd1_brightness = (READ_VPP_REG(VPP_VADJ1_Y) & 0xff) |
			(bri_val << 8);

		WRITE_VPP_REG(VPP_VADJ1_Y, vd1_brightness);
	} else {
		if ((vf->source_type == VFRAME_SOURCE_TYPE_TUNER) ||
			(vf->source_type == VFRAME_SOURCE_TYPE_CVBS) ||
			(vf->source_type == VFRAME_SOURCE_TYPE_COMP))
			vd1_brightness = bri_val;
		else if (vf->source_type == VFRAME_SOURCE_TYPE_HDMI) {
			if ((((vf->signal_type >> 29) & 0x1) == 1) &&
				(((vf->signal_type >> 16) & 0xff) == 9)) {
				bri_val += ao0;
				if (bri_val < -1024)
					bri_val = -1024;
				vd1_brightness = bri_val;
			} else
				vd1_brightness = bri_val;
		} else {
			bri_val += ao0;
			if (bri_val < -1024)
				bri_val = -1024;
			vd1_brightness = bri_val;
		}

		a01 = ((vd1_brightness << 16) & 0x0fff0000) |
				((ao1 <<  0) & 0x00000fff);
		a_2 = ((ao2 <<	0) & 0x00000fff);
		/*p01 = ((po0 << 16) & 0x0fff0000) |*/
				/*((po1 <<  0) & 0x00000fff);*/
		/*p_2 = ((po2 <<	0) & 0x00000fff);*/

		WRITE_VPP_REG(VPP_MATRIX_CTRL, ctl);
		WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET0_1, a01);
		WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET2, a_2);
		WRITE_VPP_REG(VPP_MATRIX_CTRL, ori);
	}
}


/* brightness/contrast adjust process begin */
static void vd1_brightness_contrast(signed int brightness,
		signed int contrast)
{
	signed int ao0 =  -64, g00 = 1024, g01 =    0, g02 =    0, po0 =  64;
	signed int ao1 = -512, g10 =    0, g11 = 1024, g12 =    0, po1 = 512;
	signed int ao2 = -512, g20 =    0, g21 =    0, g22 = 1024, po2 = 512;
	unsigned int gc0 =    0, gc1 =    0, gc2 =    0, gc3 =    0, gc4 = 0;
	unsigned int a01 =    0, a_2 =    0, p01 =    0, p_2 =    0;
	/* enable vd0_csc */
	unsigned int ori = READ_VPP_REG(VPP_MATRIX_CTRL) | 0x00000020;
	/* point to vd0_csc */
	unsigned int ctl = (ori & 0xfffffcff) | 0x00000100;

	po0 += brightness >> 1;
	if (po0 >  1023)
		po0 =  1023;
	if (po0 < -1024)
		po0 = -1024;
	g00  *= contrast + 2048;
	g00 >>= 11;
	if (g00 >  4095)
		g00 =  4095;
	if (g00 < -4096)
		g00 = -4096;
	if (contrast < 0) {
		g11  *= contrast   + 2048;
		g11 >>= 11;
	}
	if (brightness < 0) {
		g11  += brightness >> 1;
		if (g11 >  4095)
			g11 =  4095;
		if (g11 < -4096)
			g11 = -4096;
	}
	if (contrast < 0) {
		g22  *= contrast   + 2048;
		g22 >>= 11;
	}
	if (brightness < 0) {
		g22  += brightness >> 1;
		if (g22 >  4095)
			g22 =  4095;
		if (g22 < -4096)
			g22 = -4096;
	}
	gc0 = ((g00 << 16) & 0x1fff0000) | ((g01 <<  0) & 0x00001fff);
	gc1 = ((g02 << 16) & 0x1fff0000) | ((g10 <<  0) & 0x00001fff);
	gc2 = ((g11 << 16) & 0x1fff0000) | ((g12 <<  0) & 0x00001fff);
	gc3 = ((g20 << 16) & 0x1fff0000) | ((g21 <<  0) & 0x00001fff);
	gc4 = ((g22 <<  0) & 0x00001fff);
	/* #if (MESON_CPU_TYPE >= MESON_CPU_TYPE_MESONG9TV) */
	if (is_meson_gxtvbb_cpu()) {
		a01 = ((ao0 << 16) & 0x0fff0000) |
				((ao1 <<  0) & 0x00000fff);
		a_2 = ((ao2 <<  0) & 0x00000fff);
		p01 = ((po0 << 16) & 0x0fff0000) |
				((po1 <<  0) & 0x00000fff);
		p_2 = ((po2 <<  0) & 0x00000fff);
	} else {
		/* #else */
		a01 = ((ao0 << 16) & 0x07ff0000) |
				((ao1 <<  0) & 0x000007ff);
		a_2 = ((ao2 <<  0) & 0x000007ff);
		p01 = ((po0 << 16) & 0x07ff0000) |
				((po1 <<  0) & 0x000007ff);
		p_2 = ((po2 <<  0) & 0x000007ff);
	}
	/* #endif */
	WRITE_VPP_REG(VPP_MATRIX_CTRL, ctl);
	WRITE_VPP_REG(VPP_MATRIX_COEF00_01, gc0);
	WRITE_VPP_REG(VPP_MATRIX_COEF02_10, gc1);
	WRITE_VPP_REG(VPP_MATRIX_COEF11_12, gc2);
	WRITE_VPP_REG(VPP_MATRIX_COEF20_21, gc3);
	WRITE_VPP_REG(VPP_MATRIX_COEF22, gc4);
	WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET0_1, a01);
	WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET2, a_2);
	WRITE_VPP_REG(VPP_MATRIX_OFFSET0_1, p01);
	WRITE_VPP_REG(VPP_MATRIX_OFFSET2, p_2);
	WRITE_VPP_REG(VPP_MATRIX_CTRL, ori);
}

void amvecm_bricon_process(signed int bri_val,
		signed int cont_val, struct vframe_s *vf)
{
	if (vecm_latch_flag & FLAG_VADJ1_BRI) {
		vecm_latch_flag &= ~FLAG_VADJ1_BRI;
		vpp_vd_adj1_brightness(bri_val, vf);
		pr_amve_dbg("\n[amve..] set vd1_brightness OK!!!\n");
		if (dnlp_debug&0x100)
			pr_info("\n[amve..]%s :brightness:%d!!!\n",
				__func__, bri_val);
	}

	if (vecm_latch_flag & FLAG_VADJ1_CON) {
		vecm_latch_flag &= ~FLAG_VADJ1_CON;
		if (contrast_adj_sel)
			vpp_vd1_mtx_rgb_contrast(cont_val, vf);
		else
			vpp_vd_adj1_contrast(cont_val, vf);
		pr_amve_dbg("\n[amve..] set vd1_contrast OK!!!\n");
		if (dnlp_debug&0x100)
			pr_info("\n[amve..]%s :contrast:%d!!!\n",
				__func__, cont_val);
	}

	if (0) { /* vecm_latch_flag & FLAG_BRI_CON) { */
		vecm_latch_flag &= ~FLAG_BRI_CON;
		vd1_brightness_contrast(bri_val, cont_val);
		pr_amve_dbg("\n[amve..] set vd1_brightness_contrast OK!!!\n");
	}
}
/* brightness/contrast adjust process end */

void amvecm_color_process(signed int sat_val,
		signed int hue_val, struct vframe_s *vf)
{
	if (vecm_latch_flag & FLAG_VADJ1_COLOR) {
		vecm_latch_flag &= ~FLAG_VADJ1_COLOR;
		vpp_vd_adj1_saturation_hue(sat_val, hue_val, vf);
		if (dnlp_debug&0x100)
			pr_info("\n[amve..]%s :saturation:%d,hue:%d!!!\n",
				__func__, sat_val, hue_val);
	}
}
/* saturation/hue adjust process end */

/* 3d process begin */
void amvecm_3d_black_process(void)
{
	if (vecm_latch_flag & FLAG_3D_BLACK_DIS) {
		/* disable reg_3dsync_enable */
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 0, 31, 1);
		WRITE_VPP_REG_BITS(VIU_MISC_CTRL0, 0, 8, 1);
		WRITE_VPP_REG_BITS(VPP_BLEND_ONECOLOR_CTRL, 0, 26, 1);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 0, 13, 1);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC2, 0, 31, 1);
		vecm_latch_flag &= ~FLAG_3D_BLACK_DIS;
	}
	if (vecm_latch_flag & FLAG_3D_BLACK_EN) {
		WRITE_VPP_REG_BITS(VIU_MISC_CTRL0, 1, 8, 1);
		WRITE_VPP_REG_BITS(VPP_BLEND_ONECOLOR_CTRL, 1, 26, 1);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC2, 1, 31, 1);
		WRITE_VPP_REG_BITS(VPP_BLEND_ONECOLOR_CTRL,
				sync_3d_black_color&0xffffff, 0, 24);
		if (sync_3d_sync_to_vbo)
			WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 1, 13, 1);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 1, 31, 1);
		vecm_latch_flag &= ~FLAG_3D_BLACK_EN;
	}
}
void amvecm_3d_sync_process(void)
{

	if (vecm_latch_flag & FLAG_3D_SYNC_DIS) {
		/* disable reg_3dsync_enable */
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 0, 31, 1);
		vecm_latch_flag &= ~FLAG_3D_SYNC_DIS;
	}
	if (vecm_latch_flag & FLAG_3D_SYNC_EN) {
		/*select vpu pwm source clock*/
		switch (READ_VPP_REG_BITS(VPU_VIU_VENC_MUX_CTRL, 0, 2)) {
		case 0:/* ENCL */
			WRITE_VPP_REG_BITS(VPU_VPU_PWM_V0, 0, 29, 2);
			break;
		case 1:/* ENCI */
			WRITE_VPP_REG_BITS(VPU_VPU_PWM_V0, 1, 29, 2);
			break;
		case 2:/* ENCP */
			WRITE_VPP_REG_BITS(VPU_VPU_PWM_V0, 2, 29, 2);
			break;
		case 3:/* ENCT */
			WRITE_VPP_REG_BITS(VPU_VPU_PWM_V0, 3, 29, 2);
			break;
		default:
			break;
		}
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC2, sync_3d_h_start, 0, 13);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC2, sync_3d_h_end, 16, 13);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_v_start, 0, 13);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_v_end, 16, 13);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_polarity, 29, 1);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_out_inv, 15, 1);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 1, 31, 1);
		vecm_latch_flag &= ~FLAG_3D_SYNC_EN;
	}
}
/* 3d process end */

/*gxlx sr adaptive param*/
#define SR_SD_SCALE_LEVEL 0x1
#define SR_HD_SCALE_LEVEL 0x2
#define SR_4k_LEVEL 0x4
#define SR_CVBS_LEVEL 0x8
#define SR_NOSCALE_LEVEL 0x10
static void amve_sr_reg_setting(unsigned int adaptive_level)
{
	if (adaptive_level & SR_SD_SCALE_LEVEL)
		am_set_regmap(&sr1reg_sd_scale);
	else if (adaptive_level & SR_HD_SCALE_LEVEL)
		am_set_regmap(&sr1reg_hd_scale);
	else if (adaptive_level & SR_4k_LEVEL) {
		amvecm_sharpness_enable(1);/*peaking*/
		amvecm_sharpness_enable(3);/*lcti*/
		amvecm_sharpness_enable(5);/*drtlpf theta*/
		amvecm_sharpness_enable(7);/*debanding*/
		amvecm_sharpness_enable(9);/*dejaggy*/
		amvecm_sharpness_enable(11);/*dering*/
		amvecm_sharpness_enable(13);/*drlpf*/
	} else if (adaptive_level & SR_CVBS_LEVEL)
		am_set_regmap(&sr1reg_cvbs);
	else if (adaptive_level & SR_NOSCALE_LEVEL)
		am_set_regmap(&sr1reg_hv_noscale);
}
void amve_sharpness_adaptive_setting(struct vframe_s *vf,
	 unsigned int sps_h_en, unsigned int sps_v_en)
{
	static unsigned int adaptive_level = 1;
	unsigned int cur_level;
	unsigned int width, height;
	struct vinfo_s *vinfo = get_current_vinfo();

	if (vf == NULL)
		return;
	if (vinfo->mode == VMODE_CVBS)
		cur_level = SR_CVBS_LEVEL;
	else {
		width = (vf->type & VIDTYPE_COMPRESS) ?
			vf->compWidth : vf->width;
		height = (vf->type & VIDTYPE_COMPRESS) ?
			vf->compHeight : vf->height;

		if ((sps_h_en == 1) && (sps_v_en == 1)) {
			/*super scaler h and v scale up x2 */
			if ((height >= 2160) && (width >= 3840))
				cur_level = SR_4k_LEVEL;
			else if ((height >= 720) && (width >= 1280))
				cur_level = SR_HD_SCALE_LEVEL;
			else
				cur_level = SR_SD_SCALE_LEVEL;
		} else {
			/*1. super scaler no up scale */
			/*2. super scaler h up scale x2*/
			if ((height >= 2160) && (width >= 3840))
				cur_level = SR_4k_LEVEL;
			else
				cur_level = SR_NOSCALE_LEVEL;
		}
	}

	if (adaptive_level == cur_level)
		return;

	amve_sr_reg_setting(cur_level);
	adaptive_level = cur_level;
	sr_adapt_level = adaptive_level;
	pr_amve_dbg("\n[amcm..]sr_adapt_level = %d :1->sd;2->hd;4->4k\n",
			sr_adapt_level);

}
/*gxlx sr init*/
void amve_sharpness_init(void)
{
	am_set_regmap(&sr1reg_sd_scale);
}
