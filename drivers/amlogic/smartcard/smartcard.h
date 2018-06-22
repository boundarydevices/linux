/*
 * drivers/amlogic/smartcard/smartcard.h
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

#ifndef _AML_SMC_H_
#define _AML_SMC_H_
extern int amlogic_gpio_name_map_num(const char *name);
extern int amlogic_gpio_direction_output(unsigned int pin, int value,
	const char *owner);
extern int amlogic_gpio_request(unsigned int pin, const char *label);
extern unsigned long get_mpeg_clk(void);
extern long smc_get_reg_base(void);
#endif
