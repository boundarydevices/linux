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
#define pr_fmt(fmt) "aml_audio_hw: " fmt

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
#include <linux/amlogic/cpu_version.h>
#include "audio_hw.h"

/* i2s mode 0: master 1: slave */
/* source: 0: linein; 1: ATV; 2: HDMI-in */
unsigned int IEC958_MODE = AIU_958_MODE_PCM16;
unsigned int I2S_MODE = AIU_I2S_MODE_PCM16;
unsigned int audio_in_source;
void set_i2s_source(unsigned int source)
{
	audio_in_source = source;
}

int audio_in_buf_ready;
int audio_out_buf_ready;

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

void audio_set_aiubuf(u32 addr, u32 size, unsigned int channel)
{
#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE
	aml_write_cbus(AIU_MEM_I2S_START_PTR, addr & 0xffffff00);
	aml_write_cbus(AIU_MEM_I2S_RD_PTR, addr & 0xffffff00);
#else
	aml_write_cbus(AIU_MEM_I2S_START_PTR, addr & 0xffffffc0);
	aml_write_cbus(AIU_MEM_I2S_RD_PTR, addr & 0xffffffc0);
#endif

	if (channel == 8) {
		/*select cts_aoclkx2_int as AIU clk to hdmi_tx_audio_mster_clk*/
		aml_cbus_update_bits(AIU_CLK_CTRL_MORE, 1 << 6, 1 << 6);
		/*unmute all channels*/
		aml_cbus_update_bits(AIU_I2S_MUTE_SWAP, 0xff << 8, 0 << 8);
#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE
		aml_write_cbus(AIU_MEM_I2S_END_PTR,
			(addr & 0xffffff00) + (size & 0xffffff00) - 256);
#else
		aml_write_cbus(AIU_MEM_I2S_END_PTR,
			(addr & 0xffffffc0) + (size & 0xffffffc0) - 256);
#endif
	} else {
		/*select cts_clk_i958 as AIU clk to hdmi_tx_audio_mster_clk*/
		aml_cbus_update_bits(AIU_CLK_CTRL_MORE, 1 << 6, 0 << 6);
		/*unmute 0/1 channel*/
		aml_cbus_update_bits(AIU_I2S_MUTE_SWAP, 0xff << 8, 0xfc << 8);
#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE
		aml_write_cbus(AIU_MEM_I2S_END_PTR,
			(addr & 0xffffff00) + (size & 0xffffff00) - 256);
#else
		aml_write_cbus(AIU_MEM_I2S_END_PTR,
			(addr & 0xffffffc0) + (size & 0xffffffc0) - 64);
#endif
	}
	/* Hold I2S */
	aml_write_cbus(AIU_I2S_MISC, 0x0004);
	/* Release hold and force audio data to left or right */
	aml_write_cbus(AIU_I2S_MISC, 0x0010);

	if (channel == 8) {
		pr_info("%s channel == 8\n", __func__);
		/* [31:16] IRQ block. */
		aml_write_cbus(AIU_MEM_I2S_MASKS, (24 << 16) |
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
		aml_write_cbus(AIU_MEM_I2S_MASKS, (24 << 16) |
					(0xff << 8) |
					(0xff << 0));
#else
		/* [31:16] IRQ block. */
		aml_write_cbus(AIU_MEM_I2S_MASKS, (24 << 16) |
				   (0x3 << 8) |
				   (0x3 << 0));
#endif
	}
	/* 16 bit PCM mode */
	/* aml_cbus_update_bits(AIU_MEM_I2S_CONTROL, 1, 6, 1); */
	/* Set init high then low to initialize the I2S memory logic */
	aml_cbus_update_bits(AIU_MEM_I2S_CONTROL, 1, 1);
	aml_cbus_update_bits(AIU_MEM_I2S_CONTROL, 1, 0);

	aml_write_cbus(AIU_MEM_I2S_BUF_CNTL, 1 | (0 << 1));
	aml_write_cbus(AIU_MEM_I2S_BUF_CNTL, 0 | (0 << 1));

	audio_out_buf_ready = 1;
}

void audio_set_958outbuf(u32 addr, u32 size, int flag)
{
	aml_write_cbus(AIU_MEM_IEC958_START_PTR, addr & 0xffffffc0);
	if (aml_read_cbus(AIU_MEM_IEC958_START_PTR) ==
	    aml_read_cbus(AIU_MEM_I2S_START_PTR)) {
		aml_write_cbus(AIU_MEM_IEC958_RD_PTR,
			       aml_read_cbus(AIU_MEM_I2S_RD_PTR));
	} else
		aml_write_cbus(AIU_MEM_IEC958_RD_PTR,
			       addr & 0xffffffc0);
	if (flag == 0) {
		/* this is for 16bit 2 channel */
#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE
		aml_write_cbus(AIU_MEM_IEC958_END_PTR,
			       (addr & 0xffffffc0) +
			       (size & 0xffffffc0) - 8);
#else
		aml_write_cbus(AIU_MEM_IEC958_END_PTR,
			       (addr & 0xffffffc0) +
			       (size & 0xffffffc0) - 64);
#endif
	} else {
		/* this is for RAW mode */
#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE
		aml_write_cbus(AIU_MEM_IEC958_END_PTR,
					(addr & 0xffffffc0) +
					(size & 0xffffffc0) - 8);
#else
		aml_write_cbus(AIU_MEM_IEC958_END_PTR,
			       (addr & 0xffffffc0) +
			       (size & 0xffffffc0) - 1);
#endif
	}
#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE
	aml_cbus_update_bits(AIU_MEM_IEC958_MASKS, 0xffff, 0xffff);
#else
	aml_cbus_update_bits(AIU_MEM_IEC958_MASKS, 0xffff, 0x303);
#endif
	aml_cbus_update_bits(AIU_MEM_IEC958_CONTROL, 1, 1);
	aml_cbus_update_bits(AIU_MEM_IEC958_CONTROL, 1, 0);

	aml_write_cbus(AIU_MEM_IEC958_BUF_CNTL, 1 | (0 << 1));
	aml_write_cbus(AIU_MEM_IEC958_BUF_CNTL, 0 | (0 << 1));
}

/*
 * i2s mode 0: master 1: slave
 * din_sel 0:spdif 1:i2s 2:pcm 3: dmic
 */
static void i2sin_fifo0_set_buf(u32 addr, u32 size, u32 i2s_mode,
		u32 i2s_sync, u32 din_sel, u32 ch)
{
	unsigned char mode = 0;
	unsigned int sync_mode = 0, din_pos = 0;

	if (i2s_sync)
		sync_mode = i2s_sync;
	if (i2s_mode & I2SIN_SLAVE_MODE)
		mode = 1;
	if (din_sel != 1)
		din_pos = 1;
	aml_write_cbus(AUDIN_FIFO0_START, addr & 0xffffffc0);
	aml_write_cbus(AUDIN_FIFO0_PTR, (addr & 0xffffffc0));
	aml_write_cbus(AUDIN_FIFO0_END,
		       (addr & 0xffffffc0) + (size & 0xffffffc0) - 8);

	aml_write_cbus(AUDIN_FIFO0_CTRL, (1 << AUDIN_FIFO0_EN)	/* FIFO0_EN */
		       |(1 << AUDIN_FIFO0_LOAD)	/* load start address */
		       |(din_sel << AUDIN_FIFO0_DIN_SEL)

		       /* DIN from i2sin */
		       /* |(1<<6)    // 32 bits data in. */
		       /* |(0<<7)    // put the 24bits data to  low 24 bits */
		       | (4 << AUDIN_FIFO0_ENDIAN) /* AUDIN_FIFO0_ENDIAN */
		       |((ch == 2?2:1) << AUDIN_FIFO0_CHAN) /* ch mode ctl */
		       |(0 << 16)	/* to DDR */
		       |(1 << AUDIN_FIFO0_UG)	/* Urgent request. */
		       |(0 << 17)	/* Overflow Interrupt mask */
		       |(0 << 18)
		       /* Audio in INT */
		       /* |(1<<19)    // hold 0 enable */
		       | (0 << AUDIN_FIFO0_UG)	/* hold0 to aififo */
	    );

	aml_write_cbus(AUDIN_FIFO0_CTRL1, 0 << 4	/* fifo0_dest_sel */
		       | 2 << 2	/* fifo0_din_byte_num */
		       | din_pos << 0);	/* fifo0_din_pos */

	if (audio_in_source == 0) {
		aml_write_cbus(AUDIN_I2SIN_CTRL,
				   ((0xf>>(4 - ch/2)) << I2SIN_CHAN_EN)
				   | (3 << I2SIN_SIZE)
				   | (1 << I2SIN_LRCLK_INVT)
				   | (1 << I2SIN_LRCLK_SKEW)
				   | (sync_mode << I2SIN_POS_SYNC)
				   | (!mode << I2SIN_LRCLK_SEL)
				   | (!mode << I2SIN_CLK_SEL)
				   | (!mode << I2SIN_DIR));

	} else if (audio_in_source == 1) {
		aml_write_cbus(AUDIN_I2SIN_CTRL, (1 << I2SIN_CHAN_EN)
				   | (0 << I2SIN_SIZE)
				   | (0 << I2SIN_LRCLK_INVT)
				   | (0 << I2SIN_LRCLK_SKEW)
				   | (1 << I2SIN_POS_SYNC)
				   | (0 << I2SIN_LRCLK_SEL)
				   | (0 << I2SIN_CLK_SEL)
				   | (0 << I2SIN_DIR));
	} else if (audio_in_source == 2) {
		aml_write_cbus(AUDIN_I2SIN_CTRL, (1 << I2SIN_CHAN_EN)
				   | (3 << I2SIN_SIZE)
				   | (1 << I2SIN_LRCLK_INVT)
				   | (1 << I2SIN_LRCLK_SKEW)
				   | (1 << I2SIN_POS_SYNC)
				   | (1 << I2SIN_LRCLK_SEL)
				   | (1 << I2SIN_CLK_SEL)
				   | (1 << I2SIN_DIR));
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

	aml_write_cbus(AUDIN_SPDIF_MODE,
		       (aml_read_cbus(AUDIN_SPDIF_MODE) & 0x7fffc000) |
		       (spdif_mode_14bit << 0));
	aml_write_cbus(AUDIN_SPDIF_FS_CLK_RLTN,
		       (period_32k << 0) |
		       (period_44k << 6) | (period_48k << 12) |
		       /* Spdif_fs_clk_rltn */
		       (period_96k << 18) | (period_192k << 24));

}

static void spdifin_fifo1_set_buf(u32 addr, u32 size, u32 src)
{
	aml_write_cbus(AUDIN_SPDIF_MODE,
			   aml_read_cbus(AUDIN_SPDIF_MODE) & 0x7fffffff);
	/*set channel invert from old spdif in mode*/
	aml_cbus_update_bits(AUDIN_SPDIF_MODE, (1 << 19), (1 << 19));
	aml_write_cbus(AUDIN_FIFO1_START, addr & 0xffffffc0);
	aml_write_cbus(AUDIN_FIFO1_PTR, (addr & 0xffffffc0));
	aml_write_cbus(AUDIN_FIFO1_END,
		       (addr & 0xffffffc0) + (size & 0xffffffc0) - 8);
	aml_write_cbus(AUDIN_FIFO1_CTRL, (1 << AUDIN_FIFO1_EN)	/* FIFO0_EN */
		       |(1 << AUDIN_FIFO1_LOAD)	/* load start address. */
		       |(src << AUDIN_FIFO1_DIN_SEL)

		       /* DIN from i2sin. */
		       /* |(1<<6)   // 32 bits data in. */
		       /* |(0<<7)   // put the 24bits data to  low 24 bits */
		       | (4 << AUDIN_FIFO1_ENDIAN)	/* AUDIN_FIFO0_ENDIAN */
		       |(2 << AUDIN_FIFO1_CHAN)	/* 2 channel */
		       |(0 << 16)	/* to DDR */
		       |(1 << AUDIN_FIFO1_UG)	/* Urgent request. */
		       |(0 << 17)	/* Overflow Interrupt mask */
		       |(0 << 18)
		       /* Audio in INT */
		       /* |(1<<19)   //hold 0 enable */
		       | (0 << AUDIN_FIFO1_UG)	/* hold0 to aififo */
	    );

	/*
	 *  according clk81 to set reg spdif_mode(0x2800)
	 *  the last 14 bit and reg Spdif_fs_clk_rltn(0x2801)
	 */
	spdifin_reg_set();
	/*3 byte mode, (23:0)*/
	if (src == PAO_IN) {
		aml_write_cbus(AUDIN_FIFO1_CTRL1, 0x08);
	} else if (src == HDMI_IN) {
		/* there are two inputs for HDMI_IN. New I2S:SPDIF */
		aml_write_cbus(AUDIN_FIFO1_CTRL1, 0x08);
		if (1) {
			/* new SPDIF in module */
			aml_write_cbus(AUDIN_DECODE_FORMAT, 1<<24);
		} else {
			/* new I2S in module */
			aml_write_cbus(AUDIN_DECODE_FORMAT, 0x103ad);
		}
	} else
		aml_write_cbus(AUDIN_FIFO1_CTRL1, 0x88);
}

void audio_in_i2s_set_buf(u32 addr, u32 size,
	u32 i2s_mode, u32 i2s_sync, u32 din_sel, u32 ch)
{
	pr_info("i2sin_fifo0_set_buf din_sel:%d ch:%d\n", din_sel, ch);
	i2sin_fifo0_set_buf(addr, size, i2s_mode, i2s_sync, din_sel, ch);
	audio_in_buf_ready = 1;
}

void audio_in_spdif_set_buf(u32 addr, u32 size, u32 src)
{
	pr_info("spdifin_fifo1_set_buf, src = %d\n", src);
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
		aml_cbus_update_bits(AUDIN_FIFO0_CTRL, 0x2, 0x2);
		aml_write_cbus(AUDIN_FIFO0_PTR, 0);
		rd = aml_read_cbus(AUDIN_FIFO0_PTR);
		start = aml_read_cbus(AUDIN_FIFO0_START);
		if (rd != start) {
			pr_err("error %08x, %08x !\n",
			       rd, start);
			goto reset_again;
		}
		aml_cbus_update_bits(AUDIN_I2SIN_CTRL, 1 << I2SIN_EN,
				     1 << I2SIN_EN);
	} else {
		aml_cbus_update_bits(AUDIN_I2SIN_CTRL, 1 << I2SIN_EN,
				     0 << I2SIN_EN);
	}
}

void audio_in_spdif_enable(int flag)
{
	int rd = 0, start = 0;

	if (flag) {
 reset_again:
		/* reset FIFO 0 */
		aml_cbus_update_bits(AUDIN_FIFO1_CTRL, 0x2, 0x2);
		aml_write_cbus(AUDIN_FIFO1_PTR, 0);
		rd = aml_read_cbus(AUDIN_FIFO1_PTR);
		start = aml_read_cbus(AUDIN_FIFO1_START);
		if (rd != start) {
			pr_err("error %08x, %08x !\n", rd, start);
			goto reset_again;
		}
		aml_write_cbus(AUDIN_SPDIF_MODE,
			       aml_read_cbus(AUDIN_SPDIF_MODE) | (1 << 31));
	} else {
		aml_write_cbus(AUDIN_SPDIF_MODE,
			       aml_read_cbus(AUDIN_SPDIF_MODE) & ~(1 << 31));
	}
}

int if_audio_in_i2s_enable(void)
{
	return aml_read_cbus(AUDIN_I2SIN_CTRL) & (1 << 15);
}

int if_audio_in_spdif_enable(void)
{
	return aml_read_cbus(AUDIN_SPDIF_MODE) & (1 << 31);
}

unsigned int audio_in_i2s_rd_ptr(void)
{
	unsigned int val;

	val = aml_read_cbus(AUDIN_FIFO0_RDPTR);
	pr_info("audio in i2s rd ptr: %x\n", val);
	return val;
}

unsigned int audio_in_spdif_rd_ptr(void)
{
	unsigned int val;

	val = aml_read_cbus(AUDIN_FIFO1_RDPTR);
	pr_info("audio in spdif rd ptr: %x\n", val);
	return val;
}

unsigned int audio_in_i2s_wr_ptr(void)
{
	unsigned int val;

	aml_write_cbus(AUDIN_FIFO0_PTR, 1);
	val = aml_read_cbus(AUDIN_FIFO0_PTR);
	return (val) & (~0x3F);
	/* return val&(~0x7); */
}

unsigned int audio_in_spdif_wr_ptr(void)
{
	unsigned int val;

	aml_write_cbus(AUDIN_FIFO1_PTR, 1);
	val = aml_read_cbus(AUDIN_FIFO1_PTR);
	return (val) & (~0x3F);
}

void audio_in_i2s_set_wrptr(unsigned int val)
{
	aml_write_cbus(AUDIN_FIFO0_RDPTR, val);
}

void audio_in_spdif_set_wrptr(unsigned int val)
{
	aml_write_cbus(AUDIN_FIFO1_RDPTR, val);
}

#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE
void audio_set_i2s_mode(u32 mode, unsigned int channel)
{
	aml_write_cbus(AIU_I2S_SOURCE_DESC, 0x800);

	aml_cbus_update_bits(AIU_CLK_CTRL_MORE, 0x1f, 0);

	if (channel == 8) {
		aml_cbus_update_bits(AIU_I2S_SOURCE_DESC, 1 << 0, 1);

		if (mode == AIU_I2S_MODE_PCM32) {
			aml_cbus_update_bits(AIU_MEM_I2S_CONTROL, 1 << 6, 0);

			aml_cbus_update_bits(AIU_I2S_SOURCE_DESC, 1 << 9,
						1 << 9);
			aml_cbus_update_bits(AIU_I2S_SOURCE_DESC, 7 << 6,
						7 << 6);
		} else if (mode == AIU_I2S_MODE_PCM24) {
			/* todo: to verify it */
			aml_cbus_update_bits(AIU_MEM_I2S_CONTROL, 1 << 6, 0);

			aml_cbus_update_bits(AIU_I2S_SOURCE_DESC, 1 << 9,
						1 << 9);
			aml_cbus_update_bits(AIU_I2S_SOURCE_DESC, 7 << 6,
						7 << 6);
			aml_cbus_update_bits(AIU_I2S_SOURCE_DESC, 1 << 5,
						1 << 5);

		} else if (mode == AIU_I2S_MODE_PCM16) {
			aml_cbus_update_bits(AIU_MEM_I2S_CONTROL, 1 << 6,
						1 << 6);

			aml_cbus_update_bits(AIU_I2S_SOURCE_DESC, 2 << 3,
						2 << 3);

			aml_cbus_update_bits(AIU_CLK_CTRL_MORE, 0x1f, 0x5);
		}
	} else if (channel == 2) {
		aml_cbus_update_bits(AIU_I2S_SOURCE_DESC, 1 << 0, 0);

		if (mode == AIU_I2S_MODE_PCM16) {
			aml_cbus_update_bits(AIU_MEM_I2S_CONTROL, 1 << 6,
						1 << 6);

			aml_cbus_update_bits(AIU_I2S_SOURCE_DESC, 2 << 3,
						2 << 3);
		} else if (mode == AIU_I2S_MODE_PCM24) {
			aml_cbus_update_bits(AIU_MEM_I2S_CONTROL, 1 << 6, 0);

			aml_cbus_update_bits(AIU_I2S_SOURCE_DESC, 1 << 5,
						1 << 5);
			aml_cbus_update_bits(AIU_I2S_SOURCE_DESC, 2 << 3,
						2 << 3);
		} else if (mode == AIU_I2S_MODE_PCM32) {
			aml_cbus_update_bits(AIU_MEM_I2S_CONTROL, 1 << 6, 0);

			aml_cbus_update_bits(AIU_I2S_SOURCE_DESC, 1 << 9,
						1 << 9);
			aml_cbus_update_bits(AIU_I2S_SOURCE_DESC, 7 << 6,
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
		aml_write_cbus(AIU_I2S_SOURCE_DESC, 1);

		if (mode == AIU_I2S_MODE_PCM16) {
			aml_cbus_update_bits(AIU_MEM_I2S_CONTROL, 1 << 6,
					     1 << 6);
			aml_cbus_update_bits(AIU_I2S_SOURCE_DESC, 1 << 5, 0);
		} else if (mode == AIU_I2S_MODE_PCM32) {
			aml_cbus_update_bits(AIU_MEM_I2S_CONTROL, 1 << 6, 0);
			aml_cbus_update_bits(AIU_I2S_SOURCE_DESC, 1 << 5,
					     1 << 5);
		} else if (mode == AIU_I2S_MODE_PCM24) {
			aml_cbus_update_bits(AIU_MEM_I2S_CONTROL, 1 << 6, 0);
			aml_cbus_update_bits(AIU_I2S_SOURCE_DESC, 1 << 5,
					     1 << 5);
		}

		aml_cbus_update_bits(AIU_MEM_I2S_MASKS, 0xffff, mask[mode]);
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
	aml_cbus_update_bits(AIU_CLK_CTRL, 1 << 12, 0);
#if IEC958_OVERCLOCK == 1
	/* 958 divisor: 0=no div; 1=div by 2; 2=div by 3; 3=div by 4. */
/*	aml_cbus_update_bits(AIU_CLK_CTRL, 3 << 4, 1 << 4);  */
#else
	/* 958 divisor: 0=no div; 1=div by 2; 2=div by 3; 3=div by 4. */
/*	aml_cbus_update_bits(AIU_CLK_CTRL, 3 << 4, 1 << 4);  */
#endif
	/* enable 958 divider */
	aml_cbus_update_bits(AIU_CLK_CTRL_MORE, 1 << 6, 0 << 6);
	aml_cbus_update_bits(AIU_CLK_CTRL, 1 << 1, 1 << 1);
}

void audio_util_set_dac_i2s_format(unsigned int format)
{
	/* invert aoclk */
	aml_cbus_update_bits(AIU_CLK_CTRL, 1 << 6, 1 << 6);
	/* invert lrclk */
	aml_cbus_update_bits(AIU_CLK_CTRL, 1 << 7, 1 << 7);
	/* alrclk skew: 1=alrclk transitions on the cycle before msb is sent */
	aml_cbus_update_bits(AIU_CLK_CTRL, 0x3 << 8, 1 << 8);
#if MCLKFS_RATIO == 512
	/* i2s divisor: 0=no div; 1=div by 2; 2=div by 4; 3=div by 8. */
	aml_cbus_update_bits(AIU_CLK_CTRL, 0x3 << 2, 0x3 << 2);
#elif MCLKFS_RATIO == 256
	aml_cbus_update_bits(AIU_CLK_CTRL, 0x3 << 2, 0x2 << 2);
#else
	aml_cbus_update_bits(AIU_CLK_CTRL, 0x3 << 2, 0x1 << 2);
#endif
	/* enable I2S clock */
	aml_cbus_update_bits(AIU_CLK_CTRL, 1, 1);

	if (format == AUDIO_ALGOUT_DAC_FORMAT_LEFT_JUSTIFY)
		aml_cbus_update_bits(AIU_CLK_CTRL, 0x3 << 8, 0);

	if (dac_mute_const == 0x800000)
		aml_write_cbus(AIU_I2S_DAC_CFG, 0x000f);
	else
		/* Payload 24-bit, Msb first, alrclk = aoclk/64 */
		aml_write_cbus(AIU_I2S_DAC_CFG, 0x0007);

	/* four 2-channel */
#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE
	aml_write_cbus(AIU_I2S_SOURCE_DESC, (1 << 11));
#else
	aml_write_cbus(AIU_I2S_SOURCE_DESC, 0x0001);
#endif
}

/* set sclk and lrclk, mclk = 256fs. */
void audio_set_i2s_clk_div(void)
{
	/* aiclk source */
	aml_cbus_update_bits(AIU_CLK_CTRL, 1 << 10, 1 << 10);
	/* Set mclk over sclk ratio */
	aml_cbus_update_bits(AIU_CLK_CTRL_MORE, 0x3f << 8, (4 - 1) << 8);
	/* set dac/adc lrclk ratio over sclk----64fs */
	aml_cbus_update_bits(AIU_CODEC_DAC_LRCLK_CTRL, 0xfff, (64 - 1));
	aml_cbus_update_bits(AIU_CODEC_ADC_LRCLK_CTRL, 0xfff, (64 - 1));
	/* Enable sclk */
	aml_cbus_update_bits(AIU_CLK_CTRL_MORE, 1 << 14, 1 << 14);
}

void audio_set_spdif_clk_div(uint div)
{
	uint val = 0;

	if (div < 1 && div > 4)
		return;

	val = div - 1;
	/* 958 divisor: 0=no div; 1=div by 2; 2=div by 3; 3=div by 4. */
	aml_cbus_update_bits(AIU_CLK_CTRL, 3 << 4, val << 4);
	/* enable 958 divider */
	aml_cbus_update_bits(AIU_CLK_CTRL, 1 << 1, 1 << 1);
}

void audio_enable_output(int flag)
{
	if (flag) {
		aml_write_cbus(AIU_RST_SOFT, 0x05);
		aml_read_cbus(AIU_I2S_SYNC);
		aml_cbus_update_bits(AIU_MEM_I2S_CONTROL, 3 << 1, 3 << 1);

		/* Maybe cause POP noise */
		/* audio_i2s_unmute(); */
	} else {
		aml_cbus_update_bits(AIU_MEM_I2S_CONTROL, 3 << 1, 0);

		/* Maybe cause POP noise */
		/* audio_i2s_mute(); */
	}
	/* audio_out_enabled(flag); */
}

int if_audio_out_enable(void)
{
	return aml_read_cbus(AIU_MEM_I2S_CONTROL) & (0x3 << 1);
}
EXPORT_SYMBOL(if_audio_out_enable);

int if_958_audio_out_enable(void)
{
	return aml_read_cbus(AIU_MEM_IEC958_CONTROL) & (0x3 << 1);
}
EXPORT_SYMBOL(if_958_audio_out_enable);

unsigned int read_i2s_rd_ptr(void)
{
	unsigned int val;

	val = aml_read_cbus(AIU_MEM_I2S_RD_PTR);
	return val;
}

unsigned int read_iec958_rd_ptr(void)
{
	unsigned int val;

	val = aml_read_cbus(AIU_MEM_IEC958_RD_PTR);
	return val;
}

void aml_audio_i2s_unmute(void)
{
	aml_cbus_update_bits(AIU_I2S_MUTE_SWAP, 0xff << 8, 0);
}

void aml_audio_i2s_mute(void)
{
	aml_cbus_update_bits(AIU_I2S_MUTE_SWAP, 0xff << 8, 0xff << 8);
}

void audio_i2s_unmute(void)
{
	aml_cbus_update_bits(AIU_I2S_MUTE_SWAP, 0xff << 8, 0);
	aml_cbus_update_bits(AIU_958_CTRL, 0x3 << 3, 0 << 3);
}

void audio_i2s_mute(void)
{
	aml_cbus_update_bits(AIU_I2S_MUTE_SWAP, 0xff << 8, 0xff << 8);
	aml_cbus_update_bits(AIU_958_CTRL, 0x3 << 3, 0x3 << 3);
}

void audio_mute_left_right(unsigned int flag)
{
	if (flag == 0) {	/* right */
		aml_cbus_update_bits(AIU_958_CTRL, 0x3 << 3, 0x1 << 3);
	} else if (flag == 1) {	/* left */
		aml_cbus_update_bits(AIU_958_CTRL, 0x3 << 3, 0x2 << 3);
	}
}

void audio_hw_958_reset(unsigned int slow_domain, unsigned int fast_domain)
{
	aml_write_cbus(AIU_958_DCU_FF_CTRL, 0);
	aml_write_cbus(AIU_RST_SOFT, (slow_domain << 3) | (fast_domain << 2));
}

void audio_hw_958_raw(void)
{
	aml_write_cbus(AIU_958_MISC, 1);
	/* raw */
	aml_cbus_update_bits(AIU_MEM_IEC958_CONTROL, 1 << 8, 1 << 8);
	/* 8bit */
	aml_cbus_update_bits(AIU_MEM_IEC958_CONTROL, 1 << 7, 0);
	/* endian */
	aml_cbus_update_bits(AIU_MEM_IEC958_CONTROL, 0x7 << 3, 1 << 3);

	aml_write_cbus(AIU_958_BPF, IEC958_bpf);
	aml_write_cbus(AIU_958_BRST, IEC958_brst);
	aml_write_cbus(AIU_958_LENGTH, IEC958_length);
	aml_write_cbus(AIU_958_PADDSIZE, IEC958_padsize);
	/* disable int */
	aml_cbus_update_bits(AIU_958_DCU_FF_CTRL, 0x3 << 2, 0);

	if (IEC958_mode == 1) {	/* search in byte */
		aml_cbus_update_bits(AIU_958_DCU_FF_CTRL, 0x7 << 4, 0x7 << 4);
	} else if (IEC958_mode == 2) {	/* search in word */
		aml_cbus_update_bits(AIU_958_DCU_FF_CTRL, 0x7 << 4, 0x5 << 4);
	} else {
		aml_cbus_update_bits(AIU_958_DCU_FF_CTRL, 0x7 << 4, 0);
	}
	aml_write_cbus(AIU_958_CHSTAT_L0, IEC958_chstat0_l);
	aml_write_cbus(AIU_958_CHSTAT_L1, IEC958_chstat1_l);
	aml_write_cbus(AIU_958_CHSTAT_R0, IEC958_chstat0_r);
	aml_write_cbus(AIU_958_CHSTAT_R1, IEC958_chstat1_r);

	aml_write_cbus(AIU_958_SYNWORD1, IEC958_syncword1);
	aml_write_cbus(AIU_958_SYNWORD2, IEC958_syncword2);
	aml_write_cbus(AIU_958_SYNWORD3, IEC958_syncword3);
	aml_write_cbus(AIU_958_SYNWORD1_MASK, IEC958_syncword1_mask);
	aml_write_cbus(AIU_958_SYNWORD2_MASK, IEC958_syncword2_mask);
	aml_write_cbus(AIU_958_SYNWORD3_MASK, IEC958_syncword3_mask);

	pr_info("%s: %d\n", __func__, __LINE__);
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
		aml_write_cbus(AIU_958_CHSTAT_L0, set->chstat0_l);
		aml_write_cbus(AIU_958_CHSTAT_L1, set->chstat1_l);
		aml_write_cbus(AIU_958_CHSTAT_R0, set->chstat0_r);
		aml_write_cbus(AIU_958_CHSTAT_R1, set->chstat1_r);
	}
}

static void audio_hw_set_958_pcm24(struct _aiu_958_raw_setting_t *set)
{
	/* in pcm mode, set bpf to 128 */
	aml_write_cbus(AIU_958_BPF, 0x80);
	set_958_channel_status(set->chan_stat);
}

void audio_set_958_mode(unsigned int mode, struct _aiu_958_raw_setting_t *set)
{
	if (mode == AIU_958_MODE_PCM_RAW) {
		mode = AIU_958_MODE_PCM16;	/* use 958 raw pcm mode */
		aml_write_cbus(AIU_958_VALID_CTRL, 3);
	} else
		aml_write_cbus(AIU_958_VALID_CTRL, 0);

	if (mode == AIU_958_MODE_RAW) {

		audio_hw_958_raw();
		aml_write_cbus(AIU_958_MISC, 1);
		/* raw */
		aml_cbus_update_bits(AIU_MEM_IEC958_CONTROL,
				     1 << 8, 1 << 8);
		/* 8bit */
		aml_cbus_update_bits(AIU_MEM_IEC958_CONTROL, 1 << 7, 0);
		/* endian */
		aml_cbus_update_bits(AIU_MEM_IEC958_CONTROL,
				     0x7 << 3, 0x1 << 3);

		pr_info("IEC958 RAW\n");
	} else if (mode == AIU_958_MODE_PCM32) {
		audio_hw_set_958_pcm24(set);
#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE
		aml_write_cbus(AIU_958_MISC, 0x3480);
		/* pcm */
		aml_cbus_update_bits(AIU_MEM_IEC958_CONTROL, 1 << 8,
					1 << 8);
#else
		aml_write_cbus(AIU_958_MISC, 0x2020 | (1 << 7));
		/* pcm */
		aml_cbus_update_bits(AIU_MEM_IEC958_CONTROL, 1 << 8, 0);
#endif
		/* 16bit */
		aml_cbus_update_bits(AIU_MEM_IEC958_CONTROL, 1 << 7, 0);
		/* endian */
		aml_cbus_update_bits(AIU_MEM_IEC958_CONTROL,
				     0x7 << 3, 0);

		pr_info("IEC958 PCM32\n");
	} else if (mode == AIU_958_MODE_PCM24) {
		audio_hw_set_958_pcm24(set);
#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE
			aml_write_cbus(AIU_958_MISC, 0x3480);
			/* pcm */
			aml_cbus_update_bits(AIU_MEM_IEC958_CONTROL, 1 << 8,
						1 << 8);
#else
			aml_write_cbus(AIU_958_MISC, 0x2020 | (1 << 7));
			/* pcm */
			aml_cbus_update_bits(AIU_MEM_IEC958_CONTROL, 1 << 8, 0);
#endif
			/* 16bit */
			aml_cbus_update_bits(AIU_MEM_IEC958_CONTROL, 1 << 7, 0);
			/* endian */
			aml_cbus_update_bits(AIU_MEM_IEC958_CONTROL,
					     0x7 << 3, 0);

		pr_info("IEC958 24bit\n");
	} else if (mode == AIU_958_MODE_PCM16) {
		audio_hw_set_958_pcm24(set);
		aml_write_cbus(AIU_958_MISC, 0x2042);
		/* pcm */
#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE
		aml_cbus_update_bits(AIU_MEM_IEC958_CONTROL, 1 << 8,
					1 << 8);
#else
		aml_cbus_update_bits(AIU_MEM_IEC958_CONTROL, 1 << 8, 0);
#endif
		/* 16bit */
		aml_cbus_update_bits(AIU_MEM_IEC958_CONTROL,
				     1 << 7, 1 << 7);
		/* endian */
		aml_cbus_update_bits(AIU_MEM_IEC958_CONTROL,
				     0x7 << 3, 0);
		pr_info("IEC958 16bit\n");
	}

	audio_hw_958_reset(0, 1);

#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE
	if (mode == AIU_958_MODE_PCM32)
		aml_cbus_update_bits(AIU_958_DCU_FF_CTRL, 1 << 8, 1 << 8);
#endif

	aml_write_cbus(AIU_958_FORCE_LEFT, 1);
}

void audio_out_i2s_enable(unsigned int flag)
{
	if (flag) {
		aml_write_cbus(AIU_RST_SOFT, 0x01);
		aml_read_cbus(AIU_I2S_SYNC);
		aml_cbus_update_bits(AIU_MEM_I2S_CONTROL, 0x3 << 1, 0x3 << 1);
		/* Maybe cause POP noise */
		/* audio_i2s_unmute(); */
	} else {
		aml_cbus_update_bits(AIU_MEM_I2S_CONTROL, 0x3 << 1, 0);

		/* Maybe cause POP noise */
		/* audio_i2s_mute(); */
	}
	/* audio_out_enabled(flag); */
}

void audio_hw_958_enable(unsigned int flag)
{
	if (flag) {
		aml_write_cbus(AIU_RST_SOFT, 0x04);
		aml_write_cbus(AIU_958_FORCE_LEFT, 0);
		aml_cbus_update_bits(AIU_958_DCU_FF_CTRL, 1, 1);
		aml_cbus_update_bits(AIU_MEM_IEC958_CONTROL, 0x3 << 1,
				     0x3 << 1);
	} else {
		aml_write_cbus(AIU_RST_SOFT, 0x04);
		aml_write_cbus(AIU_958_FORCE_LEFT, 0);
		aml_write_cbus(AIU_958_DCU_FF_CTRL, 0);
		aml_cbus_update_bits(AIU_MEM_IEC958_CONTROL, 0x3 << 1,
				     0);
	}
}

unsigned int read_i2s_mute_swap_reg(void)
{
	unsigned int val;

	val = aml_read_cbus(AIU_I2S_MUTE_SWAP);
	return val;
}

void audio_i2s_swap_left_right(unsigned int flag)
{
    /*only LPCM output can set aiu hw channel swap*/
	if (IEC958_mode_codec == 0 || IEC958_mode_codec == 9)
		aml_cbus_update_bits(AIU_958_CTRL, 0x3 << 1, flag << 1);

	aml_cbus_update_bits(AIU_I2S_MUTE_SWAP, 0x3, flag);
	aml_cbus_update_bits(AIU_I2S_MUTE_SWAP, 0x3 << 2, flag << 2);
}

void audio_i2s_958_same_source(unsigned int same)
{
	aml_cbus_update_bits(AIU_I2S_MISC, 1 << 3, (!!same) << 3);
}

void set_hw_resample_source(int source)
{
	aml_cbus_update_bits(AUD_RESAMPLE_CTRL0, 1 << 29, source << 29);
}
EXPORT_SYMBOL(set_hw_resample_source);
#if 0
unsigned int audio_hdmi_init_ready(void)
{
	return READ_MPEG_REG_BITS(AIU_HDMI_CLK_DATA_CTRL, 0, 2);
}

/* power gate control for iec958 audio out */
unsigned int audio_spdifout_pg_enable(unsigned char enable)
{
	if (enable) {
		aml_cbus_update_bits(MPLL_958_CNTL, 1, 14, 1);
		AUDIO_CLK_GATE_ON(AIU_IEC958);
		AUDIO_CLK_GATE_ON(AIU_ICE958_AMCLK);
	} else {
		AUDIO_CLK_GATE_OFF(AIU_IEC958);
		AUDIO_CLK_GATE_OFF(AIU_ICE958_AMCLK);
		aml_cbus_update_bits(MPLL_958_CNTL, 0, 14, 1);
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
