/*
 * drivers/amlogic/media/common/vfm/vfm.c
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

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/io.h>
#include <linux/uaccess.h>
/* Amlogic headers */
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>

#include <linux/amlogic/major.h>
/*for dumpinfos*/
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/codec_mm/configs.h>

/* Local headers */
#include "vftrace.h"
#include "vfm.h"
static DEFINE_SPINLOCK(lock);

#define DRV_NAME    "vfm"
#define DEV_NAME    "vfm"
#define BUS_NAME    "vfm"
#define CLS_NAME    "vfm"
#define VFM_NAME_LEN    100
#define VFM_MAP_SIZE    10
#define VFM_MAP_COUNT   20
static struct device *vfm_dev;
struct vfm_map_s {
	char id[VFM_NAME_LEN];
	char name[VFM_MAP_SIZE][VFM_NAME_LEN];
	int vfm_map_size;
	int valid;
	int active;
};
struct vfm_map_s *vfm_map[VFM_MAP_COUNT];
static int vfm_map_num;
int vfm_debug_flag;		/* 1; */
int vfm_trace_enable;	/* 1; */
int vfm_trace_num = 40;		/*  */

void vf_update_active_map(void)
{
	int i, j;
	struct vframe_provider_s *vfp;

	for (i = 0; i < vfm_map_num; i++) {
		if (vfm_map[i] && vfm_map[i]->valid) {
			for (j = 0; j < (vfm_map[i]->vfm_map_size - 1); j++) {
				vfp = vf_get_provider_by_name(vfm_map[i]->name
					[j]);
				if (vfp == NULL)
					vfm_map[i]->active &= (~(1 << j));
				else {
					if ((j > 0 && vfm_map[i]->active & 0x1)
						|| j == 0)
						vfm_map[i]->active |= (1 << j);
				}
			}
		}
	}
}

static int get_vfm_map_index(const char *id)
{
	int index = -1;
	int i;

	for (i = 0; i < vfm_map_num; i++) {
		if (vfm_map[i]) {
			if (vfm_map[i]->valid &&
				(!strcmp(vfm_map[i]->id, id))) {
				index = i;
				break;
			}
		}
	}
	return index;
}

static int vfm_map_remove_by_index(int index)
{
	int i;
	int ret = 0;
	struct vframe_provider_s *vfp;

	vfm_map[index]->active = 0;
	for (i = 0; i < (vfm_map[index]->vfm_map_size - 1); i++) {
		vfp = vf_get_provider_by_name(vfm_map[index]->name[i]);
		if (vfp && vfp->ops && vfp->ops->event_cb) {
			vfp->ops->event_cb(VFRAME_EVENT_RECEIVER_FORCE_UNREG,
				NULL, vfp->op_arg);
			pr_err("%s: VFRAME_EVENT_RECEIVER_FORCE_UNREG %s\n",
				__func__, vfm_map[index]->name[i]);
		}
	}
	for (i = 0; i < (vfm_map[index]->vfm_map_size - 1); i++) {
		vfp = vf_get_provider_by_name(vfm_map[index]->name[i]);
		if (vfp)
			break;
	}
	if (i < (vfm_map[index]->vfm_map_size - 1)) {
		pr_err("failed remove vfm map %s with active provider %s.\n",
			vfm_map[index]->id, vfm_map[index]->name[i]);
		ret = -1;
	}
	vfm_map[index]->valid = 0;
	return ret;
}

int vfm_map_remove(char *id)
{
	int i;
	int index;
	int ret = 0;

	if (!strcmp(id, "all")) {
		for (i = 0; i < vfm_map_num; i++) {
			if (vfm_map[i])
				ret = vfm_map_remove_by_index(i);
		}
	} else {
		index = get_vfm_map_index(id);
		if (index >= 0)
			ret = vfm_map_remove_by_index(index);
	}
	return ret;
}
EXPORT_SYMBOL(vfm_map_remove);

