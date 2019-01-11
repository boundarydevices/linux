/*
 * drivers/amlogic/media/gdc/inc/sys/system_log.h
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

#ifndef __SYSTEM_LOG_H__
#define __SYSTEM_LOG_H__

//changeable logs
#include <linux/kernel.h>
extern unsigned int gdc_log_level;

enum log_level_e {
	LOG_NO_THING,
	LOG_CRIT,
	LOG_ERR,
	LOG_WARNING,
	LOG_INFO,
	LOG_DEBUG,
	LOG_MAX
};

#if 1
#define gdc_log(level, fmt, ...)			 \
	do {			\
		if (level <= gdc_log_level)		\
			pr_info("%s: "fmt, __func__, ##__VA_ARGS__);  \
	} while (0)

#else
#define gdc_log(...)
#endif

#endif // __SYSTEM_LOG_H__
