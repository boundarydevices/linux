/*
 * sound/soc/amlogic/auge/ddr_mngr.c
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
#define pr_fmt(fmt) "audio_ddr_mngr: " fmt

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/of_device.h>

#include "regs.h"
#include "ddr_mngr.h"
#include "audio_utils.h"

#include "resample.h"
#include "resample_hw.h"
#include "effects_hw.h"
#include "effects_hw_v2.h"
#include "effects_v2.h"
#include "pwrdet_hw.h"

#define DRV_NAME "audio-ddr-manager"

static DEFINE_MUTEX(ddr_mutex);
#if 0
struct ddr_desc {
	/* start address of DDR */
	unsigned int start;
	/* finish address of DDR */
	unsigned int finish;
	/* interrupt address or counts of DDR blocks */
	unsigned int intrpt;
	/* fifo total counts */
	unsigned int fifo_depth;
	/* fifo start threshold */
	unsigned int fifo_thr;
	enum ddr_types data_type;
	unsigned int edian;
	unsigned int pp_mode;
	//unsigned int reg_base;
	struct clk *ddr;
	struct clk *ddr_arb;
};
#endif

struct ddr_chipinfo {
	/* INT and Start address is same or separated */
	bool int_start_same_addr;
	/* force finished */
	bool force_finished;
	/* same source */
	bool same_src_fn;
	/* insert channel number */
	bool insert_chnum;

	/* ddr bus in urgent */
	bool ugt;

	/* source sel switch to ctrl1
	 * for toddr, 0: source sel is controlled by ctrl0
	 *            1: source sel is controlled by ctrl1
	 * for frddr, 0: source sel is controlled by ctrl0
	 *            1: source sel is controlled by ctrl2
	 */
	bool src_sel_ctrl;

	/*
	 * resample source sel switch
	 * resample : from ctrl0 to ctrl3
	 * toddr : from ctrl0 to ctrl1
	 */
	bool asrc_src_sel_ctrl;
	/* spdif in 32bit, only support left justified */
	bool asrc_only_left_j;

	/* toddr number max
	 * 0: default, 3 toddr, axg, g12a, g12b
	 * 4: 4 toddr, tl1
	 */
	int fifo_num;
};

struct toddr {
	//struct ddr_desc dscrpt;
	struct device *dev;
	unsigned int resample: 1;
	unsigned int ext_signed: 1;
	unsigned int msb_bit;
	unsigned int lsb_bit;
	unsigned int reg_base;
	unsigned int bitdepth;
	unsigned int channels;
	unsigned int rate;
	enum toddr_src src;
	unsigned int fifo_id;

	unsigned int asrc_src_sel;

	int is_lb; /* check whether for loopback */
	int irq;
	bool in_use: 1;
	struct aml_audio_controller *actrl;
	struct ddr_chipinfo *chipinfo;
};

enum status {
	DISABLED,
	READY,    /* controls has set enable, but ddr is not in running */
	RUNNING,
};

struct toddr_attach {
	bool enable;
	int id;
	int status;
	/* which module should be attached,
	 * check which toddr in use should be attached
	 */
	enum toddr_src attach_module;
};

struct frddr_attach {
	bool enable;
	int status;
	/* which module for attach ,
	 * check which frddr in use should be added
	 */
	enum frddr_dest attach_module;
};

struct frddr {
	//struct ddr_desc dscrpt;
	struct device *dev;
	enum frddr_dest dest;
	struct aml_audio_controller *actrl;
	unsigned int reg_base;
	unsigned int fifo_id;

	unsigned int msb;
	unsigned int type;

	int irq;
	bool in_use;
	struct ddr_chipinfo *chipinfo;
};

#define DDRMAX 4
static struct frddr frddrs[DDRMAX];
static struct toddr toddrs[DDRMAX];

/* resample */
static struct toddr_attach attach_resample_a;
static struct toddr_attach attach_resample_b;
static void aml_check_resample(struct toddr *to, bool enable);

/* power detect */
static struct toddr_attach attach_pwrdet;
static void aml_check_pwrdet(bool enable);
static bool aml_check_pwrdet_module(int src);

/* Audio EQ DRC */
static struct frddr_attach attach_aed;
static void aml_check_aed(bool enable, int dst);
static bool aml_check_aed_module(int dst);

/* to DDRS */
static struct toddr *register_toddr_l(struct device *dev,
	struct aml_audio_controller *actrl,
	irq_handler_t handler, void *data)
{
	struct toddr *to;
	unsigned int mask_bit;
	int i, ret;

	/* lookup unused toddr */
	for (i = 0; i < DDRMAX; i++) {
		if (!toddrs[i].in_use)
			break;
	}

	if (i >= DDRMAX)
		return NULL;

	to = &toddrs[i];

	/* irqs request */
	ret = request_irq(to->irq, handler,
		0, dev_name(dev), data);
	if (ret) {
		dev_err(dev, "failed to claim irq %u\n", to->irq);
		return NULL;
	}
	/* enable audio ddr arb */
	mask_bit = i;
	aml_audiobus_update_bits(actrl, EE_AUDIO_ARB_CTRL,
			1<<31|1<<mask_bit, 1<<31|1<<mask_bit);

	to->dev = dev;
	to->actrl = actrl;
	to->in_use = true;
	pr_info("toddrs[%d] registered by device %s\n", i, dev_name(dev));
	return to;
}

