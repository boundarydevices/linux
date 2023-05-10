/*
 * Copyright (C) 2015-2016 Freescale Semiconductor, Inc.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <sound/control.h>
#include <sound/pcm_params.h>
#include <sound/soc-dapm.h>
#include <linux/pinctrl/consumer.h>
#include <linux/mfd/syscon.h>
#include "../codecs/wm8960.h"
#include "fsl_sai.h"
#include "imx-audmux.h"

struct imx_wm8960_data {
	enum of_gpio_flags hp_active_low;
	enum of_gpio_flags mic_active_low;
	bool is_headset_jack;
	struct platform_device *pdev;
	struct platform_device *asrc_pdev;
	struct snd_soc_card card;
	struct clk *codec_clk;
	unsigned int clk_frequency;
	bool is_codec_master;
	bool is_codec_rpmsg;
	bool is_playback_only;
	bool is_capture_only;
	bool is_stream_in_use[2];
	bool is_stream_opened[2];
	struct regmap *gpr;
	unsigned int hp_det[2];
	u32 asrc_rate;
	u32 asrc_format;
	struct snd_soc_jack imx_hp_jack;
	struct snd_soc_jack_pin imx_hp_jack_pin;
	struct snd_soc_jack_gpio imx_hp_jack_gpio;
	struct snd_soc_jack imx_mic_jack;
	struct snd_soc_jack_pin imx_mic_jack_pin;
	struct snd_soc_jack_gpio imx_mic_jack_gpio;
	struct snd_soc_dai_link imx_wm8960_dai[3];
	struct gpio_desc *amp_mute;
	struct gpio_desc *amp_standby;
	unsigned amp_standby_enter_wait_ms;
	unsigned amp_standby_exit_delay_ms;
#define AMP_GAIN_CNT 2
	struct gpio_desc *amp_gain[AMP_GAIN_CNT];
	unsigned char amp_gain_seq[1 << AMP_GAIN_CNT];
	char amp_gain_value;
	char amp_mute_value;
	char amp_disabled;
	char hp_det_status;
	char amp_standby_state;
	char amp_mute_on_hp_detect;
	char amp_standby_set_pending;
	char amp_mute_clear_pending;
};

static int hp_jack_status_check(void *data)
{
	struct imx_wm8960_data *imx_data = (struct imx_wm8960_data *)data;
	struct snd_soc_jack *jack = &imx_data->imx_hp_jack;
	struct snd_soc_dapm_context *dapm = &jack->card->dapm;
	int hp_status, ret;

	hp_status = gpio_get_value_cansleep(imx_data->imx_hp_jack_gpio.gpio);

	if (hp_status != imx_data->hp_active_low) {
		snd_soc_dapm_disable_pin(dapm, "Ext Spk");
		if (imx_data->is_headset_jack) {
			snd_soc_dapm_disable_pin(dapm, "Main MIC");
			snd_soc_dapm_enable_pin(dapm, "Mic Jack");
		}
		ret = imx_data->imx_hp_jack_gpio.report;
	} else {
		snd_soc_dapm_enable_pin(dapm, "Ext Spk");
		if (imx_data->is_headset_jack) {
			snd_soc_dapm_disable_pin(dapm, "Mic Jack");
			snd_soc_dapm_enable_pin(dapm, "Main MIC");
		}
		ret = 0;
	}

	return ret;
}

static int mic_jack_status_check(void *data)
{
	struct imx_wm8960_data *imx_data = (struct imx_wm8960_data *)data;
	struct snd_soc_jack *jack = &imx_data->imx_mic_jack;
	struct snd_soc_dapm_context *dapm = &jack->card->dapm;
	int mic_status, ret;

	mic_status = gpio_get_value_cansleep(imx_data->imx_mic_jack_gpio.gpio);

	if (mic_status != imx_data->mic_active_low) {
		snd_soc_dapm_disable_pin(dapm, "Main MIC");
		snd_soc_dapm_enable_pin(dapm, "Mic Jack");
		ret = imx_data->imx_mic_jack_gpio.report;
	} else {
		snd_soc_dapm_disable_pin(dapm, "Mic Jack");
		snd_soc_dapm_enable_pin(dapm, "Main MIC");
		ret = 0;
	}

	return ret;
}

static int do_mute(struct gpio_desc *gd, int mute)
{
	if (!gd)
		return 0;

	gpiod_set_value(gd, mute);
	return 0;
}

static int amp_mute(struct imx_wm8960_data *data, int mute)
{
	data->amp_disabled = data->amp_mute_value = mute;
	if (!mute && data->amp_mute_on_hp_detect && data->hp_det_status)
		mute = 1;
	pr_debug("mute=%x\n", mute);
	return do_mute(data->amp_mute, mute);
}

static int amp_stdby(struct imx_wm8960_data *data, int stdby)
{
	if (!data->amp_standby)
		return 0;
	if (stdby == data->amp_standby_state)
		return 0;

	if (stdby)
		msleep(data->amp_standby_enter_wait_ms);
	data->amp_standby_state = stdby;
	pr_debug("stdby=%x\n", stdby);
	gpiod_set_value(data->amp_standby, stdby);
	if (!stdby)
		msleep(data->amp_standby_exit_delay_ms);
	return 0;
}

static int event_amp(struct snd_soc_dapm_widget *w,
		    struct snd_kcontrol *k, int event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;
	struct imx_wm8960_data *data = container_of(card,
			struct imx_wm8960_data, card);

	if (event & SND_SOC_DAPM_POST_PMU) {
		amp_stdby(data, 0);
		data->amp_mute_clear_pending = 1;
		return 0;
	}
	if (event & SND_SOC_DAPM_PRE_PMD) {
		amp_mute(data, 1);
		data->amp_standby_set_pending = 1;
		return 0;
	}
	return 0;
}

static int event_amp_last(struct snd_soc_dapm_widget *w,
		    struct snd_kcontrol *k, int event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;
	struct imx_wm8960_data *data = container_of(card,
			struct imx_wm8960_data, card);

	if ((event & SND_SOC_DAPM_POST_PMD) && data->amp_standby_set_pending) {
		data->amp_standby_set_pending = 0;
		return amp_stdby(data, 1);
	}
	if ((event & SND_SOC_DAPM_POST_PMU) && data->amp_mute_clear_pending) {
		data->amp_mute_clear_pending = 0;
		return amp_mute(data, 0);
	}
	return 0;
}
#define SND_SOC_DAPM_POST2(wname, wevent) \
{	.id = snd_soc_dapm_post, .name = wname, .kcontrol_news = NULL, \
	.num_kcontrols = 0, .reg = SND_SOC_NOPM, .event = wevent, \
	.event_flags = SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD, \
	.subseq = 1}


static const struct snd_soc_dapm_widget imx_wm8960_dapm_widgets[] = {
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
	SND_SOC_DAPM_MIC("Mic Jack", NULL),
	SND_SOC_DAPM_MIC("Main MIC", NULL),
	SND_SOC_DAPM_SPK("Ext Spk", event_amp),
	SND_SOC_DAPM_POST2("amp_post", event_amp_last),
};

static int imx_wm8960_jack_init(struct snd_soc_card *card,
		struct snd_soc_jack *jack, struct snd_soc_jack_pin *pin,
		struct snd_soc_jack_gpio *gpio)
{
	int ret;

	ret = snd_soc_card_jack_new_pins(card, pin->pin, pin->mask, jack,
					 pin, 1);
	if (ret) {
		return ret;
	}

	ret = snd_soc_jack_add_gpios(jack, 1, gpio);
	if (ret)
		return ret;

	return 0;
}

static ssize_t headphone_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct snd_soc_card *card = dev_get_drvdata(dev);
	struct imx_wm8960_data *data = snd_soc_card_get_drvdata(card);
	int hp_status;

	/* Check if headphone is plugged in */
	hp_status = gpio_get_value_cansleep(data->imx_hp_jack_gpio.gpio);

	if (hp_status != data->hp_active_low)
		strcpy(buf, "Headphone\n");
	else
		strcpy(buf, "Speaker\n");

	return strlen(buf);
}

