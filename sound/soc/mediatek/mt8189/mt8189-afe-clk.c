// SPDX-License-Identifier: GPL-2.0
/*
 *  mt8189-afe-clk.c  --  MediaTek 8189 afe clock ctrl
 *
 *  Copyright (c) 2025 MediaTek Inc.
 *  Author: Darren Ye <darren.ye@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>

#include "mt8189-afe-common.h"
#include "mt8189-afe-clk.h"

/* apll1 tuner default value*/
#define APLL1_TUNER_CON0_VALUE 0x6f28bd4d

/* apll2 tuner default value + 1*/
#define APLL2_TUNER_CON0_VALUE 0x78fd5265
static DEFINE_MUTEX(mutex_request_dram);

static const char *aud_clks[CLK_NUM] = {
	/* afe clk */
	[CLK_HOPPING] = "aud_hopping_clk",
	[CLK_F26M] = "aud_f26m_clk",
	[CLK_APLL1] = "aud_apll1_clk",
	[CLK_APLL2] = "aud_apll2_clk",
	[CLK_APLL1_TUNER] = "aud_apll_tuner1_clk",
	[CLK_APLL2_TUNER] = "aud_apll_tuner2_clk",
	/* ck clk */
	[CLK_MUX_AUDIOINTBUS] = "top_mux_audio_int",
	[CLK_TOP_MAINPLL_D4_D4] = "top_mainpll_d4_d4",
	[CLK_TOP_MUX_AUD_1] = "top_mux_aud_1",
	[CLK_TOP_APLL1_CK] = "top_apll1_ck",
	[CLK_TOP_MUX_AUD_2] = "top_mux_aud_2",
	[CLK_TOP_APLL2_CK] = "top_apll2_ck",
	[CLK_TOP_MUX_AUD_ENG1] = "top_mux_aud_eng1",
	[CLK_TOP_APLL1_D4] = "top_apll1_d4",
	[CLK_TOP_MUX_AUD_ENG2] = "top_mux_aud_eng2",
	[CLK_TOP_APLL2_D4] = "top_apll2_d4",
	[CLK_TOP_MUX_AUDIO_H] = "top_mux_audio_h",
	[CLK_TOP_APLL1_D2] = "top_apll1_d2",
	[CLK_TOP_APLL2_D2] = "top_apll2_d2",
	[CLK_TOP_I2SIN0_M_SEL] = "top_i2sin0_m_sel",
	[CLK_TOP_I2SIN1_M_SEL] = "top_i2sin1_m_sel",
	[CLK_TOP_FMI2S_M_SEL] = "top_fmi2s_m_sel",
	[CLK_TOP_TDMOUT_M_SEL] = "top_tdmout_m_sel",
	[CLK_TOP_APLL12_DIV_I2SIN0] = "top_apll12_div_i2sin0",
	[CLK_TOP_APLL12_DIV_I2SIN1] = "top_apll12_div_i2sin1",
	[CLK_TOP_APLL12_DIV_FMI2S] = "top_apll12_div_fmi2s",
	[CLK_TOP_APLL12_DIV_TDMOUT_M] = "top_apll12_div_tdmout_m",
	[CLK_TOP_APLL12_DIV_TDMOUT_B] = "top_apll12_div_tdmout_b",
	[CLK_CLK26M] = "top_clk26m_clk",
	[CLK_PERAO_AUDIO_SLV_CK_PERI] = "aud_slv_ck_peri",
	[CLK_PERAO_AUDIO_MST_CK_PERI] = "aud_mst_ck_peri",
	[CLK_PERAO_INTBUS_CK_PERI] = "aud_intbus_ck_peri",
};

int mt8189_set_audio_int_bus_parent(struct mtk_base_afe *afe,
				    int clk_id)
{
	struct mt8189_afe_private *afe_priv = afe->platform_priv;
	// struct clk *clk;
	int ret = 0;

	if (clk_id < 0 || clk_id > CLK_CLK26M)
		return -EINVAL;

	ret = clk_set_parent(afe_priv->clk[CLK_MUX_AUDIOINTBUS],
			     afe_priv->clk[clk_id]);
	if (ret) {
		dev_err(afe->dev, "clk_set_parent %s-%s fail %d\n",
			aud_clks[CLK_MUX_AUDIOINTBUS],
			aud_clks[clk_id], ret);
		return ret;
	}

	return 0;
}

