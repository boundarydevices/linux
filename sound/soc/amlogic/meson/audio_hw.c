/*
 * sound/soc/amlogic/meson/audio_hw.c
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
#undef pr_fmt
#define pr_fmt(fmt) "audio_hw: " fmt

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/clk.h>
#include <linux/module.h>

/* Amlogic headers */
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/media/sound/aiu_regs.h>
#include <linux/amlogic/media/sound/audin_regs.h>
#include <linux/amlogic/media/sound/audio_iomap.h>
#include "audio_hw.h"

/* i2s mode 0: master 1: slave */
/* source: 0: linein; 1: ATV; 2: HDMI-in */
unsigned int IEC958_MODE = AIU_958_MODE_PCM16;
unsigned int I2S_MODE = AIU_I2S_MODE_PCM16;
unsigned int audio_in_source;

unsigned int IEC958_bpf = 0x7dd;
EXPORT_SYMBOL(IEC958_bpf);
unsigned int IEC958_brst = 0xc;
EXPORT_SYMBOL(IEC958_brst);
unsigned int IEC958_length = 0x7dd * 8;
EXPORT_SYMBOL(IEC958_length);
unsigned int IEC958_padsize = 0x8000;
EXPORT_SYMBOL(IEC958_padsize);
unsigned int IEC958_mode = 1;
EXPORT_SYMBOL(IEC958_mode);
unsigned int IEC958_syncword1 = 0x7ffe;
EXPORT_SYMBOL(IEC958_syncword1);
unsigned int IEC958_syncword2 = 0x8001;
EXPORT_SYMBOL(IEC958_syncword2);
unsigned int IEC958_syncword3;
EXPORT_SYMBOL(IEC958_syncword3);
unsigned int IEC958_syncword1_mask;
EXPORT_SYMBOL(IEC958_syncword1_mask);
unsigned int IEC958_syncword2_mask;
EXPORT_SYMBOL(IEC958_syncword2_mask);
unsigned int IEC958_syncword3_mask = 0xffff;
EXPORT_SYMBOL(IEC958_syncword3_mask);
unsigned int IEC958_chstat0_l = 0x1902;
EXPORT_SYMBOL(IEC958_chstat0_l);
unsigned int IEC958_chstat0_r = 0x1902;
EXPORT_SYMBOL(IEC958_chstat0_r);
unsigned int IEC958_chstat1_l = 0x200;
EXPORT_SYMBOL(IEC958_chstat1_l);
unsigned int IEC958_chstat1_r = 0x200;
EXPORT_SYMBOL(IEC958_chstat1_r);
unsigned int IEC958_mode_raw;
EXPORT_SYMBOL(IEC958_mode_raw);

/*
 * bit 0:soc in slave mode for adc;
 * bit 1:audio in data source from spdif in;
 * bit 2:adc & spdif in work at the same time;
 */
unsigned int audioin_mode = I2SIN_MASTER_MODE;

/* Bit 3:  mute constant
 * 0 => 'h0000000
 * 1 => 'h800000
 */
unsigned int dac_mute_const = 0x800000;

/*
 * chipset info
 */
static struct aml_chipset_info aml_chipinfo;

void aml_chipset_update_info(struct aml_chipset_info *chipinfo)
{
	if (!chipinfo)
		return;

	aml_chipinfo.is_tv_chipset = chipinfo->is_tv_chipset;
	aml_chipinfo.audin_clk_support = chipinfo->audin_clk_support;
	aml_chipinfo.audin_ext_support = chipinfo->audin_ext_support;
	aml_chipinfo.audin_lr_invert = chipinfo->audin_lr_invert;
	aml_chipinfo.audbuf_gate_rm  = chipinfo->audbuf_gate_rm;
	aml_chipinfo.split_complete = chipinfo->split_complete;
	aml_chipinfo.spdif_pao  = chipinfo->spdif_pao;
	aml_chipinfo.spdifin_more_rate = chipinfo->spdifin_more_rate;
	aml_chipinfo.fmt_polarity = chipinfo->fmt_polarity;
}

bool is_meson_tv_chipset(void)
{
	return aml_chipinfo.is_tv_chipset;
}

static bool is_audin_clk_support(void)
{
	return aml_chipinfo.audin_clk_support;
}

/* audin supports for exteranl codec */
bool is_audin_ext_support(void)
{
	return aml_chipinfo.audin_ext_support;
}

bool is_audin_lr_invert_check(void)
{
	return aml_chipinfo.audin_lr_invert;
}

bool is_audbuf_gate_rm(void)
{
	return aml_chipinfo.audbuf_gate_rm;
}

#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE
static bool is_split_fully_support(void)
{
	return aml_chipinfo.split_complete;
}
#endif

static bool is_spdif_pao_support(void)
{
	return aml_chipinfo.spdif_pao;
}

static bool is_spdif_in_more_rate_support(void)
{
	return aml_chipinfo.spdifin_more_rate;
}

void chipset_set_spdif_pao(void)
{
	aml_audin_update_bits(AUDIN_HDMIRX_PAO_CTRL,
		0x1 << 1,
		0x1 << 1);
}

