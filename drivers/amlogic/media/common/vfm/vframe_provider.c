/*
 * drivers/amlogic/media/common/vfm/vframe_provider.c
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

/* Standard Linux headers */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/list.h>

/* Amlogic headers */
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>

/* Local headers */
#include "vfm.h"
#include "vftrace.h"

#define MAX_PROVIDER_NUM    32
struct vframe_provider_s *provider_table[MAX_PROVIDER_NUM];

int provider_list(char *buf)
{
	struct vframe_provider_s *p = NULL;
	int len = 0;
	int i;

	len += sprintf(buf + len, "\nprovider list:\n");
	for (i = 0; i < MAX_PROVIDER_NUM; i++) {
		p = provider_table[i];
		if (p)
			len += sprintf(buf + len, "   %s\n", p->name);
	}
	return len;
}

/*
 * get vframe provider from the provider list by receiver name.
 *
 */
struct vframe_provider_s *vf_get_provider_by_name(const char *provider_name)
{
	struct vframe_provider_s *p = NULL;
	int i;

	if (provider_name) {
		int namelen = strlen(provider_name);

		for (i = 0; i < MAX_PROVIDER_NUM; i++) {
			p = provider_table[i];
			if (p && p->name && !strncmp(p->name,
					provider_name, namelen)) {
				if (strlen(p->name) == namelen
					|| p->name[namelen] == '.')
					break;
			}
		}
		if (i == MAX_PROVIDER_NUM)
			p = NULL;
	}
	return p;
}

struct vframe_provider_s *vf_get_provider(const char *receiver_name)
{
	struct vframe_provider_s *p = NULL;
	char *provider_name;

	provider_name = vf_get_provider_name(receiver_name);
	p = vf_get_provider_by_name(provider_name);
	return p;
}
EXPORT_SYMBOL(vf_get_provider);

/*#define NO_CHEKC_PROVIDER_USE*/
#ifdef NO_CHEKC_PROVIDER_USE
static inline int use_provider(struct vframe_provider_s *prov)
{
	return prov != NULL;
}
static inline void unuse_provider(struct vframe_provider_s *prov)
{
}
static int vf_provider_close(struct vframe_provider_s *prov)
{
	return 1;
}
#else
static inline int use_provider(struct vframe_provider_s *prov)
{
	int ret = 0;

	if (prov) {
		ret = atomic_inc_return(&prov->use_cnt);
		if (ret <= 0)
			atomic_dec_return(&prov->use_cnt);
	}
	return ret > 0;
}
static inline void unuse_provider(struct vframe_provider_s *prov)
{
	if (prov)
		atomic_dec_return(&prov->use_cnt);
}
#define CLOSED_CNT -100000
static int vf_provider_close(struct vframe_provider_s *prov)
{
	int ret;
	int wait_max = 10000;

	if (!prov)
		return -1;
	ret = atomic_add_return(CLOSED_CNT, &prov->use_cnt);
	while (ret > CLOSED_CNT && wait_max-- > 0) {
		schedule();/*shecule and  wait for complete.*/
		ret = atomic_read(&prov->use_cnt);
	};
	if (ret > CLOSED_CNT) {
		pr_err("**ERR***,release, provider %s not finised,%d, wait=%d\n",
			prov->name, ret, wait_max);
	}
	return 0;
}
#endif
int vf_notify_provider(const char *receiver_name, int event_type, void *data)
{
	int ret = -1;
	struct vframe_provider_s *provider = vf_get_provider(receiver_name);

	if (use_provider(provider)) {
		if (provider->ops && provider->ops->event_cb) {
			provider->ops->event_cb(event_type, data,
				provider->op_arg);
			ret = 0;
		}
		unuse_provider(provider);
	} else {
		/* pr_err("Error: %s, fail to get provider of receiver %s\n",*/
		   /*__func__, receiver_name); */
	}
	return ret;
}
EXPORT_SYMBOL(vf_notify_provider);

