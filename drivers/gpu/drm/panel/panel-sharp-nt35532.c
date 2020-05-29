/*
 * Copyright (C) 2019 BayLibre
 * Author: Alexandre Bailon <abailon@baylibre.com>
 *
 * Based on panel-sharp-ls043t1le01 driver.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
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

struct sharp_nt_panel {
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

static inline struct sharp_nt_panel *to_sharp_nt_panel(struct drm_panel *panel)
{
	return container_of(panel, struct sharp_nt_panel, base);
}

static int sharp_nt_panel_on(struct sharp_nt_panel *sharp_nt)
{
	struct mipi_dsi_device *dsi = sharp_nt->dsi;
	int ret;


	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret < 0)
		return ret;

	ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret < 0)
		return ret;

	return 0;
}

static int sharp_nt_panel_off(struct sharp_nt_panel *sharp_nt)
{
	struct mipi_dsi_device *dsi = sharp_nt->dsi;
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


static int sharp_nt_panel_disable(struct drm_panel *panel)
{
	struct sharp_nt_panel *sharp_nt = to_sharp_nt_panel(panel);

	if (!sharp_nt->enabled)
		return 0;

	backlight_disable(sharp_nt->backlight);

	sharp_nt->enabled = false;

	return 0;
}

static int sharp_nt_panel_unprepare(struct drm_panel *panel)
{
	struct sharp_nt_panel *sharp_nt = to_sharp_nt_panel(panel);
	int ret;

	if (!sharp_nt->prepared)
		return 0;

	ret = sharp_nt_panel_off(sharp_nt);
	if (ret < 0) {
		dev_err(panel->dev, "failed to set panel off: %d\n", ret);
		return ret;
	}

	gpiod_set_value(sharp_nt->pwr2_gpio, 0);
	gpiod_set_value(sharp_nt->pwr_gpio, 0);
	gpiod_set_value(sharp_nt->reset_gpio, 0);

	sharp_nt->prepared = false;

	return 0;
}

static int sharp_nt_panel_prepare(struct drm_panel *panel)
{
	struct sharp_nt_panel *sharp_nt = to_sharp_nt_panel(panel);
	int ret;

	if (sharp_nt->prepared)
		return 0;

	gpiod_set_value(sharp_nt->pwr_gpio, 1);
	gpiod_set_value(sharp_nt->pwr2_gpio, 1);

	msleep(20);

	gpiod_set_value(sharp_nt->reset_gpio, 1);
	msleep(1);
	gpiod_set_value(sharp_nt->reset_gpio, 0);
	msleep(1);
	gpiod_set_value(sharp_nt->reset_gpio, 1);
	msleep(10);

	ret = sharp_nt_panel_on(sharp_nt);
	if (ret < 0) {
		dev_err(panel->dev, "failed to set panel on: %d\n", ret);
		goto poweroff;
	}

	sharp_nt->prepared = true;

	return 0;

poweroff:
	gpiod_set_value(sharp_nt->pwr2_gpio, 0);
	gpiod_set_value(sharp_nt->pwr_gpio, 0);
	gpiod_set_value(sharp_nt->reset_gpio, 0);

	return ret;
}

static int sharp_nt_panel_enable(struct drm_panel *panel)
{
	struct sharp_nt_panel *sharp_nt = to_sharp_nt_panel(panel);

	if (sharp_nt->enabled)
		return 0;

	backlight_enable(sharp_nt->backlight);

	sharp_nt->enabled = true;

	return 0;
}

static const struct drm_display_mode default_mode = {
	.clock = 137380,
	.hdisplay = 1080,
	.hsync_start = 1080 + 72,
	.hsync_end = 1080 + 72 + 8,
	.htotal = 1080 + 72 + 8 + 16,
	.vdisplay = 1920,
	.vsync_start = 1920 + 14,
	.vsync_end = 1920 + 14 + 2,
	.vtotal = 1920 + 14 + 2 + 6,
};

static int sharp_nt_panel_get_modes(struct drm_panel *panel,
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

static const struct drm_panel_funcs sharp_nt_panel_funcs = {
	.disable = sharp_nt_panel_disable,
	.unprepare = sharp_nt_panel_unprepare,
	.prepare = sharp_nt_panel_prepare,
	.enable = sharp_nt_panel_enable,
	.get_modes = sharp_nt_panel_get_modes,
};

static int sharp_nt_panel_add(struct sharp_nt_panel *sharp_nt)
{
	struct device *dev = &sharp_nt->dsi->dev;

	sharp_nt->mode = &default_mode;
	sharp_nt->reset_gpio = devm_gpiod_get(dev, "reset",
					      GPIOD_OUT_LOW);
	if (IS_ERR(sharp_nt->reset_gpio)) {
		dev_err(dev, "cannot get reset-gpios %ld\n",
			PTR_ERR(sharp_nt->reset_gpio));
		return PTR_ERR(sharp_nt->reset_gpio);
	}

	sharp_nt->pwr_gpio = devm_gpiod_get(dev, "pwr",
					    GPIOD_OUT_LOW);
	if (IS_ERR(sharp_nt->pwr_gpio)) {
		dev_err(dev, "cannot get pwr-gpios %ld\n",
			PTR_ERR(sharp_nt->pwr_gpio));
		return PTR_ERR(sharp_nt->pwr_gpio);
	}

	sharp_nt->pwr2_gpio = devm_gpiod_get(dev, "pwr2",
					     GPIOD_OUT_LOW);
	if (IS_ERR(sharp_nt->pwr_gpio)) {
		dev_err(dev, "cannot get pwr2-gpios %ld\n",
			PTR_ERR(sharp_nt->pwr2_gpio));
		return PTR_ERR(sharp_nt->pwr2_gpio);
	}

	sharp_nt->backlight = devm_of_find_backlight(dev);
	if (IS_ERR(sharp_nt->backlight)) {
		dev_err(dev, "failed to get backlight\n");
		return PTR_ERR(sharp_nt->backlight);
	}

	drm_panel_init(&sharp_nt->base, &sharp_nt->dsi->dev,
		       &sharp_nt_panel_funcs, DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&sharp_nt->base);

	return 0;
}

static void sharp_nt_panel_del(struct sharp_nt_panel *sharp_nt)
{
	if (sharp_nt->base.dev)
		drm_panel_remove(&sharp_nt->base);
}

static int sharp_nt_panel_probe(struct mipi_dsi_device *dsi)
{
	struct sharp_nt_panel *sharp_nt;
	int ret;

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO |
			MIPI_DSI_MODE_VIDEO_HSE |
			MIPI_DSI_CLOCK_NON_CONTINUOUS |
			MIPI_DSI_MODE_NO_EOT_PACKET;

	sharp_nt = devm_kzalloc(&dsi->dev, sizeof(*sharp_nt), GFP_KERNEL);
	if (!sharp_nt)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, sharp_nt);
	sharp_nt->dsi = dsi;

	ret = sharp_nt_panel_add(sharp_nt);
	if (ret < 0)
		return ret;

	return mipi_dsi_attach(dsi);
}

static int sharp_nt_panel_remove(struct mipi_dsi_device *dsi)
{
	struct sharp_nt_panel *sharp_nt = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = sharp_nt_panel_disable(&sharp_nt->base);
	if (ret < 0)
		dev_err(&dsi->dev, "failed to disable panel: %d\n", ret);

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "failed to detach from DSI host: %d\n", ret);

	sharp_nt_panel_del(sharp_nt);

	return 0;
}

static void sharp_nt_panel_shutdown(struct mipi_dsi_device *dsi)
{
	struct sharp_nt_panel *sharp_nt = mipi_dsi_get_drvdata(dsi);

	sharp_nt_panel_disable(&sharp_nt->base);
}

static const struct of_device_id sharp_nt_of_match[] = {
	{ .compatible = "sharp,nt35532", },
	{ }
};
MODULE_DEVICE_TABLE(of, sharp_nt_of_match);

static struct mipi_dsi_driver sharp_nt_panel_driver = {
	.driver = {
		.name = "panel-sharp-nt35532",
		.of_match_table = sharp_nt_of_match,
	},
	.probe = sharp_nt_panel_probe,
	.remove = sharp_nt_panel_remove,
	.shutdown = sharp_nt_panel_shutdown,
};
module_mipi_dsi_driver(sharp_nt_panel_driver);

MODULE_AUTHOR("Alexandre Bailon <abailon@baylibre.com>");
MODULE_LICENSE("GPL v2");
