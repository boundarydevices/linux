/*
 * Copyright 2008-2013 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

 /*!
  * @file       fsl-esai.c
  * @brief      this file implements the esai interface
  *             in according to ASoC architeture
  */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/pinctrl/consumer.h>

#include <linux/clk.h>
#include <linux/delay.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <mach/clock.h>
#include <asm/mach-types.h>

#include "fsl_esai.h"
#include "fsl_asrc.h"
#include "imx-pcm.h"

#define IMX_ESAI_NET            (1 << 0)
#define IMX_ESAI_SYN            (1 << 1)

static struct imx_esai *local_esai;

static int imx_esai_set_dai_sysclk(struct snd_soc_dai *cpu_dai,
				   int clk_id, unsigned int freq, int dir)
{
	struct imx_esai *esai = snd_soc_dai_get_drvdata(cpu_dai);
	u32 ecr, tccr, rccr;

	ecr = readl(esai->base + ESAI_ECR);
	tccr = readl(esai->base + ESAI_TCCR);
	rccr = readl(esai->base + ESAI_RCCR);

	if (dir == SND_SOC_CLOCK_IN) {
		tccr &= ~(ESAI_TCCR_THCKD | ESAI_TCCR_TCKD | ESAI_TCCR_TFSD);
		rccr &= ~(ESAI_RCCR_RHCKD | ESAI_RCCR_RCKD | ESAI_RCCR_RFSD);
	} else {
		tccr |= ESAI_TCCR_THCKD | ESAI_TCCR_TCKD | ESAI_TCCR_TFSD;
		rccr |= ESAI_RCCR_RHCKD | ESAI_RCCR_RCKD | ESAI_RCCR_RFSD;

		if (clk_id == ESAI_CLK_FSYS) {
			ecr &= ~(ESAI_ECR_ETI | ESAI_ECR_ETO);
			ecr &= ~(ESAI_ECR_ERI | ESAI_ECR_ERO);
		} else if (clk_id == ESAI_CLK_EXTAL) {
			ecr |= ESAI_ECR_ETI;
			ecr |= ESAI_ECR_ETO;
			ecr |= ESAI_ECR_ERI;
			ecr |= ESAI_ECR_ERO;
		} else if (clk_id == ESAI_CLK_EXTAL_DIV) {
			ecr |= ESAI_ECR_ETI;
			ecr &= ~ESAI_ECR_ETO;
			ecr |= ESAI_ECR_ERI;
			ecr &= ~ESAI_ECR_ERO;
		}
	}

	writel(ecr, esai->base + ESAI_ECR);
	writel(tccr, esai->base + ESAI_TCCR);
	writel(rccr, esai->base + ESAI_RCCR);

	ESAI_DUMP();
	return 0;
}

static int imx_esai_set_dai_clkdiv(struct snd_soc_dai *cpu_dai,
				   int div_id, int div)
{
	struct imx_esai *esai = snd_soc_dai_get_drvdata(cpu_dai);
	u32 tccr, rccr;

	tccr = readl(esai->base + ESAI_TCCR);
	rccr = readl(esai->base + ESAI_RCCR);

	switch (div_id) {
	case ESAI_TX_DIV_PSR:
		if ((div << ESAI_TCCR_TPSR_SHIFT) & ESAI_TCCR_TPSR_MASK)
			return -EINVAL;
		tccr &= ESAI_TCCR_TPSR_MASK;
		if (div)
			tccr |= ESAI_TCCR_TPSR_BYPASS;
		else
			tccr &= ~ESAI_TCCR_TPSR_DIV8;
		break;
	case ESAI_TX_DIV_PM:
		if ((div << ESAI_TCCR_TPM_SHIFT) & ESAI_TCCR_TPM_MASK)
			return -EINVAL;
		tccr &= ESAI_TCCR_TPM_MASK;
		tccr |= ESAI_TCCR_TPM(div);
		break;
	case ESAI_TX_DIV_FP:
		if ((div << ESAI_TCCR_TFP_SHIFT) & ESAI_TCCR_TFP_MASK)
			return -EINVAL;
		tccr &= ESAI_TCCR_TFP_MASK;
		tccr |= ESAI_TCCR_TFP(div);
		break;
	case ESAI_RX_DIV_PSR:
		if ((div << ESAI_RCCR_RPSR_SHIFT) & ESAI_RCCR_RPSR_MASK)
			return -EINVAL;
		rccr &= ESAI_RCCR_RPSR_MASK;
		if (div)
			rccr |= ESAI_RCCR_RPSR_BYPASS;
		else
			rccr &= ~ESAI_RCCR_RPSR_DIV8;
		break;
	case ESAI_RX_DIV_PM:
		if ((div << ESAI_RCCR_RPM_SHIFT) & ESAI_RCCR_RPM_MASK)
			return -EINVAL;
		rccr &= ESAI_RCCR_RPM_MASK;
		rccr |= ESAI_RCCR_RPM(div);
		break;
	case ESAI_RX_DIV_FP:
		if ((div << ESAI_RCCR_RFP_SHIFT) & ESAI_RCCR_RFP_MASK)
			return -EINVAL;
		rccr &= ESAI_RCCR_RFP_MASK;
		rccr |= ESAI_RCCR_RFP(div);
		break;
	default:
		return -EINVAL;
	}
	writel(tccr, esai->base + ESAI_TCCR);
	writel(rccr, esai->base + ESAI_RCCR);
	return 0;
}

