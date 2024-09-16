// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 * Authors:
 * Jitao Shi <jitao.shi@mediatek.com>
 * Vitor Sato Eschholz <vsatoes@baylibre.com>
 */
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>

#include <drm/drm_connector.h>
#include <drm/drm_crtc.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>

#include <video/mipi_display.h>

enum {
	IOVCC = 0,
	PP3300 = 1,
	MAX_SUPPLY = 2
};

struct startek_panel {
	struct drm_panel base;
	struct mipi_dsi_device *dsi;
	struct device *dev;

	struct gpio_desc *reset_gpio;
	struct gpio_desc *dcdc_en_gpio;
	struct regulator_bulk_data supplies[MAX_SUPPLY];
};

enum dsi_cmd_type {
	INIT_DCS_CMD,
	INIT_GEN_CMD,
	DELAY_CMD,
};

struct panel_init_cmd {
	enum dsi_cmd_type type;
	size_t len;
	const char *data;
};

#define _INIT_GEN_CMD(...) { \
	.type = INIT_GEN_CMD, \
	.len = sizeof((char[]){__VA_ARGS__}), \
	.data = (char[]){__VA_ARGS__} }

#define _INIT_DCS_CMD(...) { \
	.type = INIT_DCS_CMD, \
	.len = sizeof((char[]){__VA_ARGS__}), \
	.data = (char[]){__VA_ARGS__} }

#define _INIT_DELAY_CMD(...) { \
	.type = DELAY_CMD,\
	.len = sizeof((char[]){__VA_ARGS__}), \
	.data = (char[]){__VA_ARGS__} }

static const struct panel_init_cmd startek_init_cmd[] = {
	_INIT_GEN_CMD(0xB0, 0x05),
	_INIT_GEN_CMD(0xB3, 0x52),
	_INIT_GEN_CMD(0xB8, 0x7F),
	_INIT_GEN_CMD(0xBC, 0x20),
	_INIT_GEN_CMD(0xD6, 0x7F),
	_INIT_GEN_CMD(0xB0, 0x01),
	_INIT_GEN_CMD(0xC0, 0x0D),
	_INIT_GEN_CMD(0xC1, 0x0D),
	_INIT_GEN_CMD(0xC2, 0x06),
	_INIT_GEN_CMD(0xC3, 0x06),
	_INIT_GEN_CMD(0xC4, 0x08),
	_INIT_GEN_CMD(0xC5, 0x08),
	_INIT_GEN_CMD(0xC6, 0x0A),
	_INIT_GEN_CMD(0xC7, 0x0A),
	_INIT_GEN_CMD(0xC8, 0x0C),
	_INIT_GEN_CMD(0xC9, 0x0C),
	_INIT_GEN_CMD(0xCA, 0x00),
	_INIT_GEN_CMD(0xCB, 0x00),
	_INIT_GEN_CMD(0xCC, 0x0E),
	_INIT_GEN_CMD(0xCD, 0x0E),
	_INIT_GEN_CMD(0xCE, 0x01),
	_INIT_GEN_CMD(0xCF, 0x01),
	_INIT_GEN_CMD(0xD0, 0x04),
	_INIT_GEN_CMD(0xD1, 0x04),
	_INIT_GEN_CMD(0xD2, 0x00),
	_INIT_GEN_CMD(0xD3, 0x00),
	_INIT_GEN_CMD(0xD4, 0x0D),
	_INIT_GEN_CMD(0xD5, 0x0D),
	_INIT_GEN_CMD(0xD6, 0x05),
	_INIT_GEN_CMD(0xD7, 0x05),
	_INIT_GEN_CMD(0xD8, 0x07),
	_INIT_GEN_CMD(0xD9, 0x07),
	_INIT_GEN_CMD(0xDA, 0x09),
	_INIT_GEN_CMD(0xDB, 0x09),
	_INIT_GEN_CMD(0xDC, 0x0B),
	_INIT_GEN_CMD(0xDD, 0x0B),
	_INIT_GEN_CMD(0xDE, 0x00),
	_INIT_GEN_CMD(0xDF, 0x00),
	_INIT_GEN_CMD(0xE0, 0x0E),
	_INIT_GEN_CMD(0xE1, 0x0E),
	_INIT_GEN_CMD(0xE2, 0x01),
	_INIT_GEN_CMD(0xE3, 0x01),
	_INIT_GEN_CMD(0xE4, 0x03),
	_INIT_GEN_CMD(0xE5, 0x03),
	_INIT_GEN_CMD(0xE6, 0x00),
	_INIT_GEN_CMD(0xE7, 0x00),
	_INIT_GEN_CMD(0xB0, 0x03),
	_INIT_GEN_CMD(0xBA, 0xF0),
	_INIT_GEN_CMD(0xC8, 0x07),
	_INIT_GEN_CMD(0xC9, 0x03),
	_INIT_GEN_CMD(0xCA, 0x41),
	_INIT_GEN_CMD(0xD2, 0x01),
	_INIT_GEN_CMD(0xD3, 0x05),
	_INIT_GEN_CMD(0xD4, 0x05),
	_INIT_GEN_CMD(0xD5, 0x8A),
	_INIT_GEN_CMD(0xE4, 0xC0),
	_INIT_GEN_CMD(0xE5, 0x00),
	_INIT_GEN_CMD(0xB0, 0x00),
	_INIT_GEN_CMD(0xBF, 0x1F),
	_INIT_GEN_CMD(0xC0, 0x12),
	_INIT_GEN_CMD(0xC2, 0x1E),
	_INIT_GEN_CMD(0xC4, 0x1E),
	_INIT_GEN_CMD(0xB0, 0x06),
	_INIT_GEN_CMD(0xB8, 0xA5),
	_INIT_GEN_CMD(0xC0, 0xA5),
	_INIT_GEN_CMD(0xBC, 0x11),
	_INIT_GEN_CMD(0xD5, 0x48),
	_INIT_GEN_CMD(0xB8, 0x00),
	_INIT_GEN_CMD(0xC0, 0x00),
	_INIT_DCS_CMD(0x11),
	_INIT_DELAY_CMD(120),
	_INIT_DCS_CMD(0x29),
	_INIT_DELAY_CMD(20),
	{},
};

