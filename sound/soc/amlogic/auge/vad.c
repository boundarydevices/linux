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

#include <linux/amlogic/pm.h>
#include <linux/pm_wakeirq.h>
#include <linux/pm_wakeup.h>

#include "vad_hw_coeff.c"
#include "vad_hw.h"
#include "vad.h"

#define DRV_NAME "VAD"

#define DMA_BUFFER_BYTES_MAX (2 * 1024 * 1024)

enum vad_level {
	LEVEL_USER,
	LEVEL_KERNEL,
};

struct vad {
	struct aml_audio_controller *actrl;
	struct device *dev;

	struct clk *gate;
	struct clk *pll;
	struct clk *clk;

	struct toddr *tddr;

	struct tasklet_struct tasklet;

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

static void vad_notify_user_space(struct device *dev)
{
	pr_info("Notify to wake up user space\n");

	pm_wakeup_event(dev, 2000);
}

static int vad_engine_check(void)
{
	return 1;
}

/* Check buffer in kernel for VAD */
static void vad_transfer_buffer_output(struct vad *p_vad)
{

	for (;;) {
		if (vad_engine_check()) {
			vad_notify_user_space(p_vad->dev);
			break;
		}
	}
}

static void vad_tasklet(unsigned long data)
{
	struct vad *p_vad = (struct vad *)data;

	vad_transfer_buffer_output(p_vad);
}

static irqreturn_t vad_wakeup_isr(int irq, void *data)
{
	struct vad *p_vad = (struct vad *)data;

	pr_info("%s\n", __func__);

	if (p_vad->level == LEVEL_KERNEL)
		tasklet_schedule(&p_vad->tasklet);

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

		tasklet_init(&p_vad->tasklet, vad_tasklet,
			     (unsigned long)p_vad);

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
	if (p_vad->level == LEVEL_KERNEL)
		tasklet_kill(&p_vad->tasklet);

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
	unsigned int start, end, addr;
	unsigned int rd_th;

	if (!p_vad || !p_vad->en || !p_vad->tddr)
		return;

	addr = aml_toddr_get_position(p_vad->tddr);

	if (isvad) {	/* switch to vad buffer */
		struct toddr *tddr = p_vad->tddr;

		p_vad->start_last = tddr->start_addr;
		p_vad->end_last   = tddr->end_addr;

		rd_th = 0x100;

		pr_debug("Switch to VAD buffer\n");
		pr_debug("\t ASAL start:%d, end:%d, bytes:%d, current:%d\n",
			tddr->start_addr, tddr->end_addr,
			tddr->end_addr - tddr->start_addr, addr);

		start = p_vad->dma_buffer.addr;
		end   = start + p_vad->dma_buffer.bytes - 8;

		pr_debug("\t VAD start:%d, end:%d, bytes:%d\n",
			start, end,
			end - start);
	} else {
		pr_debug("Switch to ALSA buffer\n");

		//addr = aml_toddr_get_addr(p_vad->tddr, VAD_WAKEUP_ADDR);
		addr = aml_toddr_get_position(p_vad->tddr);

		start = p_vad->start_last;
		end   = p_vad->end_last;

		rd_th = 0x40;

		vad_set_trunk_data_readable(true);
	}

	p_vad->addr = addr;
	aml_toddr_set_buf(p_vad->tddr, start, end);
	aml_toddr_force_finish(p_vad->tddr);
	aml_toddr_update_fifos_rd_th(p_vad->tddr, rd_th);
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

	p_vad->switch_buffer = ucontrol->value.integer.value[0];

	if (p_vad->en)
		vad_update_buffer(p_vad->switch_buffer);

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
