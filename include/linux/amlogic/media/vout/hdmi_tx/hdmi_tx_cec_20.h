/*
 * include/linux/amlogic/media/vout/hdmi_tx/hdmi_tx_cec_20.h
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

#ifndef _TX_CEC_H_
#define _TX_CEC_H_

#include <linux/irq.h>
#include <linux/amlogic/cpu_version.h>
#include "hdmi_info_global.h"
#include "hdmi_tx_module.h"

#define CEC0_LOG_ADDR 4 /* MBX logical address */
#define TV_CEC_INTERVAL     (HZ*3)

#define CEC_VERSION     "v1.3"
#define _RX_DATA_BUF_SIZE_ 16

#define AO_CEC  /* for switch between aocec and hdmi cec2.0 */

#define MAX_MSG           16
#define MAX_NUM_OF_DEV    16

enum _cec_log_dev_addr_e {
	CEC_TV_ADDR					= 0x00,
	CEC_RECORDING_DEVICE_1_ADDR,
	CEC_RECORDING_DEVICE_2_ADDR,
	CEC_TUNER_1_ADDR,
	CEC_PLAYBACK_DEVICE_1_ADDR,
	CEC_AUDIO_SYSTEM_ADDR,
	CEC_TUNER_2_ADDR,
	CEC_TUNER_3_ADDR,
	CEC_PLAYBACK_DEVICE_2_ADDR,
	CEC_RECORDING_DEVICE_3_ADDR,
	CEC_TUNER_4_ADDR,
	CEC_PLAYBACK_DEVICE_3_ADDR,
	CEC_RESERVED_1_ADDR,
	CEC_RESERVED_2_ADDR,
	CEC_FREE_USE_ADDR,
	CEC_UNREGISTERED_ADDR
};

#define CEC_BROADCAST_ADDR CEC_UNREGISTERED_ADDR

#define CEC_TV                   (1 << CEC_TV_ADDR)
#define CEC_RECORDING_DEVICE_1   (1 << CEC_RECORDING_DEVICE_1_ADDR)
#define CEC_RECORDING_DEVICE_2   (1 << CEC_RECORDING_DEVICE_2_ADDR)
#define CEC_TUNER_1              (1 << CEC_TUNER_1_ADDR)
#define CEC_PLAYBACK_DEVICE_1    (1 << CEC_PLAYBACK_DEVICE_1_ADDR)
#define CEC_AUDIO_SYSTEM         (1 << CEC_AUDIO_SYSTEM_ADDR)
#define CEC_TUNER_2              (1 << CEC_TUNER_2_ADDR)
#define CEC_TUNER_3              (1 << CEC_TUNER_3_ADDR)
#define CEC_PLAYBACK_DEVICE_2    (1 << CEC_PLAYBACK_DEVICE_2_ADDR)
#define CEC_RECORDING_DEVICE_3   (1 << CEC_RECORDING_DEVICE_3_ADDR)
#define CEC_TUNER_4              (1 << CEC_TUNER_4_ADDR)
#define CEC_PLAYBACK_DEVICE_3    (1 << CEC_PLAYBACK_DEVICE_3_ADDR)
#define CEC_RESERVED_1           (1 << CEC_RESERVED_1_ADDR)
#define CEC_RESERVED_2           (1 << CEC_RESERVED_2_ADDR)
#define CEC_FREE_USE             (1 << CEC_FREE_USE_ADDR)
#define CEC_UNREGISTERED         (1 << CEC_UNREGISTERED_ADDR)

#define CEC_DISPLAY_DEVICE       (CEC_TV | CEC_FREE_USE)
#define CEC_RECORDING_DEVICE     (CEC_RECORDING_DEVICE_1 \
	    | CEC_RECORDING_DEVICE_2 | CEC_RECORDING_DEVICE_3)
#define CEC_PLAYBACK_DEVICE      (CEC_PLAYBACK_DEVICE_1 \
	    | CEC_PLAYBACK_DEVICE_2 | CEC_PLAYBACK_DEVICE_3)
#define CEC_TUNER_DEVICE         (CEC_TUNER_1 | CEC_TUNER_2 \
	    | CEC_TUNER_3 | CEC_TUNER_4)
#define CEC_AUDIO_SYSTEM_DEVICE  (CEC_AUDIO_SYSTEM)

