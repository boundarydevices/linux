/*
 * SSM4567 amplifier audio driver
 *
 * Copyright 2014 Google Chromium project.
 *  Author: Anatol Pomozov <anatol@chromium.org>
 *
 * Based on code copyright/by:
 *   Copyright 2013 Analog Devices Inc.
 *
 * Licensed under the GPL-2.
 */

#include <linux/acpi.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#define SSM3525_REG_VENDER_ID 0x00
#define SSM3525_REG_DEVICE_ID1 0x01
#define SSM3525_REG_DEVICE_ID2 0x02
#define SSM3525_REG_REVISION_ID 0x03
#define SSM3525_REG_REGULATOR_ENABLE 0x04
#define SSM3525_REG_AMP_SNS_CTRL		0x05
#define SSM3525_REG_DAC_CTRL		0x06
#define SSM3525_REG_DAC_VOLUME		0x07
#define SSM3525_REG_LIMITER_CTRL_1		0x08
#define SSM3525_REG_LIMITER_CTRL_2		0x09
#define SSM3525_REG_LIMITER_CTRL_3		0x0A
#define SSM3525_REG_VBAT_LIM_CTRL_1		0x0B
#define SSM3525_REG_VBAT_LIM_CTRL_2		0x0C
#define SSM3525_REG_VBAT_LIM_CTRL_3		0x0D
#define SSM3525_REG_LIMITER_LINK		0x0E
#define SSM3525_REG_DAC_FLIP		0x0F
#define SSM3525_REG_FAULT_CTRL		0x10
#define SSM3525_REG_STATUS          0x11
#define SSM3525_REG_TEMP			0x12
#define SSM3525_REG_BATTERY_V_OUT	0x13

#define SSM3525_REG_POWER_CTRL		0x20
#define SSM3525_REG_PDM_CTRL		0x21
#define SSM3525_REG_SAI_CTRL_1		0x22
#define SSM3525_REG_SAI_CTRL_2		0x23
#define SSM3525_REG_SAI_PLACEMENT_1		0x24
#define SSM3525_REG_SAI_PLACEMENT_2		0x25
#define SSM3525_REG_SAI_PLACEMENT_3		0x26
#define SSM3525_REG_SAI_PLACEMENT_4		0x27
#define SSM3525_REG_SAI_PLACEMENT_5		0x28
#define SSM3525_REG_SAI_PLACEMENT_6		0x29

#define SSM3525_REG_AGC_PLACEMENT_1		0x2A
#define SSM3525_REG_AGC_PLACEMENT_2		0x2B
#define SSM3525_REG_AGC_PLACEMENT_3		0x2C
#define SSM3525_REG_AGC_PLACEMENT_4		0x2D
#define SSM3525_REG_SOFT_RESET		0x2E

/* POWER_CTRL */
#define SSM3525_POWER_APWDN_EN		BIT(7)
#define SSM3525_POWER_BSNS_PWDN		BIT(6)
#define SSM3525_POWER_VSNS_PWDN		BIT(5)
#define SSM3525_POWER_ISNS_PWDN		BIT(4)
#define SSM3525_POWER_BOOST_PWDN		BIT(3)
#define SSM3525_POWER_AMP_PWDN		BIT(2)
#define SSM3525_POWER_VBAT_ONLY		BIT(1)
#define SSM3525_POWER_SPWDN			BIT(0)

/* DAC_CTRL */
#define SSM3525_DAC_HV			BIT(7)
#define SSM3525_DAC_MUTE		BIT(6)
#define SSM3525_DAC_HPF			BIT(5)
#define SSM3525_DAC_LPM			BIT(4)
#define SSM3525_DAC_FS_MASK	0x7
#define SSM3525_DAC_FS_8000_12000	0x0
#define SSM3525_DAC_FS_16000_24000	0x1
#define SSM3525_DAC_FS_32000_48000	0x2
#define SSM3525_DAC_FS_64000_96000	0x3
#define SSM3525_DAC_FS_128000_192000	0x4

