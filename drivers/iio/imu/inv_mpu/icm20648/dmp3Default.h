/*
 * Copyright (C) 2017-2019 InvenSense, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define CFG_FIFO_SIZE                   (4184)

/* data output control */
#define DATA_OUT_CTL1			(4 * 16)
#define DATA_OUT_CTL2			(4 * 16 + 2)
#define DATA_INTR_CTL			(4 * 16 + 12)
#define FIFO_WATERMARK			(31 * 16 + 14)

/* motion event control */
#define MOTION_EVENT_CTL		(4 * 16 + 14)

/* indicates to DMP which sensors are available */
/*	1: gyro samples available
	2: accel samples available
	8: secondary samples available	*/
#define DATA_RDY_STATUS			(8 * 16 + 10)

/* batch mode */
#define BM_BATCH_CNTR			(27 * 16)
#define BM_BATCH_THLD			(19 * 16 + 12)
#define BM_BATCH_MASK			(21 * 16 + 14)

/* sensor output data rate */
#define ODR_ACCEL				(11 * 16 + 14)
#define ODR_GYRO				(11 * 16 + 10)
#define ODR_CPASS				(11 * 16 +  6)
#define ODR_ALS					(11 * 16 +  2)
#define ODR_QUAT6				(10 * 16 + 12)
#define ODR_QUAT9				(10 * 16 +  8)
#define ODR_PQUAT6				(10 * 16 +  4)
#define ODR_GEOMAG				(10 * 16 +  0)
#define ODR_PRESSURE			(11 * 16 + 12)
#define ODR_GYRO_CALIBR			(11 * 16 +  8)
#define ODR_CPASS_CALIBR		(11 * 16 +  4)

/* sensor output data rate counter */
#define ODR_CNTR_ACCEL			(9 * 16 + 14)
#define ODR_CNTR_GYRO			(9 * 16 + 10)
#define ODR_CNTR_CPASS			(9 * 16 +  6)
#define ODR_CNTR_ALS			(9 * 16 +  2)
#define ODR_CNTR_QUAT6			(8 * 16 + 12)
#define ODR_CNTR_QUAT9			(8 * 16 +  8)
#define ODR_CNTR_PQUAT6			(8 * 16 +  4)
#define ODR_CNTR_GEOMAG			(8 * 16 +  0)
#define ODR_CNTR_PRESSURE		(9 * 16 + 12)
#define ODR_CNTR_GYRO_CALIBR	(9 * 16 +  8)
#define ODR_CNTR_CPASS_CALIBR	(9 * 16 +  4)

/* mounting matrix */
#define CPASS_MTX_00            (23 * 16)
#define CPASS_MTX_01            (23 * 16 + 4)
#define CPASS_MTX_02            (23 * 16 + 8)
#define CPASS_MTX_10            (23 * 16 + 12)
#define CPASS_MTX_11            (24 * 16)
#define CPASS_MTX_12            (24 * 16 + 4)
#define CPASS_MTX_20            (24 * 16 + 8)
#define CPASS_MTX_21            (24 * 16 + 12)
#define CPASS_MTX_22            (25 * 16)

#define GYRO_SF					(19 * 16)
#define ACCEL_FB_GAIN			(34 * 16)
#define ACCEL_ONLY_GAIN			(16 * 16 + 12)

/* bias calibration */
#define GYRO_BIAS_X				(139 * 16 +  4)
#define GYRO_BIAS_Y				(139 * 16 +  8)
#define GYRO_BIAS_Z				(139 * 16 + 12)
#define GYRO_ACCURACY			(138 * 16 +  2)
#define GYRO_BIAS_SET			(138 * 16 +  6)
#define GYRO_LAST_TEMPR			(134 * 16)
#define GYRO_SLOPE_X			(78 * 16 +  4)
#define GYRO_SLOPE_Y			(78 * 16 +  8)
#define GYRO_SLOPE_Z			(78 * 16 + 12)

#define ACCEL_BIAS_X            (110 * 16 +  4)
#define ACCEL_BIAS_Y            (110 * 16 +  8)
#define ACCEL_BIAS_Z            (110 * 16 + 12)
#define ACCEL_ACCURACY			(97 * 16)
#define ACCEL_CAL_RESET			(77 * 16)
#define ACCEL_VARIANCE_THRESH	(93 * 16)
#define ACCEL_CAL_RATE			(94 * 16 + 4)
#define ACCEL_PRE_SENSOR_DATA	(97 * 16 + 4)
#define ACCEL_COVARIANCE		(101 * 16 + 8)
#define ACCEL_ALPHA_VAR			(91 * 16)
#define ACCEL_A_VAR				(92 * 16)
#define ACCEL_CAL_INIT			(94 * 16 + 2)

