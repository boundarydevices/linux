/*
 * drivers/amlogic/media/common/ge2d/ge2d_log.h
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

#ifndef _GE2D_LOG_H_
#define _GE2D_LOG_H_

#include <stdarg.h>
#include <linux/printk.h>

#define GE2D_LOG_TAG "[GE2D]"
#define GE2D_LOG_LEVEL_NULL 0
#define GE2D_LOG_LEVEL_DEBUG 1
#define GE2D_LOG_LEVEL_DEBUG2 2

extern unsigned int ge2d_log_level;
#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#define ge2d_log_info(fmt, ...) \
	pr_info(fmt, ##__VA_ARGS__)

#define ge2d_log_err(fmt, ...) \
	pr_err(fmt, ##__VA_ARGS__)

#define ge2d_log_dbg(fmt, ...) \
	do { \
		if (ge2d_log_level >= GE2D_LOG_LEVEL_DEBUG) { \
			pr_info(fmt, ##__VA_ARGS__); \
		} \
	} while (0)

#define ge2d_log_dbg2(fmt, ...) \
	do { \
		if (ge2d_log_level >= GE2D_LOG_LEVEL_DEBUG2) { \
			pr_info(fmt, ##__VA_ARGS__); \
		} \
	} while (0)

#endif
