/*
 * drivers/amlogic/media/vout/vout_serve/vout_func.c
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
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/media/vout/vout_notify.h>

/* Local Headers */
#include "vout_func.h"

static DEFINE_MUTEX(vout_mutex);

static struct vout_module_s vout_module = {
	.vout_server_list = {
		&vout_module.vout_server_list,
		&vout_module.vout_server_list
	},
	.curr_vout_server = NULL,
};

#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
static struct vout_module_s vout2_module = {
	.vout_server_list = {
		&vout2_module.vout_server_list,
		&vout2_module.vout_server_list
	},
	.curr_vout_server = NULL,
};
#endif

static struct vinfo_s invalid_vinfo = {
	.name              = "invalid",
	.mode              = VMODE_INIT_NULL,
	.width             = 1920,
	.height            = 1080,
	.field_height      = 1080,
	.aspect_ratio_num  = 16,
	.aspect_ratio_den  = 9,
	.sync_duration_num = 60,
	.sync_duration_den = 1,
	.video_clk         = 148500000,
	.htotal            = 2200,
	.vtotal            = 1125,
	.viu_color_fmt     = COLOR_FMT_RGB444,
	.viu_mux           = VIU_MUX_MAX,
	.vout_device       = NULL,
};

struct vinfo_s *get_invalid_vinfo(int index)
{
	VOUTERR("invalid vinfo%d. current vmode is not supported\n", index);
	return &invalid_vinfo;
}
EXPORT_SYMBOL(get_invalid_vinfo);

void vout_trim_string(char *str)
{
	char *start, *end;
	int len;

	if (str == NULL)
		return;

	len = strlen(str);
	if ((str[len-1] == '\n') || (str[len-1] == '\r')) {
		len--;
		str[len] = '\0';
	}

	start = str;
	end = str + len - 1;
	while (*start && (*start == ' '))
		start++;
	while (*end && (*end == ' '))
		*end-- = '\0';
	strcpy(str, start);
}
EXPORT_SYMBOL(vout_trim_string);

struct vout_module_s *vout_func_get_vout_module(void)
{
	return &vout_module;
}
EXPORT_SYMBOL(vout_func_get_vout_module);

#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
struct vout_module_s *vout_func_get_vout2_module(void)
{
	return &vout2_module;
}
EXPORT_SYMBOL(vout_func_get_vout2_module);
#endif

static unsigned int vout_func_vcbus_read(unsigned int _reg)
{
	return aml_read_vcbus(_reg);
};

static void vout_func_vcbus_write(unsigned int _reg, unsigned int _value)
{
	aml_write_vcbus(_reg, _value);
};

static void vout_func_vcbus_setb(unsigned int _reg, unsigned int _value,
		unsigned int _start, unsigned int _len)
{
	vout_func_vcbus_write(_reg, ((vout_func_vcbus_read(_reg) &
			~(((1L << (_len))-1) << (_start))) |
			(((_value)&((1L<<(_len))-1)) << (_start))));
}

static inline int vout_func_check_state(int index, unsigned int state,
		struct vout_server_s *p_server)
{
	if (state & ~(1 << index)) {
		/*VOUTERR("vout%d: server %s is actived by another vout\n",
		 *	index, p_server->name);
		 */
		return -1;
	}

	return 0;
}

void vout_func_set_state(int index, enum vmode_e mode)
{
	struct vout_server_s *p_server;
	struct vout_module_s *p_module = NULL;
	int state;

	mutex_lock(&vout_mutex);

	if (index == 1)
		p_module = &vout_module;
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	else if (index == 2)
		p_module = &vout2_module;
#endif

	list_for_each_entry(p_server, &p_module->vout_server_list, list) {
		if (p_server->op.vmode_is_supported == NULL) {
			p_server->op.disable(mode);
			continue;
		}

		if (p_server->op.vmode_is_supported(mode) == true) {
			p_module->curr_vout_server = p_server;
			if (p_server->op.set_state)
				p_server->op.set_state(index);
		} else {
			if (p_server->op.get_state) {
				state = p_server->op.get_state();
				if (state & (1 << index))
					p_server->op.disable(mode);
			}
			if (p_server->op.clr_state)
				p_server->op.clr_state(index);
		}
	}

	mutex_unlock(&vout_mutex);
}
EXPORT_SYMBOL(vout_func_set_state);

