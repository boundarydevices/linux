/*
 * Freescale eSDHC i.MX controller driver for the platform bus.
 *
 * derived from the OF-version.
 *
 * Copyright (c) 2010 Pengutronix e.K.
 *   Author: Wolfram Sang <w.sang@pengutronix.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 */

#include <linux/io.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sdio.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <mach/esdhc.h>
#include "sdhci.h"
#include "sdhci-pltfm.h"
#include "sdhci-esdhc.h"

/*
 * The CMDTYPE of the CMD register (offset 0xE) should be set to
 * "11" when the STOP CMD12 is issued on imx53 to abort one
 * open ended multi-blk IO. Otherwise the TC INT wouldn't
 * be generated.
 * In exact block transfer, the controller doesn't complete the
 * operations automatically as required at the end of the
 * transfer and remains on hold if the abort command is not sent.
 * As a result, the TC flag is not asserted and SW  received timeout
 * exeception. Bit1 of Vendor Spec registor is used to fix it.
 */
#define ESDHC_FLAG_MULTIBLK_NO_INT	(1 << 1)

#define SDHCI_TUNING_BLOCK_PATTERN_LEN	64

static struct platform_device_id imx_esdhc_devtype[] = {
	{
		.name = "sdhci-esdhc-imx25",
		.driver_data = IMX25_ESDHC,
	}, {
		.name = "sdhci-esdhc-imx35",
		.driver_data = IMX35_ESDHC,
	}, {
		.name = "sdhci-esdhc-imx51",
		.driver_data = IMX51_ESDHC,
	}, {
		.name = "sdhci-esdhc-imx53",
		.driver_data = IMX53_ESDHC,
	}, {
		.name = "sdhci-usdhc-imx6q",
		.driver_data = IMX6Q_USDHC,
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(platform, imx_esdhc_devtype);

static const struct of_device_id imx_esdhc_dt_ids[] = {
	{ .compatible = "fsl,imx25-esdhc", .data = &imx_esdhc_devtype[IMX25_ESDHC], },
	{ .compatible = "fsl,imx35-esdhc", .data = &imx_esdhc_devtype[IMX35_ESDHC], },
	{ .compatible = "fsl,imx51-esdhc", .data = &imx_esdhc_devtype[IMX51_ESDHC], },
	{ .compatible = "fsl,imx53-esdhc", .data = &imx_esdhc_devtype[IMX53_ESDHC], },
	{ .compatible = "fsl,imx6q-usdhc", .data = &imx_esdhc_devtype[IMX6Q_USDHC], },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, imx_esdhc_dt_ids);

static inline void esdhc_clrset_le(struct sdhci_host *host, u32 mask, u32 val, int reg)
{
	void __iomem *base = host->ioaddr + (reg & ~0x3);
	u32 shift = (reg & 0x3) * 8;

	writel(((readl(base) & ~(mask << shift)) | (val << shift)), base);
}

static u32 esdhc_readl_le(struct sdhci_host *host, int reg)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct pltfm_imx_data *imx_data = pltfm_host->priv;
	struct esdhc_platform_data *boarddata = &imx_data->boarddata;

	/* fake CARD_PRESENT flag */
	u32 val = readl(host->ioaddr + reg);

	if ((reg == SDHCI_PRESENT_STATE) && is_imx6q_usdhc(imx_data)) {
		u32 fsl_prss = readl(host->ioaddr + SDHCI_PRESENT_STATE);
		/* save the least 20 bits */
		val |= fsl_prss & 0x000FFFFF;
		/* move bits for dat[0-3] */
		val |= (fsl_prss & 0x0F000000) >> 4;
		/* move cmd line bit */
		val |= (fsl_prss & 0x00800000) << 1;
	}

	if (unlikely((reg == SDHCI_PRESENT_STATE))) {
		if (boarddata && (boarddata->cd_type == ESDHC_CD_PERMANENT)) {
			val |= SDHCI_CARD_PRESENT;
		} else if (boarddata && (boarddata->cd_type == ESDHC_CD_GPIO)
				&& gpio_is_valid(boarddata->cd_gpio)) {
			if (gpio_get_value(boarddata->cd_gpio))
				/* no card, if a valid gpio says so... */
				val &= ~SDHCI_CARD_PRESENT;
			else
				/* in all other cases assume card is present */
				val |= SDHCI_CARD_PRESENT;
		}
	} else if ((reg == SDHCI_CAPABILITIES_1) && is_imx6q_usdhc(imx_data)) {
		val = SDHCI_SUPPORT_DDR50 | SDHCI_SUPPORT_SDR104
			| SDHCI_SUPPORT_SDR50;
	} else if ((reg == SDHCI_MAX_CURRENT) && is_imx6q_usdhc(imx_data)) {
		val = 0;
		val |= 0xFF << SDHCI_MAX_CURRENT_330_SHIFT;
		val |= 0xFF << SDHCI_MAX_CURRENT_300_SHIFT;
		val |= 0xFF << SDHCI_MAX_CURRENT_180_SHIFT;
	}

	if (unlikely(reg == SDHCI_CAPABILITIES)) {
		/* In FSL esdhc IC module, only bit20 is used to indicate the
		 * ADMA2 capability of esdhc, but this bit is messed up on
		 * some SOCs (e.g. on MX25, MX35 this bit is set, but they
		 * don't actually support ADMA2). So set the BROKEN_ADMA
		 * uirk on MX25/35 platforms.
		 */

		if (val & SDHCI_CAN_DO_ADMA1) {
			val &= ~SDHCI_CAN_DO_ADMA1;
			val |= SDHCI_CAN_DO_ADMA2;
		}
	}

	if (unlikely(reg == SDHCI_INT_STATUS)) {
		if (val & SDHCI_INT_VENDOR_SPEC_DMA_ERR) {
			val &= ~SDHCI_INT_VENDOR_SPEC_DMA_ERR;
			val |= SDHCI_INT_ADMA_ERROR;
		}
	}

	return val;
}

static void esdhc_writel_le(struct sdhci_host *host, u32 val, int reg)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct pltfm_imx_data *imx_data = pltfm_host->priv;
	struct esdhc_platform_data *boarddata = &imx_data->boarddata;
	u32 data;

