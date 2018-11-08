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
#include <sound/tas57xx.h>
#include <linux/amlogic/aml_gpio_consumer.h>

#include "tas5707.h"

#define DEV_NAME	"tas5707"

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
static void tas5707_early_suspend(struct early_suspend *h);
static void tas5707_late_resume(struct early_suspend *h);
#endif

#define TAS5707_RATES (SNDRV_PCM_RATE_8000 | \
		       SNDRV_PCM_RATE_11025 | \
		       SNDRV_PCM_RATE_16000 | \
		       SNDRV_PCM_RATE_22050 | \
		       SNDRV_PCM_RATE_32000 | \
		       SNDRV_PCM_RATE_44100 | \
		       SNDRV_PCM_RATE_48000)

#define TAS5707_FORMATS \
	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S16_BE | \
	 SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_S20_3BE | \
	 SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S24_BE | \
	 SNDRV_PCM_FMTBIT_S32_LE)

/* Power-up register defaults */
struct reg_default tas5707_reg_defaults[TAS5707_REGISTER_COUNT] = {
	{0x00, 0x6c},
	{0x01, 0x70},
	{0x02, 0x00},
	{0x03, 0xA0},
	{0x04, 0x05},
	{0x05, 0x40},
	{0x06, 0x00},
	{0x07, 0xFF},
	{0x08, 0x30},
	{0x09, 0x30},
	{0x0A, 0xFF},
	{0x0B, 0x00},
	{0x0C, 0x00},
	{0x0D, 0x00},
	{0x0E, 0x91},
	{0x10, 0x00},
	{0x11, 0x02},
	{0x12, 0xAC},
	{0x13, 0x54},
	{0x14, 0xAC},
	{0x15, 0x54},
	{0x16, 0x00},
	{0x17, 0x00},
	{0x18, 0x00},
	{0x19, 0x00},
	{0x1A, 0x30},
	{0x1B, 0x0F},
	{0x1C, 0x82},
	{0x1D, 0x02}
};

