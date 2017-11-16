/*
 * drivers/amlogic/mmc/emmc_key.c
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

#include <linux/types.h>
#include <linux/err.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/slab.h>
#include <linux/mmc/card.h>
#include <linux/mmc/emmc_partitions.h>
#include <linux/amlogic/unifykey/key_manage.h>
#include "emmc_key.h"

#define		EMMC_BLOCK_SIZE		(0x100)
#define		MAX_EMMC_BLOCK_SIZE	(128*1024)

/*
 * kernel head file
 *
 */
static struct mmc_card *mmc_card_key;
static struct aml_emmckey_info_t *emmckey_info;

static int aml_emmc_key_check(void)
{
	u8 keypart_cnt;
	u64 part_size;
	struct emmckey_valid_node_t *emmckey_valid_node, *temp_valid_node;

	emmckey_info->key_part_count =
		emmckey_info->keyarea_phy_size / EMMC_KEYAREA_SIZE;

	if (emmckey_info->key_part_count
			> EMMC_KEYAREA_COUNT) {
		emmckey_info->key_part_count = EMMC_KEYAREA_COUNT;
	}
	keypart_cnt = 0;
	part_size = EMMC_KEYAREA_SIZE;
	do {
		emmckey_valid_node = kmalloc(
			sizeof(*emmckey_valid_node), GFP_KERNEL);

		if (emmckey_valid_node == NULL) {
			pr_info("%s:%d,kmalloc memory fail\n",
				__func__, __LINE__);
			return -ENOMEM;
		}
		emmckey_valid_node->phy_addr = emmckey_info->keyarea_phy_addr
						+ part_size * keypart_cnt;
		emmckey_valid_node->phy_size = EMMC_KEYAREA_SIZE;
		emmckey_valid_node->next = NULL;
		emmckey_info->key_valid = 0;
		if (emmckey_info->key_valid_node == NULL) {

			emmckey_info->key_valid_node = emmckey_valid_node;

		} else{
			temp_valid_node = emmckey_info->key_valid_node;

			while (temp_valid_node->next != NULL)
				temp_valid_node = temp_valid_node->next;

			temp_valid_node->next = emmckey_valid_node;
		}
	} while (++keypart_cnt < emmckey_info->key_part_count);

	emmckey_info->key_valid = 1;
	return 0;
}

int32_t emmc_key_read(uint8_t *buffer,
	uint32_t length, uint32_t *actual_length)
{
	int ret;
	u64  addr = 0;
	u32  size = 0;
	int blk, cnt;
	unsigned char *dst = NULL;
	struct mmc_card *card = mmc_card_key;
	int bit = card->csd.read_blkbits;

	size = length;
	*actual_length = length;
	addr = get_reserve_partition_off_from_tbl() + EMMCKEY_RESERVE_OFFSET;
	blk = addr >> bit;
	cnt = size >> bit;
	dst = (unsigned char *)buffer;
	mmc_claim_host(card->host);
	do {
		ret = mmc_read_internal(card, blk, EMMC_BLOCK_SIZE, dst);
		if (ret) {
			pr_err("%s [%d] mmc_write_internal error\n",
				__func__, __LINE__);
			return ret;
		}
		blk += EMMC_BLOCK_SIZE;
		cnt -= EMMC_BLOCK_SIZE;
		dst = (unsigned char *)buffer + MAX_EMMC_BLOCK_SIZE;
	} while (cnt != 0);
	pr_info("%s:%d, read %s\n", __func__, __LINE__, (ret) ? "error":"ok");

	mmc_release_host(card->host);
	return ret;
}
EXPORT_SYMBOL(emmc_key_read);

int32_t emmc_key_write(uint8_t *buffer,
	uint32_t length, uint32_t *actual_length)
{
	int ret;
	u64  addr = 0;
	u32  size = 0;
	int blk, cnt;
	unsigned char *src = NULL;
	struct mmc_card *card = mmc_card_key;
	int bit = card->csd.read_blkbits;

	size = length;
	addr = get_reserve_partition_off_from_tbl() + EMMCKEY_RESERVE_OFFSET;
	blk = addr >> bit;
	cnt = size >> bit;
	src = (unsigned char *)buffer;
	mmc_claim_host(card->host);
	do {
		ret = mmc_write_internal(card, blk, EMMC_BLOCK_SIZE, src);
		if (ret) {
			pr_err("%s [%d] mmc_write_internal error\n",
				__func__, __LINE__);
			return ret;
		}
		blk += EMMC_BLOCK_SIZE;
		cnt -= EMMC_BLOCK_SIZE;
		src = (unsigned char *)buffer + MAX_EMMC_BLOCK_SIZE;
	} while (cnt != 0);
	pr_info("%s:%d, write %s\n", __func__, __LINE__, (ret) ? "error":"ok");
	mmc_release_host(card->host);
	return ret;
}
EXPORT_SYMBOL(emmc_key_write);

int emmc_key_init(struct mmc_card *card)
{
	u64  addr = 0;
	u32  size = 0;
	u64  lba_start = 0, lba_end = 0;
	int err = 0;
	int bit = card->csd.read_blkbits;

	pr_info("card key: card_blk_probe.\n");
	emmckey_info = kmalloc(sizeof(*emmckey_info), GFP_KERNEL);
	if (emmckey_info == NULL) {
		pr_info("%s:%d,kmalloc memory fail\n", __func__, __LINE__);
		return -ENOMEM;
	}
	memset(emmckey_info, 0, sizeof(*emmckey_info));
	emmckey_info->key_init = 0;

	size = EMMCKEY_AREA_PHY_SIZE;
	if (get_reserve_partition_off_from_tbl() < 0) {
		err = -EINVAL;
		goto exit_err;
	}
	addr = get_reserve_partition_off_from_tbl() + EMMCKEY_RESERVE_OFFSET;
	lba_start = addr >> bit;
	lba_end = (addr + size) >> bit;
	emmckey_info->key_init = 1;

	pr_info("%s:%d emmc key lba_start:0x%llx,lba_end:0x%llx\n",
	 __func__, __LINE__, lba_start, lba_end);

	if (!emmckey_info->key_init) {
		err = -EINVAL;

		pr_info("%s:%d,emmc key init fail\n", __func__, __LINE__);
		goto exit_err;
	}
	emmckey_info->keyarea_phy_addr = addr;
	emmckey_info->keyarea_phy_size = size;
	emmckey_info->lba_start = lba_start;
	emmckey_info->lba_end   = lba_end;
	mmc_card_key = card;
	err = aml_emmc_key_check();
	if (err) {
		pr_info("%s:%d,emmc key check fail\n", __func__, __LINE__);
	goto exit_err;
	}

	storage_ops_read(emmc_key_read);
	storage_ops_write(emmc_key_write);

	pr_info("emmc key: %s:%d ok.\n", __func__, __LINE__);
	return err;

exit_err:
	kfree(emmckey_info);
	return err;
}

