// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2023 NXP
 */

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#include <dt-bindings/phy/phy-imx8-pcie.h>

/* i.MX8Q HSIO registers */
#define IMX8QM_PHY_CTRL0_OFFSET			0x0
#define IMX8QM_PHYX2_CTRL0_APB_MASK		GENMASK(1, 0)
#define IMX8QM_PHY_CTRL0_APB_RSTN_0		BIT(0)
#define IMX8QM_PHY_CTRL0_APB_RSTN_1		BIT(1)
#define IMX8QM_PHY_MODE_MASK			GENMASK(20, 17)
#define IMX8QM_PHY_MODE_SATA			BIT(19)
#define IMX8QM_PHY_PIPE_RSTN_0_MASK		GENMASK(25, 24)
#define IMX8QM_PHY_PIPE_RSTN_1_MASK		GENMASK(27, 26)

#define IMX8QM_PHY_STTS0_OFFSET			0x4
#define IMX8QM_STTS0_LANE0_TX_PLL_LOCK		BIT(4)
#define IMX8QM_STTS0_LANE1_TX_PLL_LOCK		BIT(12)

#define IMX8QM_PCIE_CTRL1_OFFSET		0x4
#define IMX8QM_PCIE_CTRL2_OFFSET		0x8
#define IMX8QM_CTRL_BUTTON_RST_N		BIT(21)
#define IMX8QM_CTRL_PERST_N			BIT(22)
#define IMX8QM_CTRL_POWER_UP_RST_N		BIT(23)

#define IMX8QM_PCIE_STTS0_OFFSET		0xC
#define IMX8QM_CTRL_STTS0_PM_REQ_CORE_RST	BIT(19)

#define IMX8QM_SATA_CTRL0_OFFSET		0x0
#define IMX8QM_SATA_CTRL_EPCS_TXDEEMP		BIT(5)
#define IMX8QM_SATA_CTRL_EPCS_TXDEEMP_SEL	BIT(6)
#define IMX8QM_SATA_CTRL_EPCS_PHYRESET_N	BIT(7)
#define IMX8QM_SATA_CTRL_RESET_N		BIT(12)

#define IMX8QM_MISC_CTRL0_OFFSET		0x0
#define IMX8QM_MISC_IOB_RXENA			BIT(0)
#define IMX8QM_MISC_IOB_TXENA			BIT(1)
#define IMX8QM_MISC_IOB_A_0_TXOE		BIT(2)
#define IMX8QM_MISC_IOB_A_0_M1M0_MASK		GENMASK(4, 3)
#define IMX8QM_MISC_IOB_A_0_M1M0_2		BIT(4)
#define IMX8QM_MISC_PHYX1_EPCS_SEL		BIT(12)
#define IMX8QM_MISC_PCIE_AB_SELECT		BIT(13)
#define IMX8QM_MISC_CLKREQN_OUT_OVERRIDE_1	BIT(24)
#define IMX8QM_MISC_CLKREQN_OUT_OVERRIDE_0	BIT(25)

#define IMX8QM_PHY_REG03_RX_IMPED_RATIO		0x03
#define IMX8QM_PHY_REG09_TX_IMPED_RATIO		0x09
#define IMX8QM_PHY_REG10_TX_POST_CURSOR_RATIO	0x0a
#define IMX8QM_PHY_GEN1_TX_POST_CURSOR_RATIO	0x15
#define IMX8QM_PHY_REG22_TX_POST_CURSOR_RATIO	0x16
#define IMX8QM_PHY_GEN2_TX_POST_CURSOR_RATIO	0x00
#define IMX8QM_PHY_REG24_TX_AMP_RATIO_MARGIN0	0x18
#define IMX8QM_PHY_TX_AMP_RATIO_MARGIN0		0x64
#define IMX8QM_PHY_REG25_TX_AMP_RATIO_MARGIN1	0x19
#define IMX8QM_PHY_TX_AMP_RATIO_MARGIN1		0x70
#define IMX8QM_PHY_REG26_TX_AMP_RATIO_MARGIN2	0x1a
#define IMX8QM_PHY_TX_AMP_RATIO_MARGIN2		0x69
#define IMX8QM_PHY_REG48_PMA_STATUS		0x30
#define IMX8QM_PHY_REG48_PMA_RDY		BIT(7)
#define IMX8QM_PHY_REG128_UPDATE_SETTING	0x80
#define IMX8QM_PHY_UPDATE_SETTING		0x01
#define IMX8QM_PHY_IMPED_RATIO_100OHM		0x5d

