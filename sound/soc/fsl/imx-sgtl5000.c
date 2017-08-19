/*
 * Copyright 2012 Freescale Semiconductor, Inc.
 * Copyright 2012 Linaro Ltd.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <sound/jack.h>
#include <sound/soc.h>

#include "../codecs/sgtl5000.h"
#include "imx-audmux.h"

#define DAI_NAME_SIZE	32

struct imx_sgtl5000_data {
	struct snd_soc_dai_link dai;
	struct snd_soc_card card;
	char codec_dai_name[DAI_NAME_SIZE];
	char platform_name[DAI_NAME_SIZE];
	struct clk *codec_clk;
	unsigned int clk_frequency;
	struct gpio_desc *mute_hp;
	struct gpio_desc *mute_lo;
	struct gpio_desc *amp_standby;
#define AMP_GAIN_CNT 2
	struct gpio_desc *amp_gain[AMP_GAIN_CNT];
	unsigned char amp_gain_seq[1 << AMP_GAIN_CNT];
	int amp_gain_value;
	int mute_hp_value;
	int hp_disabled;
	int hp_det_status;
	int standby_state;
	bool limit_16bit_samples;
	unsigned amp_standby_enter_wait_ms;
	unsigned amp_standby_exit_delay_ms;
	int spk_mute_on_hp_detect;
	struct snd_soc_jack hp_jack;
	struct snd_soc_jack_pin hp_jack_pins;
	struct snd_soc_jack_gpio hp_jack_gpio;
};

static ssize_t show_headphone(struct device *dev,
        struct device_attribute *attr, char *buf)
{
	struct snd_soc_card *card = dev_get_drvdata(dev);
	struct imx_sgtl5000_data *data = snd_soc_card_get_drvdata(card);
	int hp_det_status;

	if (data->hp_jack_gpio.desc) {
		/* Check if headphone is plugged in */
		hp_det_status = gpiod_get_value_cansleep(data->hp_jack_gpio.desc);

		if (hp_det_status)
			strcpy(buf, "headphone\n");
		else
			strcpy(buf, "speaker\n");
	} else {
		strcpy(buf, "no detect gpio connected\n");
	}
	return strlen(buf);
}

static DEVICE_ATTR(headphone, S_IRUGO, show_headphone, NULL);

static int hpjack_status_check(void *_data)
{
	struct imx_sgtl5000_data *data = _data;
	int hp_det_status, ret;

	if (!data->hp_jack_gpio.desc)
		return 0;

	hp_det_status = gpiod_get_value_cansleep(data->hp_jack_gpio.desc);
	if (data->spk_mute_on_hp_detect) {
		if (hp_det_status && !data->hp_disabled) {
			int i;

			for (i = 0; i < 5; i++) {
				hp_det_status = gpiod_get_value_cansleep(
						data->hp_jack_gpio.desc);
				if (!hp_det_status)
					break;
				msleep(1);
			}
		}
		gpiod_set_value_cansleep(data->mute_hp,
			hp_det_status | data->hp_disabled);
	}
	data->hp_det_status = hp_det_status;
	if (!hp_det_status)
		ret = 0;
	else
		ret = data->hp_jack_gpio.report;
	return ret;
}

static int hp_init(struct device *dev, struct imx_sgtl5000_data *data)
{
	int ret;

	data->hp_jack_pins.pin = "Headphone Jack";
	data->hp_jack_pins.mask = SND_JACK_HEADPHONE;
	data->hp_jack_gpio.name = "headphone detect";
	data->hp_jack_gpio.report = SND_JACK_HEADPHONE;
	data->hp_jack_gpio.debounce_time = 250;

	data->hp_jack_gpio.gpiod_dev = dev;
	data->hp_jack_gpio.name = "hp-detect";
	data->hp_jack_gpio.jack_status_check = hpjack_status_check;
	data->hp_jack_gpio.data = data;

	ret = snd_soc_card_jack_new(&data->card, "Headphone Jack", SND_JACK_HEADPHONE,
		&data->hp_jack, &data->hp_jack_pins, 1);

	ret = snd_soc_jack_add_gpios(&data->hp_jack, 1, &data->hp_jack_gpio);
	if (!ret)
		data->hp_det_status = gpiod_get_value_cansleep(data->hp_jack_gpio.desc);
	else
		snd_soc_jack_report(&data->hp_jack, SND_JACK_HEADPHONE, SND_JACK_HEADPHONE);

	return ret;
}

