/*
 * linux/drivers/video/backlight/pwm_bl.c
 *
 * simple PWM based backlight control, board code has to setup
 * 1) pin configuration so PWM waveforms can output
 * 2) platform_data being correctly configured
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/pwm.h>
#include <linux/pwm_backlight.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>

struct pwm_bl_data {
	struct pwm_device	*pwm;
	struct device		*dev;
	unsigned int		period;
	unsigned int		lth_brightness;
	unsigned int		*levels;
	bool			enabled;
	struct regulator	*power_supply;
	struct gpio_descs	*enable_gpios;
	struct gpio_descs	legacy_enable;
	struct gpio_desc	*legacy_desc;	/* part of above struct gpio_descs */
	unsigned int		scale;
	bool			legacy;
	int			on_delay;
	int			disp_cnt;
	struct device_node	*disp_node[4];
	int			(*notify)(struct device *,
					  int brightness);
	void			(*notify_after)(struct device *,
					int brightness);
	int			(*check_fb)(struct device *, struct fb_info *);
	void			(*exit)(struct device *);
};

static void set_gpios(struct gpio_descs *d, int active)
{
	int i;

	if (active) {
		for (i = 0; i < d->ndescs; i++) {
			if (i)
				msleep(10);
			gpiod_set_value_cansleep(d->desc[i], active);
		}
	} else {
		for (i = d->ndescs - 1; i >= 0 ; i--) {
			gpiod_set_value_cansleep(d->desc[i], active);
			if (i)
				msleep(10);
		}
	}
}

static void pwm_backlight_power_on(struct pwm_bl_data *pb, int brightness)
{
	int err;

	if (pb->enabled)
		return;

	if (pb->on_delay)
		msleep(pb->on_delay);
	err = regulator_enable(pb->power_supply);
	if (err < 0)
		dev_err(pb->dev, "failed to enable power supply\n");

	if (pb->enable_gpios)
		set_gpios(pb->enable_gpios, 1);

	pwm_enable(pb->pwm);
	pb->enabled = true;
}

static void pwm_backlight_power_off(struct pwm_bl_data *pb)
{
	if (!pb->enabled)
		return;

	pwm_config(pb->pwm, 0, pb->period);
	pwm_disable(pb->pwm);

	if (pb->enable_gpios)
		set_gpios(pb->enable_gpios, 0);

	regulator_disable(pb->power_supply);
	pb->enabled = false;
}

static int compute_duty_cycle(struct pwm_bl_data *pb, int brightness)
{
	unsigned int lth = pb->lth_brightness;
	u64 duty_cycle;

	if (pb->levels)
		duty_cycle = pb->levels[brightness];
	else
		duty_cycle = brightness;

	duty_cycle *= pb->period - lth;
	do_div(duty_cycle, pb->scale);

	return duty_cycle + lth;
}

static int pwm_backlight_update_status(struct backlight_device *bl)
{
	struct pwm_bl_data *pb = bl_get_data(bl);
	int brightness = bl->props.brightness;
	int duty_cycle;

	if (bl->props.power != FB_BLANK_UNBLANK ||
	    bl->props.fb_blank != FB_BLANK_UNBLANK ||
	    bl->props.state & BL_CORE_FBBLANK)
		brightness = 0;

	if (pb->notify)
		brightness = pb->notify(pb->dev, brightness);

	duty_cycle = compute_duty_cycle(pb, brightness);
	if (duty_cycle > 0) {
		pwm_config(pb->pwm, duty_cycle, pb->period);
		pwm_backlight_power_on(pb, brightness);
	} else
		pwm_backlight_power_off(pb);

	if (pb->notify_after)
		pb->notify_after(pb->dev, brightness);

	return 0;
}

static int pwm_backlight_check_fb(struct backlight_device *bl,
				  struct fb_info *info)
{
	struct pwm_bl_data *pb = bl_get_data(bl);

	if (pb->disp_cnt) {
		struct device_node *np = info->device->of_node;
		int i;

		for (i = 0 ; i < pb->disp_cnt; i++) {
			if (np == pb->disp_node[i]) {
				bl->props.power = FB_BLANK_UNBLANK;
				return 1;
			}
		}
		return 0;
	}
	return !pb->check_fb || pb->check_fb(pb->dev, info);
}

static const struct backlight_ops pwm_backlight_ops = {
	.update_status	= pwm_backlight_update_status,
	.check_fb	= pwm_backlight_check_fb,
};

#ifdef CONFIG_OF
static int pwm_backlight_parse_dt(struct device *dev,
				  struct platform_pwm_backlight_data *data,
				  struct pwm_bl_data *pb)
{
	struct device_node *node = dev->of_node;
	struct property *prop;
	int length;
	u32 value;
	int ret;
	int i;

	if (!node)
		return -ENODEV;

	memset(data, 0, sizeof(*data));

	/* determine the number of brightness levels */
	prop = of_find_property(node, "brightness-levels", &length);
	if (!prop)
		return -EINVAL;

