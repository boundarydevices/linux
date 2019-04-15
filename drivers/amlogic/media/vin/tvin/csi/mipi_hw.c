/*
 * drivers/amlogic/media/vin/tvin/csi/mipi_hw.c
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

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/etherdevice.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/amlogic/media/mipi/am_mipi_csi2.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin_v4l2.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/amlogic/power_ctrl.h>
#include "csi.h"

#define HHI_CSI_PHY         0xff63c000
#define HHI_CSI_PHY_CNTL0  (0x0d3 << 2)
#define HHI_CSI_PHY_CNTL1  (0x0d4 << 2)
#define HHI_CSI_PHY_CNTL2  (0x0d5 << 2)

static struct csi_adapt g_csi;

static inline uint32_t WRITE_CBUS_REG(void __iomem *base, uint32_t addr,
		unsigned long val)
{
	void __iomem *io_addr = base + addr;

	__raw_writel(val, io_addr);

	return 0;
}

inline void phy_update_wr_reg_bits(unsigned int reg,
				unsigned int mask, unsigned int val)
{
	unsigned int tmp, orig;
	void __iomem *base = g_csi.csi_phy;

	if (base !=  NULL) {
		orig = __raw_readl(base + reg);
		tmp = orig & ~mask;
		tmp |= val & mask;
		__raw_writel(tmp, base + reg);
	}
}

inline void WRITE_CSI_PHY_REG_BITS(unsigned int adr, unsigned int val,
		unsigned int start, unsigned int len)
{
	phy_update_wr_reg_bits(adr,
		((1 << len) - 1) << start, val << start);
}

inline void WRITE_CSI_PHY_REG(int addr, uint32_t val)
{
	void __iomem *reg_addr = g_csi.csi_phy + addr;

	__raw_writel(val, reg_addr);
}

inline uint32_t READ_CSI_PHY_REG(int addr)
{
	uint32_t val;
	void __iomem *reg_addr = g_csi.csi_phy + addr;

	val = __raw_readl(reg_addr);
	return val;
}

inline void WRITE_CSI_HST_REG(int addr, uint32_t val)
{
	void __iomem *reg_addr = g_csi.csi_host + addr;

	__raw_writel(val, reg_addr);
}

inline uint32_t READ_CSI_HST_REG(int addr)
{
	uint32_t val;
	void __iomem *reg_addr = g_csi.csi_host + addr;

	val = __raw_readl(reg_addr);
	return val;
}

inline void WRITE_CSI_ADPT_REG(int addr, uint32_t val)
{
	void __iomem *reg_addr = g_csi.csi_adapt + addr;

	__raw_writel(val, reg_addr);
}

inline uint32_t READ_CSI_ADPT_REG(int addr)
{
	uint32_t val;
	void __iomem *reg_addr = g_csi.csi_adapt + addr;

	val = __raw_readl(reg_addr);
	return val;
}

inline uint32_t READ_CSI_ADPT_REG_BIT(int addr,
		const uint32_t _start, const uint32_t _len)
{
	void __iomem *reg_addr = g_csi.csi_adapt + addr;

	return	((__raw_readl(reg_addr) >> (_start)) & ((1L << (_len)) - 1));
}

void mipi_phy_reg_wr_and_check(int32_t addr, int32_t data)
{

#ifdef MIPI_WR_AND_CHECK
	uint32_t tmp;
#endif
	WRITE_CSI_PHY_REG(addr, data);
#ifdef MIPI_WR_AND_CHECK
	tmp = READ_CSI_PHY_REG(addr);
	if (tmp != data) {
		pr_info("%s, addr = %x, data = %x, read write unmatch\n",
			__func__, addr, tmp);
	}
#endif

}

void init_am_mipi_csi2_clock(void)
{
	int32_t rc = 0;
	uint32_t csi_rate = 200000000;
	uint32_t adapt_rate = 666666667;

	g_csi.csi_clk = devm_clk_get(&(g_csi.p_dev->dev),
		"cts_csi_phy_clk_composite");
	if (IS_ERR(g_csi.csi_clk)) {
		pr_err("%s: cannot get clk_gate_csi !!!\n",
			__func__);
		g_csi.csi_clk = NULL;
		return;
	}
	clk_set_rate(g_csi.csi_clk, csi_rate);
	rc = clk_prepare_enable(g_csi.csi_clk);
	if (rc != 0) {
		pr_err("Failed to enable csi clk\n");
		return;
	}

	g_csi.adapt_clk = devm_clk_get(&(g_csi.p_dev->dev),
		"cts_csi_adapt_clk_composite");
	if (IS_ERR(g_csi.adapt_clk)) {
		pr_err("%s: cannot get clk_gate_adapt !!!\n",
			__func__);
		g_csi.adapt_clk = NULL;
		return;
	}
	clk_set_rate(g_csi.adapt_clk, adapt_rate);
	rc = clk_prepare_enable(g_csi.adapt_clk);
	if (rc != 0) {
		pr_err("Failed to enable adapt clk\n");
		return;
	}
}

void disable_am_mipi_csi2_clk(void)
{
	clk_disable_unprepare(g_csi.csi_clk);
	clk_disable_unprepare(g_csi.adapt_clk);
}

void deinit_am_mipi_csi2_clock(void)
{
	if (g_csi.csi_clk != NULL) {
		devm_clk_put(&g_csi.p_dev->dev, g_csi.csi_clk);
		g_csi.csi_clk = NULL;
	}

	if (g_csi.adapt_clk != NULL) {
		devm_clk_put(&g_csi.p_dev->dev, g_csi.adapt_clk);
		g_csi.adapt_clk = NULL;
	}
}

static void init_am_mipi_csi2_host(struct csi_parm_s *info)
{
	pr_info("info->lanes = %d\n", info->lanes);
	WRITE_CSI_HST_REG(CSI2_HOST_CSI2_RESETN, 1);
	WRITE_CSI_HST_REG(CSI2_HOST_N_LANES, (info->lanes-1)&3);
	WRITE_CSI_HST_REG(CSI2_HOST_MASK1, 0x0);
	WRITE_CSI_HST_REG(CSI2_HOST_MASK2, 0x0);
	udelay(1);
}

static int init_am_mipi_csi2_adapter(struct csi_parm_s *info)
{
	unsigned int data32;
	struct amcsi_dev_s *csi_devp;

	csi_devp = container_of(info, struct amcsi_dev_s, csi_parm);
	WRITE_CSI_ADPT_REG(CSI2_CLK_RESET, 0);
	data32  = 0;
	data32 |= CSI2_CFG_422TO444_MODE        << 21;
	data32 |= 0x1f                          << 16;
	data32 |= CSI2_CFG_COLOR_EXPAND         << 15;
	data32 |= CSI2_CFG_BUFFER_PIC_SIZE      << 11;
	data32 |= CSI2_CFG_USE_NULL_PACKET      << 10;
	data32 |= CSI2_CFG_INV_FIELD            <<  9;
	data32 |= CSI2_CFG_INTERLACE_EN         <<  8;
	data32 |= CSI2_CFG_ALL_TO_MEM           <<  4;
	data32 |= 0xf;
	WRITE_CSI_ADPT_REG(CSI2_GEN_CTRL0,  data32);

	data32  = 0;
	data32 |= (csi_devp->para.h_active - 1) << 16;
	data32 |= 0                             <<  0;
	WRITE_CSI_ADPT_REG(CSI2_X_START_END_ISP, data32);

	data32  = 0;
	data32 |= (csi_devp->para.v_active - 1) << 16;
	data32 |= 0                             <<  0;
	WRITE_CSI_ADPT_REG(CSI2_Y_START_END_ISP, data32);

	WRITE_CSI_ADPT_REG(CSI2_INTERRUPT_CTRL_STAT, (1 << 1));

	/*Enable clock*/
	data32  = 0;
	data32 |= 0 <<  2;
	data32 |= 1 <<  1;
	data32 |= 0 <<  0;
	WRITE_CSI_ADPT_REG(CSI2_CLK_RESET, data32);
	return 0;
}

