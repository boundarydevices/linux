// SPDX-License-Identifier: GPL-2.0+
//
// tps6286x-regulator.c - TI TPS62864/6 step-down DC/DC converter
// Copyright (C) 2021 Boundary Devices LLC

#ifndef __LINUX_REGULATOR_TPS6286X_H
#define __LINUX_REGULATOR_TPS6286X_H

/*
 * struct tps6286x_regulator_platform_data - tps6286x regulator platform data.
 *
 * @reg_init_data: The regulator init data.
 * @en_discharge: Enable discharge the output capacitor via internal
 *                register.
 * @vsel_gpio: Gpio number for vsel. It should be -1 if this is tied with
 *              fixed logic.
 * @vsel_def_state: Default state of vsel. 1 if it is high else 0.
 */
struct tps6286x_regulator_platform_data {
	struct regulator_init_data *reg_init_data;
	bool en_discharge;
	int vsel_gpio;
	int vsel_def_state;
};

#endif /* __LINUX_REGULATOR_TPS6286x_H */
