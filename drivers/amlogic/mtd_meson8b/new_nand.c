/*
 * drivers/amlogic/mtd/new_nand.c
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

/*****************************HYNIX******************************************/
uint8_t aml_nand_get_reg_value_hynix(struct aml_nand_chip *aml_chip,
	uint8_t *buf, uint8_t *addr, int chipnr, int cnt)
{
	struct nand_chip *chip = &aml_chip->chip;
	struct mtd_info *mtd = aml_chip->mtd;
	int j;

	if ((aml_chip->new_nand_info.type == 0)
		|| (aml_chip->new_nand_info.type > 10))
		return 0;

	aml_chip->aml_nand_wait_devready(aml_chip, chipnr);
	aml_chip->aml_nand_command(aml_chip,
		NAND_CMD_HYNIX_GET_VALUE, -1, -1, chipnr);

	for (j = 0; j < cnt; j++) {
		chip->cmd_ctrl(mtd,
			addr[j], NAND_CTRL_CHANGE | NAND_NCE | NAND_ALE);
		NFC_SEND_CMD_IDLE(controller, 10);
		buf[j] = chip->read_byte(mtd);
		NFC_SEND_CMD_IDLE(controller, 10);
	}

	aml_chip->aml_nand_wait_devready(aml_chip, chipnr);

	return 0;
}

static int dummy_read(struct aml_nand_chip *aml_chip, unsigned char chipnr)
{
	struct nand_chip *chip = &aml_chip->chip;
	struct mtd_info *mtd = aml_chip->mtd;
	int i, ret = 0;

	ret = aml_chip->aml_nand_wait_devready(aml_chip, chipnr);
	aml_chip->aml_nand_command(aml_chip,
		NAND_CMD_READ0, -1, -1, chipnr);
	for (i = 0; i < 5; i++)
		chip->cmd_ctrl(mtd,
		0x0, NAND_CTRL_CHANGE | NAND_NCE | NAND_ALE);
	aml_chip->aml_nand_command(aml_chip,
		NAND_CMD_READSTART, -1, -1, chipnr);

	ret = aml_chip->aml_nand_wait_devready(aml_chip, chipnr);

	return ret;
}

uint8_t aml_nand_set_reg_value_hynix(struct aml_nand_chip *aml_chip,
	uint8_t *buf, uint8_t *addr, int chipnr, int cnt)
{
	struct nand_chip *chip = &aml_chip->chip;
	struct mtd_info *mtd = aml_chip->mtd;
	int j;

	if ((aml_chip->new_nand_info.type == 0)
		|| (aml_chip->new_nand_info.type > 10))
		return 0;
	aml_chip->aml_nand_wait_devready(aml_chip, chipnr);
	aml_chip->aml_nand_command(aml_chip,
		NAND_CMD_HYNIX_SET_VALUE_START, -1, -1, chipnr);
	for (j = 0; j < cnt; j++) {
		chip->cmd_ctrl(mtd, addr[j],
			NAND_CTRL_CHANGE | NAND_NCE | NAND_ALE);
		NFC_SEND_CMD_IDLE(controller, 15);
		aml_chip->aml_nand_write_byte(aml_chip, buf[j]);
		NFC_SEND_CMD_IDLE(controller, 0);

	}
	aml_chip->aml_nand_command(aml_chip,
		NAND_CMD_HYNIX_SET_VALUE_END, -1, -1, chipnr);
	aml_chip->aml_nand_wait_devready(aml_chip, chipnr);

	return 0;
}

int8_t aml_nand_get_20nm_OTP_value(struct aml_nand_chip *aml_chip,
	unsigned char *buf, int chipnr)
{
	struct nand_chip *chip = &aml_chip->chip;
	struct mtd_info *mtd = aml_chip->mtd;
	int i, j, k, reg_cnt_otp, total_reg_cnt, check_flag = 0;
	unsigned char  *tmp_buf;
	u8 **def_value;
	u8 ***offset_value;
	u8 reg_cnt, retry_cnt;

	def_value =
		(u8 **)aml_chip->new_nand_info.read_rety_info.reg_default_value;
	offset_value =
		(u8 ***)aml_chip->new_nand_info.read_rety_info.reg_offset_value;

	total_reg_cnt = chip->read_byte(mtd);
	reg_cnt_otp = chip->read_byte(mtd);

	pr_info("%s %d : total byte %d\n",
		__func__, __LINE__, total_reg_cnt * reg_cnt_otp);

	for (i = 0; i < 8; i++) {
		check_flag = 0;
		memset(buf, 0, 128);
		for (j = 0; j < 128; j++) {
			buf[j] = chip->read_byte(mtd);
			ndelay(50);
		}
		for (j = 0; j < 64; j += 8) {
			for (k = 0; k < 7; k++) {
				if (((buf[k+j] < 0x80) && (buf[k+j+64] < 0x80))
				|| ((buf[k+j] > 0x80) && (buf[k+j+64] > 0x80))
			|| ((unsigned char)(buf[k+j]^buf[k+j+64]) != 0xFF)) {
					check_flag = 1;
					break;
				}
				if (check_flag)
					break;
			}
			if (check_flag)
				break;
		}
		if (check_flag == 0)
			break;
	}

	if (check_flag) {
		pr_info("%s %d:20nm flash default value abnormal,chip[%d]!!!\n",
			__func__, __LINE__, chipnr);
		/*WARN_ON();*/
	} else {
		reg_cnt = aml_chip->new_nand_info.read_rety_info.reg_cnt;
		retry_cnt = aml_chip->new_nand_info.read_rety_info.retry_cnt;
		tmp_buf = buf;
		memcpy(&def_value[chipnr][0], tmp_buf, reg_cnt);
		tmp_buf += reg_cnt;
		for (j = 0; j < retry_cnt; j++) {
			for (k = 0; k < reg_cnt; k++) {
				offset_value[chipnr][j][k] = (char)tmp_buf[0];
				tmp_buf++;
			}
		}
	}
	return check_flag;
}

int8_t aml_nand_get_1ynm_OTP_value(struct aml_nand_chip *aml_chip,
	unsigned char *buf, int chipnr)
{
	struct nand_chip *chip = &aml_chip->chip;
	struct mtd_info *mtd = aml_chip->mtd;
	int i, j = 1, k, m;
	int read_otp_cnt = 0;
	unsigned char retry_value_sta[32] = {0};
	u8 **def_value, *reg_addr;
	u8 ***offset_value;
	u8 reg_cnt, retry_cnt;

	def_value =
		(u8 **)aml_chip->new_nand_info.read_rety_info.reg_default_value;
	offset_value =
		(u8 ***)aml_chip->new_nand_info.read_rety_info.reg_offset_value;
	reg_addr = aml_chip->new_nand_info.read_rety_info.reg_addr;

	memset(buf, 0, 528);
	for (i = 0; i < 528; i++) {
		buf[i] = chip->read_byte(mtd);
		NFC_SEND_CMD_IDLE(controller, 0);
		NFC_SEND_CMD_IDLE(controller, 0);
	}

	for (read_otp_cnt = 0; read_otp_cnt < 8; read_otp_cnt++) {
		for (j = 0; j < 8; j++) {
			for (k = 0; k < 4; k++) {
				if (retry_value_sta[j * 4 + k] == 0) {
					m = k + j * 4 + 16 + read_otp_cnt * 64;
			if ((buf[m] ^ buf[m + 32]) == 0xFF) {
				if (j == 0)
					def_value[chipnr][k] = buf[m];
				else
					offset_value[chipnr][j-1][k] = buf[m];
				retry_value_sta[j * 4 + k] = 1;
			}
				}

			}
		}
	}
	reg_cnt = aml_chip->new_nand_info.read_rety_info.reg_cnt;
	for (i = 0; i < controller->chip_num; i++) {
		if (aml_chip->valid_chip[i]) {
			for (j = 0; j < reg_cnt; j++)
				pr_info("%s, REG(0x%x):value:0x%2x,chip[%d]\n",
					__func__,
					reg_addr[j],
					def_value[i][j],
					i);
		}
	}

	retry_cnt = aml_chip->new_nand_info.read_rety_info.retry_cnt;
	for (i = 0; i < controller->chip_num; i++) {
		if (aml_chip->valid_chip[i]) {
			for (j = 0; j < retry_cnt; j++)
				for (k = 0; k < reg_cnt; k++)
					pr_info("%s,%dst,(0x%x)::0x%2x,[%d]\n",
						__func__, j,
						reg_addr[k],
						offset_value[i][j][k],
						i);
			pr_info("\n");
		}
	}
	for (i = 0; i < 32; i++)
		if (retry_value_sta[i] == 0) {
			pr_info("  chip[%d] flash %d value abnormal not safe\n",
				chipnr, i);
			return 1;
		}

	return 0;
}

uint8_t aml_nand_get_reg_value_formOTP_hynix(struct aml_nand_chip *aml_chip,
	int chipnr)
{
	struct nand_chip *chip = &aml_chip->chip;
	struct mtd_info *mtd = aml_chip->mtd;
	int  check_flag = 0;
	unsigned char *one_copy_buf;

	if ((aml_chip->new_nand_info.type < 3)
		|| (aml_chip->new_nand_info.type > 10))
		return 0;

	one_copy_buf = kmalloc(528, GFP_KERNEL);
	if (one_copy_buf == NULL) {
		/*WARN_ON();*/
		return 0;
	}

	aml_chip->aml_nand_wait_devready(aml_chip, chipnr);
	aml_chip->aml_nand_command(aml_chip,
		NAND_CMD_RESET, -1, -1, chipnr);
	aml_chip->aml_nand_wait_devready(aml_chip, chipnr);

	aml_chip->aml_nand_command(aml_chip,
		NAND_CMD_HYNIX_SET_VALUE_START, -1, -1, chipnr);
		NFC_SEND_CMD_IDLE(controller, 0);
	if (aml_chip->new_nand_info.type == HYNIX_20NM_8GB) {
		chip->cmd_ctrl(mtd, 0xff,
			NAND_CTRL_CHANGE | NAND_NCE | NAND_ALE);
		NFC_SEND_CMD_IDLE(controller, 0);
		aml_chip->aml_nand_write_byte(aml_chip, 0x40);
		NFC_SEND_CMD_IDLE(controller, 0);
		chip->cmd_ctrl(mtd, 0xcc,
			NAND_CTRL_CHANGE | NAND_NCE | NAND_ALE);
	} else if (aml_chip->new_nand_info.type == HYNIX_20NM_4GB) {
		chip->cmd_ctrl(mtd, 0xae,
			NAND_CTRL_CHANGE | NAND_NCE | NAND_ALE);
		NFC_SEND_CMD_IDLE(controller, 0);
		aml_chip->aml_nand_write_byte(aml_chip, 0x00);
		NFC_SEND_CMD_IDLE(controller, 0);
		chip->cmd_ctrl(mtd, 0xb0,
			NAND_CTRL_CHANGE | NAND_NCE | NAND_ALE);
	} else if (aml_chip->new_nand_info.type == HYNIX_1YNM_8GB) {
		chip->cmd_ctrl(mtd, 0x38,
			NAND_CTRL_CHANGE | NAND_NCE | NAND_ALE);
		NFC_SEND_CMD_IDLE(controller, 0);
		aml_chip->aml_nand_write_byte(aml_chip, 0x52);
		NFC_SEND_CMD_IDLE(controller, 0);
	}
		NFC_SEND_CMD_IDLE(controller, 0);

	if ((aml_chip->new_nand_info.type == HYNIX_20NM_4GB)
		|| (aml_chip->new_nand_info.type == HYNIX_20NM_8GB))
		aml_chip->aml_nand_write_byte(aml_chip, 0x4d);

	aml_chip->aml_nand_command(aml_chip, 0x16, -1, -1, chipnr);
	aml_chip->aml_nand_command(aml_chip, 0x17, -1, -1, chipnr);
	aml_chip->aml_nand_command(aml_chip, 0x04, -1, -1, chipnr);
	aml_chip->aml_nand_command(aml_chip, 0x19, -1, -1, chipnr);
	aml_chip->aml_nand_command(aml_chip, NAND_CMD_READ0, 0, 0x200, chipnr);

	aml_chip->aml_nand_wait_devready(aml_chip, chipnr);
	if (aml_chip->ops_mode & AML_CHIP_NONE_RB)
		chip->cmd_ctrl(mtd, NAND_CMD_READ0 & 0xff,
			NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);


	if ((aml_chip->new_nand_info.type == HYNIX_20NM_4GB)
		|| (aml_chip->new_nand_info.type == HYNIX_20NM_8GB))
		check_flag  = aml_nand_get_20nm_OTP_value(aml_chip,
			one_copy_buf, chipnr);
	else if (aml_chip->new_nand_info.type == HYNIX_1YNM_8GB)
		check_flag  = aml_nand_get_1ynm_OTP_value(aml_chip,
			one_copy_buf, chipnr);

	aml_chip->aml_nand_command(aml_chip, NAND_CMD_RESET, -1, -1, chipnr);

	aml_chip->aml_nand_wait_devready(aml_chip, chipnr);
	if ((aml_chip->new_nand_info.type == HYNIX_20NM_4GB)
		|| (aml_chip->new_nand_info.type == HYNIX_20NM_8GB))
		aml_chip->aml_nand_command(aml_chip, 0x38, -1, -1, chipnr);
	else if (aml_chip->new_nand_info.type == HYNIX_1YNM_8GB) {
		aml_chip->aml_nand_command(aml_chip,
			NAND_CMD_HYNIX_SET_VALUE_START, -1, -1, chipnr);
		chip->cmd_ctrl(mtd, 0x38,
			NAND_CTRL_CHANGE | NAND_NCE | NAND_ALE);
		NFC_SEND_CMD_IDLE(controller, 0);
		aml_chip->aml_nand_write_byte(aml_chip, 0);
		NFC_SEND_CMD_IDLE(controller, 0);
		aml_chip->aml_nand_command(aml_chip, 0X16, -1, -1, chipnr);
	}
	aml_chip->aml_nand_wait_devready(aml_chip, chipnr);

	return check_flag;
}

void aml_nand_set_readretry_default_value_hynix(struct mtd_info *mtd)
{
	unsigned char hynix_reg_read_value_tmp[READ_RETRY_REG_NUM];
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	struct nand_chip *chip = mtd->priv;
	int i;

	if ((aml_chip->new_nand_info.type == 0)
		|| (aml_chip->new_nand_info.type > 10))
		return;


	memset(&hynix_reg_read_value_tmp[0], 0, READ_RETRY_REG_NUM);

	chip->select_chip(mtd, 0);
	for (i = 0; i < controller->chip_num; i++) {
		if (aml_chip->valid_chip[i]) {
			aml_nand_set_reg_value_hynix(aml_chip,
		&aml_chip->new_nand_info.read_rety_info.reg_default_value[i][0],
		&aml_chip->new_nand_info.read_rety_info.reg_addr[0],
		i, aml_chip->new_nand_info.read_rety_info.reg_cnt);
			/*
			 *aml_nand_hynix_get_parameters(aml_chip,
			 *	&hynix_reg_read_value_tmp[0],
			 *	&aml_chip->hynix_reg_read_addr[0], i, 4);
			 */
		}
	}
}

