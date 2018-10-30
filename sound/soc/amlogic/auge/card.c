/*
 * sound/soc/amlogic/auge/card.c
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

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/extcon.h>
#include <sound/jack.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>
#include "../../../../drivers/gpio/gpiolib.h"
#include "card.h"

#include "effects.h"

struct aml_jack {
	struct snd_soc_jack jack;
	struct snd_soc_jack_pin pin;
	struct snd_soc_jack_gpio gpio;
};

struct aml_chipset_info {
	/* INT address separated from start address for ddr */
	bool ddr_addr_separated;
	/* two spdif out ? */
	bool spdif_b;
	/* eq/drc function */
	bool eqdrc_fn;
};

struct aml_card_data {
	struct snd_soc_card snd_card;
	struct aml_dai_props {
		/* sync with android audio hal,
		 * dai link is used for which output,
		 */
		const char *suffix_name;

		struct aml_dai cpu_dai;
		struct aml_dai codec_dai;
		unsigned int mclk_fs;
	} *dai_props;
	unsigned int mclk_fs;
	struct aml_jack hp_jack;
	struct aml_jack mic_jack;
	struct snd_soc_dai_link *dai_link;
	int spk_mute_gpio;
	bool spk_mute_active_low;
	struct gpio_desc *avout_mute_desc;
	struct loopback_cfg lb_cfg;
	struct timer_list timer;
	struct work_struct work;
	bool hp_last_state;
	bool hp_cur_state;
	bool hp_det_status;
	int hp_gpio_det;
	int hp_detect_flag;
	bool hp_det_enable;
	bool micphone_last_state;
	bool micphone_cur_state;
	bool micphone_det_status;
	int micphone_gpio_det;
	int mic_detect_flag;
	bool mic_det_enable;

	struct aml_chipset_info *chipinfo;
};

#define aml_priv_to_dev(priv) ((priv)->snd_card.dev)
#define aml_priv_to_link(priv, i) ((priv)->snd_card.dai_link + (i))
#define aml_priv_to_props(priv, i) ((priv)->dai_props + (i))
#define aml_card_to_priv(card) \
	(container_of(card, struct aml_card_data, snd_card))

#define DAI	"sound-dai"
#define CELL	"#sound-dai-cells"
#define PREFIX	"aml-audio-card,"

#define aml_card_init_hp(card, sjack, prefix)\
	aml_card_init_jack(card, sjack, 1, prefix)
#define aml_card_init_mic(card, sjack, prefix)\
	aml_card_init_jack(card, sjack, 0, prefix)

static const unsigned int headphone_cable[] = {
	EXTCON_JACK_HEADPHONE,
	EXTCON_NONE,
};

static const unsigned int microphone_cable[] = {
	EXTCON_JACK_MICROPHONE,
	EXTCON_NONE,
};

struct extcon_dev *audio_extcon_headphone;
struct extcon_dev *audio_extcon_microphone;

static void jack_audio_start_timer(struct aml_card_data *card_data,
				  unsigned long delay)
{
	card_data->timer.expires = jiffies + delay;
	add_timer(&card_data->timer);
}

static void jack_audio_stop_timer(struct aml_card_data *card_data)
{
	del_timer_sync(&card_data->timer);
	cancel_work_sync(&card_data->work);
}
static void jack_timer_func(unsigned long data)
{
	struct aml_card_data *card_data = (struct aml_card_data *)data;
	unsigned long delay = msecs_to_jiffies(150);

	schedule_work(&card_data->work);
	mod_timer(&card_data->timer, jiffies + delay);
}

static int jack_audio_hp_detect(struct aml_card_data *card_data)
{
	int loop_num = 0;
	int change_cnt = 0;

	card_data->hp_cur_state =
		gpio_get_value_cansleep(card_data->hp_jack.gpio.gpio);
	if (card_data->hp_last_state != card_data->hp_cur_state) {
		while (loop_num < 5) {
			card_data->hp_cur_state =
				gpio_get_value_cansleep(
					card_data->hp_jack.gpio.gpio);

			if (card_data->hp_last_state != card_data->hp_cur_state)
				change_cnt++;
			else
				change_cnt = 0;

			msleep_interruptible(50);
			loop_num = loop_num + 1;
		}
		if (change_cnt >= 5) {
			card_data->hp_last_state = card_data->hp_cur_state;
			card_data->hp_det_status = card_data->hp_last_state;
		}
		return card_data->hp_det_status;
	}
	return -1;
}

