/*
 *   Boundary Devices FTx06 touch screen controller.
 *
 *   Copyright (c) by Boundary Devices <info@boundarydevices.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/fb.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/wait.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <linux/input/mt.h>
#include <linux/regulator/consumer.h>


struct dummy_i2c_device {
	struct clk *clk;
	struct regulator *c1;
	struct regulator *c2;
	struct gpio_descs *reset_gpios;
	struct gpio_descs *enable_gpios;
};

static const char reset_gpios[] = "reset";
static const char enable_gpios[] = "enable";

static void di2cd_set_gpios(struct gpio_descs *d, int active)
{
	int i;

	for (i = 0; i < d->ndescs; i++)
		gpiod_set_value(d->desc[i], active);
}

static struct gpio_descs* di2cd_setup_gpios(struct device *dev, const char* name, int active)
{
	struct gpio_descs *d;
	int i;

	d = devm_gpiod_get_array_optional(dev, name, active ? GPIOD_OUT_HIGH : GPIOD_OUT_LOW);

	if (IS_ERR(d))
		return d;
	if (!d)
		return d;
	for (i = 0; i < d->ndescs; i++) {
		struct gpio_desc *gd = d->desc[i];

		dev_info(dev, "%s: %d active %s\n", name, desc_to_gpio(gd), gpiod_is_active_low(gd) ? "low" : "high");
	}
	return d;
}

static int di2cd_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct clk *clk;
	struct dummy_i2c_device *d;
	struct device *dev = &client->dev;
	struct regulator *c;
	int ret;

	d = devm_kzalloc(dev, sizeof(*d), GFP_KERNEL);
	if (!d) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	clk = devm_clk_get(dev, NULL);
	if (IS_ERR(clk)) {
		/* assuming clock enabled by default */
		if (PTR_ERR(clk) != -EPROBE_DEFER)
			dev_err(dev, "clk missing(%ld)\n", PTR_ERR(clk));
		return PTR_ERR(clk);
	}
	d->clk = clk;

	c = devm_regulator_get(dev, "c1");
	if (IS_ERR(c)) {
		if (PTR_ERR(c) == -EPROBE_DEFER)
			return PTR_ERR(c);
		pr_err("%s: c1 devm_regulator_get failed(%ld), ignoring\n", __func__, PTR_ERR(c));
		c = NULL;
	}
	d->c1 = c;

	c = devm_regulator_get(dev, "c2");
	if (IS_ERR(c)) {
		if (PTR_ERR(c) == -EPROBE_DEFER)
			return PTR_ERR(c);
		pr_err("%s: c2 devm_regulator_get failed(%ld), ignoring\n", __func__, PTR_ERR(c));
		c = NULL;
	}
	d->c2 = c;
	i2c_set_clientdata(client, d);

	clk_prepare_enable(clk);
	pr_info("%s: enabled\n", __func__);
	if (d->c1) {
		ret = regulator_enable(d->c1);
		if (ret) {
			pr_err("%s:c1 enable error\n", __func__);
			return ret;
		}
	}

	if (d->c2) {
		ret = regulator_enable(d->c2);
		if (ret) {
			pr_err("%s:c2 enable error\n", __func__);
			return ret;
		}
	}
	d->reset_gpios = di2cd_setup_gpios(dev, reset_gpios, 0);
	d->enable_gpios = di2cd_setup_gpios(dev, enable_gpios, 1);

	return 0;
}

static int di2cd_remove(struct i2c_client *client)
{
	struct dummy_i2c_device *d = i2c_get_clientdata(client);

	di2cd_set_gpios(d->reset_gpios, 1);
	di2cd_set_gpios(d->enable_gpios, 0);

	clk_disable_unprepare(d->clk);
	if (d->c1)
		regulator_disable(d->c1);
	if (d->c2)
		regulator_disable(d->c2);
	return 0;
}

static const struct i2c_device_id di2cd_idtable[] = {
	{ "dummy_i2c_device", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, di2cd_idtable);

static const struct of_device_id di2cd_dt_ids[] = {
       {
               .compatible = "dummy_i2c_device",
       }, {
               /* sentinel */
       }
};
MODULE_DEVICE_TABLE(of, di2cd_dt_ids);

static struct i2c_driver di2cd_driver = {
	.driver = {
		.owner		= THIS_MODULE,
		.name		= "dummy_i2c_device",
		.of_match_table = di2cd_dt_ids,
	},
	.id_table	= di2cd_idtable,
	.probe		= di2cd_probe,
	.remove		= di2cd_remove,
};

module_i2c_driver(di2cd_driver);

MODULE_AUTHOR("Boundary Devices <info@boundarydevices.com>");
MODULE_DESCRIPTION("I2C interface dummy device.");
MODULE_LICENSE("GPL");
