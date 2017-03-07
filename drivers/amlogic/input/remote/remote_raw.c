/*
 * drivers/amlogic/input/remote/remote_raw.c
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

#include <linux/export.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/kmod.h>
#include <linux/sched.h>
#include <linux/freezer.h>
#include <linux/jiffies.h>
#include "remote_meson.h"

static DEFINE_MUTEX(remote_raw_handler_lock);
static LIST_HEAD(remote_raw_handler_list);
static LIST_HEAD(remote_raw_client_list);

#define MAX_REMOTE_EVENT_SIZE      512


static int ir_raw_event_thread(void *data)
{
	DEFINE_REMOTE_RAW_EVENT(ev);
	struct remote_raw_handler *handler;
	struct remote_raw_handle *raw = (struct remote_raw_handle *)data;
	int retval;

	while (!kthread_should_stop()) {
		/*spin_lock_irq(&raw->lock); */
		retval = kfifo_len(&raw->kfifo);

		if (retval < sizeof(ev)) {
			set_current_state(TASK_INTERRUPTIBLE);

			if (kthread_should_stop())
				set_current_state(TASK_RUNNING);

			/* spin_unlock_irq(&raw->lock); */
			schedule();
			continue;
		}

		retval = kfifo_out(&raw->kfifo, &ev, sizeof(ev));
		/*spin_unlock_irq(&raw->lock); */

		mutex_lock(&remote_raw_handler_lock);
		list_for_each_entry(handler, &remote_raw_handler_list, list)
			handler->decode(raw->dev, ev, handler->data);
		mutex_unlock(&remote_raw_handler_lock);
	}

	return 0;
}

int remote_raw_event_store(struct remote_dev *dev,
	struct remote_raw_event *ev)
{
	if (!dev->raw)
		return -EINVAL;

	if (kfifo_in(&dev->raw->kfifo, ev, sizeof(*ev)) != sizeof(*ev))
		return -ENOMEM;

	return 0;
}

int remote_raw_event_store_edge(struct remote_dev *dev,
	enum raw_event_type type, u32 duration)
{
	DEFINE_REMOTE_RAW_EVENT(ev);
	int rc = 0;
	unsigned long timeout;

	if (!dev->raw)
		return -EINVAL;

	timeout = dev->raw->jiffies_old +
		msecs_to_jiffies(dev->raw->max_frame_time);

	if (time_after(jiffies, timeout) || !dev->raw->last_type)
		type |= RAW_START_EVENT;
	else
		ev.duration = duration;

	if (type & RAW_START_EVENT)
		ev.reset = true;
	else if (dev->raw->last_type & RAW_SPACE)
		ev.pulse = false;
	else if (dev->raw->last_type & RAW_PULSE)
		ev.pulse = true;
	else
		return 0;

	rc = remote_raw_event_store(dev, &ev);

	dev->raw->last_type = type;
	dev->raw->jiffies_old = jiffies;
	return rc;
}


void remote_raw_event_handle(struct remote_dev *dev)
{
/*	unsigned long flags;*/

	if (!dev || !dev->raw)
		return;

/*	spin_lock_irqsave(&dev->raw->lock, flags);*/
	wake_up_process(dev->raw->thread);
/*	spin_unlock_irqrestore(&dev->raw->lock, flags);*/
}

int remote_raw_event_register(struct remote_dev *dev)
{
	int ret;

	dev_info(dev->dev, "remote_raw_event_register\n");
	dev->raw = kzalloc(sizeof(*dev->raw), GFP_KERNEL);
	if (!dev->raw)
		return -ENOMEM;

	dev->raw->dev = dev;
	dev->raw->max_frame_time = dev->max_frame_time;

	dev->raw->jiffies_old = jiffies;

	ret = kfifo_alloc(&dev->raw->kfifo,
		 sizeof(struct remote_raw_event)*MAX_REMOTE_EVENT_SIZE,
		 GFP_KERNEL);
	if (ret < 0)
		goto out;

	dev->raw->thread = kthread_run(ir_raw_event_thread, dev->raw,
		 "remote-thread");

	if (IS_ERR(dev->raw->thread)) {
		ret = PTR_ERR(dev->raw->thread);
		goto err_alloc_thread;
	}
	mutex_lock(&remote_raw_handler_lock);
	list_add_tail(&dev->raw->list, &remote_raw_client_list);
	mutex_unlock(&remote_raw_handler_lock);
	return 0;
err_alloc_thread:
	kfifo_free(&dev->raw->kfifo);
out:
	kfree(dev->raw);
	return ret;
}

void remote_raw_event_unregister(struct remote_dev *dev)
{
	if (!dev || !dev->raw)
		return;

	kthread_stop(dev->raw->thread);
	mutex_lock(&remote_raw_handler_lock);
	list_del(&dev->raw->list);
	mutex_unlock(&remote_raw_handler_lock);

}

int remote_raw_handler_register(struct remote_raw_handler *handler)
{
	mutex_lock(&remote_raw_handler_lock);
	list_add_tail(&(handler->list), &remote_raw_handler_list);
	mutex_unlock(&remote_raw_handler_lock);
	return 0;
}

void remote_raw_handler_unregister(struct remote_raw_handler *handler)
{
	mutex_lock(&remote_raw_handler_lock);
	list_del(&(handler->list));
	mutex_unlock(&remote_raw_handler_lock);
}


void remote_raw_init(void)
{
	static bool raw_init;

	if (!raw_init) {
		raw_init = true;
		pr_info("%s: loading raw decoder\n", DRIVER_NAME);

		/* Load the decoder modules */
		request_module_nowait("remote_decoder_xmp");
	}
}



