// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014 MediaTek Inc.
 * Copyright (c) 2022 BayLibre, SAS
 * Author: Jie Qiu <jie.qiu@mediatek.com>
 */
#include "mtk_hdmi_common.h"

const char *const mtk_hdmi_clk_names_mt8195[MTK_MT8195_HDMI_CLK_COUNT] = {
	[MTK_MT8195_HDMI_CLK_UNIVPLL_D6D4] = "univpll_d6_d4",
	[MTK_MT8195_HDMI_CLK_MSDCPLL_D2] = "msdcpll_d2",
	[MTK_MT8195_HDMI_CLK_HDMI_APB_SEL] = "hdmi_apb_sel",
	[MTK_MT8195_HDMI_UNIVPLL_D4D8] = "univpll_d4_d8",
	[MTK_MT8195_HDIM_HDCP_SEL] = "hdcp_sel",
	[MTK_MT8195_HDMI_HDCP_24M_SEL] = "hdcp24_sel",
	[MTK_MT8195_HDMI_VPP_SPLIT_HDMI] = "split_hdmi",
};

const char * const mtk_hdmi_clk_names_mt8183[MTK_MT8183_HDMI_CLK_COUNT] = {
	[MTK_MT8183_HDMI_CLK_HDMI_PIXEL] = "pixel",
	[MTK_MT8183_HDMI_CLK_HDMI_PLL] = "pll",
	[MTK_MT8183_HDMI_CLK_AUD_BCLK] = "bclk",
	[MTK_MT8183_HDMI_CLK_AUD_SPDIF] = "spdif",
};

/* 2 - 720x480@60Hz 4:3 */
struct drm_display_mode mode_720x480_60hz_4v3 = {
	DRM_MODE("720x480", DRM_MODE_TYPE_DRIVER, 27000, 720, 736,
	798, 858, 0, 480, 489, 495, 525, 0,
	DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	.picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,};

/* 19 - 1280x720@50Hz 16:9 */
struct drm_display_mode mode_1280x720_50hz_16v9 = {
	DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 74250, 1280, 1720,
	1760, 1980, 0, 720, 725, 730, 750, 0,
	DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,};

/* 16 - 1920x1080@60Hz 16:9 */
struct drm_display_mode mode_1920x1080_60hz_16v9 = {
	DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 148500, 1920, 2008,
	2052, 2200, 0, 1080, 1084, 1089, 1125, 0,
	DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,};

/* 95 - 3840x2160@30Hz 16:9 */
struct drm_display_mode mode_3840x2160_30hz_16v9 = {
	DRM_MODE("3840x2160", DRM_MODE_TYPE_DRIVER, 297000, 3840, 4016,
	4104, 4400, 0, 2160, 2168, 2178, 2250, 0,
	DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,};

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
		dev_dbg(hdmi->dev, "Getting clk name: %s\n", mtk_hdmi_clk_names[i]);

		if (IS_ERR(hdmi->clk[i])) {
			dev_err(hdmi->dev, "Failed to get clk name: %s\n", mtk_hdmi_clk_names[i]);
			return PTR_ERR(hdmi->clk[i]);
		}
	}

	return 0;
}

void mtk_hdmi_show_EDID_raw_data(struct mtk_hdmi *hdmi, struct edid *edid)
{
	unsigned short bTemp, i, j;
	unsigned char *raw;
	unsigned char *p;

	raw = (unsigned char *)edid;

	for (bTemp = 0; bTemp < (1 + raw[0x7e]); bTemp++) {
		DRM_DEV_DEBUG_DRIVER(hdmi->dev,
			"===================================================================\n");
		DRM_DEV_DEBUG_DRIVER(hdmi->dev, "EDID Block Number=#%d\n", bTemp);
		DRM_DEV_DEBUG_DRIVER(hdmi->dev,
			"   | 00  01  02  03  04  05  06  07  08  09  0a  0b  0c  0d  0e  0f\n");
		DRM_DEV_DEBUG_DRIVER(hdmi->dev,
			"===================================================================\n");
		j = bTemp * EDID_LENGTH;
		for (i = 0; i < 8; i++) {
			p = &raw[j + i * 16];
			DRM_DEV_DEBUG_DRIVER(hdmi->dev,
				"%02x:  %02x  %02x  %02x  %02x  %02x  %02x  %02x  %02x  %02x  %02x  ",
				i * 16 + j,
				p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9]);
			DRM_DEV_DEBUG_DRIVER(hdmi->dev, "%02x  %02x  %02x  %02x  %02x  %02x\n",
				p[10], p[11], p[12], p[13], p[14], p[15]);
		}
	}
	DRM_DEV_DEBUG_DRIVER(hdmi->dev, "===========================================\n");
}

