// SPDX-License-Identifier: GPL-2.0
/*
 * PCIe host controller driver for Freescale Layerscape SoCs
 *
 * Copyright (C) 2014 Freescale Semiconductor.
 * Copyright 2021 NXP
 *
 * Author: Minghuan Lian <Minghuan.Lian@freescale.com>
 */

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/iopoll.h>
#include <linux/of_pci.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/resource.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>

#include "pcie-designware.h"

/* PEX Internal Configuration Registers */
#define PCIE_STRFMR1		0x71c /* Symbol Timer & Filter Mask Register1 */
#define PCIE_ABSERR		0x8d0 /* Bridge Slave Error Response Register */
#define PCIE_ABSERR_SETTING	0x9401 /* Forward error of non-posted request */

/* PF Message Command Register */
#define LS_PCIE_PF_MCR		0x2c
#define PF_MCR_PTOMR		BIT(0)
#define PF_MCR_EXL2S		BIT(1)

/* LS1021A PEXn PM Write Control Register */
#define SCFG_PEXPMWRCR(idx)	(0x5c + (idx) * 0x64)
#define PMXMTTURNOFF		BIT(31)
#define SCFG_PEXSFTRSTCR	0x190
#define PEXSR(idx)		BIT(idx)

/* LS1043A PEX PME control register */
#define SCFG_PEXPMECR		0x144
#define PEXPME(idx)		BIT(31 - (idx) * 4)

/* LS1043A PEX LUT debug register */
#define LS_PCIE_LDBG	0x7fc
#define LDBG_SR		BIT(30)
#define LDBG_WE		BIT(31)

#define PCIE_IATU_NUM		6

#define LS_PCIE_IS_L2(v)	\
	(((v) & PORT_LOGIC_LTSSM_STATE_MASK) == PORT_LOGIC_LTSSM_STATE_L2)

struct ls_pcie;

struct ls_pcie_host_pm_ops {
	int (*pm_init)(struct ls_pcie *pcie);
	void (*send_turn_off_message)(struct ls_pcie *pcie);
	void (*exit_from_l2)(struct ls_pcie *pcie);
};

struct ls_pcie_drvdata {
	const u32 pf_off;
	const u32 lut_off;
	const struct ls_pcie_host_pm_ops *pm_ops;
};

struct ls_pcie {
	struct dw_pcie *pci;
	const struct ls_pcie_drvdata *drvdata;
	void __iomem *pf_base;
	void __iomem *lut_base;
	bool big_endian;
	bool ep_presence;
	bool pm_support;
	struct regmap *scfg;
	int index;
};

#define ls_pcie_lut_readl_addr(addr)	ls_pcie_lut_readl(pcie, addr)
#define ls_pcie_pf_readl_addr(addr)	ls_pcie_pf_readl(pcie, addr)
#define to_ls_pcie(x)	dev_get_drvdata((x)->dev)

static bool ls_pcie_is_bridge(struct ls_pcie *pcie)
{
	struct dw_pcie *pci = pcie->pci;
	u32 header_type;

	header_type = ioread8(pci->dbi_base + PCI_HEADER_TYPE);
	header_type &= 0x7f;

	return header_type == PCI_HEADER_TYPE_BRIDGE;
}

/* Clear multi-function bit */
static void ls_pcie_clear_multifunction(struct ls_pcie *pcie)
{
	struct dw_pcie *pci = pcie->pci;

	iowrite8(PCI_HEADER_TYPE_BRIDGE, pci->dbi_base + PCI_HEADER_TYPE);
}

/* Drop MSG TLP except for Vendor MSG */
static void ls_pcie_drop_msg_tlp(struct ls_pcie *pcie)
{
	u32 val;
	struct dw_pcie *pci = pcie->pci;

	val = ioread32(pci->dbi_base + PCIE_STRFMR1);
	val &= 0xDFFFFFFF;
	iowrite32(val, pci->dbi_base + PCIE_STRFMR1);
}

/* Forward error response of outbound non-posted requests */
static void ls_pcie_fix_error_response(struct ls_pcie *pcie)
{
	struct dw_pcie *pci = pcie->pci;

	iowrite32(PCIE_ABSERR_SETTING, pci->dbi_base + PCIE_ABSERR);
}

static u32 ls_pcie_lut_readl(struct ls_pcie *pcie, u32 off)
{
	if (pcie->big_endian)
		return ioread32be(pcie->lut_base + off);

	return ioread32(pcie->lut_base + off);
}

static void ls_pcie_lut_writel(struct ls_pcie *pcie, u32 off, u32 val)
{
	if (pcie->big_endian)
		return iowrite32be(val, pcie->lut_base + off);

	return iowrite32(val, pcie->lut_base + off);

}

