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

#ifndef _INV_MPU_IIO_20648_H_
#define _INV_MPU_IIO_20648_H_

/* #define DEBUG */

/* #define BIAS_CONFIDENCE_HIGH 1 */

/* register and associated bit definition */
/* bank 0 register map */
#define REG_WHO_AM_I            0x00

#define REG_USER_CTRL           0x03
#define BIT_DMP_EN                      0x80
#define BIT_FIFO_EN                     0x40
#define BIT_I2C_MST_EN                  0x20
#define BIT_I2C_IF_DIS                  0x10
#define BIT_DMP_RST                     0x08

#define REG_LP_CONFIG           0x05
#define BIT_I2C_MST_CYCLE               0x40
#define BIT_ACCEL_CYCLE                 0x20
#define BIT_GYRO_CYCLE                  0x10

#define REG_PWR_MGMT_1          0x06
#define BIT_H_RESET                     0x80
#define BIT_SLEEP                       0x40
#define BIT_LP_EN                       0x20
#define BIT_CLK_PLL                     0x01

#define REG_PWR_MGMT_2          0x07
#define BIT_PWR_PRESSURE_STBY           0x40
#define BIT_PWR_ACCEL_STBY              0x38
#define BIT_PWR_GYRO_STBY               0x07
#define BIT_PWR_ALL_OFF                 0x7f

#define REG_INT_PIN_CFG         0x0F
#define BIT_BYPASS_EN                   0x2

#define REG_INT_ENABLE          0x10
#define BIT_DMP_INT_EN                  0x02
#define BIT_WOM_INT_EN                  0x08

#define REG_INT_ENABLE_1        0x11
#define BIT_DATA_RDY_3_EN               0x08
#define BIT_DATA_RDY_2_EN               0x04
#define BIT_DATA_RDY_1_EN               0x02
#define BIT_DATA_RDY_0_EN               0x01

#define REG_INT_ENABLE_2        0x12
#define BIT_FIFO_OVERFLOW_EN_0          0x1

#define REG_INT_ENABLE_3        0x13
#define REG_DMP_INT_STATUS      0x18
#define REG_INT_STATUS          0x19
#define REG_INT_STATUS_1        0x1A

#define REG_ACCEL_XOUT_H_SH     0x2D

#define REG_GYRO_XOUT_H_SH      0x33

#define REG_DMP_START_MODE      0x27
#define BIT_DMP_POKE                   0x2
#define BIT_DMP_START_MODE             0x1

#define REG_TEMPERATURE         0x39

#define REG_EXT_SLV_SENS_DATA_00 0x3B
#define REG_EXT_SLV_SENS_DATA_08 0x43
#define REG_EXT_SLV_SENS_DATA_09 0x44
#define REG_EXT_SLV_SENS_DATA_10 0x45

#define REG_FIFO_EN             0x66
#define BIT_SLV_0_FIFO_EN               1

#define REG_FIFO_EN_2           0x67
#define BIT_PRS_FIFO_EN                 0x20
#define BIT_ACCEL_FIFO_EN               0x10
#define BITS_GYRO_FIFO_EN               0x0E

#define REG_FIFO_RST            0x68

#define REG_FIFO_SIZE_0         0x6E
#define BIT_ACCEL_FIFO_SIZE_128         0x00
#define BIT_ACCEL_FIFO_SIZE_256         0x04
#define BIT_ACCEL_FIFO_SIZE_512         0x08
#define BIT_ACCEL_FIFO_SIZE_1024        0x0C
#define BIT_GYRO_FIFO_SIZE_128          0x00
#define BIT_GYRO_FIFO_SIZE_256          0x01
#define BIT_GYRO_FIFO_SIZE_512          0x02
#define BIT_GYRO_FIFO_SIZE_1024         0x03
#define BIT_FIFO_SIZE_1024              0x01
#define BIT_FIFO_SIZE_512               0x00
#define BIT_FIFO_3_SIZE_256             0x40
#define BIT_FIFO_3_SIZE_64              0x00

#define REG_FIFO_COUNT_H        0x70
#define REG_FIFO_R_W            0x72

#define REG_FIFO_CFG            0x76
#define BIT_MULTI_FIFO_CFG              0x01
#define BIT_SINGLE_FIFO_CFG             0x00
#define BIT_GYRO_FIFO_NUM               (0 << 2)
#define BIT_ACCEL_FIFO_NUM              (1 << 2)
#define BIT_PRS_FIFO_NUM                2
#define BIT_EXT_FIFO_NUM                3

#define REG_MEM_START_ADDR      0x7C
#define REG_MEM_R_W             0x7D
#define REG_MEM_BANK_SEL        0x7E

/* bank 1 register map */
#define REG_XA_OFFS_H           0x14
#define REG_YA_OFFS_H           0x17
#define REG_ZA_OFFS_H           0x1A

#define REG_TIMEBASE_CORRECTION_PLL 0x28
#define REG_TIMEBASE_CORRECTION_RCOSC 0x29
#define REG_SELF_TEST1                0x02
#define REG_SELF_TEST2                0x03
#define REG_SELF_TEST3                0x04
#define REG_SELF_TEST4                0x0E
#define REG_SELF_TEST5                0x0F
#define REG_SELF_TEST6                0x10