int mt8189_set_audio_h_parent(struct mtk_base_afe *afe,
				    int clk_id)
{
	struct mt8189_afe_private *afe_priv = afe->platform_priv;
	int ret = 0;

	if (clk_id < 0 || clk_id > CLK_CLK26M) {
		dev_err(afe->dev, "invalid clk_id %d\n", clk_id);
		return -EINVAL;
	}

	ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUDIO_H],
			     afe_priv->clk[clk_id]);
	if (ret) {
		dev_err(afe->dev, "clk_set_parent %s-%s fail %d\n",
			aud_clks[CLK_TOP_MUX_AUDIO_H],
			aud_clks[clk_id], ret);
		return ret;
	}

	return 0;
}


static int apll1_mux_setting(struct mtk_base_afe *afe, bool enable)
{
	struct mt8189_afe_private *afe_priv = afe->platform_priv;
	int ret = 0;

	dev_dbg(afe->dev, "enable: %d\n", enable);

	if (enable) {
		ret = clk_prepare_enable(afe_priv->clk[CLK_TOP_MUX_AUD_1]);
		if (ret) {
			dev_err(afe->dev, "clk_prepare_enable %s fail %d\n",
				aud_clks[CLK_TOP_MUX_AUD_1], ret);
			return ret;
		}
		ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_1],
				     afe_priv->clk[CLK_TOP_APLL1_CK]);
		if (ret) {
			dev_err(afe->dev, "clk_set_parent %s-%s fail %d\n",
				aud_clks[CLK_TOP_MUX_AUD_1],
				aud_clks[CLK_TOP_APLL1_CK], ret);
			return ret;
		}

		/* 180.6336 / 4 = 45.1584MHz */
		ret = clk_prepare_enable(afe_priv->clk[CLK_TOP_MUX_AUD_ENG1]);
		if (ret) {
			dev_err(afe->dev, "clk_prepare_enable %s fail %d\n",
				aud_clks[CLK_TOP_MUX_AUD_ENG1], ret);
			return ret;
		}
		ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_ENG1],
				     afe_priv->clk[CLK_TOP_APLL1_D4]);
		if (ret) {
			dev_err(afe->dev, "clk_set_parent %s-%s fail %d\n",
				aud_clks[CLK_TOP_MUX_AUD_ENG1],
				aud_clks[CLK_TOP_APLL1_D4], ret);
			return ret;
		}

		ret = clk_prepare_enable(afe_priv->clk[CLK_TOP_MUX_AUDIO_H]);
		if (ret) {
			dev_err(afe->dev, "clk_prepare_enable %s fail %d\n",
				 aud_clks[CLK_TOP_MUX_AUDIO_H], ret);
			return ret;
		}
		mt8189_set_audio_h_parent(afe, CLK_TOP_APLL1_CK);
	} else {
		ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_ENG1],
				     afe_priv->clk[CLK_CLK26M]);
		if (ret) {
			dev_info(afe->dev, "clk_set_parent %s-%s fail %d\n",
				aud_clks[CLK_TOP_MUX_AUD_ENG1],
				aud_clks[CLK_CLK26M], ret);
			return ret;
		}
		clk_disable_unprepare(afe_priv->clk[CLK_TOP_MUX_AUD_ENG1]);

		ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_1],
				     afe_priv->clk[CLK_CLK26M]);
		if (ret) {
			dev_err(afe->dev, "clk_set_parent %s-%s fail %d\n",
				aud_clks[CLK_TOP_MUX_AUD_1],
				aud_clks[CLK_CLK26M], ret);
			return ret;
		}
		clk_disable_unprepare(afe_priv->clk[CLK_TOP_MUX_AUD_1]);

		mt8189_set_audio_h_parent(afe, CLK_CLK26M);
		clk_disable_unprepare(afe_priv->clk[CLK_TOP_MUX_AUDIO_H]);
	}

	return 0;
}

