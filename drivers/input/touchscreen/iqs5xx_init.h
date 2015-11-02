/*
 * iqs5xx_init.h - Azoteq IQS5xx B000 settings for a specific hardware setup.
 * This can be exported from the IQS5XX GUI, or modified here.
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

#ifndef IQS5XX_INIT_H
#define IQS5XX_INIT_H

/******************** SYSTEM CONTROL DEFINES ***************************/
#define	SYSTEM_CONTROL_0_VAL      0x00
        #define ACK_RESET         0x80
        #define AUTO_ATI          0x20
        #define ALP_RESEED        0x10
        #define RESEED            0x08
        #define ACTIVE_MODE       0x00
        #define IDLE_TMODE        0x01
        #define IDLE_MODE         0x02
        #define LP1_MODE          0x03
        #define LP2_MODE          0x04

#define	SYSTEM_CONTROL_1_VAL      0x00
        #define RESET             0x02
        #define SUSPEND           0x01
/******************** ATI SETTINGS DEFINES ***************************/
#define ALP_ATI_C_VAL             23
#define GLOBAL_ATI_C_VAL          3
#define ATI_TARGET_VAL            500	//2 BYTES;
#define ALP_ATI_TARGET_VAL        700	//2 BYTES;
#define REF_DRIFT_LIM_VAL         75
#define ALP_LTA_DRIFT_LIM_VAL     50
#define RE_ATI_LOWER_LIM_VAL      0
#define RE_ATI_UPPER_LIM_VAL      255
#define MAX_COUNT_LIM_VAL         2000	//2 BYTES;
#define RE_ATI_TIMER_VAL          5
/******************** TIMING SETTINGS DEFINES ***************************/
#define RR_ACTIVE_MODE_VAL        13	//2 BYTES;
#define RR_IDLE_TMODE_VAL         50	//2 BYTES;
#define RR_IDLE_MODE_VAL          75	//2 BYTES;
#define RR_LP1_MODE_VAL           50	//2 BYTES;
#define RR_LP2_MODE_VAL           125	//2 BYTES;
#define TO_ACTIVE_MODE_VAL        5
#define TO_IDLE_TMODE_VAL         10
#define TO_IDLE_MODE_VAL          20
#define TO_LP1_MODE_VAL           30
#define REF_UPDATE_TIM_VAL        8
#define SNAP_TIMEOUT_VAL          10
#define I2C_TIMEOUT_VAL           100
/******************** SYSTEM CONFIG DEFINES ***************************/
#define	SYSTEM_CONFIG_0_VAL       0x00
        #define MANUAL_CTRL       0x80
        #define SETUP_COMP        0x40
        #define WDT_ENABLE        0x20
        #define ALP_RE_ATI        0x08
        #define RE_ATI            0x04
        #define IO_WAKEUP_SEL     0x02
        #define IO_WAKEUP         0x01

#define	SYSTEM_CONFIG_1_VAL       0x00
        #define PROX_EVENT        0x80
        #define TOUCH_EVENT       0x40
        #define SNAP_EVENT        0x20
        #define ALP_PROX_EVENT    0x10
        #define RE_ATI_EVENT      0x08
        #define TP_EVENT          0x04
        #define GESTURE_EVENT     0x02
        #define EVENT_MODE        0x01
/******************** THRESHOLD SETTINGS DEFINES ***************************/
#define SNAP_THR_VAL              100	//2 BYTES;
#define PROX_THR_TP_VAL           23
#define PROX_THR_ALP_VAL          8
#define GLOBAL_TMULT_SET_VAL      18
#define GLOBAL_TMULT_CLEAR_VAL    12
/******************** CHANNEL SET UP (RX-TX MAPPING) DEFINES ***************************/
#define TOTAL_RX_VAL              10
#define TOTAL_TX_VAL              15
#define ALP_CHAN_SETUP_VAL        0x00
        #define CHARGE_TYPE_PROJ  0x00
        #define CHARGE_TYPE_SELF  0x80
        #define RX_GROUP_A        0x00
        #define RX_GROUP_B        0x40
        #define PROX_REV_EN       0x20
        #define LP1_LP2_TP        0x00
        #define LP1_LP2_ALP       0x10
#define ALP_RX_SELECT_VAL         0x02AA	//2 BYTES;
#define ALP_TX_SELECT_VAL         0x5555	//2 BYTES;
#define RX_MAPPING_VAL            0, 1, 2, 3, 4, 5, 6, 7, 8, 9
#define TX_MAPPING_VAL            0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14	//6,4,3,2,0,1,5,7,8,9,10,11,12 (TS65)
#define ACTIVE_CHANNELS_VAL       0x3F, 0xFF, 0x3F, 0xFF, 0x3F, 0xFF, 0x3F, 0xFF, 0x3F, 0xFF, 0x3F, 0xFF, 0x3F, 0xFF, 0x3F, 0xFF, 0x3F, 0xFF, 0x3F, 0xFF, 0x3F, 0xFF, 0x3F, 0xFF, 0x3F, 0xFF, 0x3F, 0xFF, 0x3F, 0xFF
#define SNAP_CHANNELS_VAL         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
/******************** FILTER SETTINGS DEFINES ***************************/
#define	FILTER_SETTINGS_0_VAL     0x00
        #define ALP_CNT_FIL_EN    0x08
        #define IIR_SELECT_EN     0x04
        #define MAV_FILTER_EN     0x02
        #define IIR_FILTER_EN     0x01
