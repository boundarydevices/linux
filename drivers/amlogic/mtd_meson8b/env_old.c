/*
 * drivers/amlogic/mtd_old/env_old.c
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
#include <linux/crc32.h>
#include "aml_mtd.h"
#include "env_old.h"

#include<linux/cdev.h>
#include <linux/device.h>



#define ENV_NAME "nand_env"
static dev_t uboot_env_no;
struct cdev uboot_env;
struct device *uboot_dev;
struct class *uboot_env_class;

static struct aml_nand_chip *aml_chip_env;
static struct mtd_info *nand_env_mtd = NULL;

static unsigned default_environment_size =
	(ENV_SIZE - sizeof(struct aml_nand_bbt_info));


/* protect flag inside */
static int rsv_protect = 1;

static inline void _aml_rsv_disprotect(void)
{
	rsv_protect = 0;
}

static inline void _aml_rsv_protect(void)
{
	rsv_protect = 1;
}

static inline int _aml_rsv_isprotect(void)
{
	return rsv_protect;
}

int aml_nand_rsv_erase_protect(struct mtd_info *mtd, unsigned int block_addr)
{
	if (!_aml_rsv_isprotect())
		return 0;
#if 0
	pr_err(" %s() %d: fixme\n", __func__, __LINE__);

	if (aml_chip->aml_nandkey_info != NULL) {
		if (aml_chip->aml_nandkey_info->valid)
		if ((block_addr >= aml_chip->aml_nandkey_info->start_block)
			&& (block_addr < aml_chip->aml_nandkey_info->end_block))
			return -1; /*need skip key blocks*/
	}
	if (aml_chip->aml_nandbbt_info != NULL) {
		if (aml_chip->aml_nandbbt_info->valid)
		if ((block_addr >= aml_chip->aml_nandbbt_info->start_block)
			&& (block_addr < aml_chip->aml_nandbbt_info->end_block))
			return -1; /*need skip bbt blocks*/
	}
#endif

	return 0;
}

int aml_nand_read_env(struct mtd_info *mtd, size_t offset, u_char *buf)
{
	struct env_oobinfo_t *env_oobinfo;
	int error = 0, err, start_blk, total_blk;
	size_t addr = 0;
	size_t amount_loaded = 0;
	size_t len;
	//struct mtd_oob_ops aml_oob_ops;
	struct mtd_oob_ops  *aml_oob_ops = NULL;
	u_char *data_buf = NULL;
	u_char env_oob_buf[sizeof(struct env_oobinfo_t)];
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	struct aml_nandenv_info_t *envinfo;

	envinfo = aml_chip->aml_nandenv_info;
	if (!envinfo->env_valid)
		return 1;

	addr = (1024 * mtd->writesize / aml_chip->plane_num);
#ifdef NEW_NAND_SUPPORT
	if ((aml_chip->new_nand_info.type)
		&& (aml_chip->new_nand_info.type < 10))
		addr += RETRY_NAND_BLK_NUM* mtd->erasesize;
#endif
	start_blk = addr / mtd->erasesize;
	total_blk = mtd->size / mtd->erasesize;
	addr = envinfo->env_valid_node->phy_blk_addr;
	addr *= mtd->erasesize;
	addr += envinfo->env_valid_node->phy_page_addr
		* mtd->writesize;

	data_buf = kzalloc(mtd->writesize, GFP_KERNEL);
	if (data_buf == NULL) {
		err = -ENOMEM;
		goto exit;
	}

	aml_oob_ops = kzalloc(sizeof(struct mtd_oob_ops),
		GFP_KERNEL);
	if (aml_oob_ops == NULL) {
		err = -ENOMEM;
		goto exit;
	}

	env_oobinfo = (struct env_oobinfo_t *)env_oob_buf;
	while (amount_loaded < CONFIG_ENV_SIZE ) {

		aml_oob_ops->mode = MTD_OPS_AUTO_OOB;
		aml_oob_ops->len = mtd->writesize;
		aml_oob_ops->ooblen = sizeof(struct env_oobinfo_t);
		aml_oob_ops->ooboffs = 0;
		aml_oob_ops->datbuf = data_buf;
		aml_oob_ops->oobbuf = env_oob_buf;

		error = mtd->_read_oob(mtd, addr, aml_oob_ops);
		if ((error != 0) && (error != -EUCLEAN)) {
			pr_err("blk check good but read failed: %llx, %d\n",
				(uint64_t)addr, error);
			err = 1;
			goto exit;
		}

		if (memcmp(env_oobinfo->name, ENV_NAND_MAGIC, 4))
			pr_err("invalid env magic: %llx\n", (uint64_t)addr);

		addr += mtd->writesize;
		len = min(mtd->writesize,
			CONFIG_ENV_SIZE - amount_loaded);
		memcpy(buf + amount_loaded, data_buf, len);
		amount_loaded += mtd->writesize;
	}
	if (amount_loaded < CONFIG_ENV_SIZE) {
		err = 1;
		goto exit;
	}

	kfree(data_buf);
	kfree(aml_oob_ops);
	return 0;

exit:
	if (aml_oob_ops) {
		kfree(aml_oob_ops);
		aml_oob_ops = NULL;
	}
	if (data_buf) {
		kfree(data_buf);
		data_buf = NULL;
	}
	return err;
}