static ssize_t micphone_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct snd_soc_card *card = dev_get_drvdata(dev);
	struct imx_wm8960_data *data = snd_soc_card_get_drvdata(card);
	int mic_status;

	/* Check if headphone is plugged in */
	mic_status = gpio_get_value_cansleep(data->imx_mic_jack_gpio.gpio);

	if (mic_status != data->mic_active_low)
		strcpy(buf, "Mic Jack\n");
	else
		strcpy(buf, "Main MIC\n");

	return strlen(buf);
}
static DEVICE_ATTR_RO(headphone);
static DEVICE_ATTR_RO(micphone);

static int imx_hifi_hw_params(struct snd_pcm_substream *substream,
				     struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = asoc_rtd_to_codec(rtd, 0);
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	struct snd_soc_card *card = rtd->card;
	struct imx_wm8960_data *data = snd_soc_card_get_drvdata(card);
	bool tx = substream->stream == SNDRV_PCM_STREAM_PLAYBACK;
	struct device *dev = card->dev;
	unsigned int sample_rate = params_rate(params);
	unsigned int pll_out;
	unsigned int fmt;
	int ret = 0;

	data->is_stream_in_use[tx] = true;

	if (data->is_stream_in_use[!tx])
		return 0;

	if (data->is_codec_master)
		fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBM_CFM;
	else
		fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBS_CFS;

	/* set cpu DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, fmt);
	if (ret) {
		dev_err(dev, "failed to set cpu dai fmt: %d\n", ret);
		return ret;
	}
	/* set codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, fmt);
	if (ret) {
		dev_err(dev, "failed to set codec dai fmt: %d\n", ret);
		return ret;
	}

	if (!data->is_codec_master) {
		ret = snd_soc_dai_set_tdm_slot(cpu_dai, 0, 0, 2, params_width(params));
		if (ret) {
			dev_err(dev, "failed to set cpu dai tdm slot: %d\n", ret);
			return ret;
		}

		ret = snd_soc_dai_set_sysclk(cpu_dai, 0, 0, SND_SOC_CLOCK_OUT);
		if (ret == -ENOTSUPP)
			ret = 0;
		if (ret) {
			dev_err(dev, "failed to set cpu sysclk: %d\n", ret);
			return ret;
		}
		return 0;
	} else {
		ret = snd_soc_dai_set_sysclk(cpu_dai, 0, 0, SND_SOC_CLOCK_IN);
		if (ret == -ENOTSUPP)
			ret = 0;
		if (ret) {
			dev_err(dev, "failed to set cpu sysclk: %d\n", ret);
			return ret;
		}
	}

	data->clk_frequency = clk_get_rate(data->codec_clk);

	/* Set codec pll */
	if (params_width(params) == 24)
		pll_out = sample_rate * 768;
	else
		pll_out = sample_rate * 512;

	ret = snd_soc_dai_set_pll(codec_dai, WM8960_SYSCLK_AUTO, 0, data->clk_frequency, pll_out);
	if (ret)
		return ret;
	ret = snd_soc_dai_set_sysclk(codec_dai, WM8960_SYSCLK_AUTO, pll_out, 0);

	return ret;
}