#define CPASS_BIAS_X            (126 * 16 +  4)
#define CPASS_BIAS_Y            (126 * 16 +  8)
#define CPASS_BIAS_Z            (126 * 16 + 12)
#define CPASS_ACCURACY			(37 * 16)
#define CPASS_BIAS_SET			(34 * 16 + 14)
#define MAR_MODE				(37 * 16 + 2)
#define CPASS_COVARIANCE		(115 * 16)
#define CPASS_COVARIANCE_CUR	(118 * 16 +  8)
#define CPASS_REF_MAG_3D		(122 * 16)
#define CPASS_CAL_INIT			(114 * 16)
#define CPASS_EST_FIRST_BIAS	(113 * 16)
#define MAG_DISTURB_STATE		(113 * 16 + 2)
#define CPASS_VAR_COUNT			(112 * 16 + 6)
#define CPASS_COUNT_7			(87 * 16 + 2)
#define CPASS_MAX_INNO			(124 * 16)
#define CPASS_BIAS_OFFSET		(113 * 16 + 4)
#define CPASS_CUR_BIAS_OFFSET	(114 * 16 + 4)
#define CPASS_PRE_SENSOR_DATA	(87 * 16 + 4)

/* Compass Cal params to be adjusted according to sampling rate */
#define CPASS_TIME_BUFFER		(112 * 16 + 14)
#define CPASS_RADIUS_3D_THRESH_ANOMALY	(112 * 16 + 8)

#define CPASS_STATUS_CHK		(25 * 16 + 12)

/* 9-axis */
#define MAGN_THR_9X				(80 * 16)
#define MAGN_LPF_THR_9X			(80 * 16 +  8)
#define QFB_THR_9X				(80 * 16 + 12)

/* DMP running counter */
#define DMPRATE_CNTR			(18 * 16 + 4)

/* pedometer */
#define PEDSTD_BP_B				(49 * 16 + 12)
#define PEDSTD_BP_A4			(52 * 16)
#define PEDSTD_BP_A3			(52 * 16 +  4)
#define PEDSTD_BP_A2			(52 * 16 +  8)
#define PEDSTD_BP_A1			(52 * 16 + 12)
#define PEDSTD_SB				(50 * 16 +  8)
#define PEDSTD_SB_TIME			(50 * 16 + 12)
#define PEDSTD_PEAKTHRSH		(57 * 16 +  8)
#define PEDSTD_TIML				(50 * 16 + 10)
#define PEDSTD_TIMH				(50 * 16 + 14)
#define PEDSTD_PEAK				(57 * 16 +  4)
#define PEDSTD_STEPCTR			(54 * 16)
#define PEDSTD_STEPCTR2			(58 * 16 +  8)
#define PEDSTD_TIMECTR			(60 * 16 +  4)
#define PEDSTD_DECI				(58 * 16)
#define PEDSTD_SB2				(60 * 16 + 14)
#define STPDET_TIMESTAMP		(18 * 16 +  8)
#define PEDSTEP_IND				(19 * 16 +  4)

/* SMD */
#define SMD_VAR_TH              (141 * 16 + 12)
#define SMD_VAR_TH_DRIVE        (143 * 16 + 12)
#define SMD_DRIVE_TIMER_TH      (143 * 16 +  8)
#define SMD_TILT_ANGLE_TH       (179 * 16 + 12)
#define BAC_SMD_ST_TH           (179 * 16 +  8)
#define BAC_ST_ALPHA4           (180 * 16 + 12)
#define BAC_ST_ALPHA4A          (176 * 16 + 12)

/* Wake on Motion */
#define WOM_ENABLE              (64 * 16 + 14)
#define WOM_STATUS              (64 * 16 + 6)
#define WOM_THRESHOLD           (64 * 16)
#define WOM_CNTR_TH             (64 * 16 + 12)

