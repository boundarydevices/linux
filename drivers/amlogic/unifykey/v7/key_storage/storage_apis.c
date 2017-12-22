/*
 * drivers/amlogic/unifykey/v7/key_storage/storage_apis.c
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

#include "storage.h"
#include "storage_util.h"
#include "storage_def.h"

#undef pr_fmt
#define pr_fmt(fmt) "unifykey: " fmt


/* parameters from share key input memory*/
uint64_t storage_api_write(void *in)
{
	uint8_t *name, *buf;
	uint32_t namelen, bufsize, attr;
	uint32_t *input;

	input = (uint32_t *)in;
	namelen =  *input++;
	bufsize =  *input++;
	attr =  *input++;
	name = (uint8_t *)input;
	buf = name + namelen;
	return storage_write(name, namelen, buf, bufsize, attr);
}

uint64_t storage_api_read(void *in, void **out)
{
	uint32_t *input;
	uint8_t *name, *buf;
	uint32_t namelen, readsize;
	uint32_t outlen;
	uint32_t *outlen_addr;
	uint32_t ret;

	input = (uint32_t *)in;
	namelen =  *input++;
	readsize =  *input++;
	name = (uint8_t *)input;
	ret = storage_tell(name, namelen, &outlen);
	if (ret != RET_OK)
		return ret;

	*out = storage_malloc(outlen+sizeof(outlen));
	if (out == NULL)
		return RET_EMEM;

	outlen_addr = (uint32_t *)(*out);
	buf = (uint8_t *)(outlen_addr + 1);
	ret = storage_read(name, namelen, buf, readsize, outlen_addr);
	return ret;
}

uint64_t storage_api_query(void *in, void **out)
{
	uint32_t *input, *output;
	uint8_t *name;
	uint32_t namelen;

	input = (uint32_t *)in;
	*out = storage_malloc(sizeof(uint32_t));
	if (out == NULL)
		return RET_EMEM;

	output = (uint32_t *)(*out);
	namelen = *input++;
	name = (uint8_t *)input;
	return storage_query(name, namelen, output);
}

uint64_t storage_api_verify(void *in, void **out)
{
	uint32_t *input;
	uint8_t *name;
	uint32_t namelen;

	input = (uint32_t *)in;
	*out = storage_malloc(32);
	if (out == NULL)
		return RET_EMEM;

	namelen = *input++;
	name = (uint8_t *)input;

	return storage_verify(name, namelen, (uint8_t *)(*out));
}


uint64_t storage_api_tell(void *in, void **out)
{
	uint8_t *name;
	uint32_t namelen;
	uint32_t *input;
	uint32_t *output;


	input = (uint32_t *)in;
	*out = storage_malloc(sizeof(uint32_t));
	if (out == NULL)
		return RET_EMEM;

	namelen =  *input++;
	name = (uint8_t *)input;
	output = (uint32_t *)(*out);
	return storage_tell(name, namelen, output);
}


uint64_t storage_api_status(void *in, void **out)
{
	uint32_t *input;
	uint32_t *output;
	uint8_t *name;
	uint32_t namelen;

	input = (uint32_t *)in;
	*out = storage_malloc(sizeof(uint32_t));
	if (out == NULL)
		return RET_EMEM;

	output = (uint32_t *)(*out);
	namelen = *input++;
	name = (uint8_t *)input;

	return storage_status(name, namelen, output);
}

void storage_api_notify_ex(uint32_t flashsize)
{
	if (flashsize > get_share_storage_block_size()) {
		pr_err("flash size is too large!\n");
		return;
	}
	storage_init(flashsize);
}

void storage_api_storage_type(uint32_t is_emmc)
{
	storage_set_type(is_emmc);
}


