// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek MIPI CSI v2.1 driver
 *
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <dt-bindings/phy/phy.h>
#include <linux/bitfield.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "phy-mtk-io.h"
#include "phy-mtk-mipi-csi-2-1-rx-reg.h"

#define MTK_PHY_CPHY_SETTLE_DELAY_DT 0x10
#define MTK_PHY_DPHY_SETTLE_DELAY_DT 0x46
#define MTK_PHY_SETTLE_DELAY_CK 0x11
#define MTK_PHY_HS_TRAIL_PARAMETER 0x34

#define MTK_PHY_BITS(base, reg, field, val) do { \
	u32 __iomem *__p = (base) + (reg); \
	u32 __v = readl(__p); \
	__v &= ~field##_MASK; \
	__v |= (((val) << field##_SHIFT) & field##_MASK); \
	writel(__v, __p); \
} while (0)

#define MTK_PHY_READ_BITS(base, reg, field) \
({ \
	u32 __iomem *__p = (base) + (reg); \
	u32 __v = readl(__p); \
	__v &= field##_MASK; \
	__v >>= field##_SHIFT; \
	__v; \
})

#define MTK_PHY_READ_REG(base, reg) \
({ \
	u32 __iomem *__p = (base) + (reg); \
	u32 __v = readl(__p); \
	__v; \
})

#define MTK_PHY_WRITE_REG(base, reg, val) { \
	u32 __iomem *__p = (base) + (reg); \
	writel(val, __p); \
}

enum CSI_PORT {
	CSI_PORT_0 = 0,
	CSI_PORT_1,
	CSI_PORT_0A,
	CSI_PORT_0B,
	CSI_PORT_1A,
	CSI_PORT_1B,
	CSI_PORT_MAX_NUM,
};

struct mtk_mipi_cdphy_port {
	struct device *dev;
	void __iomem *base;
	struct phy *phy;
	u32 type;
	u32 mode;
	u32 num_lanes;

	s64 mipi_pixel_rate;
	s64 buffered_pixel_rate;
	s64 customized_pixel_rate;

	u32 port;
	u32 port_a;
	u32 port_b;
	u32 port_num;
	u32 is_4d1c:1;
	u32 is_cphy:1;
	u32 m_csi_efuse;

	void __iomem *reg_ana_csi_rx[CSI_PORT_MAX_NUM];
	void __iomem *reg_ana_dphy_top[CSI_PORT_MAX_NUM];
	void __iomem *reg_ana_cphy_top[CSI_PORT_MAX_NUM];

	u32 cphy_settle_delay_dt;
	u32 dphy_settle_delay_dt;
	u32 settle_delay_ck;
	u32 hs_trail_parameter;
};

enum PHY_TYPE {
	DPHY = 0,
	CPHY,
	CDPHY,
};

#define SENINF_CSI_PORT_NAMES \
	"0", \
	"1", \
	"0A", \
	"0B", \
	"1A", \
	"1B", \

static const char * const csi_port_names[] = {
	SENINF_CSI_PORT_NAMES
};

static int csirx_phyA_power_on(struct mtk_mipi_cdphy_port *port_ctx, u32 port_idx, int en)
{
	void __iomem *base = port_ctx->reg_ana_csi_rx[port_idx];

	MTK_PHY_BITS(base, CDPHY_RX_ANA_8, RG_CSI0_L0_T0AB_EQ_OS_CAL_EN, 0);
	MTK_PHY_BITS(base, CDPHY_RX_ANA_8, RG_CSI0_L1_T1AB_EQ_OS_CAL_EN, 0);
	MTK_PHY_BITS(base, CDPHY_RX_ANA_8, RG_CSI0_L2_T1BC_EQ_OS_CAL_EN, 0);
	MTK_PHY_BITS(base, CDPHY_RX_ANA_8, RG_CSI0_XX_T0BC_EQ_OS_CAL_EN, 0);
	MTK_PHY_BITS(base, CDPHY_RX_ANA_8, RG_CSI0_XX_T0CA_EQ_OS_CAL_EN, 0);
	MTK_PHY_BITS(base, CDPHY_RX_ANA_8, RG_CSI0_XX_T1CA_EQ_OS_CAL_EN, 0);
	MTK_PHY_BITS(base, CDPHY_RX_ANA_0, RG_CSI0_BG_LPF_EN, 0);
	MTK_PHY_BITS(base, CDPHY_RX_ANA_0, RG_CSI0_BG_CORE_EN, 0);
	usleep_range(200, 300);

	if (en) {
		MTK_PHY_BITS(base, CDPHY_RX_ANA_0,
			     RG_CSI0_BG_CORE_EN, 1);
		usleep_range(30, 40);
		MTK_PHY_BITS(base, CDPHY_RX_ANA_0,
			     RG_CSI0_BG_LPF_EN, 1);
		udelay(1);
		MTK_PHY_BITS(base, CDPHY_RX_ANA_8,
			     RG_CSI0_L0_T0AB_EQ_OS_CAL_EN, 1);
		MTK_PHY_BITS(base, CDPHY_RX_ANA_8,
			     RG_CSI0_L1_T1AB_EQ_OS_CAL_EN, 1);
		MTK_PHY_BITS(base, CDPHY_RX_ANA_8,
			     RG_CSI0_L2_T1BC_EQ_OS_CAL_EN, 1);
		MTK_PHY_BITS(base, CDPHY_RX_ANA_8,
			     RG_CSI0_XX_T0BC_EQ_OS_CAL_EN, 1);
		MTK_PHY_BITS(base, CDPHY_RX_ANA_8,
			     RG_CSI0_XX_T0CA_EQ_OS_CAL_EN, 1);
		MTK_PHY_BITS(base, CDPHY_RX_ANA_8,
			     RG_CSI0_XX_T1CA_EQ_OS_CAL_EN, 1);
		udelay(1);
	}

	return 0;
}

