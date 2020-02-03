// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Mediatek
 * Author: Andrew Perepech <andrew.perepech@mediatek.com>
 *
 * Based on panel-sharp-nt35532 driver.
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
	{0xD6, 1, {0x01} },
	{0xB3, 6, {0x14, 0x00, 0x00, 0x00, 0x00, 0x00} },
	{0xB4, 2, {0x0C, 0x00} },
	{0xB6, 3, {0x4B, 0xDB, 0x16} },/* 0xCB,0x16 */
	{0xBE, 2, {0x00, 0x04} },
	{0xC0, 1, {0x00} },
	{0xC1, 34, {0x04, 0x60, 0x00, 0x20, 0xA9, 0x30, 0x20, 0x63,
				0xF0, 0xFF, 0xFF, 0x9B, 0x7B, 0xCF, 0xB5, 0xFF,
				0xFF, 0x87, 0x8C, 0x41, 0x22, 0x54, 0x02, 0x00,
				0x00, 0x00, 0x00, 0x00, 0x22, 0x33, 0x03, 0x22,
				0x00, 0xFF} },
	{0xC2, 8, {0x31, 0xf7, 0x80, 0x06, 0x04, 0x00, 0x00, 0x08} },
	{0xC3, 3, {0x00, 0x00, 0x00} },
	{0xC4, 11, {0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x02} },
	/* reduce noise*/
	{0xC5, 1, {0x00} },
	{0xC6, 21, {0xC8, 0x3C, 0x3C, 0x07, 0x01, 0x07, 0x01, 0x00,
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				0x00, 0x0E, 0x1A, 0x07, 0xC8} },
	{0xC7, 30, {0x03, 0x15, 0x1F, 0x2A, 0x39, 0x46, 0x4E, 0x5B,
				0x3D, 0x45, 0x52, 0x5F, 0x68, 0x6D, 0x72, 0x01,
				0x15, 0x1F, 0x2A, 0x39, 0x46, 0x4E, 0x5B, 0x3D,
				0x45, 0x52, 0x5F, 0x68, 0x6D, 0x78} },
	{0xCB, 15, {0xFF, 0xE1, 0x87, 0xFF, 0x00, 0x00, 0x00, 0x00,
				0xFF, 0xE1, 0x87, 0xFF, 0xE8, 0x00, 0x00} },
	{0xCC, 1, {0x34} },
	{0xD0, 10, {0x11, 0x00, 0x00, 0x56, 0xD5, 0x40, 0x19, 0x19,
				0x09, 0x00} },
	{0xD1, 4, {0x00, 0x48, 0x16, 0x0F} },
	{0xD2, 3, {0x5C, 0x00, 0x00} },
	{0xD3, 26, {0x1B, 0x33, 0xBB, 0xBB, 0xB3, 0x33, 0x33, 0x33,
				0x33, 0x00, 0x01, 0x00, 0x00, 0xD8, 0xA0, 0x0C,
				0x4D, 0x4D, 0x33, 0x33, 0x72, 0x12, 0x8A, 0x57,
				0x3D, 0xBC} },
	{0xD5, 7, {0x06, 0x00, 0x00, 0x01, 0x39, 0x01, 0x39} },
	{0xD8, 3, {0x00, 0x00, 0x00} },
	{0xD9, 3, {0x00, 0x00, 0x00} },
	{0xFD, 4, {0x00, 0x00, 0x00, 0x30} },
	{0x35, 1, {0x00} },
	/* Test revert */
	/* {0x36, 1, {0xC0} }, */
	{0x51, 1, {0xff} },
	/*  Write CTRL Display */
	{0x53, 1, {0x24} },
	/* Write Display Brightness */
	{0x55, 1, {0x00} },

};

