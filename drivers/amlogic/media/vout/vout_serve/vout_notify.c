/*
 * drivers/amlogic/media/vout/vout_serve/vout_notify.c
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

/* Linux Headers */
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/sched.h>

/* Amlogic Headers */
#include <linux/amlogic/media/vout/vout_notify.h>

/* Local Headers */
#include "vout_serve.h"

static BLOCKING_NOTIFIER_HEAD(vout_notifier_list);
static DEFINE_MUTEX(vout_mutex);

static struct vout_module_s vout_module = {
	.vout_server_list = {
		&vout_module.vout_server_list,
		&vout_module.vout_server_list
	},
	.curr_vout_server = NULL,
};

/**
 *	vout_register_client - register a client notifier
 *	@nb: notifier block to callback on events
 */
int vout_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&vout_notifier_list, nb);
}
EXPORT_SYMBOL(vout_register_client);

/**
 *	vout_unregister_client - unregister a client notifier
 *	@nb: notifier block to callback on events
 */
int vout_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&vout_notifier_list, nb);
}
EXPORT_SYMBOL(vout_unregister_client);

/**
 * vout_notifier_call_chain - notify clients of fb_events
 *
 */
int vout_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&vout_notifier_list, val, v);
}
EXPORT_SYMBOL_GPL(vout_notifier_call_chain);

static struct vinfo_s vinfo_invalid = {
	.name              = "invalid",
	.mode              = VMODE_INIT_NULL,
	.width             = 1920,
	.height            = 1080,
	.field_height      = 1080,
	.aspect_ratio_num  = 16,
	.aspect_ratio_den  = 9,
	.sync_duration_num = 60,
	.sync_duration_den = 1,
	.video_clk = 148500000,
	.viu_color_fmt = COLOR_FMT_RGB444,
	.vout_device = NULL,
};

static struct vinfo_s *get_invalid_vinfo(void)
{
	VOUTERR("invalid vinfo. current vmode is not supported\n");
	return &vinfo_invalid;
}

/*
 *interface export to client who want to get current vinfo.
 */
struct vinfo_s *get_current_vinfo(void)
{
	struct vinfo_s *info = NULL;

	if (vout_module.curr_vout_server) {
		if (vout_module.curr_vout_server->op.get_vinfo)
			info = vout_module.curr_vout_server->op.get_vinfo();
	}
	if (info == NULL) /* avoid crash mistake */
		info = get_invalid_vinfo();

	return info;
}
EXPORT_SYMBOL(get_current_vinfo);

/*
 *interface export to client who want to get current vmode.
 */
enum vmode_e get_current_vmode(void)
{
	const struct vinfo_s *info;
	enum vmode_e mode = VMODE_MAX;

	if (vout_module.curr_vout_server) {
		if (vout_module.curr_vout_server->op.get_vinfo) {
			info = vout_module.curr_vout_server->op.get_vinfo();
			mode = info->mode;
		}
	}

	return mode;
}
EXPORT_SYMBOL(get_current_vmode);

const char *get_name_by_vmode(enum vmode_e mode)
{
	const char *str = NULL;
	const struct vinfo_s *info = NULL;

	if (vout_module.curr_vout_server) {
		if (vout_module.curr_vout_server->op.get_vinfo) {
			info = vout_module.curr_vout_server->op.get_vinfo();
			str = info->name;
		}
	}
	if (info == NULL)
		info = get_invalid_vinfo();
	str = info->name;

	return str;
}
EXPORT_SYMBOL(get_name_by_vmode);

/* fps = 9600/duration/100 hz */
static int vsource_fps_table[][2] = {
	/* duration    fps */
	{1600,         6000,},
	{1601,         5994,},
	{1602,         5994,},
	{1920,         5000,},
	{3200,         3000,},
	{3203,         2997,},
	{3840,         2500,},
	{4000,         2400,},
	{4004,         2397,},
};

int get_vsource_fps(int duration)
{
	int i;
	int fps = 6000;

	for (i = 0; i < 9; i++) {
		if (duration == vsource_fps_table[i][0]) {
			fps = vsource_fps_table[i][1];
			break;
		}
	}

	return fps;
}