#define IMX8QM_PHYX2_LANE1_BASE_ADDR		0x5F190000
#define IMX8QM_PHYX1_LANE0_BASE_ADDR		0x5F1A0000

/* Parameters for the waiting for PCIe PHY PLL to lock */
#define PHY_INIT_WAIT_USLEEP_MAX	10
#define PHY_INIT_WAIT_TIMEOUT		(1000 * PHY_INIT_WAIT_USLEEP_MAX)

enum imx8q_pcie_phy_mode {
	IMX8Q_PHY_PCIE_MODE,
	IMX8Q_PHY_SATA_MODE,
};

struct imx8q_pcie_phy {
	void __iomem		*base;
	struct clk_bulk_data	*clks;
	struct device		*dev;
	struct phy		*phy;
	struct regmap		*ctrl_gpr;
	struct regmap		*misc_gpr;
	struct regmap		*phy_gpr;
	u32			clk_cnt;
	u32			hsio_cfg;
	u32			lane_id;
	u32			imped_ratio;
	u32			refclk_pad_mode;
	enum			imx8q_pcie_phy_mode mode;
};

static int imx8q_pcie_phy_power_on(struct phy *phy)
{
	int ret;
	u32 val, cond, pad_mode;
	struct imx8q_pcie_phy *imx8_phy = phy_get_drvdata(phy);

	pad_mode = imx8_phy->refclk_pad_mode;
	if (imx8_phy->mode == IMX8Q_PHY_SATA_MODE) {
		/* SATA mode PHY */
		regmap_update_bits(imx8_phy->phy_gpr,
				IMX8QM_PHY_CTRL0_OFFSET,
				IMX8QM_PHY_MODE_MASK,
				IMX8QM_PHY_MODE_SATA);
		/*
		 * It is possible, for PCIe and SATA are sharing
		 * the same clock source, HPLL or external oscillator.
		 * When PCIe is in low power modes (L1.X or L2 etc),
		 * the clock source can be turned off. In this case,
		 * if this clock source is required to be toggling by
		 * SATA, then SATA functions will be abnormal.
		 * Set the override here to avoid it.
		 */
		regmap_update_bits(imx8_phy->misc_gpr,
				IMX8QM_MISC_CTRL0_OFFSET,
				IMX8QM_MISC_CLKREQN_OUT_OVERRIDE_1 |
				IMX8QM_MISC_CLKREQN_OUT_OVERRIDE_0,
				IMX8QM_MISC_CLKREQN_OUT_OVERRIDE_1 |
				IMX8QM_MISC_CLKREQN_OUT_OVERRIDE_0);
	} else {
		/* PCIe mode PHY */
		regmap_update_bits(imx8_phy->ctrl_gpr,
				IMX8QM_PCIE_CTRL2_OFFSET,
				IMX8QM_CTRL_BUTTON_RST_N,
				0);
		regmap_update_bits(imx8_phy->ctrl_gpr,
				IMX8QM_PCIE_CTRL2_OFFSET,
				IMX8QM_CTRL_PERST_N,
				0);
		regmap_update_bits(imx8_phy->ctrl_gpr,
				IMX8QM_PCIE_CTRL2_OFFSET,
				IMX8QM_CTRL_POWER_UP_RST_N,
				0);
		mdelay(1);
		regmap_update_bits(imx8_phy->ctrl_gpr,
				IMX8QM_PCIE_CTRL2_OFFSET,
				IMX8QM_CTRL_BUTTON_RST_N,
				IMX8QM_CTRL_BUTTON_RST_N);
		regmap_update_bits(imx8_phy->ctrl_gpr,
				IMX8QM_PCIE_CTRL2_OFFSET,
				IMX8QM_CTRL_PERST_N,
				IMX8QM_CTRL_PERST_N);
		regmap_update_bits(imx8_phy->ctrl_gpr,
				IMX8QM_PCIE_CTRL2_OFFSET,
				IMX8QM_CTRL_POWER_UP_RST_N,
				IMX8QM_CTRL_POWER_UP_RST_N);
	}

	if (imx8_phy->hsio_cfg == PCIEAX2SATA) {
		/*
		 * bit 0 rx ena 1.
		 * bit12 PHY_X1_EPCS_SEL 1.
		 * bit13 phy_ab_select 0.
		 */
		if (imx8_phy->lane_id == 0) {
			regmap_update_bits(imx8_phy->phy_gpr,
				IMX8QM_PHY_CTRL0_OFFSET,
				IMX8QM_PHYX2_CTRL0_APB_MASK,
				IMX8QM_PHY_CTRL0_APB_RSTN_0 |
				IMX8QM_PHY_CTRL0_APB_RSTN_1);
			regmap_update_bits(imx8_phy->phy_gpr,
				IMX8QM_PHY_CTRL0_OFFSET,
				IMX8QM_PHY_PIPE_RSTN_0_MASK |
				IMX8QM_PHY_PIPE_RSTN_1_MASK,
				IMX8QM_PHY_PIPE_RSTN_0_MASK |
				IMX8QM_PHY_PIPE_RSTN_1_MASK);
		} else if (imx8_phy->lane_id == 2) {
			regmap_update_bits(imx8_phy->phy_gpr,
				IMX8QM_PHY_CTRL0_OFFSET,
				IMX8QM_PHY_CTRL0_APB_RSTN_0,
				IMX8QM_PHY_CTRL0_APB_RSTN_0);
		}

		regmap_update_bits(imx8_phy->misc_gpr,
			IMX8QM_MISC_CTRL0_OFFSET,
			IMX8QM_MISC_PHYX1_EPCS_SEL,
			IMX8QM_MISC_PHYX1_EPCS_SEL);
		regmap_update_bits(imx8_phy->misc_gpr,
			IMX8QM_MISC_CTRL0_OFFSET,
			IMX8QM_MISC_PCIE_AB_SELECT,
			0);
	} else if (imx8_phy->hsio_cfg == PCIEAX1PCIEBX1SATA) {
		if (imx8_phy->lane_id == 0) {
			regmap_update_bits(imx8_phy->phy_gpr,
				IMX8QM_PHY_CTRL0_OFFSET,
				IMX8QM_PHY_CTRL0_APB_RSTN_0,
				IMX8QM_PHY_CTRL0_APB_RSTN_0);
			regmap_update_bits(imx8_phy->phy_gpr,
				IMX8QM_PHY_CTRL0_OFFSET,
				IMX8QM_PHY_PIPE_RSTN_0_MASK,
				IMX8QM_PHY_PIPE_RSTN_0_MASK);
		} else if (imx8_phy->lane_id == 1) {
			regmap_update_bits(imx8_phy->phy_gpr,
				IMX8QM_PHY_CTRL0_OFFSET,
				IMX8QM_PHY_CTRL0_APB_RSTN_1,
				IMX8QM_PHY_CTRL0_APB_RSTN_1);
			regmap_update_bits(imx8_phy->phy_gpr,
				IMX8QM_PHY_CTRL0_OFFSET,
				IMX8QM_PHY_PIPE_RSTN_1_MASK,
				IMX8QM_PHY_PIPE_RSTN_1_MASK);
		} else if (imx8_phy->lane_id == 2) {
			regmap_update_bits(imx8_phy->phy_gpr,
				IMX8QM_PHY_CTRL0_OFFSET,
				IMX8QM_PHY_CTRL0_APB_RSTN_0,
				IMX8QM_PHY_CTRL0_APB_RSTN_0);
		}

		regmap_update_bits(imx8_phy->misc_gpr,
			IMX8QM_MISC_CTRL0_OFFSET,
			IMX8QM_MISC_PCIE_AB_SELECT,
			IMX8QM_MISC_PCIE_AB_SELECT);
		regmap_update_bits(imx8_phy->misc_gpr,
			IMX8QM_MISC_CTRL0_OFFSET,
			IMX8QM_MISC_PHYX1_EPCS_SEL,
			IMX8QM_MISC_PHYX1_EPCS_SEL);
	} else if (imx8_phy->hsio_cfg == PCIEAX2PCIEBX1) {
		/*
		 * bit 0 rx ena 1.
		 * bit12 PHY_X1_EPCS_SEL 0.
		 * bit13 phy_ab_select 1.
		 */
		if (imx8_phy->lane_id == 0) {
			regmap_update_bits(imx8_phy->phy_gpr,
				IMX8QM_PHY_CTRL0_OFFSET,
				IMX8QM_PHYX2_CTRL0_APB_MASK,
				IMX8QM_PHY_CTRL0_APB_RSTN_0 |
				IMX8QM_PHY_CTRL0_APB_RSTN_1);
			regmap_update_bits(imx8_phy->phy_gpr,
				IMX8QM_PHY_CTRL0_OFFSET,
				IMX8QM_PHY_PIPE_RSTN_0_MASK |
				IMX8QM_PHY_PIPE_RSTN_1_MASK,
				IMX8QM_PHY_PIPE_RSTN_0_MASK |
				IMX8QM_PHY_PIPE_RSTN_1_MASK);
		} else if (imx8_phy->lane_id == 2) {
			regmap_update_bits(imx8_phy->phy_gpr,
				IMX8QM_PHY_CTRL0_OFFSET,
				IMX8QM_PHY_CTRL0_APB_RSTN_0,
				IMX8QM_PHY_CTRL0_APB_RSTN_0);
			regmap_update_bits(imx8_phy->phy_gpr,
				IMX8QM_PHY_CTRL0_OFFSET,
				IMX8QM_PHY_PIPE_RSTN_0_MASK,
				IMX8QM_PHY_PIPE_RSTN_0_MASK);
		}

		regmap_update_bits(imx8_phy->misc_gpr,
			IMX8QM_MISC_CTRL0_OFFSET,
			IMX8QM_MISC_PHYX1_EPCS_SEL,
			0);
		regmap_update_bits(imx8_phy->misc_gpr,
			IMX8QM_MISC_CTRL0_OFFSET,
			IMX8QM_MISC_PCIE_AB_SELECT,
			IMX8QM_MISC_PCIE_AB_SELECT);
	}

	if (pad_mode == IMX8_PCIE_REFCLK_PAD_INPUT ||
	    pad_mode == IMX8_PCIE_REFCLK_PAD_UNUSED) {
		/* Configure the PAD as input */
		regmap_update_bits(imx8_phy->misc_gpr,
			IMX8QM_MISC_CTRL0_OFFSET,
			IMX8QM_MISC_IOB_RXENA,
			IMX8QM_MISC_IOB_RXENA);
		regmap_update_bits(imx8_phy->misc_gpr,
			IMX8QM_MISC_CTRL0_OFFSET,
			IMX8QM_MISC_IOB_TXENA,
			0);
	} else {
		/* Configure the PHY to output the refclock via PAD */
		regmap_update_bits(imx8_phy->misc_gpr,
			IMX8QM_MISC_CTRL0_OFFSET,
			IMX8QM_MISC_IOB_RXENA,
			0);
		regmap_update_bits(imx8_phy->misc_gpr,
			IMX8QM_MISC_CTRL0_OFFSET,
			IMX8QM_MISC_IOB_TXENA,
			IMX8QM_MISC_IOB_TXENA);
		regmap_update_bits(imx8_phy->misc_gpr,
			IMX8QM_MISC_CTRL0_OFFSET,
			IMX8QM_MISC_IOB_A_0_TXOE |
			IMX8QM_MISC_IOB_A_0_M1M0_MASK,
			IMX8QM_MISC_IOB_A_0_TXOE |
			IMX8QM_MISC_IOB_A_0_M1M0_2);
	}

	if (imx8_phy->mode == IMX8Q_PHY_SATA_MODE) {
		regmap_update_bits(imx8_phy->ctrl_gpr,
				IMX8QM_SATA_CTRL0_OFFSET,
				IMX8QM_SATA_CTRL_EPCS_TXDEEMP,
				IMX8QM_SATA_CTRL_EPCS_TXDEEMP);
		regmap_update_bits(imx8_phy->ctrl_gpr,
				IMX8QM_SATA_CTRL0_OFFSET,
				IMX8QM_SATA_CTRL_EPCS_TXDEEMP_SEL,
				IMX8QM_SATA_CTRL_EPCS_TXDEEMP_SEL);

		/* clear PHY RST, then set it */
		regmap_update_bits(imx8_phy->ctrl_gpr,
				IMX8QM_SATA_CTRL0_OFFSET,
				IMX8QM_SATA_CTRL_EPCS_PHYRESET_N,
				0);

		regmap_update_bits(imx8_phy->ctrl_gpr,
				IMX8QM_SATA_CTRL0_OFFSET,
				IMX8QM_SATA_CTRL_EPCS_PHYRESET_N,
				IMX8QM_SATA_CTRL_EPCS_PHYRESET_N);

		/* CTRL RST: SET -> delay 1 us -> CLEAR -> SET */
		regmap_update_bits(imx8_phy->ctrl_gpr,
				IMX8QM_SATA_CTRL0_OFFSET,
				IMX8QM_SATA_CTRL_RESET_N,
				IMX8QM_SATA_CTRL_RESET_N);
		udelay(1);
		regmap_update_bits(imx8_phy->ctrl_gpr,
				IMX8QM_SATA_CTRL0_OFFSET,
				IMX8QM_SATA_CTRL_RESET_N,
				0);
		regmap_update_bits(imx8_phy->ctrl_gpr,
				IMX8QM_SATA_CTRL0_OFFSET,
				IMX8QM_SATA_CTRL_RESET_N,
				IMX8QM_SATA_CTRL_RESET_N);

		/* Configure PHY parameters */
		writeb(imx8_phy->imped_ratio,
			imx8_phy->base + IMX8QM_PHY_REG03_RX_IMPED_RATIO);
		writeb(imx8_phy->imped_ratio,
			imx8_phy->base + IMX8QM_PHY_REG09_TX_IMPED_RATIO);

		/* Configure the tx_amplitude to pass the tests. */
		writeb(IMX8QM_PHY_TX_AMP_RATIO_MARGIN0,
			imx8_phy->base + IMX8QM_PHY_REG24_TX_AMP_RATIO_MARGIN0);
		writeb(IMX8QM_PHY_TX_AMP_RATIO_MARGIN1,
			imx8_phy->base + IMX8QM_PHY_REG25_TX_AMP_RATIO_MARGIN1);
		writeb(IMX8QM_PHY_TX_AMP_RATIO_MARGIN2,
			imx8_phy->base + IMX8QM_PHY_REG26_TX_AMP_RATIO_MARGIN2);

		/* Adjust the OOB COMINIT/COMWAKE to pass the tests. */
		writeb(IMX8QM_PHY_GEN1_TX_POST_CURSOR_RATIO,
			imx8_phy->base + IMX8QM_PHY_REG10_TX_POST_CURSOR_RATIO);
		writeb(IMX8QM_PHY_GEN2_TX_POST_CURSOR_RATIO,
			imx8_phy->base + IMX8QM_PHY_REG22_TX_POST_CURSOR_RATIO);

		writeb(IMX8QM_PHY_UPDATE_SETTING,
			imx8_phy->base + IMX8QM_PHY_REG128_UPDATE_SETTING);

		usleep_range(50, 100);
	} else {
		/* Toggle the phy apb_pclk to make sure clear the PM_REQ_CORE_RST bit */
		clk_disable_unprepare(imx8_phy->clks[0].clk);
		mdelay(1);
		ret = clk_prepare_enable(imx8_phy->clks[0].clk);
		if (ret) {
			dev_err(imx8_phy->dev, "unable to enable phy apb_pclk\n");
			return ret;
		}

		/* Bit19 PM_REQ_CORE_RST of pcie_stts0 should be cleared. */
		ret = regmap_read_poll_timeout(imx8_phy->ctrl_gpr,
				IMX8QM_PCIE_STTS0_OFFSET, val,
				(val & IMX8QM_CTRL_STTS0_PM_REQ_CORE_RST) == 0,
				PHY_INIT_WAIT_USLEEP_MAX,
				PHY_INIT_WAIT_TIMEOUT);
		if (ret) {
			dev_err(imx8_phy->dev, "PM_REQ_CORE_RST is set\n");
			return ret;
		}
	}

	/* Polling to check the PHY is ready or not. */
	if (imx8_phy->lane_id == 1)
		cond = IMX8QM_STTS0_LANE1_TX_PLL_LOCK;
	else
		cond = IMX8QM_STTS0_LANE0_TX_PLL_LOCK;

	ret = regmap_read_poll_timeout(imx8_phy->phy_gpr,
			IMX8QM_PHY_STTS0_OFFSET, val, ((val & cond) == cond),
			PHY_INIT_WAIT_USLEEP_MAX,
			PHY_INIT_WAIT_TIMEOUT);
	if (ret)
		dev_err(imx8_phy->dev, "PHY PLL lock timeout\n");
	else
		dev_info(imx8_phy->dev, "PHY PLL is locked\n");

	if (imx8_phy->mode == IMX8Q_PHY_SATA_MODE) {
		cond = IMX8QM_PHY_REG48_PMA_RDY;
		ret = read_poll_timeout(readb, val, ((val & cond) == cond),
				PHY_INIT_WAIT_USLEEP_MAX,
				PHY_INIT_WAIT_TIMEOUT, false,
				imx8_phy->base + IMX8QM_PHY_REG48_PMA_STATUS);
		if (ret)
			dev_err(imx8_phy->dev, "PHY calibration is timeout\n");
		else
			dev_info(imx8_phy->dev, "PHY calibration is done\n");
	}

	return ret;
}