#define TAS5707_EQ_LENGTH 280
#define TAS5707_EQ_PARAM_LENGTH 20
static u8 tas5707_EQ_table[TAS5707_EQ_LENGTH] = {
	/*0x29---ch1_bq[0]*/
	0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/*0x2A---ch1_bq[1]*/
	0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/*0x2B---ch1_bq[2]*/
	0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/*0x2C---ch1_bq[3]*/
	0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/*0x2D---ch1_bq[4]*/
	0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/*0x2E---ch1_bq[5]*/
	0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/*0x2F---ch1_bq[6]*/
	0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/*0x30---ch2_bq[0]*/
	0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/*0x31---ch2_bq[1]*/
	0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/*0x32---ch2_bq[2]*/
	0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/*0x33---ch2_bq[3]*/
	0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/*0x34---ch2_bq[4]*/
	0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/*0x35---ch2_bq[5]*/
	0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/*0x36---ch2_bq[6]*/
	0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

#define TAS5707_DRC_LENGTH 36
#define TAS5707_DRC_PARAM_LENGTH 8
#define TAS5707_DRC_TKO_LENGTH 4
static u8 tas5707_drc1_table[TAS5707_DRC_LENGTH] = {
	/* 0x3A drc1_ae */
	0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x3B drc1_aa */
	0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x3C drc1_ad */
	0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x40---drc1_t */
	0xFD, 0xA2, 0x14, 0x90,
	/* 0x41---drc1_k */
	0x03, 0x84, 0x21, 0x09,
	/* 0x42---drc1_o */
	0x00, 0x08, 0x42, 0x10,
};

/* codec private data */
struct tas5707_priv {
	struct regmap *regmap;
	struct snd_soc_codec *codec;
	struct tas57xx_platform_data *pdata;

	/*Platform provided EQ configuration */
	int num_eq_conf_texts;
	const char **eq_conf_texts;
	int eq_cfg;
	struct soc_enum eq_conf_enum;
	unsigned char Ch1_vol;
	unsigned char Ch2_vol;
	unsigned char master_vol;
	unsigned int mclk;
	unsigned int EQ_enum_value;
	unsigned int DRC_enum_value;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
};

static int tas5707_set_EQ_enum(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol);
static int tas5707_get_EQ_enum(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol);
static int tas5707_set_DRC_enum(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol);
static int tas5707_get_DRC_enum(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol);
static int tas5707_get_EQ_param(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol);
static int tas5707_set_EQ_param(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol);
static int tas5707_get_DRC_param(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol);
static int tas5707_set_DRC_param(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol);

static const DECLARE_TLV_DB_SCALE(mvol_tlv, -12700, 50, 1);
static const DECLARE_TLV_DB_SCALE(chvol_tlv, -10300, 50, 1);

static const struct snd_kcontrol_new tas5707_snd_controls[] = {
	SOC_SINGLE_TLV("Master Volume", DDX_MASTER_VOLUME, 0,
			   0xff, 1, mvol_tlv),
	SOC_SINGLE_TLV("Ch1 Volume", DDX_CHANNEL1_VOL, 0,
			   0xff, 1, chvol_tlv),
	SOC_SINGLE_TLV("Ch2 Volume", DDX_CHANNEL2_VOL, 0,
			   0xff, 1, chvol_tlv),
	SOC_SINGLE("Ch1 Switch", DDX_SOFT_MUTE, 0, 1, 1),
	SOC_SINGLE("Ch2 Switch", DDX_SOFT_MUTE, 1, 1, 1),
	SOC_SINGLE("Shutdown Switch", DDX_SYS_CTL_2, 6, 1, 1),
	SOC_SINGLE_RANGE("Fine Master Volume", DDX_CHANNEL3_VOL, 0,
			   0x80, 0x83, 0),
	SOC_SINGLE_BOOL_EXT("Set EQ Enable", 0,
			   tas5707_get_EQ_enum, tas5707_set_EQ_enum),
	SOC_SINGLE_BOOL_EXT("Set DRC Enable", 0,
			   tas5707_get_DRC_enum, tas5707_set_DRC_enum),
	SND_SOC_BYTES_EXT("EQ table", TAS5707_EQ_LENGTH,
			   tas5707_get_EQ_param, tas5707_set_EQ_param),
	SND_SOC_BYTES_EXT("DRC table", TAS5707_DRC_LENGTH,
			   tas5707_get_DRC_param, tas5707_set_DRC_param),
};

static int tas5707_set_dai_sysclk(struct snd_soc_dai *codec_dai,
				  int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct tas5707_priv *tas5707 = snd_soc_codec_get_drvdata(codec);

	tas5707->mclk = freq;
	/* 0x74 = 512fs; 0x6c = 256fs */
	if (freq == 512 * 48000)
		snd_soc_write(codec, DDX_CLOCK_CTL, 0x74);
	else
		snd_soc_write(codec, DDX_CLOCK_CTL, 0x6c);
	return 0;
}

static int tas5707_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	default:
		return 0;//-EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
	case SND_SOC_DAIFMT_RIGHT_J:
	case SND_SOC_DAIFMT_LEFT_J:
		break;
	default:
		return 0;//-EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_NB_IF:
		break;
	default:
		return 0;//-EINVAL;
	}

	return 0;
}

static int tas5707_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params,
			     struct snd_soc_dai *dai)
{
	unsigned int rate;

	rate = params_rate(params);
	pr_debug("rate: %u\n", rate);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S24_BE:
		pr_debug("24bit\n");
	/* fall through */
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

static int tas5707_set_bias_level(struct snd_soc_codec *codec,
				  enum snd_soc_bias_level level)
{
	pr_debug("level = %d\n", level);

	switch (level) {
	case SND_SOC_BIAS_ON:
		break;

	case SND_SOC_BIAS_PREPARE:
		/* Full power on */
		break;

	case SND_SOC_BIAS_STANDBY:
		break;

	case SND_SOC_BIAS_OFF:
		/* The chip runs through the power down sequence for us. */
		break;
	}
	codec->component.dapm.bias_level = level;

