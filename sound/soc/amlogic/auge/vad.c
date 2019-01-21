/*
 * sound/soc/amlogic/auge/vad.c
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
 */
#define DEBUG

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/ioctl.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/clk.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>

#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/freezer.h>

#include <linux/amlogic/pm.h>
#include <linux/pm_wakeirq.h>
#include <linux/pm_wakeup.h>

#include <linux/input.h>
#include <linux/amlogic/vad_api.h>

#include "vad_hw_coeff.c"
#include "vad_hw.h"
#include "vad.h"

#define DRV_NAME "VAD"

#define DMA_BUFFER_BYTES_MAX (2 * 1024 * 1024)


#define VAD_READ_FRAME_COUNT    (1024 / 2)

/*#define __VAD_DUMP_DATA__*/
#define VAD_DUMP_FILE_NAME "/mnt/vaddump.pcm"

enum vad_level {
	LEVEL_USER,
	LEVEL_KERNEL,
};

struct vad {
	struct aml_audio_controller *actrl;
	struct device *dev;
	struct input_dev *input_device;

	struct clk *gate;
	struct clk *pll;
	struct clk *clk;

	struct toddr *tddr;

	char *buf;
	struct task_struct *thread;

	struct snd_dma_buffer dma_buffer;
	unsigned int start_last;
	unsigned int end_last;
	unsigned int addr;
	int switch_buffer;

	/* vad flag interrupt */
	int irq_wakeup;
	/* frame sync interrupt */
	int irq_fs;
	/* data source select
	 * Data src sel:
	 * 0: tdmin_a;
	 * 1: tdmin_b;
	 * 2: tdmin_c;
	 * 3: spdifin;
	 * 4: pdmin;
	 * 5: loopback_b;
	 * 6: tdmin_lb;
	 * 7: loopback_a;
	 */
	int src;
	/* Enable */
	int en;

	/* user space or kernel space to check hot word
	 * 1: check hot word in kernel
	 * 0: check hot word in user space
	 */
	enum vad_level level;

	int (*callback)(char *buf, int frames, int rate,
			int channels, int bitdepth);

#ifdef __VAD_DUMP_DATA__
	struct file *fp;
	mm_segment_t fs;
	loff_t pos;
#endif
};

static struct vad *s_vad;

static struct vad *get_vad(void)
{
	struct vad *p_vad;

	p_vad = s_vad;

	if (!p_vad) {
		pr_debug("Not init vad\n");
		return NULL;
	}

	return p_vad;
}

static void vad_key_report(void)
{
	struct vad *p_vad = get_vad();

	if (!p_vad)
		return;
	pr_info("%s\n", __func__);

	input_event(p_vad->input_device, EV_KEY, KEY_POWER, 1);
	input_sync(p_vad->input_device);
	input_event(p_vad->input_device, EV_KEY, KEY_POWER, 0);
	input_sync(p_vad->input_device);
}

static void vad_input_device_init(struct vad *pvad)
{
	pvad->input_device->name = "vad_keypad";
	pvad->input_device->phys = "vad_keypad/input3";
	pvad->input_device->dev.parent = pvad->dev;
	pvad->input_device->id.bustype = BUS_ISA;
	pvad->input_device->id.vendor  = 0x0001;
	pvad->input_device->id.product = 0x0001;
	pvad->input_device->id.version = 0x0100;
	pvad->input_device->evbit[0] = BIT_MASK(EV_KEY);
	pvad->input_device->keybit[BIT_WORD(BTN_0)] = BIT_MASK(BTN_0);
	set_bit(KEY_POWER, pvad->input_device->keybit);
}

static bool vad_is_enable(void)
{
	struct vad *p_vad = get_vad();

	if (!p_vad)
		return false;

	return p_vad->en;
}

static bool vad_src_check(enum vad_src src)
{
	struct vad *p_vad = get_vad();

	if (!p_vad)
		return false;

	if (p_vad->src == src)
		return true;

	return false;
}

