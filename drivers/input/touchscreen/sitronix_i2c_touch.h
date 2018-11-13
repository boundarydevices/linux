/*
 * drivers/input/touchscreen/sitronix_i2c_touch.h
 *
 * Touchscreen driver for Sitronix
 *
 * Copyright (C) 2011 Sitronix Technology Co., Ltd.
 *	Rudy Huang <rudy_huang@sitronix.com.tw>
 */
/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#ifndef __SITRONIX_I2C_TOUCH_h
#define __SITRONIX_I2C_TOUCH_h

#include <linux/kernel.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <asm/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/io.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif


#include <linux/device.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/async.h>
#include <linux/ioport.h>
#include <asm/irq.h>
#include <asm/delay.h>
#include <linux/irq.h>
#include <linux/gpio.h>
//#include <mach/irqs.h>
//#include <soc/sprd/hardware.h>
//#include <mach/sys_config.h>
//#include <linux/init-input.h>

#include <linux/ioctl.h> /* needed for the _IOW etc stuff used later */



#define SITRONIX_TOUCH_DRIVER_VERSION 0x03
#define SITRONIX_MAX_SUPPORTED_POINT 10
#define SITRONIX_I2C_TOUCH_DRV_NAME "sitronix"
#define SITRONIX_I2C_TOUCH_DEV_NAME "sitronixDev"
//#define SITRONIX_I2C_TOUCH_MT_INPUT_DEV_NAME "sitronix"
#define SITRONIX_I2C_TOUCH_MT_INPUT_DEV_NAME "SITRONIX"
#define SITRONIX_I2C_TOUCH_KEY_INPUT_DEV_NAME "sitronix-i2c-touch-key"

//#define CONFIG_ARCH_SUNXI

#ifdef CONFIG_MACH_DEVKIT8000
#define SITRONIX_RESET_GPIO	170
#define SITRONIX_INT_GPIO	157
#elif defined(CONFIG_MACH_OMAP4_PANDA)
#define SITRONIX_RESET_GPIO	44
#define SITRONIX_INT_GPIO	47
#elif defined(CONFIG_ARCH_MSM8X60)
#define SITRONIX_RESET_GPIO	58
#define SITRONIX_INT_GPIO	61
#endif // CONFIG_MACH_DEVKIT8000

// MT SLOT feature is implmented in linux kernel 2.6.38 and later. Make sure that version of your linux kernel before using this feature.
#define SITRONIX_SUPPORT_MT_SLOT
//#define SITRONIX_SWAP_XY
#define SITRONIX_I2C_COMBINED_MESSAGE
#ifndef SITRONIX_I2C_COMBINED_MESSAGE
#define SITRONIX_I2C_SINGLE_MESSAGE
#endif // SITRONIX_I2C_COMBINED_MESSAGE
//#define SITRONIX_MONITOR_THREAD
#define DELAY_MONITOR_THREAD_START_PROBE 10000
#define DELAY_MONITOR_THREAD_START_RESUME 3000
#define SITRONIX_FW_UPGRADE_FEATURE
//#define SITRONIX_PERMISSION_THREAD
#define SITRONIX_SYSFS
#define SITRONIX_LEVEL_TRIGGERED

// When enable_irq() is invoked, irq will be sent once while INT is not triggered if CONFIG_HARDIRQS_SW_RESEND is set.
// This behavior is implemented by linux kernel, it is used to prevent irq from losting when irq is edge-triggered mode.
#ifndef SITRONIX_LEVEL_TRIGGERED
#define SITRONIX_INT_POLLING_MODE
#define INT_POLLING_MODE_INTERVAL 14
#endif // SITRONIX_LEVEL_TRIGGERED
//#define SITRONIX_IDENTIFY_ID
//#define SITRONIX_MULTI_SLAVE_ADDR

//#define EnableDbgMsg 1
//#define EnableUpgradeMsg 1

#ifdef EnableDbgMsg
#define DbgMsg(arg...) printk(arg)
#else
#define DbgMsg(arg...)
#endif

#define ErrorMsg(fmt,arg...)	printk(KERN_ERR"<<-SITRONIX-ERROR->> "fmt"\n",##arg)

#ifdef EnableUpgradeMsg
#define UpgradeMsg(arg...) printk(arg)
#else
#define UpgradeMsg(arg...)
#endif