/*
 * ESAI Network Mode or TDM slots configuration.
 */
static int imx_esai_set_dai_tdm_slot(struct snd_soc_dai *cpu_dai,
	unsigned int tx_mask, unsigned int rx_mask, int slots, int slot_width)
{
	struct imx_esai *esai = snd_soc_dai_get_drvdata(cpu_dai);
	u32 tccr, rccr;

	tccr = readl(esai->base + ESAI_TCCR);

	tccr &= ESAI_TCCR_TDC_MASK;
	tccr |= ESAI_TCCR_TDC(slots - 1);

	writel(tccr, esai->base + ESAI_TCCR);
	writel((tx_mask & 0xffff), esai->base + ESAI_TSMA);
	writel(((tx_mask >> 16) & 0xffff), esai->base + ESAI_TSMB);

	rccr = readl(esai->base + ESAI_RCCR);

	rccr &= ESAI_RCCR_RDC_MASK;
	rccr |= ESAI_RCCR_RDC(slots - 1);

	writel(rccr, esai->base + ESAI_RCCR);
	writel((rx_mask & 0xffff), esai->base + ESAI_RSMA);
	writel(((rx_mask >> 16) & 0xffff), esai->base + ESAI_RSMB);

	ESAI_DUMP();
	return 0;
}

/*
 * ESAI DAI format configuration.
 */
