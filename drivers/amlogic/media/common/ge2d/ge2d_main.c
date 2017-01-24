/*
 * drivers/amlogic/media/common/ge2d/ge2d_main.c
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
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/fb.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/sysfs.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/reset.h>
#include <linux/clk.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif
/* Amlogic Headers */
#include <linux/amlogic/media/ge2d/ge2d.h>
#include <linux/amlogic/media/ge2d/ge2d_cmd.h>
#include <linux/amlogic/cpu_version.h>

/* Local Headers */
#include "ge2dgen.h"
#include "ge2d_log.h"
#include "ge2d_wq.h"

#define GE2D_CLASS_NAME "ge2d"

struct ge2d_device_s {
	char name[20];
	unsigned int open_count;
	int major;
	unsigned int dbg_enable;
	struct class *cla;
	struct device *dev;
};

static struct ge2d_device_s ge2d_device;
static DEFINE_MUTEX(ge2d_mutex);
unsigned int ge2d_log_level;
unsigned int ge2d_dump_reg_enable;
unsigned int ge2d_dump_reg_cnt;

static int ge2d_open(struct inode *inode, struct file *file);
static long ge2d_ioctl(struct file *filp, unsigned int cmd,
		       unsigned long args);
#ifdef CONFIG_COMPAT
static long ge2d_compat_ioctl(struct file *filp, unsigned int cmd,
			      unsigned long args);
#endif
static int ge2d_release(struct inode *inode, struct file *file);
static ssize_t log_level_show(struct class *cla,
			      struct class_attribute *attr,
			      char *buf);
static ssize_t log_level_store(struct class *cla,
			       struct class_attribute *attr,
			       const char *buf, size_t count);
static ssize_t dump_reg_enable_show(struct class *cla,
			      struct class_attribute *attr,
			      char *buf);
static ssize_t dump_reg_enable_store(struct class *cla,
				 struct class_attribute *attr,
				 const char *buf, size_t count);
static ssize_t dump_reg_cnt_show(struct class *cla,
			      struct class_attribute *attr,
			      char *buf);
static ssize_t dump_reg_cnt_store(struct class *cla,
			       struct class_attribute *attr,
			       const char *buf, size_t count);


static const struct file_operations ge2d_fops = {
	.owner		= THIS_MODULE,
	.open		= ge2d_open,
	.unlocked_ioctl = ge2d_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = ge2d_compat_ioctl,
#endif
	.release	= ge2d_release,
};

static struct class_attribute ge2d_class_attrs[] = {
	__ATTR(wq_status, 0644,
			work_queue_status_show, NULL),
	__ATTR(fq_status, 0644,
			free_queue_status_show, NULL),
	__ATTR(log_level, 0644,
			log_level_show, log_level_store),
	__ATTR(dump_reg_enable, 0644,
			dump_reg_enable_show, dump_reg_enable_store),
	__ATTR(dump_reg_cnt, 0644,
			dump_reg_cnt_show, dump_reg_cnt_store),
	__ATTR_NULL
};

static struct class ge2d_class = {
	.name = GE2D_CLASS_NAME,
	.class_attrs = ge2d_class_attrs,
};

static ssize_t dump_reg_enable_show(struct class *cla,
			      struct class_attribute *attr,
			      char *buf)
{
	return snprintf(buf, 40, "%d\n", ge2d_dump_reg_enable);
}

static ssize_t dump_reg_enable_store(struct class *cla,
			       struct class_attribute *attr,
			       const char *buf, size_t count)
{
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	ge2d_log_info("ge2d dump_reg_enbale: %d->%d\n",
		ge2d_dump_reg_enable, res);
	ge2d_dump_reg_enable = res;

	return count;
}

static ssize_t dump_reg_cnt_show(struct class *cla,
			      struct class_attribute *attr,
			      char *buf)
{
	return snprintf(buf, 40, "%d\n", ge2d_dump_reg_cnt);
}

