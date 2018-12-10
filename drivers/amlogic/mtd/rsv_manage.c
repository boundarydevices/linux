/*
 * drivers/amlogic/mtd/rsv_manage.c
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
static struct free_node_t *get_free_node(struct mtd_info *mtd)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	unsigned long index;

	pr_info("%s %d: bitmap=%llx\n", __func__, __LINE__,
		aml_chip->freeNodeBitmask);

	index = find_first_zero_bit((void *)&aml_chip->freeNodeBitmask,
				    RESERVED_BLOCK_NUM);
	if (index > RESERVED_BLOCK_NUM) {
		pr_info("%s %d: index is greater than max! error",
			__func__, __LINE__);
		return NULL;
	}
	WARN_ON(test_and_set_bit(index, (void *)&aml_chip->freeNodeBitmask));

	pr_info("%s %d: bitmap=%llx\n", __func__, __LINE__,
		aml_chip->freeNodeBitmask);

	return aml_chip->free_node[index];

}

static void release_free_node(struct mtd_info *mtd,
	struct free_node_t *free_node)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	unsigned int index_save = free_node->index;

	pr_info("%s %d: bitmap=%llx\n", __func__, __LINE__,
		aml_chip->freeNodeBitmask);

	if (free_node->index > RESERVED_BLOCK_NUM)
		pr_info("%s %d: index=%d is greater than max! error",
			__func__, __LINE__, free_node->index);

	WARN_ON(!test_and_clear_bit(free_node->index,
				(void *)&aml_chip->freeNodeBitmask));

	/*memset zero to protect from dead-loop*/
	memset(free_node, 0, sizeof(struct free_node_t));
	free_node->index = index_save;
	pr_info("%s %d: bitmap=%llx\n", __func__, __LINE__,
		aml_chip->freeNodeBitmask);
}


int aml_nand_rsv_erase_protect(struct mtd_info *mtd, unsigned int block_addr)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	unsigned int bbt_start, bbt_end, key_start, key_end;

	if (!_aml_rsv_isprotect())
		return 0;

	if (!aml_chip->aml_nandbbt_info)
		return -1;

	if (!aml_chip->aml_nandkey_info)
		return -1;

	bbt_start = aml_chip->aml_nandbbt_info->start_block;
	bbt_end = aml_chip->aml_nandbbt_info->end_block;
	key_start = aml_chip->aml_nandkey_info->start_block;
	key_end = aml_chip->aml_nandkey_info->end_block;

#ifdef AML_NAND_UBOOT
	if (aml_chip->aml_nandkey_info != NULL) {
		if (aml_chip->aml_nandkey_info->valid)
			if ((!(info_disprotect & DISPROTECT_KEY))
			&& ((block_addr >= key_start)
			&& (block_addr < key_end)))
				return -1; /*need skip key blocks*/
	}
	if (aml_chip->aml_nandbbt_info != NULL) {
		if (aml_chip->aml_nandbbt_info->valid)
			if ((block_addr >= bbt_start)
			&& (block_addr < bbt_end))
				return -1; /*need skip bbt blocks*/
	}
#else

	if (aml_chip->aml_nandkey_info->valid)
		if ((block_addr >= key_start)
		&& (block_addr < key_end))
			return -1; /*need skip key blocks*/


	if (aml_chip->aml_nandbbt_info->valid)
		if ((block_addr >= bbt_start)
		&& (block_addr < bbt_end))
			return -1; /*need skip bbt blocks*/

#endif
	return 0;
}


