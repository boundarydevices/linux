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

#include <linux/version.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/mod_devicetable.h>
#include <linux/power_supply.h>
#include <linux/of.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/file.h>
#include <linux/syscalls.h>
#include <linux/sort.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
#include <linux/mfd/core.h>
#include <linux/mfd/max77958.h>
#include <linux/mfd/max77958-private.h>
#include <linux/usbc/max77958-usbc.h>
#if defined(CONFIG_CCIC_MAX77958_DEBUG)
#include <linux/usbc/max77958-debug.h>
#endif

struct max77958_usbc_platform_data *g_usbc_data;

#define MAXIM_DEFAULT_FW		"/sdcard/Firmware/usbpd/secure_max77958.bin"

static int max77958_i2c_opcode_write(struct max77958_usbc_platform_data *usbc_data,
		struct max77958_apcmd_data *cmd_data)
{
	int ret = 0;

	if (cmd_data->cmd_length > OPCODE_DATA_LENGTH + OPCODE_SIZE)
		return -EMSGSIZE;

	pr_info("opcode 0x%x, write_length %d",
			cmd_data->cmd[0], cmd_data->cmd_length);
	print_hex_dump(KERN_INFO, "max77958: opcode_write: ",
			DUMP_PREFIX_OFFSET, 16, 1, cmd_data->cmd,
			cmd_data->cmd_length, false);
	/* Write opcode and data */
	ret = max77958_bulk_write(usbc_data->i2c, OPCODE_WRITE,
			cmd_data->cmd_length, cmd_data->cmd);
	/* Write end of data by 0x00 */
	if (cmd_data->cmd_length < OPCODE_DATA_LENGTH + OPCODE_SIZE)
		max77958_write_reg(usbc_data->i2c, OPCODE_WRITE_END, 0x00);

	return ret;
}

static int max77958_i2c_opcode_read(struct max77958_usbc_platform_data *usbc_data,
		u8 opcode, u8 length, u8 *values)
{
	int size = 0;

	if (length > (OPCODE_DATA_LENGTH + OPCODE_SIZE))
		return -EMSGSIZE;

	/*
	 * We don't need to use opcode to get any feedback
	 */

	/* Read opcode data */
	size = max77958_bulk_read(usbc_data->i2c, OPCODE_READ,
			length, values);

	pr_info("opcode 0x%x, read_length %d, ret_error %d",
			opcode, length, size);
	print_hex_dump(KERN_INFO, "max77958: opcode_read: ",
			DUMP_PREFIX_OFFSET, 16, 1, values,
			length, false);
	return size;
}


static void max77958_reset_ccpd(struct max77958_usbc_platform_data *usbc_data)
{
	max77958_write_reg(usbc_data->i2c, 0x80, 0x0F);
	msleep(100); /* need 100ms delay */
}

static void max77958_copy_apcmd_data(struct max77958_apcmd_data *from,
	struct max77958_apcmd_data *to)
{
	if (from == NULL || to == NULL)
		return;
	*to = *from;
}

struct max77958_apcmd_node *max77958_alloc_apcmd_data(void)
{
	struct max77958_apcmd_node *node =
		kzalloc(sizeof(struct max77958_apcmd_node), GFP_KERNEL);

	if (node) {
		node->cmd_data.cmd[0] = OPCODE_RSVD;
		node->cmd_data.val = REG_NONE;
		node->cmd_data.mask = REG_NONE;
		node->cmd_data.reg = REG_NONE;
	}
	return node;
}

static struct max77958_apcmd_node *max77958_head_apcmd_data(struct max77958_apcmd_queue *cmd_queue)
{

	struct max77958_apcmd_node *node;

	if (list_empty(&cmd_queue->node_head)) {
		pr_info("%s: Queue, Empty!\n", __func__);
		return NULL;
	}

	node = list_entry(cmd_queue->node_head.next,
				struct max77958_apcmd_node,
				node);
	if (node == NULL) {
		pr_info("%s: node is Null\n", __func__);
		return NULL;
	}
	return node;
}

