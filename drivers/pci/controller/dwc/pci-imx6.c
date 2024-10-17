// SPDX-License-Identifier: GPL-2.0
/*
 * PCIe host controller driver for Freescale i.MX6 SoCs
 *
 * Copyright (C) 2013 Kosagi
 *		https://www.kosagi.com
 *
 * Author: Sean Cross <xobs@kosagi.com>
 */

#include <dt-bindings/phy/phy-imx8-pcie.h>
#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/mfd/syscon/imx6q-iomuxc-gpr.h>
#include <linux/mfd/syscon/imx7-iomuxc-gpr.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/resource.h>
#include <linux/signal.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/reset.h>
#include <linux/phy/phy.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>

#include "pcie-designware.h"

#define IMX95_PCIE2_BASE_ADDR			0x4c380000
#define IMX95_PCIE_PHY_GEN_CTRL			0x0
#define IMX95_PCIE_REF_USE_PAD			BIT(17)

#define IMX95_PCIE_PHY_MPLLA_CTRL		0x10
#define IMX95_PCIE_PHY_MPLL_STATE		BIT(30)

#define IMX95_PCIE_SS_RW_REG_0			0xf0
#define IMX95_PCIE_REF_CLKEN			BIT(23)
#define IMX95_PCIE_PHY_CR_PARA_SEL		BIT(9)

#define IMX95_PE0_LUT_ACSCTRL			0x1008
#define IMX95_PEO_LUT_RWA			BIT(16)
#define IMX95_PE0_LUT_ENLOC			GENMASK(4, 0)

#define IMX95_PE0_LUT_DATA1			0x100c
#define IMX95_PE0_LUT_VLD			BIT(31)
#define IMX95_PE0_LUT_DAC_ID			GENMASK(10, 8)
#define IMX95_PE0_LUT_STREAM_ID			GENMASK(5, 0)

#define IMX95_PE0_LUT_DATA2			0x1010
#define IMX95_PE0_LUT_REQID			GENMASK(31, 16)
#define IMX95_PE0_LUT_MASK			GENMASK(15, 0)

#define IMX95_PE0_GEN_CTRL_1			0x1050
#define IMX95_PCIE_DEVICE_TYPE			GENMASK(3, 0)

#define IMX95_PE0_GEN_CTRL_3			0x1058
#define IMX95_PCIE_LTSSM_EN			BIT(0)

#define IMX95_PE0_PM_CTRL			0x1060
#define IMX95_PCIE_PME_PF_INDEX			GENMASK(4, 0)
#define IMX95_PCIE_PM_PME_REQ			BIT(16)
#define IMX95_PCIE_PM_READY_ENTR_L23		BIT(19)

#define IMX95_PE0_PM_STS			0x1064
#define IMX95_PCIE_PM_LINKST_IN_L2		BIT(14)

#define IMX95_SID_MASK				GENMASK(5, 0)
#define IMX95_MAX_LUT				32

#define IMX8MQ_GPR_PCIE_REF_USE_PAD		BIT(9)
#define IMX8MQ_GPR_PCIE_CLK_REQ_OVERRIDE_EN	BIT(10)
#define IMX8MQ_GPR_PCIE_CLK_REQ_OVERRIDE	BIT(11)
#define IMX8MQ_GPR_PCIE_VREG_BYPASS		BIT(12)
#define IMX8MQ_GPR12_PCIE2_CTRL_DEVICE_TYPE	GENMASK(11, 8)
#define IMX8MQ_PCIE2_BASE_ADDR			0x33c00000
#define IMX8_HSIO_PCIEB_BASE_ADDR		0x5f010000

#define to_imx6_pcie(x)	dev_get_drvdata((x)->dev)

enum imx6_pcie_variants {
	IMX6Q,
	IMX6Q_EP,
	IMX6SX,
	IMX6SX_EP,
	IMX6QP,
	IMX6QP_EP,
	IMX7D,
	IMX7D_EP,
	IMX8MQ,
	IMX8MM,
	IMX8MP,
	IMX8MQ_EP,
	IMX8MM_EP,
	IMX8MP_EP,
	IMX8QM,
	IMX8QM_EP,
	IMX8QXP,
	IMX8QXP_EP,
	IMX95,
	IMX95_EP,
};

#define IMX6_PCIE_FLAG_IMX6_PHY			BIT(0)
#define IMX6_PCIE_FLAG_IMX6_SPEED_CHANGE	BIT(1)
#define IMX6_PCIE_FLAG_SUPPORTS_SUSPEND		BIT(2)
#define IMX6_PCIE_FLAG_IMX6_CPU_ADDR_FIXUP	BIT(3)
#define IMX6_PCIE_FLAG_SUPPORT_64BIT		BIT(8)

#define imx6_check_flag(pci, val)	(pci->drvdata->flags & val)

struct imx6_pcie_drvdata {
	enum imx6_pcie_variants variant;
	enum dw_pcie_device_mode mode;
	u32 flags;
	int dbi_length;
	const char *gpr;
};

struct imx6_pcie {
	struct dw_pcie		*pci;
	int			reset_gpio;
	int			host_wake_irq;
	bool			gpio_active_high;
	bool			link_is_up;
	struct clk		*pcie_bus;
	struct clk		*pcie_phy;
	struct clk		*pcie_inbound_axi;
	struct clk		*pcie;
	struct clk		*pcie_aux;
	struct clk		*pcie_ref;
	struct regmap		*iomuxc_gpr;
	u16			msi_ctrl;
	u32			controller_id;
	struct reset_control	*pciephy_reset;
	struct reset_control	*apps_reset;
	struct reset_control	*turnoff_reset;
	u32			tx_deemph_gen1;
	u32			tx_deemph_gen2_3p5db;
	u32			tx_deemph_gen2_6db;
	u32			tx_swing_full;
	u32			tx_swing_low;
	u32			hsio_cfg;
	u32			local_addr;
	u32			refclk_pad_mode;
	struct regulator	*vpcie;
	struct regulator	*vph;
	void __iomem		*phy_base;

	/* power domain for pcie */
	struct device		*pd_pcie;
	/* power domain for pcie phy */
	struct device		*pd_pcie_phy;
	struct phy		*phy;
	const struct imx6_pcie_drvdata *drvdata;
};

/* Parameters for the waiting for PCIe PHY PLL to lock on i.MX7 */
#define PHY_PLL_LOCK_WAIT_USLEEP_MAX	200
#define PHY_PLL_LOCK_WAIT_TIMEOUT	(2000 * PHY_PLL_LOCK_WAIT_USLEEP_MAX)

/* PCIe Port Logic registers (memory-mapped) */
#define PL_OFFSET 0x700

#define PCIE_PHY_CTRL (PL_OFFSET + 0x114)
#define PCIE_PHY_CTRL_DATA(x)		FIELD_PREP(GENMASK(15, 0), (x))
#define PCIE_PHY_CTRL_CAP_ADR		BIT(16)
#define PCIE_PHY_CTRL_CAP_DAT		BIT(17)
#define PCIE_PHY_CTRL_WR		BIT(18)
#define PCIE_PHY_CTRL_RD		BIT(19)

#define PCIE_PHY_STAT (PL_OFFSET + 0x110)
#define PCIE_PHY_STAT_ACK		BIT(16)

/* PHY registers (not memory-mapped) */
#define PCIE_PHY_ATEOVRD			0x10
#define  PCIE_PHY_ATEOVRD_EN			BIT(2)
#define  PCIE_PHY_ATEOVRD_REF_CLKDIV_SHIFT	0
#define  PCIE_PHY_ATEOVRD_REF_CLKDIV_MASK	0x1

#define PCIE_PHY_MPLL_OVRD_IN_LO		0x11
#define  PCIE_PHY_MPLL_MULTIPLIER_SHIFT		2
#define  PCIE_PHY_MPLL_MULTIPLIER_MASK		0x7f
#define  PCIE_PHY_MPLL_MULTIPLIER_OVRD		BIT(9)

#define PCIE_PHY_RX_ASIC_OUT 0x100D
#define PCIE_PHY_RX_ASIC_OUT_VALID	(1 << 0)

/* iMX7 PCIe PHY registers */
#define PCIE_PHY_CMN_REG4		0x14
/* These are probably the bits that *aren't* DCC_FB_EN */
#define PCIE_PHY_CMN_REG4_DCC_FB_EN	0x29

#define PCIE_PHY_CMN_REG15	        0x54
#define PCIE_PHY_CMN_REG15_DLY_4	BIT(2)
#define PCIE_PHY_CMN_REG15_PLL_PD	BIT(5)
#define PCIE_PHY_CMN_REG15_OVRD_PLL_PD	BIT(7)

#define PCIE_PHY_CMN_REG24		0x90
#define PCIE_PHY_CMN_REG24_RX_EQ	BIT(6)
#define PCIE_PHY_CMN_REG24_RX_EQ_SEL	BIT(3)

#define PCIE_PHY_CMN_REG26		0x98
#define PCIE_PHY_CMN_REG26_ATT_MODE	0xBC

#define PHY_RX_OVRD_IN_LO 0x1005
#define PHY_RX_OVRD_IN_LO_RX_DATA_EN		BIT(5)
#define PHY_RX_OVRD_IN_LO_RX_PLL_EN		BIT(3)

/* iMX8 HSIO registers */
#define IMX8QM_PCIE_CTRL0_OFFSET		0x0
#define IMX8QM_PCIE_CTRL0_TYPE_MASK		GENMASK(27, 24)
#define IMX8QM_PCIE_CTRL2_OFFSET		0x8
#define IMX8QM_PCIE_STTS0_OFFSET		0xC
#define CTRL2_LTSSM_ENABLE			BIT(4)
#define CTRL2_READY_ENTR_L23			BIT(5)
#define CTRL2_PM_XMT_TURNOFF			BIT(9)
#define STTS0_PM_LINKST_IN_L2			BIT(13)

static int imx6_pcie_cz_enabled;
static unsigned int imx6_pcie_grp_offset(const struct imx6_pcie *imx6_pcie)
{
	WARN_ON(imx6_pcie->drvdata->variant != IMX8MQ &&
		imx6_pcie->drvdata->variant != IMX8MQ_EP &&
		imx6_pcie->drvdata->variant != IMX8MM &&
		imx6_pcie->drvdata->variant != IMX8MM_EP &&
		imx6_pcie->drvdata->variant != IMX8MP &&
		imx6_pcie->drvdata->variant != IMX8MP_EP);
	return imx6_pcie->controller_id == 1 ? IOMUXC_GPR16 : IOMUXC_GPR14;
}

static void imx6_pcie_configure_type(struct imx6_pcie *imx6_pcie)
{
	unsigned int addr, mask, val, mode;

	if (imx6_pcie->drvdata->mode == DW_PCIE_EP_TYPE)
		mode = PCI_EXP_TYPE_ENDPOINT;
	else
		mode = PCI_EXP_TYPE_ROOT_PORT;

	addr = IOMUXC_GPR12;
	switch (imx6_pcie->drvdata->variant) {
	case IMX95:
	case IMX95_EP:
		addr = IMX95_PE0_GEN_CTRL_1;
		mask = IMX95_PCIE_DEVICE_TYPE;
		val = mode;
		break;
	case IMX8QM:
	case IMX8QM_EP:
	case IMX8QXP:
	case IMX8QXP_EP:
		addr = IMX8QM_PCIE_CTRL0_OFFSET;
		mask = IMX8QM_PCIE_CTRL0_TYPE_MASK;
		val = FIELD_PREP(IMX8QM_PCIE_CTRL0_TYPE_MASK, mode);
		break;
	case IMX8MQ:
	case IMX8MQ_EP:
		if (imx6_pcie->controller_id == 1) {
			mask = IMX8MQ_GPR12_PCIE2_CTRL_DEVICE_TYPE;
			val  = FIELD_PREP(IMX8MQ_GPR12_PCIE2_CTRL_DEVICE_TYPE,
					  mode);
		} else {
			mask = IMX6Q_GPR12_DEVICE_TYPE;
			val  = FIELD_PREP(IMX6Q_GPR12_DEVICE_TYPE, mode);
		}
		break;
	default:
		mask = IMX6Q_GPR12_DEVICE_TYPE;
		val  = FIELD_PREP(IMX6Q_GPR12_DEVICE_TYPE, mode);
		break;
	}

	regmap_update_bits(imx6_pcie->iomuxc_gpr, addr, mask, val);

	/* On i.MX95 the PCI PM needs to be enabled in order to
	 * generate the CLKREQ output signal.
	 */
	if ((mode == PCI_EXP_TYPE_ROOT_PORT) &&
	    (imx6_pcie->drvdata->variant == IMX95)) {
		val = dw_pcie_find_ext_capability(imx6_pcie->pci, PCI_EXT_CAP_ID_L1SS);
		dw_pcie_writel_dbi(imx6_pcie->pci, val + PCI_L1SS_CTL1, 1<<1);
	}
}