void aml_nand_enter_enslc_mode_hynix(struct mtd_info *mtd)
{
	unsigned char hynix_reg_program_value_tmp[ENHANCE_SLC_REG_NUM];
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	struct nand_chip *chip = mtd->priv;
	int i, j;

	if ((aml_chip->new_nand_info.type == 0)
		|| (aml_chip->new_nand_info.type > 10))
		return;

	memset(&hynix_reg_program_value_tmp[0], 0, ENHANCE_SLC_REG_NUM);

	chip->select_chip(mtd, 0);
	for (i = 0; i < controller->chip_num; i++) {
		if (aml_chip->valid_chip[i]) {
			for (j = 0;
		j < aml_chip->new_nand_info.slc_program_info.reg_cnt; j++)
				hynix_reg_program_value_tmp[j] =
	aml_chip->new_nand_info.slc_program_info.reg_default_value[i][j] +
	aml_chip->new_nand_info.slc_program_info.reg_offset_value[j];

			aml_nand_set_reg_value_hynix(aml_chip,
				&hynix_reg_program_value_tmp[0],
			&aml_chip->new_nand_info.slc_program_info.reg_addr[0],
			i,
			aml_chip->new_nand_info.slc_program_info.reg_cnt);
			memset(&hynix_reg_program_value_tmp[0],
			0, aml_chip->new_nand_info.slc_program_info.reg_cnt);
		}
	}
}

/* working in Normal program mode */
void aml_nand_exit_enslc_mode_hynix(struct mtd_info *mtd)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	struct nand_chip *chip = mtd->priv;
	int i;

	if ((aml_chip->new_nand_info.type == 0)
		|| (aml_chip->new_nand_info.type > 10))
		return;

	chip->select_chip(mtd, 0);
	for (i = 0; i < controller->chip_num; i++) {
		if (aml_chip->valid_chip[i])
			aml_nand_set_reg_value_hynix(aml_chip,
	&aml_chip->new_nand_info.slc_program_info.reg_default_value[i][0],
			&aml_chip->new_nand_info.slc_program_info.reg_addr[0],
			i, aml_chip->new_nand_info.slc_program_info.reg_cnt);
		dummy_read(aml_chip, i);
	}
}

int aml_nand_slcprog_1ynm_hynix(struct mtd_info *mtd,
	unsigned char *buf, unsigned char *oob_buf, unsigned int page_addr)
{
	int pages_per_blk = mtd->erasesize / mtd->writesize;
	int offset_in_blk = page_addr % pages_per_blk;
	int error = 0;
	struct mtd_oob_ops aml_oob_ops;
	/*struct nand_chip *chip = mtd->priv;*/
	unsigned char *data_buf;
	loff_t op_add;

	unsigned int op_page_add, temp_value;
	unsigned int priv_slc_page, next_msb_page;

	temp_value = pagelist_1ynm_hynix256_mtd[offset_in_blk];
	op_page_add = (page_addr / pages_per_blk) * pages_per_blk + temp_value;
	data_buf = kzalloc(mtd->writesize, GFP_KERNEL);
	if (data_buf == NULL)
		return -ENOMEM;
	temp_value = pagelist_1ynm_hynix256_mtd[offset_in_blk - 1];
	if (offset_in_blk > 1)
		priv_slc_page =
		(page_addr / pages_per_blk) * pages_per_blk + temp_value;
	else
		priv_slc_page = op_page_add;
	next_msb_page = priv_slc_page + 1;
	memset(data_buf, 0xff, mtd->writesize);
	while (next_msb_page < op_page_add) {
		aml_oob_ops.mode = MTD_OPS_RAW;
		aml_oob_ops.len = mtd->writesize;
		aml_oob_ops.ooblen = mtd->oobavail;
		aml_oob_ops.ooboffs = 0;/*fixme! all layout offs is zero*/
		aml_oob_ops.datbuf = data_buf;
		aml_oob_ops.oobbuf = oob_buf;
		op_add = next_msb_page*mtd->writesize;
		mtd->_write_oob(mtd, op_add, &aml_oob_ops);
		pr_info("Eneter 1y nm SLC mode ,must fill 0xff data into %d\n",
			next_msb_page);
		next_msb_page++;
	}
	aml_oob_ops.mode = MTD_OPS_AUTO_OOB;
	aml_oob_ops.len = mtd->writesize;
	aml_oob_ops.ooblen = mtd->oobavail;
	aml_oob_ops.ooboffs = 0;/*fixme! all layout offs is zero*/
	aml_oob_ops.datbuf = buf;
	aml_oob_ops.oobbuf = oob_buf;
	op_add = op_page_add*mtd->writesize;
	error = mtd->_write_oob(mtd, op_add, &aml_oob_ops);
	pr_info("Eneter 1y nm SLC mode ,write systerm data into %d\n",
		op_page_add);
	if (error) {
		pr_info("blk check good but write failed: %llx, %d\n",
			(uint64_t)page_addr, error);
		goto err;
	}
err:
	kfree(data_buf);
	return error;
}

void aml_nand_read_retry_handle_hynix(struct mtd_info *mtd, int chipnr)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	u8 hynix_reg_read_value[READ_RETRY_REG_NUM];
	int i, cur_cnt;
	int retry_zone, retry_offset;
	struct new_tech_nand_t *new_nand_info;
	u8 **def_value, *reg_addr;
	u8 ***offset_value;
	u8 reg_cnt, retry_cnt;

	new_nand_info = &aml_chip->new_nand_info;
	retry_cnt = new_nand_info->read_rety_info.retry_cnt;
	offset_value = (u8 ***)new_nand_info->read_rety_info.reg_offset_value;
	def_value = (u8 **)new_nand_info->read_rety_info.reg_default_value;
	reg_addr = new_nand_info->read_rety_info.reg_addr;
	reg_cnt = new_nand_info->read_rety_info.reg_cnt;

	if ((new_nand_info->type == 0) || (new_nand_info->type > 10))
		return;

	if (new_nand_info->read_rety_info.cur_cnt[chipnr] < retry_cnt)
		cur_cnt = new_nand_info->read_rety_info.cur_cnt[chipnr];
	else {
		retry_zone =
		new_nand_info->read_rety_info.cur_cnt[chipnr] / retry_cnt;
		retry_offset =
		new_nand_info->read_rety_info.cur_cnt[chipnr] % retry_cnt;
		cur_cnt = (retry_zone + retry_offset) % retry_cnt;
	}

	memset(&hynix_reg_read_value[0], 0, READ_RETRY_REG_NUM);

	for (i = 0; i < reg_cnt; i++) {
		if ((new_nand_info->type == HYNIX_26NM_8GB)
			|| (new_nand_info->type == HYNIX_26NM_4GB)) {
			if (offset_value[0][cur_cnt][i] == READ_RETRY_ZERO)
				hynix_reg_read_value[i] = 0;
			else
				hynix_reg_read_value[i] =
			def_value[chipnr][i] + offset_value[0][cur_cnt][i];
		} else if ((new_nand_info->type == HYNIX_20NM_8GB)
			|| (new_nand_info->type == HYNIX_20NM_4GB)
			|| (new_nand_info->type == HYNIX_1YNM_8GB))
			hynix_reg_read_value[i] =
				offset_value[chipnr][cur_cnt][i];
	}

	aml_nand_set_reg_value_hynix(aml_chip,
		&hynix_reg_read_value[0],
		&new_nand_info->read_rety_info.reg_addr[0],
		chipnr,
		reg_cnt);
	new_nand_info->read_rety_info.cur_cnt[chipnr]++;
}


void aml_nand_read_retry_exit_hynix(struct mtd_info *mtd, int chipnr)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	struct nand_chip *chip = mtd->priv;
	int i;
	struct new_tech_nand_t *new_nand_info;

	new_nand_info = &aml_chip->new_nand_info;

	if ((new_nand_info->type == 0)
		|| (new_nand_info->type > 10))
		return;

	pr_info("hyinx retry cnt :%d\n",
		new_nand_info->read_rety_info.cur_cnt[chipnr]);

	chip->select_chip(mtd, chipnr);
	for (i = 0; i < controller->chip_num; i++) {
		if (aml_chip->valid_chip[i])
			aml_nand_set_reg_value_hynix(aml_chip,
			&new_nand_info->read_rety_info.reg_default_value[i][0],
			&new_nand_info->read_rety_info.reg_addr[0],
			i, new_nand_info->read_rety_info.reg_cnt);
	}
	memset(&aml_chip->new_nand_info.read_rety_info.cur_cnt[0],
		0, MAX_CHIP_NUM);
}

void aml_nand_get_slc_default_value_hynix(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	int i;

	chip->select_chip(mtd, 0);
	for (i = 0; i < controller->chip_num; i++) {
		if (aml_chip->valid_chip[i])
			aml_nand_get_reg_value_hynix(aml_chip,
	&aml_chip->new_nand_info.slc_program_info.reg_default_value[i][0],
			&aml_chip->new_nand_info.slc_program_info.reg_addr[0],
			i, aml_chip->new_nand_info.slc_program_info.reg_cnt);
	}
}

void aml_nand_get_read_default_value_hynix(struct mtd_info *mtd)
{
	struct mtd_oob_ops aml_oob_ops;
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	struct nand_chip *chip = mtd->priv;
	size_t addr;
	unsigned char *data_buf;
	char oob_buf[4];
	unsigned char page_list[RETRY_NAND_COPY_NUM] = {0x07, 0x0B, 0x0F, 0x13};
	int nand_type, total_blk, phys_erase_shift = fls(mtd->erasesize) - 1;
	int error = 0, i, j;
	u8 **def_value, *reg_addr;
	u8 ***offset_value;
	u8 reg_cnt, retry_cnt;
	struct new_tech_nand_t *new_nand_info;

	new_nand_info = &aml_chip->new_nand_info;
	retry_cnt = new_nand_info->read_rety_info.retry_cnt;
	offset_value = (u8 ***)new_nand_info->read_rety_info.reg_offset_value;
	def_value = (u8 **)new_nand_info->read_rety_info.reg_default_value;
	reg_addr = new_nand_info->read_rety_info.reg_addr;
	reg_cnt = new_nand_info->read_rety_info.reg_cnt;

	data_buf = kzalloc(mtd->writesize, GFP_KERNEL);
	if (data_buf == NULL)
		return;

	if (nand_boot_flag)
		addr = (1024 * mtd->writesize / aml_chip->plane_num);
	else
		addr = 0;

	total_blk = 0;
	aml_chip->new_nand_info.read_rety_info.default_flag = 0;
	if (aml_chip->new_nand_info.type == HYNIX_1YNM_8GB) {
		page_list[0] = 0;
		page_list[1] = 1;
		page_list[2] = 3;
		page_list[3] = 5;
	}

	while (total_blk < RETRY_NAND_BLK_NUM) {
		nand_type = aml_chip->new_nand_info.type;
		aml_chip->new_nand_info.type = 0;
		error = mtd->_block_isbad(mtd, addr);
		aml_chip->new_nand_info.type = nand_type;
		if (error) {
			pr_info("%s %d detect bad blk at blk:0x%x\n",
				__func__, __LINE__,
				(u32)addr >> phys_erase_shift);
			addr += mtd->erasesize;
			total_blk++;
			continue;
		}

		aml_oob_ops.mode = MTD_OPS_AUTO_OOB;
		aml_oob_ops.len = mtd->writesize;
		aml_oob_ops.ooblen = 4;
		aml_oob_ops.ooboffs = 0;/*fixme! all layout offs is zero*/
		aml_oob_ops.datbuf = data_buf;
		aml_oob_ops.oobbuf = (uint8_t *)oob_buf;

		memset(oob_buf, 0, 4);
		memset((unsigned char *)aml_oob_ops.datbuf,
			0x0, mtd->writesize);
		memset((unsigned char *)aml_oob_ops.oobbuf,
			0x0, aml_oob_ops.ooblen);

		for (i = 0; i < RETRY_NAND_COPY_NUM; i++) {
			memset(oob_buf, 0, 4);
			memset((unsigned char *)aml_oob_ops.datbuf,
				0x0, mtd->writesize);
			memset((unsigned char *)aml_oob_ops.oobbuf,
				0x0, aml_oob_ops.ooblen);
			nand_type = aml_chip->new_nand_info.type;
			aml_chip->new_nand_info.type = 0;
			error = mtd->_read_oob(mtd,
			(addr + page_list[i]*mtd->writesize), &aml_oob_ops);
			aml_chip->new_nand_info.type = nand_type;
			if ((error != 0) && (error != -EUCLEAN)) {
				pr_info("%s %d read failed at blk:%zd pg:%zd\n",
					__func__, __LINE__,
					addr >> phys_erase_shift,
			(addr + page_list[i]*mtd->writesize) / mtd->writesize);
				continue;
			}
			if (!memcmp(oob_buf, RETRY_NAND_MAGIC, 4)) {
				memcpy(&def_value[0][0],
					(unsigned char *)aml_oob_ops.datbuf,
					MAX_CHIP_NUM * READ_RETRY_REG_NUM);
		for (i = 0; i < controller->chip_num; i++) {
			if (aml_chip->valid_chip[i]) {
				for (j = 0; j < reg_cnt; j++)
					pr_info("%s,(0x%x)val:0x%2x [%d]\n",
						__func__, reg_addr[j],
						def_value[i][j], i);
			}
		}

		if ((aml_chip->new_nand_info.type == HYNIX_20NM_8GB)
		|| (aml_chip->new_nand_info.type == HYNIX_20NM_4GB)
		|| (aml_chip->new_nand_info.type == HYNIX_1YNM_8GB)) {
			for (i = 0; i < controller->chip_num; i++) {
				if (aml_chip->valid_chip[i])
					for (j = 0; j < retry_cnt; j++)
						memcpy(&offset_value[i][j][0],
					(unsigned char *)(aml_oob_ops.datbuf +
					MAX_CHIP_NUM*READ_RETRY_REG_NUM +
					j*READ_RETRY_REG_NUM
					+ i*HYNIX_RETRY_CNT*READ_RETRY_REG_NUM),
					READ_RETRY_REG_NUM);
			}
		}
			aml_chip->new_nand_info.read_rety_info.default_flag = 1;
			goto READ_OK;
			}
		}

		addr += mtd->erasesize;
		total_blk++;
	}
	aml_chip->new_nand_info.read_rety_info.default_flag = 0;

	pr_info("%s %d read def readretry reg value using SLC\n",
		__func__, __LINE__);
	chip->select_chip(mtd, 0);

for (i = 0; i < controller->chip_num; i++) {
	if (aml_chip->valid_chip[i]) {
		if ((aml_chip->new_nand_info.type == HYNIX_26NM_8GB)
		|| (aml_chip->new_nand_info.type == HYNIX_26NM_4GB)) {
			aml_nand_get_reg_value_hynix(aml_chip,
			&def_value[i][0], &reg_addr[0], i, reg_cnt);
		} else if ((aml_chip->new_nand_info.type == HYNIX_20NM_8GB)
		|| (aml_chip->new_nand_info.type == HYNIX_20NM_4GB)
		|| (aml_chip->new_nand_info.type == HYNIX_1YNM_8GB))
			aml_nand_get_reg_value_formOTP_hynix(aml_chip, i);
	}
}
READ_OK:
	kfree(data_buf);
}

