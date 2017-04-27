/*
 * drivers/amlogic/mtd_old/key_old.c
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
#include "key_old.h"
#include "env_old.h"

static struct aml_nand_chip *aml_chip_key;

int aml_nand_key_check(struct mtd_info *mtd)
{
	int ret = 0;
#if 0
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);

	ret = aml_nand_scan_rsv_info(mtd, aml_chip->aml_nandkey_info);
	if ((ret != 0) && ((ret != (-1))))
		pr_info("%s %d\n", __func__, __LINE__);

	if (aml_chip->aml_nandkey_info->valid == 0)
		pr_info("%s %d NO key exist\n", __func__, __LINE__);
#endif
	pr_info("%s() %d\n", __func__, __LINE__);
	return ret;
}

int aml_nand_key_init(struct mtd_info *mtd)
{
	int ret = 0, error;
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	//struct nand_chip *chip = &aml_chip->chip;
	struct aml_nandkey_info_t *key_info;
	int remain_start_block, remain_total_block;
	int remain_block = 0, bad_blk_cnt = 0;
	int phys_erase_shift, max_key_blk;
	loff_t offset;

	aml_chip->aml_nandkey_info =
		kzalloc(sizeof(struct aml_nandkey_info_t),
			GFP_KERNEL);
	if (aml_chip->aml_nandkey_info == NULL) {
		pr_err("%s() %d: ENOMEM\n", __func__, __LINE__);
		return -ENOMEM;
	}
	key_info = aml_chip->aml_nandkey_info;

	key_info->env_init = 0;
	key_info->mtd = mtd;
	/*key_info->env_valid_node->phy_blk_addr = -1;*/


	phys_erase_shift = fls(mtd->erasesize) - 1;
	max_key_blk = (NAND_MINIKEY_PART_SIZE >> phys_erase_shift);
	if (max_key_blk < NAND_MINIKEY_PART_BLOCKNUM)
		max_key_blk = NAND_MINIKEY_PART_BLOCKNUM;

	offset = mtd->size - mtd->erasesize;
	remain_start_block = (int)(offset >> phys_erase_shift);
	remain_total_block = REMAIN_TAIL_BLOCK_NUM;
	key_info->start_block = remain_start_block;
	key_info->end_block = remain_start_block;
	bad_blk_cnt = 0;
	do {
		offset = mtd->erasesize;
		offset *= remain_start_block;
		error = mtd->_block_isbad(mtd, offset);
		if (error) {
			key_info->nand_bbt_info.nand_bbt[bad_blk_cnt++] =
				remain_start_block;
			if (bad_blk_cnt >= MAX_BAD_BLK_NUM) {
				pr_err("bad block too much,%s\n", __func__);
				return -ENOMEM;
			}
			key_info->start_block--;
			remain_start_block--;
			continue;
		}
		remain_start_block--;
	} while (++remain_block < remain_total_block);

	key_info->start_block -= (remain_block-1);

	pr_info("%s()%d: key [%d,%d]\n", __func__, __LINE__,
		key_info->start_block, key_info->end_block);

	return ret;
}

int aml_key_init(struct aml_nand_chip *aml_chip)
{
	int ret = 0;
	struct mtd_info *mtd = aml_chip->mtd;
	/* avoid null */
	aml_chip_key = aml_chip;

	ret = aml_nand_key_init(mtd);
	/* fixme, NOOP key */
	/* storage_ops_read(amlnf_key_read); */
	/* storage_ops_write(amlnf_key_write); */
	return ret;
}