static int pcie_phy_poll_ack(struct imx6_pcie *imx6_pcie, bool exp_val)
{
	struct dw_pcie *pci = imx6_pcie->pci;
	bool val;
	u32 max_iterations = 10;
	u32 wait_counter = 0;

	do {
		val = dw_pcie_readl_dbi(pci, PCIE_PHY_STAT) &
			PCIE_PHY_STAT_ACK;
		wait_counter++;

		if (val == exp_val)
			return 0;

		udelay(1);
	} while (wait_counter < max_iterations);

	return -ETIMEDOUT;
}

static int pcie_phy_wait_ack(struct imx6_pcie *imx6_pcie, int addr)
{
	struct dw_pcie *pci = imx6_pcie->pci;
	u32 val;
	int ret;

	val = PCIE_PHY_CTRL_DATA(addr);
	dw_pcie_writel_dbi(pci, PCIE_PHY_CTRL, val);

	val |= PCIE_PHY_CTRL_CAP_ADR;
	dw_pcie_writel_dbi(pci, PCIE_PHY_CTRL, val);

	ret = pcie_phy_poll_ack(imx6_pcie, true);
	if (ret)
		return ret;

	val = PCIE_PHY_CTRL_DATA(addr);
	dw_pcie_writel_dbi(pci, PCIE_PHY_CTRL, val);

	return pcie_phy_poll_ack(imx6_pcie, false);
}

/* Read from the 16-bit PCIe PHY control registers (not memory-mapped) */
static int pcie_phy_read(struct imx6_pcie *imx6_pcie, int addr, u16 *data)
{
	struct dw_pcie *pci = imx6_pcie->pci;
	u32 phy_ctl;
	int ret;

	ret = pcie_phy_wait_ack(imx6_pcie, addr);
	if (ret)
		return ret;

	/* assert Read signal */
	phy_ctl = PCIE_PHY_CTRL_RD;
	dw_pcie_writel_dbi(pci, PCIE_PHY_CTRL, phy_ctl);

	ret = pcie_phy_poll_ack(imx6_pcie, true);
	if (ret)
		return ret;

	*data = dw_pcie_readl_dbi(pci, PCIE_PHY_STAT);

	/* deassert Read signal */
	dw_pcie_writel_dbi(pci, PCIE_PHY_CTRL, 0x00);

	return pcie_phy_poll_ack(imx6_pcie, false);
}

static int pcie_phy_write(struct imx6_pcie *imx6_pcie, int addr, u16 data)
{
	struct dw_pcie *pci = imx6_pcie->pci;
	u32 var;
	int ret;

	/* write addr */
	/* cap addr */
	ret = pcie_phy_wait_ack(imx6_pcie, addr);
	if (ret)
		return ret;

	var = PCIE_PHY_CTRL_DATA(data);
	dw_pcie_writel_dbi(pci, PCIE_PHY_CTRL, var);

	/* capture data */
	var |= PCIE_PHY_CTRL_CAP_DAT;
	dw_pcie_writel_dbi(pci, PCIE_PHY_CTRL, var);

	ret = pcie_phy_poll_ack(imx6_pcie, true);
	if (ret)
		return ret;

	/* deassert cap data */
	var = PCIE_PHY_CTRL_DATA(data);
	dw_pcie_writel_dbi(pci, PCIE_PHY_CTRL, var);

	/* wait for ack de-assertion */
	ret = pcie_phy_poll_ack(imx6_pcie, false);
	if (ret)
		return ret;

	/* assert wr signal */
	var = PCIE_PHY_CTRL_WR;
	dw_pcie_writel_dbi(pci, PCIE_PHY_CTRL, var);

	/* wait for ack */
	ret = pcie_phy_poll_ack(imx6_pcie, true);
	if (ret)
		return ret;

	/* deassert wr signal */
	var = PCIE_PHY_CTRL_DATA(data);
	dw_pcie_writel_dbi(pci, PCIE_PHY_CTRL, var);

	/* wait for ack de-assertion */
	ret = pcie_phy_poll_ack(imx6_pcie, false);
	if (ret)
		return ret;

	dw_pcie_writel_dbi(pci, PCIE_PHY_CTRL, 0x0);

	return 0;
}

static void imx95_pcie_inti_phy(struct imx6_pcie *imx6_pcie)
{
	regmap_update_bits(imx6_pcie->iomuxc_gpr,
			IMX95_PCIE_SS_RW_REG_0,
			IMX95_PCIE_PHY_CR_PARA_SEL,
			IMX95_PCIE_PHY_CR_PARA_SEL);
	/* Different clock modes should be handled. */
	if (imx6_pcie->refclk_pad_mode == IMX8_PCIE_REFCLK_PAD_INPUT) {
		/* External clock mode */
		regmap_update_bits(imx6_pcie->iomuxc_gpr,
				   IMX95_PCIE_PHY_GEN_CTRL,
				   IMX95_PCIE_REF_USE_PAD,
				   IMX95_PCIE_REF_USE_PAD);
		regmap_update_bits(imx6_pcie->iomuxc_gpr,
				   IMX95_PCIE_SS_RW_REG_0,
				   IMX95_PCIE_REF_CLKEN, 0);
	} else {
		/* Internal PLL clock mode */
		regmap_update_bits(imx6_pcie->iomuxc_gpr,
				   IMX95_PCIE_PHY_GEN_CTRL,
				   IMX95_PCIE_REF_USE_PAD, 0);
		regmap_update_bits(imx6_pcie->iomuxc_gpr,
				   IMX95_PCIE_SS_RW_REG_0,
				   IMX95_PCIE_REF_CLKEN,
				   IMX95_PCIE_REF_CLKEN);
	}
}

static void imx6_pcie_init_phy(struct imx6_pcie *imx6_pcie)
{
	switch (imx6_pcie->drvdata->variant) {
	case IMX8QM:
	case IMX8QM_EP:
	case IMX8QXP:
	case IMX8QXP_EP:
	case IMX8MM:
	case IMX8MM_EP:
	case IMX8MP:
	case IMX8MP_EP:
		/*
		 * The PHY initialization had been done in the PHY
		 * driver, break here directly.
		 */
		break;
	case IMX95:
	case IMX95_EP:
		imx95_pcie_inti_phy(imx6_pcie);
		break;
	case IMX8MQ:
	case IMX8MQ_EP:
		/*
		 * TODO: Currently this code assumes external
		 * oscillator is being used
		 */
		regmap_update_bits(imx6_pcie->iomuxc_gpr,
				   imx6_pcie_grp_offset(imx6_pcie),
				   IMX8MQ_GPR_PCIE_REF_USE_PAD,
				   IMX8MQ_GPR_PCIE_REF_USE_PAD);
		/*
		 * Regarding the datasheet, the PCIE_VPH is suggested
		 * to be 1.8V. If the PCIE_VPH is supplied by 3.3V, the
		 * VREG_BYPASS should be cleared to zero.
		 */
		if (imx6_pcie->vph &&
		    regulator_get_voltage(imx6_pcie->vph) > 3000000)
			regmap_update_bits(imx6_pcie->iomuxc_gpr,
					   imx6_pcie_grp_offset(imx6_pcie),
					   IMX8MQ_GPR_PCIE_VREG_BYPASS,
					   0);
		break;
	case IMX7D:
	case IMX7D_EP:
		regmap_update_bits(imx6_pcie->iomuxc_gpr, IOMUXC_GPR12,
				   IMX7D_GPR12_PCIE_PHY_REFCLK_SEL, 0);
		break;
	case IMX6SX:
	case IMX6SX_EP:
		regmap_update_bits(imx6_pcie->iomuxc_gpr, IOMUXC_GPR12,
				   IMX6SX_GPR12_PCIE_RX_EQ_MASK,
				   IMX6SX_GPR12_PCIE_RX_EQ_2);
		fallthrough;
	default:
		regmap_update_bits(imx6_pcie->iomuxc_gpr, IOMUXC_GPR12,
				   IMX6Q_GPR12_PCIE_CTL_2, 0 << 10);

		/* configure constant input signal to the pcie ctrl and phy */
		regmap_update_bits(imx6_pcie->iomuxc_gpr, IOMUXC_GPR12,
				   IMX6Q_GPR12_LOS_LEVEL, 9 << 4);

		regmap_update_bits(imx6_pcie->iomuxc_gpr, IOMUXC_GPR8,
				   IMX6Q_GPR8_TX_DEEMPH_GEN1,
				   imx6_pcie->tx_deemph_gen1 << 0);
		regmap_update_bits(imx6_pcie->iomuxc_gpr, IOMUXC_GPR8,
				   IMX6Q_GPR8_TX_DEEMPH_GEN2_3P5DB,
				   imx6_pcie->tx_deemph_gen2_3p5db << 6);
		regmap_update_bits(imx6_pcie->iomuxc_gpr, IOMUXC_GPR8,
				   IMX6Q_GPR8_TX_DEEMPH_GEN2_6DB,
				   imx6_pcie->tx_deemph_gen2_6db << 12);
		regmap_update_bits(imx6_pcie->iomuxc_gpr, IOMUXC_GPR8,
				   IMX6Q_GPR8_TX_SWING_FULL,
				   imx6_pcie->tx_swing_full << 18);
		regmap_update_bits(imx6_pcie->iomuxc_gpr, IOMUXC_GPR8,
				   IMX6Q_GPR8_TX_SWING_LOW,
				   imx6_pcie->tx_swing_low << 25);
		break;
	}
}

static void imx7d_pcie_wait_for_phy_pll_lock(struct imx6_pcie *imx6_pcie)
{
	u32 val;
	struct device *dev = imx6_pcie->pci->dev;

	if (regmap_read_poll_timeout(imx6_pcie->iomuxc_gpr,
				     IOMUXC_GPR22, val,
				     val & IMX7D_GPR22_PCIE_PHY_PLL_LOCKED,
				     PHY_PLL_LOCK_WAIT_USLEEP_MAX,
				     PHY_PLL_LOCK_WAIT_TIMEOUT))
		dev_err(dev, "PCIe PLL lock timeout\n");
}

static int imx6_setup_phy_mpll(struct imx6_pcie *imx6_pcie)
{
	unsigned long phy_rate = clk_get_rate(imx6_pcie->pcie_phy);
	int mult, div;
	u16 val;

	if (!(imx6_pcie->drvdata->flags & IMX6_PCIE_FLAG_IMX6_PHY))
		return 0;

	switch (phy_rate) {
	case 125000000:
		/*
		 * The default settings of the MPLL are for a 125MHz input
		 * clock, so no need to reconfigure anything in that case.
		 */
		return 0;
	case 100000000:
		mult = 25;
		div = 0;
		break;
	case 200000000:
		mult = 25;
		div = 1;
		break;
	default:
		dev_err(imx6_pcie->pci->dev,
			"Unsupported PHY reference clock rate %lu\n", phy_rate);
		return -EINVAL;
	}

	pcie_phy_read(imx6_pcie, PCIE_PHY_MPLL_OVRD_IN_LO, &val);
	val &= ~(PCIE_PHY_MPLL_MULTIPLIER_MASK <<
		 PCIE_PHY_MPLL_MULTIPLIER_SHIFT);
	val |= mult << PCIE_PHY_MPLL_MULTIPLIER_SHIFT;
	val |= PCIE_PHY_MPLL_MULTIPLIER_OVRD;
	pcie_phy_write(imx6_pcie, PCIE_PHY_MPLL_OVRD_IN_LO, val);

	pcie_phy_read(imx6_pcie, PCIE_PHY_ATEOVRD, &val);
	val &= ~(PCIE_PHY_ATEOVRD_REF_CLKDIV_MASK <<
		 PCIE_PHY_ATEOVRD_REF_CLKDIV_SHIFT);
	val |= div << PCIE_PHY_ATEOVRD_REF_CLKDIV_SHIFT;
	val |= PCIE_PHY_ATEOVRD_EN;
	pcie_phy_write(imx6_pcie, PCIE_PHY_ATEOVRD, val);

	return 0;
}