static int apply_efuse_data(struct mtk_mipi_cdphy_port *port_ctx)
{
	int ret = 0;
	u32 port;
	void __iomem *base;
	u32 m_csi_efuse = port_ctx->m_csi_efuse;

	if (m_csi_efuse == 0) {
		dev_dbg(port_ctx->dev, "No efuse data. Returned.\n");
		return -1;
	}

	port = port_ctx->port;
	base = port_ctx->reg_ana_csi_rx[port];

	MTK_PHY_BITS(base, CDPHY_RX_ANA_2,
		     RG_CSI0_L0P_T0A_HSRT_CODE, (m_csi_efuse >> 27) & 0x1f);
	MTK_PHY_BITS(base, CDPHY_RX_ANA_2,
		     RG_CSI0_L0N_T0B_HSRT_CODE, (m_csi_efuse >> 27) & 0x1f);
	MTK_PHY_BITS(base, CDPHY_RX_ANA_3,
		     RG_CSI0_L1P_T0C_HSRT_CODE, (m_csi_efuse >> 22) & 0x1f);
	MTK_PHY_BITS(base, CDPHY_RX_ANA_3,
		     RG_CSI0_L1N_T1A_HSRT_CODE, (m_csi_efuse >> 22) & 0x1f);
	MTK_PHY_BITS(base, CDPHY_RX_ANA_4,
		     RG_CSI0_L2P_T1B_HSRT_CODE, (m_csi_efuse >> 17) & 0x1f);
	MTK_PHY_BITS(base, CDPHY_RX_ANA_4,
		     RG_CSI0_L2N_T1C_HSRT_CODE, (m_csi_efuse >> 17) & 0x1f);

	dev_dbg(port_ctx->dev,
		"CSI%dA CDPHY_RX_ANA_2(0x%x) CDPHY_RX_ANA_3(0x%x) CDPHY_RX_ANA_4(0x%x)",
		port_ctx->port,
		MTK_PHY_READ_REG(base, CDPHY_RX_ANA_2),
		MTK_PHY_READ_REG(base, CDPHY_RX_ANA_3),
		MTK_PHY_READ_REG(base, CDPHY_RX_ANA_4));

	if (port_ctx->is_4d1c == 0)
		return ret;

	port = port_ctx->port_b;
	base = port_ctx->reg_ana_csi_rx[port];

	MTK_PHY_BITS(base, CDPHY_RX_ANA_2,
		     RG_CSI0_L0P_T0A_HSRT_CODE, (m_csi_efuse >> 12) & 0x1f);
	MTK_PHY_BITS(base, CDPHY_RX_ANA_2,
		     RG_CSI0_L0N_T0B_HSRT_CODE, (m_csi_efuse >> 12) & 0x1f);
	MTK_PHY_BITS(base, CDPHY_RX_ANA_3,
		     RG_CSI0_L1P_T0C_HSRT_CODE, (m_csi_efuse >> 7) & 0x1f);
	MTK_PHY_BITS(base, CDPHY_RX_ANA_3,
		     RG_CSI0_L1N_T1A_HSRT_CODE, (m_csi_efuse >> 7) & 0x1f);


	MTK_PHY_BITS(base, CDPHY_RX_ANA_4,
		     RG_CSI0_L2P_T1B_HSRT_CODE, (m_csi_efuse >> 2) & 0x1f);
	MTK_PHY_BITS(base, CDPHY_RX_ANA_4,
		     RG_CSI0_L2N_T1C_HSRT_CODE, (m_csi_efuse >> 2) & 0x1f);
	dev_dbg(port_ctx->dev,
		"CSI%dB CDPHY_RX_ANA_2(0x%x) CDPHY_RX_ANA_3(0x%x) CDPHY_RX_ANA_4(0x%x)",
		port_ctx->port,
		MTK_PHY_READ_REG(base, CDPHY_RX_ANA_2),
		MTK_PHY_READ_REG(base, CDPHY_RX_ANA_3),
		MTK_PHY_READ_REG(base, CDPHY_RX_ANA_4));

	return ret;
}

static int phy_init_iomem(struct mtk_mipi_cdphy_port *port_ctx)
{
	void __iomem *ana_base = port_ctx->base;

	port_ctx->reg_ana_csi_rx[CSI_PORT_0] =
	port_ctx->reg_ana_csi_rx[CSI_PORT_0A] = ana_base + 0;
	port_ctx->reg_ana_csi_rx[CSI_PORT_0B] = ana_base + 0x1000;

	port_ctx->reg_ana_dphy_top[CSI_PORT_0A] =
	port_ctx->reg_ana_dphy_top[CSI_PORT_0B] =
	port_ctx->reg_ana_dphy_top[CSI_PORT_0] = ana_base + 0x2000;

	port_ctx->reg_ana_cphy_top[CSI_PORT_0A] =
	port_ctx->reg_ana_cphy_top[CSI_PORT_0B] =
	port_ctx->reg_ana_cphy_top[CSI_PORT_0] = ana_base + 0x3000;

	return 0;
}

