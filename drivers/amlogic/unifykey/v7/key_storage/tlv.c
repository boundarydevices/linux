/*
 * drivers/amlogic/unifykey/v7/key_storage/tlv.c
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

#include <linux/string.h>
#include "storage.h"
#include "storage_util.h"

static uint32_t Tlv_WriteUint32(uint8_t *output, uint32_t tag, uint32_t value)
{
	uint8_t *out;

	out = output;
	*((uint32_t *)out) = tag;
	out += 4;
	*((uint32_t *)out) = 4;
	out += 4;
	*((uint32_t *)out) = value;
	return 12;
}

static uint32_t Tlv_WriteBuf(uint8_t *output, uint32_t tag,
					uint32_t length, uint8_t *input)
{
	uint8_t *out = output;
	*((uint32_t *)out) = tag;
	out += 4;
	*((uint32_t *)out) = (((length+3)/4)*4);
	out += 4;
	memset(out, 0, ((length+3)/4)*4);
	memcpy(out, input, length);
	return 8+(((length+3)/4)*4);
}

int32_t Tlv_ReadHead(uint8_t *buf, struct storage_block_enc_head *pblockhead)
{
	uint32_t tag;
	uint32_t sum;
	uint32_t length;
	uint8_t *input;

	input = buf;
	tag = *((uint32_t *)input);
	input += 4;
	sum =  *((uint32_t *)input);
	input += 4;
	if (tag != emTlvHead)
		return -1;

	while (sum > 0) {
		tag = *((unsigned int *)input);
		input += 4;
		length = *((unsigned int *)input);
		input += 4;
		sum -= 8;
		switch (tag) {
		case emTlvHeadSize:
			pblockhead->blocksize = *((uint32_t *)input);
			sum -= length;
			input += length;
			break;
		case emTlvHeadFlashSize:
			pblockhead->flashsize = *((uint32_t *)input);
			sum -= length;
			input += length;
			break;
		default:
			sum -= length;
			input += length;
			break;
		}
	}
	return 0;
}

int32_t Tlv_ReadObject(uint8_t *input,
				struct storage_node *node, uint32_t *allsize)
{
	uint8_t *buf;
	uint32_t tag;
	uint32_t length;
	uint32_t isum;
	struct storage_object *pcontent = &(node->object);

	buf = input;
	tag = *(unsigned int *)buf;
	buf += 4;
	isum = *(unsigned int *)buf;
	buf += 4;
	if (tag != emTlvObject)
		return -1;
	*allsize = isum+8;

	while (isum > 0) {
		tag = *(uint32_t *)buf;
		buf += 4;
		length = *(uint32_t *)buf;
		buf += 4;
		isum -= 8;
		switch (tag) {
		case emTlvObjNameSize:
			pcontent->namesize = *((uint32_t *)buf);
			buf += 4;
			isum -= 4;
			break;
		case emTlvObjName:
			memset(pcontent->name, 0, MAX_OBJ_NAME_LEN);
			memcpy(pcontent->name, buf, pcontent->namesize);
			buf += length;
			isum -= length;
			break;
		case emTlvObjType:
			pcontent->type = *((uint32_t *)buf);
			buf += 4;
			isum -= 4;
			break;
		case emTlvObjAttr:
			pcontent->attribute = *((uint32_t *)buf);
			buf += 4;
			isum -= 4;
			break;
		case emTlvObjDataSize:
			pcontent->datasize = *((uint32_t *)buf);
			buf += 4;
			isum -= 4;
			break;
		case emTlvObjHashBuf:
			if (length != 32)
				goto Tlv_ReadKeyContent_err;
			memcpy(pcontent->hashptr, buf, length);
			buf += length;
			isum -= length;
			break;
		case emTlvObjDataBuf:
			pcontent->dataptr =
					storage_malloc(pcontent->datasize);
			if (!pcontent->dataptr)
				goto Tlv_ReadKeyContent_err;
			memcpy(pcontent->dataptr,
						buf, pcontent->datasize);
			buf += length;
			isum -= length;
			break;
		default:
			buf += length;
			isum -= length;
			break;
		}
	}
	return 0;

Tlv_ReadKeyContent_err:
	node->status = OBJ_STATUS_INVALID;
	return -1;
}

void Tlv_WriteHead(struct storage_block_enc_head *enchead, uint8_t *output)
{
	uint8_t *buffer;
	uint32_t *sum;
	uint32_t length;

	buffer = output;
	*(uint32_t *)buffer = emTlvHead;
	buffer += 4;
	sum = (uint32_t *)buffer;
	buffer += 4;

	*sum = 0;

	length = Tlv_WriteUint32(buffer, emTlvHeadSize, enchead->blocksize);
	*sum += length;
	buffer += length;

	length = Tlv_WriteUint32(buffer, emTlvHeadFlashSize,
				 enchead->flashsize);
	*sum += length;
	buffer += length;
}

void Tlv_WriteObject(struct storage_object *object,
					uint8_t *output, uint32_t *outlen)
{
	uint32_t length;
	uint32_t *sum;
	uint8_t *buffer;
	uint32_t temp;

	buffer = output;
	*outlen = 0;
	*(uint32_t *)buffer = emTlvObject;
	buffer += 4;
	sum = (uint32_t *)buffer;
	buffer += 4;
	*outlen += 8;

	temp = strlen(object->name);
	if (temp != 0) {
		length = Tlv_WriteUint32(buffer, emTlvObjNameSize, temp);
		buffer += length;
		*outlen += length;

		length = Tlv_WriteBuf(buffer, emTlvObjName,
					temp, (uint8_t *)(object->name));
		buffer += length;
		*outlen += length;
	}

	if (object->dataptr && (object->datasize != 0)) {
		length = Tlv_WriteUint32(buffer,
					emTlvObjDataSize, object->datasize);
		buffer += length;
		*outlen += length;

		length = Tlv_WriteBuf(buffer, emTlvObjDataBuf,
					object->datasize, object->dataptr);
		buffer += length;
		*outlen += length;
	}

	length = Tlv_WriteBuf(buffer, emTlvObjHashBuf, 32, object->hashptr);
	buffer += length;
	*outlen += length;

	length = Tlv_WriteUint32(buffer, emTlvObjType, object->type);
	buffer += length;
	*outlen += length;

	length = Tlv_WriteUint32(buffer, emTlvObjAttr, object->attribute);
	buffer += length;
	*outlen += length;

	*sum = (*outlen-8);
}