int vfm_map_add(char *id, char *name_chain)
{
	int i, j;
	int ret = -1;
	char *ptr, *token = NULL;
	struct vfm_map_s *p;
	int old_num = vfm_map_num;
	unsigned long flags;
	int add_ok = 0;
	int cnt = 10;

	p = kmalloc(sizeof(struct vfm_map_s), GFP_KERNEL);
	if (!p) {
		pr_err("%s: Error, map no mem!!\n", __func__);
		return -ENOMEM;
	}
	memset(p, 0, sizeof(struct vfm_map_s));
	if (strlen(id) >= sizeof(p->id)) {
		memcpy(p->id, id, sizeof(p->id));
		p->id[sizeof(p->id)-1] = '\0';
	} else
		memcpy(p->id, id, strlen(id));
	p->valid = 1;
	ptr = name_chain;

	do {
		token = strsep(&ptr, "\n ");
		if (token == NULL)
			break;
		if (*token == '\0')
			continue;
		if (strlen(token) >= sizeof(p->name[p->vfm_map_size])) {
			memcpy(p->name[p->vfm_map_size], token,
				sizeof(p->name[p->vfm_map_size]));
			p->name[p->vfm_map_size][
				sizeof(p->name[p->vfm_map_size])-1] = '\0';
		} else
			memcpy(p->name[p->vfm_map_size], token, strlen(token));
		p->vfm_map_size++;
	} while (token && cnt--);

	cnt = 10; /*limit the cnt of retry to avoid the infinite loop*/

retry:
	for (i = 0; i < vfm_map_num; i++) {
		struct vfm_map_s *pi = vfm_map[i];

		if (!pi || (strcmp(pi->id, p->id))) {
			/*not same id to next one*/
			continue;
		} else if (pi->valid) {
			for (j = 0; j < p->vfm_map_size; j++) {
				if (strcmp(pi->name[j],
					p->name[j])){
					break;
				}
			}
			if (j == p->vfm_map_size) {
				pi->valid = 1;
				kfree(p);
				add_ok = 1;
				break;
			}
		} else if (!pi->valid) {
			/*
			 *over write old setting.
			 *  don't free old one,
			 * because it may on used.
			 */
			for (j = 0; j < p->vfm_map_size; j++) {
				/*over write node.*/
				strcpy(pi->name[j], p->name[j]);
			}
			pi->vfm_map_size = p->vfm_map_size;
			pi->valid = 1;
			kfree(p);
			add_ok = 1;
			break;
		}
	}
	if (!add_ok) {
		spin_lock_irqsave(&lock, flags);
		if (i == old_num && old_num != vfm_map_num && cnt--) {
			spin_unlock_irqrestore(&lock, flags);
			pr_err("%s: vfm_map changed on add, need retry!\n",
				__func__);
			goto retry;
		}
		if (i == vfm_map_num) {
			if (i < VFM_MAP_COUNT) {
				vfm_map[i] = p;
				vfm_map_num++;
				add_ok = 1;
			} else{
				pr_err("%s: Error, map full\n", __func__);
				ret = -1;
				kfree(p);
			}
		} else
			kfree(p);
		spin_unlock_irqrestore(&lock, flags);
	}
	if (add_ok)
		ret = 0;
	return ret;
}
EXPORT_SYMBOL(vfm_map_add);

static char *vf_get_provider_name_inmap(int i, const char *receiver_name)
{
	int j;
	char *provider_name = NULL;

	for (j = 0; j < vfm_map[i]->vfm_map_size; j++) {
		if (!strcmp(vfm_map[i]->name[j], receiver_name)) {
			if ((j > 0) &&
				((vfm_map[i]->active >> (j - 1)) & 0x1)) {
				provider_name = vfm_map[i]->name[j - 1];
			}
			break;
		}
	}
	return provider_name;
}

char *vf_get_provider_name(const char *receiver_name)
{
	int i;
	char *provider_name = NULL;

	for (i = 0; i < vfm_map_num; i++) {
		if (vfm_map[i] && vfm_map[i]->active) {
			provider_name = vf_get_provider_name_inmap(i,
				receiver_name);
		}
		if (provider_name)
			break;
	}
	return provider_name;
}

