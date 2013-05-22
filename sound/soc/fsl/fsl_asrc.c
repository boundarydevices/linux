/*
 * Copyright (C) 2010-2013 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_i2c.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/mxc_asrc.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/dmaengine_pcm.h>
#include <mach/clock.h>

#include "fsl_asrc.h"

static bool filter(struct dma_chan *chan, void *param)
{
	if (!imx_dma_is_general_purpose(chan))
		return false;

	chan->private = param;

	return true;
}

static int asrc_p2p_request_channel(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai     = rtd->cpu_dai;
	struct imx_asrc_p2p *asrc_p2p   = snd_soc_dai_get_drvdata(cpu_dai);
	enum dma_slave_buswidth buswidth = DMA_SLAVE_BUSWIDTH_4_BYTES;
	struct imx_pcm_dma_params *dma_params_be = NULL;
	struct imx_pcm_dma_params *dma_params_fe = NULL;
	struct dma_slave_config slave_config;
	dma_cap_mask_t mask;
	struct dma_chan *chan;
	int ret;
	struct snd_soc_dpcm *dpcm;

	/* find the be for this fe stream */
	list_for_each_entry(dpcm, &rtd->dpcm[substream->stream].be_clients,
								list_be) {
		if (dpcm->fe == rtd) {
			struct snd_soc_pcm_runtime *be = dpcm->be;
			struct snd_soc_dai *dai = be->cpu_dai;
			struct snd_pcm_substream *be_substream;
			be_substream = snd_soc_dpcm_get_substream(be,
							substream->stream);
			dma_params_be = snd_soc_dai_get_dma_data(dai,
								be_substream);
			break;
		}
	}

	if (!dma_params_be) {
		dev_err(rtd->card->dev, "can not get be substream\n");
		return -EINVAL;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dma_params_fe = &asrc_p2p->dma_params_tx;
	else
		dma_params_fe = &asrc_p2p->dma_params_rx;

	/* reconfig memory to FIFO dma request */
	dma_params_fe->dma_addr =
		asrc_p2p->asrc_ops.asrc_p2p_per_addr(
					asrc_p2p->asrc_index, 1);
	dma_params_fe->dma =
		asrc_p2p->asrc_ops.asrc_p2p_get_dma_request(
					asrc_p2p->asrc_index, 1);
	dma_params_fe->burstsize = dma_params_be->burstsize;

	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);
	dma_cap_set(DMA_CYCLIC, mask);

	/* config p2p dma channel */
	asrc_p2p->asrc_p2p_dma_data.peripheral_type = IMX_DMATYPE_ASRC;
	asrc_p2p->asrc_p2p_dma_data.priority        = DMA_PRIO_HIGH;
	asrc_p2p->asrc_p2p_dma_data.dma_request     = asrc_p2p->asrc_ops.
			asrc_p2p_get_dma_request(asrc_p2p->asrc_index, 0);
	/* need to get target device's dma dma_addr, burstsize */
	asrc_p2p->asrc_p2p_dma_data.dma_request_p2p  = dma_params_be->dma;

	/* Request channel */
	asrc_p2p->asrc_p2p_dma_chan =
		dma_request_channel(mask, filter, &asrc_p2p->asrc_p2p_dma_data);

	if (!asrc_p2p->asrc_p2p_dma_chan) {
		dev_err(rtd->card->dev, "can not request dma channel\n");
		goto error;
	}
	chan = asrc_p2p->asrc_p2p_dma_chan;

	slave_config.direction      = DMA_DEV_TO_DEV;
	slave_config.src_addr       = asrc_p2p->asrc_ops.
				asrc_p2p_per_addr(asrc_p2p->asrc_index, 0);
	slave_config.src_addr_width = buswidth;
	slave_config.src_maxburst   = dma_params_be->burstsize;
	slave_config.dst_addr       = dma_params_be->dma_addr;
	slave_config.dst_addr_width = buswidth;
	slave_config.dst_maxburst   = dma_params_be->burstsize;

	ret = dmaengine_slave_config(asrc_p2p->asrc_p2p_dma_chan,
							&slave_config);
	if (ret) {
		dev_err(rtd->card->dev, "can not config dma channel\n");
		goto error;
	}
	asrc_p2p->asrc_p2p_desc = chan->device->device_prep_dma_cyclic(
						chan,
						0xffff,
						64,
						64,
						DMA_DEV_TO_DEV,
						NULL);
	if (!asrc_p2p->asrc_p2p_desc) {
		dev_err(&chan->dev->device,
				"cannot prepare slave dma\n");
		goto error;
	}

	return 0;