static int imx_hifi_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = asoc_rtd_to_codec(rtd, 0);
	struct snd_soc_card *card = rtd->card;
	struct imx_wm8960_data *data = snd_soc_card_get_drvdata(card);
	bool tx = substream->stream == SNDRV_PCM_STREAM_PLAYBACK;
	struct device *dev = card->dev;
	int ret;

	data->is_stream_in_use[tx] = false;

	if (data->is_codec_master && !data->is_stream_in_use[!tx]) {
		ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_CBS_CFS | SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF);
		if (ret)
			dev_warn(dev, "failed to set codec dai fmt: %d\n", ret);
	}

	return 0;
}

static u32 imx_wm8960_rates[] = { 8000, 16000, 32000, 48000 };
static struct snd_pcm_hw_constraint_list imx_wm8960_rate_constraints = {
	.count = ARRAY_SIZE(imx_wm8960_rates),
	.list = imx_wm8960_rates,
};

static int imx_hifi_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
//	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	struct snd_soc_card *card = rtd->card;
	struct imx_wm8960_data *data = snd_soc_card_get_drvdata(card);
	bool tx = substream->stream == SNDRV_PCM_STREAM_PLAYBACK;
//	struct fsl_sai *sai = dev_get_drvdata(cpu_dai->dev);
	int ret = 0;

	data->is_stream_opened[tx] = true;
#if 0
	if (data->is_stream_opened[tx] != sai->is_stream_opened[tx] ||
	    data->is_stream_opened[!tx] != sai->is_stream_opened[!tx]) {
		data->is_stream_opened[tx] = false;
		return -EBUSY;
	}
#endif
	if (!data->is_codec_master) {
		ret = snd_pcm_hw_constraint_list(substream->runtime, 0,
				SNDRV_PCM_HW_PARAM_RATE, &imx_wm8960_rate_constraints);
		if (ret)
			return ret;
	}

	return ret;
}

static void imx_hifi_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct imx_wm8960_data *data = snd_soc_card_get_drvdata(card);
	bool tx = substream->stream == SNDRV_PCM_STREAM_PLAYBACK;

	data->is_stream_opened[tx] = false;
}

static struct snd_soc_ops imx_hifi_ops = {
	.hw_params = imx_hifi_hw_params,
	.hw_free = imx_hifi_hw_free,
	.startup   = imx_hifi_startup,
	.shutdown  = imx_hifi_shutdown,
};

static void imx_wm8960_add_hp_jack(struct imx_wm8960_data *data)
{
	struct device *dev = &data->pdev->dev;
	int ret;

	if (data->is_headset_jack) {
		data->imx_hp_jack_pin.mask |= SND_JACK_MICROPHONE;
		data->imx_hp_jack_gpio.report |= SND_JACK_MICROPHONE;
	}

	data->imx_hp_jack_gpio.jack_status_check = hp_jack_status_check;
	data->imx_hp_jack_gpio.data = data;
	ret = imx_wm8960_jack_init(&data->card, &data->imx_hp_jack,
				   &data->imx_hp_jack_pin, &data->imx_hp_jack_gpio);
	if (ret) {
		dev_warn(dev, "hp jack init failed (%d)\n", ret);
		return;
	}

	ret = device_create_file(dev, &dev_attr_headphone);
	if (ret)
		dev_warn(dev, "create hp attr failed (%d)\n", ret);
}


