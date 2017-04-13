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

#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/export.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/time.h>
#include <linux/clk.h>
#include <linux/of_address.h>
#include <linux/amlogic/cpu_version.h>


#define AML_PWM_M8BB_NUM		6
#define AML_PWM_GXBB_NUM		8
#define AML_PWM_GXTVBB_NUM		16
#define AML_PWM_TXLX_NUM		20



enum pwm_channel {
	PWM_A = 0,
	PWM_B,
	PWM_C,
	PWM_D,
	PWM_E,
	PWM_F,
	PWM_AO_A,
	PWM_AO_B,
	PWM_AO_C,
	PWM_AO_D,

	PWM_A2,
	PWM_B2,
	PWM_C2,
	PWM_D2,
	PWM_E2,
	PWM_F2,
	PWM_AO_A2,
	PWM_AO_B2,
	PWM_AO_C2,
	PWM_AO_D2,
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
	u32 output_mask;
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

/*
 * add addr if hardware add
 *exam: txlx add pwm ao c/d
 */
struct aml_pwm_baseaddr {
	void __iomem *ab_base;
	void __iomem *cd_base;
	void __iomem *ef_base;
	void __iomem *aoab_base;
	void __iomem *aocd_base;
};

struct aml_pwm_chip {
	struct pwm_chip chip;
	struct aml_pwm_baseaddr baseaddr;
	void __iomem *ao_blink_base;/*for txl*/
	struct aml_pwm_variant variant;
	u32 inverter_mask;
	unsigned int clk_mask;
	struct clk	*xtal_clk;
	struct clk	*vid_pll_clk;
	struct clk	*fclk_div4_clk;
	struct clk	*fclk_div3_clk;
};

/*there are 8 registers
 *for each pwm group
 */
struct pwm_aml_regs {
	u32 dar;/* A Duty Register */
	u32 dbr;/* B Duty Register */
	u32 miscr;/* misc Register */
	u32 dsr;/*DS Register*/
	u32 tr;/*times Register*/
	u32 da2r;/* A2 Duty Register */
	u32 db2r;/* B2 Duty Register */
	u32 br;/*Blink Register*/
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