static struct vframe_match_s vframe_match_table_1[] = {
	{5000, 5000, 50,   1},
	{2500, 5000, 50,   1},
	{6000, 6000, 60,   1},
	{3000, 6000, 60,   1},
	{2400, 6000, 60,   1},
	{2397, 5994, 5994, 100},
	{2997, 5994, 5994, 100},
	{5994, 5994, 5994, 100},
	{0,    6000, 60,   1},
};

static struct vframe_match_s vframe_match_table_2[] = {
	{5000, 5000, 50,   1},
	{2500, 5000, 50,   1},
	{6000, 6000, 60,   1},
	{3000, 6000, 60,   1},
	{2400, 4800, 48,   1},
	{2397, 5994, 5994, 100},
	{2997, 5994, 5994, 100},
	{5994, 5994, 5994, 100},
	{0,    6000, 60,   1},
};

struct vframe_match_s *get_vframe_match(int duration, int fr_policy)
{
	int i, n, fps;
	struct vframe_match_s *vtable;

	fps = get_vsource_fps(duration);

	switch (fr_policy) {
	case 1:
		vtable = vframe_match_table_1;
		n = ARRAY_SIZE(vframe_match_table_1);
		break;
	case 2:
		vtable = vframe_match_table_2;
		n = ARRAY_SIZE(vframe_match_table_2);
		break;
	default:
		VOUTPR("%s: fr_auto_policy = %d, disabled\n",
			__func__, fr_policy);
		return 0;
	}
	for (i = 0; i < n; i++) {
		if (fps == vtable[i].fps)
			break;
	}
	VOUTPR("%s: policy = %d, duration = %d, fps = %d, frame_rate = %d\n",
		__func__, fr_policy, duration, fps, vtable[i].frame_rate);

	return &vtable[i];
}
EXPORT_SYMBOL(get_vframe_match);

/*
 *interface export to client who want to notify about source frame rate.
 */
int set_vframe_rate_hint(int duration)
{
	int r = -1;
	struct vout_server_s  *p_server;

	list_for_each_entry(p_server, &vout_module.vout_server_list, list) {
		if ((p_server->op.set_vframe_rate_hint != NULL) &&
			(p_server->op.set_vframe_rate_hint(duration) == 0)) {
			return 0;
		}
	}

	return r;
}
EXPORT_SYMBOL(set_vframe_rate_hint);

/*
 *interface export to client who want to notify about source frame rate end.
 */
int set_vframe_rate_end_hint(void)
{
	int ret = -1;
	struct vout_server_s  *p_server;

	list_for_each_entry(p_server, &vout_module.vout_server_list, list) {
		if ((p_server->op.set_vframe_rate_end_hint != NULL) &&
			(p_server->op.set_vframe_rate_end_hint() == 0)) {
			return 0;
		}
	}

	return ret;
}
EXPORT_SYMBOL(set_vframe_rate_end_hint);

/*
 *interface export to client who want to notify about source fr_policy.
 */
int set_vframe_rate_policy(int policy)
{
	int ret = -1;
	struct vout_server_s  *p_server;

	list_for_each_entry(p_server, &vout_module.vout_server_list, list) {
		if ((p_server->op.set_vframe_rate_policy != NULL) &&
			(p_server->op.set_vframe_rate_policy(policy) == 0)) {
			return 0;
		}
	}

	return ret;
}
EXPORT_SYMBOL(set_vframe_rate_policy);

/*
 *interface export to client who want to notify about source fr_policy.
 */
int get_vframe_rate_policy(void)
{
	int ret = -1;
	struct vout_server_s  *p_server;

	list_for_each_entry(p_server, &vout_module.vout_server_list, list) {
		if (p_server->op.get_vframe_rate_policy != NULL) {
			ret = p_server->op.get_vframe_rate_policy();
			return ret;
		}
	}

	return ret;
}
EXPORT_SYMBOL(get_vframe_rate_policy);

#ifdef CONFIG_SCREEN_ON_EARLY
static int wake_up_flag;
void wakeup_early_suspend_proc(void)
{
	wake_up_flag = 1;
}
#endif

int vout_suspend(void)
{
	int ret = 0;
	struct vout_server_s  *p_server = vout_module.curr_vout_server;

#ifdef CONFIG_SCREEN_ON_EARLY
	wake_up_flag = 0;
	int i = 0;

	for (; i < 20; i++) {
		if (wake_up_flag)
			break;
		msleep(100);
	}

	wake_up_flag = 0;
#endif

	mutex_lock(&vout_mutex);

	if (p_server) {
		if (p_server->op.vout_suspend)
			ret = p_server->op.vout_suspend();
	}

	mutex_unlock(&vout_mutex);
	return ret;
}
EXPORT_SYMBOL(vout_suspend);