static char *vf_get_receiver_name_inmap(int i, const char *provider_name)
{
	int j;
	int provide_namelen = strlen(provider_name);
	bool found = false;
	char *receiver_name = NULL;
	int namelen;

	for (j = 0; j < vfm_map[i]->vfm_map_size; j++) {
		namelen = strlen(vfm_map[i]->name[j]);
		if (vfm_debug_flag & 2) {
			pr_err("%s:vfm_map:%s\n", __func__,
				vfm_map[i]->name[j]);
		}
		if ((!strncmp(vfm_map[i]->name[j], provider_name, namelen)) &&
			((j + 1) < vfm_map[i]->vfm_map_size)) {

			if (namelen == provide_namelen) {
				/* exact match */
				receiver_name = vfm_map[i]->name[j + 1];
				found = true;
				break;
			} else if (provider_name[namelen] == '.') {
				/*
				 *continue looking, an exact matching
				 * has higher priority
				 */
				receiver_name = vfm_map[i]->name[j + 1];
			}
		}

	}
	return receiver_name;
}

char *vf_get_receiver_name(const char *provider_name)
{
	int i;
	char *receiver_name = NULL;

	for (i = 0; i < vfm_map_num; i++) {
		if (vfm_map[i] && vfm_map[i]->valid && vfm_map[i]->active) {
			receiver_name = vf_get_receiver_name_inmap(i,
				provider_name);
		}
		if (receiver_name)
			break;
	}
	return receiver_name;
}

static void vfm_init(void)
{
#if ((defined CONFIG_AMLOGIC_POST_PROCESS_MANAGER) && \
	(defined CONFIG_AMLOGIC_MEDIA_DEINTERLACE))
	char def_id[] = "default";
#ifndef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
	char def_name_chain[] = "decoder ppmgr deinterlace amvideo";
#else
	char def_name_chain[] = "decoder amvideo";
#endif
#elif (defined CONFIG_AMLOGIC_POST_PROCESS_MANAGER)
	char def_id[] = "default";
	char def_name_chain[] = "decoder ppmgr amvideo";
#elif (defined CONFIG_AMLOGIC_MEDIA_DEINTERLACE)
	char def_id[] = "default";
	char def_name_chain[] = "decoder deinterlace amvideo";
#else /**/
	char def_id[] = "default";
	char def_name_chain[] = "decoder amvideo";
#endif /**/
#ifdef CONFIG_TVIN_VIUIN
	char def_ext_id[] = "default_ext";
	char def_ext_name_chain[] = "vdin amvideo2";
#endif /**/
#ifdef CONFIG_VDIN_MIPI
	char def_mipi_id[] = "default_mipi";
	char def_mipi_name_chain[] = "vdin mipi";
#endif /**/
#ifdef CONFIG_AMLOGIC_V4L_VIDEO2
	char def_amlvideo2_id[] = "default_amlvideo2";
	char def_amlvideo2_chain[] = "vdin1 amlvideo2.1";
#endif /**/
#if (defined CONFIG_TVIN_AFE) || (defined CONFIG_TVIN_HDMI)
#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER
	char tvpath_id[] = "tvpath";
	char tvpath_chain[] = "vdin0 ppmgr deinterlace amvideo";
#else
	char tvpath_id[] = "tvpath";
	char tvpath_chain[] = "vdin0 deinterlace amvideo";
#endif
#endif /**/
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	char def_dvbl_id[] = "dvblpath";
/*	char def_dvbl_chain[] = "dvbldec dvbl amvideo";*/
	char def_dvbl_chain[] = "dvbldec amvideo";

	char def_dvel_id[] = "dvelpath";
	char def_dvel_chain[] = "dveldec dvel";
#endif
#if 1/*def CONFIG_AM_HDMIIN_DV*/
	char def_dvhdmiin_id[] = "dvhdmiin";
	char def_dvhdmiin_chain[] = "dv_vdin amvideo";
