/*
 * drivers/amlogic/unifykey/storagekey.c
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

/* extern from bl31 */
/*
 * when RET_OK
 * query: retval=1: key exsit,=0: key not exsit；
 * tell: retvak = key size
 * status: retval=1: secure, retval=0: non-secure

 */
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/scatterlist.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/kallsyms.h>
#include <linux/amlogic/efuse.h>
#include <linux/amlogic/unifykey/security_key.h>
#include <linux/amlogic/unifykey/key_manage.h>
#include <linux/of.h>
#include "unifykey.h"
#include "amlkey_if.h"

/* key buffer status */
/* bit0, dirty flag*/
#define KEYBUFFER_CLEAN		(0 << 0)
#define KEYBUFFER_DIRTY		(1 << 0)
#define SECUESTORAGE_HEAD_SIZE		(256)
#define SECUESTORAGE_WHOLE_SIZE		(0x40000)

#define OTHER_METHOD_CALL

#undef pr_fmt
#define pr_fmt(fmt) "unifykey: " fmt

struct storagekey_info_t {
	uint8_t *buffer;
	uint32_t size;
	uint32_t status;
};

static struct storagekey_info_t storagekey_info = {
	.buffer = NULL,
	/* default size */
	.size = SECUESTORAGE_WHOLE_SIZE,
	.status = KEYBUFFER_CLEAN,
};


store_key_ops store_key_read;
store_key_ops store_key_write;

#ifndef OTHER_METHOD_CALL
int store_operation_init(void)
{
	int ret = 0;

	if (kallsyms_lookup_name("nand_key_read")) {
		pr_info(" %s() nand storeage ops!\n", __func__);
		store_key_read = nand_key_read;
		store_key_write = nand_key_write;
	} else if (kallsyms_lookup_name("emmc_key_read")) {
		pr_info(" %s() emmc storeage ops!\n", __func__);
		store_key_read = emmc_key_read;
		store_key_write = emmc_key_write;
	} else {
		ret =  -1;
		pr_err(" %s() fail!\n", __func__);
	}

	return ret;
}
#endif

void storage_ops_read(store_key_ops read)
{
#ifdef OTHER_METHOD_CALL
	store_key_read = read;
#endif
}
EXPORT_SYMBOL(storage_ops_read);

void storage_ops_write(store_key_ops write)
{
#ifdef OTHER_METHOD_CALL
	store_key_write = write;
#endif
}
EXPORT_SYMBOL(storage_ops_write);

/**
 *1.init
 * return ok 0, fail 1
 */
int32_t amlkey_init_gen(uint8_t *seed, uint32_t len, int encrypt_type)
{
	int32_t ret = 0;
	uint32_t actual_size = 0;

#ifndef OTHER_METHOD_CALL
	ret = store_operation_init();
	if (ret < 0) {
		ret = -1;
		pr_err(" %s store_operation_init fail!\n", __func__);
		goto _out;
	}
#endif
	/* do nothing for now*/
	pr_info("%s() enter!\n", __func__);
	if (storagekey_info.buffer != NULL) {
		pr_err("%s() %d: already init!\n", __func__, __LINE__);
		goto _out;
	}

	/* get buffer from bl31 */
	storagekey_info.buffer =
		secure_storage_getbuffer(&storagekey_info.size);
	if (storagekey_info.buffer == NULL) {
		pr_err("%s() %d: can't get buffer from bl31!\n",
				__func__, __LINE__);
		ret = -1;
		goto _out;
	}

	/* full fill key infos from storage. */
	if (store_key_read) {
		ret = store_key_read(storagekey_info.buffer,
					storagekey_info.size, &actual_size);
		if (ret != 0) {
			pr_err("fail to read key data\n");
			memset(storagekey_info.buffer, 0,
				SECUESTORAGE_HEAD_SIZE);
			ret = -1;
			goto _out;
		}
		if (actual_size >= (1U << 20)) {
			pr_err("really need more than 1M mem? please check\n");
			memset(storagekey_info.buffer, 0,
				SECUESTORAGE_HEAD_SIZE);
			ret = -1;
			goto _out;
		}
	} else {
		pr_err("store_key_read is NULL\n");
		pr_err("check whether emmc_key/nand_key driver is enabled\n");
		ret = -1;
		goto _out;
	}

	storagekey_info.size = actual_size;
	pr_info("%s() storagekey_info.buffer=%p, storagekey_info.size = %0x!\n",
		__func__,
		storagekey_info.buffer,
		storagekey_info.size);

_out:
	return ret;
}