static int csirx_phyA_init(struct mtk_mipi_cdphy_port *port_ctx)
{
	u32 i, port;
	void __iomem *base;

	for (i = 0; i <= port_ctx->is_4d1c; i++) {
		port = i ? port_ctx->port_b : port_ctx->port;
		base = port_ctx->reg_ana_csi_rx[port];

		MTK_PHY_BITS(base, CDPHY_RX_ANA_1,
			     RG_CSI0_BG_LPRX_VTL_SEL, 0x4);
		MTK_PHY_BITS(base, CDPHY_RX_ANA_1,
			     RG_CSI0_BG_LPRX_VTH_SEL, 0x4);
		MTK_PHY_BITS(base, CDPHY_RX_ANA_2,
			     RG_CSI0_BG_ALP_RX_VTL_SEL, 0x4);
		MTK_PHY_BITS(base, CDPHY_RX_ANA_2,
			     RG_CSI0_BG_ALP_RX_VTH_SEL, 0x4);
		MTK_PHY_BITS(base, CDPHY_RX_ANA_1,
			     RG_CSI0_BG_VREF_SEL, 0x8);
		MTK_PHY_BITS(base, CDPHY_RX_ANA_1,
			     RG_CSI0_CDPHY_EQ_DES_VREF_SEL, 0x2);
		MTK_PHY_BITS(base, CDPHY_RX_ANA_5,
			     RG_CSI0_CDPHY_EQ_BW, 0x3);
		MTK_PHY_BITS(base, CDPHY_RX_ANA_5,
			     RG_CSI0_CDPHY_EQ_IS, 0x1);
		MTK_PHY_BITS(base, CDPHY_RX_ANA_5,
			     RG_CSI0_CDPHY_EQ_LATCH_EN, 0x1);
		MTK_PHY_BITS(base, CDPHY_RX_ANA_5,
			     RG_CSI0_CDPHY_EQ_DG0_EN, 0x1);
		MTK_PHY_BITS(base, CDPHY_RX_ANA_5,
			     RG_CSI0_CDPHY_EQ_DG1_EN, 0x1);
		MTK_PHY_BITS(base, CDPHY_RX_ANA_5,
			     RG_CSI0_CDPHY_EQ_SR0, 0x0);
		MTK_PHY_BITS(base, CDPHY_RX_ANA_5,
			     RG_CSI0_CDPHY_EQ_SR1, 0x0);
		MTK_PHY_BITS(base, CDPHY_RX_ANA_9,
			     RG_CSI0_RESERVE, 0x3003);
		MTK_PHY_BITS(base, CDPHY_RX_ANA_SETTING_0,
			     CSR_CSI_RST_MODE, 0x2);

		MTK_PHY_BITS(base, CDPHY_RX_ANA_2,
			     RG_CSI0_L0P_T0A_HSRT_CODE, 0x10);
		MTK_PHY_BITS(base, CDPHY_RX_ANA_2,
			     RG_CSI0_L0N_T0B_HSRT_CODE, 0x10);
		MTK_PHY_BITS(base, CDPHY_RX_ANA_3,
			     RG_CSI0_L1P_T0C_HSRT_CODE, 0x10);
		MTK_PHY_BITS(base, CDPHY_RX_ANA_3,
			     RG_CSI0_L1N_T1A_HSRT_CODE, 0x10);
		MTK_PHY_BITS(base, CDPHY_RX_ANA_4,
			     RG_CSI0_L2P_T1B_HSRT_CODE, 0x10);
		MTK_PHY_BITS(base, CDPHY_RX_ANA_4,
			     RG_CSI0_L2N_T1C_HSRT_CODE, 0x10);
		MTK_PHY_BITS(base, CDPHY_RX_ANA_0,
			     RG_CSI0_CPHY_T0_CDR_FIRST_EDGE_EN, 0x0);
		MTK_PHY_BITS(base, CDPHY_RX_ANA_0,
			     RG_CSI0_CPHY_T1_CDR_FIRST_EDGE_EN, 0x0);
		MTK_PHY_BITS(base, CDPHY_RX_ANA_2,
			     RG_CSI0_CPHY_T0_CDR_SELF_CAL_EN, 0x0);
		MTK_PHY_BITS(base, CDPHY_RX_ANA_2,
			     RG_CSI0_CPHY_T1_CDR_SELF_CAL_EN, 0x0);

		MTK_PHY_BITS(base, CDPHY_RX_ANA_6,
			     RG_CSI0_CPHY_T0_CDR_CK_DELAY, 0x0);
		MTK_PHY_BITS(base, CDPHY_RX_ANA_7,
			     RG_CSI0_CPHY_T1_CDR_CK_DELAY, 0x0);
		MTK_PHY_BITS(base, CDPHY_RX_ANA_6,
			     RG_CSI0_CPHY_T0_CDR_AB_WIDTH, 0x9);
		MTK_PHY_BITS(base, CDPHY_RX_ANA_6,
			     RG_CSI0_CPHY_T0_CDR_BC_WIDTH, 0x9);
		MTK_PHY_BITS(base, CDPHY_RX_ANA_6,
			     RG_CSI0_CPHY_T0_CDR_CA_WIDTH, 0x9);
		MTK_PHY_BITS(base, CDPHY_RX_ANA_7,
			     RG_CSI0_CPHY_T1_CDR_AB_WIDTH, 0x9);
		MTK_PHY_BITS(base, CDPHY_RX_ANA_7,
			     RG_CSI0_CPHY_T1_CDR_BC_WIDTH, 0x9);
		MTK_PHY_BITS(base, CDPHY_RX_ANA_7,
			     RG_CSI0_CPHY_T1_CDR_CA_WIDTH, 0x9);

		dev_dbg(port_ctx->dev, "port:%d CDPHY_RX_ANA_0(0x%x)\n",
			port, MTK_PHY_READ_REG(base, CDPHY_RX_ANA_0));
	}

	apply_efuse_data(port_ctx);

	return 0;
}

