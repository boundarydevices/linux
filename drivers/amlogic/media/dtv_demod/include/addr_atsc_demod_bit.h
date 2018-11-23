/*
 * drivers/amlogic/media/dtv_demod/include/addr_atsc_demod_bit.h
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

#ifndef __ADDR_ATSC_DEMOD_BIT_H__
#define __ADDR_ATSC_DEMOD_BIT_H__

struct	ATSC_DEMOD_REG_0X52_BITS {
	unsigned int	lpf_crgn_3                     :5,
	reserved0                      :3,
	lpf_crgn_4                     :5,
	reserved1                      :3,
	lpf_crgn_5                     :5,
	reserved2                      :3,
	lpf_crgn_6                     :5,
	reserved3                      :3;
};
struct	ATSC_DEMOD_REG_0X53_BITS {
	unsigned int	lpf_crgn_0                     :5,
	reserved4                      :3,
	lpf_crgn_1                     :5,
	reserved5                      :3,
	lpf_crgn_2                     :5,
	reserved6                      :11;
};

union ATSC_DEMOD_REG_0X54_BITS {
	unsigned int bits;
	struct {
	unsigned int cr_rate :23,
	reserved7 :9;
	} b;
};
union ATSC_DEMOD_REG_0X55_BITS {
	unsigned int bits;
	struct {
	unsigned int	ck_rate                        :24,
	reserved8                      :8;
	} b;
};
struct	ATSC_DEMOD_REG_0X56_BITS {
	unsigned int	dc_frz                         :1,
	fagc_frz                       :1,
	inv_spectrum                   :1,
	qam_vsr_frz                    :1,
	ck_rate_frz                    :1,
	no_sym_en_ctrl                 :1,
	cr_open_loop                   :1,
	qam_vsr_sel                    :1,
	dc_rmv_bw                      :4,
	reserved9                      :20;
};
struct	ATSC_DEMOD_REG_0X58_BITS {
	unsigned int	fagc_bw                        :4,
	reserved10                     :4,
	fagc_ref_level                 :20,
	fagc_ref_level_sel             :1,
	reserved11                     :3;
};
struct	ATSC_DEMOD_REG_0X59_BITS {
	unsigned int	fs_dly_num                     :10,
	reserved12                     :22;
};
struct	ATSC_DEMOD_REG_0X5A_BITS {
	unsigned int	pk512_th                       :12,
	reserved13                     :4,
	prescale                       :3,
	reserved14                     :13;
};
struct	ATSC_DEMOD_REG_0X5B_BITS {
	unsigned int	fagc_fix_gain                  :12,
	reserved15                     :4,
	fcorv_th                       :8,
	reserved16                     :8;
};
struct	ATSC_DEMOD_REG_0X5C_BITS {
	unsigned int	m1m2rate_1                     :8,
	m1m2rate_2                     :8,
	m1m2rate_3                     :8,
	m1m2rate_4                     :8;
};
union	ATSC_DEMOD_REG_0X5D_BITS {
	unsigned int bits;
	struct	 {
		unsigned int	fld_conf_cnt                   :8,
		clk_conf_cnt                   :8,
		clk_conf_cnt_limit             :8;
	} b;
};
struct	ATSC_DEMOD_REG_0X5E_BITS {
	unsigned int	balance_track_bw               :8,
	balance_track_dir              :1,
	reserved17                     :3,
	balance_track                  :3,
	reserved18                     :17;
};
struct	ATSC_DEMOD_REG_0X5F_BITS {
	unsigned int	div_mix_rate                   :4,
	div_mix_en                     :1,
	reserved19                     :27;
};
struct	ATSC_DEMOD_REG_0X60_BITS {
	unsigned int	cr_stepsize                    :4,
	cr_dc_sum_cnt                  :4,
	cr_enable                      :1,
	reserved20                     :23;
};
union ATSC_DEMOD_REG_0X61_BITS {
	unsigned int bits;
	struct	 {
		unsigned int	cr_bin_freq1                   :11,
		reserved21                     :5,
		cr_bin_freq2                   :11,
		reserved22                     :5;
	} b;
};
struct	ATSC_DEMOD_REG_0X62_BITS {
	unsigned int	coef0                          :6,
	reserved23                     :2,
	coef1                          :6,
	reserved24                     :2,
	coef2                          :6,
	reserved25                     :2,
	coef3                          :6,
	reserved26                     :2;
};
struct	ATSC_DEMOD_REG_0X63_BITS {
	unsigned int	coef4                          :6,
	reserved27                     :2,
	coef5                          :6,
	reserved28                     :2,
	coef6                          :6,
	reserved29                     :2,
	coef7                          :6,
	reserved30                     :2;
};
struct	ATSC_DEMOD_REG_0X64_BITS {
	unsigned int	coef8                          :6,
	reserved31                     :2,
	coef9                          :6,
	reserved32                     :2,
	coef10                         :6,
	reserved33                     :2,
	coef11                         :6,
	reserved34                     :2;
};
struct	ATSC_DEMOD_REG_0X65_BITS {
	unsigned int	coef12                         :6,
	reserved35                     :2,
	coef13                         :7,
	reserved36                     :1,
	coef14                         :7,
	reserved37                     :1,
	coef15                         :8;
};
struct	ATSC_DEMOD_REG_0X66_BITS {
	unsigned int	coef16                         :8,
	reserved38                     :24;
};
struct	ATSC_DEMOD_REG_0X67_BITS {
	unsigned int	coef17                         :9,
	reserved39                     :7,
	coef18                         :9,
	reserved40                     :7;
};
struct	ATSC_DEMOD_REG_0X68_BITS {
	unsigned int	coef19                         :10,
	reserved41                     :6,
	coef20                         :10,
	reserved42                     :6;
};
struct	ATSC_DEMOD_REG_0X69_BITS {
	unsigned int	cf_rnd_bits                    :2,
	reserved43                     :30;
};
union	ATSC_DEMOD_REG_0X6A_BITS {
	unsigned int bits;
	struct{
		unsigned int	peak_thd                       :4,
		cfo_alpha                      :4,
		ccfo_intv2                     :6,
		reserved44                     :2,
		ccfo_intv1                     :6,
		reserved45                     :2,
		ccfo_intv0                     :6,
		ccfo_do_disable                :1,
		ccfo_enable                    :1;
	} b;
};
struct	ATSC_DEMOD_REG_0X6B_BITS {
	unsigned int	ccfo_freq_coef1                :10,
	reserved46                     :6,
	ccfo_freq_coef2                :10,
	reserved47                     :6;
};
struct	ATSC_DEMOD_REG_0X6C_BITS {
	unsigned int	ccfo_freq_coef3                :10,
	reserved48                     :6,
	ccfo_freq_coef4                :10,
	reserved49                     :6;
};
struct	ATSC_DEMOD_REG_0X6D_BITS {
	unsigned int	ccfo_fs                        :10,
	reserved50                     :22;
};
union ATSC_DEMOD_REG_0X6E_BITS {
	unsigned int bits;
	struct {
	unsigned int	adc_fs                         :26,
	reserved51                     :6;
	} b;
};
struct	ATSC_DEMOD_REG_0X70_BITS {
	unsigned int	gain_step                      :8,
	gain_step_er                   :8,
	soft_gain_in                   :12,
	reserved52                     :4;
};
struct	ATSC_DEMOD_REG_0X71_BITS {
	unsigned int	dagc_mode                      :2,
	reserved53                     :2,
	dagc_power_alpha               :2,
	reserved54                     :2,
	dagc_out_mux                   :1,
	reserved55                     :23;
};
union ATSC_DEMOD_REG_0X72_BITS {
	unsigned int bits;
	struct	 {
		unsigned int	avg_power                      :8,
		gain_result                    :12,
		dagc_ready                     :1,
		reserved56                     :11;
	} b;
};
struct	ATSC_DEMOD_REG_0X73_BITS {
	unsigned int	cr_err_lpf1                    :24,
	reserved57                     :8;
};
struct	ATSC_DEMOD_REG_0X74_BITS {
	unsigned int	cr_err_lpf2                    :19,
	reserved58                     :13;
};
struct	ATSC_DEMOD_REG_0X75_BITS {
	unsigned int	clk_offset                     :21,
	reserved59                     :11;
};
struct	ATSC_DEMOD_REG_0X76_BITS {
	unsigned int	agc_sat1                       :1,
	reserved60                     :3,
	agc_sat2                       :1,
	reserved61                     :27;
};
struct	ATSC_DEMOD_REG_0X77_BITS {
	unsigned int	i_dc_out1                      :12,
	reserved62                     :4,
	q_dc_out1                      :12,
	reserved63                     :4;
};
struct	ATSC_DEMOD_REG_0X78_BITS {
	unsigned int	i_dc_out2                      :12,
	reserved64                     :4,
	q_dc_out2                      :12,
	reserved65                     :4;
};
struct	ATSC_DEMOD_REG_0X79_BITS {
	unsigned int	cr_avg_cw1_i                   :12,
	reserved66                     :4,
	cr_avg_cw1_q                   :12,
	reserved67                     :4;
};
struct	ATSC_DEMOD_REG_0X7A_BITS {
	unsigned int	cr_avg_cw2_i                   :12,
	reserved68                     :4,
	cr_avg_cw2_q                   :12,
	reserved69                     :4;
};
struct	ATSC_DEMOD_REG_0X7B_BITS {
	unsigned int	phase_shift                    :23,
	reserved70                     :1,
	qam_phase_err_ctrl             :2,
	reserved71                     :6;
};
struct	ATSC_DEMOD_REG_0X7C_BITS {
	unsigned int	cr_ofst_gna                    :15,
	reserved72                     :17;
};
struct	ATSC_DEMOD_REG_0X7D_BITS {
	unsigned int	cr_ofst_a2                     :17,
	reserved73                     :15;
};
struct	ATSC_DEMOD_REG_0X7E_BITS {
	unsigned int	cr_ofst_a1                     :18,
	reserved74                     :14;
};
struct	ATSC_DEMOD_REG_0X7F_BITS {
	unsigned int	ru_ofst_gna                    :15,
	reserved75                     :17;
};
struct	ATSC_DEMOD_REG_0X80_BITS {
	unsigned int	ru_ofst_a2                     :17,
	reserved76                     :15;
};
struct	ATSC_DEMOD_REG_0X81_BITS {
	unsigned int	ru_ofst_a1                     :18,
	reserved77                     :14;
};
struct	ATSC_DEMOD_REG_0X82_BITS {
	unsigned int	fagc_ref_l_eqst0               :8,
	fagc_ref_l_eqst1               :8,
	fagc_ref_l_eqst2               :8,
	fagc_ref_l_eqst3               :8;
};
struct	ATSC_DEMOD_REG_0X83_BITS {
	unsigned int	fagc_ref_l_eqst4               :8,
	fagc_ref_l_eqst5               :8,
	fagc_ref_l_eqst6               :8,
	fagc_ref_l_eqst7               :8;
};
struct	ATSC_DEMOD_REG_0X84_BITS {
	unsigned int	fagc_ref_l_eqst8               :8,
	fagc_ref_l_eqst9               :8,
	reserved78                     :16;
};
struct	ATSC_DEMOD_REG_0X85_BITS {
	unsigned int	fagc_ref_h_eqst0               :8,
	fagc_ref_h_eqst1               :8,
	fagc_ref_h_eqst2               :8,
	fagc_ref_h_eqst3               :8;
};
struct	ATSC_DEMOD_REG_0X86_BITS {
	unsigned int	fagc_ref_h_eqst4               :8,
	fagc_ref_h_eqst5               :8,
	fagc_ref_h_eqst6               :8,
	fagc_ref_h_eqst7               :8;
};
struct	ATSC_DEMOD_REG_0X87_BITS {
	unsigned int	fagc_ref_h_eqst8               :8,
	fagc_ref_h_eqst9               :8,
	reserved79                     :16;
};
union	ATSC_DEMOD_REG_0X88_BITS {
	unsigned int bits;
	struct{
		unsigned int	ccfo_pow_peak                  :16,
		ccfo_avg_peak                  :16;
	} b;
};

#endif