int32_t amlkey_init_m8b(uint8_t *seed, uint32_t len, int encrypt_type)
{
	int32_t ret = 0;
	uint32_t actual_size = 0;

#ifndef OTHER_METHOD_CALL
	ret = store_operation_init();
	if (ret < 0) {
		ret = -1;
		pr_err(" %s store_operation_init fail!\n", __func__);
		goto _out;
	}
#endif
	/* do nothing for now*/
	pr_info("%s() enter!\n", __func__);
	if (storagekey_info.buffer != NULL) {
		pr_err("%s() %d: already init!\n", __func__, __LINE__);
		goto _out;
	}

	if ((void *)kallsyms_lookup_name("nand_key_read")
		== (void *)store_key_read)
		secure_storage_type(0);
	else
		secure_storage_type(1);

	/* get buffer from bl31 */
	storagekey_info.buffer =
		secure_storage_getbuffer(&storagekey_info.size);
	if (storagekey_info.buffer == NULL) {
		pr_err("%s() %d: can't get buffer from bl31!\n",
				__func__, __LINE__);
		ret = -1;
		goto _out;
	}

	if (encrypt_type == -1)
		encrypt_type = 0;
	secure_storage_set_enctype(encrypt_type);

	/* full fill key infos from storage. */
	if (store_key_read) {
		ret = store_key_read(storagekey_info.buffer,
					storagekey_info.size, &actual_size);
		if (ret != 0) {
			pr_err("fail to read key data\n");
			memset(storagekey_info.buffer, 0,
				SECUESTORAGE_HEAD_SIZE);
			ret = -1;
			goto _out;
		}
		if (actual_size >= (1U << 20)) {
			pr_err("really need more than 1M mem? please check\n");
			memset(storagekey_info.buffer, 0,
				SECUESTORAGE_HEAD_SIZE);
			ret = -1;
			goto _out;
		}
	} else {
		pr_err("store_key_read is NULL\n");
		pr_err("check whether emmc_key/nand_key driver is enabled\n");
		ret = -1;
		goto _out;
	}

	storagekey_info.size = actual_size;
	pr_info("%s() storagekey_info.buffer=%p, storagekey_info.size = %0x!\n",
		__func__,
		storagekey_info.buffer,
		storagekey_info.size);

	storagekey_info.size = actual_size;
	secure_storage_notifier_ex(actual_size);

_out:
	return ret;
}


/**
 *2. query if the key already programmed
 * return: exsit 1, non 0
 */
int32_t amlkey_isexsit(const uint8_t *name)
{
	int32_t ret = 0;
	uint32_t retval;

	if (name == NULL) {
		pr_err("%s() %d, invalid key ", __func__, __LINE__);
		return 0;
	}

	ret = secure_storage_query((uint8_t *)name, &retval);
	if (ret) {
		pr_err("%s() %d: ret %d\n", __func__, __LINE__, ret);
		retval = 0;
	}

	return (int32_t)retval;
}

/**
 * 3. query if the prgrammed key is secure
 * return secure 1, non 0;
 */
int32_t amlkey_get_attr(const uint8_t *name)
{
	int32_t ret = 0;
	uint32_t retval;

	if (name == NULL) {
		pr_err("%s() %d, invalid key ", __func__, __LINE__);
		return 0;
	}

	ret = secure_storage_status((uint8_t *)name, &retval);
	if (ret) {
		pr_err("%s() %d: ret %d\n", __func__, __LINE__, ret);
		retval = 0;
	}

	return (int32_t)retval;
}

/**
 * 3.1 query if the prgrammed key is secure
 * return secure 1, non 0;
 */
int32_t amlkey_issecure(const uint8_t *name)
{
	return amlkey_get_attr(name) & KEY_UNIFY_ATTR_SECURE_MASK;
}

