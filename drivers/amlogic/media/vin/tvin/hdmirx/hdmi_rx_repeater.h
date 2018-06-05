/*
 * drivers/amlogic/media/vin/tvin/hdmirx/hdmirx_repeater.h
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

#ifndef __HDMIRX_REPEATER__
#define __HDMIRX_REPEATER__

/* EDID */
#define MAX_RECEIVE_EDID	33
#define MAX_KSV_SIZE		5
#define MAX_REPEAT_COUNT	127
#define MAX_REPEAT_DEPTH	7
#define MAX_KSV_LIST_SIZE	(MAX_KSV_SIZE*MAX_REPEAT_COUNT)
#define DETAILED_TIMING_LEN	18
/*size of one format in edid*/
#define FORMAT_SIZE			sizeof(struct edid_audio_block_t)
#define EDID_DEFAULT_START		132
#define EDID_DESCRIP_OFFSET		2
#define EDID_BLOCK1_OFFSET		128
#define KSV_LIST_WR_TH			100
#define KSV_V_WR_TH			500
#define KSV_LIST_WR_MAX			5
#define KSV_LIST_WAIT_DELAY		500/*according to the timer,5s*/

#define is_audio_support(x) (((x) == AUDIO_FORMAT_LPCM) || \
		((x) == AUDIO_FORMAT_DTS) || ((x) == AUDIO_FORMAT_DDP))

enum repeater_state_e {
	REPEATER_STATE_WAIT_KSV,
	REPEATER_STATE_WAIT_ACK,
	REPEATER_STATE_IDLE,
	REPEATER_STATE_START,
};

extern int receive_edid_len;
extern bool new_edid;
extern int hdcp_array_len;
extern int hdcp_len;
extern int hdcp_repeat_depth;
extern bool new_hdcp;
extern bool repeat_plug;
extern int up_phy_addr;/*d c b a 4bit*/
extern bool downstream_rp_en;

void rx_set_repeater_support(bool enable);
extern int rx_set_receiver_edid(unsigned char *data, int len);
extern void rx_start_repeater_auth(void);
extern void rx_set_repeat_signal(bool repeat);
extern bool rx_set_repeat_aksv(unsigned char *data, int len, int depth,
		bool dev_exceed, bool cascade_exceed);
extern unsigned char *rx_get_receiver_edid(void);
extern void repeater_dwork_handle(struct work_struct *work);
bool rx_set_receive_hdcp(unsigned char *data,
	int len, int depth, bool cas_exceed, bool devs_exceed);
void rx_repeat_hpd_state(bool plug);
void rx_repeat_hdcp_ver(int version);
void rx_check_repeat(void);
extern bool hdmirx_is_key_write(void);



#endif