bool vad_tdm_is_running(int tdm_idx)
{
	enum vad_src src;

	if (tdm_idx > 2)
		return false;

	src = (enum vad_src)tdm_idx;

	if (vad_is_enable() && vad_src_check(src))
		return true;

	return false;
}

bool vad_pdm_is_running(void)
{
	if (vad_is_enable() && vad_src_check(VAD_SRC_PDMIN))
		return true;

	return false;
}

static void vad_notify_user_space(struct vad *p_vad)
{
	pr_info("Notify to wake up user space\n");

	pm_wakeup_event(p_vad->dev, 2000);
	p_vad->addr = 0;
	vad_key_report();
}

/* For test */
static int vad_in_kernel_test;
static int vad_wakeup_count;

int register_vad_callback(int (*callback)(char *, int, int, int, int))
{
	struct vad *p_vad = get_vad();

	if (!p_vad)
		return -1;

	p_vad->callback = callback;
	return 0;
}
EXPORT_SYMBOL(register_vad_callback);

void unregister_vad_callback(void)
{
	struct vad *p_vad = get_vad();

	if (!p_vad)
		return;

	p_vad->callback = NULL;
}
EXPORT_SYMBOL(unregister_vad_callback);

/* transfer audio data to algorithm
 * 0: no wake word found
 * 1: contained wake word, should wake up system
 */
static int vad_transfer_data_to_algorithm(
	struct vad *p_vad, char *buf, int frames,
	int rate, int channels, int bitdepth)
{
	int ret = 0;
	/* TODO: for test */
	if (vad_in_kernel_test) {

		if (vad_wakeup_count < 50)
			return 0;

		vad_wakeup_count = 0;
		return 1;
	}

	/* Do check by algorithm */
	if (p_vad->callback != NULL)
		ret = p_vad->callback(buf, frames, rate, channels, bitdepth);

	return ret;
}

/* Check buffer in kernel for VAD */
static int vad_engine_check(struct vad *p_vad)
{
	unsigned char *hwbuf;
	int bytes, read_bytes;
	int start, end, size, last_addr, curr_addr;
	int chnum, bytes_per_sample;
	int frame_count = VAD_READ_FRAME_COUNT;

	if (!p_vad->tddr || !p_vad->tddr->actrl ||
		!p_vad->tddr->in_use)
		return 0;

	hwbuf = p_vad->dma_buffer.area;
	size  = p_vad->dma_buffer.bytes - 8;
	start = p_vad->dma_buffer.addr;
	end   = start + size;
	last_addr = p_vad->addr;
	curr_addr = aml_toddr_get_position(p_vad->tddr);

	if (curr_addr < start || curr_addr > end ||
		last_addr < start || last_addr > end) {
		pr_info("%s line:%d, start:%x,end:%x, addr:%x, curr_addr=%x\n",
			__func__, __LINE__,
			start,
			end,
			p_vad->addr,
			curr_addr);
		p_vad->addr = curr_addr;
		return 0;
	}

	bytes = (curr_addr - last_addr + size) % size;
	chnum = p_vad->tddr->channels;
	/* bytes for each sample */
	bytes_per_sample = p_vad->tddr->bitdepth >> 3;

	read_bytes = frame_count * chnum * bytes_per_sample;
	if (bytes < read_bytes) {
		pr_debug("%s line:%d, %d bytes, need more data\n",
			__func__, __LINE__, bytes);
		return 0;
	}

	if (!p_vad->buf) {
		p_vad->buf = kzalloc(read_bytes, GFP_KERNEL);
		if (!p_vad->buf) {
			pr_err("%s failed to allocate buffer for vad\n",
				__func__);
			return -ENOMEM;
		}
	}
	memset(p_vad->buf, 0x0, read_bytes);

	pr_debug("start:%x,end:%x, curr_addr:%x, last_addr:%x, offset:%d, read_bytes:%d\n",
		start,
		end,
		curr_addr,
		last_addr,
		bytes,
		read_bytes);


	if (last_addr + read_bytes <= end) {
		memcpy(p_vad->buf,
			hwbuf + last_addr - start,
			read_bytes);
		p_vad->addr = last_addr + read_bytes;
	} else {
		int tmp_bytes = end - last_addr;

		memcpy(p_vad->buf,
			hwbuf + last_addr - start,
			tmp_bytes);
		memcpy(p_vad->buf + tmp_bytes,
			hwbuf,
			read_bytes - tmp_bytes);
		p_vad->addr = start + read_bytes - tmp_bytes;
	}

#ifdef __VAD_DUMP_DATA__
	vfs_write(p_vad->fp, p_vad->buf, read_bytes, &p_vad->pos);
#endif

	return vad_transfer_data_to_algorithm(p_vad,
		p_vad->buf, frame_count,
		p_vad->tddr->rate,
		p_vad->tddr->channels,
		p_vad->tddr->bitdepth);
}