static void max77958_dequeue_apcmd_data(struct max77958_apcmd_queue *cmd_queue)
{

	struct max77958_apcmd_node *node = max77958_head_apcmd_data(cmd_queue);

	if (node)
		list_del(&node->node);
	kfree(node);
}

void max77958_clear_apcmd_queue(struct max77958_usbc_platform_data *usbc_data)
{
	struct max77958_apcmd_queue *cmd_queue = NULL;

	pr_debug("%s: enter\n", __func__);
	mutex_lock(&usbc_data->apcmd_lock);
	cmd_queue = &(usbc_data->apcmd_queue);

	while (!list_empty(&cmd_queue->node_head)) {
		max77958_dequeue_apcmd_data(cmd_queue);
	}

	mutex_unlock(&usbc_data->apcmd_lock);
	pr_debug("%s: exit\n", __func__);
}

static void max77958_send_apcmd(struct max77958_usbc_platform_data *data)
{
	struct max77958_apcmd_node *node;
	struct max77958_apcmd_queue *cmd_queue = NULL;
	struct max77958_apcmd_data *cmd_data;

	cmd_queue = &(data->apcmd_queue);

	node = max77958_head_apcmd_data(cmd_queue);
	if (node) {
		cmd_data = &node->cmd_data;
		max77958_copy_apcmd_data(cmd_data, &(data->last_apcmd));
		if (cmd_data->cmd_length) {
			max77958_i2c_opcode_write(data, cmd_data);
			cmd_data->cmd_length = 0;
		}
	}
}

void max77958_queue_apcmd(struct max77958_usbc_platform_data *data,
		struct max77958_apcmd_node *node)
{
	struct max77958_apcmd_queue *cmd_queue = &(data->apcmd_queue);

	mutex_lock(&data->apcmd_lock);
	list_add_tail(&node->node, &cmd_queue->node_head);

	/* Start sending if list was empty */
	if (cmd_queue->node_head.next == &node->node)
		max77958_send_apcmd(data);
	mutex_unlock(&data->apcmd_lock);
}

void max77958_vendor_msg_response(struct max77958_usbc_platform_data
	*usbpd_data, char *apcmd_data)
{
	unsigned char vendor_msg[OPCODE_DATA_LENGTH] = {0,};
	union VDM_HEADER_Type vendor_msg_header;

	memcpy(vendor_msg, apcmd_data, OPCODE_DATA_LENGTH);
	memcpy(&vendor_msg_header, &vendor_msg[4], sizeof(vendor_msg_header));
	if ((vendor_msg_header.BITS.VDM_command_type) == VDM_RESPONDER_NAK) {
		pr_info("Vendor Define message is NAK[%d]", vendor_msg[1]);
		return;
	}

	switch (vendor_msg[1]) {
	case OPCODE_ID_VDM_DISCOVER_IDENTITY:
		pr_info("VDM_DISCOVER_IDENTITY");
		break;
	case OPCODE_ID_VDM_DISCOVER_SVIDS:
		pr_info("VDM_DISCOVER_SVIDS");
		break;
	case OPCODE_ID_VDM_DISCOVER_MODES:
		pr_info("VDM_DISCOVER_MODES");
		break;
	case OPCODE_ID_VDM_ENTER_MODE:
		pr_info("VDM_ENTER_MODE");
		break;
	case OPCODE_ID_VDM_SVID_DP_STATUS:
		pr_info("VDM_SVID_DP_STATUS");
		break;
	case OPCODE_ID_VDM_SVID_DP_CONFIGURE:
		pr_info("VDM_SVID_DP_CONFIGURE");
		break;
	case OPCODE_ID_VDM_ATTENTION:
		pr_info("VDM_ATTENTION");
		break;
	case OPCODE_ID_VDM_EXIT_MODE:
		pr_info("VDM_EXIT_MODE");
		break;

	default:
		break;
	}
}