void aml_nand_save_read_default_value_hynix(struct mtd_info *mtd)
{
	struct mtd_oob_ops aml_oob_ops;
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	size_t addr;
	unsigned char *data_buf;
	char oob_buf[4];
	unsigned char page_list[RETRY_NAND_COPY_NUM] = {0x07, 0x0B, 0x0F, 0x13};
	int i, j, total_blk, phys_erase_shift = fls(mtd->erasesize)-1;
	int error = 0;
	struct erase_info erase_info_read;
	u8 **def_value;
	u8 reg_cnt;

	def_value =
		(u8 **)aml_chip->new_nand_info.read_rety_info.reg_default_value;
	reg_cnt = aml_chip->new_nand_info.read_rety_info.reg_cnt;

	if (aml_chip->new_nand_info.type != HYNIX_1YNM_8GB)
		for (i = 0; i < controller->chip_num; i++) {
			if (aml_chip->valid_chip[i]) {
				for (j = 0; j < reg_cnt; j++)
					if ((def_value[i][j] < 0x18)
						|| (def_value[i][j] > 0xd8)) {
						pr_info("invalid def value!\n");
						return;
					}
			}
		}

	data_buf = kzalloc(mtd->writesize, GFP_KERNEL);
	if (data_buf == NULL)
		return;

	if (nand_boot_flag)
		addr = (1024 * mtd->writesize / aml_chip->plane_num);
	else
		addr = 0;

	total_blk = 0;
	while (total_blk < RETRY_NAND_BLK_NUM) {
		error = mtd->_block_isbad(mtd, addr);
		if (error) {
			pr_info("%s %d detect bad blk at blk:%zd\n",
				__func__, __LINE__, addr >> phys_erase_shift);
			addr += mtd->erasesize;
			total_blk++;
			continue;
		}

		memset(&erase_info_read, 0, sizeof(struct erase_info));
		erase_info_read.mtd = mtd;
		erase_info_read.addr = addr;
		erase_info_read.len = mtd->erasesize;

		error = mtd->_erase(mtd, &erase_info_read);
		if (error) {
			pr_info("%s %d erase failed at blk:%zd\n",
				__func__, __LINE__, addr >> phys_erase_shift);
			mtd->_block_markbad(mtd, addr);
			addr += mtd->erasesize;
			total_blk++;
			continue;
		}

	/*if(aml_chip->new_nand_info.slc_program_info.enter_enslc_mode)*/
		aml_chip->new_nand_info.slc_program_info.enter_enslc_mode(mtd);

		aml_oob_ops.mode = MTD_OPS_AUTO_OOB;
		aml_oob_ops.len = mtd->writesize;
		aml_oob_ops.ooblen = 4;
		aml_oob_ops.ooboffs = 0;/*fixme! all layout offs is zero*/
		aml_oob_ops.datbuf = data_buf;
		aml_oob_ops.oobbuf = (uint8_t *)oob_buf;
		memcpy(oob_buf, RETRY_NAND_MAGIC, 4);
		memset((unsigned char *)aml_oob_ops.datbuf,
			0x0, mtd->writesize);
		memcpy((unsigned char *)aml_oob_ops.datbuf,
		&aml_chip->new_nand_info.read_rety_info.reg_default_value[0][0],
		MAX_CHIP_NUM * READ_RETRY_REG_NUM);

		for (i = 0; i < controller->chip_num; i++) {
			if (aml_chip->valid_chip[i]) {
				for (j = 0; j < HYNIX_RETRY_CNT; j++)
					memcpy((u8 *)(aml_oob_ops.datbuf +
					MAX_CHIP_NUM * READ_RETRY_REG_NUM +
	j * READ_RETRY_REG_NUM + i * READ_RETRY_REG_NUM * HYNIX_RETRY_CNT),
	&aml_chip->new_nand_info.read_rety_info.reg_offset_value[i][j][0],
					READ_RETRY_REG_NUM);
			}
		}
		for (i = 0; i < RETRY_NAND_COPY_NUM; i++) {
			if (aml_chip->new_nand_info.type != HYNIX_1YNM_8GB)
				error = mtd->_write_oob(mtd,
					addr + page_list[i]*mtd->writesize,
					&aml_oob_ops);
			else
				error = aml_nand_slcprog_1ynm_hynix(mtd,
					data_buf,
					(unsigned char *)oob_buf,
					(addr/mtd->writesize)+i);

			if (error) {
				pr_info("%s %d write failed blk:%zd page:%zd\n",
					__func__, __LINE__,
					addr >> phys_erase_shift,
		(addr + page_list[i] * mtd->writesize) / mtd->writesize);
				continue;
			}
		}
		if (aml_chip->new_nand_info.type == HYNIX_1YNM_8GB)
			error = aml_nand_slcprog_1ynm_hynix(mtd,
				data_buf,
				(unsigned char *)oob_buf,
				(addr/mtd->writesize) + i);
	/*if (aml_chip->new_nand_info.slc_program_info.exit_enslc_mode)*/
		aml_chip->new_nand_info.slc_program_info.exit_enslc_mode(mtd);

		addr += mtd->erasesize;
		total_blk++;
	}

	kfree(data_buf);
}

/*********************TOSHIBA***********************/
uint8_t aml_nand_set_reg_value_toshiba(struct aml_nand_chip *aml_chip,
	uint8_t *buf, uint8_t *addr, int chipnr, int cnt)
{
	struct nand_chip *chip = &aml_chip->chip;
	struct mtd_info *mtd = aml_chip->mtd;
	int j;

	aml_chip->aml_nand_wait_devready(aml_chip, chipnr);

	aml_chip->aml_nand_select_chip(aml_chip, chipnr);

	if (aml_chip->new_nand_info.read_rety_info.cur_cnt[chipnr] == 0) {
		aml_chip->aml_nand_command(aml_chip,
			NAND_CMD_TOSHIBA_PRE_CON1, -1, -1, chipnr);
		NFC_SEND_CMD_IDLE(controller, 2);
		aml_chip->aml_nand_command(aml_chip,
			NAND_CMD_TOSHIBA_PRE_CON2, -1, -1, chipnr);
		NFC_SEND_CMD_IDLE(controller, 2);
	}

	for (j = 0; j < cnt; j++) {
		aml_chip->aml_nand_command(aml_chip,
			NAND_CMD_TOSHIBA_SET_VALUE, -1, -1, chipnr);
		NFC_SEND_CMD_IDLE(controller, 2);
		chip->cmd_ctrl(mtd, addr[j],
			NAND_CTRL_CHANGE | NAND_NCE | NAND_ALE);
		NFC_SEND_CMD_IDLE(controller, 2);
		aml_chip->aml_nand_write_byte(aml_chip, buf[j]);
		NFC_SEND_CMD_IDLE(controller, 2);
	}

	aml_chip->aml_nand_command(aml_chip,
		NAND_CMD_TOSHIBA_BEF_COMMAND1, -1, -1, chipnr);
	NFC_SEND_CMD_IDLE(controller, 2);
	aml_chip->aml_nand_command(aml_chip,
		NAND_CMD_TOSHIBA_BEF_COMMAND2, -1, -1, chipnr);
	NFC_SEND_CMD_IDLE(controller, 2);
	aml_chip->aml_nand_wait_devready(aml_chip, chipnr);

	return 0;
}


void aml_nand_read_retry_handle_toshiba(struct mtd_info *mtd, int chipnr)
{
	/*void*/
}

void aml_nand_read_retry_exit_toshiba(struct mtd_info *mtd, int chipnr)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);

	struct nand_chip *chip = &aml_chip->chip;
	uint8_t buf[5] = {0};
	int j;

	if (aml_chip->new_nand_info.type == TOSHIBA_A19NM) {
		for (j = 0;
		j < aml_chip->new_nand_info.read_rety_info.reg_cnt; j++) {
			aml_chip->aml_nand_command(aml_chip,
				NAND_CMD_TOSHIBA_SET_VALUE, -1, -1, chipnr);
			udelay(1);
			chip->cmd_ctrl(mtd,
			aml_chip->new_nand_info.read_rety_info.reg_addr[j],
			NAND_CTRL_CHANGE | NAND_NCE | NAND_ALE);
			udelay(1);
			aml_chip->aml_nand_write_byte(aml_chip, buf[j]);
		}
	}
	aml_chip->aml_nand_wait_devready(aml_chip, chipnr);
	aml_chip->aml_nand_command(aml_chip,
		NAND_CMD_RESET, -1, -1, chipnr);
	aml_chip->aml_nand_wait_devready(aml_chip, chipnr);

	memset(&aml_chip->new_nand_info.read_rety_info.cur_cnt[0],
		0, MAX_CHIP_NUM);
}


/*******************************************SUMSUNG***********************/
uint8_t aml_nand_set_reg_value_samsung(struct aml_nand_chip *aml_chip,
	uint8_t *buf, uint8_t *addr, int chipnr, int cnt)
{
	struct nand_chip *chip = &aml_chip->chip;
	struct mtd_info *mtd = aml_chip->mtd;
	int j;

	if (aml_chip->new_nand_info.type != SUMSUNG_2XNM)
		return 0;

	aml_chip->aml_nand_wait_devready(aml_chip, chipnr);

	aml_chip->aml_nand_select_chip(aml_chip, chipnr);

	for (j = 0; j < cnt; j++) {
		aml_chip->aml_nand_command(aml_chip,
			NAND_CMD_SAMSUNG_SET_VALUE, -1, -1, chipnr);
		chip->cmd_ctrl(mtd, 0, NAND_CTRL_CHANGE | NAND_NCE | NAND_ALE);
		chip->cmd_ctrl(mtd, addr[j],
			NAND_CTRL_CHANGE | NAND_NCE | NAND_ALE);
		aml_chip->aml_nand_write_byte(aml_chip, buf[j]);
		NFC_SEND_CMD_IDLE(controller, 20);
	}

	aml_chip->aml_nand_wait_devready(aml_chip, chipnr);
	return 0;
}

void aml_nand_read_retry_handle_samsung(struct mtd_info *mtd, int chipnr)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	int cur_cnt;

	if (aml_chip->new_nand_info.type != SUMSUNG_2XNM)
		return;

	cur_cnt = aml_chip->new_nand_info.read_rety_info.cur_cnt[chipnr];

	aml_nand_set_reg_value_samsung(aml_chip,
(u8 *)&aml_chip->new_nand_info.read_rety_info.reg_offset_value[0][cur_cnt][0],
		&aml_chip->new_nand_info.read_rety_info.reg_addr[0],
		chipnr,
		aml_chip->new_nand_info.read_rety_info.reg_cnt);

	cur_cnt++;
	aml_chip->new_nand_info.read_rety_info.cur_cnt[chipnr] =
(cur_cnt > (aml_chip->new_nand_info.read_rety_info.retry_cnt-1)) ? 0 : cur_cnt;
}

void aml_nand_read_retry_exit_samsung(struct mtd_info *mtd, int chipnr)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);

	if (aml_chip->new_nand_info.type != SUMSUNG_2XNM)
		return;

	pr_info("samsung retry cnt :%d\n",
		aml_chip->new_nand_info.read_rety_info.cur_cnt[chipnr]);
	aml_nand_set_reg_value_samsung(aml_chip,
(uint8_t *)&aml_chip->new_nand_info.read_rety_info.reg_offset_value[0][0][0],
		&aml_chip->new_nand_info.read_rety_info.reg_addr[0],
		chipnr,
		aml_chip->new_nand_info.read_rety_info.reg_cnt);
	memset(&aml_chip->new_nand_info.read_rety_info.cur_cnt[0],
		0, MAX_CHIP_NUM);
}

/*
 *void aml_nand_set_reg_default_hynix(void)
 *{
 *	struct mtd_info *mtd = (struct mtd_info *)&nand_info[nand_curr_device];
 *
 *	if (!strcmp(mtd->name, NAND_BOOT_NAME)) {
 *		aml_nand_set_readretry_default_value_hynix(mtd);
 *		aml_nand_exit_enslc_mode_hynix(mtd);
 *	}
 *}
 */

/***********************************MICRON************************************/

uint8_t aml_nand_set_reg_value_micron(struct aml_nand_chip *aml_chip,
	uint8_t *buf, uint8_t *addr, int chipnr, int cnt)
{
	struct nand_chip *chip = &aml_chip->chip;
	struct mtd_info *mtd = aml_chip->mtd;
	int i;

	if (aml_chip->new_nand_info.type != MICRON_20NM)
		return 0;

	aml_chip->aml_nand_wait_devready(aml_chip, chipnr);

	aml_chip->aml_nand_select_chip(aml_chip, chipnr);

	aml_chip->aml_nand_command(aml_chip,
		NAND_CMD_MICRON_SET_VALUE, -1, -1, chipnr);

	chip->cmd_ctrl(mtd, addr[0],
		NAND_CTRL_CHANGE | NAND_NCE | NAND_ALE);
	NFC_SEND_CMD_IDLE(controller, 10);
	aml_chip->aml_nand_write_byte(aml_chip, buf[0]);
	for (i = 0; i < 3; i++) {
		NFC_SEND_CMD_IDLE(controller, 1);
		aml_chip->aml_nand_write_byte(aml_chip, 0x0);
	}

	aml_chip->aml_nand_wait_devready(aml_chip, chipnr);

	return 0;
}

void aml_nand_read_retry_handle_micron(struct mtd_info *mtd, int chipnr)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	int cur_cnt;

	if (aml_chip->new_nand_info.type != MICRON_20NM)
		return;

	cur_cnt = aml_chip->new_nand_info.read_rety_info.cur_cnt[chipnr];
	aml_nand_set_reg_value_micron(aml_chip,
(u8 *)&aml_chip->new_nand_info.read_rety_info.reg_offset_value[0][cur_cnt][0],
		&aml_chip->new_nand_info.read_rety_info.reg_addr[0],
		chipnr,
		aml_chip->new_nand_info.read_rety_info.reg_cnt);

	cur_cnt++;
	aml_chip->new_nand_info.read_rety_info.cur_cnt[chipnr] =
(cur_cnt > (aml_chip->new_nand_info.read_rety_info.retry_cnt-1)) ? 0 : cur_cnt;
}

void aml_nand_read_retry_exit_micron(struct mtd_info *mtd, int chipnr)
{

	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	int default_val = 0;

	if (aml_chip->new_nand_info.type != MICRON_20NM)
		return;

	pr_info("micron retry cnt :%d\n",
		aml_chip->new_nand_info.read_rety_info.cur_cnt[chipnr]);
	aml_nand_set_reg_value_micron(aml_chip, (uint8_t *)&default_val,
		&aml_chip->new_nand_info.read_rety_info.reg_addr[0],
		chipnr, aml_chip->new_nand_info.read_rety_info.reg_cnt);
	memset(&aml_chip->new_nand_info.read_rety_info.cur_cnt[0],
		0, MAX_CHIP_NUM);
}

/**************************INTEL***************************************/
void aml_nand_read_retry_handle_intel(struct mtd_info *mtd, int chipnr)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	int cur_cnt;
	int advance = 1;

	cur_cnt = aml_chip->new_nand_info.read_rety_info.cur_cnt[chipnr];
	pr_info("intel NAND set partmeters here and read_retry_cnt:%d\n",
		cur_cnt);
	if (cur_cnt == 3)
		aml_nand_set_reg_value_micron(aml_chip, (uint8_t *)&advance,
			&aml_chip->new_nand_info.read_rety_info.reg_addr[1],
			chipnr,
			aml_chip->new_nand_info.read_rety_info.reg_cnt);

	aml_nand_set_reg_value_micron(aml_chip,
(u8 *)&aml_chip->new_nand_info.read_rety_info.reg_offset_value[0][cur_cnt][0],
		&aml_chip->new_nand_info.read_rety_info.reg_addr[0],
		chipnr,
		aml_chip->new_nand_info.read_rety_info.reg_cnt);
	udelay(10);

	cur_cnt++;
	aml_chip->new_nand_info.read_rety_info.cur_cnt[chipnr] =
(cur_cnt > (aml_chip->new_nand_info.read_rety_info.retry_cnt-1)) ? 0 : cur_cnt;
}