/**
 * 3.2 query if the prgrammed key is encrypt
 * return encrypt 1, non 0;
 */
int32_t amlkey_isencrypt(const uint8_t *name)
{
	return amlkey_get_attr(name) & KEY_UNIFY_ATTR_ENCRYPT_MASK;
}

/**
 * 4. actual bytes of key value
 *  return actual size.
 */
ssize_t amlkey_size(const uint8_t *name)
{
	ssize_t size = 0;
	int32_t ret = 0;
	uint32_t retval;

	if (name == NULL) {
		pr_err("%s() %d, invalid key ", __func__, __LINE__);
		return 0;
	}

	ret = secure_storage_tell((uint8_t *)name, &retval);
	if (ret) {
		pr_err("%s() %d: ret %d\n", __func__, __LINE__, ret);
		retval = 0;
	}
	size = (ssize_t)retval;
	return size;
}

/**
 *5. read non-secure key in bytes, return bytes readback actully.
 * return actual size read back.
 */
ssize_t amlkey_read(const uint8_t *name, uint8_t *buffer, uint32_t len)
{
	int32_t ret = 0;
	ssize_t retval = 0;
	uint32_t actul_len;

	if (name == NULL) {
		pr_err("%s() %d, invalid key ", __func__, __LINE__);
		return 0;
	}
	ret = secure_storage_read((uint8_t *)name, buffer, len, &actul_len);
	if (ret) {
		pr_err("%s() %d: return %d\n", __func__, __LINE__, ret);
		retval = 0;
		goto _out;
	}
	retval = actul_len;
_out:
	return retval;
}

/**
 * 6.write key with attr in bytes , return bytes readback actully
 * attr: bit0, secure/non-secure
 *       bit8, encrypt/non-encrypt
 * return actual size write down.
 */
ssize_t amlkey_write(const uint8_t *name,
	uint8_t *buffer,
	uint32_t len,
	uint32_t attr)
{
	int32_t ret = 0;
	ssize_t retval = 0;
	uint32_t actual_length;
	uint8_t *buf = NULL;

	if (name == NULL) {
		pr_err("%s() %d, invalid key ", __func__, __LINE__);
		return retval;
	}

	pr_info("%s %d\n", __func__, __LINE__);
	ret = secure_storage_write((uint8_t *)name,
			buffer, len,
			attr);
	if (ret) {
		pr_err("%s() %d: return %d\n", __func__, __LINE__, ret);
		retval = 0;
		goto _out;
	} else {
		retval = (ssize_t)len;
		/* write down! */
		if (storagekey_info.buffer != NULL) {
			buf = kmalloc(storagekey_info.size, GFP_KERNEL);
			if (!buf)
				return -ENOMEM;
			memcpy(buf, storagekey_info.buffer,
					storagekey_info.size);
			if (store_key_write)
				ret = store_key_write(buf,
						storagekey_info.size,
						&actual_length);
			if (ret) {
				pr_err("%s() %d, store_key_write fail\n",
						__func__, __LINE__);
				retval = 0;
			}
			kfree(buf);
		}
	}
_out:
	return retval;
}
/**
 * 7. get the hash value of programmed secure key | 32bytes length, sha256
 * return success 0, fail -1
 */
int32_t amlkey_hash_4_secure(const uint8_t *name, uint8_t *hash)
{
	int32_t ret = 0;

	ret = secure_storage_verify((uint8_t *)name, hash);

	return ret;
}

/**
 * 7. del key by name
 * return success 0, fail -1
 */
int32_t amlkey_del(const uint8_t *name)
{
	int32_t ret = 0;
	uint32_t actual_length;

	if ((ret == 0) && (storagekey_info.buffer != NULL)) {
		/* flush back */
		if (store_key_write)
			ret = store_key_write(storagekey_info.buffer,
				storagekey_info.size, &actual_length);
		if (ret) {
			pr_err("%s() %d, store_key_write fail\n",
				 __func__,
				 __LINE__);
		}
	} else {
		pr_err("%s() %d, remove key fail\n",
			 __func__,
			 __LINE__);
	}

	return ret;
}