void audio_set_aiubuf(u32 addr, u32 size, unsigned int channel,
	snd_pcm_format_t format)
{
#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE
	aml_aiu_write(AIU_MEM_I2S_START_PTR, addr & 0xffffff00);
	aml_aiu_write(AIU_MEM_I2S_RD_PTR, addr & 0xffffff00);
#else
	aml_aiu_write(AIU_MEM_I2S_START_PTR, addr & 0xffffffc0);
	aml_aiu_write(AIU_MEM_I2S_RD_PTR, addr & 0xffffffc0);
#endif

	if (channel == 8) {
		/*select cts_aoclkx2_int as AIU clk to hdmi_tx_audio_mster_clk*/
		aml_aiu_update_bits(AIU_CLK_CTRL_MORE, 1 << 6, 1 << 6);
		/*unmute all channels*/
		aml_aiu_update_bits(AIU_I2S_MUTE_SWAP, 0xff << 8, 0 << 8);
#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE
		aml_aiu_write(AIU_MEM_I2S_END_PTR,
			(addr & 0xffffff00) + (size & 0xffffff00) - 256);
#else
		aml_aiu_write(AIU_MEM_I2S_END_PTR,
			(addr & 0xffffffc0) + (size & 0xffffffc0) - 256);
#endif
	} else {
		/*select cts_clk_i958 as AIU clk to hdmi_tx_audio_mster_clk*/
		aml_aiu_update_bits(AIU_CLK_CTRL_MORE, 1 << 6, 0 << 6);
		/*unmute 0/1 channel*/
		aml_aiu_update_bits(AIU_I2S_MUTE_SWAP, 0xff << 8, 0xfc << 8);
#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE
		aml_aiu_write(AIU_MEM_I2S_END_PTR,
			(addr & 0xffffff00) + (size & 0xffffff00) - 256);
#else
		aml_aiu_write(AIU_MEM_I2S_END_PTR,
			(addr & 0xffffffc0) + (size & 0xffffffc0) - 64);
#endif
	}
	/* Hold I2S */
	aml_aiu_update_bits(AIU_I2S_MISC, 0x1 << 2,
				1 << 2);
	/* force audio data to left or right */
	aml_aiu_update_bits(AIU_I2S_MISC, 0x1 << 4,
				1 << 4);
	/* Release hold */
	aml_aiu_update_bits(AIU_I2S_MISC, 0x1 << 2,
				0 << 2);

	if (channel == 8) {
		pr_info("%s channel == 8\n", __func__);
		/* [31:16] IRQ block. */
		aml_aiu_write(AIU_MEM_I2S_MASKS, (24 << 16) |
		/* [15: 8] chan_mem_mask.
		 *  Each bit indicates which channels exist in memory
		 */
				   (0xff << 8) |
		/* [ 7: 0] chan_rd_mask.
		 *  Each bit indicates which channels are READ from memory
		 */
				   (0xff << 0));
	} else {
#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE
		/* [31:16] IRQ block. */
		aml_aiu_write(AIU_MEM_I2S_MASKS, (24 << 16) |
					(0xff << 8) |
					(0xff << 0));
#else
		/* [31:16] IRQ block. */
		aml_aiu_write(AIU_MEM_I2S_MASKS, (24 << 16) |
				   (0x3 << 8) |
				   (0x3 << 0));
#endif
	}
	/* 16 bit PCM mode */
	/* aml_aiu_update_bits(AIU_MEM_I2S_CONTROL, 1, 6, 1); */
	/* Set init high then low to initialize the I2S memory logic */
	aml_aiu_update_bits(AIU_MEM_I2S_CONTROL, 1, 1);
	aml_aiu_update_bits(AIU_MEM_I2S_CONTROL, 1, 0);

	aml_aiu_write(AIU_MEM_I2S_BUF_CNTL, 1 | (0 << 1));
	aml_aiu_write(AIU_MEM_I2S_BUF_CNTL, 0 | (0 << 1));

}

void audio_set_958outbuf(u32 addr, u32 size, int flag)
{
#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE
	aml_aiu_write(AIU_MEM_IEC958_START_PTR, addr & 0xffffffff);
	aml_aiu_write(AIU_MEM_IEC958_RD_PTR, addr & 0xffffffff);
#else
	aml_aiu_write(AIU_MEM_IEC958_START_PTR, addr & 0xffffffc0);
	aml_aiu_write(AIU_MEM_IEC958_RD_PTR, addr & 0xffffffc0);
#endif
	if (flag == 0) {
		/* this is for 16bit 2 channel */
#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE
		aml_aiu_write(AIU_MEM_IEC958_END_PTR,
			       (addr & 0xffffffff) +
			       (size & 0xffffffff) - 1);
#else
		aml_aiu_write(AIU_MEM_IEC958_END_PTR,
			       (addr & 0xffffffc0) +
			       (size & 0xffffffc0) - 64);
#endif
	} else {
		/* this is for RAW mode */
#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE
		aml_aiu_write(AIU_MEM_IEC958_END_PTR,
					(addr & 0xffffffff) +
					(size & 0xffffffff) - 1);
#else
		aml_aiu_write(AIU_MEM_IEC958_END_PTR,
			       (addr & 0xffffffc0) +
			       (size & 0xffffffc0) - 1);
#endif
	}
#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE
	aml_aiu_update_bits(AIU_MEM_IEC958_MASKS, 0xffff, 0xffff);
#else
	aml_aiu_update_bits(AIU_MEM_IEC958_MASKS, 0xffff, 0x303);
#endif
	aml_aiu_update_bits(AIU_MEM_IEC958_CONTROL, 1, 1);
	aml_aiu_update_bits(AIU_MEM_IEC958_CONTROL, 1, 0);

	aml_aiu_write(AIU_MEM_IEC958_BUF_CNTL, 1 | (0 << 1));
	aml_aiu_write(AIU_MEM_IEC958_BUF_CNTL, 0 | (0 << 1));
}

/* audin clk select
 * now support chipset: txlx, txhd
 */
void audio_in_clk_sel(void)
{
	if (is_audin_clk_support())
		aml_audin_update_bits(AUDIN_SOURCE_SEL, 1 << 2,
					1 << 2);
}

/*
 * i2s mode 0: master 1: slave
 * din_sel 0:spdif 1:i2s 2:pcm 3: dmic
 */