static int imx_esai_set_dai_fmt(struct snd_soc_dai *cpu_dai, unsigned int fmt)
{
	struct imx_esai *esai = snd_soc_dai_get_drvdata(cpu_dai);
	u32 tcr, tccr, rcr, rccr, saicr;

	tcr = readl(esai->base + ESAI_TCR);
	tccr = readl(esai->base + ESAI_TCCR);
	rcr = readl(esai->base + ESAI_RCR);
	rccr = readl(esai->base + ESAI_RCCR);
	saicr = readl(esai->base + ESAI_SAICR);

	/* DAI mode */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		/* data on rising edge of bclk, frame low 1clk before data */
		tcr &= ~ESAI_TCR_TFSL;
		tcr |= ESAI_TCR_TFSR;
		rcr &= ~ESAI_RCR_RFSL;
		rcr |= ESAI_RCR_RFSR;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		/* data on rising edge of bclk, frame high with data */
		tcr &= ~(ESAI_TCR_TFSL | ESAI_TCR_TFSR);
		rcr &= ~(ESAI_RCR_RFSL | ESAI_RCR_RFSR);
		break;
	case SND_SOC_DAIFMT_DSP_B:
		/* data on rising edge of bclk, frame high with data */
		tcr |= ESAI_TCR_TFSL;
		rcr |= ESAI_RCR_RFSL;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		/* data on rising edge of bclk, frame high 1clk before data */
		tcr |= ESAI_TCR_TFSL;
		rcr |= ESAI_RCR_RFSL;
		break;
	default:
		return -EINVAL;
	}

	/* DAI clock inversion */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_IB_IF:
		tccr |= ESAI_TCCR_TFSP;
		tccr &= ~(ESAI_TCCR_TCKP | ESAI_TCCR_THCKP);
		rccr &= ~(ESAI_RCCR_RCKP | ESAI_RCCR_RHCKP);
		rccr |= ESAI_RCCR_RFSP;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		tccr &= ~(ESAI_TCCR_TCKP | ESAI_TCCR_THCKP | ESAI_TCCR_TFSP);
		rccr &= ~(ESAI_RCCR_RCKP | ESAI_RCCR_RHCKP | ESAI_RCCR_RFSP);
		break;
	case SND_SOC_DAIFMT_NB_IF:
		tccr |= ESAI_TCCR_TCKP | ESAI_TCCR_THCKP | ESAI_TCCR_TFSP;
		rccr |= ESAI_RCCR_RCKP | ESAI_RCCR_RHCKP | ESAI_RCCR_RFSP;
		break;
	case SND_SOC_DAIFMT_NB_NF:
		tccr &= ~ESAI_TCCR_TFSP;
		tccr |= ESAI_TCCR_TCKP | ESAI_TCCR_THCKP;
		rccr &= ~ESAI_RCCR_RFSP;
		rccr |= ESAI_RCCR_RCKP | ESAI_RCCR_RHCKP;
		break;
	default:
		return -EINVAL;
	}

	/* DAI clock master masks */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		tccr &= ~(ESAI_TCCR_TFSD | ESAI_TCCR_TCKD);
		rccr &= ~(ESAI_RCCR_RFSD | ESAI_RCCR_RCKD);
		break;
	case SND_SOC_DAIFMT_CBS_CFM:
		tccr &= ~ESAI_TCCR_TFSD;
		tccr |= ESAI_TCCR_TCKD;
		rccr &= ~ESAI_RCCR_RFSD;
		rccr |= ESAI_RCCR_RCKD;
		break;
	case SND_SOC_DAIFMT_CBM_CFS:
		tccr &= ~ESAI_TCCR_TCKD;
		tccr |= ESAI_TCCR_TFSD;
		rccr &= ~ESAI_RCCR_RCKD;
		rccr |= ESAI_RCCR_RFSD;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		tccr |= (ESAI_TCCR_TFSD | ESAI_TCCR_TCKD);
		rccr |= (ESAI_RCCR_RFSD | ESAI_RCCR_RCKD);
		break;
	default:
		return -EINVAL;
	}

	/* sync */
	if (esai->flags & IMX_ESAI_SYN)
		saicr |= ESAI_SAICR_SYNC;
	else
		saicr &= ~ESAI_SAICR_SYNC;

	tcr &= ESAI_TCR_TMOD_MASK;
	rcr &= ESAI_RCR_RMOD_MASK;
	if (esai->flags & IMX_ESAI_NET) {
		tcr |= ESAI_TCR_TMOD_NETWORK;
		rcr |= ESAI_RCR_RMOD_NETWORK;
	} else {
		tcr |= ESAI_TCR_TMOD_NORMAL;
		rcr |= ESAI_RCR_RMOD_NORMAL;
	}

	writel(tcr, esai->base + ESAI_TCR);
	writel(tccr, esai->base + ESAI_TCCR);
	writel(rcr, esai->base + ESAI_RCR);
	writel(rccr, esai->base + ESAI_RCCR);

	writel(saicr, esai->base + ESAI_SAICR);

	ESAI_DUMP();
	return 0;
}

static int imx_esai_startup(struct snd_pcm_substream *substream,
			    struct snd_soc_dai *cpu_dai)
{
	struct imx_esai *esai = snd_soc_dai_get_drvdata(cpu_dai);
	struct imx_pcm_dma_params *dma_data;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK &&
			(local_esai->imx_esai_txrx_state & IMX_DAI_ESAI_TX)) {
		pr_err("error: too much esai playback!\n");
		return -EINVAL;
	}

	if (!(local_esai->imx_esai_txrx_state & IMX_DAI_ESAI_TXRX)) {
		clk_prepare_enable(esai->clk);

		writel(ESAI_GPIO_ESAI, esai->base + ESAI_PRRC);
		writel(ESAI_GPIO_ESAI, esai->base + ESAI_PCRC);
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		local_esai->imx_esai_txrx_state |= IMX_DAI_ESAI_TX;
		dma_data = &esai->pcm_params.dma_params_tx;
	} else {
		local_esai->imx_esai_txrx_state |= IMX_DAI_ESAI_RX;
		dma_data = &esai->pcm_params.dma_params_rx;
	}

	snd_soc_dai_set_dma_data(cpu_dai, substream, dma_data);

	ESAI_DUMP();
	return 0;
}