static int csirx_dphy_init(struct mtk_mipi_cdphy_port *port_ctx)
{
	void __iomem *base = port_ctx->reg_ana_dphy_top[port_ctx->port];
	u32 settle_delay_dt, settle_delay_ck, hs_trail, hs_trail_en;
	int bit_per_pixel;
	u64 data_rate;

	settle_delay_dt = port_ctx->is_cphy ? port_ctx->cphy_settle_delay_dt :
					 port_ctx->dphy_settle_delay_dt;

	dev_info(port_ctx->dev, "phyRx: real_settle(0x%x),dphy_settle(0x%x),cphy_settle(0x%x)\n",
		 settle_delay_dt,
		 port_ctx->dphy_settle_delay_dt,
		 port_ctx->cphy_settle_delay_dt);

	MTK_PHY_BITS(base, DPHY_RX_DATA_LANE0_HS_PARAMETER,
		     RG_CDPHY_RX_LD0_TRIO0_HS_SETTLE_PARAMETER,
		     settle_delay_dt);
	MTK_PHY_BITS(base, DPHY_RX_DATA_LANE1_HS_PARAMETER,
		     RG_CDPHY_RX_LD1_TRIO1_HS_SETTLE_PARAMETER,
		     settle_delay_dt);
	MTK_PHY_BITS(base, DPHY_RX_DATA_LANE2_HS_PARAMETER,
		     RG_CDPHY_RX_LD2_TRIO2_HS_SETTLE_PARAMETER,
		     settle_delay_dt);
	MTK_PHY_BITS(base, DPHY_RX_DATA_LANE3_HS_PARAMETER,
		     RG_CDPHY_RX_LD3_TRIO3_HS_SETTLE_PARAMETER,
		     settle_delay_dt);

	settle_delay_ck = port_ctx->settle_delay_ck;

	dev_info(port_ctx->dev, "phyRx: settle_delay_ck: %d\n", settle_delay_ck);

	MTK_PHY_BITS(base, DPHY_RX_CLOCK_LANE0_HS_PARAMETER,
		     RG_DPHY_RX_LC0_HS_SETTLE_PARAMETER,
		     settle_delay_ck);
	MTK_PHY_BITS(base, DPHY_RX_CLOCK_LANE1_HS_PARAMETER,
		     RG_DPHY_RX_LC1_HS_SETTLE_PARAMETER,
		     settle_delay_ck);

	/* Settle delay by lane*/
	MTK_PHY_BITS(base, DPHY_RX_DATA_LANE0_HS_PARAMETER,
		     RG_CDPHY_RX_LD0_TRIO0_HS_PREPARE_PARAMETER, 2);
	MTK_PHY_BITS(base, DPHY_RX_DATA_LANE1_HS_PARAMETER,
		     RG_CDPHY_RX_LD1_TRIO1_HS_PREPARE_PARAMETER, 2);
	MTK_PHY_BITS(base, DPHY_RX_DATA_LANE2_HS_PARAMETER,
		     RG_CDPHY_RX_LD2_TRIO2_HS_PREPARE_PARAMETER, 2);
	MTK_PHY_BITS(base, DPHY_RX_DATA_LANE3_HS_PARAMETER,
		     RG_CDPHY_RX_LD3_TRIO3_HS_PREPARE_PARAMETER, 2);

	hs_trail = port_ctx->hs_trail_parameter;

	dev_info(port_ctx->dev, "phyRx: hs_trail: %d\n", hs_trail);

	MTK_PHY_BITS(base, DPHY_RX_DATA_LANE0_HS_PARAMETER,
		     RG_DPHY_RX_LD0_HS_TRAIL_PARAMETER, hs_trail);
	MTK_PHY_BITS(base, DPHY_RX_DATA_LANE1_HS_PARAMETER,
		     RG_DPHY_RX_LD1_HS_TRAIL_PARAMETER, hs_trail);
	MTK_PHY_BITS(base, DPHY_RX_DATA_LANE2_HS_PARAMETER,
		     RG_DPHY_RX_LD2_HS_TRAIL_PARAMETER, hs_trail);
	MTK_PHY_BITS(base, DPHY_RX_DATA_LANE3_HS_PARAMETER,
		     RG_DPHY_RX_LD3_HS_TRAIL_PARAMETER, hs_trail);

	if (!port_ctx->is_cphy) {
		bit_per_pixel = 10;
		if (port_ctx->customized_pixel_rate != 0)
			data_rate = port_ctx->customized_pixel_rate * bit_per_pixel;
		else
			data_rate = port_ctx->mipi_pixel_rate * bit_per_pixel;

		do_div(data_rate, port_ctx->num_lanes);
		hs_trail_en = data_rate < 1400000000;
		MTK_PHY_BITS(base, DPHY_RX_DATA_LANE0_HS_PARAMETER,
			     RG_DPHY_RX_LD0_HS_TRAIL_EN, hs_trail_en);
		MTK_PHY_BITS(base, DPHY_RX_DATA_LANE1_HS_PARAMETER,
			     RG_DPHY_RX_LD1_HS_TRAIL_EN, hs_trail_en);
		MTK_PHY_BITS(base, DPHY_RX_DATA_LANE2_HS_PARAMETER,
			     RG_DPHY_RX_LD2_HS_TRAIL_EN, hs_trail_en);
		MTK_PHY_BITS(base, DPHY_RX_DATA_LANE3_HS_PARAMETER,
			     RG_DPHY_RX_LD3_HS_TRAIL_EN, hs_trail_en);
	}

	return 0;
}

static int csirx_cphy_init(struct mtk_mipi_cdphy_port *port_ctx)
{
	void __iomem *base = port_ctx->reg_ana_cphy_top[port_ctx->port];

	MTK_PHY_BITS(base, CPHY_RX_DETECT_CTRL_POST,
		     RG_CPHY_RX_DATA_VALID_POST_EN, 1);

	return 0;
}

static int csirx_phy_init(struct mtk_mipi_cdphy_port *port_ctx)
{
	/* phy init iomem */
	phy_init_iomem(port_ctx);

	/* phyA init */
	csirx_phyA_init(port_ctx);

	/* phyD init */
	csirx_dphy_init(port_ctx);

	/* phyC init */
	csirx_cphy_init(port_ctx);

	return 0;
}


