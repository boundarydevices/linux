/*
 * linux/drivers/leds-pwm.c
 *
 * simple PWM based LED control
 *
 * Copyright 2009 Luotao Fu @ Pengutronix (l.fu@pengutronix.de)
 *
 * based on leds-gpio.c by Raphael Assenat <raph@8d.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/ctype.h>
#include <linux/leds.h>
#include <linux/err.h>
#include <linux/pwm.h>
#include <linux/leds_pwm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include "leds.h"

/* lowercase notes are an octave higher*/
/* n means move up n octaves */
/* Note: sharp (#) or flat (_) precedes note that it applies to */
unsigned base_A_G[] = {
	36363636,	/* "A",	27.50 hz, 1250 cm */
	32396317,	/* "B",	30.87 hz, 1110 cm */
	61156103,	/* "C",	16.35 hz, 2100 cm */
	54483894,	/* "D",	18.35 hz, 1870 cm */
	48539631,	/* "E",	20.60 hz, 1670 cm */
	45815311,	/* "F",	21.83 hz, 1580 cm */
	40816802,	/* "G",	24.50 hz, 1400 cm */
};
unsigned sharp_A_G[] = {
	34322702,	/* "#A", "_B"	29.14 hz, 1180 cm */
	0,		/* "B",		30.87 hz, 1110 cm */
	57723675,	/* "#C", "_D"	17.32 hz, 1990 cm */
	51425948,	/* "#D", "_E"	19.45 hz, 1770 cm */
	0,		/* "E",		20.60 hz, 1670 cm */
	43243895,	/* "#F", "_G"	23.12 hz, 1490 cm */
	38525931,	/* "#G", "_A"	25.96 hz, 1320 cm */
};
unsigned flat_A_G[] = {
	38525931,	/* "_A"	25.96 hz, 1320 cm */
	34322702,	/* "_B"	29.14 hz, 1180 cm */
	0,		/* "C",	16.35 hz, 2100 cm */
	57723675,	/* "_D" 17.32 hz, 1990 cm */
	51425948,	/* "_E"	19.45 hz, 1770 cm */
	0,		/* "F",	21.83 hz, 1580 cm */
	43243895,	/* "_G"	23.12 hz, 1490 cm */
};

struct led_pwm_data {
	struct led_classdev	cdev;
	struct pwm_device	*pwm;
	unsigned int 		active_low;
	unsigned int		period;
	unsigned char		octave;
#define NM_NORMAL 0
#define NM_SHARP 1
#define NM_FLAT 2
	unsigned char		note_mode;
};

static void led_pwm_set(struct led_classdev *led_cdev,
	enum led_brightness brightness)
{
	struct led_pwm_data *led_dat =
		container_of(led_cdev, struct led_pwm_data, cdev);
	unsigned int max = led_dat->cdev.max_brightness;
	unsigned int period =  led_dat->period;

	if (period == 0)
		pwm_disable(led_dat->pwm);
	else if (brightness == 0) {
		pwm_config(led_dat->pwm, 0, period);
		pwm_enable(led_dat->pwm);
	} else {
		pwm_config(led_dat->pwm, brightness * period / max, period);
		pwm_enable(led_dat->pwm);
	}
}

static ssize_t led_frequency_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct led_pwm_data *led_dat =
		container_of(led_cdev, struct led_pwm_data, cdev);
	unsigned int period =  led_dat->period;
	unsigned int freq = period ? 1000000000 / period : 0;

	return sprintf(buf, "%u\n", freq);
}

