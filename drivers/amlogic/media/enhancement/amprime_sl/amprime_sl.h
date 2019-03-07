/*
 * drivers/amlogic/media/enhancement/amprime_sl/amprime_sl.h
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

#ifndef __AMPRIME_SL_H__
#define __AMPRIME_SL_H__

#define DRIVER_VER "20190307"

#define ENABLE 1
#define DISABLE 0

enum cpuID_e {
	_CPU_MAJOR_ID_G12,
	_CPU_MAJOR_ID_TL1,
	_CPU_MAJOR_ID_UNKNOWN,
};

struct prime_sl_device_data_s {
	enum cpuID_e cpu_id;
};


#define PRIMESL_LUTC_ADDR_PORT		(0x3980)
#define PRIMESL_LUTC_DATA_PORT		(0x3981)
#define PRIMESL_LUTP_ADDR_PORT		(0x3982)
#define PRIMESL_LUTP_DATA_PORT		(0x3983)
#define PRIMESL_LUTD_ADDR_PORT		(0x3984)
#define PRIMESL_LUTD_DATA_PORT		(0x3985)
#define PRIMESL_CTRL0		        (0x3990)
union PRIMESL_CTRL0_BITS {
	unsigned int d32;
	struct {
		unsigned int primesl_en:1,
		gclk_ctrl:2,
		reg_gclk_ctrl:1,
		inv_y_ratio:11,
		reserved3:1,
		inv_chroma_ratio:11,
		reserved2:1,
		clip_en:1,
		legacy_mode_en:1,
		reserved1:2;
	} b;
};
#define PRIMESL_CTRL1		(0x3991)
union PRIMESL_CTRL1_BITS {
	unsigned int d32;
	struct {
		unsigned int footroom:10,
		reserved2:6,
		l_headroom:10,
		reserved1:6;
	} b;
};
#define PRIMESL_CTRL2		(0x3992)
union PRIMESL_CTRL2_BITS {
	unsigned int d32;
	struct {
		unsigned int c_headroom:10,
		reserved1:22;
	} b;
};
#define PRIMESL_CTRL3		(0x3993)
union PRIMESL_CTRL3_BITS {
	unsigned int d32;
	struct {
		unsigned int mua:14,
		reserved2:2,
		mub:14,
		reserved1:2;
	} b;
};
#define PRIMESL_CTRL4		(0x3994)
union PRIMESL_CTRL4_BITS {
	unsigned int d32;
	struct {
		unsigned int oct_7_0:10,
		reserved2:6,
		oct_7_1:10,
		reserved1:6;
	} b;
};
#define PRIMESL_CTRL5		(0x3995)
union PRIMESL_CTRL5_BITS {
	unsigned int d32;
	struct {
		unsigned int oct_7_2:10,
		reserved2:6,
		oct_7_3:10,
		reserved1:6;
	} b;
};
#define PRIMESL_CTRL6		(0x3996)
union PRIMESL_CTRL6_BITS {
	unsigned int d32;
	struct {
		unsigned int oct_7_4:10,
		reserved2:6,
		oct_7_5:10,
		reserved1:6;
	} b;
};
#define PRIMESL_CTRL7		(0x3997)
union PRIMESL_CTRL7_BITS {
	unsigned int d32;
	struct {
		unsigned int oct_7_6:10,
		reserved1:22;
	} b;
};
#define PRIMESL_CTRL8		(0x3998)
union PRIMESL_CTRL8_BITS {
	unsigned int d32;
	struct {
		unsigned int d_lut_threshold_3_0:13,
		reserved2:3,
		d_lut_threshold_3_1:13,
		reserved1:3;
	} b;
};
#define PRIMESL_CTRL9		(0x3999)
union PRIMESL_CTRL9_BITS {
	unsigned int d32;
	struct {
		unsigned int d_lut_threshold_3_2:13,
		reserved1:19;
	} b;
};
#define PRIMESL_CTRL10		(0x399a)
union PRIMESL_CTRL10_BITS {
	unsigned int d32;
	struct {
		unsigned int d_lut_step_4_0:4,
		d_lut_step_4_1:4,
		d_lut_step_4_2:4,
		d_lut_step_4_3:4,
		reserved1:16;
	} b;
};
#define PRIMESL_CTRL11		(0x399b)
union PRIMESL_CTRL11_BITS {
	unsigned int d32;
	struct {
		unsigned int rgb2yuv_9_1:13,
		reserved2:3,
		rgb2yuv_9_0:13,
		reserved1:3;
	} b;
};
#define PRIMESL_CTRL12		(0x399c)
union PRIMESL_CTRL12_BITS {
	unsigned int d32;
	struct {
		unsigned int rgb2yuv_9_3:13,
		reserved2:3,
		rgb2yuv_9_2:13,
		reserved1:3;
	} b;
};
#define PRIMESL_CTRL13		(0x399d)
union PRIMESL_CTRL13_BITS {
	unsigned int d32;
	struct {
		unsigned int rgb2yuv_9_5:13,
		reserved2:3,
		rgb2yuv_9_4:13,
		reserved1:3;
	} b;
};
#define PRIMESL_CTRL14		(0x399e)
union PRIMESL_CTRL14_BITS {
	unsigned int d32;
	struct {
		unsigned int rgb2yuv_9_7:13,
		reserved2:3,
		rgb2yuv_9_6:13,
		reserved1:3;
	} b;
};
#define PRIMESL_CTRL15		(0x399f)
union PRIMESL_CTRL15_BITS {
	unsigned int d32;
	struct {
		unsigned int rgb2yuv_9_8:13,
		reserved1:19;
	} b;
};
#define PRIME_PRIMESL_EN	   (0)
#define PRIME_GCLK_CTRL	           (1)
#define PRIME_REG_GCLK_CTRL	   (2)
#define PRIME_INV_Y_RATIO	   (3)
#define PRIME_INV_CHROMA_RATIO	   (4)
#define PRIME_CLIP_EN	           (5)
#define PRIME_LEGACY_MODE_EN	   (6)
#define PRIME_FOOTROOM	           (7)
#define PRIME_L_HEADROOM	   (8)
#define PRIME_C_HEADROOM	   (9)
#define PRIME_MUA	           (10)
#define PRIME_MUB	           (11)
#define PRIME_OCT_7_0	           (12)
#define PRIME_OCT_7_1	           (13)
#define PRIME_OCT_7_2	           (14)
#define PRIME_OCT_7_3	           (15)
#define PRIME_OCT_7_4              (16)
#define PRIME_OCT_7_5	           (17)
#define PRIME_OCT_7_6	           (18)
#define PRIME_D_LUT_THRESHOLD_3_0  (19)
#define PRIME_D_LUT_THRESHOLD_3_1  (20)
#define PRIME_D_LUT_THRESHOLD_3_2  (21)
#define PRIME_D_LUT_STEP_4_0	   (22)
#define PRIME_D_LUT_STEP_4_1	   (23)
#define PRIME_D_LUT_STEP_4_2	   (24)
#define PRIME_D_LUT_STEP_4_3	   (25)
#define PRIME_RGB2YUV_9_0	   (26)
#define PRIME_RGB2YUV_9_1	   (27)
#define PRIME_RGB2YUV_9_2	   (28)
#define PRIME_RGB2YUV_9_3	   (29)
#define PRIME_RGB2YUV_9_4	   (30)
#define PRIME_RGB2YUV_9_5	   (31)
#define PRIME_RGB2YUV_9_6	   (32)
#define PRIME_RGB2YUV_9_7	   (33)
#define PRIME_RGB2YUV_9_8	   (34)


struct prime_sl_t {
	unsigned int legacy_mode_en;
	unsigned int clip_en;
	unsigned int reg_gclk_ctrl;
	unsigned int gclk_ctrl;
	unsigned int primesl_en;

	unsigned int inv_chroma_ratio;
	unsigned int inv_y_ratio;
	unsigned int l_headroom;
	unsigned int footroom;
	unsigned int c_headroom;
	unsigned int mub;
	unsigned int mua;
	int oct[7];
	unsigned int d_lut_threshold[3];
	unsigned int d_lut_step[4];
	int rgb2yuv[9];

	uint16_t olut_c[65];	/*from */
	uint16_t olut_p[65];
	uint16_t olut_d[65];

	unsigned int lut_c[65];
	unsigned int lut_p[65];
	unsigned int lut_d[65];
};

