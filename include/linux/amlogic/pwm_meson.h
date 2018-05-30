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
#include <linux/mutex.h>
#include <linux/time.h>
#include <linux/clk.h>
#include <linux/of_address.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/clk-provider.h>
/* for pwm channel index*/
#include <dt-bindings/pwm/meson.h>

/*a group pwm registers offset address
 * for example:
 * PWM A B
 * PWM C D
 * PWM E F
 * PWM AO A
 * PWM AO B
 */
#define REG_PWM_A				0x0
#define REG_PWM_B				0x4
#define REG_MISC_AB				0x8
#define REG_DS_AB				0xc
#define REG_TIME_AB				0x10
#define REG_PWM_A2				0x14
#define REG_PWM_B2				0x18
#define REG_BLINK_AB			0x1c

/* pwm output enable */
#define MISC_A_EN				BIT(0)
#define MISC_B_EN				BIT(1)
#define MISC_A2_EN				BIT(25)
#define MISC_B2_EN				BIT(24)
/* pwm polarity enable */
#define MISC_A_INVERT			BIT(26)
#define MISC_B_INVERT			BIT(27)
/* when you want 0% or 100% waveform
 * constant bit should be set.
 */
#define MISC_A_CONSTANT			BIT(28)
#define MISC_B_CONSTANT			BIT(29)
/*
 * pwm a and b clock enable/disable
 */
#define MISC_A_CLK_EN			BIT(15)
#define MISC_B_CLK_EN			BIT(23)
/*
 * blink control bit
 */
#define BLINK_A					BIT(8)
#define BLINK_B					BIT(9)


#define PWM_HIGH_SHIFT			16
#define MISC_CLK_DIV_MASK		0x7f
#define MISC_B_CLK_DIV_SHIFT	16
#define MISC_A_CLK_DIV_SHIFT	8
#define MISC_B_CLK_SEL_SHIFT	6
#define MISC_A_CLK_SEL_SHIFT	4
#define MISC_CLK_SEL_WIDTH		2
#define PWM_CHANNELS_PER_GROUP	2

static const unsigned int mux_reg_shifts[] = {
	MISC_A_CLK_SEL_SHIFT,
	MISC_B_CLK_SEL_SHIFT,
	MISC_A_CLK_SEL_SHIFT,
	MISC_B_CLK_SEL_SHIFT
};

/*pwm register att*/
struct meson_pwm_variant {
	unsigned int times;
	unsigned int constant;
	unsigned int blink_enable;
	unsigned int blink_times;
};

/*for soc data:
 *double channel enable
 * double_channel = false ,could use PWM A
 * double_channel = true , could use PWM A and PWM A2
 */
struct meson_pwm_data {
	bool double_channel;
	const char * const *parent_names;
};

struct meson_pwm {
	struct pwm_chip chip;
	void __iomem *base;
	struct meson_pwm_data *data;
	struct meson_pwm_variant variant;
	u32 inverter_mask;
	struct mutex lock;
	spinlock_t pwm_lock;
	unsigned int clk_mask;
};

/*the functions only use for meson pwm driver*/
void pwm_set_reg_bits(void __iomem  *reg,
						unsigned int mask,
						const unsigned int val);

void pwm_write_reg(void __iomem *reg,
						const unsigned int  val);

void pwm_clear_reg_bits(void __iomem *reg, const unsigned int  val);

void pwm_write_reg1(void __iomem *reg, const unsigned int  val);

int meson_pwm_sysfs_init(struct device *dev);
void meson_pwm_sysfs_exit(struct device *dev);


#if IS_ENABLED(CONFIG_AMLOGIC_PWM)
int pwm_register_debug(struct meson_pwm *meson);
struct meson_pwm *to_meson_pwm(struct pwm_chip *chip);
int pwm_constant_enable(struct meson_pwm *meson, int index);
int pwm_constant_disable(struct meson_pwm *meson, int index);
int pwm_blink_enable(struct meson_pwm *meson, int index);
int pwm_blink_disable(struct meson_pwm *meson, int index);
int pwm_set_blink_times(struct meson_pwm *meson,
								int index,
								int value);
int pwm_set_times(struct meson_pwm *meson,
						int index, int value);


#else
static inline int pwm_register_debug
				(struct meson_pwm *meson)
{
	return -EINVAL;
}

struct meson_pwm *to_meson_pwm(struct pwm_chip *chip)
{
	return NULL;
}

static inline int pwm_constant_enable
				(struct meson_pwm *meson, int index)
{
	return -EINVAL;
}

static inline int pwm_constant_disable
				(struct meson_pwm *meson, int index)
{
	return -EINVAL;
}

static inline int pwm_blink_enable
				(struct meson_pwm *meson, int index)
{
	return -EINVAL;
}

static inline int pwm_blink_disable
				(struct meson_pwm *meson, int index)
{
	return -EINVAL;
}

static inline int pwm_set_blink_times(struct meson_pwm *meson,
								int index,
								int value);
{
	return -EINVAL;
}
static inline int pwm_set_times(struct meson_pwm *meson,
							int index, int value)

{
	return -EINVAL;
}

#endif  /* IS_ENABLED(CONFIG_PWM_MESON) */

#endif   /* _PWM_MESON_H_ */

