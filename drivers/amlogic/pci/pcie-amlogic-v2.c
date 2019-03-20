/*
 * drivers/amlogic/pci/pcie-amlogic-v2.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/of_gpio.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/resource.h>
#include <linux/signal.h>
#include <linux/types.h>
#include <linux/module.h>
#include "../drivers/pci/host/pcie-designware.h"
#include "pcie-amlogic.h"

#include <dt-bindings/clock/amlogic,axg-clkc.h>
#include <linux/clk-provider.h>
#include "../clk/clkc.h"


struct amlogic_pcie {
	struct pcie_port	pp;
	struct pcie_phy		*phy;
	void __iomem		*elbi_base;	/* DT 0th resource */
	void __iomem		*cfg_base;	/* DT 2nd resource */
	int			reset_gpio;
	struct clk		*clk;
	struct clk		*phy_clk;
	struct clk		*bus_clk;
	int			pcie_num;
	int			gpio_type;
	u32			port_num;
	u32			pm_enable;
	u32			device_attch;
	u32			rst_mod;
	u32			pwr_ctl;
};

#define to_amlogic_pcie(x)	container_of(x, struct amlogic_pcie, pp)
struct pcie_phy_aml_regs pcie_aml_regs_v2;
struct pcie_phy		*g_pcie_phy_v2;

static void amlogic_elb_writel(struct amlogic_pcie *amlogic_pcie, u32 val,
								u32 reg)
{
	writel(val, amlogic_pcie->elbi_base + reg);
}

static u32 amlogic_elb_readl(struct amlogic_pcie *amlogic_pcie, u32 reg)
{
	return readl(amlogic_pcie->elbi_base + reg);
}

static void amlogic_cfg_writel(struct amlogic_pcie *amlogic_pcie, u32 val,
								u32 reg)
{
	writel(val, amlogic_pcie->cfg_base + reg);
}

static u32 amlogic_cfg_readl(struct amlogic_pcie *amlogic_pcie, u32 reg)
{
	return readl(amlogic_pcie->cfg_base + reg);
}

static void cr_bus_addr(unsigned int addr)
{
	union phy_r4 phy_r4 = {.d32 = 0};
	union phy_r5 phy_r5 = {.d32 = 0};
	unsigned long timeout_jiffies;

	phy_r4.b.phy_cr_data_in = addr;
	writel(phy_r4.d32, pcie_aml_regs_v2.pcie_phy_r[4]);

	phy_r4.b.phy_cr_cap_addr = 0;
	writel(phy_r4.d32, pcie_aml_regs_v2.pcie_phy_r[4]);
	phy_r4.b.phy_cr_cap_addr = 1;
	writel(phy_r4.d32, pcie_aml_regs_v2.pcie_phy_r[4]);
	timeout_jiffies = jiffies +
			msecs_to_jiffies(1000);
	do {
		phy_r5.d32 = readl(pcie_aml_regs_v2.pcie_phy_r[5]);
	} while (phy_r5.b.phy_cr_ack == 0 &&
		time_is_after_jiffies(timeout_jiffies));

	phy_r4.b.phy_cr_cap_addr = 0;
	writel(phy_r4.d32, pcie_aml_regs_v2.pcie_phy_r[4]);
	timeout_jiffies = jiffies +
			msecs_to_jiffies(1000);
	do {
		phy_r5.d32 = readl(pcie_aml_regs_v2.pcie_phy_r[5]);
	} while (phy_r5.b.phy_cr_ack == 1 &&
		time_is_after_jiffies(timeout_jiffies));
}

static int cr_bus_read(unsigned int addr)
{
	int data;
	union phy_r4 phy_r4 = {.d32 = 0};
	union phy_r5 phy_r5 = {.d32 = 0};
	unsigned long timeout_jiffies;

	cr_bus_addr(addr);

	phy_r4.b.phy_cr_read = 0;
	writel(phy_r4.d32, pcie_aml_regs_v2.pcie_phy_r[4]);
	phy_r4.b.phy_cr_read = 1;
	writel(phy_r4.d32, pcie_aml_regs_v2.pcie_phy_r[4]);

	timeout_jiffies = jiffies +
			msecs_to_jiffies(1000);
	do {
		phy_r5.d32 = readl(pcie_aml_regs_v2.pcie_phy_r[5]);
	} while (phy_r5.b.phy_cr_ack == 0 &&
		time_is_after_jiffies(timeout_jiffies));

	data = phy_r5.b.phy_cr_data_out;

	phy_r4.b.phy_cr_read = 0;
	writel(phy_r4.d32, pcie_aml_regs_v2.pcie_phy_r[4]);
	timeout_jiffies = jiffies +
			msecs_to_jiffies(1000);
	do {
		phy_r5.d32 = readl(pcie_aml_regs_v2.pcie_phy_r[5]);
	} while (phy_r5.b.phy_cr_ack == 1 &&
		time_is_after_jiffies(timeout_jiffies));

	return data;
}