int vf_notify_provider_by_name(const char *provider_name, int event_type,
							   void *data)
{
	int ret = -1;
	struct vframe_provider_s *provider =
		vf_get_provider_by_name(provider_name);
	if (use_provider(provider)) {
		if (provider->ops && provider->ops->event_cb) {
			provider->ops->event_cb(event_type, data,
				provider->op_arg);
			ret = 0;
		}
		unuse_provider(provider);
	} else{
		/* pr_err("Error: %s, fail to get provider of receiver %s\n",*/
				/*__func__, receiver_name); */
	}
	return ret;
}
EXPORT_SYMBOL(vf_notify_provider_by_name);

void vf_provider_init(struct vframe_provider_s *prov,
		const char *name,
		const struct vframe_operations_s *ops, void *op_arg)
{
	if (!prov)
		return;
	/* memset(prov, 0, sizeof(struct vframe_provider_s)); */
	prov->name = name;
	prov->ops = ops;
	prov->op_arg = op_arg;
	prov->traceget = NULL;
	prov->traceput = NULL;
	atomic_set(&prov->use_cnt, 0);
	INIT_LIST_HEAD(&prov->list);
}
EXPORT_SYMBOL(vf_provider_init);

int vf_reg_provider(struct vframe_provider_s *prov)
{
	struct vframe_provider_s *p = NULL;
	struct vframe_receiver_s *receiver = NULL;
	int i;

	if (!prov || !prov->name)
		return -1;
	prov->traceget = NULL;
	prov->traceput = NULL;
	for (i = 0; i < MAX_PROVIDER_NUM; i++) {
		p = provider_table[i];
		if (p) {
			if (!strcmp(p->name, prov->name))
				return -1;
		}
	}
	for (i = 0; i < MAX_PROVIDER_NUM; i++) {
		if (provider_table[i] == NULL) {
			provider_table[i] = prov;
			break;
		}
	}
	if (i < MAX_PROVIDER_NUM) {
		vf_update_active_map();
		receiver = vf_get_receiver(prov->name);
		if (receiver && receiver->ops && receiver->ops->event_cb) {
			receiver->ops->event_cb(VFRAME_EVENT_PROVIDER_REG,
				(void *)prov->name,
				receiver->op_arg);
		} else{
			pr_err("%s Error to notify receiver\n", __func__);
		}
		if (vfm_debug_flag & 1)
			pr_err("%s:%s\n", __func__, prov->name);
	} else {
		pr_err("%s: Error, provider_table full\n", __func__);
	}
	atomic_set(&prov->use_cnt, 0);/*set it ready for use.*/
	if (vfm_trace_enable & 1)
		prov->traceget = vftrace_alloc_trace(prov->name, 1,
				vfm_trace_num);
	if (vfm_trace_enable & 2)
		prov->traceput = vftrace_alloc_trace(prov->name, 0,
			vfm_trace_num);
	return 0;
}
EXPORT_SYMBOL(vf_reg_provider);

void vf_unreg_provider(struct vframe_provider_s *prov)
{
	struct vframe_provider_s *p = NULL;
	struct vframe_receiver_s *receiver = NULL;
	int i;

	for (i = 0; i < MAX_PROVIDER_NUM; i++) {
		p = provider_table[i];
		if (p && !strcmp(p->name, prov->name)) {
			if (p->traceget) {
				vftrace_free_trace(p->traceget);
				p->traceget = NULL;
			}
			if (p->traceput) {
				vftrace_free_trace(p->traceput);
				p->traceput = NULL;
			}
			vf_provider_close(prov);
			provider_table[i] = NULL;
			if (vfm_debug_flag & 1)
				pr_err("%s:%s\n", __func__, prov->name);
			receiver = vf_get_receiver(prov->name);
			if (receiver && receiver->ops
				&& receiver->ops->event_cb) {
				receiver->ops->event_cb(
					VFRAME_EVENT_PROVIDER_UNREG,
					NULL,
					receiver->op_arg);
			} else {
				pr_err("%s Error to notify receiver\n",
					__func__);
			}
			vf_update_active_map();
			break;
		}
	}
}
EXPORT_SYMBOL(vf_unreg_provider);

