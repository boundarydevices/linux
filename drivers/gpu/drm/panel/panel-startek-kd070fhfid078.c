// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Jitao Shi <jitao.shi@mediatek.com>
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

struct startek_panel {
	struct drm_panel base;
	struct mipi_dsi_device *dsi;
	struct device *dev;
	const struct panel_desc *desc;

	struct gpio_desc *reset_gpio;
	struct gpio_desc *dcdc_en_gpio;
	struct regulator *iovcc_supply;
	struct regulator *pp3300_supply;

	bool prepared_power;
	bool prepared;
	bool enabled;
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

static int startek_panel_init_dcs_cmd(struct startek_panel *startek)
{
	struct mipi_dsi_device *dsi = startek->dsi;
	struct drm_panel *panel = &startek->base;
	int i, err = 0;

	if (startek->desc->init_cmds) {
		const struct panel_init_cmd *init_cmds = startek->desc->init_cmds;

		for (i = 0; init_cmds[i].len != 0; i++) {
			const struct panel_init_cmd *cmd = &init_cmds[i];

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
	}
	return 0;
}

static int startek_panel_enter_sleep_mode(struct startek_panel *startek)
{
	struct mipi_dsi_device *dsi = startek->dsi;
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

static int startek_panel_unprepare_power(struct drm_panel *panel)
{
	struct startek_panel *startek = to_startek_panel(panel);

	if (!startek->prepared_power)
		return 0;

	msleep(150);

	gpiod_set_value(startek->reset_gpio, 0);
	mdelay(15);
	gpiod_set_value(startek->dcdc_en_gpio, 0);
	mdelay(3);
	if (startek->iovcc_supply)
		regulator_disable(startek->iovcc_supply);
	if (startek->pp3300_supply)
		regulator_disable(startek->pp3300_supply);

	startek->prepared_power = false;

	return 0;
}

static int startek_panel_unprepare(struct drm_panel *panel)
{
	struct startek_panel *startek = to_startek_panel(panel);
	int ret;

	if (!startek->prepared)
		return 0;

	ret = startek_panel_enter_sleep_mode(startek);
	if (ret < 0) {
		dev_err(panel->dev, "failed to set panel off: %d\n",
			ret);
		return ret;
	}

	startek_panel_unprepare_power(panel);

	startek->prepared = false;

	return 0;
}

static int startek_panel_prepare_power(struct drm_panel *panel)
{
	struct startek_panel *startek = to_startek_panel(panel);
	int ret = 0;

	if (startek->prepared_power)
		return 0;

	gpiod_set_value(startek->reset_gpio, 0);
	gpiod_set_value(startek->dcdc_en_gpio, 0);
	mdelay(1);

	if (startek->pp3300_supply) {
		ret = regulator_enable(startek->pp3300_supply);
		if (ret < 0) {
			dev_err(panel->dev, "Enable pp3300_supply fail, %d\n", ret);
			return ret;
		}
	}

	if (startek->iovcc_supply) {
		ret = regulator_enable(startek->iovcc_supply);
		if (ret < 0) {
			dev_err(panel->dev, "Enable iovcc_supply fail, %d\n", ret);
			return ret;
		}
	}
	mdelay(10);

	gpiod_set_value(startek->dcdc_en_gpio, 1);
	mdelay(15);
	gpiod_set_value(startek->reset_gpio, 1);
	mdelay(10);

	startek->prepared_power = true;

	return 0;
}

static int startek_panel_prepare(struct drm_panel *panel)
{
	struct startek_panel *startek = to_startek_panel(panel);
	int ret;

	if (startek->prepared)
		return 0;

	startek_panel_prepare_power(panel);

	ret = startek_panel_init_dcs_cmd(startek);
	if (ret < 0) {
		dev_err(panel->dev, "failed to init panel: %d\n", ret);
		return ret;
	}

	startek->prepared = true;

	return 0;
}

static int startek_panel_enable(struct drm_panel *panel)
{
	struct startek_panel *startek = to_startek_panel(panel);

	if (startek->enabled)
		return 0;

	msleep(130);
	startek->enabled = true;

	return 0;
}

static int startek_panel_disable(struct drm_panel *panel)
{
	struct startek_panel *startek = to_startek_panel(panel);

	if (!startek->enabled)
		return 0;

	startek->enabled = false;

	return 0;
}

static const struct drm_display_mode startek_kd070fhfid078_default_mode = {
	.clock = 157126,
	.hdisplay = 1200,
	.hsync_start = 1200 + 50,
	.hsync_end = 1200 + 50 + 10,
	.htotal = 1200 + 50 + 10 + 70,
	.vdisplay = 1920,
	.vsync_start = 1920 + 25,
	.vsync_end = 1920 + 25 + 4,
	.vtotal = 1920 + 25 + 4 + 20,
};

static const struct panel_desc startek_kd070fhfid078_desc = {
	.modes = &startek_kd070fhfid078_default_mode,
	.bpc = 8,
	.size = {
		.width_mm = 95,
		.height_mm = 151,
	},
	.lanes = 4,
	.format = MIPI_DSI_FMT_RGB888,
	.mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
		      MIPI_DSI_MODE_LPM,
	.init_cmds = startek_init_cmd,
};

static int startek_panel_get_modes(struct drm_panel *panel,
				struct drm_connector *connector)

{
	struct startek_panel *startek = to_startek_panel(panel);
	const struct drm_display_mode *m = startek->desc->modes;
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

	connector->display_info.width_mm = startek->desc->size.width_mm;
	connector->display_info.height_mm = startek->desc->size.height_mm;
	connector->display_info.bpc = startek->desc->bpc;

	return 1;
}

static const struct drm_panel_funcs startek_panel_funcs = {
	.unprepare = startek_panel_unprepare,
	.prepare = startek_panel_prepare,
	.disable = startek_panel_disable,
	.enable = startek_panel_enable,
	.get_modes = startek_panel_get_modes,
};

static int startek_panel_add(struct startek_panel *startek)
{
	struct device *dev = &startek->dsi->dev;
	int ret;

	startek->iovcc_supply = devm_regulator_get(dev, "iovcc");
	if (IS_ERR(startek->iovcc_supply))
		return PTR_ERR(startek->iovcc_supply);

	startek->pp3300_supply = devm_regulator_get(dev, "pp3300");
	if (IS_ERR(startek->pp3300_supply))
		return PTR_ERR(startek->pp3300_supply);

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

	startek->base.funcs = &startek_panel_funcs;
	startek->base.dev = &startek->dsi->dev;

	ret = drm_panel_of_backlight(&startek->base);
	if (ret)
		return ret;

	drm_panel_init(&startek->base, &startek->dsi->dev, &startek_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&startek->base);

	return 0;
}

static int startek_panel_probe(struct mipi_dsi_device *dsi)
{
	struct startek_panel *startek;
	struct device *dev = &dsi->dev;
	int ret;
	const struct panel_desc *desc;

	startek = devm_kzalloc(&dsi->dev, sizeof(*startek), GFP_KERNEL);
	if (!startek)
		return -ENOMEM;

	startek->dev = dev;
	desc = of_device_get_match_data(&dsi->dev);
	dsi->lanes = desc->lanes;
	dsi->format = desc->format;
	dsi->mode_flags = desc->mode_flags;
	startek->desc = desc;
	startek->dsi = dsi;
	ret = startek_panel_add(startek);
	if (ret < 0)
		return ret;

	mipi_dsi_set_drvdata(dsi, startek);

	ret = mipi_dsi_attach(dsi);
	if (ret)
		drm_panel_remove(&startek->base);

	return ret;
}

static void startek_panel_shutdown(struct mipi_dsi_device *dsi)
{
	struct startek_panel *startek = mipi_dsi_get_drvdata(dsi);

	drm_panel_disable(&startek->base);
	drm_panel_unprepare(&startek->base);
}

static int startek_panel_remove(struct mipi_dsi_device *dsi)
{
	struct startek_panel *startek = mipi_dsi_get_drvdata(dsi);
	int ret;

	startek_panel_shutdown(dsi);

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "failed to detach from DSI host: %d\n", ret);

	if (startek->base.dev)
		drm_panel_remove(&startek->base);

	return 0;
}

static const struct of_device_id startek_of_match[] = {
	{ .compatible = "startek,kd070fhfid078",
	  .data = &startek_kd070fhfid078_desc
	},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, startek_of_match);

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
MODULE_DESCRIPTION("STARTEK kd070fhfid078 1200x1920 video mode panel driver");
MODULE_LICENSE("GPL v2");