static int apll2_mux_setting(struct mtk_base_afe *afe, bool enable)
{
	struct mt8189_afe_private *afe_priv = afe->platform_priv;
	int ret = 0;

	dev_dbg(afe->dev, "enable: %d\n", enable);

	if (enable) {
		ret = clk_prepare_enable(afe_priv->clk[CLK_TOP_MUX_AUD_2]);
		if (ret) {
			dev_err(afe->dev, "clk_prepare_enable %s fail %d\n",
				aud_clks[CLK_TOP_MUX_AUD_2], ret);
			return ret;
		}
		ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_2],
				     afe_priv->clk[CLK_TOP_APLL2_CK]);
		if (ret) {
			dev_err(afe->dev, "clk_set_parent %s-%s fail %d\n",
				aud_clks[CLK_TOP_MUX_AUD_2],
				aud_clks[CLK_TOP_APLL2_CK], ret);
			return ret;
		}

		/* 196.608 / 4 = 49.152MHz */
		ret = clk_prepare_enable(afe_priv->clk[CLK_TOP_MUX_AUD_ENG2]);
		if (ret) {
			dev_err(afe->dev, "clk_prepare_enable %s fail %d\n",
				aud_clks[CLK_TOP_MUX_AUD_ENG2], ret);
			return ret;
		}
		ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_ENG2],
				     afe_priv->clk[CLK_TOP_APLL2_D4]);
		if (ret) {
			dev_err(afe->dev, "clk_set_parent %s-%s fail %d\n",
				aud_clks[CLK_TOP_MUX_AUD_ENG2],
				aud_clks[CLK_TOP_APLL2_D4], ret);
			return ret;
		}

		ret = clk_prepare_enable(afe_priv->clk[CLK_TOP_MUX_AUDIO_H]);
		if (ret) {
			dev_err(afe->dev, "clk_prepare_enable %s fail %d\n",
				 aud_clks[CLK_TOP_MUX_AUDIO_H], ret);
			return ret;
		}

		mt8189_set_audio_h_parent(afe, CLK_TOP_APLL2_CK);
	} else {
		ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_ENG2],
				     afe_priv->clk[CLK_CLK26M]);
		if (ret) {
			dev_err(afe->dev, "clk_set_parent %s-%s fail %d\n",
				aud_clks[CLK_TOP_MUX_AUD_ENG2],
				aud_clks[CLK_CLK26M], ret);
			return ret;
		}
		clk_disable_unprepare(afe_priv->clk[CLK_TOP_MUX_AUD_ENG2]);

		ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_2],
				     afe_priv->clk[CLK_CLK26M]);
		if (ret) {
			dev_info(afe->dev, "clk_set_parent %s-%s fail %d\n",
				aud_clks[CLK_TOP_MUX_AUD_2],
				aud_clks[CLK_CLK26M], ret);
			return ret;
		}
		clk_disable_unprepare(afe_priv->clk[CLK_TOP_MUX_AUD_2]);

		mt8189_set_audio_h_parent(afe, CLK_CLK26M);
		clk_disable_unprepare(afe_priv->clk[CLK_TOP_MUX_AUDIO_H]);
	}

	return 0;
}

int mt8189_afe_disable_apll(struct mtk_base_afe *afe)
{
	struct mt8189_afe_private *afe_priv = afe->platform_priv;
	int ret = 0;

	ret = clk_prepare_enable(afe_priv->clk[CLK_TOP_MUX_AUDIO_H]);
	if (ret) {
		dev_err(afe->dev, "clk_prepare_enable %s fail %d\n",
			 aud_clks[CLK_TOP_MUX_AUDIO_H], ret);
		return ret;
	}

	ret = clk_prepare_enable(afe_priv->clk[CLK_TOP_MUX_AUD_1]);
	if (ret) {
		dev_err(afe->dev, "clk_prepare_enable %s fail %d\n",
			aud_clks[CLK_TOP_MUX_AUD_1], ret);
		goto clk_ck_mux_aud1_err;
	}

	ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_1],
			     afe_priv->clk[CLK_CLK26M]);
	if (ret) {
		dev_err(afe->dev, "clk_set_parent %s-%s fail %d\n",
			aud_clks[CLK_TOP_MUX_AUD_1],
			aud_clks[CLK_CLK26M], ret);
		goto clk_ck_mux_aud1_parent_err;
	}
	ret = clk_prepare_enable(afe_priv->clk[CLK_TOP_MUX_AUD_2]);
	if (ret) {
		dev_err(afe->dev, "clk_prepare_enable %s fail %d\n",
			aud_clks[CLK_TOP_MUX_AUD_2], ret);
		goto clk_ck_mux_aud2_err;
	}

	ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_2],
			     afe_priv->clk[CLK_CLK26M]);
	if (ret) {
		dev_err(afe->dev, "clk_set_parent %s-%s fail %d\n",
			aud_clks[CLK_TOP_MUX_AUD_2],
			aud_clks[CLK_CLK26M], ret);
		goto clk_ck_mux_aud2_parent_err;
	}

	clk_disable_unprepare(afe_priv->clk[CLK_TOP_MUX_AUD_1]);
	clk_disable_unprepare(afe_priv->clk[CLK_TOP_MUX_AUD_2]);
	mt8189_set_audio_h_parent(afe, CLK_CLK26M);
	clk_disable_unprepare(afe_priv->clk[CLK_TOP_MUX_AUDIO_H]);

	return 0;