static int vad_freeze_thread(void *data)
{
	struct vad *p_vad = data;

	if (!p_vad)
		return 0;

	current->flags |= PF_NOFREEZE;

	for (;;) {
		msleep_interruptible(10);
		if (kthread_should_stop()) {
			pr_info("%s line:%d\n", __func__, __LINE__);

			break;
		}

		if (!p_vad || !p_vad->en || !p_vad->callback ||
			!p_vad->tddr || !p_vad->switch_buffer) {
			msleep_interruptible(10);
			continue;
		}

		if (vad_engine_check(p_vad) > 0)
			vad_notify_user_space(p_vad);
	}

	dev_info(p_vad->dev, "vad: freeze thread exit\n");
	return 0;
}


static irqreturn_t vad_wakeup_isr(int irq, void *data)
{
	struct vad *p_vad = (struct vad *)data;

	if (vad_in_kernel_test) {
		pr_info("%s vad_wakeup_count:%d\n", __func__, vad_wakeup_count);
		if ((p_vad->level == LEVEL_KERNEL) && p_vad->switch_buffer)
			vad_wakeup_count++;
	}
	return IRQ_HANDLED;
}

static irqreturn_t vad_fs_isr(int irq, void *data)
{
	return IRQ_HANDLED;
}

static int vad_set_clks(struct vad *p_vad, bool enable)
{
	if (enable) {
		int ret = 0;

		/* enable clock gate */
		ret = clk_prepare_enable(p_vad->gate);

		/* enable clock */
		ret = clk_prepare_enable(p_vad->pll);
		if (ret) {
			pr_err("Can't enable vad pll: %d\n", ret);
			return -EINVAL;
		}

		clk_set_rate(p_vad->clk, 25000000);
		ret = clk_prepare_enable(p_vad->clk);
		if (ret) {
			pr_err("Can't enable vad clk: %d\n", ret);
			return -EINVAL;
		}
	} else {
		/* disable clock and gate */
		clk_disable_unprepare(p_vad->clk);
		clk_disable_unprepare(p_vad->pll);
		clk_disable_unprepare(p_vad->gate);
	}

	return 0;
}

