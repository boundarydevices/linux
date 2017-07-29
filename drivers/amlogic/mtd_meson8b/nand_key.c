/*
 * drivers/amlogic/mtd_meson8b/nand_key.c
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

/* #include "key_old.h" */
#include "aml_mtd.h"
#include "env_old.h"
#include "linux/crc32.h"
#include <linux/amlogic/unifykey/key_manage.h>

#define KEYSIZE (CONFIG_KEYSIZE - (sizeof(uint32_t)))

/*error code*/
#define NAND_KEY_RD_ERR                 2
#define NAND_KEY_CONFIG_ERR         3

#define REMAIN_TAIL_BLOCK_NUM   8


/* static struct aml_nand_chip *aml_chip_key; */
static int default_keyironment_size =
	(KEYSIZE - sizeof(struct aml_nand_bbt_info));

struct mesonkey_t {
	uint32_t crc;/* CRC32 over data bytes */
	uint8_t data[KEYSIZE];/* Environment data */
};

static struct env_free_node_t *alloc_fn(void)
{
	struct env_free_node_t *fn;

	fn = kzalloc(sizeof(struct env_free_node_t), GFP_KERNEL);
	return fn;
}

static int aml_nand_read_key(struct mtd_info *mtd,
	uint64_t offset, u_char *buf)
{
	struct env_oobinfo_t *key_oobinfo;
	int error = 0;
	uint64_t addr = offset;
	size_t amount_loaded = 0;
	size_t len;
	struct mtd_oob_ops oob_ops;
	struct mtd_oob_region aml_oob_region;
	uint8_t *data_buf;
	uint8_t key_oob_buf[sizeof(struct env_oobinfo_t)];

	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);

	if (!aml_chip->aml_nandkey_info->env_valid)
		return 1;

	data_buf = kzalloc(mtd->writesize, GFP_KERNEL);
	if (data_buf == NULL)
		return -ENOMEM;

	key_oobinfo = (struct env_oobinfo_t *)key_oob_buf;

	error = mtd->ooblayout->free(mtd, 0,
		(struct mtd_oob_region *)&aml_oob_region);
	if (error != 0) {
		pr_err("read oob free failed %s() %d\n",
			__func__, __LINE__);
		error = NAND_KEY_RD_ERR;
		goto exit;
	}

	while (amount_loaded < CONFIG_KEYSIZE) {

		oob_ops.mode = MTD_OPS_AUTO_OOB;
		oob_ops.len = mtd->writesize;
		oob_ops.ooblen = sizeof(struct env_oobinfo_t);
		oob_ops.ooboffs = aml_oob_region.offset;
		oob_ops.datbuf = data_buf;
		oob_ops.oobbuf = key_oob_buf;

		memset((uint8_t *)oob_ops.datbuf, 0x0, mtd->writesize);
		memset((uint8_t *)oob_ops.oobbuf, 0x0, oob_ops.ooblen);

		error = mtd->_read_oob(mtd, addr, &oob_ops);
		if ((error != 0) && (error != -EUCLEAN)) {
			pr_err("blk check failed: %llx, %d\n",
				(uint64_t)addr, error);
			error = NAND_KEY_RD_ERR;
			goto exit;
		}

		if (memcmp(key_oobinfo->name, ENV_KEY_MAGIC, 4))
			pr_err("invalid key magic: %llx\n", (uint64_t)addr);

		addr += mtd->writesize;
		len = min(mtd->writesize, CONFIG_KEYSIZE - amount_loaded);
		memcpy(buf + amount_loaded, data_buf, len);
		amount_loaded += mtd->writesize;
	}
	if (amount_loaded < CONFIG_KEYSIZE) {
		error = NAND_KEY_CONFIG_ERR;
		pr_err("key size err, w:%d,c %d, %s\n",
			mtd->writesize, CONFIG_KEYSIZE, __func__);
		goto exit;
	}
exit:
	kfree(data_buf);
	return error;
}