static void imx6_pcie_reset_phy(struct imx6_pcie *imx6_pcie)
{
	u16 tmp;

	if (!(imx6_pcie->drvdata->flags & IMX6_PCIE_FLAG_IMX6_PHY))
		return;

	pcie_phy_read(imx6_pcie, PHY_RX_OVRD_IN_LO, &tmp);
	tmp |= (PHY_RX_OVRD_IN_LO_RX_DATA_EN |
		PHY_RX_OVRD_IN_LO_RX_PLL_EN);
	pcie_phy_write(imx6_pcie, PHY_RX_OVRD_IN_LO, tmp);

	usleep_range(2000, 3000);

	pcie_phy_read(imx6_pcie, PHY_RX_OVRD_IN_LO, &tmp);
	tmp &= ~(PHY_RX_OVRD_IN_LO_RX_DATA_EN |
		  PHY_RX_OVRD_IN_LO_RX_PLL_EN);
	pcie_phy_write(imx6_pcie, PHY_RX_OVRD_IN_LO, tmp);
}

#ifdef CONFIG_ARM
/*  Added for PCI abort handling */
static int imx6q_pcie_abort_handler(unsigned long addr,
		unsigned int fsr, struct pt_regs *regs)
{
	unsigned long pc = instruction_pointer(regs);
	unsigned long instr = *(unsigned long *)pc;
	int reg = (instr >> 12) & 15;

	/*
	 * If the instruction being executed was a read,
	 * make it look like it read all-ones.
	 */
	if ((instr & 0x0c100000) == 0x04100000) {
		unsigned long val;

		if (instr & 0x00400000)
			val = 255;
		else
			val = -1;

		regs->uregs[reg] = val;
		regs->ARM_pc += 4;
		return 0;
	}

	if ((instr & 0x0e100090) == 0x00100090) {
		regs->uregs[reg] = -1;
		regs->ARM_pc += 4;
		return 0;
	}

	return 1;
}
#endif

static int imx6_pcie_attach_pd(struct device *dev)
{
	struct imx6_pcie *imx6_pcie = dev_get_drvdata(dev);
	struct device_link *link;

	/* Do nothing when in a single power domain */
	if (dev->pm_domain)
		return 0;

	imx6_pcie->pd_pcie = dev_pm_domain_attach_by_name(dev, "pcie");
	if (IS_ERR(imx6_pcie->pd_pcie))
		return PTR_ERR(imx6_pcie->pd_pcie);
	/* Do nothing when power domain missing */
	if (!imx6_pcie->pd_pcie)
		return 0;
	link = device_link_add(dev, imx6_pcie->pd_pcie,
			DL_FLAG_STATELESS |
			DL_FLAG_PM_RUNTIME |
			DL_FLAG_RPM_ACTIVE);
	if (!link) {
		dev_err(dev, "Failed to add device_link to pcie pd.\n");
		return -EINVAL;
	}

	imx6_pcie->pd_pcie_phy = dev_pm_domain_attach_by_name(dev, "pcie_phy");
	if (IS_ERR(imx6_pcie->pd_pcie_phy))
		return PTR_ERR(imx6_pcie->pd_pcie_phy);

	link = device_link_add(dev, imx6_pcie->pd_pcie_phy,
			DL_FLAG_STATELESS |
			DL_FLAG_PM_RUNTIME |
			DL_FLAG_RPM_ACTIVE);
	if (!link) {
		dev_err(dev, "Failed to add device_link to pcie_phy pd.\n");
		return -EINVAL;
	}

	return 0;
}

static int imx6_pcie_enable_ref_clk(struct imx6_pcie *imx6_pcie)
{
	struct dw_pcie *pci = imx6_pcie->pci;
	struct device *dev = pci->dev;
	unsigned int offset;
	int ret = 0;

	switch (imx6_pcie->drvdata->variant) {
	case IMX6SX:
	case IMX6SX_EP:
		ret = clk_prepare_enable(imx6_pcie->pcie_inbound_axi);
		if (ret) {
			dev_err(dev, "unable to enable pcie_axi clock\n");
			break;
		}

		regmap_update_bits(imx6_pcie->iomuxc_gpr, IOMUXC_GPR12,
				   IMX6SX_GPR12_PCIE_TEST_POWERDOWN, 0);
		break;
	case IMX6QP:
	case IMX6QP_EP:
	case IMX6Q:
	case IMX6Q_EP:
		/* power up core phy and enable ref clock */
		regmap_update_bits(imx6_pcie->iomuxc_gpr, IOMUXC_GPR1,
				   IMX6Q_GPR1_PCIE_TEST_PD, 0 << 18);
		/*
		 * the async reset input need ref clock to sync internally,
		 * when the ref clock comes after reset, internal synced
		 * reset time is too short, cannot meet the requirement.
		 * add one ~10us delay here.
		 */
		usleep_range(10, 100);
		regmap_update_bits(imx6_pcie->iomuxc_gpr, IOMUXC_GPR1,
				   IMX6Q_GPR1_PCIE_REF_CLK_EN, 1 << 16);
		break;
	case IMX7D:
	case IMX7D_EP:
		break;
	case IMX8MM:
	case IMX8MM_EP:
	case IMX8MQ:
	case IMX8MQ_EP:
	case IMX8MP:
	case IMX8MP_EP:
		ret = clk_prepare_enable(imx6_pcie->pcie_aux);
		if (ret) {
			dev_err(dev, "unable to enable pcie_aux clock\n");
			break;
		}

		offset = imx6_pcie_grp_offset(imx6_pcie);
		/*
		 * Set the over ride low and enabled
		 * make sure that REF_CLK is turned on.
		 */
		regmap_update_bits(imx6_pcie->iomuxc_gpr, offset,
				   IMX8MQ_GPR_PCIE_CLK_REQ_OVERRIDE,
				   0);
		regmap_update_bits(imx6_pcie->iomuxc_gpr, offset,
				   IMX8MQ_GPR_PCIE_CLK_REQ_OVERRIDE_EN,
				   IMX8MQ_GPR_PCIE_CLK_REQ_OVERRIDE_EN);
		break;
	case IMX8QM:
	case IMX8QM_EP:
	case IMX8QXP:
	case IMX8QXP_EP:
		ret = clk_prepare_enable(imx6_pcie->pcie_inbound_axi);
		if (ret) {
			dev_err(dev, "unable to enable pcie_axi clock\n");
			return ret;
		}
		break;
	case IMX95:
		ret = clk_prepare_enable(imx6_pcie->pcie_aux);
		if (ret) {
			dev_err(dev, "unable to enable pcie_aux clock\n");
			break;
		}
		if (imx6_pcie->refclk_pad_mode == IMX8_PCIE_REFCLK_PAD_OUTPUT) {
			ret = clk_prepare_enable(imx6_pcie->pcie_ref);
			if (ret) {
				dev_err(dev, "unable to enable ref clock\n");
				clk_disable_unprepare(imx6_pcie->pcie_aux);
				break;
			}
		}
		break;
	case IMX95_EP:
		ret = clk_prepare_enable(imx6_pcie->pcie_aux);
		if (ret)
			dev_err(dev, "unable to enable pcie_aux clock\n");
		break;
	}

	return ret;
}

static void imx6_pcie_disable_ref_clk(struct imx6_pcie *imx6_pcie)
{
	switch (imx6_pcie->drvdata->variant) {
	case IMX6SX:
	case IMX6SX_EP:
		clk_disable_unprepare(imx6_pcie->pcie_inbound_axi);
		break;
	case IMX6QP:
	case IMX6QP_EP:
	case IMX6Q:
	case IMX6Q_EP:
		regmap_update_bits(imx6_pcie->iomuxc_gpr, IOMUXC_GPR1,
				IMX6Q_GPR1_PCIE_REF_CLK_EN, 0);
		regmap_update_bits(imx6_pcie->iomuxc_gpr, IOMUXC_GPR1,
				IMX6Q_GPR1_PCIE_TEST_PD,
				IMX6Q_GPR1_PCIE_TEST_PD);
		break;
	case IMX7D:
	case IMX7D_EP:
		regmap_update_bits(imx6_pcie->iomuxc_gpr, IOMUXC_GPR12,
				   IMX7D_GPR12_PCIE_PHY_REFCLK_SEL,
				   IMX7D_GPR12_PCIE_PHY_REFCLK_SEL);
		break;
	case IMX95:
		if (imx6_pcie->refclk_pad_mode == IMX8_PCIE_REFCLK_PAD_OUTPUT)
			clk_disable_unprepare(imx6_pcie->pcie_ref);
		fallthrough;
	case IMX95_EP:
	case IMX8MM:
	case IMX8MM_EP:
	case IMX8MQ:
	case IMX8MQ_EP:
	case IMX8MP:
	case IMX8MP_EP:
		clk_disable_unprepare(imx6_pcie->pcie_aux);
		break;
	case IMX8QM:
	case IMX8QM_EP:
	case IMX8QXP:
	case IMX8QXP_EP:
		clk_disable_unprepare(imx6_pcie->pcie_inbound_axi);
		break;
	default:
		break;
	}
}

static int imx6_pcie_clk_enable(struct imx6_pcie *imx6_pcie)
{
	struct dw_pcie *pci = imx6_pcie->pci;
	struct device *dev = pci->dev;
	int ret;

	ret = clk_prepare_enable(imx6_pcie->pcie_phy);
	if (ret) {
		dev_err(dev, "unable to enable pcie_phy clock\n");
		return ret;
	}

	ret = clk_prepare_enable(imx6_pcie->pcie_bus);
	if (ret) {
		dev_err(dev, "unable to enable pcie_bus clock\n");
		goto err_pcie_bus;
	}

	ret = clk_prepare_enable(imx6_pcie->pcie);
	if (ret) {
		dev_err(dev, "unable to enable pcie clock\n");
		goto err_pcie;
	}

	ret = imx6_pcie_enable_ref_clk(imx6_pcie);
	if (ret) {
		dev_err(dev, "unable to enable pcie ref clock\n");
		goto err_ref_clk;
	}

	/* allow the clocks to stabilize */
	usleep_range(200, 500);
	return 0;

err_ref_clk:
	clk_disable_unprepare(imx6_pcie->pcie);
err_pcie:
	clk_disable_unprepare(imx6_pcie->pcie_bus);
err_pcie_bus:
	clk_disable_unprepare(imx6_pcie->pcie_phy);

	return ret;
}

static void imx6_pcie_clk_disable(struct imx6_pcie *imx6_pcie)
{
	imx6_pcie_disable_ref_clk(imx6_pcie);
	clk_disable_unprepare(imx6_pcie->pcie);
	clk_disable_unprepare(imx6_pcie->pcie_bus);
	clk_disable_unprepare(imx6_pcie->pcie_phy);
}

