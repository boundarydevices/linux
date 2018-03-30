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
#include "hdmi_rx_drv.h"
#include "hdmi_rx_hw.h"
#include "hdmi_rx_repeater.h"
#include "hdmi_rx_wrapper.h"
#include "hdmi_rx_edid.h"
/*edid original data from device*/
static unsigned char receive_edid[MAX_RECEIVE_EDID];
int receive_edid_len = MAX_RECEIVE_EDID;
MODULE_PARM_DESC(receive_edid, "\n receive_edid\n");
module_param_array(receive_edid, byte, &receive_edid_len, 0664);
bool new_edid;
/*original bksv from device*/
static unsigned char receive_hdcp[MAX_KSV_LIST_SIZE];
int hdcp_array_len = MAX_KSV_LIST_SIZE;
MODULE_PARM_DESC(receive_hdcp, "\n receive_hdcp\n");
module_param_array(receive_hdcp, byte, &hdcp_array_len, 0664);
int hdcp_len;
int hdcp_repeat_depth;
bool new_hdcp;
bool repeat_plug;
int up_phy_addr;/*d c b a 4bit*/

bool downstream_rp_en;

enum repeater_state_e rpt_state;

bool hdmirx_repeat_support(void)
{
	return downstream_rp_en;
}

void rx_start_repeater_auth(void)
{
	rpt_state = REPEATER_STATE_START;
	rx.hdcp.delay = 0;
	hdcp_len = 0;
	hdcp_repeat_depth = 0;
	rx.hdcp.dev_exceed = 0;
	rx.hdcp.cascade_exceed = 0;
	memset(&receive_hdcp, 0, sizeof(receive_hdcp));
}

void repeater_dwork_handle(struct work_struct *work)
{
	if (hdmirx_repeat_support()) {
		if (rx.hdcp.hdcp_version && hdmirx_is_key_write()
			&& rx.open_fg) {
			extcon_set_state_sync(rx.hdcp.rx_excton_auth,
				EXTCON_DISP_HDMI, 0);
			extcon_set_state_sync(rx.hdcp.rx_excton_auth,
				EXTCON_DISP_HDMI, rx.hdcp.hdcp_version);
		}
	}
}

bool hdmirx_is_key_write(void)
{
	if (hdmirx_rd_dwc(DWC_HDCP_BKSV0) != 0)
		return 1;
	else
		return 0;
}

