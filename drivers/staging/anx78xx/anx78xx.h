/*
 * Copyright(c) 2015, Analogix Semiconductor. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ANX78xx_H
#define __ANX78xx_H

#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/gpio/consumer.h>

#define AUX_ERR  1
#define AUX_OK   0

struct anx78xx_platform_data {
	struct gpio_desc *gpiod_pd;
	struct gpio_desc *gpiod_reset;
	spinlock_t lock;
};

struct anx78xx {
	struct i2c_client *client;
	struct anx78xx_platform_data *pdata;
	struct delayed_work work;
	struct workqueue_struct *workqueue;
	struct mutex lock;
	int	first_time;
};

void anx78xx_poweron(struct anx78xx *data);
void anx78xx_poweroff(struct anx78xx *data);

#endif
