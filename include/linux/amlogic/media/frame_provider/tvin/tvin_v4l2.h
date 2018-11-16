/*
 * include/linux/amlogic/media/frame_provider/tvin/tvin_v4l2.h
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

#ifndef __TVIN_V4L2_H
#define __TVIN_V4L2_H
#include "tvin.h"

/*
 *   below macro defined applied to camera driver
 */
enum camera_light_mode_e {
	ADVANCED_AWB = 0,
	SIMPLE_AWB,
	MANUAL_DAY,
	MANUAL_A,
	MANUAL_CWF,
	MANUAL_CLOUDY,
};

enum camera_saturation_e {
	SATURATION_N4_STEP = 0,
	SATURATION_N3_STEP,
	SATURATION_N2_STEP,
	SATURATION_N1_STEP,
	SATURATION_0_STEP,
	SATURATION_P1_STEP,
	SATURATION_P2_STEP,
	SATURATION_P3_STEP,
	SATURATION_P4_STEP,
};

enum camera_brightness_e {
	BRIGHTNESS_N4_STEP = 0,
	BRIGHTNESS_N3_STEP,
	BRIGHTNESS_N2_STEP,
	BRIGHTNESS_N1_STEP,
	BRIGHTNESS_0_STEP,
	BRIGHTNESS_P1_STEP,
	BRIGHTNESS_P2_STEP,
	BRIGHTNESS_P3_STEP,
	BRIGHTNESS_P4_STEP,
};

enum camera_contrast_e {
	CONTRAST_N4_STEP = 0,
	CONTRAST_N3_STEP,
	CONTRAST_N2_STEP,
	CONTRAST_N1_STEP,
	CONTRAST_0_STEP,
	CONTRAST_P1_STEP,
	CONTRAST_P2_STEP,
	CONTRAST_P3_STEP,
	CONTRAST_P4_STEP,
};

enum camera_hue_e {
	HUE_N180_DEGREE = 0,
	HUE_N150_DEGREE,
	HUE_N120_DEGREE,
	HUE_N90_DEGREE,
	HUE_N60_DEGREE,
	HUE_N30_DEGREE,
	HUE_0_DEGREE,
	HUE_P30_DEGREE,
	HUE_P60_DEGREE,
	HUE_P90_DEGREE,
	HUE_P120_DEGREE,
	HUE_P150_DEGREE,
};

enum camera_special_effect_e {
	SPECIAL_EFFECT_NORMAL = 0,
	SPECIAL_EFFECT_BW,
	SPECIAL_EFFECT_BLUISH,
	SPECIAL_EFFECT_SEPIA,
	SPECIAL_EFFECT_REDDISH,
	SPECIAL_EFFECT_GREENISH,
	SPECIAL_EFFECT_NEGATIVE,
};

enum camera_exposure_e {
	EXPOSURE_N4_STEP = 0,
	EXPOSURE_N3_STEP,
	EXPOSURE_N2_STEP,
	EXPOSURE_N1_STEP,
	EXPOSURE_0_STEP,
	EXPOSURE_P1_STEP,
	EXPOSURE_P2_STEP,
	EXPOSURE_P3_STEP,
	EXPOSURE_P4_STEP,
};

enum camera_sharpness_e {
	SHARPNESS_1_STEP = 0,
	SHARPNESS_2_STEP,
	SHARPNESS_3_STEP,
	SHARPNESS_4_STEP,
	SHARPNESS_5_STEP,
	SHARPNESS_6_STEP,
	SHARPNESS_7_STEP,
	SHARPNESS_8_STEP,
	SHARPNESS_AUTO_STEP,
};

enum camera_mirror_flip_e {
	MF_NORMAL = 0,
	MF_MIRROR,
	MF_FLIP,
	MF_MIRROR_FLIP,
};

enum camera_wb_flip_e {
	CAM_WB_AUTO = 0,
	CAM_WB_CLOUD,
	CAM_WB_DAYLIGHT,
	CAM_WB_INCANDESCENCE,
	CAM_WB_TUNGSTEN,
	CAM_WB_FLUORESCENT,
	CAM_WB_MANUAL,
	CAM_WB_SHADE,
	CAM_WB_TWILIGHT,
	CAM_WB_WARM_FLUORESCENT,
};

