/*
 * Freescale i.MX6UL touchscreen controller driver
 *
 * Copyright (C) 2015 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/io.h>

/* ADC configuration registers field define */
#define ADC_AIEN		(0x1 << 7)
#define ADC_CONV_DISABLE	0x1F
#define ADC_CAL			(0x1 << 7)
#define ADC_CALF		0x2
#define ADC_12BIT_MODE		(0x2 << 2)
#define ADC_IPG_CLK		0x00
#define ADC_CLK_DIV_8		(0x03 << 5)
#define ADC_SHORT_SAMPLE_MODE	(0x0 << 4)
#define ADC_HARDWARE_TRIGGER	(0x1 << 13)
#define SELECT_CHANNEL_4	0x04
#define SELECT_CHANNEL_1	0x01
#define DISABLE_CONVERSION_INT	(0x0 << 7)

/* ADC registers */
#define REG_ADC_HC0		0x00
#define REG_ADC_HC1		0x04
#define REG_ADC_HC2		0x08
#define REG_ADC_HC3		0x0C
#define REG_ADC_HC4		0x10
#define REG_ADC_HS		0x14
#define REG_ADC_R0		0x18
#define REG_ADC_CFG		0x2C
#define REG_ADC_GC		0x30
#define REG_ADC_GS		0x34

#define ADC_TIMEOUT		msecs_to_jiffies(100)

/* TSC registers */
#define REG_TSC_BASIC_SETING	0x00
#define REG_TSC_PRE_CHARGE_TIME	0x10
#define REG_TSC_FLOW_CONTROL	0x20
#define REG_TSC_MEASURE_VALUE	0x30
#define REG_TSC_INT_EN		0x40
#define REG_TSC_INT_SIG_EN	0x50
#define REG_TSC_INT_STATUS	0x60
#define REG_TSC_DEBUG_MODE	0x70
#define REG_TSC_DEBUG_MODE2	0x80

/* TSC configuration registers field define */
#define DETECT_4_WIRE_MODE	(0x0 << 4)
#define AUTO_MEASURE		0x1
#define MEASURE_SIGNAL		0x1
#define DETECT_SIGNAL		(0x1 << 4)
#define VALID_SIGNAL		(0x1 << 8)
#define MEASURE_INT_EN		0x1
#define MEASURE_SIG_EN		0x1
#define VALID_SIG_EN		(0x1 << 8)
#define DE_GLITCH_2		(0x2 << 29)
#define START_SENSE		(0x1 << 12)
#define TSC_DISABLE		(0x1 << 16)
#define DETECT_MODE		0x2

struct imx6ul_tsc {
	struct device *dev;
	struct input_dev *input;
	void __iomem *tsc_regs;
	void __iomem *adc_regs;
	struct clk *tsc_clk;
	struct clk *adc_clk;

	int tsc_irq;
	int adc_irq;
	int value;
	int xnur_gpio;
	int measure_delay_time;
	int pre_charge_time;

	struct completion completion;
};

/*
 * TSC module need ADC to get the measure value. So
 * before config TSC, we should initialize ADC module.
 */
static void imx6ul_adc_init(struct imx6ul_tsc *tsc)
{
	int adc_hc = 0;
	int adc_gc;
	int adc_gs;
	int adc_cfg;
	int timeout;

	adc_cfg = readl(tsc->adc_regs + REG_ADC_CFG);
	adc_cfg |= ADC_12BIT_MODE | ADC_IPG_CLK;
	adc_cfg |= ADC_CLK_DIV_8 | ADC_SHORT_SAMPLE_MODE;
	adc_cfg &= ~ADC_HARDWARE_TRIGGER;
	writel(adc_cfg, tsc->adc_regs + REG_ADC_CFG);

	/* enable calibration interrupt */
	adc_hc |= ADC_AIEN;
	adc_hc |= ADC_CONV_DISABLE;
	writel(adc_hc, tsc->adc_regs + REG_ADC_HC0);

	/* start ADC calibration */
	adc_gc = readl(tsc->adc_regs + REG_ADC_GC);
	adc_gc |= ADC_CAL;
	writel(adc_gc, tsc->adc_regs + REG_ADC_GC);

	timeout = wait_for_completion_timeout
			(&tsc->completion, ADC_TIMEOUT);
	if (timeout == 0)
		dev_err(tsc->dev, "Timeout for adc calibration\n");

	adc_gs = readl(tsc->adc_regs + REG_ADC_GS);
	if (adc_gs & ADC_CALF)
		dev_err(tsc->dev, "ADC calibration failed\n");

	/* TSC need the ADC work in hardware trigger */
	adc_cfg = readl(tsc->adc_regs + REG_ADC_CFG);
	adc_cfg |= ADC_HARDWARE_TRIGGER;
	writel(adc_cfg, tsc->adc_regs + REG_ADC_CFG);
}

