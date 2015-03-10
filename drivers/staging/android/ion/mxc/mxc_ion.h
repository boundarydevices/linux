/*
 * drivers/gpu/mxc/mxc_ion.h
 *
 * Copyright (C) 2011 Google, Inc.
 * Copyright (C) 2012-2015 Freescale Semiconductor, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _LINUX_MXC_ION_H
#define _LINUX_MXC_ION_H

#include "../ion.h"

struct ion_phys_data {
	ion_user_handle_t handle;
	unsigned long phys;
};

#define ION_IOC_PHYS   _IOWR(ION_IOC_MAGIC, 7, struct ion_phys_data)

#endif