static void cr_bus_write(unsigned int addr, unsigned int data)
{
	union phy_r4 phy_r4 = {.d32 = 0};
	union phy_r5 phy_r5 = {.d32 = 0};
	unsigned long timeout_jiffies;

	cr_bus_addr(addr);

	phy_r4.b.phy_cr_data_in = data;
	writel(phy_r4.d32, pcie_aml_regs_v2.pcie_phy_r[4]);

	phy_r4.b.phy_cr_cap_data = 0;
	writel(phy_r4.d32, pcie_aml_regs_v2.pcie_phy_r[4]);
	phy_r4.b.phy_cr_cap_data = 1;
	writel(phy_r4.d32, pcie_aml_regs_v2.pcie_phy_r[4]);
	timeout_jiffies = jiffies +
		msecs_to_jiffies(1000);
	do {
		phy_r5.d32 = readl(pcie_aml_regs_v2.pcie_phy_r[5]);
	} while (phy_r5.b.phy_cr_ack == 0 &&
		time_is_after_jiffies(timeout_jiffies));

	phy_r4.b.phy_cr_cap_data = 0;
	writel(phy_r4.d32, pcie_aml_regs_v2.pcie_phy_r[4]);
	timeout_jiffies = jiffies +
		msecs_to_jiffies(1000);
	do {
		phy_r5.d32 = readl(pcie_aml_regs_v2.pcie_phy_r[5]);
	} while (phy_r5.b.phy_cr_ack == 1 &&
		time_is_after_jiffies(timeout_jiffies));

	phy_r4.b.phy_cr_write = 0;
	writel(phy_r4.d32, pcie_aml_regs_v2.pcie_phy_r[4]);
	phy_r4.b.phy_cr_write = 1;
	writel(phy_r4.d32, pcie_aml_regs_v2.pcie_phy_r[4]);
	timeout_jiffies = jiffies +
		msecs_to_jiffies(1000);
	do {
		phy_r5.d32 = readl(pcie_aml_regs_v2.pcie_phy_r[5]);
	} while (phy_r5.b.phy_cr_ack == 0 &&
		time_is_after_jiffies(timeout_jiffies));

	phy_r4.b.phy_cr_write = 0;
	writel(phy_r4.d32, pcie_aml_regs_v2.pcie_phy_r[4]);
	timeout_jiffies = jiffies +
		msecs_to_jiffies(1000);
	do {
		phy_r5.d32 = readl(pcie_aml_regs_v2.pcie_phy_r[5]);
	} while (phy_r5.b.phy_cr_ack == 1 &&
		time_is_after_jiffies(timeout_jiffies));
}

static void amlogic_phy_cr_writel(u32 val, u32 reg)
{
	cr_bus_write(reg, val);
}

static u32 amlogic_phy_cr_readl(u32 reg)
{
	return cr_bus_read(reg);
}

static ssize_t show_pcie_cr_read(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	u32 status = 0;

	return sprintf(buf, "%d\n", status);
}


static ssize_t store_pcie_cr_read(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	u32 reg;
	u32 val;

	if (kstrtouint(buf, 0, &reg) != 0)
		return -EINVAL;

	val = amlogic_phy_cr_readl(reg);
	dev_info(dev, "reg 0x%x value is 0x%x\n", reg, val);

	return count;
}
DEVICE_ATTR(phyread_v2, 0644, show_pcie_cr_read, store_pcie_cr_read);

static ssize_t show_pcie_cr_write(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	u32 status = 0;

	return sprintf(buf, "%d\n", status);
}

static ssize_t store_pcie_cr_write(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned int reg, val;
	int ret = 0;
	char *pstr;
	char *pend;
	char *strbuf;

	if (count > 40)
		return -EINVAL;
	strbuf = kzalloc(count + 2, GFP_KERNEL);
	if (!strbuf)
		return -ENOMEM;

	strncpy(strbuf, buf, count);
	pend = strbuf + count;
	pstr = strchr(strbuf, ' ');
	if (!pstr) {
		ret = -EINVAL;
		goto cr_end;
	}
	*pstr = 0;
	ret = kstrtouint(strbuf, 0, &reg);
	pstr++;
	if (pstr >= pend) {
		ret = -EINVAL;
		goto cr_end;
	}
	ret = kstrtouint(pstr, 0, &val);
	if (ret) {
		ret = -EINVAL;
		goto cr_end;
	}
	amlogic_phy_cr_writel(val, reg);
	ret = count;
cr_end:
	kfree(strbuf);
	return ret;
}
DEVICE_ATTR(phywrite_v2, 0644, show_pcie_cr_write, store_pcie_cr_write);

static void amlogic_pcie_assert_reset(struct amlogic_pcie *amlogic_pcie)
{
	struct pcie_port *pp = &amlogic_pcie->pp;
	struct device *dev = pp->dev;

	if (amlogic_pcie->gpio_type == 0) {
		dev_info(amlogic_pcie->pp.dev,
				"gpio multiplex, don't reset!\n");
	} else if (amlogic_pcie->gpio_type == 1) {
		dev_info(amlogic_pcie->pp.dev, "pad gpio\n");
		if (amlogic_pcie->reset_gpio >= 0)
			devm_gpio_request(dev,
				amlogic_pcie->reset_gpio, "RESET");

		if (gpio_is_valid(amlogic_pcie->reset_gpio)) {
			dev_info(amlogic_pcie->pp.dev,
				"GPIO pad: amlogic_pcie_assert_reset\n");
			gpio_direction_output(
				amlogic_pcie->reset_gpio, 0);
			usleep_range(5000, 6000);
			gpio_direction_input(
				amlogic_pcie->reset_gpio);
		}
	} else {
		dev_info(amlogic_pcie->pp.dev, "normal gpio\n");
		if (amlogic_pcie->reset_gpio >= 0)
			devm_gpio_request(dev,
				amlogic_pcie->reset_gpio, "RESET");
		gpio_direction_output(
			amlogic_pcie->reset_gpio, 0);
		if (gpio_is_valid(amlogic_pcie->reset_gpio)) {
			dev_info(amlogic_pcie->pp.dev,
				"GPIO normal: amlogic_pcie_assert_reset\n");
			gpio_set_value_cansleep(
				amlogic_pcie->reset_gpio, 0);
			usleep_range(5000, 6000);
			gpio_set_value_cansleep(
				amlogic_pcie->reset_gpio, 1);
		}
	}

}

