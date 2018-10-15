/*
 * drivers/amlogic/amaudio2/amaudio2.c
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

#define pr_fmt(fmt) "amaudio2: " fmt

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/of_device.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif
/* Amlogic headers */
#include <linux/amlogic/major.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/media/sound/aiu_regs.h>
#include <linux/amlogic/media/sound/audin_regs.h>
#include <linux/amlogic/media/sound/audio_iomap.h>

/*#define AMAUDIO2_DEBUG*/
/*#define AMAUDIO2_USE_IRQ*/

#include "amaudio2.h"

#define BASE_IRQ                        (32)
#define AM_IRQ(reg)                     (reg + BASE_IRQ)
#define INT_I2S_DDR                     AM_IRQ(48)
#define INT_AUDIO_IN                    AM_IRQ(7)
#define IRQ_OUT                         INT_I2S_DDR
#define AMAUDIO2_DEVICE_COUNT           ARRAY_SIZE(amaudio_ports)

#define SOFT_BUFFER_SIZE                (PAGE_SIZE * 4 * 8)
#define MAX_LATENCY                     (64 * 32 * 3)
#define MIN_LATENCY                     (64 * 32)
/*max size is SOFT_BUFFER_SIZE*/
#define BUFFER_THRESHOLD_DEFAULT        (4096 * 3)
/*irq size: min 2, max 32*/
#define INT_NUM                         (16)
#define I2S_BLOCK                       (64)
#define INT_BLOCK                       ((INT_NUM)*(I2S_BLOCK))

static unsigned char *amaudio_start_addr;
static dma_addr_t amaudio_phy_start_addr;
static struct device *amaudio_dev;
static struct class *amaudio_classp;

int direct_left_gain = 256;
int direct_right_gain = 256;
int music_gain;
int audio_out_mode = 2;

int amaudio2_enable;
EXPORT_SYMBOL(amaudio2_enable);
int amaudio2_read_enable;
EXPORT_SYMBOL(amaudio2_read_enable);

static unsigned long playback_data_cache_start;
static unsigned long playback_data_cache_buf_size;
static unsigned long playback_data_cache_rp;
static unsigned long playback_data_cache_wp;
static void *playback_tmp_start;
static unsigned int amaudio_delay;
struct mutex amaudio2_utils_lock;

static int external_mute_flag;
static int external_mute_enable = 1;

static int int_num = INT_NUM;
static int i2s_num = I2S_BLOCK;
static int int_block = INT_BLOCK;
static int soft_buffer_threshold = BUFFER_THRESHOLD_DEFAULT;
static unsigned int latency = MIN_LATENCY * 2;

void (*aml_cover_memcpy)(struct BUF *des, int a, struct BUF *src, int b,
				unsigned int count) = cover_memcpy;
void (*aml_direct_mix_memcpy)(struct BUF *des, int a, struct BUF *src, int b,
				unsigned int count) = direct_mix_memcpy;
void (*aml_inter_mix_memcpy)(struct BUF *des, int a, struct BUF *src, int b,
			  unsigned int count) = inter_mix_memcpy;

static const struct file_operations amaudio_fops = {
	.owner		= THIS_MODULE,
	.open		= amaudio_open,
	.release	= amaudio_release,
	.unlocked_ioctl = amaudio_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= amaudio_compat_ioctl,
#endif
};

static const struct file_operations amaudio_out_fops = {
	.owner		= THIS_MODULE,
	.open		= amaudio_open,
	.release	= amaudio_release,
	.mmap		= amaudio_mmap,
	.unlocked_ioctl = amaudio_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= amaudio_compat_ioctl,
#endif
};

static const struct file_operations amaudio_in_fops = {
	.owner		= THIS_MODULE,
	.open		= amaudio_open,
	.release	= amaudio_release,
	.mmap		= amaudio_mmap,
	.unlocked_ioctl = amaudio_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= amaudio_compat_ioctl,
#endif
};

static const struct file_operations amaudio_ctl_fops = {
	.owner		= THIS_MODULE,
	.open		= amaudio_open,
	.release	= amaudio_release,
	.unlocked_ioctl = amaudio_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= amaudio_compat_ioctl,
#endif
};

static const struct file_operations amaudio_utils_fops = {
	.owner		= THIS_MODULE,
	.open		= amaudio_open,
	.release	= amaudio_release,
	.read		= amaudio_read,
	.unlocked_ioctl = amaudio_utils_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= amaudio_compat_utils_ioctl,
#endif
};

static struct amaudio_port_t amaudio_ports[] = {
	{
		.name = "amaudio2_out",
		.fops = &amaudio_out_fops,
	},
	{
		.name = "amaudio2_in",
		.fops = &amaudio_in_fops,
	},
	{
		.name = "amaudio2_ctl",
		.fops = &amaudio_ctl_fops,
	},
	{
		.name = "amaudio2_utils",
		.fops = &amaudio_utils_fops,
	},
};

static inline int16_t clip16(int x)
{
	if (x < -32768)
		return -32768;
	else if (x > 32767)
		return 32767;

	return (int16_t)x;
}

static inline int32_t clip24(int x)
{
	if (x < -8388608)
		return -8388608;
	else if (x > 8388607)
		return 8388607;

	return (int32_t)x;
}

static inline int32_t clip32(long x)
{
	if (x < -2147483648LL)
		return (int32_t)(-2147483648LL);
	else if (x > 2147483647LL)
		return (int32_t)(2147483647LL);

	return (int32_t)x;
}

int External_Mute(int mute_flag)
{
	if (mute_flag == 1) {
		external_mute_flag = 1;
		pr_info("amaudio2_out external mute!\n");
	} else {
		external_mute_flag = 0;
		pr_info("amaudio2_out external unmute!\n");
	}
	return 0;
}
EXPORT_SYMBOL(External_Mute);