void aml_nand_read_retry_exit_intel(struct mtd_info *mtd, int chipnr)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	int default_val = 0;

	aml_nand_set_reg_value_micron(aml_chip, (uint8_t *)&default_val,
		&aml_chip->new_nand_info.read_rety_info.reg_addr[0],
		chipnr, aml_chip->new_nand_info.read_rety_info.reg_cnt);
	aml_nand_set_reg_value_micron(aml_chip, (uint8_t *)&default_val,
		&aml_chip->new_nand_info.read_rety_info.reg_addr[1],
		chipnr, aml_chip->new_nand_info.read_rety_info.reg_cnt);
	memset(&aml_chip->new_nand_info.read_rety_info.cur_cnt[0],
		0, MAX_CHIP_NUM);
}
/***********************************SANDISK************************************/

uint8_t aml_nand_dynamic_read_init_start(struct aml_nand_chip *aml_chip,
	int chipnr)
{
	struct nand_chip *chip = &aml_chip->chip;
	struct mtd_info *mtd = aml_chip->mtd;
	uint8_t *buf;
	int i = 0;

	buf = &aml_chip->new_nand_info.dynamic_read_info.reg_addr_init[0];

	aml_chip->aml_nand_command(aml_chip, NAND_CMD_SANDISK_INIT_ONE,
		-1, -1, chipnr);

	aml_chip->aml_nand_command(aml_chip,
		NAND_CMD_SANDISK_INIT_TWO, -1, -1, chipnr);

	for (i = 0; i < DYNAMIC_REG_INIT_NUM; i++) {
		aml_chip->aml_nand_command(aml_chip,
			NAND_CMD_SANDISK_LOAD_VALUE_ONE, -1, -1, chipnr);
		NFC_SEND_CMD_IDLE(controller, 10);
		chip->cmd_ctrl(mtd, buf[i],
			NAND_CTRL_CHANGE | NAND_NCE | NAND_ALE);
		NFC_SEND_CMD_IDLE(controller, 10);
		aml_chip->aml_nand_write_byte(aml_chip, 0x0);
		NFC_SEND_CMD_IDLE(controller, 10);
	}
	return 0;
}

void aml_nand_dynamic_read_init(struct mtd_info *mtd)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	int i;

	if (aml_chip->new_nand_info.type < SANDISK_19NM)
		return;

	aml_chip->new_nand_info.dynamic_read_info.dynamic_read_flag = 1;

	for (i = 0; i < controller->chip_num; i++) {
		if (aml_chip->valid_chip[i]) {
			if ((aml_chip->new_nand_info.type == SANDISK_19NM)
			|| (aml_chip->new_nand_info.type == SANDISK_24NM))
				aml_nand_dynamic_read_init_start(aml_chip, i);
		}
	}
}

u8 aml_nand_dynamic_read_load_register_value(struct aml_nand_chip *aml_chip,
	uint8_t *buf, uint8_t *addr, int chipnr, int cnt)
{
	struct nand_chip *chip = &aml_chip->chip;
	struct mtd_info *mtd = aml_chip->mtd;
	int j;

	if (aml_chip->new_nand_info.type < SANDISK_19NM)
		return 0;

	aml_chip->aml_nand_wait_devready(aml_chip, chipnr);

	aml_chip->aml_nand_select_chip(aml_chip, chipnr);

	aml_chip->aml_nand_command(aml_chip,
		NAND_CMD_SANDISK_INIT_ONE, -1, -1, chipnr);
	aml_chip->aml_nand_command(aml_chip,
		NAND_CMD_SANDISK_INIT_TWO, -1, -1, chipnr);

	for (j = 0; j < cnt; j++) {
		aml_chip->aml_nand_command(aml_chip,
			NAND_CMD_SANDISK_LOAD_VALUE_ONE, -1, -1, chipnr);
		NFC_SEND_CMD_IDLE(controller, 10);
		chip->cmd_ctrl(mtd, addr[j],
			NAND_CTRL_CHANGE | NAND_NCE | NAND_ALE);
		NFC_SEND_CMD_IDLE(controller, 10);
		aml_chip->aml_nand_write_byte(aml_chip, buf[j]);
		NFC_SEND_CMD_IDLE(controller, 10);
	}
	aml_chip->aml_nand_wait_devready(aml_chip, chipnr);
	return 0;
}

void aml_nand_dynamic_read_handle(struct mtd_info *mtd, int page, int chipnr)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	struct nand_chip *chip = &aml_chip->chip;
	u8 dynamic_reg_read_value[DYNAMIC_REG_NUM];
	u8 dynamic_reg_addr_value[DYNAMIC_REG_NUM];
	int cur_lower_page, cur_upper_page, i;
	int pages_per_blk;
	struct new_tech_nand_t *new_nand_info;

	new_nand_info = &aml_chip->new_nand_info;
	if (new_nand_info->type != SANDISK_19NM)
		return;

	cur_upper_page =
	new_nand_info->dynamic_read_info.cur_case_num_upper_page[chipnr];
	cur_lower_page =
	new_nand_info->dynamic_read_info.cur_case_num_lower_page[chipnr];

	pages_per_blk = (1 << (chip->phys_erase_shift - chip->page_shift));
	if (((page != 0) && (page % 2) == 0) || (page == (pages_per_blk - 1))) {
		memset(&dynamic_reg_read_value[0], 0, DYNAMIC_REG_NUM);
		memset(&dynamic_reg_addr_value[0], 0, DYNAMIC_REG_NUM);
		for (i = 0; i < DYNAMIC_REG_NUM; i++) {
			dynamic_reg_read_value[i] =
new_nand_info->dynamic_read_info.reg_offset_value_upper_page[cur_upper_page][i];
			dynamic_reg_addr_value[i] =
			new_nand_info->dynamic_read_info.reg_addr_upper_page[i];

		}

		aml_nand_dynamic_read_load_register_value(aml_chip,
			&dynamic_reg_read_value[0],
			&dynamic_reg_addr_value[0], chipnr, DYNAMIC_REG_NUM);
		udelay(2);

		cur_upper_page++;
	new_nand_info->dynamic_read_info.cur_case_num_upper_page[chipnr] =
		(cur_upper_page > DYNAMIC_READ_CNT_UPPER) ? 0 : cur_upper_page;

		/* enable dynamic read */
		aml_chip->aml_nand_command(aml_chip,
			NAND_CMD_SANDISK_DYNAMIC_ENABLE, -1, -1, chipnr);
		udelay(1);
	} else {
		memset(&dynamic_reg_read_value[0], 0, DYNAMIC_REG_NUM);
		memset(&dynamic_reg_addr_value[0], 0, DYNAMIC_REG_NUM);
		for (i = 0; i < DYNAMIC_REG_NUM; i++) {
			dynamic_reg_read_value[i] =
new_nand_info->dynamic_read_info.reg_offset_value_lower_page[cur_lower_page][i];
			dynamic_reg_addr_value[i] =
			new_nand_info->dynamic_read_info.reg_addr_lower_page[i];
		}

		aml_nand_dynamic_read_load_register_value(aml_chip,
			&dynamic_reg_read_value[0],
			&dynamic_reg_addr_value[0],
			chipnr, DYNAMIC_REG_NUM);

		cur_lower_page++;
	new_nand_info->dynamic_read_info.cur_case_num_lower_page[chipnr] =
		(cur_lower_page > DYNAMIC_READ_CNT_LOWER) ? 0 : cur_lower_page;

		/* enable dynamic read */
		aml_chip->aml_nand_command(aml_chip,
			NAND_CMD_SANDISK_DYNAMIC_ENABLE, -1, -1, chipnr);
	}
}


void aml_nand_read_retry_handle_sandisk(struct mtd_info *mtd, int chipnr)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	int cur_cnt;
	struct new_tech_nand_t *new_nand_info;

	new_nand_info = &aml_chip->new_nand_info;
	cur_cnt = new_nand_info->read_rety_info.cur_cnt[chipnr];

	aml_nand_dynamic_read_load_register_value(aml_chip,
(uint8_t *)&new_nand_info->read_rety_info.reg_offset_value[0][cur_cnt][0],
		&new_nand_info->read_rety_info.reg_addr[0],
		chipnr,
		new_nand_info->read_rety_info.reg_cnt);
	cur_cnt++;
	new_nand_info->read_rety_info.cur_cnt[chipnr] =
	(cur_cnt > (new_nand_info->read_rety_info.retry_cnt-1)) ? 0 : cur_cnt;
}

void aml_nand_dynamic_read_exit(struct mtd_info *mtd, int chipnr)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);

	if (aml_chip->new_nand_info.type < SANDISK_19NM)
		return;

	aml_chip->aml_nand_wait_devready(aml_chip, chipnr);
	aml_chip->aml_nand_command(aml_chip,
		NAND_CMD_SANDISK_DYNAMIC_DISABLE, -1, -1, chipnr);
	aml_chip->aml_nand_wait_devready(aml_chip, chipnr);

	aml_nand_dynamic_read_init(mtd);

	aml_chip->aml_nand_wait_devready(aml_chip, chipnr);
	aml_chip->aml_nand_command(aml_chip, NAND_CMD_RESET, -1, -1, chipnr);
	aml_chip->aml_nand_wait_devready(aml_chip, chipnr);

	memset(&aml_chip->new_nand_info.read_rety_info.cur_cnt[0],
		0, MAX_CHIP_NUM);
}

uint8_t aml_nand_set_featureReg_value_sandisk(struct aml_nand_chip *aml_chip,
	uint8_t *buf, uint8_t addr, int chipnr, int cnt)
{
	struct nand_chip *chip = &aml_chip->chip;
	struct mtd_info *mtd = aml_chip->mtd;
	int j;

	chip->select_chip(mtd, chipnr);

	aml_chip->aml_nand_wait_devready(aml_chip, chipnr);

	udelay(1);

	chip->cmd_ctrl(mtd, NAND_CMD_SANDISK_SET_VALUE,
		NAND_CTRL_CHANGE | NAND_NCE | NAND_CLE);

	udelay(1);
	chip->cmd_ctrl(mtd, addr, NAND_CTRL_CHANGE | NAND_NCE | NAND_ALE);

	for (j = 0; j < cnt; j++) {
		ndelay(200);
		aml_chip->aml_nand_write_byte(aml_chip, buf[j]);
	}
	udelay(10);
	aml_chip->aml_nand_wait_devready(aml_chip, chipnr);

	return 0;
}

void aml_nand_read_retry_handleA19_sandisk(struct mtd_info *mtd, int chipnr)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	struct nand_chip *chip = &aml_chip->chip;
	int cur_cnt;
	unsigned int page = aml_chip->page_addr;
	int pages_per_blk;
	int page_info = 1;
	struct new_tech_nand_t *new_nand_info;

	new_nand_info = &aml_chip->new_nand_info;
	pages_per_blk = (1 << (chip->phys_erase_shift - chip->page_shift));
	page = page % pages_per_blk;

	if (((page != 0) && (page % 2) == 0) || (page == (pages_per_blk - 1)))
		page_info =  0;

	cur_cnt = new_nand_info->read_rety_info.cur_cnt[chipnr];

	aml_nand_set_featureReg_value_sandisk(aml_chip,
(u8 *)(&new_nand_info->read_rety_info.reg_offset_value[page_info][cur_cnt][0]),
	(uint8_t)(unsigned int)(new_nand_info->read_rety_info.reg_addr[0]),
	chipnr, new_nand_info->read_rety_info.reg_cnt);

	aml_chip->aml_nand_command(aml_chip,
		NAND_CMD_SANDISK_DSP_OFF, -1, -1, chipnr);

	if (new_nand_info->type == SANDISK_A19NM)
		aml_chip->aml_nand_command(aml_chip,
			NAND_CMD_SANDISK_DSP_ON, -1, -1, chipnr);

	aml_chip->aml_nand_command(aml_chip,
		NAND_CMD_SANDISK_RETRY_STA, -1, -1, chipnr);

	cur_cnt++;
	new_nand_info->read_rety_info.cur_cnt[chipnr] =
	(cur_cnt > (new_nand_info->read_rety_info.retry_cnt-1)) ? 0 : cur_cnt;
}

void aml_nand_read_retry_exit_A19_sandisk(struct mtd_info *mtd, int chipnr)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	uint8_t buf[4] = {0};

	aml_nand_set_featureReg_value_sandisk(aml_chip, buf,
(uint8_t)(unsigned int)(aml_chip->new_nand_info.read_rety_info.reg_addr[0]),
		chipnr,
		aml_chip->new_nand_info.read_rety_info.reg_cnt);

	aml_chip->aml_nand_wait_devready(aml_chip, chipnr);
	aml_chip->aml_nand_command(aml_chip, NAND_CMD_RESET, -1, -1, chipnr);
	aml_chip->aml_nand_wait_devready(aml_chip, chipnr);

	memset(&aml_chip->new_nand_info.read_rety_info.cur_cnt[0],
		0, MAX_CHIP_NUM);
}

void aml_nand_enter_slc_mode_sandisk(struct mtd_info *mtd)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);

	aml_chip->new_nand_info.dynamic_read_info.slc_flag = 1;
}

void aml_nand_exit_slc_mode_sandisk(struct mtd_info *mtd)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);

	aml_chip->new_nand_info.dynamic_read_info.slc_flag = 0;
}

uint8_t aml_nand_set_featureReg_value_toshiba(struct aml_nand_chip *aml_chip,
	uint8_t *buf, uint8_t addr, int chipnr, int cnt)
{
	struct nand_chip *chip = &aml_chip->chip;
	struct mtd_info *mtd = aml_chip->mtd;
	int j;

	chip->select_chip(mtd, chipnr);

	aml_chip->aml_nand_wait_devready(aml_chip, chipnr);

	chip->cmd_ctrl(mtd, NAND_CMD_SANDISK_SET_VALUE,
		NAND_CTRL_CHANGE | NAND_NCE | NAND_CLE);

	NFC_SEND_CMD_IDLE(controller, 10);
	chip->cmd_ctrl(mtd, addr, NAND_CTRL_CHANGE | NAND_NCE | NAND_ALE);

	for (j = 0; j < cnt; j++) {
		NFC_SEND_CMD_IDLE(controller, 10);
		aml_chip->aml_nand_write_byte(aml_chip, buf[j]);
	}
	aml_chip->aml_nand_wait_devready(aml_chip, chipnr);
	return 0;
}

void aml_nand_read_set_flash_feature_toshiba(struct mtd_info *mtd, int chipnr)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	uint8_t reg10h[4] = {4, 0, 0, 0};
	uint8_t reg80h[4] = {0, 0, 0, 0};

	aml_nand_set_featureReg_value_toshiba(aml_chip,
		reg10h, NAND_CMD_SANDISK_SET_OUTPUT_DRV, chipnr, 4);

	aml_nand_set_featureReg_value_toshiba(aml_chip,
		reg80h, NAND_CMD_SANDISK_SET_VENDOR_SPC, chipnr, 4);
	pr_info("set flash toggle mode end\n");
}



void aml_nand_set_toggle_mode_toshiba(struct mtd_info *mtd, int chipnr)
{
	/*void*/
}

void aml_nand_debug_toggle_flash(struct mtd_info *mtd, int chipnr)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);

	if (aml_chip->mfr_type == NAND_MFR_TOSHIBA)
		aml_nand_set_toggle_mode_toshiba(mtd, 0);

}