	if (unlikely(reg == SDHCI_INT_ENABLE || reg == SDHCI_SIGNAL_ENABLE)) {
		if (boarddata->cd_type == ESDHC_CD_GPIO)
			/*
			 * These interrupts won't work with a custom
			 * card_detect gpio (only applied to mx25/35)
			 */
			val &= ~(SDHCI_INT_CARD_REMOVE | SDHCI_INT_CARD_INSERT);

		if (val & SDHCI_INT_CARD_INT) {
			/*
			 * Clear and then set D3CD bit to avoid missing the
			 * card interrupt.  This is a eSDHC controller problem
			 * so we need to apply the following workaround: clear
			 * and set D3CD bit will make eSDHC re-sample the card
			 * interrupt. In case a card interrupt was lost,
			 * re-sample it by the following steps.
			 */
			data = readl(host->ioaddr + SDHCI_HOST_CONTROL);
			data &= ~SDHCI_CTRL_D3CD;
			writel(data, host->ioaddr + SDHCI_HOST_CONTROL);
			data |= SDHCI_CTRL_D3CD;
			writel(data, host->ioaddr + SDHCI_HOST_CONTROL);
		}
	}

	if (unlikely((imx_data->flags & ESDHC_FLAG_MULTIBLK_NO_INT)
				&& (reg == SDHCI_INT_STATUS)
				&& (val & SDHCI_INT_DATA_END))) {
			u32 v;
			v = readl(host->ioaddr + SDHCI_VENDOR_SPEC);
			v &= ~SDHCI_VENDOR_SPEC_SDIO_QUIRK;
			writel(v, host->ioaddr + SDHCI_VENDOR_SPEC);
	}

	if (unlikely(reg == SDHCI_INT_ENABLE || reg == SDHCI_SIGNAL_ENABLE)) {
		if (val & SDHCI_INT_ADMA_ERROR) {
			val &= ~SDHCI_INT_ADMA_ERROR;
			val |= SDHCI_INT_VENDOR_SPEC_DMA_ERR;
		}
	}

	writel(val, host->ioaddr + reg);
}

static u16 esdhc_readw_le(struct sdhci_host *host, int reg)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct pltfm_imx_data *imx_data = pltfm_host->priv;
	u16 ret;
	u32 val;

	if (unlikely(reg == SDHCI_HOST_VERSION)) {
		u16 val = readw(host->ioaddr + (reg ^ 2));
		/*
		 * uSDHC supports SDHCI v3.0, but it's encoded as value
		 * 0x3 in host controller version register, which violates
		 * SDHCI_SPEC_300 definition.  Work it around here.
		 */
		if ((val & SDHCI_SPEC_VER_MASK) == 3)
			return --val;
	}

	if (reg == SDHCI_HOST_CONTROL2) {
		ret = 0;
		val = readl(host->ioaddr + SDHCI_VENDOR_SPEC);
		if (val & SDHCI_VENDOR_SPEC_VSELECT)
			ret |= SDHCI_CTRL_VDD_180;

		if (is_imx6q_usdhc(imx_data)) {
			val = readl(host->ioaddr + SDHCI_MIX_CTRL);
			if (val & SDHCI_MIX_CTRL_EXE_TUNE)
				ret |= SDHCI_CTRL_EXEC_TUNING;
			if (val & SDHCI_MIX_CTRL_SMPCLK_SEL)
				ret |= SDHCI_CTRL_TUNED_CLK;
		}

		ret |= (imx_data->uhs_mode & SDHCI_CTRL_UHS_MASK);
		ret &= ~SDHCI_CTRL_PRESET_VAL_ENABLE;

		return ret;
	}

	return readw(host->ioaddr + reg);
}

