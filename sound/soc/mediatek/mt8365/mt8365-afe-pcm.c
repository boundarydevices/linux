/*
 * mt8365-afe-pcm.c  --  Mediatek 8365 ALSA SoC AFE platform driver
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

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/dma-mapping.h>
#include <linux/pm_runtime.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include "mt8365-afe-common.h"
#include "mt8365-afe-utils.h"
#include "mt8365-reg.h"
#include "mt8365-afe-debug.h"
#include "mt8365-afe-controls.h"
#include "../common/mtk-base-afe.h"
#include "../common/mtk-afe-platform-driver.h"
#include "../common/mtk-afe-fe-dai.h"

#define MT8365_I2S0_MCLK_MULTIPLIER 256
#define MT8365_I2S1_MCLK_MULTIPLIER 256
#define MT8365_I2S2_MCLK_MULTIPLIER 256
#define MT8365_I2S3_MCLK_MULTIPLIER 256
#define MT8365_TDM_OUT_MCLK_MULTIPLIER 256
#define MT8365_TDM_IN_MCLK_MULTIPLIER 256

#define AFE_BASE_END_OFFSET 8

#define ARRAYSIZE(array) (sizeof(array) / sizeof((array)[0]))

static unsigned int mGasrc1Out;
static unsigned int mGasrc2Out;
static unsigned int mTdmGasrcOut;
static unsigned int mCM2Input;

static inline u32 rx_frequency_palette(unsigned int fs)
{
	/* *
	 * A = (26M / fs) * 64
	 * B = 8125 / A
	 * return = DEC2HEX(B * 2^23)
	 */
	switch (fs) {
	case FS_8000HZ:    return 0x050000;
	case FS_11025HZ:	  return 0x06E400;
	case FS_12000HZ:	  return 0x078000;
	case FS_16000HZ:  return 0x0A0000;
	case FS_22050HZ:  return 0x0DC800;
	case FS_24000HZ:  return 0x0F0000;
	case FS_32000HZ:	  return 0x140000;
	case FS_44100HZ:	  return 0x1B9000;
	case FS_48000HZ:	  return 0x1E0000;
	case FS_88200HZ:	  return 0x372000;
	case FS_96000HZ:	  return 0x3C0000;
	case FS_176400HZ: return 0x6E4000;
	case FS_192000HZ: return 0x780000;
	default:                  return 0x0;
	}
}

static inline u32 AutoRstThHi(unsigned int fs)
{
	switch (fs) {
	case FS_8000HZ:		return 0x36000;
	case FS_11025HZ:		return 0x27000;
	case FS_12000HZ:		return 0x24000;
	case FS_16000HZ:		return 0x1B000;
	case FS_22050HZ:		return 0x14000;
	case FS_24000HZ:		return 0x12000;
	case FS_32000HZ:		return 0x0D800;
	case FS_44100HZ:		return 0x09D00;
	case FS_48000HZ:		return 0x08E00;
	case FS_88200HZ:		return 0x04E00;
	case FS_96000HZ:		return 0x04800;
	case FS_176400HZ:	return 0x02700;
	case FS_192000HZ:	return 0x02400;
	default:		return 0x0;
	}
}

static inline u32 AutoRstThLo(unsigned int fs)
{
	switch (fs) {
	case FS_8000HZ:		return 0x30000;
	case FS_11025HZ:		return 0x23000;
	case FS_12000HZ:		return 0x20000;
	case FS_16000HZ:		return 0x18000;
	case FS_22050HZ:		return 0x11000;
	case FS_24000HZ:		return 0x0FE00;
	case FS_32000HZ:		return 0x0BE00;
	case FS_44100HZ:		return 0x08A00;
	case FS_48000HZ:		return 0x07F00;
	case FS_88200HZ:		return 0x04500;
	case FS_96000HZ:		return 0x04000;
	case FS_176400HZ:	return 0x02300;
	case FS_192000HZ:	return 0x02000;
	default:		return 0x0;
	}
}

static const u32 *get_iir_coef(unsigned int input_fs,
	unsigned int output_fs, unsigned int *count)
{
#define RATIOVER 9
#define INV_COEF 10
#define NO_NEED 11

	static const u32 IIR_COEF_48_TO_44p1[30] = {
		0x061fb0, 0x0bd256, 0x061fb0, 0xe3a3e6, 0xf0a300, 0x000003,
		0x0e416d, 0x1bb577, 0x0e416d, 0xe59178, 0xf23637, 0x000003,
		0x0c7d72, 0x189060, 0x0c7d72, 0xe96f09, 0xf505b2, 0x000003,
		0x126054, 0x249143, 0x126054, 0xe1fc0c, 0xf4b20a, 0x000002,
		0x000000, 0x323c85, 0x323c85, 0xf76d4e, 0x000000, 0x000002,
	};

	static const u32 IIR_COEF_44p1_TO_32[42] = {
		0x0a6074, 0x0d237a, 0x0a6074, 0xdd8d6c, 0xe0b3f6, 0x000002,
		0x0e41f8, 0x128d48, 0x0e41f8, 0xefc14e, 0xf12d7a, 0x000003,
		0x0cfa60, 0x11e89c, 0x0cfa60, 0xf1b09e, 0xf27205, 0x000003,
		0x15b69c, 0x20e7e4, 0x15b69c, 0xea799a, 0xe9314a, 0x000002,
		0x0f79e2, 0x1a7064, 0x0f79e2, 0xf65e4a, 0xf03d8e, 0x000002,
		0x10c34f, 0x1ffe4b, 0x10c34f, 0x0bbecb, 0xf2bc4b, 0x000001,
		0x000000, 0x23b063, 0x23b063, 0x07335f, 0x000000, 0x000002,
	};

	static const u32 IIR_COEF_48_TO_32[42] = {
		0x0a2a9b, 0x0a2f05, 0x0a2a9b, 0xe73873, 0xe0c525, 0x000002,
		0x0dd4ad, 0x0e765a, 0x0dd4ad, 0xf49808, 0xf14844, 0x000003,
		0x18a8cd, 0x1c40d0, 0x18a8cd, 0xed2aab, 0xe542ec, 0x000002,
		0x13e044, 0x1a47c4, 0x13e044, 0xf44aed, 0xe9acc7, 0x000002,
		0x1abd9c, 0x2a5429, 0x1abd9c, 0xff3441, 0xe0fc5f, 0x000001,
		0x0d86db, 0x193e2e, 0x0d86db, 0x1a6f15, 0xf14507, 0x000001,
		0x000000, 0x1f820c, 0x1f820c, 0x0a1b1f, 0x000000, 0x000002,
	};

	static const u32 IIR_COEF_32_TO_16[48] = {
		0x122893, 0xffadd4, 0x122893, 0x0bc205, 0xc0ee1c, 0x000001,
		0x1bab8a, 0x00750d, 0x1bab8a, 0x06a983, 0xe18a5c, 0x000002,
		0x18f68e, 0x02706f, 0x18f68e, 0x0886a9, 0xe31bcb, 0x000002,
		0x149c05, 0x054487, 0x149c05, 0x0bec31, 0xe5973e, 0x000002,
		0x0ea303, 0x07f24a, 0x0ea303, 0x115ff9, 0xe967b6, 0x000002,
		0x0823fd, 0x085531, 0x0823fd, 0x18d5b4, 0xee8d21, 0x000002,
		0x06888e, 0x0acbbb, 0x06888e, 0x40b55c, 0xe76dce, 0x000001,
		0x000000, 0x2d31a9, 0x2d31a9, 0x23ba4f, 0x000000, 0x000001,
	};

	static const u32 IIR_COEF_96_TO_44p1[48] = {
		0x08b543, 0xfd80f4, 0x08b543, 0x0e2332, 0xe06ed0, 0x000002,
		0x1b6038, 0xf90e7e, 0x1b6038, 0x0ec1ac, 0xe16f66, 0x000002,
		0x188478, 0xfbb921, 0x188478, 0x105859, 0xe2e596, 0x000002,
		0x13eff3, 0xffa707, 0x13eff3, 0x13455c, 0xe533b7, 0x000002,
		0x0dc239, 0x03d458, 0x0dc239, 0x17f120, 0xe8b617, 0x000002,
		0x0745f1, 0x05d790, 0x0745f1, 0x1e3d75, 0xed5f18, 0x000002,
		0x05641f, 0x085e2b, 0x05641f, 0x48efd0, 0xe3e9c8, 0x000001,
		0x000000, 0x28f632, 0x28f632, 0x273905, 0x000000, 0x000001,
	};

	static const u32 IIR_COEF_44p1_TO_16[48] = {
		0x0998fb, 0xf7f925, 0x0998fb, 0x1e54a0, 0xe06605, 0x000002,
		0x0d828e, 0xf50f97, 0x0d828e, 0x0f41b5, 0xf0a999, 0x000003,
		0x17ebeb, 0xee30d8, 0x17ebeb, 0x1f48ca, 0xe2ae88, 0x000002,
		0x12fab5, 0xf46ddc, 0x12fab5, 0x20cc51, 0xe4d068, 0x000002,
		0x0c7ac6, 0xfbd00e, 0x0c7ac6, 0x2337da, 0xe8028c, 0x000002,
		0x060ddc, 0x015b3e, 0x060ddc, 0x266754, 0xec21b6, 0x000002,
		0x0407b5, 0x04f827, 0x0407b5, 0x52e3d0, 0xe0149f, 0x000001,
		0x000000, 0x1f9521, 0x1f9521, 0x2ac116, 0x000000, 0x000001,
	};

	static const u32 IIR_COEF_48_TO_16[48] = {
		0x0955ff, 0xf6544a, 0x0955ff, 0x2474e5, 0xe062e6, 0x000002,
		0x0d4180, 0xf297f4, 0x0d4180, 0x12415b, 0xf0a3b0, 0x000003,
		0x0ba079, 0xf4f0b0, 0x0ba079, 0x1285d3, 0xf1488b, 0x000003,
		0x12247c, 0xf1033c, 0x12247c, 0x2625be, 0xe48e0d, 0x000002,
		0x0b98e0, 0xf96d1a, 0x0b98e0, 0x27e79c, 0xe7798a, 0x000002,
		0x055e3b, 0xffed09, 0x055e3b, 0x2a2e2d, 0xeb2854, 0x000002,
		0x01a934, 0x01ca03, 0x01a934, 0x2c4fea, 0xee93ab, 0x000002,
		0x000000, 0x1c46c5, 0x1c46c5, 0x2d37dc, 0x000000, 0x000001,
	};

	static const u32 IIR_COEF_96_TO_16[48] = {
		0x0805a1, 0xf21ae3, 0x0805a1, 0x3840bb, 0xe02a2e, 0x000002,
		0x0d5dd8, 0xe8f259, 0x0d5dd8, 0x1c0af6, 0xf04700, 0x000003,
		0x0bb422, 0xec08d9, 0x0bb422, 0x1bfccc, 0xf09216, 0x000003,
		0x08fde6, 0xf108be, 0x08fde6, 0x1bf096, 0xf10ae0, 0x000003,
		0x0ae311, 0xeeeda3, 0x0ae311, 0x37c646, 0xe385f5, 0x000002,
		0x044089, 0xfa7242, 0x044089, 0x37a785, 0xe56526, 0x000002,
		0x00c75c, 0xffb947, 0x00c75c, 0x378ba3, 0xe72c5f, 0x000002,
		0x000000, 0x0ef76e, 0x0ef76e, 0x377fda, 0x000000, 0x000001,
	};

	static const struct {
		const u32 *coef;
		unsigned int cnt;
	} iir_coef_tbl_list[8] = {
		/* 0: 0.9188 */
		{ IIR_COEF_48_TO_44p1, ARRAYSIZE(IIR_COEF_48_TO_44p1) },
		/* 1: 0.7256 */
		{ IIR_COEF_44p1_TO_32, ARRAYSIZE(IIR_COEF_44p1_TO_32) },
		/* 2: 0.6667 */
		{ IIR_COEF_48_TO_32, ARRAYSIZE(IIR_COEF_48_TO_32) },
		/* 3: 0.5 */
		{ IIR_COEF_32_TO_16, ARRAYSIZE(IIR_COEF_32_TO_16) },
		/* 4: 0.4594 */
		{ IIR_COEF_96_TO_44p1, ARRAYSIZE(IIR_COEF_96_TO_44p1) },
		/* 5: 0.3628 */
		{ IIR_COEF_44p1_TO_16, ARRAYSIZE(IIR_COEF_44p1_TO_16) },
		/* 6: 0.3333 */
		{ IIR_COEF_48_TO_16, ARRAYSIZE(IIR_COEF_48_TO_16) },
		/* 7: 0.1667 */
		{ IIR_COEF_96_TO_16, ARRAYSIZE(IIR_COEF_96_TO_16) },
	};

	static const u32 freq_new_index[16] = {
		0, 1, 2, 99, 3, 4, 5, 99, 6, 7, 8, 9, 10, 11, 12, 99
	};

	static const u32 iir_coef_tbl_matrix[13][13] = {
		{/*0*/
			NO_NEED, NO_NEED, NO_NEED, NO_NEED, NO_NEED,
			NO_NEED, NO_NEED, NO_NEED, NO_NEED, NO_NEED,
			NO_NEED, NO_NEED, NO_NEED
		},
		{/*1*/
			1, NO_NEED, NO_NEED, NO_NEED, NO_NEED,
			NO_NEED, NO_NEED, NO_NEED, NO_NEED,
			NO_NEED, NO_NEED, NO_NEED, NO_NEED
		},
		{/*2*/
			2, 0, NO_NEED, NO_NEED, NO_NEED, NO_NEED,
			NO_NEED, NO_NEED, NO_NEED, NO_NEED,
			NO_NEED, NO_NEED, NO_NEED
		},
		{/*3*/
			3, INV_COEF, INV_COEF, NO_NEED, NO_NEED,
			NO_NEED, NO_NEED, NO_NEED, NO_NEED,
			NO_NEED, NO_NEED, NO_NEED, NO_NEED
		},
		{/*4*/
			5, 3, INV_COEF, 2, NO_NEED, NO_NEED,
			NO_NEED, NO_NEED, NO_NEED,
			NO_NEED, NO_NEED, NO_NEED, NO_NEED
		},
		{/*5*/
			6, 4, 3, 2, 0, NO_NEED, NO_NEED, NO_NEED,
			NO_NEED, NO_NEED, NO_NEED,
			NO_NEED, NO_NEED
		},
		{/*6*/
			INV_COEF, INV_COEF, INV_COEF, 3, INV_COEF,
			INV_COEF, NO_NEED, NO_NEED, NO_NEED,
			NO_NEED, NO_NEED, NO_NEED
		},
		{/*7*/
			INV_COEF, INV_COEF, INV_COEF, 5, 3,
			INV_COEF, 1, NO_NEED, NO_NEED,
			NO_NEED, NO_NEED, NO_NEED, NO_NEED
		},
		{/*8*/
			7, INV_COEF, INV_COEF, 6, 4, 3, 2, 0, NO_NEED,
			NO_NEED, NO_NEED, NO_NEED, NO_NEED
		},
		{/*9*/
			INV_COEF, INV_COEF, INV_COEF, INV_COEF,
			INV_COEF, INV_COEF, 5, 3, INV_COEF,
			NO_NEED, NO_NEED, NO_NEED, NO_NEED
		},
		{/*10*/
			INV_COEF, INV_COEF, INV_COEF, 7, INV_COEF,
			INV_COEF, 6, 4, 3, 0,
			NO_NEED, NO_NEED, NO_NEED
		},
		{ /*11*/
			RATIOVER, INV_COEF, INV_COEF, INV_COEF,
			INV_COEF, INV_COEF, INV_COEF, INV_COEF,
			INV_COEF, 3, INV_COEF, NO_NEED, NO_NEED
		},
		{/*12*/
			RATIOVER, RATIOVER, INV_COEF, INV_COEF,
			INV_COEF, INV_COEF, 7, INV_COEF,
			INV_COEF, 4, 3, 0, NO_NEED
		},
	};

	const u32 *coef = NULL;
	unsigned int cnt = 0;
	u32 i = freq_new_index[input_fs];
	u32 j = freq_new_index[output_fs];

	if (i >= 13 || j >= 13) {
	} else {
		u32 k = iir_coef_tbl_matrix[i][j];

		if (k >= NO_NEED) {
		} else if (k == RATIOVER) {
		} else if (k == INV_COEF) {
		} else {
			coef = iir_coef_tbl_list[k].coef;
			cnt = iir_coef_tbl_list[k].cnt;
		}
	}
	*count = cnt;
	return coef;
}

static const unsigned int mt8365_afe_backup_list[] = {
	AUDIO_TOP_CON0,
	AFE_CONN0,
	AFE_CONN1,
	AFE_CONN3,
	AFE_CONN4,
	AFE_CONN5,
	AFE_CONN6,
	AFE_CONN7,
	AFE_CONN8,
	AFE_CONN9,
	AFE_CONN10,
	AFE_CONN11,
	AFE_CONN12,
	AFE_CONN13,
	AFE_CONN14,
	AFE_CONN15,
	AFE_CONN16,
	AFE_CONN17,
	AFE_CONN18,
	AFE_CONN19,
	AFE_CONN20,
	AFE_CONN21,
	AFE_CONN26,
	AFE_CONN27,
	AFE_CONN28,
	AFE_CONN29,
	AFE_CONN30,
	AFE_CONN31,
	AFE_CONN32,
	AFE_CONN33,
	AFE_CONN34,
	AFE_CONN35,
	AFE_CONN36,
	AFE_CONN_24BIT,
	AFE_CONN_24BIT_1,
	AFE_DAC_CON0,
	AFE_DAC_CON1,
	AFE_DL1_BASE,
	AFE_DL1_END,
	AFE_DL2_BASE,
	AFE_DL2_END,
	AFE_VUL_BASE,
	AFE_VUL_END,
	AFE_AWB_BASE,
	AFE_AWB_END,
	AFE_VUL3_BASE,
	AFE_VUL3_END,
	AFE_HDMI_OUT_BASE,
	AFE_HDMI_OUT_END,
	AFE_HDMI_IN_2CH_BASE,
	AFE_HDMI_IN_2CH_END,
	AFE_ADDA_UL_DL_CON0,
	AFE_ADDA_DL_SRC2_CON0,
	AFE_ADDA_DL_SRC2_CON1,
	AFE_I2S_CON,
	AFE_I2S_CON1,
	AFE_I2S_CON2,
	AFE_I2S_CON3,
	AFE_ADDA_UL_SRC_CON0,
	AFE_AUD_PAD_TOP,
	AFE_HD_ENGEN_ENABLE,
};

static const struct snd_pcm_hardware mt8365_afe_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP |
		 SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_MMAP_VALID),
	.buffer_bytes_max = 256 * 1024,
	.period_bytes_min = 512,
	.period_bytes_max = 128 * 1024,
	.periods_min = 2,
	.periods_max = 256,
	.fifo_size = 0,
};

struct mt8365_afe_rate {
	unsigned int rate;
	unsigned int reg_val;
};

static const struct mt8365_afe_rate mt8365_afe_fs_rates[] = {
	{ .rate = 8000, .reg_val = MT8365_FS_8K },
	{ .rate = 11025, .reg_val = MT8365_FS_11D025K },
	{ .rate = 12000, .reg_val = MT8365_FS_12K },
	{ .rate = 16000, .reg_val = MT8365_FS_16K },
	{ .rate = 22050, .reg_val = MT8365_FS_22D05K },
	{ .rate = 24000, .reg_val = MT8365_FS_24K },
	{ .rate = 32000, .reg_val = MT8365_FS_32K },
	{ .rate = 44100, .reg_val = MT8365_FS_44D1K },
	{ .rate = 48000, .reg_val = MT8365_FS_48K },
	{ .rate = 88200, .reg_val = MT8365_FS_88D2K },
	{ .rate = 96000, .reg_val = MT8365_FS_96K },
	{ .rate = 176400, .reg_val = MT8365_FS_176D4K },
	{ .rate = 192000, .reg_val = MT8365_FS_192K },
};

static int mt8365_afe_fs_timing(unsigned int rate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mt8365_afe_fs_rates); i++)
		if (mt8365_afe_fs_rates[i].rate == rate)
			return mt8365_afe_fs_rates[i].reg_val;

	return -EINVAL;
}

bool mt8365_afe_rate_supported(unsigned int rate, unsigned int id)
{
	switch (id) {
	case MT8365_AFE_IO_TDM_IN:
		if (rate >= 8000 && rate <= 192000)
			return true;
		break;
	case MT8365_AFE_IO_DMIC:
		if (rate >= 8000 && rate <= 48000)
			return true;
		break;
	default:
		break;
	}

	return false;
}

bool mt8365_afe_channel_supported(unsigned int channel, unsigned int id)
{
	switch (id) {
	case MT8365_AFE_IO_TDM_IN:
		if (channel >= 1 && channel <= 8)
			return true;
		break;
	case MT8365_AFE_IO_DMIC:
		if (channel >= 1 && channel <= 8)
			return true;
		break;
	default:
		break;
	}

	return false;
}

bool mt8365_afe_clk_group_44k(int sample_rate)
{
	if ((sample_rate == 11025)
		|| (sample_rate == 22050)
		|| (sample_rate == 44100)
		|| (sample_rate == 88200)
		|| (sample_rate == 176400))
		return true;
	else
		return false;
}

bool mt8365_afe_clk_group_48k(int sample_rate)
{
	return (!mt8365_afe_clk_group_44k(sample_rate));
}

static int mt8365_afe_irq_direction_enable(struct mtk_base_afe *afe,
	int irq_id,
	int direction)
{
	struct mtk_base_afe_irq *irq;

	if (irq_id >= MT8365_AFE_IRQ_NUM)
		return -1;

	irq = &afe->irqs[irq_id];

	if (direction == MT8365_AFE_IRQ_DIR_MCU) {
		regmap_update_bits(afe->regmap, AFE_IRQ_MCU_DSP_EN,
		       (1 << irq->irq_data->irq_clr_shift),
		       0);
		regmap_update_bits(afe->regmap, AFE_IRQ_MCU_EN,
		       (1 << irq->irq_data->irq_clr_shift),
		       (1 << irq->irq_data->irq_clr_shift));
	} else if (direction == MT8365_AFE_IRQ_DIR_DSP) {
		regmap_update_bits(afe->regmap, AFE_IRQ_MCU_DSP_EN,
		       (1 << irq->irq_data->irq_clr_shift),
		       (1 << irq->irq_data->irq_clr_shift));
		regmap_update_bits(afe->regmap, AFE_IRQ_MCU_EN,
		       (1 << irq->irq_data->irq_clr_shift),
		       0);
	} else {
		regmap_update_bits(afe->regmap, AFE_IRQ_MCU_DSP_EN,
		       (1 << irq->irq_data->irq_clr_shift),
		       (1 << irq->irq_data->irq_clr_shift));
		regmap_update_bits(afe->regmap, AFE_IRQ_MCU_EN,
		       (1 << irq->irq_data->irq_clr_shift),
		       (1 << irq->irq_data->irq_clr_shift));
	}
	return 0;
}

static int mt8365_afe_set_i2s_out(struct mtk_base_afe *afe, unsigned int rate,
	int bit_width)
{
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	unsigned int val;
	int fs = mt8365_afe_fs_timing(rate);

	if (fs < 0)
		return -EINVAL;

	val = AFE_I2S_CON1_I2S2_TO_PAD |
	      AFE_I2S_CON1_LOW_JITTER_CLK |
	      AFE_I2S_CON1_RATE(fs) |
	      AFE_I2S_CON1_FORMAT_I2S;

	/* 1:bck=32lrck(16bit) or bck=64lrck(32bit) 0:fix bck=64lrck */
	if (afe_priv->i2s_dynamic_bck) {
		if (bit_width > 16)
			val |= AFE_I2S_CON1_WLEN_32BIT;
		else
			val &= ~(u32)AFE_I2S_CON1_WLEN_32BIT;
	} else
		val |= AFE_I2S_CON1_WLEN_32BIT;

	regmap_update_bits(afe->regmap, AFE_I2S_CON1,
		~(u32)AFE_I2S_CON1_EN, val);

	return 0;
}

static int mt8365_afe_set_2nd_i2s_out(struct mtk_base_afe *afe,
	unsigned int rate, int bit_width)
{
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	unsigned int val;
	int fs = mt8365_afe_fs_timing(rate);

	if (fs < 0)
		return -EINVAL;

	val = AFE_I2S_CON3_LOW_JITTER_CLK |
	      AFE_I2S_CON3_RATE(fs) |
	      AFE_I2S_CON3_FORMAT_I2S;

	/* 1:bck=32lrck(16bit) or bck=64lrck(32bit) 0:fix bck=64lrck */
	if (afe_priv->i2s_dynamic_bck) {
		if (bit_width > 16)
			val |= AFE_I2S_CON3_WLEN_32BIT;
		else
			val &= ~(u32)AFE_I2S_CON3_WLEN_32BIT;
	} else
		val |= AFE_I2S_CON3_WLEN_32BIT;

	regmap_update_bits(afe->regmap, AFE_I2S_CON3,
		~(u32)AFE_I2S_CON3_EN, val);

	return 0;
}

static int mt8365_afe_set_i2s_in(struct mtk_base_afe *afe, unsigned int rate,
	int bit_width)
{
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	unsigned int val;
	int fs = mt8365_afe_fs_timing(rate);

	if (fs < 0)
		return -EINVAL;

	val = AFE_I2S_CON2_LOW_JITTER_CLK |
	      AFE_I2S_CON2_RATE(fs) |
	      AFE_I2S_CON2_FORMAT_I2S;

	/* 1:bck=32lrck(16bit) or bck=64lrck(32bit) 0:fix bck=64lrck */
	if (afe_priv->i2s_dynamic_bck) {
		if (bit_width > 16)
			val |= AFE_I2S_CON2_WLEN_32BIT;
		else
			val &= AFE_I2S_CON2_WLEN_32BIT;
	} else
		val |= AFE_I2S_CON2_WLEN_32BIT;

	regmap_update_bits(afe->regmap, AFE_I2S_CON2,
		~(u32)AFE_I2S_CON2_EN, val);

	regmap_update_bits(afe->regmap, AFE_ADDA_TOP_CON0, 0x1, 0x1);

	return 0;
}

static int mt8365_afe_set_2nd_i2s_asrc(struct mtk_base_afe *afe,
	unsigned int rate_in,
	unsigned int rate_out, unsigned int width,
	unsigned int mono, int o16bit, int tracking)
{
	int ifs, ofs = 0;
	unsigned int val = 0;
	unsigned int mask = 0;
	const u32 *coef;
	u32 iir_stage;
	unsigned int coef_count = 0;

	ifs = mt8365_afe_fs_timing(rate_in);

	if (ifs < 0)
		return -EINVAL;

	ofs = mt8365_afe_fs_timing(rate_out);

	if (ifs < 0)
		return -EINVAL;

	regmap_update_bits(afe->regmap, AFE_ASRC_2CH_CON2,
		O16BIT_MASK | IS_MONO_MASK, o16bit << 19 | mono << 16);

	coef = get_iir_coef(ifs, ofs, &coef_count);
	iir_stage = ((u32)coef_count / 6) - 1;

	if (coef) {
		unsigned int i;

		/* CPU control IIR coeff SRAM */
		regmap_update_bits(afe->regmap, AFE_ASRC_2CH_CON0,
		0x1 << 1, 0x1 << 1);

		/* set to 0, IIR coeff SRAM addr */
		regmap_update_bits(afe->regmap, AFE_ASRC_2CH_CON13,
		0xffffffff, 0x0);

		for (i = 0; i < coef_count; ++i)
			regmap_update_bits(afe->regmap, AFE_ASRC_2CH_CON12,
			0xffffffff, coef[i]);

		/* disable IIR coeff SRAM access */
		regmap_update_bits(afe->regmap, AFE_ASRC_2CH_CON0,
		0x0 << 1, 0x1 << 1);
		regmap_update_bits(afe->regmap, AFE_ASRC_2CH_CON2,
			CLR_IIR_HISTORY_MASK | IIR_EN_MASK |
			IIR_STAGE_MASK,
			CLR_IIR_HISTORY | IIR_EN | iir_stage << 8);
	} else {
		regmap_update_bits(afe->regmap, AFE_ASRC_2CH_CON2,
			IIR_EN_MASK, IIR_DIS);
	}

	/* CON3 setting (RX OFS) */
	regmap_update_bits(afe->regmap, AFE_ASRC_2CH_CON3,
	0x00FFFFFF, rx_frequency_palette(ofs));
	/* CON4 setting (RX IFS) */
	regmap_update_bits(afe->regmap, AFE_ASRC_2CH_CON4,
	0x00FFFFFF, rx_frequency_palette(ifs));

	/* CON5 setting */
	if (tracking) {
		val = CALI_64_CYCLE |
			CALI_AUTORST_ENABLE |
			AUTO_TUNE_FREQ5_ENABLE |
			COMP_FREQ_RES_ENABLE |
			CALI_BP_DGL_BYPASS |
			AUTO_TUNE_FREQ4_DISABLE |
			CALI_AUTO_RESTART_ENABLE |
			CALI_USE_FREQ |
			CALI_SEL_01 |
			CALI_OFF;

		mask = CALI_CYCLE_MASK |
			CALI_AUTORST_MASK |
			AUTO_TUNE_FREQ5_MASK |
			COMP_FREQ_RES_MASK |
			CALI_SEL_MASK |
			CALI_BP_DGL_MASK |
			AUTO_TUNE_FREQ4_MASK |
			CALI_AUTO_RESTART_MASK |
			CALI_USE_FREQ_OUT_MASK |
			CALI_ON_MASK;
		regmap_update_bits(afe->regmap, AFE_ASRC_2CH_CON5,
			mask, val);
		regmap_update_bits(afe->regmap, AFE_ASRC_2CH_CON5,
			CALI_ON_MASK, CALI_ON);
	} else {
		regmap_update_bits(afe->regmap, AFE_ASRC_2CH_CON5,
			0xffffffff, 0x0);
	}
	/* CON6 setting fix 8125 */
	regmap_update_bits(afe->regmap, AFE_ASRC_2CH_CON6,
		0x0000ffff, 0x1FBD);
	/* CON9 setting (RX IFS) */
	regmap_update_bits(afe->regmap, AFE_ASRC_2CH_CON9,
		0x000fffff, AutoRstThHi(ifs));
	/* CON10 setting (RX IFS) */
	regmap_update_bits(afe->regmap, AFE_ASRC_2CH_CON10,
		0x000fffff, AutoRstThLo(ifs));
	regmap_update_bits(afe->regmap, AFE_ASRC_2CH_CON0,
		CHSET_STR_CLR, 0x1 << 4);

	return 0;
}

static int mt8365_afe_set_2nd_i2s_asrc_enable(
	struct mtk_base_afe *afe, bool enable)
{
	if (enable)
		regmap_update_bits(afe->regmap, AFE_ASRC_2CH_CON0,
			ASM_ON_MASK, ASM_ON);
	else
		regmap_update_bits(afe->regmap, AFE_ASRC_2CH_CON0,
			ASM_ON_MASK, 0);
	return 0;
}

static int mt8365_afe_set_2nd_i2s_in(struct mtk_base_afe *afe,
	unsigned int rate,
	int bit_width)
{
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	struct mt8365_be_dai_data *be =
	&afe_priv->be_data[MT8365_AFE_IO_2ND_I2S - MT8365_AFE_BACKEND_BASE];

	unsigned int val;
	int fs = mt8365_afe_fs_timing(rate);

	if (fs < 0)
		return -EINVAL;

	val = AFE_I2S_CON_FROM_IO_MUX |
	      AFE_I2S_CON_LOW_JITTER_CLK |
	      AFE_I2S_CON_RATE(fs) |
	      AFE_I2S_CON_FORMAT_I2S;

	/* 1:bck=32lrck(16bit) or bck=64lrck(32bit) 0:fix bck=64lrck */
	if (afe_priv->i2s_dynamic_bck) {
		if (bit_width > 16)
			val |= AFE_I2S_CON_WLEN_32BIT;
		else
			val &= ~(u32)AFE_I2S_CON_WLEN_32BIT;
	} else
		val |= AFE_I2S_CON_WLEN_32BIT;

	if ((be->fmt_mode & SND_SOC_DAIFMT_MASTER_MASK)
		== SND_SOC_DAIFMT_CBM_CFM) {
		val |= AFE_I2S_CON_SRC_SLAVE;
		val &= ~(u32)AFE_I2S_CON_FROM_IO_MUX;//from consys
	}

	regmap_update_bits(afe->regmap, AFE_I2S_CON,
		~(u32)AFE_I2S_CON_EN, val);

	return 0;
}

static void mt8365_afe_set_i2s_out_enable(struct mtk_base_afe *afe, bool enable)
{
	unsigned long flags;
	struct mt8365_afe_private *afe_priv = afe->platform_priv;

	spin_lock_irqsave(&afe_priv->afe_ctrl_lock, flags);

	if (enable) {
		afe_priv->i2s_out_on_ref_cnt++;
		if (afe_priv->i2s_out_on_ref_cnt == 1)
			regmap_update_bits(afe->regmap,
			AFE_I2S_CON1, 0x1, enable);
	} else {
		afe_priv->i2s_out_on_ref_cnt--;
		if (afe_priv->i2s_out_on_ref_cnt == 0)
			regmap_update_bits(afe->regmap,
			AFE_I2S_CON1, 0x1, enable);
		else if (afe_priv->i2s_out_on_ref_cnt < 0)
			afe_priv->i2s_out_on_ref_cnt = 0;
	}

	spin_unlock_irqrestore(&afe_priv->afe_ctrl_lock, flags);
}