static void imx_wm8960_add_mic_jack(struct imx_wm8960_data *data)
{
	struct device *dev = &data->pdev->dev;
	int ret;

	if (!data->is_headset_jack) {
		data->imx_mic_jack_gpio.jack_status_check = mic_jack_status_check;
		data->imx_mic_jack_gpio.data = data;
		ret = imx_wm8960_jack_init(&data->card, &data->imx_mic_jack,
					   &data->imx_mic_jack_pin, &data->imx_mic_jack_gpio);
		if (ret) {
			dev_warn(dev, "mic jack init failed (%d)\n", ret);
			return;
		}
	}

	ret = device_create_file(dev, &dev_attr_micphone);
	if (ret)
		dev_warn(dev, "create mic attr failed (%d)\n", ret);
}

static int imx_wm8960_late_probe(struct snd_soc_card *card)
{
	struct snd_soc_pcm_runtime *rtd = list_first_entry(
		&card->rtd_list, struct snd_soc_pcm_runtime, list);
	struct snd_soc_dai *codec_dai = asoc_rtd_to_codec(rtd, 0);
	struct imx_wm8960_data *data = snd_soc_card_get_drvdata(card);

	/*
	 * codec ADCLRC pin configured as GPIO, DACLRC pin is used as a frame
	 * clock for ADCs and DACs
	 */
	snd_soc_component_update_bits(codec_dai->component, WM8960_IFACE2, 1<<6, 1<<6);

	/* GPIO1 used as headphone detect output */
	snd_soc_component_update_bits(codec_dai->component, WM8960_ADDCTL4, 7<<4, 3<<4);

	if (gpio_is_valid(data->imx_hp_jack_gpio.gpio))
		imx_wm8960_add_hp_jack(data);

	if (gpio_is_valid(data->imx_mic_jack_gpio.gpio))
		imx_wm8960_add_mic_jack(data);
	if (data->hp_det[0] > 3)
		return 0;

	/* Enable headphone jack detect */
	snd_soc_component_update_bits(codec_dai->component, WM8960_ADDCTL2, 1<<6, 1<<6);
	snd_soc_component_update_bits(codec_dai->component, WM8960_ADDCTL2, 1<<5, data->hp_det[1]<<5);
	snd_soc_component_update_bits(codec_dai->component, WM8960_ADDCTL4, 3<<2, data->hp_det[0]<<2);
	snd_soc_component_update_bits(codec_dai->component, WM8960_ADDCTL1, 3, 3);

	return 0;
}

static int be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
			struct snd_pcm_hw_params *params)
{
	struct snd_soc_card *card = rtd->card;
	struct imx_wm8960_data *data = snd_soc_card_get_drvdata(card);
	struct snd_interval *rate;
	struct snd_mask *mask;

	if (!data->asrc_pdev)
		return -EINVAL;

	rate = hw_param_interval(params, SNDRV_PCM_HW_PARAM_RATE);
	rate->max = rate->min = data->asrc_rate;

	mask = hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT);
	snd_mask_none(mask);
	snd_mask_set(mask, data->asrc_format);

	return 0;
}

