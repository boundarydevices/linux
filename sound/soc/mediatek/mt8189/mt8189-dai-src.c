// SPDX-License-Identifier: GPL-2.0
/*
 *  MediaTek ALSA SoC Audio DAI SRC Control
 *
 *  Copyright (c) 2024 MediaTek Inc.
 *  Author: Zeyuan Chen <zeyuan.chen@mediatek.com>
 */

#include <linux/regmap.h>
#include <sound/pcm_params.h>
#include "mt8189-afe-common.h"
#include "mt8189-interconnection.h"
#include "mt8189-afe-clk.h"

#define GSRC_REG

#define HW_SRC_0_EN_W_NAME "HW_SRC_0_Enable"
#define HW_SRC_1_EN_W_NAME "HW_SRC_1_Enable"
#define HW_SRC_2_EN_W_NAME "HW_SRC_2_Enable"
#define HW_SRC_3_EN_W_NAME "HW_SRC_3_Enable"
#define HW_SRC_4_EN_W_NAME "HW_SRC_4_Enable"

struct mtk_afe_src_priv {
	int dl_rate;
	int ul_rate;
};

static const unsigned int src_iir_coeff_32_to_16[] = {
	0x1bd356f3, 0x012e014f, 0x1bd356f3, 0x081be0a6, 0xe28e2407, 0x00000002,
	0x0d7d8ee8, 0x01b9274d, 0x0d7d8ee8, 0x09857a7b, 0xe4cae309, 0x00000002,
	0x0c999cbe, 0x038e89c5, 0x0c999cbe, 0x0beae5bc, 0xe7ded2a4, 0x00000002,
	0x0b4b6e2c, 0x061cd206, 0x0b4b6e2c, 0x0f6a2551, 0xec069422, 0x00000002,
	0x13ad5974, 0x129397e7, 0x13ad5974, 0x13d3c166, 0xf11cacb8, 0x00000002,
	0x126195d4, 0x1b259a6c, 0x126195d4, 0x184cdd94, 0xf634a151, 0x00000002,
	0x092aa1ea, 0x11add077, 0x092aa1ea, 0x3682199e, 0xf31b28fc, 0x00000001,
	0x0e09b91b, 0x0010b76f, 0x0e09b91b, 0x0f0e2575, 0xc19d364a, 0x00000001
};

static const unsigned int src_iir_coeff_44_to_16[] = {
	0x0c4deacd, 0xf5b3be35, 0x0c4deacd, 0x20349d1f, 0xe0b9a80d, 0x00000002,
	0x0c5dbbaa, 0xf6157998, 0x0c5dbbaa, 0x200c143d, 0xe25209ea, 0x00000002,
	0x0a9de1bd, 0xf85ee460, 0x0a9de1bd, 0x206099de, 0xe46a166c, 0x00000002,
	0x081f9a34, 0xfb7ffe47, 0x081f9a34, 0x212dd0f7, 0xe753c9ab, 0x00000002,
	0x0a6f9ddb, 0xfd863e9e, 0x0a6f9ddb, 0x226bd8a2, 0xeb2ead0b, 0x00000002,
	0x05497d0e, 0x01ebd7f0, 0x05497d0e, 0x23eba2f6, 0xef958aff, 0x00000002,
	0x008e7c5f, 0x00be6aad, 0x008e7c5f, 0x4a74b30a, 0xe6b0319a, 0x00000001,
	0x00000000, 0x38f3c5aa, 0x38f3c5aa, 0x012e1306, 0x00000000, 0x00000006
};

static const unsigned int src_iir_coeff_44_to_32[] = {

	0x0db45c84, 0x1113e68a, 0x0db45c84, 0xdf58fbd3, 0xe0e51ba2, 0x00000002,
	0x0e0c4d8f, 0x11eaf5ef, 0x0e0c4d8f, 0xe11e9264, 0xe2da4b80, 0x00000002,
	0x0cf2558c, 0x1154c11a, 0x0cf2558c, 0xe41c6288, 0xe570c517, 0x00000002,
	0x0b5132d7, 0x10545ecd, 0x0b5132d7, 0xe8e2e944, 0xe92f8dc6, 0x00000002,
	0x1234ffbb, 0x1cfba5c7, 0x1234ffbb, 0xf00653e0, 0xee9406e3, 0x00000002,
	0x0cfd073a, 0x170277ad, 0x0cfd073a, 0xf96e16e7, 0xf59562f9, 0x00000002,
	0x08506c2b, 0x1011cd72, 0x08506c2b, 0x164a9eae, 0xe4203311, 0xffffffff,
	0x00000000, 0x3d58af1e, 0x3d58af1e, 0x001bee13, 0x00000000, 0x00000007
};

static const unsigned int src_iir_coeff_48_to_16[] = {
	0x0296a398, 0xfd69dca0, 0x0296a398, 0x209438ae, 0xe01ff8bd, 0x00000002,
	0x0f4ff31a, 0xf0d6d390, 0x0f4ff31a, 0x209bc955, 0xe076c324, 0x00000002,
	0x0e848ff6, 0xf1fe6347, 0x0e848ff6, 0x20cfd5ae, 0xe12123ee, 0x00000002,
	0x14852eaf, 0xed794a1e, 0x14852eaf, 0x21503c83, 0xe28b3223, 0x00000002,
	0x1362223b, 0xf17676c3, 0x1362223b, 0x225be0ce, 0xe56963fa, 0x00000002,
	0x097f5e31, 0xfca98852, 0x097f5e31, 0x24310c19, 0xea69523c, 0x00000002,
	0x0698d721, 0x04abec68, 0x0698d721, 0x4ced8edd, 0xe134d677, 0x00000001,
	0x00000000, 0x3aebe58f, 0x3aebe58f, 0x04f3b027, 0x00000000, 0x00000004,
};

static const unsigned int src_iir_coeff_48_to_32[] = {
	0x0eca2fa9, 0x0f2b0cd3, 0x0eca2fa9, 0xf50313ef, 0xf15857a7, 0x00000003,
	0x0ee239a9, 0x1045115c, 0x0ee239a9, 0xec9f2976, 0xe5090807, 0x00000002,
	0x0ec57a45, 0x11d000f7, 0x0ec57a45, 0xf0bb67bb, 0xe84c86de, 0x00000002,
	0x0e85ba7e, 0x13ee7e9a, 0x0e85ba7e, 0xf6c74ebb, 0xecdba82c, 0x00000002,
	0x1cba1ac9, 0x2da90ada, 0x1cba1ac9, 0xfecba589, 0xf2c756e1, 0x00000002,
	0x0f79dec4, 0x1c27f5e0, 0x0f79dec4, 0x03c44399, 0xfc96c6aa, 0x00000003,
	0x1104a702, 0x21a72c89, 0x1104a702, 0x1b6a6fb8, 0xfb5ee0f2, 0x00000001,
	0x0622fc30, 0x061a0c67, 0x0622fc30, 0xe88911f2, 0xe0da327a, 0x00000002
};

static const unsigned int src_iir_coeff_48_to_44[] = {
	0x04c9f583, 0x09432e05, 0x04c9f583, 0xe2110f3c, 0xf02e6fc0, 0x00000003,
	0x07ba6f6a, 0x0efa321a, 0x07ba6f6a, 0xe28bbeec, 0xf0961f38, 0x00000003,
	0x078c0697, 0x0eaf26e3, 0x078c0697, 0xe34c6da5, 0xf1252a3b, 0x00000003,
	0x0740313a, 0x0e309947, 0x0740313a, 0xe493cc7c, 0xf20a3c0c, 0x00000003,
	0x0d782eb5, 0x1a8c0013, 0x0d782eb5, 0xe6e46d5d, 0xf39f3526, 0x00000003,
	0x0b9857c6, 0x17043894, 0x0b9857c6, 0xeb3b4422, 0xf68fd3be, 0x00000003,
	0x0444fe1c, 0x08857bff, 0x0444fe1c, 0xc9c6b7ed, 0xedbd865b, 0x00000001,
	0x00000000, 0x7fffffff, 0x7fffffff, 0xffbc6f93, 0x00000000, 0x00000007
};

static const unsigned int src_iir_coeff_96_to_16[] = {
	0x05c89f29, 0xf6443184, 0x05c89f29, 0x1bbe0f00, 0xf034bf19, 0x00000003,
	0x05e47be3, 0xf6284bfe, 0x05e47be3, 0x1b73d610, 0xf0a9a268, 0x00000003,
	0x09eb6c29, 0xefbc8df5, 0x09eb6c29, 0x365264ff, 0xe286ce76, 0x00000002,
	0x0741f28e, 0xf492d155, 0x0741f28e, 0x35a08621, 0xe4320cfe, 0x00000002,
	0x087cdc22, 0xf3daa1c7, 0x087cdc22, 0x34c55ef0, 0xe6664705, 0x00000002,
	0x038022af, 0xfc43da62, 0x038022af, 0x33d2b188, 0xe8e92eb8, 0x00000002,
	0x001de8ed, 0x0001bd74, 0x001de8ed, 0x33061aa8, 0xeb0d6ae7, 0x00000002,
	0x00000000, 0x3abd8743, 0x3abd8743, 0x032b3f7f, 0x00000000, 0x00000005
};