static void max77958_check_apcmd(struct max77958_usbc_platform_data *usbc_data,
		const struct max77958_apcmd_data *cmd_data)
{
	int len = cmd_data->rsp_length;
	unsigned char data[OPCODE_DATA_LENGTH + 4] = {0,};
	u8 response = 0xff;

	if (!len)
		return;
	max77958_i2c_opcode_read(usbc_data, cmd_data->cmd[0],
			len, data);

	/* opcode identifying the messsage type. (0x51)*/
	response = data[0];

	if (response != cmd_data->cmd[0]) {
		pr_info("Response [0x%02x] != [0x%02x]",
				response, cmd_data->cmd[0]);
	}

	/* to do(read switch case) */
	switch (response) {
	case OPCODE_BC_CTRL1_R:
	case OPCODE_BC_CTRL1_W:
	case OPCODE_BC_CTRL2_R:
	case OPCODE_BC_CTRL2_W:
	case OPCODE_CONTROL1_R:
	case OPCODE_CONTROL1_W:
	case OPCODE_CCCONTROL1_R:
	case OPCODE_CCCONTROL1_W:
	case OPCODE_CCCONTROL2_R:
	case OPCODE_CCCONTROL2_W:
	case OPCODE_CCCONTROL3_R:
	case OPCODE_CCCONTROL3_W:
	case OPCODE_CCCONTROL4_R:
	case OPCODE_CCCONTROL4_W:
	case OPCODE_VCONN_ILIM_R:
	case OPCODE_VCONN_ILIM_W:
	case OPCODE_CCCONTROL5_R:
	case OPCODE_CCCONTROL5_W:
	case OPCODE_CCCONTROL6_R:
	case OPCODE_CCCONTROL6_W:
	case OPCODE_GET_SINK_CAP:
		break;
	case OPCODE_CURRENT_SRC_CAP:
		max77958_current_pdo(usbc_data, data);
		break;
	case OPCODE_GET_SOURCE_CAP:
	case OPCODE_SRCCAP_REQ:
	case OPCODE_SET_SOURCE_CAP:
	case OPCODE_SEND_GET_REQ:
	case OPCODE_READ_GET_REQ_RESP:
	case OPCODE_SEND_GET_RESP:
	case OPCODE_SWAP_REQ:
	case OPCODE_SWAP_RESP:
	case OPCODE_VDM_DISCOVER_ID_RESP:
	case OPCODE_VDM_DISCOVER_ID_REQ:
	case OPCODE_VDM_DISCOVER_SVID_RESP:
	case OPCODE_VDM_DISCOVER_SVID_REQ:
	case OPCODE_VDM_DISCOVER_MODE_RESP:
	case OPCODE_VDM_DISCOVER_MODE_REQ:
	case OPCODE_VDM_ENTER_MODE_REQ:
	case OPCODE_VDM_EXIT_MODE_REQ:
	case OPCODE_SEND_VDM:
	case OPCODE_GET_PD_MSG:
		break;
	case OPCODE_GET_VDM_RESP:
		max77958_vendor_msg_response(usbc_data, data);
		break;
	case OPCODE_SET_CSTM_INFORMATION_R:
	case OPCODE_SET_CSTM_INFORMATION_W:
	case OPCODE_SET_DEVCONFG_INFORMATION_R:
	case OPCODE_SET_DEVCONFG_INFORMATION_W:
	case OPCODE_ACTION_BLOCK_MTP_UPDATE:
		break;

	case OPCODE_MASTER_I2C_READ:
	case OPCODE_MASTER_I2C_WRITE:
		break;

	default:
		break;
	}
}

static void max77958_execute_apcmd(struct max77958_usbc_platform_data *data)
{
	struct max77958_apcmd_node *node;
	struct max77958_apcmd_queue *cmd_queue = NULL;

	cmd_queue = &(data->apcmd_queue);
	if (list_empty(&cmd_queue->node_head)) {
		pr_info("Queue, Empty");
		return;
	}

	node = max77958_head_apcmd_data(cmd_queue);
	if (node) {
		if (node->cmd_data.rsp_length)
			max77958_check_apcmd(data, &node->cmd_data);
		max77958_dequeue_apcmd_data(cmd_queue);

		max77958_send_apcmd(data);
	}
}


static void max77958_control_vbus(struct work_struct *work)
{
	struct max77958_usbc_platform_data *usbpd_data = g_usbc_data;

	pr_info("current_pr=%d", usbpd_data->cc_data->current_pr);
	if (usbpd_data->cc_data->current_pr == CC_SRC)
		max77958_vbus_turn_on_ctrl(usbpd_data, 1);
}

