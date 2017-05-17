/*
 * drivers/amlogic/mtd/aml_key.c
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

#include "aml_mtd.h"

#ifndef AML_NAND_UBOOT
#include <linux/amlogic/unifykey/key_manage.h>
#endif


static struct aml_nand_chip *aml_chip_key;

/*
 * This function reads the u-boot keys.
 */
int32_t amlnf_key_read(uint8_t *buf, uint32_t len, uint32_t *actual_length)
{
	struct aml_nand_chip *aml_chip = aml_chip_key;
	struct nand_menson_key *key_ptr = NULL;
	u32 keysize = aml_chip->keysize - sizeof(u32);
	size_t offset = 0;
	struct mtd_info *mtd = aml_chip->mtd;

	*actual_length = keysize;

	if (aml_chip == NULL) {
		pr_info("%s(): amlnf key not ready yet!",
			__func__);
		return -EFAULT;
	}

	if (len > keysize) {
		/*
		 *No return here! keep consistent, should memset zero
		 *for the rest.
		 */
		pr_info("%s key data len too much\n",
			__func__);
		memset(buf + keysize, 0, len - keysize);
	}

	key_ptr = kzalloc(aml_chip->keysize, GFP_KERNEL);
	if (key_ptr == NULL)
		return -ENOMEM;

	aml_nand_read_key(mtd, offset, (u8 *)key_ptr->data);

	memcpy(buf, key_ptr->data, keysize);

	kfree(key_ptr);
	return 0;
}

/*
 * This function write the keys.
 */
int32_t amlnf_key_write(uint8_t *buf, uint32_t len, uint32_t *actual_length)
{
	struct aml_nand_chip *aml_chip = aml_chip_key;
	struct mtd_info *mtd = aml_chip->mtd;
	struct nand_menson_key *key_ptr = NULL;
	u32 keysize = aml_chip->keysize - sizeof(u32);
	int error = 0;

	if (aml_chip == NULL) {
		pr_info("%s(): amlnf key not ready yet!", __func__);
		return -EFAULT;
	}

	if (len > keysize) {
		/*
		 *No return here! keep consistent, should memset zero
		 *for the rest.
		 */
		pr_info("key data len too much,%s\n", __func__);
		memset(buf + keysize, 0, len - keysize);
	}

	key_ptr = kzalloc(aml_chip->keysize, GFP_KERNEL);
	if (key_ptr == NULL)
		return -ENOMEM;

	memcpy(key_ptr->data + 0, buf, keysize);
	aml_nand_save_key(mtd, buf);

	kfree(key_ptr);
	return error;
}

int amlnf_key_erase(void)
{
	int ret = 0;

	if (aml_chip_key == NULL) {
		pr_info("%s amlnf not ready yet!\n", __func__);
		return -1;
	}

	return ret;
}

int aml_key_init(struct aml_nand_chip *aml_chip)
{
	int ret = 0;

	/* avoid null */
	aml_chip_key = aml_chip;
	storage_ops_read(amlnf_key_read);
	storage_ops_write(amlnf_key_write);
	return ret;
}

int aml_nand_update_key(struct aml_nand_chip *aml_chip, char *key_ptr)
{
	int malloc_flag = 0;
	char *key_buf = NULL;

	if (key_buf == NULL) {
		key_buf = kzalloc(aml_chip->keysize, GFP_KERNEL);
		malloc_flag = 1;
		if (key_buf == NULL)
			return -ENOMEM;
		memset(key_buf, 0, aml_chip->keysize);
	} else
		key_buf = key_ptr;

	if (malloc_flag && (key_buf)) {
		kfree(key_buf);
		key_buf = NULL;
	}
	return 0;
}
