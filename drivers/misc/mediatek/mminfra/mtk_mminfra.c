// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 * Author: zhengnan chen <zhengnan.chen@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/scmi_protocol.h>
#include <linux/slab.h>
#include "tinysys-scmi.h"

#define MMINFRA_CG_CON0		0x100
#define GCED_CG_BIT		BIT(0)
#define GCEM_CG_BIT		BIT(1)
#define SMI_CG_BIT		BIT(2)

#define MMINFRA_CG_CON1		0x110
#define GCE26M_CG_BIT		BIT(17)

#define MMINFRA_AID_REMAP	0x864

#define MMINFRA_SCMI_MTCMOS_CB	2

static const char * const mtk_mminfra_clks[] = {"gce_d", "gce_m", "gce_26m"};
#define MTK_MMINFRA_CLK_NR_MAX		ARRAY_SIZE(mtk_mminfra_clks)

struct mtk_mminfra_plat_data {
	bool clk_init_on;
	bool bkrs_enable;
};

struct mtk_mminfra_data {
	struct device				*dev;
	struct regmap				*regmap;
	struct clk_bulk_data			clks[MTK_MMINFRA_CLK_NR_MAX];
	struct notifier_block			pd_nb;
	int					feature_id;
	u32					backup_val;
	struct scmi_tinysys_info_st		*tinfo;
	const struct mtk_mminfra_plat_data	*plat_data;
};

static struct mtk_mminfra_plat_data mtk_mminfra_mt8189 = {
	.clk_init_on = true,
	.bkrs_enable = true,
};

static bool mtk_mminfra_check_scmi_status(struct mtk_mminfra_data *data)
{
	if (data->tinfo)
		return true;

	data->tinfo = get_scmi_tinysys_info();

	if (IS_ERR_OR_NULL(data->tinfo)) {
		dev_err(data->dev, "%s: tinfo is wrong!!\n", __func__);
		data->tinfo = NULL;
		return false;
	}

	if (IS_ERR_OR_NULL(data->tinfo->ph)) {
		dev_err(data->dev, "%s: tinfo->ph is wrong!!\n", __func__);
		data->tinfo = NULL;
		return false;
	}

	return true;
}

static void mtk_do_mminfra_bkrs(bool is_restore, struct mtk_mminfra_data *data)
{
	int err;

	err = scmi_tinysys_common_set(data->tinfo->ph, data->feature_id,
				      MMINFRA_SCMI_MTCMOS_CB,
				      (is_restore) ? 0 : 1, 0, 0, 0);
	if (err) {
		dev_err(data->dev, "%s: call scmi(%d) err=%d\n",
			__func__, is_restore, err);
		if (err == -ETIMEDOUT) {
			dev_err(data->dev, "%s: call scmi(%d) timeout\n",
				__func__, is_restore);
			/* sspm is sometimes slow in processing scmi cmd, and
			 * the Linux kernel may continue running before sspm
			 * has completed the mminfra restore(which sspm
			 * doesn't have time to complete, so scmi will
			 * respond with a timeout message).
			 * This will result in a restore failure message.
			 * Therefore, a 3ms wait is required to allow sspm
			 * to complete the mminfra cmd.
			 */
			mdelay(3);
		}
	}
}

static int mtk_mminfra_clk_enable(struct mtk_mminfra_data *data)
{
	int ret;

	ret = clk_bulk_prepare_enable(MTK_MMINFRA_CLK_NR_MAX, data->clks);
	if (ret)
		dev_err(data->dev, "%s fail, ret = %d\n", __func__, ret);

	return ret;
}

static void mtk_mminfra_clk_disable(struct mtk_mminfra_data *data)
{
	clk_bulk_disable_unprepare(MTK_MMINFRA_CLK_NR_MAX, data->clks);
}

static void mtk_mminfra_cg_check(bool is_on, struct mtk_mminfra_data *data)
{
	u32 con0_val;
	u32 con1_val_gce;

	regmap_read(data->regmap, MMINFRA_CG_CON0, &con0_val);
	regmap_read(data->regmap, MMINFRA_CG_CON1, &con1_val_gce);
	con0_val &= (SMI_CG_BIT | GCEM_CG_BIT | GCED_CG_BIT);
	con1_val_gce &= GCE26M_CG_BIT;

	if (is_on && (con0_val || con1_val_gce)) {
		/* SMI CG still off */
		dev_err(data->dev, "%s cg still off, CG_CON0:0x%x CG_CON1:0x%x\n",
			__func__, con0_val, con1_val_gce);
	} else if (!is_on && (!con0_val || !con1_val_gce)) {
		/* SMI CG still on */
		dev_err(data->dev, "%s cg still on, CG_CON0:0x%x CG_CON1:0x%x\n",
			__func__, con0_val, con1_val_gce);
	}
}

