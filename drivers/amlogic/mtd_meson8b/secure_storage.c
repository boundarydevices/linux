/*
 * drivers/amlogic/mtd_meson8b/secure_storage.c
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
#include "env_old.h"

#define CONFIG_SECURE_SIZE          (0x10000*2)/* 128k */
#define SECURE_SIZE                (CONFIG_SECURE_SIZE - (sizeof(uint32_t)))
#define SECURE_STORE_MAGIC         (0x6365736e)
#define REMAIN_BLOCK_NUM            4
#define NAND_SECURE_BLK             2



struct secure_t {
	uint32_t crc;        /* CRC32 over data bytes    */
	uint8_t data[SECURE_SIZE]; /* Environment data      */
};

struct secure_oobinfo_t {
	int32_t name;
	uint32_t timestamp;
};

struct mtd_info *nand_secure_mtd;
static int aml_nand_read_secure(struct mtd_info *mtd,
	loff_t offset, uint8_t *buf)
{
	struct secure_oobinfo_t *secure_oobinfo;
	struct mtd_oob_ops  *aml_oob_ops = NULL;
	struct mtd_oob_region aml_oob_region;
	int error = 0, err;
	loff_t addr = 0, len;
	size_t amount_loaded = 0;
	uint8_t *data_buf = NULL;
	uint8_t secure_oob_buf[sizeof(struct secure_oobinfo_t)];
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	struct aml_nandsecure_info_t *s_info;

	s_info = aml_chip->aml_nandsecure_info;
	if (!s_info->secure_valid)
		return 1;
	err = mtd->ooblayout->free(mtd, 0,
		(struct mtd_oob_region *)&aml_oob_region);
	if (err != 0) {
		pr_err("read oob free failed :func : %s,line : %d\n",
			__func__, __LINE__);
		err = -ENOMEM;
		goto exit;
	}

	addr = s_info->secure_valid_node->phy_blk_addr;
	addr *= mtd->erasesize;
	addr += s_info->secure_valid_node->phy_page_addr
		* mtd->writesize;
	pr_err("%s(): valid addr: %llx  at block %d page %d\n",
		__func__, (uint64_t)addr,
		s_info->secure_valid_node->phy_blk_addr,
		s_info->secure_valid_node->phy_page_addr);

	data_buf = kzalloc(mtd->writesize, GFP_KERNEL);
	if (data_buf == NULL) {
		err = -ENOMEM;
		goto exit;
	}

	aml_oob_ops = kzalloc(sizeof(struct mtd_oob_ops), GFP_KERNEL);
	if (aml_oob_ops == NULL) {
		err = -ENOMEM;
		goto exit;
	}

	secure_oobinfo = (struct secure_oobinfo_t *)secure_oob_buf;
	while (amount_loaded < CONFIG_SECURE_SIZE) {
		aml_oob_ops->mode = MTD_OPS_AUTO_OOB;
		aml_oob_ops->len = mtd->writesize;
		aml_oob_ops->ooblen = sizeof(struct secure_oobinfo_t);
		aml_oob_ops->ooboffs = aml_oob_region.offset;
		aml_oob_ops->datbuf = data_buf;
		aml_oob_ops->oobbuf = secure_oob_buf;

		memset((uint8_t *)aml_oob_ops->datbuf, 0x0,
			mtd->writesize);
		memset((uint8_t *)aml_oob_ops->oobbuf, 0x0,
			aml_oob_ops->ooblen);

		error = mtd->_read_oob(mtd, addr, aml_oob_ops);
		if ((error != 0) && (error != -EUCLEAN)) {
			pr_err("blk check good but read failed: %llx, %d\n",
				(uint64_t)addr, error);
			err = -EIO;
			goto exit;
		}

		if (secure_oobinfo->name != SECURE_STORE_MAGIC)
			pr_err("%s() invalid magic: %llx, magic = ox%x\n",
				__func__,
				(uint64_t)addr, secure_oobinfo->name);

		addr += mtd->writesize;
		len = min(mtd->writesize, CONFIG_SECURE_SIZE - amount_loaded);
		memcpy(buf + amount_loaded, data_buf, len);
		amount_loaded += mtd->writesize;
	}
	if (amount_loaded < CONFIG_SECURE_SIZE) {
		err = -EIO;
		goto exit;
	}

	kfree(data_buf);
	kfree(aml_oob_ops);
	return 0;

exit:
	kfree(aml_oob_ops);
	aml_oob_ops = NULL;
	kfree(data_buf);
	data_buf = NULL;
	return err;
}

