/*
 * sound/soc/amlogic/meson/audio_hw_pcm.c
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
#define pr_fmt(fmt) "audio_hw_pcm" fmt

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/clk.h>
#include <sound/soc.h>

#include <linux/amlogic/iomap.h>
#include <linux/amlogic/media/sound/audin_regs.h>
#include "audio_hw_pcm.h"

#include <linux/amlogic/media/sound/audio_iomap.h>

static unsigned int pcmin_buffer_addr;
static unsigned int pcmin_buffer_size;

static unsigned int pcmout_buffer_addr;
static unsigned int pcmout_buffer_size;

static int valid_channel[] = {
	0x1,    /* slot number 1 */
	0x3,    /* slot number 2 */
	0x7,    /* slot number 3 */
	0xf,    /* slot number 4 */
	0x1f,    /* slot number 5 */
	0x3f,    /* slot number 6 */
	0x7f,    /* slot number 7 */
	0xff,    /* slot number 8 */
	0x1ff,    /* slot number 9 */
	0x3ff,    /* slot number 10 */
	0x7ff,    /* slot number 11 */
	0xfff,    /* slot number 12 */
	0x1fff,    /* slot number 13 */
	0x3fff,    /* slot number 14 */
	0x7fff,    /* slot number 15 */
	0xffff    /* slot number 16 */
};

/* counter for pcm clk used */
static int aml_pcm_clk_count;

static unsigned int aml_pcm_format = SND_SOC_DAIFMT_DSP_B;

void aml_set_pcm_format(int pcm_mode)
{
	pr_info(" %s, pcm format:0x%x\n", __func__, pcm_mode);

	aml_pcm_format = pcm_mode;
}

static uint32_t aml_audin_read_bits(uint32_t reg, const uint32_t start,
				   const uint32_t len)
{
	return (aml_audin_read(reg) >> start) & ((1L << len) - 1);
}

static void pcm_in_register_show(void)
{
	pr_debug("PCMIN registers show:\n");
	pr_debug("\tAUDIN_FIFO1_START(0x%04x): 0x%08x\n", AUDIN_FIFO1_START,
		  aml_audin_read(AUDIN_FIFO1_START));
	pr_debug("\tAUDIN_FIFO1_END(0x%04x):   0x%08x\n", AUDIN_FIFO1_END,
		  aml_audin_read(AUDIN_FIFO1_END));
	pr_debug("\tAUDIN_FIFO1_PTR(0x%04x):   0x%08x\n", AUDIN_FIFO1_PTR,
		  aml_audin_read(AUDIN_FIFO1_PTR));
	pr_debug("\tAUDIN_FIFO1_RDPTR(0x%04x): 0x%08x\n", AUDIN_FIFO1_RDPTR,
		  aml_audin_read(AUDIN_FIFO1_RDPTR));
	pr_debug("\tAUDIN_FIFO1_CTRL(0x%04x):  0x%08x\n", AUDIN_FIFO1_CTRL,
		  aml_audin_read(AUDIN_FIFO1_CTRL));
	pr_debug("\tAUDIN_FIFO1_CTRL1(0x%04x): 0x%08x\n", AUDIN_FIFO1_CTRL1,
		  aml_audin_read(AUDIN_FIFO1_CTRL1));
	pr_debug("\tPCMIN_CTRL0(0x%04x):       0x%08x\n", PCMIN_CTRL0,
		  aml_audin_read(PCMIN_CTRL0));
	pr_debug("\tPCMIN_CTRL1(0x%04x):       0x%08x\n", PCMIN_CTRL1,
		  aml_audin_read(PCMIN_CTRL1));
}

