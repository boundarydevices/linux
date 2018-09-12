/*
 * drivers/amlogic/mtd/boot.c
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

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/mtd/partitions.h>
#ifndef AML_NAND_UBOOT
#include<linux/cdev.h>
#include <linux/device.h>
#endif

#include "aml_mtd.h"
static DEFINE_MUTEX(boot_mutex);

#if 0
#ifndef AML_NAND_UBOOT
struct device *devp;
struct cdev uboot_cdev;
static dev_t uboot_devno;
static struct aml_nand_chip *aml_chip_uboot;
#endif
#endif
/*provide a policy that calculate the backup number of bootloader*/
static int __attribute__((unused)) get_boot_num(
	struct mtd_info *mtd, size_t rwsize)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	size_t bad_blk_len_low = 0, bad_blk_len_up = 0, skip;
	size_t aviable_space;
	loff_t offset = 0;
	int ret = 1, error = 0; /*initial for only one copy*/

	if (!rwsize) { /*not need to policy call, only one */
		ret = 1;
		return ret;
	}

	while (offset < mtd->size) {
		error = mtd->_block_isbad(mtd, offset);
		if (error != 0) {
			if (offset < mtd->size / 2)
				bad_blk_len_low += mtd->erasesize;
			else if (offset > mtd->size / 2)
				bad_blk_len_up += mtd->erasesize;
			else
				bad_blk_len_up = offset;
		}
		offset += mtd->erasesize;
	}

	pr_info("rwsize:0x%zx skip_low:0x%zx skip_up:0x%zx\n",
		rwsize, bad_blk_len_low, bad_blk_len_up);

	skip = bad_blk_len_low + bad_blk_len_up;
	aviable_space = mtd->size - skip - 2 * mtd->writesize;

	if (rwsize*2 <= aviable_space) {
		ret = 1;
		if (rwsize + mtd->writesize + bad_blk_len_low > mtd->size / 2)
			return 1; /*1st must be write*/
		if (rwsize + mtd->writesize + bad_blk_len_up <= mtd->size / 2)
			ret++;
	} else /*needn't consider bad block length, unlikly so many bad blocks*/
		ret = 1;

	aml_chip->boot_copy_num = ret;
	pr_info("copy number:%d\n", ret);

	return ret;
}
/*
 * set nand info into page0_buf for romboot.
 */
void __attribute__((unused)) nand_info_page_prepare(
	struct aml_nand_chip *aml_chip,
	u8 *page0_buf)
{
	struct nand_chip *chip = &aml_chip->chip;
	struct mtd_info *mtd = aml_chip->mtd;
	struct aml_nand_chip *aml_chip_normal;
	int nand_read_info;
	u32 configure_data;
	struct _nand_page0 *p_nand_page0 = NULL;
	struct _ext_info *p_ext_info = NULL;
	struct _fip_info *p_fip_info = NULL;
	struct nand_setup *p_nand_setup = NULL;
	int each_boot_pages, boot_num, bbt_pages;
	uint32_t pages_per_blk_shift, bbt_size;

	pages_per_blk_shift = (chip->phys_erase_shift - chip->page_shift);
	aml_chip_normal = mtd_to_nand_chip(nand_info[1]);
	bbt_size = aml_chip_normal->aml_nandbbt_info->size;
	if (aml_chip->bl_mode) {
		boot_num = aml_chip->fip_copies;
		each_boot_pages = aml_chip->fip_size / mtd->writesize;
	} else {
		boot_num = (!aml_chip->boot_copy_num) ?
			1 : aml_chip->boot_copy_num;
		each_boot_pages = BOOT_TOTAL_PAGES/boot_num;
	}

	p_nand_page0 = (struct _nand_page0 *) page0_buf;
	p_nand_setup = &p_nand_page0->nand_setup;
	p_ext_info = &p_nand_page0->ext_info;
	p_fip_info = &p_nand_page0->fip_info;

	configure_data = NFC_CMD_N2M(aml_chip->ran_mode,
			aml_chip->bch_mode, 0, (chip->ecc.size >> 3),
			chip->ecc.steps);
	/* en_slc mode will not be used on slc */
	/* en_slc = 0; */

	memset(p_nand_page0, 0x0, sizeof(struct _nand_page0));
	/* info_cfg->ext = (configure_data | (1<<23) |(1<<22) | (2<<20)); */
	/*
	 *p_nand_setup->cfg.d32 =
	 *(configure_data|(1<<23) | (1<<22) | (2<<20) | (1<<19));
	 **/
	/* randomizer mode depends on chip's cofig */
	p_nand_setup->cfg.d32 = (configure_data|(1<<23) | (1<<22) | (2<<20));
	pr_info("cfg.d32 0x%x\n", p_nand_setup->cfg.d32);
	/* need finish here for romboot retry */
	p_nand_setup->id = 0;
	p_nand_setup->max = 0;

	memset(p_nand_page0->page_list,
		0,
		NAND_PAGELIST_CNT);
	/* chip_num occupy the lowest 2 bit */
	nand_read_info = controller->chip_num;

	p_ext_info->read_info = nand_read_info;
	p_ext_info->page_per_blk = aml_chip->block_size / aml_chip->page_size;
	/* fixme, only ce0 is enabled! */
	p_ext_info->ce_mask = 0x01;
	/* xlc is not in using for now */
	p_ext_info->xlc = 1;
	p_ext_info->boot_num = boot_num;
	p_ext_info->each_boot_pages = each_boot_pages;
	bbt_pages =
	(bbt_size + mtd->writesize - 1) / mtd->writesize;
	p_ext_info->bbt_occupy_pages = bbt_pages;
	p_ext_info->bbt_start_block =
		(BOOT_TOTAL_PAGES >> pages_per_blk_shift) + NAND_GAP_BLOCK_NUM;
	/* fill descrete infos */
	if (aml_chip->bl_mode) {
		p_fip_info->version = 1;
		p_fip_info->mode = NAND_FIPMODE_DISCRETE;
		p_fip_info->fip_start =
			1024 + RESERVED_BLOCK_NUM * p_ext_info->page_per_blk;
		pr_info("ver %d, mode %d, fip 0x%x\n",
			p_fip_info->version, p_fip_info->mode,
			p_fip_info->fip_start);
	}
	/* pr_info("new_type = 0x%x\n", p_ext_info->new_type); */
	pr_info("page_per_blk = 0x%x, bbt_pages 0x%x\n",
		p_ext_info->page_per_blk, bbt_pages);
	pr_info("boot_num = %d each_boot_pages = %d\n", boot_num,
		each_boot_pages);
}