static int aml_nand_write_secure(struct mtd_info *mtd,
	loff_t offset, uint8_t *buf)
{
	struct secure_oobinfo_t *secure_oobinfo;
	struct mtd_oob_ops  *aml_oob_ops = NULL;
	struct mtd_oob_region aml_oob_region;
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	uint8_t secure_oob_buf[sizeof(struct secure_oobinfo_t)];
	size_t len, amount_saved = 0;
	uint8_t *data_buf = NULL;
	int error = 0, err;
	loff_t addr = 0;

	aml_oob_ops = kzalloc(sizeof(struct mtd_oob_ops), GFP_KERNEL);
	if (aml_oob_ops == NULL) {
		err = -ENOMEM;
		goto exit;
	}
	data_buf = kzalloc(mtd->writesize, GFP_KERNEL);
	if (data_buf == NULL) {
		err = -ENOMEM;
		goto exit;
	}
	err = mtd->ooblayout->free(mtd, 0,
		(struct mtd_oob_region *)&aml_oob_region);
	if (err != 0) {
		pr_err("%s() %d: read oob failed\n", __func__, __LINE__);
		err = -ENOMEM;
		goto exit;
	}

	addr = offset;
	secure_oobinfo = (struct secure_oobinfo_t *)secure_oob_buf;
	secure_oobinfo->name = SECURE_STORE_MAGIC;
	secure_oobinfo->timestamp =
		aml_chip->aml_nandsecure_info->secure_valid_node->timestamp;

	while (amount_saved < CONFIG_SECURE_SIZE) {
		aml_oob_ops->mode = MTD_OPS_AUTO_OOB;
		aml_oob_ops->len = mtd->writesize;
		aml_oob_ops->ooblen = sizeof(struct secure_oobinfo_t);
		aml_oob_ops->ooboffs = aml_oob_region.offset;
		aml_oob_ops->datbuf = data_buf;
		aml_oob_ops->oobbuf = secure_oob_buf;
		memset((uint8_t *)aml_oob_ops->datbuf, 0x0, mtd->writesize);
		len = min(mtd->writesize, CONFIG_SECURE_SIZE - amount_saved);
		memcpy((uint8_t *)aml_oob_ops->datbuf, buf + amount_saved, len);

		error = mtd->_write_oob(mtd, addr, aml_oob_ops);
		if (error) {
			pr_err("write failed: %llx, %d\n",
				(uint64_t)addr, error);
			err = 1;
			goto exit;
		}

		addr += mtd->writesize;
		amount_saved += mtd->writesize;
	}

	if (amount_saved < CONFIG_SECURE_SIZE) {
		err = 1;
		goto exit;
	}

	kfree(data_buf);
	kfree(aml_oob_ops);
	return 0;

exit:
	kfree(aml_oob_ops);
	aml_oob_ops = NULL;
	kfree(data_buf);
	data_buf = NULL;
	return err;
}

/*
 * alloc free node
 */
static struct env_free_node_t *alloc_fn(void)
{
	return kzalloc(sizeof(struct env_free_node_t), GFP_KERNEL);
}