static void mt8365_afe_set_2nd_i2s_out_enable(struct mtk_base_afe *afe,
	bool enable)
{
	regmap_update_bits(afe->regmap, AFE_I2S_CON3, 0x1, enable);
}

static void mt8365_afe_set_i2s_in_enable(struct mtk_base_afe *afe,
	bool enable)
{
	regmap_update_bits(afe->regmap, AFE_I2S_CON2, 0x1, enable);
}

static void mt8365_afe_set_2nd_i2s_in_enable(struct mtk_base_afe *afe,
	bool enable)
{
	regmap_update_bits(afe->regmap, AFE_I2S_CON, 0x1, enable);
}

static int mt8365_afe_set_adda_out(struct mtk_base_afe *afe, unsigned int rate)
{
	unsigned int val = 0;

	switch (rate) {
	case 8000:
		val |= (0 << 28) | AFE_ADDA_DL_VOICE_DATA;
		break;
	case 11025:
		val |= 1 << 28;
		break;
	case 12000:
		val |= 2 << 28;
		break;
	case 16000:
		val |= (3 << 28) | AFE_ADDA_DL_VOICE_DATA;
		break;
	case 22050:
		val |= 4 << 28;
		break;
	case 24000:
		val |= 5 << 28;
		break;
	case 32000:
		val |= 6 << 28;
		break;
	case 44100:
		val |= 7 << 28;
		break;
	case 48000:
		val |= 8 << 28;
		break;
	default:
		return -EINVAL;
	}

	val |= AFE_ADDA_DL_8X_UPSAMPLE |
	       AFE_ADDA_DL_MUTE_OFF |
	       AFE_ADDA_DL_DEGRADE_GAIN;

	regmap_update_bits(afe->regmap, AFE_ADDA_PREDIS_CON0, 0xffffffff, 0);
	regmap_update_bits(afe->regmap, AFE_ADDA_PREDIS_CON1, 0xffffffff, 0);
	regmap_update_bits(afe->regmap, AFE_ADDA_DL_SRC2_CON0, 0xffffffff, val);
	/* SA suggest apply -0.3db to audio/speech path */
	regmap_update_bits(afe->regmap, AFE_ADDA_DL_SRC2_CON1,
		0xffffffff, 0xf74f0000);
	/* SA suggest use default value for sdm */
	regmap_update_bits(afe->regmap, AFE_ADDA_DL_SDM_DCCOMP_CON,
		0xffffffff, 0x0700701e);

	return 0;
}

static int mt8365_afe_set_adda_in(struct mtk_base_afe *afe, unsigned int rate)
{
	unsigned int val = 0;

	switch (rate) {
	case 8000:
		val |= (0 << 17);
		break;
	case 16000:
		val |= (1 << 17);
		break;
	case 32000:
		val |= (2 << 17);
		break;
	case 48000:
		val |= (3 << 17);
		break;
	default:
		return -EINVAL;
	}

	regmap_update_bits(afe->regmap, AFE_ADDA_UL_SRC_CON0, 0x000e0000, val);
	/* Using Internal ADC */
	regmap_update_bits(afe->regmap, AFE_ADDA_TOP_CON0, 0x1, 0x0);

	return 0;
}

static int mt8365_afe_enable_adda_on(struct mtk_base_afe *afe)
{
	unsigned long flags;
	struct mt8365_afe_private *afe_priv = afe->platform_priv;

	spin_lock_irqsave(&afe_priv->afe_ctrl_lock, flags);

	afe_priv->adda_afe_on_ref_cnt++;
	if (afe_priv->adda_afe_on_ref_cnt == 1)
		regmap_update_bits(afe->regmap, AFE_ADDA_UL_DL_CON0,
			AFE_ADDA_UL_DL_ADDA_AFE_ON_MASK,
			AFE_ADDA_UL_DL_ADDA_AFE_ON);

	spin_unlock_irqrestore(&afe_priv->afe_ctrl_lock, flags);

	return 0;
}

static int mt8365_afe_disable_adda_on(struct mtk_base_afe *afe)
{
	unsigned long flags;
	struct mt8365_afe_private *afe_priv = afe->platform_priv;

	spin_lock_irqsave(&afe_priv->afe_ctrl_lock, flags);

	afe_priv->adda_afe_on_ref_cnt--;
	if (afe_priv->adda_afe_on_ref_cnt == 0)
		regmap_update_bits(afe->regmap, AFE_ADDA_UL_DL_CON0,
			AFE_ADDA_UL_DL_ADDA_AFE_ON_MASK,
			AFE_ADDA_UL_DL_ADDA_AFE_OFF);
	else if (afe_priv->adda_afe_on_ref_cnt < 0)
		afe_priv->adda_afe_on_ref_cnt = 0;

	spin_unlock_irqrestore(&afe_priv->afe_ctrl_lock, flags);

	return 0;
}

static void mt8365_afe_set_adda_out_enable(struct mtk_base_afe *afe,
	bool enable)
{
	regmap_update_bits(afe->regmap, AFE_ADDA_DL_SRC2_CON0, 0x1, enable);

	if (enable)
		mt8365_afe_enable_adda_on(afe);
	else
		mt8365_afe_disable_adda_on(afe);
}

static void mt8365_afe_set_adda_in_enable(struct mtk_base_afe *afe,
	bool enable)
{
	if (enable) {
		regmap_update_bits(afe->regmap, AFE_ADDA_UL_SRC_CON0, 0x1, 0x1);
		mt8365_afe_enable_adda_on(afe);
		/* enable aud_pad_top fifo */
		regmap_update_bits(afe->regmap, AFE_AUD_PAD_TOP,
			0xffffffff, 0x31);
	} else {
		/* disable aud_pad_top fifo */
		regmap_update_bits(afe->regmap, AFE_AUD_PAD_TOP,
			0xffffffff, 0x30);
		regmap_update_bits(afe->regmap, AFE_ADDA_UL_SRC_CON0, 0x1, 0x0);
		/* de suggest disable ADDA_UL_SRC at least wait 125us */
		udelay(150);
		mt8365_afe_disable_adda_on(afe);
	}
}

static unsigned int mt8365_afe_tdm_ch_fixup(unsigned int channels)
{
	if (channels > 4)
		return 8;
	else if (channels > 2)
		return 4;
	else
		return 2;
}

static unsigned int mt8365_afe_tdm_out_ch_per_sdata(unsigned int mode,
	unsigned int channels)
{
	if (mode == MT8365_AFE_TDM_OUT_TDM)
		return mt8365_afe_tdm_ch_fixup(channels);
	else
		return 2;
}

static int mt8365_afe_tdm_out_bitwidth_fixup(unsigned int mode,
	int bitwidth)
{
	if (mode == MT8365_AFE_TDM_OUT_I2S_32BITS)
		return 32;
	else
		return bitwidth;
}

static int mt8365_afe_tdm_out_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8365_afe_private *afe_priv = afe->platform_priv;

	mt8365_afe_enable_main_clk(afe);

	mt8365_afe_enable_top_cg(afe, MT8365_TOP_CG_TDM_OUT);
	mt8365_afe_enable_clk(afe,
		afe_priv->clocks[MT8365_CLK_AUD_TDMOUT_M]);

	mt8365_afe_enable_clk(afe,
		afe_priv->clocks[MT8365_CLK_AUD_TDMOUT_B]);

	return 0;
}

static void mt8365_afe_tdm_out_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	struct mt8365_be_dai_data *be =
		&afe_priv->be_data[dai->id - MT8365_AFE_BACKEND_BASE];
	const unsigned int rate = substream->runtime->rate;
	const unsigned int stream = substream->stream;

	if (be->prepared[stream]) {
		/* disable tdm */
		regmap_update_bits(afe->regmap, AFE_TDM_CON1, 0x1, 0);


		if (rate % 8000)
			mt8365_afe_disable_apll_associated_cfg(afe,
			MT8365_AFE_APLL1);
		else
			mt8365_afe_disable_apll_associated_cfg(afe,
			MT8365_AFE_APLL2);

		be->prepared[stream] = false;
	}

	mt8365_afe_disable_clk(afe,
		afe_priv->clocks[MT8365_CLK_AUD_TDMOUT_M]);

	mt8365_afe_disable_clk(afe,
		afe_priv->clocks[MT8365_CLK_AUD_TDMOUT_B]);

	mt8365_afe_disable_top_cg(afe, MT8365_TOP_CG_TDM_OUT);
	mt8365_afe_disable_main_clk(afe);

}

static int mt8365_afe_tdm_out_prepare(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_pcm_runtime * const runtime = substream->runtime;
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	struct mt8365_be_dai_data *be =
		&afe_priv->be_data[dai->id - MT8365_AFE_BACKEND_BASE];
	const unsigned int tdm_out_mode = afe_priv->tdm_out_mode;
	const unsigned int rate = runtime->rate;
	const unsigned int channels = runtime->channels;
	const unsigned int out_channels_per_sdata =
		mt8365_afe_tdm_out_ch_per_sdata(tdm_out_mode,
		runtime->channels);
	const int bit_width = snd_pcm_format_width(runtime->format);
	const int out_bit_width =
		mt8365_afe_tdm_out_bitwidth_fixup(tdm_out_mode,
		bit_width);
	const unsigned int stream = substream->stream;
	unsigned int val;
	unsigned int bck_inverse = 0;

	if (be->prepared[stream]) {
		dev_info(afe->dev, "%s prepared already\n", __func__);
		return 0;
	}

	if (rate % 8000) {
		mt8365_afe_enable_apll_associated_cfg(afe, MT8365_AFE_APLL1);
		mt8365_afe_set_clk_parent(afe,
			afe_priv->clocks[MT8365_CLK_TDMOUT_M_SEL],
			afe_priv->clocks[MT8365_CLK_AUD1]);
	} else {
		mt8365_afe_enable_apll_associated_cfg(afe, MT8365_AFE_APLL2);
		mt8365_afe_set_clk_parent(afe,
			afe_priv->clocks[MT8365_CLK_TDMOUT_M_SEL],
			afe_priv->clocks[MT8365_CLK_AUD2]);
	}

	mt8365_afe_set_clk_rate(afe, afe_priv->clocks[MT8365_CLK_AUD_TDMOUT_M],
		rate * MT8365_TDM_OUT_MCLK_MULTIPLIER);

	mt8365_afe_set_clk_rate(afe, afe_priv->clocks[MT8365_CLK_AUD_TDMOUT_B],
		rate * out_channels_per_sdata * out_bit_width);

	val = AFE_TDM_CON1_MSB_ALIGNED;

	if ((tdm_out_mode == MT8365_AFE_TDM_OUT_I2S ||
	     tdm_out_mode == MT8365_AFE_TDM_OUT_I2S_32BITS) &&
	    (be->fmt_mode & SND_SOC_DAIFMT_FORMAT_MASK) == SND_SOC_DAIFMT_I2S) {
		val |= AFE_TDM_CON1_1_BCK_DELAY |
		       AFE_TDM_CON1_LRCK_INV;
		bck_inverse = AUD_TCON3_HDMI_BCK_INV;
	} else if (tdm_out_mode == MT8365_AFE_TDM_OUT_TDM) {
		val |= AFE_TDM_CON1_1_BCK_DELAY;
		bck_inverse = AUD_TCON3_HDMI_BCK_INV;
	}

	/* bit width related */
	if (out_bit_width > 16) {
		val |= AFE_TDM_CON1_WLEN_32BIT |
		       AFE_TDM_CON1_32_BCK_CYCLES |
		       AFE_TDM_CON1_LRCK_WIDTH(32);
	} else {
		val |= AFE_TDM_CON1_WLEN_16BIT |
		       AFE_TDM_CON1_16_BCK_CYCLES |
		       AFE_TDM_CON1_LRCK_WIDTH(16);
	}

	/* channel per sdata */
	if (out_channels_per_sdata > 4)
		val |= AFE_TDM_CON1_8CH_PER_SDATA;
	else if (out_channels_per_sdata > 2)
		val |= AFE_TDM_CON1_4CH_PER_SDATA;
	else
		val |= AFE_TDM_CON1_2CH_PER_SDATA;

	regmap_update_bits(afe->regmap, AFE_TDM_CON1,
		~(u32)AFE_TDM_CON1_EN, val);

	/* set tdm2 config */
	if (out_channels_per_sdata == 2) {
		switch (channels) {
		case 1:
			fallthrough;
		case 2:
			val = AFE_TDM_CH_START_O28_O29;
			val |= (AFE_TDM_CH_ZERO << 4);
			val |= (AFE_TDM_CH_ZERO << 8);
			val |= (AFE_TDM_CH_ZERO << 12);
			break;
		case 3:
			fallthrough;
		case 4:
			val = AFE_TDM_CH_START_O28_O29;
			val |= (AFE_TDM_CH_START_O30_O31 << 4);
			val |= (AFE_TDM_CH_ZERO << 8);
			val |= (AFE_TDM_CH_ZERO << 12);
			break;
		case 5:
			fallthrough;
		case 6:
			val = AFE_TDM_CH_START_O28_O29;
			val |= (AFE_TDM_CH_START_O30_O31 << 4);
			val |= (AFE_TDM_CH_START_O32_O33 << 8);
			val |= (AFE_TDM_CH_ZERO << 12);
			break;
		case 7:
			fallthrough;
		case 8:
			val = AFE_TDM_CH_START_O28_O29;
			val |= (AFE_TDM_CH_START_O30_O31 << 4);
			val |= (AFE_TDM_CH_START_O32_O33 << 8);
			val |= (AFE_TDM_CH_START_O34_O35 << 12);
			break;
		default:
			val = 0;
		}
	} else {
		val = AFE_TDM_CH_START_O28_O29;
		val |= (AFE_TDM_CH_ZERO << 4);
		val |= (AFE_TDM_CH_ZERO << 8);
		val |= (AFE_TDM_CH_ZERO << 12);
	}

	regmap_update_bits(afe->regmap, AFE_TDM_CON2,
			   AFE_TDM_CON2_SOUT_MASK, val);

	regmap_update_bits(afe->regmap, AUDIO_TOP_CON3,
			   AUD_TCON3_HDMI_BCK_INV, bck_inverse);

	regmap_update_bits(afe->regmap, AFE_HDMI_OUT_CON0,
			   AFE_HDMI_OUT_CON0_CH_MASK, channels << 4);


	/* enable tdm */
	regmap_update_bits(afe->regmap, AFE_TDM_CON1, 0x1, 0x1);

	be->prepared[stream] = true;

	return 0;
}

static int mt8365_afe_tdm_out_trigger(struct snd_pcm_substream *substream,
	int cmd, struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);

	dev_info(afe->dev, "%s cmd=%d %s\n", __func__, cmd, dai->name);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		regmap_write(afe->regmap, AFE_HDMI_CONN0,
		     AFE_HDMI_CONN0_O28_I28 | AFE_HDMI_CONN0_O29_I29 |
		     AFE_HDMI_CONN0_O30_I30 | AFE_HDMI_CONN0_O31_I31 |
		     AFE_HDMI_CONN0_O32_I32 | AFE_HDMI_CONN0_O33_I33 |
		     AFE_HDMI_CONN0_O34_I34 | AFE_HDMI_CONN0_O35_I35);

		/* enable Out control */
		regmap_update_bits(afe->regmap, AFE_HDMI_OUT_CON0, 0x1, 0x1);
		return 0;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		/* disable Out control */
		regmap_update_bits(afe->regmap, AFE_HDMI_OUT_CON0, 0x1, 0);
		return 0;
	default:
		return -EINVAL;
	}
}

static int mt8365_afe_tdm_out_set_fmt(struct snd_soc_dai *dai,
				unsigned int fmt)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	struct mt8365_be_dai_data *be =
		&afe_priv->be_data[dai->id - MT8365_AFE_BACKEND_BASE];

	be->fmt_mode = 0;
	/* set DAI format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
	case SND_SOC_DAIFMT_LEFT_J:
		be->fmt_mode |= fmt & SND_SOC_DAIFMT_FORMAT_MASK;
		break;
	default:
		dev_err(afe->dev, "invalid dai format %u\n", fmt);
		return -EINVAL;
	}
	return 0;
}

static int mt8365_afe_tdm_in_startup(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8365_afe_private *afe_priv = afe->platform_priv;

	mt8365_afe_enable_main_clk(afe);

	mt8365_afe_enable_top_cg(afe, MT8365_TOP_CG_TDM_IN);
	mt8365_afe_enable_clk(afe,
		afe_priv->clocks[MT8365_CLK_AUD_TDMIN_M]);

	mt8365_afe_enable_clk(afe,
		afe_priv->clocks[MT8365_CLK_AUD_TDMIN_B]);

	return 0;
}

static void mt8365_afe_tdm_in_shutdown(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	struct mt8365_be_dai_data *be =
		&afe_priv->be_data[dai->id - MT8365_AFE_BACKEND_BASE];
	unsigned int rate = dai->rate;
	const unsigned int stream = substream->stream;

	if (be->prepared[stream]) {
		if (rate % 8000)
			mt8365_afe_disable_apll_associated_cfg(afe,
				MT8365_AFE_APLL1);
		else
			mt8365_afe_disable_apll_associated_cfg(afe,
				MT8365_AFE_APLL2);

		be->prepared[stream] = false;
	}

	mt8365_afe_disable_clk(afe,
		afe_priv->clocks[MT8365_CLK_AUD_TDMIN_M]);

	mt8365_afe_disable_clk(afe,
		afe_priv->clocks[MT8365_CLK_AUD_TDMIN_B]);

	mt8365_afe_disable_top_cg(afe, MT8365_TOP_CG_TDM_IN);
	mt8365_afe_disable_main_clk(afe);
}

static int mt8365_afe_tdm_in_prepare(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	struct mt8365_be_dai_data *be =
		&afe_priv->be_data[dai->id - MT8365_AFE_BACKEND_BASE];
	unsigned int channels = dai->channels;
	unsigned int rate = dai->rate;
	unsigned int bit_width = dai->sample_bits;
	const unsigned int stream = substream->stream;
	unsigned int val;
	unsigned int bck;

	dev_info(afe->dev, "%s channels %d rate %d bit_width %d\n", __func__,
		channels, rate, bit_width);

	if (be->prepared[stream]) {
		dev_info(afe->dev, "%s prepared already\n", __func__);
		return 0;
	}

	if (rate % 8000) {
		mt8365_afe_enable_apll_associated_cfg(afe, MT8365_AFE_APLL1);
		mt8365_afe_set_clk_parent(afe,
			afe_priv->clocks[MT8365_CLK_TDMIN_M_SEL],
			afe_priv->clocks[MT8365_CLK_AUD1]);
	} else {
		mt8365_afe_enable_apll_associated_cfg(afe, MT8365_AFE_APLL2);
		mt8365_afe_set_clk_parent(afe,
			afe_priv->clocks[MT8365_CLK_TDMIN_M_SEL],
			afe_priv->clocks[MT8365_CLK_AUD2]);
	}

	bck = mt8365_afe_tdm_ch_fixup(channels) * rate * bit_width;

	mt8365_afe_set_clk_rate(afe, afe_priv->clocks[MT8365_CLK_AUD_TDMIN_M],
		rate * MT8365_TDM_IN_MCLK_MULTIPLIER);

	mt8365_afe_set_clk_rate(afe, afe_priv->clocks[MT8365_CLK_AUD_TDMIN_B],
		bck);

	val = 0;

	if ((be->fmt_mode & SND_SOC_DAIFMT_FORMAT_MASK) == SND_SOC_DAIFMT_I2S)
		val |= AFE_TDM_IN_CON1_I2S;

	/* bck&lrck phase */
	switch (be->fmt_mode & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_IB_IF:
		val |= AFE_TDM_IN_CON1_LRCK_INV |
		       AFE_TDM_IN_CON1_BCK_INV;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		val |= AFE_TDM_IN_CON1_LRCK_INV;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		val |= AFE_TDM_IN_CON1_BCK_INV;
		break;
	default:
		break;
	}

	/* bit width */
	if (bit_width > 16) {
		val |= AFE_TDM_IN_CON1_WLEN_32BIT |
		       AFE_TDM_IN_CON1_FAST_LRCK_CYCLE_32BCK |
		       AFE_TDM_IN_CON1_LRCK_WIDTH(32);
	} else {
		val |= AFE_TDM_IN_CON1_WLEN_16BIT |
		       AFE_TDM_IN_CON1_FAST_LRCK_CYCLE_16BCK |
		       AFE_TDM_IN_CON1_LRCK_WIDTH(16);
	}

	switch (channels) {
	case 2:
		val |= AFE_TDM_IN_CON1_2CH_PER_SDATA;
		break;
	case 4:
		val |= AFE_TDM_IN_CON1_4CH_PER_SDATA;
		break;
	case 6:
		val |= AFE_TDM_IN_CON1_8CH_PER_SDATA;
		regmap_update_bits(afe->regmap, AFE_TDM_IN_CON2,
			AFE_TDM_IN_CON2_DISABLE_CH67,
			AFE_TDM_IN_CON2_DISABLE_CH67);
		break;
	case 8:
		val |= AFE_TDM_IN_CON1_8CH_PER_SDATA;
		break;
	default:
		break;
	}

	regmap_update_bits(afe->regmap, AFE_TDM_IN_CON1,
			   ~(u32)AFE_TDM_IN_CON1_EN, val);

	be->prepared[stream] = true;

	return 0;
}

static int mt8365_afe_tdm_in_trigger(struct snd_pcm_substream *substream,
	int cmd, struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
			fallthrough;
	case SNDRV_PCM_TRIGGER_RESUME:
		regmap_update_bits(afe->regmap, AFE_CONN_TDMIN_CON,
			AFE_CONN_TDMIN_CON0_MASK,
			AFE_CONN_TDMIN_O40_I40 | AFE_CONN_TDMIN_O41_I41);

		regmap_update_bits(afe->regmap, AFE_HDMI_IN_2CH_CON0, 0x1, 0x1);

		/* enable tdm in */
		regmap_update_bits(afe->regmap, AFE_TDM_IN_CON1, 0x1, 0x1);
		return 0;
	case SNDRV_PCM_TRIGGER_STOP:
			fallthrough;
	case SNDRV_PCM_TRIGGER_SUSPEND:
		/* disable tdm in */
		regmap_update_bits(afe->regmap, AFE_TDM_IN_CON1, 0x1, 0);

		regmap_update_bits(afe->regmap, AFE_HDMI_IN_2CH_CON0, 0x1, 0);
		return 0;
	default:
		return -EINVAL;
	}

	return 0;
}

static int mt8365_afe_tdm_in_set_fmt(struct snd_soc_dai *dai,
				unsigned int fmt)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	struct mt8365_be_dai_data *be =
		&afe_priv->be_data[dai->id - MT8365_AFE_BACKEND_BASE];

	be->fmt_mode = 0;
	/* set DAI format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
			fallthrough;
	case SND_SOC_DAIFMT_LEFT_J:
		be->fmt_mode |= fmt & SND_SOC_DAIFMT_FORMAT_MASK;
		break;
	default:
		dev_err(afe->dev, "invalid dai format %u\n", fmt);
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
			fallthrough;
	case SND_SOC_DAIFMT_NB_IF:
			fallthrough;
	case SND_SOC_DAIFMT_IB_NF:
			fallthrough;
	case SND_SOC_DAIFMT_IB_IF:
		be->fmt_mode |= fmt & SND_SOC_DAIFMT_INV_MASK;
		break;
	default:
		dev_err(afe->dev, "invalid dai format %u\n", fmt);
		return -EINVAL;
	}

	return 0;
}

static int mt8365_memif_fs(struct snd_pcm_substream *substream,
	unsigned int rate)
{
	return mt8365_afe_fs_timing(rate);
}

static int mt8365_irq_fs(struct snd_pcm_substream *substream,
	unsigned int rate)
{
	return mt8365_memif_fs(substream, rate);
}

static const struct mt8365_cm_ctrl_reg cm_ctrl_reg[MT8365_CM_NUM] = {
	[MT8365_CM1] = {
		.con0 = AFE_CM1_CON0,
		.con1 = AFE_CM1_CON1,
		.con2 = AFE_CM1_CON2,
		.con3 = AFE_CM1_CON3,
		.con4 = AFE_CM1_CON4,
	},
	[MT8365_CM2] = {
		.con0 = AFE_CM2_CON0,
		.con1 = AFE_CM2_CON1,
		.con2 = AFE_CM2_CON2,
		.con3 = AFE_CM2_CON3,
		.con4 = AFE_CM2_CON4,
	}
};

static int mt8365_afe_cm2_mux_conn(struct mtk_base_afe *afe)
{
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	unsigned int input = afe_priv->cm2_mux_input;

	dev_dbg(afe->dev, "%s input %d\n", __func__, input);

	/* TDM_IN interconnect to CM2 */
	regmap_update_bits(afe->regmap, AFE_CM2_CONN0,
		CM2_AFE_CM2_CONN_CFG1_MASK,
		CM2_AFE_CM2_CONN_CFG1(TDM_IN_CH0));
	regmap_update_bits(afe->regmap, AFE_CM2_CONN0,
		CM2_AFE_CM2_CONN_CFG2_MASK,
		CM2_AFE_CM2_CONN_CFG2(TDM_IN_CH1));
	regmap_update_bits(afe->regmap, AFE_CM2_CONN0,
		CM2_AFE_CM2_CONN_CFG3_MASK,
		CM2_AFE_CM2_CONN_CFG3(TDM_IN_CH2));
	regmap_update_bits(afe->regmap, AFE_CM2_CONN0,
		CM2_AFE_CM2_CONN_CFG4_MASK,
		CM2_AFE_CM2_CONN_CFG4(TDM_IN_CH3));
	regmap_update_bits(afe->regmap, AFE_CM2_CONN0,
		CM2_AFE_CM2_CONN_CFG5_MASK,
		CM2_AFE_CM2_CONN_CFG5(TDM_IN_CH4));
	regmap_update_bits(afe->regmap, AFE_CM2_CONN0,
		CM2_AFE_CM2_CONN_CFG6_MASK,
		CM2_AFE_CM2_CONN_CFG6(TDM_IN_CH5));
	regmap_update_bits(afe->regmap, AFE_CM2_CONN1,
		CM2_AFE_CM2_CONN_CFG7_MASK,
		CM2_AFE_CM2_CONN_CFG7(TDM_IN_CH6));
	regmap_update_bits(afe->regmap, AFE_CM2_CONN1,
		CM2_AFE_CM2_CONN_CFG8_MASK,
		CM2_AFE_CM2_CONN_CFG8(TDM_IN_CH7));

	/* ref data interconnect to CM2 */
	if (input == MT8365_FROM_GASRC1) {
		regmap_update_bits(afe->regmap, AFE_CM2_CONN1,
			CM2_AFE_CM2_CONN_CFG9_MASK,
			CM2_AFE_CM2_CONN_CFG9(GENERAL1_ASRC_OUT_LCH));
		regmap_update_bits(afe->regmap, AFE_CM2_CONN1,
			CM2_AFE_CM2_CONN_CFG10_MASK,
			CM2_AFE_CM2_CONN_CFG10(GENERAL1_ASRC_OUT_RCH));
	} else if (input == MT8365_FROM_GASRC2) {
		regmap_update_bits(afe->regmap, AFE_CM2_CONN1,
			CM2_AFE_CM2_CONN_CFG9_MASK,
			CM2_AFE_CM2_CONN_CFG9(GENERAL2_ASRC_OUT_LCH));
		regmap_update_bits(afe->regmap, AFE_CM2_CONN1,
			CM2_AFE_CM2_CONN_CFG10_MASK,
			CM2_AFE_CM2_CONN_CFG10(GENERAL2_ASRC_OUT_RCH));
	} else if (input == MT8365_FROM_TDM_ASRC) {
		regmap_update_bits(afe->regmap, AFE_CM2_CONN1,
			CM2_AFE_CM2_CONN_CFG9_MASK,
			CM2_AFE_CM2_CONN_CFG9(TDM_OUT_ASRC_CH0));
		regmap_update_bits(afe->regmap, AFE_CM2_CONN1,
			CM2_AFE_CM2_CONN_CFG10_MASK,
			CM2_AFE_CM2_CONN_CFG10(TDM_OUT_ASRC_CH1));
		regmap_update_bits(afe->regmap, AFE_CM2_CONN1,
			CM2_AFE_CM2_CONN_CFG11_MASK,
			CM2_AFE_CM2_CONN_CFG11(TDM_OUT_ASRC_CH2));
		regmap_update_bits(afe->regmap, AFE_CM2_CONN1,
			CM2_AFE_CM2_CONN_CFG12_MASK,
			CM2_AFE_CM2_CONN_CFG12(TDM_OUT_ASRC_CH3));
		regmap_update_bits(afe->regmap, AFE_CM2_CONN2,
			CM2_AFE_CM2_CONN_CFG13_MASK,
			CM2_AFE_CM2_CONN_CFG13(TDM_OUT_ASRC_CH4));
		regmap_update_bits(afe->regmap, AFE_CM2_CONN2,
			CM2_AFE_CM2_CONN_CFG14_MASK,
			CM2_AFE_CM2_CONN_CFG14(TDM_OUT_ASRC_CH5));
		regmap_update_bits(afe->regmap, AFE_CM2_CONN2,
			CM2_AFE_CM2_CONN_CFG15_MASK,
			CM2_AFE_CM2_CONN_CFG15(TDM_OUT_ASRC_CH6));
		regmap_update_bits(afe->regmap, AFE_CM2_CONN2,
			CM2_AFE_CM2_CONN_CFG16_MASK,
			CM2_AFE_CM2_CONN_CFG16(TDM_OUT_ASRC_CH7));
	} else {
		dev_dbg(afe->dev, "%s wrong CM2 input %d\n",
				__func__, input);
		return -1;
	}

	return 0;
}

static int mt8365_afe_get_cm_update_cnt(struct mtk_base_afe *afe,
	enum mt8365_cm_num cmNum, unsigned int rate, unsigned int channel)
{

	/* caculate cm update cnt */
	/* total_cnt = clk / fs, clk is 26m or 24m or 22m*/
	/* div_cnt = total_cnt / ch_pair, max ch 16ch ,2ch is a set */
	/* best_cnt < div_cnt ,we set best_cnt = div_cnt -10 */
	/* ch01 = best_cnt, ch23 = 2* ch01_up_cnt */
	/* ch45 = 3* ch01_up_cnt ...ch1415 = 8* ch01_up_cnt */

	unsigned int total_cnt, div_cnt, ch_pair, best_cnt;
	unsigned int ch_update_cnt[MT8365_CM_UPDATA_CNT_SET];
	int i;

	if (cmNum == MT8365_CM1) {
		total_cnt = MT8365_CLK_26M / rate;
	} else if (cmNum == MT8365_CM2) {
		if (mt8365_afe_clk_group_48k(rate))
			total_cnt = MT8365_CLK_24M / rate;
		else
			total_cnt = MT8365_CLK_22M / rate;
	} else {
		dev_dbg(afe->dev, "%s wrong cmNum %d\n",
			__func__, cmNum);
		return -1;
	}

	if (channel % 2)
		ch_pair = (channel / 2) + 1;
	else
		ch_pair = channel / 2;

	div_cnt =  total_cnt / ch_pair;
	best_cnt = div_cnt - 10;

	if (best_cnt <= 0) {
		dev_dbg(afe->dev, "%s wrong best_cnt %d\n",
			__func__, best_cnt);
		return -1;
	}

	for (i = 0; i < ch_pair; i++)
		ch_update_cnt[i] = (i + 1) * best_cnt;

	dev_dbg(afe->dev, "%s channel %d ch_pair %d\n",
		__func__, channel, ch_pair);

	switch (channel) {
	case 16:
		fallthrough;
	case 15:
		regmap_update_bits(afe->regmap, cm_ctrl_reg[cmNum].con4,
			CM_AFE_CM_UPDATE_CNT2_MASK,
			CM_AFE_CM_UPDATE_CNT2(ch_update_cnt[7]));
		fallthrough;
	case 14:
		fallthrough;
	case 13:
		regmap_update_bits(afe->regmap, cm_ctrl_reg[cmNum].con4,
			CM_AFE_CM_UPDATE_CNT1_MASK,
			CM_AFE_CM_UPDATE_CNT1(ch_update_cnt[6]));
		fallthrough;
	case 12:
		fallthrough;
	case 11:
		regmap_update_bits(afe->regmap, cm_ctrl_reg[cmNum].con3,
			CM_AFE_CM_UPDATE_CNT2_MASK,
			CM_AFE_CM_UPDATE_CNT2(ch_update_cnt[5]));
		fallthrough;
	case 10:
		fallthrough;
	case 9:
		regmap_update_bits(afe->regmap, cm_ctrl_reg[cmNum].con3,
			CM_AFE_CM_UPDATE_CNT1_MASK,
			CM_AFE_CM_UPDATE_CNT1(ch_update_cnt[4]));
		fallthrough;
	case 8:
		fallthrough;
	case 7:
		regmap_update_bits(afe->regmap, cm_ctrl_reg[cmNum].con2,
			CM_AFE_CM_UPDATE_CNT2_MASK,
			CM_AFE_CM_UPDATE_CNT2(ch_update_cnt[3]));
		fallthrough;
	case 6:
		fallthrough;
	case 5:
		regmap_update_bits(afe->regmap, cm_ctrl_reg[cmNum].con2,
			CM_AFE_CM_UPDATE_CNT1_MASK,
			CM_AFE_CM_UPDATE_CNT1(ch_update_cnt[2]));
		fallthrough;
	case 4:
		fallthrough;
	case 3:
		regmap_update_bits(afe->regmap, cm_ctrl_reg[cmNum].con1,
			CM_AFE_CM_UPDATE_CNT2_MASK,
			CM_AFE_CM_UPDATE_CNT2(ch_update_cnt[1]));
		fallthrough;
	case 2:
		fallthrough;
	case 1:
		regmap_update_bits(afe->regmap, cm_ctrl_reg[cmNum].con1,
			CM_AFE_CM_UPDATE_CNT1_MASK,
			CM_AFE_CM_UPDATE_CNT1(ch_update_cnt[0]));
		break;
	default:
		return -1;
	}

