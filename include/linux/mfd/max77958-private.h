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



#ifndef __LINUX_MFD_MAX77958_PRIV_H
#define __LINUX_MFD_MAX77958_PRIV_H

#include <linux/i2c.h>
#include <linux/mfd/max77958.h>
#define MAX77958_I2C_ADDR			(0x92)
#define MAX77958_REG_INVALID		(0xff)
#define MAX77958_IRQSRC_USBC		(1 << 0)
#define TYPE_MAX77958						0x0

enum max77958_hw_rev {
	MAX77958_PASS1 = 0x1,
	MAX77958_PASS2 = 0x2,
	MAX77958_PASS3 = 0x3,
};

enum max77958_irq_source {
		USBC_INT = 0,
		CC_INT,
		PD_INT,
		ACTION_INT,
		MAX77958_IRQ_GROUP_NR,
};

enum max77958_irq {
	/* USBC */
	MAX77958_USBC_IRQ_APC_INT,

	/* CC */
	MAX77958_CC_IRQ_VCONNCOP_INT,
	MAX77958_CC_IRQ_VSAFE0V_INT,
	MAX77958_CC_IRQ_DETABRT_INT,
	MAX77958_CC_IRQ_WTR_INT,
	MAX77958_CC_IRQ_CCPINSTAT_INT,
	MAX77958_CC_IRQ_CCISTAT_INT,
	MAX77958_CC_IRQ_CCVCNSTAT_INT,
	MAX77958_CC_IRQ_CCSTAT_INT,

	MAX77958_USBC_IRQ_VBUS_INT,
	MAX77958_USBC_IRQ_VBADC_INT,
	MAX77958_USBC_IRQ_DCD_INT,
	MAX77958_USBC_IRQ_CHGT_INT,
	MAX77958_USBC_IRQ_UIDADC_INT,
	MAX77958_USBC_IRQ_SYSM_INT,

	/* PD */
	MAX77958_PD_IRQ_PDMSG_INT,
	MAX77958_PD_IRQ_PS_RDY_INT,
	MAX77958_PD_IRQ_DATAROLE_INT,
/* for special customer */
/* MAX77958_PD_IRQ_FCTIDI_INT */
	MAX77958_PD_IRQ_DISPLAYPORT_INT,
	MAX77958_PD_IRQ_SSACC_INT,
	MAX77958_PD_IRQ_FCTIDI_INT,

	/* VDM */
	/*
	*	MAX77958_IRQ_VDM_DISCOVER_ID_INT,
	*	MAX77958_IRQ_VDM_DISCOVER_SVIDS_INT,
	*	MAX77958_IRQ_VDM_DISCOVER_MODES_INT,
	*	MAX77958_IRQ_VDM_ENTER_MODE_INT,
	*	MAX77958_IRQ_VDM_DP_STATUS_UPDATE_INT,
	*	MAX77958_IRQ_VDM_DP_CONFIGURE_INT,
	*	MAX77958_IRQ_VDM_ATTENTION_INT,
	*/

	/*ACTION*/
	MAX77958_IRQ_ACTION0_INT,
	MAX77958_IRQ_ACTION1_INT,
	MAX77958_IRQ_ACTION2_INT,
	MAX77958_IRQ_EXTENDED_ACTION_INT,

	MAX77958_IRQ_NR,
};


struct max77958_platform_data {
	/* IRQ */
	int irq_base;
	bool wakeup;
};

struct max77958_dev {
	struct device *dev;
	struct i2c_client *i2c;
	struct mutex i2c_lock;

	int type;

	int irq;
	int irq_base;
	bool wakeup;
	struct mutex irqlock;
	int irq_masks_cur[MAX77958_IRQ_GROUP_NR];
	int irq_masks_cache[MAX77958_IRQ_GROUP_NR];

	u8 HW_Revision;
	u8 Device_Revision;
	u8 FW_Revision;
	u8 FW_Minor_Revision;
	struct work_struct fw_work;
	struct workqueue_struct *fw_workqueue;
	struct completion fw_completion;
	int fw_update_state;

	u8 boot_complete;

	struct max77958_platform_data *pdata;
};

extern int max77958_irq_init(struct max77958_dev *max77958);
extern void max77958_irq_exit(struct max77958_dev *max77958);
extern int max77958_usbc_fw_update(struct max77958_dev *max77958,
		const u8 *fw_bin, int fw_bin_len, int enforce_do);


#endif /* __LINUX_MFD_MAX77958_PRIV_H */