	return 0;
}

static const struct snd_soc_dai_ops tas5707_dai_ops = {
	.hw_params = tas5707_hw_params,
	.set_sysclk = tas5707_set_dai_sysclk,
	.set_fmt = tas5707_set_dai_fmt,
};

static struct snd_soc_dai_driver tas5707_dai = {
	.name = DEV_NAME,
	.playback = {
		.stream_name = "HIFI Playback",
		.channels_min = 2,
		.channels_max = 8,
		.rates = TAS5707_RATES,
		.formats = TAS5707_FORMATS,
	},
	.ops = &tas5707_dai_ops,
};

static int tas5707_set_master_vol(struct snd_soc_codec *codec)
{
	struct tas57xx_platform_data *pdata = dev_get_platdata(codec->dev);

	/* using user BSP defined master vol config; */
	if (pdata && pdata->custom_master_vol) {
		pr_debug("tas5707_set_master_vol::%d\n",
			pdata->custom_master_vol);
		snd_soc_write(codec, DDX_MASTER_VOLUME,
			      (0xff - pdata->custom_master_vol));
	} else {
		snd_soc_write(codec, DDX_MASTER_VOLUME, 0x69);
	}

	return 0;
}

static int tas5707_set_DRC_param(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);
	struct tas5707_priv *tas5707 = snd_soc_codec_get_drvdata(codec);
	struct soc_bytes_ext *params = (void *)kcontrol->private_value;
	void *data;
	u8 *val, *p = &tas5707_drc1_table[0];
	unsigned int i, addr;

	data = kmemdup(ucontrol->value.bytes.data,
		params->max, GFP_KERNEL | GFP_DMA);
	if (!data)
		return -ENOMEM;

	val = (u8 *)data;
	memcpy(p, val, params->max / sizeof(u8));

	for (i = 0; i < 3; i++) {
		addr = DDX_DRC1_AE + i;
		regmap_raw_write(tas5707->regmap,
			addr, p, TAS5707_DRC_PARAM_LENGTH);
		p += TAS5707_DRC_PARAM_LENGTH;
	}
	for (i = 0; i < 3; i++) {
		addr = DDX_DRC1_T + i;
		regmap_raw_write(tas5707->regmap,
			addr, p, TAS5707_DRC_TKO_LENGTH);
		p += TAS5707_DRC_TKO_LENGTH;
	}
	kfree(data);
	return 0;
}

static int tas5707_get_DRC_param(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	/*struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	 *struct snd_soc_codec *codec = snd_soc_component_to_codec(component);
	 *struct tas5707_priv *tas5707 = snd_soc_codec_get_drvdata(codec);
	 */
	unsigned int i, addr;
	u8 *val, *p = &tas5707_drc1_table[0];

	val = (u8 *)ucontrol->value.bytes.data;
	for (i = 0; i < 3; i++) {
		addr = DDX_DRC1_AE + i;
		/*regmap_raw_read(tas5707->regmap,
		 *	addr, p, TAS5707_DRC_PARAM_LENGTH);
		 */
		memcpy(val, p, TAS5707_DRC_PARAM_LENGTH);
		p += TAS5707_DRC_PARAM_LENGTH;
		val += TAS5707_DRC_PARAM_LENGTH;
	}
	for (i = 0; i < 3; i++) {
		addr = DDX_DRC1_T + i;
		/*regmap_raw_read(tas5707->regmap,
		 *	addr, p, TAS5707_DRC_TKO_LENGTH);
		 */
		memcpy(val, p, TAS5707_DRC_TKO_LENGTH);
		p += TAS5707_DRC_TKO_LENGTH;
		val += TAS5707_DRC_TKO_LENGTH;
	}
	return 0;
}

static int tas5707_set_EQ_param(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);
	struct tas5707_priv *tas5707 = snd_soc_codec_get_drvdata(codec);
	struct soc_bytes_ext *params = (void *)kcontrol->private_value;
	void *data;
	u8 *val, *p = &tas5707_EQ_table[0];
	unsigned int i = 0, addr;

	data = kmemdup(ucontrol->value.bytes.data,
		params->max, GFP_KERNEL | GFP_DMA);
	if (!data)
		return -ENOMEM;

	val = (u8 *)data;
	memcpy(p, val, params->max / sizeof(u8));

	for (i = 0; i < 14; i++) {
		addr = DDX_CH1_BQ_0 + i;
		regmap_raw_write(tas5707->regmap,
			addr, p, TAS5707_EQ_PARAM_LENGTH);
		p += TAS5707_EQ_PARAM_LENGTH;
	}
	kfree(data);

	return 0;
}