/*
 * This is a TSC workaround. Currently TSC misconnect two
 * ADC channels, this function remap channel configure for
 * hardware trigger.
 */
static void imx6ul_tsc_channel_config(struct imx6ul_tsc *tsc)
{
	int adc_hc0, adc_hc1, adc_hc2, adc_hc3, adc_hc4;

	adc_hc0 = DISABLE_CONVERSION_INT;
	writel(adc_hc0, tsc->adc_regs + REG_ADC_HC0);

	adc_hc1 = DISABLE_CONVERSION_INT | SELECT_CHANNEL_4;
	writel(adc_hc1, tsc->adc_regs + REG_ADC_HC1);

	adc_hc2 = DISABLE_CONVERSION_INT;
	writel(adc_hc2, tsc->adc_regs + REG_ADC_HC2);

	adc_hc3 = DISABLE_CONVERSION_INT | SELECT_CHANNEL_1;
	writel(adc_hc3, tsc->adc_regs + REG_ADC_HC3);

	adc_hc4 = DISABLE_CONVERSION_INT;
	writel(adc_hc4, tsc->adc_regs + REG_ADC_HC4);
}

/*
 * TSC setting, confige the pre-charge time and measure delay time.
 * different touch screen may need different pre-charge time and
 * measure delay time.
 */
static void imx6ul_tsc_set(struct imx6ul_tsc *tsc)
{
	int basic_setting = 0;
	int start;

	basic_setting |= tsc->measure_delay_time << 8;
	basic_setting |= DETECT_4_WIRE_MODE | AUTO_MEASURE;
	writel(basic_setting, tsc->tsc_regs + REG_TSC_BASIC_SETING);

	writel(DE_GLITCH_2, tsc->tsc_regs + REG_TSC_DEBUG_MODE2);

	writel(tsc->pre_charge_time, tsc->tsc_regs + REG_TSC_PRE_CHARGE_TIME);
	writel(MEASURE_INT_EN, tsc->tsc_regs + REG_TSC_INT_EN);
	writel(MEASURE_SIG_EN | VALID_SIG_EN,
		tsc->tsc_regs + REG_TSC_INT_SIG_EN);

	/* start sense detection */
	start = readl(tsc->tsc_regs + REG_TSC_FLOW_CONTROL);
	start |= START_SENSE;
	start &= ~TSC_DISABLE;
	writel(start, tsc->tsc_regs + REG_TSC_FLOW_CONTROL);
}

static void imx6ul_tsc_init(struct imx6ul_tsc *tsc)
{
	imx6ul_adc_init(tsc);
	imx6ul_tsc_channel_config(tsc);
	imx6ul_tsc_set(tsc);
}

static irqreturn_t tsc_irq(int irq, void *dev_id)
{
	struct imx6ul_tsc *tsc = (struct imx6ul_tsc *)dev_id;
	int status;
	int x, y;
	int xnur;
	int debug_mode2;
	int state_machine;
	int start;
	unsigned long timeout;

	status = readl(tsc->tsc_regs + REG_TSC_INT_STATUS);

	/* write 1 to clear the bit measure-signal */
	writel(MEASURE_SIGNAL | DETECT_SIGNAL,
		tsc->tsc_regs + REG_TSC_INT_STATUS);

	/* It's a HW self-clean bit. Set this bit and start sense detection */
	start = readl(tsc->tsc_regs + REG_TSC_FLOW_CONTROL);
	start |= START_SENSE;
	writel(start, tsc->tsc_regs + REG_TSC_FLOW_CONTROL);

	if (status & MEASURE_SIGNAL) {
		tsc->value = readl(tsc->tsc_regs + REG_TSC_MEASURE_VALUE);
		x = (tsc->value >> 16) & 0x0fff;
		y = tsc->value & 0x0fff;

		/*
		 * Delay some time(max 2ms), wait the pre-charge done.
		 * After the pre-change mode, TSC go into detect mode.
		 * And in detect mode, we can get the xnur gpio value.
		 * If xnur is low, this means the touch screen still
		 * be touched. If xnur is high, this means finger leave
		 * the touch screen.
		 */
		timeout = jiffies + HZ/500;
		do {
			if (time_after(jiffies, timeout)) {
				xnur = 0;
				goto touch_event;
			}
			usleep_range(200, 400);
			debug_mode2 = readl(tsc->tsc_regs + REG_TSC_DEBUG_MODE2);
			state_machine = (debug_mode2 >> 20) & 0x7;
		} while (state_machine != DETECT_MODE);
		usleep_range(200, 400);

		xnur = gpio_get_value(tsc->xnur_gpio);
touch_event:
		if (xnur == 0) {
			input_report_key(tsc->input, BTN_TOUCH, 1);
			input_report_abs(tsc->input, ABS_X, x);
			input_report_abs(tsc->input, ABS_Y, y);
		} else
			input_report_key(tsc->input, BTN_TOUCH, 0);

		input_sync(tsc->input);
	}

	return IRQ_HANDLED;
}

