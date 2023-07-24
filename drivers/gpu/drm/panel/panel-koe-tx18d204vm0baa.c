// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Huijuan Xie <huijuan.xie@mediatek.com>
 */

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_gpio.h>
#include <asm-generic/gpio.h>

#include <drm/drm_connector.h>
#include <drm/drm_crtc.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>

#include <video/mipi_display.h>

#include "drm/bridge/ite-it6122.h"

#define KD_DSI_CUSTOM_WRITE

struct panel_desc {
	const struct drm_display_mode *modes;
	unsigned int bpc;

	/**
	 * @width_mm: width of the panel's active display area
	 * @height_mm: height of the panel's active display area
	 */
	struct {
		unsigned int width_mm;
		unsigned int height_mm;
	} size;

	unsigned long mode_flags;
	enum mipi_dsi_pixel_format format;
	const struct panel_init_cmd *init_cmds;
	unsigned int lanes;
};

struct koe_panel {
	struct drm_panel base;
	struct mipi_dsi_device *dsi;
	struct device *dev;
	const struct panel_desc *desc;
	bool enabled;
	enum drm_panel_orientation orientation;
	struct gpio_desc *pwr_en;
	struct gpio_desc *bl_en;
	bool prepared_power;
	bool prepared;
	struct notifier_block bl_notifier;
	bool it6122_attached;
};

/* ----------------------------------------------------------------- */
/* Local Constants */
/* ----------------------------------------------------------------- */

#define GPIO_OUT_ONE		1
#define GPIO_OUT_ZERO		0

#define REGFLAG_DELAY		0xFE
#define REGFLAG_END_OF_TABLE		0x00

static inline struct koe_panel *to_koe_panel(struct drm_panel *panel)
{
	return container_of(panel, struct koe_panel, base);
}

static int koe_panel_unprepare_power(struct drm_panel *panel)
{
	struct koe_panel *koe = to_koe_panel(panel);
	int ret = 0;

	if (!koe->prepared_power)
		return ret;

	pr_debug("[Kernel/LCM] %s enter\n", __func__);

	gpiod_set_value(koe->bl_en, GPIO_OUT_ZERO);
	mdelay(5);

	gpiod_set_value(koe->pwr_en, GPIO_OUT_ZERO);
	mdelay(5);

	if (koe->it6122_attached) {
		pr_debug("wayT-- check if power off it6122(%d)\n", it6122_bridge_enabled());
		if (it6122_bridge_enabled()) {
			ret = it6122_bridge_power_on_off(false);
			if (ret) {
				pr_err("failed to power off it6122, rc=%d\n", ret);
				return ret;
			}
		}
	}
	mdelay(5);

	koe->prepared_power = false;

	return ret;
}

static int koe_panel_unprepare(struct drm_panel *panel)
{
	struct koe_panel *koe = to_koe_panel(panel);
	int ret;

	if (!koe->prepared)
		return 0;

	koe_panel_unprepare_power(panel);

	koe->prepared = false;

	return 0;
}

static int koe_panel_prepare_power(struct drm_panel *panel)
{
	struct koe_panel *koe = to_koe_panel(panel);
	int ret = 0;

	if (koe->prepared_power)
		return ret;

	mdelay(100);

	if (koe->it6122_attached) {
		pr_debug("wayT-- check if power on it6122(%d)\n", it6122_bridge_enabled());
		if (it6122_bridge_enabled()) {
			ret = it6122_bridge_power_on_off(true);
			if (ret) {
				pr_err("failed to power on it6122, rc=%d\n", ret);
				return ret;
			}
		}
	}
	mdelay(5);

	gpiod_set_value(koe->pwr_en, GPIO_OUT_ONE);
	mdelay(5);

	gpiod_set_value(koe->bl_en, GPIO_OUT_ONE);
	mdelay(5);

	koe->prepared_power = true;

	return ret;
}

static int koe_panel_prepare(struct drm_panel *panel)
{
	struct koe_panel *koe = to_koe_panel(panel);

	if (koe->prepared)
		return 0;

	koe->prepared = true;

	return 0;
}

static int koe_panel_enable(struct drm_panel *panel)
{
	struct koe_panel *koe = to_koe_panel(panel);

	if (koe->enabled)
		return 0;

	koe_panel_prepare_power(panel);

	koe->enabled = true;

	return 0;
}

static int koe_panel_disable(struct drm_panel *panel)
{
	struct koe_panel *koe = to_koe_panel(panel);

	if (!koe->enabled)
		return 0;

	koe->enabled = false;

	return 0;
}

static const struct drm_display_mode koe_default_mode = {
	.clock = 148500,
	.hdisplay = 1920,
	.hsync_start = 1920 + 124,
	.hsync_end = 1920 + 124 + 32,
	.htotal = 1920 + 124 + 32 + 124,
	.vdisplay = 1080,
	.vsync_start = 1080 + 20,
	.vsync_end = 1080 + 20 + 5,
	.vtotal = 1080 + 20 + 5 + 20,
};