#define CEC_IOC_MAGIC                   'C'
#define CEC_IOC_GET_PHYSICAL_ADDR       _IOR(CEC_IOC_MAGIC, 0x00, uint16_t)
#define CEC_IOC_GET_VERSION             _IOR(CEC_IOC_MAGIC, 0x01, int)
#define CEC_IOC_GET_VENDOR_ID           _IOR(CEC_IOC_MAGIC, 0x02, uint32_t)
#define CEC_IOC_GET_PORT_INFO           _IOR(CEC_IOC_MAGIC, 0x03, int)
#define CEC_IOC_GET_PORT_NUM            _IOR(CEC_IOC_MAGIC, 0x04, int)
#define CEC_IOC_GET_SEND_FAIL_REASON    _IOR(CEC_IOC_MAGIC, 0x05, uint32_t)
#define CEC_IOC_SET_OPTION_WAKEUP       _IOW(CEC_IOC_MAGIC, 0x06, uint32_t)
#define CEC_IOC_SET_OPTION_ENALBE_CEC   _IOW(CEC_IOC_MAGIC, 0x07, uint32_t)
#define CEC_IOC_SET_OPTION_SYS_CTRL     _IOW(CEC_IOC_MAGIC, 0x08, uint32_t)
#define CEC_IOC_SET_OPTION_SET_LANG     _IOW(CEC_IOC_MAGIC, 0x09, uint32_t)
#define CEC_IOC_GET_CONNECT_STATUS      _IOR(CEC_IOC_MAGIC, 0x0A, uint32_t)
#define CEC_IOC_ADD_LOGICAL_ADDR        _IOW(CEC_IOC_MAGIC, 0x0B, uint32_t)
#define CEC_IOC_CLR_LOGICAL_ADDR        _IOW(CEC_IOC_MAGIC, 0x0C, uint32_t)
#define CEC_IOC_SET_DEV_TYPE            _IOW(CEC_IOC_MAGIC, 0x0D, uint32_t)
#define CEC_IOC_SET_ARC_ENABLE          _IOW(CEC_IOC_MAGIC, 0x0E, uint32_t)
#define CEC_IOC_SET_AUTO_DEVICE_OFF     _IOW(CEC_IOC_MAGIC, 0x0F, uint32_t)
#define CEC_IOC_GET_BOOT_ADDR           _IOW(CEC_IOC_MAGIC, 0x10, uint32_t)
#define CEC_IOC_GET_BOOT_REASON         _IOW(CEC_IOC_MAGIC, 0x11, uint32_t)

#define CEC_FAIL_NONE                   0
#define CEC_FAIL_NACK                   1
#define CEC_FAIL_BUSY                   2
#define CEC_FAIL_OTHER                  3

enum hdmi_port_type {
	HDMI_INPUT = 0,
	HDMI_OUTPUT = 1
};

struct hdmi_port_info {
	int type;
	/* Port ID should start from 1 which corresponds to HDMI "port 1". */
	int port_id;
	int cec_supported;
	int arc_supported;
	uint16_t physical_address;
};

enum cec_dev_type_addr {
	CEC_DISPLAY_DEVICE_TYPE = 0x0,
	CEC_RECORDING_DEVICE_TYPE,
	CEC_RESERVED_DEVICE_TYPE,
	CEC_TUNER_DEVICE_TYPE,
	CEC_PLAYBACK_DEVICE_TYPE,
	CEC_AUDIO_SYSTEM_DEVICE_TYPE,
	CEC_UNREGISTERED_DEVICE_TYPE,
};

enum cec_feature_abort_e {
	CEC_UNRECONIZED_OPCODE = 0x0,
	CEC_NOT_CORRECT_MODE_TO_RESPOND,
	CEC_CANNOT_PROVIDE_SOURCE,
	CEC_INVALID_OPERAND,
	CEC_REFUSED,
	CEC_UNABLE_TO_DETERMINE,
};

/*
 * CEC OPCODES
 */