static int aml_nand_write_key(struct mtd_info *mtd,
	uint64_t offset, u_char *buf)
{
	struct env_oobinfo_t *key_oobinfo;
	int error = 0;
	uint64_t addr = 0;
	size_t amount_saved = 0;
	size_t len;
	struct mtd_oob_ops oob_ops;
	struct mtd_oob_region oob_reg;
	uint8_t *data_buf;
	uint8_t key_oob_buf[sizeof(struct env_oobinfo_t)];

	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);

	data_buf = kzalloc(mtd->writesize, GFP_KERNEL);
	if (data_buf == NULL)
		return -ENOMEM;

	addr = offset;
	key_oobinfo = (struct env_oobinfo_t *)key_oob_buf;
	memcpy(key_oobinfo->name, ENV_KEY_MAGIC, 4);
	key_oobinfo->ec = aml_chip->aml_nandkey_info->env_valid_node->ec;
	key_oobinfo->timestamp =
		aml_chip->aml_nandkey_info->env_valid_node->timestamp;
	key_oobinfo->status_page = 1;

	error = mtd->ooblayout->free(mtd, 0, (struct mtd_oob_region *)&oob_reg);
	if (error != 0) {
		pr_err("read oob free failed: %s() %d\n", __func__, __LINE__);
		error = 1;
		goto exit;
	}

	while (amount_saved < CONFIG_KEYSIZE) {

		oob_ops.mode = MTD_OPS_AUTO_OOB;
		oob_ops.len = mtd->writesize;
		oob_ops.ooblen = sizeof(struct env_oobinfo_t);
		oob_ops.ooboffs = oob_reg.offset;
		oob_ops.datbuf = data_buf;
		oob_ops.oobbuf = key_oob_buf;

		memset((uint8_t *)oob_ops.datbuf, 0x0, mtd->writesize);
		len = min(mtd->writesize, CONFIG_KEYSIZE - amount_saved);
		memcpy((uint8_t *)oob_ops.datbuf, buf + amount_saved, len);

		error = mtd->_write_oob(mtd, addr, &oob_ops);
		if (error) {
			pr_err("blk write failed: %llx, %d\n",
				(uint64_t)addr, error);
			error = 1;
			goto exit;
		}

		addr += mtd->writesize;
		amount_saved += mtd->writesize;
	}
	if (amount_saved < CONFIG_KEYSIZE) {
		error = 1;
		pr_err("amount_saved < CONFIG_KEYSIZE, %s\n", __func__);
		goto exit;
	}

	aml_chip->aml_nandkey_info->env_valid = 1;
exit:
	kfree(data_buf);
	return error;
}

static int aml_nand_get_key(struct mtd_info *mtd, u_char *buf)
{
	struct aml_nand_bbt_info *nand_bbt_info;
	int error = 0, flag = 0;
	uint64_t addr = 0;
	struct mesonkey_t *key_ptr = (struct mesonkey_t *)buf;
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	struct env_valid_node_t *cur_valid_node, *tail_valid_node;

	cur_valid_node = aml_chip->aml_nandkey_info->env_valid_node;
	tail_valid_node = cur_valid_node->next;
	do {
		while (tail_valid_node != NULL) {
			if (cur_valid_node->rd_flag == NAND_KEY_RD_ERR) {
				cur_valid_node = tail_valid_node;
				tail_valid_node = tail_valid_node->next;
			} else
				break;
		}
		if (cur_valid_node->rd_flag == NAND_KEY_RD_ERR) {
			error = NAND_KEY_RD_ERR;
			pr_err("no valid block get key,%s\n", __func__);
			goto exit;
		}

		addr = cur_valid_node->phy_blk_addr;
		addr *= mtd->erasesize;
		addr += cur_valid_node->phy_page_addr * mtd->writesize;
		pr_info("read:addr:0x%llx,phy_blk_addr:%d,phy_page_addr:%d,%s:%d\n",
			addr,
			cur_valid_node->phy_blk_addr,
			cur_valid_node->phy_page_addr,
			__func__, __LINE__);

		error = aml_nand_read_key(mtd, addr, (u_char *)key_ptr);
		if (error) {
			pr_err("nand key read fail,%s\n", __func__);
			if (error == NAND_KEY_RD_ERR) {
				cur_valid_node->rd_flag = NAND_KEY_RD_ERR;
				flag = 1;
			} else
				goto exit;
		} else
			flag = 0;
	} while (flag);
	nand_bbt_info = &aml_chip->aml_nandkey_info->nand_bbt_info;
	memcpy(nand_bbt_info->bbt_head_magic,
		key_ptr->data + default_keyironment_size,
		sizeof(struct aml_nand_bbt_info));
exit:
	return error;
}