void rx_check_repeat(void)
{
	if (!hdmirx_repeat_support())
		return;

	if (rx.hdcp.repeat != repeat_plug) {
		rx_set_repeat_signal(repeat_plug);
		if (!repeat_plug) {
			hdcp_len = 0;
			hdcp_repeat_depth = 0;
			rx.hdcp.dev_exceed = 0;
			rx.hdcp.cascade_exceed = 0;
			memset(&receive_hdcp, 0, sizeof(receive_hdcp));
			new_edid = true;
			memset(&receive_edid, 0, sizeof(receive_edid));
			rx_send_hpd_pulse();
		}
	}
	if (new_edid) {
		/*check downstream plug when new plug occur*/
		/*check receive change*/
		hdmi_rx_top_edid_update();
		new_edid = false;
		rx_send_hpd_pulse();
	}
	if (repeat_plug) {
		switch (rpt_state) {
		case REPEATER_STATE_START:
			rx_pr("[RX] receive aksv\n");
			hdmirx_wr_bits_dwc(DWC_HDCP_RPT_CTRL,
						KSVLIST_TIMEOUT, 0);
			hdmirx_wr_bits_dwc(DWC_HDCP_RPT_CTRL,
						KSVLIST_LOSTAUTH, 0);
			hdmirx_wr_bits_dwc(DWC_HDCP_RPT_CTRL,
						KSVLIST_READY, 0);
			rpt_state = REPEATER_STATE_WAIT_KSV;
		break;

		case REPEATER_STATE_WAIT_KSV:
		if (!rx.cur_5v_sts) {
			rpt_state = REPEATER_STATE_IDLE;
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
				rpt_state = REPEATER_STATE_IDLE;
				rx_pr("[RX] receive ksv wait timeout\n");
			}
			if (rx_set_repeat_aksv(receive_hdcp, hdcp_len,
				hdcp_repeat_depth, rx.hdcp.dev_exceed,
				rx.hdcp.cascade_exceed)) {
				rpt_state = REPEATER_STATE_IDLE;
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

int rx_get_tag_code(uint8_t *edid_data)
{
	int tag_code;

	if ((*edid_data >> 5) != 7)
		tag_code = (*edid_data >> 5);
	else
		tag_code = (7 << 8) | *(edid_data + 1);/*extern tag*/

	return tag_code;
}

int rx_get_ceadata_offset(uint8_t *cur_edid, uint8_t *addition)
{
	int i;
	int type;

	if ((cur_edid == NULL) || (addition == NULL))
		return 0;

	type = rx_get_tag_code(addition);
	i = EDID_DEFAULT_START;/*block check start index*/
	while (i < 255) {
		if (type == rx_get_tag_code(cur_edid + i))
			return i;
		i += (1 + (*(cur_edid + i) & 0x1f));
	}
	if (log_level & VIDEO_LOG)
		rx_pr("type: %#x, start addr: %#x\n", type, i);

	return 0;
}

void rx_mix_edid_audio(uint8_t *cur_data, uint8_t *addition)
{
struct edid_audio_block_t *ori_data;
struct edid_audio_block_t *add_data;
unsigned char ori_len, add_len;
int i, j;

if ((cur_data == 0) || (addition == 0) ||
	(*cur_data >> 5) != (*addition >> 5))
	return;

ori_data = (struct edid_audio_block_t *)(cur_data + 1);
add_data = (struct edid_audio_block_t *)(addition + 1);
ori_len = (*cur_data & 0x1f)/FORMAT_SIZE;
add_len = (*addition & 0x1f)/FORMAT_SIZE;

for (i = 0; i < add_len; i++) {
	if (log_level & VIDEO_LOG)
		rx_pr("mix audio format:%d\n", add_data[i].format_code);
	/*only support lpcm dts dd+*/
	if (!is_audio_support(add_data[i].format_code))
		continue;
	/*mix audio data*/
	for (j = 0; j < ori_len; j++) {
		if (ori_data[j].format_code ==
					add_data[i].format_code) {
			if (log_level & VIDEO_LOG)
				rx_pr("mix audio mix format:%d\n",
					add_data[i].format_code);
			/*choose channel is lager*/
			ori_data[j].max_channel =
				((ori_data[j].max_channel >
				add_data[i].max_channel) ?
				ori_data[j].max_channel :
				add_data[i].max_channel);
			/*mix sample freqence*/
			*(((unsigned char *)&ori_data[j]) + 1) =
			*(((unsigned char *)&ori_data[j]) + 1) |
				*(((unsigned char *)&add_data[i]) + 1);
			/*mix bit rate*/
			if (add_data[i].format_code ==
						AUDIO_FORMAT_LPCM)
				*(((unsigned char *)&ori_data[j]) + 2) =
				*(((unsigned char *)&ori_data[j]) + 2) |
				*(((unsigned char *)&add_data[i]) + 2);
			else
				ori_data[j].bit_rate.others =
				add_data[i].bit_rate.others;
		} else {
			if (j == (ori_len - 1)) {
				if (log_level & VIDEO_LOG)
					rx_pr("mix audio add new format: %d\n",
					add_data[i].format_code);
				if (((*cur_data & 0x1f) + FORMAT_SIZE)
							 <= 0x1f) {
					memcpy(cur_data +
						 (*cur_data & 0x1f) + 1,
					&add_data[i], FORMAT_SIZE);
					*cur_data += FORMAT_SIZE;
				}
			}
		}
	}
}
}
void rx_mix_edid_hdr(uint8_t *cur_data, uint8_t *addition)
{
	struct edid_hdr_block_t *cur_block;
	struct edid_hdr_block_t *add_block;

	if ((cur_data == 0) || (addition == 0) ||
		(*cur_data >> 5) != (*addition >> 5))
		return;

	cur_block = (struct edid_hdr_block_t *)(cur_data + 1);
	add_block = (struct edid_hdr_block_t *)(addition + 1);
	if (add_block->max_lumi | add_block->avg_lumi |
					add_block->min_lumi) {
		cur_block->max_lumi = add_block->max_lumi;
		cur_block->avg_lumi = add_block->avg_lumi;
		cur_block->min_lumi = add_block->min_lumi;
		if ((*cur_data & 0x1f) < (*addition & 0x1f))
			*cur_data = *addition;
	}
}

void rx_mix_block(uint8_t *cur_data, uint8_t *addition)
{
	int tag_code;

	if ((cur_data == 0) || (addition == 0) ||
		(*cur_data >> 5) != (*addition >> 5))
		return;
	if (log_level & VIDEO_LOG)
		rx_pr("before type:%d - %d,len:%d - %d\n",
	(*cur_data >> 5), (*addition >> 5),
	(*cur_data & 0x1f), (*addition & 0x1f));

	tag_code = rx_get_tag_code(cur_data);

	switch (tag_code) {
	case EDID_TAG_AUDIO:
		rx_mix_edid_audio(cur_data, addition);
		break;

	case EDID_TAG_HDR:
		rx_mix_edid_hdr(cur_data, addition);
		break;
	}
	if (log_level & VIDEO_LOG)
		rx_pr("end type:%d - %d,len:%d - %d\n",
	(*cur_data >> 5), (*addition >> 5),
	(*cur_data & 0x1f), (*addition & 0x1f));
}

void rx_modify_edid(unsigned char *buffer,
				int len, unsigned char *addition)
{
	int start_addr = 0;/*the addr in edid need modify*/
	int start_addr_temp = 0;/*edid_temp start addr*/
	int temp_len = 0;
	unsigned char *cur_data = NULL;
	int addition_size = 0;
	int cur_size = 0;
	int i;

	if ((len <= 255) || (buffer == 0) || (addition == 0))
		return;

	/*get the mix block value*/
	if (*addition & 0x1f) {
		/*get addition block index in local edid*/
		start_addr = rx_get_ceadata_offset(buffer, addition);
		if (start_addr > EDID_DEFAULT_START) {
			cur_size = (*(buffer + start_addr) & 0x1f) + 1;
			addition_size = (*addition & 0x1f) + 1;
			cur_data = kmalloc(
				addition_size + cur_size, GFP_KERNEL);
			if (cur_data != 0) {
				rx_pr("allocate cur_data memory failed\n");
				return;
			}
			memcpy(cur_data, buffer + start_addr, cur_size);
			/*add addition block property to local edid*/
			rx_mix_block(cur_data, addition);
			addition_size = (*cur_data & 0x1f) + 1;
		} else
			return;
		if (log_level & VIDEO_LOG)
			rx_pr(
			"start_addr: %#x,cur_size: %d,addition_size: %d\n",
			start_addr, cur_size, addition_size);

		/*set the block value to edid_temp*/
		start_addr_temp = rx_get_ceadata_offset(buffer, addition);
		temp_len = ((buffer[start_addr_temp] & 0x1f) + 1);
		if (log_level & VIDEO_LOG)
			rx_pr("edid_temp start: %#x, len: %d\n",
			start_addr_temp, temp_len);
		/*move data behind current data if need*/
		if (temp_len < addition_size) {
			for (i = 0; i < EDID_SIZE - start_addr_temp -
				addition_size; i++) {
				buffer[255 - i] =
				buffer[255 - (addition_size - temp_len)
						 - i];
			}
		} else if (temp_len > addition_size) {
			for (i = 0; i < EDID_SIZE - start_addr_temp -
						temp_len; i++) {
				buffer[start_addr_temp + i + addition_size]
				 = buffer[start_addr_temp + i + temp_len];
			}
		}
		/*check detail description offset if needs modify*/
		if (start_addr_temp < buffer[EDID_BLOCK1_OFFSET +
			EDID_DESCRIP_OFFSET] + EDID_BLOCK1_OFFSET)
			buffer[EDID_BLOCK1_OFFSET + EDID_DESCRIP_OFFSET]
				+= (addition_size - temp_len);
		/*copy current edid data*/
		memcpy(buffer + start_addr_temp, cur_data,
						addition_size);
	}
	kfree(cur_data);
}

void rx_edid_update_audio_info(unsigned char *p_edid,
						unsigned int len)
{
	if (p_edid == NULL)
		return;
	rx_modify_edid(p_edid, len, receive_edid);
}



int rx_set_receiver_edid(unsigned char *data, int len)
{
	if ((data == NULL) || (len == 0) || (len > MAX_RECEIVE_EDID))
		return false;

	memcpy(receive_edid, data, len);
	new_edid = true;
	return true;
}
EXPORT_SYMBOL(rx_set_receiver_edid);

void rx_set_repeater_support(bool enable)
{
	downstream_rp_en = enable;
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
	/*rx_pr("set ksv list len:%d,depth:%d\n", len, depth);*/
	if ((len == 0) || (data == 0) || (depth == 0))
		return false;
	/*set repeat depth*/
	if ((depth <= MAX_REPEAT_DEPTH) && (!cascade_exceed)) {
		hdmirx_wr_bits_dwc(DWC_HDCP_RPT_BSTATUS, MAX_CASCADE_EXCEEDED,
		0);
		hdmirx_wr_bits_dwc(DWC_HDCP_RPT_BSTATUS, DEPTH, depth);
		rx.hdcp.depth = depth;
	} else {
		hdmirx_wr_bits_dwc(DWC_HDCP_RPT_BSTATUS, MAX_CASCADE_EXCEEDED,
		1);
	}
	/*set repeat count*/
	if ((len <= MAX_REPEAT_COUNT) && (!dev_exceed)) {
		hdmirx_wr_bits_dwc(DWC_HDCP_RPT_BSTATUS, MAX_DEVS_EXCEEDED,
		0);
		rx.hdcp.count = len;
		hdmirx_wr_bits_dwc(DWC_HDCP_RPT_BSTATUS, DEVICE_COUNT,
								rx.hdcp.count);
	} else {
		hdmirx_wr_bits_dwc(DWC_HDCP_RPT_BSTATUS, MAX_DEVS_EXCEEDED,
		1);
	}
	/*set repeat status*/
	if (rx.hdcp.count > 0) {
		rx.hdcp.repeat = true;
		hdmirx_wr_bits_dwc(DWC_HDCP_RPT_CTRL, REPEATER, 1);
	} else {
		rx.hdcp.repeat = false;
		hdmirx_wr_bits_dwc(DWC_HDCP_RPT_CTRL, REPEATER, 0);
	}
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
	hdmirx_wr_bits_dwc(DWC_HDCP_RPT_CTRL, KSVLIST_READY,
					(rx.hdcp.count > 0));
	/* Wait for HW completion of V value*/
	rx_poll_dwc(DWC_HDCP_RPT_CTRL, FIFO_READY, FIFO_READY, KSV_V_WR_TH);
	rx_pr("[RX]write Ready signal!\n");

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
	if ((data != 0) && (len != 0) && (len <= MAX_REPEAT_COUNT))
		memcpy(receive_hdcp, data, len*MAX_KSV_SIZE);
	rx_pr("receive ksv list len:%d,depth:%d,cas:%d,dev:%d\n", len,
	depth, cas_exceed, devs_exceed);
	hdcp_len = len;
	hdcp_repeat_depth = depth;
	rx.hdcp.cascade_exceed = cas_exceed;
	rx.hdcp.dev_exceed = devs_exceed;

	return true;
}
EXPORT_SYMBOL(rx_set_receive_hdcp);

void rx_repeat_hpd_state(bool plug)
{
	repeat_plug = plug;
}
EXPORT_SYMBOL(rx_repeat_hpd_state);

void rx_repeat_hdcp_ver(int version)
{

}
EXPORT_SYMBOL(rx_repeat_hdcp_ver);

void rx_edid_physical_addr(int a, int b, int c, int d)
{
	up_phy_addr = ((d & 0xf) << 12)  | ((c & 0xf) << 8) | ((b &
	0xf) << 4) | (a & 0xf);
}
EXPORT_SYMBOL(rx_edid_physical_addr);


