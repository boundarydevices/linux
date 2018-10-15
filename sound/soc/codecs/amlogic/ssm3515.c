/*
 * SSM3515 amplifier audio driver
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

#define SSM3515_REG_POWER_CTRL		0x00
#define SSM3515_REG_AMP_SNS_CTRL		0x01
#define SSM3515_REG_DAC_CTRL		0x02
#define SSM3515_REG_DAC_VOLUME		0x03
#define SSM3515_REG_SAI_CTRL_1		0x04
#define SSM3515_REG_SAI_CTRL_2		0x05
#define SSM3515_REG_BATTERY_V_OUT		0x06
#define SSM3515_REG_LIMITER_CTRL_1		0x07
#define SSM3515_REG_LIMITER_CTRL_2		0x08
#define SSM3515_REG_LIMITER_CTRL_3		0x09
#define SSM3515_REG_STATUS_1		0x0A
#define SSM3515_REG_FAULT_CTRL		0x0B

/* POWER_CTRL */
#define SSM3515_POWER_APWDN_EN		BIT(7)
#define SSM3515_POWER_BSNS_PWDN		BIT(6)
#define SSM3515_POWER_S_RESET		BIT(1)
#define SSM3515_POWER_SPWDN			BIT(0)

/* DAC_CTRL */
#define SSM3515_DAC_HV			BIT(7)
#define SSM3515_DAC_MUTE		BIT(6)
#define SSM3515_DAC_HPF			BIT(5)
#define SSM3515_DAC_LPM			BIT(4)
#define SSM3515_DAC_FS_MASK	0x7
#define SSM3515_DAC_FS_8000_12000	0x0
#define SSM3515_DAC_FS_16000_24000	0x1
#define SSM3515_DAC_FS_32000_48000	0x2
#define SSM3515_DAC_FS_64000_96000	0x3
#define SSM3515_DAC_FS_128000_192000	0x4

/* SAI_CTRL_1 */
#define SSM3515_SAI_CTRL_1_BCLK			BIT(6)
#define SSM3515_SAI_CTRL_1_TDM_BLCKS_MASK	(0x3 << 3)
#define SSM3515_SAI_CTRL_1_TDM_BLCKS_32		(0x0 << 3)
#define SSM3515_SAI_CTRL_1_TDM_BLCKS_48		(0x1 << 3)
#define SSM3515_SAI_CTRL_1_TDM_BLCKS_64		(0x2 << 3)
#define SSM3515_SAI_CTRL_1_FSYNC		BIT(2)
#define SSM3515_SAI_CTRL_1_LJ			BIT(1)
#define SSM3515_SAI_CTRL_1_TDM			BIT(0)

/* SAI_CTRL_2 */
#define SSM3515_SAI_CTRL_2_AUTO_SLOT		BIT(3)
#define SSM3515_SAI_CTRL_2_TDM_SLOT_MASK	0x7
#define SSM3515_SAI_CTRL_2_TDM_SLOT(x)		(x)

static int device_num;
struct ssm3515 *g_ssm3515;

struct ssm3515 {
	struct regmap *regmap;
	unsigned int slots_mask;
	const char *amp_cfg;
	int total_num;
};

static const struct reg_default ssm3515_reg_defaults[] = {
	{ SSM3515_REG_POWER_CTRL,	0x81 },
	{ SSM3515_REG_AMP_SNS_CTRL, 0x03 },
	{ SSM3515_REG_DAC_CTRL, 0x22 },
	{ SSM3515_REG_DAC_VOLUME, 0x40 },
	{ SSM3515_REG_SAI_CTRL_1, 0x11 },
};


static bool ssm3515_readable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case SSM3515_REG_POWER_CTRL ... SSM3515_REG_FAULT_CTRL:
		return true;
	default:
		return false;
	}

}

static bool ssm3515_writeable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case SSM3515_REG_POWER_CTRL ... SSM3515_REG_SAI_CTRL_2:
	case SSM3515_REG_LIMITER_CTRL_1 ... SSM3515_REG_LIMITER_CTRL_3:
	case SSM3515_REG_FAULT_CTRL:
		return true;
	 /*The datasheet states that soft reset register is read-only,*/
	 /*but logically it is write-only. */
	default:
		return false;
	}
}

static bool ssm3515_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case SSM3515_REG_BATTERY_V_OUT:
	case SSM3515_REG_STATUS_1:
	default:
		return false;
	}
}