static const unsigned int src_iir_coeff_96_to_44[] = {
	0x1b4feb25, 0xfa1874df, 0x1b4feb25, 0x0fc84364, 0xe27e7427, 0x00000002,
	0x0d22ad1f, 0xfe465ea8, 0x0d22ad1f, 0x10d89ab2, 0xe4aa760e, 0x00000002,
	0x0c17b497, 0x004c9a14, 0x0c17b497, 0x12ba36ef, 0xe7a11513, 0x00000002,
	0x0a968b87, 0x031b65c2, 0x0a968b87, 0x157c39d1, 0xeb9561ce, 0x00000002,
	0x11cea26a, 0x0d025bcc, 0x11cea26a, 0x18ef4a32, 0xf05a2342, 0x00000002,
	0x0fe5d188, 0x156af55c, 0x0fe5d188, 0x1c6234df, 0xf50cd288, 0x00000002,
	0x07a1ea25, 0x0e900dd7, 0x07a1ea25, 0x3d441ae6, 0xf0314c15, 0x00000001,
	0x0dd3517a, 0xfc7f1621, 0x0dd3517a, 0x1ee4972a, 0xc193ad77, 0x00000001
};

static unsigned int mtk_get_src_freq_mode(struct mtk_base_afe *afe, int rate)
{
	switch (rate) {
	case 8000:
		return 0x00050000;
	case 11025:
		return 0x0006E400;
	case 12000:
		return 0x00078000;
	case 16000:
		return 0x000A0000;
	case 22050:
		return 0x000DC800;
	case 24000:
		return 0x000F0000;
	case 32000:
		return 0x00140000;
	case 44100:
		return 0x001B9000;
	case 48000:
		return 0x001E0000;
	case 88200:
		return 0x00372000;
	case 96000:
		return 0x003C0000;
	case 176400:
		return 0x006E4000;
	case 192000:
		return 0x00780000;
	case 352800:
		return 0x00DC8000;
	case 384000:
		return 0x00F00000;
	default:
		dev_info(afe->dev, "%s(), rate %d invalid!!!\n",
			 __func__, rate);
		return 0;
	}
}

const unsigned int *get_iir_coeff(unsigned int rate_in,
				  unsigned int rate_out,
				  unsigned int *param_num)
{
	if ((rate_in == 32000 && rate_out == 16000) ||
		(rate_in == 96000 && rate_out == 48000)) {
		*param_num = ARRAY_SIZE(src_iir_coeff_32_to_16);
		return src_iir_coeff_32_to_16;
	} else if (rate_in == 44100 && rate_out == 16000) {
		*param_num = ARRAY_SIZE(src_iir_coeff_44_to_16);
		return src_iir_coeff_44_to_16;
	} else if (rate_in == 44100 && rate_out == 32000) {
		*param_num = ARRAY_SIZE(src_iir_coeff_44_to_32);
		return src_iir_coeff_44_to_32;
	} else if ((rate_in == 48000 && rate_out == 16000) ||
		   (rate_in == 96000 && rate_out == 32000)) {
		*param_num = ARRAY_SIZE(src_iir_coeff_48_to_16);
		return src_iir_coeff_48_to_16;
	} else if (rate_in == 48000 && rate_out == 32000) {
		*param_num = ARRAY_SIZE(src_iir_coeff_48_to_32);
		return src_iir_coeff_48_to_32;
	} else if (rate_in == 48000 && rate_out == 44100) {
		*param_num = ARRAY_SIZE(src_iir_coeff_48_to_44);
		return src_iir_coeff_48_to_44;
	} else if (rate_in == 96000 && rate_out == 16000) {
		*param_num = ARRAY_SIZE(src_iir_coeff_96_to_16);
		return src_iir_coeff_96_to_16;
	} else if ((rate_in == 96000 && rate_out == 44100) ||
		   (rate_in == 48000 && rate_out == 22050)) {
		*param_num = ARRAY_SIZE(src_iir_coeff_96_to_44);
		return src_iir_coeff_96_to_44;
	}

	*param_num = 0;
	return NULL;
}

//#define DEBUG_COEFF
static int mtk_set_src_0_param(struct mtk_base_afe *afe, int id)
{
#ifdef GSRC_REG
	struct mt8189_afe_private *afe_priv = afe->platform_priv;
	struct mtk_afe_src_priv *src_priv = afe_priv->dai_priv[id];
	unsigned int iir_coeff_num;
	unsigned int iir_stage;
	int rate_in = src_priv->dl_rate;
	int rate_out = src_priv->ul_rate;
	unsigned int out_freq_mode = mtk_get_src_freq_mode(afe,
				     rate_out);
	unsigned int in_freq_mode = mtk_get_src_freq_mode(afe,
				    rate_in);
	regmap_write(afe->regmap,
		     AFE_GASRC0_NEW_CON6, 0x0);

	/* set out freq mode */
	regmap_update_bits(afe->regmap, AFE_GASRC0_NEW_CON2,
			   ASM_FREQ_1_MASK_SFT,
			   out_freq_mode << ASM_FREQ_1_SFT);

	/* set in freq mode */
	regmap_update_bits(afe->regmap, AFE_GASRC0_NEW_CON3,
			   ASM_FREQ_2_MASK_SFT,
			   in_freq_mode << ASM_FREQ_2_SFT);

	regmap_write(afe->regmap,
		     AFE_GASRC0_NEW_CON7, 0x00001fbd);
	regmap_write(afe->regmap,
		     AFE_GASRC0_NEW_CON0, 0x00000000);

	/* set iir if in_rate > out_rate */
	if (rate_in > rate_out) {
		int i;
#ifdef DEBUG_COEFF
		int reg_val;
#endif
		const unsigned int *iir_coeff = get_iir_coeff(rate_in, rate_out,
						&iir_coeff_num);

		if (iir_coeff_num == 0 || !iir_coeff) {
			dev_err(afe->dev, "%s(), iir coeff error\n",
				__func__);
			return -EINVAL;
		}

		/* COEFF_SRAM_CTRL */
		regmap_update_bits(afe->regmap, AFE_GASRC0_NEW_CON0,
				   COEFF_SRAM_CTRL_MASK_SFT,
				   0x1 << COEFF_SRAM_CTRL_SFT);
#ifdef DEBUG_COEFF
		regmap_read(afe->regmap, AFE_GASRC0_NEW_CON0,
			    &reg_val);

		dev_info(afe->dev, "%s(), AFE_GASRC0_NEW_CON0 0x%x\n",
			 __func__, reg_val);
#endif
		/* Clear coeff history to r/w coeff from the first position */
		regmap_update_bits(afe->regmap, AFE_GASRC0_NEW_CON11,
				   COEFF_SRAM_ADR_MASK_SFT,
				   0x0);
		/* Write SRC coeff, should not read the reg during write */
		for (i = 0; i < iir_coeff_num; i++) {
			regmap_write(afe->regmap, AFE_GASRC0_NEW_CON10,
				     iir_coeff[i]);
#ifdef DEBUG_COEFF
			dev_info(afe->dev, "%s(), write 10 coeff = 0x%x\n",
				 __func__, iir_coeff[i]);
#endif
		}
#ifdef DEBUG_COEFF
		regmap_write(afe->regmap, AFE_GASRC0_NEW_CON11, 0x0);
		regmap_read(afe->regmap, AFE_GASRC0_NEW_CON11,
				    &reg_val);
		dev_info(afe->dev, "%s(), i = %d, before debug  read AFE_GASRC0_NEW_CON11 = 0x%x\n",
				 __func__, i, reg_val);
		for (i = 0; i < iir_coeff_num; i++) {
			regmap_read(afe->regmap, AFE_GASRC0_NEW_CON10,
				    &reg_val);
			dev_info(afe->dev, "%s(), i = %d, debug AFE_GASRC0_NEW_CON10 = 0x%x\n",
				 __func__, i, reg_val);
			regmap_read(afe->regmap, AFE_GASRC0_NEW_CON11,
				    &reg_val);
			dev_info(afe->dev, "%s(), i = %d, debug AFE_GASRC0_NEW_CON11 = 0x%x\n",
				 __func__, i, reg_val);
		}
#endif
		/* disable sram access */
		regmap_update_bits(afe->regmap, AFE_GASRC0_NEW_CON0,
				   COEFF_SRAM_CTRL_MASK_SFT,
				   0x0);
		/* CHSET_IIR_STAGE */
		iir_stage = (iir_coeff_num / 6) - 1;
		regmap_update_bits(afe->regmap, AFE_GASRC0_NEW_CON0,
				   CHSET0_IIR_STAGE_MASK_SFT,
				   iir_stage << CHSET0_IIR_STAGE_SFT);
		/* CHSET_IIR_EN */
		regmap_update_bits(afe->regmap, AFE_GASRC0_NEW_CON0,
				   CHSET0_IIR_EN_MASK_SFT,
				   0x1 << CHSET0_IIR_EN_SFT);
	} else {
		/* CHSET_IIR_EN off */
		regmap_update_bits(afe->regmap, AFE_GASRC0_NEW_CON0,
				   CHSET0_IIR_EN_MASK_SFT,
				   0x0);
	}
#endif

	return 0;
}

