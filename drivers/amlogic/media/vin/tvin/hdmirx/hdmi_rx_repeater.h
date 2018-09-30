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
#define MAX_RECEIVE_EDID	40/*33*/
#define MAX_HDR_LUMI		3
#define MAX_KSV_SIZE		5
#define MAX_REPEAT_DEPTH	7
#define MAX_KSV_LIST_SIZE	sizeof(struct hdcp14_topo_s)
#define HDCP14_KSV_MAX_COUNT	127
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

struct hdcp14_topo_s {
	unsigned char max_cascade_exceeded:1;
	unsigned char depth:3;
	unsigned char max_devs_exceeded:1;
	unsigned char device_count:7; /* 1 ~ 127 */
	unsigned char ksv_list[HDCP14_KSV_MAX_COUNT * 5];
};

struct hdcp_hw_info_s {
	unsigned int cur_5v:4;
	unsigned int open:4;
	unsigned int frame_rate:8;
	unsigned int signal_stable:1;
	unsigned int reseved:15;
};

extern int receive_edid_len;
extern bool new_edid;
extern int hdcp_array_len;
extern int hdcp_len;
extern int hdcp_repeat_depth;
extern bool new_hdcp;
extern bool repeat_plug;
extern int up_phy_addr;/*d c b a 4bit*/

extern void rx_set_repeater_support(bool enable);
extern int rx_set_receiver_edid(const char *data, int len);
extern void rx_start_repeater_auth(void);
extern void rx_set_repeat_signal(bool repeat);
extern bool rx_set_repeat_aksv(unsigned char *data, int len, int depth,
		bool dev_exceed, bool cascade_exceed);
extern unsigned char *rx_get_dw_edid_addr(void);
extern void repeater_dwork_handle(struct work_struct *work);
bool rx_set_receive_hdcp(unsigned char *data,
	int len, int depth, bool cas_exceed, bool devs_exceed);
void rx_repeat_hpd_state(bool plug);
void rx_repeat_hdcp_ver(int version);
void rx_check_repeat(void);
extern bool hdmirx_is_key_write(void);
extern void rx_reload_firm_reset(int reset);
extern void rx_firm_reset_end(void);
extern unsigned char *rx_get_dw_hdcp_addr(void);
extern unsigned char *rx_get_dw_edid_addr(void);


#endif
