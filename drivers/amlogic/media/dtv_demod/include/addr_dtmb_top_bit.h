/*
 * drivers/amlogic/media/dtv_demod/include/addr_dtmb_top_bit.h
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

#ifndef __ADDR_DTMB_TOP_BIT_H__
#define __ADDR_DTMB_TOP_BIT_H__

/*dtmb_cfg_01*/

union DTMB_TOP_CTRL_SW_RST_BITS {
	unsigned int d32;
	struct {
		unsigned int ctrl_sw_rst:1, ctrl_sw_rst_noreg:1, reserved0:30;
	} b;
};
/*dtmb_cfg_02*/

union DTMB_TOP_TESTBUS_BITS {
	unsigned int d32;
	struct {
		unsigned int testbus_addr:16, testbus_en:1, reserved1:15;
	} b;
};
/*dtmb_cfg_03*/

union DTMB_TOP_TB_BITS {
	unsigned int d32;
	struct {
		unsigned int tb_act_width:5,
		    reserved2:3,
		    tb_dc_mk:3,
		    reserved3:1, tb_capture_stop:1, tb_self_test:1,
		    reserved4:18;
	} b;
};
/*dtmb_cfg_07*/

union DTMB_TOP_CTRL_ENABLE_BITS {
	unsigned int d32;
	struct {
		unsigned int ctrl_enable:24, reserved5:8;
	} b;
};
/*dtmb_cfg_08*/

union DTMB_TOP_CTRL_LOOP_BITS {
	unsigned int d32;
	struct {
		unsigned int ctrl_src_pnphase_loop:1,
		    ctrl_src_sfo_loop:1,
		    ctrl_ddc_fcfo_loop:1, ctrl_ddc_icfo_loop:1, reserved6:28;
	} b;
};
/*dtmb_cfg_09*/

union DTMB_TOP_CTRL_FSM_BITS {
	unsigned int d32;
	struct {
		unsigned int ctrl_fsm_state:5,
		    reserved7:3,
		    ctrl_fsm_v:1, reserved8:3, ctrl_reset_state:4,
		    reserved9:16;
	} b;
};
/*dtmb_cfg_0a*/

union DTMB_TOP_CTRL_AGC_BITS {
	unsigned int d32;
	struct {
		unsigned int ctrl_fast_agc:1,
		    ctrl_agc_bypass:1,
		    ts_cfo_bypass:1, sfo_strong0_bypass:1, reserved10:28;
	} b;
};
/*dtmb_cfg_0b*/

union DTMB_TOP_CTRL_TS_SFO_CFO_BITS {
	unsigned int d32;
	struct {
		unsigned int ctrl_ts_q:10,
		    reserved11:2,
		    ctrl_pnphase_q:7, reserved12:1, ctrl_sfo_q:4, ctrl_cfo_q:8;
	} b;
};
/*dtmb_cfg_0c*/

union DTMB_TOP_CTRL_FEC_BITS {
	unsigned int d32;
	struct {
		unsigned int reserved13:8,
		    ctrl_ts_to_th:4,
		    ctrl_pnphase_to_th:4,
		    ctrl_sfo_to_th:4,
		    ctrl_fe_to_th:4, ctrl_che_to_th:4, ctrl_fec_to_th:4;
	} b;
};
/*dtmb_cfg_0d*/

union DTMB_TOP_CTRL_INTLV_TIME_BITS {
	unsigned int d32;
	struct {
		unsigned int ctrl_intlv720_time:12, ctrl_intlv240_time:12,
			reserved14:8;
	} b;
};
/*dtmb_cfg_0e*/

union DTMB_TOP_CTRL_DAGC_CCI_BITS {
	unsigned int d32;
	struct {
		unsigned int dagc_mode:2,
		    cci_dagc_mode:2,
		    cci_bypass:1,
		    fe_bypass:1,
		    reserved15:1,
		    new_sync1:1, new_sync2:1, fec_inzero_check:1,
		    reserved16:22;
	} b;
};
/*dtmb_cfg_0f*/
union DTMB_TOP_CTRL_TPS_BITS {
	unsigned int d32;
	struct {
		unsigned int sfo_gain:2,
		    freq_reverse:1,
		    qam4_nr:1,
		    intlv_mode:1,
		    code_rate:2,
		    constell:2,
		    tps_carrier_mode:1,
		    freq_reverse_known:1, tps_known:1, ctrl_tps_to_th:4,
		    reserved17:16;
	} b;
};
/*dtmb_cfg_c7*/
union DTMB_TOP_CCI_FLG_BITS {
	unsigned int d32;
	struct {
		unsigned int cci_flg_cnt:8, m_cci_ready:1, reserved18:23;
	} b;
};
/*dtmb_cfg_ca*/
union DTMB_TOP_FRONT_IQIB_CHECK_BITS {
	unsigned int d32;
	struct {
		unsigned int front_iqib_check_b:12,
		    front_iqib_check_a:10, reserved19:10;
	} b;
};
/*dtmb_cfg_cb*/
union DTMB_TOP_SYNC_TS_BITS {
	unsigned int d32;
	struct {
		unsigned int sync_ts_idx:2, sync_ts_pos:13, sync_ts_q:10,
			reserved20:7;
	} b;
};
/*dtmb_cfg_cd*/

union DTMB_TOP_SYNC_PNPHASE_BITS {
	unsigned int d32;
	struct {
		unsigned int sync_pnphase_max_q_idx:2,
		    sync_pnphase:8, sync_pnphase_max_q:7, reserved21:15;
	} b;
};
/*dtmb_cfg_d2*/

union DTMB_TOP_CTRL_DDC_ICFO_BITS {
	unsigned int d32;
	struct {
		unsigned int ctrl_ddc_icfo:20, reserved22:12;
	} b;
};
/*dtmb_cfg_d3*/