	data->max_brightness = length / sizeof(u32);

	/* read brightness levels from DT property */
	if (data->max_brightness > 0) {
		size_t size = sizeof(*data->levels) * data->max_brightness;

		data->levels = devm_kzalloc(dev, size, GFP_KERNEL);
		if (!data->levels)
			return -ENOMEM;

		ret = of_property_read_u32_array(node, "brightness-levels",
						 data->levels,
						 data->max_brightness);
		if (ret < 0)
			return ret;

		ret = of_property_read_u32(node, "default-brightness-level",
					   &value);
		if (ret < 0)
			return ret;

		data->dft_brightness = value;
		data->max_brightness--;
	}

	ret = of_property_read_u32(node, "on-delay", &value);
	if (!ret)
		pb->on_delay = value;

	data->enable_gpio = -EINVAL;
	for (i = 0 ; i < ARRAY_SIZE(pb->disp_node); i++) {
		pb->disp_node[i] = of_parse_phandle(node, "display", i);
		if (!pb->disp_node[i])
			break;
	}
	pb->disp_cnt = i;
	return 0;
}

static const struct of_device_id pwm_backlight_of_match[] = {
	{ .compatible = "pwm-backlight" },
	{ }
};

MODULE_DEVICE_TABLE(of, pwm_backlight_of_match);
#else
static int pwm_backlight_parse_dt(struct device *dev,
				  struct platform_pwm_backlight_data *data,
				  struct pwm_bl_data *pb)
{
	return -ENODEV;
}
#endif

static int pwm_backlight_initial_power_state(const struct pwm_bl_data *pb)
{
	struct device_node *node = pb->dev->of_node;

#if defined(CONFIG_FB_MXC_LCDIF) || defined(CONFIG_FB_MXC_SYNC_PANEL)
	if (pb->disp_cnt) {
		int i;
		struct device_node *np;
		struct fb_info *fbi;

		for (i = 0 ; i < pb->disp_cnt; i++) {
			np = pb->disp_node[i];
			fbi = find_registered_fb_from_dt(np);
			if (fbi)
				return FB_BLANK_UNBLANK;
		}
		return FB_BLANK_POWERDOWN;
	}
#endif

	/* Not booted with device tree or no phandle link to the node */
	if (!node || !node->phandle)
		return FB_BLANK_UNBLANK;

	/* The regulator is disabled, do not enable the backlight */
	if (!regulator_is_enabled(pb->power_supply))
		return FB_BLANK_POWERDOWN;
#if 0
	/* The PWM is disabled, keep it like this */
	if (!pwm_is_enabled(pb->pwm))
		return FB_BLANK_POWERDOWN;
#endif
	return FB_BLANK_UNBLANK;
}