static int WL2_amplifier_power_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	int value = 0;
	int ret;
	int i;
	struct ssm3515 *ssm3515 = NULL;
	ret = -1;
	for (i = 0; i < device_num; i++) {
		if (!strcmp(g_ssm3515[i].amp_cfg, "WL2")) {
			ssm3515 = g_ssm3515+i;
			break;
		}
	}
	if (!ssm3515) {
		pr_info("no wl2 error");
		return ret;
	}
	ret = regmap_read(ssm3515->regmap, SSM3515_REG_POWER_CTRL, &value);
	if (ret)
		return ret;
	ucontrol->value.integer.value[0] = value;
	return 0;
}

static int WL2_amplifier_power_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	int value, ret, i;
	struct ssm3515 *ssm3515 = NULL;
	ret = -1;
	for (i = 0; i < device_num; i++) {
		if (!strcmp(g_ssm3515[i].amp_cfg, "WL2")) {
			ssm3515 = g_ssm3515+i;
			break;
		}
	}
	if (!ssm3515) {
		pr_info("no wl2 error");
		return ret;
	}
	value = ucontrol->value.integer.value[0];
	ret = regmap_write(ssm3515->regmap, SSM3515_REG_POWER_CTRL, value);
	if (ret)
		return ret;
	return 0;
}

static int WR2_amplifier_power_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	int value = 0;
	int ret, i;
	struct ssm3515 *ssm3515 = NULL;
	ret = -1;
	for (i = 0; i < device_num; i++) {
		if (!strcmp(g_ssm3515[i].amp_cfg, "WR2")) {
			ssm3515 = g_ssm3515+i;
			break;
		}
	}
	if (!ssm3515) {
		pr_info("no wr2 error");
		return ret;
	}
	ret = regmap_read(ssm3515->regmap, SSM3515_REG_POWER_CTRL, &value);
	if (ret)
		return ret;
	ucontrol->value.integer.value[0] = value;
	return 0;
}

static int WR2_amplifier_power_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	int value, ret, i;
	struct ssm3515 *ssm3515 = NULL;
	ret = -1;
	for (i = 0; i < device_num; i++) {
		if (!strcmp(g_ssm3515[i].amp_cfg, "WR2")) {
			ssm3515 = g_ssm3515+i;
			break;
		}
	}
	if (!ssm3515) {
		pr_info("no wr2 error");
		return ret;
	}
	value = ucontrol->value.integer.value[0];
	ret = regmap_write(ssm3515->regmap, SSM3515_REG_POWER_CTRL, value);
	if (ret)
		return ret;
	return 0;
}

static int TL1_amplifier_power_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	int value = 0;
	int ret, i;
	struct ssm3515 *ssm3515 = NULL;
	ret = -1;
	for (i = 0; i < device_num; i++) {
		if (!strcmp(g_ssm3515[i].amp_cfg, "TL1")) {
			ssm3515 = g_ssm3515+i;
			break;
		}
	}
	if (!ssm3515) {
		pr_info("no tl1 error");
		return ret;
	}
	ret = regmap_read(ssm3515->regmap, SSM3515_REG_POWER_CTRL, &value);
	if (ret)
		return ret;
	ucontrol->value.integer.value[0] = value;
	return 0;
}

static int TL1_amplifier_power_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	int value, ret, i;
	struct ssm3515 *ssm3515 = NULL;
	ret = -1;
	for (i = 0; i < device_num; i++) {
		if (!strcmp(g_ssm3515[i].amp_cfg, "TL1")) {
			ssm3515 = g_ssm3515+i;
			break;
		}
	}
	if (!ssm3515) {
		pr_info("no tl1 error");
		return ret;
	}
	value = ucontrol->value.integer.value[0];
	ret = regmap_write(ssm3515->regmap, SSM3515_REG_POWER_CTRL, value);
	if (ret)
		return ret;
	return 0;
}

static int TR1_amplifier_power_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	int value = 0;
	int ret, i;
	struct ssm3515 *ssm3515 = NULL;
	ret = -1;
	for (i = 0; i < device_num; i++) {
		if (!strcmp(g_ssm3515[i].amp_cfg, "TR1")) {
			ssm3515 = g_ssm3515+i;
			break;
		}
	}
	if (!ssm3515) {
		pr_info("no tr1 error");
		return ret;
	}
	ret = regmap_read(ssm3515->regmap, SSM3515_REG_POWER_CTRL, &value);
	if (ret)
		return ret;
	ucontrol->value.integer.value[0] = value;
	return 0;
}