static void i2sin_fifo0_set_buf(u32 addr, u32 size, u32 i2s_mode,
		u32 i2s_sync, u32 din_sel, u32 ch, snd_pcm_format_t format)
{
	unsigned char mode = 0;
	unsigned int sync_mode = 0, din_pos = 0;
#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE_MMAP
	unsigned int fifo_bytes = 0;
	unsigned int i2s_size = 0;
#endif
	if (i2s_sync)
		sync_mode = i2s_sync;
	if (i2s_mode & I2SIN_SLAVE_MODE)
		mode = 1;
#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE_MMAP
	if (format == SNDRV_PCM_FORMAT_S16_LE) {
		fifo_bytes = 1;
		i2s_size = 0;
	} else {
		fifo_bytes = 2;
		i2s_size = 3;
	}
#endif
	if (din_sel != 1)
		din_pos = 1;
	aml_audin_write(AUDIN_FIFO0_START, addr & 0xffffffc0);
	aml_audin_write(AUDIN_FIFO0_PTR, (addr & 0xffffffc0));
	aml_audin_write(AUDIN_FIFO0_END,
			   (addr & 0xffffffc0) + (size & 0xffffffc0) - 8);

	aml_audin_write(AUDIN_FIFO0_CTRL, (1 << AUDIN_FIFO_EN)	/* FIFO_EN */
			   | (1 << AUDIN_FIFO_LOAD) /* load start address */
			   | (din_sel << AUDIN_FIFO_DIN_SEL) /*DIN from i2sin*/
			   | (4 << AUDIN_FIFO_ENDIAN) /*AUDIN_FIFO_ENDIAN*/
#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE_MMAP
			   | (1 << AUDIN_FIFO_CHAN) /* ch mode ctl */
#else
			   | ((ch == 2?2:1) << AUDIN_FIFO_CHAN) /*ch mode ctl*/
#endif
			   | (1 << AUDIN_FIFO_UG) /* Urgent request. */
	);
#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE_MMAP
	if (format == SNDRV_PCM_FORMAT_S16_LE)
		aml_write_cbus(AUDIN_FIFO0_CTRL,
				aml_read_cbus(AUDIN_FIFO0_CTRL) |
				(7 << AUDIN_FIFO_ENDIAN));
	else
		aml_write_cbus(AUDIN_FIFO0_CTRL,
				aml_read_cbus(AUDIN_FIFO0_CTRL) |
				(6 << AUDIN_FIFO_ENDIAN));
#endif
#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE_MMAP
	aml_write_cbus(AUDIN_FIFO0_CTRL1, 0 << 4	/* fifo0_dest_sel */
			   | fifo_bytes << 2 /* fifo0_din_byte_num */
			   | 0 << 0);	/* fifo0_din_pos */
#else
	aml_audin_write(AUDIN_FIFO0_CTRL1, 0 << 4	/* fifo0_dest_sel */
			   | 2 << 2	/* 0: 8bit; 1:16bit; 2:32bit */
			   | din_pos << 0);	/* fifo0_din_pos */
#endif
#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE_MMAP
	if (format == SNDRV_PCM_FORMAT_S16_LE)
		aml_write_cbus(AUDIN_FIFO0_CTRL1,
				aml_read_cbus(AUDIN_FIFO0_CTRL1) |
				(0 << 0));
	else
		aml_write_cbus(AUDIN_FIFO0_CTRL1,
				aml_read_cbus(AUDIN_FIFO0_CTRL1) |
				(1 << 0));
#endif
	if (audio_in_source == 1 && (!is_audin_ext_support())) {
		aml_audin_write(AUDIN_I2SIN_CTRL, (1 << I2SIN_CHAN_EN)
				   | (0 << I2SIN_SIZE)
				   | (0 << I2SIN_LRCLK_INVT)
				   | (0 << I2SIN_LRCLK_SKEW)
				   | (1 << I2SIN_POS_SYNC)
				   | (0 << I2SIN_LRCLK_SEL)
				   | (0 << I2SIN_CLK_SEL)
				   | (0 << I2SIN_DIR));
	} else if (audio_in_source == 2 && (!is_audin_ext_support())) {
		aml_audin_write(AUDIN_I2SIN_CTRL, (1 << I2SIN_CHAN_EN)
				   | (3 << I2SIN_SIZE)
				   | (1 << I2SIN_LRCLK_INVT)
				   | (1 << I2SIN_LRCLK_SKEW)
				   | (1 << I2SIN_POS_SYNC)
				   | (1 << I2SIN_LRCLK_SEL)
				   | (1 << I2SIN_CLK_SEL)
				   | (1 << I2SIN_DIR));
	} else {
#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE_MMAP
		aml_write_cbus(AUDIN_I2SIN_CTRL,
				   ((0xf>>(4 - ch/2)) << I2SIN_CHAN_EN)
				   | (i2s_size << I2SIN_SIZE)
				   | (1 << I2SIN_LRCLK_INVT)
				   | (1 << I2SIN_LRCLK_SKEW)
				   | (sync_mode << I2SIN_POS_SYNC)
				   | (!mode << I2SIN_LRCLK_SEL)
				   | (!mode << I2SIN_CLK_SEL)
				   | (!mode << I2SIN_DIR));
#else
		aml_audin_write(AUDIN_I2SIN_CTRL,
				   ((0xf>>(4 - ch/2)) << I2SIN_CHAN_EN)
				   | (3 << I2SIN_SIZE)
				   | (1 << I2SIN_LRCLK_INVT)
				   | (1 << I2SIN_LRCLK_SKEW)
				   | (sync_mode << I2SIN_POS_SYNC)
				   | (!mode << I2SIN_LRCLK_SEL)
				   | (!mode << I2SIN_CLK_SEL)
				   | (!mode << I2SIN_DIR));
#endif
	}

}

static void i2sin_fifo2_set_buf(u32 addr, u32 size, u32 src, u32 ch)
{
	aml_audin_write(AUDIN_FIFO2_START, addr & 0xffffffc0);
	aml_audin_write(AUDIN_FIFO2_PTR, (addr & 0xffffffc0));
	aml_audin_write(AUDIN_FIFO2_END,
			   (addr & 0xffffffc0) + (size & 0xffffffc0) - 8);
	aml_audin_write(AUDIN_FIFO2_CTRL, (1 << AUDIN_FIFO_EN)	/* FIFO0_EN */
			   |(1 << AUDIN_FIFO_LOAD)	/*load start address.*/
			   |(src << AUDIN_FIFO_DIN_SEL)
			   |(4 << AUDIN_FIFO_ENDIAN) /* AUDIN_FIFO0_ENDIAN */
			   |((ch == 2?2:1) << AUDIN_FIFO_CHAN)	/*2 channel*/
			   |(1 << AUDIN_FIFO_UG)	/* Urgent request. */
	);

	/* HDMI I2S-in module */
	aml_audin_write(AUDIN_DECODE_FORMAT,
				(0 << 24) /*spdif enable*/
				|(0 << 16)/*i2s enable*/
				|((ch == 2?0x3:0xff) << 8)/*2ch:0x3; 8ch:0xff*/
				|(1 << 7) /*0:spdif; 1: i2s*/
				|((ch == 2?0:1) << 6)/*0:2ch; 1:8ch*/
				|(2 << 4)/*0:left-justify; 1:right-justify;*/
						 /*2: I2S mode; 3: DSP mode*/
				|(3 << 2)/*0:16bit; 1:18bit; 2:20bit; 3:24bit*/
				|(0 << 1)/*0:left-right; 1:right-left*/
				|(1 << 0)/*0:one bit audio; 2:i2s*/
	);

	if (audio_in_source == 1) {
		/* ATV from adec */
		aml_audin_write(AUDIN_ATV_DEMOD_CTRL, 7);
		aml_audin_update_bits(AUDIN_FIFO2_CTRL,
				 (0x7 << AUDIN_FIFO_DIN_SEL),
				 (ATV_ADEC << AUDIN_FIFO_DIN_SEL));
		aml_audin_write(AUDIN_FIFO2_CTRL1, 0x09);
	} else if (audio_in_source == 2) {
		/* HDMI from PAO */
		aml_audin_update_bits(AUDIN_FIFO2_CTRL,
				 (0x7 << AUDIN_FIFO_DIN_SEL),
				 (PAO_IN << AUDIN_FIFO_DIN_SEL));
		aml_audin_write(AUDIN_FIFO2_CTRL1, 0x08);

		if (is_spdif_pao_support())
			chipset_set_spdif_pao();
	} else if (audio_in_source == 3) {
		/* spdif-in from spdif-pad */
		aml_audin_write(AUDIN_FIFO2_CTRL1, 0x88);
	} else {
		/* AV from acodec */
		aml_audin_write(AUDIN_FIFO2_CTRL1, 0x0c);
	}

}

