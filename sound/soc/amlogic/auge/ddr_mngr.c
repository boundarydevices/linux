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

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/slab.h>
#include "regs.h"
#include "ddr_mngr.h"
#include "audio_utils.h"

#define DRV_NAME "aml_audio_ddr_manager"

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
struct toddr {
	//struct ddr_desc dscrpt;
	struct device *dev;
	unsigned int resample: 1;
	unsigned int ext_signed: 1;
	unsigned int msb_bit;
	unsigned int lsb_bit;
	unsigned int reg_base;
	enum toddr_src src;
	int irq;
	bool in_use: 1;
	struct aml_audio_controller *actrl;
};

struct frddr {
	//struct ddr_desc dscrpt;
	struct device *dev;
	enum frddr_dest dest;
	struct aml_audio_controller *actrl;
	unsigned int reg_base;
	unsigned int fifo_id;
	int irq;
	bool in_use;
};

#define DDRMAX 3
static struct frddr frddrs[DDRMAX];
static struct toddr toddrs[DDRMAX];

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
}

void aml_toddr_select_src(struct toddr *to, enum toddr_src src)
{
	struct aml_audio_controller *actrl = to->actrl;
	unsigned int reg_base = to->reg_base;
	unsigned int reg;

	/* store to check toddr num */
	to->src = src;

	/* check whether loopback enable */
	if (loopback_is_enable())
		src = LOOPBACK;

	reg = calc_toddr_address(EE_AUDIO_TODDR_A_CTRL0, reg_base);
	aml_audiobus_update_bits(actrl,	reg, 0x7, src & 0x7);
}

void aml_toddr_set_fifos(struct toddr *to, unsigned int thresh)
{
	struct aml_audio_controller *actrl = to->actrl;
	unsigned int reg_base = to->reg_base;
	unsigned int reg;

	reg = calc_toddr_address(EE_AUDIO_TODDR_A_CTRL1, reg_base);
	aml_audiobus_write(actrl, reg, (thresh-1)<<16|2<<8);
}

void aml_toddr_set_format(struct toddr *to,
	unsigned int type, unsigned int msb, unsigned int lsb)
{
	struct aml_audio_controller *actrl = to->actrl;
	unsigned int reg_base = to->reg_base;
	unsigned int reg;

	reg = calc_toddr_address(EE_AUDIO_TODDR_A_CTRL0, reg_base);
	aml_audiobus_update_bits(actrl, reg, 0x1fff<<3,
				type<<13|msb<<8|lsb<<3);
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
	aml_audiobus_update_bits(actrl,	reg, 1<<31, enable<<31);
}

void aml_frddr_select_dst(struct frddr *fr, enum frddr_dest dst)
{
	struct aml_audio_controller *actrl = fr->actrl;
	unsigned int reg_base = fr->reg_base;
	unsigned int reg;

	fr->dest = dst;

	reg = calc_frddr_address(EE_AUDIO_FRDDR_A_CTRL0, reg_base);
	aml_audiobus_update_bits(actrl,	reg, 0x7, dst & 0x7);
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
}

unsigned int aml_frddr_get_fifo_id(struct frddr *fr)
{
	return fr->fifo_id;
}

static int aml_ddr_mngr_platform_probe(struct platform_device *pdev)
{
	int i;

	/* irqs */
	toddrs[DDR_A].irq = platform_get_irq_byname(pdev, "toddr_a");
	toddrs[DDR_B].irq = platform_get_irq_byname(pdev, "toddr_b");
	toddrs[DDR_C].irq = platform_get_irq_byname(pdev, "toddr_c");

	frddrs[DDR_A].irq = platform_get_irq_byname(pdev, "frddr_a");
	frddrs[DDR_B].irq = platform_get_irq_byname(pdev, "frddr_b");
	frddrs[DDR_C].irq = platform_get_irq_byname(pdev, "frddr_c");

	for (i = 0; i < DDRMAX; i++) {
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
	frddrs[DDR_A].reg_base = EE_AUDIO_FRDDR_A_CTRL0;
	frddrs[DDR_B].reg_base = EE_AUDIO_FRDDR_B_CTRL0;
	frddrs[DDR_C].reg_base = EE_AUDIO_FRDDR_C_CTRL0;
	frddrs[DDR_A].fifo_id = DDR_A;
	frddrs[DDR_B].fifo_id = DDR_B;
	frddrs[DDR_C].fifo_id = DDR_C;

	return 0;
}

static const struct of_device_id aml_ddr_mngr_device_id[] = {
	{ .compatible = "amlogic, audio-ddr-manager", },
	{},
};
MODULE_DEVICE_TABLE(of, aml_ddr_mngr_device_id);

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

