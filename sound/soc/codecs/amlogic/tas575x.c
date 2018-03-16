/*
 * sound/soc/codecs/amlogic/tas575x.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * Author: Shuai Li <shuai.li@amlogic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>

#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>

#include "tas575x.h"

struct tas575x_private {
	//const struct tas575x_chip	 *chip;
	struct regmap	*regmap;
	unsigned int	format;
	unsigned int	tx_slots_mask;
	unsigned int	slot_width;
};

 /* Power-up register defaults */
struct reg_default tas575x_reg_defaults[] = {
	{TAS575X_MUTE,	0},
};

static int tas575x_set_dai_fmt(struct snd_soc_dai *dai, unsigned int format)
{
	struct tas575x_private *priv = snd_soc_codec_get_drvdata(dai->codec);

	pr_info("%s, format:0x%x\n", __func__, format);
	priv->format = format;

	return 0;
}

static int tas575x_set_tdm_slot(struct snd_soc_dai *dai,
		unsigned int tx_mask, unsigned int rx_mask,
		int slots, int slot_width)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas575x_private *priv = snd_soc_codec_get_drvdata(dai->codec);
	unsigned int first_slot, last_slot, tdm_offset;
	int ret;

	tx_mask = priv->tx_slots_mask;
	first_slot = __ffs(tx_mask);
	last_slot = __fls(tx_mask);

	dev_dbg(codec->dev,
		"%s() tx_mask=0x%x rx_mask=0x%x slots=%d slot_width=%d\n",
		__func__, tx_mask, rx_mask, slots, slot_width);

	if (last_slot - first_slot != hweight32(tx_mask) - 1) {
		dev_err(codec->dev, "tdm tx mask must be contiguous\n");
		return -EINVAL;
	}

	tdm_offset = first_slot * slot_width;

	dev_info(codec->dev, "tdm_offset:%#x, width:%d\n",
			tdm_offset, slot_width);

	if (tdm_offset > 255) {
		dev_err(codec->dev, "tdm tx slot selection out of bounds\n");
		return -EINVAL;
	}

	priv->slot_width = slot_width;
	tdm_offset += 1;
	ret = regmap_write(priv->regmap, TAS575X_I2S_SHIFT, tdm_offset);
	if (ret < 0)
		dev_err(codec->dev, "failed to write register: %d\n", ret);
	return ret;
}

static int tas575x_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas575x_private *priv = snd_soc_codec_get_drvdata(dai->codec);
	u32 val, width;
	bool is_dsp = false;

	switch (priv->format & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		val = 0x00 << 4;
		break;
	case SND_SOC_DAIFMT_DSP_A:
	case SND_SOC_DAIFMT_DSP_B:
		val = 0x01 << 4;
		is_dsp = true;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		val = 0x02 << 4;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		val = 0x03 << 4;
		break;
	default:
		return -EINVAL;
	}

	width = params_width(params);
	if (is_dsp) {
		/* in dsp format, bit width is fixed */
		width = priv->slot_width;
	} else {
		width = params_width(params);
	}

	switch (width) {
	case 16:
		val |= 0x00;
		break;
	case 20:
		val |= 0x01;
		break;
	case 24:
		val |= 0x02;
		break;
	case 32:
		val |= 0x03;
		break;
	default:
		return -EINVAL;
	}

	pr_info("%s, val:0x%x\n", __func__, val);
	return snd_soc_update_bits(codec, TAS575X_I2S_FMT,
				  0x33, val);
}

static int tas575x_probe(struct snd_soc_codec *codec)
{
	/* set stanby mode */
	snd_soc_write(codec, TAS575X_STANDBY, 0x10);
	/* reset */
	snd_soc_write(codec, TAS575X_RESET, 0x01);
	/* set for DAC path */
	snd_soc_write(codec, TAS575X_DATA_PATH, 0x22);
	/* vlome control default to -30db*/
	snd_soc_write(codec, TAS575X_CH_B_DIG_VOL, 0x6c);
	snd_soc_write(codec, TAS575X_CH_A_DIG_VOL, 0x6c);
	/* exit stanby mode */
	snd_soc_write(codec, TAS575X_STANDBY, 0x0);

	return 0;
}

