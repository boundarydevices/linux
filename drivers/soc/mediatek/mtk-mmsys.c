// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: James Liao <jamesjj.liao@mediatek.com>
 */

#include <linux/device.h>
#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/soc/mediatek/mtk-mmsys.h>
#include <linux/soc/mediatek/mtk-cmdq.h>

#include "mtk-mmsys.h"
#include "mt8167-mmsys.h"
#include "mt8183-mmsys.h"
#include "mt8192-mmsys.h"
#include "mt8365-mmsys.h"

static const struct mtk_mmsys_driver_data mt2701_mmsys_driver_data = {
	.clk_driver = "clk-mt2701-mm",
	.routes = mmsys_default_routing_table,
	.num_routes = ARRAY_SIZE(mmsys_default_routing_table),
};

static const struct mtk_mmsys_driver_data mt2712_mmsys_driver_data = {
	.clk_driver = "clk-mt2712-mm",
	.routes = mmsys_default_routing_table,
	.num_routes = ARRAY_SIZE(mmsys_default_routing_table),
};

static const struct mtk_mmsys_driver_data mt6779_mmsys_driver_data = {
	.clk_driver = "clk-mt6779-mm",
};

static const struct mtk_mmsys_driver_data mt6797_mmsys_driver_data = {
	.clk_driver = "clk-mt6797-mm",
};

static const struct mtk_mmsys_driver_data mt8167_mmsys_driver_data = {
	.clk_driver = "clk-mt8167-mm",
	.routes = mt8167_mmsys_routing_table,
	.num_routes = ARRAY_SIZE(mt8167_mmsys_routing_table),
};

static const struct mtk_mmsys_driver_data mt8173_mmsys_driver_data = {
	.clk_driver = "clk-mt8173-mm",
	.routes = mmsys_default_routing_table,
	.num_routes = ARRAY_SIZE(mmsys_default_routing_table),
};

static const struct mtk_mmsys_driver_data mt8183_mmsys_driver_data = {
	.clk_driver = "clk-mt8183-mm",
	.routes = mmsys_mt8183_routing_table,
	.num_routes = ARRAY_SIZE(mmsys_mt8183_routing_table),
	.mdp_routes = mmsys_mt8183_mdp_routing_table,
	.mdp_num_routes = ARRAY_SIZE(mmsys_mt8183_mdp_routing_table),
	.mdp_isp_ctrl = mmsys_mt8183_mdp_isp_ctrl_table,
};

static const struct mtk_mmsys_driver_data mt8192_mmsys_driver_data = {
	.clk_driver = "clk-mt8192-mm",
	.routes = mmsys_mt8192_routing_table,
	.num_routes = ARRAY_SIZE(mmsys_mt8192_routing_table),
};

static const struct mtk_mmsys_driver_data mt8365_mmsys_driver_data = {
	.clk_driver = "clk-mt8365-mm",
	.routes = mt8365_mmsys_routing_table,
	.num_routes = ARRAY_SIZE(mt8365_mmsys_routing_table),
};

struct mtk_mmsys {
	void __iomem *regs;
	const struct mtk_mmsys_driver_data *data;
	phys_addr_t addr;
	u8 subsys_id;
};

void mtk_mmsys_ddp_connect(struct device *dev,
			   enum mtk_ddp_comp_id cur,
			   enum mtk_ddp_comp_id next)
{
	struct mtk_mmsys *mmsys = dev_get_drvdata(dev);
	const struct mtk_mmsys_routes *routes = mmsys->data->routes;
	u32 reg;
	int i;

	for (i = 0; i < mmsys->data->num_routes; i++)
		if (cur == routes[i].from_comp && next == routes[i].to_comp) {
			reg = readl_relaxed(mmsys->regs + routes[i].addr);
			reg &= ~routes[i].mask;
			reg |= routes[i].val;
			writel_relaxed(reg, mmsys->regs + routes[i].addr);
		}
}
EXPORT_SYMBOL_GPL(mtk_mmsys_ddp_connect);