static void amlogic_set_max_payload(struct amlogic_pcie *amlogic_pcie, int size)
{
	int max_payload_size = 1;
	u32 val = 0;

	switch (size) {
	case 128:
		max_payload_size = 0;
		break;
	case 256:
		max_payload_size = 1;
		break;
	case 512:
		max_payload_size = 2;
		break;
	case 1024:
		max_payload_size = 3;
		break;
	case 2048:
		max_payload_size = 4;
		break;
	case 4096:
		max_payload_size = 5;
		break;
	default:
		max_payload_size = 1;
		break;
	}

	val = amlogic_elb_readl(amlogic_pcie, PCIE_DEV_CTRL_DEV_STUS);
	val &= (~(0x7<<5));
	amlogic_elb_writel(amlogic_pcie, val, PCIE_DEV_CTRL_DEV_STUS);

	val = amlogic_elb_readl(amlogic_pcie, PCIE_DEV_CTRL_DEV_STUS);
	val |= (max_payload_size<<5);
	amlogic_elb_writel(amlogic_pcie, val, PCIE_DEV_CTRL_DEV_STUS);
}

static void amlogic_set_max_rd_req_size
				(struct amlogic_pcie *amlogic_pcie, int size)
{
	int max_rd_req_size = 1;
	u32 val = 0;

	switch (size) {
	case 128:
		max_rd_req_size = 0;
		break;
	case 256:
		max_rd_req_size = 1;
		break;
	case 512:
		max_rd_req_size = 2;
		break;
	case 1024:
		max_rd_req_size = 3;
		break;
	case 2048:
		max_rd_req_size = 4;
		break;
	case 4096:
		max_rd_req_size = 5;
		break;
	default:
		max_rd_req_size = 1;
		break;
	}

	val = amlogic_elb_readl(amlogic_pcie, PCIE_DEV_CTRL_DEV_STUS);
	val &= (~(0x7<<12));
	amlogic_elb_writel(amlogic_pcie, val, PCIE_DEV_CTRL_DEV_STUS);

	val = amlogic_elb_readl(amlogic_pcie, PCIE_DEV_CTRL_DEV_STUS);
	val |= (max_rd_req_size<<12);
	amlogic_elb_writel(amlogic_pcie, val, PCIE_DEV_CTRL_DEV_STUS);
}

static void amlogic_pcie_init_dw(struct amlogic_pcie *amlogic_pcie)
{
	u32 val = 0;

	val = amlogic_cfg_readl(amlogic_pcie, PCIE_CFG0);
	val &= (~APP_LTSSM_ENABLE);
	amlogic_cfg_writel(amlogic_pcie, val, PCIE_CFG0);

	val = amlogic_elb_readl(amlogic_pcie, PCIE_PORT_LINK_CTRL_OFF);
	val |= (1<<7);
	amlogic_elb_writel(amlogic_pcie, val, PCIE_PORT_LINK_CTRL_OFF);

	val = amlogic_elb_readl(amlogic_pcie, PCIE_PORT_LINK_CTRL_OFF);
	val &= (~(0x3f<<16));
	amlogic_elb_writel(amlogic_pcie, val, PCIE_PORT_LINK_CTRL_OFF);

	val = amlogic_elb_readl(amlogic_pcie, PCIE_PORT_LINK_CTRL_OFF);
	val |= (0x1<<16);
	amlogic_elb_writel(amlogic_pcie, val, PCIE_PORT_LINK_CTRL_OFF);

	val = amlogic_elb_readl(amlogic_pcie, PCIE_GEN2_CTRL_OFF);
	val &= (~(0x1f<<8));
	amlogic_elb_writel(amlogic_pcie, val, PCIE_GEN2_CTRL_OFF);

	val = amlogic_elb_readl(amlogic_pcie, PCIE_GEN2_CTRL_OFF);
	val |= (0x1<<8);
	amlogic_elb_writel(amlogic_pcie, val, PCIE_GEN2_CTRL_OFF);

	val = amlogic_elb_readl(amlogic_pcie, PCIE_GEN2_CTRL_OFF);
	val |= (1<<17);
	amlogic_elb_writel(amlogic_pcie, val, PCIE_GEN2_CTRL_OFF);

	amlogic_elb_writel(amlogic_pcie, 0x0, PCIE_BASE_ADDR0);
	amlogic_elb_writel(amlogic_pcie, 0x0, PCIE_BASE_ADDR1);
}

