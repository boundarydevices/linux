// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Qiqi Wang <qiqi.wang@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include <dt-bindings/clock/mediatek,mt8189-clk.h>

#include "clk-mtk.h"
#include "clk-gate.h"

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs afe0_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x0,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs afe1_cg_regs = {
	.set_ofs = 0x10,
	.clr_ofs = 0x10,
	.sta_ofs = 0x10,
};

static const struct mtk_gate_regs afe2_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x4,
	.sta_ofs = 0x4,
};

static const struct mtk_gate_regs afe3_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0x8,
	.sta_ofs = 0x8,
};

static const struct mtk_gate_regs afe4_cg_regs = {
	.set_ofs = 0xC,
	.clr_ofs = 0xC,
	.sta_ofs = 0xC,
};

#define GATE_AFE0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &afe0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_AFE0_V(_id, _name, _parent) {    \
		.id = _id,              \
		.name = _name,              \
		.parent_name = _parent,         \
	}

#define GATE_AFE1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &afe1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_AFE1_V(_id, _name, _parent) {    \
		.id = _id,              \
		.name = _name,              \
		.parent_name = _parent,         \
	}

#define GATE_AFE2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &afe2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_AFE2_V(_id, _name, _parent) {    \
		.id = _id,              \
		.name = _name,              \
		.parent_name = _parent,         \
	}

#define GATE_AFE3(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &afe3_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_AFE3_V(_id, _name, _parent) {    \
		.id = _id,              \
		.name = _name,              \
		.parent_name = _parent,         \
	}

#define GATE_AFE4(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &afe4_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_AFE4_V(_id, _name, _parent) {    \
		.id = _id,              \
		.name = _name,              \
		.parent_name = _parent,         \
	}

