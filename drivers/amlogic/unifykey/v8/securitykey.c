/*
 * drivers/amlogic/unifykey/v8/securitykey.c
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
#include <linux/of_fdt.h>
#include <linux/module.h>
#include <linux/libfdt_env.h>
#include <linux/of_reserved_mem.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/amlogic/unifykey/security_key.h>
#include <linux/arm-smccc.h>

#undef pr_fmt
#define pr_fmt(fmt) "unifykey: " fmt

static void __iomem *storage_in_base;
static void __iomem *storage_out_base;
static void __iomem *storage_block_base;

static unsigned long phy_storage_in_base;
static unsigned long phy_storage_out_base;
static unsigned long phy_storage_block_base;
static unsigned long storage_block_size;

static unsigned long storage_read_func;
static unsigned long storage_write_func;
static unsigned long storage_query_func;
static unsigned long storage_tell_func;
static unsigned long storage_status_func;
static unsigned long storage_verify_func;
static unsigned long storage_list_func;
static unsigned long storage_remove_func;
static unsigned long storage_set_enctype_func;
static unsigned long storage_get_enctype_func;
static unsigned long storage_version_func;

static DEFINE_SPINLOCK(storage_lock);
static unsigned long lockflags;

static int storage_init_status;

static uint64_t storage_smc_ops(uint64_t func)
{
	struct arm_smccc_res res;

	arm_smccc_smc((unsigned long)func, 0, 0, 0, 0, 0, 0, 0, &res);
	return res.a0;
}

static uint64_t storage_smc_ops2(uint64_t func, uint64_t arg1)
{
	struct arm_smccc_res res;

	arm_smccc_smc((unsigned long)func,
				(unsigned long)arg1,
				0, 0, 0, 0, 0, 0, &res);
	return res.a0;
}

static inline int32_t smc_to_linux_errno(uint64_t errno)
{
	int32_t ret = (int32_t)(errno & 0xffffffff);
	return ret;
}

void *secure_storage_getbuffer(uint32_t *size)
{
	*size = storage_block_size;
	return (void *)storage_block_base;
}

void secure_storage_notifier_ex(uint32_t storagesize)
{
}

void secure_storage_type(uint32_t is_emmc)
{
}

int32_t secure_storage_write(uint8_t *keyname, uint8_t *keybuf,
			uint32_t keylen, uint32_t keyattr)
{
	uint32_t *input;
	uint32_t namelen;
	uint8_t *keydata, *name;
	uint64_t ret;

	spin_lock_irqsave(&storage_lock, lockflags);
	if (storage_init_status == 1) {
		namelen = strlen((const char *)keyname);
		input = (uint32_t *)storage_in_base;
		*input++ = namelen;
		*input++ = keylen;
		*input++ = keyattr;
		name = (uint8_t *)input;
		memcpy(name, keyname, namelen);
		keydata = name + namelen;
		memcpy(keydata, keybuf, keylen);
		ret = storage_smc_ops(storage_write_func);
	} else
		ret = RET_EUND;
	spin_unlock_irqrestore(&storage_lock, lockflags);
	return smc_to_linux_errno(ret);
}

int32_t secure_storage_read(uint8_t *keyname, uint8_t *keybuf,
				uint32_t keylen, uint32_t *readlen)
{
	uint32_t *input = (uint32_t *)storage_in_base;
	uint32_t *output = (uint32_t *)storage_out_base;
	uint32_t namelen;
	uint8_t *name, *buf;
	uint64_t ret;

	spin_lock_irqsave(&storage_lock, lockflags);
	if (storage_init_status == 1) {
		namelen = strlen((const char *)keyname);
		*input++ = namelen;
		*input++ = keylen;
		name = (uint8_t *)input;
		memcpy(name, keyname, namelen);
		ret = storage_smc_ops(storage_read_func);
		if (ret == RET_OK) {
			*readlen = *output;
			buf = (uint8_t *)(output + 1);
			memcpy(keybuf, buf, *readlen);
		}
	} else
		ret = RET_EUND;
	spin_unlock_irqrestore(&storage_lock, lockflags);
	return smc_to_linux_errno(ret);
}

int32_t secure_storage_verify(uint8_t *keyname, uint8_t *hashbuf)
{
	uint32_t *input = (uint32_t *)storage_in_base;
	uint32_t *output = (uint32_t *)storage_out_base;
	uint32_t namelen;
	uint8_t *name;
	uint64_t ret;

	spin_lock_irqsave(&storage_lock, lockflags);
	if (storage_init_status == 1) {
		namelen = strlen((const char *)keyname);
		*input++ = namelen;
		name = (uint8_t *)input;
		memcpy(name, keyname, namelen);
		ret = storage_smc_ops(storage_verify_func);
		if (ret == RET_OK)
			memcpy(hashbuf, (uint8_t *)output, 32);
	} else
		ret = RET_EUND;
	spin_unlock_irqrestore(&storage_lock, lockflags);

	return smc_to_linux_errno(ret);
}

int32_t secure_storage_query(uint8_t *keyname, uint32_t *retval)
{
	uint32_t *input = (uint32_t *)storage_in_base;
	uint32_t *output = (uint32_t *)storage_out_base;
	uint32_t namelen;
	uint8_t *name;
	uint64_t ret;

	spin_lock_irqsave(&storage_lock, lockflags);
	if (storage_init_status == 1) {
		namelen = strlen((const char *)keyname);
		*input++ = namelen;
		name = (uint8_t *)input;
		memcpy(name, keyname, namelen);
		ret = storage_smc_ops(storage_query_func);
		if (ret == RET_OK)
			*retval = *output;
	} else
		ret = RET_EUND;
	spin_unlock_irqrestore(&storage_lock, lockflags);

	return smc_to_linux_errno(ret);
}

int32_t secure_storage_tell(uint8_t *keyname, uint32_t *retval)
{
	uint32_t *input = (uint32_t *)storage_in_base;
	uint32_t *output = (uint32_t *)storage_out_base;
	uint32_t namelen;
	uint8_t *name;
	uint64_t ret;

	spin_lock_irqsave(&storage_lock, lockflags);
	if (storage_init_status == 1) {
		namelen = strlen((const char *)keyname);
		*input++ = namelen;
		name = (uint8_t *)input;
		memcpy(name, keyname, namelen);
		ret = storage_smc_ops(storage_tell_func);
		if (ret == RET_OK)
			*retval = *output;
	} else
		ret = RET_EUND;
	spin_unlock_irqrestore(&storage_lock, lockflags);
	return smc_to_linux_errno(ret);
}

int32_t secure_storage_status(uint8_t *keyname, uint32_t *retval)
{
	uint32_t *input = (uint32_t *)storage_in_base;
	uint32_t *output = (uint32_t *)storage_out_base;
	uint32_t namelen;
	uint8_t *name;
	uint64_t ret;

	spin_lock_irqsave(&storage_lock, lockflags);
	if (storage_init_status == 1) {
		namelen = strlen((const char *)keyname);
		*input++ = namelen;
		name = (uint8_t *)input;
		memcpy(name, keyname, namelen);
		ret = storage_smc_ops(storage_status_func);
		if (ret == RET_OK)
			*retval = *output;
	} else
		ret = RET_EUND;
	spin_unlock_irqrestore(&storage_lock, lockflags);
	return smc_to_linux_errno(ret);
}

int32_t secure_storage_list(uint8_t *listbuf,
		uint32_t buflen, uint32_t *readlen)
{
	uint32_t *output = (uint32_t *)storage_out_base;
	uint64_t ret;

	spin_lock_irqsave(&storage_lock, lockflags);
	if (storage_init_status == 1) {
		ret = storage_smc_ops(storage_list_func);
		if (ret == RET_OK) {
			if (*output > buflen)
				*readlen = buflen;
			else
				*readlen = *output;
			memcpy(listbuf, (uint8_t *)(output+1), *readlen);
		}
	} else
		ret = RET_EUND;
	spin_unlock_irqrestore(&storage_lock, lockflags);
	return smc_to_linux_errno(ret);
}

int32_t secure_storage_remove(uint8_t *keyname)
{
	uint32_t *input = (uint32_t *)storage_in_base;
	uint32_t namelen;
	uint8_t *name;
	uint64_t ret;

	spin_lock_irqsave(&storage_lock, lockflags);
	if (storage_init_status == 1) {
		namelen = strlen((const char *)keyname);
		*input++ = namelen;
		name = (uint8_t *)input;
		memcpy(name, keyname, namelen);
		ret = storage_smc_ops(storage_remove_func);
	} else
		ret = RET_EUND;
	spin_unlock_irqrestore(&storage_lock, lockflags);
	return smc_to_linux_errno(ret);
}

int32_t secure_storage_set_enctype(uint32_t type)
{
	uint64_t ret;

	spin_lock_irqsave(&storage_lock, lockflags);
	ret = storage_smc_ops2(storage_set_enctype_func, type);
	spin_unlock_irqrestore(&storage_lock, lockflags);
	return smc_to_linux_errno(ret);
}
int32_t secure_storage_get_enctype(void)
{
	uint64_t ret;

	spin_lock_irqsave(&storage_lock, lockflags);
	ret = storage_smc_ops(storage_get_enctype_func);
	spin_unlock_irqrestore(&storage_lock, lockflags);
	return smc_to_linux_errno(ret);
}
int32_t secure_storage_version(void)
{
	uint64_t ret;

	spin_lock_irqsave(&storage_lock, lockflags);
	ret = storage_smc_ops(storage_version_func);
	spin_unlock_irqrestore(&storage_lock, lockflags);
	return smc_to_linux_errno(ret);
}

static int storage_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	unsigned int id;
	int ret;

	phy_storage_in_base = 0;
	phy_storage_out_base = 0;
	phy_storage_block_base = 0;
	storage_block_size = 0;

	if (!of_property_read_u32(np, "storage_in_func", &id))
		phy_storage_in_base = storage_smc_ops(id);
	if (!of_property_read_u32(np, "storage_out_func", &id))
		phy_storage_out_base = storage_smc_ops(id);
	if (!of_property_read_u32(np, "storage_block_func", &id))
		phy_storage_block_base = storage_smc_ops(id);
	if (!of_property_read_u32(np, "storage_size_func", &id))
		storage_block_size = storage_smc_ops(id);

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
	if (!of_property_read_u32(np, "storage_set_enctype", &id))
		storage_set_enctype_func = id;
	if (!of_property_read_u32(np, "storage_get_enctype", &id))
		storage_get_enctype_func = id;
	if (!of_property_read_u32(np, "storage_version", &id))
		storage_version_func = id;

	if (!phy_storage_in_base || !phy_storage_out_base
			|| !phy_storage_block_base || !storage_block_size) {
		pr_info("probe fail!\n");
		return -1;
	}

	if (phy_storage_in_base == SMC_UNK
			|| phy_storage_out_base == SMC_UNK
			|| phy_storage_block_base == SMC_UNK
			|| storage_block_size == SMC_UNK) {
		storage_in_base = NULL;
		storage_out_base = NULL;
		storage_block_base = NULL;
	}	else {
		if (pfn_valid(__phys_to_pfn(phy_storage_in_base)))
			storage_in_base = (void __iomem *)
				__phys_to_virt(phy_storage_in_base);
		else
			storage_in_base = ioremap_cache(
				phy_storage_in_base, storage_block_size);

		if (pfn_valid(__phys_to_pfn(phy_storage_out_base)))
			storage_out_base = (void __iomem *)
				__phys_to_virt(phy_storage_out_base);
		else
			storage_out_base = ioremap_cache(
				phy_storage_out_base, storage_block_size);

		if (pfn_valid(__phys_to_pfn(phy_storage_block_base)))
			storage_block_base = (void __iomem *)
				__phys_to_virt(phy_storage_block_base);
		else
			storage_block_base = ioremap_cache(
				phy_storage_block_base, storage_block_size);
	}

	pr_info("storage in base: 0x%lx\n", (long)storage_in_base);
	pr_info("storage out base: 0x%lx\n", (long)storage_out_base);
	pr_info("storage block base: 0x%lx\n", (long)storage_block_base);

	ret = 0;
	if (!storage_in_base || !storage_out_base || !storage_block_base)
		ret = -1;
	if (ret == 0)
		storage_init_status = 1;
	else
		storage_init_status = -1;

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
