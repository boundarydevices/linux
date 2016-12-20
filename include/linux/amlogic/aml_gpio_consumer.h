/*
 * include/linux/amlogic/aml_gpio_consumer.h
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
#ifndef __AML_GPIO_CONSUMER_H__
#define __AML_GPIO_CONSUMER_H__
#include <linux/gpio/consumer.h>
#include <linux/of_gpio.h>
#define AML_GPIO_IRQ(irq_bank, filter, type) \
((irq_bank&0x7)|(filter&0x7)<<8|(type&0x3)<<16)

enum {
	GPIO_IRQ0 = 0,
	GPIO_IRQ1,
	GPIO_IRQ2,
	GPIO_IRQ3,
	GPIO_IRQ4,
	GPIO_IRQ5,
	GPIO_IRQ6,
	GPIO_IRQ7,
};

enum {
	GPIO_IRQ_HIGH = 0,
	GPIO_IRQ_LOW,
	GPIO_IRQ_RISING,
	GPIO_IRQ_FALLING,
};

enum {
	FILTER_NUM0 = 0,
	FILTER_NUM1,
	FILTER_NUM2,
	FILTER_NUM3,
	FILTER_NUM4,
	FILTER_NUM5,
	FILTER_NUM6,
	FILTER_NUM7,
};

#endif
