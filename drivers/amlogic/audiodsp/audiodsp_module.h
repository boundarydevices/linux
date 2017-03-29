/*
 * drivers/amlogic/audiodsp/audiodsp_module.h
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

#ifndef AUDIO_DSP_MODULES_H
#define AUDIO_DSP_MODULES_H
#include <linux/device.h>
#include <linux/timer.h>
#if 0 /* tmp_mask_for_kernel_4_4 */
#include <linux/wakelock.h>
#endif

/*
 * #include <asm/dsp/audiodsp_control.h>
 * #include <asm/dsp/dsp_register.h>
 */
#include "audiodsp_control.h"
#include <linux/amlogic/media/sound/dsp_register.h>

#include "codec_message.h"
#include <linux/dma-mapping.h>
struct audiodsp_priv {
	struct class *class;
	struct device *dev;
	struct device *micro_dev;
	struct timer_list dsp_mointer;
	struct list_head mcode_list;
	int mcode_id;
	spinlock_t mcode_lock;
	int code_mem_size;
	struct mail_msg *mailbox_reg;
	struct mail_msg *mailbox_reg2;
	struct dsp_working_info *dsp_work_details;
	unsigned long dsp_code_start;
	unsigned long dsp_code_size;
	void *dsp_stack_start;
	unsigned long dsp_stack_size;
	void *dsp_gstack_start;
	unsigned long dsp_gstack_size;
	unsigned long dsp_heap_start;
	unsigned long dsp_heap_size;
	struct mutex dsp_mutex;
	char dsp_codename[32];
	unsigned long dsp_start_time;	/* system jiffies */
	unsigned long dsp_end_time;	/* system jiffies */
	int stream_fmt;
	int last_stream_fmt;
	struct frame_fmt frame_format;
	struct frame_info cur_frame_info;
	unsigned int last_valid_pts;
	int out_len_after_last_valid_pts;
	int decode_error_count;
	int decode_fatal_err;
	int dsp_is_started;
	void *stream_buffer_mem;
	int stream_buffer_mem_size;
	int decoded_nb_frames;
	int first_lookup_over;
	int format_wait_count;
	unsigned long stream_buffer_start;
	unsigned long stream_buffer_end;
	unsigned long stream_buffer_size;
	struct mutex stream_buffer_mutex;
	struct completion decode_completion;
	void __iomem *p;

	/* for power management */
#if 0 /* tmp_mask_for_kernel_4_4 */
	struct wake_lock wakelock;
#endif
	unsigned int dsp_abnormal_count;
	unsigned int last_ablevel;
	unsigned int last_pcmlevel;
};

struct audiodsp_priv *audiodsp_privdata(void);

#define DSP_PRNT(fmt, args...)  pr_info("[dsp]" fmt, ##args)
extern void tsync_pcr_recover(void);
extern void tsync_pcr_recover(void);
extern unsigned int IEC958_mode_raw;
extern unsigned int IEC958_mode_codec;
/*extern int decopt; */
extern struct audio_info *get_audio_info(void);
extern void aml_alsa_hw_reprepare(void);
extern void dsp_get_debug_interface(int flag);
/* extern void audiodsp_moniter(unsigned long); */

#endif				/*  */