static void spdifin_reg_set(void)
{
	/* get clk81 clk_rate */
	unsigned int clk_rate = clk81;
	u32 spdif_clk_time = 54;	/* 54us */
	u32 spdif_mode_14bit = (u32)((clk_rate / 500000 + 1) >> 1)
					* spdif_clk_time;
	/* sysclk/32(bit)/2(ch)/2(bmc) */
	u32 period_data = (u32)(clk_rate / 64000 + 1) >> 1;
	u32 period_32k = (period_data + (1 << 4)) >> 5;	/* 32k min period */
	u32 period_44k = (period_data / 22 + 1) >> 1;	/* 44k min period */
	u32 period_48k = (period_data / 24 + 1) >> 1;	/* 48k min period */
	u32 period_96k = (period_data / 48 + 1) >> 1;	/* 96k min period */
	u32 period_192k = (period_data / 96 + 1) >> 1;	/* 192k min period */

	pr_info("spdifin_reg_set: clk_rate=%d\n", clk_rate);

	aml_audin_write(AUDIN_SPDIF_MODE,
		       (aml_audin_read(AUDIN_SPDIF_MODE) & 0x7fffc000) |
		       (spdif_mode_14bit << 0));
	aml_audin_write(AUDIN_SPDIF_FS_CLK_RLTN,
		       (period_32k << 0) |
		       (period_44k << 6) | (period_48k << 12) |
		       /* Spdif_fs_clk_rltn */
		       (period_96k << 18) | (period_192k << 24));

	/* 88k/176k support */
	if (is_spdif_in_more_rate_support())
		aml_audin_update_bits(AUDIN_SPDIF_ENHANCE_CNTL,
			0x1 << 12,
			0x1 << 12);
}

static void spdifin_fifo1_set_buf(u32 addr, u32 size, u32 src)
{
	if (audio_in_source == 3)
		src = SPDIF_IN;

	aml_audin_write(AUDIN_SPDIF_MODE,
			   aml_audin_read(AUDIN_SPDIF_MODE) & 0x7fffffff);
	aml_audin_write(AUDIN_FIFO1_START, addr & 0xffffffc0);
	aml_audin_write(AUDIN_FIFO1_PTR, (addr & 0xffffffc0));
	aml_audin_write(AUDIN_FIFO1_END,
			   (addr & 0xffffffc0) + (size & 0xffffffc0) - 8);
	aml_audin_write(AUDIN_FIFO1_CTRL, (1 << AUDIN_FIFO_EN)	/* FIFO_EN */
			   |(1 << AUDIN_FIFO_LOAD)	/*load start address.*/
			   |(src << AUDIN_FIFO_DIN_SEL)
			   |(4 << AUDIN_FIFO_ENDIAN) /* AUDIN_FIFO_ENDIAN */
			   |(2 << AUDIN_FIFO_CHAN)	/* 2 channel */
			   |(1 << AUDIN_FIFO_UG)	/* Urgent request. */
	);

	/*
	 *  according clk81 to set reg spdif_mode(0x2800)
	 *  the last 14 bit and reg Spdif_fs_clk_rltn(0x2801)
	 */
	spdifin_reg_set();

	/*3 byte mode, (23:0)*/
	if (src == PAO_IN) {
		aml_audin_write(AUDIN_FIFO1_CTRL1, 0x08);
		if (is_spdif_pao_support())
			chipset_set_spdif_pao();
	} else if (src == HDMI_IN) {
		/* HDMI spdif-in module */
		aml_audin_write(AUDIN_FIFO1_CTRL1, 0x08);
		aml_audin_write(AUDIN_DECODE_FORMAT, 1<<24);
	} else
		aml_audin_write(AUDIN_FIFO1_CTRL1, 0x88);
}

void audio_in_i2s_set_buf(u32 addr, u32 size, u32 i2s_mode,
	u32 i2s_sync, u32 din_sel, u32 ch, snd_pcm_format_t format)

{
	pr_debug("i2sin_fifo0_set_buf din_sel:%d ch:%d\n", din_sel, ch);
	i2sin_fifo0_set_buf(addr, size, i2s_mode, i2s_sync,
		din_sel, ch, format);

}

void audio_in_i2s2_set_buf(u32 addr, u32 size, u32 src, u32 ch)
{
	pr_debug("audio_in_i2s2_set_buf, src = %d\n", src);
	i2sin_fifo2_set_buf(addr, size, src, ch);
}

void audio_in_spdif_set_buf(u32 addr, u32 size, u32 src)
{
	pr_debug("spdifin_fifo1_set_buf, src = %d\n", src);
	spdifin_fifo1_set_buf(addr, size, src);
}

/* extern void audio_in_enabled(int flag); */

void audio_in_i2s_enable(int flag)
{
	int rd = 0, start = 0;

	if (flag) {
		/* reset only when start i2s input */
 reset_again:
		/* reset FIFO 0 */
		aml_audin_update_bits(AUDIN_FIFO0_CTRL, 0x2, 0x2);
		aml_audin_write(AUDIN_FIFO0_PTR, 0);
		rd = aml_audin_read(AUDIN_FIFO0_PTR);
		start = aml_audin_read(AUDIN_FIFO0_START);
		if (rd != start) {
			pr_err("error %08x, %08x !\n",
			       rd, start);
			goto reset_again;
		}
		aml_audin_update_bits(AUDIN_I2SIN_CTRL, 1 << I2SIN_EN,
					 1 << I2SIN_EN);
	} else {
		aml_audin_update_bits(AUDIN_I2SIN_CTRL, 1 << I2SIN_EN,
					 0 << I2SIN_EN);
	}
}

void audio_in_i2s2_enable(int flag)
{
	int rd = 0, start = 0;

	if (flag) {
		/* reset only when start i2s input */
 reset_again:
		/* reset FIFO 2 */
		aml_audin_update_bits(AUDIN_FIFO2_CTRL, 0x2, 0x2);
		aml_audin_write(AUDIN_FIFO2_PTR, 0);
		rd = aml_audin_read(AUDIN_FIFO2_PTR);
		start = aml_audin_read(AUDIN_FIFO2_START);
		if (rd != start) {
			pr_err("error %08x, %08x !\n",
				   rd, start);
			goto reset_again;
		}
		aml_audin_update_bits(AUDIN_DECODE_FORMAT, 1 << 16,
					 1 << 16);
	} else {
		aml_audin_update_bits(AUDIN_DECODE_FORMAT, 1 << 16,
					 0 << 16);
	}
}

