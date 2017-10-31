/*
 * ALSA SoC ES7243 adc driver
 *
 * Author:      David Yang, <yangxiaohua@everest-semi.com>
 *		or
 *		<info@everest-semi.com>
 * Copyright:   (C) 2017 Everest Semiconductor Co Ltd.,
 *
 * Based on sound/soc/codecs/wm8731.c by Richard Purdie
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Notes:
 *  ES7243 is a stereo ADC of Everest
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include <sound/initval.h>
#include <linux/regmap.h>
#include <linux/printk.h>

#include "es7243.h"

#define ES7243_TDM_ENABLE  0
#define ES7243_CHANNELS_MAX 6
#define ES7243_I2C_BUS_NUM 0
#define ES7243_CODEC_RW_TEST_EN	0
#define ES7243_IDLE_RESET_EN		1
#define ES7243_PGA_GAIN 0x43  /*27DB*/
/*#define ES7243_PGA_GAIN 0x11  //1DB*/
/*method select: 0: i2c_detect, 1:of_device_id*/
#define ES7243_MATCH_DTS_EN		1

struct i2c_client *i2c_clt[(ES7243_CHANNELS_MAX)/2];

/* codec private data */
struct es7243_priv {
	struct regmap *regmap;
	struct i2c_client *i2c;
	unsigned int dmic_amic;
	unsigned int sysclk;
	struct snd_pcm_hw_constraint_list *sysclk_constraints;
	unsigned int tdm;
	struct delayed_work pcm_pop_work;
};
struct snd_soc_codec *tron_codec[4];
int es7243_init_reg;
static int es7243_codec_num;

static const struct regmap_config es7243_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};
/*
 * ES7243 register cache
 */
static const u8 es7243_reg[] = {
	0x00, 0x00, 0x10, 0x04,	/* 0 */
	0x02, 0x13, 0x00, 0x3f,	/* 4 */
	0x11, 0x00, 0xc0, 0xc0,	/* 8 */
	0x12, 0xa0, 0x40,	/* 12 */
};


static const struct reg_default es7243_reg_defaults[] = {
	{0x00, 0x00}, {0x01, 0x00}, {0x02, 0x10}, {0x03, 0x04},	/* 0 */
	{0x04, 0x02}, {0x05, 0x13}, {0x06, 0x00}, {0x07, 0x3f},	/* 4 */
	{0x08, 0x11}, {0x09, 0x00}, {0x0a, 0xc0}, {0x0b, 0xc0},	/* 8 */
	{0x0c, 0x12}, {0x0d, 0xa0}, {0x0e, 0x40},	/* 12 */

};


#if 0
static bool es7243_writeable(struct device *dev, unsigned int reg)
{
	if (reg <= 14)
		return true;
	else
		return false;
}


static bool es7243_readable(struct device *dev, unsigned int reg)
{
	if (reg <= 14)
		return true;
	else
		return false;
}
static bool es7243_volatile(struct device *dev, unsigned int reg)
{
	if (reg <= 14)
		return true;
	else
		return false;
}
#endif

/* Aute mute thershold */
static const char * const threshold_txt[] = {
	"-96dB",
	"-84dB",
	"-72dB",
	"-60dB"
};
static const struct soc_enum automute_threshold =
SOC_ENUM_SINGLE(ES7243_MUTECTL_REG05, 1, 4, threshold_txt);

/* Analog Input gain */
static const char * const pga_txt[] = {"not allowed", "0dB", "6dB"};
static const struct soc_enum pga_gain =
SOC_ENUM_SINGLE(ES7243_ANACTL1_REG08, 4, 3, pga_txt);

/* Speed Mode Selection */
static const char * const speed_txt[] = {
	"Single Speed Mode",
	"Double Speed Mode",
	"Quard Speed Mode"
};
static const struct soc_enum speed_mode =
SOC_ENUM_SINGLE(ES7243_MODECFG_REG00, 2, 3, speed_txt);

