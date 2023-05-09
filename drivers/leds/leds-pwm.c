// SPDX-License-Identifier: GPL-2.0-only
/*
 * linux/drivers/leds-pwm.c
 *
 * simple PWM based LED control
 *
 * Copyright 2009 Luotao Fu @ Pengutronix (l.fu@pengutronix.de)
 *
 * based on leds-gpio.c by Raphael Assenat <raph@8d.com>
 */

#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/leds.h>
#include <linux/err.h>
#include <linux/pwm.h>
#include <linux/slab.h>
#include "leds.h"

/* lowercase notes are an octave higher*/
/* n means move up n octaves */
/* Note: sharp (#) or flat (_) precedes note that it applies to */
static unsigned base_a_g[] = {
	36363636,	/* "A",	27.50 hz, 1250 cm */
	32396317,	/* "B",	30.87 hz, 1110 cm */
	61156103,	/* "C",	16.35 hz, 2100 cm */
	54483894,	/* "D",	18.35 hz, 1870 cm */
	48539631,	/* "E",	20.60 hz, 1670 cm */
	45815311,	/* "F",	21.83 hz, 1580 cm */
	40816802,	/* "G",	24.50 hz, 1400 cm */
};
static unsigned sharp_a_g[] = {
	34322702,	/* "#A", "_B"	29.14 hz, 1180 cm */
	0,		/* "B",		30.87 hz, 1110 cm */
	57723675,	/* "#C", "_D"	17.32 hz, 1990 cm */
	51425948,	/* "#D", "_E"	19.45 hz, 1770 cm */
	0,		/* "E",		20.60 hz, 1670 cm */
	43243895,	/* "#F", "_G"	23.12 hz, 1490 cm */
	38525931,	/* "#G", "_A"	25.96 hz, 1320 cm */
};
static unsigned flat_a_g[] = {
	38525931,	/* "_A"	25.96 hz, 1320 cm */
	34322702,	/* "_B"	29.14 hz, 1180 cm */
	0,		/* "C",	16.35 hz, 2100 cm */
	57723675,	/* "_D" 17.32 hz, 1990 cm */
	51425948,	/* "_E"	19.45 hz, 1770 cm */
	0,		/* "F",	21.83 hz, 1580 cm */
	43243895,	/* "_G"	23.12 hz, 1490 cm */
};

struct led_pwm {
	const char	*name;
	u8		active_low;
	u8		default_state;
	unsigned int	max_brightness;
};

struct led_pwm_data {
	struct led_classdev	cdev;
	struct pwm_device	*pwm;
	struct pwm_state	pwmstate;
	unsigned int		active_low;
	unsigned char		octave;
#define NM_NORMAL 0
#define NM_SHARP 1
#define NM_FLAT 2
	unsigned char		note_mode;
};

struct led_pwm_priv {
	int num_leds;
	struct led_pwm_data leds[];
};