static int unregister_toddr_l(struct device *dev, void *data)
{
	struct toddr *to;
	struct aml_audio_controller *actrl;
	unsigned int mask_bit;
	unsigned int value;
	int i;

	if (dev == NULL)
		return -EINVAL;

	for (i = 0; i < DDRMAX; i++) {
		if ((toddrs[i].dev) == dev && toddrs[i].in_use)
			break;
	}

	if (i >= DDRMAX)
		return -EINVAL;

	to = &toddrs[i];

	/* check for loopback */
	if (to->is_lb) {
		loopback_set_status(0);
		to->is_lb = 0;
	}

	/* disable audio ddr arb */
	mask_bit = i;
	actrl = to->actrl;
	aml_audiobus_update_bits(actrl, EE_AUDIO_ARB_CTRL,
			1<<mask_bit, 0<<mask_bit);
	/* no ddr active, disable arb switch */
	value = aml_audiobus_read(actrl, EE_AUDIO_ARB_CTRL) & 0x77;
	if (value == 0)
		aml_audiobus_update_bits(actrl, EE_AUDIO_ARB_CTRL,
				1<<31, 0<<31);

	free_irq(to->irq, data);
	to->dev = NULL;
	to->actrl = NULL;
	to->in_use = false;
	pr_info("toddrs[%d] released by device %s\n", i, dev_name(dev));

	return 0;
}

int fetch_toddr_index_by_src(int toddr_src)
{
	int i;

	for (i = 0; i < DDRMAX; i++) {
		if (toddrs[i].in_use
			&& (toddrs[i].src == toddr_src)) {
			return i;
		}
	}

	pr_err("invalid toddr src\n");

	return -1;
}

struct toddr *fetch_toddr_by_src(int toddr_src)
{
	int i;

	for (i = 0; i < DDRMAX; i++) {
		if (toddrs[i].in_use
			&& (toddrs[i].src == toddr_src)) {
			return &toddrs[i];
		}
	}

	pr_err("invalid toddr src\n");

	return NULL;
}

struct toddr *aml_audio_register_toddr(struct device *dev,
	struct aml_audio_controller *actrl,
	irq_handler_t handler, void *data)
{
	struct toddr *to = NULL;

	mutex_lock(&ddr_mutex);
	to = register_toddr_l(dev, actrl,
		handler, data);
	mutex_unlock(&ddr_mutex);
	return to;
}

int aml_audio_unregister_toddr(struct device *dev, void *data)
{
	int ret;

	mutex_lock(&ddr_mutex);
	ret = unregister_toddr_l(dev, data);
	mutex_unlock(&ddr_mutex);
	return ret;
}

static inline unsigned int
	calc_toddr_address(unsigned int reg, unsigned int base)
{
	return base + reg - EE_AUDIO_TODDR_A_CTRL0;
}

int aml_toddr_set_buf(struct toddr *to, unsigned int start,
			unsigned int end)
{
	struct aml_audio_controller *actrl = to->actrl;
	unsigned int reg_base = to->reg_base;
	unsigned int reg;

	reg = calc_toddr_address(EE_AUDIO_TODDR_A_START_ADDR, reg_base);
	aml_audiobus_write(actrl, reg, start);
	reg = calc_toddr_address(EE_AUDIO_TODDR_A_FINISH_ADDR, reg_base);
	aml_audiobus_write(actrl, reg, end);

	/* int address */
	if (to->chipinfo
		&& (!to->chipinfo->int_start_same_addr)) {
		reg = calc_toddr_address(EE_AUDIO_TODDR_A_INIT_ADDR, reg_base);
		aml_audiobus_write(actrl, reg, start);
	}

	return 0;
}

int aml_toddr_set_intrpt(struct toddr *to, unsigned int intrpt)
{
	struct aml_audio_controller *actrl = to->actrl;
	unsigned int reg_base = to->reg_base;
	unsigned int reg;

	reg = calc_toddr_address(EE_AUDIO_TODDR_A_INT_ADDR, reg_base);
	aml_audiobus_write(actrl, reg, intrpt);
	reg = calc_toddr_address(EE_AUDIO_TODDR_A_CTRL0, reg_base);
	aml_audiobus_update_bits(actrl, reg, 0xff<<16, 4<<16);

	return 0;
}

unsigned int aml_toddr_get_position(struct toddr *to)
{
	struct aml_audio_controller *actrl = to->actrl;
	unsigned int reg_base = to->reg_base;
	unsigned int reg;

	reg = calc_toddr_address(EE_AUDIO_TODDR_A_STATUS2, reg_base);
	return aml_audiobus_read(actrl, reg);
}

void aml_toddr_enable(struct toddr *to, bool enable)
{
	struct aml_audio_controller *actrl = to->actrl;
	unsigned int reg_base = to->reg_base;
	unsigned int reg;

	reg = calc_toddr_address(EE_AUDIO_TODDR_A_CTRL0, reg_base);
	aml_audiobus_update_bits(actrl,	reg, 1<<31, enable<<31);

	/* check resample */
	aml_check_resample(to, enable);

	/* check power detect */
	if (aml_check_pwrdet_module(to->src))
		aml_check_pwrdet(enable);

	if (!enable)
		aml_audiobus_write(actrl, reg, 0x0);
}

void aml_toddr_select_src(struct toddr *to, enum toddr_src src)
{
	struct aml_audio_controller *actrl = to->actrl;
	unsigned int reg_base = to->reg_base;
	unsigned int reg;

	/* store to check toddr num */
	to->src = src;

	/* check whether loopback enable */
	if (loopback_check_enable(src)) {
		loopback_set_status(1);
		to->is_lb = 1; /* in loopback */
		src = LOOPBACK_A;
	}

	if (to->chipinfo
		&& to->chipinfo->src_sel_ctrl) {
		reg = calc_toddr_address(EE_AUDIO_TODDR_A_CTRL1, reg_base);
		aml_audiobus_update_bits(actrl, reg,
			0xf << 28,
			(src & 0xf) << 28);
	} else {
		reg = calc_toddr_address(EE_AUDIO_TODDR_A_CTRL0, reg_base);
		aml_audiobus_update_bits(actrl,	reg, 0x7, src & 0x7);
	}
}

