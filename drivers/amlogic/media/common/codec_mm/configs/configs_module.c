/*
 * drivers/amlogic/media/common/codec_mm/configs/configs_module.c
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/amlogic/media/codec_mm/configs.h>
#include <linux/amlogic/media/codec_mm/configs_api.h>
#define MAX_OPENED_CNT 65536
#include "configs_priv.h"
#define MODULE_NAME "media-configs-dev"

static struct class *config_dev_class;
static unsigned int config_major;
struct mediaconfig_node {
	const char *parent;
	const char *name;
	const char *dev_name;
	int rw_flags;
	atomic_t opened_cnt;
	struct device *class_dev;
	struct mconfig_node node;

};

static ssize_t all_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	ssize_t s;

	s = configs_list_nodes(NULL, buf, PAGE_SIZE,
		LIST_MODE_NODE_CMDVAL_ALL);
	if (s > 0)
		return s;
	return -EPERM;
}

static ssize_t media_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	ssize_t s;

	s = configs_list_path_nodes("media", buf, PAGE_SIZE,
		LIST_MODE_NODE_CMDVAL_ALL);
	if (s > 0)
		return s;
	return -EPERM;
}

static ssize_t video_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	ssize_t s;

	s = configs_list_path_nodes("media.video", buf, PAGE_SIZE,
		LIST_MODE_NODE_CMDVAL_ALL);
	if (s > 0)
		return s;
	return -EPERM;
}
static ssize_t decoder_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	ssize_t s;

	s = configs_list_path_nodes("media.decoder", buf, PAGE_SIZE,
		LIST_MODE_NODE_CMDVAL_ALL);
	if (s > 0)
		return s;
	return -EPERM;
}
static ssize_t vdec_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	ssize_t s;

	s = configs_list_path_nodes("media.vdec", buf, PAGE_SIZE,
		LIST_MODE_NODE_CMDVAL_ALL);
	if (s > 0)
		return s;
	return -EPERM;
}
static ssize_t tsync_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	ssize_t s;

	s = configs_list_path_nodes("media.tsync", buf, PAGE_SIZE,
		LIST_MODE_NODE_CMDVAL_ALL);
	if (s > 0)
		return s;
	return -EPERM;
}


static ssize_t amports_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	ssize_t s;

	s = configs_list_path_nodes("media.amports", buf, PAGE_SIZE,
		LIST_MODE_NODE_CMDVAL_ALL);
	if (s > 0)
		return s;
	return -EPERM;
}
static ssize_t parser_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	ssize_t s;

	s = configs_list_path_nodes("media.parser", buf, PAGE_SIZE,
		LIST_MODE_NODE_CMDVAL_ALL);
	if (s > 0)
		return s;
	return -EPERM;
}



static ssize_t show_config(struct class *class,
			struct class_attribute *attr, char *buf)
{
	ssize_t s;

	s = configs_list_nodes(NULL, buf, PAGE_SIZE,
		LIST_MODE_NODE_CMDVAL_ALL);
	if (s > 0)
		return s;
	return -EPERM;
}

static ssize_t store_config(struct class *class,
			struct class_attribute *attr,
			const char *buf, size_t size)
{
	ssize_t ret;

	ret = configs_set_path_valonpath(buf);
	if (ret >= 0)
		return size;
	return ret;
}

static ssize_t show_config_debug(struct class *class,
			struct class_attribute *attr, char *buf)
{
	return config_dump(buf, PAGE_SIZE);
}

static ssize_t store_config_debug(struct class *class,
			struct class_attribute *attr,
			const char *buf, size_t size)
{
	configs_config_setstr(buf);
	return size;
}

static ssize_t audio_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	ssize_t s;

	s = configs_list_path_nodes("media.audio", buf, PAGE_SIZE,
		LIST_MODE_NODE_CMDVAL_ALL);
	if (s > 0)
		return s;
	return -EPERM;
}
static ssize_t vfm_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	ssize_t s;

	s = configs_list_path_nodes("media.vfm", buf, PAGE_SIZE,
		LIST_MODE_NODE_CMDVAL_ALL);
	if (s > 0)
		return s;
	return -EPERM;
}

static struct class_attribute configs_class_attrs[] = {
	__ATTR_RO(all),
	__ATTR_RO(media),
	__ATTR_RO(video),
	__ATTR_RO(decoder),
	__ATTR_RO(amports),
	__ATTR_RO(tsync),
	__ATTR_RO(parser),
	__ATTR_RO(vdec),
	__ATTR_RO(audio),
	__ATTR_RO(vfm),
	__ATTR(config, 0664,
	show_config, store_config),
	__ATTR(debug, 0664,
	show_config_debug, store_config_debug),
	__ATTR_NULL
};

static struct class media_configs_class = {
		.name = "media-configs",
		.class_attrs = configs_class_attrs,
};

static struct mediaconfig_node mediaconfig_nodes[] = {
	{"", "media", "media", CONFIG_FOR_RW},
	{"media", "decoder", "media.decoder", CONFIG_FOR_RW},
	{"media", "parser", "media.parser", CONFIG_FOR_RW},
	{"media", "video", "media.video", CONFIG_FOR_RW},
	{"media", "amports", "media.amports", CONFIG_FOR_RW},
	{"media", "tsync", "media.tsync", CONFIG_FOR_RW},
	{"media", "codec_mm", "media.codec_mm", CONFIG_FOR_RW},
	{"media", "audio", "media.audio", CONFIG_FOR_RW},
	{"media", "vfm", "media.vfm", CONFIG_FOR_RW},
};

struct config_file_private {
	struct mediaconfig_node *node;
	char common[128];
	pid_t pid;
	pid_t tgid;
	long opened_jiffies;
	long last_access_jiffies;
	long access_get_cnt;
	long access_set_cnt;
	long access_dump_cnt;
	long error_cnt;
	int enable_trace_get;
	int enable_trace_set;
	int last_read_end;
};

static int configs_open(struct inode *inode, struct file *file)
{
	struct mediaconfig_node *node = &mediaconfig_nodes[iminor(inode)];
	struct config_file_private *priv;

	if (atomic_read(&node->opened_cnt) > MAX_OPENED_CNT) {
		pr_err("too many files opened.!!\n");
		return -EMFILE;
	}
	priv = kzalloc(sizeof(struct config_file_private), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	file->private_data = priv;
	priv->pid = current->pid;
	priv->tgid = current->tgid;
	priv->opened_jiffies = jiffies;
	priv->node = node;
	atomic_inc(&node->opened_cnt);
	configs_inc_node_ref(&node->node);
	return 0;
}
static ssize_t configs_read(struct file *file, char __user *buf,
			size_t count, loff_t *ppos)
{
	struct config_file_private *priv = file->private_data;
	struct mediaconfig_node *node = priv->node;
	int ret;

	if (*ppos > 0)
		return 0;/*don't support seek read. read end.*/
	if (!access_ok(VERIFY_WRITE, buf, count))
		return -EIO;
	ret = configs_list_nodes(&node->node, buf, count,
		LIST_MODE_NODE_CMDVAL_ALL);
	if (ret > 0)
		*ppos = ret;
	return ret;
}