/* SAI_CTRL_1 */
#define SSM3525_SAI_CTRL_1_BCLK			BIT(6)
#define SSM3525_SAI_CTRL_1_TDM_BLCKS_MASK	(0x3 << 4)
#define SSM3525_SAI_CTRL_1_TDM_BLCKS_32		(0x0 << 4)
#define SSM3525_SAI_CTRL_1_TDM_BLCKS_48		(0x1 << 4)
#define SSM3525_SAI_CTRL_1_TDM_BLCKS_64		(0x2 << 4)
#define SSM3525_SAI_CTRL_1_FSYNC		BIT(3)
#define SSM3525_SAI_CTRL_1_LJ			BIT(2)
#define SSM3525_SAI_CTRL_1_TDM			BIT(1)
#define SSM3525_SAI_CTRL_1_PDM			BIT(0)

/* SAI_CTRL_2 */
#define SSM3525_SAI_CTRL_2_AUTO_SLOT		BIT(3)
#define SSM3525_SAI_CTRL_2_TDM_SLOT_MASK	0x7
#define SSM3525_SAI_CTRL_2_TDM_SLOT(x)		(x)

static int device_num;
struct ssm3525 *g_ssm3525;

struct ssm3525 {
	struct regmap *regmap;
	unsigned int slots_mask;
	const char *amp_cfg;
	int total_num;
};

static const struct reg_default ssm3525_reg_defaults[] = {
	{ SSM3525_REG_REGULATOR_ENABLE, 0x01 },
	{ SSM3525_REG_AMP_SNS_CTRL, 0x23 },
	{ SSM3525_REG_DAC_CTRL, 0x22 },
	{ SSM3525_REG_SAI_PLACEMENT_1, 0x01 },
	{ SSM3525_REG_SAI_PLACEMENT_2, 0x20 },
	{ SSM3525_REG_SAI_PLACEMENT_3, 0x28 },
	{ SSM3525_REG_SAI_PLACEMENT_4, 0x28 },
	{ SSM3525_REG_SAI_PLACEMENT_5, 0x08 },
	{ SSM3525_REG_SAI_PLACEMENT_6, 0x08 },
};


static bool ssm3525_readable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case SSM3525_REG_VENDER_ID ... SSM3525_REG_SOFT_RESET:
		return true;
	default:
		return false;
	}

}

static bool ssm3525_writeable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case SSM3525_REG_REGULATOR_ENABLE ... SSM3525_REG_FAULT_CTRL:
	case SSM3525_REG_POWER_CTRL ... SSM3525_REG_SOFT_RESET:
	 /*The datasheet states that soft reset register is read-only,*/
	 /*but logically it is write-only. */
		return true;
	default:
		return false;
	}
}

static bool ssm3525_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case SSM3525_REG_VENDER_ID ... SSM3525_REG_SOFT_RESET:
		return true;
	default:
		return false;
	}
}

static int WL1_amplifier_power_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	int value = 0;
	int ret;
	int i;
	struct ssm3525 *ssm3525 = NULL;
	ret = -1;
	for (i = 0; i < device_num; i++) {
		if (!strcmp(g_ssm3525[i].amp_cfg, "WL1")) {
			ssm3525 = g_ssm3525+i;
			break;
		}
	}
	if (!ssm3525) {
		pr_info("no wl1 error");
		return ret;
	}
	ret = regmap_read(ssm3525->regmap, SSM3525_REG_POWER_CTRL, &value);
	if (ret)
		return ret;
	ucontrol->value.integer.value[0] = value;
	return 0;
}

static int WL1_amplifier_power_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	int value, ret, i;
	struct ssm3525 *ssm3525 = NULL;
	ret = -1;
	for (i = 0; i < device_num; i++) {
		if (!strcmp(g_ssm3525[i].amp_cfg, "WL1")) {
			ssm3525 = g_ssm3525+i;
			break;
		}
	}
	if (!ssm3525) {
		pr_info("no wl1 error");
		return ret;
	}
	value = ucontrol->value.integer.value[0];
	ret = regmap_write(ssm3525->regmap, SSM3525_REG_POWER_CTRL, value);
	if (ret)
		return ret;
	return 0;
}

static int WR1_amplifier_power_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	int value = 0;
	int ret, i;
	struct ssm3525 *ssm3525 = NULL;
	ret = -1;
	for (i = 0; i < device_num; i++) {
		if (!strcmp(g_ssm3525[i].amp_cfg, "WR1")) {
			ssm3525 = g_ssm3525+i;
			break;
		}
	}
	if (!ssm3525) {
		pr_info("no wr1 error");
		return ret;
	}
	ret = regmap_read(ssm3525->regmap, SSM3525_REG_POWER_CTRL, &value);
	if (ret)
		return ret;
	ucontrol->value.integer.value[0] = value;
	return 0;
}