void pcm_master_in_enable(struct snd_pcm_substream *substream, int flag)
{
	unsigned int fs_offset;

	if (aml_pcm_format == SND_SOC_DAIFMT_DSP_A)
		fs_offset = 1;
	else {
		fs_offset = 0;

		if (aml_pcm_format != SND_SOC_DAIFMT_DSP_B)
			pr_err("Unsupport DSP mode\n");
	}

	/* reset fifo */
RESET_FIFO:
	aml_audin_update_bits(AUDIN_FIFO1_CTRL, 1 << 1, 1 << 1);
	aml_audin_write(AUDIN_FIFO1_PTR, 0);
	if (aml_audin_read(AUDIN_FIFO1_PTR) !=
			aml_audin_read(AUDIN_FIFO1_START))
		goto RESET_FIFO;

	aml_audin_update_bits(AUDIN_FIFO1_CTRL, 1 << 1, 0 << 1);

	/* reset pcmin */
	aml_audin_update_bits(PCMIN_CTRL0, 1 << 30, 1 << 30);
	aml_audin_update_bits(PCMIN_CTRL0, 1 << 30, 0 << 30);

	/* disable fifo */
	aml_audin_update_bits(AUDIN_FIFO1_CTRL, 1, 0);

	/* disable pcmin */
	aml_audin_update_bits(PCMIN_CTRL0, 1 << 31, 0 << 31);

	if (flag) {
		unsigned int pcm_mode = 1, pcm_wlen = 16;
		unsigned int num_slot = substream->runtime->channels;
		unsigned int valid_slot = valid_channel[num_slot - 1];
		unsigned int max_bits;
		unsigned int valid_bits;

		/* whatever pcm out is enable */
		aml_pcm_clk_count++;

		switch (substream->runtime->format) {
		case SNDRV_PCM_FORMAT_S32_LE:
			pcm_wlen = 32;
			break;
		case SNDRV_PCM_FORMAT_S24_LE:
			pcm_wlen = 24;
			break;
		case SNDRV_PCM_FORMAT_S16_LE:
			pcm_wlen = 16;
			break;
		case SNDRV_PCM_FORMAT_S8:
			pcm_wlen = 8;
			break;
		}

		pcm_mode = (pcm_wlen >> 3) - 1;
		max_bits = pcm_wlen - 1;
		valid_bits = pcm_wlen - 1;
		/* set buffer start ptr end */
		aml_audin_write(AUDIN_FIFO1_START, pcmin_buffer_addr);
		aml_audin_write(AUDIN_FIFO1_PTR, pcmin_buffer_addr);
		aml_audin_write(AUDIN_FIFO1_END,
			pcmin_buffer_addr + pcmin_buffer_size - 8);

		/* fifo control */
		aml_audin_write(AUDIN_FIFO1_CTRL,
			(1 << 15) |    /* urgent request */
			(1 << 11) |    /* channel */
			(4 << 8) |     /* endian */
			(2 << 3) |     /* PCMIN input selection */
			(1 << 2) |     /* load address */
			(0 << 1) |     /* reset fifo */
			(1 << 0)       /* fifo enable */
		);

		/* fifo control1 */
		aml_audin_write(AUDIN_FIFO1_CTRL1,
			/* data destination DDR */
			(0 << 4) |
			/* fifo1 din byte num.  00 : 1 byte. 01: 2 bytes.
			 * 10: 3 bytes. 11: 4 bytes
			 */
			(pcm_mode << 2) |
			/* data position */
			(((pcm_mode == 2) ? 1 : 0) << 0)
		);

		/* pcmin control1 */
		aml_audin_write(PCMIN_CTRL1,
			/* pcmin SRC sel */
			(0 << 29) |
			/* pcmin clock sel */
			(1 << 28) |
			/* using negedge of PCM clock to latch the input data */
			(1 << 27) |
			/* max slot number in one frame */
			(max_bits << 21) |
			/* valid bit number in one slot */
			(valid_bits << 16) |
			/* slot valid */
			(valid_slot << 0)
		);

		/* pcmin control0 */
		aml_audin_write(PCMIN_CTRL0,
			/* pcmin enable */
			(1 << 31) |
			/* sync on clock posedge */
			(1 << 29) |
			/* FS SKEW */
			(fs_offset << 16) |
			/* waithing 1 system clock cycles
			 * then sample the PCMIN singals
			 */
			(((pcm_mode == 2) ? 4 : 0) << 4) |
			/* use clock counter to do the sample */
			(0 << 3) |
			/* fs inverted. */
			(0 << 2) |
			/* msb first */
			(1 << 1) |
			/* left justified */
			(1 << 0)
		);

		if (!pcm_out_is_enable()) {
			unsigned int bit_offset_s = 0, slot_offset_s = 0,
				bit_offset_e = 0, slot_offset_e = 0;

			if (aml_pcm_format == SND_SOC_DAIFMT_DSP_A) {
				bit_offset_s = pcm_wlen - 1;
				slot_offset_s = num_slot - 1;
				bit_offset_e = 0;
				slot_offset_e = 0;
			} else if (aml_pcm_format == SND_SOC_DAIFMT_DSP_B) {
				bit_offset_s = 0;
				slot_offset_s = 0;
				bit_offset_e = 1;
				slot_offset_e = 0;
			} else
				pr_err("Unsupport DSP mode\n");

			pr_info("pcm master in, pcm mode:%d, valid bits:0x%x, valid slot:0x%x\n",
				pcm_mode,
				valid_bits,
				valid_slot);

			aml_audin_write(PCMOUT_CTRL2,
				/* underrun use mute constant */
				(0 << 29) |
				/* pcmo max slot number in one frame*/
				((num_slot - 1) << 22) |
				/* pcmo max bit number in one slot*/
				(valid_bits << 16) |
				(valid_slot << 0)
				);
			aml_audin_write(PCMOUT_CTRL1,
				/* pcmo output data byte number.  00 : 8bits.
				 * 01: 16bits. 10: 24bits. 11: 32bits
				 */
				(pcm_mode << 30) |
				/* use posedge of PCM clock to output data*/
				(0 << 28) |
				/* invert fs phase */
				(1 << 26) |
				/* invert the fs_o for master mode */
				(1 << 25) |
				/* fs_o start postion frame
				 * slot counter number
				 */
				(slot_offset_s << 18) |
				/*fs_o start postion slot bit counter number*/
				(bit_offset_s << 12) |
				/*fs_o end postion frame slot counter number.*/
				(slot_offset_e << 6) |
				/* fs_o end postion slot bit counter number.*/
				(bit_offset_e << 0)
			);
			aml_audin_update_bits(PCMOUT_CTRL0, 1 << 31, 1 << 31);
			aml_audin_update_bits(PCMOUT_CTRL0, 1 << 29, 1 << 29);
		}
	} else {
		aml_pcm_clk_count--;
		if (aml_pcm_clk_count <= 0) {
			/* disable pcmout */
			aml_audin_update_bits(PCMOUT_CTRL0, 1 << 31, 0 << 31);
		}
	}

	pr_debug("PCMIN %s\n", flag ? "enable" : "disable");
	pcm_in_register_show();
}