/* only read bad block  labeled ops */
int aml_nand_scan_shipped_bbt(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	struct aml_nand_platform *plat = aml_chip->platform;
	unsigned char *data_buf;
	int32_t read_cnt, page, pages_per_blk;
	loff_t addr;
	int  start_blk = 0, total_blk = 0, bad_blk_cnt = 0, phys_erase_shift;
	int realpage, col0_data = 0, col0_oob = 0, valid_page_num = 1;
	int col_data_sandisk[6] = {0}, bad_sandisk_flag = 0;
	int i, j;
	uint32_t offset;

	if (!mtd->erasesize)
		return -EINVAL;

	phys_erase_shift = fls(mtd->erasesize) - 1;
	chip->pagebuf = -1;
	pages_per_blk = (1 << (chip->phys_erase_shift - chip->page_shift));

	data_buf = kzalloc(mtd->writesize, GFP_KERNEL);
	if (data_buf == NULL)
		return -ENOMEM;

	if ((strncmp((char *)plat->name,
		NAND_BOOT_NAME, strlen((const char *)NAND_BOOT_NAME)))) {
		memset(&aml_chip->nand_bbt_info->nand_bbt[0],
			0, MAX_BAD_BLK_NUM);
		if (nand_boot_flag)
			offset = (1024 * mtd->writesize /
			aml_chip->plane_num);
		else
			offset = 0;

		start_blk = (int)(offset >> phys_erase_shift);
		total_blk = (int)(mtd->size >> phys_erase_shift);
	}

	pr_info("scanning flash chip_num=%d %d %d %d\n",  controller->chip_num,
		aml_chip->valid_chip[0], aml_chip->valid_chip[1],
		aml_chip->valid_chip[2]);
	do {
		offset = mtd->erasesize;
		offset *= start_blk;
	for (i = 0; i < controller->chip_num; i++) {
		/* if (aml_chip->valid_chip[i]) { */
		for (read_cnt = 0; read_cnt < 2; read_cnt++) {
			if (aml_chip->mfr_type  == NAND_MFR_SANDISK) {
				addr = (loff_t)offset + read_cnt*mtd->writesize;
			} else
				addr = (loff_t)offset +
			(pages_per_blk - 1) * read_cnt * mtd->writesize;

			realpage = (int)(addr >> chip->page_shift);
			page = realpage & chip->pagemask;

			if (page != -1) {
				valid_page_num =
					mtd->writesize>>chip->page_shift;
				valid_page_num /= aml_chip->plane_num;

				aml_chip->page_addr = page / valid_page_num;
	if (unlikely(aml_chip->page_addr >= aml_chip->internal_page_nums)) {
		aml_chip->page_addr -= aml_chip->internal_page_nums;
		aml_chip->page_addr |=
		(1 << aml_chip->internal_chip_shift)*aml_chip->internal_chipnr;
	}
			}
			if (aml_chip->plane_num == 2) {
				chip->select_chip(mtd, i);
				aml_chip->aml_nand_wait_devready(aml_chip, i);
				if (aml_nand_get_fbb_issue()) {
					chip->cmd_ctrl(mtd,
						NAND_CMD_SEQIN, NAND_CTRL_CLE);
					chip->cmd_ctrl(mtd,
						0, NAND_CTRL_ALE);
				}
				aml_chip->aml_nand_command(aml_chip,
					NAND_CMD_READ0,
					0x00, aml_chip->page_addr, i);

			if (!aml_chip->aml_nand_wait_devready(aml_chip, i))
				pr_info("%s, %d,selected chip%d not ready\n",
					__func__, __LINE__, i);

				if (aml_chip->ops_mode & AML_CHIP_NONE_RB)
					chip->cmd_ctrl(mtd,
					NAND_CMD_READ0 & 0xff,
					NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);
				udelay(2);
				aml_chip->aml_nand_command(aml_chip,
					NAND_CMD_TWOPLANE_READ1,
					0x00, aml_chip->page_addr, i);
				udelay(2);

		if (aml_chip->mfr_type  == NAND_MFR_SANDISK) {
			for (j = 0; j < 6; j++)
				col_data_sandisk[j] = chip->read_byte(mtd);
		} else
			col0_data = chip->read_byte(mtd);

				aml_chip->aml_nand_command(aml_chip,
					NAND_CMD_TWOPLANE_READ2,
					aml_chip->page_size,
					aml_chip->page_addr, i);
				udelay(2);

				if (aml_chip->mfr_type  == NAND_MFR_SANDISK)
					col0_oob = 0x0;
				else
					col0_oob = chip->read_byte(mtd);
				/* pr_info("col0_oob=%x\n",col0_oob); */

			} else if (aml_chip->plane_num == 1) {
				chip->select_chip(mtd, i);
				/* nand_get_chip(); */
				/*aml_chip->aml_nand_select_chip(aml_chip, i);*/
				if (aml_nand_get_fbb_issue()) {
					chip->cmd_ctrl(mtd,
						NAND_CMD_SEQIN, NAND_CTRL_CLE);
					chip->cmd_ctrl(mtd,
						0, NAND_CTRL_ALE);
				}
				aml_chip->aml_nand_command(aml_chip,
					NAND_CMD_READ0, 0x00,
					aml_chip->page_addr, i);
				udelay(2);

			if (!aml_chip->aml_nand_wait_devready(aml_chip, i))
				pr_info("%s, %d,selected chip%d not ready\n",
					__func__, __LINE__, i);

				if (aml_chip->ops_mode & AML_CHIP_NONE_RB)
					chip->cmd_ctrl(mtd,
					NAND_CMD_READ0 & 0xff,
					NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);
				udelay(2);

			if (aml_chip->mfr_type  == NAND_MFR_SANDISK) {
				for (j = 0; j < 6; j++)
					col_data_sandisk[j] =
						chip->read_byte(mtd);
			} else
				col0_data = chip->read_byte(mtd);

			/* pr_info("col0_data =%x\n",col0_data); */

			if (aml_chip->mfr_type  != NAND_MFR_SANDISK)
				aml_chip->aml_nand_command(aml_chip,
					NAND_CMD_RNDOUT,
					aml_chip->page_size, -1, i);
			udelay(2);

			if (aml_chip->mfr_type  == NAND_MFR_SANDISK)
				col0_oob = 0x0;
			else
				col0_oob = chip->read_byte(mtd);
				/* pr_info("col0_oob =%x\n",col0_oob); */
			}

	if ((aml_chip->mfr_type  == 0xC8)) {
		if ((col0_oob != 0xFF) || (col0_data != 0xFF)) {
			pr_info("detect factory Bad blk:%llx blk:%d chip:%d\n",
				(uint64_t)addr, start_blk, i);
			aml_chip->nand_bbt_info->nand_bbt[bad_blk_cnt++] =
				start_blk | 0x8000;
			aml_chip->block_status[start_blk] = NAND_FACTORY_BAD;
			break;
		}
	}

	if ((col0_oob == 0xFF))
		continue;

	if (col0_oob != 0xFF) {
		pr_info("%s:%d factory ship bbt found\n", __func__, __LINE__);
		if (aml_chip->mfr_type  == 0xc2) {
			if (col0_oob != 0xFF) {
				pr_info("factory Bad blk:%llx blk=%d chip=%d\n",
					(uint64_t)addr, start_blk, i);
			aml_chip->nand_bbt_info->nand_bbt[bad_blk_cnt++] =
				start_blk|0x8000;
			aml_chip->block_status[start_blk] = NAND_FACTORY_BAD;
				break;
			}
		}

		if (aml_chip->mfr_type  == NAND_MFR_DOSILICON ||
		    aml_chip->mfr_type  == NAND_MFR_ATO ||
			aml_chip->mfr_type  == NAND_MFR_HYNIX) {
			if (col0_oob != 0xFF) {
				pr_info("factory Bad blk:%llx blk=%d chip=%d\n",
				       (uint64_t)addr, start_blk, i);
			aml_chip->nand_bbt_info->nand_bbt[bad_blk_cnt++] =
				start_blk|0x8000;
			aml_chip->block_status[start_blk] = NAND_FACTORY_BAD;
				break;
			}
		}

		if (aml_chip->mfr_type  == 0xef) {
			if (col0_oob != 0xFF) {
				pr_info("factory Bad blk:%llx blk=%d chip=%d\n",
					(uint64_t)addr, start_blk, i);
			aml_chip->nand_bbt_info->nand_bbt[bad_blk_cnt++] =
				start_blk|0x8000;
			aml_chip->block_status[start_blk] = NAND_FACTORY_BAD;
				break;
			}
		}

		if (aml_chip->mfr_type  == NAND_MFR_SANDISK) {
			for (j = 0; j < 6; j++) {
				if (col_data_sandisk[j] == 0x0) {
					bad_sandisk_flag = 1;
					break;
				}
			}
			if (bad_sandisk_flag) {
				pr_info("factory Bad blk:%llx blk=%d chip=%d\n",
					(uint64_t)addr, start_blk, i);
			aml_chip->nand_bbt_info->nand_bbt[bad_blk_cnt++] =
				start_blk|0x8000;
			aml_chip->block_status[start_blk] = NAND_FACTORY_BAD;
				bad_sandisk_flag = 0;
				break;
			}
		}

		if (aml_chip->mfr_type  == NAND_MFR_SAMSUNG) {
			if ((col0_oob != 0xFF) && (col0_data != 0xFF)) {
				pr_info("factory Bad blk:%llx blk=%d chip=%d\n",
					(uint64_t)addr, start_blk, i);
			aml_chip->nand_bbt_info->nand_bbt[bad_blk_cnt++] =
				start_blk|0x8000;
			aml_chip->block_status[start_blk] = NAND_FACTORY_BAD;
					break;
			}
		}

		if (aml_chip->mfr_type  == NAND_MFR_TOSHIBA) {
			if ((col0_oob != 0xFF) && (col0_data != 0xFF)) {
				pr_info("factory Bad blk:%llx blk=%d chip=%d\n",
					(uint64_t)addr, start_blk, i);
			aml_chip->nand_bbt_info->nand_bbt[bad_blk_cnt++] =
				start_blk|0x8000;
			aml_chip->block_status[start_blk] = NAND_FACTORY_BAD;
				break;
			}
		}

		if (aml_chip->mfr_type  == NAND_MFR_MICRON) {
			if (col0_oob == 0x0) {
				pr_info("factory Bad blk:%llx blk=%d chip=%d\n",
					(uint64_t)addr, start_blk, i);
			aml_chip->nand_bbt_info->nand_bbt[bad_blk_cnt++] =
				start_blk|0x8000;
			aml_chip->block_status[start_blk] = NAND_FACTORY_BAD;
				break;
			}
		}
	}
		}
		/* } */
	}
	} while ((++start_blk) < total_blk);

	pr_info("aml_nand_scan_bbt: factory Bad block bad_blk_cnt=%d\n",
		bad_blk_cnt);
	kfree(data_buf);
	return 0;
}