void vout_func_update_viu(int index)
{
	struct vinfo_s *vinfo = NULL;
	struct vout_server_s *p_server;
	struct vout_module_s *p_module = NULL;
	unsigned int mux_bit = 0xff, mux_sel = VIU_MUX_MAX;
	unsigned int clk_bit = 0xff, clk_sel = 0;

	mutex_lock(&vout_mutex);

	if (index == 1)
		p_module = &vout_module;
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	else if (index == 2)
		p_module = &vout2_module;
#endif

	p_server = p_module->curr_vout_server;

	if (p_server->op.get_vinfo)
		vinfo = p_server->op.get_vinfo();
	else
		vinfo = get_invalid_vinfo(index);

	switch (index) {
	case 1:
		mux_bit = 0;
		clk_sel = 0;
		break;
	case 2:
		mux_bit = 2;
		clk_sel = 1;
		break;
	default:
		break;
	}

	mux_sel = vinfo->viu_mux;
	switch (mux_sel) {
	case VIU_MUX_ENCL:
		clk_bit = 1;
		break;
	case VIU_MUX_ENCI:
		clk_bit = 2;
		break;
	case VIU_MUX_ENCP:
		clk_bit = 0;
		break;
	default:
		break;
	}

	if (mux_bit < 0xff) {
		vout_func_vcbus_setb(VPU_VIU_VENC_MUX_CTRL,
				mux_sel, mux_bit, 2);
	}
	if (clk_bit < 0xff)
		vout_func_vcbus_setb(VPU_VENCX_CLK_CTRL, clk_sel, clk_bit, 1);

#if 0
	VOUTPR("%s: %d, mux_sel=%d, clk_sel=%d\n",
		__func__, index, mux_sel, clk_sel);
#endif
	mutex_unlock(&vout_mutex);
}
EXPORT_SYMBOL(vout_func_update_viu);

int vout_func_set_vmode(int index, enum vmode_e mode)
{
	int ret = -1;
	struct vout_module_s *p_module = NULL;

	mutex_lock(&vout_mutex);

	if (index == 1)
		p_module = &vout_module;
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	else if (index == 2)
		p_module = &vout2_module;
#endif
	ret = p_module->curr_vout_server->op.set_vmode(mode);

	mutex_unlock(&vout_mutex);

	return ret;
}
EXPORT_SYMBOL(vout_func_set_vmode);

/*
 * interface export to client who want to set current vmode.
 */
int vout_func_set_current_vmode(int index, enum vmode_e mode)
{
	vout_func_set_state(index, mode);
	vout_func_update_viu(index);

	return vout_func_set_vmode(index, mode);
}
EXPORT_SYMBOL(vout_func_set_current_vmode);

/*
 *interface export to client who want to set current vmode.
 */
enum vmode_e vout_func_validate_vmode(int index, char *name)
{
	enum vmode_e ret = VMODE_MAX;
	struct vout_server_s  *p_server;
	struct vout_module_s *p_module = NULL;
	int state;

	mutex_lock(&vout_mutex);

	if (index == 1)
		p_module = &vout_module;
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	else if (index == 2)
		p_module = &vout2_module;
#endif

	list_for_each_entry(p_server, &p_module->vout_server_list, list) {
		/* check state for another vout */
		if (p_server->op.get_state) {
			state = p_server->op.get_state();
			if (vout_func_check_state(index, state, p_server))
				continue;
		}
		if (p_server->op.validate_vmode) {
			ret = p_server->op.validate_vmode(name);
			if (ret != VMODE_MAX) /* valid vmode find. */
				break;
		}
	}
	mutex_unlock(&vout_mutex);