/* bank 2 register map */
#define REG_GYRO_SMPLRT_DIV     0x00

#define REG_GYRO_CONFIG_1       0x01
#define SHIFT_GYRO_FS_SEL               1
#define BIT_GYRO_DLPCFG_184Hz           (1 << 3)
#define BIT_GYRO_FCHOICE                1

#define REG_GYRO_CONFIG_2       0x02
#define BIT_GYRO_CTEN                   0x38

#define REG_XG_OFFS_USR_H       0x03
#define REG_YG_OFFS_USR_H       0x05
#define REG_ZG_OFFS_USR_H       0x07

#define REG_ODR_ALIGN_EN        0x09
#define BIT_ODR_ALIGN_EN                   1

#define REG_ACCEL_SMPLRT_DIV_1  0x10
#define REG_ACCEL_SMPLRT_DIV_2  0x11

#define REG_ACCEL_WOM_THR       0x13

#define REG_ACCEL_CONFIG        0x14
#define BIT_ACCEL_FCHOICE               1
#define SHIFT_ACCEL_FS                  1
#define SHIFT_ACCEL_DLPCFG              3
#define ACCEL_CONFIG_LOW_POWER_SET      7

#define REG_ACCEL_CONFIG_2       0x15
#define BIT_ACCEL_CTEN                  0x1C

#define REG_PRS_ODR_CONFIG      0x20
#define REG_PRGM_START_ADDRH    0x50

#define REG_FSYNC_CONFIG        0x52
#define BIT_TRIGGER_EIS                 0x91

#define REG_MOD_CTRL_USR        0x54
#define BIT_ODR_SYNC                    0x7

/* bank 3 register map */
#define REG_I2C_MST_ODR_CONFIG  0

#define REG_I2C_MST_CTRL         1
#define BIT_I2C_MST_P_NSR       0x10

#define REG_I2C_MST_DELAY_CTRL  0x02
#define BIT_DELAY_ES_SHADOW     0x80
#define BIT_SLV0_DLY_EN                 0x01
#define BIT_SLV1_DLY_EN                 0x02
#define BIT_SLV2_DLY_EN                 0x04
#define BIT_SLV3_DLY_EN                 0x08

#define REG_I2C_SLV0_ADDR       0x03
#define REG_I2C_SLV0_REG        0x04
#define REG_I2C_SLV0_CTRL       0x05
#define REG_I2C_SLV0_DO         0x06

#define REG_I2C_SLV1_ADDR       0x07
#define REG_I2C_SLV1_REG        0x08
#define REG_I2C_SLV1_CTRL       0x09
#define REG_I2C_SLV1_DO         0x0A

#define REG_I2C_SLV2_ADDR       0x0B
#define REG_I2C_SLV2_REG        0x0C
#define REG_I2C_SLV2_CTRL       0x0D
#define REG_I2C_SLV2_DO         0x0E

#define REG_I2C_SLV3_ADDR       0x0F
#define REG_I2C_SLV3_REG        0x10
#define REG_I2C_SLV3_CTRL       0x11
#define REG_I2C_SLV3_DO         0x12

#define REG_I2C_SLV4_CTRL       0x15

#define INV_MPU_BIT_SLV_EN      0x80
#define INV_MPU_BIT_BYTE_SW     0x40
#define INV_MPU_BIT_REG_DIS     0x20
#define INV_MPU_BIT_GRP         0x10
#define INV_MPU_BIT_I2C_READ    0x80

/* register for all banks */
#define REG_BANK_SEL            0x7F
#define BANK_SEL_0                      0x00
#define BANK_SEL_1                      0x10
#define BANK_SEL_2                      0x20
#define BANK_SEL_3                      0x30

/* data definitions */
#define BYTES_PER_SENSOR         6
#define FIFO_COUNT_BYTE          2
#define HARDWARE_FIFO_SIZE       1024
#define FIFO_SIZE                (HARDWARE_FIFO_SIZE * 7 / 8)
#define POWER_UP_TIME            100
#define REG_UP_TIME_USEC         100
#define DMP_RESET_TIME           20
#define GYRO_ENGINE_UP_TIME      50
#define MPU_MEM_BANK_SIZE        256
#define IIO_BUFFER_BYTES         8
#define HEADERED_NORMAL_BYTES    8
#define HEADERED_Q_BYTES         16
#define LEFT_OVER_BYTES          128
#define BASE_SAMPLE_RATE         1125
#define DRY_RUN_TIME             50
/* initial rate is important. For DMP mode, it is set as 1 since DMP decimate*/
#define MPU_INIT_SENSOR_RATE     1

#ifdef BIAS_CONFIDENCE_HIGH
#define DEFAULT_ACCURACY         3
#else
#define DEFAULT_ACCURACY         1
#endif

#define FREQ_225

