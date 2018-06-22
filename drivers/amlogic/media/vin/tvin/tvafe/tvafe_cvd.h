/*
 * drivers/amlogic/media/vin/tvin/tvafe/tvafe_cvd.h
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

#ifndef _TVAFE_CVD_H
#define _TVAFE_CVD_H

#include <linux/amlogic/media/frame_provider/tvin/tvin.h>

/***************************Local defines**********************************/
/* cvd2 memory size defines */
#define DECODER_MOTION_BUFFER_ADDR_OFFSET   0x70000
#define DECODER_MOTION_BUFFER_4F_LENGTH     0x15a60
/*motion is not use,only 3d-com need mem:1135x625x10bit/8 * 4 = 0x361ef8*/
#define DECODER_VBI_ADDR_OFFSET             0x400000/*0x86000*/
#define DECODER_VBI_SIZE                0x80000/*0x1000*/
#define DECODER_VBI_START_ADDR              0x0

/* vbi start line: unit is hcount value */
#define VBI_START_CC	0x54
#define VBI_START_WSS	0x54
#define VBI_START_TT	0x82
#define VBI_START_VPS	0x82

/* cvd2 function enable/disable defines*/
/* #define TVAFE_CVD2_NOT_TRUST_NOSIG  */
/* Do not trust Reg no signal flag */
/* #define SYNC_HEIGHT_AUTO_TUNING */

/* cvd2 function enable/disable defines*/
/* #define TVAFE_CVD2_TUNER_DET_ACCELERATED*/
/* accelerate tuner mode detection */

/* cvd2 VBI function enable/disable defines*/
/* #define TVAFE_CVD2_CC_ENABLE  // enable cvd2 close caption */

/* cvd2 auto adjust de enable/disable defines*/
#define TVAFE_CVD2_AUTO_DE_ENABLE
/* enable cvd2 de auto ajust */
#define TVAFE_CVD2_AUTO_DE_CHECK_CNT	100
/* check lines counter 100*10ms */
#define TVAFE_CVD2_AUTO_DE_TH	0xd0
#define TVAFE_CVD2_AUTO_VS_TH	0x6
/* audo de threshold */
#define TVAFE_CVD2_PAL_DE_START	0x17
/* default de start value for pal */

/* test with vlsi guys */
#define TVAFE_VS_VE_VAL		20
/**************************************** */
/* *** enum definitions *****************/
/* **************************************/
enum tvafe_cvd2_state_e {
	TVAFE_CVD2_STATE_INIT = 0,
	TVAFE_CVD2_STATE_FIND,
};

/* ****************************************** */
/* *** structure definitions *********** */
/***************************************************** */
struct tvafe_cvd2_hw_data_s {
	bool no_sig;
	bool h_lock;
	bool v_lock;
	bool h_nonstd;
	bool v_nonstd;
	bool no_color_burst;
	bool comb3d_off;
	bool chroma_lock;
	bool pal;
	bool secam;
	bool line625;
	bool noisy;
	bool vcr;
	bool vcrtrick;
	bool vcrff;
	bool vcrrew;
	unsigned char cordic;

	unsigned char acc4xx_cnt;
	unsigned char acc425_cnt;
	unsigned char acc3xx_cnt;
	unsigned char acc358_cnt;
	bool secam_detected;
	bool secam_phase;
	bool fsc_358;
	bool fsc_425;
	bool fsc_443;
};

/* cvd2 memory */
struct tvafe_cvd2_mem_s {
	unsigned int start;
	/* memory start addr for cvd2 module */
	unsigned int size;
	/* memory size for cvd2 module */
};

#ifdef TVAFE_CVD2_AUTO_DE_ENABLE
struct tvafe_cvd2_lines_s {
	unsigned int val[4];
	unsigned int check_cnt;
	unsigned int de_offset;
};
#endif

