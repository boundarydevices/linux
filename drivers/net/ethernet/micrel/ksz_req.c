/**
 * Microchip driver request common code
 *
 * Copyright (c) 2015-2016 Microchip Technology Inc.
 *	Tristram Ha <Tristram.Ha@microchip.com>
 *
 * Copyright (c) 2009-2014 Micrel, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#include "ksz_req.h"


#define PARAM_DATA_SIZE			80

static void get_user_data(int *kernel, int *user, void *info)
{
	if (info)
		__get_user(*kernel, user);
	else
		*kernel = *user;
}  /* get_user_data */

static void put_user_data(int *kernel, int *user, void *info)
{
	if (info)
		__put_user(*kernel, user);
	else
		*user = *kernel;
}  /* put_user_data */

static int read_user_data(void *kernel, void *user, size_t size, void *info)
{
	if (info) {
		if (!access_ok(VERIFY_READ, user, size) ||
		    copy_from_user(kernel, user, size))
			return -EFAULT;
	} else
		memcpy(kernel, user, size);
	return 0;
}  /* read_user_data */

static int write_user_data(void *kernel, void *user, size_t size, void *info)
{
	if (info) {
		if (!access_ok(VERIFY_WRITE, user, size) ||
		    copy_to_user(user, kernel, size))
			return -EFAULT;
	} else
		memcpy(user, kernel, size);
	return 0;
}  /* write_user_data */

static int _chk_ioctl_size(int len, int size, int additional, int *req_size,
	int *result, void *param, u8 *data, void *info)
{
	if (len < size) {
		printk(KERN_INFO "wrong size: %d %d\n", len, size);
		*req_size = size + additional;
		*result = DEV_IOC_INVALID_LEN;
		return -1;
	}
	if (size >= PARAM_DATA_SIZE) {
		printk(KERN_INFO "large size: %d\n", size);
		*result = -EFAULT;
		return -1;
	}
	if (data) {
		int err = read_user_data(data, param, size, info);

		if (err) {
			*result = -EFAULT;
			return -1;
		}
	}
	return 0;
}  /* _chk_ioctl_size */

static int chk_ioctl_size(int len, int size, int additional, int *req_size,
	int *result, void *param, u8 *data)
{
	return _chk_ioctl_size(len, size, additional, req_size, result, param,
		data, data);
	return 0;
}  /* chk_ioctl_size */