static int imx_sgtl5000_dai_init(struct snd_soc_pcm_runtime *rtd)
{
	struct imx_sgtl5000_data *data = snd_soc_card_get_drvdata(rtd->card);
	struct device *dev = rtd->card->dev;
	int ret;

	ret = snd_soc_dai_set_sysclk(rtd->codec_dai, SGTL5000_SYSCLK,
				     data->clk_frequency, SND_SOC_CLOCK_IN);
	if (ret) {
		dev_err(dev, "could not set codec driver clock params\n");
		return ret;
	}

	return 0;
}

int do_mute(struct gpio_desc *gd, int mute)
{
	if (!gd)
		return 0;

	gpiod_set_value(gd, mute);
	return 0;
}

static int event_spk_mute(struct imx_sgtl5000_data *data, int mute)
{
	data->hp_disabled = data->mute_hp_value = mute;
	if (!mute && data->spk_mute_on_hp_detect && data->hp_det_status)
		mute = 1;
	return do_mute(data->mute_hp, mute);
}

static int event_spk_stdby(struct imx_sgtl5000_data *data, int stdby)
{
	if (!data->amp_standby)
		return 0;
	if (stdby == data->standby_state)
		return 0;

	if (stdby)
		msleep(data->amp_standby_enter_wait_ms);
	data->standby_state = stdby;
	gpiod_set_value(data->amp_standby, stdby);
	if (!stdby)
		msleep(data->amp_standby_exit_delay_ms);
	return 0;
}

static int event_spk2(struct snd_soc_dapm_widget *w,
		    struct snd_kcontrol *k, int event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;
	struct imx_sgtl5000_data *data = container_of(card,
			struct imx_sgtl5000_data, card);
	int stdby = SND_SOC_DAPM_EVENT_ON(event) ? 0 : 1;

	if (event & (SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD))
		return event_spk_stdby(data, stdby);
	return event_spk_mute(data, stdby);
}

static int event_lo(struct snd_soc_dapm_widget *w,
		    struct snd_kcontrol *k, int event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;
	struct imx_sgtl5000_data *data = container_of(card,
			struct imx_sgtl5000_data, card);

	return do_mute(data->mute_lo, SND_SOC_DAPM_EVENT_ON(event) ? 0 : 1);
}

#define SND_SOC_DAPM_SPK2(wname, wevent) \
{	.id = snd_soc_dapm_spk, .name = wname, .kcontrol_news = NULL, \
	.num_kcontrols = 0, .reg = SND_SOC_NOPM, .event = wevent, \
	.event_flags =  SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU | \
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD}

static const struct snd_soc_dapm_widget imx_sgtl5000_dapm_widgets[] = {
	SND_SOC_DAPM_MIC("Mic Jack", NULL),
	SND_SOC_DAPM_LINE("Line In Jack", NULL),
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
	SND_SOC_DAPM_SPK("Line Out Jack", event_lo),
	SND_SOC_DAPM_SPK2("Ext Spk", event_spk2),
};

static int imx_sgtl_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct imx_sgtl5000_data *data = snd_soc_card_get_drvdata(rtd->card);

	if (data->limit_16bit_samples) {
		snd_pcm_hw_constraint_minmax(substream->runtime,
			SNDRV_PCM_HW_PARAM_SAMPLE_BITS, 16, 16);
	}
	return 0;
}


static struct snd_soc_ops imx_sgtl_ops = {
	.startup	= imx_sgtl_startup,
};


static int amp_gain_set(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card =  snd_kcontrol_chip(kcontrol);
	struct imx_sgtl5000_data *data = container_of(card,
			struct imx_sgtl5000_data, card);
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
	struct imx_sgtl5000_data *data = container_of(card,
			struct imx_sgtl5000_data, card);

	ucontrol->value.integer.value[0] = data->amp_gain_value;
	return 0;
}

