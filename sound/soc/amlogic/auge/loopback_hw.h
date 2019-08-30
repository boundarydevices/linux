/*
 * sound/soc/amlogic/auge/loopback_hw.h
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

#ifndef __AML_LOOPBACK_HW_H__
#define __AML_LOOPBACK_HW_H__

#include <linux/types.h>
#include "loopback.h"

struct data_cfg {
	/*
	 * 0: extend bits as "0"
	 * 1: extend bits as "msb"
	 */
	unsigned int ext_signed;
	/* channel number */
	unsigned int chnum;
	/* channel selected */
	unsigned int chmask;
	/* combined data */
	unsigned int type;
	/* the msb positioin in data */
	unsigned int m;
	/* the lsb position in data */
	unsigned int n;

	/* loopback datalb src */
	unsigned int src;

	unsigned int datalb_src;

	/* channel and mask in new ctrol register */
	bool ch_ctrl_switch;
};

void tdminlb_set_clk(enum datalb_src lb_src,
		     int sclk_div, int ratio, bool enable);

extern void tdminlb_set_format(int i2s_fmt);

void tdminlb_set_ctrl(enum datalb_src src);

extern void tdminlb_enable(int tdm_index, int in_enable);

extern void tdminlb_fifo_enable(int is_enable);

extern void tdminlb_set_format(int i2s_fmt);
void tdminlb_set_lanemask_and_chswap
	(int swap, int lane_mask, unsigned int mask);

extern void tdminlb_set_src(int src);
extern void lb_set_datain_src(int id, int src);

extern void lb_set_datain_cfg(int id, struct data_cfg *datain_cfg);
extern void lb_set_datalb_cfg(int id, struct data_cfg *datalb_cfg);

extern void lb_enable(int id, bool enable);

#endif
