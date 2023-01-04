// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2016 InforceComputing
 * Author: Vinay Simha BN <simhavcs@gmail.com>
 *
 * Copyright (C) 2016 Linaro Ltd
 * Author: Sumit Semwal <sumit.semwal@linaro.org>
 *
 * From internet archives, the panel for Nexus 7 2nd Gen, 2013 model is a
 * stk model LT070ME05000, and its data sheet is at:
 * http://panelone.net/en/7-0-inch/stk_LT070ME05000_7.0_inch-datasheet
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>

#include <drm/drm_crtc.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

struct stk_panel {
	struct drm_panel base;
	struct mipi_dsi_device *dsi;

	struct gpio_desc *enable_gpio;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *dcdc_en_gpio;
	struct backlight_device *backlight;
	struct regulator *iovcc_supply;
	struct regulator *pp3300_supply;

	bool prepared;
	bool enabled;

	const struct drm_display_mode *mode;
};

static inline struct stk_panel *to_stk_panel(struct drm_panel *panel)
{
	return container_of(panel, struct stk_panel, base);
}

static int stk_panel_init(struct stk_panel *stk)
{
	struct mipi_dsi_device *dsi = stk->dsi;
	struct device *dev = &stk->dsi->dev;
	int ret;

	ret = mipi_dsi_dcs_soft_reset(dsi);
	if (ret < 0) {
		dev_err(dev, "failed to mipi_dsi_dcs_soft_reset: %d\n", ret);
		return ret;
	}

	mdelay(5);

	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "failed to set exit sleep mode: %d\n", ret);
		return ret;
	}
	msleep(120);

	ret = mipi_dsi_generic_write(dsi, (u8[]){0xB0, 0x04}, 2);
	if (ret < 0) {
		dev_err(dev, "failed to set mcap: %d\n", ret);
		return ret;
	}
	/* Interface setting, video mode */
	ret = mipi_dsi_generic_write(dsi,(u8[])
				     {  0xB3, 0x14, 0x08, 0x00, 0x22, 0x00}, 6);
	if (ret < 0) {
		dev_err(dev, "failed to set display interface setting: %d\n"
			, ret);
		return ret;
	}

	ret = mipi_dsi_generic_write(dsi,  (u8[]){0xB4, 0x0C, 0x00}, 3);
	if (ret < 0) {
		dev_err(dev, "failed to set Interface ID setting: %d\n", ret);
		return ret;
	}

	ret = mipi_dsi_generic_write(dsi, (u8[]){ 0xB6, 0x3A, 0xD3},3);
	if (ret < 0) {
		dev_err(dev, "failed to set DSI control: %d\n", ret);
		return ret;
	}

	ret = mipi_dsi_dcs_set_display_brightness(dsi, 0x77);
	if (ret < 0) {
		dev_err(dev, "failed to write display brightness: %d\n", ret);
		return ret;
	}

	ret = mipi_dsi_dcs_write(dsi, MIPI_DCS_WRITE_CONTROL_DISPLAY,
				 (u8[]){ 0x2C }, 1);
	if (ret < 0) {
		dev_err(dev, "failed to write control display: %d\n", ret);
		return ret;
	}

	ret = mipi_dsi_dcs_set_pixel_format(dsi, 0x77);
	if (ret < 0) {
		dev_err(dev, "failed to set pixel format: %d\n", ret);
		return ret;
	}

	ret = mipi_dsi_dcs_set_column_address(dsi, 0, stk->mode->hdisplay - 1);
	if (ret < 0) {
		dev_err(dev, "failed to set column address: %d\n", ret);
		return ret;
	}

	ret = mipi_dsi_dcs_set_page_address(dsi, 0, stk->mode->vdisplay - 1);
	if (ret < 0) {
		dev_err(dev, "failed to set page address: %d\n", ret);
		return ret;
	}

	return 0;
}

static int stk_panel_on(struct stk_panel *stk)
{
	struct mipi_dsi_device *dsi = stk->dsi;
	struct device *dev = &stk->dsi->dev;
	int ret;

	ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret < 0)
		dev_err(dev, "failed to set display on: %d\n", ret);

	mdelay(20);

	return ret;
}

static void stk_panel_off(struct stk_panel *stk)
{
	struct mipi_dsi_device *dsi = stk->dsi;
	struct device *dev = &stk->dsi->dev;
	int ret;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret < 0)
		dev_err(dev, "failed to set display off: %d\n", ret);

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret < 0)
		dev_err(dev, "failed to enter sleep mode: %d\n", ret);

	msleep(100);
}