static const struct snd_kcontrol_new es7243_snd_controls[] = {
//	SOC_ENUM("Input PGA", pga_gain),
//	SOC_SINGLE("ADC Mute", ES7243_MUTECTL_REG05, 3, 1, 0),
//	SOC_SINGLE("AutoMute Enable", ES7243_MUTECTL_REG05, 0, 1, 1),
//	SOC_ENUM("AutoMute Threshold", automute_threshold),
//	SOC_SINGLE("TRI State Output", ES7243_STATECTL_REG06, 7, 1, 0),
//	SOC_SINGLE("MCLK Disable", ES7243_STATECTL_REG06, 6, 1, 0),
//	SOC_SINGLE("Reset ADC Digital", ES7243_STATECTL_REG06, 3, 1, 0),
//	SOC_SINGLE("Reset All Digital", ES7243_STATECTL_REG06, 4, 1, 0),
//	SOC_SINGLE("Master Mode", ES7243_MODECFG_REG00, 1, 1, 0),
//	SOC_SINGLE("Software Mode", ES7243_MODECFG_REG00, 0, 1, 0),
//	SOC_ENUM("Speed Mode", speed_mode),
//	SOC_SINGLE("High Pass Filter Disable", ES7243_MODECFG_REG00, 4, 1, 0),
//	SOC_SINGLE("TDM Mode", ES7243_SDPFMT_REG01, 7, 1, 0),
//	SOC_SINGLE("BCLK Invertor", ES7243_SDPFMT_REG01, 6, 1, 0),
//	SOC_SINGLE("LRCK Polarity Set", ES7243_SDPFMT_REG01, 5, 1, 0),
//	SOC_SINGLE("Analog Power down", ES7243_ANACTL2_REG09, 7, 1, 0),
};

static const struct snd_soc_dapm_widget es7243_dapm_widgets[] = {
	SND_SOC_DAPM_INPUT("Line Input 1"),

	SND_SOC_DAPM_PGA("Line Input Switch", ES7243_ANACTL1_REG08,
			0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Left PGA", ES7243_ANACTL0_REG07, 1, 1, NULL, 0),
	SND_SOC_DAPM_PGA("Right PGA", ES7243_ANACTL0_REG07, 0, 1, NULL, 0),

	SND_SOC_DAPM_ADC("Left ADC", "Capture", ES7243_ANACTL0_REG07, 3, 1),
	SND_SOC_DAPM_ADC("Right ADC", "Capture", ES7243_ANACTL0_REG07, 2, 1),

	SND_SOC_DAPM_AIF_OUT("I2S OUT", "I2S1 Stream",  4,
			SND_SOC_NOPM, 3, 1),
};

static const struct snd_soc_dapm_route es7243_intercon[] = {
	/* Input Mux*/
	{"Line Input Switch", NULL, "Line Input 1"},

	/* input pga */
	{"Left PGA", NULL, "Line Input Switch"},
	{"Right PGA", NULL, "Line Input Switch"},
	/* ADC */
	{"Left ADC", NULL, "Left PGA"},
	{"Right ADC", NULL, "Right PGA"},
	/* I2S stream */
	{"I2S OUT", NULL, "Left ADC"},
	{"I2S OUT", NULL, "Right ADC"},
};

static int es7243_read(u8 reg, u8 *rt_value, struct i2c_client *client)
{
	int ret;
	u8 read_cmd[3] = {0};
	u8 cmd_len = 0;

	read_cmd[0] = reg;
	cmd_len = 1;

	if (client->adapter == NULL)
		pr_err("es7243_read client->adapter==NULL\n");

	ret = i2c_master_send(client, read_cmd, cmd_len);
	if (ret != cmd_len) {
		pr_err("es7243_read error1\n");
		return -1;
	}

	ret = i2c_master_recv(client, rt_value, 1);
	if (ret != 1) {
		pr_err("es7243_read error2, ret = %d.\n", ret);
		return -1;
	}

	return 0;
}

static int es7243_write(u8 reg, unsigned char value, struct i2c_client *client)
{
	int ret = 0;
	u8 write_cmd[2] = {0};

	write_cmd[0] = reg;
	write_cmd[1] = value;

	ret = i2c_master_send(client, write_cmd, 2);
	if (ret != 2) {
		pr_err("es7243_write error->[REG-0x%02x,val-0x%02x]\n",
				reg, value);
		return -1;
	}

	return 0;
}

static int es7243_update_bits(u8 reg, u8 mask, u8 value,
		struct i2c_client *client)
{
	u8 val_old, val_new;

	es7243_read(reg, &val_old, client);
	val_new = (val_old & ~mask) | (value & mask);
	if (val_new != val_old)
		es7243_write(reg, val_new, client);

	return 0;
}
#if 0
static int es7243_multi_chips_read(u8 reg, unsigned char *rt_value)
{
	u8 i;

	for (i = 0; i < (ES7243_CHANNELS_MAX)/2; i++)
		es7243_read(reg, rt_value++, i2c_clt[i]);

	return 0;
}
#endif
#if ES7243_CODEC_RW_TEST_EN
static int es7243_multi_chips_write(u8 reg, unsigned char value)
{
	u8 i;

	for (i = 0; i < (ES7243_CHANNELS_MAX)/2; i++)
		es7243_write(reg, value, i2c_clt[i]);

	return 0;
}
#endif
#if 0
static int es7243_multi_chips_update_bits(u8 reg, u8 mask, u8 value)
{
	u8 i;

	for (i = 0; i < (ES7243_CHANNELS_MAX)/2; i++)
		es7243_update_bits(reg, mask, value, i2c_clt[i]);

	return 0;
}
#endif
struct _coeff_div {
	u32 mclk;	/* mclk frequency */
	u32 sr_rate;	/* sample rate */
	u8 speedmode;		/* speed mode,0=single,1=double,2=quad */
	u8 adc_clk_div;         /* adcclk and dacclk divider */
	u8 lrckdiv;	/* adclrck divider and daclrck divider */
	u8 bclkdiv;		/* sclk divider */
	u8 osr;		/* adc osr */
};


/* codec hifi mclk clock divider coefficients */
static const struct _coeff_div coeff_div[] = {
	/* 12.288MHZ */
	{12288000, 8000, 0, 0x0c, 0x60, 24, 32},
	{12288000, 12000, 0, 0x08, 0x40, 16, 32},
	{12288000, 16000, 0, 0x06, 0x30, 12, 32},
	{12288000, 24000, 0, 0x04, 0x20, 8, 32},
	{12288000, 32000, 0, 0x03, 0x18, 6, 32},
	{12288000, 48000, 0, 0x02, 0x10, 4, 32},
	{12288000, 64000, 1, 0x03, 0x0c, 3, 32},
	{12288000, 96000, 1, 0x02, 0x08, 2, 32},
	/* 11.2896MHZ */
	{11289600, 11025, 0, 0x08, 0x40, 16, 32},
	{11289600, 22050, 0, 0x04, 0x20, 8, 32},
	{11289600, 44100, 0, 0x02, 0x10, 4, 32},
	{11289600, 88200, 1, 0x02, 0x08, 2, 32},

	/* 12.000MHZ */
	{12000000, 8000, 0, 0x0c, 0xbc, 30, 31},
	{12000000, 11025, 0, 0x08, 0x44, 17, 34},
	{12000000, 12000, 0, 0x08, 0xaa, 20, 31},
	{12000000, 16000, 0, 0x06, 0x9e, 15, 31},
	{12000000, 22050, 0, 0x04, 0x22, 8, 34},
	{12000000, 24000, 0, 0x04, 0x94, 10, 31},
	{12000000, 32000, 0, 0x03, 0x8a, 5, 31},
	{12000000, 44100, 0, 0x02, 0x11, 4, 34},
	{12000000, 48000, 0, 0x02, 0x85, 5, 31},
	{12000000, 96000, 1, 0x02, 0x85, 1, 31},
};
static inline int get_coeff(int mclk, int rate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(coeff_div); i++) {
		if (coeff_div[i].sr_rate == rate && coeff_div[i].mclk == mclk)
			return i;
	}

	return -EINVAL;
}

