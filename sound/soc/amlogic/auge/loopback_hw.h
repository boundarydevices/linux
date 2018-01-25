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

#include "audio_utils.h"

struct lb_cfg {
	/*
	 * 0: extend bits as "0"
	 * 1: extend bits as "msb"
	 */
	unsigned int ext_signed;
	/* total channel number */
	unsigned int chnum;
	/* which channel is selected for loopback */
	unsigned int chmask;
};

struct data_in {
	struct lb_cfg *config;
	struct audio_data *ddrdata;
};

struct data_lb {
	struct lb_cfg *config;
	unsigned int ddr_type;
	unsigned int msb;
	unsigned int lsb;
};

struct loopback {
	struct device *dev;
	unsigned int lb_mode;

	struct data_in *datain;
	struct data_lb *datalb;
};


extern void datain_config(struct data_in *datain);

extern void datalb_config(struct data_lb *datalb);

extern void datalb_ctrl(struct loopback_cfg *lb_cfg);

extern int lb_is_enable(void);

extern void lb_enable(bool is_enable);

extern void lb_mode(int mode);

extern void tdmin_lb_enable(int tdm_index, int in_enable);

extern void tdmin_lb_fifo_enable(int is_enable);
#endif
