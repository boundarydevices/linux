// SPDX-License-Identifier: GPL-2.0
/*
 * PCIe controller EP driver for Freescale Layerscape SoCs
 *
 * Copyright (C) 2018 NXP Semiconductor.
 *
 * Author: Xiaowei Bao <xiaowei.bao@nxp.com>
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/of_pci.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/resource.h>

#include "pcie-designware.h"

#define PCIE_DBI2_OFFSET		0x1000	/* DBI2 base address*/
#define PCIE_LINK_CAP			0x7C	/* PCIe Link Capabilities*/
#define MAX_LINK_SP_MASK		0x0F
#define MAX_LINK_W_MASK			0x3F
#define MAX_LINK_W_SHIFT		4

/* PEX PFa PCIE pme and message interrupt registers*/
#define PEX_PF0_PME_MES_DR             0xC0020
#define PEX_PF0_PME_MES_DR_LUD         (1 << 7)
#define PEX_PF0_PME_MES_DR_LDD         (1 << 9)
#define PEX_PF0_PME_MES_DR_HRD         (1 << 10)

#define PEX_PF0_PME_MES_IER            0xC0028
#define PEX_PF0_PME_MES_IER_LUDIE      (1 << 7)
#define PEX_PF0_PME_MES_IER_LDDIE      (1 << 9)
#define PEX_PF0_PME_MES_IER_HRDIE      (1 << 10)

#define to_ls_pcie_ep(x)	dev_get_drvdata((x)->dev)

struct ls_pcie_ep_drvdata {
	u32				func_offset;
	const struct dw_pcie_ep_ops	*ops;
	const struct dw_pcie_ops	*dw_pcie_ops;
};

struct ls_pcie_ep {
	struct dw_pcie			*pci;
	struct pci_epc_features		*ls_epc;
	const struct ls_pcie_ep_drvdata *drvdata;
	u8				max_speed;
	u8				max_width;
	bool				big_endian;
	int				irq;
};

static int ls_pcie_establish_link(struct dw_pcie *pci)
{
	return 0;
}

static const struct dw_pcie_ops dw_ls_pcie_ep_ops = {
	.start_link = ls_pcie_establish_link,
};

static u32 ls_lut_readl(struct ls_pcie_ep *pcie, u32 offset)
{
	struct dw_pcie *pci = pcie->pci;

	if (pcie->big_endian)
		return ioread32be(pci->dbi_base + offset);
	else
		return ioread32(pci->dbi_base + offset);
}

static void ls_lut_writel(struct ls_pcie_ep *pcie, u32 offset,
			  u32 value)
{
	struct dw_pcie *pci = pcie->pci;

	if (pcie->big_endian)
		iowrite32be(value, pci->dbi_base + offset);
	else
		iowrite32(value, pci->dbi_base + offset);
}

static irqreturn_t ls_pcie_ep_event_handler(int irq, void *dev_id)
{
	struct ls_pcie_ep *pcie = (struct ls_pcie_ep *)dev_id;
	struct dw_pcie *pci = pcie->pci;
	u32 val;

	val = ls_lut_readl(pcie, PEX_PF0_PME_MES_DR);
	if (!val)
		return IRQ_NONE;

	if (val & PEX_PF0_PME_MES_DR_LUD)
		dev_info(pci->dev, "Detect the link up state !\n");
	else if (val & PEX_PF0_PME_MES_DR_LDD)
		dev_info(pci->dev, "Detect the link down state !\n");
	else if (val & PEX_PF0_PME_MES_DR_HRD)
		dev_info(pci->dev, "Detect the hot reset state !\n");

	dw_pcie_dbi_ro_wr_en(pci);
	dw_pcie_writew_dbi(pci, PCIE_LINK_CAP,
			   (pcie->max_width << MAX_LINK_W_SHIFT) |
			   pcie->max_speed);
	dw_pcie_dbi_ro_wr_dis(pci);

	ls_lut_writel(pcie, PEX_PF0_PME_MES_DR, val);

	return IRQ_HANDLED;
}

static int ls_pcie_ep_interrupt_init(struct ls_pcie_ep *pcie,
				     struct platform_device *pdev)
{
	u32 val;
	int ret;