void aml_toddr_set_fifos(struct toddr *to, unsigned int thresh)
{
	struct aml_audio_controller *actrl = to->actrl;
	unsigned int reg_base = to->reg_base;
	unsigned int reg, mask, val;

	reg = calc_toddr_address(EE_AUDIO_TODDR_A_CTRL1, reg_base);

	if (to->chipinfo
			&& to->chipinfo->src_sel_ctrl) {
		mask = 0xfff << 12 | 0xf << 8;
		val = (thresh-1) << 12 | 2 << 8;
	} else {
		mask = 0xff << 16 | 0xf << 8;
		val = (thresh-1) << 16 | 2 << 8;
	}

	aml_audiobus_update_bits(actrl, reg, mask, val);

	if (to->chipinfo && to->chipinfo->ugt) {
		reg = calc_toddr_address(EE_AUDIO_TODDR_A_CTRL0, reg_base);
		aml_audiobus_update_bits(actrl, reg, 0x0 << 0, 0x1 << 0);
	}
}

void aml_toddr_set_format(struct toddr *to, struct toddr_fmt *fmt)
{
	struct aml_audio_controller *actrl = to->actrl;
	unsigned int reg_base = to->reg_base;
	unsigned int reg;

	to->bitdepth = fmt->bit_depth;
	to->channels = fmt->ch_num;
	to->rate     = fmt->rate;

	reg = calc_toddr_address(EE_AUDIO_TODDR_A_CTRL0, reg_base);
	aml_audiobus_update_bits(actrl, reg,
		0x7 << 24 | 0x1fff << 3,
		fmt->endian << 24 | fmt->type << 13 |
		fmt->msb << 8 | fmt->lsb << 3);
}

void aml_toddr_insert_chanum(struct toddr *to)
{
	struct aml_audio_controller *actrl = to->actrl;
	unsigned int reg_base = to->reg_base;
	unsigned int reg;

	reg = calc_toddr_address(EE_AUDIO_TODDR_A_CTRL1, reg_base);
	aml_audiobus_update_bits(actrl, reg, 1 << 24, 1 << 24);
}

unsigned int aml_toddr_read(struct toddr *to)
{
	struct aml_audio_controller *actrl = to->actrl;
	unsigned int reg_base = to->reg_base;
	unsigned int reg;

	reg = calc_toddr_address(EE_AUDIO_TODDR_A_CTRL0, reg_base);

	return aml_audiobus_read(actrl, reg);
}

void aml_toddr_write(struct toddr *to, unsigned int val)
{
	struct aml_audio_controller *actrl = to->actrl;
	unsigned int reg_base = to->reg_base;
	unsigned int reg;

	reg = calc_toddr_address(EE_AUDIO_TODDR_A_CTRL0, reg_base);

	aml_audiobus_write(actrl, reg, val);
}

void aml_toddr_set_resample(struct toddr *to, bool enable)
{
	struct aml_audio_controller *actrl = to->actrl;
	unsigned int reg_base = to->reg_base;
	unsigned int reg;

	pr_info("toddr selects data to %s resample\n",
		enable ? "enable" : "disable");
	reg = calc_toddr_address(EE_AUDIO_TODDR_A_CTRL0, reg_base);
	aml_audiobus_update_bits(actrl,	reg, 1<<30, enable<<30);
}

void aml_toddr_set_resample_ab(struct toddr *to, int asrc_src_sel, bool enable)
{
	struct aml_audio_controller *actrl = to->actrl;
	unsigned int reg_base = to->reg_base;
	unsigned int reg;

	pr_info("toddr selects data to %s resample %c\n",
		enable ? "enable" : "disable",
		(asrc_src_sel == 0) ? 'a' : 'b');
	reg = calc_toddr_address(EE_AUDIO_TODDR_A_CTRL1, reg_base);
	if (asrc_src_sel == 0)
		aml_audiobus_update_bits(actrl,	reg, 1 << 27, enable << 27);
	else
		aml_audiobus_update_bits(actrl,	reg, 1 << 26, enable << 26);
}

static void aml_resample_enable(
	struct toddr_attach *p_attach_resample,
	bool enable)
{
	struct toddr *to = fetch_toddr_by_src(p_attach_resample->attach_module);

	if (!to)
		return;

	if (to->chipinfo
			&& to->chipinfo->asrc_src_sel_ctrl) {
		/* fix asrc_src_sel */
		switch (p_attach_resample->attach_module) {
		case LOOPBACK_A:
			to->asrc_src_sel = ASRC_LOOPBACK_A;
			break;
		case LOOPBACK_B:
			to->asrc_src_sel = ASRC_LOOPBACK_B;
			break;
		default:
			to->asrc_src_sel = to->fifo_id;
			break;
		}
	}

	pr_info("Resample %d in running, module:%d, toddr:%d, asrc_src_sel:%d\n",
		p_attach_resample->id,
		p_attach_resample->attach_module,
		to->fifo_id,
		to->asrc_src_sel);

	if (enable) {
		int bitwidth = to->bitdepth;
		/* channels and bit depth for resample */
		if (to->chipinfo
			&& to->chipinfo->asrc_only_left_j
			&& (to->src == SPDIFIN)
			&& (bitwidth == 32)) {
			struct aml_audio_controller *actrl = to->actrl;
			unsigned int reg_base = to->reg_base;
			unsigned int reg;
			unsigned int endian, toddr_type;

			/* TODO: fixed me */
			pr_info("Warning: Not support 32bit sample rate for axg chipset\n");
			bitwidth = 24;
			endian = 5;
			toddr_type = 4;

			/* FIX ME */
			reg = calc_toddr_address(EE_AUDIO_TODDR_A_CTRL0,
				reg_base);
			aml_audiobus_update_bits(actrl, reg,
				0x7 << 24 | 0x7 << 13,
				endian << 24 | toddr_type << 13);
		}

		resample_format_set(p_attach_resample->id,
			to->channels, bitwidth);

		/* toddr index for resample */
		if (to->chipinfo
			&& to->chipinfo->asrc_src_sel_ctrl)
			resample_src_select_ab(p_attach_resample->id,
				to->asrc_src_sel);
		else
			resample_src_select(to->fifo_id);
	}

	/* resample enable or not */
	resample_enable(p_attach_resample->id, enable);

	/* select reample data */
	if (to->chipinfo
			&& to->chipinfo->asrc_src_sel_ctrl)
		aml_toddr_set_resample_ab(to, p_attach_resample->id, enable);
	else
		aml_toddr_set_resample(to, enable);
}

