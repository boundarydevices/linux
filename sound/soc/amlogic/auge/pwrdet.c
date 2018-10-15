/*
 * sound/soc/amlogic/auge/pwrdet.c
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
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/pm_wakeirq.h>

#include <linux/amlogic/pm.h>

#include <sound/pcm.h>

#include "pwrdet.h"
#include "pwrdet_hw.h"
#include "ddr_mngr.h"

#define DRV_NAME "power-detect"


static struct aml_pwrdet *s_pwrdet;

static irqreturn_t pwrdet_isr_handler(int irq, void *data)
{
	int det_val = audiobus_read(EE_AUDIO_POW_DET_VALUE
		+ s_pwrdet->chipinfo->address_shift);
	int acc_data = det_val & 0x03ffffff;

	pr_info("%s, Status:%d, acc_data:0x%x, status2:0x%x\n",
		__func__,
		(det_val & 0x80000000) >> 31,
		acc_data,
		audiobus_read(EE_AUDIO_TODDR_A_STATUS2)
		);

	return IRQ_HANDLED;
}

static void pwrdet_free_irq(struct aml_pwrdet *p_pwrdet)
{
	free_irq(p_pwrdet->irq, p_pwrdet);
}

static int pwrdet_request_irq(struct aml_pwrdet *p_pwrdet)
{
	int ret;

	ret = request_irq(p_pwrdet->irq,
			pwrdet_isr_handler,
			IRQF_SHARED,
			"power-detect",
			p_pwrdet);

	if (ret) {
		pr_err("failed to claim irq %u\n", p_pwrdet->irq);
		return ret;
	}

	return ret;
}

void pwrdet_set(bool enable)
{
	pwrdet_threshold(s_pwrdet->chipinfo->address_shift,
		s_pwrdet->hi_th, s_pwrdet->lo_th);

	pwrdet_downsample_ctrl(true, true, 3, 0x20);

	aml_pwrdet_enable(enable, PDMIN);

	pr_info("pwrdet,ctrl:0x%x, hi:0x%x, lo:0x%x\n",
		audiobus_read(EE_AUDIO_POW_DET_CTRL0),
		audiobus_read(EE_AUDIO_POW_DET_TH_HI),
		audiobus_read(EE_AUDIO_POW_DET_TH_LO));
}

static ssize_t pwrdet_enable_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	int enable = 1;

	pwrdet_set(true);

	/* power detect irq */
	pwrdet_request_irq(s_pwrdet);

	return sprintf(buf, "%d\n", enable);
}

static struct class_attribute pwrdet_attrs[] = {
	__ATTR_RO(pwrdet_enable),

	__ATTR_NULL
};

static struct class pwrdet_class = {
	.name = DRV_NAME,
	.class_attrs = pwrdet_attrs,
};

static struct pwrdet_chipinfo axg_pwrdet_chipinfo = {
	.address_shift = 0,
};

static struct pwrdet_chipinfo g12a_pwrdet_chipinfo = {
	.address_shift = 1,
	.matchid_fn   = true,
};

static const struct of_device_id aml_pwrdet_device_id[] = {
	{
		.compatible = "amlogic, axg-power-detect",
		.data       = &axg_pwrdet_chipinfo,
	},
	{
		.compatible = "amlogic, g12a-power-detect",
		.data       = &g12a_pwrdet_chipinfo,
	},
	{},
};
MODULE_DEVICE_TABLE(of, aml_pwrdet_device_id);

static int aml_pwrdet_platform_probe(struct platform_device *pdev)
{
	struct aml_pwrdet *p_pwrdet;
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct pwrdet_chipinfo *p_chipinfo;

	int ret;

	p_pwrdet = devm_kzalloc(&pdev->dev,
			sizeof(struct aml_pwrdet),
			GFP_KERNEL);
	if (!p_pwrdet) {
		/*dev_err(&pdev->dev, "Can't allocate pcm_p\n");*/
		ret = -ENOMEM;
		goto fail;
	}

	/* match data */
	p_chipinfo = (struct pwrdet_chipinfo *)
		of_device_get_match_data(dev);
	if (!p_chipinfo)
		dev_warn_once(dev, "check whether to update power detect chipinfo\n");
	p_pwrdet->chipinfo = p_chipinfo;

	/* irq */
	p_pwrdet->irq = of_irq_get_byname(node, "pwrdet_irq");
	if (p_pwrdet->irq < 0) {
		pr_err("Can't get power detect irq number\n");
		ret = -EINVAL;
		goto fail;
	}
	ret = of_property_read_u32(node, "pwrdet_src",
		&p_pwrdet->det_src);
	if (ret) {
		pr_err("failed to get det_src\n");
		ret = -EINVAL;
		goto fail;
	}

	ret = of_property_read_u32(node, "hi_th",
		&p_pwrdet->hi_th);
	if (ret) {
		pr_err("failed to get hi_th\n");
		ret = -EINVAL;
		goto fail;
	}
	ret = of_property_read_u32(node, "lo_th",
		&p_pwrdet->lo_th);
	if (ret) {
		pr_err("failed to get lo_th\n");
		ret = -EINVAL;
		goto fail;
	}

	dev_set_drvdata(dev, p_pwrdet);
	s_pwrdet = p_pwrdet;

	pr_info("parse power det src:%d, \thi_th:0x%x, lo_th:0x%x\n",
		p_pwrdet->det_src,
		p_pwrdet->hi_th,
		p_pwrdet->lo_th);

	ret = class_register(&pwrdet_class);
	if (ret < 0) {
		pr_err("Create pwrdet class failed\n");
		ret = -EEXIST;
		goto fail;
	}

	return 0;

fail:
	return ret;

}

static int aml_pwrdet_platform_remove(struct platform_device *pdev)
{
	return 0;
}

static int aml_pwrdet_platform_suspend(struct platform_device *pdev,
	pm_message_t state)
{
	struct device *dev = &pdev->dev;
	struct aml_pwrdet *p_pwrdet = dev_get_drvdata(dev);

	pr_info("%s entry freeze:%d\n", __func__, is_pm_freeze_mode());

	/*whether in freeze*/
	if (!is_pm_freeze_mode())
		return 0;

	pwrdet_set(true);

	/* power detect irq */
	pwrdet_request_irq(p_pwrdet);

	/* pm would wakeup kernel */
	device_init_wakeup(dev, 1);
	dev_pm_set_wake_irq(dev, p_pwrdet->irq);

	return 0;
}

static int aml_pwrdet_platform_resume(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct aml_pwrdet *p_pwrdet = dev_get_drvdata(dev);

	pr_info("%s freeze:%d\n", __func__, is_pm_freeze_mode());

	/* pm clean irq */
	dev_pm_clear_wake_irq(dev);
	/* free irq */
	pwrdet_free_irq(p_pwrdet);

	return 0;
}

struct platform_driver aml_pwrdet = {
	.driver  = {
		.name           = DRV_NAME,
		.of_match_table = aml_pwrdet_device_id,
	},
	.probe   = aml_pwrdet_platform_probe,
	.remove  = aml_pwrdet_platform_remove,
	.suspend = aml_pwrdet_platform_suspend,
	.resume  = aml_pwrdet_platform_resume,
};
module_platform_driver(aml_pwrdet);

/* Module information */
MODULE_AUTHOR("Amlogic, Inc.");
MODULE_DESCRIPTION("ALSA Soc Aml Audio Power detect");
MODULE_LICENSE("GPL v2");