static int amaudio_open(struct inode *inode, struct file *file)
{
	struct amaudio_port_t *this = &amaudio_ports[iminor(inode)];
	struct amaudio_t *amaudio =
			kzalloc(sizeof(struct amaudio_t), GFP_KERNEL);
	int res = 0;

	if (iminor(inode) == 0) {
		pr_debug("amaudio2_out open!\n");
		of_dma_configure(amaudio_dev, amaudio_dev->of_node);

		int_num = INT_NUM;
		i2s_num = I2S_BLOCK;
		int_block = INT_BLOCK;
		soft_buffer_threshold = BUFFER_THRESHOLD_DEFAULT;
		latency = MIN_LATENCY * 2;
		aml_cover_memcpy = cover_memcpy;
		aml_direct_mix_memcpy = direct_mix_memcpy;
		aml_inter_mix_memcpy = inter_mix_memcpy;

		if (aml_i2s_playback_channel == 8) {
			int_num *= 2;
			i2s_num *= 4;
			int_block = int_num * i2s_num;
			soft_buffer_threshold *= 8;
			latency *= 8;
			aml_cover_memcpy = cover_memcpy_8_channel;
			aml_direct_mix_memcpy = direct_mix_memcpy_8_channel;
			aml_inter_mix_memcpy = inter_mix_memcpy_8_channel;
		}

		amaudio->sw.addr = amaudio_start_addr;
		amaudio->sw.paddr = amaudio_phy_start_addr;
		amaudio->sw.size = SOFT_BUFFER_SIZE;

		amaudio->hw.addr = (unsigned char *)aml_i2s_playback_start_addr;
		amaudio->hw.paddr = aml_i2s_playback_phy_start_addr;
		amaudio->hw.size = get_i2s_out_size();
		amaudio->hw.rd = get_i2s_out_ptr();

#ifdef AMAUDIO2_USE_IRQ
		spin_lock_init(&amaudio->sw.lock);
		spin_lock_init(&amaudio->hw.lock);

		if (request_irq(IRQ_OUT, i2s_out_callback, IRQF_SHARED,
				"i2s_out", amaudio)) {
			res = -EINVAL;
			pr_err("amaudio2 request irq error!\n");
			goto error;
		}
#else
		mutex_init(&amaudio->sw.lock);
		mutex_init(&amaudio->hw.lock);

		if (request_threaded_irq(IRQ_OUT, NULL, i2s_out_callback,
				IRQF_SHARED | IRQF_ONESHOT,
				"i2s_out", (void *)amaudio)) {
			res = -EINVAL;
			pr_err("amaudio2 request irq error!\n");
			goto error;
		}
#endif

		aml_aiu_update_bits(AIU_MEM_I2S_MASKS, 0xffff << 16,
				int_num << 16);

		amaudio2_enable = 1;
		 // pr_info("channel: %d, int_num = %d,"
		 // "int_block = %d, amaudio->hw.size = %d\n",
		 // aml_i2s_playback_channel, int_num,
		 // int_block, amaudio->hw.size);

	} else if (iminor(inode) == 1) {
		pr_debug("amaudio2_in opened\n");
		of_dma_configure(amaudio_dev, amaudio_dev->of_node);
	} else if (iminor(inode) == 2) {
		pr_info("amaudio2_ctl opened\n");
	} else if (iminor(inode) == 3) {
		pr_info("amaudio2_utils opened\n");
		playback_data_cache_buf_size = SOFT_BUFFER_SIZE;
		playback_data_cache_rp = playback_data_cache_start;
		playback_data_cache_wp = playback_data_cache_start;
		mutex_init(&amaudio2_utils_lock);
	} else {
		pr_err("BUG:%s,%d, please check\n", __FILE__, __LINE__);
		res = -EINVAL;
		goto error;
	}

	amaudio->type = iminor(inode);
	amaudio->dev = this->dev;
	file->private_data = amaudio;
	file->f_op = this->fops;
	this->runtime = amaudio;
	return res;
error: kfree(amaudio);
	return res;
}

static int amaudio_release(struct inode *inode, struct file *file)
{
	struct amaudio_t *amaudio = (struct amaudio_t *)file->private_data;

	if (iminor(inode) == 0) {
		free_irq(IRQ_OUT, amaudio);
		amaudio->sw.addr = 0;
		amaudio2_enable = 0;
	} else if (iminor(inode) == 3)
		amaudio2_read_enable = 0;

	kfree(amaudio);
	return 0;
}

static int amaudio_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct amaudio_t *amaudio = (struct amaudio_t *)file->private_data;

	if (amaudio->type == 0) {
		int mmap_flag = dma_mmap_coherent(amaudio->dev, vma,
						  (void *)amaudio->sw.addr,
						  amaudio->sw.paddr,
						  amaudio->sw.size);
		return mmap_flag;
	} else if (amaudio->type == 1) {
		pr_debug("audio in mmap!\n");
	} else {
		return -ENODEV;
	}

	return 0;
}

int remap_8ch_to_2ch(int32_t *in_buf, int16_t *out_buf, size_t frames)
{
	uint i;

	if (in_buf == NULL || out_buf == NULL || frames == 0)
		return -EINVAL;

	for (i = 0; i < frames; i++) {
		out_buf[2*i + 0] = (int16_t)((in_buf[8*i + 2]) >> 16);
		out_buf[2*i + 1] = (int16_t)((in_buf[8*i + 3]) >> 16);
	}
	return 0;
}

int get_pcm_cache_space(void)
{
	int space;

	if (playback_data_cache_wp >= playback_data_cache_rp) {
		space =  playback_data_cache_buf_size - (playback_data_cache_wp
				 - playback_data_cache_rp);
	} else {
		space =  playback_data_cache_rp - playback_data_cache_wp;
	}
	return space;
}
EXPORT_SYMBOL(get_pcm_cache_space);

static int get_pcm_cache_content(void)
{
	return playback_data_cache_buf_size - get_pcm_cache_space();
}

static unsigned int get_hw_delay_size(void)
{
	unsigned int size;

	if (amaudio2_enable == 0) {
		unsigned int i2s_out_ptr = get_i2s_out_ptr();
		unsigned int buffer_size = get_i2s_out_size();

		size = (aml_i2s_alsa_write_addr + buffer_size - i2s_out_ptr)
				% buffer_size;
	} else
		size = amaudio_delay;

	if (aml_i2s_playback_channel == 8)
		size = size >> 3;

	return size;
}

static void buffer_copy(int size)
{
	int tail;

	mutex_lock(&amaudio2_utils_lock);
	if ((playback_data_cache_wp + size) > (playback_data_cache_buf_size
					+ playback_data_cache_start)) {
		tail = playback_data_cache_buf_size + playback_data_cache_start
					- playback_data_cache_wp;

		memcpy((char *)playback_data_cache_wp,
					playback_tmp_start, tail);
		playback_data_cache_wp = playback_data_cache_start;

		memcpy((char *)playback_data_cache_wp,
					playback_tmp_start + tail, size - tail);
		playback_data_cache_wp = playback_data_cache_start
					+ size - tail;
	} else {
		memcpy((char *)playback_data_cache_wp,
					playback_tmp_start, size);
		playback_data_cache_wp += size;
	}
	mutex_unlock(&amaudio2_utils_lock);
}

int cache_pcm_write(char __user *buf, int size)
{
	if (playback_data_cache_wp == 0 || playback_data_cache_rp == 0
				   || playback_data_cache_buf_size == 0) {
		pr_info("write ! check the audio cached buffer ptr\n");
		return -1;
	}

	if (copy_from_user((char *)playback_tmp_start, buf, size)) {
		pr_err("copy audio pcm from user failed\n");
		return -EFAULT;
	}

	if (aml_i2s_playback_channel == 8) {
		remap_8ch_to_2ch(playback_tmp_start,
					playback_tmp_start, size >> 5);
		size = size >> 3;
	}

	buffer_copy(size);

	return size;
}
EXPORT_SYMBOL(cache_pcm_write);

#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE
static int amaudio2_cache_pcm_write(char *buf, int size)
{
	if (playback_data_cache_wp == 0 || playback_data_cache_rp == 0
				   || playback_data_cache_buf_size == 0) {
		pr_info("write ! check the audio cached buffer ptr\n");
		return -1;
	}

	memcpy((char *)playback_tmp_start, buf, size);
	if (aml_i2s_playback_channel == 8) {
		remap_8ch_to_2ch(playback_tmp_start,
					playback_tmp_start, size >> 5);
		size = size >> 3;
	}

	buffer_copy(size);

	return size;
}
#endif