	return 0;
}

static int mt8365_afe_configure_cm(struct mtk_base_afe *afe,
	enum mt8365_cm_num cmNum,
	unsigned int channels,
	unsigned int rate)
{
	unsigned int val, mask;
	unsigned int fs = mt8365_afe_fs_timing(rate);

	val = CM_AFE_CM_CH_NUM(channels)
		| CM_AFE_CM_START_DATA(0);

	mask = CM_AFE_CM_CH_NUM_MASK
		| CM_AFE_CM_START_DATA_MASK;

	if (cmNum == MT8365_CM1) {
		val |= CM_AFE_CM1_VUL_SEL_CM1_OUTPUT
			| CM_AFE_CM1_IN_MODE(fs);

		mask |= CM_AFE_CM1_VUL_SEL
			 | CM_AFE_CM1_IN_MODE_MASK;
	} else if (cmNum == MT8365_CM2) {
		if (mt8365_afe_clk_group_48k(rate))
			val |= CM_AFE_CM2_CLK_SEL_24M;
		else
			val |= CM_AFE_CM2_CLK_SEL_22M;

		val |= CM_AFE_CM2_TDM_SEL_CM2_OUTPUT;

		mask |= CM_AFE_CM2_TDM_SEL
			 | CM_AFE_CM1_IN_MODE_MASK
			 | CM_AFE_CM2_CLK_SEL;

		mt8365_afe_cm2_mux_conn(afe);
	} else {
		dev_dbg(afe->dev, "%s wrong cmNum %d\n",
			__func__, cmNum);
		return -1;
	}

	regmap_update_bits(afe->regmap, cm_ctrl_reg[cmNum].con0, mask, val);

	mt8365_afe_get_cm_update_cnt(afe, cmNum, rate, channels);

	return 0;
}

int mt8365_afe_fe_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct snd_pcm_runtime *runtime = substream->runtime;
	int memif_num = asoc_rtd_to_cpu(rtd, 0)->id;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];
	const struct snd_pcm_hardware *mtk_afe_hardware = afe->mtk_afe_hardware;
	int ret;

	memif->substream = substream;

	snd_pcm_hw_constraint_step(substream->runtime, 0,
		SNDRV_PCM_HW_PARAM_BUFFER_BYTES, 16);

	snd_soc_set_runtime_hwparams(substream, mtk_afe_hardware);

	ret = snd_pcm_hw_constraint_integer(runtime,
		SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		dev_err(afe->dev, "snd_pcm_hw_constraint_integer failed\n");

	mt8365_afe_enable_main_clk(afe);
	return ret;
}

static void mt8365_afe_fe_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	int memif_num = asoc_rtd_to_cpu(rtd, 0)->id;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];

	dev_dbg(afe->dev, "%s %s\n", __func__, memif->data->name);

	memif->substream = NULL;

	mt8365_afe_disable_main_clk(afe);
}

static int mt8365_afe_fe_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	int dai_id = asoc_rtd_to_cpu(rtd, 0)->id;
	struct mt8365_control_data *ctrl_data = &afe_priv->ctrl_data;
	struct mtk_base_afe_memif *memif = &afe->memif[dai_id];
	struct mt8365_fe_dai_data *fe_data = &afe_priv->fe_data[dai_id];
	const size_t request_size = params_buffer_bytes(params);
	unsigned int channels = params_channels(params);
	unsigned int rate = params_rate(params);
	int ret, fs = 0;
	unsigned int base_end_offset = 8;

	dev_info(afe->dev,
		"%s %s period = %d rate = %d channels = %d\n",
		__func__, memif->data->name, params_period_size(params),
		rate, channels);

	if (dai_id == MT8365_AFE_MEMIF_VUL2) {
		if (!ctrl_data->bypass_cm1)
			/* configure cm1 */
			mt8365_afe_configure_cm(afe,
				MT8365_CM1, channels, rate);
		else
			regmap_update_bits(afe->regmap, AFE_CM1_CON0,
				CM_AFE_CM1_VUL_SEL,
				CM_AFE_CM1_VUL_SEL_O17_O18);
	} else if (dai_id == MT8365_AFE_MEMIF_TDM_IN) {
		if (!ctrl_data->bypass_cm2)
			/* configure cm2 */
			mt8365_afe_configure_cm(afe,
				MT8365_CM2, channels, rate);
		else
			regmap_update_bits(afe->regmap, AFE_CM2_CON0,
				CM_AFE_CM2_TDM_SEL,
				CM_AFE_CM2_TDM_SEL_TDM);

		base_end_offset = 4;
	}

	if (request_size > fe_data->sram_size) {
		ret = snd_pcm_lib_malloc_pages(substream, request_size);
		if (ret < 0) {
			dev_err(afe->dev,
				"%s %s malloc pages %zu bytes failed %d\n",
				__func__, memif->data->name, request_size, ret);
			return ret;
		}

		fe_data->use_sram = false;

		mt8365_afe_emi_clk_on(afe);
	} else {
		struct snd_dma_buffer *dma_buf = &substream->dma_buffer;

		dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
		dma_buf->dev.dev = substream->pcm->card->dev;
		dma_buf->area = (unsigned char *)fe_data->sram_vir_addr;
		dma_buf->addr = fe_data->sram_phy_addr;
		dma_buf->bytes = request_size;
		snd_pcm_set_runtime_buffer(substream, dma_buf);

		fe_data->use_sram = true;
	}

	memif->phys_buf_addr = lower_32_bits(substream->runtime->dma_addr);
	memif->buffer_size = substream->runtime->dma_bytes;

	/* start */
	regmap_write(afe->regmap, memif->data->reg_ofs_base,
		memif->phys_buf_addr);
	/* end */
	regmap_write(afe->regmap,
		memif->data->reg_ofs_base + base_end_offset,
		memif->phys_buf_addr + memif->buffer_size - 1);

	/* set channel */
	if (memif->data->mono_shift >= 0) {
		unsigned int mono = (params_channels(params) == 1) ? 1 : 0;

		if (memif->data->mono_reg < 0)
			dev_info(afe->dev, "%s mono_reg is NULL\n", __func__);
		else
			regmap_update_bits(afe->regmap, memif->data->mono_reg,
				1 << memif->data->mono_shift,
				mono << memif->data->mono_shift);
	}

	/* set rate */
	if (memif->data->fs_shift < 0)
		return 0;

	fs = afe->memif_fs(substream, params_rate(params));

	if (fs < 0)
		return -EINVAL;

	if (memif->data->fs_reg < 0)
		dev_info(afe->dev, "%s fs_reg is NULL\n", __func__);
	else
		regmap_update_bits(afe->regmap, memif->data->fs_reg,
			memif->data->fs_maskbit << memif->data->fs_shift,
			fs << memif->data->fs_shift);

	return 0;
}

static int mt8365_afe_fe_hw_free(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	int dai_id = asoc_rtd_to_cpu(rtd, 0)->id;
	struct mtk_base_afe_memif *memif = &afe->memif[dai_id];
	struct mt8365_fe_dai_data *fe_data = &afe_priv->fe_data[dai_id];
	int ret = 0;

	dev_dbg(afe->dev, "%s %s\n", __func__, memif->data->name);

	if (fe_data->use_sram) {
		snd_pcm_set_runtime_buffer(substream, NULL);
	} else {
		ret = snd_pcm_lib_free_pages(substream);

		mt8365_afe_emi_clk_off(afe);
	}

	return ret;
}

static int mt8365_afe_fe_prepare(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	int dai_id = asoc_rtd_to_cpu(rtd, 0)->id;
	struct mtk_base_afe_memif *memif = &afe->memif[dai_id];

	/* set format */
	if (memif->data->hd_reg >= 0) {
		switch (substream->runtime->format) {
		case SNDRV_PCM_FORMAT_S16_LE:
			regmap_update_bits(afe->regmap, memif->data->hd_reg,
				3 << memif->data->hd_shift,
				0 << memif->data->hd_shift);
			break;
		case SNDRV_PCM_FORMAT_S32_LE:
			regmap_update_bits(afe->regmap, memif->data->hd_reg,
				3 << memif->data->hd_shift,
				3 << memif->data->hd_shift);

			if (dai_id == MT8365_AFE_MEMIF_TDM_IN) {
				regmap_update_bits(afe->regmap,
					memif->data->hd_reg,
					3 << memif->data->hd_shift,
					1 << memif->data->hd_shift);
				regmap_update_bits(afe->regmap,
					memif->data->hd_reg,
					1 << memif->data->hd_align_mshift,
					1 << memif->data->hd_align_mshift);
			}
			break;
		case SNDRV_PCM_FORMAT_S24_LE:
			regmap_update_bits(afe->regmap, memif->data->hd_reg,
				3 << memif->data->hd_shift,
				1 << memif->data->hd_shift);
			break;
		default:
			return -EINVAL;
		}
	}

	mt8365_afe_irq_direction_enable(afe,
		memif->irq_usage,
		MT8365_AFE_IRQ_DIR_MCU);

	return 0;
}

int mt8365_afe_fe_trigger(struct snd_pcm_substream *substream, int cmd,
	struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	int dai_id = asoc_rtd_to_cpu(rtd, 0)->id;
	struct mt8365_control_data *ctrl_data = &afe_priv->ctrl_data;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		/* enable channel merge */
		if (dai_id == MT8365_AFE_MEMIF_VUL2 &&
		    !ctrl_data->bypass_cm1) {
			regmap_update_bits(afe->regmap,
				AFE_CM1_CON0,
				CM_AFE_CM_ON, CM_AFE_CM_ON);
		} else if (dai_id == MT8365_AFE_MEMIF_TDM_IN &&
			   !ctrl_data->bypass_cm2) {
			regmap_update_bits(afe->regmap,
				AFE_CM2_CON0,
				CM_AFE_CM_ON, CM_AFE_CM_ON);
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		/* disable channel merge */
		if (dai_id == MT8365_AFE_MEMIF_VUL2 &&
		    !ctrl_data->bypass_cm1) {
			regmap_update_bits(afe->regmap,
				AFE_CM1_CON0,
				CM_AFE_CM_ON, CM_AFE_CM_OFF);
		} else if (dai_id == MT8365_AFE_MEMIF_TDM_IN &&
			   !ctrl_data->bypass_cm2) {
			regmap_update_bits(afe->regmap,
				AFE_CM2_CON0,
				CM_AFE_CM_ON, CM_AFE_CM_OFF);
		}
		break;
	default:
		break;
	}

	return mtk_afe_fe_trigger(substream, cmd, dai);
}

static int mt8365_afe_i2s_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	const unsigned int clk_mode =
		afe_priv->i2s_clk_modes[MT8365_AFE_1ST_I2S];

	dev_dbg(afe->dev, "%s '%s'\n",
		__func__, snd_pcm_stream_str(substream));

	if (clk_mode == MT8365_AFE_I2S_SHARED_CLOCK && snd_soc_dai_active(dai))
		return 0;

	mt8365_afe_enable_main_clk(afe);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK ||
	    clk_mode == MT8365_AFE_I2S_SHARED_CLOCK)
		mt8365_afe_enable_clk(afe,
		afe_priv->clocks[MT8365_CLK_AUD_I2S1_M]);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE ||
	    clk_mode == MT8365_AFE_I2S_SHARED_CLOCK)
		mt8365_afe_enable_clk(afe,
		afe_priv->clocks[MT8365_CLK_AUD_I2S2_M]);

	return 0;
}

static void mt8365_afe_i2s_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	struct mt8365_be_dai_data *be =
		&afe_priv->be_data[dai->id - MT8365_AFE_BACKEND_BASE];
	const unsigned int rate = substream->runtime->rate;
	const unsigned int stream = substream->stream;
	const unsigned int clk_mode =
		afe_priv->i2s_clk_modes[MT8365_AFE_1ST_I2S];
	const bool reset_i2s_out_change =
		(stream == SNDRV_PCM_STREAM_PLAYBACK) ||
		(clk_mode == MT8365_AFE_I2S_SHARED_CLOCK);
	const bool reset_i2s_in_change =
		(stream == SNDRV_PCM_STREAM_CAPTURE) ||
		(clk_mode == MT8365_AFE_I2S_SHARED_CLOCK);

	dev_dbg(afe->dev, "%s '%s'\n",
		__func__, snd_pcm_stream_str(substream));

	if (clk_mode == MT8365_AFE_I2S_SHARED_CLOCK && snd_soc_dai_active(dai))
		return;

	if (be->prepared[stream]) {
		if (reset_i2s_out_change)
			mt8365_afe_set_i2s_out_enable(afe, false);

		if (reset_i2s_in_change)
			mt8365_afe_set_i2s_in_enable(afe, false);

		if (rate % 8000)
			mt8365_afe_disable_apll_associated_cfg(afe,
			MT8365_AFE_APLL1);
		else
			mt8365_afe_disable_apll_associated_cfg(afe,
			MT8365_AFE_APLL2);

		if (reset_i2s_out_change)
			be->prepared[SNDRV_PCM_STREAM_PLAYBACK] = false;

		if (reset_i2s_in_change)
			be->prepared[SNDRV_PCM_STREAM_CAPTURE] = false;
	}

	if (reset_i2s_out_change)
		mt8365_afe_disable_clk(afe,
		afe_priv->clocks[MT8365_CLK_AUD_I2S1_M]);

	if (reset_i2s_in_change)
		mt8365_afe_disable_clk(afe,
		afe_priv->clocks[MT8365_CLK_AUD_I2S2_M]);

	mt8365_afe_disable_main_clk(afe);
}

static int mt8365_afe_i2s_prepare(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	struct mt8365_be_dai_data *be =
		&afe_priv->be_data[dai->id - MT8365_AFE_BACKEND_BASE];
	const unsigned int rate = substream->runtime->rate;
	const int bit_width = snd_pcm_format_width(substream->runtime->format);
	const unsigned int stream = substream->stream;
	const unsigned int clk_mode =
		afe_priv->i2s_clk_modes[MT8365_AFE_1ST_I2S];
	const bool apply_i2s_out_change =
		(stream == SNDRV_PCM_STREAM_PLAYBACK) ||
		(clk_mode == MT8365_AFE_I2S_SHARED_CLOCK);
	const bool apply_i2s_in_change =
		(stream == SNDRV_PCM_STREAM_CAPTURE) ||
		(clk_mode == MT8365_AFE_I2S_SHARED_CLOCK);
	int ret;

	if ((clk_mode == MT8365_AFE_I2S_SHARED_CLOCK) &&
	    (dai->playback_widget->power || dai->capture_widget->power)) {
		dev_dbg(afe->dev, "%s '%s' widget powered(%u-%u) already\n",
			__func__, snd_pcm_stream_str(substream),
			dai->playback_widget->power,
			dai->capture_widget->power);
		return 0;
	}

	if (be->prepared[stream]) {
		dev_info(afe->dev, "%s '%s' prepared already\n",
			 __func__, snd_pcm_stream_str(substream));
		return 0;
	}

	if (apply_i2s_out_change) {
		ret = mt8365_afe_set_i2s_out(afe, rate, bit_width);
		if (ret)
			return ret;
	}

	if (apply_i2s_in_change) {
		ret = mt8365_afe_set_i2s_in(afe, rate, bit_width);
		if (ret)
			return ret;
	}

	if (rate % 8000)
		mt8365_afe_enable_apll_associated_cfg(afe, MT8365_AFE_APLL1);
	else
		mt8365_afe_enable_apll_associated_cfg(afe, MT8365_AFE_APLL2);

	if (apply_i2s_out_change) {
		mt8365_afe_set_clk_parent(afe,
			afe_priv->clocks[MT8365_CLK_I2S1_M_SEL],
			((rate % 8000) ?
			afe_priv->clocks[MT8365_CLK_AUD1] :
			afe_priv->clocks[MT8365_CLK_AUD2]));

		mt8365_afe_set_clk_rate(afe,
			afe_priv->clocks[MT8365_CLK_AUD_I2S1_M],
			rate * MT8365_I2S1_MCLK_MULTIPLIER);

		mt8365_afe_set_i2s_out_enable(afe, true);

		be->prepared[SNDRV_PCM_STREAM_PLAYBACK] = true;
	}

	if (apply_i2s_in_change) {
		mt8365_afe_set_clk_parent(afe,
			afe_priv->clocks[MT8365_CLK_I2S2_M_SEL],
			((rate % 8000) ?
			afe_priv->clocks[MT8365_CLK_AUD1] :
			afe_priv->clocks[MT8365_CLK_AUD2]));

		mt8365_afe_set_clk_rate(afe,
			afe_priv->clocks[MT8365_CLK_AUD_I2S2_M],
			rate * MT8365_I2S2_MCLK_MULTIPLIER);

		mt8365_afe_set_i2s_in_enable(afe, true);

		be->prepared[SNDRV_PCM_STREAM_CAPTURE] = true;
	}

	return 0;
}

static int mt8365_afe_2nd_i2s_hw_params(struct snd_pcm_substream *substream,
			  struct snd_pcm_hw_params *params,
			  struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	unsigned int width_val = params_width(params) > 16 ?
		(AFE_CONN_24BIT_O00 | AFE_CONN_24BIT_O01) : 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		regmap_update_bits(afe->regmap, AFE_CONN_24BIT,
			   AFE_CONN_24BIT_O00 | AFE_CONN_24BIT_O01, width_val);

	return 0;
}

static int mt8365_afe_2nd_i2s_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	const unsigned int clk_mode =
		afe_priv->i2s_clk_modes[MT8365_AFE_2ND_I2S];
	struct mt8365_be_dai_data *be =
		&afe_priv->be_data[dai->id - MT8365_AFE_BACKEND_BASE];
	const bool i2s_in_slave = (substream->stream
		== SNDRV_PCM_STREAM_CAPTURE)
		&& ((be->fmt_mode & SND_SOC_DAIFMT_MASTER_MASK)
			== SND_SOC_DAIFMT_CBM_CFM);

	dev_dbg(afe->dev, "%s '%s'\n",
		__func__, snd_pcm_stream_str(substream));

	if (clk_mode == MT8365_AFE_I2S_SHARED_CLOCK && snd_soc_dai_active(dai))
		return 0;

	mt8365_afe_enable_main_clk(afe);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK ||
	    clk_mode == MT8365_AFE_I2S_SHARED_CLOCK)
		mt8365_afe_enable_clk(afe,
			afe_priv->clocks[MT8365_CLK_AUD_I2S3_M]);

	if ((substream->stream == SNDRV_PCM_STREAM_CAPTURE ||
	    clk_mode == MT8365_AFE_I2S_SHARED_CLOCK) && !i2s_in_slave)
		mt8365_afe_enable_clk(afe,
			afe_priv->clocks[MT8365_CLK_AUD_I2S0_M]);

	if (i2s_in_slave)
		mt8365_afe_enable_top_cg(afe, MT8365_TOP_CG_I2S_IN);

	return 0;
}

static void mt8365_afe_2nd_i2s_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	struct mt8365_be_dai_data *be =
		&afe_priv->be_data[dai->id - MT8365_AFE_BACKEND_BASE];
	const unsigned int rate = substream->runtime->rate;
	const unsigned int stream = substream->stream;
	const unsigned int clk_mode =
		afe_priv->i2s_clk_modes[MT8365_AFE_2ND_I2S];
	const bool reset_i2s_out_change =
		(stream == SNDRV_PCM_STREAM_PLAYBACK) ||
		(clk_mode == MT8365_AFE_I2S_SHARED_CLOCK);
	const bool reset_i2s_in_change =
		(stream == SNDRV_PCM_STREAM_CAPTURE) ||
		(clk_mode == MT8365_AFE_I2S_SHARED_CLOCK);
	const bool i2s_in_slave = (substream->stream
		== SNDRV_PCM_STREAM_CAPTURE)
		&& ((be->fmt_mode & SND_SOC_DAIFMT_MASTER_MASK)
			== SND_SOC_DAIFMT_CBM_CFM);

	dev_dbg(afe->dev, "%s '%s'\n",
		__func__, snd_pcm_stream_str(substream));

	if (clk_mode == MT8365_AFE_I2S_SHARED_CLOCK && snd_soc_dai_active(dai))
		return;

	if (be->prepared[stream]) {
		if (reset_i2s_out_change)
			mt8365_afe_set_2nd_i2s_out_enable(afe, false);

		if (reset_i2s_in_change) {
			if (i2s_in_slave)
				mt8365_afe_set_2nd_i2s_asrc_enable(afe, false);
			mt8365_afe_set_2nd_i2s_in_enable(afe, false);
		}
		if (rate % 8000)
			mt8365_afe_disable_apll_associated_cfg(afe,
			MT8365_AFE_APLL1);
		else
			mt8365_afe_disable_apll_associated_cfg(afe,
			MT8365_AFE_APLL2);

		if (reset_i2s_out_change)
			be->prepared[SNDRV_PCM_STREAM_PLAYBACK] = false;

		if (reset_i2s_in_change)
			be->prepared[SNDRV_PCM_STREAM_CAPTURE] = false;
	}

	if (reset_i2s_out_change)
		mt8365_afe_disable_clk(afe,
		afe_priv->clocks[MT8365_CLK_AUD_I2S3_M]);

	if (reset_i2s_in_change  && !i2s_in_slave)
		mt8365_afe_disable_clk(afe,
		afe_priv->clocks[MT8365_CLK_AUD_I2S0_M]);

	if (i2s_in_slave)
		mt8365_afe_disable_top_cg(afe, MT8365_TOP_CG_I2S_IN);

	mt8365_afe_disable_main_clk(afe);
}

static int mt8365_afe_2nd_i2s_prepare(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	struct mt8365_be_dai_data *be =
		&afe_priv->be_data[dai->id - MT8365_AFE_BACKEND_BASE];
	const unsigned int rate = substream->runtime->rate;
	const int bit_width = snd_pcm_format_width(substream->runtime->format);
	const unsigned int stream = substream->stream;
	const unsigned int clk_mode =
		afe_priv->i2s_clk_modes[MT8365_AFE_2ND_I2S];
	const bool apply_i2s_out_change =
		(stream == SNDRV_PCM_STREAM_PLAYBACK) ||
		(clk_mode == MT8365_AFE_I2S_SHARED_CLOCK);
	const bool apply_i2s_in_change =
		(stream == SNDRV_PCM_STREAM_CAPTURE) ||
		(clk_mode == MT8365_AFE_I2S_SHARED_CLOCK);
	int ret;

	if ((clk_mode == MT8365_AFE_I2S_SHARED_CLOCK) &&
		(dai->playback_widget->power || dai->capture_widget->power)) {
		dev_dbg(afe->dev, "%s '%s' widget powered(%u-%u) already\n",
			__func__, snd_pcm_stream_str(substream),
			dai->playback_widget->power,
			dai->capture_widget->power);
		return 0;
	}

	if (be->prepared[stream]) {
		dev_info(afe->dev, "%s '%s' prepared already\n",
			 __func__, snd_pcm_stream_str(substream));
		return 0;
	}

	if (apply_i2s_out_change) {

		ret = mt8365_afe_set_2nd_i2s_out(afe, rate, bit_width);
		if (ret)
			return ret;
	}

	if (apply_i2s_in_change) {
		if ((be->fmt_mode & SND_SOC_DAIFMT_MASTER_MASK)
			== SND_SOC_DAIFMT_CBM_CFM) {
			ret = mt8365_afe_set_2nd_i2s_asrc(afe, 32000, rate,
					(unsigned int)bit_width, 0, 0, 1);
			if (ret < 0)
				return ret;
		}
		ret = mt8365_afe_set_2nd_i2s_in(afe, rate, bit_width);
		if (ret)
			return ret;
	}

	if (rate % 8000)
		mt8365_afe_enable_apll_associated_cfg(afe, MT8365_AFE_APLL1);
	else
		mt8365_afe_enable_apll_associated_cfg(afe, MT8365_AFE_APLL2);

	if (apply_i2s_out_change) {
		mt8365_afe_set_clk_parent(afe,
			afe_priv->clocks[MT8365_CLK_I2S3_M_SEL],
			((rate % 8000) ?
			afe_priv->clocks[MT8365_CLK_AUD1] :
			afe_priv->clocks[MT8365_CLK_AUD2]));

		mt8365_afe_set_clk_rate(afe,
			afe_priv->clocks[MT8365_CLK_AUD_I2S3_M],
			rate * MT8365_I2S3_MCLK_MULTIPLIER);

		mt8365_afe_set_2nd_i2s_out_enable(afe, true);
		be->prepared[SNDRV_PCM_STREAM_PLAYBACK] = true;
	}

	if (apply_i2s_in_change) {
		mt8365_afe_set_clk_parent(afe,
			afe_priv->clocks[MT8365_CLK_I2S0_M_SEL],
			((rate % 8000) ?
			afe_priv->clocks[MT8365_CLK_AUD1] :
			afe_priv->clocks[MT8365_CLK_AUD2]));

		mt8365_afe_set_clk_rate(afe,
			afe_priv->clocks[MT8365_CLK_AUD_I2S0_M],
			rate * MT8365_I2S0_MCLK_MULTIPLIER);

		mt8365_afe_set_2nd_i2s_in_enable(afe, true);

		if ((be->fmt_mode & SND_SOC_DAIFMT_MASTER_MASK)
			== SND_SOC_DAIFMT_CBM_CFM)
			mt8365_afe_set_2nd_i2s_asrc_enable(afe, true);

		be->prepared[SNDRV_PCM_STREAM_CAPTURE] = true;
	}

	return 0;
}

static int mt8365_afe_2nd_i2s_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	struct mt8365_be_dai_data *be =
		&afe_priv->be_data[dai->id - MT8365_AFE_BACKEND_BASE];
	const unsigned int clk_mode =
		afe_priv->i2s_clk_modes[MT8365_AFE_2ND_I2S];

	be->fmt_mode = 0;

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		be->fmt_mode |= SND_SOC_DAIFMT_I2S;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		be->fmt_mode |= SND_SOC_DAIFMT_LEFT_J;
		break;
	default:
		dev_info(afe->dev, "invalid audio format for 2nd i2s!\n");
		return -EINVAL;
	}

	if (((fmt & SND_SOC_DAIFMT_INV_MASK) != SND_SOC_DAIFMT_NB_NF) &&
		((fmt & SND_SOC_DAIFMT_INV_MASK) != SND_SOC_DAIFMT_NB_IF) &&
		((fmt & SND_SOC_DAIFMT_INV_MASK) != SND_SOC_DAIFMT_IB_NF) &&
		((fmt & SND_SOC_DAIFMT_INV_MASK) != SND_SOC_DAIFMT_IB_IF)) {
		dev_info(afe->dev, "invalid audio format for 2nd i2s!\n");
		return -EINVAL;
	}

	be->fmt_mode |= (fmt & SND_SOC_DAIFMT_INV_MASK);

	if (((fmt & SND_SOC_DAIFMT_MASTER_MASK) == SND_SOC_DAIFMT_CBM_CFM) &&
		(clk_mode == MT8365_AFE_I2S_SEPARATE_CLOCK))
		be->fmt_mode |= (fmt & SND_SOC_DAIFMT_MASTER_MASK);

	return 0;
}

static void mt8365_afe_enable_pcm1(struct mtk_base_afe *afe)
{
	regmap_update_bits(afe->regmap, PCM_INTF_CON1,
			   PCM_INTF_CON1_EN, PCM_INTF_CON1_EN);
}

static void mt8365_afe_disable_pcm1(struct mtk_base_afe *afe)
{
	regmap_update_bits(afe->regmap, PCM_INTF_CON1,
			   PCM_INTF_CON1_EN, 0x0);
}

static int mt8365_afe_configure_pcm1(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_pcm_runtime * const runtime = substream->runtime;
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	bool slave_mode = afe_priv->pcm_intf_data.slave_mode;
	bool lrck_inv = afe_priv->pcm_intf_data.lrck_inv;
	bool bck_inv = afe_priv->pcm_intf_data.bck_inv;
	unsigned int fmt = afe_priv->pcm_intf_data.format;
	unsigned int bit_width = dai->sample_bits;
	unsigned int val = 0;

	if (!slave_mode) {
		val |= PCM_INTF_CON1_MASTER_MODE |
		       PCM_INTF_CON1_BYPASS_ASRC;

		if (lrck_inv)
			val |= PCM_INTF_CON1_SYNC_OUT_INV;
		if (bck_inv)
			val |= PCM_INTF_CON1_BCLK_OUT_INV;
	} else {
		val |= PCM_INTF_CON1_SLAVE_MODE;

		if (lrck_inv)
			val |= PCM_INTF_CON1_SYNC_IN_INV;
		if (bck_inv)
			val |= PCM_INTF_CON1_BCLK_IN_INV;

		// TODO: add asrc setting
	}

	val |= PCM_INTF_CON1_FORMAT(fmt);

	if (fmt == MT8365_PCM_FORMAT_PCMA ||
	    fmt == MT8365_PCM_FORMAT_PCMB)
		val |= PCM_INTF_CON1_SYNC_LEN(1);
	else
		val |= PCM_INTF_CON1_SYNC_LEN(bit_width);

	switch (runtime->rate) {
	case 48000:
		val |= PCM_INTF_CON1_FS_48K;
		break;
	case 32000:
		val |= PCM_INTF_CON1_FS_32K;
		break;
	case 16000:
		val |= PCM_INTF_CON1_FS_16K;
		break;
	case 8000:
		val |= PCM_INTF_CON1_FS_8K;
		break;
	default:
		return -EINVAL;
	}

	if (bit_width > 16)
		val |= PCM_INTF_CON1_24BIT | PCM_INTF_CON1_64BCK;
	else
		val |= PCM_INTF_CON1_16BIT | PCM_INTF_CON1_32BCK;

	val |= PCM_INTF_CON1_EXT_MODEM;

	regmap_update_bits(afe->regmap, PCM_INTF_CON1,
			   PCM_INTF_CON1_CONFIG_MASK, val);

	return 0;
}

static int mt8365_afe_pcm1_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);

	if (snd_soc_dai_active(dai))
		return 0;

	mt8365_afe_enable_main_clk(afe);

	return 0;
}

static void mt8365_afe_pcm1_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);

	if (snd_soc_dai_active(dai))
		return;

	mt8365_afe_disable_pcm1(afe);
	mt8365_afe_disable_main_clk(afe);
}

static int mt8365_afe_pcm1_prepare(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	int ret;

	if ((dai->stream_active[SNDRV_PCM_STREAM_PLAYBACK] + dai->stream_active[SNDRV_PCM_STREAM_CAPTURE]) > 1) {
		dev_info(afe->dev, "%s '%s' active(%u-%u) already\n",
			 __func__, snd_pcm_stream_str(substream),
			 dai->stream_active[SNDRV_PCM_STREAM_PLAYBACK],
			 dai->stream_active[SNDRV_PCM_STREAM_CAPTURE]);
		return 0;
	}

	ret = mt8365_afe_configure_pcm1(substream, dai);
	if (ret)
		return ret;

	mt8365_afe_enable_pcm1(afe);

	return 0;
}

static int mt8365_afe_pcm1_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8365_afe_private *afe_priv = afe->platform_priv;

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		afe_priv->pcm_intf_data.format = MT8365_PCM_FORMAT_I2S;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		afe_priv->pcm_intf_data.bck_inv = false;
		afe_priv->pcm_intf_data.lrck_inv = false;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		afe_priv->pcm_intf_data.bck_inv = false;
		afe_priv->pcm_intf_data.lrck_inv = true;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		afe_priv->pcm_intf_data.bck_inv = true;
		afe_priv->pcm_intf_data.lrck_inv = false;
		break;
	case SND_SOC_DAIFMT_IB_IF:
		afe_priv->pcm_intf_data.bck_inv = true;
		afe_priv->pcm_intf_data.lrck_inv = true;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		afe_priv->pcm_intf_data.slave_mode = true;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		afe_priv->pcm_intf_data.slave_mode = false;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static void audio_dmic_adda_enable(struct mtk_base_afe *afe)
{
	mt8365_afe_enable_adda_on(afe);
	regmap_update_bits(afe->regmap, AFE_ADDA_UL_DL_CON0,
		AFE_ADDA_UL_DL_DMIC_CLKDIV_ON_MASK,
		AFE_ADDA_UL_DL_DMIC_CLKDIV_ON);
}

static void audio_dmic_adda_disable(struct mtk_base_afe *afe)
{
	regmap_update_bits(afe->regmap, AFE_ADDA_UL_DL_CON0,
		AFE_ADDA_UL_DL_DMIC_CLKDIV_ON_MASK,
		AFE_ADDA_UL_DL_DMIC_CLKDIV_OFF);
	mt8365_afe_disable_adda_on(afe);
}