int aml_nand_read_rsv_info(struct mtd_info *mtd,
	struct aml_nandrsv_info_t *nandrsv_info, size_t offset, u_char *buf)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	struct oobinfo_t *oobinfo;
	int error = 0, ret = 0;
	loff_t addr = 0;
	size_t amount_loaded = 0;
	size_t len;
	struct mtd_oob_ops aml_oob_ops;
	unsigned char *data_buf;
	unsigned char oob_buf[sizeof(struct oobinfo_t)];

READ_RSV_AGAIN:
	addr = nandrsv_info->valid_node->phy_blk_addr;
	addr *= mtd->erasesize;
	addr += nandrsv_info->valid_node->phy_page_addr * mtd->writesize;
	pr_info("%s:%d,read %s info to %llx\n", __func__, __LINE__,
		nandrsv_info->name, addr);

	data_buf = aml_chip->rsv_data_buf;

	oobinfo = (struct oobinfo_t *)oob_buf;
	while (amount_loaded < nandrsv_info->size) {
		aml_oob_ops.mode = MTD_OPS_AUTO_OOB;
		aml_oob_ops.len = mtd->writesize;
		aml_oob_ops.ooblen = sizeof(struct oobinfo_t);
		/*aml_oob_ops.ooboffs = mtd->ecclayout->oobfree[0].offset;*/
		aml_oob_ops.ooboffs = 0;
		aml_oob_ops.datbuf = data_buf;
		aml_oob_ops.oobbuf = oob_buf;

		memset((unsigned char *)aml_oob_ops.datbuf,
			0x0, mtd->writesize);
		memset((unsigned char *)aml_oob_ops.oobbuf,
			0x0, aml_oob_ops.ooblen);

		error = mtd->_read_oob(mtd, addr, &aml_oob_ops);
		if ((error != 0) && (error != -EUCLEAN)) {
			pr_info("blk good but read failed: %llx, %d\n",
				(uint64_t)addr, error);
			ret = aml_nand_scan_rsv_info(mtd, nandrsv_info);
			if (ret == -1)
				return 1;
			goto READ_RSV_AGAIN;
		}

		if (memcmp(oobinfo->name, nandrsv_info->name, 4))
			pr_info("invalid nand info %s magic: %llx\n",
				nandrsv_info->name, (uint64_t)addr);

		addr += mtd->writesize;
		len = min_t(uint32_t, mtd->writesize,
			(nandrsv_info->size - amount_loaded));
		memcpy(buf + amount_loaded, data_buf, len);
		amount_loaded += mtd->writesize;
	}
	if (amount_loaded < nandrsv_info->size)
		return 1;
#if 0
	uint64_t  dump_len = 0;
	unsigned char *tmp =  NULL;

	if (!strncmp(nandrsv_info->name,
		KEY_NAND_MAGIC, strlen(nandrsv_info->name))) {
		tmp = buf;
		dump_len = nandrsv_info->size / 16;
		while (dump_len--) {
			pr_info("\t%02x %02x %02x %02x %02x %02x %02x %02x",
				tmp[0], tmp[1], tmp[2], tmp[3],
				tmp[4], tmp[5], tmp[6], tmp[7]);
			pr_info(" %02x %02x %02x %02x %02x %02x %02x %02x\n",
				tmp[8], tmp[9], tmp[10], tmp[11],
				tmp[12], tmp[13], tmp[14], tmp[15]);
			tmp += 16;
		}
	}
#endif
	/* if(data_buf) */
	/* kfree(data_buf); */
	return 0;
}

int aml_nand_read_env(struct mtd_info *mtd, size_t offset, u_char *buf)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);

	if (!aml_chip->aml_nandenv_info->valid) {
		pr_info("%s %d: %s is invalid! read exit!\n",
			__func__, __LINE__,
			aml_chip->aml_nandenv_info->name);
		return 1;
	}
	if (aml_nand_read_rsv_info(mtd,
		aml_chip->aml_nandenv_info, offset, (u_char *)buf))
		return 1;
	return 0;
}

int aml_nand_read_key(struct mtd_info *mtd, size_t offset, u_char *buf)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);

	if (!aml_chip->aml_nandkey_info->valid) {
		pr_info("%s %d: %s is invalid! read exit!\n",
			__func__, __LINE__,
			aml_chip->aml_nandkey_info->name);
		return 1;
	}
	if (aml_nand_read_rsv_info(mtd,
		aml_chip->aml_nandkey_info, offset, (u_char *)buf))
		return 1;
	return 0;
}

