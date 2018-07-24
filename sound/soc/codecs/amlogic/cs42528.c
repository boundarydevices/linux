#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <linux/amlogic/aml_gpio_consumer.h>

#include "cs42528.h"

#define DEV_NAME	"cs42528"


#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
static void cs42528_early_suspend(struct early_suspend *h);
static void cs42528_late_resume(struct early_suspend *h);
#endif

/* Power-up register defaults */
struct reg_default cs42528_reg_defaults[] = {
	{ 0x00, 0x00 },
	{ 0x03, 0x08 },	/* Functional Mode */
	{ 0x04, 0x40 },	/* Interface Formats */
	{ 0x05, 0x80 },	/* Misc Control */
	{ 0x06, 0x02 },	/* Clock Control */
	{ 0x0D, 0x28 },	/* Volume Control */
	{ 0x0E, 0xff },	/* Channel Mute */
	{ 0x0F, 0x00 },	/* Volume Control A1 */
	{ 0x10, 0x00 },	/* Volume Control B1 */
	{ 0x11, 0x00 },	/* Volume Control A2 */
	{ 0x12, 0x00 },	/* Volume Control B2 */
	{ 0x13, 0x00 },	/* Volume Control A3 */
	{ 0x14, 0x00 },	/* Volume Control B3 */
	{ 0x15, 0x00 },	/* Volume Control A4 */
	{ 0x16, 0x00 },	/* Volume Control B4 */
	{ 0x17, 0x00 },	/* Channel invert */
	{ 0x18, 0x09 },	/* Mixing Ctrl Pair 1 */
	{ 0x19, 0x09 },	/* Mixing Ctrl Pair 2 */
	{ 0x1A, 0x09 },	/* Mixing Ctrl Pair 3 */
	{ 0x1B, 0x09 },	/* Mixing Ctrl Pair 4 */
	{ 0x1C, 0x00 },	/* ADC Left Ch. Gain */
	{ 0x1D, 0x00 },	/* ADC Right Ch. Gain */
	{ 0x1E, 0x80 },	/* RVCR Mode Ctrl 1 */
	{ 0x1F, 0x00 },	/* RVCR Mode Ctrl 2 */
	{ 0x24, 0x00 },	/* Channel Status Data Buffer Control */
	{ 0x28, 0x1f },	/* MuteC Pin Control */
};

#define CS42528_REG_COUNT 26
static int cs42528_reg_table[CS42528_REG_COUNT][2] = {
	{ 0x00, 0x00 },
	{ 0x03, 0x08 },	/* Functional Mode */
	{ 0x04, 0x40 },	/* Interface Formats */
	{ 0x05, 0x80 },	/* Misc Control */
	{ 0x06, 0x02 },	/* Clock Control */
	{ 0x0D, 0x28 },	/* Volume Control */
	{ 0x0E, 0xff },	/* Channel Mute */
	{ 0x0F, 0x00 },	/* Volume Control A1 */
	{ 0x10, 0x00 },	/* Volume Control B1 */
	{ 0x11, 0x00 },	/* Volume Control A2 */
	{ 0x12, 0x00 },	/* Volume Control B2 */
	{ 0x13, 0x00 },	/* Volume Control A3 */
	{ 0x14, 0x00 },	/* Volume Control B3 */
	{ 0x15, 0x00 },	/* Volume Control A4 */
	{ 0x16, 0x00 },	/* Volume Control B4 */
	{ 0x17, 0x00 },	/* Channel invert */
	{ 0x18, 0x09 },	/* Mixing Ctrl Pair 1 */
	{ 0x19, 0x09 },	/* Mixing Ctrl Pair 2 */
	{ 0x1A, 0x09 },	/* Mixing Ctrl Pair 3 */
	{ 0x1B, 0x09 },	/* Mixing Ctrl Pair 4 */
	{ 0x1C, 0x00 },	/* ADC Left Ch. Gain */
	{ 0x1D, 0x00 },	/* ADC Right Ch. Gain */
	{ 0x1E, 0x80 },	/* RVCR Mode Ctrl 1 */
	{ 0x1F, 0x00 },	/* RVCR Mode Ctrl 2 */
	{ 0x24, 0x00 },	/* Channel Status Data Buffer Control */
	{ 0x28, 0x1f },	/* MuteC Pin Control */
};

