/*
 * drivers/amlogic/input/keyboard/adc_keypad.h
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

#ifndef __LINUX_ADC_KEYPAD_H
#define __LINUX_ADC_KEYPAD_H
#include <linux/list.h>
#include <linux/input.h>
#include <linux/kobject.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/iio/consumer.h>
#include <dt-bindings/iio/adc/amlogic-saradc.h>
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
#include <linux/amlogic/pm.h>
#endif

#define DRIVE_NAME "adc_keypad"
#define MAX_NAME_LEN 20

enum TOLERANCE_RANGE {
	TOL_MIN = 0,
	TOL_MAX = 255
};

enum SAMPLE_VALUE_RANGE {
	SAM_MIN = 0,
	SAM_MAX = 4095 /*12bit adc*/
};

struct adc_key {
	char name[MAX_NAME_LEN];
	unsigned int chan;
	unsigned int code;  /* input key code */
	int value; /* voltage/3.3v * 1023 */
	int tolerance;
	struct list_head list;
};

struct meson_adc_kp {
	unsigned char chan[SARADC_CH_NUM];
	unsigned char chan_num;   /*number of channel exclude duplicate*/
	unsigned char count;
	unsigned int report_code;
	unsigned int prev_code;
	unsigned int poll_period; /*key scan period*/
	struct mutex kp_lock;
	struct class kp_class;
	struct list_head adckey_head;
	struct input_polled_dev *poll_dev;
	struct iio_channel *pchan[SARADC_CH_NUM];
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	struct early_suspend early_suspend;
#endif
};

#endif