static DECLARE_TLV_DB_SCALE(dac_tlv, -10350, 50, 0);

static const struct snd_kcontrol_new tas575x_snd_controls[] = {
	SOC_SINGLE_TLV("Channel B Playback Volume",
		       TAS575X_CH_B_DIG_VOL, 0, 0xff, 1, dac_tlv),
	SOC_SINGLE_TLV("Channel A Playback Volume",
			   TAS575X_CH_A_DIG_VOL, 0, 0xff, 1, dac_tlv),
};

static struct snd_soc_codec_driver soc_codec_dev_tas575x = {
	.probe = tas575x_probe,
	//.remove = tas575x_remove,
	//.suspend = tas575x_suspend,
	//.resume = tas575x_resume,

	.component_driver = {
		.controls		= tas575x_snd_controls,
		.num_controls		= ARRAY_SIZE(tas575x_snd_controls),
		//.dapm_widgets		= tas575x_dapm_widgets,
		//.num_dapm_widgets	= ARRAY_SIZE(tas575x_dapm_widgets),
		//.dapm_routes		= tas575x_audio_route,
		//.num_dapm_routes	= ARRAY_SIZE(tas575x_audio_route),
	},
};

static const struct snd_soc_dai_ops tas575x_dai_ops = {
	.set_fmt	= tas575x_set_dai_fmt,
	.set_tdm_slot = tas575x_set_tdm_slot,
	.hw_params	= tas575x_hw_params,
	//.digital_mute	= tas575x_mute,
};

static struct snd_soc_dai_driver tas575x_dai = {
	.name = "tas575x-hifi",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_192000,
		.formats = SNDRV_PCM_FMTBIT_S32_LE |
			   SNDRV_PCM_FMTBIT_S24_LE |
			   SNDRV_PCM_FMTBIT_S16_LE,
	},
	.ops = &tas575x_dai_ops,
};

static const struct regmap_config tas575x_regmap = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = 0xff,
	.reg_defaults = tas575x_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(tas575x_reg_defaults),
	.cache_type = REGCACHE_RBTREE,
};

static const struct of_device_id tas575x_of_match[] = {
	{ .compatible = "ti, tas5754", 0 },
	{ .compatible = "ti, tas5756", 0 },
	{ }
};
MODULE_DEVICE_TABLE(of, tas575x_of_match);

static int tas575x_i2c_probe(struct i2c_client *client,
		     const struct i2c_device_id *id)
{
	struct tas575x_private *priv;
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	int ret;
	//const struct of_device_id *of_id;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	i2c_set_clientdata(client, priv);

	priv->regmap = devm_regmap_init_i2c(client, &tas575x_regmap);
	if (IS_ERR(priv->regmap)) {
		ret = PTR_ERR(priv->regmap);
		dev_err(&client->dev, "Failed to allocate register map: %d\n",
			ret);
		return ret;
	}

	if (of_property_read_bool(node, "tx_slots_mask")) {
		ret = of_property_read_u32(node,
				"tx_slots_mask", &priv->tx_slots_mask);
		pr_info("got tx_slot_mask %#x\n", priv->tx_slots_mask);
	}

	return snd_soc_register_codec(dev, &soc_codec_dev_tas575x,
					  &tas575x_dai, 1);

}

static int tas575x_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);

	return 0;
}

static const struct i2c_device_id tas575x_i2c_id[] = {
	{ "tas5754", (kernel_ulong_t) 0 },
	{ "tas5756", (kernel_ulong_t) 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tas575x_i2c_id);

static struct i2c_driver tas575x_i2c_driver = {
	.driver = {
		.name = "tas575x",
		.of_match_table = of_match_ptr(tas575x_of_match),
	},
	.probe = tas575x_i2c_probe,
	.remove = tas575x_i2c_remove,
	.id_table = tas575x_i2c_id,
};
module_i2c_driver(tas575x_i2c_driver);

MODULE_DESCRIPTION("ASoC TAS575x driver");
MODULE_AUTHOR("Shuai Li <shuai.li@amlogic.com>");
MODULE_LICENSE("GPL");
