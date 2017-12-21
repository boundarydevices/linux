/*
 * sound/soc/amlogic/meson/i2s.h
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

#ifndef __AML_I2S_H__
#define __AML_I2S_H__

/* #define debug_printk */
#ifdef debug_printk
#define dug_printk(fmt, args...)  printk(fmt, ## args)
#else
#define dug_printk(fmt, args...)
#endif

/*hw timer, timer D*/
#define BASE_IRQ                (32)
#define AM_IRQ(reg)             (reg + BASE_IRQ)
#define INT_TIMER_D             AM_IRQ(29)
/* note: we use TIEMRD. MODE: 1: periodic, 0: one-shot*/
#define TIMERD_MODE             1
/* timerbase resolution: 00: 1us; 01: 10us; 10: 100us; 11: 1ms*/
#define TIMERD_RESOLUTION       0x1
/* timer count: 16bits*/
#define TIMER_COUNT             100

struct audio_stream {
	int stream_id;
	unsigned int last_ptr;
	unsigned int size;
	unsigned int sample_rate;
	unsigned int I2S_addr;
	spinlock_t lock;
	struct snd_pcm_substream *stream;
	unsigned int i2s_mode; /* 0:master, 1:slave, */
	unsigned int device_type;
};
struct aml_audio_buffer {
	void *buffer_start;
	unsigned int buffer_size;
#ifndef CONFIG_AMLOGIC_SND_SPLIT_MODE
	char cache_buffer_bytes[256];
	int cached_len;
#endif
	int find_start;
	int invert_flag;
	int cached_sample;
};

struct aml_i2s_dma_params {
	char *name;			/* stream identifier */
	struct snd_pcm_substream *substream;
	void (*dma_intr_handler)(u32, struct snd_pcm_substream *);
};
struct aml_dai_info {
	unsigned int i2s_mode; /* 0:master, 1:slave, */
};
enum {
	I2S_MASTER_MODE = 0,
	I2S_SLAVE_MODE,
};
/*--------------------------------------------------------------------------
 * Data types
 *--------------------------------------------------------------------------
 */
struct aml_runtime_data {
	struct aml_i2s_dma_params *params;
	dma_addr_t dma_buffer;		/* physical address of dma buffer */
	dma_addr_t dma_buffer_end;	/* first address beyond DMA buffer */

	struct snd_pcm *pcm;
	struct snd_pcm_substream *substream;
	struct audio_stream s;
	struct timer_list timer;	/* timeer for playback and capture */
	spinlock_t timer_lock;
	void *buf; /* tmp buffer for playback or capture */
	int active;
	unsigned int xrun_num;

	/* hrtimer */
	struct hrtimer hrtimer;
	ktime_t wakeups_per_second;
};

#ifdef CONFIG_AMLOGIC_AMAUDIO2
extern int amaudio2_enable;
extern int amaudio2_read_enable;
extern int cache_pcm_write(char __user *buf, int size);
extern int get_pcm_cache_space(void);
#endif

#endif