static int jack_audio_micphone_detect(struct aml_card_data *card_data)
{
	int loop_num = 0;
	int change_cnt = 0;

	card_data->micphone_cur_state =
		gpio_get_value_cansleep(card_data->mic_jack.gpio.gpio);
	if (card_data->micphone_last_state != card_data->micphone_cur_state) {
		while (loop_num < 5) {
			card_data->micphone_cur_state =
				gpio_get_value_cansleep(
					card_data->mic_jack.gpio.gpio);
			if (card_data->micphone_last_state !=
				card_data->micphone_cur_state)
				change_cnt++;
			else
				change_cnt = 0;

			msleep_interruptible(50);
			loop_num = loop_num + 1;
		}
		if (change_cnt >= 5) {
			card_data->micphone_last_state =
				card_data->micphone_cur_state;
			card_data->micphone_det_status =
				card_data->micphone_last_state;
		}
		return card_data->micphone_det_status;
	}
	return -1;
}
static void jack_work_func(struct work_struct *work)
{
	struct aml_card_data *card_data = NULL;
	int status = SND_JACK_HEADPHONE;
	int flag = 0;

	card_data = container_of(work, struct aml_card_data, work);

	if (card_data->hp_det_enable == 1) {
		flag = jack_audio_hp_detect(card_data);
		if (flag == -1)
			return;
		if (card_data->hp_detect_flag != flag) {

			card_data->hp_detect_flag = flag;

			if (flag) {
				extcon_set_state_sync(audio_extcon_headphone,
					EXTCON_JACK_HEADPHONE, 1);
				snd_soc_jack_report(&card_data->hp_jack.jack,
					status, SND_JACK_HEADPHONE);
			} else {
				extcon_set_state_sync(audio_extcon_headphone,
					EXTCON_JACK_HEADPHONE, 0);
				snd_soc_jack_report(&card_data->hp_jack.jack, 0,
						SND_JACK_HEADPHONE);
			}

		}
	}
	if (card_data->mic_det_enable == 1) {
		flag = jack_audio_micphone_detect(card_data);
		if (flag == -1)
			return;
		if (card_data->mic_detect_flag != flag) {

			card_data->mic_detect_flag = flag;

			if (flag) {
				extcon_set_state_sync(audio_extcon_microphone,
					EXTCON_JACK_MICROPHONE, 1);
				snd_soc_jack_report(&card_data->mic_jack.jack,
					status, SND_JACK_MICROPHONE);
			} else {
				extcon_set_state_sync(audio_extcon_microphone,
					EXTCON_JACK_MICROPHONE, 0);
				snd_soc_jack_report(&card_data->mic_jack.jack,
					0, SND_JACK_MICROPHONE);
			}

		}
	}
}