#define CTP_AP_SP_SYNC_NONE	0
#define CTP_AP_SP_SYNC_APP		(1 << 0)
#define CTP_AP_SP_SYNC_GPIO	(1 << 1)
#define CTP_AP_SP_SYNC_BOTH	(CTP_AP_SP_SYNC_GPIO | CTP_AP_SP_SYNC_APP)

#define CTP_AP_SP_SYNC_WAY	CTP_AP_SP_SYNC_BOTH

typedef enum{
	FIRMWARE_VERSION,
	STATUS_REG,
	DEVICE_CONTROL_REG,
	TIMEOUT_TO_IDLE_REG,
	XY_RESOLUTION_HIGH,
	X_RESOLUTION_LOW,
	Y_RESOLUTION_LOW,
	DEVICE_CONTROL_REG2 = 0x09,
	FIRMWARE_REVISION_3 = 0x0C,
	FIRMWARE_REVISION_2,
	FIRMWARE_REVISION_1,
	FIRMWARE_REVISION_0,
	FINGERS,
	KEYS_REG,
	XY0_COORD_H,
	X0_COORD_L,
	Y0_COORD_L,
	I2C_PROTOCOL = 0x3E,
	MAX_NUM_TOUCHES,
	DATA_0_HIGH,
	DATA_0_LOW,
	MISC_CONTROL = 0xF1,
	SMART_WAKE_UP_REG = 0xF2,
	CHIP_ID = 0xF4,
	PAGE_REG = 0xff,
}RegisterOffset;


//#define SITRONIX_SMART_WAKE_UP
#ifdef SITRONIX_SMART_WAKE_UP
typedef enum{
	SWK_NO = 0x0,
	CHARACTER_C	    = 0x63,
	CHARACTER_E	    = 0x65,
	CHARACTER_M	    = 0x6D,
	CHARACTER_O	    = 0x6F,
	CHARACTER_S	    = 0x73,
	CHARACTER_V	    = 0x76,
	CHARACTER_W	    = 0x77,
	CHARACTER_Z	    = 0x7A,
	LEFT_TO_RIGHT_SLIDE = 0xB0,
	RIGHT_TO_LEFT_SLIDE = 0xB4,
	TOP_TO_DOWN_SLIDE   = 0xB8,
	DOWN_TO_UP_SLIDE    = 0xBC,
	DOUBLE_CLICK	    = 0xC0,
	SINGLE_CLICK	    = 0xC8,
}SWK_ID;
#endif

//#define SITRONIX_GESTURE
#ifdef SITRONIX_GESTURE
typedef enum{
	G_NO			 = 0x0,
	G_ZOOM_IN		 = 0x2,
	G_ZOOM_OUT	 = 0x3,
	G_L_2_R		 = 0x4,
	G_R_2_L		 = 0x5,
	G_T_2_D		 = 0x6,
	G_D_2_T		 = 0x7,
	G_PALM			 = 0x8,
	G_SINGLE_TAB	 = 0x9,
}GESTURE_ID;
#endif

#define SITRONIX_TS_CHANGE_MODE_DELAY 150

struct touch_data_a {
	u8 xy_coord_h;
	u8 x_coord_l;
	u8 y_coord_l;
};

struct touch_data_b {
	u8 xy_coord_h;
	u8 x_coord_l;
	u8 y_coord_l;
	u8 spare;
};

#define X_RES_H_SHFT 4
#define X_RES_H_BMSK 0xf
#define Y_RES_H_SHFT 0
#define Y_RES_H_BMSK 0xf
#define FINGERS_SHFT 0
#define FINGERS_BMSK 0xf
#define X_COORD_VALID_SHFT 7
#define X_COORD_VALID_BMSK 0x1
#define X_COORD_H_SHFT 4
#define X_COORD_H_BMSK 0x7
#define Y_COORD_H_SHFT 0
#define Y_COORD_H_BMSK 0x7

typedef enum{
	SITRONIX_RESERVED_TYPE_0,
	SITRONIX_A_TYPE,
	SITRONIX_B_TYPE,
}I2C_PROTOCOL_TYPE;

#define I2C_PROTOCOL_SHFT 0x0
#define I2C_PROTOCOL_BMSK 0x3

typedef enum{
	SENSING_BOTH,
	SENSING_X_ONLY,
	SENSING_Y_ONLY,
	SENSING_BOTH_NOT,
}ONE_D_SENSING_CONTROL_MODE;

#define ONE_D_SENSING_CONTROL_SHFT 0x2
#define ONE_D_SENSING_CONTROL_BMSK 0x3