static int vad_init(struct vad *p_vad)
{
	int ret = 0, flag = 0;

	/* malloc buffer */
	ret = snd_dma_alloc_pages(SNDRV_DMA_TYPE_DEV,
				p_vad->dev,
				DMA_BUFFER_BYTES_MAX,
				&p_vad->dma_buffer);
	if (ret) {
		dev_err(p_vad->dev, "Cannot allocate buffer(s)\n");
		return ret;
	}

	/* register irq */
	if (p_vad->level == LEVEL_KERNEL) {
		flag = IRQF_SHARED | IRQF_NO_SUSPEND;

	/* Start Micro/Audio Dock firmware loader thread */
	if (!p_vad->thread) {
		p_vad->thread =
			kthread_create(vad_freeze_thread, p_vad,
					   "vad_freeze_thread");
		if (IS_ERR(p_vad->thread)) {
			int err = PTR_ERR(p_vad->thread);

			p_vad->thread = NULL;
			dev_info(p_vad->dev,
					"vad freeze: Creating thread failed\n");
			return err;
		}
		wake_up_process(p_vad->thread);
		vad_wakeup_count = 0;
	}

#ifdef __VAD_DUMP_DATA__
	p_vad->fp = filp_open(VAD_DUMP_FILE_NAME, O_RDWR | O_CREAT, 0666);
	if (IS_ERR(p_vad->fp)) {
		pr_err("create file %s error/n", VAD_DUMP_FILE_NAME);
		return -1;
	}
	p_vad->fs = get_fs();
	p_vad->pos = 0;
	set_fs(KERNEL_DS);
#endif

	} else if (p_vad->level == LEVEL_USER)
		flag = IRQF_SHARED;

	ret = request_irq(p_vad->irq_wakeup,
			vad_wakeup_isr, flag, "vad_wakeup",
			p_vad);
	if (ret) {
		dev_err(p_vad->dev, "failed to claim irq_wakeup %u\n",
					p_vad->irq_wakeup);
		return -ENXIO;
	}

	ret = request_irq(p_vad->irq_fs,
			vad_fs_isr, 0, "vad_fs",
			p_vad);
	if (ret) {
		dev_err(p_vad->dev, "failed to claim irq_fs %u\n",
					p_vad->irq_fs);
		return -ENXIO;
	}

	/* clock ready */
	vad_set_clks(p_vad, true);

	return ret;
}

static void vad_deinit(struct vad *p_vad)
{
	if (p_vad->level == LEVEL_KERNEL) {
#ifdef __VAD_DUMP_DATA__
		if (p_vad->fp) {
			set_fs(p_vad->fs);
			filp_close(p_vad->fp, NULL);
		}
#endif
		if (p_vad->thread) {
			kthread_stop(p_vad->thread);
			p_vad->thread = NULL;
		}
		kfree(p_vad->buf);
		p_vad->buf = NULL;
	}

	/* free irq */
	free_irq(p_vad->irq_wakeup, p_vad);
	free_irq(p_vad->irq_fs, p_vad);

	/* free buffer */
	snd_dma_free_pages(&p_vad->dma_buffer);

	/* clock disabled */
	vad_set_clks(p_vad, false);
}

void vad_update_buffer(int isvad)
{
	struct vad *p_vad = get_vad();
	unsigned int start, end;
	unsigned int rd_th;

	if (!p_vad || !p_vad->en || !p_vad->tddr ||
		!p_vad->tddr->in_use || !p_vad->tddr->actrl)
		return;

	if (p_vad->switch_buffer == isvad)
		return;
	p_vad->switch_buffer = isvad;

	if (isvad) {	/* switch to vad buffer */
		struct toddr *tddr = p_vad->tddr;

		p_vad->start_last = tddr->start_addr;
		p_vad->end_last   = tddr->end_addr;

		rd_th = 0x100;

		pr_debug("Switch to VAD buffer\n");
		pr_debug("\t ASAL start:%x, end:%x, bytes:%d\n",
			tddr->start_addr, tddr->end_addr,
			tddr->end_addr - tddr->start_addr);

		start = p_vad->dma_buffer.addr;
		end   = start + p_vad->dma_buffer.bytes - 8;

		pr_debug("\t VAD start:%x, end:%x, bytes:%d\n",
			start, end,
			end - start);
	} else {
		pr_debug("Switch to ALSA buffer\n");
		start = p_vad->start_last;
		end   = p_vad->end_last;
		rd_th = 0x40;
		vad_set_trunk_data_readable(true);
	}
	aml_toddr_set_buf(p_vad->tddr, start, end);
	aml_toddr_force_finish(p_vad->tddr);
	aml_toddr_update_fifos_rd_th(p_vad->tddr, rd_th);
	p_vad->addr = 0;
}