enum camera_focus_mode_e {
	CAM_FOCUS_MODE_RELEASE = 0,
	CAM_FOCUS_MODE_FIXED,
	CAM_FOCUS_MODE_INFINITY,
	CAM_FOCUS_MODE_AUTO,
	CAM_FOCUS_MODE_MACRO,
	CAM_FOCUS_MODE_EDOF,
	CAM_FOCUS_MODE_CONTI_VID,
	CAM_FOCUS_MODE_CONTI_PIC,
};

/* removed this when move to new v4l2 */
#define V4L2_CID_AUTO_FOCUS_START               (V4L2_CID_CAMERA_CLASS_BASE+28)
#define V4L2_CID_AUTO_FOCUS_STOP                (V4L2_CID_CAMERA_CLASS_BASE+29)
#define V4L2_CID_AUTO_FOCUS_STATUS              (V4L2_CID_CAMERA_CLASS_BASE+30)
#define V4L2_AUTO_FOCUS_STATUS_IDLE             (0 << 0)
#define V4L2_AUTO_FOCUS_STATUS_BUSY             (1 << 0)
#define V4L2_AUTO_FOCUS_STATUS_REACHED          (1 << 1)
#define V4L2_AUTO_FOCUS_STATUS_FAILED           (1 << 2)

/* removed this when move to new v4l2 */

enum camera_night_mode_flip_e {
	CAM_NM_AUTO = 0,
	CAM_NM_ENABLE,
};

enum camera_effect_flip_e {
	CAM_EFFECT_ENC_NORMAL = 0,
	CAM_EFFECT_ENC_GRAYSCALE,
	CAM_EFFECT_ENC_SEPIA,
	CAM_EFFECT_ENC_SEPIAGREEN,
	CAM_EFFECT_ENC_SEPIABLUE,
	CAM_EFFECT_ENC_COLORINV,
};

enum camera_banding_flip_e {
	CAM_BANDING_DISABLED = 0,
	CAM_BANDING_50HZ,
	CAM_BANDING_60HZ,
	CAM_BANDING_AUTO,
	CAM_BANDING_OFF,
};

struct camera_info_s {
	const char *camera_name;
	enum camera_saturation_e saturation;
	enum camera_brightness_e brighrness;
	enum camera_contrast_e contrast;
	enum camera_hue_e hue;
	/* enum camera_special_effect_e special_effect; */
	enum camera_exposure_e exposure;
	enum camera_sharpness_e sharpness;
	enum camera_mirror_flip_e mirro_flip;
	enum tvin_sig_fmt_e resolution;
	enum camera_wb_flip_e white_balance;
	enum camera_night_mode_flip_e night_mode;
	enum camera_effect_flip_e effect;
	int qulity;
};

/* ---------- enum ---------- */

/* LPF responding time: cycles to reach 90% target */
enum xml_resp_s {
	XML_RESP_0 = 0,		/* immediately */
	XML_RESP_1,		/* 10 cycles */
	XML_RESP_2,		/* 20 cycles */
	XML_RESP_3,		/* 50 cycles */
	XML_RESP_4,		/* 100 cycles */
	XML_RESP_5,		/* 200 cycles */
	XML_RESP_6,		/* 400 cycles */
	XML_RESP_7,		/* 800 cycles */
	XML_RESP_8,		/* 1600 cycles */
	XML_RESP_9,		/* 3200 cycles */
};

enum cam_scanmode_e {
	CAM_SCANMODE_NULL = 0,	/* turn off af */
	CAM_SCANMODE_PROBE,
	CAM_SCANMODE_FULL,
};

/*state for cmd*/
enum cam_cmd_state_e {
	CAM_STATE_NULL,
	CAM_STATE_DOING,
	CAM_STATE_ERROR,
	CAM_STATE_SUCCESS,
};