static int esdhc_change_pinstate_by_clk(struct sdhci_host *host,
					unsigned int clock)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct pltfm_imx_data *imx_data = pltfm_host->priv;
	struct pinctrl_state *pinctrl;
	int ret = 0;

	if (IS_ERR(imx_data->pinctrl) || \
			IS_ERR(imx_data->pins_default) || \
			IS_ERR(imx_data->pins_100mhz) || \
			IS_ERR(imx_data->pins_200mhz))
		return -EINVAL;

	if (clock > 100000000)
		pinctrl = imx_data->pins_200mhz;
	else if (clock > 52000000)
		pinctrl = imx_data->pins_100mhz;
	else
		pinctrl = imx_data->pins_default;

	if (pinctrl == imx_data->pins_current)
		return ret;

	ret = pinctrl_select_state(imx_data->pinctrl, pinctrl);
	if (ret)
		dev_err(mmc_dev(host->mmc),
			"could not set pinctrl for clk %d: %d\n", clock, ret);
	else
		imx_data->pins_current = pinctrl;
	return ret;
}

static void esdhc_writew_le(struct sdhci_host *host, u16 val, int reg)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct pltfm_imx_data *imx_data = pltfm_host->priv;
	struct esdhc_platform_data *boarddata = &imx_data->boarddata;
	u32 v;

	switch (reg) {
	case SDHCI_CLOCK_CONTROL:
		v = readl(host->ioaddr + SDHCI_VENDOR_SPEC);
		if (val & SDHCI_CLOCK_CARD_EN)
			v |= SDHCI_VENDOR_SPEC_FRC_SDCLK_ON;
		else
			v &= ~SDHCI_VENDOR_SPEC_FRC_SDCLK_ON;
		writel(v, host->ioaddr + SDHCI_VENDOR_SPEC);

		return;
	case SDHCI_HOST_CONTROL2:
		v = readl(host->ioaddr + SDHCI_VENDOR_SPEC);
		if (val & SDHCI_CTRL_VDD_180)
			v |= SDHCI_VENDOR_SPEC_VSELECT;
		else
			v &= ~SDHCI_VENDOR_SPEC_VSELECT;
		writel(v, host->ioaddr + SDHCI_VENDOR_SPEC);

		imx_data->is_ddr = val & SDHCI_CTRL_UHS_DDR50;
		imx_data->is_tuned_clk = val & SDHCI_CTRL_TUNED_CLK;
		imx_data->uhs_mode = val & SDHCI_CTRL_UHS_MASK;

		/* Make necessary pinctrl state chage according to freq */
		esdhc_change_pinstate_by_clk(host, host->clock);

		if (!boarddata->delay_line)
			return;

		if (val & SDHCI_CTRL_UHS_DDR50) {
			v = boarddata->delay_line
				<< SDHCI_DLL_OVERRIDE_OFFSET;
			v |= (1 << SDHCI_DLL_OVERRIDE_EN_OFFSET);
		} else {
			v = 0;
		}

		writel(v, host->ioaddr + SDHCI_DLL_CTRL);

		return;
	case SDHCI_TRANSFER_MODE:
		/*
		 * Postpone this write, we must do it together with a
		 * command write that is down below.
		 */
		if ((imx_data->flags & ESDHC_FLAG_MULTIBLK_NO_INT)
				&& (host->cmd->opcode == SD_IO_RW_EXTENDED)
				&& (host->cmd->data->blocks > 1)
				&& (host->cmd->data->flags & MMC_DATA_READ)) {
			u32 v;
			v = readl(host->ioaddr + SDHCI_VENDOR_SPEC);
			v |= SDHCI_VENDOR_SPEC_SDIO_QUIRK;
			writel(v, host->ioaddr + SDHCI_VENDOR_SPEC);
		}

		imx_data->scratchpad = val;

		if (val & SDHCI_TRNS_AUTO_CMD23) {
			/*
			 * clear SDHCI_TRNS_AUTO_CMD23, which is conflict with
			 * SDHCI_MIX_CTRL_DDREN.
			 */
			imx_data->scratchpad &= ~SDHCI_TRNS_AUTO_CMD23;
			imx_data->scratchpad |= SDHCI_MIX_CTRL_AC23EN;
		}

		if (imx_data->is_tuned_clk)
			imx_data->scratchpad |= SDHCI_MIX_CTRL_SMPCLK_SEL;
		else
			imx_data->scratchpad &= ~SDHCI_MIX_CTRL_SMPCLK_SEL;

		if (imx_data->is_ddr)
			imx_data->scratchpad |= SDHCI_MIX_CTRL_DDREN;
		else
			imx_data->scratchpad &= ~SDHCI_MIX_CTRL_DDREN;
		return;
	case SDHCI_COMMAND:
		if ((host->cmd->opcode == MMC_STOP_TRANSMISSION ||
		     host->cmd->opcode == MMC_SET_BLOCK_COUNT) &&
	            (imx_data->flags & ESDHC_FLAG_MULTIBLK_NO_INT))
			val |= SDHCI_CMD_ABORTCMD;

		if (is_imx6q_usdhc(imx_data)) {
			u32 m = readl(host->ioaddr + SDHCI_MIX_CTRL);
			m = imx_data->scratchpad | (m & 0xffff0000);
			writel(m, host->ioaddr + SDHCI_MIX_CTRL);
			writel(val << 16,
			       host->ioaddr + SDHCI_TRANSFER_MODE);
		} else {
			writel(val << 16 | imx_data->scratchpad,
			       host->ioaddr + SDHCI_TRANSFER_MODE);
		}
		return;
	case SDHCI_BLOCK_SIZE:
		val &= ~SDHCI_MAKE_BLKSZ(0x7, 0);
		break;
	}
	esdhc_clrset_le(host, 0xffff, val, reg);
}