static int cache_pcm_read(char __user *buf, int size)
{
	int tail;
	int data_left = get_pcm_cache_content();

	if (data_left  <= 0)
		return 0;
	if (data_left < size)
		size = data_left;
	if (playback_data_cache_wp == 0 || playback_data_cache_rp == 0
				   || playback_data_cache_buf_size == 0) {
		pr_info("read!check the audio cached buffer ptr\n");
		return -1;
	}

	mutex_lock(&amaudio2_utils_lock);
	if ((playback_data_cache_rp + size) > (playback_data_cache_buf_size
				+ playback_data_cache_start)) {
		tail = playback_data_cache_buf_size + playback_data_cache_start
				- playback_data_cache_rp;
		if (copy_to_user(buf, (char *)playback_data_cache_rp, tail)) {
			pr_err("copy audio pcm to user failed\n");
			return -EFAULT;
		}
		playback_data_cache_rp = playback_data_cache_start;
		if (copy_to_user(buf + tail, (char *)playback_data_cache_rp,
				size - tail)) {
			pr_err("copy audio pcm to user failed\n");
			return -EFAULT;
		}
		playback_data_cache_rp = playback_data_cache_start
				+ size - tail;
	} else {
		if (copy_to_user(buf, (char *)playback_data_cache_rp,  size)) {
			pr_err("copy audio pcm to user failed\n");
			return -EFAULT;
		}
		playback_data_cache_rp += size;
	}
	mutex_unlock(&amaudio2_utils_lock);
	return size;
}

static ssize_t amaudio_read(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	struct amaudio_t *amaudio = (struct amaudio_t *)file->private_data;

	amaudio2_read_enable = 1;
	/* just use the software buffer to temp store the output sample data */
	if (amaudio->type == 3) {
		if (if_audio_out_enable() == 0 || count == 0 ||
				amaudio2_read_enable == 0)
			return 0;
		return cache_pcm_read(buf, count);
	}
	return 0;
}

static unsigned int get_i2s_out_size(void)
{
	return aml_aiu_read(AIU_MEM_I2S_END_PTR)
	       - aml_aiu_read(AIU_MEM_I2S_START_PTR) + i2s_num;
}

static unsigned int get_i2s_out_ptr(void)
{
	return aml_aiu_read(AIU_MEM_I2S_RD_PTR)
	       - aml_aiu_read(AIU_MEM_I2S_START_PTR);
}

void cover_memcpy(struct BUF *des, int a, struct BUF *src, int b,
				unsigned int count)
{
	int i;
	int samp;

	int16_t *des_left = (int16_t *)(des->addr + a);
	int16_t *des_right = des_left + 16;
	int16_t *src_buf = (int16_t *)(src->addr + b);

#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE
	for (i = 0; i < count; i += 4) {
		samp = ((*src_buf++) * direct_left_gain) >> 8;
		*des_left++ = (int16_t)samp;
		samp = ((*src_buf++) * direct_right_gain) >> 8;
		*des_right++ = (int16_t)samp;
	}
#else
	int j;

	for (i = 0; i < count; i += 64) {
		for (j = 0; j < 16; j++) {
			samp = ((*src_buf++) * direct_left_gain) >> 8;
			*des_left++ = (int16_t)samp;
			samp = ((*src_buf++) * direct_right_gain) >> 8;
			*des_right++ = (int16_t)samp;
		}
		des_left += 16;
		des_right += 16;
	}
#endif
}

void direct_mix_memcpy(struct BUF *des, int a, struct BUF *src, int b,
				unsigned int count)
{
	int i;
	int samp;

	int16_t *des_left = (int16_t *)(des->addr + a);
	int16_t *des_right = des_left + 16;
	int16_t *src_buf = (int16_t *)(src->addr + b);

#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE
	for (i = 0; i < count; i += 4) {
		samp = ((*des_left) * music_gain +
			(*src_buf++) * direct_left_gain) >> 8;
		*des_left++ = clip16(samp);

		samp = ((*des_right) * music_gain +
			(*src_buf++) * direct_right_gain) >> 8;
		*des_right++ = clip16(samp);
	}
#else
	int j;

	for (i = 0; i < count; i += 64) {
		for (j = 0; j < 16; j++) {
			samp = ((*des_left) * music_gain +
				(*src_buf++) * direct_left_gain) >> 8;
			*des_left++ = clip16(samp);

			samp = ((*des_right) * music_gain +
			    (*src_buf++) * direct_right_gain) >> 8;
			*des_right++ = clip16(samp);
		}
		des_left += 16;
		des_right += 16;
	}
#endif
}

void inter_mix_memcpy(struct BUF *des, int a, struct BUF *src, int b,
				unsigned int count)
{
	int i;
	int16_t sampL, sampR;
	int samp, sampLR;

	int16_t *des_left = (int16_t *)(des->addr + a);
	int16_t *des_right = des_left + 16;
	int16_t *src_buf = (int16_t *)(src->addr + b);

#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE
	for (i = 0; i < count; i += 4) {
		sampL = *src_buf++;
		sampR = *src_buf++;
		/* Here has risk to distortion.
		 * Linein signals are always weak,
		 * so add them direct.
		 */
		sampLR = (sampL * direct_left_gain +
		sampR * direct_right_gain) >> 1;
		samp = ((*des_left) * music_gain + sampLR) >> 8;
		*des_left++ = clip16(samp);

		samp = ((*des_right) * music_gain + sampLR) >> 8;
		*des_right++ = clip16(samp);
	}
#else
	int j;

	for (i = 0; i < count; i += 64) {
		for (j = 0; j < 16; j++) {
			sampL = *src_buf++;
			sampR = *src_buf++;
			/* Here has risk to distortion.
			 * Linein signals are always weak,
			 * so add them direct.
			 */
			sampLR = (sampL * direct_left_gain +
				 sampR * direct_right_gain) >> 1;
			samp = ((*des_left) * music_gain + sampLR) >> 8;
			*des_left++ = clip16(samp);

			samp = ((*des_right) * music_gain + sampLR) >> 8;
			*des_right++ = clip16(samp);
		}
		des_left += 16;
		des_right += 16;
	}
#endif
}

