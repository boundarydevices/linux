/*
 * sound/soc/amlogic/auge/locker.c
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
#define DEBUG


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "locker_hw.h"

#define DRV_NAME "audiolocker"

struct audiolocker {
	struct device *dev;
	struct clk *lock_out;
	struct clk *lock_in;
	/* locker src (parent of locker in/out)*/
	struct clk *out_src;
	struct clk *in_src;
	/* pll (parent of locker src) */
	struct clk *out_calc;
	struct clk *in_ref;

	int irq;
	int expected_freq;
	int dividor;
	bool enable;
};

/*#define AUDIOLOCKER_TEST*/

struct audiolocker *s_locker;

static int audiolocker_pll_config(struct audiolocker *p_audiolocker)
{
#ifdef AUDIOLOCKER_TEST
	clk_set_rate(p_audiolocker->out_calc, 49000000);
	clk_set_rate(p_audiolocker->in_ref, 49006000);

	/* mpll1 --> mclk_d */
	audiobus_write(EE_AUDIO_MCLK_D_CTRL,
		1 << 31 | 1 << 24 | (49 - 1) << 0);

	/* mpll2 --> mclk_e */
	audiobus_write(EE_AUDIO_MCLK_E_CTRL,
		1 << 31 | 2 << 24 | (49 - 1) << 0);

	/* lockin select mclk_d, lockout select mclk_e */
	audiobus_write(EE_AUDIO_CLK_LOCKER_CTRL,
		1 << 31 | /* lockout enable */
		4 << 24 | /*lock_out_clk, 3:mst_d_mclk, 27~24*/
		0 << 16 | /*clk_div, 23~16*/
		1 << 15 | /* locker in enable */
		3 << 8  | /*lock_in_clk, 4:mst_e_mclk, 11~8*/
		0 << 0    /*clk_div, 7~0*/
		);
#else
	int ret;

	clk_set_rate(p_audiolocker->out_calc,
		p_audiolocker->expected_freq);
	clk_set_rate(p_audiolocker->in_ref,
		p_audiolocker->expected_freq);

	clk_set_rate(p_audiolocker->out_src,
		p_audiolocker->expected_freq / p_audiolocker->dividor);
	clk_set_rate(p_audiolocker->in_src,
		p_audiolocker->expected_freq / p_audiolocker->dividor);

	ret = clk_prepare_enable(p_audiolocker->in_ref);
	if (ret) {
		pr_err("Can't enable pll_ref clock: %d\n", ret);
		return -EINVAL;
	}
	ret = clk_prepare_enable(p_audiolocker->out_calc);
	if (ret) {
		pr_err("Can't enable pll_calc clock: %d\n", ret);
		return -EINVAL;
	}

	ret = clk_prepare_enable(p_audiolocker->in_src);
	if (ret) {
		pr_err("Can't enable in_src clock: %d\n", ret);
		return -EINVAL;
	}
	ret = clk_prepare_enable(p_audiolocker->out_src);
	if (ret) {
		pr_err("Can't enable out_src clock: %d\n", ret);
		return -EINVAL;
	}

	ret = clk_prepare_enable(p_audiolocker->lock_in);
	if (ret) {
		pr_err("Can't enable lock_in clock: %d\n", ret);
		return -EINVAL;
	}
	ret = clk_prepare_enable(p_audiolocker->lock_out);
	if (ret) {
		pr_err("Can't enable lock_out clock: %d\n", ret);
		return -EINVAL;
	}
#endif

	return 0;
}

static void audiolocker_init(struct audiolocker *p_audiolocker)
{
	if (p_audiolocker->enable) {
		/* audio pll */
		audiolocker_pll_config(p_audiolocker);

		/* audiolocker irq*/
		audiolocker_irq_config();
	} else
		audiolocker_disable();
}

static irqreturn_t locker_isr_handler(int irq, void *data)
{
	struct audiolocker *p_audiolocker = (struct audiolocker *)data;

	audiolocker_update_clks(
		p_audiolocker->out_calc,
		p_audiolocker->in_ref);

	return IRQ_HANDLED;
}

static ssize_t locker_enable_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct audiolocker *p_audiolocker = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", p_audiolocker->enable);
}

static ssize_t locker_enable_set(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct audiolocker *p_audiolocker = dev_get_drvdata(dev);
	int target, ret;

	ret = kstrtoint(buf, 10, &target);
	if (ret) {
		pr_info("%s: invalid data\n", __func__);
		return -EINVAL;
	}

	if (target)
		p_audiolocker->enable = true;
	else
		p_audiolocker->enable = false;

	audiolocker_init(p_audiolocker);

	return count;
}

static DEVICE_ATTR(locker_enable, 0644,
	locker_enable_show,
	locker_enable_set);

void audio_locker_set(int enable)
{
	if (!s_locker) {
		pr_debug("audio locker is not init\n");
		return;
	}

	s_locker->enable = enable;
	audiolocker_init(s_locker);
}

int audio_locker_get(void)
{
	if (!s_locker) {
		pr_debug("audio locker is not init\n");
		return -1;
	}

	return s_locker->enable;
}