static int aml_nand_write_env(struct mtd_info *mtd,
	loff_t offset, u_char *buf)
{
	struct env_oobinfo_t *env_oobinfo;
	int error = 0, err;
	loff_t addr = 0;
	size_t amount_saved = 0;
	size_t len;
	struct mtd_oob_ops  * aml_oob_ops;
	u_char *data_buf = NULL;
	u_char env_oob_buf[sizeof(struct env_oobinfo_t)];
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	struct aml_nandenv_info_t *envinfo;

	envinfo = aml_chip->aml_nandenv_info;
	aml_oob_ops = kzalloc(sizeof(struct mtd_oob_ops),
		GFP_KERNEL);
	if (aml_oob_ops == NULL) {
		err = -ENOMEM;
		goto exit;
	}
	data_buf = kzalloc(mtd->writesize, GFP_KERNEL);
	if (data_buf == NULL) {
		err = -ENOMEM;
		goto exit;
	}
	addr = offset;
	env_oobinfo = (struct env_oobinfo_t *)env_oob_buf;
	memcpy(env_oobinfo->name, ENV_NAND_MAGIC, 4);
	env_oobinfo->ec = envinfo->env_valid_node->ec;
	env_oobinfo->timestamp = envinfo->env_valid_node->timestamp;
	env_oobinfo->status_page = 1;

	while (amount_saved < CONFIG_ENV_SIZE ) {

		aml_oob_ops->mode = MTD_OPS_AUTO_OOB;
		aml_oob_ops->len = mtd->writesize;
		aml_oob_ops->ooblen = sizeof(struct env_oobinfo_t);
		aml_oob_ops->ooboffs = 0;
		aml_oob_ops->datbuf = data_buf;
		aml_oob_ops->oobbuf = env_oob_buf;

		memset((u_char *)aml_oob_ops->datbuf,
			0x0, mtd->writesize);
		len = min(mtd->writesize,
			CONFIG_ENV_SIZE - amount_saved);
		memcpy((u_char *)aml_oob_ops->datbuf,
			buf + amount_saved, len);

		error = mtd->_write_oob(mtd, addr, aml_oob_ops);
		if (error) {
			pr_err("blk check good but write failed: %llx, %d\n",
				(uint64_t)addr, error);
					//return 1;
			err = 1;
			goto exit;
		}

		addr += mtd->writesize;;
		amount_saved += mtd->writesize;
	}
	if (amount_saved < CONFIG_ENV_SIZE) {
		err = 1;
		goto exit;
	}

	kfree(data_buf);
	kfree(aml_oob_ops);
	return 0;

exit:
	if (aml_oob_ops) {
		kfree(aml_oob_ops);
		aml_oob_ops = NULL;
	}
	if (data_buf) {
		kfree(data_buf);
		data_buf = NULL;
	}
	return err;
}

