/*
 * mt8365_afe_common.h  --  Mediatek 8365 audio driver common definitions
 *
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Jia Zeng <jia.zeng@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _MT8365_AFE_COMMON_H_
#define _MT8365_AFE_COMMON_H_

#define COMMON_CLOCK_FRAMEWORK_API
#define IDLE_TASK_DRIVER_API
#define ENABLE_AFE_APLL_TUNER

#include <linux/clk.h>
#include <linux/regmap.h>
#include <sound/asound.h>
#include "../common/mtk-base-afe.h"

enum {
	MT8365_AFE_MEMIF_DL1,
	MT8365_AFE_MEMIF_DL2,
	MT8365_AFE_MEMIF_TDM_OUT,
	/*
	 * MT8365_AFE_MEMIF_SPDIF_OUT,
	 */
	MT8365_AFE_MEMIF_AWB,
	MT8365_AFE_MEMIF_VUL,
	MT8365_AFE_MEMIF_VUL2,
	MT8365_AFE_MEMIF_VUL3,
	MT8365_AFE_MEMIF_TDM_IN,
	/*
	 * MT8365_AFE_MEMIF_SPDIF_IN,
	 */
	MT8365_AFE_MEMIF_NUM,
	MT8365_AFE_BACKEND_BASE = MT8365_AFE_MEMIF_NUM,
	MT8365_AFE_IO_TDM_OUT = MT8365_AFE_BACKEND_BASE,
	MT8365_AFE_IO_TDM_IN,
	MT8365_AFE_IO_I2S,
	MT8365_AFE_IO_2ND_I2S,
	MT8365_AFE_IO_PCM1,
	MT8365_AFE_IO_VIRTUAL_DL_SRC,
	MT8365_AFE_IO_VIRTUAL_TDM_OUT_SRC,
	MT8365_AFE_IO_VIRTUAL_FM,
	MT8365_AFE_IO_DMIC,
	MT8365_AFE_IO_INT_ADDA,
	MT8365_AFE_IO_GASRC1,
	MT8365_AFE_IO_GASRC2,
	MT8365_AFE_IO_TDM_ASRC,
	MT8365_AFE_IO_HW_GAIN1,
	MT8365_AFE_IO_HW_GAIN2,
	MT8365_AFE_BACKEND_END,
	MT8365_AFE_BACKEND_NUM = (MT8365_AFE_BACKEND_END -
				  MT8365_AFE_BACKEND_BASE),
};

enum {
	MT8365_AFE_IRQ1,
	MT8365_AFE_IRQ2,
	MT8365_AFE_IRQ3,
	MT8365_AFE_IRQ4,
	MT8365_AFE_IRQ5,
	MT8365_AFE_IRQ6,
	MT8365_AFE_IRQ7,
	MT8365_AFE_IRQ8,
	MT8365_AFE_IRQ9,
	MT8365_AFE_IRQ10,
	MT8365_AFE_IRQ_NUM,
};

enum {
	MT8365_TOP_CG_AFE,
	MT8365_TOP_CG_I2S_IN,
	MT8365_TOP_CG_22M,
	MT8365_TOP_CG_24M,
	MT8365_TOP_CG_INTDIR_CK,
	MT8365_TOP_CG_APLL2_TUNER,
	MT8365_TOP_CG_APLL_TUNER,
	MT8365_TOP_CG_SPDIF,
	MT8365_TOP_CG_TDM_OUT,
	MT8365_TOP_CG_TDM_IN,
	MT8365_TOP_CG_ADC,
	MT8365_TOP_CG_DAC,
	MT8365_TOP_CG_DAC_PREDIS,
	MT8365_TOP_CG_TML,
	MT8365_TOP_CG_I2S1_BCLK,
	MT8365_TOP_CG_I2S2_BCLK,
	MT8365_TOP_CG_I2S3_BCLK,
	MT8365_TOP_CG_I2S4_BCLK,
	MT8365_TOP_CG_DMIC0_ADC,
	MT8365_TOP_CG_DMIC1_ADC,
	MT8365_TOP_CG_DMIC2_ADC,
	MT8365_TOP_CG_DMIC3_ADC,
	MT8365_TOP_CG_CONNSYS_I2S_ASRC,
	MT8365_TOP_CG_GENERAL1_ASRC,
	MT8365_TOP_CG_GENERAL2_ASRC,
	MT8365_TOP_CG_TDM_ASRC,
	MT8365_TOP_CG_NUM
};