void aml_set_resample(int id, bool enable, int resample_module)
{
	struct toddr_attach *p_attach_resample;

	bool update_running = false;

	if (id == 0)
		p_attach_resample = &attach_resample_a;
	else
		p_attach_resample = &attach_resample_b;

	p_attach_resample->enable        = enable;
	p_attach_resample->id            = id;
	p_attach_resample->attach_module = resample_module;

	if (enable) {
		if ((p_attach_resample->status == DISABLED)
			|| (p_attach_resample->status == READY)) {
			struct toddr *to = fetch_toddr_by_src(
				p_attach_resample->attach_module);

			if (!to) {
				p_attach_resample->status = READY;
			} else {
				p_attach_resample->status = RUNNING;
				update_running = true;
				pr_info("Capture with resample\n");
			}
		}
	} else {
		if (p_attach_resample->status == RUNNING)
			update_running = true;

		p_attach_resample->status = DISABLED;
	}

	if (update_running)
		aml_resample_enable(p_attach_resample, enable);
}

/*
 * when try to enable resample, if toddr is not in used,
 * set resample status as ready
 */
static void aml_check_resample(struct toddr *to, bool enable)
{
	struct toddr_attach *p_attach_resample;
	bool is_module_resample;
	bool resample_b_check = false;

	p_attach_resample = &attach_resample_a;

start_check:
	is_module_resample = false;
	if (p_attach_resample->enable
		&& (to->src == p_attach_resample->attach_module))
		is_module_resample = true;

	/* resample in enable */
	if (is_module_resample) {
		if (enable)
			p_attach_resample->status = RUNNING;
		else
			p_attach_resample->status = DISABLED;

		aml_resample_enable(p_attach_resample, enable);
	}

	if ((!resample_b_check)
		&& (get_resample_module_num() == 2)) {
		p_attach_resample = &attach_resample_b;
		resample_b_check = true;
		goto start_check;
	}
}

static void aml_set_pwrdet(struct toddr *to,
	bool enable)
{
	if (enable) {
		struct aml_audio_controller *actrl = to->actrl;
		unsigned int reg_base = to->reg_base;
		unsigned int reg, val;
		unsigned int toddr_type, msb, lsb;

		reg = calc_toddr_address(EE_AUDIO_TODDR_A_CTRL0, reg_base);
		val = aml_audiobus_read(actrl, reg);
		toddr_type = (val >> 13) & 0x7;
		msb = (val >> 8) & 0x1f;
		lsb = (val >> 3) & 0x1f;

		aml_pwrdet_format_set(toddr_type, msb, lsb);
	}
	pwrdet_src_select(enable, to->src);
}

void aml_pwrdet_enable(bool enable, int pwrdet_module)
{
	attach_pwrdet.enable = enable;
	attach_pwrdet.attach_module = pwrdet_module;
	if (enable) {
		if ((attach_pwrdet.status == DISABLED)
			|| (attach_pwrdet.status == READY)) {
			struct toddr *to = fetch_toddr_by_src(pwrdet_module);

			if (!to) {
				attach_pwrdet.status = READY;
				pr_info("not in capture, power detect is ready\n");
			} else {
				attach_pwrdet.status = RUNNING;
				aml_set_pwrdet(to, enable);
			}
		}
	} else {
		if (attach_pwrdet.status == RUNNING) {
			struct toddr *to = fetch_toddr_by_src(pwrdet_module);

			aml_set_pwrdet(to, enable);
		}
		attach_pwrdet.status = DISABLED;
	}
}

static bool aml_check_pwrdet_module(int src)
{
	bool is_module_pwrdet = false;

	if (attach_pwrdet.enable
		&& (src == attach_pwrdet.attach_module))
		is_module_pwrdet = true;

	return is_module_pwrdet;
}

static void aml_check_pwrdet(bool enable)
{
	/* power detect in enable */
	if (attach_pwrdet.enable) {
		if (enable) {
			/* check whether ready ? */
			if (attach_pwrdet.status == READY)
				aml_pwrdet_enable(true,
					attach_pwrdet.attach_module);
		} else {
			if (attach_pwrdet.status == RUNNING)
				attach_pwrdet.status = READY;
		}
	}
}

/* from DDRS */
static struct frddr *register_frddr_l(struct device *dev,
	struct aml_audio_controller *actrl,
	irq_handler_t handler, void *data)
{
	struct frddr *from;
	unsigned int mask_bit;
	int i, ret;

	/* lookup unused frddr */
	for (i = 0; i < DDRMAX; i++) {
		if (!frddrs[i].in_use)
			break;
	}

	if (i >= DDRMAX)
		return NULL;

	from = &frddrs[i];

	/* enable audio ddr arb */
	mask_bit = i + 4;
	aml_audiobus_update_bits(actrl, EE_AUDIO_ARB_CTRL,
			1<<31|1<<mask_bit, 1<<31|1<<mask_bit);

	/* irqs request */
	ret = request_irq(from->irq, handler,
		0, dev_name(dev), data);
	if (ret) {
		dev_err(dev, "failed to claim irq %u\n", from->irq);
		return NULL;
	}
	from->dev = dev;
	from->actrl = actrl;
	from->in_use = true;
	pr_info("frddrs[%d] registered by device %s\n", i, dev_name(dev));
	return from;
}