int aml_nand_save_env(struct mtd_info *mtd, u_char *buf)
{
	struct aml_nand_bbt_info *nand_bbt_info;
	struct env_free_node_t *env_free_node = NULL;
	struct env_free_node_t *env_tmp_node = NULL;
	int error = 0, pages_per_blk, i = 1;
	loff_t addr = 0;
	struct erase_info  *aml_env_erase_info;
	env_t *env_ptr = (env_t *)buf;
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	struct aml_nandenv_info_t *envinfo;

	envinfo = aml_chip->aml_nandenv_info;
	if (!envinfo->env_init)
		return 1;

	aml_env_erase_info = kzalloc(sizeof(struct erase_info),
		GFP_KERNEL);
	if (aml_env_erase_info == NULL) {
		pr_err("%s %d no mem for aml_env_erase_info\n",
			__func__, __LINE__);
		error = -ENOMEM;
		goto exit;
	}

	pages_per_blk = mtd->erasesize / mtd->writesize;
	if ((mtd->writesize < CONFIG_ENV_SIZE)
		&& (envinfo->env_valid == 1))
		i = (CONFIG_ENV_SIZE + mtd->writesize - 1) / mtd->writesize;

RE_SEARCH:
	if (envinfo->env_valid) {
		envinfo->env_valid_node->phy_page_addr += i;
		if ((envinfo->env_valid_node->phy_page_addr + i)
			> pages_per_blk) {
			env_free_node = kzalloc(sizeof(struct env_free_node_t),
				GFP_KERNEL);
			if (env_free_node == NULL) {
				pr_err("%s %d no mem for env_free_node\n",
					__func__, __LINE__);
				error = -ENOMEM;
				goto exit;
			}
			env_free_node->phy_blk_addr =
				envinfo->env_valid_node->phy_blk_addr;
			env_free_node->ec = envinfo->env_valid_node->ec;
			env_tmp_node = envinfo->env_free_node;
			while (env_tmp_node->next != NULL) {
				env_tmp_node = env_tmp_node->next;
			}
			env_tmp_node->next = env_free_node;

			env_tmp_node = envinfo->env_free_node;
			envinfo->env_valid_node->phy_blk_addr
				= env_tmp_node->phy_blk_addr;
			envinfo->env_valid_node->phy_page_addr = 0;
			envinfo->env_valid_node->ec = env_tmp_node->ec;
			envinfo->env_valid_node->timestamp += 1;
			envinfo->env_free_node = env_tmp_node->next;
			kfree(env_tmp_node);
		}
	}
	else {

		env_tmp_node = envinfo->env_free_node;
		envinfo->env_valid_node->phy_blk_addr =
			env_tmp_node->phy_blk_addr;
		envinfo->env_valid_node->phy_page_addr = 0;
		envinfo->env_valid_node->ec = env_tmp_node->ec;
		envinfo->env_valid_node->timestamp += 1;
		envinfo->env_free_node = env_tmp_node->next;
		kfree(env_tmp_node);
	}

	addr = envinfo->env_valid_node->phy_blk_addr;
	addr *= mtd->erasesize;
	addr += envinfo->env_valid_node->phy_page_addr * mtd->writesize;
	if (envinfo->env_valid_node->phy_page_addr == 0) {
		error = mtd->_block_isbad(mtd, addr);
		if (error != 0) {
			/*bad block here, need fix it*/
			/*because of env_blk list may*/
			/*be include bad block,*/
			/*so we need check it and done here*/
			/* if don't, some bad blocks may be */
			/* erase here and env will lost*/
			/*or too much ecc error*/
			pr_err("have bad block in env_blk list!!!!\n");
			envinfo->env_valid_node->phy_page_addr
				= pages_per_blk - i;
			goto RE_SEARCH;
		}

		memset(aml_env_erase_info, 0, sizeof(struct erase_info));
		aml_env_erase_info->mtd = mtd;
		aml_env_erase_info->addr = addr;
		aml_env_erase_info->len = mtd->erasesize;

		error = mtd->_erase(mtd, aml_env_erase_info);
		if (error) {
			pr_err("env free blk erase failed %d\n", error);
			mtd->_block_markbad(mtd, addr);
			//return error;
			goto exit;
		}
		envinfo->env_valid_node->ec++;
	}

	nand_bbt_info = &envinfo->nand_bbt_info;
	if ((!memcmp(nand_bbt_info->bbt_head_magic,
			BBT_HEAD_MAGIC, 4))
		&& (!memcmp(nand_bbt_info->bbt_tail_magic,
			BBT_TAIL_MAGIC, 4))) {
		memcpy(env_ptr->data + default_environment_size,
			envinfo->nand_bbt_info.bbt_head_magic,
			sizeof(struct aml_nand_bbt_info));
		env_ptr->crc = (crc32((0 ^ 0xffffffffL),
			env_ptr->data, ENV_SIZE) ^ 0xffffffffL);
	}

	if (aml_nand_write_env(mtd, addr, (u_char *) env_ptr)) {
		pr_err("update nand env FAILED!\n");
		error = 1;
		goto exit;
	}

	kfree(aml_env_erase_info);
	return error;
exit:
	if (aml_env_erase_info) {
		kfree(aml_env_erase_info);
		aml_env_erase_info = NULL;
	}
	if (env_free_node) {
		kfree(env_free_node);
		env_free_node = NULL;
	}
	return error;
}