static int mtk_set_src_1_param(struct mtk_base_afe *afe, int id)
{
	struct mt8189_afe_private *afe_priv = afe->platform_priv;
	struct mtk_afe_src_priv *src_priv = afe_priv->dai_priv[id];
	unsigned int iir_coeff_num;
	unsigned int iir_stage;
	int rate_in = src_priv->dl_rate;
	int rate_out = src_priv->ul_rate;
	unsigned int out_freq_mode = mtk_get_src_freq_mode(afe,
				     rate_out);
	unsigned int in_freq_mode = mtk_get_src_freq_mode(afe,
				    rate_in);
	regmap_write(afe->regmap,
		     AFE_GASRC1_NEW_CON6, 0x0);

	/* set out freq mode */
	regmap_update_bits(afe->regmap, AFE_GASRC1_NEW_CON2,
			   ASM_FREQ_1_MASK_SFT,
			   out_freq_mode << ASM_FREQ_1_SFT);

	/* set in freq mode */
	regmap_update_bits(afe->regmap, AFE_GASRC1_NEW_CON3,
			   ASM_FREQ_2_MASK_SFT,
			   in_freq_mode << ASM_FREQ_2_SFT);

	regmap_write(afe->regmap,
		     AFE_GASRC1_NEW_CON7, 0x00001fbd);
	regmap_write(afe->regmap,
		     AFE_GASRC1_NEW_CON0, 0x00000000);

	/* set iir if in_rate > out_rate */
	if (rate_in > rate_out) {
		int i;
#ifdef DEBUG_COEFF
		int reg_val;
#endif
		const unsigned int *iir_coeff = get_iir_coeff(rate_in, rate_out,
						&iir_coeff_num);

		if (iir_coeff_num == 0 || !iir_coeff) {
			dev_err(afe->dev, "%s(), iir coeff error\n",
				__func__);
			return -EINVAL;
		}

		/* COEFF_SRAM_CTRL */
		regmap_update_bits(afe->regmap, AFE_GASRC1_NEW_CON0,
				   COEFF_SRAM_CTRL_MASK_SFT,
				   0x1 << COEFF_SRAM_CTRL_SFT);
		/* Clear coeff history to r/w coeff from the first position */
		regmap_update_bits(afe->regmap, AFE_GASRC1_NEW_CON11,
				   COEFF_SRAM_ADR_MASK_SFT,
				   0x0);
		/* Write SRC coeff, should not read the reg during write */
		for (i = 0; i < iir_coeff_num; i++)
			regmap_write(afe->regmap, AFE_GASRC1_NEW_CON10,
				     iir_coeff[i]);

#ifdef DEBUG_COEFF
		regmap_update_bits(afe->regmap, AFE_GASRC1_NEW_CON11,
				   COEFF_SRAM_ADR_MASK_SFT,
				   0x0);

		for (i = 0; i < iir_coeff_num; i++) {
			regmap_read(afe->regmap, AFE_GASRC1_NEW_CON10,
				    &reg_val);
			dev_info(afe->dev, "%s(), i = %d, coeff = 0x%x\n",
				 __func__, i, reg_val);
		}
#endif
		/* disable sram access */
		regmap_update_bits(afe->regmap, AFE_GASRC1_NEW_CON0,
				   COEFF_SRAM_CTRL_MASK_SFT,
				   0x0);
		/* CHSET_IIR_STAGE */
		iir_stage = (iir_coeff_num / 6) - 1;
		regmap_update_bits(afe->regmap, AFE_GASRC1_NEW_CON0,
				   CHSET0_IIR_STAGE_MASK_SFT,
				   iir_stage << CHSET0_IIR_STAGE_SFT);
		/* CHSET_IIR_EN */
		regmap_update_bits(afe->regmap, AFE_GASRC1_NEW_CON0,
				   CHSET0_IIR_EN_MASK_SFT,
				   0x1 << CHSET0_IIR_EN_SFT);
	} else {
		/* CHSET_IIR_EN off */
		regmap_update_bits(afe->regmap, AFE_GASRC1_NEW_CON0,
				   CHSET0_IIR_EN_MASK_SFT, 0x0);
	}

	return 0;
}

static int mtk_set_src_2_param(struct mtk_base_afe *afe, int id)
{
	struct mt8189_afe_private *afe_priv = afe->platform_priv;
	struct mtk_afe_src_priv *src_priv = afe_priv->dai_priv[id];
	unsigned int iir_coeff_num;
	unsigned int iir_stage;
	int rate_in = src_priv->dl_rate;
	int rate_out = src_priv->ul_rate;
	unsigned int out_freq_mode = mtk_get_src_freq_mode(afe,
				     rate_out);
	unsigned int in_freq_mode = mtk_get_src_freq_mode(afe,
				    rate_in);

	regmap_write(afe->regmap,
		     AFE_GASRC2_NEW_CON6, 0x0);
	/* set out freq mode */
	regmap_update_bits(afe->regmap, AFE_GASRC2_NEW_CON2,
			   ASM_FREQ_1_MASK_SFT,
			   out_freq_mode << ASM_FREQ_1_SFT);

	/* set in freq mode */
	regmap_update_bits(afe->regmap, AFE_GASRC2_NEW_CON3,
			   ASM_FREQ_2_MASK_SFT,
			   in_freq_mode << ASM_FREQ_2_SFT);

	regmap_write(afe->regmap,
		     AFE_GASRC2_NEW_CON7, 0x00001fbd);
	regmap_write(afe->regmap,
		     AFE_GASRC2_NEW_CON0, 0x00000000);

	/* set iir if in_rate > out_rate */
	if (rate_in > rate_out) {
		int i;
#ifdef DEBUG_COEFF
		int reg_val;
#endif
		const unsigned int *iir_coeff = get_iir_coeff(rate_in, rate_out,
						&iir_coeff_num);

		if (iir_coeff_num == 0 || !iir_coeff) {
			dev_err(afe->dev, "%s(), iir coeff error\n",
				__func__);
			return -EINVAL;
		}

		/* COEFF_SRAM_CTRL */
		regmap_update_bits(afe->regmap, AFE_GASRC2_NEW_CON0,
				   COEFF_SRAM_CTRL_MASK_SFT,
				   0x1 << COEFF_SRAM_CTRL_SFT);
		/* Clear coeff history to r/w coeff from the first position */
		regmap_update_bits(afe->regmap, AFE_GASRC2_NEW_CON11,
				   COEFF_SRAM_ADR_MASK_SFT,
				   0x0);
		/* Write SRC coeff, should not read the reg during write */
		for (i = 0; i < iir_coeff_num; i++)
			regmap_write(afe->regmap, AFE_GASRC2_NEW_CON10,
				     iir_coeff[i]);

#ifdef DEBUG_COEFF
		regmap_update_bits(afe->regmap, AFE_GASRC2_NEW_CON11,
				   COEFF_SRAM_ADR_MASK_SFT,
				   0x0);

		for (i = 0; i < iir_coeff_num; i++) {
			regmap_read(afe->regmap, AFE_GASRC2_NEW_CON10,
				    &reg_val);
			dev_info(afe->dev, "%s(), i = %d, coeff = 0x%x\n",
				 __func__, i, reg_val);
		}
#endif
		/* disable sram access */
		regmap_update_bits(afe->regmap, AFE_GASRC2_NEW_CON0,
				   COEFF_SRAM_CTRL_MASK_SFT,
				   0x0);
		/* CHSET_IIR_STAGE */
		iir_stage = (iir_coeff_num / 6) - 1;
		regmap_update_bits(afe->regmap, AFE_GASRC2_NEW_CON0,
				   CHSET0_IIR_STAGE_MASK_SFT,
				   iir_stage << CHSET0_IIR_STAGE_SFT);
		/* CHSET_IIR_EN */
		regmap_update_bits(afe->regmap, AFE_GASRC2_NEW_CON0,
				   CHSET0_IIR_EN_MASK_SFT,
				   0x1 << CHSET0_IIR_EN_SFT);
	} else {
		/* CHSET_IIR_EN off */
		regmap_update_bits(afe->regmap, AFE_GASRC2_NEW_CON0,
				   CHSET0_IIR_EN_MASK_SFT,
				   0x0);
	}

	return 0;
}