static void amlogic_enable_memory_space(struct amlogic_pcie *amlogic_pcie)
{
	u32 val = 0;

	dev_info(amlogic_pcie->pp.dev, "Set the RC Bus Master, Memory Space and I/O Space enables.\n");
	val = amlogic_elb_readl(amlogic_pcie, 0x04);
	val = 7;
	amlogic_elb_writel(amlogic_pcie, val, 0x04);
}


static int amlogic_pcie_establish_link(struct amlogic_pcie *amlogic_pcie)
{
	struct pcie_port *pp = &amlogic_pcie->pp;
	u32 val;

	amlogic_pcie_init_dw(amlogic_pcie);
	amlogic_set_max_payload(amlogic_pcie, 256);
	amlogic_set_max_rd_req_size(amlogic_pcie, 256);

	dw_pcie_setup_rc(pp);
	amlogic_enable_memory_space(amlogic_pcie);

	amlogic_pcie_assert_reset(amlogic_pcie);

	val = amlogic_cfg_readl(amlogic_pcie, PCIE_CFG0);
	val |= APP_LTSSM_ENABLE;
	amlogic_cfg_writel(amlogic_pcie, val, PCIE_CFG0);

	/* check if the link is up or not */
	if (!dw_pcie_wait_for_link(pp))
		return 0;

	return -ETIMEDOUT;
}

static irqreturn_t amlogic_pcie_msi_irq_handler(int irq, void *arg)
{
	struct amlogic_pcie *amlogic_pcie = arg;
	struct pcie_port *pp = &amlogic_pcie->pp;

	return dw_handle_msi_irq(pp);
}

static void amlogic_pcie_msi_init(struct amlogic_pcie *amlogic_pcie)
{
	struct pcie_port *pp = &amlogic_pcie->pp;

	dw_pcie_msi_init(pp);
}

static void amlogic_pcie_enable_interrupts(struct amlogic_pcie *amlogic_pcie)
{
	if (IS_ENABLED(CONFIG_PCI_MSI))
		amlogic_pcie_msi_init(amlogic_pcie);
}

static u32 amlogic_pcie_readl_rc(struct pcie_port *pp, u32 reg)
{
	u32 val;

	val = readl(pp->dbi_base + reg);
	return val;
}

static void amlogic_pcie_writel_rc(struct pcie_port *pp, u32 reg, u32 val)
{
	writel(val, pp->dbi_base + reg);
}

static int amlogic_pcie_rd_own_conf(struct pcie_port *pp, int where, int size,
				u32 *val)
{
	int ret;
	struct amlogic_pcie *amlogic_pcie = to_amlogic_pcie(pp);

	if (amlogic_pcie->device_attch == 0)
		return 0;

	/* the device class is not reported correctly from the register */
	if (where == PCI_CLASS_REVISION) {
		dev_info(pp->dev, "the device class is not reported correctly from the register\n");
		*val = readl(pp->dbi_base + PCI_CLASS_REVISION);
		*val &= 0xff;	/* keep revision id */
		*val |= PCI_CLASS_BRIDGE_PCI << 16;
		return PCIBIOS_SUCCESSFUL;
	}

	ret = dw_pcie_cfg_read(pp->dbi_base + where, size, val);
	return ret;
}

static int amlogic_pcie_wr_own_conf(struct pcie_port *pp, int where, int size,
				u32 val)
{
	int ret;
	struct amlogic_pcie *amlogic_pcie = to_amlogic_pcie(pp);

	if (amlogic_pcie->device_attch == 0)
		return 0;

	ret = dw_pcie_cfg_write(pp->dbi_base + where, size, val);
	return ret;
}

static int amlogic_pcie_link_up(struct pcie_port *pp)
{
	u32   smlh_up = 0;
	u32   rdlh_up = 0;
	u32   ltssm_up = 0;
	u32   speed_okay = 0;
	u32   current_data_rate;
	int   cnt = 0;
	u32   val = 0;
	u32   linkup = 0;
	struct amlogic_pcie *amlogic_pcie = to_amlogic_pcie(pp);

	val = readl(pp->dbi_base + PCIE_PHY_DEBUG_R1);
	linkup = ((val & PCIE_PHY_DEBUG_R1_LINK_UP) &&
		(!(val & PCIE_PHY_DEBUG_R1_LINK_IN_TRAINING)));
	if (linkup)
		return linkup;

	while (smlh_up == 0 || rdlh_up == 0
		|| ltssm_up == 0 || speed_okay == 0) {
		udelay(20);
		smlh_up = amlogic_cfg_readl(amlogic_pcie, PCIE_CFG_STATUS12);
		smlh_up = (smlh_up >> 6) & 0x1;

		rdlh_up = amlogic_cfg_readl(amlogic_pcie, PCIE_CFG_STATUS12);
		rdlh_up = (rdlh_up >> 16) & 0x1;
		ltssm_up = amlogic_cfg_readl(amlogic_pcie, PCIE_CFG_STATUS12);
		ltssm_up = ((ltssm_up >> 10) & 0x1f) == 0x11 ? 1 : 0;
		current_data_rate = amlogic_cfg_readl(
			amlogic_pcie, PCIE_CFG_STATUS17);
		current_data_rate = (current_data_rate >> 7) & 0x1;

		if ((current_data_rate == PCIE_GEN2) ||
			 (current_data_rate == PCIE_GEN1))
			speed_okay = 1;

		if (smlh_up)
			dev_dbg(pp->dev, "smlh_link_up is on\n");
		if (rdlh_up)
			dev_dbg(pp->dev, "rdlh_link_up is on\n");
		if (ltssm_up)
			dev_dbg(pp->dev, "ltssm_up is on\n");
		if (speed_okay)
			dev_dbg(pp->dev, "speed_okay\n");

		cnt++;

		if (cnt >= WAIT_LINKUP_TIMEOUT) {
			dev_err(pp->dev, "Error: Wait linkup timeout.\n");
			return 0;
		}

		udelay(20);
	}

	if (current_data_rate == PCIE_GEN2)
		dev_info(pp->dev, "PCIE SPEED IS GEN2\n");

	return 1;
}