void cover_memcpy_8_channel(struct BUF *des, int a, struct BUF *src, int b,
				unsigned int count)
{
	int i;
#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE
	if (aml_i2s_playback_format == 32 || aml_i2s_playback_format == 24) {
		int32_t *to = (int32_t *)(des->addr + a);
		int32_t *tfrom = (int32_t *)(src->addr + b);

		for (i = 0; i < count; i += 8) {
			*to++ = (int32_t)
				(((long)(*tfrom++) * direct_left_gain) >> 8);
			*to++ = (int32_t)
				(((long)(*tfrom++) * direct_right_gain) >> 8);
		}
	} else {
		int16_t *to = (int16_t *)(des->addr + a);
		int16_t *tfrom = (int16_t *)(src->addr + b);

		for (i = 0; i < count; i += 4) {
			*to++ = (int16_t)
				(((*tfrom++) * direct_left_gain) >> 8);
			*to++ = (int16_t)
				(((*tfrom++) * direct_right_gain) >> 8);
		}
	}
#else
	int j;

	if (aml_i2s_playback_format == 32 || aml_i2s_playback_format == 24) {
		int32_t *to = (int32_t *)(des->addr + a);
		int32_t *tfrom = (int32_t *)(src->addr + b);
		int32_t *lf, *cf, *rf, *ls, *rs, *lef, *sbl, *sbr;

		lf = to;
		cf = to + 1 * 8;
		rf = to + 2 * 8;
		ls = to + 3 * 8;
		rs = to + 4 * 8;
		lef = to + 5 * 8;
		sbl = to + 6 * 8;
		sbr = to + 7 * 8;
		for (j = 0; j < count; j += 256) {
			for (i = 0; i < 8; i++) {
				*lf++ = (((*tfrom++) >> 8) * direct_left_gain)
					>> 8;
				*cf++ = (((*tfrom++) >> 8) * direct_right_gain)
					>> 8;
				*rf++ = (((*tfrom++) >> 8) * direct_left_gain)
					>> 8;
				*ls++ = (((*tfrom++) >> 8) * direct_right_gain)
					>> 8;
				*rs++ = (((*tfrom++) >> 8) * direct_left_gain)
					>> 8;
				*lef++ = (((*tfrom++) >> 8) * direct_right_gain)
					>> 8;
				*sbl++ = (((*tfrom++) >> 8) * direct_left_gain)
					>> 8;
				*sbr++ = (((*tfrom++) >> 8) * direct_right_gain)
					>> 8;
			}
			lf += 56;
			cf += 56;
			rf += 56;
			ls += 56;
			rs += 56;
			lef += 56;
			sbl += 56;
			sbr += 56;
		}
	} else {
		int16_t *to = (int16_t *)(des->addr + a);
		int16_t *tfrom = (int16_t *)(src->addr + b);
		int16_t *lf, *cf, *rf, *ls, *rs, *lef, *sbl, *sbr;

		lf = to;
		cf = to + 1 * 16;
		rf = to + 2 * 16;
		ls = to + 3 * 16;
		rs = to + 4 * 16;
		lef = to + 5 * 16;
		sbl = to + 6 * 16;
		sbr = to + 7 * 16;
		for (j = 0; j < count; j += 256) {
			for (i = 0; i < 16; i++) {
				*lf++ = ((*tfrom++) * direct_left_gain) >> 8;
				*cf++ = ((*tfrom++) * direct_right_gain) >> 8;
				*rf++ = ((*tfrom++) * direct_left_gain) >> 8;
				*ls++ = ((*tfrom++) * direct_right_gain) >> 8;
				*rs++ = ((*tfrom++) * direct_left_gain) >> 8;
				*lef++ = ((*tfrom++) * direct_right_gain) >> 8;
				*sbl++ = ((*tfrom++) * direct_left_gain) >> 8;
				*sbr++ = ((*tfrom++) * direct_right_gain) >> 8;
			}
			lf += 7 * 16;
			cf += 7 * 16;
			rf += 7 * 16;
			ls += 7 * 16;
			rs += 7 * 16;
			lef += 7 * 16;
			sbl += 7 * 16;
			sbr += 7 * 16;
		}
	}
#endif
}

void direct_mix_memcpy_8_channel(struct BUF *des, int a, struct BUF *src, int b,
				unsigned int count)
{
	int i;
	int samp;

#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE
	if (aml_i2s_playback_format == 32 || aml_i2s_playback_format == 24) {
		int32_t *to = (int32_t *)(des->addr + a);
		int32_t *tfrom = (int32_t *)(src->addr + b);

		for (i = 0; i < count; i += 8) {
			samp = *to;
			*to++ = clip32(((long)(samp) * music_gain +
				(long)(*tfrom++) * direct_left_gain) >> 8);
			samp = *to;
			*to++ = clip32(((long)(samp) * music_gain +
				(long)(*tfrom++) * direct_right_gain) >> 8);
		}
	} else {
		int16_t *to = (int16_t *)(des->addr + a);
		int16_t *tfrom = (int16_t *)(src->addr + b);

		for (i = 0; i < count; i += 4) {
			samp = *to;
			*to++ = clip16(((samp) * music_gain +
				(*tfrom++) * direct_left_gain) >> 8);
			samp = *to;
			*to++ = clip16(((samp) * music_gain +
				(*tfrom++) * direct_right_gain) >> 8);
		}
	}
#else
	int j;

	if (aml_i2s_playback_format == 32 || aml_i2s_playback_format == 24) {
		int32_t *to = (int32_t *)(des->addr + a);
		int32_t *tfrom = (int32_t *)(src->addr + b);
		int32_t *lf, *cf, *rf, *ls, *rs, *lef, *sbl, *sbr;

		lf = to;
		cf = to + 1 * 8;
		rf = to + 2 * 8;
		ls = to + 3 * 8;
		rs = to + 4 * 8;
		lef = to + 5 * 8;
		sbl = to + 6 * 8;
		sbr = to + 7 * 8;
		for (j = 0; j < count; j += 256) {
			for (i = 0; i < 8; i++) {
				samp = *lf;
				*lf++ = clip24(((samp) * music_gain +
					((*tfrom++) >> 8) * direct_left_gain)
					>> 8);
				samp = *cf;
				*cf++ = clip24(((samp) * music_gain +
					((*tfrom++) >> 8) * direct_right_gain)
					>> 8);
				samp = *rf;
				*rf++ = clip24(((samp) * music_gain +
					((*tfrom++) >> 8) * direct_left_gain)
					>> 8);
				samp = *ls;
				*ls++ = clip24(((samp) * music_gain +
					((*tfrom++) >> 8) * direct_right_gain)
					>> 8);
				samp = *rs;
				*rs++ = clip24(((samp) * music_gain +
					((*tfrom++) >> 8) * direct_left_gain)
					>> 8);
				samp = *lef;
				*lef++ = clip24(((samp) * music_gain +
					((*tfrom++) >> 8) * direct_right_gain)
					>> 8);
				samp = *sbl;
				*sbl++ = clip24(((samp) * music_gain +
					((*tfrom++) >> 8) * direct_left_gain)
					>> 8);
				samp = *sbr;
				*sbr++ = clip24(((samp) * music_gain +
					((*tfrom++) >> 8) * direct_right_gain)
					>> 8);
			}
			lf += 56;
			cf += 56;
			rf += 56;
			ls += 56;
			rs += 56;
			lef += 56;
			sbl += 56;
			sbr += 56;
		}
	} else {
		int16_t *to = (int16_t *)(des->addr + a);
		int16_t *tfrom = (int16_t *)(src->addr + b);
		int16_t *lf, *cf, *rf, *ls, *rs, *lef, *sbl, *sbr;

		lf = to;
		cf = to + 1 * 16;
		rf = to + 2 * 16;
		ls = to + 3 * 16;
		rs = to + 4 * 16;
		lef = to + 5 * 16;
		sbl = to + 6 * 16;
		sbr = to + 7 * 16;
		for (j = 0; j < count; j += 256) {
			for (i = 0; i < 16; i++) {
				samp = *lf;
				*lf++ = clip16(((samp) * music_gain +
					(*tfrom++) * direct_left_gain) >> 8);
				samp = *cf;
				*cf++ = clip16(((samp) * music_gain +
					(*tfrom++) * direct_right_gain) >> 8);
				samp = *rf;
				*rf++ = clip16(((samp) * music_gain +
					(*tfrom++) * direct_left_gain) >> 8);
				samp = *ls;
				*ls++ = clip16(((samp) * music_gain +
					(*tfrom++) * direct_right_gain) >> 8);
				samp = *rs;
				*rs++ = clip16(((samp) * music_gain +
					(*tfrom++) * direct_left_gain) >> 8);
				samp = *lef;
				*lef++ = clip16(((samp) * music_gain +
					(*tfrom++) * direct_right_gain) >> 8);
				samp = *sbl;
				*sbl++ = clip16(((samp) * music_gain +
					(*tfrom++) * direct_left_gain) >> 8);
				samp = *sbr;
				*sbr++ = clip16(((samp) * music_gain +
					(*tfrom++) * direct_right_gain) >> 8);
			}
			lf += 7 * 16;
			cf += 7 * 16;
			rf += 7 * 16;
			ls += 7 * 16;
			rs += 7 * 16;
			lef += 7 * 16;
			sbl += 7 * 16;
			sbr += 7 * 16;
		}
	}
#endif
}