/* mtd support interface:
 * function:int (*_erase) (struct mtd_info *mtd, struct erase_info *instr);
 */
int m3_nand_boot_erase_cmd(struct mtd_info *mtd, int page)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	struct nand_chip *chip = mtd->priv;
	loff_t ofs;

	ofs = ((loff_t)page << chip->page_shift);

	if (chip->block_bad(mtd, ofs))
		return -1;

	aml_chip->aml_nand_select_chip(aml_chip, 0);
	aml_chip->aml_nand_command(aml_chip,
		NAND_CMD_ERASE1, -1, page, 0);
	aml_chip->aml_nand_command(aml_chip,
		NAND_CMD_ERASE2, -1, -1, 0);
	chip->waitfunc(mtd, chip);

	return 0;
}

/* mtd support interface:
 * chip->ecc.read_page
 * function:int (*read_page)(struct mtd_info *mtd, struct nand_chip *chip,
 *		uint8_t *buf, int oob_required, int page);
 */
int m3_nand_boot_read_page_hwecc(struct mtd_info *mtd,
	struct nand_chip *chip, uint8_t *buf, int oob_required, int page)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	uint8_t *oob_buf = chip->oob_poi;
	uint32_t nand_page_size = chip->ecc.steps * chip->ecc.size;
	uint32_t pages_per_blk_shift =
		chip->phys_erase_shift - chip->page_shift;
	int user_byte_num = (chip->ecc.steps * aml_chip->user_byte_mode);
	int bch_mode = aml_chip->bch_mode, ran_mode = 0;
	int error = 0, i = 0, stat = 0;
	int ecc_size, configure_data_w, pages_per_blk_w, configure_data;
	int pages_per_blk, read_page;
	int en_slc = 0;
	/* using info page structure */
	struct _nand_page0 *p_nand_page0 = NULL;
	struct _ext_info *p_ext_info = NULL;
	struct nand_setup *p_nand_setup = NULL;
	int each_boot_pages, boot_num;
	uint64_t ofs, tmp;
	uint32_t remainder;

	u8 type = aml_chip->new_nand_info.type;

	if (aml_chip->support_new_nand == 1)
		en_slc = ((type < 10) && type) ? 1:0;
	if (aml_chip->bl_mode)
		boot_num =
			(get_cpu_type() < MESON_CPU_MAJOR_ID_AXG) ? 4 : 8;
	else
		boot_num = (!aml_chip->boot_copy_num) ?
			1 : aml_chip->boot_copy_num;
	each_boot_pages = BOOT_TOTAL_PAGES / boot_num;
	if (page >= (each_boot_pages * boot_num)) {
		memset(buf, 0, (1 << chip->page_shift));
		pr_info("nand boot read out of uboot failed, page:%d\n", page);
		goto exit;
	}
	/* nand page info */
	if ((page % each_boot_pages) == 0) {
		if (aml_chip->bch_mode == NAND_ECC_BCH_SHORT)
			configure_data_w =
				NFC_CMD_N2M(aml_chip->ran_mode,
		NAND_ECC_BCH60_1K, 1, (chip->ecc.size >> 3), chip->ecc.steps);
		else
			configure_data_w =
				NFC_CMD_N2M(aml_chip->ran_mode,
		aml_chip->bch_mode, 0, (chip->ecc.size >> 3), chip->ecc.steps);

		ecc_size = chip->ecc.size;  /* backup ecc size */

		if (aml_chip->bch_mode != NAND_ECC_BCH_SHORT) {
			nand_page_size =
				(mtd->writesize / 512) * NAND_ECC_UNIT_SHORT;
			bch_mode = NAND_ECC_BCH_SHORT;
			chip->ecc.size = NAND_ECC_UNIT_SHORT;
		} else
			bch_mode = aml_chip->bch_mode;

		chip->cmdfunc(mtd, NAND_CMD_READ0, 0x00, page);
		memset(buf, 0xff, (1 << chip->page_shift));
		/* read back page0 and check it */
		if (aml_chip->valid_chip[0]) {
			if (!aml_chip->aml_nand_wait_devready(aml_chip, i)) {
				pr_info("don't found selected chip:%d ready\n",
					i);
				error = -EBUSY;
				goto exit;
			}
			if (aml_chip->ops_mode & AML_CHIP_NONE_RB)
				chip->cmd_ctrl(mtd, NAND_CMD_READ0 & 0xff,
					NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);
			if (en_slc == 0) {
				ran_mode = aml_chip->ran_mode;
				aml_chip->ran_mode = 1;
			}
			error = aml_chip->aml_nand_dma_read(aml_chip,
				buf, nand_page_size, bch_mode);

			if (error) {
				pr_info(" page0 aml_nand_dma_read failed\n");
				goto exit;
			}

			aml_chip->aml_nand_get_user_byte(aml_chip,
				oob_buf, user_byte_num);
			stat = aml_chip->aml_nand_hwecc_correct(aml_chip,
				buf, nand_page_size, oob_buf);
			if (stat < 0) {
				if(aml_chip->ran_mode
					&& (aml_chip->zero_cnt <  aml_chip->ecc_max)) {
					memset(buf, 0xff, nand_page_size);
					memset(oob_buf, 0xff, user_byte_num);
				} else {
					mtd->ecc_stats.failed++;
					pr_info("page0 read ecc fail\n");
				}
			} else
				mtd->ecc_stats.corrected += stat;
			if (en_slc == 0)
				aml_chip->ran_mode = ran_mode;
		} else {
			pr_info("nand boot page 0 no valid chip failed\n");
			error = -ENODEV;
			goto exit;
			/* goto exit; */
		}

		/* check page 0 info here */
		p_nand_page0 = (struct _nand_page0 *) buf;
		p_nand_setup = &p_nand_page0->nand_setup;
		p_ext_info = &p_nand_page0->ext_info;

		configure_data = p_nand_setup->cfg.b.cmd;
		pages_per_blk = p_ext_info->page_per_blk;
		pages_per_blk_w =
			(1 << (chip->phys_erase_shift - chip->page_shift));

		if ((pages_per_blk_w != pages_per_blk)
			|| (configure_data != configure_data_w)) {
			pr_info("page%d fail ", page);
			pr_info("configure:0x%x-0x%x pages_per_blk:0x%x-0x%x\n",
				configure_data_w, configure_data,
				pages_per_blk_w, pages_per_blk);
		}

		bch_mode = aml_chip->bch_mode;
		chip->ecc.size = ecc_size;
		nand_page_size = chip->ecc.steps * chip->ecc.size;
	}

	read_page = page;
	read_page++;