static void imx6_pcie_assert_core_reset(struct imx6_pcie *imx6_pcie)
{
	switch (imx6_pcie->drvdata->variant) {
	case IMX7D:
	case IMX7D_EP:
	case IMX8MQ:
	case IMX8MQ_EP:
		reset_control_assert(imx6_pcie->pciephy_reset);
		fallthrough;
	case IMX8MM:
	case IMX8MM_EP:
	case IMX8MP:
	case IMX8MP_EP:
		reset_control_assert(imx6_pcie->apps_reset);
		break;
	case IMX6SX:
	case IMX6SX_EP:
		regmap_update_bits(imx6_pcie->iomuxc_gpr, IOMUXC_GPR12,
				   IMX6SX_GPR12_PCIE_TEST_POWERDOWN,
				   IMX6SX_GPR12_PCIE_TEST_POWERDOWN);
		/* Force PCIe PHY reset */
		regmap_update_bits(imx6_pcie->iomuxc_gpr, IOMUXC_GPR5,
				   IMX6SX_GPR5_PCIE_BTNRST_RESET,
				   IMX6SX_GPR5_PCIE_BTNRST_RESET);
		break;
	case IMX6QP:
	case IMX6QP_EP:
		regmap_update_bits(imx6_pcie->iomuxc_gpr, IOMUXC_GPR1,
				   IMX6Q_GPR1_PCIE_SW_RST,
				   IMX6Q_GPR1_PCIE_SW_RST);
		break;
	case IMX6Q:
	case IMX6Q_EP:
		regmap_update_bits(imx6_pcie->iomuxc_gpr, IOMUXC_GPR1,
				   IMX6Q_GPR1_PCIE_TEST_PD, 1 << 18);
		regmap_update_bits(imx6_pcie->iomuxc_gpr, IOMUXC_GPR1,
				   IMX6Q_GPR1_PCIE_REF_CLK_EN, 0 << 16);
		break;
	case IMX95: /* Power on reset, nothing to do here */
	case IMX95_EP:
	case IMX8QM:
	case IMX8QM_EP:
	case IMX8QXP:
	case IMX8QXP_EP:
		break;
	}

	/* Some boards don't have PCIe reset GPIO. */
	if (gpio_is_valid(imx6_pcie->reset_gpio))
		gpio_set_value_cansleep(imx6_pcie->reset_gpio,
					imx6_pcie->gpio_active_high);
}

static int imx6_pcie_deassert_core_reset(struct imx6_pcie *imx6_pcie)
{
	u32 val;
	struct dw_pcie *pci = imx6_pcie->pci;
	struct device *dev = pci->dev;

	switch (imx6_pcie->drvdata->variant) {
	case IMX95:
	case IMX95_EP:
		/* Polling the MPLL_STATE */
		if (regmap_read_poll_timeout(imx6_pcie->iomuxc_gpr,
					IMX95_PCIE_PHY_MPLLA_CTRL, val,
					val & IMX95_PCIE_PHY_MPLL_STATE,
					10, 10000))
			dev_err(dev, "PCIe PLL lock timeout\n");
		break;
	case IMX8MQ:
	case IMX8MQ_EP:
		reset_control_deassert(imx6_pcie->pciephy_reset);
		break;
	case IMX7D:
	case IMX7D_EP:
		reset_control_deassert(imx6_pcie->pciephy_reset);

		/* Workaround for ERR010728, failure of PCI-e PLL VCO to
		 * oscillate, especially when cold.  This turns off "Duty-cycle
		 * Corrector" and other mysterious undocumented things.
		 */
		if (likely(imx6_pcie->phy_base)) {
			/* De-assert DCC_FB_EN */
			writel(PCIE_PHY_CMN_REG4_DCC_FB_EN,
			       imx6_pcie->phy_base + PCIE_PHY_CMN_REG4);
			/* Assert RX_EQS and RX_EQS_SEL */
			writel(PCIE_PHY_CMN_REG24_RX_EQ_SEL
				| PCIE_PHY_CMN_REG24_RX_EQ,
			       imx6_pcie->phy_base + PCIE_PHY_CMN_REG24);
			/* Assert ATT_MODE */
			writel(PCIE_PHY_CMN_REG26_ATT_MODE,
			       imx6_pcie->phy_base + PCIE_PHY_CMN_REG26);
		} else {
			dev_warn(dev, "Unable to apply ERR010728 workaround. DT missing fsl,imx7d-pcie-phy phandle ?\n");
		}

		imx7d_pcie_wait_for_phy_pll_lock(imx6_pcie);
		break;
	case IMX6SX:
	case IMX6SX_EP:
		regmap_update_bits(imx6_pcie->iomuxc_gpr, IOMUXC_GPR5,
				   IMX6SX_GPR5_PCIE_BTNRST_RESET, 0);
		break;
	case IMX6QP:
	case IMX6QP_EP:
		regmap_update_bits(imx6_pcie->iomuxc_gpr, IOMUXC_GPR1,
				   IMX6Q_GPR1_PCIE_SW_RST, 0);

		usleep_range(200, 500);
		break;
	case IMX6Q:		/* Nothing to do */
	case IMX6Q_EP:
	case IMX8MM:
	case IMX8MM_EP:
	case IMX8MP:
	case IMX8MP_EP:
	case IMX8QM:
	case IMX8QM_EP:
	case IMX8QXP:
	case IMX8QXP_EP:
		break;
	}

	/* Some boards don't have PCIe reset GPIO. */
	if (gpio_is_valid(imx6_pcie->reset_gpio)) {
		msleep(100);
		gpio_set_value_cansleep(imx6_pcie->reset_gpio,
					!imx6_pcie->gpio_active_high);
		/* Wait for 100ms after PERST# deassertion (PCIe r5.0, 6.6.1) */
		msleep(100);
	}

	return 0;
}

static int imx6_pcie_wait_for_speed_change(struct imx6_pcie *imx6_pcie)
{
	struct dw_pcie *pci = imx6_pcie->pci;
	struct device *dev = pci->dev;
	u32 tmp;
	unsigned int retries;

	for (retries = 0; retries < 200; retries++) {
		tmp = dw_pcie_readl_dbi(pci, PCIE_LINK_WIDTH_SPEED_CONTROL);
		/* Test if the speed change finished. */
		if (!(tmp & PORT_LOGIC_SPEED_CHANGE))
			return 0;
		usleep_range(100, 1000);
	}

	dev_err(dev, "Speed change timeout\n");
	return -ETIMEDOUT;
}

static void imx6_pcie_ltssm_enable(struct device *dev)
{
	struct imx6_pcie *imx6_pcie = dev_get_drvdata(dev);

	switch (imx6_pcie->drvdata->variant) {
	case IMX6Q:
	case IMX6Q_EP:
	case IMX6SX:
	case IMX6SX_EP:
	case IMX6QP:
	case IMX6QP_EP:
		regmap_update_bits(imx6_pcie->iomuxc_gpr, IOMUXC_GPR12,
				   IMX6Q_GPR12_PCIE_CTL_2,
				   IMX6Q_GPR12_PCIE_CTL_2);
		break;
	case IMX7D:
	case IMX7D_EP:
	case IMX8MQ:
	case IMX8MQ_EP:
	case IMX8MM:
	case IMX8MM_EP:
	case IMX8MP:
	case IMX8MP_EP:
		reset_control_deassert(imx6_pcie->apps_reset);
		break;
	case IMX8QM:
	case IMX8QM_EP:
	case IMX8QXP:
	case IMX8QXP_EP:
		/* Bit4 of the CTRL2 */
		regmap_update_bits(imx6_pcie->iomuxc_gpr,
				IMX8QM_PCIE_CTRL2_OFFSET,
				CTRL2_LTSSM_ENABLE,
				CTRL2_LTSSM_ENABLE);
		break;
	case IMX95:
	case IMX95_EP:
		regmap_update_bits(imx6_pcie->iomuxc_gpr,
				IMX95_PE0_GEN_CTRL_3,
				IMX95_PCIE_LTSSM_EN,
				IMX95_PCIE_LTSSM_EN);
		break;
	}
}

static void imx6_pcie_ltssm_disable(struct device *dev)
{
	struct imx6_pcie *imx6_pcie = dev_get_drvdata(dev);

	switch (imx6_pcie->drvdata->variant) {
	case IMX6Q:
	case IMX6Q_EP:
	case IMX6SX:
	case IMX6SX_EP:
	case IMX6QP:
	case IMX6QP_EP:
		regmap_update_bits(imx6_pcie->iomuxc_gpr, IOMUXC_GPR12,
				   IMX6Q_GPR12_PCIE_CTL_2, 0);
		break;
	case IMX7D:
	case IMX7D_EP:
	case IMX8MQ:
	case IMX8MQ_EP:
	case IMX8MM:
	case IMX8MM_EP:
	case IMX8MP:
	case IMX8MP_EP:
		reset_control_assert(imx6_pcie->apps_reset);
		break;
	case IMX8QM:
	case IMX8QM_EP:
	case IMX8QXP:
	case IMX8QXP_EP:
		/* Bit4 of the CTRL2 */
		regmap_update_bits(imx6_pcie->iomuxc_gpr,
				IMX8QM_PCIE_CTRL2_OFFSET,
				CTRL2_LTSSM_ENABLE, 0);
		regmap_update_bits(imx6_pcie->iomuxc_gpr,
				IMX8QM_PCIE_CTRL2_OFFSET,
				CTRL2_READY_ENTR_L23, 0);
		break;
	case IMX95:
	case IMX95_EP:
		regmap_update_bits(imx6_pcie->iomuxc_gpr,
				IMX95_PE0_GEN_CTRL_3,
				IMX95_PCIE_LTSSM_EN, 0);
		break;
	}
}

static int imx6_pcie_start_link(struct dw_pcie *pci)
{
	struct imx6_pcie *imx6_pcie = to_imx6_pcie(pci);
	struct device *dev = pci->dev;
	u8 offset = dw_pcie_find_capability(pci, PCI_CAP_ID_EXP);
	u32 tmp;
	int ret;

	/*
	 * Force Gen1 operation when starting the link.  In case the link is
	 * started in Gen2 mode, there is a possibility the devices on the
	 * bus will not be detected at all.  This happens with PCIe switches.
	 */
	if (!imx6_pcie_cz_enabled) {
		dw_pcie_dbi_ro_wr_en(pci);
		tmp = dw_pcie_readl_dbi(pci, offset + PCI_EXP_LNKCAP);
		tmp &= ~PCI_EXP_LNKCAP_SLS;
		tmp |= PCI_EXP_LNKCAP_SLS_2_5GB;
		dw_pcie_writel_dbi(pci, offset + PCI_EXP_LNKCAP, tmp);
		dw_pcie_dbi_ro_wr_dis(pci);
	}

	/* Start LTSSM. */
	imx6_pcie_ltssm_enable(dev);

	ret = dw_pcie_wait_for_link(pci);
	if (ret)
		goto err_reset_phy;

	if (pci->link_gen > 1) {
		/* Allow faster modes after the link is up */
		dw_pcie_dbi_ro_wr_en(pci);
		tmp = dw_pcie_readl_dbi(pci, offset + PCI_EXP_LNKCAP);
		tmp &= ~PCI_EXP_LNKCAP_SLS;
		tmp |= pci->link_gen;
		dw_pcie_writel_dbi(pci, offset + PCI_EXP_LNKCAP, tmp);

		/*
		 * Start Directed Speed Change so the best possible
		 * speed both link partners support can be negotiated.
		 */
		tmp = dw_pcie_readl_dbi(pci, PCIE_LINK_WIDTH_SPEED_CONTROL);
		tmp |= PORT_LOGIC_SPEED_CHANGE;
		dw_pcie_writel_dbi(pci, PCIE_LINK_WIDTH_SPEED_CONTROL, tmp);
		dw_pcie_dbi_ro_wr_dis(pci);

		if (imx6_pcie->drvdata->flags &
		    IMX6_PCIE_FLAG_IMX6_SPEED_CHANGE) {
			/*
			 * On i.MX7, DIRECT_SPEED_CHANGE behaves differently
			 * from i.MX6 family when no link speed transition
			 * occurs and we go Gen1 -> yep, Gen1. The difference
			 * is that, in such case, it will not be cleared by HW
			 * which will cause the following code to report false
			 * failure.
			 */

			ret = imx6_pcie_wait_for_speed_change(imx6_pcie);
			if (ret) {
				dev_err(dev, "Failed to bring link up!\n");
				goto err_reset_phy;
			}
		}

		/* Make sure link training is finished as well! */
		ret = dw_pcie_wait_for_link(pci);
		if (ret)
			goto err_reset_phy;
	} else {
		dev_info(dev, "Link: Only Gen1 is enabled\n");
	}

	imx6_pcie->link_is_up = true;
	tmp = dw_pcie_readw_dbi(pci, offset + PCI_EXP_LNKSTA);
	dev_info(dev, "Link up, Gen%i\n", tmp & PCI_EXP_LNKSTA_CLS);
	return 0;

err_reset_phy:
	imx6_pcie->link_is_up = false;
	dev_dbg(dev, "PHY DEBUG_R0=0x%08x DEBUG_R1=0x%08x\n",
		dw_pcie_readl_dbi(pci, PCIE_PORT_DEBUG0),
		dw_pcie_readl_dbi(pci, PCIE_PORT_DEBUG1));
	imx6_pcie_reset_phy(imx6_pcie);
	/*
	 * FIXME.
	 * When PCIeB is link down, PHYX2_PCLK1 assertion(1b'1) would
	 * break SATA function
	 * Let PCIe_b probe failed to make SATA functional.
	 */
	if (unlikely(imx6_pcie->drvdata->variant == IMX8QM
			&& imx6_pcie->hsio_cfg == PCIEAX1PCIEBX1SATA
			&& imx6_pcie->controller_id == 1)) {
		dev_info(dev, "Let PCIe_b probe failed when link is down.\n");
		return ret;
	}
	return 0;
}