static int stk_panel_disable(struct drm_panel *panel)
{
	struct stk_panel *stk = to_stk_panel(panel);

	if (!stk->enabled)
		return 0;

	backlight_disable(stk->backlight);

	stk->enabled = false;

	return 0;
}

static int stk_panel_unprepare(struct drm_panel *panel)
{
	struct stk_panel *stk = to_stk_panel(panel);

	if (!stk->prepared)
		return 0;

	stk_panel_off(stk);
	regulator_disable(stk->iovcc_supply);
	regulator_disable(stk->pp3300_supply);
	gpiod_set_value(stk->enable_gpio, 1);
	gpiod_set_value(stk->reset_gpio, 0);
	gpiod_set_value(stk->dcdc_en_gpio, 1);

	stk->prepared = false;

	return 0;
}

static int stk_panel_prepare(struct drm_panel *panel)
{
	struct stk_panel *stk = to_stk_panel(panel);
	struct device *dev = &stk->dsi->dev;
	int ret;

	if (stk->prepared)
		return 0;

	gpiod_set_value(stk->reset_gpio, 0);
	gpiod_set_value(stk->dcdc_en_gpio, 0);
	gpiod_set_value(stk->enable_gpio, 0);
	ret = regulator_enable(stk->pp3300_supply);
	if (ret < 0)
		return ret;
	ret = regulator_enable(stk->iovcc_supply);
	if (ret < 0)
		return ret;
	mdelay(8);
	mdelay(20);
	gpiod_set_value(stk->enable_gpio, 1);
	mdelay(20);
	gpiod_set_value(stk->dcdc_en_gpio, 1);
	mdelay(20);
	gpiod_set_value(stk->reset_gpio, 1);
	mdelay(10);

	ret = stk_panel_init(stk);
	if (ret < 0) {
		dev_err(dev, "failed to init panel: %d\n", ret);
		goto poweroff;
	}

	ret = stk_panel_on(stk);
	if (ret < 0) {
		dev_err(dev, "failed to set panel on: %d\n", ret);
		goto poweroff;
	}

	stk->prepared = true;

	return 0;

poweroff:
	regulator_disable(stk->iovcc_supply);
	regulator_disable(stk->pp3300_supply);
	gpiod_set_value(stk->enable_gpio, 0);
	gpiod_set_value(stk->reset_gpio, 0);
	gpiod_set_value(stk->dcdc_en_gpio, 0);

	return ret;
}

static int stk_panel_enable(struct drm_panel *panel)
{
	struct stk_panel *stk = to_stk_panel(panel);

	if (stk->enabled)
		return 0;

	backlight_enable(stk->backlight);

	stk->enabled = true;

	return 0;
}

static const struct drm_display_mode default_mode = {
		.clock = 163204,
		.hdisplay = 1200,
		.hsync_start = 1200 + 144,
		.hsync_end = 1200 + 144 + 16,
		.htotal = 1200 + 144 + 16 + 45, // 1405
		.vdisplay = 1920,
		.vsync_start = 1920 + 8,
		.vsync_end = 1920 + 8 + 4,
		.vtotal = 1920 + 8 + 4 + 4, // 1936
};

