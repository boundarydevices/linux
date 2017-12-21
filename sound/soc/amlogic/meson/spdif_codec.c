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

#define DRV_NAME "spdif-dit"

#define STUB_RATES	SNDRV_PCM_RATE_8000_192000
#define STUB_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | \
	SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

static struct spdif_codec {
	struct device *pdev;
	struct pinctrl *p_pinctrl;
	unsigned int spdif_pinmux;
	bool aml_audio_spdif_mute_flag;
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
	if (!v_spdif_codec.spdif_pinmux) {
		v_spdif_codec.spdif_pinmux = 1;
		v_spdif_codec.p_pinctrl =
			devm_pinctrl_get_select(dev, "audio_spdif");
		if (IS_ERR(v_spdif_codec.p_pinctrl)) {
			v_spdif_codec.p_pinctrl = NULL;
			dev_err(dev, "aml_spdif_pinmux_init can't get pinctrl\n");
		}
	}
}

void aml_spdif_pinmux_deinit(struct device *dev)
{
	dev_dbg(dev, "aml_spdif_mute\n");
	if (v_spdif_codec.spdif_pinmux) {
		v_spdif_codec.spdif_pinmux = 0;
		if (v_spdif_codec.p_pinctrl)
			devm_pinctrl_put(v_spdif_codec.p_pinctrl);
	}
}
static int aml_audio_set_spdif_mute(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	v_spdif_codec.aml_audio_spdif_mute_flag =
		ucontrol->value.integer.value[0];

	pr_info("aml_audio_set_spdif_mute: flag=%d\n",
		v_spdif_codec.aml_audio_spdif_mute_flag);

	if (v_spdif_codec.aml_audio_spdif_mute_flag)
		aml_spdif_pinmux_deinit(v_spdif_codec.pdev);
	else
		aml_spdif_pinmux_init(v_spdif_codec.pdev);
	return 0;
}

static int aml_audio_get_spdif_mute(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		v_spdif_codec.aml_audio_spdif_mute_flag;
	return 0;
}

static const struct snd_kcontrol_new spdif_controls[] = {
	SOC_SINGLE_BOOL_EXT("Audio spdif mute",
			    0, aml_audio_get_spdif_mute,
			    aml_audio_set_spdif_mute),
};

static int spdif_probe(struct snd_soc_codec *codec)
{
	return snd_soc_add_codec_controls(codec,
			spdif_controls, ARRAY_SIZE(spdif_controls));
}
static struct snd_soc_codec_driver soc_codec_spdif_dit = {
	.probe =	spdif_probe,
};

static ssize_t spdif_mute_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	if (v_spdif_codec.spdif_pinmux)
		return sprintf(buf, "spdif_unmute\n");
	else
		return sprintf(buf, "spdif_mute\n");

}

static ssize_t spdif_mute_set(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	if (strncmp(buf, "spdif_mute", 10))
		aml_spdif_pinmux_init(dev);
	else if (strncmp(buf, "spdif_unmute", 12))
		aml_spdif_pinmux_deinit(dev);
	else
		dev_err(dev, "spdif set the wrong value\n");

	return count;
}

static DEVICE_ATTR(spdif_mute, 0660, spdif_mute_show, spdif_mute_set);

static int spdif_dit_probe(struct platform_device *pdev)
{
	int ret = device_create_file(&pdev->dev, &dev_attr_spdif_mute);

	v_spdif_codec.pdev = &pdev->dev;

	aml_spdif_pinmux_init(&pdev->dev);
	if (ret < 0)
		dev_err(&pdev->dev,
			"spdif: failed to add spdif_mute sysfs: %d\n", ret);

	return snd_soc_register_codec(&pdev->dev, &soc_codec_spdif_dit,
				      &dit_stub_dai, 1);
}

static int spdif_dit_remove(struct platform_device *pdev)
{
	aml_spdif_pinmux_deinit(&pdev->dev);
	device_remove_file(&pdev->dev, &dev_attr_spdif_mute);
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

MODULE_AUTHOR("Steve Chen <schen@mvista.com>");
MODULE_DESCRIPTION("SPDIF dummy codec driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
