// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright 2024 NXP
 *  Author: Robert Chiras <robert.chiras@nxp.com>
 *
 * Remoteproc driver for the ISP FW running on M0+ core.
 *
 */

#include <linux/clk.h>
#include <linux/firmware.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/remoteproc.h>


#include "imx_rproc.h"

#define IMX95_CAM_BLKCTL_REG_CM0P_ADDR_OFFSET1	0x300
#define IMX95_CAM_BLKCTL_REG_CM0P_ADDR_OFFSET2	0x304
#define IMX95_CAM_BLKCTL_REG_CM0P_CPUWAIT	0x308
#define IMX95_CAM_BLKCTL_REG_CM0P_CTL		0x30c
#define IMX95_CAM_BLKCTL_REG_CM0P_STAT		0x310
#define IMX95_CAM_BLKCTL_CM0P_CPUWAIT_CPW	BIT(0)
#define IMX95_CAM_BLKCTL_CM0P_CPUWAIT_RST	BIT(1)

#define IMX95_CAMID_CSR_REG_SRAMCTL_RAMCR	0x0
#define IMX95_CAMID_CSR_REG_SRAMCTL_RAMIAS	0x4
#define IMX95_CAMID_CSR_REG_SRAMCTL_RAMIAE	0x8
#define IMX95_CAMID_CSR_REG_SRAMCTL_RAMSR	0xc
#define IMX95_CAMID_CSR_RAMSR_IDONE		BIT(0)

static const struct reg_field ocram_done =
			REG_FIELD(IMX95_CAMID_CSR_REG_SRAMCTL_RAMSR, 0, 0);

#define WAIT_MAX_RETRIES 100

struct imx_isp_rproc {
	struct rproc *rproc;
	struct device *dev;

	struct regmap *blk_ctl;
	struct regmap *ocram_cfg;
	const struct imx_isp_rproc_dcfg *cfg;

	phys_addr_t mba_phys;
	size_t mba_size;
	bool mba_init;
};

/* Custom registers to initialize */
struct imx_isp_dcfg {
	u32				src_reg;
	u32				src_mask;
	u32				src_val;
};

struct imx_isp_rproc_dcfg {
	const struct imx_rproc_dcfg *dcfg;
	const struct imx_isp_dcfg isp_dcfg[2];
};


static const struct imx_rproc_dcfg isp_rproc_cfg_imx95 = {
	.src_reg	= IMX95_CAM_BLKCTL_REG_CM0P_CPUWAIT,
	.src_mask	= IMX95_CAM_BLKCTL_CM0P_CPUWAIT_CPW |
			  IMX95_CAM_BLKCTL_CM0P_CPUWAIT_RST,
	.src_start	= IMX95_CAM_BLKCTL_CM0P_CPUWAIT_RST,
	.src_stop	= IMX95_CAM_BLKCTL_CM0P_CPUWAIT_CPW,
	.method		= IMX_RPROC_MMIO,
};

static const struct imx_isp_rproc_dcfg imx_isp_rproc_cfg_imx95 = {
	.dcfg		= &isp_rproc_cfg_imx95,
	.isp_dcfg	= {
		{
			.src_reg = IMX95_CAM_BLKCTL_REG_CM0P_ADDR_OFFSET1,
			.src_mask = 0xFFFFFF00,
			.src_val = 0X80000000
		},
		{
			.src_reg = IMX95_CAM_BLKCTL_REG_CM0P_ADDR_OFFSET2,
			.src_mask = 0xFFFFFF00,
			.src_val = 0X20000000
		},
	}
};


static int imx_isp_start(struct rproc *rproc)
{
	struct imx_isp_rproc *isp = (struct imx_isp_rproc *)rproc->priv;
	const struct imx_isp_rproc_dcfg *cfg = isp->cfg;
	const struct imx_rproc_dcfg *dcfg = cfg->dcfg;
	int i;
	int ret = 0;

	for (i = 0; i < ARRAY_SIZE(cfg->isp_dcfg); i++) {
		const struct imx_isp_dcfg *isp_cfg = &cfg->isp_dcfg[i];

		if (!isp_cfg->src_reg)
			continue;

		ret |= regmap_update_bits(isp->blk_ctl,
					  isp_cfg->src_reg,
					  isp_cfg->src_mask,
					  isp_cfg->src_val);
	}

	/* Start the processor */
	ret |= regmap_update_bits(isp->blk_ctl,
				  dcfg->src_reg,
				  dcfg->src_mask,
				  IMX95_CAM_BLKCTL_CM0P_CPUWAIT_CPW |
				  IMX95_CAM_BLKCTL_CM0P_CPUWAIT_RST);
	ret |= regmap_update_bits(isp->blk_ctl,
				  dcfg->src_reg,
				  dcfg->src_mask,
				  dcfg->src_start);
	if (ret)
		dev_err(isp->dev, "Failed to enable remote core!\n");

	return ret;
}