static int mtk_set_src_3_param(struct mtk_base_afe *afe, int id)
{
	struct mt8189_afe_private *afe_priv = afe->platform_priv;
	struct mtk_afe_src_priv *src_priv = afe_priv->dai_priv[id];
	unsigned int iir_coeff_num;
	unsigned int iir_stage;
	int rate_in = src_priv->dl_rate;
	int rate_out = src_priv->ul_rate;
	unsigned int out_freq_mode = mtk_get_src_freq_mode(afe,
				     rate_out);
	unsigned int in_freq_mode = mtk_get_src_freq_mode(afe,
				    rate_in);

	regmap_write(afe->regmap,
		     AFE_GASRC3_NEW_CON6, 0x0);

	/* set out freq mode */
	regmap_update_bits(afe->regmap, AFE_GASRC3_NEW_CON2,
			   ASM_FREQ_1_MASK_SFT,
			   out_freq_mode << ASM_FREQ_1_SFT);

	/* set in freq mode */
	regmap_update_bits(afe->regmap, AFE_GASRC3_NEW_CON3,
			   ASM_FREQ_2_MASK_SFT,
			   in_freq_mode << ASM_FREQ_2_SFT);

	regmap_write(afe->regmap,
		     AFE_GASRC3_NEW_CON7, 0x00001fbd);
	regmap_write(afe->regmap,
		     AFE_GASRC3_NEW_CON0, 0x0);

	/* set iir if in_rate > out_rate */
	if (rate_in > rate_out) {
		int i;
#ifdef DEBUG_COEFF
		int reg_val;
#endif
		const unsigned int *iir_coeff = get_iir_coeff(rate_in, rate_out,
						&iir_coeff_num);

		if (iir_coeff_num == 0 || !iir_coeff) {
			dev_err(afe->dev, "%s(), iir coeff error\n",
				__func__);
			return -EINVAL;
		}

		/* COEFF_SRAM_CTRL */
		regmap_update_bits(afe->regmap, AFE_GASRC3_NEW_CON0,
				   COEFF_SRAM_CTRL_MASK_SFT,
				   0x1 << COEFF_SRAM_CTRL_SFT);
		/* Clear coeff history to r/w coeff from the first position */
		regmap_update_bits(afe->regmap, AFE_GASRC3_NEW_CON11,
				   COEFF_SRAM_ADR_MASK_SFT,
				   0x0);
		/* Write SRC coeff, should not read the reg during write */
		for (i = 0; i < iir_coeff_num; i++)
			regmap_write(afe->regmap, AFE_GASRC3_NEW_CON10,
				     iir_coeff[i]);

#ifdef DEBUG_COEFF
		regmap_update_bits(afe->regmap, AFE_GASRC3_NEW_CON11,
				   COEFF_SRAM_ADR_MASK_SFT,
				   0x0);

		for (i = 0; i < iir_coeff_num; i++) {
			regmap_read(afe->regmap, AFE_GASRC3_NEW_CON10,
				    &reg_val);
			dev_info(afe->dev, "%s(), i = %d, coeff = 0x%x\n",
				 __func__, i, reg_val);
		}
#endif
		/* disable sram access */
		regmap_update_bits(afe->regmap, AFE_GASRC3_NEW_CON0,
				   COEFF_SRAM_CTRL_MASK_SFT,
				   0x0);
		/* CHSET_IIR_STAGE */
		iir_stage = (iir_coeff_num / 6) - 1;
		regmap_update_bits(afe->regmap, AFE_GASRC3_NEW_CON0,
				   CHSET0_IIR_STAGE_MASK_SFT,
				   iir_stage << CHSET0_IIR_STAGE_SFT);
		/* CHSET_IIR_EN */
		regmap_update_bits(afe->regmap, AFE_GASRC3_NEW_CON0,
				   CHSET0_IIR_EN_MASK_SFT,
				   0x1 << CHSET0_IIR_EN_SFT);
	} else {
		/* CHSET_IIR_EN off */
		regmap_update_bits(afe->regmap, AFE_GASRC3_NEW_CON0,
				   CHSET0_IIR_EN_MASK_SFT, 0x0);
	}

	return 0;
}

static int mtk_set_src_4_param(struct mtk_base_afe *afe, unsigned int id)
{
	struct mt8189_afe_private *afe_priv = afe->platform_priv;
	struct mtk_afe_src_priv *src_priv = afe_priv->dai_priv[id];
	unsigned int iir_coeff_num;
	unsigned int iir_stage;
	int rate_in = src_priv->dl_rate;
	int rate_out = src_priv->ul_rate;
	unsigned int out_freq_mode = mtk_get_src_freq_mode(afe,
				     rate_out);
	unsigned int in_freq_mode = mtk_get_src_freq_mode(afe,
				    rate_in);

	regmap_write(afe->regmap,
		     AFE_GASRC3_NEW_CON6, 0x0);

	/* set out freq mode */
	regmap_update_bits(afe->regmap, AFE_GASRC4_NEW_CON2,
			   ASM_FREQ_1_MASK_SFT,
			   out_freq_mode << ASM_FREQ_1_SFT);

	/* set in freq mode */
	regmap_update_bits(afe->regmap, AFE_GASRC4_NEW_CON3,
			   ASM_FREQ_2_MASK_SFT,
			   in_freq_mode << ASM_FREQ_2_SFT);

	regmap_write(afe->regmap,
		     AFE_GASRC4_NEW_CON7, 0x00001fbd);
	regmap_write(afe->regmap,
		     AFE_GASRC4_NEW_CON0, 0x0);

	/* set iir if in_rate > out_rate */
	if (rate_in > rate_out) {
		int i;
#ifdef DEBUG_COEFF
		int reg_val;
#endif
		const unsigned int *iir_coeff = get_iir_coeff(rate_in, rate_out,
						&iir_coeff_num);

		if (iir_coeff_num == 0 || !iir_coeff) {
			dev_err(afe->dev, "%s(), iir coeff error\n",
				__func__);
			return -EINVAL;
		}

		/* COEFF_SRAM_CTRL */
		regmap_update_bits(afe->regmap, AFE_GASRC4_NEW_CON0,
				   COEFF_SRAM_CTRL_MASK_SFT,
				   0x1 << COEFF_SRAM_CTRL_SFT);
		/* Clear coeff history to r/w coeff from the first position */
		regmap_update_bits(afe->regmap, AFE_GASRC4_NEW_CON11,
				   COEFF_SRAM_ADR_MASK_SFT,
				   0x0);
		/* Write SRC coeff, should not read the reg during write */
		for (i = 0; i < iir_coeff_num; i++)
			regmap_write(afe->regmap, AFE_GASRC4_NEW_CON10,
				     iir_coeff[i]);

#ifdef DEBUG_COEFF
		regmap_update_bits(afe->regmap, AFE_GASRC4_NEW_CON11,
				   COEFF_SRAM_ADR_MASK_SFT,
				   0x0);

		for (i = 0; i < iir_coeff_num; i++) {
			regmap_read(afe->regmap, AFE_GASRC4_NEW_CON10,
				    &reg_val);
			dev_info(afe->dev, "%s(), i = %d, coeff = 0x%x\n",
				 __func__, i, reg_val);
		}
#endif
		/* disable sram access */
		regmap_update_bits(afe->regmap, AFE_GASRC4_NEW_CON0,
				   COEFF_SRAM_CTRL_MASK_SFT,
				   0x0);
		/* CHSET_IIR_STAGE */
		iir_stage = (iir_coeff_num / 6) - 1;
		regmap_update_bits(afe->regmap, AFE_GASRC4_NEW_CON0,
				   CHSET0_IIR_STAGE_MASK_SFT,
				   iir_stage << CHSET0_IIR_STAGE_SFT);
		/* CHSET_IIR_EN */
		regmap_update_bits(afe->regmap, AFE_GASRC4_NEW_CON0,
				   CHSET0_IIR_EN_MASK_SFT,
				   0x1 << CHSET0_IIR_EN_SFT);
	} else {
		/* CHSET_IIR_EN off */
		regmap_update_bits(afe->regmap, AFE_GASRC4_NEW_CON0,
				   CHSET0_IIR_EN_MASK_SFT, 0x0);
	}

	return 0;
}

static int mtk_hw_src_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol,
			    int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8189_afe_private *afe_priv = afe->platform_priv;
	unsigned int id = 0;
	struct mtk_afe_src_priv *src_priv;
	unsigned int reg;

	if (strcmp(w->name, HW_SRC_0_EN_W_NAME) == 0)
		id = MT8189_DAI_SRC_0;
	else if (strcmp(w->name, HW_SRC_1_EN_W_NAME) == 0)
		id = MT8189_DAI_SRC_1;
	else if (strcmp(w->name, HW_SRC_2_EN_W_NAME) == 0)
		id = MT8189_DAI_SRC_2;
	else if (strcmp(w->name, HW_SRC_3_EN_W_NAME) == 0)
		id = MT8189_DAI_SRC_3;
	else
		id = MT8189_DAI_SRC_4;

	src_priv = afe_priv->dai_priv[id];

	dev_info(afe->dev, "%s(), name %s, event 0x%x, id %d, src_priv %p, dl_rate %d, ul_rate %d\n",
		 __func__,
		 w->name, event,
		 id, src_priv,
		 src_priv->dl_rate,
		 src_priv->ul_rate);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (id == MT8189_DAI_SRC_0)
			mtk_set_src_0_param(afe, id);
		else if (id == MT8189_DAI_SRC_1)
			mtk_set_src_1_param(afe, id);
		else if (id == MT8189_DAI_SRC_2)
			mtk_set_src_2_param(afe, id);
		else if (id == MT8189_DAI_SRC_3)
			mtk_set_src_3_param(afe, id);
		else
			mtk_set_src_4_param(afe, id);
		break;
	case SND_SOC_DAPM_POST_PMU:
		if (id == MT8189_DAI_SRC_0)
			reg = AFE_GASRC0_NEW_CON0;
		else if (id == MT8189_DAI_SRC_1)
			reg = AFE_GASRC1_NEW_CON0;
		else if (id == MT8189_DAI_SRC_2)
			reg = AFE_GASRC2_NEW_CON0;
		else if (id == MT8189_DAI_SRC_3)
			reg = AFE_GASRC3_NEW_CON0;
		else
			reg = AFE_GASRC4_NEW_CON0;