static int max77958_firmware_update(struct max77958_usbc_platform_data *data)
{
	struct max77958_usbc_platform_data *usbc_data = data;
	unsigned char *fw_data;
	struct max77958_fw_header fw_header;
	struct file *fp;
	long fw_size, nread;
	int error = 0;
	const u8 *fw_bin;
	int fw_bin_len;
	u8 fw_enable = 0;

	if (!usbc_data) {
		pr_info("usbc_data is null!!");
		return -ENODEV;
	}

	fp = filp_open(MAXIM_DEFAULT_FW, O_RDONLY, S_IRUSR);
	if (IS_ERR(fp)) {
		pr_info("failed to open %s.", MAXIM_DEFAULT_FW);
		error = -ENOENT;
		goto open_err;
	}

	fw_size = fp->f_path.dentry->d_inode->i_size;

	if (fw_size > 0) {
		fw_data = kzalloc(fw_size, GFP_KERNEL);
		nread = vfs_read(fp, (char __user *)fw_data,
			fw_size, &fp->f_pos);

		pr_info("start, file path %s, size %ld bytes",
				MAXIM_DEFAULT_FW, fw_size);
		filp_close(fp, NULL);

		if (nread != fw_size) {
			pr_info("failed to read fw file, nread %ld bytes",
					nread);
			error = -EIO;
		} else {
			fw_bin = fw_data;
			fw_bin_len = fw_size;
			/* Copy fw header */
			memcpy(&fw_header, fw_bin, FW_HEADER_SIZE);
			max77958_read_reg(usbc_data->i2c,
					REG_UIC_FW_REV,
					&usbc_data->FW_Revision);
			max77958_read_reg(usbc_data->i2c,
					REG_UIC_FW_SUBREV,
					&usbc_data->FW_Minor_Revision);
			pr_info("chip %02X.%02X, fw %02X.%02X",
					usbc_data->FW_Revision,
					usbc_data->FW_Minor_Revision,
					fw_header.data4, fw_header.data5);
			fw_enable = 1;
			if (fw_enable)
				max77958_usbc_fw_update(usbc_data->max77958,
					fw_bin, fw_bin_len, 1);
			else
				pr_info("failed the I2C communication");
			goto done;
		}
	}
done:
	kfree(fw_data);
open_err:
	return error;
}


void max77958_firmware_update_sys_file(struct max77958_usbc_platform_data
	*usbpd_data)
{
	usbpd_data->fw_update = 1;
	max77958_write_reg(usbpd_data->i2c, REG_PD_INT_M, 0xFF);
	max77958_write_reg(usbpd_data->i2c, REG_CC_INT_M, 0xFF);
	max77958_write_reg(usbpd_data->i2c, REG_UIC_INT_M, 0xFF);
	max77958_write_reg(usbpd_data->i2c, REG_ACTION_INT_M, 0xFF);
	max77958_firmware_update(usbpd_data);
	max77958_write_reg(usbpd_data->i2c, REG_UIC_INT_M, REG_UIC_INT_M_INIT);
	max77958_write_reg(usbpd_data->i2c, REG_CC_INT_M, REG_CC_INT_M_INIT);
	max77958_write_reg(usbpd_data->i2c, REG_PD_INT_M, REG_PD_INT_M_INIT);
	max77958_write_reg(usbpd_data->i2c, REG_ACTION_INT_M,
		REG_ACTION_INT_M_INIT);
	usbpd_data->fw_update = 0;
}

