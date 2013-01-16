/*
 * Freescale eSDHC controller driver generics for OF and pltfm.
 *
 * Copyright 2007, 2012-2013 Freescale Semiconductor, Inc.
 * Copyright (c) 2009 MontaVista Software, Inc.
 * Copyright (c) 2010 Pengutronix e.K.
 *   Author: Wolfram Sang <w.sang@pengutronix.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 */

#ifndef _DRIVERS_MMC_SDHCI_ESDHC_H
#define _DRIVERS_MMC_SDHCI_ESDHC_H

/*
 * Ops and quirks for the Freescale eSDHC controller.
 */

#define ESDHC_DEFAULT_QUIRKS	(SDHCI_QUIRK_FORCE_BLK_SZ_2048 | \
				SDHCI_QUIRK_NO_BUSY_IRQ | \
				SDHCI_QUIRK_NONSTANDARD_CLOCK | \
				SDHCI_QUIRK_DATA_TIMEOUT_USES_SDCLK | \
				SDHCI_QUIRK_PIO_NEEDS_DELAY | \
				SDHCI_QUIRK_RESTORE_IRQS_AFTER_RESET)

#define ESDHC_SYSTEM_CONTROL	0x2c
#define ESDHC_CLOCK_MASK	0x0000fff0
#define ESDHC_PREDIV_SHIFT	8
#define ESDHC_DIVIDER_SHIFT	4
#define ESDHC_CLOCK_PEREN	0x00000004
#define ESDHC_CLOCK_HCKEN	0x00000002
#define ESDHC_CLOCK_IPGEN	0x00000001

/* pltfm-specific */
#define ESDHC_HOST_CONTROL_LE	0x20

/* OF-specific */
#define ESDHC_DMA_SYSCTL	0x40c
#define ESDHC_DMA_SNOOP		0x00000040

#define ESDHC_HOST_CONTROL_RES	0x05

#define	SDHCI_CTRL_D3CD			0x08

/* VENDOR SPEC register */
#define SDHCI_VENDOR_SPEC		0xC0
#define SDHCI_VENDOR_SPEC_VSELECT	(1 << 1)
#define SDHCI_VENDOR_SPEC_FRC_SDCLK_ON	(1 << 8)
#define SDHCI_VENDOR_SPEC_SDIO_QUIRK	0x00000002
#define SDHCI_WTMK_LVL			0x44

/* mix control register */
#define SDHCI_MIX_CTRL			0x48
#define SDHCI_MIX_CTRL_AC23EN	(1 << 7)
#define SDHCI_MIX_CTRL_DDREN	(1 << 3)
#define SDHCI_MIX_CTRL_EXE_TUNE		(1 << 22)
#define SDHCI_MIX_CTRL_SMPCLK_SEL	(1 << 23)
#define SDHCI_MIX_CTRL_AUTO_TUNE	(1 << 24)
#define SDHCI_MIX_CTRL_FBCLK_SEL	(1 << 25)

/* protocol control register */
#define SDHCI_PROT_CTRL_DMA_MASK	(3 << 8)
#define SDHCI_PROT_CTRL_LCTL		(1 << 0)
#define SDHCI_PROT_CTRL_DTW_MASK	(3 << 1)
#define SDHCI_PROT_CTRL_1BIT		(0)
#define SDHCI_PROT_CTRL_4BIT		(1 << 1)
#define SDHCI_PROT_CTRL_8BIT		(1 << 2)

/* dll control register */
#define SDHCI_DLL_CTRL					0x60
#define SDHCI_DLL_OVERRIDE_OFFSET		9
#define SDHCI_DLL_OVERRIDE_EN_OFFSET	8

/* tune control register */
#define SDHCI_TUNE_CTRL_STATUS			0x68
#define SDHCI_TUNE_CTRL_STEP			1
#define SDHCI_TUNE_CTRL_MIN				0
#define SDHCI_TUNE_CTRL_MAX				((1 << 7) - 1)

