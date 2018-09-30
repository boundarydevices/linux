/*
 * drivers/amlogic/media/vin/tvin/hdmirx/hdmirx_repeater.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>

/* Local include */
#include "hdmi_rx_repeater.h"
#include "hdmi_rx_drv.h"
#include "hdmi_rx_hw.h"
#include "hdmi_rx_wrapper.h"
#include "hdmi_rx_edid.h"
/*edid original data from device*/
static unsigned char receive_edid[MAX_RECEIVE_EDID];
int receive_edid_len = MAX_RECEIVE_EDID;
MODULE_PARM_DESC(receive_edid, "\n receive_edid\n");
module_param_array(receive_edid, byte, &receive_edid_len, 0664);
int edid_len;
MODULE_PARM_DESC(edid_len, "\n edid_len\n");
module_param(edid_len, int, 0664);
bool new_edid;
/*original bksv from device*/
static unsigned char receive_hdcp[MAX_KSV_LIST_SIZE];
int hdcp_array_len = MAX_KSV_LIST_SIZE;
MODULE_PARM_DESC(receive_hdcp, "\n receive_hdcp\n");
module_param_array(receive_hdcp, byte, &hdcp_array_len, 0664);
int hdcp_len;
int hdcp_repeat_depth;
bool new_hdcp;
bool start_auth_14;
MODULE_PARM_DESC(start_auth_14, "\n start_auth_14\n");
module_param(start_auth_14, bool, 0664);

bool repeat_plug;
MODULE_PARM_DESC(repeat_plug, "\n repeat_plug\n");
module_param(repeat_plug, bool, 0664);

int up_phy_addr;/*d c b a 4bit*/
MODULE_PARM_DESC(up_phy_addr, "\n up_phy_addr\n");
module_param(up_phy_addr, int, 0664);
int hdcp22_firm_switch_timeout;

unsigned char *rx_get_dw_hdcp_addr(void)
{
	return receive_hdcp;
}

void rx_start_repeater_auth(void)
{
	rx.hdcp.state = REPEATER_STATE_START;
	start_auth_14 = 1;
	rx.hdcp.delay = 0;
	hdcp_len = 0;
	hdcp_repeat_depth = 0;
	rx.hdcp.dev_exceed = 0;
	rx.hdcp.cascade_exceed = 0;
	rx.hdcp.depth = 0;
	rx.hdcp.count = 0;
	memset(&receive_hdcp, 0, sizeof(receive_hdcp));
}

void rx_check_repeat(void)
{
	struct hdcp14_topo_s *topo_data = (struct hdcp14_topo_s *)receive_hdcp;

	if (!hdmirx_repeat_support())
		return;

	if (rx.hdcp.repeat != repeat_plug) {
		/*pull down hpd if downstream plug low*/
		rx_set_cur_hpd(0);
		rx_pr("firm_change:%d,repeat_plug:%d,repeat:%d\n",
			rx.firm_change, repeat_plug, rx.hdcp.repeat);
		rx_set_repeat_signal(repeat_plug);
		if (!repeat_plug) {
			edid_len = 0;
			rx.hdcp.dev_exceed = 0;
			rx.hdcp.cascade_exceed = 0;
			memset(&receive_hdcp, 0, sizeof(receive_hdcp));
			memset(&receive_edid, 0, sizeof(receive_edid));
			up_phy_addr = 0;
			/*new_edid = true;*/
			/* rx_set_cur_hpd(1); */
			/*rx.firm_change = 0;*/
			rx_pr("1firm_change:%d,repeat_plug:%d,repeat:%d\n",
				rx.firm_change, repeat_plug, rx.hdcp.repeat);
		}
	}

	/*this is addition for the downstream edid too late*/
	if (new_edid && rx.hdcp.repeat && (!rx.firm_change)) {
		/*check downstream plug when new plug occur*/
		/*check receive change*/
		/*it's contained in hwconfig*/
		hdmi_rx_top_edid_update();
		hdcp22_firm_switch_timeout = 0;
		new_edid = false;
		rx_send_hpd_pulse();
	}

	if (repeat_plug) {
		switch (rx.hdcp.state) {
		case REPEATER_STATE_START:
			rx_pr("[RX] receive aksv\n");
			hdmirx_wr_bits_dwc(DWC_HDCP_RPT_CTRL,
						KSVLIST_TIMEOUT, 0);
			hdmirx_wr_bits_dwc(DWC_HDCP_RPT_CTRL,
						KSVLIST_LOSTAUTH, 0);
			hdmirx_wr_bits_dwc(DWC_HDCP_RPT_CTRL,
						KSVLIST_READY, 0);
			hdmirx_wr_bits_dwc(DWC_HDCP_RPT_BSTATUS,
				MAX_CASCADE_EXCEEDED, 0);
			hdmirx_wr_bits_dwc(DWC_HDCP_RPT_BSTATUS,
				MAX_DEVS_EXCEEDED, 0);
			rx.hdcp.state = REPEATER_STATE_WAIT_KSV;
		break;

		case REPEATER_STATE_WAIT_KSV:
		if (!rx.cur_5v_sts) {
			rx.hdcp.state = REPEATER_STATE_IDLE;
			break;
		}
		if (hdmirx_rd_bits_dwc(DWC_HDCP_RPT_CTRL, WAITING_KSV)) {
			rx.hdcp.delay++;
			if (rx.hdcp.delay == 1)
				rx_pr("[RX] receive ksv wait signal\n");
			if (rx.hdcp.delay < KSV_LIST_WR_MAX) {
				break;
			} else if (rx.hdcp.delay >= KSV_LIST_WAIT_DELAY) {
				hdmirx_wr_bits_dwc(DWC_HDCP_RPT_CTRL,
						KSVLIST_TIMEOUT, 1);
				rx.hdcp.state = REPEATER_STATE_IDLE;
				rx_pr("[RX] receive ksv wait timeout\n");
			}
			if (rx_set_repeat_aksv(topo_data->ksv_list,
				topo_data->device_count,
				topo_data->depth, topo_data->max_devs_exceeded,
				topo_data->max_cascade_exceeded)) {
				rx.hdcp.state = REPEATER_STATE_IDLE;
			}
		}
		/*if support hdcp2.2 jump to wait_ack else to idle*/
		break;

		case REPEATER_STATE_WAIT_ACK:
		/*if receive ack jump to idle else send auth_req*/
		break;

		case REPEATER_STATE_IDLE:

		break;

		default:
		break;
		}
	}
	/*hdr change from app*/
	/*if (new_hdr_lum) {*/
		/*hdmi_rx_ctrl_edid_update();*/
		/*new_hdr_lum = false;*/
		/*rx_send_hpd_pulse();*/
	/*}*/
}