static int aml_nand_env_init(struct mtd_info *mtd)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	struct nand_chip *chip = &aml_chip->chip;
	struct env_oobinfo_t *env_oobinfo;
	struct env_free_node_t *env_free_node;
	struct env_free_node_t *env_tmp_node = NULL;
	struct env_free_node_t *env_prev_node = NULL;
	int error = 0, ret = 0, env_page_num = 0;
	int err, start_blk, total_blk, env_blk;
	int i, j, pages_per_blk;
	int bad_blk_cnt = 0, max_env_blk, phys_erase_shift;
	loff_t offset;
	u_char *data_buf = NULL;
	u_char   *good_addr;
	int k, env_status = 0, scan_status = 0;
	struct mtd_oob_ops  *aml_oob_ops;
	u_char env_oob_buf[sizeof(struct env_oobinfo_t)];
	struct aml_nandenv_info_t *envinfo;
	struct env_valid_node_t *vnode;

	good_addr = kzalloc(256, GFP_KERNEL);
	if (good_addr == NULL)
		return -ENOMEM;
	memset(good_addr, 0, 256);

	aml_oob_ops = kzalloc(sizeof(struct mtd_oob_ops),
		GFP_KERNEL);
	if (aml_oob_ops == NULL) {
		pr_err("%s %d no mem for aml_oob_ops\n",
			__func__, __LINE__);
		err = -ENOMEM;
		goto exit;
	}
	data_buf = kzalloc(mtd->writesize, GFP_KERNEL);
	if (data_buf == NULL) {
		err = -ENOMEM;
		goto exit;
	}
	aml_chip->aml_nandenv_info =
		kzalloc(sizeof(struct aml_nandenv_info_t),
		GFP_KERNEL);
	if (aml_chip->aml_nandenv_info == NULL) {
		err = -ENOMEM;
		goto exit;
	}
	envinfo = aml_chip->aml_nandenv_info;
	envinfo->mtd = mtd;
	envinfo->env_valid_node = kzalloc(sizeof(struct env_valid_node_t),
		GFP_KERNEL);
	if (envinfo->env_valid_node == NULL) {
		err = -ENOMEM;
		goto exit;
	}
	vnode = envinfo->env_valid_node;
	vnode->phy_blk_addr = -1;
	env_page_num = CONFIG_ENV_SIZE /mtd->writesize;

	phys_erase_shift = fls(mtd->erasesize) - 1;
	max_env_blk = (NAND_MINI_PART_SIZE >> phys_erase_shift);
	if (max_env_blk < 2)
		max_env_blk = 2;
	if (nand_boot_flag)
		offset = (1024 * mtd->writesize / aml_chip->plane_num);
	else {
		default_environment_size = 0;
		offset = 0;
	}
#ifdef NEW_NAND_SUPPORT
	if ((aml_chip->new_nand_info.type)
		&& (aml_chip->new_nand_info.type < 10))
		offset += RETRY_NAND_BLK_NUM * mtd->erasesize;
