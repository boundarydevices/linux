/*
 * drivers/amlogic/drm/am_meson_lcd.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <drm/drm_modeset_helper.h>
#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_panel.h>
#include <drm/drm_mipi_dsi.h>
#include <video/display_timing.h>
#include <linux/component.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>

#include "am_meson_lcd.h"

struct am_drm_lcd_s {
	struct drm_panel panel;
	struct drm_connector connector;
	struct drm_encoder encoder;
	struct mipi_dsi_host dsi_host;
	struct drm_device *drm;
	struct aml_lcd_drv_s *lcd_drv;
	struct drm_display_mode *mode;
	struct display_timing *timing;
};

static struct am_drm_lcd_s *am_drm_lcd;

static struct drm_display_mode am_lcd_mode = {
	.name = "panel",
	.status = 0,
	.clock = 74250,
	.hdisplay = 1280,
	.hsync_start = 1390,
	.hsync_end = 1430,
	.htotal = 1650,
	.hskew = 0,
	.vdisplay = 720,
	.vsync_start = 725,
	.vsync_end = 730,
	.vtotal = 750,
	.vscan = 0,
	.vrefresh = 60,
};

static struct display_timing am_lcd_timing = {
	.pixelclock = { 55000000, 65000000, 75000000 },
	.hactive = { 1024, 1024, 1024 },
	.hfront_porch = { 40, 40, 40 },
	.hback_porch = { 220, 220, 220 },
	.hsync_len = { 20, 60, 100 },
	.vactive = { 768, 768, 768 },
	.vfront_porch = { 7, 7, 7 },
	.vback_porch = { 21, 21, 21 },
	.vsync_len = { 10, 10, 10 },
	.flags = DISPLAY_FLAGS_DE_HIGH,
};

/* ***************************************************************** */
/*     drm driver function                                           */
/* ***************************************************************** */
#if 0
static inline struct am_drm_lcd_s *host_to_lcd(struct mipi_dsi_host *host)
{
	return container_of(host, struct am_drm_lcd_s, dsi_host);
}
#endif

static inline struct am_drm_lcd_s *con_to_lcd(struct drm_connector *con)
{
	return container_of(con, struct am_drm_lcd_s, connector);
}

static inline struct am_drm_lcd_s *encoder_to_lcd(struct drm_encoder *encoder)
{
	return container_of(encoder, struct am_drm_lcd_s, encoder);
}

static inline struct am_drm_lcd_s *panel_to_lcd(struct drm_panel *panel)
{
	return container_of(panel, struct am_drm_lcd_s, panel);
}

static int am_lcd_connector_get_modes(struct drm_connector *connector)
{
	struct drm_display_mode *mode;
	struct am_drm_lcd_s *lcd;
	int count = 0;

	lcd = con_to_lcd(connector);

	pr_info("***************************************************\n");
	pr_info("am_drm_lcd: %s: lcd mode [%s] display size: %d x %d\n",
		__func__, lcd->mode->name,
		lcd->mode->hdisplay, lcd->mode->vdisplay);

	mode = drm_mode_duplicate(connector->dev, lcd->mode);
	pr_info("am_drm_lcd: %s: drm mode [%s] display size: %d x %d\n",
		__func__, mode->name, mode->hdisplay, mode->vdisplay);
	pr_info("am_drm_lcd: %s: lcd config size: %d x %d\n",
		__func__, lcd->lcd_drv->lcd_config->lcd_basic.h_active,
		lcd->lcd_drv->lcd_config->lcd_basic.v_active);

	drm_mode_probed_add(connector, mode);
	count = 1;
	pr_info("am_drm_lcd: %s %d\n", __func__, __LINE__);
	pr_info("***************************************************\n");

	return count;
}

enum drm_mode_status am_lcd_connector_mode_valid(
		struct drm_connector *connector,
		struct drm_display_mode *mode)
{
	struct am_drm_lcd_s *lcd;

	lcd = con_to_lcd(connector);
	if (!lcd)
		return MODE_ERROR;
	if (!lcd->lcd_drv)
		return MODE_ERROR;

	pr_info("am_drm_lcd: %s: mode [%s] display size: %d x %d\n",
		__func__, mode->name, mode->hdisplay, mode->vdisplay);
	pr_info("am_drm_lcd: %s: lcd config size: %d x %d\n",
		__func__, lcd->lcd_drv->lcd_config->lcd_basic.h_active,
		lcd->lcd_drv->lcd_config->lcd_basic.v_active);

