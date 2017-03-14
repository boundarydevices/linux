/*
 * include/linux/amlogic/cpucore_cooling.h
 *
 * Copyright (C) 2016 Amlogic, Inc. All rights reserved.
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

#ifndef __CPUCORE_COOLING_H__
#define __CPUCORE_COOLING_H__

#include <linux/thermal.h>
#include <linux/cpumask.h>
struct cpucore_cooling_device {
	int id;
	struct thermal_cooling_device *cool_dev;
	unsigned int cpucore_state;
	unsigned int cpucore_val;
	struct list_head node;
	int max_cpu_core_num;
	int cluster_id;
	int stop_flag;
};
#define CPU_STOP 0x80000000
#ifdef CONFIG_AMLOGIC_CPUCORE_THERMAL

/**
 * cpucore_cooling_register - function to create cpucore cooling device.
 * @clip_cpus: cpumask of cpus where the frequency constraints will happen
 */
struct thermal_cooling_device *cpucore_cooling_register(struct device_node *,
							int);

/**
 * cpucore_cooling_unregister - function to remove cpucore cooling device.
 * @cdev: thermal cooling device pointer.
 */
void cpucore_cooling_unregister(struct thermal_cooling_device *cdev);


#else
static inline struct thermal_cooling_device *
cpucore_cooling_register(struct device_node *np)
{
	return NULL;
}
static inline
void cpucore_cooling_unregister(struct thermal_cooling_device *cdev)
{
}
#endif

#endif /* __CPU_COOLING_H__ */