#ifdef GSRC_REG
		/* IFS_SEL */
		regmap_update_bits(afe->regmap, reg,
				   CHSET0_IFS_SEL_MASK_SFT,
				   0x2 << CHSET0_IFS_SEL_SFT);
		/* OFS_SEL */
		regmap_update_bits(afe->regmap, reg,
				   CHSET0_OFS_SEL_MASK_SFT,
				   0x1 << CHSET0_OFS_SEL_SFT);
		/* ASM_ON */
		regmap_update_bits(afe->regmap, reg,
				   ASM_ON_MASK_SFT,
				   0x1 << ASM_ON_SFT);
		/* CHSET_ON */
		regmap_update_bits(afe->regmap, reg,
				   CHSET_ON_MASK_SFT,
				   0x1 << CHSET_ON_SFT);
		/* CHSET_STR_CLR */
		regmap_update_bits(afe->regmap, reg,
				   CHSET_STR_CLR_MASK_SFT,
				   0x1 << CHSET_STR_CLR_SFT);
#endif
		break;
	case SND_SOC_DAPM_PRE_PMD:
		if (id == MT8189_DAI_SRC_0)
			reg = AFE_GASRC0_NEW_CON0;
		else if (id == MT8189_DAI_SRC_1)
			reg = AFE_GASRC1_NEW_CON0;
		else if (id == MT8189_DAI_SRC_2)
			reg = AFE_GASRC2_NEW_CON0;
		else if (id == MT8189_DAI_SRC_3)
			reg = AFE_GASRC3_NEW_CON0;
		else
			reg = AFE_GASRC4_NEW_CON0;

#ifdef GSRC_REG
		/* ASM_OFF */
		regmap_update_bits(afe->regmap, reg,
				   ASM_ON_MASK_SFT, 0x0);
		/* CHSET_OFF */
		regmap_update_bits(afe->regmap, reg,
				   CHSET_ON_MASK_SFT, 0x0);
		/* CHSET_STR_CLR */
		regmap_update_bits(afe->regmap, reg,
				   CHSET_STR_CLR_MASK_SFT, 0x0);
#endif
		break;
	default:
		break;
	}

	return 0;
}

/* dai component */
static const struct snd_kcontrol_new mtk_hw_src_0_in_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH1", AFE_CONN180_1,
				    I_DL0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1", AFE_CONN180_1,
				    I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN180_1,
				    I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH1", AFE_CONN180_1,
				    I_DL3_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH1", AFE_CONN180_1,
				    I_DL4_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH1", AFE_CONN180_1,
				    I_DL5_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH1", AFE_CONN180_1,
				    I_DL6_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH1", AFE_CONN180_1,
				    I_DL_24CH_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL24_CH1", AFE_CONN180_2,
				    I_DL24_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN180_0,
				    I_ADDA_UL_CH1, 1, 0),
};

static const struct snd_kcontrol_new mtk_hw_src_0_in_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH2", AFE_CONN181_1,
				    I_DL0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH2", AFE_CONN181_1,
				    I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN181_1,
				    I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH2", AFE_CONN181_1,
				    I_DL3_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH2", AFE_CONN181_1,
				    I_DL4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH2", AFE_CONN181_1,
				    I_DL5_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH2", AFE_CONN181_1,
				    I_DL6_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH2", AFE_CONN181_1,
				    I_DL_24CH_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL24_CH2", AFE_CONN181_2,
				    I_DL24_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN181_0,
				    I_ADDA_UL_CH2, 1, 0),
};

static const struct snd_kcontrol_new mtk_hw_src_1_in_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH1", AFE_CONN182_1,
				    I_DL0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1", AFE_CONN182_1,
				    I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN182_1,
				    I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH1", AFE_CONN182_1,
				    I_DL3_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH1", AFE_CONN182_1,
				    I_DL4_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH1", AFE_CONN182_1,
				    I_DL5_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH1", AFE_CONN182_1,
				    I_DL6_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH1", AFE_CONN182_1,
				    I_DL_24CH_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL24_CH1", AFE_CONN182_2,
				    I_DL24_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_GAIN1_OUT_CH1", AFE_CONN182_0,
				    I_GAIN1_OUT_CH1, 1, 0),
};

static const struct snd_kcontrol_new mtk_hw_src_1_in_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH2", AFE_CONN183_1,
				    I_DL0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH2", AFE_CONN183_1,
				    I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN183_1,
				    I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH2", AFE_CONN183_1,
				    I_DL3_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH2", AFE_CONN183_1,
				    I_DL4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH2", AFE_CONN183_1,
				    I_DL5_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH2", AFE_CONN183_1,
				    I_DL6_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH2", AFE_CONN183_1,
				    I_DL_24CH_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL24_CH2", AFE_CONN183_2,
				    I_DL24_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_GAIN1_OUT_CH2", AFE_CONN183_0,
				    I_GAIN1_OUT_CH2, 1, 0),
};

static const struct snd_kcontrol_new mtk_hw_src_2_in_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH1", AFE_CONN184_1,
				    I_DL0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1", AFE_CONN184_1,
				    I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN184_1,
				    I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH1", AFE_CONN184_1,
				    I_DL3_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH1", AFE_CONN184_1,
				    I_DL4_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH1", AFE_CONN184_1,
				    I_DL5_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH1", AFE_CONN184_1,
				    I_DL6_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH1", AFE_CONN184_1,
				    I_DL_24CH_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL24_CH1", AFE_CONN184_2,
				    I_DL24_CH1, 1, 0),
};

static const struct snd_kcontrol_new mtk_hw_src_2_in_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH2", AFE_CONN185_1,
				    I_DL0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH2", AFE_CONN185_1,
				    I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN185_1,
				    I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH2", AFE_CONN185_1,
				    I_DL3_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH2", AFE_CONN185_1,
				    I_DL4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH2", AFE_CONN185_1,
				    I_DL5_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH2", AFE_CONN185_1,
				    I_DL6_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH2", AFE_CONN185_1,
				    I_DL_24CH_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL24_CH2", AFE_CONN185_2,
				    I_DL24_CH2, 1, 0),
};

static const struct snd_kcontrol_new mtk_hw_src_3_in_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH1", AFE_CONN186_1,
				    I_DL0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1", AFE_CONN186_1,
				    I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN186_1,
				    I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH1", AFE_CONN186_1,
				    I_DL3_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH1", AFE_CONN186_1,
				    I_DL4_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH1", AFE_CONN186_1,
				    I_DL5_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH1", AFE_CONN186_1,
				    I_DL6_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH1", AFE_CONN186_1,
				    I_DL_24CH_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL24_CH1", AFE_CONN186_2,
				    I_DL24_CH1, 1, 0),
};

static const struct snd_kcontrol_new mtk_hw_src_3_in_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH2", AFE_CONN187_1,
				    I_DL0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH2", AFE_CONN187_1,
				    I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN187_1,
				    I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH2", AFE_CONN187_1,
				    I_DL3_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH2", AFE_CONN187_1,
				    I_DL4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH2", AFE_CONN187_1,
				    I_DL5_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH2", AFE_CONN187_1,
				    I_DL6_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH2", AFE_CONN187_1,
				    I_DL_24CH_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL24_CH2", AFE_CONN187_2,
				    I_DL24_CH2, 1, 0),
};

static const struct snd_kcontrol_new mtk_hw_src_4_in_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH1", AFE_CONN188_1,
				    I_DL0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1", AFE_CONN188_1,
				    I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN188_1,
				    I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH1", AFE_CONN188_1,
				    I_DL3_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH1", AFE_CONN188_1,
				    I_DL4_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH1", AFE_CONN188_1,
				    I_DL5_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH1", AFE_CONN188_1,
				    I_DL6_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH1", AFE_CONN188_1,
				    I_DL_24CH_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL24_CH1", AFE_CONN188_2,
				    I_DL24_CH1, 1, 0),
};

static const struct snd_kcontrol_new mtk_hw_src_4_in_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH2", AFE_CONN189_1,
				    I_DL0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH2", AFE_CONN189_1,
				    I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN189_1,
				    I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH2", AFE_CONN189_1,
				    I_DL3_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH2", AFE_CONN189_1,
				    I_DL4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH2", AFE_CONN189_1,
				    I_DL5_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH2", AFE_CONN189_1,
				    I_DL6_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH2", AFE_CONN189_1,
				    I_DL_24CH_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL24_CH2", AFE_CONN189_2,
				    I_DL24_CH2, 1, 0),
};