/* The set of rates we can generate from the above for each SYSCLK */

static unsigned int rates_12288[] = {
	8000, 12000, 16000, 24000, 24000, 32000, 48000, 96000,
};

static struct snd_pcm_hw_constraint_list constraints_12288 = {
	.count	= ARRAY_SIZE(rates_12288),
	.list	= rates_12288,
};

static unsigned int rates_112896[] = {
	8000, 11025, 22050, 44100,
};

static struct snd_pcm_hw_constraint_list constraints_112896 = {
	.count	= ARRAY_SIZE(rates_112896),
	.list	= rates_112896,
};

static unsigned int rates_12[] = {
	8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000,
	48000, 88235, 96000,
};

static struct snd_pcm_hw_constraint_list constraints_12 = {
	.count	= ARRAY_SIZE(rates_12),
	.list	= rates_12,
};

/*
 * Note that this should be called from init rather than from hw_params.
 */
static int es7243_set_dai_sysclk(struct snd_soc_dai *codec_dai,
		int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct es7243_priv *es7243 = snd_soc_codec_get_drvdata(codec);

	switch (freq) {
	case 11289600:
	case 22579200:
		es7243->sysclk_constraints = &constraints_112896;
		es7243->sysclk = freq;
		return 0;

	case 12288000:
	case 24576000:
	case 4096000:
	case 2048000:
		es7243->sysclk_constraints = &constraints_12288;
		es7243->sysclk = freq;
		return 0;

	case 12000000:
	case 24000000:
		es7243->sysclk_constraints = &constraints_12;
		es7243->sysclk = freq;
		return 0;
	}
	return -EINVAL;
}

