// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2021 BayLibre, SAS
 * Author: Neil Armstrong <narmstrong@baylibre.com>
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_graph.h>
#include <linux/reset.h>
#include <linux/phy/phy.h>

#include <video/mipi_display.h>

#include <drm/bridge/dw_mipi_dsi.h>
#include <drm/drm_mipi_dsi.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_device.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_print.h>

#include "meson_drv.h"
#include "meson_dw_mipi_dsi.h"
#include "meson_registers.h"
#include "meson_venc.h"

#define DRIVER_NAME "meson-dw-mipi-dsi"
#define DRIVER_DESC "Amlogic Meson MIPI-DSI DRM driver"

struct meson_dw_mipi_dsi {
	struct meson_drm *priv;
	struct device *dev;
	void __iomem *base;
	struct phy *phy;
	union phy_configure_opts phy_opts;
	struct dw_mipi_dsi *dmd;
	struct dw_mipi_dsi_plat_data pdata;
	struct mipi_dsi_device *dsi_device;
	struct clk *px_clk;
};

#define encoder_to_meson_dw_mipi_dsi(x) \
	container_of(x, struct meson_dw_mipi_dsi, encoder)

static int dw_mipi_dsi_phy_init(void *priv_data)
{
	struct meson_dw_mipi_dsi *mipi_dsi = priv_data;
	struct meson_drm *priv = mipi_dsi->priv;
	int ret;

	ret = clk_set_rate(mipi_dsi->px_clk, mipi_dsi->phy_opts.mipi_dphy.hs_clk_rate);
	if (ret) {
		pr_err("Failed to set DSI PLL rate %lu\n",
		       mipi_dsi->phy_opts.mipi_dphy.hs_clk_rate);

		return ret;
	}

	phy_power_on(mipi_dsi->phy);

	writel_relaxed(1, priv->io_base + _REG(ENCL_VIDEO_EN));

	return 0;
}

static void dw_mipi_dsi_phy_power_off(void *priv_data)
{
	struct meson_dw_mipi_dsi *mipi_dsi = priv_data;

	phy_power_off(mipi_dsi->phy);
}

static int
dw_mipi_dsi_get_lane_mbps(void *priv_data, const struct drm_display_mode *mode,
			  unsigned long mode_flags, u32 lanes, u32 format,
			  unsigned int *lane_mbps)
{
	struct meson_dw_mipi_dsi *mipi_dsi = priv_data;

	*lane_mbps = mipi_dsi->phy_opts.mipi_dphy.hs_clk_rate / 1000000;

	return 0;
}

static int
dw_mipi_dsi_phy_get_timing(void *priv_data, unsigned int lane_mbps,
			   struct dw_mipi_dsi_dphy_timing *timing)
{
	/* TOFIX handle other cases */

	timing->clk_lp2hs = 37;
	timing->clk_hs2lp = 135;
	timing->data_lp2hs = 50;
	timing->data_hs2lp = 3;

	return 0;
}

static int
dw_mipi_dsi_get_esc_clk_rate(void *priv_data, unsigned int *esc_clk_rate)
{
	*esc_clk_rate = 4; /* Mhz */

	return 0;
}

static const struct dw_mipi_dsi_phy_ops meson_dw_mipi_dsi_phy_ops = {
	.init = dw_mipi_dsi_phy_init,
	.power_off = dw_mipi_dsi_phy_power_off,
	.get_lane_mbps = dw_mipi_dsi_get_lane_mbps,
	.get_timing = dw_mipi_dsi_phy_get_timing,
	.get_esc_clk_rate = dw_mipi_dsi_get_esc_clk_rate,
};

static int meson_dw_mipi_dsi_bind(struct device *dev, struct device *master,
				void *data)
{
	struct meson_dw_mipi_dsi *mipi_dsi = dev_get_drvdata(dev);
	struct drm_device *drm = data;
	struct meson_drm *priv = drm->dev_private;
	struct drm_encoder *encoder;
	int ret;

	/* Check before if we are supposed to have a sub-device... */
	if (!mipi_dsi->dsi_device)
		return -EPROBE_DEFER;

	mipi_dsi->priv = priv;

	return 0;
}

static const struct component_ops meson_dw_mipi_dsi_ops = {
	.bind	= meson_dw_mipi_dsi_bind,
};

static int meson_dw_mipi_dsi_host_attach(void *priv_data,
					 struct mipi_dsi_device *device)
{
	struct meson_dw_mipi_dsi *mipi_dsi = priv_data;

	mipi_dsi->dsi_device = device;

	phy_init(mipi_dsi->phy);

	return 0;
}

