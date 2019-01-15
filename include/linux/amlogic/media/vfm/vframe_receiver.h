/*
 * include/linux/amlogic/media/vfm/vframe_receiver.h
 *
 * Copyright (C) 2016 Amlogic, Inc. All rights reserved.
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

#ifndef VFRAME_RECEIVER_H
#define VFRAME_RECEIVER_H

/* Standard Linux headers */
#include <linux/list.h>

/* Amlogic headers */
#include <linux/amlogic/media/vfm/vframe.h>

#define VFRAME_EVENT_PROVIDER_UNREG                            1
#define VFRAME_EVENT_PROVIDER_LIGHT_UNREG                      2
#define VFRAME_EVENT_PROVIDER_START                            3
#define VFRAME_EVENT_PROVIDER_VFRAME_READY                     4
#define VFRAME_EVENT_PROVIDER_QUREY_STATE                      5
#define VFRAME_EVENT_PROVIDER_RESET                            6
#define VFRAME_EVENT_PROVIDER_FORCE_BLACKOUT		       7
#define VFRAME_EVENT_PROVIDER_REG                              8
#define VFRAME_EVENT_PROVIDER_LIGHT_UNREG_RETURN_VFRAME        9
#define VFRAME_EVENT_PROVIDER_DPBUF_CONFIG                     10
#define VFRAME_EVENT_PROVIDER_QUREY_VDIN2NR                    11
#define VFRAME_EVENT_PROVIDER_SET_3D_VFRAME_INTERLEAVE         12
#define VFRAME_EVENT_PROVIDER_FR_HINT                          13
#define VFRAME_EVENT_PROVIDER_FR_END_HINT                      14
#define VFRAME_EVENT_PROVIDER_QUREY_DISPLAY_INFO    15
#define VFRAME_EVENT_PROVIDER_PROPERTY_CHANGED 16

enum receviver_start_e {
	RECEIVER_STATE_NULL = -1,
	RECEIVER_STATE_NONE = 0,
	RECEIVER_INACTIVE,
	RECEIVER_ACTIVE
};

struct vframe_receiver_op_s {
	int (*event_cb)(int type, void *data, void *private_data);
} /*vframe_receiver_op_t */;

struct vframe_receiver_s {
	const char *name;
	const struct vframe_receiver_op_s *ops;
	void *op_arg;
	struct list_head list;
} /*vframe_receiver_t */;

extern struct vframe_receiver_s *vf_receiver_alloc(void);
extern void vf_receiver_init(struct vframe_receiver_s *recv,
			     const char *name,
			     const struct vframe_receiver_op_s *ops,
			     void *op_arg);
extern void vf_receiver_free(struct vframe_receiver_s *recv);

extern int vf_reg_receiver(struct vframe_receiver_s *recv);
extern void vf_unreg_receiver(struct vframe_receiver_s *recv);

struct vframe_receiver_s *vf_get_receiver(const char *provider_name);

int vf_notify_receiver(const char *provider_name, int event_type, void *data);

int vf_notify_receiver_by_name(const char *receiver_name, int event_type,
	void *data);

#endif /* VFRAME_RECEIVER_H */