static int es7243_set_dai_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{
	u8 iface = 0;
	u8 adciface = 0;
	u8 i;

	for (i = 0; i < (ES7243_CHANNELS_MAX)/2; i++) {
		es7243_read(ES7243_SDPFMT_REG01, &adciface, i2c_clt[i]);
		es7243_read(ES7243_MODECFG_REG00, &iface, i2c_clt[i]);

		/* set master/slave audio interface */
		switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
		case SND_SOC_DAIFMT_CBM_CFM:    /* MASTER MODE */
			iface |= 0x02;
			break;
		case SND_SOC_DAIFMT_CBS_CFS:    /* SLAVE MODE */
			iface &= 0xfd;
			break;
		default:
			return -EINVAL;
		}


		/* interface format */
		switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
		case SND_SOC_DAIFMT_I2S:
			adciface &= 0xFC;
			break;
		case SND_SOC_DAIFMT_RIGHT_J:
			return -EINVAL;
		case SND_SOC_DAIFMT_LEFT_J:
			adciface &= 0xFC;
			adciface |= 0x01;
			break;
		case SND_SOC_DAIFMT_DSP_A:
			adciface &= 0xDC;
			adciface |= 0x03;
			break;
		case SND_SOC_DAIFMT_DSP_B:
			adciface &= 0xDC;
			adciface |= 0x23;
			break;
		default:
			return -EINVAL;
		}

		/* clock inversion */
		adciface &= 0xbF;
		switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
		case SND_SOC_DAIFMT_NB_NF:
			adciface &= 0xbF;
			break;
		case SND_SOC_DAIFMT_IB_IF:
			adciface |= 0x60;
			break;
		case SND_SOC_DAIFMT_IB_NF:
		adciface |= 0x40;
			break;
		case SND_SOC_DAIFMT_NB_IF:
			adciface |= 0x20;
			break;
		default:
			return -EINVAL;
		}

		es7243_update_bits(ES7243_MODECFG_REG00, 0x02,
				iface, i2c_clt[i]);
		es7243_update_bits(ES7243_SDPFMT_REG01, 0x03,
				adciface, i2c_clt[i]);
	}
	return 0;
}
static void es7243_init_codec(struct snd_soc_codec *codec, u8 i)
{
	/*enter into hardware mode*/
	es7243_write(ES7243_MODECFG_REG00, 0x01, i2c_clt[i]);
	/*soft reset codec*/
	es7243_write(ES7243_STATECTL_REG06, 0x18, i2c_clt[i]);
	/*dsp for tdm mode, DSP-A, 32BIT*/
	es7243_write(ES7243_SDPFMT_REG01, 0x93, i2c_clt[i]);
	/*i2s mode, 16BIT*/
	/*es7243_write(ES7243_SDPFMT_REG01, 0x0C, i2c_clt[i]);*/
	es7243_write(ES7243_LRCDIV_REG02, 0x10, i2c_clt[i]);
	es7243_write(ES7243_BCKDIV_REG03, 0x04, i2c_clt[i]);
	es7243_write(ES7243_CLKDIV_REG04, 0x02, i2c_clt[i]);
	es7243_write(ES7243_MUTECTL_REG05, 0x1a, i2c_clt[i]);
	/*enable microphone input and pga gain for 27db*/

	es7243_write(ES7243_ANACTL1_REG08, ES7243_PGA_GAIN, i2c_clt[i]);//
	/*es7243_write(ES7243_ANACTL2_REG09, 0x3F, i2c_clt[i]); */
	/*power up adc and analog input*/
	es7243_write(ES7243_ANACTL0_REG07, 0x10, i2c_clt[i]);
	es7243_write(ES7243_STATECTL_REG06, 0x00, i2c_clt[i]);
	es7243_write(ES7243_MUTECTL_REG05, 0x13, i2c_clt[i]);
	/* power on device */
	/*es7243_set_bias_level(codec, SND_SOC_BIAS_STANDBY);*/
	/*codec->dapm.idle_bias_off = 1;*/
	/*snd_soc_add_codec_controls(codec, es7243_snd_controls,*/
	/*ARRAY_SIZE(es7243_snd_controls));*/
}