static int pwm_backlight_probe(struct platform_device *pdev)
{
	struct platform_pwm_backlight_data *data = dev_get_platdata(&pdev->dev);
	struct platform_pwm_backlight_data defdata;
	struct backlight_properties props;
	struct backlight_device *bl;
	struct device_node *node = pdev->dev.of_node;
	struct pwm_bl_data *pb;
	struct pwm_args pargs;
	int ret;

	pb = devm_kzalloc(&pdev->dev, sizeof(*pb), GFP_KERNEL);
	if (!pb)
		return -ENOMEM;

	if (!data) {
		ret = pwm_backlight_parse_dt(&pdev->dev, &defdata, pb);
		if (ret < 0) {
			dev_err(&pdev->dev, "failed to find platform data\n");
			return ret;
		}

		data = &defdata;
	}

	if (data->init) {
		ret = data->init(&pdev->dev);
		if (ret < 0)
			return ret;
	}

	if (data->levels) {
		unsigned int i;

		for (i = 0; i <= data->max_brightness; i++)
			if (data->levels[i] > pb->scale)
				pb->scale = data->levels[i];

		pb->levels = data->levels;
	} else
		pb->scale = data->max_brightness;

	pb->notify = data->notify;
	pb->notify_after = data->notify_after;
	pb->check_fb = data->check_fb;
	pb->exit = data->exit;
	pb->dev = &pdev->dev;
	pb->enabled = false;

	pb->enable_gpios = devm_gpiod_get_array_optional(&pdev->dev, "enable",
							 GPIOD_OUT_LOW);
	if (IS_ERR(pb->enable_gpios)) {
		ret = PTR_ERR(pb->enable_gpios);
		goto err_alloc;
	}

	/*
	 * Compatibility fallback for drivers still using the integer GPIO
	 * platform data. Must go away soon.
	 */
	if (!pb->enable_gpios && gpio_is_valid(data->enable_gpio)) {
		ret = devm_gpio_request_one(&pdev->dev, data->enable_gpio,
					    GPIOF_OUT_INIT_HIGH, "enable");
		if (ret < 0) {
			dev_err(&pdev->dev, "failed to request GPIO#%d: %d\n",
				data->enable_gpio, ret);
			goto err_alloc;
		}

		pb->legacy_enable.desc[0] = gpio_to_desc(data->enable_gpio);
		pb->legacy_enable.ndescs = 1;
		pb->enable_gpios = &pb->legacy_enable;
		gpiod_direction_output(pb->legacy_enable.desc[0], 1);
	}

	pb->power_supply = devm_regulator_get(&pdev->dev, "power");
	if (IS_ERR(pb->power_supply)) {
		ret = PTR_ERR(pb->power_supply);
		goto err_alloc;
	}

	pb->pwm = devm_pwm_get(&pdev->dev, NULL);
	if (IS_ERR(pb->pwm) && PTR_ERR(pb->pwm) != -EPROBE_DEFER && !node) {
		dev_err(&pdev->dev, "unable to request PWM, trying legacy API\n");
		pb->legacy = true;
		pb->pwm = pwm_request(data->pwm_id, "pwm-backlight");
	}

	if (IS_ERR(pb->pwm)) {
		ret = PTR_ERR(pb->pwm);
		if (ret != -EPROBE_DEFER)
			dev_err(&pdev->dev, "unable to request PWM\n");
		goto err_alloc;
	}

	dev_dbg(&pdev->dev, "got pwm for backlight\n");

	/*
	 * FIXME: pwm_apply_args() should be removed when switching to
	 * the atomic PWM API.
	 */
	pwm_apply_args(pb->pwm);

	/*
	 * The DT case will set the pwm_period_ns field to 0 and store the
	 * period, parsed from the DT, in the PWM device. For the non-DT case,
	 * set the period from platform data if it has not already been set
	 * via the PWM lookup table.
	 */
	pwm_get_args(pb->pwm, &pargs);
	pb->period = pargs.period;
	if (!pb->period && (data->pwm_period_ns > 0))
		pb->period = data->pwm_period_ns;

	pb->lth_brightness = data->lth_brightness * (pb->period / pb->scale);

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;
	props.max_brightness = data->max_brightness;
	bl = backlight_device_register(dev_name(&pdev->dev), &pdev->dev, pb,
				       &pwm_backlight_ops, &props);
	if (IS_ERR(bl)) {
		dev_err(&pdev->dev, "failed to register backlight\n");
		ret = PTR_ERR(bl);
		if (pb->legacy)
			pwm_free(pb->pwm);
		goto err_alloc;
	}

	if (data->dft_brightness > data->max_brightness) {
		dev_warn(&pdev->dev,
			 "invalid default brightness level: %u, using %u\n",
			 data->dft_brightness, data->max_brightness);
		data->dft_brightness = data->max_brightness;
	}

	bl->props.brightness = data->dft_brightness;
	bl->props.power = pwm_backlight_initial_power_state(pb);
	backlight_update_status(bl);

	platform_set_drvdata(pdev, bl);
	return 0;

err_alloc:
	if (data->exit)
		data->exit(&pdev->dev);
	return ret;
}

static int pwm_backlight_remove(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct pwm_bl_data *pb = bl_get_data(bl);

	backlight_device_unregister(bl);
	pwm_backlight_power_off(pb);

	if (pb->exit)
		pb->exit(&pdev->dev);
	if (pb->legacy)
		pwm_free(pb->pwm);

	return 0;
}

static void pwm_backlight_shutdown(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct pwm_bl_data *pb = bl_get_data(bl);

	pwm_backlight_power_off(pb);
}

#ifdef CONFIG_PM_SLEEP
static int pwm_backlight_suspend(struct device *dev)
{
	struct backlight_device *bl = dev_get_drvdata(dev);
	struct pwm_bl_data *pb = bl_get_data(bl);

	if (pb->notify)
		pb->notify(pb->dev, 0);

	pwm_backlight_power_off(pb);

	if (pb->notify_after)
		pb->notify_after(pb->dev, 0);

	return 0;
}

static int pwm_backlight_resume(struct device *dev)
{
	struct backlight_device *bl = dev_get_drvdata(dev);

	backlight_update_status(bl);

	return 0;
}
#endif

static const struct dev_pm_ops pwm_backlight_pm_ops = {
#ifdef CONFIG_PM_SLEEP
	.suspend = pwm_backlight_suspend,
	.resume = pwm_backlight_resume,
	.poweroff = pwm_backlight_suspend,
	.restore = pwm_backlight_resume,
#endif
};

static struct platform_driver pwm_backlight_driver = {
	.driver		= {
		.name		= "pwm-backlight",
		.pm		= &pwm_backlight_pm_ops,
		.of_match_table	= of_match_ptr(pwm_backlight_of_match),
	},
	.probe		= pwm_backlight_probe,
	.remove		= pwm_backlight_remove,
	.shutdown	= pwm_backlight_shutdown,
};

module_platform_driver(pwm_backlight_driver);

MODULE_DESCRIPTION("PWM based Backlight Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:pwm-backlight");