void aml_nand_new_nand_param_init(struct mtd_info *mtd,
	struct aml_nand_flash_dev *type)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	struct new_tech_nand_t *nand_info;
	struct aml_nand_dynamic_read *nand_dynamic_read;
	struct new_tech_nand_t *new_nand_info = &aml_chip->new_nand_info;

	aml_chip->support_new_nand = 0;
	nand_dynamic_read = &new_nand_info->dynamic_read_info;
	nand_info = &aml_chip->new_nand_info;
	memset(nand_info, 0, sizeof(struct new_tech_nand_t));
	switch (type->new_type) {
	case HYNIX_26NM_8GB:
		nand_info->type = HYNIX_26NM_8GB;
		aml_chip->ran_mode = 1;
		aml_chip->support_new_nand = 1;
		pr_info("aml_chip->hynix_new_nand_type =: %d\n",
			nand_info->type);
		/* read retry */
		nand_info->read_rety_info.reg_cnt = 4;
		nand_info->read_rety_info.retry_cnt = 6;

		nand_info->read_rety_info.reg_addr[0] = 0xAC;
		nand_info->read_rety_info.reg_addr[1] = 0xAD;
		nand_info->read_rety_info.reg_addr[2] = 0xAE;
		nand_info->read_rety_info.reg_addr[3] = 0xAF;

		nand_info->read_rety_info.reg_offset_value[0][0][0] = 0;
		nand_info->read_rety_info.reg_offset_value[0][0][1] = 0x06;
		nand_info->read_rety_info.reg_offset_value[0][0][2] = 0x0A;
		nand_info->read_rety_info.reg_offset_value[0][0][3] = 0x06;

		nand_info->read_rety_info.reg_offset_value[0][1][0] =
			READ_RETRY_ZERO;
		nand_info->read_rety_info.reg_offset_value[0][1][1] = -0x03;
		nand_info->read_rety_info.reg_offset_value[0][1][2] = -0x07;
		nand_info->read_rety_info.reg_offset_value[0][1][3] = -0x08;

		nand_info->read_rety_info.reg_offset_value[0][2][0] =
			READ_RETRY_ZERO;
		nand_info->read_rety_info.reg_offset_value[0][2][1] = -0x06;
		nand_info->read_rety_info.reg_offset_value[0][2][2] = -0x0D;
		nand_info->read_rety_info.reg_offset_value[0][2][3] = -0x0F;

		nand_info->read_rety_info.reg_offset_value[0][3][0] =
			READ_RETRY_ZERO;
		nand_info->read_rety_info.reg_offset_value[0][3][1] = -0x0B;
		nand_info->read_rety_info.reg_offset_value[0][3][2] = -0x14;
		nand_info->read_rety_info.reg_offset_value[0][3][3] = -0x17;

		nand_info->read_rety_info.reg_offset_value[0][4][0] =
			READ_RETRY_ZERO;
		nand_info->read_rety_info.reg_offset_value[0][4][1] =
			READ_RETRY_ZERO;
		nand_info->read_rety_info.reg_offset_value[0][4][2] = -0x1A;
		nand_info->read_rety_info.reg_offset_value[0][4][3] = -0x1E;

		nand_info->read_rety_info.reg_offset_value[0][5][0] =
			READ_RETRY_ZERO;
		nand_info->read_rety_info.reg_offset_value[0][5][1] =
			READ_RETRY_ZERO;
		nand_info->read_rety_info.reg_offset_value[0][5][2] = -0x20;
		nand_info->read_rety_info.reg_offset_value[0][5][3] = -0x25;

		nand_info->slc_program_info.reg_cnt = 5;

		nand_info->slc_program_info.reg_addr[0] = 0xA4;
		nand_info->slc_program_info.reg_addr[1] = 0xA5;
		nand_info->slc_program_info.reg_addr[2] = 0xB0;
		nand_info->slc_program_info.reg_addr[3] = 0xB1;
		nand_info->slc_program_info.reg_addr[4] = 0xC9;

		nand_info->slc_program_info.reg_offset_value[0] = 0x25;
		nand_info->slc_program_info.reg_offset_value[1] = 0x25;
		nand_info->slc_program_info.reg_offset_value[2] = 0x25;
		nand_info->slc_program_info.reg_offset_value[3] = 0x25;
		nand_info->slc_program_info.reg_offset_value[4] = 0x01;

		nand_info->read_rety_info.get_default_value =
			aml_nand_get_read_default_value_hynix;
		nand_info->read_rety_info.save_default_value =
			aml_nand_save_read_default_value_hynix;
		nand_info->read_rety_info.read_retry_handle =
			aml_nand_read_retry_handle_hynix;

		nand_info->slc_program_info.enter_enslc_mode =
			aml_nand_enter_enslc_mode_hynix;
		nand_info->slc_program_info.exit_enslc_mode =
			aml_nand_exit_enslc_mode_hynix;
		nand_info->slc_program_info.get_default_value =
			aml_nand_get_slc_default_value_hynix;
		break;

	case HYNIX_26NM_4GB:
		nand_info->type = HYNIX_26NM_4GB;
		aml_chip->support_new_nand = 1;
		aml_chip->ran_mode = 1;
		pr_info("aml_chip->hynix_new_nand_type =: %d\n",
			nand_info->type);
				/* read retry */
		nand_info->read_rety_info.reg_cnt = 4;
		nand_info->read_rety_info.retry_cnt = 6;

		nand_info->read_rety_info.reg_addr[0] = 0xA7;
		nand_info->read_rety_info.reg_addr[1] = 0xAD;
		nand_info->read_rety_info.reg_addr[2] = 0xAE;
		nand_info->read_rety_info.reg_addr[3] = 0xAF;

		nand_info->read_rety_info.reg_offset_value[0][0][0] = 0;
		nand_info->read_rety_info.reg_offset_value[0][0][1] = 0x06;
		nand_info->read_rety_info.reg_offset_value[0][0][2] = 0x0A;
		nand_info->read_rety_info.reg_offset_value[0][0][3] = 0x06;

		nand_info->read_rety_info.reg_offset_value[0][1][0] =
			READ_RETRY_ZERO;
		nand_info->read_rety_info.reg_offset_value[0][1][1] = -0x03;
		nand_info->read_rety_info.reg_offset_value[0][1][2] = -0x07;
		nand_info->read_rety_info.reg_offset_value[0][1][3] = -0x08;

		nand_info->read_rety_info.reg_offset_value[0][2][0] =
			READ_RETRY_ZERO;
		nand_info->read_rety_info.reg_offset_value[0][2][1] = -0x06;
		nand_info->read_rety_info.reg_offset_value[0][2][2] = -0x0D;
		nand_info->read_rety_info.reg_offset_value[0][2][3] = -0x0F;

		nand_info->read_rety_info.reg_offset_value[0][3][0] =
			READ_RETRY_ZERO;
		nand_info->read_rety_info.reg_offset_value[0][3][1] = -0x09;
		nand_info->read_rety_info.reg_offset_value[0][3][2] = -0x14;
		nand_info->read_rety_info.reg_offset_value[0][3][3] = -0x17;

		nand_info->read_rety_info.reg_offset_value[0][4][0] =
			READ_RETRY_ZERO;
		nand_info->read_rety_info.reg_offset_value[0][4][1] =
			READ_RETRY_ZERO;
		nand_info->read_rety_info.reg_offset_value[0][4][2] = -0x1A;
		nand_info->read_rety_info.reg_offset_value[0][4][3] = -0x1E;

		nand_info->read_rety_info.reg_offset_value[0][5][0] =
			READ_RETRY_ZERO;
		nand_info->read_rety_info.reg_offset_value[0][5][1] =
			READ_RETRY_ZERO;
		nand_info->read_rety_info.reg_offset_value[0][5][2] = -0x20;
		nand_info->read_rety_info.reg_offset_value[0][5][3] = -0x25;

		nand_info->slc_program_info.reg_cnt = 5;

		nand_info->slc_program_info.reg_addr[0] = 0xA0;
		nand_info->slc_program_info.reg_addr[1] = 0xA1;
		nand_info->slc_program_info.reg_addr[2] = 0xB0;
		nand_info->slc_program_info.reg_addr[3] = 0xB1;
		nand_info->slc_program_info.reg_addr[4] = 0xC9;

		nand_info->slc_program_info.reg_offset_value[0] = 0x26;
		nand_info->slc_program_info.reg_offset_value[1] = 0x26;
		nand_info->slc_program_info.reg_offset_value[2] = 0x26;
		nand_info->slc_program_info.reg_offset_value[3] = 0x26;
		nand_info->slc_program_info.reg_offset_value[4] = 0x01;

		nand_info->read_rety_info.get_default_value =
			aml_nand_get_read_default_value_hynix;
		nand_info->read_rety_info.save_default_value =
			aml_nand_save_read_default_value_hynix;
		nand_info->read_rety_info.read_retry_handle =
			aml_nand_read_retry_handle_hynix;

		nand_info->slc_program_info.enter_enslc_mode =
			aml_nand_enter_enslc_mode_hynix;
		nand_info->slc_program_info.exit_enslc_mode =
			aml_nand_exit_enslc_mode_hynix;
		nand_info->slc_program_info.get_default_value =
			aml_nand_get_slc_default_value_hynix;
		break;

	case HYNIX_20NM_8GB:
		nand_info->type = HYNIX_20NM_8GB;
		aml_chip->support_new_nand = 1;
		aml_chip->ran_mode = 1;
		pr_info("aml_chip->hynix_new_nand_type =: %d\n",
			nand_info->type);
		/* read retry */
		nand_info->read_rety_info.reg_cnt = 8;
		nand_info->read_rety_info.retry_cnt = 7;

		nand_info->read_rety_info.reg_addr[0] = 0xCC;
		nand_info->read_rety_info.reg_addr[1] = 0xBF;
		nand_info->read_rety_info.reg_addr[2] = 0xAA;
		nand_info->read_rety_info.reg_addr[3] = 0xAB;
		nand_info->read_rety_info.reg_addr[4] = 0xCD;
		nand_info->read_rety_info.reg_addr[5] = 0xAD;
		nand_info->read_rety_info.reg_addr[6] = 0xAE;
		nand_info->read_rety_info.reg_addr[7] = 0xAF;

		nand_info->slc_program_info.reg_cnt = 4;

		nand_info->slc_program_info.reg_addr[0] = 0xB0;
		nand_info->slc_program_info.reg_addr[1] = 0xB1;
		nand_info->slc_program_info.reg_addr[2] = 0xA0;
		nand_info->slc_program_info.reg_addr[3] = 0xA1;

		nand_info->slc_program_info.reg_offset_value[0] = 0x0A;
		nand_info->slc_program_info.reg_offset_value[1] = 0x0A;
		nand_info->slc_program_info.reg_offset_value[2] = 0x0A;
		nand_info->slc_program_info.reg_offset_value[3] = 0x0A;


		nand_info->read_rety_info.get_default_value =
			aml_nand_get_read_default_value_hynix;
		nand_info->read_rety_info.save_default_value =
			aml_nand_save_read_default_value_hynix;
		nand_info->read_rety_info.read_retry_handle =
			aml_nand_read_retry_handle_hynix;
		nand_info->read_rety_info.read_retry_exit =
			aml_nand_read_retry_exit_hynix;

		nand_info->slc_program_info.enter_enslc_mode =
			aml_nand_enter_enslc_mode_hynix;
		nand_info->slc_program_info.exit_enslc_mode =
			aml_nand_exit_enslc_mode_hynix;
		nand_info->slc_program_info.get_default_value =
			aml_nand_get_slc_default_value_hynix;
		break;

	case HYNIX_20NM_4GB:
		nand_info->type = HYNIX_20NM_4GB;
		aml_chip->support_new_nand = 1;
		aml_chip->ran_mode = 1;
		pr_info("aml_chip->hynix_new_nand_type =: %d\n",
			nand_info->type);

		/* read retry */
		nand_info->read_rety_info.reg_cnt = 8;
		nand_info->read_rety_info.retry_cnt = 7;

		nand_info->read_rety_info.reg_addr[0] = 0xB0;
		nand_info->read_rety_info.reg_addr[1] = 0xB1;
		nand_info->read_rety_info.reg_addr[2] = 0xB2;
		nand_info->read_rety_info.reg_addr[3] = 0xB3;
		nand_info->read_rety_info.reg_addr[4] = 0xB4;
		nand_info->read_rety_info.reg_addr[5] = 0xB5;
		nand_info->read_rety_info.reg_addr[6] = 0xB6;
		nand_info->read_rety_info.reg_addr[7] = 0xB7;

		nand_info->slc_program_info.reg_cnt = 4;

		nand_info->slc_program_info.reg_addr[0] = 0xA0;
		nand_info->slc_program_info.reg_addr[1] = 0xA1;
		nand_info->slc_program_info.reg_addr[2] = 0xA7;
		nand_info->slc_program_info.reg_addr[3] = 0xA8;

		nand_info->slc_program_info.reg_offset_value[0] = 0x0A;
		nand_info->slc_program_info.reg_offset_value[1] = 0x0A;
		nand_info->slc_program_info.reg_offset_value[2] = 0x0A;
		nand_info->slc_program_info.reg_offset_value[3] = 0x0A;


		nand_info->read_rety_info.get_default_value =
			aml_nand_get_read_default_value_hynix;
		nand_info->read_rety_info.save_default_value =
			aml_nand_save_read_default_value_hynix;
		nand_info->read_rety_info.read_retry_handle =
			aml_nand_read_retry_handle_hynix;
		nand_info->read_rety_info.read_retry_exit =
			aml_nand_read_retry_exit_hynix;

		nand_info->slc_program_info.enter_enslc_mode =
			aml_nand_enter_enslc_mode_hynix;
		nand_info->slc_program_info.exit_enslc_mode =
			aml_nand_exit_enslc_mode_hynix;
		nand_info->slc_program_info.get_default_value =
			aml_nand_get_slc_default_value_hynix;
		break;

	case HYNIX_1YNM_8GB:
		nand_info->type = HYNIX_1YNM_8GB;
		aml_chip->support_new_nand = 1;
		aml_chip->ran_mode = 1;
		pr_info("aml_chip->hynix_new_nand_type =: %d\n",
			nand_info->type);
		nand_info->read_rety_info.reg_cnt = 4;
		nand_info->read_rety_info.retry_cnt = 7;
		nand_info->read_rety_info.reg_addr[0] = 0x38;
		nand_info->read_rety_info.reg_addr[1] = 0x39;
		nand_info->read_rety_info.reg_addr[2] = 0x3a;
		nand_info->read_rety_info.reg_addr[3] = 0x3b;
		nand_info->read_rety_info.get_default_value =
			aml_nand_get_read_default_value_hynix;
		nand_info->read_rety_info.save_default_value =
			aml_nand_save_read_default_value_hynix;
		nand_info->read_rety_info.read_retry_handle =
			aml_nand_read_retry_handle_hynix;
		nand_info->read_rety_info.read_retry_exit =
			aml_nand_read_retry_exit_hynix;
		break;

	case TOSHIBA_24NM:
		nand_info->type =  TOSHIBA_24NM;
		aml_chip->support_new_nand = 1;
		aml_chip->ran_mode = 1;

		nand_info->read_rety_info.reg_addr[0] = 0x04;
		nand_info->read_rety_info.reg_addr[1] = 0x05;
		nand_info->read_rety_info.reg_addr[2] = 0x06;
		nand_info->read_rety_info.reg_addr[3] = 0x07;

		nand_info->read_rety_info.reg_offset_value[0][0][0] = 0;
		nand_info->read_rety_info.reg_offset_value[0][0][1] = 0;
		nand_info->read_rety_info.reg_offset_value[0][0][2] = 0;
		nand_info->read_rety_info.reg_offset_value[0][0][3] = 0;

		nand_info->read_rety_info.reg_offset_value[0][1][0] = 0x04;
		nand_info->read_rety_info.reg_offset_value[0][1][1] = 0x04;
		nand_info->read_rety_info.reg_offset_value[0][1][2] = 0x04;
		nand_info->read_rety_info.reg_offset_value[0][1][3] = 0x04;

		nand_info->read_rety_info.reg_offset_value[0][2][0] = 0x7c;
		nand_info->read_rety_info.reg_offset_value[0][2][1] = 0x7c;
		nand_info->read_rety_info.reg_offset_value[0][2][2] = 0x7c;
		nand_info->read_rety_info.reg_offset_value[0][2][3] = 0x7c;

		nand_info->read_rety_info.reg_offset_value[0][3][0] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][3][1] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][3][2] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][3][3] = 0x78;

		nand_info->read_rety_info.reg_offset_value[0][4][0] = 0x74;
		nand_info->read_rety_info.reg_offset_value[0][4][1] = 0x74;
		nand_info->read_rety_info.reg_offset_value[0][4][2] = 0x74;
		nand_info->read_rety_info.reg_offset_value[0][4][3] = 0x74;

		nand_info->read_rety_info.reg_offset_value[0][5][0] = 0x08;
		nand_info->read_rety_info.reg_offset_value[0][5][1] = 0x08;
		nand_info->read_rety_info.reg_offset_value[0][5][2] = 0x08;
		nand_info->read_rety_info.reg_offset_value[0][5][3] = 0x08;

		nand_info->read_rety_info.reg_cnt = 4;
		nand_info->read_rety_info.retry_cnt = 6;

		nand_info->read_rety_info.read_retry_handle =
			aml_nand_read_retry_handle_toshiba;
		nand_info->read_rety_info.read_retry_exit =
			aml_nand_read_retry_exit_toshiba;
		break;

	case TOSHIBA_A19NM:
		nand_info->type =  TOSHIBA_A19NM;
		aml_chip->support_new_nand = 1;
		aml_chip->ran_mode = 1;

		nand_info->read_rety_info.reg_addr[0] = 0x04;
		nand_info->read_rety_info.reg_addr[1] = 0x05;
		nand_info->read_rety_info.reg_addr[2] = 0x06;
		nand_info->read_rety_info.reg_addr[3] = 0x07;
		nand_info->read_rety_info.reg_addr[4] = 0x0d;

		nand_info->read_rety_info.reg_offset_value[0][0][0] = 0;
		nand_info->read_rety_info.reg_offset_value[0][0][1] = 0;
		nand_info->read_rety_info.reg_offset_value[0][0][2] = 0;
		nand_info->read_rety_info.reg_offset_value[0][0][3] = 0;
		nand_info->read_rety_info.reg_offset_value[0][0][4] = 0;

		nand_info->read_rety_info.reg_offset_value[0][1][0] = 0x04;
		nand_info->read_rety_info.reg_offset_value[0][1][1] = 0x04;
		nand_info->read_rety_info.reg_offset_value[0][1][2] = 0x04;
		nand_info->read_rety_info.reg_offset_value[0][1][3] = 0x04;
		nand_info->read_rety_info.reg_offset_value[0][1][4] = 0x0;

		nand_info->read_rety_info.reg_offset_value[0][2][0] = 0x7c;
		nand_info->read_rety_info.reg_offset_value[0][2][1] = 0x7c;
		nand_info->read_rety_info.reg_offset_value[0][2][2] = 0x7c;
		nand_info->read_rety_info.reg_offset_value[0][2][3] = 0x7c;
		nand_info->read_rety_info.reg_offset_value[0][2][4] = 0x0;

		nand_info->read_rety_info.reg_offset_value[0][3][0] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][3][1] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][3][2] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][3][3] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][3][4] = 0x0;

		nand_info->read_rety_info.reg_offset_value[0][4][0] = 0x74;
		nand_info->read_rety_info.reg_offset_value[0][4][1] = 0x74;
		nand_info->read_rety_info.reg_offset_value[0][4][2] = 0x74;
		nand_info->read_rety_info.reg_offset_value[0][4][3] = 0x74;
		nand_info->read_rety_info.reg_offset_value[0][4][4] = 0x0;

		nand_info->read_rety_info.reg_offset_value[0][5][0] = 0x08;
		nand_info->read_rety_info.reg_offset_value[0][5][1] = 0x08;
		nand_info->read_rety_info.reg_offset_value[0][5][2] = 0x08;
		nand_info->read_rety_info.reg_offset_value[0][5][3] = 0x08;
		nand_info->read_rety_info.reg_offset_value[0][5][4] = 0x0;

		nand_info->read_rety_info.reg_offset_value[0][6][0] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][6][1] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][6][2] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][6][3] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][6][4] = 0x0;

		nand_info->read_rety_info.reg_cnt = 5;
		nand_info->read_rety_info.retry_cnt = 7;

		nand_info->read_rety_info.read_retry_handle =
			aml_nand_read_retry_handle_toshiba;
		nand_info->read_rety_info.read_retry_exit =
			aml_nand_read_retry_exit_toshiba;
		break;

	case SUMSUNG_2XNM:
		nand_info->type =  SUMSUNG_2XNM;
		aml_chip->support_new_nand = 1;
		aml_chip->ran_mode = 1;

		nand_info->read_rety_info.reg_addr[0] = 0xA7;
		nand_info->read_rety_info.reg_addr[1] = 0xA4;
		nand_info->read_rety_info.reg_addr[2] = 0xA5;
		nand_info->read_rety_info.reg_addr[3] = 0xA6;

		nand_info->read_rety_info.reg_offset_value[0][0][0] = 0;
		nand_info->read_rety_info.reg_offset_value[0][0][1] = 0;
		nand_info->read_rety_info.reg_offset_value[0][0][2] = 0;
		nand_info->read_rety_info.reg_offset_value[0][0][3] = 0;

		nand_info->read_rety_info.reg_offset_value[0][1][0] = 0x05;
		nand_info->read_rety_info.reg_offset_value[0][1][1] = 0x0A;
		nand_info->read_rety_info.reg_offset_value[0][1][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][1][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][2][0] = 0x28;
		nand_info->read_rety_info.reg_offset_value[0][2][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][2][2] = 0xEc;
		nand_info->read_rety_info.reg_offset_value[0][2][3] = 0xD8;

		nand_info->read_rety_info.reg_offset_value[0][3][0] = 0xED;
		nand_info->read_rety_info.reg_offset_value[0][3][1] = 0xF5;
		nand_info->read_rety_info.reg_offset_value[0][3][2] = 0xED;
		nand_info->read_rety_info.reg_offset_value[0][3][3] = 0xE6;

		nand_info->read_rety_info.reg_offset_value[0][4][0] = 0x0A;
		nand_info->read_rety_info.reg_offset_value[0][4][1] = 0x0F;
		nand_info->read_rety_info.reg_offset_value[0][4][2] = 0x05;
		nand_info->read_rety_info.reg_offset_value[0][4][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][5][0] = 0x0F;
		nand_info->read_rety_info.reg_offset_value[0][5][1] = 0x0A;
		nand_info->read_rety_info.reg_offset_value[0][5][2] = 0xFB;
		nand_info->read_rety_info.reg_offset_value[0][5][3] = 0xEC;

		nand_info->read_rety_info.reg_offset_value[0][6][0] = 0XE8;
		nand_info->read_rety_info.reg_offset_value[0][6][1] = 0XEF;
		nand_info->read_rety_info.reg_offset_value[0][6][2] = 0XE8;
		nand_info->read_rety_info.reg_offset_value[0][6][3] = 0XDC;

		nand_info->read_rety_info.reg_offset_value[0][7][0] = 0xF1;
		nand_info->read_rety_info.reg_offset_value[0][7][1] = 0xFB;
		nand_info->read_rety_info.reg_offset_value[0][7][2] = 0xFE;
		nand_info->read_rety_info.reg_offset_value[0][7][3] = 0xF0;

		nand_info->read_rety_info.reg_offset_value[0][8][0] = 0x0A;
		nand_info->read_rety_info.reg_offset_value[0][8][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][8][2] = 0xFB;
		nand_info->read_rety_info.reg_offset_value[0][8][3] = 0xEC;

		nand_info->read_rety_info.reg_offset_value[0][9][0] = 0xD0;
		nand_info->read_rety_info.reg_offset_value[0][9][1] = 0xE2;
		nand_info->read_rety_info.reg_offset_value[0][9][2] = 0xD0;
		nand_info->read_rety_info.reg_offset_value[0][9][3] = 0xC2;

		nand_info->read_rety_info.reg_offset_value[0][10][0] = 0x14;
		nand_info->read_rety_info.reg_offset_value[0][10][1] = 0x0F;
		nand_info->read_rety_info.reg_offset_value[0][10][2] = 0xFB;
		nand_info->read_rety_info.reg_offset_value[0][10][3] = 0xEC;

		nand_info->read_rety_info.reg_offset_value[0][11][0] = 0xE8;
		nand_info->read_rety_info.reg_offset_value[0][11][1] = 0xFB;
		nand_info->read_rety_info.reg_offset_value[0][11][2] = 0xE8;
		nand_info->read_rety_info.reg_offset_value[0][11][3] = 0xDC;

		nand_info->read_rety_info.reg_offset_value[0][12][0] = 0X1E;
		nand_info->read_rety_info.reg_offset_value[0][12][1] = 0X14;
		nand_info->read_rety_info.reg_offset_value[0][12][2] = 0XFB;
		nand_info->read_rety_info.reg_offset_value[0][12][3] = 0XEC;

		nand_info->read_rety_info.reg_offset_value[0][13][0] = 0xFB;
		nand_info->read_rety_info.reg_offset_value[0][13][1] = 0xFF;
		nand_info->read_rety_info.reg_offset_value[0][13][2] = 0xFB;
		nand_info->read_rety_info.reg_offset_value[0][13][3] = 0xF8;

		nand_info->read_rety_info.reg_offset_value[0][14][0] = 0x07;
		nand_info->read_rety_info.reg_offset_value[0][14][1] = 0x0C;
		nand_info->read_rety_info.reg_offset_value[0][14][2] = 0x02;
		nand_info->read_rety_info.reg_offset_value[0][14][3] = 0x00;


		nand_info->read_rety_info.reg_cnt = 4;
		nand_info->read_rety_info.retry_cnt = 15;

		nand_info->read_rety_info.read_retry_handle =
			aml_nand_read_retry_handle_samsung;
		nand_info->read_rety_info.read_retry_exit =
			aml_nand_read_retry_exit_samsung;
		break;

	case SANDISK_24NM:
		nand_info->type =  SANDISK_24NM;
		aml_chip->support_new_nand = 1;
		aml_chip->ran_mode = 1;
		nand_dynamic_read->dynamic_read_flag = 0; /* DRF */
		nand_dynamic_read->reg_addr_init[0] = 0x04;
		nand_dynamic_read->reg_addr_init[1] = 0x05;
		nand_dynamic_read->reg_addr_init[2] = 0x06;
		nand_dynamic_read->reg_addr_init[3] = 0x07;
		nand_dynamic_read->reg_addr_init[4] = 0x08;
		nand_dynamic_read->reg_addr_init[5] = 0x09;
		nand_dynamic_read->reg_addr_init[6] = 0x0a;
		nand_dynamic_read->reg_addr_init[7] = 0x0b;
		nand_dynamic_read->reg_addr_init[8] = 0x0c;
		nand_info->read_rety_info.reg_addr[0] = 0x04;
		nand_info->read_rety_info.reg_addr[1] = 0x05;
		nand_info->read_rety_info.reg_addr[2] = 0x07;
		nand_info->read_rety_info.reg_offset_value[0][0][0] = 0;
		nand_info->read_rety_info.reg_offset_value[0][0][1] = 0xf0;
		nand_info->read_rety_info.reg_offset_value[0][0][2] = 0;
		nand_info->read_rety_info.reg_offset_value[0][1][0] = 0;
		nand_info->read_rety_info.reg_offset_value[0][1][1] = 0xe0;
		nand_info->read_rety_info.reg_offset_value[0][1][2] = 0;
		nand_info->read_rety_info.reg_offset_value[0][2][0] = 0xff;
		nand_info->read_rety_info.reg_offset_value[0][2][1] = 0xf0;
		nand_info->read_rety_info.reg_offset_value[0][2][2] = 0xf0;
		nand_info->read_rety_info.reg_offset_value[0][3][0] = 0xee;
		nand_info->read_rety_info.reg_offset_value[0][3][1] = 0xe0;
		nand_info->read_rety_info.reg_offset_value[0][3][2] = 0xe0;
		nand_info->read_rety_info.reg_offset_value[0][4][0] = 0xde;
		nand_info->read_rety_info.reg_offset_value[0][4][1] = 0xd0;
		nand_info->read_rety_info.reg_offset_value[0][4][2] = 0xd0;
		nand_info->read_rety_info.reg_offset_value[0][5][0] = 0xcd;
		nand_info->read_rety_info.reg_offset_value[0][5][1] = 0xc0;
		nand_info->read_rety_info.reg_offset_value[0][5][2] = 0xc0;
		nand_info->read_rety_info.reg_offset_value[0][6][0] = 0x01;
		nand_info->read_rety_info.reg_offset_value[0][6][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][6][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][7][0] = 0x02;
		nand_info->read_rety_info.reg_offset_value[0][7][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][7][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][8][0] = 0x03;
		nand_info->read_rety_info.reg_offset_value[0][8][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][8][2] = 0x00;
		nand_info->read_rety_info.reg_cnt = 3;
		nand_info->read_rety_info.retry_cnt = 9;
		nand_dynamic_read->dynamic_read_init =
			aml_nand_dynamic_read_init;
		nand_info->read_rety_info.read_retry_handle =
			aml_nand_read_retry_handle_sandisk;
		nand_info->read_rety_info.read_retry_exit =
			aml_nand_dynamic_read_exit;
		break;

	case SANDISK_A19NM:
		nand_info->type =  SANDISK_A19NM;
		aml_chip->support_new_nand = 1;
		aml_chip->ran_mode = 1;
		nand_dynamic_read->dynamic_read_flag = 0; /* DRF */


		nand_info->read_rety_info.reg_addr[0] = 0x11;
		nand_info->read_rety_info.reg_cnt = 4;
		nand_info->read_rety_info.retry_cnt = 29;

		/* //////////lower page read ////////////////////////////// */
		nand_info->read_rety_info.reg_offset_value[0][0][0] = 0x7c;
		nand_info->read_rety_info.reg_offset_value[0][0][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][0][2] = 0x7c;
		nand_info->read_rety_info.reg_offset_value[0][0][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][1][0] = 0x04;
		nand_info->read_rety_info.reg_offset_value[0][1][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][1][2] = 0x04;
		nand_info->read_rety_info.reg_offset_value[0][1][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][2][0] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][2][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][2][2] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][2][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][3][0] = 0x08;
		nand_info->read_rety_info.reg_offset_value[0][3][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][3][2] = 0x08;
		nand_info->read_rety_info.reg_offset_value[0][3][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][4][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][4][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][4][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][4][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][5][0] = 0x7c;
		nand_info->read_rety_info.reg_offset_value[0][5][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][5][2] = 0x7c;
		nand_info->read_rety_info.reg_offset_value[0][5][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][6][0] = 0x04;
		nand_info->read_rety_info.reg_offset_value[0][6][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][6][2] = 0x04;
		nand_info->read_rety_info.reg_offset_value[0][6][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][7][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][7][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][7][2] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][7][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][8][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][8][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][8][2] = 0x74;
		nand_info->read_rety_info.reg_offset_value[0][8][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][9][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][9][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][9][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][9][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][10][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][10][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][10][2] = 0x7c;
		nand_info->read_rety_info.reg_offset_value[0][10][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][11][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][11][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][11][2] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][11][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][12][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][12][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][12][2] = 0x74;
		nand_info->read_rety_info.reg_offset_value[0][12][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][13][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][13][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][13][2] = 0x70;
		nand_info->read_rety_info.reg_offset_value[0][13][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][14][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][14][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][14][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][14][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][15][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][15][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][15][2] = 0x7c;
		nand_info->read_rety_info.reg_offset_value[0][15][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][16][0] = 0x10;
		nand_info->read_rety_info.reg_offset_value[0][16][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][16][2] = 0x04;
		nand_info->read_rety_info.reg_offset_value[0][16][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][17][0] = 0x7c;
		nand_info->read_rety_info.reg_offset_value[0][17][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][17][2] = 0x7c;
		nand_info->read_rety_info.reg_offset_value[0][17][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][18][0] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][18][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][18][2] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][18][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][19][0] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][19][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][19][2] = 0x74;
		nand_info->read_rety_info.reg_offset_value[0][19][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][20][0] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][20][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][20][2] = 0x70;
		nand_info->read_rety_info.reg_offset_value[0][20][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][21][0] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][21][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][21][2] = 0x6c;
		nand_info->read_rety_info.reg_offset_value[0][21][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][22][0] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][22][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][22][2] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][22][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][23][0] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][23][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][23][2] = 0x74;
		nand_info->read_rety_info.reg_offset_value[0][23][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][24][0] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][24][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][24][2] = 0x70;
		nand_info->read_rety_info.reg_offset_value[0][24][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][25][0] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][25][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][25][2] = 0x6c;
		nand_info->read_rety_info.reg_offset_value[0][25][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][26][0] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][26][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][26][2] = 0x74;
		nand_info->read_rety_info.reg_offset_value[0][26][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][27][0] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][27][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][27][2] = 0x70;
		nand_info->read_rety_info.reg_offset_value[0][27][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][28][0] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][28][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][28][2] = 0x6c;
		nand_info->read_rety_info.reg_offset_value[0][28][3] = 0x00;

		/* ///////////////upper page read//////////////////////////// */
		nand_info->read_rety_info.reg_offset_value[1][0][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][0][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][0][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][0][3] = 0x7c;

		nand_info->read_rety_info.reg_offset_value[1][1][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][1][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][1][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][1][3] = 0x04;

		nand_info->read_rety_info.reg_offset_value[1][2][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][2][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][2][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][2][3] = 0x78;

		nand_info->read_rety_info.reg_offset_value[1][3][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][3][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][3][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][3][3] = 0x08;

		nand_info->read_rety_info.reg_offset_value[1][4][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][4][1] = 0x7c;
		nand_info->read_rety_info.reg_offset_value[1][4][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][4][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[1][5][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][5][1] = 0x7c;
		nand_info->read_rety_info.reg_offset_value[1][5][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][5][3] = 0x7c;

		nand_info->read_rety_info.reg_offset_value[1][6][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][6][1] = 0x7c;
		nand_info->read_rety_info.reg_offset_value[1][6][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][6][3] = 0x04;

		nand_info->read_rety_info.reg_offset_value[1][7][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][7][1] = 0x7c;
		nand_info->read_rety_info.reg_offset_value[1][7][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][7][3] = 0x78;

		nand_info->read_rety_info.reg_offset_value[1][8][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][8][1] = 0x7c;
		nand_info->read_rety_info.reg_offset_value[1][8][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][8][3] = 0x74;

		nand_info->read_rety_info.reg_offset_value[1][9][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][9][1] = 0x78;
		nand_info->read_rety_info.reg_offset_value[1][9][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][9][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[1][10][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][10][1] = 0x78;
		nand_info->read_rety_info.reg_offset_value[1][10][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][10][3] = 0x7c;

		nand_info->read_rety_info.reg_offset_value[1][11][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][11][1] = 0x78;
		nand_info->read_rety_info.reg_offset_value[1][11][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][11][3] = 0x78;

		nand_info->read_rety_info.reg_offset_value[1][12][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][12][1] = 0x78;
		nand_info->read_rety_info.reg_offset_value[1][12][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][12][3] = 0x74;

		nand_info->read_rety_info.reg_offset_value[1][13][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][13][1] = 0x78;
		nand_info->read_rety_info.reg_offset_value[1][13][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][13][3] = 0x70;

		nand_info->read_rety_info.reg_offset_value[1][14][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][14][1] = 0x04;
		nand_info->read_rety_info.reg_offset_value[1][14][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][14][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[1][15][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][15][1] = 0x04;
		nand_info->read_rety_info.reg_offset_value[1][15][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][15][3] = 0x7c;

		nand_info->read_rety_info.reg_offset_value[1][16][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][16][1] = 0x04;
		nand_info->read_rety_info.reg_offset_value[1][16][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][16][3] = 0x04;

		nand_info->read_rety_info.reg_offset_value[1][17][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][17][1] = 0x74;
		nand_info->read_rety_info.reg_offset_value[1][17][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][17][3] = 0x7c;

		nand_info->read_rety_info.reg_offset_value[1][18][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][18][1] = 0x74;
		nand_info->read_rety_info.reg_offset_value[1][18][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][18][3] = 0x78;

		nand_info->read_rety_info.reg_offset_value[1][19][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][19][1] = 0x74;
		nand_info->read_rety_info.reg_offset_value[1][19][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][19][3] = 0x74;

		nand_info->read_rety_info.reg_offset_value[1][20][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][20][1] = 0x74;
		nand_info->read_rety_info.reg_offset_value[1][20][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][20][3] = 0x70;

		nand_info->read_rety_info.reg_offset_value[1][21][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][21][1] = 0x74;
		nand_info->read_rety_info.reg_offset_value[1][21][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][21][3] = 0x6c;

		nand_info->read_rety_info.reg_offset_value[1][22][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][22][1] = 0x70;
		nand_info->read_rety_info.reg_offset_value[1][22][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][22][3] = 0x78;

		nand_info->read_rety_info.reg_offset_value[1][23][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][23][1] = 0x70;
		nand_info->read_rety_info.reg_offset_value[1][23][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][23][3] = 0x74;

		nand_info->read_rety_info.reg_offset_value[1][24][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][24][1] = 0x70;
		nand_info->read_rety_info.reg_offset_value[1][24][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][24][3] = 0x70;

		nand_info->read_rety_info.reg_offset_value[1][25][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][25][1] = 0x70;
		nand_info->read_rety_info.reg_offset_value[1][25][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][25][3] = 0x6c;

		nand_info->read_rety_info.reg_offset_value[1][26][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][26][1] = 0x6c;
		nand_info->read_rety_info.reg_offset_value[1][26][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][26][3] = 0x74;

		nand_info->read_rety_info.reg_offset_value[1][27][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][27][1] = 0x6c;
		nand_info->read_rety_info.reg_offset_value[1][27][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][27][3] = 0x70;

		nand_info->read_rety_info.reg_offset_value[1][28][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][28][1] = 0x6c;
		nand_info->read_rety_info.reg_offset_value[1][28][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][28][3] = 0x6c;


		nand_info->read_rety_info.read_retry_handle =
			aml_nand_read_retry_handleA19_sandisk;
		nand_info->read_rety_info.read_retry_exit =
			aml_nand_read_retry_exit_A19_sandisk;
		break;

	case SANDISK_A19NM_4G:
		nand_info->type =  SANDISK_A19NM_4G;
		aml_chip->support_new_nand = 1;
		aml_chip->ran_mode = 1;
		nand_dynamic_read->dynamic_read_flag = 0; /* DRF */


		nand_info->read_rety_info.reg_addr[0] = 0x11;
		nand_info->read_rety_info.reg_cnt = 4;
		nand_info->read_rety_info.retry_cnt = 30;

		/* //////////lower page read /////////////////////////////// */
		nand_info->read_rety_info.reg_offset_value[0][0][0] = 0x7c;
		nand_info->read_rety_info.reg_offset_value[0][0][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][0][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][0][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][1][0] = 0x04;
		nand_info->read_rety_info.reg_offset_value[0][1][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][1][2] = 0x7C;
		nand_info->read_rety_info.reg_offset_value[0][1][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][2][0] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][2][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][2][2] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][2][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][3][0] = 0x08;
		nand_info->read_rety_info.reg_offset_value[0][3][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][3][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][3][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][4][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][4][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][4][2] = 0x7C;
		nand_info->read_rety_info.reg_offset_value[0][4][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][5][0] = 0x7c;
		nand_info->read_rety_info.reg_offset_value[0][5][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][5][2] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][5][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][6][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][6][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][6][2] = 0x74;
		nand_info->read_rety_info.reg_offset_value[0][6][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][7][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][7][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][7][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][7][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][8][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][8][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][8][2] = 0x7C;
		nand_info->read_rety_info.reg_offset_value[0][8][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][9][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][9][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][9][2] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][9][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][10][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][10][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][10][2] = 0x74;
		nand_info->read_rety_info.reg_offset_value[0][10][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][11][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][11][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][11][2] = 0x70;
		nand_info->read_rety_info.reg_offset_value[0][11][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][12][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][12][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][12][2] = 0x04;
		nand_info->read_rety_info.reg_offset_value[0][12][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][13][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][13][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][13][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][13][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][14][0] = 0x0C;
		nand_info->read_rety_info.reg_offset_value[0][14][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][14][2] = 0x7C;
		nand_info->read_rety_info.reg_offset_value[0][14][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][15][0] = 0x0C;
		nand_info->read_rety_info.reg_offset_value[0][15][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][15][2] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][15][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][16][0] = 0x10;
		nand_info->read_rety_info.reg_offset_value[0][16][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][16][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][16][3] = 0x00;


		nand_info->read_rety_info.reg_offset_value[0][17][0] = 0x10;
		nand_info->read_rety_info.reg_offset_value[0][17][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][17][2] = 0x04;
		nand_info->read_rety_info.reg_offset_value[0][17][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][18][0] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][18][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][18][2] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][18][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][19][0] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][19][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][19][2] = 0x74;
		nand_info->read_rety_info.reg_offset_value[0][19][3] = 0x00;


		nand_info->read_rety_info.reg_offset_value[0][20][0] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][20][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][20][2] = 0x70;
		nand_info->read_rety_info.reg_offset_value[0][20][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][21][0] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][21][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][21][2] = 0x6c;
		nand_info->read_rety_info.reg_offset_value[0][21][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][22][0] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][22][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][22][2] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][22][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][23][0] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][23][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][23][2] = 0x74;
		nand_info->read_rety_info.reg_offset_value[0][23][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][24][0] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][24][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][24][2] = 0x6C;
		nand_info->read_rety_info.reg_offset_value[0][24][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][25][0] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][25][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][25][2] = 0x70;
		nand_info->read_rety_info.reg_offset_value[0][25][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][26][0] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][26][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][26][2] = 0x70;
		nand_info->read_rety_info.reg_offset_value[0][26][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][27][0] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][27][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][27][2] = 0x6c;
		nand_info->read_rety_info.reg_offset_value[0][27][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][28][0] = 0x78;
		nand_info->read_rety_info.reg_offset_value[0][28][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][28][2] = 0x68;
		nand_info->read_rety_info.reg_offset_value[0][28][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[0][29][0] = 0x74;
		nand_info->read_rety_info.reg_offset_value[0][29][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[0][29][2] = 0x6c;
		nand_info->read_rety_info.reg_offset_value[0][29][3] = 0x00;

		/* ///////////////upper page read//////////////////////////// */
		nand_info->read_rety_info.reg_offset_value[1][0][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][0][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][0][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][0][3] = 0x7c;

		nand_info->read_rety_info.reg_offset_value[1][1][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][1][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][1][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][1][3] = 0x78;

		nand_info->read_rety_info.reg_offset_value[1][2][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][2][1] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][2][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][2][3] = 0x74;

		nand_info->read_rety_info.reg_offset_value[1][3][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][3][1] = 0x7C;
		nand_info->read_rety_info.reg_offset_value[1][3][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][3][3] = 0x7c;

		nand_info->read_rety_info.reg_offset_value[1][4][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][4][1] = 0x7c;
		nand_info->read_rety_info.reg_offset_value[1][4][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][4][3] = 0x78;

		nand_info->read_rety_info.reg_offset_value[1][5][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][5][1] = 0x7c;
		nand_info->read_rety_info.reg_offset_value[1][5][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][5][3] = 0x74;

		nand_info->read_rety_info.reg_offset_value[1][6][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][6][1] = 0x7c;
		nand_info->read_rety_info.reg_offset_value[1][6][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][6][3] = 0x70;

		nand_info->read_rety_info.reg_offset_value[1][7][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][7][1] = 0x78;
		nand_info->read_rety_info.reg_offset_value[1][7][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][7][3] = 0x7c;

		nand_info->read_rety_info.reg_offset_value[1][8][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][8][1] = 0x78;
		nand_info->read_rety_info.reg_offset_value[1][8][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][8][3] = 0x78;

		nand_info->read_rety_info.reg_offset_value[1][9][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][9][1] = 0x78;
		nand_info->read_rety_info.reg_offset_value[1][2][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][2][3] = 0x74;

		nand_info->read_rety_info.reg_offset_value[1][10][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][10][1] = 0x78;
		nand_info->read_rety_info.reg_offset_value[1][10][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][10][3] = 0x70;

		nand_info->read_rety_info.reg_offset_value[1][11][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][11][1] = 0x78;
		nand_info->read_rety_info.reg_offset_value[1][11][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][11][3] = 0x6c;

		nand_info->read_rety_info.reg_offset_value[1][12][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][12][1] = 0x04;
		nand_info->read_rety_info.reg_offset_value[1][12][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][12][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[1][13][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][13][1] = 0x04;
		nand_info->read_rety_info.reg_offset_value[1][13][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][13][3] = 0x7c;

		nand_info->read_rety_info.reg_offset_value[1][14][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][14][1] = 0x04;
		nand_info->read_rety_info.reg_offset_value[1][14][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][14][3] = 0x78;

		nand_info->read_rety_info.reg_offset_value[1][15][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][15][1] = 0x04;
		nand_info->read_rety_info.reg_offset_value[1][15][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][15][3] = 0x74;

		nand_info->read_rety_info.reg_offset_value[1][16][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][16][1] = 0x08;
		nand_info->read_rety_info.reg_offset_value[1][16][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][16][3] = 0x7c;

		nand_info->read_rety_info.reg_offset_value[1][17][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][17][1] = 0x08;
		nand_info->read_rety_info.reg_offset_value[1][17][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][17][3] = 0x00;

		nand_info->read_rety_info.reg_offset_value[1][18][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][18][1] = 0x74;
		nand_info->read_rety_info.reg_offset_value[1][18][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][18][3] = 0x74;

		nand_info->read_rety_info.reg_offset_value[1][19][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][19][1] = 0x74;
		nand_info->read_rety_info.reg_offset_value[1][19][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][19][3] = 0x70;

		nand_info->read_rety_info.reg_offset_value[1][20][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][20][1] = 0x74;
		nand_info->read_rety_info.reg_offset_value[1][20][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][20][3] = 0x6c;

		nand_info->read_rety_info.reg_offset_value[1][21][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][21][1] = 0x74;
		nand_info->read_rety_info.reg_offset_value[1][21][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][21][3] = 0x68;

		nand_info->read_rety_info.reg_offset_value[1][22][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][22][1] = 0x70;
		nand_info->read_rety_info.reg_offset_value[1][22][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][22][3] = 0x74;

		nand_info->read_rety_info.reg_offset_value[1][23][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][23][1] = 0x70;
		nand_info->read_rety_info.reg_offset_value[1][23][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][23][3] = 0x70;

		nand_info->read_rety_info.reg_offset_value[1][24][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][24][1] = 0x70;
		nand_info->read_rety_info.reg_offset_value[1][24][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][24][3] = 0x68;

		nand_info->read_rety_info.reg_offset_value[1][25][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][25][1] = 0x70;
		nand_info->read_rety_info.reg_offset_value[1][25][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][25][3] = 0x6c;

		nand_info->read_rety_info.reg_offset_value[1][26][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][26][1] = 0x6c;
		nand_info->read_rety_info.reg_offset_value[1][26][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][26][3] = 0x6c;

		nand_info->read_rety_info.reg_offset_value[1][27][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][27][1] = 0x6c;
		nand_info->read_rety_info.reg_offset_value[1][27][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][27][3] = 0x68;

		nand_info->read_rety_info.reg_offset_value[1][28][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][28][1] = 0x6c;
		nand_info->read_rety_info.reg_offset_value[1][28][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][28][3] = 0x64;

		nand_info->read_rety_info.reg_offset_value[1][29][0] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][29][1] = 0x68;
		nand_info->read_rety_info.reg_offset_value[1][29][2] = 0x00;
		nand_info->read_rety_info.reg_offset_value[1][29][3] = 0x68;

		nand_info->read_rety_info.read_retry_handle =
			aml_nand_read_retry_handleA19_sandisk;
		nand_info->read_rety_info.read_retry_exit =
			aml_nand_read_retry_exit_A19_sandisk;
		break;

	case SANDISK_19NM:
		nand_info->type =  SANDISK_19NM;
		aml_chip->support_new_nand = 1;
		aml_chip->ran_mode = 1;
		nand_dynamic_read->dynamic_read_flag = 0;

		nand_dynamic_read->slc_flag = 0;

		memset(&nand_dynamic_read->cur_case_num_lower_page[0],
			0, MAX_CHIP_NUM);
		memset(&nand_dynamic_read->cur_case_num_upper_page[0],
			0, MAX_CHIP_NUM);

		nand_dynamic_read->reg_addr_init[0] = 0x04;
		nand_dynamic_read->reg_addr_init[1] = 0x05;
		nand_dynamic_read->reg_addr_init[2] = 0x06;
		nand_dynamic_read->reg_addr_init[3] = 0x07;
		nand_dynamic_read->reg_addr_init[4] = 0x08;
		nand_dynamic_read->reg_addr_init[5] = 0x09;
		nand_dynamic_read->reg_addr_init[6] = 0x0a;
		nand_dynamic_read->reg_addr_init[7] = 0x0b;
		nand_dynamic_read->reg_addr_init[8] = 0x0c;

		nand_dynamic_read->reg_addr_lower_page[0] = 0x04;
		nand_dynamic_read->reg_addr_lower_page[1] = 0x05;
		nand_dynamic_read->reg_addr_lower_page[2] = 0x07;

		nand_dynamic_read->reg_addr_upper_page[0] = 0x04;
		nand_dynamic_read->reg_addr_upper_page[1] = 0x05;
		nand_dynamic_read->reg_addr_upper_page[2] = 0x07;

	nand_dynamic_read->reg_offset_value_lower_page[0][0] = 0xF0;
	nand_dynamic_read->reg_offset_value_lower_page[0][1] = 0x0;
	nand_dynamic_read->reg_offset_value_lower_page[0][2] = 0xF0;

	nand_dynamic_read->reg_offset_value_lower_page[1][0] = 0xE0;
	nand_dynamic_read->reg_offset_value_lower_page[1][1] = 0x0;
	nand_dynamic_read->reg_offset_value_lower_page[1][2] = 0xE0;

	nand_dynamic_read->reg_offset_value_lower_page[2][0] = 0xD0;
	nand_dynamic_read->reg_offset_value_lower_page[2][1] = 0x0;
	nand_dynamic_read->reg_offset_value_lower_page[2][2] = 0xD0;

	nand_dynamic_read->reg_offset_value_lower_page[3][0] = 0x10;
	nand_dynamic_read->reg_offset_value_lower_page[3][1] = 0x0;
	nand_dynamic_read->reg_offset_value_lower_page[3][2] = 0x10;

	nand_dynamic_read->reg_offset_value_lower_page[4][0] = 0x20;
	nand_dynamic_read->reg_offset_value_lower_page[4][1] = 0x0;
	nand_dynamic_read->reg_offset_value_lower_page[4][2] = 0x20;

	nand_dynamic_read->reg_offset_value_lower_page[5][0] = 0x30;
	nand_dynamic_read->reg_offset_value_lower_page[5][1] = 0x0;
	nand_dynamic_read->reg_offset_value_lower_page[5][2] = 0x30;

	nand_dynamic_read->reg_offset_value_lower_page[6][0] = 0xC0;
	nand_dynamic_read->reg_offset_value_lower_page[6][1] = 0x0;
	nand_dynamic_read->reg_offset_value_lower_page[6][2] = 0xD0;

	nand_dynamic_read->reg_offset_value_lower_page[7][0] = 0x0;
	nand_dynamic_read->reg_offset_value_lower_page[7][1] = 0x0;
	nand_dynamic_read->reg_offset_value_lower_page[7][2] = 0x10;

	nand_dynamic_read->reg_offset_value_lower_page[8][0] = 0x0;
	nand_dynamic_read->reg_offset_value_lower_page[8][1] = 0x0;
	nand_dynamic_read->reg_offset_value_lower_page[8][2] = 0x20;

	nand_dynamic_read->reg_offset_value_lower_page[9][0] = 0x10;
	nand_dynamic_read->reg_offset_value_lower_page[9][1] = 0x0;
	nand_dynamic_read->reg_offset_value_lower_page[9][2] = 0x20;

	nand_dynamic_read->reg_offset_value_lower_page[10][0] = 0xB0;
	nand_dynamic_read->reg_offset_value_lower_page[10][1] = 0x0;
	nand_dynamic_read->reg_offset_value_lower_page[10][2] = 0xD0;

	nand_dynamic_read->reg_offset_value_lower_page[11][0] = 0xA0;
	nand_dynamic_read->reg_offset_value_lower_page[11][1] = 0x0;
	nand_dynamic_read->reg_offset_value_lower_page[11][2] = 0xD0;

	nand_dynamic_read->reg_offset_value_lower_page[12][0] = 0x90;
	nand_dynamic_read->reg_offset_value_lower_page[12][1] = 0x0;
	nand_dynamic_read->reg_offset_value_lower_page[12][2] = 0xD0;

	nand_dynamic_read->reg_offset_value_lower_page[13][0] = 0xB0;
	nand_dynamic_read->reg_offset_value_lower_page[13][1] = 0x0;
	nand_dynamic_read->reg_offset_value_lower_page[13][2] = 0xC0;

	nand_dynamic_read->reg_offset_value_lower_page[14][0] = 0xA0;
	nand_dynamic_read->reg_offset_value_lower_page[14][1] = 0x0;
	nand_dynamic_read->reg_offset_value_lower_page[14][2] = 0xC0;

	nand_dynamic_read->reg_offset_value_lower_page[15][0] = 0x90;
	nand_dynamic_read->reg_offset_value_lower_page[15][1] = 0x0;
	nand_dynamic_read->reg_offset_value_lower_page[15][2] = 0xC0;

	nand_dynamic_read->reg_offset_value_upper_page[0][0] = 0x0;
	nand_dynamic_read->reg_offset_value_upper_page[0][1] = 0xF0;
	nand_dynamic_read->reg_offset_value_upper_page[0][2] = 0x0;

	nand_dynamic_read->reg_offset_value_upper_page[1][0] = 0xF;
	nand_dynamic_read->reg_offset_value_upper_page[1][1] = 0xE0;
	nand_dynamic_read->reg_offset_value_upper_page[1][2] = 0x0;

	nand_dynamic_read->reg_offset_value_upper_page[2][0] = 0xF;
	nand_dynamic_read->reg_offset_value_upper_page[2][1] = 0xD0;
	nand_dynamic_read->reg_offset_value_upper_page[2][2] = 0x0;

	nand_dynamic_read->reg_offset_value_upper_page[3][0] = 0xE;
	nand_dynamic_read->reg_offset_value_upper_page[3][1] = 0xE0;
	nand_dynamic_read->reg_offset_value_upper_page[3][2] = 0x0;

	nand_dynamic_read->reg_offset_value_upper_page[4][0] = 0xE;
	nand_dynamic_read->reg_offset_value_upper_page[4][1] = 0xD0;
	nand_dynamic_read->reg_offset_value_upper_page[4][2] = 0x0;

	nand_dynamic_read->reg_offset_value_upper_page[5][0] = 0xD;
	nand_dynamic_read->reg_offset_value_upper_page[5][1] = 0xF0;
	nand_dynamic_read->reg_offset_value_upper_page[5][2] = 0x0;

	nand_dynamic_read->reg_offset_value_upper_page[6][0] = 0xD;
	nand_dynamic_read->reg_offset_value_upper_page[6][1] = 0xE0;
	nand_dynamic_read->reg_offset_value_upper_page[6][2] = 0x0;

	nand_dynamic_read->reg_offset_value_upper_page[7][0] = 0xD;
	nand_dynamic_read->reg_offset_value_upper_page[7][1] = 0xD0;
	nand_dynamic_read->reg_offset_value_upper_page[7][2] = 0x0;

	nand_dynamic_read->reg_offset_value_upper_page[8][0] = 0x1;
	nand_dynamic_read->reg_offset_value_upper_page[8][1] = 0x10;
	nand_dynamic_read->reg_offset_value_upper_page[8][2] = 0x0;

	nand_dynamic_read->reg_offset_value_upper_page[9][0] = 0x2;
	nand_dynamic_read->reg_offset_value_upper_page[9][1] = 0x20;
	nand_dynamic_read->reg_offset_value_upper_page[9][2] = 0x0;

	nand_dynamic_read->reg_offset_value_upper_page[10][0] = 0x2;
	nand_dynamic_read->reg_offset_value_upper_page[10][1] = 0x10;
	nand_dynamic_read->reg_offset_value_upper_page[10][2] = 0x0;

	nand_dynamic_read->reg_offset_value_upper_page[11][0] = 0x3;
	nand_dynamic_read->reg_offset_value_upper_page[11][1] = 0x20;
	nand_dynamic_read->reg_offset_value_upper_page[11][2] = 0x0;

	nand_dynamic_read->reg_offset_value_upper_page[12][0] = 0xF;
	nand_dynamic_read->reg_offset_value_upper_page[12][1] = 0x00;
	nand_dynamic_read->reg_offset_value_upper_page[12][2] = 0x0;

	nand_dynamic_read->reg_offset_value_upper_page[13][0] = 0xE;
	nand_dynamic_read->reg_offset_value_upper_page[13][1] = 0xF0;
	nand_dynamic_read->reg_offset_value_upper_page[13][2] = 0x0;

	nand_dynamic_read->reg_offset_value_upper_page[14][0] = 0xD;
	nand_dynamic_read->reg_offset_value_upper_page[14][1] = 0xC0;
	nand_dynamic_read->reg_offset_value_upper_page[14][2] = 0x0;

	nand_dynamic_read->reg_offset_value_upper_page[15][0] = 0xF;
	nand_dynamic_read->reg_offset_value_upper_page[15][1] = 0xF0;
	nand_dynamic_read->reg_offset_value_upper_page[15][2] = 0x0;

	nand_dynamic_read->reg_offset_value_upper_page[16][0] = 0x1;
	nand_dynamic_read->reg_offset_value_upper_page[16][1] = 0x00;
	nand_dynamic_read->reg_offset_value_upper_page[16][2] = 0x0;

	nand_dynamic_read->reg_offset_value_upper_page[17][0] = 0x20;
	nand_dynamic_read->reg_offset_value_upper_page[17][1] = 0x00;
	nand_dynamic_read->reg_offset_value_upper_page[17][2] = 0x0;

	nand_dynamic_read->reg_offset_value_upper_page[18][0] = 0xD;
	nand_dynamic_read->reg_offset_value_upper_page[18][1] = 0xB0;
	nand_dynamic_read->reg_offset_value_upper_page[18][2] = 0x0;

	nand_dynamic_read->reg_offset_value_upper_page[19][0] = 0xC;
	nand_dynamic_read->reg_offset_value_upper_page[19][1] = 0xA0;
	nand_dynamic_read->reg_offset_value_upper_page[19][2] = 0x0;

	nand_dynamic_read->dynamic_read_init =
		aml_nand_dynamic_read_init;
	nand_dynamic_read->dynamic_read_handle =
		aml_nand_dynamic_read_handle;
	nand_dynamic_read->dynamic_read_exit =
		aml_nand_dynamic_read_exit;
	nand_dynamic_read->enter_slc_mode =
		aml_nand_enter_slc_mode_sandisk;
	nand_dynamic_read->exit_slc_mode =
		aml_nand_exit_slc_mode_sandisk;
		break;

	case MICRON_20NM:
		nand_info->type =  MICRON_20NM;
		aml_chip->support_new_nand = 1;
		aml_chip->ran_mode = 1;

		nand_info->read_rety_info.reg_addr[0] = 0x89;

		nand_info->read_rety_info.reg_offset_value[0][0][0] = 0x1;
		nand_info->read_rety_info.reg_offset_value[0][1][0] = 0x2;
		nand_info->read_rety_info.reg_offset_value[0][2][0] = 0x3;
		nand_info->read_rety_info.reg_offset_value[0][3][0] = 0x4;
		nand_info->read_rety_info.reg_offset_value[0][4][0] = 0x5;
		nand_info->read_rety_info.reg_offset_value[0][5][0] = 0x6;
		nand_info->read_rety_info.reg_offset_value[0][6][0] = 0x7;


		nand_info->read_rety_info.reg_cnt = 1;
		nand_info->read_rety_info.retry_cnt = 7;

		nand_info->read_rety_info.read_retry_handle =
			aml_nand_read_retry_handle_micron;
		nand_info->read_rety_info.read_retry_exit =
			aml_nand_read_retry_exit_micron;
		break;

	case INTEL_20NM:
		aml_chip->ran_mode = 1;
		nand_info->type =  INTEL_20NM;
		aml_chip->support_new_nand = 1;
		nand_info->read_rety_info.reg_addr[0] = 0x89;
		nand_info->read_rety_info.reg_addr[1] = 0x93;

		nand_info->read_rety_info.reg_offset_value[0][0][0] = 0x1;
		nand_info->read_rety_info.reg_offset_value[0][1][0] = 0x2;
		nand_info->read_rety_info.reg_offset_value[0][2][0] = 0x3;
		nand_info->read_rety_info.reg_offset_value[0][3][0] = 0x0;
		nand_info->read_rety_info.reg_offset_value[0][4][0] = 0x1;
		nand_info->read_rety_info.reg_offset_value[0][5][0] = 0x2;
		nand_info->read_rety_info.reg_offset_value[0][6][0] = 0x3;

		nand_info->read_rety_info.reg_cnt = 2;
		nand_info->read_rety_info.retry_cnt = 7;

		nand_info->read_rety_info.read_retry_handle =
			aml_nand_read_retry_handle_intel;
		nand_info->read_rety_info.read_retry_exit =
			aml_nand_read_retry_exit_intel;
		break;
	}
}