void inter_mix_memcpy_8_channel(struct BUF *des, int a, struct BUF *src, int b,
				unsigned int count)
{
	int i;
	int samp, sampLR, sampL, sampR;

#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE
	if (aml_i2s_playback_format == 32 || aml_i2s_playback_format == 24) {
		int32_t *to = (int32_t *)(des->addr + a);
		int32_t *tfrom = (int32_t *)(src->addr + b);

		for (i = 0; i < count; i += 8) {
			sampL = (int)
				(((long)(*tfrom++) * direct_left_gain) >> 8);
			sampR = (int)
				(((long)(*tfrom++) * direct_right_gain) >> 8);
			sampLR = (sampL + sampR) >> 1;

			samp = *to;
			*to++ = clip32(
				(((long)samp * music_gain) >> 8) + sampLR);
			samp = *to;
			*to++ = clip32(
				(((long)samp * music_gain) >> 8) + sampLR);
		}
	} else {
		int16_t *to = (int16_t *)(des->addr + a);
		int16_t *tfrom = (int16_t *)(src->addr + b);

		for (i = 0; i < count; i += 4) {
			sampL = (int)(((*tfrom++) * direct_left_gain) >> 8);
			sampR = (int)(((*tfrom++) * direct_right_gain) >> 8);
			sampLR = (sampL + sampR) >> 1;

			samp = *to;
			*to++ = clip16(
				(((long)samp * music_gain) >> 8) + sampLR);
			samp = *to;
			*to++ = clip16(
				(((long)samp * music_gain) >> 8) + sampLR);
		}
	}
#else
	int j;

	if (aml_i2s_playback_format == 32 || aml_i2s_playback_format == 24) {
		int32_t *to = (int32_t *)(des->addr + a);
		int32_t *tfrom = (int32_t *)(src->addr + b);
		int32_t *lf, *cf, *rf, *ls, *rs, *lef, *sbl, *sbr;

		lf = to;
		cf = to + 1 * 8;
		rf = to + 2 * 8;
		ls = to + 3 * 8;
		rs = to + 4 * 8;
		lef = to + 5 * 8;
		sbl = to + 6 * 8;
		sbr = to + 7 * 8;
		for (j = 0; j < count; j += 256) {
			for (i = 0; i < 8; i++) {
				sampL = (((*tfrom++) >> 8) * direct_left_gain)
					>> 8;
				sampR = (((*tfrom++) >> 8) * direct_right_gain)
					>> 8;
				sampLR = (sampL + sampR) >> 1;

				samp = *lf;
				*lf++ = clip24(
					((samp * music_gain) >> 8) + sampLR);
				samp = *cf;
				*cf++ = clip24(
					((samp * music_gain) >> 8) + sampLR);

				sampL = (((*tfrom++) >> 8) * direct_left_gain)
					>> 8;
				sampR = (((*tfrom++) >> 8) * direct_right_gain)
					>> 8;
				sampLR = (sampL + sampR) >> 1;
				samp = *rf;
				*rf++ = clip24(
					((samp * music_gain) >> 8) + sampLR);
				samp = *ls;
				*ls++ = clip24(
					((samp * music_gain) >> 8) + sampLR);

				sampL = (((*tfrom++) >> 8) * direct_left_gain)
					>> 8;
				sampR = (((*tfrom++) >> 8) * direct_right_gain)
					>> 8;
				sampLR = (sampL + sampR) >> 1;
				samp = *rs;
				*rs++ = clip24(
					((samp * music_gain) >> 8) + sampLR);
				samp = *lef;
				*lef++ = clip24(
					((samp * music_gain) >> 8) + sampLR);

				sampL = (((*tfrom++) >> 8) * direct_left_gain)
					>> 8;
				sampR = (((*tfrom++) >> 8) * direct_right_gain)
					>> 8;
				sampLR = (sampL + sampR) >> 1;
				samp = *sbl;
				*sbl++ = clip24(
					((samp * music_gain) >> 8) + sampLR);
				samp = *sbr;
				*sbr++ = clip24(
					((samp * music_gain) >> 8) + sampLR);
			}
			lf += 56;
			cf += 56;
			rf += 56;
			ls += 56;
			rs += 56;
			lef += 56;
			sbl += 56;
			sbr += 56;
		}
	} else {
		int16_t *to = (int16_t *)(des->addr + a);
		int16_t *tfrom = (int16_t *)(src->addr + b);
		int16_t *lf, *cf, *rf, *ls, *rs, *lef, *sbl, *sbr;

		lf = to;
		cf = to + 1 * 16;
		rf = to + 2 * 16;
		ls = to + 3 * 16;
		rs = to + 4 * 16;
		lef = to + 5 * 16;
		sbl = to + 6 * 16;
		sbr = to + 7 * 16;
		for (j = 0; j < count; j += 256) {
			for (i = 0; i < 16; i++) {
				sampL = ((*tfrom++) * direct_left_gain) >> 8;
				sampR = ((*tfrom++) * direct_right_gain) >> 8;
				sampLR = (sampL + sampR) >> 1;
				samp = *lf;
				*lf++ = clip16(
					((samp * music_gain) >> 8) + sampLR);
				samp = *cf;
				*cf++ = clip16(
					((samp * music_gain) >> 8) + sampLR);

				sampL = ((*tfrom++) * direct_left_gain) >> 8;
				sampR = ((*tfrom++) * direct_right_gain) >> 8;
				sampLR = (sampL + sampR) >> 1;
				samp = *rf;
				*rf++ = clip16(
					((samp * music_gain) >> 8) + sampLR);
				samp = *ls;
				*ls++ = clip16(
					((samp * music_gain) >> 8) + sampLR);

				sampL = ((*tfrom++) * direct_left_gain) >> 8;
				sampR = ((*tfrom++) * direct_right_gain) >> 8;
				sampLR = (sampL + sampR) >> 1;
				samp = *rs;
				*rs++ = clip16(
					((samp * music_gain) >> 8) + sampLR);
				samp = *lef;
				*lef++ = clip16(
					((samp * music_gain) >> 8) + sampLR);

				sampL = ((*tfrom++) * direct_left_gain) >> 8;
				sampR = ((*tfrom++) * direct_right_gain) >> 8;
				sampLR = (sampL + sampR) >> 1;
				samp = *sbl;
				*sbl++ = clip16(
					((samp * music_gain) >> 8) + sampLR);
				samp = *sbr;
				*sbr++ = clip16(
					((samp * music_gain) >> 8) + sampLR);
			}
			lf += 7 * 16;
			cf += 7 * 16;
			rf += 7 * 16;
			ls += 7 * 16;
			rs += 7 * 16;
			lef += 7 * 16;
			sbl += 7 * 16;
			sbr += 7 * 16;
		}
	}
#endif
}