int aml_nand_save_secure(struct mtd_info *mtd, uint8_t *buf)
{
	struct env_free_node_t *fn = NULL;
	struct env_free_node_t *tn;
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	struct erase_info  *nand_erase_info;
	int error = 0, pages_per_blk, i = 1;
	int16_t blk;
	loff_t addr = 0;
	struct secure_t *secure_ptr = (struct secure_t *)buf;
	struct aml_nandsecure_info_t *sinfo;
	struct env_valid_node_t *svn;

	sinfo = aml_chip->aml_nandsecure_info;
	if (!sinfo->secure_init)
		return 1;
	nand_erase_info = kzalloc(sizeof(struct erase_info), GFP_KERNEL);
	if (nand_erase_info == NULL) {
		pr_err("%s %d no mem for nand_erase_info\n",
			__func__, __LINE__);
		error = -ENOMEM;
		goto exit;
	}

	pages_per_blk = mtd->erasesize / mtd->writesize;
	if ((mtd->writesize < CONFIG_SECURE_SIZE)
		&& (sinfo->secure_valid == 1))
		i = (CONFIG_SECURE_SIZE + mtd->writesize - 1) / mtd->writesize;

	svn = sinfo->secure_valid_node;
	if (sinfo->secure_valid) {
		svn->phy_page_addr += i;
		/* if current valid node block is full, get a free one */
		if ((svn->phy_page_addr + i) > pages_per_blk) {

			fn = alloc_fn();
			if (fn == NULL) {
				error = -ENOMEM;
				goto exit;
			}
			tn = alloc_fn();
			if (tn == NULL) {
				pr_err("%s %d no mem for secure_tmp_node\n",
					__func__, __LINE__);
				error = -ENOMEM;
				goto exit;
			}
			#if 0
			/* fixme, bug here! */
			tn = (struct env_free_node_t *)(svn);

			fn = sinfo->secure_free_node;

			svn->phy_blk_addr = fn->phy_blk_addr;
			svn->phy_page_addr = 0;
			svn->timestamp += 1;
			sinfo->secure_free_node = tn;
			#endif
			blk = svn->phy_blk_addr;
			fn = sinfo->secure_free_node;
			svn->phy_blk_addr = fn->phy_blk_addr;
			svn->phy_page_addr = 0;
			svn->timestamp += 1;
			sinfo->secure_free_node->phy_blk_addr = blk;
		}
	} else {
		/* get a free node from free list */
		tn = sinfo->secure_free_node;
		svn->phy_blk_addr = tn->phy_blk_addr;
		svn->phy_page_addr = 0;
		svn->timestamp += 1;
		sinfo->secure_free_node = tn->next;
		kfree(tn);
	}

	addr = svn->phy_blk_addr;
	addr *= mtd->erasesize;
	addr += svn->phy_page_addr * mtd->writesize;

	if (svn->phy_page_addr == 0) {

		memset(nand_erase_info, 0, sizeof(struct erase_info));
		nand_erase_info->mtd = mtd;
		nand_erase_info->addr = addr;
		nand_erase_info->len = mtd->erasesize;

		aml_chip->key_protect = 1;
		aml_chip->secure_protect = 1;

		error = mtd->_erase(mtd, nand_erase_info);
		if (error) {
			pr_err("secure free blk erase failed %d\n", error);
			mtd->_block_markbad(mtd, addr);
			goto exit;
		}
		aml_chip->secure_protect = 0;
		aml_chip->key_protect = 0;

	}

	if (aml_nand_write_secure(mtd, addr, (uint8_t *) secure_ptr)) {
		pr_err("nand secure info update FAILED!\n");
		error = 1;
		goto exit;
	}
	pr_err("nand secure info save Ok\ns");
	kfree(nand_erase_info);
	return error;

exit:
	kfree(nand_erase_info);
	nand_erase_info = NULL;
	kfree(fn);
	fn = NULL;
	return error;
}