enum cam_command_e {
	/* common */
	CAM_COMMAND_INIT = 0,
	CAM_COMMAND_GET_STATE,
	CAM_COMMAND_SCENES,
	CAM_COMMAND_EFFECT,
	CAM_COMMAND_AWB,
	CAM_COMMAND_MWB,
	CAM_COMMAND_SET_WORK_MODE,
	/* ae related */
	CAM_COMMAND_AE_ON,
	CAM_COMMAND_AE_OFF,
	CAM_COMMAND_SET_AE_LEVEL,
	/* af related */
	CAM_COMMAND_AF,
	CAM_COMMAND_FULLSCAN,
	CAM_COMMAND_TOUCH_FOCUS,
	CAM_COMMAND_CONTINUOUS_FOCUS_ON,
	CAM_COMMAND_CONTINUOUS_FOCUS_OFF,
	CAM_COMMAND_BACKGROUND_FOCUS_ON,
	CAM_COMMAND_BACKGROUND_FOCUS_OFF,
	/* flash related */
	CAM_COMMAND_SET_FLASH_MODE,
	/* torch related */
	CAM_COMMAND_TORCH,
	/* bypass isp for raw data */
	CMD_ISP_BYPASS,
};
extern const char *cam_cmd_to_str(enum cam_command_e cmd);

/* ---------- xml struct ---------- */

/* all "0" means no vcm */
struct xml_vcm_s {
	unsigned short vcm_max;
	unsigned short vcm_min;
	unsigned short vcm_reseponding_time;
	/* in the unit of mS, responding_cycle =*/
	/*responding_time/sensor_frame_rate + 1, 1 for IIC and so on */
};

/* "0" means no flash */
enum flash_mode_s {
	FLASH_MODE_NULL = 0,	/* no use flash */
	FLASH_MODE_ON,
	FLASH_MODE_OFF,
	FLASH_MODE_AUTO,
};

#define WAVE_PARM_NUM		12
struct wave_s {
	unsigned int torch_rising_time;
	unsigned int flash_rising_time;
	unsigned int torch_flash_ratio;
	unsigned int wave_clock_div;	/* u16 */
	unsigned int pulse_init_time;	/* u11 */
	unsigned int pulse_high_time;	/* u11 */
	unsigned int pulse_low_time;	/* u11 */
	unsigned int time_to_latch;	/* u26 */
	unsigned int latch_time;	/* u26 */
	unsigned int latch_time_timeout;	/* u26 */
	unsigned int time_to_off;	/* u11 */
	unsigned int pulse_qty_max;	/* u8 */
};

struct xml_window_s {
	unsigned char ratio_x0;	/* 0 ~ 255, x0 = (format.h * ratio_x0) >> 8 */
	unsigned char ratio_y0;	/* 0 ~ 255, y0 = (format.v * ratio_y0) >> 8 */
	unsigned char ratio_x1;	/* 0 ~ 255, x1 = (format.h * ratio_x1) >> 8 */
	unsigned char ratio_y1;	/* 0 ~ 255, y1 = (format.v * ratio_y1) >> 8 */
};

#define AE_PARM_NUM			67
struct xml_algorithm_ae_s {
	unsigned int ae_algorithm;	/* 0:basic;    1:enhanced */
	unsigned int ae_statistics[3];	/* 0: false, 1: true */
	unsigned int ae_exp[3];	/* 0: false, 1: true */
	unsigned int ae_ag[3];	/* 0: false, 1: true */
	unsigned int ae_skip[3];	/* 0: false, 1: true */
	unsigned int ratio_winl;	/* 0 ~ 1024 */
	unsigned int ratio_winr;	/* 0 ~ 1024 */
	unsigned int ratio_wint;	/* 0 ~ 1024 */
	unsigned int ratio_winb;	/* 0 ~ 1024 */
	unsigned int alert_mode;	/* 0: disable, 1: enable */
	unsigned int tune_mode;	/* 0: average mode, 1: blind up mode */
	unsigned int ratio_r;	/* 0 ~ 255 */
	unsigned int ratio_g;	/* 0 ~ 255 */
	unsigned int ratio_b;	/* 0 ~ 255 */
	unsigned int stepdnr;	/* 0 ~ 255 */
	unsigned int stepdng;	/* 0 ~ 255 */
	unsigned int stepdnb;	/* 0 ~ 255 */
	unsigned int stepup;	/* 0 ~ 255 */
	unsigned int slow_lpfcoef;	/* 0 ~ 255 */
	unsigned int fast_lpfcoef;	/* 0 ~ 255 */
	unsigned int coef_cur[16];	/* 0 ~ 1023 */
	unsigned int coef_env[16];	/* 0 ~ 1023 */
	unsigned int env_hign;	/* 0 ~ 255 */
	unsigned int env_hign2mid;	/* 0 ~ 255 */
	unsigned int env_low2mid;	/* 0 ~ 255 */
	unsigned int env_low;	/* 0 ~ 255 */
	unsigned int thr_r_high;	/* 0 ~ 255 */
	unsigned int thr_r_mid;	/* 0 ~ 255 */
	unsigned int thr_r_low;	/* 0 ~ 255 */
	unsigned int thr_g_high;	/* 0 ~ 255 */
	unsigned int thr_g_mid;	/* 0 ~ 255 */
	unsigned int thr_g_low;	/* 0 ~ 255 */
	unsigned int thr_b_high;	/* 0 ~ 255 */
	unsigned int thr_b_mid;	/* 0 ~ 255 */
	unsigned int thr_b_low;	/* 0 ~ 255 */
	unsigned int lpftype_high;	/* 0 ~ 255 */
	unsigned int lpftype_mid;	/* 0 ~ 255 */
	unsigned int lpftype_low;	/* 0 ~ 255 */
	unsigned int targethigh;	/* 0 ~ 255 */
	unsigned int targetmid;	/* 0 ~ 255 */
	unsigned int targetlow;	/* 0 ~ 255 */
	unsigned int radium_inner_h;	/* 0 ~ 255 */
	unsigned int radium_inner_m;	/* 0 ~ 255 */
	unsigned int radium_inner_l;	/* 0 ~ 255 */
	unsigned int radium_outer_h;	/* 0 ~ 255 */
	unsigned int radium_outer_m;	/* 0 ~ 255 */
	unsigned int radium_outer_l;	/* 0 ~ 255 */
	unsigned int flash_thr;	/* 0 ~ 255 */

