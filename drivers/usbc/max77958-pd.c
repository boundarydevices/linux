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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/mod_devicetable.h>
#include <linux/power_supply.h>
#include <linux/of.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/mfd/max77958-private.h>
#include <linux/usbc/max77958-usbc.h>

#define STRUCTURED_VDM          1
#define OPCODE_HEADER_SIZE      1

enum {
	Reserved		= 0,
	Discover_Identity	= 1,
	Discover_SVIDs	=  2,
	Discover_Modes	= 3,
	Enter_Mode		= 4,
	Exit_Mode		= 5,
	Attention		= 6,
	Configure		= 17
};

enum {
	REQ			= 0,
	ACK			= 1,
	NAK			= 2,
	BUSY			= 3
};

enum {
	Version_1_0		= 0,
	Version_2_0		= 1,
	Reserved1		= 2,
	Reserved2		= 3
};


union send_vdm_byte_data {
	uint8_t        DATA;
	struct {
		uint8_t     BDATA[1];
	} BYTES;
	struct {
		uint8_t	Num_Of_VDO:3,
				Cmd_Type:2,
				Reserved:3;
	} BITS;
};


union und_data_msg_vdm_header_type {
	uint32_t        DATA;
	struct {
		uint8_t     BDATA[4];
	} BYTES;
	struct {
		uint32_t	VDM_command:5,
				Rsvd2_VDM_header:1,
				VDM_command_type:2,
				Object_Position:3,
				Rsvd_VDM_header:2,
				Structured_VDM_Version:2,
				VDM_Type:1,
				Standard_Vendor_ID:16;
	} BITS;
};

struct VMD_DISCOVER_IDENTITY {
	union send_vdm_byte_data byte_data;
	union und_data_msg_vdm_header_type vdm_header;
} __attribute__((aligned(1), packed));


const struct VMD_DISCOVER_IDENTITY DISCOVER_IDENTITY = {
	{
		.BITS.Num_Of_VDO = 1,
		.BITS.Cmd_Type = ACK,
		.BITS.Reserved = 0
	},

	{
		.BITS.VDM_command = Discover_Identity,
		.BITS.Rsvd2_VDM_header = 0,
		.BITS.VDM_command_type = REQ,
		.BITS.Object_Position = 0,
		.BITS.Rsvd_VDM_header = 0,
		.BITS.Structured_VDM_Version = Version_1_0,
		.BITS.VDM_Type = STRUCTURED_VDM,
		.BITS.Standard_Vendor_ID = 0xFF00
	}
};

void max77958_vbus_turn_on_ctrl(struct max77958_usbc_platform_data
	*usbc_data, bool enable)
{
}

void max77958_notify_cci_vbus_current(struct max77958_usbc_platform_data
	*usbc_data)
{
	unsigned int rp_currentlvl;
	struct power_supply *psy_charger = usbc_data->psy_charger;
	union power_supply_propval val;

	switch (usbc_data->cc_data->ccistat) {
	case 1:
		rp_currentlvl = CCISTAT_NONE;
		break;
	case 2:
		rp_currentlvl = CCISTAT_500MA;
		break;
	case 3:
		rp_currentlvl = CCISTAT_1500MA;
		break;
	default:
		rp_currentlvl = CCISTAT_3000MA;
		break;
	}

	if (!usbc_data->pd_data->psrdy_received &&
			usbc_data->cc_data->current_pr == SNK) {
		if (psy_charger) {
			val.intval = rp_currentlvl;
			psy_charger->desc->set_property(psy_charger,
					POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
					&val);
		} else {
			pr_err("%s: Fail to get psy charger\n", __func__);
		}
	}
}


void max77958_select_pdo(void *data, int num)
{
	struct max77958_usbc_platform_data *usbpd_data = data;
	struct max77958_apcmd_data enqueue_data;

	max77958_init_apcmd_data(&enqueue_data);
	enqueue_data.opcode = OPCODE_SRCCAP_REQ;
	enqueue_data.write_data[0] = num;
	enqueue_data.write_length = 0x1;
	enqueue_data.read_length = 0x1;
	max77958_request_apcmd(usbpd_data, &enqueue_data);
}

