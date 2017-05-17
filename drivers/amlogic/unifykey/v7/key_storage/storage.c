/*
 * drivers/amlogic/unifykey/v7/key_storage/storage.c
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
#include "storage.h"
#include "storage_util.h"
#include "storage_data.h"
#include "storage_def.h"
#include "crypto_api.h"


static void storage_hash(uint8_t *input, uint32_t insize, uint8_t *output)
{
	sha256(input, insize, output);
}


static int32_t check_object_valid(struct storage_object *object)
{
	uint8_t temp[32];

	storage_hash(object->dataptr, object->datasize, temp);
	if (!memcmp(object->hashptr, temp, 32))
		return 0;
	else
		return -1;
}

int32_t storage_writeToFlash(void)
{
	uint8_t *internal_storage = NULL;
	uint8_t *penchead, *pcontent, *pcontent_bak;
	uint8_t *pns_rawhead, *pns_enchead, *pns_content;
	struct storage_block_raw_head *rawhead_t;
	struct storage_block_enc_head enchead_t;
	struct storage_node *node;
	struct storage_object *obj;

	uint32_t datasize;
	uint32_t outlen;
	uint32_t sumsize;
	int32_t ret = 0;


	internal_storage = (uint8_t *)get_internal_storage_base();
	if (!internal_storage)
		goto storage_writeflash_err;

	penchead = internal_storage + STORAGE_BLOCK_RAW_HEAD_SIZE;
	pcontent = penchead + STORAGE_BLOCK_ENC_HEAD_SIZE;
	pcontent_bak = pcontent;
	pns_rawhead = (uint8_t *)get_share_storage_block_base();
	pns_enchead = pns_rawhead + STORAGE_BLOCK_RAW_HEAD_SIZE;
	pns_content = pns_enchead + STORAGE_BLOCK_ENC_HEAD_SIZE;

	/* internal: raw head */
	memset(pns_rawhead, 0, STORAGE_BLOCK_RAW_HEAD_SIZE);
	rawhead_t = (struct storage_block_raw_head *)pns_rawhead;
	memcpy(rawhead_t->mark, "AMLSECURITY", 11);
	rawhead_t->version = BLOCK_VERSION;
	rawhead_t->enctype = storage_chose_enctype();
	/*fixme: no enc support now*/
	//set_encryptkeyfrom(rawhead_t->enctype);

	/* internal: storage */
	datasize = 0;
	list_for_each_entry(node, &nodelist, list)
		if (datasize < STORAGE_BLOCK_SIZE -
			STORAGE_BLOCK_RAW_HEAD_SIZE -
			STORAGE_BLOCK_ENC_HEAD_SIZE) {
			obj = &(node->object);
			if ((node->status & OBJ_STATUS_VALID)
			    && (obj->dataptr)) {
				Tlv_WriteObject(obj, pcontent, &outlen);
				datasize += outlen;
				pcontent += outlen;
			}
		}
	sumsize = (((datasize+15)/16)*16);
	sumsize += STORAGE_BLOCK_RAW_HEAD_SIZE;
	sumsize += STORAGE_BLOCK_ENC_HEAD_SIZE;
	if (sumsize > flash_storage_size) {
		//ERROR("storage size is larger than flash!\n");
		goto storage_writeflash_err;
	}
	memset(&enchead_t, 0, sizeof(enchead_t));
	enchead_t.blocksize = datasize;
	enchead_t.flashsize = flash_storage_size;
	Tlv_WriteHead(&enchead_t, penchead);
#if 1
	ret = do_aes_internal(1, penchead,
				STORAGE_BLOCK_ENC_HEAD_SIZE,
				pns_enchead, (int *)&outlen);
	if (ret)
		goto storage_writeflash_err;
#else
	ret = 0;
	memcpy(pns_enchead, penchead,
				STORAGE_BLOCK_ENC_HEAD_SIZE);
#endif

#if 1
	ret = do_aes_internal(1, pcontent_bak,
				((datasize+15)/16)*16,
				pns_content, (int *)&outlen);
	if (ret)
		goto storage_writeflash_err;
#else
	ret = 0;
	memcpy(pns_content, pcontent_bak, ((datasize+15)/16)*16);
