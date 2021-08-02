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



#ifndef __LINUX_MFD_MAX77958_H
#define __LINUX_MFD_MAX77958_H

#define MAX77958_CCPD_NAME	"MAX77958"

#undef  __CONST_FFS
#define __CONST_FFS(_x) \
	((_x) & 0x0F ? \
	 ((_x) & 0x03 ? ((_x) & 0x01 ? 0 : 1) : ((_x) & 0x04 ? 2 : 3)) : \
	 ((_x) & 0x30 ? ((_x) & 0x10 ? 4 : 5) : ((_x) & 0x40 ? 6 : 7)))

#undef FFS
#define FFS(_x) \
	((_x) ? __CONST_FFS(_x) : 0)

#undef  BIT_RSVD
#define BIT_RSVD  0

#undef  BITS
#define BITS(_end, _start) \
	((BIT(_end) - BIT(_start)) + BIT(_end))

#undef  __BITS_GET
#define __BITS_GET(_word, _mask, _shift) \
	(((_word) & (_mask)) >> (_shift))

#undef  BITS_GET
#define BITS_GET(_word, _bit) \
	__BITS_GET(_word, _bit, FFS(_bit))

#undef  __BITS_SET
#define __BITS_SET(_word, _mask, _shift, _val) \
	(((_word) & ~(_mask)) | (((_val) << (_shift)) & (_mask)))

#undef  BITS_SET
#define BITS_SET(_word, _bit, _val) \
	__BITS_SET(_word, _bit, FFS(_bit), _val)

#undef  BITS_MATCH
#define BITS_MATCH(_word, _bit) \
	(((_word) & (_bit)) == (_bit))

#define CONFIG_MAX77958_ENABLE
/*
 * Register address
 */
#ifdef CONFIG_MAX77958_ENABLE
#define	REG_UIC_HW_REV			0x00
#define	REG_UIC_DEVICE_ID		0x01
#define	REG_UIC_FW_REV			0x02
#define	REG_UIC_FW_SUBREV		0x03
#define	REG_UIC_INT				  0x04
#define	REG_CC_INT				  0x05
#define	REG_PD_INT				  0x06

#define	REG_ACTION_INT			0x07
#define	REG_USBC_STATUS1		0x08
#define	REG_USBC_STATUS2		0x09
#define	REG_BC_STATUS			  0x0A
#define REG_DP_STATUS       0x0B
#define	REG_CC_STATUS0			0x0C
#define	REG_CC_STATUS1			0x0D

#define	REG_PD_STATUS0			0x0E
#define	REG_PD_STATUS1			0x0F

#define	REG_UIC_INT_M			0x10
#define	REG_CC_INT_M			0x11
#define	REG_PD_INT_M			0x12
#define	REG_ACTION_INT_M	0x13
#else
#define	REG_UIC_HW_REV			0x00
#define	REG_UIC_DEVICE_ID		0x00
#define	REG_UIC_FW_REV			0x01
#define	REG_UIC_FW_SUBREV		0x09
#define	REG_UIC_INT				0x02
#define	REG_CC_INT				0x03
#define	REG_PD_INT				0x04
#define	REG_VDM_INT				0x05
#define	REG_USBC_STATUS1		0x06
#define	REG_USBC_STATUS2		0x07
#define	REG_BC_STATUS			0x08

#define	REG_CC_STATUS0			0x0A
#define	REG_CC_STATUS1			0x0B

#define	REG_PD_STATUS0			0x0C
#define	REG_PD_STATUS1			0x0D

#define	REG_UIC_INT_M			0x0E
#define	REG_CC_INT_M			0x0F
#define	REG_PD_INT_M			0x10
#define	REG_VDM_INT_M			0x11
#endif

#define REG_OPCODE				0x21
#define REG_OPCODE_DATA			0x22
#define REG_OPCDE_RES			0x51

#define REG_END_DATA			0x41

/*
 * REG_UIC_INT Interrupts
 */
#define BIT_APCmdResI			BIT(7)
#define BIT_SYSMsgI				BIT(6)
#define BIT_VBUSDetI			BIT(5)
#define BIT_VbADCI				BIT(4)
#define BIT_DCDTmoI				BIT(3)
#define BIT_FakeVBUSI			BIT(2)
#define BIT_CHGTypI				BIT(1)
#define BIT_Action4I			BIT(0)

/*
 * REG_CC_INT Interrupts
 */