static int tas5707_get_EQ_param(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	/*struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	 *struct snd_soc_codec *codec = snd_soc_component_to_codec(component);
	 *struct tas5707_priv *tas5707 = snd_soc_codec_get_drvdata(codec);
	 */
	unsigned int i, addr;
	u8 *val = (u8 *)ucontrol->value.bytes.data;
	u8 *p = &tas5707_EQ_table[0];

	for (i = 0; i < 14; i++) {
		addr = DDX_CH1_BQ_0 + i;
		/*regmap_raw_read(tas5707->regmap,
		 *  addr, p, TAS5707_EQ_PARAM_LENGTH);
		 */
		memcpy(val, p, TAS5707_EQ_PARAM_LENGTH);
		p += TAS5707_EQ_PARAM_LENGTH;
		val += TAS5707_EQ_PARAM_LENGTH;
	}
	return 0;
}

static int tas5707_set_EQ_enum(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);
	struct tas5707_priv *tas5707 = snd_soc_codec_get_drvdata(codec);
	u8 tas5707_eq_ctl_table[] = { 0x00, 0x00, 0x00, 0x80 };

	tas5707->EQ_enum_value = ucontrol->value.integer.value[0];

	if (tas5707->EQ_enum_value == 1)
		tas5707_eq_ctl_table[3] &= 0x7F;

	regmap_raw_write(tas5707->regmap,
		DDX_BANKSWITCH_AND_EQCTL,
		tas5707_eq_ctl_table, 4);

	return 0;
}

static int tas5707_get_EQ_enum(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);
	struct tas5707_priv *tas5707 = snd_soc_codec_get_drvdata(codec);
	bool enable = (bool)tas5707->EQ_enum_value & 0x1;

	ucontrol->value.integer.value[0] = enable;
	return 0;
}

static int tas5707_set_DRC_enum(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);
	struct tas5707_priv *tas5707 = snd_soc_codec_get_drvdata(codec);
	u8 tas5707_drc_ctl_table[] = { 0x00, 0x00, 0x00, 0x00 };

	tas5707->DRC_enum_value = ucontrol->value.integer.value[0];

	if (tas5707->DRC_enum_value == 1)
		tas5707_drc_ctl_table[3] |= 0x01;

	regmap_raw_write(tas5707->regmap, DDX_DRC_CTL,
		tas5707_drc_ctl_table, 4);

	return 0;
}

static int tas5707_get_DRC_enum(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);
	struct tas5707_priv *tas5707 = snd_soc_codec_get_drvdata(codec);
	bool enable = (bool)tas5707->DRC_enum_value & 0x1;

	ucontrol->value.integer.value[0] = enable;
	return 0;
}

static int reset_tas5707_GPIO(struct snd_soc_codec *codec)
{
	struct tas5707_priv *tas5707 = snd_soc_codec_get_drvdata(codec);
	struct tas57xx_platform_data *pdata = tas5707->pdata;
	int ret = 0;

	if (pdata->reset_pin < 0)
		return 0;

	ret = devm_gpio_request_one(codec->dev, pdata->reset_pin,
					    GPIOF_OUT_INIT_LOW,
					    "tas5707-reset-pin");
	if (ret < 0)
		return -1;

	gpio_direction_output(pdata->reset_pin, GPIOF_OUT_INIT_LOW);
	udelay(1000);
	gpio_direction_output(pdata->reset_pin, GPIOF_OUT_INIT_HIGH);
	mdelay(15);

	return 0;
}

