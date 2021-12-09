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

/* HHI Registers */
#define HHI_VIID_CLK_DIV	0x128 /* 0x4a offset in data sheet */
#define VCLK2_DIV_MASK		0xff
#define VCLK2_DIV_EN		BIT(16)
#define VCLK2_DIV_RESET		BIT(17)
#define CTS_ENCL_SEL_MASK	(0xf << 12)
#define CTS_ENCL_SEL_SHIFT	12
#define HHI_VIID_CLK_CNTL	0x12c /* 0x4b offset in data sheet */
#define VCLK2_EN		BIT(19)
#define VCLK2_SEL_MASK		(0x7 << 16)
#define VCLK2_SEL_SHIFT		16
#define VCLK2_SOFT_RESET	BIT(15)
#define VCLK2_DIV1_EN		BIT(0)
#define HHI_VID_CLK_CNTL2	0x194 /* 0x65 offset in data sheet */
#define CTS_ENCL_EN		BIT(3)

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

	return drm_bridge_attach(bridge->encoder, encoder_dsi->next_bridge,
				 &encoder_dsi->bridge, flags);
}

static void meson_encoder_dsi_enable(struct drm_bridge *bridge)
{
	struct meson_encoder_dsi *encoder_dsi = bridge_to_meson_encoder_dsi(bridge);
	struct meson_drm *priv = encoder_dsi->priv;

	writel_bits_relaxed(BIT(3), BIT(3), priv->io_base + _REG(ENCL_VIDEO_MODE_ADV));
	writel_relaxed(0, priv->io_base + _REG(ENCL_TST_EN));
}

static void meson_encoder_dsi_disable(struct drm_bridge *bridge)
{
	struct meson_encoder_dsi *encoder_dsi = bridge_to_meson_encoder_dsi(bridge);
	struct meson_drm *priv = encoder_dsi->priv;

	writel_relaxed(0, priv->io_base + _REG(ENCL_VIDEO_EN));
}

static int meson_encoder_dsi_atomic_check(struct drm_bridge *bridge,
					  struct drm_bridge_state *bridge_state,
					  struct drm_crtc_state *crtc_state,
					  struct drm_connector_state *conn_state)
{
	struct meson_encoder_dsi *encoder_dsi = bridge_to_meson_encoder_dsi(bridge);

	switch (encoder_dsi->dsi_device->format) {
	case MIPI_DSI_FMT_RGB888:
		break;
	case MIPI_DSI_FMT_RGB666:
		break;
	case MIPI_DSI_FMT_RGB666_PACKED:
	case MIPI_DSI_FMT_RGB565:
	default:
		DRM_DEV_ERROR(encoder_dsi->dev,
				"invalid pixel format %d\n",
				encoder_dsi->dsi_device->format);
		return -EINVAL;
	};

	return 0;
}

static void dw_mipi_dsi_set_vclk(struct meson_encoder_dsi *mipi_dsi,
				 struct drm_display_mode *mode)
{
	struct meson_drm *priv = mipi_dsi->priv;
	unsigned int vclk2_div;
	int ret;

	vclk2_div = mipi_dsi->phy_opts.mipi_dphy.hs_clk_rate / (mode->clock * 1000);

	/* Disable VCLK2 */
	regmap_update_bits(priv->hhi, HHI_VIID_CLK_CNTL, VCLK2_EN, 0);

	/* Setup the VCLK2 divider value */
	regmap_update_bits(priv->hhi, HHI_VIID_CLK_DIV,
				VCLK2_DIV_MASK, (vclk2_div - 1));

	/* select gp0 for vclk2 */
	regmap_update_bits(priv->hhi, HHI_VIID_CLK_CNTL,
				VCLK2_SEL_MASK, (0 << VCLK2_SEL_SHIFT));

	/* enable vclk2 gate */
	regmap_update_bits(priv->hhi, HHI_VIID_CLK_CNTL, VCLK2_EN, VCLK2_EN);

	/* select vclk2_div1 for encl */
	regmap_update_bits(priv->hhi, HHI_VIID_CLK_DIV,
				CTS_ENCL_SEL_MASK, (8 << CTS_ENCL_SEL_SHIFT));

	/* release vclk2_div_reset and enable vclk2_div */
	regmap_update_bits(priv->hhi, HHI_VIID_CLK_DIV,
				VCLK2_DIV_EN | VCLK2_DIV_RESET, VCLK2_DIV_EN);

	/* enable vclk2_div1 gate */
	regmap_update_bits(priv->hhi, HHI_VIID_CLK_CNTL,
				VCLK2_DIV1_EN, VCLK2_DIV1_EN);

	/* reset vclk2 */
	regmap_update_bits(priv->hhi, HHI_VIID_CLK_CNTL,
				VCLK2_SOFT_RESET, VCLK2_SOFT_RESET);
	regmap_update_bits(priv->hhi, HHI_VIID_CLK_CNTL,
				VCLK2_SOFT_RESET, 0);

	/* enable encl_clk */
	regmap_update_bits(priv->hhi, HHI_VID_CLK_CNTL2,
				CTS_ENCL_EN, CTS_ENCL_EN);

	usleep_range(10000, 11000);
}

