/*
 * drivers/amlogic/input/saradc/saradc.c
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
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/amlogic/saradc.h>
#include <linux/amlogic/cpu_version.h>
#include <asm/barrier.h>
#include "saradc_reg.h"

/* #define ENABLE_DYNAMIC_POWER */
#define CLEAN_BUFF_BEFORE_SARADC 1

/* flag_12bit = 0 : 10 bit */
/* flag_12bit = 1 : 12 bit */
static char flag_12bit;

#define saradc_info(x...) dev_info(adc->dev, x)
#define saradc_dbg(x...) /* dev_info(adc->dev, x) */
#define saradc_err(x...) dev_err(adc->dev, x)

#define SARADC_STATE_IDLE 0
#define SARADC_STATE_BUSY 1
#define SARADC_STATE_SUSPEND 2

struct saradc {
	struct device *dev;
	void __iomem *mem_base;
	void __iomem *clk_mem_base;
	struct clk *clk;
	spinlock_t lock;
	int ref_val;
	int ref_nominal;
	int coef;
	int state;
};

static struct saradc *gp_saradc;

void setb(
		void __iomem *mem_base,
		unsigned int bits_desc,
		unsigned int bits_val)
{
	unsigned int mem_offset, val;
	unsigned int bits_offset, bits_mask;

	if (IS_ERR(mem_base))
		return;
	mem_offset = of_mem_offset(bits_desc);
	bits_offset = of_bits_offset(bits_desc);
	bits_mask = (1L<<of_bits_len(bits_desc))-1;
	val = readl(mem_base+mem_offset);
	val &= ~(bits_mask << bits_offset);
	val |= (bits_val & bits_mask) << bits_offset;
	writel(val, mem_base+mem_offset);
}
EXPORT_SYMBOL(setb);

unsigned int getb(
		void __iomem *mem_base,
		unsigned int bits_desc)
{
	unsigned int mem_offset, val;
	unsigned int bits_offset, bits_mask;

	if (IS_ERR(mem_base))
		return -1;
	mem_offset = of_mem_offset(bits_desc);
	bits_offset = of_bits_offset(bits_desc);
	bits_mask = (1L<<of_bits_len(bits_desc))-1;
	val = readl(mem_base+mem_offset);
	return (val >> bits_offset) & bits_mask;
}
EXPORT_SYMBOL(getb);

static void saradc_power_control(struct saradc *adc, int on)
{
	void __iomem *mem_base = adc->mem_base;

	if (on) {
		setb(mem_base, BANDGAP_EN, 1);
		setb(mem_base, ADC_EN, 1);
		udelay(5);
		setb(mem_base, CLK_EN, 1);
		setb(adc->clk_mem_base, REGC_CLK_EN, 1);
	} else {
		setb(adc->clk_mem_base, REGC_CLK_EN, 0);
		setb(mem_base, CLK_EN, 0);
		setb(mem_base, ADC_EN, 0);
		setb(mem_base, BANDGAP_EN, 0);
	}
}

static void saradc_reset(struct saradc *adc)
{
	void __iomem *mem_base = adc->mem_base;
	int clk_div;


	if (getb(mem_base, FLAG_INITIALIZED)) {
		saradc_info("initialized by BL30\n");
#ifndef ENABLE_DYNAMIC_POWER
		saradc_power_control(adc, 1);
#endif
		return;
	}
	writel(0x84004040, mem_base+SARADC_REG0);
	writel(0, mem_base+SARADC_CH_LIST);
	writel(0xaaaa, mem_base+SARADC_AVG_CNTL);
	if (flag_12bit)
		writel(0x9b88000a, mem_base+SARADC_REG3);
	else
		writel(0x9388000a, mem_base+SARADC_REG3);
	/* set SARADC_DELAY with 0x190a380a when 32k */
	writel(0x10a000a, mem_base+SARADC_DELAY);
	writel(0x3eb1a0c, mem_base+SARADC_AUX_SW);
	writel(0x3eb1a0c, mem_base+SARADC_AUX_SW);
	writel(0x8c000c, mem_base+SARADC_CH10_SW);
	writel(0xc000c, mem_base+SARADC_DETECT_IDLE_SW);

	clk_prepare_enable(adc->clk);
	clk_div = clk_get_rate(adc->clk) / 1200000;
	setb(mem_base, CLK_DIV, clk_div);
	setb(adc->clk_mem_base, REGC_CLK_DIV, clk_div);
	setb(adc->clk_mem_base, REGC_CLK_SRC, 0);
	saradc_info("initialized by kernel, clk_div=%d\n", clk_div);
#ifndef ENABLE_DYNAMIC_POWER
	saradc_power_control(adc, 1);
#endif
}