static int aml_card_init_jack(struct snd_soc_card *card,
				      struct aml_jack *sjack,
				      int is_hp, char *prefix)
{
	struct aml_card_data *priv = aml_card_to_priv(card);
	struct device *dev = card->dev;
	struct gpio_desc *desc;
	enum of_gpio_flags flags;
	char prop[128];
	char *pin_name;
	char *gpio_name;
	int mask;
	int det;

	sjack->gpio.gpio = -ENOENT;

	if (is_hp) {
		snprintf(prop, sizeof(prop), "%shp-det-gpio", prefix);
		pin_name	= "Headphones";
		gpio_name	= "Headphone detection";
		mask		= SND_JACK_HEADPHONE;

		desc = of_get_named_gpiod_flags(dev->of_node, prop, 0, &flags);
		if (IS_ERR(desc)) {
			priv->hp_det_enable = 0;
			return -1;
		}
		priv->hp_det_enable = 1;
		det = desc_to_gpio(desc);
		gpio_request(det, "hp-det-gpio");
	} else {
		snprintf(prop, sizeof(prop), "%smic-det-gpio", prefix);
		pin_name	= "Mic Jack";
		gpio_name	= "Mic detection";
		mask		= SND_JACK_MICROPHONE;

		desc = of_get_named_gpiod_flags(dev->of_node, prop, 0, &flags);
		if (IS_ERR(desc)) {
			priv->mic_det_enable = 0;
			return -1;
		}
		priv->mic_det_enable = 1;
		det = desc_to_gpio(desc);
		gpio_request(det, "mic-det-gpio");
	}

	if (gpio_is_valid(det)) {
		sjack->pin.pin		= pin_name;
		sjack->pin.mask		= mask;

		sjack->gpio.name	= gpio_name;
		sjack->gpio.report	= mask;
		sjack->gpio.gpio	= det;
		sjack->gpio.invert	= !!(flags & OF_GPIO_ACTIVE_LOW);
		sjack->gpio.debounce_time = 150;

		gpio_direction_input(det);
		gpiod_set_pull(desc, GPIOD_PULL_DIS);
		snd_soc_card_jack_new(card, pin_name, mask,
				      &sjack->jack,
				      &sjack->pin, 1);

		snd_soc_jack_add_gpios(&sjack->jack, 1,
				       &sjack->gpio);
	} else
		pr_info("detect gpio is invalid\n");

	if (is_hp) {
		if (det >= 0)
			priv->hp_gpio_det = det;
	} else {
		if (det >= 0)
			priv->micphone_gpio_det = det;
	}
	return 0;
}

static void audio_jack_detect(struct aml_card_data *card_data)
{
	init_timer(&card_data->timer);
	card_data->timer.function = jack_timer_func;
	card_data->timer.data = (unsigned long)card_data;

	INIT_WORK(&card_data->work, jack_work_func);

	jack_audio_start_timer(card_data,
			msecs_to_jiffies(5000));
}

static void audio_extcon_register(struct aml_card_data *priv,
	struct device *dev)
{
	struct extcon_dev *edev;
	int ret;

	if (priv->hp_det_enable == 1) {
		/*audio extcon headphone*/
		edev = extcon_dev_allocate(headphone_cable);
		if (IS_ERR(edev)) {
			pr_info("failed to allocate audio extcon headphone\n");
			return;
		}
		edev->dev.parent = dev;
		edev->name = "audio_extcon_headphone";
		dev_set_name(&edev->dev, "headphone");
		ret = extcon_dev_register(edev);
		if (ret < 0) {
			pr_info("failed to register audio extcon headphone\n");
			return;
		}
		audio_extcon_headphone = edev;
	}
	if (priv->mic_det_enable == 1) {
		/*audio extcon microphone*/
		edev = extcon_dev_allocate(microphone_cable);
		if (IS_ERR(edev)) {
			pr_info("failed to allocate audio extcon microphone\n");
			return;
		}

		edev->dev.parent = dev;
		edev->name = "audio_extcon_microphone";
		dev_set_name(&edev->dev, "microphone");
		ret = extcon_dev_register(edev);
		if (ret < 0) {
			pr_info("failed to register audio extcon microphone\n");
			return;
		}
		audio_extcon_microphone = edev;
	}
}

static void aml_card_remove_jack(struct aml_jack *sjack)
{
	if (gpio_is_valid(sjack->gpio.gpio))
		snd_soc_jack_free_gpios(&sjack->jack, 1, &sjack->gpio);
}

static int aml_card_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct aml_card_data *priv =	snd_soc_card_get_drvdata(rtd->card);
	struct aml_dai_props *dai_props =
		aml_priv_to_props(priv, rtd->num);
	int ret;

	ret = clk_prepare_enable(dai_props->cpu_dai.clk);
	if (ret)
		return ret;

	ret = clk_prepare_enable(dai_props->codec_dai.clk);
	if (ret)
		clk_disable_unprepare(dai_props->cpu_dai.clk);

	return ret;
}

