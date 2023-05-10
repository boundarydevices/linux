/*
 * max77823_fuelgauge.h
 * Samsung MAX77823 Fuel Gauge Header
 *
 * Copyright (C) 2014 Samsung Electronics, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MAX77823_FUELGAUGE_H
#define __MAX77823_FUELGAUGE_H __FILE__

#if defined(ANDROID_ALARM_ACTIVATED)
#include <linux/android_alarm.h>
#endif

#include <linux/mfd/core.h>
#include <linux/mfd/max77823.h>
#include <linux/mfd/max77823-private.h>
#if defined(CONFIG_FUELGAUGE_MAX77843)
#include <linux/mfd/max77843-private.h>
#endif

#include <linux/regulator/machine.h>

#if defined(CONFIG_FUELGAUGE_MAX77823_VOLTAGE_TRACKING)
#define MAX77823_REG_STATUS		0x00
#define MAX77823_REG_VALRT_TH		0x01
#define MAX77823_REG_TALRT_TH		0x02
#define MAX77823_REG_SALRT_TH		0x03
#define MAX77823_REG_VCELL			0x09
#define MAX77823_REG_TEMPERATURE	0x08
#define MAX77823_REG_AVGVCELL		0x19
#define MAX77823_REG_CONFIG		0x1D
#define MAX77823_REG_VERSION		0x21
#define MAX77823_REG_LEARNCFG		0x28
#define MAX77823_REG_FILTERCFG	0x29
#define MAX77823_REG_MISCCFG		0x2B
#define MAX77823_REG_CGAIN		0x2E
#define MAX77823_REG_RCOMP		0x38
#define MAX77823_REG_VFOCV		0xFB
#define MAX77823_REG_SOC_VF		0xFF
#endif

#if defined(CONFIG_FUELGAUGE_MAX77823_COULOMB_COUNTING) || \
	defined(CONFIG_FUELGAUGE_MAX77843)
#define PRINT_COUNT	10

#define STATUS_REG				0x00
#define VALRT_THRESHOLD_REG	0x01
#define TALRT_THRESHOLD_REG	0x02
#define SALRT_THRESHOLD_REG	0x03
#define REMCAP_REP_REG			0x05
#define SOCREP_REG				0x06
#define TEMPERATURE_REG		0x08
#define VCELL_REG				0x09
#define CURRENT_REG				0x0A
#define AVG_CURRENT_REG		0x0B
#define SOCMIX_REG				0x0D
#define SOCAV_REG				0x0E
#define REMCAP_MIX_REG			0x0F
#define FULLCAP_REG				0x10
#define TIME_TO_EMPTY_REG		0x11
#define FULLCAPREP_REG			0x35
#define RFAST_REG				0x15
#define AVR_TEMPERATURE_REG	0x16
#define CYCLES_REG				0x17
#define DESIGNCAP_REG			0x18
#define AVR_VCELL_REG			0x19
#define TIME_TO_FULL_REG		0x20
#define CONFIG_REG				0x1D
#define REMCAP_AV_REG			0x1F
#define FULLCAP_NOM_REG		0x23
#define MISCCFG_REG				0x2B
#define QRTABLE20_REG			0x32
#define RCOMP_REG				0x38
#define FSTAT_REG				0x3D
#define QRTABLE30_REG			0x42
#define DQACC_REG				0x45
#define DPACC_REG				0x46
#define OCV_REG					0xEE
#define VFOCV_REG				0xFB
#define VFSOC_REG				0xFF

#define LOW_BATT_COMP_RANGE_NUM	5
#define LOW_BATT_COMP_LEVEL_NUM	2
#define MAX_LOW_BATT_CHECK_CNT	10

enum {
	FG_LEVEL = 0,
	FG_TEMPERATURE,
	FG_VOLTAGE,
	FG_CURRENT,
	FG_CURRENT_AVG,
	FG_CHECK_STATUS,
	FG_RAW_SOC,
	FG_VF_SOC,
	FG_AV_SOC,
	FG_FULLCAP,
	FG_MIXCAP,
	FG_AVCAP,
	FG_REPCAP,
};

enum {
	POSITIVE = 0,
	NEGATIVE,
};

enum {
	RANGE = 0,
	SLOPE,
	OFFSET,
	TABLE_MAX
};

#define CURRENT_RANGE_MAX_NUM	5
#define TEMP_RANGE_MAX_NUM	3

struct max77823_fuelgauge_battery_data_t {
	u8 rcomp0;
	u8 rcomp_charging;
	u16 QResidual20;
	u16 QResidual30;
	u16 Capacity;
	u16 low_battery_comp_voltage;
	s32 low_battery_table[CURRENT_RANGE_MAX_NUM][TABLE_MAX];
	s32 temp_adjust_table[TEMP_RANGE_MAX_NUM][TABLE_MAX];
	u8	*type_str;
};

struct max77823_fuelgauge_info {
	/* test print count */
	int pr_cnt;
	/* full charge comp */
	struct delayed_work	full_comp_work;
	u32 previous_fullcap;
	u32 previous_vffullcap;
	/* low battery comp */
	int low_batt_comp_cnt[LOW_BATT_COMP_RANGE_NUM][LOW_BATT_COMP_LEVEL_NUM];
	int low_batt_comp_flag;
	/* low battery boot */
	int low_batt_boot_flag;
	bool is_low_batt_alarm;

	/* battery info */
	u32 soc;

	/* miscellaneous */
	unsigned long fullcap_check_interval;
	int full_check_flag;
	bool is_first_check;
};