static int audiolocker_platform_probe(struct platform_device *pdev)
{
	struct audiolocker *p_audiolocker;
	int ret;

	pr_info("%s\n", __func__);

	p_audiolocker = devm_kzalloc(&pdev->dev,
			sizeof(struct audiolocker),
			GFP_KERNEL);
	if (!p_audiolocker) {
		/*dev_err(&pdev->dev, "Can't allocate for audiolocker\n");*/
		return -ENOMEM;
	}

	p_audiolocker->lock_in = devm_clk_get(&pdev->dev, "lock_in");
	if (IS_ERR(p_audiolocker->lock_in)) {
		dev_err(&pdev->dev,
			"Can't retrieve lock_in clock\n");
		ret = PTR_ERR(p_audiolocker->lock_in);
		return ret;
	}
	p_audiolocker->lock_out = devm_clk_get(&pdev->dev, "lock_out");
	if (IS_ERR(p_audiolocker->lock_out)) {
		dev_err(&pdev->dev,
			"Can't retrieve lock_out clock\n");
		ret = PTR_ERR(p_audiolocker->lock_out);
		return ret;
	}
	p_audiolocker->in_src = devm_clk_get(&pdev->dev, "in_src");
	if (IS_ERR(p_audiolocker->in_src)) {
		dev_err(&pdev->dev,
			"Can't retrieve in_src clock\n");
		ret = PTR_ERR(p_audiolocker->in_src);
		return ret;
	}
	p_audiolocker->out_src = devm_clk_get(&pdev->dev, "out_src");
	if (IS_ERR(p_audiolocker->out_src)) {
		dev_err(&pdev->dev,
			"Can't retrieve out_src clock\n");
		ret = PTR_ERR(p_audiolocker->out_src);
		return ret;
	}
	p_audiolocker->in_ref = devm_clk_get(&pdev->dev, "in_ref");
	if (IS_ERR(p_audiolocker->in_ref)) {
		dev_err(&pdev->dev,
			"Can't retrieve in_ref clock\n");
		ret = PTR_ERR(p_audiolocker->in_ref);
		return ret;
	}
	p_audiolocker->out_calc = devm_clk_get(&pdev->dev, "out_calc");
	if (IS_ERR(p_audiolocker->out_calc)) {
		dev_err(&pdev->dev,
			"Can't retrieve out_calc clock\n");
		ret = PTR_ERR(p_audiolocker->out_calc);
		return ret;
	}

	ret = clk_set_parent(p_audiolocker->lock_in,
		p_audiolocker->in_src);
	if (ret) {
		dev_err(&pdev->dev,
			"Can't set lock_in parent clock\n");
		ret = PTR_ERR(p_audiolocker->lock_in);
		return ret;
	}

	ret = clk_set_parent(p_audiolocker->lock_out,
		p_audiolocker->out_src);
	if (ret) {
		dev_err(&pdev->dev,
			"Can't set lock_out parent clock\n");
		ret = PTR_ERR(p_audiolocker->lock_out);
		return ret;
	}

	ret = clk_set_parent(p_audiolocker->in_src,
		p_audiolocker->in_ref);
	if (ret) {
		dev_err(&pdev->dev,
			"Can't set in_src parent clock\n");
		ret = PTR_ERR(p_audiolocker->in_src);
		return ret;
	}

	ret = clk_set_parent(p_audiolocker->out_src,
		p_audiolocker->out_calc);
	if (ret) {
		dev_err(&pdev->dev,
			"Can't set out_src parent clock\n");
		ret = PTR_ERR(p_audiolocker->out_src);
		return ret;
	}

	of_property_read_u32(pdev->dev.of_node, "frequency",
			&p_audiolocker->expected_freq);

	of_property_read_u32(pdev->dev.of_node, "dividor",
			&p_audiolocker->dividor);
	if (!p_audiolocker->dividor)
		p_audiolocker->dividor = 1;

	/* irq */
	p_audiolocker->irq = platform_get_irq_byname(pdev, "irq");
	if (p_audiolocker->irq < 0) {
		dev_err(&pdev->dev,
			"Can't get irq irq number\n");
		return -EINVAL;
	}

	ret = request_irq(p_audiolocker->irq,
			locker_isr_handler,
			IRQF_SHARED,
			"audiolocker",
			p_audiolocker);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"audio audiolocker irq register fail\n");
		return -EINVAL;
	}

	p_audiolocker->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, p_audiolocker);

	s_locker = p_audiolocker;

	ret = device_create_file(&pdev->dev, &dev_attr_locker_enable);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"failed to register class\n");
		free_irq(p_audiolocker->irq, p_audiolocker);
		return -EINVAL;
	}

	return 0;
}

static const struct of_device_id audiolocker_device_id[] = {
	{ .compatible = "amlogic, audiolocker" },
	{}
};
MODULE_DEVICE_TABLE(of, audiolocker_device_id);

static struct platform_driver audiolocker_platform_driver = {
	.driver = {
		.name  = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(audiolocker_device_id),
	},
	.probe  = audiolocker_platform_probe,
};
module_platform_driver(audiolocker_platform_driver);