static inline struct startek_panel *to_startek_panel(struct drm_panel *panel)
{
	return container_of(panel, struct startek_panel, base);
}

static int startek_panel_init(struct startek_panel *startek)
{
	struct mipi_dsi_device *dsi = startek->dsi;
	struct drm_panel *panel = &startek->base;
	int i, err = 0;

	for (i = 0; startek_init_cmd[i].len != 0; i++) {
		const struct panel_init_cmd *cmd = &startek_init_cmd[i];

		switch (cmd->type) {
		case DELAY_CMD:
			msleep(cmd->data[0]);
			err = 0;
			break;

		case INIT_DCS_CMD:
			err = mipi_dsi_dcs_write(dsi, cmd->data[0],
						 cmd->len <= 1 ? NULL :
						 &cmd->data[1],
						 cmd->len - 1);
			break;

		case INIT_GEN_CMD:
			err = mipi_dsi_generic_write(dsi,
						     cmd->len <= 1 ? NULL :
						     &cmd->data[0],
						     cmd->len);
			break;

		default:
			err = -EINVAL;
		}

		if (err < 0) {
			dev_err(panel->dev,
				"failed to write command %u\n", i);
			return err;
		}
	}

	return 0;
}

static int startek_panel_on(struct startek_panel *startek)
{
	struct mipi_dsi_device *dsi = startek->dsi;
	struct mipi_dsi_multi_context dsi_ctx = {.dsi = dsi};

	mipi_dsi_dcs_set_display_on_multi(&dsi_ctx);

	mipi_dsi_msleep(&dsi_ctx, 20);

	return dsi_ctx.accum_err;
}

static int startek_panel_off(struct startek_panel *startek)
{
	struct mipi_dsi_device *dsi = startek->dsi;
	struct mipi_dsi_multi_context dsi_ctx = {.dsi = dsi};

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	mipi_dsi_dcs_set_display_off_multi(&dsi_ctx);
	mipi_dsi_dcs_enter_sleep_mode_multi(&dsi_ctx);

	mipi_dsi_msleep(&dsi_ctx, 100);

	return dsi_ctx.accum_err;
}

static int startek_panel_unprepare(struct drm_panel *panel)
{
	struct startek_panel *startek = to_startek_panel(panel);
	int ret;

	ret = startek_panel_off(startek);
	if (ret < 0) {
		dev_err(panel->dev, "failed to set panel off: %d\n", ret);
		return ret;
	}

	regulator_bulk_disable(ARRAY_SIZE(startek->supplies), startek->supplies);

	gpiod_set_value(startek->reset_gpio, 0);
	mdelay(15);
	gpiod_set_value(startek->dcdc_en_gpio, 0);
	mdelay(3);

	return 0;
}

static int startek_panel_prepare(struct drm_panel *panel)
{
	struct startek_panel *startek = to_startek_panel(panel);
	int ret;

	gpiod_set_value(startek->reset_gpio, 0);
	gpiod_set_value(startek->dcdc_en_gpio, 0);
	mdelay(1);

	ret = regulator_enable(startek->supplies[PP3300].consumer);
	if (ret < 0) {
		dev_err(panel->dev, "Enable pp3300_supply fail, %d\n", ret);
		return ret;
	}

	ret = regulator_enable(startek->supplies[IOVCC].consumer);
	if (ret < 0) {
		dev_err(panel->dev, "Enable iovcc_supply fail, %d\n", ret);
		goto pp3300off;
	}
	mdelay(10);

	gpiod_set_value(startek->dcdc_en_gpio, 1);
	mdelay(15);
	gpiod_set_value(startek->reset_gpio, 1);
	mdelay(10);

	ret = startek_panel_init(startek);
	if (ret < 0) {
		dev_err(panel->dev, "failed to init panel: %d\n", ret);
		goto poweroff;
	}

	ret = startek_panel_on(startek);
	if (ret < 0) {
		dev_err(panel->dev, "failed to power on panel: %d\n", ret);
		goto poweroff;
	}

	return 0;

poweroff:
	regulator_disable(startek->supplies[PP3300].consumer);
pp3300off:
	regulator_disable(startek->supplies[IOVCC].consumer);
	gpiod_set_value(startek->dcdc_en_gpio, 0);
	gpiod_set_value(startek->reset_gpio, 0);

	return ret;
}