enum {
	MT8365_CLK_TOP_AUD_SEL,
	MT8365_CLK_AUD_I2S0_M,
	MT8365_CLK_AUD_I2S1_M,
	MT8365_CLK_AUD_I2S2_M,
	MT8365_CLK_AUD_I2S3_M,
	MT8365_CLK_AUD_TDMOUT_M,
	MT8365_CLK_AUD_TDMOUT_B,
	MT8365_CLK_AUD_TDMIN_M,
	MT8365_CLK_AUD_TDMIN_B,
	MT8365_CLK_AUD_SPDIF_M,
	MT8365_CLK_SPDIF_IN,
	MT8365_CLK_ENGEN1,
	MT8365_CLK_ENGEN2,
	MT8365_CLK_AUD1,
	MT8365_CLK_AUD2,
	MT8365_CLK_I2S0_M_SEL,
	MT8365_CLK_I2S1_M_SEL,
	MT8365_CLK_I2S2_M_SEL,
	MT8365_CLK_I2S3_M_SEL,
	MT8365_CLK_TDMOUT_M_SEL,
	MT8365_CLK_TDMIN_M_SEL,
	MT8365_CLK_SPDIF_B_SEL,
	MT8365_CLK_AUD_SYSPLL3_D4,
	MT8365_CLK_CLK26M,
	MT8365_CLK_AUD_SYSPLL1_D4,
	MT8365_CLK_AUD_SYSPLL4_D2,
	MT8365_CLK_AUD_INTBUS_SEL,
	MT8365_CLK_NUM
};

enum {
	MT8365_AFE_APLL1 = 0,
	MT8365_AFE_APLL2,
	MT8365_AFE_APLL_NUM,
};

enum {
	MT8365_AFE_1ST_I2S = 0,
	MT8365_AFE_2ND_I2S,
	MT8365_AFE_I2S_SETS,
};

enum {
	MT8365_AFE_I2S_SEPARATE_CLOCK = 0,
	MT8365_AFE_I2S_SHARED_CLOCK,
};

enum {
	MT8365_AFE_TDM_OUT_I2S = 0,
	MT8365_AFE_TDM_OUT_TDM,
	MT8365_AFE_TDM_OUT_I2S_32BITS,
};

enum mt8365_afe_tdm_ch_start {
	AFE_TDM_CH_START_O28_O29 = 0,
	AFE_TDM_CH_START_O30_O31,
	AFE_TDM_CH_START_O32_O33,
	AFE_TDM_CH_START_O34_O35,
	AFE_TDM_CH_ZERO,
};

enum {
	MT8365_PCM_FORMAT_I2S = 0,
	MT8365_PCM_FORMAT_EIAJ,
	MT8365_PCM_FORMAT_PCMA,
	MT8365_PCM_FORMAT_PCMB,
};

enum {
	MT8365_FS_8K = 0,
	MT8365_FS_11D025K,
	MT8365_FS_12K,
	MT8365_FS_384K,
	MT8365_FS_16K,
	MT8365_FS_22D05K,
	MT8365_FS_24K,
	MT8365_FS_130K,
	MT8365_FS_32K,
	MT8365_FS_44D1K,
	MT8365_FS_48K,
	MT8365_FS_88D2K,
	MT8365_FS_96K,
	MT8365_FS_176D4K,
	MT8365_FS_192K,
};