static int csirx_phyA_setting(struct mtk_mipi_cdphy_port *port_ctx)
{
	void __iomem *base, *baseA, *baseB;

	base = port_ctx->reg_ana_csi_rx[port_ctx->port];
	baseA = port_ctx->reg_ana_csi_rx[port_ctx->port_a];
	baseB = port_ctx->reg_ana_csi_rx[port_ctx->port_b];

	if (!port_ctx->is_cphy) { /* dphy */
		if (port_ctx->is_4d1c) {
			MTK_PHY_BITS(baseA, CDPHY_RX_ANA_0,
				     RG_CSI0_CPHY_EN, 0);
			MTK_PHY_BITS(baseB, CDPHY_RX_ANA_0,
				     RG_CSI0_CPHY_EN, 0);
			/* clear clk sel first */
			MTK_PHY_BITS(baseA, CDPHY_RX_ANA_0,
				     RG_CSI0_DPHY_L0_CKMODE_EN, 0);
			MTK_PHY_BITS(baseA, CDPHY_RX_ANA_0,
				     RG_CSI0_DPHY_L1_CKMODE_EN, 0);
			MTK_PHY_BITS(baseA, CDPHY_RX_ANA_0,
				     RG_CSI0_DPHY_L2_CKMODE_EN, 0);
			MTK_PHY_BITS(baseB, CDPHY_RX_ANA_0,
				     RG_CSI0_DPHY_L0_CKMODE_EN, 0);
			MTK_PHY_BITS(baseB, CDPHY_RX_ANA_0,
				     RG_CSI0_DPHY_L1_CKMODE_EN, 0);
			MTK_PHY_BITS(baseB, CDPHY_RX_ANA_0,
				     RG_CSI0_DPHY_L2_CKMODE_EN, 0);

			MTK_PHY_BITS(baseA, CDPHY_RX_ANA_0,
				     RG_CSI0_DPHY_L0_CKSEL, 1);
			MTK_PHY_BITS(baseA, CDPHY_RX_ANA_0,
				     RG_CSI0_DPHY_L1_CKSEL, 1);
			MTK_PHY_BITS(baseA, CDPHY_RX_ANA_0,
				     RG_CSI0_DPHY_L2_CKSEL, 1);
			MTK_PHY_BITS(baseB, CDPHY_RX_ANA_0,
				     RG_CSI0_DPHY_L0_CKSEL, 1);
			MTK_PHY_BITS(baseB, CDPHY_RX_ANA_0,
				     RG_CSI0_DPHY_L1_CKSEL, 1);
			MTK_PHY_BITS(baseB, CDPHY_RX_ANA_0,
				     RG_CSI0_DPHY_L2_CKSEL, 1);

			MTK_PHY_BITS(baseA, CDPHY_RX_ANA_0,
				     RG_CSI0_DPHY_L0_CKMODE_EN, 0);
			MTK_PHY_BITS(baseA, CDPHY_RX_ANA_0,
				     RG_CSI0_DPHY_L1_CKMODE_EN, 0);
			MTK_PHY_BITS(baseA, CDPHY_RX_ANA_0,
				     RG_CSI0_DPHY_L2_CKMODE_EN, 1);
			MTK_PHY_BITS(baseB, CDPHY_RX_ANA_0,
				     RG_CSI0_DPHY_L0_CKMODE_EN, 0);
			MTK_PHY_BITS(baseB, CDPHY_RX_ANA_0,
				     RG_CSI0_DPHY_L1_CKMODE_EN, 0);
			MTK_PHY_BITS(baseB, CDPHY_RX_ANA_0,
				     RG_CSI0_DPHY_L2_CKMODE_EN, 0);

			MTK_PHY_BITS(baseA, CDPHY_RX_ANA_5,
				     RG_CSI0_CDPHY_EQ_BW, 0x3);
			MTK_PHY_BITS(baseA, CDPHY_RX_ANA_5,
				     RG_CSI0_CDPHY_EQ_IS, 0x1);
			MTK_PHY_BITS(baseA, CDPHY_RX_ANA_5,
				     RG_CSI0_CDPHY_EQ_LATCH_EN, 0x1);
			MTK_PHY_BITS(baseA, CDPHY_RX_ANA_5,
				     RG_CSI0_CDPHY_EQ_DG0_EN, 0x1);
			MTK_PHY_BITS(baseA, CDPHY_RX_ANA_5,
				     RG_CSI0_CDPHY_EQ_DG1_EN, 0x1);
			MTK_PHY_BITS(baseA, CDPHY_RX_ANA_5,
				     RG_CSI0_CDPHY_EQ_SR0, 0x1);
			MTK_PHY_BITS(baseA, CDPHY_RX_ANA_5,
				     RG_CSI0_CDPHY_EQ_SR1, 0x0);

			MTK_PHY_BITS(baseB, CDPHY_RX_ANA_5,
				     RG_CSI0_CDPHY_EQ_BW, 0x3);
			MTK_PHY_BITS(baseB, CDPHY_RX_ANA_5,
				     RG_CSI0_CDPHY_EQ_IS, 0x1);
			MTK_PHY_BITS(baseB, CDPHY_RX_ANA_5,
				     RG_CSI0_CDPHY_EQ_LATCH_EN, 0x1);
			MTK_PHY_BITS(baseB, CDPHY_RX_ANA_5,
				     RG_CSI0_CDPHY_EQ_DG0_EN, 0x1);
			MTK_PHY_BITS(baseB, CDPHY_RX_ANA_5,
				     RG_CSI0_CDPHY_EQ_DG1_EN, 0x1);
			MTK_PHY_BITS(baseB, CDPHY_RX_ANA_5,
				     RG_CSI0_CDPHY_EQ_SR0, 0x1);
			MTK_PHY_BITS(baseB, CDPHY_RX_ANA_5,
				     RG_CSI0_CDPHY_EQ_SR1, 0x0);
		} else {
			MTK_PHY_BITS(base, CDPHY_RX_ANA_0,
				     RG_CSI0_CPHY_EN, 0);
			/* clear clk sel first */
			MTK_PHY_BITS(base, CDPHY_RX_ANA_0,
				     RG_CSI0_DPHY_L0_CKMODE_EN, 0);
			MTK_PHY_BITS(base, CDPHY_RX_ANA_0,
				     RG_CSI0_DPHY_L1_CKMODE_EN, 0);
			MTK_PHY_BITS(base, CDPHY_RX_ANA_0,
				     RG_CSI0_DPHY_L2_CKMODE_EN, 0);

			MTK_PHY_BITS(base, CDPHY_RX_ANA_0,
				     RG_CSI0_DPHY_L0_CKSEL, 0);
			MTK_PHY_BITS(base, CDPHY_RX_ANA_0,
				     RG_CSI0_DPHY_L1_CKSEL, 0);
			MTK_PHY_BITS(base, CDPHY_RX_ANA_0,
				     RG_CSI0_DPHY_L2_CKSEL, 0);

			MTK_PHY_BITS(base, CDPHY_RX_ANA_0,
				     RG_CSI0_DPHY_L0_CKMODE_EN, 0);
			MTK_PHY_BITS(base, CDPHY_RX_ANA_0,
				     RG_CSI0_DPHY_L1_CKMODE_EN, 1);
			MTK_PHY_BITS(base, CDPHY_RX_ANA_0,
				     RG_CSI0_DPHY_L2_CKMODE_EN, 0);

			MTK_PHY_BITS(base, CDPHY_RX_ANA_5,
				     RG_CSI0_CDPHY_EQ_BW, 0x3);
			MTK_PHY_BITS(base, CDPHY_RX_ANA_5,
				     RG_CSI0_CDPHY_EQ_IS, 0x1);
			MTK_PHY_BITS(base, CDPHY_RX_ANA_5,
				     RG_CSI0_CDPHY_EQ_LATCH_EN, 0x1);
			MTK_PHY_BITS(base, CDPHY_RX_ANA_5,
				     RG_CSI0_CDPHY_EQ_DG0_EN, 0x1);
			MTK_PHY_BITS(base, CDPHY_RX_ANA_5,
				     RG_CSI0_CDPHY_EQ_DG1_EN, 0x1);
			MTK_PHY_BITS(base, CDPHY_RX_ANA_5,
				     RG_CSI0_CDPHY_EQ_SR0, 0x1);
			MTK_PHY_BITS(base, CDPHY_RX_ANA_5,
				     RG_CSI0_CDPHY_EQ_SR1, 0x0);
		}
	} else { /* cphy */
		if (port_ctx->is_4d1c) {
			MTK_PHY_BITS(baseA, CDPHY_RX_ANA_0,
				     RG_CSI0_CPHY_EN, 1);
			MTK_PHY_BITS(baseB, CDPHY_RX_ANA_0,
				     RG_CSI0_CPHY_EN, 1);

			MTK_PHY_BITS(baseA, CDPHY_RX_ANA_5,
				     RG_CSI0_CDPHY_EQ_BW, 0x3);
			MTK_PHY_BITS(baseA, CDPHY_RX_ANA_5,
				     RG_CSI0_CDPHY_EQ_IS, 0x1);
			MTK_PHY_BITS(baseA, CDPHY_RX_ANA_5,
				     RG_CSI0_CDPHY_EQ_LATCH_EN, 0x1);
			MTK_PHY_BITS(baseA, CDPHY_RX_ANA_5,
				     RG_CSI0_CDPHY_EQ_DG0_EN, 0x1);
			MTK_PHY_BITS(baseA, CDPHY_RX_ANA_5,
				     RG_CSI0_CDPHY_EQ_DG1_EN, 0x0);
			MTK_PHY_BITS(baseA, CDPHY_RX_ANA_5,
				     RG_CSI0_CDPHY_EQ_SR0, 0x3);
			MTK_PHY_BITS(baseA, CDPHY_RX_ANA_5,
				     RG_CSI0_CDPHY_EQ_SR1, 0x0);

			MTK_PHY_BITS(baseA, CDPHY_RX_ANA_5,
				     RG_CSI0_CDPHY_EQ_BW, 0x3);
			MTK_PHY_BITS(baseA, CDPHY_RX_ANA_5,
				     RG_CSI0_CDPHY_EQ_IS, 0x1);
			MTK_PHY_BITS(baseA, CDPHY_RX_ANA_5,
				     RG_CSI0_CDPHY_EQ_LATCH_EN, 0x1);
			MTK_PHY_BITS(baseA, CDPHY_RX_ANA_5,
				     RG_CSI0_CDPHY_EQ_DG0_EN, 0x1);
			MTK_PHY_BITS(baseA, CDPHY_RX_ANA_5,
				     RG_CSI0_CDPHY_EQ_DG1_EN, 0x0);
			MTK_PHY_BITS(baseA, CDPHY_RX_ANA_5,
				     RG_CSI0_CDPHY_EQ_SR0, 0x3);
			MTK_PHY_BITS(baseA, CDPHY_RX_ANA_5,
				     RG_CSI0_CDPHY_EQ_SR1, 0x0);
		} else {
			MTK_PHY_BITS(base, CDPHY_RX_ANA_0,
				     RG_CSI0_CPHY_EN, 1);
			MTK_PHY_BITS(base, CDPHY_RX_ANA_5,
				     RG_CSI0_CDPHY_EQ_BW, 0x3);
			MTK_PHY_BITS(base, CDPHY_RX_ANA_5,
				     RG_CSI0_CDPHY_EQ_IS, 0x1);
			MTK_PHY_BITS(base, CDPHY_RX_ANA_5,
				     RG_CSI0_CDPHY_EQ_LATCH_EN, 0x1);
			MTK_PHY_BITS(base, CDPHY_RX_ANA_5,
				     RG_CSI0_CDPHY_EQ_DG0_EN, 0x1);
			MTK_PHY_BITS(base, CDPHY_RX_ANA_5,
				     RG_CSI0_CDPHY_EQ_DG1_EN, 0x0);
			MTK_PHY_BITS(base, CDPHY_RX_ANA_5,
				     RG_CSI0_CDPHY_EQ_SR0, 0x3);
			MTK_PHY_BITS(base, CDPHY_RX_ANA_5,
				     RG_CSI0_CDPHY_EQ_SR1, 0x0);
		}
	}

	/* phyA power on */
	if (port_ctx->is_4d1c) {
		csirx_phyA_power_on(port_ctx, port_ctx->port_a, 1);
		csirx_phyA_power_on(port_ctx, port_ctx->port_b, 1);
	} else {
		csirx_phyA_power_on(port_ctx, port_ctx->port, 1);
	}

	return 0;
}

