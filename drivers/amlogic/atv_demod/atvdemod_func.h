/*
 * drivers/amlogic/atv_demod/atvdemod_func.h
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

#ifndef __ATV_DEMOD_FUNC_H__
#define __ATV_DEMOD_FUNC_H__

struct v4l2_frontend;

#define HHI_ATV_DMD_SYS_CLK_CNTL		0x10f3

extern unsigned int reg_23cf; /* IIR filter */
extern int broad_std_except_pal_m;
extern unsigned int aud_std;
extern unsigned int aud_mode;
extern bool audio_thd_en;

enum broadcast_standard_e {
	ATVDEMOD_STD_NTSC = 0,
	ATVDEMOD_STD_NTSC_J,
	ATVDEMOD_STD_PAL_M,
	ATVDEMOD_STD_PAL_BG,
	ATVDEMOD_STD_DTV,
	ATVDEMOD_STD_SECAM_DK2,
	ATVDEMOD_STD_SECAM_DK3,
	ATVDEMOD_STD_PAL_BG_NICAM,
	ATVDEMOD_STD_PAL_DK_CHINA,
	ATVDEMOD_STD_SECAM_L,
	ATVDEMOD_STD_PAL_I,
	ATVDEMOD_STD_PAL_DK1,
	ATVDEMOD_STD_MAX,
};
enum gde_curve_e {
	ATVDEMOD_CURVE_M = 0,
	ATVDEMOD_CURVE_A,
	ATVDEMOD_CURVE_B,
	ATVDEMOD_CURVE_CHINA,
	ATVDEMOD_CURVE_MAX,
};
enum sound_format_e {
	ATVDEMOD_SOUND_STD_MONO = 0,
	ATVDEMOD_SOUND_STD_NICAM,
	ATVDEMOD_SOUND_STD_MAX,
};

extern void set_audio_gain_val(int val);
extern void set_video_gain_val(int val);
extern void atv_dmd_soft_reset(void);
extern void atv_dmd_input_clk_32m(void);
extern void read_version_register(void);
extern void check_communication_interface(void);
extern void power_on_receiver(void);
extern void atv_dmd_misc(void);
extern void configure_receiver(int Broadcast_Standard,
			       unsigned int Tuner_IF_Frequency,
			       int Tuner_Input_IF_inverted, int GDE_Curve,
			       int sound_format);
extern int atvdemod_clk_init(void);
extern int atvdemod_init(void);
extern void atvdemod_uninit(void);
extern void atv_dmd_set_std(void);
extern void retrieve_adc_power(int *adc_level);
extern void retrieve_vpll_carrier_lock(int *lock);
extern void retrieve_vpll_carrier_line_lock(int *lock);
extern void retrieve_vpll_carrier_audio_power(int *power);
extern void retrieve_video_lock(int *lock);
extern int retrieve_vpll_carrier_afc(void);

extern void atv_dmd_non_std_set(bool enable);
extern void atvdemod_video_overmodulated(void);
extern void atvdemod_det_snr_serice(void);
extern int atvdemod_get_snr_val(void);
extern void atvdemod_mixer_tune(void);

/*atv demod block address*/
/*address interval is 4, because it's 32bit interface,*/
/*but the address is in byte */
#define ATV_DMD_TOP_CTRL			0x0
#define ATV_DMD_TOP_CTRL1			0x4
#define ATV_DMD_RST_CTRL			0x8

#define APB_BLOCK_ADDR_SYSTEM_MGT			0x0
#define APB_BLOCK_ADDR_AA_LP_NOTCH			0x1
#define APB_BLOCK_ADDR_MIXER_1				0x2
#define APB_BLOCK_ADDR_MIXER_3				0x3
#define APB_BLOCK_ADDR_ADC_SE				0x4
#define APB_BLOCK_ADDR_PWR_ANL				0x5
#define APB_BLOCK_ADDR_CARR_RCVY			0x6
#define APB_BLOCK_ADDR_FE_DROOP_MDF			0x7
#define APB_BLOCK_ADDR_SIF_IC_STD			0x8
#define APB_BLOCK_ADDR_SIF_STG_2			0x9
#define APB_BLOCK_ADDR_SIF_STG_3			0xa
#define APB_BLOCK_ADDR_IC_AGC				0xb
#define APB_BLOCK_ADDR_DAC_UPS				0xc
#define APB_BLOCK_ADDR_GDE_EQUAL			0xd
#define APB_BLOCK_ADDR_VFORMAT				0xe
#define APB_BLOCK_ADDR_VDAGC				0xf
#define APB_BLOCK_ADDR_VERS_REGISTER			0x10
#define APB_BLOCK_ADDR_INTERPT_MGT			0x11
#define APB_BLOCK_ADDR_ADC_MGR				0x12
#define APB_BLOCK_ADDR_GP_VD_FLT			0x13
#define APB_BLOCK_ADDR_CARR_DMD				0x14
#define APB_BLOCK_ADDR_SIF_VD_IF			0x15
#define APB_BLOCK_ADDR_VD_PKING				0x16
#define APB_BLOCK_ADDR_FE_DR_SMOOTH			0x17
#define APB_BLOCK_ADDR_AGC_PWM				0x18
#define APB_BLOCK_ADDR_DAC_UPS_24M			0x19
#define APB_BLOCK_ADDR_VFORMAT_DP			0x1a
#define APB_BLOCK_ADDR_VD_PKING_DAC			0x1b
#define APB_BLOCK_ADDR_MONO_PROC			0x1c
#define APB_BLOCK_ADDR_TOP				0x1d