READ_BAD_BLOCK:
	ofs = ((uint64_t)read_page << chip->page_shift);
	tmp = ofs;
	div_u64_rem(tmp, mtd->erasesize, &remainder);
	if (!remainder) {
		if (chip->block_bad(mtd, ofs)) {
			read_page +=
			1 << (chip->phys_erase_shift-chip->page_shift);
			goto READ_BAD_BLOCK;
		}
	}

	if (aml_chip->support_new_nand == 1) {
		if (en_slc) {
			pages_per_blk =
			(1 << (chip->phys_erase_shift - chip->page_shift));
			read_page = page % pages_per_blk;
			if (type == HYNIX_1YNM_8GB)
				read_page =
				pagelist_1ynm_hynix256_mtd[read_page + 1] +
				(page / each_boot_pages) * each_boot_pages;
			else
				read_page =
					pagelist_hynix256[read_page + 1] +
					(page / each_boot_pages) *
					each_boot_pages;
		}
	}

	chip->cmdfunc(mtd, NAND_CMD_READ0, 0x00, read_page);

	memset(buf, 0xff, (1 << chip->page_shift));
	if (aml_chip->valid_chip[0]) {
		if (!aml_chip->aml_nand_wait_devready(aml_chip, 0)) {
			pr_info("don't found selected chip0 ready, page: %d\n",
				page);
			error = -EBUSY;
			goto exit;
		}
		if (aml_chip->ops_mode & AML_CHIP_NONE_RB)
			chip->cmd_ctrl(mtd, NAND_CMD_READ0 & 0xff,
				NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);

		error = aml_chip->aml_nand_dma_read(aml_chip,
			buf, nand_page_size, bch_mode);
		if (error) {
			error = -ENODEV;
			pr_info("aml_nand_dma_read failed: page:%d\n", page);
			goto exit;
		}

		aml_chip->aml_nand_get_user_byte(aml_chip,
			oob_buf, user_byte_num);
		stat = aml_chip->aml_nand_hwecc_correct(aml_chip,
			buf, nand_page_size, oob_buf);
		if (stat < 0) {
			error = -ENODEV;
			mtd->ecc_stats.failed++;
			pr_info("read data ecc failed at page%d blk%d chip%d\n",
				page, (page >> pages_per_blk_shift), i);
		} else
			mtd->ecc_stats.corrected += stat;
	} else
		error = -ENODEV;

