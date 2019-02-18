/*
 * drivers/amlogic/media/enhancement/amvecm/dnlp_algorithm/dnlp_alg.h
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

#ifndef __AM_DNLP_AGL_H
#define __AM_DNLP_AGL_H

struct param_for_dnlp_s {
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

struct dnlp_alg_input_param_s {
	unsigned int *pre_1_gamma;
	unsigned int *pre_0_gamma;
	unsigned int *nTstCnt;
	unsigned int *ve_dnlp_luma_sum;
	int *RBASE;
	bool *menu_chg_en;
};

struct dnlp_alg_output_param_s {
	unsigned char *ve_dnlp_tgt;
};

struct dnlp_dbg_ro_param_s {
	int *ro_luma_avg4;
	int *ro_var_d8;
	int *ro_scurv_gain;
	int *ro_blk_wht_ext0;
	int *ro_blk_wht_ext1;
	int *ro_dnlp_brightness;
	int *GmScurve;
	int *clash_curve;
	int *clsh_scvbld;
	int *blkwht_ebld;
};

struct dnlp_dbg_rw_param_s {
	int *dnlp_scurv_low;
	int *dnlp_scurv_mid1;
	int *dnlp_scurv_mid2;
	int *dnlp_scurv_hgh1;
	int *dnlp_scurv_hgh2;
	int *gain_var_lut49;
	int *wext_gain;
};

struct dnlp_dbg_print_s {
	int *dnlp_printk;
};

struct dnlp_alg_s {
	void (*dnlp_algorithm_main)(unsigned int raw_hst_sum);
	void (*dnlp_para_set)(struct dnlp_alg_output_param_s **dnlp_output,
			struct dnlp_alg_input_param_s **dnlp_input,
			struct dnlp_dbg_rw_param_s **rw_param,
			struct dnlp_dbg_ro_param_s **ro_param,
			struct param_for_dnlp_s **rw_node,
			struct dnlp_dbg_print_s **dbg_print);
	void (*dnlp3_param_refrsh)(void);
};
extern struct dnlp_alg_s *dnlp_alg_init(struct dnlp_alg_s **dnlp_alg);
extern struct dnlp_alg_s *dnlp_alg_function;
#endif
