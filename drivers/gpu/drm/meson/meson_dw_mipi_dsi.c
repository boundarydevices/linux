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

/*  MIPI DSI/VENC Color Format Definitions */
#define MIPI_DSI_VENC_COLOR_30B   0x0
#define MIPI_DSI_VENC_COLOR_24B   0x1
#define MIPI_DSI_VENC_COLOR_18B   0x2
#define MIPI_DSI_VENC_COLOR_16B   0x3

#define COLOR_16BIT_CFG_1         0x0
#define COLOR_16BIT_CFG_2         0x1
#define COLOR_16BIT_CFG_3         0x2
#define COLOR_18BIT_CFG_1         0x3
#define COLOR_18BIT_CFG_2         0x4
#define COLOR_24BIT               0x5
#define COLOR_20BIT_LOOSE         0x6
#define COLOR_24_BIT_YCBCR        0x7
#define COLOR_16BIT_YCBCR         0x8
#define COLOR_30BIT               0x9
#define COLOR_36BIT               0xa
#define COLOR_12BIT               0xb
#define COLOR_RGB_111             0xc
#define COLOR_RGB_332             0xd
#define COLOR_RGB_444             0xe

/*  MIPI DSI Relative REGISTERs Definitions */
/* For MIPI_DSI_TOP_CNTL */
#define BIT_DPI_COLOR_MODE        20
#define BIT_IN_COLOR_MODE         16
#define BIT_CHROMA_SUBSAMPLE      14
#define BIT_COMP2_SEL             12
#define BIT_COMP1_SEL             10
#define BIT_COMP0_SEL              8
#define BIT_DE_POL                 6
#define BIT_HSYNC_POL              5
#define BIT_VSYNC_POL              4
#define BIT_DPICOLORM              3
#define BIT_DPISHUTDN              2
#define BIT_EDPITE_INTR_PULSE      1
#define BIT_ERR_INTR_PULSE         0

struct meson_dw_mipi_dsi {
	struct meson_drm *priv;
	struct device *dev;
	void __iomem *base;
	struct phy *phy;
	union phy_configure_opts phy_opts;
	struct dw_mipi_dsi *dmd;
	struct dw_mipi_dsi_plat_data pdata;
	struct mipi_dsi_device *dsi_device;
	const struct drm_display_mode *mode;
	struct clk *px_clk;
};

#define encoder_to_meson_dw_mipi_dsi(x) \
	container_of(x, struct meson_dw_mipi_dsi, encoder)

static void meson_dw_mipi_dsi_hw_init(struct meson_dw_mipi_dsi *mipi_dsi)
{

	pr_err("%s:%d\n", __func__, __LINE__);
	writel_relaxed((1 << 4) | (1 << 5) | (0 << 6),
			mipi_dsi->base + MIPI_DSI_TOP_CNTL);

	writel_bits_relaxed(0xf, 0xf,
			    mipi_dsi->base + MIPI_DSI_TOP_SW_RESET);
	writel_bits_relaxed(0xf, 0,
			    mipi_dsi->base + MIPI_DSI_TOP_SW_RESET);

	writel_bits_relaxed(0x3, 0x3,
			    mipi_dsi->base + MIPI_DSI_TOP_CLK_CNTL);

	writel_relaxed(0, mipi_dsi->base + MIPI_DSI_TOP_MEM_PD);
}