static int configs_close(struct inode *inode, struct file *file)
{
	struct config_file_private *priv = file->private_data;
	struct mediaconfig_node *node = priv->node;

	configs_dec_node_ref(&node->node);
	atomic_dec(&node->opened_cnt);

	kfree(priv);

	return 0;
}
static long configs_ioctl(struct file *file, unsigned int cmd, ulong arg)
{
	struct config_file_private *priv = file->private_data;
	struct mediaconfig_node *node = priv->node;
	struct media_config_io_str io;
	struct media_config_io_str *user_io = (void *)arg;
	int r = -1;

	priv->last_access_jiffies = jiffies;
	switch (cmd) {
	case MEDIA_CONFIG_SET_CMD_STR:
		priv->access_set_cnt++;
		r = copy_from_user(io.cmd_path,
			user_io->cmd_path, sizeof(io.cmd_path));
		r |= copy_from_user(io.val, user_io->val, sizeof(io.val));
		/*pr_info("set%s:%s dev=%s\n", io.cmd_path,*/
		/*io.val, node->dev_name);*/
		if (r) {
			r = -EIO;
			break;
		}
		if (!strncmp(io.cmd_path, node->dev_name,
		strlen(node->dev_name)))
			r = configs_set_node_nodepath_str(NULL,
				io.cmd_path,
				io.val);
		else
		    pr_info("set %s %s  not match devname=%s\n",
		io.cmd_path, io.val, node->dev_name);
		break;
	case MEDIA_CONFIG_GET_CMD_STR:
		r = -1;
		priv->access_get_cnt++;
		r = copy_from_user(&io.cmd_path,
				user_io->cmd_path, sizeof(io.cmd_path));
		if (r) {
			r = -EIO;
			break;
		}
		io.val[0] = '\0';
		if (!strncmp(io.cmd_path, node->dev_name,
		strlen(node->dev_name))) {
			r = configs_get_node_nodepath_str(NULL,
				io.cmd_path,
				io.val,
				sizeof(io.val));

		/*pr_info("configs_get_node_nodepath_str*/
		/*ret %x [%s]\n", r, io.val);*/
		if (r > 0 && r <= sizeof(io.val) - 1) {
			/*+1 for end str.*/
			if (copy_to_user(user_io->val, io.val, r + 1) != 0) {
				r = -EIO;
				break;
				}
				put_user(r, &user_io->ret);
				r = 0;
			} else {
				 put_user(-1, &user_io->ret);
			}
		} else
			pr_info("set %s %s  not match devname=%s\n",
			io.cmd_path, io.val, node->dev_name);
		break;
	default:
		r = -EINVAL;
		pr_err("unsupport cmd %x\n", cmd);
	}
	if (r < 0)
		priv->error_cnt++;
	return r;
}



