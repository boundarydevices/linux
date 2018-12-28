/*
 * drivers/amlogic/mmc/amlsd_of.c
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

#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/mmc/host.h>
#include <linux/mmc/core.h>
#include <linux/mmc/mmc.h>
#include <linux/mtd/partitions.h>
#include <linux/slab.h>
#include <linux/amlogic/sd.h>
#include <linux/gpio/consumer.h>
#include <linux/amlogic/amlsd.h>
#include <linux/amlogic/cpu_version.h>

unsigned int sd_emmc_debug;

static const struct sd_caps host_caps[] = {
	SD_CAPS(MMC_CAP_4_BIT_DATA, "MMC_CAP_4_BIT_DATA"),
	SD_CAPS(MMC_CAP_MMC_HIGHSPEED, "MMC_CAP_MMC_HIGHSPEED"),
	SD_CAPS(MMC_CAP_SD_HIGHSPEED, "MMC_CAP_SD_HIGHSPEED"),
	SD_CAPS(MMC_CAP_SDIO_IRQ, "MMC_CAP_SDIO_IRQ"),
	SD_CAPS(MMC_CAP_SPI, "MMC_CAP_SPI"),
	SD_CAPS(MMC_CAP_NEEDS_POLL, "MMC_CAP_NEEDS_POLL"),
	SD_CAPS(MMC_CAP_8_BIT_DATA, "MMC_CAP_8_BIT_DATA"),
	SD_CAPS(MMC_CAP_NONREMOVABLE, "MMC_CAP_NONREMOVABLE"),
	SD_CAPS(MMC_CAP_WAIT_WHILE_BUSY, "MMC_CAP_WAIT_WHILE_BUSY"),
	SD_CAPS(MMC_CAP_ERASE, "MMC_CAP_ERASE"),
	SD_CAPS(MMC_CAP_1_8V_DDR, "MMC_CAP_1_8V_DDR"),
	SD_CAPS(MMC_CAP_1_2V_DDR, "MMC_CAP_1_2V_DDR"),
	SD_CAPS(MMC_CAP_POWER_OFF_CARD, "MMC_CAP_POWER_OFF_CARD"),
	SD_CAPS(MMC_CAP_BUS_WIDTH_TEST, "MMC_CAP_BUS_WIDTH_TEST"),
	SD_CAPS(MMC_CAP_UHS_SDR12, "MMC_CAP_UHS_SDR12"),
	SD_CAPS(MMC_CAP_UHS_SDR25, "MMC_CAP_UHS_SDR25"),
	SD_CAPS(MMC_CAP_UHS_SDR50, "MMC_CAP_UHS_SDR50"),
	SD_CAPS(MMC_CAP_UHS_SDR104, "MMC_CAP_UHS_SDR104"),
	SD_CAPS(MMC_CAP_UHS_DDR50, "MMC_CAP_UHS_DDR50"),
	SD_CAPS(MMC_CAP_DRIVER_TYPE_A, "MMC_CAP_DRIVER_TYPE_A"),
	SD_CAPS(MMC_CAP_DRIVER_TYPE_C, "MMC_CAP_DRIVER_TYPE_C"),
	SD_CAPS(MMC_CAP_DRIVER_TYPE_D, "MMC_CAP_DRIVER_TYPE_D"),
	SD_CAPS(MMC_CAP_CMD23, "MMC_CAP_CMD23"),
	SD_CAPS(MMC_CAP_HW_RESET, "MMC_CAP_HW_RESET"),
	SD_CAPS(MMC_CAP_AGGRESSIVE_PM, "MMC_CAP_AGGRESSIVE_PM"),
	SD_CAPS(MMC_PM_KEEP_POWER, "MMC_PM_KEEP_POWER"),
};

static int amlsd_get_host_caps(struct device_node *of_node,
		struct amlsd_platform *pdata)
{
	const char *str_caps;
	struct property *prop;
	u32 i, caps = 0;

	of_property_for_each_string(of_node, "caps", prop, str_caps) {
		for (i = 0; i < ARRAY_SIZE(host_caps); i++) {
			if (!strcasecmp(host_caps[i].name, str_caps))
				caps |= host_caps[i].caps;
		}
	};
	if (caps & MMC_CAP_8_BIT_DATA)
		caps |= MMC_CAP_4_BIT_DATA;

	pdata->caps = caps;
	pr_debug("%s:pdata->caps = %x\n", pdata->pinname, pdata->caps);
	return 0;
}

static const struct sd_caps host_caps2[] = {
	SD_CAPS(MMC_CAP2_BOOTPART_NOACC, "MMC_CAP2_BOOTPART_NOACC"),
	/*SD_CAPS(MMC_CAP2_CACHE_CTRL, "MMC_CAP2_CACHE_CTRL"),*/
	/* SD_CAPS(MMC_CAP2_POWEROFF_NOTIFY, "MMC_CAP2_POWEROFF_NOTIFY"), */
	SD_CAPS(MMC_CAP2_NO_MULTI_READ, "MMC_CAP2_NO_MULTI_READ"),
	/*SD_CAPS(MMC_CAP2_NO_SLEEP_CMD, "MMC_CAP2_NO_SLEEP_CMD"),*/
	SD_CAPS(MMC_CAP2_HS200_1_8V_SDR, "MMC_CAP2_HS200_1_8V_SDR"),
	SD_CAPS(MMC_CAP2_HS200_1_2V_SDR, "MMC_CAP2_HS200_1_2V_SDR"),
	SD_CAPS(MMC_CAP2_HS200, "MMC_CAP2_HS200"),
	SD_CAPS(MMC_CAP2_HS400_1_8V, "MMC_CAP2_HS400_1_8V"),
	SD_CAPS(MMC_CAP2_HS400_1_2V, "MMC_CAP2_HS400_1_2V"),
	SD_CAPS(MMC_CAP2_HS400, "MMC_CAP2_HS400"),
	/*SD_CAPS(MMC_CAP2_BROKEN_VOLTAGE, "MMC_CAP2_BROKEN_VOLTAGE"),*/
	/* SD_CAPS(MMC_CAP2_DETECT_ON_ERR, "MMC_CAP2_DETECT_ON_ERR"), */
	SD_CAPS(MMC_CAP2_HC_ERASE_SZ, "MMC_CAP2_HC_ERASE_SZ"),
	SD_CAPS(MMC_CAP2_CD_ACTIVE_HIGH, "MMC_CAP2_CD_ACTIVE_HIGH"),
	SD_CAPS(MMC_CAP2_RO_ACTIVE_HIGH, "MMC_CAP2_RO_ACTIVE_HIGH"),
};

