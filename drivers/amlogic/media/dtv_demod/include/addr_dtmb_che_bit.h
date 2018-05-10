/*
 * drivers/amlogic/media/dtv_demod/include/addr_dtmb_che_bit.h
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

#ifndef __ADDR_DTMB_CHE_BIT_H__
#define __ADDR_DTMB_CHE_BIT_H__

struct DTMB_CHE_TE_HREB_SNR_BITS {
	unsigned int te_hreb_snr:21, reserved0:11;
};
struct DTMB_CHE_MC_SC_TIMING_POWTHR_BITS {
	unsigned int mc_timing_powthr1:5,
	    reserved1:3,
	    mc_timing_powthr0:5,
	    reserved2:2,
	    sc_timing_powthr1:5, reserved3:4, sc_timing_powthr0:5, reserved4:3;
};
struct DTMB_CHE_MC_SC_PROTECT_GD_BITS {
	unsigned int h_valid:2,
	    reserved5:2,
	    dist:3,
	    reserved6:1,
	    ma_size:3,
	    reserved7:1,
	    mc_protect_gd:5, reserved8:3, sc_protect_gd:5, reserved9:7;
};
struct DTMB_CHE_TIMING_LIMIT_BITS {
	unsigned int ncoh_thd:3,
	    reserved10:1,
	    coh_thd:3,
	    reserved11:1,
	    strong_loc_thd:8, reserved12:4, timing_limit:5, reserved13:7;
};
struct DTMB_CHE_TPS_CONFIG_BITS {
	unsigned int tps_pst_num:5,
	    reserved14:3,
	    tps_pre_num:5, reserved15:3, chi_power_thr:8, reserved16:8;
};
struct DTMB_CHE_FD_TD_STEPSIZE_BITS {
	unsigned int fd_stepsize_thr03:5,
	    fd_stepsize_thr02:5,
	    fd_stepsize_thr01:5,
	    td_stepsize_thr03:5,
	    td_stepsize_thr02:5, td_stepsize_thr01:5, reserved17:2;
};
struct DTMB_CHE_QSTEP_SET_BITS {
	unsigned int factor_stable_thres:10,
	    reserved18:2, qstep_set:13, qstep_set_val:1, reserved19:6;
};
struct DTMB_CHE_SEG_CONFIG_BITS {
	unsigned int seg_bypass:1,
	    seg_num_1seg_log2:3,
	    seg_alpha:3,
	    seg_read_val:1, seg_read_addr:12, noise_input_shift:4, reserved20:8;
};
struct DTMB_CHE_FD_TD_LEAKSIZE_CONFIG1_BITS {
	unsigned int fd_leaksize_thr03:5,
	    fd_leaksize_thr02:5,
	    fd_leaksize_thr01:5,
	    td_leaksize_thr03:5,
	    td_leaksize_thr02:5, td_leaksize_thr01:5, reserved21:2;
};
struct DTMB_CHE_FD_TD_LEAKSIZE_CONFIG2_BITS {
	unsigned int fd_leaksize_thr13:5,
	    fd_leaksize_thr12:5,
	    fd_leaksize_thr11:5,
	    td_leaksize_thr13:5,
	    td_leaksize_thr12:5, td_leaksize_thr11:5, reserved22:2;
};
struct DTMB_CHE_FD_TD_COEFF_BITS {
	unsigned int td_coeff_frz:14,
	    reserved23:2,
	    td_coeff_addr:4,
	    td_coeff_init:1,
	    td_coeff_rst:1,
	    fd_coeff_init:1,
	    fd_coeff_done:1,
	    fd_coeff_rst:1, td_coeff_done:1, fd_coeff_addr:5, reserved24:1;
};
struct DTMB_CHE_M_CCI_THR_CONFIG1_BITS {
	unsigned int m_cci_thr_mc1:10,
	    m_cci_thr_mc2:10, m_cci_thr_mc3:10, reserved25:2;
};
struct DTMB_CHE_M_CCI_THR_CONFIG2_BITS {
	unsigned int m_cci_thr_sc2:10,
	    m_cci_thr_sc3:10, m_cci_thr_mc0:10, reserved26:2;
};
struct DTMB_CHE_M_CCI_THR_CONFIG3_BITS {
	unsigned int m_cci_thr_ma:10,
	    m_cci_thr_sc0:10, m_cci_thr_sc1:10, reserved27:2;
};
struct DTMB_CHE_CCIDET_CONFIG_BITS {
	unsigned int ccidet_dly:7,
	    ccidet_malpha:3,
	    ccidet_sc_mask_rng:5,
	    ccidet_mc_mask_rng:5,
	    ccidet_masize:4,
	    ccidect_sat_sft:3,
	    ccicnt_out_sel:2, tune_mask:1, m_cci_bypass:1, reserved28:1;
};
struct DTMB_CHE_IBDFE_CONFIG1_BITS {
	unsigned int ibdfe_cci_just_thr:13,
	    reserved29:3,
	    ibdfe_dmsg_point:5, reserved30:3, ibdfe_dmsg_alp:3, reserved31:5;
};
struct DTMB_CHE_IBDFE_CONFIG2_BITS {
	unsigned int ibdfe_rou_rat_1:10,
	    reserved32:6, ibdfe_rou_rat_0:10, reserved33:6;
};
struct DTMB_CHE_IBDFE_CONFIG3_BITS {
	unsigned int ibdfe_rou_rat_3:10,
	    reserved34:6, ibdfe_rou_rat_2:10, reserved35:6;
};
struct DTMB_CHE_TD_COEFF_BITS {
	unsigned int td_coeff:24, reserved36:8;
};
struct DTMB_CHE_FD_TD_STEPSIZE_ADJ_BITS {
	unsigned int fd_stepsize_adj:3, td_stepsize_adj:3, reserved37:26;
};
struct DTMB_CHE_FD_COEFF_BITS {
	unsigned int fd_coeff:24, reserved38:8;
};
struct DTMB_CHE_FD_LEAKSIZE_BITS {
	unsigned int fd_leaksize:18, reserved39:14;
};
struct DTMB_CHE_IBDFE_CONFIG4_BITS {
	unsigned int ibdfe_fdbk_iter:4,
	    ibdfe_eqout_iter:4,
	    eq_dist_thr_tps:4,
	    eq_soft_slicer_en:1,
	    reserved40:3,
	    gd_len:5, ibdfe_blank_y:1, reserved41:1, ibdfe_dmsg_start_cnt:9;
};
struct DTMB_CHE_IBDFE_CONFIG5_BITS {
	unsigned int ibdfe_init_snr:12,
	    reserved42:4, eq_init_snr:12, reserved43:4;
};
struct DTMB_CHE_IBDFE_CONFIG6_BITS {
	unsigned int ibdfe_const_thr3:4,
	    ibdfe_const_thr2:4,
	    ibdfe_const_thr1:4,
	    ibdfe_const_thr0:4,
	    ibdfe_threshold3:4,
	    ibdfe_threshold2:4, ibdfe_threshold1:4, ibdfe_threshold0:4;
};
struct DTMB_CHE_IBDFE_CONFIG7_BITS {
	unsigned int ibdfe_pick_thr3:8,
	    ibdfe_pick_thr2:8, ibdfe_pick_thr1:8, ibdfe_pick_thr0:8;
};
struct DTMB_CHE_DCM_SC_MC_GD_LEN_BITS {
	unsigned int dcm_mc_gd_len:6,
	    reserved44:2, dcm_sc_gd_len:6, reserved45:2, eq_dsnr_slc2drm:16;
};
struct DTMB_CHE_EQMC_PICK_THR_BITS {
	unsigned int eqmc_pick_thr3:8,
	    eqmc_pick_thr2:8, eqmc_pick_thr1:8, eqmc_pick_thr0:8;
};
struct DTMB_CHE_EQMC_THRESHOLD_BITS {
	unsigned int eqmc_const_thr3:4,
	    eqmc_const_thr2:4,
	    eqmc_const_thr1:4,
	    eqmc_const_thr0:4,
	    eqmc_threshold3:4,
	    eqmc_threshold2:4, eqmc_threshold1:4, eqmc_threshold0:4;
};
struct DTMB_CHE_EQSC_PICK_THR_BITS {
	unsigned int eqsc_pick_thr3:8,
	    eqsc_pick_thr2:8, eqsc_pick_thr1:8, eqsc_pick_thr0:8;
};
struct DTMB_CHE_EQSC_THRESHOLD_BITS {
	unsigned int eqsc_const_thr3:4,
	    eqsc_const_thr2:4,
	    eqsc_const_thr1:4,
	    eqsc_const_thr0:4,
	    eqsc_threshold3:4,
	    eqsc_threshold2:4, eqsc_threshold1:4, eqsc_threshold0:4;
};
struct DTMB_CHE_PROTECT_GD_TPS_BITS {
	unsigned int pow_norm:10,
	    ncoh_thd_tps:3,
	    coh_thd_tps:3, thr_max:10, protect_gd_tps:5, reserved46:1;
};
struct DTMB_CHE_FD_TD_STEPSIZE_THR1_BITS {
	unsigned int fd_stepsize_thr13:5,
	    fd_stepsize_thr12:5,
	    fd_stepsize_thr11:5,
	    td_stepsize_thr13:5,
	    td_stepsize_thr12:5, td_stepsize_thr11:5, reserved47:2;
};
struct DTMB_CHE_TDFD_SWITCH_SYM1_BITS {
	unsigned int tdfd_switch_sym00:16, tdfd_switch_sym01:16;
};
struct DTMB_CHE_TDFD_SWITCH_SYM2_BITS {
	unsigned int tdfd_switch_sym10:16, tdfd_switch_sym11:16;
};
struct DTMB_CHE_EQ_CONFIG_BITS {
	unsigned int eq_dsnr_h2drm:6,
	    eq_cmp_en:1,
	    eq_imp_setzero_en:1,
	    dcm_sc_bypass:1,
	    dcm_mc_bypass:1,
	    dcm_sc_h_limit:4,
	    dcm_mc_h_limit:4,
	    eqsnr_imp_alp:3, eqsnr_avg_alp:3, dcm_alpha:2, reserved48:6;
};
struct DTMB_CHE_EQSC_SNR_IMP_THR1_BITS {
	unsigned int eqsc_snr_imp_thr1:12, eqsc_snr_imp_thr0:12, reserved49:8;
};
struct DTMB_CHE_EQSC_SNR_IMP_THR2_BITS {
	unsigned int eqsc_snr_imp_thr3:12, eqsc_snr_imp_thr2:12, reserved50:8;
};
struct DTMB_CHE_EQMC_SNR_IMP_THR1_BITS {
	unsigned int eqmc_snr_imp_thr1:12, eqmc_snr_imp_thr0:12, reserved51:8;
};
struct DTMB_CHE_EQMC_SNR_IMP_THR2_BITS {
	unsigned int eqmc_snr_imp_thr3:12, eqmc_snr_imp_thr2:12, reserved52:8;
};
struct DTMB_CHE_EQSC_SNR_DROP_THR_BITS {
	unsigned int eqsc_snr_drop_thr3:8,
	    eqsc_snr_drop_thr2:8, eqsc_snr_drop_thr1:8, eqsc_snr_drop_thr0:8;
};
struct DTMB_CHE_EQMC_SNR_DROP_THR_BITS {
	unsigned int eqmc_snr_drop_thr3:8,
	    eqmc_snr_drop_thr2:8, eqmc_snr_drop_thr1:8, eqmc_snr_drop_thr0:8;
};
struct DTMB_CHE_M_CCI_THR_BITS {
	unsigned int ccidet_mask_rng_tps:5,
	    m_cci_thr_tps:10, m_cci_thr_ma_tps:10, reserved53:7;
};
struct DTMB_CHE_TPS_MC_BITS {
	unsigned int tps_mc_run_tim_limit:10,
	    tps_mc_suc_limit:7, tps_mc_q_thr:7, tps_mc_alpha:3, reserved54:5;
};
struct DTMB_CHE_TPS_SC_BITS {
	unsigned int tps_sc_run_tim_limit:10,
	    tps_sc_suc_limit:7, tps_sc_q_thr:7, tps_sc_alpha:3, reserved55:5;
};
struct DTMB_CHE_CHE_SET_FSM_BITS {
	unsigned int che_open_loop_len:12,
	    reserved56:4,
	    che_set_fsm_st:3, reserved57:1, che_set_fsm_en:1, reserved58:11;
};
struct DTMB_CHE_ZERO_NUM_THR_BITS {
	unsigned int null_frame_thr:16, zero_num_thr:12, reserved59:4;
};
struct DTMB_CHE_TIMING_READY_BITS {
	unsigned int timing_offset:11,
	    reserved60:5, timing_ready:1, reserved61:15;
};

#endif