void rx_reload_firm_reset(int reset)
{
	if (reset)
		hdmirx_load_firm_reset(reset);
	else
		rx_firm_reset_end();
}

void rx_firm_reset_end(void)
{
	rx_pr("%s new_edid:%d\n", __func__, new_edid);
	if (new_edid) {
		new_edid = 0;
		hdmi_rx_top_edid_update();
	}
	rx.firm_change = 0;
}
unsigned char *rx_get_dw_edid_addr(void)
{
	return receive_edid;
}

int rx_set_receiver_edid(const char *data, int len)
{
	if ((data == NULL) || (len == 0))
		return false;

	if ((len > MAX_RECEIVE_EDID) || (len < 3)) {
		memset(receive_edid, 0, sizeof(receive_edid));
		edid_len = 0;
	} else {
		memcpy(receive_edid, data, len);
		edid_len = len;
	}
	new_edid = true;
	return true;
}

void rx_hdcp14_resume(void)
{
	hdcp22_kill_esm = 0;
	extcon_set_state_sync(rx.rx_excton_rx22, EXTCON_DISP_HDMI, 0);
	hdmirx_wr_dwc(DWC_HDCP22_CONTROL,
				0x1000);
	extcon_set_state_sync(rx.rx_excton_rx22, EXTCON_DISP_HDMI, 1);
	hpd_to_esm = 1;
	rx_pr("hdcp14 on\n");
}

void rx_set_repeater_support(bool enable)
{
	downstream_repeat_support = enable;
}
EXPORT_SYMBOL(rx_set_repeater_support);

bool rx_poll_dwc(uint16_t addr, uint32_t exp_data,
			uint32_t mask, uint32_t max_try)
{
	uint32_t rd_data;
	uint32_t cnt = 0;
	uint8_t done = 0;

	rd_data = hdmirx_rd_dwc(addr);
	while (((cnt < max_try) || (max_try == 0)) && (done != 1)) {
		if ((rd_data & mask) == (exp_data & mask)) {
			done = 1;
		} else {
			cnt++;
			rd_data = hdmirx_rd_dwc(addr);
		}
	}
	if (log_level & VIDEO_LOG)
		rx_pr("poll dwc cnt :%d\n", cnt);
	if (done == 0) {
		/* if(log_level & ERR_LOG) */
		rx_pr("poll dwc%x time-out!\n", addr);
		return false;
	}
	return true;
}