static u8 esdhc_readb_le(struct sdhci_host *host, int reg)
{
	u8 ret = 0;
	u32 val;

	switch (reg) {
	case SDHCI_POWER_CONTROL:
		val = readl(host->ioaddr + SDHCI_VENDOR_SPEC);
		if (val & SDHCI_VENDOR_SPEC_VSELECT)
			ret = SDHCI_POWER_180;
		else
			ret = SDHCI_POWER_300;
		ret |= SDHCI_POWER_ON;
		break;
	case SDHCI_HOST_CONTROL:
		val = readl(host->ioaddr + SDHCI_HOST_CONTROL);
		if (val & SDHCI_PROT_CTRL_LCTL)
			ret |= SDHCI_CTRL_LED;
		else
			ret &= ~SDHCI_CTRL_LED;
		ret |= (val & SDHCI_PROT_CTRL_DMA_MASK) >> 5;
		if (SDHCI_PROT_CTRL_8BIT == (val & SDHCI_PROT_CTRL_DTW_MASK)) {
			ret &= ~SDHCI_CTRL_4BITBUS;
			ret |= SDHCI_CTRL_8BITBUS;
		} else if (SDHCI_PROT_CTRL_4BIT
				== (val & SDHCI_PROT_CTRL_DTW_MASK)) {
			ret &= ~SDHCI_CTRL_8BITBUS;
			ret |= SDHCI_CTRL_4BITBUS;
		} else if (SDHCI_PROT_CTRL_1BIT
				== (val & SDHCI_PROT_CTRL_DTW_MASK)) {
			ret &= ~(SDHCI_CTRL_8BITBUS | SDHCI_CTRL_4BITBUS);
		}
		break;
	case SDHCI_SOFTWARE_RESET:
		val = readl(host->ioaddr + SDHCI_CLOCK_CONTROL);
		ret = val >> 24;
		break;
	case SDHCI_RESPONSE + 3:
		val = readl(host->ioaddr + SDHCI_RESPONSE);
		ret = val >> 24;
		break;
	case SDHCI_RESPONSE + 7:
		val = readl(host->ioaddr + SDHCI_RESPONSE + 4);
		ret = val >> 24;
		break;
	case SDHCI_RESPONSE + 11:
		val = readl(host->ioaddr + SDHCI_RESPONSE + 8);
		ret = val >> 24;
		break;
	default:
		break;
	}

	return ret;
}

static void esdhc_writeb_le(struct sdhci_host *host, u8 val, int reg)
{
	u32 new_val;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct pltfm_imx_data *imx_data = pltfm_host->priv;

	switch (reg) {
	case SDHCI_POWER_CONTROL:
		/*
		 * FSL put some DMA bits here
		 * If your board has a regulator, code should be here
		 */
		return;
	case SDHCI_HOST_CONTROL:
		/* FSL messed up here, so we can just keep those three */
		new_val = val & SDHCI_CTRL_LED;
		if (val & SDHCI_CTRL_4BITBUS) {
			new_val &= ~SDHCI_PROT_CTRL_8BIT;
			new_val |= SDHCI_PROT_CTRL_4BIT;
		} else if (val & SDHCI_CTRL_8BITBUS) {
			new_val &= ~SDHCI_PROT_CTRL_4BIT;
			new_val |= SDHCI_PROT_CTRL_8BIT;
		}
		/* ensure the endianess */
		new_val |= ESDHC_HOST_CONTROL_LE;
		/* DMA mode bits are shifted */
		new_val |= (val & SDHCI_CTRL_DMA_MASK) << 5;

		esdhc_clrset_le(host, 0xffff, new_val, reg);
		return;
	}
	esdhc_clrset_le(host, 0xff, val, reg);

	/*
	 * The esdhc has a design violation to SDHC spec which tells
	 * that software reset should not affect card detection circuit.
	 * But esdhc clears its SYSCTL register bits [0..2] during the
	 * software reset.  This will stop those clocks that card detection
	 * circuit relies on.  To work around it, we turn the clocks on back
	 * to keep card detection circuit functional.
	 */
	if ((reg == SDHCI_SOFTWARE_RESET) && (val & 1)) {
		esdhc_clrset_le(host, 0x7, 0x7, ESDHC_SYSTEM_CONTROL);
		/*
		 * The RSTA, reset all, on usdhc will not clear MIX_CTRL
		 * register, do it manually here.
		 */
		if (is_imx6q_usdhc(imx_data))
			writel(0, host->ioaddr + SDHCI_MIX_CTRL);
	}
}

static unsigned int esdhc_pltfm_get_max_clock(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);

	return clk_get_rate(pltfm_host->clk);
}

static unsigned int esdhc_pltfm_get_min_clock(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);

	return clk_get_rate(pltfm_host->clk) / 256 / 16;
}

