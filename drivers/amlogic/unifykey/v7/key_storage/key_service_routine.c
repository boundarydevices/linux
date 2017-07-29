/*
 * drivers/amlogic/unifykey/v7/key_storage/key_service_routine.c
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
#include <linux/amlogic/unifykey/v7/key_service_routine.h>
#include "storage.h"


uint64_t storage_service_routine(uint32_t fid,
			     uint64_t x1, void *in, void **out)
{
	uint64_t ret = 0;

	switch (fid) {
	case GET_SHARE_STORAGE_BLOCK_BASE:
		ret = get_share_storage_block_base();
		break;
	case FREE_SHARE_STORAGE:
		free_share_storage();
		break;
	case GET_SHARE_STORAGE_BLOCK_SIZE:
		ret = get_share_storage_block_size();
		break;
	case SECURITY_KEY_SET_ENCTYPE:
		ret = (uint64_t)storage_set_enctype(x1);
		break;
	case SECURITY_KEY_NOTIFY_EX:
		storage_api_notify_ex(x1);
		break;
	case SECURITY_KEY_STORAGE_TYPE:
		storage_api_storage_type(x1);
		break;
	case SECURITY_KEY_QUERY:
		ret = storage_api_query(in, out);
		break;
	case SECURITY_KEY_VERIFY:
		ret = storage_api_verify(in, out);
		break;
	case SECURITY_KEY_STATUS:
		ret = storage_api_status(in, out);
		break;
	case SECURITY_KEY_WRITE:
		ret = storage_api_write(in);
		break;
	case SECURITY_KEY_READ:
		ret = storage_api_read(in, out);
		break;
	case SECURITY_KEY_TELL:
		ret = storage_api_tell(in, out);
		break;
	default:
		break;
	}

	return ret;
}