static void imx6_pcie_stop_link(struct dw_pcie *pci)
{
	struct device *dev = pci->dev;

	/* Turn off PCIe LTSSM */
	imx6_pcie_ltssm_disable(dev);
}

static u64 imx6_pcie_cpu_addr_fixup(struct dw_pcie *pcie, u64 cpu_addr)
{
	unsigned int offset;
	struct dw_pcie_ep *ep = &pcie->ep;
	struct dw_pcie_rp *pp = &pcie->pp;
	struct imx6_pcie *imx6_pcie = to_imx6_pcie(pcie);
	struct resource_entry *entry;

	if (!(imx6_pcie->drvdata->flags & IMX6_PCIE_FLAG_IMX6_CPU_ADDR_FIXUP))
		return cpu_addr;

	if (imx6_pcie->drvdata->mode == DW_PCIE_EP_TYPE) {
		offset = ep->phys_base;
	} else {
		entry = resource_list_first_type(&pp->bridge->windows,
						 IORESOURCE_MEM);
		offset = entry->res->start;
	}

	return (cpu_addr + imx6_pcie->local_addr - offset);
}

static int imx6_pcie_update_lut(struct imx6_pcie *imx6_pcie, int index,
				u16 reqid, u16 mask, u8 sid)
{
	struct dw_pcie *pci = imx6_pcie->pci;
	struct device *dev = pci->dev;
	u32 data1, data2;

	if (sid >= 64) {
		dev_err(dev, "Too big stream id: %d\n", sid);
		return -EINVAL;
	}

	data1 = FIELD_PREP(IMX95_PE0_LUT_DAC_ID, 0);
	data1 |= FIELD_PREP(IMX95_PE0_LUT_STREAM_ID, sid);
	data1 |= IMX95_PE0_LUT_VLD;

	regmap_write(imx6_pcie->iomuxc_gpr, IMX95_PE0_LUT_DATA1, data1);

	data2 = mask;
	data2 |= FIELD_PREP(IMX95_PE0_LUT_REQID, reqid);

	regmap_write(imx6_pcie->iomuxc_gpr, IMX95_PE0_LUT_DATA2, data2);

	regmap_write(imx6_pcie->iomuxc_gpr, IMX95_PE0_LUT_ACSCTRL, index);

	return 0;
}

struct imx6_of_map {
	u32 bdf;
	u32 phandle;
	u32 sid;
	u32 sid_len;
};

static int imx6_check_msi_and_smmmu(struct imx6_pcie *imx6_pcie,
				    struct imx6_of_map *msi_map,
				    u32 msi_size, u32 msi_map_mask,
				    struct imx6_of_map *smmu_map,
				    u32 smmu_size, u32 smmu_map_mask)
{
	struct dw_pcie *pci = imx6_pcie->pci;
	struct device *dev = pci->dev;
	int i;

	if (msi_map && smmu_map) {
		if (msi_size != smmu_size)
			return -EINVAL;
		if (msi_map_mask != smmu_map_mask)
			return -EINVAL;

		for (i = 0; i < msi_size / sizeof(*msi_map); i++) {
			if (msi_map->bdf != smmu_map->bdf) {
				dev_err(dev, "bdf setting is not match\n");
				return -EINVAL;
			}
			if ((msi_map->sid & IMX95_SID_MASK) != smmu_map->sid) {
				dev_err(dev, "sid setting is not match\n");
				return -EINVAL;
			}
			if ((msi_map->sid_len & IMX95_SID_MASK) != smmu_map->sid_len) {
				dev_err(dev, "sid_len setting is not match\n");
				return -EINVAL;
			}
		}
	}

	return 0;
}

/*
 * Simple static config lut according to dts settings
 * DAC index and stream ID used as a match result of LUT.
 * Pre-allocated and used by PCIes.
 *
 * Currently stream ID from 32-64 for PCIe.
 * 32-40: first PCI bus.
 * 40-48: second PCI bus.
 *
 * DAC_ID is index of TRDC.DAC index, start from 2 at iMX95.
 * ITS [pci(2bit): streamid(6bits)]
 *	pci 0 is 0
 *	pci 1 is 3
 */
static int imx6_pcie_config_sid(struct imx6_pcie *imx6_pcie)
{
	struct imx6_of_map *msi_map = NULL, *smmu_map = NULL, *cur;
	struct dw_pcie *pci = imx6_pcie->pci;
	struct device *dev = pci->dev;
	int i, j, lut_index, nr_map, msi_size = 0, smmu_size = 0;
	u32 msi_map_mask = 0xffff, smmu_map_mask = 0xffff;
	u32 mask;
	int size;
	int ret = 0;

	of_get_property(dev->of_node, "msi-map", &msi_size);
	if (msi_size) {
		msi_map = devm_kzalloc(dev, msi_size, GFP_KERNEL);
		if (!msi_map)
			return -ENOMEM;

		if (of_property_read_u32_array(dev->of_node, "msi-map",
					       (u32 *)msi_map,
					       msi_size / sizeof(u32)))
			return -EINVAL;

		of_property_read_u32(dev->of_node, "msi-map-mask",
				     &msi_map_mask);
	}
	cur = msi_map;
	size = msi_size;
	mask = msi_map_mask;

	of_get_property(dev->of_node, "iommu-map", &smmu_size);
	if (smmu_size) {
		smmu_map = devm_kzalloc(dev, smmu_size, GFP_KERNEL);
		if (!smmu_map)
			return -ENOMEM;

		if (of_property_read_u32_array(dev->of_node, "iommu-map",
					       (u32 *)smmu_map,
					       smmu_size / sizeof(u32)))
			return -EINVAL;

		of_property_read_u32(dev->of_node, "iommu-map-mask",
				     &smmu_map_mask);
	}

	if (imx6_check_msi_and_smmmu(imx6_pcie, msi_map, msi_size, msi_map_mask,
				     smmu_map, smmu_size, smmu_map_mask))
		return -EINVAL;

	if (!cur) {
		cur = smmu_map;
		size = smmu_size;
		mask = smmu_map_mask;
	}

	nr_map = size / (sizeof(*cur));

	lut_index = 0;
	for (i = 0; i < nr_map; i++) {
		for (j = 0; j < cur->sid_len; j++) {
			imx6_pcie_update_lut(imx6_pcie, lut_index,
					     cur->bdf + j, mask,
					     (cur->sid + j) & IMX95_SID_MASK);
			lut_index++;
		}
		cur++;

		if (lut_index >= IMX95_MAX_LUT) {
			dev_err(dev, "its-map/iommu-map exceed HW limiation\n");
			ret = -EINVAL;
			break;
		}
	}

	devm_kfree(dev, smmu_map);
	devm_kfree(dev, msi_map);

	return 0;
}

static int imx6_pcie_host_init(struct dw_pcie_rp *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct device *dev = pci->dev;
	struct imx6_pcie *imx6_pcie = to_imx6_pcie(pci);
	int ret;

	if (imx6_pcie->vpcie) {
		ret = regulator_enable(imx6_pcie->vpcie);
		if (ret) {
			dev_err(dev, "failed to enable vpcie regulator: %d\n",
				ret);
			return ret;
		}
	}

	imx6_pcie_assert_core_reset(imx6_pcie);
	imx6_pcie_init_phy(imx6_pcie);

	ret = imx6_pcie_clk_enable(imx6_pcie);
	if (ret) {
		dev_err(dev, "unable to enable pcie clocks: %d\n", ret);
		goto err_reg_disable;
	}

	if (imx6_pcie->phy) {
		ret = phy_init(imx6_pcie->phy);
		if (ret) {
			dev_err(dev, "pcie PHY power up failed\n");
			goto err_clk_disable;
		}
	}

	if (imx6_pcie->phy) {
		ret = phy_power_on(imx6_pcie->phy);
		if (ret) {
			dev_err(dev, "waiting for PHY ready timeout!\n");
			goto err_phy_off;
		}
	}

	imx6_pcie_configure_type(imx6_pcie);
	ret = imx6_pcie_deassert_core_reset(imx6_pcie);
	if (ret < 0) {
		dev_err(dev, "pcie deassert core reset failed: %d\n", ret);
		goto err_phy_off;
	}

	imx6_setup_phy_mpll(imx6_pcie);

	ret = imx6_pcie_config_sid(imx6_pcie);
	if (ret < 0) {
		dev_err(dev, "failed to config sid:%d\n", ret);
		goto err_phy_off;
	}

	return 0;

err_phy_off:
	if (imx6_pcie->phy)
		phy_exit(imx6_pcie->phy);
err_clk_disable:
	imx6_pcie_clk_disable(imx6_pcie);
err_reg_disable:
	if (imx6_pcie->vpcie)
		regulator_disable(imx6_pcie->vpcie);
	return ret;
}

static void imx6_pcie_host_exit(struct dw_pcie_rp *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct imx6_pcie *imx6_pcie = to_imx6_pcie(pci);

	if (imx6_pcie_cz_enabled) {
		pr_info("PCIe compliance tests mode is enabled.\n");
		return;
	}

	if (imx6_pcie->phy) {
		if (phy_power_off(imx6_pcie->phy))
			dev_err(pci->dev, "unable to power off PHY\n");
		phy_exit(imx6_pcie->phy);
	}
	imx6_pcie_clk_disable(imx6_pcie);

	if (imx6_pcie->vpcie)
		regulator_disable(imx6_pcie->vpcie);
}

static const struct dw_pcie_host_ops imx6_pcie_host_ops = {
	.host_init = imx6_pcie_host_init,
	.host_deinit = imx6_pcie_host_exit,
};

static const struct dw_pcie_ops dw_pcie_ops = {
	.start_link = imx6_pcie_start_link,
	.stop_link = imx6_pcie_stop_link,
	.cpu_addr_fixup = imx6_pcie_cpu_addr_fixup,
};

static void imx6_pcie_ep_init(struct dw_pcie_ep *ep)
{
	enum pci_barno bar;
	struct dw_pcie *pci = to_dw_pcie_from_ep(ep);
	const struct pci_epc_features *epc_features;

	for (bar = BAR_0; bar <= BAR_5; bar++)
		dw_pcie_ep_reset_bar(pci, bar);
	if (ep->ops->get_features) {
		epc_features = ep->ops->get_features(ep);
		ep->page_size = epc_features->align;
	}
}

static int imx6_pcie_ep_raise_irq(struct dw_pcie_ep *ep, u8 func_no,
				  enum pci_epc_irq_type type,
				  u16 interrupt_num)
{
	struct dw_pcie *pci = to_dw_pcie_from_ep(ep);

	switch (type) {
	case PCI_EPC_IRQ_LEGACY:
		return dw_pcie_ep_raise_legacy_irq(ep, func_no);
	case PCI_EPC_IRQ_MSI:
		return dw_pcie_ep_raise_msi_irq(ep, func_no, interrupt_num);
	default:
		dev_err(pci->dev, "UNKNOWN IRQ type\n");
		return -EINVAL;
	}

	return 0;
}

