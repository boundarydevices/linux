/*
 * drivers/amlogic/mailbox/meson_mhu.h
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

#define CONTROLLER_NAME		"mhu_ctlr"

#define CHANNEL_MAX		2
#define CHANNEL_LOW_PRIORITY	"cpu_to_scp_low"
#define CHANNEL_HIGH_PRIORITY	"cpu_to_scp_high"

struct mhu_data_buf {
	u32 cmd;
	int tx_size;
	void *tx_buf;
	int rx_size;
	void *rx_buf;
	void *cl_data;
};

extern struct device *the_scpi_device;
