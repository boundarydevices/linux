/*
 * Copyright (c) 2017, Fuzhou Rockchip Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/backlight.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

static const u32 lts_bus_formats[] = {
	MEDIA_BUS_FMT_RGB888_1X24,
};

static const struct display_timing lts_default_timing = {	// 60fps
	.pixelclock = { 162000*1000, 162000*1000, 162000*1000},
	.hactive = { 1200, 1200, 1200},
#if 1
	.hfront_porch = { 70, 70, 70},
	.hsync_len = {8, 8, 8},
	.hback_porch = {70, 70, 70},
#else
	.hfront_porch = { 11, 11, 11},
	.hsync_len = { 8, 8, 8},
	.hback_porch = { 10, 10, 10},
#endif
	.vactive = { 1920, 1920, 1920},
#if 1
	.vfront_porch = {4, 4, 4},
	.vsync_len = {2, 2, 2},
	.vback_porch = {84, 84, 84},
#else
	.vfront_porch = { 4, 4, 4},
	.vsync_len = { 4, 4, 4},
	.vback_porch = { 76, 76, 76},
#endif
	.flags = DISPLAY_FLAGS_HSYNC_LOW |
		 DISPLAY_FLAGS_VSYNC_LOW |
		 DISPLAY_FLAGS_DE_LOW |
		 DISPLAY_FLAGS_PIXDATA_NEGEDGE,
};

struct lts_panel {
	struct drm_panel base;
	struct mipi_dsi_device *link;

	struct backlight_device *backlight;
	struct regulator *supply;
	struct gpio_desc *reset_gpio,*enable_gpio;

	struct videomode vm;
	u32 width_mm;
	u32 height_mm;
	u32 bpc;
	bool prepared;
	bool deep_standby;
	bool enabled;
};

static inline struct lts_panel *to_lts_panel(struct drm_panel *panel)
{
	return container_of(panel, struct lts_panel, base);
}

static int lts_panel_exit_sleep_mode(struct lts_panel *lts);

static int lts_panel_power_on(struct lts_panel *lts)
{
	int ret=0;
	int x;

	//ret = regulator_enable(lts->supply);
	//if (ret < 0)
	//	return ret;

	for (x=0;x<1;x++) {
		/* reset width 20ms, wait 150ms */
		gpiod_set_value_cansleep(lts->reset_gpio, 0);
		if (lts->enable_gpio) gpiod_set_value_cansleep(lts->enable_gpio, 1);
		usleep_range(20*1000, 30*1000);
		gpiod_set_value_cansleep(lts->reset_gpio, 1);
		usleep_range(300*1000, 310*1000);
	}
	lts->link->mode_flags |= MIPI_DSI_MODE_LPM;
	if (!ret) ret=lts_panel_exit_sleep_mode(lts);

	return ret;
}

static int lts_panel_power_off(struct lts_panel *lts)
{
	int ret=0;

	//ret = regulator_disable(lts->supply);
	//if (ret)
	//	DRM_DEV_ERROR(lts->base.dev, "failed to disable regulator: %d\n",
	//		      ret);

	return ret;
}

static int lts_panel_enter_sleep_mode(struct lts_panel *lts)
{
	int ret=0;
	u8 data = 0;
	int i;

	struct mipi_dsi_msg msg = {
		.channel = lts->link->channel,
		.tx_buf = &data,
		.tx_len = sizeof(data)
	};

	ret = mipi_dsi_dcs_enter_sleep_mode(lts->link);
	if(ret) {
		DRM_DEV_ERROR(lts->base.dev, "failed to enter sleep mode: %d\n",
			      ret);
		return ret;
	}
		
	usleep_range(150*1000,160*1000);
		
	return ret;
}