int aml_nand_read_dtb(struct mtd_info *mtd, size_t offset, u_char *buf)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);

	if (!aml_chip->aml_nanddtb_info->valid) {
		pr_info("%s %d: %s is invalid! read exit!",
			__func__, __LINE__,
			aml_chip->aml_nanddtb_info->name);
		return 1;
	}
	if (aml_nand_read_rsv_info(mtd,
		aml_chip->aml_nanddtb_info, offset, (u_char *)buf))
		return 1;
	return 0;
}

static int aml_nand_write_rsv(struct mtd_info *mtd,
	struct aml_nandrsv_info_t *nandrsv_info, loff_t offset, u_char *buf)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	struct oobinfo_t *oobinfo;
	int error = 0;
	loff_t addr = 0;
	size_t amount_saved = 0;
	size_t len;
	struct mtd_oob_ops aml_oob_ops;
	unsigned char *data_buf;
	unsigned char oob_buf[sizeof(struct oobinfo_t)];

	data_buf = aml_chip->rsv_data_buf;
	/* if (data_buf == NULL) */
	/* return -ENOMEM; */

	addr = offset;
	pr_info("%s:%d,write info to %llx\n", __func__, __LINE__, addr);
	oobinfo = (struct oobinfo_t *)oob_buf;
	memcpy(oobinfo->name, nandrsv_info->name, 4);
	oobinfo->ec = nandrsv_info->valid_node->ec;
	oobinfo->timestamp = nandrsv_info->valid_node->timestamp;

#if 0
	uint64_t  dump_len = 0;
	unsigned char *tmp =  NULL;

	if (!strncmp(nandrsv_info->name,
		KEY_NAND_MAGIC, strlen(nandrsv_info->name))) {
		tmp = buf;
		dump_len = nandrsv_info->size / 16;
		while (dump_len--) {
			pr_info("\t%02x %02x %02x %02x %02x %02x %02x %02x",
				tmp[0], tmp[1], tmp[2], tmp[3],
				tmp[4], tmp[5], tmp[6], tmp[7]);
			pr_info(" %02x %02x %02x %02x %02x %02x %02x %02x\n",
				tmp[8], tmp[9], tmp[10], tmp[11],
				tmp[12], tmp[13], tmp[14], tmp[15]);
			tmp += 16;
		}
	}
#endif
	while (amount_saved < nandrsv_info->size) {
		aml_oob_ops.mode = MTD_OPS_AUTO_OOB;
		aml_oob_ops.len = mtd->writesize;
		aml_oob_ops.ooblen = sizeof(struct oobinfo_t);
		aml_oob_ops.ooboffs = 0;/*mtd->ecclayout->oobfree[0].offset;*/
		aml_oob_ops.datbuf = data_buf;
		aml_oob_ops.oobbuf = oob_buf;

		memset((unsigned char *)aml_oob_ops.datbuf,
			0x0, mtd->writesize);
		len = min_t(uint32_t, mtd->writesize,
			nandrsv_info->size - amount_saved);
		memcpy((unsigned char *)aml_oob_ops.datbuf,
			buf + amount_saved, len);

		error = mtd->_write_oob(mtd, addr, &aml_oob_ops);
		if (error) {
			pr_info("blk check good but write failed: %llx, %d\n",
				(uint64_t)addr, error);
			return 1;
		}
		addr += mtd->writesize;
		amount_saved += mtd->writesize;
	}
	if (amount_saved < nandrsv_info->size)
		return 1;
	/* kfree(data_buf); */
	return 0;
}

int aml_nand_save_rsv_info(struct mtd_info *mtd,
	struct aml_nandrsv_info_t *nandrsv_info, u_char *buf)
{
	struct free_node_t *free_node = NULL, *tmp_node = NULL;
	int error = 0, pages_per_blk, valid_page_addr, i = 1;
	loff_t addr = 0;
	struct erase_info erase_info;

	pages_per_blk = mtd->erasesize / mtd->writesize;
	/*solve these abnormals caused by power off and ecc error*/
	if ((nandrsv_info->valid_node->status & POWER_ABNORMAL_FLAG)
		|| (nandrsv_info->valid_node->status & ECC_ABNORMAL_FLAG))
		nandrsv_info->valid_node->phy_page_addr = pages_per_blk;

	if (mtd->writesize < nandrsv_info->size)
		i = (nandrsv_info->size + mtd->writesize - 1) / mtd->writesize;
	pr_info("%s:%d, %s: valid=%d, pages=%d\n", __func__, __LINE__,
		nandrsv_info->name, nandrsv_info->valid, i);
RE_SEARCH:
	if (nandrsv_info->valid) {
		nandrsv_info->valid_node->phy_page_addr += i;
		valid_page_addr = nandrsv_info->valid_node->phy_page_addr;
		if ((valid_page_addr + i) > pages_per_blk) {
			if ((valid_page_addr - i) == pages_per_blk) {
				addr = nandrsv_info->valid_node->phy_blk_addr;
				addr *= mtd->erasesize;
				memset(&erase_info,
					0, sizeof(struct erase_info));
				erase_info.mtd = mtd;
				erase_info.addr = addr;
				erase_info.len = mtd->erasesize;
				_aml_rsv_disprotect();
				error = mtd->_erase(mtd, &erase_info);
				_aml_rsv_protect();
				nandrsv_info->valid_node->ec++;
				pr_info("---erase bad env block:%llx\n", addr);
			}
			/* free_node = kzalloc(sizeof(struct free_node_t), */
			/* GFP_KERNEL); */
			free_node = get_free_node(mtd);
			if (free_node == NULL)
				return -ENOMEM;

			free_node->phy_blk_addr =
				nandrsv_info->valid_node->phy_blk_addr;
			free_node->ec = nandrsv_info->valid_node->ec;
			tmp_node = nandrsv_info->free_node;
			while (tmp_node->next != NULL)
				tmp_node = tmp_node->next;

			tmp_node->next = free_node;

			tmp_node = nandrsv_info->free_node;
			nandrsv_info->valid_node->phy_blk_addr =
				tmp_node->phy_blk_addr;
			nandrsv_info->valid_node->phy_page_addr = 0;
			nandrsv_info->valid_node->ec = tmp_node->ec;
			nandrsv_info->valid_node->timestamp += 1;
			nandrsv_info->free_node = tmp_node->next;
			release_free_node(mtd, tmp_node);
		}
	} else {
		tmp_node = nandrsv_info->free_node;
		nandrsv_info->valid_node->phy_blk_addr = tmp_node->phy_blk_addr;
		nandrsv_info->valid_node->phy_page_addr = 0;
		nandrsv_info->valid_node->ec = tmp_node->ec;
		nandrsv_info->valid_node->timestamp += 1;
		nandrsv_info->free_node = tmp_node->next;
		release_free_node(mtd, tmp_node);
	}

