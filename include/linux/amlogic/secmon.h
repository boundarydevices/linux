/*
 * include/linux/amlogic/secmon.h
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

#ifndef __SEC_MON_H__
#define __SEC_MON_H__

void __iomem *get_secmon_sharemem_input_base(void);
void __iomem *get_secmon_sharemem_output_base(void);
long get_secmon_phy_input_base(void);
long get_secmon_phy_output_base(void);

void sharemem_mutex_lock(void);
void sharemem_mutex_unlock(void);

#endif