static int aml_nand_save_key(struct mtd_info *mtd, u_char *buf)
{
	struct aml_nand_bbt_info *nand_bbt_info;
	struct env_free_node_t *env_free_node, *key_tmp_node;
	int error = 0, pages_per_blk, i = 1;
	uint64_t addr = 0;
	struct erase_info aml_key_erase_info;
	struct mesonkey_t *key_ptr = (struct mesonkey_t *)buf;
	int group_block_count = 0;
	int group_max_block = NAND_MINIKEY_PART_BLOCKNUM;
	struct env_valid_node_t *tmp_valid_node, *tail_valid_node;
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	struct aml_nandkey_info_t *keyinfo;

	if (!aml_chip->aml_nandkey_info->env_init)
		return 1;

	keyinfo = aml_chip->aml_nandkey_info;
	pages_per_blk = mtd->erasesize / mtd->writesize;
	if ((mtd->writesize < CONFIG_KEYSIZE) &&
		(keyinfo->env_valid == 1))
		i = (CONFIG_KEYSIZE + mtd->writesize - 1) / mtd->writesize;

	if (keyinfo->env_valid) {
		keyinfo->env_valid_node->phy_page_addr += i;
		tail_valid_node = keyinfo->env_valid_node->next;
		while (tail_valid_node) {
			tail_valid_node->phy_page_addr += i;
			tail_valid_node = tail_valid_node->next;
		}

		if ((keyinfo->env_valid_node->phy_page_addr
			+ i) > pages_per_blk) {

			env_free_node = alloc_fn();
			if (env_free_node == NULL)
				return -ENOMEM;

			env_free_node->phy_blk_addr =
				keyinfo->env_valid_node->phy_blk_addr;
			env_free_node->ec = keyinfo->env_valid_node->ec;
			key_tmp_node = keyinfo->env_free_node;
			if (keyinfo->env_free_node == NULL)
				keyinfo->env_free_node = env_free_node;
			else {
				while (key_tmp_node->next != NULL)
					key_tmp_node = key_tmp_node->next;
				key_tmp_node->next = env_free_node;
			}
			tail_valid_node = keyinfo->env_valid_node->next;
			while (tail_valid_node) {
				env_free_node = alloc_fn();
				if (env_free_node == NULL)
					return -ENOMEM;

				env_free_node->phy_blk_addr =
					tail_valid_node->phy_blk_addr;
				env_free_node->ec = tail_valid_node->ec;
				keyinfo->env_valid_node->next =
					tail_valid_node->next;
				kfree(tail_valid_node);
				tail_valid_node =
					keyinfo->env_valid_node->next;

				key_tmp_node = keyinfo->env_free_node;
				while (key_tmp_node->next != NULL)
					key_tmp_node = key_tmp_node->next;
				key_tmp_node->next = env_free_node;
			}

			key_tmp_node = keyinfo->env_free_node;
			keyinfo->env_valid_node->phy_blk_addr =
				key_tmp_node->phy_blk_addr;
			keyinfo->env_valid_node->phy_page_addr = 0;
			keyinfo->env_valid_node->ec = key_tmp_node->ec;
			keyinfo->env_valid_node->timestamp += 1;
			keyinfo->env_valid_node->next = NULL;
			keyinfo->env_free_node = key_tmp_node->next;
			kfree(key_tmp_node);

			group_block_count++;
			tail_valid_node = keyinfo->env_valid_node;
			key_tmp_node = keyinfo->env_free_node;
			while (key_tmp_node &&
				group_block_count < group_max_block) {
				tmp_valid_node =
					kzalloc(sizeof(struct env_valid_node_t),
						GFP_KERNEL);
				if (tmp_valid_node == NULL)
					return -ENOMEM;

				tmp_valid_node->ec = key_tmp_node->ec;
				tmp_valid_node->phy_blk_addr =
					key_tmp_node->phy_blk_addr;
				tmp_valid_node->phy_page_addr = 0;
				tmp_valid_node->timestamp += 1;
				tmp_valid_node->next = NULL;
				keyinfo->env_free_node = key_tmp_node->next;
				kfree(key_tmp_node);
				key_tmp_node = keyinfo->env_free_node;
				while (tail_valid_node->next != NULL)
					tail_valid_node = tail_valid_node->next;
				tail_valid_node->next = tmp_valid_node;
				group_block_count++;
			}
		}
	} else {

		key_tmp_node = keyinfo->env_free_node;
		keyinfo->env_valid_node->phy_blk_addr =
			key_tmp_node->phy_blk_addr;
		keyinfo->env_valid_node->phy_page_addr = 0;
		keyinfo->env_valid_node->ec = key_tmp_node->ec;
		keyinfo->env_valid_node->timestamp += 1;
		keyinfo->env_valid_node->next = NULL;
		keyinfo->env_free_node = key_tmp_node->next;
		kfree(key_tmp_node);
		group_block_count++;
		tail_valid_node = keyinfo->env_valid_node;
		key_tmp_node = keyinfo->env_free_node;
		while (key_tmp_node &&
			group_block_count < group_max_block) {
			tmp_valid_node =
				kzalloc(sizeof(struct env_valid_node_t),
				GFP_KERNEL);
			tmp_valid_node->ec = key_tmp_node->ec;
			tmp_valid_node->phy_blk_addr =
				key_tmp_node->phy_blk_addr;
			tmp_valid_node->phy_page_addr = 0;
			tmp_valid_node->timestamp += 1;
			tmp_valid_node->next = NULL;
			keyinfo->env_free_node = key_tmp_node->next;
			kfree(key_tmp_node);
			key_tmp_node = keyinfo->env_free_node;
			while (tail_valid_node->next != NULL)
				tail_valid_node = tail_valid_node->next;
			tail_valid_node->next = tmp_valid_node;
			group_block_count++;
		}
	}

	tail_valid_node = keyinfo->env_valid_node;
	while (tail_valid_node != NULL) {
		addr = tail_valid_node->phy_blk_addr;
		addr *= mtd->erasesize;
		addr += tail_valid_node->phy_page_addr *
			mtd->writesize;
		pr_info(
			"write:addr:0x%llx,phy_blk_addr:%d,phy_page_addr:%d,%s:%d\n",
			addr,
			tail_valid_node->phy_blk_addr,
			tail_valid_node->phy_page_addr,
			__func__, __LINE__);

		if (tail_valid_node->phy_page_addr == 0) {
			memset(&aml_key_erase_info, 0,
				sizeof(struct erase_info));
			aml_key_erase_info.mtd = mtd;
			aml_key_erase_info.addr = addr;
			aml_key_erase_info.len = mtd->erasesize;
			error = mtd->_erase(mtd, &aml_key_erase_info);
			if (error) {
				pr_err("key free blk erase failed %d\n", error);
				mtd->_block_markbad(mtd, addr);
				return error;
			}
			tail_valid_node->ec++;
		}
		nand_bbt_info = &keyinfo->nand_bbt_info;
		if ((!memcmp(nand_bbt_info->bbt_head_magic,
			BBT_HEAD_MAGIC, 4)) &&
			(!memcmp(nand_bbt_info->bbt_tail_magic,
			BBT_TAIL_MAGIC, 4))) {

			memcpy(key_ptr->data + default_keyironment_size,
				keyinfo->nand_bbt_info.bbt_head_magic,
				sizeof(struct aml_nand_bbt_info));
			key_ptr->crc =
				(crc32((0 ^ 0xffffffffL),
				key_ptr->data, KEYSIZE) ^ 0xffffffffL);
		}

		if (aml_nand_write_key(mtd,
			addr, (u_char *) key_ptr)) {
			pr_err("update nand key FAILED!\n");
			return 1;
		}
		tail_valid_node = tail_valid_node->next;
	}

	return error;
}

