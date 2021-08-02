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


#define OPCODE_READ_MAX_LENGTH		(32 + 1)
#define OPCODE_WRITE_MAX_LENGTH     (32 + 1) /* opcode + data 31bytes */
#define OPCODE_WRITE_START_ADDR		0x21
#define OPCODE_WRITE_END_ADDR		0x41
#define OPCODE_READ_START_ADDR		0x51
#define OPCODE_READ_END_ADDR		0x71

enum mxim_ioctl_cmd {
	MXIM_DEBUG_OPCODE_WRITE = 1000,
	MXIM_DEBUG_OPCODE_READ,
	MXIM_DEBUG_REG_WRITE = 1002,
	MXIM_DEBUG_REG_READ,
	MXIM_DEBUG_REG_DUMP,
	MXIM_DEBUG_FW_VERSION = 1005,
};

enum mxim_registers {
	MXIM_REG_UIC_HW_REV = 0x00,
	MXIM_REG_UIC_DEVICE_ID,
	MXIM_REG_UIC_FW_REV,
	MXIM_REG_UIC_FW_SUBREV = 0x03,
	MXIM_REG_UIC_INT,
	MXIM_REG_CC_INT,
	MXIM_REG_PD_INT,
	MXIM_REG_ACTION_INT,
	MXIM_REG_USBC_STATUS1,
	MXIM_REG_USBC_STATUS2,
	MXIM_REG_BC_STATUS = 0x0A,
	MXIM_REG_DP_STATUS,
	MXIM_REG_CC_STATUS0 = 0x0C,
	MXIM_REG_CC_STATUS1,
	MXIM_REG_PD_STATUS0,
	MXIM_REG_PD_STATUS1,
	MXIM_REG_UIC_INT_M,
	MXIM_REG_CC_INT_M,
	MXIM_REG_PD_INT_M,
	MXIM_REG_ACTION_INT_M,
	MXIM_REG_MAX,
};


struct mxim_debug_registers {
	unsigned char reg;
	unsigned char val;
	int ignore;
};

struct mxim_debug_pdev {
	struct mxim_debug_registers registers;
	struct i2c_client *client;
	struct class *class;
	struct device *dev;
	struct mutex lock;
	unsigned char opcode_wdata[OPCODE_WRITE_MAX_LENGTH];
	unsigned char opcode_rdata[OPCODE_READ_MAX_LENGTH];
};

void mxim_debug_set_i2c_client(struct i2c_client *client);
int mxim_debug_init(void);
void mxim_debug_exit(void);