static void max77958_get_version_info(struct max77958_usbc_platform_data
	*usbc_data)
{
	u8 chip_info[4] = {0, };

	max77958_read_reg(usbc_data->i2c, REG_UIC_HW_REV, &chip_info[0]);
	max77958_read_reg(usbc_data->i2c, REG_UIC_DEVICE_ID, &chip_info[1]);
	max77958_read_reg(usbc_data->i2c, REG_UIC_FW_REV, &chip_info[2]);
	max77958_read_reg(usbc_data->i2c, REG_UIC_FW_SUBREV, &chip_info[3]);

	usbc_data->HW_Revision = chip_info[0];
	usbc_data->Device_Revision = chip_info[1];
	usbc_data->FW_Revision = chip_info[2];
	usbc_data->FW_Minor_Revision = chip_info[3];

	pr_info("HW rev is %02Xh, Dev rev is %02Xh, FW rev is %02X.%02X!",
			usbc_data->HW_Revision,
			usbc_data->Device_Revision,
			usbc_data->FW_Revision,
			usbc_data->FW_Minor_Revision);
}


void max77958_execute_sysmsg(struct max77958_usbc_platform_data
	*usbc_data, u8 sysmsg)
{
	u8 interrupt;

	if (usbc_data->shut_down) {
		pr_info("ignore the sysmsg during shutdown mode.");
		return;
	}

	switch (sysmsg) {
	case SYSERROR_NONE:
		break;
	case SYSERROR_BOOT_WDT:
		usbc_data->watchdog_count++;
		pr_info("SYSERROR_BOOT_WDT: %d", usbc_data->watchdog_count);
		max77958_write_reg(usbc_data->i2c,
			REG_UIC_INT_M, REG_UIC_INT_M_INIT);
		max77958_write_reg(usbc_data->i2c,
			REG_CC_INT_M, REG_CC_INT_M_INIT);
		max77958_write_reg(usbc_data->i2c,
			REG_PD_INT_M, REG_PD_INT_M_INIT);
		max77958_write_reg(usbc_data->i2c,
			REG_ACTION_INT_M, REG_ACTION_INT_M_INIT);
		max77958_clear_apcmd_queue(usbc_data);
		break;
	case SYSERROR_BOOT_SWRSTREQ:
		break;
	case SYSMSG_BOOT_POR:
		if (usbc_data->max77958->boot_complete) {
			usbc_data->por_count++;
			max77958_reset_ic(usbc_data->max77958);
			max77958_write_reg(usbc_data->i2c, REG_UIC_INT_M,
				REG_UIC_INT_M_INIT);
			max77958_write_reg(usbc_data->i2c, REG_CC_INT_M,
				REG_CC_INT_M_INIT);
			max77958_write_reg(usbc_data->i2c, REG_PD_INT_M,
				REG_PD_INT_M_INIT);
			max77958_write_reg(usbc_data->i2c, REG_ACTION_INT_M,
				REG_ACTION_INT_M_INIT);
			/* clear UIC_INT to prevent infinite sysmsg irq */
			max77958_read_reg(usbc_data->i2c,
				REG_UIC_INT, &interrupt);
			max77958_clear_apcmd_queue(usbc_data);
			pr_info("SYSERROR_BOOT_POR: %d, ",
				usbc_data->por_count);
			pr_info("UIC_INT:0x%02x", interrupt);
		}
		break;
	case SYSERROR_AFC_DONE:
		break;

	case SYSERROR_APCMD_UNKNOWN:
		break;
	case SYSERROR_APCMD_INPROGRESS:
		break;
	case SYSERROR_APCMD_FAIL:
		break;
	case SYSERROR_CCx_5V_SHORT:
		pr_info("CC-VBUS SHORT");
		break;
	default:
		break;
	}
}

static irqreturn_t max77958_apcmd_irq(int irq, void *data)
{
	struct max77958_usbc_platform_data *usbc_data = data;

	pr_debug("%s: enter irq%d\n", __func__, irq);
	max77958_read_reg(usbc_data->i2c, REG_USBC_STATUS2,
		&usbc_data->usbc_status2);
	mutex_lock(&usbc_data->apcmd_lock);
	max77958_execute_apcmd(usbc_data);
	mutex_unlock(&usbc_data->apcmd_lock);

	pr_debug("%s: exit usbc_status2=%02x\n", __func__, usbc_data->usbc_status2);
	return IRQ_HANDLED;
}

