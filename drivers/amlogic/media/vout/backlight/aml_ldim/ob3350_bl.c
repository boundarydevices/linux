/*
 * drivers/amlogic/media/vout/backlight/aml_ldim/ob3350_bl.c
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

static int ob3350_on_flag;

struct ob3350 {
	struct class cls;
	unsigned char cur_addr;
};

struct ob3350 *bl_ob3350;

static int ob3350_hw_init_on(void)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();

	ldim_gpio_set(ldim_drv->ldev_conf->en_gpio,
		ldim_drv->ldev_conf->en_gpio_on);
	mdelay(2);

	ldim_set_duty_pwm(&(ldim_drv->ldev_conf->pwm_config));
	ldim_drv->pinmux_ctrl(1);
	mdelay(20);

	return 0;
}

static int ob3350_hw_init_off(void)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();

	ldim_gpio_set(ldim_drv->ldev_conf->en_gpio,
		ldim_drv->ldev_conf->en_gpio_off);
	ldim_drv->pinmux_ctrl(0);
	ldim_pwm_off(&(ldim_drv->ldev_conf->pwm_config));

	return 0;
}

static int ob3350_smr(unsigned short *buf, unsigned char len)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	unsigned int dim_max, dim_min;
	unsigned int level, val;

	if (ob3350_on_flag == 0) {
		if (ldim_debug_print)
			LDIMPR("%s: on_flag=%d\n", __func__, ob3350_on_flag);
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

static int ob3350_power_on(void)
{
	if (ob3350_on_flag) {
		LDIMPR("%s: ob3350 is already on, exit\n", __func__);
		return 0;
	}

	ob3350_hw_init_on();
	ob3350_on_flag = 1;

	LDIMPR("%s: ok\n", __func__);
	return 0;
}

static int ob3350_power_off(void)
{
	ob3350_on_flag = 0;
	ob3350_hw_init_off();

	LDIMPR("%s: ok\n", __func__);
	return 0;
}

static ssize_t ob3350_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	int ret = 0;

	if (!strcmp(attr->attr.name, "status")) {
		ret = sprintf(buf, "ob3350 status:\n"
				"dev_index      = %d\n"
				"on_flag        = %d\n"
				"en_on          = %d\n"
				"en_off         = %d\n"
				"dim_max        = %d\n"
				"dim_min        = %d\n"
				"pwm_duty       = %d%%\n\n",
				ldim_drv->dev_index, ob3350_on_flag,
				ldim_drv->ldev_conf->en_gpio_on,
				ldim_drv->ldev_conf->en_gpio_off,
				ldim_drv->ldev_conf->dim_max,
				ldim_drv->ldev_conf->dim_min,
				ldim_drv->ldev_conf->pwm_config.pwm_duty);
	}

	return ret;
}

static struct class_attribute ob3350_class_attrs[] = {
	__ATTR(status, 0644, ob3350_show, NULL),
	__ATTR_NULL
};

static int ob3350_ldim_driver_update(void)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();

	ldim_drv->device_power_on = ob3350_power_on;
	ldim_drv->device_power_off = ob3350_power_off;
	ldim_drv->device_bri_update = ob3350_smr;

	return 0;
}

int ldim_dev_ob3350_probe(void)
{
	int ret;

	ob3350_on_flag = 0;
	bl_ob3350 = kzalloc(sizeof(struct ob3350), GFP_KERNEL);
	if (bl_ob3350 == NULL) {
		pr_err("malloc bl_ob3350 failed\n");
		return -1;
	}

	ob3350_ldim_driver_update();

	bl_ob3350->cls.name = kzalloc(10, GFP_KERNEL);
	sprintf((char *)bl_ob3350->cls.name, "ob3350");
	bl_ob3350->cls.class_attrs = ob3350_class_attrs;
	ret = class_register(&bl_ob3350->cls);
	if (ret < 0)
		pr_err("register ob3350 class failed\n");

	ob3350_on_flag = 1; /* default enable in uboot */
	LDIMPR("%s ok\n", __func__);

	return ret;
}

int ldim_dev_ob3350_remove(void)
{
	kfree(bl_ob3350);
	bl_ob3350 = NULL;

	return 0;
}