static int unregister_frddr_l(struct device *dev, void *data)
{
	struct frddr *from;
	struct aml_audio_controller *actrl;
	unsigned int mask_bit;
	unsigned int value;
	int i;

	if (dev == NULL)
		return -EINVAL;

	for (i = 0; i < DDRMAX; i++) {
		if ((frddrs[i].dev) == dev && frddrs[i].in_use)
			break;
	}

	if (i >= DDRMAX)
		return -EINVAL;

	from = &frddrs[i];

	/* disable audio ddr arb */
	mask_bit = i + 4;
	actrl = from->actrl;
	aml_audiobus_update_bits(actrl, EE_AUDIO_ARB_CTRL,
			1<<mask_bit, 0<<mask_bit);
	/* no ddr active, disable arb switch */
	value = aml_audiobus_read(actrl, EE_AUDIO_ARB_CTRL) & 0x77;
	if (value == 0)
		aml_audiobus_update_bits(actrl, EE_AUDIO_ARB_CTRL,
				1<<31, 0<<31);

	free_irq(from->irq, data);
	from->dev = NULL;
	from->actrl = NULL;
	from->in_use = false;
	pr_info("frddrs[%d] released by device %s\n", i, dev_name(dev));
	return 0;
}

int fetch_frddr_index_by_src(int frddr_src)
{
	int i;

	for (i = 0; i < DDRMAX; i++) {
		if (frddrs[i].in_use
			&& (frddrs[i].dest == frddr_src)) {
			return i;
		}
	}

	pr_err("invalid frdd_src\n");
	return -1;
}

struct frddr *fetch_frddr_by_src(int frddr_src)
{
	int i;

	for (i = 0; i < DDRMAX; i++) {
		if (frddrs[i].in_use
			&& (frddrs[i].dest == frddr_src)) {
			return &frddrs[i];
		}
	}

	pr_err("invalid frddr src\n");

	return NULL;
}

/*
 * check frddr_src is used by other frddr for sharebuffer
 * if used, disabled the other share frddr src, the module would
 * for current frddr, and the checked frddr
 */
int aml_check_sharebuffer_valid(struct frddr *fr, int ss_sel)
{
	int current_fifo_id = fr->fifo_id;
	unsigned int i;
	int ret = 1;

	for (i = 0; i < DDRMAX; i++) {
		if (frddrs[i].in_use
			&& (frddrs[i].fifo_id != current_fifo_id)
			&& (frddrs[i].dest == ss_sel)) {

			pr_info("ss_sel:%d used, invalid for share buffer\n",
				ss_sel);
			ret = 0;
			break;
		}
	}

	return ret;
}

struct frddr *aml_audio_register_frddr(struct device *dev,
	struct aml_audio_controller *actrl,
	irq_handler_t handler, void *data)
{
	struct frddr *fr = NULL;

	mutex_lock(&ddr_mutex);
	fr = register_frddr_l(dev, actrl, handler, data);
	mutex_unlock(&ddr_mutex);
	return fr;
}

int aml_audio_unregister_frddr(struct device *dev, void *data)
{
	int ret;

	mutex_lock(&ddr_mutex);
	ret = unregister_frddr_l(dev, data);
	mutex_unlock(&ddr_mutex);
	return ret;
}

static inline unsigned int
	calc_frddr_address(unsigned int reg, unsigned int base)
{
	return base + reg - EE_AUDIO_FRDDR_A_CTRL0;
}

int aml_frddr_set_buf(struct frddr *fr, unsigned int start,
			unsigned int end)
{
	struct aml_audio_controller *actrl = fr->actrl;
	unsigned int reg_base = fr->reg_base;
	unsigned int reg;

	reg = calc_frddr_address(EE_AUDIO_FRDDR_A_START_ADDR, reg_base);
	aml_audiobus_write(actrl, reg, start);
	reg = calc_frddr_address(EE_AUDIO_FRDDR_A_FINISH_ADDR, reg_base);
	aml_audiobus_write(actrl, reg, end);

	/* int address */
	if (fr->chipinfo
		&& (!fr->chipinfo->int_start_same_addr)) {
		reg = calc_frddr_address(EE_AUDIO_FRDDR_A_INIT_ADDR, reg_base);
		aml_audiobus_write(actrl, reg, start);
	}

	return 0;
}

int aml_frddr_set_intrpt(struct frddr *fr, unsigned int intrpt)
{
	struct aml_audio_controller *actrl = fr->actrl;
	unsigned int reg_base = fr->reg_base;
	unsigned int reg;

	reg = calc_frddr_address(EE_AUDIO_FRDDR_A_INT_ADDR, reg_base);
	aml_audiobus_write(actrl, reg, intrpt);
	reg = calc_frddr_address(EE_AUDIO_FRDDR_A_CTRL0, reg_base);
	aml_audiobus_update_bits(actrl, reg, 0xff<<16, 4<<16);

	return 0;
}

unsigned int aml_frddr_get_position(struct frddr *fr)
{
	struct aml_audio_controller *actrl = fr->actrl;
	unsigned int reg_base = fr->reg_base;
	unsigned int reg;

	reg = calc_frddr_address(EE_AUDIO_FRDDR_A_STATUS2, reg_base);
	return aml_audiobus_read(actrl, reg);
}

