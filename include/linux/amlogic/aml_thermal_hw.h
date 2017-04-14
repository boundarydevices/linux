/*
 * include/linux/amlogic/aml_thermal_hw.h
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

#ifndef ARCH__THERMAL_H__
#define ARCH__THERMAL_H__

#ifndef mc_capable
#define mc_capable()		0
#endif

struct thermal_cooling_device;
extern int thermal_firmware_init(void);
extern int get_cpu_temp(void);
extern int aml_thermal_min_update(struct thermal_cooling_device *cdev);
#endif