static int imx_isp_stop(struct rproc *rproc)
{
	struct imx_isp_rproc *isp = (struct imx_isp_rproc *)rproc->priv;
	const struct imx_isp_rproc_dcfg *cfg = isp->cfg;
	const struct imx_rproc_dcfg *dcfg = cfg->dcfg;
	int ret;

	/* Stop the processor */
	ret = regmap_update_bits(isp->blk_ctl,
				 dcfg->src_reg,
				 dcfg->src_mask,
				 dcfg->src_stop);

	isp->mba_init = false;

	if (ret)
		dev_err(isp->dev, "Failed to disable remote core!\n");

	return ret;
}

static int imx_isp_alloc_memory_region(struct imx_isp_rproc *isp)
{
	struct device_node *node;
	struct device_node *child;
	struct resource r;
	unsigned int map[2] = {0};
	int ret = 0;
	int i;
	struct regmap_field *field;
	u32 init_done = 0;

	child = of_get_child_by_name(isp->dev->of_node, "mba");
	if (!child) {
		node = of_parse_phandle(isp->dev->of_node,
					"memory-region", 0);
	} else {
		node = of_parse_phandle(child, "memory-region", 0);
		of_property_read_variable_u32_array(child, "ocram-map",
						  map, 0, ARRAY_SIZE(map));
		of_node_put(child);
	}

	ret = of_address_to_resource(node, 0, &r);
	of_node_put(node);
	if (ret) {
		dev_err(isp->dev, "unable to resolve MBA region\n");
		return ret;
	}

	isp->mba_phys = r.start;
	isp->mba_size = resource_size(&r);

	if (!map[0])
		map[0] = 0;
	if (!map[1])
		map[1] = isp->mba_size;

	/* Stop the processor, if running */
	imx_isp_stop(isp->rproc);

	/* Initialize OCRAM */
	dev_info(isp->dev, "OCRAM init: %pa+%zx", &isp->mba_phys, isp->mba_size);
	field = devm_regmap_field_alloc(isp->dev, isp->ocram_cfg, ocram_done);
	if (IS_ERR(field)) {
		dev_err(isp->dev, "ocram_done field alloc failed");
		return PTR_ERR(field);
	}

	ret = regmap_update_bits(isp->ocram_cfg,
			 IMX95_CAMID_CSR_REG_SRAMCTL_RAMIAS,
			 ~0,
			 map[0]);
	ret |= regmap_update_bits(isp->ocram_cfg,
			 IMX95_CAMID_CSR_REG_SRAMCTL_RAMIAE,
			 ~0,
			 map[1]);
	/* Request initialization */
	ret |= regmap_update_bits(isp->ocram_cfg,
			 IMX95_CAMID_CSR_REG_SRAMCTL_RAMCR,
			 BIT(0),
			 BIT(0));

	if (ret) {
		dev_err(isp->dev, "OCRAM init map failed: %d", ret);
		return ret;
	}

	for (i = 0; i < WAIT_MAX_RETRIES; i++) {
		ret = regmap_field_read(field, &init_done);
		if (ret)
			return ret;
		if (init_done)
			break;
		usleep_range(100, 200);
	}

	if (!init_done) {
		dev_err(isp->dev, "OCRAM initialization failed\n");
		return -EFAULT;
	}

	regmap_update_bits(isp->ocram_cfg,
			   IMX95_CAMID_CSR_REG_SRAMCTL_RAMSR,
			   BIT(0),
			   BIT(0));

	dev_info(isp->dev, "OCRAM mapped at: %u+%x", map[0], map[1]);

	isp->mba_init = true;

	return ret;
}