static void pcm_pop_work_events(struct work_struct *work)
{
	int i;

	pr_debug("es8316--------pcm_pop_work_events\n");

	for (i = 0; i < (ES7243_CHANNELS_MAX)/2; i++)
		es7243_init_codec(tron_codec[i], i);

	es7243_init_reg = 1;
}

static int es7243_pcm_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct es7243_priv *es7243 = snd_soc_codec_get_drvdata(codec);


	if (es7243_init_reg == 0) {
		pr_debug(">>>>>>>>es8316_pcm_startup es8316_init_reg=0\n");
		schedule_delayed_work(&es7243->pcm_pop_work,
				msecs_to_jiffies(100));
	}

	return 0;
}
static int es7243_pcm_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *dai)
{
	u8 i;
	u8 osrate, mclkdiv, bclkdiv, adciface, speedmode, adcdiv, adclrckdiv;

	for (i = 0; i < (ES7243_CHANNELS_MAX)/2; i++) {

		es7243_read(ES7243_STMOSR_REG0D, &osrate, i2c_clt[i]);
		osrate &= 0xc0;
		es7243_read(ES7243_MODECFG_REG00, &mclkdiv, i2c_clt[i]);
		mclkdiv &= 0x9f;
		es7243_read(ES7243_BCKDIV_REG03, &bclkdiv, i2c_clt[i]);
		bclkdiv &= 0xc0;
		es7243_read(ES7243_SDPFMT_REG01, &adciface, i2c_clt[i]);
		adciface &= 0xE3;
		es7243_read(ES7243_MODECFG_REG00, &speedmode, i2c_clt[i]);
		speedmode &= 0xf3;
		es7243_read(ES7243_CLKDIV_REG04, &adcdiv, i2c_clt[i]);
		adcdiv &= 0xf0;
		es7243_read(ES7243_LRCDIV_REG02, &adclrckdiv, i2c_clt[i]);

		/* bit size */
		switch (params_format(params)) {
		case SNDRV_PCM_FORMAT_S16_LE:
			adciface |= 0x000C;
			break;
		case SNDRV_PCM_FORMAT_S20_3LE:
			adciface |= 0x0004;
			break;
		case SNDRV_PCM_FORMAT_S24_LE:
			break;
		case SNDRV_PCM_FORMAT_S32_LE:
			adciface |= 0x0010;
			break;
		}

		/* set iface & srate*/
		es7243_update_bits(ES7243_SDPFMT_REG01, 0x1c,
				adciface, i2c_clt[i]);
	}
	return 0;
}

static int es7243_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	u8 i;

	for (i = 0; i < (ES7243_CHANNELS_MAX)/2; i++) {
		dev_dbg(codec->dev, "%s %d\n", __func__, mute);

		if (mute)
			es7243_update_bits(ES7243_MUTECTL_REG05, 0x08, 0x08,
					i2c_clt[i]);
		else
			es7243_update_bits(ES7243_MUTECTL_REG05, 0x08, 0x00,
					i2c_clt[i]);
	}
	return 0;
}

static int es7243_set_bias_level(struct snd_soc_codec *codec,
		enum snd_soc_bias_level level)
{
	u8 i;

	for (i = 0; i < (ES7243_CHANNELS_MAX)/2; i++) {
		switch (level) {
		case SND_SOC_BIAS_ON:
			es7243_write(ES7243_ANACTL0_REG07, 0x80, i2c_clt[i]);
			es7243_write(ES7243_STATECTL_REG06, 0x00, i2c_clt[i]);
			es7243_write(ES7243_MUTECTL_REG05, 0x13, i2c_clt[i]);
			dev_dbg(codec->dev, "%s on\n", __func__);
			break;
		case SND_SOC_BIAS_PREPARE:
			dev_dbg(codec->dev, "%s prepare\n", __func__);
			es7243_update_bits(ES7243_MUTECTL_REG05,
					0x08, 0x00, i2c_clt[i]);
			es7243_update_bits(ES7243_STATECTL_REG06,
					0x40, 0x00, i2c_clt[i]);
			msleep(50);
			break;
		case SND_SOC_BIAS_STANDBY:
			dev_dbg(codec->dev, "%s standby\n", __func__);
			es7243_update_bits(ES7243_MUTECTL_REG05,
					0x08, 0x08, i2c_clt[i]);
			es7243_update_bits(ES7243_STATECTL_REG06,
					0x40, 0x00, i2c_clt[i]);
			msleep(50);
			es7243_update_bits(ES7243_ANACTL0_REG07,
					0x0f, 0x00, i2c_clt[i]);
			break;
		case SND_SOC_BIAS_OFF:
			dev_dbg(codec->dev, "%s off\n", __func__);
			es7243_update_bits(ES7243_MUTECTL_REG05,
					0x08, 0x08, i2c_clt[i]);
			msleep(50);
			es7243_update_bits(ES7243_ANACTL0_REG07,
					0x0f, 0x0f, i2c_clt[i]);
			msleep(50);
			es7243_update_bits(ES7243_STATECTL_REG06,
					0x40, 0x40, i2c_clt[i]);
			break;
		}
	}
	return 0;
}

