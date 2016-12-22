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
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>

struct platform_lp8860_backlight_data {
	unsigned int		max_brightness;
	unsigned int		dft_brightness;
	unsigned int		*levels;
	struct gpio_desc	*reset_gpio;
	int			(*init)(struct device *dev);
	int			(*notify)(struct device *,
					  int brightness);
	void			(*notify_after)(struct device *,
					int brightness);
	int			(*check_fb)(struct device *, struct fb_info *);
	void			(*exit)(struct device *);
};

struct lp8860_bl_data {
	struct device		*dev;
	struct i2c_client	*client;
	struct backlight_device *bl;
	int			setting;
	int			last_setting;
	unsigned int		*levels;
	bool			enabled;
	struct regulator	*power_supply;
	struct gpio_desc	*reset_gpio;
	unsigned int		scale;
	int			disp_cnt;
	struct device_node	*disp_node[4];
	int			(*notify)(struct device *,
					  int brightness);
	void			(*notify_after)(struct device *,
					int brightness);
	int			(*check_fb)(struct device *, struct fb_info *);
	void			(*exit)(struct device *);
};

static int write_reg(struct i2c_client *client, u8 regnum, u8 value )
{
	u8 buf[] = {
		regnum,
		value
	};
	struct i2c_msg msg = {
		client->addr, 0, sizeof(buf), buf
	};
	int ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret != 1)
		pr_warn("%s: i2c_transfer failed\n", __func__);
	else
		pr_debug("%s: set register 0x%02x to 0x%02x\n",
		       __func__, regnum, value);
	return (ret != 1) ? -EIO : 0;
}

static void lp8860_backlight_power_on(struct lp8860_bl_data *pb)
{
	int err;

	if (!pb->enabled) {
		err = regulator_enable(pb->power_supply);
		if (err < 0)
			dev_err(pb->dev, "failed to enable power supply\n");

		if (!IS_ERR(pb->reset_gpio)) {
			gpiod_set_value(pb->reset_gpio, 0);
			msleep(61);
		}
		pb->enabled = true;
	}
	if (pb->setting != pb->last_setting) {
		pr_debug("%s:setting=0x%x\n", __func__, pb->setting);
		write_reg(pb->client, 0, (u8)pb->setting);
		write_reg(pb->client, 1, (u8)(pb->setting >> 8));
		pb->setting = pb->last_setting;
	}
}

static int lp8860_detect(struct i2c_client *client, struct gpio_desc *reset_gpio)
{
	int ret;

	if (!IS_ERR(reset_gpio)) {
		gpiod_set_value(reset_gpio, 0);
		msleep(61);
	}
	ret = write_reg(client, 0, 0);
	if (!ret)
		ret = write_reg(client, 1, 0);
	if (!IS_ERR(reset_gpio)) {
		gpiod_set_value(reset_gpio, 1);
	}
	return ret;
}

static void lp8860_backlight_power_off(struct lp8860_bl_data *pb)
{
	if (!pb->enabled)
		return;

	pb->last_setting = 0;
	if (!IS_ERR(pb->reset_gpio)) {
		gpiod_set_value(pb->reset_gpio, 1);
	} else {
		write_reg(pb->client, 0, 0);
		write_reg(pb->client, 1, 0);
	}
	regulator_disable(pb->power_supply);
	pb->enabled = false;
}

static int compute_setting(struct lp8860_bl_data *pb, int brightness)
{
	int setting;

	if (pb->levels)
		setting = pb->levels[brightness];
	else
		setting = brightness;

	return (setting * 0xffff) / pb->scale;
}

static int lp8860_backlight_update_status(struct backlight_device *bl)
{
	struct lp8860_bl_data *pb = bl_get_data(bl);
	int brightness = bl->props.brightness;

	if (bl->props.power != FB_BLANK_UNBLANK ||
	    bl->props.fb_blank != FB_BLANK_UNBLANK ||
	    bl->props.state & BL_CORE_FBBLANK)
		brightness = 0;

	if (pb->notify)
		brightness = pb->notify(pb->dev, brightness);

	pb->setting = compute_setting(pb, brightness);
	if (pb->setting > 0)
		lp8860_backlight_power_on(pb);
	else
		lp8860_backlight_power_off(pb);

	if (pb->notify_after)
		pb->notify_after(pb->dev, brightness);

	return 0;
}

static int lp8860_backlight_get_brightness(struct backlight_device *bl)
{
	return bl->props.brightness;
}

static int lp8860_backlight_check_fb(struct backlight_device *bl,
				  struct fb_info *info)
{
	struct lp8860_bl_data *pb = bl_get_data(bl);

	if (pb->disp_cnt) {
		struct device_node *np = info->device->of_node;
		int i;

		for (i = 0 ; i < pb->disp_cnt; i++) {
			if (np == pb->disp_node[i])
				return 1;
		}
		return 0;
	}
	return !pb->check_fb || pb->check_fb(pb->dev, info);
}

static const struct backlight_ops lp8860_backlight_ops = {
	.update_status	= lp8860_backlight_update_status,
	.get_brightness	= lp8860_backlight_get_brightness,
	.check_fb	= lp8860_backlight_check_fb,
};