static int TR1_amplifier_power_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	int value, ret, i;
	struct ssm3515 *ssm3515 = NULL;
	ret = -1;
	for (i = 0; i < device_num; i++) {
		if (!strcmp(g_ssm3515[i].amp_cfg, "TR1")) {
			ssm3515 = g_ssm3515+i;
			break;
		}
	}
	if (!ssm3515) {
		pr_info("no tl1 error");
		return ret;
	}
	value = ucontrol->value.integer.value[0];
	ret = regmap_write(ssm3515->regmap, SSM3515_REG_POWER_CTRL, value);
	if (ret)
		return ret;
	return 0;
}

static struct snd_kcontrol_new ssm3515_snd_controls[] = {
	SOC_SINGLE_EXT("WR2 Amp Power Control", SND_SOC_NOPM, 0, 0xff, 0,
		   WR2_amplifier_power_get, WR2_amplifier_power_put),
	SOC_SINGLE_EXT("WL2 Amp Power Control", SND_SOC_NOPM, 0, 0xff, 0,
		   WL2_amplifier_power_get, WL2_amplifier_power_put),
	SOC_SINGLE_EXT("TL1 Amp Power Control", SND_SOC_NOPM, 0, 0xff, 0,
		   TL1_amplifier_power_get, TL1_amplifier_power_put),
	SOC_SINGLE_EXT("TR1 Amp Power Control", SND_SOC_NOPM, 0, 0xff, 0,
		   TR1_amplifier_power_get, TR1_amplifier_power_put),
};

static int ssm3515_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct ssm3515 *ssm3515 = snd_soc_codec_get_drvdata(codec);
	unsigned int rate = params_rate(params);
	unsigned int dacfs;

	if (rate >= 8000 && rate <= 12000)
		dacfs = SSM3515_DAC_FS_8000_12000;
	else if (rate >= 16000 && rate <= 24000)
		dacfs = SSM3515_DAC_FS_16000_24000;
	else if (rate >= 32000 && rate <= 48000)
		dacfs = SSM3515_DAC_FS_32000_48000;
	else if (rate >= 64000 && rate <= 96000)
		dacfs = SSM3515_DAC_FS_64000_96000;
	else if (rate >= 128000 && rate <= 192000)
		dacfs = SSM3515_DAC_FS_128000_192000;
	else
		return -EINVAL;

	return regmap_update_bits(ssm3515->regmap, SSM3515_REG_DAC_CTRL,
				SSM3515_DAC_FS_MASK, dacfs);
}

static int ssm3515_mute(struct snd_soc_dai *dai, int mute)
{
	struct ssm3515 *ssm3515 = snd_soc_codec_get_drvdata(dai->codec);
	unsigned int val;

	val = mute ? SSM3515_DAC_MUTE : 0;
	return regmap_update_bits(ssm3515->regmap, SSM3515_REG_DAC_CTRL,
			SSM3515_DAC_MUTE, val);
}

static int ssm3515_set_tdm_slot(struct snd_soc_dai *dai, unsigned int tx_mask,
	unsigned int rx_mask, int slots, int width)
{
	struct ssm3515 *ssm3515 = snd_soc_dai_get_drvdata(dai);
	unsigned int blcks;
	int slot;
	int ret;

	tx_mask = ssm3515->slots_mask;
	if (tx_mask == 0)
		return -EINVAL;

	slot = __ffs(tx_mask);
	if (tx_mask != BIT(slot))
		return -EINVAL;

	switch (width) {
	case 32:
		blcks = SSM3515_SAI_CTRL_1_TDM_BLCKS_32;
		break;
	case 48:
		blcks = SSM3515_SAI_CTRL_1_TDM_BLCKS_48;
		break;
	case 64:
		blcks = SSM3515_SAI_CTRL_1_TDM_BLCKS_64;
		break;
	default:
		return -EINVAL;
	}

	ret = regmap_update_bits(ssm3515->regmap, SSM3515_REG_SAI_CTRL_2,
		SSM3515_SAI_CTRL_2_AUTO_SLOT | SSM3515_SAI_CTRL_2_TDM_SLOT_MASK,
		SSM3515_SAI_CTRL_2_TDM_SLOT(slot));
	if (ret)
		return ret;

	return regmap_update_bits(ssm3515->regmap, SSM3515_REG_SAI_CTRL_1,
		SSM3515_SAI_CTRL_1_TDM_BLCKS_MASK, blcks);
}

