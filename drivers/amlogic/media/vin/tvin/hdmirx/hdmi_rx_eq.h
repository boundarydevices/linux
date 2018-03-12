/*
 * drivers/amlogic/media/vin/tvin/hdmirx/hdmi_rx_eq.h
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

#ifndef _HDMI_RX_EQ_H
#define _HDMI_RX_EQ_H
/* time mS */
#define WaitTimeStartConditions	3
/* WAIT FOR, CDR LOCK and TMDSVALID */
#define sleep_time_CDR	10
/* Maximum slope  accumulator to consider the cable as a short cable */
#define AccLimit	360
/* Minimum slope accumulator to consider the following setting */
#define AccMinLimit	0
/* suitable for a long cable */
/* Maximum allowable setting, HW as the maximum = 15, should */
#define maxsetting	14
/* only be need for ultra long cables at high rates, */
/* condition never detected on LAB */
/* Default setting for short cables, */
/* if system cannot find any one better than this. */
#define shortcableSetting	4
/* Default setting when system cannot detect the cable type */
#define ErrorcableSetting	4
/* Minimum current slope needed to consider the cable */
/* as "very long" and therefore */
#define minSlope	50
/* max setting is suitable for equalization */
/* Maximum number of early-counter measures to average each setting */
#define avgAcq	4
/* Threshold Value for the statistics counter */
/* 3'd0: Selects counter threshold 1K */
/* 3'd1: Selects counter threshold 2K */
/* 3'd2: Selects counter threshold 4K */
/* 3d3: Selects counter threshold 8K */
/* 3d4: Selects counter threshold 16K */
/* Number of retries in case of algorithm ending with errors */
#define NTRYS	1
/* theoretical threshold for an equalized system */
#define equalizedCounterValue	512
/* theoretical threshold for an equalized system */
#define equalizedCounterValue_HDMI20	512
/* Maximum  difference between pairs */
#define MINMAX_maxDiff	4
/* Maximum  difference between pairs  under HDMI2.0 MODE */
#define MINMAX_maxDiff_HDMI20	2
/* FATBIT MASK FOR hdmi-1.4 xxxx_001 or xxxx_110 */
#define EQ_FATBIT_MASK	0x0000
/* hdmi 1.4 ( pll rate = 00) xx00_001 or xx11_110 */
#define EQ_FATBIT_MASK_4k	0x0c03
/* for hdmi2.0 x000_001 or x111_110 */
#define EQ_FATBIT_MASK_HDMI20	0x0e03

#define block_delay_ms(x) msleep_interruptible((x))

/*macro define end*/

/*--------------------------enum define---------------------*/
enum eq_states_e {
	EQ_ENABLE,
	EQ_MANUAL,
	EQ_USE_PRE,
	EQ_DISABLE,
};

enum phy_eq_channel_e {
	EQ_CH0,
	EQ_CH1,
	EQ_CH2,
	EQ_CH_NUM,
};

enum phy_eq_cmd_e {
	EQ_START = 0x1,
	EQ_STOP,
};

enum eq_sts_e {
	E_EQ_START,
	E_EQ_FINISH,
	E_EQ_PASS,
	E_EQ_SAME,
	E_EQ_FAIL
};

enum e_eq_freq {
	E_EQ_3G,
	E_EQ_HD,
	E_EQ_SD,
	E_EQ_6G = 4,
	E_EQ_FREQ
};

struct eq_cfg_coef_s {
	uint16_t fat_bit_sts;
	uint8_t min_max_diff;
};

enum eq_cable_type_e {
	E_CABLE_NOT_FOUND,
	E_LONG_CABLE,
	E_SHORT_CABLE,
	E_LONG_CABLE2,
	E_ERR_CABLE = 255,
};

struct st_eq_data {
	/* Best long cable setting */
	uint16_t bestLongSetting;
	/* long cable setting detected and valid */
	uint8_t validLongSetting;
	/* best short cable setting */
	uint16_t bestShortSetting;
	/* best short cable setting detected and valid */
	uint8_t validShortSetting;
	/* TMDS Valid for channel */
	uint8_t tmdsvalid;
	/* best setting to be programed */
	uint16_t bestsetting;
	/* Accumulator register */
	uint16_t acc;
	/* Acquisition register */
	uint16_t acq;
	uint16_t acq_n[15];
	uint16_t lastacq;
	uint8_t eq_ref[3];
};

/*struct define end*/
extern struct st_eq_data eq_ch0;
extern struct st_eq_data eq_ch1;
extern struct st_eq_data eq_ch2;
extern int delay_ms_cnt;
extern int eq_max_setting;
extern bool phy_pddq_en;
extern int eq_dbg_ch0;
extern int eq_dbg_ch1;
extern int eq_dbg_ch2;
extern int long_cable_best_setting;

/*--------------------------function declare------------------*/
int rx_eq_algorithm(void);
int hdmirx_phy_start_eq(void);
uint8_t SettingFinder(void);
bool eq_maxvsmin(int ch0Setting, int ch1Setting, int ch2Setting);

/* int hdmirx_phy_suspend_eq(void); */
bool hdmirx_phy_check_tmds_valid(void);
void hdmirx_phy_conf_eq_setting(int rx_port_sel,
	int ch0Setting,	int ch1Setting, int ch2Setting);
void eq_cfg(void);
void eq_run(void);
void rx_set_eq_run_state(enum eq_sts_e state);
enum eq_sts_e rx_get_eq_run_state(void);
extern void dump_eq_data(void);
extern void eq_dwork_handler(struct work_struct *work);

/*function declare end*/

#endif /*_HDMI_RX_EQ_H*/