#ifdef CONFIG_OF
static int lp8860_backlight_parse_dt(struct device *dev,
				  struct platform_lp8860_backlight_data *data,
				  struct lp8860_bl_data *pb)
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

	data->reset_gpio = devm_gpiod_get_index(dev, "reset", 0,
						GPIOD_OUT_HIGH);
	if (IS_ERR(data->reset_gpio)) {
		if (PTR_ERR(data->reset_gpio) == -EPROBE_DEFER)
		return -EPROBE_DEFER;
	}

	for (i = 0 ; i < ARRAY_SIZE(pb->disp_node); i++) {
		pb->disp_node[i] = of_parse_phandle(node, "display", i);
		if (!pb->disp_node[i])
			break;
	}
	pb->disp_cnt = i;
	return 0;
}

static struct of_device_id lp8860_backlight_of_match[] = {
	{ .compatible = "lp8860-backlight" },
	{ }
};

MODULE_DEVICE_TABLE(of, lp8860_backlight_of_match);
#else
static int lp8860_backlight_parse_dt(struct device *dev,
				  struct platform_lp8860_backlight_data *data,
				  struct lp8860_bl_data *pb)
{
	return -ENODEV;
}
#endif

static int lp8860_backlight_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct platform_lp8860_backlight_data *data = dev_get_platdata(dev);
	struct platform_lp8860_backlight_data defdata;
	struct backlight_properties props;
	struct backlight_device *bl;
	struct lp8860_bl_data *pb;
	int ret;

	pb = devm_kzalloc(dev, sizeof(*pb), GFP_KERNEL);
	if (!pb)
		return -ENOMEM;

	if (!data) {
		ret = lp8860_backlight_parse_dt(dev, &defdata, pb);
		if (ret < 0) {
			dev_err(dev, "failed to find platform data\n");
			return ret;
		}

		data = &defdata;
		ret = lp8860_detect(client, data->reset_gpio);
		if (ret) {
			dev_err(dev, "detect failed %d\n", ret);
			return ret;
		}
	}

	if (data->init) {
		ret = data->init(dev);
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

	pb->client = client;
	pb->reset_gpio = data->reset_gpio;
	pb->notify = data->notify;
	pb->notify_after = data->notify_after;
	pb->check_fb = data->check_fb;
	pb->exit = data->exit;
	pb->dev = dev;
	pb->enabled = false;

	pb->power_supply = devm_regulator_get(dev, "power");
	if (IS_ERR(pb->power_supply)) {
		ret = PTR_ERR(pb->power_supply);
		goto err_gpio;
	}

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;
	props.max_brightness = data->max_brightness;
	bl = backlight_device_register(dev_name(dev), dev, pb,
				       &lp8860_backlight_ops, &props);
	if (IS_ERR(bl)) {
		dev_err(dev, "failed to register backlight\n");
		ret = PTR_ERR(bl);
		goto err_gpio;
	}
	pb->bl = bl;

	if (data->dft_brightness > data->max_brightness) {
		dev_warn(dev,
			 "invalid default brightness level: %u, using %u\n",
			 data->dft_brightness, data->max_brightness);
		data->dft_brightness = data->max_brightness;
	}

	bl->props.brightness = data->dft_brightness;
	backlight_update_status(bl);
	i2c_set_clientdata(client, pb);
	return 0;

err_gpio:
	if (data->exit)
		data->exit(dev);
	return ret;
}

static int lp8860_backlight_remove(struct i2c_client *client)
{
	struct lp8860_bl_data *pb = i2c_get_clientdata(client);

	backlight_device_unregister(pb->bl);
	lp8860_backlight_power_off(pb);

	if (pb->exit)
		pb->exit(&client->dev);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int lp8860_backlight_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct lp8860_bl_data *pb = i2c_get_clientdata(client);

	if (pb->notify)
		pb->notify(pb->dev, 0);

	lp8860_backlight_power_off(pb);

	if (pb->notify_after)
		pb->notify_after(pb->dev, 0);
	return 0;
}

static int lp8860_backlight_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct lp8860_bl_data *pb = i2c_get_clientdata(client);

	backlight_update_status(pb->bl);
	return 0;
}
#endif

static const struct dev_pm_ops lp8860_backlight_pm_ops = {
#ifdef CONFIG_PM_SLEEP
	.suspend = lp8860_backlight_suspend,
	.resume = lp8860_backlight_resume,
	.poweroff = lp8860_backlight_suspend,
	.restore = lp8860_backlight_resume,
#endif
};

static const struct i2c_device_id lp8860_id[] = {
	{ "lp8860-backlight", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, lp8860_id);

static struct i2c_driver lp8860_backlight_driver = {
	.driver		= {
		.name		= "lp8860-backlight",
		.pm		= &lp8860_backlight_pm_ops,
		.of_match_table	= of_match_ptr(lp8860_backlight_of_match),
	},
	.probe		= lp8860_backlight_probe,
	.remove		= lp8860_backlight_remove,
	.id_table	= lp8860_id,
};

module_i2c_driver(lp8860_backlight_driver);

MODULE_DESCRIPTION("lp8860 based Backlight Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:lp8860-backlight");