static void amlogic_pcie_host_init(struct pcie_port *pp)
{
	struct amlogic_pcie *amlogic_pcie = to_amlogic_pcie(pp);
	int ret;

	ret = amlogic_pcie_establish_link(amlogic_pcie);
	if (ret)
		if (amlogic_pcie->phy->device_attch == 0)
			return;

	amlogic_pcie->phy->device_attch = 1;
	if (!ret)
		amlogic_pcie->device_attch = 1;

	amlogic_pcie_enable_interrupts(amlogic_pcie);
}

static struct pcie_host_ops amlogic_pcie_host_ops = {
	.readl_rc = amlogic_pcie_readl_rc,
	.writel_rc = amlogic_pcie_writel_rc,
	.rd_own_conf = amlogic_pcie_rd_own_conf,
	.wr_own_conf = amlogic_pcie_wr_own_conf,
	.link_up = amlogic_pcie_link_up,
	.host_init = amlogic_pcie_host_init,
};

static int __init amlogic_add_pcie_port(struct amlogic_pcie *amlogic_pcie,
				       struct platform_device *pdev)
{
	struct pcie_port *pp = &amlogic_pcie->pp;
	struct device *dev = pp->dev;
	int ret;

	if (IS_ENABLED(CONFIG_PCI_MSI)) {
		pp->msi_irq = platform_get_irq(pdev, 0);
		if (!pp->msi_irq) {
			dev_err(dev, "failed to get msi irq\n");
			return -ENODEV;
		}

		ret = devm_request_irq(dev, pp->msi_irq,
					amlogic_pcie_msi_irq_handler,
					IRQF_SHARED | IRQF_NO_THREAD,
					"amlogic-pcie", amlogic_pcie);
		if (ret) {
			dev_err(dev, "failed to request msi irq\n");
			return ret;
		}
	}

	pp->root_bus_nr = -1;
	pp->ops = &amlogic_pcie_host_ops;
	pp->dbi_base = amlogic_pcie->elbi_base;

	ret = dw_pcie_host_init(pp);
	if (ret) {
		dev_err(dev, "failed to initialize host\n");
		return ret;
	}

	if (amlogic_pcie->device_attch == 0) {
		dev_err(pp->dev, "link timeout, disable PCIE PLL\n");
		clk_disable_unprepare(amlogic_pcie->bus_clk);
		clk_disable_unprepare(amlogic_pcie->clk);
		dev_err(pp->dev, "power down pcie phy\n");
		writel(0x1d, pcie_aml_regs_v2.pcie_phy_r[0]);
		amlogic_pcie->phy->power_state = 0;
	}

	return 0;
}

static void power_switch_to_pcie(struct pcie_phy *phy)
{
	u32 val;

	writel(readl(phy->power_base) & (~(0x1<<18)), phy->power_base);

	writel(readl(phy->hhi_mem_pd_base) & (~(0xf<<26)),
			phy->hhi_mem_pd_base);
	udelay(100);

	val = readl((void __iomem *)(unsigned long)phy->reset_base);
	writel((val & (~(0x1<<12))),
		(void __iomem *)(unsigned long)phy->reset_base);
	udelay(100);

	writel(readl(phy->power_base+0x4) & (~(0x1<<18)),
			phy->power_base + 0x4);

	val = readl((void __iomem *)(unsigned long)phy->reset_base);
	writel((val | (0x1<<12)),
			(void __iomem	*)(unsigned long)phy->reset_base);
	udelay(100);
}


static int __init amlogic_pcie_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct amlogic_pcie *amlogic_pcie;
	struct pcie_port *pp;
	struct device_node *np = dev->of_node;
	struct pcie_phy		*phy;
	struct resource *elbi_base;
	struct resource *phy_base;
	struct resource *cfg_base;
	struct resource *reset_base;
	struct resource *power_base = NULL;
	struct resource *hhi_mem_pd_base = NULL;
	int ret;
	int pcie_num = 0;
	int num_lanes = 0;
	int gpio_type = 0;
	unsigned long rate;