int vad_transfer_chunk_data(unsigned long data, int frames)
{
	struct vad *p_vad = get_vad();
	char __user *buf = (char __user *)data;
	unsigned char *hwbuf;
	int bytes;
	int start, end, addr, size;
	int chnum, bytes_per_sample;

	if (!buf || !p_vad || !p_vad->en || !p_vad->tddr)
		return 0;

	size  = p_vad->dma_buffer.bytes;
	start = p_vad->dma_buffer.addr;
	end   = start + size - 8;
	addr  = p_vad->addr;
	hwbuf = p_vad->dma_buffer.area;

	if (addr < start || addr > end)
		return 0;

	chnum = p_vad->tddr->channels;
	/* bytes for each sample */
	bytes_per_sample = p_vad->tddr->bitdepth >> 3;

	bytes = frames * chnum * bytes_per_sample < size ?
		frames * chnum * bytes_per_sample : size;

	pr_debug("%s dma bytes:%d, wanted bytes:%d, actual bytes:%d\n",
		__func__,
		size,
		frames * chnum * bytes_per_sample,
		bytes);

	pr_debug("%s dma bytes:%d, start:%d, end:%d, current:%d\n",
		__func__,
		size,
		start,
		end,
		addr);

	if (addr - start >= bytes) {
		if (copy_to_user(buf,
			hwbuf + addr - bytes - start,
			bytes))
			return 0;
	} else {
		int tmp_bytes = bytes - (addr - start);
		int tmp_offset = (end - tmp_bytes) - start;

		if (copy_to_user(buf,
			hwbuf + tmp_offset,
			tmp_bytes))
			return 0;

		if (copy_to_user(buf + tmp_bytes,
			hwbuf,
			addr - start))
			return 0;
	}

	/* After data copied, reset dma buffer */
	memset(hwbuf, 0x0, size);

	return bytes / (chnum * bytes_per_sample);
}

void vad_set_toddr_info(struct toddr *to)
{
	struct vad *p_vad = get_vad();

	if (!p_vad || !p_vad->en)
		return;

	pr_debug("%s update vad toddr:%p\n", __func__, to);

	p_vad->tddr = to;
}

void vad_enable(bool enable)
{
	struct vad *p_vad = get_vad();

	if (!p_vad || !p_vad->en)
		return;

	/* Force VAD enable to set parameters */
	if (enable) {
		int *p_de_coeff = vad_de_coeff;
		int len_de = ARRAY_SIZE(vad_de_coeff);
		int *p_win_coeff = vad_ram_coeff;
		int len_ram = ARRAY_SIZE(vad_ram_coeff);

		vad_set_enable(true);
		vad_set_ram_coeff(len_ram, p_win_coeff);
		vad_set_de_params(len_de, p_de_coeff);
		vad_set_pwd();
		vad_set_cep();
		vad_set_src(p_vad->src);
		vad_set_in();

		/* reset then enable VAD */
		vad_set_enable(false);
	}
	vad_set_enable(enable);
}

static int vad_get_enable_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct vad *p_vad = snd_kcontrol_chip(kcontrol);

	if (!p_vad) {
		pr_debug("VAD is not inited\n");
		return 0;
	}

	ucontrol->value.integer.value[0] = p_vad->en;

	return 0;
}

static int vad_set_enable_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct vad *p_vad = snd_kcontrol_chip(kcontrol);

	if (!p_vad) {
		pr_debug("VAD is not inited\n");
		return 0;
	}

	if (p_vad->en == ucontrol->value.integer.value[0])
		return 0;

	p_vad->en = ucontrol->value.integer.value[0];

	if (p_vad->en) {
		vad_init(p_vad);

		aml_set_vad(p_vad->en, p_vad->src);
	} else {
		aml_set_vad(p_vad->en, p_vad->src);

		vad_deinit(p_vad);
	}

	return 0;
}


static const char *const vad_src_txt[] = {
	"TDMIN_A",
	"TDMIN_B",
	"TDMIN_C",
	"SPDIFIN",
	"PDMIN",
	"LOOPBACK_B",
	"TDMIN_LB",
	"LOOPBACK_A",
};

