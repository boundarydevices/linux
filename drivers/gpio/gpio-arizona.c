/*
 * gpiolib support for Wolfson Arizona class devices
 *
 * Copyright 2012 Wolfson Microelectronics PLC.
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>

#include <linux/mfd/arizona/core.h>
#include <linux/mfd/arizona/pdata.h>
#include <linux/mfd/arizona/registers.h>

struct arizona_gpio {
	struct arizona *arizona;
	struct gpio_chip gpio_chip;
};

static inline struct arizona_gpio *to_arizona_gpio(struct gpio_chip *chip)
{
	return container_of(chip, struct arizona_gpio, gpio_chip);
}

static int arizona_gpio_direction_in(struct gpio_chip *chip, unsigned offset)
{
	struct arizona_gpio *arizona_gpio = to_arizona_gpio(chip);
	struct arizona *arizona = arizona_gpio->arizona;

	return regmap_update_bits(arizona->regmap, ARIZONA_GPIO1_CTRL + offset,
				  ARIZONA_GPN_DIR, ARIZONA_GPN_DIR);
}

static int arizona_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	struct arizona_gpio *arizona_gpio = to_arizona_gpio(chip);
	struct arizona *arizona = arizona_gpio->arizona;
	unsigned int val;
	int ret;

	ret = regmap_read(arizona->regmap, ARIZONA_GPIO1_CTRL + offset, &val);
	if (ret < 0)
		return ret;

	if (val & ARIZONA_GPN_LVL)
		return 1;
	else
		return 0;
}

static int arizona_gpio_direction_out(struct gpio_chip *chip,
				     unsigned offset, int value)
{
	struct arizona_gpio *arizona_gpio = to_arizona_gpio(chip);
	struct arizona *arizona = arizona_gpio->arizona;

	if (value)
		value = ARIZONA_GPN_LVL;

	return regmap_update_bits(arizona->regmap, ARIZONA_GPIO1_CTRL + offset,
				  ARIZONA_GPN_DIR | ARIZONA_GPN_LVL, value);
}

static void arizona_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	struct arizona_gpio *arizona_gpio = to_arizona_gpio(chip);
	struct arizona *arizona = arizona_gpio->arizona;

	if (value)
		value = ARIZONA_GPN_LVL;

	regmap_update_bits(arizona->regmap, ARIZONA_GPIO1_CTRL + offset,
			   ARIZONA_GPN_LVL, value);
}

#ifdef CONFIG_DEBUG_FS
static void arizona_gpio_dbg_show(struct seq_file *s, struct gpio_chip *chip)
{
	struct arizona_gpio *arizona_gpio = to_arizona_gpio(chip);
	struct arizona *arizona = arizona_gpio->arizona;
	int i;

	for (i = 0; i < chip->ngpio; i++) {
		int gpio = i + chip->base;
		int reg, ret;
		const char *label;

		/* We report the GPIO even if it's not requested since
		 * we're also reporting things like alternate
		 * functions which apply even when the GPIO is not in
		 * use as a GPIO.
		 */
		label = gpiochip_is_requested(chip, i);
		if (!label)
			label = "Unrequested";

		seq_printf(s, " gpio-%-3d (%-20.20s) ", gpio, label);

		ret = regmap_read(arizona->regmap, ARIZONA_GPIO1_CTRL + i, &reg);

		if (ret < 0) {
			dev_err(arizona->dev,
				"GPIO control %d read failed: %d\n",
				gpio, reg);
			seq_printf(s, "\n");
			continue;
		}

		/* No decode yet; note that GPIO2 is special */
		seq_printf(s, "(%x)\n", reg);
	}
}
#else
#define arizona_gpio_dbg_show NULL
#endif

static struct gpio_chip template_chip = {
	.label			= "arizona",
	.owner			= THIS_MODULE,
	.direction_input	= arizona_gpio_direction_in,
	.get			= arizona_gpio_get,
	.direction_output	= arizona_gpio_direction_out,
	.set			= arizona_gpio_set,
	.dbg_show		= arizona_gpio_dbg_show,
	.can_sleep		= 1,
};

static int __devinit arizona_gpio_probe(struct platform_device *pdev)
{
	struct arizona *arizona = dev_get_drvdata(pdev->dev.parent);
	struct arizona_pdata *pdata = arizona->dev->platform_data;
	struct arizona_gpio *arizona_gpio;
	int ret;

	arizona_gpio = devm_kzalloc(&pdev->dev, sizeof(*arizona_gpio),
				    GFP_KERNEL);
	if (arizona_gpio == NULL)
		return -ENOMEM;

	arizona_gpio->arizona = arizona;
	arizona_gpio->gpio_chip = template_chip;
	arizona_gpio->gpio_chip.dev = &pdev->dev;

	switch (arizona->type) {
	case WM5102:
		arizona_gpio->gpio_chip.ngpio = 5;
		break;
	default:
		dev_err(&pdev->dev, "Unknown chip variant %d\n",
			arizona->type);
		return -EINVAL;
	}

	if (pdata && pdata->gpio_base)
		arizona_gpio->gpio_chip.base = pdata->gpio_base;
	else
		arizona_gpio->gpio_chip.base = -1;

	ret = gpiochip_add(&arizona_gpio->gpio_chip);
	if (ret < 0) {
		dev_err(&pdev->dev, "Could not register gpiochip, %d\n",
			ret);
		goto err;
	}

	platform_set_drvdata(pdev, arizona_gpio);

	return ret;

err:
	return ret;
}

static int __devexit arizona_gpio_remove(struct platform_device *pdev)
{
	struct arizona_gpio *arizona_gpio = platform_get_drvdata(pdev);

	return gpiochip_remove(&arizona_gpio->gpio_chip);
}

static struct platform_driver arizona_gpio_driver = {
	.driver.name	= "arizona-gpio",
	.driver.owner	= THIS_MODULE,
	.probe		= arizona_gpio_probe,
	.remove		= __devexit_p(arizona_gpio_remove),
};

static __init int arizona_gpio_init(void)
{
	return platform_driver_register(&arizona_gpio_driver);
}
module_init(arizona_gpio_init);

static __exit void arizona_gpio_exit(void)
{
	platform_driver_unregister(&arizona_gpio_driver);
}
module_exit(arizona_gpio_exit);


MODULE_AUTHOR("Mark Brown <broonie@opensource.wolfsonmicro.com>");
MODULE_DESCRIPTION("GPIO interface for Arizona devices");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:arizona-gpio");