static void aml_card_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct aml_card_data *priv =	snd_soc_card_get_drvdata(rtd->card);
	struct aml_dai_props *dai_props =
		aml_priv_to_props(priv, rtd->num);

	clk_disable_unprepare(dai_props->cpu_dai.clk);

	clk_disable_unprepare(dai_props->codec_dai.clk);
}

static int aml_card_hw_params(struct snd_pcm_substream *substream,
				      struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct aml_card_data *priv = snd_soc_card_get_drvdata(rtd->card);
	struct snd_soc_dai_link *dai_link = aml_priv_to_link(priv, rtd->num);
	struct aml_dai_props *dai_props =
		aml_priv_to_props(priv, rtd->num);
	unsigned int mclk = 0, mclk_fs = 0;
	int i = 0, ret = 0;
	int clk_idx = substream->stream;

	if (priv->mclk_fs)
		mclk_fs = priv->mclk_fs;
	else if (dai_props->mclk_fs)
		mclk_fs = dai_props->mclk_fs;

	if (mclk_fs) {
		mclk = params_rate(params) * mclk_fs;

		for (i = 0; i < rtd->num_codecs; i++) {
			codec_dai = rtd->codec_dais[i];
			ret = snd_soc_dai_set_sysclk(codec_dai, 0, mclk,
				SND_SOC_CLOCK_IN);

			if (ret && ret != -ENOTSUPP)
				goto err;
		}

		ret = snd_soc_dai_set_sysclk(cpu_dai, clk_idx, mclk,
					     SND_SOC_CLOCK_OUT);
		if (ret && ret != -ENOTSUPP)
			goto err;

		ret = snd_soc_dai_set_fmt(cpu_dai, dai_link->dai_fmt);
		if (ret && ret != -ENOTSUPP)
			goto err;
	}

	if (loopback_is_enable() && mclk)
		loopback_hw_params(substream, params, &priv->lb_cfg, mclk);
	return 0;
err:
	return ret;
}

int aml_card_prepare(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct aml_card_data *priv = snd_soc_card_get_drvdata(rtd->card);

	if (loopback_is_enable())
		loopback_prepare(substream, &priv->lb_cfg);

	return 0;
}

int aml_card_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct aml_card_data *priv = snd_soc_card_get_drvdata(rtd->card);

	if (loopback_is_enable())
		loopback_trigger(substream, cmd, &priv->lb_cfg);

	return 0;
}

static struct snd_soc_ops aml_card_ops = {
	.startup    = aml_card_startup,
	.shutdown   = aml_card_shutdown,
	.hw_params  = aml_card_hw_params,
	.prepare    = aml_card_prepare,
	.trigger    = aml_card_trigger,
};

static int aml_card_dai_init(struct snd_soc_pcm_runtime *rtd)
{
	struct aml_card_data *priv = snd_soc_card_get_drvdata(rtd->card);
	struct snd_soc_dai *codec = rtd->codec_dai;
	struct snd_soc_dai *cpu = rtd->cpu_dai;
	struct aml_dai_props *dai_props =
		aml_priv_to_props(priv, rtd->num);
	static int hp_mic_detect_cnt;
	bool idle_clk = false;
	int ret, i;

	/* enable dai-link mclk when CONTINUOUS clk setted */
	idle_clk = !!(rtd->dai_link->dai_fmt & SND_SOC_DAIFMT_CONT);

	for (i = 0; i < rtd->num_codecs; i++) {
		codec = rtd->codec_dais[i];

		ret = aml_card_init_dai(codec, &dai_props->codec_dai, idle_clk);
		if (ret < 0)
			return ret;
	}

	ret = aml_card_init_dai(cpu, &dai_props->cpu_dai, idle_clk);
	if (ret < 0)
		return ret;

	if (hp_mic_detect_cnt == 0) {
		aml_card_init_hp(rtd->card, &priv->hp_jack, PREFIX);
		aml_card_init_mic(rtd->card, &priv->mic_jack, PREFIX);
		hp_mic_detect_cnt = 1;
	}

	return 0;
}