static int amp_hp_set(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card =  snd_kcontrol_chip(kcontrol);
	struct imx_sgtl5000_data *data = container_of(card,
			struct imx_sgtl5000_data, card);
	int value = ucontrol->value.integer.value[0];

	if (value > 1)
		return -EINVAL;
	value = (value ^ 1 ) | data->hp_disabled;
	data->mute_hp_value = value;
	if (data->mute_hp)
		gpiod_set_value(data->mute_hp, value);
	return 0;
}

static int amp_hp_get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card =  snd_kcontrol_chip(kcontrol);
	struct imx_sgtl5000_data *data = container_of(card,
			struct imx_sgtl5000_data, card);

	ucontrol->value.integer.value[0] = data->mute_hp_value ^ 1;
	return 0;
}

static const struct snd_kcontrol_new more_controls[] = {
	SOC_SINGLE_EXT("amp_gain", 0, 0, (1 << AMP_GAIN_CNT) - 1, 0,
		       amp_gain_get, amp_gain_set),
	SOC_SINGLE_EXT("amp_hp", 0, 0, 1, 0,
		       amp_hp_get, amp_hp_set),
};

static int imx_sgtl5000_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *ssi_np, *codec_np;
	struct platform_device *ssi_pdev;
	struct i2c_client *codec_dev;
	struct imx_sgtl5000_data *data = NULL;
	struct gpio_desc *gd = NULL;
	int int_port, ext_port;
	unsigned amp_standby_enter_wait_ms = 0;
	unsigned amp_standby_exit_delay_ms = 0;
	int spk_mute_on_hp_detect;
	const struct snd_kcontrol_new *kcontrols = more_controls;
	int kcontrols_cnt = ARRAY_SIZE(more_controls);
	int ret;
	int i;

	ret = of_property_read_u32(np, "mux-int-port", &int_port);
	if (ret) {
		dev_err(&pdev->dev, "mux-int-port missing or invalid\n");
		return ret;
	}
	ret = of_property_read_u32(np, "mux-ext-port", &ext_port);
	if (ret) {
		dev_err(&pdev->dev, "mux-ext-port missing or invalid\n");
		return ret;
	}
	spk_mute_on_hp_detect = of_property_read_bool(np, "spk-mute-on-hp-detect");
	of_property_read_u32(np, "amp-standby-enter-wait-ms", &amp_standby_enter_wait_ms);
	of_property_read_u32(np, "amp-standby-exit-delay-ms", &amp_standby_exit_delay_ms);

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
		dev_err(&pdev->dev, "audmux internal port setup failed\n");
		return ret;
	}
	ret = imx_audmux_v2_configure_port(ext_port,
			IMX_AUDMUX_V2_PTCR_SYN,
			IMX_AUDMUX_V2_PDCR_RXDSEL(int_port));
	if (ret) {
		dev_err(&pdev->dev, "audmux external port setup failed\n");
		return ret;
	}

	ssi_np = of_parse_phandle(pdev->dev.of_node, "ssi-controller", 0);
	codec_np = of_parse_phandle(pdev->dev.of_node, "audio-codec", 0);
	if (!ssi_np || !codec_np) {
		dev_err(&pdev->dev, "phandle missing or invalid\n");
		ret = -EINVAL;
		goto fail;
	}

	ssi_pdev = of_find_device_by_node(ssi_np);
	if (!ssi_pdev) {
		dev_err(&pdev->dev, "failed to find SSI platform device\n");
		ret = -EPROBE_DEFER;
		goto fail;
	}
	codec_dev = of_find_i2c_device_by_node(codec_np);
	if (!codec_dev) {
		dev_err(&pdev->dev, "failed to find codec platform device\n");
		return -EPROBE_DEFER;
	}

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto fail;
	}

	data->spk_mute_on_hp_detect = spk_mute_on_hp_detect;
	data->amp_standby_enter_wait_ms = amp_standby_enter_wait_ms;
	data->amp_standby_exit_delay_ms = amp_standby_exit_delay_ms;
	data->codec_clk = clk_get(&codec_dev->dev, NULL);
	if (IS_ERR(data->codec_clk)) {
		ret = PTR_ERR(data->codec_clk);
		goto fail;
	}

	data->clk_frequency = clk_get_rate(data->codec_clk);

	data->dai.name = "HiFi";
	data->dai.stream_name = "HiFi";
	data->dai.codec_dai_name = "sgtl5000";
	data->dai.codec_of_node = codec_np;
	data->dai.cpu_of_node = ssi_np;
	data->dai.platform_of_node = ssi_np;
	data->dai.init = &imx_sgtl5000_dai_init;
	data->dai.ops = &imx_sgtl_ops;
	data->dai.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
			    SND_SOC_DAIFMT_CBM_CFM;

	data->limit_16bit_samples = of_property_read_bool(pdev->dev.of_node, "limit-to-16-bit-samples");
	gd = devm_gpiod_get_index_optional(&pdev->dev, "mute", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(gd)) {
		ret = PTR_ERR(gd);
		goto fail;
	}
	data->mute_hp = gd;
	data->mute_hp_value = 1;
	data->standby_state = 1;

	gd = devm_gpiod_get_index_optional(&pdev->dev, "line-out-mute", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(gd)) {
		ret = PTR_ERR(gd);
		goto fail;
	}
	data->mute_lo = gd;

	gd = devm_gpiod_get_index_optional(&pdev->dev, "amp-standby", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(gd)) {
		ret = PTR_ERR(gd);
		goto fail;
	}
	data->amp_standby = gd;

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

	data->card.dev = &pdev->dev;
	ret = snd_soc_of_parse_card_name(&data->card, "model");
	if (ret)
		goto fail;
	ret = snd_soc_of_parse_audio_routing(&data->card, "audio-routing");
	if (ret)
		goto fail;
	data->card.num_links = 1;
	data->card.owner = THIS_MODULE;
	if (!data->amp_gain[0]) {
		kcontrols++;
		kcontrols_cnt--;
	}
	if (!data->mute_hp)
		kcontrols_cnt--;
	if (kcontrols_cnt) {
		data->card.controls	 = kcontrols;
		data->card.num_controls  = kcontrols_cnt;
	}
	data->card.dai_link = &data->dai;
	data->card.dapm_widgets = imx_sgtl5000_dapm_widgets;
	data->card.num_dapm_widgets = ARRAY_SIZE(imx_sgtl5000_dapm_widgets);

	platform_set_drvdata(pdev, &data->card);
	snd_soc_card_set_drvdata(&data->card, data);

	ret = devm_snd_soc_register_card(&pdev->dev, &data->card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n", ret);
		goto fail;
	}

	of_node_put(ssi_np);
	of_node_put(codec_np);
	if (!hp_init(&pdev->dev, data)) {
		ret = device_create_file(&pdev->dev, &dev_attr_headphone);
		if (ret)
			dev_err(&pdev->dev, "create hp attr failed (%d)\n", ret);
	}

	return 0;