#ifdef FREQ_225
#define MPU_DEFAULT_DMP_FREQ     225
#define PEDOMETER_FREQ           (MPU_DEFAULT_DMP_FREQ >> 2)
#define PED_ACCEL_GAIN           67108864L
#define DEFAULT_ACCEL_GAIN       (33554432L * 5 / 11)
#define DEFAULT_ACCEL_GAIN_112       (33554432L * 10 / 11)
#define ALPHA_FILL_FULL          1020054733
#define A_FILL_FULL              53687091
#else
#define MPU_DEFAULT_DMP_FREQ     102
#define PEDOMETER_FREQ           (MPU_DEFAULT_DMP_FREQ >> 1)
#define PED_ACCEL_GAIN           67108864L
#define DEFAULT_ACCEL_GAIN       33554432L
#define ALPHA_FILL_FULL          966367642
#define A_FILL_FULL              107374182
#endif
#define ALPHA_FILL_PED           858993459
#define A_FILL_PED               214748365

#define DMP_OFFSET               0x90
#define DMP_IMAGE_SIZE_20648           (14250 + DMP_OFFSET)
#define MIN_MST_ODR_CONFIG       4
#define MAX_MST_ODR_CONFIG       5
#define MAX_COMPASS_RATE    100
#define MAX_MST_NON_COMPASS_ODR_CONFIG 7
#define THREE_AXES               3
#define NINE_ELEM                (THREE_AXES * THREE_AXES)
#define MPU_TEMP_SHIFT           16
#define SOFT_IRON_MATRIX_SIZE    (4 * 9)
#define DMP_DIVIDER              (BASE_SAMPLE_RATE / MPU_DEFAULT_DMP_FREQ)
#define MAX_5_BIT_VALUE          0x1F
#define BAD_COMPASS_DATA         0x7FFF
#define BAD_CAL_COMPASS_DATA     0x7FFF0000
#define DEFAULT_BATCH_RATE       400
#define DEFAULT_BATCH_TIME    (MSEC_PER_SEC / DEFAULT_BATCH_RATE)
#define NINEQ_DEFAULT_COMPASS_RATE 25
#define DMP_ACCEL_SCALE_2G       0x2000000 /* scale for Q25 */
#define DMP_ACCEL_SCALE2_2G      0x80000   /* scale back to LSB */

#define SENCONDARY_GYRO_OFF       0x01
#define SENCONDARY_GYRO_ON        0x02
#define SENCONDARY_COMPASS_OFF    0x04
#define SENCONDARY_COMPASS_ON     0x08
#define SENCONDARY_PROX_OFF       0x10
#define SENCONDARY_PROX_ON        0x20

#define NINEQ_MIN_COMPASS_RATE 35
#define GEOMAG_MIN_COMPASS_RATE    70

#define MAX_PRESSURE_RATE        30
#define MAX_ALS_RATE             5
#define DATA_AKM_99_BYTES_DMP  10
#define DATA_AKM_89_BYTES_DMP  9
#define DATA_ALS_BYTES_DMP     8
#define APDS9900_AILTL_REG      0x04
#define BMP280_DIG_T1_LSB_REG                0x88
#define INV_RAW_DATA_BYTES             6
/* poke feature related. */
#define INV_POKE_SIZE_GYRO           20
#define INV_POKE_GYRO_OFFSET_TO_TS  12
#define INV_DMP_GYRO_START_ADDR      10
#define INV_POKE_SIZE_ACCEL          20
#define INV_POKE_ACCEL_OFFSET_TO_TS  12
#define INV_DMP_ACCEL_START_ADDR     4
#define INV_POKE_SIZE_MAG           20
#define INV_POKE_MAG_OFFSET_TO_TS  12
#define INV_DMP_MAG_START_ADDR     18

#define TEMPERATURE_SCALE  3340827L
#define TEMPERATURE_OFFSET 1376256L
#define SECONDARY_INIT_WAIT 100
#define SW_REV_NO_RUN_FLAG              8
#define AK99XX_SHIFT                    23
#define AK89XX_SHIFT                    22

/* this is derived from 1000 divided by 55, which is the pedometer
   running frequency */
#define MS_PER_PED_TICKS         18

/* data limit definitions */
#define MIN_FIFO_RATE            4
#define MAX_FIFO_RATE            MPU_DEFAULT_DMP_FREQ
#define MAX_DMP_OUTPUT_RATE      MPU_DEFAULT_DMP_FREQ

#define MAX_MPU_MEM              8192
#define MAX_PRS_RATE             281
#define MIN_COMPASS_RATE         35

/* enum for sensor
   The sequence is important.
   It represents the order of apperance from DMP */
enum INV_SENSORS {
	SENSOR_ACCEL = 0,
	SENSOR_TEMP,
	SENSOR_GYRO,
	SENSOR_COMPASS,
	SENSOR_ALS,
	SENSOR_SIXQ,
	SENSOR_NINEQ,
	SENSOR_PEDQ,
	SENSOR_GEOMAG,
	SENSOR_PRESSURE,
	SENSOR_COMPASS_CAL,
	SENSOR_NUM_MAX,
	SENSOR_INVALID,
};

enum inv_devices {
	ICM20648,
	INV_NUM_PARTS
};
#endif /* #ifndef _INV_MPU_IIO_20648_H_ */