static const struct pci_epc_features imx6q_pcie_epc_features = {
	.linkup_notifier = false,
	.msi_capable = true,
	.msix_capable = false,
	.reserved_bar = 1 << BAR_0 | 1 << BAR_1 | 1 << BAR_2,
	.align = SZ_64K,
};

static const struct pci_epc_features imx8m_pcie_epc_features = {
	.linkup_notifier = false,
	.msi_capable = true,
	.msix_capable = false,
	.reserved_bar = 1 << BAR_1 | 1 << BAR_3,
	.align = SZ_64K,
};

static const struct pci_epc_features imx8q_pcie_epc_features = {
	.linkup_notifier = false,
	.msi_capable = true,
	.msix_capable = false,
	.reserved_bar = 1 << BAR_1 | 1 << BAR_3 | 1 << BAR_5,
};

static const struct pci_epc_features imx95_pcie_epc_features = {
	.msi_capable = true,
	.bar_fixed_size[1] = SZ_64K,
	.align = SZ_4K,
};

static const struct pci_epc_features*
imx6_pcie_ep_get_features(struct dw_pcie_ep *ep)
{
	struct dw_pcie *pci = to_dw_pcie_from_ep(ep);
	struct imx6_pcie *imx6_pcie = to_imx6_pcie(pci);

	switch (imx6_pcie->drvdata->variant) {
	case IMX95_EP:
		return &imx95_pcie_epc_features;
	case IMX8QM_EP:
	case IMX8QXP_EP:
		return &imx8q_pcie_epc_features;
	case IMX6Q_EP:
	case IMX6QP_EP:
		return &imx6q_pcie_epc_features;
	default:
		return &imx8m_pcie_epc_features;
	}
}

static const struct dw_pcie_ep_ops pcie_ep_ops = {
	.ep_init = imx6_pcie_ep_init,
	.raise_irq = imx6_pcie_ep_raise_irq,
	.get_features = imx6_pcie_ep_get_features,
};

static int imx6_add_pcie_ep(struct imx6_pcie *imx6_pcie,
			   struct platform_device *pdev)
{
	int ret;
	struct dw_pcie_ep *ep;
	struct dw_pcie *pci = imx6_pcie->pci;
	struct dw_pcie_rp *pp = &pci->pp;
	struct device *dev = pci->dev;

	imx6_pcie_host_init(pp);
	ep = &pci->ep;
	ep->ops = &pcie_ep_ops;

	ret = dw_pcie_ep_init(ep);
	if (ret) {
		dev_err(dev, "failed to initialize endpoint\n");
		return ret;
	}
	/* Start LTSSM. */
	imx6_pcie_ltssm_enable(dev);

	return 0;
}

static void imx6_pcie_pm_turnoff(struct imx6_pcie *imx6_pcie)
{
	u32 val;
	struct device *dev = imx6_pcie->pci->dev;

	/* Some variants have a turnoff reset in DT */
	if (imx6_pcie->turnoff_reset) {
		reset_control_assert(imx6_pcie->turnoff_reset);
		reset_control_deassert(imx6_pcie->turnoff_reset);
		goto pm_turnoff_sleep;
	}

	/* Others poke directly at IOMUXC registers */
	switch (imx6_pcie->drvdata->variant) {
	case IMX6SX:
	case IMX6SX_EP:
	case IMX6QP:
	case IMX6QP_EP:
		regmap_update_bits(imx6_pcie->iomuxc_gpr, IOMUXC_GPR12,
				IMX6SX_GPR12_PCIE_PM_TURN_OFF,
				IMX6SX_GPR12_PCIE_PM_TURN_OFF);
		regmap_update_bits(imx6_pcie->iomuxc_gpr, IOMUXC_GPR12,
				IMX6SX_GPR12_PCIE_PM_TURN_OFF, 0);
		break;
	case IMX8QM:
	case IMX8QM_EP:
	case IMX8QXP:
	case IMX8QXP_EP:
		regmap_update_bits(imx6_pcie->iomuxc_gpr,
				IMX8QM_PCIE_CTRL2_OFFSET,
				CTRL2_PM_XMT_TURNOFF,
				CTRL2_PM_XMT_TURNOFF);
		regmap_update_bits(imx6_pcie->iomuxc_gpr,
				IMX8QM_PCIE_CTRL2_OFFSET,
				CTRL2_PM_XMT_TURNOFF,
				0);
		regmap_update_bits(imx6_pcie->iomuxc_gpr,
				IMX8QM_PCIE_CTRL2_OFFSET,
				CTRL2_READY_ENTR_L23,
				CTRL2_READY_ENTR_L23);
		/* check the L2 is entered or not. */
		if (regmap_read_poll_timeout(imx6_pcie->iomuxc_gpr,
					IMX8QM_PCIE_STTS0_OFFSET, val,
					val & STTS0_PM_LINKST_IN_L2,
					10, 10000))
			dev_err(dev, "PCIE%d can't enter into L2.\n",
					imx6_pcie->controller_id);
		else
			dev_info(dev, "PCIE%d enter into L2 (STTS:0x%08x).\n",
					imx6_pcie->controller_id, val);

		break;
	case IMX95:
	case IMX95_EP:
		regmap_update_bits(imx6_pcie->iomuxc_gpr,
				IMX95_PE0_PM_CTRL,
				IMX95_PCIE_PME_PF_INDEX, 0);
		regmap_update_bits(imx6_pcie->iomuxc_gpr,
				IMX95_PE0_PM_CTRL,
				IMX95_PCIE_PM_PME_REQ,
				IMX95_PCIE_PM_PME_REQ);
		regmap_update_bits(imx6_pcie->iomuxc_gpr,
				IMX95_PE0_PM_CTRL,
				IMX95_PCIE_PM_PME_REQ, 0);
		regmap_update_bits(imx6_pcie->iomuxc_gpr,
				IMX95_PE0_PM_CTRL,
				IMX95_PCIE_PM_READY_ENTR_L23,
				IMX95_PCIE_PM_READY_ENTR_L23);

		/* check the L2 is entered or not. */
		if (regmap_read_poll_timeout(imx6_pcie->iomuxc_gpr,
					IMX95_PE0_PM_STS, val,
					val & IMX95_PCIE_PM_LINKST_IN_L2,
					10, 10000))
			dev_err(dev, "PCIE%d can't enter into L2.\n",
					imx6_pcie->controller_id);
		else
			dev_info(dev, "PCIE%d enter into L2 (STTS:0x%08x).\n",
					imx6_pcie->controller_id, val);

		break;
	default:
		dev_err(dev, "PME_Turn_Off not implemented\n");
		return;
	}

	/*
	 * Components with an upstream port must respond to
	 * PME_Turn_Off with PME_TO_Ack but we can't check.
	 *
	 * The standard recommends a 1-10ms timeout after which to
	 * proceed anyway as if acks were received.
	 */
pm_turnoff_sleep:
	usleep_range(1000, 10000);
}

static void imx6_pcie_msi_save_restore(struct imx6_pcie *imx6_pcie, bool save)
{
	u8 offset;
	u16 val;
	struct dw_pcie *pci = imx6_pcie->pci;

	if (pci_msi_enabled()) {
		offset = dw_pcie_find_capability(pci, PCI_CAP_ID_MSI);
		if (save) {
			val = dw_pcie_readw_dbi(pci, offset + PCI_MSI_FLAGS);
			imx6_pcie->msi_ctrl = val;
		} else {
			dw_pcie_dbi_ro_wr_en(pci);
			val = imx6_pcie->msi_ctrl;
			dw_pcie_writew_dbi(pci, offset + PCI_MSI_FLAGS, val);
			dw_pcie_dbi_ro_wr_dis(pci);
		}
	}
}

static int imx6_pcie_suspend_noirq(struct device *dev)
{
	struct imx6_pcie *imx6_pcie = dev_get_drvdata(dev);
	struct dw_pcie_rp *pp = &imx6_pcie->pci->pp;

	if (!(imx6_pcie->drvdata->flags & IMX6_PCIE_FLAG_SUPPORTS_SUSPEND))
		return 0;

	if (unlikely(imx6_pcie->drvdata->variant == IMX6Q)) {
		/*
		 * L2 can exit by 'reset' or Inband beacon (from remote EP)
		 * toggling phy_powerdown has same effect as 'inband beacon'
		 * So, toggle bit18 of GPR1, used as a workaround of errata
		 * ERR005723 "PCIe PCIe does not support L2 Power Down"
		 */
		regmap_update_bits(imx6_pcie->iomuxc_gpr, IOMUXC_GPR1,
				   IMX6Q_GPR1_PCIE_TEST_PD,
				   IMX6Q_GPR1_PCIE_TEST_PD);
	} else {
		imx6_pcie_msi_save_restore(imx6_pcie, true);
		imx6_pcie_pm_turnoff(imx6_pcie);
		imx6_pcie_stop_link(imx6_pcie->pci);
		imx6_pcie_host_exit(pp);
	}

	return 0;
}

static int imx6_pcie_resume_noirq(struct device *dev)
{
	int ret;
	struct imx6_pcie *imx6_pcie = dev_get_drvdata(dev);
	struct dw_pcie_rp *pp = &imx6_pcie->pci->pp;

	if (!(imx6_pcie->drvdata->flags & IMX6_PCIE_FLAG_SUPPORTS_SUSPEND))
		return 0;

	if (unlikely(imx6_pcie->drvdata->variant == IMX6Q)) {
		/*
		 * L2 can exit by 'reset' or Inband beacon (from remote EP)
		 * toggling phy_powerdown has same effect as 'inband beacon'
		 * So, toggle bit18 of GPR1, used as a workaround of errata
		 * ERR005723 "PCIe PCIe does not support L2 Power Down"
		 */
		regmap_update_bits(imx6_pcie->iomuxc_gpr, IOMUXC_GPR1,
				IMX6Q_GPR1_PCIE_TEST_PD, 0);
	} else {
		ret = imx6_pcie_host_init(pp);
		if (ret)
			return ret;
		imx6_pcie_msi_save_restore(imx6_pcie, false);
		dw_pcie_setup_rc(pp);

		if (imx6_pcie->link_is_up)
			imx6_pcie_start_link(imx6_pcie->pci);
	}

	return 0;
}

static int imx6_pcie_suspend(struct device *dev)
{
	struct imx6_pcie *imx6_pcie = dev_get_drvdata(dev);

	if (imx6_pcie->host_wake_irq >= 0)
		enable_irq_wake(imx6_pcie->host_wake_irq);

	return 0;
}

static int imx6_pcie_resume(struct device *dev)
{
	struct imx6_pcie *imx6_pcie = dev_get_drvdata(dev);

	if (imx6_pcie->host_wake_irq >= 0)
		disable_irq_wake(imx6_pcie->host_wake_irq);

	return 0;
}

static const struct dev_pm_ops imx6_pcie_pm_ops = {
	NOIRQ_SYSTEM_SLEEP_PM_OPS(imx6_pcie_suspend_noirq,
				  imx6_pcie_resume_noirq)
	SYSTEM_SLEEP_PM_OPS(imx6_pcie_suspend, imx6_pcie_resume)
};

irqreturn_t host_wake_irq_handler(int irq, void *priv)
{
	struct imx6_pcie *imx6_pcie = priv;
	struct device *dev = imx6_pcie->pci->dev;

	dev_dbg(dev, "%s: host wakeup by pcie", __func__);

	/* Notify PM core we are wakeup source */
	pm_wakeup_event(dev, 0);
	pm_system_wakeup();

	return IRQ_HANDLED;
}

static int __init __maybe_unused imx6_pcie_compliance_test_enable(char *str)
{
	if (!strcmp(str, "yes")) {
		pr_info("Enable the i.MX PCIe compliance tests mode.\n");
		imx6_pcie_cz_enabled = 1;
	}
	return 1;
}