static int dw_mipi_dsi_phy_init(void *priv_data)
{
	struct meson_dw_mipi_dsi *mipi_dsi = priv_data;
	unsigned int dpi_data_format, venc_data_width;
	int ret;

	pr_err("%s:%d\n", __func__, __LINE__);

	ret = clk_set_rate(mipi_dsi->px_clk, mipi_dsi->phy_opts.mipi_dphy.hs_clk_rate);
	if (ret) {
		pr_err("Failed to set DSI PLL rate %lu\n",
		       mipi_dsi->phy_opts.mipi_dphy.hs_clk_rate);

		return ret;
	}

	switch (mipi_dsi->dsi_device->format) {
	case MIPI_DSI_FMT_RGB888:
		dpi_data_format = COLOR_24BIT;
		venc_data_width = MIPI_DSI_VENC_COLOR_24B;
		break;
	case MIPI_DSI_FMT_RGB666:
		dpi_data_format = COLOR_18BIT_CFG_2;
		venc_data_width = MIPI_DSI_VENC_COLOR_18B;
		break;
	case MIPI_DSI_FMT_RGB666_PACKED:
	case MIPI_DSI_FMT_RGB565:
		return -EINVAL;
	};

	/* Configure color format for DPI register */
	writel_relaxed((dpi_data_format  << BIT_DPI_COLOR_MODE)  |
		       (venc_data_width  << BIT_IN_COLOR_MODE) |
			0 << BIT_COMP0_SEL |
			1 << BIT_COMP1_SEL |
			2 << BIT_COMP2_SEL |
			(mipi_dsi->mode->flags & DRM_MODE_FLAG_NHSYNC ? 0 : BIT(BIT_HSYNC_POL)) |
			(mipi_dsi->mode->flags & DRM_MODE_FLAG_NVSYNC ? 0 : BIT(BIT_VSYNC_POL)),
			mipi_dsi->base + MIPI_DSI_TOP_CNTL);

	phy_power_on(mipi_dsi->phy);

	return 0;
}

static void dw_mipi_dsi_phy_power_off(void *priv_data)
{
	struct meson_dw_mipi_dsi *mipi_dsi = priv_data;

	pr_err("%s:%d\n", __func__, __LINE__);

	phy_power_off(mipi_dsi->phy);
}

static int
dw_mipi_dsi_get_lane_mbps(void *priv_data, const struct drm_display_mode *mode,
			  unsigned long mode_flags, u32 lanes, u32 format,
			  unsigned int *lane_mbps)
{
	struct meson_dw_mipi_dsi *mipi_dsi = priv_data;
	int bpp;

	pr_err("%s:%d\n", __func__, __LINE__);

	mipi_dsi->mode = mode;

	bpp = mipi_dsi_pixel_format_to_bpp(mipi_dsi->dsi_device->format);

	phy_mipi_dphy_get_default_config(mode->clock * 1000,
					 bpp, mipi_dsi->dsi_device->lanes,
					 &mipi_dsi->phy_opts.mipi_dphy);

	phy_configure(mipi_dsi->phy, &mipi_dsi->phy_opts);

	*lane_mbps = mipi_dsi->phy_opts.mipi_dphy.hs_clk_rate / 1000000;

	return 0;
}

static int
dw_mipi_dsi_phy_get_timing(void *priv_data, unsigned int lane_mbps,
			   struct dw_mipi_dsi_dphy_timing *timing)
{
	/* TOFIX handle other cases */

	pr_err("%s:%d\n", __func__, __LINE__);

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

	pr_err("%s:%d\n", __func__, __LINE__);

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

	pr_err("%s:%d\n", __func__, __LINE__);

	/* Check before if we are supposed to have a sub-device... */
	if (!mipi_dsi->dsi_device) {
		dw_mipi_dsi_remove(mipi_dsi->dmd);
		//clk_disable_unprepare(mipi_dsi->px_clk);
		return -EPROBE_DEFER;
	}

	mipi_dsi->priv = priv;

	meson_dw_mipi_dsi_hw_init(mipi_dsi);

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

	pr_err("%s:%d\n", __func__, __LINE__);

	switch (device->format) {
	case MIPI_DSI_FMT_RGB888:
		break;
	case MIPI_DSI_FMT_RGB666:
		break;
	case MIPI_DSI_FMT_RGB666_PACKED:
	case MIPI_DSI_FMT_RGB565:
		DRM_DEV_ERROR(mipi_dsi->dev, "invalid pixel format %d\n", device->format);
		return -EINVAL;
	};