static int ssm3515_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
#if 0
	struct ssm3515 *ssm3515 = snd_soc_dai_get_drvdata(dai);
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
		ctrl1 |= SSM3515_SAI_CTRL_1_BCLK;
		invert_fclk = false;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		ctrl1 |= SSM3515_SAI_CTRL_1_FSYNC;
		invert_fclk = true;
		break;
	case SND_SOC_DAIFMT_IB_IF:
		ctrl1 |= SSM3515_SAI_CTRL_1_BCLK;
		invert_fclk = true;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		ctrl1 |= SSM3515_SAI_CTRL_1_LJ;
		invert_fclk = !invert_fclk;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		ctrl1 |= SSM3515_SAI_CTRL_1_TDM;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		ctrl1 |= SSM3515_SAI_CTRL_1_TDM | SSM3515_SAI_CTRL_1_LJ;
		break;
	default:
		return -EINVAL;
	}

	if (invert_fclk)
		ctrl1 |= SSM3515_SAI_CTRL_1_FSYNC;

	return regmap_update_bits(ssm3515->regmap, SSM3515_REG_SAI_CTRL_1,
			SSM3515_SAI_CTRL_1_BCLK |
			SSM3515_SAI_CTRL_1_FSYNC |
			SSM3515_SAI_CTRL_1_LJ |
			SSM3515_SAI_CTRL_1_TDM,
			ctrl1);
#endif
	return 0;
}

static int ssm3515_set_power(struct ssm3515 *ssm3515, bool enable)
{
	int ret = 0;

	if (!enable) {
		ret = regmap_update_bits(ssm3515->regmap,
			SSM3515_REG_POWER_CTRL,
			SSM3515_POWER_SPWDN, SSM3515_POWER_SPWDN);
	}

	if (enable) {
		ret = regmap_write(ssm3515->regmap, SSM3515_REG_POWER_CTRL,
			SSM3515_POWER_S_RESET);
		if (ret)
			return ret;

		ret = regmap_update_bits(ssm3515->regmap,
			SSM3515_REG_POWER_CTRL,
			SSM3515_POWER_SPWDN, 0x00);
	}

	return ret;
}

static int ssm3515_set_bias_level(struct snd_soc_codec *codec,
	enum snd_soc_bias_level level)
{
	struct ssm3515 *ssm3515 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	switch (level) {
	case SND_SOC_BIAS_ON:
		break;
	case SND_SOC_BIAS_PREPARE:
		break;
	case SND_SOC_BIAS_STANDBY:
		if (snd_soc_codec_get_bias_level(codec) == SND_SOC_BIAS_OFF)
			ret = ssm3515_set_power(ssm3515, true);
		break;
	case SND_SOC_BIAS_OFF:
		ret = ssm3515_set_power(ssm3515, false);
		break;
	}

	return ret;
}

static const struct snd_soc_dai_ops ssm3515_dai_ops = {
	.hw_params	= ssm3515_hw_params,
	.digital_mute	= ssm3515_mute,
	.set_fmt	= ssm3515_set_dai_fmt,
	.set_tdm_slot	= ssm3515_set_tdm_slot,
};

static struct snd_soc_dai_driver ssm3515_dai = {
	.name = "ssm3515-hifi",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 8,
		.rates = SNDRV_PCM_RATE_8000_192000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
			SNDRV_PCM_FMTBIT_S32,
	},
	.capture = {
		.stream_name = "Capture ",
		.channels_min = 1,
		.channels_max = 8,
		.rates = SNDRV_PCM_RATE_8000_192000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
			SNDRV_PCM_FMTBIT_S32,
	},
	.ops = &ssm3515_dai_ops,
};

static struct snd_soc_codec_driver ssm3515_codec_nocomponent = {
	.set_bias_level = ssm3515_set_bias_level,
	.idle_bias_off = true,
};

static struct snd_soc_codec_driver ssm3515_codec_driver = {
	.set_bias_level = ssm3515_set_bias_level,
	.idle_bias_off = true,

