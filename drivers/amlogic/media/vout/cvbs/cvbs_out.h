/*
 * drivers/amlogic/media/vout/cvbs/cvbs_out.h
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

#ifndef _CVBS_OUT_H_
#define _CVBS_OUT_H_

/* Amlogic Headers */
#include <linux/amlogic/media/vout/vout_notify.h>
#include "cvbs_mode.h"

#define CVBS_CLASS_NAME	"cvbs"
#define CVBS_NAME	"cvbs"
#define	MAX_NUMBER_PARA  10

#define _TM_V 'V'

#define VOUT_IOC_CC_OPEN           _IO(_TM_V, 0x01)
#define VOUT_IOC_CC_CLOSE          _IO(_TM_V, 0x02)
#define VOUT_IOC_CC_DATA           _IOW(_TM_V, 0x03, struct vout_CCparm_s)

#define print_info(fmt, args...) pr_info(fmt, ##args)

ssize_t show_info(char *name, char *buf);
inline ssize_t show_info(char *name, char *buf)
{
	return snprintf(buf, 40, "%s\n", name);
}

#define  STORE_INFO(name) \
	{mutex_lock(&cvbs_mutex);\
	snprintf(name, 40, "%s", buf) ;\
	mutex_unlock(&cvbs_mutex); }

#define SET_CVBS_CLASS_ATTR(name, op) \
static char name[40]; \
static ssize_t aml_CVBS_attr_##name##_show(struct class *cla, \
		struct class_attribute *attr, char *buf) \
{ \
	return show_info(name, buf);	\
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

struct disp_module_info_s {
	struct vinfo_s *vinfo;
	struct cdev   *cdev;
	dev_t         devno;
	struct class  *base_class;
	struct device *dev;
};

static  DEFINE_MUTEX(cvbs_mutex);

struct vout_CCparm_s {
	unsigned int type;
	unsigned char data1;
	unsigned char data2;
};

struct reg_s {
	unsigned int reg;
	unsigned int val;
};

struct cvbsregs_set_t {
	enum cvbs_mode_e cvbsmode;
	const struct reg_s *enc_reg_setting;
};

#endif
