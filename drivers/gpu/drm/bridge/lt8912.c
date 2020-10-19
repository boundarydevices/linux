/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#define DEBUG
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/of_gpio.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <video/of_videomode.h>
#include <video/videomode.h>
#include <drm/drm_probe_helper.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_drv.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_of.h>
#include <drm/drm_panel.h>
#include <video/mipi_display.h>
#include <video/videomode.h>

#include <linux/regmap.h>

#define MAIN_LANE_EN	0x40

#define CEC_DSI_SETTLE	0x11
#define CEC_DSI_LANE	0x13
#define CEC_DSI_HSYNC	0x18
#define CEC_DSI_VSYNC	0x19
#define CEC_DSI_HACTIVE	0x1c
#define CEC_DSI_HTOTAL	0x34
#define CEC_DSI_VTOTAL	0x36
#define CEC_DSI_VBP	0x38
#define CEC_DSI_VFP	0x3a
#define CEC_DSI_HBP	0x3c
#define CEC_DSI_HFP	0x3e

struct lt8912 {
	struct device *dev;
	struct gpio_desc *gp_reset;
	struct device_node *host_node;
	struct drm_connector connector;
	struct mipi_dsi_device *dsi;
	struct i2c_client *i2c_client[3];
	struct regmap *regmap[3];
	struct videomode mode;
	struct i2c_adapter *ddc;
	struct drm_bridge bridge;
	u8 dsi_lanes;
	bool is_power_on;
	struct workqueue_struct *workq;
	struct delayed_work lt8912_check_hpd_work_id;
};

enum lt8912_i2c_addr {
	I2C_ADDR_MAIN = 0x48,
	I2C_ADDR_CEC_DSI = 0x49,
	I2C_ADDR_I2S = 0x4a,
};

enum lt8912_i2c_idx {
	I2C_MAIN = 0,
	I2C_CEC_DSI = 1,
	I2C_I2S = 2,
};

struct lt8912_reg_cfg {
	enum lt8912_i2c_idx idx;
	unsigned int reg;
	unsigned int val;
	int sleep_in_ms;
};

static struct lt8912_reg_cfg lt8912_init_setup[] = {
/* Digital clock en*/
	/* power down */
	{I2C_MAIN, 0x08, 0xff, 0},
	/* HPD override */
	{I2C_MAIN, 0x09, 0xff, 0},
	/* color space */
	{I2C_MAIN, 0x0a, 0xff, 0},
	/* Fixed */
	{I2C_MAIN, 0x0b, 0x7c, 0},
	/* HDCP */
	{I2C_MAIN, 0x0c, 0xff, 0},
	{I2C_MAIN, 0x42, 0x04, 0},

/*Tx Analog*/
	/* Fixed */
	{I2C_MAIN, 0x31, 0xb1, 0},
	/* V1P2 */
	{I2C_MAIN, 0x32, 0xb1, 0},
	/* Fixed */
	{I2C_MAIN, 0x33, 0x0e, 0},
	/* Fixed */
	{I2C_MAIN, 0x37, 0x00, 0},
	/* Fixed */
	{I2C_MAIN, 0x38, 0x22, 0},
	/* Fixed */
	{I2C_MAIN, 0x60, 0x82, 0},
/*Cbus Analog*/
	/* Fixed */
	{I2C_MAIN, 0x39, 0x45, 0},
	{I2C_MAIN, 0x3a, 0x00, 0}, //20180718
	{I2C_MAIN, 0x3b, 0x00, 0},
/*HDMI Pll Analog*/
	{I2C_MAIN, 0x44, 0x31, 0},
	/* Fixed */
	{I2C_MAIN, 0x55, 0x44, 0},
	/* Fixed */
	{I2C_MAIN, 0x57, 0x01, 0},
	{I2C_MAIN, 0x5a, 0x02, 0},
/*MIPI Analog*/
	/* Fixed */
	{I2C_MAIN, 0x3e, 0xd6, 0},  //0xde.  //0xf6 = pn swap
	{I2C_MAIN, 0x3f, 0xd4, 0},
	{I2C_MAIN, 0x41, 0x3c, 0},
	{I2C_MAIN, 0xB2, 0x00, 1},	/* 0 DVI, 1 HDMI */
	{I2C_CEC_DSI, 0x12,0x04, 0},
};