exit:
	return error;
}

/*
 * read oob only.
 */
int m3_nand_boot_read_oob(struct mtd_info *mtd,
	struct nand_chip *chip, int page)
{
	int ret = 0;
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	/* send cmd 00-addr-30 */
	aml_chip->aml_nand_command(aml_chip, NAND_CMD_READ0,
				0, page, 0);
	ret = m3_nand_boot_read_page_hwecc(mtd,
		chip, aml_chip->aml_nand_data_buf, 0, page);

	return ret;
}

/* mtd support interface:
 * chip->ecc.write_page
 * function:int (*write_page)(struct mtd_info *mtd, struct nand_chip *chip,
 *		uint8_t *buf, int oob_required, int page);
 */
int m3_nand_boot_write_page_hwecc(struct mtd_info *mtd,
	struct nand_chip *chip, const uint8_t *buf, int oob_required, int page)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	uint8_t *oob_buf = chip->oob_poi;
	uint32_t nand_page_size = chip->ecc.steps * chip->ecc.size;
	int user_byte_num = (chip->ecc.steps * aml_chip->user_byte_mode);
	int error = 0, i = 0, bch_mode, ecc_size;
	int each_boot_pages, boot_num;

	if (aml_chip->bl_mode)
		boot_num =
			(get_cpu_type() < MESON_CPU_MAJOR_ID_AXG) ? 4 : 8;
	else
		boot_num = (!aml_chip->boot_copy_num) ?
			1 : aml_chip->boot_copy_num;
	each_boot_pages = BOOT_TOTAL_PAGES / boot_num;
	ecc_size = chip->ecc.size;
	if (((aml_chip->page_addr % each_boot_pages) == 0)
		&& (aml_chip->bch_mode != NAND_ECC_BCH_SHORT)) {
		nand_page_size = (mtd->writesize / 512) * NAND_ECC_UNIT_SHORT;
		bch_mode = NAND_ECC_BCH_SHORT;
		chip->ecc.size = NAND_ECC_UNIT_SHORT;
	} else
		bch_mode = aml_chip->bch_mode;
	/* setting magic for romboot checks. */
	for (i = 0; i < mtd->oobavail; i += 2) {
		oob_buf[i] = 0x55;
		oob_buf[i+1] = 0xaa;
	}

	i = 0;
	if (aml_chip->valid_chip[i]) {
		aml_chip->aml_nand_select_chip(aml_chip, i);
		aml_chip->aml_nand_set_user_byte(aml_chip,
			oob_buf, user_byte_num);
		error = aml_chip->aml_nand_dma_write(aml_chip,
			(unsigned char *)buf, nand_page_size, bch_mode);
		if (error)
			goto exit;
		aml_chip->aml_nand_command(aml_chip,
			NAND_CMD_PAGEPROG, -1, -1, i);
	} else {
		error = -ENODEV;
		goto exit;
	}
exit:
	if (((aml_chip->page_addr % each_boot_pages) == 0)
			&& (aml_chip->bch_mode != NAND_ECC_BCH_SHORT))
		chip->ecc.size = ecc_size;
	return error;
}

/* mtd support interface:
 * chip->write_page
 * function:	int (*write_page)(struct mtd_info *mtd, struct nand_chip *chip,
 *			uint32_t offset, int data_len, const uint8_t *buf,
 *			int oob_required, int page, int cached, int raw);
 */

