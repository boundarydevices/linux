// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/soc/mediatek/mtk-cmdq.h>

#include "mtk_disp_drv.h"
#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"

/*
 * struct mtk_disp_padding - DISP_RDMA driver structure
 * @data: local driver data
 */
struct mtk_disp_padding {
	struct clk		*clk;
	void __iomem		*regs;
	struct cmdq_client_reg	cmdq_reg;
};

static int mtk_disp_padding_bind(struct device *dev, struct device *master,
				void *data)
{
	return 0;
}

static void mtk_disp_padding_unbind(struct device *dev, struct device *master,
				void *data)
{
}

static const struct component_ops mtk_disp_padding_component_ops = {
	.bind	= mtk_disp_padding_bind,
	.unbind = mtk_disp_padding_unbind,
};

static int mtk_disp_padding_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_padding *priv;
	struct resource *res;
	int irq;
	int ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(priv->clk)) {
		dev_err(dev, "failed to get clk\n");
		return PTR_ERR(priv->clk);
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	priv->regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(priv->regs)) {
		dev_err(dev, "failed to do ioremap\n");
		return PTR_ERR(priv->regs);
	}

#if IS_REACHABLE(CONFIG_MTK_CMDQ)
	ret = cmdq_dev_get_client_reg(dev, &priv->cmdq_reg, 0);
	if (ret) {
		dev_err(dev, "failed to get gce client reg\n");
		return ret;
	}
#endif

	platform_set_drvdata(pdev, priv);

	pm_runtime_enable(dev);

	ret = component_add(dev, &mtk_disp_padding_component_ops);
	if (ret) {
		pm_runtime_disable(dev);
		dev_err(dev, "failed to add component: %d\n", ret);
	}

	return ret;
}

static int mtk_disp_padding_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &mtk_disp_padding_component_ops);
	pm_runtime_disable(&pdev->dev);
	return 0;
}

static const struct of_device_id mtk_disp_padding_driver_dt_match[] = {
	{ .compatible = "mediatek,mt8188-vdo1-padding" },
	{},
};
MODULE_DEVICE_TABLE(of, mtk_disp_padding_driver_dt_match);

struct platform_driver mtk_disp_padding_driver = {
	.probe		= mtk_disp_padding_probe,
	.remove		= mtk_disp_padding_remove,
	.driver		= {
		.name	= "mediatek-disp-padding",
		.owner	= THIS_MODULE,
		.of_match_table = mtk_disp_padding_driver_dt_match,
	},
};

int mtk_disp_padding_clk_enable(struct device *dev)
{
	struct mtk_disp_padding *padding = dev_get_drvdata(dev);

	return clk_prepare_enable(padding->clk);
}

void mtk_disp_padding_clk_disable(struct device *dev)
{
	struct mtk_disp_padding *padding = dev_get_drvdata(dev);

	clk_disable_unprepare(padding->clk);
}

void mtk_disp_padding_config(struct device *dev, struct cmdq_pkt *cmdq_pkt)
{
	struct mtk_disp_padding *padding = dev_get_drvdata(dev);

	// bypass padding
	mtk_ddp_write_mask(cmdq_pkt,
		0b11, &padding->cmdq_reg, padding->regs, 0, 0b11);
}
