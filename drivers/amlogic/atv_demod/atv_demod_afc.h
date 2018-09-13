/*
 * drivers/amlogic/atv_demod/atv_demod_afc.h
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

#ifndef __ATV_DEMOD_AFC_H__
#define __ATV_DEMOD_AFC_H__

#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/mutex.h>


#define AFC_LOCK_STATUS_NULL 0
#define AFC_LOCK_STATUS_PRE_UNLOCK 1
#define AFC_LOCK_STATUS_PRE_LOCK 2
#define AFC_LOCK_STATUS_PRE_OVER_RANGE 3
#define AFC_LOCK_STATUS_POST_PROCESS 4
#define AFC_LOCK_STATUS_POST_LOCK 5
#define AFC_LOCK_STATUS_POST_UNLOCK 6
#define AFC_LOCK_STATUS_POST_OVER_RANGE 7
#define AFC_LOCK_STATUS_NUM 8
#define AFC_LOCK_PRE_STEP_NUM 9

#define AFC_BEST_LOCK 50

struct atv_demod_afc {
	struct work_struct work;
	struct timer_list timer;

	struct dvb_frontend *fe;

	struct mutex mtx;

	bool state;

	int timer_delay_cnt;

	unsigned int no_sig_cnt;
	unsigned int pre_step;
	unsigned int pre_lock_cnt;
	unsigned int pre_unlock_cnt;
	int offset;
	unsigned int status;
	unsigned int wave_cnt;

	bool lock;

	void (*disable)(struct atv_demod_afc *afc);
	void (*enable)(struct atv_demod_afc *afc);
};

extern void atv_demod_afc_init(struct atv_demod_afc *afc);


#endif /* __ATV_DEMOD_AFC_H__ */