enum {
	FS_8000HZ  = 0, /* 0000b */
	FS_11025HZ = 1, /* 0001b */
	FS_12000HZ = 2, /* 0010b */
	FS_384000HZ = 3, /* 0011b */
	FS_16000HZ = 4, /* 0100b */
	FS_22050HZ = 5, /* 0101b */
	FS_24000HZ = 6, /* 0110b */
	FS_130000HZ = 7, /* 0111b */
	FS_32000HZ = 8, /* 1000b */
	FS_44100HZ = 9, /* 1001b */
	FS_48000HZ = 10, /* 1010b */
	FS_88200HZ = 11, /* 1011b */
	FS_96000HZ = 12, /* 1100b */
	FS_176400HZ = 13, /* 1101b */
	FS_192000HZ = 14, /* 1110b */
	FS_260000HZ = 15, /* 1111b */
};

enum {
	MT8365_AFE_DEBUGFS_AFE,
	MT8365_AFE_DEBUGFS_MEMIF,
	MT8365_AFE_DEBUGFS_IRQ,
	MT8365_AFE_DEBUGFS_CONN,
	MT8365_AFE_DEBUGFS_DBG,
	MT8365_AFE_DEBUGFS_NUM,
};

enum {
	MT8365_AFE_IRQ_DIR_MCU = 0,
	MT8365_AFE_IRQ_DIR_DSP,
	MT8365_AFE_IRQ_DIR_BOTH,
};

struct mt8365_fe_dai_data {
	bool use_sram;
	unsigned int sram_phy_addr;
	void __iomem *sram_vir_addr;
	unsigned int sram_size;
};

struct mt8365_be_dai_data {
	bool prepared[SNDRV_PCM_STREAM_LAST + 1];
	unsigned int fmt_mode;
};

struct mt8365_pcm_intf_data {
	bool slave_mode;
	bool lrck_inv;
	bool bck_inv;
	unsigned int format;
};

#define MT8365_CLK_26M 26000000
#define MT8365_CLK_24M 24000000
#define MT8365_CLK_22M 22000000
#define MT8365_CM_UPDATA_CNT_SET 8

enum mt8365_cm_num {
	MT8365_CM1 = 0,
	MT8365_CM2,
	MT8365_CM_NUM,
};

enum mt8365_cm2_mux_in {
	MT8365_FROM_GASRC1 = 1,
	MT8365_FROM_GASRC2,
	MT8365_FROM_TDM_ASRC,
	MT8365_CM_MUX_NUM,
};

enum cm2_mux_conn_in {
	GENERAL2_ASRC_OUT_LCH = 0,
	GENERAL2_ASRC_OUT_RCH = 1,
	TDM_IN_CH0 = 2,
	TDM_IN_CH1 = 3,
	TDM_IN_CH2 = 4,
	TDM_IN_CH3 = 5,
	TDM_IN_CH4 = 6,
	TDM_IN_CH5 = 7,
	TDM_IN_CH6 = 8,
	TDM_IN_CH7 = 9,
	GENERAL1_ASRC_OUT_LCH = 10,
	GENERAL1_ASRC_OUT_RCH = 11,
	TDM_OUT_ASRC_CH0 = 12,
	TDM_OUT_ASRC_CH1 = 13,
	TDM_OUT_ASRC_CH2 = 14,
	TDM_OUT_ASRC_CH3 = 15,
	TDM_OUT_ASRC_CH4 = 16,
	TDM_OUT_ASRC_CH5 = 17,
	TDM_OUT_ASRC_CH6 = 18,
	TDM_OUT_ASRC_CH7 = 19
};

struct mt8365_cm_ctrl_reg {
	unsigned int con0;
	unsigned int con1;
	unsigned int con2;
	unsigned int con3;
	unsigned int con4;
};

struct mt8365_control_data {
	bool bypass_cm1;
	bool bypass_cm2;
	unsigned int loopback_type;
};

enum dmic_input_mode {
	DMIC_MODE_3P25M = 0,
	DMIC_MODE_1P625M,
	DMIC_MODE_812P5K,
	DMIC_MODE_406P25K,
};

struct mt8365_dmic_data {
	bool two_wire_mode;
	unsigned int clk_phase_sel_ch1;
	unsigned int clk_phase_sel_ch2;
	bool iir_on;
	unsigned int irr_mode;
	unsigned int dmic_mode;
	unsigned int dmic_channel;
};

