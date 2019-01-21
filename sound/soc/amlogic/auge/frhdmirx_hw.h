/*
 * sound/soc/amlogic/auge/frhdmirx_hw.h
 *
 * Copyright (C) 2018 Amlogic, Inc. All rights reserved.
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
#ifndef __FRHDMIRX_HW_H__
#define __FRHDMIRX_HW_H__

#define INT_PAO_PAPB_MASK    24
#define INT_PAO_PCPD_MASK    16

extern void frhdmirx_enable(bool enable);
extern void frhdmirx_src_select(int src);
extern void frhdmirx_ctrl(int channels, int src);
extern void frhdmirx_clr_PAO_irq_bits(void);
extern unsigned int frhdmirx_get_ch_status0to31(void);
extern unsigned int frhdmirx_get_chan_status_pc(void);
#endif