static unsigned int esdhc_pltfm_get_ro(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct pltfm_imx_data *imx_data = pltfm_host->priv;
	struct esdhc_platform_data *boarddata = &imx_data->boarddata;

	switch (boarddata->wp_type) {
	case ESDHC_WP_GPIO:
		if (gpio_is_valid(boarddata->wp_gpio))
			return gpio_get_value(boarddata->wp_gpio);
	case ESDHC_WP_CONTROLLER:
		return !(readl(host->ioaddr + SDHCI_PRESENT_STATE) &
			       SDHCI_WRITE_PROTECT);
	case ESDHC_WP_NONE:
		break;
	}

	return -ENOSYS;
}

static int esdhc_pltfm_set_buswidth(struct sdhci_host *host, int width)
{
	u32 reg = readl(host->ioaddr + SDHCI_HOST_CONTROL);
	reg &= ~SDHCI_PROT_CTRL_DTW_MASK;

	if (width == MMC_BUS_WIDTH_8)
		reg |= SDHCI_PROT_CTRL_8BIT;
	else if (width == MMC_BUS_WIDTH_4)
		reg |= SDHCI_PROT_CTRL_4BIT;

	writel(reg, host->ioaddr + SDHCI_HOST_CONTROL);
	return 0;
}

static void esdhc_reset(struct sdhci_host *host, u32 rst_bits)
{
	u32 timeout;
	u32 reg;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct pltfm_imx_data *imx_data = pltfm_host->priv;

	reg = readl(host->ioaddr + SDHCI_SYS_CTRL);
	reg |= rst_bits;
	writel(reg, host->ioaddr + SDHCI_SYS_CTRL);

	/* Wait for max 100ms */
	timeout = 100;

	/* hw clears the bit when it's done */
	while (readl(host->ioaddr + SDHCI_SYS_CTRL)
			& rst_bits) {
		if (timeout == 0) {
			dev_err(mmc_dev(host->mmc),
				"Reset never completes!\n");
			return;
		}
		timeout--;
		mdelay(1);
	}

	/*
	 * The RSTA, reset all, on usdhc will not clear following regs:
	 * > SDHCI_MIX_CTRL
	 * > SDHCI_TUNE_CTRL_STATUS
	 *
	 * Do it manually here.
	 */
	if ((rst_bits & SDHCI_SYS_CTRL_RSTA) && is_imx6q_usdhc(imx_data)) {
		writel(0, host->ioaddr + SDHCI_MIX_CTRL);
		writel(0, host->ioaddr + SDHCI_TUNE_CTRL_STATUS);
	}
}

static void esdhc_prepare_tuning(struct sdhci_host *host, u32 val)
{
	u32 reg;

	esdhc_reset(host, SDHCI_SYS_CTRL_RSTA);
	mdelay(1);

	reg = readl(host->ioaddr + SDHCI_MIX_CTRL);
	reg |= SDHCI_MIX_CTRL_EXE_TUNE | \
		   SDHCI_MIX_CTRL_SMPCLK_SEL | \
		   SDHCI_MIX_CTRL_FBCLK_SEL;
	writel(reg, host->ioaddr + SDHCI_MIX_CTRL);
	writel((val << 8), host->ioaddr + SDHCI_TUNE_CTRL_STATUS);
}

static void request_done(struct mmc_request *mrq)
{
	complete(&mrq->completion);
}

static int esdhc_send_tuning_cmd(struct sdhci_host *host, u32 opcode)
{
	struct mmc_command cmd = {0};
	struct mmc_request mrq = {0};
	struct mmc_data data = {0};
	struct scatterlist sg;
	char tuning_pattern[SDHCI_TUNING_BLOCK_PATTERN_LEN];

	cmd.opcode = opcode;
	cmd.arg = 0;
	cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;

	data.blksz = SDHCI_TUNING_BLOCK_PATTERN_LEN;
	data.blocks = 1;
	data.flags = MMC_DATA_READ;
	data.sg = &sg;
	data.sg_len = 1;

	sg_init_one(&sg, tuning_pattern, sizeof(tuning_pattern));

	mrq.cmd = &cmd;
	mrq.cmd->mrq = &mrq;
	mrq.data = &data;
	mrq.data->mrq = &mrq;
	mrq.cmd->data = mrq.data;

	mrq.done = request_done;

	init_completion(&(mrq.completion));
	sdhci_request(host->mmc, &mrq);
	wait_for_completion(&(mrq.completion));

	if (cmd.error)
		return cmd.error;
	if (data.error)
		return data.error;

	return 0;
}

static void esdhc_post_tuning(struct sdhci_host *host)
{
	u32 reg;

	reg = readl(host->ioaddr + SDHCI_MIX_CTRL);
	reg &= ~SDHCI_MIX_CTRL_EXE_TUNE;
	writel(reg, host->ioaddr + SDHCI_MIX_CTRL);
}