SND_SOC_DAILINK_DEFS(hifi,
	DAILINK_COMP_ARRAY(COMP_EMPTY()),
	DAILINK_COMP_ARRAY(COMP_CODEC(NULL, "wm8960-hifi")),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(hifi_fe,
	DAILINK_COMP_ARRAY(COMP_EMPTY()),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(hifi_be,
	DAILINK_COMP_ARRAY(COMP_EMPTY()),
	DAILINK_COMP_ARRAY(COMP_CODEC(NULL, "wm8960-hifi")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()));

static int of_parse_gpr(struct platform_device *pdev,
			struct imx_wm8960_data *data)
{
	int ret;
	struct of_phandle_args args;

	/* Check if board is using gpr, discard if not the case */
	ret = of_parse_phandle_with_fixed_args(pdev->dev.of_node,
					       "gpr", 3, 0, &args);
	if (ret)
		return 0;

	data->gpr = syscon_node_to_regmap(args.np);
	if (IS_ERR(data->gpr)) {
		ret = PTR_ERR(data->gpr);
		dev_err(&pdev->dev, "failed to get gpr regmap\n");
		return ret;
	}

	regmap_update_bits(data->gpr, args.args[0], args.args[1],
			   args.args[2]);

	return 0;
}

static int setup_audmux(struct device *dev)
{
	struct device_node *np = dev->of_node;
	int int_port, ext_port;
	int ret;

	ret = of_property_read_u32(np, "mux-int-port", &int_port);
	if (ret) {
		dev_err(dev, "mux-int-port missing or invalid\n");
		return ret;
	}
	ret = of_property_read_u32(np, "mux-ext-port", &ext_port);
	if (ret) {
		dev_err(dev, "mux-ext-port missing or invalid\n");
		return ret;
	}

	/*
	 * The port numbering in the hardware manual starts at 1, while
	 * the audmux API expects it starts at 0.
	 */
	int_port--;
	ext_port--;
	ret = imx_audmux_v2_configure_port(int_port,
			IMX_AUDMUX_V2_PTCR_SYN |
			IMX_AUDMUX_V2_PTCR_TFSEL(ext_port) |
			IMX_AUDMUX_V2_PTCR_TCSEL(ext_port) |
			IMX_AUDMUX_V2_PTCR_TFSDIR |
			IMX_AUDMUX_V2_PTCR_TCLKDIR,
			IMX_AUDMUX_V2_PDCR_RXDSEL(ext_port));
	if (ret) {
		dev_err(dev, "audmux internal port setup failed\n");
		return ret;
	}
	imx_audmux_v2_configure_port(ext_port,
			IMX_AUDMUX_V2_PTCR_SYN,
			IMX_AUDMUX_V2_PDCR_RXDSEL(int_port));
	if (ret)
		dev_err(dev, "audmux external port setup failed\n");
	return ret;
}

static int amp_gain_set(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card =  snd_kcontrol_chip(kcontrol);
	struct imx_wm8960_data *data = container_of(card,
			struct imx_wm8960_data, card);
	int value = ucontrol->value.integer.value[0];
	int i;

	if (value >= (1 << AMP_GAIN_CNT))
		return -EINVAL;

	data->amp_gain_value = value;
	value = data->amp_gain_seq[value];

	for (i = 0; i < AMP_GAIN_CNT; i++) {
		struct gpio_desc *gd = data->amp_gain[i];

		if (!gd)
			break;
		gpiod_set_value(gd, value & 1);
		value >>= 1;
	}
	return 0;
}

static int amp_gain_get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card =  snd_kcontrol_chip(kcontrol);
	struct imx_wm8960_data *data = container_of(card,
			struct imx_wm8960_data, card);

	ucontrol->value.integer.value[0] = data->amp_gain_value;
	return 0;
}

static int amp_enable_set(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card =  snd_kcontrol_chip(kcontrol);
	struct imx_wm8960_data *data = container_of(card,
			struct imx_wm8960_data, card);
	int value = ucontrol->value.integer.value[0];

	if (value > 1)
		return -EINVAL;
	value = (value ^ 1 ) | data->amp_disabled;
	data->amp_mute_value = value;
	if (data->amp_mute)
		gpiod_set_value(data->amp_mute, value);
	return 0;
}

static int amp_enable_get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card =  snd_kcontrol_chip(kcontrol);
	struct imx_wm8960_data *data = container_of(card,
			struct imx_wm8960_data, card);

	ucontrol->value.integer.value[0] = data->amp_mute_value ^ 1;
	return 0;
}

static const struct snd_kcontrol_new more_controls[] = {
	SOC_SINGLE_EXT("amp_gain", 0, 0, (1 << AMP_GAIN_CNT) - 1, 0,
		       amp_gain_get, amp_gain_set),
	SOC_SINGLE_EXT("amp_enable", 0, 0, 1, 0,
		       amp_enable_get, amp_enable_set),
};