	if (mode->hdisplay != lcd->lcd_drv->lcd_config->lcd_basic.h_active)
		return MODE_BAD_WIDTH;
	if (mode->vdisplay != lcd->lcd_drv->lcd_config->lcd_basic.v_active)
		return MODE_BAD_WIDTH;

	pr_info("am_drm_lcd: %s %d: check mode OK\n", __func__, __LINE__);

	return MODE_OK;
}


static const struct drm_connector_helper_funcs am_lcd_connector_helper_funcs = {
	.get_modes = am_lcd_connector_get_modes,
	.mode_valid = am_lcd_connector_mode_valid,
	//.best_encoder
	//.atomic_best_encoder
};

static enum drm_connector_status am_lcd_connector_detect(
		struct drm_connector *connector, bool force)
{
	pr_info("am_drm_lcd: %s %d\n", __func__, __LINE__);
	return connector_status_connected;
}

#if 0
static const struct drm_connector_funcs am_lcd_connector_funcs = {
	.dpms			= drm_atomic_helper_connector_dpms,
	.detect			= am_lcd_connector_detect,
	.fill_modes		= drm_helper_probe_single_connector_modes,
	.destroy		= drm_connector_cleanup,
	.reset			= drm_atomic_helper_connector_reset,
	.atomic_duplicate_state	= drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state	= drm_atomic_helper_connector_destroy_state,
};
#else

static int am_lcd_connector_dpms(struct drm_connector *connector, int mode)
{
	int ret = 0;

	pr_info("am_drm_lcd: %s %d\n", __func__, __LINE__);

	ret = drm_atomic_helper_connector_dpms(connector, mode);
	return ret;
}

static int am_lcd_connector_fill_modes(struct drm_connector *connector,
		uint32_t maxX, uint32_t maxY)
{
	int count = 0;

	pr_info("am_drm_lcd: %s %d\n", __func__, __LINE__);
	count = drm_helper_probe_single_connector_modes(connector, maxX, maxY);
	pr_info("am_drm_lcd: %s %d: count=%d\n", __func__, __LINE__, count);
	return count;
}

static void am_lcd_connector_destroy(struct drm_connector *connector)
{
	pr_info("am_drm_lcd: %s %d\n", __func__, __LINE__);

	drm_connector_cleanup(connector);
}

static void am_lcd_connector_reset(struct drm_connector *connector)
{
	pr_info("am_drm_lcd: %s %d\n", __func__, __LINE__);

	drm_atomic_helper_connector_reset(connector);
}

static struct drm_connector_state *am_lcd_connector_duplicate_state(
		struct drm_connector *connector)
{
	struct drm_connector_state *state;

	pr_info("am_drm_lcd: %s %d\n", __func__, __LINE__);

	state = drm_atomic_helper_connector_duplicate_state(connector);
	return state;
}

static void am_lcd_connector_destroy_state(struct drm_connector *connector,
					  struct drm_connector_state *state)
{
	pr_info("am_drm_lcd: %s %d\n", __func__, __LINE__);

	drm_atomic_helper_connector_destroy_state(connector, state);
}

static const struct drm_connector_funcs am_lcd_connector_funcs = {
	.dpms			= am_lcd_connector_dpms,
	.detect			= am_lcd_connector_detect,
	.fill_modes		= am_lcd_connector_fill_modes,
	.destroy		= am_lcd_connector_destroy,
	.reset			= am_lcd_connector_reset,
	.atomic_duplicate_state	= am_lcd_connector_duplicate_state,
	.atomic_destroy_state	= am_lcd_connector_destroy_state,
};
#endif

static void am_lcd_encoder_mode_set(struct drm_encoder *encoder,
				   struct drm_display_mode *mode,
				   struct drm_display_mode *adjusted_mode)
{
	pr_info("am_drm_lcd: %s %d\n", __func__, __LINE__);
}