#define SMT_IOC_MAGIC   0xf1

enum{
	SMT_GET_DRIVER_REVISION = 1,
	SMT_GET_FW_REVISION,
	SMT_ENABLE_IRQ,
	SMT_DISABLE_IRQ,
	SMT_RESUME,
	SMT_SUSPEND,
	SMT_HW_RESET,
	SMT_REPROBE,
	SMT_RAW_TEST,
	SMT_IOC_MAXNR,
};

#define IOCTL_SMT_GET_DRIVER_REVISION				_IOC(_IOC_READ,  SMT_IOC_MAGIC, SMT_GET_DRIVER_REVISION,			1)
#define IOCTL_SMT_GET_FW_REVISION					_IOC(_IOC_READ,  SMT_IOC_MAGIC, SMT_GET_FW_REVISION,				4)
#define IOCTL_SMT_ENABLE_IRQ					_IOC(_IOC_NONE, SMT_IOC_MAGIC, SMT_ENABLE_IRQ, 0)
#define IOCTL_SMT_DISABLE_IRQ					_IOC(_IOC_NONE, SMT_IOC_MAGIC, SMT_DISABLE_IRQ, 0)
#define IOCTL_SMT_RESUME					_IOC(_IOC_NONE, SMT_IOC_MAGIC, SMT_RESUME, 0)
#define IOCTL_SMT_SUSPEND					_IOC(_IOC_NONE, SMT_IOC_MAGIC, SMT_SUSPEND, 0)
#define IOCTL_SMT_HW_RESET					_IOC(_IOC_NONE, SMT_IOC_MAGIC, SMT_HW_RESET, 0)
#define IOCTL_SMT_REPROBE					_IOC(_IOC_NONE, SMT_IOC_MAGIC, SMT_REPROBE, 0)
#define IOCTL_SMT_RAW_TEST					_IOC(_IOC_NONE, SMT_IOC_MAGIC, SMT_RAW_TEST, 0)


//#define SITRONIX_AA_KEY
#define SITRONIX_KEY_BOUNDARY_MANUAL_SPECIFY

typedef struct {
	u8 y_h		:3,
	   reserved	:1,
	   x_h		:3,
	   valid	:1;
	u8 x_l;
	u8 y_l;
	u8 z;
}xy_data_t;

typedef struct {
	xy_data_t	xy_data[SITRONIX_MAX_SUPPORTED_POINT];
}stx_report_data_t;

struct sitronix_sensor_key_t{
	unsigned int code;
};

#ifndef SITRONIX_AA_KEY
enum{
	AREA_NONE,
	AREA_DISPLAY,
};
#else
enum{
	AREA_NONE,
	AREA_DISPLAY,
	AREA_KEY,
	AREA_INVALID,
};

struct sitronix_AA_key{
	int x_low;
	int x_high;
	int y_low;
	int y_high;
	unsigned int code;
};
#endif // SITRONIX_AA_KEY

typedef struct {
	uint8_t offset;
	uint8_t shft;
	uint8_t bmsk;
}sitronix_reg_field;

typedef struct {
	sitronix_reg_field dis_coord_flag;
}sitronix_i2c_protocol_map;
#if 0
static sitronix_i2c_protocol_map sitronix_i2c_ptcl_v1 = {
	.dis_coord_flag = {
		.offset = 0x09,
		.shft = 0,
		.bmsk = 0x1,
	},
};

static sitronix_i2c_protocol_map sitronix_i2c_ptcl_v2 = {
	.dis_coord_flag = {
		.offset = 0xF1,
		.shft = 2,
		.bmsk = 0x1,
	},
};
#endif
#define GTP_ERROR(fmt,arg...)          printk("<<-GTP-ERROR->> "fmt"\n",##arg)

struct sitronix_i2c_touch_platform_data {
	uint32_t version;	/* Use this entry for panels with */
				/* (major << 8 | minor) version or above. */
				/* If non-zero another array entry follows */
	void (*reset_ic)(void);
	u32 num_max_touches;
	u32 soft_rst_dly;
	const char *product_id;
	const char *fw_name;
	char *name;
	u32 x_max;
	u32 y_max;
	u32 x_min;
	u32 y_min;
	u32 panel_minx;
	u32 panel_miny;
	u32 panel_maxx;
	u32 panel_maxy;
	bool no_force_update;
	bool i2c_pull_up;
	bool enable_power_off;
	bool fw_vkey_support;
	u32 button_map[4];
	u8 num_button;
	//size_t config_data_len[GOODIX_MAX_CFG_GROUP];
	//u8 *config_data[GOODIX_MAX_CFG_GROUP];
};
//petitk add