static struct lt8912_reg_cfg lt8912_mipi_basic_set[] = {
	{I2C_CEC_DSI, 0x14,0x00, 0},
	{I2C_CEC_DSI, 0x15,0x00, 0},
	{I2C_CEC_DSI, 0x1a,0x03, 0},
	{I2C_CEC_DSI, 0x1b,0x03, 0},
};

static struct lt8912_reg_cfg lt8912_ddsconfig[] = {
	{I2C_CEC_DSI, 0x4e,0xff, 0},
	{I2C_CEC_DSI, 0x4f,0x56, 0},
	{I2C_CEC_DSI, 0x50,0x69, 0},
	{I2C_CEC_DSI, 0x51,0x80, 0},
	{I2C_CEC_DSI, 0x1f,0x5e, 0},
	{I2C_CEC_DSI, 0x20,0x01, 0},
	{I2C_CEC_DSI, 0x21,0x2c, 0},
	{I2C_CEC_DSI, 0x22,0x01, 0},
	{I2C_CEC_DSI, 0x23,0xfa, 0},
	{I2C_CEC_DSI, 0x24,0x00, 0},
	{I2C_CEC_DSI, 0x25,0xc8, 0},
	{I2C_CEC_DSI, 0x26,0x00, 0},
	{I2C_CEC_DSI, 0x27,0x5e, 0},
	{I2C_CEC_DSI, 0x28,0x01, 0},
	{I2C_CEC_DSI, 0x29,0x2c, 0},
	{I2C_CEC_DSI, 0x2a,0x01, 0},
	{I2C_CEC_DSI, 0x2b,0xfa, 0},
	{I2C_CEC_DSI, 0x2c,0x00, 0},
	{I2C_CEC_DSI, 0x2d,0xc8, 0},
	{I2C_CEC_DSI, 0x2e,0x00, 0},
	{I2C_CEC_DSI, 0x42,0x64, 0},
	{I2C_CEC_DSI, 0x43,0x00, 0},
	{I2C_CEC_DSI, 0x44,0x04, 0},
	{I2C_CEC_DSI, 0x45,0x00, 0},
	{I2C_CEC_DSI, 0x46,0x59, 0},
	{I2C_CEC_DSI, 0x47,0x00, 0},
	{I2C_CEC_DSI, 0x48,0xf2, 0},
	{I2C_CEC_DSI, 0x49,0x06, 0},
	{I2C_CEC_DSI, 0x4a,0x00, 0},
	{I2C_CEC_DSI, 0x4b,0x72, 0},
	{I2C_CEC_DSI, 0x4c,0x45, 0},
	{I2C_CEC_DSI, 0x4d,0x00, 0},
	{I2C_CEC_DSI, 0x52,0x08, 0},
	{I2C_CEC_DSI, 0x53,0x00, 0},
	{I2C_CEC_DSI, 0x54,0xb2, 0},
	{I2C_CEC_DSI, 0x55,0x00, 0},
	{I2C_CEC_DSI, 0x56,0xe4, 0},
	{I2C_CEC_DSI, 0x57,0x0d, 0},
	{I2C_CEC_DSI, 0x58,0x00, 0},
	{I2C_CEC_DSI, 0x59,0xe4, 0},
	{I2C_CEC_DSI, 0x5a,0x8a, 0},
	{I2C_CEC_DSI, 0x5b,0x00, 0},
	{I2C_CEC_DSI, 0x5c,0x34, 0},
	{I2C_CEC_DSI, 0x1e,0x4f, 0},
	{I2C_CEC_DSI, 0x51,0x00, 0},
};

static struct lt8912_reg_cfg lt8912_rxlogicres[] = {
	{I2C_MAIN, 0x03,0x7f, 100},
	{I2C_MAIN, 0x03,0xff, 0},
};