error:
	if (asrc_p2p->asrc_p2p_dma_chan) {
		dma_release_channel(asrc_p2p->asrc_p2p_dma_chan);
		asrc_p2p->asrc_p2p_dma_chan = NULL;
	}
	return -EINVAL;
}

static enum asrc_word_width get_asrc_input_width(
			struct snd_pcm_hw_params *params)
{
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_U16:
	case SNDRV_PCM_FORMAT_S16_LE:
	case SNDRV_PCM_FORMAT_S16_BE:
		return ASRC_WIDTH_16_BIT;
	case SNDRV_PCM_FORMAT_S20_3LE:
	case SNDRV_PCM_FORMAT_S20_3BE:
	case SNDRV_PCM_FORMAT_S24_3LE:
	case SNDRV_PCM_FORMAT_S24_3BE:
	case SNDRV_PCM_FORMAT_S24_BE:
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_U24_BE:
	case SNDRV_PCM_FORMAT_U24_LE:
	case SNDRV_PCM_FORMAT_U24_3BE:
	case SNDRV_PCM_FORMAT_U24_3LE:
		return ASRC_WIDTH_24_BIT;
	case SNDRV_PCM_FORMAT_S8:
	case SNDRV_PCM_FORMAT_U8:
	case SNDRV_PCM_FORMAT_S32:
	case SNDRV_PCM_FORMAT_U32:
	default:
		pr_err("Format is not support!\n");
		return -EINVAL;
	}
}

static int config_asrc(struct snd_pcm_substream *substream,
					 struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai     = rtd->cpu_dai;
	struct imx_asrc_p2p *asrc_p2p   = snd_soc_dai_get_drvdata(cpu_dai);
	unsigned int rate    = params_rate(params);
	unsigned int channel = params_channels(params);
	struct asrc_config config = {0};
	int output_word_width = 0;
	int ret = 0;
	if ((channel != 2) && (channel != 4) && (channel != 6)) {
		pr_err("param channel is not correct\n");
		return -EINVAL;
	}

	ret = asrc_p2p->asrc_ops.asrc_p2p_req_pair(channel,
					&asrc_p2p->asrc_index);
	if (ret < 0) {
		pr_err("Fail to request asrc pair\n");
		return -EINVAL;
	}

	if (asrc_p2p->output_width == 16)
		output_word_width = ASRC_WIDTH_16_BIT;
	else
		output_word_width = ASRC_WIDTH_24_BIT;

	config.input_word_width   = get_asrc_input_width(params);
	config.output_word_width  = output_word_width;
	config.pair               = asrc_p2p->asrc_index;
	config.channel_num        = channel;
	config.input_sample_rate  = rate;
	config.output_sample_rate = asrc_p2p->output_rate;
	config.inclk              = INCLK_NONE;
	config.outclk             = OUTCLK_ESAI_TX;

	ret = asrc_p2p->asrc_ops.asrc_p2p_config_pair(&config);
	if (ret < 0) {
		pr_err("Fail to config asrc\n");
		return ret;
	}

	return 0;
}