fail:
	if (data && !IS_ERR(data->codec_clk))
		clk_put(data->codec_clk);
	of_node_put(ssi_np);
	of_node_put(codec_np);

	return ret;
}

static int imx_sgtl5000_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct imx_sgtl5000_data *data = snd_soc_card_get_drvdata(card);
	int i;

	if (data->amp_standby)
		gpiod_set_value(data->amp_standby, 1);
	if (data->mute_lo)
		gpiod_set_value(data->mute_lo, 1);
	if (data->mute_hp)
		gpiod_set_value(data->mute_hp, 1);
	for (i = 0; i < AMP_GAIN_CNT; i++) {
		struct gpio_desc *gd = data->amp_gain[i];

		if (gd)
			gpiod_set_value(gd, 0);
	}
	clk_put(data->codec_clk);
	device_remove_file(&pdev->dev, &dev_attr_headphone);

	return 0;
}

static const struct of_device_id imx_sgtl5000_dt_ids[] = {
	{ .compatible = "fsl,imx-audio-sgtl5000", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, imx_sgtl5000_dt_ids);

static struct platform_driver imx_sgtl5000_driver = {
	.driver = {
		.name = "imx-sgtl5000",
		.pm = &snd_soc_pm_ops,
		.of_match_table = imx_sgtl5000_dt_ids,
	},
	.probe = imx_sgtl5000_probe,
	.remove = imx_sgtl5000_remove,
};
module_platform_driver(imx_sgtl5000_driver);

MODULE_AUTHOR("Shawn Guo <shawn.guo@linaro.org>");
MODULE_DESCRIPTION("Freescale i.MX SGTL5000 ASoC machine driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:imx-sgtl5000");
