
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
extern unsigned char ve_dnlp_tgt[65];
extern int GmScurve[65];
extern int clash_curve[65];
extern int clsh_scvbld[65];
extern int blkwht_ebld[65];
extern bool ve_en;
extern unsigned int ve_dnlp_rt;
extern unsigned int ve_dnlp_luma_sum;
extern ulong ve_dnlp_lpf[64];
extern ulong ve_dnlp_reg[16];
extern ulong ve_dnlp_reg_def[16];
extern struct dnlp_parse_cmd_s dnlp_parse_cmd[];

extern int dnlp_scurv_low[65];
extern int dnlp_scurv_mid1[65];
extern int dnlp_scurv_mid2[65];
extern int dnlp_scurv_hgh1[65];
extern int dnlp_scurv_hgh2[65];
extern int gain_var_lut49[49];
extern int wext_gain[48];

extern int ro_luma_avg4;
extern int ro_var_d8;
extern int ro_scurv_gain;
extern int ro_blk_wht_ext0;
extern int ro_blk_wht_ext1;
extern int ro_dnlp_brightness;

extern void ve_dnlp_calculate_tgtx_v3(struct vframe_s *vf);
extern void ve_set_v3_dnlp(struct ve_dnlp_curve_param_s *p);
extern void ve_dnlp_calculate_lpf(void);
extern void ve_dnlp_calculate_reg(void);
extern void dnlp_alg_param_init(void);
#endif


