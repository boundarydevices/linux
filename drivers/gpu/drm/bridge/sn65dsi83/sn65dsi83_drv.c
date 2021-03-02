/*
 * Licensed under the GPL-2.
 */

#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_graph.h>
#include <linux/slab.h>

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_bridge.h>
#include <drm/drm_edid.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_print.h>
#include <drm/drm_connector.h>
#include <drm/drm_crtc_helper.h>
#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include "sn65dsi83_timing.h"
#include "sn65dsi83_brg.h"

struct sn65dsi83 {
	u8 channel_id;
	enum drm_connector_status status;
	bool powered;
	struct drm_display_mode curr_mode;
	struct drm_bridge bridge;
	struct drm_connector connector;
	struct device_node *host_node;
	struct mipi_dsi_device *dsi;
	struct sn65dsi83_brg *brg;
};

static int sn65dsi83_attach_dsi(struct sn65dsi83 *sn65dsi83);
#define DRM_DEVICE(A) A->dev->dev
/* Connector funcs */
static struct sn65dsi83 *connector_to_sn65dsi83(struct drm_connector *connector)
{
	return container_of(connector, struct sn65dsi83, connector);
}

static int sn65dsi83_connector_get_modes(struct drm_connector *connector)
{
	struct sn65dsi83 *sn65dsi83 = connector_to_sn65dsi83(connector);
	struct sn65dsi83_brg *brg = sn65dsi83->brg;
	struct device *dev = connector->dev->dev;
	struct drm_display_mode *mode;
	u32 bus_format = MEDIA_BUS_FMT_RGB888_1X24;
	u32 *bus_flags = &connector->display_info.bus_flags;
	int ret;

	dev_dbg(dev, "%s\n", __func__);
	mode = drm_mode_create(connector->dev);
	if (!mode) {
		DRM_DEV_ERROR(dev, "Failed to create display mode!\n");
		return 0;
	}

	drm_display_mode_from_videomode(&brg->vm, mode);
	mode->width_mm = brg->width_mm;
	mode->height_mm = brg->height_mm;
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;

	drm_mode_probed_add(connector, mode);
	drm_connector_list_update(connector);

	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;

	if (brg->vm.flags & DISPLAY_FLAGS_DE_HIGH)
		*bus_flags |= DRM_BUS_FLAG_DE_HIGH;
	if (brg->vm.flags & DISPLAY_FLAGS_DE_LOW)
		*bus_flags |= DRM_BUS_FLAG_DE_LOW;
	if (brg->vm.flags & DISPLAY_FLAGS_PIXDATA_NEGEDGE)
		*bus_flags |= DRM_BUS_FLAG_PIXDATA_DRIVE_NEGEDGE;
	if (brg->vm.flags & DISPLAY_FLAGS_PIXDATA_POSEDGE)
		*bus_flags |= DRM_BUS_FLAG_PIXDATA_DRIVE_POSEDGE;

	ret = drm_display_info_set_bus_formats(&connector->display_info,
					       &bus_format, 1);
	if (ret)
		return ret;

	return 1;
}

static enum drm_mode_status
sn65dsi83_connector_mode_valid(struct drm_connector *connector,
			       struct drm_display_mode *mode)
{
	struct sn65dsi83 *sn65dsi83 = connector_to_sn65dsi83(connector);
	struct device *dev = connector->dev->dev;

	if (mode->clock > (sn65dsi83->brg->vm.pixelclock / 1000))
		return MODE_CLOCK_HIGH;

	dev_dbg(dev, "%s: mode: %d*%d@%d is valid\n", __func__,
		 mode->hdisplay, mode->vdisplay, mode->clock);
	return MODE_OK;
}

static struct drm_connector_helper_funcs sn65dsi83_connector_helper_funcs = {
	.get_modes = sn65dsi83_connector_get_modes,
	.mode_valid = sn65dsi83_connector_mode_valid,
};