	phy_init(mipi_dsi->phy);

	return 0;
}

static int meson_dw_mipi_dsi_host_detach(void *priv_data,
					 struct mipi_dsi_device *device)
{
	struct meson_dw_mipi_dsi *mipi_dsi = priv_data;

	pr_err("%s:%d\n", __func__, __LINE__);

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

	pr_err("%s:%d\n", __func__, __LINE__);

	mipi_dsi = devm_kzalloc(&pdev->dev, sizeof(*mipi_dsi), GFP_KERNEL);
	if (!mipi_dsi)
		return -ENOMEM;

	mipi_dsi->dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	mipi_dsi->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(mipi_dsi->base))
		return PTR_ERR(mipi_dsi->base);

	pr_err("%s:%d\n", __func__, __LINE__);

	mipi_dsi->phy = devm_phy_get(&pdev->dev, "dphy");
	if (IS_ERR(mipi_dsi->phy)) {
		ret = PTR_ERR(mipi_dsi->phy);
		dev_err(&pdev->dev, "failed to get mipi dphy: %d\n", ret);
		return ret;
	}

	pr_err("%s:%d\n", __func__, __LINE__);

	mipi_dsi->px_clk = devm_clk_get(&pdev->dev, "px_clk");
	if (IS_ERR(mipi_dsi->px_clk)) {
		dev_err(&pdev->dev, "Unable to get PLL clk\n");
		return PTR_ERR(mipi_dsi->px_clk);
	}

	pr_err("%s:%d\n", __func__, __LINE__);

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

	pr_err("%s:%d\n", __func__, __LINE__);

	ret = clk_prepare_enable(mipi_dsi->px_clk);
	if (ret) {
		dev_err(&pdev->dev, "Unable to prepare/enable PX clock\n");
		return ret;
	}

	pr_err("%s:%d\n", __func__, __LINE__);

	reset_control_assert(top_rst);
	usleep_range(10, 20);
	reset_control_deassert(top_rst);

	pr_err("%s:%d\n", __func__, __LINE__);

	/* MIPI DSI Controller */

	mipi_dsi->pdata.base = mipi_dsi->base;
	mipi_dsi->pdata.max_data_lanes = 4;
	mipi_dsi->pdata.phy_ops = &meson_dw_mipi_dsi_phy_ops;
	mipi_dsi->pdata.host_ops = &meson_dw_mipi_dsi_host_ops;
	mipi_dsi->pdata.priv_data = mipi_dsi;
	platform_set_drvdata(pdev, mipi_dsi);

	pr_err("%s:%d\n", __func__, __LINE__);

	mipi_dsi->dmd = dw_mipi_dsi_probe(pdev, &mipi_dsi->pdata);
	if (IS_ERR(mipi_dsi->dmd)) {
		ret = PTR_ERR(mipi_dsi->dmd);
		if (ret != -EPROBE_DEFER)
			dev_err(&pdev->dev,
				"Failed to probe dw_mipi_dsi: %d\n", ret);
		goto err_clkdisable;
	}

	pr_err("%s:%d\n", __func__, __LINE__);

	return component_add(mipi_dsi->dev, &meson_dw_mipi_dsi_ops);

err_clkdisable:
	clk_disable_unprepare(mipi_dsi->px_clk);

	return ret;
}

static int meson_dw_mipi_dsi_remove(struct platform_device *pdev)
{
	struct meson_dw_mipi_dsi *mipi_dsi = dev_get_drvdata(&pdev->dev);

	pr_err("%s:%d\n", __func__, __LINE__);

	dw_mipi_dsi_remove(mipi_dsi->dmd);

	component_del(mipi_dsi->dev, &meson_dw_mipi_dsi_ops);

	clk_disable_unprepare(mipi_dsi->px_clk);

	return 0;
}

static const struct of_device_id meson_dw_mipi_dsi_of_table[] = {
	{ .compatible = "amlogic,meson-g12a-dw-mipi-dsi", },
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
