/*
 * drivers/amlogic/watchdog/meson_wdt.c
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

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/moduleparam.h>
#include <linux/timer.h>
#include <linux/watchdog.h>
#include <linux/of.h>
#include <uapi/linux/reboot.h>
#include <linux/notifier.h>
#include <linux/suspend.h>
#include <linux/reboot.h>
#include <linux/clk.h>
#include <linux/of_address.h>

#undef pr_fmt
#define pr_fmt(fmt)    "aml-wdt: %s: " fmt, __func__

#define	CTRL	0x0
#define	CTRL1	0x4
#define	TCNT	0x8
#define	RESET	0xc

struct aml_wdt_dev {
	unsigned int min_timeout, max_timeout;
	unsigned int default_timeout, reset_watchdog_time, shutdown_timeout;
	unsigned int firmware_timeout, suspend_timeout, timeout;
	unsigned int one_second;
	struct device *dev;
	struct mutex lock;
	unsigned int reset_watchdog_method;
	struct delayed_work boot_queue;
	void __iomem *reg_base;
	struct notifier_block pm_notifier;
	struct notifier_block reboot_notifier;
};


static void aml_update_bits(void __iomem  *reg, unsigned int mask,
							unsigned int val)
{
	unsigned int tmp, orig;

	orig = readl(reg);
	tmp = orig & ~mask;
	tmp |= val & mask;
	writel(tmp, reg);
}

static void enable_watchdog(struct aml_wdt_dev *awdtv)
{
	aml_update_bits(awdtv->reg_base + CTRL, 0x1<<18, 0x1<<18);
}

static void disable_watchdog(struct aml_wdt_dev *awdtv)
{
	aml_update_bits(awdtv->reg_base + CTRL, 0x1<<18, 0<<18);
}

static void reset_watchdog(struct aml_wdt_dev *awdtv)
{
	writel(0x0, awdtv->reg_base + RESET);
}

static void set_watchdog_cnt(struct aml_wdt_dev *awdtv, unsigned int cnt)
{
	writel((cnt & 0xffff), awdtv->reg_base + TCNT);
}

static unsigned int read_watchdog_tcnt(struct aml_wdt_dev *awdtv)
{
	return (readl(awdtv->reg_base + TCNT) & 0xffff0000) >> 16;
}

static int aml_wdt_start(struct watchdog_device *wdog)
{
	struct aml_wdt_dev *wdev = watchdog_get_drvdata(wdog);

	mutex_lock(&wdev->lock);
	if (wdog->timeout == 0xffffffff)
		set_watchdog_cnt(wdev, wdev->default_timeout *
							wdev->one_second);
	else
		set_watchdog_cnt(wdev, wdog->timeout * wdev->one_second);
	enable_watchdog(wdev);
	mutex_unlock(&wdev->lock);
#if 0
	if (wdev->boot_queue)
		cancel_delayed_work(&wdev->boot_queue);
#endif
	return 0;
}

static int aml_wdt_stop(struct watchdog_device *wdog)
{
	struct aml_wdt_dev *wdev = watchdog_get_drvdata(wdog);

	mutex_lock(&wdev->lock);
	disable_watchdog(wdev);
	mutex_unlock(&wdev->lock);
	return 0;
}

static int aml_wdt_ping(struct watchdog_device *wdog)
{
	struct aml_wdt_dev *wdev = watchdog_get_drvdata(wdog);

	mutex_lock(&wdev->lock);
	reset_watchdog(wdev);
	mutex_unlock(&wdev->lock);

	return 0;
}

static int aml_wdt_set_timeout(struct watchdog_device *wdog,
				unsigned int timeout)
{
	struct aml_wdt_dev *wdev = watchdog_get_drvdata(wdog);

	mutex_lock(&wdev->lock);
	set_watchdog_cnt(wdev, timeout * wdev->one_second);
	wdog->timeout = timeout;
	wdev->timeout = timeout;
	mutex_unlock(&wdev->lock);

	return 0;
}

unsigned int aml_wdt_get_timeleft(struct watchdog_device *wdog)
{
	struct aml_wdt_dev *wdev = watchdog_get_drvdata(wdog);
	unsigned int timeleft;

	mutex_lock(&wdev->lock);
	timeleft = read_watchdog_tcnt(wdev);
	mutex_unlock(&wdev->lock);
	return timeleft/wdev->one_second;
}

static void boot_moniter_work(struct work_struct *work)
{
	struct aml_wdt_dev *wdev = container_of(work, struct aml_wdt_dev,
							boot_queue.work);
	reset_watchdog(wdev);
	mod_delayed_work(system_freezable_wq, &wdev->boot_queue,
	round_jiffies(msecs_to_jiffies(wdev->reset_watchdog_time*1000)));
}

static const struct watchdog_info aml_wdt_info = {
	.options = WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING,
	.identity = "aml Watchdog",
};

static const struct watchdog_ops aml_wdt_ops = {
	.owner		= THIS_MODULE,
	.start		= aml_wdt_start,
	.stop		= aml_wdt_stop,
	.ping		= aml_wdt_ping,
	.set_timeout	= aml_wdt_set_timeout,
	.get_timeleft   = aml_wdt_get_timeleft,
};

void aml_init_pdata(struct aml_wdt_dev *wdev)
{
	int ret;
	struct clk *clk;

	ret = of_property_read_u32(wdev->dev->of_node, "default_timeout",
						&wdev->default_timeout);
	if (ret) {
		dev_err(wdev->dev,
		"dt probe default_timeout failed: %d\n", ret);
		wdev->default_timeout = 5;
	}
	ret = of_property_read_u32(wdev->dev->of_node, "reset_watchdog_method",
					&wdev->reset_watchdog_method);
	if (ret) {
		dev_err(wdev->dev,
			"dt probe reset_watchdog_method failed: %d\n", ret);
		wdev->reset_watchdog_method = 1;
	}
	ret = of_property_read_u32(wdev->dev->of_node, "reset_watchdog_time",
						&wdev->reset_watchdog_time);
	if (ret) {
		dev_err(wdev->dev,
			"dt probe reset_watchdog_time failed: %d\n", ret);
		wdev->reset_watchdog_time = 2;
	}

	ret = of_property_read_u32(wdev->dev->of_node, "shutdown_timeout",
						&wdev->shutdown_timeout);
	if (ret) {
		dev_err(wdev->dev,
			"dt probe shutdown_timeout failed: %d\n", ret);
		wdev->shutdown_timeout = 10;
	}

	ret = of_property_read_u32(wdev->dev->of_node, "firmware_timeout",
						&wdev->firmware_timeout);
	if (ret) {
		dev_err(wdev->dev,
				"dt probe firmware_timeout failed: %d\n", ret);
		wdev->firmware_timeout = 6;
	}

	ret = of_property_read_u32(wdev->dev->of_node, "suspend_timeout",
						&wdev->suspend_timeout);
	if (ret) {
		dev_err(wdev->dev,
				"dt probe suspend_timeout failed: %d\n", ret);
		wdev->suspend_timeout = 6;
	}


	wdev->reg_base = of_iomap(wdev->dev->of_node, 0);
	clk = clk_get(wdev->dev, NULL);
	aml_update_bits(wdev->reg_base + CTRL, 0x3<<21, 0x3<<21);
	aml_update_bits(wdev->reg_base + CTRL, 0x3<<24, 0x3<<24);
	aml_update_bits(wdev->reg_base + CTRL, 0x3ffff, 24000);

	wdev->one_second = 1000;
	wdev->max_timeout = 0xffff/1000;
	wdev->min_timeout = 1;
}

static int aml_wtd_pm_notify(struct notifier_block *nb, unsigned long event,
	void *dummy)
{
	struct aml_wdt_dev *wdev;

	wdev = container_of(nb, struct aml_wdt_dev, pm_notifier);

	if (event == PM_SUSPEND_PREPARE) {
		disable_watchdog(wdev);
		pr_info("disable watchdog\n");
	}
	if (event == PM_POST_SUSPEND) {
		enable_watchdog(wdev);
		pr_info("enable watchdog\n");
	}
	return NOTIFY_OK;
}

static int aml_wtd_reboot_notify(struct notifier_block *nb,
				 unsigned long event,
				void *dummy)
{
	struct aml_wdt_dev *wdev;

	wdev = container_of(nb, struct aml_wdt_dev, reboot_notifier);
	if (event == SYS_DOWN || event == SYS_HALT) {
		disable_watchdog(wdev);
		pr_info("disable watchdog\n");
	}
	if (wdev->reset_watchdog_method == 1)
		cancel_delayed_work(&wdev->boot_queue);
	return NOTIFY_OK;
}


static struct notifier_block aml_wdt_pm_notifier = {
	.notifier_call = aml_wtd_pm_notify,
};

static struct notifier_block aml_wdt_reboot_notifier = {
	.notifier_call = aml_wtd_reboot_notify,
};

static int aml_wdt_probe(struct platform_device *pdev)
{
	struct watchdog_device *aml_wdt;
	struct aml_wdt_dev *wdev;
	int ret;

	aml_wdt = devm_kzalloc(&pdev->dev, sizeof(*aml_wdt), GFP_KERNEL);
	if (!aml_wdt)
		return -ENOMEM;

	wdev = devm_kzalloc(&pdev->dev, sizeof(*wdev), GFP_KERNEL);
	if (!wdev)
		return -ENOMEM;
	wdev->dev		= &pdev->dev;
	mutex_init(&wdev->lock);
	aml_init_pdata(wdev);

	aml_wdt->info	      = &aml_wdt_info;
	aml_wdt->ops	      = &aml_wdt_ops;
	aml_wdt->min_timeout = wdev->min_timeout;
	aml_wdt->max_timeout = wdev->max_timeout;
	aml_wdt->timeout = 0xffffffff;
	wdev->timeout = 0xffffffff;

	watchdog_set_drvdata(aml_wdt, wdev);
	platform_set_drvdata(pdev, aml_wdt);
	if (wdev->reset_watchdog_method == 1) {

		INIT_DELAYED_WORK(&wdev->boot_queue, boot_moniter_work);
		mod_delayed_work(system_freezable_wq, &wdev->boot_queue,
	 round_jiffies(msecs_to_jiffies(wdev->reset_watchdog_time*1000)));
		enable_watchdog(wdev);
		set_watchdog_cnt(wdev,
				 wdev->default_timeout * wdev->one_second);
		dev_info(wdev->dev, "creat work queue for watch dog\n");
	}
	ret = watchdog_register_device(aml_wdt);
	if (ret)
		return ret;

	wdev->pm_notifier = aml_wdt_pm_notifier;
	wdev->reboot_notifier = aml_wdt_reboot_notifier;
	register_pm_notifier(&wdev->pm_notifier);
	register_reboot_notifier(&wdev->reboot_notifier);
	dev_info(wdev->dev, "AML Watchdog Timer probed done\n");

	return 0;
}

static void aml_wdt_shutdown(struct platform_device *pdev)
{
	struct watchdog_device *wdog = platform_get_drvdata(pdev);
	struct aml_wdt_dev *wdev = watchdog_get_drvdata(wdog);

	if (wdev->reset_watchdog_method == 1)
		cancel_delayed_work(&wdev->boot_queue);
	disable_watchdog(wdev);
}

static int aml_wdt_remove(struct platform_device *pdev)
{
	struct watchdog_device *wdog = platform_get_drvdata(pdev);

	aml_wdt_stop(wdog);
	return 0;
}

#ifdef	CONFIG_PM
static int aml_wdt_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int aml_wdt_resume(struct platform_device *pdev)
{
	return 0;
}

#else
#define	aml_wdt_suspend	NULL
#define	aml_wdt_resume		NULL
#endif

static const struct of_device_id aml_wdt_of_match[] = {
	{ .compatible = "amlogic, meson-wdt", },
	{},
};
MODULE_DEVICE_TABLE(of, aml_wdt_of_match);

static struct platform_driver aml_wdt_driver = {
	.probe		= aml_wdt_probe,
	.remove		= aml_wdt_remove,
	.shutdown	= aml_wdt_shutdown,
	.suspend	= aml_wdt_suspend,
	.resume		= aml_wdt_resume,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "aml_wdt",
		.of_match_table = aml_wdt_of_match,
	},
};

static int __init aml_wdt_driver_init(void)
{
	return platform_driver_register(&(aml_wdt_driver));
}

module_init(aml_wdt_driver_init);

static void __exit aml_wdt_driver_exit(void)
{
	platform_driver_unregister(&(aml_wdt_driver));
}

module_exit(aml_wdt_driver_exit);

MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:aml_wdt");


