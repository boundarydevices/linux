/*
 * drivers/amlogic/pinctrl/pinconf-meson-g12a.h
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

#ifndef PINCONF_MESON_G12A_H
#define PINCONF_MESON_G12A_H

#include <linux/pinctrl/pinconf.h>

struct meson_drive_bank {
	const char *name;
	unsigned int first;
	unsigned int last;
	struct meson_reg_desc reg;
};

struct meson_drive_data {
	struct meson_drive_bank *drive_banks;
	unsigned int num_drive_banks;
};

#define BANK_DRIVE(n, f, l, r, o)					\
	{								\
		.name   = n,						\
		.first	= f,						\
		.last	= l,						\
		.reg	= {r, o},					\
	}

extern const struct pinconf_ops meson_g12a_pinconf_ops;

#endif /*PINCONF_MESON_G12A_H*/
