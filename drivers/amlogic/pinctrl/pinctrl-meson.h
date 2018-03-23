/*
 * drivers/amlogic/pinctrl/pinctrl-meson.h
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

#include <linux/gpio.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/types.h>
#include <linux/module.h>
#include "../../pinctrl/core.h"

/**
 * struct meson_pmx_group - a pinmux group
 *
 * @name:	group name
 * @pins:	pins in the group
 * @num_pins:	number of pins in the group
 * @is_gpio:	whether the group is a single GPIO group
 * @reg:	register offset for the group in the domain mux registers
 * @bit		bit index enabling the group
 * @domain:	index of the domain this group belongs to
 */
struct meson_pmx_group {
	const char *name;
	const unsigned int *pins;
	unsigned int num_pins;
	bool is_gpio;
	const void *data;
};

/**
 * struct meson_pmx_func - a pinmux function
 *
 * @name:	function name
 * @groups:	groups in the function
 * @num_groups:	number of groups in the function
 */
struct meson_pmx_func {
	const char *name;
	const char * const *groups;
	unsigned int num_groups;
};

/**
 * struct meson_reg_desc - a register descriptor
 *
 * @reg:	register offset in the regmap
 * @bit:	bit index in register
 *
 * The structure describes the information needed to control pull,
 * pull-enable, direction, etc. for a single pin
 */
struct meson_reg_desc {
	unsigned int reg;
	unsigned int bit;
};

/**
 * enum meson_reg_type - type of registers encoded in @meson_reg_desc
 */
enum meson_reg_type {
	REG_PULLEN,
	REG_PULL,
	REG_DIR,
	REG_OUT,
	REG_IN,
	NUM_REG,
};

/**
 * struct meson bank
 *
 * @name:	bank name
 * @first:	first pin of the bank
 * @last:	last pin of the bank
 * @irq:	irq base number of the bank
 * @regs:	array of register descriptors
 *
 * A bank represents a set of pins controlled by a contiguous set of
 * bits in the domain registers. The structure specifies which bits in
 * the regmap control the different functionalities. Each member of
 * the @regs array refers to the first pin of the bank.
 */
struct meson_bank {
	const char *name;
	unsigned int first;
	unsigned int last;
	int irq;
	struct meson_reg_desc regs[NUM_REG];
};

struct meson_pinctrl;
struct meson_pinctrl_data {
	const char *name;
	int (*init)(struct meson_pinctrl *);
	const struct pinctrl_pin_desc *pins;
	struct meson_pmx_group *groups;
	struct meson_pmx_func *funcs;
	const struct meson_desc_pin *meson_pins;
	struct meson_bank *banks;
	unsigned int num_pins;
	unsigned int num_groups;
	unsigned int num_funcs;
	unsigned int num_banks;
	const struct pinmux_ops *pmx_ops;
	const struct pinconf_ops *pcf_ops;
	void *pmx_data;
	void *pcf_drv_data;
};

struct meson_pinctrl {
	struct device *dev;
	struct pinctrl_dev *pcdev;
	struct pinctrl_desc desc;
	struct meson_pinctrl_data *data;
	struct regmap *reg_mux;
	struct regmap *reg_pullen;
	struct regmap *reg_pull;
	struct regmap *reg_gpio;
	struct regmap *reg_drive;
	struct gpio_chip chip;
	struct device_node *of_node;
	struct device_node *of_irq;
};

#define CMD_TEST_N_DIR 0x82000046
#define TEST_N_OUTPUT  1

#define MESON_PIN(x) PINCTRL_PIN(x, #x)

#define FUNCTION(fn)							\
	{								\
		.name = #fn,						\
		.groups = fn ## _groups,				\
		.num_groups = ARRAY_SIZE(fn ## _groups),		\
	}

#define BANK(n, f, l, i, per, peb, pr, pb, dr, db, or, ob, ir, ib)\
	{								\
		.name	= n,						\
		.first	= f,						\
		.last	= l,						\
		.irq    = i,						\
		.regs	= {						\
			[REG_PULLEN]	= { per, peb },			\
			[REG_PULL]	= { pr, pb },			\
			[REG_DIR]	= { dr, db },			\
			[REG_OUT]	= { or, ob },			\
			[REG_IN]	= { ir, ib },			\
		},							\
	}

extern int meson_pinctrl_probe(struct platform_device *pdev);

/* Common pmx functions */
extern int meson_pmx_get_funcs_count(struct pinctrl_dev *pcdev);
extern const char *meson_pmx_get_func_name(struct pinctrl_dev *pcdev,
					unsigned int selector);
extern int meson_pmx_get_groups(struct pinctrl_dev *pcdev,
					unsigned int selector,
					const char * const **groups,
					unsigned int * const num_groups);

/* Common pinconf functions */
extern int meson_pinconf_common_get(struct meson_pinctrl *pc, unsigned int pin,
				enum pin_config_param param, u16 *arg);

extern int meson_pinconf_common_set(struct meson_pinctrl *pc, unsigned int pin,
					enum pin_config_param param, u16 arg);