static int imx_asrc_p2p_startup(struct snd_pcm_substream *substream,
			    struct snd_soc_dai *cpu_dai)
{
	struct imx_asrc_p2p *asrc_p2p   = snd_soc_dai_get_drvdata(cpu_dai);
	struct imx_pcm_dma_params *dma_params;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dma_params = &asrc_p2p->dma_params_tx;
	else
		dma_params = &asrc_p2p->dma_params_rx;

	snd_soc_dai_set_dma_data(cpu_dai, substream, dma_params);
	return 0;
}

static int imx_asrc_p2p_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *cpu_dai)
{
	struct imx_asrc_p2p *asrc_p2p   = snd_soc_dai_get_drvdata(cpu_dai);
	struct imx_pcm_dma_params *dma_params;
	int ret = 0;
	ret = config_asrc(substream, params);
	if (ret < 0)
		return ret;

	ret = asrc_p2p_request_channel(substream);
	if (ret)
		return ret;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dma_params = &asrc_p2p->dma_params_tx;
	else
		dma_params = &asrc_p2p->dma_params_rx;

	/* reconfig memory to FIFO dma request */
	dma_params->dma_addr =
		asrc_p2p->asrc_ops.asrc_p2p_per_addr(asrc_p2p->asrc_index, 1);
	dma_params->dma =
		asrc_p2p->asrc_ops.
			asrc_p2p_get_dma_request(asrc_p2p->asrc_index, 1);

	return ret;
}

static int imx_asrc_p2p_hw_free(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *cpu_dai)
{
	struct imx_asrc_p2p *asrc_p2p = snd_soc_dai_get_drvdata(cpu_dai);

	if (asrc_p2p->asrc_p2p_dma_chan) {
		/* Release p2p dma resource */
		dma_release_channel(asrc_p2p->asrc_p2p_dma_chan);
		asrc_p2p->asrc_p2p_dma_chan = NULL;
	}

	if (asrc_p2p->asrc_index != -1) {
		asrc_p2p->asrc_ops.asrc_p2p_release_pair(asrc_p2p->asrc_index);
		asrc_p2p->asrc_ops.asrc_p2p_finish_conv(asrc_p2p->asrc_index);
	}
	asrc_p2p->asrc_index = -1;


	return 0;
}