	return ret;
}
EXPORT_SYMBOL(vout_func_validate_vmode);

int vout_func_set_vframe_rate_hint(int index, int duration)
{
	int ret = -1;
	struct vout_server_s *p_server = NULL;

	mutex_lock(&vout_mutex);

	if (index == 1)
		p_server = vout_module.curr_vout_server;
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	else if (index == 2)
		p_server = vout2_module.curr_vout_server;
#endif

	if (!IS_ERR_OR_NULL(p_server)) {
		if (p_server->op.set_vframe_rate_hint)
			ret = p_server->op.set_vframe_rate_hint(duration);
	}

	mutex_unlock(&vout_mutex);
	return ret;
}
EXPORT_SYMBOL(vout_func_set_vframe_rate_hint);

/*
 *interface export to client who want to notify about source frame rate end.
 */
int vout_func_set_vframe_rate_end_hint(int index)
{
	int ret = -1;
	struct vout_server_s *p_server = NULL;

	mutex_lock(&vout_mutex);

	if (index == 1)
		p_server = vout_module.curr_vout_server;
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	else if (index == 2)
		p_server = vout2_module.curr_vout_server;
#endif

	if (!IS_ERR_OR_NULL(p_server)) {
		if (p_server->op.set_vframe_rate_end_hint)
			ret = p_server->op.set_vframe_rate_end_hint();
	}

	mutex_unlock(&vout_mutex);
	return ret;
}
EXPORT_SYMBOL(vout_func_set_vframe_rate_end_hint);

/*
 *interface export to client who want to notify about source fr_policy.
 */
int vout_func_set_vframe_rate_policy(int index, int policy)
{
	int ret = -1;
	struct vout_server_s *p_server = NULL;

	mutex_lock(&vout_mutex);

	if (index == 1)
		p_server = vout_module.curr_vout_server;
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	else if (index == 2)
		p_server = vout2_module.curr_vout_server;
#endif

	if (!IS_ERR_OR_NULL(p_server)) {
		if (p_server->op.set_vframe_rate_policy)
			ret = p_server->op.set_vframe_rate_policy(policy);
	}

	mutex_unlock(&vout_mutex);
	return ret;
}
EXPORT_SYMBOL(vout_func_set_vframe_rate_policy);

/*
 *interface export to client who want to notify about source fr_policy.
 */
int vout_func_get_vframe_rate_policy(int index)
{
	int ret = -1;
	struct vout_server_s *p_server = NULL;

	mutex_lock(&vout_mutex);

	if (index == 1)
		p_server = vout_module.curr_vout_server;
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	else if (index == 2)
		p_server = vout2_module.curr_vout_server;
#endif

	if (!IS_ERR_OR_NULL(p_server)) {
		if (p_server->op.get_vframe_rate_policy)
			ret = p_server->op.get_vframe_rate_policy();
	}

	mutex_unlock(&vout_mutex);
	return ret;
}
EXPORT_SYMBOL(vout_func_get_vframe_rate_policy);

int vout_func_vout_suspend(int index)
{
	int ret = 0;
	struct vout_server_s *p_server = NULL;

	mutex_lock(&vout_mutex);

	if (index == 1)
		p_server = vout_module.curr_vout_server;
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	else if (index == 2)
		p_server = vout2_module.curr_vout_server;
#endif

	if (!IS_ERR_OR_NULL(p_server)) {
		if (p_server->op.vout_suspend)
			ret = p_server->op.vout_suspend();
	}

	mutex_unlock(&vout_mutex);
	return ret;
}
EXPORT_SYMBOL(vout_func_vout_suspend);

int vout_func_vout_resume(int index)
{
	struct vout_server_s *p_server = NULL;

	mutex_lock(&vout_mutex);

	if (index == 1)
		p_server = vout_module.curr_vout_server;
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	else if (index == 2)
		p_server = vout2_module.curr_vout_server;
#endif

	if (!IS_ERR_OR_NULL(p_server)) {
		if (p_server->op.vout_resume) {
			/* ignore error when resume. */
			p_server->op.vout_resume();
		}
	}

	mutex_unlock(&vout_mutex);
	return 0;
}
EXPORT_SYMBOL(vout_func_vout_resume);

