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
#define FW_LOG_LEVEL LOG_MAX

enum log_level_e {
	LOG_NOTHING,
	LOG_EMERG,
	LOG_ALERT,
	LOG_CRIT,
	LOG_ERR,
	LOG_WARNING,
	LOG_NOTICE,
	LOG_INFO,
	LOG_DEBUG,
	LOG_IRQ,
	LOG_MAX
};

extern const char *const gdc_log_level[LOG_MAX];

#define FILE (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#if 1
#define LOG(level, fmt, arg...)			 \
	do {			\
		if ((level) <= FW_LOG_LEVEL)		\
		trace_printk("%s: %s(%d) %s: " fmt "\n",\
				FILE, __func__, __LINE__,	  \
				gdc_log_level[level], ## arg);	\
	} while (0)
#else
#define LOG(...)
#endif

#endif // __SYSTEM_LOG_H__
