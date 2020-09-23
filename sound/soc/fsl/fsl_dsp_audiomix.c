// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2019 NXP.
 */

#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>

#include "fsl_dsp_audiomix.h"

struct imx_audiomix_dsp_data {
	struct regmap *regmap;
};

void imx_audiomix_dsp_start(struct imx_audiomix_dsp_data *data)
{
	regmap_update_bits(data->regmap, AudioDSP_REG2, 1 << 5,  0);
}
EXPORT_SYMBOL(imx_audiomix_dsp_start);

void imx_audiomix_dsp_pid_set(struct imx_audiomix_dsp_data *data, u32 val)
{
	regmap_write(data->regmap, AudioDSP_REG3, val);
}
EXPORT_SYMBOL(imx_audiomix_dsp_pid_set);

bool imx_audiomix_dsp_reset(struct imx_audiomix_dsp_data *data)
{
	u32 val = 0;

	regmap_read(data->regmap, AudioDSP_REG3, &val);
	if (val == 0)
		return true;
	else
		return false;
}
EXPORT_SYMBOL(imx_audiomix_dsp_reset);

static int imx_audiomix_dsp_probe(struct platform_device *pdev)
{
	struct imx_audiomix_dsp_data *drvdata;

	drvdata = devm_kzalloc(&pdev->dev, sizeof(*drvdata), GFP_KERNEL);
	if (drvdata == NULL)
		return -ENOMEM;

	drvdata->regmap = syscon_regmap_lookup_by_compatible("fsl,imx8mp-audio-blk-ctrl");
	if (IS_ERR(drvdata->regmap))
		dev_warn(&pdev->dev, "cannot find iomuxc registers\n");

	platform_set_drvdata(pdev, drvdata);

	return 0;
}

static const struct of_device_id imx_audiomix_dsp_dt_ids[] = {
       { .compatible = "fsl,audiomix-dsp", },
       { /* sentinel */ },
};

static struct platform_driver imx_audiomix_dsp_driver = {
	.probe  = imx_audiomix_dsp_probe,
	.driver = {
		.name	= "audiomix-dsp",
		.of_match_table = imx_audiomix_dsp_dt_ids,
	},
};
module_platform_driver(imx_audiomix_dsp_driver);