static int max77958_get_current_src_cap(void *data)
{
	struct max77958_usbc_platform_data *usbpd_data = data;
	struct max77958_apcmd_data enqueue_data;

	max77958_init_apcmd_data(&enqueue_data);
	enqueue_data.opcode = OPCODE_CURRENT_SRC_CAP;
	enqueue_data.write_data[0] = 0x0;
	enqueue_data.write_length = 0x1;
	enqueue_data.read_length = 31;
	max77958_request_apcmd(usbpd_data, &enqueue_data);
	return 0;
}


void max77958_current_pdo(struct max77958_usbc_platform_data
	*usbc_data, unsigned char *data)
{
	int i;
	u8 pdo_num = 0x00;
	u32 pdo_value;
	int max_cur = 0;
	int max_vol = 0;

	pdo_num = ((data[1] >> 3) & 0x07);

	for (i = 0; i < pdo_num; i++) {
		pdo_value = 0;
		max_cur = 0;
		max_vol = 0;
		pdo_value = (data[2 + (i * 4)]
				| (data[3 + (i * 4)] << 8)
				| (data[4 + (i * 4)] << 16)
				| (data[5 + (i * 4)] << 24));
		max_cur = (0x3FF & pdo_value)*10;
		max_vol = (0x3FF & (pdo_value >> 10)) * 50;
		pr_info("PDO[%d] = 0x%x MAX_CURR(%d) MAX_VOLT(%d)",
				i, pdo_value, max_cur, max_vol);

	}
	usbc_data->pd_data->pdo_list = true;
}


