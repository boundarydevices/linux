/*******************************************************************************
* Copyright (C) Maxim Integrated Products, Inc., All Rights Reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL MAXIM INTEGRATED BE LIABLE FOR ANY CLAIM, DAMAGES
* OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*
* Except as contained in this notice, the name of Maxim Integrated
* Products, Inc. shall not be used except as stated in the Maxim Integrated
* Products, Inc. Branding Policy.
*
* The mere transfer of this software does not imply any licenses
* of trade secrets, proprietary technology, copyrights, patents,
* trademarks, maskwork rights, or any other form of intellectual
* property whatsoever. Maxim Integrated Products, Inc. retains all
* ownership rights.
*******************************************************************************
*/


#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/mfd/core.h>
#include <linux/regulator/machine.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/mfd/max77958.h>
#include <linux/mfd/max77958-private.h>
#include <linux/usbc/max77958-bc12.h>
#include <linux/usbc/max77958-usbc.h>
#include <linux/usbc/max77958-pass2.h>
#if defined(CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif /* CONFIG_OF */

#define I2C_ADDR_BC12	(0x4A >> 1)

static struct mfd_cell max77958_devs[] = {
	{ .name = "max77958-usbc", },
};

int max77958_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest)
{
	struct max77958_dev *max77958 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&max77958->i2c_lock);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	mutex_unlock(&max77958->i2c_lock);
	if (ret < 0) {
		pr_info("%s:%s reg(0x%x), ret(%d)\n",
				MFD_DEV_NAME, __func__, reg, ret);
		return ret;
	}
	ret &= 0xff;
	*dest = ret;
	return 0;
}
EXPORT_SYMBOL_GPL(max77958_read_reg);