	/***********************AE_ENH********************************/

	unsigned int ratio_histr;	/* 0 ~1023 */
	unsigned int ratio_histg;	/* 0 ~1023 */
	unsigned int ratio_histb;	/* 0 ~1023 */
	unsigned int target_r;	/* 0 ~ 255 */
	unsigned int target_g;	/* 0 ~ 255 */
	unsigned int target_b;	/* 0 ~ 255 */
	unsigned int maxrate_inner;	/* 0 ~1023 */
	unsigned int maxrate_outer;	/* 0 ~1023 */
	unsigned int slow_lpfcoef_enh;	/* 0 ~ 255 */
	unsigned int fast_lpfcoef_enh;	/* 0 ~ 255 */
	unsigned int flash_thr_enh;	/* 0 ~ 255 */

	/***********************AE_ADD********************************/
	unsigned int ae_ratio_low;	/* 0 ~ 1024             0x00000005 */
	unsigned int ae_ratio_low2mid;	/* 0 ~ 1024             0x0000000f */
	unsigned int ae_ratio_mid2high;	/* 0 ~ 1024             0x0000001e */
	unsigned int ae_ratio_high;	/* 0 ~ 1024             0x00000028 */
	unsigned int ae_min_diff;	/* 0 ~ 255              0x00000032 */
	unsigned int ae_max_diff;	/* 0 ~ 255              0x0000000f */

	unsigned int reserve[16];
	unsigned int aet_fmt_gain;	/* 0db for each fmt */

};