static int mtk_mminfra_pd_callback(struct notifier_block *nb,
				   unsigned long flags, void *data)
{
	struct mtk_mminfra_data *mminfra_data;
	int ret;
	u32 val;

	mminfra_data = container_of(nb, struct mtk_mminfra_data, pd_nb);

	switch (flags) {
	case GENPD_NOTIFY_ON:
		ret = mtk_mminfra_clk_enable(mminfra_data);
		if (ret)
			break;
		mtk_mminfra_cg_check(true, mminfra_data);

		if (mminfra_data->plat_data->bkrs_enable) {
			mtk_do_mminfra_bkrs(true, mminfra_data);
			regmap_read(mminfra_data->regmap, MMINFRA_AID_REMAP, &val);
			if (val != mminfra_data->backup_val) {
				dev_err(mminfra_data->dev,
					"%s: HRE restore fail val:0x%x\n",
					__func__, val);
				WARN_ON_ONCE(1);
			}
		}
		dev_dbg(mminfra_data->dev, "%s: enable clk\n", __func__);
		break;
	case GENPD_NOTIFY_PRE_OFF:
		regmap_read(mminfra_data->regmap, MMINFRA_AID_REMAP, &mminfra_data->backup_val);
		if (mminfra_data->plat_data->bkrs_enable)
			mtk_do_mminfra_bkrs(false, mminfra_data);

		mtk_mminfra_clk_disable(mminfra_data);
		mtk_mminfra_cg_check(false, mminfra_data);
		dev_dbg(mminfra_data->dev, "%s: disable clk\n", __func__);
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static int mtk_mminfra_dts_clk_init(struct device *dev,
				    struct mtk_mminfra_data *data,
				    const char * const clks[],
				    unsigned int clk_nr_required)
{
	int i, ret;

	for (i = 0; i < clk_nr_required; i++)
		data->clks[i].id = clks[i];
	ret = devm_clk_bulk_get(dev, clk_nr_required, data->clks);

	return ret;
}

static int mtk_mminfra_probe(struct platform_device *pdev)
{
	struct mtk_mminfra_data *data;
	struct device *dev = &pdev->dev;
	int ret;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;
	data->dev = dev;
	data->plat_data = of_device_get_match_data(dev);

	if (!mtk_mminfra_check_scmi_status(data))
		return -EPROBE_DEFER;

	ret = of_property_read_u32(data->tinfo->sdev->dev.of_node,
				   "scmi-mminfra",
				   &data->feature_id);
	if (ret)
		return ret;

	data->regmap = syscon_regmap_lookup_by_phandle(dev->of_node, "mediatek,mminfra");
	if (IS_ERR(data->regmap))
		return PTR_ERR(data->regmap);

	data->pd_nb.notifier_call = mtk_mminfra_pd_callback;
	ret = dev_pm_genpd_add_notifier(dev, &data->pd_nb);
	if (ret) {
		dev_err(dev, "%s: failed to add genpd notifier\n", __func__);
		return ret;
	}

	ret = mtk_mminfra_dts_clk_init(dev, data, mtk_mminfra_clks,
				       MTK_MMINFRA_CLK_NR_MAX);
	if (ret)
		return ret;

	if (data->plat_data->clk_init_on) {
		ret = mtk_mminfra_clk_enable(data);
		if (ret)
			return ret;
		dev_dbg(dev, "%s: init-clk-on enable clk done\n", __func__);
	}

	pm_runtime_enable(dev);

	return ret;
}

static int mtk_mminfra_remove(struct platform_device *pdev)
{
	dev_pm_genpd_remove_notifier(&pdev->dev);

	return 0;
}

static const struct of_device_id of_mminfra_match_tbl[] = {
	{.compatible = "mediatek,mt8189-mminfra", .data = &mtk_mminfra_mt8189},
	{}
};
MODULE_DEVICE_TABLE(of, of_mminfra_match_tbl);

static struct platform_driver mtk_mminfra_driver = {
	.probe = mtk_mminfra_probe,
	.remove = mtk_mminfra_remove,
	.driver = {
		.name = "mtk-mminfra",
		.of_match_table = of_mminfra_match_tbl,
	},
};

module_platform_driver(mtk_mminfra_driver);

MODULE_DESCRIPTION("MTK MMinfra driver");
MODULE_LICENSE("GPL");