clk_ck_mux_aud2_parent_err:
	clk_disable_unprepare(afe_priv->clk[CLK_TOP_MUX_AUD_2]);
clk_ck_mux_aud2_err:
	clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_1],
		       afe_priv->clk[CLK_TOP_APLL1_CK]);
clk_ck_mux_aud1_parent_err:
	clk_disable_unprepare(afe_priv->clk[CLK_TOP_MUX_AUD_1]);
clk_ck_mux_aud1_err:
	clk_disable_unprepare(afe_priv->clk[CLK_TOP_MUX_AUDIO_H]);

	return ret;
}

int mt8189_afe_enable_ao_clock(struct mtk_base_afe *afe)
{
	struct mt8189_afe_private *afe_priv = afe->platform_priv;
	int ret = 0;

	dev_dbg(afe->dev, "%s() successfully start\n", __func__);
	/* Peri clock AO enable */
	ret = clk_prepare_enable(afe_priv->clk[CLK_PERAO_INTBUS_CK_PERI]);
	if (ret) {
		dev_info(afe->dev, "%s() clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_PERAO_INTBUS_CK_PERI], ret);
		goto CLK_PERAO_INTBUS_CK_PERI_ERR;
	}

	ret = clk_prepare_enable(afe_priv->clk[CLK_PERAO_AUDIO_SLV_CK_PERI]);
	if (ret) {
		dev_info(afe->dev, "%s() clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_PERAO_AUDIO_SLV_CK_PERI], ret);
		goto CLK_PERAO_AUDIO_SLV_CK_PERI_ERR;
	}

	ret = clk_prepare_enable(afe_priv->clk[CLK_PERAO_AUDIO_MST_CK_PERI]);
	if (ret) {
		dev_info(afe->dev, "%s() clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_PERAO_AUDIO_MST_CK_PERI], ret);
		goto CLK_PERAO_AUDIO_MST_CK_PERI_ERR;
	}
	return 0;
CLK_PERAO_AUDIO_MST_CK_PERI_ERR:
	clk_disable_unprepare(afe_priv->clk[CLK_PERAO_AUDIO_MST_CK_PERI]);
CLK_PERAO_AUDIO_SLV_CK_PERI_ERR:
	clk_disable_unprepare(afe_priv->clk[CLK_PERAO_AUDIO_SLV_CK_PERI]);
CLK_PERAO_INTBUS_CK_PERI_ERR:
	clk_disable_unprepare(afe_priv->clk[CLK_PERAO_INTBUS_CK_PERI]);

	return ret;
}

int mt8189_afe_apll_init(struct mtk_base_afe *afe)
{
	struct mt8189_afe_private *afe_priv = afe->platform_priv;

	if (afe_priv->apmixed) {
		regmap_write(afe_priv->apmixed, APLL1_TUNER_CON0, APLL1_TUNER_CON0_VALUE);
		regmap_write(afe_priv->apmixed, APLL2_TUNER_CON0, APLL2_TUNER_CON0_VALUE);
	} else {
		dev_warn(afe->dev, "apmixed regmap is null ptr\n");
	}

	return 0;
}


int mt8189_afe_enable_clock(struct mtk_base_afe *afe)
{
	struct mt8189_afe_private *afe_priv = afe->platform_priv;
	int ret = 0;

	/* IPM2.0: USE HOPPING & 26M */
	ret = clk_prepare_enable(afe_priv->clk[CLK_HOPPING]);
	if (ret) {
		dev_err(afe->dev, "clk_prepare_enable %s fail %d\n",
			 aud_clks[CLK_HOPPING], ret);
		goto CLK_AFE_ERR;
	}
	ret = clk_prepare_enable(afe_priv->clk[CLK_F26M]);
	if (ret) {
		dev_err(afe->dev, "clk_prepare_enable %s fail %d\n",
			 aud_clks[CLK_F26M], ret);
		goto CLK_AFE_ERR;
	}

	/* IPM2.0: Remove CLK_MUX_AUDIO */

	ret = clk_prepare_enable(afe_priv->clk[CLK_MUX_AUDIOINTBUS]);
	if (ret) {
		dev_err(afe->dev, "clk_prepare_enable %s fail %d\n",
			 aud_clks[CLK_MUX_AUDIOINTBUS], ret);
		goto CLK_MUX_AUDIO_INTBUS_ERR;
	}
	ret = mt8189_set_audio_int_bus_parent(afe, CLK_CLK26M);

	ret = clk_prepare_enable(afe_priv->clk[CLK_TOP_MUX_AUDIO_H]);
	if (ret) {
		dev_info(afe->dev, "clk_prepare_enable %s fail %d\n",
			 aud_clks[CLK_TOP_MUX_AUDIO_H], ret);
		goto CLK_MUX_AUDIO_H_PARENT_ERR;
	}

	ret = mt8189_set_audio_h_parent(afe, CLK_CLK26M);

	return 0;

CLK_AFE_ERR:
	/* IPM2.0: Use HOPPING & 26M */
	clk_disable_unprepare(afe_priv->clk[CLK_HOPPING]);
	clk_disable_unprepare(afe_priv->clk[CLK_F26M]);

CLK_MUX_AUDIO_H_PARENT_ERR:
	clk_disable_unprepare(afe_priv->clk[CLK_TOP_MUX_AUDIO_H]);
CLK_MUX_AUDIO_INTBUS_ERR:
	clk_disable_unprepare(afe_priv->clk[CLK_MUX_AUDIOINTBUS]);

	return ret;
}

void mt8189_afe_disable_clock(struct mtk_base_afe *afe)
{
	struct mt8189_afe_private *afe_priv = afe->platform_priv;

	mt8189_set_audio_int_bus_parent(afe, CLK_CLK26M);
	clk_disable_unprepare(afe_priv->clk[CLK_MUX_AUDIOINTBUS]);
	mt8189_set_audio_h_parent(afe, CLK_CLK26M);
	clk_disable_unprepare(afe_priv->clk[CLK_TOP_MUX_AUDIO_H]);

	/* IPM2.0: Use HOPPING & 26M */
	clk_disable_unprepare(afe_priv->clk[CLK_HOPPING]);
	clk_disable_unprepare(afe_priv->clk[CLK_F26M]);

}

int mt8189_afe_dram_request(struct device *dev)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);
	struct mt8189_afe_private *afe_priv = afe->platform_priv;

	dev_dbg(dev, "%s(), dram_resource_counter %d\n",
		__func__, afe_priv->dram_resource_counter);

	mutex_lock(&mutex_request_dram);

	afe_priv->dram_resource_counter++;
	mutex_unlock(&mutex_request_dram);
	return 0;
}

int mt8189_afe_dram_release(struct device *dev)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);
	struct mt8189_afe_private *afe_priv = afe->platform_priv;

	dev_dbg(dev, "%s(), dram_resource_counter %d\n",
		 __func__, afe_priv->dram_resource_counter);

	mutex_lock(&mutex_request_dram);
	afe_priv->dram_resource_counter--;

	if (afe_priv->dram_resource_counter < 0) {
		dev_warn(dev, "dram_resource_counter %d\n",
			 afe_priv->dram_resource_counter);
		afe_priv->dram_resource_counter = 0;
	}
	mutex_unlock(&mutex_request_dram);
	return 0;
}