static int imx_wm8960_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *cpu_np = NULL, *codec_np = NULL;
	struct platform_device *cpu_pdev;
	struct imx_wm8960_data *data;
	struct platform_device *asrc_pdev = NULL;
	struct device_node *asrc_np;
	struct gpio_desc *gd = NULL;
	unsigned amp_standby_enter_wait_ms = 0;
	unsigned amp_standby_exit_delay_ms = 0;
	const struct snd_kcontrol_new *kcontrols = more_controls;
	int kcontrols_cnt = ARRAY_SIZE(more_controls);
	u32 width;
	int ret;
	int i;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto fail;
	}

	data->pdev = pdev;
	data->imx_hp_jack_pin.pin = "Headphone Jack";
	data->imx_hp_jack_pin.mask = SND_JACK_HEADPHONE;

	data->imx_hp_jack_gpio.name = "headphone detect";
	data->imx_hp_jack_gpio.report = SND_JACK_HEADPHONE;
	data->imx_hp_jack_gpio.debounce_time = 250;
	data->imx_hp_jack_gpio.invert = 0;

	data->imx_mic_jack_pin.pin = "Mic Jack";
	data->imx_mic_jack_pin.mask = SND_JACK_MICROPHONE;

	data->imx_mic_jack_gpio.name = "mic detect";
	data->imx_mic_jack_gpio.report = SND_JACK_MICROPHONE;
	data->imx_mic_jack_gpio.debounce_time = 250;
	data->imx_mic_jack_gpio.invert = 0;

	if (of_property_read_bool(pdev->dev.of_node, "codec-rpmsg"))
		data->is_codec_rpmsg = true;

	cpu_np = of_parse_phandle(np, "audio-cpu", 0);
	/* Give a chance to old DT binding */
	if (!cpu_np)
		cpu_np = of_parse_phandle(np, "cpu-dai", 0);
	if (!cpu_np) {
		dev_err(&pdev->dev, "cpu dai phandle missing or invalid\n");
		ret = -EINVAL;
		goto fail;
	}

	if (strstr(cpu_np->name, "ssi")) {
		ret = setup_audmux(&pdev->dev);
		if (ret)
			return ret;
	}

	cpu_pdev = of_find_device_by_node(cpu_np);
	if (!cpu_pdev) {
		dev_err(&pdev->dev, "failed to find SSI/SAI platform device\n");
		ret = -EPROBE_DEFER;
		goto fail;
	}

	codec_np = of_parse_phandle(np, "audio-codec", 0);
	if (!codec_np) {
		dev_err(&pdev->dev, "phandle missing or invalid\n");
		ret = -EINVAL;
		goto fail;
	}

	if (data->is_codec_rpmsg) {
		struct platform_device *codec_dev;

		codec_dev = of_find_device_by_node(codec_np);
		if (!codec_dev || !codec_dev->dev.driver) {
			dev_err(&pdev->dev, "failed to find codec platform device\n");
			ret = -EPROBE_DEFER;
			goto fail;
		}

		data->codec_clk = devm_clk_get(&codec_dev->dev, "mclk");
		if (IS_ERR(data->codec_clk)) {
			ret = PTR_ERR(data->codec_clk);
			dev_err(&pdev->dev, "failed to get codec clk: %d\n", ret);
			goto fail;
		}
	} else {
		struct i2c_client *codec_dev;

		codec_dev = of_find_i2c_device_by_node(codec_np);
		if (!codec_dev || !codec_dev->dev.driver) {
			dev_dbg(&pdev->dev, "failed to find codec platform device\n");
			ret = -EPROBE_DEFER;
			goto fail;
		}

		data->codec_clk = devm_clk_get(&codec_dev->dev, "mclk");
		if (IS_ERR(data->codec_clk)) {
			ret = PTR_ERR(data->codec_clk);
			dev_err(&pdev->dev, "failed to get codec clk: %d\n", ret);
			goto fail;
		}
	}

	gd = devm_gpiod_get_index_optional(&pdev->dev, "amp-mute", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(gd)) {
		ret = PTR_ERR(gd);
		goto fail;
	}
	data->amp_mute = gd;
	data->amp_mute_value = 1;
	data->amp_standby_state = 1;
	gd = devm_gpiod_get_index_optional(&pdev->dev, "amp-standby", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(gd)) {
		ret = PTR_ERR(gd);
		goto fail;
	}
	data->amp_standby = gd;

	of_property_read_u32(np, "amp-standby-enter-wait-ms", &amp_standby_enter_wait_ms);
	of_property_read_u32(np, "amp-standby-exit-delay-ms", &amp_standby_exit_delay_ms);
	data->amp_standby_enter_wait_ms = amp_standby_enter_wait_ms;
	data->amp_standby_exit_delay_ms = amp_standby_exit_delay_ms;
	for (i = 0; i < (1 << AMP_GAIN_CNT); i++)
		data->amp_gain_seq[i] = i;

	for (i = 0; i < AMP_GAIN_CNT; i++) {
		gd = devm_gpiod_get_index_optional(&pdev->dev, "amp-gain", i, GPIOD_OUT_LOW);
		if (IS_ERR(gd)) {
			ret = PTR_ERR(gd);
			goto fail;
		}
		data->amp_gain[i] = gd;
		if (!gd)
			break;
	}
	of_property_read_u8_array(np, "amp-gain-seq", data->amp_gain_seq,
			(1 << i));

	if (of_property_read_bool(np, "codec-master"))
		data->is_codec_master = true;

	if (of_property_read_bool(pdev->dev.of_node, "capture-only"))
		data->is_capture_only = true;

	if (of_property_read_bool(pdev->dev.of_node, "playback-only"))
		data->is_playback_only = true;

	if (data->is_capture_only && data->is_playback_only) {
		ret = -EINVAL;
		dev_err(&pdev->dev, "failed for playback only and capture only\n");
		goto fail;
	}

	data->imx_wm8960_dai[0].name = "HiFi";
	data->imx_wm8960_dai[0].stream_name = "HiFi";
	data->imx_wm8960_dai[0].ops = &imx_hifi_ops;
	data->imx_wm8960_dai[0].cpus = hifi_cpus;
	data->imx_wm8960_dai[0].num_cpus = ARRAY_SIZE(hifi_cpus);
	data->imx_wm8960_dai[0].codecs = hifi_codecs;
	data->imx_wm8960_dai[0].num_codecs = ARRAY_SIZE(hifi_codecs);
	data->imx_wm8960_dai[0].platforms = hifi_platforms;
	data->imx_wm8960_dai[0].num_platforms = ARRAY_SIZE(hifi_platforms);

	data->imx_wm8960_dai[1].name = "HiFi-ASRC-FE";
	data->imx_wm8960_dai[1].stream_name = "HiFi-ASRC-FE";
	data->imx_wm8960_dai[1].dynamic = 1;
	data->imx_wm8960_dai[1].ignore_pmdown_time = 1;
	data->imx_wm8960_dai[1].dpcm_playback = 1;
	data->imx_wm8960_dai[1].dpcm_capture = 1;
	data->imx_wm8960_dai[1].dpcm_merged_chan = 1;
	data->imx_wm8960_dai[1].cpus = hifi_fe_cpus;
	data->imx_wm8960_dai[1].num_cpus = ARRAY_SIZE(hifi_fe_cpus);
	data->imx_wm8960_dai[1].codecs = hifi_fe_codecs;
	data->imx_wm8960_dai[1].num_codecs = ARRAY_SIZE(hifi_fe_codecs);
	data->imx_wm8960_dai[1].platforms = hifi_fe_platforms;
	data->imx_wm8960_dai[1].num_platforms = ARRAY_SIZE(hifi_fe_platforms);

	data->imx_wm8960_dai[2].name = "HiFi-ASRC-BE";
	data->imx_wm8960_dai[2].stream_name = "HiFi-ASRC-BE";
	data->imx_wm8960_dai[2].no_pcm = 1;
	data->imx_wm8960_dai[2].ignore_pmdown_time = 1;
	data->imx_wm8960_dai[2].dpcm_playback = 1;
	data->imx_wm8960_dai[2].dpcm_capture = 1;
	data->imx_wm8960_dai[2].ops = &imx_hifi_ops;
	data->imx_wm8960_dai[2].be_hw_params_fixup = be_hw_params_fixup;
	data->imx_wm8960_dai[2].cpus = hifi_be_cpus;
	data->imx_wm8960_dai[2].num_cpus = ARRAY_SIZE(hifi_be_cpus);
	data->imx_wm8960_dai[2].codecs = hifi_be_codecs;
	data->imx_wm8960_dai[2].num_codecs = ARRAY_SIZE(hifi_be_codecs);
	data->imx_wm8960_dai[2].platforms = hifi_be_platforms;
	data->imx_wm8960_dai[2].num_platforms = ARRAY_SIZE(hifi_be_platforms);

	if (data->is_capture_only) {
		data->imx_wm8960_dai[0].capture_only = true;
		data->imx_wm8960_dai[1].capture_only = true;
		data->imx_wm8960_dai[2].capture_only = true;
	}

	if (data->is_playback_only) {
		data->imx_wm8960_dai[0].playback_only = true;
		data->imx_wm8960_dai[1].playback_only = true;
		data->imx_wm8960_dai[2].playback_only = true;
	}

	ret = of_parse_gpr(pdev, data);
	if (ret)
		goto fail;

	data->hp_det[0] = ~0;
	of_property_read_u32_array(np, "hp-det", data->hp_det, 2);

	asrc_np = of_parse_phandle(np, "asrc-controller", 0);
	if (asrc_np) {
		asrc_pdev = of_find_device_by_node(asrc_np);
		data->asrc_pdev = asrc_pdev;
	}

	if (!data->amp_gain[0]) {
		kcontrols++;
		kcontrols_cnt--;
	}
	if (!data->amp_mute)
		kcontrols_cnt--;
	if (kcontrols_cnt) {
		data->card.controls	 = kcontrols;
		data->card.num_controls  = kcontrols_cnt;
	}
	data->card.dai_link = data->imx_wm8960_dai;

	if (data->is_codec_rpmsg) {
		data->imx_wm8960_dai[0].codecs->name     = "rpmsg-audio-codec-wm8960";
		data->imx_wm8960_dai[0].codecs->dai_name = "rpmsg-wm8960-hifi";
	} else
		data->imx_wm8960_dai[0].codecs->of_node	= codec_np;

	data->imx_wm8960_dai[0].cpus->dai_name = dev_name(&cpu_pdev->dev);
	data->imx_wm8960_dai[0].platforms->of_node = cpu_np;

	if (!asrc_pdev) {
		data->card.num_links = 1;
	} else {
		data->imx_wm8960_dai[1].cpus->of_node = asrc_np;
		data->imx_wm8960_dai[1].platforms->of_node = asrc_np;
		if (data->is_codec_rpmsg) {
			data->imx_wm8960_dai[2].codecs->name     = "rpmsg-audio-codec-wm8960";
			data->imx_wm8960_dai[2].codecs->dai_name = "rpmsg-wm8960-hifi";
		} else
			data->imx_wm8960_dai[2].codecs->of_node	= codec_np;
		data->imx_wm8960_dai[2].cpus->dai_name = dev_name(&cpu_pdev->dev);
		data->card.num_links = 3;

		ret = of_property_read_u32(asrc_np, "fsl,asrc-rate",
				&data->asrc_rate);
		if (ret) {
			dev_err(&pdev->dev, "failed to get output rate\n");
			ret = -EINVAL;
			goto fail;
		}

		ret = of_property_read_u32(asrc_np, "fsl,asrc-width", &width);
		if (ret) {
			dev_err(&pdev->dev, "failed to get output rate\n");
			ret = -EINVAL;
			goto fail;
		}

		if (width == 24)
			data->asrc_format = SNDRV_PCM_FORMAT_S24_LE;
		else
			data->asrc_format = SNDRV_PCM_FORMAT_S16_LE;
	}

	data->card.dev = &pdev->dev;
	data->card.owner = THIS_MODULE;
	ret = snd_soc_of_parse_card_name(&data->card, "model");
	if (ret)
		goto fail;
	data->card.dapm_widgets = imx_wm8960_dapm_widgets;
	data->card.num_dapm_widgets = ARRAY_SIZE(imx_wm8960_dapm_widgets);

	ret = snd_soc_of_parse_audio_routing(&data->card, "audio-routing");
	if (ret)
		goto fail;

	data->card.late_probe = imx_wm8960_late_probe;

	data->imx_hp_jack_gpio.gpio = of_get_named_gpio_flags(np,
							      "hp-det-gpio", 0,
							      &data->hp_active_low);
	if (!gpio_is_valid(data->imx_hp_jack_gpio.gpio)) {
		/* Try with gpios*/
		data->imx_hp_jack_gpio.gpio = of_get_named_gpio_flags(np,
					      "hp-det-gpios", 0,
					      &data->hp_active_low);
	}
	data->imx_mic_jack_gpio.gpio = of_get_named_gpio_flags(np,
							       "mic-det-gpio", 0,
							       &data->mic_active_low);
	if (!gpio_is_valid(data->imx_mic_jack_gpio.gpio)) {
		/* Try with gpios*/
		data->imx_mic_jack_gpio.gpio = of_get_named_gpio_flags(np,
					       "mic-det-gpios", 0,
					       &data->mic_active_low);
	}

	if (gpio_is_valid(data->imx_hp_jack_gpio.gpio) &&
	    gpio_is_valid(data->imx_mic_jack_gpio.gpio) &&
	    data->imx_hp_jack_gpio.gpio == data->imx_mic_jack_gpio.gpio)
		data->is_headset_jack = true;

	platform_set_drvdata(pdev, &data->card);
	snd_soc_card_set_drvdata(&data->card, data);
	ret = devm_snd_soc_register_card(&pdev->dev, &data->card);
	if (ret)
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n", ret);
fail:
	if (cpu_np)
		of_node_put(cpu_np);
	if (codec_np)
		of_node_put(codec_np);

	return ret;
}

static int imx_wm8960_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct imx_wm8960_data *data = snd_soc_card_get_drvdata(card);

	if (data->amp_mute)
		gpiod_set_value(data->amp_mute, 1);
	if (data->amp_standby)
		gpiod_set_value(data->amp_standby, 1);
	device_remove_file(&pdev->dev, &dev_attr_micphone);
	device_remove_file(&pdev->dev, &dev_attr_headphone);

	return 0;
}

static const struct of_device_id imx_wm8960_dt_ids[] = {
	{ .compatible = "fsl,imx-audio-wm8960", },
	{ .compatible = "fsl,imx7d-evk-wm8960"  },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, imx_wm8960_dt_ids);

static struct platform_driver imx_wm8960_driver = {
	.driver = {
		.name = "imx-wm8960",
		.pm = &snd_soc_pm_ops,
		.of_match_table = imx_wm8960_dt_ids,
	},
	.probe = imx_wm8960_probe,
	.remove = imx_wm8960_remove,
};
module_platform_driver(imx_wm8960_driver);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("Freescale i.MX WM8960 ASoC machine driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:imx-wm8960");