void mtk_mmsys_ddp_disconnect(struct device *dev,
			      enum mtk_ddp_comp_id cur,
			      enum mtk_ddp_comp_id next)
{
	struct mtk_mmsys *mmsys = dev_get_drvdata(dev);
	const struct mtk_mmsys_routes *routes = mmsys->data->routes;
	u32 reg;
	int i;

	for (i = 0; i < mmsys->data->num_routes; i++)
		if (cur == routes[i].from_comp && next == routes[i].to_comp) {
			reg = readl_relaxed(mmsys->regs + routes[i].addr);
			reg &= ~routes[i].mask;
			writel_relaxed(reg, mmsys->regs + routes[i].addr);
		}
}
EXPORT_SYMBOL_GPL(mtk_mmsys_ddp_disconnect);

void mtk_mmsys_mdp_connect(struct device *dev, struct mmsys_cmdq_cmd *cmd,
			   enum mtk_mdp_comp_id cur,
			   enum mtk_mdp_comp_id next)
{
	struct mtk_mmsys *mmsys = dev_get_drvdata(dev);
	const struct mtk_mmsys_routes *routes = mmsys->data->mdp_routes;
	int i;

	WARN_ON(!routes);
	WARN_ON(mmsys->subsys_id == 0);
	for (i = 0; i < mmsys->data->mdp_num_routes; i++)
		if (cur == routes[i].from_comp && next == routes[i].to_comp)
			cmdq_pkt_write_mask(cmd->pkt, mmsys->subsys_id,
					    mmsys->addr + routes[i].addr,
					    routes[i].val, 0xFFFFFFFF);
}
EXPORT_SYMBOL_GPL(mtk_mmsys_mdp_connect);

void mtk_mmsys_mdp_disconnect(struct device *dev, struct mmsys_cmdq_cmd *cmd,
			      enum mtk_mdp_comp_id cur,
			      enum mtk_mdp_comp_id next)
{
	struct mtk_mmsys *mmsys = dev_get_drvdata(dev);
	const struct mtk_mmsys_routes *routes = mmsys->data->mdp_routes;
	int i;

	WARN_ON(mmsys->subsys_id == 0);
	for (i = 0; i < mmsys->data->mdp_num_routes; i++)
		if (cur == routes[i].from_comp && next == routes[i].to_comp)
			cmdq_pkt_write_mask(cmd->pkt, mmsys->subsys_id,
					    mmsys->addr + routes[i].addr,
					    0, 0xFFFFFFFF);
}
EXPORT_SYMBOL_GPL(mtk_mmsys_mdp_disconnect);

void mtk_mmsys_mdp_isp_ctrl(struct device *dev, struct mmsys_cmdq_cmd *cmd,
			    enum mtk_mdp_comp_id id)
{
	struct mtk_mmsys *mmsys = dev_get_drvdata(dev);
	const unsigned int *isp_ctrl = mmsys->data->mdp_isp_ctrl;
	u32 reg;

	WARN_ON(mmsys->subsys_id == 0);
	/* Direct link */
	if (id == MDP_COMP_CAMIN) {
		/* Reset MDP_DL_ASYNC_TX */
		/* Bit  3: MDP_DL_ASYNC_TX / MDP_RELAY */
		if (isp_ctrl[ISP_CTRL_MMSYS_SW0_RST_B]) {
			reg = mmsys->addr + isp_ctrl[ISP_CTRL_MMSYS_SW0_RST_B];
			cmdq_pkt_write_mask(cmd->pkt, mmsys->subsys_id, reg,
					    0x0, 0x00000008);
			cmdq_pkt_write_mask(cmd->pkt, mmsys->subsys_id, reg,
					    1 << 3, 0x00000008);
		}

		/* Reset MDP_DL_ASYNC_RX */
		/* Bit  10: MDP_DL_ASYNC_RX */
		if (isp_ctrl[ISP_CTRL_MMSYS_SW1_RST_B]) {
			reg = mmsys->addr + isp_ctrl[ISP_CTRL_MMSYS_SW1_RST_B];
			cmdq_pkt_write_mask(cmd->pkt, mmsys->subsys_id, reg,
					    0x0, 0x00000400);
			cmdq_pkt_write_mask(cmd->pkt, mmsys->subsys_id, reg,
					    1 << 10, 0x00000400);
		}

		/* Enable sof mode */
		if (isp_ctrl[ISP_CTRL_ISP_RELAY_CFG_WD]) {
			reg = mmsys->addr + isp_ctrl[ISP_CTRL_ISP_RELAY_CFG_WD];
			cmdq_pkt_write_mask(cmd->pkt, mmsys->subsys_id, reg,
					    0 << 31, 0x80000000);
		}
	}

