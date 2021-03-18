/*
 * Copyright (C) 2017-2019 InvenSense, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "../inv_mpu_iio.h"

static int inv_load_firmware(struct inv_mpu_state *st)
{
	int bank, write_size;
	int result, size;
	u16 memaddr;
	u8 *data;

	data = st->firmware;
	size = st->dmp_image_size - DMP_OFFSET;
	memaddr = DMP_OFFSET;
	data += DMP_OFFSET;
	for (bank = 0; size > 0; bank++, size -= write_size) {
		if (size > MAX_DMP_READ_SIZE)
			write_size = MAX_DMP_READ_SIZE;
		else
			write_size = size;
		result = mem_w(memaddr, write_size, data);
		if (result) {
			pr_err("error writing firmware:%d\n", bank);
			pr_info("%s: error writing firmware %d\n", __func__,
				bank);
			return result;
		}
		memaddr += write_size;
		data += write_size;
	}

	pr_info("%s: DMP load firmware successed DMP_Size %d\n",
		__func__, st->dmp_image_size);
	return 0;
}

static int inv_verify_firmware(struct inv_mpu_state *st)
{
	int bank, write_size, size;
	int result;
	u16 memaddr;
	u8 firmware[MPU_MEM_BANK_SIZE];
	u8 *data;

	data = st->firmware;
	size = st->dmp_image_size - DMP_OFFSET;
	memaddr = DMP_OFFSET;
	data += DMP_OFFSET;
	for (bank = 0; size > 0; bank++, size -= write_size) {
		if (size > MAX_DMP_READ_SIZE)
			write_size = MAX_DMP_READ_SIZE;
		else
			write_size = size;
		result = mem_r(memaddr, write_size, firmware);
		if (result)
			return result;
		if (0 != memcmp(firmware, data, write_size)) {
			pr_err("load data error, bank=%d\n", bank);
			pr_info("%s: varification, load data error bank %d\n",
				__func__, bank);
			return -EINVAL;
		}
		memaddr += write_size;
		data += write_size;
	}
	return 0;
}

/*
 * inv_firmware_load() -  calling this function will load the firmware.
 */
int inv_firmware_load(struct inv_mpu_state *st)
{
	int result;

	result = inv_switch_power_in_lp(st, true);
	if (result) {
		pr_info("%s: load firmware set power error\n", __func__);
		pr_err("load firmware set power error\n");
		goto firmware_write_fail;
	}
	result = inv_stop_dmp(st);
	if (result) {
		pr_info("%s: load firmware : stop dmp error\n", __func__);
		pr_err("load firmware:stop dmp error\n");
		goto firmware_write_fail;
	}
	result = inv_load_firmware(st);
	if (result) {
		pr_info("%s: load firmware:load firmware error\n", __func__);
		pr_err("load firmware:load firmware eror\n");
		goto firmware_write_fail;
	}
	result = inv_verify_firmware(st);
	if (result) {
		pr_info("%s: load firmware:verify firmware error\n", __func__);
		pr_err("load firmware:verify firmware error\n");
		goto firmware_write_fail;
	}
	result = inv_setup_dmp_firmware(st);
	if (result) {
		pr_info("%s: load firmware:setup dmp error\n", __func__);
		pr_err("load firmware:setup dmp error\n");
	}
firmware_write_fail:
	result |= inv_set_power(st, false);
	if (result) {
		pr_info("%s: load firmware:shuting down power error\n",
			__func__);
		pr_err("load firmware:shuting down power error\n");
		return result;
	}

	st->chip_config.firmware_loaded = 1;

	pr_info("%s: load firmware successed\n", __func__);
	return 0;
}

int inv_dmp_read(struct inv_mpu_state *st, int off, int size, u8 *buf)
{
	int read_size, i, result;
	u16 addr;

	result = inv_switch_power_in_lp(st, true);
	if (result)
		return result;
	inv_stop_dmp(st);

	addr = off;
	i = 0;
	while (size > 0) {
		if (addr % MAX_DMP_READ_SIZE)
			read_size = MAX_DMP_READ_SIZE
				- (addr % MAX_DMP_READ_SIZE);
		else if (size > MAX_DMP_READ_SIZE)
			read_size = MAX_DMP_READ_SIZE;
		else
			read_size = size;

		result = mem_r(addr, read_size, &buf[i]);
		if (result)
			return result;

		addr += read_size;
		i += read_size;
		size -= read_size;
	}

	return 0;
}
