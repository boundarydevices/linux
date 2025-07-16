// SPDX-License-Identifier: GPL-2.0-or-later
/* This driver supports TI 12Bit ADC devices
 *
 *	 - ADS7038 with SPI interface
 *	 - ADS7138 with I2C interface (future)
 *
 * Copyright (C) 2023 SYS TEC electronic AG
 * Author: Andre Werner <andre.werner@systec-electronic.com>
 */
#include <linux/bitops.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/regmap.h>

#include "ti-ads7038.h"

static const struct regmap_range ads7038_noaccess_ranges[] = {
	regmap_reg_range(0x06, 0x06), regmap_reg_range(0x08, 0x08),
	regmap_reg_range(0x0A, 0x0A), regmap_reg_range(0x0C, 0x0C),
	regmap_reg_range(0x0E, 0x0F), regmap_reg_range(0x15, 0x15),
	regmap_reg_range(0x19, 0x19), regmap_reg_range(0x1B, 0x1B),
	regmap_reg_range(0x1D, 0x1D), regmap_reg_range(0x1F, 0x1F),
	regmap_reg_range(0x40, 0x5F), regmap_reg_range(0x70, 0x7F),
	regmap_reg_range(0xB0, 0xC2), regmap_reg_range(0xC4, 0xC4),
	regmap_reg_range(0xC6, 0xC6), regmap_reg_range(0xC8, 0xC8),
	regmap_reg_range(0xCA, 0xCA), regmap_reg_range(0xCC, 0xCC),
	regmap_reg_range(0xCE, 0xCE), regmap_reg_range(0xD0, 0xD0),
	regmap_reg_range(0xD2, 0xE8), regmap_reg_range(0xEA, 0xEA),
};

static const struct regmap_range ads7038_readable_ranges[] = {
	regmap_reg_range(0x00, 0x00),	/*This is a dummy entry to provide a valid access pointer */
};

static const struct regmap_access_table ads7038_readable_table = {
	.yes_ranges = ads7038_readable_ranges,
	.n_yes_ranges = 0,	/* regmap.c: In case zero "yes ranges" are supplied, any reg is OK */
	.no_ranges = ads7038_noaccess_ranges,
	.n_no_ranges = ARRAY_SIZE(ads7038_noaccess_ranges),
};

static const struct regmap_range ads7038_volatile_ranges[] = {
	regmap_reg_range(ADS7038_SYSTEM_STATUS_REG, ADS7038_SYSTEM_STATUS_REG),
	regmap_reg_range(ADS7038_GPI_VALUE_REG, ADS7038_GPI_VALUE_REG),
	regmap_reg_range(ADS7038_EVENT_FLAG_REG, ADS7038_EVENT_FLAG_REG),
	regmap_reg_range(ADS7038_MAX_CH0_LSB_REG, ADS7038_MAX_CH7_MSB_REG),
	regmap_reg_range(ADS7038_MIN_CH0_LSB_REG, ADS7038_MIN_CH7_MSB_REG),
	regmap_reg_range(ADS7038_RECENT_CH0_LSB_REG,
			 ADS7038_RECENT_CH7_MSB_REG),
};

static const struct regmap_access_table ads7038_volatile_table = {
	.yes_ranges = ads7038_volatile_ranges,
	.n_yes_ranges = ARRAY_SIZE(ads7038_volatile_ranges),
	.no_ranges = ads7038_noaccess_ranges,
	.n_no_ranges = ARRAY_SIZE(ads7038_noaccess_ranges),
};

/* It is okay to include noaccess registers in range, because they have been proven at first. */
static const struct regmap_range ads7038_writable_range[] = {
	regmap_reg_range(ADS7038_SYSTEM_STATUS_REG, ADS7038_GPO_VALUE_REG),
	regmap_reg_range(ADS7038_SEQUENCE_CFG_REG, ADS7038_ALERT_PIN_CFG_REG),
	regmap_reg_range(ADS7038_EVENT_HIGH_FLAG_REG, ADS7038_LOW_TH_CH7_REG),
	regmap_reg_range(ADS7038_GPO0_TRIG_EVENT_SEL_REG,
			 ADS7038_GPO_VALUE_TRIG_REG),
};

static const struct regmap_access_table ads7038_writable_table = {
	.yes_ranges = ads7038_writable_range,
	.n_yes_ranges = ARRAY_SIZE(ads7038_writable_range),
	.no_ranges = ads7038_noaccess_ranges,
	.n_no_ranges = ARRAY_SIZE(ads7038_noaccess_ranges),
};

const struct regmap_config ads7038_regmap_config = {
	.name = "ads7038",
	.reg_bits = ADS7038_REGISTER_SIZE,
	.val_bits = ADS7038_REGISTER_SIZE,

	.wr_table = &ads7038_writable_table,
	.rd_table = &ads7038_readable_table,
	.volatile_table = &ads7038_volatile_table,

	.max_register = ADS7038_REG_ADDRESS_MAX,
	.cache_type = REGCACHE_RBTREE,
};
EXPORT_SYMBOL(ads7038_regmap_config);

MODULE_AUTHOR("Andre Werner <andre.werner@systec-electronic.com>");
MODULE_DESCRIPTION("ADS7038 SPI bus driver");
MODULE_LICENSE("GPL");
