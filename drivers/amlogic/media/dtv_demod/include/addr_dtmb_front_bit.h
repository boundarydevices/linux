/*
 * drivers/amlogic/media/dtv_demod/include/addr_dtmb_front_bit.h
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

#ifndef __ADDR_DTMB_FRONT_BIT_H__
#define __ADDR_DTMB_FRONT_BIT_H__

union   DTMB_FRONT_AFIFO_ADC_BITS {
	unsigned int d32;
	struct {
		unsigned int afifo_nco_rate:8,
			     afifo_data_format:1,
			     afifo_bypass:1,
			     adc_sample:6,
			     adc_IQ:1,
			     reserved0:15;
	} b;
};
union DTMB_FRONT_AGC_CONFIG1_BITS {
	unsigned int d32;
	struct {
			unsigned int agc_target:4,
			     agc_cal_intv:2,
			     reserved1:2,
			     agc_gain_step2:6,
			     reserved2:2,
			     agc_gain_step1:6,
			     reserved3:2,
			     agc_a_filter_coef2:3,
			     reserved4:1,
			     agc_a_filter_coef1:3,
			     reserved5:1;
	} b;
};
union DTMB_FRONT_AGC_CONFIG2_BITS {
	unsigned int d32;
	struct {
			unsigned int agc_imp_thresh:4,
			     agc_imp_en:1,
			     agc_iq_exchange:1,
			     reserved6:2,
			     agc_clip_ratio:5,
			     reserved7:3,
			     agc_signal_clip_thr:6,
			     reserved8:2,
			     agc_sd_rate:7,
			     reserved9:1;
	} b;
};
union DTMB_FRONT_AGC_CONFIG3_BITS {
	unsigned int d32;
	struct {
			unsigned int agc_rffb_value:11,
			     reserved10:1,
			     agc_iffb_value:11,
			     reserved11:1,
			     agc_gain_step_rf:1,
			     agc_rfgain_freeze:1,
			     agc_tuning_slope:1,
			     agc_rffb_set:1,
			     agc_gain_step_if:1,
			     agc_ifgain_freeze:1,
			     agc_if_only:1,
			     agc_iffb_set:1;
	} b;
};
union DTMB_FRONT_AGC_CONFIG4_BITS {
	unsigned int d32;
	struct {
			unsigned int agc_rffb_gain_sat_i:8,
			     agc_rffb_gain_sat:8,
			     agc_iffb_gain_sat_i:8,
			     agc_iffb_gain_sat:8;
	} b;
};
union DTMB_FRONT_DDC_BYPASS_BITS {
	unsigned int d32;
	struct {
			unsigned int ddc_phase:25,
			     reserved12:3,
			     ddc_bypass:1,
			     reserved13:3;
	} b;
};
union DTMB_FRONT_DC_HOLD_BITS {
	unsigned int d32;
	struct {
			unsigned int dc_hold:1,
			     dc_alpha:3,
			     mobi_det_accu_len:3,
			     reserved14:1,
			     mobi_det_observe_len:3,
			     reserved15:1,
			     channel_static_th:4,
			     channel_portable_th:4,
			     dc_bypass:1,
			     reserved16:3,
			     dc_len:3,
			     reserved17:5;
	} b;
};
union DTMB_FRONT_DAGC_TARGET_POWER_BITS {
	unsigned int d32;
	struct {
			unsigned int dagc_target_power_l:8,
			     dagc_target_power_h:8,
			     dagc_target_power_ler:8,
			     dagc_target_power_her:8;
	} b;
};
union DTMB_FRONT_ACF_BYPASS_BITS {
	unsigned int d32;
	struct {
			unsigned int coef65:11,
			     reserved18:1,
			     coef66:11,
			     reserved19:1,
			     acf_bypass:1,
			     reserved20:7;
	} b;
};
union DTMB_FRONT_COEF_SET1_BITS {
	unsigned int d32;
	struct {
			unsigned int coef63:11,
			     reserved21:1,
			     coef64:11,
			     reserved22:9;
	} b;
};
union DTMB_FRONT_COEF_SET2_BITS {
	unsigned int d32;
	struct {
			unsigned int coef62:10,
			     reserved23:22;
	} b;
};
union DTMB_FRONT_COEF_SET3_BITS {
	unsigned int d32;
	struct {
			unsigned int coef60:10,
			     reserved24:2,
			     coef61:10,
			     reserved25:10;
	} b;
};
union DTMB_FRONT_COEF_SET4_BITS {
	unsigned int d32;
	struct {
			unsigned int coef59:9,
			     reserved26:23;
	} b;
};
union DTMB_FRONT_COEF_SET5_BITS {
	unsigned int d32;
	struct {
			unsigned int coef57:9,
			     reserved27:3,
			     coef58:9,
			     reserved28:11;
	} b;
};
union DTMB_FRONT_COEF_SET6_BITS {
	unsigned int d32;
	struct {
			unsigned int coef54:8,
			     coef55:8,
			     coef56:8,
			     reserved29:8;
	} b;
};
union DTMB_FRONT_COEF_SET7_BITS {
	unsigned int d32;
	struct {
			unsigned int coef53:7,
			     reserved30:25;
	} b;
};
union DTMB_FRONT_COEF_SET8_BITS {
	unsigned int d32;
	struct {
			unsigned int coef49:7,
			     reserved31:1,
			     coef50:7,
			     reserved32:1,
			     coef51:7,
			     reserved33:1,
			     coef52:7,
			     reserved34:1;
	} b;
};
union DTMB_FRONT_COEF_SET9_BITS {
	unsigned int d32;
	struct {
			unsigned int coef45:7,
			     reserved35:1,
			     coef46:7,
			     reserved36:1,
			     coef47:7,
			     reserved37:1,
			     coef48:7,
			     reserved38:1;
	} b;
};
union DTMB_FRONT_COEF_SET10_BITS {
	unsigned int d32;
	struct {
			unsigned int coef42:6,
			     reserved39:2,
			     coef43:6,
			     reserved40:2,
			     coef44:6,
			     reserved41:10;
	} b;
};
union DTMB_FRONT_COEF_SET11_BITS {
	unsigned int d32;
	struct {
			unsigned int coef38:6,
			     reserved42:2,
			     coef39:6,
			     reserved43:2,
			     coef40:6,
			     reserved44:2,
			     coef41:6,
			     reserved45:2;
	} b;
};
union DTMB_FRONT_COEF_SET12_BITS {
	unsigned int d32;
	struct {
			unsigned int coef34:6,
			     reserved46:2,
			     coef35:6,
			     reserved47:2,
			     coef36:6,
			     reserved48:2,
			     coef37:6,
			     reserved49:2;
	} b;
};
union DTMB_FRONT_COEF_SET13_BITS {
	unsigned int d32;
	struct {
			unsigned int coef30:6,
			     reserved50:2,
			     coef31:6,
			     reserved51:2,
			     coef32:6,
			     reserved52:2,
			     coef33:6,
			     reserved53:2;
	} b;
};
union DTMB_FRONT_COEF_SET14_BITS {
	unsigned int d32;
	struct {
			unsigned int coef27:5,
			     reserved54:3,
			     coef28:5,
			     reserved55:3,
			     coef29:5,
			     reserved56:11;
	} b;
};
union DTMB_FRONT_COEF_SET15_BITS {
	unsigned int d32;
	struct {
			unsigned int coef23:5,
			     reserved57:3,
			     coef24:5,
			     reserved58:3,
			     coef25:5,
			     reserved59:3,
			     coef26:5,
			     reserved60:3;
	} b;
};
union DTMB_FRONT_COEF_SET16_BITS {
	unsigned int d32;
	struct {
			unsigned int coef19:5,
			     reserved61:3,
			     coef20:5,
			     reserved62:3,
			     coef21:5,
			     reserved63:3,
			     coef22:5,
			     reserved64:3;
	} b;
};
union DTMB_FRONT_COEF_SET17_BITS {
	unsigned int d32;
	struct {
			unsigned int coef15:5,
			     reserved65:3,
			     coef16:5,
			     reserved66:3,
			     coef17:5,
			     reserved67:3,
			     coef18:5,
			     reserved68:3;
	} b;
};
union DTMB_FRONT_COEF_SET18_BITS {
	unsigned int d32;
	struct {
			unsigned int coef08:4,
			     coef09:4,
			     coef10:4,
			     coef11:4,
			     coef12:4,
			     coef13:4,
			     coef14:4,
			     reserved69:4;
	} b;
};
union DTMB_FRONT_COEF_SET19_BITS {
	unsigned int d32;
	struct {
			unsigned int coef00:4,
			     coef01:4,
			     coef02:4,
			     coef03:4,
			     coef04:4,
			     coef05:4,
			     coef06:4,
			     coef07:4;
	} b;
};
union DTMB_FRONT_SRC_CONFIG1_BITS {
	unsigned int d32;
	struct {
			unsigned int src_norm_inrate:24,
			     src_tim_shr:4,
			     src_ted_disable:1,
			     reserved70:3;
	} b;
};
union DTMB_FRONT_SRC_CONFIG2_BITS {
	unsigned int d32;
	struct {
			unsigned int src_stable_timeout:4,
			     src_seg_len:3,
			     reserved71:1,
			     src_ted_beta:3,
			     reserved72:1,
			     src_time_err_thr:4,
			     src_time_mu1:5,
			     reserved73:3,
			     src_time_mu2:5,
			     reserved74:3;
	} b;
};
union DTMB_FRONT_SFIFO_OUT_LEN_BITS {
	unsigned int d32;
	struct {
			unsigned int sfifo_out_len:4,
			     reserved75:28;
	} b;
};
union DTMB_FRONT_DAGC_GAIN_BITS {
	unsigned int d32;
	struct {
			unsigned int dagc_bypass:1,
			     dagc_power_alpha:2,
			     dagc_bw:3,
			     dagc_gain_ctrl:12,
			     dagc_gain_step_er:6,
			     dagc_gain_step:6,
			     reserved76:2;
	} b;
};
union DTMB_FRONT_IQIB_STEP_BITS {
	unsigned int d32;
	struct {
			unsigned int iqib_step_b:2,
			     iqib_step_a:2,
			     iqib_period:3,
			     reserved77:1,
			     iqib_bypass:1,
			     reserved78:23;
	} b;
};
union DTMB_FRONT_IQIB_CONFIG_BITS {
	unsigned int d32;
	struct {
			unsigned int iqib_set_b:12,
			     iqib_set_a:10,
			     reserved79:2,
			     iqib_set_val:1,
			     iqib_hold:1,
			     reserved80:6;
	} b;
};
union DTMB_FRONT_ST_CONFIG_BITS {
	unsigned int d32;
	struct {
			unsigned int st_enable:1,
			     reserved81:3,
			     st_dc_len:3,
			     reserved82:1,
			     st_alpha:3,
			     reserved83:1,
			     st_Q_thrsh:8,
			     st_dist:3,
			     reserved84:1,
			     st_len:5,
			     reserved85:3;
	} b;
};
union DTMB_FRONT_ST_FREQ_BITS {
	unsigned int d32;
	struct {
			unsigned int st_freq_v:1,
			     st_freq_i:19,
			     reserved86:12;
	} b;
};

#endif