#endif
	start_blk = (int)(offset >> phys_erase_shift);
	total_blk = (int)(mtd->size >> phys_erase_shift);
	pages_per_blk = (1 << (chip->phys_erase_shift - chip->page_shift));
	env_oobinfo = (struct env_oobinfo_t *)env_oob_buf;
	if ((default_environment_size + sizeof(struct aml_nand_bbt_info))
		> ENV_SIZE)
		total_blk = start_blk + max_env_blk;

	env_blk = 0;
	do {
		offset = mtd->erasesize;
		offset *= start_blk;
		scan_status = 0;
		error = mtd->_block_isbad(mtd, offset);
		if (error) {
			for (j=0; j<MAX_BAD_BLK_NUM; j++) {
				if (envinfo->nand_bbt_info.nand_bbt[j] == 0) {
					envinfo->nand_bbt_info.nand_bbt[j] =
						start_blk;
					bad_blk_cnt++;
					break;
				}
			}
			continue;
		}
RE_ENV:
		aml_oob_ops->mode = MTD_OPS_AUTO_OOB;
		aml_oob_ops->len = mtd->writesize;
		aml_oob_ops->ooblen = sizeof(struct env_oobinfo_t);
		aml_oob_ops->ooboffs = 0;
		aml_oob_ops->datbuf = data_buf;
		aml_oob_ops->oobbuf = env_oob_buf;

		memset((u_char *)aml_oob_ops->datbuf,
			0x0, mtd->writesize);
		memset((u_char *)aml_oob_ops->oobbuf,
			0x0, aml_oob_ops->ooblen);

		error = mtd->_read_oob(mtd, offset, aml_oob_ops);
		if ((error != 0) && (error != -EUCLEAN)) {
			pr_err("blk check good but read failed: %llx, %d\n",
				(uint64_t)offset, error);
			offset += CONFIG_ENV_SIZE;
			if ((scan_status++ > 6)
				|| (!((uint32_t)offset % mtd->erasesize))) {
				pr_err("can not scan env blks\n");
				scan_status = 0;
				continue;
			}
			goto RE_ENV;
		}
		envinfo->env_init = 1;
		if (!memcmp(env_oobinfo->name, ENV_NAND_MAGIC, 4)) {
			envinfo->env_valid = 1;
			if (vnode->phy_blk_addr >= 0) {
				env_free_node =
					kzalloc(sizeof(struct env_free_node_t),
					GFP_KERNEL);
				if (env_free_node == NULL) {
					pr_err("%s %d no mem for env_free_node\n",
						__func__, __LINE__);
					err = -ENOMEM;
					goto exit;
				}
				env_free_node->dirty_flag = 1;
				if (env_oobinfo->timestamp
					> vnode->timestamp) {

					env_free_node->phy_blk_addr =
						vnode->phy_blk_addr;
					env_free_node->ec =	vnode->ec;
					vnode->phy_blk_addr = start_blk;
					vnode->phy_page_addr = 0;
					vnode->ec =
						env_oobinfo->ec;
					vnode->timestamp =
						env_oobinfo->timestamp;
				} else {
					env_free_node->phy_blk_addr = start_blk;
					env_free_node->ec = env_oobinfo->ec;
				}
				if (envinfo->env_free_node == NULL)
					envinfo->env_free_node = env_free_node;
				else {
					env_tmp_node = envinfo->env_free_node;
					while (env_tmp_node->next != NULL) {
						env_tmp_node = env_tmp_node->next;
					}
					env_tmp_node->next = env_free_node;
				}
			} else {
				vnode->phy_blk_addr = start_blk;
				vnode->phy_page_addr = 0;
				vnode->ec = env_oobinfo->ec;
				vnode->timestamp = env_oobinfo->timestamp;
			}
		} else if (env_blk < max_env_blk) {
			env_free_node = kzalloc(sizeof(struct env_free_node_t),
				GFP_KERNEL);
			if (!env_free_node) {
					err = -ENOMEM;
					goto exit;
			}
			env_free_node->phy_blk_addr = start_blk;
			env_free_node->ec = env_oobinfo->ec;
			if (envinfo->env_free_node == NULL)
				envinfo->env_free_node = env_free_node;
			else {
				env_tmp_node = envinfo->env_free_node;
				env_prev_node = env_tmp_node;
				while (env_tmp_node != NULL) {
					if (env_tmp_node->dirty_flag == 1)
						break;
					env_prev_node = env_tmp_node;
					env_tmp_node = env_tmp_node->next;
				}
				if (env_prev_node == env_tmp_node) {
					env_free_node->next = env_tmp_node;
					envinfo->env_free_node = env_free_node;
				}
				else {
					env_prev_node->next = env_free_node;
					env_free_node->next = env_tmp_node;
				}
			}
		}
		env_blk++;
		if ((env_blk >= max_env_blk) && (envinfo->env_valid == 1))
			break;

	} while ((++start_blk) < ENV_NAND_SCAN_BLK);
	if (start_blk >= ENV_NAND_SCAN_BLK) {
		memcpy(envinfo->nand_bbt_info.bbt_head_magic,
			BBT_HEAD_MAGIC, 4);
		memcpy(envinfo->nand_bbt_info.bbt_tail_magic,
			BBT_TAIL_MAGIC, 4);
	}

	if (envinfo->env_valid == 1) {

		aml_oob_ops->mode = MTD_OPS_AUTO_OOB;
		aml_oob_ops->len = mtd->writesize;
		aml_oob_ops->ooblen = sizeof(struct env_oobinfo_t);
		/*aml_oob_ops->ooboffs = mtd->ecclayout->oobfree[0].offset;*/
		aml_oob_ops->ooboffs = 0;
		aml_oob_ops->datbuf = data_buf;
		aml_oob_ops->oobbuf = env_oob_buf;

		for (i=0; i<pages_per_blk; i++) {
			memset((u_char *)aml_oob_ops->datbuf,
				0x0, mtd->writesize);
			memset((u_char *)aml_oob_ops->oobbuf,
				0x0, aml_oob_ops->ooblen);

			offset = envinfo->env_valid_node->phy_blk_addr;
			offset *= mtd->erasesize;
			offset += i * mtd->writesize;
			error = mtd->_read_oob(mtd, offset, aml_oob_ops);
			if ((error != 0) && (error != -EUCLEAN)) {
				pr_err("chk good read fail: %llx, %d\n",
					(uint64_t)offset, error);
				ret = -1;
				continue;
			}

			if (!memcmp(env_oobinfo->name, ENV_NAND_MAGIC, 4)) {
				good_addr[i] = 1;
				envinfo->env_valid_node->phy_page_addr = i;
			}
			else
				break;
		}
	}
	if ((mtd->writesize < CONFIG_ENV_SIZE)
		&& (envinfo->env_valid == 1)) {
		i = envinfo->env_valid_node->phy_page_addr;
		if ( ((i+1)%env_page_num) != 0 ) {
			ret = -1;
			pr_err("aml_nand_env_init :  find  env incomplete\n");
		}
		if (ret == -1) {
			for ( i=0; i < (pages_per_blk / env_page_num); i++ ) {
				env_status =0;
				for (k = 0; k < env_page_num; k++) {
					if (!good_addr[ k + i * env_page_num]) {
						pr_err("find %d page env fail\n",
							(k + i * env_page_num));
						env_status = 1;
						break;
					}
				}
				if (!env_status) {
					pr_err("find %d page env ok\n",
						(i * env_page_num));
					envinfo->env_valid_node->phy_page_addr =
						k + i * env_page_num - 1;
				}
			}
		}

		i = (CONFIG_ENV_SIZE + mtd->writesize - 1) / mtd->writesize;
		envinfo->env_valid_node->phy_page_addr -= (i - 1);
	}

	offset = envinfo->env_valid_node->phy_blk_addr;
	offset *= mtd->erasesize;
	offset += envinfo->env_valid_node->phy_page_addr * mtd->writesize;
	pr_info("aml nand env valid addr: %llx\n", (uint64_t)offset);
	pr_info("CONFIG_ENV_SIZE=0x%x\n", CONFIG_ENV_SIZE);
	pr_info("ENV_SIZE=0x%x\n", ENV_SIZE);
	pr_info("bbt=0x%x\n", sizeof(struct aml_nand_bbt_info));
	pr_info("default_environment_size=0x%x\n",
		default_environment_size);

	kfree(data_buf);
	kfree(aml_oob_ops);
	return 0;
