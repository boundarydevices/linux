/*
 * include/linux/amlogic/cpu_hotplug.h
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

#ifndef __cpu_hotplug_h_
#define __cpu_hotplug_h_

#define CPU_HOTPLUG_NONE 0
#define CPU_HOTPLUG_PLUG 1
#define CPU_HOTPLUG_UNPLUG 2

#ifdef CONFIG_AMLOGIC_CPU_HOTPLUG
void cpufreq_set_max_cpu_num(unsigned int n, unsigned int c);
void cpu_hotplug_set_max(unsigned int cpu_num, int clustr);
unsigned int cpu_hotplug_get_max(unsigned int clustr);
unsigned int cpu_hotplug_get_min(int clustr);
int cpu_hotplug_gov(int clustr, int num, int flg, cpumask_t *mask);
int select_cpu_for_hotplug(struct task_struct *p,
				  int cpu, int sd_flags, int wake_flags);
#else
static inline void cpufreq_set_max_cpu_num(unsigned int n, unsigned int c)
{
}
static inline void cpu_hotplug_set_max(unsigned int cpu_num, int clustr)
{

}
static inline unsigned int cpu_hotplug_get_max(unsigned int clustr)
{
	return 0;
}
static inline unsigned int cpu_hotplug_get_min(int clustr)
{
	return 0;
}
static inline int cpu_hotplug_gov(int clustr, int num, int flg, cpumask_t *mask)
{
	return -1;
}
static inline int select_cpu_for_hotplug(struct task_struct *p,
				  int cpu, int sd_flags, int wake_flags)
{
	return -1;
}

#endif

#endif