int mt8189_apll1_enable(struct mtk_base_afe *afe)
{
	struct mt8189_afe_private *afe_priv = afe->platform_priv;
	int ret;

	/* setting for APLL */
	apll1_mux_setting(afe, true);

	ret = clk_prepare_enable(afe_priv->clk[CLK_APLL1]);
	if (ret) {
		dev_err(afe->dev, "clk_prepare_enable %s fail %d\n",
			aud_clks[CLK_APLL1], ret);
		goto ERR_CLK_APLL1;
	}

	ret = clk_prepare_enable(afe_priv->clk[CLK_APLL1_TUNER]);
	if (ret) {
		dev_err(afe->dev, "clk_prepare_enable %s fail %d\n",
			aud_clks[CLK_APLL1_TUNER], ret);
		goto ERR_CLK_APLL1_TUNER;
	}

	/* sel 44.1kHz:1, apll_div:7, upper bound:3 */
	regmap_update_bits(afe->regmap, AFE_APLL1_TUNER_CFG,
			   XTAL_EN_128FS_SEL_MASK_SFT | APLL_DIV_MASK_SFT | UPPER_BOUND_MASK_SFT,
			   (0x1 << XTAL_EN_128FS_SEL_SFT) | (7 << APLL_DIV_SFT) |
			   (3 << UPPER_BOUND_SFT));

	/* apll1 freq tuner enable */
	regmap_update_bits(afe->regmap, AFE_APLL1_TUNER_CFG,
			   FREQ_TUNER_EN_MASK_SFT,
			   0x1 << FREQ_TUNER_EN_SFT);

	/* audio apll1 on */
	regmap_update_bits(afe->regmap, AUDIO_ENGEN_CON0,
			   AUDIO_APLL1_EN_ON_MASK_SFT,
			   0x1 << AUDIO_APLL1_EN_ON_SFT);

	return 0;

ERR_CLK_APLL1_TUNER:
	clk_disable_unprepare(afe_priv->clk[CLK_APLL1_TUNER]);
ERR_CLK_APLL1:
	clk_disable_unprepare(afe_priv->clk[CLK_APLL1]);

	return ret;
}