static int aml_nand_secure_init(struct mtd_info *mtd)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	struct nand_chip *chip = &aml_chip->chip;
	struct secure_oobinfo_t *secure_oobinfo;
	/* free node */
	struct env_free_node_t *fn;
	/* temp node */
	struct env_free_node_t *tn;
	/* previous node */
	struct env_free_node_t *pn;
	int error = 0, err, start_blk, tmp_blk;
	int secure_blk, i, pages_per_blk;
	int max_secure_blk, phys_erase_shift;
	loff_t offset;
	uint8_t *data_buf = NULL;
	uint32_t remain_start_block, remain_tatol_block;
	uint32_t remain_block, total_blk;
	struct mtd_oob_ops  *aml_oob_ops = NULL;
	struct mtd_oob_region aml_oob_region;
	uint8_t secure_oob_buf[sizeof(struct secure_oobinfo_t)];
	struct aml_nandsecure_info_t *sinfo;
	struct env_valid_node_t *svn;

	err = mtd->ooblayout->free(mtd, 0,
		(struct mtd_oob_region *)&aml_oob_region);

	if (err != 0) {
		pr_err("%s() %d: read oob free failed\n",
			__func__, __LINE__);
		err = -ENOMEM;
		goto exit;
	}

	aml_oob_ops = kzalloc(sizeof(struct mtd_oob_ops), GFP_KERNEL);
	if (aml_oob_ops == NULL) {
		err = -ENOMEM;
		goto exit;
	}
	data_buf = kzalloc(mtd->writesize, GFP_KERNEL);
	if (data_buf == NULL) {
		err = -ENOMEM;
		goto exit;
	}
	aml_chip->aml_nandsecure_info
		= kzalloc(sizeof(struct aml_nandsecure_info_t), GFP_KERNEL);
	if (aml_chip->aml_nandsecure_info == NULL) {
		err = -ENOMEM;
		goto exit;
	}

	sinfo = aml_chip->aml_nandsecure_info;
	sinfo->mtd = mtd;

	sinfo->secure_valid_node
		= kzalloc(sizeof(struct env_valid_node_t), GFP_KERNEL);
	if (sinfo->secure_valid_node == NULL) {
		pr_err("%s %d no mem\n", __func__, __LINE__);
		err = -ENOMEM;
		goto exit;
	}
	svn = sinfo->secure_valid_node;
	svn->phy_blk_addr = -1;

	phys_erase_shift = fls(mtd->erasesize) - 1;
	max_secure_blk = NAND_SECURE_BLK;

	offset = mtd->size - mtd->erasesize;
	total_blk = (int)(offset >> phys_erase_shift);

	pages_per_blk = (1 << (chip->phys_erase_shift - chip->page_shift));
	secure_oobinfo = (struct secure_oobinfo_t *)secure_oob_buf;
#if 1
	remain_tatol_block = REMAIN_BLOCK_NUM;
	remain_block = 0;
	remain_start_block = aml_chip->aml_nandkey_info->start_block - 1;
	sinfo->end_block = remain_start_block;
	do {
		offset = mtd->erasesize;
		offset *= remain_start_block;
		pr_err(">>>> off 0x%llx\n", offset);
		error = mtd->_block_isbad(mtd, offset);
		if (error == FACTORY_BAD_BLOCK_ERROR) {
			remain_start_block--;
			continue;
		}
		remain_start_block--;
	} while (++remain_block < remain_tatol_block);

	sinfo->start_block = (int)(offset >> phys_erase_shift);
	pr_err("%s,%d : secure start blk=%d end_blk=%d\n",
		__func__, __LINE__,
		sinfo->start_block,
		sinfo->end_block);
#else
	int bad_blk_cnt = 0;

	offset = mtd->size - mtd->erasesize;
	/* if without keys, scan secureblocks @ the tails */
	remain_start_block = (int)(offset >> phys_erase_shift);
	remain_block = 0;
	remain_tatol_block = REMAIN_BLOCK_NUM;
	sinfo->start_block = remain_start_block;
	sinfo->end_block = remain_start_block;
	bad_blk_cnt = 0;
	do {
		offset = mtd->erasesize;
		offset *= remain_start_block;
		error = mtd->_block_isbad(mtd, offset);
		if (error == FACTORY_BAD_BLOCK_ERROR) {
			sinfo->start_block--;
			remain_start_block--;
			continue;
		}
		remain_start_block--;
	} while (++remain_block < remain_tatol_block);
	sinfo->start_block -= (remain_block - 1);
	pr_err("secure start_blk=%d,end_blk=%d,%s:%d\n",
		sinfo->start_block,
		sinfo->end_block,
		__func__, __LINE__);
