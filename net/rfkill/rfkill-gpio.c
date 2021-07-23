// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2011, NVIDIA Corporation.
 */

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/regulator/consumer.h>
#include <linux/rfkill.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/acpi.h>
#include <linux/gpio/consumer.h>

struct rfkill_gpio_data {
	const char		*name;
	struct device		*dev;
	enum rfkill_type	type;
	struct regulator	*vdd;
	struct gpio_desc	*reset_gpio;
	struct gpio_desc	*shutdown_gpio;
	struct gpio_desc	*pulse_on_gpio;
	unsigned		pulse_duration;
	struct gpio_desc	*power_key_gpio;
	unsigned		power_key_low_off;
	unsigned		power_key_low_on;

	struct rfkill		*rfkill_dev;
	struct clk		*clk;
	struct pinctrl		*pinctrl;
	struct pinctrl_state	*pins_off;
	struct pinctrl_state	*pins_on;

	bool			clk_enabled;
};

static int rfkill_gpio_set_power(void *data, bool blocked)
{
	struct rfkill_gpio_data *rfkill = data;
	int ret;

	if (blocked) {
		if (rfkill->pinctrl)
			pinctrl_select_state(rfkill->pinctrl, rfkill->pins_off);
		if (rfkill->power_key_gpio) {
			gpiod_set_value_cansleep(rfkill->power_key_gpio, 0);
			msleep(rfkill->power_key_low_off);
			gpiod_set_value_cansleep(rfkill->power_key_gpio, 1);
			msleep(50);	/* allow graceful shutdown */
		}
		gpiod_set_value_cansleep(rfkill->shutdown_gpio, 1);
		gpiod_set_value_cansleep(rfkill->reset_gpio, 1);
		if (!IS_ERR(rfkill->clk) && rfkill->clk_enabled)
			clk_disable_unprepare(rfkill->clk);
		if (rfkill->vdd)
			regulator_disable(rfkill->vdd);
	} else {
		if (rfkill->power_key_gpio)
			gpiod_set_value_cansleep(rfkill->power_key_gpio, 0);
		if (rfkill->vdd) {
			ret = regulator_enable(rfkill->vdd);
			if (ret) {
				dev_err(rfkill->dev,
					"Failed to enable vdd regulator: %d\n",
					ret);
			}
		}
		if (!IS_ERR(rfkill->clk) && !rfkill->clk_enabled)
			clk_prepare_enable(rfkill->clk);
		gpiod_set_value_cansleep(rfkill->reset_gpio, 0);
		gpiod_set_value_cansleep(rfkill->shutdown_gpio, 0);
		if (rfkill->power_key_gpio) {
			gpiod_set_value_cansleep(rfkill->power_key_gpio, 1);
			msleep(2);
			gpiod_set_value_cansleep(rfkill->power_key_gpio, 0);
			msleep(rfkill->power_key_low_on);
			gpiod_set_value_cansleep(rfkill->power_key_gpio, 1);
		}
		if (rfkill->pulse_on_gpio) {
			gpiod_set_value_cansleep(rfkill->pulse_on_gpio, 1);
			msleep(rfkill->pulse_duration);
			pr_info("%s:msleep %d\n", __func__, rfkill->pulse_duration);
			gpiod_set_value_cansleep(rfkill->pulse_on_gpio, 0);
		}
		if (rfkill->pinctrl)
			pinctrl_select_state(rfkill->pinctrl, rfkill->pins_on);
	}

	rfkill->clk_enabled = !blocked;
	return 0;
}

static const struct rfkill_ops rfkill_gpio_ops = {
	.set_block = rfkill_gpio_set_power,
};

static const struct acpi_gpio_params reset_gpios = { 0, 0, false };
static const struct acpi_gpio_params shutdown_gpios = { 1, 0, false };

static const struct acpi_gpio_mapping acpi_rfkill_default_gpios[] = {
	{ "reset-gpios", &reset_gpios, 1 },
	{ "shutdown-gpios", &shutdown_gpios, 1 },
	{ },
};

static int rfkill_gpio_acpi_probe(struct device *dev,
				  struct rfkill_gpio_data *rfkill)
{
	const struct acpi_device_id *id;

	id = acpi_match_device(dev->driver->acpi_match_table, dev);
	if (!id)
		return -ENODEV;

	rfkill->type = (unsigned)id->driver_data;

	return devm_acpi_dev_add_driver_gpios(dev, acpi_rfkill_default_gpios);
}

static int rfkill_gpio_get_pdata_from_of(struct device *dev,
		struct rfkill_gpio_data *rfkill)
{
	struct device_node *np = dev->of_node;

	if (!np) {
		np = of_find_matching_node(NULL, dev->driver->of_match_table);
		if (!np) {
			dev_notice(dev, "device tree node not available\n");
			return -ENODEV;
		}
	}
	of_property_read_u32(np, "type", &rfkill->type);
	of_property_read_string(np, "name", &rfkill->name);
	return 0;
}