static int aml_card_dai_link_of(struct device_node *node,
					struct aml_card_data *priv,
					int idx,
					bool is_top_level_node)
{
	struct device *dev = aml_priv_to_dev(priv);
	struct snd_soc_dai_link *dai_link = aml_priv_to_link(priv, idx);
	struct aml_dai_props *dai_props = aml_priv_to_props(priv, idx);
	struct aml_dai *cpu_dai = &dai_props->cpu_dai;
	struct aml_dai *codec_dai = &dai_props->codec_dai;
	struct device_node *cpu = NULL;
	struct device_node *plat = NULL;
	struct device_node *codec = NULL;
	char prop[128];
	char *prefix = "";
	int ret, single_cpu;

	/* For single DAI link & old style of DT node */
	if (is_top_level_node)
		prefix = PREFIX;

	snprintf(prop, sizeof(prop), "%scpu", prefix);
	cpu = of_get_child_by_name(node, prop);

	snprintf(prop, sizeof(prop), "%splat", prefix);
	plat = of_get_child_by_name(node, prop);

	snprintf(prop, sizeof(prop), "%scodec", prefix);
	codec = of_get_child_by_name(node, prop);

	if (!cpu || !codec) {
		ret = -EINVAL;
		dev_err(dev, "%s: Can't find %s DT node\n", __func__, prop);
		goto dai_link_of_err;
	}

	ret = aml_card_parse_daifmt(dev, node, codec,
					    prefix, &dai_link->dai_fmt);
	if (ret < 0)
		goto dai_link_of_err;

	of_property_read_u32(node, "mclk-fs", &dai_props->mclk_fs);

	ret = aml_card_parse_cpu(cpu, dai_link,
					 DAI, CELL, &single_cpu);
	if (ret < 0)
		goto dai_link_of_err;

#if 0
	ret = aml_card_parse_codec(codec, dai_link, DAI, CELL);
#else
	ret = snd_soc_of_get_dai_link_codecs(dev, codec, dai_link);
#endif
	if (ret < 0)
		goto dai_link_of_err;

	ret = aml_card_parse_platform(plat, dai_link, DAI, CELL);
	if (ret < 0)
		goto dai_link_of_err;

	ret = snd_soc_of_parse_tdm_slot(cpu,	&cpu_dai->tx_slot_mask,
						&cpu_dai->rx_slot_mask,
						&cpu_dai->slots,
						&cpu_dai->slot_width);
	if (ret < 0)
		goto dai_link_of_err;

	ret = snd_soc_of_parse_tdm_slot(codec,	&codec_dai->tx_slot_mask,
						&codec_dai->rx_slot_mask,
						&codec_dai->slots,
						&codec_dai->slot_width);
	if (ret < 0)
		goto dai_link_of_err;

	ret = aml_card_parse_codec_confs(codec, &priv->snd_card);
	if (ret < 0)
		goto dai_link_of_err;

	ret = aml_card_parse_clk_cpu(cpu, dai_link, cpu_dai);
	if (ret < 0)
		goto dai_link_of_err;

#if 0
	ret = aml_card_parse_clk_codec(codec, dai_link, codec_dai);
	if (ret < 0)
		goto dai_link_of_err;
#endif

	ret = aml_card_canonicalize_dailink(dai_link);
	if (ret < 0)
		goto dai_link_of_err;

	/* sync with android audio hal, what's the link used for. */
	of_property_read_string(node, "suffix-name", &dai_props->suffix_name);

	if (dai_props->suffix_name)
		ret = aml_card_set_dailink_name(dev, dai_link,
					"%s-%s-%s",
					dai_link->cpu_dai_name,
					dai_link->codecs->dai_name,
					dai_props->suffix_name);
	else
		ret = aml_card_set_dailink_name(dev, dai_link,
					"%s-%s",
					dai_link->cpu_dai_name,
					dai_link->codecs->dai_name);
	if (ret < 0)
		goto dai_link_of_err;

	dai_link->ops = &aml_card_ops;
	dai_link->init = aml_card_dai_init;

	dev_dbg(dev, "\tname : %s\n", dai_link->stream_name);
	dev_dbg(dev, "\tformat : %04x\n", dai_link->dai_fmt);
	dev_dbg(dev, "\tcpu : %s / %d\n",
		dai_link->cpu_dai_name,
		dai_props->cpu_dai.sysclk);
	dev_dbg(dev, "\tcodec : %s / %d\n",
		dai_link->codecs->dai_name,
		dai_props->codec_dai.sysclk);

	aml_card_canonicalize_cpu(dai_link, single_cpu);

dai_link_of_err:
	of_node_put(cpu);
	of_node_put(codec);

	return ret;
}

