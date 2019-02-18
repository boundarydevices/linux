/*
 * drivers/amlogic/media/enhancement/amvecm/dnlp_cal.h
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

#ifndef __AM_DNLP_CAL_H
#define __AM_DNLP_CAL_H

struct dnlp_alg_param_s {
	unsigned int dnlp_alg_enable;
	unsigned int dnlp_respond;
	unsigned int dnlp_sel;
	unsigned int dnlp_respond_flag;
	unsigned int dnlp_smhist_ck;
	unsigned int dnlp_mvreflsh;
	unsigned int dnlp_pavg_btsft;
	unsigned int dnlp_dbg_i2r;
	unsigned int dnlp_cuvbld_min;
	unsigned int dnlp_cuvbld_max;
	unsigned int dnlp_schg_sft;
	unsigned int dnlp_bbd_ratio_low;
	unsigned int dnlp_bbd_ratio_hig;
	unsigned int dnlp_limit_rng;
	unsigned int dnlp_range_det;
	unsigned int dnlp_blk_cctr;
	unsigned int dnlp_brgt_ctrl;
	unsigned int dnlp_brgt_range;
	unsigned int dnlp_brght_add;
	unsigned int dnlp_brght_max;
	unsigned int dnlp_dbg_adjavg;
	unsigned int dnlp_auto_rng;
	unsigned int dnlp_lowrange;
	unsigned int dnlp_hghrange;
	unsigned int dnlp_satur_rat;
	unsigned int dnlp_satur_max;
	unsigned int dnlp_set_saturtn;
	unsigned int dnlp_sbgnbnd;
	unsigned int dnlp_sendbnd;
	unsigned int dnlp_clashBgn;
	unsigned int dnlp_clashEnd;
	unsigned int dnlp_var_th;
	unsigned int dnlp_clahe_gain_neg;
	unsigned int dnlp_clahe_gain_pos;
	unsigned int dnlp_clahe_gain_delta;
	unsigned int dnlp_mtdbld_rate;
	unsigned int dnlp_adpmtd_lbnd;
	unsigned int dnlp_adpmtd_hbnd;
	unsigned int dnlp_blkext_ofst;
	unsigned int dnlp_whtext_ofst;
	unsigned int dnlp_blkext_rate;
	unsigned int dnlp_whtext_rate;
	unsigned int dnlp_bwext_div4x_min;
	unsigned int dnlp_iRgnBgn;
	unsigned int dnlp_iRgnEnd;
	unsigned int dnlp_dbg_map;
	unsigned int dnlp_final_gain;
	unsigned int dnlp_cliprate_v3;
	unsigned int dnlp_cliprate_min;
	unsigned int dnlp_adpcrat_lbnd;
	unsigned int dnlp_adpcrat_hbnd;
	unsigned int dnlp_scurv_low_th;
	unsigned int dnlp_scurv_mid1_th;
	unsigned int dnlp_scurv_mid2_th;
	unsigned int dnlp_scurv_hgh1_th;
	unsigned int dnlp_scurv_hgh2_th;
	unsigned int dnlp_mtdrate_adp_en;
};

struct dnlp_parse_cmd_s {
	char *parse_string;
	unsigned int *value;
};

extern struct ve_dnlp_table_s am_ve_new_dnlp;
extern struct ve_dnlp_curve_param_s dnlp_curve_param_load;
extern unsigned int ve_dnlp_rt;
extern bool ve_en;
extern unsigned int ve_dnlp_rt;
extern unsigned int ve_dnlp_luma_sum;
extern ulong ve_dnlp_lpf[64];
extern ulong ve_dnlp_reg[16];
extern ulong ve_dnlp_reg_v2[32];
extern ulong ve_dnlp_reg_def[16];
extern struct dnlp_parse_cmd_s dnlp_parse_cmd[];

extern int ve_dnlp_calculate_tgtx(struct vframe_s *vf);
extern void ve_set_v3_dnlp(struct ve_dnlp_curve_param_s *p);
extern void ve_dnlp_calculate_lpf(void);
extern void ve_dnlp_calculate_reg(void);
extern void dnlp_alg_param_init(void);

extern int *dnlp_scurv_low_copy;
extern int *dnlp_scurv_mid1_copy;
extern int *dnlp_scurv_mid2_copy;
extern int *dnlp_scurv_hgh1_copy;
extern int *dnlp_scurv_hgh2_copy;
extern int *gain_var_lut49_copy;
extern int *wext_gain_copy;

extern int *ro_luma_avg4_copy;
extern int *ro_var_d8_copy;
extern int *ro_scurv_gain_copy;
extern int *ro_blk_wht_ext0_copy;
extern int *ro_blk_wht_ext1_copy;
extern int *ro_dnlp_brightness_copy;
extern int *GmScurve_copy;
extern int *clash_curve_copy;
extern int *clsh_scvbld_copy;
extern int *blkwht_ebld_copy;

extern unsigned int *ve_dnlp_luma_sum_copy;
extern unsigned char *ve_dnlp_tgt_copy;

extern void dnlp_dbg_node_copy(void);
extern bool dnlp_insmod_ok; /*0:fail, 1:ok*/
extern int *dnlp_printk_copy;
#endif