static ssize_t dump_reg_cnt_store(struct class *cla,
			       struct class_attribute *attr,
			       const char *buf, size_t count)
{
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	ge2d_log_info("ge2d dump_reg: %d->%d\n", ge2d_dump_reg_cnt, res);
	ge2d_dump_reg_cnt = res;
	return count;
}

static ssize_t log_level_show(struct class *cla,
			      struct class_attribute *attr,
			      char *buf)
{
	return snprintf(buf, 40, "%d\n", ge2d_log_level);
}

static ssize_t log_level_store(struct class *cla,
			       struct class_attribute *attr,
			       const char *buf, size_t count)
{
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	ge2d_log_info("ge2d log_level: %d->%d\n", ge2d_log_level, res);
	ge2d_log_level = res;

	return count;
}

static int ge2d_open(struct inode *inode, struct file *file)
{
	struct ge2d_context_s *context = NULL;

	/* we create one ge2d workqueue for this file handler. */
	context = create_ge2d_work_queue();
	if (!context) {
		ge2d_log_err("can't create work queue\n");
		return -1;
	}
	file->private_data = context;
	ge2d_device.open_count++;

	return 0;
}

static long ge2d_ioctl(struct file *filp, unsigned int cmd, unsigned long args)
{
	struct ge2d_context_s *context = NULL;
	struct config_para_s ge2d_config;
	struct ge2d_para_s para;
	struct config_para_ex_s ge2d_config_ex;
	int ret = 0;
#ifdef CONFIG_COMPAT
	struct compat_config_para_s __user *uf;
	struct compat_config_para_ex_s __user *uf_ex;
	int r = 0;
	int i, j;
#endif
	void __user *argp = (void __user *)args;

	context = (struct ge2d_context_s *)filp->private_data;
	memset(&ge2d_config, 0, sizeof(struct config_para_s));
	memset(&ge2d_config_ex, 0, sizeof(struct config_para_ex_s));
	switch (cmd) {
	case GE2D_CONFIG:
	case GE2D_SRCCOLORKEY:
		ret = copy_from_user(&ge2d_config,
				argp, sizeof(struct config_para_s));
		break;
#ifdef CONFIG_COMPAT
	case GE2D_SRCCOLORKEY32:
		uf = (struct compat_config_para_s *)argp;
		r = get_user(ge2d_config.src_dst_type, &uf->src_dst_type);
		r |= get_user(ge2d_config.alu_const_color,
			&uf->alu_const_color);
		r |= get_user(ge2d_config.src_format, &uf->src_format);
		r |= get_user(ge2d_config.dst_format, &uf->dst_format);
		for (i = 0; i < 4; i++) {
			r |= get_user(ge2d_config.src_planes[i].addr,
				&uf->src_planes[i].addr);
			r |= get_user(ge2d_config.src_planes[i].w,
				&uf->src_planes[i].w);
			r |= get_user(ge2d_config.src_planes[i].h,
				&uf->src_planes[i].h);
		}
		for (j = 0; j < 4; j++) {
			r |= get_user(ge2d_config.dst_planes[j].addr,
				&uf->dst_planes[j].addr);
			r |= get_user(ge2d_config.dst_planes[j].w,
				&uf->dst_planes[j].w);
			r |= get_user(ge2d_config.dst_planes[j].h,
				&uf->dst_planes[j].h);
		}
		r |= copy_from_user(&ge2d_config.src_key, &uf->src_key,
			sizeof(struct src_key_ctrl_s));
		if (r) {
			pr_err("GE2D_SRCCOLORKEY32 get parameter failed .\n");
			return -EFAULT;
			}
		break;
#endif
#ifdef CONFIG_COMPAT
	case GE2D_CONFIG32:
		uf = (struct compat_config_para_s *)argp;
		r = get_user(ge2d_config.src_dst_type, &uf->src_dst_type);
		r |= get_user(ge2d_config.alu_const_color,
			&uf->alu_const_color);
		r |= get_user(ge2d_config.src_format, &uf->src_format);
		r |= get_user(ge2d_config.dst_format, &uf->dst_format);
		for (i = 0; i < 4; i++) {
			r |= get_user(ge2d_config.src_planes[i].addr,
				&uf->src_planes[i].addr);
			r |= get_user(ge2d_config.src_planes[i].w,
				&uf->src_planes[i].w);
			r |= get_user(ge2d_config.src_planes[i].h,
				&uf->src_planes[i].h);
		}
		for (j = 0; j < 4; j++) {
			r |= get_user(ge2d_config.dst_planes[j].addr,
				&uf->dst_planes[j].addr);
			r |= get_user(ge2d_config.dst_planes[j].w,
				&uf->dst_planes[j].w);
			r |= get_user(ge2d_config.dst_planes[j].h,
				&uf->dst_planes[j].h);
		}
		r |= copy_from_user(&ge2d_config.src_key, &uf->src_key,
			sizeof(struct src_key_ctrl_s));
		if (r) {
			pr_err("GE2D_CONFIG32 get parameter failed .\n");
			return -EFAULT;
		}

		break;
#endif
	case GE2D_CONFIG_EX:
		ret = copy_from_user(&ge2d_config_ex, argp,
				sizeof(struct config_para_ex_s));
		break;
#ifdef CONFIG_COMPAT
	case GE2D_CONFIG_EX32:
		uf_ex = (struct compat_config_para_ex_s *)argp;
		r = copy_from_user(&ge2d_config_ex.src_para, &uf_ex->src_para,
			sizeof(struct src_dst_para_ex_s));
		r |= copy_from_user(&ge2d_config_ex.src2_para,
			&uf_ex->src2_para,
			sizeof(struct src_dst_para_ex_s));
		r |= copy_from_user(&ge2d_config_ex.dst_para, &uf_ex->dst_para,
			sizeof(struct src_dst_para_ex_s));

		r |= copy_from_user(&ge2d_config_ex.src_key, &uf_ex->src_key,
			sizeof(struct src_key_ctrl_s));
		r |= copy_from_user(&ge2d_config_ex.src2_key, &uf_ex->src2_key,
			sizeof(struct src_key_ctrl_s));

		r |= get_user(ge2d_config_ex.alu_const_color,
			&uf_ex->alu_const_color);
		r |= get_user(ge2d_config_ex.src1_gb_alpha,
			&uf_ex->src1_gb_alpha);
		r |= get_user(ge2d_config_ex.op_mode, &uf_ex->op_mode);
		r |= get_user(ge2d_config_ex.bitmask_en, &uf_ex->bitmask_en);
		r |= get_user(ge2d_config_ex.bytemask_only,
			&uf_ex->bytemask_only);
		r |= get_user(ge2d_config_ex.bitmask, &uf_ex->bitmask);
		r |= get_user(ge2d_config_ex.dst_xy_swap, &uf_ex->dst_xy_swap);
		r |= get_user(ge2d_config_ex.hf_init_phase,
			&uf_ex->hf_init_phase);
		r |= get_user(ge2d_config_ex.hf_rpt_num, &uf_ex->hf_rpt_num);
		r |= get_user(ge2d_config_ex.hsc_start_phase_step,
			&uf_ex->hsc_start_phase_step);
		r |= get_user(ge2d_config_ex.hsc_phase_slope,
			&uf_ex->hsc_phase_slope);
		r |= get_user(ge2d_config_ex.vf_init_phase,
			&uf_ex->vf_init_phase);
		r |= get_user(ge2d_config_ex.vf_rpt_num, &uf_ex->vf_rpt_num);
		r |= get_user(ge2d_config_ex.vsc_start_phase_step,
			&uf_ex->vsc_start_phase_step);
		r |= get_user(ge2d_config_ex.vsc_phase_slope,
			&uf_ex->vsc_phase_slope);
		r |= get_user(ge2d_config_ex.src1_vsc_phase0_always_en,
			&uf_ex->src1_vsc_phase0_always_en);
		r |= get_user(ge2d_config_ex.src1_hsc_phase0_always_en,
			&uf_ex->src1_hsc_phase0_always_en);
		r |= get_user(ge2d_config_ex.src1_hsc_rpt_ctrl,
			&uf_ex->src1_hsc_rpt_ctrl);
		r |= get_user(ge2d_config_ex.src1_vsc_rpt_ctrl,
			&uf_ex->src1_vsc_rpt_ctrl);

		for (i = 0; i < 4; i++) {
			r |= get_user(ge2d_config_ex.src_planes[i].addr,
				&uf_ex->src_planes[i].addr);
			r |= get_user(ge2d_config_ex.src_planes[i].w,
				&uf_ex->src_planes[i].w);
			r |= get_user(ge2d_config_ex.src_planes[i].h,
				&uf_ex->src_planes[i].h);
		}

		for (i = 0; i < 4; i++) {
			r |= get_user(ge2d_config_ex.src2_planes[i].addr,
				&uf_ex->src2_planes[i].addr);
			r |= get_user(ge2d_config_ex.src2_planes[i].w,
				&uf_ex->src2_planes[i].w);
			r |= get_user(ge2d_config_ex.src2_planes[i].h,
				&uf_ex->src2_planes[i].h);
		}

		for (j = 0; j < 4; j++) {
			r |= get_user(ge2d_config_ex.dst_planes[j].addr,
				&uf_ex->dst_planes[j].addr);
			r |= get_user(ge2d_config_ex.dst_planes[j].w,
				&uf_ex->dst_planes[j].w);
			r |= get_user(ge2d_config_ex.dst_planes[j].h,
				&uf_ex->dst_planes[j].h);
		}

		if (r) {
			pr_err("GE2D_CONFIG_EX32 get parameter failed .\n");
			return -EFAULT;
			}
		break;
#endif
	case GE2D_SET_COEF:
	case GE2D_ANTIFLICKER_ENABLE:
		break;
	case GE2D_CONFIG_OLD:
	case GE2D_CONFIG_EX_OLD:
	case GE2D_SRCCOLORKEY_OLD:
		pr_err("ioctl not support yed.\n");
		return -EINVAL;
	default:
		ret = copy_from_user(&para, argp, sizeof(struct ge2d_para_s));
		break;

	}

	switch (cmd) {
	case GE2D_CONFIG:
#ifdef CONFIG_COMPAT
	case GE2D_CONFIG32:
#endif
		ret = ge2d_context_config(context, &ge2d_config);
		break;
	case GE2D_CONFIG_EX:
#ifdef CONFIG_COMPAT
	case GE2D_CONFIG_EX32:
#endif
		ret = ge2d_context_config_ex(context, &ge2d_config_ex);
		break;
	case GE2D_SET_COEF:
		ge2d_wq_set_scale_coef(context, args & 0xff, args >> 16);
		break;
	case GE2D_ANTIFLICKER_ENABLE:
		ge2d_antiflicker_enable(context, args);
		break;
	case GE2D_SRCCOLORKEY:
#ifdef CONFIG_COMPAT
	case GE2D_SRCCOLORKEY32:
#endif
		ge2d_log_dbg("src colorkey...,key_enable=0x%x,key_color=0x%x,key_mask=0x%x,key_mode=0x%x\n",
			     ge2d_config.src_key.key_enable,
			     ge2d_config.src_key.key_color,
			     ge2d_config.src_key.key_mask,
			     ge2d_config.src_key.key_mode);

		ge2dgen_src_key(context, ge2d_config.src_key.key_enable,
				ge2d_config.src_key.key_color,
				ge2d_config.src_key.key_mask,
				ge2d_config.src_key.key_mode); /* RGBA MODE */
		break;
	case GE2D_FILLRECTANGLE:
		ge2d_log_dbg("fill rect...,x=%d,y=%d,w=%d,h=%d,color=0x%x\n",
			     para.src1_rect.x, para.src1_rect.y,
			     para.src1_rect.w, para.src1_rect.h,
			     para.color);

		fillrect(context,
			 para.src1_rect.x, para.src1_rect.y,
			 para.src1_rect.w, para.src1_rect.h,
			 para.color);
		break;
	case GE2D_FILLRECTANGLE_NOBLOCK:
		ge2d_log_dbg("fill rect noblk\t");
		ge2d_log_dbg("x=%d,y=%d,w=%d,h=%d,color=0x%x\n",
			     para.src1_rect.x, para.src1_rect.y,
			     para.src1_rect.w, para.src1_rect.h,
			     para.color);

		fillrect_noblk(context,
			       para.src1_rect.x, para.src1_rect.y,
			       para.src1_rect.w, para.src1_rect.h,
			       para.color);
		break;
	case GE2D_STRETCHBLIT:
		/* stretch blit */
		ge2d_log_dbg("stretchblt\t");
		ge2d_log_dbg("x=%d,y=%d,w=%d,h=%d,dst.w=%d,dst.h=%d\n",
			     para.src1_rect.x, para.src1_rect.y,
			     para.src1_rect.w, para.src1_rect.h,
			     para.dst_rect.w, para.dst_rect.h);

		stretchblt(context,
			   para.src1_rect.x, para.src1_rect.y,
			   para.src1_rect.w, para.src1_rect.h,
			   para.dst_rect.x,  para.dst_rect.y,
			   para.dst_rect.w,  para.dst_rect.h);
		break;
	case GE2D_STRETCHBLIT_NOBLOCK:
		/* stretch blit */
		ge2d_log_dbg("stretchblt noblk\t");
		ge2d_log_dbg("x=%d,y=%d,w=%d,h=%d,dst.w=%d,dst.h=%d\n",
			     para.src1_rect.x, para.src1_rect.y,
			     para.src1_rect.w, para.src1_rect.h,
			     para.dst_rect.w, para.dst_rect.h);

		stretchblt_noblk(context,
				 para.src1_rect.x, para.src1_rect.y,
				 para.src1_rect.w, para.src1_rect.h,
				 para.dst_rect.x,  para.dst_rect.y,
				 para.dst_rect.w,  para.dst_rect.h);
		break;
	case GE2D_BLIT:
		/* bitblt */
		ge2d_log_dbg("blit...\n");
		bitblt(context,
		       para.src1_rect.x, para.src1_rect.y,
		       para.src1_rect.w, para.src1_rect.h,
		       para.dst_rect.x, para.dst_rect.y);
		break;
	case GE2D_BLIT_NOBLOCK:
		/* bitblt */
		ge2d_log_dbg("blit...,noblk\n");
		bitblt_noblk(context,
			     para.src1_rect.x, para.src1_rect.y,
			     para.src1_rect.w, para.src1_rect.h,
			     para.dst_rect.x, para.dst_rect.y);
		break;
	case GE2D_BLEND:
		ge2d_log_dbg("blend ...\n");
		blend(context,
		      para.src1_rect.x, para.src1_rect.y,
		      para.src1_rect.w, para.src1_rect.h,
		      para.src2_rect.x, para.src2_rect.y,
		      para.src2_rect.w, para.src2_rect.h,
		      para.dst_rect.x, para.dst_rect.y,
		      para.dst_rect.w, para.dst_rect.h,
		      para.op);
		break;
	case GE2D_BLEND_NOBLOCK:
		ge2d_log_dbg("blend ...,noblk\n");
		blend_noblk(context,
			    para.src1_rect.x, para.src1_rect.y,
			    para.src1_rect.w, para.src1_rect.h,
			    para.src2_rect.x, para.src2_rect.y,
			    para.src2_rect.w, para.src2_rect.h,
			    para.dst_rect.x, para.dst_rect.y,
			    para.dst_rect.w, para.dst_rect.h,
			    para.op);
		break;
	case GE2D_BLEND_NOALPHA:
		ge2d_log_dbg("blend_noalpha ...\n");
		blend_noalpha(context,
			  para.src1_rect.x, para.src1_rect.y,
			  para.src1_rect.w, para.src1_rect.h,
			  para.src2_rect.x, para.src2_rect.y,
			  para.src2_rect.w, para.src2_rect.h,
			  para.dst_rect.x, para.dst_rect.y,
			  para.dst_rect.w, para.dst_rect.h,
			  para.op);
			break;
	case GE2D_BLEND_NOALPHA_NOBLOCK:
		ge2d_log_dbg("blend_noalpha ...,noblk\n");
		blend_noalpha_noblk(context,
				para.src1_rect.x, para.src1_rect.y,
				para.src1_rect.w, para.src1_rect.h,
				para.src2_rect.x, para.src2_rect.y,
				para.src2_rect.w, para.src2_rect.h,
				para.dst_rect.x, para.dst_rect.y,
				para.dst_rect.w, para.dst_rect.h,
				para.op);
		break;
	case GE2D_BLIT_NOALPHA:
		/* bitblt_noalpha */
		ge2d_log_dbg("blit_noalpha...\n");
		bitblt_noalpha(context,
			       para.src1_rect.x, para.src1_rect.y,
			       para.src1_rect.w, para.src1_rect.h,
			       para.dst_rect.x, para.dst_rect.y);
		break;
	case GE2D_BLIT_NOALPHA_NOBLOCK:
		/* bitblt_noalpha */
		ge2d_log_dbg("blit_noalpha...,noblk\n");
		bitblt_noalpha_noblk(context,
				     para.src1_rect.x, para.src1_rect.y,
				     para.src1_rect.w, para.src1_rect.h,
				     para.dst_rect.x, para.dst_rect.y);
		break;
	case GE2D_STRETCHBLIT_NOALPHA:
		/* stretch blit */
		ge2d_log_dbg("stretchblt_noalpha\t");
		ge2d_log_dbg("x=%d,y=%d,w=%d,h=%d,dst.w=%d,dst.h=%d\n",
			     para.src1_rect.x, para.src1_rect.y,
			     para.src1_rect.w, para.src1_rect.h,
			     para.dst_rect.w, para.dst_rect.h);

		stretchblt_noalpha(context,
				   para.src1_rect.x, para.src1_rect.y,
				   para.src1_rect.w, para.src1_rect.h,
				   para.dst_rect.x,  para.dst_rect.y,
				   para.dst_rect.w,  para.dst_rect.h);
		break;
	case GE2D_STRETCHBLIT_NOALPHA_NOBLOCK:
		/* stretch blit */
		ge2d_log_dbg("stretchblt_noalpha_noblk\t");
		ge2d_log_dbg("x=%d,y=%d,w=%d,h=%d,dst.w=%d,dst.h=%d\n",
			     para.src1_rect.x, para.src1_rect.y,
			     para.src1_rect.w, para.src1_rect.h,
			     para.dst_rect.w, para.dst_rect.h);

		stretchblt_noalpha_noblk(context,
					 para.src1_rect.x, para.src1_rect.y,
					 para.src1_rect.w, para.src1_rect.h,
					 para.dst_rect.x,  para.dst_rect.y,
					 para.dst_rect.w,  para.dst_rect.h);
		break;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long ge2d_compat_ioctl(struct file *filp,
			      unsigned int cmd, unsigned long args)
{
	unsigned long ret;

	args = (unsigned long)compat_ptr(args);
	ret = ge2d_ioctl(filp, cmd, args);

	return ret;
}
#endif

static int ge2d_release(struct inode *inode, struct file *file)
{
	struct ge2d_context_s *context = NULL;

	context = (struct ge2d_context_s *)file->private_data;
	if (context && (destroy_ge2d_work_queue(context) == 0)) {
		ge2d_device.open_count--;
		return 0;
	}
	ge2d_log_dbg("release one ge2d device\n");
	return -1;
}

static int ge2d_probe(struct platform_device *pdev)
{
	int ret = 0;
	int irq = 0;
	struct reset_control *rstc = NULL;
	struct clk *clk_gate;
	struct clk *clk;
	/* get interrupt resource */
	irq = platform_get_irq_byname(pdev, "ge2d");
	if (irq == -ENXIO) {
		ge2d_log_err("get ge2d irq resource error\n");
		ret = -ENXIO;
		goto failed1;
	}

	clk_gate = devm_clk_get(&pdev->dev, "clk_ge2d_gate");
	if (IS_ERR(clk_gate)) {
		ge2d_log_err("cannot get clock\n");
		clk_gate = NULL;
		ret = -ENOENT;
		goto failed1;
	}
	ge2d_log_info("clock source clk_ge2d_gate %p\n", clk_gate);
	clk_prepare_enable(clk_gate);

	clk = devm_clk_get(&pdev->dev, "clk_ge2d");
	if (IS_ERR(clk)) {
		ge2d_log_err("cannot get clock\n");
		clk = NULL;
		ret = -ENOENT;
		goto failed1;
	}
	ge2d_log_info("clock clk_ge2d source %p\n", clk);
	clk_prepare_enable(clk);

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXBB) {
		struct clk *clk_vapb0;
		int vapb_rate;

		clk_vapb0 = devm_clk_get(&pdev->dev, "clk_vapb_0");
		ge2d_log_info("clock source clk_vapb_0 %p\n", clk_vapb0);
		clk_prepare_enable(clk_vapb0);

		if (!IS_ERR(clk_vapb0)) {
			vapb_rate = clk_get_rate(clk_vapb0);
			clk_set_rate(clk_vapb0, vapb_rate);
			ge2d_log_info("ge2d clock is %d MHZ\n",
				vapb_rate/1000000);
		}
	}
	ret = ge2d_wq_init(pdev, irq, rstc, clk_gate);
failed1:
	return ret;
}

static int ge2d_remove(struct platform_device *pdev)
{
	ge2d_log_info("%s\n", __func__);
	ge2d_wq_deinit();

	return 0;
}

static const struct of_device_id ge2d_dt_match[] = {
	{.compatible = "amlogic, ge2d",},
	{},
};

static struct platform_driver ge2d_driver = {
	.probe      = ge2d_probe,
	.remove     = ge2d_remove,
	.driver     = {
		.name = "ge2d",
		.of_match_table = ge2d_dt_match,
	}
};

static int init_ge2d_device(void)
{
	int  ret = 0;

	strcpy(ge2d_device.name, "ge2d");
	ret = register_chrdev(0, ge2d_device.name, &ge2d_fops);
	if (ret <= 0) {
		ge2d_log_err("register ge2d device error\n");
		return  ret;
	}
	ge2d_device.major = ret;
	ge2d_device.dbg_enable = 0;
	ge2d_log_info("ge2d_dev major:%d\n", ret);
	ret = class_register(&ge2d_class);
	if (ret < 0) {
		ge2d_log_err("error create ge2d class\n");
		return ret;
	}
	ge2d_device.cla = &ge2d_class;
	ge2d_device.dev = device_create(ge2d_device.cla,
					NULL, MKDEV(ge2d_device.major,
					0), NULL, ge2d_device.name);
	if (IS_ERR(ge2d_device.dev)) {
		ge2d_log_err("create ge2d device error\n");
		class_unregister(ge2d_device.cla);
		return -1;
	}

	if (platform_driver_register(&ge2d_driver)) {
		ge2d_log_err("failed to register OSD driver!\n");
		return -ENODEV;
	}

	return ret;
}

static int remove_ge2d_device(void)
{
	if (!ge2d_device.cla)
		return 0;

	if (ge2d_device.dev)
		device_destroy(ge2d_device.cla, MKDEV(ge2d_device.major, 0));
	class_unregister(ge2d_device.cla);
	unregister_chrdev(ge2d_device.major, ge2d_device.name);

	return  0;
}

static int __init ge2d_init_module(void)
{
	ge2d_log_info("%s\n", __func__);
	return init_ge2d_device();
}

static void __exit ge2d_remove_module(void)
{
	platform_driver_unregister(&ge2d_driver);
	remove_ge2d_device();
	ge2d_log_info("%s\n", __func__);
}

module_init(ge2d_init_module);
module_exit(ge2d_remove_module);