exit:
	kfree(data_buf);
	data_buf = NULL;
	kfree(aml_oob_ops);
	aml_oob_ops = NULL;
	kfree(aml_chip->aml_nandenv_info->env_valid_node);
		aml_chip->aml_nandenv_info->env_valid_node = NULL;
	kfree(aml_chip->aml_nandenv_info);
	aml_chip->aml_nandenv_info = NULL;
	kfree(env_free_node);
	env_free_node = NULL;
	return err;
}

static int __attribute__((unused))
	aml_nand_update_env(struct mtd_info *mtd)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	env_t *env_ptr;
	loff_t offset;
	int error = 0;
	struct aml_nandenv_info_t *envinfo;

	envinfo = aml_chip->aml_nandenv_info;
	env_ptr = kzalloc(sizeof(env_t), GFP_KERNEL);
	if (env_ptr == NULL)
		return -ENOMEM;

	if (envinfo->env_valid == 1) {

		offset = envinfo->env_valid_node->phy_blk_addr;
		offset *= mtd->erasesize;
		offset +=
			envinfo->env_valid_node->phy_page_addr * mtd->writesize;

		error = aml_nand_read_env(mtd, offset, (u_char *)env_ptr);
		if (error) {
			pr_err("nand env read failed: %llx, %d\n",
				(uint64_t)offset, error);
			return error;
		}

		error = aml_nand_save_env(mtd, (u_char *)env_ptr);
		if (error) {
			pr_err("update env bbt failed %d\n", error);
			return error;
		}
	}

	return error;
}

/* unpack bbt from bbt stored */
int unpack_bbt(int start, int total,
	struct aml_nand_chip *aml_chip,
	struct aml_nand_bbt_info *bbtinfo)
{
	int ret = 0;
	int i, j;
	uint16_t *bbt;

	bbt = bbtinfo->nand_bbt;
	for (i = start; i < total; i++) {
		aml_chip->block_status[i] = NAND_BLOCK_GOOD;
		for (j = 0; j < MAX_BAD_BLK_NUM; j++) {
			if ((bbt[j] & 0x7fff) == i) {
				if (bbt[j] & 0x8000) {
				    aml_chip->block_status[i]
						= NAND_FACTORY_BAD;
					pr_info("%s() fbb=%d, status[%d] =%d\n",
						__func__,
						i, i,
						aml_chip->block_status[i]);
				} else {
				    aml_chip->block_status[i]
						= NAND_BLOCK_BAD;
					pr_info("%s() used bad blk=%d\n",
						__func__, i);
				}
				break;
			}
		}
	}
	return ret;
}

int aml_nand_env_check(struct mtd_info *mtd)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	env_t *env_ptr;
	int error = 0, start_blk, total_blk, phys_erase_shift;
	loff_t offset;
	struct aml_nandenv_info_t *envinfo;
	struct aml_nand_bbt_info *bbtinfo;
	struct aml_nand_bbt_info *bbt_env;

	aml_chip->update_env_flag=0;
	error = aml_nand_env_init(mtd);
	if (error)
		return error;
	envinfo = aml_chip->aml_nandenv_info;
	env_ptr = kzalloc(sizeof(env_t), GFP_KERNEL);
	if (env_ptr == NULL)
		return -ENOMEM;

	if (envinfo->env_valid == 1) {

		offset = envinfo->env_valid_node->phy_blk_addr;
		offset *= mtd->erasesize;
		offset += envinfo->env_valid_node->phy_page_addr
			* mtd->writesize;

		error = aml_nand_read_env(mtd,
			offset,
			(u_char *)env_ptr);
		if (error) {
			pr_err("%s() failed: %llx, %d\n",
				__func__, (uint64_t)offset, error);
			goto exit;
		}

		phys_erase_shift = fls(mtd->erasesize) - 1;
		offset = (1024 * mtd->writesize / aml_chip->plane_num);
#ifdef NEW_NAND_SUPPORT
		if ((aml_chip->new_nand_info.type)
			&& (aml_chip->new_nand_info.type < 10))
			offset += RETRY_NAND_BLK_NUM* mtd->erasesize;
#endif
		start_blk = (int)(offset >> phys_erase_shift);
		total_blk = (int)(mtd->size >> phys_erase_shift);
		bbtinfo = (struct aml_nand_bbt_info *)
			(env_ptr->data + default_environment_size);

		if ((!memcmp(bbtinfo->bbt_head_magic, BBT_HEAD_MAGIC, 4))
			&& (!memcmp(bbtinfo->bbt_tail_magic,
				BBT_TAIL_MAGIC, 4))) {
			pr_info("%s() %d: GOT bbt!\n", __func__, __LINE__);
			/* fixme, */
			unpack_bbt(start_blk, total_blk,
				aml_chip, bbtinfo);
			bbt_env = &envinfo->nand_bbt_info;
			memcpy((u_char *)bbt_env->bbt_head_magic,
				(u_char *)bbtinfo,
				sizeof(struct aml_nand_bbt_info));
		}
	}

	if (aml_chip->update_env_flag) {
		error = aml_nand_save_env(mtd, (u_char *)env_ptr);
		if (error) {
			pr_err("nand env save failed: %d\n", error);
			goto exit;
		}
	}

