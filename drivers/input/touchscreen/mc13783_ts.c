/*
 * Driver for the Freescale Semiconductor MC13783 touchscreen.
 *
 * Copyright 2004-2007 Freescale Semiconductor, Inc. All Rights Reserved.
 * Copyright (C) 2009 Sascha Hauer, Pengutronix
 *
 * Initial development of this code was funded by
 * Phytec Messtechnik GmbH, http://www.phytec.de/
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */
#include <linux/platform_device.h>
#include <linux/mfd/mc13783.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/init.h>

#define DEFAULT_SAMPLE_TOLERANCE 300

static unsigned int sample_tolerance = DEFAULT_SAMPLE_TOLERANCE;
module_param(sample_tolerance, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(sample_tolerance,
		"If the minimal and maximal value read out for one axis (out "
		"of three) differ by this value (default: "
		__stringify(DEFAULT_SAMPLE_TOLERANCE) ") or more, the reading "
		"is supposed to be wrong and is discarded.  Set to 0 to "
		"disable this check.");

struct mc13783_ts_priv {
	struct input_dev *idev;
	struct mc13xxx *mc13xxx;
	struct delayed_work work;
	unsigned int sample[4];
	const char* name;
	u8 mc13892;
	/*
	 * Delay between Touchscreen polarization and ADC Conversion.
	 * Given in clock ticks of a 32 kHz clock which gives a granularity of
	 * about 30.5ms
	 */
	u8 ato;
	/* Use the ATO delay only for the first conversion or for each one */
	bool atox;
};

static irqreturn_t mc13783_ts_handler(int irq, void *data)
{
	struct mc13783_ts_priv *priv = data;

	mc13xxx_irq_ack(priv->mc13xxx, irq);

	/*
	 * Kick off reading coordinates. Note that if work happens already
	 * be queued for future execution (it rearms itself) it will not
	 * be rescheduled for immediate execution here. However the rearm
	 * delay is HZ / 50 which is acceptable.
	 */
	schedule_delayed_work(&priv->work, 0);

	return IRQ_HANDLED;
}

#define sort3(a0, a1, a2) ({						\
		if (a0 > a1)						\
			swap(a0, a1);					\
		if (a1 > a2)						\
			swap(a1, a2);					\
		if (a0 > a1)						\
			swap(a0, a1);					\
		})

static void mc13783_ts_report_sample(struct mc13783_ts_priv *priv)
{
	struct input_dev *idev = priv->idev;
	int cr0, cr1;

	/*
	 * the values are 10-bit wide only, but the two least significant
	 * bits are for future 12 bit use and reading yields 0
	 */
	cr0 = ~(priv->sample[2] >> 12) & 0xfff;
	cr1 = ~(priv->sample[3] >> 12) & 0xfff;

	cr0 = (cr0 + cr1) >> 1;
	if (cr0 < 0x800)
		cr0 = 0;

	if (cr0) {
		/* report the median coordinate and average pressure */
		int x0, x1, x2, y0, y1, y2;

		x0 = priv->sample[0] & 0xfff;
		x1 = priv->sample[1] & 0xfff;
		x2 = priv->sample[2] & 0xfff;
		y0 = priv->sample[3] & 0xfff;
		y1 = (priv->sample[0] >> 12) & 0xfff;
		y2 = (priv->sample[1] >> 12) & 0xfff;

		dev_dbg(&idev->dev,
			"x: (% 4d,% 4d,% 4d) y: (% 4d, % 4d,% 4d) cr: (% 4d, % 4d)\n",
			x0, x1, x2, y0, y1, y2, cr0, cr1);

		if (priv->mc13892) {
			x2 = x1;
			if (x0 > x1) {
				x2 = x0;
				x0 = x1;
			}
			y2 = y1;
			if (y0 > y1) {
				y2 = y0;
				y0 = y1;
			}
			x1 = (x0 + x2) >> 1;
			y1 = (y0 + y2) >> 1;
		} else {
			sort3(x0, x1, x2);
			sort3(y0, y1, y2);
		}
		if (sample_tolerance && (x2 - x0 >= sample_tolerance ||
					 y2 - y0 >= sample_tolerance)) {
			dev_dbg(&idev->dev, "discard event\n");
			goto exit1;
		}
		input_report_abs(idev, ABS_X, x1);
		input_report_abs(idev, ABS_Y, y1);

		dev_dbg(&idev->dev, "report (%d, %d, %d)\n", x1, y1, cr0);
	} else {
		dev_dbg(&idev->dev, "report release\n");
	}

	input_report_abs(idev, ABS_PRESSURE, cr0);
	input_report_key(idev, BTN_TOUCH, cr0);
	input_sync(idev);
	if (!cr0)
		return;
exit1:
	schedule_delayed_work(&priv->work, HZ / 50);
}