static void am_lcd_encoder_enable(struct drm_encoder *encoder)
{
	enum vmode_e vmode = get_current_vmode();
	struct am_drm_lcd_s *lcd = encoder_to_lcd(encoder);

	if (!lcd)
		return;
	if (!lcd->lcd_drv)
		return;

	if (vmode == VMODE_LCD)
		DRM_INFO("am_lcd_encoder_enable\n");
	else
		DRM_INFO("am_lcd_encoder_enable fail! vmode:%d\n", vmode);

	pr_info("am_drm_lcd: %s %d\n", __func__, __LINE__);
	vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE_PRE, &vmode);
	mutex_lock(&lcd->lcd_drv->power_mutex);
	aml_lcd_notifier_call_chain(LCD_EVENT_PREPARE, NULL);
	aml_lcd_notifier_call_chain(LCD_EVENT_ENABLE, NULL);
	mutex_unlock(&lcd->lcd_drv->power_mutex);
	vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE, &vmode);
	pr_info("am_drm_lcd: %s %d\n", __func__, __LINE__);
}

static void am_lcd_encoder_disable(struct drm_encoder *encoder)
{
	struct am_drm_lcd_s *lcd = encoder_to_lcd(encoder);

	if (!lcd)
		return;
	if (!lcd->lcd_drv)
		return;

	pr_info("am_drm_lcd: %s %d\n", __func__, __LINE__);
	mutex_lock(&lcd->lcd_drv->power_mutex);
	aml_lcd_notifier_call_chain(LCD_EVENT_DISABLE, NULL);
	aml_lcd_notifier_call_chain(LCD_EVENT_UNPREPARE, NULL);
	mutex_unlock(&lcd->lcd_drv->power_mutex);
	pr_info("am_drm_lcd: %s %d\n", __func__, __LINE__);
}

static void am_lcd_encoder_commit(struct drm_encoder *encoder)
{
	pr_info("am_drm_lcd: %s %d\n", __func__, __LINE__);
}

static int am_lcd_encoder_atomic_check(struct drm_encoder *encoder,
				struct drm_crtc_state *crtc_state,
				struct drm_connector_state *conn_state)
{
	pr_info("am_drm_lcd: %s %d\n", __func__, __LINE__);
	return 0;
}

static const struct drm_encoder_helper_funcs am_lcd_encoder_helper_funcs = {
	.commit = am_lcd_encoder_commit,
	.mode_set = am_lcd_encoder_mode_set,
	.enable = am_lcd_encoder_enable,
	.disable = am_lcd_encoder_disable,
	.atomic_check = am_lcd_encoder_atomic_check,
};

static const struct drm_encoder_funcs am_lcd_encoder_funcs = {
	.destroy        = drm_encoder_cleanup,
};

static int am_lcd_disable(struct drm_panel *panel)
{
	struct am_drm_lcd_s *lcd = panel_to_lcd(panel);

	if (!lcd)
		return -ENODEV;
	if (!lcd->lcd_drv)
		return -ENODEV;

	pr_info("am_drm_lcd: %s %d\n", __func__, __LINE__);

	mutex_lock(&lcd->lcd_drv->power_mutex);
	aml_lcd_notifier_call_chain(LCD_EVENT_DISABLE, NULL);
	mutex_unlock(&lcd->lcd_drv->power_mutex);

	pr_info("am_drm_lcd: %s %d\n", __func__, __LINE__);

	return 0;
}

static int am_lcd_unprepare(struct drm_panel *panel)
{
	struct am_drm_lcd_s *lcd = panel_to_lcd(panel);

	if (!lcd)
		return -ENODEV;
	if (!lcd->lcd_drv)
		return -ENODEV;

	pr_info("am_drm_lcd: %s %d\n", __func__, __LINE__);

	mutex_lock(&lcd->lcd_drv->power_mutex);
	aml_lcd_notifier_call_chain(LCD_EVENT_UNPREPARE, NULL);
	mutex_unlock(&lcd->lcd_drv->power_mutex);

	pr_info("am_drm_lcd: %s %d\n", __func__, __LINE__);

	return 0;
}

static int am_lcd_prepare(struct drm_panel *panel)
{
	struct am_drm_lcd_s *lcd = panel_to_lcd(panel);

	if (!lcd)
		return -ENODEV;
	if (!lcd->lcd_drv)
		return -ENODEV;

	pr_info("am_drm_lcd: %s %d\n", __func__, __LINE__);

	mutex_lock(&lcd->lcd_drv->power_mutex);
	aml_lcd_notifier_call_chain(LCD_EVENT_PREPARE, NULL);
	mutex_unlock(&lcd->lcd_drv->power_mutex);

	pr_info("am_drm_lcd: %s %d\n", __func__, __LINE__);

	return 0;
}

