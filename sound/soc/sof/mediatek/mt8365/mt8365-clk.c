// SPDX-License-Identifier: (GPL-2.0-only OR BSD-3-Clause)
/*
 * Copyright (c) 2025 MediaTek Inc.
 *
 * Author: Aary Patil <aary.patil@mediatek.com>
 *
 * Hardware interface for mt8365 DSP clock
 */

#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/io.h>
#include "mt8365.h"
#include "mt8365-clk.h"
#include "../adsp_helper.h"
#include "../../sof-audio.h"

static const char *adsp_clks[ADSP_CLK_MAX] = {
	[CLK_TOP_DSP_SEL] = "CLK_DSP_SEL",
	[CLK_TOP_CLK26M_D52] = "CLK26M_CK",
	[CLK_TOP_SYS_26M_D2] = "AD_SYS_26M_D2",
	[CLK_TOP_DSPPLL] = "DSPPLL_CK",
	[CLK_TOP_DSPPLL_D2] = "DSPPLL_D2",
	[CLK_TOP_DSPPLL_D4] = "DSPPLL_D4",
	[CLK_TOP_DSPPLL_D8] = "DSPPLL_D8",
	[CLK_TOP_DSP_26M] = "PDN_DSP_26M",
	[CLK_TOP_DSP_32K] = "PDN_DSP_32K",
};

int mt8365_adsp_init_clock(struct snd_sof_dev *sdev)
{
	struct device *dev = sdev->dev;
	struct adsp_priv *priv = sdev->pdata->hw_pdata;
	int i;

	priv->clk = devm_kcalloc(dev, ADSP_CLK_MAX, sizeof(*priv->clk), GFP_KERNEL);

	if (!priv->clk)
		return -ENOMEM;

	for (i = 0; i < ADSP_CLK_MAX; i++) {
		priv->clk[i] = devm_clk_get(dev, adsp_clks[i]);
		if (IS_ERR(priv->clk[i]))
			return PTR_ERR(priv->clk[i]);
	}

	return 0;
}

static int adsp_enable_all_clock(struct snd_sof_dev *sdev)
{
	struct device *dev = sdev->dev;
	struct adsp_priv *priv = sdev->pdata->hw_pdata;
	int ret;

	ret = clk_prepare_enable(priv->clk[CLK_TOP_DSPPLL]);
	if (ret) {
		dev_err(dev, "%s clk_prepare_enable(CLK_TOP_DSPPLL) fail %d\n",
			__func__, ret);
		return ret;
	}

	ret = clk_prepare_enable(priv->clk[CLK_TOP_DSP_SEL]);
	if (ret) {
		dev_err(dev, "%s clk_prepare_enable(CLK_DSP_SEL) fail %d\n",
			__func__, ret);
		goto disable_top_dsppll_clk;
	}

	ret = clk_prepare_enable(priv->clk[CLK_TOP_SYS_26M_D2]);
	if (ret) {
		dev_err(dev, "%s clk_prepare_enable(CLK_TOP_SYS_26M_D2) fail %d\n",
			__func__, ret);
		goto disable_top_dsp_sel_clk;
	}

	ret = clk_prepare_enable(priv->clk[CLK_TOP_DSPPLL_D2]);
	if (ret) {
		dev_err(dev, "%s clk_prepare_enable(CLK_TOP_DSPPLL_D2) fail %d\n",
			__func__, ret);
		goto disable_top_sys_26m_d2_clk;
	}

	ret = clk_prepare_enable(priv->clk[CLK_TOP_DSPPLL_D4]);
	if (ret) {
		dev_err(dev, "%s clk_prepare_enable(CLK_TOP_DSPPLL_D4) fail %d\n",
			__func__, ret);
		goto disable_top_dsppll_d2_clk;
	}

	ret = clk_prepare_enable(priv->clk[CLK_TOP_DSPPLL_D8]);
	if (ret) {
		dev_err(dev, "%s clk_prepare_enable(CLK_TOP_DSPPLL_D8) fail %d\n",
			__func__, ret);
		goto disable_top_dsppll_d4_clk;
	}

	ret = clk_prepare_enable(priv->clk[CLK_TOP_DSP_26M]);
	if (ret) {
		dev_err(dev, "%s clk_prepare_enable(CLK_TOP_DSP_26M) fail %d\n",
			__func__, ret);
		goto disable_top_dsppll_d8_clk;
	}

	ret = clk_prepare_enable(priv->clk[CLK_TOP_DSP_32K]);
	if (ret) {
		dev_err(dev, "%s clk_prepare_enable(CLK_TOP_DSP_32K) fail %d\n",
			__func__, ret);
		goto disable_top_dsp_26m_clk;
	}

	return 0;

disable_top_dsp_26m_clk:
	clk_disable_unprepare(priv->clk[CLK_TOP_DSP_26M]);
disable_top_dsppll_d8_clk:
	clk_disable_unprepare(priv->clk[CLK_TOP_DSPPLL_D8]);
disable_top_dsppll_d4_clk:
	clk_disable_unprepare(priv->clk[CLK_TOP_DSPPLL_D4]);
disable_top_dsppll_d2_clk:
	clk_disable_unprepare(priv->clk[CLK_TOP_DSPPLL_D2]);
disable_top_sys_26m_d2_clk:
	clk_disable_unprepare(priv->clk[CLK_TOP_SYS_26M_D2]);
disable_top_dsp_sel_clk:
	clk_disable_unprepare(priv->clk[CLK_TOP_DSP_SEL]);
disable_top_dsppll_clk:
	clk_disable_unprepare(priv->clk[CLK_TOP_DSPPLL]);

	return ret;
}

static void adsp_disable_all_clock(struct snd_sof_dev *sdev)
{
	struct adsp_priv *priv = sdev->pdata->hw_pdata;

	clk_disable_unprepare(priv->clk[CLK_TOP_DSP_32K]);
	clk_disable_unprepare(priv->clk[CLK_TOP_DSP_26M]);
	clk_disable_unprepare(priv->clk[CLK_TOP_DSPPLL_D8]);
	clk_disable_unprepare(priv->clk[CLK_TOP_DSPPLL_D4]);
	clk_disable_unprepare(priv->clk[CLK_TOP_DSPPLL_D2]);
	clk_disable_unprepare(priv->clk[CLK_TOP_SYS_26M_D2]);
	clk_disable_unprepare(priv->clk[CLK_TOP_DSP_SEL]);
	clk_disable_unprepare(priv->clk[CLK_TOP_DSPPLL]);
}

static int adsp_default_clk_init(struct snd_sof_dev *sdev, bool enable)
{
	struct device *dev = sdev->dev;
	struct adsp_priv *priv = sdev->pdata->hw_pdata;
	int ret;

	dev_dbg(dev, "%s: %s\n", __func__, enable ? "on" : "off");

	if (enable) {
		ret = adsp_enable_all_clock(sdev);
		if (ret) {
			dev_err(dev, "failed to adsp_enable_clock: %d\n", ret);
			return ret;
		}
	} else {
		adsp_disable_all_clock(sdev);
	}

	return 0;
}

int adsp_clock_on(struct snd_sof_dev *sdev)
{
	/* Open ADSP clock */
	return adsp_default_clk_init(sdev, 1);
}

int adsp_clock_off(struct snd_sof_dev *sdev)
{
	/* Close ADSP clock */
	return adsp_default_clk_init(sdev, 0);
}
