/*
 * sound/soc/amlogic/aml_meson/tv.h
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

#ifndef AML_TV_H
#define AML_TV_H

#include <sound/soc.h>
#include <linux/gpio/consumer.h>

#define AML_I2C_BUS_AO 0
#define AML_I2C_BUS_A 1
#define AML_I2C_BUS_B 2
#define AML_I2C_BUS_C 3
#define AML_I2C_BUS_D 4

struct aml_card_info {
	/* tv chipset*/
	struct aml_chipset_info chipset_info;
	/*init, such as EQ, DRC, controls, parse channel mask */
	int (*chipset_init)(struct snd_soc_card *card);
	int (*set_audin_source)(int audin_src);
	int (*set_resample_param)(int index);
};

struct aml_audio_private_data {
	int clock_en;
	bool suspended;
	void *data;

	int hp_last_state;
	bool hp_det_status;
	int av_hs_switch;
	int hp_det_inv;
	int timer_en;
	int detect_flag;
	struct work_struct work;
	struct mutex lock;
	struct gpio_desc *hp_det_desc;

	struct pinctrl *pin_ctl;
	struct timer_list timer;
	struct gpio_desc *av_mute_desc;
	int av_mute_inv;
	struct gpio_desc *amp_mute_desc;
	int amp_mute_inv;
	struct clk *clk;
	int sleep_time;
	struct work_struct init_work;
	int aml_EQ_enable;
	int aml_DRC_enable;

	/* snd card */
	struct snd_card card;

	/* tv info */
	struct aml_card_info *cardinfo;
#ifdef CONFIG_AMLOGIC_AO_CEC
	int arc_enable;
#endif
	int Hardware_resample_enable;
	int spdif_sample_rate_index;
	int hdmi_sample_rate_index;
	int Speaker0_Channel_Mask;
	int Speaker1_Channel_Mask;
	int Speaker2_Channel_Mask;
	int Speaker3_Channel_Mask;
	int EQ_DRC_Channel_Mask;
	int DAC0_Channel_Mask;
	int DAC1_Channel_Mask;
	int Spdif_samesource_Channel_Mask;
	int audio_in_GPIO;
	int audio_in_GPIO_inv;
	struct gpio_desc *source_switch;
};

struct codec_info {
	char name[I2C_NAME_SIZE];
	char name_bus[I2C_NAME_SIZE];
};

#ifdef CONFIG_AMLOGIC_AMAUDIO2
extern int External_Mute(int mute_flag);
#else
int External_Mute(int mute_flag) { return 0; }
#endif
#endif