#define CS42528_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | \
			 SNDRV_PCM_FMTBIT_S24_LE | \
			 SNDRV_PCM_FMTBIT_S32_LE)

/* codec private data */
struct cs42528_priv {
	struct regmap *regmap;
	struct snd_soc_codec *codec;
	struct cs42528_platform_data *pdata;
	int sysclk;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
};

/* -127dB to 0dB with step of 0.5dB */
static const DECLARE_TLV_DB_SCALE(dac_tlv, -12700, 50, 0);

/* -15dB to 15dB with step of 1dB */
static const DECLARE_TLV_DB_SCALE(adc_tlv, -1500, 100, 0);

static const char *const cs42528_szc[] = {
	"Immediate Change",
	"Zero Cross",
	"Soft Ramp",
	"Soft Ramp on Zero Cross"
};

static const struct soc_enum dac_szc_enum =
	SOC_ENUM_SINGLE(CS42528_TXCTL, 4, 4, cs42528_szc);

static const struct snd_kcontrol_new cs42528_snd_controls[] = {
	SOC_DOUBLE_R_TLV("DAC1 Playback Volume", CS42528_VOLCTLA1,
			 CS42528_VOLCTLB1, 0, 0xff, 1, dac_tlv),
	SOC_DOUBLE_R_TLV("DAC2 Playback Volume", CS42528_VOLCTLA2,
			 CS42528_VOLCTLB2, 0, 0xff, 1, dac_tlv),
	SOC_DOUBLE_R_TLV("DAC3 Playback Volume", CS42528_VOLCTLA3,
			 CS42528_VOLCTLB3, 0, 0xff, 1, dac_tlv),
	SOC_DOUBLE_R_TLV("DAC4 Playback Volume", CS42528_VOLCTLA4,
			 CS42528_VOLCTLB4, 0, 0xff, 1, dac_tlv),
	SOC_DOUBLE_R_S_TLV("ADC Channel Gain", CS42528_ADCLGAIN,
			   CS42528_ADCRGAIN, 0, -0x0f, 0x0f, 5, 0, adc_tlv),
	SOC_DOUBLE("DAC1 Invert Switch", CS42528_CHINV, 0, 1, 1, 0),
	SOC_DOUBLE("DAC2 Invert Switch", CS42528_CHINV, 2, 3, 1, 0),
	SOC_DOUBLE("DAC3 Invert Switch", CS42528_CHINV, 4, 5, 1, 0),
	SOC_DOUBLE("DAC4 Invert Switch", CS42528_CHINV, 6, 7, 1, 0),
	SOC_SINGLE("DAC Single Volume Control Switch", CS42528_TXCTL, 6, 1, 0),
	SOC_ENUM("DAC Soft Ramp & Zero Cross Control Switch", dac_szc_enum),
	SOC_SINGLE("DAC Auto Mute Switch", CS42528_TXCTL, 3, 1, 0),
	SOC_SINGLE("DAC Serial Port Mute Switch", CS42528_TXCTL, 2, 1, 0),
};