const struct soc_enum vad_src_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(vad_src_txt),
			vad_src_txt);

static int vad_get_src_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct vad *p_vad = snd_kcontrol_chip(kcontrol);

	if (!p_vad) {
		pr_debug("VAD is not inited\n");
		return 0;
	}

	ucontrol->value.integer.value[0] = p_vad->src;

	return 0;
}

static int vad_set_src_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct vad *p_vad = snd_kcontrol_chip(kcontrol);

	if (!p_vad) {
		pr_debug("VAD is not inited\n");
		return 0;
	}

	p_vad->src = ucontrol->value.integer.value[0];

	if (p_vad->en)
		aml_set_vad(p_vad->en, p_vad->src);

	return 0;
}

static int vad_get_switch_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct vad *p_vad = snd_kcontrol_chip(kcontrol);

	if (!p_vad) {
		pr_debug("VAD is not inited\n");
		return 0;
	}

	ucontrol->value.integer.value[0] = p_vad->switch_buffer;

	return 0;
}

static int vad_set_switch_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct vad *p_vad = snd_kcontrol_chip(kcontrol);

	if (!p_vad) {
		pr_debug("VAD is not inited\n");
		return 0;
	}

	if (p_vad->en)
		vad_update_buffer(ucontrol->value.integer.value[0]);

	return 0;
}

static int vad_test_get_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = vad_in_kernel_test;

	return 0;
}

static int vad_test_set_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	vad_in_kernel_test = ucontrol->value.integer.value[0];

	return 0;
}


static const struct snd_kcontrol_new vad_controls[] = {
	SOC_SINGLE_BOOL_EXT("VAD enable",
		0,
		vad_get_enable_enum,
		vad_set_enable_enum),

	SOC_ENUM_EXT("VAD Source sel",
		vad_src_enum,
		vad_get_src_enum,
		vad_set_src_enum),

	SOC_SINGLE_BOOL_EXT("VAD Switch",
		0,
		vad_get_switch_enum,
		vad_set_switch_enum),

	SOC_SINGLE_BOOL_EXT("VAD Test",
		0,
		vad_test_get_enum,
		vad_test_set_enum),
};

int card_add_vad_kcontrols(struct snd_soc_card *card)
{
	unsigned int idx;
	int err;

	struct vad *p_vad = get_vad();

	if (!p_vad)
		return -ENODEV;

	for (idx = 0; idx < ARRAY_SIZE(vad_controls); idx++) {
		err = snd_ctl_add(card->snd_card,
				snd_ctl_new1(&vad_controls[idx],
				p_vad));
		if (err < 0)
			return err;
	}

	return 0;
}

static const struct of_device_id vad_device_id[] = {
	{
		.compatible = "amlogic, snd-vad",
	},
	{},
};

MODULE_DEVICE_TABLE(of, vad_device_id);

