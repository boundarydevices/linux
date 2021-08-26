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

#ifndef __LINUX_MFD_MAX77958_UIC_H
#define __LINUX_MFD_MAX77958_UIC_H
#include <linux/mfd/max77958.h>
#include <linux/usbc/max77958-pd.h>
#include <linux/usbc/max77958-cc.h>
#include <linux/usbc/max77958-bc12.h>

struct max77958_opcode {
	unsigned char opcode;
	unsigned char data[OPCODE_DATA_LENGTH];
	int read_length;
	int write_length;
};

#define REG_NONE			0xff

#define HWCUSTMINFOSIZE  12
#define CUSTMINFOSIZE    12

#define DISABLED 0
#define ENABLED 1

#define SRC	0
#define SNK	1
#define DRP	2

#define MoistureDetectionDisable 0
#define MoistureDetectionEnable 1

#define MemoryUpdateRAM 0
#define MemoryUpdateMTP 1

#define PD_VID 0x0B6A
#define PD_PID 0x6860

#define I2C_SID0 0x69
#define I2C_SID1 0x62
#define I2C_SID2 0x64
#define I2C_SID3 0x28

union max77958_custm_info {
	unsigned int CustmInfoArry[CUSTMINFOSIZE];

	struct {
		unsigned int debugSRC : 1;
		unsigned int debugSNK : 1;
		unsigned int AudioAcc : 1;
		unsigned int TrySNK : 1;
		unsigned int TypeCstate : 2;
		unsigned int notdef1 : 2;
		unsigned int VID : 16;
		unsigned int PID : 16;
		unsigned int srcPDOtype : 2;
		unsigned int srcPDOpeakCurrent : 2;
		unsigned int notdef2 : 4; //8
		unsigned int srcPDOVoltage : 16;
		unsigned int srcPDOMaxCurrent : 16;
		unsigned int srcPDOOverIDebounce : 16;
		unsigned int srcPDOOverIThreshold : 16;
		unsigned int snkPDOtype : 2;
		unsigned int notdef3 : 6;
		unsigned int snkPDOVoltage : 16;
		unsigned int snkPDOOperatingI : 16;
		unsigned int i2cSID1: 8;
		unsigned int i2cSID2: 8;
		unsigned int i2cSID3: 8;
		unsigned int i2cSID4: 8;
		unsigned short spare: 8;
	} custm_bits;
};

union max77958_device_info {
	unsigned short HWCustmInfoArry[HWCUSTMINFOSIZE];

	struct {
		unsigned short UcCtrl1Ctrl2_dflt;
		unsigned short GpCtrl1Ctrl2_dflt;
		unsigned short PD0_dflt;
		unsigned short PD1_dflt;
		unsigned short PO0_dflt;
		unsigned short PO1_dflt;
		unsigned short SupplyCfg;
		unsigned short PD0PD1_su;
		unsigned short PO0PO1_su;
		unsigned short PD0PD1_aftersu;
		unsigned short PO0PO1_aftersu;
		unsigned short sw_ctrl_dflt;
	} hwcustm_bits;
};

struct max77958_apcmd_data {
	u8	opcode;
	u8  prev_opcode;
	u8	response;
	u8	read_data[OPCODE_DATA_LENGTH];
	u8	write_data[OPCODE_DATA_LENGTH];
	int read_length;
	int write_length;
	u8	reg;
	u8	val;
	u8	mask;
	u8  seq;
	u8	is_uvdm;
	u8  is_chg_int;
	u8  is_chg_notify;
};

struct max77958_apcmd_node {
	struct max77958_apcmd_data cmd_data;
	struct list_head node;
};

struct max77958_apcmd_queue {
	struct mutex command_mutex;
	struct list_head node_head;
};


struct max77958_usbc_platform_data {
	struct max77958_dev *max77958;
	struct device *dev;
	struct i2c_client *i2c; /*0xCC */

	int irq_base;

	/* interrupt pin */
	int irq_apcmd;
	int irq_sysmsg;

	/* VDM pin */
	int irq_vdm0;
	int irq_vdm1;
	int irq_vdm2;
	int irq_vdm3;
	int irq_vdm4;
	int irq_vdm5;
	int irq_vdm6;
	int irq_vdm7;

	/* register information */
	u8 usbc_status1;
	u8 usbc_status2;
	u8 bc_status;
	u8 cc_status0;
	u8 cc_status1;
	u8 pd_status0;
	u8 pd_status1;

	int watchdog_count;
	int por_count;

	u8 opcode_res;
	/* USBC System message interrupt */
	u8 sysmsg;
	u8 pd_msg;

	/* F/W state */
	u8 HW_Revision;
	u8 Device_Revision;
	u8 FW_Revision;
	u8 FW_Minor_Revision;

	u8 connected_device;
	int op_code_done;


	/* F/W opcode command data */
	struct max77958_apcmd_queue apcmd_queue;
	struct max77958_apcmd_data last_apcmd;
	struct mutex apcmd_lock;

	enum max77958_wtrstat prev_wtrstat;
	enum max77958_wtrstat current_wtrstat;

	u8 power_role;

	struct max77958_bc12_data *bc12_data;
	struct max77958_pd_data *pd_data;
	struct max77958_cc_data *cc_data;
	struct max77958_platform_data *max77958_data;
	struct wake_lock max77958_usbc_wake_lock;
	int vbus_enable;
	int pd_pr_swap;
	int shut_down;
	int fw_update;
	struct delayed_work vbus_hard_reset_work;
	uint8_t ReadMSG[32];
	struct power_supply *psy_charger;
};

enum {
	OPCODE_ID_VDM_DISCOVER_IDENTITY = 0x1,
	OPCODE_ID_VDM_DISCOVER_SVIDS = 0x2,
	OPCODE_ID_VDM_DISCOVER_MODES = 0x3,
	OPCODE_ID_VDM_ENTER_MODE = 0x4,
	OPCODE_ID_VDM_EXIT_MODE = 0x5,
	OPCODE_ID_VDM_ATTENTION = 0x6,
	OPCODE_ID_VDM_SVID_DP_STATUS = 0x10,
	OPCODE_ID_VDM_SVID_DP_CONFIGURE = 0x11,
};

union VDM_HEADER_Type {
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

#define VDM_RESPONDER_ACK	0x1
#define VDM_RESPONDER_NAK	0x2
#define VDM_RESPONDER_BUSY	0x3


extern void max77958_clear_apcmd_queue(struct max77958_usbc_platform_data
	*usbc_data);
extern void max77958_request_apcmd(struct max77958_usbc_platform_data *data,
		struct max77958_apcmd_data *input_data);
extern void max77958_insert_apcmd(struct max77958_usbc_platform_data *data,
		struct max77958_apcmd_data *input_data);
extern void max77958_init_apcmd_data(struct max77958_apcmd_data *cmd_data);

extern void max77958_notify_cci_vbus_current(struct max77958_usbc_platform_data
	*usbc_data);
extern void max77958_vbus_turn_on_ctrl(struct max77958_usbc_platform_data
	*usbc_data, bool enable);
extern void max77958_notify_dr_status(struct max77958_usbc_platform_data
	*usbpd_data, uint8_t attach);

extern int max77958_cc_init(struct max77958_usbc_platform_data *usbc_data);
extern int max77958_bc12_init(struct max77958_usbc_platform_data *usbc_data);
extern int max77958_pd_init(struct max77958_usbc_platform_data *usbc_data);
extern void max77958_current_pdo(struct max77958_usbc_platform_data *usbc_data,
	unsigned char *data);
extern void max77958_reset_ic(struct max77958_dev *max77958);
#endif