int vout_resume(void)
{
	struct vout_server_s  *p_server = vout_module.curr_vout_server;

	mutex_lock(&vout_mutex);

	if (p_server) {
		if (p_server->op.vout_resume) {
			/* ignore error when resume. */
			p_server->op.vout_resume();
		}
	}

	mutex_unlock(&vout_mutex);
	return 0;
}
EXPORT_SYMBOL(vout_resume);

/*
 * interface export to client who want to set current vmode.
 */
int set_current_vmode(enum vmode_e mode)
{
	int ret = -1;
	struct vout_server_s  *p_server;
	char *str;

	mutex_lock(&vout_mutex);
	list_for_each_entry(p_server, &vout_module.vout_server_list, list) {
		if (p_server->op.vmode_is_supported == NULL) {
			p_server->op.disable(mode);
			continue;
		}

		if (p_server->op.vmode_is_supported(mode) == true) {
			vout_module.curr_vout_server = p_server;
			ret = p_server->op.set_vmode(mode);
			str = p_server->op.get_vinfo()->name;
			if (vout_module.curr_vout_server)
				update_vout_mode(str);
			/* break;  do not exit , should disable other modules */
		} else
			p_server->op.disable(mode);
	}

	mutex_unlock(&vout_mutex);

	return ret;
}
EXPORT_SYMBOL(set_current_vmode);

/*
 *interface export to client who want to set current vmode.
 */
enum vmode_e validate_vmode(char *name)
{
	enum vmode_e ret = VMODE_MAX;
	struct vout_server_s  *p_server;

	mutex_lock(&vout_mutex);
	list_for_each_entry(p_server, &vout_module.vout_server_list, list) {
		if (p_server->op.validate_vmode)
			ret = p_server->op.validate_vmode(name);

		if (ret != VMODE_MAX) /* valid vmode find. */
			break;
	}
	mutex_unlock(&vout_mutex);

	return ret;
}
EXPORT_SYMBOL(validate_vmode);

/*
 *here we offer two functions to get and register vout module server
 *vout module server will set and store tvmode attributes for vout encoder
 *we can ensure TVMOD SET MODULE independent with these two function.
 */


int vout_register_server(struct vout_server_s *mem_server)
{
	struct list_head *p_iter;
	struct vout_server_s  *p_server;

	if (mem_server == NULL) {
		VOUTERR("%s: server is NULL\n", __func__);
		return -1;
	}
	VOUTPR("%s\n", __func__);

	mutex_lock(&vout_mutex);
	list_for_each(p_iter, &vout_module.vout_server_list) {
		p_server = list_entry(p_iter, struct vout_server_s, list);

		if (p_server->name && mem_server->name &&
			strcmp(p_server->name, mem_server->name) == 0) {
			/* vout server already registered. */
			mutex_unlock(&vout_mutex);
			return -1;
		}
	}
	list_add(&mem_server->list, &vout_module.vout_server_list);
	mutex_unlock(&vout_mutex);
	return 0;
}
EXPORT_SYMBOL(vout_register_server);

int vout_unregister_server(struct vout_server_s *mem_server)
{
	struct vout_server_s  *p_server;

	if (mem_server == NULL) {
		VOUTERR("%s: server is NULL\n", __func__);
		return -1;
	}

	mutex_lock(&vout_mutex);
	list_for_each_entry(p_server, &vout_module.vout_server_list, list) {
		if (p_server->name && mem_server->name &&
			strcmp(p_server->name, mem_server->name) == 0) {
			/*
			 * We will not move current vout server pointer
			 * automatically if current vout server pointer
			 * is the one which will be deleted next.
			 * So you should change current vout server
			 * first then remove it
			 */
			if (vout_module.curr_vout_server == p_server)
				vout_module.curr_vout_server = NULL;

			list_del(&mem_server->list);
			mutex_unlock(&vout_mutex);
			return 0;
		}
	}
	mutex_unlock(&vout_mutex);
	return 0;
}
EXPORT_SYMBOL(vout_unregister_server);