static int meson_dw_mipi_dsi_host_detach(void *priv_data,
					 struct mipi_dsi_device *device)
{
	struct meson_dw_mipi_dsi *mipi_dsi = priv_data;

	if (device == mipi_dsi->dsi_device)
		mipi_dsi->dsi_device = NULL;
	else
		return -EINVAL;
	
	phy_exit(mipi_dsi->phy);

	return 0;
}

static const struct dw_mipi_dsi_host_ops meson_dw_mipi_dsi_host_ops = {
	.attach = meson_dw_mipi_dsi_host_attach,
	.detach = meson_dw_mipi_dsi_host_detach,
};

static int meson_dw_mipi_dsi_probe(struct platform_device *pdev)
{
	struct meson_dw_mipi_dsi *mipi_dsi;
	struct reset_control *top_rst;
	struct resource *res;
	int ret;

	mipi_dsi = devm_kzalloc(&pdev->dev, sizeof(*mipi_dsi), GFP_KERNEL);
	if (!mipi_dsi)
		return -ENOMEM;

	mipi_dsi->dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	mipi_dsi->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(mipi_dsi->base))
		return PTR_ERR(mipi_dsi->base);

	mipi_dsi->phy = devm_phy_get(&pdev->dev, "dphy");
	if (IS_ERR(mipi_dsi->phy)) {
		ret = PTR_ERR(mipi_dsi->phy);
		dev_err(&pdev->dev, "failed to get mipi dphy: %d\n", ret);
		return ret;
	}

	mipi_dsi->px_clk = devm_clk_get(&pdev->dev, "px_clk");
	if (IS_ERR(mipi_dsi->px_clk)) {
		dev_err(&pdev->dev, "Unable to get PLL clk\n");
		return PTR_ERR(mipi_dsi->px_clk);
	}

	/*
	 * We use a TOP reset signal because the APB reset signal
	 * is handled by the TOP control registers.
	 */
	top_rst = devm_reset_control_get_exclusive(&pdev->dev, "top");
	if (IS_ERR(top_rst)) {
		ret = PTR_ERR(top_rst);

		if (ret != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get reset control: %d\n", ret);

		return ret;
	}

	ret = clk_prepare_enable(mipi_dsi->px_clk);
	if (ret) {
		dev_err(&pdev->dev, "Unable to prepare/enable PX clock\n");
		goto err_clkdisable;
	}

	reset_control_assert(top_rst);
	usleep_range(10, 20);
	reset_control_deassert(top_rst);

	/* MIPI DSI Controller */

	mipi_dsi->pdata.base = mipi_dsi->base;
	mipi_dsi->pdata.max_data_lanes = 4;
	mipi_dsi->pdata.phy_ops = &meson_dw_mipi_dsi_phy_ops;
	mipi_dsi->pdata.host_ops = &meson_dw_mipi_dsi_host_ops;
	mipi_dsi->pdata.priv_data = mipi_dsi;
	platform_set_drvdata(pdev, mipi_dsi);

	mipi_dsi->dmd = dw_mipi_dsi_probe(pdev, &mipi_dsi->pdata);
	if (IS_ERR(mipi_dsi->dmd)) {
		ret = PTR_ERR(mipi_dsi->dmd);
		if (ret != -EPROBE_DEFER)
			dev_err(&pdev->dev,
				"Failed to probe dw_mipi_dsi: %d\n", ret);
		goto err_clkdisable;
	}

	return component_add(mipi_dsi->dev, &meson_dw_mipi_dsi_ops);

err_clkdisable:
	clk_disable_unprepare(mipi_dsi->px_clk);

	return ret;
}

static int meson_dw_mipi_dsi_remove(struct platform_device *pdev)
{
	struct meson_dw_mipi_dsi *mipi_dsi = dev_get_drvdata(&pdev->dev);

	component_del(mipi_dsi->dev, &meson_dw_mipi_dsi_ops);

	clk_disable_unprepare(mipi_dsi->px_clk);

	return 0;
}

static const struct of_device_id meson_dw_mipi_dsi_of_table[] = {
	{ .compatible = "amlogic,meson-axg-dw-mipi-dsi", },
	{ }
};
MODULE_DEVICE_TABLE(of, meson_dw_mipi_dsi_of_table);

static struct platform_driver meson_dw_mipi_dsi_platform_driver = {
	.probe		= meson_dw_mipi_dsi_probe,
	.remove		= meson_dw_mipi_dsi_remove,
	.driver		= {
		.name		= DRIVER_NAME,
		.of_match_table	= meson_dw_mipi_dsi_of_table,
	},
};
module_platform_driver(meson_dw_mipi_dsi_platform_driver);

MODULE_AUTHOR("Neil Armstrong <narmstrong@baylibre.com>");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