/* tas5707 DRC for channel L/R */
static int tas5707_set_drc1(struct snd_soc_codec *codec)
{
	struct tas5707_priv *tas5707 = snd_soc_codec_get_drvdata(codec);
	unsigned int i, addr;
	u8 *p = &tas5707_drc1_table[0];

	for (i = 0; i < 3; i++) {
		addr = DDX_DRC1_AE + i;
		regmap_raw_write(tas5707->regmap,
			addr, p, TAS5707_DRC_PARAM_LENGTH);
		p += TAS5707_DRC_PARAM_LENGTH;
	}
	for (i = 0; i < 3; i++) {
		addr = DDX_DRC1_T + i;
		regmap_raw_write(tas5707->regmap,
			addr, p, TAS5707_DRC_TKO_LENGTH);
		p += TAS5707_DRC_TKO_LENGTH;
	}

	return 0;
}

static int tas5707_set_drc(struct snd_soc_codec *codec)
{
	struct tas5707_priv *tas5707 = snd_soc_codec_get_drvdata(codec);
	char drc_mask = 0;
	u8 tas5707_drc_ctl_table[] = { 0x00, 0x00, 0x00, 0x00 };

	regmap_raw_write(tas5707->regmap, DDX_DRC_CTL,
		tas5707_drc_ctl_table, 4);
	drc_mask |= 0x01;
	tas5707_drc_ctl_table[3] = drc_mask;
	tas5707_set_drc1(codec);
	regmap_raw_write(tas5707->regmap, DDX_DRC_CTL,
		tas5707_drc_ctl_table, 4);

	return 0;
}

static int tas5707_set_eq_biquad(struct snd_soc_codec *codec)
{
	struct tas5707_priv *tas5707 = snd_soc_codec_get_drvdata(codec);
	int i = 0;
	u8 addr, *p = &tas5707_EQ_table[0];

	for (i = 0; i < 14; i++) {
		addr = DDX_CH1_BQ_0 + i;
		regmap_raw_write(tas5707->regmap,
			addr, p, TAS5707_EQ_PARAM_LENGTH);
		p += TAS5707_EQ_PARAM_LENGTH;
	}
	return 0;
}

static int tas5707_set_eq(struct snd_soc_codec *codec)
{
	struct tas5707_priv *tas5707 = snd_soc_codec_get_drvdata(codec);
	u8 tas5707_eq_ctl_table[] = { 0x00, 0x00, 0x00, 0x80 };

	regmap_raw_write(tas5707->regmap, DDX_BANKSWITCH_AND_EQCTL,
			 tas5707_eq_ctl_table, 4);
	tas5707_set_eq_biquad(codec);
	tas5707_eq_ctl_table[3] &= 0x7F;
	regmap_raw_write(tas5707->regmap, DDX_BANKSWITCH_AND_EQCTL,
			 tas5707_eq_ctl_table, 4);

	return 0;
}

static int tas5707_init(struct snd_soc_codec *codec)
{
	unsigned char burst_data[][4] = {
		{ 0x00, 0x01, 0x77, 0x72 },
		{ 0x00, 0x00, 0x42, 0x03 },
		{ 0x01, 0x02, 0x13, 0x45 },
	};
	struct tas5707_priv *tas5707 = snd_soc_codec_get_drvdata(codec);

	reset_tas5707_GPIO(codec);

	dev_info(codec->dev, "tas5707_init!\n");
	snd_soc_write(codec, DDX_OSC_TRIM, 0x00);
	msleep(50);
	snd_soc_write(codec, DDX_CLOCK_CTL, 0x6c);
	snd_soc_write(codec, DDX_SYS_CTL_1, 0xa0);
	snd_soc_write(codec, DDX_SERIAL_DATA_INTERFACE, 0x05);
	snd_soc_write(codec, DDX_BKND_ERR, 0x02);

	regmap_raw_write(tas5707->regmap, DDX_INPUT_MUX, burst_data[0], 4);
	regmap_raw_write(tas5707->regmap, DDX_CH4_SOURCE_SELECT,
			 burst_data[1], 4);
	regmap_raw_write(tas5707->regmap, DDX_PWM_MUX, burst_data[2], 4);

	/*drc */
	if (tas5707->DRC_enum_value)
		tas5707_set_drc(codec);
	/*eq */
	if (tas5707->EQ_enum_value)
		tas5707_set_eq(codec);

	snd_soc_write(codec, DDX_VOLUME_CONFIG, 0xD1);
	snd_soc_write(codec, DDX_SYS_CTL_2, 0x84);
	snd_soc_write(codec, DDX_START_STOP_PERIOD, 0x95);
	snd_soc_write(codec, DDX_PWM_SHUTDOWN_GROUP, 0x30);
	snd_soc_write(codec, DDX_MODULATION_LIMIT, 0x02);

	/*normal operation */
	if ((tas5707_set_master_vol(codec)) < 0)
		dev_err(codec->dev, "fail to set tas5707 master vol!\n");

	snd_soc_write(codec, DDX_CHANNEL1_VOL, tas5707->Ch1_vol);
	snd_soc_write(codec, DDX_CHANNEL2_VOL, tas5707->Ch2_vol);
	snd_soc_write(codec, DDX_SOFT_MUTE, 0x00);
	snd_soc_write(codec, DDX_CHANNEL3_VOL, 0x80);

	return 0;
}

