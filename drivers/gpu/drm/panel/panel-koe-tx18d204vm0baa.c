// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Huijuan Xie <huijuan.xie@mediatek.com>
 */

#include <drm/drm_connector.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/media-bus-format.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_gpio.h>

#include <video/display_timing.h>
#include <video/of_display_timing.h>
#include <video/videomode.h>

#define GPIO_OUT_ONE		1
#define GPIO_OUT_ZERO		0

struct panel_desc {
	const struct drm_display_mode *modes;
	unsigned int bpc;
	unsigned int width;
	unsigned int height;
	unsigned int bus_format;
	struct videomode video_mode;
};

struct koe_panel {
	struct drm_panel panel;
	struct device *dev;
	const struct panel_desc *desc;
	struct gpio_desc *blen;
	struct regulator *pp3300_supply;
	bool prepared_power;
};

static const struct drm_display_mode koe_default_mode = {
	.clock = 148500,
	.hdisplay = 1920,
	.hsync_start = 1920 + 100,
	.hsync_end = 1920 + 100 + 50,
	.htotal = 1920 + 100 + 50 + 130,
	.vdisplay = 1080,
	.vsync_start = 1080 + 14,
	.vsync_end = 1080 + 14 + 10,
	.vtotal = 1080 + 14 + 10 + 21,
};

static const struct panel_desc koe_desc = {
	.modes = &koe_default_mode,
	.width = 109,
	.height = 103,
	.bpc = 8,
	.bus_format = MEDIA_BUS_FMT_RGB888_1X7X4_SPWG,
};

static const struct of_device_id koe_panel_of_table[] = {
	{.compatible = "koe,tx18d204vm0baa",
	 .data = &koe_desc},
	{ /* Sentinel */ },
};

static inline struct koe_panel *to_koe_panel(struct drm_panel *panel)
{
	return container_of(panel, struct koe_panel, panel);
}

static int koe_panel_disable(struct drm_panel *panel)
{
	struct koe_panel *koe = to_koe_panel(panel);

	if (koe->blen)
		gpiod_set_value(koe->blen, GPIO_OUT_ZERO);

	mdelay(100);

	return 0;
}

static int koe_panel_prepare(struct drm_panel *panel)
{
	struct koe_panel *koe = to_koe_panel(panel);
	int ret;

	if (koe->prepared_power) {
		dev_info(koe->dev, "[Kernel/LCM] LVDS has already power on\n");
		return 0;
	}

	if (koe->pp3300_supply) {
		ret = regulator_enable(koe->pp3300_supply);
		if (ret < 0) {
			dev_err(panel->dev, "Enable pp3300_supply fail, %d\n", ret);
			return ret;
		}
	}

	mdelay(100);

	koe->prepared_power = true;

	return 0;
}

static int koe_panel_unprepare(struct drm_panel *panel)
{
	struct koe_panel *koe = to_koe_panel(panel);

	if (!koe->prepared_power) {
		dev_info(koe->dev, "[Kernel/LCM] LVDS has already power on\n");
		return 0;
	}

	if (koe->pp3300_supply)
		regulator_disable(koe->pp3300_supply);

	koe->prepared_power = false;

	return 0;
}

static int koe_panel_enable(struct drm_panel *panel)
{
	struct koe_panel *koe = to_koe_panel(panel);

	if (koe->blen)
		gpiod_set_value(koe->blen, GPIO_OUT_ONE);

	return 0;
}

static int koe_panel_get_modes(struct drm_panel *panel,
				struct drm_connector *connector)
{
	struct koe_panel *koe = to_koe_panel(panel);
	const struct drm_display_mode *m = koe_desc.modes;
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, m);
	if (!mode) {
		pr_err("failed to add mode %ux%u@%u\n",
			m->hdisplay, m->vdisplay, drm_mode_vrefresh(m));
		return -ENOMEM;
	}

	mode->type |= DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	connector->display_info.width_mm = koe->desc->width;
	connector->display_info.height_mm = koe->desc->height;
	drm_display_info_set_bus_formats(&connector->display_info,
					 &koe->desc->bus_format, 1);
	connector->display_info.bpc = koe->desc->bpc;

	return 1;
}

static const struct drm_panel_funcs koe_panel_funcs = {
	.disable = koe_panel_disable,
	.unprepare = koe_panel_unprepare,
	.prepare = koe_panel_prepare,
	.enable = koe_panel_enable,
	.get_modes = koe_panel_get_modes,
};

static int koe_panel_probe(struct platform_device *pdev)
{
	struct koe_panel *koe;
	const struct panel_desc *desc;

	koe = devm_kzalloc(&pdev->dev, sizeof(*koe), GFP_KERNEL);
	if (!koe)
		return -ENOMEM;

	koe->dev = &pdev->dev;
	desc = of_device_get_match_data(&pdev->dev);
	if (!desc)
		dev_err(koe->dev, "%s() desc is NULL\n", __func__);
	koe->desc = desc;

	if (!koe->desc)
		dev_err(koe->dev, "%s lvds desc is NULL\n", __func__);

	/* Get GPIOs controller. */
	koe->blen = devm_gpiod_get(&pdev->dev, "blen", GPIOD_OUT_HIGH);
	if (IS_ERR(koe->blen)) {
		dev_err(koe->dev, "cannot get blen-gpios %ld\n", PTR_ERR(koe->blen));
		return PTR_ERR(koe->blen);
	}

	koe->pp3300_supply = devm_regulator_get(&pdev->dev, "pp3300");
	if (IS_ERR(koe->pp3300_supply)) {
		dev_err(koe->dev, "cannot get pp3300 %ld\n", PTR_ERR(koe->pp3300_supply));
		return PTR_ERR(koe->pp3300_supply);
	}

	/*
	 * TODO: Handle all power supplies specified in the DT node in a generic
	 * way for panels that don't care about power supply ordering. LVDS
	 * panels that require a specific power sequence will need a dedicated
	 * driver.
	 */

	/* Register the panel. */
	drm_panel_init(&koe->panel, koe->dev, &koe_panel_funcs,
		       DRM_MODE_CONNECTOR_LVDS);

	drm_panel_of_backlight(&koe->panel);

	drm_panel_add(&koe->panel);
	dev_set_drvdata(koe->dev, koe);

	return 0;
}

static int koe_panel_remove(struct platform_device *pdev)
{
	struct koe_panel *koe = dev_get_drvdata(&pdev->dev);

	drm_panel_remove(&koe->panel);

	drm_panel_disable(&koe->panel);

	return 0;
}

MODULE_DEVICE_TABLE(of, koe_panel_of_table);

static struct platform_driver koe_panel_driver = {
	.probe		= koe_panel_probe,
	.remove		= koe_panel_remove,
	.driver		= {
		.name	= "koe_tx18d204vm0baa",
		.of_match_table = koe_panel_of_table,
	},
};

static int __init koe_panel_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&koe_panel_driver);
	if (ret < 0) {
		pr_err("%s() Failed to register %s driver: %d\n", __func__,
				koe_panel_driver.driver.name, ret);
		goto err;
	}

	return 0;
err:
	platform_driver_unregister(&koe_panel_driver);

	return ret;
}

static void __exit koe_panel_exit(void)
{
	platform_driver_unregister(&koe_panel_driver);
}

module_init(koe_panel_init);
module_exit(koe_panel_exit);

MODULE_AUTHOR("Huijuan Xie <huijuan.xie@mediatek.com>");
MODULE_DESCRIPTION("KOE TX18D204VM0BAA 1920*1080@60Hz single link LVDS panel driver");
MODULE_LICENSE("GPL");