static int WR1_amplifier_power_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	int value, ret, i;
	struct ssm3525 *ssm3525 = NULL;
	ret = -1;
	for (i = 0; i < device_num; i++) {
		if (!strcmp(g_ssm3525[i].amp_cfg, "WR1")) {
			ssm3525 = g_ssm3525+i;
			break;
		}
	}
	if (!ssm3525) {
		pr_info("no wr1 error");
		return ret;
	}
	value = ucontrol->value.integer.value[0];
	ret = regmap_write(ssm3525->regmap, SSM3525_REG_POWER_CTRL, value);
	if (ret)
		return ret;
	return 0;
}

static struct snd_kcontrol_new ssm3525_snd_controls[] = {
	SOC_SINGLE_EXT("WL1 Amp Power Control", SND_SOC_NOPM, 0, 0xff, 0,
	       WL1_amplifier_power_get, WL1_amplifier_power_put),
	SOC_SINGLE_EXT("WR1 Amp Power Control", SND_SOC_NOPM, 0, 0xff, 0,
	       WR1_amplifier_power_get, WR1_amplifier_power_put),
};
static int ssm3525_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct ssm3525 *ssm3525 = snd_soc_codec_get_drvdata(codec);
	unsigned int rate = params_rate(params);
	unsigned int dacfs;

	if (rate >= 8000 && rate <= 12000)
		dacfs = SSM3525_DAC_FS_8000_12000;
	else if (rate >= 16000 && rate <= 24000)
		dacfs = SSM3525_DAC_FS_16000_24000;
	else if (rate >= 32000 && rate <= 48000)
		dacfs = SSM3525_DAC_FS_32000_48000;
	else if (rate >= 64000 && rate <= 96000)
		dacfs = SSM3525_DAC_FS_64000_96000;
	else if (rate >= 128000 && rate <= 192000)
		dacfs = SSM3525_DAC_FS_128000_192000;
	else
		return -EINVAL;

	return regmap_update_bits(ssm3525->regmap, SSM3525_REG_DAC_CTRL,
				SSM3525_DAC_FS_MASK, dacfs);
}

static int ssm3525_mute(struct snd_soc_dai *dai, int mute)
{
	struct ssm3525 *ssm3525 = snd_soc_codec_get_drvdata(dai->codec);
	unsigned int val;

	val = mute ? SSM3525_DAC_MUTE : 0;
	return regmap_update_bits(ssm3525->regmap, SSM3525_REG_DAC_CTRL,
			SSM3525_DAC_MUTE, val);
}

static int ssm3525_set_tdm_slot(struct snd_soc_dai *dai, unsigned int tx_mask,
	unsigned int rx_mask, int slots, int width)
{
	struct ssm3525 *ssm3525 = snd_soc_dai_get_drvdata(dai);
	unsigned int blcks;
	int slot;
	int ret;

	tx_mask = ssm3525->slots_mask;

	if (tx_mask == 0)
		return -EINVAL;

	slot = __ffs(tx_mask);
	if (tx_mask != BIT(slot))
		return -EINVAL;

	switch (width) {
	case 32:
		blcks = SSM3525_SAI_CTRL_1_TDM_BLCKS_32;
		break;
	case 48:
		blcks = SSM3525_SAI_CTRL_1_TDM_BLCKS_48;
		break;
	case 64:
		blcks = SSM3525_SAI_CTRL_1_TDM_BLCKS_64;
		break;
	default:
		return -EINVAL;
	}

	ret = regmap_update_bits(ssm3525->regmap, SSM3525_REG_SAI_CTRL_2,
		SSM3525_SAI_CTRL_2_AUTO_SLOT | SSM3525_SAI_CTRL_2_TDM_SLOT_MASK,
		SSM3525_SAI_CTRL_2_TDM_SLOT(slot));
	if (ret)
		return ret;

	return regmap_update_bits(ssm3525->regmap, SSM3525_REG_SAI_CTRL_1,
		SSM3525_SAI_CTRL_1_TDM_BLCKS_MASK, blcks);
}

