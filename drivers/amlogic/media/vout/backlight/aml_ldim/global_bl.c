/*
 * drivers/amlogic/media/vout/backlight/aml_ldim/global_bl.c
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

#include <linux/init.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/amlogic/media/vout/lcd/aml_ldim.h>
#include <linux/amlogic/media/vout/lcd/aml_bl.h>
#include "ldim_drv.h"
#include "ldim_dev_drv.h"

static int global_on_flag;

struct global {
	struct class cls;
	unsigned char cur_addr;
};

struct global *bl_global;

static int global_hw_init_on(void)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();

	ldim_set_duty_pwm(&(ldim_drv->ldev_conf->pwm_config));
	ldim_drv->pinmux_ctrl(1);
	mdelay(2);
	ldim_gpio_set(ldim_drv->ldev_conf->en_gpio,
		ldim_drv->ldev_conf->en_gpio_on);
	mdelay(20);

	return 0;
}

static int global_hw_init_off(void)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();

	ldim_gpio_set(ldim_drv->ldev_conf->en_gpio,
		ldim_drv->ldev_conf->en_gpio_off);
	ldim_drv->pinmux_ctrl(0);
	ldim_pwm_off(&(ldim_drv->ldev_conf->pwm_config));

	return 0;
}

static int global_smr(unsigned short *buf, unsigned char len)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	unsigned int dim_max, dim_min;
	unsigned int level, val;

	if (global_on_flag == 0) {
		if (ldim_debug_print)
			LDIMPR("%s: on_flag=%d\n", __func__, global_on_flag);
		return 0;
		}

	if (len != 1) {
		LDIMERR("%s: data len %d invalid\n", __func__, len);
		return -1;
	}

	dim_max = ldim_drv->ldev_conf->dim_max;
	dim_min = ldim_drv->ldev_conf->dim_min;
	level = buf[0];
	val = dim_min + ((level * (dim_max - dim_min)) / LD_DATA_MAX);

	ldim_drv->ldev_conf->pwm_config.pwm_duty = val;
	ldim_set_duty_pwm(&(ldim_drv->ldev_conf->pwm_config));

	return 0;
}

static int global_power_on(void)
{
	if (global_on_flag) {
		LDIMPR("%s: global is already on, exit\n", __func__);
		return 0;
	}

	global_hw_init_on();
	global_on_flag = 1;

	LDIMPR("%s: ok\n", __func__);
	return 0;
}

static int global_power_off(void)
{
	global_on_flag = 0;
	global_hw_init_off();

	LDIMPR("%s: ok\n", __func__);
	return 0;
}

static ssize_t global_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	int ret = 0;

	if (!strcmp(attr->attr.name, "status")) {
		ret = sprintf(buf, "global status:\n"
				"dev_index      = %d\n"
				"on_flag        = %d\n"
				"en_on          = %d\n"
				"en_off         = %d\n"
				"dim_max        = %d\n"
				"dim_min        = %d\n"
				"pwm_duty       = %d%%\n\n",
				ldim_drv->dev_index, global_on_flag,
				ldim_drv->ldev_conf->en_gpio_on,
				ldim_drv->ldev_conf->en_gpio_off,
				ldim_drv->ldev_conf->dim_max,
				ldim_drv->ldev_conf->dim_min,
				ldim_drv->ldev_conf->pwm_config.pwm_duty);
	}

	return ret;
}

static struct class_attribute global_class_attrs[] = {
	__ATTR(status, 0644, global_show, NULL),
	__ATTR_NULL
};

static int global_ldim_driver_update(void)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();

	ldim_drv->device_power_on = global_power_on;
	ldim_drv->device_power_off = global_power_off;
	ldim_drv->device_bri_update = global_smr;

	return 0;
}

int ldim_dev_global_probe(void)
{
	int ret;

	global_on_flag = 0;
	bl_global = kzalloc(sizeof(struct global), GFP_KERNEL);
	if (bl_global == NULL) {
		pr_err("malloc bl_global failed\n");
		return -1;
	}

	global_ldim_driver_update();

	bl_global->cls.name = kzalloc(10, GFP_KERNEL);
	if (bl_global->cls.name == NULL) {
		LDIMERR("%s: malloc failed\n", __func__);
		return -1;
	}
	sprintf((char *)bl_global->cls.name, "global");
	bl_global->cls.class_attrs = global_class_attrs;
	ret = class_register(&bl_global->cls);
	if (ret < 0)
		pr_err("register global class failed\n");

	global_on_flag = 1; /* default enable in uboot */
	LDIMPR("%s ok\n", __func__);

	return ret;
}

int ldim_dev_global_remove(void)
{
	kfree(bl_global);
	bl_global = NULL;

	return 0;
}

