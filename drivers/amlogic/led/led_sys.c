/*
 * drivers/amlogic/led/led_sys.c
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

#define pr_fmt(fmt)	"sysled: " fmt

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/leds.h>
#include <linux/gpio.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/slab.h>

#include "led_sys.h"


#define AML_DEV_NAME		"sysled"
#define AML_LED_NAME		"led-sys"


static void aml_sysled_output_setup(struct aml_sysled_dev *ldev,
				enum led_brightness value)
{
	unsigned int level = !!value;

	if (ldev->d.active_low)
		level = !level;
	gpio_direction_output(ldev->d.pin, level);
}

static void aml_sysled_work(struct work_struct *work)
{
	struct aml_sysled_dev *ldev;

	ldev = container_of(work, struct aml_sysled_dev, work);

	mutex_lock(&ldev->lock);
	aml_sysled_output_setup(ldev, ldev->new_brightness);
	mutex_unlock(&ldev->lock);
}


static void aml_sysled_brightness_set(struct led_classdev *cdev,
	enum led_brightness value)
{
	struct aml_sysled_dev *ldev;
	struct platform_device *pdev;

	pdev = to_platform_device(cdev->dev->parent);
	ldev = platform_get_drvdata(pdev);
	ldev->new_brightness = value;
	schedule_work(&ldev->work);
}


static int aml_sysled_dt_parse(struct platform_device *pdev)
{
	struct device_node *node;
	struct aml_sysled_dev *ldev;
	int led_gpio;
	enum of_gpio_flags flags;

	ldev = platform_get_drvdata(pdev);
	node = pdev->dev.of_node;
	if (!node) {
		pr_err("failed to find node for %s\n", AML_DEV_NAME);
		return -ENODEV;
	}

	led_gpio = of_get_named_gpio_flags(node, "led_gpio", 0, &flags);
	if (!gpio_is_valid(led_gpio)) {
		pr_err("gpio %d is not valid\n", led_gpio);
		return -EINVAL;
	}

	ldev->d.pin = led_gpio;
	ldev->d.active_low = flags & OF_GPIO_ACTIVE_LOW;
	pr_info("led_gpio = %u\n", ldev->d.pin);
	pr_info("active_low = %u\n", ldev->d.active_low);
	gpio_request(ldev->d.pin, AML_DEV_NAME);
	gpio_direction_output(ldev->d.pin, 1);

	return 0;
}



static const struct of_device_id aml_sysled_dt_match[] = {
	{
		.compatible = "amlogic, sysled",
	},
	{},
};


static int aml_sysled_probe(struct platform_device *pdev)
{
	struct aml_sysled_dev *ldev;
	int ret;

	ldev = kzalloc(sizeof(struct aml_sysled_dev), GFP_KERNEL);

	/* set driver data */
	platform_set_drvdata(pdev, ldev);

	/* parse dt param */
	ret = aml_sysled_dt_parse(pdev);
	if (ret)
		return ret;

	/* register led class device */
	ldev->cdev.name = AML_LED_NAME;
	ldev->cdev.brightness_set = aml_sysled_brightness_set;
	mutex_init(&ldev->lock);
	INIT_WORK(&ldev->work, aml_sysled_work);
	ret = led_classdev_register(&pdev->dev, &ldev->cdev);
	if (ret < 0) {
		kfree(ldev);
		return ret;
	}

	/* set led default on */
	aml_sysled_output_setup(ldev, 1);

	pr_info("module probed ok\n");
	return 0;
}


static int __exit aml_sysled_remove(struct platform_device *pdev)
{
	struct aml_sysled_dev *ldev = platform_get_drvdata(pdev);

	led_classdev_unregister(&ldev->cdev);
	cancel_work_sync(&ldev->work);
	gpio_free(ldev->d.pin);
	platform_set_drvdata(pdev, NULL);
	kfree(ldev);
	pr_info("module removed ok\n");
	return 0;
}


static void aml_sysled_shutdown(struct platform_device *pdev)
{
	struct aml_sysled_dev *ldev = platform_get_drvdata(pdev);
	/* set led off*/
	aml_sysled_output_setup(ldev, 0);
	pr_info("module shutdown ok\n");
}


#ifdef CONFIG_PM
static int aml_sysled_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct aml_sysled_dev *ldev = platform_get_drvdata(pdev);
	/* set led off */
	aml_sysled_output_setup(ldev, 0);
	pr_info("module suspend ok\n");
	return 0;
}

static int aml_sysled_resume(struct platform_device *pdev)
{
	struct aml_sysled_dev *ldev = platform_get_drvdata(pdev);
	/* set led on */
	aml_sysled_output_setup(ldev, 1);
	pr_info("module resume ok\n");
	return 0;
}
#endif


static struct platform_driver aml_sysled_driver = {
	.driver = {
		.name = AML_DEV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = aml_sysled_dt_match,
	},
	.probe = aml_sysled_probe,
	.remove = __exit_p(aml_sysled_remove),
	.shutdown = aml_sysled_shutdown,
#ifdef	CONFIG_PM
	.suspend = aml_sysled_suspend,
	.resume = aml_sysled_resume,
#endif
};


static int __init aml_sysled_init(void)
{
	pr_info("module init\n");
	if (platform_driver_register(&aml_sysled_driver)) {
		pr_err("failed to register driver\n");
		return -ENODEV;
	}

	return 0;
}


static void __exit aml_sysled_exit(void)
{
	pr_info("module exit\n");
	platform_driver_unregister(&aml_sysled_driver);
}


module_init(aml_sysled_init);
module_exit(aml_sysled_exit);

MODULE_DESCRIPTION("Amlogic sys led driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Amlogic, Inc.");