#endif
	/* write message*/
	//msg->cmd = MSG_CMD_WRITEKEY;
	//msg->state += 1;
	ret = 0;
	version = BLOCK_VERSION;
	aesfrom = storage_chose_enctype();
	goto storage_writeflash_exit;

storage_writeflash_err:
		ret = -1;
storage_writeflash_exit:
	return ret;
}

static void __storage_add(struct storage_node *node)
{
	list_add(&node->list, &nodelist);
}

static void __storage_del(struct storage_node *node)
{
	list_del(&node->list);
	if (node->object.dataptr != NULL)
		storage_free(node->object.dataptr);
	storage_free(node);
}


static struct storage_node *storage_find(uint8_t *name, uint32_t namelen)
{
	struct storage_node *node;
	struct storage_object *object;

	list_for_each_entry(node, &nodelist, list)
		if (node->status & OBJ_STATUS_VALID) {
			object = &(node->object);
			if ((object->namesize == namelen) &&
				!memcmp(object->name, name, namelen))
				return node;
		}

	return NULL;
}


static struct storage_node *storage_create_empty(void)
{
	struct storage_node *node;

	node = storage_malloc(sizeof(*node));
	if (node == NULL)
		return NULL;
	memset(node, 0, sizeof(struct storage_node));

	__storage_add(node);
	return node;
}

static struct storage_node *storage_create(uint8_t *name, uint32_t namelen)
{
	struct storage_node *node;
	struct storage_object *object;

	node = storage_create_empty();
	if (node == NULL)
		return NULL;

	object = &(node->object);
	memcpy(object->name, name, namelen);
	object->namesize = namelen;

	return node;
}

static void storage_reset(void)
{
	struct storage_node *node;

	list_for_each_entry(node, &nodelist, list)
		__storage_del(node);
}

static int32_t storage_infocheck(struct storage_node *node, uint32_t attr)
{
	struct storage_object *obj = &(node->object);

	if (obj->attribute == attr)
		return 0;
	else
		return -1;
}

uint32_t storage_write(uint8_t *name, uint32_t namelen,
			uint8_t *writebuf, uint32_t bufsize, uint32_t attr)
{
	struct storage_node *node;
	struct storage_object *object;

	if (namelen >= MAX_OBJ_NAME_LEN)
		return RET_EINVAL;
	node = storage_find(name, namelen);
	if (node == NULL) { /*it's new object*/
		node = storage_create(name, namelen);
		if (node == NULL)
			return RET_EMEM;
		object = &(node->object);
		object->dataptr = storage_malloc((int32_t)bufsize);
		if (!object->dataptr)
			return RET_EMEM;
		node->status |= OBJ_STATUS_VALID;
		object->attribute = attr;
		object->type = OBJ_TYPE_GENERIC;
		memcpy(object->dataptr, writebuf, bufsize);
		object->datasize = bufsize;
		storage_hash(object->dataptr,
			object->datasize, object->hashptr);
	} else {  /* the object is existed*/
		if (storage_infocheck(node, attr) < 0)
			return RET_EINVAL;
		object = &(node->object);
		if (object->datasize < bufsize) {
			storage_free(object->dataptr);
			object->dataptr = storage_malloc(bufsize);
			if (!object->dataptr)
				return RET_EMEM;
			memcpy(object->dataptr, writebuf, bufsize);
			object->datasize = bufsize;
			storage_hash(object->dataptr,
				object->datasize, object->hashptr);
		} else {
			memcpy(object->dataptr, writebuf, bufsize);
			object->datasize = bufsize;
			storage_hash(object->dataptr,
				object->datasize, object->hashptr);
		}
	}
	if (!storage_writeToFlash())
		return RET_OK;
	else
		return RET_EFAIL;
}

