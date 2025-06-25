/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mt8189-afe-common.h  --  MediaTek 8189 audio driver definitions
 *
 *  Copyright (c) 2025 MediaTek Inc.
 *  Author: Darren Ye <darren.ye@mediatek.com>
 */

#ifndef _MT_8189_AFE_COMMON_H_
#define _MT_8189_AFE_COMMON_H_
#include <sound/soc.h>
#include <linux/list.h>
#include <linux/regmap.h>
#include "mt8189-reg.h"
#include "mtk-base-afe.h"

enum {
	MTK_AFE_RATE_8K,
	MTK_AFE_RATE_11K,
	MTK_AFE_RATE_12K,
	MTK_AFE_RATE_384K,
	MTK_AFE_RATE_16K,
	MTK_AFE_RATE_22K,
	MTK_AFE_RATE_24K,
	MTK_AFE_RATE_352K,
	MTK_AFE_RATE_32K,
	MTK_AFE_RATE_44K,
	MTK_AFE_RATE_48K,
	MTK_AFE_RATE_88K,
	MTK_AFE_RATE_96K,
	MTK_AFE_RATE_176K,
	MTK_AFE_RATE_192K,
	MTK_AFE_RATE_260K,
};

/* HW IPM 2.0 */
enum {
	MTK_AFE_IPM2P0_RATE_8K = 0x0,
	MTK_AFE_IPM2P0_RATE_11K = 0x1,
	MTK_AFE_IPM2P0_RATE_12K = 0x2,
	MTK_AFE_IPM2P0_RATE_16K = 0x4,
	MTK_AFE_IPM2P0_RATE_22K = 0x5,
	MTK_AFE_IPM2P0_RATE_24K = 0x6,
	MTK_AFE_IPM2P0_RATE_32K = 0x8,
	MTK_AFE_IPM2P0_RATE_44K = 0x9,
	MTK_AFE_IPM2P0_RATE_48K = 0xa,
	MTK_AFE_IPM2P0_RATE_88K = 0xd,
	MTK_AFE_IPM2P0_RATE_96K = 0xe,
	MTK_AFE_IPM2P0_RATE_176K = 0x11,
	MTK_AFE_IPM2P0_RATE_192K = 0x12,
	MTK_AFE_IPM2P0_RATE_352K = 0x15,
	MTK_AFE_IPM2P0_RATE_384K = 0x16,
};

enum {
	MTK_AFE_DAI_MEMIF_RATE_8K,
	MTK_AFE_DAI_MEMIF_RATE_16K,
	MTK_AFE_DAI_MEMIF_RATE_32K,
	MTK_AFE_DAI_MEMIF_RATE_48K,
};

enum {
	MTK_AFE_PCM_RATE_8K,
	MTK_AFE_PCM_RATE_16K,
	MTK_AFE_PCM_RATE_32K,
	MTK_AFE_PCM_RATE_48K,
};

enum {
	MTKAIF_PROTOCOL_1 = 0,
	MTKAIF_PROTOCOL_2,
	MTKAIF_PROTOCOL_2_CLK_P2,
};

enum {
	MTK_AFE_ADDA_DL_GAIN_MUTE = 0,
	MTK_AFE_ADDA_DL_GAIN_NORMAL = 0xf74f,
	/* SA suggest apply -0.3db to audio/speech path */
};

enum {
	MT8189_MEMIF_DL0,
	MT8189_MEMIF_DL1,
	MT8189_MEMIF_DL2,
	MT8189_MEMIF_DL3,
	MT8189_MEMIF_DL4,
	MT8189_MEMIF_DL5,
	MT8189_MEMIF_DL6,
	MT8189_MEMIF_DL7,
	MT8189_MEMIF_DL8,
	MT8189_MEMIF_DL23,
	MT8189_MEMIF_DL24,
	MT8189_MEMIF_DL25,
	MT8189_MEMIF_DL_24CH,
	MT8189_MEMIF_VUL0,
	MT8189_MEMIF_VUL1,
	MT8189_MEMIF_VUL2,
	MT8189_MEMIF_VUL3,
	MT8189_MEMIF_VUL4,
	MT8189_MEMIF_VUL5,
	MT8189_MEMIF_VUL6,
	MT8189_MEMIF_VUL7,
	MT8189_MEMIF_VUL8,
	MT8189_MEMIF_VUL9,
	MT8189_MEMIF_VUL10,
	MT8189_MEMIF_VUL24,
	MT8189_MEMIF_VUL25,
	MT8189_MEMIF_VUL_CM0,
	MT8189_MEMIF_VUL_CM1,
	MT8189_MEMIF_ETDM_IN0,
	MT8189_MEMIF_ETDM_IN1,
	MT8189_MEMIF_HDMI,
	MT8189_MEMIF_NUM,
	MT8189_DAI_ADDA = MT8189_MEMIF_NUM,
	MT8189_DAI_ADDA_CH34,
	MT8189_DAI_ADDA_CH56,
	MT8189_DAI_AP_DMIC,
	MT8189_DAI_AP_DMIC_CH34,
	MT8189_DAI_I2S_IN0,
	MT8189_DAI_I2S_IN1,
	MT8189_DAI_I2S_OUT0,
	MT8189_DAI_I2S_OUT1,
	MT8189_DAI_I2S_OUT4,
	MT8189_DAI_PCM_0,
	MT8189_DAI_TDM,
	MT8189_DAI_TDM_DPTX,
	MT8189_DAI_NUM,
};