static const struct drm_display_mode default_mode = {
	.clock = 157126,
	.hdisplay = 1200,
	.hsync_start = 1200 + 50,
	.hsync_end = 1200 + 50 + 10,
	.htotal = 1200 + 50 + 10 + 70,
	.vdisplay = 1920,
	.vsync_start = 1920 + 25,
	.vsync_end = 1920 + 25 + 4,
	.vtotal = 1920 + 25 + 4 + 20,
	.width_mm = 95,
	.height_mm = 151,
};

static int startek_panel_get_modes(struct drm_panel *panel,
				   struct drm_connector *connector)

{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode) {
		dev_err(panel->dev, "failed to add mode %ux%u@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_set_name(mode);
	drm_mode_probed_add(connector, mode);

	connector->display_info.width_mm = default_mode.width_mm;
	connector->display_info.height_mm = default_mode.height_mm;
	connector->display_info.bpc = 8;

	return 1;
}

static const struct drm_panel_funcs startek_panel_funcs = {
	.unprepare = startek_panel_unprepare,
	.prepare = startek_panel_prepare,
	.get_modes = startek_panel_get_modes,
};

static const struct of_device_id startek_of_match[] = {
	{ .compatible = "startek,kd070fhfid078", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, startek_of_match);

static int startek_panel_add(struct startek_panel *startek)
{
	struct device *dev = &startek->dsi->dev;
	int ret;

	startek->supplies[IOVCC].supply = "iovcc";
	startek->supplies[PP3300].supply = "pp3300";

	ret = devm_regulator_bulk_get(dev, ARRAY_SIZE(startek->supplies),
				      startek->supplies);
	if (ret) {
		dev_err(dev, "regulator_bulk failed\n");
		return ret;
	}

	startek->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(startek->reset_gpio)) {
		ret = PTR_ERR(startek->reset_gpio);
		dev_err(dev, "cannot get reset-gpios %d\n", ret);
		return ret;
	}

	startek->dcdc_en_gpio = devm_gpiod_get(dev, "dcdc", GPIOD_OUT_LOW);
	if (IS_ERR(startek->dcdc_en_gpio)) {
		ret = PTR_ERR(startek->dcdc_en_gpio);
		dev_err(dev, "cannot get dcdc-en-gpio %d\n", ret);
		return ret;
	}

	drm_panel_init(&startek->base, dev, &startek_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);

	ret = drm_panel_of_backlight(&startek->base);
	if (ret)
		return ret;

	drm_panel_add(&startek->base);

	return 0;
}

static int startek_panel_probe(struct mipi_dsi_device *dsi)
{
	struct startek_panel *startek;
	int ret;

	startek = devm_kzalloc(&dsi->dev, sizeof(*startek), GFP_KERNEL);
	if (!startek)
		return -ENOMEM;

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE | MIPI_DSI_MODE_LPM;

	mipi_dsi_set_drvdata(dsi, startek);

	startek->dsi = dsi;

	ret = startek_panel_add(startek);
	if (ret < 0)
		return ret;

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&startek->base);

	return 0;
}

static void startek_panel_shutdown(struct mipi_dsi_device *dsi)
{
	struct startek_panel *startek = mipi_dsi_get_drvdata(dsi);

	drm_panel_disable(&startek->base);
	drm_panel_unprepare(&startek->base);
}

static void startek_panel_remove(struct mipi_dsi_device *dsi)
{
	struct startek_panel *startek = mipi_dsi_get_drvdata(dsi);
	int ret;

	startek_panel_shutdown(dsi);

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&startek->base);
}

static struct mipi_dsi_driver startek_panel_driver = {
	.driver = {
		.name = "panel-startek-kd070fhfid078",
		.of_match_table = startek_of_match,
	},
	.probe = startek_panel_probe,
	.remove = startek_panel_remove,
	.shutdown = startek_panel_shutdown,
};
module_mipi_dsi_driver(startek_panel_driver);

MODULE_AUTHOR("Jitao Shi <jitao.shi@mediatek.com>");
MODULE_DESCRIPTION("STARTEK KD070FHFID078");
MODULE_LICENSE("GPL");
