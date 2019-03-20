/*
 * sound/soc/amlogic/auge/pdm.h
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

#ifndef __AML_PDM_H__
#define __AML_PDM_H__

#include <linux/clk.h>
#include <linux/pinctrl/consumer.h>


#define DRV_NAME "snd_pdm"

#define DEFAULT_FS_RATIO		256

#define PDM_CHANNELS_MIN		1
/* 8ch pdm in, 8 ch tdmin_lb */
#define PDM_CHANNELS_LB_MAX		(PDM_CHANNELS_MAX + 8)


#define PDM_RATES			(SNDRV_PCM_RATE_96000 |\
					SNDRV_PCM_RATE_64000 |\
					SNDRV_PCM_RATE_48000 |\
					SNDRV_PCM_RATE_32000 |\
					SNDRV_PCM_RATE_16000 |\
					SNDRV_PCM_RATE_8000)

#define PDM_FORMATS			(SNDRV_PCM_FMTBIT_S16_LE |\
					SNDRV_PCM_FMTBIT_S24_LE |\
					SNDRV_PCM_FMTBIT_S32_LE)

enum {
	PDM_RUN_MUTE_VAL = 0,
	PDM_RUN_MUTE_CHMASK,

	PDM_RUN_MAX,
};

struct pdm_chipinfo {
	/* pdm supports mute function */
	bool mute_fn;
	/* truncate invalid data when filter init */
	bool truncate_data;
	/* train */
	bool train;
};

struct aml_pdm {
	struct device *dev;
	struct aml_audio_controller *actrl;
	struct pinctrl *pdm_pins;
	struct clk *clk_gate;
	/* sel: fclk_div3(666M) */
	struct clk *sysclk_srcpll;
	/* consider same source with tdm, 48k(24576000) */
	struct clk *dclk_srcpll;
	struct clk *clk_pdm_sysclk;
	struct clk *clk_pdm_dclk;
	struct toddr *tddr;
	/*
	 * filter mode:0~4,
	 * from mode 0 to 4, the performance is from high to low,
	 * the group delay (latency) is from high to low.
	 */
	int filter_mode;
	/* dclk index */
	int dclk_idx;
	/* PCM or Raw Data */
	int bypass;

	/* lane mask in, each lane carries two channels */
	int lane_mask_in;

	/* PDM clk on/off, only clk on, pdm registers can be accessed */
	bool clk_on;

	/* train */
	bool train_en;

	struct pdm_chipinfo *chipinfo;
	struct snd_kcontrol *controls[PDM_RUN_MAX];
};

#endif /*__AML_PDM_H__*/