struct sl_hdr_metadata_variables {
	int tmInputSignalBlackLevelOffset;
	int tmInputSignalWhiteLevelOffset;
	int shadowGain;
	int highlightGain;
	int midToneWidthAdjFactor;
	int tmOutputFineTuningNumVal;
	int tmOutputFineTuningX[10];
	int tmOutputFineTuningY[10];
	int saturationGainNumVal;
	int saturationGainX[6];
	int saturationGainY[6];
};

struct sl_hdr_metadata_tables {
	int luminanceMappingNumVal;
	int luminanceMappingX[65];
	int luminanceMappingY[65];
	int colourCorrectionNumVal;
	int colourCorrectionX[65];
	int colourCorrectionY[65];
};

struct sl_hdr_metadata {
	int partID;
	int majorSpecVersionID;
	int minorSpecVersionID;
	int payloadMode;
	int hdrPicColourSpace;
	int hdrDisplayColourSpace;
	int hdrDisplayMaxLuminance;
	int hdrDisplayMinLuminance;
	int sdrPicColourSpace;
	int sdrDisplayColourSpace;
	int sdrDisplayMaxLuminance;
	int sdrDisplayMinLuminance;
	int matrixCoefficient[4];
	int chromaToLumaInjection[2];
	int kCoefficient[3];
	union {
		struct sl_hdr_metadata_variables variables;
		struct sl_hdr_metadata_tables tables;
	} u;
};

