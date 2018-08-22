/*
 * include/linux/amlogic/dmc_monitor.h
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

#ifndef __DMC_MONITOR_H__
#define __DMC_MONITOR_H__

#define PROTECT_READ		(1 << 0)
#define PROTECT_WRITE		(1 << 1)

#define DMC_MON_RW		0x8200004A

#define DMC_READ		0
#define DMC_WRITE		1

#define DMC_WRITE_VIOLATION	(1 << 1)

/*
 * Address is aligned to 64 KB
 */
#define DMC_ADDR_SIZE		(0x10000)
#define DMC_TAG			"DMC VIOLATION"

struct dmc_monitor;
struct dmc_mon_ops {
	void (*handle_irq)(struct dmc_monitor *mon);
	int  (*set_montor)(struct dmc_monitor *mon);
	void (*disable)(struct dmc_monitor *mon);
	size_t (*dump_reg)(char *buf);
};

struct dmc_monitor {
	unsigned long io_base;
	unsigned long addr_start;
	unsigned long addr_end;
	unsigned int  device;
	unsigned short port_num;
	unsigned short chip;
	unsigned long last_addr;
	unsigned long same_page;
	unsigned long last_status;
	struct ddr_port_desc *port;
	struct dmc_mon_ops   *ops;
	struct delayed_work work;
};

extern void dmc_monitor_disable(void);

/*
 * start: physical start address, aligned to 64KB
 * end: physical end address, aligned to 64KB
 * dev_mask: device bit to set
 * en: 0: close monitor, 1: enable monitor
 */
extern int dmc_set_monitor(unsigned long start, unsigned long end,
			   unsigned long dev_mask, int en);

/*
 * start: physical start address, aligned to 64KB
 * end: physical end address, aligned to 64KB
 * port_name: name of port to set, see ddr_port_desc for each chip in
 *            drivers/amlogic/ddr_tool/ddr_port_desc.c
 * en: 0: close monitor, 1: enable monitor
 */
extern int dmc_set_monitor_by_name(unsigned long start, unsigned long end,
		    const char *port_name, int en);

extern unsigned int get_all_dev_mask(void);

/*
 * Following functions are internal used only
 */
extern unsigned long dmc_rw(unsigned long addr, unsigned long value, int rw);

extern char *to_ports(int id);
extern char *to_sub_ports(int mid, int sid, char *id_str);

extern struct dmc_mon_ops gx_dmc_mon_ops;
extern struct dmc_mon_ops g12_dmc_mon_ops;

#endif /* __DMC_MONITOR_H__ */