struct edid *mtk_hdmi_bridge_get_edid(struct drm_bridge *bridge,
					     struct drm_connector *connector)
{
	struct mtk_hdmi *hdmi = hdmi_ctx_from_bridge(bridge);
	struct edid *edid;

	DRM_DEV_DEBUG_DRIVER(hdmi->dev, "[%s][%d]\n", __func__, __LINE__);

	if (!hdmi->ddc_adpt)
		return NULL;
	edid = drm_get_edid(connector, hdmi->ddc_adpt);
	if (!edid)
		return NULL;

	mtk_hdmi_show_EDID_raw_data(hdmi, edid);

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

	DRM_DEV_DEBUG_DRIVER(hdmi->dev, "[%s][%d]\n", __func__, __LINE__);
	DRM_DEV_DEBUG_DRIVER(hdmi->dev,
		"vic:%d,name:%s,hdisplay:%d,fresh_rate:%d\n",
		drm_match_cea_mode(adjusted_mode),
		adjusted_mode->name,
		adjusted_mode->hdisplay,
		drm_mode_vrefresh(adjusted_mode));

	DRM_DEV_DEBUG_DRIVER(hdmi->dev,
		"hsync_start:%d,hsync_end:%d,htotal:%d,hskew:%d,vdisplay:%d",
		adjusted_mode->hsync_start,
		adjusted_mode->hsync_end,
		adjusted_mode->htotal,
		adjusted_mode->hskew,
		adjusted_mode->vdisplay);

	DRM_DEV_DEBUG_DRIVER(hdmi->dev,
		"vsync_start:%d,vsync_end:%d,vtotal:%d,vscan:%d,flag:%d",
		adjusted_mode->vsync_start,
		adjusted_mode->vsync_end,
		adjusted_mode->vtotal,
		adjusted_mode->vscan,
		adjusted_mode->flags);

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