	pcie->irq = platform_get_irq_byname(pdev, "pme");
	if (pcie->irq < 0) {
		dev_err(&pdev->dev, "Can't get 'pme' irq.\n");
		return pcie->irq;
	}

	ret = devm_request_irq(&pdev->dev, pcie->irq,
			       ls_pcie_ep_event_handler, IRQF_SHARED,
			       pdev->name, pcie);
	if (ret) {
		dev_err(&pdev->dev, "Can't register PCIe IRQ.\n");
		return ret;
	}

	/* Enable interrupts */
	val = ls_lut_readl(pcie, PEX_PF0_PME_MES_IER);
	val |=  PEX_PF0_PME_MES_IER_LDDIE | PEX_PF0_PME_MES_IER_HRDIE |
		PEX_PF0_PME_MES_IER_LUDIE;
	ls_lut_writel(pcie, PEX_PF0_PME_MES_IER, val);

	return 0;
}

static const struct pci_epc_features*
ls_pcie_ep_get_features(struct dw_pcie_ep *ep)
{
	struct dw_pcie *pci = to_dw_pcie_from_ep(ep);
	struct ls_pcie_ep *pcie = to_ls_pcie_ep(pci);

	return pcie->ls_epc;
}

static void ls_pcie_ep_init(struct dw_pcie_ep *ep)
{
	struct dw_pcie *pci = to_dw_pcie_from_ep(ep);
	struct ls_pcie_ep *pcie = to_ls_pcie_ep(pci);
	struct dw_pcie_ep_func *ep_func;
	enum pci_barno bar;

	ep_func = dw_pcie_ep_get_func_from_ep(ep, 0);
	if (!ep_func)
		return;

	for (bar = 0; bar < PCI_STD_NUM_BARS; bar++)
		dw_pcie_ep_reset_bar(pci, bar);

	pcie->ls_epc->msi_capable = ep_func->msi_cap ? true : false;
	pcie->ls_epc->msix_capable = ep_func->msix_cap ? true : false;
}

static int ls_pcie_ep_raise_irq(struct dw_pcie_ep *ep, u8 func_no,
				enum pci_epc_irq_type type, u16 interrupt_num)
{
	struct dw_pcie *pci = to_dw_pcie_from_ep(ep);

	switch (type) {
	case PCI_EPC_IRQ_LEGACY:
		return dw_pcie_ep_raise_legacy_irq(ep, func_no);
	case PCI_EPC_IRQ_MSI:
		return dw_pcie_ep_raise_msi_irq(ep, func_no, interrupt_num);
	case PCI_EPC_IRQ_MSIX:
		return dw_pcie_ep_raise_msix_irq_doorbell(ep, func_no,
							  interrupt_num);
	default:
		dev_err(pci->dev, "UNKNOWN IRQ type\n");
		return -EINVAL;
	}
}

static unsigned int ls_pcie_ep_func_conf_select(struct dw_pcie_ep *ep,
						u8 func_no)
{
	struct dw_pcie *pci = to_dw_pcie_from_ep(ep);
	struct ls_pcie_ep *pcie = to_ls_pcie_ep(pci);

	WARN_ON(func_no && !pcie->drvdata->func_offset);
	return pcie->drvdata->func_offset * func_no;
}

static const struct dw_pcie_ep_ops ls_pcie_ep_ops = {
	.ep_init = ls_pcie_ep_init,
	.raise_irq = ls_pcie_ep_raise_irq,
	.get_features = ls_pcie_ep_get_features,
	.func_conf_select = ls_pcie_ep_func_conf_select,
};

static const struct ls_pcie_ep_drvdata ls1_ep_drvdata = {
	.ops = &ls_pcie_ep_ops,
	.dw_pcie_ops = &dw_ls_pcie_ep_ops,
};

static const struct ls_pcie_ep_drvdata ls2_ep_drvdata = {
	.func_offset = 0x20000,
	.ops = &ls_pcie_ep_ops,
	.dw_pcie_ops = &dw_ls_pcie_ep_ops,
};