static irqreturn_t max77958_sysmsg_irq(int irq, void *data)
{
	struct max77958_usbc_platform_data *usbc_data = data;
	u8 i = 0;
	u8 raw_data[3] = {0, };
	u8 usbc_status2 = 0;

	pr_debug("%s: enter irq%d\n", __func__, irq);
	for (i = 0; i < 3; i++) {
		usbc_status2 = 0;
		max77958_read_reg(usbc_data->i2c, REG_USBC_STATUS2,
				&usbc_status2);
		raw_data[i] = usbc_status2;
	}
	if ((raw_data[0] != raw_data[1]) || (raw_data[0] != raw_data[2])) {
		pr_info("[W]sysmsg was changed suddenly, %02x %02x %02x",
			raw_data[0], raw_data[1], raw_data[2]);
	}
	max77958_execute_sysmsg(usbc_data, usbc_status2);
	usbc_data->sysmsg = usbc_status2;
	pr_debug("%s: exit usbc_status2=%02x\n", __func__, usbc_status2);
	return IRQ_HANDLED;
}



int max77958_init_irq_handler(struct max77958_usbc_platform_data *usbc_data)
{
	int ret = 0;

	wake_lock_init(&usbc_data->max77958_usbc_wake_lock, WAKE_LOCK_SUSPEND,
			"max77958usbc->wake_lock");

	usbc_data->irq_apcmd = usbc_data->irq_base + MAX77958_USBC_IRQ_APC_INT;
	if (usbc_data->irq_apcmd) {
		ret = request_threaded_irq(usbc_data->irq_apcmd,
				NULL, max77958_apcmd_irq,
				0,
				"usbc-apcmd-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n",
				__func__, ret);
			return ret;
		}
	}

	usbc_data->irq_sysmsg = usbc_data->irq_base
		+ MAX77958_USBC_IRQ_SYSM_INT;
	if (usbc_data->irq_sysmsg) {
		ret = request_threaded_irq(usbc_data->irq_sysmsg,
				NULL, max77958_sysmsg_irq,
				0,
				"usbc-sysmsg-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n",
				__func__, ret);
			return ret;
		}
	}


	return ret;
}

#ifndef CONFIG_MAX77958_ENABLE
#define I2C_ADDR_PMIC	(0xCC >> 1)	/* Top sys, Haptic */
static int max77705_i2c_master_write(struct max77958_usbc_platform_data
	*usbpd_data, int slave_addr, u8 *reg_addr)
{
	int err;
	int tries = 0;
	u8 buffer[2] = { reg_addr[0], reg_addr[1]};

	struct i2c_msg msgs[] = {
		{
			.addr = slave_addr,
			.flags = usbpd_data->i2c->flags & I2C_M_TEN,
			.len = 2,
			.buf = buffer,
		},
	};

	do {
		err = i2c_transfer(usbpd_data->i2c->adapter, msgs, 1);
		if (err < 0) {
			pr_info("i2c_transfer error:%d\n", err);
			pr_info("i2c_transfer addr : %x\n", reg_addr[0]);
			pr_info("i2c_transfer data : %x\n", reg_addr[1]);
		}
	} while ((err != 1) && (++tries < 20));

	if (err != 1) {
		pr_info("write transfer error:%d\n", err);
		pr_info("write transfer addr : %x\n", reg_addr[0]);
		pr_info("write transfer data : %x\n", reg_addr[1]);
		err = -EIO;
		return err;
	}

	return 1;
}