static int am_lcd_enable(struct drm_panel *panel)
{
	struct am_drm_lcd_s *lcd = panel_to_lcd(panel);

	if (!lcd)
		return -ENODEV;
	if (!lcd->lcd_drv)
		return -ENODEV;

	pr_info("am_drm_lcd: %s %d\n", __func__, __LINE__);

	mutex_lock(&lcd->lcd_drv->power_mutex);
	aml_lcd_notifier_call_chain(LCD_EVENT_ENABLE, NULL);
	mutex_unlock(&lcd->lcd_drv->power_mutex);

	pr_info("am_drm_lcd: %s %d\n", __func__, __LINE__);

	return 0;
}

static int am_lcd_get_modes(struct drm_panel *panel)
{
	struct am_drm_lcd_s *lcd = panel_to_lcd(panel);
	struct drm_connector *connector = panel->connector;
	struct drm_device *drm = panel->drm;
	struct drm_display_mode *mode;
	struct lcd_config_s *pconf;

	if (!lcd->mode)
		return 0;

	mode = drm_mode_duplicate(drm, lcd->mode);
	if (!mode) {
		pr_err("error: am_drm_lcd: failed to add mode %ux%u@%u\n",
			mode->hdisplay, mode->vdisplay, mode->vrefresh);
	}

	pr_info("am_drm_lcd: %s %d\n", __func__, __LINE__);

	mode->type |= DRM_MODE_TYPE_DRIVER;
	mode->type |= DRM_MODE_TYPE_PREFERRED;

	drm_mode_set_name(mode);

	drm_mode_probed_add(connector, mode);

	pconf = lcd->lcd_drv->lcd_config;
	connector->display_info.bpc = pconf->lcd_basic.lcd_bits * 3;
	connector->display_info.width_mm = pconf->lcd_basic.screen_width;
	connector->display_info.height_mm = pconf->lcd_basic.screen_height;

	connector->display_info.bus_flags = DRM_BUS_FLAG_DE_HIGH;

	pr_info("am_drm_lcd: %s %d\n", __func__, __LINE__);

	return 1;
}

static int am_lcd_get_timings(struct drm_panel *panel,
				    unsigned int num_timings,
				    struct display_timing *timings)
{
	struct am_drm_lcd_s *lcd = panel_to_lcd(panel);

	if (!lcd)
		return 0;
	if (!lcd->timing)
		return 0;

	pr_info("am_drm_lcd: %s %d\n", __func__, __LINE__);

	num_timings = 1;

	if (timings)
		memcpy(&timings[0], lcd->timing, sizeof(struct display_timing));

	pr_info("am_drm_lcd: %s %d\n", __func__, __LINE__);

	return 1;
}

static const struct drm_panel_funcs am_drm_lcd_funcs = {
	.disable = am_lcd_disable,
	.unprepare = am_lcd_unprepare,
	.prepare = am_lcd_prepare,
	.enable = am_lcd_enable,
	.get_modes = am_lcd_get_modes,
	.get_timings = am_lcd_get_timings,
};