	.component_driver = {
		.controls		= ssm3515_snd_controls,
		.num_controls		= ARRAY_SIZE(ssm3515_snd_controls),
	},
};

static const struct regmap_config ssm3515_regmap_config = {
	.val_bits = 8,
	.reg_bits = 8,

	.max_register = SSM3515_REG_FAULT_CTRL,
	.readable_reg = ssm3515_readable_reg,
	.writeable_reg = ssm3515_writeable_reg,
	.volatile_reg = ssm3515_volatile_reg,
/*
 * TODO: Activate register map cache. It has been temporarily deactivated to
 * eliminate a potential source of trouble during driver development.
 */
	/*.cache_type = REGCACHE_RBTREE,*/
	/*.reg_defaults = ssm3515_reg_defaults,*/
	/*.num_reg_defaults = ARRAY_SIZE(ssm3515_reg_defaults),*/
};

static int ssm3515_i2c_probe(struct i2c_client *i2c,
	const struct i2c_device_id *id)
{
	struct ssm3515 *ssm3515;
	int ret, i;

	device_num++;

	ssm3515 = devm_kzalloc(&i2c->dev, sizeof(*ssm3515), GFP_KERNEL);
	if (ssm3515 == NULL)
		return -ENOMEM;

	i2c_set_clientdata(i2c, ssm3515);

	ssm3515->regmap = devm_regmap_init_i2c(i2c, &ssm3515_regmap_config);
	if (IS_ERR(ssm3515->regmap))
		return PTR_ERR(ssm3515->regmap);

	ret = ssm3515_set_power(ssm3515, false);
	if (ret)
		return ret;

	ret = regmap_write(ssm3515->regmap, SSM3515_REG_POWER_CTRL,
		SSM3515_POWER_S_RESET);
	if (ret)
		return ret;

	ret = regmap_write(ssm3515->regmap, SSM3515_REG_POWER_CTRL,
		SSM3515_POWER_S_RESET);
	if (ret)
		return ret;

	for (i = 0; i < ARRAY_SIZE(ssm3515_reg_defaults); i++) {
		ret = regmap_write(ssm3515->regmap, ssm3515_reg_defaults[i].reg,
			ssm3515_reg_defaults[i].def);
		if (ret)
			return ret;
	}

#ifdef CONFIG_OF
	if (i2c->dev.of_node) {
		const struct device_node *np = i2c->dev.of_node;
		u32 val;

		if (of_property_read_u32(np, "slots_mask", &val) >= 0)
			ssm3515->slots_mask = val;

		of_property_read_string(np, "amp_config", &ssm3515->amp_cfg);

		if (of_property_read_u32(np, "total_num", &val) >= 0)
			ssm3515->total_num = val;
	}
#endif
	if (device_num == 1) {
		g_ssm3515 = kzalloc(sizeof(struct ssm3515) *
				ssm3515->total_num, GFP_KERNEL);
		if (g_ssm3515 == NULL)
			return -ENOMEM;
	}

	g_ssm3515[device_num-1] = *ssm3515;

	if (device_num == ssm3515->total_num) {
		ret = snd_soc_register_codec(&i2c->dev, &ssm3515_codec_driver,
			&ssm3515_dai, 1);
	} else {
		ret = snd_soc_register_codec(&i2c->dev,
			&ssm3515_codec_nocomponent, &ssm3515_dai, 1);
	}
	if (ret)
		return ret;
	return 0;
}

static int ssm3515_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id ssm3515_dt_ids[] = {
	{ .compatible = "adi, ssm3515", },
	{ }
};
MODULE_DEVICE_TABLE(of, ssm3515_dt_ids);
#endif

static const struct i2c_device_id ssm3515_i2c_ids[] = {
	{ " ssm3515", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ssm3515_i2c_ids);


static struct i2c_driver ssm3515_driver = {
	.driver = {
		.name = "ssm3515",
		.of_match_table = of_match_ptr(ssm3515_dt_ids),
	},
	.probe = ssm3515_i2c_probe,
	.remove = ssm3515_i2c_remove,
	.id_table = ssm3515_i2c_ids,
};
module_i2c_driver(ssm3515_driver);

MODULE_DESCRIPTION("ASoC SSM3515 driver");
MODULE_AUTHOR("Anatol Pomozov <anatol@chromium.org>");
MODULE_LICENSE("GPL");