int max77958_bulk_read(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
	struct max77958_dev *max77958 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&max77958->i2c_lock);
	ret = i2c_smbus_read_i2c_block_data(i2c, reg, count, buf);
	mutex_unlock(&max77958->i2c_lock);
	if (ret < 0) {
		pr_info("%s:%s reg(0x%x), ret(%d)\n",
				MFD_DEV_NAME, __func__, reg, ret);
		return ret;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(max77958_bulk_read);

int max77958_write_reg(struct i2c_client *i2c, u8 reg, u8 value)
{
	struct max77958_dev *max77958 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&max77958->i2c_lock);
	ret = i2c_smbus_write_byte_data(i2c, reg, value);
	mutex_unlock(&max77958->i2c_lock);
	if (ret < 0) {
		pr_info("%s:%s reg(0x%x), ret(%d)\n",
				MFD_DEV_NAME, __func__, reg, ret);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(max77958_write_reg);

int max77958_bulk_write(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
	struct max77958_dev *max77958 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&max77958->i2c_lock);
	ret = i2c_smbus_write_i2c_block_data(i2c, reg, count, buf);
	mutex_unlock(&max77958->i2c_lock);
	if (ret < 0)
		return ret;
	return 0;
}
EXPORT_SYMBOL_GPL(max77958_bulk_write);


int max77958_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask)
{
	struct max77958_dev *max77958 = i2c_get_clientdata(i2c);
	int ret;
	u8 old_val, new_val;

	mutex_lock(&max77958->i2c_lock);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	if (ret < 0)
		goto err;
	if (ret >= 0) {
		old_val = ret & 0xff;
		new_val = (val & mask) | (old_val & (~mask));
		ret = i2c_smbus_write_byte_data(i2c, reg, new_val);
		if (ret < 0)
			goto err;
	}
err:
	mutex_unlock(&max77958->i2c_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(max77958_update_reg);

#if defined(CONFIG_OF)
static int of_max77958_dt(struct device *dev,
	struct max77958_platform_data *pdata)
{
	struct device_node *np_max77958 = dev->of_node;

	if (!np_max77958)
		return -EINVAL;

	pdata->wakeup = of_property_read_bool(np_max77958,
		"max77958,wakeup");

	return 0;
}
#endif /* CONFIG_OF */

void max77958_reset_ic(struct max77958_dev *max77958)
{
	pr_info("Reset!!");
	max77958_write_reg(max77958->i2c, 0x80, 0x0F);
	msleep(100);
}
static void max77958_usbc_wait_response_q(struct work_struct *work)
{
	struct max77958_dev *max77958;
	u8 read_value = 0x00;
	u8 dummy[2] = { 0, };

	max77958 = container_of(work, struct max77958_dev, fw_work);

	while (max77958->fw_update_state == FW_UPDATE_WAIT_RESP_START) {
		max77958_bulk_read(max77958->i2c, REG_UIC_INT, 1, dummy);
		read_value = dummy[0];
		if ((read_value & BIT_APCmdResI) == BIT_APCmdResI)
			break;
	}

	complete_all(&max77958->fw_completion);
}

static int max77958_usbc_wait_response(struct max77958_dev *max77958)
{
	unsigned long time_remaining = 0;

	max77958->fw_update_state = FW_UPDATE_WAIT_RESP_START;

	init_completion(&max77958->fw_completion);
	queue_work(max77958->fw_workqueue, &max77958->fw_work);

	time_remaining = wait_for_completion_timeout(
			&max77958->fw_completion,
			msecs_to_jiffies(FW_WAIT_TIMEOUT));

	max77958->fw_update_state = FW_UPDATE_WAIT_RESP_STOP;

	if (!time_remaining) {
		pr_info("Failed to update due to timeout");
		cancel_work_sync(&max77958->fw_work);
		return FW_UPDATE_TIMEOUT_FAIL;
	}

	return 0;
}


int max77958_update_action_mtp(struct max77958_dev *max77958,
		const u8 *action_bin, int fw_bin_len)
{
	int offset = 0;
	unsigned long duration = 0;
	int size = 0;
	int try_count = 0;
	int ret = 0;
	u8 action_data[28];
	u8 w_data[OPCODE_MAX_LENGTH] = { 0, };
	u8 r_data[OPCODE_MAX_LENGTH] = { 0, };
	int length = 0;
	int mtp_pos = 0;
	int prev_pos = 0;
	u8 status2 = 0x0;

retry:
	offset = 0;
	duration = 0;
	size = 0;
	ret = 0;
	mtp_pos = 0;
	prev_pos = 0x0;
	try_count++;

	/* to do (unmask interrupt) */
	ret = max77958_read_reg(max77958->i2c,
		REG_UIC_HW_REV, &max77958->HW_Revision);
	ret = max77958_read_reg(max77958->i2c,
		REG_UIC_DEVICE_ID, &max77958->Device_Revision);
	ret = max77958_read_reg(max77958->i2c,
		REG_UIC_FW_REV, &max77958->FW_Revision);
	ret = max77958_read_reg(max77958->i2c,
		REG_UIC_FW_SUBREV, &max77958->FW_Minor_Revision);
	if (ret < 0) {
		pr_info("Failed to i2c communication");
		goto out;
	}

	duration = jiffies;

	pr_info("hardware: %02X.%02X",
			max77958->HW_Revision, max77958->Device_Revision);

	pr_info("firmware : %02X.%02X",
			max77958->FW_Revision, max77958->FW_Minor_Revision);
	if ((max77958->FW_Revision != 0xff) && try_count < 10) {

		while (offset < fw_bin_len) {
			memset(action_data, 0, sizeof(action_data));
			memset(w_data, 0, sizeof(w_data));
			memset(r_data, 0, sizeof(r_data));
			for (size = 0x0;
				size < 28 && offset < fw_bin_len; size++)
				action_data[size] = action_bin[offset++];
			w_data[0] = OPCODE_ACTION_BLOCK_MTP_UPDATE;
			if (mtp_pos) {
				prev_pos = mtp_pos;
				w_data[1] = mtp_pos++;
			} else {
				w_data[1] = 0xF0;
				mtp_pos++;
			}
			length = size;
			memcpy(&w_data[2], &action_data, length);
			print_hex_dump(KERN_INFO, "max77958: opcode_write: ",
					DUMP_PREFIX_OFFSET, 16, 1, w_data,
					OPCODE_MAX_LENGTH, false);
			/* write the half data */
			max77958_bulk_write(max77958->i2c,
				OPCODE_WRITE,
				I2C_SMBUS_BLOCK_HALF,
				w_data);
			max77958_bulk_write(max77958->i2c,
				OPCODE_WRITE + I2C_SMBUS_BLOCK_HALF,
				OPCODE_MAX_LENGTH - I2C_SMBUS_BLOCK_HALF,
				&w_data[I2C_SMBUS_BLOCK_HALF]);
			max77958_usbc_wait_response(max77958);
			/* Read opcode data */

			max77958_bulk_read(max77958->i2c,
				REG_USBC_STATUS2, 1, &status2);
			max77958_bulk_read(max77958->i2c, OPCODE_READ,
					3, r_data);
			print_hex_dump(KERN_INFO, "max77958: opcode_read: ",
					DUMP_PREFIX_OFFSET, 16, 1, r_data,
					I2C_SMBUS_BLOCK_HALF, false);
			if (r_data[0] == 0xFF) {
				pr_info("MTP Erase Fail");
				break;
			} else if (r_data[0] == prev_pos || r_data[0] == 0xF0) {
				pr_info("MTP Write Done");
			} else {
				pr_info("REG_USBC_STATUS2 [%x][%x]",
					status2, r_data[0]);
				pr_info("REG_USBC_STATUS2 [%x][%x]",
					r_data[1], r_data[3]);
				goto retry;
			}
		}
	} else {
		pr_info("Need to update the firmware! or MTP write fail [%x]",
			try_count);
		goto out;
	}

	duration = jiffies - duration;
	pr_info("Duration : %dms", jiffies_to_msecs(duration));
out:
	return 0;
}


static void max77958_update_custm_info(struct max77958_dev *max77958)
{
	union max77958_custm_info customer_info;
	u8 w_data[OPCODE_MAX_LENGTH] = { 0, };
	int length = 0;

	memset(&customer_info, 0x00, sizeof(customer_info));
	pr_info("update customer information");
	customer_info.custm_bits.debugSRC = DISABLED;
	customer_info.custm_bits.debugSNK = DISABLED;
	customer_info.custm_bits.AudioAcc = DISABLED;
	customer_info.custm_bits.TrySNK = ENABLED;
	customer_info.custm_bits.TypeCstate = DRP;
	customer_info.custm_bits.notdef1 = MemoryUpdateRAM|(MoistureDetectionEnable<<1);
	customer_info.custm_bits.VID = PD_VID;
	customer_info.custm_bits.PID = PD_PID;
	customer_info.custm_bits.i2cSID1 = I2C_SID0;
	customer_info.custm_bits.i2cSID2 = I2C_SID1;
	customer_info.custm_bits.i2cSID3 = I2C_SID2;
	customer_info.custm_bits.i2cSID4 = I2C_SID3;
	customer_info.custm_bits.snkPDOVoltage = 0x0064; /* 5000mV */
	customer_info.custm_bits.snkPDOOperatingI = 0x012c; /* 3000mA */
	customer_info.custm_bits.srcPDOpeakCurrent = 0x0; /* 100% IOC */
	customer_info.custm_bits.srcPDOVoltage = 0x0064; /* 5000mV */
	customer_info.custm_bits.srcPDOMaxCurrent = 0x0096; /* 1500mA */
	customer_info.custm_bits.srcPDOOverIDebounce = 0x003c; /* 60ms */
	customer_info.custm_bits.srcPDOOverIThreshold = 0x005A; /* 900mA */
	w_data[0] = OPCODE_SET_CSTM_INFORMATION_W;
	length = sizeof(union max77958_custm_info);
	memcpy(&w_data[1], &customer_info, sizeof(union max77958_custm_info));
	print_hex_dump(KERN_INFO, "max77958: opcode_write: ",
			DUMP_PREFIX_OFFSET, 16, 1, w_data,
			length + OPCODE_SIZE, false);
	max77958_bulk_write(max77958->i2c, OPCODE_WRITE,
			length + OPCODE_SIZE, w_data);
	/* Write end of data by 0x00 */
	if (length < OPCODE_DATA_LENGTH)
		max77958_write_reg(max77958->i2c, OPCODE_WRITE_END, 0x00);
	max77958_usbc_wait_response(max77958);

	memset(w_data, 0x00, sizeof(w_data));
	w_data[0] = OPCODE_SET_CSTM_INFORMATION_R;
	length = sizeof(union max77958_custm_info);
	print_hex_dump(KERN_INFO, "max77958: opcode_write: ",
			DUMP_PREFIX_OFFSET, 16, 1, w_data,
			length + OPCODE_SIZE, false);
	max77958_bulk_write(max77958->i2c, OPCODE_WRITE,
			length + OPCODE_SIZE, w_data);
	/* Write end of data by 0x00 */
	if (length < OPCODE_DATA_LENGTH)
		max77958_write_reg(max77958->i2c, OPCODE_WRITE_END, 0x00);

	max77958_usbc_wait_response(max77958);
	/* Read opcode data */
	memset(w_data, 0x00, sizeof(w_data));
	max77958_bulk_read(max77958->i2c, OPCODE_READ,
			length + OPCODE_SIZE, w_data);
	print_hex_dump(KERN_INFO, "max77958: opcode_read: ",
			DUMP_PREFIX_OFFSET, 16, 1, w_data,
			length + OPCODE_SIZE, false);
}

static void max77958_update_device_info(struct max77958_dev *max77958)
{
	union max77958_device_info device_info;
	u8 w_data[OPCODE_MAX_LENGTH] = { 0, };
	int length = 0;

	memset(&device_info, 0x00, sizeof(device_info));
	pr_info("update device information");
	device_info.hwcustm_bits.UcCtrl1Ctrl2_dflt = 0x5A85;
	device_info.hwcustm_bits.GpCtrl1Ctrl2_dflt = 0x6622;
	device_info.hwcustm_bits.PD0_dflt = 0x0056;
	device_info.hwcustm_bits.PD1_dflt = 0x0078;
	device_info.hwcustm_bits.PO0_dflt = 0x009A;
	device_info.hwcustm_bits.PO1_dflt = 0x0036;
	device_info.hwcustm_bits.SupplyCfg = 0x5001;
	device_info.hwcustm_bits.PD0PD1_su = 0x0000;
	device_info.hwcustm_bits.PO0PO1_su = 0x0000;
	device_info.hwcustm_bits.PD0PD1_aftersu = 0x7856;
	device_info.hwcustm_bits.PO0PO1_aftersu = 0x369A;
	device_info.hwcustm_bits.sw_ctrl_dflt = 0x1201;
	w_data[0] = OPCODE_SET_DEVCONFG_INFORMATION_W;
	length = sizeof(union max77958_device_info);
	memcpy(&w_data[1], &device_info, sizeof(union max77958_device_info));
	print_hex_dump(KERN_INFO, "max77958: opcode_write: ",
			DUMP_PREFIX_OFFSET, 16, 1, w_data,
			length + OPCODE_SIZE, false);
	max77958_bulk_write(max77958->i2c, OPCODE_WRITE,
			length + OPCODE_SIZE, w_data);
	/* Write end of data by 0x00 */
	if (length < OPCODE_DATA_LENGTH)
		max77958_write_reg(max77958->i2c, OPCODE_WRITE_END, 0x00);
	max77958_usbc_wait_response(max77958);

	memset(w_data, 0x00, sizeof(w_data));
	w_data[0] = OPCODE_SET_DEVCONFG_INFORMATION_R;
	length = sizeof(union max77958_custm_info);
	print_hex_dump(KERN_INFO, "max77958: opcode_write: ",
			DUMP_PREFIX_OFFSET, 16, 1, w_data,
			length + OPCODE_SIZE, false);
	max77958_bulk_write(max77958->i2c, OPCODE_WRITE,
			length + OPCODE_SIZE, w_data);
	/* Write end of data by 0x00 */
	if (length < OPCODE_DATA_LENGTH)
		max77958_write_reg(max77958->i2c, OPCODE_WRITE_END, 0x00);

	max77958_usbc_wait_response(max77958);
	memset(w_data, 0x00, sizeof(w_data));
	/* Read opcode data */
	max77958_bulk_read(max77958->i2c, OPCODE_READ,
			length + OPCODE_SIZE, w_data);
	print_hex_dump(KERN_INFO, "max77958: opcode_read: ",
			DUMP_PREFIX_OFFSET, 16, 1, w_data,
			length + OPCODE_SIZE, false);
}

static void max77958_update_configuration(struct max77958_dev *max77958)
{
	int exe_flag = 0;

	if(exe_flag) {
		disable_irq(max77958->irq);
		max77958_write_reg(max77958->i2c, REG_PD_INT_M, 0xFF);
		max77958_write_reg(max77958->i2c, REG_CC_INT_M, 0xFF);
		max77958_write_reg(max77958->i2c, REG_UIC_INT_M, 0xFF);
		max77958_write_reg(max77958->i2c, REG_ACTION_INT_M, 0xFF);
		max77958_update_custm_info(max77958);
		max77958_update_device_info(max77958);
		//max77958_update_action_mtp(max77958,
		//	ACTION_FLASH_FW_PASS2, sizeof(ACTION_FLASH_FW_PASS2));
		max77958_reset_ic(max77958);
		enable_irq(max77958->irq);
	}
}



static int __max77958_usbc_fw_update(
		struct max77958_dev *max77958, const u8 *fw_bin)
{
	u8 fw_cmd = FW_CMD_END;
	u8 fw_len = 0;
	u8 fw_opcode = 0;
	u8 fw_data_len = 0;
	u8 fw_data[FW_CMD_WRITE_SIZE] = { 0, };
	u8 verify_data[FW_VERIFY_DATA_SIZE] = { 0, };
	int ret = -FW_UPDATE_CMD_FAIL;

	/*
	 * fw_bin[0] = Write Command (0x01)
	 * or
	 * fw_bin[0] = Read Command (0x03)
	 * or
	 * fw_bin[0] = End Command (0x00)
	 */
	fw_cmd = fw_bin[0];

	/*
	 * Check FW Command
	 */
	if (fw_cmd == FW_CMD_END) {
		max77958_reset_ic(max77958);
		max77958->fw_update_state = FW_UPDATE_END;
		return FW_UPDATE_END;
	}

	/*
	 * fw_bin[1] = Length ( OPCode + Data )
	 */
	fw_len = fw_bin[1];

	/*
	 * Check fw data length
	 * We support 0x22 or 0x04 only
	 */
	if (fw_len != 0x22 && fw_len != 0x04)
		return FW_UPDATE_MAX_LENGTH_FAIL;

	/*
	 * fw_bin[2] = OPCode
	 */
	fw_opcode = fw_bin[2];

	/*
	 * In case write command,
	 * fw_bin[35:3] = Data
	 *
	 * In case read command,
	 * fw_bin[5:3]  = Data
	 */
	fw_data_len = fw_len - 1; /* exclude opcode */
	memcpy(fw_data, &fw_bin[3], fw_data_len);

	switch (fw_cmd) {
	case FW_CMD_WRITE:
		if (fw_data_len > I2C_SMBUS_BLOCK_MAX) {
			/* write the half data */
			max77958_bulk_write(max77958->i2c,
					fw_opcode,
					I2C_SMBUS_BLOCK_HALF,
					fw_data);
			max77958_bulk_write(max77958->i2c,
					fw_opcode + I2C_SMBUS_BLOCK_HALF,
					fw_data_len - I2C_SMBUS_BLOCK_HALF,
					&fw_data[I2C_SMBUS_BLOCK_HALF]);
		} else
			max77958_bulk_write(max77958->i2c,
					fw_opcode,
					fw_data_len,
					fw_data);

		ret = max77958_usbc_wait_response(max77958);
		if (ret)
			return ret;

		/* msleep(1); */

		return FW_CMD_WRITE_SIZE;


	case FW_CMD_READ:
		max77958_bulk_read(max77958->i2c,
				fw_opcode,
				fw_data_len,
				verify_data);
		/*
		 * Check fw data sequence number
		 * It should be increased from 1 step by step.
		 */
		if (memcmp(verify_data, &fw_data[1], 2)) {
			pr_info("[0x%02x 0x%02x], [0x%02x]",
				verify_data[0], fw_data[0], verify_data[1]);
			pr_info("[0x%02x], [0x%02x, 0x%02x]",
				fw_data[1],	verify_data[2], fw_data[2]);
			return FW_UPDATE_VERIFY_FAIL;
		}

		return FW_CMD_READ_SIZE;
	}

	pr_info("Command error");

	return ret;
}


int max77958_usbc_fw_update(struct max77958_dev *max77958,
		const u8 *fw_bin, int fw_bin_len, int enforce_do)
{
	struct max77958_fw_header fw_header;
	int offset = 0;
	unsigned long duration = 0;
	int size = 0;
	int try_count = 0;
	int ret = 0;
	u8 try_command = 0;

	disable_irq(max77958->irq);
	max77958_write_reg(max77958->i2c, REG_PD_INT_M, 0xFF);
	max77958_write_reg(max77958->i2c, REG_CC_INT_M, 0xFF);
	max77958_write_reg(max77958->i2c, REG_UIC_INT_M, 0xFF);
	max77958_write_reg(max77958->i2c, REG_ACTION_INT_M, 0xFF);
retry:
	offset = 0;
	duration = 0;
	size = 0;
	ret = 0;

	/* to do (unmask interrupt) */
	ret = max77958_read_reg(max77958->i2c,
		REG_UIC_HW_REV, &max77958->HW_Revision);
	ret = max77958_read_reg(max77958->i2c,
		REG_UIC_DEVICE_ID, &max77958->Device_Revision);
	ret = max77958_read_reg(max77958->i2c,
		REG_UIC_FW_REV, &max77958->FW_Revision);
	ret = max77958_read_reg(max77958->i2c,
		REG_UIC_FW_SUBREV, &max77958->FW_Minor_Revision);
	if (ret < 0 && (try_count == 0 && try_command == 0)) {
		pr_info("Failed to read FW_REV");
		goto out;
	}

	duration = jiffies;

	pr_info("chip : %02X.%02X, FW : %02X.%02X",
		max77958->FW_Revision, max77958->FW_Minor_Revision,
		fw_bin[4], fw_bin[5]);
	if ((max77958->FW_Revision == 0xff) ||
			(max77958->FW_Revision < fw_bin[4]) ||
			((max77958->FW_Revision == fw_bin[4]) &&
			(max77958->FW_Minor_Revision < fw_bin[5])) ||
			enforce_do) {

		max77958_write_reg(max77958->i2c, 0x21, 0xD0);
		max77958_write_reg(max77958->i2c, 0x41, 0x00);
		msleep(500);

		max77958_read_reg(max77958->i2c,
			REG_UIC_FW_REV, &max77958->FW_Revision);
		max77958_read_reg(max77958->i2c,
			REG_UIC_FW_SUBREV, &max77958->FW_Minor_Revision);
		pr_info("Start FW updating (%02X.%02X)",
			max77958->FW_Revision, max77958->FW_Minor_Revision);
		if (max77958->FW_Revision != 0xFF && try_command < 3) {
			try_command++;
			goto retry;
		}

		/* Copy fw header */
		memcpy(&fw_header, fw_bin, FW_HEADER_SIZE);

		if (max77958->FW_Revision != 0xFF) {
			if (++try_count < FW_VERIFY_TRY_COUNT) {
				pr_info("the Fail to enter secure mode %d",
						try_count);
				goto retry;
			} else {
				pr_info("the Secure Update Fail!!");
				goto out;
			}
		}

		for (offset = FW_HEADER_SIZE;
				offset < fw_bin_len && size != FW_UPDATE_END;) {

			size = __max77958_usbc_fw_update(max77958,
				&fw_bin[offset]);

			switch (size) {
			case FW_UPDATE_VERIFY_FAIL:
				offset -= FW_CMD_WRITE_SIZE;
				fallthrough;
			case FW_UPDATE_TIMEOUT_FAIL:
				/*
				 * Retry FW updating
				 */
				if (++try_count < FW_VERIFY_TRY_COUNT) {
					pr_info("Retry fw write.");
					pr_info("ret %d, count %d, offset %d",
						size, try_count, offset);
					goto retry;
				} else {
					pr_info("Failed to update FW.");
					pr_info("ret %d, offset %d",
							size, (offset + size));
					goto out;
				}
				break;
			case FW_UPDATE_CMD_FAIL:
			case FW_UPDATE_MAX_LENGTH_FAIL:
				pr_info("Failed to update FW.");
				pr_info("ret %d, offset %d",
						size, (offset + size));
				goto out;
			case FW_UPDATE_END: /* 0x00 */
				max77958_read_reg(max77958->i2c,
					REG_UIC_FW_REV, &max77958->FW_Revision);
				max77958_read_reg(max77958->i2c,
					REG_UIC_FW_SUBREV,
					&max77958->FW_Minor_Revision);
				pr_info("chip : %02X.%02X, FW : %02X.%02X",
					max77958->FW_Revision,
					max77958->FW_Minor_Revision,
					fw_header.data4, fw_header.data5);
				pr_info("Completed");
				max77958_update_configuration(max77958);
				break;
			default:
				offset += size;
				break;
			}
			if (offset == fw_bin_len) {
				max77958_reset_ic(max77958);
				max77958_read_reg(max77958->i2c,
					REG_UIC_FW_REV, &max77958->FW_Revision);
				max77958_read_reg(max77958->i2c,
					REG_UIC_FW_SUBREV,
					&max77958->FW_Minor_Revision);
				pr_info("chip : %02X.%02X, FW : %02X.%02X",
					max77958->FW_Revision,
					max77958->FW_Minor_Revision,
					fw_header.data4, fw_header.data5);
				max77958_update_configuration(max77958);
				pr_info("Completed via SYS path");
			}
		}
	} else {
		pr_info("Don't need to update!");
		goto out;
	}

	duration = jiffies - duration;
	pr_info("Duration : %dms", jiffies_to_msecs(duration));
out:
	enable_irq(max77958->irq);
	return 0;
}
EXPORT_SYMBOL_GPL(max77958_usbc_fw_update);
/*#endif*/

static int max77958_i2c_probe(struct i2c_client *i2c,
		const struct i2c_device_id *dev_id)
{
	struct max77958_dev *max77958;
	struct max77958_platform_data *pdata = i2c->dev.platform_data;

	int ret = 0;

	pr_info("%s:%s\n", MFD_DEV_NAME, __func__);

	max77958 = kzalloc(sizeof(struct max77958_dev), GFP_KERNEL);
	if (!max77958)
		return -ENOMEM;

	if (i2c->dev.of_node) {
		pdata = devm_kzalloc(&i2c->dev,
			sizeof(struct max77958_platform_data),
			GFP_KERNEL);
		if (!pdata) {
			ret = -ENOMEM;
			goto err;
		}

		ret = of_max77958_dt(&i2c->dev, pdata);
		if (ret < 0) {
			dev_err(&i2c->dev, "Failed to get device of_node\n");
			goto err;
		}

		i2c->dev.platform_data = pdata;
	} else
		pdata = i2c->dev.platform_data;

	max77958->dev = &i2c->dev;
	max77958->i2c = i2c;
	max77958->irq = i2c->irq;
	if (pdata) {
		max77958->pdata = pdata;

		pdata->irq_base = irq_alloc_descs(-1, 0, MAX77958_IRQ_NR, -1);
		if (pdata->irq_base < 0) {
			pr_err("%s:%s irq_alloc_descs Fail! ret(%d)\n",
				MFD_DEV_NAME,	__func__, pdata->irq_base);
			ret = -EINVAL;
			goto err;
		} else
			max77958->irq_base = pdata->irq_base;

		max77958->wakeup = pdata->wakeup;
	} else {
		ret = -EINVAL;
		goto err;
	}
	mutex_init(&max77958->i2c_lock);

	i2c_set_clientdata(i2c, max77958);

	init_completion(&max77958->fw_completion);
	max77958->fw_workqueue = create_singlethread_workqueue("fw_update");
	if (max77958->fw_workqueue == NULL)
		return -ENOMEM;
	INIT_WORK(&max77958->fw_work, max77958_usbc_wait_response_q);
	max77958_usbc_fw_update(max77958, BOOT_FLASH_FW_PASS2,
		ARRAY_SIZE(BOOT_FLASH_FW_PASS2), 0);
	max77958_update_configuration(max77958);
	ret = max77958_irq_init(max77958);
	disable_irq(max77958->irq);

	if (ret < 0)
		goto err_irq_init;
	ret = mfd_add_devices(max77958->dev, -1, max77958_devs,
			ARRAY_SIZE(max77958_devs), NULL, 0, NULL);
	if (ret < 0)
		goto err_mfd;

	device_init_wakeup(max77958->dev, pdata->wakeup);
	max77958->boot_complete = 1;

	return ret;

err_mfd:
	mfd_remove_devices(max77958->dev);
err_irq_init:
	i2c_unregister_device(max77958->i2c);
	mutex_destroy(&max77958->i2c_lock);
err:
	kfree(max77958);
	return ret;
}

static void max77958_i2c_remove(struct i2c_client *i2c)
{
	struct max77958_dev *max77958 = i2c_get_clientdata(i2c);

	device_init_wakeup(max77958->dev, 0);
	max77958_irq_exit(max77958);
	mfd_remove_devices(max77958->dev);
	i2c_unregister_device(max77958->i2c);
	kfree(max77958);
}

static const struct i2c_device_id max77958_i2c_id[] = {
	{ MFD_DEV_NAME, TYPE_MAX77958 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max77958_i2c_id);

#if defined(CONFIG_OF)
static const struct of_device_id max77958_i2c_dt_ids[] = {
	{ .compatible = MFD_DEV_NAME },
	{ },
};
MODULE_DEVICE_TABLE(of, max77958_i2c_dt_ids);
#endif /* CONFIG_OF */

#if defined(CONFIG_PM)
static int max77958_suspend(struct device *dev)
{
	pr_info("%s:%s\n", MFD_DEV_NAME, __func__);
	return 0;
}

static int max77958_resume(struct device *dev)
{
	pr_info("%s:%s\n", MFD_DEV_NAME, __func__);
	return 0;
}
#else
#define max77958_suspend	NULL
#define max77958_resume		NULL
#endif /* CONFIG_PM */

#ifdef CONFIG_HIBERNATION
static int max77958_freeze(struct device *dev)
{
	pr_info("%s:%s\n", MFD_DEV_NAME, __func__);
	return 0;
}

static int max77958_restore(struct device *dev)
{
	pr_info("%s:%s\n", MFD_DEV_NAME, __func__);
	return 0;
}
#endif

const struct dev_pm_ops max77958_pm = {
	.suspend = max77958_suspend,
	.resume = max77958_resume,
#ifdef CONFIG_HIBERNATION
	.freeze =  max77958_freeze,
	.thaw = max77958_restore,
	.restore = max77958_restore,
#endif
};

static struct i2c_driver max77958_i2c_driver = {
	.driver		= {
		.name	= MFD_DEV_NAME,
		.owner	= THIS_MODULE,
#if defined(CONFIG_PM)
		.pm	= &max77958_pm,
#endif /* CONFIG_PM */
#if defined(CONFIG_OF)
		.of_match_table	= max77958_i2c_dt_ids,
#endif /* CONFIG_OF */
	},
	.probe		= max77958_i2c_probe,
	.remove		= max77958_i2c_remove,
	.id_table	= max77958_i2c_id,
};

static int __init max77958_i2c_init(void)
{
	pr_info("%s:%s\n", MFD_DEV_NAME, __func__);
	return i2c_add_driver(&max77958_i2c_driver);
}
/* init early so consumer devices can complete system boot */
subsys_initcall(max77958_i2c_init);

static void __exit max77958_i2c_exit(void)
{
	i2c_del_driver(&max77958_i2c_driver);
}
module_exit(max77958_i2c_exit);
