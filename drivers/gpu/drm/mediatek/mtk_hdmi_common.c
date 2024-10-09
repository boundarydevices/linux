// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014 MediaTek Inc.
 * Copyright (c) 2022 BayLibre, SAS
 * Author: Jie Qiu <jie.qiu@mediatek.com>
 */
#include "mtk_hdmi_common.h"

const char * const mtk_hdmi_clk_names_mt8183[MTK_MT8183_HDMI_CLK_COUNT] = {
	[MTK_MT8183_HDMI_CLK_HDMI_PIXEL] = "pixel",
	[MTK_MT8183_HDMI_CLK_HDMI_PLL] = "pll",
	[MTK_MT8183_HDMI_CLK_AUD_BCLK] = "bclk",
	[MTK_MT8183_HDMI_CLK_AUD_SPDIF] = "spdif",
};

struct mtk_hdmi *hdmi_ctx_from_bridge(struct drm_bridge *b)
{
	return container_of(b, struct mtk_hdmi, bridge);
}

u32 mtk_hdmi_read(struct mtk_hdmi *hdmi, u32 offset)
{
	return readl(hdmi->regs + offset);
}

void mtk_hdmi_write(struct mtk_hdmi *hdmi, u32 offset, u32 val)
{
	writel(val, hdmi->regs + offset);
}

inline void mtk_hdmi_clear_bits(struct mtk_hdmi *hdmi, u32 offset,
				       u32 bits)
{
	void __iomem *reg = hdmi->regs + offset;
	u32 tmp;

	tmp = readl(reg);
	tmp &= ~bits;
	writel(tmp, reg);
}

inline void mtk_hdmi_set_bits(struct mtk_hdmi *hdmi, u32 offset,
				     u32 bits)
{
	void __iomem *reg = hdmi->regs + offset;
	u32 tmp;

	tmp = readl(reg);
	tmp |= bits;
	writel(tmp, reg);
}

void mtk_hdmi_mask(struct mtk_hdmi *hdmi, u32 offset, u32 val, u32 mask)
{
	void __iomem *reg = hdmi->regs + offset;
	u32 tmp;

	tmp = readl(reg);
	tmp = (tmp & ~mask) | (val & mask);
	writel(tmp, reg);
}

int mtk_hdmi_setup_spd_infoframe(struct mtk_hdmi *hdmi, u8 *buffer, size_t bufsz,
					const char *vendor, const char *product)
{
	struct hdmi_spd_infoframe frame;
	ssize_t err;

	err = hdmi_spd_infoframe_init(&frame, vendor, product);
	if (err < 0) {
		dev_err(hdmi->dev, "Failed to initialize SPD infoframe: %zd\n",
			err);
		return err;
	}

	err = hdmi_spd_infoframe_pack(&frame, buffer, bufsz);
	if (err < 0) {
		dev_err(hdmi->dev, "Failed to pack SDP infoframe: %zd\n", err);
		return err;
	}

	return 0;
}

int mtk_hdmi_get_all_clk(struct mtk_hdmi *hdmi, struct device_node *np,
	const char *const *mtk_hdmi_clk_names, size_t num_clocks)
{
	int i;

	for (i = 0; i < num_clocks; i++) {
		hdmi->clk[i] = of_clk_get_by_name(np, mtk_hdmi_clk_names[i]);
		dev_err(hdmi->dev, "Getting clk name: %s\n", mtk_hdmi_clk_names[i]);

		if (IS_ERR(hdmi->clk[i]))
			return PTR_ERR(hdmi->clk[i]);
	}

	return 0;
}

struct edid *mtk_hdmi_bridge_get_edid(struct drm_bridge *bridge,
					     struct drm_connector *connector)
{
	struct mtk_hdmi *hdmi = hdmi_ctx_from_bridge(bridge);
	struct edid *edid;

	if (!hdmi->ddc_adpt)
		return NULL;
	edid = drm_get_edid(connector, hdmi->ddc_adpt);
	if (!edid)
		return NULL;
	return edid;
}

bool mtk_hdmi_bridge_mode_fixup(struct drm_bridge *bridge,
				       const struct drm_display_mode *mode,
				       struct drm_display_mode *adjusted_mode)
{
	return true;
}

void
mtk_hdmi_bridge_mode_set(struct drm_bridge *bridge,
			 const struct drm_display_mode *mode,
			 const struct drm_display_mode *adjusted_mode)
{
	struct mtk_hdmi *hdmi = hdmi_ctx_from_bridge(bridge);

	drm_mode_copy(&hdmi->mode, adjusted_mode);
}

int mtk_hdmi_setup_avi_infoframe(struct mtk_hdmi *hdmi, u8 *buffer, size_t bufsz,
					struct drm_display_mode *mode)
{
	struct hdmi_avi_infoframe frame;
	ssize_t err;

	err = drm_hdmi_avi_infoframe_from_display_mode(&frame, &hdmi->conn,
						       mode);

	if (err < 0) {
		dev_err(hdmi->dev,
			"Failed to get AVI infoframe from mode: %zd\n", err);
		return err;
	}