#define PCIE_PLL_RATE 100000000
	int j = 0;
	u32 val = 0;
	static u32 port_num;
	u32 pm_enable = 1;
	int pcie_apb_rst_bit = 0;
	int pcie_phy_rst_bit = 0;
	int pcie_ctrl_a_rst_bit = 0;
	u32 pwr_ctl = 0;

	dev_info(&pdev->dev, "amlogic_pcie_probe!\n");

	amlogic_pcie = devm_kzalloc(dev, sizeof(*amlogic_pcie), GFP_KERNEL);
	if (!amlogic_pcie)
		return -ENOMEM;

	amlogic_pcie->device_attch = 0;
	amlogic_pcie->rst_mod = 1;
	pp = &amlogic_pcie->pp;
	pp->dev = dev;
	port_num++;
	amlogic_pcie->port_num = port_num;
	if (amlogic_pcie->port_num == 1) {
		phy = devm_kzalloc(dev, sizeof(*phy), GFP_KERNEL);
		if (!phy) {
			port_num--;
			return -ENOMEM;
		}
		g_pcie_phy_v2 = phy;
	}

	amlogic_pcie->phy = g_pcie_phy_v2;

	ret = of_property_read_u32(np, "pcie-apb-rst-bit", &pcie_apb_rst_bit);
	if (ret)
		amlogic_pcie->rst_mod = 0;

	ret = of_property_read_u32(np, "pcie-phy-rst-bit", &pcie_phy_rst_bit);
	if (ret)
		amlogic_pcie->rst_mod = 0;

	ret = of_property_read_u32(np, "pcie-ctrl-a-rst-bit",
				&pcie_ctrl_a_rst_bit);
	if (ret)
		amlogic_pcie->rst_mod = 0;

	ret = of_property_read_u32(np, "pcie-num", &pcie_num);
	if (ret)
		amlogic_pcie->pcie_num = 0;

	amlogic_pcie->pcie_num = pcie_num;

	ret = of_property_read_u32(np, "pm-enable", &pm_enable);
	if (ret)
		amlogic_pcie->pm_enable = 1;
	else
		amlogic_pcie->pm_enable = pm_enable;

	ret = of_property_read_u32(np, "num-lanes", &num_lanes);
	if (ret)
		pp->lanes = 0;
	pp->lanes = num_lanes;

	ret = of_property_read_u32(np, "pwr-ctl", &pwr_ctl);
	if (ret)
		amlogic_pcie->pwr_ctl = 0;
	else
		amlogic_pcie->pwr_ctl = pwr_ctl;

	if (pwr_ctl) {
		power_base = platform_get_resource_byname(
			pdev, IORESOURCE_MEM, "pwr");
		if (power_base) {
			amlogic_pcie->phy->power_base =
				ioremap(power_base->start,
				resource_size(power_base));
			if (IS_ERR(amlogic_pcie->phy->power_base))
				return PTR_ERR(amlogic_pcie->phy->power_base);
		}

		hhi_mem_pd_base = platform_get_resource_byname(
			pdev, IORESOURCE_MEM, "hii");
		if (hhi_mem_pd_base) {
			amlogic_pcie->phy->hhi_mem_pd_base =
				ioremap(hhi_mem_pd_base->start,
				resource_size(hhi_mem_pd_base));
			if (IS_ERR(amlogic_pcie->phy->hhi_mem_pd_base))
				return PTR_ERR(amlogic_pcie->
						phy->hhi_mem_pd_base);
		}
	}

	if (!amlogic_pcie->phy->reset_base) {
		reset_base = platform_get_resource_byname(
			pdev, IORESOURCE_MEM, "reset");
		amlogic_pcie->phy->reset_base = devm_ioremap_resource(
			dev, reset_base);
		if (IS_ERR(amlogic_pcie->phy->reset_base)) {
			ret = PTR_ERR(amlogic_pcie->phy->reset_base);
			return ret;
		}
	}

	if (pwr_ctl)
		power_switch_to_pcie(amlogic_pcie->phy);

	if (!amlogic_pcie->phy->phy_base) {
		phy_base = platform_get_resource_byname(
			pdev, IORESOURCE_MEM, "phy");
		amlogic_pcie->phy->phy_base =
			 devm_ioremap_resource(dev, phy_base);
		if (IS_ERR(amlogic_pcie->phy->phy_base)) {
			ret = PTR_ERR(amlogic_pcie->phy->phy_base);
			port_num--;
			return ret;
		}
	}

	if (!amlogic_pcie->phy->power_state) {
		for (j = 0; j < 7; j++)
			pcie_aml_regs_v2.pcie_phy_r[j] = (void __iomem *)
				((unsigned long)amlogic_pcie->phy->phy_base
					 + 4*j);
		writel(0x1c, pcie_aml_regs_v2.pcie_phy_r[0]);
		amlogic_pcie->phy->power_state = 1;
	}

	ret = of_property_read_u32(np, "gpio-type", &gpio_type);
	amlogic_pcie->gpio_type = gpio_type;

	amlogic_pcie->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);

	/* RESET0[1,2,6,7] = 0*/
	if (!amlogic_pcie->phy->reset_state) {
		if (amlogic_pcie->rst_mod == 0) {
			val = readl(amlogic_pcie->phy->reset_base);
			val &= ~((0x3<<6) | (0x3<<1));
			writel(val, amlogic_pcie->phy->reset_base);
		} else {
			val = readl(amlogic_pcie->phy->reset_base);
			val &= ~((0x1<<pcie_apb_rst_bit)|(0x1<<pcie_phy_rst_bit)
					| (0x1<<pcie_ctrl_a_rst_bit));
			writel(val, amlogic_pcie->phy->reset_base);
		}
	}
	amlogic_pcie->phy_clk = devm_clk_get(dev, "pcie_phy");
	if (IS_ERR(amlogic_pcie->phy_clk)) {
		dev_err(dev, "Failed to get pcie pcie_phy clock\n");
		ret = PTR_ERR(amlogic_pcie->phy_clk);
		goto fail_pcie;
	}

	ret = clk_prepare_enable(amlogic_pcie->phy_clk);
	if (ret)
		goto fail_pcie_phy;
	amlogic_pcie->bus_clk = devm_clk_get(dev, "pcie_refpll");
	if (IS_ERR(amlogic_pcie->bus_clk)) {
		dev_err(dev, "Failed to get pcie pcie_refpll clock\n");
		ret = PTR_ERR(amlogic_pcie->bus_clk);
		goto fail_pcie;
	}

	ret = clk_prepare_enable(amlogic_pcie->bus_clk);
	if (ret)
		goto fail_pcie;

	if (!amlogic_pcie->phy->reset_state) {
		rate = clk_get_rate(amlogic_pcie->bus_clk);
		if (rate != PCIE_PLL_RATE) {
			ret = -ENODEV;
			goto fail_pcie;
		}
	}

	/*RESET0[6,7] = 1*/
	if (!amlogic_pcie->phy->reset_state) {
		if (amlogic_pcie->rst_mod == 0) {
			val = readl(amlogic_pcie->phy->reset_base);
			val |=	  (0x3<<6);
			writel(val, amlogic_pcie->phy->reset_base);
		} else {
			val = readl(amlogic_pcie->phy->reset_base);
			val |= ((0x1<<pcie_apb_rst_bit)|
					(0x1<<pcie_phy_rst_bit));
			writel(val, amlogic_pcie->phy->reset_base);
		}
		mdelay(1);
	}
	amlogic_pcie->clk = devm_clk_get(dev, "pcie");
	if (IS_ERR(amlogic_pcie->clk)) {
		dev_err(dev, "Failed to get pcie rc clock\n");
		ret = PTR_ERR(amlogic_pcie->clk);
		goto fail_bus_clk;
	}

	ret = clk_prepare_enable(amlogic_pcie->clk);
	if (ret)
		goto fail_bus_clk;

	/*RESET0[1,2] = 1*/
	if (amlogic_pcie->pcie_num == 1) {
		if (amlogic_pcie->rst_mod == 0) {
			val = readl(amlogic_pcie->phy->reset_base);
			val |=	(0x1<<1);
			writel(val, amlogic_pcie->phy->reset_base);
		} else {
			val = readl(amlogic_pcie->phy->reset_base);
			val |=	(0x1<<pcie_ctrl_a_rst_bit);
			writel(val, amlogic_pcie->phy->reset_base);
		}
		mdelay(1);
	} else {
		if (amlogic_pcie->rst_mod == 0) {
			val = readl(amlogic_pcie->phy->reset_base);
			val |= (0x1<<2);
			writel(val, amlogic_pcie->phy->reset_base);
		} else {
			val = readl(amlogic_pcie->phy->reset_base);
			val |= (0x1<<pcie_ctrl_a_rst_bit);
			writel(val, amlogic_pcie->phy->reset_base);
		}
		mdelay(1);
	}

	amlogic_pcie->phy->reset_state = 1;

	elbi_base = platform_get_resource_byname(pdev, IORESOURCE_MEM, "elbi");
	amlogic_pcie->elbi_base = devm_ioremap_resource(dev, elbi_base);
	if (IS_ERR(amlogic_pcie->elbi_base)) {
		ret = PTR_ERR(amlogic_pcie->elbi_base);
		goto fail_clk;
	}

	cfg_base = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cfg");
	amlogic_pcie->cfg_base = devm_ioremap_resource(dev, cfg_base);
	if (IS_ERR(amlogic_pcie->cfg_base)) {
		ret = PTR_ERR(amlogic_pcie->cfg_base);
		goto fail_clk;
	}

	ret = amlogic_add_pcie_port(amlogic_pcie, pdev);
	if (ret < 0)
		goto fail_clk;

	platform_set_drvdata(pdev, amlogic_pcie);
	device_create_file(&pdev->dev, &dev_attr_phyread_v2);
	device_create_file(&pdev->dev, &dev_attr_phywrite_v2);
	return 0;