static const struct snd_soc_dapm_widget mtk_dai_src_widgets[] = {
	/* inter-connections */
	SND_SOC_DAPM_MIXER("HW_SRC_0_IN_CH1", SND_SOC_NOPM, 0, 0,
			   mtk_hw_src_0_in_ch1_mix,
			   ARRAY_SIZE(mtk_hw_src_0_in_ch1_mix)),
	SND_SOC_DAPM_MIXER("HW_SRC_0_IN_CH2", SND_SOC_NOPM, 0, 0,
			   mtk_hw_src_0_in_ch2_mix,
			   ARRAY_SIZE(mtk_hw_src_0_in_ch2_mix)),
	SND_SOC_DAPM_MIXER("HW_SRC_1_IN_CH1", SND_SOC_NOPM, 0, 0,
			   mtk_hw_src_1_in_ch1_mix,
			   ARRAY_SIZE(mtk_hw_src_1_in_ch1_mix)),
	SND_SOC_DAPM_MIXER("HW_SRC_1_IN_CH2", SND_SOC_NOPM, 0, 0,
			   mtk_hw_src_1_in_ch2_mix,
			   ARRAY_SIZE(mtk_hw_src_1_in_ch2_mix)),
	SND_SOC_DAPM_MIXER("HW_SRC_2_IN_CH1", SND_SOC_NOPM, 0, 0,
			   mtk_hw_src_2_in_ch1_mix,
			   ARRAY_SIZE(mtk_hw_src_2_in_ch1_mix)),
	SND_SOC_DAPM_MIXER("HW_SRC_2_IN_CH2", SND_SOC_NOPM, 0, 0,
			   mtk_hw_src_2_in_ch2_mix,
			   ARRAY_SIZE(mtk_hw_src_2_in_ch2_mix)),
	SND_SOC_DAPM_MIXER("HW_SRC_3_IN_CH1", SND_SOC_NOPM, 0, 0,
			   mtk_hw_src_3_in_ch1_mix,
			   ARRAY_SIZE(mtk_hw_src_3_in_ch1_mix)),
	SND_SOC_DAPM_MIXER("HW_SRC_3_IN_CH2", SND_SOC_NOPM, 0, 0,
			   mtk_hw_src_3_in_ch2_mix,
			   ARRAY_SIZE(mtk_hw_src_3_in_ch2_mix)),
	SND_SOC_DAPM_MIXER("HW_SRC_4_IN_CH1", SND_SOC_NOPM, 0, 0,
			   mtk_hw_src_4_in_ch1_mix,
			   ARRAY_SIZE(mtk_hw_src_4_in_ch1_mix)),
	SND_SOC_DAPM_MIXER("HW_SRC_4_IN_CH2", SND_SOC_NOPM, 0, 0,
			   mtk_hw_src_4_in_ch2_mix,
			   ARRAY_SIZE(mtk_hw_src_4_in_ch2_mix)),

	SND_SOC_DAPM_SUPPLY(HW_SRC_0_EN_W_NAME,
			    SND_SOC_NOPM, 0, 0,
			    mtk_hw_src_event,
			    SND_SOC_DAPM_PRE_PMU |
			    SND_SOC_DAPM_POST_PMU |
			    SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_SUPPLY(HW_SRC_1_EN_W_NAME,
			    SND_SOC_NOPM, 0, 0,
			    mtk_hw_src_event,
			    SND_SOC_DAPM_PRE_PMU |
			    SND_SOC_DAPM_POST_PMU |
			    SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_SUPPLY(HW_SRC_2_EN_W_NAME,
			    SND_SOC_NOPM, 0, 0,
			    mtk_hw_src_event,
			    SND_SOC_DAPM_PRE_PMU |
			    SND_SOC_DAPM_POST_PMU |
			    SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_SUPPLY(HW_SRC_3_EN_W_NAME,
			    SND_SOC_NOPM, 0, 0,
			    mtk_hw_src_event,
			    SND_SOC_DAPM_PRE_PMU |
			    SND_SOC_DAPM_POST_PMU |
			    SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_SUPPLY(HW_SRC_4_EN_W_NAME,
			    SND_SOC_NOPM, 0, 0,
			    mtk_hw_src_event,
			    SND_SOC_DAPM_PRE_PMU |
			    SND_SOC_DAPM_POST_PMU |
			    SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_INPUT("HW SRC 0 Out Endpoint"),
	SND_SOC_DAPM_INPUT("HW SRC 1 Out Endpoint"),
	SND_SOC_DAPM_INPUT("HW SRC 2 Out Endpoint"),
	SND_SOC_DAPM_INPUT("HW SRC 3 Out Endpoint"),
	SND_SOC_DAPM_INPUT("HW SRC 4 Out Endpoint"),
	SND_SOC_DAPM_OUTPUT("HW SRC 0 In Endpoint"),
	SND_SOC_DAPM_OUTPUT("HW SRC 1 In Endpoint"),
	SND_SOC_DAPM_OUTPUT("HW SRC 2 In Endpoint"),
	SND_SOC_DAPM_OUTPUT("HW SRC 3 In Endpoint"),
	SND_SOC_DAPM_OUTPUT("HW SRC 4 In Endpoint"),
};

static int mtk_afe_src_en_connect(struct snd_soc_dapm_widget *source,
				  struct snd_soc_dapm_widget *sink)
{
	struct snd_soc_dapm_widget *w = source;
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8189_afe_private *afe_priv = afe->platform_priv;
	struct mtk_afe_src_priv *src_priv;
	int ret = 0;

	if (strcmp(w->name, HW_SRC_0_EN_W_NAME) == 0)
		src_priv = afe_priv->dai_priv[MT8189_DAI_SRC_0];
	else if (strcmp(w->name, HW_SRC_1_EN_W_NAME) == 0)
		src_priv = afe_priv->dai_priv[MT8189_DAI_SRC_1];
	else if (strcmp(w->name, HW_SRC_2_EN_W_NAME) == 0)
		src_priv = afe_priv->dai_priv[MT8189_DAI_SRC_2];
	else if (strcmp(w->name, HW_SRC_3_EN_W_NAME) == 0)
		src_priv = afe_priv->dai_priv[MT8189_DAI_SRC_3];
	else
		src_priv = afe_priv->dai_priv[MT8189_DAI_SRC_4];

	ret = (src_priv->dl_rate > 0 && src_priv->ul_rate > 0) ? 1 : 0;
	if (ret)
		dev_info(afe->dev,
			 "%s(), source %s, sink %s, dl_rate %d, ul_rate %d\n",
			 __func__, source->name, sink->name,
			 src_priv->dl_rate, src_priv->ul_rate);

	return ret;
}

static int mtk_afe_src_apll_connect(struct snd_soc_dapm_widget *source,
				    struct snd_soc_dapm_widget *sink)
{
	struct snd_soc_dapm_widget *w = sink;
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8189_afe_private *afe_priv = afe->platform_priv;
	struct mtk_afe_src_priv *src_priv;
	int cur_apll;
	int need_apll;

	if (strcmp(w->name, HW_SRC_0_EN_W_NAME) == 0)
		src_priv = afe_priv->dai_priv[MT8189_DAI_SRC_0];
	else if (strcmp(w->name, HW_SRC_1_EN_W_NAME) == 0)
		src_priv = afe_priv->dai_priv[MT8189_DAI_SRC_1];
	else if (strcmp(w->name, HW_SRC_1_EN_W_NAME) == 0)
		src_priv = afe_priv->dai_priv[MT8189_DAI_SRC_2];
	else if (strcmp(w->name, HW_SRC_1_EN_W_NAME) == 0)
		src_priv = afe_priv->dai_priv[MT8189_DAI_SRC_3];
	else
		src_priv = afe_priv->dai_priv[MT8189_DAI_SRC_4];

	if (!src_priv) {
		dev_warn(afe->dev, "%s(), src_priv == NULL\n",
			 __func__);
		return 0;
	}

	/* which apll */
	cur_apll = mt8189_get_apll_by_name(afe, source->name);
	/* choose APLL from SRC IFS rate */
	need_apll = mt8189_get_apll_by_rate(afe, src_priv->ul_rate);

	return (need_apll == cur_apll) ? 1 : 0;
}

static const struct snd_soc_dapm_route mtk_dai_src_routes[] = {
	{"HW_SRC_0_IN_CH1", "DL0_CH1", "DL0"},
	{"HW_SRC_0_IN_CH2", "DL0_CH2", "DL0"},
	{"HW_SRC_1_IN_CH1", "DL0_CH1", "DL0"},
	{"HW_SRC_1_IN_CH2", "DL0_CH2", "DL0"},
	{"HW_SRC_2_IN_CH1", "DL0_CH1", "DL0"},
	{"HW_SRC_2_IN_CH2", "DL0_CH2", "DL0"},
	{"HW_SRC_3_IN_CH1", "DL0_CH1", "DL0"},
	{"HW_SRC_3_IN_CH2", "DL0_CH2", "DL0"},
	{"HW_SRC_4_IN_CH1", "DL0_CH1", "DL0"},
	{"HW_SRC_4_IN_CH2", "DL0_CH2", "DL0"},

	{"HW_SRC_0_IN_CH1", "DL1_CH1", "DL1"},
	{"HW_SRC_0_IN_CH2", "DL1_CH2", "DL1"},
	{"HW_SRC_1_IN_CH1", "DL1_CH1", "DL1"},
	{"HW_SRC_1_IN_CH2", "DL1_CH2", "DL1"},
	{"HW_SRC_2_IN_CH1", "DL1_CH1", "DL1"},
	{"HW_SRC_2_IN_CH2", "DL1_CH2", "DL1"},
	{"HW_SRC_3_IN_CH1", "DL1_CH1", "DL1"},
	{"HW_SRC_3_IN_CH2", "DL1_CH2", "DL1"},
	{"HW_SRC_4_IN_CH1", "DL1_CH1", "DL1"},
	{"HW_SRC_4_IN_CH2", "DL1_CH2", "DL1"},

	{"HW_SRC_0_IN_CH1", "DL2_CH1", "DL2"},
	{"HW_SRC_0_IN_CH2", "DL2_CH2", "DL2"},
	{"HW_SRC_1_IN_CH1", "DL2_CH1", "DL2"},
	{"HW_SRC_1_IN_CH2", "DL2_CH2", "DL2"},
	{"HW_SRC_2_IN_CH1", "DL2_CH1", "DL2"},
	{"HW_SRC_2_IN_CH2", "DL2_CH2", "DL2"},
	{"HW_SRC_3_IN_CH1", "DL2_CH1", "DL2"},
	{"HW_SRC_3_IN_CH2", "DL2_CH2", "DL2"},
	{"HW_SRC_4_IN_CH1", "DL2_CH1", "DL2"},
	{"HW_SRC_4_IN_CH2", "DL2_CH2", "DL2"},

	{"HW_SRC_0_IN_CH1", "DL3_CH1", "DL3"},
	{"HW_SRC_0_IN_CH2", "DL3_CH2", "DL3"},
	{"HW_SRC_1_IN_CH1", "DL3_CH1", "DL3"},
	{"HW_SRC_1_IN_CH2", "DL3_CH2", "DL3"},
	{"HW_SRC_2_IN_CH1", "DL3_CH1", "DL3"},
	{"HW_SRC_2_IN_CH2", "DL3_CH2", "DL3"},
	{"HW_SRC_3_IN_CH1", "DL3_CH1", "DL3"},
	{"HW_SRC_3_IN_CH2", "DL3_CH2", "DL3"},
	{"HW_SRC_4_IN_CH1", "DL3_CH1", "DL3"},
	{"HW_SRC_4_IN_CH2", "DL3_CH2", "DL3"},

	{"HW_SRC_0_IN_CH1", "DL4_CH1", "DL4"},
	{"HW_SRC_0_IN_CH2", "DL4_CH2", "DL4"},
	{"HW_SRC_1_IN_CH1", "DL4_CH1", "DL4"},
	{"HW_SRC_1_IN_CH2", "DL4_CH2", "DL4"},
	{"HW_SRC_2_IN_CH1", "DL4_CH1", "DL4"},
	{"HW_SRC_2_IN_CH2", "DL4_CH2", "DL4"},
	{"HW_SRC_3_IN_CH1", "DL4_CH1", "DL4"},
	{"HW_SRC_3_IN_CH2", "DL4_CH2", "DL4"},
	{"HW_SRC_4_IN_CH1", "DL4_CH1", "DL4"},
	{"HW_SRC_4_IN_CH2", "DL4_CH2", "DL4"},

	{"HW_SRC_0_IN_CH1", "DL5_CH1", "DL5"},
	{"HW_SRC_0_IN_CH2", "DL5_CH2", "DL5"},
	{"HW_SRC_1_IN_CH1", "DL5_CH1", "DL5"},
	{"HW_SRC_1_IN_CH2", "DL5_CH2", "DL5"},
	{"HW_SRC_2_IN_CH1", "DL5_CH1", "DL5"},
	{"HW_SRC_2_IN_CH2", "DL5_CH2", "DL5"},
	{"HW_SRC_3_IN_CH1", "DL5_CH1", "DL5"},
	{"HW_SRC_3_IN_CH2", "DL5_CH2", "DL5"},
	{"HW_SRC_4_IN_CH1", "DL5_CH1", "DL5"},
	{"HW_SRC_4_IN_CH2", "DL5_CH2", "DL5"},

	{"HW_SRC_0_IN_CH1", "DL6_CH1", "DL6"},
	{"HW_SRC_0_IN_CH2", "DL6_CH2", "DL6"},
	{"HW_SRC_1_IN_CH1", "DL6_CH1", "DL6"},
	{"HW_SRC_1_IN_CH2", "DL6_CH2", "DL6"},
	{"HW_SRC_2_IN_CH1", "DL6_CH1", "DL6"},
	{"HW_SRC_2_IN_CH2", "DL6_CH2", "DL6"},
	{"HW_SRC_3_IN_CH1", "DL6_CH1", "DL6"},
	{"HW_SRC_3_IN_CH2", "DL6_CH2", "DL6"},
	{"HW_SRC_4_IN_CH1", "DL6_CH1", "DL6"},
	{"HW_SRC_4_IN_CH2", "DL6_CH2", "DL6"},

	{"HW_SRC_0_IN_CH1", "DL24_CH1", "DL6"},
	{"HW_SRC_0_IN_CH2", "DL24_CH2", "DL6"},
	{"HW_SRC_1_IN_CH1", "DL24_CH1", "DL6"},
	{"HW_SRC_1_IN_CH2", "DL24_CH2", "DL6"},
	{"HW_SRC_2_IN_CH1", "DL24_CH1", "DL6"},
	{"HW_SRC_2_IN_CH2", "DL24_CH2", "DL6"},
	{"HW_SRC_3_IN_CH1", "DL24_CH1", "DL6"},
	{"HW_SRC_3_IN_CH2", "DL24_CH2", "DL6"},
	{"HW_SRC_4_IN_CH1", "DL24_CH1", "DL6"},
	{"HW_SRC_4_IN_CH2", "DL24_CH2", "DL6"},
	{"HW_SRC_0_IN_CH1", "DL_24CH_CH1", "DL_24CH"},
	{"HW_SRC_0_IN_CH2", "DL_24CH_CH2", "DL_24CH"},
	{"HW_SRC_1_IN_CH1", "DL_24CH_CH1", "DL_24CH"},
	{"HW_SRC_1_IN_CH2", "DL_24CH_CH2", "DL_24CH"},
	{"HW_SRC_2_IN_CH1", "DL_24CH_CH1", "DL_24CH"},
	{"HW_SRC_2_IN_CH2", "DL_24CH_CH2", "DL_24CH"},
	{"HW_SRC_3_IN_CH1", "DL_24CH_CH1", "DL_24CH"},
	{"HW_SRC_3_IN_CH2", "DL_24CH_CH2", "DL_24CH"},
	{"HW_SRC_4_IN_CH1", "DL_24CH_CH1", "DL_24CH"},
	{"HW_SRC_4_IN_CH2", "DL_24CH_CH2", "DL_24CH"},

	{"HW_SRC_0_In", NULL, "HW_SRC_0_IN_CH1"},
	{"HW_SRC_0_In", NULL, "HW_SRC_0_IN_CH2"},
	{"HW_SRC_1_In", NULL, "HW_SRC_1_IN_CH1"},
	{"HW_SRC_1_In", NULL, "HW_SRC_1_IN_CH2"},
	{"HW_SRC_2_In", NULL, "HW_SRC_2_IN_CH1"},
	{"HW_SRC_2_In", NULL, "HW_SRC_2_IN_CH2"},
	{"HW_SRC_3_In", NULL, "HW_SRC_3_IN_CH1"},
	{"HW_SRC_3_In", NULL, "HW_SRC_3_IN_CH2"},
	{"HW_SRC_4_In", NULL, "HW_SRC_4_IN_CH1"},
	{"HW_SRC_4_In", NULL, "HW_SRC_4_IN_CH2"},

	{"HW_SRC_0_In", NULL, HW_SRC_0_EN_W_NAME, mtk_afe_src_en_connect},
	{"HW_SRC_0_Out", NULL, HW_SRC_0_EN_W_NAME, mtk_afe_src_en_connect},
	{"HW_SRC_1_In", NULL, HW_SRC_1_EN_W_NAME, mtk_afe_src_en_connect},
	{"HW_SRC_1_Out", NULL, HW_SRC_1_EN_W_NAME, mtk_afe_src_en_connect},
	{"HW_SRC_2_In", NULL, HW_SRC_2_EN_W_NAME, mtk_afe_src_en_connect},
	{"HW_SRC_2_Out", NULL, HW_SRC_2_EN_W_NAME, mtk_afe_src_en_connect},
	{"HW_SRC_3_In", NULL, HW_SRC_3_EN_W_NAME, mtk_afe_src_en_connect},
	{"HW_SRC_3_Out", NULL, HW_SRC_3_EN_W_NAME, mtk_afe_src_en_connect},
	{"HW_SRC_4_In", NULL, HW_SRC_4_EN_W_NAME, mtk_afe_src_en_connect},
	{"HW_SRC_4_Out", NULL, HW_SRC_4_EN_W_NAME, mtk_afe_src_en_connect},

	/* hires source from apll1 */
	{HW_SRC_0_EN_W_NAME, NULL, APLL2_W_NAME, mtk_afe_src_apll_connect},
	{HW_SRC_1_EN_W_NAME, NULL, APLL1_W_NAME, mtk_afe_src_apll_connect},
	{HW_SRC_2_EN_W_NAME, NULL, APLL2_W_NAME, mtk_afe_src_apll_connect},
	{HW_SRC_3_EN_W_NAME, NULL, APLL1_W_NAME, mtk_afe_src_apll_connect},
	{HW_SRC_4_EN_W_NAME, NULL, APLL2_W_NAME, mtk_afe_src_apll_connect},

	{"HW SRC 0 In Endpoint", NULL, "HW_SRC_0_In"},
	{"HW SRC 1 In Endpoint", NULL, "HW_SRC_1_In"},
	{"HW SRC 2 In Endpoint", NULL, "HW_SRC_2_In"},
	{"HW SRC 3 In Endpoint", NULL, "HW_SRC_3_In"},
	{"HW SRC 4 In Endpoint", NULL, "HW_SRC_4_In"},
	{"HW_SRC_0_Out", NULL, "HW SRC 0 Out Endpoint"},
	{"HW_SRC_1_Out", NULL, "HW SRC 1 Out Endpoint"},
	{"HW_SRC_2_Out", NULL, "HW SRC 2 Out Endpoint"},
	{"HW_SRC_3_Out", NULL, "HW SRC 3 Out Endpoint"},
	{"HW_SRC_4_Out", NULL, "HW SRC 4 Out Endpoint"},
};

/* dai ops */
static int mtk_dai_src_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8189_afe_private *afe_priv = afe->platform_priv;
	int id = dai->id;
	struct mtk_afe_src_priv *src_priv = afe_priv->dai_priv[id];
	unsigned int reg, sft, mask;
	unsigned int rate = params_rate(params);
#ifdef GSRC_REG
	unsigned int rate_reg = mt8189_rate_transform(afe->dev, rate);
#endif

	dev_info(afe->dev, "%s(), id %d, stream %d, rate %d\n",
		 __func__,
		 id,
		 substream->stream,
		 rate);

	if (id == MT8189_DAI_SRC_0)
		reg = AFE_GASRC0_NEW_CON5;
	else if (id == MT8189_DAI_SRC_1)
		reg = AFE_GASRC1_NEW_CON5;
	else if (id == MT8189_DAI_SRC_2)
		reg = AFE_GASRC2_NEW_CON5;
	else if (id == MT8189_DAI_SRC_3)
		reg = AFE_GASRC3_NEW_CON5;
	else
		reg = AFE_GASRC4_NEW_CON5;

	/* rate */
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		src_priv->dl_rate = rate;
		sft = IN_EN_SEL_FS_SFT;
		mask = IN_EN_SEL_FS_MASK;
	} else {
		src_priv->ul_rate = rate;
		sft = OUT_EN_SEL_FS_SFT;
		mask = OUT_EN_SEL_FS_MASK;
	}
#ifdef GSRC_REG
	regmap_update_bits(afe->regmap,
			   reg,
			   mask << sft,
			   rate_reg << sft);
#endif
	return 0;
}

static int mtk_dai_src_hw_free(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8189_afe_private *afe_priv = afe->platform_priv;
	int id = dai->id;
	struct mtk_afe_src_priv *src_priv = afe_priv->dai_priv[id];
#ifdef GSRC_REG
	unsigned int reg, sft, mask;
#endif
	dev_info(afe->dev, "%s(), id %d, stream %d\n",
		 __func__,
		 id,
		 substream->stream);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		src_priv->dl_rate = 0;
	else
		src_priv->ul_rate = 0;

#ifdef GSRC_REG
	if (id == MT8189_DAI_SRC_0)
		reg = AFE_GASRC0_NEW_CON5;
	else if (id == MT8189_DAI_SRC_1)
		reg = AFE_GASRC1_NEW_CON5;
	else if (id == MT8189_DAI_SRC_2)
		reg = AFE_GASRC2_NEW_CON5;
	else if (id == MT8189_DAI_SRC_3)
		reg = AFE_GASRC3_NEW_CON5;
	else
		reg = AFE_GASRC4_NEW_CON5;

	/* rate */
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		sft = IN_EN_SEL_FS_SFT;
		mask = IN_EN_SEL_FS_MASK;
	} else {
		sft = OUT_EN_SEL_FS_SFT;
		mask = OUT_EN_SEL_FS_MASK;
	}
	regmap_update_bits(afe->regmap,
			   reg,
			   mask << sft,
			   0 << sft);
#endif

	return 0;
}

static const struct snd_soc_dai_ops mtk_dai_src_ops = {
	.hw_params = mtk_dai_src_hw_params,
	.hw_free = mtk_dai_src_hw_free,
};

/* dai driver */
#define MTK_SRC_RATES (SNDRV_PCM_RATE_8000_48000 |\
		       SNDRV_PCM_RATE_88200 |\
		       SNDRV_PCM_RATE_96000 |\
		       SNDRV_PCM_RATE_176400 |\
		       SNDRV_PCM_RATE_192000)

#define MTK_SRC_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
			 SNDRV_PCM_FMTBIT_S24_LE |\
			 SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver mtk_dai_src_driver[] = {
	{
		.name = "HW_SRC_0",
		.id = MT8189_DAI_SRC_0,
		.playback = {
			.stream_name = "HW_SRC_0_In",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_SRC_RATES,
			.formats = MTK_SRC_FORMATS,
		},
		.capture = {
			.stream_name = "HW_SRC_0_Out",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_SRC_RATES,
			.formats = MTK_SRC_FORMATS,
		},
		.ops = &mtk_dai_src_ops,
	},
	{
		.name = "HW_SRC_1",
		.id = MT8189_DAI_SRC_1,
		.playback = {
			.stream_name = "HW_SRC_1_In",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_SRC_RATES,
			.formats = MTK_SRC_FORMATS,
		},
		.capture = {
			.stream_name = "HW_SRC_1_Out",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_SRC_RATES,
			.formats = MTK_SRC_FORMATS,
		},
		.ops = &mtk_dai_src_ops,
	},
	{
		.name = "HW_SRC_2",
		.id = MT8189_DAI_SRC_2,
		.playback = {
			.stream_name = "HW_SRC_2_In",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_SRC_RATES,
			.formats = MTK_SRC_FORMATS,
		},
		.capture = {
			.stream_name = "HW_SRC_2_Out",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_SRC_RATES,
			.formats = MTK_SRC_FORMATS,
		},
		.ops = &mtk_dai_src_ops,
	},
	{
		.name = "HW_SRC_3",
		.id = MT8189_DAI_SRC_3,
		.playback = {
			.stream_name = "HW_SRC_3_In",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_SRC_RATES,
			.formats = MTK_SRC_FORMATS,
		},
		.capture = {
			.stream_name = "HW_SRC_3_Out",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_SRC_RATES,
			.formats = MTK_SRC_FORMATS,
		},
		.ops = &mtk_dai_src_ops,
	},
	{
		.name = "HW_SRC_4",
		.id = MT8189_DAI_SRC_4,
		.playback = {
			.stream_name = "HW_SRC_4_In",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_SRC_RATES,
			.formats = MTK_SRC_FORMATS,
		},
		.capture = {
			.stream_name = "HW_SRC_4_Out",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_SRC_RATES,
			.formats = MTK_SRC_FORMATS,
		},
		.ops = &mtk_dai_src_ops,
	},
};

static int init_src_priv_data(struct mtk_base_afe *afe)
{
	struct mt8189_afe_private *afe_priv = afe->platform_priv;
	struct mtk_afe_src_priv *src_priv;
	int size = sizeof(struct mtk_afe_src_priv);
	int id;

	for (id = MT8189_DAI_SRC_START; id < MT8189_DAI_SRC_END; id++) {
		src_priv = devm_kzalloc(afe->dev, size, GFP_KERNEL);
		if (!src_priv)
			return -ENOMEM;

		afe_priv->dai_priv[id] = src_priv;
	}

	return 0;
}

int mt8189_dai_src_register(struct mtk_base_afe *afe)
{
	struct mtk_base_afe_dai *dai;
	int ret;

	dai = devm_kzalloc(afe->dev, sizeof(*dai), GFP_KERNEL);
	if (!dai)
		return -ENOMEM;

	list_add(&dai->list, &afe->sub_dais);

	dai->dai_drivers = mtk_dai_src_driver;
	dai->num_dai_drivers = ARRAY_SIZE(mtk_dai_src_driver);

	dai->dapm_widgets = mtk_dai_src_widgets;
	dai->num_dapm_widgets = ARRAY_SIZE(mtk_dai_src_widgets);
	dai->dapm_routes = mtk_dai_src_routes;
	dai->num_dapm_routes = ARRAY_SIZE(mtk_dai_src_routes);

	ret = init_src_priv_data(afe);
	if (ret)
		return ret;

	return 0;
}