#define BIT_VCONNOCPI			BIT(7)
#define BIT_VSAFE0VI			BIT(6)
#define BIT_AttachSrcErrI		BIT(5)
#define BIT_WTRI			    BIT(4)
#define BIT_CCPinStatI			BIT(3)
#define	BIT_CCIStatI			BIT(2)
#define	BIT_CCVcnStatI			BIT(1)
#define	BIT_CCStatI				BIT(0)

/*
 * REG_PD_INT Interrupts
 */
#define BIT_PDMsgI				BIT(7)
#define BIT_PSRdyI				BIT(6)
#define BIT_DataRoleI       BIT(5)
#define BIT_DisplayPortI		BIT(2)
#define BIT_SSAccI				BIT(1)
#define BIT_FCTIDI				BIT(0)


/*
 * REG_ACTION_INT Interrupts
 */
/*
	* #define BIT_Action3I			BIT(7)
	* #define BIT_Action2I			BIT(6)
	* #define BIT_Action1I			BIT(5)
	* #define BIT_Action0I			BIT(4)
	* #define BIT_VDM_EnterI			BIT(3)
	* #define BIT_VDM_ModeI			BIT(2)
	* #define BIT_VDM_SvidI			BIT(1)
	* #define BIT_VDM_IdentityI		BIT(0)
 */

#define BIT_Action_EnterI		BIT(3)
#define BIT_Action2I			  BIT(2)
#define BIT_Action1I			  BIT(1)
#define BIT_Action0I			  BIT(0)

/*
 * REG_USBC_STATUS1
 */
#define BIT_VBADC				BITS(7, 4)
#define BIT_FakeVbus			BIT(3)

/*
 * REG_USBC_STATUS2
 */
#define BIT_SYSMsg				BITS(7, 0)

/*
 * REG_BC_STATUS
 */
#define BIT_VBUSDet				BIT(7)
#define BIT_PrChgTyp			BITS(5, 3)
#define BIT_DCDTmo				BIT(2)
#define BIT_ChgTyp				BITS(1, 0)

/*
 * REG_DP_STATUS
 */
#define BIT_DP_ExitMode						BIT(7)
#define BIT_DP_Attention					BIT(6)
#define BIT_DP_Configure                BIT(5)
#define BIT_DP_Status					BIT(4)
#define BIT_DP_EnterMode						BIT(3)
#define BIT_DP_DiscoverMode		BIT(2)
#define BIT_DP_DiscoverSVID		BIT(1)
#define BIT_DP_DiscoverIdentity			BIT(0)

/*
 * REG_CC_STATUS0
 */
#define BIT_CCPinStat			BITS(7, 6)
#define BIT_CCIStat				BITS(5, 4)
#define BIT_CCVcnStat			BIT(3)
#define BIT_CCStat				BITS(2, 0)

/*
 * REG_CC_STATUS2
 */
#define BIT_VCONNOCP			BIT(5)
#define BIT_VCONNSC				BIT(4)
#define BIT_VSAFE0V				BIT(3)
#define BIT_AttachSrcErr		BIT(2)
#define BIT_WtrStat				BIT(1)


/*
 * REG_PD_STATUS0
 */
#define BIT_PDMsg				BITS(7, 0)

/*
 * REG_PD_STATUS1
 */
/* #define BIT_PD_DataRole			BIT(7) */
#define BIT_DataRole			BIT(7)
#define BIT_PowerRole			BIT(6)
#define BIT_VCONNS				BIT(5)
#define BIT_PSRDY				  BIT(4)
#define BIT_FCT_ID				BITS(3, 0)

#define DATA_ROLE_SWAP			0x1
#define POWER_ROLE_SWAP			0x2
#define VCONN_ROLE_SWAP			0x3
#define MANUAL_ROLE_SWAP		0x4

#define ROLE_ACCEPT				0x1
#define ROLE_REJECT				0x2
#define ROLE_BUSY				0x3

#define UNKNOWN_STATE			0xFF

/*
 * MAX77958 role
 */
enum max77958_data_role {
	UFP = 0,
	DFP,
};

enum max77958_vcon_role {
	OFF = 0,
	ON
};

enum max77958_power_role {
	SNK = 0,
	SRC
};


/*
 * F/W update
 */
#define FW_CMD_READ			0x3
#define FW_CMD_READ_SIZE	6	/* cmd(1) + len(1) + data(4) */

#define FW_CMD_WRITE		0x1
#define FW_CMD_WRITE_SIZE	36	/* cmd(1) + len(1) + data(34) */

#define FW_CMD_END			0x0

#define FW_HEADER_SIZE		8
#define FW_VERIFY_DATA_SIZE 3