/*
 * This function is called to initialize the TX port before enable
 * the tx port.
 */
static int imx_esai_hw_tx_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *cpu_dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct imx_esai *esai = snd_soc_dai_get_drvdata(cpu_dai);
	u32 tcr, tfcr;
	unsigned int channels;
	struct snd_soc_dpcm *dpcm;
	struct imx_asrc_p2p *asrc_p2p = NULL;

	/*currently we only have one fe, asrc, if someday we have multi fe,
	then we must think about to distinct them.*/
	list_for_each_entry(dpcm, &rtd->dpcm[substream->stream].fe_clients,
								list_fe) {
		if (dpcm->be == rtd) {
			struct snd_soc_pcm_runtime *fe = dpcm->fe;
			struct snd_soc_dai *dai = fe->cpu_dai;
			asrc_p2p = snd_soc_dai_get_drvdata(dai);
			break;
		}
	}

	tcr = readl(esai->base + ESAI_TCR);
	tfcr = readl(esai->base + ESAI_TFCR);

	tfcr |= ESAI_TFCR_TFR;
	writel(tfcr, esai->base + ESAI_TFCR);
	tfcr &= ~ESAI_TFCR_TFR;
	/* DAI data (word) size */
	tfcr &= ESAI_TFCR_TWA_MASK;
	tcr &= ESAI_TCR_TSWS_MASK;

	if (asrc_p2p) {
		if (asrc_p2p->output_width == 16) {
			tfcr |= ESAI_WORD_LEN_16;
			tcr |= ESAI_TCR_TSHFD_MSB | ESAI_TCR_TSWS_STL32_WDL16;
		} else {
			tfcr |= ESAI_WORD_LEN_24;
			tcr |= ESAI_TCR_TSHFD_MSB | ESAI_TCR_TSWS_STL32_WDL24;
		}
	} else {
		switch (params_format(params)) {
		case SNDRV_PCM_FORMAT_S16_LE:
			tfcr |= ESAI_WORD_LEN_16;
			tcr |= ESAI_TCR_TSHFD_MSB | ESAI_TCR_TSWS_STL32_WDL16;
			break;
		case SNDRV_PCM_FORMAT_S20_3LE:
			tfcr |= ESAI_WORD_LEN_20;
			tcr |= ESAI_TCR_TSHFD_MSB | ESAI_TCR_TSWS_STL32_WDL20;
			break;
		case SNDRV_PCM_FORMAT_S24_LE:
			tfcr |= ESAI_WORD_LEN_24;
			tcr |= ESAI_TCR_TSHFD_MSB | ESAI_TCR_TSWS_STL32_WDL24;
			break;
		default:
			return -EINVAL;
		}
	}

	channels = params_channels(params);
	tfcr &= ESAI_TFCR_TE_MASK;
	tfcr |= ESAI_TFCR_TE(channels);

	tfcr |= ESAI_TFCR_TFWM(64);

	/* Left aligned, Zero padding */
	tcr |= ESAI_TCR_PADC;
	/* TDR initialized from the FIFO */
	tfcr |= ESAI_TFCR_TIEN;

	writel(tcr, esai->base + ESAI_TCR);
	writel(tfcr, esai->base + ESAI_TFCR);

	ESAI_DUMP();
	return 0;
}

/*
 * This function is called to initialize the RX port before enable
 * the rx port.
 */