int m3_nand_boot_write_page(struct mtd_info *mtd, struct nand_chip *chip,
	uint32_t offset, int data_len, const uint8_t *buf,
	int oob_required, int page, int cached, int raw)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	int status, write_page, ran_mode = 0;
	struct new_tech_nand_t *new_nand_info;
	struct aml_nand_slc_program *slc_program_info;
	int new_nand_type = 0;
	int pages_per_blk;

	unsigned char *fill_buf = NULL;
	uint32_t priv_slc_page;
	int en_slc = 0, each_boot_pages, boot_num;
	u8 type = aml_chip->new_nand_info.type;
	uint64_t ofs, tmp;
	uint32_t remainder;

	new_nand_info = &aml_chip->new_nand_info;
	slc_program_info = &new_nand_info->slc_program_info;
	/* check cpuid, */
	if (aml_chip->bl_mode)
		boot_num =
			(get_cpu_type() < MESON_CPU_MAJOR_ID_AXG) ? 4 : 8;
	else
		boot_num = (!aml_chip->boot_copy_num) ?
			1 : aml_chip->boot_copy_num;

	each_boot_pages = BOOT_TOTAL_PAGES / boot_num;
	if (aml_chip->support_new_nand == 1) {
		new_nand_type = type;
		en_slc = ((type < 10) && type) ? 1 : 0;
		if (new_nand_type == HYNIX_1YNM_8GB) {
			fill_buf = kzalloc(mtd->writesize, GFP_KERNEL);
			if (fill_buf == NULL) {
				pr_info("malloc fill buf fail\n");
				return -ENOMEM;
			}
			memset(fill_buf, 0xff, mtd->writesize);
		}

		if (en_slc) {
			if (page >= (each_boot_pages/2 - 1)) {
				kfree(fill_buf);
				return 0;
			}
		if (slc_program_info->enter_enslc_mode)
			slc_program_info->enter_enslc_mode(mtd);
		} else {
			if (page >= (each_boot_pages - 1)) {
				kfree(fill_buf);
				return 0;
			}
		}
		pages_per_blk = (1<<(chip->phys_erase_shift-chip->page_shift));
	} else {
		if (page >= (BOOT_TOTAL_PAGES - 1))
			return 0;
	}

	/* actual page to be written */
	write_page = page;
	/* zero page of each copy */
	if ((write_page % each_boot_pages) == 0) {
		nand_info_page_prepare(aml_chip,
			chip->buffers->databuf);
		chip->cmdfunc(mtd, NAND_CMD_SEQIN, 0x00, write_page);
		/* must enable ran_mode for info page */
		if (en_slc == 0) {
			ran_mode = aml_chip->ran_mode;
			aml_chip->ran_mode = 1;
		}
		chip->ecc.write_page(mtd,
			chip, chip->buffers->databuf, 0, write_page);
		if (en_slc == 0)
			aml_chip->ran_mode = ran_mode;

		status = chip->waitfunc(mtd, chip);

		if ((status & NAND_STATUS_FAIL) && (chip->errstat))
			status = chip->errstat(mtd,
				chip, FL_WRITING, status, write_page);

		if (status & NAND_STATUS_FAIL) {
			pr_info("uboot wr 0 page=0x%x, status=0x%x\n",
				page, status);
			kfree(fill_buf);
			return -EIO;
		}
	}
	/* +1 for skipping nand info page */
	if (en_slc) {
		if (aml_chip->support_new_nand == 1) {
			page = page % pages_per_blk;
			if (type == HYNIX_1YNM_8GB)
				write_page =
				pagelist_1ynm_hynix256_mtd[page + 1];
			else
				write_page =
				pagelist_hynix256[page + 1];
		}
	} else
		write_page++;

WRITE_BAD_BLOCK:
	ofs = ((uint64_t)write_page << chip->page_shift);
	tmp = ofs;
	div_u64_rem(tmp, mtd->erasesize, &remainder);
	if (!remainder) {
		if (chip->block_bad(mtd, ofs)) {
			write_page +=
			1 << (chip->phys_erase_shift-chip->page_shift);
			goto WRITE_BAD_BLOCK;
		}
	}
	if (aml_chip->support_new_nand == 1) {
		if (new_nand_type == HYNIX_1YNM_8GB) {
			if ((page + 1) > 1)
				priv_slc_page =
				pagelist_1ynm_hynix256_mtd[page];
			else
				priv_slc_page = page;
			while ((priv_slc_page + 1) < write_page) {
				chip->cmdfunc(mtd,
					NAND_CMD_SEQIN,
					0x00, ++priv_slc_page);
				chip->ecc.write_page_raw(mtd,
				chip, fill_buf, 0, priv_slc_page);
				chip->waitfunc(mtd, chip);
				pr_info("%s, fill page:0x%x\n",
					__func__, priv_slc_page);
			}
		}
	}
	chip->cmdfunc(mtd, NAND_CMD_SEQIN, 0x00, write_page);

	if (unlikely(raw))
		chip->ecc.write_page_raw(mtd, chip, buf, 0, write_page);
	else
		chip->ecc.write_page(mtd, chip, buf, 0, write_page);

	if (!cached || !(chip->options & NAND_CACHEPRG)) {
		status = chip->waitfunc(mtd, chip);
		if ((status & NAND_STATUS_FAIL) && (chip->errstat))
			status = chip->errstat(mtd,
				chip, FL_WRITING, status, write_page);
		if (status & NAND_STATUS_FAIL) {
			pr_info("uboot wr page=0x%x, status=0x%x\n",
				page, status);
		if (aml_chip->support_new_nand == 1) {
			if (en_slc && slc_program_info->exit_enslc_mode)
				slc_program_info->exit_enslc_mode(mtd);
		}
			kfree(fill_buf);
			return -EIO;
		}
	} else
		status = chip->waitfunc(mtd, chip);

	if (aml_chip->support_new_nand == 1) {
		if (en_slc && slc_program_info->exit_enslc_mode)
			slc_program_info->exit_enslc_mode(mtd);
	}
	kfree(fill_buf);
	return 0;
}