static void i2s_copy(struct amaudio_t *amaudio)
{
	struct BUF *hw = &amaudio->hw;
	struct BUF *sw = &amaudio->sw;
	unsigned int i2s_out_ptr = get_i2s_out_ptr();
	unsigned int alsa_delay =
		(aml_i2s_alsa_write_addr + hw->size - i2s_out_ptr) % hw->size;
#ifdef AMAUDIO2_USE_IRQ
	unsigned long swirqflags, hwirqflags;
#endif

	amaudio_delay = (hw->wr + hw->size - i2s_out_ptr) % hw->size;

#ifdef AMAUDIO2_USE_IRQ
	spin_lock_irqsave(&hw->lock, hwirqflags);
#else
	mutex_lock(&hw->lock);
#endif

	hw->rd = i2s_out_ptr;
	hw->level = amaudio_delay;
	if (hw->level <= int_block ||
			(alsa_delay - amaudio_delay) < int_block) {
		hw->wr = (hw->rd + latency) % hw->size;
		hw->wr /= int_block;
		hw->wr *= int_block;
		hw->level = latency;

		goto EXIT;
	}

	if (sw->level > soft_buffer_threshold) {
		/*
		 * pr_debug(
		 * "Reset sw: hw->wr = %x,hw->rd = %x, hw->level = %x,"
		 * "alsa_delay:%x,sw->wr = %x, sw->rd = %x,sw->level = %x\n",
		 * hw->wr, hw->rd, hw->level, alsa_delay,
		 * sw->wr, sw->rd, sw->level);
		 */
		sw->rd = (sw->wr + int_block) % sw->size;
		sw->level = sw->size - int_block;
		goto EXIT;
	}

	if (sw->level < int_block)
		goto EXIT;

	WARN_ON((hw->wr + int_block > hw->size) ||
				(sw->rd + int_block > sw->size));

	if (external_mute_enable == 1 && external_mute_flag == 1) {
		memset((hw->addr + hw->wr), 0, int_block);
	} else {
		if (audio_out_mode == 0)
			(*aml_cover_memcpy)
					(hw, hw->wr, sw, sw->rd, int_block);
		else if (audio_out_mode == 1)
			(*aml_inter_mix_memcpy)
					(hw, hw->wr, sw, sw->rd, int_block);
		else if (audio_out_mode == 2)
			(*aml_direct_mix_memcpy)
					(hw, hw->wr, sw, sw->rd, int_block);

#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE
		if (amaudio2_enable == 1 && amaudio2_read_enable == 1) {
			char *read_buf = (char *)(hw->addr + hw->wr);

			amaudio2_cache_pcm_write(read_buf, int_block);
		}
#endif

	}

	hw->wr = (hw->wr + int_block) % hw->size;
	hw->level = (hw->wr + hw->size - i2s_out_ptr) % hw->size;

#ifdef AMAUDIO2_USE_IRQ
	spin_unlock_irqrestore(&hw->lock, hwirqflags);
	spin_lock_irqsave(&sw->lock, swirqflags);
#else
	mutex_unlock(&hw->lock);
	mutex_lock(&sw->lock);
#endif

	sw->rd = (sw->rd + int_block) % sw->size;
	sw->level = (sw->size + sw->wr - sw->rd) % sw->size;

#ifdef AMAUDIO2_USE_IRQ
	spin_unlock_irqrestore(&sw->lock, swirqflags);
#else
	mutex_unlock(&sw->lock);
#endif
	return;

EXIT:

#ifdef AMAUDIO2_USE_IRQ
	spin_unlock_irqrestore(&hw->lock, hwirqflags);
#else
	mutex_unlock(&hw->lock);
#endif
}

#ifdef AMAUDIO2_DEBUG
#define period_reset	(1000*1000)
#define CCCNT_WARN		2000
static unsigned long start_time;
static unsigned long isr_time;
static unsigned long total_time;
static unsigned long total_cnt;
#endif

static irqreturn_t i2s_out_callback(int irq, void *data)
{
	struct amaudio_t *amaudio = (struct amaudio_t *)data;

#ifdef AMAUDIO2_DEBUG
	unsigned long clk1, clk2;
	unsigned int ratio = 0;

	clk1 = sched_clock()/1000;
#endif

	i2s_copy(amaudio);

#ifdef AMAUDIO2_DEBUG
	clk2 = sched_clock()/1000;
	isr_time += (clk2-clk1);
	total_time = (clk2-start_time);
	total_cnt++;

	if (total_time >= period_reset) {
		ratio = isr_time * 100 / total_time;
		if ((ratio >= 20) || (total_cnt > CCCNT_WARN)) {
			pr_err("Warning:isrtime:%ld totalTime:%ld ratio:%d, cnt:%d\n",
				isr_time, total_time, ratio, (int)total_cnt);
		}
		start_time	= sched_clock()/1000;
		isr_time	= 0;
		total_time = 0;
		total_cnt = 0;
	}
#endif

	return IRQ_HANDLED;
}

/* -----------------control interface---------------- */

#ifdef CONFIG_COMPAT
static long amaudio_compat_ioctl(struct file *file, unsigned int cmd, ulong arg)
{
	return amaudio_ioctl(file, cmd, (ulong)compat_ptr(arg));
}

static long amaudio_compat_utils_ioctl(struct file *file,
				unsigned int cmd, ulong arg)
{
	return amaudio_utils_ioctl(file, cmd, (ulong)compat_ptr(arg));
}
#endif

static long amaudio_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	struct amaudio_t *amaudio = (struct amaudio_t *)file->private_data;
	s32 r = 0;
#ifdef AMAUDIO2_USE_IRQ
	unsigned long swirqflags, hwirqflags;