/* depends each platform's max i2s num */
#define MT8189_DAI_I2S_MAX_NUM 11
#define MT8189_RECORD_MEMIF MT8189_MEMIF_VUL9
#define MT8189_ECHO_REF_MEMIF MT8189_MEMIF_VUL8
#define MT8189_PRIMARY_MEMIF MT8189_MEMIF_DL0
#define MT8189_FAST_MEMIF MT8189_MEMIF_DL2
#define MT8189_DEEP_MEMIF MT8189_MEMIF_DL3
#define MT8189_VOIP_MEMIF MT8189_MEMIF_DL1
#define MT8189_MMAP_DL_MEMIF MT8189_MEMIF_DL4
#define MT8189_MMAP_UL_MEMIF MT8189_MEMIF_VUL2
#define MT8189_BARGE_IN_MEMIF MT8189_MEMIF_VUL_CM0

/* update irq ID (= enum) from AFE_IRQ_MCU_STATUS */
enum {
	MT8189_IRQ_0,
	MT8189_IRQ_1,
	MT8189_IRQ_2,
	MT8189_IRQ_3,
	MT8189_IRQ_4,
	MT8189_IRQ_5,
	MT8189_IRQ_6,
	MT8189_IRQ_7,
	MT8189_IRQ_8,
	MT8189_IRQ_9,
	MT8189_IRQ_10,
	MT8189_IRQ_11,
	MT8189_IRQ_12,
	MT8189_IRQ_13,
	MT8189_IRQ_14,
	MT8189_IRQ_15,
	MT8189_IRQ_16,
	MT8189_IRQ_17,
	MT8189_IRQ_18,
	MT8189_IRQ_19,
	MT8189_IRQ_20,
	MT8189_IRQ_21,
	MT8189_IRQ_22,
	MT8189_IRQ_23,
	MT8189_IRQ_24,
	MT8189_IRQ_25,
	MT8189_IRQ_26,
	MT8189_IRQ_31,
	MT8189_IRQ_NUM,
};

/* update irq ID (= enum) from AFE_IRQ_MCU_STATUS */
enum {
	MT8189_CUS_IRQ_TDM = 0,  /* used only for TDM */
	MT8189_CUS_IRQ_NUM,
};

enum {
	/* AUDIO_ENGEN_CON0 */
	MT8189_AUDIO_26M_EN_ON,
	MT8189_AUDIO_F3P25M_EN_ON,
	MT8189_AUDIO_APLL1_EN_ON,
	MT8189_AUDIO_APLL2_EN_ON,
	MT8189_AUDIO_F26M_EN_RST,
	MT8189_MULTI_USER_RST,
	MT8189_MULTI_USER_BYPASS,
	/* AUDIO_TOP_CON4 */
	MT8189_CG_AUDIO_HOPPING_CK,
	MT8189_CG_AUDIO_F26M_CK,
	MT8189_CG_APLL1_CK,
	MT8189_CG_APLL2_CK,
	MT8189_PDN_APLL_TUNER2,
	MT8189_PDN_APLL_TUNER1,
	MT8189_AUDIO_CG_NUM,
};

/* MCLK */
enum {
	MT8189_I2SIN0_MCK = 0,
	MT8189_I2SIN1_MCK,
	MT8189_FMI2S_MCK,
	MT8189_TDMOUT_MCK,
	MT8189_TDMOUT_BCK,
	MT8189_MCK_NUM,
};

enum {
	CM0,
	CM1,
	CM_NUM,
};

struct snd_pcm_substream;
struct mtk_base_irq_data;
struct clk;
struct mt8189_afe_private {
	struct clk **clk;
	struct regmap *topckgen;
	struct regmap *apmixed;
	struct regmap *infracfg;
	struct regmap *pmic_regmap;
	int irq_cnt[MT8189_MEMIF_NUM];
	int stf_positive_gain_db;
	int dram_resource_counter;
	int sgen_sel;
	int sgen_mode;
	int sgen_rate;
	int sgen_amplitude;
	/* deep buffer playback */
	int deep_playback_state;
	/* fast playback */
	int fast_playback_state;
	/* mmap playback */
	int mmap_playback_state;
	/* mmap record */
	int mmap_record_state;
	/* primary playback */
	int primary_playback_state;
	/* voip rx */
	int voip_rx_state;
	/* xrun assert */
	int xrun_assert[MT8189_MEMIF_NUM];

	/* dai */
	bool dai_on[MT8189_DAI_NUM];
	void *dai_priv[MT8189_DAI_NUM];

	/* adda */
	int mtkaif_protocol;
	int mtkaif_chosen_phase[4];
	int mtkaif_phase_cycle[4];
	int mtkaif_calibration_num_phase;
	int mtkaif_dmic;
	int mtkaif_dmic_ch34;
	int mtkaif_adda6_only;
	/* support ap_dmic */
	int ap_dmic;

	/* add for vs1 voter */
	/* adda dl/ul is on */
	bool is_adda_dl_on;
	bool is_adda_ul_on;
	/* adda dl vol idx is at maximum */
	bool is_adda_dl_max_vol;
	/* current vote status of vs1 */
	bool is_mt6363_vote;

	/* mck */
	int mck_rate[MT8189_MCK_NUM];

	/* channel merge */
	unsigned int cm_rate[CM_NUM];
	unsigned int cm_channels;
};

struct audio_swpm_data {
	unsigned int afe_on;
	unsigned int user_case;
	unsigned int output_device;
	unsigned int input_device;
	unsigned int adda_mode;
	unsigned int sample_rate;
	unsigned int channel_num;
};

int mt8189_dai_adda_register(struct mtk_base_afe *afe);
int mt8189_dai_i2s_register(struct mtk_base_afe *afe);
int mt8189_dai_pcm_register(struct mtk_base_afe *afe);
int mt8189_dai_tdm_register(struct mtk_base_afe *afe);

#endif