static int csirx_dphy_setting(struct mtk_mipi_cdphy_port *port_ctx)
{
	void __iomem *base = port_ctx->reg_ana_dphy_top[port_ctx->port];

	if (port_ctx->is_4d1c) {
		MTK_PHY_BITS(base, DPHY_RX_LANE_SELECT, RG_DPHY_RX_LD3_SEL, 4);
		MTK_PHY_BITS(base, DPHY_RX_LANE_SELECT, RG_DPHY_RX_LD2_SEL, 0);
		MTK_PHY_BITS(base, DPHY_RX_LANE_SELECT, RG_DPHY_RX_LD1_SEL, 3);
		MTK_PHY_BITS(base, DPHY_RX_LANE_SELECT, RG_DPHY_RX_LD0_SEL, 1);
		MTK_PHY_BITS(base, DPHY_RX_LANE_SELECT, RG_DPHY_RX_LC0_SEL, 2);

		MTK_PHY_BITS(base, DPHY_RX_LANE_EN, DPHY_RX_LD0_EN, 1);
		MTK_PHY_BITS(base, DPHY_RX_LANE_EN, DPHY_RX_LD1_EN, 1);
		MTK_PHY_BITS(base, DPHY_RX_LANE_EN, DPHY_RX_LD2_EN, 1);
		MTK_PHY_BITS(base, DPHY_RX_LANE_EN, DPHY_RX_LD3_EN, 1);
		MTK_PHY_BITS(base, DPHY_RX_LANE_EN, DPHY_RX_LC0_EN, 1);
		MTK_PHY_BITS(base, DPHY_RX_LANE_EN, DPHY_RX_LC1_EN, 0);
	} else {
		MTK_PHY_BITS(base, DPHY_RX_LANE_SELECT, RG_DPHY_RX_LD3_SEL, 5);
		MTK_PHY_BITS(base, DPHY_RX_LANE_SELECT, RG_DPHY_RX_LD2_SEL, 3);
		MTK_PHY_BITS(base, DPHY_RX_LANE_SELECT, RG_DPHY_RX_LD1_SEL, 2);
		MTK_PHY_BITS(base, DPHY_RX_LANE_SELECT, RG_DPHY_RX_LD0_SEL, 0);
		MTK_PHY_BITS(base, DPHY_RX_LANE_SELECT, RG_DPHY_RX_LC1_SEL, 4);
		MTK_PHY_BITS(base, DPHY_RX_LANE_SELECT, RG_DPHY_RX_LC0_SEL, 1);

		MTK_PHY_BITS(base, DPHY_RX_LANE_EN, DPHY_RX_LD0_EN, 1);
		MTK_PHY_BITS(base, DPHY_RX_LANE_EN, DPHY_RX_LD1_EN, 1);
		MTK_PHY_BITS(base, DPHY_RX_LANE_EN, DPHY_RX_LD2_EN, 1);
		MTK_PHY_BITS(base, DPHY_RX_LANE_EN, DPHY_RX_LD3_EN, 1);
		MTK_PHY_BITS(base, DPHY_RX_LANE_EN, DPHY_RX_LC0_EN, 1);
		MTK_PHY_BITS(base, DPHY_RX_LANE_EN, DPHY_RX_LC1_EN, 1);
	}

	MTK_PHY_BITS(base, DPHY_RX_LANE_SELECT, DPHY_RX_CK_DATA_MUX_EN, 1);

	return 0;
}

