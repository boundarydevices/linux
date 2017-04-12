/*
 * drivers/amlogic/cpu_hotplug/cpu_hotplug.c
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
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/jiffies.h>
#include <linux/kernel_stat.h>
#include <linux/mutex.h>
#include <linux/hrtimer.h>
#include <linux/tick.h>
#include <linux/ktime.h>
#include <linux/sched.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/sched/rt.h>
#include <linux/notifier.h>
#include "linux/amlogic/cpu_hotplug.h"

#define MAX_CLUSTRS		2
struct cpu_hotplug_s {
	int clusters;
	unsigned int flgs[MAX_CLUSTRS];
	unsigned int max_num[MAX_CLUSTRS];
	unsigned int gov_num[MAX_CLUSTRS];
	cpumask_t cpumask[MAX_CLUSTRS];
	unsigned int cpunum[MAX_CLUSTRS];
	unsigned int min_num[MAX_CLUSTRS];
	struct task_struct *hotplug_thread;
	struct task_struct *null_thread[MAX_CLUSTRS];
	struct mutex mutex;
};

static struct cpu_hotplug_s hpg;

int cpu_hotplug_cpumask_init(void)
{
	int cpu, clstr;

	hpg.clusters = 0;
	for (clstr = 0; clstr < MAX_CLUSTRS; clstr++) {
		hpg.cpunum[clstr] = 0;
		cpumask_clear(&hpg.cpumask[clstr]);
	}

	for (cpu = 0; cpu < num_possible_cpus(); cpu++)	{
		clstr = topology_physical_package_id(cpu);
		if (clstr < 0)
			continue;
		clstr &= (MAX_CLUSTRS - 1);
		cpumask_set_cpu(cpu, &hpg.cpumask[clstr]);
		if (hpg.clusters < clstr + 1)
			hpg.clusters = clstr + 1;
		hpg.cpunum[clstr]++;
	}
	return 0;
}


unsigned int cpu_hotplug_get_max(unsigned int clustr)
{
	if (clustr >= hpg.clusters)
		return 0;
	return hpg.max_num[clustr];
}
unsigned int cpu_hotplug_get_min(int clustr)
{
	return hpg.min_num[clustr];
}

int cpu_num_online(int clustr)
{
	cpumask_t mask;

	cpumask_and(&mask, &hpg.cpumask[clustr], cpu_online_mask);
	return cpumask_weight(&mask);
}

void cpu_hotplug_set_max(unsigned int num, int clustr)
{
	unsigned int cpu_online;

	if (!num || clustr > hpg.clusters) {
		dev_err(NULL, " %s <:%d %d>\n", __func__, num, clustr);
		return;
	}

	mutex_lock(&hpg.mutex);
	if (num > hpg.cpunum[clustr])
		num = hpg.cpunum[clustr];
	if (hpg.max_num[clustr] == num) {
		mutex_unlock(&hpg.mutex);
		return;
	}

	cpu_online = cpu_num_online(clustr);
	hpg.max_num[clustr] = num;
	if (num < cpu_online) {
		hpg.flgs[clustr] = CPU_HOTPLUG_UNPLUG;
		if (hpg.hotplug_thread)
			wake_up_process(hpg.hotplug_thread);
	} else if (num > cpu_online) {
		if (cpu_online < hpg.gov_num[clustr]) {
			hpg.flgs[clustr] = CPU_HOTPLUG_PLUG;
			if (hpg.hotplug_thread)
				wake_up_process(hpg.hotplug_thread);
		}
	}
	mutex_unlock(&hpg.mutex);
}

void cpufreq_set_max_cpu_num(unsigned int n, unsigned int c)
{
	cpu_hotplug_set_max(n, c);
}

int cpu_hotplug_gov(int clustr, int num, int flg, cpumask_t *mask)
{
	if (!num || clustr > hpg.clusters
		|| hpg.gov_num[clustr] == num) {
		return -1;
	}
	mutex_lock(&hpg.mutex);
	if (num > hpg.cpunum[clustr])
		num = hpg.cpunum[clustr];
	hpg.gov_num[clustr] = num;
	hpg.flgs[clustr] = flg;
	if (hpg.hotplug_thread)
		wake_up_process(hpg.hotplug_thread);
	mutex_unlock(&hpg.mutex);
	return 0;
}

static int __ref cpu_hotplug_thread(void *data)
{
	unsigned int clustr, cpu, flg, online;
	int target, cnt;
	unsigned long flags;

	while (1) {
		if (kthread_should_stop())
			break;
		mutex_lock(&hpg.mutex);
		for (clustr = 0; clustr < hpg.clusters; clustr++) {
			if (!hpg.flgs[clustr])
				continue;
			flg = hpg.flgs[clustr];
			hpg.flgs[clustr] = 0;
			if (flg == CPU_HOTPLUG_PLUG) {
				for_each_cpu(cpu, &hpg.cpumask[clustr]) {
					if (cpu_online(cpu))
						continue;
					online = cpu_num_online(clustr);
					if (online >= hpg.gov_num[clustr] ||
						online >= hpg.max_num[clustr])
						break;
					device_online(get_cpu_device(cpu));
					cpumask_set_cpu(cpu,
					&hpg.null_thread[clustr]->cpus_allowed);
				}
			} else if (flg == CPU_HOTPLUG_UNPLUG) {
				cnt = 0;
				while ((online = cpu_num_online(clustr)) > 1) {
					if (online <= hpg.gov_num[clustr] &&
						online <= hpg.max_num[clustr])
						break;
					if (cnt++ > 20)
						break;
					raw_spin_lock_irqsave(
					&hpg.null_thread[clustr]->pi_lock,
					flags);
					target = cpumask_next(
					cpumask_first(&hpg.cpumask[clustr]),
					&hpg.cpumask[clustr]);
					target = select_cpu_for_hotplug(
						hpg.null_thread[clustr],
						target, SD_BALANCE_WAKE, 0);
					raw_spin_unlock_irqrestore(
					&hpg.null_thread[clustr]->pi_lock,
					flags);
					if (!cpumask_test_cpu(target,
						&hpg.cpumask[clustr])) {
						goto clear_cpu;
					}
					if (!cpu_online(target) ||
					cpumask_first(hpg.cpumask) == target)
						goto clear_cpu;
					device_offline(get_cpu_device(target));
clear_cpu:
					cpumask_clear_cpu(target,
					&hpg.null_thread[clustr]->cpus_allowed);
				}
			}
		}
		mutex_unlock(&hpg.mutex);
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
		set_current_state(TASK_RUNNING);
	}
	return 1;
}

static int do_null_task(void *data)
{
	while (1) {
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
		set_current_state(TASK_RUNNING);
	}
	return 1;
}

static ssize_t show_hotplug_max_cpus(struct kobject *kobj,
	struct attribute *attr, char *buf)
{
	unsigned int max = 0;
	unsigned int c = 0;

	for (c = 0; c < hpg.clusters; c++)
		max |= cpu_hotplug_get_max(c) << (c * 8);
	return sprintf(buf, "%u\n", max);
}

static ssize_t store_hotplug_max_cpus(struct kobject *kobj,
	struct attribute *attr, const char *buf, size_t count)
{
	unsigned int input;
	unsigned int max;
	unsigned int c = 0;
	int ret;

	ret = kstrtouint(buf, 0, &input);

	for (c = 0; c < hpg.clusters; c++)	{
		max = input & 0xff;
		if (max)
			cpu_hotplug_set_max(max, c);
		input = input >> 8;
	}
	return count;
}
define_one_global_rw(hotplug_max_cpus);


static int __init cpu_hotplug_init(void)
{
	int  clstr;
	int err;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO-1 };

	mutex_init(&hpg.mutex);
	mutex_lock(&hpg.mutex);
	cpu_hotplug_cpumask_init();
	for (clstr = 0; clstr < hpg.clusters; clstr++) {
		hpg.null_thread[clstr] = kthread_create(do_null_task,
			NULL, "null");
		if (!hpg.null_thread[clstr]) {
			hpg.null_thread[clstr] = NULL;
			return -1;
		}
		cpumask_copy(&hpg.null_thread[clstr]->cpus_allowed,
			&hpg.cpumask[clstr]);
		cpumask_clear_cpu(cpumask_first(&hpg.cpumask[clstr]),
			&hpg.null_thread[clstr]->cpus_allowed);

		wake_up_process(hpg.null_thread[clstr]);
		hpg.max_num[clstr] = hpg.cpunum[clstr];
		hpg.gov_num[clstr] = hpg.cpunum[clstr];
		hpg.flgs[clstr] = CPU_HOTPLUG_NONE;
		hpg.min_num[clstr] = 1;
	}


	hpg.hotplug_thread = kthread_create(cpu_hotplug_thread,
		NULL, "cpu_hogplug_thread");
	if (IS_ERR(hpg.hotplug_thread)) {
		err = PTR_ERR(hpg.hotplug_thread);
		hpg.hotplug_thread = NULL;
		mutex_unlock(&hpg.mutex);
		return err;
	}

	sched_setscheduler_nocheck(hpg.hotplug_thread, SCHED_FIFO, &param);
	get_task_struct(hpg.hotplug_thread);
	mutex_unlock(&hpg.mutex);
	wake_up_process(hpg.hotplug_thread);

	err = sysfs_create_file(&cpu_subsys.dev_root->kobj,
		&hotplug_max_cpus.attr);

	return 0;
}

static void __exit cpu_hotplug_exit(void)
{
	unsigned int c = 0;

	for (c = 0; c < hpg.clusters; c++)
	kthread_stop(hpg.null_thread[c]);
	kthread_stop(hpg.hotplug_thread);
	put_task_struct(hpg.hotplug_thread);
}

MODULE_DESCRIPTION("amlogic cpu hotplug");
MODULE_LICENSE("GPL v2");

module_init(cpu_hotplug_init);
module_exit(cpu_hotplug_exit);

