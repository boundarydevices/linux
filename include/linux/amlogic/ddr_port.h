/*
 * include/linux/amlogic/ddr_port.h
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

#ifndef __DDR_PORT_DESC_H__
#define __DDR_PORT_DESC_H__

#define MAX_PORTS			63
#define MAX_NAME			15
#define PORT_MAJOR			32


struct ddr_port_desc {
	char port_name[MAX_NAME];
	unsigned char port_id;
};

extern int ddr_find_port_desc(int cpu_type, struct ddr_port_desc **desc);
#endif /* __DDR_PORT_DESC_H__ */