/* extra char device for bootloader */
#define AML_CHAR_BOOT_DEV	(0)
#if (AML_CHAR_BOOT_DEV)
int erase_bootloader(struct mtd_info *mtd, int boot_num)
{
	struct nand_chip *chip = mtd->priv;
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	int page, each_boot_pages, boot_copy_num;
	int pages_per_block;
	int start_page, end_page;
	int status;

	if (aml_chip->bl_mode)
		boot_copy_num = 4;
	else
		boot_copy_num = (!aml_chip_uboot->boot_copy_num) ?
			1 : aml_chip_uboot->boot_copy_num;
	each_boot_pages = BOOT_TOTAL_PAGES/boot_copy_num;

	nand_get_device(mtd, FL_ERASING);
	/* Calculate start page and end page */
	start_page = boot_num * each_boot_pages;
	end_page = start_page + each_boot_pages;

	/* Calculate pages in each block */
	pages_per_block = 1 << (chip->phys_erase_shift - chip->page_shift);
	chip->select_chip(mtd, 0);/*fixit, chipnr is set 0 */
	for (page = start_page; page < end_page; page += pages_per_block) {
		chip->erase_cmd(mtd, page & chip->pagemask);
		pr_info("%s: finish erase page 0x%08x\n",
			__func__, page & chip->pagemask);
		status = chip->waitfunc(mtd, chip);
		if (status & NAND_STATUS_FAIL)
			pr_info("%s: failed erase, page 0x%08x\n",
			__func__, page & chip->pagemask);
	}
	chip->select_chip(mtd, -1);
	nand_release_device(mtd);
	return 0;
}

/*
 * Data structure to carry mtd
 */
struct uboot_file_info {
	struct mtd_info *mtd;
	int bootsize;
	int bootnum;
};

static int uboot_open(struct inode *inode, struct file *filp)
{
	struct uboot_file_info *ufi;
	struct mtd_info *mtd;
	int minor = iminor(inode);
	int devnum = minor >> 1;

	mutex_lock(&boot_mutex);
	mtd = get_mtd_device(NULL, devnum);
	if (IS_ERR(mtd))
		return PTR_ERR(mtd);

	ufi = kzalloc(sizeof(struct uboot_file_info), GFP_KERNEL);
	if (!ufi)
		return -ENOMEM;
	ufi->mtd = mtd;
	filp->private_data = ufi;
	mutex_unlock(&boot_mutex);
	return 0;
}

static loff_t uboot_llseek(struct file *file, loff_t off, int whence)
{
	loff_t newpos;
	struct mtd_info *mtd = &aml_chip_uboot->mtd;

	switch (whence) {
	case 0: /* SEEK_SET (start postion)*/
		newpos = off;
		break;

	case 1: /* SEEK_CUR */
		newpos = file->f_pos + off;
		break;

	case 2: /* SEEK_END */
		newpos = (loff_t)(BOOT_TOTAL_PAGES * mtd->writesize) - 1;
		newpos = newpos - off;
		break;

	default: /* can't happen */
		return -EINVAL;
	}

	if (newpos < 0)
		return -EINVAL;
	if (newpos >= (loff_t)(BOOT_TOTAL_PAGES * mtd->writesize))
		return -EINVAL;

	file->f_pos = newpos;

	return newpos;
}


/*
 * This function reads the u-boot envionment variables.
 * The f_pos points directly to the env location.
 */