void aml_frddr_enable(struct frddr *fr, bool enable)
{
	struct aml_audio_controller *actrl = fr->actrl;
	unsigned int reg_base = fr->reg_base;
	unsigned int reg;

	reg = calc_frddr_address(EE_AUDIO_FRDDR_A_CTRL0, reg_base);
	/* ensure disable before enable frddr */
	aml_audiobus_update_bits(actrl,	reg, 1<<31, enable<<31);

	if (!enable)
		aml_audiobus_write(actrl, reg, 0x0);

	/* check for Audio EQ/DRC */
	if (aml_check_aed_module(fr->dest))
		aml_check_aed(enable, fr->dest);
}

void aml_frddr_select_dst(struct frddr *fr, enum frddr_dest dst)
{
	struct aml_audio_controller *actrl = fr->actrl;
	unsigned int reg_base = fr->reg_base;
	unsigned int reg, src_sel_en;

	fr->dest = dst;

	if (fr->chipinfo
		&& fr->chipinfo->src_sel_ctrl) {
		reg = calc_frddr_address(EE_AUDIO_FRDDR_A_CTRL2, reg_base);
		src_sel_en = 4;
	} else {
		reg = calc_frddr_address(EE_AUDIO_FRDDR_A_CTRL0, reg_base);
		src_sel_en = 3;
	}

	aml_audiobus_update_bits(actrl, reg, 0x7, dst & 0x7);

	/* same source en */
	if (fr->chipinfo
		&& fr->chipinfo->same_src_fn) {
		aml_audiobus_update_bits(actrl, reg,
			1 << src_sel_en, 1 << src_sel_en);
	}
}

/* select dst for same source
 * sel: share buffer req_sel 1~2
 * sel 0 is aleardy used for reg_frddr_src_sel1
 * sel 1 is for reg_frddr_src_sel2
 * sel 2 is for reg_frddr_src_sel3
 */
void aml_frddr_select_dst_ss(struct frddr *fr,
	enum frddr_dest dst, int sel, bool enable)
{
	struct aml_audio_controller *actrl = fr->actrl;
	unsigned int reg_base = fr->reg_base;
	unsigned int reg, ss_valid;

	reg = calc_frddr_address(EE_AUDIO_FRDDR_A_CTRL0, reg_base);

	ss_valid = aml_check_sharebuffer_valid(fr, dst);

	/* same source en */
	if (fr->chipinfo
		&& fr->chipinfo->same_src_fn
		&& ss_valid) {
		int s_v = 0, s_m = 0;

			switch (sel) {
			case 1:
				s_m = 0xf << 4;
				s_v = enable ? (dst << 4 | 1 << 7) : 0 << 4;
				break;
			case 2:
				s_m = 0xf << 8;
				s_v = enable ? (dst << 8 | 1 << 11) : 0 << 8;
				break;
			default:
				pr_warn_once("sel :%d is not supported for same source\n",
					sel);
				break;
			}
			pr_info("%s sel:%d, dst_src:%d\n",
				__func__, sel, dst);
			aml_audiobus_update_bits(actrl, reg, s_m, s_v);
	}
}

void aml_frddr_set_fifos(struct frddr *fr,
		unsigned int depth, unsigned int thresh)
{
	struct aml_audio_controller *actrl = fr->actrl;
	unsigned int reg_base = fr->reg_base;
	unsigned int reg;

	reg = calc_frddr_address(EE_AUDIO_FRDDR_A_CTRL1, reg_base);
	aml_audiobus_update_bits(actrl,	reg,
			0xffff<<16 | 0xf<<8,
			(depth - 1)<<24 | (thresh - 1)<<16 | 2<<8);

	if (fr->chipinfo && fr->chipinfo->ugt) {
		reg = calc_toddr_address(EE_AUDIO_FRDDR_A_CTRL0, reg_base);
		aml_audiobus_update_bits(actrl, reg, 0x0 << 0, 0x1 << 0);
	}
}

unsigned int aml_frddr_get_fifo_id(struct frddr *fr)
{
	return fr->fifo_id;
}

void aml_frddr_set_format(struct frddr *fr,
	unsigned int msb, unsigned int frddr_type)
{
	fr->msb  = msb;
	fr->type = frddr_type;
}


static void aml_aed_enable(struct frddr_attach *p_attach_aed, bool enable)
{
	struct frddr *fr = fetch_frddr_by_src(p_attach_aed->attach_module);


	if (check_aed_v2()) {
		if (fr->chipinfo
			&& fr->chipinfo->src_sel_ctrl) {
			struct aml_audio_controller *actrl = fr->actrl;
			unsigned int reg_base = fr->reg_base;
			unsigned int reg;

			reg = calc_frddr_address(EE_AUDIO_FRDDR_A_CTRL2,
				reg_base);
			aml_audiobus_update_bits(actrl,
				reg, 0x1 << 3, enable << 3);
		}

		aed_set_ctrl(enable, 0, p_attach_aed->attach_module);
		aed_set_format(fr->msb, fr->type);
		aed_enable(enable, fr->dest, fr->fifo_id);
	} else {
		if (enable) {
			/* frddr type and bit depth for AED */
			aml_aed_format_set(fr->dest);
		}
		aed_src_select(enable, fr->dest, fr->fifo_id);
	}
}

void aml_set_aed(bool enable, int aed_module)
{
	bool update_running = false;

	/* when try to enable AED, if frddr is not in used,
	 * set AED status as ready
	 */
	attach_aed.enable = enable;
	attach_aed.attach_module = aed_module;

	if (enable) {
		if ((attach_aed.status == DISABLED)
			|| (attach_aed.status == READY)) {
			struct frddr *fr = fetch_frddr_by_src(aed_module);

			if (!fr) {
				attach_aed.status = READY;
			} else {
				attach_aed.status = RUNNING;
				update_running = true;
				pr_info("Playback with AED\n");
			}
		}
	} else {
		if (attach_aed.status == RUNNING)
			update_running = true;

		attach_aed.status = DISABLED;
	}

	if (update_running)
		aml_aed_enable(&attach_aed, enable);
}