static int lts_panel_exit_sleep_mode(struct lts_panel *lts)
{
	int ret=0;
	u8 data = 0;
	int i;

	struct mipi_dsi_msg msg = {
		.channel = lts->link->channel,
		.tx_buf = &data,
		.tx_len = sizeof(data)
	};

	dev_warn(&lts->link->dev, "%s\n", __func__);
	
	ret = mipi_dsi_dcs_exit_sleep_mode(lts->link);
	if(ret) {
		DRM_DEV_ERROR(lts->base.dev, "failed to exit sleep mode: %d\n",
			      ret);
		return ret;
	}

	usleep_range(300*1000,310*1000);
		
	return ret;
}

static int lts_panel_set_display_on(struct lts_panel *lts)
{
	int ret=0;
	u8 data = 0x00;

	dev_warn(&lts->link->dev, "%s\n", __func__);

	ret = mipi_dsi_dcs_set_display_on(lts->link);
	if(ret) {
		DRM_DEV_ERROR(lts->base.dev, "failed to set display on: %d\n",
			      ret);
		return ret;
	}

	usleep_range(200*1000,210*1000);

	return ret;
}

static int lts_panel_set_display_off(struct lts_panel *lts)
{
	int ret=0;

	ret = mipi_dsi_dcs_set_display_off(lts->link);
	if(ret)
		DRM_DEV_ERROR(lts->base.dev, "failed to set display off: %d\n",
			      ret);

	return ret;
}

static int lts_panel_disable(struct drm_panel *panel)
{
	struct lts_panel *lts = to_lts_panel(panel);
	int err;

	if (!lts->enabled)
		return 0;

	lts->link->mode_flags &= ~MIPI_DSI_MODE_LPM;
	lts->backlight->props.power = FB_BLANK_POWERDOWN;
	backlight_update_status(lts->backlight);

	lts_panel_set_display_off(lts);

	lts->enabled = false;

	return 0;
}

static int lts_panel_unprepare(struct drm_panel *panel)
{
// XXX BT currently does nothing because I'm not sure it's ever possible
// to come back properly from unprepare if unprepare were to shut the
// panel down - or, at least, I think coming back up would require a
// level of coordination with the DSI host that I think isn't available...
#if 0
	struct lts_panel *lts = to_lts_panel(panel);
	int err;

	if (!lts->prepared)
		return 0;

	err = lts_panel_power_off(lts);
	if (err < 0) {
		DRM_DEV_ERROR(panel->dev, "failed to enter sleep mode: %d\n",
			      err);
		return err;
	}


	lts->prepared = false;
#endif

	return 0;
}

static int lts_panel_prepare(struct drm_panel *panel)
{
	struct lts_panel *lts = to_lts_panel(panel);
	int err;

	if (lts->prepared)
		return 0;

	dev_warn(&lts->link->dev, "%s\n", __func__);
#if 0
	err = lts_panel_power_on(lts);
	if (err < 0) {
		DRM_DEV_ERROR(panel->dev, "failed to power on: %d\n",
			      err);
		goto poweroff;
	}
#endif

	lts->prepared = true;

	return 0;

poweroff:

	return err;
}

static int lts_panel_enable(struct drm_panel *panel)
{
	struct lts_panel *lts = to_lts_panel(panel);
	int ret;

	if (lts->enabled)
		return 0;
	dev_warn(&lts->link->dev, "%s\n", __func__);
	lts->link->mode_flags |= MIPI_DSI_MODE_LPM;
	lts_panel_power_on(lts);

	ret = lts_panel_set_display_on(lts);
	if (ret < 0) {
		DRM_DEV_ERROR(panel->dev, "failed to set display on: %d\n",
			      ret);
		return ret;
	}

	lts->backlight->props.power = FB_BLANK_UNBLANK;
	ret = backlight_update_status(lts->backlight);
	if (ret) {
		DRM_DEV_ERROR(panel->drm->dev,
			      "Failed to enable backlight %d\n", ret);
		return ret;
	}

	lts->enabled = true;

	return 0;
}