static u32 ls_pcie_pf_readl(struct ls_pcie *pcie, u32 off)
{
	if (pcie->big_endian)
		return ioread32be(pcie->pf_base + off);

	return ioread32(pcie->pf_base + off);
}

static void ls_pcie_pf_writel(struct ls_pcie *pcie, u32 off, u32 val)
{
	if (pcie->big_endian)
		return iowrite32be(val, pcie->pf_base + off);

	return iowrite32(val, pcie->pf_base + off);

}

static void ls_pcie_send_turnoff_msg(struct ls_pcie *pcie)
{
	u32 val;
	int ret;

	val = ls_pcie_pf_readl(pcie, LS_PCIE_PF_MCR);
	val |= PF_MCR_PTOMR;
	ls_pcie_pf_writel(pcie, LS_PCIE_PF_MCR, val);

	ret = readx_poll_timeout(ls_pcie_pf_readl_addr, LS_PCIE_PF_MCR,
				 val, !(val & PF_MCR_PTOMR), 100, 10000);
	if (ret)
		dev_info(pcie->pci->dev, "poll turn off message timeout\n");
}

static void ls1021a_pcie_send_turnoff_msg(struct ls_pcie *pcie)
{
	u32 val;

	if (!pcie->scfg) {
		dev_dbg(pcie->pci->dev, "SYSCFG is NULL\n");
		return;
	}

	/* Send Turn_off message */
	regmap_read(pcie->scfg, SCFG_PEXPMWRCR(pcie->index), &val);
	val |= PMXMTTURNOFF;
	regmap_write(pcie->scfg, SCFG_PEXPMWRCR(pcie->index), val);

	mdelay(10);

	/* Clear Turn_off message */
	regmap_read(pcie->scfg, SCFG_PEXPMWRCR(pcie->index), &val);
	val &= ~PMXMTTURNOFF;
	regmap_write(pcie->scfg, SCFG_PEXPMWRCR(pcie->index), val);
}

static void ls1043a_pcie_send_turnoff_msg(struct ls_pcie *pcie)
{
	u32 val;

	if (!pcie->scfg) {
		dev_dbg(pcie->pci->dev, "SYSCFG is NULL\n");
		return;
	}

	/* Send Turn_off message */
	regmap_read(pcie->scfg, SCFG_PEXPMECR, &val);
	val |= PEXPME(pcie->index);
	regmap_write(pcie->scfg, SCFG_PEXPMECR, val);

	mdelay(10);

	/* Clear Turn_off message */
	regmap_read(pcie->scfg, SCFG_PEXPMECR, &val);
	val &= ~PEXPME(pcie->index);
	regmap_write(pcie->scfg, SCFG_PEXPMECR, val);
}

static void ls_pcie_exit_from_l2(struct ls_pcie *pcie)
{
	u32 val;
	int ret;

	val = ls_pcie_pf_readl(pcie, LS_PCIE_PF_MCR);
	val |= PF_MCR_EXL2S;
	ls_pcie_pf_writel(pcie, LS_PCIE_PF_MCR, val);

	ret = readx_poll_timeout(ls_pcie_pf_readl_addr, LS_PCIE_PF_MCR,
				 val, !(val & PF_MCR_EXL2S), 100, 10000);
	if (ret)
		dev_info(pcie->pci->dev, "poll exit L2 state timeout\n");
}

static void ls_pcie_retrain_link(struct ls_pcie *pcie)
{
	struct dw_pcie *pci = pcie->pci;
	u8 offset = dw_pcie_find_capability(pci, PCI_CAP_ID_EXP);
	u32 val;

	val = dw_pcie_readw_dbi(pci, offset + PCI_EXP_LNKCTL);
	val |= PCI_EXP_LNKCTL_RL;
	dw_pcie_writew_dbi(pci, offset + PCI_EXP_LNKCTL, val);
}

static void ls1021a_pcie_exit_from_l2(struct ls_pcie *pcie)
{
	u32 val;

	regmap_read(pcie->scfg, SCFG_PEXSFTRSTCR, &val);
	val |= PEXSR(pcie->index);
	regmap_write(pcie->scfg, SCFG_PEXSFTRSTCR, val);

	regmap_read(pcie->scfg, SCFG_PEXSFTRSTCR, &val);
	val &= ~PEXSR(pcie->index);
	regmap_write(pcie->scfg, SCFG_PEXSFTRSTCR, val);

	mdelay(1);

	ls_pcie_retrain_link(pcie);
}
static void ls1043a_pcie_exit_from_l2(struct ls_pcie *pcie)
{
	u32 val;

	val = ls_pcie_lut_readl(pcie, LS_PCIE_LDBG);
	val |= LDBG_WE;
	ls_pcie_lut_writel(pcie, LS_PCIE_LDBG, val);

	val = ls_pcie_lut_readl(pcie, LS_PCIE_LDBG);
	val |= LDBG_SR;
	ls_pcie_lut_writel(pcie, LS_PCIE_LDBG, val);

	val = ls_pcie_lut_readl(pcie, LS_PCIE_LDBG);
	val &= ~LDBG_SR;
	ls_pcie_lut_writel(pcie, LS_PCIE_LDBG, val);

	val = ls_pcie_lut_readl(pcie, LS_PCIE_LDBG);
	val &= ~LDBG_WE;
	ls_pcie_lut_writel(pcie, LS_PCIE_LDBG, val);

	mdelay(1);

	ls_pcie_retrain_link(pcie);
}