static int32_t storage_read_permit(struct storage_node *node)
{
	struct storage_object *obj = &(node->object);

	if (!(obj->attribute & OBJ_ATTR_SECURE))
		return 1;
	return 0;
}
uint32_t storage_read(uint8_t *name, uint32_t namelen,
	uint8_t *readbuf, uint32_t readsize, uint32_t *outlen)
{
	struct storage_node   *node;
	struct storage_object *obj;

	if (namelen >= MAX_OBJ_NAME_LEN)
		return RET_EINVAL;
	node = storage_find(name, namelen);
	if (node == NULL)
		return RET_EFAIL;
	if (!storage_read_permit(node))
		return RET_EFAIL;
	obj = &(node->object);
	if (!obj->dataptr || !obj->datasize)
		return RET_EFAIL;
	if (obj->datasize >= readsize)
		*outlen = readsize;
	else
		*outlen = obj->datasize;

	memcpy(readbuf, obj->dataptr, *outlen);
	return RET_OK;
}


/**
 *    securestore_key_query - query whether key was burned.
 *    @keyname : key name will be queried.
 *    @query_result: query return value,
 *			0: key was NOT burned;
 *			1: key was burned;
 *			others: reserved.
 *
 *    return: 0: successful; others: failed.
 */
uint32_t storage_query(uint8_t *name, uint32_t namelen, uint32_t *retval)
{
	struct storage_node   *node;
	struct storage_object *obj;

	if (namelen >= MAX_OBJ_NAME_LEN)
		return RET_EINVAL;
	node = storage_find(name, namelen);
	if (node == 0) {
		*retval = 0;
		return RET_OK;
	}
	obj = &(node->object);
	if (obj->datasize && obj->dataptr)
		*retval = 1;
	else
		*retval = 0;
	return RET_OK;
}

uint32_t storage_verify(uint8_t *name, uint32_t namelen, uint8_t *hashbuf)
{
	struct storage_node   *node;
	struct storage_object *obj;

	if (namelen >= MAX_OBJ_NAME_LEN)
		return RET_EINVAL;
	node = storage_find(name, namelen);
	if (node == 0)
		return RET_EFAIL;
	obj = &(node->object);
	if (!obj->datasize || !obj->dataptr)
		return RET_EFAIL;

	memcpy(hashbuf, obj->hashptr, 32);
	return RET_OK;
}



uint32_t storage_tell(uint8_t *name, uint32_t namelen, uint32_t *outlen)
{
	struct storage_node   *node;
	struct storage_object *obj;

	if (namelen >= MAX_OBJ_NAME_LEN)
		return RET_EINVAL;
	node = storage_find(name, namelen);
	if (node == 0)
		return RET_EFAIL;
	obj = &(node->object);
	if (!obj->dataptr || !obj->datasize)
		return RET_EFAIL;
	*outlen = obj->datasize;
	return RET_OK;
}


uint32_t storage_status(uint8_t *name, uint32_t namelen, uint32_t *retval)
{
	struct storage_node   *node;
	struct storage_object *obj;

	if (namelen >= MAX_OBJ_NAME_LEN)
		return RET_EINVAL;

	node = storage_find(name, namelen);
	if (node == NULL)
		return RET_EFAIL;
	obj = &(node->object);

	*retval = (obj->attribute & (OBJ_ATTR_SECURE|OBJ_ATTR_ENC));
	return RET_OK;
}