__setup("pcie_cz_enabled=", imx6_pcie_compliance_test_enable);

static int imx6_pcie_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct dw_pcie *pci;
	struct imx6_pcie *imx6_pcie;
	struct device_node *np;
	struct resource *dbi_base;
	struct device_node *node = dev->of_node;
	struct gpio_desc *host_wake_gpio;
	int ret;
	u16 val;

	imx6_pcie = devm_kzalloc(dev, sizeof(*imx6_pcie), GFP_KERNEL);
	if (!imx6_pcie)
		return -ENOMEM;

	pci = devm_kzalloc(dev, sizeof(*pci), GFP_KERNEL);
	if (!pci)
		return -ENOMEM;

	pci->dev = dev;
	pci->ops = &dw_pcie_ops;
	pci->pp.ops = &imx6_pcie_host_ops;

	imx6_pcie->pci = pci;
	imx6_pcie->drvdata = of_device_get_match_data(dev);

	/* Find the PHY if one is defined, only imx7d uses it */
	np = of_parse_phandle(node, "fsl,imx7d-pcie-phy", 0);
	if (np) {
		struct resource res;

		ret = of_address_to_resource(np, 0, &res);
		if (ret) {
			dev_err(dev, "Unable to map PCIe PHY\n");
			return ret;
		}
		imx6_pcie->phy_base = devm_ioremap_resource(dev, &res);
		if (IS_ERR(imx6_pcie->phy_base))
			return PTR_ERR(imx6_pcie->phy_base);
	}

	pci->dbi_base = devm_platform_get_and_ioremap_resource(pdev, 0, &dbi_base);
	if (IS_ERR(pci->dbi_base))
		return PTR_ERR(pci->dbi_base);

	if (of_property_read_u32(node, "hsio-cfg", &imx6_pcie->hsio_cfg))
		imx6_pcie->hsio_cfg = 0;
	if (of_property_read_u32(node, "local-addr", &imx6_pcie->local_addr))
		imx6_pcie->local_addr = 0;

	/* Fetch GPIOs */
	imx6_pcie->reset_gpio = of_get_named_gpio(node, "reset-gpio", 0);
	imx6_pcie->gpio_active_high = of_property_read_bool(node,
						"reset-gpio-active-high");
	if (gpio_is_valid(imx6_pcie->reset_gpio)) {
		ret = devm_gpio_request_one(dev, imx6_pcie->reset_gpio,
				imx6_pcie->gpio_active_high ?
					GPIOF_OUT_INIT_HIGH :
					GPIOF_OUT_INIT_LOW,
				"PCIe reset");
		if (ret) {
			dev_err(dev, "unable to get reset gpio\n");
			return ret;
		}
	} else if (imx6_pcie->reset_gpio == -EPROBE_DEFER) {
		return imx6_pcie->reset_gpio;
	}

	/* Fetch clocks */
	imx6_pcie->pcie_bus = devm_clk_get(dev, "pcie_bus");
	if (IS_ERR(imx6_pcie->pcie_bus))
		return dev_err_probe(dev, PTR_ERR(imx6_pcie->pcie_bus),
				     "pcie_bus clock source missing or invalid\n");

	imx6_pcie->pcie = devm_clk_get(dev, "pcie");
	if (IS_ERR(imx6_pcie->pcie))
		return dev_err_probe(dev, PTR_ERR(imx6_pcie->pcie),
				     "pcie clock source missing or invalid\n");

	switch (imx6_pcie->drvdata->variant) {
	case IMX6SX:
	case IMX6SX_EP:
		imx6_pcie->pcie_inbound_axi = devm_clk_get(dev,
							   "pcie_inbound_axi");
		if (IS_ERR(imx6_pcie->pcie_inbound_axi))
			return dev_err_probe(dev, PTR_ERR(imx6_pcie->pcie_inbound_axi),
					     "pcie_inbound_axi clock missing or invalid\n");
		break;
	case IMX8MQ:
	case IMX8MQ_EP:
		imx6_pcie->pcie_aux = devm_clk_get(dev, "pcie_aux");
		if (IS_ERR(imx6_pcie->pcie_aux))
			return dev_err_probe(dev, PTR_ERR(imx6_pcie->pcie_aux),
					     "pcie_aux clock source missing or invalid\n");
		fallthrough;
	case IMX7D:
	case IMX7D_EP:
		if (dbi_base->start == IMX8MQ_PCIE2_BASE_ADDR)
			imx6_pcie->controller_id = 1;

		imx6_pcie->pciephy_reset = devm_reset_control_get_exclusive(dev,
									    "pciephy");
		if (IS_ERR(imx6_pcie->pciephy_reset)) {
			dev_err(dev, "Failed to get PCIEPHY reset control\n");
			return PTR_ERR(imx6_pcie->pciephy_reset);
		}

		imx6_pcie->apps_reset = devm_reset_control_get_exclusive(dev,
									 "apps");
		if (IS_ERR(imx6_pcie->apps_reset)) {
			dev_err(dev, "Failed to get PCIE APPS reset control\n");
			return PTR_ERR(imx6_pcie->apps_reset);
		}
		break;
	case IMX8MM:
	case IMX8MM_EP:
	case IMX8MP:
	case IMX8MP_EP:
		imx6_pcie->pcie_aux = devm_clk_get(dev, "pcie_aux");
		if (IS_ERR(imx6_pcie->pcie_aux))
			return dev_err_probe(dev, PTR_ERR(imx6_pcie->pcie_aux),
					     "pcie_aux clock source missing or invalid\n");
		imx6_pcie->apps_reset = devm_reset_control_get_exclusive(dev,
									 "apps");
		if (IS_ERR(imx6_pcie->apps_reset))
			return dev_err_probe(dev, PTR_ERR(imx6_pcie->apps_reset),
					     "failed to get pcie apps reset control\n");

		imx6_pcie->phy = devm_phy_get(dev, "pcie-phy");
		if (IS_ERR(imx6_pcie->phy))
			return dev_err_probe(dev, PTR_ERR(imx6_pcie->phy),
					     "failed to get pcie phy\n");

		break;
	case IMX8QM:
	case IMX8QM_EP:
	case IMX8QXP:
	case IMX8QXP_EP:
		if (dbi_base->start == IMX8_HSIO_PCIEB_BASE_ADDR)
			imx6_pcie->controller_id = 1;

		imx6_pcie->pcie_inbound_axi = devm_clk_get(&pdev->dev,
				"pcie_inbound_axi");
		if (IS_ERR(imx6_pcie->pcie_inbound_axi)) {
			dev_err(&pdev->dev,
				"pcie clock source missing or invalid\n");
			return PTR_ERR(imx6_pcie->pcie_inbound_axi);
		}

		imx6_pcie->iomuxc_gpr = syscon_regmap_lookup_by_phandle(node,
				"ctrl-csr");
		if (IS_ERR(imx6_pcie->iomuxc_gpr)) {
			dev_err(dev, "unable to find hsio ctrl registers\n");
			return PTR_ERR(imx6_pcie->iomuxc_gpr);
		}

		imx6_pcie->phy = devm_phy_get(dev, "pcie-phy");
		if (IS_ERR(imx6_pcie->phy))
			return dev_err_probe(dev, PTR_ERR(imx6_pcie->phy),
					     "failed to get pcie phy\n");

		break;
	case IMX95:
		imx6_pcie->pcie_ref = devm_clk_get(dev, "ref");
		if (IS_ERR(imx6_pcie->pcie_ref))
			return dev_err_probe(dev, PTR_ERR(imx6_pcie->pcie_ref),
					     "pcie_ref clock source missing or invalid\n");
		fallthrough;
	case IMX95_EP:
		if (dbi_base->start == IMX95_PCIE2_BASE_ADDR)
			imx6_pcie->controller_id = 1;
		of_property_read_u32(node, "fsl,refclk-pad-mode",
				&imx6_pcie->refclk_pad_mode);
		imx6_pcie->pcie_aux = devm_clk_get(dev, "pcie_aux");
		if (IS_ERR(imx6_pcie->pcie_aux))
			return dev_err_probe(dev, PTR_ERR(imx6_pcie->pcie_aux),
					     "pcie_aux clock source missing or invalid\n");
		imx6_pcie->iomuxc_gpr = syscon_regmap_lookup_by_phandle(node,
				"ctrl-ssr");
		if (IS_ERR(imx6_pcie->iomuxc_gpr)) {
			dev_err(dev, "unable to find hsio ctrl registers\n");
			return PTR_ERR(imx6_pcie->iomuxc_gpr);
		}
		break;
	default:
		break;
	}

	/* Don't fetch the pcie_phy clock, if it has abstract PHY driver */
	if (imx6_pcie->phy == NULL) {
		imx6_pcie->pcie_phy = devm_clk_get(dev, "pcie_phy");
		if (IS_ERR(imx6_pcie->pcie_phy))
			return dev_err_probe(dev, PTR_ERR(imx6_pcie->pcie_phy),
					     "pcie_phy clock source missing or invalid\n");
	}


	/* Grab turnoff reset */
	imx6_pcie->turnoff_reset = devm_reset_control_get_optional_exclusive(dev, "turnoff");
	if (IS_ERR(imx6_pcie->turnoff_reset)) {
		dev_err(dev, "Failed to get TURNOFF reset control\n");
		return PTR_ERR(imx6_pcie->turnoff_reset);
	}

	/* Grab GPR config register range */
	if (imx6_pcie->iomuxc_gpr == NULL) {
		imx6_pcie->iomuxc_gpr =
			 syscon_regmap_lookup_by_compatible(imx6_pcie->drvdata->gpr);
		if (IS_ERR(imx6_pcie->iomuxc_gpr)) {
			dev_err(dev, "unable to find iomuxc registers\n");
			return PTR_ERR(imx6_pcie->iomuxc_gpr);
		}
	}

	if (imx6_check_flag(imx6_pcie, IMX6_PCIE_FLAG_SUPPORT_64BIT))
		dma_set_mask_and_coherent(dev, DMA_BIT_MASK(64));

	/* Grab PCIe PHY Tx Settings */
	if (of_property_read_u32(node, "fsl,tx-deemph-gen1",
				 &imx6_pcie->tx_deemph_gen1))
		imx6_pcie->tx_deemph_gen1 = 0;

	if (of_property_read_u32(node, "fsl,tx-deemph-gen2-3p5db",
				 &imx6_pcie->tx_deemph_gen2_3p5db))
		imx6_pcie->tx_deemph_gen2_3p5db = 0;

	if (of_property_read_u32(node, "fsl,tx-deemph-gen2-6db",
				 &imx6_pcie->tx_deemph_gen2_6db))
		imx6_pcie->tx_deemph_gen2_6db = 20;

	if (of_property_read_u32(node, "fsl,tx-swing-full",
				 &imx6_pcie->tx_swing_full))
		imx6_pcie->tx_swing_full = 127;

	if (of_property_read_u32(node, "fsl,tx-swing-low",
				 &imx6_pcie->tx_swing_low))
		imx6_pcie->tx_swing_low = 127;

	/* Limit link speed */
	pci->link_gen = 1;
	of_property_read_u32(node, "fsl,max-link-speed", &pci->link_gen);

	imx6_pcie->vpcie = devm_regulator_get_optional(&pdev->dev, "vpcie");
	if (IS_ERR(imx6_pcie->vpcie)) {
		if (PTR_ERR(imx6_pcie->vpcie) != -ENODEV)
			return PTR_ERR(imx6_pcie->vpcie);
		imx6_pcie->vpcie = NULL;
	}

	imx6_pcie->vph = devm_regulator_get_optional(&pdev->dev, "vph");
	if (IS_ERR(imx6_pcie->vph)) {
		if (PTR_ERR(imx6_pcie->vph) != -ENODEV)
			return PTR_ERR(imx6_pcie->vph);
		imx6_pcie->vph = NULL;
	}

	platform_set_drvdata(pdev, imx6_pcie);

	ret = imx6_pcie_attach_pd(dev);
	if (ret)
		return ret;

	if (imx6_pcie->drvdata->mode == DW_PCIE_EP_TYPE) {
		ret = imx6_add_pcie_ep(imx6_pcie, pdev);
		if (ret < 0)
			return ret;
	} else {
		ret = dw_pcie_host_init(&pci->pp);
		if (ret < 0)
			return ret;

		if (pci_msi_enabled()) {
			u8 offset = dw_pcie_find_capability(pci, PCI_CAP_ID_MSI);

			val = dw_pcie_readw_dbi(pci, offset + PCI_MSI_FLAGS);
			val |= PCI_MSI_FLAGS_ENABLE;
			dw_pcie_writew_dbi(pci, offset + PCI_MSI_FLAGS, val);
		}

		/* host wakeup support */
		imx6_pcie->host_wake_irq = -1;
		host_wake_gpio = devm_gpiod_get_optional(dev, "host-wake", GPIOD_IN);
		if (IS_ERR(host_wake_gpio))
			return PTR_ERR(host_wake_gpio);

		if (host_wake_gpio != NULL) {
			imx6_pcie->host_wake_irq = gpiod_to_irq(host_wake_gpio);
			ret = devm_request_threaded_irq(dev, imx6_pcie->host_wake_irq, NULL,
							host_wake_irq_handler,
							IRQF_ONESHOT | IRQF_TRIGGER_FALLING,
							"host_wake", imx6_pcie);
			if (ret) {
				dev_err(dev, "Failed to request host_wake_irq %d (%d)\n",
					imx6_pcie->host_wake_irq, ret);
				imx6_pcie->host_wake_irq = -1;
				return ret;
			}

			if (device_init_wakeup(dev, true)) {
				dev_err(dev, "fail to init host_wake_irq\n");
				imx6_pcie->host_wake_irq = -1;
				return ret;
			}
		}
	}

	return 0;
}