static int csirx_cphy_setting(struct mtk_mipi_cdphy_port *port_ctx)
{
	void __iomem *base = port_ctx->reg_ana_cphy_top[port_ctx->port];

	switch (port_ctx->port) {
	case CSI_PORT_0:
	case CSI_PORT_0A:
		if (port_ctx->num_lanes == 3) {
			MTK_PHY_BITS(base, CPHY_RX_CTRL, CPHY_RX_TR0_LPRX_EN, 1);
			MTK_PHY_BITS(base, CPHY_RX_CTRL, CPHY_RX_TR1_LPRX_EN, 1);
			MTK_PHY_BITS(base, CPHY_RX_CTRL, CPHY_RX_TR2_LPRX_EN, 1);
			MTK_PHY_BITS(base, CPHY_RX_CTRL, CPHY_RX_TR3_LPRX_EN, 0);
		} else if (port_ctx->num_lanes == 2) {
			MTK_PHY_BITS(base, CPHY_RX_CTRL, CPHY_RX_TR0_LPRX_EN, 1);
			MTK_PHY_BITS(base, CPHY_RX_CTRL, CPHY_RX_TR1_LPRX_EN, 1);
		} else {
			MTK_PHY_BITS(base, CPHY_RX_CTRL, CPHY_RX_TR0_LPRX_EN, 1);
		}
		break;
	case CSI_PORT_0B:
		if (port_ctx->num_lanes == 1)
			MTK_PHY_BITS(base, CPHY_RX_CTRL, CPHY_RX_TR3_LPRX_EN, 1);
		break;
	default:
		dev_err(port_ctx->dev, "unsupported CSI port setting\n");
		break;
	}

	return 0;
}

static int csirx_phy_setting(struct mtk_mipi_cdphy_port *port_ctx)
{
	/* phyA */
	csirx_phyA_setting(port_ctx);

	if (!port_ctx->is_cphy)
		csirx_dphy_setting(port_ctx);
	else
		csirx_cphy_setting(port_ctx);

	return 0;
}

static int mtk_mipi_phy_power_on(struct phy *phy)
{
	struct mtk_mipi_cdphy_port *port_ctx = phy_get_drvdata(phy);

	csirx_phy_init(port_ctx);

	csirx_phy_setting(port_ctx);

	return 0;
}

static int mtk_mipi_phy_power_off(struct phy *phy)
{
	struct mtk_mipi_cdphy_port *port_ctx = phy_get_drvdata(phy);

	if (port_ctx->is_4d1c) {
		csirx_phyA_power_on(port_ctx, port_ctx->port_a, 0);
		csirx_phyA_power_on(port_ctx, port_ctx->port_b, 0);
	} else {
		csirx_phyA_power_on(port_ctx, port_ctx->port_a, 0);
	}
	return 0;
}

static struct phy *mtk_mipi_cdphy_xlate(struct device *dev,
					struct of_phandle_args *args)
{
	struct mtk_mipi_cdphy_port *priv = dev_get_drvdata(dev);

	/*
	 * If PHY is CD-PHY then we need to get the operating mode
	 * For now only D-PHY mode is supported
	 */
	if (priv->type == CDPHY) {
		if (args->args_count != 1) {
			dev_err(dev, "CDPHY: invalid number of arguments\n");
			return ERR_PTR(-EINVAL);
		}
		switch (args->args[0]) {
		case PHY_TYPE_DPHY:
			priv->mode = DPHY;
			priv->is_4d1c = 1;
			priv->is_cphy = 0;
			break;
		case PHY_TYPE_CPHY:
			priv->mode = CPHY;
			priv->is_4d1c = 0;
			priv->is_cphy = 1;
			break;
		default:
			dev_err(dev, "Unsupported PHY type: %i\n", args->args[0]);
			return ERR_PTR(-EINVAL);
		}
	} else if (priv->type == CPHY) {
		if (args->args_count != 1) {
			dev_err(dev, "CPHY: invalid number of arguments\n");
			return ERR_PTR(-EINVAL);
		}
		priv->mode = CPHY;
		priv->is_4d1c = 0;
		priv->is_cphy = 1;
	} else if (priv->type == DPHY) {
		if (args->args_count != 1) {
			dev_err(dev, "Dphy: invalid number of arguments\n");
			return ERR_PTR(-EINVAL);
		}
		priv->mode = DPHY;
		priv->is_4d1c = 1;
		priv->is_cphy = 0;
	} else {
		if (args->args_count) {
			dev_err(dev, "other: invalid number of arguments\n");
			return ERR_PTR(-EINVAL);
		}
		priv->mode = DPHY;
		priv->is_4d1c = 1;
		priv->is_cphy = 0;
	}

	return priv->phy;
}

static const struct phy_ops mtk_cdphy_ops = {
	.power_on	= mtk_mipi_phy_power_on,
	.power_off	= mtk_mipi_phy_power_off,
	.owner		= THIS_MODULE,
};

