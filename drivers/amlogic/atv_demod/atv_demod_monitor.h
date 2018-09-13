/*
 * drivers/amlogic/atv_demod/atv_demod_monitor.h
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

#ifndef __ATV_DEMOD_MONITOR_H__
#define __ATV_DEMOD_MONITOR_H__

#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/mutex.h>


struct atv_demod_monitor {
	struct work_struct work;
	struct timer_list timer;

	struct dvb_frontend *fe;

	struct mutex mtx;

	bool state;
	bool lock;

	void (*disable)(struct atv_demod_monitor *monitor);
	void (*enable)(struct atv_demod_monitor *monitor);
};

extern void atv_demod_monitor_init(struct atv_demod_monitor *monitor);


#endif /* __ATV_DEMOD_MONITOR_H__ */
