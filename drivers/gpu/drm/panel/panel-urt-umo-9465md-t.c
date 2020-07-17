// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Mediatek
 * Author: Andrew Perepech <andrew.perepech@mediatek.com>
 *
 * Based on panel-truly-r63350a driver.
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>

#include <drm/drm_crtc.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>

#include <video/mipi_display.h>


struct lcm_init_struct {
	u8 cmd;
	u8 count;
	u8 params[64];
};

static const struct lcm_init_struct lcm_init[] = {
	{0xB0, 1, {0x00} },
	{0xB1, 1, {0x21} },
	{0xB2, 1, {0x4B} },
	{0xB3, 1, {0x68} },
	{0xB7, 1, {0x73} },
	{0xBA, 1, {0xAB} },
	{0xBB, 1, {0xE8} },
	{0xBE, 1, {0x3F} },
	{0xBD, 1, {0xFF} },
	{0xC3, 1, {0x50} },
	{0xC4, 1, {0x19} },
	{0xC5, 1, {0xB9} },
	{0xC6, 1, {0x19} },
	{0xC7, 1, {0x15} },
	{0xCA, 1, {0x00} },
	{0xCB, 1, {0x3F} },
	{0xCC, 1, {0x30} },
	{0xCD, 1, {0x27} },
	{0xCE, 1, {0x27} },
	{0xCF, 1, {0x16} },
	{0xD0, 1, {0x0E} },
	{0xD1, 1, {0x0E} },
	{0xD2, 1, {0x13} },
	{0xD3, 1, {0x14} },
	{0xD4, 1, {0x14} },
	{0xD5, 1, {0x0A} },
	{0xD6, 1, {0x3F} },
	{0xD7, 1, {0x30} },
	{0xD8, 1, {0x27} },
	{0xD9, 1, {0x27} },
	{0xDA, 1, {0x16} },
	{0xDB, 1, {0x0E} },
	{0xDC, 1, {0x0E} },
	{0xDD, 1, {0x13} },
	{0xDE, 1, {0x14} },
	{0xDF, 1, {0x14} },
	{0xE0, 1, {0x0A} },
	{0xE5, 1, {0x52} },
	{0xB0, 1, {0x02} },
	{0xB1, 1, {0x25} },
	{0xB2, 1, {0x00} },
	{0xB3, 1, {0x11} },
	{0xB4, 1, {0x12} },
	{0xB5, 1, {0x09} },
	{0xB6, 1, {0x0B} },
	{0xB7, 1, {0x05} },
	{0xB8, 1, {0x07} },
	{0xB9, 1, {0x01} },
	{0xBA, 1, {0x00} },
	{0xBB, 1, {0x00} },
	{0xBC, 1, {0x00} },
	{0xBD, 1, {0x00} },
	{0xBE, 1, {0x00} },
	{0xBF, 1, {0x00} },
	{0xC0, 1, {0x03} },
	{0xC1, 1, {0x00} },
	{0xC2, 1, {0x00} },
	{0xC3, 1, {0x00} },
	{0xC4, 1, {0x00} },
	{0xC5, 1, {0x00} },
	{0xC6, 1, {0x00} },
	{0xC7, 1, {0x25} },
	{0xC8, 1, {0x00} },
	{0xC9, 1, {0x11} },
	{0xCA, 1, {0x12} },
	{0xCB, 1, {0x0A} },
	{0xCC, 1, {0x0C} },
	{0xCD, 1, {0x06} },
	{0xCE, 1, {0x08} },
	{0xCF, 1, {0x02} },
	{0xD0, 1, {0x00} },
	{0xD1, 1, {0x00} },
	{0xD2, 1, {0x00} },
	{0xD3, 1, {0x00} },
	{0xD4, 1, {0x00} },
	{0xD5, 1, {0x00} },
	{0xD6, 1, {0x04} },
	{0xD7, 1, {0x00} },
	{0xD8, 1, {0x00} },
	{0xD9, 1, {0x00} },
	{0xDA, 1, {0x00} },
	{0xDB, 1, {0x00} },
	{0xDC, 1, {0x00} },
	{0xB0, 1, {0x03} },
	{0xC4, 1, {0x0B} },
	{0xC6, 1, {0x07} },
	{0xC7, 1, {0x01} },
	{0xCA, 1, {0x80} },
	{0xCB, 1, {0x0A} },
	{0xCC, 1, {0x00} },
};

struct umo_r_panel {
	struct drm_panel base;
	struct mipi_dsi_device *dsi;

	struct gpio_desc *reset_gpio;
	struct gpio_desc *pwr_gpio;
	struct gpio_desc *pwr2_gpio;

	struct backlight_device *backlight;