	if (id == MDP_COMP_CAMIN2) {
		/* Reset MDP_DL_ASYNC2_TX */
		/* Bit  4: MDP_DL_ASYNC2_TX / MDP_RELAY2 */
		if (isp_ctrl[ISP_CTRL_MMSYS_SW0_RST_B]) {
			reg = mmsys->addr + isp_ctrl[ISP_CTRL_MMSYS_SW0_RST_B];
			cmdq_pkt_write_mask(cmd->pkt, mmsys->subsys_id, reg,
					    0x0, 0x00000010);
			cmdq_pkt_write_mask(cmd->pkt, mmsys->subsys_id, reg,
					    1 << 4, 0x00000010);
		}

		/* Reset MDP_DL_ASYNC2_RX */
		/* Bit  11: MDP_DL_ASYNC2_RX */
		if (isp_ctrl[ISP_CTRL_MMSYS_SW1_RST_B]) {
			reg = mmsys->addr + isp_ctrl[ISP_CTRL_MMSYS_SW1_RST_B];
			cmdq_pkt_write_mask(cmd->pkt, mmsys->subsys_id, reg,
					    0x0, 0x00000800);
			cmdq_pkt_write_mask(cmd->pkt, mmsys->subsys_id, reg,
					    1 << 11, 0x00000800);
		}

		/* Enable sof mode */
		if (isp_ctrl[ISP_CTRL_IPU_RELAY_CFG_WD]) {
			reg = mmsys->addr + isp_ctrl[ISP_CTRL_IPU_RELAY_CFG_WD];
			cmdq_pkt_write_mask(cmd->pkt, mmsys->subsys_id, reg,
					    0 << 31, 0x80000000);
		}
	}
}
EXPORT_SYMBOL_GPL(mtk_mmsys_mdp_isp_ctrl);

void mtk_mmsys_mdp_camin_ctrl(struct device *dev, struct mmsys_cmdq_cmd *cmd,
			      enum mtk_mdp_comp_id id, u32 camin_w, u32 camin_h)
{
	struct mtk_mmsys *mmsys = dev_get_drvdata(dev);
	const unsigned int *isp_ctrl = mmsys->data->mdp_isp_ctrl;
	u32 reg;

	WARN_ON(mmsys->subsys_id == 0);
	/* Config for direct link */
	if (id == MDP_COMP_CAMIN) {
		if (isp_ctrl[ISP_CTRL_MDP_ASYNC_CFG_WD]) {
			reg = mmsys->addr + isp_ctrl[ISP_CTRL_MDP_ASYNC_CFG_WD];
			cmdq_pkt_write_mask(cmd->pkt, mmsys->subsys_id, reg,
				     (camin_h << 16) + camin_w, 0x3FFF3FFF);
		}

		if (isp_ctrl[ISP_CTRL_ISP_RELAY_CFG_WD]) {
			reg = mmsys->addr + isp_ctrl[ISP_CTRL_ISP_RELAY_CFG_WD];
			cmdq_pkt_write_mask(cmd->pkt, mmsys->subsys_id, reg,
				     (camin_h << 16) + camin_w, 0x3FFF3FFF);
		}
	}
	if (id == MDP_COMP_CAMIN2) {
		if (isp_ctrl[ISP_CTRL_MDP_ASYNC_IPU_CFG_WD]) {
			reg = mmsys->addr +
			      isp_ctrl[ISP_CTRL_MDP_ASYNC_IPU_CFG_WD];
			cmdq_pkt_write_mask(cmd->pkt, mmsys->subsys_id, reg,
				     (camin_h << 16) + camin_w, 0x3FFF3FFF);
		}
		if (isp_ctrl[ISP_CTRL_IPU_RELAY_CFG_WD]) {
			reg = mmsys->addr + isp_ctrl[ISP_CTRL_IPU_RELAY_CFG_WD];
			cmdq_pkt_write_mask(cmd->pkt, mmsys->subsys_id, reg,
				     (camin_h << 16) + camin_w, 0x3FFF3FFF);
		}
	}
}
EXPORT_SYMBOL_GPL(mtk_mmsys_mdp_camin_ctrl);