#if 0
static const struct snd_soc_dapm_widget cs42528_dapm_widgets[] = {
	SND_SOC_DAPM_DAC("DAC1", "Playback", CS42528_PWRCTL, 1, 1),
	SND_SOC_DAPM_DAC("DAC2", "Playback", CS42528_PWRCTL, 2, 1),
	SND_SOC_DAPM_DAC("DAC3", "Playback", CS42528_PWRCTL, 3, 1),
	SND_SOC_DAPM_DAC("DAC4", "Playback", CS42528_PWRCTL, 4, 1),

	SND_SOC_DAPM_OUTPUT("AOUT1L"),
	SND_SOC_DAPM_OUTPUT("AOUT1R"),
	SND_SOC_DAPM_OUTPUT("AOUT2L"),
	SND_SOC_DAPM_OUTPUT("AOUT2R"),
	SND_SOC_DAPM_OUTPUT("AOUT3L"),
	SND_SOC_DAPM_OUTPUT("AOUT3R"),
	SND_SOC_DAPM_OUTPUT("AOUT4L"),
	SND_SOC_DAPM_OUTPUT("AOUT4R"),

	SND_SOC_DAPM_ADC("ADC", "Capture", CS42528_PWRCTL, 5, 1),

	SND_SOC_DAPM_INPUT("AIN1L"),
	SND_SOC_DAPM_INPUT("AIN1R"),
	SND_SOC_DAPM_INPUT("AIN2L"),
	SND_SOC_DAPM_INPUT("AIN2R"),

	SND_SOC_DAPM_SUPPLY("PWR", CS42528_PWRCTL, 0, 1, NULL, 0),
};

static const struct snd_soc_dapm_route cs42528_dapm_routes[] = {
	/* Playback */
	{ "AOUT1L", NULL, "DAC1" },
	{ "AOUT1R", NULL, "DAC1" },
	{ "DAC1", NULL, "PWR" },

	{ "AOUT2L", NULL, "DAC2" },
	{ "AOUT2R", NULL, "DAC2" },
	{ "DAC2", NULL, "PWR" },

	{ "AOUT3L", NULL, "DAC3" },
	{ "AOUT3R", NULL, "DAC3" },
	{ "DAC3", NULL, "PWR" },

	{ "AOUT4L", NULL, "DAC4" },
	{ "AOUT4R", NULL, "DAC4" },
	{ "DAC4", NULL, "PWR" },

	/* Capture */
	{ "ADC1", NULL, "AIN1L" },
	{ "ADC1", NULL, "AIN1R" },
	{ "ADC1", NULL, "PWR" },

	{ "ADC2", NULL, "AIN2L" },
	{ "ADC2", NULL, "AIN2R" },
	{ "ADC2", NULL, "PWR" },
};
#endif

static int cs42528_set_dai_sysclk(struct snd_soc_dai *codec_dai,
				  int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct cs42528_priv *cs42528 = snd_soc_codec_get_drvdata(codec);

	cs42528->sysclk = freq;

	pr_info("%s, %d  clk:%d\n", __func__, __LINE__, freq);

	return 0;
}

static int cs42528_set_dai_fmt(struct snd_soc_dai *codec_dai,
			       unsigned int format)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct cs42528_priv *cs42528 = snd_soc_codec_get_drvdata(codec);
	u32 val;

	pr_info("%s, %d  format:%d\n", __func__, __LINE__, format);

	/* Set DAI format */
	switch (format & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_LEFT_J:
		val = CS42528_INTF_DAC_DIF_LEFTJ;
		break;
	case SND_SOC_DAIFMT_I2S:
		val = CS42528_INTF_DAC_DIF_I2S;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		val = CS42528_INTF_DAC_DIF_RIGHTJ;
		break;
	default:
		dev_err(codec->dev, "unsupported dai format\n");
		return -EINVAL;
	}

	regmap_update_bits(cs42528->regmap, CS42528_INTF,
			   CS42528_INTF_DAC_DIF_MASK, val);

	/* Set master/slave audio interface */
	switch (format & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		break;
	default:
		dev_err(codec->dev, "unsupported master/slave mode\n");
		return -EINVAL;
	}

	return 0;
}

static int cs42528_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params,
			     struct snd_soc_dai *dai)
{
	unsigned int rate;

	rate = params_rate(params);

	pr_info("%s %d rate: %u\n", __func__, __LINE__, rate);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S24_BE:
		pr_debug("24bit\n");
		break;

	case SNDRV_PCM_FORMAT_S32_LE:
	case SNDRV_PCM_FORMAT_S20_3LE:
	case SNDRV_PCM_FORMAT_S20_3BE:
		pr_debug("20bit\n");
		break;