	bool prepared;
	bool enabled;

	const struct drm_display_mode *mode;
};

static inline struct umo_r_panel *to_umo_r_panel(struct drm_panel *panel)
{
	return container_of(panel, struct umo_r_panel, base);
}


static ssize_t umo_r_write_buffer(struct mipi_dsi_device *dsi, u8 cmd,
			       const void *data, size_t len)
{
	ssize_t err;
	size_t size;
	u8 *tx;

	if (len > 0) {
		size = len + 1;

		tx = kmalloc(size, GFP_KERNEL);
		if (!tx)
			return -ENOMEM;

		tx[0] = cmd;
		memcpy(&tx[1], data, len);
	} else {
		tx = &cmd;
		size = 1;
	}

	if (cmd < 0xB0)
		err = mipi_dsi_dcs_write_buffer(dsi, tx, size);
	else
		err = mipi_dsi_generic_write(dsi, tx, size);

	if (len > 0)
		kfree(tx);

	return err;
}

static ssize_t umo_r_push_table(struct mipi_dsi_device *dsi,
				  const struct lcm_init_struct *table,
				  size_t len)
{
	ssize_t err;
	size_t i;

	for (i = 0; i < len; i++) {
		err = umo_r_write_buffer(dsi, table[i].cmd,
					   table[i].params, table[i].count);
		if (err < 0)
			return err;
	}

	return err;
}

static int umo_r_panel_on(struct umo_r_panel *umo_r)
{
	struct mipi_dsi_device *dsi = umo_r->dsi;
	int ret;


	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	ret = umo_r_push_table(dsi, lcm_init, ARRAY_SIZE(lcm_init));
	if (ret < 0)
		return ret;

	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret < 0)
		return ret;

	msleep(300);

	ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret < 0)
		return ret;

	return 0;
}

static int umo_r_panel_off(struct umo_r_panel *umo_r)
{
	struct mipi_dsi_device *dsi = umo_r->dsi;
	int ret;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret < 0)
		return ret;

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret < 0)
		return ret;

	return 0;
}


static int umo_r_panel_disable(struct drm_panel *panel)
{
	struct umo_r_panel *umo_r = to_umo_r_panel(panel);

	if (!umo_r->enabled)
		return 0;

	backlight_disable(umo_r->backlight);

	umo_r->enabled = false;

	return 0;
}

static int umo_r_panel_unprepare(struct drm_panel *panel)
{
	struct umo_r_panel *umo_r = to_umo_r_panel(panel);
	int ret;

	if (!umo_r->prepared)
		return 0;

	ret = umo_r_panel_off(umo_r);
	if (ret < 0) {
		dev_err(panel->dev, "failed to set panel off: %d\n", ret);
		return ret;
	}

	gpiod_set_value(umo_r->pwr2_gpio, 0);
	gpiod_set_value(umo_r->pwr_gpio, 0);
	gpiod_set_value(umo_r->reset_gpio, 0);

	umo_r->prepared = false;

	return 0;
}

static int umo_r_panel_prepare(struct drm_panel *panel)
{
	struct umo_r_panel *umo_r = to_umo_r_panel(panel);
	int ret;

	if (umo_r->prepared)
		return 0;

	gpiod_set_value(umo_r->pwr_gpio, 1);
	gpiod_set_value(umo_r->pwr2_gpio, 1);

	msleep(20);

	gpiod_set_value(umo_r->reset_gpio, 1);
	msleep(1);
	gpiod_set_value(umo_r->reset_gpio, 0);
	msleep(1);
	gpiod_set_value(umo_r->reset_gpio, 1);
	msleep(10);

	ret = umo_r_panel_on(umo_r);
	if (ret < 0) {
		dev_err(panel->dev, "failed to set panel on: %d\n", ret);
		goto poweroff;
	}

	umo_r->prepared = true;

	return 0;

poweroff:
	gpiod_set_value(umo_r->pwr2_gpio, 0);
	gpiod_set_value(umo_r->pwr_gpio, 0);
	gpiod_set_value(umo_r->reset_gpio, 0);

	return ret;
}

static int umo_r_panel_enable(struct drm_panel *panel)
{
	struct umo_r_panel *umo_r = to_umo_r_panel(panel);

	if (umo_r->enabled)
		return 0;

	backlight_enable(umo_r->backlight);

	umo_r->enabled = true;

	return 0;
}

static const struct drm_display_mode default_mode = {
	.clock = 72500,
	.hdisplay = 800,
	.hsync_start = 800 + 72,
	.hsync_end = 800 + 72 + 24,
	.htotal = 800 + 72 + 24 + 24,
	.vdisplay = 1280,
	.vsync_start = 1280 + 14,
	.vsync_end = 1280 + 14 + 2,
	.vtotal = 1280 + 14 + 2 + 18,
};