static int tas5707_probe(struct snd_soc_codec *codec)
{

#ifdef CONFIG_HAS_EARLYSUSPEND
	tas5707->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	tas5707->early_suspend.suspend = tas5707_early_suspend;
	tas5707->early_suspend.resume = tas5707_late_resume;
	tas5707->early_suspend.param = codec;
	register_early_suspend(&(tas5707->early_suspend));
#endif

	tas5707_init(codec);

	return 0;
}

static int tas5707_remove(struct snd_soc_codec *codec)
{
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct tas5707_priv *tas5707 = snd_soc_codec_get_drvdata(codec);

	unregister_early_suspend(&(tas5707->early_suspend));
#endif

	return 0;
}

#ifdef CONFIG_PM
static int tas5707_suspend(struct snd_soc_codec *codec)
{
	struct tas5707_priv *tas5707 = snd_soc_codec_get_drvdata(codec);
	struct tas57xx_platform_data *pdata = dev_get_platdata(codec->dev);

	dev_info(codec->dev, "tas5707_suspend!\n");

	if (pdata && pdata->suspend_func)
		pdata->suspend_func();

	/*save volume */
	tas5707->Ch1_vol = snd_soc_read(codec, DDX_CHANNEL1_VOL);
	tas5707->Ch2_vol = snd_soc_read(codec, DDX_CHANNEL2_VOL);
	tas5707->master_vol = snd_soc_read(codec, DDX_MASTER_VOLUME);
	tas5707_set_bias_level(codec, SND_SOC_BIAS_OFF);

	return 0;
}

static int tas5707_resume(struct snd_soc_codec *codec)
{
	struct tas5707_priv *tas5707 = snd_soc_codec_get_drvdata(codec);
	struct tas57xx_platform_data *pdata = dev_get_platdata(codec->dev);

	dev_info(codec->dev, "tas5707_resume!\n");

	if (pdata && pdata->resume_func)
		pdata->resume_func();

	tas5707_init(codec);
	snd_soc_write(codec, DDX_CHANNEL1_VOL, tas5707->Ch1_vol);
	snd_soc_write(codec, DDX_CHANNEL2_VOL, tas5707->Ch2_vol);
	snd_soc_write(codec, DDX_MASTER_VOLUME, tas5707->master_vol);
	tas5707_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	return 0;
}
#else
#define tas5707_suspend NULL
#define tas5707_resume NULL
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void tas5707_early_suspend(struct early_suspend *h)
{
}

static void tas5707_late_resume(struct early_suspend *h)
{
}
#endif

static const struct snd_soc_dapm_widget tas5707_dapm_widgets[] = {
	SND_SOC_DAPM_DAC("DAC", "HIFI Playback", SND_SOC_NOPM, 0, 0),
};

static const struct snd_soc_codec_driver soc_codec_dev_tas5707 = {
	.probe = tas5707_probe,
	.remove = tas5707_remove,
	.suspend = tas5707_suspend,
	.resume = tas5707_resume,
	.set_bias_level = tas5707_set_bias_level,
	.component_driver = {
		.controls = tas5707_snd_controls,
		.num_controls = ARRAY_SIZE(tas5707_snd_controls),
		.dapm_widgets = tas5707_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(tas5707_dapm_widgets),
	}
};