static int amlsd_get_host_caps2(struct device_node *of_node,
		struct amlsd_platform *pdata)
{
	const char *str_caps;
	struct property *prop;
	u32 i, caps = 0;

	of_property_for_each_string(of_node, "caps2", prop, str_caps) {
		for (i = 0; i < ARRAY_SIZE(host_caps2); i++) {
			if (!strcasecmp(host_caps2[i].name, str_caps))
				caps |= host_caps2[i].caps;
		}
	};

	pdata->caps2 = caps;
	pr_debug("%s:pdata->caps2 = %x\n", pdata->pinname, pdata->caps2);
	return 0;
}

static int amlsd_get_host_pm_caps(struct device_node *of_node,
		struct amlsd_platform *pdata)
{
	const char *str_caps;
	struct property *prop;
	u32 caps = 0;

	of_property_for_each_string(of_node, "caps", prop, str_caps) {
		if (!strcasecmp("MMC_PM_KEEP_POWER", str_caps))
			caps |= MMC_PM_KEEP_POWER;
	};

	pdata->pm_caps = caps;
	pr_debug("%s:pdata->pm_caps = %x\n", pdata->pinname, pdata->pm_caps);
	return 0;
}

int amlsd_get_platform_data(struct platform_device *pdev,
		struct amlsd_platform *pdata,
		struct mmc_host *mmc, u32 index)
{
	struct device_node *of_node;
	struct device_node *child;
	u32 i, prop;
	const char *str = "none";

#ifdef CONFIG_AMLOGIC_M8B_MMC
	of_node = pdev->dev.of_node;
#else
	if (!mmc->parent)
		return 0;
	of_node = mmc->parent->of_node;
#endif
	if (of_node) {
		child = of_node->child;
		WARN_ON(!child);
		WARN_ON(index >= MMC_MAX_DEVICE);
		for (i = 0; i < index; i++)
			child = child->sibling;
		if (!child)
			return -EINVAL;

		/*	amlsd_init_pins_input(child, pdata);*/

		SD_PARSE_U32_PROP_HEX(child, "port",
				prop, pdata->port);
		SD_PARSE_U32_PROP_HEX(child, "ocr_avail",
				prop, pdata->ocr_avail);
		WARN_ON(!pdata->ocr_avail);
		SD_PARSE_U32_PROP_DEC(child, "f_min",
				prop, pdata->f_min);
		SD_PARSE_U32_PROP_DEC(child, "f_max",
				prop, pdata->f_max);
		SD_PARSE_U32_PROP_HEX(child, "max_req_size",
				prop, pdata->max_req_size);
		SD_PARSE_GPIO_NUM_PROP(child, "gpio_cd",
				str, pdata->gpio_cd);
		SD_PARSE_GPIO_NUM_PROP(child, "gpio_ro",
				str, pdata->gpio_ro);
		SD_PARSE_GPIO_NUM_PROP(child, "vol_switch",
				str, pdata->vol_switch);

		SD_PARSE_U32_PROP_HEX(child, "power_level",
				prop, pdata->power_level);
		SD_PARSE_GPIO_NUM_PROP(child, "gpio_power",
				str, pdata->gpio_power);
		SD_PARSE_U32_PROP_DEC(child, "calc_f",
				prop, pdata->calc_f);

		SD_PARSE_U32_PROP_DEC(child, "gpio_cd_level",
				prop, pdata->gpio_cd_level);
		SD_PARSE_STRING_PROP(child, "pinname",
				str, pdata->pinname);
		SD_PARSE_STRING_PROP(child, "dmode",
				str, pdata->dmode);
		SD_PARSE_U32_PROP_DEC(child, "auto_clk_close",
				prop, pdata->auto_clk_close);
		SD_PARSE_U32_PROP_DEC(child, "vol_switch_18",
				prop, pdata->vol_switch_18);
		SD_PARSE_U32_PROP_DEC(child, "vol_switch_delay",
				prop, pdata->vol_switch_delay);
		SD_PARSE_U32_PROP_DEC(child, "card_type",
				prop, pdata->card_type);
		SD_PARSE_U32_PROP_DEC(child, "tx_delay",
						prop, pdata->tx_delay);
		SD_PARSE_U32_PROP_DEC(child, "latest_dat",
						prop, pdata->latest_dat);
		SD_PARSE_U32_PROP_DEC(child, "co_phase",
						prop, pdata->co_phase);
		if (aml_card_type_mmc(pdata)) {
			/*tx_phase set default value first*/
			SD_PARSE_U32_PROP_DEC(child, "tx_phase",
					prop, pdata->tx_phase);
		}
		if (aml_card_type_non_sdio(pdata)) {
			/*card in default value*/
			pdata->card_in_delay = 0;
			SD_PARSE_U32_PROP_DEC(child, "card_in_delay",
					prop, pdata->card_in_delay);
		}
		SD_PARSE_GPIO_NUM_PROP(child, "hw_reset",
				str, pdata->hw_reset);
		SD_PARSE_GPIO_NUM_PROP(child, "gpio_dat3",
				str, pdata->gpio_dat3);

		pdata->xfer_pre = of_amlsd_xfer_pre;
		pdata->xfer_post = of_amlsd_xfer_post;

		amlsd_get_host_caps(child, pdata);
		amlsd_get_host_caps2(child, pdata);
		amlsd_get_host_pm_caps(child, pdata);
		pdata->port_init = of_amlsd_init;
#ifdef CARD_DETECT_IRQ
		pdata->irq_init = of_amlsd_irq_init;
#endif
		pdata->ro = of_amlsd_ro;
	}
	return 0;
}