#endif

	switch (cmd) {
	case AMAUDIO_IOC_GET_SIZE:
		/* total size of internal buffer */
		r = amaudio->sw.size;
		break;
	case AMAUDIO_IOC_GET_PTR:
		/* the read pointer of internal buffer */
#ifdef AMAUDIO2_USE_IRQ
		spin_lock_irqsave(&amaudio->sw.lock, swirqflags);
#else
		mutex_lock(&amaudio->sw.lock);
#endif

		r = amaudio->sw.rd;

#ifdef AMAUDIO2_USE_IRQ
		spin_unlock_irqrestore(&amaudio->sw.lock, swirqflags);
#else
		mutex_unlock(&amaudio->sw.lock);
#endif
		break;
	case AMAUDIO_IOC_UPDATE_APP_PTR:
		/*
		 * the user space write pointer
		 * of the internal buffer
		 */
#ifdef AMAUDIO2_USE_IRQ
		spin_lock_irqsave(&amaudio->sw.lock, swirqflags);
#else
		mutex_lock(&amaudio->sw.lock);
#endif

		amaudio->sw.wr = arg;
		amaudio->sw.level = (amaudio->sw.size + amaudio->sw.wr
					 - amaudio->sw.rd) % amaudio->sw.size;

#ifdef AMAUDIO2_USE_IRQ
		spin_unlock_irqrestore(&amaudio->sw.lock, swirqflags);
#else
		mutex_unlock(&amaudio->sw.lock);
#endif

		if (amaudio->sw.wr % i2s_num)
			pr_err("wr:%x, not %d Bytes align\n",
						amaudio->sw.wr, i2s_num);
		break;
	case AMAUDIO_IOC_RESET:
		/*
		 * reset the internal write buffer
		 * pointer to get a given latency
		 * this api should be called before fill datas
		 */
#ifdef AMAUDIO2_USE_IRQ
		spin_lock_irqsave(&amaudio->hw.lock, hwirqflags);
#else
		mutex_lock(&amaudio->hw.lock);
#endif

		amaudio->hw.rd = get_i2s_out_ptr();
		amaudio->hw.wr = (amaudio->hw.rd + latency) % amaudio->hw.size;
		amaudio->hw.wr /= int_block;
		amaudio->hw.wr *= int_block;
		amaudio->hw.level = latency;

#ifdef AMAUDIO2_USE_IRQ
		spin_unlock_irqrestore(&amaudio->hw.lock, hwirqflags);
		/* empty the buffer */
		spin_lock_irqsave(&amaudio->sw.lock, swirqflags);
#else
		mutex_unlock(&amaudio->hw.lock);
		mutex_lock(&amaudio->sw.lock);
#endif

		amaudio->sw.wr = 0;
		amaudio->sw.rd = 0;
		amaudio->sw.level = 0;

#ifdef AMAUDIO2_USE_IRQ
		spin_unlock_irqrestore(&amaudio->sw.lock, swirqflags);
#else
		mutex_unlock(&amaudio->sw.lock);
#endif
		/*pr_info("Reset amaudio2: latency=%d bytes\n", latency);*/
		break;
	case AMAUDIO_IOC_AUDIO_OUT_MODE:
		/*
		 * audio_out_mode = 0, covered alsa audio mode;
		 * audio_out_mode = 1, karaOK mode, Linein left and
		 * right channel inter mixed with android alsa audio;
		 * audio_out_mode = 2,
		 * TV in direct mix with android audio;
		 */
		if (arg > 2)
			return -EINVAL;

		audio_out_mode = arg;
		break;
	case AMAUDIO_IOC_MIC_LEFT_GAIN:
		/* in karaOK mode, mic volume can be set from 0-256 */
		if (arg > 256)
			return -EINVAL;
		direct_left_gain = arg;
		break;
	case AMAUDIO_IOC_MIC_RIGHT_GAIN:
		if (arg > 256)
			return -EINVAL;

		direct_right_gain = arg;
		break;
	case AMAUDIO_IOC_MUSIC_GAIN:
		/* music volume can be set from 0-256 */
		if (arg > 256)
			return -EINVAL;

		music_gain = arg;
		break;
	default:
		break;
	};
	return r;
}

static long amaudio_utils_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	int r = 0;

	switch (cmd) {
	case AMAUDIO_IOC_GET_READY_SIZE:
		r = get_pcm_cache_content();
		break;
	case AMAUDIO_IOC_GET_HW_DELAY_SIZE:
		r = get_hw_delay_size();
		break;
	case AMAUDIO_IOC_RESET_BUFFER:
		mutex_lock(&amaudio2_utils_lock);
		playback_data_cache_rp = playback_data_cache_start;
		playback_data_cache_wp = playback_data_cache_start;
		mutex_unlock(&amaudio2_utils_lock);
		pr_info("amaudio_utils_ioctl: reset buffer!\n");
		break;
	case AMAUDIO_IOC_RUNNING_FLAG:
		r = aml_i2s_playback_running_flag;
		break;
	default:
		break;
	}

	return r;
}

/* -----------------class interface---------------- */
static ssize_t show_audio_out_mode(struct class *class,
				struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", audio_out_mode);
}

static ssize_t store_audio_out_mode(struct class *class,
				struct class_attribute *attr,
				const char *buf, size_t count)
{
	if (buf[0] == '0') {
		pr_info("Audio_in data covered the android local data as output!\n");
		audio_out_mode = 0;
	} else if (buf[0] == '1') {
		pr_info("Audio_in left and right channel mix first, then mix with android!\n");
		audio_out_mode = 1;
	} else if (buf[0] == '2') {
		pr_info("Audio_in data direct mixed with the android local data as output!\n");
		audio_out_mode = 2;
	}
	return count;
}

static ssize_t show_direct_left_gain(struct class *class,
				struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", direct_left_gain);
}

static ssize_t store_direct_left_gain(struct class *class,
				struct class_attribute *attr,
				const char *buf, size_t count)
{
	int val = 0;

	if (buf[0] && kstrtoint(buf, 10, &val))
		return -EINVAL;

	if (val < 0)
		val = 0;
	if (val > 256)
		val = 256;

	direct_left_gain = val;
	pr_debug("direct_left_gain set to %d\n", direct_left_gain);
	return count;
}

static ssize_t show_direct_right_gain(struct class *class,
				struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", direct_right_gain);
}

static ssize_t store_direct_right_gain(struct class *class,
				struct class_attribute *attr,
				const char *buf, size_t count)
{
	int val = 0;

	if (buf[0] && kstrtoint(buf, 10, &val))
		return -EINVAL;

	if (val < 0)
		val = 0;
	if (val > 256)
		val = 256;

	direct_right_gain = val;
	pr_debug("direct_right_gain set to %d\n", direct_right_gain);
	return count;
}

static ssize_t show_music_gain(struct class *class,
				struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", music_gain);
}

static ssize_t store_music_gain(struct class *class,
				struct class_attribute *attr, const char *buf,
				size_t count)
{
	int val = 0;

	if (buf[0] && kstrtoint(buf, 10, &val))
		return -EINVAL;

	if (val < 0)
		val = 0;
	if (val > 256)
		val = 256;

	music_gain = val;
	pr_debug("music_gain set to %d\n", music_gain);
	return count;
}