void mt8189_apll1_disable(struct mtk_base_afe *afe)
{
	struct mt8189_afe_private *afe_priv = afe->platform_priv;

	/* audio apll1 off */
	regmap_update_bits(afe->regmap, AUDIO_ENGEN_CON0,
			   AUDIO_APLL1_EN_ON_MASK_SFT,
			   0x0);

	/* apll1 freq tuner disable */
	regmap_update_bits(afe->regmap, AFE_APLL1_TUNER_CFG,
			   FREQ_TUNER_EN_MASK_SFT,
			   0x0);

	clk_disable_unprepare(afe_priv->clk[CLK_APLL1_TUNER]);
	clk_disable_unprepare(afe_priv->clk[CLK_APLL1]);

	apll1_mux_setting(afe, false);
}

int mt8189_apll2_enable(struct mtk_base_afe *afe)
{
	struct mt8189_afe_private *afe_priv = afe->platform_priv;
	int ret;

	/* setting for APLL */
	apll2_mux_setting(afe, true);

	ret = clk_prepare_enable(afe_priv->clk[CLK_APLL2]);
	if (ret) {
		dev_err(afe->dev, "clk_prepare_enable %s fail %d\n",
			aud_clks[CLK_APLL2], ret);
		goto ERR_CLK_APLL2;
	}

	ret = clk_prepare_enable(afe_priv->clk[CLK_APLL2_TUNER]);
	if (ret) {
		dev_err(afe->dev, "clk_prepare_enable %s fail %d\n",
			aud_clks[CLK_APLL2_TUNER], ret);
		goto ERR_CLK_APLL2_TUNER;
	}

	/* sel 48kHz: 2, apll_div: 7, upper bound: 3*/
	regmap_update_bits(afe->regmap, AFE_APLL2_TUNER_CFG,
			   XTAL_EN_128FS_SEL_MASK_SFT | APLL_DIV_MASK_SFT | UPPER_BOUND_MASK_SFT,
			   (0x2 << XTAL_EN_128FS_SEL_SFT) | (7 << APLL_DIV_SFT) |
			   (3 << UPPER_BOUND_SFT));

	/* apll2 freq tuner enable */
	regmap_update_bits(afe->regmap, AFE_APLL2_TUNER_CFG,
			   FREQ_TUNER_EN_MASK_SFT,
			   0x1 << FREQ_TUNER_EN_SFT);

	/* audio apll2 on */
	regmap_update_bits(afe->regmap, AUDIO_ENGEN_CON0,
			   AUDIO_APLL2_EN_ON_MASK_SFT,
			   0x1 << AUDIO_APLL2_EN_ON_SFT);

	return 0;

ERR_CLK_APLL2_TUNER:
	clk_disable_unprepare(afe_priv->clk[CLK_APLL2_TUNER]);
ERR_CLK_APLL2:
	clk_disable_unprepare(afe_priv->clk[CLK_APLL2]);

	return ret;

	return 0;
}