#define FW_VERIFY_TRY_COUNT 10

#define FW_WAIT_TIMEOUT			(1000 * 5) /* 5 sec */
#define I2C_SMBUS_BLOCK_HALF	(I2C_SMBUS_BLOCK_MAX / 2)

struct max77958_fw_header {
	u8 data0;
	u8 data1;
	u8 data2;
	u8 data3;
	u8 data4; /* FW version LSB */
	u8 data5;
	u8 data6;
	u8 data7; /* FW version MSB */
};

enum {
	FW_UPDATE_START = 0x00,
	FW_UPDATE_WAIT_RESP_START,
	FW_UPDATE_WAIT_RESP_STOP,
	FW_UPDATE_DOING,
	FW_UPDATE_END,
};

enum {
	FW_UPDATE_FAIL = 0xF0,
	FW_UPDATE_I2C_FAIL,
	FW_UPDATE_TIMEOUT_FAIL,
	FW_UPDATE_VERIFY_FAIL,
	FW_UPDATE_CMD_FAIL,
	FW_UPDATE_MAX_LENGTH_FAIL,
};


enum {
	NO_DETERMINATION = 0,
	CC1_ACTIVE,
	CC2_ACTVIE,
};


/*
 * All type of Interrupts
 */

enum max77958_ccistat_type {
	CCISTAT_NONE,
	CCISTAT_500MA,
	CCISTAT_1500MA,
	CCISTAT_3000MA,
};

enum max77958_chg_type {
	CHGTYP_NOTHING = 0,
	CHGTYP_USB_SDP,
	CHGTYP_CDP,
	CHGTYP_DCP,
};

enum max77958_pr_chg_type {
	Unknown = 0,
	Samsung_2A,
	Apple_05A,
	Apple_1A,
	Apple_2A,
	Apple_12W,
	DCP_3A,
	RFU_CHG,
};

enum max77958_ccpd_device {
	DEV_NONE = 0,
	DEV_OTG,

	DEV_USB,
	DEV_CDP,
	DEV_DCP,

	DEV_APPLE500MA,
	DEV_APPLE1A,
	DEV_APPLE2A,
	DEV_APPLE12W,
	DEV_DCP3A,
	DEV_HVDCP,
	DEV_QC,

	DEV_FCT_0,
	DEV_FCT_1K,
	DEV_FCT_255K,
	DEV_FCT_301K,
	DEV_FCT_523K,
	DEV_FCT_619K,
	DEV_FCT_OPEN,

	DEV_PD_TA,
	DEV_PD_AMA,

	DEV_UNKNOWN,
};

enum max77958_fctid {
	FCT_GND = 1,
	FCT_1Kohm,
	FCT_255Kohm,
	FCT_301Kohm,
	FCT_523Kohm,
	FCT_619Kohm,
	FCT_OPEN,
};

enum max77958_cc_pin_state {
	CC_NO_CON = 0,
	CC_SNK,
	CC_SRC,
	CC_AUD_ACC,
	CC_DBG_SRC,
	CC_ERR,
	CC_DISABLED,
	CC_DBG_SNK,
	CC_RFU,
};

enum max77958_usbc_SYSMsg {
	SYSERROR_NONE					= 0x00,
	SYSERROR_BOOT_WDT				= 0x03,
	SYSERROR_BOOT_SWRSTREQ		    = 0x04,
	SYSMSG_BOOT_POR					= 0x05,
	SYSERROR_AFC_DONE				= 0x20,
	SYSERROR_APCMD_UNKNOWN			= 0x31,
	SYSERROR_APCMD_INPROGRESS		= 0x32,
	SYSERROR_APCMD_FAIL				= 0x33,
	SYSERROR_USBPD_RX_ERROR			= 0x50,
	SYSERROR_USBPD_TX_ERROR			= 0x51,
	SYSERROR_CCx_5V_SHORT			= 0x61,
	SYSERROR_SBUx_GND_SHORT			= 0x62,
	SYSERROR_SBUx_5V_SHORT			= 0x63,
	SYSERROR_UNKNOWN_RID_TWICE		= 0x70,
	SYSERROR_USER_PD_COLLISION		= 0x80,
	SYSERROR_MTP_WRITEFAIL			= 0xC0,
	SYSERROR_MTP_READFAIL		= 0xC1,
	SYSERROR_MTP_ERASEFAIL		= 0xC2,
	SYSERROR_MTP_CUSTMINFONOTSET	= 0xC3,
	SYSERROR_MTP_OVERACTIONBLKSIZE	= 0xC4,
	SYSERROR_FIRMWAREERROR			= 0xE0,
	SYSERROR_ACTION_UNKNOWN			= 0xE1,
	SYSERROR_ACTION_OFFSET			= 0xE2,
	SYSERROR_ACTION_UNKNOWN_CMD		= 0xE3,
	SYSERROR_EXECUTE_I2C_WRITEBURST_FAIL = 0xEA,
	SYSERROR_EXECUTE_I2C_READBURST_FAIL	= 0xEB,
	SYSERROR_EXECUTE_I2C_READ_FAIL			= 0xEC,
	SYSERROR_EXECUTE_I2C_WRITE_FAIL		= 0xED,
};