/* FullCap learning setting */
#define VFFULLCAP_CHECK_INTERVAL	300 /* sec */
/* soc should be 0.1% unit */
#define VFSOC_FOR_FULLCAP_LEARNING	950
#define LOW_CURRENT_FOR_FULLCAP_LEARNING	20
#define HIGH_CURRENT_FOR_FULLCAP_LEARNING	120
#define LOW_AVGCURRENT_FOR_FULLCAP_LEARNING	20
#define HIGH_AVGCURRENT_FOR_FULLCAP_LEARNING	100

/* power off margin */
/* soc should be 0.1% unit */
#define POWER_OFF_SOC_HIGH_MARGIN	20
#define POWER_OFF_VOLTAGE_HIGH_MARGIN	3500
#define POWER_OFF_VOLTAGE_LOW_MARGIN	3400

/* FG recovery handler */
/* soc should be 0.1% unit */
#define STABLE_LOW_BATTERY_DIFF	30
#define STABLE_LOW_BATTERY_DIFF_LOWBATT	10
#define LOW_BATTERY_SOC_REDUCE_UNIT	10

struct max77823_fuelgauge_data {
	struct device           *dev;
	struct i2c_client       *i2c;
	struct mutex            fuelgauge_mutex;
	struct max77823_platform_data *max77823_pdata;
#if defined(CONFIG_FUELGAUGE_MAX77843)
	struct max77843_platform_data *max77843_pdata;
#endif
	sec_battery_platform_data_t *pdata;
	struct max77823_fuelgauge_battery_data_t *battery_data;
	struct power_supply		psy_fg;
	struct delayed_work isr_work;

	int cable_type;
	bool is_charging;

	/* HW-dedicated fuel guage info structure
	 * used in individual fuel gauge file only
	 * (ex. dummy_fuelgauge.c)
	 */
	struct max77823_fuelgauge_info info;

	bool is_fuel_alerted;
	struct wake_lock fuel_alert_wake_lock;

	unsigned int capacity_old;	/* only for atomic calculation */
	unsigned int capacity_max;	/* only for dynamic calculation */

	bool initial_update_of_soc;
	struct mutex fg_lock;

	/* register programming */
	int reg_addr;
	u8 reg_data[2];

	int fg_irq;
};
#endif
#endif /* __MAX77823_FUELGAUGE_H */