static const struct mtk_gate afe_clks[] = {
	/* AFE0 */
	GATE_AFE0(CLK_AFE_DL0_DAC_TML, "afe_dl0_dac_tml",
			"f26m_ck"/* parent */, 7),
	GATE_AFE0(CLK_AFE_DL0_DAC_HIRES, "afe_dl0_dac_hires",
			"audio_h_ck"/* parent */, 8),
	GATE_AFE0(CLK_AFE_DL0_DAC, "afe_dl0_dac",
			"f26m_ck"/* parent */, 9),
	GATE_AFE0(CLK_AFE_DL0_PREDIS, "afe_dl0_predis",
			"f26m_ck"/* parent */, 10),
	GATE_AFE0(CLK_AFE_DL0_NLE, "afe_dl0_nle",
			"f26m_ck"/* parent */, 11),
	GATE_AFE0(CLK_AFE_PCM0, "afe_pcm0",
			"aud_engen1_ck"/* parent */, 14),
	GATE_AFE0(CLK_AFE_CM1, "afe_cm1",
			"f26m_ck"/* parent */, 17),
	GATE_AFE0(CLK_AFE_CM0, "afe_cm0",
			"f26m_ck"/* parent */, 18),
	GATE_AFE0(CLK_AFE_HW_GAIN23, "afe_hw_gain23",
			"f26m_ck"/* parent */, 20),
	GATE_AFE0(CLK_AFE_HW_GAIN01, "afe_hw_gain01",
			"f26m_ck"/* parent */, 21),
	GATE_AFE0(CLK_AFE_FM_I2S, "afe_fm_i2s",
			"aud_engen1_ck"/* parent */, 24),
	GATE_AFE0(CLK_AFE_MTKAIFV4, "afe_mtkaifv4",
			"f26m_ck"/* parent */, 25),
	/* AFE1 */
	GATE_AFE1(CLK_AFE_AUDIO_HOPPING, "afe_audio_hopping_ck",
			"f26m_ck"/* parent */, 0),
	GATE_AFE1(CLK_AFE_AUDIO_F26M, "afe_audio_f26m_ck",
			"f26m_ck"/* parent */, 1),
	GATE_AFE1(CLK_AFE_APLL1, "afe_apll1_ck",
			"aud_1_ck"/* parent */, 2),
	GATE_AFE1(CLK_AFE_APLL2, "afe_apll2_ck",
			"aud_2_ck"/* parent */, 3),
	GATE_AFE1(CLK_AFE_H208M, "afe_h208m_ck",
			"audio_h_ck"/* parent */, 4),
	GATE_AFE1(CLK_AFE_APLL_TUNER2, "afe_apll_tuner2",
			"aud_engen2_ck"/* parent */, 12),
	GATE_AFE1(CLK_AFE_APLL_TUNER1, "afe_apll_tuner1",
			"aud_engen1_ck"/* parent */, 13),
	/* AFE2 */
	GATE_AFE2(CLK_AFE_DMIC1_ADC_HIRES_TML, "afe_dmic1_aht",
			"audio_h_ck"/* parent */, 0),
	GATE_AFE2(CLK_AFE_DMIC1_ADC_HIRES, "afe_dmic1_adc_hires",
			"audio_h_ck"/* parent */, 1),
	GATE_AFE2(CLK_AFE_DMIC1_TML, "afe_dmic1_tml",
			"f26m_ck"/* parent */, 2),
	GATE_AFE2(CLK_AFE_DMIC1_ADC, "afe_dmic1_adc",
			"f26m_ck"/* parent */, 3),
	GATE_AFE2(CLK_AFE_DMIC0_ADC_HIRES_TML, "afe_dmic0_aht",
			"audio_h_ck"/* parent */, 4),
	GATE_AFE2(CLK_AFE_DMIC0_ADC_HIRES, "afe_dmic0_adc_hires",
			"audio_h_ck"/* parent */, 5),
	GATE_AFE2(CLK_AFE_DMIC0_TML, "afe_dmic0_tml",
			"f26m_ck"/* parent */, 6),
	GATE_AFE2(CLK_AFE_DMIC0_ADC, "afe_dmic0_adc",
			"f26m_ck"/* parent */, 7),
	GATE_AFE2(CLK_AFE_UL0_ADC_HIRES_TML, "afe_ul0_aht",
			"audio_h_ck"/* parent */, 20),
	GATE_AFE2(CLK_AFE_UL0_ADC_HIRES, "afe_ul0_adc_hires",
			"audio_h_ck"/* parent */, 21),
	GATE_AFE2(CLK_AFE_UL0_TML, "afe_ul0_tml",
			"f26m_ck"/* parent */, 22),
	GATE_AFE2(CLK_AFE_UL0_ADC, "afe_ul0_adc",
			"f26m_ck"/* parent */, 23),
	/* AFE3 */
	GATE_AFE3(CLK_AFE_ETDM_IN1, "afe_etdm_in1",
			"aud_engen1_ck"/* parent */, 12),
	GATE_AFE3(CLK_AFE_ETDM_IN0, "afe_etdm_in0",
			"aud_engen1_ck"/* parent */, 13),
	GATE_AFE3(CLK_AFE_ETDM_OUT4, "afe_etdm_out4",
			"aud_engen1_ck"/* parent */, 17),
	GATE_AFE3(CLK_AFE_ETDM_OUT1, "afe_etdm_out1",
			"aud_engen1_ck"/* parent */, 20),
	GATE_AFE3(CLK_AFE_ETDM_OUT0, "afe_etdm_out0",
			"aud_engen1_ck"/* parent */, 21),
	GATE_AFE3(CLK_AFE_TDM_OUT, "afe_tdm_out",
			"aud_1_ck"/* parent */, 24),
	/* AFE4 */
	GATE_AFE4(CLK_AFE_GENERAL4_ASRC, "afe_general4_asrc",
			"audio_h_ck"/* parent */, 20),
	GATE_AFE4(CLK_AFE_GENERAL3_ASRC, "afe_general3_asrc",
			"audio_h_ck"/* parent */, 21),
	GATE_AFE4(CLK_AFE_GENERAL2_ASRC, "afe_general2_asrc",
			"audio_h_ck"/* parent */, 22),
	GATE_AFE4(CLK_AFE_GENERAL1_ASRC, "afe_general1_asrc",
			"audio_h_ck"/* parent */, 23),
	GATE_AFE4(CLK_AFE_GENERAL0_ASRC, "afe_general0_asrc",
			"audio_h_ck"/* parent */, 24),
	GATE_AFE4(CLK_AFE_CONNSYS_I2S_ASRC, "afe_connsys_i2s_asrc",
			"audio_h_ck"/* parent */, 25),
};