static void mc13783_ts_work(struct work_struct *work)
{
	struct mc13783_ts_priv *priv =
		container_of(work, struct mc13783_ts_priv, work.work);
	unsigned int mode = MC13XXX_ADC_MODE_TS;
	unsigned int channel = 12;

	if (mc13xxx_adc_do_conversion(priv->mc13xxx,
				mode, channel,
				priv->ato, priv->atox,
				priv->sample) == 0) {
		mc13783_ts_report_sample(priv);
	}
}

static int mc13783_ts_open(struct input_dev *dev)
{
	struct mc13783_ts_priv *priv = input_get_drvdata(dev);
	int ret;

	mc13xxx_lock(priv->mc13xxx);

	mc13xxx_irq_ack(priv->mc13xxx, MC13XXX_IRQ_TS);

	ret = mc13xxx_irq_request(priv->mc13xxx, MC13XXX_IRQ_TS,
		mc13783_ts_handler, dev->name, priv);
	if (ret)
		goto out;

	ret = mc13xxx_reg_rmw(priv->mc13xxx, MC13XXX_ADC0,
			MC13XXX_ADC0_TSMOD_MASK, MC13XXX_ADC0_TSMOD0);
	if (ret)
		mc13xxx_irq_free(priv->mc13xxx, MC13XXX_IRQ_TS, priv);
out:
	mc13xxx_unlock(priv->mc13xxx);
	return ret;
}

static void mc13783_ts_close(struct input_dev *dev)
{
	struct mc13783_ts_priv *priv = input_get_drvdata(dev);

	mc13xxx_lock(priv->mc13xxx);
	mc13xxx_reg_rmw(priv->mc13xxx, MC13XXX_ADC0,
			MC13XXX_ADC0_TSMOD_MASK, 0);
	mc13xxx_irq_free(priv->mc13xxx, MC13XXX_IRQ_TS, priv);
	mc13xxx_unlock(priv->mc13xxx);

	cancel_delayed_work_sync(&priv->work);
}

static int __init mc13783_ts_probe(struct platform_device *pdev)
{
	struct mc13783_ts_priv *priv;
	struct input_dev *idev;
	int ret = -ENOMEM;
	struct mc13xxx_ts_platform_data *touch;

	pr_info("%s:\n", __func__);
	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	idev = input_allocate_device();
	if (!priv || !idev)
		goto err_free_mem;

	INIT_DELAYED_WORK(&priv->work, mc13783_ts_work);
	priv->mc13xxx = dev_get_drvdata(pdev->dev.parent);
	priv->idev = idev;
	touch = dev_get_platdata(&pdev->dev);
	if (touch) {
		priv->ato = touch->ato;
		priv->atox = touch->atox;
	} else {
		dev_dbg(&pdev->dev, "missing platform data\n");
	}

	priv->mc13892 = platform_get_device_id(pdev)->driver_data;

	idev->name = platform_get_device_id(pdev)->name;
	idev->dev.parent = &pdev->dev;

	idev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	idev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
	input_set_abs_params(idev, ABS_X, 0, 0xfff, 0, 0);
	input_set_abs_params(idev, ABS_Y, 0, 0xfff, 0, 0);
	input_set_abs_params(idev, ABS_PRESSURE, 0, 0xfff, 0, 0);

	idev->open = mc13783_ts_open;
	idev->close = mc13783_ts_close;

	input_set_drvdata(idev, priv);

	ret = input_register_device(priv->idev);
	if (ret) {
		dev_err(&pdev->dev,
			"register input device failed with %d\n", ret);
		goto err_free_mem;
	}

	platform_set_drvdata(pdev, priv);
	return 0;

err_free_mem:
	input_free_device(idev);
	kfree(priv);
	return ret;
}

static int mc13783_ts_remove(struct platform_device *pdev)
{
	struct mc13783_ts_priv *priv = platform_get_drvdata(pdev);

	input_unregister_device(priv->idev);
	kfree(priv);

	return 0;
}

static const struct platform_device_id mc13xxx_ids[] = {
        { "mc13783-ts", 0 },
        { "mc13892-ts", 1 },
        { }
};
MODULE_DEVICE_TABLE(platform, mc13xxx_ids);

static struct platform_driver mc13783_ts_driver = {
	.remove		= mc13783_ts_remove,
	.driver		= {
		.name	= "mc13783-ts",
	},
	.id_table = mc13xxx_ids,
};

module_platform_driver_probe(mc13783_ts_driver, mc13783_ts_probe);

MODULE_DESCRIPTION("MC13783 input touchscreen driver");
MODULE_AUTHOR("Sascha Hauer <s.hauer@pengutronix.de>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform: mc13783-ts");
MODULE_ALIAS("platform: mc13892-ts");