static int vad_platform_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct device_node *node_prt = NULL;
	struct platform_device *pdev_parent;
	struct device *dev = &pdev->dev;
	struct aml_audio_controller *actrl = NULL;
	struct vad *p_vad = NULL;
	int ret = 0;

	p_vad = devm_kzalloc(&pdev->dev, sizeof(struct vad), GFP_KERNEL);
	if (!p_vad)
		return -ENOMEM;

	p_vad->dev = dev;
	dev_set_drvdata(dev, p_vad);

	/* get audio controller */
	node_prt = of_get_parent(node);
	if (node_prt == NULL)
		return -ENXIO;

	pdev_parent = of_find_device_by_node(node_prt);
	of_node_put(node_prt);
	actrl = (struct aml_audio_controller *)
				platform_get_drvdata(pdev_parent);
	p_vad->actrl = actrl;

	/* clock */
	p_vad->gate = devm_clk_get(&pdev->dev, "gate");
	if (IS_ERR(p_vad->gate)) {
		dev_err(&pdev->dev,
			"Can't get vad clock gate\n");
		return PTR_ERR(p_vad->gate);
	}
	p_vad->pll = devm_clk_get(&pdev->dev, "pll");
	if (IS_ERR(p_vad->pll)) {
		dev_err(&pdev->dev,
			"Can't retrieve vad pll clock\n");
		return PTR_ERR(p_vad->pll);
	}
	p_vad->clk = devm_clk_get(&pdev->dev, "clk");
	if (IS_ERR(p_vad->clk)) {
		dev_err(&pdev->dev,
			"Can't retrieve vad clock\n");
		return PTR_ERR(p_vad->clk);
	}
	ret = clk_set_parent(p_vad->clk, p_vad->pll);
	if (ret) {
		dev_err(&pdev->dev,
			"Can't set p_vad->clk parent clock\n");
		return PTR_ERR(p_vad->clk);
	}

	/* irqs */
	p_vad->irq_wakeup = platform_get_irq_byname(pdev, "irq_wakeup");
	if (p_vad->irq_wakeup < 0) {
		dev_err(dev, "Failed to get irq_wakeup:%d\n",
			p_vad->irq_wakeup);
		return -ENXIO;
	}
	p_vad->irq_fs = platform_get_irq_byname(pdev, "irq_frame_sync");
	if (p_vad->irq_fs < 0) {
		dev_err(dev, "Failed to get irq_frame_sync:%d\n",
			p_vad->irq_fs);
		return -ENXIO;
	}

	/* data source select */
	ret = of_property_read_u32(node, "src",
			&p_vad->src);
	if (ret < 0) {
		dev_err(dev, "Failed to get vad data src select:%d\n",
			p_vad->src);
		return -EINVAL;
	}
	/* to deal with hot word in user space or kernel space */
	ret = of_property_read_u32(node, "level",
			&p_vad->level);
	if (ret < 0) {
		dev_info(dev,
			"Failed to get vad level, default in user space\n");
		p_vad->level = 0;
	}

	pr_info("%s vad data source sel:%d, level:%d\n",
		__func__,
		p_vad->src,
		p_vad->level);

	p_vad->input_device = input_allocate_device();
	if (!p_vad->input_device) {
		kfree(p_vad);
		return -ENOMEM;
	}
	vad_input_device_init(p_vad);
	if (input_register_device(p_vad->input_device)) {
		input_free_device(p_vad->input_device);
		kfree(p_vad);
		return -EINVAL;
	}

	s_vad = p_vad;

	device_init_wakeup(dev, 1);

	return 0;
}

int vad_platform_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct device *dev = &pdev->dev;
	struct vad *p_vad = dev_get_drvdata(dev);

	pr_info("%s\n", __func__);

	/* whether in freeze */
	if (is_pm_freeze_mode()
		&& vad_is_enable()) {
		pr_info("%s, Entry in freeze\n", __func__);

		if (p_vad->level == LEVEL_USER)
			dev_pm_set_wake_irq(dev, p_vad->irq_wakeup);
	}

	return 0;
}

int vad_platform_resume(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct vad *p_vad = dev_get_drvdata(dev);

	pr_info("%s\n", __func__);

	/* whether in freeze mode */
	if (is_pm_freeze_mode()
		&& vad_is_enable()) {
		pr_info("%s, Exist from freeze\n", __func__);

		if (p_vad->level == LEVEL_USER)
			dev_pm_clear_wake_irq(dev);
	}

	return 0;
}

struct platform_driver vad_driver = {
	.driver = {
		.name = DRV_NAME,
		.of_match_table = vad_device_id,
	},
	.probe   = vad_platform_probe,
	.suspend = vad_platform_suspend,
	.resume  = vad_platform_resume,
};

module_platform_driver(vad_driver);

MODULE_AUTHOR("Amlogic, Inc.");
MODULE_DESCRIPTION("Amlogic Voice Activity Detection ASoc driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("Platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, vad_device_id);