static void am_drm_lcd_display_mode_timing_init(struct am_drm_lcd_s *lcd)
{
	struct lcd_config_s *pconf;
	unsigned short tmp;

	if (!lcd->lcd_drv) {
		pr_info("invalid lcd driver\n");
		return;
	}

	pr_info("am_drm_lcd: %s %d\n", __func__, __LINE__);

	pconf = lcd->lcd_drv->lcd_config;

	lcd->mode = &am_lcd_mode;
	lcd->timing = &am_lcd_timing;

	lcd->mode->clock = pconf->lcd_timing.lcd_clk / 1000;
	lcd->mode->hdisplay = pconf->lcd_basic.h_active;
	tmp = pconf->lcd_basic.h_period - pconf->lcd_basic.h_active -
			pconf->lcd_timing.hsync_bp;
	lcd->mode->hsync_start = pconf->lcd_basic.h_active + tmp -
			pconf->lcd_timing.hsync_width;
	lcd->mode->hsync_end = pconf->lcd_basic.h_active + tmp;
	lcd->mode->htotal = pconf->lcd_basic.h_period;
	lcd->mode->vdisplay = pconf->lcd_basic.v_active;
	tmp = pconf->lcd_basic.v_period - pconf->lcd_basic.v_active -
			pconf->lcd_timing.vsync_bp;
	lcd->mode->vsync_start = pconf->lcd_basic.v_active + tmp -
			pconf->lcd_timing.vsync_width;
	lcd->mode->vsync_end = pconf->lcd_basic.v_active + tmp;
	lcd->mode->vtotal = pconf->lcd_basic.v_period;
	lcd->mode->width_mm = pconf->lcd_basic.screen_width;
	lcd->mode->height_mm = pconf->lcd_basic.screen_height;
	lcd->mode->vrefresh = pconf->lcd_timing.sync_duration_num /
				pconf->lcd_timing.sync_duration_den;

	lcd->timing->pixelclock.min = pconf->lcd_timing.lcd_clk;
	lcd->timing->pixelclock.typ = pconf->lcd_timing.lcd_clk;
	lcd->timing->pixelclock.max = pconf->lcd_timing.lcd_clk;
	lcd->timing->hactive.min = pconf->lcd_basic.h_active;
	lcd->timing->hactive.typ = pconf->lcd_basic.h_active;
	lcd->timing->hactive.max = pconf->lcd_basic.h_active;
	tmp = pconf->lcd_basic.h_period - pconf->lcd_basic.h_active -
		pconf->lcd_timing.hsync_bp - pconf->lcd_timing.hsync_width;
	lcd->timing->hfront_porch.min = tmp;
	lcd->timing->hfront_porch.typ = tmp;
	lcd->timing->hfront_porch.max = tmp;
	lcd->timing->hback_porch.min = pconf->lcd_timing.hsync_bp;
	lcd->timing->hback_porch.typ = pconf->lcd_timing.hsync_bp;
	lcd->timing->hback_porch.max = pconf->lcd_timing.hsync_bp;
	lcd->timing->hsync_len.min = pconf->lcd_timing.hsync_width;
	lcd->timing->hsync_len.typ = pconf->lcd_timing.hsync_width;
	lcd->timing->hsync_len.max = pconf->lcd_timing.hsync_width;
	lcd->timing->vactive.min = pconf->lcd_basic.v_active;
	lcd->timing->vactive.typ = pconf->lcd_basic.v_active;
	lcd->timing->vactive.max = pconf->lcd_basic.v_active;
	tmp = pconf->lcd_basic.v_period - pconf->lcd_basic.v_active -
		pconf->lcd_timing.vsync_bp - pconf->lcd_timing.vsync_width;
	lcd->timing->vfront_porch.min = tmp;
	lcd->timing->vfront_porch.typ = tmp;
	lcd->timing->vfront_porch.max = tmp;
	lcd->timing->vback_porch.min = pconf->lcd_timing.vsync_bp;
	lcd->timing->vback_porch.typ = pconf->lcd_timing.vsync_bp;
	lcd->timing->vback_porch.max = pconf->lcd_timing.vsync_bp;
	lcd->timing->vsync_len.min = pconf->lcd_timing.vsync_width;
	lcd->timing->vsync_len.typ = pconf->lcd_timing.vsync_width;
	lcd->timing->vsync_len.max = pconf->lcd_timing.vsync_width;

	pr_info("am_drm_lcd: %s: lcd config:\n"
		"lcd_clk             %d\n"
		"h_active            %d\n"
		"v_active            %d\n"
		"screen_width        %d\n"
		"screen_height       %d\n"
		"sync_duration_den   %d\n"
		"sync_duration_num   %d\n",
		__func__,
		lcd->lcd_drv->lcd_config->lcd_timing.lcd_clk,
		lcd->lcd_drv->lcd_config->lcd_basic.h_active,
		lcd->lcd_drv->lcd_config->lcd_basic.v_active,
		lcd->lcd_drv->lcd_config->lcd_basic.screen_width,
		lcd->lcd_drv->lcd_config->lcd_basic.screen_height,
		lcd->lcd_drv->lcd_config->lcd_timing.sync_duration_den,
		lcd->lcd_drv->lcd_config->lcd_timing.sync_duration_num);
	pr_info("am_drm_lcd: %s: display mode:\n"
		"clock       %d\n"
		"hdisplay    %d\n"
		"vdisplay    %d\n"
		"width_mm    %d\n"
		"height_mm   %d\n"
		"vrefresh    %d\n",
		__func__,
		lcd->mode->clock,
		lcd->mode->hdisplay,
		lcd->mode->vdisplay,
		lcd->mode->width_mm,
		lcd->mode->height_mm,
		lcd->mode->vrefresh);
	pr_info("am_drm_lcd: %s: timing:\n"
		"pixelclock   %d\n"
		"hactive      %d\n"
		"vactive      %d\n",
		__func__,
		lcd->timing->pixelclock.typ,
		lcd->timing->hactive.typ,
		lcd->timing->vactive.typ);

	pr_info("am_drm_lcd: %s %d\n", __func__, __LINE__);
}