static bool aml_check_aed_module(int dst)
{
	bool is_module_aed = false;

	if (attach_aed.enable
		&& (dst == attach_aed.attach_module))
		is_module_aed = true;

	return is_module_aed;
}

static void aml_check_aed(bool enable, int dst)
{
	/* check effect module is sync with crruent frddr dst */
	if (attach_aed.attach_module != dst)
		return;

	/* AED in enable */
	if (attach_aed.enable) {
		if (enable)
			attach_aed.status = RUNNING;
		else
			attach_aed.status = DISABLED;

		aml_aed_enable(&attach_aed, enable);
	}
}

void frddr_init_without_mngr(unsigned int frddr_index, unsigned int src0_sel)
{
	unsigned int offset, reg;
	unsigned int start_addr, end_addr, int_addr;
	static int buf[256];

	memset(buf, 0x0, sizeof(buf));
	start_addr = virt_to_phys(buf);
	end_addr = start_addr + sizeof(buf) - 1;
	int_addr = sizeof(buf) / 64;

	offset = EE_AUDIO_FRDDR_B_START_ADDR - EE_AUDIO_FRDDR_A_START_ADDR;
	reg = EE_AUDIO_FRDDR_A_START_ADDR + offset * frddr_index;
	audiobus_write(reg, start_addr);

	offset = EE_AUDIO_FRDDR_B_INIT_ADDR - EE_AUDIO_FRDDR_A_INIT_ADDR;
	reg = EE_AUDIO_FRDDR_A_INIT_ADDR + offset * frddr_index;
	audiobus_write(reg, start_addr);

	offset = EE_AUDIO_FRDDR_B_FINISH_ADDR - EE_AUDIO_FRDDR_A_FINISH_ADDR;
	reg = EE_AUDIO_FRDDR_A_FINISH_ADDR + offset * frddr_index;
	audiobus_write(reg, end_addr);

	offset = EE_AUDIO_FRDDR_B_INT_ADDR - EE_AUDIO_FRDDR_A_INT_ADDR;
	reg = EE_AUDIO_FRDDR_A_INT_ADDR + offset * frddr_index;
	audiobus_write(reg, int_addr);

	offset = EE_AUDIO_FRDDR_B_CTRL1 - EE_AUDIO_FRDDR_A_CTRL1;
	reg = EE_AUDIO_FRDDR_A_CTRL1 + offset * frddr_index;
	audiobus_write(reg,
		(0x40 - 1) << 24 | (0x20 - 1) << 16 | 2 << 8 | 0 << 0);

	offset = EE_AUDIO_FRDDR_B_CTRL0 - EE_AUDIO_FRDDR_A_CTRL0;
	reg = EE_AUDIO_FRDDR_A_CTRL0 + offset * frddr_index;
	audiobus_write(reg,
		1 << 31
		| 0 << 24
		| 4 << 16
		| 1 << 3 /* src0 enable */
		| src0_sel << 0 /* src0 sel */
	);
}

void frddr_deinit_without_mngr(unsigned int frddr_index)
{
	unsigned int offset, reg;

	offset = EE_AUDIO_FRDDR_B_CTRL0 - EE_AUDIO_FRDDR_A_CTRL0;
	reg = EE_AUDIO_FRDDR_A_CTRL0 + offset * frddr_index;
	audiobus_write(reg, 0x0);
}

static int toddr_src_idx = -1;

static const char *const toddr_src_sel_texts[] = {
	"TDMIN_A", "TDMIN_B", "TDMIN_C", "SPDIFIN",
	"PDMIN", "FRATV", "TDMIN_LB", "LOOPBACK_A",
	"FRHDMIRX", "LOOPBACK_B", "SPDIFIN_LB",
	"RESERVED", "RESERVED", "RESERVED", "RESERVED",
	"VAD"
};

static const struct soc_enum toddr_input_source_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(toddr_src_sel_texts),
		toddr_src_sel_texts);

int toddr_src_get(void)
{
	return toddr_src_idx;
}

const char *toddr_src_get_str(int idx)
{
	if (idx < 0 || idx > 15)
		return NULL;

	return toddr_src_sel_texts[idx];
}

static int toddr_src_enum_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = toddr_src_idx;

	return 0;
}

static int toddr_src_enum_set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	toddr_src_idx = ucontrol->value.enumerated.item[0];

	return 0;
}

static int frddr_src_idx = -1;

static const char *const frddr_src_sel_texts[] = {
	"TDMOUT_A", "TDMOUT_B", "TDMOUT_C", "SPDIFOUT_A", "SPDIFOUT_B"
};

static const struct soc_enum frddr_output_source_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(frddr_src_sel_texts),
		frddr_src_sel_texts);

int frddr_src_get(void)
{
	return frddr_src_idx;
}

const char *frddr_src_get_str(int idx)
{
	if (idx < 0 || idx > 4)
		return NULL;

	return frddr_src_sel_texts[idx];
}

static int frddr_src_enum_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = frddr_src_idx;

	return 0;
}

static int frddr_src_enum_set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	frddr_src_idx = ucontrol->value.enumerated.item[0];

	return 0;
}

static const struct snd_kcontrol_new snd_ddr_controls[] = {
	SOC_ENUM_EXT("Audio In Source",
		toddr_input_source_enum,
		toddr_src_enum_get,
		toddr_src_enum_set),
	SOC_ENUM_EXT("Audio Out Sink",
		toddr_input_source_enum,
		frddr_src_enum_get,
		frddr_src_enum_set),
};