/* sys control register */
#define SDHCI_SYS_CTRL					0x2C
#define SDHCI_SYS_CTRL_RSTA				(1 << 24)

/*
 * There is an INT DMA ERR mis-match between eSDHC and STD SDHC SPEC:
 * Bit25 is used in STD SPEC, and is reserved in fsl eSDHC design,
 * but bit28 is used as the INT DMA ERR in fsl eSDHC design.
 * Define this macro DMA error INT for fsl eSDHC
 */
#define SDHCI_INT_VENDOR_SPEC_DMA_ERR	0x10000000

/* pinctrl state */
#define ESDHC_PINCTRL_STATE_100MHZ		"state_100mhz"
#define ESDHC_PINCTRL_STATE_200MHZ		"state_200mhz"

enum imx_esdhc_type {
	IMX25_ESDHC,
	IMX35_ESDHC,
	IMX51_ESDHC,
	IMX53_ESDHC,
	IMX6Q_USDHC,
};

struct pltfm_imx_data {
	int flags;
	u32 scratchpad;
	enum imx_esdhc_type devtype;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_current;
	struct pinctrl_state *pins_default;
	struct pinctrl_state *pins_100mhz;
	struct pinctrl_state *pins_200mhz;
	struct esdhc_platform_data boarddata;
	struct clk *clk_ipg;
	struct clk *clk_ahb;
	struct clk *clk_per;
	u32 is_ddr;
	u32 is_tuned_clk;
	u32 uhs_mode;
};

static inline int is_imx25_esdhc(struct pltfm_imx_data *data)
{
	return data->devtype == IMX25_ESDHC;
}

static inline int is_imx35_esdhc(struct pltfm_imx_data *data)
{
	return data->devtype == IMX35_ESDHC;
}

static inline int is_imx51_esdhc(struct pltfm_imx_data *data)
{
	return data->devtype == IMX51_ESDHC;
}

static inline int is_imx53_esdhc(struct pltfm_imx_data *data)
{
	return data->devtype == IMX53_ESDHC;
}

static inline int is_imx6q_usdhc(struct pltfm_imx_data *data)
{
	return data->devtype == IMX6Q_USDHC;
}

static inline void esdhc_set_clock(struct sdhci_host *host, unsigned int clock)
{
	int pre_div = 2;
	int div = 1;
	u32 temp;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct pltfm_imx_data *imx_data = pltfm_host->priv;

	if (clock == 0)
		goto out;

	if (is_imx6q_usdhc(imx_data)) {
		pre_div = 1;
		if (imx_data->is_ddr)
			pre_div = 2;
	}

	temp = sdhci_readl(host, ESDHC_SYSTEM_CONTROL);
	temp &= ~(ESDHC_CLOCK_IPGEN | ESDHC_CLOCK_HCKEN | ESDHC_CLOCK_PEREN
		| ESDHC_CLOCK_MASK);
	sdhci_writel(host, temp, ESDHC_SYSTEM_CONTROL);

	while (host->max_clk / pre_div / 16 > clock && pre_div < 256)
		pre_div *= 2;

	while (host->max_clk / pre_div / div > clock && div < 16)
		div++;

	dev_dbg(mmc_dev(host->mmc), "desired SD clock: %d, actual: %d\n",
		clock, host->max_clk / pre_div / div);

	if (imx_data->is_ddr)
		pre_div >>= 2;
	else
		pre_div >>= 1;
	div--;

	temp = sdhci_readl(host, ESDHC_SYSTEM_CONTROL);
	temp |= (ESDHC_CLOCK_IPGEN | ESDHC_CLOCK_HCKEN | ESDHC_CLOCK_PEREN
		| (div << ESDHC_DIVIDER_SHIFT)
		| (pre_div << ESDHC_PREDIV_SHIFT));
	sdhci_writel(host, temp, ESDHC_SYSTEM_CONTROL);
	mdelay(1);
out:
	host->clock = clock;
}

#endif /* _DRIVERS_MMC_SDHCI_ESDHC_H */