static struct env_free_node_t *get_free_tail(struct env_free_node_t *head)
{
	struct env_free_node_t *fn = head;

	while (fn->next != NULL)
		fn = fn->next;
	return fn;
}

static int aml_nand_key_init(struct mtd_info *mtd)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	struct nand_chip *chip = &aml_chip->chip;
	struct env_oobinfo_t *key_oobinfo;
	/* free_node; */
	struct env_free_node_t *fn;
	/* tmp_node; */
	struct env_free_node_t *tn;
	struct env_free_node_t *key_prev_node;
	struct aml_nandkey_info_t *k_info;
	#ifdef NAND_KEY_SAVE_MULTI_BLOCK
	struct env_valid_node_t *env_valid_node;
	struct env_valid_node_t *tmp_valid_node;
	struct env_free_node_t  *multi_free_node;
	struct env_free_node_t  *free_tmp_node;
	int have_env_free_node_flag;
	#endif
	struct mtd_oob_region aml_oob_region;
	int error = 0, start_blk, key_blk, i;
	int pages_per_blk, bad_blk_cnt = 0;
	int max_key_blk, phys_erase_shift;
	uint64_t offset;
	uint8_t *data_buf;
	struct mtd_oob_ops aml_oob_ops;
	uint8_t key_oob_buf[sizeof(struct env_oobinfo_t)];
	int remain_block = 0;
	int remain_start_block;
	int remain_total_blk;
	int key_end;
	uint32_t env_node_size;

	data_buf = kzalloc(mtd->writesize, GFP_KERNEL);
	if (data_buf == NULL)
		return -ENOMEM;

	aml_chip->aml_nandkey_info =
		kzalloc(sizeof(struct aml_nandkey_info_t), GFP_KERNEL);

	if (aml_chip->aml_nandkey_info == NULL) {
		kfree(data_buf);
		data_buf = NULL;
		return -ENOMEM;
	}
	env_node_size = sizeof(struct env_free_node_t);
	k_info = aml_chip->aml_nandkey_info;
	k_info->env_init = 0;
	k_info->mtd = mtd;
	k_info->env_valid_node =
		kzalloc(sizeof(struct env_valid_node_t), GFP_KERNEL);
	if (k_info->env_valid_node == NULL) {
		kfree(data_buf);
		data_buf = NULL;
		kfree(k_info);
		k_info = NULL;
		return -ENOMEM;
	}
	k_info->env_valid_node->phy_blk_addr = -1;

	phys_erase_shift = fls(mtd->erasesize) - 1;
	max_key_blk = (NAND_MINIKEY_PART_SIZE >> phys_erase_shift);
	if (max_key_blk < NAND_MINIKEY_PART_BLOCKNUM)
		max_key_blk = NAND_MINIKEY_PART_BLOCKNUM;

