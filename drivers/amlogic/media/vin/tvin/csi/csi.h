/*
 * drivers/amlogic/media/vin/tvin/csi/csi.h
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

#ifndef __CSI_INPUT_H
#define __CSI_INPUT_H
#include <linux/cdev.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin_v4l2.h>
#include <linux/amlogic/media/mipi/am_mipi_csi2.h>
#include "../tvin_frontend.h"
#include "../tvin_global.h"
#ifdef PRINT_DEBUG_INFO
#define DPRINT(...)		printk(__VA_ARGS__)
#else
#define DPRINT(...)
#endif

#define CSI2_CFG_USE_NULL_PACKET    0
#define CSI2_CFG_BUFFER_PIC_SIZE    1
#define CSI2_CFG_422TO444_MODE      1
#define CSI2_CFG_INV_FIELD          0
#define CSI2_CFG_INTERLACE_EN       0
#define CSI2_CFG_COLOR_EXPAND       0
#define CSI2_CFG_ALL_TO_MEM         0


/*mipi-csi2 phy registers*/
#define MIPI_PHY_CTRL	          0x00
#define MIPI_PHY_CLK_LANE_CTRL	  0x04
#define MIPI_PHY_DATA_LANE_CTRL	  0x08
#define MIPI_PHY_DATA_LANE_CTRL1  0x0C
#define MIPI_PHY_TCLK_MISS	      0x10
#define MIPI_PHY_TCLK_SETTLE	  0x14
#define MIPI_PHY_THS_EXIT	      0x18
#define MIPI_PHY_THS_SKIP	      0x1C
#define MIPI_PHY_THS_SETTLE	      0x20
#define MIPI_PHY_TINIT	          0x24
#define MIPI_PHY_TULPS_C	      0x28
#define MIPI_PHY_TULPS_S	      0x2C
#define MIPI_PHY_TMBIAS		      0x30
#define MIPI_PHY_TLP_EN_W	      0x34
#define MIPI_PHY_TLPOK	          0x38
#define MIPI_PHY_TWD_INIT	      0x3C
#define MIPI_PHY_TWD_HS		      0x40
#define MIPI_PHY_AN_CTRL0	      0x44
#define MIPI_PHY_AN_CTRL1	      0x48
#define MIPI_PHY_AN_CTRL2	      0x4C
#define MIPI_PHY_CLK_LANE_STS	  0x50
#define MIPI_PHY_DATA_LANE0_STS	  0x54
#define MIPI_PHY_DATA_LANE1_STS	  0x58
#define MIPI_PHY_DATA_LANE2_STS	  0x5C
#define MIPI_PHY_DATA_LANE3_STS	  0x60
#define MIPI_PHY_INT_STS	      0x6C
#define MIPI_PHY_MUX_CTRL0       (0x61 << 2)
#define MIPI_PHY_MUX_CTRL1	     (0x62 << 2)

/*mipi-csi2 host registers*/
#define CSI2_HOST_CSI2_RESETN    0x010
#define CSI2_HOST_N_LANES        0x004
#define CSI2_HOST_MASK1          0x028
#define CSI2_HOST_MASK2          0x02c

/*adapt frontend registers*/
#define CSI2_CLK_RESET           (0x00 << 2)
#define CSI2_GEN_CTRL0           (0x01 << 2)
#define CSI2_GEN_CTRL1           (0x02 << 2)
#define CSI2_X_START_END_ISP     (0x03 << 2)
#define CSI2_Y_START_END_ISP     (0x04 << 2)
#define CSI2_INTERRUPT_CTRL_STAT (0x14 << 2)
#define CSI2_GEN_STAT0           (0x20 << 2)
#define CSI2_ERR_STAT0           (0x21 << 2)
#define CSI2_PIC_SIZE_STAT       (0x22 << 2)

enum amcsi_status_e {
	TVIN_AMCSI_STOP,
	TVIN_AMCSI_RUNNING,
	TVIN_AMCSI_START,
};

struct amcsi_dev_s {
	int                     index;
	dev_t                   devt;
	struct cdev             cdev;
	struct device           *dev;
	struct platform_device  *pdev;
	unsigned int            overflow_cnt;
	enum amcsi_status_e     dec_status;
	struct vdin_parm_s      para;
	struct csi_parm_s       csi_parm;
	unsigned char           reset;
	unsigned int            reset_count;
	unsigned int            irq_num;
	struct tvin_frontend_s  frontend;
	unsigned int            period;
	unsigned int            min_frmrate;
	struct timer_list       t;
	void __iomem            *csi_adapt_addr;
};

struct csi_adapt {
	struct platform_device  *p_dev;
	struct resource         csi_phy_reg;
	struct resource         csi_host_reg;
	struct resource         csi_adapt_reg;
	void __iomem            *csi_phy;
	void __iomem            *csi_host;
	void __iomem            *csi_adapt;
	struct clk              *csi_clk;
	struct clk              *adapt_clk;
};

void deinit_am_mipi_csi2_clock(void);
void am_mipi_csi2_para_init(struct platform_device *pdev);
void WRITE_CSI_ADPT_REG(int addr, uint32_t val);
uint32_t READ_CSI_PHY_REG(int addr);
uint32_t READ_CSI_HST_REG(int addr);
uint32_t READ_CSI_ADPT_REG(int addr);
uint32_t READ_CSI_ADPT_REG_BIT(int addr,
	const uint32_t _start, const uint32_t _len);

#endif