#endif
	int i;

	for (i = 0; i < VFM_MAP_COUNT; i++)
		vfm_map[i] = NULL;
	vfm_map_add(def_id, def_name_chain);
#ifdef CONFIG_VDIN_MIPI
	vfm_map_add(def_mipi_id, def_mipi_name_chain);
#endif /**/

#if (defined CONFIG_TVIN_AFE) || (defined CONFIG_TVIN_HDMI)
	vfm_map_add(tvpath_id, tvpath_chain);
#endif /**/
#ifdef CONFIG_AMLOGIC_V4L_VIDEO2
	vfm_map_add(def_amlvideo2_id, def_amlvideo2_chain);
#endif /**/
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	vfm_map_add(def_dvbl_id, def_dvbl_chain);
	vfm_map_add(def_dvel_id, def_dvel_chain);
#endif
#if 1/*def CONFIG_AM_HDMIIN_DV*/
	vfm_map_add(def_dvhdmiin_id, def_dvhdmiin_chain);
#endif
}

/*
 * cat /sys/class/vfm/map
 */
static ssize_t vfm_map_show(struct class *class,
	struct class_attribute *attr, char *buf)
{
	int i, j;
	int len = 0;

	for (i = 0; i < vfm_map_num; i++) {
		if (vfm_map[i] && vfm_map[i]->valid) {
			len += sprintf(buf + len, "[%02d]  %s { ",
				i,/*in slot num.*/
				vfm_map[i]->id);
			for (j = 0; j < vfm_map[i]->vfm_map_size; j++) {
				if (j < (vfm_map[i]->vfm_map_size - 1)) {
					len += sprintf(buf + len, "%s(%d) ",
					   vfm_map[i]->name[j],
					   (vfm_map[i]->active >> j) & 0x1);
				} else{
					len += sprintf(buf + len, "%s",
						vfm_map[i]->name[j]);
				}
			}
			len += sprintf(buf + len, "}\n");
		}
	}
	len += provider_list(buf + len);
	len += receiver_list(buf + len);
	return len;
}

static int vfm_vf_get_states(struct vframe_provider_s *vfp,
	struct vframe_states *states)
{
	int ret = -1;
	unsigned long flags;

	spin_lock_irqsave(&lock, flags);
	ret = vf_get_states(vfp, states);
	spin_unlock_irqrestore(&lock, flags);
	return ret;
}

static inline struct vframe_s *vfm_vf_peek(
	struct vframe_provider_s *vfp)
{
	if (!(vfp && vfp->ops && vfp->ops->peek))
		return NULL;
	return vfp->ops->peek(vfp->op_arg);
}

