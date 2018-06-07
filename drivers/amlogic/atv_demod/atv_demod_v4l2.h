/*
 * amlogic atv demod driver
 *
 * Author: nengwen.chen <nengwen.chen@amlogic.com>
 *
 *
 * Copyright (C) 2018 Amlogic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ATV_DEMOD_V4L2_H__
#define __ATV_DEMOD_V4L2_H__

#include <linux/amlogic/aml_atvdemod.h>
#include <media/v4l2-device.h>
#include "drivers/media/dvb-core/dvb_frontend.h"

/** generic attach function. */
#ifdef CONFIG_AMLOGIC_ATV_DEMOD
#define v4l2_attach(FUNCTION, ARGS...) ({ \
	void *__r = NULL; \
	typeof(&FUNCTION) __a = symbol_request(FUNCTION); \
	if (__a) { \
		__r = (void *) __a(ARGS); \
		if (__r == NULL) \
			symbol_put(FUNCTION); \
	} else { \
		pr_err("V4L2_FE: Unable to find " \
				"symbol "#FUNCTION"()\n"); \
	} \
	__r; \
})

#define v4l2_detach(FUNCTION)    symbol_put_addr(FUNCTION)

#else

#define v4l2_attach(FUNCTION, ARGS...) ({ \
	FUNCTION(ARGS); \
})

#define v4l2_detach(FUNCTION)    {}

#endif /* CONFIG_MEDIA_ATTACH */


#define V4L2_FE_NO_EXIT 0
#define V4L2_FE_NORMAL_EXIT 1
#define V4L2_FE_DEVICE_REMOVED 2

#define V4L2FE_STATE_IDLE 1
#define V4L2FE_STATE_RETUNE 2
#define V4L2FE_STATE_TUNING_FAST 4
#define V4L2FE_STATE_TUNING_SLOW 8
#define V4L2FE_STATE_TUNED 16
#define V4L2FE_STATE_ZIGZAG_FAST 32
#define V4L2FE_STATE_ZIGZAG_SLOW 64
#define V4L2FE_STATE_DISEQC 128
#define V4L2FE_STATE_ERROR 256
#define V4L2FE_STATE_WAITFORLOCK (V4L2FE_STATE_TUNING_FAST |\
		V4L2FE_STATE_TUNING_SLOW | V4L2FE_STATE_ZIGZAG_FAST |\
		V4L2FE_STATE_ZIGZAG_SLOW | V4L2FE_STATE_DISEQC)
#define V4L2FE_STATE_SEARCHING_FAST (V4L2FE_STATE_TUNING_FAST |\
		V4L2FE_STATE_ZIGZAG_FAST)
#define V4L2FE_STATE_SEARCHING_SLOW (V4L2FE_STATE_TUNING_SLOW |\
		V4L2FE_STATE_ZIGZAG_SLOW)
#define V4L2FE_STATE_LOSTLOCK (V4L2FE_STATE_ZIGZAG_FAST |\
		V4L2FE_STATE_ZIGZAG_SLOW)

#define V4L2_IOCTL_MAX_MSGS 64

#define V4L2_SET_FRONTEND    _IOW('V', 105, struct v4l2_analog_parameters)
#define V4L2_GET_FRONTEND    _IOR('V', 106, struct v4l2_analog_parameters)
#define V4L2_GET_EVENT       _IOR('V', 107, struct v4l2_frontend_event)
#define V4L2_SET_MODE        _IOW('V', 108, int)
#define V4L2_READ_STATUS     _IOR('V', 109, enum v4l2_status)
#define V4L2_SET_PROPERTY    _IOWR('V', 110, struct v4l2_properties)
#define V4L2_GET_PROPERTY    _IOWR('V', 111, struct v4l2_properties)

#define ANALOG_FLAG_ENABLE_AFC          0x00000001
#define ANALOG_FLAG_MANUL_SCAN          0x00000011

#define V4L2_UNDEFINED           0
#define V4L2_TUNE                1
#define V4L2_SOUND_SYS           2
#define V4L2_SLOW_SEARCH_MODE    3