/*
 *interface export to client who want to shutdown.
 */
int vout_func_vout_shutdown(int index)
{
	int ret = -1;
	struct vout_server_s *p_server;
	struct vout_module_s *p_module = NULL;

	if (index == 1)
		p_module = &vout_module;
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	else if (index == 2)
		p_module = &vout2_module;
#endif

	list_for_each_entry(p_server, &p_module->vout_server_list, list) {
		if (p_server->op.vout_shutdown)
			ret = p_server->op.vout_shutdown();
	}

	return ret;
}
EXPORT_SYMBOL(vout_func_vout_shutdown);

/*
 *here we offer two functions to get and register vout module server
 *vout module server will set and store tvmode attributes for vout encoder
 *we can ensure TVMOD SET MODULE independent with these two function.
 */

int vout_func_vout_register_server(int index,
		struct vout_server_s *mem_server)
{
	struct list_head *p_iter;
	struct vout_server_s *p_server;
	struct vout_module_s *p_module = NULL;

	if (mem_server == NULL) {
		VOUTERR("vout%d: server is NULL\n", index);
		return -1;
	}
	if (mem_server->name == NULL) {
		VOUTERR("vout%d: server name is NULL\n", index);
		return -1;
	}
	VOUTPR("vout%d: register server: %s\n", index, mem_server->name);

	mutex_lock(&vout_mutex);

	if (index == 1)
		p_module = &vout_module;
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	else if (index == 2)
		p_module = &vout2_module;
#endif

	list_for_each(p_iter, &p_module->vout_server_list) {
		p_server = list_entry(p_iter, struct vout_server_s, list);

		if (p_server->name &&
			(strcmp(p_server->name, mem_server->name) == 0)) {
			VOUTPR("vout%d: server %s is already registered\n",
				index, mem_server->name);
			/* vout server already registered. */
			mutex_unlock(&vout_mutex);
			return -1;
		}
	}
	list_add(&mem_server->list, &p_module->vout_server_list);
	mutex_unlock(&vout_mutex);
	return 0;
}
EXPORT_SYMBOL(vout_func_vout_register_server);

int vout_func_vout_unregister_server(int index,
		struct vout_server_s *mem_server)
{
	struct vout_server_s  *p_server;
	struct vout_module_s *p_module = NULL;

	if (mem_server == NULL) {
		VOUTERR("vout%d: server is NULL\n", index);
		return -1;
	}
	VOUTPR("vout%d: unregister server: %s\n", index, mem_server->name);

	mutex_lock(&vout_mutex);

	if (index == 1)
		p_module = &vout_module;
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	else if (index == 2)
		p_module = &vout2_module;
#endif

	list_for_each_entry(p_server, &p_module->vout_server_list, list) {
		if (p_server->name && mem_server->name &&
			(strcmp(p_server->name, mem_server->name) == 0)) {
			/*
			 * We will not move current vout server pointer
			 * automatically if current vout server pointer
			 * is the one which will be deleted next.
			 * So you should change current vout server
			 * first then remove it
			 */
			if (p_module->curr_vout_server == p_server)
				p_module->curr_vout_server = NULL;

			list_del(&mem_server->list);
			mutex_unlock(&vout_mutex);
			return 0;
		}
	}
	mutex_unlock(&vout_mutex);
	return 0;
}
EXPORT_SYMBOL(vout_func_vout_unregister_server);

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

int vout_get_vsource_fps(int duration)
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
EXPORT_SYMBOL(vout_get_vsource_fps);

int vout_get_hpd_state(void)
{
	int ret = 0;

#ifdef CONFIG_AMLOGIC_HDMITX
	ret = get_hpd_state();
#endif

	return ret;
}
EXPORT_SYMBOL(vout_get_hpd_state);