int card_add_ddr_kcontrols(struct snd_soc_card *card)
{
	unsigned int idx;
	int err;

	for (idx = 0; idx < ARRAY_SIZE(snd_ddr_controls); idx++) {
		err = snd_ctl_add(card->snd_card,
				snd_ctl_new1(&snd_ddr_controls[idx],
				NULL));
		if (err < 0)
			return err;
	}

	return 0;
}

static struct ddr_chipinfo axg_ddr_chipinfo = {
	.int_start_same_addr   = true,
	.asrc_only_left_j      = true,
};

static struct ddr_chipinfo g12a_ddr_chipinfo = {
	.same_src_fn           = true,
	.asrc_only_left_j      = true,
};

static struct ddr_chipinfo tl1_ddr_chipinfo = {
	.same_src_fn           = true,
	.ugt                   = true,
	.src_sel_ctrl          = true,
	.asrc_src_sel_ctrl     = true,
	.fifo_num              = 4,
};

static const struct of_device_id aml_ddr_mngr_device_id[] = {
	{
		.compatible = "amlogic, axg-audio-ddr-manager",
		.data       = &axg_ddr_chipinfo,
	},
	{
		.compatible = "amlogic, g12a-audio-ddr-manager",
		.data       = &g12a_ddr_chipinfo,
	},
	{
		.compatible = "amlogic, tl1-audio-ddr-manager",
		.data       = &tl1_ddr_chipinfo,
	},
	{},
};
MODULE_DEVICE_TABLE(of, aml_ddr_mngr_device_id);

static int aml_ddr_mngr_platform_probe(struct platform_device *pdev)
{
	struct ddr_chipinfo *p_ddr_chipinfo;
	int ddr_num = 3; /* early chipset support max 3 ddr num */
	int i;

	p_ddr_chipinfo = (struct ddr_chipinfo *)
		of_device_get_match_data(&pdev->dev);
	if (!p_ddr_chipinfo)
		dev_warn_once(&pdev->dev,
			"check whether to update ddr_mngr chipinfo\n");

	/* irqs */
	toddrs[DDR_A].irq = platform_get_irq_byname(pdev, "toddr_a");
	toddrs[DDR_B].irq = platform_get_irq_byname(pdev, "toddr_b");
	toddrs[DDR_C].irq = platform_get_irq_byname(pdev, "toddr_c");

	frddrs[DDR_A].irq = platform_get_irq_byname(pdev, "frddr_a");
	frddrs[DDR_B].irq = platform_get_irq_byname(pdev, "frddr_b");
	frddrs[DDR_C].irq = platform_get_irq_byname(pdev, "frddr_c");

	if (p_ddr_chipinfo
		&& (p_ddr_chipinfo->fifo_num == 4)) {
		toddrs[DDR_D].irq = platform_get_irq_byname(pdev, "toddr_d");
		frddrs[DDR_D].irq = platform_get_irq_byname(pdev, "frddr_d");
		ddr_num = p_ddr_chipinfo->fifo_num;
	}

	for (i = 0; i < ddr_num; i++) {
		pr_info("%d, irqs toddr %d, frddr %d\n",
			i, toddrs[i].irq, frddrs[i].irq);
		if (toddrs[i].irq <= 0 || frddrs[i].irq <= 0) {
			dev_err(&pdev->dev, "platform_get_irq_byname failed\n");
			return -ENXIO;
		}
	}

	/* inits */
	toddrs[DDR_A].reg_base = EE_AUDIO_TODDR_A_CTRL0;
	toddrs[DDR_B].reg_base = EE_AUDIO_TODDR_B_CTRL0;
	toddrs[DDR_C].reg_base = EE_AUDIO_TODDR_C_CTRL0;
	toddrs[DDR_A].fifo_id  = DDR_A;
	toddrs[DDR_B].fifo_id  = DDR_B;
	toddrs[DDR_C].fifo_id  = DDR_C;

	frddrs[DDR_A].reg_base = EE_AUDIO_FRDDR_A_CTRL0;
	frddrs[DDR_B].reg_base = EE_AUDIO_FRDDR_B_CTRL0;
	frddrs[DDR_C].reg_base = EE_AUDIO_FRDDR_C_CTRL0;
	frddrs[DDR_A].fifo_id  = DDR_A;
	frddrs[DDR_B].fifo_id  = DDR_B;
	frddrs[DDR_C].fifo_id  = DDR_C;

	if (p_ddr_chipinfo) {
		toddrs[DDR_A].chipinfo = p_ddr_chipinfo;
		toddrs[DDR_B].chipinfo = p_ddr_chipinfo;
		toddrs[DDR_C].chipinfo = p_ddr_chipinfo;
		frddrs[DDR_A].chipinfo = p_ddr_chipinfo;
		frddrs[DDR_B].chipinfo = p_ddr_chipinfo;
		frddrs[DDR_C].chipinfo = p_ddr_chipinfo;

		if (p_ddr_chipinfo->fifo_num == 4) {
			toddrs[DDR_D].reg_base = EE_AUDIO_TODDR_D_CTRL0;
			toddrs[DDR_D].fifo_id  = DDR_D;

			frddrs[DDR_D].reg_base = EE_AUDIO_FRDDR_D_CTRL0;
			frddrs[DDR_D].fifo_id  = DDR_D;
		}
	}
	return 0;
}

struct platform_driver aml_audio_ddr_manager = {
	.driver = {
		.name = DRV_NAME,
		.of_match_table = aml_ddr_mngr_device_id,
	},
	.probe   = aml_ddr_mngr_platform_probe,
};
module_platform_driver(aml_audio_ddr_manager);

/* Module information */
MODULE_AUTHOR("Amlogic, Inc.");
MODULE_DESCRIPTION("ALSA Soc Aml Audio DDR Manager");
MODULE_LICENSE("GPL v2");