struct v4l2_frontend;

struct v4l2_analog_parameters {
	unsigned int frequency;
	unsigned int audmode;
	unsigned int soundsys; /*A2,BTSC,EIAJ,NICAM */
	v4l2_std_id std;
	unsigned int flag;
	unsigned int afc_range;
	unsigned int reserved;
};

enum v4l2_status {
	V4L2_HAS_SIGNAL  = 0x01, /* found something above the noise level */
	V4L2_HAS_CARRIER = 0x02, /* found a DVB signal */
	V4L2_HAS_VITERBI = 0x04, /* FEC is stable  */
	V4L2_HAS_SYNC    = 0x08, /* found sync bytes  */
	V4L2_HAS_LOCK    = 0x10, /* everything's working... */
	V4L2_TIMEDOUT    = 0x20, /* no lock within the last ~2 seconds */
	V4L2_REINIT      = 0x40, /* frontend was reinitialized, */
};				/* application is recommended to reset */
				/* DiSEqC, tone and parameters */

struct v4l2_property {
	unsigned int cmd;
	unsigned int data;
	int result;
};

struct v4l2_properties {
	unsigned int num;
	union {
		struct v4l2_property *props;
		__u64 reserved;
	};
};

enum v4l2_search {
	V4L2_SEARCH_SUCCESS = (1 << 0),
	V4L2_SEARCH_ASLEEP  = (1 << 1),
	V4L2_SEARCH_FAILED  = (1 << 2),
	V4L2_SEARCH_INVALID = (1 << 3),
	V4L2_SEARCH_AGAIN   = (1 << 4),
	V4L2_SEARCH_ERROR   = (1 << 31),
};

struct v4l2_frontend_event {
	enum v4l2_status status;
	struct v4l2_analog_parameters parameters;
};

#define MAX_EVENT 8

struct v4l2_fe_events {
	struct v4l2_frontend_event events[MAX_EVENT];
	int eventw;
	int eventr;
	int overflow;
	wait_queue_head_t wait_queue;
	struct mutex mtx;
};

struct v4l2_frontend_private {
	struct v4l2_atvdemod_device *v4l2dev;

	struct v4l2_fe_events events;
	struct semaphore sem;
	struct list_head list_head;
	wait_queue_head_t wait_queue;
	struct task_struct *thread;
	unsigned long release_jiffies;
	unsigned int exit;
	unsigned int wakeup;
	enum v4l2_status status;
	unsigned int delay;

	unsigned int state;
	enum v4l2_search algo_status;
};

struct v4l2_adapter {
	struct device *dev;

	struct dvb_frontend fe;

	struct i2c_client i2c;
	unsigned int tuner_id;
};

struct v4l2_frontend_ops {
	int (*set_property)(struct v4l2_frontend *fe,
			struct v4l2_property *tvp);
	int (*get_property)(struct v4l2_frontend *fe,
			struct v4l2_property *tvp);

	enum v4l2_search (*search)(struct v4l2_frontend *v4l2_fe);
};

struct v4l2_frontend {
	struct device *dev;

	struct dvb_frontend fe;
	unsigned int tuner_id;
	struct i2c_client i2c;

	void *frontend_priv;
	void *tuner_priv;
	void *analog_demod_priv;

	struct v4l2_analog_parameters params;

	struct v4l2_frontend_ops ops;
};

struct v4l2_atvdemod_device {
	char *name;
	struct class *clsp;
	struct device *dev;

	struct v4l2_device v4l2_dev;
	struct video_device *video_dev;

	struct mutex lock;

	struct i2c_client i2c;
	unsigned int tuner_id;
};

int v4l2_resister_frontend(struct v4l2_frontend *v4l2_fe);
int v4l2_unresister_frontend(struct v4l2_frontend *v4l2_fe);

void v4l2_frontend_detach(struct v4l2_frontend *v4l2_fe);
int v4l2_frontend_suspend(struct v4l2_frontend *v4l2_fe);
int v4l2_frontend_resume(struct v4l2_frontend *v4l2_fe);

#endif // __ATV_DEMOD_V4L2_H__