	err = hdmi_avi_infoframe_pack(&frame, buffer, bufsz);

	if (err < 0) {
		dev_err(hdmi->dev, "Failed to pack AVI infoframe: %zd\n", err);
		return err;
	}

	return 0;
}

void mtk_hdmi_send_infoframe(struct mtk_hdmi *hdmi, u8 *buffer_spd, size_t bufsz_spd,
			     u8 *buffer_avi, size_t bufsz_avi, struct drm_display_mode *mode)
{
	mtk_hdmi_setup_avi_infoframe(hdmi, buffer_avi, bufsz_avi, mode);
	mtk_hdmi_setup_spd_infoframe(hdmi, buffer_spd, bufsz_spd, "mediatek", "On-chip HDMI");
}

int mtk_hdmi_dt_parse_pdata(struct mtk_hdmi *hdmi, struct platform_device *pdev,
			    const char *const *clk_names, size_t num_clocks)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *cec_np, *remote, *i2c_np;
	struct platform_device *cec_pdev;
	struct regmap *regmap;
	struct resource *mem;
	int ret;
	struct mtk_hdmi_ddc *ddc;

	ret = mtk_hdmi_get_all_clk(hdmi, np, clk_names, num_clocks);
	if (ret) {
		dev_err(dev, "Failed to get all clks\n");
		return ret;
	}

	/* The CEC module handles HDMI hotplug detection */
	cec_np = of_get_compatible_child(np->parent, "mediatek,mt8173-cec");
	if (!cec_np) {
		dev_err(dev, "Failed to find CEC node\n");
		return -EINVAL;
	}

	cec_pdev = of_find_device_by_node(cec_np);
	if (!cec_pdev) {
		dev_err(hdmi->dev, "Waiting for CEC device %pOF\n",
				cec_np);
		of_node_put(cec_np);
		return -EPROBE_DEFER;
	}
	of_node_put(cec_np);
	hdmi->cec_dev = &cec_pdev->dev;
	/*
	 * The mediatek,syscon-hdmi property contains a phandle link to the
	 * MMSYS_CONFIG device and the register offset of the HDMI_SYS_CFG
	 * registers it contains.
	 */
	regmap = syscon_regmap_lookup_by_phandle(np, "mediatek,syscon-hdmi");
	ret = of_property_read_u32_index(np, "mediatek,syscon-hdmi", 1,
			&hdmi->sys_offset);
	if (IS_ERR(regmap))
		ret = PTR_ERR(regmap);
	if (ret) {
		dev_err(dev,
				"Failed to get system configuration registers: %d\n",
				ret);
		goto put_device;
	}
	hdmi->sys_regmap = regmap;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		ret = -ENOMEM;
		goto put_device;
	}

	hdmi->regs = devm_ioremap_resource(dev, mem);
	if (IS_ERR(hdmi->regs)) {
		ret = PTR_ERR(hdmi->regs);
		goto put_device;
	}

	remote = of_graph_get_remote_node(np, 1, 0);
	if (!remote) {
		ret = -EINVAL;
		goto put_device;
	}

	if (!of_device_is_compatible(remote, "hdmi-connector")) {
		hdmi->next_bridge = of_drm_find_bridge(remote);
		if (!hdmi->next_bridge) {
			dev_err(dev, "Waiting for external bridge\n");
			of_node_put(remote);
			ret = -EPROBE_DEFER;
			goto put_device;
		}
	}

	i2c_np = of_parse_phandle(pdev->dev.of_node, "ddc-i2c-bus", 0);
	if (!i2c_np) {
		of_node_put(pdev->dev.of_node);
		dev_err(dev, "Failed to find ddc-i2c-bus");
		ret = -EINVAL;
		goto put_device;
	}

	hdmi->ddc_adpt = of_find_i2c_adapter_by_node(i2c_np);
	of_node_put(i2c_np);
	if (!hdmi->ddc_adpt) {
		dev_err(dev, "Failed to get ddc i2c adapter by node");
		ret = -EPROBE_DEFER;
		goto put_device;
	}

	return 0;
put_device:
	put_device(hdmi->cec_dev);
	return ret;
}

static int mtk_hdmi_register_audio_driver(struct device *dev)
{
	struct platform_device *pdev;
	struct mtk_hdmi *hdmi = dev_get_drvdata(dev);
	struct hdmi_codec_pdata codec_data = {
		.data = hdmi,
	};

	set_hdmi_codec_pdata_mt8183(&codec_data);

	pdev = platform_device_register_data(dev, HDMI_CODEC_DRV_NAME,
					     PLATFORM_DEVID_AUTO, &codec_data,
					     sizeof(codec_data));
	if (IS_ERR(pdev))
		return PTR_ERR(pdev);

	DRM_INFO("%s driver bound to HDMI\n", HDMI_CODEC_DRV_NAME);
	return 0;
}