static ssize_t show_aml_external_mute_enable(struct class *class,
				struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", external_mute_enable);
}

static ssize_t store__aml_external_mute_enable(struct class *class,
				struct class_attribute *attr,
				const char *buf, size_t count)
{
	if (buf[0] == '0') {
		pr_info("amaudio2_out external mute enable!\n");
		external_mute_enable = 0;
	} else if (buf[0] == '1') {
		pr_info("amaudio2_out external mute disable!\n");
		external_mute_enable = 1;
	}

	return count;
}

static struct class_attribute amaudio_attrs[] = {
	__ATTR(aml_audio_out_mode,
	       0644,
	       show_audio_out_mode,
	       store_audio_out_mode),
	__ATTR(aml_direct_left_gain,
	       0644,
	       show_direct_left_gain,
	       store_direct_left_gain),
	__ATTR(aml_direct_right_gain,
	       0644,
	       show_direct_right_gain,
	       store_direct_right_gain),
	__ATTR(aml_music_gain,
	       0644,
	       show_music_gain,
	       store_music_gain),
	__ATTR(aml_external_mute_enable,
	       0644,
	       show_aml_external_mute_enable,
	       store__aml_external_mute_enable),
	__ATTR_NULL
};

static struct class amaudio_class = {
	.name		= AMAUDIO2_CLASS_NAME,
	.class_attrs	= amaudio_attrs,
};

static int amaudio2_init(struct platform_device *pdev)
{
	int ret = 0;
	int i = 0;
	struct amaudio_port_t *ap;

	/*pr_info("amaudio2: driver %s init!\n", AMAUDIO2_DRIVER_NAME);*/

	ret = register_chrdev(AMAUDIO2_MAJOR, AMAUDIO2_DRIVER_NAME,
				&amaudio_fops);
	if (ret < 0) {
		pr_err("Can't register %s divece ", AMAUDIO2_DRIVER_NAME);
		ret = -ENODEV;
		goto err;
	}

	amaudio_classp = class_create(THIS_MODULE, AMAUDIO2_DEVICE_NAME);
	if (IS_ERR(amaudio_classp)) {
		ret = PTR_ERR(amaudio_classp);
		goto err1;
	}

	ap = &amaudio_ports[0];
	for (i = 0; i < AMAUDIO2_DEVICE_COUNT; ap++, i++) {
		ap->dev = device_create(amaudio_classp, NULL,
					MKDEV(AMAUDIO2_MAJOR, i), NULL,
					amaudio_ports[i].name);
		if (IS_ERR(ap->dev)) {
			pr_err("amaudio2: failed to create amaudio device node\n");
			goto err2;
		}
	}

	ret = class_register(&amaudio_class);
	if (ret) {
		pr_err("amaudio2 class create fail.\n");
		goto err3;
	}

	amaudio_dev =
		device_create(&amaudio_class, NULL, MKDEV(AMAUDIO2_MAJOR, 15),
					NULL, AMAUDIO2_CLASS_NAME);
	if (IS_ERR(amaudio_dev)) {
		pr_err("amaudio2 creat device fail.\n");
		goto err4;
	}

	of_dma_configure(amaudio_dev, amaudio_dev->of_node);
	amaudio_start_addr =
				(unsigned char *)dma_alloc_coherent(amaudio_dev,
				SOFT_BUFFER_SIZE,
				&amaudio_phy_start_addr,
				GFP_KERNEL);

	if (!amaudio_start_addr) {
		pr_err("amaudio2 out soft DMA buffer alloc failed\n");
		goto err4;
	}

	playback_data_cache_start = (unsigned long)kmalloc(
			(SOFT_BUFFER_SIZE), GFP_KERNEL);
	if (!playback_data_cache_start) {
		pr_err("malloc for the audio output cache buffer fail\n");
		goto err5;
	}
	playback_tmp_start = kmalloc(
			(SOFT_BUFFER_SIZE), GFP_KERNEL);
	if (!playback_tmp_start)
		goto err6;

	pr_info("amaudio2: driver %s succuess!\n", AMAUDIO2_DRIVER_NAME);
	return 0;

err6:
	kfree((void *)playback_data_cache_start);
	playback_data_cache_start = 0;
err5:
	dma_free_coherent(amaudio_dev, SOFT_BUFFER_SIZE,
			(void *)amaudio_start_addr,
			amaudio_phy_start_addr);
	amaudio_start_addr = 0;
err4:
	device_destroy(&amaudio_class, MKDEV(AMAUDIO2_MAJOR, 10));
err3:
	class_unregister(&amaudio_class);
err2:
	for (ap = &amaudio_ports[0], i = 0; i < AMAUDIO2_DEVICE_COUNT;
	     ap++, i++)
		device_destroy(amaudio_classp, MKDEV(AMAUDIO2_MAJOR, i));
err1:
	class_destroy(amaudio_classp);
err:
	unregister_chrdev(AMAUDIO2_MAJOR, AMAUDIO2_DRIVER_NAME);
	return ret;
}

static int amaudio2_exit(struct platform_device *pdev)
{
	int i = 0;
	struct amaudio_port_t *ap;

	if (playback_data_cache_start) {
		kfree((void *)playback_data_cache_start);
		playback_data_cache_start = 0;
	}

	kfree(playback_tmp_start);

	if (amaudio_start_addr) {
		dma_free_coherent(amaudio_dev, SOFT_BUFFER_SIZE,
			(void *)amaudio_start_addr,
			amaudio_phy_start_addr);
		amaudio_start_addr = 0;
	}

	device_destroy(&amaudio_class, MKDEV(AMAUDIO2_MAJOR, 10));
	class_unregister(&amaudio_class);
	for (ap = &amaudio_ports[0], i = 0; i < AMAUDIO2_DEVICE_COUNT;
	     ap++, i++)
		device_destroy(amaudio_classp, MKDEV(AMAUDIO2_MAJOR, i));
	class_destroy(amaudio_classp);
	unregister_chrdev(AMAUDIO2_MAJOR, AMAUDIO2_DRIVER_NAME);

	return 0;
}

static const struct of_device_id amlogic_match[] = {
	{.compatible = "amlogic, aml_amaudio2",},
	{},
};

static struct platform_driver aml_amaudio2_driver = {
	.driver = {
		   .name = "aml_amaudio2_driver",
		   .owner = THIS_MODULE,
		   .of_match_table = amlogic_match,
		   },

	.probe = amaudio2_init,
	.remove = amaudio2_exit,
};

static int __init aml_amaudio2_modinit(void)
{
	return platform_driver_register(&aml_amaudio2_driver);
}

static void __exit aml_amaudio2_modexit(void)
{
	platform_driver_unregister(&aml_amaudio2_driver);
}

module_init(aml_amaudio2_modinit);
module_exit(aml_amaudio2_modexit);

MODULE_DESCRIPTION("AMLOGIC TV Audio driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Amlogic Inc.");
MODULE_VERSION("2.0.0");