static void imx6_pcie_shutdown(struct platform_device *pdev)
{
	struct imx6_pcie *imx6_pcie = platform_get_drvdata(pdev);

	if (imx6_pcie->host_wake_irq >= 0) {
		device_init_wakeup(&pdev->dev, false);
		disable_irq(imx6_pcie->host_wake_irq);
		imx6_pcie->host_wake_irq = -1;
	}

	/* bring down link, so bootloader gets clean state in case of reboot */
	imx6_pcie_assert_core_reset(imx6_pcie);
}

static const struct imx6_pcie_drvdata drvdata[] = {
	[IMX6Q] = {
		.variant = IMX6Q,
		.flags = IMX6_PCIE_FLAG_IMX6_PHY |
			 IMX6_PCIE_FLAG_SUPPORTS_SUSPEND |
			 IMX6_PCIE_FLAG_IMX6_SPEED_CHANGE,
		.dbi_length = 0x200,
		.gpr = "fsl,imx6q-iomuxc-gpr",
	},
	[IMX6Q_EP] = {
		.variant = IMX6Q_EP,
		.mode = DW_PCIE_EP_TYPE,
		.flags = IMX6_PCIE_FLAG_IMX6_PHY,
		.gpr = "fsl,imx6q-iomuxc-gpr",
	},
	[IMX6SX] = {
		.variant = IMX6SX,
		.flags = IMX6_PCIE_FLAG_IMX6_PHY |
			 IMX6_PCIE_FLAG_IMX6_SPEED_CHANGE |
			 IMX6_PCIE_FLAG_SUPPORTS_SUSPEND,
		.gpr = "fsl,imx6q-iomuxc-gpr",
	},
	[IMX6SX_EP] = {
		.variant = IMX6SX_EP,
		.mode = DW_PCIE_EP_TYPE,
		.flags = IMX6_PCIE_FLAG_IMX6_PHY,
		.gpr = "fsl,imx6q-iomuxc-gpr",
	},
	[IMX6QP] = {
		.variant = IMX6QP,
		.flags = IMX6_PCIE_FLAG_IMX6_PHY |
			 IMX6_PCIE_FLAG_IMX6_SPEED_CHANGE |
			 IMX6_PCIE_FLAG_SUPPORTS_SUSPEND,
		.dbi_length = 0x200,
		.gpr = "fsl,imx6q-iomuxc-gpr",
	},
	[IMX6QP_EP] = {
		.variant = IMX6QP_EP,
		.mode = DW_PCIE_EP_TYPE,
		.flags = IMX6_PCIE_FLAG_IMX6_PHY,
		.gpr = "fsl,imx6q-iomuxc-gpr",
	},
	[IMX7D] = {
		.variant = IMX7D,
		.flags = IMX6_PCIE_FLAG_SUPPORTS_SUSPEND,
		.gpr = "fsl,imx7d-iomuxc-gpr",
	},
	[IMX7D_EP] = {
		.variant = IMX7D_EP,
		.mode = DW_PCIE_EP_TYPE,
		.gpr = "fsl,imx7d-iomuxc-gpr",
	},
	[IMX8MQ] = {
		.variant = IMX8MQ,
		.flags = IMX6_PCIE_FLAG_SUPPORTS_SUSPEND,
		.gpr = "fsl,imx8mq-iomuxc-gpr",
	},
	[IMX8MM] = {
		.variant = IMX8MM,
		.flags = IMX6_PCIE_FLAG_SUPPORTS_SUSPEND,
		.gpr = "fsl,imx8mm-iomuxc-gpr",
	},
	[IMX8MP] = {
		.variant = IMX8MP,
		.flags = IMX6_PCIE_FLAG_SUPPORTS_SUSPEND,
		.gpr = "fsl,imx8mp-iomuxc-gpr",
	},
	[IMX8MQ_EP] = {
		.variant = IMX8MQ_EP,
		.mode = DW_PCIE_EP_TYPE,
		.gpr = "fsl,imx8mq-iomuxc-gpr",
	},
	[IMX8MM_EP] = {
		.variant = IMX8MM_EP,
		.mode = DW_PCIE_EP_TYPE,
		.gpr = "fsl,imx8mm-iomuxc-gpr",
	},
	[IMX8MP_EP] = {
		.variant = IMX8MP_EP,
		.mode = DW_PCIE_EP_TYPE,
		.gpr = "fsl,imx8mp-iomuxc-gpr",
	},
	[IMX8QM] = {
		.variant = IMX8QM,
		.flags = IMX6_PCIE_FLAG_SUPPORTS_SUSPEND |
			 IMX6_PCIE_FLAG_IMX6_CPU_ADDR_FIXUP,
	},
	[IMX8QM_EP] = {
		.variant = IMX8QM_EP,
		.mode = DW_PCIE_EP_TYPE,
		.flags = IMX6_PCIE_FLAG_IMX6_CPU_ADDR_FIXUP,
	},
	[IMX8QXP] = {
		.variant = IMX8QXP,
		.flags = IMX6_PCIE_FLAG_SUPPORTS_SUSPEND |
			 IMX6_PCIE_FLAG_IMX6_CPU_ADDR_FIXUP,
	},
	[IMX8QXP_EP] = {
		.variant = IMX8QXP_EP,
		.mode = DW_PCIE_EP_TYPE,
		.flags = IMX6_PCIE_FLAG_IMX6_CPU_ADDR_FIXUP,
	},
	[IMX95] = {
		.variant = IMX95,
		.flags = IMX6_PCIE_FLAG_SUPPORTS_SUSPEND,
	},
	[IMX95_EP] = {
		.variant = IMX95_EP,
		.mode = DW_PCIE_EP_TYPE,
		.flags = IMX6_PCIE_FLAG_SUPPORT_64BIT,
	},
};

static const struct of_device_id imx6_pcie_of_match[] = {
	{ .compatible = "fsl,imx6q-pcie",  .data = &drvdata[IMX6Q],  },
	{ .compatible = "fsl,imx6q-pcie-ep", .data = &drvdata[IMX6Q_EP], },
	{ .compatible = "fsl,imx6sx-pcie", .data = &drvdata[IMX6SX], },
	{ .compatible = "fsl,imx6sx-pcie-ep", .data = &drvdata[IMX6SX_EP], },
	{ .compatible = "fsl,imx6qp-pcie", .data = &drvdata[IMX6QP], },
	{ .compatible = "fsl,imx6qp-pcie-ep", .data = &drvdata[IMX6QP_EP], },
	{ .compatible = "fsl,imx7d-pcie",  .data = &drvdata[IMX7D],  },
	{ .compatible = "fsl,imx7d-pcie-ep", .data = &drvdata[IMX7D_EP], },
	{ .compatible = "fsl,imx8mq-pcie", .data = &drvdata[IMX8MQ], },
	{ .compatible = "fsl,imx8mm-pcie", .data = &drvdata[IMX8MM], },
	{ .compatible = "fsl,imx8mp-pcie", .data = &drvdata[IMX8MP], },
	{ .compatible = "fsl,imx8mq-pcie-ep", .data = &drvdata[IMX8MQ_EP], },
	{ .compatible = "fsl,imx8mm-pcie-ep", .data = &drvdata[IMX8MM_EP], },
	{ .compatible = "fsl,imx8mp-pcie-ep", .data = &drvdata[IMX8MP_EP], },
	{ .compatible = "fsl,imx8qm-pcie", .data = &drvdata[IMX8QM], },
	{ .compatible = "fsl,imx8qm-pcie-ep", .data = &drvdata[IMX8QM_EP], },
	{ .compatible = "fsl,imx8qxp-pcie", .data = &drvdata[IMX8QXP], },
	{ .compatible = "fsl,imx8qxp-pcie-ep", .data = &drvdata[IMX8QXP_EP], },
	{ .compatible = "fsl,imx95-pcie", .data = &drvdata[IMX95], },
	{ .compatible = "fsl,imx95-pcie-ep", .data = &drvdata[IMX95_EP], },
	{},
};

static struct platform_driver imx6_pcie_driver = {
	.driver = {
		.name	= "imx6q-pcie",
		.of_match_table = imx6_pcie_of_match,
		.suppress_bind_attrs = true,
		.pm = &imx6_pcie_pm_ops,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
	.probe    = imx6_pcie_probe,
	.shutdown = imx6_pcie_shutdown,
};

static void imx6_pcie_quirk(struct pci_dev *dev)
{
	struct pci_bus *bus = dev->bus;
	struct dw_pcie_rp *pp = bus->sysdata;

	/* Bus parent is the PCI bridge, its parent is this platform driver */
	if (!bus->dev.parent || !bus->dev.parent->parent)
		return;

	/* Make sure we only quirk devices associated with this driver */
	if (bus->dev.parent->parent->driver != &imx6_pcie_driver.driver)
		return;

	if (pci_is_root_bus(bus)) {
		struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
		struct imx6_pcie *imx6_pcie = to_imx6_pcie(pci);

		/*
		 * Limit config length to avoid the kernel reading beyond
		 * the register set and causing an abort on i.MX 6Quad
		 */
		if (imx6_pcie->drvdata->dbi_length) {
			dev->cfg_size = imx6_pcie->drvdata->dbi_length;
			dev_info(&dev->dev, "Limiting cfg_size to %d\n",
					dev->cfg_size);
		}
	}
}
DECLARE_PCI_FIXUP_CLASS_HEADER(PCI_VENDOR_ID_SYNOPSYS, 0xabcd,
			PCI_CLASS_BRIDGE_PCI, 8, imx6_pcie_quirk);

static int __init imx6_pcie_init(void)
{
#ifdef CONFIG_ARM
	struct device_node *np;

	np = of_find_matching_node(NULL, imx6_pcie_of_match);
	if (!np)
		return -ENODEV;
	of_node_put(np);

	/*
	 * Since probe() can be deferred we need to make sure that
	 * hook_fault_code is not called after __init memory is freed
	 * by kernel and since imx6q_pcie_abort_handler() is a no-op,
	 * we can install the handler here without risking it
	 * accessing some uninitialized driver state.
	 */
	hook_fault_code(8, imx6q_pcie_abort_handler, SIGBUS, 0,
			"external abort on non-linefetch");
#endif

	return platform_driver_register(&imx6_pcie_driver);
}
device_initcall(imx6_pcie_init);
MODULE_LICENSE("GPL v2");