/* Activity Recognition */
#define BAC_RATE                (48  * 16 + 10)
#define BAC_STATE               (179 * 16 +  0)
#define BAC_STATE_PREV          (179 * 16 +  4)
#define BAC_ACT_ON              (182 * 16 +  0)
#define BAC_ACT_OFF             (183 * 16 +  0)
#define BAC_STILL_S_F           (177 * 16 +  0)
#define BAC_RUN_S_F             (177 * 16 +  4)
#define BAC_DRIVE_S_F           (178 * 16 +  0)
#define BAC_WALK_S_F            (178 * 16 +  4)
#define BAC_SMD_S_F             (178 * 16 +  8)
#define BAC_BIKE_S_F            (178 * 16 + 12)
#define BAC_E1_SHORT            (146 * 16 +  0)
#define BAC_E2_SHORT            (146 * 16 +  4)
#define BAC_E3_SHORT            (146 * 16 +  8)
#define BAC_VAR_RUN             (148 * 16 + 12)
#define BAC_TILT_INIT           (181 * 16 +  0)
#define BAC_MAG_ON              (225 * 16 +  0)
#define BAC_PS_ON               (74  * 16 +  0)
#define BAC_BIKE_PREFERENCE     (173 * 16 +  8)
#define BAC_MAG_I2C_ADDR        (229 * 16 +  8)
#define BAC_PS_I2C_ADDR         (75  * 16 +  4)
#define BAC_DRIVE_CONFIDENCE    (144 * 16 +  0)
#define BAC_WALK_CONFIDENCE     (144 * 16 +  4)
#define BAC_SMD_CONFIDENCE      (144 * 16 +  8)
#define BAC_BIKE_CONFIDENCE     (144 * 16 + 12)
#define BAC_STILL_CONFIDENCE    (145 * 16 +  0)
#define BAC_RUN_CONFIDENCE      (145 * 16 +  4)

/* Flip/Pick-up */
#define FP_VAR_ALPHA            (245 * 16 +  8)
#define FP_STILL_TH             (246 * 16 +  4)
#define FP_MID_STILL_TH         (244 * 16 +  8)
#define FP_NOT_STILL_TH         (246 * 16 +  8)
#define FP_VIB_REJ_TH           (241 * 16 +  8)
#define FP_MAX_PICKUP_T_TH      (244 * 16 + 12)
#define FP_PICKUP_TIMEOUT_TH    (248 * 16 +  8)
#define FP_STILL_CONST_TH       (246 * 16 + 12)
#define FP_MOTION_CONST_TH      (240 * 16 +  8)
#define FP_VIB_COUNT_TH         (242 * 16 +  8)
#define FP_STEADY_TILT_TH       (247 * 16 +  8)
#define FP_STEADY_TILT_UP_TH    (242 * 16 + 12)
#define FP_Z_FLAT_TH_MINUS      (243 * 16 +  8)
#define FP_Z_FLAT_TH_PLUS       (243 * 16 + 12)
#define FP_DEV_IN_POCKET_TH     (76  * 16 + 12)
#define FP_PICKUP_CNTR          (247 * 16 +  4)
#define FP_RATE                 (240 * 16 + 12)

/* Accel FSR */
#define ACC_SCALE               (30 * 16 + 0)
#define ACC_SCALE2              (79 * 16 + 4)

/* S-Health keys */
#define S_HEALTH_WALK_RUN_1		(213 * 16 +  12)
#define S_HEALTH_WALK_RUN_2		(213 * 16 +   8)
#define S_HEALTH_WALK_RUN_3		(213 * 16 +   4)
#define S_HEALTH_WALK_RUN_4		(213 * 16 +   0)
#define S_HEALTH_WALK_RUN_5		(212 * 16 +  12)
#define S_HEALTH_WALK_RUN_6		(212 * 16 +   8)
#define S_HEALTH_WALK_RUN_7		(212 * 16 +   4)
#define S_HEALTH_WALK_RUN_8		(212 * 16 +   0)
#define S_HEALTH_WALK_RUN_9		(211 * 16 +  12)
#define S_HEALTH_WALK_RUN_10	(211 * 16 +   8)
#define S_HEALTH_WALK_RUN_11	(211 * 16 +   4)
#define S_HEALTH_WALK_RUN_12	(211 * 16 +   0)
#define S_HEALTH_WALK_RUN_13	(210 * 16 +  12)
#define S_HEALTH_WALK_RUN_14	(210 * 16 +   8)
#define S_HEALTH_WALK_RUN_15	(210 * 16 +   4)
#define S_HEALTH_WALK_RUN_16	(210 * 16 +   0)
#define S_HEALTH_WALK_RUN_17	(209 * 16 +  12)
#define S_HEALTH_WALK_RUN_18	(209 * 16 +   8)
#define S_HEALTH_WALK_RUN_19	(209 * 16 +   4)
#define S_HEALTH_WALK_RUN_20	(209 * 16 +   0)
#define S_HEALTH_CADENCE1		(213 * 16 +  14)
#define S_HEALTH_CADENCE2		(213 * 16 +  10)
#define S_HEALTH_CADENCE3		(213 * 16 +   6)
#define S_HEALTH_CADENCE4		(213 * 16 +   2)
#define S_HEALTH_CADENCE5		(212 * 16 +  14)
#define S_HEALTH_CADENCE6		(212 * 16 +  10)
#define S_HEALTH_CADENCE7		(212 * 16 +   6)
#define S_HEALTH_CADENCE8		(212 * 16 +   2)
#define S_HEALTH_CADENCE9		(211 * 16 +  14)
#define S_HEALTH_CADENCE10		(211 * 16 +  10)
#define S_HEALTH_CADENCE11		(211 * 16 +   6)
#define S_HEALTH_CADENCE12		(211 * 16 +   2)
#define S_HEALTH_CADENCE13		(210 * 16 +  14)
#define S_HEALTH_CADENCE14		(210 * 16 +  10)
#define S_HEALTH_CADENCE15		(210 * 16 +   6)
#define S_HEALTH_CADENCE16		(210 * 16 +   2)
#define S_HEALTH_CADENCE17		(209 * 16 +  14)
#define S_HEALTH_CADENCE18		(209 * 16 +  10)
#define S_HEALTH_CADENCE19		(209 * 16 +   6)
#define S_HEALTH_CADENCE20		(209 * 16 +   2)
#define S_HEALTH_INT_PERIOD		(214 * 16 +   6)
#define S_HEALTH_INT_PERIOD2	(214 * 16 +  10)
#define S_HEALTH_BACKUP1		(214 * 16 +   0)
#define S_HEALTH_BACKUP2		(214 * 16 +   2)
#define S_HEALTH_RATE           (208 * 16 +  14)

