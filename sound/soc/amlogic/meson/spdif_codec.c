/*
 * sound/soc/amlogic/meson/spdif_codec.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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
#undef pr_fmt
#define pr_fmt(fmt) "snd_spdif_codec: " fmt

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <linux/of.h>
#include <linux/pinctrl/consumer.h>
#ifdef CONFIG_AMLOGIC_HDMITX
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_ext.h>
#endif
#include "spdif_dai.h"
#include <linux/amlogic/media/sound/spdif_info.h>

#define DRV_NAME "spdif-dit"

#define STUB_RATES	SNDRV_PCM_RATE_8000_192000
#define STUB_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | \
	SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

static struct spdif_codec {
	struct device *pdev;
	struct pinctrl *p_pinctrl_out;
	struct pinctrl_state *p_pinctrl_out_state;
	struct pinctrl *p_pinctrl_out_mute;
	struct pinctrl_state *p_pinctrl_out_mute_state;
	bool spdif_pinmux_out;
	struct pinctrl *p_pinctrl_in;
	struct pinctrl_state *p_pinctrl_in_state;
	struct pinctrl *p_pinctrl_in_mute;
	struct pinctrl_state *p_pinctrl_in_mute_state;
	bool spdif_pinmux_in;
#ifdef CONFIG_AMLOGIC_HDMITX
	bool aml_audio_hdmiout_mute_flag;
#endif
} v_spdif_codec;

static struct snd_soc_dai_driver dit_stub_dai = {
	.name = "dit-hifi",
	.playback = {
		     .stream_name = "Playback",
		     .channels_min = 1,
		     .channels_max = 8,
		     .rates = STUB_RATES,
		     .formats = STUB_FORMATS,
		     },
	.capture = {
		    .stream_name = "Capture",
		    .channels_min = 1,
		    .channels_max = 8,
		    .rates = STUB_RATES,
		    .formats = STUB_FORMATS,
		    },
};

void aml_spdif_pinmux_init(struct device *dev)
{
	v_spdif_codec.p_pinctrl_out_mute =
		devm_pinctrl_get_select(dev, "audio_spdif_out_mute");
	if (IS_ERR(v_spdif_codec.p_pinctrl_out_mute)) {
		v_spdif_codec.p_pinctrl_out_mute = NULL;
		dev_err(dev, "audio_spdif_out_mute can't get pinctrl\n");
	} else {
		v_spdif_codec.p_pinctrl_out_mute_state =
			pinctrl_lookup_state(v_spdif_codec.p_pinctrl_out_mute,
				"audio_spdif_out_mute");
		if (IS_ERR(v_spdif_codec.p_pinctrl_out_mute_state)) {
			devm_pinctrl_put(v_spdif_codec.p_pinctrl_out_mute);
			v_spdif_codec.p_pinctrl_out_mute_state = NULL;
			dev_err(dev, "audio_spdif_out_mute can't get pinctrl\n");
		}
	}

	v_spdif_codec.p_pinctrl_out =
		devm_pinctrl_get_select(dev, "audio_spdif_out");
	if (IS_ERR(v_spdif_codec.p_pinctrl_out)) {
		v_spdif_codec.p_pinctrl_out = NULL;
		dev_err(dev, "audio_spdif_out can't get pinctrl\n");
	} else {
		v_spdif_codec.p_pinctrl_out_state =
			pinctrl_lookup_state(v_spdif_codec.p_pinctrl_out,
				"audio_spdif_out");
		if (IS_ERR(v_spdif_codec.p_pinctrl_out_state)) {
			devm_pinctrl_put(v_spdif_codec.p_pinctrl_out);
			v_spdif_codec.p_pinctrl_out_state = NULL;
			dev_err(dev, "audio_spdif_out can't get pinctrl\n");
		}
	}

	v_spdif_codec.p_pinctrl_in_mute =
		devm_pinctrl_get_select(dev, "audio_spdif_in_mute");
	if (IS_ERR(v_spdif_codec.p_pinctrl_in_mute)) {
		v_spdif_codec.p_pinctrl_in_mute = NULL;
		dev_err(dev, "audio_spdif_in_mute can't get pinctrl\n");
	}
	v_spdif_codec.p_pinctrl_in =
		devm_pinctrl_get_select(dev, "audio_spdif_in");
	if (IS_ERR(v_spdif_codec.p_pinctrl_in)) {
		v_spdif_codec.p_pinctrl_in = NULL;
		dev_err(dev, "audio_spdif_in can't get pinctrl\n");
	}
}

void aml_spdif_pinmux_deinit(struct device *dev)
{
	if (v_spdif_codec.p_pinctrl_out)
		devm_pinctrl_put(v_spdif_codec.p_pinctrl_out);

	if (v_spdif_codec.p_pinctrl_out_mute)
		devm_pinctrl_put(v_spdif_codec.p_pinctrl_out_mute);

	if (v_spdif_codec.p_pinctrl_in)
		devm_pinctrl_put(v_spdif_codec.p_pinctrl_in);

	if (v_spdif_codec.p_pinctrl_in_mute)
		devm_pinctrl_put(v_spdif_codec.p_pinctrl_in_mute);
}

static int aml_audio_set_spdif_mute(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	v_spdif_codec.spdif_pinmux_out =
			ucontrol->value.integer.value[0];

	pr_info("aml_audio_set_spdif_mute: flag=%d\n",
				v_spdif_codec.spdif_pinmux_out);

	if (v_spdif_codec.spdif_pinmux_out == 0 &&
			v_spdif_codec.p_pinctrl_out &&
			v_spdif_codec.p_pinctrl_out_state)
		pinctrl_select_state(v_spdif_codec.p_pinctrl_out,
			v_spdif_codec.p_pinctrl_out_state);
	else if (v_spdif_codec.spdif_pinmux_out == 1 &&
			v_spdif_codec.p_pinctrl_out_mute &&
			v_spdif_codec.p_pinctrl_out_mute_state)
		pinctrl_select_state(v_spdif_codec.p_pinctrl_out_mute,
			v_spdif_codec.p_pinctrl_out_mute_state);
	return 0;
}

static int aml_audio_get_spdif_mute(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
			v_spdif_codec.spdif_pinmux_out;
	return 0;
}

#ifdef CONFIG_AMLOGIC_HDMITX
/* call HDMITX API to enable/disable internal audio out */
static int aml_get_hdmi_out_audio(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = !hdmitx_ext_get_audio_status();

	v_spdif_codec.aml_audio_hdmiout_mute_flag =
			ucontrol->value.integer.value[0];
	return 0;
}