static void powerup_csi_analog(struct csi_parm_s *info)
{
	void __iomem *base_addr;

	base_addr = ioremap_nocache(HHI_CSI_PHY, 0x400);
	if (base_addr == NULL) {
		pr_err("%s: Failed to ioremap addr\n", __func__);
		return;
	}

	WRITE_CBUS_REG(base_addr, HHI_CSI_PHY_CNTL0, 0x0b440585);
	WRITE_CBUS_REG(base_addr, HHI_CSI_PHY_CNTL1, 0x803f0000);
	WRITE_CBUS_REG(base_addr, HHI_CSI_PHY_CNTL2, 0xf002);

	iounmap(base_addr);

	power_ctrl_mempd0(1, 3, 6);
}

static void powerdown_csi_analog(void)
{
	power_ctrl_mempd0(0, 3, 6);
}

static void init_am_mipi_phy(struct csi_parm_s *info)
{
	uint32_t settle = 0;
	uint32_t ui_val = 0;
	uint32_t cycle_time = 5;

	ui_val = 1000 / info->settle;
	if ((1000 % info->settle) != 0)
		ui_val += 1;
	settle = (85 + 145 + (16 * ui_val)) / 2;
	settle = settle / cycle_time;
	pr_info("settle = 0x%04x\n", settle);

	mipi_phy_reg_wr_and_check(MIPI_PHY_CLK_LANE_CTRL, 0x3d8);
	mipi_phy_reg_wr_and_check(MIPI_PHY_TCLK_MISS, 0x9);
	mipi_phy_reg_wr_and_check(MIPI_PHY_TCLK_SETTLE, 0x1f);
	mipi_phy_reg_wr_and_check(MIPI_PHY_THS_EXIT, 0x1f);
	mipi_phy_reg_wr_and_check(MIPI_PHY_THS_SKIP, 0xa);
	mipi_phy_reg_wr_and_check(MIPI_PHY_THS_SETTLE, settle);
	mipi_phy_reg_wr_and_check(MIPI_PHY_TINIT, 0x4e20);
	mipi_phy_reg_wr_and_check(MIPI_PHY_TMBIAS, 0x100);
	mipi_phy_reg_wr_and_check(MIPI_PHY_TULPS_C, 0x1000);
	mipi_phy_reg_wr_and_check(MIPI_PHY_TULPS_S, 0x100);
	mipi_phy_reg_wr_and_check(MIPI_PHY_TLP_EN_W, 0x0c);
	mipi_phy_reg_wr_and_check(MIPI_PHY_TLPOK, 0x100);
	mipi_phy_reg_wr_and_check(MIPI_PHY_TWD_INIT, 0x400000);
	mipi_phy_reg_wr_and_check(MIPI_PHY_TWD_HS, 0x400000);
	mipi_phy_reg_wr_and_check(MIPI_PHY_DATA_LANE_CTRL, 0x0);
	mipi_phy_reg_wr_and_check(MIPI_PHY_DATA_LANE_CTRL1,
		0x3 | (0x1f << 2) | (0x3 << 7));
	mipi_phy_reg_wr_and_check(MIPI_PHY_MUX_CTRL0, 0x00000123);
	mipi_phy_reg_wr_and_check(MIPI_PHY_MUX_CTRL1, 0x00000123);
	mipi_phy_reg_wr_and_check(MIPI_PHY_CTRL, 0);
}

