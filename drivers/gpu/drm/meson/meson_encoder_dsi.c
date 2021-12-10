// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2016 BayLibre, SAS
 * Author: Neil Armstrong <narmstrong@baylibre.com>
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_graph.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_simple_kms_helper.h>
#include <drm/drm_bridge.h>
#include <drm/drm_bridge_connector.h>
#include <drm/drm_device.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_print.h>

#include "meson_drv.h"
#include "meson_encoder_dsi.h"
#include "meson_registers.h"
#include "meson_venc.h"
#include "meson_vclk.h"

struct meson_encoder_dsi {
	struct drm_encoder encoder;
	struct drm_bridge bridge;
	struct drm_bridge *next_bridge;
	struct meson_drm *priv;
};

#define bridge_to_meson_encoder_dsi(x) \
	container_of(x, struct meson_encoder_dsi, bridge)

static int meson_encoder_dsi_attach(struct drm_bridge *bridge,
				    enum drm_bridge_attach_flags flags)
{
	struct meson_encoder_dsi *encoder_dsi = bridge_to_meson_encoder_dsi(bridge);

	pr_err("%s:%d\n", __func__, __LINE__);

	return drm_bridge_attach(bridge->encoder, encoder_dsi->next_bridge,
				 &encoder_dsi->bridge, flags);
}

static void meson_encoder_dsi_mode_set(struct drm_bridge *bridge,
					const struct drm_display_mode *mode,
					const struct drm_display_mode *adjusted_mode)
{
	struct meson_encoder_dsi *encoder_dsi = bridge_to_meson_encoder_dsi(bridge);
	struct meson_drm *priv = encoder_dsi->priv;

	pr_err("%s:%d\n", __func__, __LINE__);

	meson_vclk_setup(priv, MESON_VCLK_TARGET_DSI, mode->clock, 0, 0, 0, false);

	meson_venc_mipi_dsi_mode_set(priv, mode);
	meson_encl_load_gamma(priv);

	writel_relaxed(0, priv->io_base + _REG(ENCL_VIDEO_EN));

	writel_bits_relaxed(BIT(3), BIT(3), priv->io_base + _REG(ENCL_VIDEO_MODE_ADV));
	writel_relaxed(0, priv->io_base + _REG(ENCL_TST_EN));
}

static void meson_encoder_dsi_atomic_enable(struct drm_bridge *bridge,
					     struct drm_bridge_state *bridge_state)
{
	struct meson_encoder_dsi *encoder_dsi = bridge_to_meson_encoder_dsi(bridge);
	struct meson_drm *priv = encoder_dsi->priv;

	pr_err("%s:%d\n", __func__, __LINE__);

	writel_bits_relaxed(BIT(0), 0, priv->io_base + _REG(VPP_WRAP_OSD1_MATRIX_EN_CTRL));

	writel_relaxed(1, priv->io_base + _REG(ENCL_VIDEO_EN));
}

static void meson_encoder_dsi_atomic_disable(struct drm_bridge *bridge,
					      struct drm_bridge_state *bridge_state)
{
	struct meson_encoder_dsi *meson_encoder_dsi =
					bridge_to_meson_encoder_dsi(bridge);
	struct meson_drm *priv = meson_encoder_dsi->priv;

	pr_err("%s:%d\n", __func__, __LINE__);

	writel_relaxed(0, priv->io_base + _REG(ENCL_VIDEO_EN));

	writel_bits_relaxed(BIT(0), BIT(0), priv->io_base + _REG(VPP_WRAP_OSD1_MATRIX_EN_CTRL));
}

static const struct drm_bridge_funcs meson_encoder_dsi_bridge_funcs = {
	.attach		= meson_encoder_dsi_attach,
	.mode_set	= meson_encoder_dsi_mode_set,
	.atomic_enable	= meson_encoder_dsi_atomic_enable,
	.atomic_disable	= meson_encoder_dsi_atomic_disable,
	.atomic_duplicate_state = drm_atomic_helper_bridge_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_bridge_destroy_state,
	.atomic_reset = drm_atomic_helper_bridge_reset,
};

int meson_encoder_dsi_init(struct meson_drm *priv)
{
	struct meson_encoder_dsi *meson_encoder_dsi;
	struct device_node *remote;
	int ret;

	pr_err("%s:%d\n", __func__, __LINE__);

	DRM_DEBUG_DRIVER("\n");

	meson_encoder_dsi = devm_kzalloc(priv->dev, sizeof(*meson_encoder_dsi), GFP_KERNEL);
	if (!meson_encoder_dsi)
		return -ENOMEM;

	/* DSI Transceiver Bridge */
	remote = of_graph_get_remote_node(priv->dev->of_node, 2, 0);
	if (!remote) {
		dev_err(priv->dev, "DSI transceiver device is disabled");
		return 0;
	}

	pr_err("%s:%d\n", __func__, __LINE__);

	meson_encoder_dsi->next_bridge = of_drm_find_bridge(remote);
	if (!meson_encoder_dsi->next_bridge) {
		dev_err(priv->dev, "Failed to find DSI transceiver bridge: %d\n", ret);
		return -EPROBE_DEFER;
	}

	pr_err("%s:%d\n", __func__, __LINE__);

	/* DSI Encoder Bridge */
	meson_encoder_dsi->bridge.funcs = &meson_encoder_dsi_bridge_funcs;
	meson_encoder_dsi->bridge.of_node = priv->dev->of_node;
	meson_encoder_dsi->bridge.type = DRM_MODE_CONNECTOR_DSI;

	drm_bridge_add(&meson_encoder_dsi->bridge);

	meson_encoder_dsi->priv = priv;

	/* Encoder */
	ret = drm_simple_encoder_init(priv->drm, &meson_encoder_dsi->encoder,
				      DRM_MODE_ENCODER_DSI);
	if (ret) {
		dev_err(priv->dev, "Failed to init HDMI encoder: %d\n", ret);
		return ret;
	}

	pr_err("%s:%d\n", __func__, __LINE__);

	meson_encoder_dsi->encoder.possible_crtcs = BIT(0);

	/* Attach DSI Encoder Bridge to Encoder */
	ret = drm_bridge_attach(&meson_encoder_dsi->encoder, &meson_encoder_dsi->bridge, NULL, 0);
	if (ret) {
		dev_err(priv->dev, "Failed to attach bridge: %d\n", ret);
		return ret;
	}

	pr_err("%s:%d\n", __func__, __LINE__);

	/*
	 * We should have now in place:
	 * encoder->[dsi encoder bridge]->[dw-mipi-dsi bridge]->[panel bridge]->[panel]
	 */

	DRM_DEBUG_DRIVER("DSI encoder initialized\n");

	return 0;
}