void mt8189_apll2_disable(struct mtk_base_afe *afe)
{
	struct mt8189_afe_private *afe_priv = afe->platform_priv;

	/* audio apll2 off */
	regmap_update_bits(afe->regmap, AUDIO_ENGEN_CON0,
			   AUDIO_APLL2_EN_ON_MASK_SFT,
			   0x0);

	/* apll2 freq tuner disable */
	regmap_update_bits(afe->regmap, AFE_APLL2_TUNER_CFG,
			   FREQ_TUNER_EN_MASK_SFT,
			   0x0);

	clk_disable_unprepare(afe_priv->clk[CLK_APLL2_TUNER]);
	clk_disable_unprepare(afe_priv->clk[CLK_APLL2]);

	apll2_mux_setting(afe, false);
}

int mt8189_get_apll_rate(struct mtk_base_afe *afe, int apll)
{
	struct mt8189_afe_private *afe_priv = afe->platform_priv;
	int clk_id = 0;

	if (apll < MT8189_APLL1 || apll > MT8189_APLL2) {
		dev_warn(afe->dev, "invalid clk id\n");
		return 0;
	}

	if (apll == MT8189_APLL1)
		clk_id = CLK_TOP_APLL1_CK;
	else
		clk_id = CLK_TOP_APLL2_CK;

	dev_info(afe->dev, "apll: %d, clk_id: %d, rate: %lu\n",
		apll,
		clk_id,
		clk_get_rate(afe_priv->clk[clk_id]));

	return clk_get_rate(afe_priv->clk[clk_id]);
}

int mt8189_get_apll_by_rate(struct mtk_base_afe *afe, int rate)
{
	return ((rate % 8000) == 0) ? MT8189_APLL2 : MT8189_APLL1;
}

int mt8189_get_apll_by_name(struct mtk_base_afe *afe, const char *name)
{
	if (strcmp(name, APLL1_W_NAME) == 0)
		return MT8189_APLL1;
	else
		return MT8189_APLL2;
}

/* mck */
struct mt8189_mck_div {
	int m_sel_id;
	int div_clk_id;
};


static const struct mt8189_mck_div mck_div[MT8189_MCK_NUM] = {
	[MT8189_I2SIN0_MCK] = {
		.m_sel_id = CLK_TOP_I2SIN0_M_SEL,
		.div_clk_id = CLK_TOP_APLL12_DIV_I2SIN0,
	},
	[MT8189_I2SIN1_MCK] = {
		.m_sel_id = CLK_TOP_I2SIN1_M_SEL,
		.div_clk_id = CLK_TOP_APLL12_DIV_I2SIN1,
	},
	[MT8189_FMI2S_MCK] = {
		.m_sel_id = CLK_TOP_FMI2S_M_SEL,
		.div_clk_id = CLK_TOP_APLL12_DIV_FMI2S,
	},
	[MT8189_TDMOUT_MCK] = {
		.m_sel_id = CLK_TOP_TDMOUT_M_SEL,
		.div_clk_id = CLK_TOP_APLL12_DIV_TDMOUT_M,
	},
	[MT8189_TDMOUT_BCK] = {
		.m_sel_id = -1,
		.div_clk_id = CLK_TOP_APLL12_DIV_TDMOUT_B,
	},
};