#define CEC_OC_ABORT_MESSAGE                     0xFF
#define CEC_OC_ACTIVE_SOURCE                     0x82
#define CEC_OC_CEC_VERSION                       0x9E
#define CEC_OC_CLEAR_ANALOGUE_TIMER              0x33
#define CEC_OC_CLEAR_DIGITAL_TIMER               0x99
#define CEC_OC_CLEAR_EXTERNAL_TIMER              0xA1
#define CEC_OC_DECK_CONTROL                      0x42
#define CEC_OC_DECK_STATUS                       0x1B
#define CEC_OC_DEVICE_VENDOR_ID                  0x87
#define CEC_OC_FEATURE_ABORT                     0x00
#define CEC_OC_GET_CEC_VERSION                   0x9F
#define CEC_OC_GET_MENU_LANGUAGE                 0x91
#define CEC_OC_GIVE_AUDIO_STATUS                 0x71
#define CEC_OC_GIVE_DECK_STATUS                  0x1A
#define CEC_OC_GIVE_DEVICE_POWER_STATUS          0x8F
#define CEC_OC_GIVE_DEVICE_VENDOR_ID             0x8C
#define CEC_OC_GIVE_OSD_NAME                     0x46
#define CEC_OC_GIVE_PHYSICAL_ADDRESS             0x83
#define CEC_OC_GIVE_SYSTEM_AUDIO_MODE_STATUS     0x7D
#define CEC_OC_GIVE_TUNER_DEVICE_STATUS          0x08
#define CEC_OC_IMAGE_VIEW_ON                     0x04
#define CEC_OC_INACTIVE_SOURCE                   0x9D
#define CEC_OC_MENU_REQUEST                      0x8D
#define CEC_OC_MENU_STATUS                       0x8E
#define CEC_OC_PLAY                              0x41
#define CEC_OC_POLLING_MESSAGE                   0xFC
#define CEC_OC_RECORD_OFF                        0x0B
#define CEC_OC_RECORD_ON                         0x09
#define CEC_OC_RECORD_STATUS                     0x0A
#define CEC_OC_RECORD_TV_SCREEN                  0x0F
#define CEC_OC_REPORT_AUDIO_STATUS               0x7A
#define CEC_OC_REPORT_PHYSICAL_ADDRESS           0x84
#define CEC_OC_REPORT_POWER_STATUS               0x90
#define CEC_OC_REQUEST_ACTIVE_SOURCE             0x85
#define CEC_OC_ROUTING_CHANGE                    0x80
#define CEC_OC_ROUTING_INFORMATION               0x81
#define CEC_OC_SELECT_ANALOGUE_SERVICE           0x92
#define CEC_OC_SELECT_DIGITAL_SERVICE            0x93
#define CEC_OC_SET_ANALOGUE_TIMER                0x34
#define CEC_OC_SET_AUDIO_RATE                    0x9A
#define CEC_OC_SET_DIGITAL_TIMER                 0x97
#define CEC_OC_SET_EXTERNAL_TIMER                0xA2
#define CEC_OC_SET_MENU_LANGUAGE                 0x32
#define CEC_OC_SET_OSD_NAME                      0x47
#define CEC_OC_SET_OSD_STRING                    0x64
#define CEC_OC_SET_STREAM_PATH                   0x86
#define CEC_OC_SET_SYSTEM_AUDIO_MODE             0x72
#define CEC_OC_SET_TIMER_PROGRAM_TITLE           0x67
#define CEC_OC_STANDBY                           0x36
#define CEC_OC_SYSTEM_AUDIO_MODE_REQUEST         0x70
#define CEC_OC_SYSTEM_AUDIO_MODE_STATUS          0x7E
#define CEC_OC_TEXT_VIEW_ON                      0x0D
#define CEC_OC_TIMER_CLEARED_STATUS              0x43
#define CEC_OC_TIMER_STATUS                      0x35
#define CEC_OC_TUNER_DEVICE_STATUS               0x07
#define CEC_OC_TUNER_STEP_DECREMENT              0x06
#define CEC_OC_TUNER_STEP_INCREMENT              0x05
#define CEC_OC_USER_CONTROL_PRESSED              0x44
#define CEC_OC_USER_CONTROL_RELEASED             0x45
#define CEC_OC_VENDOR_COMMAND                    0x89
#define CEC_OC_VENDOR_COMMAND_WITH_ID            0xA0
#define CEC_OC_VENDOR_REMOTE_BUTTON_DOWN         0x8A
#define CEC_OC_VENDOR_REMOTE_BUTTON_UP           0x8B

/* cec global struct */

enum cec_node_status_e {
	STATE_UNKNOWN = 0x00,
	STATE_START,
	STATE_STOP
};

enum cec_power_status_e {
	POWER_ON = 0x00,
	POWER_STANDBY,
	TRANS_STANDBY_TO_ON,
	TRANS_ON_TO_STANDBY,
};

