// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright 2021 NXP
 *
 * The driver exports a standard gpiochip interface
 */

#include <linux/init.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/gpio/driver.h>
#include <linux/platform_device.h>
#include <linux/firmware/imx/svc/rm.h>
#include <dt-bindings/firmware/imx/rsrc.h>

#define PIN_NUMBER 8

struct imxscfw {
	struct mutex	lock;
	struct imx_sc_ipc *handle;
	struct gpio_chip chip;
	struct device *dev;
};

static unsigned int sc_arr[] = {
	IMX_SC_R_BOARD_R0,
	IMX_SC_R_BOARD_R1,
	IMX_SC_R_BOARD_R2,
	IMX_SC_R_BOARD_R3,
	IMX_SC_R_BOARD_R4,
	IMX_SC_R_BOARD_R5,
	IMX_SC_R_BOARD_R6,  //R6 is MII select
	IMX_SC_R_BOARD_R7,
};

static int imxscfw_get(struct gpio_chip *chip, unsigned int offset)
{
	struct imxscfw *scu = gpiochip_get_data(chip);
	int err = -EINVAL, level = 0;

	if (offset >= sizeof(sc_arr)/sizeof(unsigned int))
		return err;

	mutex_lock(&scu->lock);

	/* to read PIN state via scu api */
	err = imx_sc_misc_get_control(scu->handle, sc_arr[offset],
				      0, &level);
	mutex_unlock(&scu->lock);

	if (err) {
		pr_err("%s: failed %d\n", __func__, err);
		return -EINVAL;
	}

	return level;
}

static void imxscfw_set(struct gpio_chip *chip, unsigned int offset, int value)
{
	struct imxscfw *scu = gpiochip_get_data(chip);
	int err;

	if (offset >= sizeof(sc_arr)/sizeof(unsigned int))
		return;

	mutex_lock(&scu->lock);

	/* to set PIN output level via scu api */
	err = imx_sc_misc_set_control(scu->handle, sc_arr[offset], 0, value);

	mutex_unlock(&scu->lock);

	if (err)
		pr_err("%s: failed %d\n", __func__, err);


}

static int imx_scu_gpio_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct imxscfw *port;
	struct gpio_chip *gc;
	int ret;

	port = devm_kzalloc(&pdev->dev, sizeof(*port), GFP_KERNEL);
	if (!port)
		return -ENOMEM;

	ret = imx_scu_get_handle(&port->handle);
	if (ret)
		return ret;

	mutex_init(&port->lock);
	gc = &port->chip;
	gc->of_node = np;
	gc->parent = dev;
	gc->label = "imx-scu-gpio";
	gc->ngpio = PIN_NUMBER;
	gc->base = of_alias_get_id(np, "gpio") * 32;

	gc->get = imxscfw_get;
	gc->set = imxscfw_set;

	platform_set_drvdata(pdev, port);

	ret = devm_gpiochip_add_data(dev, gc, port);

	return ret;
}

static const struct of_device_id imx_scu_gpio_dt_ids[] = {
	{ .compatible = "fsl,imx-scu-gpio" },
	{ /* sentinel */ }
};

static struct platform_driver imx_scu_gpio_driver = {
	.driver	= {
		.name = "gpio-imx-scu",
		.of_match_table = imx_scu_gpio_dt_ids,
	},
	.probe = imx_scu_gpio_probe,
};

static int __init _imx_scu_gpio_init(void)
{
	return platform_driver_register(&imx_scu_gpio_driver);
}

subsys_initcall_sync(_imx_scu_gpio_init);

MODULE_AUTHOR("Shenwei Wang");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("NXP GPIO over SCU-MISC API, i.MX8");