static irqreturn_t adc_irq(int irq, void *dev_id)
{
	struct imx6ul_tsc *tsc = (struct imx6ul_tsc *)dev_id;
	int coco;
	int value;

	coco = readl(tsc->adc_regs + REG_ADC_HS);
	if (coco & 0x01) {
		value = readl(tsc->adc_regs + REG_ADC_R0);
		complete(&tsc->completion);
	}

	return IRQ_HANDLED;
}

static const struct of_device_id imx6ul_tsc_match[] = {
	{ .compatible = "fsl,imx6ul-tsc", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, imx6ul_tsc_match);

static int imx6ul_tsc_probe(struct platform_device *pdev)
{
	struct imx6ul_tsc *tsc;
	struct resource *tsc_mem;
	struct resource *adc_mem;
	struct input_dev *input_dev;
	struct device_node *np = pdev->dev.of_node;
	int err;

	tsc = kzalloc(sizeof(struct imx6ul_tsc), GFP_KERNEL);
	if (!tsc) {
		err = -ENOMEM;
		goto err_free_mem_tsc;
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
		err = -ENOMEM;
		goto err_free_mem;
	}

	tsc->dev = &pdev->dev;

	tsc->input = input_dev;
	tsc->input->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	tsc->input->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
	input_set_abs_params(tsc->input, ABS_X, 0, 0xFFF, 0, 0);
	input_set_abs_params(tsc->input, ABS_Y, 0, 0xFFF, 0, 0);

	tsc->input->name = "iMX6UL TouchScreen Controller";

	tsc->xnur_gpio = of_get_named_gpio(np, "xnur-gpio", 0);
	err = gpio_request_one(tsc->xnur_gpio, GPIOF_IN, "tsc_X-");
	if (err) {
		dev_err(&pdev->dev, "failed to request GPIO tsc_X-\n");
		goto err_free_mem;
	}

	tsc_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	tsc->tsc_regs = devm_ioremap_resource(&pdev->dev, tsc_mem);
	if (IS_ERR(tsc->tsc_regs)) {
		dev_err(&pdev->dev, "failed to remap tsc memory\n");
		err = PTR_ERR(tsc->tsc_regs);
		goto err_free_mem;
	}

	adc_mem = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	tsc->adc_regs = devm_ioremap_resource(&pdev->dev, adc_mem);
	if (IS_ERR(tsc->adc_regs)) {
		dev_err(&pdev->dev, "failed to remap adc memory\n");
		err = PTR_ERR(tsc->adc_regs);
		goto err_free_mem;
	}

	tsc->tsc_clk = devm_clk_get(&pdev->dev, "tsc");
	if (IS_ERR(tsc->tsc_clk)) {
		dev_err(&pdev->dev, "failed getting tsc clock\n");
		err = PTR_ERR(tsc->tsc_clk);
		goto err_free_mem;
	}

	tsc->adc_clk = devm_clk_get(&pdev->dev, "adc");
	if (IS_ERR(tsc->adc_clk)) {
		dev_err(&pdev->dev, "failed getting adc clock\n");
		err = PTR_ERR(tsc->adc_clk);
		goto err_free_mem;
	}

	tsc->tsc_irq = platform_get_irq(pdev, 0);
	if (tsc->tsc_irq <= 0) {
		dev_err(&pdev->dev, "no tsc irq resource?\n");
		err = -EINVAL;
		goto err_free_mem;
	}

	tsc->adc_irq = platform_get_irq(pdev, 1);
	if (tsc->adc_irq <= 0) {
		dev_err(&pdev->dev, "no adc irq resource?\n");
		err = -EINVAL;
		goto err_free_mem;
	}

	err = devm_request_threaded_irq(tsc->dev, tsc->tsc_irq,
					NULL, tsc_irq,
					IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
					dev_name(&pdev->dev), tsc);
	if (err < 0) {
		dev_err(&pdev->dev,
			"failed requesting tsc irq %d\n",
					    tsc->tsc_irq);
		goto err_free_mem;
	}

	err = devm_request_irq(tsc->dev, tsc->adc_irq,
				adc_irq, 0,
				dev_name(&pdev->dev), tsc);
	if (err < 0) {
		dev_err(&pdev->dev,
			"failed requesting adc irq %d\n",
					    tsc->adc_irq);
		goto err_free_mem;
	}

	err = of_property_read_u32(np, "measure_delay_time",
				&tsc->measure_delay_time);
	if (err)
		tsc->measure_delay_time = 0xffff;

	err = of_property_read_u32(np, "pre_charge_time",
				&tsc->pre_charge_time);
	if (err)
		tsc->pre_charge_time = 0xfff;

	init_completion(&tsc->completion);

	err = clk_prepare_enable(tsc->adc_clk);
	if (err) {
		dev_err(&pdev->dev,
			"Could not prepare or enable the adc clock.\n");
		goto err_free_mem;
	}

	err = clk_prepare_enable(tsc->tsc_clk);
	if (err) {
		dev_err(&pdev->dev,
			"Could not prepare or enable the tsc clock.\n");
		goto error_tsc_clk_enable;
	}

	err = input_register_device(tsc->input);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to register input device\n");
		err = -EIO;
		goto err_input_register;
	}

	imx6ul_tsc_init(tsc);

	platform_set_drvdata(pdev, tsc);
	return 0;

err_input_register:
	clk_disable_unprepare(tsc->tsc_clk);
error_tsc_clk_enable:
	clk_disable_unprepare(tsc->adc_clk);
err_free_mem:
	input_free_device(tsc->input);
err_free_mem_tsc:
	kfree(tsc);
	return err;
}