#define es7243_RATES SNDRV_PCM_RATE_8000_96000

#define es7243_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | \
		SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_ops es7243_ops = {
	.startup = es7243_pcm_startup,
	.hw_params = es7243_pcm_hw_params,
	.set_fmt = es7243_set_dai_fmt,
	.set_sysclk = es7243_set_dai_sysclk,
	.digital_mute = es7243_mute,
};

static struct snd_soc_dai_driver es7243_dai0 = {
	.name = "ES7243 HiFi 0",
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = es7243_RATES,
		.formats = es7243_FORMATS,
	},
	.ops = &es7243_ops,
	.symmetric_rates = 1,
};
static struct snd_soc_dai_driver es7243_dai1 = {
	.name = "ES7243 HiFi 1",
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = es7243_RATES,
		.formats = es7243_FORMATS,
	},
	.ops = &es7243_ops,
	.symmetric_rates = 1,
};
static struct snd_soc_dai_driver es7243_dai2 = {
	.name = "ES7243 HiFi 2",
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = es7243_RATES,
		.formats = es7243_FORMATS,
	},
	.ops = &es7243_ops,
	.symmetric_rates = 1,
};

#if ES7243_CHANNELS_MAX > 6
static struct snd_soc_dai_driver es7243_dai3 = {
	.name = "ES7243 HiFi 3",
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = es7243_RATES,
		.formats = es7243_FORMATS,
	},
	.ops = &es7243_ops,
	.symmetric_rates = 1,
};
#endif
static struct snd_soc_dai_driver *es7243_dai[] = {
#if ES7243_CHANNELS_MAX > 0
	&es7243_dai0,
#endif

#if ES7243_CHANNELS_MAX > 2
	&es7243_dai1,
#endif

#if ES7243_CHANNELS_MAX > 4
	&es7243_dai2,
#endif

#if ES7243_CHANNELS_MAX > 6
	&es7243_dai3,
#endif
};

static int es7243_suspend(struct snd_soc_codec *codec)
{
	es7243_set_bias_level(codec, SND_SOC_BIAS_OFF);

	return 0;
}

static int es7243_resume(struct snd_soc_codec *codec)
{
	snd_soc_cache_sync(codec);
	es7243_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	return 0;
}

static int es7243_probe(struct snd_soc_codec *codec)
{
	struct es7243_priv *es7243 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

#if !ES7243_CODEC_RW_TEST_EN
	/*ret = snd_soc_codec_set_cache_io(codec, 8, 8, SND_SOC_I2C);*/
#else
	codec->control_data = devm_regmap_init_i2c(es7243->i2c,
			&es7243_regmap_config);
	ret = PTR_RET(codec->control_data);
#endif
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}
	/*ret = snd_soc_codec_set_cache_io(codec, 8, 8, SND_SOC_I2C);*/
	/*if (ret < 0) {*/
	/*dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);*/
	/*return ret;*/
	/*}*/
	pr_info("begin->>>>>>>>>>%s!\n", __func__);

	tron_codec[es7243_codec_num++] = codec;


	INIT_DELAYED_WORK(&es7243->pcm_pop_work, pcm_pop_work_events);

	return 0;
}

