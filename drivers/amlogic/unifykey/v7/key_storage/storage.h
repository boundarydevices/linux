/*
 * drivers/amlogic/unifykey/v7/key_storage/storage.h
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

#ifndef __STORAGE_H_
#define __STORAGE_H_

#include <linux/types.h>
#include <linux/list.h>
#include <linux/string.h>

#define KEY_MAX_COUNT  32


#define MAX_OBJ_NAME_LEN	16
#define MAX_MAGIC_STR_LEN       16

/*Attribute*/
#define OBJ_ATTR_SECURE	(1<<0)
#define OBJ_ATTR_OTP	(1<<1)
#define OBJ_ATTR_ENC	(1<<8)

/* Type */
#define OBJ_TYPE_GENERIC	0xA00000BF

struct storage_object {
	char     name[MAX_OBJ_NAME_LEN];
	uint16_t slot;
	uint16_t type1;
	uint16_t valid_size;
	uint16_t storage_size;
	uint16_t state;
	uint16_t checksum;
	char     hashptr[36];
	uint32_t namesize;
	uint32_t attribute; /*secure, OTP*/
	uint32_t type; /*AES, RSA, GENERIC, ...*/
	uint32_t datasize;
	char	 reserve[40];
	uint8_t  *storage_data;
	uint8_t  *dataptr;
};

#define OBJ_STATUS_FREE 0
#define OBJ_STATUS_VALID (1<<0)
#define OBJ_STATUS_INVALID (1<<1) /*BE DELETED*/
struct storage_node {
	struct list_head list;
	struct storage_object object;
	uint32_t status;
};

/* Storage BLOCK RAW HEAD: fixed 512B*/
#define ENC_TYPE_DEFAULT 0
#define ENC_TYPE_EFUSE	1
#define ENC_TYPE_FIXED	2
#define STORAGE_BLOCK_RAW_HEAD_SIZE 512
#define BLOCK_VERSION	0

struct emmc_key_head {
	uint8_t   mark[MAX_MAGIC_STR_LEN];
	uint32_t  mark_checksum;
	uint32_t  checksum;
	uint32_t  reserve;
};

struct key_head_item {
	uint32_t position;
	uint32_t state;
	uint32_t reserve;
};
struct key_storage_head {
	char     mark[MAX_MAGIC_STR_LEN];
	uint32_t version;
	uint32_t inver; //inver = ~version + 1
	uint32_t tag;
	uint32_t size; //tatol size
	uint32_t item_cnt;
	struct key_head_item item[KEY_MAX_COUNT];
	char     reserve[92];
};


struct storage_block_raw_head {
	uint8_t mark[16]; /* AMLSECURITY*/
	uint32_t version;
	uint32_t enctype; /*from EFUSE, from default, from fixed*/
};

/* Storage BLOCK ENC HEAD: fixed 512B*/
#define STORAGE_BLOCK_ENC_HEAD_SIZE 512
struct storage_block_enc_head {
	uint32_t blocksize;
	uint32_t flashsize;
};

/* Storage Format: TLV*/
enum emTLVTag {
	emTlvNone,

	emTlvHead,
	emTlvHeadSize,

	emTlvObject,
	emTlvObjNameSize,
	emTlvObjName,
	emTlvObjDataSize,
	emTlvObjDataBuf,
	emTlvObjType,
	emTlvObjAttr,
	emTlvObjHashBuf,

	emTlvHeadFlashSize,
};

#define MAX_STORAGE_NODE_COUNT 64

/* message struct*/
enum message_state {
	MSG_FREE = 0x00000000,
	MSG_BUSY = 0x00000001,
	MSG_READY,
};
#define MSG_CMD_WRITEKEY 1
struct storage_ns_message {
	uint32_t cmdlock;
	uint32_t cmd;
	uint32_t state;
	uint32_t input;
	uint32_t inlen;
	uint32_t output;
	uint32_t outlen;
};

/* return value*/
#define RET_OK		0
#define RET_EFAIL	1
#define RET_EINVAL	2
#define RET_EMEM	3

/* storge hal api verify result */
#define STORAGE_VERIFY_RET_SUCCESS	0
#define STORAGE_VERIFY_RET_HASH_FAIL   -1
#define STORAGE_VERIFY_RET_NO_KEY	-2

/* internal apis*/
uint32_t get_share_storage_block_base(void);
void free_share_storage(void);
void free_internal_storage(void);
uint64_t get_share_storage_block_size(void);

uint32_t storage_write(uint8_t *name, uint32_t namelen,
					uint8_t *writebuf,
					uint32_t bufsize, uint32_t attr);
uint32_t storage_read(uint8_t *name, uint32_t namelen,
					uint8_t *readbuf, uint32_t readsize,
					uint32_t *outlen);
uint32_t storage_tell(uint8_t *name, uint32_t namelen, uint32_t *outlen);
uint32_t storage_query(uint8_t *name, uint32_t namelen, uint32_t *retval);
uint32_t storage_verify(uint8_t *name, uint32_t namelen, uint8_t *hashbuf);
uint32_t storage_status(uint8_t *name, uint32_t namelen, uint32_t *retval);
uint32_t storage_list(uint8_t *outbuf);
uint32_t storage_remove(uint8_t *name, uint32_t namelen);
int64_t  storage_get_enctype(void);
int64_t storage_set_enctype(uint64_t type);
int32_t storage_chose_enctype(void);
int64_t storage_version(void);

/*exported APIs*/
void storage_init(uint32_t flashsize);
void storage_set_type(uint32_t is_emmc);
uint64_t storage_api_write(void *in);
uint64_t storage_api_read(void *in, void **out);
uint64_t storage_api_tell(void *in, void **out);
uint64_t storage_api_query(void *in, void **out);
uint64_t storage_api_verify(void *in, void **out);
uint64_t storage_api_status(void *in, void **out);
void storage_api_notify(void);
void storage_api_notify_ex(uint32_t flashsize);
void storage_api_storage_type(uint32_t is_emmc);
uint64_t storage_api_list(void);
uint64_t storage_api_remove(void);
int64_t storage_api_get_enctype(void);
int64_t storage_api_set_enctype(uint64_t type);
int64_t storage_api_version(void);

/* aes apis*/
void set_encryptkeyfrom(int32_t from);
int32_t efuse_has_burn_enckey(void);

/* TLV */
int32_t Tlv_ReadHead(uint8_t *buf, struct storage_block_enc_head *pblockhead);
int32_t Tlv_ReadObject(uint8_t *input, struct storage_node *node,
					uint32_t *allsize);
void Tlv_WriteHead(struct storage_block_enc_head *enchead, uint8_t *output);
void Tlv_WriteObject(struct storage_object *object, uint8_t *output,
					uint32_t *outlen);

#endif