static struct lt8912_reg_cfg lt8912_lvds_bypass_cfg[] = {
	 //lvds power up
	{I2C_MAIN, 0x44, 0x30, 0},
	{I2C_MAIN, 0x51, 0x05, 0},

	 //core pll bypass
	{I2C_MAIN, 0x50, 0x24, 0},//cp=50uA
	{I2C_MAIN, 0x51, 0x2d, 0},//Pix_clk as reference,second order passive LPF PLL
	{I2C_MAIN, 0x52, 0x04, 0},//loopdiv=0;use second-order PLL
	{I2C_MAIN, 0x69, 0x0e, 0},//CP_PRESET_DIV_RATIO
	{I2C_MAIN, 0x69, 0x8e, 0},
	{I2C_MAIN, 0x6a, 0x00, 0},
	{I2C_MAIN, 0x6c, 0xb8, 0},//RGD_CP_SOFT_K_EN,RGD_CP_SOFT_K[13:8]
	{I2C_MAIN, 0x6b, 0x51, 0},

	{I2C_MAIN, 0x04, 0xfb, 0},//core pll reset
	{I2C_MAIN, 0x04, 0xff, 0},

	//scaler bypass
	{I2C_MAIN, 0x7f, 0x00, 0},//disable scaler
	{I2C_MAIN, 0xa8, 0x13, 0},//0x13 : JEIDA, 0x33:VSEA


	{I2C_MAIN, 0x02, 0xf7, 0},//lvds pll reset
	{I2C_MAIN, 0x02, 0xff, 0},
	{I2C_MAIN, 0x03, 0xcf, 0},
	{I2C_MAIN, 0x03, 0xff, 0},
};

static inline struct lt8912 *bridge_to_lt8912(struct drm_bridge *b)
{
	return container_of(b, struct lt8912, bridge);
}

static inline struct lt8912 *connector_to_lt8912(struct drm_connector *c)
{
	return container_of(c, struct lt8912, connector);
}

/*i2c part*/
static const struct regmap_config lt8912_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xff,
};

static inline int lt8912_write(struct lt8912 *lt, u8 idx, u8 reg, u8 val) {
	return regmap_write(lt->regmap[idx], reg, val);
}

static int lt8912_write_w(struct lt8912 *lt, enum lt8912_i2c_idx idx, u8 reg, u16 val) {
	int ret;
	ret = lt8912_write(lt, idx, reg, val & 0xff);
	if (ret) {
		goto exit;
	}
	ret = lt8912_write(lt, idx, reg + 1, val >> 8);
exit:
	return ret;
}

static int lt8912_write_array(struct lt8912 *lt, struct lt8912_reg_cfg *cfg, int elements) {
	int i, ret = 0;

	for (i = 0; i < elements; i++) {
		ret = lt8912_write(lt, cfg[i].idx, cfg[i].reg, cfg[i].val);
		if (ret) {
			dev_err(lt->dev, "%s: write array error (%d)\n", __func__, ret);
			break;
		}
		if (cfg[i].sleep_in_ms)
			msleep(cfg[i].sleep_in_ms);
	}

	return ret;
}

static inline int lt8912_read(struct lt8912 *lt, u8 idx, u8 reg, u8* val) {
	unsigned int tmp_val;
	int ret;

	ret = regmap_read(lt->regmap[idx], reg, &tmp_val);
	*val = tmp_val & 0xff;

	return ret;
}

static int lt8912_init_i2c(struct lt8912 *lt) {
	struct i2c_board_info info[] = {
		{ I2C_BOARD_INFO("lt8912p0", I2C_ADDR_MAIN), },
		{ I2C_BOARD_INFO("lt8912p1", I2C_ADDR_CEC_DSI), },
		{ I2C_BOARD_INFO("lt8912p2", I2C_ADDR_I2S), }
	};
	unsigned int i;
	int ret;

	if (!lt)
		return -ENODEV;

	for (i = 0; i < ARRAY_SIZE(info); i++) {
		if (i > 0) {
			lt->i2c_client[i] = i2c_new_dummy(lt->i2c_client[0]->adapter, info[i].addr);
			if (!lt->i2c_client[i])
				return -ENODEV;
		}
		lt->regmap[i] = devm_regmap_init_i2c(lt->i2c_client[i], &lt8912_regmap_config);
		if (IS_ERR(lt->regmap[i])) {
			ret = PTR_ERR(lt->regmap[i]);
			dev_err(lt->dev, "Failed to initialize regmap: %d\n", ret);
			return ret;
		}
	}
	return 0;
}