#endif

	tmp_blk = start_blk = sinfo->start_block;
	secure_blk = 0;
	do {

		offset = mtd->erasesize;
		offset *= start_blk;
		error = mtd->_block_isbad(mtd, offset);
		if (error)
			continue;
		aml_oob_ops->mode = MTD_OPS_AUTO_OOB;
		aml_oob_ops->len = mtd->writesize;
		aml_oob_ops->ooblen = sizeof(struct secure_oobinfo_t);
		aml_oob_ops->ooboffs = aml_oob_region.offset;
		aml_oob_ops->datbuf = data_buf;
		aml_oob_ops->oobbuf = secure_oob_buf;

		memset((uint8_t *)aml_oob_ops->datbuf,
			0x0, mtd->writesize);
		memset((uint8_t *)aml_oob_ops->oobbuf,
			0x0, aml_oob_ops->ooblen);

		error = mtd->_read_oob(mtd, offset, aml_oob_ops);
		if ((error != 0) && (error != -EUCLEAN)) {
			pr_err("blk check good but read failed: %llx, %d\n",
				(uint64_t)offset, error);
			continue;
		}

		sinfo->secure_init = 1;
		if ((secure_oobinfo->name
			== SECURE_STORE_MAGIC)) {
			sinfo->secure_valid = 1;
			if (svn->phy_blk_addr >= 0) {
				fn = alloc_fn();
				if (fn == NULL) {
					pr_err("%s %d no mem for secure_free_node\n",
						__func__, __LINE__);
					err = -ENOMEM;
					goto exit;
				}
				fn->dirty_flag = 1;
				if (secure_oobinfo->timestamp
					> svn->timestamp) {
					fn->phy_blk_addr
						= svn->phy_blk_addr;
					svn->phy_blk_addr
						= start_blk;
					svn->phy_page_addr = 0;
					svn->timestamp
						= secure_oobinfo->timestamp;
				} else
					fn->phy_blk_addr = start_blk;
				if (sinfo->secure_free_node == NULL)
					sinfo->secure_free_node = fn;
				else {
					tn = sinfo->secure_free_node;
					while (tn->next != NULL)
						tn = tn->next;
					tn->next = fn;
				}
			} else {
				svn->phy_blk_addr = start_blk;
				svn->phy_page_addr = 0;
				svn->timestamp = secure_oobinfo->timestamp;
			}
		} else if (secure_blk < max_secure_blk) {
			fn = alloc_fn();
			if (fn == NULL) {
				err = -ENOMEM;
				goto exit;
			}
			fn->phy_blk_addr = start_blk;
			if (sinfo->secure_free_node == NULL) {
				sinfo->secure_free_node = fn;
			} else {
				tn = sinfo->secure_free_node;
				pn = tn;
				while (tn != NULL) {
					if (tn->dirty_flag == 1)
						break;
					pn = tn;
					tn = tn->next;
				}
				if (pn == tn) {
					fn->next = tn;
					sinfo->secure_free_node = fn;
				} else {
					pn->next = fn;
					fn->next = tn;
				}
			}
		}
		secure_blk++;

		if ((secure_blk >= max_secure_blk)
			&& (sinfo->secure_valid == 1))
			break;
	}  while ((++start_blk) <= sinfo->end_block);

	if (sinfo->secure_valid == 1) {
		aml_oob_ops->mode = MTD_OPS_AUTO_OOB;
		aml_oob_ops->len = mtd->writesize;
		aml_oob_ops->ooblen = sizeof(struct secure_oobinfo_t);
		aml_oob_ops->ooboffs = aml_oob_region.offset;
		aml_oob_ops->datbuf = data_buf;
		aml_oob_ops->oobbuf = secure_oob_buf;

		for (i = 0; i < pages_per_blk; i++) {

			memset((uint8_t *)aml_oob_ops->datbuf,
				0x0, mtd->writesize);
			memset((uint8_t *)aml_oob_ops->oobbuf,
				0x0, aml_oob_ops->ooblen);

			offset = svn->phy_blk_addr;
			offset *= mtd->erasesize;
			offset += i * mtd->writesize;
			error = mtd->_read_oob(mtd, offset, aml_oob_ops);
			if ((error != 0) && (error != -EUCLEAN)) {
				pr_err("blk check good but read failed: %llx, %d\n",
					(uint64_t)offset, error);
				continue;
			}

			if (secure_oobinfo->name == SECURE_STORE_MAGIC)
				svn->phy_page_addr = i;
			else
				break;
		}
	}
	if ((mtd->writesize < CONFIG_SECURE_SIZE)
			&& (sinfo->secure_valid == 1)) {
		i = (CONFIG_SECURE_SIZE + mtd->writesize - 1) / mtd->writesize;
		svn->phy_page_addr -= (i - 1);
	}

	pr_err("secure_valid_node->add =%d\n",
		svn->phy_blk_addr);
	if (sinfo->secure_free_node)
		pr_err("secure_free_node->add =%d\n",
			sinfo->secure_free_node->phy_blk_addr);

	offset = svn->phy_blk_addr;
	offset *= mtd->erasesize;
	offset += mtd->writesize
		* svn->phy_page_addr;
	pr_err("aml nand secure info valid addr: %llx\n", (uint64_t)offset);
	pr_err("CONFIG_SECURE_SIZE=0x%x;\n", CONFIG_SECURE_SIZE);

	kfree(data_buf);
	kfree(aml_oob_ops);
	return 0;