static void vfm_dump_provider(const char *name)
{
	struct vframe_provider_s *prov = vf_get_provider_by_name(name);
	struct vframe_states states;
	unsigned long flags;
	struct vframe_s *vf;
	char *buf, *pbuf;

	if (!prov)
		return;

	buf = kzalloc(0x400, GFP_KERNEL);
	if (buf == NULL)
		return;

	pbuf = buf;

	if (!vfm_vf_get_states(prov, &states)) {
		pr_info("vframe_pool_size=%d\n",
			states.vf_pool_size);
		pr_info("vframe buf_free_num=%d\n",
			states.buf_free_num);
		pr_info("vframe buf_recycle_num=%d\n",
			states.buf_recycle_num);
		pr_info("vframe buf_avail_num=%d\n",
			states.buf_avail_num);

		spin_lock_irqsave(&lock, flags);

		vf = vfm_vf_peek(prov);
		if (vf) {
			pbuf += sprintf(pbuf,
				"vframe ready frame delayed =%dms\n",
				(int)(jiffies_64 -
				vf->ready_jiffies64) * 1000 /
				HZ);
			pbuf += sprintf(pbuf, "vf index=%d\n", vf->index);
			pbuf += sprintf(pbuf, "vf->pts=%d\n", vf->pts);
			pbuf += sprintf(pbuf, "vf->type=%d\n", vf->type);
			if (vf->type & VIDTYPE_COMPRESS) {
				pbuf += sprintf(pbuf, "vf compHeadAddr=%x\n",
						vf->compHeadAddr);
				pbuf += sprintf(pbuf, "vf compBodyAddr =%x\n",
						vf->compBodyAddr);
			} else {
				pbuf += sprintf(pbuf, "vf canvas0Addr=%x\n",
					vf->canvas0Addr);
				pbuf += sprintf(pbuf, "vf canvas1Addr=%x\n",
					vf->canvas1Addr);
				pbuf += sprintf(pbuf,
					"vf canvas0Addr.y.addr=%x(%d)\n",
					canvas_get_addr(
					canvasY(vf->canvas0Addr)),
					canvas_get_addr(
					canvasY(vf->canvas0Addr)));
				pbuf += sprintf(pbuf,
					"vf canvas0Adr.uv.adr=%x(%d)\n",
					canvas_get_addr(
					canvasUV(vf->canvas0Addr)),
					canvas_get_addr(
					canvasUV(vf->canvas0Addr)));
			}
		}
		spin_unlock_irqrestore(&lock, flags);

		pr_info("%s\n", buf);
	}
	vftrace_dump_trace_infos(prov->traceget);
	vftrace_dump_trace_infos(prov->traceput);

	kfree(buf);
}

#define VFM_CMD_ADD 1
#define VFM_CMD_RM  2
#define VFM_CMD_DUMP  3
#define VFM_CMD_ADDDUMMY 4

/*dummy receiver*/

static int dummy_receiver_event_fun(int type, void *data, void *arg)
{
	struct vframe_receiver_s *dummy_vf_recv
		= (struct vframe_receiver_s *)arg;
	if (type == VFRAME_EVENT_PROVIDER_UNREG) {
		char *provider_name = (char *)data;

		pr_info("%s, provider %s unregistered\n",
			__func__, provider_name);
	} else if (type ==
		VFRAME_EVENT_PROVIDER_VFRAME_READY) {
		struct vframe_s *vframe_tmp = vf_get(dummy_vf_recv->name);

		while (vframe_tmp) {
			vf_put(vframe_tmp, dummy_vf_recv->name);
			vf_notify_provider(dummy_vf_recv->name,
				VFRAME_EVENT_RECEIVER_PUT, NULL);
			vframe_tmp = vf_get(dummy_vf_recv->name);
		}
	} else if (type == VFRAME_EVENT_PROVIDER_QUREY_STATE) {
		return RECEIVER_ACTIVE;
	} else if (type == VFRAME_EVENT_PROVIDER_REG) {
		char *provider_name = (char *)data;

		pr_info("%s, provider %s registered\n",
			__func__, provider_name);
	}
	return 0;
}

static const struct vframe_receiver_op_s dummy_vf_receiver = {
	.event_cb = dummy_receiver_event_fun
};

static void add_dummy_receiver(char *vfm_name_)
{
	struct vframe_receiver_s *dummy_vf_recv =
	 kmalloc(sizeof(struct vframe_receiver_s), GFP_KERNEL);
	pr_info("%s(%s)\n", __func__, vfm_name_);
	if (dummy_vf_recv) {
		char *vfm_name = kmalloc(16, GFP_KERNEL);

		snprintf(vfm_name, 16, "%s", vfm_name_);
		vf_receiver_init(dummy_vf_recv, vfm_name,
			&dummy_vf_receiver, dummy_vf_recv);
		vf_reg_receiver(dummy_vf_recv);
		pr_info("%s: %s\n", __func__, dummy_vf_recv->name);
	}
}

/**/

/*
 * echo add <name> <node1 node2 ...> > /sys/class/vfm/map
 * echo rm <name>                    > /sys/class/vfm/map
 * echo rm all                       > /sys/class/vfm/map
 * echo dump providername			> /sys/class/vfm/map
 * echo dummy name > /sys/class/vfm/map
 * <name> the name of the path.
 * <node1 node2 ...> the name of the nodes in the path.
 */