void pcm_in_enable(struct snd_pcm_substream *substream, int flag)
{
	unsigned int fs_offset;

	if (aml_pcm_format == SND_SOC_DAIFMT_DSP_A)
		fs_offset = 1;
	else {
		fs_offset = 0;

		if (aml_pcm_format != SND_SOC_DAIFMT_DSP_B)
			pr_err("Unsupport DSP mode\n");
	}
	/* reset fifo */
 RESET_FIFO:
	aml_audin_update_bits(AUDIN_FIFO1_CTRL, 1 << 1, 1 << 1);
	aml_audin_write(AUDIN_FIFO1_PTR, 0);
	if (aml_audin_read(AUDIN_FIFO1_PTR) !=
			aml_audin_read(AUDIN_FIFO1_START))
		goto RESET_FIFO;
	aml_audin_update_bits(AUDIN_FIFO1_CTRL, 1 << 1, 0 << 1);

	/* reset pcmin */
	aml_audin_update_bits(PCMIN_CTRL0, 1 << 30, 1 << 30);
	aml_audin_update_bits(PCMIN_CTRL0, 1 << 30, 0 << 30);

	/* disable fifo */
	aml_audin_update_bits(AUDIN_FIFO1_CTRL, 1, 0);

	/* disable pcmin */
	aml_audin_update_bits(PCMIN_CTRL0, 1 << 31, 0 << 31);

	if (flag) {
		unsigned int pcm_mode = 1;
		unsigned int num_slot = substream->runtime->channels;
		unsigned int valid_slot = valid_channel[num_slot - 1];
		unsigned int max_bits = 0xf;
		unsigned int valid_bits = 0xf;

		switch (substream->runtime->format) {
		case SNDRV_PCM_FORMAT_S32_LE:
			pcm_mode = 3;
			max_bits = 0x1f;
			valid_bits = 0x1f;
			break;
		case SNDRV_PCM_FORMAT_S24_LE:
			pcm_mode = 2;
			max_bits = 0x17;
			valid_bits = 0x17;

			//pcm_mode = 3;
			max_bits = 0x1f;
			break;
		case SNDRV_PCM_FORMAT_S16_LE:
			pcm_mode = 1;
			max_bits = 0xf;
			valid_bits = 0xf;
			break;
		case SNDRV_PCM_FORMAT_S8:
			max_bits = 0x7;
			valid_bits = 0x7;
			pcm_mode = 0;
			break;
		}
		/* set buffer start ptr end */
		aml_audin_write(AUDIN_FIFO1_START, pcmin_buffer_addr);
		aml_audin_write(AUDIN_FIFO1_PTR, pcmin_buffer_addr);
		aml_audin_write(AUDIN_FIFO1_END,
			       pcmin_buffer_addr + pcmin_buffer_size - 8);

		/* fifo control */
		/* urgent request */
		aml_audin_write(AUDIN_FIFO1_CTRL,
			(1 << 15) |
			(1 << 11) |	/* channel */
			(4 << 8) |	/* endian */
			(2 << 3) |	/* PCMIN input selection */
			(1 << 2) |	/* load address */
			(0 << 1) |	/* reset fifo */
			(1 << 0) /* fifo enable */
			);

		/* fifo control1 */
		/* data destination DDR */
		aml_audin_write(AUDIN_FIFO1_CTRL1,
			(0 << 4) |
			/* fifo1 din byte num.	00 : 1 byte. 01: 2 bytes.
			 *10: 3 bytes. 11: 4 bytes
			 */
			(pcm_mode << 2) |
			/* data position */
			(((pcm_mode == 2) ? 1 : 0) << 0)
			);

		/* pcmin control1 */
		aml_audin_write(PCMIN_CTRL1,
			/* external chip */
			(0 << 29) |
			/* external chip */
			(0 << 28) |
			/* using negedge of PCM clock to latch the input data */
			(1 << 27) |
			/* slot bit msb 16 clocks per slot */
			(max_bits << 21) |
			/* valid bit number in one slot  */
			(valid_bits << 16) |
			(valid_slot << 0)	/* slot valid */
			);

		/* pcmin control0 */
		aml_audin_write(PCMIN_CTRL0,
			(1 << 31) | /* pcmin enable */
			(1 << 29) |	/* sync on clock posedge */
			(fs_offset  << 16) | /* FS SKEW */
			/* waithing 1 system clock cycles
			 *	then sample the PCMIN singals
			 */
			(((pcm_mode == 2) ? 4 : 0) << 4) |
			(0 << 3) |	/* use clock counter to do the sample */
			(0 << 2) |	/* fs not inverted. H=left, L=right */
			(1 << 1) |	/* msb first */
			(1 << 0));	/* left justified */
	}

	pr_debug("PCMIN %s\n", flag ? "enable" : "disable");
	pcm_in_register_show();
}