static int  saradc_internal_cal(struct saradc *adc)
{
	int val[5], nominal[5] = {0, 256, 512, 768, 1023};
	int i;

	saradc_info("calibration start:\n");
	adc->coef = 0;
	for (i = 0; i < 5; i++) {
		setb(adc->mem_base, CAL_CNTL, i);
		udelay(10);
		val[i] = get_adc_sample(0, CHAN_7);
		saradc_info("nominal=%d, value=%d\n", nominal[i], val[i]);
		if (val[i] < 0)
			goto cal_end;
	}
	adc->ref_val = val[2];
	adc->ref_nominal = nominal[2];
	if (val[3] > val[1]) {
		adc->coef = (nominal[3] - nominal[1]) << 12;
		adc->coef /= val[3] - val[1];
	}
cal_end:
	saradc_info("calibration end: coef=%d\n", adc->coef);
	setb(adc->mem_base, CAL_CNTL, 7);
	return 0;
}

static int saradc_get_cal_value(struct saradc *adc, int val)
{
	int nominal;

	/*((nominal - ref_nominal) << 10) / (val - ref_val) = coef*/
	/*==> nominal = ((val - ref_val) * coef >> 10) + ref_nominal*/

	nominal = val;
	if ((adc->coef > 0) && (val > 0)) {
		nominal = (val - adc->ref_val) * adc->coef;
		nominal >>= 12;
		nominal += adc->ref_nominal;
	}
	if (nominal < 0)
		nominal = 0;
	if (nominal > 1023)
		nominal = 1023;
	return nominal;
}

static int saradc_get_cal_value_12bit(struct saradc *adc, int val)
{
	int nominal;

	/*((nominal - ref_nominal) << 10) / (val - ref_val) = coef*/
	/*==> nominal = ((val - ref_val) * coef >> 10) + ref_nominal*/

	nominal = val;
	if ((adc->coef > 0) && (val > 0)) {
		nominal = (val - adc->ref_val) * adc->coef;
		nominal >>= 12;
		nominal += adc->ref_nominal;
	}
	if (nominal < 0)
		nominal = 0;
	if (nominal > 4095)
		nominal = 4095;
	return nominal;
}