static int get_csi_port(struct device *dev, int *port)
{
	int i, ret;
	const char *name;

	ret = of_property_read_string(dev->of_node, "mediatek,csi-port", &name);
	if (ret)
		return ret;

	for (i = 0; i < CSI_PORT_MAX_NUM; i++) {
		if (!strcasecmp(name, csi_port_names[i])) {
			dev_info(dev, "mediatek,csi-port: %s\n", csi_port_names[i]);
			*port = i;
			return 0;
		}
	}

	return -1;
}

int mtk_mipi_init_port(struct mtk_mipi_cdphy_port *port_ctx, int port)
{
	u32 port_num;

	if (port >= CSI_PORT_0A)
		port_num = (port - CSI_PORT_0) >> 1;
	else
		port_num = port;

	port_ctx->port = CSI_PORT_0;
	port_ctx->port_num = port_num;
	port_ctx->port_a = CSI_PORT_0A;
	port_ctx->port_b = CSI_PORT_0B;
	port_ctx->is_4d1c = (port == port_num);

	dev_info(port_ctx->dev, "%s port %d, port_num %d, port_a %d, port_b %d, is_4d1c %d\n",
		 __func__,
		 port_ctx->port,
		 port_ctx->port_num,
		 port_ctx->port_a,
		 port_ctx->port_b,
		 port_ctx->is_4d1c);

	return 0;
}

static int mtk_mipi_cdphy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct phy_provider *phy_provider;
	struct mtk_mipi_cdphy_port *port_ctx;
	struct phy *phy;
	int ret;
	u32 phy_type;
	u32 port;

	dev_info(dev, "MIPI CDPHY | %s start\n", __func__);
	port_ctx = devm_kzalloc(dev, sizeof(*port_ctx), GFP_KERNEL);
	if (!port_ctx)
		return -ENOMEM;

	dev_set_drvdata(dev, port_ctx);

	port_ctx->dev = dev;

	port_ctx->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(port_ctx->base))
		return PTR_ERR(port_ctx->base);

	ret = of_property_read_u32(dev->of_node, "num-lanes", &port_ctx->num_lanes);
	if (ret) {
		dev_err(dev, "Failed to read num-lanes property: %i\n", ret);
		return ret;
	}

	ret = get_csi_port(dev, &port);
	if (ret) {
		dev_err(dev, "get csi port ret %d\n", ret);
		return ret;
	}

	dev_info(dev, "csi port: %d num_lanes: %d\n", port, port_ctx->num_lanes);

	mtk_mipi_init_port(port_ctx, port);

	/* mipi platform properties */
	port_ctx->cphy_settle_delay_dt = MTK_PHY_CPHY_SETTLE_DELAY_DT;
	port_ctx->dphy_settle_delay_dt = MTK_PHY_DPHY_SETTLE_DELAY_DT;
	port_ctx->settle_delay_ck = MTK_PHY_SETTLE_DELAY_CK;
	port_ctx->hs_trail_parameter = MTK_PHY_HS_TRAIL_PARAMETER;

	of_property_read_u32(dev->of_node, "cphy_settle_delay_dt",
			     &port_ctx->cphy_settle_delay_dt);
	of_property_read_u32(dev->of_node, "dphy_settle_delay_dt",
			     &port_ctx->dphy_settle_delay_dt);
	of_property_read_u32(dev->of_node, "settle_delay_ck",
			     &port_ctx->settle_delay_ck);
	of_property_read_u32(dev->of_node, "hs_trail_parameter",
			     &port_ctx->hs_trail_parameter);

	dev_info(dev,
		 "phyRx d_settlte/d_settle_ck/hs_trail/c_settle= 0x%x/0x%x/0x%x/0x%x\n",
		 port_ctx->dphy_settle_delay_dt,
		 port_ctx->settle_delay_ck,
		 port_ctx->hs_trail_parameter,
		 port_ctx->cphy_settle_delay_dt);

	/*
	 * phy-type is optional, if not present, PHY is considered to be CD-PHY
	 */
	if (device_property_present(dev, "phy-type")) {
		ret = of_property_read_u32(dev->of_node, "phy-type", &phy_type);
		if (ret) {
			dev_err(dev, "Failed to read phy-type property: %i\n", ret);
			return ret;
		}
		switch (phy_type) {
		case PHY_TYPE_DPHY:
			port_ctx->type = DPHY;
			dev_info(dev, "DTS CDPHY mode: DPHY\n");
			break;
		case PHY_TYPE_CPHY:
			port_ctx->type = CPHY;
			dev_info(dev, "DTS CDPHY mode: CPHY\n");
			break;
		default:
			dev_err(dev, "Unsupported PHY type: %i\n", phy_type);
			return -EINVAL;
		}
	} else {
		port_ctx->type = CDPHY;
		dev_info(dev, "DTS CDPHY is not specified, defaulting to CDPHY\n");
	}

	phy = devm_phy_create(dev, NULL, &mtk_cdphy_ops);
	if (IS_ERR(phy)) {
		dev_err(dev, "Failed to create PHY: %ld\n", PTR_ERR(phy));
		return PTR_ERR(phy);
	}

	port_ctx->phy = phy;
	phy_set_drvdata(phy, port_ctx);

	phy_provider = devm_of_phy_provider_register(dev, mtk_mipi_cdphy_xlate);
	if (IS_ERR(phy_provider)) {
		dev_err(dev, "Failed to register PHY provider: %ld\n",
			PTR_ERR(phy_provider));
		return PTR_ERR(phy_provider);
	}
	dev_info(dev, "MIPI CDPHY | %s success\n", __func__);

	return 0;
}

static const struct of_device_id mtk_mipi_cdphy_of_match[] = {
	{ .compatible = "mediatek,mt8189-csi-rx" },
	{ /* sentinel */},
};
MODULE_DEVICE_TABLE(of, mtk_mipi_cdphy_of_match);

static struct platform_driver mipi_cdphy_pdrv = {
	.probe = mtk_mipi_cdphy_probe,
	.driver	= {
		.name	= "mtk-mipi-csi-2-1",
		.of_match_table = mtk_mipi_cdphy_of_match,
	},
};
module_platform_driver(mipi_cdphy_pdrv);

MODULE_DESCRIPTION("MediaTek MIPI CSI CD-PHY v2.1 Driver");
MODULE_AUTHOR("Rongdong Wang <rongdong.wang@mediatek.com>");
MODULE_LICENSE("GPL");