static int aml_card_parse_aux_devs(struct device_node *node,
					   struct aml_card_data *priv)
{
	struct device *dev = aml_priv_to_dev(priv);
	struct device_node *aux_node;
	int i, n, len;

	if (!of_find_property(node, PREFIX "aux-devs", &len))
		return 0;		/* Ok to have no aux-devs */

	n = len / sizeof(__be32);
	if (n <= 0)
		return -EINVAL;

	priv->snd_card.aux_dev = devm_kzalloc(dev,
			n * sizeof(*priv->snd_card.aux_dev), GFP_KERNEL);
	if (!priv->snd_card.aux_dev)
		return -ENOMEM;

	for (i = 0; i < n; i++) {
		aux_node = of_parse_phandle(node, PREFIX "aux-devs", i);
		if (!aux_node)
			return -EINVAL;
		priv->snd_card.aux_dev[i].codec_of_node = aux_node;
	}

	priv->snd_card.num_aux_devs = n;
	return 0;
}

static int spk_mute_set(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *soc_card = snd_kcontrol_chip(kcontrol);
	struct aml_card_data *priv = aml_card_to_priv(soc_card);
	int gpio = priv->spk_mute_gpio;
	bool active_low = priv->spk_mute_active_low;
	bool mute = ucontrol->value.integer.value[0];

	if (gpio_is_valid(gpio)) {
		bool value = active_low ? !mute : mute;

		gpio_set_value(gpio, value);
		pr_info("spk_mute_set: mute flag = %d\n", mute);
	}

	return 0;
}

static int spk_mute_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *soc_card = snd_kcontrol_chip(kcontrol);
	struct aml_card_data *priv = aml_card_to_priv(soc_card);
	int gpio = priv->spk_mute_gpio;
	bool active_low = priv->spk_mute_active_low;

	if (gpio_is_valid(gpio)) {
		bool value = gpio_get_value(gpio);
		bool mute = active_low ? !value : value;

		ucontrol->value.integer.value[0] = mute;
	}

	return 0;
}

static const struct snd_kcontrol_new card_controls[] = {
	SOC_SINGLE_BOOL_EXT("SPK mute", 0,
			    spk_mute_get,
			    spk_mute_set),
};

static int aml_card_parse_gpios(struct device_node *node,
					   struct aml_card_data *priv)
{
	struct device *dev = aml_priv_to_dev(priv);
	struct snd_soc_card *soc_card = &priv->snd_card;
	enum of_gpio_flags flags;
	int gpio;
	bool active_low;
	int ret;

	gpio = of_get_named_gpio_flags(node, "spk_mute", 0, &flags);
	priv->spk_mute_gpio = gpio;

	if (gpio_is_valid(gpio)) {
		active_low = !!(flags & OF_GPIO_ACTIVE_LOW);
		flags = active_low ? GPIOF_OUT_INIT_HIGH : GPIOF_OUT_INIT_LOW;
		priv->spk_mute_active_low = active_low;

		ret = devm_gpio_request_one(dev, gpio, flags, "spk_mute");
		if (ret < 0)
			return ret;

		ret = snd_soc_add_card_controls(soc_card, card_controls,
					ARRAY_SIZE(card_controls));
	}

