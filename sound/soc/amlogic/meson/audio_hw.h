/*
 * sound/soc/amlogic/meson/audio_hw.h
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

#ifndef __AML_AUDIO_HW_H__
#define __AML_AUDIO_HW_H__

#include "sound/asound.h"
#include <linux/amlogic/media/sound/spdif_info.h>

#define AUDIO_CLK_GATE_ON(a) CLK_GATE_ON(a)
#define AUDIO_CLK_GATE_OFF(a) CLK_GATE_OFF(a)

struct _aiu_clk_setting_t {
	unsigned short pll;
	unsigned short mux;
	unsigned short devisor;
};

struct audio_output_config_t {
	/* audio clock */
	unsigned short clock;
	/* analog output */
	unsigned short i2s_mode;
	unsigned short i2s_dac_mode;
	unsigned short i2s_preemphsis;
	/* digital output */
	unsigned short i958_buf_start_addr;
	unsigned short i958_buf_blksize;
	unsigned short i958_int_flag;
	unsigned short i958_mode;
	unsigned short i958_sync_mode;
	unsigned short i958_preemphsis;
	unsigned short i958_copyright;
	unsigned short bpf;
	unsigned short brst;
	unsigned short length;
	unsigned short paddsize;
	struct iec958_chsts chan_status;
};

struct _aiu_958_raw_setting_t {
	unsigned short int_flag;
	unsigned short bpf;
	unsigned short brst;
	unsigned short length;
	unsigned short paddsize;
	struct iec958_chsts *chan_stat;
};

struct aml_chipset_info {
	/* is tv chipset */
	bool is_tv_chipset;
	/* audin clk support */
	bool audin_clk_support;
	/* audin(i2s in) for external codec */
	bool audin_ext_support;
	/* audin (i2s in) check l/r invert */
	bool audin_lr_invert;
	/* aud_buf gate is removed*/
	bool audbuf_gate_rm;
	/* split mode supoort completely */
	bool split_complete;
	/* spdif pao mode */
	bool spdif_pao;
	/* spdif in 88k/176k*/
	bool spdifin_more_rate;
	/* cpudai fmt polarity */
	unsigned int fmt_polarity;
};

enum {
	I2SIN_MASTER_MODE = 0,
	I2SIN_SLAVE_MODE = 1 << 0,
	SPDIFIN_MODE = 1 << 1,
};
enum {
	AML_AUDIO_NA = 0,
	AML_AUDIO_SPDIFIN = 1 << 0,
	AML_AUDIO_SPDIFOUT = 1 << 1,
	AML_AUDIO_I2SIN = 1 << 2,
	AML_AUDIO_I2SOUT = 1 << 3,
	AML_AUDIO_PCMIN = 1 << 4,
	AML_AUDIO_PCMOUT = 1 << 5,
	AML_AUDIO_I2SIN2 = 1 << 6,
};

#define AUDIO_CLK_256FS             0
#define AUDIO_CLK_384FS             1

#define AUDIO_CLK_FREQ_192  0
#define AUDIO_CLK_FREQ_1764 1
#define AUDIO_CLK_FREQ_96   2
#define AUDIO_CLK_FREQ_882  3
#define AUDIO_CLK_FREQ_48   4
#define AUDIO_CLK_FREQ_441  5
#define AUDIO_CLK_FREQ_32   6

#define AUDIO_CLK_FREQ_8		7
#define AUDIO_CLK_FREQ_11		8
#define AUDIO_CLK_FREQ_12		9
#define AUDIO_CLK_FREQ_16		10
#define AUDIO_CLK_FREQ_22		11
#define AUDIO_CLK_FREQ_24		12

#define AIU_958_MODE_RAW    0
#define AIU_958_MODE_PCM16  1
#define AIU_958_MODE_PCM24  2
#define AIU_958_MODE_PCM32  3
#define AIU_958_MODE_PCM_RAW  4

#define AIU_I2S_MODE_PCM16   0
#define AIU_I2S_MODE_PCM24   2
#define AIU_I2S_MODE_PCM32   3

#define AUDIO_ALGOUT_DAC_FORMAT_DSP             0
#define AUDIO_ALGOUT_DAC_FORMAT_LEFT_JUSTIFY    1