static int lts_panel_get_modes(struct drm_panel *panel)
{
	struct drm_display_mode *mode;
	struct lts_panel *lts = to_lts_panel(panel);
	struct device *dev = &lts->link->dev;
	struct drm_connector *connector = panel->connector;
	u32 *bus_flags = &connector->display_info.bus_flags;
	int ret;

	dev_warn(dev, "%s\n", __func__);

	mode = drm_mode_create(connector->dev);
	if (!mode) {
		DRM_DEV_ERROR(dev, "Failed to create display mode!\n");
		return 0;
	}

	drm_display_mode_from_videomode(&lts->vm, mode);
	mode->width_mm = lts->width_mm;
	mode->height_mm = lts->height_mm;
	connector->display_info.width_mm = lts->width_mm;
	connector->display_info.height_mm = lts->height_mm;

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;

	if (lts->vm.flags & DISPLAY_FLAGS_DE_HIGH)
		*bus_flags |= DRM_BUS_FLAG_DE_HIGH;
	if (lts->vm.flags & DISPLAY_FLAGS_DE_LOW)
		*bus_flags |= DRM_BUS_FLAG_DE_LOW;
	if (lts->vm.flags & DISPLAY_FLAGS_PIXDATA_NEGEDGE)
		*bus_flags |= DRM_BUS_FLAG_PIXDATA_NEGEDGE;
	if (lts->vm.flags & DISPLAY_FLAGS_PIXDATA_POSEDGE)
		*bus_flags |= DRM_BUS_FLAG_PIXDATA_POSEDGE;

	ret = drm_display_info_set_bus_formats(&connector->display_info,
			lts_bus_formats, ARRAY_SIZE(lts_bus_formats));
	if (ret)
		return ret;

	drm_mode_probed_add(panel->connector, mode);

	return 1;

}

static const struct drm_panel_funcs lts_panel_funcs = {
	.disable = lts_panel_disable,
	.unprepare = lts_panel_unprepare,
	.prepare = lts_panel_prepare,
	.enable = lts_panel_enable,
	.get_modes = lts_panel_get_modes,
};

static const struct of_device_id lts_of_match[] = {
	{ .compatible = "lts,lts-lcd133", },
	{ }
};
MODULE_DEVICE_TABLE(of, lts_of_match);

static int lts_panel_add(struct lts_panel *lts)
{
	struct device *dev = &lts->link->dev;
	struct device_node *np;
	int err;
	//lts->supply = devm_regulator_get(dev, "power");
	//if (IS_ERR(lts->supply))
	//	return PTR_ERR(lts->supply);

	lts->reset_gpio = devm_gpiod_get_optional(dev, "reset",
						       GPIOD_OUT_LOW);
	if (IS_ERR(lts->reset_gpio)) {
		err = PTR_ERR(lts->reset_gpio);
		dev_dbg(dev, "failed to get reset gpio: %d\n", err);
		lts->reset_gpio = NULL;
	}

	lts->enable_gpio = devm_gpiod_get_optional(dev, "enable",
							GPIOD_OUT_LOW);
	if (IS_ERR(lts->enable_gpio)) lts->enable_gpio = NULL;

	np = of_parse_phandle(dev->of_node, "backlight", 0);
	if (np) {
		lts->backlight = of_find_backlight_by_node(np);
		of_node_put(np);

		if (!lts->backlight)
			return -EPROBE_DEFER;
	}

	of_property_read_u32(np, "panel-width-mm", &lts->width_mm);
	of_property_read_u32(np, "panel-height-mm", &lts->height_mm);
	of_property_read_u32(np, "panel-bpc", &lts->bpc);

	drm_panel_init(&lts->base);
	lts->base.funcs = &lts_panel_funcs;
	lts->base.dev = &lts->link->dev;

	err = drm_panel_add(&lts->base);
	if (err < 0)
		goto put_backlight;

	return 0;

put_backlight:
	put_device(&lts->backlight->dev);

	return err;
}

static void lts_panel_del(struct lts_panel *lts)
{
	if (lts->base.dev)
		drm_panel_remove(&lts->base);

	put_device(&lts->backlight->dev);
}