enum iir_mode {
	IIR_MODE0 = 0,
	IIR_MODE1,
	IIR_MODE2,
	IIR_MODE3,
	IIR_MODE4,
	IIR_MODE5,
};

enum {
	MT8365_GASRC1 = 0,
	MT8365_GASRC2,
	MT8365_GASRC_NUM,
	MT8365_TDM_ASRC1 = MT8365_GASRC_NUM,
	MT8365_TDM_ASRC2,
	MT8365_TDM_ASRC3,
	MT8365_TDM_ASRC4,
	MT8365_TDM_ASRC_NUM,
};

struct mt8365_gasrc_ctrl_reg {
	unsigned int con0;
	unsigned int con2;
	unsigned int con3;
	unsigned int con4;
	unsigned int con5;
	unsigned int con6;
	unsigned int con9;
	unsigned int con10;
	unsigned int con12;
	unsigned int con13;
};

struct mt8365_gasrc_data {
	bool duplex;
	bool tx_mode;
	bool cali_on;
	bool tdm_asrc_out_cm2;
	bool iir_on;
};

#ifdef CONFIG_MTK_HIFIXDSP_SUPPORT
struct mt8365_adsp_data {
	/* information adsp supply */
	bool adsp_on;
	int (*hostless_active)(void);
	/* information afe supply */
	int (*set_afe_memif)(struct mtk_base_afe *afe,
				       int memif_id,
				       unsigned int rate,
				       unsigned int channels,
				       snd_pcm_format_t format);
	int (*set_afe_memif_enable)(struct mtk_base_afe *afe,
				       int memif_id,
				       unsigned int rate,
				       unsigned int period_size,
				       int enable);
	void (*get_afe_memif_sram)(struct mtk_base_afe *afe,
				       int memif_id,
				       unsigned int *paddr,
				       unsigned int *size);
	void (*set_afe_init)(struct mtk_base_afe *afe);
	void (*set_afe_uninit)(struct mtk_base_afe *afe);
};
#endif

struct mt8365_afe_private {
	struct clk *clocks[MT8365_CLK_NUM];
	struct mt8365_fe_dai_data fe_data[MT8365_AFE_MEMIF_NUM];
	struct mt8365_be_dai_data be_data[MT8365_AFE_BACKEND_NUM];
	struct mt8365_pcm_intf_data pcm_intf_data;
	struct mt8365_control_data ctrl_data;
	struct mt8365_dmic_data dmic_data;
	struct mt8365_gasrc_data gasrc_data[MT8365_TDM_ASRC_NUM];
#ifdef CONFIG_MTK_HIFIXDSP_SUPPORT
	struct mt8365_adsp_data adsp_data;
#endif
	int afe_on_ref_cnt;
	int top_cg_ref_cnt[MT8365_TOP_CG_NUM];
	void __iomem *afe_sram_vir_addr;
	unsigned int afe_sram_phy_addr;
	unsigned int afe_sram_size;
	/* locks */
	spinlock_t afe_ctrl_lock;
	struct mutex afe_clk_mutex;
#ifdef IDLE_TASK_DRIVER_API
	int emi_clk_ref_cnt;
	struct mutex emi_clk_mutex;
#endif
#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs_dentry[MT8365_AFE_DEBUGFS_NUM];
#endif
	unsigned int i2s_clk_modes[MT8365_AFE_I2S_SETS];
	unsigned int i2s_dynamic_bck;
	int apll_tuner_ref_cnt[MT8365_AFE_APLL_NUM];
	int i2s_out_on_ref_cnt;
	int adda_afe_on_ref_cnt;
	unsigned int tdm_out_mode;
	unsigned int cm2_mux_input;
};

bool mt8365_afe_rate_supported(unsigned int rate, unsigned int id);
bool mt8365_afe_channel_supported(unsigned int channel, unsigned int id);
#ifdef CONFIG_MTK_HIFIXDSP_SUPPORT
struct mtk_base_afe *mt8365_afe_pcm_get_info(void);
#endif
#endif