	if (hdmi->conf && hdmi->conf->is_mt8195) {
		frame.colorimetry = hdmi->colorimtery;
		//no need, since we cannot support other extended colorimetry?
		if (frame.colorimetry == HDMI_COLORIMETRY_EXTENDED)
			frame.extended_colorimetry = hdmi->extended_colorimetry;

		/* quantiation range:limited or full */
		if (frame.colorspace == HDMI_COLORSPACE_RGB)
			frame.quantization_range = hdmi->quantization_range;
		else
			frame.ycc_quantization_range = hdmi->ycc_quantization_range;
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

static struct mtk_hdmi_ddc *hdmi_ddc_ctx_from_mtk_hdmi(struct mtk_hdmi *hdmi)
{
	return container_of(hdmi->ddc_adpt, struct mtk_hdmi_ddc, adap);
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

	if (!hdmi->conf || !hdmi->conf->is_mt8195) {
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
	}

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

	if (!hdmi->conf || !hdmi->conf->is_mt8195) {
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

	//TODO: rework this... this is ugly
	if (hdmi->conf && hdmi->conf->is_mt8195) {
		ddc = hdmi_ddc_ctx_from_mtk_hdmi(hdmi);
		ddc->regs = hdmi->regs;
	}

	return 0;
put_device:
	if (!hdmi->conf || !hdmi->conf->is_mt8195)
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

	if (hdmi->conf && hdmi->conf->is_mt8195)
		set_hdmi_codec_pdata_mt8195(&codec_data);
	else
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

	if (hdmi->conf && hdmi->conf->is_mt8195)
		ret = mtk_hdmi_dt_parse_pdata(hdmi, pdev, mtk_hdmi_clk_names_mt8195,
					      ARRAY_SIZE(mtk_hdmi_clk_names_mt8195));
	else
		ret = mtk_hdmi_dt_parse_pdata(hdmi, pdev, mtk_hdmi_clk_names_mt8183,
					      ARRAY_SIZE(mtk_hdmi_clk_names_mt8183));

	if (ret)
		return ret;

	platform_set_drvdata(pdev, hdmi);

	if (hdmi->conf && hdmi->conf->is_mt8195) {
		mtk_hdmi_output_init_mt8195(hdmi);
		hdmi->bridge.funcs = &mtk_mt8195_hdmi_bridge_funcs;
	} else {
		mtk_hdmi_output_init_mt8183(hdmi);
		hdmi->bridge.funcs = &mtk_mt8183_hdmi_bridge_funcs;
	}

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

	if (hdmi->conf && hdmi->conf->is_mt8195)
		mtk_hdmi_clk_disable_mt8195(hdmi);
	else
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

static const struct mtk_hdmi_conf mtk_hdmi_conf_mt8195 = {
	.is_mt8195 = true,
#ifdef CONFIG_DRM_MEDIATEK_HDMI_MT8195_SUSPEND_LOW_POWER
	.low_power = true,
#endif
	.reg_hdmitx_config_ofs = 0x900,
	.set_abist = set_abist_pattern,
};

static const struct mtk_hdmi_conf mtk_hdmi_conf_mt8188 = {
	.is_mt8195 = true,
#ifdef CONFIG_DRM_MEDIATEK_HDMI_MT8195_SUSPEND_LOW_POWER
	.low_power = true,
#endif
	.reg_hdmitx_config_ofs = 0xEA0,
	.set_abist = set_abist_pattern,
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
	{ .compatible = "mediatek,mt8195-hdmi",
	  .data = &mtk_hdmi_conf_mt8195,
	},
	{ .compatible = "mediatek,mt8188-hdmi",
	  .data = &mtk_hdmi_conf_mt8188,
	},
	{}
};
MODULE_DEVICE_TABLE(of, mtk_drm_hdmi_of_ids);

#ifdef CONFIG_PM_SLEEP
static __maybe_unused int mtk_hdmi_suspend(struct device *dev)
{
	struct mtk_hdmi *hdmi = dev_get_drvdata(dev);
	int ret;

	if (hdmi->conf && hdmi->conf->is_mt8195 && hdmi->conf->low_power) {
		if (hdmi->power_clk_enabled) {
			mtk_hdmi_clk_disable_mt8195(hdmi);
			ret = pm_runtime_put_sync(hdmi->dev);
			DRM_DEV_DEBUG_DRIVER(hdmi->dev, "[%s][%d],power domain off %d\n",
					     __func__, __LINE__, ret);
			hdmi->power_clk_enabled = false;
		}
	} else {
		device_set_wakeup_path(dev);
		if (!hdmi->conf || !hdmi->conf->is_mt8195)
			mtk_hdmi_clk_disable_mt8183(hdmi);
	}
	dev_dbg(dev, "hdmi suspend success!\n");
	return 0;
}

static __maybe_unused int mtk_hdmi_resume(struct device *dev)
{

	struct mtk_hdmi *hdmi = dev_get_drvdata(dev);
	int ret;

	if (hdmi->conf && hdmi->conf->is_mt8195 && hdmi->conf->low_power) {
		if (!hdmi->power_clk_enabled) {
			ret = pm_runtime_get_sync(hdmi->dev);
			DRM_DEV_DEBUG_DRIVER(hdmi->dev, "[%s][%d],power domain on %d\n",
					    __func__, __LINE__, ret);
			//TODO:
			//mtk_hdmi_clk_enable(hdmi);
			hdmi->power_clk_enabled = true;
		}

	} else {
		int ret = 0;
		struct mtk_hdmi *hdmi = dev_get_drvdata(dev);
		//mtk_hdmi_clk_enable(hdmi);
		//TODO:
		//if(!hdmi->conf || !hdmi->conf->is_mt8195)
		//ret = mtk_hdmi_clk_enable_audio(hdmi);
		if (ret)
			dev_err(dev, "hdmi resume failed!\n");
		return ret;
	}
	dev_dbg(dev, "hdmi resume success!\n");
	return 0;
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
	&mtk_hdmi_mt8195_ddc_driver,
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
MODULE_LICENSE("GPL v2");