void vf_light_unreg_provider(struct vframe_provider_s *prov)
{
	struct vframe_provider_s *p = NULL;
	struct vframe_receiver_s *receiver = NULL;
	int i;

	for (i = 0; i < MAX_PROVIDER_NUM; i++) {
		p = provider_table[i];
		if (p && !strcmp(p->name, prov->name)) {
			if (vfm_debug_flag & 1)
				pr_err("%s:%s\n", __func__, prov->name);
			receiver = vf_get_receiver(p->name);
			if (receiver && receiver->ops
				&& receiver->ops->event_cb) {
				receiver->ops->event_cb(
					VFRAME_EVENT_PROVIDER_LIGHT_UNREG,
					NULL, receiver->op_arg);
			}
			break;
		}
	}
}
EXPORT_SYMBOL(vf_light_unreg_provider);

void vf_ext_light_unreg_provider(struct vframe_provider_s *prov)
{
	struct vframe_provider_s *p = NULL;
	struct vframe_receiver_s *receiver = NULL;
	int i;

	for (i = 0; i < MAX_PROVIDER_NUM; i++) {
		p = provider_table[i];
		if (p && !strcmp(p->name, prov->name)) {
			provider_table[i] = NULL;
			if (vfm_debug_flag & 1)
				pr_err("%s:%s\n", __func__, prov->name);

			receiver = vf_get_receiver(prov->name);
			if (receiver && receiver->ops
				&& receiver->ops->event_cb) {
				receiver->ops->event_cb(
					VFRAME_EVENT_PROVIDER_LIGHT_UNREG,
					 NULL, receiver->op_arg);
			}
			vf_update_active_map();
			break;
		}
	}
}
EXPORT_SYMBOL(vf_ext_light_unreg_provider);

struct vframe_s *vf_peek(const char *receiver)
{
	struct vframe_provider_s *vfp;
	struct vframe_s *vf = NULL;
	vfp = vf_get_provider(receiver);
	if (use_provider(vfp)) {
		if (vfp->ops && vfp->ops->peek)
			vf = vfp->ops->peek(vfp->op_arg);
		unuse_provider(vfp);
	}
	return vf;
}
EXPORT_SYMBOL(vf_peek);

struct vframe_s *vf_get(const char *receiver)
{

	struct vframe_provider_s *vfp;
	struct vframe_s *vf = NULL;
	vfp = vf_get_provider(receiver);
	if (use_provider(vfp)) {
		if (vfp->ops && vfp->ops->get)
			vf = vfp->ops->get(vfp->op_arg);
		if (vf)
			vftrace_info_in(vfp->traceget, vf);
		unuse_provider(vfp);
	}
	return vf;
}
EXPORT_SYMBOL(vf_get);

void vf_put(struct vframe_s *vf, const char *receiver)
{
	struct vframe_provider_s *vfp;

	vfp = vf_get_provider(receiver);
	if (use_provider(vfp)) {
		if (vfp->ops && vfp->ops->put)
			vfp->ops->put(vf, vfp->op_arg);
		if (vf)
			vftrace_info_in(vfp->traceput, vf);
		unuse_provider(vfp);
	}
}
EXPORT_SYMBOL(vf_put);

int vf_get_states(struct vframe_provider_s *vfp,
	struct vframe_states *states)
{
	int ret = -1;

	if (use_provider(vfp)) {
		if (vfp->ops && vfp->ops->vf_states)
			ret = vfp->ops->vf_states(states, vfp->op_arg);
		unuse_provider(vfp);
	}
	return ret;
}
EXPORT_SYMBOL(vf_get_states);
int vf_get_states_by_name(const char *receiver_name,
	struct vframe_states *states)
{
	struct vframe_provider_s *vfp;

	vfp = vf_get_provider(receiver_name);
	return vf_get_states(vfp, states);
}
EXPORT_SYMBOL(vf_get_states_by_name);