//#define CONFIG_TOUCHSCREEN_SITRONIX_I2C_TOUCH
//#define KERN_DEBUG    "<7>"    /* debug-level messages */
////////////

#define stmsg(format, ...)	\
	printk("[sitronix 000] " format "\n", ## __VA_ARGS__)
//Upgrade
//#define ST_UPGRADE_FIRMWARE


#ifdef ST_UPGRADE_FIRMWARE
//#define ST_UPGRADE_BY_ID
//#define ST_REPLACE_CFG
//#define ST_FIREWARE_FILE
#define ST_IC_A8008
//#define ST_IC_A8010

#ifdef ST_IC_A8008
#define ST_FW_LEN	0x4000
#define ST_CFG_LEN	0xFE
#define ST_CFG_OFFSET	0x3F00
#define ST_FLASH_PAGE_SIZE 1024
#define ST_FLASH_SIZE 0x3FFE
#define ST_CHECK_FW_OFFSET	0x3F00

#else
#define ST_FW_LEN	0x6900
#define ST_CFG_LEN	0x2C0
#define ST_CFG_OFFSET	0xBC00
#define ST_FLASH_PAGE_SIZE 1024
#define ST_FLASH_SIZE 0xBEC0
#define ST_CHECK_FW_OFFSET	0x6900
#endif


#define ISP_PACKET_SIZE 8
#define ST_FW_INFO_LEN	0x10

#define ISP_CMD_ERASE						0x80
#define ISP_CMD_SEND_DATA					0x81
#define	ISP_CMD_WRITE_FLASH					0x82
#define	ISP_CMD_READ_FLASH					0x83
#define	ISP_CMD_RESET						0x84
#define	ISP_CMD_UNLOCK						0x87
#define	ISP_CMD_READY						0x8F


#ifdef ST_FIREWARE_FILE
// from file
#define st_file file
#define st_filp_open filp_open
#define st_filp_close filp_close
#define ST_FW_DIR "/data"
#define ST_CFG_PATH "/sitronix_tp.cfg"
#define ST_FW_PATH "/data/sitronix_fw.bin"
//#define ST_FW_PATH "/data/FW4221_16K.bin"

#else
// from .h
#define SITRONIX_FW {\
}

#define SITRONIX_CFG {\
}

#ifdef ST_UPGRADE_BY_ID
#define SITRONIX_IDS {\
0x1,0x0,0x5,0x5,\
0x1,0x0,0x5,0x0\
}
#define SITRONIX_ID_OFF {\
0xDB,\
0xDB\
}

#define SITRONIX_FW1 {\
}

#define SITRONIX_CFG1 {\
}

#define SITRONIX_FW2 {\
}

#define SITRONIX_CFG2 {\
}

#endif	//ST_UPGRADE_BY_ID

#ifdef ST_REPLACE_CFG
#define STIRONIX_CFG_CHECKSUM_OFFSET	0xFC
#define STIRONIX_FW_SET_COUNT	2
#define SITRONIX_CFG_VERSION {\
0x47,\
0x48\
}
#define SITRONIX_F_OFFSET {\
0x9E,0x9F,0xA0,\
0x9E,0x9F,0xA0\
}
#define SITRONIX_T_OFFSET {\
0x9E,0x9F,0xA0\
}
#endif	//ST_REPLACE_CFG

#endif //ST_FIREWARE_FILE

#endif //ST_UPGRADE_FIRMWARE

//#define ST_TEST_RAW

#ifdef ST_TEST_RAW
#define MAX_RX_COUNT	12
#define MAX_TX_COUNT	21
#define MAX_KEY_COUNT	4
#define MAX_SENSOR_COUNT	MAX_RX_COUNT+MAX_TX_COUNT+MAX_KEY_COUNT
#define TX_ASSIGNMENT	1

#define MAX_RAW_LIMIT	7800
#define MIN_RAW_LIMIT	3700
#endif //ST_TEST_RAW

#if defined(ST_TEST_RAW) || defined(ST_UPGRADE_FIRMWARE)
#define st_u8 u8
#define st_char char
#define st_msleep msleep
#define st_int int
#endif

#endif // __SITRONIX_I2C_TOUCH_h