enum max77958_pdmsg {
	PDMSG_NOTHING_HAPPENED                = 0x00,
	PDMSG_SNK_PSRDY_RECEIVED              = 0x01,
	PDMSG_SNK_ERROR_RECOVERY              = 0x02,
	PDMSG_SNK_SENDERRESPONSETIMER_TIMEOUT = 0x03,
	PDMSG_SRC_PSRDY_SENT                  = 0x04,
	PDMSG_SRC_ERROR_RECOVERY              = 0x05,
	PDMSG_SRC_SENDERRESPONSETIMER_TIMEOUT = 0x06,
	PDMSG_DR_SWAP_REQ_RECEIVED            = 0x07,
	PDMSG_PR_SWAP_REQ_RECEIVED            = 0x08,
	PDMSG_VCONN_SWAP_REQ_RECEIVED         = 0x09,
	PDMSG_VDM_ATTENTION_MSG_RECEIVED      = 0x11,
	PDMSG_REJECT_RECEIVED                 = 0x12,
	PDMSG_PRSWAP_SNKTOSRC_SENT            = 0x14,
	PDMSG_PRSWAP_SRCTOSNK_SENT            = 0x15,
	PDMSG_HARDRESET_RECEIVED              = 0x16,
	PDMSG_HARDRESET_SENT                  = 0x19,
	PDMSG_PRSWAP_SRCTOSWAP                = 0x1A,
	PDMSG_PRSWAP_SWAPTOSNK                = 0x1B,
	PDMSG_PRSWAP_SNKTOSWAP                = 0x1C,
	PDMSG_PRSWAP_SWAPTOSRC                = 0x1D,
	PDMSG_SNK_DISABLED                    = 0x20,
	PDMSG_SRC_DISABLED                    = 0x21,
	PDMSG_VDM_NAK_RECEIVED                = 0x40,
	PDMSG_VDM_BUSY_RECEIVED               = 0x41,
	PDMSG_VDM_ACK_RECEIVED                = 0x42,
	PDMSG_VDM_REQ_RECEIVED                = 0x43,
	PDMSG_VDM_DISCOVERMODEs               = 0x63,
	PDMSG_VDM_DP_STATUS                   = 0x65,
};
enum max77958_wtrstat {
	DRY = 0x00,
	WATER = 0x01,
};

/*
 * External type definition
 */
#define OPCODE_WAIT_TIMEOUT (3000) /* 3000ms */

#define OPCODE_WRITE_COMMAND 0x21
#define OPCODE_READ_COMMAND 0x51
#define OPCODE_SIZE 1
#define OPCODE_DATA_LENGTH 32
#define OPCODE_MAX_LENGTH (OPCODE_DATA_LENGTH + OPCODE_SIZE)
#define OPCODE_WRITE 0x21
#define OPCODE_WRITE_END 0x41
#define OPCODE_READ 0x51

