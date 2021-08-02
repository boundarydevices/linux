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



#ifndef __LINUX_MFD_MAX77958_CC_H
#define __LINUX_MFD_MAX77958_CC_H
#include <linux/mfd/max77958.h>
#include <linux/wakelock.h>

#define MAX77958_CC_NAME	"MAX77958_CC"


struct max77958_cc_data {

	/* interrupt pin */
	int irq_vconncop;
	int irq_vsafe0v;
	int irq_detabrt;
	int irq_wtr;
	int irq_ccpinstat;
	int irq_ccistat;
	int irq_ccvcnstat;
	int irq_ccstat;

	u8 usbc_status1;
	u8 usbc_status2;
	u8 bc_status;
	u8 cc_status0;
	u8 cc_status1;
	u8 pd_status0;
	u8 pd_status1;

	u8 opcode_res;

	/* VCONN Over Current Detection */
	u8 vconnocp;
	/* VCONN Over Short Circuit Detection */
	u8 vconnsc;
	/* Status of VBUS Detection */
	u8 vsafe0v;
	/* Charger Detection Abort Status */
	u8 detabrt;
	/* Output of active CC pin */
	u8 ccpinstat;
	/* CC Pin Detected Allowed VBUS Current in UFP mode */
	u8 ccistat;
	/* Status of Vconn Output */
	u8 ccvcnstat;
	/* CC Pin State Machine Detection */

	enum max77958_vcon_role	current_vcon;
	enum max77958_vcon_role	previous_vcon;
	enum max77958_cc_pin_state current_pr;
	enum max77958_cc_pin_state previous_pr;
	/* CC pin state Machine Detection */
	enum max77958_cc_pin_state  ccstat;


	/* wakelock */
	struct wake_lock max77958_cc_wake_lock;
};
#endif