static const struct ls_pcie_ep_drvdata lx2_ep_drvdata = {
	.func_offset = 0x8000,
	.ops = &ls_pcie_ep_ops,
	.dw_pcie_ops = &dw_ls_pcie_ep_ops,
};

static const struct of_device_id ls_pcie_ep_of_match[] = {
	{ .compatible = "fsl,ls1046a-pcie-ep", .data = &ls1_ep_drvdata },
	{ .compatible = "fsl,ls1088a-pcie-ep", .data = &ls2_ep_drvdata },
	{ .compatible = "fsl,ls1028a-pcie-ep", .data = &ls1_ep_drvdata },
	{ .compatible = "fsl,ls2088a-pcie-ep", .data = &ls2_ep_drvdata },
	{ .compatible = "fsl,lx2160ar2-pcie-ep", .data = &lx2_ep_drvdata },
	{ },
};

static int __init ls_add_pcie_ep(struct ls_pcie_ep *pcie,
				 struct platform_device *pdev)
{
	struct dw_pcie *pci = pcie->pci;
	struct device *dev = pci->dev;
	struct dw_pcie_ep *ep;
	struct resource *res;
	int ret;

	ep = &pci->ep;
	ep->ops = pcie->drvdata->ops;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "addr_space");
	if (!res)
		return -EINVAL;

	ep->phys_base = res->start;
	ep->addr_size = resource_size(res);

	ret = ls_pcie_ep_interrupt_init(pcie, pdev);
	if (ret)
		return  ret;

	ret = dw_pcie_ep_init(ep);
	if (ret) {
		dev_err(dev, "failed to initialize endpoint\n");
		return ret;
	}

	return 0;
}

static int __init ls_pcie_ep_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct dw_pcie *pci;
	struct ls_pcie_ep *pcie;
	struct pci_epc_features *ls_epc;
	struct resource *dbi_base;
	int ret;

	pcie = devm_kzalloc(dev, sizeof(*pcie), GFP_KERNEL);
	if (!pcie)
		return -ENOMEM;

	pci = devm_kzalloc(dev, sizeof(*pci), GFP_KERNEL);
	if (!pci)
		return -ENOMEM;

	ls_epc = devm_kzalloc(dev, sizeof(*ls_epc), GFP_KERNEL);
	if (!ls_epc)
		return -ENOMEM;

	pcie->drvdata = of_device_get_match_data(dev);

	pci->dev = dev;
	pci->ops = pcie->drvdata->dw_pcie_ops;

	ls_epc->bar_fixed_64bit = (1 << BAR_2) | (1 << BAR_4),

	pcie->pci = pci;
	pcie->ls_epc = ls_epc;

	dbi_base = platform_get_resource_byname(pdev, IORESOURCE_MEM, "regs");
	pci->dbi_base = devm_pci_remap_cfg_resource(dev, dbi_base);
	if (IS_ERR(pci->dbi_base))
		return PTR_ERR(pci->dbi_base);

	pcie->big_endian = of_property_read_bool(dev->of_node, "big-endian");

	pcie->max_speed = dw_pcie_readw_dbi(pci, PCIE_LINK_CAP) &
			  MAX_LINK_SP_MASK;
	pcie->max_width = (dw_pcie_readw_dbi(pci, PCIE_LINK_CAP) >>
			  MAX_LINK_W_SHIFT) & MAX_LINK_W_MASK;

	pci->dbi_base2 = pci->dbi_base + PCIE_DBI2_OFFSET;

	/* set 64-bit DMA mask and coherent DMA mask */
	if (dma_set_mask_and_coherent(dev, DMA_BIT_MASK(64)))
		dev_warn(dev, "Failed to set 64-bit DMA mask.\n");

	platform_set_drvdata(pdev, pcie);

	ret = ls_add_pcie_ep(pcie, pdev);

	return ret;
}

static struct platform_driver ls_pcie_ep_driver = {
	.driver = {
		.name = "layerscape-pcie-ep",
		.of_match_table = ls_pcie_ep_of_match,
		.suppress_bind_attrs = true,
	},
};
builtin_platform_driver_probe(ls_pcie_ep_driver, ls_pcie_ep_probe);