static void mt8365_afe_enable_dmic(struct mtk_base_afe *afe,
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	struct mt8365_dmic_data *dmic_data = &afe_priv->dmic_data;

	switch (dmic_data->dmic_channel) {
	case 8:
		fallthrough;
	case 7:
		regmap_update_bits(afe->regmap, AFE_DMIC3_UL_SRC_CON0,
			DMIC_TOP_CON_DMIC_CH1_ON_MASK,
			DMIC_TOP_CON_DMIC_CH1_ON);
		regmap_update_bits(afe->regmap, AFE_DMIC3_UL_SRC_CON0,
			DMIC_TOP_CON_DMIC_CH2_ON_MASK,
			DMIC_TOP_CON_DMIC_CH2_ON);
		regmap_update_bits(afe->regmap, AFE_DMIC3_UL_SRC_CON0,
			DMIC_TOP_CON_DMIC_SRC_ON_MASK,
			DMIC_TOP_CON_DMIC_SRC_ON);
		fallthrough;
	case 6:
		fallthrough;
	case 5:
		regmap_update_bits(afe->regmap, AFE_DMIC2_UL_SRC_CON0,
			DMIC_TOP_CON_DMIC_CH1_ON_MASK,
			DMIC_TOP_CON_DMIC_CH1_ON);
		regmap_update_bits(afe->regmap, AFE_DMIC2_UL_SRC_CON0,
			DMIC_TOP_CON_DMIC_CH2_ON_MASK,
			DMIC_TOP_CON_DMIC_CH2_ON);
		regmap_update_bits(afe->regmap, AFE_DMIC2_UL_SRC_CON0,
			DMIC_TOP_CON_DMIC_SRC_ON_MASK,
			DMIC_TOP_CON_DMIC_SRC_ON);
		fallthrough;
	case 4:
		fallthrough;
	case 3:
		regmap_update_bits(afe->regmap, AFE_DMIC1_UL_SRC_CON0,
			DMIC_TOP_CON_DMIC_CH1_ON_MASK,
			DMIC_TOP_CON_DMIC_CH1_ON);
		regmap_update_bits(afe->regmap, AFE_DMIC1_UL_SRC_CON0,
			DMIC_TOP_CON_DMIC_CH2_ON_MASK,
			DMIC_TOP_CON_DMIC_CH2_ON);
		regmap_update_bits(afe->regmap, AFE_DMIC1_UL_SRC_CON0,
			DMIC_TOP_CON_DMIC_SRC_ON_MASK,
			DMIC_TOP_CON_DMIC_SRC_ON);
		fallthrough;
	case 2:
		fallthrough;
	case 1:
		regmap_update_bits(afe->regmap, AFE_DMIC0_UL_SRC_CON0,
			DMIC_TOP_CON_DMIC_CH1_ON_MASK,
			DMIC_TOP_CON_DMIC_CH1_ON);
		regmap_update_bits(afe->regmap, AFE_DMIC0_UL_SRC_CON0,
			DMIC_TOP_CON_DMIC_CH2_ON_MASK,
			DMIC_TOP_CON_DMIC_CH2_ON);
		regmap_update_bits(afe->regmap, AFE_DMIC0_UL_SRC_CON0,
			DMIC_TOP_CON_DMIC_SRC_ON_MASK,
			DMIC_TOP_CON_DMIC_SRC_ON);
		break;
	default:
		break;
	}
}

static void mt8365_afe_disable_dmic(struct mtk_base_afe *afe,
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	struct mt8365_dmic_data *dmic_data = &afe_priv->dmic_data;

	dev_info(afe->dev, "%s dmic_channel %d\n",
		__func__, dmic_data->dmic_channel);

	switch (dmic_data->dmic_channel) {
	case 8:
		fallthrough;
	case 7:
		regmap_update_bits(afe->regmap, AFE_DMIC3_UL_SRC_CON0,
			DMIC_TOP_CON_DMIC_CH1_ON_MASK,
			DMIC_TOP_CON_DMIC_CH1_OFF);
		regmap_update_bits(afe->regmap, AFE_DMIC3_UL_SRC_CON0,
			DMIC_TOP_CON_DMIC_CH2_ON_MASK,
			DMIC_TOP_CON_DMIC_CH2_OFF);
		regmap_update_bits(afe->regmap, AFE_DMIC3_UL_SRC_CON0,
			DMIC_TOP_CON_DMIC_SRC_ON_MASK,
			DMIC_TOP_CON_DMIC_SRC_OFF);
		regmap_update_bits(afe->regmap, AFE_DMIC3_UL_SRC_CON0,
			DMIC_TOP_CON_DMIC_SDM3_LEVEL_MODE,
			DMIC_TOP_CON_DMIC_SDM3_DE_SELECT);
		fallthrough;
	case 6:
		fallthrough;
	case 5:
		regmap_update_bits(afe->regmap, AFE_DMIC2_UL_SRC_CON0,
			DMIC_TOP_CON_DMIC_CH1_ON_MASK,
			DMIC_TOP_CON_DMIC_CH1_OFF);
		regmap_update_bits(afe->regmap, AFE_DMIC2_UL_SRC_CON0,
			DMIC_TOP_CON_DMIC_CH2_ON_MASK,
			DMIC_TOP_CON_DMIC_CH2_OFF);
		regmap_update_bits(afe->regmap, AFE_DMIC2_UL_SRC_CON0,
			DMIC_TOP_CON_DMIC_SRC_ON_MASK,
			DMIC_TOP_CON_DMIC_SRC_OFF);
		regmap_update_bits(afe->regmap, AFE_DMIC2_UL_SRC_CON0,
			DMIC_TOP_CON_DMIC_SDM3_LEVEL_MODE,
			DMIC_TOP_CON_DMIC_SDM3_DE_SELECT);
		fallthrough;
	case 4:
	case 3:
		regmap_update_bits(afe->regmap, AFE_DMIC1_UL_SRC_CON0,
			DMIC_TOP_CON_DMIC_CH1_ON_MASK,
			DMIC_TOP_CON_DMIC_CH1_OFF);
		regmap_update_bits(afe->regmap, AFE_DMIC1_UL_SRC_CON0,
			DMIC_TOP_CON_DMIC_CH2_ON_MASK,
			DMIC_TOP_CON_DMIC_CH2_OFF);
		regmap_update_bits(afe->regmap, AFE_DMIC1_UL_SRC_CON0,
			DMIC_TOP_CON_DMIC_SRC_ON_MASK,
			DMIC_TOP_CON_DMIC_SRC_OFF);
		regmap_update_bits(afe->regmap, AFE_DMIC1_UL_SRC_CON0,
			DMIC_TOP_CON_DMIC_SDM3_LEVEL_MODE,
			DMIC_TOP_CON_DMIC_SDM3_DE_SELECT);
		fallthrough;
	case 2:
		fallthrough;
	case 1:
		regmap_update_bits(afe->regmap, AFE_DMIC0_UL_SRC_CON0,
			DMIC_TOP_CON_DMIC_CH1_ON_MASK,
			DMIC_TOP_CON_DMIC_CH1_OFF);
		regmap_update_bits(afe->regmap, AFE_DMIC0_UL_SRC_CON0,
			DMIC_TOP_CON_DMIC_CH2_ON_MASK,
			DMIC_TOP_CON_DMIC_CH2_OFF);
		regmap_update_bits(afe->regmap, AFE_DMIC0_UL_SRC_CON0,
			DMIC_TOP_CON_DMIC_SRC_ON_MASK,
			DMIC_TOP_CON_DMIC_SRC_OFF);
		regmap_update_bits(afe->regmap, AFE_DMIC0_UL_SRC_CON0,
			DMIC_TOP_CON_DMIC_SDM3_LEVEL_MODE,
			DMIC_TOP_CON_DMIC_SDM3_DE_SELECT);
		break;
	default:
		break;
	}
}

static const struct reg_sequence mt8365_afe_dmic_iir_coeff_reg_defaults[] = {
	{ AFE_DMIC0_IIR_COEF_02_01, 0x00000000 },
	{ AFE_DMIC0_IIR_COEF_04_03, 0x00003FB8 },
	{ AFE_DMIC0_IIR_COEF_06_05, 0x3FB80000 },
	{ AFE_DMIC0_IIR_COEF_08_07, 0x3FB80000 },
	{ AFE_DMIC0_IIR_COEF_10_09, 0x0000C048 },
	{ AFE_DMIC1_IIR_COEF_02_01, 0x00000000 },
	{ AFE_DMIC1_IIR_COEF_04_03, 0x00003FB8 },
	{ AFE_DMIC1_IIR_COEF_06_05, 0x3FB80000 },
	{ AFE_DMIC1_IIR_COEF_08_07, 0x3FB80000 },
	{ AFE_DMIC1_IIR_COEF_10_09, 0x0000C048 },
	{ AFE_DMIC2_IIR_COEF_02_01, 0x00000000 },
	{ AFE_DMIC2_IIR_COEF_04_03, 0x00003FB8 },
	{ AFE_DMIC2_IIR_COEF_06_05, 0x3FB80000 },
	{ AFE_DMIC2_IIR_COEF_08_07, 0x3FB80000 },
	{ AFE_DMIC2_IIR_COEF_10_09, 0x0000C048 },
	{ AFE_DMIC3_IIR_COEF_02_01, 0x00000000 },
	{ AFE_DMIC3_IIR_COEF_04_03, 0x00003FB8 },
	{ AFE_DMIC3_IIR_COEF_06_05, 0x3FB80000 },
	{ AFE_DMIC3_IIR_COEF_08_07, 0x3FB80000 },
	{ AFE_DMIC3_IIR_COEF_10_09, 0x0000C048 },
};

static int mt8365_afe_load_dmic_iir_coeff_table(struct mtk_base_afe *afe)
{
	return regmap_multi_reg_write(afe->regmap,
			mt8365_afe_dmic_iir_coeff_reg_defaults,
			ARRAY_SIZE(mt8365_afe_dmic_iir_coeff_reg_defaults));
}

static int mt8365_afe_configure_dmic(struct mtk_base_afe *afe,
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	struct mt8365_dmic_data *dmic_data = &afe_priv->dmic_data;
	bool two_wire_mode = dmic_data->two_wire_mode;
	unsigned int clk_phase_sel_ch1 = dmic_data->clk_phase_sel_ch1;
	unsigned int clk_phase_sel_ch2 = dmic_data->clk_phase_sel_ch2;
	bool iir_on = dmic_data->iir_on;
	unsigned int irr_mode = dmic_data->irr_mode;
	unsigned int dmic_mode = dmic_data->dmic_mode;
	unsigned int val = 0;
	unsigned int channels = dai->channels;
	unsigned int rate = dai->rate;

	dmic_data->dmic_channel = channels;

	dev_info(afe->dev, "%s dmic_channel %d dmic_rate %d dmic_mode %d\n",
		__func__, dmic_data->dmic_channel, rate, dmic_mode);

	val |= DMIC_TOP_CON_DMIC_SDM3_LEVEL_MODE;

	if (dmic_mode > DMIC_MODE_1P625M)
		val |= DMIC_TOP_CON_DMIC_LOW_POWER_MODE(dmic_mode);
	else {
		val |= DMIC_TOP_CON_DMIC_NO_LOW_POWER_MODE
			| DMIC_TOP_CON_DMIC_INPUT_MODE(dmic_mode);
	}

	if (two_wire_mode) {
		val |= DMIC_TOP_CON_DMIC_TWO_WIRE_MODE;
	} else {
		val |= DMIC_TOP_CON_DMIC_CK_PHASE_SEL_CH1(clk_phase_sel_ch1);
		val |= DMIC_TOP_CON_DMIC_CK_PHASE_SEL_CH2(clk_phase_sel_ch2);
	}

	switch (rate) {
	case 48000:
		val |= DMIC_TOP_CON_DMIC_VOICE_MODE_48K;
		break;
	case 32000:
		val |= DMIC_TOP_CON_DMIC_VOICE_MODE_32K;
		break;
	case 16000:
		val |= DMIC_TOP_CON_DMIC_VOICE_MODE_16K;
		break;
	case 8000:
		val |= DMIC_TOP_CON_DMIC_VOICE_MODE_8K;
		break;
	default:
		return -EINVAL;
	}

	if (iir_on) {
		if (irr_mode == IIR_MODE0)
			mt8365_afe_load_dmic_iir_coeff_table(afe); /* SW mode */
		val |= DMIC_TOP_CON_DMIC_IIR_MODE(irr_mode);
		val |= DMIC_TOP_CON_DMIC_IIR_ON;
	}

	switch (dmic_data->dmic_channel) {
	case 8:
		fallthrough;
	case 7:
		regmap_update_bits(afe->regmap, AFE_DMIC3_UL_SRC_CON0,
			DMIC_TOP_CON_CONFIG_MASK, val);
		fallthrough;
	case 6:
		fallthrough;
	case 5:
		regmap_update_bits(afe->regmap, AFE_DMIC2_UL_SRC_CON0,
			DMIC_TOP_CON_CONFIG_MASK, val);
		fallthrough;
	case 4:
		fallthrough;
	case 3:
		regmap_update_bits(afe->regmap, AFE_DMIC1_UL_SRC_CON0,
			DMIC_TOP_CON_CONFIG_MASK, val);
		fallthrough;
	case 2:
		fallthrough;
	case 1:
		regmap_update_bits(afe->regmap, AFE_DMIC0_UL_SRC_CON0,
			DMIC_TOP_CON_CONFIG_MASK, val);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int mt8365_afe_dmic_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);

	mt8365_afe_enable_main_clk(afe);

	mt8365_afe_enable_top_cg(afe, MT8365_TOP_CG_DMIC0_ADC);
	mt8365_afe_enable_top_cg(afe, MT8365_TOP_CG_DMIC1_ADC);
	mt8365_afe_enable_top_cg(afe, MT8365_TOP_CG_DMIC2_ADC);
	mt8365_afe_enable_top_cg(afe, MT8365_TOP_CG_DMIC3_ADC);

	audio_dmic_adda_enable(afe);

	return 0;
}

static void mt8365_afe_dmic_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);

	mt8365_afe_disable_dmic(afe, substream, dai);
	audio_dmic_adda_disable(afe);
	/* HW Request delay 125ms before CG off */
	udelay(125);
	mt8365_afe_disable_top_cg(afe, MT8365_TOP_CG_DMIC3_ADC);
	mt8365_afe_disable_top_cg(afe, MT8365_TOP_CG_DMIC2_ADC);
	mt8365_afe_disable_top_cg(afe, MT8365_TOP_CG_DMIC1_ADC);
	mt8365_afe_disable_top_cg(afe, MT8365_TOP_CG_DMIC0_ADC);

	mt8365_afe_disable_main_clk(afe);
}

static int mt8365_afe_dmic_prepare(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);

	mt8365_afe_configure_dmic(afe, substream, dai);
	mt8365_afe_enable_dmic(afe, substream, dai);

	return 0;
}

static int mt8365_afe_int_adda_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	unsigned int stream = substream->stream;

	mt8365_afe_enable_main_clk(afe);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		mt8365_afe_enable_top_cg(afe, MT8365_TOP_CG_DAC);
		mt8365_afe_enable_top_cg(afe, MT8365_TOP_CG_DAC_PREDIS);
	} else if (stream == SNDRV_PCM_STREAM_CAPTURE) {
		mt8365_afe_enable_top_cg(afe, MT8365_TOP_CG_ADC);
	}

	return 0;
}

static void mt8365_afe_int_adda_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	struct mt8365_be_dai_data *be =
		&afe_priv->be_data[dai->id - MT8365_AFE_BACKEND_BASE];
	unsigned int stream = substream->stream;

	if (be->prepared[stream]) {
		if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
			mt8365_afe_set_adda_out_enable(afe, false);
			mt8365_afe_set_i2s_out_enable(afe, false);
		} else
			mt8365_afe_set_adda_in_enable(afe, false);

		be->prepared[stream] = false;
	}

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		mt8365_afe_disable_top_cg(afe, MT8365_TOP_CG_DAC_PREDIS);
		mt8365_afe_disable_top_cg(afe, MT8365_TOP_CG_DAC);
	} else if (stream == SNDRV_PCM_STREAM_CAPTURE) {
		mt8365_afe_disable_top_cg(afe, MT8365_TOP_CG_ADC);
	}

	mt8365_afe_disable_main_clk(afe);
}

static int mt8365_afe_int_adda_prepare(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	struct mt8365_be_dai_data *be =
		&afe_priv->be_data[dai->id - MT8365_AFE_BACKEND_BASE];
	const unsigned int rate = substream->runtime->rate;
	const unsigned int stream = substream->stream;
	const int bit_width = snd_pcm_format_width(substream->runtime->format);
	int ret;

	dev_info(afe->dev, "%s '%s' rate = %u\n", __func__,
		snd_pcm_stream_str(substream), rate);

	if (be->prepared[stream]) {
		dev_info(afe->dev, "%s '%s' prepared already\n",
			 __func__, snd_pcm_stream_str(substream));
		return 0;
	}

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		ret = mt8365_afe_set_adda_out(afe, rate);
		if (ret)
			return ret;

		ret = mt8365_afe_set_i2s_out(afe, rate, bit_width);
		if (ret)
			return ret;

		mt8365_afe_set_adda_out_enable(afe, true);
		mt8365_afe_set_i2s_out_enable(afe, true);
	} else {
		ret = mt8365_afe_set_adda_in(afe, rate);
		if (ret)
			return ret;

		mt8365_afe_set_adda_in_enable(afe, true);
	}

	be->prepared[stream] = true;

	return 0;
}

static const struct mt8365_gasrc_ctrl_reg
	gasrc_ctrl_reg[MT8365_TDM_ASRC_NUM] = {
	[MT8365_GASRC1] = {
		.con0 = AFE_GENERAL1_ASRC_2CH_CON0,
		.con2 = AFE_GENERAL1_ASRC_2CH_CON2,
		.con3 = AFE_GENERAL1_ASRC_2CH_CON3,
		.con4 = AFE_GENERAL1_ASRC_2CH_CON4,
		.con5 = AFE_GENERAL1_ASRC_2CH_CON5,
		.con6 = AFE_GENERAL1_ASRC_2CH_CON6,
		.con9 = AFE_GENERAL1_ASRC_2CH_CON9,
		.con10 = AFE_GENERAL1_ASRC_2CH_CON10,
		.con12 = AFE_GENERAL1_ASRC_2CH_CON12,
		.con13 = AFE_GENERAL1_ASRC_2CH_CON13,
	},
	[MT8365_GASRC2] = {
		.con0 = AFE_GENERAL2_ASRC_2CH_CON0,
		.con2 = AFE_GENERAL2_ASRC_2CH_CON2,
		.con3 = AFE_GENERAL2_ASRC_2CH_CON3,
		.con4 = AFE_GENERAL2_ASRC_2CH_CON4,
		.con5 = AFE_GENERAL2_ASRC_2CH_CON5,
		.con6 = AFE_GENERAL2_ASRC_2CH_CON6,
		.con9 = AFE_GENERAL2_ASRC_2CH_CON9,
		.con10 = AFE_GENERAL2_ASRC_2CH_CON10,
		.con12 = AFE_GENERAL2_ASRC_2CH_CON12,
		.con13 = AFE_GENERAL2_ASRC_2CH_CON13,
	},
	[MT8365_TDM_ASRC1] = {
		.con0 = AFE_TDM_GASRC1_ASRC_2CH_CON0,
		.con2 = AFE_TDM_GASRC1_ASRC_2CH_CON2,
		.con3 = AFE_TDM_GASRC1_ASRC_2CH_CON3,
		.con4 = AFE_TDM_GASRC1_ASRC_2CH_CON4,
		.con5 = AFE_TDM_GASRC1_ASRC_2CH_CON5,
		.con6 = AFE_TDM_GASRC1_ASRC_2CH_CON6,
		.con9 = AFE_TDM_GASRC1_ASRC_2CH_CON9,
		.con10 = AFE_TDM_GASRC1_ASRC_2CH_CON10,
		.con12 = AFE_TDM_GASRC1_ASRC_2CH_CON12,
		.con13 = AFE_TDM_GASRC1_ASRC_2CH_CON13,
	},
	[MT8365_TDM_ASRC2] = {
		.con0 = AFE_TDM_GASRC2_ASRC_2CH_CON0,
		.con2 = AFE_TDM_GASRC2_ASRC_2CH_CON2,
		.con3 = AFE_TDM_GASRC2_ASRC_2CH_CON3,
		.con4 = AFE_TDM_GASRC2_ASRC_2CH_CON4,
		.con5 = AFE_TDM_GASRC2_ASRC_2CH_CON5,
		.con6 = AFE_TDM_GASRC2_ASRC_2CH_CON6,
		.con9 = AFE_TDM_GASRC2_ASRC_2CH_CON9,
		.con10 = AFE_TDM_GASRC2_ASRC_2CH_CON10,
		.con12 = AFE_TDM_GASRC2_ASRC_2CH_CON12,
		.con13 = AFE_TDM_GASRC2_ASRC_2CH_CON13,
	},
	[MT8365_TDM_ASRC3] = {
		.con0 = AFE_TDM_GASRC3_ASRC_2CH_CON0,
		.con2 = AFE_TDM_GASRC3_ASRC_2CH_CON2,
		.con3 = AFE_TDM_GASRC3_ASRC_2CH_CON3,
		.con4 = AFE_TDM_GASRC3_ASRC_2CH_CON4,
		.con5 = AFE_TDM_GASRC3_ASRC_2CH_CON5,
		.con6 = AFE_TDM_GASRC3_ASRC_2CH_CON6,
		.con9 = AFE_TDM_GASRC3_ASRC_2CH_CON9,
		.con10 = AFE_TDM_GASRC3_ASRC_2CH_CON10,
		.con12 = AFE_TDM_GASRC3_ASRC_2CH_CON12,
		.con13 = AFE_TDM_GASRC3_ASRC_2CH_CON13,
	},
	[MT8365_TDM_ASRC4] = {
		.con0 = AFE_TDM_GASRC4_ASRC_2CH_CON0,
		.con2 = AFE_TDM_GASRC4_ASRC_2CH_CON2,
		.con3 = AFE_TDM_GASRC4_ASRC_2CH_CON3,
		.con4 = AFE_TDM_GASRC4_ASRC_2CH_CON4,
		.con5 = AFE_TDM_GASRC4_ASRC_2CH_CON5,
		.con6 = AFE_TDM_GASRC4_ASRC_2CH_CON6,
		.con9 = AFE_TDM_GASRC4_ASRC_2CH_CON9,
		.con10 = AFE_TDM_GASRC4_ASRC_2CH_CON10,
		.con12 = AFE_TDM_GASRC4_ASRC_2CH_CON12,
		.con13 = AFE_TDM_GASRC4_ASRC_2CH_CON13,
	},
};

static int mt8365_dai_num_to_gasrc(int num)
{
	int val = num - MT8365_AFE_IO_GASRC1;

	if (val < 0 || val > MT8365_GASRC_NUM)
		return -EINVAL;

	return val;
}

static void mt8365_afe_gasrc_set_input_fs(struct mtk_base_afe *afe,
	struct snd_soc_dai *dai, int fs_timing)
{
	const int gasrc_id = mt8365_dai_num_to_gasrc(dai->id);
	unsigned int val = 0;
	unsigned int mask = 0;

	switch (gasrc_id) {
	case MT8365_GASRC1:
		mask = GASRC_MODE_GASRC1_IN_MODE_MASK;
		val = GASRC_MODE_GASRC1_IN_MODE(fs_timing);
		break;
	case MT8365_GASRC2:
		mask = GASRC_MODE_GASRC2_IN_MODE_MASK;
		val = GASRC_MODE_GASRC2_IN_MODE(fs_timing);
		break;
	default:
		return;
	}

	regmap_update_bits(afe->regmap, GENERAL_ASRC_MODE, mask, val);
}

static void mt8365_afe_gasrc_set_output_fs(struct mtk_base_afe *afe,
	struct snd_soc_dai *dai, int fs_timing)
{
	const int gasrc_id = mt8365_dai_num_to_gasrc(dai->id);
	unsigned int val = 0;
	unsigned int mask = 0;

	switch (gasrc_id) {
	case MT8365_GASRC1:
		mask = GASRC_MODE_GASRC1_OUT_MODE_MASK;
		val = GASRC_MODE_GASRC1_OUT_MODE(fs_timing);
		break;
	case MT8365_GASRC2:
		mask = GASRC_MODE_GASRC2_OUT_MODE_MASK;
		val = GASRC_MODE_GASRC2_OUT_MODE(fs_timing);
		break;
	default:
		return;
	}

	regmap_update_bits(afe->regmap, GENERAL_ASRC_MODE, mask, val);
}

static void mt8365_afe_gasrc_enable_iir(struct mtk_base_afe *afe,
	int gasrc_id)
{
	unsigned int ctrl_reg = gasrc_ctrl_reg[gasrc_id].con2;

	regmap_update_bits(afe->regmap, ctrl_reg,
		GASRC_CON0_CHSET_IIR_EN,
		GASRC_CON0_CHSET_IIR_EN);
}

static void mt8365_afe_gasrc_disable_iir(struct mtk_base_afe *afe,
	int gasrc_id)
{
	unsigned int ctrl_reg = gasrc_ctrl_reg[gasrc_id].con2;

	regmap_update_bits(afe->regmap, ctrl_reg,
		GASRC_CON0_CHSET_IIR_EN, 0);
}

static int mt8365_afe_enable_gasrc(struct mtk_base_afe *afe,
	int gasrc_id)
{
	unsigned int ctrl_reg = 0;
	int ret = 0;

	if (gasrc_id == MT8365_GASRC1 ||  gasrc_id == MT8365_GASRC2)
		regmap_update_bits(afe->regmap, GENERAL_ASRC_EN_ON,
			GASRC_EN_ON_MASK(gasrc_id), GASRC_EN_ON(gasrc_id));

	ctrl_reg = gasrc_ctrl_reg[gasrc_id].con0;
	regmap_update_bits(afe->regmap, ctrl_reg,
		GASRC_CON0_ASM_ON, GASRC_CON0_ASM_ON);

	return ret;
}

static int mt8365_afe_disable_gasrc(struct mtk_base_afe *afe,
	int gasrc_id)
{
	unsigned int ctrl_reg = 0;
	struct mt8365_afe_private *afe_priv = afe->platform_priv;

	ctrl_reg = gasrc_ctrl_reg[gasrc_id].con0;
	regmap_update_bits(afe->regmap, ctrl_reg,
		GASRC_CON0_ASM_ON, 0x0);

	if (gasrc_id == MT8365_GASRC1 ||  gasrc_id == MT8365_GASRC2)
		regmap_update_bits(afe->regmap, GENERAL_ASRC_EN_ON,
			GASRC_EN_ON_MASK(gasrc_id), GASRC_EN_OFF(gasrc_id));

	if (afe_priv->gasrc_data[gasrc_id].iir_on)
		mt8365_afe_gasrc_disable_iir(afe, gasrc_id);

	return 0;
}

static void mt8365_afe_clear_gasrc(struct mtk_base_afe *afe,
	int gasrc_id)
{
	unsigned int ctrl_reg = 0;

	ctrl_reg = gasrc_ctrl_reg[gasrc_id].con0;

	regmap_update_bits(afe->regmap, ctrl_reg,
		GASRC_CON0_CHSET_STR_CLR, GASRC_CON0_CHSET_STR_CLR);
}

struct mt8365_gasrc_rx_freq {
	unsigned int rate;
	unsigned int freq_val;
};

static const
	struct mt8365_gasrc_rx_freq gasrc_frequency_palette[] = {
	{ .rate = 8000, .freq_val = 0x050000},
	{ .rate = 11025, .freq_val = 0x06E400},
	{ .rate = 12000, .freq_val = 0x078000},
	{ .rate = 16000, .freq_val = 0x0A0000},
	{ .rate = 22050, .freq_val = 0x0DC800},
	{ .rate = 24000, .freq_val = 0x0F0000},
	{ .rate = 32000, .freq_val = 0x140000},
	{ .rate = 44100, .freq_val = 0x1B9000},
	{ .rate = 48000, .freq_val = 0x1E0000},
	{ .rate = 88200, .freq_val = 0x372000},
	{ .rate = 96000, .freq_val = 0x3C0000},
	{ .rate = 176400, .freq_val = 0x6E4000},
	{ .rate = 192000, .freq_val = 0x780000},
};

struct mt8365_gasrc_tx_freq {
	unsigned int rate;
	unsigned int freq_val1; /* 48k domain val*/
	unsigned int freq_val2; /* 44.1k domain val*/
};

static const
	struct mt8365_gasrc_tx_freq gasrc_period_palette[] = {
	{ .rate = 8000, .freq_val1 = 0x4C2C0, .freq_val2 = 0x15DEA2},
	{ .rate = 11025, .freq_val1 = 0x3745A, .freq_val2 = 0xFDE80},
	{ .rate = 12000, .freq_val1 = 0x32C80, .freq_val2 = 0xE946C},
	{ .rate = 16000, .freq_val1 = 0x26160, .freq_val2 = 0xAEF51},
	{ .rate = 22050, .freq_val1 = 0x1BA2D, .freq_val2 = 0x7EF40},
	{ .rate = 24000, .freq_val1 = 0x19640, .freq_val2 = 0x74A36},
	{ .rate = 32000, .freq_val1 = 0x130B0, .freq_val2 = 0x577A8},
	{ .rate = 44100, .freq_val1 = 0xDD16, .freq_val2 = 0x3F7A0},
	{ .rate = 48000, .freq_val1 = 0xCB20, .freq_val2 = 0x3A51B},
	{ .rate = 88200, .freq_val1 = 0x6E8B, .freq_val2 = 0x1FBD0},
	{ .rate = 96000, .freq_val1 = 0x6590, .freq_val2 = 0x1D28D},
	{ .rate = 176400, .freq_val1 = 0x3745, .freq_val2 = 0xFDE8},
	{ .rate = 192000, .freq_val1 = 0x32C8, .freq_val2 = 0xE947},
};

static const
	struct mt8365_gasrc_tx_freq gasrc_tx_AutoRstThHi[] = {
	{ .rate = 8000, .freq_val1 = 0x51000, .freq_val2 = 0x170000},
	{ .rate = 11025, .freq_val1 = 0x3B000, .freq_val2 = 0x110000},
	{ .rate = 12000, .freq_val1 = 0x36000, .freq_val2 = 0xF8000},
	{ .rate = 16000, .freq_val1 = 0x28000, .freq_val2 = 0xBA000},
	{ .rate = 22050, .freq_val1 = 0x1D000, .freq_val2 = 0x87000},
	{ .rate = 24000, .freq_val1 = 0x1B000, .freq_val2 = 0x7C000},
	{ .rate = 32000, .freq_val1 = 0x14000, .freq_val2 = 0x5D000},
	{ .rate = 44100, .freq_val1 = 0xEB00, .freq_val2 = 0x44000},
	{ .rate = 48000, .freq_val1 = 0xD700, .freq_val2 = 0x3F000},
	{ .rate = 88200, .freq_val1 = 0x7500, .freq_val2 = 0x22000},
	{ .rate = 96000, .freq_val1 = 0x6C00, .freq_val2 = 0x1F000},
	{ .rate = 176400, .freq_val1 = 0x3B00, .freq_val2 = 0x11000},
	{ .rate = 192000, .freq_val1 = 0x3600, .freq_val2 = 0xF800},
};

static const
	struct mt8365_gasrc_tx_freq gasrc_tx_AutoRstThLo[] = {
	{ .rate = 8000, .freq_val1 = 0x47000, .freq_val2 = 0x150000},
	{ .rate = 11025, .freq_val1 = 0x34000, .freq_val2 = 0xEE000},
	{ .rate = 12000, .freq_val1 = 0x30000, .freq_val2 = 0xDB000},
	{ .rate = 16000, .freq_val1 = 0x24000, .freq_val2 = 0xA4000},
	{ .rate = 22050, .freq_val1 = 0x1A000, .freq_val2 = 0x77000},
	{ .rate = 24000, .freq_val1 = 0x18000, .freq_val2 = 0x6D000},
	{ .rate = 32000, .freq_val1 = 0x1E000, .freq_val2 = 0x52000},
	{ .rate = 44100, .freq_val1 = 0xCF00, .freq_val2 = 0x3C000},
	{ .rate = 48000, .freq_val1 = 0xBE00, .freq_val2 = 0x37000},
	{ .rate = 88200, .freq_val1 = 0x6800, .freq_val2 = 0x1E000},
	{ .rate = 96000, .freq_val1 = 0x5F00, .freq_val2 = 0x1B000},
	{ .rate = 176400, .freq_val1 = 0x3400, .freq_val2 = 0xEE00},
	{ .rate = 192000, .freq_val1 = 0x3000, .freq_val2 = 0xDB00},
};

static const
	struct mt8365_gasrc_rx_freq gasrc_rx_AutoRstThHi[] = {
	{ .rate = 8000, .freq_val = 0x36000},
	{ .rate = 11025, .freq_val = 0x27000},
	{ .rate = 12000, .freq_val = 0x24000},
	{ .rate = 16000, .freq_val = 0x1B000},
	{ .rate = 22050, .freq_val = 0x14000},
	{ .rate = 24000, .freq_val = 0x12000},
	{ .rate = 32000, .freq_val = 0x0D800},
	{ .rate = 44100, .freq_val = 0x09D00},
	{ .rate = 48000, .freq_val = 0x08E00},
	{ .rate = 88200, .freq_val = 0x04E00},
	{ .rate = 96000, .freq_val = 0x04800},
	{ .rate = 176400, .freq_val = 0x02700},
	{ .rate = 192000, .freq_val = 0x02400},
};

static const
	struct mt8365_gasrc_rx_freq gasrc_rx_AutoRstThLo[] = {
	{ .rate = 8000, .freq_val = 0x30000},
	{ .rate = 11025, .freq_val = 0x23000},
	{ .rate = 12000, .freq_val = 0x20000},
	{ .rate = 16000, .freq_val = 0x18000},
	{ .rate = 22050, .freq_val = 0x11000},
	{ .rate = 24000, .freq_val = 0x0FE00},
	{ .rate = 32000, .freq_val = 0x0BE00},
	{ .rate = 44100, .freq_val = 0x08A00},
	{ .rate = 48000, .freq_val = 0x07F00},
	{ .rate = 88200, .freq_val = 0x04500},
	{ .rate = 96000, .freq_val = 0x04000},
	{ .rate = 176400, .freq_val = 0x02300},
	{ .rate = 192000, .freq_val = 0x02000},
};