static int es7243_remove(struct snd_soc_codec *codec)
{
	es7243_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

#if ES7243_CODEC_RW_TEST_EN
static unsigned int es7243_codec_read(struct snd_soc_codec *codec,
		unsigned int reg)
{

	u8 val_r;
	struct es7243_priv *es7243 = dev_get_drvdata(codec->dev);

	es7243_read(reg, &val_r, es7243->i2c);
	return val_r;
}

static int es7243_codec_write(struct snd_soc_codec *codec,
		unsigned int reg, unsigned int value)
{

	es7243_multi_chips_write(reg, value);
	return 0;
}
#endif
static const struct snd_soc_codec_driver soc_codec_dev_es7243 = {
	.probe = es7243_probe,
	.remove = es7243_remove,
	.suspend = es7243_suspend,
	.resume = es7243_resume,
	.set_bias_level = es7243_set_bias_level,
	.idle_bias_off = true,
	.reg_word_size = sizeof(u8),
	.reg_cache_default = es7243_reg_defaults,
	.reg_cache_size = ARRAY_SIZE(es7243_reg_defaults),
	.component_driver = {
		.controls = es7243_snd_controls,
		.num_controls = ARRAY_SIZE(es7243_snd_controls),
		.dapm_widgets = es7243_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(es7243_dapm_widgets),
		.dapm_routes = es7243_intercon,
		.num_dapm_routes = ARRAY_SIZE(es7243_intercon),
	}
#if ES7243_CODEC_RW_TEST_EN
	.read = es7243_codec_read,
	.write = es7243_codec_write,
#endif
};
#if 0
static const struct regmap_config es7243_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.use_single_rw = true,
	.max_register = 0x0E,
	.volatile_reg = es7243_volatile,
	.readable_reg = es7243_readable,
	.cache_type = REGCACHE_RBTREE,
	.reg_defaults = es7243_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(es7243_reg_defaults),
};
#endif
static ssize_t es7243_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	long int val = 0;
	int flag = 0, ret = 0;
	u8 i = 0, reg, num, value_w, value_r;

	struct es7243_priv *es7243 = dev_get_drvdata(dev);

	ret = kstrtol(buf, 16, &val);
	if (ret != 0)
		return ret;

	flag = (val >> 16) & 0xFF;

	if (flag) {
		reg = (val >> 8) & 0xFF;
		value_w = val & 0xFF;
		pr_debug("\nWrite: start REG:0x%02x,val:0x%02x,count:0x%02x\n",
				reg, value_w, flag);
		while (flag--) {
			es7243_write(reg, value_w, es7243->i2c);
			pr_debug("Write 0x%02x to REG:0x%02x\n", value_w, reg);
			reg++;
		}
	} else {
		reg = (val >> 8) & 0xFF;
		num = val & 0xff;
		pr_debug("\nRead: start REG:0x%02x,count:0x%02x\n", reg, num);

		do {
			value_r = 0;
			es7243_read(reg, &value_r, es7243->i2c);
			pr_debug("REG[0x%02x]: 0x%02x;  ", reg, value_r);
			reg++;
			i++;
			if ((i == num) || (i%4 == 0))
				pr_debug("\n");
		} while (i < num);
	}

	return count;
}

static ssize_t es7243_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	pr_debug("echo flag|reg|val > es7243\n");
	pr_debug("eg read star address=0x06,count 0x10:echo 0610 >es7243\n");
	return 0;
}

static DEVICE_ATTR(es7243, 0644, es7243_show, es7243_store);

static struct attribute *es7243_debug_attrs[] = {
	&dev_attr_es7243.attr,
	NULL,
};

static struct attribute_group es7243_debug_attr_group = {
	.name   = "es7243_debug",
	.attrs  = es7243_debug_attrs,
};

/*
 * If the i2c layer weren't so broken, we could pass this kind of data
 * around
 */
static int es7243_i2c_probe(struct i2c_client *i2c,
		const struct i2c_device_id *i2c_id)
{
	struct es7243_priv *es7243;
	int ret;

	pr_debug("begin->>>>>>>>>>%s!\n", __func__);

	es7243 = devm_kzalloc(&i2c->dev,
			sizeof(struct es7243_priv), GFP_KERNEL);
	if (es7243 == NULL)
		return -ENOMEM;
	es7243->i2c = i2c;
	es7243->tdm =  ES7243_TDM_ENABLE;  //to initialize tdm mode
	dev_set_drvdata(&i2c->dev, es7243);

	if (i2c_id->driver_data < (ES7243_CHANNELS_MAX)/2) {
		i2c_clt[i2c_id->driver_data] = i2c;
		ret =  snd_soc_register_codec(&i2c->dev,
				&soc_codec_dev_es7243,
				es7243_dai[i2c_id->driver_data], 1);
		if (ret < 0) {
			kfree(es7243);
			return ret;
		}
	}
	ret = sysfs_create_group(&i2c->dev.kobj, &es7243_debug_attr_group);
	if (ret)
		pr_err("failed to create attr group\n");
	return ret;
}
static int __exit es7243_i2c_remove(struct i2c_client *i2c)
{
	snd_soc_unregister_codec(&i2c->dev);
	kfree(i2c_get_clientdata(i2c));
	return 0;
}

