/*
 * include/linux/amlogic/power_ctrl.h
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

#ifndef _POWER_CTRL_H_
#define _POWER_CTRL_H_
#include <linux/types.h>

int power_ctrl_sleep(bool power_on, unsigned int shift);
int power_ctrl_iso(bool power_on, unsigned int shift);
int power_ctrl_mempd0(bool power_on, unsigned int mask_val, unsigned int shift);
#endif /*_POWER_CTRL_H_*/