static unsigned int mt8365_afe_gasrc_get_freq_val(unsigned int rate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(gasrc_frequency_palette); i++)
		if (gasrc_frequency_palette[i].rate == rate)
			return gasrc_frequency_palette[i].freq_val;

	return -EINVAL;
}

static unsigned int mt8365_afe_gasrc_get_period_val(unsigned int rate,
	unsigned int domain)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(gasrc_period_palette); i++) {
		if (gasrc_period_palette[i].rate == rate) {
			if (domain)
				return gasrc_period_palette[i].freq_val1;
			else
				return gasrc_period_palette[i].freq_val2;
		}
	}
	return -EINVAL;
}

static void mt8365_afe_gasrc_rx_frequency_palette(
	struct mtk_base_afe *afe, int gasrc_id,
	int input_rate, int output_rate)
{
	unsigned int ctrl_reg = 0;

	ctrl_reg = gasrc_ctrl_reg[gasrc_id].con3;
	if (mt8365_afe_gasrc_get_freq_val(output_rate) > 0)
		regmap_write(afe->regmap, ctrl_reg,
			mt8365_afe_gasrc_get_freq_val(output_rate));

	ctrl_reg = gasrc_ctrl_reg[gasrc_id].con4;
	if (mt8365_afe_gasrc_get_freq_val(input_rate) > 0)
		regmap_write(afe->regmap, ctrl_reg,
			mt8365_afe_gasrc_get_freq_val(input_rate));
}

static void mt8365_afe_gasrc_tx_frequency_palette(
	struct mtk_base_afe *afe,
	int gasrc_id, int input_rate,
	int output_rate, unsigned int domain)
{
	unsigned int ctrl_reg = 0;

	ctrl_reg = gasrc_ctrl_reg[gasrc_id].con3;
	if (mt8365_afe_gasrc_get_period_val(input_rate, domain) > 0)
		regmap_write(afe->regmap, ctrl_reg,
			mt8365_afe_gasrc_get_period_val(input_rate, domain));

	ctrl_reg = gasrc_ctrl_reg[gasrc_id].con4;
	if (mt8365_afe_gasrc_get_period_val(output_rate, domain) > 0)
		regmap_write(afe->regmap, ctrl_reg,
			mt8365_afe_gasrc_get_period_val(output_rate, domain));
}

static unsigned int mt8365_afe_gasrc_get_tx_autorst_th_high(
	unsigned int rate, unsigned int domain)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(gasrc_tx_AutoRstThHi); i++) {
		if (gasrc_tx_AutoRstThHi[i].rate == rate) {
			if (domain)
				return gasrc_tx_AutoRstThHi[i].freq_val1;
			else
				return gasrc_tx_AutoRstThHi[i].freq_val2;
		}
	}
	return -EINVAL;
}

static unsigned int mt8365_afe_gasrc_get_tx_autorst_th_low(
	unsigned int rate, unsigned int domain)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(gasrc_tx_AutoRstThLo); i++) {
		if (gasrc_tx_AutoRstThLo[i].rate == rate) {
			if (domain)
				return gasrc_tx_AutoRstThLo[i].freq_val1;
			else
				return gasrc_tx_AutoRstThLo[i].freq_val2;
		}
	}
	return -EINVAL;
}

static void mt8365_afe_gasrc_autoRstThHi_for_tx(struct mtk_base_afe *afe,
	int gasrc_id, int output_rate, unsigned int domain)
{
	unsigned int ctrl_reg = 0;
	unsigned int val = 0;

	ctrl_reg = gasrc_ctrl_reg[gasrc_id].con9;
	val = mt8365_afe_gasrc_get_tx_autorst_th_high(output_rate, domain);
	if (val > 0)
		regmap_write(afe->regmap, ctrl_reg, val);
}

static void mt8365_afe_gasrc_autoRstThLo_for_tx(struct mtk_base_afe *afe,
	int gasrc_id, int output_rate, unsigned int domain)
{
	unsigned int ctrl_reg = 0;
	unsigned int val = 0;

	ctrl_reg = gasrc_ctrl_reg[gasrc_id].con10;
	val = mt8365_afe_gasrc_get_tx_autorst_th_low(output_rate, domain);
	if (val > 0)
		regmap_write(afe->regmap, ctrl_reg, val);
}

static unsigned int mt8365_afe_gasrc_get_rx_autorst_th_high(unsigned int rate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(gasrc_rx_AutoRstThHi); i++)
		if (gasrc_rx_AutoRstThHi[i].rate == rate)
			return gasrc_rx_AutoRstThHi[i].freq_val;

	return -EINVAL;
}

static unsigned int mt8365_afe_gasrc_get_rx_autorst_th_low(unsigned int rate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(gasrc_rx_AutoRstThLo); i++)
		if (gasrc_rx_AutoRstThLo[i].rate == rate)
			return gasrc_rx_AutoRstThLo[i].freq_val;

	return -EINVAL;
}

static void mt8365_afe_gasrc_autoRstThHi_for_rx(struct mtk_base_afe *afe,
	int gasrc_id, int input_rate)
{
	unsigned int ctrl_reg = 0;

	ctrl_reg = gasrc_ctrl_reg[gasrc_id].con9;
	if (mt8365_afe_gasrc_get_rx_autorst_th_high(input_rate) > 0)
		regmap_write(afe->regmap, ctrl_reg,
			mt8365_afe_gasrc_get_rx_autorst_th_high(input_rate));
}

static void mt8365_afe_gasrc_autoRstThLo_for_rx(struct mtk_base_afe *afe,
	int gasrc_id, int input_rate)
{
	unsigned int ctrl_reg = 0;

	ctrl_reg = gasrc_ctrl_reg[gasrc_id].con10;
	if (mt8365_afe_gasrc_get_rx_autorst_th_low(input_rate) > 0)
		regmap_write(afe->regmap, ctrl_reg,
			mt8365_afe_gasrc_get_rx_autorst_th_low(input_rate));
}

static void mt8365_afe_gasrc_setup_cali_tx(struct mtk_base_afe *afe,
	int gasrc_id, unsigned int domain)
{
	unsigned int mask = 0;
	unsigned int val = 0;
	unsigned int ctrl_reg = 0;

	ctrl_reg = gasrc_ctrl_reg[gasrc_id].con5;

	val = (domain ? GASRC_CON5_CALI_96_CYCLE : GASRC_CON5_CALI_441_CYCLE) |
		GASRC_CON5_CALI_AUTORST_ENABLE |
		GASRC_CON5_AUTO_TUNE_FREQ5_ENABLE |
		GASRC_CON5_COMP_FREQ_RES_ENABLE |
		GASRC_CON5_CALI_BP_DGL_BYPASS |
		GASRC_CON5_AUTO_TUNE_FREQ4_DISABLE |
		GASRC_CON5_CALI_AUTO_RESTART_ENABLE |
		GASRC_CON5_CALI_USE_PERIOD |
		GASRC_CON5_CALI_OFF;

	mask = GASRC_CON5_CALI_CYCLE_MASK |
		GASRC_CON5_CALI_AUTORST_ENABLE |
		GASRC_CON5_AUTO_TUNE_FREQ5_ENABLE |
		GASRC_CON5_COMP_FREQ_RES_ENABLE |
		GASRC_CON5_CALI_BP_DGL_BYPASS |
		GASRC_CON5_AUTO_TUNE_FREQ4_ENABLE |
		GASRC_CON5_CALI_AUTO_RESTART_ENABLE |
		GASRC_CON5_CALI_USE_FREQ |
		GASRC_CON5_CALI_ON;

	if (gasrc_id == MT8365_GASRC1 || gasrc_id == MT8365_GASRC2) {
		val |= GASRC_CON5_CALI_SEL_01;
		mask |= GASRC_CON5_CALI_SEL_MASK;

	}

	regmap_update_bits(afe->regmap, ctrl_reg, mask, val);
}

static void mt8365_afe_gasrc_enable_cali(struct mtk_base_afe *afe,
	int gasrc_id)
{
	unsigned int ctrl_reg = 0;

	ctrl_reg = gasrc_ctrl_reg[gasrc_id].con5;

	regmap_update_bits(afe->regmap, ctrl_reg,
		GASRC_CON5_CALI_ON, GASRC_CON5_CALI_ON);
}

static void mt8365_afe_gasrc_disable_cali(struct mtk_base_afe *afe,
	int gasrc_id)
{
	unsigned int ctrl_reg = 0;

	ctrl_reg = gasrc_ctrl_reg[gasrc_id].con5;

	regmap_update_bits(afe->regmap, ctrl_reg,
		GASRC_CON5_CALI_ON, GASRC_CON5_CALI_OFF);
}

static void mt8365_afe_gasrc_setup_cali_rx(struct mtk_base_afe *afe,
	int gasrc_id)
{
	unsigned int mask = 0;
	unsigned int val = 0;
	unsigned int ctrl_reg = 0;

	ctrl_reg = gasrc_ctrl_reg[gasrc_id].con5;

	val = GASRC_CON5_CALI_64_CYCLE  |
		GASRC_CON5_CALI_AUTORST_ENABLE |
		GASRC_CON5_AUTO_TUNE_FREQ5_ENABLE |
		GASRC_CON5_COMP_FREQ_RES_ENABLE |
		GASRC_CON5_CALI_BP_DGL_BYPASS |
		GASRC_CON5_AUTO_TUNE_FREQ4_DISABLE |
		GASRC_CON5_CALI_AUTO_RESTART_ENABLE |
		GASRC_CON5_CALI_USE_FREQ |
		GASRC_CON5_CALI_OFF;

	mask = GASRC_CON5_CALI_CYCLE_MASK |
		GASRC_CON5_CALI_AUTORST_ENABLE |
		GASRC_CON5_AUTO_TUNE_FREQ5_ENABLE |
		GASRC_CON5_COMP_FREQ_RES_ENABLE |
		GASRC_CON5_CALI_BP_DGL_BYPASS |
		GASRC_CON5_AUTO_TUNE_FREQ4_ENABLE |
		GASRC_CON5_CALI_AUTO_RESTART_ENABLE |
		GASRC_CON5_CALI_USE_FREQ |
		GASRC_CON5_CALI_ON;

	regmap_update_bits(afe->regmap, ctrl_reg, mask, val);
}

static void mt8365_afe_gasrc_fixed_denominator(struct mtk_base_afe *afe,
	int gasrc_id)
{
	unsigned int ctrl_reg = 0;

	ctrl_reg = gasrc_ctrl_reg[gasrc_id].con6;

	regmap_write(afe->regmap, ctrl_reg, 0x1FBD);
}

enum afe_gasrc_iir_coeff_table_id {
	MT8365_AFE_GASRC_IIR_COEFF_48_TO_44p1 = 0,
	MT8365_AFE_GASRC_IIR_COEFF_44p1_TO_32,
	MT8365_AFE_GASRC_IIR_COEFF_48_TO_32,
	MT8365_AFE_GASRC_IIR_COEFF_32_TO_16,
	MT8365_AFE_GASRC_IIR_COEFF_96_TO_44p1,
	MT8365_AFE_GASRC_IIR_COEFF_44p1_TO_16,
	MT8365_AFE_GASRC_IIR_COEFF_48_TO_16,
	MT8365_AFE_GASRC_IIR_COEFF_96_TO_16,
	MT8365_AFE_GASRC_IIR_TABLES,
};

struct mt8365_afe_gasrc_iir_coeff_table_id {
	int input_rate;
	int output_rate;
	int coeff_count;
	int table_id;
};

static const struct mt8365_afe_gasrc_iir_coeff_table_id
	mt8365_afe_gasrc_iir_coeff_table_ids[] = {
	{
		.input_rate = 48000,
		.output_rate = 44100,
		.coeff_count = 30,
		.table_id = MT8365_AFE_GASRC_IIR_COEFF_48_TO_44p1,
	},
	{
		.input_rate = 44100,
		.output_rate = 32000,
		.coeff_count = 42,
		.table_id = MT8365_AFE_GASRC_IIR_COEFF_44p1_TO_32,
	},
	{
		.input_rate = 48000,
		.output_rate = 32000,
		.coeff_count = 42,
		.table_id = MT8365_AFE_GASRC_IIR_COEFF_48_TO_32,
	},
	{
		.input_rate = 32000,
		.output_rate = 16000,
		.coeff_count = 48,
		.table_id = MT8365_AFE_GASRC_IIR_COEFF_32_TO_16,
	},
	{
		.input_rate = 96000,
		.output_rate = 44100,
		.coeff_count = 48,
		.table_id = MT8365_AFE_GASRC_IIR_COEFF_96_TO_44p1,
	},
	{
		.input_rate = 44100,
		.output_rate = 16000,
		.coeff_count = 48,
		.table_id = MT8365_AFE_GASRC_IIR_COEFF_44p1_TO_16,
	},
	{
		.input_rate = 48000,
		.output_rate = 16000,
		.coeff_count = 48,
		.table_id = MT8365_AFE_GASRC_IIR_COEFF_48_TO_16,
	},
	{
		.input_rate = 96000,
		.output_rate = 16000,
		.coeff_count = 48,
		.table_id = MT8365_AFE_GASRC_IIR_COEFF_96_TO_16,
	},
};

#define IIR_NUMS (48)

static const unsigned int
	mt8365_afe_gasrc_iir_coeffs[MT8365_AFE_GASRC_IIR_TABLES][IIR_NUMS] = {
	[MT8365_AFE_GASRC_IIR_COEFF_48_TO_44p1] = {
		0x0622d4, 0x0bd866, 0x0622d4,
		0xc747cc, 0xe14600, 0x000002,
		0x0720b6, 0x0ddabb, 0x0720b6,
		0xe59178, 0xf23637, 0x000003,
		0x0c7d72, 0x189060, 0x0c7d72,
		0xd2de13, 0xea0b64, 0x000002,
		0x0eb376, 0x1d4103, 0x0eb376,
		0xe1fc0c, 0xf4b20a, 0x000002,
		0x000000, 0x3c43ff, 0x3c43ff,
		0xfeedaa, 0x000000, 0x000005,
		0xffffff, 0xffffff, 0xffffff,
		0xffffff, 0xffffff, 0xffffff,
		0xffffff, 0xffffff, 0xffffff,
		0xffffff, 0xffffff, 0xffffff,
		0xffffff, 0xffffff, 0xffffff,
		0xffffff, 0xffffff, 0xffffff,
	},
	[MT8365_AFE_GASRC_IIR_COEFF_44p1_TO_32] = {
		0x02145b, 0x02a20b, 0x02145b,
		0xdd8d6c, 0xe0b3f6, 0x000002,
		0x0e41f8, 0x128d48, 0x0e41f8,
		0xdf829d, 0xe25af4, 0x000002,
		0x0cfa60, 0x11e89c, 0x0cfa60,
		0xe3613c, 0xe4e40b, 0x000002,
		0x115ee3, 0x1a531d, 0x115ee3,
		0xea799a, 0xe9314a, 0x000002,
		0x0f79e2, 0x1a7064, 0x0f79e2,
		0xf65e4a, 0xf03d8e, 0x000002,
		0x10c34f, 0x1ffe4b, 0x10c34f,
		0x0bbecb, 0xf2bc4b, 0x000001,
		0x000000, 0x358477, 0x358477,
		0x007336, 0x000000, 0x000006
	},
	[MT8365_AFE_GASRC_IIR_COEFF_48_TO_32] = {
		0x03f333, 0x03cd81, 0x03f333,
		0xcba1ad, 0xc0e834, 0x000001,
		0x0ea233, 0x0e7a31, 0x0ea233,
		0xe7001b, 0xe182b7, 0x000002,
		0x0db060, 0x0e616c, 0x0db060,
		0xe964a5, 0xe31704, 0x000002,
		0x135c2d, 0x168145, 0x135c2d,
		0xedc25d, 0xe5b59c, 0x000002,
		0x13553f, 0x1a02e5, 0x13553f,
		0xf5400d, 0xea128f, 0x000002,
		0x19c8dc, 0x295add, 0x19c8dc,
		0x019986, 0xe179e6, 0x000001,
		0x0cf3a4, 0x184826, 0x0cf3a4,
		0x1cba28, 0xf1166a, 0x000001,
		0x000000, 0x31315d, 0x31315d,
		0x00a9b9, 0x000000, 0x000006
	},
	[MT8365_AFE_GASRC_IIR_COEFF_32_TO_16] = {
		0x122893, 0xffadd4, 0x122893,
		0x0bc205, 0xc0ee1c, 0x000001,
		0x1bab8a, 0x00750d, 0x1bab8a,
		0x06a983, 0xe18a5c, 0x000002,
		0x18f68e, 0x02706f, 0x18f68e,
		0x0886a9, 0xe31bcb, 0x000002,
		0x149c05, 0x054487, 0x149c05,
		0x0bec31, 0xe5973e, 0x000002,
		0x0ea303, 0x07f24a, 0x0ea303,
		0x115ff9, 0xe967b6, 0x000002,
		0x0823fd, 0x085531, 0x0823fd,
		0x18d5b4, 0xee8d21, 0x000002,
		0x06888e, 0x0acbbb, 0x06888e,
		0x40b55c, 0xe76dce, 0x000001,
		0x000000, 0x2d31a9, 0x2d31a9,
		0x23ba4f, 0x000000, 0x000001,
	},
	[MT8365_AFE_GASRC_IIR_COEFF_96_TO_44p1] = {
		0x08b543, 0xfd80f4, 0x08b543,
		0x0e2332, 0xe06ed0, 0x000002,
		0x1b6038, 0xf90e7e, 0x1b6038,
		0x0ec1ac, 0xe16f66, 0x000002,
		0x188478, 0xfbb921, 0x188478,
		0x105859, 0xe2e596, 0x000002,
		0x13eff3, 0xffa707, 0x13eff3,
		0x13455c, 0xe533b7, 0x000002,
		0x0dc239, 0x03d458, 0x0dc239,
		0x17f120, 0xe8b617, 0x000002,
		0x0745f1, 0x05d790, 0x0745f1,
		0x1e3d75, 0xed5f18, 0x000002,
		0x05641f, 0x085e2b, 0x05641f,
		0x48efd0, 0xe3e9c8, 0x000001,
		0x000000, 0x28f632, 0x28f632,
		0x273905, 0x000000, 0x000001,
	},
	[MT8365_AFE_GASRC_IIR_COEFF_44p1_TO_16] = {
		0x01ec62, 0xfe6435, 0x01ec62,
		0x1e54a0, 0xe06605, 0x000002,
		0x0d828e, 0xf50f97, 0x0d828e,
		0x1e836a, 0xe15331, 0x000002,
		0x0bf5f5, 0xf7186c, 0x0bf5f5,
		0x1f48ca, 0xe2ae88, 0x000002,
		0x0f2ef8, 0xf6be4a, 0x0f2ef8,
		0x20cc51, 0xe4d068, 0x000002,
		0x0c7ac6, 0xfbd00e, 0x0c7ac6,
		0x2337da, 0xe8028c, 0x000002,
		0x060ddc, 0x015b3e, 0x060ddc,
		0x266754, 0xec21b6, 0x000002,
		0x0407b5, 0x04f827, 0x0407b5,
		0x52e3d0, 0xe0149f, 0x000001,
		0x000000, 0x1f4447, 0x1f4447,
		0x02ac11, 0x000000, 0x000005
	},
	[MT8365_AFE_GASRC_IIR_COEFF_48_TO_16] = {
		0x01def5, 0xfe0fde, 0x01def5,
		0x2474e5, 0xe062e6, 0x000002,
		0x0d4180, 0xf297f4, 0x0d4180,
		0x2482b6, 0xe14760, 0x000002,
		0x0ba079, 0xf4f0b0, 0x0ba079,
		0x250ba7, 0xe29116, 0x000002,
		0x0e8397, 0xf40296, 0x0e8397,
		0x2625be, 0xe48e0d, 0x000002,
		0x0b98e0, 0xf96d1a, 0x0b98e0,
		0x27e79c, 0xe7798a, 0x000002,
		0x055e3b, 0xffed09, 0x055e3b,
		0x2a2e2d, 0xeb2854, 0x000002,
		0x01a934, 0x01ca03, 0x01a934,
		0x2c4fea, 0xee93ab, 0x000002,
		0x000000, 0x24a572, 0x24a572,
		0x02d37e, 0x000000, 0x000005
	},
	[MT8365_AFE_GASRC_IIR_COEFF_96_TO_16] = {
		0x0805a1, 0xf21ae3, 0x0805a1,
		0x3840bb, 0xe02a2e, 0x000002,
		0x0d5dd8, 0xe8f259, 0x0d5dd8,
		0x1c0af6, 0xf04700, 0x000003,
		0x0bb422, 0xec08d9, 0x0bb422,
		0x1bfccc, 0xf09216, 0x000003,
		0x08fde6, 0xf108be, 0x08fde6,
		0x1bf096, 0xf10ae0, 0x000003,
		0x0ae311, 0xeeeda3, 0x0ae311,
		0x37c646, 0xe385f5, 0x000002,
		0x044089, 0xfa7242, 0x044089,
		0x37a785, 0xe56526, 0x000002,
		0x00c75c, 0xffb947, 0x00c75c,
		0x378ba3, 0xe72c5f, 0x000002,
		0x000000, 0x0ef76e, 0x0ef76e,
		0x377fda, 0x000000, 0x000001,
	},
};

static bool mt8365_afe_gasrc_fill_iir_coeff_table(struct mtk_base_afe *afe,
	int gasrc_id, int table_id, int coeff_count)
{
	const unsigned int *coef = NULL;
	unsigned int ctrl_reg;
	unsigned int iir_stage;
	int i;

	if ((table_id < 0) ||
		(table_id >= MT8365_AFE_GASRC_IIR_TABLES))
		return false;

	dev_dbg(afe->dev, "%s table_id %d coeff_count %d\n",
		__func__, table_id, coeff_count);

	coef = &mt8365_afe_gasrc_iir_coeffs[table_id][0];

	iir_stage = (coeff_count / 6) - 1;

	/* enable access for iir sram */
	ctrl_reg = gasrc_ctrl_reg[gasrc_id].con0;
	regmap_update_bits(afe->regmap, ctrl_reg,
		GASRC_CON0_COEFF_SRAM_CTRL,
		GASRC_CON0_COEFF_SRAM_CTRL);

	/* fill coeffs from addr 0 */
	ctrl_reg = gasrc_ctrl_reg[gasrc_id].con13;
	regmap_write(afe->regmap, ctrl_reg, 0x0);

	/* fill all coeffs */
	ctrl_reg = gasrc_ctrl_reg[gasrc_id].con12;
	for (i = 0; i < coeff_count; i++)
		regmap_write(afe->regmap, ctrl_reg, coef[i]);

	/* disable access for iir sram */
	ctrl_reg = gasrc_ctrl_reg[gasrc_id].con0;
	regmap_update_bits(afe->regmap, ctrl_reg,
		GASRC_CON0_COEFF_SRAM_CTRL, 0);

	/* enable irr */
	ctrl_reg = gasrc_ctrl_reg[gasrc_id].con2;
	regmap_update_bits(afe->regmap, ctrl_reg,
		GASRC_CON0_CHSET_IIR_STAGE_MASK,
		GASRC_CON0_CHSET_IIR_STAGE(iir_stage));

	regmap_update_bits(afe->regmap, ctrl_reg,
		GASRC_CON0_CHSET0_CLR_IIR_HISTORY,
		GASRC_CON0_CHSET0_CLR_IIR_HISTORY);

	return true;
}

static bool mt8365_afe_gasrc_found_iir_coeff_table_id(int input_rate,
	int output_rate, int *table_id, int *coeff_count)
{
	int i;
	const struct mt8365_afe_gasrc_iir_coeff_table_id *table =
		mt8365_afe_gasrc_iir_coeff_table_ids;

	if (!table_id)
		return false;

	/* no need to apply iir for up-sample */
	if (input_rate <= output_rate)
		return false;

	for (i = 0; i < ARRAY_SIZE(mt8365_afe_gasrc_iir_coeff_table_ids); i++) {
		if ((input_rate * table[i].output_rate) ==
			(output_rate * table[i].input_rate)) {
			*table_id = table[i].table_id;
			*coeff_count = table[i].coeff_count;
			return true;
		}
	}

	return false;
}

static bool mt8365_afe_load_gasrc_iir_coeff_table(struct mtk_base_afe *afe,
	int gasrc_id, int input_rate, int output_rate)
{
	int table_id;
	int coeff_count;

	if (mt8365_afe_gasrc_found_iir_coeff_table_id(input_rate,
			output_rate, &table_id, &coeff_count)) {
		return mt8365_afe_gasrc_fill_iir_coeff_table(afe,
			gasrc_id, table_id, coeff_count);
	}

	return false;
}

static int mt8365_afe_configure_gasrc(struct mtk_base_afe *afe,
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	struct snd_pcm_runtime * const runtime = substream->runtime;
	unsigned int stream = substream->stream;
	const int gasrc_id = mt8365_dai_num_to_gasrc(dai->id);
	int input_fs = 0, output_fs = 0;
	int input_rate = 0, output_rate = 0;
	unsigned int infs_48k_domain = 0;
	bool duplex;
	struct snd_soc_pcm_runtime *be = substream->private_data;
	struct snd_pcm_substream *paired_substream = NULL;

	if (gasrc_id < 0) {
		dev_info(afe->dev, "%s Invaild gasrc_id return %d !!!\n",
			__func__, gasrc_id);
		return 0;
	}

	duplex = afe_priv->gasrc_data[gasrc_id].duplex;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		input_fs = mt8365_afe_fs_timing(runtime->rate);
		input_rate = runtime->rate;

		if (duplex) {
			paired_substream = snd_soc_dpcm_get_substream(be,
				SNDRV_PCM_STREAM_CAPTURE);
			output_fs =
				mt8365_afe_fs_timing(
					paired_substream->runtime->rate);
			output_rate = paired_substream->runtime->rate;
		} else {
			output_fs = mt8365_afe_fs_timing(runtime->rate);
			output_rate = runtime->rate;
		}
	} else if (stream == SNDRV_PCM_STREAM_CAPTURE) {
		output_fs = mt8365_afe_fs_timing(runtime->rate);
		output_rate = runtime->rate;

		if (duplex) {
			paired_substream =
				snd_soc_dpcm_get_substream(be,
					SNDRV_PCM_STREAM_PLAYBACK);
			input_fs =
				mt8365_afe_fs_timing(
					paired_substream->runtime->rate);
			input_rate = paired_substream->runtime->rate;
		} else {
			input_fs = mt8365_afe_fs_timing(runtime->rate);
			input_rate = runtime->rate;
		}
	}

	dev_dbg(afe->dev, "%s input_fs 0x%x, output_fs 0x%x\n",
		__func__, input_fs, output_fs);
	dev_dbg(afe->dev, "%s input_rate %d, output_rate %d\n",
		__func__, input_rate, output_rate);

	dev_dbg(afe->dev, "%s tx_mode %d, cali_on %d\n",
		__func__, afe_priv->gasrc_data[gasrc_id].tx_mode,
		afe_priv->gasrc_data[gasrc_id].cali_on);

	if (mt8365_afe_load_gasrc_iir_coeff_table(afe, gasrc_id,
			input_rate, output_rate)) {
		mt8365_afe_gasrc_enable_iir(afe, gasrc_id);
		afe_priv->gasrc_data[gasrc_id].iir_on = true;
	} else {
		mt8365_afe_gasrc_disable_iir(afe, gasrc_id);
		afe_priv->gasrc_data[gasrc_id].iir_on = false;
	}

	mt8365_afe_gasrc_set_input_fs(afe, dai, input_fs);
	mt8365_afe_gasrc_set_output_fs(afe, dai, output_fs);

	infs_48k_domain = mt8365_afe_clk_group_48k(input_rate);

	/* config con3/con4 */
	if (afe_priv->gasrc_data[gasrc_id].tx_mode) {
		mt8365_afe_gasrc_tx_frequency_palette(afe,
			gasrc_id, input_rate, output_rate, infs_48k_domain);

		if (afe_priv->gasrc_data[gasrc_id].cali_on) {
			mt8365_afe_gasrc_setup_cali_tx(afe,
				gasrc_id, infs_48k_domain);
			mt8365_afe_gasrc_enable_cali(afe, gasrc_id);
		} else
			mt8365_afe_gasrc_disable_cali(afe, gasrc_id);
	} else {
		mt8365_afe_gasrc_rx_frequency_palette(afe,
			gasrc_id, input_rate, output_rate);

		if (afe_priv->gasrc_data[gasrc_id].cali_on) {
			mt8365_afe_gasrc_setup_cali_rx(afe, gasrc_id);
			mt8365_afe_gasrc_enable_cali(afe, gasrc_id);
		} else
			mt8365_afe_gasrc_disable_cali(afe, gasrc_id);
	}

	mt8365_afe_gasrc_fixed_denominator(afe, gasrc_id);

	if (afe_priv->gasrc_data[gasrc_id].tx_mode) {
		mt8365_afe_gasrc_autoRstThHi_for_tx(afe,
			gasrc_id, output_rate, infs_48k_domain);
		mt8365_afe_gasrc_autoRstThLo_for_tx(afe,
			gasrc_id, output_rate, infs_48k_domain);
	} else {
		mt8365_afe_gasrc_autoRstThHi_for_rx(afe,
			gasrc_id, input_rate);
		mt8365_afe_gasrc_autoRstThLo_for_rx(afe,
			gasrc_id, input_rate);
	}

	return 0;
}

static void mt8365_afe_tdm_asrc_set(struct mtk_base_afe *afe,
	struct snd_soc_dai *dai, int fs_timing)
{
	unsigned int val = 0;
	unsigned int mask = 0;

	val = TDM_ASRC_OUT_MODE(fs_timing)
		| TDM_ASRC_IN_SEL_FROM_TDMOUT
		| TDM_ASRC_EN_ON;

	mask = TDM_ASRC_OUT_MODE_MASK
		| TDM_ASRC_IN_SEL_MASK
		| TDM_ASRC_EN_ON;

	regmap_update_bits(afe->regmap, AFE_TDM_ASRC_CON0, mask, val);
}

static void mt8365_afe_tdm_asrc_one_heart_mode(struct mtk_base_afe *afe,
	struct snd_soc_dai *dai)
{
	unsigned int ctrl_reg = 0;
	int tdm_asrc_id;

	for (tdm_asrc_id = MT8365_TDM_ASRC2;
			tdm_asrc_id < MT8365_TDM_ASRC_NUM; tdm_asrc_id++) {
		ctrl_reg = gasrc_ctrl_reg[tdm_asrc_id].con0;

		regmap_update_bits(afe->regmap,
			gasrc_ctrl_reg[tdm_asrc_id].con0,
			TDM_ASRC_CON0_ONE_HEART,
			TDM_ASRC_CON0_ONE_HEART);
	}
}

static void mt8365_afe_tdm_asrc_iir_coeff(struct mtk_base_afe *afe,
	int input_fs, int output_fs)
{
	int tdm_asrc_id;
	struct mt8365_afe_private *afe_priv = afe->platform_priv;

	for (tdm_asrc_id = MT8365_TDM_ASRC1;
			tdm_asrc_id < MT8365_TDM_ASRC_NUM; tdm_asrc_id++) {
		if (mt8365_afe_load_gasrc_iir_coeff_table(afe, tdm_asrc_id,
				input_fs, output_fs)) {
			mt8365_afe_gasrc_enable_iir(afe, tdm_asrc_id);
			afe_priv->gasrc_data[tdm_asrc_id].iir_on = true;
		} else {
			mt8365_afe_gasrc_disable_iir(afe, tdm_asrc_id);
			afe_priv->gasrc_data[tdm_asrc_id].iir_on = false;
		}
	}
}

static void mt8365_afe_tdm_asrc_tx_frequency_palette(struct mtk_base_afe *afe,
	int input_rate, int output_rate, int domain)
{
	int tdm_asrc_id;

	for (tdm_asrc_id = MT8365_TDM_ASRC1;
			tdm_asrc_id < MT8365_TDM_ASRC_NUM; tdm_asrc_id++) {
		mt8365_afe_gasrc_tx_frequency_palette(afe, tdm_asrc_id,
			input_rate, output_rate, domain);
	}
}

static void mt8365_afe_tdm_asrc_rx_frequency_palette(struct mtk_base_afe *afe,
	int input_rate, int output_rate)
{
	int tdm_asrc_id;

	for (tdm_asrc_id = MT8365_TDM_ASRC1;
			tdm_asrc_id < MT8365_TDM_ASRC_NUM; tdm_asrc_id++) {
		mt8365_afe_gasrc_rx_frequency_palette(afe, tdm_asrc_id,
			input_rate, output_rate);
	}
}

static void mt8365_afe_tdm_asrc_setup_cali_tx(struct mtk_base_afe *afe,
	int domain, bool cmNum)
{
	int tdm_asrc_id;
	unsigned int ctrl_reg = 0;

	for (tdm_asrc_id = MT8365_TDM_ASRC1;
			tdm_asrc_id < MT8365_TDM_ASRC_NUM; tdm_asrc_id++) {
		mt8365_afe_gasrc_setup_cali_tx(afe, tdm_asrc_id, domain);
		ctrl_reg = gasrc_ctrl_reg[tdm_asrc_id].con5;
		if (cmNum)
			regmap_update_bits(afe->regmap, ctrl_reg,
				GASRC_CON5_CALI_SEL_MASK,
				GASRC_CON5_CALI_SEL_01);
		else
			regmap_update_bits(afe->regmap, ctrl_reg,
				GASRC_CON5_CALI_SEL_MASK,
				GASRC_CON5_CALI_SEL_00);
	}
}