static int stk_panel_get_modes(struct drm_panel *panel,
				 struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode) {
		dev_err(panel->dev, "failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode);

	drm_mode_probed_add(connector, mode);

	connector->display_info.width_mm = 95;
	connector->display_info.height_mm = 151;

	return 1;
}

static int dsi_dcs_bl_get_brightness(struct backlight_device *bl)
{
	struct mipi_dsi_device *dsi = bl_get_data(bl);
	int ret;
	u16 brightness = bl->props.brightness;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_get_display_brightness(dsi, &brightness);
	if (ret < 0)
		return ret;
	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	return brightness & 0xff;
}

static int dsi_dcs_bl_update_status(struct backlight_device *bl)
{
	struct mipi_dsi_device *dsi = bl_get_data(bl);
	struct device *dev = &dsi->dev;
	int ret;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_set_display_brightness(dsi, bl->props.brightness);
	if (ret < 0) {
		dev_err(dev, "failed to set DSI control: %d\n", ret);
		return ret;
	}

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	return 0;
}

static const struct backlight_ops dsi_bl_ops = {
	.update_status = dsi_dcs_bl_update_status,
	.get_brightness = dsi_dcs_bl_get_brightness,
};

static struct backlight_device *
drm_panel_create_dsi_backlight(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct backlight_properties props;

	memset(&props, 0, sizeof(props));
	props.type = BACKLIGHT_RAW;
	props.brightness = 255;
	props.max_brightness = 255;

	return devm_backlight_device_register(dev, dev_name(dev), dev, dsi,
					      &dsi_bl_ops, &props);
}

static const struct drm_panel_funcs stk_panel_funcs = {
	.disable = stk_panel_disable,
	.unprepare = stk_panel_unprepare,
	.prepare = stk_panel_prepare,
	.enable = stk_panel_enable,
	.get_modes = stk_panel_get_modes,
};

static const struct of_device_id stk_of_match[] = {
	{ .compatible = "startek,kd070fhfid015", },
	{ }
};
MODULE_DEVICE_TABLE(of, stk_of_match);

static int stk_panel_add(struct stk_panel *stk)
{
	struct device *dev = &stk->dsi->dev;
	int ret;

	stk->mode = &default_mode;

	stk->iovcc_supply = devm_regulator_get(dev, "iovcc");
	if (IS_ERR(stk->iovcc_supply))
		return PTR_ERR(stk->iovcc_supply);

	stk->pp3300_supply = devm_regulator_get(dev, "pp3300");
	if (IS_ERR(stk->pp3300_supply))
		return PTR_ERR(stk->pp3300_supply);

	stk->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(stk->reset_gpio)) {
		ret = PTR_ERR(stk->reset_gpio);
		dev_err(dev, "cannot get reset-gpios %d\n", ret);
		return ret;
	}

	stk->dcdc_en_gpio = devm_gpiod_get(dev, "dcdc", GPIOD_OUT_LOW);
	if (IS_ERR(stk->dcdc_en_gpio)) {
		ret = PTR_ERR(stk->dcdc_en_gpio);
		dev_err(dev, "cannot get dcdc-en-gpio %d\n", ret);
		return ret;
	}

	stk->enable_gpio = devm_gpiod_get(dev, "enable", GPIOD_OUT_LOW);
	if (IS_ERR(stk->enable_gpio)) {
		ret = PTR_ERR(stk->enable_gpio);
		dev_err(dev, "cannot get enable-gpio %d\n", ret);
		return ret;
	}

	stk->backlight = drm_panel_create_dsi_backlight(stk->dsi);
	if (IS_ERR(stk->backlight)) {
		ret = PTR_ERR(stk->backlight);
		dev_err(dev, "failed to register backlight %d\n", ret);
		return ret;
	}

	drm_panel_init(&stk->base, &stk->dsi->dev, &stk_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&stk->base);

	return 0;
}

static void stk_panel_del(struct stk_panel *stk)
{
	if (stk->base.dev)
		drm_panel_remove(&stk->base);
}

static int stk_panel_probe(struct mipi_dsi_device *dsi)
{
	struct stk_panel *stk;
	int ret;

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = (MIPI_DSI_MODE_VIDEO |
			   MIPI_DSI_MODE_LPM);

	stk = devm_kzalloc(&dsi->dev, sizeof(*stk), GFP_KERNEL);
	if (!stk)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, stk);

	stk->dsi = dsi;

	ret = stk_panel_add(stk);
	if (ret < 0)
		return ret;

	return mipi_dsi_attach(dsi);
}

static int stk_panel_remove(struct mipi_dsi_device *dsi)
{
	struct stk_panel *stk = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = stk_panel_disable(&stk->base);
	if (ret < 0)
		dev_err(&dsi->dev, "failed to disable panel: %d\n", ret);

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "failed to detach from DSI host: %d\n",
			ret);

	stk_panel_del(stk);

	return 0;
}

static void stk_panel_shutdown(struct mipi_dsi_device *dsi)
{
	struct stk_panel *stk = mipi_dsi_get_drvdata(dsi);

	stk_panel_disable(&stk->base);
}

static struct mipi_dsi_driver stk_panel_driver = {
	.driver = {
		.name = "panel-startek-kd070fhfid015",
		.of_match_table = stk_of_match,
	},
	.probe = stk_panel_probe,
	.remove = stk_panel_remove,
	.shutdown = stk_panel_shutdown,
};
module_mipi_dsi_driver(stk_panel_driver);

MODULE_AUTHOR("Guillaume La Roque <glaroque@baylibre.com>");
MODULE_DESCRIPTION("STARTEK KD070FHFID015");
MODULE_LICENSE("GPL v2");