void audio_in_spdif_enable(int flag)
{
	int rd = 0, start = 0;

	if (flag) {
 reset_again:
		/* reset FIFO 1 */
		aml_audin_update_bits(AUDIN_FIFO1_CTRL, 0x2, 0x2);
		aml_audin_write(AUDIN_FIFO1_PTR, 0);
		rd = aml_audin_read(AUDIN_FIFO1_PTR);
		start = aml_audin_read(AUDIN_FIFO1_START);
		if (rd != start) {
			pr_err("error %08x, %08x !\n", rd, start);
			goto reset_again;
		}
		aml_audin_write(AUDIN_SPDIF_MODE,
			       aml_audin_read(AUDIN_SPDIF_MODE) | (1 << 31));
	} else {
		aml_audin_write(AUDIN_SPDIF_MODE,
			       aml_audin_read(AUDIN_SPDIF_MODE) & ~(1 << 31));
	}
}

int if_audio_in_i2s_enable(void)
{
	return aml_audin_read(AUDIN_I2SIN_CTRL) & (1 << 15);
}

int if_audio_in_spdif_enable(void)
{
	return aml_audin_read(AUDIN_SPDIF_MODE) & (1 << 31);
}

unsigned int audio_in_i2s_rd_ptr(void)
{
	unsigned int val;

	val = aml_audin_read(AUDIN_FIFO0_RDPTR);
	pr_info("audio in i2s rd ptr: %x\n", val);
	return val;
}

unsigned int audio_in_spdif_rd_ptr(void)
{
	unsigned int val;

	val = aml_audin_read(AUDIN_FIFO1_RDPTR);
	pr_info("audio in spdif rd ptr: %x\n", val);
	return val;
}

unsigned int audio_in_i2s_wr_ptr(void)
{
	unsigned int val;

	aml_audin_write(AUDIN_FIFO0_PTR, 1);
	val = aml_audin_read(AUDIN_FIFO0_PTR);
	return (val) & (~0x3F);
	/* return val&(~0x7); */
}

unsigned int audio_in_i2s2_wr_ptr(void)
{
	unsigned int val;

	aml_audin_write(AUDIN_FIFO2_PTR, 1);
	val = aml_audin_read(AUDIN_FIFO2_PTR);
	return (val) & (~0x3F);
	/* return val&(~0x7); */
}

unsigned int audio_in_spdif_wr_ptr(void)
{
	unsigned int val;

	aml_audin_write(AUDIN_FIFO1_PTR, 1);
	val = aml_audin_read(AUDIN_FIFO1_PTR);
	return (val) & (~0x3F);
}

void audio_in_i2s_set_wrptr(unsigned int val)
{
	aml_audin_write(AUDIN_FIFO0_RDPTR, val);
}

void audio_in_spdif_set_wrptr(unsigned int val)
{
	aml_audin_write(AUDIN_FIFO1_RDPTR, val);
}

#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE
void audio_set_i2s_mode(u32 mode, unsigned int channel)
{
	aml_aiu_write(AIU_I2S_SOURCE_DESC, 0x800);

	if (channel == 8) {
		aml_aiu_update_bits(AIU_I2S_SOURCE_DESC, 1 << 0, 1);

		if (mode == AIU_I2S_MODE_PCM32) {
			aml_aiu_update_bits(AIU_MEM_I2S_CONTROL, 1 << 6, 0);

			aml_aiu_update_bits(AIU_I2S_SOURCE_DESC, 1 << 9,
						1 << 9);
			aml_aiu_update_bits(AIU_I2S_SOURCE_DESC, 7 << 6,
						7 << 6);
		} else if (mode == AIU_I2S_MODE_PCM24) {
			aml_aiu_update_bits(AIU_MEM_I2S_CONTROL, 1 << 6, 0);

			aml_aiu_update_bits(AIU_I2S_SOURCE_DESC, 1 << 9,
						1 << 9);
		} else if (mode == AIU_I2S_MODE_PCM16) {
			/* todo: this mode to be updated */
			aml_aiu_update_bits(AIU_MEM_I2S_CONTROL, 1 << 6,
						1 << 6);

			aml_aiu_update_bits(AIU_I2S_SOURCE_DESC, 3 << 3,
						2 << 3);
			/* txhd fixed this issue */
			if (is_split_fully_support())
				aml_aiu_update_bits(AIU_I2S_SOURCE_DESC,
						1 << 12,
						1 << 12);
		}
	} else if (channel == 2) {
		aml_aiu_update_bits(AIU_I2S_SOURCE_DESC, 1 << 0, 0);

		if (mode == AIU_I2S_MODE_PCM16) {
			aml_aiu_update_bits(AIU_MEM_I2S_CONTROL, 1 << 6,
						1 << 6);

			aml_aiu_update_bits(AIU_I2S_SOURCE_DESC, 3 << 3,
						2 << 3);
		} else if (mode == AIU_I2S_MODE_PCM24) {
			aml_aiu_update_bits(AIU_MEM_I2S_CONTROL, 1 << 6, 0);

			aml_aiu_update_bits(AIU_I2S_SOURCE_DESC, 1 << 9,
						1 << 9);
		} else if (mode == AIU_I2S_MODE_PCM32) {
			aml_aiu_update_bits(AIU_MEM_I2S_CONTROL, 1 << 6, 0);

			aml_aiu_update_bits(AIU_I2S_SOURCE_DESC, 1 << 9,
						1 << 9);
			aml_aiu_update_bits(AIU_I2S_SOURCE_DESC, 7 << 6,
						7 << 6);
		}
	}

	/* In split mode, there are not mask control,
	 * so aiu_mem_i2s_mask[15:0] must set 8'hffff_ffff.
	 */
	/* aml_write_cbus(AIU_MEM_I2S_MASKS,
	 *	(16 << 16) |
	 *	(0xff << 8) |
	 *	(0xff << 0));
	 */
}
#else
void audio_set_i2s_mode(u32 mode)
{
	const unsigned short mask[4] = {
		0x303,		/* 2x16 */
		0x303,		/* 2x24 */
		0x303,		/* 8x24 */
		0x303,		/* 2x32 */
	};

	if (mode < sizeof(mask) / sizeof(unsigned short)) {
		/* four two channels stream */
		aml_aiu_write(AIU_I2S_SOURCE_DESC, 1);

		if (mode == AIU_I2S_MODE_PCM16) {
			aml_aiu_update_bits(AIU_MEM_I2S_CONTROL, 1 << 6,
					     1 << 6);
			aml_aiu_update_bits(AIU_I2S_SOURCE_DESC, 1 << 5, 0);
		} else if (mode == AIU_I2S_MODE_PCM32) {
			aml_aiu_update_bits(AIU_MEM_I2S_CONTROL, 1 << 6, 0);
			aml_aiu_update_bits(AIU_I2S_SOURCE_DESC, 1 << 5,
					     1 << 5);
		} else if (mode == AIU_I2S_MODE_PCM24) {
			aml_aiu_update_bits(AIU_MEM_I2S_CONTROL, 1 << 6, 0);
			aml_aiu_update_bits(AIU_I2S_SOURCE_DESC, 1 << 5,
					     1 << 5);
		}

		aml_aiu_update_bits(AIU_MEM_I2S_MASKS, 0xffff, mask[mode]);
	}
}
#endif