static int esdhc_executing_tuning(struct sdhci_host *host, u32 opcode)
{
	int min, max, avg;
	min = host->tuning_min;
	while (min < host->tuning_max) {
		esdhc_prepare_tuning(host, min);
		if (!esdhc_send_tuning_cmd(host, opcode))
			break;
		min += host->tuning_step;
	}

	max = min + host->tuning_step;
	while (max < host->tuning_max) {
		esdhc_prepare_tuning(host, max);
		if (esdhc_send_tuning_cmd(host, opcode)) {
			max -= host->tuning_step;
			break;
		}
		max += host->tuning_step;
	}

	avg = (min + max) / 2;
	esdhc_prepare_tuning(host, avg);
	esdhc_send_tuning_cmd(host, opcode);
	esdhc_post_tuning(host);
	return 0;
}

static struct sdhci_ops sdhci_esdhc_ops = {
	.read_l = esdhc_readl_le,
	.read_w = esdhc_readw_le,
	.read_b = esdhc_readb_le,
	.write_l = esdhc_writel_le,
	.write_w = esdhc_writew_le,
	.write_b = esdhc_writeb_le,
	.set_clock = esdhc_set_clock,
	.get_max_clock = esdhc_pltfm_get_max_clock,
	.get_min_clock = esdhc_pltfm_get_min_clock,
	.get_ro = esdhc_pltfm_get_ro,
	.platform_8bit_width = esdhc_pltfm_set_buswidth,
};

static struct sdhci_pltfm_data sdhci_esdhc_imx_pdata = {
	.quirks = ESDHC_DEFAULT_QUIRKS | SDHCI_QUIRK_NO_HISPD_BIT
			| SDHCI_QUIRK_NO_ENDATTR_IN_NOPDESC
			| SDHCI_QUIRK_BROKEN_ADMA_ZEROLEN_DESC
			| SDHCI_QUIRK_BROKEN_ADMA
			| SDHCI_QUIRK_BROKEN_CARD_DETECTION,
	.ops = &sdhci_esdhc_ops,
};

static irqreturn_t cd_irq(int irq, void *data)
{
	struct sdhci_host *sdhost = (struct sdhci_host *)data;

	esdhc_reset(sdhost, SDHCI_SYS_CTRL_RSTA);

	tasklet_schedule(&sdhost->card_tasklet);
	return IRQ_HANDLED;
};

#ifdef CONFIG_OF
static int __devinit
sdhci_esdhc_imx_probe_dt(struct platform_device *pdev,
			 struct esdhc_platform_data *boarddata)
{
	struct device_node *np = pdev->dev.of_node;
	struct sdhci_host *host = platform_get_drvdata(pdev);

	if (!np)
		return -ENODEV;

	if (of_get_property(np, "non-removable", NULL))
		boarddata->cd_type = ESDHC_CD_PERMANENT;

	if (of_get_property(np, "fsl,cd-controller", NULL))
		boarddata->cd_type = ESDHC_CD_CONTROLLER;

	if (of_get_property(np, "fsl,wp-controller", NULL))
		boarddata->wp_type = ESDHC_WP_CONTROLLER;

	boarddata->cd_gpio = of_get_named_gpio(np, "cd-gpios", 0);
	if (gpio_is_valid(boarddata->cd_gpio))
		boarddata->cd_type = ESDHC_CD_GPIO;

	boarddata->wp_gpio = of_get_named_gpio(np, "wp-gpios", 0);
	if (gpio_is_valid(boarddata->wp_gpio))
		boarddata->wp_type = ESDHC_WP_GPIO;

	of_property_read_u32(np, "bus-width", &boarddata->max_bus_width);

	if (of_property_read_u32(np, "fsl,delay-line", &boarddata->delay_line))
		boarddata->delay_line = 0;

	if (of_find_property(np, "no-1-8-v", NULL))
		boarddata->support_vsel = false;
	else
		boarddata->support_vsel = true;

	of_property_read_u32(np, "ocr-limit", &boarddata->ocr_limit);

	if (of_find_property(np, "power-off-card", NULL))
		host->mmc->caps |= MMC_CAP_POWER_OFF_CARD;

	if (of_find_property(np, "keep-power-in-suspend", NULL))
		host->mmc->pm_caps |= MMC_PM_KEEP_POWER;

	if (of_find_property(np, "enable-sdio-wakeup", NULL))
		host->mmc->pm_caps |= MMC_PM_WAKE_SDIO_IRQ;

	return 0;
}
#else
static inline int
sdhci_esdhc_imx_probe_dt(struct platform_device *pdev,
			 struct esdhc_platform_data *boarddata)
{
	return -ENODEV;
}
#endif

