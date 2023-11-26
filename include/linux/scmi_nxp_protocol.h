/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * SCMI Message Protocol driver NXP extension header
 *
 * Copyright (C) 2018-2021 ARM Ltd.
 */

#ifndef _LINUX_SCMI_NXP_PROTOCOL_H
#define _LINUX_SCMI_NXP_PROTOCOL_H

#include <linux/bitfield.h>
#include <linux/device.h>
#include <linux/notifier.h>
#include <linux/types.h>

enum scmi_nxp_protocol {
	SCMI_PROTOCOL_IMX_BBM = 0x81,
};

struct scmi_imx_bbm_proto_ops {
	int (*rtc_time_set)(const struct scmi_protocol_handle *ph, u32 id, uint64_t sec);
	int (*rtc_time_get)(const struct scmi_protocol_handle *ph, u32 id, u64 *val);
	int (*rtc_alarm_set)(const struct scmi_protocol_handle *ph, u32 id, u64 sec);
	int (*button_get)(const struct scmi_protocol_handle *ph, u32 *state);
};

enum scmi_nxp_notification_events {
	SCMI_EVENT_IMX_BBM_RTC = 0x0,
	SCMI_EVENT_IMX_BBM_BUTTON = 0x1,
};

#define SCMI_IMX_BBM_RTC_TIME_SET	0x6
#define SCMI_IMX_BBM_RTC_TIME_GET	0x7
#define SCMI_IMX_BBM_RTC_ALARM_SET	0x8
#define SCMI_IMX_BBM_BUTTON_GET		0x9

struct scmi_imx_bbm_notif_report {
	bool			is_rtc;
	bool			is_button;
	ktime_t			timestamp;
	unsigned int		rtc_id;
	unsigned int		rtc_evt;
};

#endif