/*
 *  if normal clock, i2s clock is twice of 958 clock,
 *  so the divisor for i2s is 8, but 4 for 958
 *  if over clock, the devisor for i2s is 8, but for 958 should be 1,
 *  because 958 should be 4 times speed according to i2s.
 *  This is dolby digital plus's spec
 */

/* iec958 and i2s clock are separated after M6TV. */
void audio_util_set_dac_958_format(unsigned int format)
{
	/* 958 divisor more, if true, divided by 2, 4, 6, 8 */
	aml_aiu_update_bits(AIU_CLK_CTRL, 1 << 12, 0);
#if IEC958_OVERCLOCK == 1
	/* 958 divisor: 0=no div; 1=div by 2; 2=div by 3; 3=div by 4. */
	/* aml_aiu_update_bits(AIU_CLK_CTRL, 3 << 4, 1 << 4); */
#else
	/* 958 divisor: 0=no div; 1=div by 2; 2=div by 3; 3=div by 4. */
	/* aml_cbus_update_bits(AIU_CLK_CTRL, 3 << 4, 1 << 4); */
#endif
	/* enable 958 divider */
	aml_aiu_update_bits(AIU_CLK_CTRL, 1 << 1, 1 << 1);
}

void audio_util_set_i2s_format(unsigned int format)
{
	/* invert aoclk */
	aml_aiu_update_bits(AIU_CLK_CTRL, 1 << 6, 1 << 6);
	/* invert lrclk */
	aml_aiu_update_bits(AIU_CLK_CTRL, 1 << 7, 1 << 7);
	/* alrclk skew: 1=alrclk transitions on the cycle before msb is sent */
	aml_aiu_update_bits(AIU_CLK_CTRL, 0x3 << 8, 1 << 8);
#if MCLKFS_RATIO == 512
	/* i2s divisor: 0=no div; 1=div by 2; 2=div by 4; 3=div by 8. */
	aml_aiu_update_bits(AIU_CLK_CTRL, 0x3 << 2, 0x3 << 2);
#elif MCLKFS_RATIO == 256
	aml_aiu_update_bits(AIU_CLK_CTRL, 0x3 << 2, 0x2 << 2);
#else
	aml_aiu_update_bits(AIU_CLK_CTRL, 0x3 << 2, 0x1 << 2);
#endif
	/* enable I2S clock */
	aml_aiu_update_bits(AIU_CLK_CTRL, 1, 1);

	if (format == AUDIO_ALGOUT_DAC_FORMAT_LEFT_JUSTIFY)
		aml_aiu_update_bits(AIU_CLK_CTRL, 0x3 << 8, 0);

	if (dac_mute_const == 0x800000)
		aml_aiu_write(AIU_I2S_DAC_CFG, 0x000f);
	else
		/* Payload 24-bit, Msb first, alrclk = aoclk/64 */
		aml_aiu_write(AIU_I2S_DAC_CFG, 0x0007);

	/* four 2-channel */
#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE
	aml_aiu_update_bits(AIU_I2S_SOURCE_DESC, 1 << 11, 1 << 11);
#else
	aml_aiu_write(AIU_I2S_SOURCE_DESC, 0x0001);
#endif
}

/* set sclk and lrclk, mclk = 256fs. */
void audio_set_i2s_clk_div(void)
{
	/* aiclk source */
	aml_aiu_update_bits(AIU_CLK_CTRL, 1 << 10, 1 << 10);
	/* Set mclk over sclk ratio */
	aml_aiu_update_bits(AIU_CLK_CTRL_MORE, 0x3f << 8, (4 - 1) << 8);
	/* set dac/adc lrclk ratio over sclk----64fs */
	aml_aiu_update_bits(AIU_CODEC_DAC_LRCLK_CTRL, 0xfff, (64 - 1));
	aml_aiu_update_bits(AIU_CODEC_ADC_LRCLK_CTRL, 0xfff, (64 - 1));
	/* Enable sclk */
	aml_aiu_update_bits(AIU_CLK_CTRL_MORE, 1 << 14, 1 << 14);
}

void audio_set_spdif_clk_div(uint div)
{
	uint val = 0;

	if (div < 1 && div > 4)
		return;

	val = div - 1;
	/* 958 divisor: 0=no div; 1=div by 2; 2=div by 3; 3=div by 4. */
	aml_aiu_update_bits(AIU_CLK_CTRL, 3 << 4, val << 4);
	/* enable 958 divider */
	aml_aiu_update_bits(AIU_CLK_CTRL, 1 << 1, 1 << 1);
}

void audio_enable_output(int flag)
{
	if (flag) {
		aml_aiu_write(AIU_RST_SOFT, 0x05);
		aml_aiu_read(AIU_I2S_SYNC);
		aml_aiu_update_bits(AIU_MEM_I2S_CONTROL, 3 << 1, 3 << 1);

		/* Maybe cause POP noise */
		/* audio_i2s_unmute(); */
	} else {
		aml_aiu_update_bits(AIU_MEM_I2S_CONTROL, 3 << 1, 0);

		/* Maybe cause POP noise */
		/* audio_i2s_mute(); */
	}
	/* audio_out_enabled(flag); */
}

int if_audio_out_enable(void)
{
	return aml_aiu_read(AIU_MEM_I2S_CONTROL) & (0x3 << 1);
}
EXPORT_SYMBOL(if_audio_out_enable);

int if_958_audio_out_enable(void)
{
	return aml_aiu_read(AIU_MEM_IEC958_CONTROL) & (0x3 << 1);
}
EXPORT_SYMBOL(if_958_audio_out_enable);

unsigned int read_i2s_rd_ptr(void)
{
	unsigned int val;

	val = aml_aiu_read(AIU_MEM_I2S_RD_PTR);
	return val;
}

unsigned int read_iec958_rd_ptr(void)
{
	unsigned int val;

	val = aml_aiu_read(AIU_MEM_IEC958_RD_PTR);
	return val;
}

void aml_audio_i2s_unmute(void)
{
	aml_aiu_update_bits(AIU_I2S_MUTE_SWAP, 0xff << 8, 0);
}

void aml_audio_i2s_mute(void)
{
	aml_aiu_update_bits(AIU_I2S_MUTE_SWAP, 0xff << 8, 0xff << 8);
}