#define AWB_PARM_NUM			58
struct xml_algorithm_awb_s {
	unsigned int awb_algorithm;	/* 0:basic;    1:enhanced */
	unsigned int ratio_winl;	/* 0 ~ 1024 */
	unsigned int ratio_winr;	/* 0 ~ 1024 */
	unsigned int ratio_wint;	/* 0 ~ 1024 */
	unsigned int ratio_winb;	/* 0 ~ 1024 */
	unsigned int ratio_rgb;	/* 0 ~ 255 */
	unsigned int ratio_yh;	/* 0 ~ 255 */
	unsigned int ratio_ym;	/* 0 ~ 255 */
	unsigned int ratio_yl;	/* 0 ~ 255 */
	unsigned int yyh;	/* 0 ~ 255 */
	unsigned int yym;	/* 0 ~ 255 */
	unsigned int yyl;	/* 0 ~ 255 */
	unsigned int coef_r[4];	/* 0 ~ 255 */
	unsigned int coef_g[4];	/* 0 ~ 255 */
	unsigned int coef_b[4];	/* 0 ~ 255 */
	unsigned int inner_rg;	/* 0 ~ 1023 */
	unsigned int inner_bg;	/* 0 ~ 1023 */
	unsigned int outer_rg;	/* 0 ~ 1023 */
	unsigned int outer_bg;	/* 0 ~ 1023 */
	unsigned int r_max;	/* 0 ~ 4095 */
	unsigned int r_min;	/* 0 ~ 4095 */
	unsigned int b_max;	/* 0 ~ 4095 */
	unsigned int b_min;	/* 0 ~ 4095 */
	unsigned int thr_gb_h;	/* 0 ~ 255 */
	unsigned int thr_gb_m;	/* 0 ~ 255 */
	unsigned int thr_gb_l;	/* 0 ~ 255 */
	unsigned int thr_gr_h;	/* 0 ~ 255 */
	unsigned int thr_gr_m;	/* 0 ~ 255 */
	unsigned int thr_gr_l;	/* 0 ~ 255 */
	unsigned int thr_br_h;	/* 0 ~ 255 */
	unsigned int thr_br_m;	/* 0 ~ 255 */
	unsigned int thr_br_l;	/* 0 ~ 255 */
	unsigned int thr_du_h;	/* 0 ~ 255 */
	unsigned int thr_du_m;	/* 0 ~ 255 */
	unsigned int thr_du_l;	/* 0 ~ 255 */
	unsigned int thr_dv_h;	/* 0 ~ 255 */
	unsigned int thr_dv_m;	/* 0 ~ 255 */
	unsigned int thr_dv_l;	/* 0 ~ 255 */
	unsigned int thr_yh_h;	/* 0 ~ 255 */
	unsigned int thr_yh_m;	/* 0 ~ 255 */
	unsigned int thr_yh_l;	/* 0 ~ 255 */
	unsigned int thr_yl_h;	/* 0 ~ 255 */
	unsigned int thr_yl_m;	/* 0 ~ 255 */
	unsigned int thr_yl_l;	/* 0 ~ 255 */

	/*********************awb_enh****************/
	unsigned int ratio_yuv;
	unsigned int slow_lpfcoef;	/* 0 ~ 255 */
	unsigned int fast_lpfcoef;	/* 0 ~ 255 */
	unsigned int outer;	/* 0 ~ 1023 */
	unsigned int inner;	/* 0 ~ 1023 */
	unsigned int rw_limith;	/* 0 ~ 4095 */
	unsigned int rw_limitl;	/* 0 ~ 4095 */
	unsigned int gw_limith;	/* 0 ~ 4095 */
	unsigned int gw_limitl;	/* 0 ~ 4095 */
	unsigned int bw_limith;	/* 0 ~ 4095 */
	unsigned int bw_limitl;	/* 0 ~ 4095 */
	unsigned int thr_u[20];	/* 0 ~ 255 */
	unsigned int thr_v[20];	/* 0 ~ 255 */
	unsigned int reserve[16];
};

#define AF_PARM_NUM			13

#define FOCUS_GRIDS 16

struct xml_algorithm_af_s {
	/*for lose focus */
	unsigned int enter_static_ratio;	/* 10bit/1024 */
	unsigned int detect_step_cnt;
	unsigned int ave_vdc_thr;	/* the threshold of enter move */
	/*full scan & detect window ratio */
	/* cut 4 border in top bottom left right widht=1/ratio */
	unsigned int win_ratio;
	/*for climbing algorithm */
	unsigned int step[FOCUS_GRIDS];
	unsigned int valid_step_cnt;
	unsigned int jump_offset;
	unsigned int field_delay;
	/*window for touch focus */
	unsigned int x;		/* x coord of touch focus win */
	unsigned int y;		/* y coord of touch focus win */
	unsigned int radius_ratio;	/* radius of touch focus win */
	unsigned int hillside_fall;
	unsigned int reserve[15];
};

#define XML_LUT_LS 1025		/* 32*32 32-bit */
struct xml_lut_ls_s {
	unsigned int reg_map[XML_LUT_LS];
};

#define XML_LUT_GC 257		/* 257*3 10-bit */
struct xml_lut_gc_s {
	unsigned short gamma_r[XML_LUT_GC];
	unsigned short gamma_g[XML_LUT_GC];
	unsigned short gamma_b[XML_LUT_GC];
};

#define XML_TOP 9		/* top */
struct xml_top_s {
	unsigned int reg_map[XML_TOP];
};

