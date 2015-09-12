/*
 * Copyright(c) 2015, Analogix Semiconductor. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __SLIMPORT_TX_DRV_H
#define __SLIMPORT_TX_DRV_H

#include "anx78xx.h"
#include "slimport_tx_reg.h"

#define FW_VERSION	0x22

#define DVI_MODE	0x00
#define HDMI_MODE	0x01

#define SP_POWER_ON	1
#define SP_POWER_DOWN	0

#define MAX_BUF_CNT	16

#define SP_BREAK(current_status, next_status) \
	{ if (next_status != (current_status) + 1) break; }

enum rx_cbl_type {
	DWN_STRM_IS_NULL,
	DWN_STRM_IS_HDMI,
	DWN_STRM_IS_DIGITAL,
	DWN_STRM_IS_ANALOG,
	DWN_STRM_NUM
};

enum sp_tx_state {
	STATE_WAITTING_CABLE_PLUG,
	STATE_SP_INITIALIZED,
	STATE_SINK_CONNECTION,
	STATE_PARSE_EDID,
	STATE_LINK_TRAINING,
	STATE_VIDEO_OUTPUT,
	STATE_HDCP_AUTH,
	STATE_AUDIO_OUTPUT,
	STATE_PLAY_BACK
};

enum sp_tx_power_block {
	SP_TX_PWR_REG = REGISTER_PD,
	SP_TX_PWR_HDCP = HDCP_PD,
	SP_TX_PWR_AUDIO = AUDIO_PD,
	SP_TX_PWR_VIDEO = VIDEO_PD,
	SP_TX_PWR_LINK = LINK_PD,
	SP_TX_PWR_TOTAL = TOTAL_PD,
	SP_TX_PWR_NUMS
};

enum hdmi_color_depth {
	HDMI_LEGACY = 0x00,
	HDMI_24BIT = 0x04,
	HDMI_30BIT = 0x05,
	HDMI_36BIT = 0x06,
	HDMI_48BIT = 0x07,
};

enum sp_tx_send_msg {
	MSG_OCM_EN,
	MSG_INPUT_HDMI,
	MSG_INPUT_DVI,
	MSG_CLEAR_IRQ,
};

enum sink_connection_status {
	SC_INIT,
	SC_CHECK_CABLE_TYPE,
	SC_WAITTING_CABLE_TYPE = SC_CHECK_CABLE_TYPE+5,
	SC_SINK_CONNECTED,
	SC_NOT_CABLE,
	SC_STATE_NUM
};

enum cable_type_status {
	CHECK_AUXCH,
	GETTED_CABLE_TYPE,
	CABLE_TYPE_STATE_NUM
};

enum sp_tx_lt_status {
	LT_INIT,
	LT_WAIT_PLL_LOCK,
	LT_CHECK_LINK_BW,
	LT_START,
	LT_WAITTING_FINISH,
	LT_ERROR,
	LT_FINISH,
	LT_END,
	LT_STATES_NUM
};

enum hdcp_status {
	HDCP_CAPABLE_CHECK,
	HDCP_WAITTING_VID_STB,
	HDCP_HW_ENABLE,
	HDCP_WAITTING_FINISH,
	HDCP_FINISH,
	HDCP_FAILE,
	HDCP_NOT_SUPPORT,
	HDCP_PROCESS_STATE_NUM
};

enum video_output_status {
	VO_WAIT_VIDEO_STABLE,
	VO_WAIT_TX_VIDEO_STABLE,
	VO_CHECK_VIDEO_INFO,
	VO_FINISH,
	VO_STATE_NUM
};

enum audio_output_status {
	AO_INIT,
	AO_CTS_RCV_INT,
	AO_AUDIO_RCV_INT,
	AO_RCV_INT_FINISH,
	AO_OUTPUT,
	AO_STATE_NUM
};

struct packet_avi {
	u8 avi_data[13];
};


struct packet_spd {
	u8 spd_data[25];
};

struct packet_mpeg {
	u8 mpeg_data[13];
};

struct audio_info_frame {
	u8 type;
	u8 version;
	u8 length;
	u8 pb_byte[11];
};

enum packets_type {
	AVI_PACKETS,
	SPD_PACKETS,
	MPEG_PACKETS,
	VSI_PACKETS,
	AUDIF_PACKETS
};

struct common_int {
	u8 common_int[5];
	u8 change_flag;
};

struct hdmi_rx_int {
	u8 hdmi_rx_int[7];
	u8 change_flag;
};

enum xtal_enum {
	XTAL_19D2M,
	XTAL_24M,
	XTAL_25M,
	XTAL_26M,
	XTAL_27M,
	XTAL_38D4M,
	XTAL_52M,
	XTAL_NOT_SUPPORT,
	XTAL_CLK_NUM
};

enum sp_ssc_dep {
	SSC_DEP_DISABLE = 0x0,
	SSC_DEP_500PPM,
	SSC_DEP_1000PPM,
	SSC_DEP_1500PPM,
	SSC_DEP_2000PPM,
	SSC_DEP_2500PPM,
	SSC_DEP_3000PPM,
	SSC_DEP_3500PPM,
	SSC_DEP_4000PPM,
	SSC_DEP_4500PPM,
	SSC_DEP_5000PPM,
	SSC_DEP_5500PPM,
	SSC_DEP_6000PPM
};

struct anx78xx_clock_data {
	unsigned char xtal_clk;
	unsigned int xtal_clk_m10;
};

bool sp_chip_detect(struct anx78xx *anx78xx);

void sp_main_process(struct anx78xx *anx78xx);

void sp_tx_variable_init(void);

u8 sp_tx_cur_states(void);

void sp_tx_clean_state_machine(void);

#endif