static ssize_t uboot_read(struct file *file,
	char __user *buf,
	size_t count,
	loff_t *ppos)
{
	struct mtd_info *mtd = aml_chip_uboot->mtd;
	struct nand_chip *chip = &aml_chip_uboot->chip;
	unsigned char *data_buf, *buffer;
	int ret, page;
	size_t align_count = 0, read_size;
	loff_t addr;
	int chipnr;

	pr_info("%s %d file->f_pos =0x%llx, 0x%zx\n",
		__func__, __LINE__, (uint64_t)*ppos, count);

	if (*ppos >= (loff_t)(BOOT_TOTAL_PAGES * mtd->writesize)) {
		pr_err("boot read: data access violation!\n");
		return -EFAULT;
	}
	align_count =
		((((u32)count + mtd->writesize)-1)/mtd->writesize)
		* mtd->writesize;
	data_buf = vmalloc(align_count + mtd->writesize);
	if (!data_buf) {
		pr_info("malloc buf for rom_write failed");
		goto err_exit0;
	}
	addr = *ppos;
	buffer = data_buf;
	nand_get_device(mtd, FL_READING);
	chipnr = (int)(addr >> chip->chip_shift);
	chip->select_chip(mtd, chipnr);
	for (read_size = 0; read_size < align_count;
		read_size += mtd->writesize) {
		while ((addr%mtd->erasesize == 0)
			&& chip->block_bad(mtd, addr, 0))
			addr += mtd->erasesize;
		page = addr >> chip->page_shift;
		chip->cmdfunc(mtd, NAND_CMD_READ0, 0x00, page);
		chip->ecc.read_page(mtd, chip, buffer, 0, page);
		addr += mtd->writesize;
		buffer += mtd->writesize;
	}
	chip->select_chip(mtd, -1);
	nand_release_device(mtd);
	ret = copy_to_user(buf, data_buf, count);
err_exit0:
	vfree(data_buf);

	return count;
}

static ssize_t uboot_write(struct file *file, const char __user *buf,
			 size_t count, loff_t *ppos)
{
	struct mtd_info *mtd = &aml_chip_uboot->mtd;
	struct nand_chip *chip = &aml_chip_uboot->chip;
	unsigned char *data_buf, *buffer;
	int ret, page;
	size_t align_count = 0, write_size;
	loff_t addr;
	int chipnr;

	pr_info("%s %d file->f_pos =0x%llx\n",
			__func__, __LINE__, (uint64_t)*ppos);
	if (*ppos >= (loff_t)(BOOT_TOTAL_PAGES * mtd->writesize)) {
		pr_err("boot write: data access violation!\n");
		return -EFAULT;
	}

	align_count =
		((((u32)count + mtd->writesize)-1)/mtd->writesize)
			* mtd->writesize;
	pr_info("%s %d 0x%zx %d\n", __func__, __LINE__,
		align_count, aml_chip_uboot->boot_copy_num);
	data_buf = vmalloc(align_count + mtd->writesize);/*fixme*/
	if (!data_buf) {
		pr_info("malloc buf for rom_write failed\n");
		goto err_exit0;
	}
	memset(data_buf, 0x0, align_count);
	if (!aml_chip->bl_mode) {
		if (!aml_chip_uboot->boot_copy_num)
			get_boot_num(mtd, align_count);
	}

	ret = copy_from_user(data_buf, buf, count);
	addr = *ppos;
	buffer = data_buf;
	nand_get_device(mtd, FL_WRITING);
	chipnr = (int)(addr >> chip->chip_shift);
	chip->select_chip(mtd, chipnr);
	for (write_size = 0; write_size < align_count;
		write_size += mtd->writesize) {
		while ((addr % mtd->erasesize == 0)
			&& chip->block_bad(mtd, addr, 0))
			addr += mtd->erasesize;
		page = addr >> chip->page_shift;
		pr_info("%s %d write_size=0x%zx page=0x%x\n",
			__func__, __LINE__, write_size, page);
		chip->write_page(mtd, chip, 0, 0, buffer, 0, page, 0, 0);
		addr += mtd->writesize;
		buffer += mtd->writesize;
	}
	chip->select_chip(mtd, -1);
	nand_release_device(mtd);
	file->f_pos += align_count;
err_exit0:
	vfree(data_buf);
	return count;
}

static int uboot_close(struct inode *inode, struct file *file)
{
	return 0;
}

static int uboot_suspend(struct device *dev, pm_message_t state)
{
	return 0;
}

static int uboot_resume(struct device *dev)
{
	return 0;
}

struct boot_info {
	int bootnum;
	int bootsize;
};

#define BOOT_SET_INFO	_IOWR('U', 1, struct boot_info)
#define BOOT_GET_INFO	_IOWR('U', 2, struct boot_info)
#define BOOT_ERASE_INFO	_IOWR('U', 3, int)