static int ssm3525_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
#if 0
	struct ssm3525 *ssm3525 = snd_soc_dai_get_drvdata(dai);
	unsigned int ctrl1 = 0;
	bool invert_fclk;


	fmt = fmt | SND_SOC_DAIFMT_DSP_A |
		SND_SOC_DAIFMT_CBS_CFS |
		SND_SOC_DAIFMT_NB_NF |
		SND_SOC_DAIFMT_CONT;

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		invert_fclk = false;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		ctrl1 |= SSM3525_SAI_CTRL_1_BCLK;
		invert_fclk = false;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		ctrl1 |= SSM3525_SAI_CTRL_1_FSYNC;
		invert_fclk = true;
		break;
	case SND_SOC_DAIFMT_IB_IF:
		ctrl1 |= SSM3525_SAI_CTRL_1_BCLK;
		invert_fclk = true;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		ctrl1 |= SSM3525_SAI_CTRL_1_LJ;
		invert_fclk = !invert_fclk;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		ctrl1 |= SSM3525_SAI_CTRL_1_TDM;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		ctrl1 |= SSM3525_SAI_CTRL_1_TDM | SSM3525_SAI_CTRL_1_LJ;
		break;
	case SND_SOC_DAIFMT_PDM:
		ctrl1 |= SSM3525_SAI_CTRL_1_PDM;
		break;
	default:
		return -EINVAL;
	}

	if (invert_fclk)
		ctrl1 |= SSM3525_SAI_CTRL_1_FSYNC;

	return regmap_update_bits(ssm3525->regmap, SSM3525_REG_SAI_CTRL_1,
			SSM3525_SAI_CTRL_1_BCLK |
			SSM3525_SAI_CTRL_1_FSYNC |
			SSM3525_SAI_CTRL_1_LJ |
			SSM3525_SAI_CTRL_1_TDM |
			SSM3525_SAI_CTRL_1_PDM,
			ctrl1);
#endif
	return 0;
}

static int ssm3525_set_power(struct ssm3525 *ssm3525, bool enable)
{
	int ret = 0;

	if (!enable) {
		ret = regmap_update_bits(ssm3525->regmap,
			SSM3525_REG_POWER_CTRL,
			SSM3525_POWER_SPWDN, SSM3525_POWER_SPWDN);
	}


	if (enable) {
		ret = regmap_write(ssm3525->regmap, SSM3525_REG_SOFT_RESET,
			0x00);
		if (ret)
			return ret;

		ret = regmap_update_bits(ssm3525->regmap,
			SSM3525_REG_POWER_CTRL,
			SSM3525_POWER_SPWDN, 0x00);
	}

	return ret;
}

static int ssm3525_set_bias_level(struct snd_soc_codec *codec,
	enum snd_soc_bias_level level)
{
	struct ssm3525 *ssm3525 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	switch (level) {
	case SND_SOC_BIAS_ON:
		break;
	case SND_SOC_BIAS_PREPARE:
		break;
	case SND_SOC_BIAS_STANDBY:
		if (snd_soc_codec_get_bias_level(codec) == SND_SOC_BIAS_OFF)
			ret = ssm3525_set_power(ssm3525, true);
		break;
	case SND_SOC_BIAS_OFF:
		ret = ssm3525_set_power(ssm3525, false);
		break;
	}

	return ret;
}

static const struct snd_soc_dai_ops ssm3525_dai_ops = {
	.hw_params	= ssm3525_hw_params,
	.digital_mute	= ssm3525_mute,
	.set_fmt	= ssm3525_set_dai_fmt,
	.set_tdm_slot	= ssm3525_set_tdm_slot,
};

static struct snd_soc_dai_driver ssm3525_dai = {
	.name = "ssm3525-hifi",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 8,
		.rates = SNDRV_PCM_RATE_8000_192000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
			SNDRV_PCM_FMTBIT_S32,
	},
	.capture = {
		.stream_name = "Capture Sense",
		.channels_min = 1,
		.channels_max = 8,
		.rates = SNDRV_PCM_RATE_8000_192000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
			SNDRV_PCM_FMTBIT_S32,
	},
	.ops = &ssm3525_dai_ops,
};

static struct snd_soc_codec_driver ssm3525_codec_nocomponent = {
	.set_bias_level = ssm3525_set_bias_level,
	.idle_bias_off = true,
};