static void reset_am_mipi_csi2_host(void)
{
	WRITE_CSI_HST_REG(CSI2_HOST_CSI2_RESETN, 0);
}

static void reset_am_mipi_csi2_adapter(void)
{
	WRITE_CSI_ADPT_REG(CSI2_CLK_RESET, 1);
}

static void reset_am_mipi_phy(void)
{
	WRITE_CSI_PHY_REG_BITS(MIPI_PHY_CTRL, 0x1, 31, 1);
}

void am_mipi_csi2_para_init(struct platform_device *pdev)
{
	struct resource rs;
	int i = 0;
	int rtn = -1;

	memset(&g_csi, 0, sizeof(struct csi_adapt));
	g_csi.p_dev = pdev;

	for (i = 0; i < 3; i++) {
		rtn = of_address_to_resource(pdev->dev.of_node, i, &rs);
		if (rtn != 0) {
			pr_err("%s:Error idx %d get mipi csi reg resource\n",
				__func__, i);
			continue;
		} else {
			pr_info("%s: rs idx %d info: name: %s\n", __func__,
				i, rs.name);

			if (strcmp(rs.name, "csi_phy") == 0) {
				g_csi.csi_phy_reg = rs;
				g_csi.csi_phy =
				ioremap_nocache(g_csi.csi_phy_reg.start,
				resource_size(&g_csi.csi_phy_reg));
			} else if (strcmp(rs.name, "csi_host") == 0) {
				g_csi.csi_host_reg = rs;
				g_csi.csi_host =
				ioremap_nocache(g_csi.csi_host_reg.start,
				resource_size(&g_csi.csi_host_reg));
			} else if (strcmp(rs.name, "csi_adapt") == 0) {
				g_csi.csi_adapt_reg = rs;
				g_csi.csi_adapt =
				ioremap_nocache(g_csi.csi_adapt_reg.start,
				resource_size(&g_csi.csi_adapt_reg));
			} else {
				pr_err("%s:Error match address\n", __func__);
			}
		}
	}
}

void am_mipi_csi2_init(struct csi_parm_s *info)
{
	powerup_csi_analog(info);
	init_am_mipi_phy(info);
	init_am_mipi_csi2_host(info);
	init_am_mipi_csi2_adapter(info);
}

void am_mipi_csi2_uninit(void)
{
	reset_am_mipi_phy();
	reset_am_mipi_csi2_host();
	reset_am_mipi_csi2_adapter();
	disable_am_mipi_csi2_clk();
	powerdown_csi_analog();
}

void cal_csi_para(struct csi_parm_s *info)
{
}