#define XML_TP 20		/* test pattern */
struct xml_tp_s {
	unsigned int reg_map[XML_TP];
};

#define XML_CG 6		/* clamp & gain */
struct xml_cg_s {
	unsigned int reg_map[XML_CG];
};

#define XML_LS 5		/* lens shielding */
struct xml_ls_s {
	unsigned int reg_map[XML_LS];
};

#define XML_GC 1		/* gamma curve */
struct xml_gc_s {
	unsigned int reg_map[XML_GC];
};

#define XML_DP 11		/* defect pixel */
struct xml_dp_s {
	unsigned int reg_map[XML_DP];
};

#define XML_DM 2		/* demosaicing */
struct xml_dm_s {
	unsigned int reg_map[XML_DM];
};

#define XML_CSC 9		/* colr space conversion */
struct xml_csc_s {
	unsigned int reg_map[XML_CSC << 1];	/* sd/hd sensitive */
};

#define XML_NR 13		/* noise reduction */
struct xml_nr_s {
	unsigned int reg_map[XML_NR];
};

#define XML_SH 33		/* shanrpness */
struct xml_sharp_s {
	unsigned int reg_map[XML_SH];
};

#define XML_DBG 2		/* debug */
struct xml_dbg_s {
	unsigned int reg_map[XML_DBG];
};

#define XML_BN 3		/* black level & noise meter */
struct xml_bn_s {
	unsigned int reg_map[XML_BN];
};

#define XML_AE 6		/* auto explosure statistics */
struct xml_ae_s {
	unsigned int reg_map[XML_AE];
};

#define XML_AWB 5		/* auto white balance statistics */
struct xml_awb_s {
	unsigned int reg_map[XML_AWB];
};

#define XML_AF 17		/* auto focus statistics */
struct xml_af_s {
	unsigned int reg_map[XML_AF];
};

/*
 *#define XML_WAVE 1 // wave generatore
 *struct xml_wave_s {
 *	unsigned int reg_map[XML_WAVE];
 *} xml_wave_t;
 */
struct xml_peripheral_s {
	/* struct xml_sensor_s sensor; */
	/* struct xml_ae_level_s  ae_level; */
	struct xml_vcm_s vcm;
};

struct xml_default_regs_s {
	struct xml_top_s top;
	struct xml_tp_s tp;	/* disable */
	struct xml_cg_s cg;	/* straight */
	struct xml_ls_s ls;
	struct xml_gc_s gc;
	struct xml_dp_s dp;	/* w/o static lut */
	struct xml_dm_s dm;
	struct xml_csc_s csc;	/* RGB->YUV */
	struct xml_nr_s nr;	/* disable */
	struct xml_sharp_s sharp;	/* disable */
	struct xml_dbg_s dbg;	/* disable */
	struct xml_bn_s bn;	/* disable */
	struct xml_ae_s ae_reg;
	struct xml_awb_s awb_reg;
	struct xml_af_s af_reg;
	struct xml_lut_ls_s lnsd;
	struct xml_lut_gc_s lut_gc;
};

struct xml_scenes_s {
	struct xml_algorithm_ae_s ae;
	struct xml_algorithm_awb_s awb;
	struct xml_algorithm_af_s af;
};

/*only G0 R1 B2 G3*/
struct xml_wb_manual_s {
	unsigned int reg_map[2];
};

struct xml_effect_manual_s {
	struct xml_csc_s csc;
	/* RGB->YUV with effect: r, g, b,*/
	/*brightness, contrast, hue, saturation, y_mirror, ... */
};

/* ---------- camera struct ---------- */

/* start tvin service will get format information */

/*
 *struct cam_format_s {
 *    unsigned short h;
 *   unsigned short v;
 *   unsigned short frame_time; //
 *  //in the unit of uS, for example, frame_time = 40(mS) means 25Hz format.
 *} cam_format_t;
 */
struct cam_function_s {
	bool (*set_af_new_step)(void *priv, unsigned int af_debug_control);
	unsigned int (*get_aet_current_step)(void *priv);
	unsigned int (*get_aet_current_gain)(void *priv);
	unsigned int (*get_aet_min_gain)(void *priv);
	unsigned int (*get_aet_max_gain)(void *priv);
	unsigned int (*get_aet_max_step)(void *priv);
	unsigned int (*get_aet_gain_by_step)(void *priv,
		unsigned int new_step);
	 bool (*set_aet_new_step)(void *priv, unsigned int new_step,
		bool exp_mode, bool ag_mode);
	 bool (*check_mains_freq)(void *priv);
	void *priv_data;
};

