// SPDX-License-Identifier: GPL-2.0
/*
 *  MediaTek ALSA SoC AFE platform driver for 8189
 *
 *  Copyright (c) 2025 MediaTek Inc.
 *  Author: Darren Ye <darren.ye@mediatek.com>
 */

#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/pm_runtime.h>
#include <sound/soc.h>
#include <linux/regmap.h>
#include <linux/of_device.h>
#include <linux/of_reserved_mem.h>

#include "mtk-afe-debug.h"
#include "mt8189-afe-common.h"
#include "mtk-afe-platform-driver.h"
#include "mtk-afe-fe-dai.h"
#include "mt8189-afe-clk.h"
#include "mt8189-interconnection.h"

/* skip interconn debug read reg for dram size constrict */
#define SKIP_INTERCONN_DRAM_SIZE

#define AFE_SYS_DEBUG_SIZE (1024 * 64) // 64K
#define MAX_DEBUG_WRITE_INPUT 256

static ssize_t mt8189_debug_read_reg(char *buffer, int size, struct mtk_base_afe *afe);

static const struct snd_pcm_hardware mt8189_afe_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP |
		 SNDRV_PCM_INFO_NO_PERIOD_WAKEUP |
		 SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_MMAP_VALID),
	.formats = (SNDRV_PCM_FMTBIT_S16_LE |
		    SNDRV_PCM_FMTBIT_S24_LE |
		    SNDRV_PCM_FMTBIT_S32_LE),
	.period_bytes_min = 96,
	.period_bytes_max = 4 * 48 * 1024,
	.periods_min = 2,
	.periods_max = 256,
	.buffer_bytes_max = 256 * 1024,
	.fifo_size = 0,
};

unsigned int mt8189_rate_transform(struct device *dev, unsigned int rate)
{
	switch (rate) {
	case 8000:
		return MTK_AFE_IPM2P0_RATE_8K;
	case 11025:
		return MTK_AFE_IPM2P0_RATE_11K;
	case 12000:
		return MTK_AFE_IPM2P0_RATE_12K;
	case 16000:
		return MTK_AFE_IPM2P0_RATE_16K;
	case 22050:
		return MTK_AFE_IPM2P0_RATE_22K;
	case 24000:
		return MTK_AFE_IPM2P0_RATE_24K;
	case 32000:
		return MTK_AFE_IPM2P0_RATE_32K;
	case 44100:
		return MTK_AFE_IPM2P0_RATE_44K;
	case 48000:
		return MTK_AFE_IPM2P0_RATE_48K;
	case 88200:
		return MTK_AFE_IPM2P0_RATE_88K;
	case 96000:
		return MTK_AFE_IPM2P0_RATE_96K;
	case 176400:
		return MTK_AFE_IPM2P0_RATE_176K;
	case 192000:
		return MTK_AFE_IPM2P0_RATE_192K;
	/* not support 260K */
	case 352800:
		return MTK_AFE_IPM2P0_RATE_352K;
	case 384000:
		return MTK_AFE_IPM2P0_RATE_384K;
	default:
		dev_info(dev, "rate %u invalid, use %d!!!\n",
			 rate, MTK_AFE_IPM2P0_RATE_48K);
		return MTK_AFE_IPM2P0_RATE_48K;
	}
}

void mt8189_set_cm_rate(struct mtk_base_afe *afe, int id, unsigned int rate)
{
	struct mt8189_afe_private *afe_priv = afe->platform_priv;

	afe_priv->cm_rate[id] = rate;
}

static int mt8189_convert_cm_ch(unsigned int ch)
{
	return ch - 1;
}

static unsigned int calculate_cm_update(int rate, int ch)
{
	unsigned int update_val;

	update_val = 26000000 / rate / (ch / 2);
	update_val = update_val * 10 / 7;
	if (update_val > 100)
		update_val = 100;
	if (update_val < 7)
		update_val = 7;

	return update_val;
}

static int mt8189_set_cm(struct mtk_base_afe *afe, int id,
	       bool update, bool swap, unsigned int ch)
{
	unsigned int rate = 0;
	unsigned int update_val = 0;
	int reg;
	struct mt8189_afe_private *afe_priv = afe->platform_priv;

	dev_info(afe->dev, "%s()-0, CM%d, rate %d, update %d, swap %d, ch %d\n",
		__func__, id, rate, update, swap, ch);

	rate = afe_priv->cm_rate[id];
	update_val = update ? calculate_cm_update(rate, (int)ch) : 0x64;

	reg = AFE_CM0_CON0 + 0x10 * id;
	/* update cnt */
	mtk_regmap_update_bits(afe->regmap, reg, AFE_CM_UPDATE_CNT_MASK,
							update_val, AFE_CM_UPDATE_CNT_SFT);

	/* rate */
	mtk_regmap_update_bits(afe->regmap, reg, AFE_CM_1X_EN_SEL_FS_MASK,
							rate, AFE_CM_1X_EN_SEL_FS_SFT);

	/* ch num */
	ch = mt8189_convert_cm_ch(ch);
	mtk_regmap_update_bits(afe->regmap, reg, AFE_CM_CH_NUM_MASK,
							ch, AFE_CM_CH_NUM_SFT);

	/* swap */
	mtk_regmap_update_bits(afe->regmap, reg, AFE_CM_BYTE_SWAP_MASK,
							swap, AFE_CM_BYTE_SWAP_SFT);

	return 0;
}

static int mt8189_enable_cm_bypass(struct mtk_base_afe *afe, int id, bool en)
{
	int reg = AFE_CM0_CON0 + 0x10 * id;

	mtk_regmap_update_bits(afe->regmap, reg, AFE_CM_BYPASS_MODE_MASK,
							en, AFE_CM_BYPASS_MODE_SFT);

	return 0;
}

static int mt8189_fe_startup(struct snd_pcm_substream *substream,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = snd_soc_substream_to_rtd(substream);
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_dai *cpu_dai = snd_soc_rtd_to_cpu(rtd, 0);
	int memif_num = cpu_dai->id;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];
	const struct snd_pcm_hardware *mtk_afe_hardware = afe->mtk_afe_hardware;
	int ret;

	dev_info(afe->dev, "%s(), memif_num: %d.\n", __func__, memif_num);

	memif->substream = substream;

	snd_pcm_hw_constraint_step(substream->runtime, 0,
				   SNDRV_PCM_HW_PARAM_BUFFER_BYTES, 16);

	snd_soc_set_runtime_hwparams(substream, mtk_afe_hardware);

	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		dev_info(afe->dev, "snd_pcm_hw_constraint_integer failed\n");

	/* dynamic allocate irq to memif */
	if (memif->irq_usage < 0) {
		int irq_id = mtk_dynamic_irq_acquire(afe);

		if (irq_id != afe->irqs_size) {
			/* link */
			memif->irq_usage = irq_id;
		} else {
			dev_info(afe->dev, "%s() error: no more asys irq\n",
				__func__);
			ret = -EBUSY;
		}
	}

	return ret;
}

void mt8189_fe_shutdown(struct snd_pcm_substream *substream,
			struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = snd_soc_substream_to_rtd(substream);
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8189_afe_private *afe_priv = afe->platform_priv;
	struct snd_soc_dai *cpu_dai = snd_soc_rtd_to_cpu(rtd, 0);
	int memif_num = cpu_dai->id;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];
	int irq_id = memif->irq_usage;

	dev_info(afe->dev, "%s(), memif_num: %d.\n", __func__, memif_num);

	memif->substream = NULL;
	afe_priv->irq_cnt[memif_num] = 0;
	afe_priv->xrun_assert[memif_num] = 0;

	if (!memif->const_irq) {
		mtk_dynamic_irq_release(afe, irq_id);
		memif->irq_usage = -1;
		memif->substream = NULL;
	}
}

int mt8189_fe_hw_params(struct snd_pcm_substream *substream,
			 struct snd_pcm_hw_params *params,
			 struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	unsigned int channels = params_channels(params);
	struct mt8189_afe_private *afe_priv = afe->platform_priv;

	afe_priv->cm_channels = channels;

	return mtk_afe_fe_hw_params(substream, params, dai);
}
int mt8189_fe_trigger(struct snd_pcm_substream *substream, int cmd,
		      struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = snd_soc_substream_to_rtd(substream);
	struct snd_pcm_runtime *const runtime = substream->runtime;
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8189_afe_private *afe_priv = afe->platform_priv;
	struct snd_soc_dai *cpu_dai = snd_soc_rtd_to_cpu(rtd, 0);
	int id = cpu_dai->id;
	struct mtk_base_afe_memif *memif = &afe->memif[id];
	int irq_id = memif->irq_usage;
	struct mtk_base_afe_irq *irqs = &afe->irqs[irq_id];
	const struct mtk_base_irq_data *irq_data = irqs->irq_data;
	unsigned int counter = runtime->period_size;
	unsigned int rate = runtime->rate;
	int fs;
	int ret = 0;
	unsigned int tmp_reg = 0;

	dev_info(afe->dev, "%s(), %s cmd %d, irq_id %d\n", __func__,
		memif->data->name, cmd, irq_id);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		dev_info(afe->dev, "%s(), %s cmd %d, id %d\n", __func__,
			memif->data->name, cmd, id);
		ret = mtk_memif_set_enable(afe, id);
		if (ret) {
			dev_err(afe->dev, "%s(), error, id %d, memif enable, ret %d\n",
				__func__, id, ret);
			return ret;
		}

		/*
		 * for small latency record
		 * ul memif need read some data before irq enable
		 */
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			if ((runtime->period_size * 1000) / rate <= 10)
				udelay(300);
		}

		/* set irq counter */
		if (afe_priv->irq_cnt[id] > 0)
			counter = afe_priv->irq_cnt[id];

		mtk_regmap_update_bits(afe->regmap, irq_data->irq_cnt_reg,
				   irq_data->irq_cnt_maskbit,
				   counter, irq_data->irq_cnt_shift);

		/* set irq fs */
		fs = afe->irq_fs(substream, runtime->rate);
		if (fs < 0)
			return -EINVAL;

		if (irq_data->irq_fs_reg >= 0)
			mtk_regmap_update_bits(afe->regmap, irq_data->irq_fs_reg,
				   irq_data->irq_fs_maskbit,
				   fs, irq_data->irq_fs_shift);

		/* enable interrupt */
		mtk_regmap_update_bits(afe->regmap, irq_data->irq_en_reg,
				       1, 1, irq_data->irq_en_shift);

		return 0;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		ret = mtk_memif_set_disable(afe, id);
		if (ret) {
			dev_info(afe->dev,
				"error, id %d, memif enable, ret %d\n",
				id, ret);
		}

		/* disable interrupt */
		mtk_regmap_update_bits(afe->regmap, irq_data->irq_en_reg,
			       1, 0, irq_data->irq_en_shift);

		/* clear pending IRQ */
		regmap_read(afe->regmap, irq_data->irq_clr_reg, &tmp_reg);
		regmap_update_bits(afe->regmap, irq_data->irq_clr_reg,
			   AFE_IRQ_CLR_CFG_MASK_SFT | AFE_IRQ_MISS_FLAG_CLR_CFG_MASK_SFT,
			   tmp_reg^(AFE_IRQ_CLR_CFG_MASK_SFT | AFE_IRQ_MISS_FLAG_CLR_CFG_MASK_SFT));

		return ret;
	default:
		return -EINVAL;
	}
}

static int mt8189_memif_fs(struct snd_pcm_substream *substream,
			   unsigned int rate)
{
	struct snd_soc_pcm_runtime *rtd = snd_soc_substream_to_rtd(substream);
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, AFE_PCM_NAME);
	struct mtk_base_afe *afe = NULL;
	struct snd_soc_dai *cpu_dai = snd_soc_rtd_to_cpu(rtd, 0);
	int id = cpu_dai->id;
	unsigned int rate_reg = 0;
	int cm = 0;

	if (!component)
		return -EINVAL;

	afe = snd_soc_component_get_drvdata(component);

	if (!afe)
		return -EINVAL;

	rate_reg = mt8189_rate_transform(afe->dev, rate);

	switch (id) {
	case MT8189_MEMIF_VUL8:
	case MT8189_MEMIF_VUL_CM0:
		cm = CM0;
		break;
	case MT8189_MEMIF_VUL9:
	case MT8189_MEMIF_VUL_CM1:
		cm = CM1;
		break;
	default:
		cm = CM0;
		break;
	}

	mt8189_set_cm_rate(afe, cm, rate_reg);

	return rate_reg;
}

static int mt8189_get_dai_fs(struct mtk_base_afe *afe,
			     int dai_id, unsigned int rate)
{
	return mt8189_rate_transform(afe->dev, rate);
}

static int mt8189_irq_fs(struct snd_pcm_substream *substream, unsigned int rate)
{
	struct snd_soc_pcm_runtime *rtd = snd_soc_substream_to_rtd(substream);
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, AFE_PCM_NAME);
	struct mtk_base_afe *afe = NULL;

	if (!component)
		return -EINVAL;
	afe = snd_soc_component_get_drvdata(component);
	return mt8189_rate_transform(afe->dev, rate);
}

int mt8189_get_memif_pbuf_size(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	if ((runtime->period_size * 1000) / runtime->rate > 10)
		return MT8189_MEMIF_PBUF_SIZE_256_BYTES;
	else
		return MT8189_MEMIF_PBUF_SIZE_32_BYTES;
}

/* FE DAIs */
static const struct snd_soc_dai_ops mt8189_memif_dai_ops = {
	.startup        = mt8189_fe_startup,
	.shutdown       = mt8189_fe_shutdown,
	.hw_params      = mt8189_fe_hw_params,
	.hw_free        = mtk_afe_fe_hw_free,
	.prepare        = mtk_afe_fe_prepare,
	.trigger        = mt8189_fe_trigger,
};

#define MTK_PCM_RATES (SNDRV_PCM_RATE_8000_48000 |\
		       SNDRV_PCM_RATE_88200 |\
		       SNDRV_PCM_RATE_96000 |\
		       SNDRV_PCM_RATE_176400 |\
		       SNDRV_PCM_RATE_192000)

#define MTK_PCM_DAI_RATES (SNDRV_PCM_RATE_8000 |\
			   SNDRV_PCM_RATE_16000 |\
			   SNDRV_PCM_RATE_32000 |\
			   SNDRV_PCM_RATE_48000)

#define MTK_PCM_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
			 SNDRV_PCM_FMTBIT_S24_LE |\
			 SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver mt8189_memif_dai_driver[] = {
	/* FE DAIs: memory intefaces to CPU */
	{
		.name = "DL0",
		.id = MT8189_MEMIF_DL0,
		.playback = {
			.stream_name = "DL0",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8189_memif_dai_ops,
	},
	{
		.name = "DL1",
		.id = MT8189_MEMIF_DL1,
		.playback = {
			.stream_name = "DL1",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8189_memif_dai_ops,
	},
	{
		.name = "DL2",
		.id = MT8189_MEMIF_DL2,
		.playback = {
			.stream_name = "DL2",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8189_memif_dai_ops,
	},
	{
		.name = "DL3",
		.id = MT8189_MEMIF_DL3,
		.playback = {
			.stream_name = "DL3",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8189_memif_dai_ops,
	},
	{
		.name = "DL4",
		.id = MT8189_MEMIF_DL4,
		.playback = {
			.stream_name = "DL4",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8189_memif_dai_ops,
	},
	{
		.name = "DL5",
		.id = MT8189_MEMIF_DL5,
		.playback = {
			.stream_name = "DL5",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8189_memif_dai_ops,
	},
	{
		.name = "DL6",
		.id = MT8189_MEMIF_DL6,
		.playback = {
			.stream_name = "DL6",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8189_memif_dai_ops,
	},
	{
		.name = "DL7",
		.id = MT8189_MEMIF_DL7,
		.playback = {
			.stream_name = "DL7",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8189_memif_dai_ops,
	},
	{
		.name = "DL8",
		.id = MT8189_MEMIF_DL8,
		.playback = {
			.stream_name = "DL8",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8189_memif_dai_ops,
	},
	{
		.name = "DL23",
		.id = MT8189_MEMIF_DL23,
		.playback = {
			.stream_name = "DL23",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8189_memif_dai_ops,
	},
	{
		.name = "DL24",
		.id = MT8189_MEMIF_DL24,
		.playback = {
			.stream_name = "DL24",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8189_memif_dai_ops,
	},
	{
		.name = "DL25",
		.id = MT8189_MEMIF_DL25,
		.playback = {
			.stream_name = "DL25",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8189_memif_dai_ops,
	},
	{
		.name = "DL_24CH",
		.id = MT8189_MEMIF_DL_24CH,
		.playback = {
			.stream_name = "DL_24CH",
			.channels_min = 1,
			.channels_max = 8,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8189_memif_dai_ops,
	},
	{
		.name = "UL0",
		.id = MT8189_MEMIF_VUL0,
		.capture = {
			.stream_name = "UL0",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8189_memif_dai_ops,
	},
	{
		.name = "UL1",
		.id = MT8189_MEMIF_VUL1,
		.capture = {
			.stream_name = "UL1",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8189_memif_dai_ops,
	},
	{
		.name = "UL2",
		.id = MT8189_MEMIF_VUL2,
		.capture = {
			.stream_name = "UL2",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8189_memif_dai_ops,
	},
	{
		.name = "UL3",
		.id = MT8189_MEMIF_VUL3,
		.capture = {
			.stream_name = "UL3",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8189_memif_dai_ops,
	},
	{
		.name = "UL4",
		.id = MT8189_MEMIF_VUL4,
		.capture = {
			.stream_name = "UL4",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8189_memif_dai_ops,
	},
	{
		.name = "UL5",
		.id = MT8189_MEMIF_VUL5,
		.capture = {
			.stream_name = "UL5",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8189_memif_dai_ops,
	},
	{
		.name = "UL6",
		.id = MT8189_MEMIF_VUL6,
		.capture = {
			.stream_name = "UL6",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8189_memif_dai_ops,
	},
	{
		.name = "UL7",
		.id = MT8189_MEMIF_VUL7,
		.capture = {
			.stream_name = "UL7",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8189_memif_dai_ops,
	},
	{
		.name = "UL8",
		.id = MT8189_MEMIF_VUL8,
		.capture = {
			.stream_name = "UL8",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8189_memif_dai_ops,
	},
	{
		.name = "UL9",
		.id = MT8189_MEMIF_VUL9,
		.capture = {
			.stream_name = "UL9",
			.channels_min = 1,
			.channels_max = 16,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8189_memif_dai_ops,
	},
	{
		.name = "UL10",
		.id = MT8189_MEMIF_VUL10,
		.capture = {
			.stream_name = "UL10",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8189_memif_dai_ops,
	},
	{
		.name = "UL24",
		.id = MT8189_MEMIF_VUL24,
		.capture = {
			.stream_name = "UL24",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8189_memif_dai_ops,
	},
	{
		.name = "UL25",
		.id = MT8189_MEMIF_VUL25,
		.capture = {
			.stream_name = "UL25",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8189_memif_dai_ops,
	},
	{
		.name = "UL_CM0",
		.id = MT8189_MEMIF_VUL_CM0,
		.capture = {
			.stream_name = "UL_CM0",
			.channels_min = 1,
			.channels_max = 8,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8189_memif_dai_ops,
	},
	{
		.name = "UL_CM1",
		.id = MT8189_MEMIF_VUL_CM1,
		.capture = {
			.stream_name = "UL_CM1",
			.channels_min = 1,
			.channels_max = 16,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8189_memif_dai_ops,
	},
	{
		.name = "UL_ETDM_IN0",
		.id = MT8189_MEMIF_ETDM_IN0,
		.capture = {
			.stream_name = "UL_ETDM_IN0",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8189_memif_dai_ops,
	},
	{
		.name = "UL_ETDM_IN1",
		.id = MT8189_MEMIF_ETDM_IN1,
		.capture = {
			.stream_name = "UL_ETDM_IN1",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8189_memif_dai_ops,
	},
	{
		.name = "HDMI",
		.id = MT8189_MEMIF_HDMI,
		.playback = {
			.stream_name = "HDMI",
			.channels_min = 2,
			.channels_max = 8,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8189_memif_dai_ops,
	},
};

static const struct snd_kcontrol_new mt8189_pcm_kcontrols[] = {
};

enum {
	CM0_MUX_VUL8_2CH,
	CM0_MUX_VUL8_8CH,
	CM0_MUX_MASK,
};
enum {
	CM1_MUX_VUL9_2CH,
	CM1_MUX_VUL9_16CH,
	CM1_MUX_MASK,
};

static int ul_cm0_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol,
			int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8189_afe_private *afe_priv = afe->platform_priv;
	unsigned int channels = afe_priv->cm_channels;

	dev_info(afe->dev, "%s(), event 0x%x, name %s, channels %d\n",
		 __func__, event, w->name, channels);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		mt8189_enable_cm_bypass(afe, CM0, 0x0);
		mt8189_set_cm(afe, CM0, true, false, channels);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		mt8189_enable_cm_bypass(afe, CM0, 0x1);
		break;
	default:
		break;
	}
	return 0;
}

static int ul_cm1_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol,
			int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8189_afe_private *afe_priv = afe->platform_priv;
	unsigned int channels = afe_priv->cm_channels;

	dev_info(afe->dev, "%s(), event 0x%x, name %s, channels %d\n",
		 __func__, event, w->name, channels);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		mt8189_enable_cm_bypass(afe, CM1, 0x0);
		mt8189_set_cm(afe, CM1, true, false, channels);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		mt8189_enable_cm_bypass(afe, CM1, 0x1);
		break;
	default:
		break;
	}
	return 0;
}

/* dma widget & routes*/
static const struct snd_kcontrol_new memif_ul0_ch1_mix[] = {
	/* Normal record */
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN018_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN018_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN018_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN018_0,
					I_ADDA_UL_CH4, 1, 0),
	/* AP DMIC */
	SOC_DAPM_SINGLE_AUTODISABLE("AP_DMIC_UL_CH1", AFE_CONN018_0,
				    I_DMIC0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH1", AFE_CONN018_1,
				    I_DL0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1", AFE_CONN018_1,
				    I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN018_1,
				    I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH1", AFE_CONN018_1,
				    I_DL3_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH1", AFE_CONN018_1,
				    I_DL4_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH1", AFE_CONN018_1,
				    I_DL6_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL7_CH1", AFE_CONN018_1,
				    I_DL7_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL23_CH1", AFE_CONN018_2,
				    I_DL23_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH1", AFE_CONN018_1,
				    I_DL_24CH_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN018_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN0_CH1", AFE_CONN018_4,
				    I_I2SIN0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN1_CH1", AFE_CONN018_4,
				    I_I2SIN1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_0_OUT_CH1", AFE_CONN018_6,
				    I_SRC_0_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_2_OUT_CH1", AFE_CONN018_6,
				    I_SRC_2_OUT_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul0_ch2_mix[] = {
	/* Normal record */
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN019_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN019_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN019_0,
					I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN019_0,
					I_ADDA_UL_CH4, 1, 0),
	/* AP DMIC */
	SOC_DAPM_SINGLE_AUTODISABLE("AP_DMIC_UL_CH2", AFE_CONN019_0,
				    I_DMIC0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH2", AFE_CONN019_1,
				    I_DL0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH2", AFE_CONN019_1,
				    I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN019_1,
				    I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH2", AFE_CONN019_1,
				    I_DL3_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH2", AFE_CONN019_1,
				    I_DL4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH2", AFE_CONN019_1,
				    I_DL6_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL7_CH2", AFE_CONN019_1,
				    I_DL7_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL23_CH2", AFE_CONN018_2,
				    I_DL23_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH2", AFE_CONN019_1,
				    I_DL_24CH_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN019_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH2", AFE_CONN019_4,
				    I_PCM_0_CAP_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN0_CH2", AFE_CONN019_4,
				    I_I2SIN0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN1_CH2", AFE_CONN019_4,
				    I_I2SIN1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_0_OUT_CH2", AFE_CONN019_6,
				    I_SRC_0_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_2_OUT_CH2", AFE_CONN019_6,
				    I_SRC_2_OUT_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul1_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN020_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH1", AFE_CONN020_1,
				    I_DL0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1", AFE_CONN020_1,
				    I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN020_1,
				    I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH1", AFE_CONN020_1,
				    I_DL3_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH1", AFE_CONN020_1,
				    I_DL4_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH1", AFE_CONN020_1,
				    I_DL6_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL7_CH1", AFE_CONN020_1,
				    I_DL7_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL23_CH1", AFE_CONN020_2,
				    I_DL23_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH1", AFE_CONN020_1,
				    I_DL_24CH_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN020_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN0_CH1", AFE_CONN020_4,
					I_I2SIN0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN1_CH1", AFE_CONN020_4,
				    I_I2SIN1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_0_OUT_CH1", AFE_CONN020_6,
				    I_SRC_0_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_2_OUT_CH1", AFE_CONN020_6,
				    I_SRC_2_OUT_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul1_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN021_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH2", AFE_CONN021_1,
				    I_DL0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH2", AFE_CONN021_1,
				    I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN021_1,
				    I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH2", AFE_CONN021_1,
				    I_DL3_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH2", AFE_CONN021_1,
				    I_DL4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH2", AFE_CONN021_1,
				    I_DL6_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL7_CH2", AFE_CONN021_1,
				    I_DL7_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL23_CH2", AFE_CONN021_2,
				    I_DL23_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH2", AFE_CONN021_1,
				    I_DL_24CH_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN021_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH2", AFE_CONN021_4,
				    I_PCM_0_CAP_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN0_CH2", AFE_CONN021_4,
						I_I2SIN0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN1_CH2", AFE_CONN021_4,
				    I_I2SIN1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_0_OUT_CH2", AFE_CONN021_6,
				    I_SRC_0_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_2_OUT_CH2", AFE_CONN021_6,
				    I_SRC_2_OUT_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul2_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN022_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN022_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN022_0,
					I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN022_0,
					I_ADDA_UL_CH4, 1, 0),
	/* AP DMIC */
	SOC_DAPM_SINGLE_AUTODISABLE("AP_DMIC_UL_CH3", AFE_CONN022_0,
					I_DMIC1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_GAIN1_OUT_CH1", AFE_CONN022_0,
				    I_GAIN1_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_1_OUT_CH1", AFE_CONN022_6,
				    I_SRC_1_OUT_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul2_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN023_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN023_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN023_0,
					I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN023_0,
					I_ADDA_UL_CH4, 1, 0),
	/* AP DMIC */
	SOC_DAPM_SINGLE_AUTODISABLE("AP_DMIC_UL_CH4", AFE_CONN023_0,
					I_DMIC1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_GAIN1_OUT_CH2", AFE_CONN023_0,
				    I_GAIN1_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_1_OUT_CH2", AFE_CONN023_6,
				    I_SRC_1_OUT_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul3_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN024_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN1_CH1", AFE_CONN024_4,
				    I_I2SIN1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_3_OUT_CH1", AFE_CONN024_6,
				    I_SRC_3_OUT_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul3_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN025_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN1_CH2", AFE_CONN025_4,
				    I_I2SIN1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_3_OUT_CH2", AFE_CONN025_6,
				    I_SRC_3_OUT_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul4_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN026_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH1", AFE_CONN026_1,
				    I_DL0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1", AFE_CONN026_1,
				    I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH1", AFE_CONN026_1,
				    I_DL6_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN026_1,
				    I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH1", AFE_CONN026_1,
				    I_DL3_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH1", AFE_CONN026_1,
				    I_DL_24CH_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN026_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_GAIN0_OUT_CH1", AFE_CONN026_0,
				    I_GAIN0_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_3_OUT_CH1", AFE_CONN026_6,
				    I_SRC_3_OUT_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul4_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN027_0,
					I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH2", AFE_CONN027_1,
				    I_DL0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH2", AFE_CONN027_1,
				    I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH2", AFE_CONN027_1,
				    I_DL6_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN027_1,
				    I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH2", AFE_CONN027_1,
				    I_DL3_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH2", AFE_CONN027_1,
				    I_DL_24CH_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN027_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH2", AFE_CONN027_4,
				    I_PCM_0_CAP_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_GAIN0_OUT_CH2", AFE_CONN027_0,
				    I_GAIN0_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_3_OUT_CH2", AFE_CONN027_6,
				    I_SRC_3_OUT_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul5_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN028_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH1", AFE_CONN028_1,
				    I_DL0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1", AFE_CONN028_1,
				    I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH1", AFE_CONN028_1,
				    I_DL6_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN028_1,
				    I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH1", AFE_CONN028_1,
				    I_DL3_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH1", AFE_CONN028_1,
				    I_DL_24CH_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("GAIN0_OUT_CH1", AFE_CONN028_0,
				    I_GAIN0_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_3_OUT_CH1", AFE_CONN028_6,
				    I_SRC_3_OUT_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul5_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN029_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH2", AFE_CONN029_1,
				    I_DL0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH2", AFE_CONN029_1,
				    I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH2", AFE_CONN029_1,
				    I_DL6_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN029_1,
				    I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH2", AFE_CONN029_1,
				    I_DL3_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH2", AFE_CONN029_1,
				    I_DL_24CH_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN029_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH2", AFE_CONN029_4,
				    I_PCM_0_CAP_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("GAIN0_OUT_CH2", AFE_CONN029_0,
				    I_GAIN0_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_3_OUT_CH2", AFE_CONN029_6,
				    I_SRC_3_OUT_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul6_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN030_0,
					I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("AP_DMIC_UL_CH1", AFE_CONN030_0,
				    I_DMIC0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH1", AFE_CONN030_1,
				    I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN030_1,
				    I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN0_CH1", AFE_CONN030_4,
				    I_I2SIN0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_4_OUT_CH1", AFE_CONN030_6,
				    I_SRC_4_OUT_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul6_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN031_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("AP_DMIC_UL_CH2", AFE_CONN031_0,
				    I_DMIC0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH2", AFE_CONN031_1,
				    I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN031_1,
				    I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN0_CH2", AFE_CONN031_4,
				    I_I2SIN0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_4_OUT_CH2", AFE_CONN031_6,
				    I_SRC_4_OUT_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul7_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN032_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN032_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("AP_DMIC_UL_CH3", AFE_CONN032_0,
				    I_DMIC1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH1", AFE_CONN032_1,
				    I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN032_1,
				    I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN0_CH1", AFE_CONN032_4,
				    I_I2SIN0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_4_OUT_CH1", AFE_CONN032_6,
				    I_SRC_4_OUT_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul7_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN033_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN033_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("AP_DMIC_UL_CH4", AFE_CONN033_0,
				    I_DMIC1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH2", AFE_CONN033_1,
				    I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN033_1,
				    I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN0_CH2", AFE_CONN033_4,
				    I_I2SIN0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_4_OUT_CH2", AFE_CONN033_6,
				    I_SRC_4_OUT_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul8_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN034_0,
				    I_ADDA_UL_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul8_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN035_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN035_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH2", AFE_CONN035_4,
				    I_PCM_0_CAP_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul9_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN036_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN036_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN036_0,
				    I_ADDA_UL_CH3, 1, 0),
};

static const struct snd_kcontrol_new memif_ul9_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN037_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN037_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN037_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN037_0,
				    I_ADDA_UL_CH4, 1, 0),
};

static const struct snd_kcontrol_new memif_ul24_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN066_0,
				    I_ADDA_UL_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul24_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN067_0,
				    I_ADDA_UL_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul_cm0_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN040_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN040_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN040_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN040_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("AP_DMIC_UL_CH1", AFE_CONN040_0,
				    I_DMIC0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_GAIN1_OUT_CH1", AFE_CONN040_0,
				    I_GAIN1_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_0_OUT_CH1", AFE_CONN040_6,
				    I_SRC_0_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_1_OUT_CH1", AFE_CONN040_6,
				    I_SRC_1_OUT_CH1, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm0_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN041_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN041_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN041_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN041_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("AP_DMIC_UL_CH2", AFE_CONN041_0,
				    I_DMIC0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_GAIN1_OUT_CH2", AFE_CONN041_0,
				    I_GAIN1_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_0_OUT_CH2", AFE_CONN041_6,
				    I_SRC_0_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_1_OUT_CH2", AFE_CONN041_6,
				    I_SRC_1_OUT_CH2, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm0_ch3_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN042_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN042_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN042_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN042_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("AP_DMIC_UL_CH3", AFE_CONN042_0,
				    I_DMIC1_CH1, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm0_ch4_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN043_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN043_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN043_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN043_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("AP_DMIC_UL_CH4", AFE_CONN043_0,
				    I_DMIC1_CH2, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm0_ch5_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN044_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN044_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN044_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN044_0,
				    I_ADDA_UL_CH4, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm0_ch6_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN045_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN045_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN045_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN045_0,
				    I_ADDA_UL_CH4, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm0_ch7_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN046_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN046_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN046_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN046_0,
				    I_ADDA_UL_CH4, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm0_ch8_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN047_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN047_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN047_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN047_0,
				    I_ADDA_UL_CH4, 1, 0),
};

static const struct snd_kcontrol_new memif_ul_cm1_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN048_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN048_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN048_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN048_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH5", AFE_CONN048_0,
				    I_ADDA_UL_CH5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH6", AFE_CONN048_0,
				    I_ADDA_UL_CH6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_0_OUT_CH1", AFE_CONN048_6,
				    I_SRC_0_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_3_OUT_CH1", AFE_CONN048_6,
				    I_SRC_3_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_4_OUT_CH1", AFE_CONN048_6,
				    I_SRC_4_OUT_CH1, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN049_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN049_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN049_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN049_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH5", AFE_CONN049_0,
				    I_ADDA_UL_CH5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH6", AFE_CONN049_0,
				    I_ADDA_UL_CH6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_0_OUT_CH2", AFE_CONN049_6,
				    I_SRC_0_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_3_OUT_CH2", AFE_CONN049_6,
				    I_SRC_3_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_4_OUT_CH2", AFE_CONN049_6,
				    I_SRC_4_OUT_CH2, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch3_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN050_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN050_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN050_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN050_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH5", AFE_CONN050_0,
				    I_ADDA_UL_CH5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH6", AFE_CONN050_0,
				    I_ADDA_UL_CH6, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch4_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN051_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN051_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN051_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN051_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH5", AFE_CONN051_0,
				    I_ADDA_UL_CH5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH6", AFE_CONN051_0,
				    I_ADDA_UL_CH6, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch5_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN052_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN052_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN052_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN052_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH5", AFE_CONN052_0,
				    I_ADDA_UL_CH5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH6", AFE_CONN052_0,
				    I_ADDA_UL_CH6, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch6_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN053_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN053_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN053_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN053_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH5", AFE_CONN053_0,
				    I_ADDA_UL_CH5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH6", AFE_CONN053_0,
				    I_ADDA_UL_CH6, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch7_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN054_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN054_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN054_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN054_0,
				    I_ADDA_UL_CH4, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch8_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN055_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN055_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN055_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN055_0,
				    I_ADDA_UL_CH4, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch9_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN056_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN056_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN056_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN056_0,
				    I_ADDA_UL_CH4, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch10_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN057_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN057_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN057_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN057_0,
				    I_ADDA_UL_CH4, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch11_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN058_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN058_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN058_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN058_0,
				    I_ADDA_UL_CH4, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch12_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN059_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN059_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN059_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN059_0,
				    I_ADDA_UL_CH4, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch13_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN060_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN060_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN060_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN060_0,
				    I_ADDA_UL_CH4, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch14_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN061_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN061_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN061_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN061_0,
				    I_ADDA_UL_CH4, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch15_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN062_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN062_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN062_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN062_0,
				    I_ADDA_UL_CH4, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch16_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN063_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN063_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN063_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN063_0,
				    I_ADDA_UL_CH4, 1, 0),
};

static const char * const cm0_mux_map[] = {
	"CM0_8CH_PATH",
	"CM0_2CH_PATH",
};
static const char * const cm1_mux_map[] = {
	"CM1_16CH_PATH",
	"CM1_2CH_PATH",
};

static int cm0_mux_map_value[] = {
	CM0_MUX_VUL8_8CH,
	CM0_MUX_VUL8_2CH,
};

static int cm1_mux_map_value[] = {
	CM1_MUX_VUL9_16CH,
	CM1_MUX_VUL9_2CH,
};

static SOC_VALUE_ENUM_SINGLE_DECL(ul_cm0_mux_map_enum,
				  AFE_CM0_CON0,
				  AFE_CM0_OUTPUT_MUX_SFT,
				  AFE_CM0_OUTPUT_MUX_MASK,
				  cm0_mux_map,
				  cm0_mux_map_value);
static SOC_VALUE_ENUM_SINGLE_DECL(ul_cm1_mux_map_enum,
				  AFE_CM1_CON0,
				  AFE_CM1_OUTPUT_MUX_SFT,
				  AFE_CM1_OUTPUT_MUX_MASK,
				  cm1_mux_map,
				  cm1_mux_map_value);

static const struct snd_kcontrol_new ul_cm0_mux_control =
	SOC_DAPM_ENUM("CM0_UL_MUX Select", ul_cm0_mux_map_enum);
static const struct snd_kcontrol_new ul_cm1_mux_control =
	SOC_DAPM_ENUM("CM1_UL_MUX Select", ul_cm1_mux_map_enum);

static const struct snd_soc_dapm_widget mt8189_memif_widgets[] = {
	/* inter-connections */
	SND_SOC_DAPM_MIXER("UL0_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul0_ch1_mix, ARRAY_SIZE(memif_ul0_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL0_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul0_ch2_mix, ARRAY_SIZE(memif_ul0_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL1_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul1_ch1_mix, ARRAY_SIZE(memif_ul1_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL1_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul1_ch2_mix, ARRAY_SIZE(memif_ul1_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL2_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul2_ch1_mix, ARRAY_SIZE(memif_ul2_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL2_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul2_ch2_mix, ARRAY_SIZE(memif_ul2_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL3_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul3_ch1_mix, ARRAY_SIZE(memif_ul3_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL3_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul3_ch2_mix, ARRAY_SIZE(memif_ul3_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL4_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul4_ch1_mix, ARRAY_SIZE(memif_ul4_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL4_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul4_ch2_mix, ARRAY_SIZE(memif_ul4_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL5_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul5_ch1_mix, ARRAY_SIZE(memif_ul5_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL5_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul5_ch2_mix, ARRAY_SIZE(memif_ul5_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL6_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul6_ch1_mix, ARRAY_SIZE(memif_ul6_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL6_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul6_ch2_mix, ARRAY_SIZE(memif_ul6_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL7_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul7_ch1_mix, ARRAY_SIZE(memif_ul7_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL7_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul7_ch2_mix, ARRAY_SIZE(memif_ul7_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL8_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul8_ch1_mix, ARRAY_SIZE(memif_ul8_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL8_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul8_ch2_mix, ARRAY_SIZE(memif_ul8_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL9_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul9_ch1_mix, ARRAY_SIZE(memif_ul9_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL9_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul9_ch2_mix, ARRAY_SIZE(memif_ul9_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL24_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul24_ch1_mix, ARRAY_SIZE(memif_ul24_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL24_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul24_ch2_mix, ARRAY_SIZE(memif_ul24_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL_CM0_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm0_ch1_mix, ARRAY_SIZE(memif_ul_cm0_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL_CM0_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm0_ch2_mix, ARRAY_SIZE(memif_ul_cm0_ch2_mix)),
	SND_SOC_DAPM_MIXER("UL_CM0_CH3", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm0_ch3_mix, ARRAY_SIZE(memif_ul_cm0_ch3_mix)),
	SND_SOC_DAPM_MIXER("UL_CM0_CH4", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm0_ch4_mix, ARRAY_SIZE(memif_ul_cm0_ch4_mix)),
	SND_SOC_DAPM_MIXER("UL_CM0_CH5", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm0_ch5_mix, ARRAY_SIZE(memif_ul_cm0_ch5_mix)),
	SND_SOC_DAPM_MIXER("UL_CM0_CH6", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm0_ch6_mix, ARRAY_SIZE(memif_ul_cm0_ch6_mix)),
	SND_SOC_DAPM_MIXER("UL_CM0_CH7", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm0_ch7_mix, ARRAY_SIZE(memif_ul_cm0_ch7_mix)),
	SND_SOC_DAPM_MIXER("UL_CM0_CH8", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm0_ch8_mix, ARRAY_SIZE(memif_ul_cm0_ch8_mix)),
	SND_SOC_DAPM_MUX_E("CM0_UL_MUX", SND_SOC_NOPM, 0, 0,
			   &ul_cm0_mux_control,
			   ul_cm0_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_MIXER("UL_CM1_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch1_mix, ARRAY_SIZE(memif_ul_cm1_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch2_mix, ARRAY_SIZE(memif_ul_cm1_ch2_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH3", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch3_mix, ARRAY_SIZE(memif_ul_cm1_ch3_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH4", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch4_mix, ARRAY_SIZE(memif_ul_cm1_ch4_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH5", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch5_mix, ARRAY_SIZE(memif_ul_cm1_ch5_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH6", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch6_mix, ARRAY_SIZE(memif_ul_cm1_ch6_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH7", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch7_mix, ARRAY_SIZE(memif_ul_cm1_ch7_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH8", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch8_mix, ARRAY_SIZE(memif_ul_cm1_ch8_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH9", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch9_mix, ARRAY_SIZE(memif_ul_cm1_ch9_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH10", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch10_mix, ARRAY_SIZE(memif_ul_cm1_ch10_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH11", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch11_mix, ARRAY_SIZE(memif_ul_cm1_ch11_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH12", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch12_mix, ARRAY_SIZE(memif_ul_cm1_ch12_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH13", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch13_mix, ARRAY_SIZE(memif_ul_cm1_ch13_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH14", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch14_mix, ARRAY_SIZE(memif_ul_cm1_ch14_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH15", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch15_mix, ARRAY_SIZE(memif_ul_cm1_ch15_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH16", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch16_mix, ARRAY_SIZE(memif_ul_cm1_ch16_mix)),
	SND_SOC_DAPM_MUX("CM1_UL_MUX", SND_SOC_NOPM, 0, 0,
			   &ul_cm1_mux_control),
	SND_SOC_DAPM_SUPPLY("CM0_Enable",
			AFE_CM0_CON0, AFE_CM0_ON_SFT, 0,
			ul_cm0_event,
			SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_SUPPLY("CM1_Enable",
			AFE_CM1_CON0, AFE_CM0_ON_SFT, 0,
			ul_cm1_event,
			SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_INPUT("UL2_VIRTUAL_INPUT"),

	/* dynamic pinctrl */
	SND_SOC_DAPM_PINCTRL("ETDMIN_SPK_PIN", "aud-gpio-i2sin1-on", "aud-gpio-i2sin1-off"),
	SND_SOC_DAPM_PINCTRL("ETDMOUT_SPK_PIN", "aud-gpio-i2sout1-on", "aud-gpio-i2sout1-off"),
	SND_SOC_DAPM_PINCTRL("ETDMIN_HP_PIN", "aud-gpio-i2sin0-on", "aud-gpio-i2sin0-off"),
	SND_SOC_DAPM_PINCTRL("ETDMOUT_HP_PIN", "aud-gpio-i2sout0-on", "aud-gpio-i2sout0-off"),
	SND_SOC_DAPM_PINCTRL("ETDMOUT_HDMI_PIN", "aud-gpio-pcm-on", "aud-gpio-pcm-off"),
	SND_SOC_DAPM_PINCTRL("AP_DMIC0_PIN", "aud-gpio-ap-dmic-on", "aud-gpio-ap-dmic-off"),
	SND_SOC_DAPM_PINCTRL("AP_DMIC1_PIN", "aud-gpio-ap-dmic1-on", "aud-gpio-ap-dmic1-off"),
};

static const struct snd_soc_dapm_route mt8189_memif_routes[] = {
	{"UL0", NULL, "UL0_CH1"},
	{"UL0", NULL, "UL0_CH2"},
	/* Normal record */
	{"UL0_CH1", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL0_CH1", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL0_CH1", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL0_CH1", "ADDA_UL_CH4", "ADDA_CH34_UL_Mux"},
	{"UL0_CH2", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL0_CH2", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL0_CH2", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL0_CH2", "ADDA_UL_CH4", "ADDA_CH34_UL_Mux"},

	{"UL0_CH1", "AP_DMIC_UL_CH1", "AP DMIC Capture"},
	{"UL0_CH2", "AP_DMIC_UL_CH2", "AP DMIC Capture"},

	{"UL0_CH1", "I2SIN0_CH1", "I2SIN0"},
	{"UL0_CH2", "I2SIN0_CH2", "I2SIN0"},
	{"UL0_CH1", "I2SIN1_CH1", "I2SIN1"},
	{"UL0_CH2", "I2SIN1_CH2", "I2SIN1"},

	{"UL0_CH1", "PCM_0_CAP_CH1", "PCM 0 Capture"},
	{"UL0_CH2", "PCM_0_CAP_CH1", "PCM 0 Capture"},

	{"UL1", NULL, "UL1_CH1"},
	{"UL1", NULL, "UL1_CH2"},

	{"UL1_CH1", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL1_CH2", "ADDA_UL_CH2", "ADDA_UL_Mux"},

	{"UL1_CH1", "I2SIN0_CH1", "I2SIN0"},
	{"UL1_CH2", "I2SIN0_CH2", "I2SIN0"},
	{"UL1_CH1", "I2SIN1_CH1", "I2SIN1"},
	{"UL1_CH2", "I2SIN1_CH2", "I2SIN1"},

	{"UL1_CH1", "PCM_0_CAP_CH1", "PCM 0 Capture"},
	{"UL1_CH2", "PCM_0_CAP_CH1", "PCM 0 Capture"},

	{"UL2", NULL, "UL2_CH1"},
	{"UL2", NULL, "UL2_CH2"},

	{"UL2_CH1", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL2_CH1", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL2_CH1", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL2_CH1", "ADDA_UL_CH4", "ADDA_CH34_UL_Mux"},
	{"UL2_CH2", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL2_CH2", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL2_CH2", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL2_CH2", "ADDA_UL_CH4", "ADDA_CH34_UL_Mux"},

	{"UL2_CH1", "AP_DMIC_UL_CH3", "AP DMIC CH34 Capture"},
	{"UL2_CH2", "AP_DMIC_UL_CH4", "AP DMIC CH34 Capture"},

	{"UL3", NULL, "UL3_CH1"},
	{"UL3", NULL, "UL3_CH2"},

	{"UL3_CH1", "ADDA_UL_CH1", "ADDA Capture"},
	{"UL3_CH2", "ADDA_UL_CH2", "ADDA Capture"},
	{"UL3_CH1", "I2SIN1_CH1", "I2SIN1"},
	{"UL3_CH2", "I2SIN1_CH2", "I2SIN1"},

	{"UL4", NULL, "UL4_CH1"},
	{"UL4", NULL, "UL4_CH2"},
	{"UL4_CH1", "ADDA_UL_CH1", "ADDA Capture"},
	{"UL4_CH2", "ADDA_UL_CH2", "ADDA Capture"},

	{"UL4_CH1", "PCM_0_CAP_CH1", "PCM 0 Capture"},
	{"UL4_CH2", "PCM_0_CAP_CH1", "PCM 0 Capture"},

	{"UL5", NULL, "UL5_CH1"},
	{"UL5", NULL, "UL5_CH2"},

	{"UL5_CH1", "ADDA_UL_CH1", "ADDA Capture"},
	{"UL5_CH2", "ADDA_UL_CH2", "ADDA Capture"},

	{"UL6", NULL, "UL6_CH1"},
	{"UL6", NULL, "UL6_CH2"},
	{"UL6_CH1", "ADDA_UL_CH1", "ADDA Capture"},
	{"UL6_CH2", "ADDA_UL_CH2", "ADDA Capture"},
	{"UL6_CH1", "I2SIN0_CH1", "I2SIN0"},
	{"UL6_CH2", "I2SIN0_CH2", "I2SIN0"},
	{"UL6_CH1", "AP_DMIC_UL_CH1", "AP DMIC Capture"},
	{"UL6_CH2", "AP_DMIC_UL_CH2", "AP DMIC Capture"},

	{"UL7", NULL, "UL7_CH1"},
	{"UL7", NULL, "UL7_CH2"},
	{"UL7_CH1", "ADDA_UL_CH1", "ADDA Capture"},
	{"UL7_CH1", "ADDA_UL_CH2", "ADDA Capture"},
	{"UL7_CH2", "ADDA_UL_CH1", "ADDA Capture"},
	{"UL7_CH2", "ADDA_UL_CH2", "ADDA Capture"},
	{"UL7_CH1", "I2SIN0_CH1", "I2SIN0"},
	{"UL7_CH2", "I2SIN0_CH2", "I2SIN0"},
	{"UL7_CH1", "AP_DMIC_UL_CH3", "AP DMIC CH34 Capture"},
	{"UL7_CH2", "AP_DMIC_UL_CH4", "AP DMIC CH34 Capture"},

	{"UL8", NULL, "CM0_UL_MUX"},
	{"CM0_UL_MUX", "CM0_2CH_PATH", "UL8_CH1"},
	{"CM0_UL_MUX", "CM0_2CH_PATH", "UL8_CH2"},
	{"CM0_UL_MUX", "CM0_8CH_PATH", "UL_CM0_CH1"},
	{"CM0_UL_MUX", "CM0_8CH_PATH", "UL_CM0_CH2"},
	{"CM0_UL_MUX", "CM0_8CH_PATH", "UL_CM0_CH3"},
	{"CM0_UL_MUX", "CM0_8CH_PATH", "UL_CM0_CH4"},
	{"CM0_UL_MUX", "CM0_8CH_PATH", "UL_CM0_CH5"},
	{"CM0_UL_MUX", "CM0_8CH_PATH", "UL_CM0_CH6"},
	{"CM0_UL_MUX", "CM0_8CH_PATH", "UL_CM0_CH7"},
	{"CM0_UL_MUX", "CM0_8CH_PATH", "UL_CM0_CH8"},
	{"UL_CM0_CH1", NULL, "CM0_Enable"},
	{"UL_CM0_CH2", NULL, "CM0_Enable"},
	{"UL_CM0_CH3", NULL, "CM0_Enable"},
	{"UL_CM0_CH4", NULL, "CM0_Enable"},
	{"UL_CM0_CH5", NULL, "CM0_Enable"},
	{"UL_CM0_CH6", NULL, "CM0_Enable"},
	{"UL_CM0_CH7", NULL, "CM0_Enable"},
	{"UL_CM0_CH8", NULL, "CM0_Enable"},

	{"UL9", NULL, "CM1_UL_MUX"},
	{"CM1_UL_MUX", "CM1_2CH_PATH", "UL9_CH1"},
	{"CM1_UL_MUX", "CM1_2CH_PATH", "UL9_CH2"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH1"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH2"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH3"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH4"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH5"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH6"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH7"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH8"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH9"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH10"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH11"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH12"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH13"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH14"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH15"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH16"},

	{"UL_CM1_CH1", NULL, "CM1_Enable"},
	{"UL_CM1_CH2", NULL, "CM1_Enable"},
	{"UL_CM1_CH3", NULL, "CM1_Enable"},
	{"UL_CM1_CH4", NULL, "CM1_Enable"},
	{"UL_CM1_CH5", NULL, "CM1_Enable"},
	{"UL_CM1_CH6", NULL, "CM1_Enable"},
	{"UL_CM1_CH7", NULL, "CM1_Enable"},
	{"UL_CM1_CH8", NULL, "CM1_Enable"},
	{"UL_CM1_CH9", NULL, "CM1_Enable"},
	{"UL_CM1_CH10", NULL, "CM1_Enable"},
	{"UL_CM1_CH11", NULL, "CM1_Enable"},
	{"UL_CM1_CH12", NULL, "CM1_Enable"},
	{"UL_CM1_CH13", NULL, "CM1_Enable"},
	{"UL_CM1_CH14", NULL, "CM1_Enable"},
	{"UL_CM1_CH15", NULL, "CM1_Enable"},
	{"UL_CM1_CH16", NULL, "CM1_Enable"},

	/* UL9 o36o37 <- ADDA */
	{"UL9_CH1", "ADDA_UL_CH1", "ADDA Capture"},
	{"UL9_CH1", "ADDA_UL_CH2", "ADDA Capture"},
	{"UL9_CH2", "ADDA_UL_CH1", "ADDA Capture"},
	{"UL9_CH2", "ADDA_UL_CH2", "ADDA Capture"},

	{"UL24", NULL, "UL24_CH1"},
	{"UL24", NULL, "UL24_CH2"},
	{"UL24_CH1", "ADDA_UL_CH1", "ADDA Capture"},

	{"UL_CM0", NULL, "UL_CM0_CH1"},
	{"UL_CM0", NULL, "UL_CM0_CH2"},
	{"UL_CM0", NULL, "UL_CM0_CH3"},
	{"UL_CM0", NULL, "UL_CM0_CH4"},
	{"UL_CM0", NULL, "UL_CM0_CH5"},
	{"UL_CM0", NULL, "UL_CM0_CH6"},
	{"UL_CM0", NULL, "UL_CM0_CH7"},
	{"UL_CM0", NULL, "UL_CM0_CH8"},
	{"UL_CM0_CH1", "ADDA_UL_CH1", "ADDA Capture"},
	{"UL_CM0_CH1", "ADDA_UL_CH2", "ADDA Capture"},
	{"UL_CM0_CH2", "ADDA_UL_CH1", "ADDA Capture"},
	{"UL_CM0_CH2", "ADDA_UL_CH2", "ADDA Capture"},
	{"UL_CM0_CH3", "ADDA_UL_CH1", "ADDA Capture"},
	{"UL_CM0_CH3", "ADDA_UL_CH2", "ADDA Capture"},
	{"UL_CM0_CH4", "ADDA_UL_CH1", "ADDA Capture"},
	{"UL_CM0_CH4", "ADDA_UL_CH2", "ADDA Capture"},
	{"UL_CM0_CH1", "AP_DMIC_UL_CH1", "AP DMIC Capture"},
	{"UL_CM0_CH2", "AP_DMIC_UL_CH2", "AP DMIC Capture"},
	{"UL_CM0_CH3", "AP_DMIC_UL_CH3", "AP DMIC CH34 Capture"},
	{"UL_CM0_CH4", "AP_DMIC_UL_CH4", "AP DMIC CH34 Capture"},

	{"UL_CM1", NULL, "UL_CM1_CH1"},
	{"UL_CM1", NULL, "UL_CM1_CH2"},
	{"UL_CM1", NULL, "UL_CM1_CH3"},
	{"UL_CM1", NULL, "UL_CM1_CH4"},
	{"UL_CM1", NULL, "UL_CM1_CH5"},
	{"UL_CM1", NULL, "UL_CM1_CH6"},
	{"UL_CM1", NULL, "UL_CM1_CH7"},
	{"UL_CM1", NULL, "UL_CM1_CH8"},
	{"UL_CM1", NULL, "UL_CM1_CH9"},
	{"UL_CM1", NULL, "UL_CM1_CH10"},
	{"UL_CM1", NULL, "UL_CM1_CH11"},
	{"UL_CM1", NULL, "UL_CM1_CH12"},
	{"UL_CM1", NULL, "UL_CM1_CH13"},
	{"UL_CM1", NULL, "UL_CM1_CH14"},
	{"UL_CM1", NULL, "UL_CM1_CH15"},
	{"UL_CM1", NULL, "UL_CM1_CH16"},
	{"UL_CM1_CH1", "ADDA_UL_CH1", "ADDA Capture"},
	{"UL_CM1_CH1", "ADDA_UL_CH2", "ADDA Capture"},
	{"UL_CM1_CH2", "ADDA_UL_CH1", "ADDA Capture"},
	{"UL_CM1_CH2", "ADDA_UL_CH2", "ADDA Capture"},
	{"UL_CM1_CH3", "ADDA_UL_CH1", "ADDA Capture"},
	{"UL_CM1_CH3", "ADDA_UL_CH2", "ADDA Capture"},
	{"UL_CM1_CH4", "ADDA_UL_CH1", "ADDA Capture"},
	{"UL_CM1_CH4", "ADDA_UL_CH2", "ADDA Capture"},
	{"UL_CM1_CH5", "ADDA_UL_CH1", "ADDA Capture"},
	{"UL_CM1_CH5", "ADDA_UL_CH2", "ADDA Capture"},
	{"UL_CM1_CH6", "ADDA_UL_CH1", "ADDA Capture"},
	{"UL_CM1_CH6", "ADDA_UL_CH2", "ADDA Capture"},
};

static const struct mtk_base_memif_data memif_data[MT8189_MEMIF_NUM] = {
	[MT8189_MEMIF_DL0] = {
		.name = "DL0",
		.id = MT8189_MEMIF_DL0,
		.reg_ofs_base = AFE_DL0_BASE,
		.reg_ofs_cur = AFE_DL0_CUR,
		.reg_ofs_end = AFE_DL0_END,
		.reg_ofs_base_msb = AFE_DL0_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL0_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL0_END_MSB,
		.fs_reg = AFE_DL0_CON0,
		.fs_shift = DL0_SEL_FS_SFT,
		.fs_maskbit = DL0_SEL_FS_MASK,
		.mono_reg = AFE_DL0_CON0,
		.mono_shift = DL0_MONO_SFT,
		.enable_reg = AFE_DL0_CON0,
		.enable_shift = DL0_ON_SFT,
		.hd_reg = AFE_DL0_CON0,
		.hd_mask = DL0_HD_MODE_MASK,
		.hd_shift = DL0_HD_MODE_SFT,
		.hd_align_reg = AFE_DL0_CON0,
		.hd_align_mshift = DL0_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL0_CON0,
		.pbuf_mask = DL0_PBUF_SIZE_MASK,
		.pbuf_shift = DL0_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL0_CON0,
		.minlen_mask = DL0_MINLEN_MASK,
		.minlen_shift = DL0_MINLEN_SFT,
	},
	[MT8189_MEMIF_DL1] = {
		.name = "DL1",
		.id = MT8189_MEMIF_DL1,
		.reg_ofs_base = AFE_DL1_BASE,
		.reg_ofs_cur = AFE_DL1_CUR,
		.reg_ofs_end = AFE_DL1_END,
		.reg_ofs_base_msb = AFE_DL1_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL1_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL1_END_MSB,
		.fs_reg = AFE_DL1_CON0,
		.fs_shift = DL1_SEL_FS_SFT,
		.fs_maskbit = DL1_SEL_FS_MASK,
		.mono_reg = AFE_DL1_CON0,
		.mono_shift = DL1_MONO_SFT,
		.enable_reg = AFE_DL1_CON0,
		.enable_shift = DL1_ON_SFT,
		.hd_reg = AFE_DL1_CON0,
		.hd_mask = DL1_HD_MODE_MASK,
		.hd_shift = DL1_HD_MODE_SFT,
		.hd_align_reg = AFE_DL1_CON0,
		.hd_align_mshift = DL1_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL1_CON0,
		.pbuf_mask = DL1_PBUF_SIZE_MASK,
		.pbuf_shift = DL1_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL1_CON0,
		.minlen_mask = DL1_MINLEN_MASK,
		.minlen_shift = DL1_MINLEN_SFT,
	},
	[MT8189_MEMIF_DL2] = {
		.name = "DL2",
		.id = MT8189_MEMIF_DL2,
		.reg_ofs_base = AFE_DL2_BASE,
		.reg_ofs_cur = AFE_DL2_CUR,
		.reg_ofs_end = AFE_DL2_END,
		.reg_ofs_base_msb = AFE_DL2_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL2_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL2_END_MSB,
		.fs_reg = AFE_DL2_CON0,
		.fs_shift = DL2_SEL_FS_SFT,
		.fs_maskbit = DL2_SEL_FS_MASK,
		.mono_reg = AFE_DL2_CON0,
		.mono_shift = DL2_MONO_SFT,
		.enable_reg = AFE_DL2_CON0,
		.enable_shift = DL2_ON_SFT,
		.hd_reg = AFE_DL2_CON0,
		.hd_mask = DL2_HD_MODE_MASK,
		.hd_shift = DL2_HD_MODE_SFT,
		.hd_align_reg = AFE_DL2_CON0,
		.hd_align_mshift = DL2_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL2_CON0,
		.pbuf_mask = DL2_PBUF_SIZE_MASK,
		.pbuf_shift = DL2_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL2_CON0,
		.minlen_mask = DL2_MINLEN_MASK,
		.minlen_shift = DL2_MINLEN_SFT,
	},
	[MT8189_MEMIF_DL3] = {
		.name = "DL3",
		.id = MT8189_MEMIF_DL3,
		.reg_ofs_base = AFE_DL3_BASE,
		.reg_ofs_cur = AFE_DL3_CUR,
		.reg_ofs_end = AFE_DL3_END,
		.reg_ofs_base_msb = AFE_DL3_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL3_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL3_END_MSB,
		.fs_reg = AFE_DL3_CON0,
		.fs_shift = DL3_SEL_FS_SFT,
		.fs_maskbit = DL3_SEL_FS_MASK,
		.mono_reg = AFE_DL3_CON0,
		.mono_shift = DL3_MONO_SFT,
		.enable_reg = AFE_DL3_CON0,
		.enable_shift = DL3_ON_SFT,
		.hd_reg = AFE_DL3_CON0,
		.hd_mask = DL3_HD_MODE_MASK,
		.hd_shift = DL3_HD_MODE_SFT,
		.hd_align_reg = AFE_DL3_CON0,
		.hd_align_mshift = DL3_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL3_CON0,
		.pbuf_mask = DL3_PBUF_SIZE_MASK,
		.pbuf_shift = DL3_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL3_CON0,
		.minlen_mask = DL3_MINLEN_MASK,
		.minlen_shift = DL3_MINLEN_SFT,
	},
	[MT8189_MEMIF_DL4] = {
		.name = "DL4",
		.id = MT8189_MEMIF_DL4,
		.reg_ofs_base = AFE_DL4_BASE,
		.reg_ofs_cur = AFE_DL4_CUR,
		.reg_ofs_end = AFE_DL4_END,
		.reg_ofs_base_msb = AFE_DL4_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL4_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL4_END_MSB,
		.fs_reg = AFE_DL4_CON0,
		.fs_shift = DL4_SEL_FS_SFT,
		.fs_maskbit = DL4_SEL_FS_MASK,
		.mono_reg = AFE_DL4_CON0,
		.mono_shift = DL4_MONO_SFT,
		.enable_reg = AFE_DL4_CON0,
		.enable_shift = DL4_ON_SFT,
		.hd_reg = AFE_DL4_CON0,
		.hd_mask = DL4_HD_MODE_MASK,
		.hd_shift = DL4_HD_MODE_SFT,
		.hd_align_reg = AFE_DL4_CON0,
		.hd_align_mshift = DL4_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL4_CON0,
		.pbuf_mask = DL4_PBUF_SIZE_MASK,
		.pbuf_shift = DL4_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL4_CON0,
		.minlen_mask = DL4_MINLEN_MASK,
		.minlen_shift = DL4_MINLEN_SFT,
	},
	[MT8189_MEMIF_DL5] = {
		.name = "DL5",
		.id = MT8189_MEMIF_DL5,
		.reg_ofs_base = AFE_DL5_BASE,
		.reg_ofs_cur = AFE_DL5_CUR,
		.reg_ofs_end = AFE_DL5_END,
		.reg_ofs_base_msb = AFE_DL5_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL5_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL5_END_MSB,
		.fs_reg = AFE_DL5_CON0,
		.fs_shift = DL5_SEL_FS_SFT,
		.fs_maskbit = DL5_SEL_FS_MASK,
		.mono_reg = AFE_DL5_CON0,
		.mono_shift = DL5_MONO_SFT,
		.enable_reg = AFE_DL5_CON0,
		.enable_shift = DL5_ON_SFT,
		.hd_reg = AFE_DL5_CON0,
		.hd_mask = DL5_HD_MODE_MASK,
		.hd_shift = DL5_HD_MODE_SFT,
		.hd_align_reg = AFE_DL5_CON0,
		.hd_align_mshift = DL5_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL5_CON0,
		.pbuf_mask = DL5_PBUF_SIZE_MASK,
		.pbuf_shift = DL5_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL5_CON0,
		.minlen_mask = DL5_MINLEN_MASK,
		.minlen_shift = DL5_MINLEN_SFT,
	},
	[MT8189_MEMIF_DL6] = {
		.name = "DL6",
		.id = MT8189_MEMIF_DL6,
		.reg_ofs_base = AFE_DL6_BASE,
		.reg_ofs_cur = AFE_DL6_CUR,
		.reg_ofs_end = AFE_DL6_END,
		.reg_ofs_base_msb = AFE_DL6_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL6_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL6_END_MSB,
		.fs_reg = AFE_DL6_CON0,
		.fs_shift = DL6_SEL_FS_SFT,
		.fs_maskbit = DL6_SEL_FS_MASK,
		.mono_reg = AFE_DL6_CON0,
		.mono_shift = DL6_MONO_SFT,
		.enable_reg = AFE_DL6_CON0,
		.enable_shift = DL6_ON_SFT,
		.hd_reg = AFE_DL6_CON0,
		.hd_mask = DL6_HD_MODE_MASK,
		.hd_shift = DL6_HD_MODE_SFT,
		.hd_align_reg = AFE_DL6_CON0,
		.hd_align_mshift = DL6_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL6_CON0,
		.pbuf_mask = DL6_PBUF_SIZE_MASK,
		.pbuf_shift = DL6_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL6_CON0,
		.minlen_mask = DL6_MINLEN_MASK,
		.minlen_shift = DL6_MINLEN_SFT,
	},
	[MT8189_MEMIF_DL7] = {
		.name = "DL7",
		.id = MT8189_MEMIF_DL7,
		.reg_ofs_base = AFE_DL7_BASE,
		.reg_ofs_cur = AFE_DL7_CUR,
		.reg_ofs_end = AFE_DL7_END,
		.reg_ofs_base_msb = AFE_DL7_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL7_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL7_END_MSB,
		.fs_reg = AFE_DL7_CON0,
		.fs_shift = DL7_SEL_FS_SFT,
		.fs_maskbit = DL7_SEL_FS_MASK,
		.mono_reg = AFE_DL7_CON0,
		.mono_shift = DL7_MONO_SFT,
		.enable_reg = AFE_DL7_CON0,
		.enable_shift = DL7_ON_SFT,
		.hd_reg = AFE_DL7_CON0,
		.hd_mask = DL7_HD_MODE_MASK,
		.hd_shift = DL7_HD_MODE_SFT,
		.hd_align_reg = AFE_DL7_CON0,
		.hd_align_mshift = DL7_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL7_CON0,
		.pbuf_mask = DL7_PBUF_SIZE_MASK,
		.pbuf_shift = DL7_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL7_CON0,
		.minlen_mask = DL7_MINLEN_MASK,
		.minlen_shift = DL7_MINLEN_SFT,
	},
	[MT8189_MEMIF_DL8] = {
		.name = "DL8",
		.id = MT8189_MEMIF_DL8,
		.reg_ofs_base = AFE_DL8_BASE,
		.reg_ofs_cur = AFE_DL8_CUR,
		.reg_ofs_end = AFE_DL8_END,
		.reg_ofs_base_msb = AFE_DL8_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL8_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL8_END_MSB,
		.fs_reg = AFE_DL8_CON0,
		.fs_shift = DL8_SEL_FS_SFT,
		.fs_maskbit = DL8_SEL_FS_MASK,
		.mono_reg = AFE_DL8_CON0,
		.mono_shift = DL8_MONO_SFT,
		.enable_reg = AFE_DL8_CON0,
		.enable_shift = DL8_ON_SFT,
		.hd_reg = AFE_DL8_CON0,
		.hd_mask = DL8_HD_MODE_MASK,
		.hd_shift = DL8_HD_MODE_SFT,
		.hd_align_reg = AFE_DL8_CON0,
		.hd_align_mshift = DL8_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL8_CON0,
		.pbuf_mask = DL8_PBUF_SIZE_MASK,
		.pbuf_shift = DL8_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL8_CON0,
		.minlen_mask = DL8_MINLEN_MASK,
		.minlen_shift = DL8_MINLEN_SFT,
	},
	[MT8189_MEMIF_DL23] = {
		.name = "DL23",
		.id = MT8189_MEMIF_DL23,
		.reg_ofs_base = AFE_DL23_BASE,
		.reg_ofs_cur = AFE_DL23_CUR,
		.reg_ofs_end = AFE_DL23_END,
		.reg_ofs_base_msb = AFE_DL23_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL23_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL23_END_MSB,
		.fs_reg = AFE_DL23_CON0,
		.fs_shift = DL23_SEL_FS_SFT,
		.fs_maskbit = DL23_SEL_FS_MASK,
		.mono_reg = AFE_DL23_CON0,
		.mono_shift = DL23_MONO_SFT,
		.enable_reg = AFE_DL23_CON0,
		.enable_shift = DL23_ON_SFT,
		.hd_reg = AFE_DL23_CON0,
		.hd_mask = DL23_HD_MODE_MASK,
		.hd_shift = DL23_HD_MODE_SFT,
		.hd_align_reg = AFE_DL23_CON0,
		.hd_align_mshift = DL23_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL23_CON0,
		.pbuf_mask = DL23_PBUF_SIZE_MASK,
		.pbuf_shift = DL23_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL23_CON0,
		.minlen_mask = DL23_MINLEN_MASK,
		.minlen_shift = DL23_MINLEN_SFT,
	},
	[MT8189_MEMIF_DL24] = {
		.name = "DL24",
		.id = MT8189_MEMIF_DL24,
		.reg_ofs_base = AFE_DL24_BASE,
		.reg_ofs_cur = AFE_DL24_CUR,
		.reg_ofs_end = AFE_DL24_END,
		.reg_ofs_base_msb = AFE_DL24_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL24_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL24_END_MSB,
		.fs_reg = AFE_DL24_CON0,
		.fs_shift = DL24_SEL_FS_SFT,
		.fs_maskbit = DL24_SEL_FS_MASK,
		.mono_reg = AFE_DL24_CON0,
		.mono_shift = DL24_MONO_SFT,
		.enable_reg = AFE_DL24_CON0,
		.enable_shift = DL24_ON_SFT,
		.hd_reg = AFE_DL24_CON0,
		.hd_mask = DL24_HD_MODE_MASK,
		.hd_shift = DL24_HD_MODE_SFT,
		.hd_align_reg = AFE_DL24_CON0,
		.hd_align_mshift = DL24_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL24_CON0,
		.pbuf_mask = DL24_PBUF_SIZE_MASK,
		.pbuf_shift = DL24_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL24_CON0,
		.minlen_mask = DL24_MINLEN_MASK,
		.minlen_shift = DL24_MINLEN_SFT,
	},
	[MT8189_MEMIF_DL25] = {
		.name = "DL25",
		.id = MT8189_MEMIF_DL25,
		.reg_ofs_base = AFE_DL25_BASE,
		.reg_ofs_cur = AFE_DL25_CUR,
		.reg_ofs_end = AFE_DL25_END,
		.reg_ofs_base_msb = AFE_DL25_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL25_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL25_END_MSB,
		.fs_reg = AFE_DL25_CON0,
		.fs_shift = DL25_SEL_FS_SFT,
		.fs_maskbit = DL25_SEL_FS_MASK,
		.mono_reg = AFE_DL25_CON0,
		.mono_shift = DL25_MONO_SFT,
		.enable_reg = AFE_DL25_CON0,
		.enable_shift = DL25_ON_SFT,
		.hd_reg = AFE_DL25_CON0,
		.hd_mask = DL25_HD_MODE_MASK,
		.hd_shift = DL25_HD_MODE_SFT,
		.hd_align_reg = AFE_DL25_CON0,
		.hd_align_mshift = DL25_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL25_CON0,
		.pbuf_mask = DL25_PBUF_SIZE_MASK,
		.pbuf_shift = DL25_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL25_CON0,
		.minlen_mask = DL25_MINLEN_MASK,
		.minlen_shift = DL25_MINLEN_SFT,
	},
	[MT8189_MEMIF_DL_24CH] = {
		.name = "DL_24CH",
		.id = MT8189_MEMIF_DL_24CH,
		.reg_ofs_base = AFE_DL_24CH_BASE,
		.reg_ofs_cur = AFE_DL_24CH_CUR,
		.reg_ofs_end = AFE_DL_24CH_END,
		.reg_ofs_base_msb = AFE_DL_24CH_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL_24CH_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL_24CH_END_MSB,
		.fs_reg = AFE_DL_24CH_CON0,
		.fs_shift = DL_24CH_SEL_FS_SFT,
		.fs_maskbit = DL_24CH_SEL_FS_MASK,
		.mono_reg = -1,
		.mono_shift = -1,
		.enable_reg = AFE_DL_24CH_CON0,
		.enable_shift = DL_24CH_ON_SFT,
		.hd_reg = AFE_DL_24CH_CON0,
		.hd_mask = DL_24CH_HD_MODE_MASK,
		.hd_shift = DL_24CH_HD_MODE_SFT,
		.hd_align_reg = AFE_DL_24CH_CON0,
		.hd_align_mshift = DL_24CH_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL_24CH_CON0,
		.pbuf_mask = DL_24CH_PBUF_SIZE_MASK,
		.pbuf_shift = DL_24CH_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL_24CH_CON0,
		.minlen_mask = DL_24CH_MINLEN_MASK,
		.minlen_shift = DL_24CH_MINLEN_SFT,
		.ch_num_reg = AFE_DL_24CH_CON0,
		.ch_num_maskbit = DL_24CH_NUM_MASK,
		.ch_num_shift = DL_24CH_NUM_SFT,
	},
	[MT8189_MEMIF_VUL0] = {
		.name = "VUL0",
		.id = MT8189_MEMIF_VUL0,
		.reg_ofs_base = AFE_VUL0_BASE,
		.reg_ofs_cur = AFE_VUL0_CUR,
		.reg_ofs_end = AFE_VUL0_END,
		.reg_ofs_base_msb = AFE_VUL0_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL0_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL0_END_MSB,
		.fs_reg = AFE_VUL0_CON0,
		.fs_shift = VUL0_SEL_FS_SFT,
		.fs_maskbit = VUL0_SEL_FS_MASK,
		.mono_reg = AFE_VUL0_CON0,
		.mono_shift = VUL0_MONO_SFT,
		.enable_reg = AFE_VUL0_CON0,
		.enable_shift = VUL0_ON_SFT,
		.hd_reg = AFE_VUL0_CON0,
		.hd_mask = VUL0_HD_MODE_MASK,
		.hd_shift = VUL0_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL0_CON0,
		.hd_align_mshift = VUL0_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT8189_MEMIF_VUL1] = {
		.name = "VUL1",
		.id = MT8189_MEMIF_VUL1,
		.reg_ofs_base = AFE_VUL1_BASE,
		.reg_ofs_cur = AFE_VUL1_CUR,
		.reg_ofs_end = AFE_VUL1_END,
		.reg_ofs_base_msb = AFE_VUL1_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL1_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL1_END_MSB,
		.fs_reg = AFE_VUL1_CON0,
		.fs_shift = VUL1_SEL_FS_SFT,
		.fs_maskbit = VUL1_SEL_FS_MASK,
		.mono_reg = AFE_VUL1_CON0,
		.mono_shift = VUL1_MONO_SFT,
		.enable_reg = AFE_VUL1_CON0,
		.enable_shift = VUL1_ON_SFT,
		.hd_reg = AFE_VUL1_CON0,
		.hd_mask = VUL1_HD_MODE_MASK,
		.hd_shift = VUL1_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL1_CON0,
		.hd_align_mshift = VUL1_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT8189_MEMIF_VUL2] = {
		.name = "VUL2",
		.id = MT8189_MEMIF_VUL2,
		.reg_ofs_base = AFE_VUL2_BASE,
		.reg_ofs_cur = AFE_VUL2_CUR,
		.reg_ofs_end = AFE_VUL2_END,
		.reg_ofs_base_msb = AFE_VUL2_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL2_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL2_END_MSB,
		.fs_reg = AFE_VUL2_CON0,
		.fs_shift = VUL2_SEL_FS_SFT,
		.fs_maskbit = VUL2_SEL_FS_MASK,
		.mono_reg = AFE_VUL2_CON0,
		.mono_shift = VUL2_MONO_SFT,
		.enable_reg = AFE_VUL2_CON0,
		.enable_shift = VUL2_ON_SFT,
		.hd_reg = AFE_VUL2_CON0,
		.hd_mask = VUL2_HD_MODE_MASK,
		.hd_shift = VUL2_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL2_CON0,
		.hd_align_mshift = VUL2_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT8189_MEMIF_VUL3] = {
		.name = "VUL3",
		.id = MT8189_MEMIF_VUL3,
		.reg_ofs_base = AFE_VUL3_BASE,
		.reg_ofs_cur = AFE_VUL3_CUR,
		.reg_ofs_end = AFE_VUL3_END,
		.reg_ofs_base_msb = AFE_VUL3_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL3_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL3_END_MSB,
		.fs_reg = AFE_VUL3_CON0,
		.fs_shift = VUL3_SEL_FS_SFT,
		.fs_maskbit = VUL3_SEL_FS_MASK,
		.mono_reg = AFE_VUL3_CON0,
		.mono_shift = VUL3_MONO_SFT,
		.enable_reg = AFE_VUL3_CON0,
		.enable_shift = VUL3_ON_SFT,
		.hd_reg = AFE_VUL3_CON0,
		.hd_mask = VUL3_HD_MODE_MASK,
		.hd_shift = VUL3_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL3_CON0,
		.hd_align_mshift = VUL3_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT8189_MEMIF_VUL4] = {
		.name = "VUL4",
		.id = MT8189_MEMIF_VUL4,
		.reg_ofs_base = AFE_VUL4_BASE,
		.reg_ofs_cur = AFE_VUL4_CUR,
		.reg_ofs_end = AFE_VUL4_END,
		.reg_ofs_base_msb = AFE_VUL4_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL4_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL4_END_MSB,
		.fs_reg = AFE_VUL4_CON0,
		.fs_shift = VUL4_SEL_FS_SFT,
		.fs_maskbit = VUL4_SEL_FS_MASK,
		.mono_reg = AFE_VUL4_CON0,
		.mono_shift = VUL4_MONO_SFT,
		.enable_reg = AFE_VUL4_CON0,
		.enable_shift = VUL4_ON_SFT,
		.hd_reg = AFE_VUL4_CON0,
		.hd_mask = VUL4_HD_MODE_MASK,
		.hd_shift = VUL4_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL4_CON0,
		.hd_align_mshift = VUL4_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT8189_MEMIF_VUL5] = {
		.name = "VUL5",
		.id = MT8189_MEMIF_VUL5,
		.reg_ofs_base = AFE_VUL5_BASE,
		.reg_ofs_cur = AFE_VUL5_CUR,
		.reg_ofs_end = AFE_VUL5_END,
		.reg_ofs_base_msb = AFE_VUL5_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL5_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL5_END_MSB,
		.fs_reg = AFE_VUL5_CON0,
		.fs_shift = VUL5_SEL_FS_SFT,
		.fs_maskbit = VUL5_SEL_FS_MASK,
		.mono_reg = AFE_VUL5_CON0,
		.mono_shift = VUL5_MONO_SFT,
		.enable_reg = AFE_VUL5_CON0,
		.enable_shift = VUL5_ON_SFT,
		.hd_reg = AFE_VUL5_CON0,
		.hd_mask = VUL5_HD_MODE_MASK,
		.hd_shift = VUL5_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL5_CON0,
		.hd_align_mshift = VUL5_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT8189_MEMIF_VUL6] = {
		.name = "VUL6",
		.id = MT8189_MEMIF_VUL6,
		.reg_ofs_base = AFE_VUL6_BASE,
		.reg_ofs_cur = AFE_VUL6_CUR,
		.reg_ofs_end = AFE_VUL6_END,
		.reg_ofs_base_msb = AFE_VUL6_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL6_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL6_END_MSB,
		.fs_reg = AFE_VUL6_CON0,
		.fs_shift = VUL6_SEL_FS_SFT,
		.fs_maskbit = VUL6_SEL_FS_MASK,
		.mono_reg = AFE_VUL6_CON0,
		.mono_shift = VUL6_MONO_SFT,
		.enable_reg = AFE_VUL6_CON0,
		.enable_shift = VUL6_ON_SFT,
		.hd_reg = AFE_VUL6_CON0,
		.hd_mask = VUL6_HD_MODE_MASK,
		.hd_shift = VUL6_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL6_CON0,
		.hd_align_mshift = VUL6_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT8189_MEMIF_VUL7] = {
		.name = "VUL7",
		.id = MT8189_MEMIF_VUL7,
		.reg_ofs_base = AFE_VUL7_BASE,
		.reg_ofs_cur = AFE_VUL7_CUR,
		.reg_ofs_end = AFE_VUL7_END,
		.reg_ofs_base_msb = AFE_VUL7_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL7_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL7_END_MSB,
		.fs_reg = AFE_VUL7_CON0,
		.fs_shift = VUL7_SEL_FS_SFT,
		.fs_maskbit = VUL7_SEL_FS_MASK,
		.mono_reg = AFE_VUL7_CON0,
		.mono_shift = VUL7_MONO_SFT,
		.enable_reg = AFE_VUL7_CON0,
		.enable_shift = VUL7_ON_SFT,
		.hd_reg = AFE_VUL7_CON0,
		.hd_mask = VUL7_HD_MODE_MASK,
		.hd_shift = VUL7_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL7_CON0,
		.hd_align_mshift = VUL7_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT8189_MEMIF_VUL8] = {
		.name = "VUL8",
		.id = MT8189_MEMIF_VUL8,
		.reg_ofs_base = AFE_VUL8_BASE,
		.reg_ofs_cur = AFE_VUL8_CUR,
		.reg_ofs_end = AFE_VUL8_END,
		.reg_ofs_base_msb = AFE_VUL8_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL8_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL8_END_MSB,
		.fs_reg = AFE_VUL8_CON0,
		.fs_shift = VUL8_SEL_FS_SFT,
		.fs_maskbit = VUL8_SEL_FS_MASK,
		.mono_reg = AFE_VUL8_CON0,
		.mono_shift = VUL8_MONO_SFT,
		.enable_reg = AFE_VUL8_CON0,
		.enable_shift = VUL8_ON_SFT,
		.hd_reg = AFE_VUL8_CON0,
		.hd_mask = VUL8_HD_MODE_MASK,
		.hd_shift = VUL8_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL8_CON0,
		.hd_align_mshift = VUL8_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT8189_MEMIF_VUL9] = {
		.name = "VUL9",
		.id = MT8189_MEMIF_VUL9,
		.reg_ofs_base = AFE_VUL9_BASE,
		.reg_ofs_cur = AFE_VUL9_CUR,
		.reg_ofs_end = AFE_VUL9_END,
		.reg_ofs_base_msb = AFE_VUL9_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL9_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL9_END_MSB,
		.fs_reg = AFE_VUL9_CON0,
		.fs_shift = VUL9_SEL_FS_SFT,
		.fs_maskbit = VUL9_SEL_FS_MASK,
		.mono_reg = AFE_VUL9_CON0,
		.mono_shift = VUL9_MONO_SFT,
		.enable_reg = AFE_VUL9_CON0,
		.enable_shift = VUL9_ON_SFT,
		.hd_reg = AFE_VUL9_CON0,
		.hd_mask = VUL9_HD_MODE_MASK,
		.hd_shift = VUL9_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL9_CON0,
		.hd_align_mshift = VUL9_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT8189_MEMIF_VUL10] = {
		.name = "VUL10",
		.id = MT8189_MEMIF_VUL10,
		.reg_ofs_base = AFE_VUL10_BASE,
		.reg_ofs_cur = AFE_VUL10_CUR,
		.reg_ofs_end = AFE_VUL10_END,
		.reg_ofs_base_msb = AFE_VUL10_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL10_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL10_END_MSB,
		.fs_reg = AFE_VUL10_CON0,
		.fs_shift = VUL10_SEL_FS_SFT,
		.fs_maskbit = VUL10_SEL_FS_MASK,
		.mono_reg = AFE_VUL10_CON0,
		.mono_shift = VUL10_MONO_SFT,
		.enable_reg = AFE_VUL10_CON0,
		.enable_shift = VUL10_ON_SFT,
		.hd_reg = AFE_VUL10_CON0,
		.hd_mask = VUL10_HD_MODE_MASK,
		.hd_shift = VUL10_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL10_CON0,
		.hd_align_mshift = VUL10_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT8189_MEMIF_VUL24] = {
		.name = "VUL24",
		.id = MT8189_MEMIF_VUL24,
		.reg_ofs_base = AFE_VUL24_BASE,
		.reg_ofs_cur = AFE_VUL24_CUR,
		.reg_ofs_end = AFE_VUL24_END,
		.reg_ofs_base_msb = AFE_VUL24_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL24_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL24_END_MSB,
		.fs_reg = AFE_VUL24_CON0,
		.fs_shift = VUL24_SEL_FS_SFT,
		.fs_maskbit = VUL24_SEL_FS_MASK,
		.mono_reg = AFE_VUL24_CON0,
		.mono_shift = VUL24_MONO_SFT,
		.enable_reg = AFE_VUL24_CON0,
		.enable_shift = VUL24_ON_SFT,
		.hd_reg = AFE_VUL24_CON0,
		.hd_mask = VUL24_HD_MODE_MASK,
		.hd_shift = VUL24_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL24_CON0,
		.hd_align_mshift = VUL24_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT8189_MEMIF_VUL25] = {
		.name = "VUL25",
		.id = MT8189_MEMIF_VUL25,
		.reg_ofs_base = AFE_VUL25_BASE,
		.reg_ofs_cur = AFE_VUL25_CUR,
		.reg_ofs_end = AFE_VUL25_END,
		.reg_ofs_base_msb = AFE_VUL25_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL25_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL25_END_MSB,
		.fs_reg = AFE_VUL25_CON0,
		.fs_shift = VUL25_SEL_FS_SFT,
		.fs_maskbit = VUL25_SEL_FS_MASK,
		.mono_reg = AFE_VUL25_CON0,
		.mono_shift = VUL25_MONO_SFT,
		.enable_reg = AFE_VUL25_CON0,
		.enable_shift = VUL25_ON_SFT,
		.hd_reg = AFE_VUL25_CON0,
		.hd_mask = VUL25_HD_MODE_MASK,
		.hd_shift = VUL25_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL25_CON0,
		.hd_align_mshift = VUL25_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT8189_MEMIF_VUL_CM0] = {
		.name = "VUL_CM0",
		.id = MT8189_MEMIF_VUL_CM0,
		.reg_ofs_base = AFE_VUL_CM0_BASE,
		.reg_ofs_cur = AFE_VUL_CM0_CUR,
		.reg_ofs_end = AFE_VUL_CM0_END,
		.reg_ofs_base_msb = AFE_VUL_CM0_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL_CM0_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL_CM0_END_MSB,
		.enable_reg = AFE_VUL_CM0_CON0,
		.enable_shift = VUL_CM0_ON_SFT,
		.hd_reg = AFE_VUL_CM0_CON0,
		.hd_mask = VUL_CM0_HD_MODE_MASK,
		.hd_shift = VUL_CM0_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL_CM0_CON0,
		.hd_align_mshift = VUL_CM0_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT8189_MEMIF_VUL_CM1] = {
		.name = "VUL_CM1",
		.id = MT8189_MEMIF_VUL_CM1,
		.reg_ofs_base = AFE_VUL_CM1_BASE,
		.reg_ofs_cur = AFE_VUL_CM1_CUR,
		.reg_ofs_end = AFE_VUL_CM1_END,
		.reg_ofs_base_msb = AFE_VUL_CM1_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL_CM1_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL_CM1_END_MSB,
		.enable_reg = AFE_VUL_CM1_CON0,
		.enable_shift = VUL_CM1_ON_SFT,
		.hd_reg = AFE_VUL_CM1_CON0,
		.hd_mask = VUL_CM1_HD_MODE_MASK,
		.hd_shift = VUL_CM1_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL_CM1_CON0,
		.hd_align_mshift = VUL_CM1_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT8189_MEMIF_ETDM_IN0] = {
		.name = "ETDM_IN0",
		.id = MT8189_MEMIF_ETDM_IN0,
		.reg_ofs_base = AFE_ETDM_IN0_BASE,
		.reg_ofs_cur = AFE_ETDM_IN0_CUR,
		.reg_ofs_end = AFE_ETDM_IN0_END,
		.reg_ofs_base_msb = AFE_ETDM_IN0_BASE_MSB,
		.reg_ofs_cur_msb = AFE_ETDM_IN0_CUR_MSB,
		.reg_ofs_end_msb = AFE_ETDM_IN0_END_MSB,
		.fs_reg = ETDM_IN0_CON3,
		.fs_shift = REG_FS_TIMING_SEL_SFT,
		.fs_maskbit = REG_FS_TIMING_SEL_MASK,
		.enable_reg = AFE_ETDM_IN0_CON0,
		.enable_shift = ETDM_IN0_ON_SFT,
		.hd_reg = AFE_ETDM_IN0_CON0,
		.hd_mask = ETDM_IN0_HD_MODE_MASK,
		.hd_shift = ETDM_IN0_HD_MODE_SFT,
		.hd_align_reg = AFE_ETDM_IN0_CON0,
		.hd_align_mshift = ETDM_IN0_HALIGN_SFT,
		.hd_msb_shift = -1,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT8189_MEMIF_ETDM_IN1] = {
		.name = "ETDM_IN1",
		.id = MT8189_MEMIF_ETDM_IN1,
		.reg_ofs_base = AFE_ETDM_IN1_BASE,
		.reg_ofs_cur = AFE_ETDM_IN1_CUR,
		.reg_ofs_end = AFE_ETDM_IN1_END,
		.reg_ofs_base_msb = AFE_ETDM_IN1_BASE_MSB,
		.reg_ofs_cur_msb = AFE_ETDM_IN1_CUR_MSB,
		.reg_ofs_end_msb = AFE_ETDM_IN1_END_MSB,
		.fs_reg = ETDM_IN1_CON3,
		.fs_shift = REG_FS_TIMING_SEL_SFT,
		.fs_maskbit = REG_FS_TIMING_SEL_MASK,
		.enable_reg = AFE_ETDM_IN1_CON0,
		.enable_shift = ETDM_IN1_ON_SFT,
		.hd_reg = AFE_ETDM_IN1_CON0,
		.hd_mask = ETDM_IN1_HD_MODE_MASK,
		.hd_shift = ETDM_IN1_HD_MODE_SFT,
		.hd_align_reg = AFE_ETDM_IN1_CON0,
		.hd_align_mshift = ETDM_IN1_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},

	[MT8189_MEMIF_HDMI] = {
		.name = "HDMI",
		.id = MT8189_MEMIF_HDMI,
		.reg_ofs_base = AFE_HDMI_OUT_BASE,
		.reg_ofs_cur = AFE_HDMI_OUT_CUR,
		.reg_ofs_end = AFE_HDMI_OUT_END,
		.reg_ofs_base_msb = AFE_HDMI_OUT_BASE_MSB,
		.reg_ofs_cur_msb = AFE_HDMI_OUT_CUR_MSB,
		.reg_ofs_end_msb = AFE_HDMI_OUT_END_MSB,
		.fs_reg = -1,
		.fs_shift = -1,
		.fs_maskbit = -1,
		.mono_reg = -1,
		.mono_shift = -1,
		.enable_reg = AFE_HDMI_OUT_CON0,
		.enable_shift = HDMI_OUT_ON_SFT,
		.hd_reg = AFE_HDMI_OUT_CON0,
		.hd_mask = HDMI_OUT_HD_MODE_MASK,
		.hd_shift = HDMI_OUT_HD_MODE_SFT,
		.hd_align_reg = AFE_HDMI_OUT_CON0,
		.hd_align_mshift = HDMI_OUT_HALIGN_SFT,
		.hd_msb_shift = -1,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_HDMI_OUT_CON0,
		.pbuf_mask = HDMI_OUT_PBUF_SIZE_MASK,
		.pbuf_shift = HDMI_OUT_PBUF_SIZE_SFT,
		.minlen_reg = AFE_HDMI_OUT_CON0,
		.minlen_mask = HDMI_OUT_MINLEN_MASK,
		.minlen_shift = HDMI_OUT_MINLEN_SFT,
	},
};

static const struct mtk_base_irq_data irq_data[MT8189_IRQ_NUM] = {
	[MT8189_IRQ_0] = {
		.id = MT8189_IRQ_0,
		.irq_cnt_reg = AFE_IRQ0_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ0_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ0_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ0_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ0_MCU_CFG0,
		.irq_en_shift = AFE_IRQ0_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ0_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ0_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ0_MCU_SCP_EN_SFT,
	},
	[MT8189_IRQ_1] = {
		.id = MT8189_IRQ_1,
		.irq_cnt_reg = AFE_IRQ1_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ1_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ1_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ1_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ1_MCU_CFG0,
		.irq_en_shift = AFE_IRQ1_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ1_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ1_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ1_MCU_SCP_EN_SFT,
	},
	[MT8189_IRQ_2] = {
		.id = MT8189_IRQ_2,
		.irq_cnt_reg = AFE_IRQ2_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ2_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ2_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ2_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ2_MCU_CFG0,
		.irq_en_shift = AFE_IRQ2_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ2_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ2_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ2_MCU_SCP_EN_SFT,
	},
	[MT8189_IRQ_3] = {
		.id = MT8189_IRQ_3,
		.irq_cnt_reg = AFE_IRQ3_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ3_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ3_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ3_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ3_MCU_CFG0,
		.irq_en_shift = AFE_IRQ3_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ3_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ3_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ3_MCU_SCP_EN_SFT,
	},
	[MT8189_IRQ_4] = {
		.id = MT8189_IRQ_4,
		.irq_cnt_reg = AFE_IRQ4_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ4_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ4_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ4_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ4_MCU_CFG0,
		.irq_en_shift = AFE_IRQ4_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ4_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ4_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ4_MCU_SCP_EN_SFT,
	},
	[MT8189_IRQ_5] = {
		.id = MT8189_IRQ_5,
		.irq_cnt_reg = AFE_IRQ5_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ5_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ5_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ5_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ5_MCU_CFG0,
		.irq_en_shift = AFE_IRQ5_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ5_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ5_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ5_MCU_SCP_EN_SFT,
	},
	[MT8189_IRQ_6] = {
		.id = MT8189_IRQ_6,
		.irq_cnt_reg = AFE_IRQ6_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ6_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ6_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ6_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ6_MCU_CFG0,
		.irq_en_shift = AFE_IRQ6_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ6_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ6_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ6_MCU_SCP_EN_SFT,
	},
	[MT8189_IRQ_7] = {
		.id = MT8189_IRQ_7,
		.irq_cnt_reg = AFE_IRQ7_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ7_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ7_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ7_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ7_MCU_CFG0,
		.irq_en_shift = AFE_IRQ7_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ7_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ7_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ7_MCU_SCP_EN_SFT,
	},
	[MT8189_IRQ_8] = {
		.id = MT8189_IRQ_8,
		.irq_cnt_reg = AFE_IRQ8_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ8_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ8_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ8_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ8_MCU_CFG0,
		.irq_en_shift = AFE_IRQ8_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ8_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ8_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ8_MCU_SCP_EN_SFT,
	},
	[MT8189_IRQ_9] = {
		.id = MT8189_IRQ_9,
		.irq_cnt_reg = AFE_IRQ9_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ9_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ9_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ9_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ9_MCU_CFG0,
		.irq_en_shift = AFE_IRQ9_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ9_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ9_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ9_MCU_SCP_EN_SFT,
	},
	[MT8189_IRQ_10] = {
		.id = MT8189_IRQ_10,
		.irq_cnt_reg = AFE_IRQ10_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ10_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ10_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ10_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ10_MCU_CFG0,
		.irq_en_shift = AFE_IRQ10_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ10_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ10_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ10_MCU_SCP_EN_SFT,
	},
	[MT8189_IRQ_11] = {
		.id = MT8189_IRQ_11,
		.irq_cnt_reg = AFE_IRQ11_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ11_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ11_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ11_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ11_MCU_CFG0,
		.irq_en_shift = AFE_IRQ11_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ11_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ11_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ11_MCU_SCP_EN_SFT,
	},
	[MT8189_IRQ_12] = {
		.id = MT8189_IRQ_12,
		.irq_cnt_reg = AFE_IRQ12_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ12_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ12_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ12_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ12_MCU_CFG0,
		.irq_en_shift = AFE_IRQ12_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ12_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ12_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ12_MCU_SCP_EN_SFT,
	},
	[MT8189_IRQ_13] = {
		.id = MT8189_IRQ_13,
		.irq_cnt_reg = AFE_IRQ13_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ13_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ13_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ13_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ13_MCU_CFG0,
		.irq_en_shift = AFE_IRQ13_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ13_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ13_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ13_MCU_SCP_EN_SFT,
	},
	[MT8189_IRQ_14] = {
		.id = MT8189_IRQ_14,
		.irq_cnt_reg = AFE_IRQ14_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ14_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ14_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ14_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ14_MCU_CFG0,
		.irq_en_shift = AFE_IRQ14_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ14_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ14_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ14_MCU_SCP_EN_SFT,
	},
	[MT8189_IRQ_15] = {
		.id = MT8189_IRQ_15,
		.irq_cnt_reg = AFE_IRQ15_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ15_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ15_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ15_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ15_MCU_CFG0,
		.irq_en_shift = AFE_IRQ15_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ15_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ15_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ15_MCU_SCP_EN_SFT,
	},
	[MT8189_IRQ_16] = {
		.id = MT8189_IRQ_16,
		.irq_cnt_reg = AFE_IRQ16_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ16_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ16_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ16_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ16_MCU_CFG0,
		.irq_en_shift = AFE_IRQ16_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ16_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ16_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ16_MCU_SCP_EN_SFT,
	},
	[MT8189_IRQ_17] = {
		.id = MT8189_IRQ_17,
		.irq_cnt_reg = AFE_IRQ17_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ17_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ17_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ17_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ17_MCU_CFG0,
		.irq_en_shift = AFE_IRQ17_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ17_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ17_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ17_MCU_SCP_EN_SFT,
	},
	[MT8189_IRQ_18] = {
		.id = MT8189_IRQ_18,
		.irq_cnt_reg = AFE_IRQ18_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ18_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ18_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ18_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ18_MCU_CFG0,
		.irq_en_shift = AFE_IRQ18_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ18_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ18_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ18_MCU_SCP_EN_SFT,
	},
	[MT8189_IRQ_19] = {
		.id = MT8189_IRQ_19,
		.irq_cnt_reg = AFE_IRQ19_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ19_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ19_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ19_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ19_MCU_CFG0,
		.irq_en_shift = AFE_IRQ19_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ19_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ19_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ19_MCU_SCP_EN_SFT,
	},
	[MT8189_IRQ_20] = {
		.id = MT8189_IRQ_20,
		.irq_cnt_reg = AFE_IRQ20_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ20_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ20_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ20_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ20_MCU_CFG0,
		.irq_en_shift = AFE_IRQ20_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ20_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ20_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ20_MCU_SCP_EN_SFT,
	},
	[MT8189_IRQ_21] = {
		.id = MT8189_IRQ_21,
		.irq_cnt_reg = AFE_IRQ21_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ21_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ21_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ21_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ21_MCU_CFG0,
		.irq_en_shift = AFE_IRQ21_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ21_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ21_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ21_MCU_SCP_EN_SFT,
	},
	[MT8189_IRQ_22] = {
		.id = MT8189_IRQ_22,
		.irq_cnt_reg = AFE_IRQ22_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ22_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ22_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ22_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ22_MCU_CFG0,
		.irq_en_shift = AFE_IRQ22_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ22_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ22_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ22_MCU_SCP_EN_SFT,
	},
	[MT8189_IRQ_23] = {
		.id = MT8189_IRQ_23,
		.irq_cnt_reg = AFE_IRQ23_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ23_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ23_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ23_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ23_MCU_CFG0,
		.irq_en_shift = AFE_IRQ23_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ23_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ23_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ23_MCU_SCP_EN_SFT,
	},
	[MT8189_IRQ_24] = {
		.id = MT8189_IRQ_24,
		.irq_cnt_reg = AFE_IRQ24_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ24_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ24_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ24_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ24_MCU_CFG0,
		.irq_en_shift = AFE_IRQ24_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ24_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ24_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ24_MCU_SCP_EN_SFT,
	},
	[MT8189_IRQ_25] = {
		.id = MT8189_IRQ_25,
		.irq_cnt_reg = AFE_IRQ25_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ25_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ25_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ25_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ25_MCU_CFG0,
		.irq_en_shift = AFE_IRQ25_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ25_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ25_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ25_MCU_SCP_EN_SFT,
	},
	[MT8189_IRQ_26] = {
		.id = MT8189_IRQ_26,
		.irq_cnt_reg = AFE_IRQ26_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ26_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ26_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ26_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ26_MCU_CFG0,
		.irq_en_shift = AFE_IRQ26_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ26_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ26_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ26_MCU_SCP_EN_SFT,
	},

	//HDMI
	[MT8189_IRQ_31] = {
		.id = MT8189_CUS_IRQ_TDM,
		.irq_cnt_reg = AFE_CUSTOM_IRQ0_MCU_CFG1,
		.irq_cnt_shift = AFE_CUSTOM_IRQ0_MCU_CNT_SFT,
		.irq_cnt_maskbit = AFE_CUSTOM_IRQ0_MCU_CNT_MASK,
		.irq_fs_reg = -1,
		.irq_fs_shift = -1,
		.irq_fs_maskbit = -1,
		.irq_en_reg = AFE_CUSTOM_IRQ0_MCU_CFG0,
		.irq_en_shift = AFE_CUSTOM_IRQ0_MCU_ON_SFT,
		.irq_clr_reg = AFE_CUSTOM_IRQ0_MCU_CFG1,
		.irq_clr_shift = AFE_CUSTOM_IRQ0_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_CUSTOM_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_CUSTOM_IRQ_MCU_SCP_EN,
	},
};

static const int memif_irq_usage[MT8189_MEMIF_NUM] = {
	/* TODO: verify each memif & irq */
	[MT8189_MEMIF_DL0] = MT8189_IRQ_0,
	[MT8189_MEMIF_DL1] = MT8189_IRQ_1,
	[MT8189_MEMIF_DL2] = MT8189_IRQ_2,
	[MT8189_MEMIF_DL3] = MT8189_IRQ_3,
	[MT8189_MEMIF_DL4] = MT8189_IRQ_4,
	[MT8189_MEMIF_DL5] = MT8189_IRQ_5,
	[MT8189_MEMIF_DL6] = MT8189_IRQ_6,
	[MT8189_MEMIF_DL7] = MT8189_IRQ_7,
	[MT8189_MEMIF_DL8] = MT8189_IRQ_8,
	[MT8189_MEMIF_DL23] = MT8189_IRQ_9,
	[MT8189_MEMIF_DL24] = MT8189_IRQ_10,
	[MT8189_MEMIF_DL25] = MT8189_IRQ_11,
	[MT8189_MEMIF_DL_24CH] = MT8189_IRQ_12,
	[MT8189_MEMIF_VUL0] = MT8189_IRQ_13,
	[MT8189_MEMIF_VUL1] = MT8189_IRQ_14,
	[MT8189_MEMIF_VUL2] = MT8189_IRQ_15,
	[MT8189_MEMIF_VUL3] = MT8189_IRQ_16,
	[MT8189_MEMIF_VUL4] = MT8189_IRQ_17,
	[MT8189_MEMIF_VUL5] = MT8189_IRQ_18,
	[MT8189_MEMIF_VUL6] = MT8189_IRQ_19,
	[MT8189_MEMIF_VUL7] = MT8189_IRQ_20,
	[MT8189_MEMIF_VUL8] = MT8189_IRQ_21,
	[MT8189_MEMIF_VUL9] = MT8189_IRQ_22,
	[MT8189_MEMIF_VUL10] = MT8189_IRQ_23,
	[MT8189_MEMIF_VUL24] = MT8189_IRQ_24,
	[MT8189_MEMIF_VUL25] = MT8189_IRQ_25,
	[MT8189_MEMIF_VUL_CM0] = MT8189_IRQ_26,
	[MT8189_MEMIF_VUL_CM1] = MT8189_IRQ_0,
	[MT8189_MEMIF_ETDM_IN0] = MT8189_IRQ_0,
	[MT8189_MEMIF_ETDM_IN1] = MT8189_IRQ_0,
	[MT8189_MEMIF_HDMI] = MT8189_IRQ_31
};

static bool mt8189_is_volatile_reg(struct device *dev, unsigned int reg)
{
	/* these auto-gen reg has read-only bit, so put it as volatile */
	/* volatile reg cannot be cached, so cannot be set when power off */
	switch (reg) {
	case AUDIO_TOP_CON0:    /* reg bit controlled by CCF */
	case AUDIO_TOP_CON1:    /* reg bit controlled by CCF */
	case AUDIO_TOP_CON2:
	case AUDIO_TOP_CON3:
	case AUDIO_TOP_CON4:
	case AFE_APLL1_TUNER_MON0:
	case AFE_APLL2_TUNER_MON0:
	case AFE_SPM_CONTROL_ACK:
	case AUDIO_TOP_IP_VERSION:
	case AUDIO_ENGEN_CON0_MON:
	case AFE_CONNSYS_I2S_IPM_VER_MON:
	case AFE_CONNSYS_I2S_MON:
	case AFE_PCM_INTF_MON:
	case AFE_PCM_TOP_IP_VERSION:
	case AFE_IRQ_MCU_STATUS:
	case AFE_CUSTOM_IRQ_MCU_STATUS:
	case AFE_IRQ_MCU_MON0:
	case AFE_IRQ_MCU_MON1:
	case AFE_IRQ_MCU_MON2:
	case AFE_IRQ0_CNT_MON:
	case AFE_IRQ1_CNT_MON:
	case AFE_IRQ2_CNT_MON:
	case AFE_IRQ3_CNT_MON:
	case AFE_IRQ4_CNT_MON:
	case AFE_IRQ5_CNT_MON:
	case AFE_IRQ6_CNT_MON:
	case AFE_IRQ7_CNT_MON:
	case AFE_IRQ8_CNT_MON:
	case AFE_IRQ9_CNT_MON:
	case AFE_IRQ10_CNT_MON:
	case AFE_IRQ11_CNT_MON:
	case AFE_IRQ12_CNT_MON:
	case AFE_IRQ13_CNT_MON:
	case AFE_IRQ14_CNT_MON:
	case AFE_IRQ15_CNT_MON:
	case AFE_IRQ16_CNT_MON:
	case AFE_IRQ17_CNT_MON:
	case AFE_IRQ18_CNT_MON:
	case AFE_IRQ19_CNT_MON:
	case AFE_IRQ20_CNT_MON:
	case AFE_IRQ21_CNT_MON:
	case AFE_IRQ22_CNT_MON:
	case AFE_IRQ23_CNT_MON:
	case AFE_IRQ24_CNT_MON:
	case AFE_IRQ25_CNT_MON:
	case AFE_IRQ26_CNT_MON:
	case AFE_CM0_MON:
	case AFE_CM0_IP_VERSION:
	case AFE_CM1_MON:
	case AFE_CM1_IP_VERSION:
	case AFE_ADDA_UL0_SRC_DEBUG_MON0:
	case AFE_ADDA_UL0_SRC_MON0:
	case AFE_ADDA_UL0_SRC_MON1:
	case AFE_ADDA_UL0_IP_VERSION:
	case AFE_ADDA_DMIC0_SRC_DEBUG_MON0:
	case AFE_ADDA_DMIC0_SRC_MON0:
	case AFE_ADDA_DMIC0_SRC_MON1:
	case AFE_ADDA_DMIC0_IP_VERSION:
	case AFE_ADDA_DMIC1_SRC_DEBUG_MON0:
	case AFE_ADDA_DMIC1_SRC_MON0:
	case AFE_ADDA_DMIC1_SRC_MON1:
	case AFE_ADDA_DMIC1_IP_VERSION:
	case AFE_MTKAIF_IPM_VER_MON:
	case AFE_MTKAIF_MON:
	case AFE_AUD_PAD_TOP_MON:
	case AFE_ADDA_MTKAIFV4_MON0:
	case AFE_ADDA_MTKAIFV4_MON1:
	case AFE_ADDA6_MTKAIFV4_MON0:
	case ETDM_IN0_MON:
	case ETDM_IN1_MON:
	case ETDM_OUT0_MON:
	case ETDM_OUT1_MON:
	case ETDM_OUT4_MON:
	case AFE_CONN_MON0:
	case AFE_CONN_MON1:
	case AFE_CONN_MON2:
	case AFE_CONN_MON3:
	case AFE_CONN_MON4:
	case AFE_CONN_MON5:
	case AFE_CBIP_SLV_DECODER_MON0:
	case AFE_CBIP_SLV_DECODER_MON1:
	case AFE_CBIP_SLV_MUX_MON0:
	case AFE_CBIP_SLV_MUX_MON1:
	case AFE_DL0_CUR_MSB:
	case AFE_DL0_CUR:
	case AFE_DL0_RCH_MON:
	case AFE_DL0_LCH_MON:
	case AFE_DL1_CUR_MSB:
	case AFE_DL1_CUR:
	case AFE_DL1_RCH_MON:
	case AFE_DL1_LCH_MON:
	case AFE_DL2_CUR_MSB:
	case AFE_DL2_CUR:
	case AFE_DL2_RCH_MON:
	case AFE_DL2_LCH_MON:
	case AFE_DL3_CUR_MSB:
	case AFE_DL3_CUR:
	case AFE_DL3_RCH_MON:
	case AFE_DL3_LCH_MON:
	case AFE_DL4_CUR_MSB:
	case AFE_DL4_CUR:
	case AFE_DL4_RCH_MON:
	case AFE_DL4_LCH_MON:
	case AFE_DL5_CUR_MSB:
	case AFE_DL5_CUR:
	case AFE_DL5_RCH_MON:
	case AFE_DL5_LCH_MON:
	case AFE_DL6_CUR_MSB:
	case AFE_DL6_CUR:
	case AFE_DL6_RCH_MON:
	case AFE_DL6_LCH_MON:
	case AFE_DL7_CUR_MSB:
	case AFE_DL7_CUR:
	case AFE_DL7_RCH_MON:
	case AFE_DL7_LCH_MON:
	case AFE_DL8_CUR_MSB:
	case AFE_DL8_CUR:
	case AFE_DL8_RCH_MON:
	case AFE_DL8_LCH_MON:
	case AFE_DL_24CH_CUR_MSB:
	case AFE_DL_24CH_CUR:
	case AFE_DL23_CUR_MSB:
	case AFE_DL23_CUR:
	case AFE_DL23_RCH_MON:
	case AFE_DL23_LCH_MON:
	case AFE_DL24_CUR_MSB:
	case AFE_DL24_CUR:
	case AFE_DL24_RCH_MON:
	case AFE_DL24_LCH_MON:
	case AFE_DL25_CUR_MSB:
	case AFE_DL25_CUR:
	case AFE_DL25_RCH_MON:
	case AFE_DL25_LCH_MON:
	case AFE_VUL0_CUR_MSB:
	case AFE_VUL0_CUR:
	case AFE_VUL1_CUR_MSB:
	case AFE_VUL1_CUR:
	case AFE_VUL2_CUR_MSB:
	case AFE_VUL2_CUR:
	case AFE_VUL3_CUR_MSB:
	case AFE_VUL3_CUR:
	case AFE_VUL4_CUR_MSB:
	case AFE_VUL4_CUR:
	case AFE_VUL5_CUR_MSB:
	case AFE_VUL5_CUR:
	case AFE_VUL6_CUR_MSB:
	case AFE_VUL6_CUR:
	case AFE_VUL7_CUR_MSB:
	case AFE_VUL7_CUR:
	case AFE_VUL8_CUR_MSB:
	case AFE_VUL8_CUR:
	case AFE_VUL9_CUR_MSB:
	case AFE_VUL9_CUR:
	case AFE_VUL10_CUR_MSB:
	case AFE_VUL10_CUR:
	case AFE_VUL24_CUR_MSB:
	case AFE_VUL24_CUR:
	case AFE_VUL25_CUR_MSB:
	case AFE_VUL25_CUR:
	case AFE_VUL_CM0_CUR_MSB:
	case AFE_VUL_CM0_CUR:
	case AFE_VUL_CM1_CUR_MSB:
	case AFE_VUL_CM1_CUR:
	case AFE_ETDM_IN0_CUR_MSB:
	case AFE_ETDM_IN0_CUR:
	case AFE_ETDM_IN1_CUR_MSB:
	case AFE_ETDM_IN1_CUR:
	case AFE_HDMI_OUT_CUR_MSB:
	case AFE_HDMI_OUT_CUR:
	case AFE_HDMI_OUT_END:
	case AFE_HDMI_OUT_MON0:
	case AFE_PROT_SIDEBAND0_MON:
	case AFE_PROT_SIDEBAND1_MON:
	case AFE_PROT_SIDEBAND2_MON:
	case AFE_PROT_SIDEBAND3_MON:
	case AFE_DOMAIN_SIDEBAND0_MON:
	case AFE_DOMAIN_SIDEBAND1_MON:
	case AFE_DOMAIN_SIDEBAND2_MON:
	case AFE_DOMAIN_SIDEBAND3_MON:
	case AFE_DOMAIN_SIDEBAND4_MON:
	case AFE_DOMAIN_SIDEBAND5_MON:
	case AFE_DOMAIN_SIDEBAND6_MON:
	case AFE_DOMAIN_SIDEBAND7_MON:
	case AFE_DOMAIN_SIDEBAND8_MON:
	case AFE_DOMAIN_SIDEBAND9_MON:
	case AFE_PCM0_INTF_CON1_MASK_MON:
	case AFE_CONNSYS_I2S_CON_MASK_MON:
	case AFE_MTKAIF0_CFG0_MASK_MON:
	case AFE_MTKAIF1_CFG0_MASK_MON:
	case AFE_ADDA_UL0_SRC_CON0_MASK_MON:
	case AFE_ASRC_NEW_CON0:
	case AFE_ASRC_NEW_CON6:
	case AFE_ASRC_NEW_CON8:
	case AFE_ASRC_NEW_CON9:
	case AFE_ASRC_NEW_CON12:
	case AFE_ASRC_NEW_IP_VERSION:
	case AFE_GASRC0_NEW_CON0:
	case AFE_GASRC0_NEW_CON6:
	case AFE_GASRC0_NEW_CON8:
	case AFE_GASRC0_NEW_CON9:
	case AFE_GASRC0_NEW_CON10:
	case AFE_GASRC0_NEW_CON11:
	case AFE_GASRC0_NEW_CON12:
	case AFE_GASRC0_NEW_IP_VERSION:
	case AFE_GASRC1_NEW_CON0:
	case AFE_GASRC1_NEW_CON6:
	case AFE_GASRC1_NEW_CON8:
	case AFE_GASRC1_NEW_CON9:
	case AFE_GASRC1_NEW_CON12:
	case AFE_GASRC1_NEW_IP_VERSION:
	case AFE_GASRC2_NEW_CON0:
	case AFE_GASRC2_NEW_CON6:
	case AFE_GASRC2_NEW_CON8:
	case AFE_GASRC2_NEW_CON9:
	case AFE_GASRC2_NEW_CON12:
	case AFE_GASRC2_NEW_IP_VERSION:
	case AFE_GAIN0_CUR_L:
	case AFE_GAIN0_CUR_R:
	case AFE_GAIN1_CUR_L:
	case AFE_GAIN1_CUR_R:
	case AFE_GAIN2_CUR_L:
	case AFE_GAIN2_CUR_R:
	case AFE_GAIN3_CUR_L:
	case AFE_GAIN3_CUR_R:
	case AFE_IRQ_MCU_EN:
	case AFE_CUSTOM_IRQ_MCU_EN:
	case AFE_IRQ_MCU_DSP_EN:
	case AFE_IRQ_MCU_DSP2_EN:
	case AFE_DL5_CON0:
	case AFE_DL6_CON0:
	case AFE_DL23_CON0:
	case AFE_DL_24CH_CON0:
	case AFE_VUL1_CON0:
	case AFE_VUL3_CON0:
	case AFE_VUL4_CON0:
	case AFE_VUL5_CON0:
	case AFE_VUL9_CON0:
	case AFE_VUL25_CON0:
	case AFE_IRQ0_MCU_CFG0:
	case AFE_IRQ1_MCU_CFG0:
	case AFE_IRQ2_MCU_CFG0:
	case AFE_IRQ3_MCU_CFG0:
	case AFE_IRQ4_MCU_CFG0:
	case AFE_IRQ5_MCU_CFG0:
	case AFE_IRQ6_MCU_CFG0:
	case AFE_IRQ7_MCU_CFG0:
	case AFE_IRQ8_MCU_CFG0:
	case AFE_IRQ9_MCU_CFG0:
	case AFE_IRQ10_MCU_CFG0:
	case AFE_IRQ11_MCU_CFG0:
	case AFE_IRQ12_MCU_CFG0:
	case AFE_IRQ13_MCU_CFG0:
	case AFE_IRQ14_MCU_CFG0:
	case AFE_IRQ15_MCU_CFG0:
	case AFE_IRQ16_MCU_CFG0:
	case AFE_IRQ17_MCU_CFG0:
	case AFE_IRQ18_MCU_CFG0:
	case AFE_IRQ19_MCU_CFG0:
	case AFE_IRQ20_MCU_CFG0:
	case AFE_IRQ21_MCU_CFG0:
	case AFE_IRQ22_MCU_CFG0:
	case AFE_IRQ23_MCU_CFG0:
	case AFE_IRQ24_MCU_CFG0:
	case AFE_IRQ25_MCU_CFG0:
	case AFE_IRQ26_MCU_CFG0:
	case AFE_CUSTOM_IRQ0_MCU_CFG0:
	case AFE_IRQ0_MCU_CFG1:
	case AFE_IRQ1_MCU_CFG1:
	case AFE_IRQ2_MCU_CFG1:
	case AFE_IRQ3_MCU_CFG1:
	case AFE_IRQ4_MCU_CFG1:
	case AFE_IRQ5_MCU_CFG1:
	case AFE_IRQ6_MCU_CFG1:
	case AFE_IRQ7_MCU_CFG1:
	case AFE_IRQ8_MCU_CFG1:
	case AFE_IRQ9_MCU_CFG1:
	case AFE_IRQ10_MCU_CFG1:
	case AFE_IRQ11_MCU_CFG1:
	case AFE_IRQ12_MCU_CFG1:
	case AFE_IRQ13_MCU_CFG1:
	case AFE_IRQ14_MCU_CFG1:
	case AFE_IRQ15_MCU_CFG1:
	case AFE_IRQ16_MCU_CFG1:
	case AFE_IRQ17_MCU_CFG1:
	case AFE_IRQ18_MCU_CFG1:
	case AFE_IRQ19_MCU_CFG1:
	case AFE_IRQ20_MCU_CFG1:
	case AFE_IRQ21_MCU_CFG1:
	case AFE_IRQ22_MCU_CFG1:
	case AFE_IRQ23_MCU_CFG1:
	case AFE_IRQ24_MCU_CFG1:
	case AFE_IRQ25_MCU_CFG1:
	case AFE_IRQ26_MCU_CFG1:
	case AFE_CUSTOM_IRQ0_MCU_CFG1:
	/* for vow using */
	case AFE_IRQ_MCU_SCP_EN:
	case AFE_VUL_CM0_BASE_MSB:
	case AFE_VUL_CM0_BASE:
	case AFE_VUL_CM0_END_MSB:
	case AFE_VUL_CM0_END:
	case AFE_VUL_CM0_CON0:
		return true;
	default:
		return false;
	};
}

static const struct regmap_config mt8189_afe_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,

	.volatile_reg = mt8189_is_volatile_reg,

	.max_register = AFE_MAX_REGISTER,
	.num_reg_defaults_raw = AFE_MAX_REGISTER,

	.cache_type = REGCACHE_FLAT,
};

static irqreturn_t mt8189_afe_irq_handler(int irq_id, void *dev)
{
	struct mtk_base_afe *afe = dev;
	struct mtk_base_afe_irq *irq;
	unsigned int status = 0;
	unsigned int status_mcu;
	unsigned int mcu_en = 0;
	unsigned int cus_status = 0;
	unsigned int cus_status_mcu;
	unsigned int cus_mcu_en = 0;
	unsigned int tmp_reg = 0;
	int ret, cus_ret;
	int i;
	struct timespec64 ts64;
	unsigned long long t1, t2;
	/* one interrupt period = 5ms */
	unsigned long long timeout_limit = 5000000;

	/* get irq that is sent to MCU */
	regmap_read(afe->regmap, AFE_IRQ_MCU_EN, &mcu_en);
	regmap_read(afe->regmap, AFE_CUSTOM_IRQ_MCU_EN, &cus_mcu_en);

	ret = regmap_read(afe->regmap, AFE_IRQ_MCU_STATUS, &status);
	cus_ret = regmap_read(afe->regmap, AFE_CUSTOM_IRQ_MCU_STATUS, &cus_status);
	/* only care IRQ which is sent to MCU */
	status_mcu = status & mcu_en & AFE_IRQ_STATUS_BITS;
	cus_status_mcu = cus_status & cus_mcu_en & AFE_IRQ_STATUS_BITS;
	if ((ret || (status_mcu == 0)) &&
	    (cus_ret || (cus_status_mcu == 0))) {
		dev_info(afe->dev, "%s(), irq status err, ret %d, status 0x%x, mcu_en 0x%x\n",
			 __func__, ret, status, mcu_en);
		dev_info(afe->dev, "%s(), irq status err, ret %d, cus_status_mcu 0x%x, cus_mcu_en 0x%x\n",
			 __func__, ret, cus_status_mcu, cus_mcu_en);

		goto err_irq;
	}

	ktime_get_ts64(&ts64);
	t1 = timespec64_to_ns(&ts64);

	for (i = 0; i < MT8189_MEMIF_NUM; i++) {
		struct mtk_base_afe_memif *memif = &afe->memif[i];

		if (!memif->substream)
			continue;

		if (memif->irq_usage < 0)
			continue;
		irq = &afe->irqs[memif->irq_usage];

		if (i == MT8189_MEMIF_HDMI) {
			if (cus_status_mcu & (0x1 << irq->irq_data->id))
				snd_pcm_period_elapsed(memif->substream);
		} else {
			if (status_mcu & (0x1 << irq->irq_data->id))
				snd_pcm_period_elapsed(memif->substream);
		}
	}

	ktime_get_ts64(&ts64);
	t2 = timespec64_to_ns(&ts64);
	t2 = t2 - t1; /* in ns (10^9) */

	if (t2 > timeout_limit) {
		dev_info(afe->dev, "%s(), mcu_en 0x%x, cus_mcu_en 0x%x, timeout %llu, limit %llu, ret %d\n",
			__func__, mcu_en, cus_mcu_en,
			t2, timeout_limit, ret);
	}

err_irq:
	/* clear irq */
	for (i = 0; i < MT8189_IRQ_NUM; ++i) {
		/* cus_status_mcu only bit0 is used for TDM */
		if ((status_mcu & (0x1 << i)) || (cus_status_mcu & 0x1)) {
			regmap_read(afe->regmap, irq_data[i].irq_clr_reg, &tmp_reg);
			regmap_update_bits(afe->regmap, irq_data[i].irq_clr_reg,
					   AFE_IRQ_CLR_CFG_MASK_SFT |
					   AFE_IRQ_MISS_FLAG_CLR_CFG_MASK_SFT,
					   tmp_reg ^ (AFE_IRQ_CLR_CFG_MASK_SFT |
					   AFE_IRQ_MISS_FLAG_CLR_CFG_MASK_SFT));
		}
	}

	return IRQ_HANDLED;
}

static int mt8189_afe_runtime_suspend(struct device *dev)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);
	unsigned int value = 0;
	unsigned int tmp_reg = 0;
	int ret, i;

	dev_info(afe->dev, "%s() successfully start\n", __func__);

	if (!afe->regmap) {
		dev_info(afe->dev, "%s() skip regmap\n", __func__);
		goto skip_regmap;
	}

	/* Add to be off for free run*/
	/* disable AFE */
	regmap_update_bits(afe->regmap, AUDIO_ENGEN_CON0, 0x1, 0x0);

	ret = regmap_read_poll_timeout(afe->regmap,
				       AUDIO_ENGEN_CON0_MON,
				       value,
				       (value & AUDIO_ENGEN_MON_SFT) == 0,
				       20,
				       1 * 1000 * 1000);
	dev_dbg(afe->dev, "%s() read_poll ret %d\n", __func__, ret);
	if (ret)
		dev_info(afe->dev, "%s(), ret %d\n", __func__, ret);

	/* make sure all irq status are cleared */
	for (i = 0; i < MT8189_IRQ_NUM; i++) {
		regmap_read(afe->regmap, irq_data[i].irq_clr_reg, &tmp_reg);
		regmap_update_bits(afe->regmap, irq_data[i].irq_clr_reg,
				AFE_IRQ_CLR_CFG_MASK_SFT |
				AFE_IRQ_MISS_FLAG_CLR_CFG_MASK_SFT,
				tmp_reg ^ (AFE_IRQ_CLR_CFG_MASK_SFT |
				AFE_IRQ_MISS_FLAG_CLR_CFG_MASK_SFT));
	}

	/* reset sgen */
	regmap_write(afe->regmap, AFE_SINEGEN_CON0, 0x0);
	regmap_update_bits(afe->regmap, AFE_SINEGEN_CON1,
			   SINE_DOMAIN_MASK_SFT,
			   0x0 << SINE_DOMAIN_SFT);
	regmap_update_bits(afe->regmap, AFE_SINEGEN_CON1,
			   SINE_MODE_MASK_SFT,
			   0x0 << SINE_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_SINEGEN_CON1,
			   INNER_LOOP_BACKI_SEL_MASK_SFT,
			   0x0 << INNER_LOOP_BACKI_SEL_SFT);
	regmap_update_bits(afe->regmap, AFE_SINEGEN_CON1,
			   INNER_LOOP_BACK_MODE_MASK_SFT,
			   0xff << INNER_LOOP_BACK_MODE_SFT);

	regmap_write(afe->regmap, AUDIO_TOP_CON4, 0x3fff);

	/* cache only */
	regcache_cache_only(afe->regmap, true);
	regcache_mark_dirty(afe->regmap);

skip_regmap:
	mt8189_afe_disable_clock(afe);
	return 0;
}

static int mt8189_afe_runtime_resume(struct device *dev)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);
	int ret;

	dev_info(afe->dev, "%s() successfully start\n", __func__);

	ret = mt8189_afe_enable_clock(afe);
	if (ret)
		return ret;

	if (!afe->regmap) {
		dev_info(afe->dev, "%s() skip regmap\n", __func__);
		goto skip_regmap;
	}

	regcache_cache_only(afe->regmap, false);
	regcache_sync(afe->regmap);
	/* IPM2.0: Clear AUDIO_TOP_CON4 for enabling AP side module clk */
	regmap_write(afe->regmap, AUDIO_TOP_CON4, 0x0);

	/* Add to be on for free run */
	regmap_write(afe->regmap, AUDIO_TOP_CON0, 0x0);
	regmap_write(afe->regmap, AUDIO_TOP_CON1, 0x0);
	regmap_write(afe->regmap, AUDIO_TOP_CON2, 0x0);
	regmap_write(afe->regmap, AUDIO_TOP_CON3, 0x0);
	/* Can't set AUDIO_TOP_CON3 to be 0x0, it will hang in FPGA env */

	regmap_write(afe->regmap, AUDIO_TOP_CON3, 0x0);

	regmap_update_bits(afe->regmap, AFE_CBIP_CFG0, 0x1, 0x1);

	/* force cpu use 8_24 format when writing 32bit data */
	regmap_update_bits(afe->regmap, AFE_MEMIF_CON0,
			   CPU_HD_ALIGN_MASK_SFT, 0 << CPU_HD_ALIGN_SFT);


	/* enable AFE */
	regmap_update_bits(afe->regmap, AUDIO_ENGEN_CON0, 0x1, 0x1);

skip_regmap:
	return 0;
}

static u32 copy_from_buffer_request(void *dest, size_t destsize, const void *src,
				    size_t srcsize, u32 offset, size_t request)
{
	/* if request == -1, offset == 0, copy full srcsize */
	if (offset + request > srcsize)
		request = srcsize - offset;

	/* if destsize == -1, don't check the request size */
	if (!dest || destsize < request) {
		pr_info("%s, buffer null or not enough space", __func__);
		return 0;
	}

	memcpy(dest, src + offset, request);
	return request;
}

/*
 * sysfs bin_attribute node
 */

static ssize_t afe_sysfs_debug_read(struct file *filep, struct kobject *kobj,
				    struct bin_attribute *attr,
				    char *buf, loff_t offset, size_t size)
{
	size_t read_size, ceil_size, page_mask;
	ssize_t ret;
	struct mtk_base_afe *afe = (struct mtk_base_afe *)attr->private;
	char *buffer = NULL; /* for reduce kernel stack */

	buffer = kmalloc(AFE_SYS_DEBUG_SIZE, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	// sys fs op align with page size
	read_size = mt8189_debug_read_reg(buffer, AFE_SYS_DEBUG_SIZE, afe);
	page_mask = ~(PAGE_SIZE-1);
	ceil_size = (read_size&page_mask) + PAGE_SIZE;

	ret = copy_from_buffer_request(buf, -1, buffer, ceil_size, offset, size);
	kfree(buffer);

	return ret;
}

/*
 * sysfs bin_attribute node
 */
static ssize_t afe_sysfs_debug_write(struct file *filep, struct kobject *kobj,
				     struct bin_attribute *attr,
				     char *buf, loff_t offset, size_t size)
{
	struct mtk_base_afe *afe = (struct mtk_base_afe *)attr->private;

	char input[MAX_DEBUG_WRITE_INPUT];
	char *temp, *command, *str_begin;
	char delim[] = " ,";

	if (!size) {
		dev_info(afe->dev, "%s(), count is 0, return directly\n",
			 __func__);
		goto exit;
	}

	if (size >= MAX_DEBUG_WRITE_INPUT)
		size = MAX_DEBUG_WRITE_INPUT - 1;

	memset((void *)input, 0, MAX_DEBUG_WRITE_INPUT);
	memcpy(input, buf, size);
	input[(int)size] = '\0';

	str_begin = kstrndup(input, MAX_DEBUG_WRITE_INPUT - 1,
			     GFP_KERNEL);

	if (!str_begin) {
		dev_info(afe->dev, "%s(), kstrdup fail\n", __func__);
		goto exit;
	}
	temp = str_begin;

	command = strsep(&temp, delim);

	if (strcmp("write_reg", command) == 0)
		mtk_afe_write_reg(afe, (void *)temp);
exit:

	return size;
}

struct bin_attribute bin_attr_afe_dump = {
	.attr = {
		.name = "mtk_afe_node",
		.mode = 0444,
	},
	.size = AFE_SYS_DEBUG_SIZE,
	.read = afe_sysfs_debug_read,
	.write = afe_sysfs_debug_write,
};

static struct bin_attribute *afe_bin_attrs[] = {
	&bin_attr_afe_dump,
	NULL,
};

struct attribute_group afe_bin_attr_group = {
	.name = "mtk_afe_attrs",
	.bin_attrs = afe_bin_attrs,
};


static int mt8189_afe_component_probe(struct snd_soc_component *component)
{
	struct mtk_base_afe *afe = NULL;
	struct snd_soc_card *sndcard = NULL;
	struct snd_card *card = NULL;
	int ret = 0;

	if (component) {
		afe = snd_soc_component_get_drvdata(component);
		sndcard = component->card;
		card = sndcard->snd_card;

		mtk_afe_add_sub_dai_control(component);

		bin_attr_afe_dump.private = (void *)afe;
		ret = snd_card_add_dev_attr(card, &afe_bin_attr_group);
		if (ret)
			pr_info("snd_card_add_dev_attr fail\n");
	}

	return 0;
}

static const struct snd_soc_component_driver mt8189_afe_component = {
	.name = AFE_PCM_NAME,
	.probe = mt8189_afe_component_probe,
	.pcm_construct = mtk_afe_pcm_new,
	.pcm_destruct = mtk_afe_pcm_free,
	.open = mtk_afe_pcm_open,
	.pointer = mtk_afe_pcm_pointer,
	.copy = mtk_afe_pcm_copy_user,
};

static ssize_t mt8189_debug_read_reg(char *buffer, int size, struct mtk_base_afe *afe)
{
	int n = 0, i = 0;
	unsigned int value;
	struct mt8189_afe_private *afe_priv = afe->platform_priv;

	if (!buffer)
		return -ENOMEM;

	n += scnprintf(buffer + n, size - n,
		       "mtkaif calibration phase %d, %d\n",
		       afe_priv->mtkaif_chosen_phase[0],
		       afe_priv->mtkaif_chosen_phase[1]);

	n += scnprintf(buffer + n, size - n,
		       "mtkaif calibration cycle %d, %d\n",
		       afe_priv->mtkaif_phase_cycle[0],
		       afe_priv->mtkaif_phase_cycle[1]);

	for (i = 0; i < afe->memif_size; i++) {
		n += scnprintf(buffer + n, size - n,
			       "memif[%d], irq_usage %d\n",
			       i, afe->memif[i].irq_usage);
	}
	regmap_read(afe_priv->topckgen, CLK_CFG_7, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 CLK_CFG_7 = 0x%x\n", CLK_CFG_7, value);
	regmap_read(afe_priv->topckgen, CLK_CFG_9, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 CLK_CFG_9 = 0x%x\n", CLK_CFG_9, value);
	regmap_read(afe_priv->topckgen, CLK_CFG_10, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 CLK_CFG_10 = 0x%x\n", CLK_CFG_10, value);
	regmap_read(afe_priv->topckgen, CLK_CFG_11, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 CLK_CFG_11 = 0x%x\n", CLK_CFG_11, value);
	regmap_read(afe_priv->topckgen, CLK_CFG_12, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 CLK_CFG_12 = 0x%x\n", CLK_CFG_12, value);
	regmap_read(afe_priv->topckgen, CLK_CFG_UPDATE, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 CLK_CFG_UPDATE = 0x%x\n", CLK_CFG_UPDATE, value);
	regmap_read(afe_priv->topckgen, CLK_CFG_UPDATE1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 CLK_CFG_UPDATE1 = 0x%x\n", CLK_CFG_UPDATE1, value);
	regmap_read(afe_priv->topckgen, CLK_AUDDIV_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 CLK_AUDDIV_0 = 0x%x\n", CLK_AUDDIV_0, value);
	regmap_read(afe_priv->topckgen, CLK_AUDDIV_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 CLK_AUDDIV_2 = 0x%x\n", CLK_AUDDIV_2, value);

	regmap_read(afe_priv->topckgen, CLK_AUDDIV_5, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 CLK_AUDDIV_5 = 0x%x\n", CLK_AUDDIV_5, value);

	regmap_read(afe_priv->apmixed, AP_PLL_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AP_PLL_CON3 = 0x%x\n", AP_PLL_CON3, value);
	regmap_read(afe_priv->apmixed, PLLEN_ALL, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 PLLEN_ALL = 0x%x\n", PLLEN_ALL, value);
	regmap_read(afe_priv->apmixed, APLL1_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 APLL1_CON1 = 0x%x\n", APLL1_CON1, value);
	regmap_read(afe_priv->apmixed, APLL1_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 APLL1_CON2 = 0x%x\n", APLL1_CON2, value);
	regmap_read(afe_priv->apmixed, APLL1_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 APLL1_CON4 = 0x%x\n", APLL1_CON4, value);
	regmap_read(afe_priv->apmixed, APLL2_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 APLL2_CON1 = 0x%x\n", APLL2_CON1, value);
	regmap_read(afe_priv->apmixed, APLL2_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 APLL2_CON2 = 0x%x\n", APLL2_CON2, value);
	regmap_read(afe_priv->apmixed, APLL2_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 APLL2_CON4 = 0x%x\n", APLL2_CON4, value);
	regmap_read(afe_priv->apmixed, APLL1_TUNER_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 APLL1_TUNER_CON0 = 0x%x\n", APLL1_TUNER_CON0, value);
	regmap_read(afe_priv->apmixed, APLL2_TUNER_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 APLL2_TUNER_CON0 = 0x%x\n", APLL2_TUNER_CON0, value);

	regmap_read(afe->regmap, AUDIO_TOP_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AUDIO_TOP_CON0 = 0x%x\n", AUDIO_TOP_CON0, value);
	regmap_read(afe->regmap, AUDIO_TOP_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AUDIO_TOP_CON1 = 0x%x\n", AUDIO_TOP_CON1, value);
	regmap_read(afe->regmap, AUDIO_TOP_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AUDIO_TOP_CON2 = 0x%x\n", AUDIO_TOP_CON2, value);
	regmap_read(afe->regmap, AUDIO_TOP_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AUDIO_TOP_CON3 = 0x%x\n", AUDIO_TOP_CON3, value);
	regmap_read(afe->regmap, AUDIO_TOP_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AUDIO_TOP_CON4 = 0x%x\n", AUDIO_TOP_CON4, value);
	regmap_read(afe->regmap, AUDIO_ENGEN_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AUDIO_ENGEN_CON0 = 0x%x\n", AUDIO_ENGEN_CON0, value);
	regmap_read(afe->regmap, AUDIO_ENGEN_CON0_USER1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AUDIO_ENGEN_CON0_USER1 = 0x%x\n", AUDIO_ENGEN_CON0_USER1, value);
	regmap_read(afe->regmap, AUDIO_ENGEN_CON0_USER2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AUDIO_ENGEN_CON0_USER2 = 0x%x\n", AUDIO_ENGEN_CON0_USER2, value);
	regmap_read(afe->regmap, AFE_SINEGEN_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_SINEGEN_CON0 = 0x%x\n", AFE_SINEGEN_CON0, value);
	regmap_read(afe->regmap, AFE_SINEGEN_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_SINEGEN_CON1 = 0x%x\n", AFE_SINEGEN_CON1, value);
	regmap_read(afe->regmap, AFE_SINEGEN_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_SINEGEN_CON2 = 0x%x\n", AFE_SINEGEN_CON2, value);
	regmap_read(afe->regmap, AFE_SINEGEN_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_SINEGEN_CON3 = 0x%x\n", AFE_SINEGEN_CON3, value);
	regmap_read(afe->regmap, AFE_APLL1_TUNER_CFG, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_APLL1_TUNER_CFG = 0x%x\n", AFE_APLL1_TUNER_CFG, value);
	regmap_read(afe->regmap, AFE_APLL1_TUNER_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_APLL1_TUNER_MON0 = 0x%x\n", AFE_APLL1_TUNER_MON0, value);
	regmap_read(afe->regmap, AFE_APLL2_TUNER_CFG, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_APLL2_TUNER_CFG = 0x%x\n", AFE_APLL2_TUNER_CFG, value);
	regmap_read(afe->regmap, AFE_APLL2_TUNER_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_APLL2_TUNER_MON0 = 0x%x\n", AFE_APLL2_TUNER_MON0, value);
	regmap_read(afe->regmap, AUDIO_TOP_RG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AUDIO_TOP_RG0 = 0x%x\n", AUDIO_TOP_RG0, value);
	regmap_read(afe->regmap, AUDIO_TOP_RG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AUDIO_TOP_RG1 = 0x%x\n", AUDIO_TOP_RG1, value);
	regmap_read(afe->regmap, AUDIO_TOP_RG2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AUDIO_TOP_RG2 = 0x%x\n", AUDIO_TOP_RG2, value);
	regmap_read(afe->regmap, AUDIO_TOP_RG3, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AUDIO_TOP_RG3 = 0x%x\n", AUDIO_TOP_RG3, value);
	regmap_read(afe->regmap, AUDIO_TOP_RG4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AUDIO_TOP_RG4 = 0x%x\n", AUDIO_TOP_RG4, value);
	regmap_read(afe->regmap, AFE_SPM_CONTROL_REQ, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_SPM_CONTROL_REQ = 0x%x\n", AFE_SPM_CONTROL_REQ, value);
	regmap_read(afe->regmap, AFE_SPM_CONTROL_ACK, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_SPM_CONTROL_ACK = 0x%x\n", AFE_SPM_CONTROL_ACK, value);
	regmap_read(afe->regmap, AUDIO_TOP_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AUDIO_TOP_IP_VERSION = 0x%x\n", AUDIO_TOP_IP_VERSION, value);
	regmap_read(afe->regmap, AUDIO_ENGEN_CON0_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AUDIO_ENGEN_CON0_MON = 0x%x\n", AUDIO_ENGEN_CON0_MON, value);
	regmap_read(afe->regmap, AUDIO_USE_DEFAULT_DELSEL0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AUDIO_USE_DEFAULT_DELSEL0 = 0x%x\n",
		       AUDIO_USE_DEFAULT_DELSEL0, value);
	regmap_read(afe->regmap, AUDIO_USE_DEFAULT_DELSEL1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AUDIO_USE_DEFAULT_DELSEL1 = 0x%x\n",
		       AUDIO_USE_DEFAULT_DELSEL1, value);
	regmap_read(afe->regmap, AUDIO_USE_DEFAULT_DELSEL2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AUDIO_USE_DEFAULT_DELSEL2 = 0x%x\n",
		       AUDIO_USE_DEFAULT_DELSEL2, value);
	regmap_read(afe->regmap, AFE_CONNSYS_I2S_IPM_VER_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_CONNSYS_I2S_IPM_VER_MON = 0x%x\n",
		       AFE_CONNSYS_I2S_IPM_VER_MON, value);
	regmap_read(afe->regmap, AFE_CONNSYS_I2S_MON_SEL, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_CONNSYS_I2S_MON_SEL = 0x%x\n",
		       AFE_CONNSYS_I2S_MON_SEL, value);
	regmap_read(afe->regmap, AFE_CONNSYS_I2S_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_CONNSYS_I2S_MON = 0x%x\n", AFE_CONNSYS_I2S_MON, value);
	regmap_read(afe->regmap, AFE_CONNSYS_I2S_CON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_CONNSYS_I2S_CON = 0x%x\n", AFE_CONNSYS_I2S_CON, value);
	regmap_read(afe->regmap, AFE_PCM0_INTF_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_PCM0_INTF_CON0 = 0x%x\n", AFE_PCM0_INTF_CON0, value);
	regmap_read(afe->regmap, AFE_PCM0_INTF_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_PCM0_INTF_CON1 = 0x%x\n", AFE_PCM0_INTF_CON1, value);
	regmap_read(afe->regmap, AFE_PCM_INTF_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_PCM_INTF_MON = 0x%x\n", AFE_PCM_INTF_MON, value);
	regmap_read(afe->regmap, AFE_PCM_TOP_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_PCM_TOP_IP_VERSION = 0x%x\n", AFE_PCM_TOP_IP_VERSION, value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_EN, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ_MCU_EN = 0x%x\n", AFE_IRQ_MCU_EN, value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_DSP_EN, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ_MCU_DSP_EN = 0x%x\n", AFE_IRQ_MCU_DSP_EN, value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_DSP2_EN, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ_MCU_DSP2_EN = 0x%x\n", AFE_IRQ_MCU_DSP2_EN, value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_SCP_EN, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ_MCU_SCP_EN = 0x%x\n", AFE_IRQ_MCU_SCP_EN, value);
	regmap_read(afe->regmap, AFE_CUSTOM_IRQ_MCU_EN, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_CUSTOM_IRQ_MCU_EN = 0x%x\n", AFE_CUSTOM_IRQ_MCU_EN, value);
	regmap_read(afe->regmap, AFE_CUSTOM_IRQ_MCU_DSP_EN, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_CUSTOM_IRQ_MCU_DSP_EN = 0x%x\n",
		       AFE_CUSTOM_IRQ_MCU_DSP_EN, value);
	regmap_read(afe->regmap, AFE_CUSTOM_IRQ_MCU_DSP2_EN, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_CUSTOM_IRQ_MCU_DSP2_EN = 0x%x\n",
		       AFE_CUSTOM_IRQ_MCU_DSP2_EN, value);
	regmap_read(afe->regmap, AFE_CUSTOM_IRQ_MCU_SCP_EN, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_CUSTOM_IRQ_MCU_SCP_EN = 0x%x\n",
		       AFE_CUSTOM_IRQ_MCU_SCP_EN, value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_STATUS, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ_MCU_STATUS = 0x%x\n", AFE_IRQ_MCU_STATUS, value);
	regmap_read(afe->regmap, AFE_CUSTOM_IRQ_MCU_STATUS, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_CUSTOM_IRQ_MCU_STATUS = 0x%x\n",
		       AFE_CUSTOM_IRQ_MCU_STATUS, value);
	regmap_read(afe->regmap, AFE_IRQ0_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ0_MCU_CFG0 = 0x%x\n", AFE_IRQ0_MCU_CFG0, value);
	regmap_read(afe->regmap, AFE_IRQ0_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ0_MCU_CFG1 = 0x%x\n", AFE_IRQ0_MCU_CFG1, value);
	regmap_read(afe->regmap, AFE_IRQ1_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ1_MCU_CFG0 = 0x%x\n", AFE_IRQ1_MCU_CFG0, value);
	regmap_read(afe->regmap, AFE_IRQ1_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ1_MCU_CFG1 = 0x%x\n", AFE_IRQ1_MCU_CFG1, value);
	regmap_read(afe->regmap, AFE_IRQ2_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ2_MCU_CFG0 = 0x%x\n", AFE_IRQ2_MCU_CFG0, value);
	regmap_read(afe->regmap, AFE_IRQ2_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ2_MCU_CFG1 = 0x%x\n", AFE_IRQ2_MCU_CFG1, value);
	regmap_read(afe->regmap, AFE_IRQ3_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ3_MCU_CFG0 = 0x%x\n", AFE_IRQ3_MCU_CFG0, value);
	regmap_read(afe->regmap, AFE_IRQ3_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ3_MCU_CFG1 = 0x%x\n", AFE_IRQ3_MCU_CFG1, value);
	regmap_read(afe->regmap, AFE_IRQ4_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ4_MCU_CFG0 = 0x%x\n", AFE_IRQ4_MCU_CFG0, value);
	regmap_read(afe->regmap, AFE_IRQ4_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ4_MCU_CFG1 = 0x%x\n", AFE_IRQ4_MCU_CFG1, value);
	regmap_read(afe->regmap, AFE_IRQ5_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ5_MCU_CFG0 = 0x%x\n", AFE_IRQ5_MCU_CFG0, value);
	regmap_read(afe->regmap, AFE_IRQ5_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ5_MCU_CFG1 = 0x%x\n", AFE_IRQ5_MCU_CFG1, value);
	regmap_read(afe->regmap, AFE_IRQ6_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ6_MCU_CFG0 = 0x%x\n", AFE_IRQ6_MCU_CFG0, value);
	regmap_read(afe->regmap, AFE_IRQ6_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ6_MCU_CFG1 = 0x%x\n", AFE_IRQ6_MCU_CFG1, value);
	regmap_read(afe->regmap, AFE_IRQ7_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ7_MCU_CFG0 = 0x%x\n", AFE_IRQ7_MCU_CFG0, value);
	regmap_read(afe->regmap, AFE_IRQ7_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ7_MCU_CFG1 = 0x%x\n", AFE_IRQ7_MCU_CFG1, value);
	regmap_read(afe->regmap, AFE_IRQ8_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ8_MCU_CFG0 = 0x%x\n", AFE_IRQ8_MCU_CFG0, value);
	regmap_read(afe->regmap, AFE_IRQ8_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ8_MCU_CFG1 = 0x%x\n", AFE_IRQ8_MCU_CFG1, value);
	regmap_read(afe->regmap, AFE_IRQ9_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ9_MCU_CFG0 = 0x%x\n", AFE_IRQ9_MCU_CFG0, value);
	regmap_read(afe->regmap, AFE_IRQ9_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ9_MCU_CFG1 = 0x%x\n", AFE_IRQ9_MCU_CFG1, value);
	regmap_read(afe->regmap, AFE_IRQ10_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ10_MCU_CFG0 = 0x%x\n", AFE_IRQ10_MCU_CFG0, value);
	regmap_read(afe->regmap, AFE_IRQ10_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ10_MCU_CFG1 = 0x%x\n", AFE_IRQ10_MCU_CFG1, value);
	regmap_read(afe->regmap, AFE_IRQ11_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ11_MCU_CFG0 = 0x%x\n", AFE_IRQ11_MCU_CFG0, value);
	regmap_read(afe->regmap, AFE_IRQ11_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ11_MCU_CFG1 = 0x%x\n", AFE_IRQ11_MCU_CFG1, value);
	regmap_read(afe->regmap, AFE_IRQ12_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ12_MCU_CFG0 = 0x%x\n", AFE_IRQ12_MCU_CFG0, value);
	regmap_read(afe->regmap, AFE_IRQ12_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ12_MCU_CFG1 = 0x%x\n", AFE_IRQ12_MCU_CFG1, value);
	regmap_read(afe->regmap, AFE_IRQ13_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ13_MCU_CFG0 = 0x%x\n", AFE_IRQ13_MCU_CFG0, value);
	regmap_read(afe->regmap, AFE_IRQ13_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ13_MCU_CFG1 = 0x%x\n", AFE_IRQ13_MCU_CFG1, value);
	regmap_read(afe->regmap, AFE_IRQ14_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ14_MCU_CFG0 = 0x%x\n", AFE_IRQ14_MCU_CFG0, value);
	regmap_read(afe->regmap, AFE_IRQ14_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ14_MCU_CFG1 = 0x%x\n", AFE_IRQ14_MCU_CFG1, value);
	regmap_read(afe->regmap, AFE_IRQ15_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ15_MCU_CFG0 = 0x%x\n", AFE_IRQ15_MCU_CFG0, value);
	regmap_read(afe->regmap, AFE_IRQ15_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ15_MCU_CFG1 = 0x%x\n", AFE_IRQ15_MCU_CFG1, value);
	regmap_read(afe->regmap, AFE_IRQ16_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ16_MCU_CFG0 = 0x%x\n", AFE_IRQ16_MCU_CFG0, value);
	regmap_read(afe->regmap, AFE_IRQ16_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ16_MCU_CFG1 = 0x%x\n", AFE_IRQ16_MCU_CFG1, value);
	regmap_read(afe->regmap, AFE_IRQ17_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ17_MCU_CFG0 = 0x%x\n", AFE_IRQ17_MCU_CFG0, value);
	regmap_read(afe->regmap, AFE_IRQ17_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ17_MCU_CFG1 = 0x%x\n", AFE_IRQ17_MCU_CFG1, value);
	regmap_read(afe->regmap, AFE_IRQ18_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ18_MCU_CFG0 = 0x%x\n", AFE_IRQ18_MCU_CFG0, value);
	regmap_read(afe->regmap, AFE_IRQ18_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ18_MCU_CFG1 = 0x%x\n", AFE_IRQ18_MCU_CFG1, value);
	regmap_read(afe->regmap, AFE_IRQ19_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ19_MCU_CFG0 = 0x%x\n", AFE_IRQ19_MCU_CFG0, value);
	regmap_read(afe->regmap, AFE_IRQ19_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ19_MCU_CFG1 = 0x%x\n", AFE_IRQ19_MCU_CFG1, value);
	regmap_read(afe->regmap, AFE_IRQ20_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ20_MCU_CFG0 = 0x%x\n", AFE_IRQ20_MCU_CFG0, value);
	regmap_read(afe->regmap, AFE_IRQ20_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ20_MCU_CFG1 = 0x%x\n", AFE_IRQ20_MCU_CFG1, value);
	regmap_read(afe->regmap, AFE_IRQ21_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ21_MCU_CFG0 = 0x%x\n", AFE_IRQ21_MCU_CFG0, value);
	regmap_read(afe->regmap, AFE_IRQ21_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ21_MCU_CFG1 = 0x%x\n", AFE_IRQ21_MCU_CFG1, value);
	regmap_read(afe->regmap, AFE_IRQ22_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ22_MCU_CFG0 = 0x%x\n", AFE_IRQ22_MCU_CFG0, value);
	regmap_read(afe->regmap, AFE_IRQ22_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ22_MCU_CFG1 = 0x%x\n", AFE_IRQ22_MCU_CFG1, value);
	regmap_read(afe->regmap, AFE_IRQ23_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ23_MCU_CFG0 = 0x%x\n", AFE_IRQ23_MCU_CFG0, value);
	regmap_read(afe->regmap, AFE_IRQ23_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ23_MCU_CFG1 = 0x%x\n", AFE_IRQ23_MCU_CFG1, value);
	regmap_read(afe->regmap, AFE_IRQ24_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ24_MCU_CFG0 = 0x%x\n", AFE_IRQ24_MCU_CFG0, value);
	regmap_read(afe->regmap, AFE_IRQ24_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ24_MCU_CFG1 = 0x%x\n", AFE_IRQ24_MCU_CFG1, value);
	regmap_read(afe->regmap, AFE_IRQ25_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ25_MCU_CFG0 = 0x%x\n", AFE_IRQ25_MCU_CFG0, value);
	regmap_read(afe->regmap, AFE_IRQ25_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ25_MCU_CFG1 = 0x%x\n", AFE_IRQ25_MCU_CFG1, value);
	regmap_read(afe->regmap, AFE_IRQ26_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ26_MCU_CFG0 = 0x%x\n", AFE_IRQ26_MCU_CFG0, value);
	regmap_read(afe->regmap, AFE_IRQ26_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ26_MCU_CFG1 = 0x%x\n", AFE_IRQ26_MCU_CFG1, value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ_MCU_MON0 = 0x%x\n", AFE_IRQ_MCU_MON0, value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_MON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ_MCU_MON1 = 0x%x\n", AFE_IRQ_MCU_MON1, value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_MON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ_MCU_MON2 = 0x%x\n", AFE_IRQ_MCU_MON2, value);
	regmap_read(afe->regmap, AFE_IRQ0_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ0_CNT_MON = 0x%x\n", AFE_IRQ0_CNT_MON, value);
	regmap_read(afe->regmap, AFE_IRQ1_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ1_CNT_MON = 0x%x\n", AFE_IRQ1_CNT_MON, value);
	regmap_read(afe->regmap, AFE_IRQ2_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ2_CNT_MON = 0x%x\n", AFE_IRQ2_CNT_MON, value);
	regmap_read(afe->regmap, AFE_IRQ3_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ3_CNT_MON = 0x%x\n", AFE_IRQ3_CNT_MON, value);
	regmap_read(afe->regmap, AFE_IRQ4_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ4_CNT_MON = 0x%x\n", AFE_IRQ4_CNT_MON, value);
	regmap_read(afe->regmap, AFE_IRQ5_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ5_CNT_MON = 0x%x\n", AFE_IRQ5_CNT_MON, value);
	regmap_read(afe->regmap, AFE_IRQ6_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ6_CNT_MON = 0x%x\n", AFE_IRQ6_CNT_MON, value);
	regmap_read(afe->regmap, AFE_IRQ7_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ7_CNT_MON = 0x%x\n", AFE_IRQ7_CNT_MON, value);
	regmap_read(afe->regmap, AFE_IRQ8_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ8_CNT_MON = 0x%x\n", AFE_IRQ8_CNT_MON, value);
	regmap_read(afe->regmap, AFE_IRQ9_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ9_CNT_MON = 0x%x\n", AFE_IRQ9_CNT_MON, value);
	regmap_read(afe->regmap, AFE_IRQ10_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ10_CNT_MON = 0x%x\n", AFE_IRQ10_CNT_MON, value);
	regmap_read(afe->regmap, AFE_IRQ11_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ11_CNT_MON = 0x%x\n", AFE_IRQ11_CNT_MON, value);
	regmap_read(afe->regmap, AFE_IRQ12_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ12_CNT_MON = 0x%x\n", AFE_IRQ12_CNT_MON, value);
	regmap_read(afe->regmap, AFE_IRQ13_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ13_CNT_MON = 0x%x\n", AFE_IRQ13_CNT_MON, value);
	regmap_read(afe->regmap, AFE_IRQ14_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ14_CNT_MON = 0x%x\n", AFE_IRQ14_CNT_MON, value);
	regmap_read(afe->regmap, AFE_IRQ15_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ15_CNT_MON = 0x%x\n", AFE_IRQ15_CNT_MON, value);
	regmap_read(afe->regmap, AFE_IRQ16_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ16_CNT_MON = 0x%x\n", AFE_IRQ16_CNT_MON, value);
	regmap_read(afe->regmap, AFE_IRQ17_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ17_CNT_MON = 0x%x\n", AFE_IRQ17_CNT_MON, value);
	regmap_read(afe->regmap, AFE_IRQ18_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ18_CNT_MON = 0x%x\n", AFE_IRQ18_CNT_MON, value);
	regmap_read(afe->regmap, AFE_IRQ19_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ19_CNT_MON = 0x%x\n", AFE_IRQ19_CNT_MON, value);
	regmap_read(afe->regmap, AFE_IRQ20_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ20_CNT_MON = 0x%x\n", AFE_IRQ20_CNT_MON, value);
	regmap_read(afe->regmap, AFE_IRQ21_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ21_CNT_MON = 0x%x\n", AFE_IRQ21_CNT_MON, value);
	regmap_read(afe->regmap, AFE_IRQ22_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ22_CNT_MON = 0x%x\n", AFE_IRQ22_CNT_MON, value);
	regmap_read(afe->regmap, AFE_IRQ23_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ23_CNT_MON = 0x%x\n", AFE_IRQ23_CNT_MON, value);
	regmap_read(afe->regmap, AFE_IRQ24_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ24_CNT_MON = 0x%x\n", AFE_IRQ24_CNT_MON, value);
	regmap_read(afe->regmap, AFE_IRQ25_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ25_CNT_MON = 0x%x\n", AFE_IRQ25_CNT_MON, value);
	regmap_read(afe->regmap, AFE_IRQ26_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_IRQ26_CNT_MON = 0x%x\n", AFE_IRQ26_CNT_MON, value);
	regmap_read(afe->regmap, AFE_GAIN0_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_GAIN0_CON0 = 0x%x\n", AFE_GAIN0_CON0, value);
	regmap_read(afe->regmap, AFE_GAIN0_CON1_R, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_GAIN0_CON1_R = 0x%x\n", AFE_GAIN0_CON1_R, value);
	regmap_read(afe->regmap, AFE_GAIN0_CON1_L, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_GAIN0_CON1_L = 0x%x\n", AFE_GAIN0_CON1_L, value);
	regmap_read(afe->regmap, AFE_GAIN0_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_GAIN0_CON2 = 0x%x\n", AFE_GAIN0_CON2, value);
	regmap_read(afe->regmap, AFE_GAIN0_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_GAIN0_CON3 = 0x%x\n", AFE_GAIN0_CON3, value);
	regmap_read(afe->regmap, AFE_GAIN0_CUR_R, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_GAIN0_CUR_R = 0x%x\n", AFE_GAIN0_CUR_R, value);
	regmap_read(afe->regmap, AFE_GAIN0_CUR_L, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_GAIN0_CUR_L = 0x%x\n", AFE_GAIN0_CUR_L, value);
	regmap_read(afe->regmap, AFE_GAIN1_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_GAIN1_CON0 = 0x%x\n", AFE_GAIN1_CON0, value);
	regmap_read(afe->regmap, AFE_GAIN1_CON1_R, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_GAIN1_CON1_R = 0x%x\n", AFE_GAIN1_CON1_R, value);
	regmap_read(afe->regmap, AFE_GAIN1_CON1_L, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_GAIN1_CON1_L = 0x%x\n", AFE_GAIN1_CON1_L, value);
	regmap_read(afe->regmap, AFE_GAIN1_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_GAIN1_CON2 = 0x%x\n", AFE_GAIN1_CON2, value);
	regmap_read(afe->regmap, AFE_GAIN1_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_GAIN1_CON3 = 0x%x\n", AFE_GAIN1_CON3, value);
	regmap_read(afe->regmap, AFE_GAIN1_CUR_R, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_GAIN1_CUR_R = 0x%x\n", AFE_GAIN1_CUR_R, value);
	regmap_read(afe->regmap, AFE_GAIN1_CUR_L, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_GAIN1_CUR_L = 0x%x\n", AFE_GAIN1_CUR_L, value);
	regmap_read(afe->regmap, AFE_GAIN2_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_GAIN2_CON0 = 0x%x\n", AFE_GAIN2_CON0, value);
	regmap_read(afe->regmap, AFE_GAIN2_CON1_R, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_GAIN2_CON1_R = 0x%x\n", AFE_GAIN2_CON1_R, value);
	regmap_read(afe->regmap, AFE_GAIN2_CON1_L, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_GAIN2_CON1_L = 0x%x\n", AFE_GAIN2_CON1_L, value);
	regmap_read(afe->regmap, AFE_GAIN2_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_GAIN2_CON2 = 0x%x\n", AFE_GAIN2_CON2, value);
	regmap_read(afe->regmap, AFE_GAIN2_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_GAIN2_CON3 = 0x%x\n", AFE_GAIN2_CON3, value);
	regmap_read(afe->regmap, AFE_GAIN2_CUR_R, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_GAIN2_CUR_R = 0x%x\n", AFE_GAIN2_CUR_R, value);
	regmap_read(afe->regmap, AFE_GAIN2_CUR_L, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_GAIN2_CUR_L = 0x%x\n", AFE_GAIN2_CUR_L, value);
	regmap_read(afe->regmap, AFE_GAIN3_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_GAIN3_CON0 = 0x%x\n", AFE_GAIN3_CON0, value);
	regmap_read(afe->regmap, AFE_GAIN3_CON1_R, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_GAIN3_CON1_R = 0x%x\n", AFE_GAIN3_CON1_R, value);
	regmap_read(afe->regmap, AFE_GAIN3_CON1_L, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_GAIN3_CON1_L = 0x%x\n", AFE_GAIN3_CON1_L, value);
	regmap_read(afe->regmap, AFE_GAIN3_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_GAIN3_CON2 = 0x%x\n", AFE_GAIN3_CON2, value);
	regmap_read(afe->regmap, AFE_GAIN3_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_GAIN3_CON3 = 0x%x\n", AFE_GAIN3_CON3, value);
	regmap_read(afe->regmap, AFE_GAIN3_CUR_R, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_GAIN3_CUR_R = 0x%x\n", AFE_GAIN3_CUR_R, value);
	regmap_read(afe->regmap, AFE_GAIN3_CUR_L, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_GAIN3_CUR_L = 0x%x\n", AFE_GAIN3_CUR_L, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_IPM_VER_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_IPM_VER_MON = 0x%x\n",
		       AFE_ADDA_DL_IPM_VER_MON, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SRC_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_SRC_CON0 = 0x%x\n",
		       AFE_ADDA_DL_SRC_CON0, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SRC_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_SRC_CON1 = 0x%x\n",
		       AFE_ADDA_DL_SRC_CON1, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SRC_DEBUG_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_SRC_DEBUG_MON0 = 0x%x\n",
		       AFE_ADDA_DL_SRC_DEBUG_MON0, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_PREDIS_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_PREDIS_CON0 = 0x%x\n",
		       AFE_ADDA_DL_PREDIS_CON0, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_PREDIS_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_PREDIS_CON1 = 0x%x\n",
		       AFE_ADDA_DL_PREDIS_CON1, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_PREDIS_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_PREDIS_CON2 = 0x%x\n",
		       AFE_ADDA_DL_PREDIS_CON2, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_PREDIS_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_PREDIS_CON3 = 0x%x\n",
		       AFE_ADDA_DL_PREDIS_CON3, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SDM_DCCOMP_CON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_SDM_DCCOMP_CON = 0x%x\n",
		       AFE_ADDA_DL_SDM_DCCOMP_CON, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SDM_TEST, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_SDM_TEST = 0x%x\n",
		       AFE_ADDA_DL_SDM_TEST, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_DC_COMP_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_DC_COMP_CFG0 = 0x%x\n",
		       AFE_ADDA_DL_DC_COMP_CFG0, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_DC_COMP_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_DC_COMP_CFG1 = 0x%x\n",
		       AFE_ADDA_DL_DC_COMP_CFG1, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SDM_OUT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_SDM_OUT_MON = 0x%x\n",
		       AFE_ADDA_DL_SDM_OUT_MON, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SRC_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_SRC_LCH_MON = 0x%x\n",
		       AFE_ADDA_DL_SRC_LCH_MON, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SRC_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_SRC_RCH_MON = 0x%x\n",
		       AFE_ADDA_DL_SRC_RCH_MON, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SRC_DEBUG, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_SRC_DEBUG = 0x%x\n",
		       AFE_ADDA_DL_SRC_DEBUG, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SDM_DITHER_CON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_SDM_DITHER_CON = 0x%x\n",
		       AFE_ADDA_DL_SDM_DITHER_CON, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SDM_AUTO_RESET_CON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_SDM_AUTO_RESET_CON = 0x%x\n",
		       AFE_ADDA_DL_SDM_AUTO_RESET_CON, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_HBF1_SCF1_CONFIG = 0x%x\n",
		       AFE_ADDA_DL_HBF1_SCF1_CONFIG, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP1_TAP2_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_HBF1_SCF1_TAP1_TAP2_CONFIG = 0x%x\n",
		       AFE_ADDA_DL_HBF1_SCF1_TAP1_TAP2_CONFIG, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP3_TAP4_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_HBF1_SCF1_TAP3_TAP4_CONFIG = 0x%x\n",
		       AFE_ADDA_DL_HBF1_SCF1_TAP3_TAP4_CONFIG, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP5_TAP6_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_HBF1_SCF1_TAP5_TAP6_CONFIG = 0x%x\n",
		       AFE_ADDA_DL_HBF1_SCF1_TAP5_TAP6_CONFIG, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP7_TAP8_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_HBF1_SCF1_TAP7_TAP8_CONFIG = 0x%x\n",
		       AFE_ADDA_DL_HBF1_SCF1_TAP7_TAP8_CONFIG, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP9_TAP10_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_HBF1_SCF1_TAP9_TAP10_CONFIG = 0x%x\n",
		       AFE_ADDA_DL_HBF1_SCF1_TAP9_TAP10_CONFIG, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP11_TAP12_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_HBF1_SCF1_TAP11_TAP12_CONFIG = 0x%x\n",
		       AFE_ADDA_DL_HBF1_SCF1_TAP11_TAP12_CONFIG, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP13_TAP14_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_HBF1_SCF1_TAP13_TAP14_CONFIG = 0x%x\n",
		       AFE_ADDA_DL_HBF1_SCF1_TAP13_TAP14_CONFIG, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP15_TAP16_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_HBF1_SCF1_TAP15_TAP16_CONFIG = 0x%x\n",
		       AFE_ADDA_DL_HBF1_SCF1_TAP15_TAP16_CONFIG, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP17_TAP18_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_HBF1_SCF1_TAP17_TAP18_CONFIG = 0x%x\n",
		       AFE_ADDA_DL_HBF1_SCF1_TAP17_TAP18_CONFIG, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP19_TAP20_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_HBF1_SCF1_TAP19_TAP20_CONFIG = 0x%x\n",
		       AFE_ADDA_DL_HBF1_SCF1_TAP19_TAP20_CONFIG, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP21_TAP22_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_HBF1_SCF1_TAP21_TAP22_CONFIG = 0x%x\n",
		       AFE_ADDA_DL_HBF1_SCF1_TAP21_TAP22_CONFIG, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP23_TAP24_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_HBF1_SCF1_TAP23_TAP24_CONFIG = 0x%x\n",
		       AFE_ADDA_DL_HBF1_SCF1_TAP23_TAP24_CONFIG, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP25_TAP26_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_HBF1_SCF1_TAP25_TAP26_CONFIG = 0x%x\n",
		       AFE_ADDA_DL_HBF1_SCF1_TAP25_TAP26_CONFIG, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP27_TAP28_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_HBF1_SCF1_TAP27_TAP28_CONFIG = 0x%x\n",
		       AFE_ADDA_DL_HBF1_SCF1_TAP27_TAP28_CONFIG, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP29_TAP30_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_HBF1_SCF1_TAP29_TAP30_CONFIG = 0x%x\n",
		       AFE_ADDA_DL_HBF1_SCF1_TAP29_TAP30_CONFIG, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP31_TAP32_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_HBF1_SCF1_TAP31_TAP32_CONFIG = 0x%x\n",
		       AFE_ADDA_DL_HBF1_SCF1_TAP31_TAP32_CONFIG, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP33_TAP34_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_HBF1_SCF1_TAP33_TAP34_CONFIG = 0x%x\n",
		       AFE_ADDA_DL_HBF1_SCF1_TAP33_TAP34_CONFIG, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP35_TAP36_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_HBF1_SCF1_TAP35_TAP36_CONFIG = 0x%x\n",
		       AFE_ADDA_DL_HBF1_SCF1_TAP35_TAP36_CONFIG, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP37_TAP38_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_HBF1_SCF1_TAP37_TAP38_CONFIG = 0x%x\n",
		       AFE_ADDA_DL_HBF1_SCF1_TAP37_TAP38_CONFIG, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP39_TAP40_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_HBF1_SCF1_TAP39_TAP40_CONFIG = 0x%x\n",
		       AFE_ADDA_DL_HBF1_SCF1_TAP39_TAP40_CONFIG, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP41_TAP42_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_HBF1_SCF1_TAP41_TAP42_CONFIG = 0x%x\n",
		       AFE_ADDA_DL_HBF1_SCF1_TAP41_TAP42_CONFIG, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP43_TAP44_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_HBF1_SCF1_TAP43_TAP44_CONFIG = 0x%x\n",
		       AFE_ADDA_DL_HBF1_SCF1_TAP43_TAP44_CONFIG, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP45_TAP46_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_HBF1_SCF1_TAP45_TAP46_CONFIG = 0x%x\n",
		       AFE_ADDA_DL_HBF1_SCF1_TAP45_TAP46_CONFIG, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP47_TAP48_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_HBF1_SCF1_TAP47_TAP48_CONFIG = 0x%x\n",
		       AFE_ADDA_DL_HBF1_SCF1_TAP47_TAP48_CONFIG, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP49_TAP50_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_HBF1_SCF1_TAP49_TAP50_CONFIG = 0x%x\n",
		       AFE_ADDA_DL_HBF1_SCF1_TAP49_TAP50_CONFIG, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP51_TAP52_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_HBF1_SCF1_TAP51_TAP52_CONFIG = 0x%x\n",
		       AFE_ADDA_DL_HBF1_SCF1_TAP51_TAP52_CONFIG, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP53_TAP54_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_HBF1_SCF1_TAP53_TAP54_CONFIG = 0x%x\n",
		       AFE_ADDA_DL_HBF1_SCF1_TAP53_TAP54_CONFIG, value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP55_TAP56_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DL_HBF1_SCF1_TAP55_TAP56_CONFIG = 0x%x\n",
		       AFE_ADDA_DL_HBF1_SCF1_TAP55_TAP56_CONFIG, value);
	regmap_read(afe->regmap, AFE_DEM_IDWA_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_DEM_IDWA_CON0 = 0x%x\n",
		       AFE_DEM_IDWA_CON0, value);
	regmap_read(afe->regmap, DEM_RECONSTRUCT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 DEM_RECONSTRUCT_MON = 0x%x\n",
		       DEM_RECONSTRUCT_MON, value);
	regmap_read(afe->regmap, AFE_CM0_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_CM0_CON0 = 0x%x\n", AFE_CM0_CON0, value);
	regmap_read(afe->regmap, AFE_CM0_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_CM0_MON = 0x%x\n", AFE_CM0_MON, value);
	regmap_read(afe->regmap, AFE_CM0_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_CM0_IP_VERSION = 0x%x\n", AFE_CM0_IP_VERSION, value);
	regmap_read(afe->regmap, AFE_CM1_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_CM1_CON0 = 0x%x\n", AFE_CM1_CON0, value);
	regmap_read(afe->regmap, AFE_CM1_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_CM1_MON = 0x%x\n", AFE_CM1_MON, value);
	regmap_read(afe->regmap, AFE_CM1_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_CM1_IP_VERSION = 0x%x\n", AFE_CM1_IP_VERSION, value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_SRC_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_UL0_SRC_CON0 = 0x%x\n", AFE_ADDA_UL0_SRC_CON0, value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_SRC_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_UL0_SRC_CON1 = 0x%x\n", AFE_ADDA_UL0_SRC_CON1, value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_SRC_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_UL0_SRC_CON2 = 0x%x\n", AFE_ADDA_UL0_SRC_CON2, value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_SRC_DEBUG, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_UL0_SRC_DEBUG = 0x%x\n", AFE_ADDA_UL0_SRC_DEBUG, value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_SRC_DEBUG_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_UL0_SRC_DEBUG_MON0 = 0x%x\n",
		       AFE_ADDA_UL0_SRC_DEBUG_MON0, value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_SRC_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_UL0_SRC_MON0 = 0x%x\n", AFE_ADDA_UL0_SRC_MON0, value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_SRC_MON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_UL0_SRC_MON1 = 0x%x\n", AFE_ADDA_UL0_SRC_MON1, value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_IIR_COEF_02_01, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_UL0_IIR_COEF_02_01 = 0x%x\n",
		       AFE_ADDA_UL0_IIR_COEF_02_01, value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_IIR_COEF_04_03, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_UL0_IIR_COEF_04_03 = 0x%x\n",
		       AFE_ADDA_UL0_IIR_COEF_04_03, value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_IIR_COEF_06_05, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_UL0_IIR_COEF_06_05 = 0x%x\n",
		       AFE_ADDA_UL0_IIR_COEF_06_05, value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_IIR_COEF_08_07, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_UL0_IIR_COEF_08_07 = 0x%x\n",
		       AFE_ADDA_UL0_IIR_COEF_08_07, value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_IIR_COEF_10_09, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_UL0_IIR_COEF_10_09 = 0x%x\n",
		       AFE_ADDA_UL0_IIR_COEF_10_09, value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_02_01, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_UL0_ULCF_CFG_02_01 = 0x%x\n",
		       AFE_ADDA_UL0_ULCF_CFG_02_01, value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_04_03, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_UL0_ULCF_CFG_04_03 = 0x%x\n",
		       AFE_ADDA_UL0_ULCF_CFG_04_03, value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_06_05, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_UL0_ULCF_CFG_06_05 = 0x%x\n",
		       AFE_ADDA_UL0_ULCF_CFG_06_05, value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_08_07, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_UL0_ULCF_CFG_08_07 = 0x%x\n",
		       AFE_ADDA_UL0_ULCF_CFG_08_07, value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_10_09, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_UL0_ULCF_CFG_10_09 = 0x%x\n",
		       AFE_ADDA_UL0_ULCF_CFG_10_09, value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_12_11, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_UL0_ULCF_CFG_12_11 = 0x%x\n",
		       AFE_ADDA_UL0_ULCF_CFG_12_11, value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_14_13, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_UL0_ULCF_CFG_14_13 = 0x%x\n",
		       AFE_ADDA_UL0_ULCF_CFG_14_13, value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_16_15, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_UL0_ULCF_CFG_16_15 = 0x%x\n",
		       AFE_ADDA_UL0_ULCF_CFG_16_15, value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_18_17, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_UL0_ULCF_CFG_18_17 = 0x%x\n",
		       AFE_ADDA_UL0_ULCF_CFG_18_17, value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_20_19, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_UL0_ULCF_CFG_20_19 = 0x%x\n",
		       AFE_ADDA_UL0_ULCF_CFG_20_19, value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_22_21, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_UL0_ULCF_CFG_22_21 = 0x%x\n",
		       AFE_ADDA_UL0_ULCF_CFG_22_21, value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_24_23, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_UL0_ULCF_CFG_24_23 = 0x%x\n",
		       AFE_ADDA_UL0_ULCF_CFG_24_23, value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_26_25, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_UL0_ULCF_CFG_26_25 = 0x%x\n",
		       AFE_ADDA_UL0_ULCF_CFG_26_25, value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_28_27, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_UL0_ULCF_CFG_28_27 = 0x%x\n",
		       AFE_ADDA_UL0_ULCF_CFG_28_27, value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_30_29, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_UL0_ULCF_CFG_30_29 = 0x%x\n",
		       AFE_ADDA_UL0_ULCF_CFG_30_29, value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_32_31, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_UL0_ULCF_CFG_32_31 = 0x%x\n",
		       AFE_ADDA_UL0_ULCF_CFG_32_31, value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_UL0_IP_VERSION = 0x%x\n",
		       AFE_ADDA_UL0_IP_VERSION, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC0_SRC_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC0_SRC_CON0 = 0x%x\n",
		       AFE_ADDA_DMIC0_SRC_CON0, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC0_SRC_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC0_SRC_CON1 = 0x%x\n",
		       AFE_ADDA_DMIC0_SRC_CON1, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC0_SRC_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC0_SRC_CON2 = 0x%x\n",
		       AFE_ADDA_DMIC0_SRC_CON2, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC0_SRC_DEBUG, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC0_SRC_DEBUG = 0x%x\n",
		       AFE_ADDA_DMIC0_SRC_DEBUG, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC0_SRC_DEBUG_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC0_SRC_DEBUG_MON0 = 0x%x\n",
		       AFE_ADDA_DMIC0_SRC_DEBUG_MON0, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC0_SRC_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC0_SRC_MON0 = 0x%x\n",
		       AFE_ADDA_DMIC0_SRC_MON0, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC0_SRC_MON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC0_SRC_MON1 = 0x%x\n",
		       AFE_ADDA_DMIC0_SRC_MON1, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC0_IIR_COEF_02_01, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC0_IIR_COEF_02_01 = 0x%x\n",
		       AFE_ADDA_DMIC0_IIR_COEF_02_01, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC0_IIR_COEF_04_03, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC0_IIR_COEF_04_03 = 0x%x\n",
		       AFE_ADDA_DMIC0_IIR_COEF_04_03, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC0_IIR_COEF_06_05, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC0_IIR_COEF_06_05 = 0x%x\n",
		       AFE_ADDA_DMIC0_IIR_COEF_06_05, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC0_IIR_COEF_08_07, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC0_IIR_COEF_08_07 = 0x%x\n",
		       AFE_ADDA_DMIC0_IIR_COEF_08_07, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC0_IIR_COEF_10_09, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC0_IIR_COEF_10_09 = 0x%x\n",
		       AFE_ADDA_DMIC0_IIR_COEF_10_09, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC0_ULCF_CFG_02_01, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC0_ULCF_CFG_02_01 = 0x%x\n",
		       AFE_ADDA_DMIC0_ULCF_CFG_02_01, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC0_ULCF_CFG_04_03, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC0_ULCF_CFG_04_03 = 0x%x\n",
		       AFE_ADDA_DMIC0_ULCF_CFG_04_03, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC0_ULCF_CFG_06_05, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC0_ULCF_CFG_06_05 = 0x%x\n",
		       AFE_ADDA_DMIC0_ULCF_CFG_06_05, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC0_ULCF_CFG_08_07, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC0_ULCF_CFG_08_07 = 0x%x\n",
		       AFE_ADDA_DMIC0_ULCF_CFG_08_07, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC0_ULCF_CFG_10_09, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC0_ULCF_CFG_10_09 = 0x%x\n",
		       AFE_ADDA_DMIC0_ULCF_CFG_10_09, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC0_ULCF_CFG_12_11, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC0_ULCF_CFG_12_11 = 0x%x\n",
		       AFE_ADDA_DMIC0_ULCF_CFG_12_11, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC0_ULCF_CFG_14_13, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC0_ULCF_CFG_14_13 = 0x%x\n",
		       AFE_ADDA_DMIC0_ULCF_CFG_14_13, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC0_ULCF_CFG_16_15, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC0_ULCF_CFG_16_15 = 0x%x\n",
		       AFE_ADDA_DMIC0_ULCF_CFG_16_15, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC0_ULCF_CFG_18_17, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC0_ULCF_CFG_18_17 = 0x%x\n",
		       AFE_ADDA_DMIC0_ULCF_CFG_18_17, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC0_ULCF_CFG_20_19, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC0_ULCF_CFG_20_19 = 0x%x\n",
		       AFE_ADDA_DMIC0_ULCF_CFG_20_19, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC0_ULCF_CFG_22_21, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC0_ULCF_CFG_22_21 = 0x%x\n",
		       AFE_ADDA_DMIC0_ULCF_CFG_22_21, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC0_ULCF_CFG_24_23, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC0_ULCF_CFG_24_23 = 0x%x\n",
		       AFE_ADDA_DMIC0_ULCF_CFG_24_23, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC0_ULCF_CFG_26_25, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC0_ULCF_CFG_26_25 = 0x%x\n",
		       AFE_ADDA_DMIC0_ULCF_CFG_26_25, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC0_ULCF_CFG_28_27, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC0_ULCF_CFG_28_27 = 0x%x\n",
		       AFE_ADDA_DMIC0_ULCF_CFG_28_27, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC0_ULCF_CFG_30_29, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC0_ULCF_CFG_30_29 = 0x%x\n",
		       AFE_ADDA_DMIC0_ULCF_CFG_30_29, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC0_ULCF_CFG_32_31, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC0_ULCF_CFG_32_31 = 0x%x\n",
		       AFE_ADDA_DMIC0_ULCF_CFG_32_31, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC0_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC0_IP_VERSION = 0x%x\n",
		       AFE_ADDA_DMIC0_IP_VERSION, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC1_SRC_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC1_SRC_CON0 = 0x%x\n",
		       AFE_ADDA_DMIC1_SRC_CON0, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC1_SRC_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC1_SRC_CON1 = 0x%x\n",
		       AFE_ADDA_DMIC1_SRC_CON1, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC1_SRC_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC1_SRC_CON2 = 0x%x\n",
		       AFE_ADDA_DMIC1_SRC_CON2, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC1_SRC_DEBUG, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC1_SRC_DEBUG = 0x%x\n",
		       AFE_ADDA_DMIC1_SRC_DEBUG, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC1_SRC_DEBUG_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC1_SRC_DEBUG_MON0 = 0x%x\n",
		       AFE_ADDA_DMIC1_SRC_DEBUG_MON0, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC1_SRC_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC1_SRC_MON0 = 0x%x\n",
		       AFE_ADDA_DMIC1_SRC_MON0, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC1_SRC_MON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC1_SRC_MON1 = 0x%x\n",
		       AFE_ADDA_DMIC1_SRC_MON1, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC1_IIR_COEF_02_01, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC1_IIR_COEF_02_01 = 0x%x\n",
		       AFE_ADDA_DMIC1_IIR_COEF_02_01, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC1_IIR_COEF_04_03, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC1_IIR_COEF_04_03 = 0x%x\n",
		       AFE_ADDA_DMIC1_IIR_COEF_04_03, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC1_IIR_COEF_06_05, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC1_IIR_COEF_06_05 = 0x%x\n",
		       AFE_ADDA_DMIC1_IIR_COEF_06_05, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC1_IIR_COEF_08_07, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC1_IIR_COEF_08_07 = 0x%x\n",
		       AFE_ADDA_DMIC1_IIR_COEF_08_07, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC1_IIR_COEF_10_09, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC1_IIR_COEF_10_09 = 0x%x\n",
		       AFE_ADDA_DMIC1_IIR_COEF_10_09, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC1_ULCF_CFG_02_01, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC1_ULCF_CFG_02_01 = 0x%x\n",
		       AFE_ADDA_DMIC1_ULCF_CFG_02_01, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC1_ULCF_CFG_04_03, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC1_ULCF_CFG_04_03 = 0x%x\n",
		       AFE_ADDA_DMIC1_ULCF_CFG_04_03, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC1_ULCF_CFG_06_05, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC1_ULCF_CFG_06_05 = 0x%x\n",
		       AFE_ADDA_DMIC1_ULCF_CFG_06_05, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC1_ULCF_CFG_08_07, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC1_ULCF_CFG_08_07 = 0x%x\n",
		       AFE_ADDA_DMIC1_ULCF_CFG_08_07, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC1_ULCF_CFG_10_09, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC1_ULCF_CFG_10_09 = 0x%x\n",
		       AFE_ADDA_DMIC1_ULCF_CFG_10_09, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC1_ULCF_CFG_12_11, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC1_ULCF_CFG_12_11 = 0x%x\n",
		       AFE_ADDA_DMIC1_ULCF_CFG_12_11, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC1_ULCF_CFG_14_13, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC1_ULCF_CFG_14_13 = 0x%x\n",
		       AFE_ADDA_DMIC1_ULCF_CFG_14_13, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC1_ULCF_CFG_16_15, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC1_ULCF_CFG_16_15 = 0x%x\n",
		       AFE_ADDA_DMIC1_ULCF_CFG_16_15, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC1_ULCF_CFG_18_17, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC1_ULCF_CFG_18_17 = 0x%x\n",
		       AFE_ADDA_DMIC1_ULCF_CFG_18_17, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC1_ULCF_CFG_20_19, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC1_ULCF_CFG_20_19 = 0x%x\n",
		       AFE_ADDA_DMIC1_ULCF_CFG_20_19, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC1_ULCF_CFG_22_21, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC1_ULCF_CFG_22_21 = 0x%x\n",
		       AFE_ADDA_DMIC1_ULCF_CFG_22_21, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC1_ULCF_CFG_24_23, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC1_ULCF_CFG_24_23 = 0x%x\n",
		       AFE_ADDA_DMIC1_ULCF_CFG_24_23, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC1_ULCF_CFG_26_25, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC1_ULCF_CFG_26_25 = 0x%x\n",
		       AFE_ADDA_DMIC1_ULCF_CFG_26_25, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC1_ULCF_CFG_28_27, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC1_ULCF_CFG_28_27 = 0x%x\n",
		       AFE_ADDA_DMIC1_ULCF_CFG_28_27, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC1_ULCF_CFG_30_29, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC1_ULCF_CFG_30_29 = 0x%x\n",
		       AFE_ADDA_DMIC1_ULCF_CFG_30_29, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC1_ULCF_CFG_32_31, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC1_ULCF_CFG_32_31 = 0x%x\n",
		       AFE_ADDA_DMIC1_ULCF_CFG_32_31, value);
	regmap_read(afe->regmap, AFE_ADDA_DMIC1_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_DMIC1_IP_VERSION = 0x%x\n",
		       AFE_ADDA_DMIC1_IP_VERSION, value);
	regmap_read(afe->regmap, AFE_ADDA_ULSRC_PHASE_CLK_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_ULSRC_PHASE_CLK_CON0 = 0x%x\n",
		       AFE_ADDA_ULSRC_PHASE_CLK_CON0, value);
	regmap_read(afe->regmap, AFE_ADDA_ULSRC_PHASE_CLK_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_ULSRC_PHASE_CLK_CON1 = 0x%x\n",
		       AFE_ADDA_ULSRC_PHASE_CLK_CON1, value);
	regmap_read(afe->regmap, AFE_ADDA_ULSRC_PHASE_CLK_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_ULSRC_PHASE_CLK_CON2 = 0x%x\n",
		       AFE_ADDA_ULSRC_PHASE_CLK_CON2, value);
	regmap_read(afe->regmap, AFE_ADDA_ULSRC_PHASE_CLK_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_ULSRC_PHASE_CLK_CON3 = 0x%x\n",
		       AFE_ADDA_ULSRC_PHASE_CLK_CON3, value);
	regmap_read(afe->regmap, AFE_ADDA_ULSRC_PHASE_CLK_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_ULSRC_PHASE_CLK_CON4 = 0x%x\n",
		       AFE_ADDA_ULSRC_PHASE_CLK_CON4, value);
	regmap_read(afe->regmap, AFE_ADDA_ULSRC_PHASE_ENGEN_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_ULSRC_PHASE_ENGEN_CON0 = 0x%x\n",
		       AFE_ADDA_ULSRC_PHASE_ENGEN_CON0, value);
	regmap_read(afe->regmap, AFE_ADDA_ULSRC_PHASE_ENGEN_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_ULSRC_PHASE_ENGEN_CON1 = 0x%x\n",
		       AFE_ADDA_ULSRC_PHASE_ENGEN_CON1, value);
	regmap_read(afe->regmap, AFE_ADDA_ULSRC_PHASE_RST_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x]	 AFE_ADDA_ULSRC_PHASE_RST_CON0 = 0x%x\n",
		       AFE_ADDA_ULSRC_PHASE_RST_CON0, value);
	regmap_read(afe->regmap, AFE_MTKAIF_IPM_VER_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_MTKAIF_IPM_VER_MON = 0x%x\n",
		       AFE_MTKAIF_IPM_VER_MON, value);
	regmap_read(afe->regmap, AFE_MTKAIF_MON_SEL, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_MTKAIF_MON_SEL = 0x%x\n",
		       AFE_MTKAIF_MON_SEL, value);
	regmap_read(afe->regmap, AFE_MTKAIF_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_MTKAIF_MON = 0x%x\n",
		       AFE_MTKAIF_MON, value);
	regmap_read(afe->regmap, AFE_MTKAIF0_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_MTKAIF0_CFG0 = 0x%x\n",
		       AFE_MTKAIF0_CFG0, value);
	regmap_read(afe->regmap, AFE_MTKAIF0_TX_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_MTKAIF0_TX_CFG0 = 0x%x\n",
		       AFE_MTKAIF0_TX_CFG0, value);
	regmap_read(afe->regmap, AFE_MTKAIF0_RX_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_MTKAIF0_RX_CFG0 = 0x%x\n",
		       AFE_MTKAIF0_RX_CFG0, value);
	regmap_read(afe->regmap, AFE_MTKAIF0_RX_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_MTKAIF0_RX_CFG1 = 0x%x\n",
		       AFE_MTKAIF0_RX_CFG1, value);
	regmap_read(afe->regmap, AFE_MTKAIF0_RX_CFG2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_MTKAIF0_RX_CFG2 = 0x%x\n",
		       AFE_MTKAIF0_RX_CFG2, value);
	regmap_read(afe->regmap, AFE_MTKAIF1_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_MTKAIF1_CFG0 = 0x%x\n",
		       AFE_MTKAIF1_CFG0, value);
	regmap_read(afe->regmap, AFE_MTKAIF1_TX_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_MTKAIF1_TX_CFG0 = 0x%x\n",
		       AFE_MTKAIF1_TX_CFG0, value);
	regmap_read(afe->regmap, AFE_MTKAIF1_RX_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_MTKAIF1_RX_CFG0 = 0x%x\n",
		       AFE_MTKAIF1_RX_CFG0, value);
	regmap_read(afe->regmap, AFE_MTKAIF1_RX_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_MTKAIF1_RX_CFG1 = 0x%x\n",
		       AFE_MTKAIF1_RX_CFG1, value);
	regmap_read(afe->regmap, AFE_MTKAIF1_RX_CFG2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_MTKAIF1_RX_CFG2 = 0x%x\n",
		       AFE_MTKAIF1_RX_CFG2, value);
	regmap_read(afe->regmap, AFE_AUD_PAD_TOP_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_AUD_PAD_TOP_CFG0 = 0x%x\n",
		       AFE_AUD_PAD_TOP_CFG0, value);
	regmap_read(afe->regmap, AFE_AUD_PAD_TOP_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_AUD_PAD_TOP_MON = 0x%x\n",
		       AFE_AUD_PAD_TOP_MON, value);
	regmap_read(afe->regmap, AFE_ADDA_MTKAIFV4_TX_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_ADDA_MTKAIFV4_TX_CFG0 = 0x%x\n",
		       AFE_ADDA_MTKAIFV4_TX_CFG0, value);
	regmap_read(afe->regmap, AFE_ADDA6_MTKAIFV4_TX_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_ADDA6_MTKAIFV4_TX_CFG0 = 0x%x\n",
		       AFE_ADDA6_MTKAIFV4_TX_CFG0, value);
	regmap_read(afe->regmap, AFE_ADDA_MTKAIFV4_RX_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_ADDA_MTKAIFV4_RX_CFG0 = 0x%x\n",
		       AFE_ADDA_MTKAIFV4_RX_CFG0, value);
	regmap_read(afe->regmap, AFE_ADDA_MTKAIFV4_RX_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_ADDA_MTKAIFV4_RX_CFG1 = 0x%x\n",
		       AFE_ADDA_MTKAIFV4_RX_CFG1, value);
	regmap_read(afe->regmap, AFE_ADDA6_MTKAIFV4_RX_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_ADDA6_MTKAIFV4_RX_CFG0 = 0x%x\n",
		       AFE_ADDA6_MTKAIFV4_RX_CFG0, value);
	regmap_read(afe->regmap, AFE_ADDA6_MTKAIFV4_RX_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_ADDA6_MTKAIFV4_RX_CFG1 = 0x%x\n",
		       AFE_ADDA6_MTKAIFV4_RX_CFG1, value);
	regmap_read(afe->regmap, AFE_ADDA_MTKAIFV4_TX_SYNCWORD_CFG, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_ADDA_MTKAIFV4_TX_SYNCWORD_CFG = 0x%x\n",
		       AFE_ADDA_MTKAIFV4_TX_SYNCWORD_CFG, value);
	regmap_read(afe->regmap, AFE_ADDA_MTKAIFV4_RX_SYNCWORD_CFG, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_ADDA_MTKAIFV4_RX_SYNCWORD_CFG = 0x%x\n",
		       AFE_ADDA_MTKAIFV4_RX_SYNCWORD_CFG, value);
	regmap_read(afe->regmap, AFE_ADDA_MTKAIFV4_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_ADDA_MTKAIFV4_MON0 = 0x%x\n",
		       AFE_ADDA_MTKAIFV4_MON0, value);
	regmap_read(afe->regmap, AFE_ADDA_MTKAIFV4_MON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_ADDA_MTKAIFV4_MON1 = 0x%x\n",
		       AFE_ADDA_MTKAIFV4_MON1, value);
	regmap_read(afe->regmap, AFE_ADDA6_MTKAIFV4_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_ADDA6_MTKAIFV4_MON0 = 0x%x\n",
		       AFE_ADDA6_MTKAIFV4_MON0, value);
	regmap_read(afe->regmap, ETDM_IN0_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_IN0_CON0 = 0x%x\n",
		       ETDM_IN0_CON0, value);
	regmap_read(afe->regmap, ETDM_IN0_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_IN0_CON1 = 0x%x\n",
		       ETDM_IN0_CON1, value);
	regmap_read(afe->regmap, ETDM_IN0_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_IN0_CON2 = 0x%x\n",
		       ETDM_IN0_CON2, value);
	regmap_read(afe->regmap, ETDM_IN0_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_IN0_CON3 = 0x%x\n",
		       ETDM_IN0_CON3, value);
	regmap_read(afe->regmap, ETDM_IN0_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_IN0_CON4 = 0x%x\n",
		       ETDM_IN0_CON4, value);
	regmap_read(afe->regmap, ETDM_IN0_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_IN0_CON5 = 0x%x\n",
		       ETDM_IN0_CON5, value);
	regmap_read(afe->regmap, ETDM_IN0_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_IN0_CON6 = 0x%x\n",
		       ETDM_IN0_CON6, value);
	regmap_read(afe->regmap, ETDM_IN0_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_IN0_CON7 = 0x%x\n",
		       ETDM_IN0_CON7, value);
	regmap_read(afe->regmap, ETDM_IN0_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_IN0_CON8 = 0x%x\n",
		       ETDM_IN0_CON8, value);
	regmap_read(afe->regmap, ETDM_IN0_CON9, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_IN0_CON9 = 0x%x\n",
		       ETDM_IN0_CON9, value);
	regmap_read(afe->regmap, ETDM_IN0_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_IN0_MON = 0x%x\n",
		       ETDM_IN0_MON, value);
	regmap_read(afe->regmap, ETDM_IN1_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_IN1_CON0 = 0x%x\n",
		       ETDM_IN1_CON0, value);
	regmap_read(afe->regmap, ETDM_IN1_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_IN1_CON1 = 0x%x\n",
		       ETDM_IN1_CON1, value);
	regmap_read(afe->regmap, ETDM_IN1_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_IN1_CON2 = 0x%x\n",
		       ETDM_IN1_CON2, value);
	regmap_read(afe->regmap, ETDM_IN1_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_IN1_CON3 = 0x%x\n",
		       ETDM_IN1_CON3, value);
	regmap_read(afe->regmap, ETDM_IN1_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_IN1_CON4 = 0x%x\n",
		       ETDM_IN1_CON4, value);
	regmap_read(afe->regmap, ETDM_IN1_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_IN1_CON5 = 0x%x\n",
		       ETDM_IN1_CON5, value);
	regmap_read(afe->regmap, ETDM_IN1_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_IN1_CON6 = 0x%x\n",
		       ETDM_IN1_CON6, value);
	regmap_read(afe->regmap, ETDM_IN1_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_IN1_CON7 = 0x%x\n",
		       ETDM_IN1_CON7, value);
	regmap_read(afe->regmap, ETDM_IN1_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_IN1_CON8 = 0x%x\n",
		       ETDM_IN1_CON8, value);
	regmap_read(afe->regmap, ETDM_IN1_CON9, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_IN1_CON9 = 0x%x\n",
		       ETDM_IN1_CON9, value);
	regmap_read(afe->regmap, ETDM_IN1_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_IN1_MON = 0x%x\n",
		       ETDM_IN1_MON, value);
	regmap_read(afe->regmap, ETDM_OUT0_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_OUT0_MON = 0x%x\n",
		       ETDM_OUT0_MON, value);
	regmap_read(afe->regmap, ETDM_OUT1_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_OUT1_MON = 0x%x\n",
		       ETDM_OUT1_MON, value);
	regmap_read(afe->regmap, ETDM_OUT4_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_OUT4_MON = 0x%x\n",
		       ETDM_OUT4_MON, value);
	regmap_read(afe->regmap, ETDM_OUT0_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_OUT0_CON0 = 0x%x\n",
		       ETDM_OUT0_CON0, value);
	regmap_read(afe->regmap, ETDM_OUT0_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_OUT0_CON1 = 0x%x\n",
		       ETDM_OUT0_CON1, value);
	regmap_read(afe->regmap, ETDM_OUT0_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_OUT0_CON2 = 0x%x\n",
		       ETDM_OUT0_CON2, value);
	regmap_read(afe->regmap, ETDM_OUT0_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_OUT0_CON3 = 0x%x\n",
		       ETDM_OUT0_CON3, value);
	regmap_read(afe->regmap, ETDM_OUT0_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_OUT0_CON4 = 0x%x\n",
		       ETDM_OUT0_CON4, value);
	regmap_read(afe->regmap, ETDM_OUT0_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_OUT0_CON5 = 0x%x\n",
		       ETDM_OUT0_CON5, value);
	regmap_read(afe->regmap, ETDM_OUT0_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_OUT0_CON6 = 0x%x\n",
		       ETDM_OUT0_CON6, value);
	regmap_read(afe->regmap, ETDM_OUT0_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_OUT0_CON7 = 0x%x\n",
		       ETDM_OUT0_CON7, value);
	regmap_read(afe->regmap, ETDM_OUT0_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_OUT0_CON8 = 0x%x\n",
		       ETDM_OUT0_CON8, value);
	regmap_read(afe->regmap, ETDM_OUT1_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_OUT1_CON0 = 0x%x\n",
		       ETDM_OUT1_CON0, value);
	regmap_read(afe->regmap, ETDM_OUT1_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_OUT1_CON1 = 0x%x\n",
		       ETDM_OUT1_CON1, value);
	regmap_read(afe->regmap, ETDM_OUT1_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_OUT1_CON2 = 0x%x\n",
		       ETDM_OUT1_CON2, value);
	regmap_read(afe->regmap, ETDM_OUT1_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_OUT1_CON3 = 0x%x\n",
		       ETDM_OUT1_CON3, value);
	regmap_read(afe->regmap, ETDM_OUT1_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_OUT1_CON4 = 0x%x\n",
		       ETDM_OUT1_CON4, value);
	regmap_read(afe->regmap, ETDM_OUT1_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_OUT1_CON5 = 0x%x\n",
		       ETDM_OUT1_CON5, value);
	regmap_read(afe->regmap, ETDM_OUT1_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_OUT1_CON6 = 0x%x\n",
		       ETDM_OUT1_CON6, value);
	regmap_read(afe->regmap, ETDM_OUT1_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_OUT1_CON7 = 0x%x\n",
		       ETDM_OUT1_CON7, value);
	regmap_read(afe->regmap, ETDM_OUT1_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_OUT1_CON8 = 0x%x\n",
		       ETDM_OUT1_CON8, value);
	regmap_read(afe->regmap, ETDM_OUT4_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_OUT4_CON0 = 0x%x\n",
		       ETDM_OUT4_CON0, value);
	regmap_read(afe->regmap, ETDM_OUT4_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_OUT4_CON1 = 0x%x\n",
		       ETDM_OUT4_CON1, value);
	regmap_read(afe->regmap, ETDM_OUT4_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_OUT4_CON2 = 0x%x\n",
		       ETDM_OUT4_CON2, value);
	regmap_read(afe->regmap, ETDM_OUT4_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_OUT4_CON3 = 0x%x\n",
		       ETDM_OUT4_CON3, value);
	regmap_read(afe->regmap, ETDM_OUT4_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_OUT4_CON4 = 0x%x\n",
		       ETDM_OUT4_CON4, value);
	regmap_read(afe->regmap, ETDM_OUT4_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_OUT4_CON5 = 0x%x\n",
		       ETDM_OUT4_CON5, value);
	regmap_read(afe->regmap, ETDM_OUT4_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_OUT4_CON6 = 0x%x\n",
		       ETDM_OUT4_CON6, value);
	regmap_read(afe->regmap, ETDM_OUT4_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_OUT4_CON7 = 0x%x\n",
		       ETDM_OUT4_CON7, value);
	regmap_read(afe->regmap, ETDM_OUT4_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_OUT4_CON8 = 0x%x\n",
		       ETDM_OUT4_CON8, value);
	regmap_read(afe->regmap, ETDM_0_3_COWORK_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_0_3_COWORK_CON0 = 0x%x\n",
		       ETDM_0_3_COWORK_CON0, value);
	regmap_read(afe->regmap, ETDM_0_3_COWORK_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_0_3_COWORK_CON1 = 0x%x\n",
		       ETDM_0_3_COWORK_CON1, value);
	regmap_read(afe->regmap, ETDM_0_3_COWORK_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_0_3_COWORK_CON2 = 0x%x\n",
		       ETDM_0_3_COWORK_CON2, value);
	regmap_read(afe->regmap, ETDM_0_3_COWORK_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_0_3_COWORK_CON3 = 0x%x\n",
		       ETDM_0_3_COWORK_CON3, value);
	regmap_read(afe->regmap, ETDM_4_7_COWORK_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_4_7_COWORK_CON0 = 0x%x\n",
		       ETDM_4_7_COWORK_CON0, value);
	regmap_read(afe->regmap, ETDM_4_7_COWORK_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_4_7_COWORK_CON1 = 0x%x\n",
		       ETDM_4_7_COWORK_CON1, value);
	regmap_read(afe->regmap, ETDM_4_7_COWORK_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_4_7_COWORK_CON2 = 0x%x\n",
		       ETDM_4_7_COWORK_CON2, value);
	regmap_read(afe->regmap, ETDM_4_7_COWORK_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] ETDM_4_7_COWORK_CON3 = 0x%x\n",
		       ETDM_4_7_COWORK_CON3, value);
#ifndef SKIP_INTERCONN_DRAM_SIZE
	regmap_read(afe->regmap, AFE_CONN004_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN004_0 = 0x%x\n",
		       AFE_CONN004_0, value);
	regmap_read(afe->regmap, AFE_CONN004_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN004_1 = 0x%x\n",
		       AFE_CONN004_1, value);
	regmap_read(afe->regmap, AFE_CONN004_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN004_2 = 0x%x\n",
		       AFE_CONN004_2, value);
	regmap_read(afe->regmap, AFE_CONN004_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN004_4 = 0x%x\n",
		       AFE_CONN004_4, value);
	regmap_read(afe->regmap, AFE_CONN004_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN004_6 = 0x%x\n",
		       AFE_CONN004_6, value);
	regmap_read(afe->regmap, AFE_CONN005_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN005_0 = 0x%x\n",
		       AFE_CONN005_0, value);
	regmap_read(afe->regmap, AFE_CONN005_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN005_1 = 0x%x\n",
		       AFE_CONN005_1, value);
	regmap_read(afe->regmap, AFE_CONN005_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN005_2 = 0x%x\n",
		       AFE_CONN005_2, value);
	regmap_read(afe->regmap, AFE_CONN005_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN005_4 = 0x%x\n",
		       AFE_CONN005_4, value);
	regmap_read(afe->regmap, AFE_CONN005_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN005_6 = 0x%x\n",
		       AFE_CONN005_6, value);
	regmap_read(afe->regmap, AFE_CONN006_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN006_0 = 0x%x\n",
		       AFE_CONN006_0, value);
	regmap_read(afe->regmap, AFE_CONN006_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN006_1 = 0x%x\n",
		       AFE_CONN006_1, value);
	regmap_read(afe->regmap, AFE_CONN006_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN006_2 = 0x%x\n",
		       AFE_CONN006_2, value);
	regmap_read(afe->regmap, AFE_CONN006_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN006_4 = 0x%x\n",
		       AFE_CONN006_4, value);
	regmap_read(afe->regmap, AFE_CONN006_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN006_6 = 0x%x\n",
		       AFE_CONN006_6, value);
	regmap_read(afe->regmap, AFE_CONN007_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN007_0 = 0x%x\n",
		       AFE_CONN007_0, value);
	regmap_read(afe->regmap, AFE_CONN007_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN007_1 = 0x%x\n",
		       AFE_CONN007_1, value);
	regmap_read(afe->regmap, AFE_CONN007_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN007_2 = 0x%x\n",
		       AFE_CONN007_2, value);
	regmap_read(afe->regmap, AFE_CONN007_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN007_4 = 0x%x\n",
		       AFE_CONN007_4, value);
	regmap_read(afe->regmap, AFE_CONN007_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN007_6 = 0x%x\n",
		       AFE_CONN007_6, value);
	regmap_read(afe->regmap, AFE_CONN008_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN008_0 = 0x%x\n",
		       AFE_CONN008_0, value);
	regmap_read(afe->regmap, AFE_CONN008_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN008_1 = 0x%x\n",
		       AFE_CONN008_1, value);
	regmap_read(afe->regmap, AFE_CONN008_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN008_2 = 0x%x\n",
		       AFE_CONN008_2, value);
	regmap_read(afe->regmap, AFE_CONN008_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN008_4 = 0x%x\n",
		       AFE_CONN008_4, value);
	regmap_read(afe->regmap, AFE_CONN008_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN008_6 = 0x%x\n",
		       AFE_CONN008_6, value);
	regmap_read(afe->regmap, AFE_CONN009_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN009_0 = 0x%x\n",
		       AFE_CONN009_0, value);
	regmap_read(afe->regmap, AFE_CONN009_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN009_1 = 0x%x\n",
		       AFE_CONN009_1, value);
	regmap_read(afe->regmap, AFE_CONN009_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN009_2 = 0x%x\n",
		       AFE_CONN009_2, value);
	regmap_read(afe->regmap, AFE_CONN009_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN009_4 = 0x%x\n",
		       AFE_CONN009_4, value);
	regmap_read(afe->regmap, AFE_CONN009_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN009_6 = 0x%x\n",
		       AFE_CONN009_6, value);
	regmap_read(afe->regmap, AFE_CONN010_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN010_0 = 0x%x\n",
		       AFE_CONN010_0, value);
	regmap_read(afe->regmap, AFE_CONN010_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN010_1 = 0x%x\n",
		       AFE_CONN010_1, value);
	regmap_read(afe->regmap, AFE_CONN010_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN010_2 = 0x%x\n",
		       AFE_CONN010_2, value);
	regmap_read(afe->regmap, AFE_CONN010_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN010_4 = 0x%x\n",
		       AFE_CONN010_4, value);
	regmap_read(afe->regmap, AFE_CONN010_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN010_6 = 0x%x\n",
		       AFE_CONN010_6, value);
	regmap_read(afe->regmap, AFE_CONN011_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN011_0 = 0x%x\n",
		       AFE_CONN011_0, value);
	regmap_read(afe->regmap, AFE_CONN011_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN011_1 = 0x%x\n",
		       AFE_CONN011_1, value);
	regmap_read(afe->regmap, AFE_CONN011_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN011_2 = 0x%x\n",
		       AFE_CONN011_2, value);
	regmap_read(afe->regmap, AFE_CONN011_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN011_4 = 0x%x\n",
		       AFE_CONN011_4, value);
	regmap_read(afe->regmap, AFE_CONN011_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN011_6 = 0x%x\n",
		       AFE_CONN011_6, value);
	regmap_read(afe->regmap, AFE_CONN014_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN014_0 = 0x%x\n",
		       AFE_CONN014_0, value);
	regmap_read(afe->regmap, AFE_CONN014_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN014_1 = 0x%x\n",
		       AFE_CONN014_1, value);
	regmap_read(afe->regmap, AFE_CONN014_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN014_2 = 0x%x\n",
		       AFE_CONN014_2, value);
	regmap_read(afe->regmap, AFE_CONN014_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN014_4 = 0x%x\n",
		       AFE_CONN014_4, value);
	regmap_read(afe->regmap, AFE_CONN014_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN014_6 = 0x%x\n",
		       AFE_CONN014_6, value);
	regmap_read(afe->regmap, AFE_CONN015_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN015_0 = 0x%x\n",
		       AFE_CONN015_0, value);
	regmap_read(afe->regmap, AFE_CONN015_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN015_1 = 0x%x\n",
		       AFE_CONN015_1, value);
	regmap_read(afe->regmap, AFE_CONN015_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN015_2 = 0x%x\n",
		       AFE_CONN015_2, value);
	regmap_read(afe->regmap, AFE_CONN015_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN015_4 = 0x%x\n",
		       AFE_CONN015_4, value);
	regmap_read(afe->regmap, AFE_CONN015_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN015_6 = 0x%x\n",
		       AFE_CONN015_6, value);
	regmap_read(afe->regmap, AFE_CONN016_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN016_0 = 0x%x\n",
		       AFE_CONN016_0, value);
	regmap_read(afe->regmap, AFE_CONN016_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN016_1 = 0x%x\n",
		       AFE_CONN016_1, value);
	regmap_read(afe->regmap, AFE_CONN016_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN016_2 = 0x%x\n",
		       AFE_CONN016_2, value);
	regmap_read(afe->regmap, AFE_CONN016_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN016_4 = 0x%x\n",
		       AFE_CONN016_4, value);
	regmap_read(afe->regmap, AFE_CONN016_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN016_6 = 0x%x\n",
		       AFE_CONN016_6, value);
	regmap_read(afe->regmap, AFE_CONN017_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN017_0 = 0x%x\n",
		       AFE_CONN017_0, value);
	regmap_read(afe->regmap, AFE_CONN017_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN017_1 = 0x%x\n",
		       AFE_CONN017_1, value);
	regmap_read(afe->regmap, AFE_CONN017_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN017_2 = 0x%x\n",
		       AFE_CONN017_2, value);
	regmap_read(afe->regmap, AFE_CONN017_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN017_4 = 0x%x\n",
		       AFE_CONN017_4, value);
	regmap_read(afe->regmap, AFE_CONN017_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN017_6 = 0x%x\n",
		       AFE_CONN017_6, value);
	regmap_read(afe->regmap, AFE_CONN018_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN018_0 = 0x%x\n",
		       AFE_CONN018_0, value);
	regmap_read(afe->regmap, AFE_CONN018_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN018_1 = 0x%x\n",
		       AFE_CONN018_1, value);
	regmap_read(afe->regmap, AFE_CONN018_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN018_2 = 0x%x\n",
		       AFE_CONN018_2, value);
	regmap_read(afe->regmap, AFE_CONN018_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN018_4 = 0x%x\n",
		       AFE_CONN018_4, value);
	regmap_read(afe->regmap, AFE_CONN018_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN018_6 = 0x%x\n",
		       AFE_CONN018_6, value);
	regmap_read(afe->regmap, AFE_CONN019_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN019_0 = 0x%x\n",
		       AFE_CONN019_0, value);
	regmap_read(afe->regmap, AFE_CONN019_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN019_1 = 0x%x\n",
		       AFE_CONN019_1, value);
	regmap_read(afe->regmap, AFE_CONN019_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN019_2 = 0x%x\n",
		       AFE_CONN019_2, value);
	regmap_read(afe->regmap, AFE_CONN019_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN019_4 = 0x%x\n",
		       AFE_CONN019_4, value);
	regmap_read(afe->regmap, AFE_CONN019_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN019_6 = 0x%x\n",
		       AFE_CONN019_6, value);
	regmap_read(afe->regmap, AFE_CONN020_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN020_0 = 0x%x\n",
		       AFE_CONN020_0, value);
	regmap_read(afe->regmap, AFE_CONN020_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN020_1 = 0x%x\n",
		       AFE_CONN020_1, value);
	regmap_read(afe->regmap, AFE_CONN020_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN020_2 = 0x%x\n",
		       AFE_CONN020_2, value);
	regmap_read(afe->regmap, AFE_CONN020_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN020_4 = 0x%x\n",
		       AFE_CONN020_4, value);
	regmap_read(afe->regmap, AFE_CONN020_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN020_6 = 0x%x\n",
		       AFE_CONN020_6, value);
	regmap_read(afe->regmap, AFE_CONN021_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN021_0 = 0x%x\n",
		       AFE_CONN021_0, value);
	regmap_read(afe->regmap, AFE_CONN021_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN021_1 = 0x%x\n",
		       AFE_CONN021_1, value);
	regmap_read(afe->regmap, AFE_CONN021_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN021_2 = 0x%x\n",
		       AFE_CONN021_2, value);
	regmap_read(afe->regmap, AFE_CONN021_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN021_4 = 0x%x\n",
		       AFE_CONN021_4, value);
	regmap_read(afe->regmap, AFE_CONN021_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN021_6 = 0x%x\n",
		       AFE_CONN021_6, value);
	regmap_read(afe->regmap, AFE_CONN022_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN022_0 = 0x%x\n",
		       AFE_CONN022_0, value);
	regmap_read(afe->regmap, AFE_CONN022_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN022_1 = 0x%x\n",
		       AFE_CONN022_1, value);
	regmap_read(afe->regmap, AFE_CONN022_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN022_2 = 0x%x\n",
		       AFE_CONN022_2, value);
	regmap_read(afe->regmap, AFE_CONN022_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN022_4 = 0x%x\n",
		       AFE_CONN022_4, value);
	regmap_read(afe->regmap, AFE_CONN022_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN022_6 = 0x%x\n",
		       AFE_CONN022_6, value);
	regmap_read(afe->regmap, AFE_CONN023_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN023_0 = 0x%x\n",
		       AFE_CONN023_0, value);
	regmap_read(afe->regmap, AFE_CONN023_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN023_1 = 0x%x\n",
		       AFE_CONN023_1, value);
	regmap_read(afe->regmap, AFE_CONN023_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN023_2 = 0x%x\n",
		       AFE_CONN023_2, value);
	regmap_read(afe->regmap, AFE_CONN023_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN023_4 = 0x%x\n",
		       AFE_CONN023_4, value);
	regmap_read(afe->regmap, AFE_CONN023_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN023_6 = 0x%x\n",
		       AFE_CONN023_6, value);
	regmap_read(afe->regmap, AFE_CONN024_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN024_0 = 0x%x\n",
		       AFE_CONN024_0, value);
	regmap_read(afe->regmap, AFE_CONN024_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN024_1 = 0x%x\n",
		       AFE_CONN024_1, value);
	regmap_read(afe->regmap, AFE_CONN024_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN024_2 = 0x%x\n",
		       AFE_CONN024_2, value);
	regmap_read(afe->regmap, AFE_CONN024_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN024_4 = 0x%x\n",
		       AFE_CONN024_4, value);
	regmap_read(afe->regmap, AFE_CONN024_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN024_6 = 0x%x\n",
		       AFE_CONN024_6, value);
	regmap_read(afe->regmap, AFE_CONN025_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN025_0 = 0x%x\n",
		       AFE_CONN025_0, value);
	regmap_read(afe->regmap, AFE_CONN025_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN025_1 = 0x%x\n",
		       AFE_CONN025_1, value);
	regmap_read(afe->regmap, AFE_CONN025_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN025_2 = 0x%x\n",
		       AFE_CONN025_2, value);
	regmap_read(afe->regmap, AFE_CONN025_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN025_4 = 0x%x\n",
		       AFE_CONN025_4, value);
	regmap_read(afe->regmap, AFE_CONN025_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN025_6 = 0x%x\n",
		       AFE_CONN025_6, value);
	regmap_read(afe->regmap, AFE_CONN026_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN026_0 = 0x%x\n",
		       AFE_CONN026_0, value);
	regmap_read(afe->regmap, AFE_CONN026_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN026_1 = 0x%x\n",
		       AFE_CONN026_1, value);
	regmap_read(afe->regmap, AFE_CONN026_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN026_2 = 0x%x\n",
		       AFE_CONN026_2, value);
	regmap_read(afe->regmap, AFE_CONN026_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN026_4 = 0x%x\n",
		       AFE_CONN026_4, value);
	regmap_read(afe->regmap, AFE_CONN026_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN026_6 = 0x%x\n",
		       AFE_CONN026_6, value);
	regmap_read(afe->regmap, AFE_CONN027_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN027_0 = 0x%x\n",
		       AFE_CONN027_0, value);
	regmap_read(afe->regmap, AFE_CONN027_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN027_1 = 0x%x\n",
		       AFE_CONN027_1, value);
	regmap_read(afe->regmap, AFE_CONN027_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN027_2 = 0x%x\n",
		       AFE_CONN027_2, value);
	regmap_read(afe->regmap, AFE_CONN027_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN027_4 = 0x%x\n",
		       AFE_CONN027_4, value);
	regmap_read(afe->regmap, AFE_CONN027_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN027_6 = 0x%x\n",
		       AFE_CONN027_6, value);
	regmap_read(afe->regmap, AFE_CONN028_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN028_0 = 0x%x\n",
		       AFE_CONN028_0, value);
	regmap_read(afe->regmap, AFE_CONN028_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN028_1 = 0x%x\n",
		       AFE_CONN028_1, value);
	regmap_read(afe->regmap, AFE_CONN028_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN028_2 = 0x%x\n",
		       AFE_CONN028_2, value);
	regmap_read(afe->regmap, AFE_CONN028_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN028_4 = 0x%x\n",
		       AFE_CONN028_4, value);
	regmap_read(afe->regmap, AFE_CONN028_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN028_6 = 0x%x\n",
		       AFE_CONN028_6, value);
	regmap_read(afe->regmap, AFE_CONN029_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN029_0 = 0x%x\n",
		       AFE_CONN029_0, value);
	regmap_read(afe->regmap, AFE_CONN029_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN029_1 = 0x%x\n",
		       AFE_CONN029_1, value);
	regmap_read(afe->regmap, AFE_CONN029_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN029_2 = 0x%x\n",
		       AFE_CONN029_2, value);
	regmap_read(afe->regmap, AFE_CONN029_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN029_4 = 0x%x\n",
		       AFE_CONN029_4, value);
	regmap_read(afe->regmap, AFE_CONN029_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN029_6 = 0x%x\n",
		       AFE_CONN029_6, value);
	regmap_read(afe->regmap, AFE_CONN030_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN030_0 = 0x%x\n",
		       AFE_CONN030_0, value);
	regmap_read(afe->regmap, AFE_CONN030_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN030_1 = 0x%x\n",
		       AFE_CONN030_1, value);
	regmap_read(afe->regmap, AFE_CONN030_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN030_2 = 0x%x\n",
		       AFE_CONN030_2, value);
	regmap_read(afe->regmap, AFE_CONN030_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN030_4 = 0x%x\n",
		       AFE_CONN030_4, value);
	regmap_read(afe->regmap, AFE_CONN030_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN030_6 = 0x%x\n",
		       AFE_CONN030_6, value);
	regmap_read(afe->regmap, AFE_CONN031_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN031_0 = 0x%x\n",
		       AFE_CONN031_0, value);
	regmap_read(afe->regmap, AFE_CONN031_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN031_1 = 0x%x\n",
		       AFE_CONN031_1, value);
	regmap_read(afe->regmap, AFE_CONN031_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN031_2 = 0x%x\n",
		       AFE_CONN031_2, value);
	regmap_read(afe->regmap, AFE_CONN031_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN031_4 = 0x%x\n",
		       AFE_CONN031_4, value);
	regmap_read(afe->regmap, AFE_CONN031_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN031_6 = 0x%x\n",
		       AFE_CONN031_6, value);
	regmap_read(afe->regmap, AFE_CONN032_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN032_0 = 0x%x\n",
		       AFE_CONN032_0, value);
	regmap_read(afe->regmap, AFE_CONN032_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN032_1 = 0x%x\n",
		       AFE_CONN032_1, value);
	regmap_read(afe->regmap, AFE_CONN032_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN032_2 = 0x%x\n",
		       AFE_CONN032_2, value);
	regmap_read(afe->regmap, AFE_CONN032_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN032_4 = 0x%x\n",
		       AFE_CONN032_4, value);
	regmap_read(afe->regmap, AFE_CONN032_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN032_6 = 0x%x\n",
		       AFE_CONN032_6, value);
	regmap_read(afe->regmap, AFE_CONN033_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN033_0 = 0x%x\n",
		       AFE_CONN033_0, value);
	regmap_read(afe->regmap, AFE_CONN033_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN033_1 = 0x%x\n",
		       AFE_CONN033_1, value);
	regmap_read(afe->regmap, AFE_CONN033_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN033_2 = 0x%x\n",
		       AFE_CONN033_2, value);
	regmap_read(afe->regmap, AFE_CONN033_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN033_4 = 0x%x\n",
		       AFE_CONN033_4, value);
	regmap_read(afe->regmap, AFE_CONN033_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN033_6 = 0x%x\n",
		       AFE_CONN033_6, value);
	regmap_read(afe->regmap, AFE_CONN034_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN034_0 = 0x%x\n",
		       AFE_CONN034_0, value);
	regmap_read(afe->regmap, AFE_CONN034_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN034_1 = 0x%x\n",
		       AFE_CONN034_1, value);
	regmap_read(afe->regmap, AFE_CONN034_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN034_2 = 0x%x\n",
		       AFE_CONN034_2, value);
	regmap_read(afe->regmap, AFE_CONN034_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN034_4 = 0x%x\n",
		       AFE_CONN034_4, value);
	regmap_read(afe->regmap, AFE_CONN034_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN034_6 = 0x%x\n",
		       AFE_CONN034_6, value);
	regmap_read(afe->regmap, AFE_CONN035_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN035_0 = 0x%x\n",
		       AFE_CONN035_0, value);
	regmap_read(afe->regmap, AFE_CONN035_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN035_1 = 0x%x\n",
		       AFE_CONN035_1, value);
	regmap_read(afe->regmap, AFE_CONN035_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN035_2 = 0x%x\n",
		       AFE_CONN035_2, value);
	regmap_read(afe->regmap, AFE_CONN035_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN035_4 = 0x%x\n",
		       AFE_CONN035_4, value);
	regmap_read(afe->regmap, AFE_CONN035_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN035_6 = 0x%x\n",
		       AFE_CONN035_6, value);
	regmap_read(afe->regmap, AFE_CONN036_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN036_0 = 0x%x\n",
		       AFE_CONN036_0, value);
	regmap_read(afe->regmap, AFE_CONN036_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN036_1 = 0x%x\n",
		       AFE_CONN036_1, value);
	regmap_read(afe->regmap, AFE_CONN036_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN036_2 = 0x%x\n",
		       AFE_CONN036_2, value);
	regmap_read(afe->regmap, AFE_CONN036_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN036_4 = 0x%x\n",
		       AFE_CONN036_4, value);
	regmap_read(afe->regmap, AFE_CONN036_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN036_6 = 0x%x\n",
		       AFE_CONN036_6, value);
	regmap_read(afe->regmap, AFE_CONN037_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN037_0 = 0x%x\n",
		       AFE_CONN037_0, value);
	regmap_read(afe->regmap, AFE_CONN037_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN037_1 = 0x%x\n",
		       AFE_CONN037_1, value);
	regmap_read(afe->regmap, AFE_CONN037_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN037_2 = 0x%x\n",
		       AFE_CONN037_2, value);
	regmap_read(afe->regmap, AFE_CONN037_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN037_4 = 0x%x\n",
		       AFE_CONN037_4, value);
	regmap_read(afe->regmap, AFE_CONN037_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN037_6 = 0x%x\n",
		       AFE_CONN037_6, value);
	regmap_read(afe->regmap, AFE_CONN038_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN038_0 = 0x%x\n",
		       AFE_CONN038_0, value);
	regmap_read(afe->regmap, AFE_CONN038_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN038_1 = 0x%x\n",
		       AFE_CONN038_1, value);
	regmap_read(afe->regmap, AFE_CONN038_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN038_2 = 0x%x\n",
		       AFE_CONN038_2, value);
	regmap_read(afe->regmap, AFE_CONN038_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN038_4 = 0x%x\n",
		       AFE_CONN038_4, value);
	regmap_read(afe->regmap, AFE_CONN038_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN038_6 = 0x%x\n",
		       AFE_CONN038_6, value);
	regmap_read(afe->regmap, AFE_CONN039_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN039_0 = 0x%x\n",
		       AFE_CONN039_0, value);
	regmap_read(afe->regmap, AFE_CONN039_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN039_1 = 0x%x\n",
		       AFE_CONN039_1, value);
	regmap_read(afe->regmap, AFE_CONN039_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN039_2 = 0x%x\n",
		       AFE_CONN039_2, value);
	regmap_read(afe->regmap, AFE_CONN039_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN039_4 = 0x%x\n",
		       AFE_CONN039_4, value);
	regmap_read(afe->regmap, AFE_CONN039_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN039_6 = 0x%x\n",
		       AFE_CONN039_6, value);
	regmap_read(afe->regmap, AFE_CONN040_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN040_0 = 0x%x\n",
		       AFE_CONN040_0, value);
	regmap_read(afe->regmap, AFE_CONN040_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN040_1 = 0x%x\n",
		       AFE_CONN040_1, value);
	regmap_read(afe->regmap, AFE_CONN040_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN040_2 = 0x%x\n",
		       AFE_CONN040_2, value);
	regmap_read(afe->regmap, AFE_CONN040_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN040_4 = 0x%x\n",
		       AFE_CONN040_4, value);
	regmap_read(afe->regmap, AFE_CONN040_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN040_6 = 0x%x\n",
		       AFE_CONN040_6, value);
	regmap_read(afe->regmap, AFE_CONN041_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN041_0 = 0x%x\n",
		       AFE_CONN041_0, value);
	regmap_read(afe->regmap, AFE_CONN041_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN041_1 = 0x%x\n",
		       AFE_CONN041_1, value);
	regmap_read(afe->regmap, AFE_CONN041_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN041_2 = 0x%x\n",
		       AFE_CONN041_2, value);
	regmap_read(afe->regmap, AFE_CONN041_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN041_4 = 0x%x\n",
		       AFE_CONN041_4, value);
	regmap_read(afe->regmap, AFE_CONN041_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN041_6 = 0x%x\n",
		       AFE_CONN041_6, value);
	regmap_read(afe->regmap, AFE_CONN042_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN042_0 = 0x%x\n",
		       AFE_CONN042_0, value);
	regmap_read(afe->regmap, AFE_CONN042_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN042_1 = 0x%x\n",
		       AFE_CONN042_1, value);
	regmap_read(afe->regmap, AFE_CONN042_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN042_2 = 0x%x\n",
		       AFE_CONN042_2, value);
	regmap_read(afe->regmap, AFE_CONN042_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN042_4 = 0x%x\n",
		       AFE_CONN042_4, value);
	regmap_read(afe->regmap, AFE_CONN042_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN042_6 = 0x%x\n",
		       AFE_CONN042_6, value);
	regmap_read(afe->regmap, AFE_CONN043_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN043_0 = 0x%x\n",
		       AFE_CONN043_0, value);
	regmap_read(afe->regmap, AFE_CONN043_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN043_1 = 0x%x\n",
		       AFE_CONN043_1, value);
	regmap_read(afe->regmap, AFE_CONN043_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN043_2 = 0x%x\n",
		       AFE_CONN043_2, value);
	regmap_read(afe->regmap, AFE_CONN043_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN043_4 = 0x%x\n",
		       AFE_CONN043_4, value);
	regmap_read(afe->regmap, AFE_CONN043_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN043_6 = 0x%x\n",
		       AFE_CONN043_6, value);
	regmap_read(afe->regmap, AFE_CONN044_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN044_0 = 0x%x\n",
		       AFE_CONN044_0, value);
	regmap_read(afe->regmap, AFE_CONN044_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN044_1 = 0x%x\n",
		       AFE_CONN044_1, value);
	regmap_read(afe->regmap, AFE_CONN044_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN044_2 = 0x%x\n",
		       AFE_CONN044_2, value);
	regmap_read(afe->regmap, AFE_CONN044_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN044_4 = 0x%x\n",
		       AFE_CONN044_4, value);
	regmap_read(afe->regmap, AFE_CONN044_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN044_6 = 0x%x\n",
		       AFE_CONN044_6, value);
	regmap_read(afe->regmap, AFE_CONN045_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN045_0 = 0x%x\n",
		       AFE_CONN045_0, value);
	regmap_read(afe->regmap, AFE_CONN045_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN045_1 = 0x%x\n",
		       AFE_CONN045_1, value);
	regmap_read(afe->regmap, AFE_CONN045_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN045_2 = 0x%x\n",
		       AFE_CONN045_2, value);
	regmap_read(afe->regmap, AFE_CONN045_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN045_4 = 0x%x\n",
		       AFE_CONN045_4, value);
	regmap_read(afe->regmap, AFE_CONN045_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN045_6 = 0x%x\n",
		       AFE_CONN045_6, value);
	regmap_read(afe->regmap, AFE_CONN046_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN046_0 = 0x%x\n",
		       AFE_CONN046_0, value);
	regmap_read(afe->regmap, AFE_CONN046_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN046_1 = 0x%x\n",
		       AFE_CONN046_1, value);
	regmap_read(afe->regmap, AFE_CONN046_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN046_2 = 0x%x\n",
		       AFE_CONN046_2, value);
	regmap_read(afe->regmap, AFE_CONN046_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN046_4 = 0x%x\n",
		       AFE_CONN046_4, value);
	regmap_read(afe->regmap, AFE_CONN046_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN046_6 = 0x%x\n",
		       AFE_CONN046_6, value);
	regmap_read(afe->regmap, AFE_CONN047_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN047_0 = 0x%x\n",
		       AFE_CONN047_0, value);
	regmap_read(afe->regmap, AFE_CONN047_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN047_1 = 0x%x\n",
		       AFE_CONN047_1, value);
	regmap_read(afe->regmap, AFE_CONN047_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN047_2 = 0x%x\n",
		       AFE_CONN047_2, value);
	regmap_read(afe->regmap, AFE_CONN047_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN047_4 = 0x%x\n",
		       AFE_CONN047_4, value);
	regmap_read(afe->regmap, AFE_CONN047_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN047_6 = 0x%x\n",
		       AFE_CONN047_6, value);
	regmap_read(afe->regmap, AFE_CONN048_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN048_0 = 0x%x\n",
		       AFE_CONN048_0, value);
	regmap_read(afe->regmap, AFE_CONN048_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN048_1 = 0x%x\n",
		       AFE_CONN048_1, value);
	regmap_read(afe->regmap, AFE_CONN048_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN048_2 = 0x%x\n",
		       AFE_CONN048_2, value);
	regmap_read(afe->regmap, AFE_CONN048_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN048_4 = 0x%x\n",
		       AFE_CONN048_4, value);
	regmap_read(afe->regmap, AFE_CONN048_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN048_6 = 0x%x\n",
		       AFE_CONN048_6, value);
	regmap_read(afe->regmap, AFE_CONN049_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN049_0 = 0x%x\n",
		       AFE_CONN049_0, value);
	regmap_read(afe->regmap, AFE_CONN049_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN049_1 = 0x%x\n",
		       AFE_CONN049_1, value);
	regmap_read(afe->regmap, AFE_CONN049_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN049_2 = 0x%x\n",
		       AFE_CONN049_2, value);
	regmap_read(afe->regmap, AFE_CONN049_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN049_4 = 0x%x\n",
		       AFE_CONN049_4, value);
	regmap_read(afe->regmap, AFE_CONN049_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN049_6 = 0x%x\n",
		       AFE_CONN049_6, value);
	regmap_read(afe->regmap, AFE_CONN050_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN050_0 = 0x%x\n",
		       AFE_CONN050_0, value);
	regmap_read(afe->regmap, AFE_CONN050_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN050_1 = 0x%x\n",
		       AFE_CONN050_1, value);
	regmap_read(afe->regmap, AFE_CONN050_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN050_2 = 0x%x\n",
		       AFE_CONN050_2, value);
	regmap_read(afe->regmap, AFE_CONN050_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN050_4 = 0x%x\n",
		       AFE_CONN050_4, value);
	regmap_read(afe->regmap, AFE_CONN050_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN050_6 = 0x%x\n",
		       AFE_CONN050_6, value);
	regmap_read(afe->regmap, AFE_CONN051_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN051_0 = 0x%x\n",
		       AFE_CONN051_0, value);
	regmap_read(afe->regmap, AFE_CONN051_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN051_1 = 0x%x\n",
		       AFE_CONN051_1, value);
	regmap_read(afe->regmap, AFE_CONN051_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN051_2 = 0x%x\n",
		       AFE_CONN051_2, value);
	regmap_read(afe->regmap, AFE_CONN051_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN051_4 = 0x%x\n",
		       AFE_CONN051_4, value);
	regmap_read(afe->regmap, AFE_CONN051_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN051_6 = 0x%x\n",
		       AFE_CONN051_6, value);
	regmap_read(afe->regmap, AFE_CONN052_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN052_0 = 0x%x\n",
		       AFE_CONN052_0, value);
	regmap_read(afe->regmap, AFE_CONN052_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN052_1 = 0x%x\n",
		       AFE_CONN052_1, value);
	regmap_read(afe->regmap, AFE_CONN052_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN052_2 = 0x%x\n",
		       AFE_CONN052_2, value);
	regmap_read(afe->regmap, AFE_CONN052_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN052_4 = 0x%x\n",
		       AFE_CONN052_4, value);
	regmap_read(afe->regmap, AFE_CONN052_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN052_6 = 0x%x\n",
		       AFE_CONN052_6, value);
	regmap_read(afe->regmap, AFE_CONN053_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN053_0 = 0x%x\n",
		       AFE_CONN053_0, value);
	regmap_read(afe->regmap, AFE_CONN053_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN053_1 = 0x%x\n",
		       AFE_CONN053_1, value);
	regmap_read(afe->regmap, AFE_CONN053_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN053_2 = 0x%x\n",
		       AFE_CONN053_2, value);
	regmap_read(afe->regmap, AFE_CONN053_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN053_4 = 0x%x\n",
		       AFE_CONN053_4, value);
	regmap_read(afe->regmap, AFE_CONN053_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN053_6 = 0x%x\n",
		       AFE_CONN053_6, value);
	regmap_read(afe->regmap, AFE_CONN054_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN054_0 = 0x%x\n",
		       AFE_CONN054_0, value);
	regmap_read(afe->regmap, AFE_CONN054_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN054_1 = 0x%x\n",
		       AFE_CONN054_1, value);
	regmap_read(afe->regmap, AFE_CONN054_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN054_2 = 0x%x\n",
		       AFE_CONN054_2, value);
	regmap_read(afe->regmap, AFE_CONN054_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN054_4 = 0x%x\n",
		       AFE_CONN054_4, value);
	regmap_read(afe->regmap, AFE_CONN054_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN054_6 = 0x%x\n",
		       AFE_CONN054_6, value);
	regmap_read(afe->regmap, AFE_CONN055_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN055_0 = 0x%x\n",
		       AFE_CONN055_0, value);
	regmap_read(afe->regmap, AFE_CONN055_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN055_1 = 0x%x\n",
		       AFE_CONN055_1, value);
	regmap_read(afe->regmap, AFE_CONN055_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN055_2 = 0x%x\n",
		       AFE_CONN055_2, value);
	regmap_read(afe->regmap, AFE_CONN055_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN055_4 = 0x%x\n",
		       AFE_CONN055_4, value);
	regmap_read(afe->regmap, AFE_CONN055_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN055_6 = 0x%x\n",
		       AFE_CONN055_6, value);
	regmap_read(afe->regmap, AFE_CONN056_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN056_0 = 0x%x\n",
		       AFE_CONN056_0, value);
	regmap_read(afe->regmap, AFE_CONN056_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN056_1 = 0x%x\n",
		       AFE_CONN056_1, value);
	regmap_read(afe->regmap, AFE_CONN056_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN056_2 = 0x%x\n",
		       AFE_CONN056_2, value);
	regmap_read(afe->regmap, AFE_CONN056_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN056_4 = 0x%x\n",
		       AFE_CONN056_4, value);
	regmap_read(afe->regmap, AFE_CONN056_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN056_6 = 0x%x\n",
		       AFE_CONN056_6, value);
	regmap_read(afe->regmap, AFE_CONN057_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN057_0 = 0x%x\n",
		       AFE_CONN057_0, value);
	regmap_read(afe->regmap, AFE_CONN057_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN057_1 = 0x%x\n",
		       AFE_CONN057_1, value);
	regmap_read(afe->regmap, AFE_CONN057_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN057_2 = 0x%x\n",
		       AFE_CONN057_2, value);
	regmap_read(afe->regmap, AFE_CONN057_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN057_4 = 0x%x\n",
		       AFE_CONN057_4, value);
	regmap_read(afe->regmap, AFE_CONN057_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN057_6 = 0x%x\n",
		       AFE_CONN057_6, value);
	regmap_read(afe->regmap, AFE_CONN058_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN058_0 = 0x%x\n",
		       AFE_CONN058_0, value);
	regmap_read(afe->regmap, AFE_CONN058_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN058_1 = 0x%x\n",
		       AFE_CONN058_1, value);
	regmap_read(afe->regmap, AFE_CONN058_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN058_2 = 0x%x\n",
		       AFE_CONN058_2, value);
	regmap_read(afe->regmap, AFE_CONN058_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN058_4 = 0x%x\n",
		       AFE_CONN058_4, value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN058_6 = 0x%x\n",
		       AFE_CONN058_6, value);
	regmap_read(afe->regmap, AFE_CONN059_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN059_0 = 0x%x\n",
		       AFE_CONN059_0, value);
	regmap_read(afe->regmap, AFE_CONN059_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN059_1 = 0x%x\n",
		       AFE_CONN059_1, value);
	regmap_read(afe->regmap, AFE_CONN059_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN059_2 = 0x%x\n",
		       AFE_CONN059_2, value);
	regmap_read(afe->regmap, AFE_CONN059_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN059_4 = 0x%x\n",
		       AFE_CONN059_4, value);
	regmap_read(afe->regmap, AFE_CONN059_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN059_6 = 0x%x\n",
		       AFE_CONN059_6, value);
	regmap_read(afe->regmap, AFE_CONN060_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN060_0 = 0x%x\n",
		       AFE_CONN060_0, value);
	regmap_read(afe->regmap, AFE_CONN060_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN060_1 = 0x%x\n",
		       AFE_CONN060_1, value);
	regmap_read(afe->regmap, AFE_CONN060_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN060_2 = 0x%x\n",
		       AFE_CONN060_2, value);
	regmap_read(afe->regmap, AFE_CONN060_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN060_4 = 0x%x\n",
		       AFE_CONN060_4, value);
	regmap_read(afe->regmap, AFE_CONN060_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN060_6 = 0x%x\n",
		       AFE_CONN060_6, value);
	regmap_read(afe->regmap, AFE_CONN061_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN061_0 = 0x%x\n",
		       AFE_CONN061_0, value);
	regmap_read(afe->regmap, AFE_CONN061_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN061_1 = 0x%x\n",
		       AFE_CONN061_1, value);
	regmap_read(afe->regmap, AFE_CONN061_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN061_2 = 0x%x\n",
		       AFE_CONN061_2, value);
	regmap_read(afe->regmap, AFE_CONN061_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN061_4 = 0x%x\n",
		       AFE_CONN061_4, value);
	regmap_read(afe->regmap, AFE_CONN061_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN061_6 = 0x%x\n",
		       AFE_CONN061_6, value);
	regmap_read(afe->regmap, AFE_CONN062_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN062_0 = 0x%x\n",
		       AFE_CONN062_0, value);
	regmap_read(afe->regmap, AFE_CONN062_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN062_1 = 0x%x\n",
		       AFE_CONN062_1, value);
	regmap_read(afe->regmap, AFE_CONN062_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN062_2 = 0x%x\n",
		       AFE_CONN062_2, value);
	regmap_read(afe->regmap, AFE_CONN062_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN062_4 = 0x%x\n",
		       AFE_CONN062_4, value);
	regmap_read(afe->regmap, AFE_CONN062_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN062_6 = 0x%x\n",
		       AFE_CONN062_6, value);
	regmap_read(afe->regmap, AFE_CONN063_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN063_0 = 0x%x\n",
		       AFE_CONN063_0, value);
	regmap_read(afe->regmap, AFE_CONN063_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN063_1 = 0x%x\n",
		       AFE_CONN063_1, value);
	regmap_read(afe->regmap, AFE_CONN063_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN063_2 = 0x%x\n",
		       AFE_CONN063_2, value);
	regmap_read(afe->regmap, AFE_CONN063_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN063_4 = 0x%x\n",
		       AFE_CONN063_4, value);
	regmap_read(afe->regmap, AFE_CONN063_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN063_6 = 0x%x\n",
		       AFE_CONN063_6, value);
	regmap_read(afe->regmap, AFE_CONN066_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN066_0 = 0x%x\n",
		       AFE_CONN066_0, value);
	regmap_read(afe->regmap, AFE_CONN066_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN066_1 = 0x%x\n",
		       AFE_CONN066_1, value);
	regmap_read(afe->regmap, AFE_CONN066_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN066_2 = 0x%x\n",
		       AFE_CONN066_2, value);
	regmap_read(afe->regmap, AFE_CONN066_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN066_4 = 0x%x\n",
		       AFE_CONN066_4, value);
	regmap_read(afe->regmap, AFE_CONN066_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN066_6 = 0x%x\n",
		       AFE_CONN066_6, value);
	regmap_read(afe->regmap, AFE_CONN067_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN067_0 = 0x%x\n",
		       AFE_CONN067_0, value);
	regmap_read(afe->regmap, AFE_CONN067_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN067_1 = 0x%x\n",
		       AFE_CONN067_1, value);
	regmap_read(afe->regmap, AFE_CONN067_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN067_2 = 0x%x\n",
		       AFE_CONN067_2, value);
	regmap_read(afe->regmap, AFE_CONN067_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN067_4 = 0x%x\n",
		       AFE_CONN067_4, value);
	regmap_read(afe->regmap, AFE_CONN067_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN067_6 = 0x%x\n",
		       AFE_CONN067_6, value);
	regmap_read(afe->regmap, AFE_CONN068_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN068_0 = 0x%x\n",
		       AFE_CONN068_0, value);
	regmap_read(afe->regmap, AFE_CONN068_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN068_1 = 0x%x\n",
		       AFE_CONN068_1, value);
	regmap_read(afe->regmap, AFE_CONN068_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN068_2 = 0x%x\n",
		       AFE_CONN068_2, value);
	regmap_read(afe->regmap, AFE_CONN068_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN068_4 = 0x%x\n",
		       AFE_CONN068_4, value);
	regmap_read(afe->regmap, AFE_CONN068_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN068_6 = 0x%x\n",
		       AFE_CONN068_6, value);
	regmap_read(afe->regmap, AFE_CONN069_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN069_0 = 0x%x\n",
		       AFE_CONN069_0, value);
	regmap_read(afe->regmap, AFE_CONN069_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN069_1 = 0x%x\n",
		       AFE_CONN069_1, value);
	regmap_read(afe->regmap, AFE_CONN069_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN069_2 = 0x%x\n",
		       AFE_CONN069_2, value);
	regmap_read(afe->regmap, AFE_CONN069_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN069_4 = 0x%x\n",
		       AFE_CONN069_4, value);
	regmap_read(afe->regmap, AFE_CONN069_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN069_6 = 0x%x\n",
		       AFE_CONN069_6, value);
	regmap_read(afe->regmap, AFE_CONN096_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN096_0 = 0x%x\n",
		       AFE_CONN096_0, value);
	regmap_read(afe->regmap, AFE_CONN096_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN096_1 = 0x%x\n",
		       AFE_CONN096_1, value);
	regmap_read(afe->regmap, AFE_CONN096_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN096_2 = 0x%x\n",
		       AFE_CONN096_2, value);
	regmap_read(afe->regmap, AFE_CONN096_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN096_4 = 0x%x\n",
		       AFE_CONN096_4, value);
	regmap_read(afe->regmap, AFE_CONN096_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN096_6 = 0x%x\n",
		       AFE_CONN096_6, value);
	regmap_read(afe->regmap, AFE_CONN097_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN097_0 = 0x%x\n",
		       AFE_CONN097_0, value);
	regmap_read(afe->regmap, AFE_CONN097_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN097_1 = 0x%x\n",
		       AFE_CONN097_1, value);
	regmap_read(afe->regmap, AFE_CONN097_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN097_2 = 0x%x\n",
		       AFE_CONN097_2, value);
	regmap_read(afe->regmap, AFE_CONN097_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN097_4 = 0x%x\n",
		       AFE_CONN097_4, value);
	regmap_read(afe->regmap, AFE_CONN097_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN097_6 = 0x%x\n",
		       AFE_CONN097_6, value);
	regmap_read(afe->regmap, AFE_CONN098_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN098_0 = 0x%x\n",
		       AFE_CONN098_0, value);
	regmap_read(afe->regmap, AFE_CONN098_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN098_1 = 0x%x\n",
		       AFE_CONN098_1, value);
	regmap_read(afe->regmap, AFE_CONN098_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN098_2 = 0x%x\n",
		       AFE_CONN098_2, value);
	regmap_read(afe->regmap, AFE_CONN098_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN098_4 = 0x%x\n",
		       AFE_CONN098_4, value);
	regmap_read(afe->regmap, AFE_CONN098_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN098_6 = 0x%x\n",
		       AFE_CONN098_6, value);
	regmap_read(afe->regmap, AFE_CONN099_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN099_0 = 0x%x\n",
		       AFE_CONN099_0, value);
	regmap_read(afe->regmap, AFE_CONN099_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN099_1 = 0x%x\n",
		       AFE_CONN099_1, value);
	regmap_read(afe->regmap, AFE_CONN099_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN099_2 = 0x%x\n",
		       AFE_CONN099_2, value);
	regmap_read(afe->regmap, AFE_CONN099_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN099_4 = 0x%x\n",
		       AFE_CONN099_4, value);
	regmap_read(afe->regmap, AFE_CONN099_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN099_6 = 0x%x\n",
		       AFE_CONN099_6, value);
	regmap_read(afe->regmap, AFE_CONN100_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN100_0 = 0x%x\n",
		       AFE_CONN100_0, value);
	regmap_read(afe->regmap, AFE_CONN100_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN100_1 = 0x%x\n",
		       AFE_CONN100_1, value);
	regmap_read(afe->regmap, AFE_CONN100_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN100_2 = 0x%x\n",
		       AFE_CONN100_2, value);
	regmap_read(afe->regmap, AFE_CONN100_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN100_4 = 0x%x\n",
		       AFE_CONN100_4, value);
	regmap_read(afe->regmap, AFE_CONN100_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN100_6 = 0x%x\n",
		       AFE_CONN100_6, value);
	regmap_read(afe->regmap, AFE_CONN108_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN108_1 = 0x%x\n",
		       AFE_CONN108_1, value);
		regmap_read(afe->regmap, AFE_CONN109_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN109_1 = 0x%x\n",
		       AFE_CONN109_1, value);
	regmap_read(afe->regmap, AFE_CONN110_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN110_0 = 0x%x\n",
		       AFE_CONN110_0, value);
	regmap_read(afe->regmap, AFE_CONN110_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN110_1 = 0x%x\n",
		       AFE_CONN110_1, value);
	regmap_read(afe->regmap, AFE_CONN110_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN110_2 = 0x%x\n",
		       AFE_CONN110_2, value);
	regmap_read(afe->regmap, AFE_CONN110_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN110_4 = 0x%x\n",
		       AFE_CONN110_4, value);
	regmap_read(afe->regmap, AFE_CONN110_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN110_6 = 0x%x\n",
		       AFE_CONN110_6, value);
	regmap_read(afe->regmap, AFE_CONN111_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN111_0 = 0x%x\n",
		       AFE_CONN111_0, value);
	regmap_read(afe->regmap, AFE_CONN111_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN111_1 = 0x%x\n",
		       AFE_CONN111_1, value);
	regmap_read(afe->regmap, AFE_CONN111_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN111_2 = 0x%x\n",
		       AFE_CONN111_2, value);
	regmap_read(afe->regmap, AFE_CONN111_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN111_4 = 0x%x\n",
		       AFE_CONN111_4, value);
	regmap_read(afe->regmap, AFE_CONN111_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN111_6 = 0x%x\n",
		       AFE_CONN111_6, value);
	regmap_read(afe->regmap, AFE_CONN116_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN116_0 = 0x%x\n",
		       AFE_CONN116_0, value);
	regmap_read(afe->regmap, AFE_CONN116_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN116_1 = 0x%x\n",
		       AFE_CONN116_1, value);
	regmap_read(afe->regmap, AFE_CONN116_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN116_2 = 0x%x\n",
		       AFE_CONN116_2, value);
	regmap_read(afe->regmap, AFE_CONN116_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN116_4 = 0x%x\n",
		       AFE_CONN116_4, value);
	regmap_read(afe->regmap, AFE_CONN116_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN116_6 = 0x%x\n",
		       AFE_CONN116_6, value);
	regmap_read(afe->regmap, AFE_CONN117_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN117_0 = 0x%x\n",
		       AFE_CONN117_0, value);
	regmap_read(afe->regmap, AFE_CONN117_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN117_1 = 0x%x\n",
		       AFE_CONN117_1, value);
	regmap_read(afe->regmap, AFE_CONN117_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN117_2 = 0x%x\n",
		       AFE_CONN117_2, value);
	regmap_read(afe->regmap, AFE_CONN117_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN117_4 = 0x%x\n",
		       AFE_CONN117_4, value);
	regmap_read(afe->regmap, AFE_CONN117_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN117_6 = 0x%x\n",
		       AFE_CONN117_6, value);
	regmap_read(afe->regmap, AFE_CONN118_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN118_0 = 0x%x\n",
		       AFE_CONN118_0, value);
	regmap_read(afe->regmap, AFE_CONN118_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN118_1 = 0x%x\n",
		       AFE_CONN118_1, value);
	regmap_read(afe->regmap, AFE_CONN118_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN118_2 = 0x%x\n",
		       AFE_CONN118_2, value);
	regmap_read(afe->regmap, AFE_CONN118_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN118_4 = 0x%x\n",
		       AFE_CONN118_4, value);
	regmap_read(afe->regmap, AFE_CONN118_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN118_6 = 0x%x\n",
		       AFE_CONN118_6, value);
	regmap_read(afe->regmap, AFE_CONN119_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN119_0 = 0x%x\n",
		       AFE_CONN119_0, value);
	regmap_read(afe->regmap, AFE_CONN119_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN119_1 = 0x%x\n",
		       AFE_CONN119_1, value);
	regmap_read(afe->regmap, AFE_CONN119_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN119_2 = 0x%x\n",
		       AFE_CONN119_2, value);
	regmap_read(afe->regmap, AFE_CONN119_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN119_4 = 0x%x\n",
		       AFE_CONN119_4, value);
	regmap_read(afe->regmap, AFE_CONN119_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN119_6 = 0x%x\n",
		       AFE_CONN119_6, value);
	regmap_read(afe->regmap, AFE_CONN120_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN120_0 = 0x%x\n",
		       AFE_CONN120_0, value);
	regmap_read(afe->regmap, AFE_CONN120_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN120_1 = 0x%x\n",
		       AFE_CONN120_1, value);
	regmap_read(afe->regmap, AFE_CONN120_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN120_2 = 0x%x\n",
		       AFE_CONN120_2, value);
	regmap_read(afe->regmap, AFE_CONN120_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN120_4 = 0x%x\n",
		       AFE_CONN120_4, value);
	regmap_read(afe->regmap, AFE_CONN120_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN120_6 = 0x%x\n",
		       AFE_CONN120_6, value);
	regmap_read(afe->regmap, AFE_CONN121_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN121_0 = 0x%x\n",
		       AFE_CONN121_0, value);
	regmap_read(afe->regmap, AFE_CONN121_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN121_1 = 0x%x\n",
		       AFE_CONN121_1, value);
	regmap_read(afe->regmap, AFE_CONN121_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN121_2 = 0x%x\n",
		       AFE_CONN121_2, value);
	regmap_read(afe->regmap, AFE_CONN121_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN121_4 = 0x%x\n",
		       AFE_CONN121_4, value);
	regmap_read(afe->regmap, AFE_CONN121_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN121_6 = 0x%x\n",
		       AFE_CONN121_6, value);
	regmap_read(afe->regmap, AFE_CONN122_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN122_0 = 0x%x\n",
		       AFE_CONN122_0, value);
	regmap_read(afe->regmap, AFE_CONN122_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN122_1 = 0x%x\n",
		       AFE_CONN122_1, value);
	regmap_read(afe->regmap, AFE_CONN122_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN122_2 = 0x%x\n",
		       AFE_CONN122_2, value);
	regmap_read(afe->regmap, AFE_CONN122_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN122_4 = 0x%x\n",
		       AFE_CONN122_4, value);
	regmap_read(afe->regmap, AFE_CONN122_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN122_6 = 0x%x\n",
		       AFE_CONN122_6, value);
	regmap_read(afe->regmap, AFE_CONN123_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN123_0 = 0x%x\n",
		       AFE_CONN123_0, value);
	regmap_read(afe->regmap, AFE_CONN123_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN123_1 = 0x%x\n",
		       AFE_CONN123_1, value);
	regmap_read(afe->regmap, AFE_CONN123_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN123_2 = 0x%x\n",
		       AFE_CONN123_2, value);
	regmap_read(afe->regmap, AFE_CONN123_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN123_4 = 0x%x\n",
		       AFE_CONN123_4, value);
	regmap_read(afe->regmap, AFE_CONN123_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN123_6 = 0x%x\n",
		       AFE_CONN123_6, value);
	regmap_read(afe->regmap, AFE_CONN180_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN180_0 = 0x%x\n",
		       AFE_CONN180_0, value);
	regmap_read(afe->regmap, AFE_CONN180_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN180_1 = 0x%x\n",
		       AFE_CONN180_1, value);
	regmap_read(afe->regmap, AFE_CONN180_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN180_2 = 0x%x\n",
		       AFE_CONN180_2, value);
	regmap_read(afe->regmap, AFE_CONN180_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN180_4 = 0x%x\n",
		       AFE_CONN180_4, value);
	regmap_read(afe->regmap, AFE_CONN180_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN180_6 = 0x%x\n",
		       AFE_CONN180_6, value);
	regmap_read(afe->regmap, AFE_CONN181_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN181_0 = 0x%x\n",
		       AFE_CONN181_0, value);
	regmap_read(afe->regmap, AFE_CONN181_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN181_1 = 0x%x\n",
		       AFE_CONN181_1, value);
	regmap_read(afe->regmap, AFE_CONN181_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN181_2 = 0x%x\n",
		       AFE_CONN181_2, value);
	regmap_read(afe->regmap, AFE_CONN181_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN181_4 = 0x%x\n",
		       AFE_CONN181_4, value);
	regmap_read(afe->regmap, AFE_CONN181_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN181_6 = 0x%x\n",
		       AFE_CONN181_6, value);
	regmap_read(afe->regmap, AFE_CONN182_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN182_0 = 0x%x\n",
		       AFE_CONN182_0, value);
	regmap_read(afe->regmap, AFE_CONN182_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN182_1 = 0x%x\n",
		       AFE_CONN182_1, value);
	regmap_read(afe->regmap, AFE_CONN182_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN182_2 = 0x%x\n",
		       AFE_CONN182_2, value);
	regmap_read(afe->regmap, AFE_CONN182_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN182_4 = 0x%x\n",
		       AFE_CONN182_4, value);
	regmap_read(afe->regmap, AFE_CONN182_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN182_6 = 0x%x\n",
		       AFE_CONN182_6, value);
	regmap_read(afe->regmap, AFE_CONN183_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN183_0 = 0x%x\n",
		       AFE_CONN183_0, value);
	regmap_read(afe->regmap, AFE_CONN183_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN183_1 = 0x%x\n",
		       AFE_CONN183_1, value);
	regmap_read(afe->regmap, AFE_CONN183_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN183_2 = 0x%x\n",
		       AFE_CONN183_2, value);
	regmap_read(afe->regmap, AFE_CONN183_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN183_4 = 0x%x\n",
		       AFE_CONN183_4, value);
	regmap_read(afe->regmap, AFE_CONN183_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN183_6 = 0x%x\n",
		       AFE_CONN183_6, value);
	regmap_read(afe->regmap, AFE_CONN184_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN184_0 = 0x%x\n",
		       AFE_CONN184_0, value);
	regmap_read(afe->regmap, AFE_CONN184_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN184_1 = 0x%x\n",
		       AFE_CONN184_1, value);
	regmap_read(afe->regmap, AFE_CONN184_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN184_2 = 0x%x\n",
		       AFE_CONN184_2, value);
	regmap_read(afe->regmap, AFE_CONN184_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN184_4 = 0x%x\n",
		       AFE_CONN184_4, value);
	regmap_read(afe->regmap, AFE_CONN184_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN184_6 = 0x%x\n",
		       AFE_CONN184_6, value);
	regmap_read(afe->regmap, AFE_CONN185_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN185_0 = 0x%x\n",
		       AFE_CONN185_0, value);
	regmap_read(afe->regmap, AFE_CONN185_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN185_1 = 0x%x\n",
		       AFE_CONN185_1, value);
	regmap_read(afe->regmap, AFE_CONN185_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN185_2 = 0x%x\n",
		       AFE_CONN185_2, value);
	regmap_read(afe->regmap, AFE_CONN185_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN185_4 = 0x%x\n",
		       AFE_CONN185_4, value);
	regmap_read(afe->regmap, AFE_CONN185_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN185_6 = 0x%x\n",
		       AFE_CONN185_6, value);
#endif
	regmap_read(afe->regmap, AFE_CONN_MON_CFG, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN_MON_CFG = 0x%x\n",
		       AFE_CONN_MON_CFG, value);
	regmap_read(afe->regmap, AFE_CONN_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN_MON0 = 0x%x\n",
		       AFE_CONN_MON0, value);
	regmap_read(afe->regmap, AFE_CONN_MON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN_MON1 = 0x%x\n",
		       AFE_CONN_MON1, value);
	regmap_read(afe->regmap, AFE_CONN_MON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN_MON2 = 0x%x\n",
		       AFE_CONN_MON2, value);
	regmap_read(afe->regmap, AFE_CONN_MON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN_MON3 = 0x%x\n",
		       AFE_CONN_MON3, value);
	regmap_read(afe->regmap, AFE_CONN_MON4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN_MON4 = 0x%x\n",
		       AFE_CONN_MON4, value);
	regmap_read(afe->regmap, AFE_CONN_MON5, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN_MON5 = 0x%x\n",
		       AFE_CONN_MON5, value);
	regmap_read(afe->regmap, AFE_CONN_RS_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN_RS_0 = 0x%x\n",
		       AFE_CONN_RS_0, value);
	regmap_read(afe->regmap, AFE_CONN_RS_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN_RS_1 = 0x%x\n",
		       AFE_CONN_RS_1, value);
	regmap_read(afe->regmap, AFE_CONN_RS_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN_RS_2 = 0x%x\n",
		       AFE_CONN_RS_2, value);
	regmap_read(afe->regmap, AFE_CONN_RS_3, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN_RS_3 = 0x%x\n",
		       AFE_CONN_RS_3, value);
	regmap_read(afe->regmap, AFE_CONN_RS_5, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN_RS_5 = 0x%x\n",
		       AFE_CONN_RS_5, value);
	regmap_read(afe->regmap, AFE_CONN_DI_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN_DI_0 = 0x%x\n",
		       AFE_CONN_DI_0, value);
	regmap_read(afe->regmap, AFE_CONN_DI_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN_DI_1 = 0x%x\n",
		       AFE_CONN_DI_1, value);
	regmap_read(afe->regmap, AFE_CONN_DI_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN_DI_2 = 0x%x\n",
		       AFE_CONN_DI_2, value);
	regmap_read(afe->regmap, AFE_CONN_DI_3, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN_DI_3 = 0x%x\n",
		       AFE_CONN_DI_3, value);
	regmap_read(afe->regmap, AFE_CONN_DI_5, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN_DI_5 = 0x%x\n",
		       AFE_CONN_DI_5, value);
	regmap_read(afe->regmap, AFE_CONN_16BIT_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN_16BIT_0 = 0x%x\n",
		       AFE_CONN_16BIT_0, value);
	regmap_read(afe->regmap, AFE_CONN_16BIT_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN_16BIT_1 = 0x%x\n",
		       AFE_CONN_16BIT_1, value);
	regmap_read(afe->regmap, AFE_CONN_16BIT_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN_16BIT_2 = 0x%x\n",
		       AFE_CONN_16BIT_2, value);
	regmap_read(afe->regmap, AFE_CONN_16BIT_3, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN_16BIT_3 = 0x%x\n",
		       AFE_CONN_16BIT_3, value);
	regmap_read(afe->regmap, AFE_CONN_16BIT_5, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN_16BIT_5 = 0x%x\n",
		       AFE_CONN_16BIT_5, value);
	regmap_read(afe->regmap, AFE_CONN_24BIT_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN_24BIT_0 = 0x%x\n",
		       AFE_CONN_24BIT_0, value);
	regmap_read(afe->regmap, AFE_CONN_24BIT_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN_24BIT_1 = 0x%x\n",
		       AFE_CONN_24BIT_1, value);
	regmap_read(afe->regmap, AFE_CONN_24BIT_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN_24BIT_2 = 0x%x\n",
		       AFE_CONN_24BIT_2, value);
	regmap_read(afe->regmap, AFE_CONN_24BIT_3, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN_24BIT_3 = 0x%x\n",
		       AFE_CONN_24BIT_3, value);
	regmap_read(afe->regmap, AFE_CONN_24BIT_5, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONN_24BIT_5 = 0x%x\n",
		       AFE_CONN_24BIT_5, value);
	regmap_read(afe->regmap, AFE_CBIP_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CBIP_CFG0 = 0x%x\n",
		       AFE_CBIP_CFG0, value);
	regmap_read(afe->regmap, AFE_CBIP_SLV_DECODER_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CBIP_SLV_DECODER_MON0 = 0x%x\n",
		       AFE_CBIP_SLV_DECODER_MON0, value);
	regmap_read(afe->regmap, AFE_CBIP_SLV_DECODER_MON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CBIP_SLV_DECODER_MON1 = 0x%x\n",
		       AFE_CBIP_SLV_DECODER_MON1, value);
	regmap_read(afe->regmap, AFE_CBIP_SLV_MUX_MON_CFG, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CBIP_SLV_MUX_MON_CFG = 0x%x\n",
		       AFE_CBIP_SLV_MUX_MON_CFG, value);
	regmap_read(afe->regmap, AFE_CBIP_SLV_MUX_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CBIP_SLV_MUX_MON0 = 0x%x\n",
		       AFE_CBIP_SLV_MUX_MON0, value);
	regmap_read(afe->regmap, AFE_CBIP_SLV_MUX_MON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CBIP_SLV_MUX_MON1 = 0x%x\n",
		       AFE_CBIP_SLV_MUX_MON1, value);
	regmap_read(afe->regmap, AFE_MEMIF_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_MEMIF_CON0 = 0x%x\n",
		       AFE_MEMIF_CON0, value);
	regmap_read(afe->regmap, AFE_MEMIF_ONE_HEART, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_MEMIF_ONE_HEART = 0x%x\n",
		       AFE_MEMIF_ONE_HEART, value);
	regmap_read(afe->regmap, AFE_DL0_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL0_BASE_MSB = 0x%x\n",
		       AFE_DL0_BASE_MSB, value);
	regmap_read(afe->regmap, AFE_DL0_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL0_BASE = 0x%x\n",
		       AFE_DL0_BASE, value);
	regmap_read(afe->regmap, AFE_DL0_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL0_CUR_MSB = 0x%x\n",
		       AFE_DL0_CUR_MSB, value);
	regmap_read(afe->regmap, AFE_DL0_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL0_CUR = 0x%x\n",
		       AFE_DL0_CUR, value);
	regmap_read(afe->regmap, AFE_DL0_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL0_END_MSB = 0x%x\n",
		       AFE_DL0_END_MSB, value);
	regmap_read(afe->regmap, AFE_DL0_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL0_END = 0x%x\n",
		       AFE_DL0_END, value);
	regmap_read(afe->regmap, AFE_DL0_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL0_RCH_MON = 0x%x\n",
		       AFE_DL0_RCH_MON, value);
	regmap_read(afe->regmap, AFE_DL0_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL0_LCH_MON = 0x%x\n",
		       AFE_DL0_LCH_MON, value);
	regmap_read(afe->regmap, AFE_DL0_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL0_CON0 = 0x%x\n",
		       AFE_DL0_CON0, value);
	regmap_read(afe->regmap, AFE_DL1_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL1_BASE_MSB = 0x%x\n",
		       AFE_DL1_BASE_MSB, value);
	regmap_read(afe->regmap, AFE_DL1_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL1_BASE = 0x%x\n",
		       AFE_DL1_BASE, value);
	regmap_read(afe->regmap, AFE_DL1_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL1_CUR_MSB = 0x%x\n",
		       AFE_DL1_CUR_MSB, value);
	regmap_read(afe->regmap, AFE_DL1_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL1_CUR = 0x%x\n",
		       AFE_DL1_CUR, value);
	regmap_read(afe->regmap, AFE_DL1_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL1_END_MSB = 0x%x\n",
		       AFE_DL1_END_MSB, value);
	regmap_read(afe->regmap, AFE_DL1_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL1_END = 0x%x\n",
		       AFE_DL1_END, value);
	regmap_read(afe->regmap, AFE_DL1_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL1_RCH_MON = 0x%x\n",
		       AFE_DL1_RCH_MON, value);
	regmap_read(afe->regmap, AFE_DL1_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL1_LCH_MON = 0x%x\n",
		       AFE_DL1_LCH_MON, value);
	regmap_read(afe->regmap, AFE_DL1_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL1_CON0 = 0x%x\n",
		       AFE_DL1_CON0, value);
	regmap_read(afe->regmap, AFE_DL2_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL2_BASE_MSB = 0x%x\n",
		       AFE_DL2_BASE_MSB, value);
	regmap_read(afe->regmap, AFE_DL2_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL2_BASE = 0x%x\n",
		       AFE_DL2_BASE, value);
	regmap_read(afe->regmap, AFE_DL2_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL2_CUR_MSB = 0x%x\n",
		       AFE_DL2_CUR_MSB, value);
	regmap_read(afe->regmap, AFE_DL2_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL2_CUR = 0x%x\n",
		       AFE_DL2_CUR, value);
	regmap_read(afe->regmap, AFE_DL2_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL2_END_MSB = 0x%x\n",
		       AFE_DL2_END_MSB, value);
	regmap_read(afe->regmap, AFE_DL2_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL2_END = 0x%x\n",
		       AFE_DL2_END, value);
	regmap_read(afe->regmap, AFE_DL2_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL2_RCH_MON = 0x%x\n",
		       AFE_DL2_RCH_MON, value);
	regmap_read(afe->regmap, AFE_DL2_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL2_LCH_MON = 0x%x\n",
		       AFE_DL2_LCH_MON, value);
	regmap_read(afe->regmap, AFE_DL2_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL2_CON0 = 0x%x\n",
		       AFE_DL2_CON0, value);
	regmap_read(afe->regmap, AFE_DL3_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL3_BASE_MSB = 0x%x\n",
		       AFE_DL3_BASE_MSB, value);
	regmap_read(afe->regmap, AFE_DL3_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL3_BASE = 0x%x\n",
		       AFE_DL3_BASE, value);
	regmap_read(afe->regmap, AFE_DL3_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL3_CUR_MSB = 0x%x\n",
		       AFE_DL3_CUR_MSB, value);
	regmap_read(afe->regmap, AFE_DL3_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL3_CUR = 0x%x\n",
		       AFE_DL3_CUR, value);
	regmap_read(afe->regmap, AFE_DL3_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL3_END_MSB = 0x%x\n",
		       AFE_DL3_END_MSB, value);
	regmap_read(afe->regmap, AFE_DL3_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL3_END = 0x%x\n",
		       AFE_DL3_END, value);
	regmap_read(afe->regmap, AFE_DL3_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL3_RCH_MON = 0x%x\n",
		       AFE_DL3_RCH_MON, value);
	regmap_read(afe->regmap, AFE_DL3_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL3_LCH_MON = 0x%x\n",
		       AFE_DL3_LCH_MON, value);
	regmap_read(afe->regmap, AFE_DL3_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL3_CON0 = 0x%x\n",
		       AFE_DL3_CON0, value);
	regmap_read(afe->regmap, AFE_DL4_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL4_BASE_MSB = 0x%x\n",
		       AFE_DL4_BASE_MSB, value);
	regmap_read(afe->regmap, AFE_DL4_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL4_BASE = 0x%x\n",
		       AFE_DL4_BASE, value);
	regmap_read(afe->regmap, AFE_DL4_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL4_CUR_MSB = 0x%x\n",
		       AFE_DL4_CUR_MSB, value);
	regmap_read(afe->regmap, AFE_DL4_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL4_CUR = 0x%x\n",
		       AFE_DL4_CUR, value);
	regmap_read(afe->regmap, AFE_DL4_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL4_END_MSB = 0x%x\n",
		       AFE_DL4_END_MSB, value);
	regmap_read(afe->regmap, AFE_DL4_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL4_END = 0x%x\n",
		       AFE_DL4_END, value);
	regmap_read(afe->regmap, AFE_DL4_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL4_RCH_MON = 0x%x\n",
		       AFE_DL4_RCH_MON, value);
	regmap_read(afe->regmap, AFE_DL4_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL4_LCH_MON = 0x%x\n",
		       AFE_DL4_LCH_MON, value);
	regmap_read(afe->regmap, AFE_DL4_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL4_CON0 = 0x%x\n",
		       AFE_DL4_CON0, value);
	regmap_read(afe->regmap, AFE_DL5_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL5_BASE_MSB = 0x%x\n",
		       AFE_DL5_BASE_MSB, value);
	regmap_read(afe->regmap, AFE_DL5_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL5_BASE = 0x%x\n",
		       AFE_DL5_BASE, value);
	regmap_read(afe->regmap, AFE_DL5_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL5_CUR_MSB = 0x%x\n",
		       AFE_DL5_CUR_MSB, value);
	regmap_read(afe->regmap, AFE_DL5_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL5_CUR = 0x%x\n",
		       AFE_DL5_CUR, value);
	regmap_read(afe->regmap, AFE_DL5_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL5_END_MSB = 0x%x\n",
		       AFE_DL5_END_MSB, value);
	regmap_read(afe->regmap, AFE_DL5_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL5_END = 0x%x\n",
		       AFE_DL5_END, value);
	regmap_read(afe->regmap, AFE_DL5_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL5_RCH_MON = 0x%x\n",
		       AFE_DL5_RCH_MON, value);
	regmap_read(afe->regmap, AFE_DL5_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL5_LCH_MON = 0x%x\n",
		       AFE_DL5_LCH_MON, value);
	regmap_read(afe->regmap, AFE_DL5_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL5_CON0 = 0x%x\n",
		       AFE_DL5_CON0, value);
	regmap_read(afe->regmap, AFE_DL6_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL6_BASE_MSB = 0x%x\n",
		       AFE_DL6_BASE_MSB, value);
	regmap_read(afe->regmap, AFE_DL6_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL6_BASE = 0x%x\n",
		       AFE_DL6_BASE, value);
	regmap_read(afe->regmap, AFE_DL6_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL6_CUR_MSB = 0x%x\n",
		       AFE_DL6_CUR_MSB, value);
	regmap_read(afe->regmap, AFE_DL6_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL6_CUR = 0x%x\n",
		       AFE_DL6_CUR, value);
	regmap_read(afe->regmap, AFE_DL6_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL6_END_MSB = 0x%x\n",
		       AFE_DL6_END_MSB, value);
	regmap_read(afe->regmap, AFE_DL6_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL6_END = 0x%x\n",
		       AFE_DL6_END, value);
	regmap_read(afe->regmap, AFE_DL6_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL6_RCH_MON = 0x%x\n",
		       AFE_DL6_RCH_MON, value);
	regmap_read(afe->regmap, AFE_DL6_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL6_LCH_MON = 0x%x\n",
		       AFE_DL6_LCH_MON, value);
	regmap_read(afe->regmap, AFE_DL6_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL6_CON0 = 0x%x\n",
		       AFE_DL6_CON0, value);
	regmap_read(afe->regmap, AFE_DL7_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL7_BASE_MSB = 0x%x\n",
		       AFE_DL7_BASE_MSB, value);
	regmap_read(afe->regmap, AFE_DL7_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL7_BASE = 0x%x\n",
		       AFE_DL7_BASE, value);
	regmap_read(afe->regmap, AFE_DL7_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL7_CUR_MSB = 0x%x\n",
		       AFE_DL7_CUR_MSB, value);
	regmap_read(afe->regmap, AFE_DL7_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL7_CUR = 0x%x\n",
		       AFE_DL7_CUR, value);
	regmap_read(afe->regmap, AFE_DL7_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL7_END_MSB = 0x%x\n",
		       AFE_DL7_END_MSB, value);
	regmap_read(afe->regmap, AFE_DL7_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL7_END = 0x%x\n",
		       AFE_DL7_END, value);
	regmap_read(afe->regmap, AFE_DL7_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL7_RCH_MON = 0x%x\n",
		       AFE_DL7_RCH_MON, value);
	regmap_read(afe->regmap, AFE_DL7_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL7_LCH_MON = 0x%x\n",
		       AFE_DL7_LCH_MON, value);
	regmap_read(afe->regmap, AFE_DL7_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL7_CON0 = 0x%x\n",
		       AFE_DL7_CON0, value);
	regmap_read(afe->regmap, AFE_DL8_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL8_BASE_MSB = 0x%x\n",
		       AFE_DL8_BASE_MSB, value);
	regmap_read(afe->regmap, AFE_DL8_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL8_BASE = 0x%x\n",
		       AFE_DL8_BASE, value);
	regmap_read(afe->regmap, AFE_DL8_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL8_CUR_MSB = 0x%x\n",
		       AFE_DL8_CUR_MSB, value);
	regmap_read(afe->regmap, AFE_DL8_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL8_CUR = 0x%x\n",
		       AFE_DL8_CUR, value);
	regmap_read(afe->regmap, AFE_DL8_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL8_END_MSB = 0x%x\n",
		       AFE_DL8_END_MSB, value);
	regmap_read(afe->regmap, AFE_DL8_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL8_END = 0x%x\n",
		       AFE_DL8_END, value);
	regmap_read(afe->regmap, AFE_DL8_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL8_RCH_MON = 0x%x\n",
		       AFE_DL8_RCH_MON, value);
	regmap_read(afe->regmap, AFE_DL8_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL8_LCH_MON = 0x%x\n",
		       AFE_DL8_LCH_MON, value);
	regmap_read(afe->regmap, AFE_DL8_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL8_CON0 = 0x%x\n",
		       AFE_DL8_CON0, value);
	regmap_read(afe->regmap, AFE_DL_24CH_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL_24CH_BASE_MSB = 0x%x\n",
		       AFE_DL_24CH_BASE_MSB, value);
	regmap_read(afe->regmap, AFE_DL_24CH_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL_24CH_BASE = 0x%x\n",
		       AFE_DL_24CH_BASE, value);
	regmap_read(afe->regmap, AFE_DL_24CH_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL_24CH_CUR_MSB = 0x%x\n",
		       AFE_DL_24CH_CUR_MSB, value);
	regmap_read(afe->regmap, AFE_DL_24CH_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL_24CH_CUR = 0x%x\n",
		       AFE_DL_24CH_CUR, value);
	regmap_read(afe->regmap, AFE_DL_24CH_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL_24CH_END_MSB = 0x%x\n",
		       AFE_DL_24CH_END_MSB, value);
	regmap_read(afe->regmap, AFE_DL_24CH_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL_24CH_END = 0x%x\n",
		       AFE_DL_24CH_END, value);
	regmap_read(afe->regmap, AFE_DL_24CH_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL_24CH_CON0 = 0x%x\n",
		       AFE_DL_24CH_CON0, value);
	regmap_read(afe->regmap, AFE_DL23_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL23_BASE_MSB = 0x%x\n",
		       AFE_DL23_BASE_MSB, value);
	regmap_read(afe->regmap, AFE_DL23_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL23_BASE = 0x%x\n",
		       AFE_DL23_BASE, value);
	regmap_read(afe->regmap, AFE_DL23_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL23_CUR_MSB = 0x%x\n",
		       AFE_DL23_CUR_MSB, value);
	regmap_read(afe->regmap, AFE_DL23_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL23_CUR = 0x%x\n",
		       AFE_DL23_CUR, value);
	regmap_read(afe->regmap, AFE_DL23_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL23_END_MSB = 0x%x\n",
		       AFE_DL23_END_MSB, value);
	regmap_read(afe->regmap, AFE_DL23_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL23_END = 0x%x\n",
		       AFE_DL23_END, value);
	regmap_read(afe->regmap, AFE_DL23_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL23_RCH_MON = 0x%x\n",
		       AFE_DL23_RCH_MON, value);
	regmap_read(afe->regmap, AFE_DL23_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL23_LCH_MON = 0x%x\n",
		       AFE_DL23_LCH_MON, value);
	regmap_read(afe->regmap, AFE_DL23_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL23_CON0 = 0x%x\n",
		       AFE_DL23_CON0, value);
	regmap_read(afe->regmap, AFE_DL24_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL24_BASE_MSB = 0x%x\n",
		       AFE_DL24_BASE_MSB, value);
	regmap_read(afe->regmap, AFE_DL24_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL24_BASE = 0x%x\n",
		       AFE_DL24_BASE, value);
	regmap_read(afe->regmap, AFE_DL24_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL24_CUR_MSB = 0x%x\n",
		       AFE_DL24_CUR_MSB, value);
	regmap_read(afe->regmap, AFE_DL24_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL24_CUR = 0x%x\n",
		       AFE_DL24_CUR, value);
	regmap_read(afe->regmap, AFE_DL24_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL24_END_MSB = 0x%x\n",
		       AFE_DL24_END_MSB, value);
	regmap_read(afe->regmap, AFE_DL24_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL24_END = 0x%x\n",
		       AFE_DL24_END, value);
	regmap_read(afe->regmap, AFE_DL24_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL24_RCH_MON = 0x%x\n",
		       AFE_DL24_RCH_MON, value);
	regmap_read(afe->regmap, AFE_DL24_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL24_LCH_MON = 0x%x\n",
		       AFE_DL24_LCH_MON, value);
	regmap_read(afe->regmap, AFE_DL24_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL24_CON0 = 0x%x\n",
		       AFE_DL24_CON0, value);
	regmap_read(afe->regmap, AFE_DL25_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL25_BASE_MSB = 0x%x\n",
		       AFE_DL25_BASE_MSB, value);
	regmap_read(afe->regmap, AFE_DL25_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL25_BASE = 0x%x\n",
		       AFE_DL25_BASE, value);
	regmap_read(afe->regmap, AFE_DL25_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL25_CUR_MSB = 0x%x\n",
		       AFE_DL25_CUR_MSB, value);
	regmap_read(afe->regmap, AFE_DL25_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL25_CUR = 0x%x\n",
		       AFE_DL25_CUR, value);
	regmap_read(afe->regmap, AFE_DL25_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL25_END_MSB = 0x%x\n",
		       AFE_DL25_END_MSB, value);
	regmap_read(afe->regmap, AFE_DL25_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL25_END = 0x%x\n",
		       AFE_DL25_END, value);
	regmap_read(afe->regmap, AFE_DL25_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL25_RCH_MON = 0x%x\n",
		       AFE_DL25_RCH_MON, value);
	regmap_read(afe->regmap, AFE_DL25_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL25_LCH_MON = 0x%x\n",
		       AFE_DL25_LCH_MON, value);
	regmap_read(afe->regmap, AFE_DL25_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DL25_CON0 = 0x%x\n",
		       AFE_DL25_CON0, value);

	/*DL_TDMOUT*/
	regmap_read(afe->regmap, AFE_HDMI_OUT_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_HDMI_OUT_BASE_MSB = 0x%x\n",
		       AFE_HDMI_OUT_BASE_MSB, value);
	regmap_read(afe->regmap, AFE_HDMI_OUT_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_HDMI_OUT_BASE = 0x%x\n",
		       AFE_HDMI_OUT_BASE, value);
	regmap_read(afe->regmap, AFE_HDMI_OUT_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_HDMI_OUT_CUR_MSB = 0x%x\n",
		       AFE_HDMI_OUT_CUR_MSB, value);
	regmap_read(afe->regmap, AFE_HDMI_OUT_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_HDMI_OUT_CUR = 0x%x\n",
		       AFE_HDMI_OUT_CUR, value);
	regmap_read(afe->regmap, AFE_HDMI_OUT_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_HDMI_OUT_END_MSB = 0x%x\n",
		       AFE_HDMI_OUT_END_MSB, value);
	regmap_read(afe->regmap, AFE_HDMI_OUT_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_HDMI_OUT_END = 0x%x\n",
		       AFE_HDMI_OUT_END, value);
	regmap_read(afe->regmap, AFE_HDMI_OUT_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_HDMI_OUT_CON0 = 0x%x\n",
		       AFE_HDMI_OUT_CON0, value);
	regmap_read(afe->regmap, AFE_HDMI_OUT_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_HDMI_OUT_MON0 = 0x%x\n",
		       AFE_HDMI_OUT_MON0, value);
/****************************************************************/


	regmap_read(afe->regmap, AFE_VUL0_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL0_BASE_MSB = 0x%x\n",
		       AFE_VUL0_BASE_MSB, value);
	regmap_read(afe->regmap, AFE_VUL0_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL0_BASE = 0x%x\n",
		       AFE_VUL0_BASE, value);
	regmap_read(afe->regmap, AFE_VUL0_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL0_CUR_MSB = 0x%x\n",
		       AFE_VUL0_CUR_MSB, value);
	regmap_read(afe->regmap, AFE_VUL0_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL0_CUR = 0x%x\n",
		       AFE_VUL0_CUR, value);
	regmap_read(afe->regmap, AFE_VUL0_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL0_END_MSB = 0x%x\n",
		       AFE_VUL0_END_MSB, value);
	regmap_read(afe->regmap, AFE_VUL0_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL0_END = 0x%x\n",
		       AFE_VUL0_END, value);
	regmap_read(afe->regmap, AFE_VUL0_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL0_CON0 = 0x%x\n",
		       AFE_VUL0_CON0, value);
	regmap_read(afe->regmap, AFE_VUL1_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL1_BASE_MSB = 0x%x\n",
		       AFE_VUL1_BASE_MSB, value);
	regmap_read(afe->regmap, AFE_VUL1_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL1_BASE = 0x%x\n",
		       AFE_VUL1_BASE, value);
	regmap_read(afe->regmap, AFE_VUL1_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL1_CUR_MSB = 0x%x\n",
		       AFE_VUL1_CUR_MSB, value);
	regmap_read(afe->regmap, AFE_VUL1_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL1_CUR = 0x%x\n",
		       AFE_VUL1_CUR, value);
	regmap_read(afe->regmap, AFE_VUL1_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL1_END_MSB = 0x%x\n",
		       AFE_VUL1_END_MSB, value);
	regmap_read(afe->regmap, AFE_VUL1_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL1_END = 0x%x\n",
		       AFE_VUL1_END, value);
	regmap_read(afe->regmap, AFE_VUL1_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL1_CON0 = 0x%x\n",
		       AFE_VUL1_CON0, value);
	regmap_read(afe->regmap, AFE_VUL2_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL2_BASE_MSB = 0x%x\n",
		       AFE_VUL2_BASE_MSB, value);
	regmap_read(afe->regmap, AFE_VUL2_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL2_BASE = 0x%x\n",
		       AFE_VUL2_BASE, value);
	regmap_read(afe->regmap, AFE_VUL2_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL2_CUR_MSB = 0x%x\n",
		       AFE_VUL2_CUR_MSB, value);
	regmap_read(afe->regmap, AFE_VUL2_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL2_CUR = 0x%x\n",
		       AFE_VUL2_CUR, value);
	regmap_read(afe->regmap, AFE_VUL2_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL2_END_MSB = 0x%x\n",
		       AFE_VUL2_END_MSB, value);
	regmap_read(afe->regmap, AFE_VUL2_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL2_END = 0x%x\n",
		       AFE_VUL2_END, value);
	regmap_read(afe->regmap, AFE_VUL2_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL2_CON0 = 0x%x\n",
		       AFE_VUL2_CON0, value);
	regmap_read(afe->regmap, AFE_VUL3_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL3_BASE_MSB = 0x%x\n",
		       AFE_VUL3_BASE_MSB, value);
	regmap_read(afe->regmap, AFE_VUL3_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL3_BASE = 0x%x\n",
		       AFE_VUL3_BASE, value);
	regmap_read(afe->regmap, AFE_VUL3_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL3_CUR_MSB = 0x%x\n",
		       AFE_VUL3_CUR_MSB, value);
	regmap_read(afe->regmap, AFE_VUL3_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL3_CUR = 0x%x\n",
		       AFE_VUL3_CUR, value);
	regmap_read(afe->regmap, AFE_VUL3_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL3_END_MSB = 0x%x\n",
		       AFE_VUL3_END_MSB, value);
	regmap_read(afe->regmap, AFE_VUL3_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL3_END = 0x%x\n",
		       AFE_VUL3_END, value);
	regmap_read(afe->regmap, AFE_VUL3_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL3_CON0 = 0x%x\n",
		       AFE_VUL3_CON0, value);
	regmap_read(afe->regmap, AFE_VUL4_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL4_BASE_MSB = 0x%x\n",
		       AFE_VUL4_BASE_MSB, value);
	regmap_read(afe->regmap, AFE_VUL4_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL4_BASE = 0x%x\n",
		       AFE_VUL4_BASE, value);
	regmap_read(afe->regmap, AFE_VUL4_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL4_CUR_MSB = 0x%x\n",
		       AFE_VUL4_CUR_MSB, value);
	regmap_read(afe->regmap, AFE_VUL4_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL4_CUR = 0x%x\n",
		       AFE_VUL4_CUR, value);
	regmap_read(afe->regmap, AFE_VUL4_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL4_END_MSB = 0x%x\n",
		       AFE_VUL4_END_MSB, value);
	regmap_read(afe->regmap, AFE_VUL4_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL4_END = 0x%x\n",
		       AFE_VUL4_END, value);
	regmap_read(afe->regmap, AFE_VUL4_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL4_CON0 = 0x%x\n",
		       AFE_VUL4_CON0, value);
	regmap_read(afe->regmap, AFE_VUL5_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL5_BASE_MSB = 0x%x\n",
		       AFE_VUL5_BASE_MSB, value);
	regmap_read(afe->regmap, AFE_VUL5_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL5_BASE = 0x%x\n",
		       AFE_VUL5_BASE, value);
	regmap_read(afe->regmap, AFE_VUL5_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL5_CUR_MSB = 0x%x\n",
		       AFE_VUL5_CUR_MSB, value);
	regmap_read(afe->regmap, AFE_VUL5_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL5_CUR = 0x%x\n",
		       AFE_VUL5_CUR, value);
	regmap_read(afe->regmap, AFE_VUL5_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL5_END_MSB = 0x%x\n",
		       AFE_VUL5_END_MSB, value);
	regmap_read(afe->regmap, AFE_VUL5_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL5_END = 0x%x\n",
		       AFE_VUL5_END, value);
	regmap_read(afe->regmap, AFE_VUL5_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL5_CON0 = 0x%x\n",
		       AFE_VUL5_CON0, value);
	regmap_read(afe->regmap, AFE_VUL6_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL6_BASE_MSB = 0x%x\n",
		       AFE_VUL6_BASE_MSB, value);
	regmap_read(afe->regmap, AFE_VUL6_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL6_BASE = 0x%x\n",
		       AFE_VUL6_BASE, value);
	regmap_read(afe->regmap, AFE_VUL6_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL6_CUR_MSB = 0x%x\n",
		       AFE_VUL6_CUR_MSB, value);
	regmap_read(afe->regmap, AFE_VUL6_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL6_CUR = 0x%x\n",
		       AFE_VUL6_CUR, value);
	regmap_read(afe->regmap, AFE_VUL6_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL6_END_MSB = 0x%x\n",
		       AFE_VUL6_END_MSB, value);
	regmap_read(afe->regmap, AFE_VUL6_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL6_END = 0x%x\n",
		       AFE_VUL6_END, value);
	regmap_read(afe->regmap, AFE_VUL6_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL6_CON0 = 0x%x\n",
		       AFE_VUL6_CON0, value);
	regmap_read(afe->regmap, AFE_VUL7_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL7_BASE_MSB = 0x%x\n",
		       AFE_VUL7_BASE_MSB, value);
	regmap_read(afe->regmap, AFE_VUL7_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL7_BASE = 0x%x\n",
		       AFE_VUL7_BASE, value);
	regmap_read(afe->regmap, AFE_VUL7_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL7_CUR_MSB = 0x%x\n",
		       AFE_VUL7_CUR_MSB, value);
	regmap_read(afe->regmap, AFE_VUL7_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL7_CUR = 0x%x\n",
		       AFE_VUL7_CUR, value);
	regmap_read(afe->regmap, AFE_VUL7_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL7_END_MSB = 0x%x\n",
		       AFE_VUL7_END_MSB, value);
	regmap_read(afe->regmap, AFE_VUL7_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL7_END = 0x%x\n",
		       AFE_VUL7_END, value);
	regmap_read(afe->regmap, AFE_VUL7_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL7_CON0 = 0x%x\n",
		       AFE_VUL7_CON0, value);
	regmap_read(afe->regmap, AFE_VUL8_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL8_BASE_MSB = 0x%x\n",
		       AFE_VUL8_BASE_MSB, value);
	regmap_read(afe->regmap, AFE_VUL8_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL8_BASE = 0x%x\n",
		       AFE_VUL8_BASE, value);
	regmap_read(afe->regmap, AFE_VUL8_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL8_CUR_MSB = 0x%x\n",
		       AFE_VUL8_CUR_MSB, value);
	regmap_read(afe->regmap, AFE_VUL8_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL8_CUR = 0x%x\n",
		       AFE_VUL8_CUR, value);
	regmap_read(afe->regmap, AFE_VUL8_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL8_END_MSB = 0x%x\n",
		       AFE_VUL8_END_MSB, value);
	regmap_read(afe->regmap, AFE_VUL8_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL8_END = 0x%x\n",
		       AFE_VUL8_END, value);
	regmap_read(afe->regmap, AFE_VUL8_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL8_CON0 = 0x%x\n",
		       AFE_VUL8_CON0, value);
	regmap_read(afe->regmap, AFE_VUL9_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL9_BASE_MSB = 0x%x\n",
		       AFE_VUL9_BASE_MSB, value);
	regmap_read(afe->regmap, AFE_VUL9_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL9_BASE = 0x%x\n",
		       AFE_VUL9_BASE, value);
	regmap_read(afe->regmap, AFE_VUL9_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL9_CUR_MSB = 0x%x\n",
		       AFE_VUL9_CUR_MSB, value);
	regmap_read(afe->regmap, AFE_VUL9_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL9_CUR = 0x%x\n",
		       AFE_VUL9_CUR, value);
	regmap_read(afe->regmap, AFE_VUL9_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL9_END_MSB = 0x%x\n",
		       AFE_VUL9_END_MSB, value);
	regmap_read(afe->regmap, AFE_VUL9_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL9_END = 0x%x\n",
		       AFE_VUL9_END, value);
	regmap_read(afe->regmap, AFE_VUL9_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL9_CON0 = 0x%x\n",
		       AFE_VUL9_CON0, value);
	regmap_read(afe->regmap, AFE_VUL10_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL10_BASE_MSB = 0x%x\n",
		       AFE_VUL10_BASE_MSB, value);
	regmap_read(afe->regmap, AFE_VUL10_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL10_BASE = 0x%x\n",
		       AFE_VUL10_BASE, value);
	regmap_read(afe->regmap, AFE_VUL10_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL10_CUR_MSB = 0x%x\n",
		       AFE_VUL10_CUR_MSB, value);
	regmap_read(afe->regmap, AFE_VUL10_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL10_CUR = 0x%x\n",
		       AFE_VUL10_CUR, value);
	regmap_read(afe->regmap, AFE_VUL10_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL10_END_MSB = 0x%x\n",
		       AFE_VUL10_END_MSB, value);
	regmap_read(afe->regmap, AFE_VUL10_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL10_END = 0x%x\n",
		       AFE_VUL10_END, value);
	regmap_read(afe->regmap, AFE_VUL10_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL10_CON0 = 0x%x\n",
		       AFE_VUL10_CON0, value);
	regmap_read(afe->regmap, AFE_VUL24_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL24_BASE_MSB = 0x%x\n",
		       AFE_VUL24_BASE_MSB, value);
	regmap_read(afe->regmap, AFE_VUL24_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL24_BASE = 0x%x\n",
		       AFE_VUL24_BASE, value);
	regmap_read(afe->regmap, AFE_VUL24_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL24_CUR_MSB = 0x%x\n",
		       AFE_VUL24_CUR_MSB, value);
	regmap_read(afe->regmap, AFE_VUL24_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL24_CUR = 0x%x\n",
		       AFE_VUL24_CUR, value);
	regmap_read(afe->regmap, AFE_VUL24_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL24_END_MSB = 0x%x\n",
		       AFE_VUL24_END_MSB, value);
	regmap_read(afe->regmap, AFE_VUL24_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL24_END = 0x%x\n",
		       AFE_VUL24_END, value);
	regmap_read(afe->regmap, AFE_VUL24_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL24_CON0 = 0x%x\n",
		       AFE_VUL24_CON0, value);
	regmap_read(afe->regmap, AFE_VUL25_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL25_BASE_MSB = 0x%x\n",
		       AFE_VUL25_BASE_MSB, value);
	regmap_read(afe->regmap, AFE_VUL25_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL25_BASE = 0x%x\n",
		       AFE_VUL25_BASE, value);
	regmap_read(afe->regmap, AFE_VUL25_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL25_CUR_MSB = 0x%x\n",
		       AFE_VUL25_CUR_MSB, value);
	regmap_read(afe->regmap, AFE_VUL25_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL25_CUR = 0x%x\n",
		       AFE_VUL25_CUR, value);
	regmap_read(afe->regmap, AFE_VUL25_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL25_END_MSB = 0x%x\n",
		       AFE_VUL25_END_MSB, value);
	regmap_read(afe->regmap, AFE_VUL25_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL25_END = 0x%x\n",
		       AFE_VUL25_END, value);
	regmap_read(afe->regmap, AFE_VUL25_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL25_CON0 = 0x%x\n",
		       AFE_VUL25_CON0, value);
	regmap_read(afe->regmap, AFE_VUL_CM0_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL_CM0_BASE_MSB = 0x%x\n",
		       AFE_VUL_CM0_BASE_MSB, value);
	regmap_read(afe->regmap, AFE_VUL_CM0_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL_CM0_BASE = 0x%x\n",
		       AFE_VUL_CM0_BASE, value);
	regmap_read(afe->regmap, AFE_VUL_CM0_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL_CM0_CUR_MSB = 0x%x\n",
		       AFE_VUL_CM0_CUR_MSB, value);
	regmap_read(afe->regmap, AFE_VUL_CM0_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL_CM0_CUR = 0x%x\n",
		       AFE_VUL_CM0_CUR, value);
	regmap_read(afe->regmap, AFE_VUL_CM0_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL_CM0_END_MSB = 0x%x\n",
		       AFE_VUL_CM0_END_MSB, value);
	regmap_read(afe->regmap, AFE_VUL_CM0_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL_CM0_END = 0x%x\n",
		       AFE_VUL_CM0_END, value);
	regmap_read(afe->regmap, AFE_VUL_CM0_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL_CM0_CON0 = 0x%x\n",
		       AFE_VUL_CM0_CON0, value);
	regmap_read(afe->regmap, AFE_VUL_CM1_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL_CM1_BASE_MSB = 0x%x\n",
		       AFE_VUL_CM1_BASE_MSB, value);
	regmap_read(afe->regmap, AFE_VUL_CM1_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL_CM1_BASE = 0x%x\n",
		       AFE_VUL_CM1_BASE, value);
	regmap_read(afe->regmap, AFE_VUL_CM1_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL_CM1_CUR_MSB = 0x%x\n",
		       AFE_VUL_CM1_CUR_MSB, value);
	regmap_read(afe->regmap, AFE_VUL_CM1_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL_CM1_CUR = 0x%x\n",
		       AFE_VUL_CM1_CUR, value);
	regmap_read(afe->regmap, AFE_VUL_CM1_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL_CM1_END_MSB = 0x%x\n",
		       AFE_VUL_CM1_END_MSB, value);
	regmap_read(afe->regmap, AFE_VUL_CM1_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL_CM1_END = 0x%x\n",
		       AFE_VUL_CM1_END, value);
	regmap_read(afe->regmap, AFE_VUL_CM1_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_VUL_CM1_CON0 = 0x%x\n",
		       AFE_VUL_CM1_CON0, value);
	regmap_read(afe->regmap, AFE_ETDM_IN0_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_ETDM_IN0_BASE_MSB = 0x%x\n",
		       AFE_ETDM_IN0_BASE_MSB, value);
	regmap_read(afe->regmap, AFE_ETDM_IN0_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_ETDM_IN0_BASE = 0x%x\n",
		       AFE_ETDM_IN0_BASE, value);
	regmap_read(afe->regmap, AFE_ETDM_IN0_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_ETDM_IN0_CUR_MSB = 0x%x\n",
		       AFE_ETDM_IN0_CUR_MSB, value);
	regmap_read(afe->regmap, AFE_ETDM_IN0_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_ETDM_IN0_CUR = 0x%x\n",
		       AFE_ETDM_IN0_CUR, value);
	regmap_read(afe->regmap, AFE_ETDM_IN0_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_ETDM_IN0_END_MSB = 0x%x\n",
		       AFE_ETDM_IN0_END_MSB, value);
	regmap_read(afe->regmap, AFE_ETDM_IN0_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_ETDM_IN0_END = 0x%x\n",
		       AFE_ETDM_IN0_END, value);
	regmap_read(afe->regmap, AFE_ETDM_IN0_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_ETDM_IN0_CON0 = 0x%x\n",
		       AFE_ETDM_IN0_CON0, value);
	regmap_read(afe->regmap, AFE_ETDM_IN1_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_ETDM_IN1_BASE_MSB = 0x%x\n",
		       AFE_ETDM_IN1_BASE_MSB, value);
	regmap_read(afe->regmap, AFE_ETDM_IN1_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_ETDM_IN1_BASE = 0x%x\n",
		       AFE_ETDM_IN1_BASE, value);
	regmap_read(afe->regmap, AFE_ETDM_IN1_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_ETDM_IN1_CUR_MSB = 0x%x\n",
		       AFE_ETDM_IN1_CUR_MSB, value);
	regmap_read(afe->regmap, AFE_ETDM_IN1_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_ETDM_IN1_CUR = 0x%x\n",
		       AFE_ETDM_IN1_CUR, value);
	regmap_read(afe->regmap, AFE_ETDM_IN1_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_ETDM_IN1_END_MSB = 0x%x\n",
		       AFE_ETDM_IN1_END_MSB, value);
	regmap_read(afe->regmap, AFE_ETDM_IN1_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_ETDM_IN1_END = 0x%x\n",
		       AFE_ETDM_IN1_END, value);
	regmap_read(afe->regmap, AFE_ETDM_IN1_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_ETDM_IN1_CON0 = 0x%x\n",
		       AFE_ETDM_IN1_CON0, value);
	regmap_read(afe->regmap, AFE_SRAM_BOUND, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_SRAM_BOUND = 0x%x\n",
		       AFE_SRAM_BOUND, value);
	regmap_read(afe->regmap, AFE_SECURE_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_SECURE_CON0 = 0x%x\n",
		       AFE_SECURE_CON0, value);
	regmap_read(afe->regmap, AFE_SECURE_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_SECURE_CON1 = 0x%x\n",
		       AFE_SECURE_CON1, value);
	regmap_read(afe->regmap, AFE_SE_SECURE_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_SE_SECURE_CON0 = 0x%x\n",
		       AFE_SE_SECURE_CON0, value);
	regmap_read(afe->regmap, AFE_SE_SECURE_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_SE_SECURE_CON1 = 0x%x\n",
		       AFE_SE_SECURE_CON1, value);
	regmap_read(afe->regmap, AFE_SE_SECURE_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_SE_SECURE_CON2 = 0x%x\n",
		       AFE_SE_SECURE_CON2, value);
	regmap_read(afe->regmap, AFE_SE_SECURE_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_SE_SECURE_CON3 = 0x%x\n",
		       AFE_SE_SECURE_CON3, value);
	regmap_read(afe->regmap, AFE_SE_PROT_SIDEBAND0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_SE_PROT_SIDEBAND0 = 0x%x\n",
		       AFE_SE_PROT_SIDEBAND0, value);
	regmap_read(afe->regmap, AFE_SE_PROT_SIDEBAND1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_SE_PROT_SIDEBAND1 = 0x%x\n",
		       AFE_SE_PROT_SIDEBAND1, value);
	regmap_read(afe->regmap, AFE_SE_PROT_SIDEBAND2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_SE_PROT_SIDEBAND2 = 0x%x\n",
		       AFE_SE_PROT_SIDEBAND2, value);
	regmap_read(afe->regmap, AFE_SE_PROT_SIDEBAND3, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_SE_PROT_SIDEBAND3 = 0x%x\n",
		       AFE_SE_PROT_SIDEBAND3, value);
	regmap_read(afe->regmap, AFE_SE_DOMAIN_SIDEBAND0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_SE_DOMAIN_SIDEBAND0 = 0x%x\n",
		       AFE_SE_DOMAIN_SIDEBAND0, value);
	regmap_read(afe->regmap, AFE_SE_DOMAIN_SIDEBAND1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_SE_DOMAIN_SIDEBAND1 = 0x%x\n",
		       AFE_SE_DOMAIN_SIDEBAND1, value);
	regmap_read(afe->regmap, AFE_SE_DOMAIN_SIDEBAND2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_SE_DOMAIN_SIDEBAND2 = 0x%x\n",
		       AFE_SE_DOMAIN_SIDEBAND2, value);
	regmap_read(afe->regmap, AFE_SE_DOMAIN_SIDEBAND3, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_SE_DOMAIN_SIDEBAND3 = 0x%x\n",
		       AFE_SE_DOMAIN_SIDEBAND3, value);
	regmap_read(afe->regmap, AFE_SE_DOMAIN_SIDEBAND4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_SE_DOMAIN_SIDEBAND4 = 0x%x\n",
		       AFE_SE_DOMAIN_SIDEBAND4, value);
	regmap_read(afe->regmap, AFE_SE_DOMAIN_SIDEBAND5, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_SE_DOMAIN_SIDEBAND5 = 0x%x\n",
		       AFE_SE_DOMAIN_SIDEBAND5, value);
	regmap_read(afe->regmap, AFE_SE_DOMAIN_SIDEBAND6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_SE_DOMAIN_SIDEBAND6 = 0x%x\n",
		       AFE_SE_DOMAIN_SIDEBAND6, value);
	regmap_read(afe->regmap, AFE_SE_DOMAIN_SIDEBAND7, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_SE_DOMAIN_SIDEBAND7 = 0x%x\n",
		       AFE_SE_DOMAIN_SIDEBAND7, value);
	regmap_read(afe->regmap, AFE_SE_DOMAIN_SIDEBAND8, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_SE_DOMAIN_SIDEBAND8 = 0x%x\n",
		       AFE_SE_DOMAIN_SIDEBAND8, value);
	regmap_read(afe->regmap, AFE_SE_DOMAIN_SIDEBAND9, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_SE_DOMAIN_SIDEBAND9 = 0x%x\n",
		       AFE_SE_DOMAIN_SIDEBAND9, value);
	regmap_read(afe->regmap, AFE_PROT_SIDEBAND0_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_PROT_SIDEBAND0_MON = 0x%x\n",
		       AFE_PROT_SIDEBAND0_MON, value);
	regmap_read(afe->regmap, AFE_PROT_SIDEBAND1_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_PROT_SIDEBAND1_MON = 0x%x\n",
		       AFE_PROT_SIDEBAND1_MON, value);
	regmap_read(afe->regmap, AFE_PROT_SIDEBAND2_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_PROT_SIDEBAND2_MON = 0x%x\n",
		       AFE_PROT_SIDEBAND2_MON, value);
	regmap_read(afe->regmap, AFE_PROT_SIDEBAND3_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_PROT_SIDEBAND3_MON = 0x%x\n",
		       AFE_PROT_SIDEBAND3_MON, value);
	regmap_read(afe->regmap, AFE_DOMAIN_SIDEBAND0_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DOMAIN_SIDEBAND0_MON = 0x%x\n",
		       AFE_DOMAIN_SIDEBAND0_MON, value);
	regmap_read(afe->regmap, AFE_DOMAIN_SIDEBAND1_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DOMAIN_SIDEBAND1_MON = 0x%x\n",
		       AFE_DOMAIN_SIDEBAND1_MON, value);
	regmap_read(afe->regmap, AFE_DOMAIN_SIDEBAND2_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DOMAIN_SIDEBAND2_MON = 0x%x\n",
		       AFE_DOMAIN_SIDEBAND2_MON, value);
	regmap_read(afe->regmap, AFE_DOMAIN_SIDEBAND3_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DOMAIN_SIDEBAND3_MON = 0x%x\n",
		       AFE_DOMAIN_SIDEBAND3_MON, value);
	regmap_read(afe->regmap, AFE_DOMAIN_SIDEBAND4_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DOMAIN_SIDEBAND4_MON = 0x%x\n",
		       AFE_DOMAIN_SIDEBAND4_MON, value);
	regmap_read(afe->regmap, AFE_DOMAIN_SIDEBAND5_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DOMAIN_SIDEBAND5_MON = 0x%x\n",
		       AFE_DOMAIN_SIDEBAND5_MON, value);
	regmap_read(afe->regmap, AFE_DOMAIN_SIDEBAND6_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DOMAIN_SIDEBAND6_MON = 0x%x\n",
		       AFE_DOMAIN_SIDEBAND6_MON, value);
	regmap_read(afe->regmap, AFE_DOMAIN_SIDEBAND7_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DOMAIN_SIDEBAND7_MON = 0x%x\n",
		       AFE_DOMAIN_SIDEBAND7_MON, value);
	regmap_read(afe->regmap, AFE_DOMAIN_SIDEBAND8_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DOMAIN_SIDEBAND8_MON = 0x%x\n",
		       AFE_DOMAIN_SIDEBAND8_MON, value);
	regmap_read(afe->regmap, AFE_DOMAIN_SIDEBAND9_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_DOMAIN_SIDEBAND9_MON = 0x%x\n",
		       AFE_DOMAIN_SIDEBAND9_MON, value);
	regmap_read(afe->regmap, AFE_SECURE_CONN0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_SECURE_CONN0 = 0x%x\n",
		       AFE_SECURE_CONN0, value);
	regmap_read(afe->regmap, AFE_SECURE_CONN_ETDM0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_SECURE_CONN_ETDM0 = 0x%x\n",
		       AFE_SECURE_CONN_ETDM0, value);
	regmap_read(afe->regmap, AFE_SECURE_CONN_ETDM1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_SECURE_CONN_ETDM1 = 0x%x\n",
		       AFE_SECURE_CONN_ETDM1, value);
	regmap_read(afe->regmap, AFE_SECURE_CONN_ETDM2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_SECURE_CONN_ETDM2 = 0x%x\n",
		       AFE_SECURE_CONN_ETDM2, value);
	regmap_read(afe->regmap, AFE_SECURE_SRAM_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_SECURE_SRAM_CON0 = 0x%x\n",
		       AFE_SECURE_SRAM_CON0, value);
	regmap_read(afe->regmap, AFE_SECURE_SRAM_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_SECURE_SRAM_CON1 = 0x%x\n",
		       AFE_SECURE_SRAM_CON1, value);
	regmap_read(afe->regmap, AFE_SE_CONN_INPUT_MASK0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_SE_CONN_INPUT_MASK0 = 0x%x\n",
		       AFE_SE_CONN_INPUT_MASK0, value);
	regmap_read(afe->regmap, AFE_SE_CONN_INPUT_MASK1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_SE_CONN_INPUT_MASK1 = 0x%x\n",
		       AFE_SE_CONN_INPUT_MASK1, value);
	regmap_read(afe->regmap, AFE_SE_CONN_INPUT_MASK2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_SE_CONN_INPUT_MASK2 = 0x%x\n",
		       AFE_SE_CONN_INPUT_MASK2, value);
	regmap_read(afe->regmap, AFE_SE_CONN_INPUT_MASK3, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_SE_CONN_INPUT_MASK3 = 0x%x\n",
		       AFE_SE_CONN_INPUT_MASK3, value);
	regmap_read(afe->regmap, AFE_SE_CONN_INPUT_MASK4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_SE_CONN_INPUT_MASK4 = 0x%x\n",
		       AFE_SE_CONN_INPUT_MASK4, value);
	regmap_read(afe->regmap, AFE_SE_CONN_INPUT_MASK5, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_SE_CONN_INPUT_MASK5 = 0x%x\n",
		       AFE_SE_CONN_INPUT_MASK5, value);
	regmap_read(afe->regmap, AFE_SE_CONN_INPUT_MASK6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_SE_CONN_INPUT_MASK6 = 0x%x\n",
		       AFE_SE_CONN_INPUT_MASK6, value);
	regmap_read(afe->regmap, AFE_SE_CONN_INPUT_MASK7, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_SE_CONN_INPUT_MASK7 = 0x%x\n",
		       AFE_SE_CONN_INPUT_MASK7, value);
	regmap_read(afe->regmap, AFE_NON_SE_CONN_INPUT_MASK0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_NON_SE_CONN_INPUT_MASK0 = 0x%x\n",
		       AFE_NON_SE_CONN_INPUT_MASK0, value);
	regmap_read(afe->regmap, AFE_NON_SE_CONN_INPUT_MASK1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_NON_SE_CONN_INPUT_MASK1 = 0x%x\n",
		       AFE_NON_SE_CONN_INPUT_MASK1, value);
	regmap_read(afe->regmap, AFE_NON_SE_CONN_INPUT_MASK2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_NON_SE_CONN_INPUT_MASK2 = 0x%x\n",
		       AFE_NON_SE_CONN_INPUT_MASK2, value);
	regmap_read(afe->regmap, AFE_NON_SE_CONN_INPUT_MASK3, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_NON_SE_CONN_INPUT_MASK3 = 0x%x\n",
		       AFE_NON_SE_CONN_INPUT_MASK3, value);
	regmap_read(afe->regmap, AFE_NON_SE_CONN_INPUT_MASK4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_NON_SE_CONN_INPUT_MASK4 = 0x%x\n",
		       AFE_NON_SE_CONN_INPUT_MASK4, value);
	regmap_read(afe->regmap, AFE_NON_SE_CONN_INPUT_MASK5, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_NON_SE_CONN_INPUT_MASK5 = 0x%x\n",
		       AFE_NON_SE_CONN_INPUT_MASK5, value);
	regmap_read(afe->regmap, AFE_NON_SE_CONN_INPUT_MASK6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_NON_SE_CONN_INPUT_MASK6 = 0x%x\n",
		       AFE_NON_SE_CONN_INPUT_MASK6, value);
	regmap_read(afe->regmap, AFE_NON_SE_CONN_INPUT_MASK7, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_NON_SE_CONN_INPUT_MASK7 = 0x%x\n",
		       AFE_NON_SE_CONN_INPUT_MASK7, value);
	regmap_read(afe->regmap, AFE_SE_CONN_OUTPUT_SEL0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_SE_CONN_OUTPUT_SEL0 = 0x%x\n",
		       AFE_SE_CONN_OUTPUT_SEL0, value);
	regmap_read(afe->regmap, AFE_SE_CONN_OUTPUT_SEL1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_SE_CONN_OUTPUT_SEL1 = 0x%x\n",
		       AFE_SE_CONN_OUTPUT_SEL1, value);
	regmap_read(afe->regmap, AFE_SE_CONN_OUTPUT_SEL2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_SE_CONN_OUTPUT_SEL2 = 0x%x\n",
		       AFE_SE_CONN_OUTPUT_SEL2, value);
	regmap_read(afe->regmap, AFE_SE_CONN_OUTPUT_SEL3, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_SE_CONN_OUTPUT_SEL3 = 0x%x\n",
		       AFE_SE_CONN_OUTPUT_SEL3, value);
	regmap_read(afe->regmap, AFE_SE_CONN_OUTPUT_SEL4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_SE_CONN_OUTPUT_SEL4 = 0x%x\n",
		       AFE_SE_CONN_OUTPUT_SEL4, value);
	regmap_read(afe->regmap, AFE_SE_CONN_OUTPUT_SEL5, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_SE_CONN_OUTPUT_SEL5 = 0x%x\n",
		       AFE_SE_CONN_OUTPUT_SEL5, value);
	regmap_read(afe->regmap, AFE_SE_CONN_OUTPUT_SEL6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_SE_CONN_OUTPUT_SEL6 = 0x%x\n",
		       AFE_SE_CONN_OUTPUT_SEL6, value);
	regmap_read(afe->regmap, AFE_SE_CONN_OUTPUT_SEL7, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_SE_CONN_OUTPUT_SEL7 = 0x%x\n",
		       AFE_SE_CONN_OUTPUT_SEL7, value);
	regmap_read(afe->regmap, AFE_PCM0_INTF_CON1_MASK_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_PCM0_INTF_CON1_MASK_MON = 0x%x\n",
		       AFE_PCM0_INTF_CON1_MASK_MON, value);
	regmap_read(afe->regmap, AFE_CONNSYS_I2S_CON_MASK_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_CONNSYS_I2S_CON_MASK_MON = 0x%x\n",
		       AFE_CONNSYS_I2S_CON_MASK_MON, value);
	regmap_read(afe->regmap, AFE_MTKAIF0_CFG0_MASK_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_MTKAIF0_CFG0_MASK_MON = 0x%x\n",
		       AFE_MTKAIF0_CFG0_MASK_MON, value);
	regmap_read(afe->regmap, AFE_MTKAIF1_CFG0_MASK_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_MTKAIF1_CFG0_MASK_MON = 0x%x\n",
		       AFE_MTKAIF1_CFG0_MASK_MON, value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_SRC_CON0_MASK_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_ADDA_UL0_SRC_CON0_MASK_MON = 0x%x\n",
		       AFE_ADDA_UL0_SRC_CON0_MASK_MON, value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_ASRC_NEW_CON0 = 0x%x\n",
		       AFE_ASRC_NEW_CON0, value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_ASRC_NEW_CON1 = 0x%x\n",
		       AFE_ASRC_NEW_CON1, value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_ASRC_NEW_CON2 = 0x%x\n",
		       AFE_ASRC_NEW_CON2, value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_ASRC_NEW_CON3 = 0x%x\n",
		       AFE_ASRC_NEW_CON3, value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_ASRC_NEW_CON4 = 0x%x\n",
		       AFE_ASRC_NEW_CON4, value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_ASRC_NEW_CON5 = 0x%x\n",
		       AFE_ASRC_NEW_CON5, value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_ASRC_NEW_CON6 = 0x%x\n",
		       AFE_ASRC_NEW_CON6, value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_ASRC_NEW_CON7 = 0x%x\n",
		       AFE_ASRC_NEW_CON7, value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_ASRC_NEW_CON8 = 0x%x\n",
		       AFE_ASRC_NEW_CON8, value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON9, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_ASRC_NEW_CON9 = 0x%x\n",
		       AFE_ASRC_NEW_CON9, value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON10, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_ASRC_NEW_CON10 = 0x%x\n",
		       AFE_ASRC_NEW_CON10, value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON11, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_ASRC_NEW_CON11 = 0x%x\n",
		       AFE_ASRC_NEW_CON11, value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON12, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_ASRC_NEW_CON12 = 0x%x\n",
		       AFE_ASRC_NEW_CON12, value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON13, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_ASRC_NEW_CON13 = 0x%x\n",
		       AFE_ASRC_NEW_CON13, value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON14, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_ASRC_NEW_CON14 = 0x%x\n",
		       AFE_ASRC_NEW_CON14, value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_ASRC_NEW_IP_VERSION = 0x%x\n",
		       AFE_ASRC_NEW_IP_VERSION, value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC0_NEW_CON0 = 0x%x\n",
		       AFE_GASRC0_NEW_CON0, value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC0_NEW_CON1 = 0x%x\n",
		       AFE_GASRC0_NEW_CON1, value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC0_NEW_CON2 = 0x%x\n",
		       AFE_GASRC0_NEW_CON2, value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC0_NEW_CON3 = 0x%x\n",
		       AFE_GASRC0_NEW_CON3, value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC0_NEW_CON4 = 0x%x\n",
		       AFE_GASRC0_NEW_CON4, value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC0_NEW_CON5 = 0x%x\n",
		       AFE_GASRC0_NEW_CON5, value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC0_NEW_CON6 = 0x%x\n",
		       AFE_GASRC0_NEW_CON6, value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC0_NEW_CON7 = 0x%x\n",
		       AFE_GASRC0_NEW_CON7, value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC0_NEW_CON8 = 0x%x\n",
		       AFE_GASRC0_NEW_CON8, value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON9, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC0_NEW_CON9 = 0x%x\n",
		       AFE_GASRC0_NEW_CON9, value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON10, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC0_NEW_CON10 = 0x%x\n",
		       AFE_GASRC0_NEW_CON10, value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON11, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC0_NEW_CON11 = 0x%x\n",
		       AFE_GASRC0_NEW_CON11, value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON12, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC0_NEW_CON12 = 0x%x\n",
		       AFE_GASRC0_NEW_CON12, value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON13, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC0_NEW_CON13 = 0x%x\n",
		       AFE_GASRC0_NEW_CON13, value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON14, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC0_NEW_CON14 = 0x%x\n",
		       AFE_GASRC0_NEW_CON14, value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC0_NEW_IP_VERSION = 0x%x\n",
		       AFE_GASRC0_NEW_IP_VERSION, value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC1_NEW_CON0 = 0x%x\n",
		       AFE_GASRC1_NEW_CON0, value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC1_NEW_CON1 = 0x%x\n",
		       AFE_GASRC1_NEW_CON1, value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC1_NEW_CON2 = 0x%x\n",
		       AFE_GASRC1_NEW_CON2, value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC1_NEW_CON3 = 0x%x\n",
		       AFE_GASRC1_NEW_CON3, value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC1_NEW_CON4 = 0x%x\n",
		       AFE_GASRC1_NEW_CON4, value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC1_NEW_CON5 = 0x%x\n",
		       AFE_GASRC1_NEW_CON5, value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC1_NEW_CON6 = 0x%x\n",
		       AFE_GASRC1_NEW_CON6, value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC1_NEW_CON7 = 0x%x\n",
		       AFE_GASRC1_NEW_CON7, value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC1_NEW_CON8 = 0x%x\n",
		       AFE_GASRC1_NEW_CON8, value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON9, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC1_NEW_CON9 = 0x%x\n",
		       AFE_GASRC1_NEW_CON9, value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON10, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC1_NEW_CON10 = 0x%x\n",
		       AFE_GASRC1_NEW_CON10, value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON11, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC1_NEW_CON11 = 0x%x\n",
		       AFE_GASRC1_NEW_CON11, value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON12, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC1_NEW_CON12 = 0x%x\n",
		       AFE_GASRC1_NEW_CON12, value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON13, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC1_NEW_CON13 = 0x%x\n",
		       AFE_GASRC1_NEW_CON13, value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON14, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC1_NEW_CON14 = 0x%x\n",
		       AFE_GASRC1_NEW_CON14, value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC1_NEW_IP_VERSION = 0x%x\n",
		       AFE_GASRC1_NEW_IP_VERSION, value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC2_NEW_CON0 = 0x%x\n",
		       AFE_GASRC2_NEW_CON0, value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC2_NEW_CON1 = 0x%x\n",
		       AFE_GASRC2_NEW_CON1, value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC2_NEW_CON2 = 0x%x\n",
		       AFE_GASRC2_NEW_CON2, value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC2_NEW_CON3 = 0x%x\n",
		       AFE_GASRC2_NEW_CON3, value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC2_NEW_CON4 = 0x%x\n",
		       AFE_GASRC2_NEW_CON4, value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC2_NEW_CON5 = 0x%x\n",
		       AFE_GASRC2_NEW_CON5, value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC2_NEW_CON6 = 0x%x\n",
		       AFE_GASRC2_NEW_CON6, value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC2_NEW_CON7 = 0x%x\n",
		       AFE_GASRC2_NEW_CON7, value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC2_NEW_CON8 = 0x%x\n",
		       AFE_GASRC2_NEW_CON8, value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON9, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC2_NEW_CON9 = 0x%x\n",
		       AFE_GASRC2_NEW_CON9, value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON10, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC2_NEW_CON10 = 0x%x\n",
		       AFE_GASRC2_NEW_CON10, value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON11, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC2_NEW_CON11 = 0x%x\n",
		       AFE_GASRC2_NEW_CON11, value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON12, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC2_NEW_CON12 = 0x%x\n",
		       AFE_GASRC2_NEW_CON12, value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON13, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC2_NEW_CON13 = 0x%x\n",
		       AFE_GASRC2_NEW_CON13, value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON14, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC2_NEW_CON14 = 0x%x\n",
		       AFE_GASRC2_NEW_CON14, value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		       "[0x%x] AFE_GASRC2_NEW_IP_VERSION = 0x%x\n",
		       AFE_GASRC2_NEW_IP_VERSION, value);

	return n;

}

#if IS_ENABLED(CONFIG_DEBUG_FS)
static ssize_t mt8189_debugfs_read(struct file *file, char __user *buf,
				   size_t count, loff_t *pos)
{
	struct mtk_base_afe *afe = file->private_data;
	const int size = AFE_SYS_DEBUG_SIZE;
	char *buffer = NULL; /* for reduce kernel stack */
	int n = 0, ret = 0;

	buffer = kmalloc(size, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	n = mt8189_debug_read_reg(buffer, size, afe);

	ret = simple_read_from_buffer(buf, count, pos, buffer, n);
	kfree(buffer);
	return ret;
}

static const struct mtk_afe_debug_cmd mt8189_debug_cmds[] = {
	MTK_AFE_DBG_CMD("write_reg", mtk_afe_debug_write_reg),
	{}
};

static const struct file_operations mt8189_debugfs_ops = {
	.open = mtk_afe_debugfs_open,
	.write = mtk_afe_debugfs_write,
	.read = mt8189_debugfs_read,
};
#endif

static int mt8189_dai_memif_register(struct mtk_base_afe *afe)
{
	struct mtk_base_afe_dai *dai;

	dai = devm_kzalloc(afe->dev, sizeof(*dai), GFP_KERNEL);
	if (!dai)
		return -ENOMEM;

	list_add(&dai->list, &afe->sub_dais);

	dai->dai_drivers = mt8189_memif_dai_driver;
	dai->num_dai_drivers = ARRAY_SIZE(mt8189_memif_dai_driver);

	dai->controls = mt8189_pcm_kcontrols;
	dai->num_controls = ARRAY_SIZE(mt8189_pcm_kcontrols);
	dai->dapm_widgets = mt8189_memif_widgets;
	dai->num_dapm_widgets = ARRAY_SIZE(mt8189_memif_widgets);
	dai->dapm_routes = mt8189_memif_routes;
	dai->num_dapm_routes = ARRAY_SIZE(mt8189_memif_routes);
	return 0;
}

typedef int (*dai_register_cb)(struct mtk_base_afe *);
static const dai_register_cb dai_register_cbs[] = {
	mt8189_dai_adda_register,
	mt8189_dai_i2s_register,
	mt8189_dai_pcm_register,
	mt8189_dai_tdm_register,
	mt8189_dai_memif_register,
};

static int mt8189_afe_pcm_dev_probe(struct platform_device *pdev)
{
	int ret, i;
	unsigned int tmp_reg = 0;
	int irq_id;
	struct mtk_base_afe *afe;
	struct mt8189_afe_private *afe_priv;
	struct resource *res;
	struct device *dev;

	ret = of_reserved_mem_device_init(&pdev->dev);
	if (ret)
		dev_warn(&pdev->dev, "failed to assign memory region: %d\n", ret);

	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(34));
	if (ret)
		return ret;

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

	/* init audio related clock */
	ret = mt8189_init_clock(afe);
	if (ret) {
		dev_info(dev, "init clock error: %d\n", ret);
		return ret;
	}

	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev))
		goto err_pm_disable;

	/* Audio device is part of genpd.
	 * Set audio as syscore device to prevent
	 * genpd automatically power off audio
	 * device when suspend
	 */
	dev_pm_syscore_device(&pdev->dev, true);

	/* regmap init */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	afe->base_addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(afe->base_addr))
		return PTR_ERR(afe->base_addr);

	/* enable clock for regcache get default value from hw */
	pm_runtime_get_sync(&pdev->dev);

	afe->regmap = devm_regmap_init_mmio(&pdev->dev, afe->base_addr,
					    &mt8189_afe_regmap_config);
	if (IS_ERR(afe->regmap))
		return PTR_ERR(afe->regmap);

	/* IPM2.0 clock flow, need debug */
	regmap_read(afe->regmap, AFE_IRQ_MCU_EN, &tmp_reg);
	regmap_write(afe->regmap, AFE_IRQ_MCU_EN, 0xffffffff);
	regmap_read(afe->regmap, AFE_IRQ_MCU_EN, &tmp_reg);
	/* IPM2.0 clock flow, need debug */

	pm_runtime_put_sync(&pdev->dev);

	regcache_cache_only(afe->regmap, true);
	regcache_mark_dirty(afe->regmap);

	/* init gpio */
//	ret = mt8189_afe_gpio_init(afe);
//	if (ret)
//		dev_info(dev, "init gpio error\n");

//	dev_info(dev, "init gpio completed.\n");

	/* init memif */
	/* IPM2.0 no need banding */
	// afe->is_memif_bit_banding = 0;
	afe->memif_32bit_supported = 1;
	afe->memif_size = MT8189_MEMIF_NUM;
	afe->memif = devm_kcalloc(dev, afe->memif_size, sizeof(*afe->memif),
				  GFP_KERNEL);

	if (!afe->memif)
		return -ENOMEM;

	for (i = 0; i < afe->memif_size; i++) {
		afe->memif[i].data = &memif_data[i];
		afe->memif[i].irq_usage = memif_irq_usage[i];
		afe->memif[i].const_irq = 1;
	}

	mutex_init(&afe->irq_alloc_lock);       /* needed when dynamic irq */

	/* init irq */
	afe->irqs_size = MT8189_IRQ_NUM;
	afe->irqs = devm_kcalloc(dev, afe->irqs_size, sizeof(*afe->irqs),
				 GFP_KERNEL);

	if (!afe->irqs)
		return -ENOMEM;

	for (i = 0; i < afe->irqs_size; i++)
		afe->irqs[i].irq_data = &irq_data[i];

	/* request irq */
	irq_id = platform_get_irq(pdev, 0);
	if (irq_id <= 0) {
		dev_info(dev, "%pOFn no irq found\n", dev->of_node);
		return irq_id < 0 ? irq_id : -ENXIO;
	}
	ret = devm_request_irq(dev, irq_id, mt8189_afe_irq_handler,
			       IRQF_TRIGGER_NONE,
			       "Afe_ISR_Handle", (void *)afe);
	if (ret) {
		dev_info(dev, "could not request_irq for Afe_ISR_Handle\n");
		return ret;
	}
	ret = enable_irq_wake(irq_id);
	if (ret < 0)
		dev_info(dev, "enable_irq_wake %d err: %d\n", irq_id, ret);

	/* init sub_dais */
	INIT_LIST_HEAD(&afe->sub_dais);

	for (i = 0; i < ARRAY_SIZE(dai_register_cbs); i++) {
		ret = dai_register_cbs[i](afe);
		if (ret) {
			dev_info(afe->dev, "dai register i %d fail, ret %d\n",
				 i, ret);
			goto err_pm_disable;
		}
	}

	/* init dai_driver and component_driver */
	ret = mtk_afe_combine_sub_dai(afe);
	if (ret) {
		dev_info(afe->dev, "mtk_afe_combine_sub_dai fail, ret %d\n",
			 ret);
		goto err_pm_disable;
	}

	/* others */
	afe->mtk_afe_hardware = &mt8189_afe_hardware;
	afe->memif_fs = mt8189_memif_fs;
	afe->irq_fs = mt8189_irq_fs;
	afe->get_dai_fs = mt8189_get_dai_fs;
	afe->get_memif_pbuf_size = mt8189_get_memif_pbuf_size;

	afe->runtime_resume = mt8189_afe_runtime_resume;
	afe->runtime_suspend = mt8189_afe_runtime_suspend;

	afe->request_dram_resource = mt8189_afe_dram_request;
	afe->release_dram_resource = mt8189_afe_dram_release;

	/* IPM2.0: No need */
	/* afe->is_scp_sema_support = 1; */

#if IS_ENABLED(CONFIG_DEBUG_FS)
	/* debugfs */
	afe->debug_cmds = mt8189_debug_cmds;
	afe->debugfs = debugfs_create_file("mtksocaudio",
					   S_IFREG | 0444, NULL,
					   afe, &mt8189_debugfs_ops);
#endif
	/* register component */
	ret = devm_snd_soc_register_component(&pdev->dev,
					      &mt8189_afe_component,
					      afe->dai_drivers,
					      afe->num_dai_drivers);
	if (ret) {
		dev_info(dev, "afe component err: %d\n", ret);
		goto err_pm_disable;
	}

	afe_priv->pmic_regmap = NULL;
	return 0;

err_pm_disable:
	pm_runtime_disable(&pdev->dev);

	return ret;
}

static int mt8189_afe_pcm_dev_remove(struct platform_device *pdev)
{
	struct mtk_base_afe *afe = platform_get_drvdata(pdev);

	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		mt8189_afe_runtime_suspend(&pdev->dev);
	/* disable afe clock */
	mt8189_afe_disable_clock(afe);
	of_reserved_mem_device_release(&pdev->dev);
	return 0;
}

static const struct of_device_id mt8189_afe_pcm_dt_match[] = {
	{ .compatible = "mediatek,mt8189-afe-pcm", },
	{},
};
MODULE_DEVICE_TABLE(of, mt8189_afe_pcm_dt_match);

static const struct dev_pm_ops mt8189_afe_pm_ops = {
	SET_RUNTIME_PM_OPS(mt8189_afe_runtime_suspend,
			   mt8189_afe_runtime_resume, NULL)
};

static struct platform_driver mt8189_afe_pcm_driver = {
	.driver = {
		.name = "mt8189-afe-pcm",
		.of_match_table = mt8189_afe_pcm_dt_match,
#if IS_ENABLED(CONFIG_PM)
		.pm = &mt8189_afe_pm_ops,
#endif
	},
	.probe = mt8189_afe_pcm_dev_probe,
	.remove = mt8189_afe_pcm_dev_remove,
};

module_platform_driver(mt8189_afe_pcm_driver);

MODULE_DESCRIPTION("MediaTek ALSA SoC AFE platform driver for 8189");
MODULE_AUTHOR("Darren Ye <darren.ye@mediatek.com>");
MODULE_LICENSE("GPL");