static int rfkill_gpio_probe(struct platform_device *pdev)
{
	struct rfkill_gpio_data *rfkill;
	struct gpio_desc *gpio;
	struct gpio_descs *descs;
	const char *type_name;
	struct device *dev = &pdev->dev;
	int ret;

	rfkill = devm_kzalloc(dev, sizeof(*rfkill), GFP_KERNEL);
	if (!rfkill)
		return -ENOMEM;

	rfkill->dev = dev;
	device_property_read_string(dev, "name", &rfkill->name);
	device_property_read_string(dev, "type", &type_name);

	if (!rfkill->name)
		rfkill->name = dev_name(dev);

	rfkill->type = rfkill_find_type(type_name);

	if (ACPI_HANDLE(dev)) {
		ret = rfkill_gpio_acpi_probe(dev, rfkill);
		if (ret)
			return ret;
	} else {
		ret = rfkill_gpio_get_pdata_from_of(dev, rfkill);
		if (ret) {
			dev_err(dev, "no platform data\n");
			return ret;
		}
	}

	rfkill->vdd = devm_regulator_get_optional(dev, "vdd");
	if (IS_ERR(rfkill->vdd)) {
		ret = PTR_ERR(rfkill->vdd);
		if (ret == -ENODEV) {
			rfkill->vdd = NULL;
		} else {
			if (ret != -EPROBE_DEFER)
				dev_err(dev, "Failed to get vdd regulator: %d\n", ret);
			return ret;
		}
	}

	rfkill->clk = devm_clk_get(dev, NULL);

	gpio = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(gpio))
		return PTR_ERR(gpio);

	rfkill->reset_gpio = gpio;

	gpio = devm_gpiod_get_optional(dev, "shutdown", GPIOD_OUT_HIGH);
	if (IS_ERR(gpio))
		return PTR_ERR(gpio);

	rfkill->shutdown_gpio = gpio;

	gpio = devm_gpiod_get_optional(dev, "pulse-on", GPIOD_OUT_LOW);
	if (IS_ERR(gpio))
		return PTR_ERR(gpio);
	rfkill->pulse_on_gpio = gpio;

	ret = of_property_read_u32(dev->of_node, "pulse-duration",
			&rfkill->pulse_duration);

	gpio = devm_gpiod_get_optional(dev, "power-key", GPIOD_OUT_LOW);
	if (IS_ERR(gpio))
		return PTR_ERR(gpio);
	rfkill->power_key_gpio = gpio;

	ret = of_property_read_u32(dev->of_node, "power-key-low-on",
			&rfkill->power_key_low_on);
	ret = of_property_read_u32(dev->of_node, "power-key-low-off",
			&rfkill->power_key_low_off);

	/* Make sure at-least one GPIO is defined for this instance */
	if (!rfkill->reset_gpio && !rfkill->shutdown_gpio && !rfkill->vdd) {
		dev_err(dev, "invalid platform data\n");
		return -EINVAL;
	}

	descs = devm_gpiod_get_array_optional(dev, "low", GPIOD_OUT_LOW);
	if (IS_ERR(descs))
		return PTR_ERR(descs);
	descs = devm_gpiod_get_array_optional(dev, "high", GPIOD_OUT_HIGH);
	if (IS_ERR(descs))
		return PTR_ERR(descs);

	rfkill->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(rfkill->pinctrl))
		rfkill->pinctrl = NULL;

	if (rfkill->pinctrl) {
		struct pinctrl_state *pins;

		pins = pinctrl_lookup_state(rfkill->pinctrl, "off");
		if (!IS_ERR(pins))
			rfkill->pins_off = pins;
		pins = pinctrl_lookup_state(rfkill->pinctrl, "on");
		if (!IS_ERR(pins))
			rfkill->pins_on = pins;
		if (rfkill->pins_off && rfkill->pins_on)
			pinctrl_select_state(rfkill->pinctrl, rfkill->pins_off);
		else
			rfkill->pinctrl = NULL;
	}

	rfkill->rfkill_dev = rfkill_alloc(rfkill->name, dev,
					  rfkill->type, &rfkill_gpio_ops,
					  rfkill);
	if (!rfkill->rfkill_dev)
		return -ENOMEM;

	ret = rfkill_register(rfkill->rfkill_dev);
	if (ret < 0)
		goto err_destroy;

	platform_set_drvdata(pdev, rfkill);

	dev_info(dev, "%s device registered.\n", rfkill->name);

	return 0;

err_destroy:
	rfkill_destroy(rfkill->rfkill_dev);

	return ret;
}

static int rfkill_gpio_remove(struct platform_device *pdev)
{
	struct rfkill_gpio_data *rfkill = platform_get_drvdata(pdev);

	rfkill_unregister(rfkill->rfkill_dev);
	rfkill_destroy(rfkill->rfkill_dev);

	return 0;
}

#ifdef CONFIG_ACPI
static const struct acpi_device_id rfkill_acpi_match[] = {
	{ "BCM4752", RFKILL_TYPE_GPS },
	{ "LNV4752", RFKILL_TYPE_GPS },
	{ },
};
MODULE_DEVICE_TABLE(acpi, rfkill_acpi_match);
#endif

static const struct of_device_id rfkill_gpio_of_match_table[] = {
	{ .compatible = "net,rfkill-gpio" },
	{ }
};
MODULE_DEVICE_TABLE(of, rfkill_gpio_of_match_table);

static struct platform_driver rfkill_gpio_driver = {
	.probe = rfkill_gpio_probe,
	.remove = rfkill_gpio_remove,
	.driver = {
		.name = "rfkill_gpio",
		.acpi_match_table = ACPI_PTR(rfkill_acpi_match),
		.of_match_table = of_match_ptr(rfkill_gpio_of_match_table),
	},
};

module_platform_driver(rfkill_gpio_driver);

MODULE_DESCRIPTION("gpio rfkill");
MODULE_AUTHOR("NVIDIA");
MODULE_LICENSE("GPL");