bool rx_set_repeat_aksv(unsigned char *data, int len, int depth,
	bool dev_exceed, bool cascade_exceed)
{
	int i;
	bool ksvlist_ready = 0;

	if ((data == 0) || (((depth == 0) || (len == 0))
		&& (!dev_exceed) && (!cascade_exceed)))
		return false;
	rx_pr("set ksv list len:%d,depth:%d, exceed count:%d,cascade:%d\n",
					len, depth, dev_exceed, cascade_exceed);
	/*set repeat depth*/
	if ((depth <= MAX_REPEAT_DEPTH) && (!cascade_exceed)) {
		hdmirx_wr_bits_dwc(DWC_HDCP_RPT_BSTATUS, MAX_CASCADE_EXCEEDED,
		0);
		hdmirx_wr_bits_dwc(DWC_HDCP_RPT_BSTATUS, DEPTH, depth);
		rx.hdcp.depth = depth;
	} else {
		hdmirx_wr_bits_dwc(DWC_HDCP_RPT_BSTATUS, MAX_CASCADE_EXCEEDED,
		1);
		rx.hdcp.depth = 0;
	}
	/*set repeat count*/
	if ((len <= HDCP14_KSV_MAX_COUNT) && (!dev_exceed)) {
		hdmirx_wr_bits_dwc(DWC_HDCP_RPT_BSTATUS, MAX_DEVS_EXCEEDED,
		0);
		rx.hdcp.count = len;
		hdmirx_wr_bits_dwc(DWC_HDCP_RPT_BSTATUS, DEVICE_COUNT,
								rx.hdcp.count);
	} else {
		hdmirx_wr_bits_dwc(DWC_HDCP_RPT_BSTATUS, MAX_DEVS_EXCEEDED,
		1);
		rx.hdcp.count = 0;
	}
	/*set repeat status*/
	/* if (rx.hdcp.count > 0) {
	 *	rx.hdcp.repeat = true;
	 *	hdmirx_wr_bits_dwc(DWC_HDCP_RPT_CTRL, REPEATER, 1);
	 *} else {
	 *	rx.hdcp.repeat = false;
	 *	hdmirx_wr_bits_dwc(DWC_HDCP_RPT_CTRL, REPEATER, 0);
	 *}
	 */
	ksvlist_ready = ((rx.hdcp.count > 0) && (rx.hdcp.depth > 0));
	rx_pr("[RX]write ksv list count:%d\n", rx.hdcp.count);
	/*write ksv list to fifo*/
	for (i = 0; i < rx.hdcp.count; i++) {
		if (rx_poll_dwc(DWC_HDCP_RPT_CTRL, ~KSV_HOLD, KSV_HOLD,
		KSV_LIST_WR_TH)) {
			/*check fifo can be written*/
			hdmirx_wr_dwc(DWC_HDCP_RPT_KSVFIFOCTRL, i);
			hdmirx_wr_dwc(DWC_HDCP_RPT_KSVFIFO1,
				*(data + i*MAX_KSV_SIZE + 4));
			hdmirx_wr_dwc(DWC_HDCP_RPT_KSVFIFO0,
				*((uint32_t *)(data + i*MAX_KSV_SIZE)));
			if (log_level & VIDEO_LOG)
				rx_pr(
				"[RX]write ksv list index:%d, ksv hi:%#x, low:%#x\n",
				i, *(data + i*MAX_KSV_SIZE +
				4), *((uint32_t *)(data + i*MAX_KSV_SIZE)));
		} else {
			return false;
		}
	}
	/* Wait for ksv_hold=0*/
	rx_poll_dwc(DWC_HDCP_RPT_CTRL, ~KSV_HOLD, KSV_HOLD, KSV_LIST_WR_TH);
	/*set ksv list ready*/
	hdmirx_wr_bits_dwc(DWC_HDCP_RPT_CTRL, KSVLIST_READY, ksvlist_ready);
	/* Wait for HW completion of V value*/
	if (ksvlist_ready)
		rx_poll_dwc(DWC_HDCP_RPT_CTRL, FIFO_READY,
						FIFO_READY, KSV_V_WR_TH);
	rx_pr("[RX]write Ready signal!\n", ksvlist_ready);

	return true;
}

void rx_set_repeat_signal(bool repeat)
{
	rx.hdcp.repeat = repeat;
	hdmirx_wr_bits_dwc(DWC_HDCP_RPT_CTRL, REPEATER, repeat);
}

bool rx_set_receive_hdcp(unsigned char *data, int len, int depth,
	bool cas_exceed, bool devs_exceed)
{
	return true;
}
EXPORT_SYMBOL(rx_set_receive_hdcp);

void rx_repeat_hpd_state(bool plug)
{
}
EXPORT_SYMBOL(rx_repeat_hpd_state);

void rx_edid_physical_addr(int a, int b, int c, int d)
{
}
EXPORT_SYMBOL(rx_edid_physical_addr);


