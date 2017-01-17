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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
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
#include <linux/amlogic/media/old_cpu_version.h>

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
	__ATTR_NULL
};

static struct class ge2d_class = {
	.name = GE2D_CLASS_NAME,
	.class_attrs = ge2d_class_attrs,
};


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

static bool command_valid(unsigned int cmd)
{
	bool ret = false;
#ifdef CONFIG_COMPAT
	ret = (cmd <= GE2D_CONFIG_EX32 &&
		cmd >= GE2D_ANTIFLICKER_ENABLE);
#else
	ret = (cmd <= GE2D_CONFIG_EX &&
		cmd >= GE2D_ANTIFLICKER_ENABLE);
#endif
	return ret;
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
	int r = 0;
	int i, j;
#endif
	void __user *argp = (void __user *)args;

	if (!command_valid(cmd))
		return -1;

	context = (struct ge2d_context_s *)filp->private_data;
	memset(&ge2d_config, 0, sizeof(struct config_para_s));
	switch (cmd) {
	case GE2D_CONFIG:
	case GE2D_SRCCOLORKEY:
		ret = copy_from_user(&ge2d_config,
				argp, sizeof(struct config_para_s));
		break;
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
	case GE2D_SET_COEF:
	case GE2D_ANTIFLICKER_ENABLE:
		break;
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
		ret = ge2d_context_config_ex(context, &ge2d_config_ex);
		break;
	case GE2D_SET_COEF:
		ge2d_wq_set_scale_coef(context, args & 0xff, args >> 16);
		break;
	case GE2D_ANTIFLICKER_ENABLE:
		ge2d_antiflicker_enable(context, args);
		break;
	case GE2D_SRCCOLORKEY:
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
	struct clk *clk;
	/* get interrupt resource */
	irq = platform_get_irq_byname(pdev, "ge2d");
	if (irq == -ENXIO) {
		ge2d_log_err("get ge2d irq resource error\n");
		ret = -ENXIO;
		goto failed1;
	}

	rstc = devm_reset_control_get(&pdev->dev, "ge2d");
	if (IS_ERR(rstc)) {
		ge2d_log_err("get ge2d rstc error: %lx\n", PTR_ERR(rstc));
		rstc = NULL;
		ret = -ENOENT;
		goto failed1;
	}

	reset_control_assert(rstc);
	clk = clk_get(&pdev->dev, "clk_ge2d");
	if (IS_ERR(clk)) {
		ge2d_log_err("cannot get clock\n");
		clk = NULL;
		ret = -ENOENT;
		goto failed1;
	}
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXBB) {
		struct clk *clk_vapb0;
		int vapb_rate;

		clk_vapb0 = clk_get(&pdev->dev, "clk_vapb_0");
		if (!IS_ERR(clk_vapb0)) {
			vapb_rate = clk_get_rate(clk_vapb0);
			clk_put(clk_vapb0);
			ge2d_log_info("ge2d clock is %d MHZ\n",
				vapb_rate/1000000);
		}
	}
	ret = ge2d_wq_init(pdev, irq, rstc, clk);
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