void audio_i2s_unmute(void)
{
	aml_aiu_update_bits(AIU_I2S_MUTE_SWAP, 0xff << 8, 0);
	aml_aiu_update_bits(AIU_958_CTRL, 0x3 << 3, 0 << 3);
}

void audio_i2s_mute(void)
{
	aml_aiu_update_bits(AIU_I2S_MUTE_SWAP, 0xff << 8, 0xff << 8);
	aml_aiu_update_bits(AIU_958_CTRL, 0x3 << 3, 0x3 << 3);
}

void audio_mute_left_right(unsigned int flag)
{
	if (flag == 0) {	/* right */
		aml_aiu_update_bits(AIU_958_CTRL, 0x3 << 3, 0x1 << 3);
	} else if (flag == 1) {	/* left */
		aml_aiu_update_bits(AIU_958_CTRL, 0x3 << 3, 0x2 << 3);
	}
}

void audio_hw_958_reset(unsigned int slow_domain, unsigned int fast_domain)
{
	aml_aiu_write(AIU_958_DCU_FF_CTRL, 0);
	aml_aiu_write(AIU_RST_SOFT, (slow_domain << 3) | (fast_domain << 2));
}

void audio_hw_958_raw(void)
{
	aml_aiu_write(AIU_958_MISC, 1);
	/* raw */
	aml_aiu_update_bits(AIU_MEM_IEC958_CONTROL, 1 << 8, 1 << 8);
	/* 8bit */
	aml_aiu_update_bits(AIU_MEM_IEC958_CONTROL, 1 << 7, 0);
	/* endian */
	aml_aiu_update_bits(AIU_MEM_IEC958_CONTROL, 0x7 << 3, 1 << 3);

	aml_aiu_write(AIU_958_BPF, IEC958_bpf);
	aml_aiu_write(AIU_958_BRST, IEC958_brst);
	aml_aiu_write(AIU_958_LENGTH, IEC958_length);
	aml_aiu_write(AIU_958_PADDSIZE, IEC958_padsize);
	/* disable int */
	aml_aiu_update_bits(AIU_958_DCU_FF_CTRL, 0x3 << 2, 0);

	if (IEC958_mode == 1) {	/* search in byte */
		aml_aiu_update_bits(AIU_958_DCU_FF_CTRL, 0x7 << 4, 0x7 << 4);
	} else if (IEC958_mode == 2) {	/* search in word */
		aml_aiu_update_bits(AIU_958_DCU_FF_CTRL, 0x7 << 4, 0x5 << 4);
	} else {
		aml_aiu_update_bits(AIU_958_DCU_FF_CTRL, 0x7 << 4, 0);
	}
	aml_aiu_write(AIU_958_CHSTAT_L0, IEC958_chstat0_l);
	aml_aiu_write(AIU_958_CHSTAT_L1, IEC958_chstat1_l);
	aml_aiu_write(AIU_958_CHSTAT_R0, IEC958_chstat0_r);
	aml_aiu_write(AIU_958_CHSTAT_R1, IEC958_chstat1_r);

	aml_aiu_write(AIU_958_SYNWORD1, IEC958_syncword1);
	aml_aiu_write(AIU_958_SYNWORD2, IEC958_syncword2);
	aml_aiu_write(AIU_958_SYNWORD3, IEC958_syncword3);
	aml_aiu_write(AIU_958_SYNWORD1_MASK, IEC958_syncword1_mask);
	aml_aiu_write(AIU_958_SYNWORD2_MASK, IEC958_syncword2_mask);
	aml_aiu_write(AIU_958_SYNWORD3_MASK, IEC958_syncword3_mask);

	pr_info("\tBPF: %x\n", IEC958_bpf);
	pr_info("\tBRST: %x\n", IEC958_brst);
	pr_info("\tLENGTH: %x\n", IEC958_length);
	pr_info("\tPADDSIZE: %x\n", IEC958_length);
	pr_info("\tsyncword: %x, %x, %x\n\n", IEC958_syncword1,
		IEC958_syncword2, IEC958_syncword3);

}

void set_958_channel_status(struct _aiu_958_channel_status_t *set)
{
	if (set) {
		aml_aiu_write(AIU_958_CHSTAT_L0, set->chstat0_l);
		aml_aiu_write(AIU_958_CHSTAT_L1, set->chstat1_l);
		aml_aiu_write(AIU_958_CHSTAT_R0, set->chstat0_r);
		aml_aiu_write(AIU_958_CHSTAT_R1, set->chstat1_r);
	}
}

static void audio_hw_set_958_pcm24(struct _aiu_958_raw_setting_t *set)
{
	/* in pcm mode, set bpf to 128 */
	aml_aiu_write(AIU_958_BPF, 0x80);
	set_958_channel_status(set->chan_stat);
}