static void max77958_pd_check_pdmsg(struct max77958_usbc_platform_data
		*usbc_data, u8 pd_msg)
{
	pr_info(" PD_MSG [%x]", pd_msg);

	switch (pd_msg) {
	case PDMSG_NOTHING_HAPPENED:
		pr_info("PDMSG_NOTHING_HAPPENED received.");
		break;
	case PDMSG_SNK_PSRDY_RECEIVED:
		pr_info("PDMSG_SNK_PSRDY_RECEIVED received.");
		break;
	case PDMSG_SNK_ERROR_RECOVERY:
		pr_info("PDMSG_SNK_ERROR_RECOVERY received.");
		break;
	case PDMSG_SNK_SENDERRESPONSETIMER_TIMEOUT:
		pr_info("PDMSG_SNK_SENDERRESPONSETIMER_TIMEOUT received.");
		break;
	case PDMSG_SRC_PSRDY_SENT:
		pr_info("PDMSG_SRC_PSRDY_SENT received.");
		break;
	case PDMSG_SRC_ERROR_RECOVERY:
		pr_info("PDMSG_SRC_PSRDY_SENT received.");
		break;
	case PDMSG_DR_SWAP_REQ_RECEIVED:
		pr_info("PDMSG_DR_SWAP_REQ_RECEIVED received.");
		break;
	case PDMSG_PR_SWAP_REQ_RECEIVED:
		pr_info("PDMSG_PR_SWAP_REQ_RECEIVED received.");
		break;
	case PDMSG_VCONN_SWAP_REQ_RECEIVED:
		pr_info("PDMSG_VCONN_SWAP_REQ_RECEIVED received.");
		break;
	case PDMSG_VDM_ATTENTION_MSG_RECEIVED:
		pr_info("PDMSG_VDM_ATTENTION_MSG_RECEIVED received.");
		break;
	case PDMSG_REJECT_RECEIVED:
		pr_info("PDMSG_REJECT_RECEIVED received.");
		break;
	case PDMSG_SNK_DISABLED:
		pr_info("PDMSG_SNK_DISABLED received.");
		break;
	case PDMSG_SRC_DISABLED:
		pr_info("PDMSG_SRC_DISABLED received.");
		break;
	case PDMSG_VDM_NAK_RECEIVED:
		pr_info("PDMSG_VDM_NAK_RECEIVED received.");
		break;
	case PDMSG_VDM_BUSY_RECEIVED:
		pr_info("PDMSG_VDM_BUSY_RECEIVED received.");
		break;
	case PDMSG_VDM_ACK_RECEIVED:
		pr_info("PDMSG_VDM_ACK_RECEIVED received.");
		break;
	case PDMSG_VDM_REQ_RECEIVED:
		pr_info("PDMSG_VDM_REQ_RECEIVED received.");
		break;
	case PDMSG_VDM_DISCOVERMODEs:
		pr_info("PDMSG_VDM_DISCOVERMODEs received.");
		break;
	case PDMSG_VDM_DP_STATUS:
		pr_info("PDMSG_VDM_DP_STATUS received.");
		break;
	case PDMSG_PRSWAP_SNKTOSRC_SENT:
		pr_info("PDMSG_PRSWAP_SNKTOSRC_SENT received.");
		usbc_data->pd_pr_swap = CC_SRC;
		break;
	case PDMSG_PRSWAP_SRCTOSNK_SENT:
		pr_info("PDMSG_PRSWAP_SRCTOSNK_SENT received.");
		usbc_data->pd_pr_swap = CC_SNK;
		break;
	case PDMSG_SRC_SENDERRESPONSETIMER_TIMEOUT:
		pr_info("PDMSG_SRC_SENDERRESPONSETIMER_TIMEOUT received.");
		max77958_vbus_turn_on_ctrl(usbc_data, OFF);
		schedule_delayed_work(&usbc_data->vbus_hard_reset_work,
			msecs_to_jiffies(800));
		break;
	case PDMSG_HARDRESET_RECEIVED:
		/*turn off the vbus both Source and Sink*/
		if (usbc_data->cc_data->current_pr == CC_SRC) {
			max77958_vbus_turn_on_ctrl(usbc_data, OFF);
			schedule_delayed_work(&usbc_data->vbus_hard_reset_work,
				msecs_to_jiffies(760));
		}
		break;
	case PDMSG_HARDRESET_SENT:
		/*turn off the vbus both Source and Sink*/
		if (usbc_data->cc_data->current_pr == CC_SRC) {
			max77958_vbus_turn_on_ctrl(usbc_data, OFF);
			schedule_delayed_work(&usbc_data->vbus_hard_reset_work,
				msecs_to_jiffies(760));
		}
		break;
	case PDMSG_PRSWAP_SRCTOSWAP:
	case PDMSG_PRSWAP_SWAPTOSNK:
		max77958_vbus_turn_on_ctrl(usbc_data, OFF);
		pr_info("PDMSG_PRSWAP_SRCTOSWAPSNK : [%x]", pd_msg);
		break;
	case PDMSG_PRSWAP_SNKTOSWAP:
		pr_info("PDMSG_PRSWAP_SNKTOSWAP received");
		/* CHGINSEL disable */
		break;
	case PDMSG_PRSWAP_SWAPTOSRC:
		max77958_vbus_turn_on_ctrl(usbc_data, ON);
		pr_info("PDMSG_PRSWAP_SWAPTOSRC received");
		break;
	default:
		break;
	}
}

static irqreturn_t max77958_pdmsg_irq(int irq, void *data)
{
	struct max77958_usbc_platform_data *usbc_data = data;
	struct max77958_pd_data *pd_data = usbc_data->pd_data;
	u8 pdmsg = 0;

	max77958_read_reg(usbc_data->i2c, REG_PD_STATUS0, &pd_data->pd_status0);
	pdmsg = pd_data->pd_status0;
	pr_info("IN");
	max77958_pd_check_pdmsg(usbc_data, pdmsg);
	pd_data->pdsmg = pdmsg;
	pr_info("OUT");
	return IRQ_HANDLED;
}

