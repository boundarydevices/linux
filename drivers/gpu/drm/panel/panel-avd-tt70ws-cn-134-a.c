// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 BayLibre
 * Author: Mattijs Korpershoek <mkorpershoek@baylibre.com>
 *
 * Based on panel-truly-r63350a driver.
 *
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
#include <drm/drm_print.h>

#include <video/mipi_display.h>


struct lcm_init_struct {
	u8 cmd;
	u8 count;
	u8 params[64];
};

static const struct lcm_init_struct lcm_init[] = {
	{0xB2, 1, {0x10} },
	{0x80, 1, {0xAC} },
	{0x81, 1, {0xB8} },
	{0x82, 1, {0x09} },
	{0x83, 1, {0x78} },
	{0x84, 1, {0x7F} },
	{0x85, 1, {0xBB} },
	{0x86, 1, {0x70} },
};

struct avd_tt_panel {
	struct drm_panel base;
	struct mipi_dsi_device *dsi;

	struct gpio_desc *vdden_gpio;
	struct gpio_desc *iovccen_gpio;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *stbyb_gpio;
	struct gpio_desc *bl_gpio;

	struct backlight_device *backlight;
	struct regulator *iovcc_supply;
	struct regulator *vdd_supply;

	bool prepared;
	bool enabled;

	const struct drm_display_mode *mode;
};

static inline struct avd_tt_panel *to_avd_tt_panel(struct drm_panel *panel)
{
	return container_of(panel, struct avd_tt_panel, base);
}


