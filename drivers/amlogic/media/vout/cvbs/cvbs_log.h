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

ssize_t show_info(char *name, char *buf);
inline ssize_t show_info(char *name, char *buf)
{
	return snprintf(buf, 40, "%s\n", name);
}

#define  STORE_INFO(name) \
	{mutex_lock(&cvbs_mutex); \
	snprintf(name, 40, "%s", buf); \
	mutex_unlock(&cvbs_mutex); }

#define SET_CVBS_CLASS_ATTR(name, op) \
static char name[40]; \
static ssize_t aml_CVBS_attr_##name##_show(struct class *cla, \
		struct class_attribute *attr, char *buf) \
{ \
	return show_info(name, buf); \
} \
static ssize_t  aml_CVBS_attr_##name##_store(struct class *cla, \
		struct class_attribute *attr, \
			    const char *buf, size_t count) \
{ \
	STORE_INFO(name); \
	op(name); \
	return strnlen(buf, count); \
} \
struct  class_attribute  class_CVBS_attr_##name =  \
__ATTR(name, 0644, \
		aml_CVBS_attr_##name##_show, aml_CVBS_attr_##name##_store)


#endif