static int ls1021a_pcie_pm_init(struct ls_pcie *pcie)
{
	struct device *dev = pcie->pci->dev;
	u32 index[2];
	int ret;

	pcie->scfg = syscon_regmap_lookup_by_phandle(dev->of_node,
						     "fsl,pcie-scfg");
	if (IS_ERR(pcie->scfg)) {
		ret = PTR_ERR(pcie->scfg);
		dev_err(dev, "No syscfg phandle specified\n");
		pcie->scfg = NULL;
		return ret;
	}

	ret = of_property_read_u32_array(dev->of_node, "fsl,pcie-scfg",
					 index, 2);
	if (ret) {
		pcie->scfg = NULL;
		return ret;
	}

	pcie->index = index[1];

	return 0;
}

static int ls_pcie_pm_init(struct ls_pcie *pcie)
{
	return 0;
}

static void ls_pcie_set_dstate(struct ls_pcie *pcie, u32 dstate)
{
	struct dw_pcie *pci = pcie->pci;
	u8 offset = dw_pcie_find_capability(pci, PCI_CAP_ID_PM);
	u32 val;

	val = dw_pcie_readw_dbi(pci, offset + PCI_PM_CTRL);
	val &= ~PCI_PM_CTRL_STATE_MASK;
	val |= dstate;
	dw_pcie_writew_dbi(pci, offset + PCI_PM_CTRL, val);
}

static int ls_pcie_host_init(struct dw_pcie_rp *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct ls_pcie *pcie = to_ls_pcie(pci);

	ls_pcie_fix_error_response(pcie);

	dw_pcie_dbi_ro_wr_en(pci);
	ls_pcie_clear_multifunction(pcie);
	dw_pcie_dbi_ro_wr_dis(pci);

	ls_pcie_drop_msg_tlp(pcie);

	if (dw_pcie_link_up(pci)) {
		dev_dbg(pci->dev, "Endpoint is present\n");
		pcie->ep_presence = true;
	}

	if (pcie->drvdata->pm_ops && pcie->drvdata->pm_ops->pm_init &&
	    !pcie->drvdata->pm_ops->pm_init(pcie))
		pcie->pm_support = true;

	return 0;
}

static struct ls_pcie_host_pm_ops ls1021a_pcie_host_pm_ops = {
	.pm_init = &ls1021a_pcie_pm_init,
	.send_turn_off_message = &ls1021a_pcie_send_turnoff_msg,
	.exit_from_l2 = &ls1021a_pcie_exit_from_l2,
};

static struct ls_pcie_host_pm_ops ls1043a_pcie_host_pm_ops = {
	.pm_init = &ls1021a_pcie_pm_init,
	.send_turn_off_message = &ls1043a_pcie_send_turnoff_msg,
	.exit_from_l2 = &ls1043a_pcie_exit_from_l2,
};

static struct ls_pcie_host_pm_ops ls_pcie_host_pm_ops = {
	.pm_init = &ls_pcie_pm_init,
	.send_turn_off_message = &ls_pcie_send_turnoff_msg,
	.exit_from_l2 = &ls_pcie_exit_from_l2,
};

static const struct dw_pcie_host_ops ls_pcie_host_ops = {
	.host_init = ls_pcie_host_init,
};

static const struct ls_pcie_drvdata ls1021a_drvdata = {
	.pm_ops = &ls1021a_pcie_host_pm_ops,
};

static const struct ls_pcie_drvdata ls1043a_drvdata = {
	.lut_off = 0x10000,
	.pm_ops = &ls1043a_pcie_host_pm_ops,
};

static const struct ls_pcie_drvdata layerscape_drvdata = {
	.lut_off = 0x80000,
	.pf_off = 0xc0000,
	.pm_ops = &ls_pcie_host_pm_ops,
};