static void max77705_usbc_umask_irq(struct max77958_usbc_platform_data
	*usbpd_data)
{
	u8 ArrSendData[2] = {0x00, 0x00};

	ArrSendData[0] = 0x23;
	ArrSendData[1] = 0x3;
	max77705_i2c_master_write(usbpd_data, I2C_ADDR_PMIC,
		ArrSendData);
}
#endif
static int max77958_usbc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *of_node = dev->parent->of_node;
	struct max77958_dev *max77958 = dev_get_drvdata(pdev->dev.parent);
	struct max77958_platform_data *pdata = dev_get_platdata(max77958->dev);
	struct max77958_usbc_platform_data *usbc_data = NULL;

	pr_debug("%s:\n", __func__);
	usbc_data =  devm_kzalloc(dev, sizeof(struct max77958_usbc_platform_data),
		GFP_KERNEL);
	if (!usbc_data)
		return -ENOMEM;

	if (of_find_property(of_node, "charger-power-supply", NULL)) {
		usbc_data->psy_charger = devm_power_supply_get_by_phandle(
							dev->parent,
							"charger-power-supply");
		if (IS_ERR(usbc_data->psy_charger)) {
			dev_err(dev, "Couldn't get the charger power supply\n");
			return PTR_ERR(usbc_data->psy_charger);
		}

		if (!usbc_data->psy_charger)
			return -EPROBE_DEFER;
	}

	g_usbc_data = usbc_data;
	usbc_data->dev = &pdev->dev;
	usbc_data->max77958 = max77958;
	usbc_data->i2c = max77958->i2c;
	usbc_data->max77958_data = pdata;
	usbc_data->irq_base = pdata->irq_base;

	usbc_data->pd_data = devm_kzalloc(dev, sizeof(struct max77958_pd_data),
		GFP_KERNEL);
	if (!usbc_data->pd_data)
		return -ENOMEM;

	usbc_data->cc_data = devm_kzalloc(dev, sizeof(struct max77958_cc_data),
		GFP_KERNEL);
	if (!usbc_data->cc_data)
		return -ENOMEM;

	usbc_data->bc12_data = devm_kzalloc(dev, sizeof(struct max77958_bc12_data),
		GFP_KERNEL);
	if (!usbc_data->bc12_data)
		return -ENOMEM;

	platform_set_drvdata(pdev, usbc_data);

#if defined(CONFIG_CCIC_MAX77958_DEBUG)
	mxim_debug_init();
	mxim_debug_set_i2c_client(usbc_data->i2c);
#endif
	usbc_data->HW_Revision = 0x0;
	usbc_data->FW_Revision = 0x0;
	usbc_data->FW_Revision = 0x0;
	usbc_data->connected_device = 0x0;
	usbc_data->cc_data->current_pr = CC_UNKNOWN_STATE;
	usbc_data->pd_data->current_dr = USB_ROLE_NONE;
	usbc_data->cc_data->current_vcon = VCR_UNKNOWN_STATE;
	usbc_data->op_code_done = 0x0;
	usbc_data->current_wtrstat = WTR_UNKNOWN_STATE;
	usbc_data->prev_wtrstat = WTR_UNKNOWN_STATE;
	mutex_init(&usbc_data->apcmd_lock);
	INIT_LIST_HEAD(&usbc_data->apcmd_queue.node_head);
	max77958_get_version_info(usbc_data);
	max77958_init_irq_handler(usbc_data);
	max77958_cc_init(usbc_data);
	max77958_bc12_init(usbc_data);
	max77958_pd_init(usbc_data);
	INIT_DELAYED_WORK(&usbc_data->vbus_hard_reset_work,
			max77958_control_vbus);
	pr_info("probing Complete..!!!");
#ifndef CONFIG_MAX77958_ENABLE
	max77705_usbc_umask_irq(usbc_data);
#endif
	max77958->boot_complete = 1;
	enable_irq(max77958->irq);
	return 0;
}