#if !ES7243_MATCH_DTS_EN
static int es7243_i2c_detect(struct i2c_client *client,
		struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = client->adapter;

	if (adapter->nr == ES7243_I2C_BUS_NUM) {
		if (client->addr == 0x10) {
			strlcpy(info->type, "MicArray_0", I2C_NAME_SIZE);
			return 0;
		} else if (client->addr == 0x11) {
			strlcpy(info->type, "MicArray_1", I2C_NAME_SIZE);
			return 0;
		} else if (client->addr == 0x12) {
			strlcpy(info->type, "MicArray_2", I2C_NAME_SIZE);
			return 0;
		} else if (client->addr == 0x13) {
			strlcpy(info->type, "MicArray_3", I2C_NAME_SIZE);
			return 0;
		}
	}

	return -ENODEV;
}
#endif
#if !ES7243_MATCH_DTS_EN
static const unsigned short es7243_i2c_addr[] = {
#if ES7243_CHANNELS_MAX > 0
	0x10,
#endif

#if ES7243_CHANNELS_MAX > 2
	0x11,
#endif

#if ES7243_CHANNELS_MAX > 4
	0x12,
#endif

#if ES7243_CHANNELS_MAX > 6
	0x13,
#endif

	I2C_CLIENT_END,
};
#endif
#if 0
static struct i2c_board_info es7243_i2c_board_info[] = {
#if ES7243_CHANNELS_MAX > 0
	{I2C_BOARD_INFO("MicArray_0", 0x10),},//es7243_0
#endif

#if ES7243_CHANNELS_MAX > 2
	{I2C_BOARD_INFO("MicArray_1", 0x11),},//es7243_1
#endif

#if ES7243_CHANNELS_MAX > 4
	{I2C_BOARD_INFO("MicArray_2", 0x12),},//es7243_2
#endif

#if ES7243_CHANNELS_MAX > 6
	{I2C_BOARD_INFO("MicArray_3", 0x13),},//es7243_3
#endif
};
#endif

static const struct i2c_device_id es7243_i2c_id[] = {
#if ES7243_CHANNELS_MAX > 0
	{ "MicArray_0", 0 },
#endif

#if ES7243_CHANNELS_MAX > 2
	{ "MicArray_1", 1 },
#endif

#if ES7243_CHANNELS_MAX > 4
	{ "MicArray_2", 2 },
#endif

#if ES7243_CHANNELS_MAX > 6
	{ "MicArray_3", 3 },
#endif
	{ }
};
MODULE_DEVICE_TABLE(i2c, es7243_i2c_id);

static const struct of_device_id es7243_dt_ids[] = {
#if ES7243_CHANNELS_MAX > 0
	{ .compatible = "MicArray_0", },
#endif

#if ES7243_CHANNELS_MAX > 2
	{ .compatible = "MicArray_1", },
#endif

#if ES7243_CHANNELS_MAX > 4
	{ .compatible = "MicArray_2", },
#endif

#if ES7243_CHANNELS_MAX > 6
	{ .compatible = "MicArray_3", },
#endif
};
MODULE_DEVICE_TABLE(of, es7243_dt_ids);


static struct i2c_driver es7243_i2c_driver = {
	.driver = {
		.name = "es7243-audio-adc",
		.owner = THIS_MODULE,
#if ES7243_MATCH_DTS_EN
		.of_match_table = es7243_dt_ids,
#endif
	},
	.probe = es7243_i2c_probe,
	.remove = __exit_p(es7243_i2c_remove),
	.class  = I2C_CLASS_HWMON,
	.id_table = es7243_i2c_id,
#if !ES7243_MATCH_DTS_EN
	.address_list = es7243_i2c_addr,
	.detect = es7243_i2c_detect,
#endif
};

static int __init es7243_modinit(void)
{
	int ret;

	ret = i2c_add_driver(&es7243_i2c_driver);
	if (ret != 0)
		pr_err("Failed to register es7243 i2c driver : %d\n", ret);
	return ret;
	return ret;
}
module_init(es7243_modinit);
static void __exit es7243_exit(void)
{
	i2c_del_driver(&es7243_i2c_driver);
}
module_exit(es7243_exit);

MODULE_DESCRIPTION("ASoC ES7243 audio adc driver");
MODULE_AUTHOR("David Yang <yangxiaohua@everest-semi.com>");
MODULE_LICENSE("GPL v2");