exit:
	kfree(env_ptr);
	return 0;
}

int aml_nand_scan_bbt(struct mtd_info *mtd)
{
	return 0;
}
/********************************************************/
int amlnf_env_save(u8 *buf, int len)
{
	u8 *env_buf = NULL;
	int ret = 0;

	pr_info("uboot env amlnf_env_save : ####\n");

	if (aml_chip_env == NULL) {
		pr_info("uboot env not init yet!,%s\n", __func__);
		return -EFAULT;
	}

	if (len > CONFIG_ENV_SIZE) {
		pr_info("uboot env data len too much,%s\n", __func__);
		return -EFAULT;
	}

	env_buf = kzalloc(CONFIG_ENV_SIZE, GFP_KERNEL);
	if (env_buf == NULL) {
		/*pr_info("nand malloc for uboot env failed\n");*/
		ret = -1;
		goto exit_err;
	}
	memset(env_buf, 0, CONFIG_ENV_SIZE);
	memcpy(env_buf, buf, len);
	ret = aml_nand_save_env(nand_env_mtd, env_buf);
	if (ret) {
		pr_info("nand uboot env error,%s\n", __func__);
		ret = -EFAULT;
		goto exit_err;
	}
exit_err:
	kfree(env_buf);
	env_buf = NULL;
	return ret;
}

int amlnf_env_read(u8 *buf, int len)
{
	u8 *env_buf = NULL;
	int ret = 0;

	pr_info("uboot env amlnf_env_read, len %d: ####\n", len);

	if (len > CONFIG_ENV_SIZE) {
		pr_info("uboot env data len too much, %s\n",
			__func__);
		return -EFAULT;
	}
	if (aml_chip_env == NULL) {
		memset(buf, 0x0, len);
		pr_info("uboot env arg_valid = 0 invalid, %s\n",
			__func__);
		return 0;
	}

	env_buf = kzalloc(CONFIG_ENV_SIZE, GFP_KERNEL);
	if (env_buf == NULL) {
		ret = -1;
		goto exit_err;
	}
	memset(env_buf, 0, CONFIG_ENV_SIZE);
	ret = aml_nand_read_env(nand_env_mtd, 0, env_buf);
	if (ret) {
		pr_info("nand uboot env error,%s\n",
			__func__);
		ret = -EFAULT;
		goto exit_err;
	}

	memcpy(buf, env_buf, len);
exit_err:
	kfree(env_buf);
	env_buf = NULL;

	return ret;
}
ssize_t env_show(struct class *class, struct class_attribute *attr,
		char *buf)
{
	pr_info("env_show : #####\n");
	return 0;
}

ssize_t env_store(struct class *class, struct class_attribute *attr,
		const char *buf, size_t count)
{
	int ret = 0;
	u8 *env_ptr = NULL;

	pr_info("env_store : #####\n");

	env_ptr = kzalloc(CONFIG_ENV_SIZE, GFP_KERNEL);
	if (env_ptr == NULL)
		return -ENOMEM;

	ret = amlnf_env_read(env_ptr, CONFIG_ENV_SIZE);
	if (ret) {
		pr_info("nand_env_read: nand env read failed\n");
		kfree(env_ptr);
		return -EFAULT;
	}

	ret = amlnf_env_save(env_ptr, CONFIG_ENV_SIZE);
	if (ret) {
		pr_info("nand_env_read: nand env read failed\n");
		kfree(env_ptr);
		return -EFAULT;
	}

	pr_info("env_store : OK #####\n");
	return count;
}

static CLASS_ATTR(env, 0644, env_show, env_store);

int uboot_env_open(struct inode *node, struct file *file)
{
	return 0;
}