static irqreturn_t max77958_psrdy_irq(int irq, void *data)
{
	struct max77958_usbc_platform_data *usbc_data = data;
	struct max77958_pd_data *pd_data = usbc_data->pd_data;
	u8 cc_status = pd_data->cc_status;
	u8 psrdy = 0;

	pr_info("IN");

	max77958_read_reg(usbc_data->i2c, REG_PD_STATUS1, &pd_data->pd_status1);
	psrdy = (pd_data->pd_status1 & BIT_PSRDY)
		>> FFS(BIT_PSRDY);

	if (psrdy) {
		switch (cc_status) {
		case CC_SNK:
			usbc_data->pd_data->psrdy_received = true;
			max77958_get_current_src_cap(usbc_data);
			break;

		case CC_SRC:
			pr_info("Sent the PSRDY_IRQ(Source)");
			break;

		default:
			pr_info("Ignore the previous PSRDY_IRQ");
			break;
		}
	}
	pr_info("OUT");
	return IRQ_HANDLED;
}

void max77958_send_disocvery_identify(void *data)
{
	struct max77958_usbc_platform_data *usbpd_data = data;
	struct max77958_apcmd_data enqueue_data;
	int len = sizeof(DISCOVER_IDENTITY);
	int vdm_header_num = sizeof(union und_data_msg_vdm_header_type);
	int vdo0_num = 0;

	max77958_init_apcmd_data(&enqueue_data);
	enqueue_data.opcode = OPCODE_SEND_VDM;
	memcpy(&enqueue_data.write_data[0], &DISCOVER_IDENTITY,
		sizeof(DISCOVER_IDENTITY));
	enqueue_data.write_length = len;
	vdo0_num = DISCOVER_IDENTITY.byte_data.BITS.Num_Of_VDO * 4;
	enqueue_data.read_length = OPCODE_SIZE + OPCODE_HEADER_SIZE
		+ vdm_header_num + vdo0_num;
	max77958_request_apcmd(usbpd_data, &enqueue_data);
}

static void max77958_datarole_irq_handler(void *data, int irq)
{
	struct max77958_usbc_platform_data *usbc_data = data;
	struct max77958_pd_data *pd_data = usbc_data->pd_data;
	u8 datarole = 0;

	max77958_read_reg(usbc_data->i2c, REG_PD_STATUS1, &pd_data->pd_status1);
	datarole = (pd_data->pd_status1 & BIT_DataRole)
		>> FFS(BIT_DataRole);
	/* abnormal data role without setting power role */
	if (usbc_data->cc_data->current_pr == UNKNOWN_STATE) {
		pr_info("INVALID IRQ IRQ(%d)_OUT", irq);
		return;
	}

	switch (datarole) {
	case UFP:
		if (pd_data->current_dr != UFP) {
			pd_data->previous_dr = pd_data->current_dr;
			pd_data->current_dr = UFP;
			if (pd_data->previous_dr != UNKNOWN_STATE) {
				pr_info("%s detach DFP usb connection\n",
					__func__);
				pr_info("%s and then attach UFP\n",
					__func__);
				pr_info("%s usb connection (DR_SWAP)\n",
					__func__);
			}
			max77958_notify_dr_status(usbc_data, 1);
		}
		pr_info("UFP");
		break;

	case DFP:
		if (pd_data->current_dr != DFP) {
			pd_data->previous_dr = pd_data->current_dr;
			pd_data->current_dr = DFP;
			if (pd_data->previous_dr != UNKNOWN_STATE) {
				pr_info("%s detach UFP usb connection\n",
					__func__);
				pr_info("%s and then attach DFP\n",
					__func__);
				pr_info("%s usb connection (DR_SWAP)\n",
					__func__);
			if (usbc_data->cc_data->current_pr == SNK)
				max77958_send_disocvery_identify(usbc_data);
			}
			max77958_notify_dr_status(usbc_data, 1);
		}
		pr_info("DFP");
		break;

	default:
		pr_info(" DATAROLE(Never Call this routine)");
		break;
	}
}

static irqreturn_t max77958_datarole_irq(int irq, void *data)
{
	pr_info("IN");
	max77958_datarole_irq_handler(data, irq);
	pr_info("OUT");
	return IRQ_HANDLED;
}

static int max77958_process_vdm_identity(void *data)
{
	struct max77958_usbc_platform_data *usbpd_data = data;
	struct max77958_apcmd_data enqueue_data;

	max77958_init_apcmd_data(&enqueue_data);
	enqueue_data.opcode = OPCODE_GET_VDM_RESP;
	enqueue_data.write_data[0] = OPCODE_ID_VDM_DISCOVER_IDENTITY;
	enqueue_data.write_length = 0x1;
	enqueue_data.read_length = 31;
	pr_info("max77958_process_vdm_identity");
	max77958_request_apcmd(usbpd_data, &enqueue_data);
	return 0;
}