static int aml_set_hdmi_out_audio(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	bool mute = ucontrol->value.integer.value[0];

	if (v_spdif_codec.aml_audio_hdmiout_mute_flag != mute) {
		hdmitx_ext_set_audio_output(!mute);
		v_spdif_codec.aml_audio_hdmiout_mute_flag = mute;
	}
	return 0;
}

static const char * const hdmi_out_channel_mask_texts[] = {
		"SPDIF",
		"2CH_I2S_0/1",
		"2CH_I2S_2/3",
		"2CH_I2S_4/5",
		"2CH_I2S_6/7",
		"4CH_I2S",
		"6CH_I2S",
		"8CH_I2S",
};

static const struct soc_enum hdmi_out_channel_mask_texts_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0,
		ARRAY_SIZE(hdmi_out_channel_mask_texts),
		hdmi_out_channel_mask_texts);

static int aml_get_hdmi_out_channel_mask(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	char channel_mask = hdmitx_ext_get_i2s_mask();
	int index = 0;

	if (channel_mask == 0x1)
		index = 1;
	else if (channel_mask == 0x2)
		index = 2;
	else if (channel_mask == 0x4)
		index = 3;
	else if (channel_mask == 0x8)
		index = 4;
	else if (channel_mask == 0x3)
		index = 5;
	else if (channel_mask == 0x7)
		index = 6;
	else if (channel_mask == 0xf)
		index = 7;
	else
		index = 0;
	ucontrol->value.integer.value[0] = index;
	return 0;
}

static int aml_set_hdmi_out_channel_mask(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	int index = ucontrol->value.integer.value[0];

	if (index == 1)
		hdmitx_ext_set_i2s_mask(2, 0x1);
	else if (index == 2)
		hdmitx_ext_set_i2s_mask(2, 0x2);
	else if (index == 3)
		hdmitx_ext_set_i2s_mask(2, 0x4);
	else if (index == 4)
		hdmitx_ext_set_i2s_mask(2, 0x8);
	else if (index == 5)
		hdmitx_ext_set_i2s_mask(4, 0x3);
	else if (index == 6)
		hdmitx_ext_set_i2s_mask(6, 0x7);
	else if (index == 7)
		hdmitx_ext_set_i2s_mask(8, 0xf);
	return 0;
}
#endif

static const struct snd_kcontrol_new spdif_controls[] = {
	SOC_SINGLE_BOOL_EXT("Audio spdif mute",
				0, aml_audio_get_spdif_mute,
				aml_audio_set_spdif_mute),
	SOC_ENUM_EXT("Audio spdif format",
				spdif_format_enum,
				spdif_format_get_enum,
				spdif_format_set_enum),
#ifdef CONFIG_AMLOGIC_HDMITX
	SOC_SINGLE_BOOL_EXT("Audio hdmi-out mute",
				0, aml_get_hdmi_out_audio,
				aml_set_hdmi_out_audio),
	SOC_ENUM_EXT("Audio hdmi-out channel mask",
				hdmi_out_channel_mask_texts_enum,
				aml_get_hdmi_out_channel_mask,
				aml_set_hdmi_out_channel_mask),
#endif
};

static int spdif_probe(struct snd_soc_codec *codec)
{
	return snd_soc_add_codec_controls(codec,
			spdif_controls, ARRAY_SIZE(spdif_controls));
}
static struct snd_soc_codec_driver soc_codec_spdif_dit = {
	.probe =	spdif_probe,
};

static int spdif_dit_probe(struct platform_device *pdev)
{
	v_spdif_codec.pdev = &pdev->dev;

	aml_spdif_pinmux_init(&pdev->dev);

	return snd_soc_register_codec(&pdev->dev, &soc_codec_spdif_dit,
				      &dit_stub_dai, 1);
}

static int spdif_dit_remove(struct platform_device *pdev)
{
	aml_spdif_pinmux_deinit(&pdev->dev);
	snd_soc_unregister_codec(&pdev->dev);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id amlogic_spdif_codec_dt_match[] = {
	{.compatible = "amlogic, aml-spdif-codec",},
	{},
};
#else
#define amlogic_spdif_codec_dt_match NULL
#endif

static struct platform_driver spdif_dit_driver = {
	.probe  = spdif_dit_probe,
	.remove = spdif_dit_remove,
	.driver = {
		.name           = DRV_NAME,
		.owner          = THIS_MODULE,
		.of_match_table = amlogic_spdif_codec_dt_match,
	},
};

static int __init spdif_codec_init(void)
{
	return platform_driver_register(&spdif_dit_driver);
}

static void __exit spdif_codec_exit(void)
{
	platform_driver_unregister(&spdif_dit_driver);
}

module_init(spdif_codec_init);
module_exit(spdif_codec_exit);

MODULE_DESCRIPTION("SPDIF dummy codec driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