struct truly_r_panel {
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

static inline struct truly_r_panel *to_truly_r_panel(struct drm_panel *panel)
{
	return container_of(panel, struct truly_r_panel, base);
}


static ssize_t truly_r_write_buffer(struct mipi_dsi_device *dsi, u8 cmd,
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

static ssize_t truly_r_push_table(struct mipi_dsi_device *dsi,
				  const struct lcm_init_struct *table,
				  size_t len)
{
	ssize_t err;
	size_t i;

	for (i = 0; i < len; i++) {
		err = truly_r_write_buffer(dsi, table[i].cmd,
					   table[i].params, table[i].count);
		if (err < 0)
			return err;
	}

	return err;
}

static int truly_r_panel_on(struct truly_r_panel *truly_r)
{
	struct mipi_dsi_device *dsi = truly_r->dsi;
	int ret;


	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	ret = truly_r_push_table(dsi, lcm_init, ARRAY_SIZE(lcm_init));
	if (ret < 0)
		return ret;

	ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret < 0)
		return ret;

	msleep(50);

	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret < 0)
		return ret;

	msleep(120);

	return 0;
}

static int truly_r_panel_off(struct truly_r_panel *truly_r)
{
	struct mipi_dsi_device *dsi = truly_r->dsi;
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


static int truly_r_panel_disable(struct drm_panel *panel)
{
	struct truly_r_panel *truly_r = to_truly_r_panel(panel);

	if (!truly_r->enabled)
		return 0;

	backlight_disable(truly_r->backlight);

	truly_r->enabled = false;

	return 0;
}

static int truly_r_panel_unprepare(struct drm_panel *panel)
{
	struct truly_r_panel *truly_r = to_truly_r_panel(panel);
	int ret;

	if (!truly_r->prepared)
		return 0;

	ret = truly_r_panel_off(truly_r);
	if (ret < 0) {
		dev_err(panel->dev, "failed to set panel off: %d\n", ret);
		return ret;
	}

	gpiod_set_value(truly_r->pwr2_gpio, 0);
	gpiod_set_value(truly_r->pwr_gpio, 0);
	gpiod_set_value(truly_r->reset_gpio, 0);

	truly_r->prepared = false;

	return 0;
}

static int truly_r_panel_prepare(struct drm_panel *panel)
{
	struct truly_r_panel *truly_r = to_truly_r_panel(panel);
	int ret;

	if (truly_r->prepared)
		return 0;

	gpiod_set_value(truly_r->pwr_gpio, 1);
	gpiod_set_value(truly_r->pwr2_gpio, 1);

	msleep(20);

	gpiod_set_value(truly_r->reset_gpio, 1);
	msleep(1);
	gpiod_set_value(truly_r->reset_gpio, 0);
	msleep(1);
	gpiod_set_value(truly_r->reset_gpio, 1);
	msleep(10);

	ret = truly_r_panel_on(truly_r);
	if (ret < 0) {
		dev_err(panel->dev, "failed to set panel on: %d\n", ret);
		goto poweroff;
	}

	truly_r->prepared = true;

	return 0;

poweroff:
	gpiod_set_value(truly_r->pwr2_gpio, 0);
	gpiod_set_value(truly_r->pwr_gpio, 0);
	gpiod_set_value(truly_r->reset_gpio, 0);

	return ret;
}

static int truly_r_panel_enable(struct drm_panel *panel)
{
	struct truly_r_panel *truly_r = to_truly_r_panel(panel);

	if (truly_r->enabled)
		return 0;

	backlight_enable(truly_r->backlight);

	truly_r->enabled = true;

	return 0;
}

static const struct drm_display_mode default_mode = {
//	.clock = 148500,
	.clock = 145200,            //1250 * 1936 *60 /1000
	.hdisplay = 1080,
	.hsync_start = 1080 + 90,
	.hsync_end = 1080 + 90 + 20,
	.htotal = 1080 + 90 + 20 + 60,
	.vdisplay = 1920,
	.vsync_start = 1920 + 10,
	.vsync_end = 1920 + 10 + 2,
	.vtotal = 1920 + 10 + 2 + 4,
};

static int truly_r_panel_get_modes(struct drm_panel *panel,
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

	connector->display_info.width_mm = 68;
	connector->display_info.height_mm = 120;

	return 1;
}

static const struct drm_panel_funcs truly_r_panel_funcs = {
	.disable = truly_r_panel_disable,
	.unprepare = truly_r_panel_unprepare,
	.prepare = truly_r_panel_prepare,
	.enable = truly_r_panel_enable,
	.get_modes = truly_r_panel_get_modes,
};

static int truly_r_panel_add(struct truly_r_panel *truly_r)
{
	struct device *dev = &truly_r->dsi->dev;

	truly_r->mode = &default_mode;
	truly_r->reset_gpio = devm_gpiod_get(dev, "reset",
					      GPIOD_OUT_LOW);
	if (IS_ERR(truly_r->reset_gpio)) {
		dev_err(dev, "cannot get reset-gpios %ld\n",
			PTR_ERR(truly_r->reset_gpio));
		return PTR_ERR(truly_r->reset_gpio);
	}

	truly_r->pwr_gpio = devm_gpiod_get(dev, "pwr",
					    GPIOD_OUT_LOW);
	if (IS_ERR(truly_r->pwr_gpio)) {
		dev_err(dev, "cannot get pwr-gpios %ld\n",
			PTR_ERR(truly_r->pwr_gpio));
		return PTR_ERR(truly_r->pwr_gpio);
	}

	truly_r->pwr2_gpio = devm_gpiod_get(dev, "pwr2",
					     GPIOD_OUT_LOW);
	if (IS_ERR(truly_r->pwr_gpio)) {
		dev_err(dev, "cannot get pwr2-gpios %ld\n",
			PTR_ERR(truly_r->pwr2_gpio));
		return PTR_ERR(truly_r->pwr2_gpio);
	}

	truly_r->backlight = devm_of_find_backlight(dev);
	if (IS_ERR(truly_r->backlight)) {
		dev_err(dev, "failed to get backlight\n");
		return PTR_ERR(truly_r->backlight);
	}

	drm_panel_init(&truly_r->base, &truly_r->dsi->dev, &truly_r_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&truly_r->base);

	return 0;
}

static void truly_r_panel_del(struct truly_r_panel *truly_r)
{
	if (truly_r->base.dev)
		drm_panel_remove(&truly_r->base);
}

static int truly_r_panel_probe(struct mipi_dsi_device *dsi)
{
	struct truly_r_panel *truly_r;
	int ret;

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO |
			MIPI_DSI_MODE_VIDEO_HSE |
			MIPI_DSI_CLOCK_NON_CONTINUOUS |
			MIPI_DSI_MODE_NO_EOT_PACKET;

	truly_r = devm_kzalloc(&dsi->dev, sizeof(*truly_r), GFP_KERNEL);
	if (!truly_r)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, truly_r);
	truly_r->dsi = dsi;

	ret = truly_r_panel_add(truly_r);
	if (ret < 0)
		return ret;

	return mipi_dsi_attach(dsi);
}