static const struct panel_desc koe_desc = {
	.modes = &koe_default_mode,
	.bpc = 8,
	.size = {
		 .width_mm = 87,
		 .height_mm = 155,
		 },
	.lanes = 4,
	.format = MIPI_DSI_FMT_RGB888,
	.mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE | MIPI_DSI_MODE_LPM,
};

static int koe_panel_get_modes(struct drm_panel *panel, struct drm_connector *connector)
{
	struct koe_panel *koe = to_koe_panel(panel);
	const struct drm_display_mode *m = koe->desc->modes;
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, m);
	if (!mode) {
		dev_err(panel->dev, "failed to add mode %ux%u@%u\n",
			m->hdisplay, m->vdisplay, drm_mode_vrefresh(m));
		return -ENOMEM;
	}

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_set_name(mode);
	drm_mode_probed_add(connector, mode);

	connector->display_info.width_mm = koe->desc->size.width_mm;
	connector->display_info.height_mm = koe->desc->size.height_mm;
	connector->display_info.bpc = koe->desc->bpc;
	drm_connector_set_panel_orientation(connector, koe->orientation);

	return 1;
}

static const struct drm_panel_funcs koe_panel_funcs = {
	.unprepare = koe_panel_unprepare,
	.prepare = koe_panel_prepare,
	.disable = koe_panel_disable,
	.enable = koe_panel_enable,
	.get_modes = koe_panel_get_modes,
};

static int koe_panel_add(struct koe_panel *koe)
{
	struct device *dev = &koe->dsi->dev;
	int ret;

	koe->pwr_en = devm_gpiod_get(dev, "pwr", GPIOD_OUT_HIGH);
	if (IS_ERR(koe->pwr_en)) {
		dev_err(dev, "cannot get pwr_en-gpios %ld\n", PTR_ERR(koe->pwr_en));
		return PTR_ERR(koe->pwr_en);
	}

	koe->bl_en = devm_gpiod_get(dev, "bl", GPIOD_OUT_HIGH);
	if (IS_ERR(koe->bl_en)) {
		dev_err(dev, "cannot get bl_en-gpios %ld\n", PTR_ERR(koe->bl_en));
		return PTR_ERR(koe->bl_en);
	}

	drm_panel_init(&koe->base, dev, &koe_panel_funcs, DRM_MODE_CONNECTOR_DSI);
	ret = of_drm_get_panel_orientation(dev->of_node, &koe->orientation);
	if (ret < 0) {
		dev_err(dev, "%pOF: failed to get orientation %d\n", dev->of_node, ret);
		return ret;
	}

	koe->base.funcs = &koe_panel_funcs;
	koe->base.dev = &koe->dsi->dev;

	drm_panel_add(&koe->base);
	return 0;
}

static int koe_panel_probe(struct mipi_dsi_device *dsi)
{
	struct koe_panel *koe;
	struct device *dev = &dsi->dev;
	int ret;
	const struct panel_desc *desc;

	koe = devm_kzalloc(&dsi->dev, sizeof(*koe), GFP_KERNEL);
	if (!koe)
		return -ENOMEM;

	koe->dev = dev;
	desc = of_device_get_match_data(&dsi->dev);
	dsi->lanes = desc->lanes;
	dsi->format = desc->format;
	dsi->mode_flags = desc->mode_flags;
	koe->desc = desc;
	koe->dsi = dsi;
	ret = koe_panel_add(koe);
	if (ret < 0)
		return ret;

	mipi_dsi_set_drvdata(dsi, koe);

	ret = mipi_dsi_attach(dsi);
	if (ret)
		drm_panel_remove(&koe->base);

	if (of_property_read_bool(dev->of_node, "mediatek,it6122-attached"))
		koe->it6122_attached = true;

	return ret;
}

static void koe_panel_shutdown(struct mipi_dsi_device *dsi)
{
	struct koe_panel *koe = mipi_dsi_get_drvdata(dsi);

	drm_panel_disable(&koe->base);
	drm_panel_unprepare(&koe->base);
}

static int koe_panel_remove(struct mipi_dsi_device *dsi)
{
	struct koe_panel *koe = mipi_dsi_get_drvdata(dsi);
	int ret;

	koe_panel_shutdown(dsi);

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "failed to detach from DSI host: %d\n", ret);

	if (koe->base.dev)
		drm_panel_remove(&koe->base);

	return 0;
}

static const struct of_device_id koe_of_match[] = {
	{.compatible = "koe,tx18d204vm0baa",
	 .data = &koe_desc},
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, koe_of_match);

static struct mipi_dsi_driver koe_panel_driver = {
	.driver = {
		   .name = "koe_tx18d204vm0baa",
		   .of_match_table = koe_of_match,
		   },
	.probe = koe_panel_probe,
	.remove = koe_panel_remove,
	.shutdown = koe_panel_shutdown,
};

module_mipi_dsi_driver(koe_panel_driver);

MODULE_AUTHOR("Huijuan Xie <huijuan.xie@mediatek.com>");
MODULE_DESCRIPTION("KOE TX18D204VM0BAA 1920*1080@60Hz single link LVDS panel driver");
MODULE_LICENSE("GPL v2");