static void mt8365_afe_tdm_asrc_setup_cali_rx(struct mtk_base_afe *afe,
	bool cmNum)
{
	int tdm_asrc_id;
	unsigned int ctrl_reg = 0;

	for (tdm_asrc_id = MT8365_TDM_ASRC1;
			tdm_asrc_id < MT8365_TDM_ASRC_NUM; tdm_asrc_id++) {
		mt8365_afe_gasrc_setup_cali_rx(afe, tdm_asrc_id);
		ctrl_reg = gasrc_ctrl_reg[tdm_asrc_id].con5;
		if (cmNum)
			regmap_update_bits(afe->regmap, ctrl_reg,
				GASRC_CON5_CALI_SEL_MASK,
				GASRC_CON5_CALI_SEL_01);
		else
			regmap_update_bits(afe->regmap, ctrl_reg,
				GASRC_CON5_CALI_SEL_MASK,
				GASRC_CON5_CALI_SEL_00);
	}
}

static void mt8365_afe_tdm_asrc_enable_cali(struct mtk_base_afe *afe)
{
	int tdm_asrc_id;

	for (tdm_asrc_id = MT8365_TDM_ASRC1;
			tdm_asrc_id < MT8365_TDM_ASRC_NUM; tdm_asrc_id++)
		mt8365_afe_gasrc_enable_cali(afe, tdm_asrc_id);
}

static void mt8365_afe_tdm_asrc_disable_cali(struct mtk_base_afe *afe)
{
	int tdm_asrc_id;

	for (tdm_asrc_id = MT8365_TDM_ASRC4;
			tdm_asrc_id >= MT8365_TDM_ASRC1; tdm_asrc_id--)
		mt8365_afe_gasrc_disable_cali(afe, tdm_asrc_id);
}

static void mt8365_afe_tdm_asrc_fixed_denominator(struct mtk_base_afe *afe)
{
	int tdm_asrc_id;

	for (tdm_asrc_id = MT8365_TDM_ASRC1;
			tdm_asrc_id < MT8365_TDM_ASRC_NUM; tdm_asrc_id++)
		mt8365_afe_gasrc_fixed_denominator(afe, tdm_asrc_id);
}

static void mt8365_afe_tdm_asrc_autoRstThLo_for_tx(struct mtk_base_afe *afe,
	int output_rate, int domain)
{
	int tdm_asrc_id;

	for (tdm_asrc_id = MT8365_TDM_ASRC1;
			tdm_asrc_id < MT8365_TDM_ASRC_NUM; tdm_asrc_id++)
		mt8365_afe_gasrc_autoRstThLo_for_tx(afe,
			tdm_asrc_id, output_rate, domain);
}

static void mt8365_afe_tdm_asrc_autoRstThHi_for_tx(struct mtk_base_afe *afe,
	int output_rate, int domain)
{
	int tdm_asrc_id;

	for (tdm_asrc_id = MT8365_TDM_ASRC1;
			tdm_asrc_id < MT8365_TDM_ASRC_NUM; tdm_asrc_id++)
		mt8365_afe_gasrc_autoRstThLo_for_tx(afe,
			tdm_asrc_id, output_rate, domain);
}

static void mt8365_afe_tdm_asrc_autoRstThLo_for_rx(struct mtk_base_afe *afe,
	int output_rate)
{
	int tdm_asrc_id;

	for (tdm_asrc_id = MT8365_TDM_ASRC1;
			tdm_asrc_id < MT8365_TDM_ASRC_NUM; tdm_asrc_id++)
		mt8365_afe_gasrc_autoRstThLo_for_rx(afe,
			tdm_asrc_id, output_rate);
}

static void mt8365_afe_tdm_asrc_autoRstThHi_for_rx(struct mtk_base_afe *afe,
	int output_rate)
{
	int tdm_asrc_id;

	for (tdm_asrc_id = MT8365_TDM_ASRC1;
			tdm_asrc_id < MT8365_TDM_ASRC_NUM; tdm_asrc_id++)
		mt8365_afe_gasrc_autoRstThHi_for_rx(afe,
			tdm_asrc_id, output_rate);
}

static void mt8365_afe_clear_tdm_asrc(struct mtk_base_afe *afe)
{
	int tdm_asrc_id;

	for (tdm_asrc_id = MT8365_TDM_ASRC1;
			tdm_asrc_id < MT8365_TDM_ASRC_NUM; tdm_asrc_id++)
		mt8365_afe_clear_gasrc(afe, tdm_asrc_id);
}

static void mt8365_afe_enable_tdm_asrc(struct mtk_base_afe *afe)
{
	unsigned int ctrl_reg = 0;

	ctrl_reg = gasrc_ctrl_reg[MT8365_TDM_ASRC1].con0;
	regmap_update_bits(afe->regmap, ctrl_reg,
		GASRC_CON0_ASM_ON, GASRC_CON0_ASM_ON);
}

static void mt8365_afe_disable_tdm_asrc(struct mtk_base_afe *afe)
{
	int tdm_asrc_id;
	unsigned int ctrl_reg = 0;
	struct mt8365_afe_private *afe_priv = afe->platform_priv;

	ctrl_reg = gasrc_ctrl_reg[MT8365_TDM_ASRC1].con0;
	regmap_update_bits(afe->regmap, ctrl_reg,
		GASRC_CON0_ASM_ON, 0x0);

	for (tdm_asrc_id = MT8365_TDM_ASRC1;
			tdm_asrc_id < MT8365_TDM_ASRC_NUM; tdm_asrc_id++) {
		if (afe_priv->gasrc_data[tdm_asrc_id].iir_on)
			mt8365_afe_gasrc_disable_iir(afe, tdm_asrc_id);
	}
}

static int mt8365_afe_configure_tdm_asrc(struct mtk_base_afe *afe,
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	struct snd_pcm_runtime * const runtime = substream->runtime;
	unsigned int stream = substream->stream;
	const int gasrc_id = mt8365_dai_num_to_gasrc(dai->id);
	int input_fs = 0, output_fs = 0;
	int input_rate = 0, output_rate = 0;
	unsigned int infs_48k_domain = 0;
	unsigned int outfs_48k_domain = 0;
	bool duplex, tdm_asrc_out_cm2;
	struct snd_soc_pcm_runtime *be = substream->private_data;
	struct snd_pcm_substream *paired_substream = NULL;

	if (gasrc_id < 0) {
		dev_info(afe->dev, "%s Invaild gasrc_id return %d !!!\n",
			__func__, gasrc_id);
		return 0;
	}

	duplex = afe_priv->gasrc_data[gasrc_id].duplex;
	tdm_asrc_out_cm2 = afe_priv->gasrc_data[gasrc_id].tdm_asrc_out_cm2;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		input_fs = mt8365_afe_fs_timing(runtime->rate);
		input_rate = runtime->rate;

		if (duplex) {
			paired_substream = snd_soc_dpcm_get_substream(be,
				SNDRV_PCM_STREAM_CAPTURE);
			output_fs =
				mt8365_afe_fs_timing(
					paired_substream->runtime->rate);
			output_rate = paired_substream->runtime->rate;
		} else {
			output_fs = mt8365_afe_fs_timing(runtime->rate);
			output_rate = runtime->rate;
		}
	} else if (stream == SNDRV_PCM_STREAM_CAPTURE) {
		output_fs = mt8365_afe_fs_timing(runtime->rate);
		output_rate = runtime->rate;

		if (duplex) {
			paired_substream =
				snd_soc_dpcm_get_substream(be,
					SNDRV_PCM_STREAM_PLAYBACK);
			input_fs =
				mt8365_afe_fs_timing(
					paired_substream->runtime->rate);
			input_rate = paired_substream->runtime->rate;
		} else {
			input_fs = mt8365_afe_fs_timing(runtime->rate);
			input_rate = runtime->rate;
		}
	}

	dev_dbg(afe->dev, "%s input_fs 0x%x, output_fs 0x%x\n",
		__func__, input_fs, output_fs);
	dev_dbg(afe->dev, "%s input_rate %d, output_rate %d\n",
		__func__, input_rate, output_rate);

	mt8365_afe_tdm_asrc_iir_coeff(afe, input_rate, output_rate);

	infs_48k_domain = mt8365_afe_clk_group_48k(input_rate);
	outfs_48k_domain = mt8365_afe_clk_group_48k(output_rate);

	if (tdm_asrc_out_cm2 && (infs_48k_domain != outfs_48k_domain))
		afe_priv->gasrc_data[gasrc_id].cali_on = true;

	dev_dbg(afe->dev, "%s tx_mode %d, cali_on %d\n",
		__func__, afe_priv->gasrc_data[gasrc_id].tx_mode,
		afe_priv->gasrc_data[gasrc_id].cali_on);

	if (afe_priv->gasrc_data[gasrc_id].tx_mode) {
		mt8365_afe_tdm_asrc_tx_frequency_palette(afe,
			input_rate, output_rate, infs_48k_domain);
		if (afe_priv->gasrc_data[gasrc_id].cali_on) {
			mt8365_afe_tdm_asrc_setup_cali_tx(afe,
				infs_48k_domain,
				tdm_asrc_out_cm2);
			mt8365_afe_tdm_asrc_enable_cali(afe);
		} else {
			mt8365_afe_tdm_asrc_disable_cali(afe);
		}
	} else {
		mt8365_afe_tdm_asrc_rx_frequency_palette(afe,
			input_rate, output_rate);
		if (afe_priv->gasrc_data[gasrc_id].cali_on) {
			mt8365_afe_tdm_asrc_setup_cali_rx(afe,
				tdm_asrc_out_cm2);
			mt8365_afe_tdm_asrc_enable_cali(afe);
		} else {
			mt8365_afe_tdm_asrc_disable_cali(afe);
		}
	}

	mt8365_afe_tdm_asrc_fixed_denominator(afe);

	if (afe_priv->gasrc_data[gasrc_id].tx_mode) {
		mt8365_afe_tdm_asrc_autoRstThHi_for_tx(afe,
			output_rate, infs_48k_domain);
		mt8365_afe_tdm_asrc_autoRstThLo_for_tx(afe,
			output_rate, infs_48k_domain);
	} else {
		mt8365_afe_tdm_asrc_autoRstThHi_for_rx(afe,
			input_rate);
		mt8365_afe_tdm_asrc_autoRstThLo_for_rx(afe,
			input_rate);
	}

	mt8365_afe_tdm_asrc_set(afe, dai, output_fs);
	mt8365_afe_tdm_asrc_one_heart_mode(afe, dai);

	return 0;
}

static int mt8365_afe_gasrc_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	const int gasrc_id = mt8365_dai_num_to_gasrc(dai->id);

	mt8365_afe_enable_main_clk(afe);

	switch (gasrc_id) {
	case MT8365_GASRC1:
		mt8365_afe_enable_top_cg(afe, MT8365_TOP_CG_GENERAL1_ASRC);
		break;
	case MT8365_GASRC2:
		mt8365_afe_enable_top_cg(afe, MT8365_TOP_CG_GENERAL2_ASRC);
		break;
	default:
		break;
	}

	return 0;
}

static void mt8365_afe_gasrc_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	const int gasrc_id = mt8365_dai_num_to_gasrc(dai->id);

	switch (gasrc_id) {
	case MT8365_GASRC1:
		mt8365_afe_disable_top_cg(afe, MT8365_TOP_CG_GENERAL1_ASRC);
		break;
	case MT8365_GASRC2:
		mt8365_afe_disable_top_cg(afe, MT8365_TOP_CG_GENERAL2_ASRC);
		break;
	default:
		break;
	}

	mt8365_afe_disable_main_clk(afe);
}

static int mt8365_afe_gasrc_prepare(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	const int gasrc_id = mt8365_dai_num_to_gasrc(dai->id);

	if (gasrc_id < 0) {
		dev_info(afe->dev, "%s Invaild gasrc_id return %d !!!\n",
			__func__, gasrc_id);
		return 0;
	}

	afe_priv->gasrc_data[gasrc_id].duplex = false;

	if (dai->stream_active[SNDRV_PCM_STREAM_CAPTURE] && dai->stream_active[SNDRV_PCM_STREAM_PLAYBACK])
		afe_priv->gasrc_data[gasrc_id].duplex = true;

	dev_dbg(afe->dev, "%s duplex %d\n", __func__,
		afe_priv->gasrc_data[gasrc_id].duplex);

	if (afe_priv->gasrc_data[gasrc_id].duplex)
		mt8365_afe_disable_gasrc(afe, gasrc_id);

	mt8365_afe_configure_gasrc(afe, substream, dai);
	mt8365_afe_clear_gasrc(afe, gasrc_id);

	return 0;
}

static int mt8365_afe_gasrc_trigger(struct snd_pcm_substream *substream,
	int cmd, struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	const int gasrc_id = mt8365_dai_num_to_gasrc(dai->id);
	int ret = 0;

	if (gasrc_id < 0) {
		dev_info(afe->dev, "%s Invaild gasrc_id return %d !!!\n",
			__func__, gasrc_id);
		return ret;
	}

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
			fallthrough;
	case SNDRV_PCM_TRIGGER_RESUME:
		ret = mt8365_afe_enable_gasrc(afe, gasrc_id);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
			fallthrough;
	case SNDRV_PCM_TRIGGER_SUSPEND:
		ret = mt8365_afe_disable_gasrc(afe, gasrc_id);
		break;
	default:
		break;
	}

	return ret;
}

static int mt8365_afe_tdm_asrc_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);

	mt8365_afe_enable_main_clk(afe);
	mt8365_afe_enable_top_cg(afe, MT8365_TOP_CG_TDM_ASRC);

	return 0;
}

static void mt8365_afe_tdm_asrc_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);

	mt8365_afe_disable_top_cg(afe, MT8365_TOP_CG_TDM_ASRC);
	mt8365_afe_disable_main_clk(afe);
}

static int mt8365_afe_tdm_asrc_prepare(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	const int gasrc_id = mt8365_dai_num_to_gasrc(dai->id);

	if (gasrc_id < 0) {
		dev_info(afe->dev, "%s Invaild gasrc_id return %d !!!\n",
			__func__, gasrc_id);
		return 0;
	}

	afe_priv->gasrc_data[gasrc_id].duplex = false;

	if (dai->stream_active[SNDRV_PCM_STREAM_CAPTURE] && dai->stream_active[SNDRV_PCM_STREAM_PLAYBACK])
		afe_priv->gasrc_data[gasrc_id].duplex = true;

	dev_dbg(afe->dev, "%s duplex %d\n", __func__,
		afe_priv->gasrc_data[gasrc_id].duplex);

	if (afe_priv->gasrc_data[gasrc_id].duplex)
		mt8365_afe_disable_tdm_asrc(afe);

	mt8365_afe_configure_tdm_asrc(afe, substream, dai);
	mt8365_afe_clear_tdm_asrc(afe);

	return 0;
}

static int mt8365_afe_tdm_asrc_trigger(struct snd_pcm_substream *substream,
	int cmd, struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
			fallthrough;
	case SNDRV_PCM_TRIGGER_RESUME:
		mt8365_afe_enable_tdm_asrc(afe);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
			fallthrough;
	case SNDRV_PCM_TRIGGER_SUSPEND:
		mt8365_afe_disable_tdm_asrc(afe);
		break;
	default:
		break;
	}

	return 0;
}

static int mt8365_afe_hw_gain1_startup(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);

	mt8365_afe_enable_main_clk(afe);
	return 0;
}

static void mt8365_afe_hw_gain1_shutdown(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	struct mt8365_be_dai_data *be =
		&afe_priv->be_data[dai->id - MT8365_AFE_BACKEND_BASE];

	const unsigned int stream = substream->stream;

	if (be->prepared[stream]) {
		regmap_update_bits(afe->regmap, AFE_GAIN1_CON0,
			AFE_GAIN1_CON0_EN_MASK, 0);
		be->prepared[stream] = false;
	}
	mt8365_afe_disable_main_clk(afe);
}

static int mt8365_afe_hw_gain1_prepare(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct snd_pcm_runtime * const runtime = substream->runtime;
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	struct mt8365_be_dai_data *be =
		&afe_priv->be_data[dai->id - MT8365_AFE_BACKEND_BASE];

	const unsigned int rate = runtime->rate;
	const unsigned int stream = substream->stream;
	int fs;
	unsigned int val1, val2;

	if (be->prepared[stream]) {
		dev_info(afe->dev, "%s prepared already\n", __func__);
		return 0;
	}

	fs = mt8365_afe_fs_timing(rate);
	regmap_update_bits(afe->regmap, AFE_GAIN1_CON0,
		AFE_GAIN1_CON0_MODE_MASK, (unsigned int)fs<<4);

	regmap_read(afe->regmap, AFE_GAIN1_CON1, &val1);
	regmap_read(afe->regmap, AFE_GAIN1_CUR, &val2);
	if ((val1 & AFE_GAIN1_CON1_MASK) != (val2 & AFE_GAIN1_CUR_MASK))
		regmap_update_bits(afe->regmap, AFE_GAIN1_CUR,
		AFE_GAIN1_CUR_MASK, val1);

	regmap_update_bits(afe->regmap, AFE_GAIN1_CON0,
		AFE_GAIN1_CON0_EN_MASK, 1);
	be->prepared[stream] = true;

	return 0;
}

static const struct snd_pcm_hardware mt8365_hostless_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_MMAP_VALID),
	.period_bytes_min = 256,
	.period_bytes_max = 4 * 48 * 1024,
	.periods_min = 2,
	.periods_max = 256,
	.buffer_bytes_max = 8 * 48 * 1024,
	.fifo_size = 0,
};

/* dai ops */
static int mtk_dai_hostless_startup(struct snd_pcm_substream *substream,
				    struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret;

	snd_soc_set_runtime_hwparams(substream, &mt8365_hostless_hardware);

	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		dev_info(afe->dev, "snd_pcm_hw_constraint_integer failed\n");
	return ret;
}

/* FE DAIs */
static const struct snd_soc_dai_ops mt8365_afe_fe_dai_ops = {
	.startup	= mt8365_afe_fe_startup,
	.shutdown	= mt8365_afe_fe_shutdown,
	.hw_params	= mt8365_afe_fe_hw_params,
	.hw_free	= mt8365_afe_fe_hw_free,
	.prepare	= mt8365_afe_fe_prepare,
	.trigger	= mt8365_afe_fe_trigger,
};

static const struct snd_soc_dai_ops mt8365_dai_hostless_ops = {
	.startup = mtk_dai_hostless_startup,
};

/* BE DAIs */
static const struct snd_soc_dai_ops mt8365_afe_i2s_ops = {
	.startup	= mt8365_afe_i2s_startup,
	.shutdown	= mt8365_afe_i2s_shutdown,
	.prepare	= mt8365_afe_i2s_prepare,
};

static const struct snd_soc_dai_ops mt8365_afe_2nd_i2s_ops = {
	.startup	= mt8365_afe_2nd_i2s_startup,
	.shutdown	= mt8365_afe_2nd_i2s_shutdown,
	.hw_params	= mt8365_afe_2nd_i2s_hw_params,
	.prepare	= mt8365_afe_2nd_i2s_prepare,
	.set_fmt	= mt8365_afe_2nd_i2s_set_fmt,
};

static const struct snd_soc_dai_ops mt8365_afe_tdm_out_ops = {
	.startup	= mt8365_afe_tdm_out_startup,
	.shutdown	= mt8365_afe_tdm_out_shutdown,
	.prepare	= mt8365_afe_tdm_out_prepare,
	.trigger	= mt8365_afe_tdm_out_trigger,
	.set_fmt	= mt8365_afe_tdm_out_set_fmt,
};

static const struct snd_soc_dai_ops mt8365_afe_tdm_in_ops = {
	.startup	= mt8365_afe_tdm_in_startup,
	.shutdown	= mt8365_afe_tdm_in_shutdown,
	.prepare	= mt8365_afe_tdm_in_prepare,
	.trigger	= mt8365_afe_tdm_in_trigger,
	.set_fmt	= mt8365_afe_tdm_in_set_fmt,
};

static const struct snd_soc_dai_ops mt8365_afe_pcm1_ops = {
	.startup	= mt8365_afe_pcm1_startup,
	.shutdown	= mt8365_afe_pcm1_shutdown,
	.prepare	= mt8365_afe_pcm1_prepare,
	.set_fmt	= mt8365_afe_pcm1_set_fmt,
};

static const struct snd_soc_dai_ops mt8365_afe_dmic_ops = {
	.startup	= mt8365_afe_dmic_startup,
	.shutdown	= mt8365_afe_dmic_shutdown,
	.prepare	= mt8365_afe_dmic_prepare,
};

static const struct snd_soc_dai_ops mt8365_afe_int_adda_ops = {
	.startup	= mt8365_afe_int_adda_startup,
	.shutdown	= mt8365_afe_int_adda_shutdown,
	.prepare	= mt8365_afe_int_adda_prepare,
};

static const struct snd_soc_dai_ops mt8365_afe_gasrc_ops = {
	.startup	= mt8365_afe_gasrc_startup,
	.shutdown	= mt8365_afe_gasrc_shutdown,
	.prepare	= mt8365_afe_gasrc_prepare,
	.trigger	= mt8365_afe_gasrc_trigger,
};

static const struct snd_soc_dai_ops mt8365_afe_tdm_asrc_ops = {
	.startup	= mt8365_afe_tdm_asrc_startup,
	.shutdown	= mt8365_afe_tdm_asrc_shutdown,
	.prepare	= mt8365_afe_tdm_asrc_prepare,
	.trigger	= mt8365_afe_tdm_asrc_trigger,
};

static const struct snd_soc_dai_ops mt8365_afe_hw_gain1_ops = {
	.startup	= mt8365_afe_hw_gain1_startup,
	.shutdown	= mt8365_afe_hw_gain1_shutdown,
	.prepare	= mt8365_afe_hw_gain1_prepare,
};

static struct snd_soc_dai_driver mt8365_afe_pcm_dais[] = {
	/* FE DAIs: memory intefaces to CPU */
	{
		.name = "DL1",
		.id = MT8365_AFE_MEMIF_DL1,
		.playback = {
			.stream_name = "DL1",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8365_afe_fe_dai_ops,
	}, {
		.name = "DL2",
		.id = MT8365_AFE_MEMIF_DL2,
		.playback = {
			.stream_name = "DL2",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8365_afe_fe_dai_ops,
	}, {
		.name = "TDM_OUT",
		.id = MT8365_AFE_MEMIF_TDM_OUT,
		.playback = {
			.stream_name = "TDM_OUT",
			.channels_min = 1,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8365_afe_fe_dai_ops,
	}, {
		.name = "AWB",
		.id = MT8365_AFE_MEMIF_AWB,
		.capture = {
			.stream_name = "AWB",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8365_afe_fe_dai_ops,
	}, {
		.name = "VUL",
		.id = MT8365_AFE_MEMIF_VUL,
		.capture = {
			.stream_name = "VUL",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8365_afe_fe_dai_ops,
	}, {
		.name = "VUL2",
		.id = MT8365_AFE_MEMIF_VUL2,
		.capture = {
			.stream_name = "VUL2",
			.channels_min = 1,
			.channels_max = 16,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8365_afe_fe_dai_ops,
	}, {
		.name = "VUL3",
		.id = MT8365_AFE_MEMIF_VUL3,
		.capture = {
			.stream_name = "VUL3",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8365_afe_fe_dai_ops,
	}, {
		.name = "TDM_IN",
		.id = MT8365_AFE_MEMIF_TDM_IN,
		.capture = {
			.stream_name = "TDM_IN",
			.channels_min = 1,
			.channels_max = 16,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8365_afe_fe_dai_ops,
	}, {
	.name = "Hostless FM DAI",
	.id = MT8365_AFE_IO_VIRTUAL_FM,
	.playback = {
		.stream_name = "Hostless FM DL",
		.channels_min = 1,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_192000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE |
				  SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
	},
	.capture = {
		.stream_name = "Hostless FM UL",
		.channels_min = 1,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_192000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE |
				  SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
	},
	.ops = &mt8365_dai_hostless_ops,
	}, {
	/* BE DAIs */
		.name = "I2S",
		.id = MT8365_AFE_IO_I2S,
		.playback = {
			.stream_name = "I2S Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.capture = {
			.stream_name = "I2S Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8365_afe_i2s_ops,
	}, {
		.name = "2ND I2S",
		.id = MT8365_AFE_IO_2ND_I2S,
		.playback = {
			.stream_name = "2ND I2S Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.capture = {
			.stream_name = "2ND I2S Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8365_afe_2nd_i2s_ops,
	}, {
	/* BE DAIs */
		.name = "TDM_OUT_IO",
		.id = MT8365_AFE_IO_TDM_OUT,
		.playback = {
			.stream_name = "TDM_OUT Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8365_afe_tdm_out_ops,
	}, {
	/* BE DAIs */
		.name = "TDM_IN_IO",
		.id = MT8365_AFE_IO_TDM_IN,
		.capture = {
			.stream_name = "TDM_IN Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8365_afe_tdm_in_ops,
	}, {
	/* BE DAIs */
		.name = "PCM1",
		.id = MT8365_AFE_IO_PCM1,
		.playback = {
			.stream_name = "PCM1 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000 |
				 SNDRV_PCM_RATE_16000 |
				 SNDRV_PCM_RATE_32000 |
				 SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.capture = {
			.stream_name = "PCM1 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000 |
				 SNDRV_PCM_RATE_16000 |
				 SNDRV_PCM_RATE_32000 |
				 SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8365_afe_pcm1_ops,
		.symmetric_rate = 1,
		.symmetric_sample_bits = 1,
	}, {
	/* BE DAIs */
		.name = "VIRTUAL_DL_SRC",
		.id = MT8365_AFE_IO_VIRTUAL_DL_SRC,
		.capture = {
			.stream_name = "VIRTUAL_DL_SRC Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
	}, {
	/* BE DAIs */
		.name = "VIRTUAL_TDM_OUT_SRC",
		.id = MT8365_AFE_IO_VIRTUAL_TDM_OUT_SRC,
		.capture = {
			.stream_name = "TDM_OUT Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
	}, {
	/* BE DAIs */
		.name = "DMIC",
		.id = MT8365_AFE_IO_DMIC,
		.capture = {
			.stream_name = "DMIC Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_8000 |
				 SNDRV_PCM_RATE_16000 |
				 SNDRV_PCM_RATE_32000 |
				 SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8365_afe_dmic_ops,
	}, {
	/* BE DAIs */
		.name = "INT ADDA",
		.id = MT8365_AFE_IO_INT_ADDA,
		.playback = {
			.stream_name = "INT ADDA Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.capture = {
			.stream_name = "INT ADDA Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000 |
				 SNDRV_PCM_RATE_16000 |
				 SNDRV_PCM_RATE_32000 |
				 SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8365_afe_int_adda_ops,
	}, {
	/* BE DAIs */
		.name = "GASRC1",
		.id = MT8365_AFE_IO_GASRC1,
		.playback = {
			.stream_name = "GASRC1 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.capture = {
			.stream_name = "GASRC1 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8365_afe_gasrc_ops,
	}, {
	/* BE DAIs */
		.name = "GASRC2",
		.id = MT8365_AFE_IO_GASRC2,
		.playback = {
			.stream_name = "GASRC2 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.capture = {
			.stream_name = "GASRC2 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8365_afe_gasrc_ops,
	}, {
	/* BE DAIs */
		.name = "TDM_ASRC",
		.id = MT8365_AFE_IO_TDM_ASRC,
		.playback = {
			.stream_name = "TDM_ASRC Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.capture = {
			.stream_name = "TDM_ASRC Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8365_afe_tdm_asrc_ops,
	}, {
	/* BE DAIs */
		.name = "HW_GAIN1",
		.id = MT8365_AFE_IO_HW_GAIN1,
		.playback = {
			.stream_name = "HW Gain 1 In",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.capture = {
			.stream_name = "HW Gain 1 Out",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8365_afe_hw_gain1_ops,
		.symmetric_rate = 1,
		.symmetric_channels = 1,
		.symmetric_sample_bits = 1,
	},
};

static const struct snd_kcontrol_new mt8365_afe_o00_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I05 Switch", AFE_CONN0, 5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I07 Switch", AFE_CONN0, 7, 1, 0),
};

static const struct snd_kcontrol_new mt8365_afe_o01_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I06 Switch", AFE_CONN1, 6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I08 Switch", AFE_CONN1, 8, 1, 0),
};

static const struct snd_kcontrol_new mt8365_afe_o03_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I05 Switch", AFE_CONN3, 5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I07 Switch", AFE_CONN3, 7, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I00 Switch", AFE_CONN3, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I10 Switch", AFE_CONN3, 10, 1, 0),
};

static const struct snd_kcontrol_new mt8365_afe_o04_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I06 Switch", AFE_CONN4, 6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I08 Switch", AFE_CONN4, 8, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I01 Switch", AFE_CONN4, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I11 Switch", AFE_CONN4, 11, 1, 0),
};

static const struct snd_kcontrol_new mt8365_afe_o05_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I00 Switch", AFE_CONN5, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I03 Switch", AFE_CONN5, 3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I05 Switch", AFE_CONN5, 5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I07 Switch", AFE_CONN5, 7, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I09 Switch", AFE_CONN5, 9, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I14 Switch", AFE_CONN5, 14, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I16 Switch", AFE_CONN5, 16, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I18 Switch", AFE_CONN5, 18, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I20 Switch", AFE_CONN5, 20, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I23 Switch", AFE_CONN5, 23, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I10L Switch", AFE_CONN5, 10, 1, 0),
};

static const struct snd_kcontrol_new mt8365_afe_o06_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I01 Switch", AFE_CONN6, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I04 Switch", AFE_CONN6, 4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I06 Switch", AFE_CONN6, 6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I08 Switch", AFE_CONN6, 8, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I22 Switch", AFE_CONN6, 22, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I15 Switch", AFE_CONN6, 15, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I17 Switch", AFE_CONN6, 17, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I19 Switch", AFE_CONN6, 19, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I21 Switch", AFE_CONN6, 21, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I24 Switch", AFE_CONN6, 24, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I11L Switch", AFE_CONN6, 11, 1, 0),
};

static const struct snd_kcontrol_new mt8365_afe_o07_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I05 Switch", AFE_CONN7, 5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I07 Switch", AFE_CONN7, 7, 1, 0),
};

static const struct snd_kcontrol_new mt8365_afe_o08_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I06 Switch", AFE_CONN8, 6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I08 Switch", AFE_CONN8, 8, 1, 0),
};

static const struct snd_kcontrol_new mt8365_afe_o09_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I00 Switch", AFE_CONN9, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I03 Switch", AFE_CONN9, 3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I09 Switch", AFE_CONN9, 9, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I14 Switch", AFE_CONN9, 14, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I16 Switch", AFE_CONN9, 16, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I18 Switch", AFE_CONN9, 18, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I20 Switch", AFE_CONN9, 20, 1, 0),
};

static const struct snd_kcontrol_new mt8365_afe_o10_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I01 Switch", AFE_CONN10, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I04 Switch", AFE_CONN10, 4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I22 Switch", AFE_CONN10, 22, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I15 Switch", AFE_CONN10, 15, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I17 Switch", AFE_CONN10, 17, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I19 Switch", AFE_CONN10, 19, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I21 Switch", AFE_CONN10, 21, 1, 0),
};

static const struct snd_kcontrol_new mt8365_afe_o11_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I00 Switch", AFE_CONN11, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I03 Switch", AFE_CONN11, 3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I09 Switch", AFE_CONN11, 9, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I14 Switch", AFE_CONN11, 14, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I16 Switch", AFE_CONN11, 16, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I18 Switch", AFE_CONN11, 18, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I20 Switch", AFE_CONN11, 20, 1, 0),
};

static const struct snd_kcontrol_new mt8365_afe_o12_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I01 Switch", AFE_CONN12, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I04 Switch", AFE_CONN12, 4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I22 Switch", AFE_CONN12, 22, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I15 Switch", AFE_CONN12, 15, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I17 Switch", AFE_CONN12, 17, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I19 Switch", AFE_CONN12, 19, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I21 Switch", AFE_CONN12, 21, 1, 0),
};

static const struct snd_kcontrol_new mt8365_afe_o13_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I00 Switch", AFE_CONN13, 0, 1, 0),
};

static const struct snd_kcontrol_new mt8365_afe_o14_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I01 Switch", AFE_CONN14, 1, 1, 0),
};

static const struct snd_kcontrol_new mt8365_afe_o15_mix[] = {
};

static const struct snd_kcontrol_new mt8365_afe_o16_mix[] = {
};

static const struct snd_kcontrol_new mt8365_afe_o17_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I03 Switch", AFE_CONN17, 3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I14 Switch", AFE_CONN17, 14, 1, 0),
};

static const struct snd_kcontrol_new mt8365_afe_o18_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I04 Switch", AFE_CONN18, 4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I15 Switch", AFE_CONN18, 15, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I23 Switch", AFE_CONN18, 23, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I25 Switch", AFE_CONN18, 25, 1, 0),
};

static const struct snd_kcontrol_new mt8365_afe_o19_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I04 Switch", AFE_CONN19, 4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I16 Switch", AFE_CONN19, 16, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I23 Switch", AFE_CONN19, 23, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I24 Switch", AFE_CONN19, 24, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I25 Switch", AFE_CONN19, 25, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I26 Switch", AFE_CONN19, 26, 1, 0),
};