fail_clk:
	clk_disable_unprepare(amlogic_pcie->clk);
fail_bus_clk:
	clk_disable_unprepare(amlogic_pcie->bus_clk);
fail_pcie:
	clk_disable_unprepare(amlogic_pcie->phy_clk);
	port_num--;
fail_pcie_phy:
	return ret;
}

static int __exit amlogic_pcie_remove(struct platform_device *pdev)
{
	struct amlogic_pcie *amlogic_pcie = platform_get_drvdata(pdev);

	if (amlogic_pcie->phy->power_state == 0) {
		dev_info(&pdev->dev, "PCIE phy power off, no remove\n");
		return 0;
	}

	device_remove_file(&pdev->dev, &dev_attr_phywrite_v2);
	device_remove_file(&pdev->dev, &dev_attr_phyread_v2);

	clk_disable_unprepare(amlogic_pcie->clk);
	clk_disable_unprepare(amlogic_pcie->bus_clk);
	clk_disable_unprepare(amlogic_pcie->phy_clk);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int amlogic_pcie_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct amlogic_pcie *amlogic_pcie = platform_get_drvdata(pdev);
	struct pcie_port *pp = &amlogic_pcie->pp;
	u32 val;

	if (!amlogic_pcie->pm_enable) {
		dev_info(dev, "don't suspend amlogic pcie\n");
		return 0;
	}

	if (amlogic_pcie->device_attch == 0) {
		dev_info(dev, "controller power off, no suspend\n");
		return 0;
	}

	dev_info(dev, "amlogic_pcie_suspend\n");

	/* clear MSE */
	val = dw_pcie_readl_rc(pp, PCI_COMMAND);
	val &= ~PCI_COMMAND_MEMORY;
	dw_pcie_writel_rc(pp, PCI_COMMAND, val);

	return 0;
}

