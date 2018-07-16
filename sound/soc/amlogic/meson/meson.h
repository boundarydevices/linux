/*
 * sound/soc/amlogic/meson/meson.h
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

#ifndef AML_MESON_H
#define AML_MESON_H

#include <sound/soc.h>
#include <linux/gpio/consumer.h>
struct aml_audio_private_data {
#if 0

	const char *name;
	const char *card;
	const char *codec;
	const char *platform;

	unsigned int daifmt;
	struct aml_audio_dai *cpu_dai;
	struct aml_audio_dai *codec_dai;

	struct snd_soc_dai_link *snd_link;
	int num_links;
	struct snd_soc_card snd_card;
#endif
	int bias_level;
	int clock_en;
	int gpio_hp_det;
	bool det_pol_inv;
	int sleep_time;
	int gpio_mute;
	int gpio_i2s_m;
	int gpio_i2s_s;
	int gpio_i2s_r;
	int gpio_i2s_o;
	bool mute_inv;
	struct pinctrl *pin_ctl;
	int hp_last_state;
	bool hp_det_status;
	unsigned int hp_val_h;
	unsigned int hp_val_l;
	unsigned int mic_val;
	unsigned int hp_detal;
	unsigned int hp_adc_ch;
	bool suspended;
	bool mic_det;
	bool hp_disable;
	int timer_en;
	int detect_flag;
	struct timer_list timer;
	struct work_struct work;
	struct mutex lock;
	struct snd_soc_jack jack;
	void *data;
	struct gpio_desc *mute_desc;
#if 0 /*tmp_mask_for_kernel_4_4*/
	struct switch_dev sdev; /* for android */
	struct switch_dev mic_sdev; /* for android */
#endif
	struct work_struct pinmux_work;
	unsigned char *area;
	dma_addr_t addr;
};

#endif
