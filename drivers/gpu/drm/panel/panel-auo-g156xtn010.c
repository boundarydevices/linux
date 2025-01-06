// SPDX-License-Identifier: GPL-2.0
/*
 * AUO g156xtn010 1366*768@60Hz single link LVDS Panel Driver
 *
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

struct panel_lvds {
	struct drm_panel panel;
	struct device *dev;
	const char *label;
	const struct panel_desc *desc;
	struct backlight_device *backlight;
	struct regulator *supply;
	struct gpio_desc *bl_en;
	bool prepared_power;
	enum drm_panel_orientation orientation;
};

static const struct drm_display_mode auo_g156xtn010_mode = {
	.clock = 75732,
	.hdisplay = 1366,
	.hsync_start = 1366 + 100,
	.hsync_end = 1366 + 100 + 50,
	.htotal = 1366 + 100 + 50 + 50,
	.vdisplay = 768,
	.vsync_start = 768 + 14,
	.vsync_end = 768 + 14 + 12,
	.vtotal = 768 + 14 + 12 + 12,
};

static const struct panel_desc auo_g156xtn010 = {
	.modes = &auo_g156xtn010_mode,
	.width = 344,
	.height = 193,
	.bpc = 8,
	.bus_format = MEDIA_BUS_FMT_RGB888_1X7X4_SPWG,
};

static const struct of_device_id panel_lvds_of_table[] = {
	{
		.compatible = "auo,g156xtn010_lvds",
		.data = &auo_g156xtn010
	},
	{ /* Sentinel */ },
};

static inline struct panel_lvds *to_panel_lvds(struct drm_panel *panel)
{
	return container_of(panel, struct panel_lvds, panel);
}

static int panel_lvds_disable(struct drm_panel *panel)
{
	return 0;
}

static int panel_lvds_unprepare(struct drm_panel *panel)
{
	struct panel_lvds *lvds = to_panel_lvds(panel);

	if (!lvds->prepared_power)
		return 0;

	gpiod_set_value(lvds->bl_en, GPIO_OUT_ZERO);
	mdelay(5);

	regulator_disable(lvds->supply);
	mdelay(5);

	lvds->prepared_power = false;

	return 0;
}

static int panel_lvds_prepare(struct drm_panel *panel)
{
	struct panel_lvds *lvds = to_panel_lvds(panel);
	int err;

	if (lvds->prepared_power) {
		pr_notice("[Kernel/LCM] LVDS has already power on\n");
		return 0;
	}

	err = regulator_enable(lvds->supply);
	if (err) {
		dev_err(lvds->dev, "Failed to enable power supply: %d\n", err);
		return err;
	}
	mdelay(5);

	gpiod_set_value(lvds->bl_en, GPIO_OUT_ONE);
	mdelay(5);

	lvds->prepared_power = true;

	return 0;
}

static int panel_lvds_enable(struct drm_panel *panel)
{
	return 0;
}

static int panel_lvds_get_modes(struct drm_panel *panel,
				struct drm_connector *connector)
{
	struct panel_lvds *lvds = to_panel_lvds(panel);
	const struct drm_display_mode *m = auo_g156xtn010.modes;
	struct drm_display_mode *mode;

	pr_debug("%s enter, m: %d\n", __func__, m->clock);
	mode = drm_mode_duplicate(connector->dev, m);
	if (!mode) {
		dev_err(panel->dev, "failed to add mode %ux%u@%u\n",
			m->hdisplay, m->vdisplay, drm_mode_vrefresh(m));
		return -ENOMEM;
	}
	if (!mode)
		return 0;

	mode->type |= DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);
	drm_mode_set_name(mode);

	connector->display_info.width_mm = lvds->desc->width;
	connector->display_info.height_mm = lvds->desc->height;
	drm_display_info_set_bus_formats(&connector->display_info,
					 &lvds->desc->bus_format, 1);
	connector->display_info.bpc = lvds->desc->bpc;

	return 1;
}

static const struct drm_panel_funcs panel_lvds_funcs = {
	.disable = panel_lvds_disable,
	.unprepare = panel_lvds_unprepare,
	.prepare = panel_lvds_prepare,
	.enable = panel_lvds_enable,
	.get_modes = panel_lvds_get_modes,
};

static int panel_lvds_probe(struct platform_device *pdev)
{
	struct panel_lvds *lvds;
	const struct panel_desc *desc;

	lvds = devm_kzalloc(&pdev->dev, sizeof(*lvds), GFP_KERNEL);
	if (!lvds)
		return -ENOMEM;

	lvds->dev = &pdev->dev;
	desc = of_device_get_match_data(&pdev->dev);
	if (!desc)
		pr_notice("%s desc is NULL\n", __func__);
	lvds->desc = desc;

	if (!lvds->desc)
		pr_notice("%s lvds desc is NULL\n", __func__);

	lvds->supply = devm_regulator_get(&pdev->dev, "power");
	if (IS_ERR(lvds->supply))
		return dev_err_probe(&pdev->dev, PTR_ERR(lvds->supply), "Failed to get power supply");

	/* Get GPIOs controller. */
	lvds->bl_en = devm_gpiod_get(&pdev->dev, "bl", GPIOD_OUT_HIGH);
	if (IS_ERR(lvds->bl_en)) {
		dev_err(&pdev->dev, "cannot get bl_en-gpios %ld\n", PTR_ERR(lvds->bl_en));
		return PTR_ERR(lvds->bl_en);
	}

	/*
	 * TODO: Handle all power supplies specified in the DT node in a generic
	 * way for panels that don't care about power supply ordering. LVDS
	 * panels that require a specific power sequence will need a dedicated
	 * driver.
	 */

	/* Register the panel. */
	drm_panel_init(&lvds->panel, lvds->dev, &panel_lvds_funcs,
		       DRM_MODE_CONNECTOR_LVDS);

	drm_panel_add(&lvds->panel);

	dev_set_drvdata(lvds->dev, lvds);

	return 0;
}

static int panel_lvds_remove(struct platform_device *pdev)
{
	struct panel_lvds *lvds = dev_get_drvdata(&pdev->dev);

	drm_panel_remove(&lvds->panel);

	drm_panel_disable(&lvds->panel);

	return 0;
}

MODULE_DEVICE_TABLE(of, panel_lvds_of_table);

static struct platform_driver panel_lvds_driver = {
	.probe		= panel_lvds_probe,
	.remove		= panel_lvds_remove,
	.driver		= {
		.name	= "g156xtn010_lvds",
		.of_match_table = panel_lvds_of_table,
	},
};

module_platform_driver(panel_lvds_driver);

MODULE_AUTHOR("Huijuan Xie <huijuan.xie@mediatek.com>");
MODULE_DESCRIPTION("AUO g156xtn010 1366*768@60Hz single link LVDS Panel Driver");
MODULE_LICENSE("GPL v2");

