/*
 * include/linux/amlogic/adc_keypad.h
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

struct adc_key {
	int code;	/* input key code */
	const char *name;
	int chan;
	int value;	/* voltage/3.3v * 1023 */
	int tolerance;
};

struct adc_kp_platform_data {
	struct adc_key *key;
	int key_num;
	int repeat_delay;
	int repeat_period;
};

#endif
