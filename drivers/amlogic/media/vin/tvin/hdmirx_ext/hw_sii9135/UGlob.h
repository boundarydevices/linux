/*------------------------------------------------------------------------------
 * Copyright © 2002-2005, Silicon Image, Inc.  All rights reserved.
 *
 * No part of this work may be reproduced, modified, distributed, transmitted,
 * transcribed, or translated into any language or computer format, in any form
 * or by any means without written permission of: Silicon Image, Inc.,
 * 1060 East Arques Avenue, Sunnyvale, California 94085
 *----------------------------------------------------------------------------
 */

#ifndef _UGLOB_
#define _UGLOB_
#include "../hdmirx_ext_drv.h"

#define TASK_HDMI_RX_API    0
#define TASK_KEYBOARD       1
#define TASK_COMM           2
#define LAST_TASK           TASK_COMM

#define SII_PCB_ID_ADDR 0x08

enum SiIPCBType_e {

	SiI_CP9011,
	SiI_CP9031,
	SiI_CP9041,
	SiI_CP9000,
	SiI_CP9133,
	SiI_FPGA_IP11,
	SiI_CP9135,
	SiI_CP9125

};

/* communication */

enum SiI_com {

	WaitStartRX = 0,
	ReadyRX = 1,
	START_FRAME_RX = 's'
};


#define RX_BUF_SIZE 26 /* used for communication */

extern IRAM BYTE RXBuffer[RX_BUF_SIZE];      /* Command buffer */
extern BYTE bRxIndex;
extern BYTE bCommState;
extern BYTE bCommTO;
extern BOOL qBuffInUse;
extern BOOL qFuncExe;
extern BOOL qDebugModeOnlyF;


extern BOOL qReqTasksProcessing;
extern DWORD wNewTaskTickCounter;
extern DWORD wTickCounter;
extern BYTE bNewTaskSlot;

#define TASK_SLOT_IN_TICKS 30



#endif

