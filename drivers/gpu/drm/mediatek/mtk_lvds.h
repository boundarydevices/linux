/* SPDX-License-Identifier: GPL-2.0
 *
 * MediaTek LVDS Driver Header
 *
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Huijuan Xie <huijuan.xie@mediatek.com>
 */

#ifndef _MTK_LVDS_H
#define _MTK_LVDS_H

#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_of.h>
#include <drm/drm_panel.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_bridge.h>
#include <drm/drm_connector.h>
#include <drm/drm_print.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_graph.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/component.h>
#include <linux/delay.h>
#include <linux/regmap.h>

#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_crtc.h"

/* LVDS TOP */
#define LVDSENC_FMT_CTRL	0x000
#define RG_LVDSENC_HSYNC_INV		BIT(0)
#define RG_LVDSENC_VSYNC_INV		BIT(1)
#define RG_LVDSENC_DE_INV		BIT(2)
#define RG_LVDSENC_DATA_FORMAT		(0x7 << 4)
#define RG_LVDSENC_8_BIT_MODDE		(0x0 << 4)
#define RG_LVDSENC_6_BIT_MODDE		(0x1 << 4)

#define LVDSENC_SRC	0x004
#define RG_LVDSENC_R_SEL		(0x3 << 0)
#define RG_LVDSENC_G_SEL		(0x3 << 2)
#define RG_LVDSENC_B_SEL		(0x3 << 4)
#define RG_LVDSENC_R		(0xff << 8)
#define RG_LVDSENC_G		(0xff << 16)
#define RG_LVDSENC_B		(0xff << 24)

#define LVDSENC_CTRL	0x008
#define RG_LVDSENC_HSYNC_SEL		(0x3 << 0)
#define RG_LVDSENC_VSYNC_SEL		(0x3 << 2)
#define RG_LVDSENC_DE_SEL		(0x3 << 4)
#define RG_LVDSENC_HSYNC		BIT(6)
#define RG_LVDSENC_VSYNC		BIT(7)
#define RG_LVDSENC_DE		BIT(8)

#define LVDSENC_R_SEL	0x00C
#define RG_LVDSENC_R0_SEL		(0x7 << 0)
#define RG_LVDSENC_R1_SEL		(0x7 << 3)
#define RG_LVDSENC_R2_SEL		(0x7 << 6)
#define RG_LVDSENC_R3_SEL		(0x7 << 9)
#define RG_LVDSENC_R4_SEL		(0x7 << 12)
#define RG_LVDSENC_R5_SEL		(0x7 << 15)
#define RG_LVDSENC_R6_SEL		(0x7 << 18)
#define RG_LVDSENC_R7_SEL		(0x7 << 21)

#define LVDSENC_G_SEL	0x010
#define RG_LVDSENC_G0_SEL		(0x7 << 0)
#define RG_LVDSENC_G1_SEL		(0x7 << 3)
#define RG_LVDSENC_G2_SEL		(0x7 << 6)
#define RG_LVDSENC_G3_SEL		(0x7 << 9)
#define RG_LVDSENC_G4_SEL		(0x7 << 12)
#define RG_LVDSENC_G5_SEL		(0x7 << 15)
#define RG_LVDSENC_G6_SEL		(0x7 << 18)
#define RG_LVDSENC_G7_SEL		(0x7 << 21)

#define LVDSENC_B_SEL	0x014
#define RG_LVDSENC_B0_SEL		(0x7 << 0)
#define RG_LVDSENC_B1_SEL		(0x7 << 3)
#define RG_LVDSENC_B2_SEL		(0x7 << 6)
#define RG_LVDSENC_B3_SEL		(0x7 << 9)
#define RG_LVDSENC_B4_SEL		(0x7 << 12)
#define RG_LVDSENC_B5_SEL		(0x7 << 15)
#define RG_LVDSENC_B6_SEL		(0x7 << 18)
#define RG_LVDSENC_B7_SEL		(0x7 << 21)

#define LVDSENC_OUT_CTRL	0x018
#define RG_LVDSENC_LVDS_7_10_FIFO_EN	BIT(0)
#define RG_LVDSENC_OUT_FIFO_EN		BIT(1)
#define RG_LVDSENC_DP_MODE		BIT(2)
#define RG_LVDSENC_LVDSRX_FIFO_EN		BIT(31)

#define LVDSENC_CH_SWAP	0x01C
#define RG_LVDSENC_CH0_SEL		(0x7 << 0)
#define RG_LVDSENC_CH1_SEL		(0x7 << 3)
#define RG_LVDSENC_CH2_SEL		(0x7 << 6)
#define RG_LVDSENC_CH3_SEL		(0x7 << 9)
#define RG_LVDSENC_CLK_SEL		(0x7 << 12)
#define RG_LVDSENC_ML_SWAP		(0x1f << 16)
#define RG_LVDSENC_PN_SWAP		(0x1f << 24)
#define RG_LVDSENC_SWAP_SEL		BIT(31)