static int imx_isp_load(struct rproc *rproc, const struct firmware *fw)
{
	struct imx_isp_rproc *isp = rproc->priv;
	void *mba_region;
	int ret = 0;

	if (!isp->mba_init) {
		ret = imx_isp_alloc_memory_region(isp);

		if (ret)
			return ret;
	}

	if (fw->size > isp->mba_size) {
		dev_err(isp->dev, "ISP firmware file too big: %lu > %lu",
			fw->size, isp->mba_size);
		return -EINVAL;
	}

	mba_region = memremap(isp->mba_phys, isp->mba_size, MEMREMAP_WC);
	if (!mba_region) {
		dev_err(isp->dev, "unable to map memory region: %pa+%zx\n",
			&isp->mba_phys, isp->mba_size);
		return -EBUSY;
	}

	memcpy(mba_region, fw->data, fw->size);
	memunmap(mba_region);
	dev_info(isp->dev, "ISP firmware (%lu bytes) loaded to: %pa+%zx",
		 fw->size, &isp->mba_phys, isp->mba_size);

	return 0;
}

static const struct rproc_ops imx_isp_rproc_ops = {
	.start = imx_isp_start,
	.stop = imx_isp_stop,
	.load = imx_isp_load,
};

static int imx_isp_rproc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	const char *fw_name;
	const struct imx_isp_rproc_dcfg *isp_cfg;
	struct imx_isp_rproc *isp_rproc;
	struct rproc *rproc;
	struct clk *clk_m0;
	int ret;

	isp_cfg = of_device_get_match_data(dev);
	if (!isp_cfg)
		return -ENODEV;

	ret = of_property_read_string(np, "nxp,isp-firmware",
				      &fw_name);
	if (ret) {
		dev_err(dev, "No firmware filename given\n");
		return -ENODEV;
	}

	clk_m0 = devm_clk_get(dev, NULL);
	if (IS_ERR(clk_m0))
		return dev_err_probe(dev, PTR_ERR(clk_m0),
				     "failed to get M0 clock\n");

	pm_runtime_enable(dev);
	pm_runtime_resume_and_get(dev);

	ret = clk_prepare_enable(clk_m0);
	if (ret) {
		dev_err(dev, "failed to enable M0 clock: %d\n", ret);
		return ret;
	}

	rproc = rproc_alloc(dev, "imx-isp-rproc", &imx_isp_rproc_ops,
			    fw_name, sizeof(*isp_rproc));
	if (!rproc) {
		ret = -ENOMEM;
		goto err_clk;
	}

	rproc->auto_boot = false;

	isp_rproc = (struct imx_isp_rproc *)rproc->priv;
	isp_rproc->rproc = rproc;
	isp_rproc->cfg = isp_cfg;
	isp_rproc->dev = &pdev->dev;

	isp_rproc->blk_ctl = syscon_regmap_lookup_by_phandle(np, "nxp,blk-ctrl");
	if (IS_ERR(isp_rproc->blk_ctl)) {
		ret = PTR_ERR(isp_rproc->blk_ctl);
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "failed to get blk-ctrl: %d\n",
				      ret);
		goto err_clk;
	}

	isp_rproc->ocram_cfg = syscon_regmap_lookup_by_phandle(np, "nxp,ocram-cfg");
	if (IS_ERR(isp_rproc->ocram_cfg)) {
		ret = PTR_ERR(isp_rproc->ocram_cfg);
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "failed to get camid-csr: %d\n",
				      ret);
		goto err_clk;
	}

	ret = imx_isp_alloc_memory_region(isp_rproc);
	if (ret)
		goto err_clk;

	dev_set_drvdata(dev, rproc);

	ret = rproc_add(rproc);
	if (ret) {
		dev_err(dev, "rproc_add failed\n");
		rproc_free(rproc);
		goto err_clk;
	}

	return 0;

err_clk:
	clk_disable_unprepare(clk_m0);

	return ret;

};

static int imx_isp_rproc_remove(struct platform_device *pdev)
{
	struct rproc *rproc = platform_get_drvdata(pdev);

	rproc_del(rproc);
	rproc_free(rproc);

	pm_runtime_disable(&pdev->dev);

	return 0;
}


static const struct of_device_id imx_isp_rproc_of_match[] = {
	{ .compatible = "nxp,imx95-isp-rproc",
	  .data = &imx_isp_rproc_cfg_imx95,
	},
	{},
};
MODULE_DEVICE_TABLE(of, imx_isp_rproc_of_match);

static struct platform_driver imx_isp_rproc_driver = {
	.probe = imx_isp_rproc_probe,
	.remove = imx_isp_rproc_remove,
	.driver = {
		.name = "imx-isp-rproc",
		.of_match_table = imx_isp_rproc_of_match,
	},
};
module_platform_driver(imx_isp_rproc_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("i.MX ISP Core Remote Processor Control Driver");
MODULE_AUTHOR("Robert Chiras <robert.chiras@nxp.com>");
