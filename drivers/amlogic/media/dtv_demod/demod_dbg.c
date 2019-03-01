/*
 * drivers/amlogic/media/dtv_demod/demod_dbg.c
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

#include <linux/debugfs.h>
#include "demod_func.h"
#include "amlfrontend.h"
#include "demod_dbg.h"
#include <linux/string.h>

static void demod_dump_atsc_reg(struct seq_file *seq)
{
	unsigned int reg_start, reg_end;

	if (is_ic_ver(IC_VER_TXLX)) {
		reg_start = 0x0;
		reg_end = 0xfff;

		for (; reg_start <= reg_end; reg_start++) {
			if (reg_start % 8 == 0)
				seq_printf(seq, "\n[addr 0x%03x] ", reg_start);
			seq_printf(seq, "0x%02x\t", atsc_read_reg(reg_start));
		}

		seq_puts(seq, "\n");
	} else if (is_ic_ver(IC_VER_TL1)) {
	}
}

static int seq_file_demod_dump_reg_show(struct seq_file *seq, void *v)
{
	if (demod_get_current_mode() == AML_ATSC)
		demod_dump_atsc_reg(seq);
	else if (demod_get_current_mode() == UNKNOWN)
		seq_puts(seq, "current mode is unknown\n");

	return 0;
}

#define DEFINE_SHOW_DEMOD(__name) \
static int __name ## _open(struct inode *inode, struct file *file)	\
{ \
	return single_open(file, __name ## _show, inode->i_private);	\
} \
									\
static const struct file_operations __name ## _fops = {			\
	.owner = THIS_MODULE,		\
	.open = __name ## _open,	\
	.read = seq_read,		\
	.llseek = seq_lseek,		\
	.release = single_release,	\
}

DEFINE_SHOW_DEMOD(seq_file_demod_dump_reg);

static struct demod_debugfs_files_t demod_debugfs_files[] = {
	{"dump_reg", S_IFREG | 0644, &seq_file_demod_dump_reg_fops},
};

static int demod_dbg_dvbc_fast_search_open(struct inode *inode,
	struct file *file)
{
	PR_INFO("Demod debug Open\n");
	return 0;
}

static int demod_dbg_dvbc_fast_search_release(struct inode *inode,
	struct file *file)
{
	PR_INFO("Demod debug Release\n");
	return 0;
}

#define BUFFER_SIZE 100
static ssize_t demod_dbg_dvbc_fast_search_show(struct file *file,
	char __user *userbuf, size_t count, loff_t *ppos)
{
	char buf[BUFFER_SIZE];
	unsigned int len;

	len = snprintf(buf, BUFFER_SIZE, "channel fast search en : %d\n",
		demod_dvbc_get_fast_search());
	//len += snprintf(buf + len, BUFFER_SIZE - len, "");

	return simple_read_from_buffer(userbuf, count, ppos, buf, len);
}

static ssize_t demod_dbg_dvbc_fast_search_store(struct file *file,
		const char __user *userbuf, size_t count, loff_t *ppos)
{
	char buf[80];
	char cmd[80], para[80];
	int ret;

	count = min_t(size_t, count, (sizeof(buf)-1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;

	ret = sscanf(buf, "%s %s", cmd, para);

	if (!strcmp(cmd, "fast_search")) {
		PR_INFO("channel fast search: ");

		if (!strcmp(para, "on")) {
			PR_INFO("on\n");
			demod_dvbc_set_fast_search(1);
		} else if (!strcmp(para, "off")) {
			PR_INFO("off\n");
			demod_dvbc_set_fast_search(0);
		}
	}

	return count;
}

static const struct file_operations demod_dbg_dvbc_fast_search_fops = {
	.owner		= THIS_MODULE,
	.open		= demod_dbg_dvbc_fast_search_open,
	.release	= demod_dbg_dvbc_fast_search_release,
	//.unlocked_ioctl = aml_demod_ioctl,
	.read = demod_dbg_dvbc_fast_search_show,
	.write = demod_dbg_dvbc_fast_search_store,
};

void aml_demod_dbg_init(void)
{
	struct dentry *root_entry = dtvdd_devp->demod_root;
	struct dentry *entry;
	unsigned int i;

	PR_INFO("%s\n", __func__);

	root_entry = debugfs_create_dir("demod", NULL);
	if (!root_entry) {
		PR_INFO("Can't create debugfs dir frontend.\n");
		return;
	}

	for (i = 0; i < ARRAY_SIZE(demod_debugfs_files); i++) {
		entry = debugfs_create_file(demod_debugfs_files[i].name,
			demod_debugfs_files[i].mode,
			root_entry, NULL,
			demod_debugfs_files[i].fops);
		if (!entry)
			PR_INFO("Can't create debugfs seq file.\n");
	}

	entry = debugfs_create_file("dvbc_channel_fast", S_IFREG | 0644,
		root_entry, NULL,
		&demod_dbg_dvbc_fast_search_fops);
	if (!entry)
		PR_INFO("Can't create debugfs fast search.\n");

}

void aml_demod_dbg_exit(void)
{
	struct dentry *root_entry = dtvdd_devp->demod_root;

	if (dtvdd_devp && root_entry)
		debugfs_remove_recursive(root_entry);
}


