/*
 * ATVDEMOD Device Driver
 *
 * Author: dezhi kong <dezhi.kong@amlogic.com>
 *
 *
 * Copyright (C) 2014 Amlogic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ATVDEMOD_FUN_H
#define __ATVDEMOD_FUN_H

/*#include "../aml_fe.h"*/
/*#include <linux/amlogic/tvin/tvin.h>*/
/*#include "../aml_fe.h"*/
#include <linux/amlogic/iomap.h>

struct dvb_frontend;

/*#define TVFE_APB_BASE_ADDR 0xd0046000*/
#define ATV_DMD_APB_BASE_ADDR 0xc8008000
#define ATV_DMD_APB_BASE_ADDR_GXTVBB 0xc8840000

#define HHI_ATV_DMD_SYS_CLK_CNTL		0x10f3

extern int atvdemod_debug_en;
extern unsigned int reg_23cf; /* IIR filter */
extern int broad_std_except_pal_m;

#define ATVDEMOD_INTERVAL	(HZ/100)	/*10ms, #define HZ 100*/

extern int amlatvdemod_reg_read(unsigned int reg, unsigned int *val);
extern int amlatvdemod_reg_write(unsigned int reg, unsigned int val);
extern int amlatvdemod_hiu_reg_read(unsigned int reg, unsigned int *val);
extern int amlatvdemod_hiu_reg_write(unsigned int reg, unsigned int val);

static inline uint32_t R_ATVDEMOD_REG(uint32_t reg)
{
	unsigned int val;

	amlatvdemod_reg_read(reg, &val);
	return val;
}

static inline void W_ATVDEMOD_REG(uint32_t reg,
				 const uint32_t val)
{
	amlatvdemod_reg_write(reg, val);
}

static inline void W_ATVDEMOD_BIT(uint32_t reg,
				    const uint32_t value,
				    const uint32_t start,
				    const uint32_t len)
{
	W_ATVDEMOD_REG(reg, ((R_ATVDEMOD_REG(reg) &
			     ~(((1L << (len)) - 1) << (start))) |
			    (((value) & ((1L << (len)) - 1)) << (start))));
}

static inline uint32_t R_ATVDEMOD_BIT(uint32_t reg,
				    const uint32_t start,
				    const uint32_t len)
{
	uint32_t val;

	val = ((R_ATVDEMOD_REG(reg) >> (start)) & ((1L << (len)) - 1));

	return val;
}

static inline uint32_t R_HIU_REG(uint32_t reg)
{
	unsigned int val;

	amlatvdemod_hiu_reg_read(reg, &val);
	return val;
}

static inline void W_HIU_REG(uint32_t reg,
				 const uint32_t val)
{
	amlatvdemod_hiu_reg_write(reg, val);
}

static inline void W_HIU_BIT(uint32_t reg,
				    const uint32_t value,
				    const uint32_t start,
				    const uint32_t len)
{
	W_HIU_REG(reg, ((R_HIU_REG(reg) &
			     ~(((1L << (len)) - 1) << (start))) |
			    (((value) & ((1L << (len)) - 1)) << (start))));
}

static inline uint32_t R_HIU_BIT(uint32_t reg,
				    const uint32_t start,
				    const uint32_t len)
{
	uint32_t val;

	val = ((R_HIU_REG(reg) >> (start)) & ((1L << (len)) - 1));

	return val;
}

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
extern void atv_dmd_wr_reg(unsigned char block, unsigned char reg,
	unsigned long data);
extern unsigned long atv_dmd_rd_reg(unsigned char block, unsigned char reg);
extern unsigned long atv_dmd_rd_byte(unsigned long block_address,
				     unsigned long reg_addr);
extern unsigned long atv_dmd_rd_word(unsigned long block_address,
				     unsigned long reg_addr);
extern unsigned long atv_dmd_rd_long(unsigned long block_address,
				     unsigned long reg_addr);
extern void atv_dmd_wr_long(unsigned long block_address,
				unsigned long reg_addr,
				unsigned long data);
extern void atv_dmd_wr_word(unsigned long block_address,
				unsigned long reg_addr,
				unsigned long data);
extern void atv_dmd_wr_byte(unsigned long block_address,
				unsigned long reg_addr,
				unsigned long data);
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

extern int get_atvdemod_snr_val(void);
extern int aml_atvdemod_get_snr(struct dvb_frontend *fe);

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

enum amlatvdemod_snr_level_e {
	very_low = 1,
	low,
	ok_minus,
	ok_plus,
	high,
};

enum audio_detect_mode {
	AUDIO_AUTO_DETECT = 0,
	AUDIO_MANUAL_DETECT,
};

#if 0
struct amlatvdemod_device_s {
	struct class *clsp;
	struct device *dev;
	struct analog_parameters parm;
	int fre_offset;
	struct pinctrl *pin;
	const char *pin_name;
};
#endif

extern int amlatvdemod_hiu_reg_read(unsigned int reg, unsigned int *val);
extern void aml_audio_overmodulation(int enable);
extern void amlatvdemod_set_std(int val);
extern void aml_fix_PWM_adjust(int enable);
extern void aml_audio_valume_gain_set(unsigned int audio_gain);
extern unsigned int aml_audio_valume_gain_get(void);
extern void aml_atvdemod_overmodule_det(void);
extern int aml_audiomode_autodet(struct dvb_frontend *fe);
extern void retrieve_frequency_offset(int *freq_offset);
extern void retrieve_field_lock(int *lock);
extern int aml_atvdemod_get_snr_ex(void);
extern void set_atvdemod_scan_mode(int val);
extern int atvauddemod_init(void);
extern void atvauddemod_set_outputmode(void);

/*from amldemod/amlfrontend.c*/
extern int vdac_enable_check_dtv(void);
#endif /* __ATVDEMOD_FUN_H */