void pcm_in_set_buf(unsigned int addr, unsigned int size)
{
	pcmin_buffer_addr = addr;
	pcmin_buffer_size = size;

	pr_debug("PCMIN buffer start: 0x%08x size: 0x%08x\n",
		  pcmin_buffer_addr, pcmin_buffer_size);
}

int pcm_in_is_enable(void)
{
	int value = aml_audin_read_bits(PCMIN_CTRL0, 31, 1);

	return value;
}

unsigned int pcm_in_rd_ptr(void)
{
	unsigned int value = aml_audin_read(AUDIN_FIFO1_RDPTR);

	pr_debug("PCMIN AUDIN_FIFO1_RDPTR: 0x%08x\n", value);

	return value;
}

unsigned int pcm_in_set_rd_ptr(unsigned int value)
{
	unsigned int old = aml_audin_read(AUDIN_FIFO1_RDPTR);

	aml_audin_write(AUDIN_FIFO1_RDPTR, value);
	pr_debug("PCMIN AUDIN_FIFO1_RDPTR: 0x%08x -> 0x%08x\n", old, value);

	return old;
}

unsigned int pcm_in_wr_ptr(void)
{
	unsigned int writing = 0;
	unsigned int written = 0;
	unsigned int value = 0;

	writing = aml_audin_read(AUDIN_FIFO1_PTR);

	aml_audin_write(AUDIN_FIFO1_PTR, 1);
	written = aml_audin_read(AUDIN_FIFO1_PTR);
	pr_debug("PCMIN AUDIN_FIFO1_PTR: 0x%08x (0x%08x)\n", written, writing);

	/* value = written; */
	value = written & (~0x07);
	return value;
}