struct cam_manual_s {
	unsigned short ae_step;
	unsigned short focus;
	unsigned short torch;
};

struct cam_window_s {
	unsigned short x0;
	unsigned short y0;
	unsigned short x1;
	unsigned short y1;
};

#define CAP_PARM_NUM			8
struct xml_capture_s {
	unsigned int ae_try_max_cnt;
	unsigned int sigle_count;
	unsigned int skip_step;
	unsigned int multi_capture_num;
	enum cam_scanmode_e af_mode;
	unsigned int eyetime;	/* ms */
	unsigned int pretime;	/* ms */
	unsigned int postime;	/* ms */
};

/*for isp work mode*/
enum camera_mode_e {
	CAMERA_PREVIEW,
	CAMERA_CAPTURE,
	CAMERA_RECORD,
};

/*this parameter must be passed to vdin when stream on*/
struct cam_parameter_s {
	enum cam_command_e cam_command;
	enum cam_scanmode_e cam_scanmode;
	struct cam_function_s cam_function;
	struct cam_manual_s cam_manual;
	struct cam_window_s cam_touch_window;
	struct xml_peripheral_s *xml_peripheral;
	struct xml_scenes_s *xml_scenes;
	struct xml_default_regs_s *xml_regs_map;
	struct xml_effect_manual_s *xml_effect_manual;
	struct xml_wb_manual_s *xml_wb_manual;
	struct xml_capture_s *xml_capture;
	struct wave_s *xml_wave;
	unsigned int level;	/* the torch light level */
	enum flash_mode_s flash_mode;	/* the flash mode */
	enum camera_mode_e cam_mode;	/* set the isp work mode */
	int exposure_level;	/* manual exposure level 2db by each step */
};

struct isp_status_s {
	unsigned short focus;
	bool fullscan_done;
};

enum vdin_format_convert_e {
	VDIN_MATRIX_XXX_YUV_BLACK = 0,
	VDIN_FORMAT_CONVERT_YUV_YUV422,
	VDIN_FORMAT_CONVERT_YUV_YUV444,
	VDIN_FORMAT_CONVERT_YUV_RGB,
	VDIN_FORMAT_CONVERT_YUV_GBR,
	VDIN_FORMAT_CONVERT_YUV_BRG,
	VDIN_FORMAT_CONVERT_RGB_YUV422,
	VDIN_FORMAT_CONVERT_GBR_YUV422,
	VDIN_FORMAT_CONVERT_BRG_YUV422,
	VDIN_FORMAT_CONVERT_RGB_YUV444,
	VDIN_FORMAT_CONVERT_RGB_RGB,
	VDIN_FORMAT_CONVERT_YUV_NV12,
	VDIN_FORMAT_CONVERT_YUV_NV21,
	VDIN_FORMAT_CONVERT_RGB_NV12,
	VDIN_FORMAT_CONVERT_RGB_NV21,
	VDIN_FORMAT_CONVERT_MAX,
};

enum vdin_cmd_e {
	VDIN_CMD_NULL = 0,
	VDIN_CMD_SET_CSC,
	VDIN_CMD_SET_CM2,
	VDIN_CMD_ISR,
	VDIN_CMD_MPEGIN_START,
	VDIN_CMD_GET_HISTGRAM,
	VDIN_CMD_MPEGIN_STOP,
	VDIN_CMD_FORCE_GO_FIELD,
};

struct vdin_arg_s {
	enum vdin_cmd_e cmd;
	unsigned int h_active;
	unsigned int v_active;
	unsigned char matrix_id;
	enum vdin_format_convert_e color_convert;
	unsigned int *cm2;
	void *private;
};

enum bt_path_e {
	BT_PATH_GPIO = 0,
	BT_PATH_CSI2,
	BT_PATH_GPIO_B,
};

enum clk_channel_e {
	CLK_CHANNEL_A = 0,
	CLK_CHANNEL_B,
};

enum cam_interface_e {
	CAM_DVP = 0,
	CAM_MIPI,
};

