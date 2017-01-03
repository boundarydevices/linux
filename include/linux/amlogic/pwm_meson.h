/*
 * include/linux/amlogic/pwm_meson.h
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

#ifndef _PWM_MESON_H
#define _PWM_MESON_H

#include <linux/err.h>



#define REG_PWM_A			0x0
#define REG_PWM_B			0x4
#define REG_MISC_AB			0x8
#define REG_DS_A_B			0xc
#define REG_TIME_AB			0x10
#define REG_PWM_A2			0x14
#define REG_PWM_B2			0x18
#define REG_BLINK_AB		0x1c


#define REG_PWM_C			0xf0
#define REG_PWM_D			0xf4
#define REG_MISC_CD			0xf8
#define REG_DS_C_D			0xfc
#define REG_TIME_CD			0x100
#define REG_PWM_C2			0x104
#define REG_PWM_D2			0x108
#define REG_BLINK_CD		0x10c



#define REG_PWM_E			0x170
#define REG_PWM_F			0x174
#define REG_MISC_EF			0x178
#define REG_DS_E_F			0x17c
#define REG_TIME_EF			0x180
#define REG_PWM_E2			0x184
#define REG_PWM_F2			0x188
#define REG_BLINK_EF		0x18c


#define REG_PWM_AO_A			0x0
#define REG_PWM_AO_B			0x4
#define REG_MISC_AO_AB			0x8
#define REG_DS_AO_A_B			0xc
#define REG_TIME_AO_AB			0x10
#define REG_PWM_AO_A2			0x14
#define REG_PWM_AO_B2			0x18
#define REG_BLINK_AO_AB			0x1c


#define FIN_FREQ			(24 * 1000)
#define DUTY_MAX			1024

#define AML_PWM_NUM			8
#define AML_PWM_NUM_NEW		16


enum pwm_channel {
	PWM_A = 0,
	PWM_B,
	PWM_C,
	PWM_D,
	PWM_E,
	PWM_F,
	PWM_AO_A,
	PWM_AO_B,

	PWM_A2,
	PWM_B2,
	PWM_C2,
	PWM_D2,
	PWM_E2,
	PWM_F2,
	PWM_AO_A2,
	PWM_AO_B2,
};

/*pwm att*/
struct aml_pwm_channel {
	unsigned int pwm_hi;
	unsigned int pwm_lo;
	unsigned int pwm_pre_div;

	unsigned int period_ns;
	unsigned int duty_ns;
	unsigned int pwm_freq;
};

/*pwm regiset att*/
struct aml_pwm_variant {
	u8 output_mask;
	u16 output_mask_new;
/*
 *add for gxtvbb , gxl , gxm
 */

	unsigned int times;
/*
 *include above and add for txl
 */
	unsigned int constant;
	unsigned int blink_enable;
	unsigned int blink_times;
};

struct aml_pwm_chip {
	struct pwm_chip chip;
	void __iomem *base;
	void __iomem *ao_base;
	struct aml_pwm_variant variant;
	u8 inverter_mask;

	unsigned int clk_mask;
	struct clk	*xtal_clk;
	struct clk	*vid_pll_clk;
	struct clk	*fclk_div4_clk;
	struct clk	*fclk_div3_clk;

};

struct aml_pwm_chip *to_aml_pwm_chip(struct pwm_chip *chip);
void pwm_set_reg_bits(void __iomem  *reg,
						unsigned int mask,
						const unsigned int val);

void pwm_write_reg(void __iomem *reg,
						const unsigned int  val);

void pwm_clear_reg_bits(void __iomem *reg, const unsigned int  val);

void pwm_write_reg1(void __iomem *reg, const unsigned int  val);


int meson_pwm_sysfs_init(struct device *dev);

#if IS_ENABLED(CONFIG_AMLOGIC_PWM)
int pwm_constant_enable(struct aml_pwm_chip *chip, int index);
int pwm_constant_disable(struct aml_pwm_chip *chip, int index);

int pwm_blink_enable(struct aml_pwm_chip *chip, int index);
int pwm_blink_disable(struct aml_pwm_chip *chip, int index);

int pwm_set_times(struct aml_pwm_chip *chip,
						int index, int value);
int pwm_set_blink_times(struct aml_pwm_chip *chip,
								int index,
								int value);

#else
static inline int pwm_constant_enable
				(struct aml_pwm_chip *chip, int index)
{
	return -EINVAL;
}

static inline int pwm_constant_disable
				(struct aml_pwm_chip *chip, int index)
{
	return -EINVAL;
}

static inline int pwm_blink_enable
				(struct aml_pwm_chip *chip, int index)
{
	return -EINVAL;
}

static inline int pwm_blink_disable
				(struct aml_pwm_chip *chip, int index)
{
	return -EINVAL;
}

static inline int pwm_set_times(struct aml_pwm_chip *chip,
						int index, int value)
{
	return -EINVAL;
}

static inline int pwm_set_blink_times(struct aml_pwm_chip *chip,
								int index,
								int value)
{
	return -EINVAL;
}

#endif  /* IS_ENABLED(CONFIG_PWM_MESON) */

#endif   /* _PWM_MESON_H_ */