static enum drm_connector_status
sn65dsi83_connector_detect(struct drm_connector *connector, bool force)
{
	struct sn65dsi83 *sn65dsi83 = connector_to_sn65dsi83(connector);
	struct device *dev = connector->dev->dev;
	enum drm_connector_status status;

	dev_dbg(dev, "%s\n", __func__);

	status = connector_status_connected;
	sn65dsi83->status = status;
	return status;
}

int drm_helper_probe_single_connector_modes(struct drm_connector *connector,
					    uint32_t maxX, uint32_t maxY);

static struct drm_connector_funcs sn65dsi83_connector_funcs = {
	.dpms = drm_helper_connector_dpms,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.detect = sn65dsi83_connector_detect,
	.destroy = drm_connector_cleanup,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

/* Bridge funcs */
static struct sn65dsi83 *bridge_to_sn65dsi83(struct drm_bridge *bridge)
{
	return container_of(bridge, struct sn65dsi83, bridge);
}

static void sn65dsi83_bridge_pre_enable(struct drm_bridge *bridge)
{
	struct sn65dsi83 *sn65dsi83 = bridge_to_sn65dsi83(bridge);

	dev_dbg(DRM_DEVICE(bridge), "%s\n", __func__);
	sn65dsi83->dsi->mode_flags |= MIPI_DSI_MODE_LPM;
	sn65dsi83->brg->funcs->setup(sn65dsi83->brg);
	sn65dsi83->dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;
}

static void sn65dsi83_bridge_enable(struct drm_bridge *bridge)
{
	struct sn65dsi83 *sn65dsi83 = bridge_to_sn65dsi83(bridge);

	dev_dbg(DRM_DEVICE(bridge), "%s\n", __func__);

	sn65dsi83->brg->funcs->power_on(sn65dsi83->brg);
	sn65dsi83->brg->funcs->start_stream(sn65dsi83->brg);
}

static void sn65dsi83_bridge_disable(struct drm_bridge *bridge)
{
	struct sn65dsi83 *sn65dsi83 = bridge_to_sn65dsi83(bridge);

	dev_dbg(DRM_DEVICE(bridge), "%s\n", __func__);
	sn65dsi83->brg->funcs->stop_stream(sn65dsi83->brg);
	sn65dsi83->brg->funcs->power_off(sn65dsi83->brg);
}

static void sn65dsi83_bridge_mode_set(struct drm_bridge *bridge,
				      const struct drm_display_mode *mode,
				      const struct drm_display_mode *adj_mode)
{
	struct sn65dsi83 *sn65dsi83 = bridge_to_sn65dsi83(bridge);

	dev_dbg(DRM_DEVICE(bridge), "%s: mode: %d*%d@%d\n", __func__,
		 mode->hdisplay, mode->vdisplay, mode->clock);
	drm_mode_copy(&sn65dsi83->curr_mode, adj_mode);
}

static int sn65dsi83_bridge_attach(struct drm_bridge *bridge,
				   enum drm_bridge_attach_flags flags)
{
	struct sn65dsi83 *sn65dsi83 = bridge_to_sn65dsi83(bridge);
	int ret;

	dev_dbg(DRM_DEVICE(bridge), "%s\n", __func__);
	if (!bridge->encoder) {
		DRM_ERROR("Parent encoder object not found");
		return -ENODEV;
	}

	sn65dsi83->connector.polled = DRM_CONNECTOR_POLL_CONNECT;

	ret = drm_connector_init(bridge->dev, &sn65dsi83->connector,
				 &sn65dsi83_connector_funcs,
				 DRM_MODE_CONNECTOR_DSI);
	if (ret) {
		DRM_ERROR("Failed to initialize connector with drm\n");
		return ret;
	}
	drm_connector_helper_add(&sn65dsi83->connector,
				 &sn65dsi83_connector_helper_funcs);
	drm_connector_attach_encoder(&sn65dsi83->connector, bridge->encoder);

	ret = sn65dsi83_attach_dsi(sn65dsi83);