#define PARAM_STATE_NULL			0x00000000
#define PARAM_STATE_HISTGRAM		0x00000001
#define PARAM_STATE_SCREENCAP		0x00000002

/* *********************************************************************** */

/* *** IOCTL command definitions ***************************************** */

/* *********************************************************************** */

#define CAMERA_IOC_MAGIC 'C'

#define CAMERA_IOC_START        \
	_IOW(CAMERA_IOC_MAGIC, 0x01, struct camera_info_s)
#define CAMERA_IOC_STOP         \
	_IO(CAMERA_IOC_MAGIC, 0x02)
#define CAMERA_IOC_SET_PARA     \
	_IOW(CAMERA_IOC_MAGIC, 0x03, struct camera_info_s)
#define CAMERA_IOC_GET_PARA     \
	_IOR(CAMERA_IOC_MAGIC, 0x04, struct camera_info_s)
#define CAMERA_IOC_START_CAPTURE_PARA     \
	_IOR(CAMERA_IOC_MAGIC, 0x05, struct camera_info_s)
#define CAMERA_IOC_STOP_CAPTURE_PARA \
	_IOR(CAMERA_IOC_MAGIC, 0x06, struct camera_info_s)

struct csi_parm_s {
	/* am_csi2_hw_t *hw_info; */
	unsigned char lanes;
	unsigned char channel;
	unsigned char mode;
	unsigned char clock_lane_mode;	/* 0 clock gate 1: always on */
	unsigned int active_pixel;
	unsigned int active_line;
	unsigned int frame_size;
	unsigned int ui_val;	/* ns */
	unsigned int hs_freq;	/* hz */
	unsigned int urgent;
	unsigned int settle;

	unsigned int lane_mask;

	enum clk_channel_e clk_channel;
	unsigned int skip_frames;
	enum tvin_color_fmt_e csi_ofmt;
};

/* add for vdin called by backend driver */
struct vdin_parm_s {
	enum tvin_port_e port;
	/* >max:use the information*/
	/*  from parameter rather than format table */
	enum tvin_sig_fmt_e fmt;
	/* for camera input mainly,the data sequence is different */
	enum tvin_color_fmt_e cfmt;
	enum tvin_scan_mode_e scan_mode; /* 1: progressive 2:interlaced */
	unsigned short h_active;
	unsigned short v_active;
	unsigned short frame_rate;
	/*for bt656 */
	enum bt_path_e bt_path;	/* 0:from gpio,1:from csi2 */
	unsigned char hsync_phase;	/* 1: inverted 0: original */
	unsigned char vsync_phase;	/* 1: inverted 0: origianl */
	unsigned short hs_bp;/* the horizontal start postion of bt656 window */
	unsigned short vs_bp;/* the vertical start postion of bt656 window */
	unsigned short fid_check_cnt;	/* vs check hs timeout cnt */
	/*for isp tell different frontends such as bt656/mipi */
	enum tvin_port_e isp_fe_port;
	/*for vdin cfmt convert & scale&skip */
	/* vdin will convert color space accroding to dfmt */
	enum tvin_color_fmt_e dfmt;
	unsigned short dest_hactive;	/* for vdin scale down */
	unsigned short dest_vactive;
	unsigned short skip_count;	/* for skip frame */

	struct csi_parm_s csi_hw_info;

	/*for reserved */
	uintptr_t reserved;
};

struct fe_arg_s {
	enum tvin_port_e port;
	int index;
	void *arg;
};

struct vdin_v4l2_ops_s {
	int (*start_tvin_service)(int no, struct vdin_parm_s *para);
	int (*stop_tvin_service)(int no);
	void (*set_tvin_canvas_info)(int start, int num);
	void (*get_tvin_canvas_info)(int *start, int *num);
	int (*tvin_fe_func)(int no, struct fe_arg_s *arg); /* for isp command */
	int (*tvin_vdin_func)(int no, struct vdin_arg_s *arg);
	void *private;
};

/*
 *macro defined applied to camera driver is ending
 */
extern int v4l2_vdin_ops_init(struct vdin_v4l2_ops_s *vdin_v4l2p);
extern struct vdin_v4l2_ops_s *get_vdin_v4l2_ops(void);
extern int vdin_reg_v4l2(struct vdin_v4l2_ops_s *ops);
#endif