enum status_req_mode_e {
	STATUS_REQ_ON = 1,
	STATUS_REQ_OFF,
	STATUS_REQ_ONCE,
};

enum deck_info_e {
	DECK_UNKNOWN_STATUS = 0,
	DECK_PLAY = 0X11,
	DECK_RECORD,
	DECK_PLAY_REVERSE,
	DECK_STILL,
	DECK_SLOW,
	DECK_SLOW_REVERSE,
	DECK_FAST_FORWARD,
	DECK_FAST_REVERSE,
	DECK_NO_MEDIA,
	DECK_STOP,
	DECK_SKIP_FORWARD_WIND,
	DECK_SKIP_REVERSE_REWIND,
	DECK_INDEX_SEARCH_FORWARD,
	DECK_INDEX_SEARCH_REVERSE,
	DECK_OTHER_STATUS,
};

enum deck_cnt_mode_e {
	DECK_CNT_SKIP_FORWARD_WIND = 1,
	DECK_CNT_SKIP_REVERSE_REWIND,
	DECK_CNT_STOP,
	DECK_CNT_EJECT,
};

enum play_mode_e {
	PLAY_FORWARD = 0X24,
	PLAY_REVERSE = 0X20,
	PLAY_STILL = 0X25,
	FAST_FORWARD_MIN_SPEED = 0X05,
	FAST_FORWARD_MEDIUM_SPEED = 0X06,
	FAST_FORWARD_MAX_SPEED = 0X07,
	FAST_REVERSE_MIN_SPEED = 0X09,
	FAST_REVERSE_MEDIUM_SPEED = 0X0A,
	FAST_REVERSE_MAX_SPEED = 0X0B,
	SLOW_FORWARD_MIN_SPEED = 0X15,
	SLOW_FORWARD_MEDIUM_SPEED = 0X16,
	SLOW_FORWARD_MAX_SPEED = 0X17,
	SLOW_REVERSE_MIN_SPEED = 0X19,
	SLOW_REVERSE_MEDIUM_SPEED = 0X1A,
	SLOW_REVERSE_MAX_SPEED = 0X1B,
};

enum cec_version_e {
	CEC_VERSION_11 = 0,
	CEC_VERSION_12,
	CEC_VERSION_12A,
	CEC_VERSION_13,
	CEC_VERSION_13A,
	CEC_VERSION_14A,
	CEC_VERSION_20,
};


#define INFO_MASK_CEC_VERSION                (1<<0)
#define INFO_MASK_VENDOR_ID                  (1<<1)
#define INFO_MASK_DEVICE_TYPE                (1<<2)
#define INFO_MASK_POWER_STATUS               (1<<3)
#define INFO_MASK_PHYSICAL_ADDRESS           (1<<4)
#define INFO_MASK_LOGIC_ADDRESS              (1<<5)
#define INFO_MASK_OSD_NAME                   (1<<6)
#define INFO_MASK_MENU_STATE                 (1<<7)
#define INFO_MASK_MENU_LANGUAGE              (1<<8)
#define INFO_MASK_DECK_INfO                  (1<<9)
#define INFO_MASK_PLAY_MODE                  (1<<10)

/*CEC UI MASK*/
/*
#define CEC_FUNC_MSAK                        0
#define ONE_TOUCH_PLAY_MASK                  1
#define ONE_TOUCH_STANDBY_MASK               2
#define AUTO_POWER_ON_MASK                   3
*/

/*
 * only for 1 tx device
 */
struct cec_global_info_t {
	dev_t dev_no;
	atomic_t open_count;
	unsigned int hal_ctl;			/* message controlled by hal */
	unsigned int vendor_id:24;
	unsigned int menu_lang;
	unsigned int cec_version;
	unsigned char power_status;
	unsigned char log_addr;
	unsigned char menu_status;
	unsigned char osd_name[16];
	struct input_dev *remote_cec_dev;	/* cec input device */
	struct hdmitx_dev *hdmitx_device;
};

enum cec_device_menu_state_e {
	DEVICE_MENU_ACTIVE = 0,
	DEVICE_MENU_INACTIVE,
};

int cec_ll_tx(const unsigned char *msg, unsigned char len);
int cec_ll_rx(unsigned char *msg, unsigned char *len);
extern void cec_enable_arc_pin(bool enable);
#endif