union DTMB_TOP_CTRL_DDC_FCFO_BITS {
	unsigned int d32;
	struct {
		unsigned int ctrl_src_sfo:17, ctrl_ddc_fcfo:14, reserved23:1;
	} b;
};
/*dtmb_cfg_d8*/

union DTMB_TOP_CTRL_TS2_BITS {
	unsigned int d32;
	struct {
		unsigned int ctrl_ts2_workcnt:8,
		    ctrl_pnphase_workcnt:8, ctrl_sfo_workcnt:8,
		    sync_fe_workcnt:8;
	} b;
};
/*dtmb_cfg_d9*/

union DTMB_TOP_FRONT_AGC_BITS {
	unsigned int d32;
	struct {
		unsigned int front_agc_if_gain:11,
		    front_agc_rf_gain:11, front_agc_power:9, reserved:1;
	} b_v2;
	struct {
		unsigned int front_agc_if_gain:11,
		    front_agc_rf_gain:11, front_agc_power:10;
	} b;
};
/*dtmb_cfg_da*/

union DTMB_TOP_FRONT_DAGC_BITS {
	unsigned int d32;
	struct {
		unsigned int front_dagc_power:8, front_dagc_gain:12,
			reserved24:12;
	} b;
};
/*dtmb_cfg_dd*/

union DTMB_TOP_FEC_LDPC_IT_AVG_BITS {
	unsigned int d32;
	struct {
		unsigned int fec_ldpc_it_avg:16, fec_ldpc_per_rpt:13,
				reserved25:3;
	} b;
};
/*dtmb_cfg_e0*/

union DTMB_TOP_CTRL_ICFO_ALL_BITS {
	unsigned int d32;
	struct {
		unsigned int ctrl_icfo_all:20, reserved26:12;
	} b;
};
/*dtmb_cfg_e1*/

union DTMB_TOP_CTRL_FCFO_ALL_BITS {
	unsigned int d32;
	struct {
		unsigned int ctrl_fcfo_all:20, reserved27:12;
	} b;
};
/*dtmb_cfg_e2*/

union DTMB_TOP_CTRL_SFO_ALL_BITS {
	unsigned int d32;
	struct {
		unsigned int ctrl_sfo_all:25, reserved28:7;
	} b;
};
/*dtmb_cfg_e3*//*have v2*/
union DTMB_TOP_FEC_LOCK_SNR_BITS {
	unsigned int d32;
	struct {
		unsigned int che_snr:12,
		    fec_lock:1, reserved29:1, che_snr_average:12, reserved30:6;
	} b_v2;
	struct {
		unsigned int che_snr:14,
		    fec_lock:1, reserved29:1, che_snr_average:14, reserved30:2;
	} b;
};
/*dtmb_cfg_e4*/

union DTMB_TOP_CHE_SEG_FACTOR_BITS {
	unsigned int d32;
	struct {
		unsigned int che_seg_factor:14, reserved31:18;
	} b;
};
/*dtmb_cfg_e5*/

union DTMB_TOP_CTRL_CHE_WORKCNT_BITS {
	unsigned int d32;
	struct {
		unsigned int ctrl_che_workcnt:8,
		    ctrl_fec_workcnt:8,
		    ctrl_constell:2,
		    ctrl_code_rate:2,
		    ctrl_intlv_mode:1,
		    ctrl_qam4_nr:1, ctrl_freq_reverse:1, reserved32:9;
	} b;
};
/*dtmb_cfg_ee*/

union DTMB_TOP_SYNC_CCI_NF1_BITS {
	unsigned int d32;
	struct {
		unsigned int sync_cci_nf1_b1:10,
		    sync_cci_nf1_a2:10, sync_cci_nf1_a1:10, reserved33:2;
	} b;
};
/*dtmb_cfg_ef*/

union DTMB_TOP_SYNC_CCI_NF2_BITS {
	unsigned int d32;
	struct {
		unsigned int sync_cci_nf2_b1:10,
		    sync_cci_nf2_a2:10, sync_cci_nf2_a1:10, reserved34:2;
	} b;
};
/*dtmb_cfg_f0*/

union DTMB_TOP_SYNC_CCI_NF2_POSITION_BITS {
	unsigned int d32;
	struct {
		unsigned int sync_cci_nf2_position:11,
		    sync_cci_nf1_position:11,
		    sync_cci_nf2_det:1, sync_cci_nf1_det:1, reserved35:8;
	} b;
};
/*dtmb_cfg_f1*/

union DTMB_TOP_CTRL_SYS_OFDM_CNT_BITS {
	unsigned int d32;
	struct {
		unsigned int ctrl_sys_ofdm_cnt:8,
		    mobi_det_power_var:19,
		    reserved36:1, ctrl_che_working_state:2, reserved37:2;
	} b;
};
/*dtmb_cfg_f2*/

union DTMB_TOP_CTRL_TPS_Q_FINAL_BITS {
	unsigned int d32;
	struct {
		unsigned int ctrl_tps_q_final:7, ctrl_tps_suc_cnt:7,
				reserved38:18;
	} b;
};
/*dtmb_cfg_f3*/

union DTMB_TOP_FRONT_DC_BITS {
	unsigned int d32;
	struct {
		unsigned int front_dc_q:10, front_dc_i:10,
				reserved39:12;
	} b;
};
/*dtmb_cfg_ff*/

union DTMB_TOP_CTRL_TOTPS_READY_CNT_BITS {
	unsigned int d32;
	struct {
		unsigned int ctrl_dead_lock_det:1,
		    ctrl_dead_lock:1,
		    reserved40:2,
		    ctrl_dead_cnt:4, reserved41:8, ctrl_totps_ready_cnt:16;
	} b;
};

#endif