static int lt8912_free_i2c(struct lt8912 *lt) {
	unsigned int i;
	//i2c client 0 will be freed by the system
	for (i = 1; i < 3; i++) {
		i2c_unregister_device(lt->i2c_client[i]);
	}
	return 0;
}
/*end of i2c part*/

/*electrical part*/
static int lt8912_hard_power_on(struct lt8912 *lt)
{
	gpiod_set_value_cansleep(lt->gp_reset, 0);
	msleep(20);
	return 0;
}

static void lt8912_hard_power_off(struct lt8912 *lt)
{
	gpiod_set_value_cansleep(lt->gp_reset, 1);
	msleep(20);
	lt->is_power_on = false;
}
/*End of electrical part*/

/* i2c configuration part */
static int lt8912_video_setup(struct lt8912* lt) {
	u32 hactive, h_total, hpw, hfp, hbp;
	u32 vactive, v_total, vpw, vfp, vbp;
	u8 settle = 0x08;
	int ret;

	if (!lt) {
		dev_err(lt->dev, "%s: invalid input\n", __func__);
		return -EINVAL;
	}

	dev_dbg(lt->dev, "%s: mode = %dx%d\n", __func__, lt->mode.hactive, lt->mode.vactive);

	hactive = lt->mode.hactive;
	hfp = lt->mode.hfront_porch;
	hpw = lt->mode.hsync_len;
	hbp = lt->mode.hback_porch;
	h_total = hactive + hfp + hpw + hbp;

	vactive = lt->mode.vactive;
	vfp = lt->mode.vfront_porch;
	vpw = lt->mode.vsync_len;
	vbp = lt->mode.vback_porch;
	v_total = vactive + vfp + vpw + vbp;

	if (vactive <= 600)
		settle = 0x04;
	else if (vactive == 1080)
		settle = 0x0a;

	ret = lt8912_write(lt, I2C_CEC_DSI, 0x10, 0x01);
	ret |= lt8912_write(lt, I2C_CEC_DSI, CEC_DSI_SETTLE, settle);
	ret |= lt8912_write(lt, I2C_CEC_DSI, CEC_DSI_HSYNC, hpw);
	ret |= lt8912_write(lt, I2C_CEC_DSI, CEC_DSI_VSYNC, vpw);
	ret |= lt8912_write_w(lt, I2C_CEC_DSI, CEC_DSI_HACTIVE, hactive);
	ret |= lt8912_write(lt, I2C_CEC_DSI, 0x2f, 0x0c);

	ret |= lt8912_write_w(lt, I2C_CEC_DSI, CEC_DSI_HTOTAL, h_total);
	ret |= lt8912_write_w(lt, I2C_CEC_DSI, CEC_DSI_VTOTAL, v_total);
	ret |= lt8912_write_w(lt, I2C_CEC_DSI, CEC_DSI_VBP, vbp);
	ret |= lt8912_write_w(lt, I2C_CEC_DSI, CEC_DSI_VFP, vfp);
	ret |= lt8912_write_w(lt, I2C_CEC_DSI, CEC_DSI_HBP, hbp);
	ret |= lt8912_write_w(lt, I2C_CEC_DSI, CEC_DSI_HFP, hfp);

	return ret;
}