struct prime_cfg_t {
	unsigned int width;	/*use?*/
	unsigned int height;	/*use?*/
	unsigned int bit_depth;	/*use?*/
	int display_OETF;	/**/
	int yuv_range;
	int display_Brightness;
};

struct prime_t {

	int en_ic;	/*0: ic not support prime sl; 1: ic support prime sl*/
	int en_top;	/*top prime sl switch*/
	int en_pause;
	int en_checkdata;
	int en_count;
	int en_count_tsk;
	int en_set;
	int en_close;

	int ok_count;
	int ok_set;

	int isr_mode;
	unsigned int vs_cnt;
	unsigned int ver_hw;
	unsigned int ver_fw;

	struct prime_cfg_t	*pCfg;
	struct sl_hdr_metadata	*pmta;
	struct prime_sl_t	*pps;

	struct prime_cfg_t	Cfg;
	struct sl_hdr_metadata	prime_metadata;
	struct prime_sl_t	prime_sl;

	/*test*/
	unsigned int dbg_nub;
	unsigned int dbg_metachange;
	struct timeval    tv[4][2];

};

extern void prime_api_init(void);
extern void prime_api_exit(void);
extern void prime_api_isr_process(void);
extern void prime_api_cmd_process(unsigned char cmd, int para);
extern unsigned int prime_api_info_show(char *buf, unsigned int size);
extern int prime_api_store(const char *buf, unsigned int para);
extern int prime_metadata_parser_process(struct prime_t *prime_sl_setting);

struct hdr_prime_sl_func_s {
	const char *version_info;
	void (*prime_api_init)(void);
	void (*prime_api_exit)(void);
	void (*prime_api_isr_process)(void);
	void (*prime_api_cmd_process)(unsigned char cmd, int para);
	unsigned int (*prime_api_info_show)(char *buf, unsigned int size);
	int (*prime_api_store)(const char *buf, unsigned int para);
	int (*prime_metadata_parser_process)(struct prime_t *prime_sl_setting);
};

extern int register_prime_functions(const struct hdr_prime_sl_func_s *func);
extern int unregister_prime_functions(void);

#ifndef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
#define _VSYNC_WR_MPEG_REG(adr, val) WRITE_VPP_REG(adr, val)
#define _VSYNC_RD_MPEG_REG(adr) READ_VPP_REG(adr)
#define _VSYNC_WR_MPEG_REG_BITS(adr, val, start, len) \
	WRITE_VPP_REG_BITS(adr, val, start, len)
#else
extern int _VSYNC_WR_MPEG_REG_BITS(u32 adr, u32 val, u32 start, u32 len);
extern u32 _VSYNC_RD_MPEG_REG(u32 adr);
extern int _VSYNC_WR_MPEG_REG(u32 adr, u32 val);
#endif
extern void prime_sl_set_reg(const struct prime_sl_t *pS);
extern void prime_sl_close(void);


#endif	/*__REG_G12A_PRIME_H__*/
