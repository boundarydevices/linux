/*
 * drivers/amlogic/pwm/pwm_meson_util.c
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

#undef pr_fmt
#define pr_fmt(fmt) "pwm: " fmt

#include <linux/amlogic/pwm_meson.h>

struct pwm_aml_regs *pwm_id_to_reg
					(int pwm_id,
					struct aml_pwm_chip *chip)
{
	struct aml_pwm_chip *aml_chip = chip;
	void __iomem *baseaddr = NULL;

	switch (pwm_id) {
	case PWM_A:
	case PWM_B:
	case PWM_A2:
	case PWM_B2:
		baseaddr = aml_chip->baseaddr.ab_base;
		break;
	case PWM_C:
	case PWM_D:
	case PWM_C2:
	case PWM_D2:
		baseaddr = aml_chip->baseaddr.cd_base;
		break;
	case PWM_E:
	case PWM_F:
	case PWM_E2:
	case PWM_F2:
		baseaddr = aml_chip->baseaddr.ef_base;
		break;
	case PWM_AO_A:
	case PWM_AO_B:
	case PWM_AO_A2:
	case PWM_AO_B2:
		baseaddr = aml_chip->baseaddr.aoab_base;
		break;
	case PWM_AO_C:
	case PWM_AO_D:
	case PWM_AO_C2:
	case PWM_AO_D2:
		baseaddr = aml_chip->baseaddr.aoab_base;
		break;
	default:
		pr_err("unknown pwm id: %d\n", pwm_id);
		break;
	}
	return (struct pwm_aml_regs *)baseaddr;
}