static int imx_esai_hw_rx_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *cpu_dai)
{
	struct imx_esai *esai = snd_soc_dai_get_drvdata(cpu_dai);
	u32 rcr, rfcr;
	unsigned int channels;

	rcr = readl(esai->base + ESAI_RCR);
	rfcr = readl(esai->base + ESAI_RFCR);

	rfcr |= ESAI_RFCR_RFR;
	writel(rfcr, esai->base + ESAI_RFCR);
	rfcr &= ~ESAI_RFCR_RFR;

	rfcr &= ESAI_RFCR_RWA_MASK;
	rcr &= ESAI_RCR_RSWS_MASK;
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		rfcr |= ESAI_WORD_LEN_16;
		rcr |= ESAI_RCR_RSHFD_MSB | ESAI_RCR_RSWS_STL32_WDL16;
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		rfcr |= ESAI_WORD_LEN_20;
		rcr |= ESAI_RCR_RSHFD_MSB | ESAI_RCR_RSWS_STL32_WDL20;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		rfcr |= ESAI_WORD_LEN_24;
		rcr |= ESAI_RCR_RSHFD_MSB | ESAI_RCR_RSWS_STL32_WDL24;
		break;
	default:
		return -EINVAL;
	}

	channels = params_channels(params);
	rfcr &= ESAI_RFCR_RE_MASK;
	rfcr |= ESAI_RFCR_RE(channels);

	rfcr |= ESAI_RFCR_RFWM(64);

	writel(rcr, esai->base + ESAI_RCR);
	writel(rfcr, esai->base + ESAI_RFCR);

	ESAI_DUMP();
	return 0;
}

/*
 * This function is called to initialize the TX or RX port,
 */
static int imx_esai_hw_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *params,
			      struct snd_soc_dai *cpu_dai)
{
	struct imx_esai *esai = snd_soc_dai_get_drvdata(cpu_dai);

	/* Tx/Rx config */
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (readl(esai->base + ESAI_TCR) & ESAI_TCR_TE0)
			return 0;

		return imx_esai_hw_tx_params(substream, params, cpu_dai);
	} else {
		if (readl(esai->base + ESAI_RCR) & ESAI_RCR_RE1)
			return 0;

		return imx_esai_hw_rx_params(substream, params, cpu_dai);
	}
}

static void imx_esai_shutdown(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *cpu_dai)
{
	struct imx_esai *esai = snd_soc_dai_get_drvdata(cpu_dai);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		local_esai->imx_esai_txrx_state &= ~IMX_DAI_ESAI_TX;
	else
		local_esai->imx_esai_txrx_state &= ~IMX_DAI_ESAI_RX;

	if (!(local_esai->imx_esai_txrx_state & IMX_DAI_ESAI_TXRX))
		/* close easi clock */
		clk_disable_unprepare(esai->clk);
}

static int imx_esai_trigger(struct snd_pcm_substream *substream, int cmd,
			    struct snd_soc_dai *cpu_dai)
{
	struct imx_esai *esai = snd_soc_dai_get_drvdata(cpu_dai);
	u32 reg, tfcr = 0, rfcr = 0;
	int i;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		tfcr = readl(esai->base + ESAI_TFCR);
		reg = readl(esai->base + ESAI_TCR);
	} else {
		rfcr = readl(esai->base + ESAI_RFCR);
		reg = readl(esai->base + ESAI_RCR);
	}
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			tfcr |= ESAI_TFCR_TFEN;
			writel(tfcr, esai->base + ESAI_TFCR);
			/* write initial words to ETDR register */
			for (i = 0; i < substream->runtime->channels; i++)
				writel(0x0, esai->base + ESAI_ETDR);
			reg |= ESAI_TCR_TE(substream->runtime->channels);
			writel(reg, esai->base + ESAI_TCR);
		} else {
			rfcr |= ESAI_RFCR_RFEN;
			writel(rfcr, esai->base + ESAI_RFCR);
			reg |= ESAI_RCR_RE(substream->runtime->channels);
			writel(reg, esai->base + ESAI_RCR);
		}
		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			reg &= ~ESAI_TCR_TE(substream->runtime->channels);
			writel(reg, esai->base + ESAI_TCR);
			tfcr |= ESAI_TFCR_TFR;
			tfcr &= ~ESAI_TFCR_TFEN;
			writel(tfcr, esai->base + ESAI_TFCR);
			tfcr &= ~ESAI_TFCR_TFR;
			writel(tfcr, esai->base + ESAI_TFCR);
		} else {
			reg &= ~ESAI_RCR_RE(substream->runtime->channels);
			writel(reg, esai->base + ESAI_RCR);
			rfcr |= ESAI_RFCR_RFR;
			rfcr &= ~ESAI_RFCR_RFEN;
			writel(rfcr, esai->base + ESAI_RFCR);
			rfcr &= ~ESAI_RFCR_RFR;
			writel(rfcr, esai->base + ESAI_RFCR);
		}
		break;
	default:
		return -EINVAL;
	}

	ESAI_DUMP();
	return 0;
}

