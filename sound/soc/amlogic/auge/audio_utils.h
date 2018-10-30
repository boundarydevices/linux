/*
 * sound/soc/amlogic/auge/audio_utils.h
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

#ifndef __AML_AUDIO_UTILS_H__
#define __AML_AUDIO_UTILS_H__

#include <linux/clk.h>
#include <linux/types.h>
#include <sound/soc.h>
#include <sound/tlv.h>

/* datain src
 * [4]: pdmin;
 * [3]: spdifin;
 * [2]: tdmin_c;
 * [1]: tdmin_b;
 * [0]: tdmin_a;
 */
enum datain_src {
	DATAIN_TDMA = 0,
	DATAIN_TDMB,
	DATAIN_TDMC,
	DATAIN_SPDIF,
	DATAIN_PDM,
};

/* datalb src/tdmlb src
 * [0]: tdmoutA
 * [1]: tdmoutB
 * [2]: tdmoutC
 * [3]: PAD_tdminA
 * [4]: PAD_tdminB
 * [5]: PAD_tdminC
 */
enum tdmin_lb_src {
	TDMINLB_TDMOUTA = 0,
	TDMINLB_TDMOUTB,
	TDMINLB_TDMOUTC,
	TDMINLB_PAD_TDMINA,
	TDMINLB_PAD_TDMINB,
	TDMINLB_PAD_TDMINC,
};

/* toddr_src_sel
 *		[7]: loopback;
 *		[6]: tdmin_lb;
 *		[5]: reserved;
 *		[4]: pdmin;
 *		[3]: spdifin;
 *		[2]: tdmin_c;
 *		[1]: tdmin_b;
 *		[0]: tdmin_a;
 */
enum fifoin_src {
	FIFOIN_TDMINA   = 0,
	FIFOIN_TDMINB   = 1,
	FIFOIN_TDMINC   = 2,
	FIFOIN_SPDIF    = 3,
	FIFOIN_PDM      = 4,
	FIFOIN_TDMINLB  = 6,
	FIFOIN_LOOPBACK = 7
};

/* audio data selected to ddr */
struct audio_data {
	unsigned int resample;
	/* reg_dat_sel
	 *      0: combined data[m:n] without gap;
	 *              like S0[m:n],S1[m:n],S2[m:n],
	 *      1: combined data[m:n] as 16bits;
	 *              like {S0[11:0],4'd0},{S1[11:0],4'd0}
	 *      2: combined data[m:n] as 16bits;
	 *              like {4'd0,S0[11:0]},{4'd0,{S1[11:0]}
	 *      3: combined data[m:n] as 32bits;
	 *              like {S0[27:4],8'd0},{S1[27:4],8'd0}
	 *      4: combined data[m:n] as 32bits;
	 *              like {8'd0,S0[27:4]},{8'd0,{S1[27:4]}
	 */
	unsigned int combined_type;
	/* the msb positioin in data */
	unsigned int msb;
	/* the lsb position in data */
	unsigned int lsb;
	/* toddr_src_sel
	 *      [7]: loopback;
	 *      [6]: tdmin_lb;
	 *      [5]: reserved;
	 *      [4]: pdmin;
	 *      [3]: spdifin;
	 *      [2]: tdmin_c;
	 *      [1]: tdmin_b;
	 *      [0]: tdmin_a;
	 */
	unsigned int src;
};

/**/
struct loopback_cfg {
	struct clk *tdmin_mpll;
	/* lb_mode
	 * 0: out rate = in data rate;
	 * 1: out rate = loopback data rate;
	 */
	unsigned int lb_mode;

	enum datain_src datain_src;
	unsigned int datain_chnum;
	unsigned int datain_chmask;
	int toddr_index;

	enum tdmin_lb_src datalb_src;
	enum tdmin_lb_src datalb_clk;
	unsigned int datalb_chnum;
	unsigned int datalb_chswap;
	unsigned int datalb_chmask;
	int frddr_index;
	unsigned int datain_datalb_total;
};

extern void loopback_set_status(int is_running);

extern int loopback_is_enable(void);

extern int loopback_check_enable(int src);

extern int snd_card_add_kcontrols(struct snd_soc_card *card);

extern int loopback_parse_of(struct device_node *node,
				struct loopback_cfg *lb_cfg);

extern int loopback_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct loopback_cfg *lb_cfg,
				unsigned int mclk);

extern int loopback_prepare(
	struct snd_pcm_substream *substream,
	struct loopback_cfg *lb_cfg);

extern int loopback_trigger(
	struct snd_pcm_substream *substream,
	int cmd,
	struct loopback_cfg *lb_cfg);

extern void audio_locker_set(int enable);

extern int audio_locker_get(void);

extern void fratv_enable(bool enable);
extern void fratv_src_select(int src);

extern void cec_arc_enable(int src, bool enable);
#endif