static int umo_r_panel_get_modes(struct drm_panel *panel,
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

	connector->display_info.width_mm = 135;
	connector->display_info.height_mm = 217;

	return 1;
}

static const struct drm_panel_funcs umo_r_panel_funcs = {
	.disable = umo_r_panel_disable,
	.unprepare = umo_r_panel_unprepare,
	.prepare = umo_r_panel_prepare,
	.enable = umo_r_panel_enable,
	.get_modes = umo_r_panel_get_modes,
};

static int umo_r_panel_add(struct umo_r_panel *umo_r)
{
	struct device *dev = &umo_r->dsi->dev;

	umo_r->mode = &default_mode;
	umo_r->reset_gpio = devm_gpiod_get(dev, "reset",
					      GPIOD_OUT_LOW);
	if (IS_ERR(umo_r->reset_gpio)) {
		dev_err(dev, "cannot get reset-gpios %ld\n",
			PTR_ERR(umo_r->reset_gpio));
		return PTR_ERR(umo_r->reset_gpio);
	}

	umo_r->pwr_gpio = devm_gpiod_get(dev, "pwr",
					    GPIOD_OUT_LOW);
	if (IS_ERR(umo_r->pwr_gpio)) {
		dev_err(dev, "cannot get pwr-gpios %ld\n",
			PTR_ERR(umo_r->pwr_gpio));
		return PTR_ERR(umo_r->pwr_gpio);
	}

	umo_r->pwr2_gpio = devm_gpiod_get(dev, "pwr2",
					     GPIOD_OUT_LOW);
	if (IS_ERR(umo_r->pwr_gpio)) {
		dev_err(dev, "cannot get pwr2-gpios %ld\n",
			PTR_ERR(umo_r->pwr2_gpio));
		return PTR_ERR(umo_r->pwr2_gpio);
	}

	umo_r->backlight = devm_of_find_backlight(dev);
	if (IS_ERR(umo_r->backlight)) {
		dev_err(dev, "failed to get backlight\n");
		return PTR_ERR(umo_r->backlight);
	}

	drm_panel_init(&umo_r->base, &umo_r->dsi->dev, &umo_r_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&umo_r->base);

	return 0;
}

static void umo_r_panel_del(struct umo_r_panel *umo_r)
{
	if (umo_r->base.dev)
		drm_panel_remove(&umo_r->base);
}

static int umo_r_panel_probe(struct mipi_dsi_device *dsi)
{
	struct umo_r_panel *umo_r;
	int ret;

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO |
			MIPI_DSI_MODE_VIDEO_HSE |
			MIPI_DSI_CLOCK_NON_CONTINUOUS |
			MIPI_DSI_MODE_NO_EOT_PACKET;

	umo_r = devm_kzalloc(&dsi->dev, sizeof(*umo_r), GFP_KERNEL);
	if (!umo_r)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, umo_r);
	umo_r->dsi = dsi;

	ret = umo_r_panel_add(umo_r);
	if (ret < 0)
		return ret;

	return mipi_dsi_attach(dsi);
}

static int umo_r_panel_remove(struct mipi_dsi_device *dsi)
{
	struct umo_r_panel *umo_r = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = umo_r_panel_disable(&umo_r->base);
	if (ret < 0)
		dev_err(&dsi->dev, "failed to disable panel: %d\n", ret);

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "failed to detach from DSI host: %d\n", ret);

	umo_r_panel_del(umo_r);

	return 0;
}

static void umo_r_panel_shutdown(struct mipi_dsi_device *dsi)
{
	struct umo_r_panel *umo_r = mipi_dsi_get_drvdata(dsi);

	umo_r_panel_disable(&umo_r->base);
}

static const struct of_device_id umo_r_of_match[] = {
	{ .compatible = "urt,umo-9465md-t", },
	{ }
};
MODULE_DEVICE_TABLE(of, umo_r_of_match);

static struct mipi_dsi_driver umo_r_panel_driver = {
	.driver = {
		.name = "panel-urt-umo-9465md-t",
		.of_match_table = umo_r_of_match,
	},
	.probe = umo_r_panel_probe,
	.remove = umo_r_panel_remove,
	.shutdown = umo_r_panel_shutdown,
};
module_mipi_dsi_driver(umo_r_panel_driver);

MODULE_AUTHOR("Andrew Perepech <andrew.perepech@mediatek.com>");
MODULE_DESCRIPTION("U.R.T. UMO-9465MD-T DSI Panel Driver");
MODULE_LICENSE("GPL v2");
