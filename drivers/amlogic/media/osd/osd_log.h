/*
 * drivers/amlogic/media/osd/osd_log.h
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

#ifndef _OSD_LOG_H_
#define _OSD_LOG_H_

#include <stdarg.h>
#include <linux/printk.h>

#define OSD_LOG_LEVEL_NULL 0
#define OSD_LOG_LEVEL_DEBUG 1
#define OSD_LOG_LEVEL_DEBUG2 2
#define OSD_LOG_LEVEL_DEBUG3 3

#define MODULE_BASE   (1 << 0)
#define MODULE_RENDER (1 << 1)
#define MODULE_FENCE  (1 << 2)
#define MODULE_BLEND  (1 << 3)
#define MODULE_CURSOR (1 << 4)

extern unsigned int osd_log_level;
extern unsigned int osd_log_module;

#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#define osd_log_info(fmt, ...) \
	pr_info(fmt, ##__VA_ARGS__)

#define osd_log_err(fmt, ...) \
	pr_err(fmt, ##__VA_ARGS__)

#define osd_log_dbg(moudle, fmt, ...) \
	do { \
		if (osd_log_level >= OSD_LOG_LEVEL_DEBUG) { \
			if (moudle & osd_log_module) { \
				pr_info(fmt, ##__VA_ARGS__); \
			} \
		} \
	} while (0)

#define osd_log_dbg2(moudle, fmt, ...) \
	do { \
		if (osd_log_level >= OSD_LOG_LEVEL_DEBUG2) { \
			if (moudle & osd_log_module) { \
				pr_info(fmt, ##__VA_ARGS__); \
			} \
		} \
	} while (0)

#define osd_log_dbg3(moudle, fmt, ...) \
	do { \
		if (osd_log_level >= OSD_LOG_LEVEL_DEBUG3) { \
			if (moudle & osd_log_module) { \
				pr_info(fmt, ##__VA_ARGS__); \
			} \
		} \
	} while (0)
#endif