enum {
	OPCODE_BC_CTRL1_R			 = 0x01,
	OPCODE_BC_CTRL1_W			 = 0x02,
	OPCODE_BC_CTRL2_R			 = 0x03,
	OPCODE_BC_CTRL2_W			 = 0x04,
	OPCODE_CONTROL1_R			 = 0x05,
	OPCODE_CONTROL1_W			 = 0x06,
	OPCODE_CCCONTROL1_R			 = 0x0B,
	OPCODE_CCCONTROL1_W			 = 0x0C,
	OPCODE_CCCONTROL2_R			 = 0x0D,
	OPCODE_CCCONTROL2_W			 = 0x0E,
	OPCODE_CCCONTROL3_R			 = 0x0F,
	OPCODE_CCCONTROL3_W			 = 0x10,
	OPCODE_CCCONTROL4_R			 = 0x11,
	OPCODE_CCCONTROL4_W			 = 0x12,
	OPCODE_VCONN_ILIM_R			 = 0x13,
	OPCODE_VCONN_ILIM_W			 = 0x14,
	OPCODE_CCCONTROL5_R			 = 0x15,
	OPCODE_CCCONTROL5_W			 = 0x16,
	OPCODE_CCCONTROL6_R			 = 0x17,
	OPCODE_CCCONTROL6_W			 = 0x18,
#if defined(MCFG_FEAT_EN_AFC)
	OPCODE_AUTO_AFC_WRITE		 = 0x20,
	OPCODE_AFC_HV_W				 = 0x21,
	OPCODE_QC20_SET				 = 0x22,
#endif /* MCFG_FEAT_EN_AFC */
	OPCODE_GET_SINK_CAP			 = 0x2F,
	OPCODE_CURRENT_SRC_CAP		 = 0x30,
	OPCODE_GET_SOURCE_CAP		 = 0x31,
	OPCODE_SRCCAP_REQ			 = 0x32,
	OPCODE_SET_SOURCE_CAP		 = 0x33,
	OPCODE_SEND_GET_REQ			 = 0x34,
	OPCODE_READ_GET_REQ_RESP	 = 0x35,
	OPCODE_SEND_GET_RESP		 = 0x36,
	OPCODE_SWAP_REQ				 = 0x37,
	OPCODE_SWAP_RESP			 = 0x38,
	OPCODE_VDM_DISCOVER_ID_RESP	 = 0x40,
	OPCODE_VDM_DISCOVER_ID_REQ	 = 0x41,
	OPCODE_VDM_DISCOVER_SVID_RESP = 0x42,
	OPCODE_VDM_DISCOVER_SVID_REQ  = 0x43,
	OPCODE_VDM_DISCOVER_MODE_RESP = 0x44,
	OPCODE_VDM_DISCOVER_MODE_REQ  = 0x45,
	OPCODE_VDM_ENTER_MODE_REQ	 = 0x46,
	OPCODE_VDM_EXIT_MODE_REQ	= 0x47,
	OPCODE_SEND_VDM				 = 0x48,
	OPCODE_GET_PD_MSG			 = 0x4A,
	OPCODE_GET_VDM_RESP			 = 0x4B,
	OPCODE_SET_CSTM_INFORMATION_R  = 0x55, /* Customer VIF Information */
	OPCODE_SET_CSTM_INFORMATION_W  = 0x56, /* Customer VIF Information */
	/* Device Config Information */
	OPCODE_SET_DEVCONFG_INFORMATION_R  = 0x57,
	/* Device Config Information */
	OPCODE_SET_DEVCONFG_INFORMATION_W  = 0x58,
	OPCODE_ACTION_BLOCK_MTP_UPDATE  = 0x60, /* Action Block update */
	OPCODE_MASTER_I2C_READ  = 0x85, /* I2C_READ */
	OPCODE_MASTER_I2C_WRITE  = 0x86, /* I2C_WRITE */
	OPCODE_RSVD = 0xFF,
};

enum VDM_MSG_IRQ_State {
	VDM_DISCOVER_ID		=	(1 << 0),
	VDM_DISCOVER_SVIDS	=	(1 << 1),
	VDM_DISCOVER_MODES	=	(1 << 2),
	VDM_ENTER_MODE		=	(1 << 3),
	VDM_DP_STATUS_UPDATE = (1 << 4),
	VDM_DP_CONFIGURE	=	(1 << 5),
	VDM_ATTENTION		=	(1 << 6),
	VDM_EXIT_MODE		=	(1 << 7),
};

/*
 * REG_INT_M Initial values
 */
#define REG_UIC_INT_M_INIT		0x05
#define REG_CC_INT_M_INIT		0x20
#define REG_PD_INT_M_INIT		0x00
#define REG_ACTION_INT_M_INIT   0xFF


#define OPCODE_CMD_REQUEST	0x1

#define MFD_DEV_NAME "max77958"

/* MAX77705 shared i2c API function */
extern int max77958_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest);
extern int max77958_bulk_read(struct i2c_client *i2c, u8 reg, int count,
		u8 *buf);
extern int max77958_write_reg(struct i2c_client *i2c, u8 reg, u8 value);
extern int max77958_bulk_write(struct i2c_client *i2c, u8 reg, int count,
		u8 *buf);
extern int max77958_write_word(struct i2c_client *i2c, u8 reg, u16 value);
extern int max77958_read_word(struct i2c_client *i2c, u8 reg);

extern int max77958_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask);

#endif

