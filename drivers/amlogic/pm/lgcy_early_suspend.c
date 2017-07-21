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
static DEFINE_MUTEX(sysfs_trigger_lock);
static LIST_HEAD(early_suspend_handlers);

/* In order to handle legacy early_suspend driver,
 * here we export sysfs interface
 * for user space to write /sys/power/early_suspend_trigger to trigger
 * early_suspend/late resume call back. If user space do not trigger
 * early_suspend/late_resume, this op will be done
 * by PM_SUSPEND_PREPARE notify.
 */
unsigned int sysfs_trigger;
unsigned int early_suspend_state;
/*
 * Avoid run early_suspend/late_resume repeatly.
 */
unsigned int already_early_suspend;

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

static inline void early_suspend(void)
{
	struct early_suspend *pos;

	mutex_lock(&early_suspend_lock);

	if (!already_early_suspend)
		already_early_suspend = 1;
	else
		goto end_early_suspend;

	pr_info("early_suspend: call handlers\n");
	list_for_each_entry(pos, &early_suspend_handlers, link)
		if (pos->suspend != NULL) {
			pr_info("early_suspend: %pf\n", pos->suspend);
			pos->suspend(pos);
		}

	pr_info("early_suspend: done\n");

end_early_suspend:
	mutex_unlock(&early_suspend_lock);


}

static inline void late_resume(void)
{
	struct early_suspend *pos;

	mutex_lock(&early_suspend_lock);

	if (already_early_suspend)
		already_early_suspend = 0;
	else
		goto end_late_resume;

	pr_info("late_resume: call handlers\n");
	list_for_each_entry_reverse(pos, &early_suspend_handlers, link)
		if (pos->resume != NULL) {
			pr_info("late_resume: %pf\n", pos->resume);
			pos->resume(pos);
		}
	pr_info("late_resume: done\n");

end_late_resume:
	mutex_unlock(&early_suspend_lock);
}

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

	mutex_lock(&sysfs_trigger_lock);
	sysfs_trigger = 1;

	if (early_suspend_state == 0)
		late_resume();
	else if (early_suspend_state == 1)
		early_suspend();
	mutex_unlock(&sysfs_trigger_lock);

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

void lgcy_early_suspend(void)
{
	mutex_lock(&sysfs_trigger_lock);

	if (!sysfs_trigger)
		early_suspend();

	mutex_unlock(&sysfs_trigger_lock);
}

void lgcy_late_resume(void)
{
	mutex_lock(&sysfs_trigger_lock);

	if (!sysfs_trigger)
		late_resume();

	mutex_unlock(&sysfs_trigger_lock);
}


static int lgcy_early_suspend_notify(struct notifier_block *nb,
	unsigned long event, void *dummy)
{
	if (event == PM_SUSPEND_PREPARE)
		lgcy_early_suspend();

	if (event == PM_POST_SUSPEND)
		lgcy_late_resume();

	return NOTIFY_OK;
}

static struct notifier_block lgcy_early_suspend_notifier = {
	.notifier_call = lgcy_early_suspend_notify,
};

unsigned int lgcy_early_suspend_init(void)
{
	int ret;

	ret = sysfs_create_group(power_kobj, &attr_group);
	if (ret)
		return ret;

	ret = register_pm_notifier(&lgcy_early_suspend_notifier);
	return ret;
}