static ssize_t vfm_map_store(struct class *class,
		 struct class_attribute *attr,
		 const char *buf, size_t count)
{
	char *buf_orig, *ps, *token;
	int i = 0;
	int cmd = 0;
	char *id = NULL;

	if (vfm_debug_flag & 0x10000)
		return count;
	pr_err("%s:%s\n", __func__, buf);
	buf_orig = kstrdup(buf, GFP_KERNEL);
	ps = buf_orig;
	while (1) {
		token = strsep(&ps, "\n ");
		if (token == NULL)
			break;
		if (*token == '\0')
			continue;
		if (i == 0) {	/* command */
			if (!strcmp(token, "add"))
				cmd = VFM_CMD_ADD;
			else if (!strcmp(token, "rm"))
				cmd = VFM_CMD_RM;
			else if (!strcmp(token, "dump"))
				cmd = VFM_CMD_DUMP;
			else if (!strcmp(token, "dummy"))
				cmd = VFM_CMD_ADDDUMMY;
			else
				break;
		} else if (i == 1) {
			id = token;
			if (cmd == VFM_CMD_ADD) {
				/* pr_err("vfm_map_add(%s,%s)\n",id,ps); */
				vfm_map_add(id, ps);
			} else if (cmd == VFM_CMD_RM) {
				/* pr_err("vfm_map_remove(%s)\n",id); */
				if (vfm_map_remove(id) < 0)
					count = 0;
			} else if (cmd == VFM_CMD_DUMP) {
				vfm_dump_provider(token);
			} else if (cmd == VFM_CMD_ADDDUMMY) {
				add_dummy_receiver(token);
			}
			break;
		}
		i++;
	}
	kfree(buf_orig);
	return count;
}

static CLASS_ATTR(map, 0664, vfm_map_show, vfm_map_store);
static struct class vfm_class = {
	.name = CLS_NAME,
	};
int vfm_map_store_fun(const char *trigger, int id, const char *buf, int size)
{
	int ret = size;

	switch (id) {
	case 0:	return vfm_map_store(NULL, NULL, buf, size);
	default:
		ret = -1;
	}
	return size;
}
int vfm_map_show_fun(const char *trigger, int id, char *sbuf, int size)
{
	int ret = -1;

	void *buf, *getbuf = NULL;

	if (size < PAGE_SIZE) {
		getbuf = (void *)__get_free_page(GFP_KERNEL);
		if (!getbuf)
			return -ENOMEM;
		buf = getbuf;
	} else {
		buf = sbuf;
	}

	switch (id) {
	case 0:
		ret = vfm_map_show(NULL, NULL, buf);
		break;
	default:
		ret = -1;
	}
	if (ret > 0 && getbuf != NULL) {
		ret = min_t(int, ret, size);
		strncpy(sbuf, buf, ret);
	}
	if (getbuf != NULL)
		free_page((unsigned long)getbuf);
	return ret;
}

static struct mconfig vfm_configs[] = {
	MC_FUN_ID("map", vfm_map_show_fun, vfm_map_store_fun, 0),
};

/*********************************************************
 * /dev/vfm APIs
 *********************************************************/
static int vfm_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int vfm_release(struct inode *inode, struct file *file)
{
	return 0;
}