#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE
#define DEFAULT_PLAYBACK_SIZE      256
#else
#define DEFAULT_PLAYBACK_SIZE      64
#endif

extern unsigned int IEC958_MODE;
extern unsigned int I2S_MODE;
extern unsigned int audio_in_source;
extern unsigned int bSpdifIN_PAO;

void audio_set_aiubuf(u32 addr, u32 size, unsigned int channel,
	snd_pcm_format_t format);
void audio_set_958outbuf(u32 addr, u32 size, int flag);
void audio_in_i2s_set_buf(u32 addr, u32 size, u32 i2s_mode,
	u32 i2s_sync, u32 din_sel, u32 ch, snd_pcm_format_t format);
void audio_in_spdif_set_buf(u32 addr, u32 size, u32 src);
void audio_in_i2s2_set_buf(u32 addr, u32 size, u32 src, u32 ch);
void audio_in_i2s_enable(int flag);
void audio_in_spdif_enable(int flag);
void audio_in_i2s2_enable(int flag);
unsigned int audio_in_i2s_rd_ptr(void);
unsigned int audio_in_i2s_wr_ptr(void);
unsigned int audio_in_i2s2_wr_ptr(void);
unsigned int audio_in_spdif_wr_ptr(void);
#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE
void audio_set_i2s_mode(u32 mode, unsigned int channel);
#else
void audio_set_i2s_mode(u32 mode);
#endif
void audio_set_i2s_clk_div(void);
void audio_set_spdif_clk_div(uint div);
void audio_enable_output(int flag);
unsigned int read_i2s_rd_ptr(void);
void audio_i2s_unmute(void);
void audio_i2s_mute(void);
void aml_audio_i2s_unmute(void);
void aml_audio_i2s_mute(void);
void audio_util_set_i2s_format(unsigned int format);
void audio_util_set_dac_958_format(unsigned int format);
void audio_set_958_mode(
	unsigned int mode,
	struct _aiu_958_raw_setting_t *set);
unsigned int read_i2s_mute_swap_reg(void);
void audio_i2s_swap_left_right(unsigned int flag);
int if_audio_out_enable(void);
int if_audio_in_i2s_enable(void);
int if_audio_in_spdif_enable(void);
void audio_out_i2s_enable(unsigned int flag);
void audio_hw_958_enable(unsigned int flag);
void audio_out_enabled(int flag);
unsigned int audio_hdmi_init_ready(void);
unsigned int read_iec958_rd_ptr(void);
void audio_in_spdif_enable(int flag);
unsigned int audio_spdifout_pg_enable(unsigned char enable);
unsigned int audio_aiu_pg_enable(unsigned char enable);
void audio_mute_left_right(unsigned int flag);
void audio_i2s_958_same_source(unsigned int same);
void set_hdmi_tx_clk_source(int source);

extern void audio_in_clk_sel(void);

extern void aml_chipset_update_info(struct aml_chipset_info *chipset_info);
extern bool is_meson_tv_chipset(void);
extern bool is_audin_ext_support(void);
extern bool is_audin_lr_invert_check(void);
extern bool is_audbuf_gate_rm(void);
extern void chipset_set_spdif_pao(void);

extern unsigned int IEC958_mode_codec;
extern unsigned int clk81;

/* OVERCLOCK == 1, our SOC privide 512fs mclk;
 * DOWNCLOCK == 1, 128fs;
 * normal mclk : 256fs
 */
#define OVERCLOCK 0
#define DOWNCLOCK 0

#define IEC958_OVERCLOCK 1

#if (OVERCLOCK == 1)
#define MCLKFS_RATIO 512
#elif (DOWNCLOCK == 1)
#define MCLKFS_RATIO 128
#else
#define MCLKFS_RATIO 256
#endif

#define DEFAULT_SAMPLERATE 48000
#define DEFAULT_MCLK_RATIO_SR MCLKFS_RATIO

#define I2S_PLL_SRC         1	/* MPLL0 */
#define MPLL_I2S_CNTL		HHI_MPLL_MP0

#define I958_PLL_SRC        2	/* MPLL1 */
#define MPLL_958_CNTL		HHI_MPLL_MP1

#endif