static int mtk_mmsys_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct platform_device *clks;
	struct platform_device *drm;
	struct mtk_mmsys *mmsys;
	struct resource res;
	struct cmdq_client_reg cmdq_reg;
	int ret;

	mmsys = devm_kzalloc(dev, sizeof(*mmsys), GFP_KERNEL);
	if (!mmsys)
		return -ENOMEM;

	mmsys->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(mmsys->regs)) {
		ret = PTR_ERR(mmsys->regs);
		dev_err(dev, "Failed to ioremap mmsys registers: %d\n", ret);
		return ret;
	}

	if (of_address_to_resource(dev->of_node, 0, &res) < 0)
		mmsys->addr = 0L;
	else
		mmsys->addr = res.start;

	if (cmdq_dev_get_client_reg(dev, &cmdq_reg, 0) != 0)
		dev_info(dev, "cmdq subsys id has not been set\n");
	mmsys->subsys_id = cmdq_reg.subsys;

	mmsys->data = of_device_get_match_data(&pdev->dev);
	platform_set_drvdata(pdev, mmsys);

	clks = platform_device_register_data(&pdev->dev, mmsys->data->clk_driver,
					     PLATFORM_DEVID_AUTO, NULL, 0);
	if (IS_ERR(clks))
		return PTR_ERR(clks);

	drm = platform_device_register_data(&pdev->dev, "mediatek-drm",
					    PLATFORM_DEVID_AUTO, NULL, 0);
	if (IS_ERR(drm)) {
		platform_device_unregister(clks);
		return PTR_ERR(drm);
	}

	return 0;
}

static const struct of_device_id of_match_mtk_mmsys[] = {
	{
		.compatible = "mediatek,mt2701-mmsys",
		.data = &mt2701_mmsys_driver_data,
	},
	{
		.compatible = "mediatek,mt2712-mmsys",
		.data = &mt2712_mmsys_driver_data,
	},
	{
		.compatible = "mediatek,mt6779-mmsys",
		.data = &mt6779_mmsys_driver_data,
	},
	{
		.compatible = "mediatek,mt6797-mmsys",
		.data = &mt6797_mmsys_driver_data,
	},
	{
		.compatible = "mediatek,mt8167-mmsys",
		.data = &mt8167_mmsys_driver_data,
	},
	{
		.compatible = "mediatek,mt8173-mmsys",
		.data = &mt8173_mmsys_driver_data,
	},
	{
		.compatible = "mediatek,mt8183-mmsys",
		.data = &mt8183_mmsys_driver_data,
	},
	{
		.compatible = "mediatek,mt8192-mmsys",
		.data = &mt8192_mmsys_driver_data,
	},
	{
		.compatible = "mediatek,mt8365-mmsys",
		.data = &mt8365_mmsys_driver_data,
	},
	{ }
};

static struct platform_driver mtk_mmsys_drv = {
	.driver = {
		.name = "mtk-mmsys",
		.of_match_table = of_match_mtk_mmsys,
	},
	.probe = mtk_mmsys_probe,
};

builtin_platform_driver(mtk_mmsys_drv);