static const struct mtk_clk_desc afe_mcd = {
	.clks = afe_clks,
	.num_clks = CLK_AFE_NR_CLK,
};

static const struct mtk_gate_regs vad0_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x0,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs vad1_cg_regs = {
	.set_ofs = 0x180,
	.clr_ofs = 0x180,
	.sta_ofs = 0x180,
};

#define GATE_VAD0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vad0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

#define GATE_VAD0_V(_id, _name, _parent) {    \
		.id = _id,              \
		.name = _name,              \
		.parent_name = _parent,         \
	}

#define GATE_VAD1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vad1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_VAD1_V(_id, _name, _parent) {    \
		.id = _id,              \
		.name = _name,              \
		.parent_name = _parent,         \
	}

static const struct mtk_gate vad_clks[] = {
	/* VAD0 */
	GATE_VAD0(CLK_VAD_CORE0_EN, "vad_core0",
			"vlp_vadsp_ck"/* parent */, 0),
	GATE_VAD0(CLK_VAD_BUSEMI_EN, "vad_busemi_en",
			"vlp_vadsp_ck"/* parent */, 2),
	GATE_VAD0(CLK_VAD_TIMER_EN, "vad_timer_en",
			"vlp_vadsp_ck"/* parent */, 3),
	GATE_VAD0(CLK_VAD_DMA0_EN, "vad_dma0_en",
			"vlp_vadsp_ck"/* parent */, 4),
	GATE_VAD0(CLK_VAD_UART_EN, "vad_uart_en",
			"vlp_vadsp_ck"/* parent */, 5),
	GATE_VAD0(CLK_VAD_VOWPLL_EN, "vad_vowpll_en",
			"vlp_vadsp_vowpll_ck"/* parent */, 16),
	/* VAD1 */
	GATE_VAD1(CLK_VADSYS_26M, "vadsys_26m",
			"vlp_vadsp_vlp_26m_ck"/* parent */, 2),
	GATE_VAD1(CLK_VADSYS_BUS, "vadsys_bus",
			"vlp_vadsp_ck"/* parent */, 5),
};

static const struct mtk_clk_desc vad_mcd = {
	.clks = vad_clks,
	.num_clks = CLK_VAD_NR_CLK,
};

static const struct of_device_id of_match_clk_mt8189_adsp[] = {
	{
		.compatible = "mediatek,mt8189-audiosys",
		.data = &afe_mcd,
	}, {
		.compatible = "mediatek,mt8189-vadsys",
		.data = &vad_mcd,
	}, {
		/* sentinel */
	}
};

static int clk_mt8189_adsp_grp_probe(struct platform_device *pdev)
{
	int r;

	r = mtk_clk_simple_probe(pdev);
	if (r)
		dev_info(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

	return r;
}

static struct platform_driver clk_mt8189_adsp_drv = {
	.probe = clk_mt8189_adsp_grp_probe,
	.driver = {
		.name = "clk-mt8189-adsp",
		.of_match_table = of_match_clk_mt8189_adsp,
	},
};

module_platform_driver(clk_mt8189_adsp_drv);
MODULE_LICENSE("GPL");