static const struct snd_kcontrol_new mt8365_afe_o20_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I17 Switch", AFE_CONN20, 17, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I24 Switch", AFE_CONN20, 24, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I26 Switch", AFE_CONN20, 26, 1, 0),
};

static const struct snd_kcontrol_new mt8365_afe_o21_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I18 Switch", AFE_CONN21, 18, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I23 Switch", AFE_CONN21, 23, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I25 Switch", AFE_CONN21, 25, 1, 0),
};

static const struct snd_kcontrol_new mt8365_afe_o22_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I19 Switch", AFE_CONN22, 19, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I24 Switch", AFE_CONN22, 24, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I26 Switch", AFE_CONN22, 26, 1, 0),
};

static const struct snd_kcontrol_new mt8365_afe_o23_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I20 Switch", AFE_CONN23, 20, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I23 Switch", AFE_CONN23, 23, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I25 Switch", AFE_CONN23, 25, 1, 0),
};

static const struct snd_kcontrol_new mt8365_afe_o24_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I21 Switch", AFE_CONN24, 21, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I24 Switch", AFE_CONN24, 24, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I26 Switch", AFE_CONN24, 26, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I23 Switch", AFE_CONN24, 23, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I25 Switch", AFE_CONN24, 25, 1, 0),
};

static const struct snd_kcontrol_new mt8365_afe_o25_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I27 Switch", AFE_CONN25, 27, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I23 Switch", AFE_CONN25, 23, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I25 Switch", AFE_CONN25, 25, 1, 0),
};

static const struct snd_kcontrol_new mt8365_afe_o26_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I28 Switch", AFE_CONN26, 28, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I24 Switch", AFE_CONN26, 24, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I26 Switch", AFE_CONN26, 26, 1, 0),
};

static const struct snd_kcontrol_new mt8365_afe_o27_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I05 Switch", AFE_CONN27, 5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I07 Switch", AFE_CONN27, 7, 1, 0),
};

static const struct snd_kcontrol_new mt8365_afe_o28_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I06 Switch", AFE_CONN28, 6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I08 Switch", AFE_CONN28, 8, 1, 0),
};

static const struct snd_kcontrol_new mt8365_afe_o29_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I05 Switch", AFE_CONN29, 5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I07 Switch", AFE_CONN29, 7, 1, 0),
};

static const struct snd_kcontrol_new mt8365_afe_o30_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I06 Switch", AFE_CONN30, 6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I08 Switch", AFE_CONN30, 8, 1, 0),
};

static const struct snd_kcontrol_new mt8365_afe_o31_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I29 Switch", AFE_CONN31, 29, 1, 0),
};

static const struct snd_kcontrol_new mt8365_afe_o32_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I30 Switch", AFE_CONN32, 30, 1, 0),
};

static const struct snd_kcontrol_new mt8365_afe_o33_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I31 Switch", AFE_CONN33, 31, 1, 0),
};

static const struct snd_kcontrol_new mt8365_afe_o34_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I32 Switch", AFE_CONN34_1, 0, 1, 0),
};

static const struct snd_kcontrol_new mt8365_afe_o35_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I33 Switch", AFE_CONN35_1, 1, 1, 0),
};

static const struct snd_kcontrol_new mt8365_afe_o36_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I34 Switch", AFE_CONN36_1, 2, 1, 0),
};

static const struct snd_kcontrol_new mtk_hw_gain1_in_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("CONNSYS_I2S_CH1", AFE_CONN13,
				    0, 1, 0),
};

static const struct snd_kcontrol_new mtk_hw_gain1_in_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("CONNSYS_I2S_CH2", AFE_CONN14,
				    1, 1, 0),
};

static const struct snd_kcontrol_new mtk_adda_dl_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("GAIN1_OUT_CH1", AFE_CONN3,
				    10, 1, 0),
};

static const struct snd_kcontrol_new mtk_adda_dl_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("GAIN1_OUT_CH2", AFE_CONN4,
				    11, 1, 0),
};

static int mt8365_afe_cm2_io_input_mux_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = mCM2Input;

	return 0;
}

static int mt8365_afe_cm2_io_input_mux_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_context *dapm =
		snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct snd_soc_component *comp = snd_soc_dapm_to_component(dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(comp);
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	int ret;

	mCM2Input = ucontrol->value.enumerated.item[0];

	afe_priv->cm2_mux_input = mCM2Input;
	ret = snd_soc_dapm_put_enum_double(kcontrol, ucontrol);

	return ret;
}

static int mt8365_afe_gasrc1_output_mux_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = mGasrc1Out;

	return 0;
}

static int mt8365_afe_gasrc1_output_mux_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_context *dapm =
		snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct snd_soc_component *comp = snd_soc_dapm_to_component(dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(comp);
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int ret;

	if (ucontrol->value.enumerated.item[0] >= e->items)
		return -EINVAL;

	mGasrc1Out = ucontrol->value.enumerated.item[0];
	ret = snd_soc_dapm_put_enum_double(kcontrol, ucontrol);

	/* 0:OPEN 1:CM1 2:CM2 */
	if (mGasrc1Out == 1) {
		afe_priv->gasrc_data[MT8365_GASRC1].tx_mode = false;
		afe_priv->gasrc_data[MT8365_GASRC1].cali_on = false;
		regmap_update_bits(afe->regmap, AFE_CM2_CON0,
			CM_AFE_CM2_GASRC1_OUT_SEL,
			GASRC1_OUT_TO_ENGEN_OUT_EN);
	} else if (mGasrc1Out == 2) {
		afe_priv->gasrc_data[MT8365_GASRC1].tx_mode = true;
		afe_priv->gasrc_data[MT8365_GASRC1].cali_on = true;
		regmap_update_bits(afe->regmap, AFE_CM2_CON0,
			CM_AFE_CM2_GASRC1_OUT_SEL,
			GASRC1_OUT_TO_TDM_OUT_EN);
	}

	return ret;
}

static int mt8365_afe_gasrc2_output_mux_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = mGasrc2Out;

	return 0;
}

static int mt8365_afe_gasrc2_output_mux_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_context *dapm =
		snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct snd_soc_component *comp = snd_soc_dapm_to_component(dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(comp);
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int ret;

	if (ucontrol->value.enumerated.item[0] >= e->items)
		return -EINVAL;

	mGasrc2Out = ucontrol->value.enumerated.item[0];
	ret = snd_soc_dapm_put_enum_double(kcontrol, ucontrol);

	/* 0:OPEN 1:CM1 2:CM2 */
	if (mGasrc2Out == 1) {
		afe_priv->gasrc_data[MT8365_GASRC2].tx_mode = false;
		afe_priv->gasrc_data[MT8365_GASRC2].cali_on = false;
		regmap_update_bits(afe->regmap, AFE_CM2_CON0,
			CM_AFE_CM2_GASRC2_OUT_SEL,
			GASRC2_OUT_TO_ENGEN_OUT_EN);
	} else if (mGasrc2Out == 2) {
		afe_priv->gasrc_data[MT8365_GASRC2].tx_mode = true;
		afe_priv->gasrc_data[MT8365_GASRC2].cali_on = true;
		regmap_update_bits(afe->regmap, AFE_CM2_CON0,
			CM_AFE_CM2_GASRC2_OUT_SEL,
			GASRC2_OUT_TO_TDM_OUT_EN);
	}

	return ret;
}

static int mt8365_afe_tdm_asrc_output_mux_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = mTdmGasrcOut;

	return 0;
}

static int mt8365_afe_tdm_asrc_output_mux_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_context *dapm =
		snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct snd_soc_component *comp = snd_soc_dapm_to_component(dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(comp);
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int ret;

	if (ucontrol->value.enumerated.item[0] >= e->items)
		return -EINVAL;

	mTdmGasrcOut = ucontrol->value.enumerated.item[0];
	ret = snd_soc_dapm_put_enum_double(kcontrol, ucontrol);

	/* 0:OPEN 1:CM1 2:CM2 */
	if (mTdmGasrcOut == 1) {
		afe_priv->gasrc_data[MT8365_TDM_ASRC1].tx_mode = false;
		afe_priv->gasrc_data[MT8365_TDM_ASRC1].cali_on = true;
		afe_priv->gasrc_data[MT8365_TDM_ASRC1].tdm_asrc_out_cm2 = false;
		regmap_update_bits(afe->regmap, AFE_TDM_ASRC_CON0,
			TDM_ASRC_OUT_SEL,
			TDM_ASRC_OUT_SEL_TO_CM1);
	} else if (mTdmGasrcOut == 2) {
		afe_priv->gasrc_data[MT8365_TDM_ASRC1].tx_mode = true;
		afe_priv->gasrc_data[MT8365_TDM_ASRC1].cali_on = false;
		afe_priv->gasrc_data[MT8365_TDM_ASRC1].tdm_asrc_out_cm2 = true;
		regmap_update_bits(afe->regmap, AFE_TDM_ASRC_CON0,
			TDM_ASRC_OUT_SEL,
			TDM_ASRC_OUT_SEL_TO_CM2);
	}

	return ret;
}

static const char * const fmi2sin_text[] = {
	"OPEN", "FM_2ND_I2S_IN"
};

static const char * const fmhwgain_text[] = {
	"OPEN", "FM_HW_GAIN_IO"
};
static const char * const ain_text[] = {
	"INT ADC", "EXT ADC",
};
static const char * const tdm_in_input_text[] = {
	"TDM_IN_FROM_IO", "TDM_IN_FROM_CM2",
};
static const char * const vul2_in_input_text[] = {
	"VUL2_IN_FROM_O17O18", "VUL2_IN_FROM_CM1",
};

static const char * const gasrc1_out_text[] = {
	"OPEN", "CM1_GASRC1", "CM2_GASRC1",
};

static const char * const gasrc2_out_text[] = {
	"OPEN", "CM1_GASRC2", "CM2_GASRC2",
};

static const char * const tdm_asrc_out_text[] = {
	"OPEN", "CM1_TDM_ASRC", "CM2_TDM_ASRC",
};

static const char * const mt8365_afe_cm2_mux_text[] = {
	"OPEN", "FROM_GASRC1_OUT", "FROM_GASRC2_OUT", "FROM_TDM_ASRC_OUT",
};

static SOC_ENUM_SINGLE_VIRT_DECL(fmi2sin_enum, fmi2sin_text);
static SOC_ENUM_SINGLE_VIRT_DECL(fmhwgain_enum, fmhwgain_text);
static SOC_ENUM_SINGLE_DECL(ain_enum, AFE_ADDA_TOP_CON0, 0, ain_text);
static SOC_ENUM_SINGLE_VIRT_DECL(tdm_in_input_enum, tdm_in_input_text);
static SOC_ENUM_SINGLE_VIRT_DECL(vul2_in_input_enum, vul2_in_input_text);
static SOC_ENUM_SINGLE_VIRT_DECL(gasrc1_out_sel_enum,	gasrc1_out_text);
static SOC_ENUM_SINGLE_VIRT_DECL(gasrc2_out_sel_enum, gasrc2_out_text);
static SOC_ENUM_SINGLE_VIRT_DECL(tdm_asrc_out_sel_enum, tdm_asrc_out_text);
static SOC_ENUM_SINGLE_VIRT_DECL(mt8365_afe_cm2_mux_input_enum,
	mt8365_afe_cm2_mux_text);
static const struct snd_kcontrol_new fmi2sin_mux =
	SOC_DAPM_ENUM("FM 2ND I2S Source", fmi2sin_enum);

static const struct snd_kcontrol_new fmhwgain_mux =
	SOC_DAPM_ENUM("FM HW Gain Source", fmhwgain_enum);

static const struct snd_kcontrol_new ain_mux =
	SOC_DAPM_ENUM("AIN Source", ain_enum);

static const struct snd_kcontrol_new tdm_in_input_mux =
	SOC_DAPM_ENUM("TDM_IN Input", tdm_in_input_enum);

static const struct snd_kcontrol_new vul2_in_input_mux =
	SOC_DAPM_ENUM("VUL2 Input", vul2_in_input_enum);

static const struct snd_kcontrol_new mt8365_afe_cm2_mux_input_mux =
	SOC_DAPM_ENUM_EXT("CM2_MUX Source", mt8365_afe_cm2_mux_input_enum,
		mt8365_afe_cm2_io_input_mux_get,
		mt8365_afe_cm2_io_input_mux_put);

static const struct snd_kcontrol_new gasrc1_out_sel_mux =
	SOC_DAPM_ENUM_EXT("GASRC1 Output", gasrc1_out_sel_enum,
	mt8365_afe_gasrc1_output_mux_get,
	mt8365_afe_gasrc1_output_mux_put);

static const struct snd_kcontrol_new gasrc2_out_sel_mux =
	SOC_DAPM_ENUM_EXT("GASRC2 Output", gasrc2_out_sel_enum,
	mt8365_afe_gasrc2_output_mux_get,
	mt8365_afe_gasrc2_output_mux_put);

static const struct snd_kcontrol_new tdm_asrc_out_sel_mux =
	SOC_DAPM_ENUM_EXT("TDM_ASRC Output", tdm_asrc_out_sel_enum,
	mt8365_afe_tdm_asrc_output_mux_get,
	mt8365_afe_tdm_asrc_output_mux_put);

static const struct snd_kcontrol_new i2s_o03_o04_enable_ctl =
	SOC_DAPM_SINGLE_VIRT("Switch", 1);

static const struct snd_kcontrol_new int_adda_o03_o04_enable_ctl =
	SOC_DAPM_SINGLE_VIRT("Switch", 1);

static const struct snd_kcontrol_new tdm_asrc_virtul_playback_enable_ctl =
	SOC_DAPM_SINGLE_VIRT("Switch", 1);

static const struct snd_soc_dapm_widget mt8365_afe_pcm_widgets[] = {
	/* inter-connections */
	SND_SOC_DAPM_MIXER("I00", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I01", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I03", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I04", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I05", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I06", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I07", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I08", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I05L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I06L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I07L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I08L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I09", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I10", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I11", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I10L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I11L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I12", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I13", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I14", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I15", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I16", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I17", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I18", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I19", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I20", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I21", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I22", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I23", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I24", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I25", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I26", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I27", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I28", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I29", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I30", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I31", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I32", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I33", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I34", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("O00", SND_SOC_NOPM, 0, 0,
			   mt8365_afe_o00_mix, ARRAY_SIZE(mt8365_afe_o00_mix)),
	SND_SOC_DAPM_MIXER("O01", SND_SOC_NOPM, 0, 0,
			   mt8365_afe_o01_mix, ARRAY_SIZE(mt8365_afe_o01_mix)),
	SND_SOC_DAPM_MIXER("O03", SND_SOC_NOPM, 0, 0,
			   mt8365_afe_o03_mix, ARRAY_SIZE(mt8365_afe_o03_mix)),
	SND_SOC_DAPM_MIXER("O04", SND_SOC_NOPM, 0, 0,
			   mt8365_afe_o04_mix, ARRAY_SIZE(mt8365_afe_o04_mix)),
	SND_SOC_DAPM_MIXER("O05", SND_SOC_NOPM, 0, 0,
			   mt8365_afe_o05_mix, ARRAY_SIZE(mt8365_afe_o05_mix)),
	SND_SOC_DAPM_MIXER("O06", SND_SOC_NOPM, 0, 0,
			   mt8365_afe_o06_mix, ARRAY_SIZE(mt8365_afe_o06_mix)),
	SND_SOC_DAPM_MIXER("O07", SND_SOC_NOPM, 0, 0,
			   mt8365_afe_o07_mix, ARRAY_SIZE(mt8365_afe_o07_mix)),
	SND_SOC_DAPM_MIXER("O08", SND_SOC_NOPM, 0, 0,
			   mt8365_afe_o08_mix, ARRAY_SIZE(mt8365_afe_o08_mix)),
	SND_SOC_DAPM_MIXER("O09", SND_SOC_NOPM, 0, 0,
			   mt8365_afe_o09_mix, ARRAY_SIZE(mt8365_afe_o09_mix)),
	SND_SOC_DAPM_MIXER("O10", SND_SOC_NOPM, 0, 0,
			   mt8365_afe_o10_mix, ARRAY_SIZE(mt8365_afe_o10_mix)),
	SND_SOC_DAPM_MIXER("O11", SND_SOC_NOPM, 0, 0,
			   mt8365_afe_o11_mix, ARRAY_SIZE(mt8365_afe_o11_mix)),
	SND_SOC_DAPM_MIXER("O12", SND_SOC_NOPM, 0, 0,
			   mt8365_afe_o12_mix, ARRAY_SIZE(mt8365_afe_o12_mix)),
	SND_SOC_DAPM_MIXER("O13", SND_SOC_NOPM, 0, 0,
			   mt8365_afe_o13_mix, ARRAY_SIZE(mt8365_afe_o13_mix)),
	SND_SOC_DAPM_MIXER("O14", SND_SOC_NOPM, 0, 0,
			   mt8365_afe_o14_mix, ARRAY_SIZE(mt8365_afe_o14_mix)),
	SND_SOC_DAPM_MIXER("O15", SND_SOC_NOPM, 0, 0,
			   mt8365_afe_o15_mix, ARRAY_SIZE(mt8365_afe_o15_mix)),
	SND_SOC_DAPM_MIXER("O16", SND_SOC_NOPM, 0, 0,
			   mt8365_afe_o16_mix, ARRAY_SIZE(mt8365_afe_o16_mix)),
	SND_SOC_DAPM_MIXER("O17", SND_SOC_NOPM, 0, 0,
			   mt8365_afe_o17_mix, ARRAY_SIZE(mt8365_afe_o17_mix)),
	SND_SOC_DAPM_MIXER("O18", SND_SOC_NOPM, 0, 0,
			   mt8365_afe_o18_mix, ARRAY_SIZE(mt8365_afe_o18_mix)),
	SND_SOC_DAPM_MIXER("O19", SND_SOC_NOPM, 0, 0,
			   mt8365_afe_o19_mix, ARRAY_SIZE(mt8365_afe_o19_mix)),
	SND_SOC_DAPM_MIXER("O20", SND_SOC_NOPM, 0, 0,
			   mt8365_afe_o20_mix, ARRAY_SIZE(mt8365_afe_o20_mix)),
	SND_SOC_DAPM_MIXER("O21", SND_SOC_NOPM, 0, 0,
			   mt8365_afe_o21_mix, ARRAY_SIZE(mt8365_afe_o21_mix)),
	SND_SOC_DAPM_MIXER("O22", SND_SOC_NOPM, 0, 0,
			   mt8365_afe_o22_mix, ARRAY_SIZE(mt8365_afe_o22_mix)),
	SND_SOC_DAPM_MIXER("O23", SND_SOC_NOPM, 0, 0,
			   mt8365_afe_o23_mix, ARRAY_SIZE(mt8365_afe_o23_mix)),
	SND_SOC_DAPM_MIXER("O24", SND_SOC_NOPM, 0, 0,
			   mt8365_afe_o24_mix, ARRAY_SIZE(mt8365_afe_o24_mix)),
	SND_SOC_DAPM_MIXER("O25", SND_SOC_NOPM, 0, 0,
			   mt8365_afe_o25_mix, ARRAY_SIZE(mt8365_afe_o25_mix)),
	SND_SOC_DAPM_MIXER("O26", SND_SOC_NOPM, 0, 0,
			   mt8365_afe_o26_mix, ARRAY_SIZE(mt8365_afe_o26_mix)),
	SND_SOC_DAPM_MIXER("O27", SND_SOC_NOPM, 0, 0,
			   mt8365_afe_o27_mix, ARRAY_SIZE(mt8365_afe_o27_mix)),
	SND_SOC_DAPM_MIXER("O28", SND_SOC_NOPM, 0, 0,
			   mt8365_afe_o28_mix, ARRAY_SIZE(mt8365_afe_o28_mix)),
	SND_SOC_DAPM_MIXER("O29", SND_SOC_NOPM, 0, 0,
			   mt8365_afe_o29_mix, ARRAY_SIZE(mt8365_afe_o29_mix)),
	SND_SOC_DAPM_MIXER("O30", SND_SOC_NOPM, 0, 0,
			   mt8365_afe_o30_mix, ARRAY_SIZE(mt8365_afe_o30_mix)),
	SND_SOC_DAPM_MIXER("O31", SND_SOC_NOPM, 0, 0,
			   mt8365_afe_o31_mix, ARRAY_SIZE(mt8365_afe_o31_mix)),
	SND_SOC_DAPM_MIXER("O32", SND_SOC_NOPM, 0, 0,
			   mt8365_afe_o32_mix, ARRAY_SIZE(mt8365_afe_o32_mix)),
	SND_SOC_DAPM_MIXER("O33", SND_SOC_NOPM, 0, 0,
			   mt8365_afe_o33_mix, ARRAY_SIZE(mt8365_afe_o33_mix)),
	SND_SOC_DAPM_MIXER("O34", SND_SOC_NOPM, 0, 0,
			   mt8365_afe_o34_mix, ARRAY_SIZE(mt8365_afe_o34_mix)),
	SND_SOC_DAPM_MIXER("O35", SND_SOC_NOPM, 0, 0,
			   mt8365_afe_o35_mix, ARRAY_SIZE(mt8365_afe_o35_mix)),
	SND_SOC_DAPM_MIXER("O36", SND_SOC_NOPM, 0, 0,
			   mt8365_afe_o36_mix, ARRAY_SIZE(mt8365_afe_o36_mix)),

	SND_SOC_DAPM_SWITCH("I2S O03_O04", SND_SOC_NOPM, 0, 0,
			   &i2s_o03_o04_enable_ctl),
	SND_SOC_DAPM_SWITCH("INT ADDA O03_O04", SND_SOC_NOPM, 0, 0,
			   &int_adda_o03_o04_enable_ctl),
	SND_SOC_DAPM_SWITCH("TDM_OUT Virtual Capture", SND_SOC_NOPM, 0, 0,
			   &tdm_asrc_virtul_playback_enable_ctl),
	SND_SOC_DAPM_MIXER("CM2_Mux IO", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("CM1_IO", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("O17O18", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("CM2_Mux_IO GASRC1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("CM2_Mux_IO GASRC2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("CM2_Mux_IO TDM_ASRC", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("TDM_OUT Output Mixer", SND_SOC_NOPM, 0, 0, NULL, 0),
	/* inter-connections */
	SND_SOC_DAPM_MIXER("HW_GAIN1_IN_CH1", SND_SOC_NOPM, 0, 0,
			   mtk_hw_gain1_in_ch1_mix,
			   ARRAY_SIZE(mtk_hw_gain1_in_ch1_mix)),
	SND_SOC_DAPM_MIXER("HW_GAIN1_IN_CH2", SND_SOC_NOPM, 0, 0,
			   mtk_hw_gain1_in_ch2_mix,
			   ARRAY_SIZE(mtk_hw_gain1_in_ch2_mix)),
	/* inter-connections */
	SND_SOC_DAPM_MIXER("ADDA_DL_CH1", SND_SOC_NOPM, 0, 0,
			   mtk_adda_dl_ch1_mix,
			   ARRAY_SIZE(mtk_adda_dl_ch1_mix)),
	SND_SOC_DAPM_MIXER("ADDA_DL_CH2", SND_SOC_NOPM, 0, 0,
			   mtk_adda_dl_ch2_mix,
			   ARRAY_SIZE(mtk_adda_dl_ch2_mix)),

	SND_SOC_DAPM_INPUT("Virtual_DL_Input"),
	SND_SOC_DAPM_INPUT("DL Source"),

	SND_SOC_DAPM_MUX("CM2_Mux_IO Input Mux",
		SND_SOC_NOPM, 0, 0, &mt8365_afe_cm2_mux_input_mux),
	SND_SOC_DAPM_MUX("AIN Mux", SND_SOC_NOPM, 0, 0, &ain_mux),
	SND_SOC_DAPM_MUX("TDM_IN Input Mux",
		SND_SOC_NOPM, 0, 0, &tdm_in_input_mux),
	SND_SOC_DAPM_MUX("VUL2 Input Mux",
		SND_SOC_NOPM, 0, 0, &vul2_in_input_mux),
	SND_SOC_DAPM_MUX("GASRC1 Output Mux",
		SND_SOC_NOPM, 0, 0, &gasrc1_out_sel_mux),
	SND_SOC_DAPM_MUX("GASRC2 Output Mux",
		SND_SOC_NOPM, 0, 0, &gasrc2_out_sel_mux),
	SND_SOC_DAPM_MUX("TDM_ASRC Output Mux",
		SND_SOC_NOPM, 0, 0, &tdm_asrc_out_sel_mux),
	SND_SOC_DAPM_MUX("FM 2ND I2S Mux", SND_SOC_NOPM, 0, 0, &fmi2sin_mux),
	SND_SOC_DAPM_MUX("FM HW Gain Mux", SND_SOC_NOPM, 0, 0, &fmhwgain_mux),

	SND_SOC_DAPM_OUTPUT("PCM1 Out"),
	SND_SOC_DAPM_INPUT("PCM1 In"),
	SND_SOC_DAPM_INPUT("DMIC In"),
	SND_SOC_DAPM_OUTPUT("GASRC1 Out"),
	SND_SOC_DAPM_INPUT("GASRC1 In"),
	SND_SOC_DAPM_OUTPUT("GASRC2 Out"),
	SND_SOC_DAPM_INPUT("GASRC2 In"),
	SND_SOC_DAPM_OUTPUT("TDM_ASRC Out"),
	SND_SOC_DAPM_INPUT("TDM_ASRC In"),
	SND_SOC_DAPM_INPUT("TDM_IN In"),
	SND_SOC_DAPM_INPUT("2ND I2S In"),
	SND_SOC_DAPM_INPUT("HW Gain 1 Out Endpoint"),
	SND_SOC_DAPM_OUTPUT("HW Gain 1 In Endpoint"),
};

static const struct snd_soc_dapm_route mt8365_afe_pcm_routes[] = {
	/* downlink */
	{"I05", NULL, "DL1"},
	{"I06", NULL, "DL1"},
	{"I07", NULL, "DL2"},
	{"I08", NULL, "DL2"},

	{"O03", "I05 Switch", "I05"},
	{"O04", "I06 Switch", "I06"},
	{"O00", "I05 Switch", "I05"},
	{"O01", "I06 Switch", "I06"},
	{"O07", "I05 Switch", "I05"},
	{"O08", "I06 Switch", "I06"},
	{"O27", "I05 Switch", "I05"},
	{"O28", "I06 Switch", "I06"},
	{"O29", "I05 Switch", "I05"},
	{"O30", "I06 Switch", "I06"},

	{"O03", "I07 Switch", "I07"},
	{"O04", "I08 Switch", "I08"},
	{"O00", "I07 Switch", "I07"},
	{"O01", "I08 Switch", "I08"},
	{"O07", "I07 Switch", "I07"},
	{"O08", "I08 Switch", "I08"},

	{"I2S O03_O04", "Switch", "O03"},
	{"I2S O03_O04", "Switch", "O04"},
	{"I2S Playback", NULL, "I2S O03_O04"},
	{"INT ADDA O03_O04", "Switch", "O03"},
	{"INT ADDA O03_O04", "Switch", "O04"},
	{"INT ADDA Playback", NULL, "INT ADDA O03_O04"},
	{"INT ADDA Playback", NULL, "ADDA_DL_CH1"},
	{"INT ADDA Playback", NULL, "ADDA_DL_CH2"},

	{"2ND I2S Playback", NULL, "O00"},
	{"2ND I2S Playback", NULL, "O01"},
	{"PCM1 Playback", NULL, "O07"},
	{"PCM1 Playback", NULL, "O08"},
	{"PCM1 Out", NULL, "PCM1 Playback"},

	/* uplink */
	{"AWB", NULL, "O05"},
	{"AWB", NULL, "O06"},
	{"VUL", NULL, "O09"},
	{"VUL", NULL, "O10"},
	{"VUL3", NULL, "O11"},
	{"VUL3", NULL, "O12"},

	{"AIN Mux", "EXT ADC", "I2S Capture"},
	{"AIN Mux", "INT ADC", "INT ADDA Capture"},
	{"I03", NULL, "AIN Mux"},
	{"I04", NULL, "AIN Mux"},

	{"I00", NULL, "2ND I2S Capture"},
	{"I01", NULL, "2ND I2S Capture"},
	{"2ND I2S Capture", NULL, "2ND I2S In"},

	{"I09", NULL, "PCM1 Capture"},
	{"I22", NULL, "PCM1 Capture"},
	{"PCM1 Capture", NULL, "PCM1 In"},

	{"I14", NULL, "DMIC Capture"},
	{"I15", NULL, "DMIC Capture"},
	{"I16", NULL, "DMIC Capture"},
	{"I17", NULL, "DMIC Capture"},
	{"I18", NULL, "DMIC Capture"},
	{"I19", NULL, "DMIC Capture"},
	{"I20", NULL, "DMIC Capture"},
	{"I21", NULL, "DMIC Capture"},
	{"DMIC Capture", NULL, "DMIC In"},
	{"HW Gain 1 In Endpoint", NULL, "HW Gain 1 In"},
	{"HW Gain 1 Out", NULL, "HW Gain 1 Out Endpoint"},
	{"HW Gain 1 In", NULL, "HW_GAIN1_IN_CH1"},
	{"HW Gain 1 In", NULL, "HW_GAIN1_IN_CH2"},
	{"ADDA_DL_CH1", "GAIN1_OUT_CH1", "Hostless FM DL"},
	{"ADDA_DL_CH2", "GAIN1_OUT_CH2", "Hostless FM DL"},

	{"HW_GAIN1_IN_CH1", "CONNSYS_I2S_CH1", "Hostless FM DL"},
	{"HW_GAIN1_IN_CH2", "CONNSYS_I2S_CH2", "Hostless FM DL"},

	{"VIRTUAL_DL_SRC Capture", NULL, "DL Source"},
	{"I05L", NULL, "VIRTUAL_DL_SRC Capture"},
	{"I06L", NULL, "VIRTUAL_DL_SRC Capture"},
	{"I07L", NULL, "VIRTUAL_DL_SRC Capture"},
	{"I08L", NULL, "VIRTUAL_DL_SRC Capture"},
	{"FM 2ND I2S Mux", "FM_2ND_I2S_IN", "2ND I2S Capture"},
	{"FM HW Gain Mux", "FM_HW_GAIN_IO", "HW Gain 1 Out"},
	{"Hostless FM UL", NULL, "FM 2ND I2S Mux"},
	{"Hostless FM UL", NULL, "FM HW Gain Mux"},

	{"O05", "I05 Switch", "I05L"},
	{"O06", "I06 Switch", "I06L"},
	{"O05", "I07 Switch", "I07L"},
	{"O06", "I08 Switch", "I08L"},

	{"O05", "I03 Switch", "I03"},
	{"O06", "I04 Switch", "I04"},
	{"O05", "I00 Switch", "I00"},
	{"O06", "I01 Switch", "I01"},
	{"O05", "I09 Switch", "I09"},
	{"O06", "I22 Switch", "I22"},
	{"O05", "I14 Switch", "I14"},
	{"O06", "I15 Switch", "I15"},
	{"O05", "I16 Switch", "I16"},
	{"O06", "I17 Switch", "I17"},
	{"O05", "I18 Switch", "I18"},
	{"O06", "I19 Switch", "I19"},
	{"O05", "I20 Switch", "I20"},
	{"O06", "I21 Switch", "I21"},
	{"O05", "I23 Switch", "I23"},
	{"O06", "I24 Switch", "I24"},

	{"O09", "I03 Switch", "I03"},
	{"O10", "I04 Switch", "I04"},
	{"O09", "I00 Switch", "I00"},
	{"O10", "I01 Switch", "I01"},
	{"O09", "I09 Switch", "I09"},
	{"O10", "I22 Switch", "I22"},
	{"O09", "I14 Switch", "I14"},
	{"O10", "I15 Switch", "I15"},
	{"O09", "I16 Switch", "I16"},
	{"O10", "I17 Switch", "I17"},
	{"O09", "I18 Switch", "I18"},
	{"O10", "I19 Switch", "I19"},
	{"O09", "I20 Switch", "I20"},
	{"O10", "I21 Switch", "I21"},

	{"O11", "I03 Switch", "I03"},
	{"O12", "I04 Switch", "I04"},
	{"O11", "I00 Switch", "I00"},
	{"O12", "I01 Switch", "I01"},
	{"O11", "I09 Switch", "I09"},
	{"O12", "I22 Switch", "I22"},
	{"O11", "I14 Switch", "I14"},
	{"O12", "I15 Switch", "I15"},
	{"O11", "I16 Switch", "I16"},
	{"O12", "I17 Switch", "I17"},
	{"O11", "I18 Switch", "I18"},
	{"O12", "I19 Switch", "I19"},
	{"O11", "I20 Switch", "I20"},
	{"O12", "I21 Switch", "I21"},

	/* GASRC1 */
	{"GASRC1 Playback", NULL, "O27"},
	{"GASRC1 Playback", NULL, "O28"},

	{"GASRC1 Output Mux", "CM1_GASRC1", "GASRC1 Playback"},
	{"GASRC1 Output Mux", "CM2_GASRC1", "GASRC1 Playback"},

	{"GASRC1 Out", NULL, "GASRC1 Output Mux"},

	{"CM2_Mux_IO GASRC1", NULL, "GASRC1 Capture"},
	{"I23", NULL, "GASRC1 Capture"},
	{"I24", NULL, "GASRC1 Capture"},

	{"GASRC1 Capture", NULL, "GASRC1 In"},

	/* GASRC2 */
	{"GASRC2 Playback", NULL, "O29"},
	{"GASRC2 Playback", NULL, "O30"},

	{"GASRC2 Output Mux", "CM1_GASRC2", "GASRC2 Playback"},
	{"GASRC2 Output Mux", "CM2_GASRC2", "GASRC2 Playback"},

	{"GASRC2 Out", NULL, "GASRC2 Output Mux"},

	{"CM2_Mux_IO GASRC2", NULL, "GASRC2 Capture"},
	{"I25", NULL, "GASRC2 Capture"},
	{"I26", NULL, "GASRC2 Capture"},

	{"GASRC2 Capture", NULL, "GASRC2 In"},

	/* TDM_IN */
	{"TDM_IN", NULL, "TDM_IN Input Mux"},

	{"TDM_IN Input Mux", "TDM_IN_FROM_IO", "TDM_IN Capture"},
	{"TDM_IN Input Mux", "TDM_IN_FROM_CM2", "CM2_Mux IO"},

	/* CM2_Mux*/
	{"CM2_Mux IO", NULL, "TDM_IN Capture"},
	{"CM2_Mux IO", NULL, "CM2_Mux_IO Input Mux"},

	{"CM2_Mux_IO Input Mux", "FROM_GASRC1_OUT", "CM2_Mux_IO GASRC1"},
	{"CM2_Mux_IO Input Mux", "FROM_GASRC2_OUT", "CM2_Mux_IO GASRC2"},
	{"CM2_Mux_IO Input Mux", "FROM_TDM_ASRC_OUT", "CM2_Mux_IO TDM_ASRC"},

	/* TDM OUT */
	{"TDM_OUT Playback", NULL, "TDM_OUT"},

	{"TDM_OUT Virtual Capture", "Switch", "TDM_OUT"},

	/* TDM ASRC */
	{"TDM_ASRC Playback", NULL, "TDM_OUT Virtual Capture"},

	{"TDM_ASRC Output Mux", "CM1_TDM_ASRC", "TDM_ASRC Playback"},
	{"TDM_ASRC Output Mux", "CM2_TDM_ASRC", "TDM_ASRC Playback"},

	{"TDM_ASRC Out", NULL, "TDM_ASRC Output Mux"},

	{"CM2_Mux_IO TDM_ASRC", NULL, "TDM_ASRC Capture"},

	{"I27", NULL, "TDM_ASRC Capture"},
	{"I28", NULL, "TDM_ASRC Capture"},
	{"I29", NULL, "TDM_ASRC Capture"},
	{"I30", NULL, "TDM_ASRC Capture"},
	{"I31", NULL, "TDM_ASRC Capture"},
	{"I32", NULL, "TDM_ASRC Capture"},
	{"I33", NULL, "TDM_ASRC Capture"},
	{"I34", NULL, "TDM_ASRC Capture"},

	{"O25", "I27 Switch", "I27"},
	{"O26", "I28 Switch", "I28"},
	{"O31", "I29 Switch", "I29"},
	{"O32", "I30 Switch", "I30"},
	{"O33", "I31 Switch", "I31"},
	{"O34", "I32 Switch", "I32"},
	{"O35", "I33 Switch", "I33"},
	{"O36", "I34 Switch", "I34"},

	{"TDM_ASRC Capture", NULL, "TDM_ASRC In"},
	/* VUL2 */
	{"VUL2", NULL, "VUL2 Input Mux"},
	{"VUL2 Input Mux", "VUL2_IN_FROM_O17O18", "O17O18"},
	{"VUL2 Input Mux", "VUL2_IN_FROM_CM1", "CM1_IO"},

	{"O17O18", NULL, "O17"},
	{"O17O18", NULL, "O18"},
	{"CM1_IO", NULL, "O17"},
	{"CM1_IO", NULL, "O18"},
	{"CM1_IO", NULL, "O19"},
	{"CM1_IO", NULL, "O20"},
	{"CM1_IO", NULL, "O21"},
	{"CM1_IO", NULL, "O22"},
	{"CM1_IO", NULL, "O23"},
	{"CM1_IO", NULL, "O24"},
	{"CM1_IO", NULL, "O25"},
	{"CM1_IO", NULL, "O26"},
	{"CM1_IO", NULL, "O31"},
	{"CM1_IO", NULL, "O32"},
	{"CM1_IO", NULL, "O33"},
	{"CM1_IO", NULL, "O34"},
	{"CM1_IO", NULL, "O35"},
	{"CM1_IO", NULL, "O36"},

	{"O17", "I14 Switch", "I14"},
	{"O18", "I15 Switch", "I15"},
	{"O19", "I16 Switch", "I16"},
	{"O20", "I17 Switch", "I17"},
	{"O21", "I18 Switch", "I18"},
	{"O22", "I19 Switch", "I19"},
	{"O23", "I20 Switch", "I20"},
	{"O24", "I21 Switch", "I21"},
	{"O25", "I23 Switch", "I23"},
	{"O26", "I24 Switch", "I24"},
	{"O25", "I25 Switch", "I25"},
	{"O26", "I26 Switch", "I26"},

	{"O17", "I03 Switch", "I03"},
	{"O18", "I04 Switch", "I04"},
	{"O18", "I23 Switch", "I23"},
	{"O18", "I25 Switch", "I25"},
	{"O19", "I04 Switch", "I04"},
	{"O19", "I23 Switch", "I23"},
	{"O19", "I24 Switch", "I24"},
	{"O19", "I25 Switch", "I25"},
	{"O19", "I26 Switch", "I26"},
	{"O20", "I24 Switch", "I24"},
	{"O20", "I26 Switch", "I26"},
	{"O21", "I23 Switch", "I23"},
	{"O21", "I25 Switch", "I25"},
	{"O22", "I24 Switch", "I24"},
	{"O22", "I26 Switch", "I26"},

	{"O23", "I23 Switch", "I23"},
	{"O23", "I25 Switch", "I25"},
	{"O24", "I24 Switch", "I24"},
	{"O24", "I26 Switch", "I26"},
	{"O24", "I23 Switch", "I23"},
	{"O24", "I25 Switch", "I25"},
	{"O13", "I00 Switch", "I00"},
	{"O14", "I01 Switch", "I01"},
	{"O03", "I10 Switch", "I10"},
	{"O04", "I11 Switch", "I11"},
};

static const struct snd_soc_component_driver mt8365_afe_pcm_dai_component = {
	.name = "mt8365-afe-pcm-dai",
	.dapm_widgets = mt8365_afe_pcm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(mt8365_afe_pcm_widgets),
	.dapm_routes = mt8365_afe_pcm_routes,
	.num_dapm_routes = ARRAY_SIZE(mt8365_afe_pcm_routes),
};

static const struct mtk_base_memif_data memif_data[MT8365_AFE_MEMIF_NUM] = {
	{
		.name = "DL1",
		.id = MT8365_AFE_MEMIF_DL1,
		.reg_ofs_base = AFE_DL1_BASE,
		.reg_ofs_cur = AFE_DL1_CUR,
		.fs_reg = AFE_DAC_CON1,
		.fs_shift = 0,
		.fs_maskbit = 0xf,
		.mono_reg = AFE_DAC_CON1,
		.mono_shift = 21,
		.hd_reg = AFE_MEMIF_PBUF_SIZE,
		.hd_shift = 16,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 1,
		.msb_reg = -1,
		.msb_shift = -1,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
	}, {
		.name = "DL2",
		.id = MT8365_AFE_MEMIF_DL2,
		.reg_ofs_base = AFE_DL2_BASE,
		.reg_ofs_cur = AFE_DL2_CUR,
		.fs_reg = AFE_DAC_CON1,
		.fs_shift = 4,
		.fs_maskbit = 0xf,
		.mono_reg = AFE_DAC_CON1,
		.mono_shift = 22,
		.hd_reg = AFE_MEMIF_PBUF_SIZE,
		.hd_shift = 18,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 2,
		.msb_reg = -1,
		.msb_shift = -1,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
	}, {
		.name = "TDM OUT",
		.id = MT8365_AFE_MEMIF_TDM_OUT,
		.reg_ofs_base = AFE_HDMI_OUT_BASE,
		.reg_ofs_cur = AFE_HDMI_OUT_CUR,
		.fs_reg = -1,
		.fs_shift = -1,
		.fs_maskbit = -1,
		.mono_reg = -1,
		.mono_shift = -1,
		.hd_reg = AFE_MEMIF_PBUF_SIZE,
		.hd_shift = 28,
		.enable_reg = AFE_HDMI_OUT_CON0,
		.enable_shift = 0,
		.msb_reg = -1,
		.msb_shift = -1,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
	}, {
		.name = "AWB",
		.id = MT8365_AFE_MEMIF_AWB,
		.reg_ofs_base = AFE_AWB_BASE,
		.reg_ofs_cur = AFE_AWB_CUR,
		.fs_reg = AFE_DAC_CON1,
		.fs_shift = 12,
		.fs_maskbit = 0xf,
		.mono_reg = AFE_DAC_CON1,
		.mono_shift = 24,
		.hd_reg = AFE_MEMIF_PBUF_SIZE,
		.hd_shift = 20,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 6,
		.msb_reg = AFE_MEMIF_MSB,
		.msb_shift = 17,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
	}, {
		.name = "VUL",
		.id = MT8365_AFE_MEMIF_VUL,
		.reg_ofs_base = AFE_VUL_BASE,
		.reg_ofs_cur = AFE_VUL_CUR,
		.fs_reg = AFE_DAC_CON1,
		.fs_shift = 16,
		.fs_maskbit = 0xf,
		.mono_reg = AFE_DAC_CON1,
		.mono_shift = 27,
		.hd_reg = AFE_MEMIF_PBUF_SIZE,
		.hd_shift = 22,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 3,
		.msb_reg = AFE_MEMIF_MSB,
		.msb_shift = 20,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
	}, {
		.name = "VUL2",
		.id = MT8365_AFE_MEMIF_VUL2,
		.reg_ofs_base = AFE_VUL_D2_BASE,
		.reg_ofs_cur = AFE_VUL_D2_CUR,
		.fs_reg = AFE_DAC_CON0,
		.fs_shift = 20,
		.fs_maskbit = 0xf,
		.mono_reg = -1,
		.mono_shift = -1,
		.hd_reg = AFE_MEMIF_PBUF_SIZE,
		.hd_shift = 14,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 9,
		.msb_reg = AFE_MEMIF_MSB,
		.msb_shift = 21,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
	}, {
		.name = "VUL3",
		.id = MT8365_AFE_MEMIF_VUL3,
		.reg_ofs_base = AFE_VUL3_BASE,
		.reg_ofs_cur = AFE_VUL3_CUR,
		.fs_reg = AFE_DAC_CON1,
		.fs_shift = 8,
		.fs_maskbit = 0xf,
		.mono_reg = AFE_DAC_CON0,
		.mono_shift = 13,
		.hd_reg = AFE_MEMIF_PBUF2_SIZE,
		.hd_shift = 10,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 12,
		.msb_reg = AFE_MEMIF_MSB,
		.msb_shift = 27,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
	}, {
		.name = "TDM IN",
		.id = MT8365_AFE_MEMIF_TDM_IN,
		.reg_ofs_base = AFE_HDMI_IN_2CH_BASE,
		.reg_ofs_cur = AFE_HDMI_IN_2CH_CUR,
		.fs_reg = -1,
		.fs_shift = -1,
		.fs_maskbit = -1,
		.mono_reg = AFE_HDMI_IN_2CH_CON0,
		.mono_shift = 1,
		.hd_reg = AFE_MEMIF_PBUF2_SIZE,
		.hd_shift = 8,
		.hd_align_mshift = 5,
		.enable_reg = AFE_HDMI_IN_2CH_CON0,
		.enable_shift = 0,
		.msb_reg = AFE_MEMIF_MSB,
		.msb_shift = 28,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
	},
};

static const struct mtk_base_irq_data irq_data[MT8365_AFE_IRQ_NUM] = {
	{
		.id = MT8365_AFE_IRQ1,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT1,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0x3ffff,
		.irq_en_reg = AFE_IRQ_MCU_CON,
		.irq_en_shift = 0,
		.irq_fs_reg = AFE_IRQ_MCU_CON,
		.irq_fs_shift = 4,
		.irq_fs_maskbit = 0xf,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = 0,
	}, {
		.id = MT8365_AFE_IRQ2,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT2,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0x3ffff,
		.irq_en_reg = AFE_IRQ_MCU_CON,
		.irq_en_shift = 1,
		.irq_fs_reg = AFE_IRQ_MCU_CON,
		.irq_fs_shift = 8,
		.irq_fs_maskbit = 0xf,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = 1,
	}, {
		.id = MT8365_AFE_IRQ3,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT3,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0x3ffff,
		.irq_en_reg = AFE_IRQ_MCU_CON,
		.irq_en_shift = 2,
		.irq_fs_reg = AFE_IRQ_MCU_CON,
		.irq_fs_shift = 16,
		.irq_fs_maskbit = 0xf,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = 2,
	}, {
		.id = MT8365_AFE_IRQ4,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT4,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0x3ffff,
		.irq_en_reg = AFE_IRQ_MCU_CON,
		.irq_en_shift = 3,
		.irq_fs_reg = AFE_IRQ_MCU_CON,
		.irq_fs_shift = 20,
		.irq_fs_maskbit = 0xf,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = 3,
	}, {
		.id = MT8365_AFE_IRQ5,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT5,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0x3ffff,
		.irq_en_reg = AFE_IRQ_MCU_CON2,
		.irq_en_shift = 3,
		.irq_fs_reg = -1,
		.irq_fs_shift = 0,
		.irq_fs_maskbit = 0x0,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = 4,
	}, {
		.id = MT8365_AFE_IRQ6,
		.irq_cnt_reg = -1,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0x0,
		.irq_en_reg = AFE_IRQ_MCU_CON,
		.irq_en_shift = 13,
		.irq_fs_reg = -1,
		.irq_fs_shift = 0,
		.irq_fs_maskbit = 0x0,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = 5,
	}, {
		.id = MT8365_AFE_IRQ7,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT7,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0x3ffff,
		.irq_en_reg = AFE_IRQ_MCU_CON,
		.irq_en_shift = 14,
		.irq_fs_reg = AFE_IRQ_MCU_CON,
		.irq_fs_shift = 24,
		.irq_fs_maskbit = 0xf,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = 6,
	}, {
		.id = MT8365_AFE_IRQ8,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT8,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0x3ffff,
		.irq_en_reg = AFE_IRQ_MCU_CON,
		.irq_en_shift = 15,
		.irq_fs_reg = AFE_IRQ_MCU_CON,
		.irq_fs_shift = 28,
		.irq_fs_maskbit = 0xf,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = 7,
	}, {
		.id = MT8365_AFE_IRQ9,
		.irq_cnt_reg = -1,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0x0,
		.irq_en_reg = AFE_IRQ_MCU_CON2,
		.irq_en_shift = 2,
		.irq_fs_reg = -1,
		.irq_fs_shift = 0,
		.irq_fs_maskbit = 0x0,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = 8,
	}, {
		.id = MT8365_AFE_IRQ10,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT10,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0x3ffff,
		.irq_en_reg = AFE_IRQ_MCU_CON2,
		.irq_en_shift = 4,
		.irq_fs_reg = -1,
		.irq_fs_shift = 0,
		.irq_fs_maskbit = 0x0,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = 9,
	},
};

static int memif_specified_irqs[MT8365_AFE_MEMIF_NUM] = {
	[MT8365_AFE_MEMIF_DL1] = MT8365_AFE_IRQ1,
	[MT8365_AFE_MEMIF_DL2] = MT8365_AFE_IRQ2,
	[MT8365_AFE_MEMIF_TDM_OUT] = MT8365_AFE_IRQ5,
	/* [MT8365_AFE_MEMIF_SPDIF_OUT] = MT8365_AFE_IRQ6,*/
	[MT8365_AFE_MEMIF_AWB] = MT8365_AFE_IRQ3,
	[MT8365_AFE_MEMIF_VUL] = MT8365_AFE_IRQ4,
	[MT8365_AFE_MEMIF_VUL2] = MT8365_AFE_IRQ7,
	[MT8365_AFE_MEMIF_VUL3] = MT8365_AFE_IRQ8,
	/* [MT8365_AFE_MEMIF_SPDIF_IN] = MT8365_AFE_IRQ9, */
	[MT8365_AFE_MEMIF_TDM_IN] = MT8365_AFE_IRQ10,
};

static const struct regmap_config mt8365_afe_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = MAX_REGISTER,
	.cache_type = REGCACHE_NONE,
};

static irqreturn_t mt8365_afe_irq_handler(int irq, void *dev_id)
{
	struct mtk_base_afe *afe = dev_id;
	unsigned int reg_value;
	unsigned int mcu_irq_mask;
	int i, ret;

	ret = regmap_read(afe->regmap, AFE_IRQ_MCU_STATUS, &reg_value);
	if (ret) {
		dev_err(afe->dev, "%s irq status err\n", __func__);
		reg_value = AFE_IRQ_STATUS_BITS;
		goto err_irq;
	}

	ret = regmap_read(afe->regmap, AFE_IRQ_MCU_EN, &mcu_irq_mask);
	if (ret) {
		dev_err(afe->dev, "%s irq mcu_en err\n", __func__);
		reg_value = AFE_IRQ_STATUS_BITS;
		goto err_irq;
	}

	/* only clr cpu irq */
	reg_value &= mcu_irq_mask;

	for (i = 0; i < MT8365_AFE_MEMIF_NUM; i++) {
		struct mtk_base_afe_memif *memif = &afe->memif[i];
		struct mtk_base_afe_irq *mcu_irq;

		if (memif->irq_usage < 0)
			continue;

		mcu_irq = &afe->irqs[memif->irq_usage];

		if (!(reg_value & (1 << mcu_irq->irq_data->irq_clr_shift)))
			continue;

		snd_pcm_period_elapsed(memif->substream);
	}

err_irq:
	/* clear irq */
	regmap_write(afe->regmap, AFE_IRQ_MCU_CLR,
		reg_value & AFE_IRQ_STATUS_BITS);

	return IRQ_HANDLED;
}

static int __maybe_unused mt8365_afe_runtime_suspend(struct device *dev)
{
	return 0;
}

static int mt8365_afe_runtime_resume(struct device *dev)
{
	return 0;
}

static int __maybe_unused mt8365_afe_suspend(struct device *dev)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);
	struct regmap *regmap = afe->regmap;
	int i;

	mt8365_afe_enable_main_clk(afe);

	if (!afe->reg_back_up)
		afe->reg_back_up =
			devm_kcalloc(dev, afe->reg_back_up_list_num,
				     sizeof(unsigned int), GFP_KERNEL);

	for (i = 0; i < afe->reg_back_up_list_num; i++)
		regmap_read(regmap, afe->reg_back_up_list[i],
			    &afe->reg_back_up[i]);

	mt8365_afe_disable_main_clk(afe);

	return 0;
}

static int __maybe_unused mt8365_afe_resume(struct device *dev)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);
	struct regmap *regmap = afe->regmap;
	int i = 0;

	if (!afe->reg_back_up) {
		dev_dbg(dev, "%s no reg_backup\n", __func__);
		return 0;
	}

	mt8365_afe_enable_main_clk(afe);

	for (i = 0; i < afe->reg_back_up_list_num; i++)
		regmap_write(regmap, afe->reg_back_up_list[i],
				 afe->reg_back_up[i]);

	mt8365_afe_disable_main_clk(afe);

	return 0;
}