static long vfm_ioctl(struct file *file, unsigned int cmd, ulong arg)
{
	long ret = 0;

	struct vfmctl *user_argp = (void __user *)arg;
	struct vfmctl argp;

	switch (cmd) {
	case VFM_IOCTL_CMD_SET:{
		ret =
		copy_from_user(argp.name, user_argp->name, sizeof(argp.name));
		ret |=
		copy_from_user(argp.val, user_argp->val, sizeof(argp.val));
		if (ret)
			ret = -EINVAL;
		else
		ret =
		vfm_map_store(NULL, NULL, argp.val, sizeof(argp.val));
		}
		break;
	case VFM_IOCTL_CMD_GET:{
	/*
	 *overflow bug, need fixed.
	 *vfm_map_show(NULL, NULL, argp.val);
	 *ret = copy_to_user(user_argp->val, argp.val, sizeof(argp.val));
	 *if (ret != 0)
	 *	return -EIO;
	 *}
	 */
		return -EIO;
		}
		break;
	case VFM_IOCTL_CMD_ADD:{
		ret =
		copy_from_user(argp.name, user_argp->name, sizeof(argp.name));
		ret |=
		copy_from_user(argp.val, user_argp->val, sizeof(argp.val));
		if (ret)
			ret = -EINVAL;
		else
		ret = vfm_map_add(argp.name, argp.val);
		}
		break;
	case VFM_IOCTL_CMD_RM:{
		ret =
		copy_from_user(argp.val, user_argp->val, sizeof(argp.val));
		if (ret)
			ret = -EINVAL;
		else
		ret = vfm_map_remove(argp.val);
		}
		break;
	case VFM_IOCTL_CMD_DUMP:{
		ret =
		copy_from_user(argp.val, user_argp->val, sizeof(argp.val));
		if (ret)
			ret = -EINVAL;
		argp.val[sizeof(argp.val) - 1] = '\0';
		vfm_dump_provider(argp.val);
		}
		break;
	case VFM_IOCTL_CMD_ADDDUMMY:{
		ret =
		copy_from_user(argp.val, user_argp->val, sizeof(argp.val));
		if (ret)
			ret = -EINVAL;
		add_dummy_receiver(argp.val);
		}

		break;
	default:
		return -EINVAL;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long vfm_compat_ioctl(struct file *file, unsigned int cmd, ulong arg)
{
	long ret = 0;

	ret = vfm_ioctl(file, cmd, (ulong)compat_ptr(arg));
	return ret;
}
#endif
static const struct file_operations vfm_fops = {
	.owner = THIS_MODULE,
	.open = vfm_open,
	.release = vfm_release,
	.unlocked_ioctl = vfm_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = vfm_compat_ioctl,
#endif
	.poll = NULL,
};

static int __init vfm_class_init(void)
{
	int error;

	vfm_init();
	error = class_register(&vfm_class);
	if (error) {
		pr_err("%s: class_register failed\n", __func__);
		return error;
	}
	error = class_create_file(&vfm_class, &class_attr_map);
	if (error) {
		pr_err("%s: class_create_file failed\n", __func__);
		class_unregister(&vfm_class);
	}
	REG_PATH_CONFIGS("media.vfm", vfm_configs);


	/* create vfm device */
	error = register_chrdev(VFM_MAJOR, "vfm", &vfm_fops);
	if (error < 0) {
		pr_err("Can't register major for vfm device\n");
		return error;
	}
	vfm_dev = device_create(&vfm_class, NULL,
		MKDEV(VFM_MAJOR, 0), NULL, DEV_NAME);
	return error;
}

static void __exit vfm_class_exit(void)
{
	class_unregister(&vfm_class);
	unregister_chrdev(VFM_MAJOR, DEV_NAME);
}

fs_initcall(vfm_class_init);
module_exit(vfm_class_exit);
MODULE_PARM_DESC(vfm_debug_flag, "\n vfm_debug_flag\n");
module_param(vfm_debug_flag, int, 0664);
MODULE_PARM_DESC(vfm_map_num, "\n vfm_map_num\n");
module_param(vfm_map_num, int, 0664);

module_param(vfm_trace_enable, int, 0664);
MODULE_PARM_DESC(vfm_trace_enable, "\n vfm_trace_enable\n");
module_param(vfm_trace_num, int, 0664);
MODULE_PARM_DESC(vfm_trace_num, "\n vfm_trace_num\n");

MODULE_DESCRIPTION("Amlogic video frame manager driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bobby Yang <bo.yang@amlogic.com>");