unsigned int pcm_in_fifo_int(void)
{
	unsigned int value = 0;

	value = aml_audin_read(AUDIN_FIFO_INT);
	pr_debug("PCMIN AUDIN_FIFO_INT: 0x%08x\n", value);

	return value;
}

static void pcm_out_register_show(void)
{
	pr_debug("PCMOUT registers show:\n");
	pr_debug("\tAUDOUT_BUF0_STA(0x%04x):  0x%08x\n", AUDOUT_BUF0_STA,
		  aml_audin_read(AUDOUT_BUF0_STA));
	pr_debug("\tAUDOUT_BUF0_EDA(0x%04x):  0x%08x\n", AUDOUT_BUF0_EDA,
		  aml_audin_read(AUDOUT_BUF0_EDA));
	pr_debug("\tAUDOUT_BUF0_WPTR(0x%04x): 0x%08x\n", AUDOUT_BUF0_WPTR,
		  aml_audin_read(AUDOUT_BUF0_WPTR));
	pr_debug("\tAUDOUT_FIFO_RPTR(0x%04x): 0x%08x\n", AUDOUT_FIFO_RPTR,
		  aml_audin_read(AUDOUT_FIFO_RPTR));
	pr_debug("\tAUDOUT_CTRL(0x%04x):      0x%08x\n", AUDOUT_CTRL,
		  aml_audin_read(AUDOUT_CTRL));
	pr_debug("\tAUDOUT_CTRL1(0x%04x):     0x%08x\n", AUDOUT_CTRL1,
		  aml_audin_read(AUDOUT_CTRL1));
	pr_debug("\tPCMOUT_CTRL0(0x%04x):     0x%08x\n", PCMOUT_CTRL0,
		  aml_audin_read(PCMOUT_CTRL0));
	pr_debug("\tPCMOUT_CTRL1(0x%04x):     0x%08x\n", PCMOUT_CTRL1,
		  aml_audin_read(PCMOUT_CTRL1));
	pr_debug("\tPCMOUT_CTRL2(0x%04x):     0x%08x\n", PCMOUT_CTRL2,
		  aml_audin_read(PCMOUT_CTRL2));
	pr_debug("\tPCMOUT_CTRL3(0x%04x):     0x%08x\n", PCMOUT_CTRL3,
		  aml_audin_read(PCMOUT_CTRL3));
}

