/*
 * drivers/amlogic/led/led_sys.h
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

#ifndef __AML_LED_SYS_H
#define __AML_LED_SYS_H

#include <linux/leds.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>

enum {
	LED_GPIO_STATE_OFF = 0,
	LED_GPIO_STATE_ON,
};

struct aml_led_gpio {
	const char *name;
	unsigned int pin;
	unsigned int active_low;
	unsigned int state;
};


struct aml_sysled_dev {
	struct aml_led_gpio d;
	struct led_classdev cdev;
	enum led_brightness new_brightness;

	struct work_struct work;
	struct mutex lock;
};

#endif