static int imx8q_pcie_phy_init(struct phy *phy)
{
	int ret;
	struct imx8q_pcie_phy *imx8_phy = phy_get_drvdata(phy);

	ret = devm_clk_bulk_get_all(imx8_phy->dev, &imx8_phy->clks);
	if (ret < 0)
		return ret;

	imx8_phy->clk_cnt = ret;

	ret = clk_bulk_prepare_enable(imx8_phy->clk_cnt, imx8_phy->clks);

	/* allow the clocks to stabilize */
	usleep_range(200, 500);
	return ret;
}

static int imx8q_pcie_phy_exit(struct phy *phy)
{
	struct imx8q_pcie_phy *imx8_phy = phy_get_drvdata(phy);

	clk_bulk_disable_unprepare(imx8_phy->clk_cnt, imx8_phy->clks);

	return 0;
}

static const struct phy_ops imx8q_pcie_phy_ops = {
	.init		= imx8q_pcie_phy_init,
	.exit		= imx8q_pcie_phy_exit,
	.power_on	= imx8q_pcie_phy_power_on,
	.owner		= THIS_MODULE,
};

static const struct of_device_id imx8q_pcie_phy_of_match[] = {
	{.compatible = "fsl,imx8qm-pcie-phy", },
	{ },
};

MODULE_DEVICE_TABLE(of, imx8q_pcie_phy_of_match);

