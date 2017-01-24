/*
 * drivers/amlogic/media/common/ion_dev/meson_ion.h
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

#ifndef __LINUX_AMLOGIC_ION_H__
#define __LINUX_AMLOGIC_ION_H__

#include <linux/types.h>
#include <ion/ion.h>

/**
 * CUSTOM IOCTL - CMD
 */

#define ION_IOC_MESON_PHYS_ADDR             8

struct meson_phys_data {
	int handle;
	unsigned int phys_addr;
	unsigned int size;
};


/**
 * meson_ion_client_create() -  allocate a client and returns it
 * @heap_type_mask:	mask of heaps this client can allocate from
 * @name:		used for debugging
 */
struct ion_client *meson_ion_client_create(unsigned int heap_mask,
		const char *name);

/**
 * meson_ion_share_fd_to_phys -
 * associate with a share fd
 * @client:	the client
 * @share_fd: passed from the user space
 * @addr point to the physical address
 * @size point to the size of this ion buffer
 */

int meson_ion_share_fd_to_phys(struct ion_client *client,
		int share_fd, ion_phys_addr_t *addr, size_t *len);


int ion_phys(struct ion_client *client, struct ion_handle *handle,
	     ion_phys_addr_t *addr, size_t *len);

#endif