#define XY_STATIC_BETA_VAL        128
#define ALP_COUNT_BETA_VAL        50
#define ALP1_LTA_BETA_VAL         8
#define ALP2_LTA_BETA_VAL         6
#define XY_DY_FIL_BOT_BETA_VAL    6
#define XY_DY_FIL_LWR_SPEED_VAL   7
#define XY_DY_FIL_UPR_SPEED_VAL   125	//2 BYTES;
/******************** HARDWARE SETTINGS DEFINES ***************************/
#define	HARDWARE_SETTINGS_A_VAL   0x00
        #define ND_ENABLE         0x20
        #define RX_FLOAT          0x04

#define	HARDWARE_SETTINGS_B_VAL   0x00
        #define CK_FREQ_125K      0x00
        #define CK_FREQ_250K      0x10
        #define CK_FREQ_500K      0x20
        #define CK_FREQ_1M        0x30
        #define CK_FREQ_2M        0x40
        #define CK_FREQ_4M        0x50
        #define CK_FREQ_8M        0x60
        #define CK_FREQ_16M       0x70

        #define ANA_DEAD_TIME     0x02
        #define INCR_PHASE        0x01

#define	HARDWARE_SETTINGS_C_VAL   0x00
        #define STAB_TIME_1_7     0x00
        #define STAB_TIME_500     0x40
        #define STAB_TIME_120     0x80
        #define STAB_TIME_UNUSED  0xC0
        #define OPAMP_BIAS_2_5u   0x00
        #define OPAMP_BIAS_5u     0x10
        #define OPAMP_BIAS_7_5u   0x20
        #define OPAMP_BIAS_10u    0x30

#define	HARDWARE_SETTINGS_D_VAL   0x00
        #define UPLEN             0x40
        #define	PASSLEN           0x07
/******************** XY CONFIG DEFINES ***************************/
#define	XY_CONFIG_VAL             0x00
        #define PALM_REJECT       0x08
        #define SWITCH_XY_AXIS    0x04
        #define FLIP_Y            0x02
        #define FLIP_X            0x01

#define MAX_NO_TOUCH_VAL          5
#define FINGER_SPLIT_FACTOR_VAL   6
#define PALM_REJECT_THR_VAL       25
#define PALM_REJECT_TO_VAL        15
#define X_RESOLUTION_VAL          1024	//2 BYTES;
#define Y_RESOLUTION_VAL          1024	//2 BYTES;
#define STAT_TOUCH_MOV_THR_VAL    20
/******************** DEBOUNCE SETTING DEFINES ***************************/
#define PROX_DEBOUNCE_VAL         0x44
#define TOUCH_DEBOUNCE_VAL        0x00
/******************** GESTURE SETTING DEFINES ***************************/
#define SINGLE_FINGER_GEST_VAL    0x00
        #define SWIPE_Y_MINUS_EN  0x20
        #define SWIPE_Y_PLUS_EN   0x10
        #define SWIPE_X_PLUS_EN   0x08
        #define SWIPE_X_MINUS_EN  0x04
        #define TAP_AND_HOLD_EN   0x02
        #define SINGLE_TAP_EN     0x01

#define MULTI_FINGER_GEST_VAL     0x00
        #define ZOOM_EN           0x04
        #define SCROLL_EN         0x02
        #define TWO_FINGER_TAP_EN 0x01

#define TAP_TIME_VAL              200	//2 BYTES;
#define TAP_DISTANCE_VAL          25	//2 BYTES;
#define HOLD_TIME_VAL             300	//2 BYTES;
#define SWIPE_INIT_TIME_VAL       200	//2 BYTES;
#define SWIPE_INIT_DIST_VAL       50	//2 BYTES;
#define SWIPE_CONS_TIME_VAL       200	//2 BYTES;
#define SWIPE_CONS_DIST_VAL       25	//2 BYTES;
#define SWIPE_ANGLE_VAL           45
#define SCROLL_INIT_DIST_VAL      50	//2 BYTES;
#define SCROLL_ANGLE_VAL          45
#define ZOOM_INIT_DIST_VAL        50	//2 BYTES;
#define ZOOM_CONS_DIST_VAL        25	//2 BYTES;
//#define SCROLL_LIFT_TIME_VAL     100	//2 BYTES;

#define Default_Setup             1
#define Automated_Setup           2

#endif
