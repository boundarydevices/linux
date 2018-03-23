/*
 * drivers/amlogic/pinctrl/pinconf-meson-g12a.c
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

#include <linux/init.h>
#include "pinctrl-meson.h"
#include "pinconf-meson-g12a.h"

static int meson_get_drive_bank(struct meson_pinctrl *pc, unsigned int pin,
				struct meson_drive_bank **bank)
{
	int i;
	struct meson_drive_data *drive = pc->data->pcf_drv_data;

	for (i = 0; i < drive->num_drive_banks; i++) {
		if (pin >= drive->drive_banks[i].first &&
			pin <= drive->drive_banks[i].last) {
			*bank = &drive->drive_banks[i];
			return 0;
		}
	}

	return -EINVAL;
}

static void meson_drive_calc_reg_and_bit(struct meson_drive_bank *drive_bank,
				unsigned int pin, unsigned int *reg,
				unsigned int *bit)
{
	struct meson_reg_desc *desc = &drive_bank->reg;
	int shift = pin - drive_bank->first;

	*reg = (desc->reg + (desc->bit + (shift << 1)) / 32) * 4;
	*bit = (desc->bit + (shift << 1)) % 32;
}

static int meson_pinconf_set_drive_strength(struct meson_pinctrl *pc,
				unsigned int pin, u16 arg)
{
	struct meson_drive_bank *drive_bank;
	unsigned int reg, bit;
	int ret;

	dev_dbg(pc->dev, "pin %u: set drive-strength\n", pin);

	ret = meson_get_drive_bank(pc, pin, &drive_bank);
	if (ret)
		return ret;

	if (arg >= 4) {
		dev_err(pc->dev,
			"pin %u: invalid drive-strength [0-3]: %d\n",
			pin, arg);
		return -EINVAL;
	}
	meson_drive_calc_reg_and_bit(drive_bank, pin,
					&reg, &bit);

	ret = regmap_update_bits(pc->reg_drive, reg,
				0x3 << bit, (arg & 0x3) << bit);
	if (ret)
		return ret;

	return 0;
}

static int meson_pinconf_get_drive_strength(struct meson_pinctrl *pc,
				unsigned int pin, u16 *arg)
{
	struct meson_drive_bank *drive_bank;
	unsigned int reg, bit;
	unsigned int val;
	int ret;

	ret = meson_get_drive_bank(pc, pin, &drive_bank);
	if (ret)
		return ret;

	meson_drive_calc_reg_and_bit(drive_bank, pin, &reg, &bit);

	ret = regmap_read(pc->reg_drive, reg, &val);
	if (ret)
		return ret;

	*arg = (val >> bit) & 0x3;

	return 0;
}

static int meson_g12a_pinconf_set(struct pinctrl_dev *pcdev, unsigned int pin,
			     unsigned long *configs, unsigned int num_configs)
{
	struct meson_pinctrl *pc = pinctrl_dev_get_drvdata(pcdev);
	enum pin_config_param param;
	int i, ret;
	u16 arg;

	for (i = 0; i < num_configs; i++) {
		param = pinconf_to_config_param(configs[i]);
		arg = pinconf_to_config_argument(configs[i]);

		if (param == PIN_CONFIG_DRIVE_STRENGTH) {
			ret = meson_pinconf_set_drive_strength(pc, pin, arg);
			if (ret)
				return ret;
		} else {
			ret = meson_pinconf_common_set(pc, pin, param, arg);
			if (ret)
				return ret;
		}
	}

	return 0;
}

static int meson_g12a_pinconf_get(struct pinctrl_dev *pcdev, unsigned int pin,
			     unsigned long *config)
{
	struct meson_pinctrl *pc = pinctrl_dev_get_drvdata(pcdev);
	enum pin_config_param param = pinconf_to_config_param(*config);
	int ret;
	u16 arg;

	if (param == PIN_CONFIG_DRIVE_STRENGTH) {
		ret = meson_pinconf_get_drive_strength(pc, pin, &arg);
		if (ret)
			return ret;
	} else {
		ret = meson_pinconf_common_get(pc, pin, param, &arg);
		if (ret)
			return ret;
	}

	*config = pinconf_to_config_packed(param, arg);
	dev_dbg(pc->dev, "pinconf for pin %u is %lu\n", pin, *config);

	return 0;
}

static int meson_g12a_pinconf_group_set(struct pinctrl_dev *pcdev,
				unsigned int num_group, unsigned long *configs,
				unsigned int num_configs)
{
	struct meson_pinctrl *pc = pinctrl_dev_get_drvdata(pcdev);
	struct meson_pmx_group *group = &pc->data->groups[num_group];
	int i;

	dev_dbg(pc->dev, "set pinconf for group %s\n", group->name);

	for (i = 0; i < group->num_pins; i++) {
		meson_g12a_pinconf_set(pcdev, group->pins[i], configs,
				  num_configs);
	}

	return 0;
}

const struct pinconf_ops meson_g12a_pinconf_ops = {
	.pin_config_get		= meson_g12a_pinconf_get,
	.pin_config_set		= meson_g12a_pinconf_set,
	.pin_config_group_set	= meson_g12a_pinconf_group_set,
	.is_generic		= true,
};