#ifdef NEW_NAND_SUPPORT
	if ((aml_chip->new_nand_info.type)
		&& (aml_chip->new_nand_info.type < 10))
		offset += RETRY_NAND_BLK_NUM * mtd->erasesize;
#endif

	/*
	 *start_blk = (int)(offset >> phys_erase_shift);
	 *total_blk = (int)(mtd->size >> phys_erase_shift);
	 *aml_chip->aml_nandkey_info->start_block=start_blk;
	 *pr_info("start_blk=%d\n",aml_chip->aml_nandkey_info->start_block);
	 *aml_chip->aml_nandkey_info->end_block=start_blk;
	 */
	pages_per_blk = (1 <<
	(chip->phys_erase_shift - chip->page_shift));
	key_oobinfo = (struct env_oobinfo_t *)key_oob_buf;
	/*
	 *if ((default_keyironment_size
	 * + sizeof(struct aml_nand_bbt_info)) > KEYSIZE)
	 *total_blk = start_blk + max_key_blk;
	 */

#define REMAIN_TAIL_BLOCK_NUM       8
	offset = mtd->size - mtd->erasesize;
	remain_start_block =
		(int)(offset >> phys_erase_shift);
	remain_total_blk = REMAIN_TAIL_BLOCK_NUM;
	k_info->start_block = remain_start_block;
	k_info->end_block = remain_start_block;
	bad_blk_cnt = 0;
	do {
		offset = mtd->erasesize;
		offset *= remain_start_block;
		error = mtd->_block_isbad(mtd, offset);
		if (error) {
			k_info->nand_bbt_info.nand_bbt[bad_blk_cnt++]
				= remain_start_block;
			if (bad_blk_cnt >= MAX_BAD_BLK_NUM) {
				pr_err("bad block too much,%s\n", __func__);
				return -ENOMEM;
			}
			k_info->start_block--;
			remain_start_block--;
			continue;
		}
		remain_start_block--;
	} while (++remain_block < remain_total_blk);
	k_info->start_block -= (remain_block - 1);
	pr_info(
		"key start_blk=%d,end_blk=%d,%s:%d\n",
		k_info->start_block,
		k_info->end_block, __func__, __LINE__);

	key_blk = 0;
	start_blk = k_info->start_block;

	error = mtd->ooblayout->free(mtd, 0,
		(struct mtd_oob_region *)&aml_oob_region);
	if (error != 0) {
		pr_err(" oob free failed: %s() %d\n",
		__func__, __LINE__);
		return -ERANGE;
	}
	key_end = remain_total_blk + k_info->start_block;
	do {
		offset = mtd->erasesize;
		offset *= start_blk;
		error = mtd->_block_isbad(mtd, offset);
		if (error) {
			start_blk++;
			continue;
		}
		aml_oob_ops.mode = MTD_OPS_AUTO_OOB;
		aml_oob_ops.len = mtd->writesize;
		aml_oob_ops.ooblen = sizeof(struct env_oobinfo_t);
		aml_oob_ops.ooboffs = aml_oob_region.offset;
		/* aml_oob_ops.ooboffs = mtd->ecclayout->oobfree[0].offset; */
		aml_oob_ops.datbuf = data_buf;
		aml_oob_ops.oobbuf = key_oob_buf;
		memset((uint8_t *)aml_oob_ops.datbuf,
			0x0, mtd->writesize);
		memset((uint8_t *)aml_oob_ops.oobbuf,
			0x0, aml_oob_ops.ooblen);

		error = mtd->_read_oob(mtd, offset, &aml_oob_ops);
		if ((error != 0) && (error != -EUCLEAN)) {
			pr_err(
			"blk check good but read failed: %llx, %d\n",
			(uint64_t)offset, error);
			continue;
		}

		k_info->env_init = 1;
		if (!memcmp(key_oobinfo->name, ENV_KEY_MAGIC, 4)) {
			k_info->env_valid = 1;
			if (k_info->env_valid_node->phy_blk_addr >= 0) {
				fn = alloc_fn();
				if (fn == NULL)
					return -ENOMEM;

				fn->dirty_flag = 1;
				have_env_free_node_flag = 0;
				if (key_oobinfo->timestamp >
					k_info->env_valid_node->timestamp) {

					fn->phy_blk_addr =
					k_info->env_valid_node->phy_blk_addr;
					fn->ec =
						k_info->env_valid_node->ec;
					fn->next = NULL;
					have_env_free_node_flag = 1;

					tmp_valid_node =
						k_info->env_valid_node->next;
					while (tmp_valid_node != NULL) {
						k_info->env_valid_node->next =
							tmp_valid_node->next;
						multi_free_node =
						kzalloc(env_node_size,
							GFP_KERNEL);
						multi_free_node->phy_blk_addr =
						tmp_valid_node->phy_blk_addr;
						multi_free_node->ec =
							tmp_valid_node->ec;
						kfree(tmp_valid_node);
						multi_free_node->dirty_flag = 1;
						free_tmp_node = fn;
						free_tmp_node =
						get_free_tail(free_tmp_node);
						free_tmp_node->next =
							multi_free_node;
						tmp_valid_node =
						k_info->env_valid_node->next;
					}
					k_info->env_valid_node->phy_blk_addr =
						start_blk;
					k_info->env_valid_node->phy_page_addr
						= 0;
					k_info->env_valid_node->ec =
						key_oobinfo->ec;
					k_info->env_valid_node->timestamp =
						key_oobinfo->timestamp;
					k_info->env_valid_node->next = NULL;

				} else if (key_oobinfo->timestamp ==
					k_info->env_valid_node->timestamp) {
					tmp_valid_node =
						k_info->env_valid_node;
					env_valid_node =
					kzalloc(sizeof(struct env_valid_node_t),
						GFP_KERNEL);
					if (env_valid_node == NULL)
						return -ENOMEM;
					env_valid_node->phy_blk_addr
						= start_blk;
					env_valid_node->phy_page_addr = 0;
					env_valid_node->timestamp =
						key_oobinfo->timestamp;
					env_valid_node->ec = key_oobinfo->ec;
					while (tmp_valid_node->next != NULL) {
						tmp_valid_node =
						tmp_valid_node->next;
					}
					tmp_valid_node->next = env_valid_node;
				} else {
					fn->phy_blk_addr
						= start_blk;
					fn->ec = key_oobinfo->ec;
					have_env_free_node_flag = 1;
				}

				if (have_env_free_node_flag) {
					if (k_info->env_free_node == NULL)
						k_info->env_free_node = fn;
					else {
						tn =
						k_info->env_free_node;
						tn = get_free_tail(tn);
						tn->next = fn;
					}
				} else {
					kfree(fn);
					fn = NULL;
				}
			} else {
				k_info->env_valid_node->phy_blk_addr =
					start_blk;
				k_info->env_valid_node->phy_page_addr = 0;
				k_info->env_valid_node->ec =
					key_oobinfo->ec;
				k_info->env_valid_node->timestamp =
					key_oobinfo->timestamp;
			}
		} else if (key_blk < max_key_blk) {
			fn = alloc_fn();
			if (fn == NULL)
				return -ENOMEM;

			fn->phy_blk_addr = start_blk;
			fn->ec = key_oobinfo->ec;
			if (k_info->env_free_node == NULL)
				k_info->env_free_node = fn;
			else {
				tn = k_info->env_free_node;
				key_prev_node = tn;
				while (tn != NULL) {
					if (tn->dirty_flag == 1)
						break;
					key_prev_node = tn;
					tn = tn->next;
				}
				if (key_prev_node == tn) {
					fn->next = tn;
					k_info->env_free_node = fn;
				} else {
					key_prev_node->next = fn;
					fn->next = tn;
				}
			}
		}
		key_blk++;
		if ((key_blk >= max_key_blk) && (k_info->env_valid == 1))
			break;

	} while ((++start_blk) < key_end);
	if (start_blk >= key_end) {
		memcpy(k_info->nand_bbt_info.bbt_head_magic,
		BBT_HEAD_MAGIC, 4);
		memcpy(k_info->nand_bbt_info.bbt_tail_magic,
		BBT_TAIL_MAGIC, 4);
	}

	if (k_info->env_valid == 1) {

		aml_oob_ops.mode = MTD_OPS_AUTO_OOB;
		aml_oob_ops.len = mtd->writesize;
		aml_oob_ops.ooblen = sizeof(struct env_oobinfo_t);
		aml_oob_ops.ooboffs = aml_oob_region.offset;
		aml_oob_ops.datbuf = data_buf;
		aml_oob_ops.oobbuf = key_oob_buf;

		for (i = 0; i < pages_per_blk; i++) {

			memset((uint8_t *)aml_oob_ops.datbuf,
			0x0, mtd->writesize);
			memset((uint8_t *)aml_oob_ops.oobbuf,
			0x0, aml_oob_ops.ooblen);

			offset = k_info->env_valid_node->phy_blk_addr;
			offset *= mtd->erasesize;
			offset += i * mtd->writesize;
			error = mtd->_read_oob(mtd, offset, &aml_oob_ops);
			if ((error != 0) && (error != -EUCLEAN)) {
				pr_err(
				"read failed: %llx, %d\n",
				(uint64_t)offset, error);
				continue;
			}

			#ifdef NAND_KEY_SAVE_MULTI_BLOCK
			if (!memcmp(key_oobinfo->name, ENV_KEY_MAGIC, 4)) {
				k_info->env_valid_node->phy_page_addr = i;
				tmp_valid_node = k_info->env_valid_node->next;
				while (tmp_valid_node != NULL) {
					tmp_valid_node->phy_page_addr = i;
					tmp_valid_node = tmp_valid_node->next;
				}
			} else
				break;
			#else
			if (!memcmp(key_oobinfo->name, ENV_KEY_MAGIC, 4))
				k_info->env_valid_node->phy_page_addr = i;
			else
				break;
			#endif
		}
	}
	#ifdef NAND_KEY_SAVE_MULTI_BLOCK
	if ((mtd->writesize < CONFIG_KEYSIZE)
			&& (k_info->env_valid == 1)) {
		i = (CONFIG_KEYSIZE + mtd->writesize - 1) / mtd->writesize;
		k_info->env_valid_node->phy_page_addr -= (i - 1);

		tmp_valid_node = k_info->env_valid_node->next;
		while (tmp_valid_node != NULL) {
			tmp_valid_node->phy_page_addr -= (i - 1);
			tmp_valid_node = tmp_valid_node->next;
		}
	}
	#else
	if ((mtd->writesize < CONFIG_KEYSIZE)
			&& (k_info->env_valid == 1)) {
		i = (CONFIG_KEYSIZE + mtd->writesize - 1) / mtd->writesize;
		k_info->env_valid_node->phy_page_addr -= (i - 1);
	}
	#endif

	offset =
		k_info->env_valid_node->phy_blk_addr;
	offset *= mtd->erasesize;
	offset +=
		k_info->env_valid_node->phy_page_addr
			* mtd->writesize;
	if (k_info->env_valid_node->phy_blk_addr < 0)
		pr_err("aml nand key not have valid addr: not wrote\n");
	else
		pr_info("aml nand key valid addr: %llx\n", offset);
	#ifdef NAND_KEY_SAVE_MULTI_BLOCK
	tmp_valid_node = k_info->env_valid_node->next;
	while (tmp_valid_node != NULL) {
		offset = tmp_valid_node->phy_blk_addr;
		offset *= mtd->erasesize;
		offset += tmp_valid_node->phy_page_addr * mtd->writesize;
		pr_info("aml nand key valid addr: %llx\n", (uint64_t)offset);
		tmp_valid_node = tmp_valid_node->next;
	}
	#endif

	pr_info(
		"CONFIG_KEYSIZE=%d;KEYSIZE=%d;bbt=%d;default_keyironment_size=%d\n",
			CONFIG_KEYSIZE, KEYSIZE,
				sizeof(struct aml_nand_bbt_info),
					default_keyironment_size);
	kfree(data_buf);

	return 0;
}