/*if_10bit=1:10bit*/
/*if_10bit=0:12bit*/
int get_adc_sample_early(int dev_id, int ch, char if_10bit)
{
	struct saradc *adc;
	void __iomem *mem_base;
	int value, count, sum;
	int max = 0;
	int min = 0x3ff;
	int min_12bit = 0xfff;
	unsigned long flags;

	if (!if_10bit)
		min = min_12bit;

	adc = gp_saradc;
	mem_base = adc->mem_base;
	if (!adc || getb(mem_base, FLAG_BUSY_BL30)
		|| (adc->state != SARADC_STATE_IDLE))
		return -1;

	spin_lock_irqsave(&adc->lock, flags);
	adc->state = SARADC_STATE_BUSY;
	setb(mem_base, FLAG_BUSY_KERNEL, 1);
	isb();
	dsb(sy);
	udelay(1);
	if (getb(mem_base, FLAG_BUSY_BL30)) {
		value = -1;
		goto end;
	}

#ifdef ENABLE_DYNAMIC_POWER
	saradc_power_control(adc, 1);
#endif
#if CLEAN_BUFF_BEFORE_SARADC
	count = 0;
	while (getb(mem_base, FIFO_COUNT) && (count < FIFO_MAX)) {
		value = readl(mem_base+SARADC_FIFO_RD);
		count++;
	}
#endif
	writel(ch, mem_base+SARADC_CH_LIST);
	setb(mem_base, DETECT_MUX, ch);
	setb(mem_base, IDLE_MUX, ch);
	setb(mem_base, SAMPLE_ENGINE_EN, 1);
	setb(mem_base, START_SAMPLE, 1);

	count = 0;
	do {
		udelay(1);
		if (++count > 1000) {
			saradc_err("busy, %x\n", readl(mem_base+SARADC_REG0));
			value = -1;
			goto end1;
		}
	} while (getb(mem_base, ALL_BUSY));

	count = 0;
	sum = 0;
	while (getb(mem_base, FIFO_COUNT) && (count < FIFO_MAX)) {
		value = readl(mem_base+SARADC_FIFO_RD);
		if (((value>>12) & 0x07) == ch) {
			if (if_10bit) {
				if (flag_12bit) {
					value &= 0xffc;
					value >>= 2;
				} else
					value &= 0x3ff;
			} else
				value &= 0xfff;

			if (value > max)
				max = value;
			if (value < min)
				min = value;
			sum += value;
			count++;
		}
	}

	if (!count) {
		value = -1;
		goto end1;
	}
	if (count > 2) {
		sum -= (max + min);
		count -= 2;
	}
	value = sum / count;
	saradc_dbg("before cal: %d, count=%d\n", value, count);
	if (adc->coef) {
		if (if_10bit)
			value = saradc_get_cal_value(adc, value);
		else
			value = saradc_get_cal_value_12bit(adc, value);
		saradc_dbg("after cal: %d\n", value);
	}
end1:
	setb(mem_base, STOP_SAMPLE, 1);
	setb(mem_base, SAMPLE_ENGINE_EN, 0);
#ifdef ENABLE_DYNAMIC_POWER
	saradc_power_control(0);
#endif
end:
	setb(mem_base, FLAG_BUSY_KERNEL, 0);
	isb();
	dsb(sy);
	udelay(1);
	adc->state = SARADC_STATE_IDLE;
	spin_unlock_irqrestore(&adc->lock, flags);
	return value;
}


int get_adc_sample(int dev_id, int ch)
{
	int val;

	val = get_adc_sample_early(dev_id, ch, 1);
	return val;
}
EXPORT_SYMBOL(get_adc_sample);

int get_adc_sample_12bit(int dev_id, int ch)
{
	int val;

	val = get_adc_sample_early(dev_id, ch, 0);
	return val;
}
EXPORT_SYMBOL(get_adc_sample_12bit);

static void __iomem *
saradc_get_reg_addr(struct platform_device *pdev, int index)
{
	struct resource *res;
	void __iomem *reg_addr;

	res = platform_get_resource(pdev, IORESOURCE_MEM, index);
	if (!res) {
		dev_err(&pdev->dev, "reg: cannot obtain I/O memory region");
		return 0;
	}
	if (!devm_request_mem_region(&pdev->dev, res->start,
			resource_size(res), dev_name(&pdev->dev))) {
		dev_err(&pdev->dev, "Memory region busy\n");
		return 0;
	}
	reg_addr = devm_ioremap_nocache(&pdev->dev, res->start,
			resource_size(res));
	return reg_addr;
}

static ssize_t ch0_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", get_adc_sample(0, 0));
}
static ssize_t ch1_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", get_adc_sample(0, 1));
}
static ssize_t ch2_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", get_adc_sample(0, 2));
}
static ssize_t ch3_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", get_adc_sample(0, 3));
}
static ssize_t ch4_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", get_adc_sample(0, 4));
}
static ssize_t ch5_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", get_adc_sample(0, 5));
}
static ssize_t ch6_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", get_adc_sample(0, 6));
}
static ssize_t ch7_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", get_adc_sample(0, 7));
}

static struct class_attribute saradc_class_attrs[] = {
		__ATTR_RO(ch0),
		__ATTR_RO(ch1),
		__ATTR_RO(ch2),
		__ATTR_RO(ch3),
		__ATTR_RO(ch4),
		__ATTR_RO(ch5),
		__ATTR_RO(ch6),
		__ATTR_RO(ch7),
		__ATTR_NULL
};