	addr = nandrsv_info->valid_node->phy_blk_addr;
	addr *= mtd->erasesize;
	addr += nandrsv_info->valid_node->phy_page_addr * mtd->writesize;

	pr_info("%s:%d,save info to %llx\n", __func__, __LINE__, addr);

	if (nandrsv_info->valid_node->phy_page_addr == 0) {
		error = mtd->_block_isbad(mtd, addr);
		if (error != 0) {
			/*
			 *bad block here, need fix it
			 *because of info_blk list may be include bad block,
			 *so we need check it and done here. if don't,
			 *some bad blocks may be erase here
			 *and env will lost or too much ecc error
			 **/
			pr_info("have bad block in info_blk list!!!!\n");
			nandrsv_info->valid_node->phy_page_addr =
				pages_per_blk - i;
			goto RE_SEARCH;
		}

		memset(&erase_info, 0, sizeof(struct erase_info));
		erase_info.mtd = mtd;
		erase_info.addr = addr;
		erase_info.len = mtd->erasesize;
		_aml_rsv_disprotect();
		error = mtd->_erase(mtd, &erase_info);
		_aml_rsv_protect();
		if (error) {
			pr_info("env free blk erase failed %d\n", error);
			mtd->_block_markbad(mtd, addr);
			return error;
		}
		nandrsv_info->valid_node->ec++;
	}

	if (aml_nand_write_rsv(mtd, nandrsv_info, addr, (u_char *) buf)) {
		pr_info("update nand env FAILED!\n");
		return 1;
	}
	if (!nandrsv_info->valid)
		nandrsv_info->valid = 1;
	/* clear status when write successfully*/
	nandrsv_info->valid_node->status = 0;
	return error;
}

int aml_nand_save_env(struct mtd_info *mtd, u_char *buf)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	/* env_t *env_ptr = (env_t *)buf; */

	if (!aml_chip->aml_nandenv_info->init) {
		pr_info("%s %d %s not init\n",
			__func__, __LINE__,
			aml_chip->aml_nandenv_info->name);
		return 1;
	}
	/*fixit*/
	/* env_ptr->crc = crc32(0, env_ptr->data, ENV_SIZE); */
	if (aml_nand_save_rsv_info(mtd, aml_chip->aml_nandenv_info, buf))
		return 1;
	return 0;
}

int aml_nand_save_key(struct mtd_info *mtd, u_char *buf)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	/* env_t *env_ptr = (env_t *)buf; */

	if (!aml_chip->aml_nandkey_info->init) {
		pr_info("%s %d %s not init\n",
			__func__, __LINE__,
			aml_chip->aml_nandkey_info->name);
		return 1;
	}
	/*fixit*/
	/* env_ptr->crc = crc32(0, env_ptr->data, ENV_SIZE); */
	if (aml_nand_save_rsv_info(mtd, aml_chip->aml_nandkey_info, buf))
		return 1;
	return 0;
}

int aml_nand_save_dtb(struct mtd_info *mtd, u_char *buf)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	/* env_t *env_ptr = (env_t *)buf; */

	if (!aml_chip->aml_nanddtb_info->init) {
		pr_info("%s %d %s not init\n",
			__func__, __LINE__,
			aml_chip->aml_nanddtb_info->name);
		return 1;
	}
	/*fixit*/
	/* env_ptr->crc = crc32(0, env_ptr->data, ENV_SIZE); */
	if (aml_nand_save_rsv_info(mtd, aml_chip->aml_nanddtb_info, buf))
		return 1;
	return 0;
}

int aml_nand_save_bbt(struct mtd_info *mtd, u_char *buf)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);

	if (!aml_chip->aml_nandbbt_info->init) {
		pr_info("%s %d %s not init\n",
			__func__, __LINE__,
			aml_chip->aml_nandbbt_info->name);
		return 1;
	}
	if (aml_nand_save_rsv_info(mtd, aml_chip->aml_nandbbt_info, buf))
		return 1;
	return 0;
}

int aml_nand_rsv_info_init(struct mtd_info *mtd)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	struct nand_chip *chip = mtd->priv;
	unsigned int pages_per_blk_shift, bbt_start_block, vernier;
	int phys_erase_shift, i;

	phys_erase_shift = fls(mtd->erasesize) - 1;
	pages_per_blk_shift = (chip->phys_erase_shift - chip->page_shift);

	/*bootloader occupy 1024 pages*/
	bbt_start_block = BOOT_TOTAL_PAGES >> pages_per_blk_shift;
	bbt_start_block += NAND_GAP_BLOCK_NUM; /*gap occupy 4 blocks*/

	vernier = bbt_start_block;
	aml_chip->rsv_data_buf = kzalloc(mtd->writesize, GFP_KERNEL);
	if (aml_chip->rsv_data_buf == NULL)
		return -ENOMEM;

	aml_chip->freeNodeBitmask = 0;
	for (i = 0; i < RESERVED_BLOCK_NUM; i++) {
		aml_chip->free_node[i] =
			kzalloc(sizeof(struct free_node_t), GFP_KERNEL);
		aml_chip->free_node[i]->index = i;
	}

	/*bbt info init*/
	aml_chip->aml_nandbbt_info =
		kzalloc(sizeof(struct aml_nandrsv_info_t), GFP_KERNEL);
	if (aml_chip->aml_nandbbt_info == NULL)
		return -ENOMEM;

	aml_chip->aml_nandbbt_info->mtd = mtd;
	aml_chip->aml_nandbbt_info->valid_node =
		kzalloc(sizeof(struct valid_node_t), GFP_KERNEL);
	if (aml_chip->aml_nandbbt_info->valid_node == NULL)
		return -ENOMEM;

	aml_chip->aml_nandbbt_info->valid_node->phy_blk_addr = -1;
	aml_chip->aml_nandbbt_info->start_block = vernier;
	aml_chip->aml_nandbbt_info->end_block =
		vernier + NAND_BBT_BLOCK_NUM;
	vernier += NAND_BBT_BLOCK_NUM;
	aml_chip->aml_nandbbt_info->size = mtd->size >> phys_erase_shift;
	memcpy(aml_chip->aml_nandbbt_info->name, BBT_NAND_MAGIC, 4);

	/*block status*/
	aml_chip->block_status =
		kzalloc((mtd->size >> phys_erase_shift), GFP_KERNEL);
	if (aml_chip->block_status == NULL) {
		pr_info("no memory for flash block status\n");
		return -ENOMEM;
	}
	memset(aml_chip->block_status, 0, (mtd->size >> phys_erase_shift));
