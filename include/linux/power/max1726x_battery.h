/*
 * Maxim Max1726x Fuel Gauge driver header file
 *
 * Author:  Mahir Ozturk <mahir.ozturk@maximintegrated.com>
 *          Jason Cole <jason.cole@maximintegrated.com>
 *          Kerem Sahin <kerem.sahin@maximintegrated.com>
 * Copyright (C) 2018 Maxim Integrated
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

//Version 1.0.2 - Added RCOMPSeg Reg for model loading
#ifndef __MAX1726X_BATTERY_H_
#define __MAX1726X_BATTERY_H_

#define MAX1726X_TABLE_SIZE             32
#define MAX1726X_TABLE_SIZE_IN_BYTES    (2 * MAX1726X_TABLE_SIZE)

/* Model loading options */
#define MODEL_LOADING_OPTION1           1
#define MODEL_LOADING_OPTION2           2
#define MODEL_LOADING_OPTION3           3

/* Model lock/unlock */
#define MODEL_UNLOCK1   0X0059
#define MODEL_UNLOCK2   0X00C4
#define MODEL_LOCK1     0X0000
#define MODEL_LOCK2     0X0000

#define MODEL_SCALING        1

struct max1726x_platform_data {
	u16 config;
	u16 config2;
	u16 designcap;
	u16 ichgterm;
	u16 vempty;
	u16 learncfg;
	u16 relaxcfg;
	u16 fullsocthr;
	u16 tgain;
	u16 toff;
	u16 curve;
	u16 rcomp0;
	u16 tempco;
	u16 qrtable00;
	u16 qrtable10;
	u16 qrtable20;
	u16 qrtable30;
	u16 cvhalftime;
	u16 cvmixcap;
	u16 dpacc;
	u16 modelcfg;
	u16 model_data[MAX1726X_TABLE_SIZE];
	u16 model_rcomp_seg;
	int (*get_charging_status)(void);
	int model_option;

	/*
	 * rsense in miliOhms.
	 * default 10 (if rsense = 0) as it is the recommended value by
	 * the datasheet although it can be changed by board designers.
	 */
	unsigned int rsense;
	int volt_min;   /* in mV */
	int volt_max;   /* in mV */
	int temp_min;   /* in DegreC */
	int temp_max;   /* in DegreeC */
	int soc_max;    /* in percent */
	int soc_min;    /* in percent */
	int curr_max;   /* in mA */
	int curr_min;   /* in mA */
	int vcharge;
#define MAX_MASK16_CONFIG	BIT(0)
#define MAX_MASK16_CONFIG2	BIT(1)
	unsigned mask16;
	unsigned mask32;
};

enum max1726x_register{
	MAX1726X_STATUS_REG                 = 0x00,
	MAX1726X_VALRTTH_REG                = 0x01,
	MAX1726X_TALRTTH_REG                = 0x02,
	MAX1726X_SALRTTH_REG                = 0x03,
	MAX1726X_REPCAP_REG                 = 0x05,
	MAX1726X_REPSOC_REG                 = 0x06,
	MAX1726X_TEMP_REG                   = 0x08,
	MAX1726X_VCELL_REG                  = 0x09,
	MAX1726X_CURRENT_REG                = 0x0A,
	MAX1726X_AVGCURRENT_REG             = 0x0B,
	MAX1726X_REMCAP_REG                 = 0x0F,

	MAX1726X_FULLCAPREP_REG             = 0x10,
	MAX1726X_TTE_REG                    = 0X11,
	MAX1726X_QRTABLE00_REG              = 0x12,
	MAX1726X_FULLSOCTHR_REG             = 0x13,
	MAX1726X_CYCLES_REG                 = 0x17,
	MAX1726X_DESIGNCAP_REG              = 0x18,
	MAX1726X_AVGVCELL_REG               = 0x19,
	MAX1726X_MAXMINVOLT_REG             = 0x1B,
	MAX1726X_CONFIG_REG                 = 0x1D,
	MAX1726X_ICHGTERM_REG               = 0x1E,

	MAX1726X_TTF_REG                    = 0X20,
	MAX1726X_VERSION_REG                = 0x21,
	MAX1726X_QRTABLE10_REG              = 0x22,
	MAX1726X_FULLCAPNOM_REG             = 0x23,
	MAX1726X_LEARNCFG_REG               = 0x28,
	MAX1726X_RELAXCFG_REG               = 0x2A,
	MAX1726X_TGAIN_REG                  = 0x2C,
	MAX1726X_TOFF_REG                   = 0x2D,

	MAX1726X_QRTABLE20_REG              = 0x32,
	MAX1726X_RCOMP0_REG                 = 0x38,
	MAX1726X_TEMPCO_REG                 = 0x39,
	MAX1726X_VEMPTY_REG                 = 0x3A,
	MAX1726X_FSTAT_REG                  = 0x3D,

	MAX1726X_QRTABLE30_REG              = 0x42,
	MAX1726X_DQACC_REG                  = 0x45,
	MAX1726X_DPACC_REG                  = 0x46,
	MAX1726X_VFSOC0_REG                 = 0x48,
	MAX1726X_QH0_REG                    = 0x4C,
	MAX1726X_QH_REG                     = 0x4D,

	MAX1726X_VFSOC0_QH0_LOCK_REG        = 0x60,
	MAX1726X_LOCK1_REG                  = 0x62,
	MAX1726X_LOCK2_REG                  = 0x63,

	MAX1726X_MODELDATA_START_REG        = 0x80,
	MAX1726X_MODELDATA_RCOMPSEG_REG     = 0xAF,

	MAX1726X_IALRTTH_REG                = 0xB4,
	MAX1726X_CVMIXCAP_REG               = 0xB6,
	MAX1726X_CVHALFIME_REG              = 0xB7,
	MAX1726X_CURVE_REG                  = 0xB9,
	MAX1726X_HIBCFG_REG                 = 0xBA,
	MAX1726X_CONFIG2_REG                = 0xBB,

	MAX1726X_MODELCFG_REG               = 0xDB,

	MAX1726X_OCV_REG                    = 0xFB,
	MAX1726X_VFSOC_REG                  = 0xFF,
};

#endif