static int aml_nand_key_check(struct mtd_info *mtd)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	struct aml_nand_platform *plat = aml_chip->platform;
	struct platform_nand_chip *chip =
		&plat->platform_nand_data.chip;
	struct aml_nand_bbt_info *nand_bbt_info;
	struct aml_nandkey_info_t *k_info;
	struct mesonkey_t *key_ptr;
	int error = 0, start_blk, total_blk, update_key_flag = 0;
	int i, j, nr, phys_erase_shift;
	uint64_t offset;

	chip = chip;
	nr = 0;
	error = aml_nand_key_init(mtd);
	if (error)
		return error;
	key_ptr = kzalloc(sizeof(struct mesonkey_t), GFP_KERNEL);
	if (key_ptr == NULL)
		return -ENOMEM;
	k_info = aml_chip->aml_nandkey_info;

	if (k_info->env_valid == 1)
		pr_info("%s() %d, inited\n", __func__, __LINE__);
	else {
			update_key_flag = 1;
			nand_bbt_info = (struct aml_nand_bbt_info *)
				(key_ptr->data + default_keyironment_size);

			memcpy(nand_bbt_info->nand_bbt,
				k_info->nand_bbt_info.nand_bbt,
					MAX_BAD_BLK_NUM * sizeof(int16_t));
			memcpy(nand_bbt_info->bbt_head_magic,
				BBT_HEAD_MAGIC, 4);
			memcpy(nand_bbt_info->bbt_tail_magic,
				BBT_TAIL_MAGIC, 4);
			phys_erase_shift = fls(mtd->erasesize) - 1;
			offset = k_info->start_block;
			offset *= mtd->erasesize;

			start_blk = (int)(offset >> phys_erase_shift);
			total_blk = (int)(mtd->size >> phys_erase_shift);
			for (i = start_blk; i < total_blk; i++) {
				aml_chip->block_status[i] = NAND_BLOCK_GOOD;
				for (j = 0; j < MAX_BAD_BLK_NUM; j++) {
					if (nand_bbt_info->nand_bbt[j] == i) {
						aml_chip->block_status[i] =
							NAND_BLOCK_BAD;
						break;
					}
				}
			}
			memcpy(k_info->nand_bbt_info.bbt_head_magic,
				key_ptr->data + default_keyironment_size,
					sizeof(struct aml_nand_bbt_info));
	}


	if (update_key_flag) {
		error = aml_nand_save_key(mtd, (u_char *)key_ptr);
		if (error) {
			pr_err("nand key save failed: %d\n", error);
			goto exit;
		}
	}