#ifndef CONFIG_MTD_ENV_IN_NAND
	/*env info init*/
	aml_chip->aml_nandenv_info =
		kzalloc(sizeof(struct aml_nandrsv_info_t), GFP_KERNEL);
	if (aml_chip->aml_nandenv_info == NULL)
		return -ENOMEM;

	aml_chip->aml_nandenv_info->mtd = mtd;
	aml_chip->aml_nandenv_info->valid_node =
		kzalloc(sizeof(struct valid_node_t), GFP_KERNEL);
	if (aml_chip->aml_nandenv_info->valid_node == NULL)
		return -ENOMEM;

	aml_chip->aml_nandenv_info->valid_node->phy_blk_addr = -1;
	aml_chip->aml_nandenv_info->start_block = vernier;
	aml_chip->aml_nandenv_info->end_block =
		vernier + NAND_ENV_BLOCK_NUM;
	vernier += NAND_ENV_BLOCK_NUM;
	aml_chip->aml_nandenv_info->size = CONFIG_ENV_SIZE;
	memcpy(aml_chip->aml_nandenv_info->name, ENV_NAND_MAGIC, 4);
#endif
	aml_chip->aml_nandkey_info =
		kzalloc(sizeof(struct aml_nandrsv_info_t), GFP_KERNEL);
	if (aml_chip->aml_nandkey_info == NULL)
		return -ENOMEM;

	/*key init*/
	aml_chip->aml_nandkey_info->mtd = mtd;
	aml_chip->aml_nandkey_info->valid_node =
		kzalloc(sizeof(struct valid_node_t), GFP_KERNEL);
	if (aml_chip->aml_nandkey_info->valid_node == NULL)
		return -ENOMEM;

	aml_chip->aml_nandkey_info->valid_node->phy_blk_addr = -1;
	aml_chip->aml_nandkey_info->start_block = vernier;
	aml_chip->aml_nandkey_info->end_block =
		vernier + NAND_KEY_BLOCK_NUM;
	vernier += NAND_KEY_BLOCK_NUM;
	aml_chip->aml_nandkey_info->size = aml_chip->keysize;
	memcpy(aml_chip->aml_nandkey_info->name, KEY_NAND_MAGIC, 4);

	aml_chip->aml_nanddtb_info =
		kzalloc(sizeof(struct aml_nandrsv_info_t), GFP_KERNEL);
	if (aml_chip->aml_nanddtb_info == NULL)
		return -ENOMEM;

	/*key init*/
	aml_chip->aml_nanddtb_info->mtd = mtd;
	aml_chip->aml_nanddtb_info->valid_node =
		kzalloc(sizeof(struct valid_node_t), GFP_KERNEL);
	if (aml_chip->aml_nanddtb_info->valid_node == NULL)
		return -ENOMEM;

	aml_chip->aml_nanddtb_info->valid_node->phy_blk_addr = -1;
	aml_chip->aml_nanddtb_info->start_block = vernier;
	aml_chip->aml_nanddtb_info->end_block =
		vernier + NAND_DTB_BLOCK_NUM;
	vernier += NAND_DTB_BLOCK_NUM;
	aml_chip->aml_nanddtb_info->size = aml_chip->dtbsize;
	memcpy(aml_chip->aml_nanddtb_info->name, DTB_NAND_MAGIC, 4);

	if ((vernier - (BOOT_TOTAL_PAGES >> pages_per_blk_shift)) >
	    RESERVED_BLOCK_NUM) {
		pr_info("ERROR: total blk number is over the limit\n");
		return -ENOMEM;
		}
		pr_info("bbt_start=%d\n",
				aml_chip->aml_nandbbt_info->start_block);
#ifndef CONFIG_MTD_ENV_IN_NAND
		pr_info("env_start=%d\n",
				aml_chip->aml_nandenv_info->start_block);
#endif
		pr_info("key_start=%d\n",
				aml_chip->aml_nandkey_info->start_block);
		pr_info("dtb_start=%d\n",
				aml_chip->aml_nanddtb_info->start_block);

	return 0;
}

int aml_nand_free_rsv_info(struct mtd_info *mtd,
	struct aml_nandrsv_info_t *nandrsv_info)
{
	struct free_node_t *tmp_node, *next_node = NULL;
	int error = 0;
	loff_t addr = 0;
	struct erase_info erase_info;

	pr_info("free %s:\n", nandrsv_info->name);

	if (nandrsv_info->valid) {
		addr = nandrsv_info->valid_node->phy_blk_addr;
		addr *= mtd->erasesize;
		memset(&erase_info,
			0, sizeof(struct erase_info));
		erase_info.mtd = mtd;
		erase_info.addr = addr;
		erase_info.len = mtd->erasesize;
		_aml_rsv_disprotect();
		error = mtd->_erase(mtd, &erase_info);
		_aml_rsv_protect();
		pr_info("erasing valid info block: %llx\n", addr);
		nandrsv_info->valid_node->phy_blk_addr = -1;
		nandrsv_info->valid_node->ec = -1;
		nandrsv_info->valid_node->phy_page_addr = 0;
		nandrsv_info->valid_node->timestamp = 0;
		nandrsv_info->valid_node->status = 0;
		nandrsv_info->valid = 0;
	}
	tmp_node = nandrsv_info->free_node;
	while (tmp_node != NULL) {
		next_node = tmp_node->next;
		release_free_node(mtd, tmp_node);
		tmp_node = next_node;
	}
	nandrsv_info->free_node = NULL;

