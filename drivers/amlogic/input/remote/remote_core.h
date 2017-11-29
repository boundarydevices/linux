/*
 * drivers/amlogic/input/remote/remote_core.h
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

#ifndef _REMOTE_MAIN_MY_H
#define _REMOTE_MAIN_MY_H

#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/kfifo.h>
#include <linux/device.h>
#include <dt-bindings/input/meson_rc.h>

#define MULTI_IR_TYPE_MASK(type) (type & 0xff)  /*8bit*/
#define LEGACY_IR_TYPE_MASK(type) ((type >> 8) & 0xff) /*8bit*/
/*bit[7] identify whether software decode or not*/
#define MULTI_IR_SOFTWARE_DECODE(type) ((MULTI_IR_TYPE_MASK(type) >> 7) == 0x1)
#define ENABLE_LEGACY_IR(type) (LEGACY_IR_TYPE_MASK(type) == 0xff)

#define remote_dbg(dev, format, arg...)     \
do {                                        \
	if (remote_debug_get_enable()) {        \
		if (likely(dev))                    \
			dev_info(dev, format, ##arg);   \
		else                                \
			pr_info(format, ##arg);         \
	}                                       \
} while (0)

enum remote_status {
	REMOTE_NORMAL = 0x00,
	REMOTE_REPEAT = 1<<0,
	REMOTE_CUSTOM_ERROR = 1<<1,
	REMOTE_DATA_ERROR = 1<<2,
	REMOTE_FRAME_ERROR = 1<<3,
	REMOTE_CHECKSUM_ERROR = 1<<4,
	REMOTE_CUSTOM_DATA    = 1<<5
};

enum raw_event_type {
	RAW_SPACE        = (1 << 0),
	RAW_PULSE        = (1 << 1),
	RAW_START_EVENT  = (1 << 2),
	RAW_STOP_EVENT   = (1 << 3),
};



struct remote_raw_handle;
struct remote_dev {
	struct device *dev;
	struct input_dev *input_device;
	struct list_head reg_list;
	struct list_head aml_list;
	struct remote_raw_handle *raw;
	spinlock_t keylock;

	struct timer_list timer_keyup;
	unsigned long keyup_jiffies;
	unsigned long keyup_delay;
	bool keypressed;

	u32 last_scancode;
	u32 last_keycode;
	int rc_type;
	u32 cur_hardcode;
	u32 cur_customcode;
	u32 repeat_time;
	u32 max_frame_time;
	int wait_next_repeat;
	void *platform_data;

	/*debug*/
	char *debug_buffer;
	int debug_buffer_size;
	int debug_current;

	u32 (*getkeycode)(struct remote_dev *, u32);
	int (*ir_report_rel)(struct remote_dev *, u32, int);
	bool (*set_custom_code)(struct remote_dev *, u32);
	bool (*is_valid_custom)(struct remote_dev *);
	bool (*is_next_repeat)(struct remote_dev *);
};

struct remote_raw_handle {
	struct list_head list;
	struct remote_dev *dev;
	struct task_struct *thread;
	struct kfifo_rec_ptr_1 kfifo;/* fifo for the pulse/space durations */
	spinlock_t lock;

	enum raw_event_type last_type;
	unsigned long jiffies_old;
	unsigned long repeat_time;
	unsigned long max_frame_time;
};

struct remote_map_table {
	u32 scancode;
	u32 keycode;
};

struct remote_map {
	struct remote_map_table *scan;
	int rc_type;
	const char *name;
	u32 size;
};

struct remote_map_list {
	struct list_head     list;
	struct remote_map    map;
};

enum {
	SCAN_CODE_REPEAT,
	SCAN_CODE_NORMAL
};

struct remote_raw_event {
	u32             duration;
	unsigned        pulse:1;
	unsigned        reset:1;
	unsigned        timeout:1;
};

#define DEFINE_REMOTE_RAW_EVENT(event) \
	struct remote_raw_event event = { \
		.duration = 0, \
		.pulse = 0, \
		.reset = 0, \
		.timeout = 0}



struct remote_raw_handler {
	struct list_head list;

	int protocols;
	void *data;
	int (*decode)(struct remote_dev *dev, struct remote_raw_event event,
		void *data_dec);
};


/* macros for IR decoders */
static inline bool geq_margin(unsigned int d1, unsigned int d2,
						unsigned int margin)
{
	return d1 > (d2 - margin);
}

static inline bool eq_margin(unsigned int d1, unsigned int d2,
						unsigned int margin)
{
	return (d1 > (d2 - margin)) && (d1 < (d2 + margin));
}

static inline bool is_transition(struct remote_raw_event *x,
	struct remote_raw_event *y)
{
	return x->pulse != y->pulse;
}

static inline void decrease_duration(struct remote_raw_event *ev,
		unsigned int duration)
{
	if (duration > ev->duration)
		ev->duration = 0;
	else
		ev->duration -= duration;
}

int remote_register_device(struct remote_dev *dev);
void remote_free_device(struct remote_dev *dev);
void remote_unregister_device(struct remote_dev *dev);
struct remote_dev *remote_allocate_device(void);
void remote_keydown(struct remote_dev *dev, int scancode, int status);

int remote_raw_event_store(struct remote_dev *dev, struct remote_raw_event *ev);
int remote_raw_event_register(struct remote_dev *dev);
void remote_raw_event_unregister(struct remote_dev *dev);
int remote_raw_handler_register(struct remote_raw_handler *handler);
void remote_raw_handler_unregister(struct remote_raw_handler *handler);
void remote_raw_event_handle(struct remote_dev *dev);
int remote_raw_event_store_edge(struct remote_dev *dev,
	enum raw_event_type type, u32 duration);
void remote_raw_init(void);

/*debug printk */
void remote_debug_set_enable(bool enable);
bool remote_debug_get_enable(void);
int debug_log_printk(struct remote_dev *dev, const char *fmt);

#endif
