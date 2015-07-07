/*
 * Fakes a power down by blanking the displays. Then wait on a GPIO toggle
 * to reset the platform.
 *
 * Copyright (C) 2015 Boundary Devices, Inc.
 *
 * Based on gpio-poweroff.c by Jamie Lentin
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>

/*
 * Hold configuration here, cannot be more than one instance of the driver
 * since pm_power_off itself is global.
 */
static int gpio_num = -1;
static int gpio_active_low;

static void fake_poweroff_do_poweroff(void)
{
	int i;
	int waspressed = 0;

	pr_info("%s\n", __func__);

	/* Turn off the displays */
	for (i = 0; i < num_registered_fb; i++) {
		if (registered_fb[i])
			fb_blank(registered_fb[i], FB_BLANK_POWERDOWN);
	}

	while (1) {
		/* Check on reset request */
		if (gpio_is_valid(gpio_num)) {
			int pressed =
				(!gpio_active_low == gpio_get_value(gpio_num));
			if (!pressed && waspressed)
				break;
			if (waspressed != pressed)
				waspressed = pressed;
		}
		mdelay(100);
	}

	machine_restart(NULL);
}

static int fake_poweroff_probe(struct platform_device *pdev)
{
	enum of_gpio_flags flags;

	/* If a pm_power_off function has already been added: force */
	if (pm_power_off != NULL)
		pr_info("%s: forcing pm_power_off as already registered",
			__func__);

	gpio_num = of_get_gpio_flags(pdev->dev.of_node, 0, &flags);
	if (!gpio_is_valid(gpio_num))
		pr_info("%s: gpio field not valid: discard\n", __func__);
	else
		gpio_active_low = flags & OF_GPIO_ACTIVE_LOW;

	/* Force pm_power_off to be fake */
	pm_power_off = &fake_poweroff_do_poweroff;

	return 0;
}

static int fake_poweroff_remove(struct platform_device *pdev)
{
	if (pm_power_off == &fake_poweroff_do_poweroff)
		pm_power_off = NULL;

	return 0;
}

static const struct of_device_id of_fake_poweroff_match[] = {
	{ .compatible = "fake-poweroff", },
	{},
};

static struct platform_driver fake_poweroff_driver = {
	.probe = fake_poweroff_probe,
	.remove = fake_poweroff_remove,
	.driver = {
		.name = "fake-poweroff",
		.owner = THIS_MODULE,
		.of_match_table = of_fake_poweroff_match,
	},
};

module_platform_driver(fake_poweroff_driver);

MODULE_AUTHOR("Gary Bisson <gary.bisson@boundarydevices.com>");
MODULE_DESCRIPTION("fake poweroff driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:fake-poweroff");