	return error;
}

int aml_nand_scan_rsv_info(struct mtd_info *mtd,
	struct aml_nandrsv_info_t *nandrsv_info)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	struct nand_chip *chip = &aml_chip->chip;
	struct mtd_oob_ops aml_oob_ops;
	struct oobinfo_t *oobinfo;
	struct free_node_t *free_node, *tmp_node = NULL;
	unsigned char oob_buf[sizeof(struct oobinfo_t)];
	uint64_t offset;
	unsigned char *data_buf, good_addr[256] = {0};
	int start_blk, max_scan_blk, i, k, scan_status = 0, env_status = 0;
	int phys_erase_shift, pages_per_blk, page_num;
	int error = 0, ret = 0;
	uint32_t  remainder;

	data_buf = aml_chip->rsv_data_buf;
	oobinfo = (struct oobinfo_t *)oob_buf;

RE_RSV_INFO_EXT:
	memset(good_addr, 0, 256);
	max_scan_blk = nandrsv_info->end_block;
	start_blk = nandrsv_info->start_block;
	pr_info("%s: info size=0x%x max_scan_blk=%d, start_blk=%d\n",
		nandrsv_info->name, nandrsv_info->size,
		max_scan_blk, start_blk);

	do {
		offset = mtd->erasesize;
		offset *= start_blk;
		scan_status = 0;
RE_RSV_INFO:
	aml_oob_ops.mode = MTD_OPS_AUTO_OOB;
	aml_oob_ops.len = mtd->writesize;
	aml_oob_ops.ooblen = sizeof(struct oobinfo_t);
	aml_oob_ops.ooboffs = 0;/*mtd->ecclayout->oobfree[0].offset;*/
	aml_oob_ops.datbuf = data_buf;
	aml_oob_ops.oobbuf = oob_buf;

	memset((unsigned char *)aml_oob_ops.datbuf,
		0x0, mtd->writesize);
	memset((unsigned char *)aml_oob_ops.oobbuf,
		0x0, aml_oob_ops.ooblen);

	error = mtd->_read_oob(mtd, offset, &aml_oob_ops);
	if ((error != 0) && (error != -EUCLEAN)) {
		pr_info("blk check good but read failed: %llx, %d\n",
			(uint64_t)offset, error);
		offset += nandrsv_info->size;
		div_u64_rem(offset, mtd->erasesize, &remainder);
		if ((scan_status++ > 6) || (!remainder)) {
			pr_info("ECC error, scan ONE block exit\n");
			scan_status = 0;
			continue;
		}
		goto RE_RSV_INFO;
	}

	nandrsv_info->init = 1;
	nandrsv_info->valid_node->status = 0;
	if (!memcmp(oobinfo->name, nandrsv_info->name, 4)) {
		nandrsv_info->valid = 1;
		if (nandrsv_info->valid_node->phy_blk_addr >= 0) {
			free_node = get_free_node(mtd);
			if (free_node == NULL)
				return -ENOMEM;

			free_node->dirty_flag = 1;
		if (oobinfo->timestamp > nandrsv_info->valid_node->timestamp) {
			free_node->phy_blk_addr =
				nandrsv_info->valid_node->phy_blk_addr;
			free_node->ec =
			nandrsv_info->valid_node->ec;
			nandrsv_info->valid_node->phy_blk_addr =
				start_blk;
			nandrsv_info->valid_node->phy_page_addr = 0;
			nandrsv_info->valid_node->ec = oobinfo->ec;
			nandrsv_info->valid_node->timestamp =
				oobinfo->timestamp;
		} else {
			free_node->phy_blk_addr = start_blk;
			free_node->ec = oobinfo->ec;
		}
			if (nandrsv_info->free_node == NULL)
				nandrsv_info->free_node = free_node;
			else {
				tmp_node = nandrsv_info->free_node;
				while (tmp_node->next != NULL)
					tmp_node = tmp_node->next;

				tmp_node->next = free_node;
			}
		} else {
			nandrsv_info->valid_node->phy_blk_addr =
				start_blk;
			nandrsv_info->valid_node->phy_page_addr = 0;
			nandrsv_info->valid_node->ec = oobinfo->ec;
			nandrsv_info->valid_node->timestamp =
				oobinfo->timestamp;
		}
	} else {
		free_node = get_free_node(mtd);
		if (free_node == NULL)
			return -ENOMEM;
		free_node->phy_blk_addr = start_blk;
		free_node->ec = oobinfo->ec;
		if (nandrsv_info->free_node == NULL)
			nandrsv_info->free_node = free_node;
		else {
			tmp_node = nandrsv_info->free_node;
			while (tmp_node->next != NULL)
				tmp_node = tmp_node->next;

			tmp_node->next = free_node;
		}
	}

	} while ((++start_blk) < max_scan_blk);

	pr_info("%s : phy_blk_addr=%d, ec=%d, phy_page_addr=%d, timestamp=%d\n",
		nandrsv_info->name,
		nandrsv_info->valid_node->phy_blk_addr,
		nandrsv_info->valid_node->ec,
		nandrsv_info->valid_node->phy_page_addr,
		nandrsv_info->valid_node->timestamp);
	pr_info("%s free list:\n", nandrsv_info->name);
	tmp_node = nandrsv_info->free_node;
	while (tmp_node != NULL) {
		pr_info("blockN=%d, ec=%d, dirty_flag=%d\n",
			tmp_node->phy_blk_addr,
			tmp_node->ec,
			tmp_node->dirty_flag);
		tmp_node = tmp_node->next;
	}

	/*second stage*/
	phys_erase_shift = fls(mtd->erasesize) - 1;
	pages_per_blk = (1 << (phys_erase_shift - chip->page_shift));
	page_num = nandrsv_info->size / mtd->writesize;
	if (page_num == 0)
		page_num++;

	pr_info("%s %d: page_num=%d\n", __func__, __LINE__, page_num);

	if (nandrsv_info->valid == 1) {
		pr_info("%s %d\n", __func__, __LINE__);
		aml_oob_ops.mode = MTD_OPS_AUTO_OOB;
		aml_oob_ops.len = mtd->writesize;
		aml_oob_ops.ooblen = sizeof(struct oobinfo_t);
		aml_oob_ops.ooboffs = 0;/*mtd->ecclayout->oobfree[0].offset;*/
		aml_oob_ops.datbuf = data_buf;
		aml_oob_ops.oobbuf = oob_buf;

	for (i = 0; i < pages_per_blk; i++) {
		memset((unsigned char *)aml_oob_ops.datbuf,
			0x0, mtd->writesize);
		memset((unsigned char *)aml_oob_ops.oobbuf,
			0x0, aml_oob_ops.ooblen);

		offset = nandrsv_info->valid_node->phy_blk_addr;
		offset *= mtd->erasesize;
		offset += i * mtd->writesize;
		error = mtd->_read_oob(mtd, offset, &aml_oob_ops);
		if ((error != 0) && (error != -EUCLEAN)) {
			pr_info("blk good but read failed:%llx,%d\n",
				(uint64_t)offset, error);
			nandrsv_info->valid_node->status |= ECC_ABNORMAL_FLAG;
			ret = -1;
			continue;
		}

		if (!memcmp(oobinfo->name, nandrsv_info->name, 4)) {
			good_addr[i] = 1;
			nandrsv_info->valid_node->phy_page_addr = i;
		} else
			break;
	}
	}

	if ((mtd->writesize < nandrsv_info->size)
		&& (nandrsv_info->valid == 1)) {
		i = nandrsv_info->valid_node->phy_page_addr;
		if (((i + 1) % page_num) != 0) {
			ret = -1;
			nandrsv_info->valid_node->status |= POWER_ABNORMAL_FLAG;
			pr_info("find %s incomplete\n", nandrsv_info->name);
		}
		if (ret == -1) {
			for (i = 0; i < (pages_per_blk / page_num); i++) {
				env_status = 0;
				for (k = 0; k < page_num; k++) {
					if (!good_addr[k + i * page_num]) {
						env_status = 1;
						break;
					}
				}
				if (!env_status) {
					pr_info("find %d page ok\n",
						i*page_num);
				nandrsv_info->valid_node->phy_page_addr =
						k + i * page_num - 1;
					ret = 0;
				}
			}
		}
		if (ret == -1) {
			nandrsv_info->valid_node->status = 0;
			aml_nand_free_rsv_info(mtd, nandrsv_info);
			goto RE_RSV_INFO_EXT;
		}
		i = (nandrsv_info->size + mtd->writesize - 1) / mtd->writesize;
		nandrsv_info->valid_node->phy_page_addr -= (i - 1);
	}

	if (nandrsv_info->valid != 1)
		ret = -1;
	offset = nandrsv_info->valid_node->phy_blk_addr;
	offset *= mtd->erasesize;
	offset += nandrsv_info->valid_node->phy_page_addr * mtd->writesize;
	pr_info("%s valid addr: %llx\n", nandrsv_info->name, (uint64_t)offset);
	return ret;
}