static int truly_r_panel_remove(struct mipi_dsi_device *dsi)
{
	struct truly_r_panel *truly_r = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = truly_r_panel_disable(&truly_r->base);
	if (ret < 0)
		dev_err(&dsi->dev, "failed to disable panel: %d\n", ret);

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "failed to detach from DSI host: %d\n", ret);

	truly_r_panel_del(truly_r);

	return 0;
}

static void truly_r_panel_shutdown(struct mipi_dsi_device *dsi)
{
	struct truly_r_panel *truly_r = mipi_dsi_get_drvdata(dsi);

	truly_r_panel_disable(&truly_r->base);
}

static const struct of_device_id truly_r_of_match[] = {
	{ .compatible = "truly,r63350a", },
	{ }
};
MODULE_DEVICE_TABLE(of, truly_r_of_match);

static struct mipi_dsi_driver truly_r_panel_driver = {
	.driver = {
		.name = "panel-truly-r63350a",
		.of_match_table = truly_r_of_match,
	},
	.probe = truly_r_panel_probe,
	.remove = truly_r_panel_remove,
	.shutdown = truly_r_panel_shutdown,
};
module_mipi_dsi_driver(truly_r_panel_driver);

MODULE_AUTHOR("Andrew Perepech <andrew.perepech@mediatek.com>");
MODULE_LICENSE("GPL v2");