static ssize_t avd_tt_write_buffer(struct mipi_dsi_device *dsi, u8 cmd,
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

static ssize_t avd_tt_push_table(struct mipi_dsi_device *dsi,
				  const struct lcm_init_struct *table,
				  size_t len)
{
	ssize_t err;
	size_t i;

	for (i = 0; i < len; i++) {
		err = avd_tt_write_buffer(dsi, table[i].cmd,
					   table[i].params, table[i].count);
		if (err < 0)
			return err;
	}

	return err;
}

static int avd_tt_panel_on(struct avd_tt_panel *avd_tt)
{
	struct mipi_dsi_device *dsi = avd_tt->dsi;
	int ret;


	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	ret = avd_tt_push_table(dsi, lcm_init, ARRAY_SIZE(lcm_init));
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

static int avd_tt_panel_off(struct avd_tt_panel *avd_tt)
{
	struct mipi_dsi_device *dsi = avd_tt->dsi;
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


static int avd_tt_panel_disable(struct drm_panel *panel)
{
	struct avd_tt_panel *avd_tt = to_avd_tt_panel(panel);

	if (!avd_tt->enabled)
		return 0;

	backlight_disable(avd_tt->backlight);

	avd_tt->enabled = false;

	return 0;
}

static int avd_tt_panel_unprepare(struct drm_panel *panel)
{
	struct avd_tt_panel *avd_tt = to_avd_tt_panel(panel);
	int ret;

	if (!avd_tt->prepared)
		return 0;

	ret = avd_tt_panel_off(avd_tt);
	if (ret < 0) {
		dev_err(panel->dev, "failed to set panel off: %d\n", ret);
		return ret;
	}

	gpiod_set_value_cansleep(avd_tt->reset_gpio, 0);
	gpiod_set_value_cansleep(avd_tt->stbyb_gpio, 0);
	gpiod_set_value_cansleep(avd_tt->iovccen_gpio, 0);
	gpiod_set_value_cansleep(avd_tt->vdden_gpio, 0);
	gpiod_set_value_cansleep(avd_tt->bl_gpio, 0);

	ret = regulator_disable(avd_tt->iovcc_supply);
	if (ret < 0) {
		dev_err(panel->dev, "failed to disable iovcc_supply: %d\n",
			ret);
		return ret;
	}
	ret = regulator_disable(avd_tt->vdd_supply);
	if (ret < 0) {
		dev_err(panel->dev, "failed to disable vdd_supply: %d\n",
			ret);
		return ret;
	}
	avd_tt->prepared = false;

	return 0;
}

static int avd_tt_panel_prepare(struct drm_panel *panel)
{
	struct avd_tt_panel *avd_tt = to_avd_tt_panel(panel);
	int ret;

	if (avd_tt->prepared)
		return 0;

	ret = regulator_enable(avd_tt->iovcc_supply);
	if (ret < 0) {
		dev_err(panel->dev, "failed to enable iovcc_supply: %d\n",
			ret);
		return ret;
	}

	ret = regulator_enable(avd_tt->vdd_supply);
	if (ret < 0) {
		dev_err(panel->dev, "failed to enable vdd_supply: %d\n",
			ret);
		return ret;
	}

	gpiod_set_value_cansleep(avd_tt->bl_gpio, 1);
	gpiod_set_value_cansleep(avd_tt->iovccen_gpio, 1);
	gpiod_set_value_cansleep(avd_tt->vdden_gpio, 1);
	gpiod_set_value_cansleep(avd_tt->stbyb_gpio, 1);
	msleep(20);
	gpiod_set_value_cansleep(avd_tt->reset_gpio, 1);
	msleep(20);
	gpiod_set_value_cansleep(avd_tt->reset_gpio, 0);
	msleep(30);
	gpiod_set_value_cansleep(avd_tt->reset_gpio, 1);
	msleep(60);

	ret = avd_tt_panel_on(avd_tt);
	if (ret < 0) {
		dev_err(panel->dev, "failed to set panel on: %d\n", ret);
		goto poweroff;
	}

	avd_tt->prepared = true;

	return 0;

poweroff:
	gpiod_set_value_cansleep(avd_tt->reset_gpio, 0);
	return ret;
}

static int avd_tt_panel_enable(struct drm_panel *panel)
{
	struct avd_tt_panel *avd_tt = to_avd_tt_panel(panel);

	if (avd_tt->enabled)
		return 0;

	backlight_enable(avd_tt->backlight);

	avd_tt->enabled = true;

	return 0;
}

static const struct drm_display_mode default_mode = {
	.clock = 42700,
	.hdisplay = 1024,
	.hsync_start = 1024 + 60,
	.hsync_end = 1024 + 60 + 10,
	.htotal = 1024 + 60 + 10 + 60,
	.vdisplay = 600,
	.vsync_start = 600 + 5,
	.vsync_end = 600 + 5 + 2,
	.vtotal = 600 + 5 + 2 + 10,
};

static int avd_tt_panel_get_modes(struct drm_panel *panel,
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

	connector->display_info.width_mm = 154;
	connector->display_info.height_mm = 86;

	return 1;
}

static const struct drm_panel_funcs avd_tt_panel_funcs = {
	.disable = avd_tt_panel_disable,
	.unprepare = avd_tt_panel_unprepare,
	.prepare = avd_tt_panel_prepare,
	.enable = avd_tt_panel_enable,
	.get_modes = avd_tt_panel_get_modes,
};

static int avd_tt_panel_add(struct avd_tt_panel *avd_tt)
{
	struct device *dev = &avd_tt->dsi->dev;
	int ret;

	avd_tt->mode = &default_mode;

	avd_tt->reset_gpio = devm_gpiod_get(dev, "reset",
						 GPIOD_OUT_HIGH);
	if (IS_ERR(avd_tt->reset_gpio)) {
		dev_err(dev, "cannot get reset-gpios %ld\n",
			PTR_ERR(avd_tt->reset_gpio));
		return PTR_ERR(avd_tt->reset_gpio);
	}


	avd_tt->iovccen_gpio = devm_gpiod_get_optional(dev, "iovccen",
						 GPIOD_OUT_HIGH);
	if (IS_ERR(avd_tt->iovccen_gpio)) {
		dev_err(dev, "cannot get iovccen-gpios %ld\n",
			PTR_ERR(avd_tt->iovccen_gpio));
		return PTR_ERR(avd_tt->iovccen_gpio);
	}

	avd_tt->vdden_gpio = devm_gpiod_get_optional(dev, "vdden",
						 GPIOD_OUT_HIGH);
	if (IS_ERR(avd_tt->vdden_gpio)) {
		dev_err(dev, "cannot get vdden-gpios %ld\n",
			PTR_ERR(avd_tt->vdden_gpio));
		return PTR_ERR(avd_tt->vdden_gpio);
	}

	avd_tt->stbyb_gpio = devm_gpiod_get(dev, "stbyb",
						 GPIOD_OUT_HIGH);
	if (IS_ERR(avd_tt->stbyb_gpio)) {
		dev_err(dev, "cannot get stbyb-gpios %ld\n",
			PTR_ERR(avd_tt->stbyb_gpio));
		return PTR_ERR(avd_tt->stbyb_gpio);
	}

	avd_tt->bl_gpio = devm_gpiod_get_optional(dev, "bl",
						 GPIOD_OUT_HIGH);
	if (IS_ERR(avd_tt->bl_gpio)) {
		dev_err(dev, "cannot get bl-gpios %ld\n",
			PTR_ERR(avd_tt->bl_gpio));
		return PTR_ERR(avd_tt->bl_gpio);
	}

	avd_tt->iovcc_supply = devm_regulator_get(dev, "iovcc");
	if (IS_ERR(avd_tt->iovcc_supply)) {
		dev_err(dev, "failed to request iovcc regulator: %ld\n",
					PTR_ERR(avd_tt->iovcc_supply));
		return PTR_ERR(avd_tt->iovcc_supply);
	}
	avd_tt->vdd_supply = devm_regulator_get(dev, "vdd");
	if (IS_ERR(avd_tt->vdd_supply)) {
		dev_err(dev, "failed to request vdd regulator: %ld\n",
					PTR_ERR(avd_tt->vdd_supply));
		return PTR_ERR(avd_tt->vdd_supply);
	}

	ret = regulator_set_voltage(avd_tt->iovcc_supply, 1800000, 1800000);
	if (ret) {
		dev_err(dev, "failed set voltage to regulator 'iovcc_supply'\n");
		return ret;
	}
	ret = regulator_set_voltage(avd_tt->vdd_supply, 3300000, 3300000);
	if (ret) {
		dev_err(dev, "failed set voltage to regulator 'dsi3vdd_supply3_supply'\n");
		return ret;
	}

	avd_tt->backlight = devm_of_find_backlight(dev);
	if (IS_ERR(avd_tt->backlight)) {
		dev_err(dev, "failed to get backlight\n");
		return PTR_ERR(avd_tt->backlight);
	}

	drm_panel_init(&avd_tt->base, dev, &avd_tt_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&avd_tt->base);

	return 0;
}

static void avd_tt_panel_del(struct avd_tt_panel *avd_tt)
{
	if (avd_tt->base.dev)
		drm_panel_remove(&avd_tt->base);
}

static int avd_tt_panel_probe(struct mipi_dsi_device *dsi)
{
	struct avd_tt_panel *avd_tt;
	int ret;

	dsi->lanes = 2;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO |
			MIPI_DSI_MODE_VIDEO_HSE |
			MIPI_DSI_CLOCK_NON_CONTINUOUS |
			MIPI_DSI_MODE_NO_EOT_PACKET;

	avd_tt = devm_kzalloc(&dsi->dev, sizeof(*avd_tt), GFP_KERNEL);
	if (!avd_tt)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, avd_tt);
	avd_tt->dsi = dsi;

	ret = avd_tt_panel_add(avd_tt);
	if (ret < 0)
		return ret;

	return mipi_dsi_attach(dsi);
}

static int avd_tt_panel_remove(struct mipi_dsi_device *dsi)
{
	struct avd_tt_panel *avd_tt = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = avd_tt_panel_disable(&avd_tt->base);
	if (ret < 0)
		dev_err(&dsi->dev, "failed to disable panel: %d\n", ret);

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "failed to detach from DSI host: %d\n", ret);

	avd_tt_panel_del(avd_tt);

	return 0;
}

static void avd_tt_panel_shutdown(struct mipi_dsi_device *dsi)
{
	struct avd_tt_panel *avd_tt = mipi_dsi_get_drvdata(dsi);

	avd_tt_panel_disable(&avd_tt->base);
}

static const struct of_device_id avd_tt_of_match[] = {
	{ .compatible = "avd,tt70ws-cn-134-a", },
	{ }
};
MODULE_DEVICE_TABLE(of, avd_tt_of_match);

static struct mipi_dsi_driver avd_tt_panel_driver = {
	.driver = {
		.name = "panel-avd-tt70ws-cn-134-a",
		.of_match_table = avd_tt_of_match,
	},
	.probe = avd_tt_panel_probe,
	.remove = avd_tt_panel_remove,
	.shutdown = avd_tt_panel_shutdown,
};
module_mipi_dsi_driver(avd_tt_panel_driver);

MODULE_AUTHOR("Mattijs Korpershoek <mkorpershoek@baylibre.com>");
MODULE_DESCRIPTION("AVD TT70WS-CN-134-A DSI Panel Driver");
MODULE_LICENSE("GPL v2");