	priv->avout_mute_desc = gpiod_get(dev,
				"avout_mute",
				GPIOD_OUT_HIGH);

	gpiod_direction_output(priv->avout_mute_desc,
		GPIOF_OUT_INIT_HIGH);
	pr_info("set av out GPIOF_OUT_INIT_HIGH!\n");

	return 0;
}

static int aml_card_parse_of(struct device_node *node,
				     struct aml_card_data *priv)
{
	struct device *dev = aml_priv_to_dev(priv);
	struct device_node *dai_link;
	struct device_node *lb_link;
	int ret;

	if (!node)
		return -EINVAL;

	dai_link = of_get_child_by_name(node, PREFIX "dai-link");

	/* The off-codec widgets */
	if (of_property_read_bool(node, PREFIX "widgets")) {
		ret = snd_soc_of_parse_audio_simple_widgets(&priv->snd_card,
					PREFIX "widgets");
		if (ret)
			goto card_parse_end;
	}

	/* DAPM routes */
	if (of_property_read_bool(node, PREFIX "routing")) {
		ret = snd_soc_of_parse_audio_routing(&priv->snd_card,
					PREFIX "routing");
		if (ret)
			goto card_parse_end;
	}

	/* Factor to mclk, used in hw_params() */
	of_property_read_u32(node, PREFIX "mclk-fs", &priv->mclk_fs);

	/* Loopback */
	lb_link = of_parse_phandle(node, PREFIX "loopback", 0);
	if (lb_link) {
		ret = loopback_parse_of(lb_link, &priv->lb_cfg);
		if (ret < 0)
			pr_err("failed parse loopback, ignore it\n");
	}

	/* Single/Muti DAI link(s) & New style of DT node */
	if (dai_link) {
		struct device_node *np = NULL;
		int i = 0;

		for_each_child_of_node(node, np) {
			dev_dbg(dev, "\tlink %d:\n", i);
			ret = aml_card_dai_link_of(np, priv,
							   i, false);
			if (ret < 0) {
				of_node_put(np);
				goto card_parse_end;
			}
			i++;
		}
	} else {
		/* For single DAI link & old style of DT node */
		ret = aml_card_dai_link_of(node, priv, 0, true);
		if (ret < 0)
			goto card_parse_end;
	}

	ret = aml_card_parse_card_name(&priv->snd_card, PREFIX);
	if (ret < 0)
		goto card_parse_end;

	ret = aml_card_parse_aux_devs(node, priv);

card_parse_end:
	of_node_put(dai_link);
	of_node_put(lb_link);

	return ret;
}


static struct aml_chipset_info g12a_chipset_info = {
	.spdif_b        = true,
	.eqdrc_fn       = true,
};

static struct aml_chipset_info tl1_chipset_info = {
	.spdif_b        = true,
};

static const struct of_device_id auge_of_match[] = {
	{
		.compatible = "amlogic, axg-sound-card",
	},
	{
		.compatible = "amlogic, g12a-sound-card",
		.data       = &g12a_chipset_info,
	},
	{
		.compatible = "amlogic, tl1-sound-card",
		.data       = &tl1_chipset_info,
	},
	{},
};
MODULE_DEVICE_TABLE(of, auge_of_match);