static ssize_t led_frequency_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct led_pwm_data *led_dat =
		container_of(led_cdev, struct led_pwm_data, cdev);
	ssize_t ret = -EINVAL;
	char *after;
	unsigned long freq = simple_strtoul(buf, &after, 10);
	size_t count = after - buf;

	if (*after && isspace(*after))
		count++;

	if (count == size) {
		ret = count;

		led_dat->period = freq ? 1000000000 / freq : 0;
		if (!led_cdev->brightness)
			led_cdev->brightness = led_cdev->max_brightness >> 1;
		led_set_brightness(led_cdev, led_cdev->brightness);
	}

	return ret;
}
static ssize_t led_period_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct led_pwm_data *led_dat =
		container_of(led_cdev, struct led_pwm_data, cdev);
	unsigned int period =  led_dat->period;

	return sprintf(buf, "%u\n", period);
}

static ssize_t led_period_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct led_pwm_data *led_dat =
		container_of(led_cdev, struct led_pwm_data, cdev);
	ssize_t ret = -EINVAL;
	char *after;
	unsigned long period = simple_strtoul(buf, &after, 10);
	size_t count = after - buf;

	if (*after && isspace(*after))
		count++;

	if (count == size) {
		ret = count;
		led_dat->period = period;
		led_set_brightness(led_cdev, led_cdev->brightness);
	}

	return ret;
}

static ssize_t led_note_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct led_pwm_data *led_dat =
		container_of(led_cdev, struct led_pwm_data, cdev);
	unsigned int period =  led_dat->period;
	unsigned i;
	int octave = 0;
	unsigned best = 0;
	unsigned best_err = 0xffffffff;
	char note_mode = 0;

	if (period == 0)
		return sprintf(buf, " \n");

	while (period < base_A_G[2]) {
		octave++;
		period <<= 1;
	}
	while (period > base_A_G[2]) {
		octave--;
		period >>= 1;
	}
	for (i = 0; i < 7; i++) {
		unsigned err = (base_A_G[i] > period) ? (base_A_G[i] - period) :
			(period - base_A_G[i]);
		if (best_err > err) {
			best = i;
			best_err = err;
			note_mode = 0;
		}
		err = (sharp_A_G[i] > period) ? (sharp_A_G[i] - period) :
			(period - sharp_A_G[i]);
		if (best_err > err) {
			best = i;
			best_err = err;
			note_mode = '#';
		}
	}
	return sprintf(buf, "%u%s%c\n", octave, &note_mode, 'A' + best);
}

static ssize_t led_note_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct led_pwm_data *led_dat =
		container_of(led_cdev, struct led_pwm_data, cdev);
	size_t count = 0;
	while (count < size) {
		char octave = led_dat->octave;
		char note_mode = led_dat->note_mode;
		char c = *buf++;
		int period = -1;
		count++;
		if (c == ' ')
			period = 0;
		else if (c == '_')
			note_mode = NM_FLAT;
		else if (c == '#')
			note_mode = NM_SHARP;
		else if ((c >= '0') && (c <= '9')) {
			octave = c - '0';
			led_dat->octave = octave;
		} else {
			if ((c >= 'a') && (c <= 'g')) {
				octave++;
				c -= 'a' - 'A';
			}
			if ((c >= 'A') && (c <= 'G')) {
				c -= 'A';
				if (note_mode == NM_NORMAL)
					period = base_A_G[(int)c];
				else if (note_mode == NM_SHARP)
					period = sharp_A_G[(int)c];
				else if (note_mode == NM_FLAT)
					period = flat_A_G[(int)c];
			}
		}
		led_dat->note_mode = note_mode;
		if (period >= 0) {
			period >>= octave;
			led_dat->period = period;
			if (!led_cdev->brightness)
				led_cdev->brightness = led_cdev->max_brightness >> 1;
			led_set_brightness(led_cdev, led_cdev->brightness);
			led_dat->note_mode = NM_NORMAL;
			if (count < size)
				msleep(period ? 1000 : 100);
		}
	}
	return count;
}

static DEVICE_ATTR(note, 0644, led_note_show, led_note_store);
static DEVICE_ATTR(frequency, 0644, led_frequency_show, led_frequency_store);
static DEVICE_ATTR(period, 0644, led_period_show, led_period_store);