int mtk_drm_hdmi_probe(struct platform_device *pdev)
{
	struct mtk_hdmi *hdmi;
	struct device *dev = &pdev->dev;
	int ret;

	hdmi = devm_kzalloc(dev, sizeof(*hdmi), GFP_KERNEL);
	if (!hdmi)
		return -ENOMEM;

	hdmi->dev = dev;
	hdmi->conf = of_device_get_match_data(dev);

	hdmi->phy = devm_phy_get(dev, "hdmi");
	if (IS_ERR(hdmi->phy)) {
		ret = PTR_ERR(hdmi->phy);
		dev_err(dev, "Failed to get HDMI PHY: %d\n", ret);
		return ret;
	}

	ret = mtk_hdmi_dt_parse_pdata(hdmi, pdev, mtk_hdmi_clk_names_mt8183,
				      ARRAY_SIZE(mtk_hdmi_clk_names_mt8183));

	if (ret)
		return ret;

	platform_set_drvdata(pdev, hdmi);

	mtk_hdmi_output_init_mt8183(hdmi);
	hdmi->bridge.funcs = &mtk_mt8183_hdmi_bridge_funcs;

	hdmi->bridge.ops = DRM_BRIDGE_OP_DETECT | DRM_BRIDGE_OP_EDID | DRM_BRIDGE_OP_HPD;
	hdmi->bridge.type = DRM_MODE_CONNECTOR_HDMIA;
	hdmi->bridge.of_node = pdev->dev.of_node;
	drm_bridge_add(&hdmi->bridge);

	ret = mtk_hdmi_register_audio_driver(dev);

	if (ret)
		return ret;

	return 0;
}

int mtk_drm_hdmi_remove(struct platform_device *pdev)
{
	struct mtk_hdmi *hdmi = platform_get_drvdata(pdev);

	drm_bridge_remove(&hdmi->bridge);

	mtk_hdmi_clk_disable_mt8183(hdmi);
	i2c_put_adapter(hdmi->ddc_adpt);

	return 0;
}

static const struct mtk_hdmi_conf mtk_hdmi_conf_mt2701 = {
	.tz_disabled = true,
};

static const struct mtk_hdmi_conf mtk_hdmi_conf_mt8167 = {
	.max_mode_clock = 148500,
	.cea_modes_only = true,
};

static const struct of_device_id mtk_drm_hdmi_of_ids[] = {
	{ .compatible = "mediatek,mt2701-hdmi",
	  .data = &mtk_hdmi_conf_mt2701,
	},
	{ .compatible = "mediatek,mt8167-hdmi",
	  .data = &mtk_hdmi_conf_mt8167,
	},
	{ .compatible = "mediatek,mt8173-hdmi",
	},
	{}
};
MODULE_DEVICE_TABLE(of, mtk_drm_hdmi_of_ids);

#ifdef CONFIG_PM_SLEEP
static __maybe_unused int mtk_hdmi_suspend(struct device *dev)
{
	struct mtk_hdmi *hdmi = dev_get_drvdata(dev);

	device_set_wakeup_path(dev);
	dev_dbg(dev, "hdmi suspend success!\n");

	mtk_hdmi_clk_disable_mt8183(hdmi);

	return 0;
}

static __maybe_unused int mtk_hdmi_resume(struct device *dev)
{
	int ret = 0;
	struct mtk_hdmi *hdmi = dev_get_drvdata(dev);

	//mtk_hdmi_clk_enable(hdmi);

	dev_dbg(dev, "hdmi resume success!\n");

	//TODO:
	//ret = mtk_hdmi_clk_enable_audio(hdmi);
	if (ret)
		dev_err(dev, "hdmi resume failed!\n");

	return ret;
}
#endif

static SIMPLE_DEV_PM_OPS(mtk_hdmi_pm_ops,
			 mtk_hdmi_suspend, mtk_hdmi_resume);

static struct platform_driver mtk_hdmi_driver = {
	.probe = mtk_drm_hdmi_probe,
	.remove = mtk_drm_hdmi_remove,
	.driver = {
		.name = "mediatek-drm-hdmi",
		.of_match_table = mtk_drm_hdmi_of_ids,
		.pm = &mtk_hdmi_pm_ops,
	},
};

static struct platform_driver * const mtk_hdmi_drivers[] = {
	&mtk_hdmi_ddc_driver,
	&mtk_cec_driver,
	&mtk_hdmi_driver,
};

static int __init mtk_hdmitx_init(void)
{
	return platform_register_drivers(mtk_hdmi_drivers,
					 ARRAY_SIZE(mtk_hdmi_drivers));
}

static void __exit mtk_hdmitx_exit(void)
{
	platform_unregister_drivers(mtk_hdmi_drivers,
				    ARRAY_SIZE(mtk_hdmi_drivers));
}

module_init(mtk_hdmitx_init);
module_exit(mtk_hdmitx_exit);

MODULE_AUTHOR("Jie Qiu <jie.qiu@mediatek.com>");
MODULE_AUTHOR("Can Zeng <can.zeng@mediatek.com>");
MODULE_DESCRIPTION("MediaTek HDMI Driver");
MODULE_LICENSE("GPL");
