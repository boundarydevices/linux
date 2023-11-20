// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek LVDS Driver
 *
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Huijuan Xie <huijuan.xie@mediatek.com>
 */

#include "mtk_lvds.h"

static void mtk_lvds_mask(struct mtk_lvds *lvds, u32 offset, u32 mask, u32 data)
{
	u32 temp = readl(lvds->regs + offset);

	writel((temp & ~mask) | (data & mask), lvds->regs + offset);
}

static inline struct mtk_lvds *bridge_to_lvds(struct drm_bridge *bridge)
{
	return container_of(bridge, struct mtk_lvds, bridge);
}

static inline struct mtk_lvds *connector_to_lvds(struct drm_connector *connector)
{
	return container_of(connector, struct mtk_lvds, connector);
}

static enum drm_connector_status mtk_lvds_connector_detect(
	struct drm_connector *connector, bool force)
{
	struct mtk_lvds *lvds = connector_to_lvds(connector);

	if (lvds->next_bridge)
		return connector_status_connected;

	return connector_status_disconnected;
}

static const struct drm_connector_funcs mtk_lvds_connector_funcs = {
	.detect = mtk_lvds_connector_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = drm_connector_cleanup,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static int mtk_lvds_connector_get_modes(struct drm_connector *connector)
{
	struct mtk_lvds *lvds = connector_to_lvds(connector);

	return lvds->next_bridge->funcs->get_modes(lvds->next_bridge, connector);
}

static struct drm_encoder *mtk_lvds_connector_best_encoder(
		struct drm_connector *connector)
{
	struct mtk_lvds *lvds = connector_to_lvds(connector);

	return lvds->bridge.encoder;
}

static const struct drm_connector_helper_funcs
		mtk_lvds_connector_helper_funcs = {
	.get_modes = mtk_lvds_connector_get_modes,
	.best_encoder = mtk_lvds_connector_best_encoder,
};

static bool mtk_lvds_bridge_mode_fixup(struct drm_bridge *bridge,
					const struct drm_display_mode *mode,
					struct drm_display_mode *adjusted_mode)
{
	return true;
}

static void mtk_lvds_poweron(struct mtk_lvds *lvds)
{
	int ret;

	if (++lvds->refcount != 1)
		return;

	ret = pm_runtime_get_sync(lvds->dev);
	if (ret < 0)
		dev_err(lvds->dev, "Failed to enable power domain: %d\n", ret);

	ret = clk_prepare_enable(lvds->dpix);
	if (ret != 0) {
		dev_err(lvds->dev, "Failed to enable dpix clock gate: %d\n",
			ret);
		goto err_phy_power_off;
	}

	ret = clk_prepare_enable(lvds->clkdig);
	if (ret != 0) {
		dev_err(lvds->dev, "Failed to enable clkdig clock gate: %d\n",
			ret);
		goto err_disable_dpix;
	}

	ret = clk_prepare_enable(lvds->pix_clk_gate);
	if (ret != 0) {
		dev_err(lvds->dev, "Failed to enable pixel clock gate: %d\n",
			ret);
		goto err_disable_clkdig;
	}

	ret = clk_prepare_enable(lvds->clkts_clk_gate);
	if (ret != 0) {
		dev_err(lvds->dev, "Failed to enable clkts clock gate: %d\n",
			ret);
		goto err_disable_pix_gate_clk;
	}

	ret = phy_power_on(lvds->phy);
	if (ret != 0) {
		dev_err(lvds->dev, "Failed to power phy: %d\n",
			ret);
		goto err_disable_cts_gate_clk;
	}

	/* LVDS Digital init */
	mtk_lvds_mask(lvds, LVDSENC_OUT_CTRL, RG_LVDSENC_LVDS_7_10_FIFO_EN,
		RG_LVDSENC_LVDS_7_10_FIFO_EN);

	mtk_lvds_mask(lvds, LVDSENC_CLK_CTRL, RG_LVDSENC_TX_CK_EN,
		RG_LVDSENC_TX_CK_EN);
	mtk_lvds_mask(lvds, LVDSENC_CLK_CTRL, RG_LVDSENC_RX_CK_EN,
		RG_LVDSENC_RX_CK_EN);
	mtk_lvds_mask(lvds, LVDSENC_CLK_CTRL, RG_LVDSENC_TEST_CK_EN,
		RG_LVDSENC_TEST_CK_EN);

	/* LVDS Digital Soft reset & un-reset*/
	mtk_lvds_mask(lvds, LVDSENC_SOFT_RESET, RG_LVDSENC_PCLK_SW_RST,
		~RG_LVDSENC_PCLK_SW_RST);
	mtk_lvds_mask(lvds, LVDSENC_SOFT_RESET, RG_LVDSENC_CTSCLK_SW_RST,
		~RG_LVDSENC_CTSCLK_SW_RST);
	mtk_lvds_mask(lvds, LVDSENC_SOFT_RESET, RG_LVDSENC_PCLK_SW_RST,
		RG_LVDSENC_PCLK_SW_RST);
	mtk_lvds_mask(lvds, LVDSENC_SOFT_RESET, RG_LVDSENC_CTSCLK_SW_RST,
		RG_LVDSENC_CTSCLK_SW_RST);

	if (lvds->connector.display_info.bpc == 6)
		mtk_lvds_mask(lvds, LVDSENC_FMT_CTRL, RG_LVDSENC_DATA_FORMAT,
			RG_LVDSENC_6_BIT_MODDE);
	else
		mtk_lvds_mask(lvds, LVDSENC_FMT_CTRL, RG_LVDSENC_DATA_FORMAT,
			RG_LVDSENC_8_BIT_MODDE);

	pr_debug("0x0=0x%x\n", readl(lvds->regs + LVDSENC_FMT_CTRL));
	pr_debug("0x18=0x%x\n", readl(lvds->regs + LVDSENC_OUT_CTRL));
	pr_debug("0x20=0x%x\n", readl(lvds->regs + LVDSENC_CLK_CTRL));

	return;

err_disable_cts_gate_clk:
	clk_disable_unprepare(lvds->clkts_clk_gate);
err_disable_pix_gate_clk:
	clk_disable_unprepare(lvds->pix_clk_gate);
err_disable_clkdig:
	clk_disable_unprepare(lvds->clkdig);
err_disable_dpix:
	clk_disable_unprepare(lvds->dpix);
err_phy_power_off:
	lvds->refcount--;
}

static void mtk_lvds_poweroff(struct mtk_lvds *lvds)
{
	int ret;
	u32 reg;

	if (lvds->refcount == 0) {
		WARN_ON(true);
		return;
	}

	if (--lvds->refcount != 0)
		return;

	reg = readl(lvds->regs + LVDSENC_OUT_CTRL) &
		 (~RG_LVDSENC_LVDS_7_10_FIFO_EN);
	writel(reg, lvds->regs + LVDSENC_OUT_CTRL);

	ret = pm_runtime_put_sync(lvds->dev);
	if (ret < 0)
		DRM_ERROR("Failed to disable power domain: %d\n", ret);

	clk_disable_unprepare(lvds->clkts_clk_gate);
	clk_disable_unprepare(lvds->pix_clk_gate);
	clk_disable_unprepare(lvds->clkdig);
	clk_disable_unprepare(lvds->dpix);
	ret = phy_power_off(lvds->phy);
	if (ret < 0)
		DRM_ERROR("Failed to disable power domain: %d\n", ret);
}

static int mtk_lvds_bridge_attach(struct drm_bridge *bridge,
	enum drm_bridge_attach_flags flags)
{
	struct mtk_lvds *lvds = bridge_to_lvds(bridge);
	int ret;

	if (bridge->encoder == NULL) {
		DRM_ERROR("Parent encoder object not found");
		return -ENODEV;
	}

	if (!(flags & DRM_BRIDGE_ATTACH_NO_CONNECTOR)) {
		dev_err(lvds->dev, "Driver does not provide a connector!");
		return -EINVAL;
	}

	if (lvds->next_bridge) {
		ret = drm_bridge_attach(bridge->encoder, lvds->next_bridge,
					&lvds->bridge, flags);
		if (ret) {
			dev_warn(lvds->dev,
				 "Failed to attach external bridge: %d\n", ret);
			goto err_connector_cleanup;
		}
	}

	return 0;

err_connector_cleanup:
	drm_connector_cleanup(&lvds->connector);
	return ret;
}

static void mtk_lvds_bridge_disable(struct drm_bridge *bridge)
{
	struct mtk_lvds *lvds = bridge_to_lvds(bridge);

	if (lvds->enabled == false)
		return;

	if (drm_panel_disable(lvds->panel)) {
		dev_err(lvds->dev, "failed to disable panel\n");
		return;
	}

	mtk_lvds_poweroff(lvds);
	lvds->enabled = false;
}

static void mtk_lvds_bridge_post_disable(struct drm_bridge *bridge)
{
	struct mtk_lvds *lvds = bridge_to_lvds(bridge);

	if (lvds->powered == false)
		return;

	if (drm_panel_unprepare(lvds->panel)) {
		dev_err(lvds->dev, "failed to unprepare panel\n");
		return;
	}

	lvds->powered = false;
}

static void mtk_lvds_bridge_mode_set(struct drm_bridge *bridge,
					const struct drm_display_mode *mode,
					const struct drm_display_mode *adjusted_mode)
{
	struct mtk_lvds *lvds = bridge_to_lvds(bridge);

	dev_dbg(lvds->dev, "cur info: name:%s, hdisplay:%d\n",
		adjusted_mode->name, adjusted_mode->hdisplay);
	dev_dbg(lvds->dev, "hsync_start:%d,hsync_end:%d, htotal:%d",
		adjusted_mode->hsync_start, adjusted_mode->hsync_end,
		adjusted_mode->htotal);
	dev_dbg(lvds->dev, "hskew:%d, vdisplay:%d\n",
		adjusted_mode->hskew, adjusted_mode->vdisplay);
	dev_dbg(lvds->dev, "vsync_start:%d, vsync_end:%d, vtotal:%d",
		adjusted_mode->vsync_start, adjusted_mode->vsync_end,
		adjusted_mode->vtotal);
	dev_dbg(lvds->dev, "vscan:%d, flag:%d\n",
		adjusted_mode->vscan, adjusted_mode->flags);

	if (lvds->connector.display_info.num_bus_formats != 0)
		lvds->lvds_fmt = lvds->connector.display_info.bus_formats[0];

	drm_mode_copy(&lvds->mode, adjusted_mode);
}

static void mtk_lvds_pre_enable(struct drm_bridge *bridge)
{
	struct mtk_lvds *lvds = bridge_to_lvds(bridge);

	if (lvds->powered)
		return;

	mtk_lvds_poweron(lvds);

	lvds->powered = true;
}

static void mtk_lvds_bridge_enable(struct drm_bridge *bridge)
{
	struct mtk_lvds *lvds = bridge_to_lvds(bridge);

	if (lvds->enabled)
		return;

	lvds->enabled = true;
}

static const struct drm_bridge_funcs mtk_lvds_bridge_funcs = {
	.attach = mtk_lvds_bridge_attach,
	.mode_fixup = mtk_lvds_bridge_mode_fixup,
	.disable = mtk_lvds_bridge_disable,
	.post_disable = mtk_lvds_bridge_post_disable,
	.mode_set = mtk_lvds_bridge_mode_set,
	.pre_enable = mtk_lvds_pre_enable,
	.enable = mtk_lvds_bridge_enable,
};

static int mtk_lvds_bind(struct device *dev, struct device *master, void *data)
{
	return 0;
}

static void mtk_lvds_unbind(struct device *dev, struct device *master,
				void *data)
{
}

static const struct component_ops mtk_lvds_component_ops = {
	.bind = mtk_lvds_bind,
	.unbind = mtk_lvds_unbind,
};

static int mtk_drm_lvds_probe(struct platform_device *pdev)
{
	struct mtk_lvds *lvds;
	struct device *dev = &pdev->dev;
	struct drm_panel *panel = NULL;
	int ret;
	struct resource *mem;

	lvds = devm_kzalloc(dev, sizeof(*lvds), GFP_KERNEL);
	if (lvds == NULL)
		return -ENOMEM;

	lvds->dev = dev;

	ret = drm_of_find_panel_or_bridge(dev->of_node, 1, 0,
						  &panel, &lvds->next_bridge);
	if (ret == -ENODEV) {
		dev_info(dev, "No panel connected in devicetree, continuing as external LVDS\n");
		lvds->next_bridge = NULL;
	} else if (ret) {
		dev_err(dev, "Failed to find panel or bridge: %d\n", ret);
		return ret;
	}

	if (panel) {
		lvds->next_bridge = devm_drm_panel_bridge_add(dev, panel);
		if (IS_ERR(lvds->next_bridge)) {
			ret = PTR_ERR(lvds->next_bridge);
			dev_err(dev, "Failed to create bridge: %d\n",
				ret);
			return -EPROBE_DEFER;
		}
	}

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	lvds->regs = devm_ioremap_resource(dev, mem);
	if (IS_ERR(lvds->regs)) {
		ret = PTR_ERR(lvds->regs);
		dev_err(dev, "Failed to ioremap mem resource: %d\n", ret);
		return ret;
	}

	lvds->pix_clk_gate = devm_clk_get(dev, "pixel");
	if (IS_ERR(lvds->pix_clk_gate)) {
		ret = PTR_ERR(lvds->pix_clk_gate);
		dev_err(dev, "Failed to get pixel clock gate: %d\n", ret);
		return ret;
	}

	lvds->clkts_clk_gate = devm_clk_get(dev, "clkts");
	if (IS_ERR(lvds->clkts_clk_gate)) {
		ret = PTR_ERR(lvds->clkts_clk_gate);
		dev_err(dev, "Failed to get clkts clock gate: %d\n", ret);
		return ret;
	}

	lvds->dpix = devm_clk_get(dev, "dpix");
	if (IS_ERR(lvds->dpix)) {
		ret = PTR_ERR(lvds->dpix);
		dev_err(dev, "Failed to get dpix clock gate: %d\n", ret);
		return ret;
	}

	lvds->clkdig = devm_clk_get(dev, "clkdig");
	if (IS_ERR(lvds->clkdig)) {
		ret = PTR_ERR(lvds->clkdig);
		dev_err(dev, "Failed to get clkdig clock gate: %d\n", ret);
		return ret;
	}

	lvds->phy = devm_phy_get(dev, "lvds");
	if (IS_ERR(lvds->phy)) {
		ret = PTR_ERR(lvds->phy);
		dev_err(dev, "Failed to get LVDS PHY: %d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, lvds);

	lvds->bridge.funcs = &mtk_lvds_bridge_funcs;
	lvds->bridge.of_node = pdev->dev.of_node;
	drm_bridge_add(&lvds->bridge);

	pm_runtime_enable(dev);

	ret = component_add(&pdev->dev, &mtk_lvds_component_ops);
	if (ret < 0) {
		dev_err(dev, "failed to add compoent, ret = %d\n", ret);
		return ret;
	}

	return 0;
}

static int mtk_drm_lvds_remove(struct platform_device *pdev)
{
	struct mtk_lvds *lvds = platform_get_drvdata(pdev);

	pm_runtime_disable(&pdev->dev);
	drm_bridge_remove(&lvds->bridge);
	return 0;
}

static const struct of_device_id mtk_drm_lvds_of_ids[] = {
	{ .compatible = "mediatek,mt8365-lvds", },
	{}
};

struct platform_driver mtk_lvds_driver = {
	.probe = mtk_drm_lvds_probe,
	.remove = mtk_drm_lvds_remove,
	.driver = {
		.name = "mediatek-drm-lvds",
		.of_match_table = mtk_drm_lvds_of_ids,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
};

module_platform_driver(mtk_lvds_driver);

MODULE_DESCRIPTION("MediaTek LVDS Driver");
MODULE_LICENSE("GPL v2");
