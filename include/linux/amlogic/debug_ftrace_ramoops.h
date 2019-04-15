/*
 * include/linux/amlogic/debug_ftrace_ramoops.h
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

#ifndef __DEBUG_FTRACE_RAMOOPS_H__
#define  __DEBUG_FTRACE_RAMOOPS_H__
#define __DEBUG_FTRACE_RAMOOPS_H__
#include <linux/ftrace.h>

extern unsigned int ramoops_ftrace_en;
extern int ramoops_io_en;
extern unsigned int dump_iomap;

#define PSTORE_FLAG_FUNC	0x1
#define PSTORE_FLAG_IO_R	0x2
#define PSTORE_FLAG_IO_W	0x3
#define PSTORE_FLAG_IO_R_END	0x4
#define PSTORE_FLAG_IO_W_END	0x5
#define PSTORE_FLAG_IO_MAX	0x6
#define PSTORE_FLAG_MASK	0xF

void notrace pstore_io_save(unsigned long reg, unsigned long val,
			    unsigned long parant, unsigned int flag,
			    unsigned long *irq_flag);

//#define SKIP_IO_TRACE
#if (defined CONFIG_AMLOGIC_DEBUG_FTRACE_PSTORE) && (!defined SKIP_IO_TRACE)
#define pstore_ftrace_io_wr(reg, val)	\
unsigned long irqflg;					\
pstore_io_save(reg, val, CALLER_ADDR0, PSTORE_FLAG_IO_W, &irqflg)

#define pstore_ftrace_io_wr_end(reg, val)	\
pstore_io_save(reg, 0, CALLER_ADDR0, PSTORE_FLAG_IO_W_END, &irqflg)

#define pstore_ftrace_io_rd(reg)		\
unsigned long irqflg;					\
pstore_io_save(reg, 0, CALLER_ADDR0, PSTORE_FLAG_IO_R, &irqflg)
#define pstore_ftrace_io_rd_end(reg)	\
pstore_io_save(reg, 0, CALLER_ADDR0, PSTORE_FLAG_IO_R_END, &irqflg)

#define need_dump_iomap()		(ramoops_io_en | dump_iomap)

#else
#define pstore_ftrace_io_wr(reg, val)		do {	} while (0)
#define pstore_ftrace_io_rd(reg)		do {	} while (0)
#define need_dump_iomap()			0
#define pstore_ftrace_io_wr_end(reg, val)	do {	} while (0)
#define pstore_ftrace_io_rd_end(reg)		do {	} while (0)

#endif

#endif