void pcm_master_out_enable(struct snd_pcm_substream *substream, int flag)
{
	unsigned int pcm_mode = 1, pcm_wlen = 16;
	unsigned int num_slot = substream->runtime->channels;
	unsigned int valid_slot = valid_channel[num_slot - 1];
	unsigned int valid_bits;
	unsigned int bit_offset_s = 0, slot_offset_s = 0,
			bit_offset_e = 0, slot_offset_e = 0;

	switch (substream->runtime->format) {
	case SNDRV_PCM_FORMAT_S32_LE:
		pcm_wlen = 32;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		pcm_wlen = 24;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
		pcm_wlen = 16;
		break;
	case SNDRV_PCM_FORMAT_S8:
		pcm_wlen = 8;
		break;
	}

	pcm_mode = (pcm_wlen >> 3) - 1;
	valid_bits = pcm_wlen - 1;

	if (aml_pcm_format == SND_SOC_DAIFMT_DSP_A) {
		bit_offset_s = pcm_wlen - 1;
		slot_offset_s = num_slot - 1;
		bit_offset_e = 0;
		slot_offset_e = 0;
	} else if (aml_pcm_format == SND_SOC_DAIFMT_DSP_B) {
		bit_offset_s = 0;
		slot_offset_s = 0;
		bit_offset_e = 1;
		slot_offset_e = 0;
	} else
		pr_err("Unsupport DSP mode\n");

	pr_info("pcm master out, pcm mode:%d, valid bits:0x%x, valid slot:0x%x\n",
		pcm_mode,
		valid_bits,
		valid_slot);

	/* reset fifo */
	aml_audin_update_bits(AUDOUT_CTRL, 1 << 30, 1 << 30);
	aml_audin_update_bits(AUDOUT_CTRL, 1 << 30, 1 << 30);
	/* disable fifo */
	aml_audin_update_bits(AUDOUT_CTRL, 1 << 31, 0 << 31);

	if (!pcm_in_is_enable()) {
		/* reset pcmout */
		aml_audin_update_bits(PCMOUT_CTRL0, 1 << 30, 1 << 30);
		aml_audin_update_bits(PCMOUT_CTRL0, 1 << 30, 0 << 30);
		/* disable pcmout */
		aml_audin_update_bits(PCMOUT_CTRL0, 1 << 31, 0 << 31);
	}

	if (flag) {
		/* set buffer start ptr end */
		aml_audin_write(AUDOUT_BUF0_STA, pcmout_buffer_addr);
		aml_audin_write(AUDOUT_BUF0_WPTR, pcmout_buffer_addr);
		aml_audin_write(AUDOUT_BUF0_EDA,
			pcmout_buffer_addr + pcmout_buffer_size - 8);

		/* fifo control */
		aml_audin_write(AUDOUT_CTRL,
			(0 << 31) |/* fifo enable */
			(0 << 30) |     /* soft reset */
			(1 << 29) |     /* load address */
			/* use cbus AUDOUT BUFFER0 write pointer
			 * as the AUDOUT FIFO write pointer
			 */
			(0 << 22) |
			(52 << 15) |    /* data request size */
			(64 << 8) |     /* buffer level to keep */
			(0 << 7) |      /* buffer level control */
			(1 << 6) |      /* DMA mode */
			(1 << 5) |      /* circular buffer */
			(0 << 4) |      /* use register set 0 always */
			(1 << 3) |      /* urgent request */
			(4 << 0)        /* endian */
		);

		aml_audin_write(AUDOUT_CTRL,
			(1 << 31) |/* fifo enable */
			(0 << 30) |     /* soft reset */
			(1 << 29) |     /* load address */
			/* use cbus AUDOUT BUFFER0 write pointer
			 * as the AUDOUT FIFO write pointer
			 */
			(1 << 22) |
			(96 << 15) |    /* data request size */
			(96 << 8) |     /* buffer level to keep */
			(1 << 7) |      /* buffer level control */
			(1 << 6) |      /* DMA mode */
			(1 << 5) |      /* circular buffer */
			(0 << 4) |      /* use register set 0 always */
			(1 << 3) |       /* urgent request */
			(4 << 0)         /* endian */
		);

		/* pcmout control3 */
		aml_audin_write(PCMOUT_CTRL3, 0); /* mute constant */

		/* pcmout control2 */
		/* FS * valid bit * slot num = BCLK */
		aml_audin_write(PCMOUT_CTRL2,
			/* underrun use mute constant */
			(0 << 29) |
			/* pcmo max slot number in one frame */
			((num_slot - 1) << 22) |
			/* pcmo max bit number in one slot */
			(valid_bits << 16) |
			/* pcmo valid slot. each bit for one slot */
			(valid_slot << 0)
		);

		/* pcmout control1 */
		aml_audin_write(PCMOUT_CTRL1,
			/* pcmo output data byte number.  00 : 8bits.
			 * 01: 16bits. 10: 24bits. 11: 32bits
			 */
			(pcm_mode << 30) |
			/* use posedge of PCM clock to output data */
			(0 << 28) |
			/* pcmo slave parts clock invert */
			(0 << 27) |
			/* invert fs phase */
			(1 << 26) |
			/* invert the fs_o for master mode */
			(1 << 25) |
			/* fs_o start postion frame slot counter number */
			(slot_offset_s << 18) |
			/* fs_o start postion slot bit counter number.*/
			(bit_offset_s << 12) |
			/* fs_o end postion frame slot counter number. */
			(slot_offset_e << 6) |
			/* fs_o end postion slot bit counter number. */
			(bit_offset_e << 0)
		);

		/* pcmout control0 */
		aml_audin_write(PCMOUT_CTRL0,
			(1 << 31) |     /* enable */
			(1 << 29) |     /* master */
			(1 << 28) |     /* sync on clock rising edge */
			/* system clock sync at clock edge of pcmout clock.
			 * 0 = sync on clock counter.
			 */
			(0 << 27) |
			/* system clock sync at counter number
			 * if sync on clock counter
			 */
			(((pcm_mode == 2) ? 4 : 0) << 15) |
			(1 << 14) |     /* msb first */
			(1 << 13) |     /* left justified */
			/* data position */
			(((pcm_mode == 2) ? 1 : 0) << 12) |
			/*slave mode, sync fs with the slot bit counter.*/
			(0 << 6) |
			/*slave mode, sync fs with frame slot counter.*/
			(0 << 0)
		);

		aml_pcm_clk_count++;
	} else {
		aml_pcm_clk_count--;
		if (aml_pcm_clk_count <= 0) {
			/* disable pcmout */
			aml_audin_update_bits(PCMOUT_CTRL0, 1 << 31, 0 << 31);
		}
	}

	pr_debug("PCMOUT %s\n", flag ? "enable" : "disable");
	pcm_out_register_show();
}

