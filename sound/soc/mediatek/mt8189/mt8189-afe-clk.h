/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mt8189-afe-clk.h  --  MediaTek 8189 afe clock ctrl definition
 *
 * Copyright (c) 2025 MediaTek Inc.
 * Author: Darren Ye <darren.ye@mediatek.com>
 */

#ifndef _MT8189_AFE_CLOCK_CTRL_H_
#define _MT8189_AFE_CLOCK_CTRL_H_

#define APLL1_TUNER_CON0 0x0040

#define APLL2_TUNER_CON0 0x0044

#define AP_PLL_CON3 0x000c
#define PLLEN_ALL 0x0070

#define APLL1_CON0 0x0334
#define APLL1_CON1 0x0338
#define APLL1_CON2 0x033c
#define APLL1_CON4 0x0344

#define APLL2_CON0 0x0348
#define APLL2_CON1 0x034c
#define APLL2_CON2 0x0350
#define APLL2_CON4 0x0358

#define CLK_CFG_6 0x0070
#define CLK_CFG_7 0x0080
#define CLK_CFG_9 0x00a0
#define CLK_CFG_10 0x00b0
#define CLK_CFG_11 0x00c0
#define CLK_CFG_12 0x00d0
#define CLK_CFG_13 0x00e0
#define CLK_CFG_UPDATE 0x004
#define CLK_CFG_UPDATE1 0x008

#define CLK_AUDDIV_0 0x0320
#define CLK_AUDDIV_1 0x0324
#define CLK_AUDDIV_2 0x0328
#define CLK_AUDDIV_3 0x0334
#define CLK_AUDDIV_4 0x0338
#define CLK_AUDDIV_5 0x033c
/* APLL */
#define APLL1_W_NAME "APLL1"
#define APLL2_W_NAME "APLL2"

enum {
	MT8189_APLL1 = 0,
	MT8189_APLL2,
};

enum {
	CLK_HOPPING = 0,
	CLK_F26M,
	CLK_APLL1,
	CLK_APLL2,
	CLK_APLL1_TUNER,
	CLK_APLL2_TUNER,
	CLK_MUX_AUDIOINTBUS,
	CLK_TOP_MAINPLL_D4_D4,
	/* apll related mux */
	CLK_TOP_MUX_AUD_1,
	CLK_TOP_APLL1_CK,
	CLK_TOP_MUX_AUD_2,
	CLK_TOP_APLL2_CK,
	CLK_TOP_MUX_AUD_ENG1,
	CLK_TOP_APLL1_D4,
	CLK_TOP_MUX_AUD_ENG2,
	CLK_TOP_APLL2_D4,/*20*/
	CLK_TOP_MUX_AUDIO_H,
	CLK_TOP_APLL1_D2,
	CLK_TOP_APLL2_D2,
	CLK_TOP_I2SIN0_M_SEL,
	CLK_TOP_I2SIN1_M_SEL,
	CLK_TOP_FMI2S_M_SEL,
	CLK_TOP_TDMOUT_M_SEL,
	CLK_TOP_APLL12_DIV_I2SIN0,
	CLK_TOP_APLL12_DIV_I2SIN1,
	CLK_TOP_APLL12_DIV_FMI2S,
	CLK_TOP_APLL12_DIV_TDMOUT_M,
	CLK_TOP_APLL12_DIV_TDMOUT_B,
	CLK_CLK26M,
	CLK_PERAO_AUDIO_SLV_CK_PERI,
	CLK_PERAO_AUDIO_MST_CK_PERI,
	CLK_PERAO_INTBUS_CK_PERI,
	CLK_NUM
};

struct mtk_base_afe;

int mt8189_init_clock(struct mtk_base_afe *afe);
int mt8189_afe_enable_clock(struct mtk_base_afe *afe);
void mt8189_afe_disable_clock(struct mtk_base_afe *afe);
int mt8189_afe_disable_apll(struct mtk_base_afe *afe);
int mt8189_afe_enable_ao_clock(struct mtk_base_afe *afe);
int mt8189_afe_dram_request(struct device *dev);
int mt8189_afe_dram_release(struct device *dev);
int mt8189_apll1_enable(struct mtk_base_afe *afe);
void mt8189_apll1_disable(struct mtk_base_afe *afe);
int mt8189_apll2_enable(struct mtk_base_afe *afe);
void mt8189_apll2_disable(struct mtk_base_afe *afe);
int mt8189_get_apll_rate(struct mtk_base_afe *afe, int apll);
int mt8189_get_apll_by_rate(struct mtk_base_afe *afe, int rate);
int mt8189_get_apll_by_name(struct mtk_base_afe *afe, const char *name);
extern void aud_intbus_mux_sel(unsigned int aud_idx);
int mt8189_mck_enable(struct mtk_base_afe *afe, int mck_id, int rate);
int mt8189_mck_disable(struct mtk_base_afe *afe, int mck_id);
int mt8189_set_audio_int_bus_parent(struct mtk_base_afe *afe,
				    int clk_id);

#endif