#ifdef CONFIG_COMPAT
static long configs_compat_ioctl(struct file *file,
		unsigned int cmd, ulong arg)
{
	return configs_ioctl(file, cmd, (ulong)compat_ptr(arg));
}
#endif
static const struct file_operations configs_fops = {
	.owner = THIS_MODULE,
	.open = configs_open,
	.read = configs_read,
	.release = configs_close,
	.unlocked_ioctl = configs_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = configs_compat_ioctl,
#endif
};


static int __init configs_init_devices(void)
{
	int i;
	int num = sizeof(mediaconfig_nodes)/sizeof(struct mediaconfig_node);

	int r;

	r = class_register(&media_configs_class);
	if (r) {
		pr_err("configs class create fail.\n");
		goto error1;
	}

	r = register_chrdev(0, MODULE_NAME, &configs_fops);
	if (r < 0) {
		pr_err("Can't allocate major for config device\n");
		goto error1;
	}
	config_major = r;

	config_dev_class = class_create(THIS_MODULE, MODULE_NAME);
	num = sizeof(mediaconfig_nodes)/sizeof(struct mediaconfig_node);
	for (i = 0; i < num; i++) {
		struct mediaconfig_node *mnode = &mediaconfig_nodes[i];

		mnode->class_dev = device_create(config_dev_class, NULL,
				MKDEV(config_major, i), NULL,
				mnode->dev_name);
		if (mnode->class_dev == NULL) {
			pr_err("device_create %s failed\n", mnode->dev_name);
			r = -1;
			goto error2;
		}
	}
	return 0;
#if 0
error3:
	for (mnode = &mediaconfig_nodes[0], i = 0; i < num; i++, mnode++)
		device_destroy(config_dev_class, MKDEV(config_major, i));
	class_destroy(config_dev_class);
#endif
error2:
	unregister_chrdev(config_major, MODULE_NAME);
error1:
	class_destroy(&media_configs_class);
	return r;
}
module_init(configs_init_devices);

static int __init media_configs_system_init(void)
{
	int i;
	int num = sizeof(mediaconfig_nodes)/sizeof(struct mediaconfig_node);
	struct mediaconfig_node *mnode;
	int r;

	pr_info("media_configs_system_init\n");
	configs_config_system_init();
	for (i = 0; i < num; i++) {
		mnode = &mediaconfig_nodes[i];
		configs_init_new_node(&mnode->node, mnode->name,
				mnode->rw_flags);
		r = configs_register_path_node(mnode->parent,
					&mnode->node);
		if (r < 0) {
			pr_err("ERR!!!.register node[%s] to [%s] failed!\n",
				mnode->name, mnode->parent);
			return r;
		}
	}

	return 0;
}

arch_initcall(media_configs_system_init);
MODULE_DESCRIPTION("AMLOGIC config modules driver");
MODULE_LICENSE("GPL");