#define SLAVE_BLOCKS_NUMBER 0x1d	/*indeed totals 0x1e, adding top*/

/*Broadcast_Standard*/
/*                      0: NTSC*/
/*                      1: NTSC-J*/
/*                      2: PAL-M,*/
/*                      3: PAL-BG*/
/*                      4: DTV*/
/*                      5: SECAM- DK2*/
/*                      6: SECAM -DK3*/
/*                      7: PAL-BG, NICAM*/
/*                      8: PAL-DK-CHINA*/
/*                      9: SECAM-L / SECAM-DK3*/
/*                      10: PAL-I*/
/*                      11: PAL-DK1*/
#define AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC		0
#define AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_J		1
#define AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_M		2
#define AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_BG		3
#define AML_ATV_DEMOD_VIDEO_MODE_PROP_DTV		4
#define AML_ATV_DEMOD_VIDEO_MODE_PROP_SECAM_DK2		5
#define AML_ATV_DEMOD_VIDEO_MODE_PROP_SECAM_DK3		6
#define AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_BG_NICAM	7
#define AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_DK		8
#define AML_ATV_DEMOD_VIDEO_MODE_PROP_SECAM_L	9
#define AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_I	10
#define AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_DK1	11
/* new add @20150813 begin */
#define AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_DK		12
#define AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_BG	13
#define AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_I	14
#define AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_M    15
/* new add @20150813 end */

/*GDE_Curve*/
/*                      0: CURVE-M*/
/*                      1: CURVE-A*/
/*                      2: CURVE-B*/
/*                      3: CURVE-CHINA*/
/*                      4: BYPASS*/
#define AML_ATV_DEMOD_VIDEO_MODE_PROP_CURVE_M		0
#define AML_ATV_DEMOD_VIDEO_MODE_PROP_CURVE_A		1
#define AML_ATV_DEMOD_VIDEO_MODE_PROP_CURVE_B		2
#define AML_ATV_DEMOD_VIDEO_MODE_PROP_CURVE_CHINA	3
#define AML_ATV_DEMOD_VIDEO_MODE_PROP_CURVE_BYPASS	4

#define HHI_GCLK_MPEG0 (0x1050)

/*sound format 0: MONO;1:NICAM*/
#define AML_ATV_DEMOD_SOUND_MODE_PROP_MONO	0
#define AML_ATV_DEMOD_SOUND_MODE_PROP_NICAM	1
/*
 * freq_hz:hs_freq
 * freq_hz_cvrt=hs_freq/0.23841858
 * vs_freq==50,freq_hz=15625;freq_hz_cvrt=0xffff
 * vs_freq==60,freq_hz=15734,freq_hz_cvrt=0x101c9
 */
#define AML_ATV_DEMOD_FREQ_50HZ_VERT	0xffff	/*65535*/
#define AML_ATV_DEMOD_FREQ_60HZ_VERT	0x101c9	/*65993*/

#define CARR_AFC_DEFAULT_VAL 0xffff

enum atvdemod_snr_level_e {
	very_low = 1,
	low,
	ok_minus,
	ok_plus,
	high,
};

extern void aml_audio_overmodulation(int enable);
extern void amlatvdemod_set_std(int val);
extern void aml_fix_PWM_adjust(int enable);
extern void aml_audio_valume_gain_set(unsigned int audio_gain);
extern unsigned int aml_audio_valume_gain_get(void);
extern int aml_audiomode_autodet(struct v4l2_frontend *v4l2_fe);
extern void retrieve_frequency_offset(int *freq_offset);
extern void retrieve_field_lock(int *lock);
extern void set_atvdemod_scan_mode(int val);
extern int atvauddemod_init(void);
extern void atvauddemod_set_outputmode(void);

/*from amldemod/amlfrontend.c*/
extern int vdac_enable_check_dtv(void);

#endif /* __ATV_DEMOD_FUNC_H__ */