void audio_set_958_mode(unsigned int mode, struct _aiu_958_raw_setting_t *set)
{
	if (mode == AIU_958_MODE_PCM_RAW) {
		mode = AIU_958_MODE_PCM16;	/* use 958 raw pcm mode */
		aml_aiu_write(AIU_958_VALID_CTRL, 3);
	} else
		aml_aiu_write(AIU_958_VALID_CTRL, 0);

	if (mode == AIU_958_MODE_RAW) {

		audio_hw_958_raw();
		aml_aiu_write(AIU_958_MISC, 1);
		/* raw */
		aml_aiu_update_bits(AIU_MEM_IEC958_CONTROL,
				     1 << 8, 1 << 8);
		/* 8bit */
		aml_aiu_update_bits(AIU_MEM_IEC958_CONTROL, 1 << 7, 0);
		/* endian */
		aml_aiu_update_bits(AIU_MEM_IEC958_CONTROL,
				     0x7 << 3, 0x1 << 3);

		pr_info("IEC958 RAW\n");
	} else if (mode == AIU_958_MODE_PCM32) {
		audio_hw_set_958_pcm24(set);
#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE
		aml_aiu_write(AIU_958_MISC, 0x3480);
		/* pcm */
		aml_aiu_update_bits(AIU_MEM_IEC958_CONTROL, 1 << 8,
					1 << 8);
#else
		aml_aiu_write(AIU_958_MISC, 0x2020 | (1 << 7));
		/* pcm */
		aml_aiu_update_bits(AIU_MEM_IEC958_CONTROL, 1 << 8, 0);
#endif
		/* 16bit */
		aml_aiu_update_bits(AIU_MEM_IEC958_CONTROL, 1 << 7, 0);
		/* endian */
		aml_aiu_update_bits(AIU_MEM_IEC958_CONTROL,
				     0x7 << 3, 0);

		pr_info("IEC958 PCM32\n");
	} else if (mode == AIU_958_MODE_PCM24) {
		audio_hw_set_958_pcm24(set);
#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE
		aml_aiu_write(AIU_958_MISC, 0x3480);
		/* pcm */
		aml_aiu_update_bits(AIU_MEM_IEC958_CONTROL, 1 << 8,
						1 << 8);
#else
		aml_aiu_write(AIU_958_MISC, 0x2020 | (1 << 7));
		/* pcm */
		aml_aiu_update_bits(AIU_MEM_IEC958_CONTROL, 1 << 8, 0);
#endif
		/* 16bit */
		aml_aiu_update_bits(AIU_MEM_IEC958_CONTROL, 1 << 7, 0);
		/* endian */
		aml_aiu_update_bits(AIU_MEM_IEC958_CONTROL,
				     0x7 << 3, 0);

		pr_info("IEC958 24bit\n");
	} else if (mode == AIU_958_MODE_PCM16) {
		audio_hw_set_958_pcm24(set);
		aml_aiu_write(AIU_958_MISC, 0x2042);
		/* pcm */
#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE
		aml_aiu_update_bits(AIU_MEM_IEC958_CONTROL, 1 << 8,
					1 << 8);
#else
		aml_aiu_update_bits(AIU_MEM_IEC958_CONTROL, 1 << 8, 0);
#endif
		/* 16bit */
		aml_aiu_update_bits(AIU_MEM_IEC958_CONTROL,
				     1 << 7, 1 << 7);
		/* endian */
		aml_aiu_update_bits(AIU_MEM_IEC958_CONTROL,
				     0x7 << 3, 0);
		pr_info("IEC958 16bit\n");
	}

	audio_hw_958_reset(0, 1);

#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE
	if (mode == AIU_958_MODE_PCM32)
		aml_aiu_update_bits(AIU_958_DCU_FF_CTRL, 1 << 8, 1 << 8);
#endif

	aml_aiu_write(AIU_958_FORCE_LEFT, 1);
}

void audio_out_i2s_enable(unsigned int flag)
{
	if (flag) {
		aml_aiu_write(AIU_RST_SOFT, 0x01);
		aml_aiu_read(AIU_I2S_SYNC);
		aml_aiu_update_bits(AIU_MEM_I2S_CONTROL, 0x3 << 1, 0x3 << 1);
		/* Maybe cause POP noise */
		/* audio_i2s_unmute(); */
	} else {
		aml_aiu_update_bits(AIU_MEM_I2S_CONTROL, 0x3 << 1, 0);

		/* Maybe cause POP noise */
		/* audio_i2s_mute(); */
	}
	/* audio_out_enabled(flag); */
}

void audio_hw_958_enable(unsigned int flag)
{
	if (flag) {
		aml_aiu_write(AIU_RST_SOFT, 0x04);
		aml_aiu_write(AIU_958_FORCE_LEFT, 0);
		aml_aiu_update_bits(AIU_958_DCU_FF_CTRL, 1, 1);
		aml_aiu_update_bits(AIU_MEM_IEC958_CONTROL, 0x3 << 1,
				     0x3 << 1);
	} else {
		aml_aiu_write(AIU_RST_SOFT, 0x04);
		aml_aiu_write(AIU_958_FORCE_LEFT, 0);
		aml_aiu_write(AIU_958_DCU_FF_CTRL, 0);
		aml_aiu_update_bits(AIU_MEM_IEC958_CONTROL, 0x3 << 1,
				     0);
	}
}

unsigned int read_i2s_mute_swap_reg(void)
{
	unsigned int val;

	val = aml_aiu_read(AIU_I2S_MUTE_SWAP);
	return val;
}

void audio_i2s_swap_left_right(unsigned int flag)
{
	/*only LPCM output can set aiu hw channel swap*/
	if (IEC958_mode_raw == 0) {
		if (IEC958_mode_codec == 0 || IEC958_mode_codec == 9)
			aml_aiu_update_bits(AIU_958_CTRL, 0x3 << 1, flag << 1);

		aml_aiu_update_bits(AIU_I2S_MUTE_SWAP, 0x3, flag);
		aml_aiu_update_bits(AIU_I2S_MUTE_SWAP, 0x3 << 2, flag << 2);
	} else {
		aml_cbus_update_bits(AIU_958_CTRL, 0x3 << 1, 0 << 1);
		/* Hold I2S */
		aml_aiu_update_bits(AIU_I2S_MISC, 0x1 << 2,
					1 << 2);
		/* force audio data to left or right */
		aml_aiu_update_bits(AIU_I2S_MISC, 0x1 << 4,
					1 << 4);
		/* Release hold */
		aml_aiu_update_bits(AIU_I2S_MISC, 0x1 << 2,
					0 << 2);
	}
}

void audio_i2s_958_same_source(unsigned int same)
{
	aml_aiu_update_bits(AIU_I2S_MISC, 1 << 3, (!!same) << 3);
}

void set_hw_resample_source(int source)
{
	aml_eqdrc_update_bits(AUD_RESAMPLE_CTRL0, 1 << 29, source << 29);
}
EXPORT_SYMBOL(set_hw_resample_source);

/*1: select pcm data/clk; 2: select AIU i2s data/clk*/
void set_hdmi_tx_clk_source(int source)
{
	aml_aiu_update_bits(AIU_HDMI_CLK_DATA_CTRL, 0x3, source);
	aml_aiu_update_bits(AIU_HDMI_CLK_DATA_CTRL, 0x3 << 4, source << 4);
}

#if 0
unsigned int audio_hdmi_init_ready(void)
{
	return READ_MPEG_REG_BITS(AIU_HDMI_CLK_DATA_CTRL, 0, 2);
}

/* power gate control for iec958 audio out */
unsigned int audio_spdifout_pg_enable(unsigned char enable)
{
	if (enable) {
		aml_aiu_update_bits(MPLL_958_CNTL, 1, 14, 1);
		AUDIO_CLK_GATE_ON(AIU_IEC958);
		AUDIO_CLK_GATE_ON(AIU_ICE958_AMCLK);
	} else {
		AUDIO_CLK_GATE_OFF(AIU_IEC958);
		AUDIO_CLK_GATE_OFF(AIU_ICE958_AMCLK);
		aml_aiu_update_bits(MPLL_958_CNTL, 0, 14, 1);
	}
	return 0;
}

/*
 *    power gate control for normal aiu  domain including i2s in/out
 *   TODO: move i2s out /adc related gate to i2s cpu dai driver
 */
unsigned int audio_aiu_pg_enable(unsigned char enable)
{
	if (enable)
		switch_mod_gate_by_name("audio", 1);
	else
		switch_mod_gate_by_name("audio", 0);

	return 0;
}
#endif
