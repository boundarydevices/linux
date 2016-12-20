/*
 * include/linux/amlogic/gpio-amlogic.h
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

#ifndef __LINUX_AMLOGIC_GPIO_AMLOGIC_H
#define __LINUX_AMLOGIC_GPIO_AMLOGIC_H
#include <linux/types.h>
#include <linux/io.h>
#define GPIO_REG_BIT(reg, bit) ((reg<<5)|bit)
#define GPIO_REG(value) ((value>>5))
#define GPIO_BIT(value) ((value&0x1F))
struct amlogic_gpio_desc {
	unsigned int num;
	char *name;
	unsigned int out_en_reg_bit;
	unsigned int out_value_reg_bit;
	unsigned int input_value_reg_bit;
	unsigned int map_to_irq;
	unsigned int pull_up_reg_bit;
	const char *gpio_owner;
};

extern struct amlogic_gpio_desc amlogic_pins[];
extern int gpio_max;
extern int (*gpio_amlogic_name_to_num)(const char *name);
extern int m8_pin_to_pullup(unsigned int pin, unsigned int *reg,
	unsigned int *bit, unsigned int *bit_en);

extern struct amlogic_set_pullup pullup_ops;
extern void __iomem *p_pull_up_addr[6];
extern void __iomem *p_pin_mux_reg_addr[11];
extern void __iomem *p_pull_upen_addr[6];

extern int gpio_irq;
extern int gpio_flag;
extern int m8_pin_map_to_direction(unsigned int pin,
		unsigned int *reg, unsigned int *bit);

extern void __iomem *p_pin_mux_reg_addr[11];
extern void __iomem *p_pull_up_addr[6];
extern void __iomem *p_gpio_oen_addr[5];
extern struct amlogic_gpio_desc amlogic_pins[];

struct amlogic_set_pullup {
	 int (*meson_set_pullup)(unsigned int, unsigned int, unsigned int);
};

static inline uint32_t aml_read_reg32(void __iomem *_reg)
{
	return readl_relaxed(_reg);
};

static inline void aml_write_reg32(void __iomem *_reg,
		const uint32_t _value)
{
	writel_relaxed(_value, _reg);
};

static inline void aml_set_reg32_bits(void __iomem *_reg,
		const uint32_t _value,
		const uint32_t _start,
		const uint32_t _len)
{
	writel_relaxed(((readl_relaxed(_reg) &
					~(((1L << (_len))-1) << (_start))) |
				(((_value)&((1L<<(_len))-1)) << (_start))),
			_reg);
}

static inline void aml_clrset_reg32_bits(void __iomem *_reg,
		const uint32_t clr, const uint32_t set)
{
	writel_relaxed((readl_relaxed(_reg) & ~(clr)) | (set), _reg);
}

static inline uint32_t aml_get_reg32_bits(void __iomem *_reg,
		const uint32_t _start, const uint32_t _len)
{
	return	(readl_relaxed(_reg) >> (_start)) &
			((1L << (_len)) - 1);
}

static inline void aml_set_reg32_mask(void __iomem *_reg,
		const uint32_t _mask)
{
	writel_relaxed((readl_relaxed(_reg) | (_mask)), _reg);
}

static inline void aml_clr_reg32_mask(void __iomem *_reg,
		const uint32_t _mask)
{
	writel_relaxed((readl_relaxed(_reg) & (~(_mask))), _reg);
}
#endif /* __LINUX_AMLOGIC_GPIO_AMLOGIC_H */