static int amlogic_pcie_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct amlogic_pcie *amlogic_pcie = platform_get_drvdata(pdev);
	struct pcie_port *pp = &amlogic_pcie->pp;
	u32 val;

	if (!amlogic_pcie->pm_enable) {
		dev_info(dev, "don't resume amlogic pcie\n");
		return 0;
	}

	if (amlogic_pcie->device_attch == 0) {
		dev_info(dev, "controller power off, no resume\n");
		return 0;
	}

	dev_info(dev, "amlogic_pcie_resume\n");

	/* set MSE */
	val = dw_pcie_readl_rc(pp, PCI_COMMAND);
	val |= PCI_COMMAND_MEMORY;
	dw_pcie_writel_rc(pp, PCI_COMMAND, val);

	return 0;
}

static int amlogic_pcie_suspend_noirq(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct amlogic_pcie *amlogic_pcie = platform_get_drvdata(pdev);

	if (!amlogic_pcie->pm_enable) {
		dev_info(dev, "don't noirq suspend amlogic pcie\n");
		return 0;
	}

	if (amlogic_pcie->phy->device_attch == 0) {
		dev_info(dev, "PCIE phy power off, no suspend noirq\n");
		return 0;
	}

	if (amlogic_pcie->device_attch == 0) {
		dev_info(dev, "controller power off, no suspend noirq\n");
		if (amlogic_pcie->pcie_num == 1) {
			writel(0x1d, pcie_aml_regs_v2.pcie_phy_r[0]);
			amlogic_pcie->phy->power_state = 0;
		}
		return 0;
	}

	dev_info(dev, "amlogic_pcie_suspend_noirq\n");

	clk_disable_unprepare(amlogic_pcie->clk);
	clk_disable_unprepare(amlogic_pcie->bus_clk);
	clk_disable_unprepare(amlogic_pcie->phy_clk);
	amlogic_pcie->phy->reset_state = 0;

	if (amlogic_pcie->pcie_num == 1) {
		writel(0x1d, pcie_aml_regs_v2.pcie_phy_r[0]);
		amlogic_pcie->phy->power_state = 0;
	}

	return 0;
}

static int amlogic_pcie_resume_noirq(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct amlogic_pcie *amlogic_pcie = platform_get_drvdata(pdev);
	unsigned long rate = 100000000;

	if (!amlogic_pcie->pm_enable) {
		dev_info(dev, "don't noirq resume amlogic pcie\n");
		return 0;
	}

	if (amlogic_pcie->phy->device_attch == 0) {
		dev_info(dev, "PCIE phy power off, no resume noirq\n");
		return 0;
	}

	if (amlogic_pcie->pcie_num == 1) {
		writel(0x1c, pcie_aml_regs_v2.pcie_phy_r[0]);
		amlogic_pcie->phy->power_state = 1;
		udelay(500);
	}

	if (amlogic_pcie->device_attch == 0) {
		dev_info(dev, "controller power off, no resume noirq\n");
		return 0;
	}

	dev_info(dev, "amlogic_pcie_resume_noirq\n");
	if (!amlogic_pcie->phy->reset_state)
		clk_set_rate(amlogic_pcie->bus_clk, rate);

	amlogic_pcie->phy->reset_state = 1;

	clk_prepare_enable(amlogic_pcie->phy_clk);
	clk_prepare_enable(amlogic_pcie->bus_clk);
	clk_prepare_enable(amlogic_pcie->clk);
	udelay(500);

	return 0;
}

#else
#define amlogic_pcie_suspend NULL
#define amlogic_pcie_resume NULL
#define amlogic_pcie_suspend_noirq NULL
#define amlogic_pcie_resume_noirq NULL
#endif

static const struct dev_pm_ops amlogic_pcie_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(amlogic_pcie_suspend, amlogic_pcie_resume)
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(amlogic_pcie_suspend_noirq,
				      amlogic_pcie_resume_noirq)
};



static const struct of_device_id amlogic_pcie_of_match[] = {
	{ .compatible = "amlogic, amlogic-pcie-v2", },
	{},
};

static struct platform_driver amlogic_pcie_driver = {
	.remove		= __exit_p(amlogic_pcie_remove),
	.driver = {
		.name	= "amlogic-pcie-v2",
		.of_match_table = amlogic_pcie_of_match,
		.pm	= &amlogic_pcie_pm_ops,
	},
};

/* AMLOGIC PCIe driver does not allow module unload */
static int __init amlogic_pcie_init(void)
{
	return platform_driver_probe(&amlogic_pcie_driver, amlogic_pcie_probe);
}
subsys_initcall(amlogic_pcie_init);