static int imx_asrc_p2p_trigger(struct snd_pcm_substream *substream, int cmd,
			    struct snd_soc_dai *cpu_dai)
{
	struct imx_asrc_p2p *asrc_p2p = snd_soc_dai_get_drvdata(cpu_dai);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		dmaengine_submit(asrc_p2p->asrc_p2p_desc);
		dma_async_issue_pending(asrc_p2p->asrc_p2p_dma_chan);
		asrc_p2p->asrc_ops.asrc_p2p_start_conv(asrc_p2p->asrc_index);
		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_STOP:
		dmaengine_terminate_all(asrc_p2p->asrc_p2p_dma_chan);
		asrc_p2p->asrc_ops.asrc_p2p_stop_conv(asrc_p2p->asrc_index);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

#define IMX_ASRC_RATES  SNDRV_PCM_RATE_8000_192000

#define IMX_ASRC_FORMATS \
	(SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S16_LE | \
	SNDRV_PCM_FORMAT_S20_3LE)

static struct snd_soc_dai_ops imx_asrc_p2p_dai_ops = {
	.startup      = imx_asrc_p2p_startup,
	.trigger      = imx_asrc_p2p_trigger,
	.hw_params    = imx_asrc_p2p_hw_params,
	.hw_free      = imx_asrc_p2p_hw_free,
};

static struct snd_soc_dai_driver imx_asrc_p2p_dai = {
	.playback = {
		.stream_name = "asrc-playback",
		.channels_min = 1,
		.channels_max = 12,
		.rates = IMX_ASRC_RATES,
		.formats = IMX_ASRC_FORMATS,
	},
	.capture  = {
		.stream_name = "asrc-capture",
		.channels_min = 1,
		.channels_max = 4,
		.rates = IMX_ASRC_RATES,
		.formats = IMX_ASRC_FORMATS,
	},
	.ops      = &imx_asrc_p2p_dai_ops,
};

/*
 * This function will register the snd_soc_pcm_link drivers.
 */
static int __devinit imx_asrc_p2p_probe(struct platform_device *pdev)
{
	struct imx_asrc_p2p *asrc_p2p;
	struct device_node *np = pdev->dev.of_node;
	const char *p;
	const uint32_t *iprop_rate, *iprop_width;
	int ret = 0;

	if (!of_device_is_available(np)) {
		dev_err(&pdev->dev, "There is no device node\n");
		return -ENODEV;
	}

	asrc_p2p = kzalloc(sizeof(struct imx_asrc_p2p), GFP_KERNEL);
	if (!asrc_p2p) {
		dev_err(&pdev->dev, "can not alloc memory\n");
		return -ENOMEM;
	}
	asrc_p2p->asrc_ops.asrc_p2p_start_conv      = asrc_start_conv;
	asrc_p2p->asrc_ops.asrc_p2p_stop_conv       = asrc_stop_conv;
	asrc_p2p->asrc_ops.asrc_p2p_get_dma_request = asrc_get_dma_request;
	asrc_p2p->asrc_ops.asrc_p2p_per_addr        = asrc_get_per_addr;
	asrc_p2p->asrc_ops.asrc_p2p_req_pair        = asrc_req_pair;
	asrc_p2p->asrc_ops.asrc_p2p_config_pair     = asrc_config_pair;
	asrc_p2p->asrc_ops.asrc_p2p_release_pair    = asrc_release_pair;
	asrc_p2p->asrc_ops.asrc_p2p_finish_conv     = asrc_finish_conv;

	asrc_p2p->asrc_index = -1;

	iprop_rate = of_get_property(np, "output-rate", NULL);
	if (iprop_rate)
		asrc_p2p->output_rate = be32_to_cpup(iprop_rate);
	else {
		dev_err(&pdev->dev, "There is no output-rate in dts\n");
		return -EINVAL;
	}
	iprop_width = of_get_property(np, "output-width", NULL);
	if (iprop_width)
		asrc_p2p->output_width = be32_to_cpup(iprop_width);

	asrc_p2p->dma_params_tx.peripheral_type = IMX_DMATYPE_ASRC;
	asrc_p2p->dma_params_rx.peripheral_type = IMX_DMATYPE_ASRC;

	platform_set_drvdata(pdev, asrc_p2p);

	p = strrchr(np->full_name, '/') + 1;
	strcpy(asrc_p2p->name, p);
	imx_asrc_p2p_dai.name = asrc_p2p->name;

	ret = snd_soc_register_dai(&pdev->dev, &imx_asrc_p2p_dai);
	if (ret) {
		dev_err(&pdev->dev, "register DAI failed\n");
		goto failed_register;
	}

	return 0;

failed_register:
	kfree(asrc_p2p);

	return ret;
}

static int __devexit imx_asrc_p2p_remove(struct platform_device *pdev)
{
	struct imx_asrc_p2p *asrc_p2p = platform_get_drvdata(pdev);

	snd_soc_unregister_dai(&pdev->dev);

	kfree(asrc_p2p);
	return 0;
}

static const struct of_device_id imx_asrc_p2p_dt_ids[] = {
	{ .compatible = "fsl,imx6q-asrc-p2p", },
	{ /* sentinel */ }
};

static struct platform_driver imx_asrc_p2p_driver = {
	.probe = imx_asrc_p2p_probe,
	.remove = __devexit_p(imx_asrc_p2p_remove),
	.driver = {
		.name = "imx-asrc-p2p",
		.owner = THIS_MODULE,
		.of_match_table = imx_asrc_p2p_dt_ids,
	},
};
module_platform_driver(imx_asrc_p2p_driver);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("i.MX ASoC ASRC P2P driver");
MODULE_LICENSE("GPL");