static int aml_card_probe(struct platform_device *pdev)
{
	struct aml_card_data *priv;
	struct snd_soc_dai_link *dai_link;
	struct aml_dai_props *dai_props;
	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	int num, ret;

	/* Get the number of DAI links */
	if (np && of_get_child_by_name(np, PREFIX "dai-link"))
		num = of_get_child_count(np);
	else
		num = 1;

	/* Allocate the private data and the DAI link array */
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	dai_props = devm_kzalloc(dev, sizeof(*dai_props) * num, GFP_KERNEL);
	dai_link  = devm_kzalloc(dev, sizeof(*dai_link)  * num, GFP_KERNEL);
	if (!dai_props || !dai_link)
		return -ENOMEM;

	priv->dai_props			= dai_props;
	priv->dai_link			= dai_link;

	/* Init snd_soc_card */
	priv->snd_card.owner		= THIS_MODULE;
	priv->snd_card.dev		= dev;
	priv->snd_card.dai_link		= priv->dai_link;
	priv->snd_card.num_links	= num;

	if (np && of_device_is_available(np)) {

		ret = aml_card_parse_of(np, priv);
		if (ret < 0) {
			if (ret != -EPROBE_DEFER)
				dev_err(dev, "%s, parse error %d\n",
					__func__, ret);
			goto err;
		}

	} else {
		struct aml_card_info *cinfo;

		cinfo = dev->platform_data;
		if (!cinfo) {
			dev_err(dev, "no info for asoc-aml-card\n");
			return -EINVAL;
		}

		if (!cinfo->name ||
		    !cinfo->codec_dai.name ||
		    !cinfo->codec ||
		    !cinfo->platform ||
		    !cinfo->cpu_dai.name) {
			dev_err(dev, "insufficient aml_card_info settings\n");
			return -EINVAL;
		}

		priv->snd_card.name	=
				(cinfo->card) ? cinfo->card : cinfo->name;
		dai_link->name		= cinfo->name;
		dai_link->stream_name	= cinfo->name;
		dai_link->platform_name	= cinfo->platform;
		dai_link->codec_name	= cinfo->codec;
		dai_link->cpu_dai_name	= cinfo->cpu_dai.name;
		dai_link->codec_dai_name = cinfo->codec_dai.name;
		dai_link->dai_fmt	= cinfo->daifmt;
		dai_link->init		= aml_card_dai_init;
		memcpy(&priv->dai_props->cpu_dai, &cinfo->cpu_dai,
					sizeof(priv->dai_props->cpu_dai));
		memcpy(&priv->dai_props->codec_dai, &cinfo->codec_dai,
					sizeof(priv->dai_props->codec_dai));
	}


	priv->chipinfo = (struct aml_chipset_info *)
		of_device_get_match_data(&pdev->dev);

	if (!priv->chipinfo)
		pr_warn_once("check whether to update sound card init data\n");

	snd_soc_card_set_drvdata(&priv->snd_card, priv);

	ret = devm_snd_soc_register_card(&pdev->dev, &priv->snd_card);
	if (ret < 0) {
		dev_err(dev, "failed to register sound card\n");
		goto err;
	}

	/* Add controls */
	ret = aml_card_add_controls(&priv->snd_card);
	if (ret < 0) {
		dev_err(dev, "failed to register mixer kcontrols\n");
		goto err;
	}

	if (priv->chipinfo && priv->chipinfo->eqdrc_fn) {
		pr_info("eq/drc v1 function enable\n");
		ret = card_add_effects_init(&priv->snd_card);
		if (ret < 0)
			pr_warn_once("Failed to add audio effects v1 controls\n");
	}

	if (priv->hp_det_enable == 1 || priv->mic_det_enable == 1) {
		audio_jack_detect(priv);
		audio_extcon_register(priv, dev);
	}
	ret = aml_card_parse_gpios(np, priv);
	if (ret >= 0)
		return ret;
err:
	pr_err("%s error ret:%d\n", __func__, ret);
	aml_card_clean_reference(&priv->snd_card);

	return ret;
}

static int aml_card_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct aml_card_data *priv = snd_soc_card_get_drvdata(card);

	aml_card_remove_jack(&priv->hp_jack);
	aml_card_remove_jack(&priv->mic_jack);
	jack_audio_stop_timer(priv);
	return aml_card_clean_reference(card);
}

static struct platform_driver aml_card = {
	.driver = {
		.name = "asoc-aml-card",
		.pm = &snd_soc_pm_ops,
		.of_match_table = auge_of_match,
	},
	.probe = aml_card_probe,
	.remove = aml_card_remove,
};

module_platform_driver(aml_card);

MODULE_ALIAS("platform:asoc-aml-card");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("ASoC aml Sound Card");
MODULE_AUTHOR("AMLogic, Inc.");