#define LVDSENC_CLK_CTRL	0x020
#define RG_LVDSENC_TX_CK_EN		BIT(0)
#define RG_LVDSENC_RX_CK_EN		BIT(1)
#define RG_LVDSENC_TEST_CK_EN		BIT(2)
#define RG_LVDSENC_TEST_CK_SEL0		BIT(8)
#define RG_LVDSENC_TEST_CK_SEL1		BIT(9)

#define LVDSENC_CRC_CTRL	0x028
#define RG_LVDSENC_CRC_START		BIT(0)
#define RG_LVDSENC_CRC_CLR		BIT(1)
#define RG_LVDSENC_LVDSRX_CRC_START1		BIT(8)
#define RG_LVDSENC_LVDSRX_CRC_CLR1		BIT(10)

#define LVDSENC_CLK	0x02C
#define RG_LVDSENC_CLK_CTRL		(0x7f << 0)
#define RG_LVDSENC_CLK_EN		BIT(7)

#define LVDSENC_MON	0x030
#define RG_LVDSENC_MON_SEL		(0x3f << 0)
#define RG_LVDSENC_MON_EN		BIT(8)
#define RG_LVDSENC_TEST_EN		BIT(16)

#define LVDSENC_SOFT_RESET	0x034
#define RG_LVDSENC_PCLK_SW_RST		BIT(0)
#define RG_LVDSENC_CTSCLK_SW_RST		BIT(1)

#define LVDSENC_INPUT_CRC_STATUS	0x03C
#define RG_LVDSENC_IN_CRC_RDY		BIT(0)
#define RG_LVDSENC_IN_CRC_VALUE		(0xffffff << 1)

#define LVDSENC_CHL_SRC0	0x040
#define RG_LVDSENC_LVDS_CH0		(0x3ff << 0)
#define RG_LVDSENC_LVDS_CH1		(0x3ff << 10)
#define RG_LVDSENC_LVDS_CH2		(0x3ff << 20)

#define LVDSENC_CHL_SRC1	0x044
#define RG_LVDSENC_LVDS_CH3		(0x3ff << 0)
#define RG_LVDSENC_LVDS_CLK		(0x3ff << 10)
#define RG_LVDSENC_PAT_EN		BIT(31)

#define LVDSENC_PATN_TOTAL	0x048
#define RG_LVDSENC_PTGEN_HTOTAL		(0x1fff << 0)
#define RG_LVDSENC_PTGEN_VTOTAL		(0xfff << 16)

#define LVDSENC_PATN_WIDTH	0x04C
#define RG_LVDSENC_PTGEN_HWIDTH		(0x1fff << 0)
#define RG_LVDSENC_PTGEN_VWIDTH		(0xfff << 16)

#define LVDSENC_PATN_START	0x050
#define RG_LVDSENC_PTGEN_HSTART		(0x1fff << 0)
#define RG_LVDSENC_PTGEN_VSTART		(0xfff << 16)

#define LVDSENC_PATN_ACTIVE	0x054
#define RG_LVDSENC_PTGEN_HACTIVE		(0x1fff << 0)
#define RG_LVDSENC_PTGEN_VACTIVE		(0xfff << 16)

#define LVDSENC_TST_PAT       0x058
#define RG_LVDSENC_TST_PATN_EN		BIT(0)
#define RG_LVDSENC_TST_PATN_TYPE		(0xff << 8)
#define RG_LVDSENC_PTGEN_CLOR_BAR_TH	(0xfff << 16)

#define LVDSENC_PAT_EDGE  0x05C
#define RG_LVDSENC_BD_R		(0xff << 0)
#define RG_LVDSENC_BD_G		(0xff << 8)
#define RG_LVDSENC_BD_B		(0xff << 16)

#define LVDSENC_PAT_SRC  0x060
#define RG_LVDSENC_PTGEN_R  (0xff << 0)
#define RG_LVDSENC_PTGEN_G  (0xff << 8)
#define RG_LVDSENC_PTGEN_B  (0xff << 16)

#define LVDSENC_CRC_STATUS 0x70
#define RG_LVDSENC_OUT_CRC_RDY		BIT(0)
#define RG_LVDSENC_OUT_CRC_VALUE  (0xffff << 1)

#define LVDS_MAX_LANE_CNT	8

struct mtk_lvds {
	struct mtk_ddp_comp ddp_comp;
	struct drm_bridge bridge;
	struct drm_bridge *next_bridge;
	struct drm_connector connector;
	struct drm_panel *panel;
	struct device *dev;
	struct phy *phy;
	struct clk *pix_clk_gate;
	struct clk *clkts_clk_gate;
	struct clk *dpix;
	struct clk *clkdig;
	struct drm_display_mode mode;
	u32 lane_count;
	u32 lvds_fmt;
	void __iomem *regs;
	bool powered;
	bool enabled;
	int refcount;
};

#endif /* _MTK_LVDS_H */