static const struct of_device_id am_meson_lcd_dt_ids[] = {
	{ .compatible = "amlogic,drm-lcd", },
	{},
};

static int am_meson_lcd_bind(struct device *dev, struct device *master,
				    void *data)
{
	struct drm_device *drm = data;
	struct drm_connector *connector;
	struct drm_encoder *encoder;
	int encoder_type, connector_type;
	int ret = 0;

	am_drm_lcd = kzalloc(sizeof(*am_drm_lcd), GFP_KERNEL);
	if (!am_drm_lcd)
		return -ENOMEM;

	pr_info("am_drm_lcd: %s %d\n", __func__, __LINE__);

	am_drm_lcd->lcd_drv = aml_lcd_get_driver();
	if (!am_drm_lcd->lcd_drv) {
		pr_err("invalid lcd driver, exit\n");
		return -ENODEV;
	}

	am_drm_lcd_display_mode_timing_init(am_drm_lcd);

	drm_panel_init(&am_drm_lcd->panel);
	am_drm_lcd->panel.dev = NULL;
	am_drm_lcd->panel.funcs = &am_drm_lcd_funcs;

	pr_info("am_drm_lcd: %s %d\n", __func__, __LINE__);

	ret = drm_panel_add(&am_drm_lcd->panel);
	if (ret < 0)
		return ret;

	pr_info("am_drm_lcd: %s %d\n", __func__, __LINE__);

	am_drm_lcd->drm = drm;

	encoder = &am_drm_lcd->encoder;
	connector = &am_drm_lcd->connector;
	encoder_type = DRM_MODE_ENCODER_LVDS;
	connector_type = DRM_MODE_CONNECTOR_LVDS;

	/* Encoder */
	drm_encoder_helper_add(encoder, &am_lcd_encoder_helper_funcs);
	ret = drm_encoder_init(drm, encoder, &am_lcd_encoder_funcs,
			       encoder_type, "am_lcd_encoder");
	if (ret) {
		pr_err("error: am_drm_lcd: Failed to init lcd encoder\n");
		return ret;
	}
	pr_info("am_drm_lcd: %s %d: encoder possible_crtcs=%d\n",
		__func__, __LINE__, encoder->possible_crtcs);

	/* Connector */
	drm_connector_helper_add(connector, &am_lcd_connector_helper_funcs);
	ret = drm_connector_init(drm, connector, &am_lcd_connector_funcs,
				connector_type);
	if (ret) {
		pr_err("error: am_drm_lcd: Failed to init lcd connector\n");
		return ret;
	}

	/* force possible_crtcs */
	encoder->possible_crtcs = BIT(0);

	drm_mode_connector_attach_encoder(connector, encoder);

	pr_info("am_drm_lcd: register ok\n");

	return ret;
}

static void am_meson_lcd_unbind(struct device *dev, struct device *master,
				    void *data)
{
	if (!am_drm_lcd)
		return;

	if (!am_drm_lcd->lcd_drv)
		return;

	pr_info("am_drm_lcd: %s %d\n", __func__, __LINE__);

	drm_panel_detach(&am_drm_lcd->panel);
	drm_panel_remove(&am_drm_lcd->panel);

	pr_info("am_drm_lcd: %s %d\n", __func__, __LINE__);

	return;
}

static const struct component_ops am_meson_lcd_ops = {
	.bind	= am_meson_lcd_bind,
	.unbind	= am_meson_lcd_unbind,
};

static int am_meson_lcd_probe(struct platform_device *pdev)
{
	return component_add(&pdev->dev, &am_meson_lcd_ops);
}

static int am_meson_lcd_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &am_meson_lcd_ops);
	return 0;
}

static struct platform_driver am_meson_lcd_pltfm_driver = {
	.probe  = am_meson_lcd_probe,
	.remove = am_meson_lcd_remove,
	.driver = {
		.name = "meson-lcd",
		.of_match_table = am_meson_lcd_dt_ids,
	},
};

module_platform_driver(am_meson_lcd_pltfm_driver);

MODULE_AUTHOR("MultiMedia Amlogic <multimedia-sh@amlogic.com>");
MODULE_DESCRIPTION("Amlogic Meson Drm LCD driver");
MODULE_LICENSE("GPL");