static int max77958_process_vdm_svids(void *data)
{
	struct max77958_usbc_platform_data *usbpd_data = data;
	struct max77958_apcmd_data enqueue_data;

	max77958_init_apcmd_data(&enqueue_data);
	enqueue_data.opcode = OPCODE_GET_VDM_RESP;
	enqueue_data.write_data[0] = OPCODE_ID_VDM_DISCOVER_SVIDS;
	enqueue_data.write_length = 0x1;
	enqueue_data.read_length = 31;
	pr_info("max77958_process_vdm_svids");
	max77958_request_apcmd(usbpd_data, &enqueue_data);
	return 0;
}

static int max77958_process_vdm_modes(void *data)
{
	struct max77958_usbc_platform_data *usbpd_data = data;
	struct max77958_apcmd_data enqueue_data;

	max77958_init_apcmd_data(&enqueue_data);
	enqueue_data.opcode = OPCODE_GET_VDM_RESP;
	enqueue_data.write_data[0] = OPCODE_ID_VDM_DISCOVER_MODES;
	enqueue_data.write_length = 0x1;
	enqueue_data.read_length = 11;
	pr_info("max77958_process_vdm_modes");
	max77958_request_apcmd(usbpd_data, &enqueue_data);
	return 0;
}

static int max77958_process_vdm_enter_mode(void *data)
{
	struct max77958_usbc_platform_data *usbpd_data = data;
	struct max77958_apcmd_data enqueue_data;

	max77958_init_apcmd_data(&enqueue_data);
	enqueue_data.opcode = OPCODE_GET_VDM_RESP;
	enqueue_data.write_data[0] = OPCODE_ID_VDM_ENTER_MODE;
	enqueue_data.write_length = 0x1;
	enqueue_data.read_length = 7;
	pr_info("max77958_process_vdm_enter_mode");
	max77958_request_apcmd(usbpd_data, &enqueue_data);
	return 0;
}

static int max77958_process_vdm_dp_status(void *data)
{
	struct max77958_usbc_platform_data *usbpd_data = data;
	struct max77958_apcmd_data enqueue_data;

	max77958_init_apcmd_data(&enqueue_data);
	enqueue_data.opcode = OPCODE_GET_VDM_RESP;
	enqueue_data.write_data[0] = OPCODE_ID_VDM_SVID_DP_STATUS;
	enqueue_data.write_length = 0x1;
	enqueue_data.read_length = 11;
	pr_info("max77958_process_vdm_dp_status");
	max77958_request_apcmd(usbpd_data, &enqueue_data);
	return 0;
}


static int max77958_process_vdm_dp_configure(void *data)
{
	struct max77958_usbc_platform_data *usbpd_data = data;
	struct max77958_apcmd_data enqueue_data;

	max77958_init_apcmd_data(&enqueue_data);
	enqueue_data.opcode = OPCODE_GET_VDM_RESP;
	enqueue_data.write_data[0] = OPCODE_ID_VDM_SVID_DP_CONFIGURE;
	enqueue_data.write_length = 0x1;
	enqueue_data.read_length = 11;
	pr_info("max77958_process_vdm_dp_configure");
	max77958_request_apcmd(usbpd_data, &enqueue_data);
	return 0;
}

static int max77958_process_vdm_attention(void *data)
{
	struct max77958_usbc_platform_data *usbpd_data = data;
	struct max77958_apcmd_data enqueue_data;

	max77958_init_apcmd_data(&enqueue_data);
	enqueue_data.opcode = OPCODE_GET_VDM_RESP;
	enqueue_data.write_data[0] = OPCODE_ID_VDM_ATTENTION;
	enqueue_data.write_length = 0x1;
	enqueue_data.read_length = 11;
	pr_info("max77958_process_vdm_attention");
	max77958_request_apcmd(usbpd_data, &enqueue_data);
	return 0;
}