static const struct regmap_config tas5707_regmap = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = TAS5707_REGISTER_COUNT,
	.reg_defaults = tas5707_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(tas5707_reg_defaults),
	.cache_type = REGCACHE_RBTREE,
};

static int tas5707_parse_dt(
	struct tas5707_priv *tas5707,
	struct device_node *np)
{
	int ret = 0;
	int reset_pin = -1;

	reset_pin = of_get_named_gpio(np, "reset_pin", 0);
	if (reset_pin < 0) {
		pr_err("%s fail to get reset pin from dts!\n", __func__);
		//ret = -1;
	} else {
		pr_info("%s pdata->reset_pin = %d!\n", __func__,
				tas5707->pdata->reset_pin);
	}
	tas5707->pdata->reset_pin = reset_pin;

	/* check eq/drc whether enable */
	ret =
	    of_property_read_u32(np, "eq_enable",
				 &tas5707->EQ_enum_value);

	ret =
	    of_property_read_u32(np, "drc_enable",
				 &tas5707->DRC_enum_value);

	pr_info("tas5707 eq_enable:%d, drc_enable:%d\n",
		tas5707->EQ_enum_value,
		tas5707->DRC_enum_value);

	return ret;
}

static int tas5707_i2c_probe(struct i2c_client *i2c,
			     const struct i2c_device_id *id)
{
	struct tas5707_priv *tas5707;
	struct tas57xx_platform_data *pdata;
	int ret;
	const char *codec_name;

	tas5707 = devm_kzalloc(&i2c->dev, sizeof(struct tas5707_priv),
			       GFP_KERNEL);
	if (!tas5707)
		return -ENOMEM;

	tas5707->regmap = devm_regmap_init_i2c(i2c, &tas5707_regmap);
	if (IS_ERR(tas5707->regmap)) {
		ret = PTR_ERR(tas5707->regmap);
		dev_err(&i2c->dev, "Failed to allocate register map: %d\n",
			ret);
		return ret;
	}

	pdata = devm_kzalloc(&i2c->dev,
				sizeof(struct tas57xx_platform_data),
				GFP_KERNEL);
	if (!pdata) {
		pr_err("%s failed to kzalloc for tas5707 pdata\n", __func__);
		return -ENOMEM;
	}
	tas5707->pdata = pdata;

	tas5707_parse_dt(tas5707, i2c->dev.of_node);

	if (of_property_read_string(i2c->dev.of_node,
			"codec_name",
				&codec_name)) {
		pr_info("no codec name\n");
		//ret = -1;
	}
	pr_info("aux name = %s\n", codec_name);
	if (codec_name)
		dev_set_name(&i2c->dev, "%s", codec_name);

	i2c_set_clientdata(i2c, tas5707);

	ret = snd_soc_register_codec(&i2c->dev, &soc_codec_dev_tas5707,
				     &tas5707_dai, 1);
	if (ret != 0)
		dev_err(&i2c->dev, "Failed to register codec (%d)\n", ret);

	return ret;
}

static int tas5707_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);

	return 0;
}

static const struct i2c_device_id tas5707_i2c_id[] = {
	{ "tas5707", 0 },
	{}
};

static const struct of_device_id tas5707_of_id[] = {
	{.compatible = "ti,tas5707",},
	{ /* senitel */ }
};
MODULE_DEVICE_TABLE(of, tas5707_of_id);

static struct i2c_driver tas5707_i2c_driver = {
	.driver = {
		.name = DEV_NAME,
		.of_match_table = tas5707_of_id,
		.owner = THIS_MODULE,
	},
	.probe = tas5707_i2c_probe,
	.remove = tas5707_i2c_remove,
	.id_table = tas5707_i2c_id,
};
module_i2c_driver(tas5707_i2c_driver);

MODULE_DESCRIPTION("ASoC Tas5707 driver");
MODULE_AUTHOR("AML MM team");
MODULE_LICENSE("GPL");