	case SNDRV_PCM_FORMAT_S16_LE:
	case SNDRV_PCM_FORMAT_S16_BE:
		pr_debug("16bit\n");
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int cs42528_digital_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;

	pr_info("%s %d mute: %d\n", __func__, __LINE__, mute);

	if (mute)
		snd_soc_write(codec, CS42528_DACMUTE, 0xff);
	else
		snd_soc_write(codec, CS42528_DACMUTE, 0);

	return 0;
}

static const struct snd_soc_dai_ops cs42528_dai_ops = {
	.hw_params = cs42528_hw_params,
	.set_sysclk = cs42528_set_dai_sysclk,
	.set_fmt = cs42528_set_dai_fmt,
	.digital_mute	= cs42528_digital_mute,
};

static struct snd_soc_dai_driver cs42528_dai = {
	.name = DEV_NAME,
	.playback = {
		.stream_name = "HIFI Playback",
		.channels_min = 2,
		.channels_max = 8,
		.rates = SNDRV_PCM_RATE_8000_192000,
		.formats = CS42528_FORMATS,
	},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 2,
		.channels_max = 8,
		.rates = SNDRV_PCM_RATE_8000_192000,
		.formats = CS42528_FORMATS,
	},
	.ops = &cs42528_dai_ops,
};

static int reset_cs42528(struct snd_soc_codec *codec)
{
	struct cs42528_priv *cs42528 = snd_soc_codec_get_drvdata(codec);
	struct cs42528_platform_data *pdata = cs42528->pdata;
	int ret = 0;

	if (pdata->reset_pin < 0)
		return 0;

	ret = devm_gpio_request_one(codec->dev, pdata->reset_pin,
					    GPIOF_OUT_INIT_LOW,
					    "cs42528-reset-pin");
	if (ret < 0)
		return -1;

	gpio_direction_output(pdata->reset_pin, GPIOF_OUT_INIT_LOW);
	mdelay(1);
	gpio_direction_output(pdata->reset_pin, GPIOF_OUT_INIT_HIGH);
	msleep(85);

	return 0;
}

static int cs42528_init(struct snd_soc_codec *codec)
{
	int i;

	dev_info(codec->dev, "cs42528_init!\n");

	reset_cs42528(codec);

	for (i = 0; i < CS42528_REG_COUNT; i++) {
		snd_soc_write(codec, cs42528_reg_table[i][0],
				cs42528_reg_table[i][1]);
	}

	// Short delay to finish register writes
	msleep(10);

	// Write CS42528 Powerup Register
	snd_soc_write(codec, CS42528_PWRCTL, 0x40);

	return 0;
}

static int cs42528_probe(struct snd_soc_codec *codec)
{

#ifdef CONFIG_HAS_EARLYSUSPEND
	cs42528->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	cs42528->early_suspend.suspend = cs42528_early_suspend;
	cs42528->early_suspend.resume = cs42528_late_resume;
	cs42528->early_suspend.param = codec;
	register_early_suspend(&(cs42528->early_suspend));
#endif

	cs42528_init(codec);

	return 0;
}

static int cs42528_remove(struct snd_soc_codec *codec)
{
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct cs42528_priv *cs42528 = snd_soc_codec_get_drvdata(codec);

	unregister_early_suspend(&(cs42528->early_suspend));
#endif

	return 0;
}

#ifdef CONFIG_PM
static int cs42528_suspend(struct snd_soc_codec *codec)
{
	dev_info(codec->dev, "cs42528_suspend!\n");
	return 0;
}

static int cs42528_resume(struct snd_soc_codec *codec)
{
	dev_info(codec->dev, "cs42528_resume!\n");
	return 0;
}
#else
#define cs42528_suspend NULL
#define cs42528_resume NULL
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void cs42528_early_suspend(struct early_suspend *h)
{
}

static void cs42528_late_resume(struct early_suspend *h)
{
}
#endif

