/*
 * drivers/amlogic/media/dtv_demod/include/addr_atsc_eq_bit.h
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

#ifndef __ADDR_ATSC_EQ_BIT_H__
#define __ADDR_ATSC_EQ_BIT_H__

struct ATSC_EQ_REG_0X90_BITS {
	unsigned int	eq_sm_ow_ctrl                  :10,
	reserved0                      :22;
};
union ATSC_EQ_REG_0X91_BITS {
	unsigned int bits;
	struct {
		unsigned int pn63_err_gen_en            :1,
		reserved1                      :3,
		ffe_out_sft_left_one           :1,
		reserved2                      :3,
		fft_leak_type_sel              :1,
		reserved3                      :3,
		iir_limit_sel                  :1,
		reserved4                      :3,
		iir_tap_rw_en                  :1,
		reserved5                      :3,
		div_path_sel                   :1,
		reserved6                      :3,
		div_en                         :1,
		reserved7                      :3,
		dfe_tap_wr_en                  :1,
		reserved8                      :3;
	} b;
};
union ATSC_EQ_REG_0X92_BITS {
	unsigned int bits;
	struct {
		unsigned int	dec_stage_sel                  :4,
		nco_trk_phs_rnd                :4,
		eq_ph_loop_open                :8,
		dfe_dly_sym_slicer_sel         :3,
		reserved9                      :1,
		err_gen_sel                    :2,
		reserved10                     :2,
		eq_err_rot_en                  :1,
		reserved11                     :5,
		cwrmv_enable                   :1,
		reserved12                     :1;
	} b;
};

union	ATSC_EQ_REG_0X93_BITS {
	unsigned int bits;
	struct {
		unsigned int	eq_out_start_dly               :11,
		reserved13                     :5,
		eq_ffe_symb_thr                :8,
		fir_out_exp                    :5,
		reserved14                     :3;
	} b;
};
struct	ATSC_EQ_REG_0X94_BITS {
	unsigned int	snr_calc_num                   :16,
	err_fft_start_cnt              :16;
};

struct	ATSC_EQ_REG_0X95_BITS {
	unsigned int	stswthup1                      :8,
	stswthup2                      :8,
	stswthup3                      :8,
	stswthup4                      :8;
};
struct	ATSC_EQ_REG_0X96_BITS {
	unsigned int	stswthup5                      :8,
	stswthup6                      :8,
	stswthup7                      :8,
	stswthup8                      :8;
};
struct	ATSC_EQ_REG_0X97_BITS {
	unsigned int	stswthdn1                      :8,
	stswthdn2                      :8,
	stswthdn3                      :8,
	stswthdn4                      :8;
};
struct	ATSC_EQ_REG_0X98_BITS {
	unsigned int	stswthdn5                      :8,
	stswthdn6                      :8,
	stswthdn7                      :8,
	stswthdn8                      :8;
};
struct	ATSC_EQ_REG_0X99_BITS {
	unsigned int	fir_step_1t                    :5,
	reserved15                     :3,
	fir_step_2t                    :5,
	reserved16                     :3,
	fir_step_3t                    :5,
	reserved17                     :3,
	fir_step_4t                    :5,
	reserved18                     :3;
};
struct	ATSC_EQ_REG_0X9A_BITS {
	unsigned int	fir_step_5t                    :5,
	reserved19                     :3,
	fir_step_6t                    :5,
	reserved20                     :3,
	fir_step_7t                    :5,
	reserved21                     :3,
	fir_step_8t                    :5,
	reserved22                     :3;
};
struct	ATSC_EQ_REG_0X9B_BITS {
	unsigned int	fir_step_1d                    :5,
	reserved23                     :3,
	fir_step_2d                    :5,
	reserved24                     :3,
	fir_step_3d                    :5,
	reserved25                     :3,
	fir_step_4d                    :5,
	reserved26                     :3;
};
struct	ATSC_EQ_REG_0X9C_BITS {
	unsigned int	fir_step_5d                    :5,
	reserved27                     :3,
	fir_step_6d                    :5,
	reserved28                     :3,
	fir_step_7d                    :5,
	reserved29                     :3,
	fir_step_8d                    :5,
	reserved30                     :3;
};
struct	ATSC_EQ_REG_0X9D_BITS {
	unsigned int	step_size_exp5_1               :5,
	reserved31                     :3,
	step_size_exp5_2               :5,
	reserved32                     :3,
	step_size_exp5_3               :5,
	reserved33                     :3,
	step_size_exp5_4               :5,
	reserved34                     :3;
};
struct	ATSC_EQ_REG_0X9E_BITS {
	unsigned int	step_size_exp5_5               :5,
	reserved35                     :3,
	step_size_exp5_6               :5,
	reserved36                     :3,
	step_size_exp5_7               :5,
	reserved37                     :3,
	step_size_exp5_8               :5,
	reserved38                     :3;
};
struct	ATSC_EQ_REG_0X9F_BITS {
	unsigned int	iir_gn4                        :7,
	reserved39                     :1,
	iir_gn5                        :7,
	reserved40                     :1,
	iir_gn6                        :7,
	reserved41                     :1,
	iir_gn7                        :7,
	reserved42                     :1;
};
struct	ATSC_EQ_REG_0XA0_BITS {
	unsigned int	iir_gn8                        :7,
	reserved43                     :25;
};
struct	ATSC_EQ_REG_0XA1_BITS {
	unsigned int	iir_gn4t                       :7,
	reserved44                     :1,
	iir_gn5t                       :7,
	reserved45                     :1,
	iir_gn6t                       :7,
	reserved46                     :1,
	iir_gn7t                       :7,
	reserved47                     :1;
};
struct	ATSC_EQ_REG_0XA2_BITS {
	unsigned int	iir_gn8t                       :7,
	reserved48                     :25;
};
struct	ATSC_EQ_REG_0XA3_BITS {
	unsigned int	ft_step_size5                  :4,
	ft_step_size6                  :4,
	ft_step_size7                  :4,
	ft_step_size8                  :4,
	pt_step_size5                  :4,
	pt_step_size6                  :4,
	pt_step_size7                  :4,
	pt_step_size8                  :4;
};
struct	ATSC_EQ_REG_0XA4_BITS {
	unsigned int	pt_gain_size5                  :5,
	reserved49                     :3,
	pt_gain_size6                  :5,
	reserved50                     :3,
	pt_gain_size7                  :5,
	reserved51                     :3,
	pt_gain_size8                  :5,
	reserved52                     :3;
};
union	ATSC_EQ_REG_0XA5_BITS {
	unsigned int bits;
	struct{
		unsigned int	eqst2_up_cnt                   :8,
		eqst3_up_cnt                   :8,
		eqst4_up_cnt                   :8,
		eqst5_up_cnt                   :8;
	} b;
};
struct	ATSC_EQ_REG_0XA6_BITS {
	unsigned int	eqst6_up_cnt                   :8,
	eqst7_up_cnt                   :8,
	eqst8_up_cnt                   :8,
	eqst9_up_cnt                   :8;
};
struct	ATSC_EQ_REG_0XA7_BITS {
	unsigned int	eqst2_to_cnt                   :7,
	reserved53                     :1,
	eqst3_to_cnt                   :7,
	reserved54                     :1,
	eqst4_to_cnt                   :7,
	reserved55                     :1,
	eqst5_to_cnt                   :7,
	reserved56                     :1;
};
struct	ATSC_EQ_REG_0XA8_BITS {
	unsigned int	eqst6_to_cnt                   :7,
	reserved57                     :1,
	eqst7_to_cnt                   :7,
	reserved58                     :1,
	eqst8_to_cnt                   :7,
	reserved59                     :1,
	eqst9_to_cnt                   :7,
	reserved60                     :1;
};
union	ATSC_EQ_REG_0XA9_BITS {
	unsigned int bits;
	struct{
		unsigned int	iir_err_range_sel4             :3,
		reserved61                     :1,
		iir_err_range_sel5             :3,
		reserved62                     :1,
		iir_err_range_sel6             :3,
		reserved63                     :1,
		iir_err_range_sel7             :3,
		reserved64                     :1,
		iir_err_range_sel8             :3,
		reserved65                     :13;
	} b;
};
struct	ATSC_EQ_REG_0XAA_BITS {
	unsigned int	eq_ctrl0                       :10,
	reserved66                     :6,
	eq_ctrl1                       :10,
	reserved67                     :6;
};
struct	ATSC_EQ_REG_0XAB_BITS {
	unsigned int	eq_ctrl2                       :10,
	reserved68                     :6,
	eq_ctrl3                       :10,
	reserved69                     :6;
};
struct	ATSC_EQ_REG_0XAC_BITS {
	unsigned int	eq_ctrl4                       :10,
	reserved70                     :6,
	eq_ctrl5                       :10,
	reserved71                     :6;
};
union ATSC_EQ_REG_0XAD_BITS {
	unsigned int bits;
	struct	 {
		unsigned int eq_ctrl6                       :10,
		reserved72                     :6,
		eq_ctrl7                       :10,
		reserved73                     :6;
	} b;
};
struct	ATSC_EQ_REG_0XAE_BITS {
	unsigned int	eq_ctrl8                       :10,
	reserved74                     :6,
	eq_ctrl9                       :10,
	reserved75                     :6;
};
struct	ATSC_EQ_REG_0XAF_BITS {
	unsigned int	ffe_leak_set_rca1              :12,
	reserved76                     :4,
	ffe_leak_set_train             :12,
	reserved77                     :4;
};
struct	ATSC_EQ_REG_0XB0_BITS {
	unsigned int	ffe_leak_set_ddm               :12,
	reserved78                     :4,
	ffe_leak_set_rca2              :12,
	reserved79                     :4;
};
struct	ATSC_EQ_REG_0XB1_BITS {
	unsigned int	ffe_slope_leak_set_rca1        :12,
	reserved80                     :4,
	ffe_slope_leak_set_train       :12,
	reserved81                     :4;
};
struct	ATSC_EQ_REG_0XB2_BITS {
	unsigned int	ffe_slope_leak_set_ddm         :12,
	reserved82                     :4,
	ffe_slope_leak_set_rca2        :12,
	reserved83                     :4;
};
struct	ATSC_EQ_REG_0XB3_BITS {
	unsigned int	ffe_leak_rate_set_ddm          :8,
	ffe_leak_rate_set_rca2         :8,
	ffe_leak_rate_set_rca1         :8,
	ffe_leak_rate_set_train        :8;
};
struct	ATSC_EQ_REG_0XB4_BITS {
	unsigned int	err_limit_set_ddm_rca          :4,
	err_limit_set_rca2_rca         :4,
	err_limit_set_rca1_rca         :4,
	err_limit_set_train_rca        :4,
	err_limit_set_ddm_cma          :4,
	err_limit_set_rca2_cma         :4,
	err_limit_set_rca1_cma         :4,
	err_limit_set_train_cma        :4;
};
struct	ATSC_EQ_REG_0XB5_BITS {
	unsigned int	ffe_tap_update_limit_set_ddm   :4,
	ffe_tap_update_limit_set_rca2  :4,
	ffe_tap_update_limit_set_rca1  :4,
	ffe_tap_update_limit_set_train :4,
	ffe_stepsizexerr_limit_set_ddm :4,
	ffe_stepsizexerr_limit_set_rca2 :4,
	ffe_stepsizexerr_limit_set_rca1 :4,
	ffe_stepsizexerr_limit_set_train :4;
};
struct	ATSC_EQ_REG_0XB6_BITS {
	unsigned int	ffe_conj_data_limit_set_ddm    :5,
	reserved84                     :3,
	ffe_conj_data_limit_set_rca2   :5,
	reserved85                     :3,
	ffe_conj_data_limit_set_rca1   :5,
	reserved86                     :3,
	ffe_conj_data_limit_set_train  :5,
	reserved87                     :3;
};
struct	ATSC_EQ_REG_0XB7_BITS {
	unsigned int	iir_leak_rate_ddm              :16,
	iir_leak_rate_blind            :16;
};
struct	ATSC_EQ_REG_0XB8_BITS {
	unsigned int	iir_leak_rate1_ddm             :16,
	iir_leak_rate1_blind           :16;
};
struct	ATSC_EQ_REG_0XB9_BITS {
	unsigned int	iir1_st0_tap_limit_up4         :8,
	iir1_st0_tap_limit_up5         :8,
	iir1_st0_tap_limit_up6         :8,
	iir1_st0_tap_limit_up7         :8;
};
struct	ATSC_EQ_REG_0XBA_BITS {
	unsigned int	iir1_st0_tap_limit_up8         :8,
	iir1_st0_tap_limit_low4        :8,
	iir1_st0_tap_limit_low5        :8,
	iir1_st0_tap_limit_low6        :8;
};
struct	ATSC_EQ_REG_0XBB_BITS {
	unsigned int	iir1_st0_tap_limit_low7        :8,
	iir1_st0_tap_limit_low8        :8,
	iir1_st1_tap_limit_up4         :8,
	iir1_st1_tap_limit_up5         :8;
};
struct	ATSC_EQ_REG_0XBC_BITS {
	unsigned int	iir1_st1_tap_limit_up6         :8,
	iir1_st1_tap_limit_up7         :8,
	iir1_st1_tap_limit_up8         :8,
	iir1_st1_tap_limit_low4        :8;
};
struct	ATSC_EQ_REG_0XBD_BITS {
	unsigned int	iir1_st1_tap_limit_low5        :8,
	iir1_st1_tap_limit_low6        :8,
	iir1_st1_tap_limit_low7        :8,
	iir1_st1_tap_limit_low8        :8;
};
struct	ATSC_EQ_REG_0XBE_BITS {
	unsigned int	iir2_st0_tap_limit_up4         :8,
	iir2_st0_tap_limit_up5         :8,
	iir2_st0_tap_limit_up6         :8,
	iir2_st0_tap_limit_up7         :8;
};
struct	ATSC_EQ_REG_0XBF_BITS {
	unsigned int	iir2_st0_tap_limit_up8         :8,
	iir2_st0_tap_limit_low4        :8,
	iir2_st0_tap_limit_low5        :8,
	iir2_st0_tap_limit_low6        :8;
};
struct	ATSC_EQ_REG_0XC0_BITS {
	unsigned int	iir2_st0_tap_limit_low7        :8,
	iir2_st0_tap_limit_low8        :8,
	reserved88                     :16;
};
struct	ATSC_EQ_REG_0XC1_BITS {
	unsigned int	bm_acum_min_diff_ctrl          :24,
	reserved89                     :8;
};
struct	ATSC_EQ_REG_0XC2_BITS {
	unsigned int	pt_err_limit                   :16,
	reserved90                     :16;
};
struct	ATSC_EQ_REG_0XC3_BITS {
	unsigned int	snr                            :16,
	reserved91                     :16;
};
struct	ATSC_EQ_REG_0XC4_BITS {
	unsigned int	bm_acum_diff                   :21,
	reserved92                     :11;
};
struct	ATSC_EQ_REG_0XC5_BITS {
	unsigned int	cwth_exp                       :10,
	reserved93                     :6,
	pwr_avgw                       :8,
	reserved94                     :8;
};
struct	ATSC_EQ_REG_0XC6_BITS {
	unsigned int	cwth_man                       :24,
	reserved95                     :8;
};
struct	ATSC_EQ_REG_0XC7_BITS {
	unsigned int	inband_pwr_th                  :16,
	reserved96                     :16;
};
union ATSC_EQ_REG_0XC8_BITS {
	unsigned int bits;
	struct	 {
		unsigned int	timing_adjust_thd              :16,
		timing_adjust_eqst             :2,
		reserved97                     :6,
		timing_adjust_bypass           :1,
		reserved98                     :7;
	} b;
};
struct	ATSC_EQ_REG_0XC9_BITS {
	unsigned int	timing_adjust_snr_thd          :16,
	timing_adjust_thd_1            :16;
};
union ATSC_EQ_REG_0XCA_BITS {
	unsigned int bits;
	struct	 {
		unsigned int	timing_offset                  :9,
		reserved99                     :7,
		timing_direct                  :1,
		reserved100                    :15;
	} b;
};
struct	ATSC_EQ_REG_0XCB_BITS {
	unsigned int	timing_adjust_sum              :22,
	reserved101                    :10;
};
struct	ATSC_EQ_REG_0XCC_BITS {
	unsigned int	timing_adjust_max              :22,
	reserved102                    :10;
};
struct	ATSC_EQ_REG_0XCD_BITS {
	unsigned int	timing_adjust_ori              :22,
	reserved103                    :10;
};
struct	ATSC_EQ_REG_0XD0_BITS {
	unsigned int	tap_fd_step1                   :8,
	tap_fd_ctrl                    :8,
	ar_stop_st                     :4,
	reserved104                    :12;
};
struct	ATSC_EQ_REG_0XD2_BITS {
	unsigned int	tap_shift_ctrl0                :8,
	tap_shift_ctrl1                :8,
	tap_shift_ctrl2                :8,
	tap_shift_ctrl3                :8;
};
struct	ATSC_EQ_REG_0XD3_BITS {
	unsigned int	tap_shift_ctrl4                :8,
	tap_shift_ctrl5                :8,
	tap_shift_ctrl6                :8,
	tap_shift_ctrl7                :8;
};
struct	ATSC_EQ_REG_0XD4_BITS {
	unsigned int	tap_shift_ctrl8                :8,
	tap_shift_ctrl9                :8,
	tap_shift_ctrla                :8,
	tap_shift_ctrlb                :8;
};
struct	ATSC_EQ_REG_0XD5_BITS {
	unsigned int	tap_shift_ctrlc                :8,
	tap_shift_ctrld                :8,
	tap_shift_ctrle                :8,
	tap_shift_ctrlf                :8;
};
struct	ATSC_EQ_REG_0XD6_BITS {
	unsigned int	tap_mask_adr                   :16,
	reserved105                    :16;
};
struct	ATSC_EQ_REG_0XD7_BITS {
	unsigned int	tap_dfe_abs                    :21,
	reserved106                    :11;
};
struct	ATSC_EQ_REG_0XD9_BITS {
	unsigned int	tap_snr                        :16,
	reserved107                    :16;
};

#endif