/* cvd2 signal information */
struct tvafe_cvd2_info_s {
	enum tvafe_cvd2_state_e state;
	unsigned int state_cnt;
#ifdef TVAFE_SET_CVBS_CDTO_EN
	unsigned int hcnt64[4];
	unsigned int hcnt64_cnt;
#endif
	unsigned int hs_adj_level;
	unsigned int vs_adj_level;
#ifdef TVAFE_SET_CVBS_PGA_EN
	unsigned short dgain[4];
	unsigned short dgain_cnt;
#endif
	unsigned int comb_check_cnt;
	unsigned int fmt_shift_cnt;
	bool non_std_enable;
	bool non_std_config;
	bool non_std_worst;
	bool adc_reload_en;
	bool hs_adj_en;
	bool vs_adj_en;
	/*0:+;1:-*/
	bool hs_adj_dir;

#ifdef TVAFE_CVD2_AUTO_DE_ENABLE
	struct tvafe_cvd2_lines_s vlines;
#endif
	unsigned int ntsc_switch_cnt;
};

/* CVD2 status list */
struct tvafe_cvd2_s {
	struct tvafe_cvd2_hw_data_s hw_data[3];
	struct tvafe_cvd2_hw_data_s hw;
	struct tvafe_cvd2_info_s info;
	unsigned int fmt_loop_cnt;
	unsigned char hw_data_cur;
	enum tvin_port_e vd_port;
	enum tvin_sig_fmt_e config_fmt;
	enum tvin_sig_fmt_e manual_fmt;
	bool cvd2_init_en;
	bool nonstd_detect_dis;
};

/* ***************************************** */
/* ******** GLOBAL FUNCTION CLAIM ******** */
/* ********************************************* */
extern int cvd_get_rf_strength(void);

extern void tvafe_cvd2_try_format(struct tvafe_cvd2_s *cvd2,
			struct tvafe_cvd2_mem_s *mem, enum tvin_sig_fmt_e fmt);
extern bool tvafe_cvd2_no_sig(struct tvafe_cvd2_s *cvd2,
			struct tvafe_cvd2_mem_s *mem);
extern bool tvafe_cvd2_fmt_chg(struct tvafe_cvd2_s *cvd2);
extern enum tvin_sig_fmt_e tvafe_cvd2_get_format(
			struct tvafe_cvd2_s *cvd2);
#ifdef TVAFE_SET_CVBS_PGA_EN
extern void tvafe_cvd2_adj_pga(struct tvafe_cvd2_s *cvd2);
#endif
#ifdef TVAFE_SET_CVBS_CDTO_EN
extern void tvafe_cvd2_adj_cdto(struct tvafe_cvd2_s *cvd2,
			unsigned int hcnt64);
#endif
extern void tvafe_cvd2_adj_hs(struct tvafe_cvd2_s *cvd2,
			unsigned int hcnt64);
extern void tvafe_cvd2_adj_hs_ntsc(struct tvafe_cvd2_s *cvd2,
			unsigned int hcnt64);

extern void tvafe_cvd2_set_default_cdto(struct tvafe_cvd2_s *cvd2);
extern void tvafe_cvd2_set_default_de(struct tvafe_cvd2_s *cvd2);
extern void tvafe_cvd2_check_3d_comb(struct tvafe_cvd2_s *cvd2);
extern void tvafe_cvd2_reset_pga(void);
extern enum tvafe_cvbs_video_e tvafe_cvd2_get_lock_status(
			struct tvafe_cvd2_s *cvd2);
extern int tvafe_cvd2_get_atv_format(void);
extern int tvafe_cvd2_get_hv_lock(void);
extern void tvafe_cvd2_hold_rst(void);
extern void tvafe_cvd2_set_reg8a(unsigned int v);
extern void tvafe_snow_config(unsigned int onoff);
extern void tvafe_snow_config_clamp(unsigned int onoff);
extern void tvafe_snow_config_acd(void);
extern void tvafe_snow_config_acd_resume(void);
extern enum tvin_aspect_ratio_e tvafe_cvd2_get_wss(void);
extern void tvafe_cvd2_get_signal_status(struct tvafe_cvd2_s *cvd2);
extern void cvd_vbi_mem_set(unsigned int offset, unsigned int size);
extern void cvd_vbi_config(void);
extern void tvafe_cvd2_rf_ntsc50_en(bool v);

extern bool tvafe_snow_function_flag;

#endif /* _TVAFE_CVD_H */

