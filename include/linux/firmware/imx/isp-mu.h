/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2023 NXP
 *
 * Header file for the ISP MU implementation.
 */

#ifndef _SC_ISPMU_H
#define _SC_ISPMU_H

#include <linux/device.h>
#include <linux/types.h>
#include <linux/mailbox_client.h>

/*
 * The MU uses 4 Tx and 4 Rx registers, but we will use all Tx regs and all Rx
 * regs as a single mailbox channel. Therefore, tx0 will represent Tx[0-3] of
 * the MU0, tx1 will represent Tx[0-3] of the MU1 and so on.
 * Channel 0 will be used for Tx and channel 1 for Rx
 */
#define ISP_MU_DEV_NUM		9
#define ISP_MU_CHAN_NUM		2
#define MU_CHAN_TX		0
#define MU_CHAN_RX		1
#define MAX_TX_TIMEOUT		(msecs_to_jiffies(1000))
#define MAX_RX_TIMEOUT		(msecs_to_jiffies(3000))

enum imx_mu_ids {
	ISP_MU_MASTER = 0,
	ISP_MU_CLIENT_0,
	ISP_MU_CLIENT_1,
	ISP_MU_CLIENT_2,
	ISP_MU_CLIENT_3,
	ISP_MU_CLIENT_4,
	ISP_MU_CLIENT_5,
	ISP_MU_CLIENT_6,
	ISP_MU_CLIENT_7,
};

/* ISP-Master commands */

/* Request: ISP-FW to abandon all work and do a SW restart */
#define ISPFW_MSG_ID_M_REBOOT		0x01

/* Request: the IDLE percentage of the M0+ core utilization */
#define ISPFW_MSG_ID_M_GET_IDLE		0x02

/* Request: the ISP current state information:
 * - Contex#0 loaded camera-config ID
 * - Contex#1 loaded camera-config ID
 * - Active context#
 * - Frame count of active context
 */
#define ISPFW_MSG_ID_M_GET_STATE	0x03

/* Request: the ISP-FW version */
#define ISPFW_MSG_ID_M_GET_VER		0x04

/* Request: ISP-FW to do an hard-/soft- reset of the ISP core */
#define ISPFW_MSG_ID_M_RESET		0x05

/* Request: ISP-FW to stop ISP pipeline */
#define ISPFW_MSG_ID_M_STOP		0x01 /* TODO: Same ID as REBOOT!?!? */

/* Request: ISP-FW to return the value read from the given address */
#define ISPFW_MSG_ID_M_GET_ADDR		0x07

/* Request: ISP-FW to write the value on the given address */
#define ISPFW_MSG_ID_M_SET_ADDR		0x08

/* Response: from ISP-FW with an error */
#define ISPFW_MSG_ID_M_ERR		0x09

/* Response: from ISP-FW that is ready to receive commands */
#define ISPFW_MSG_ID_M_READY		0x0A

/* ISP-Client commands */

/* Request: the ISP-FW to run the specified camera configuration. */
#define ISPFW_MSG_ID_C_START		0x0B

/* Response: from ISP-FW that frame associated with this client is ready */
#define ISPFW_MSG_ID_C_FRAME_DONE	0x0C

/* Request: the ISP-FW to list the tasks process-time table */
#define ISPFW_MSG_ID_C_GET_PTIME	0x0D

/* Request: the ISP-FW to list the tasks stack-usage table */
#define ISPFW_MSG_ID_C_GET_TUSAGE	0x0E

struct imx_ispfw_cmd {
	u32 cmd_data2;
	u32 cmd_data1;
	u32 cmd_data0;
	u32 cmd_id;
} __packed __aligned(4);


struct isp_mu_client {
	u8 id;
	struct device *dev;
	struct isp_mu_dev *mu;

	void (*rx_callback)(struct isp_mu_client *cl, void *msg);
};

struct isp_mu_chan {
	struct isp_mu_dev *mu_dev;

	struct mbox_client cl;
	struct mbox_chan *ch;
	int idx;
};

struct isp_mu_dev {
	u8 id;
	struct isp_mu_ipc *ipc;
	struct isp_mu_client *cl;

	struct isp_mu_chan chans[ISP_MU_CHAN_NUM];
	struct mutex lock;
	struct completion done;

	bool need_reply;
	u32 reply[4];
};

struct isp_mu_ipc {
	struct isp_mu_dev *devs[ISP_MU_DEV_NUM];
	int dev_num;
	struct device *dev;
};


int imx_ispmu_client_register(struct isp_mu_client *cl);
int imx_ispmu_client_unregister(struct isp_mu_client *cl);
int imx_ispmu_send_msg(struct isp_mu_client *cl, void *msg, void **reply);

#endif /* _SC_ISPMU_H */