static int lt8912_soft_power_on(struct lt8912* lt) {
	if (!lt->is_power_on) {
		u32 lanes = lt->dsi_lanes;
		lt8912_write_array(lt, lt8912_init_setup,
					ARRAY_SIZE(lt8912_init_setup));

		lt8912_write(lt, I2C_CEC_DSI, CEC_DSI_LANE,
			lanes & 3);
		lt8912_write(lt, I2C_MAIN, MAIN_LANE_EN,
			5 | (((1 << lanes) - 1) << (7 - lanes)));

		lt8912_write_array(lt, lt8912_mipi_basic_set,
                                ARRAY_SIZE(lt8912_mipi_basic_set));

		lt->is_power_on = true;
	}
	return 0;
}

static int lt8912_video_on(struct lt8912* lt) {
	int ret = -EINVAL;

	ret = lt8912_video_setup(lt);
	if (ret < 0) {
		goto end;
	}

	lt8912_write_array(lt, lt8912_ddsconfig,
				ARRAY_SIZE(lt8912_ddsconfig));

	lt8912_write_array(lt, lt8912_rxlogicres,
				ARRAY_SIZE(lt8912_rxlogicres));

	lt8912_write_array(lt,lt8912_lvds_bypass_cfg,
				ARRAY_SIZE(lt8912_lvds_bypass_cfg));

	ret = 0;

end:
	return ret;
}

/* i2c configuration part */

/* drm connector part */

static bool lt8912_cable_is_connected(struct lt8912 *lt)
{
	int ret;
	u8 reg_val;

	ret = lt8912_read(lt, I2C_MAIN, 0xC1, &reg_val);
	if (ret) {
		dev_err(lt->dev, "Failed to read status register (%d)\n", ret);
		return 0;
	}

	return (reg_val & BIT(7));
}

static enum drm_connector_status
lt8912_connector_detect(struct drm_connector *connector, bool force)
{
	struct lt8912 *lt = connector_to_lt8912(connector);

	if (lt8912_cable_is_connected(lt)) {
		return connector_status_connected;
	} else {
		return connector_status_disconnected;
	}
}

static const struct drm_connector_funcs lt8912_connector_funcs = {
	.detect = lt8912_connector_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = drm_connector_cleanup,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static void lt8912_bridge_mode_set(struct drm_bridge *bridge,
				   const struct drm_display_mode *mode,
				   const struct drm_display_mode *adj)
{
	struct lt8912 *lt = bridge_to_lt8912(bridge);
	drm_display_mode_to_videomode(adj, &lt->mode);
}

static int lt8912_connector_get_modes(struct drm_connector *connector)
{
	struct edid *edid;
	int ret, num = 0;
	struct lt8912 *lt = connector_to_lt8912(connector);
	u32 bus_format = MEDIA_BUS_FMT_RGB888_1X24;

	edid = drm_get_edid(connector, lt->ddc);
	if (edid) {
		drm_connector_update_edid_property(connector, edid);
		num = drm_add_edid_modes(connector, edid);
	} else {
		pr_err("%s: Failed to get edid !\n",__func__);
	}

	ret = drm_display_info_set_bus_formats(&connector->display_info,
					       &bus_format, 1);
	if (ret)
		return ret;

	return num;
}

static const struct drm_connector_helper_funcs lt8912_connector_helper_funcs = {
	.get_modes = lt8912_connector_get_modes,
};

/* end of drm connector part */

/* bridge part */

static void lt8912_bridge_enable(struct drm_bridge *bridge)
{
	struct lt8912 *lt = bridge_to_lt8912(bridge);
	lt8912_video_on(lt);
}

static void lt8912_check_hpd_work(struct work_struct *work)
{
	struct delayed_work *dw = to_delayed_work(work);
	struct lt8912 *lt = container_of(dw, struct lt8912,
			lt8912_check_hpd_work_id);
	struct drm_connector *connector = &lt->connector;

	drm_helper_hpd_irq_event(connector->dev);

	queue_delayed_work(lt->workq, &lt->lt8912_check_hpd_work_id, 500);
}

static int lt8912_attach_dsi(struct lt8912 *lt)
{
	struct device *dev = lt->dev;
	struct mipi_dsi_host *host;
	struct mipi_dsi_device *dsi;
	int ret = 0;
	const struct mipi_dsi_device_info info = { .type = "lt8912",
						   .channel = 0,
						   .node = NULL,
						 };

	host = of_find_mipi_dsi_host_by_node(lt->host_node);
	if (!host) {
		dev_err(dev, "failed to find dsi host\n");
		return -EPROBE_DEFER;
	}

	dsi = mipi_dsi_device_register_full(host, &info);
	if (IS_ERR(dsi)) {
		ret = PTR_ERR(dsi);
		dev_err(dev, "failed to create dsi device (%d)\n", ret);
		goto err_dsi_device;
	}

	lt->dsi = dsi;

	dsi->lanes = lt->dsi_lanes;
	dsi->format = MIPI_DSI_FMT_RGB888;

	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST | MIPI_DSI_MODE_EOT_PACKET;

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "failed to attach dsi to host\n");
		goto err_dsi_attach;
	}

	return 0;