exit:
	kfree(key_ptr);
	return 0;
}

static struct mtd_info *nand_key_mtd;

static int32_t nand_key_read(uint8_t *buf,
	uint32_t len, uint32_t *actual_length)
{
	struct mesonkey_t *key_ptr = NULL;
	int error = 0;
	uint32_t *length;

	length = actual_length;
	*length = 0;
	if (len > default_keyironment_size) {
		pr_err("key data len too much,%s\n", __func__);
		return -EFAULT;
	}
	key_ptr = kzalloc(CONFIG_KEYSIZE, GFP_KERNEL);
	if (key_ptr == NULL)
		return -ENOMEM;
	memset(key_ptr, 0, CONFIG_KEYSIZE);
	error = aml_nand_get_key(nand_key_mtd, (u_char *)key_ptr);
	if (error) {
		pr_err("read key error,%s\n", __func__);
		error = -EFAULT;
		goto exit;
	}
	memcpy(buf, key_ptr->data + 0, len);
	*length = len;
exit:
	kfree(key_ptr);
	return 0;
}

static int32_t nand_key_write(uint8_t *buf,
	uint32_t len, uint32_t *actual_length)
{
	struct mesonkey_t *key_ptr = NULL;
	int error = 0;
	uint32_t *length;

	length = actual_length;
	*length = 0;
	if (len > default_keyironment_size) {
		pr_err("key data len error,%s\n", __func__);
		return -EFAULT;
	}
	key_ptr = kzalloc(CONFIG_KEYSIZE, GFP_KERNEL);
	if (key_ptr == NULL)
		return -ENOMEM;
	memset(key_ptr, 0, CONFIG_KEYSIZE);
	memcpy(key_ptr->data + 0, buf, len);

	error = aml_nand_save_key(nand_key_mtd, (u_char *)key_ptr);
	if (error) {
		pr_err("save key error,%s\n", __func__);
		error = -EFAULT;
		goto exit;
	}
	*length = len;

exit:
	kfree(key_ptr);
	return error;
}
int aml_key_init(struct aml_nand_chip *aml_chip)
{
	int err = 0;

	pr_info("nand key: nand_key_probe.\n");
	err = aml_nand_key_check(aml_chip->mtd);
	if (err)
		pr_err("invalid nand key\n");

	nand_key_mtd = aml_chip->mtd;

	storage_ops_read(nand_key_read);
	storage_ops_write(nand_key_write);
	pr_info("nand key init ok! %s()%d\n", __func__, __LINE__);

	return err;
}
