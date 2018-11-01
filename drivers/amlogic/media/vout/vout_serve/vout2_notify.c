/*
 * drivers/amlogic/media/vout/vout_serve/vout2_notify.c
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
#include <linux/err.h>

/* Amlogic Headers */
#include <linux/amlogic/media/vout/vout_notify.h>

/* Local Headers */
#include "vout_func.h"

static BLOCKING_NOTIFIER_HEAD(vout2_notifier_list);

/**
 *	vout_register_client - register a client notifier
 *	@nb: notifier block to callback on events
 */
int vout2_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&vout2_notifier_list, nb);
}
EXPORT_SYMBOL(vout2_register_client);

/**
 *	vout_unregister_client - unregister a client notifier
 *	@nb: notifier block to callback on events
 */
int vout2_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&vout2_notifier_list, nb);
}
EXPORT_SYMBOL(vout2_unregister_client);

/**
 * vout_notifier_call_chain - notify clients of fb_events
 *
 */
int vout2_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&vout2_notifier_list, val, v);
}
EXPORT_SYMBOL_GPL(vout2_notifier_call_chain);

/*
 *interface export to client who want to get current vinfo.
 */
struct vinfo_s *get_current_vinfo2(void)
{
	struct vinfo_s *vinfo = NULL;
	struct vout_module_s *p_module = NULL;

	p_module = vout_func_get_vout2_module();
	if (!IS_ERR_OR_NULL(p_module->curr_vout_server)) {
		if (p_module->curr_vout_server->op.get_vinfo)
			vinfo = p_module->curr_vout_server->op.get_vinfo();
	}
	if (vinfo == NULL) /* avoid crash mistake */
		vinfo = get_invalid_vinfo(2);

	return vinfo;
}
EXPORT_SYMBOL(get_current_vinfo2);

/*
 *interface export to client who want to get current vmode.
 */
enum vmode_e get_current_vmode2(void)
{
	const struct vinfo_s *vinfo;
	struct vout_module_s *p_module = NULL;
	enum vmode_e mode = VMODE_MAX;

	p_module = vout_func_get_vout2_module();
	if (!IS_ERR_OR_NULL(p_module->curr_vout_server)) {
		if (p_module->curr_vout_server->op.get_vinfo) {
			vinfo = p_module->curr_vout_server->op.get_vinfo();
			if (vinfo)
				mode = vinfo->mode;
		}
	}

	return mode;
}
EXPORT_SYMBOL(get_current_vmode2);

const char *get_name_by_vmode2(enum vmode_e mode)
{
	const char *str = NULL;
	const struct vinfo_s *vinfo = NULL;
	struct vout_module_s *p_module = NULL;

	p_module = vout_func_get_vout2_module();
	if (!IS_ERR_OR_NULL(p_module->curr_vout_server)) {
		if (p_module->curr_vout_server->op.get_vinfo)
			vinfo = p_module->curr_vout_server->op.get_vinfo();
	}
	if (vinfo == NULL)
		vinfo = get_invalid_vinfo(2);
	str = vinfo->name;

	return str;
}
EXPORT_SYMBOL(get_name_by_vmode2);

/*
 * interface export to client who want to set current vmode.
 */
int set_current_vmode2(enum vmode_e mode)
{
	return vout_func_set_current_vmode(2, mode);
}
EXPORT_SYMBOL(set_current_vmode2);

/*
 *interface export to client who want to set current vmode.
 */
enum vmode_e validate_vmode2(char *name)
{
	return vout_func_validate_vmode(2, name);
}
EXPORT_SYMBOL(validate_vmode2);

/*
 *interface export to client who want to notify about source frame rate.
 */
int set_vframe2_rate_hint(int duration)
{
	return vout_func_set_vframe_rate_hint(2, duration);
}
EXPORT_SYMBOL(set_vframe2_rate_hint);

/*
 *interface export to client who want to notify about source frame rate end.
 */
int set_vframe2_rate_end_hint(void)
{
	return vout_func_set_vframe_rate_end_hint(2);
}
EXPORT_SYMBOL(set_vframe2_rate_end_hint);

/*
 *interface export to client who want to notify about source fr_policy.
 */
int set_vframe2_rate_policy(int policy)
{
	return vout_func_set_vframe_rate_policy(2, policy);
}
EXPORT_SYMBOL(set_vframe2_rate_policy);

/*
 *interface export to client who want to notify about source fr_policy.
 */
int get_vframe2_rate_policy(void)
{
	return vout_func_get_vframe_rate_policy(2);
}
EXPORT_SYMBOL(get_vframe2_rate_policy);

/*
 * interface export to client who want to set test bist.
 */
void set_vout2_bist(unsigned int bist)
{
	vout_func_set_test_bist(2, bist);
}
EXPORT_SYMBOL(set_vout2_bist);

int vout2_suspend(void)
{
	return vout_func_vout_suspend(2);
}
EXPORT_SYMBOL(vout2_suspend);

int vout2_resume(void)
{
	return vout_func_vout_resume(2);
}
EXPORT_SYMBOL(vout2_resume);

/*
 *interface export to client who want to shutdown.
 */
int vout2_shutdown(void)
{
	return vout_func_vout_shutdown(2);
}
EXPORT_SYMBOL(vout2_shutdown);

/*
 *here we offer two functions to get and register vout module server
 *vout module server will set and store tvmode attributes for vout encoder
 *we can ensure TVMOD SET MODULE independent with these two function.
 */

int vout2_register_server(struct vout_server_s *mem_server)
{
	return vout_func_vout_register_server(2, mem_server);
}
EXPORT_SYMBOL(vout2_register_server);

int vout2_unregister_server(struct vout_server_s *mem_server)
{
	return vout_func_vout_unregister_server(2, mem_server);
}
EXPORT_SYMBOL(vout2_unregister_server);