#define IMX_ESAI_RATES  SNDRV_PCM_RATE_8000_192000

#define IMX_ESAI_FORMATS \
	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | \
	SNDRV_PCM_FMTBIT_S24_LE)

static struct snd_soc_dai_ops imx_esai_dai_ops = {
	.startup = imx_esai_startup,
	.shutdown = imx_esai_shutdown,
	.trigger = imx_esai_trigger,
	.hw_params = imx_esai_hw_params,
	.set_sysclk = imx_esai_set_dai_sysclk,
	.set_clkdiv = imx_esai_set_dai_clkdiv,
	.set_fmt = imx_esai_set_dai_fmt,
	.set_tdm_slot = imx_esai_set_dai_tdm_slot,
};

static int imx_esai_dai_probe(struct snd_soc_dai *dai)
{
	local_esai->imx_esai_txrx_state = 0;
	return 0;
}

static struct snd_soc_dai_driver imx_esai_dai = {
	.probe = imx_esai_dai_probe,
	.playback = {
		.stream_name = "esai-playback",
		.channels_min = 1,
		.channels_max = 12,
		.rates = IMX_ESAI_RATES,
		.formats = IMX_ESAI_FORMATS,
	},
	.capture = {
		.stream_name = "esai-capture",
		.channels_min = 1,
		.channels_max = 8,
		.rates = IMX_ESAI_RATES,
		.formats = IMX_ESAI_FORMATS,
	},
	.ops = &imx_esai_dai_ops,
};