static struct class saradc_class = {
		.name = "saradc",
		.class_attrs = saradc_class_attrs,
};

static int saradc_probe(struct platform_device *pdev)
{
	int err;
	struct saradc *adc;

	if (is_meson_gxbb_cpu() || is_meson_gxtvbb_cpu())
		flag_12bit = 0;
	else
		flag_12bit = 1;

	adc = kzalloc(sizeof(struct saradc), GFP_KERNEL);
	if (!adc) {
		err = -ENOMEM;
		goto end_err;
	}
	adc->dev = &pdev->dev;

	if (!pdev->dev.of_node) {
		err = -EINVAL;
		goto end_free;
	}
	adc->mem_base = saradc_get_reg_addr(pdev, 0);
	if (!adc->mem_base) {
		err = -ENODEV;
		goto end_free;
	}
	adc->clk_mem_base = saradc_get_reg_addr(pdev, 1);
	adc->clk = devm_clk_get(&pdev->dev, "saradc_clk");
	if (IS_ERR(adc->clk)) {
		err = -ENOENT;
		goto end_free;
	}

	saradc_reset(adc);
	gp_saradc = adc;
	dev_set_drvdata(&pdev->dev, adc);
	spin_lock_init(&adc->lock);
	adc->state = SARADC_STATE_IDLE;
	saradc_internal_cal(adc);
	return 0;


end_free:
	kfree(adc);
end_err:
	dev_err(&pdev->dev, "error=%d\n", err);
	return err;
}

static int saradc_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct saradc *adc = (struct saradc *)dev_get_drvdata(&pdev->dev);
	unsigned long flags;

	spin_lock_irqsave(&adc->lock, flags);
	saradc_power_control(adc, 0);
	adc->state = SARADC_STATE_SUSPEND;
	spin_unlock_irqrestore(&adc->lock, flags);
	return 0;
}

static int saradc_resume(struct platform_device *pdev)
{
	struct saradc *adc = (struct saradc *)dev_get_drvdata(&pdev->dev);
	unsigned long flags;

	spin_lock_irqsave(&adc->lock, flags);
	saradc_power_control(adc, 1);
	adc->state = SARADC_STATE_IDLE;
	spin_unlock_irqrestore(&adc->lock, flags);
	return 0;
}

static int saradc_remove(struct platform_device *pdev)
{
	struct saradc *adc = (struct saradc *)dev_get_drvdata(&pdev->dev);
	unsigned long flags;

	spin_lock_irqsave(&adc->lock, flags);
	saradc_power_control(adc, 0);
	spin_unlock_irqrestore(&adc->lock, flags);
	kfree(adc);
	return 0;
}

static void saradc_shutdown(struct platform_device *pdev)
{
	struct saradc *adc = (struct saradc *)dev_get_drvdata(&pdev->dev);
	unsigned long flags;

	spin_lock_irqsave(&adc->lock, flags);
	saradc_power_control(adc, 0);
	spin_unlock_irqrestore(&adc->lock, flags);
}

#ifdef CONFIG_OF
static const struct of_device_id saradc_dt_match[] = {
	{ .compatible = "amlogic, saradc"},
	{},
};
#else
#define saradc_dt_match NULL
#endif

static struct platform_driver saradc_driver = {
	.probe      = saradc_probe,
	.remove     = saradc_remove,
	.suspend    = saradc_suspend,
	.resume     = saradc_resume,
	.shutdown = saradc_shutdown,
	.driver     = {
		.name   = "saradc",
		.of_match_table = saradc_dt_match,
	},
};

static int __init saradc_init(void)
{
	/* printk(KERN_INFO "SARADC Driver init.\n"); */
	class_register(&saradc_class);
	return platform_driver_register(&saradc_driver);
}

static void __exit saradc_exit(void)
{
	/* printk(KERN_INFO "SARADC Driver exit.\n"); */
	platform_driver_unregister(&saradc_driver);
	class_unregister(&saradc_class);
}

module_init(saradc_init);
module_exit(saradc_exit);

MODULE_AUTHOR("aml");
MODULE_DESCRIPTION("SARADC Driver");
MODULE_LICENSE("GPL");