static int imx6ul_tsc_remove(struct platform_device *pdev)
{
	struct imx6ul_tsc *tsc = platform_get_drvdata(pdev);

	clk_disable_unprepare(tsc->tsc_clk);
	clk_disable_unprepare(tsc->adc_clk);
	input_unregister_device(tsc->input);
	kfree(tsc);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int imx6ul_tsc_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct imx6ul_tsc *tsc = platform_get_drvdata(pdev);
	int tsc_flow;
	int adc_cfg;

	/* TSC controller enters to idle status */
	tsc_flow = readl(tsc->tsc_regs + REG_TSC_FLOW_CONTROL);
	tsc_flow |= TSC_DISABLE;
	writel(tsc_flow, tsc->tsc_regs + REG_TSC_FLOW_CONTROL);

	/* ADC controller enters to stop mode */
	adc_cfg = readl(tsc->adc_regs + REG_ADC_HC0);
	adc_cfg |= ADC_CONV_DISABLE;
	writel(adc_cfg, tsc->adc_regs + REG_ADC_HC0);

	clk_disable_unprepare(tsc->tsc_clk);
	clk_disable_unprepare(tsc->adc_clk);

	return 0;
}

static int imx6ul_tsc_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct imx6ul_tsc *tsc = platform_get_drvdata(pdev);
	int err;

	err = clk_prepare_enable(tsc->adc_clk);
	if (err)
		return err;

	err = clk_prepare_enable(tsc->tsc_clk);
	if (err)
		return err;

	imx6ul_tsc_init(tsc);
	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(imx6ul_tsc_pm_ops,
			 imx6ul_tsc_suspend,
			 imx6ul_tsc_resume);

static struct platform_driver imx6ul_tsc_driver = {
	.driver		= {
		.name	= "imx6ul-tsc",
		.owner	= THIS_MODULE,
		.of_match_table	= imx6ul_tsc_match,
		.pm	= &imx6ul_tsc_pm_ops,
	},
	.probe		= imx6ul_tsc_probe,
	.remove		= imx6ul_tsc_remove,
};
module_platform_driver(imx6ul_tsc_driver);

MODULE_AUTHOR("Haibo Chen <haibo.chen@freescale.com>");
MODULE_DESCRIPTION("Freescale i.MX6UL Touchscreen controller driver");
MODULE_LICENSE("GPL v2");