int mt8189_mck_enable(struct mtk_base_afe *afe, int mck_id, int rate)
{
	struct mt8189_afe_private *afe_priv = afe->platform_priv;
	int apll = mt8189_get_apll_by_rate(afe, rate);
	int apll_clk_id = apll == MT8189_APLL1 ?
			  CLK_TOP_MUX_AUD_1 : CLK_TOP_MUX_AUD_2;
	int m_sel_id = 0;
	int div_clk_id = 0;
	int ret = 0;

	if (mck_id < 0) {
		dev_err(afe->dev, "invalid mck_id %d\n", mck_id);
		return -EINVAL;
	}
	m_sel_id = mck_div[mck_id].m_sel_id;
	div_clk_id = mck_div[mck_id].div_clk_id;

	/* select apll */
	if (m_sel_id >= 0) {
		ret = clk_prepare_enable(afe_priv->clk[m_sel_id]);
		if (ret) {
			dev_err(afe->dev, "clk_prepare_enable %s fail %d\n",
				aud_clks[m_sel_id], ret);
			return ret;
		}
		ret = clk_set_parent(afe_priv->clk[m_sel_id],
				     afe_priv->clk[apll_clk_id]);
		if (ret) {
			dev_err(afe->dev, "clk_set_parent %s-%s fail %d\n",
				aud_clks[m_sel_id],
				aud_clks[apll_clk_id], ret);
			return ret;
		}
	}

	/* enable div, set rate */
	if (div_clk_id < 0) {
		dev_err(afe->dev, "invalid div_clk_id %d\n", div_clk_id);
		return -EINVAL;
	}

	ret = clk_prepare_enable(afe_priv->clk[div_clk_id]);
	if (ret) {
		dev_err(afe->dev, "clk_prepare_enable %s fail %d\n",
			aud_clks[div_clk_id], ret);
		return ret;
	}
	ret = clk_set_rate(afe_priv->clk[div_clk_id], rate);
	if (ret) {
		dev_err(afe->dev, "clk_set_rate %s, rate %d, fail %d\n",
			aud_clks[div_clk_id],
			rate, ret);
		return ret;
	}

	return 0;
}

int mt8189_mck_disable(struct mtk_base_afe *afe, int mck_id)
{
	struct mt8189_afe_private *afe_priv = afe->platform_priv;
	int m_sel_id = 0;
	int div_clk_id = 0;

	if (mck_id < 0) {
		dev_err(afe->dev, "mck_id = %d < 0\n", mck_id);
		return -EINVAL;
	}

	m_sel_id = mck_div[mck_id].m_sel_id;
	div_clk_id = mck_div[mck_id].div_clk_id;

	if (div_clk_id < 0) {
		dev_err(afe->dev, "div_clk_id = %d < 0\n", div_clk_id);
		return -EINVAL;
	}
	clk_disable_unprepare(afe_priv->clk[div_clk_id]);


	if (m_sel_id >= 0)
		clk_disable_unprepare(afe_priv->clk[m_sel_id]);

	return 0;
}

int mt8189_init_clock(struct mtk_base_afe *afe)
{
	struct mt8189_afe_private *afe_priv = afe->platform_priv;
	int i = 0;

	afe_priv->clk = devm_kcalloc(afe->dev, CLK_NUM, sizeof(*afe_priv->clk),
				     GFP_KERNEL);
	if (!afe_priv->clk)
		return -ENOMEM;

	for (i = 0; i < CLK_NUM; i++) {
		if (!aud_clks[i])
			dev_warn(afe->dev, "clk id %d not define!!!\n", i);

		afe_priv->clk[i] = devm_clk_get(afe->dev, aud_clks[i]);
		if (IS_ERR(afe_priv->clk[i])) {
			dev_err(afe->dev, "devm_clk_get %s fail\n", aud_clks[i]);
			return PTR_ERR(afe_priv->clk[i]);
		}
	}

	afe_priv->apmixed = syscon_regmap_lookup_by_phandle(afe->dev->of_node,
			    "mediatek,apmixedsys");
	if (IS_ERR(afe_priv->apmixed)) {
		dev_err(afe->dev, "Cannot find apmixeds\n");
		return PTR_ERR(afe_priv->apmixed);
	}

	afe_priv->topckgen = syscon_regmap_lookup_by_phandle(afe->dev->of_node,
			     "mediatek,topckgen");
	if (IS_ERR(afe_priv->topckgen)) {
		dev_err(afe->dev, "Cannot find topckgen controller\n");
		return PTR_ERR(afe_priv->topckgen);
	}

	afe_priv->infracfg = syscon_regmap_lookup_by_phandle(
				     afe->dev->of_node,
				     "mediatek,infracfg");
	if (IS_ERR(afe_priv->infracfg)) {
		dev_info(afe->dev, "Cannot find infracfg\n");
		return PTR_ERR(afe_priv->infracfg);
	}
	mt8189_afe_apll_init(afe);
	mt8189_afe_disable_apll(afe);
	mt8189_afe_enable_ao_clock(afe);
	return 0;
}