static int boot_ioctl(struct file *file, u_int cmd, u_long arg)
{
	struct uboot_file_info *ufi = file->private_data;
	struct mtd_info *mtd = ufi->mtd;
	void __user *argp = (void __user *)arg;
	int ret = 0, erase_boot_num = 0;
	u_long size;

	pr_debug("boot_ioctl\n");

	size = (cmd & IOCSIZE_MASK) >> IOCSIZE_SHIFT;
	if (cmd & IOC_IN) {
		if (!access_ok(VERIFY_READ, argp, size))
			return -EFAULT;
	}
	if (cmd & IOC_OUT) {
		if (!access_ok(VERIFY_WRITE, argp, size))
			return -EFAULT;
	}
	switch (cmd) {
	case BOOT_SET_INFO:
	{
		struct boot_info info;

		if (copy_to_user(&info, argp, sizeof(struct boot_info)))
			return -EFAULT;
		ufi->bootsize = info.bootsize;
		ufi->bootnum = info.bootnum;
		/*aml_chip_uboot->boot_copy_num = info.boot_num;*/
		/* because bad block,
		 * need call get_boot_num to get boot copies number
		 */
		get_boot_num(mtd, ufi->bootsize);
		break;
	}
	case BOOT_GET_INFO:
	{
		struct boot_info __user *info = argp;

		if (put_user(aml_chip_uboot->boot_copy_num, &(info->bootnum)))
			return -EFAULT;
		break;
	}
	case BOOT_ERASE_INFO:
		if (copy_to_user(&erase_boot_num, argp, sizeof(int)))
			return -EFAULT;
		erase_bootloader(mtd, erase_boot_num);
	break;
	default:
		ret = -ENOTTY;
	}
	return ret;
}

static long boot_unlocked_ioctl(struct file *file, u_int cmd, u_long arg)
{
	int ret;

	mutex_lock(&boot_mutex);
	ret = boot_ioctl(file, cmd, arg);
	mutex_unlock(&boot_mutex);

	return ret;
}

#ifdef CONFIG_COMPAT

#define BOOT_SET_INFO32	_IOWR('U', 8, struct boot_info)
#define BOOT_GET_INFO32	_IOWR('U', 9, struct boot_info)
#define BOOT_ERASE_INFO32 _IOWR('U', 10, int)

static long boot_compat_ioctl(struct file *file, uint32_t cmd,
	unsigned long arg)
{
	struct uboot_file_info *ufi = file->private_data;
	struct mtd_info *mtd = ufi->mtd;
	void __user *argp = compat_ptr(arg);
	int ret = 0;

	mutex_lock(&boot_mutex);

	switch (cmd) {
	case BOOT_SET_INFO32:
	{
		struct boot_info info;

		if (copy_from_user(&info, argp, sizeof(struct boot_info)))
			ret = -EFAULT;
		else
			aml_chip_uboot->boot_copy_num = info.bootnum;
		break;
	}
	case BOOT_GET_INFO32:
	{
		struct boot_info __user *info = argp;

		if (put_user(aml_chip_uboot->boot_copy_num, &(info->bootnum)))
			ret = -EFAULT;
		break;
	}
	case BOOT_ERASE_INFO32:
	{
		int erase_boot_num;

		if (copy_from_user(&erase_boot_num, argp, sizeof(int)))
			ret = -EFAULT;
		else
			erase_bootloader(mtd, erase_boot_num);
		break;
	}
	default:
		ret = boot_ioctl(file, cmd, (unsigned long)argp);
	}

	mutex_unlock(&boot_mutex);

	return ret;
}

#endif /* CONFIG_COMPAT */


static struct class uboot_class = {

	.name = "bootloader",
	.owner = THIS_MODULE,
	.suspend = uboot_suspend,
	.resume = uboot_resume,
};

static const struct file_operations uboot_fops = {
	.owner = THIS_MODULE,
	.open = uboot_open,
	.read = uboot_read,
	.write = uboot_write,
	.unlocked_ioctl	= boot_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= boot_compat_ioctl,
#endif
	.llseek	= uboot_llseek,
	.release = uboot_close,
};

int boot_device_register(struct aml_nand_chip *aml_chip)
{
	int ret = 0;

	aml_chip_uboot = aml_chip;

	pr_info("boot device register %d\n", aml_chip_uboot->boot_copy_num);

	aml_chip_uboot->boot_copy_num = 0;

	ret = alloc_chrdev_region(&uboot_devno, 0, 1, "bootloader");
	if (ret < 0) {
		pr_info("failed to allocate chrdev.");
		goto exit_error0;
	}
	cdev_init(&uboot_cdev, &uboot_fops);
	uboot_cdev.owner = THIS_MODULE;
	ret = cdev_add(&uboot_cdev, uboot_devno, 1);
	if (ret) {
		pr_info("failed to add device.");
		goto exit_error0;
	}

	ret = class_register(&uboot_class);
	if (ret < 0) {
		pr_info("class_register(&uboot_class) failed!\n");
		goto exit_error0;
	}

	devp = device_create(&uboot_class,
			NULL,
			uboot_devno,
			NULL,
			"bootloader");
	if (IS_ERR(devp)) {
		pr_info("fail to create node\n");
		ret = PTR_ERR(devp);
		goto exit_error0;
	}

	return 0;
exit_error0:
	return ret;
}
#endif
/* endof file */
