/*
 * drivers/amlogic/media/dtv_demod/include/addr_dtmb_sync_bit.h
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

#ifndef __ADDR_DTMB_SYNC_BIT_H__
#define __ADDR_DTMB_SYNC_BIT_H__

struct DTMB_SYNC_TS_CFO_PN_VALUE_BITS {
	unsigned int ts_cfo_pn1_value:16, ts_cfo_pn0_value:16;
};
struct DTMB_SYNC_TS_CFO_ERR_LIMIT_BITS {
	unsigned int ts_cfo_err_limit:16, ts_cfo_pn2_value:16;
};
struct DTMB_SYNC_TS_CFO_PN_MODIFY_BITS {
	unsigned int ts_cfo_pn1_modify:16, ts_cfo_pn0_modify:16;
};
struct DTMB_SYNC_TS_GAIN_BITS {
	unsigned int ts_gain:2,
	    reserved0:2,
	    ts_sat_shift:3,
	    reserved1:1,
	    ts_fixpn_en:1,
	    ts_fixpn:2, reserved2:1, ts_cfo_cut:4, ts_cfo_pn2_modify:16;
};
union DTMB_SYNC_FE_CONFIG_BITS {
	unsigned int d32;
	struct {
	unsigned int fe_lock_len:4,
	    fe_sat_shift:3, reserved3:1, fe_cut:4, reserved4:4, fe_modify:16;
	} b;
};
struct DTMB_SYNC_PNPHASE_OFFSET_BITS {
	unsigned int pnphase_offset2:4,
	    pnphase_offset1:4, pnphase_offset0:4, reserved5:20;
};
struct DTMB_SYNC_PNPHASE_CONFIG_BITS {
	unsigned int pnphase_gain:2,
	    reserved6:2,
	    pnphase_sat_shift:4, pnphase_cut:4, reserved7:4, pnphase_modify:16;
};
struct DTMB_SYNC_SFO_SFO_PN0_MODIFY_BITS {
	unsigned int sfo_cfo_pn0_modify:16, sfo_sfo_pn0_modify:16;
};
struct DTMB_SYNC_SFO_SFO_PN1_MODIFY_BITS {
	unsigned int sfo_cfo_pn1_modify:16, sfo_sfo_pn1_modify:16;
};
struct DTMB_SYNC_SFO_SFO_PN2_MODIFY_BITS {
	unsigned int sfo_cfo_pn2_modify:16, sfo_sfo_pn2_modify:16;
};
struct DTMB_SYNC_SFO_CONFIG_BITS {
	unsigned int sfo_sat_shift:4,
	    sfo_gain:2,
	    reserved8:2,
	    sfo_dist:2,
	    reserved9:2,
	    sfo_cfo_cut:4, sfo_sfo_cut:4, sfo_cci_th:4, reserved10:8;
};
struct DTMB_SYNC_TRACK_CFO_MAX_BITS {
	unsigned int track_cfo_max:8,
	    track_sfo_max:8, track_max_en:1, ctrl_fe_to_th:4, reserved11:11;
};
struct DTMB_SYNC_CCI_DAGC_CONFIG1_BITS {
	unsigned int cci_dagc_bypass:1,
	    cci_dagc_power_alpha:2,
	    cci_dagc_bw:3,
	    cci_dagc_gain_ctrl:12,
	    cci_dagc_gain_step_er:6, cci_dagc_gain_step:6, reserved12:2;
};
struct DTMB_SYNC_CCI_DAGC_CONFIG2_BITS {
	unsigned int cci_dagc_target_power_l:8,
	    cci_dagc_target_power_h:8,
	    cci_dagc_target_power_ler:8, cci_dagc_target_power_her:8;
};
struct DTMB_SYNC_CCI_RP_BITS {
	unsigned int cci_rpsq_n:10, reserved13:2, cci_rp_n:13, reserved14:7;
};
struct DTMB_SYNC_CCI_DET_THRES_BITS {
	unsigned int cci_avr_times:5,
	    reserved15:3, cci_det_thres:3, reserved16:21;
};
struct DTMB_SYNC_CCI_NOTCH1_CONFIG1_BITS {
	unsigned int cci_notch1_a1:10,
	    reserved17:2, cci_notch1_en:1, reserved18:19;
};
struct DTMB_SYNC_CCI_NOTCH1_CONFIG2_BITS {
	unsigned int cci_notch1_b1:10,
	    reserved19:2, cci_notch1_a2:10, reserved20:10;
};
struct DTMB_SYNC_CCI_NOTCH2_CONFIG1_BITS {
	unsigned int cci_notch2_a1:10,
	    reserved21:2, cci_notch2_en:1, reserved22:3, cci_mpthres:16;
};
struct DTMB_SYNC_CCI_NOTCH2_CONFIG2_BITS {
	unsigned int cci_notch2_b1:10,
	    reserved23:2, cci_notch2_a2:10, reserved24:10;
};

#endif