static int led_pwm_probe(struct platform_device *pdev)
{
	struct led_pwm_platform_data *pdata = pdev->dev.platform_data;
	struct led_pwm *cur_led;
	struct led_pwm_data *leds_data, *led_dat;
	int i, ret = 0;

	if (!pdata)
		return -EBUSY;

	leds_data = kzalloc(sizeof(struct led_pwm_data) * pdata->num_leds,
				GFP_KERNEL);
	if (!leds_data)
		return -ENOMEM;

	i = 0;
	while (i < pdata->num_leds) {
		int rc;
		cur_led = &pdata->leds[i];
		led_dat = &leds_data[i];

		led_dat->pwm = pwm_request(cur_led->pwm_id,
				cur_led->name);
		if (IS_ERR(led_dat->pwm)) {
			ret = PTR_ERR(led_dat->pwm);
			dev_err(&pdev->dev, "unable to request PWM %d\n",
					cur_led->pwm_id);
			goto err;
		}

		led_dat->cdev.name = cur_led->name;
		led_dat->cdev.default_trigger = cur_led->default_trigger;
		led_dat->active_low = cur_led->active_low;
		led_dat->octave = 4;
		led_dat->period = cur_led->pwm_period_ns;
		led_dat->cdev.brightness_set = led_pwm_set;
		led_dat->cdev.brightness = LED_OFF;
		led_dat->cdev.max_brightness = cur_led->max_brightness;
		led_dat->cdev.flags |= LED_CORE_SUSPENDRESUME;

		ret = led_classdev_register(&pdev->dev, &led_dat->cdev);
		if (ret < 0) {
			pwm_free(led_dat->pwm);
			goto err;
		}

		i++;
		/* register the attributes */
		rc = device_create_file(led_dat->cdev.dev, &dev_attr_frequency);
		if (rc)
			goto err;
		rc = device_create_file(led_dat->cdev.dev, &dev_attr_period);
		if (rc) {
			goto err;
		}
		rc = device_create_file(led_dat->cdev.dev, &dev_attr_note);
		if (rc) {
			goto err;
		}
	}

	platform_set_drvdata(pdev, leds_data);

	return 0;

err:
	if (i > 0) {
		for (i = i - 1; i >= 0; i--) {
			device_remove_file(led_dat->cdev.dev, &dev_attr_note);
			device_remove_file(led_dat->cdev.dev, &dev_attr_frequency);
			device_remove_file(led_dat->cdev.dev, &dev_attr_period);
			led_classdev_unregister(&leds_data[i].cdev);
			pwm_free(leds_data[i].pwm);
		}
	}

	kfree(leds_data);

	return ret;
}

static int __devexit led_pwm_remove(struct platform_device *pdev)
{
	int i;
	struct led_pwm_platform_data *pdata = pdev->dev.platform_data;
	struct led_pwm_data *leds_data;

	leds_data = platform_get_drvdata(pdev);

	for (i = 0; i < pdata->num_leds; i++) {
		led_classdev_unregister(&leds_data[i].cdev);
		pwm_free(leds_data[i].pwm);
	}

	kfree(leds_data);

	return 0;
}

static struct platform_driver led_pwm_driver = {
	.probe		= led_pwm_probe,
	.remove		= __devexit_p(led_pwm_remove),
	.driver		= {
		.name	= "leds_pwm",
		.owner	= THIS_MODULE,
	},
};

static int __init led_pwm_init(void)
{
	return platform_driver_register(&led_pwm_driver);
}

static void __exit led_pwm_exit(void)
{
	platform_driver_unregister(&led_pwm_driver);
}

module_init(led_pwm_init);
module_exit(led_pwm_exit);

MODULE_AUTHOR("Luotao Fu <l.fu@pengutronix.de>");
MODULE_DESCRIPTION("PWM LED driver for PXA");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:leds-pwm");
