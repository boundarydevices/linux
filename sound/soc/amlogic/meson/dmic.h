/*
 * sound/soc/amlogic/meson/dmic.h
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

#ifndef __DMIC_H__
#define __DMIC_H__

struct aml_dmic_priv {
	void __iomem *pdm_base;
	struct pinctrl *dmic_pins;
	struct clk *clk_pdm;
	struct clk *clk_mclk;
};

extern struct aml_dmic_priv *dmic_pub;

#endif