void pcm_out_enable(struct snd_pcm_substream *substream, int flag)
{
	unsigned int pcm_mode = 1;
	unsigned int num_slot = substream->runtime->channels;
	unsigned int valid_slot = valid_channel[num_slot - 1];
	unsigned int valid_bits = 0xf;
	unsigned int bit_offset_s, slot_offset_s, bit_offset_e, slot_offset_e;

	if (aml_pcm_format == SND_SOC_DAIFMT_DSP_A) {
		bit_offset_s = 0xF;
		slot_offset_s = num_slot - 1;
		bit_offset_e = 0;
		slot_offset_e = 0;
	} else {
		if (aml_pcm_format != SND_SOC_DAIFMT_DSP_B)
			pr_err("Unsupport DSP mode\n");

		bit_offset_s = 0;
		slot_offset_s = 0;
		bit_offset_e = 0;
		slot_offset_e = 1;
	}

	switch (substream->runtime->format) {
	case SNDRV_PCM_FORMAT_S32_LE:
		pcm_mode = 3;
		valid_bits = 0x1f;
		if (aml_pcm_format == SND_SOC_DAIFMT_DSP_A) {
			bit_offset_s = 0xF;
			slot_offset_s = (num_slot << 1) - 1;
			bit_offset_e = 0;
			slot_offset_e = 0;
		}
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		pcm_mode = 2;
		valid_bits = 0x1f;
		if (aml_pcm_format == SND_SOC_DAIFMT_DSP_A) {
			bit_offset_s = 0xF;
			slot_offset_s = (num_slot << 1) - 8 - 1;
			bit_offset_e = 0;
			slot_offset_e = 0;
		}
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
		pcm_mode = 1;
		valid_bits = 0xf;
		break;
	case SNDRV_PCM_FORMAT_S8:
		pcm_mode = 0;
		valid_bits = 0x7;
		if (aml_pcm_format == SND_SOC_DAIFMT_DSP_A) {
			bit_offset_s = 0x7;
			slot_offset_s = (num_slot >> 1) - 1;
			bit_offset_e = 0;
			slot_offset_e = 0;
		}
		break;
	}

	pr_info("pcm out, pcm mode:%d, valid bits:0x%x, valid slot:0x%x\n",
		pcm_mode,
		valid_bits,
		valid_slot);

	/* reset fifo */
	aml_audin_update_bits(AUDOUT_CTRL, 1 << 30, 1 << 30);
	aml_audin_update_bits(AUDOUT_CTRL, 1 << 30, 1 << 30);

	/* reset pcmout */
	aml_audin_update_bits(PCMOUT_CTRL0, 1 << 30, 1 << 30);
	aml_audin_update_bits(PCMOUT_CTRL0, 1 << 30, 0 << 30);

	/* disable fifo */
	aml_audin_update_bits(AUDOUT_CTRL, 1 << 31, 0 << 31);

	/* disable pcmout */
	aml_audin_update_bits(PCMOUT_CTRL0, 1 << 31, 0 << 31);

	if (flag) {
		/* set buffer start ptr end */
		aml_audin_write(AUDOUT_BUF0_STA, pcmout_buffer_addr);
		aml_audin_write(AUDOUT_BUF0_WPTR, pcmout_buffer_addr);
		aml_audin_write(AUDOUT_BUF0_EDA,
			       pcmout_buffer_addr + pcmout_buffer_size - 8);

		/* fifo control */
		aml_audin_write(AUDOUT_CTRL,
			(0 << 31) |	/* fifo enable */
			(0 << 30) |	/* soft reset */
			(1 << 29) |	/* load address */
			/* use cbus AUDOUT BUFFER0 write pointer
			 * as the AUDOUT FIFO write pointer
			 */
			(0 << 22) |
			(52 << 15) |	/* data request size */
			(64 << 8) |	/* buffer level to keep */
			(0 << 7) |	/* buffer level control */
			(1 << 6) |	/* DMA mode */
			(1 << 5) |	/* circular buffer */
			(0 << 4) |	/* use register set 0 always */
			(1 << 3) |	/* urgent request */
			(4 << 0));	/* endian */

		aml_audin_write(AUDOUT_CTRL,
			(1 << 31) |	/* fifo enable */
			(0 << 30) |	/* soft reset */
			(0 << 29) |	/* load address */
			/* use cbus AUDOUT BUFFER0 write pointer
			 * as the AUDOUT FIFO write pointer
			 */
			(0 << 22) |
			(52 << 15) |	/* data request size */
			(64 << 8) |	/* buffer level to keep */
			(0 << 7) |	/* buffer level control */
			(1 << 6) |	/* DMA mode */
			(1 << 5) |	/* circular buffer */
			(0 << 4) |	/* use register set 0 always */
			(1 << 3) |	/* urgent request */
			(4 << 0));	/* endian */

		/* pcmout control3 */
		aml_audin_write(PCMOUT_CTRL3, 0);	/* mute constant */

		/* pcmout control2 */
		/* 1 channel per frame */
		aml_audin_write(PCMOUT_CTRL2,
			(0 << 29) |
			((num_slot - 1) << 22) |
			(valid_bits << 16) |	/* 16 bits per slot */
			(valid_slot << 0)	/* enable 1 slot */
			);

		/* pcmout control1 */
		/* use posedge of PCM clock to output data */
		aml_audin_write(PCMOUT_CTRL1,
			(pcm_mode << 30) |
			(0 << 28) |
			/* use posedge of pcm clock to check the fs */
			(0 << 27));

		/* pcmout control0 */
		/* slave */
		aml_audin_write(PCMOUT_CTRL0,
			(1 << 31) |
			(0 << 29) |
			/* sync on clock rising edge */
			(1 << 28) |
			/* data sample mode */
			(0 << 27) |
			/* sync on 4 system clock later ? */
			(((pcm_mode == 2) ? 4 : 0) << 15) |
			/* msb first */
			(1 << 14) |
			/* left justified */
			(1 << 13) |
			/* data position */
			(((pcm_mode == 2) ? 1 : 0) << 12) |
			/* sync fs with the slot bit counter. */
			(3 << 6) |
			/* sync fs with frame slot counter. */
			(0 << 0));
	}

	pr_debug("PCMOUT %s\n", flag ? "enable" : "disable");
	pcm_out_register_show();
}