ssize_t uboot_env_read(struct file *file,
		char __user *buf,
		size_t count,
		loff_t *ppos)
{
	u8 *env_ptr = NULL;
	ssize_t read_size = 0;
	int ret = 0;

	if (*ppos == CONFIG_ENV_SIZE)
		return 0;
	if (*ppos >= CONFIG_ENV_SIZE) {
		pr_info("nand env: data access out of space!\n");
		return -EFAULT;
	}

	env_ptr = vmalloc(CONFIG_ENV_SIZE + 2048);
	if (env_ptr == NULL)
		return -ENOMEM;

	/*amlnand_get_device(aml_chip_env, CHIP_READING);*/
	ret = amlnf_env_read((u8 *)env_ptr, CONFIG_ENV_SIZE);
	if (ret) {
		pr_info("nand_env_read: nand env read failed:%d\n",
			ret);
		ret = -EFAULT;
		goto exit;
	}
	if ((*ppos + count) > CONFIG_ENV_SIZE)
		read_size = CONFIG_ENV_SIZE - *ppos;
	else
		read_size = count;

	ret = copy_to_user(buf, (env_ptr + *ppos), read_size);
	*ppos += read_size;
exit:
	/*amlnand_release_device(aml_chip_env);*/
	vfree(env_ptr);
	return read_size;
}

ssize_t uboot_env_write(struct file *file,
		const char __user *buf,
		size_t count, loff_t *ppos)
{
	u8 *env_ptr = NULL;
	ssize_t write_size = 0;
	int ret = 0;

	if (*ppos == CONFIG_ENV_SIZE)
		return 0;

	if (*ppos >= CONFIG_ENV_SIZE) {
		pr_info("nand env: data access out of space!\n");
		return -EFAULT;
	}

	env_ptr = vmalloc(CONFIG_ENV_SIZE + 2048);
	if (env_ptr == NULL)
		return -ENOMEM;

	/*not need nand_get_device here, mtd->_read_xx will done with it*/
	/*nand_get_device(mtd, FL_WRITING);*/
	ret = amlnf_env_read((u8 *)env_ptr, CONFIG_ENV_SIZE);
	if (ret) {
		pr_info("nand_env_read: nand env read failed\n");
		ret = -EFAULT;
		goto exit;
	}

	if ((*ppos + count) > CONFIG_ENV_SIZE)
		write_size = CONFIG_ENV_SIZE - *ppos;
	else
		write_size = count;

	ret = copy_from_user((env_ptr + *ppos), buf, write_size);

	ret = amlnf_env_save(env_ptr, CONFIG_ENV_SIZE);
	if (ret) {
		pr_info("nand_env_read: nand env read failed\n");
		ret = -EFAULT;
		goto exit;
	}

	*ppos += write_size;
exit:
	/*nand_release_device(mtd);*/
	vfree(env_ptr);
	return write_size;
}

long uboot_env_ioctl(struct file *file, u32 cmd, unsigned long args)
{
	return 0;
}

static const struct file_operations uboot_env_ops = {
	.open = uboot_env_open,
	.read = uboot_env_read,
	.write = uboot_env_write,
	.unlocked_ioctl = uboot_env_ioctl,
};

int aml_ubootenv_init(struct aml_nand_chip *aml_chip)
{
	int ret = 0;
	u8 *env_buf = NULL;

	aml_chip_env = aml_chip;
	nand_env_mtd = aml_chip->mtd;

	env_buf = kzalloc(CONFIG_ENV_SIZE, GFP_KERNEL);
	if (env_buf == NULL) {
		ret = -1;
		goto exit_err;
	}
	memset(env_buf, 0x0, CONFIG_ENV_SIZE);

#ifndef AML_NAND_UBOOT
	pr_info("%s: register env chardev\n", __func__);
	ret = alloc_chrdev_region(&uboot_env_no, 0, 1, ENV_NAME);
	if (ret < 0) {
		pr_info("alloc uboot env dev_t no failed\n");
		ret = -1;
		goto exit_err;
	}

	cdev_init(&uboot_env, &uboot_env_ops);
	uboot_env.owner = THIS_MODULE;
	ret = cdev_add(&uboot_env, uboot_env_no, 1);
	if (ret) {
		pr_info("uboot env dev add failed\n");
		ret = -1;
		goto exit_err1;
	}

	uboot_env_class = class_create(THIS_MODULE, ENV_NAME);
	if (IS_ERR(uboot_env_class)) {
		pr_info("uboot env dev add failed\n");
		ret = -1;
		goto exit_err2;
	}

	ret = class_create_file(uboot_env_class, &class_attr_env);
	if (ret) {
		pr_info("uboot env dev add failed\n");
		ret = -1;
		goto exit_err2;
	}

	uboot_dev = device_create(uboot_env_class,
		NULL,
		uboot_env_no,
		NULL,
		ENV_NAME);
	if (IS_ERR(uboot_dev)) {
		pr_info("uboot env dev add failed\n");
		ret = -1;
		goto exit_err3;
	}

	pr_info("%s: register env chardev OK\n", __func__);

	kfree(env_buf);
	env_buf = NULL;

	return ret;
exit_err3:
	class_remove_file(uboot_env_class, &class_attr_env);
	class_destroy(uboot_env_class);
exit_err2:
	cdev_del(&uboot_env);
exit_err1:
	unregister_chrdev_region(uboot_env_no, 1);

#endif /* AML_NAND_UBOOT */
exit_err:
	kfree(env_buf);
	env_buf = NULL;

	return ret;
}

