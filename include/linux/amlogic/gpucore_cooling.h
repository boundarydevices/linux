/*
 * include/linux/amlogic/gpucore_cooling.h
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

#ifndef __GPUCORE_COOLING_H__
#define __GPUCORE_COOLING_H__

#include <linux/thermal.h>
#include <linux/cpumask.h>
struct gpucore_cooling_device {
	int id;
	struct thermal_cooling_device *cool_dev;
	unsigned int gpucore_state;
	unsigned int gpucore_val;
	int max_gpu_core_num;
	unsigned int (*set_max_pp_num)(unsigned int pp);
	struct device_node *np;
	int stop_flag;
};
#define GPU_STOP 0x80000000

#ifdef CONFIG_AMLOGIC_GPUCORE_THERMAL

/**
 * gpucore_cooling_register - function to create gpucore cooling device.
 * @clip_cpus: cpumask of cpus where the frequency constraints will happen
 */
int gpucore_cooling_register(struct gpucore_cooling_device *gcd);

/**
 * gpucore_cooling_unregister - function to remove gpucore cooling device.
 * @cdev: thermal cooling device pointer.
 */
void gpucore_cooling_unregister(struct thermal_cooling_device *cdev);
struct gpucore_cooling_device *gpucore_cooling_alloc(void);
void save_gpucore_thermal_para(struct device_node *node);

#else
inline struct gpucore_cooling_device *gpucore_cooling_alloc(void)
{
	return NULL;
}

inline int gpucore_cooling_register(struct gpucore_cooling_device *gcd)
{
	return 0;
}
inline void gpucore_cooling_unregister(struct thermal_cooling_device *cdev)
{
}
inline void save_gpucore_thermal_para(struct device_node *n)
{
}
#endif

#endif /* __CPU_COOLING_H__ */