static int imx8q_pcie_phy_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct imx8q_pcie_phy *imx8_phy;
	const struct of_device_id *of_id;
	struct phy_provider *phy_provider;
	struct resource *res;

	of_id = of_match_device(imx8q_pcie_phy_of_match, dev);
	if (!of_id)
		return -EINVAL;

	imx8_phy = devm_kzalloc(dev, sizeof(*imx8_phy), GFP_KERNEL);
	if (!imx8_phy)
		return -ENOMEM;

	imx8_phy->dev = dev;

	/* Get PHY refclk pad mode */
	of_property_read_u32(np, "fsl,refclk-pad-mode",
			     &imx8_phy->refclk_pad_mode);

	/* Get HSIO configuration mode */
	if (of_property_read_u32(np, "hsio-cfg", &imx8_phy->hsio_cfg))
		imx8_phy->hsio_cfg = 0;

	if (of_property_read_u32(np, "fsl,phy-imp", &imx8_phy->imped_ratio)) {
		dev_info(dev, "phy impedance ratio is not specified.\n");
		imx8_phy->imped_ratio = IMX8QM_PHY_IMPED_RATIO_100OHM;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	imx8_phy->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(imx8_phy->base))
		return PTR_ERR(imx8_phy->base);

	if (res->start == IMX8QM_PHYX2_LANE1_BASE_ADDR)
		imx8_phy->lane_id = 1;
	else if (res->start == IMX8QM_PHYX1_LANE0_BASE_ADDR)
		imx8_phy->lane_id = 2;

	if (unlikely((imx8_phy->lane_id == 2) &&
		   (imx8_phy->hsio_cfg != PCIEAX2PCIEBX1)))
		imx8_phy->mode = IMX8Q_PHY_SATA_MODE;
	else
		imx8_phy->mode = IMX8Q_PHY_PCIE_MODE;

	/* Grab GPR config register range */
	imx8_phy->ctrl_gpr = syscon_regmap_lookup_by_phandle(np, "ctrl-csr");
	if (IS_ERR(imx8_phy->ctrl_gpr)) {
		dev_err(dev, "unable to find hsio ctrl registers\n");
		return PTR_ERR(imx8_phy->ctrl_gpr);
	}

	imx8_phy->misc_gpr = syscon_regmap_lookup_by_phandle(np, "misc-csr");
	if (IS_ERR(imx8_phy->misc_gpr)) {
		dev_err(dev, "unable to find hsio misc registers\n");
		return PTR_ERR(imx8_phy->misc_gpr);
	}

	imx8_phy->phy_gpr = syscon_regmap_lookup_by_phandle(np, "phy-csr");
	if (IS_ERR(imx8_phy->phy_gpr)) {
		dev_err(dev, "unable to find hsio phy registers\n");
		return PTR_ERR(imx8_phy->phy_gpr);
	}

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);

	imx8_phy->phy = devm_phy_create(dev, NULL, &imx8q_pcie_phy_ops);
	if (IS_ERR(imx8_phy->phy)) {
		ret = PTR_ERR(imx8_phy->phy);
		goto err_exit;
	}

	phy_set_drvdata(imx8_phy->phy, imx8_phy);

	phy_provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);

	ret = PTR_ERR_OR_ZERO(phy_provider);
	if (ret < 0)
		goto err_exit;

	return ret;

err_exit:
	pm_runtime_disable(dev);
	pm_runtime_set_suspended(dev);

	return ret;
}

static struct platform_driver imx8q_pcie_phy_driver = {
	.probe	= imx8q_pcie_phy_probe,
	.driver = {
		.name	= "imx8q-pcie-phy",
		.of_match_table	= imx8q_pcie_phy_of_match,
	}
};
module_platform_driver(imx8q_pcie_phy_driver);

MODULE_DESCRIPTION("FSL IMX8Q PCIE PHY driver");
MODULE_LICENSE("GPL v2");
