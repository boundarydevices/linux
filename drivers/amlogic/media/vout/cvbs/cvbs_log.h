/*
 * drivers/amlogic/media/vout/cvbs/cvbs_log.h
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

#ifndef _CVBS_LOG_H_
#define _CVBS_LOG_H_

#include <stdarg.h>
#include <linux/printk.h>

#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#define cvbs_log_info(fmt, ...) \
	pr_info(fmt, ##__VA_ARGS__)

#define cvbs_log_err(fmt, ...) \
	pr_err(fmt, ##__VA_ARGS__)

#define cvbs_log_dbg(fmt, ...) \
	pr_warn(fmt, ##__VA_ARGS__)

#endif