/* initialize nodelist from internal storage block*/
void storage_init(uint32_t flashsize)
{
	uint8_t *internal_storage;
	struct storage_block_raw_head *prawhead;
	struct storage_block_enc_head enchead;
	uint8_t *pcontent, *prawcontent;
	uint8_t *pblock = (uint8_t *)get_share_storage_block_base();

	int32_t ret;
	uint32_t outlen, sum;
	struct storage_node *node;


	/* reset */
	storage_reset();
	internal_storage = (uint8_t *)get_internal_storage_base();
	if (!internal_storage)
		goto storage_init_err;

	aesfrom = -1;
	version = -1;
	prawhead = (struct storage_block_raw_head *)pblock;
	if (strcmp((const char *)(prawhead->mark), "AMLSECURITY") != 0) {
		flash_storage_size = flashsize;
		goto storage_init_err;
	}
	aesfrom = prawhead->enctype;
	/*fixme: no enc support now*/
#if 0
	set_encryptkeyfrom(aesfrom);
#endif
	version = prawhead->version;

#if 1
	ret = do_aes_internal(0,
			pblock+STORAGE_BLOCK_RAW_HEAD_SIZE,
			STORAGE_BLOCK_ENC_HEAD_SIZE,
			internal_storage, (int *)&outlen);
	if (ret) {
		flash_storage_size = flashsize;
		goto storage_init_err;
	}
#else
	memcpy(internal_storage, pblock + STORAGE_BLOCK_RAW_HEAD_SIZE,
					 STORAGE_BLOCK_ENC_HEAD_SIZE);
#endif

	memset(&enchead, 0, sizeof(enchead));
	ret = Tlv_ReadHead(internal_storage, &enchead);
	if (ret || (enchead.blocksize == 0)) {
		flash_storage_size = flashsize;
		goto storage_init_err;
	}
	if (enchead.flashsize != 0)
		flash_storage_size = enchead.flashsize;
	if (flash_storage_size != flashsize)
		goto storage_init_err;

	pcontent = pblock+STORAGE_BLOCK_RAW_HEAD_SIZE
				+ STORAGE_BLOCK_ENC_HEAD_SIZE;
	prawcontent = internal_storage + STORAGE_BLOCK_ENC_HEAD_SIZE;
#if 1
	ret = do_aes_internal(0, pcontent, ((enchead.blocksize+15)/16)*16,
				prawcontent, (int *)&outlen);
	if (ret)
		goto storage_init_err;
#else
	memcpy(prawcontent, pcontent, ((enchead.blocksize+15)/16)*16);
#endif
	sum = enchead.blocksize;
	while (sum > 0) {
		node = storage_create_empty();
		if (node == NULL)
			goto storage_init_err;
		ret = Tlv_ReadObject(prawcontent, node, &outlen);
		if (ret)
			goto storage_init_err;
		ret = check_object_valid(&(node->object));
		if (!ret)
			node->status = OBJ_STATUS_VALID;
		else
			node->status = OBJ_STATUS_INVALID;
		sum -= outlen;
		prawcontent += outlen;
	}
	goto storage_init_exit;

storage_init_err:
		storage_reset();
		free_share_storage();
		free_internal_storage();
storage_init_exit:
		return;
}

uint32_t get_share_storage_block_base(void)
{
	if (storage_share_block == NULL)
		storage_share_block = storage_malloc(STORAGE_BLOCK_SIZE);

	return (uint32_t)storage_share_block;
}

uint32_t get_internal_storage_base(void)
{
	if (internal_storage_block == NULL)
		internal_storage_block = storage_malloc(STORAGE_BLOCK_SIZE/4);

	return (uint32_t)internal_storage_block;
}

void free_share_storage(void)
{
	if (!storage_shar_in_block)
		storage_free(storage_shar_in_block);
	if (!storage_shar_out_block)
		storage_free(storage_shar_out_block);
	if (!storage_share_block)
		storage_free(storage_share_block);
}

void free_internal_storage(void)
{
	if (!internal_storage_block)
		storage_free(internal_storage_block);
}

uint64_t get_share_storage_block_size(void)
{
	return flash_storage_size;
}

int64_t storage_get_enctype(void)
{
	return (int64_t)aesfrom;
}


int32_t storage_chose_enctype(void)
{
	int64_t ret = -1;
	int32_t tmp = (int32_t)storage_get_enctype();

	switch (aesfrom_outer) {
	case ENC_TYPE_DEFAULT:
		if (tmp != ENC_TYPE_EFUSE)
			ret = 0;
		break;
	case ENC_TYPE_EFUSE:
		if (tmp == ENC_TYPE_EFUSE)
			ret = 0;
		break;
	case ENC_TYPE_FIXED:
		if ((tmp == -1) || (tmp == ENC_TYPE_FIXED))
			ret = 0;
		break;
	default:
		break;
	}
	if (!ret)
		return aesfrom_outer;
	return -1;
	/*fixme: efuse not ready */
#if 0
	else {
		if (tmp != -1)
			return tmp;
		if (efuse_has_burn_enckey())
			return ENC_TYPE_EFUSE;
		else
			return ENC_TYPE_DEFAULT;
	}
#endif
}


int64_t storage_set_enctype(uint64_t type)
{
	aesfrom_outer = type;
	return 0;
}