int aml_nand_env_check(struct mtd_info *mtd)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	int ret = 0;

	/* pr_info("%s %d\n", __func__, __LINE__); */

	ret = aml_nand_scan_rsv_info(mtd, aml_chip->aml_nandenv_info);
	if ((ret != 0) && ((ret != (-1))))
		pr_info("%s %d\n", __func__, __LINE__);

	if (aml_chip->aml_nandenv_info->valid == 0)
		pr_info("%s %d NO env exist\n", __func__, __LINE__);

	return ret;
}

int aml_nand_key_check(struct mtd_info *mtd)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	int ret = 0;

	ret = aml_nand_scan_rsv_info(mtd, aml_chip->aml_nandkey_info);
	if ((ret != 0) && ((ret != (-1))))
		pr_info("%s %d\n", __func__, __LINE__);

	if (aml_chip->aml_nandkey_info->valid == 0)
		pr_info("%s %d NO key exist\n", __func__, __LINE__);

	return ret;
}

int aml_nand_dtb_check(struct mtd_info *mtd)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	int ret = 0;

	ret = aml_nand_scan_rsv_info(mtd, aml_chip->aml_nanddtb_info);
	if ((ret != 0) && ((ret != (-1))))
		pr_info("%s %d\n", __func__, __LINE__);

	if (aml_chip->aml_nanddtb_info->valid == 0)
		pr_info("%s %d NO dtb exist\n", __func__, __LINE__);

	return ret;
}

int aml_nand_bbt_check(struct mtd_info *mtd)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	int phys_erase_shift;
	int ret = 0;
	int8_t *buf = NULL;

	phys_erase_shift = fls(mtd->erasesize) - 1;

	ret = aml_nand_scan_rsv_info(mtd, aml_chip->aml_nandbbt_info);
	if ((ret != 0) && ((ret != (-1)))) {
		pr_info("%s %d\n", __func__, __LINE__);
		return ret;
	}

	ret = 0;
	buf = aml_chip->block_status;
	if (aml_chip->aml_nandbbt_info->valid == 1) {
		/*read bbt*/
		pr_info("%s %d bbt is valid, reading.\n",
			__func__, __LINE__);
		aml_nand_read_rsv_info(mtd,
			aml_chip->aml_nandbbt_info, 0, (u_char *)buf);
		goto exit_error1;
	} else {
		pr_info("%s %d bbt is invalid, scanning.\n",
			__func__, __LINE__);
		/*no bbt haven't been found, abnormal or clean nand! rebuild*/
		aml_chip->nand_bbt_info =
			kzalloc(sizeof(struct aml_nand_bbt_info), GFP_KERNEL);
		if (!aml_chip->nand_bbt_info) {
			ret = -ENOMEM;
			goto exit_error;
		}
		memset(aml_chip->block_status,
			0, (mtd->size >> phys_erase_shift));
		aml_nand_scan_shipped_bbt(mtd);
		aml_nand_save_bbt(mtd, (u_char *)buf);
	}
exit_error:
	kfree(aml_chip->nand_bbt_info);
exit_error1:
	return ret;
}


int aml_nand_scan_bbt(struct mtd_info *mtd)
{
	return 0;
}

