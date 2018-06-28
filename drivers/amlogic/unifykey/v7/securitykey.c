/*
 * drivers/amlogic/unifykey/v7/securitykey.c
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

#include <linux/errno.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/of_fdt.h>
#include <linux/module.h>
#include <linux/libfdt_env.h>
#include <linux/of_reserved_mem.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/amlogic/unifykey/security_key.h>
#include <linux/amlogic/unifykey/v7/key_service_routine.h>

#undef pr_fmt
#define pr_fmt(fmt) "unifykey: " fmt


static void *storage_block_base;
static long storage_block_size;

static unsigned long storage_free_func;
static unsigned long storage_read_func;
static unsigned long storage_write_func;
static unsigned long storage_query_func;
static unsigned long storage_tell_func;
static unsigned long storage_status_func;
static unsigned long storage_verify_func;
static unsigned long storage_list_func;
static unsigned long storage_remove_func;
static unsigned long storage_notify_ex_func;
static unsigned long storage_type_func;
static unsigned long storage_set_enctype_func;
static unsigned long storage_get_enctype_func;
static unsigned long storage_version_func;

static unsigned int block_base_func_id;
static unsigned int block_size_func_id;

static DEFINE_MUTEX(storage_lock);


static inline int32_t smc_to_linux_errno(uint64_t errno)
{
	int32_t ret = (int32_t)(errno & 0xffffffff);
	return ret;
}

void *secure_storage_getbuffer(uint32_t *size)
{
	storage_block_size = storage_service_routine(
		block_size_func_id, 0, NULL, NULL);
	storage_block_base = (void *)(uint32_t)storage_service_routine(
		block_base_func_id, 0, NULL, NULL);

	*size = storage_block_size;
	return (void *)storage_block_base;
}

void secure_storage_notifier_ex(uint32_t storagesize)
{
	mutex_lock(&storage_lock);
	storage_service_routine(storage_notify_ex_func,
				storagesize, NULL, NULL);
	mutex_unlock(&storage_lock);
}

void secure_storage_type(uint32_t is_emmc)
{
	mutex_lock(&storage_lock);
	storage_service_routine(storage_type_func,
				is_emmc, NULL, NULL);
	mutex_unlock(&storage_lock);
}


int32_t secure_storage_write(uint8_t *keyname, uint8_t *keybuf,
			uint32_t keylen, uint32_t keyattr)
{
	uint32_t *input = NULL;
	uint32_t *input_start = NULL;
	uint32_t in_len = 0;
	uint32_t namelen;
	uint8_t  *keydata, *name;
	uint64_t ret;

	mutex_lock(&storage_lock);
	namelen = strlen((const char *)keyname);
	in_len = sizeof(namelen) + sizeof(keylen) + sizeof(keyattr)
		     + namelen + keylen;
	input = kmalloc(in_len, GFP_KERNEL);
	if (input == NULL)
		ret = RET_EMEM;
	else {
		input_start = input;
		*input++ = namelen;
		*input++ = keylen;
		*input++ = keyattr;
		name = (uint8_t *)input;
		memcpy(name, keyname, namelen);
		keydata = name + namelen;
		memcpy(keydata, keybuf, keylen);
		ret = storage_service_routine(storage_write_func,
					      0, input_start, NULL);
	}
	if (input_start != NULL)
		kfree(input_start);
	mutex_unlock(&storage_lock);
	return smc_to_linux_errno(ret);
}

int32_t secure_storage_read(uint8_t *keyname, uint8_t *keybuf,
				uint32_t keylen, uint32_t *readlen)
{
	uint32_t *input = NULL;
	uint32_t *input_start = NULL;
	uint32_t in_len = 0;
	uint32_t *output = NULL;
	uint32_t namelen;
	uint8_t  *name, *buf;
	uint64_t ret;

	mutex_lock(&storage_lock);
	namelen = strlen((const char *)keyname);
	in_len = sizeof(namelen)+sizeof(keylen)+namelen;
	input = kmalloc(in_len, GFP_KERNEL);
	if (input == NULL)
		ret = RET_EMEM;
	else {
		input_start = input;
		*input++ = namelen;
		*input++ = keylen;
		name = (uint8_t *)input;
		memcpy(name, keyname, namelen);
		ret = storage_service_routine(storage_read_func, 0,
					      input_start, (void **)(&output));
		if (ret == RET_OK) {
			*readlen = *output;
			buf = (uint8_t *)(output + 1);
			memcpy(keybuf, buf, *readlen);
		}
	}
	if (input_start != NULL)
		kfree(input_start);
	if (output != NULL)
		kfree(output);
	mutex_unlock(&storage_lock);
	return smc_to_linux_errno(ret);
}

int32_t secure_storage_verify(uint8_t *keyname, uint8_t *hashbuf)
{
	uint32_t *input = NULL;
	uint32_t *input_start = NULL;
	uint32_t in_len = 0;
	uint32_t *output = NULL;
	uint32_t namelen;
	uint8_t *name;
	uint64_t ret = 0;

	mutex_lock(&storage_lock);
	namelen = strlen((const char *)keyname);
	in_len = sizeof(namelen) + namelen;
	input = kmalloc(in_len, GFP_KERNEL);
	if (input == NULL)
		ret = RET_EMEM;
	else {
		input_start = input;
		*input++ = namelen;
		name = (uint8_t *)input;
		memcpy(name, keyname, namelen);
		ret = storage_service_routine(storage_verify_func, 0,
					      input_start, (void **)(&output));
		if (ret == RET_OK)
			memcpy(hashbuf, (uint8_t *)output, 32);
	}
	if (input_start != NULL)
		kfree(input_start);
	if (output != NULL)
		kfree(output);
	mutex_unlock(&storage_lock);
	return smc_to_linux_errno(ret);
}

int32_t secure_storage_query(uint8_t *keyname, uint32_t *retval)
{
	uint32_t *input = NULL;
	uint32_t *input_start = NULL;
	uint32_t  in_len = 0;
	uint32_t *output = NULL;
	uint32_t namelen;
	uint8_t  *name;
	uint64_t ret = 0;

	mutex_lock(&storage_lock);
	namelen = strlen((const char *)keyname);
	in_len = sizeof(namelen) + namelen;
	input = kmalloc(in_len, GFP_KERNEL);
	if (input == NULL)
		ret = RET_EMEM;
	else {
		input_start = input;
		*input++ = namelen;
		name = (uint8_t *)input;
		memcpy(name, keyname, namelen);
		ret = storage_service_routine(storage_query_func, 0,
					      input_start, (void **)(&output));
		if (ret == RET_OK)
			*retval = *output;
	}
	if (input_start != NULL)
		kfree(input_start);
	if (output != NULL)
		kfree(output);
	mutex_unlock(&storage_lock);
	return smc_to_linux_errno(ret);
}

int32_t secure_storage_tell(uint8_t *keyname, uint32_t *retval)
{
	uint32_t *input = NULL;
	uint32_t *input_start = NULL;
	uint32_t in_len = 0;
	uint32_t *output = NULL;
	uint32_t namelen;
	uint8_t *name;
	uint64_t ret = 0;

	mutex_lock(&storage_lock);
	namelen = strlen((const char *)keyname);
	in_len = sizeof(namelen) + namelen;
	input = kmalloc(in_len, GFP_KERNEL);
	if (input == NULL)
		ret = RET_EMEM;
	else {
		input_start = input;
		*input++ = namelen;
		name = (uint8_t *)input;
		memcpy(name, keyname, namelen);
		ret = storage_service_routine(storage_tell_func, 0,
					      input_start, (void **)(&output));
		if (ret == RET_OK)
			*retval = *output;
	}
	if (input_start != NULL)
		kfree(input_start);
	if (output != NULL)
		kfree(output);
	mutex_unlock(&storage_lock);
	return smc_to_linux_errno(ret);
}

int32_t secure_storage_status(uint8_t *keyname, uint32_t *retval)
{
	uint32_t *input = NULL;
	uint32_t *input_start = NULL;
	uint32_t in_len = 0;
	uint32_t *output = NULL;
	uint32_t namelen;
	uint8_t *name;
	uint64_t ret = 0;

	mutex_lock(&storage_lock);
	namelen = strlen((const char *)keyname);
	in_len = sizeof(namelen) + namelen;
	input = kmalloc(in_len, GFP_KERNEL);
	if (input == NULL)
		ret = RET_EMEM;
	else {
		input_start = input;
		*input++ = namelen;
		name = (uint8_t *)input;
		memcpy(name, keyname, namelen);
		ret = storage_service_routine(storage_status_func, 0,
					      input_start, (void **)(&output));
		if (ret == RET_OK)
			*retval = *output;
	}
	if (input_start != NULL)
		kfree(input_start);
	if (output != NULL)
		kfree(output);
	mutex_unlock(&storage_lock);
	return smc_to_linux_errno(ret);
}

int32_t secure_storage_list(uint8_t *listbuf,
		uint32_t buflen, uint32_t *readlen)
{
	uint64_t ret = 0;

	return smc_to_linux_errno(ret);
}

int32_t secure_storage_remove(uint8_t *keyname)
{
	uint64_t ret = 0;

	return smc_to_linux_errno(ret);
}

int32_t secure_storage_set_enctype(uint32_t type)
{
	uint64_t ret = 0;

	mutex_lock(&storage_lock);
	ret = storage_service_routine(storage_set_enctype_func,
				      type, NULL, NULL);
	mutex_unlock(&storage_lock);

	return smc_to_linux_errno(ret);
}
int32_t secure_storage_get_enctype(void)
{
	uint64_t ret = 0;

	return smc_to_linux_errno(ret);
}
int32_t secure_storage_version(void)
{
	uint64_t ret = 0;

	return smc_to_linux_errno(ret);
}

static int storage_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	unsigned int id;
	int ret;

	storage_block_size = 0;

	if (!of_property_read_u32(np, "storage_block_func", &id))
		block_base_func_id = id;
	if (!of_property_read_u32(np, "storage_size_func", &id))
		block_size_func_id = id;

	if (!of_property_read_u32(np, "storage_free", &id))
		storage_free_func = id;
	if (!of_property_read_u32(np, "storage_read", &id))
		storage_read_func = id;
	if (!of_property_read_u32(np, "storage_write", &id))
		storage_write_func = id;
	if (!of_property_read_u32(np, "storage_query", &id))
		storage_query_func = id;
	if (!of_property_read_u32(np, "storage_tell", &id))
		storage_tell_func = id;
	if (!of_property_read_u32(np, "storage_status", &id))
		storage_status_func = id;
	if (!of_property_read_u32(np, "storage_verify", &id))
		storage_verify_func = id;
	if (!of_property_read_u32(np, "storage_list", &id))
		storage_list_func = id;
	if (!of_property_read_u32(np, "storage_remove", &id))
		storage_remove_func = id;
	if (!of_property_read_u32(np, "storage_notify_ex", &id))
		storage_notify_ex_func = id;
	if (!of_property_read_u32(np, "storage_set_type", &id))
		storage_type_func = id;
	if (!of_property_read_u32(np, "storage_set_enctype", &id))
		storage_set_enctype_func = id;
	if (!of_property_read_u32(np, "storage_get_enctype", &id))
		storage_get_enctype_func = id;
	if (!of_property_read_u32(np, "storage_version", &id))
		storage_version_func = id;

	if (!storage_block_base || !storage_block_size) {
		storage_service_routine(storage_free_func, 0, NULL, NULL);
		pr_info("probe fail!\n");
		return -1;
	}

	ret = 0;
	pr_info("probe done!\n");
	return ret;
}

static const struct of_device_id securitykey_dt_match[] = {
	{ .compatible = "aml, securitykey" },
	{ .compatible = "amlogic, securitykey" },
	{ /* sentinel */ },
};

static  struct platform_driver securitykey_platform_driver = {
	.probe		= storage_probe,
	.driver		= {
		.owner		= THIS_MODULE,
		.name		= "securitykey",
		.of_match_table	= securitykey_dt_match,
	},
};

static int __init meson_securitykey_init(void)
{
	return  platform_driver_register(&securitykey_platform_driver);
}
module_init(meson_securitykey_init);
