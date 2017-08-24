/*
 * drivers/amlogic/media/vin/tvin/hdmirx_ext/platform_iface.h
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

#ifndef __PLATFORM_INTERFACE_H__
#define __PLATFORM_INTERFACE_H__

#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include "hdmirx_ext_drv.h"

/* for timer function */
void __plat_timer_set(int timer_cnt);
void __plat_timer_init(void (*timer_func)(void));
int __plat_timer_expired(void);

/* for i2c function */
int __plat_i2c_init(struct hdmirx_ext_hw_s *phw, unsigned char dev_i2c_addr);

int __plat_i2c_read_byte(unsigned char dev_addr, unsigned char offset,
		unsigned char *pvalue);
int __plat_i2c_write_byte(unsigned char dev_addr, unsigned char offset,
		unsigned char value);

int __plat_i2c_read_block(unsigned char dev_addr, unsigned char offset,
		char *buffer, int length);
int __plat_i2c_write_block(unsigned char dev_addr, unsigned char offset,
		char *buffer, int length);

int __plat_gpio_init(unsigned int gpio_index);
void __plat_gpio_output(struct hdmirx_ext_gpio_s *gpio, unsigned int value);

void __plat_msleep(unsigned int msecs);

extern int __plat_get_video_mode_by_name(char *name);
extern char *__plat_get_video_mode_name(int mode);

struct task_struct *__plat_create_thread(int (*threadfn)(void *data),
		void *data, const char namefmt[]);

#endif