static int __devinit sdhci_esdhc_imx_probe(struct platform_device *pdev)
{
	const struct of_device_id *of_id =
			of_match_device(imx_esdhc_dt_ids, &pdev->dev);
	struct sdhci_pltfm_host *pltfm_host;
	struct sdhci_host *host;
	struct esdhc_platform_data *boarddata;
	int err, ret;
	struct pltfm_imx_data *imx_data;

	host = sdhci_pltfm_init(pdev, &sdhci_esdhc_imx_pdata);
	if (IS_ERR(host))
		return PTR_ERR(host);

	pltfm_host = sdhci_priv(host);

	imx_data = kzalloc(sizeof(struct pltfm_imx_data), GFP_KERNEL);
	if (!imx_data) {
		err = -ENOMEM;
		goto err_imx_data;
	}

	if (of_id)
		pdev->id_entry = of_id->data;
	imx_data->devtype = pdev->id_entry->driver_data;
	pltfm_host->priv = imx_data;

	imx_data->clk_ipg = devm_clk_get(&pdev->dev, "ipg");
	if (IS_ERR(imx_data->clk_ipg)) {
		err = PTR_ERR(imx_data->clk_ipg);
		goto err_clk_get;
	}

	imx_data->clk_ahb = devm_clk_get(&pdev->dev, "ahb");
	if (IS_ERR(imx_data->clk_ahb)) {
		err = PTR_ERR(imx_data->clk_ahb);
		goto err_clk_get;
	}

	imx_data->clk_per = devm_clk_get(&pdev->dev, "per");
	if (IS_ERR(imx_data->clk_per)) {
		err = PTR_ERR(imx_data->clk_per);
		goto err_clk_get;
	}

	pltfm_host->clk = imx_data->clk_per;

	clk_prepare_enable(imx_data->clk_per);
	clk_prepare_enable(imx_data->clk_ipg);
	clk_prepare_enable(imx_data->clk_ahb);

	imx_data->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(imx_data->pinctrl)) {
		err = PTR_ERR(imx_data->pinctrl);
		goto pin_err;
	}

	imx_data->pins_default = pinctrl_lookup_state(imx_data->pinctrl,
						PINCTRL_STATE_DEFAULT);
	if (IS_ERR(imx_data->pins_default)) {
		ret = PTR_ERR(imx_data->pins_default);
		dev_err(mmc_dev(host->mmc),
				"could not get default state: %d\n", ret);
	}

	host->quirks |= SDHCI_QUIRK_BROKEN_TIMEOUT_VAL;

	if (is_imx25_esdhc(imx_data) || is_imx35_esdhc(imx_data))
		/* Fix errata ENGcm07207 present on i.MX25 and i.MX35 */
		host->quirks |= SDHCI_QUIRK_NO_MULTIBLOCK
			| SDHCI_QUIRK_BROKEN_ADMA;

	if (is_imx53_esdhc(imx_data))
		imx_data->flags |= ESDHC_FLAG_MULTIBLK_NO_INT;

	if (is_imx6q_usdhc(imx_data)) {
		/*
		 * The imx6q ROM code will change the default watermark
		 * level setting to something insane.  Change it back here.
		 */
		writel(0x08100810, host->ioaddr + SDHCI_WTMK_LVL);
		host->tuning_min = SDHCI_TUNE_CTRL_MIN;
		host->tuning_max = SDHCI_TUNE_CTRL_MAX;
		host->tuning_step = SDHCI_TUNE_CTRL_STEP;
	}

	boarddata = &imx_data->boarddata;
	if (sdhci_esdhc_imx_probe_dt(pdev, boarddata) < 0) {
		if (!host->mmc->parent->platform_data) {
			dev_err(mmc_dev(host->mmc), "no board data!\n");
			err = -EINVAL;
			goto no_board_data;
		}
		imx_data->boarddata = *((struct esdhc_platform_data *)
					host->mmc->parent->platform_data);
	}

	/* write_protect */
	if (boarddata->wp_type == ESDHC_WP_GPIO) {
		err = gpio_request_one(boarddata->wp_gpio, GPIOF_IN, "ESDHC_WP");
		if (err) {
			dev_warn(mmc_dev(host->mmc),
				 "no write-protect pin available!\n");
			boarddata->wp_gpio = -EINVAL;
		}
	} else {
		boarddata->wp_gpio = -EINVAL;
	}

	/* card_detect */
	if (boarddata->cd_type != ESDHC_CD_GPIO)
		boarddata->cd_gpio = -EINVAL;

	switch (boarddata->cd_type) {
	case ESDHC_CD_GPIO:
		err = gpio_request_one(boarddata->cd_gpio, GPIOF_IN, "ESDHC_CD");
		if (err) {
			dev_err(mmc_dev(host->mmc),
				"no card-detect pin available!\n");
			goto no_card_detect_pin;
		}

		err = request_irq(gpio_to_irq(boarddata->cd_gpio), cd_irq,
				 IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
				 mmc_hostname(host->mmc), host);
		if (err) {
			dev_err(mmc_dev(host->mmc), "request irq error\n");
			goto no_card_detect_irq;
		}
		/* fall through */

	case ESDHC_CD_CONTROLLER:
		/* we have a working card_detect back */
		host->quirks &= ~SDHCI_QUIRK_BROKEN_CARD_DETECTION;
		break;

	case ESDHC_CD_PERMANENT:
		host->mmc->caps |= MMC_CAP_NONREMOVABLE;
		break;

	case ESDHC_CD_NONE:
		break;
	}

	switch (boarddata->max_bus_width) {
	case 8:
		host->mmc->caps |= MMC_CAP_4_BIT_DATA | MMC_CAP_8_BIT_DATA;
		break;
	case 4:
		host->mmc->caps |= MMC_CAP_4_BIT_DATA;
		break;
	case 1:
	default:
		host->quirks |= SDHCI_QUIRK_FORCE_1_BIT_DATA;
		break;
	}

	if (is_imx6q_usdhc(imx_data))
		host->mmc->caps |= MMC_CAP_1_8V_DDR | MMC_CAP_UHS_DDR50;

	host->ocr_avail_sd = MMC_VDD_29_30 | MMC_VDD_30_31 | \
					 MMC_VDD_32_33 | MMC_VDD_33_34;
	host->ocr_avail_mmc = MMC_VDD_29_30 | MMC_VDD_30_31 | \
					 MMC_VDD_32_33 | MMC_VDD_33_34;
	host->ocr_avail_sdio = MMC_VDD_29_30 | MMC_VDD_30_31 | \
					 MMC_VDD_32_33 | MMC_VDD_33_34;
	if (is_imx6q_usdhc(imx_data))
		sdhci_esdhc_ops.platform_execute_tuning
			= esdhc_executing_tuning;

	if ((boarddata->support_vsel) && is_imx6q_usdhc(imx_data)) {
		host->ocr_avail_sd |= MMC_VDD_165_195;
		host->ocr_avail_mmc |= MMC_VDD_165_195;
		host->ocr_avail_sdio |= MMC_VDD_165_195;

		imx_data->pins_100mhz = pinctrl_lookup_state(imx_data->pinctrl,
						ESDHC_PINCTRL_STATE_100MHZ);
		imx_data->pins_200mhz = pinctrl_lookup_state(imx_data->pinctrl,
						ESDHC_PINCTRL_STATE_200MHZ);
		if (IS_ERR(imx_data->pins_100mhz) || IS_ERR(imx_data->pins_200mhz)) {
			ret = -ENODEV;
			dev_err(mmc_dev(host->mmc),
				"could not get ultra high speed state: %d\n", ret);
			/* fall back to not support sd30 */
			host->quirks2 |= SDHCI_QUIRK2_NO_1_8_V;
		}
	} else {
		host->quirks2 |= SDHCI_QUIRK2_NO_1_8_V;
	}
	if (boarddata->ocr_limit) {
		host->ocr_avail_sd &= boarddata->ocr_limit;
		host->ocr_avail_mmc &= boarddata->ocr_limit;
		host->ocr_avail_sdio &= boarddata->ocr_limit;
	}

	err = sdhci_add_host(host);
	if (err)
		goto err_add_host;

	return 0;