static int __maybe_unused mt8365_afe_dev_runtime_suspend(struct device *dev)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);

	dev_dbg(afe->dev, "%s suspend %d %d >>\n", __func__,
		pm_runtime_status_suspended(dev), afe->suspended);

	if (pm_runtime_status_suspended(dev) || afe->suspended)
		return 0;

	mt8365_afe_suspend(dev);

	afe->suspended = true;

	dev_dbg(afe->dev, "%s <<\n", __func__);

	return 0;
}

static int __maybe_unused mt8365_afe_dev_runtime_resume(struct device *dev)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);

	dev_dbg(afe->dev, "%s suspend %d %d >>\n", __func__,
		pm_runtime_status_suspended(dev), afe->suspended);

	if (pm_runtime_status_suspended(dev) || !afe->suspended)
		return 0;

	mt8365_afe_resume(dev);

	afe->suspended = false;

	dev_dbg(afe->dev, "%s <<\n", __func__);

	return 0;
}

static int mt8365_afe_init_registers(struct mtk_base_afe *afe)
{
	size_t i;

	static struct {
		unsigned int reg;
		unsigned int mask;
		unsigned int val;
	} init_regs[] = {
		{ AFE_CONN_24BIT, GENMASK(31, 0), GENMASK(31, 0) },
		{ AFE_CONN_24BIT_1, GENMASK(21, 0), GENMASK(21, 0) },
	};

	mt8365_afe_enable_main_clk(afe);

	for (i = 0; i < ARRAY_SIZE(init_regs); i++)
		regmap_update_bits(afe->regmap, init_regs[i].reg,
				   init_regs[i].mask, init_regs[i].val);

	mt8365_afe_disable_main_clk(afe);

	return 0;
}

static bool mt8365_afe_validate_sram(struct mtk_base_afe *afe,
	u32 phy_addr, u32 size)
{
	struct mt8365_afe_private *afe_priv = afe->platform_priv;

	if (afe_priv->afe_sram_phy_addr &&
	    (phy_addr >= afe_priv->afe_sram_phy_addr) &&
	    ((phy_addr + size) <=
	     (afe_priv->afe_sram_phy_addr + afe_priv->afe_sram_size))) {
		return true;
	}

	return false;
}

static void __iomem *mt8365_afe_get_sram_vir_addr(struct mtk_base_afe *afe,
	u32 phy_addr)
{
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	u32 off = phy_addr - afe_priv->afe_sram_phy_addr;

	if (afe_priv->afe_sram_phy_addr &&
	    (off >= 0) && (off < afe_priv->afe_sram_size)) {
		return ((unsigned char *)afe_priv->afe_sram_vir_addr + off);
	}

	return NULL;
}

static void mt8365_afe_parse_of(struct mtk_base_afe *afe,
				struct device_node *np)
{
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	size_t i;
	int ret;
	unsigned int temps[4];
	char prop[128];
	unsigned int val[2];
	struct {
		char *name;
		unsigned int val;
	} of_fe_table[] = {
		{ "dl1",	MT8365_AFE_MEMIF_DL1 },
		{ "dl2",	MT8365_AFE_MEMIF_DL2 },
		{ "tdmout",	MT8365_AFE_MEMIF_TDM_OUT },
		{ "awb",	MT8365_AFE_MEMIF_AWB },
		{ "vul",	MT8365_AFE_MEMIF_VUL },
		{ "vul2",	MT8365_AFE_MEMIF_VUL2 },
		{ "vul3",	MT8365_AFE_MEMIF_VUL3 },
		{ "tdmin",	MT8365_AFE_MEMIF_TDM_IN },
	};

	ret = of_property_read_u32_array(np, "mediatek,tdm-out-mode",
					&temps[0],
					 1);
	if (ret == 0)
		afe_priv->tdm_out_mode = temps[0];
	else
		afe_priv->tdm_out_mode = MT8365_AFE_TDM_OUT_TDM;

	ret = of_property_read_u32_array(np, "mediatek,dmic-mode",
					 &temps[0],
					 1);
	if (ret == 0)
		afe_priv->dmic_data.dmic_mode = temps[0];

	ret = of_property_read_u32_array(np, "mediatek,dmic-two-wire-mode",
					 &temps[0],
					 1);
	if (ret == 0)
		afe_priv->dmic_data.two_wire_mode = temps[0];

	ret = of_property_read_u32_array(np, "mediatek,dmic-clk-phases",
					 &temps[0],
					 2);
	if (ret == 0) {
		afe_priv->dmic_data.clk_phase_sel_ch1 = temps[0];
		afe_priv->dmic_data.clk_phase_sel_ch2 = temps[1];
	} else if (!afe_priv->dmic_data.two_wire_mode) {
		afe_priv->dmic_data.clk_phase_sel_ch1 = 0;
		afe_priv->dmic_data.clk_phase_sel_ch2 = 4;
	}

	afe_priv->dmic_data.iir_on = of_property_read_bool(np,
		"mediatek,dmic-iir-on");

	if (afe_priv->dmic_data.iir_on) {
		ret = of_property_read_u32_array(np, "mediatek,dmic-irr-mode",
						 &temps[0],
						 1);
		if (ret == 0)
			afe_priv->dmic_data.irr_mode = temps[0];
	}

	if (of_property_read_u32_array(np, "mediatek,i2s-clock-modes",
	    afe_priv->i2s_clk_modes, ARRAY_SIZE(afe_priv->i2s_clk_modes))) {
		for (i = 0; i < ARRAY_SIZE(afe_priv->i2s_clk_modes); i++)
			afe_priv->i2s_clk_modes[i] =
				MT8365_AFE_I2S_SEPARATE_CLOCK;
	}

	ret = of_property_read_u32_array(np, "mediatek,i2s-dynamic-bck",
					 &temps[0],
					 1);
	if (ret == 0)
		afe_priv->i2s_dynamic_bck = temps[0];

	for (i = 0; i < ARRAY_SIZE(of_fe_table); i++) {
		bool valid_sram;

		memset(val, 0, sizeof(val));

		snprintf(prop, sizeof(prop), "mediatek,%s-use-sram",
			 of_fe_table[i].name);
		ret = of_property_read_u32_array(np, prop, &val[0], 2);
		if (ret)
			continue;

		valid_sram = mt8365_afe_validate_sram(afe, val[0], val[1]);
		if (!valid_sram) {
			dev_warn(afe->dev, "%s %s validate 0x%x 0x%x fail\n",
				 __func__, of_fe_table[i].name,
				 val[0], val[1]);
			continue;
		}

		afe_priv->fe_data[i].sram_phy_addr = val[0];
		afe_priv->fe_data[i].sram_size = val[1];
		afe_priv->fe_data[i].sram_vir_addr =
			mt8365_afe_get_sram_vir_addr(afe, val[0]);
	}

	/* hoping-clk-mode 0:26m 1:54.6m default 26m */
	ret = of_property_read_u32_array(np, "mediatek,hoping-clk-mode",
					&temps[0],
					 1);
	if (ret == 0) {
		if (temps[0])
			mt8365_afe_set_clk_parent(afe,
				afe_priv->clocks[MT8365_CLK_TOP_AUD_SEL],
				afe_priv->clocks[MT8365_CLK_AUD_SYSPLL3_D4]);
		else
			mt8365_afe_set_clk_parent(afe,
				afe_priv->clocks[MT8365_CLK_TOP_AUD_SEL],
				afe_priv->clocks[MT8365_CLK_CLK26M]);
	} else
		mt8365_afe_set_clk_parent(afe,
			afe_priv->clocks[MT8365_CLK_TOP_AUD_SEL],
			afe_priv->clocks[MT8365_CLK_CLK26M]);
}

static int mt8365_afe_component_probe(struct snd_soc_component *component)
{
	return mtk_afe_add_sub_dai_control(component);
}

static const struct snd_soc_component_driver mt8365_afe_component = {
	.name		= AFE_PCM_NAME,
	.probe		= mt8365_afe_component_probe,
	.pointer	= mtk_afe_pcm_pointer,
	.pcm_construct	= mtk_afe_pcm_new,
};

static int mt8365_afe_pcm_dev_probe(struct platform_device *pdev)
{
	struct mtk_base_afe *afe;
	struct mt8365_afe_private *afe_priv;
	struct device *dev;
	int ret, i, sel_irq;
	unsigned int irq_id;
	struct resource *res;

	afe = devm_kzalloc(&pdev->dev, sizeof(*afe), GFP_KERNEL);
	if (!afe)
		return -ENOMEM;
	platform_set_drvdata(pdev, afe);

	afe->platform_priv = devm_kzalloc(&pdev->dev, sizeof(*afe_priv),
					  GFP_KERNEL);
	if (!afe->platform_priv)
		return -ENOMEM;

	afe_priv = afe->platform_priv;
	afe->dev = &pdev->dev;
	dev = afe->dev;

	spin_lock_init(&afe_priv->afe_ctrl_lock);
	mutex_init(&afe_priv->afe_clk_mutex);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	afe->base_addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(afe->base_addr))
		return PTR_ERR(afe->base_addr);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (res) {
		afe_priv->afe_sram_vir_addr =
			devm_ioremap_resource(&pdev->dev, res);
		if (!IS_ERR(afe_priv->afe_sram_vir_addr)) {
			afe_priv->afe_sram_phy_addr = res->start;
			afe_priv->afe_sram_size = resource_size(res);
		}
	}

	/* initial audio related clock */
	ret = mt8365_afe_init_audio_clk(afe);
	if (ret) {
		dev_err(afe->dev, "mt8365_afe_init_audio_clk fail\n");
		return ret;
	}

	afe->regmap = devm_regmap_init_mmio_clk(&pdev->dev, "top_audio_sel",
		afe->base_addr,	&mt8365_afe_regmap_config);
	if (IS_ERR(afe->regmap))
		return PTR_ERR(afe->regmap);

	/* memif % irq initialize*/
	afe->memif_size = MT8365_AFE_MEMIF_NUM;
	afe->memif = devm_kcalloc(afe->dev, afe->memif_size,
				  sizeof(*afe->memif), GFP_KERNEL);
	if (!afe->memif)
		return -ENOMEM;

	afe->irqs_size = MT8365_AFE_IRQ_NUM;
	afe->irqs = devm_kcalloc(afe->dev, afe->irqs_size,
				 sizeof(*afe->irqs), GFP_KERNEL);
	if (!afe->irqs)
		return -ENOMEM;

	for (i = 0; i < afe->irqs_size; i++)
		afe->irqs[i].irq_data = &irq_data[i];

	irq_id = platform_get_irq(pdev, 0);
	if (!irq_id) {
		dev_err(afe->dev, "np %s no irq\n", afe->dev->of_node->name);
		return -ENXIO;
	}
	ret = devm_request_irq(afe->dev, irq_id, mt8365_afe_irq_handler,
			       0, "Afe_ISR_Handle", (void *)afe);
	if (ret) {
		dev_err(afe->dev, "could not request_irq\n");
		return ret;
	}

	/* init sub_dais */
	INIT_LIST_HEAD(&afe->sub_dais);

	for (i = 0; i < afe->memif_size; i++) {
		afe->memif[i].data = &memif_data[i];
		sel_irq = memif_specified_irqs[i];
		if (sel_irq >= 0) {
			afe->memif[i].irq_usage = sel_irq;
			afe->memif[i].const_irq = 1;
			afe->irqs[sel_irq].irq_occupyed = true;
		} else {
			afe->memif[i].irq_usage = -1;
		}
	}

	afe->mtk_afe_hardware = &mt8365_afe_hardware;
	afe->memif_fs = mt8365_memif_fs;
	afe->irq_fs = mt8365_irq_fs;

	pm_runtime_enable(&pdev->dev);

	pm_runtime_get_sync(&pdev->dev);
	afe->reg_back_up_list = mt8365_afe_backup_list;
	afe->reg_back_up_list_num = ARRAY_SIZE(mt8365_afe_backup_list);
	afe->runtime_resume = mt8365_afe_runtime_resume;
	afe->runtime_suspend = mt8365_afe_runtime_suspend;

	/* open afe pdn for dapm read/write audio register */
	mt8365_afe_enable_top_cg(afe, MT8365_TOP_CG_AFE);
	mt8365_afe_parse_of(afe, pdev->dev.of_node);

	ret = devm_snd_soc_register_component(&pdev->dev,
					      &mt8365_afe_component, NULL, 0);
	if (ret) {
		dev_warn(dev, "err_platform\n");
		return ret;
	}

	ret = devm_snd_soc_register_component(&pdev->dev,
					      &mt8365_afe_pcm_dai_component,
					      mt8365_afe_pcm_dais,
					      ARRAY_SIZE(mt8365_afe_pcm_dais));
	if (ret) {
		dev_warn(dev, "err_dai_component\n");
		return ret;
	}

	mt8365_afe_init_registers(afe);

	mt8365_afe_init_debugfs(afe);

	dev_info(&pdev->dev, "MT8365 AFE driver initialized.\n");

	return 0;
}

static int mt8365_afe_pcm_dev_remove(struct platform_device *pdev)
{
	struct mtk_base_afe *afe = platform_get_drvdata(pdev);

	mt8365_afe_cleanup_debugfs(afe);
	mt8365_afe_disable_top_cg(afe, MT8365_TOP_CG_AFE);

	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		mt8365_afe_runtime_suspend(&pdev->dev);

	return 0;
}

static const struct of_device_id mt8365_afe_pcm_dt_match[] = {
	{ .compatible = "mediatek,mt8365-afe-pcm", },
	{ }
};
MODULE_DEVICE_TABLE(of, mt8365_afe_pcm_dt_match);

static const struct dev_pm_ops mt8365_afe_pm_ops = {
	SET_RUNTIME_PM_OPS(mt8365_afe_dev_runtime_suspend,
			   mt8365_afe_dev_runtime_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(mt8365_afe_suspend,
				mt8365_afe_resume)
};

static struct platform_driver mt8365_afe_pcm_driver = {
	.driver = {
		   .name = "mt8365-afe-pcm",
		   .of_match_table = mt8365_afe_pcm_dt_match,
		   .pm = &mt8365_afe_pm_ops,
	},
	.probe = mt8365_afe_pcm_dev_probe,
	.remove = mt8365_afe_pcm_dev_remove,
};

module_platform_driver(mt8365_afe_pcm_driver);

MODULE_DESCRIPTION("Mediatek ALSA SoC AFE platform driver");
MODULE_AUTHOR("Jia Zeng <jia.zeng@mediatek.com>");
MODULE_LICENSE("GPL v2");