static int max77958_usbc_remove(struct platform_device *pdev)
{
	struct max77958_usbc_platform_data *usbc_data =
		platform_get_drvdata(pdev);

#if defined(CONFIG_CCIC_MAX77958_DEBUG)
	mxim_debug_exit();
#endif
	wake_lock_destroy(&usbc_data->max77958_usbc_wake_lock);
	wake_lock_destroy(&usbc_data->pd_data->max77958_pd_wake_lock);
	wake_lock_destroy(&usbc_data->cc_data->max77958_cc_wake_lock);
	wake_lock_destroy(&usbc_data->bc12_data->max77958_bc12_wake_lock);
	free_irq(usbc_data->irq_apcmd, usbc_data);
	free_irq(usbc_data->irq_sysmsg, usbc_data);
	free_irq(usbc_data->irq_vdm0, usbc_data);
	free_irq(usbc_data->irq_vdm1, usbc_data);
	free_irq(usbc_data->irq_vdm2, usbc_data);
	free_irq(usbc_data->irq_vdm3, usbc_data);
	free_irq(usbc_data->irq_vdm4, usbc_data);
	free_irq(usbc_data->irq_vdm5, usbc_data);
	free_irq(usbc_data->irq_vdm6, usbc_data);
	free_irq(usbc_data->irq_vdm7, usbc_data);
	free_irq(usbc_data->pd_data->irq_pdmsg, usbc_data);
	free_irq(usbc_data->pd_data->irq_datarole, usbc_data);
	free_irq(usbc_data->pd_data->irq_fct_id, usbc_data);
	free_irq(usbc_data->cc_data->irq_vconncop, usbc_data);
	free_irq(usbc_data->cc_data->irq_vsafe0v, usbc_data);
	free_irq(usbc_data->cc_data->irq_detabrt, usbc_data);
	free_irq(usbc_data->cc_data->irq_wtr, usbc_data);
	free_irq(usbc_data->cc_data->irq_ccpinstat, usbc_data);
	free_irq(usbc_data->cc_data->irq_ccistat, usbc_data);
	free_irq(usbc_data->cc_data->irq_ccvcnstat, usbc_data);
	free_irq(usbc_data->cc_data->irq_ccstat, usbc_data);

	free_irq(usbc_data->bc12_data->irq_vbusdet, usbc_data);
	free_irq(usbc_data->bc12_data->irq_dcdtmo, usbc_data);
	free_irq(usbc_data->bc12_data->irq_chgtype, usbc_data);
	free_irq(usbc_data->bc12_data->irq_vbadc, usbc_data);
	return 0;
}

#if defined CONFIG_PM
static int max77958_usbc_suspend(struct device *dev)
{
	return 0;
}

static int max77958_usbc_resume(struct device *dev)
{
	return 0;
}
#else
#define max77958_usbc_suspend NULL
#define max77958_usbc_resume NULL
#endif

static void max77958_usbc_shutdown(struct platform_device *pdev)
{
	struct max77958_usbc_platform_data *usbc_data =
		platform_get_drvdata(pdev);

	pr_info("entering the shutdown");
	if (!usbc_data->i2c) {
		pr_info("no max77958 i2c client");
		return;
	}
	usbc_data->shut_down = 1;
	/* unmask */
	max77958_write_reg(usbc_data->i2c, REG_PD_INT_M, 0xFF);
	max77958_write_reg(usbc_data->i2c, REG_CC_INT_M, 0xFF);
	max77958_write_reg(usbc_data->i2c, REG_UIC_INT_M, 0xFF);
	max77958_write_reg(usbc_data->i2c, REG_ACTION_INT_M, 0xFF);
	/* send the reset command */
	if (usbc_data->current_wtrstat == WTR_WATER)
		pr_info("Skip the max77958_reset_ccpd function");
	else {
		disable_irq(usbc_data->max77958->irq);
		max77958_reset_ccpd(usbc_data);
		max77958_write_reg(usbc_data->i2c, REG_PD_INT_M, 0xFF);
		max77958_write_reg(usbc_data->i2c, REG_CC_INT_M, 0xFF);
		max77958_write_reg(usbc_data->i2c, REG_UIC_INT_M, 0xFF);
		max77958_write_reg(usbc_data->i2c, REG_ACTION_INT_M, 0xFF);
	}
	pr_info("exiting the shutdown");
}

static SIMPLE_DEV_PM_OPS(max77958_usbc_pm_ops, max77958_usbc_suspend,
		max77958_usbc_resume);

static struct platform_driver max77958_usbc_driver = {
	.driver = {
		.name = "max77958-usbc",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &max77958_usbc_pm_ops,
#endif
	},
	.shutdown = max77958_usbc_shutdown,
	.probe = max77958_usbc_probe,
	.remove = max77958_usbc_remove,
};

static int __init max77958_usbc_init(void)
{
	pr_info("init");
	return platform_driver_register(&max77958_usbc_driver);
}
module_init(max77958_usbc_init);

static void __exit max77958_usbc_exit(void)
{
	platform_driver_unregister(&max77958_usbc_driver);
}
module_exit(max77958_usbc_exit);

MODULE_DESCRIPTION("max77958 USBPD driver");
MODULE_LICENSE("GPL");
