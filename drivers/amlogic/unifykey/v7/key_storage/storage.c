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
#include <linux/slab.h>
#include <linux/kallsyms.h>
#include "storage.h"
#include "storage_util.h"
#include "storage_data.h"
#include "storage_def.h"
#include "crypto_api.h"

#undef pr_fmt
#define pr_fmt(fmt) "unifykey: " fmt


static int emmc_key_transfer(u8 *buf, u32 *value, u32 len, u32 direct);
static uint32_t emmckey_checksum(uint8_t *buf, uint32_t length);
/* static var is initialized to be 0 by default */
static uint32_t storage_is_emmc;

static void storage_hash(uint8_t *input, uint32_t insize, uint8_t *output)
{
	sha256(input, insize, output);
}

static int32_t check_object_valid(struct storage_object *object)
{
	/* uboot does not support hash checking */
#if 0
	uint8_t temp[32];

	storage_hash(object->dataptr, object->datasize, temp);
	if (!memcmp(object->hashptr, temp, 32))
		return 0;
	else
		return -1;
#endif
	return 0;
}

int32_t storage_writeToFlash(void)
{
	uint8_t			 *pblock =
		(uint8_t *)get_share_storage_block_base();
	struct emmc_key_head     *pemmc_key_head;
	struct key_storage_head  *pstorage_key_head;
	uint8_t                  *pkey_data;
	struct storage_node	 *node;
	struct storage_object    *obj;
	uint32_t                 pos = 0;

	/* for emmc , there is an extra header to be handled */
	/* the below check is ugly, someone should move the
	 * related code to emmc key
	 */
	if (storage_is_emmc) {
		pemmc_key_head = (struct emmc_key_head *)pblock;
		pstorage_key_head = (struct key_storage_head *)
			(pblock + sizeof(struct emmc_key_head));
	} else {
		pemmc_key_head = NULL;
		pstorage_key_head = (struct key_storage_head *)pblock;
	}

	/* fill storage key header */
	memset(pstorage_key_head, 0x00, sizeof(struct key_storage_head));
	memcpy(pstorage_key_head->mark, "keyexist", 8);
	pstorage_key_head->version = 2;
	pstorage_key_head->item_cnt = 0;
	pstorage_key_head->size = 0;

	pstorage_key_head->size += sizeof(struct key_storage_head);
	pkey_data = (uint8_t *)pstorage_key_head +
		sizeof(struct key_storage_head);

	list_for_each_entry_reverse(node, &nodelist, list)
		if (node->status & OBJ_STATUS_VALID) {
			obj = &(node->object);
			pstorage_key_head->item[pstorage_key_head->
				item_cnt].position = pos;
			memcpy(&pkey_data[pos],
				obj, sizeof(struct storage_object));
			pos += sizeof(struct storage_object);
			memcpy(&pkey_data[pos],
				obj->storage_data, obj->storage_size);
			pos += obj->storage_size;


			pstorage_key_head->size +=
				sizeof(struct storage_object);
			pstorage_key_head->size += obj->storage_size;
			pstorage_key_head->item_cnt++;
		}

	if (pemmc_key_head != NULL) {
		/* fill the extra emmc key header */
		memset(pemmc_key_head, 0x00, sizeof(struct emmc_key_head));
		memcpy(pemmc_key_head->mark, "emmckeys", 8);
		emmc_key_transfer(pemmc_key_head->mark,
			&pemmc_key_head->mark_checksum, 8, 1);
		/*
		 *   do not blame me for the magic number 128*1024-28,
		 * only to be compatible for old next-dev
		 */
		pemmc_key_head->checksum =
			emmckey_checksum((uint8_t *)pstorage_key_head,
				128*1024-28);
	}

	return RET_OK;
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
	if (node->object.storage_data != NULL)
		storage_free(node->object.storage_data);
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

	node = storage_zalloc(sizeof(*node));
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

static void storage_del(struct storage_node *node)
{
	__storage_del(node);
}


static void storage_reset(void)
{
	struct storage_node *node;
	struct storage_node *tmp;

	list_for_each_entry_safe(node, tmp, &nodelist, list)
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


static uint16_t aml_key_checksum(char *data, int length)
{
	uint16_t checksum;
	uint8_t  *pdata;
	int      i;

	checksum = 0;
	pdata = (uint8_t *)data;
	for (i = 0; i < length; i++)
		checksum += pdata[i];

	return checksum;
}


static uint32_t emmckey_checksum(uint8_t *buf, uint32_t length)
{
	uint32_t checksum = 0;
	uint32_t cnt;

	for (cnt = 0; cnt < length; cnt++)
		checksum += buf[cnt];

	return checksum;
}



static int is_hex_str(uint8_t *buf, uint32_t len)
{
	uint32_t i;

	for (i = 0; i < len; i++)
		switch (buf[i]) {
		case '0' ... '9':
		case 'a' ... 'f':
		case 'A' ... 'F':
			break;
		default:
			return 0;
		}
	return 1;
}

static char asc_to_hex(char para)
{
	if (para >= '0' && para <= '9')
		para = para-'0';
	else if (para >= 'a' && para <= 'f')
		para = para - 'a'+0xa;
	else if (para >= 'A' && para <= 'F')
		para = para-'A'+0xa;

	return para;
}


uint32_t to_storage_data(uint8_t *data, uint32_t valid_size,
			uint8_t *storage_data, uint16_t storage_size,
			uint16_t *checksum)
{
	uint8_t               *tmp_buf;
	uint8_t               *data_clone;
	uint8_t               chr;
	int                   i;
	uint32_t              valid_comp_size = 0;
	int                   enc_data_len = 0;


	if (!is_hex_str(data, valid_size))
		return RET_EINVAL;

	data_clone = storage_zalloc(valid_size);
	if (!data_clone)
		return RET_EMEM;
	memcpy(data_clone, data, valid_size);

	for (i = 0; i < valid_size; i++) {
		chr = data_clone[i];
		if (i%2 == 0)
			data_clone[i/2] = 0x00;
		data_clone[i/2] |= (i%2 == 0) ?
			(asc_to_hex(chr) << 4):(asc_to_hex(chr) & 0xf);
	}


	valid_comp_size = (valid_size+1)>>1;
	*checksum = aml_key_checksum(data_clone, valid_comp_size);

	tmp_buf = storage_zalloc(storage_size);
	if (!tmp_buf) {
		storage_free(data_clone);
		return RET_EMEM;
	}
	memcpy(tmp_buf, &valid_comp_size, 4);
	memcpy(tmp_buf+4, data_clone, valid_comp_size);

	do_aes_internal(1, tmp_buf, storage_size, storage_data, &enc_data_len);

	storage_free(tmp_buf);
	storage_free(data_clone);

	return RET_OK;
}


uint32_t storage_write(uint8_t *name, uint32_t namelen,
			uint8_t *writebuf, uint32_t bufsize, uint32_t attr)
{
	struct storage_node   *node;
	struct storage_object *object;
	int                   is_new = 0;


	if ((namelen >= MAX_OBJ_NAME_LEN) ||
		!is_hex_str(writebuf, bufsize))
		return RET_EINVAL;

	/* update key node */
	node = storage_find(name, namelen);
	if (node == NULL) { /*it's new object*/
		is_new = 1;
		node = storage_create(name, namelen);
		if (node == NULL)
			return RET_EMEM;
		object = &(node->object);
		node->status |= OBJ_STATUS_VALID;
		object->attribute = attr;
		object->namesize = namelen;
		object->type = OBJ_TYPE_GENERIC;
		object->type1 = 0x41c; /* be compatible with old unifykey */
	} else {  /* the object is existed*/
		/* TODO: check whether attr is updated correctly */
		if (storage_infocheck(node, attr) < 0)
			return RET_EINVAL;
		object = &(node->object);
		storage_free(object->dataptr);
		storage_free(object->storage_data);
	}
	object->datasize = bufsize;
	object->valid_size = bufsize;
	object->storage_size = (((((object->valid_size+1)>>1)+4)+15)>>4)<<4;
	object->dataptr = storage_zalloc(object->valid_size);
	if (!object->dataptr) {
		if (is_new)
			storage_del(node);
		return RET_EMEM;
	}
	object->storage_data = storage_zalloc(object->storage_size);
	if (!object->storage_data) {
		if (is_new)
			storage_del(node);
		return RET_EMEM;
	}
	memcpy(object->dataptr, writebuf, object->valid_size);

	if (to_storage_data(object->dataptr, object->valid_size,
		object->storage_data, object->storage_size,
		&object->checksum) != RET_OK) {
		if (is_new)
			storage_del(node);
		return RET_EFAIL;
	}

	storage_hash(object->dataptr,
		object->datasize, object->hashptr);

	storage_writeToFlash();

	return RET_OK;
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

static int emmc_key_transfer(u8 *buf, u32 *value, u32 len, u32 direct)
{
	u8 checksum = 0;
	u32 i;

	if (direct) {
		for (i = 0; i < len; i++)
			checksum += buf[i];
		for (i = 0; i < len; i++)
			buf[i] ^= checksum;
		*value = checksum;
	} else {
		checksum = *value;
		for (i = 0; i < len; i++)
			buf[i] ^= checksum;
		checksum = 0;
		for (i = 0; i < len; i++)
			checksum += buf[i];
		if (checksum == *value)
			return 0;
		return -1;
	}

	return 0;
}

void storage_set_type(uint32_t is_emmc)
{
	storage_is_emmc = is_emmc;
}

/* initialize nodelist from internal storage block*/
void storage_init(uint32_t flashsize)
{
	uint8_t                  *pblock =
		(uint8_t *)get_share_storage_block_base();
	struct emmc_key_head     *pemmc_key_head;
	struct key_storage_head  *pstorage_key_head;
	int			 key_count;
	int                      i, j, n;
	struct storage_node      *node;
	struct storage_object    *tmp_obj;
	char			 *tmp_content;
	uint8_t                  *tmp;
	int                      out_len;
	int                      key_hex_len;

	/* we should handle an extra header for emmc key */
	if (storage_is_emmc) {
		pemmc_key_head = (struct emmc_key_head *)pblock;
		pstorage_key_head = (struct key_storage_head *)
			(pblock + sizeof(struct emmc_key_head));
	} else {
		pemmc_key_head = NULL;
		pstorage_key_head = (struct key_storage_head *)pblock;
	}

	if (pemmc_key_head != NULL) {
		if (!emmc_key_transfer(pemmc_key_head->mark,
			&pemmc_key_head->mark_checksum, 8, 0)) {
			if (memcmp(pemmc_key_head->mark,
				"emmckeys", 8) != 0)
				goto storage_init_err;
		} else
			goto storage_init_err;
	}

	if (memcmp(pstorage_key_head->mark, "keyexist", 8) != 0)
		goto storage_init_err;

	/* parse each key */
	tmp_content = (char *)&pstorage_key_head[1]; // skip storage key header
	key_count = pstorage_key_head->item_cnt;
	for (i = 0; i < key_count; i++) {
		node = storage_create_empty();
		if (node == NULL)
			goto storage_init_err;

		tmp_obj = (struct storage_object *)
			&tmp_content[pstorage_key_head->item[i].position];
		node->object.slot = tmp_obj->slot;
		node->object.state = tmp_obj->state;
		node->object.storage_size = tmp_obj->storage_size;
		node->object.valid_size = tmp_obj->valid_size;
		node->object.type1 = tmp_obj->type1;
		node->object.checksum = tmp_obj->checksum;
		node->object.type = tmp_obj->type;
		node->object.attribute = tmp_obj->attribute;
		node->object.datasize = tmp_obj->valid_size;
		strcpy(node->object.name, tmp_obj->name);
		node->object.namesize = strlen(node->object.name);

		node->object.storage_data =
			storage_malloc(node->object.storage_size);
		if (node->object.storage_data == NULL)
			goto storage_init_err;
		node->object.dataptr = storage_malloc(node->object.valid_size);
		if (node->object.dataptr == NULL)
			goto storage_init_err;
		memcpy(node->object.storage_data,
			&tmp_content[pstorage_key_head->item[i].position+
			sizeof(struct storage_object)],
			node->object.storage_size);
		memcpy(node->object.hashptr, tmp_obj->hashptr,
			sizeof(tmp_obj->hashptr));

		/* decrypt each key */
		tmp = storage_malloc(node->object.storage_size);
		if (tmp == NULL)
			goto storage_init_err;

		do_aes_internal(0, node->object.storage_data,
			node->object.storage_size, tmp, &out_len);
		memcpy(&key_hex_len, tmp, 4);
		tmp = tmp + 4;

		/* switch to ascii form */
		/* TODO: add checksum validation(refer to key_core_show) */
		memset(node->object.dataptr, 0x00, node->object.valid_size);
		n = 0;
		for (j = 0; j < node->object.valid_size>>1; j++)
			n += sprintf(&node->object.dataptr[n], "%02x", tmp[j]);
		if (node->object.valid_size % 2)
			n += sprintf(&node->object.dataptr[n], "%x",
				(tmp[j]&0xf0)>>4);
		tmp = tmp - 4;
		storage_free(tmp);

		if (check_object_valid(&(node->object)) == 0)
			node->status = OBJ_STATUS_VALID;
		else
			node->status = OBJ_STATUS_INVALID;
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
		storage_share_block = storage_malloc(
		(int32_t)get_share_storage_block_size());

	return (uint32_t)storage_share_block;
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
	if (storage_is_emmc)
		return STORAGE_BLOCK_SIZE;
	else
		return STORAGE_BLOCK_SIZE_2;
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