static void meson_encoder_dsi_hw_init(struct meson_encoder_dsi *encoder_dsi)
{
	writel_relaxed((1 << 4) | (1 << 5) | (0 << 6),
			encoder_dsi->base + MIPI_DSI_TOP_CNTL);

	writel_bits_relaxed(0xf, 0xf,
			    encoder_dsi->base + MIPI_DSI_TOP_SW_RESET);
	writel_bits_relaxed(0xf, 0,
			    encoder_dsi->base + MIPI_DSI_TOP_SW_RESET);

	writel_bits_relaxed(0x3, 0x3,
			    encoder_dsi->base + MIPI_DSI_TOP_CLK_CNTL);

	writel_relaxed(0, encoder_dsi->base + MIPI_DSI_TOP_MEM_PD);
}

static void meson_encoder_dsi_mode_set(struct drm_bridge *bridge,
				       const struct drm_display_mode *mode,
				       const struct drm_display_mode *adjusted_mode)
{
	struct meson_encoder_dsi *encoder_dsi = bridge_to_meson_encoder_dsi(bridge);
	unsigned int dpi_data_format, venc_data_width;
	struct meson_drm *priv = encoder_dsi->priv;
	int bpp;
	u32 reg;

	encoder_dsi->mode_flags = mode->flags;

	bpp = encoder_dsi_pixel_format_to_bpp(mipi_dsi->dsi_device->format);

	phy_mipi_dphy_get_default_config(mode->clock * 1000,
					 bpp, encoder_dsi->dsi_device->lanes,
					 &encoder_dsi->phy_opts.mipi_dphy);

	phy_configure(encoder_dsi->phy, &mipi_dsi->phy_opts);

	switch (encoder_dsi->dsi_device->format) {
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
		/* invalid */
		break;
	};

	dw_encoder_dsi_set_vclk(mipi_dsi, mode);
	meson_venc_encoder_dsi_mode_set(priv, mode);

	meson_encl_load_gamma(priv);

	writel_relaxed(0, priv->io_base + _REG(ENCL_VIDEO_EN));

	meson_encoder_dsi_hw_init(encoder_dsi);

	/* Configure Set color format for DPI register */
	reg = readl_relaxed(encoder_dsi->base + MIPI_DSI_TOP_CNTL) &
		~(0xf<<BIT_DPI_COLOR_MODE) &
		~(0x7<<BIT_IN_COLOR_MODE) &
		~(0x3<<BIT_CHROMA_SUBSAMPLE);

	writel_relaxed(reg |
		(dpi_data_format  << BIT_DPI_COLOR_MODE)  |
		(venc_data_width  << BIT_IN_COLOR_MODE) |
		0 << BIT_COMP0_SEL |
		1 << BIT_COMP1_SEL |
		2 << BIT_COMP2_SEL |
		(encoder_dsi->mode_flags & DRM_MODE_FLAG_NHSYNC ? 0 : BIT(BIT_HSYNC_POL)) |
		(encoder_dsi->mode_flags & DRM_MODE_FLAG_NVSYNC ? 0 : BIT(BIT_VSYNC_POL)),
		encoder_dsi->base + MIPI_DSI_TOP_CNTL);
}

static const struct drm_bridge_funcs meson_encoder_dsi_bridge_funcs = {
	.attach		= meson_encoder_dsi_attach,
	.disable	= meson_encoder_dsi_disable,
	.enable		= meson_encoder_dsi_enable,
	.atomic_check	= meson_encoder_dsi_atomic_check,
	.mode_set	= meson_encoder_dsi_mode_set,
};

int meson_encoder_dsi_init(struct meson_drm *priv)
{
	struct meson_encoder_dsi *meson_encoder_dsi;
	struct device_node *remote;
	int ret;

	DRM_DEBUG_DRIVER("\n");

	meson_encoder_dsi = devm_kzalloc(priv->dev, sizeof(*meson_encoder_dsi), GFP_KERNEL);
	if (!meson_encoder_dsi)
		return -ENOMEM;

	/* DSI Transceiver Bridge */
	remote = of_graph_get_remote_node(priv->dev->of_node, 1, 0);
	if (!remote) {
		dev_err(priv->dev, "DSI transceiver device is disabled");
		return 0;
	}

	meson_encoder_dsi->next_bridge = of_drm_find_bridge(remote);
	if (!meson_encoder_dsi->next_bridge) {
		dev_err(priv->dev, "Failed to find DSI transceiver bridge: %d\n", ret);
		return -EPROBE_DEFER;
	}

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

	meson_encoder_dsi->encoder.possible_crtcs = BIT(0);

	/* Attach DSI Encoder Bridge to Encoder */
	ret = drm_bridge_attach(&meson_encoder_dsi->encoder, &meson_encoder_dsi->bridge, NULL, 0);
	if (ret) {
		dev_err(priv->dev, "Failed to attach bridge: %d\n", ret);
		return ret;
	}

	/*
	 * We should have now in place:
	 * encoder->[dsi encoder bridge]->[dw-mipi-dsi bridge]->[panel bridge]->[panel]
	 */

	DRM_DEBUG_DRIVER("DSI encoder initialized\n");

	return 0;
}