/* EIS authentication */
#define EIS_AUTH_INPUT			(160 * 16 +   4)
#define EIS_AUTH_OUTPUT			(160 * 16 +   0)

#define ACCEL_SET		0x8000
#define GYRO_SET		0x4000
#define CPASS_SET		0x2000
#define ALS_SET			0x1000
#define QUAT6_SET		0x0800
#define QUAT9_SET		0x0400
#define PQUAT6_SET		0x0200
#define GEOMAG_SET		0x0100
#define PRESSURE_SET	0x0080
#define CPASS_CALIBR_SET 0x0020
#define PED_STEPDET_SET	0x0010
#define HEADER2_SET		0x0008
#define PED_STEPIND_SET 0x0007

/* data output control reg 2 */
#define ACCEL_ACCURACY_SET  0x4000
#define GYRO_ACCURACY_SET   0x2000
#define CPASS_ACCURACY_SET  0x1000
#define FSYNC_SET           0x0800
#define FLIP_PICKUP_SET     0x0400
#define BATCH_MODE_EN       0x0100
#define ACT_RECOG_SET       0x0080
#define SECOND_SEN_OFF_SET  0x0040

/* motion event control reg
 high byte of motion event control */
#define PEDOMETER_EN        0x4000
#define PEDOMETER_INT_EN    0x2000
#define TILT_INT_EN         0x1000
#define SMD_EN              0x0800
#define SECOND_SENSOR_AUTO  0x0400
#define ACCEL_CAL_EN        0x0200
#define GYRO_CAL_EN         0x0100
/* low byte of motion event control */
#define COMPASS_CAL_EN      0x0080
#define NINE_AXIS_EN        0x0040
#define S_HEALTH_EN         0x0020
#define FLIP_PICKUP_EN      0x0010
#define GEOMAG_RV_EN        0x0008
#define BRING_LOOK_SEE_EN   0x0004
#define BAC_ACCEL_ONLY_EN   0x0002

/* data packet size reg 1 */
#define HEADER_SZ		2
#define ACCEL_DATA_SZ	6
#define GYRO_DATA_SZ	12
#define CPASS_DATA_SZ	6
#define ALS_DATA_SZ		8
#define QUAT6_DATA_SZ	12
#define QUAT9_DATA_SZ	14
#define PQUAT6_DATA_SZ	6
#define GEOMAG_DATA_SZ	14
#define PRESSURE_DATA_SZ		6
#define CPASS_CALIBR_DATA_SZ	12
#define PED_STEPDET_TIMESTAMP_SZ	4
#define FOOTER_SZ		2

/* data packet size reg 2 */
#define HEADER2_SZ			2
#define ACCEL_ACCURACY_SZ	2
#define GYRO_ACCURACY_SZ	2
#define CPASS_ACCURACY_SZ	2
#define FSYNC_SZ			2
#define FLIP_PICKUP_SZ      2
#define ACT_RECOG_SZ        6
#define SECOND_AUTO_OFF_SZ    2

#define DMP_START_ADDRESS   ((unsigned short)0x1000)
#define DMP_MEM_BANK_SIZE   256
#define DMP_LOAD_START      0x90

#define DMP_CODE_SIZE 13463
