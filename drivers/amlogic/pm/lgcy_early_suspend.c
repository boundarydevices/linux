/*
 * drivers/amlogic/pm/lgcy_early_suspend.c
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

#include <linux/pm.h>
#include <linux/suspend.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/amlogic/iomap.h>
#include <linux/init.h>
#include <linux/of.h>
#include <asm/compiler.h>
#include <linux/errno.h>
#include <linux/suspend.h>
#include <asm/suspend.h>
#include <linux/input.h>
#include <uapi/linux/psci.h>
#include <linux/arm-smccc.h>
#include <linux/amlogic/pm.h>
#include <linux/kobject.h>
#include <../kernel/power/power.h>

static DEFINE_MUTEX(early_suspend_lock);
static LIST_HEAD(early_suspend_handlers);

void register_early_suspend(struct early_suspend *handler)
{
	struct list_head *pos;

	mutex_lock(&early_suspend_lock);
	list_for_each(pos, &early_suspend_handlers) {
		struct early_suspend *e;

		e = list_entry(pos, struct early_suspend, link);
		if (e->level > handler->level)
			break;
	}
	list_add_tail(&handler->link, pos);
	mutex_unlock(&early_suspend_lock);
}
EXPORT_SYMBOL(register_early_suspend);

void unregister_early_suspend(struct early_suspend *handler)
{
	mutex_lock(&early_suspend_lock);
	list_del(&handler->link);
	mutex_unlock(&early_suspend_lock);
}
EXPORT_SYMBOL(unregister_early_suspend);

static void early_suspend(void)
{
	struct early_suspend *pos;

	mutex_lock(&early_suspend_lock);

	pr_info("early_suspend: call handlers\n");
	list_for_each_entry(pos, &early_suspend_handlers, link) {
		if (pos->suspend != NULL) {
			pr_info("early_suspend: %pf\n", pos->suspend);
			pos->suspend(pos);
		}
	}
	mutex_unlock(&early_suspend_lock);

	pr_info("early_suspend: done\n");

}

static void late_resume(void)
{
	struct early_suspend *pos;

	mutex_lock(&early_suspend_lock);

	pr_info("late_resume: call handlers\n");
	list_for_each_entry_reverse(pos, &early_suspend_handlers, link)
		if (pos->resume != NULL) {
			pr_info("late_resume: %pf\n", pos->resume);
			pos->resume(pos);
		}
	pr_info("late_resume: done\n");

	mutex_unlock(&early_suspend_lock);
}

unsigned int early_suspend_state;
static ssize_t early_suspend_trigger_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf)
{
	unsigned int len;

	len = sprintf(buf, "%d\n", early_suspend_state);

	return len;
}
static ssize_t early_suspend_trigger_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf, size_t n)
{
	int ret;

	ret = kstrtouint(buf, 0, &early_suspend_state);
	pr_info("early_suspend_state=%d\n", early_suspend_state);

	if (ret)
		return -EINVAL;

	if (early_suspend_state == 0)
		late_resume();
	else if (early_suspend_state == 1)
		early_suspend();

	return n;
}

power_attr(early_suspend_trigger);
static struct attribute *g[] = {
	&early_suspend_trigger_attr.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = g,
};

unsigned int create_early_suspend_sysfs(void)
{
	if (sysfs_create_group(power_kobj, &attr_group))
		return -1;

	return 0;
}