static ssize_t led_frequency_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct led_pwm_data *led_dat =
		container_of(led_cdev, struct led_pwm_data, cdev);
	unsigned int period = led_dat->pwmstate.period;
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

		led_dat->pwmstate.period = freq ? 1000000000 / freq : 0;
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
	unsigned int period = led_dat->pwmstate.period;

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
		led_dat->pwmstate.period = period;
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
	unsigned int period = led_dat->pwmstate.period;
	unsigned i;
	int octave = 0;
	unsigned best = 0;
	unsigned best_err = 0xffffffff;
	char note_mode = 0;

	if (period == 0)
		return sprintf(buf, "\n");

	while (period < base_a_g[2]) {
		octave++;
		period <<= 1;
	}
	while (period > base_a_g[2]) {
		octave--;
		period >>= 1;
	}
	for (i = 0; i < 7; i++) {
		unsigned err = (base_a_g[i] > period) ? (base_a_g[i] - period) :
			(period - base_a_g[i]);
		if (best_err > err) {
			best = i;
			best_err = err;
			note_mode = 0;
		}
		err = (sharp_a_g[i] > period) ? (sharp_a_g[i] - period) :
			(period - sharp_a_g[i]);
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
		if (c == ' ') {
			period = 0;
		} else if (c == '_') {
			note_mode = NM_FLAT;
		} else if (c == '#') {
			note_mode = NM_SHARP;
		} else if ((c >= '0') && (c <= '9')) {
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
					period = base_a_g[(int)c];
				else if (note_mode == NM_SHARP)
					period = sharp_a_g[(int)c];
				else if (note_mode == NM_FLAT)
					period = flat_a_g[(int)c];
			}
		}
		led_dat->note_mode = note_mode;
		if (period >= 0) {
			period >>= octave;
			if (period)
				led_dat->pwmstate.period = period;
			if (!led_cdev->brightness)
				led_cdev->brightness = led_cdev->max_brightness
							>> 1;
			led_set_brightness(led_cdev, period ? led_cdev->brightness : 0);
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

static int led_pwm_set(struct led_classdev *led_cdev,
		       enum led_brightness brightness)
{
	struct led_pwm_data *led_dat =
		container_of(led_cdev, struct led_pwm_data, cdev);
	unsigned int max = led_dat->cdev.max_brightness;
	unsigned long long duty = led_dat->pwmstate.period;

	duty *= brightness;
	do_div(duty, max);

	if (led_dat->active_low)
		duty = led_dat->pwmstate.period - duty;

	led_dat->pwmstate.duty_cycle = duty;
	led_dat->pwmstate.enabled = duty > 0;
	return pwm_apply_state(led_dat->pwm, &led_dat->pwmstate);
}

static void led_pwm_cleanup(struct led_pwm_priv *priv)
{
	while (priv->num_leds--) {
		struct led_pwm_data *led_dat = &priv->leds[priv->num_leds];

		device_remove_file(led_dat->cdev.dev, &dev_attr_note);
		device_remove_file(led_dat->cdev.dev, &dev_attr_frequency);
		device_remove_file(led_dat->cdev.dev, &dev_attr_period);
	}
}

__attribute__((nonnull))
static int led_pwm_add(struct device *dev, struct led_pwm_priv *priv,
		       struct led_pwm *led, struct fwnode_handle *fwnode)
{
	struct led_pwm_data *led_data = &priv->leds[priv->num_leds];
	struct led_init_data init_data = { .fwnode = fwnode };
	int ret;

	led_data->active_low = led->active_low;
	led_data->cdev.name = led->name;
	led_data->cdev.brightness = LED_OFF;
	led_data->cdev.max_brightness = led->max_brightness;
	led_data->cdev.flags = LED_CORE_SUSPENDRESUME;
	led_data->octave = 4;

	led_data->pwm = devm_fwnode_pwm_get(dev, fwnode, NULL);
	if (IS_ERR(led_data->pwm))
		return dev_err_probe(dev, PTR_ERR(led_data->pwm),
				     "unable to request PWM for %s\n",
				     led->name);
	led_data->pwmstate.period = led_data->pwm->state.period;

	led_data->cdev.brightness_set_blocking = led_pwm_set;

	/* init PWM state */
	switch (led->default_state) {
	case LEDS_DEFSTATE_KEEP:
		pwm_get_state(led_data->pwm, &led_data->pwmstate);
		if (led_data->pwmstate.period)
			break;
		led->default_state = LEDS_DEFSTATE_OFF;
		dev_warn(dev,
			"failed to read period for %s, default to off",
			led->name);
		fallthrough;
	default:
		pwm_init_state(led_data->pwm, &led_data->pwmstate);
		break;
	}

	/* set brightness */
	switch (led->default_state) {
	case LEDS_DEFSTATE_ON:
		led_data->cdev.brightness = led->max_brightness;
		break;
	case LEDS_DEFSTATE_KEEP:
		{
		uint64_t brightness;

		brightness = led->max_brightness;
		brightness *= led_data->pwmstate.duty_cycle;
		do_div(brightness, led_data->pwmstate.period);
		led_data->cdev.brightness = brightness;
		}
		break;
	}

	ret = devm_led_classdev_register_ext(dev, &led_data->cdev, &init_data);
	if (ret) {
		dev_err(dev, "failed to register PWM led for %s: %d\n",
			led->name, ret);
		return ret;
	}
	/* register the attributes */
	ret = device_create_file(led_data->cdev.dev,
				 &dev_attr_frequency);
	if (ret)
		return ret;
	ret = device_create_file(led_data->cdev.dev, &dev_attr_period);
	if (ret)
		return ret;
	ret = device_create_file(led_data->cdev.dev, &dev_attr_note);
	if (ret)
		return ret;

	if (led->default_state != LEDS_DEFSTATE_KEEP) {
		ret = led_pwm_set(&led_data->cdev, led_data->cdev.brightness);
		if (ret) {
			dev_err(dev, "failed to set led PWM value for %s: %d",
				led->name, ret);
			return ret;
		}
	}

	priv->num_leds++;
	return 0;
}

static int led_pwm_create_fwnode(struct device *dev, struct led_pwm_priv *priv)
{
	struct fwnode_handle *fwnode;
	struct led_pwm led;
	int ret;

	memset(&led, 0, sizeof(led));

	device_for_each_child_node(dev, fwnode) {
		ret = fwnode_property_read_string(fwnode, "label", &led.name);
		if (ret && is_of_node(fwnode))
			led.name = to_of_node(fwnode)->name;

		if (!led.name) {
			ret = EINVAL;
			goto err_child_out;
		}

		led.active_low = fwnode_property_read_bool(fwnode,
							   "active-low");
		fwnode_property_read_u32(fwnode, "max-brightness",
					 &led.max_brightness);

		led.default_state = led_init_default_state_get(fwnode);

		ret = led_pwm_add(dev, priv, &led, fwnode);
		if (ret)
			goto err_child_out;
	}

	return 0;

err_child_out:
	fwnode_handle_put(fwnode);
	return ret;
}

static int led_pwm_probe(struct platform_device *pdev)
{
	struct led_pwm_priv *priv;
	int ret = 0;
	int count;

	count = device_get_child_node_count(&pdev->dev);

	if (!count)
		return -EINVAL;

	priv = devm_kzalloc(&pdev->dev, struct_size(priv, leds, count),
			    GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	ret = led_pwm_create_fwnode(&pdev->dev, priv);

	if (ret) {
		led_pwm_cleanup(priv);
		return ret;
	}

	platform_set_drvdata(pdev, priv);

	return 0;
}

static int led_pwm_remove(struct platform_device *pdev)
{
	struct led_pwm_priv *priv = platform_get_drvdata(pdev);

	led_pwm_cleanup(priv);

	return 0;
}

static const struct of_device_id of_pwm_leds_match[] = {
	{ .compatible = "pwm-leds", },
	{},
};
MODULE_DEVICE_TABLE(of, of_pwm_leds_match);

static struct platform_driver led_pwm_driver = {
	.probe		= led_pwm_probe,
	.remove		= led_pwm_remove,
	.driver		= {
		.name	= "leds_pwm",
		.of_match_table = of_pwm_leds_match,
	},
};

module_platform_driver(led_pwm_driver);

MODULE_AUTHOR("Luotao Fu <l.fu@pengutronix.de>");
MODULE_DESCRIPTION("generic PWM LED driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:leds-pwm");