static struct snd_soc_codec_driver ssm3525_codec_driver = {
	.set_bias_level = ssm3525_set_bias_level,
	.idle_bias_off = true,

	.component_driver = {
		.controls		= ssm3525_snd_controls,
		.num_controls		= ARRAY_SIZE(ssm3525_snd_controls),
	},
};

static const struct regmap_config ssm3525_regmap_config = {
	.val_bits = 8,
	.reg_bits = 8,

	.max_register = SSM3525_REG_SOFT_RESET,
	.readable_reg = ssm3525_readable_reg,
	.writeable_reg = ssm3525_writeable_reg,
	.volatile_reg = ssm3525_volatile_reg,
/*
 * TODO: Activate register map cache. It has been temporarily deactivated to
 * eliminate a potential source of trouble during driver development.
 */
	/*.cache_type = REGCACHE_RBTREE,*/
	/*.reg_defaults = ssm3525_reg_defaults,*/
	/*.num_reg_defaults = ARRAY_SIZE(ssm3525_reg_defaults),*/
};

static int ssm3525_i2c_probe(struct i2c_client *i2c,
	const struct i2c_device_id *id)
{
	struct ssm3525 *ssm3525;
	int ret, i;

	device_num++;
	ssm3525 = devm_kzalloc(&i2c->dev, sizeof(*ssm3525), GFP_KERNEL);
	if (ssm3525 == NULL)
		return -ENOMEM;

	i2c_set_clientdata(i2c, ssm3525);

	ssm3525->regmap = devm_regmap_init_i2c(i2c, &ssm3525_regmap_config);
	if (IS_ERR(ssm3525->regmap))
		return PTR_ERR(ssm3525->regmap);

	ret = regmap_write(ssm3525->regmap, SSM3525_REG_SOFT_RESET, 0x01);
	if (ret)
		return ret;
	ret = regmap_write(ssm3525->regmap, SSM3525_REG_SOFT_RESET, 0x00);
	if (ret)
		return ret;

	for (i = 0; i < ARRAY_SIZE(ssm3525_reg_defaults); i++) {
		ret = regmap_write(ssm3525->regmap,
			ssm3525_reg_defaults[i].reg,
			ssm3525_reg_defaults[i].def);
		if (ret)
			return ret;
	}
#ifdef CONFIG_OF
	if (i2c->dev.of_node) {
		const struct device_node *np = i2c->dev.of_node;
		u32 val;

		if (of_property_read_u32(np, "slots_mask", &val) >= 0)
			ssm3525->slots_mask = val;

		of_property_read_string(np, "amp_config", &ssm3525->amp_cfg);

		if (of_property_read_u32(np, "total_num", &val) >= 0)
			ssm3525->total_num = val;
	}
#endif
	if (device_num == 1) {
		g_ssm3525 = kzalloc(sizeof(struct ssm3525) *
				ssm3525->total_num, GFP_KERNEL);
		if (g_ssm3525 == NULL)
			return -ENOMEM;
	}
	g_ssm3525[device_num-1] = *ssm3525;

	if (device_num == ssm3525->total_num) {
		ret = snd_soc_register_codec(&i2c->dev, &ssm3525_codec_driver,
			&ssm3525_dai, 1);

	} else {
		ret = snd_soc_register_codec(&i2c->dev,
			&ssm3525_codec_nocomponent,
			&ssm3525_dai, 1);
	}
	if (ret)
		return ret;
	return 0;
}

static int ssm3525_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id ssm3525_dt_ids[] = {
	{ .compatible = "adi, ssm3525", },
	{ }
};
MODULE_DEVICE_TABLE(of, ssm3525_dt_ids);
#endif
static const struct i2c_device_id ssm3525_i2c_ids[] = {
	{ "ssm3525", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ssm3525_i2c_ids);


static struct i2c_driver ssm3525_driver = {
	.driver = {
		.name = "ssm3525",
		.of_match_table = of_match_ptr(ssm3525_dt_ids),
	},
	.probe = ssm3525_i2c_probe,
	.remove = ssm3525_i2c_remove,
	.id_table = ssm3525_i2c_ids,
};
module_i2c_driver(ssm3525_driver);

MODULE_DESCRIPTION("ASoC SSM3525 driver");
MODULE_AUTHOR("Anatol Pomozov <anatol@chromium.org>");
MODULE_LICENSE("GPL");