static int lts_panel_probe(struct mipi_dsi_device *dsi)
{
	struct lts_panel *lts;
	struct device *dev = &dsi->dev;
	struct device_node *np = dev->of_node;
	struct device_node *timings;
	int ret;
	u32 video_mode;
	u32 bpc;

	dev_warn(dev, "%s\n", __func__);
	ret = of_property_read_u32(np, "dsi-lanes", &dsi->lanes);
	if (ret < 0) {
		dev_err(dev, "Failed to get dsi-lanes property (%d)\n", ret);
		return ret;
	}

	ret = of_property_read_u32(np, "panel-bpc", &bpc);
	if (!ret) {
		switch (bpc) {
		case 6:
			dsi->format = MIPI_DSI_FMT_RGB666;
			break;
		case 8:
			dsi->format = MIPI_DSI_FMT_RGB888;
			break;

		default:
			dev_warn(dev, "invalid bpc %d\n", bpc);
			break;

		}
	}

	dsi->mode_flags = MIPI_DSI_MODE_VIDEO |
			MIPI_DSI_MODE_VIDEO_HSE; // |
			// MIPI_DSI_CLOCK_NON_CONTINUOUS;
	ret = of_property_read_u32(np, "video-mode", &video_mode);
	if (!ret) {
		switch (video_mode) {
		case 0:
			/* burst mode */
			dsi->mode_flags |= MIPI_DSI_MODE_VIDEO_BURST;
			break;
		case 1:
			/* non-burst mode with sync event */
			break;
		case 2:
			/* non-burst mode with sync pulse */
			dsi->mode_flags |= MIPI_DSI_MODE_VIDEO_SYNC_PULSE;
			break;
		default:
			dev_warn(dev, "invalid video mode %d\n", video_mode);
			break;

		}
	}

	lts = devm_kzalloc(&dsi->dev, sizeof(*lts), GFP_KERNEL);
	if (!lts)
		return -ENOMEM;

	timings = of_get_child_by_name(np, "display-timings");
	if (timings) {
		of_node_put(timings);
		ret = of_get_videomode(np, &lts->vm, 0);
		if (ret < 0) {
			dev_err(dev, "of_get_videomode error:%d\n", ret);
			return ret;
		}
	} else {
		videomode_from_timing(&lts_default_timing, &lts->vm);
	}


	mipi_dsi_set_drvdata(dsi, lts);

	lts->link = dsi;

	ret = lts_panel_add(lts);
	if (ret < 0)
		return ret;

	ret = mipi_dsi_attach(dsi);
	return ret;
}

static int lts_panel_remove(struct mipi_dsi_device *dsi)
{
	struct lts_panel *lts = mipi_dsi_get_drvdata(dsi);
	int err;

	lts_panel_disable(&lts->base);
	lts_panel_unprepare(&lts->base);
	// lts_panel_power_off(lts);

	err = mipi_dsi_detach(dsi);
	if (err < 0)
		DRM_DEV_ERROR(&dsi->dev, "failed to detach from DSI host: %d\n",
			      err);

	drm_panel_detach(&lts->base);
	lts_panel_del(lts);

	return 0;
}

static void lts_panel_shutdown(struct mipi_dsi_device *dsi)
{
	struct lts_panel *lts = mipi_dsi_get_drvdata(dsi);

	lts_panel_disable(&lts->base);
	lts_panel_unprepare(&lts->base);
	// lts_panel_power_off(lts);
}

static struct mipi_dsi_driver lts_panel_driver = {
	.driver = {
		.name = "panel-lts-lcd133",
		.of_match_table = lts_of_match,
	},
	.probe = lts_panel_probe,
	.remove = lts_panel_remove,
	.shutdown = lts_panel_shutdown,
};
module_mipi_dsi_driver(lts_panel_driver);

MODULE_AUTHOR("Hanyi Zou <hz1190@nyu.edu>");
MODULE_DESCRIPTION("LTS lcd133 panel driver");
MODULE_LICENSE("GPL v2");
