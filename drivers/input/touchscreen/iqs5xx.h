/*
 * iqs5xx.h - Azoteq IQS5xx B000 registers definition
 *
 * Copyright (C) 2015 Boundary Devices Inc.
 *
 * Based on Azoteq Arduino sample code.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#ifndef IQS5XX_H
#define IQS5XX_H

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>

// i2c slave device address
#define IQS5xx_ADDR		0x74

// Definitions for the driver
#define DEVICE_NAME             "iqs5xx"

/******************** DEVICE INFO REGISTERS ***************************/
#define PRODUCT_NUM		0x0000//(READ)	//2 BYTES;
#define PROJECT_NUM		0x0002//(READ)	//2 BYTES;
#define MAJOR_VERSION		0x0004//(READ)
#define MINOR_VERSION		0x0005//(READ)
#define BL_STATUS		0x0006//(READ)
/******************** ************************* ***************************/
#define MAX_TOUCH		0x000B//(READ)
#define PCYCLE_TIME		0x000C//(READ)
/******************** GESTURES AND EVENT STATUS REGISTERS *****************/
#define GESTURE_EVENTS_0	0x000D//(READ)
#define GESTURE_EVENTS_1	0x000E//(READ)
#define SYSTEM_INFO_0		0x000F//(READ)
#define SYSTEM_INFO_1		0x0010//(READ)
/******************** XY DATA REGISTERS ***************************/
#define NUM_OF_FINGERS		0x0011//(READ)
#define RELATIVE_X		0x0012//(READ)	//2 BYTES;
#define RELATIVE_Y		0x0014//(READ)	//2 BYTES;
/******************** INDIVIDUAL FINGER DATA ***************************/
#define XY_DATA			0x0016//(READ)	//DEFAULT POINTER ADDRESS
#define ABS_X_HIGH		0x0016//(READ)	//ADD X * 0x0007 FOR FINGER X;
#define ABS_X_LOW		0x0017//(READ)	//ADD X * 0x0007 FOR FINGER X;
#define ABS_Y_HIGH		0x0018//(READ)	//ADD X * 0x0007 FOR FINGER X;
#define ABS_Y_LOW		0x0019//(READ)	//ADD X * 0x0007 FOR FINGER X;
#define T_STRENTGH_HIGH		0x001A//(READ)	//ADD X * 0x0007 FOR FINGER X;
#define T_STRENTGH_LOW		0x001B//(READ)	//ADD X * 0x0007 FOR FINGER X;
#define TOUCH_AREA		0x001C//(READ)	//ADD X * 0x0007 FOR FINGER X;
/******************** CHANNEL STATUS REGISTERS ***************************/
#define PROX_STATUS		0x0039//(READ)	//32 BYTES;
#define TOUCH_STATUS		0x0059//(READ)	//30 BYTES;
#define SNAP_STATUS		0x0077//(READ)	//30 BYTES;
/******************** DATA STREAMING REGISTERS ***************************/
#define COUNTS			0x0095//(READ)		//300 BYTES;
#define DELTA			0x01C1//(READ)		//300 BYTES;
#define ALP_COUNTS		0x02ED//(READ)		//2 BYTES;
#define ALP_COUNTS_IND		0x02EF//(READ)		//20 BYTES;
#define REFERENCES		0x0303//(READ/WRITE)	//300 BYTES;
#define ALP_LTA			0x042F//(READ/WRITE)	//2 BYTES;
/******************** SYSTEM CONTROL REGISTERS ***************************/
#define SYSTEM_CONTROL_0	0x0431//(READ/WRITE)
#define SYSTEM_CONTROL_1	0x0432//(READ/WRITE)
/******************** ATI SETTINGS REGISTERS ***************************/
#define ALT_ATI_COMP		0x0435//(READ/WRITE)	//10 BYTES;
#define	ATI_COMP		0x043F//(READ/WRITE)	//150 BYTES;
#define ATI_C_IND		0x04D5//(READ/WRITE/E2)	//150 BYTES;
#define GLOBAL_ATI_C		0x056B//(READ/WRITE/E2)
#define ALP_ATI_C		0x056C//(READ/WRITE/E2)
#define ATI_TARGET		0x056D//(READ/WRITE/E2)	//2 BYTES;
#define ALP_ATI_TARGET		0x056F//(READ/WRITE/E2)	//2 BYTES;
#define REF_DRIFT_LIM		0x0571//(READ/WRITE/E2)
#define ALP_LTA_DRIFT_LIM	0x0572//(READ/WRITE/E2)
#define RE_ATI_LOWER_LIM	0x0573//(READ/WRITE/E2)
#define RE_ATI_UPPER_LIM	0x0574//(READ/WRITE/E2)
#define MAX_COUNT_LIM		0x0575//(READ/WRITE/E2)	//2 BYTES;
#define RE_ATI_TIMER		0x0577//(READ/WRITE/E2)
/******************** TIMING SETTINGS REGISTERS ***************************/
#define RR_ACTIVE_MODE		0x057A//(READ/WRITE/E2)	//2 BYTES;
#define	RR_IDLE_TMODE		0x057C//(READ/WRITE/E2)	//2 BYTES;
#define	RR_IDLE_MODE		0x057E//(READ/WRITE/E2)	//2 BYTES;
#define	RR_LP1_MODE		0x0580//(READ/WRITE/E2)	//2 BYTES;
#define	RR_LP2_MODE		0x0582//(READ/WRITE/E2)	//2 BYTES;
#define	TO_ACTIVE_MODE		0x0584//(READ/WRITE/E2)
#define TO_IDLE_TMODE		0x0585//(READ/WRITE/E2)
#define	TO_IDLE_MODE		0x0586//(READ/WRITE/E2)
#define	TO_LP1_MODE		0x0587//(READ/WRITE/E2)
#define	REF_UPDATE_TIM		0x0588//(READ/WRITE/E2)
#define	SNAP_TIMEOUT		0x0589//(READ/WRITE/E2)
#define	I2C_TIMEOUT		0x058A//(READ/WRITE/E2)
/******************** SYSTEM CONFIG REGISTERS ***************************/
#define SYSTEM_CONFIG_0		0x058E//(READ/WRITE/E2)
#define SYSTEM_CONFIG_1		0x058F//(READ/WRITE/E2)
/******************** THRESHOLD SETTINGS REGISTERS ***************************/
#define SNAP_THR		0x0592//(READ/WRITE/E2)	//2 BYTES;
#define	PROX_THR_TP		0x0594//(READ/WRITE/E2)
#define	PROX_THR_ALP		0x0595//(READ/WRITE/E2)
#define	GLOBAL_TMULT_SET	0x0596//(READ/WRITE/E2)
#define	GLOBAL_TMULT_CLEAR	0x0597//(READ/WRITE/E2)
#define	IND_TOUCH_MULT		0x0598//(READ/WRITE/E2)	//150 BYTES;
/******************** FILTER SETTINGS REGISTERS ***************************/
#define	FILTER_SETTINGS_0	0x0632//(READ/WRITE/E2)
#define	XY_STATIC_BETA		0x0633//(READ/WRITE/E2)
#define	ALP_COUNT_BETA		0x0634//(READ/WRITE/E2)
#define	ALP1_LTA_BETA		0x0635//(READ/WRITE/E2)
#define	ALP2_LTA_BETA		0x0636//(READ/WRITE/E2)
#define	XY_DY_FIL_BOT_BETA	0x0637//(READ/WRITE/E2)
#define	XY_DY_FIL_LWR_SPEED	0x0638//(READ/WRITE/E2)
#define	XY_DY_FIL_UPR_SPEED	0x0639//(READ/WRITE/E2)	//2 BYTES;
/******************** CHANNEL SET UP (RX-TX MAPPING) REGISTERS ***************/
#define	TOTAL_RX		0x063D//(READ/WRITE/E2)
#define	TOTAL_TX		0x063E//(READ/WRITE/E2)
#define	RX_MAPPING		0x063F//(READ/WRITE/E2)	//10 BYTES;
#define	TX_MAPPING		0x0649//(READ/WRITE/E2)	//15 BYTES;
#define	ALP_CHAN_SETUP		0x0658//(READ/WRITE/E2)
#define	ALP_RX_SELECT		0x0659//(READ/WRITE/E2)	//2 BYTES;
#define	ALP_TX_SELECT		0x065B//(READ/WRITE/E2)	//2 BYTES;
/******************** HARDWARE SETTINGS REGISTERS ***************************/
#define	HARDWARE_SETTINGS_A	0x065F//(READ/WRITE/E2)
#define	HARDWARE_SETTINGS_B1	0x0660//(READ/WRITE/E2)
#define	HARDWARE_SETTINGS_B2	0x0661//(READ/WRITE/E2)
#define	HARDWARE_SETTINGS_C1	0x0662//(READ/WRITE/E2)
#define	HARDWARE_SETTINGS_C2	0x0663//(READ/WRITE/E2)
#define	HARDWARE_SETTINGS_D1	0x0664//(READ/WRITE/E2)
#define	HARDWARE_SETTINGS_D2	0x0665//(READ/WRITE/E2)
/******************** XY CONFIG REGISTERS ***************************/
#define	XY_CONFIG		0x0669//(READ/WRITE/E2)
#define	MAX_NO_TOUCH		0x066A//(READ/WRITE/E2)
#define	FINGER SPLIT_FACTOR	0x066B//(READ/WRITE/E2)
#define	PALM_REJECT_THR		0x066C//(READ/WRITE/E2)
#define	PALM_REJECT_TO		0x066D//(READ/WRITE/E2)
#define	X_RESOLUTION		0x066E//(READ/WRITE/E2)	//2 BYTES;
#define	Y_RESOLUTION		0x0670//(READ/WRITE/E2)	//2 BYTES;
#define	STAT_TOUCH_MOV_THR	0x0672//(READ/WRITE/E2)
/*********************************************************************/
#define	DEFAULT_READ_ADDR	0x0675//(READ/WRITE/E2)
/******************** DEBOUNCE SETTING REGISTERS ***************************/
#define	PROX_DEBOUNCE		0x0679//(READ/WRITE/E2)
#define	TOUCH_DEBOUNCE		0x067A//(READ/WRITE/E2)
/******************** CHANNEL CONFIG REGISTERS ***************************/
#define	ACTIVE_CHANNELS		0x067B//(READ/WRITE/E2)	//30 BYTES;
#define	SNAP_EN_CHANNELS	0x0699//(READ/WRITE/E2)	//30 BYTES;
/******************** GESTURE SETTING REGISTERS ***************************/
#define	SINGLE_FINGER_GEST	0x06B7//(READ/WRITE/E2)
#define	MULTI_FINGER_GEST	0x06B8//(READ/WRITE/E2)
#define	TAP_TIME		0x06B9//(READ/WRITE/E2)	//2 BYTES;
#define	TAP_DISTANCE		0x06BB//(READ/WRITE/E2)	//2 BYTES;
#define HOLD_TIME		0x06BD//(READ/WRITE/E2)	//2 BYTES;
#define	SWIPE_INIT_TIME		0x06BF//(READ/WRITE/E2)	//2 BYTES;
#define	SWIPE_INIT_DIST		0x06C1//(READ/WRITE/E2)	//2 BYTES;
#define	SWIPE_CONS_TIME		0x06C2//(READ/WRITE/E2)	//2 BYTES;
#define	SWIPE_CONS_DIST		0x06C5//(READ/WRITE/E2)	//2 BYTES;
#define	SWIPE_ANGLE		0x06C7//(READ/WRITE/E2)
#define	SCROLL_INIT_DIST	0x06C8//(READ/WRITE/E2)	//2 BYTES;
#define	SCROLL_ANGLE		0x06CA//(READ/WRITE/E2)
#define	ZOOM_INIT_DIST		0x06CB//(READ/WRITE/E2)	//2 BYTES;
#define	ZOOM_CONS_DIST		0x06CD//(READ/WRITE/E2)	//2 BYTES;
#define	SCROLL_LIFT_TIME	0x00CD//(READ/WRITE/E2)	//2 BYTES;

#define	END_COMMUNICATION	0xEEEE	// Write

/* Structure to keep track of all the touches	*/
struct iqs5xx_touch_info {
	u16 x_pos;		/* the X-coordinate of the reported touch */
	u16 y_pos;		/* the Y-coordinate of the reported touch */
	u16 prev_x;		/* the previous X-coordinate for filters */
	u16 prev_y;		/* the previous Y-coordinate for filters */
	u16 strength;		/* the 'hardness' of the touch */
};

#endif