void pcm_out_mute(int flag)
{
	int value = flag ? 1 : 0;

	aml_audin_update_bits(PCMOUT_CTRL2, 1 << 31, value << 31);
}

void pcm_out_set_buf(unsigned int addr, unsigned int size)
{
	pcmout_buffer_addr = addr;
	pcmout_buffer_size = size;

	pr_debug("PCMOUT buffer addr: 0x%08x end: 0x%08x\n",
		  pcmout_buffer_addr, pcmout_buffer_size);
}

int pcm_out_is_enable(void)
{
	int value = aml_audin_read_bits(PCMOUT_CTRL0, 31, 1);

	return value;
}

int pcm_out_is_mute(void)
{
	int value = aml_audin_read_bits(PCMOUT_CTRL2, 31, 1);

	return value;
}

unsigned int pcm_out_rd_ptr(void)
{
	unsigned int value = aml_audin_read(AUDOUT_FIFO_RPTR);

	pr_debug("PCMOUT read pointer: 0x%08x\n", value);

	return value;
}

unsigned int pcm_out_wr_ptr(void)
{
	unsigned int value = 0;

	value = aml_audin_read(AUDOUT_BUF0_WPTR);
	pr_debug("PCMOUT write pointer: 0x%08x\n", value);
	return value;
}

unsigned int pcm_out_set_wr_ptr(unsigned int value)
{
	unsigned int old = aml_audin_read(AUDOUT_BUF0_WPTR);

	aml_audin_write(AUDOUT_BUF0_WPTR, value);
	pr_debug("PCMOUT write pointer: 0x%08x -> 0x%08x\n", old, value);

	return old;
}