	return ret;
}

static struct drm_bridge_funcs sn65dsi83_bridge_funcs = {
	.pre_enable = sn65dsi83_bridge_pre_enable,
	.enable = sn65dsi83_bridge_enable,
	.disable = sn65dsi83_bridge_disable,
	.mode_set = sn65dsi83_bridge_mode_set,
	.attach = sn65dsi83_bridge_attach,
};

static int sn65dsi83_parse_dt(struct device_node *np,
			      struct sn65dsi83 *sn65dsi83)
{
	struct device *dev = &sn65dsi83->brg->client->dev;
	u32 num_lanes = 2, bpp = 24, format = 2, width = 149, height = 93;
	u32 num_ch = 2;
	struct device_node *endpoint;

	endpoint = of_graph_get_next_endpoint(np, NULL);
	if (!endpoint)
		return -ENODEV;

	sn65dsi83->host_node = of_graph_get_remote_port_parent(endpoint);
	if (!sn65dsi83->host_node) {
		of_node_put(endpoint);
		return -ENODEV;
	}

	of_property_read_u32(np, "ti,dsi-lanes", &num_lanes);
	of_property_read_u32(np, "ti,lvds-format", &format);
	of_property_read_u32(np, "ti,lvds-bpp", &bpp);
	of_property_read_u32(np, "ti,width-mm", &width);
	of_property_read_u32(np, "ti,height-mm", &height);
	of_property_read_u32(np, "ti,lvds-channels", &num_ch);

	sn65dsi83->brg->even_odd_swap = of_property_read_bool(np, "even_odd_swap");

	if (num_lanes < 1 || num_lanes > 4) {
		dev_err(dev, "Invalid dsi-lanes: %u\n", num_lanes);
		return -EINVAL;
	}
	sn65dsi83->brg->num_dsi_lanes = num_lanes;

	sn65dsi83->brg->gpio_enable =
	    devm_gpiod_get(dev, "enable", GPIOD_OUT_HIGH);
	if (IS_ERR(sn65dsi83->brg->gpio_enable)) {
		dev_err(dev, "failed to parse enable gpio");
		return PTR_ERR(sn65dsi83->brg->gpio_enable);
	}

	sn65dsi83->brg->format = format;
	sn65dsi83->brg->bpp = bpp;

	if (num_ch < 1 || num_ch > 2) {
		dev_err(dev, "Invalid lvds-channels-num: %d\n", num_ch);
		return -EINVAL;
	}
	sn65dsi83->brg->num_lvds_ch = num_ch;

	sn65dsi83->brg->width_mm = width;
	sn65dsi83->brg->height_mm = height;

	/* Read default timing if there is not device tree node for */
	if ((of_get_videomode(np, &sn65dsi83->brg->vm, 0)) < 0)
		videomode_from_timing(&panel_default_timing,
				      &sn65dsi83->brg->vm);

	of_node_put(endpoint);
	of_node_put(sn65dsi83->host_node);

