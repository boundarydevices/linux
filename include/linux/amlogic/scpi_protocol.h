/*
 * include/linux/amlogic/scpi_protocol.h
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

#ifndef _SCPI_PROTOCOL_H_
#define _SCPI_PROTOCOL_H_
#include <linux/types.h>

enum scpi_client_id {
	SCPI_CL_NONE,
	SCPI_CL_CLOCKS,
	SCPI_CL_DVFS,
	SCPI_CL_POWER,
	SCPI_CL_THERMAL,
	SCPI_CL_REMOTE,
	SCPI_CL_LED_TIMER,
	SCPI_MAX,
};

enum scpi_std_cmd {
	SCPI_CMD_INVALID		= 0x00,
	SCPI_CMD_SCPI_READY		= 0x01,
	SCPI_CMD_SCPI_CAPABILITIES	= 0x02,
	SCPI_CMD_EVENT			= 0x03,
	SCPI_CMD_SET_CSS_PWR_STATE	= 0x04,
	SCPI_CMD_GET_CSS_PWR_STATE	= 0x05,
	SCPI_CMD_CFG_PWR_STATE_STAT	= 0x06,
	SCPI_CMD_GET_PWR_STATE_STAT	= 0x07,
	SCPI_CMD_SYS_PWR_STATE		= 0x08,
	SCPI_CMD_L2_READY		= 0x09,
	SCPI_CMD_SET_AP_TIMER		= 0x0a,
	SCPI_CMD_CANCEL_AP_TIME		= 0x0b,
	SCPI_CMD_DVFS_CAPABILITIES	= 0x0c,
	SCPI_CMD_GET_DVFS_INFO		= 0x0d,
	SCPI_CMD_SET_DVFS		= 0x0e,
	SCPI_CMD_GET_DVFS		= 0x0f,
	SCPI_CMD_GET_DVFS_STAT		= 0x10,
	SCPI_CMD_SET_RTC		= 0x11,
	SCPI_CMD_GET_RTC		= 0x12,
	SCPI_CMD_CLOCK_CAPABILITIES	= 0x13,
	SCPI_CMD_SET_CLOCK_INDEX	= 0x14,
	SCPI_CMD_SET_CLOCK_VALUE	= 0x15,
	SCPI_CMD_GET_CLOCK_VALUE	= 0x16,
	SCPI_CMD_PSU_CAPABILITIES	= 0x17,
	SCPI_CMD_SET_PSU		= 0x18,
	SCPI_CMD_GET_PSU		= 0x19,
	SCPI_CMD_SENSOR_CAPABILITIES	= 0x1a,
	SCPI_CMD_SENSOR_INFO		= 0x1b,
	SCPI_CMD_SENSOR_VALUE		= 0x1c,
	SCPI_CMD_SENSOR_CFG_PERIODIC	= 0x1d,
	SCPI_CMD_SENSOR_CFG_BOUNDS	= 0x1e,
	SCPI_CMD_SENSOR_ASYNC_VALUE	= 0x1f,
	SCPI_CMD_SET_USR_DATA = 0x20,
	SCPI_CMD_OSCRING_VALUE = 0x43,
	SCPI_CMD_WAKEUP_REASON_GET = 0x30,
	SCPI_CMD_WAKEUP_REASON_CLR = 0X31,
	SCPI_CMD_GET_ETHERNET_CALC = 0x32,

	SCPI_CMD_GET_CEC1		= 0xB4,
	SCPI_CMD_GET_CEC2		= 0xB5,
	SCPI_CMD_COUNT
};

struct scpi_opp_entry {
	u32 freq_hz;
	u32 volt_mv;
} __packed;

struct scpi_dvfs_info {
	unsigned int count;
	unsigned int latency; /* in usecs */
	struct scpi_opp_entry *opp;
} __packed;


unsigned long scpi_clk_get_val(u16 clk_id);
int scpi_clk_set_val(u16 clk_id, unsigned long rate);
int scpi_dvfs_get_idx(u8 domain);
int scpi_dvfs_set_idx(u8 domain, u8 idx);
struct scpi_dvfs_info *scpi_dvfs_get_opps(u8 domain);
int scpi_get_sensor(char *name);
int scpi_get_sensor_value(u16 sensor, u32 *val);
int scpi_send_usr_data(u32 client_id, u32 *val, u32 size);
int scpi_get_vrtc(u32 *p_vrtc);
int scpi_set_vrtc(u32 vrtc_val);
int scpi_get_ring_value(unsigned char *val);
int scpi_get_wakeup_reason(u32 *wakeup_reason);
int scpi_clr_wakeup_reason(void);
int scpi_get_cec_val(enum scpi_std_cmd index, u32 *p_cec);
u8  scpi_get_ethernet_calc(void);
#endif /*_SCPI_PROTOCOL_H_*/