err_dsi_attach:
	mipi_dsi_device_unregister(dsi);
err_dsi_device:
	return ret;
}

static int lt8912_detach_dsi(struct lt8912 *lt)
{
	mipi_dsi_detach(lt->dsi);
	mipi_dsi_device_unregister(lt->dsi);

	return 0;
}

static int lt8912_bridge_attach(struct drm_bridge *bridge)
{
	struct lt8912 *lt = bridge_to_lt8912(bridge);
	struct drm_connector *connector = &lt->connector;
	int ret;

	lt->connector.polled = DRM_CONNECTOR_POLL_HPD;

	ret = drm_connector_init(bridge->dev, connector,
				 &lt8912_connector_funcs,
				 DRM_MODE_CONNECTOR_HDMIA);
	if (ret) {
		dev_err(lt->dev, "failed to initialize connector\n");
		return ret;
	}

	drm_connector_helper_add(connector, &lt8912_connector_helper_funcs);
	connector->dpms = DRM_MODE_DPMS_OFF;

	ret = lt8912_attach_dsi(lt);

	drm_connector_attach_encoder(connector, bridge->encoder);

	lt8912_hard_power_on(lt);
	lt8912_soft_power_on(lt);

	INIT_DELAYED_WORK(&lt->lt8912_check_hpd_work_id, lt8912_check_hpd_work);  //there is issue in irq, so we use work queue to read connect status
	queue_delayed_work(lt->workq, &lt->lt8912_check_hpd_work_id, 10 * 100);

	return ret;
}

static void lt8912_bridge_detach(struct drm_bridge *bridge)
{
	struct lt8912 *lt = bridge_to_lt8912(bridge);

	lt8912_detach_dsi(lt);
	cancel_delayed_work_sync(&lt->lt8912_check_hpd_work_id);
	lt8912_hard_power_off(lt);

}

static const struct drm_bridge_funcs lt8912_bridge_funcs = {
	.attach = lt8912_bridge_attach,
	.detach = lt8912_bridge_detach,
	.mode_set = lt8912_bridge_mode_set,
	.enable = lt8912_bridge_enable,
};

/* end of bridge part */

static int lt8912_parse_dt(struct lt8912 *lt)
{
	struct gpio_desc *gp_reset;
	struct device_node *endpoint;
	struct device *dev = lt->dev;
	struct device_node *ddc;
	u32 dsi_lanes;
	int ret = 0;

	gp_reset = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(gp_reset)) {
		ret = PTR_ERR(gp_reset);
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "Failed to get reset gpio: %d\n", ret);
		return ret;
	}
	lt->gp_reset = gp_reset;

	ret = of_property_read_u32(dev->of_node, "dsi-lanes", &dsi_lanes);
	if (ret) {
		dev_err(dev, "Failed to get dsi lanes count: %d\n", ret);
		goto end;
	}
	lt->dsi_lanes = dsi_lanes;

	ddc = of_parse_phandle(dev->of_node, "ddc-i2c-bus", 0);
	if (ddc) {
		lt->ddc = of_find_i2c_adapter_by_node(ddc);
		of_node_put(ddc);
		if (!lt->ddc) {
			ret = -EPROBE_DEFER;
			goto end;
		}
	} else {
		dev_err(lt->dev, "%s: no ddc-i2c-bus\n", __func__);
	}

	endpoint = of_graph_get_next_endpoint(dev->of_node, NULL);
	if (!endpoint) {
		dev_err(lt->dev, "%s: Failed to get endpoint\n", __func__);
		ret = -ENODEV;
		goto end;
	}

	lt->host_node = of_graph_get_remote_port_parent(endpoint);
	if (!lt->host_node) {
		dev_err(lt->dev, "%s: Failed to get remote port\n", __func__);
		ret = -ENODEV;
		goto error_remote_port;
	}
	of_node_put(endpoint);
	return 0;