static int max77958_process_vdm_exit_mode(void *data)
{
	struct max77958_usbc_platform_data *usbpd_data = data;
	struct max77958_apcmd_data enqueue_data;

	max77958_init_apcmd_data(&enqueue_data);
	enqueue_data.opcode = OPCODE_GET_VDM_RESP;
	enqueue_data.write_data[0] = OPCODE_ID_VDM_EXIT_MODE;
	enqueue_data.write_length = 0x1;
	enqueue_data.read_length = 7;
	pr_info("max77958_process_vdm_exit_mode");
	max77958_request_apcmd(usbpd_data, &enqueue_data);
	return 0;
}

static irqreturn_t max77958_vdm_irq(int irq, void *data)
{
	struct max77958_usbc_platform_data *usbc_data = data;
	struct max77958_pd_data *pd_data = usbc_data->pd_data;
	u8 mode = 0;

	pr_info("IN");

	max77958_read_reg(usbc_data->i2c, REG_DP_STATUS, &pd_data->dp_status);
	mode = pd_data->dp_status;

	if (mode & VDM_DISCOVER_ID)
		max77958_process_vdm_identity(usbc_data);
	if (mode & VDM_DISCOVER_SVIDS)
		max77958_process_vdm_svids(usbc_data);
	if (mode & VDM_DISCOVER_MODES)
		max77958_process_vdm_modes(usbc_data);
	if (mode & VDM_ENTER_MODE)
		max77958_process_vdm_enter_mode(usbc_data);
	if (mode & VDM_DP_STATUS_UPDATE)
		max77958_process_vdm_dp_status(usbc_data);
	if (mode & VDM_DP_CONFIGURE)
		max77958_process_vdm_dp_configure(usbc_data);
	if (mode & VDM_ATTENTION)
		max77958_process_vdm_attention(usbc_data);
	if (mode & VDM_EXIT_MODE)
		max77958_process_vdm_exit_mode(usbc_data);
	pr_info("OUT");
	return IRQ_HANDLED;
}
int max77958_pd_init(struct max77958_usbc_platform_data *usbc_data)
{
	struct max77958_pd_data *pd_data = NULL;
	int ret;

	pr_info(" IN");
	pd_data = usbc_data->pd_data;

	wake_lock_init(&pd_data->max77958_pd_wake_lock, WAKE_LOCK_SUSPEND,
			"max77958pd->wake_lock");

	pd_data->irq_pdmsg = usbc_data->irq_base
		+ MAX77958_PD_IRQ_PDMSG_INT;
	if (pd_data->irq_pdmsg) {
		ret = request_threaded_irq(pd_data->irq_pdmsg,
				NULL, max77958_pdmsg_irq,
				0,
				"pd-pdmsg-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n",
				__func__, ret);
			goto err_irq;
		}
	}

	pd_data->irq_psrdy = usbc_data->irq_base
		+ MAX77958_PD_IRQ_PS_RDY_INT;
	if (pd_data->irq_psrdy) {
		ret = request_threaded_irq(pd_data->irq_psrdy,
				NULL, max77958_psrdy_irq,
				0,
				"pd-psrdy-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n",
				__func__, ret);
			goto err_irq;
		}
	}

	pd_data->irq_datarole = usbc_data->irq_base
		+ MAX77958_PD_IRQ_DATAROLE_INT;
	if (pd_data->irq_datarole) {
		ret = request_threaded_irq(pd_data->irq_datarole,
				NULL, max77958_datarole_irq,
				0,
				"pd-datarole-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n",
				__func__, ret);
			goto err_irq;
		}
	}

	pd_data->irq_vdm = usbc_data->irq_base
		+ MAX77958_PD_IRQ_DISPLAYPORT_INT;
	if (pd_data->irq_vdm) {
		ret = request_threaded_irq(pd_data->irq_vdm,
				NULL, max77958_vdm_irq,
				0,
				"pd-vdm-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n",
				__func__, ret);
			goto err_irq;
		}
	}

	pr_info(" OUT");
	return 0;

err_irq:
	kfree(pd_data);
	return ret;
}