static int imx_esai_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct pinctrl *pinctrl;
	struct imx_esai *esai;
	const uint32_t *iprop;
	uint32_t flag;
	const char *p;
	u32 dma_events[2];
	u32 fifo_depth;
	struct device_node *np = pdev->dev.of_node;
	int ret = 0;

	if (!of_device_is_available(np))
		return -ENODEV;

	esai = kzalloc(sizeof(*esai), GFP_KERNEL);
	if (!esai) {
		dev_err(&pdev->dev, "mem allocation failed\n");
		return -ENOMEM;
	}
	local_esai = esai;

	ret = of_property_read_u32(np, "flags", &flag);
	if (ret < 0) {
		dev_err(&pdev->dev, "There is no flag for esai\n");
		goto error_kmalloc;
	}
	esai->flags = flag;

	esai->irq = platform_get_irq(pdev, 0);
	if (esai->irq == NO_IRQ) {
		dev_err(&pdev->dev, "no irq for node %s\n", np->full_name);
		ret = -ENXIO;
		goto error_kmalloc;
	}

	pinctrl = devm_pinctrl_get_select_default(&pdev->dev);
	if (IS_ERR(pinctrl)) {
		dev_err(&pdev->dev, "setup pinctrl failed!");
		return PTR_ERR(pinctrl);
	}

	esai->clk = clk_get(NULL, "esai");
	if (IS_ERR(esai->clk)) {
		ret = PTR_ERR(esai->clk);
		dev_err(&pdev->dev, "Cannot get the clock: %d\n",
			ret);
		goto error_kmalloc;
	}
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "could not determine device resources\n");
		goto failed_get_resource;
	}

	esai->base = devm_request_and_ioremap(&pdev->dev, res);
	if (!esai->base) {
		dev_err(&pdev->dev, "could not map device resources\n");
		ret = -ENOMEM;
		goto failed_ioremap;
	}

	/* Determine the FIFO depth. */
	iprop = of_get_property(np, "fsl,fifo-depth", NULL);
	if (iprop)
		fifo_depth = be32_to_cpup(iprop)/4;
	else
		fifo_depth = 32;

	esai->pcm_params.dma_params_tx.burstsize = fifo_depth;
	esai->pcm_params.dma_params_rx.burstsize = fifo_depth;

	esai->pcm_params.dma_params_tx.dma_addr = res->start + ESAI_ETDR;
	esai->pcm_params.dma_params_rx.dma_addr = res->start + ESAI_ERDR;

	esai->pcm_params.dma_params_tx.peripheral_type = IMX_DMATYPE_ESAI;
	esai->pcm_params.dma_params_rx.peripheral_type = IMX_DMATYPE_ESAI;

	esai->pcm_params.dma_params_tx.dma_buf_size = IMX_ESAI_DMABUF_SIZE;
	esai->pcm_params.dma_params_rx.dma_buf_size = IMX_ESAI_DMABUF_SIZE;

	ret = of_property_read_u32_array(pdev->dev.of_node,
				"fsl,esai-dma-events", dma_events, 2);
	if (ret) {
		dev_err(&pdev->dev, "could not get dma events\n");
		goto failed_ioremap;
	}
	esai->pcm_params.dma_params_tx.dma = dma_events[0];
	esai->pcm_params.dma_params_rx.dma = dma_events[1];

	platform_set_drvdata(pdev, esai);

	p = strrchr(np->full_name, '/') + 1;
	strcpy(esai->name, p);
	imx_esai_dai.name = esai->name;

	ret = snd_soc_register_dai(&pdev->dev, &imx_esai_dai);
	if (ret) {
		dev_err(&pdev->dev, "register DAI failed\n");
		goto failed_register;
	}

	pdev->id = of_alias_get_id(np, "audio");
	if (pdev->id < 0) {
		dev_err(&pdev->dev, "Missing alias for esai in devicetree");
		goto failed_get_alias;
	}

	esai->soc_platform_pdev_fiq = \
		platform_device_alloc("imx-fiq-pcm-audio", pdev->id);
	if (!esai->soc_platform_pdev_fiq) {
		ret = -ENOMEM;
		goto failed_pdev_fiq_alloc;
	}

	platform_set_drvdata(esai->soc_platform_pdev_fiq, &esai->pcm_params);
	ret = platform_device_add(esai->soc_platform_pdev_fiq);
	if (ret) {
		dev_err(&pdev->dev, "failed to add platform device\n");
		goto failed_pdev_fiq_add;
	}

	esai->soc_platform_pdev = \
		platform_device_alloc("imx-pcm-audio", pdev->id);
	if (!esai->soc_platform_pdev) {
		ret = -ENOMEM;
		goto failed_pdev_alloc;
	}

	platform_set_drvdata(esai->soc_platform_pdev, &esai->pcm_params);
	ret = platform_device_add(esai->soc_platform_pdev);
	if (ret) {
		dev_err(&pdev->dev, "failed to add platform device\n");
		goto failed_pdev_add;
	}

	writel(ESAI_ECR_ERST, esai->base + ESAI_ECR);
	writel(ESAI_ECR_ESAIEN, esai->base + ESAI_ECR);

	return 0;

failed_pdev_add:
	platform_device_put(esai->soc_platform_pdev);
failed_pdev_alloc:
	platform_device_del(esai->soc_platform_pdev_fiq);
failed_pdev_fiq_add:
	platform_device_put(esai->soc_platform_pdev_fiq);
failed_pdev_fiq_alloc:
failed_get_alias:
	snd_soc_unregister_dai(&pdev->dev);
failed_register:
failed_ioremap:
	iounmap(esai->base);
failed_get_resource:
	clk_disable(esai->clk);
	clk_put(esai->clk);
error_kmalloc:
	kfree(esai);

	return ret;
}
static int __devexit imx_esai_remove(struct platform_device *pdev)
{
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	struct imx_esai *esai = platform_get_drvdata(pdev);

	platform_device_unregister(esai->soc_platform_pdev);
	platform_device_unregister(esai->soc_platform_pdev_fiq);

	snd_soc_unregister_dai(&pdev->dev);

	iounmap(esai->base);
	release_mem_region(res->start, resource_size(res));
	clk_put(esai->clk);
	kfree(esai);

	return 0;
}

static const struct of_device_id fsl_esai_ids[] = {
	{ .compatible = "fsl,imx6q-esai", },
	{}
};


static struct platform_driver imx_esai_driver = {
	.probe = imx_esai_probe,
	.remove = __devexit_p(imx_esai_remove),
	.driver = {
		.name = "imx-esai",
		.owner = THIS_MODULE,
		.of_match_table = fsl_esai_ids,
	},
};

module_platform_driver(imx_esai_driver);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("i.MX ASoC ESAI driver");
MODULE_LICENSE("GPL");