error_remote_port:
	of_node_put(endpoint);
end:
	return ret;
}

static int lt8912_put_dt(struct lt8912 *lt)
{
	of_node_put(lt->host_node);
	return 0;
}

static int lt8912_probe(struct i2c_client *client,
	 const struct i2c_device_id *id)
{
	static struct lt8912 *lt;
	int ret = 0;
	struct device *dev = &client->dev;

	lt = devm_kzalloc(dev, sizeof(struct lt8912), GFP_KERNEL);
	if (!lt)
		return -ENOMEM;

	lt->dev = dev;
	lt->i2c_client[0] = client;

	ret = lt8912_parse_dt(lt);
	if (ret) {
		dev_err(dev, "%s: Failed to parse DT, %d\n", __func__, ret);
		goto err_dt_parse;
	}

	ret = lt8912_init_i2c(lt);
	if (ret) {
		dev_err(dev, "%s: Failed to init i2c regmaps, %d\n", __func__, ret);
		goto err_i2c;
	}

	lt->workq = create_workqueue("lt8912_workq");
	if (!lt->workq) {
		dev_err(dev, "%s: workqueue creation failed.\n", __func__);
		ret = -EPERM;
		goto err_workqueue;
	}

	i2c_set_clientdata(client, lt);

	pm_runtime_enable(dev);
	pm_runtime_set_active(dev);

	/* Register DRM bridge */
	lt->bridge.funcs = &lt8912_bridge_funcs;
	lt->bridge.of_node = dev->of_node;
	drm_bridge_add(&lt->bridge);

	return 0;

err_workqueue:
	lt8912_free_i2c(lt);
err_i2c:
	lt8912_put_dt(lt);
err_dt_parse:
	return ret;
}

static int lt8912_remove(struct i2c_client *client)
{
	struct lt8912 *lt = i2c_get_clientdata(client);

	lt8912_bridge_detach(&lt->bridge);
	drm_bridge_remove(&lt->bridge);
	lt8912_free_i2c(lt);
	lt8912_put_dt(lt);
	pm_runtime_disable(&client->dev);

	return 0;
}

static const struct of_device_id lt8912_dt_match[] = {
        {.compatible = "lontium,lt8912_drm"},
        {}
};
MODULE_DEVICE_TABLE(of, lt8912_dt_match);

static const struct i2c_device_id lt8912_id[] = {
	{"lt8912", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, lt8912_id);

static struct mipi_dsi_driver lt8912_driver = {
	.driver.name = "lt8912_drm",
};

static struct i2c_driver lt8912_i2c_driver = {
	.driver = {
		.name = "lt8912_drm",
		.of_match_table = lt8912_dt_match,
		.owner = THIS_MODULE,
	},
	.probe = lt8912_probe,
	.remove = lt8912_remove,
	.id_table = lt8912_id,
};

static int __init lt8912_module_init(void)
{
	mipi_dsi_driver_register(&lt8912_driver);

	return i2c_add_driver(&lt8912_i2c_driver);
}

static void __exit lt8912_module_exit(void)
{
	i2c_del_driver(&lt8912_i2c_driver);
	mipi_dsi_driver_unregister(&lt8912_driver);
}

module_init(lt8912_module_init);
module_exit(lt8912_module_exit);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("lt8912 drm driver");