static const struct of_device_id ls_pcie_of_match[] = {
	{ .compatible = "fsl,ls1012a-pcie", .data = &layerscape_drvdata },
	{ .compatible = "fsl,ls1021a-pcie", .data = &ls1021a_drvdata },
	{ .compatible = "fsl,ls1028a-pcie", .data = &layerscape_drvdata },
	{ .compatible = "fsl,ls1043a-pcie", .data = &ls1043a_drvdata },
	{ .compatible = "fsl,ls1046a-pcie", .data = &layerscape_drvdata },
	{ .compatible = "fsl,ls2080a-pcie", .data = &layerscape_drvdata },
	{ .compatible = "fsl,ls2085a-pcie", .data = &layerscape_drvdata },
	{ .compatible = "fsl,ls2088a-pcie", .data = &layerscape_drvdata },
	{ .compatible = "fsl,ls1088a-pcie", .data = &layerscape_drvdata },
	{ },
};

static int ls_pcie_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct dw_pcie *pci;
	struct ls_pcie *pcie;
	struct resource *dbi_base;

	pcie = devm_kzalloc(dev, sizeof(*pcie), GFP_KERNEL);
	if (!pcie)
		return -ENOMEM;

	pci = devm_kzalloc(dev, sizeof(*pci), GFP_KERNEL);
	if (!pci)
		return -ENOMEM;

	pcie->drvdata = of_device_get_match_data(dev);

	pci->dev = dev;
	pci->pp.ops = &ls_pcie_host_ops;

	pcie->pci = pci;

	dbi_base = platform_get_resource_byname(pdev, IORESOURCE_MEM, "regs");
	pci->dbi_base = devm_pci_remap_cfg_resource(dev, dbi_base);
	if (IS_ERR(pci->dbi_base))
		return PTR_ERR(pci->dbi_base);

	pcie->big_endian = of_property_read_bool(dev->of_node, "big-endian");

	if (pcie->drvdata->lut_off)
		pcie->lut_base = pci->dbi_base + pcie->drvdata->lut_off;

	if (pcie->drvdata->pf_off)
		pcie->pf_base = pci->dbi_base + pcie->drvdata->pf_off;

	if (!ls_pcie_is_bridge(pcie))
		return -ENODEV;

	platform_set_drvdata(pdev, pcie);

	return dw_pcie_host_init(&pci->pp);
}

static bool ls_pcie_pm_check(struct ls_pcie *pcie)
{
	if (!pcie->ep_presence) {
		dev_dbg(pcie->pci->dev, "Endpoint isn't present\n");
		return false;
	}

	if (!pcie->pm_support)
		return false;

	return true;
}

#ifdef CONFIG_PM_SLEEP
static int ls_pcie_suspend_noirq(struct device *dev)
{
	struct ls_pcie *pcie = dev_get_drvdata(dev);
	struct dw_pcie *pci = pcie->pci;
	u32 val;
	int ret;

	if (!ls_pcie_pm_check(pcie))
		return 0;

	pcie->drvdata->pm_ops->send_turn_off_message(pcie);

	/* 10ms timeout to check L2 ready */
	ret = readl_poll_timeout(pci->dbi_base + PCIE_PORT_DEBUG0,
				 val, LS_PCIE_IS_L2(val), 100, 10000);
	if (ret) {
		dev_err(dev, "PCIe link enter L2 timeout! ltssm = 0x%x\n", val);
		return ret;
	}

	ls_pcie_set_dstate(pcie, 0x3);

	return 0;
}

static int ls_pcie_resume_noirq(struct device *dev)
{
	struct ls_pcie *pcie = dev_get_drvdata(dev);
	struct dw_pcie *pci = pcie->pci;
	int ret;

	if (!ls_pcie_pm_check(pcie))
		return 0;

	ls_pcie_set_dstate(pcie, 0x0);

	pcie->drvdata->pm_ops->exit_from_l2(pcie);

	/* delay 10 ms to access EP */
	mdelay(10);

	ret = ls_pcie_host_init(&pci->pp);
	if (ret) {
		dev_err(dev, "ls_pcie_host_init failed! ret = 0x%x\n", ret);
		return ret;
	}

	dw_pcie_setup_rc(&pci->pp);

	ret = dw_pcie_wait_for_link(pci);
	if (ret) {
		dev_err(dev, "wait link up timeout! ret = 0x%x\n", ret);
		return ret;
	}

	return 0;
}
#endif /* CONFIG_PM_SLEEP */

static const struct dev_pm_ops ls_pcie_pm_ops = {
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(ls_pcie_suspend_noirq,
				      ls_pcie_resume_noirq)
};

static struct platform_driver ls_pcie_driver = {
	.probe = ls_pcie_probe,
	.driver = {
		.name = "layerscape-pcie",
		.of_match_table = ls_pcie_of_match,
		.suppress_bind_attrs = true,
		.pm = &ls_pcie_pm_ops,
	},
};
builtin_platform_driver(ls_pcie_driver);