err_add_host:
	if (gpio_is_valid(boarddata->cd_gpio))
		free_irq(gpio_to_irq(boarddata->cd_gpio), host);
no_card_detect_irq:
	if (gpio_is_valid(boarddata->cd_gpio))
		gpio_free(boarddata->cd_gpio);
	if (gpio_is_valid(boarddata->wp_gpio))
		gpio_free(boarddata->wp_gpio);
no_card_detect_pin:
no_board_data:
pin_err:
	clk_disable_unprepare(imx_data->clk_per);
	clk_disable_unprepare(imx_data->clk_ipg);
	clk_disable_unprepare(imx_data->clk_ahb);
err_clk_get:
	kfree(imx_data);
err_imx_data:
	sdhci_pltfm_free(pdev);
	return err;
}

static int __devexit sdhci_esdhc_imx_remove(struct platform_device *pdev)
{
	struct sdhci_host *host = platform_get_drvdata(pdev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct pltfm_imx_data *imx_data = pltfm_host->priv;
	struct esdhc_platform_data *boarddata = &imx_data->boarddata;
	int dead = (readl(host->ioaddr + SDHCI_INT_STATUS) == 0xffffffff);

	sdhci_remove_host(host, dead);

	if (gpio_is_valid(boarddata->wp_gpio))
		gpio_free(boarddata->wp_gpio);

	if (gpio_is_valid(boarddata->cd_gpio)) {
		free_irq(gpio_to_irq(boarddata->cd_gpio), host);
		gpio_free(boarddata->cd_gpio);
	}

	clk_disable_unprepare(imx_data->clk_per);
	clk_disable_unprepare(imx_data->clk_ipg);
	clk_disable_unprepare(imx_data->clk_ahb);

	kfree(imx_data);

	sdhci_pltfm_free(pdev);

	return 0;
}

static struct platform_driver sdhci_esdhc_imx_driver = {
	.driver		= {
		.name	= "sdhci-esdhc-imx",
		.owner	= THIS_MODULE,
		.of_match_table = imx_esdhc_dt_ids,
		.pm	= SDHCI_PLTFM_PMOPS,
	},
	.id_table	= imx_esdhc_devtype,
	.probe		= sdhci_esdhc_imx_probe,
	.remove		= __devexit_p(sdhci_esdhc_imx_remove),
};

module_platform_driver(sdhci_esdhc_imx_driver);

MODULE_DESCRIPTION("SDHCI driver for Freescale i.MX eSDHC");
MODULE_AUTHOR("Wolfram Sang <w.sang@pengutronix.de>");
MODULE_LICENSE("GPL v2");