	return 0;
}

static int sn65dsi83_probe(struct i2c_client *i2c,
			   const struct i2c_device_id *id)
{
	struct sn65dsi83 *sn65dsi83;
	struct device *dev = &i2c->dev;
	int ret;

	dev_dbg(dev, "%s\n", __func__);
	if (!dev->of_node)
		return -EINVAL;

	sn65dsi83 = devm_kzalloc(dev, sizeof(*sn65dsi83), GFP_KERNEL);
	if (!sn65dsi83)
		return -ENOMEM;

	/* Initialize it before DT parser */
	sn65dsi83->brg = sn65dsi83_brg_get();
	sn65dsi83->brg->client = i2c;

	sn65dsi83->powered = false;
	sn65dsi83->status = connector_status_disconnected;

	i2c_set_clientdata(i2c, sn65dsi83);

	ret = sn65dsi83_parse_dt(dev->of_node, sn65dsi83);
	if (ret)
		return ret;

	sn65dsi83->brg->funcs->power_off(sn65dsi83->brg);
	sn65dsi83->brg->funcs->power_on(sn65dsi83->brg);
	ret = sn65dsi83->brg->funcs->reset(sn65dsi83->brg);
	if (ret != 0x00) {
		dev_err(dev, "Failed to reset the device");
		return -ENODEV;
	}

	sn65dsi83->bridge.funcs = &sn65dsi83_bridge_funcs;
	sn65dsi83->bridge.of_node = dev->of_node;

	drm_bridge_add(&sn65dsi83->bridge);

	return 0;
}

static int sn65dsi83_attach_dsi(struct sn65dsi83 *sn65dsi83)
{
	struct device *dev = &sn65dsi83->brg->client->dev;
	struct mipi_dsi_host *host;
	struct mipi_dsi_device *dsi;
	int ret = 0;
	const struct mipi_dsi_device_info info = {.type = "sn65dsi83",
		.channel = 0,
		.node = NULL,
	};

	dev_dbg(dev, "%s\n", __func__);
	host = of_find_mipi_dsi_host_by_node(sn65dsi83->host_node);
	if (!host) {
		dev_err(dev, "failed to find dsi host\n");
		return -EPROBE_DEFER;
	}

	dsi = mipi_dsi_device_register_full(host, &info);
	if (IS_ERR(dsi)) {
		dev_err(dev, "failed to create dsi device\n");
		ret = PTR_ERR(dsi);
		return -ENODEV;
	}

	sn65dsi83->dsi = dsi;

	dsi->lanes = sn65dsi83->brg->num_dsi_lanes;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_MODE_VIDEO_HSE;

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "failed to attach dsi to host\n");
		mipi_dsi_device_unregister(dsi);
	}

	return ret;
}

static void sn65dsi83_detach_dsi(struct sn65dsi83 *sn65dsi83)
{
	struct device *dev = &sn65dsi83->brg->client->dev;

	dev_dbg(dev, "%s\n", __func__);
	mipi_dsi_detach(sn65dsi83->dsi);
	mipi_dsi_device_unregister(sn65dsi83->dsi);
}

static int sn65dsi83_remove(struct i2c_client *i2c)
{
	struct sn65dsi83 *sn65dsi83 = i2c_get_clientdata(i2c);
	struct device *dev = &sn65dsi83->brg->client->dev;

	dev_dbg(dev, "%s\n", __func__);

	sn65dsi83_detach_dsi(sn65dsi83);
	drm_bridge_remove(&sn65dsi83->bridge);

	return 0;
}

static const struct i2c_device_id sn65dsi83_i2c_ids[] = {
	{"sn65dsi83", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, sn65dsi83_i2c_ids);

static const struct of_device_id sn65dsi83_of_ids[] = {
	{.compatible = "ti,sn65dsi83"},
	{.compatible = "ti,sn65dsi84"},
	{}
};

MODULE_DEVICE_TABLE(of, sn65dsi83_of_ids);

static struct mipi_dsi_driver sn65dsi83_dsi_driver = {
	.driver.name = "sn65dsi83",
};

static struct i2c_driver sn65dsi83_driver = {
	.driver = {
		   .name = "sn65dsi83",
		   .of_match_table = sn65dsi83_of_ids,
		   },
	.id_table = sn65dsi83_i2c_ids,
	.probe = sn65dsi83_probe,
	.remove = sn65dsi83_remove,
};

static int __init sn65dsi83_init(void)
{
	if (IS_ENABLED(CONFIG_DRM_MIPI_DSI))
		mipi_dsi_driver_register(&sn65dsi83_dsi_driver);

	return i2c_add_driver(&sn65dsi83_driver);
}

module_init(sn65dsi83_init);

static void __exit sn65dsi83_exit(void)
{
	i2c_del_driver(&sn65dsi83_driver);

	if (IS_ENABLED(CONFIG_DRM_MIPI_DSI))
		mipi_dsi_driver_unregister(&sn65dsi83_dsi_driver);
}

module_exit(sn65dsi83_exit);

MODULE_AUTHOR("CompuLab <compulab@compula.co.il>");
MODULE_AUTHOR("Riadh Ghaddab <rghaddab@baylibre.com>");
MODULE_DESCRIPTION("SN65DSI bridge driver");
MODULE_LICENSE("GPL");