static const struct snd_soc_codec_driver soc_codec_dev_cs42528 = {
	.probe = cs42528_probe,
	.remove = cs42528_remove,
	.suspend = cs42528_suspend,
	.resume = cs42528_resume,
	.component_driver = {
		.controls		= cs42528_snd_controls,
		.num_controls		= ARRAY_SIZE(cs42528_snd_controls),
		//.dapm_widgets		= cs42528_dapm_widgets,
		//.num_dapm_widgets	= ARRAY_SIZE(cs42528_dapm_widgets),
		//.dapm_routes		= cs42528_dapm_routes,
		//.num_dapm_routes	= ARRAY_SIZE(cs42528_dapm_routes),
	}
};

static const struct regmap_config cs42528_regmap = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = CS42528_NUMREGS,
	.reg_defaults = cs42528_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(cs42528_reg_defaults),
	.cache_type = REGCACHE_RBTREE,
};

static int cs42528_parse_dt(
	struct cs42528_priv *cs42528,
	struct device_node *np)
{
	int ret = 0;
	int reset_pin = -1;

	reset_pin = of_get_named_gpio(np, "reset_pin", 0);
	if (reset_pin < 0) {
		pr_err("%s fail to get reset pin from dts!\n", __func__);
		ret = -1;
	} else {
		pr_info("%s reset_pin = %d!\n", __func__, reset_pin);
	}
	cs42528->pdata->reset_pin = reset_pin;

	return ret;
}

static int cs42528_i2c_probe(struct i2c_client *i2c,
			     const struct i2c_device_id *id)
{
	struct cs42528_priv *cs42528;
	struct cs42528_platform_data *pdata;
	int ret = 0;
	const char *codec_name;

	cs42528 = devm_kzalloc(&i2c->dev, sizeof(struct cs42528_priv),
			       GFP_KERNEL);
	if (!cs42528)
		return -ENOMEM;

	cs42528->regmap = devm_regmap_init_i2c(i2c, &cs42528_regmap);
	if (IS_ERR(cs42528->regmap)) {
		ret = PTR_ERR(cs42528->regmap);
		dev_err(&i2c->dev, "Failed to allocate register map: %d\n",
			ret);
		return ret;
	}

	pdata = devm_kzalloc(&i2c->dev,
				sizeof(struct cs42528_platform_data),
				GFP_KERNEL);
	if (!pdata) {
		pr_err("%s failed to kzalloc for cs42528 pdata\n", __func__);
		return -ENOMEM;
	}

	cs42528->pdata = pdata;

	cs42528_parse_dt(cs42528, i2c->dev.of_node);

	if (of_property_read_string(i2c->dev.of_node,
			"codec_name",
				&codec_name)) {
		pr_info("no codec name\n");
		return -1;
	}
	pr_info("codec name = %s\n", codec_name);
	if (codec_name)
		dev_set_name(&i2c->dev, "%s", codec_name);

	i2c_set_clientdata(i2c, cs42528);

	ret = snd_soc_register_codec(&i2c->dev, &soc_codec_dev_cs42528,
				     &cs42528_dai, 1);
	if (ret != 0)
		dev_err(&i2c->dev, "Failed to register codec (%d)\n", ret);

	return ret;
}

static int cs42528_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);

	return 0;
}

static const struct i2c_device_id cs42528_i2c_id[] = {
	{ "cs42528", 0 },
	{}
};

static const struct of_device_id cs42528_of_id[] = {
	{.compatible = "cirrus,cs42528",},
	{ /* senitel */ }
};
MODULE_DEVICE_TABLE(of, cs42528_of_id);

static struct i2c_driver cs42528_i2c_driver = {
	.driver = {
		.name = DEV_NAME,
		.of_match_table = cs42528_of_id,
		.owner = THIS_MODULE,
	},
	.probe = cs42528_i2c_probe,
	.remove = cs42528_i2c_remove,
	.id_table = cs42528_i2c_id,
};
module_i2c_driver(cs42528_i2c_driver);

MODULE_DESCRIPTION("ASoC CS42528 driver");
MODULE_AUTHOR("AML MM team");
MODULE_LICENSE("GPL");
