/*
 * drivers/amlogic/media/vout/cvbs/enc_clk_config.h
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

#ifndef __ENC_CLK_CONFIG_H__
#define __ENC_CLK_CONFIG_H__

extern unsigned int cvbs_clk_path;
void set_vmode_clk(void);
extern void disable_vmode_clk(void);
extern int cvbs_cpu_type(void);

#endif