exit:
	kfree(data_buf);
	data_buf = NULL;
	kfree(aml_oob_ops);
	aml_oob_ops = NULL;
	kfree(aml_chip->aml_nandsecure_info->secure_valid_node);
	aml_chip->aml_nandsecure_info->secure_valid_node = NULL;
	kfree(aml_chip->aml_nandsecure_info);
	aml_chip->aml_nandsecure_info = NULL;
	kfree(fn);
	fn = NULL;
	return err;

}

static int secure_info_check(struct mtd_info *mtd)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	struct secure_t *secure_ptr;
	int error = 0;

	error = aml_nand_secure_init(mtd);
	if (error)
		return error;

	secure_ptr = kzalloc(sizeof(struct secure_t), GFP_KERNEL);
	if (secure_ptr == NULL)
		return -ENOMEM;
	/* default secure data set to a5; */
	memset(secure_ptr, 0xa5, sizeof(struct secure_t));

	if (aml_chip->aml_nandsecure_info->secure_valid == 1) {

		goto exit;
	} else {
		pr_info("nand secure info save\n");
		error = aml_nand_save_secure(mtd, (uint8_t *)secure_ptr);
		if (error)
			pr_err("nand secure info save failed\n");
	}

exit:
	kfree(secure_ptr);
	return 0;

}

int secure_storage_nand_read(char *buf, uint32_t len)
{
	int err, size;
	uint8_t *storage_buf = NULL;

	if (nand_secure_mtd == NULL) {
		pr_err("secure storage is init fail\n");
		return 1;
	}
	storage_buf = kzalloc(CONFIG_SECURE_SIZE, GFP_KERNEL);
	if (storage_buf == NULL)
		return -ENOMEM;
	err = aml_nand_read_secure(nand_secure_mtd, 0, storage_buf);
	if (err < 0) {
		pr_err("%s:%d,read fail\n", __func__, __LINE__);
		kfree(storage_buf);
		return err;
	} else if (err == 1) {
		pr_err("%s:%d,no any key\n", __func__, __LINE__);
		return 0;
	}

	if (len > CONFIG_SECURE_SIZE) {
		size = CONFIG_SECURE_SIZE;
		pr_info("%s() %d: len %d, %d, act %d\n",
			__func__, __LINE__,
			len, CONFIG_SECURE_SIZE,
			CONFIG_SECURE_SIZE);
	} else
		size = len;

	memcpy(buf, storage_buf, size);
	kfree(storage_buf);
	return 0;
}

int secure_storage_nand_write(char *buf, uint32_t len)
{
	int err = 0, size;
	uint8_t *storage_buf = NULL;

	if (nand_secure_mtd == NULL) {
		pr_err("secure storage init fail\n");
		return 1;
	}
	storage_buf = kzalloc(CONFIG_SECURE_SIZE, GFP_KERNEL);
	if (storage_buf == NULL)
		return -ENOMEM;

	if (len > CONFIG_SECURE_SIZE) {
		size = CONFIG_SECURE_SIZE;
		pr_err("%s()%d:　len:%x %x, act %x byte\n",
			__func__, __LINE__,
			len, CONFIG_SECURE_SIZE,
			CONFIG_SECURE_SIZE);
	} else
		size = len;
	memcpy(storage_buf, buf, size);
	err = aml_nand_save_secure(nand_secure_mtd, storage_buf);
	if (err)
		pr_err("%s:%d,secure storage write fail\n",
			__func__, __LINE__);

	kfree(storage_buf);
	return err;
}

int secure_device_init(struct mtd_info *mtd)
{
	int ret = 0;

	nand_secure_mtd = mtd;
	pr_info("%s(): %d\n", __func__, __LINE__);

	ret = secure_info_check(mtd);
	if (ret)
		pr_err("invalid secure info\n");

	return ret;

}

