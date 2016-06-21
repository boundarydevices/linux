/*
 *  max77823_fuelgauge.c
 *  Samsung MAX77823 Fuel Gauge Driver
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/mfd/max77823.h>
#include <linux/mfd/max77823-private.h>
#include <linux/of_gpio.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/battery/fuelgauge/max77823_fuelgauge.h>
#include <linux/math64.h>

#define MV_TO_UV(mv) (mv * 1000)

extern void board_fuelgauge_init(void *data);

static enum power_supply_property max77823_fuelgauge_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN,
	POWER_SUPPLY_PROP_ENERGY_EMPTY_DESIGN,
	POWER_SUPPLY_PROP_ENERGY_FULL,
	POWER_SUPPLY_PROP_ENERGY_EMPTY,
	POWER_SUPPLY_PROP_ENERGY_NOW,
	POWER_SUPPLY_PROP_ENERGY_AVG,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TEMP_AMBIENT,
};

static const char* const psy_names[] = {
[PS_BATT] = "battery",
};

static int psy_get_prop(struct max77823_fuelgauge_data *fuelgauge, enum ps_id id, enum power_supply_property property, union power_supply_propval *value)
{
	struct power_supply *psy = fuelgauge->psy_ref[id];
	int ret = -EINVAL;

	value->intval = 0;
	if (!psy) {
		unsigned long timeout = jiffies + msecs_to_jiffies(500);
		do {
			psy = power_supply_get_by_name(psy_names[id]);
			if (psy) {
				fuelgauge->psy_ref[id] = psy;
				break;
			}
			if (time_after(jiffies, timeout)) {
				pr_err("%s: fuel Failed %s(%d)\n",  __func__, psy_names[id], property);
				return -ENODEV;
			}
			msleep(1);
		} while (1);
	}
	if (psy->desc->get_property) {
		ret = psy->desc->get_property(psy, property, value);
		if (ret < 0)
			pr_err("%s: fuel Fail to get %s(%d=>%d)\n", __func__, psy_names[id], property, ret);
	}
	return ret;
}

static int psy_set_prop(struct max77823_fuelgauge_data *fuelgauge, enum ps_id id, enum power_supply_property property, union power_supply_propval *value)
{
	struct power_supply *psy = fuelgauge->psy_ref[id];
	int ret = -EINVAL;

	if (!psy) {
		unsigned long timeout = jiffies + msecs_to_jiffies(500);
		do {
			psy = power_supply_get_by_name(psy_names[id]);
			if (psy) {
				fuelgauge->psy_ref[id] = psy;
				break;
			}
			if (time_after(jiffies, timeout)) {
				pr_err("%s: fuel Failed %s\n",  __func__, psy_names[id]);
				return -ENODEV;
			}
			msleep(1);
		} while (1);
	}
	if (psy->desc->set_property) {
		ret = psy->desc->set_property(psy, property, value);
		if (ret < 0)
			pr_err("%s: fuel Fail to set %s(%d=>%d)\n", __func__, psy_names[id], property, ret);
	}
	return ret;
}

static int max77823_get_v(struct max77823_fuelgauge_data *fuelgauge, int reg)
{
	u32 v = 0;
	int ret;

	ret = max77823_read_word(fuelgauge->i2c, reg);
	if (ret < 0)
		return ret;

	pr_debug("%s: reg 0x%x=(%d)\n", __func__, reg, ret);
	v = (ret >> 3) * 625;
	return v;
}

static int max77823_get_vfocv(struct max77823_fuelgauge_data *fuelgauge)
{
	return max77823_get_v(fuelgauge, MAX77823_REG_VFOCV);
}

static int max77823_get_vcell(struct max77823_fuelgauge_data *fuelgauge)
{
	return max77823_get_v(fuelgauge, MAX77823_REG_VCELL);
}

static int max77823_get_avgvcell(struct max77823_fuelgauge_data *fuelgauge)
{
	return max77823_get_v(fuelgauge, MAX77823_REG_AVGVCELL);
}

static int max77823_get_status(struct max77823_fuelgauge_data *fuelgauge)
{
	int ret;

	ret = max77823_read_word(fuelgauge->i2c, MAX77823_REG_STATUS);
	if (ret < 0)
		return ret;

	pr_debug("%s: reg 0x%x=(0x%x)\n", __func__, MAX77823_REG_STATUS, ret);
	if (ret & (1 << 3)) {
		/* battery is absent */
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
	}
	ret = max77823_read_word(fuelgauge->i2c, MAX77823_REG_STATUS2);
	if (ret < 0)
		return ret;
	pr_debug("%s: reg 0x%x=(0x%x)\n", __func__, MAX77823_REG_STATUS2, ret);
	if (ret & (1 << 5)) {
		return POWER_SUPPLY_STATUS_FULL;
	}
	ret = max77823_read_word(fuelgauge->i2c, MAX77823_REG_MAXMIN);
	if (ret < 0)
		return ret;
	pr_debug("%s: reg 0x%x=(0x%x)\n", __func__, MAX77823_REG_MAXMIN, ret);
	max77823_write_word(fuelgauge->i2c, MAX77823_REG_MAXMIN, 0x807f);
	if (ret == 0x807f)
		ret = fuelgauge->prev_status;
	else
		fuelgauge->prev_status = ret;
	ret <<= 16;	/* sign extend */
	ret >>= 16;
	if (ret > 0)
		return POWER_SUPPLY_STATUS_CHARGING;
	ret <<= 24;	/* sign extend */
	ret >>= 24;
	if (ret < 0)
		return POWER_SUPPLY_STATUS_DISCHARGING;
	return POWER_SUPPLY_STATUS_NOT_CHARGING;
}

#ifdef CONFIG_FUELGAUGE_MAX77823_VOLTAGE_TRACKING
static void max77823_init_regs(struct max77823_fuelgauge_data *fuelgauge)
{
	int ret;

	ret = max77823_read_word(fuelgauge->i2c, MAX77823_REG_FILTERCFG);
	if (ret < 0)
		return;

	/* Clear average vcell (12 sec) */
	max77823_write_word(fuelgauge->i2c, MAX77823_REG_FILTERCFG, ret & 0xff8f);
}

static void max77823_get_version(struct max77823_fuelgauge_data *fuelgauge)
{
	int ret;

	ret = max77823_read_word(fuelgauge->i2c, MAX77823_REG_VERSION);
	if (ret < 0)
		return;
	pr_debug("MAX77823 Fuel-Gauge Ver 0x%x\n", ret);
}

static void max77823_alert_init(struct max77823_fuelgauge_data *fuelgauge)
{
	/* SALRT Threshold setting */
	max77823_write_word(fuelgauge->i2c, MAX77823_REG_SALRT_TH,
			0xff00 | fuelgauge->pdata->fuel_alert_soc);

	/* VALRT Threshold setting */
	max77823_write_word(fuelgauge->i2c, MAX77823_REG_VALRT_TH, 0xff00);

	/* TALRT Threshold setting */
	max77823_write_word(fuelgauge->i2c, MAX77823_REG_TALRT_TH, 0x7f80);
}

static bool max77823_check_status(struct max77823_fuelgauge_data *fuelgauge)
{
	bool ret = false;

	/* check if Smn was generated */
	ret = max77823_read_word(fuelgauge->i2c, MAX77823_REG_STATUS);
	if (ret < 0)
		return ret;

	pr_info("%s: status_reg(%x)\n", __func__, ret);

	/* minimum SOC threshold exceeded. */
	if (ret & (0x1 << 10))
		ret = true;

	/* clear status reg */
	max77823_write_word(fuelgauge->i2c, MAX77823_REG_STATUS, ret & 0xff);
	msleep(200);
	return ret;
}

static int max77823_set_temperature(struct max77823_fuelgauge_data *fuelgauge,
				    int temperature)
{
	max77823_write_word(fuelgauge->i2c, MAX77823_REG_TEMPERATURE, temperature << 8);
	pr_debug("%s: temperature to (%d)\n", __func__, temperature);
	return temperature;
}
/* return units of 1/10 C */
static int max77823_get_temperature(struct max77823_fuelgauge_data *fuelgauge)
{
	int ret;

	if (fuelgauge->pdata->temp_disabled)
		return 200;
	ret = max77823_read_word(fuelgauge->i2c, MAX77823_REG_TEMPERATURE);
	if (ret < 0)
		return -ERANGE;

	/* data[] store 2's compliment format number */
	ret <<= 16;
	ret >>= 16;	/* extent sign bit */
	ret = (ret * 10) >> 8;
	pr_debug("%s: temperature (%d)\n", __func__, ret);
	return ret;
}

/* soc should be 0.01% unit */
static int max77823_get_soc(struct max77823_fuelgauge_data *fuelgauge)
{
	int soc;
	int ret;

	ret = max77823_read_word(fuelgauge->i2c, MAX77823_REG_SOC_VF);
	if (ret < 0)
		return ret;

	soc = (ret * 100) >> 8;
	pr_debug("%s: raw capacity (%d)\n", __func__, soc);
	return min(soc, 10000);
}

bool max77823_fg_init(struct max77823_fuelgauge_data *fuelgauge)
{
	/* initialize fuel gauge registers */
	max77823_init_regs(fuelgauge);

	max77823_get_version(fuelgauge);

	return true;
}

bool max77823_fg_fuelalert_init(struct max77823_fuelgauge_data *fuelgauge,
				int soc)
{
	int ret;

	/* 1. Set max77823 alert configuration. */
	max77823_alert_init(fuelgauge);

	ret = max77823_read_word(fuelgauge->i2c, MAX77823_REG_CONFIG);
	if (ret < 0)
		return ret;

	/*Enable Alert (Aen = 1) */
	ret |= (1 << 2);
	max77823_write_word(fuelgauge->i2c, MAX77823_REG_CONFIG, ret);

	pr_debug("%s: config_reg(%x) irq(%d)\n", __func__, ret,
			fuelgauge->pdata->fg_irq);
	return true;
}

bool max77823_fg_is_fuelalerted(struct max77823_fuelgauge_data *fuelgauge)
{
	return max77823_check_status(fuelgauge);
}

bool max77823_fg_fuelalert_process(void *irq_data, bool is_fuel_alerted)
{
	struct max77823_fuelgauge_data *fuelgauge = irq_data;
	int ret;

	/* update SOC */
	/* max77823_get_soc(fuelgauge); */

	ret = max77823_read_word(fuelgauge->i2c, MAX77823_REG_CONFIG);
	if (ret < 0)
		return false;
	if (is_fuel_alerted) {
		ret |= (1 << 11);
		pr_info("%s: Fuel-alert Alerted!! (%x)\n", __func__, ret);
	} else {
		ret &= ~(1 << 11);
		pr_info("%s: Fuel-alert Released!! (%x)\n", __func__, ret);
	}
	max77823_write_word(fuelgauge->i2c, MAX77823_REG_CONFIG, ret);

	ret = max77823_read_word(fuelgauge->i2c, MAX77823_REG_VCELL);
	pr_debug("%s: MAX77823_REG_VCELL(%x)\n", __func__, ret);

	ret = max77823_read_word(fuelgauge->i2c, MAX77823_REG_TEMPERATURE);
	pr_debug("%s: MAX77823_REG_TEMPERATURE(%x)\n", __func__, ret);

	ret = max77823_read_word(fuelgauge->i2c, MAX77823_REG_CONFIG);
	pr_debug("%s: MAX77823_REG_CONFIG(%x)\n", __func__, ret);

	ret = max77823_read_word(fuelgauge->i2c, MAX77823_REG_VFOCV);
	pr_debug("%s: MAX77823_REG_VFOCV(%x)\n", __func__, ret);

	ret = max77823_read_word(fuelgauge->i2c, MAX77823_REG_SOC_VF);
	pr_debug("%s: MAX77823_REG_SOC_VF(%x)\n", __func__, ret);

	pr_debug("%s: FUEL GAUGE IRQ (%d)\n",
		 __func__, gpio_get_value(fuelgauge->pdata->fg_irq));

	return true;
}

bool max77823_fg_full_charged(struct max77823_fuelgauge_data *fuelgauge)
{
	return true;
}
#endif

static void fg_test_print(struct max77823_fuelgauge_data *fuelgauge)
{
#ifdef DEBUG
	u32 average_vcell;
	u32 temp;
	u32 temp2;
	u16 reg_data;
	int ret;

	ret = max77823_read_word(fuelgauge->i2c, MAX77823_REG_AVGVCELL);
	if (ret < 0) {
		pr_err("%s: Failed to read VCELL\n", __func__);
		return;
	}

	temp = (ret & 0xFFF) * 78125;
	average_vcell = temp / 1000000;

	temp = ((ret & 0xF000) >> 4) * 78125;
	temp2 = temp / 1000000;
	average_vcell += (temp2 << 4);

	pr_info("%s: AVG_VCELL(%d), data(0x%04x)\n", __func__,
		average_vcell, ret);

	reg_data = max77823_read_word(fuelgauge->i2c, FULLCAPREP_REG);
	pr_info("%s: FULLCAP(%d), data(0x%04x)\n", __func__,
		reg_data/2, reg_data);

	reg_data = max77823_read_word(fuelgauge->i2c, REMCAP_REP_REG);
	pr_info("%s: REMCAP_REP(%d), data(0x%04x)\n", __func__,
		reg_data/2, reg_data);

	reg_data = max77823_read_word(fuelgauge->i2c, REMCAP_MIX_REG);
	pr_info("%s: REMCAP_MIX(%d), data(0x%04x)\n", __func__,
		reg_data/2, reg_data);

	reg_data = max77823_read_word(fuelgauge->i2c, REMCAP_AV_REG);
	pr_info("%s: REMCAP_AV(%d), data(0x%04x)\n", __func__,
		reg_data/2, reg_data);

	reg_data = max77823_read_word(fuelgauge->i2c, MAX77823_REG_CONFIG);
	pr_info("%s: CONFIG(0x%02x), data(0x%04x)\n", __func__,
		MAX77823_REG_CONFIG, reg_data);
#endif
}

static void fg_periodic_read(struct max77823_fuelgauge_data *fuelgauge)
{
#ifdef DEBUG
	u8 reg;
	int i;
	int data[0x10];
	char *str = NULL;

	str = kzalloc(sizeof(char)*1024, GFP_KERNEL);
	if (!str)
		return;

	for (i = 0; i < 16; i++) {
		for (reg = 0; reg < 0x10; reg++)
			data[reg] = max77823_read_word(fuelgauge->i2c, reg + i * 0x10);

		sprintf(str+strlen(str),
			"%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,",
			data[0x00], data[0x01], data[0x02], data[0x03],
			data[0x04], data[0x05], data[0x06], data[0x07]);
		sprintf(str+strlen(str),
			"%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,",
			data[0x08], data[0x09], data[0x0a], data[0x0b],
			data[0x0c], data[0x0d], data[0x0e], data[0x0f]);
		if (i == 4)
			i = 13;
	}

	pr_info("%s", str);

	kfree(str);
#endif
}

#ifdef CONFIG_FUELGAUGE_MAX77823_COULOMB_COUNTING
static int fg_check_battery_present(struct max77823_fuelgauge_data *fuelgauge)
{
	int ret;

	/* 1. Check Bst bit */
	ret = max77823_read_word(fuelgauge->i2c, MAX77823_REG_STATUS);
	if (ret < 0) {
		pr_err("%s: Failed to read STATUS\n", __func__);
		return 0;
	}

	if (ret & (0x1 << 3)) {
		pr_info("%s: status(0x%x)\n", __func__, ret);
		pr_info("%s: battery is absent!!\n", __func__);
		return 0;
	}
	return 1;
}

static int fg_write_temp(struct max77823_fuelgauge_data *fuelgauge,
			 int temperature)
{
	max77823_write_word(fuelgauge->i2c, MAX77823_REG_TEMPERATURE, (temperature * 256) / 10);
	pr_debug("%s: temperature to %d\n", __func__, temperature);
	return temperature;
}

static int fg_read_temp(struct max77823_fuelgauge_data *fuelgauge)
{
	int temper = 200;	/* 20.0 C */
	int ret;

	if (fuelgauge->pdata->temp_disabled)
		return 200;
	if (fg_check_battery_present(fuelgauge)) {
		ret = max77823_read_word(fuelgauge->i2c, MAX77823_REG_TEMPERATURE);
		if (ret < 0) {
			pr_err("%s: Failed to read TEMPERATURE\n", __func__);
			return temper;
		}

		ret <<= 16;	/* sign extend value */
		ret >>= 16;
		temper = (ret * 10) >> 8;
	}

	if (!(fuelgauge->info.pr_cnt % PRINT_COUNT))
		pr_info("%s: TEMPERATURE(%d)\n",
			__func__, temper);
	return temper;
}
#endif

/* soc should be 0.1% unit */
static int fg_read_percent(struct max77823_fuelgauge_data *fuelgauge, int reg)
{
	int soc;
	int ret;

	ret = max77823_read_word(fuelgauge->i2c, reg);
	if (ret < 0) {
		pr_err("%s: Failed to read 0x%x\n", __func__, reg);
		return ret;
	}
	soc = (ret * 10) >> 8;
	return min(soc, 1000);
}

static int fg_read_vfsoc(struct max77823_fuelgauge_data *fuelgauge)
{
	return fg_read_percent(fuelgauge, MAX77823_REG_SOC_VF);
}

#ifdef CONFIG_FUELGAUGE_MAX77823_COULOMB_COUNTING
static int fg_read_power(struct max77823_fuelgauge_data *fuelgauge, int reg)
{

	int ret;

	ret = max77823_read_word(fuelgauge->i2c, reg);
	if (ret < 0)
		pr_err("%s: Failed to read 0x%x(%d)\n", __func__, reg, ret);
	/*
	 * convert from uVh to uWh, with a 10 mOhm sense resistor
	 *  uVh/(.010 Ohms) =  uVh * 100/Ohms =  100uAh
	 *  uAh * V = uWh
	 */
	return ret * 370 ;	/* 3.7 Volts nominal */
}
#endif

static int fg_read_current(struct max77823_fuelgauge_data *fuelgauge)
{
	u32 temp;
	s32 i_current;
	s32 avg_current;
	int ret;

	ret = max77823_read_word(fuelgauge->i2c, CURRENT_REG);
	if (ret < 0) {
		pr_err("%s: Failed to read CURRENT\n", __func__);
		return ret;
	}
	temp = ret << 15;	/* extend sign bit */
	temp >>= 15;

	/* 1.5625uV / 0.01Ohm(Rsense) = 156.25uA */
	i_current = temp * 15625;
	i_current /= 100;

	ret = max77823_read_word(fuelgauge->i2c, AVG_CURRENT_REG);
	if (ret < 0) {
		pr_err("%s: Failed to read AVERAGE CURRENT\n", __func__);
		return ret;
	}

	temp = ret << 15;	/* extend sign bit */
	temp >>= 15;

	/* 1.5625uV/0.01Ohm(Rsense) = 156.25uA */
	avg_current = (temp * 15625) / 100000;

	if (!(fuelgauge->info.pr_cnt++ % PRINT_COUNT)) {
#ifdef DEBUG
		fg_test_print(fuelgauge);
		pr_debug("%s: CURRENT(%dmA), AVG_CURRENT(%dmA)\n", __func__,
				i_current, avg_current);
		/* Read max77823's all registers every 5 minute. */
		fg_periodic_read(fuelgauge);
#endif
		fuelgauge->info.pr_cnt = 1;
	}

	return i_current;
}

static int fg_read_avg_current(struct max77823_fuelgauge_data *fuelgauge)
{
	u32 temp;
	s32 avg_current;
	int ret;

	ret = max77823_read_word(fuelgauge->i2c, AVG_CURRENT_REG);
	if (ret < 0) {
		pr_err("%s: Failed to read AVERAGE CURRENT\n",
		       __func__);
		return 0;
	}

	temp = ret << 15;	/* extend sign bit */
	temp >>= 15;
	/* 1.5625uV/0.01Ohm(Rsense) = 156.25uA */
	avg_current = temp * 15625;
	avg_current /= 100;
	return avg_current;
}

int fg_reset_soc(struct max77823_fuelgauge_data *fuelgauge)
{
	int vfocv, fullcap;
	int ret;

	/* delay for current stablization */
	msleep(500);

	pr_info("%s: Before quick-start - VCELL(%d), VFOCV(%d), VfSOC(%d), RepSOC(%d)\n",
		__func__, max77823_get_vcell(fuelgauge), max77823_get_vfocv(fuelgauge),
		fg_read_vfsoc(fuelgauge), fg_read_percent(fuelgauge, SOCREP_REG));
	pr_info("%s: Before quick-start - current(%d), avg current(%d)\n",
		__func__, fg_read_current(fuelgauge),
		fg_read_avg_current(fuelgauge));

	if (fuelgauge->pdata->check_jig_status ||
	    !fuelgauge->pdata->check_jig_status()) {
		pr_info("%s : Return by No JIG_ON signal\n", __func__);
		return 0;
	}

	max77823_write_word(fuelgauge->i2c, CYCLES_REG, 0);

	ret = max77823_read_word(fuelgauge->i2c, MAX77823_REG_MISCCFG);
	if (ret < 0) {
		pr_err("%s: Failed to read MiscCFG\n", __func__);
		return ret;
	}

	ret |= (0x1 << 10);
	ret = max77823_write_word(fuelgauge->i2c, MAX77823_REG_MISCCFG, ret);
	if (ret < 0) {
		pr_err("%s: Failed to write MiscCFG\n", __func__);
		return ret;
	}

	msleep(250);
	max77823_write_word(fuelgauge->i2c, FULLCAPREP_REG,
			    fuelgauge->battery_data->Capacity);
	msleep(500);

	pr_info("%s: After quick-start - VCELL(%d), VFOCV(%d), VfSOC(%d), RepSOC(%d)\n",
		__func__, max77823_get_vcell(fuelgauge), max77823_get_vfocv(fuelgauge),
		fg_read_vfsoc(fuelgauge), fg_read_percent(fuelgauge, SOCREP_REG));
	pr_info("%s: After quick-start - current(%d), avg current(%d)\n",
		__func__, fg_read_current(fuelgauge),
		fg_read_avg_current(fuelgauge));

	max77823_write_word(fuelgauge->i2c, CYCLES_REG, 0x00a0);

/* P8 is not turned off by Quickstart @3.4V
 * (It's not a problem, depend on mode data)
 * Power off for factory test(File system, etc..) */
	vfocv = max77823_get_vfocv(fuelgauge);
	if (vfocv < POWER_OFF_VOLTAGE_LOW_MARGIN) {
		pr_info("%s: Power off condition(%d)\n", __func__, vfocv);

		fullcap = max77823_read_word(fuelgauge->i2c, FULLCAPREP_REG);

		/* FullCAP * 0.009 */
		max77823_write_word(fuelgauge->i2c, REMCAP_REP_REG,
				    (u16)(fullcap * 9 / 1000));
		msleep(200);
		pr_info("%s: new soc=%d, vfocv=%d\n", __func__,
			fg_read_percent(fuelgauge, SOCREP_REG), vfocv);
	}

	pr_info("%s: Additional step - VfOCV(%d), VfSOC(%d), RepSOC(%d)\n",
		__func__, max77823_get_vfocv(fuelgauge),
		fg_read_vfsoc(fuelgauge), fg_read_percent(fuelgauge, SOCREP_REG));

	return 0;
}
#ifdef CONFIG_FUELGAUGE_MAX77823_COULOMB_COUNTING

int fg_reset_capacity_by_jig_connection(struct max77823_fuelgauge_data *fuelgauge)
{

	pr_info("%s: DesignCap = Capacity - 1 (Jig Connection)\n", __func__);

	return max77823_write_word(fuelgauge->i2c, DESIGNCAP_REG,
				   fuelgauge->battery_data->Capacity-1);
}

int fg_adjust_capacity(struct max77823_fuelgauge_data *fuelgauge)
{
	int ret;

	/* 1. Write RemCapREP(05h)=0; */
	ret = max77823_write_word(fuelgauge->i2c, REMCAP_REP_REG, 0);
	if (ret < 0) {
		pr_err("%s: Failed to write RemCap_REP\n", __func__);
		return ret;
	}
	msleep(200);

	pr_info("%s: After adjust - RepSOC(%d)\n", __func__,
		fg_read_percent(fuelgauge, SOCREP_REG));

	return 0;
}

void fg_low_batt_compensation(struct max77823_fuelgauge_data *fuelgauge,
			      u32 level)
{
	int read_val;
	u32 temp;

	pr_info("%s: Adjust SOCrep to %d!!\n", __func__, level);

	read_val = max77823_read_word(fuelgauge->i2c, FULLCAPREP_REG);
	/* RemCapREP (05h) = FullCap(10h) x 0.0090 */
	temp = read_val * (level*90) / 10000;
	max77823_write_word(fuelgauge->i2c, REMCAP_REP_REG,
			    (u16)temp);
}

static int fg_check_status_reg(struct max77823_fuelgauge_data *fuelgauge)
{
	int ret;
	int status;

	/* 1. Check Smn was generated read */
	ret = max77823_read_word(fuelgauge->i2c, MAX77823_REG_STATUS);
	if (ret < 0) {
		pr_err("%s: Failed to read STATUS\n", __func__);
		return ret;
	}
	status = ret;
	pr_info("%s: addr(0x00), data(0x%x)\n", __func__, status);

	/* 2. clear Status reg */
	ret = max77823_write_word(fuelgauge->i2c, MAX77823_REG_STATUS, status & 0xff);
	if (ret < 0) {
		pr_info("%s: Failed to write STATUS\n", __func__);
		return ret;
	}
	return (status >> 10) & 1;
}

int fg_alert_init(struct max77823_fuelgauge_data *fuelgauge, int soc)
{
	int ret;

	/* Using RepSOC */
	ret = max77823_read_word(fuelgauge->i2c, MAX77823_REG_MISCCFG);
	if (ret < 0) {
		pr_err("%s: Failed to read MISCCFG\n", __func__);
		return ret;
	}

	ret = max77823_write_word(fuelgauge->i2c, MAX77823_REG_MISCCFG, ret & ~3);
	if (ret < 0) {
		pr_info("%s: Failed to write MISCCFG\n", __func__);
		return ret;
	}

	/* SALRT Threshold setting */
	ret = max77823_write_word(fuelgauge->i2c, MAX77823_REG_SALRT_TH, 0xff00 | soc);
	if (ret < 0) {
		pr_info("%s: Failed to write SALRT\n", __func__);
		return -1;
	}

	/* Reset VALRT Threshold setting (disable) */
	ret = max77823_write_word(fuelgauge->i2c, MAX77823_REG_VALRT_TH, 0xff00);
	if (ret < 0) {
		pr_info("%s: Failed to write VALRT\n", __func__);
		return ret;
	}

	ret = max77823_read_word(fuelgauge->i2c, MAX77823_REG_VALRT_TH);
	if (ret != 0xff00)
		pr_err("%s: VALRT is not valid (0x%x)\n", __func__, ret);

	/* Reset TALRT Threshold setting (disable) */
	ret = max77823_write_word(fuelgauge->i2c, MAX77823_REG_TALRT_TH, 0x7f80);
	if (ret < 0) {
		pr_info("%s: Failed to write TALRT\n", __func__);
		return ret;
	}

	ret = max77823_read_word(fuelgauge->i2c, MAX77823_REG_TALRT_TH);
	if (ret != 0x7f80)
		pr_err("%s: TALRT is not valid (0x%x)\n", __func__, ret);

	/*mdelay(100);*/

	/* Enable SOC alerts */
	ret = max77823_read_word(fuelgauge->i2c, MAX77823_REG_CONFIG);
	if (ret < 0) {
		pr_err("%s: Failed to read CONFIG\n", __func__);
		return ret;
	}

	ret = max77823_write_word(fuelgauge->i2c, MAX77823_REG_CONFIG, ret | (1 << 2));
	if (ret < 0) {
		pr_info("%s: Failed to write CONFIG\n", __func__);
		return ret;
	}
	return 1;
}

void fg_fullcharged_compensation(struct max77823_fuelgauge_data *fuelgauge,
		u32 is_recharging, bool pre_update)
{
	static int new_fullcap_data;

	pr_info("%s: is_recharging(%d), pre_update(%d)\n",
		__func__, is_recharging, pre_update);

	new_fullcap_data =
		max77823_read_word(fuelgauge->i2c, FULLCAPREP_REG);
	if (new_fullcap_data < 0)
		new_fullcap_data = fuelgauge->battery_data->Capacity;

	/* compare with initial capacity */
	if (new_fullcap_data >
		(fuelgauge->battery_data->Capacity * 110 / 100)) {
		pr_info("%s: [Case 1] capacity = 0x%04x, NewFullCap = 0x%04x\n",
			__func__, fuelgauge->battery_data->Capacity,
			new_fullcap_data);

		new_fullcap_data =
			(fuelgauge->battery_data->Capacity * 110) / 100;

		max77823_write_word(fuelgauge->i2c, REMCAP_REP_REG,
				    (u16)(new_fullcap_data));
		max77823_write_word(fuelgauge->i2c, FULLCAPREP_REG,
				    (u16)(new_fullcap_data));
	} else if (new_fullcap_data <
		(fuelgauge->battery_data->Capacity * 50 / 100)) {
		pr_info("%s: [Case 5] capacity = 0x%04x, NewFullCap = 0x%04x\n",
			__func__, fuelgauge->battery_data->Capacity,
			new_fullcap_data);

		new_fullcap_data =
			(fuelgauge->battery_data->Capacity * 50) / 100;

		max77823_write_word(fuelgauge->i2c, REMCAP_REP_REG,
				    (u16)(new_fullcap_data));
		max77823_write_word(fuelgauge->i2c, FULLCAPREP_REG,
				    (u16)(new_fullcap_data));
	} else {
	/* compare with previous capacity */
		if (new_fullcap_data >
			(fuelgauge->info.previous_fullcap * 110 / 100)) {
			pr_info("%s: [Case 2] previous_fullcap = 0x%04x, NewFullCap = 0x%04x\n",
				__func__, fuelgauge->info.previous_fullcap,
				new_fullcap_data);

			new_fullcap_data =
				(fuelgauge->info.previous_fullcap * 110) / 100;

			max77823_write_word(fuelgauge->i2c, REMCAP_REP_REG,
					    (u16)(new_fullcap_data));
			max77823_write_word(fuelgauge->i2c, FULLCAPREP_REG,
					    (u16)(new_fullcap_data));
		} else if (new_fullcap_data <
			(fuelgauge->info.previous_fullcap * 90 / 100)) {
			pr_info("%s: [Case 3] previous_fullcap = 0x%04x, NewFullCap = 0x%04x\n",
				__func__, fuelgauge->info.previous_fullcap,
				new_fullcap_data);

			new_fullcap_data =
				(fuelgauge->info.previous_fullcap * 90) / 100;

			max77823_write_word(fuelgauge->i2c, REMCAP_REP_REG,
					    (u16)(new_fullcap_data));
			max77823_write_word(fuelgauge->i2c, FULLCAPREP_REG,
					    (u16)(new_fullcap_data));
		} else {
			pr_info("%s: [Case 4] previous_fullcap = 0x%04x, NewFullCap = 0x%04x\n",
				__func__, fuelgauge->info.previous_fullcap,
				new_fullcap_data);
		}
	}

	/* 4. Write RepSOC(06h)=100%; */
	max77823_write_word(fuelgauge->i2c, SOCREP_REG, (u16)(0x64 << 8));

	/* 5. Write MixSOC(0Dh)=100%; */
	max77823_write_word(fuelgauge->i2c, SOCMIX_REG, (u16)(0x64 << 8));

	/* 6. Write AVSOC(0Eh)=100%; */
	max77823_write_word(fuelgauge->i2c, SOCAV_REG, (u16)(0x64 << 8));

	/* if pre_update case, skip updating PrevFullCAP value. */
	if (!pre_update)
		fuelgauge->info.previous_fullcap =
			max77823_read_word(fuelgauge->i2c, FULLCAPREP_REG);

	pr_debug("%s: (A) FullCap = 0x%04x, RemCap = 0x%04x\n", __func__,
		max77823_read_word(fuelgauge->i2c, FULLCAPREP_REG),
		max77823_read_word(fuelgauge->i2c, REMCAP_REP_REG));

	fg_periodic_read(fuelgauge);
}

void fg_check_vf_fullcap_range(struct max77823_fuelgauge_data *fuelgauge)
{
	static int new_vffullcap;
	bool is_vffullcap_changed = true;

	if (fuelgauge->pdata->check_jig_status &&
	    fuelgauge->pdata->check_jig_status())
		fg_reset_capacity_by_jig_connection(fuelgauge);

	new_vffullcap = max77823_read_word(fuelgauge->i2c, FULLCAP_NOM_REG);
	if (new_vffullcap < 0)
		new_vffullcap = fuelgauge->battery_data->Capacity;

	/* compare with initial capacity */
	if (new_vffullcap >
		(fuelgauge->battery_data->Capacity * 110 / 100)) {
		pr_debug("%s: [Case 1] capacity = 0x%04x, NewVfFullCap = 0x%04x\n",
			__func__, fuelgauge->battery_data->Capacity,
			new_vffullcap);

		new_vffullcap =
			(fuelgauge->battery_data->Capacity * 110) / 100;

		max77823_write_word(fuelgauge->i2c, DQACC_REG,
				    (u16)(new_vffullcap / 4));
		max77823_write_word(fuelgauge->i2c, DPACC_REG, (u16)0x3200);
	} else if (new_vffullcap <
		(fuelgauge->battery_data->Capacity * 50 / 100)) {
		pr_debug("%s: [Case 5] capacity = 0x%04x, NewVfFullCap = 0x%04x\n",
			__func__, fuelgauge->battery_data->Capacity,
			new_vffullcap);

		new_vffullcap =
			(fuelgauge->battery_data->Capacity * 50) / 100;

		max77823_write_word(fuelgauge->i2c, DQACC_REG,
				    (u16)(new_vffullcap / 4));
		max77823_write_word(fuelgauge->i2c, DPACC_REG,
				    (u16)0x3200);
	} else {
	/* compare with previous capacity */
		if (new_vffullcap >
			(fuelgauge->info.previous_vffullcap * 110 / 100)) {
			pr_debug("%s: [Case 2] previous_vffullcap = 0x%04x, NewVfFullCap = 0x%04x\n",
				__func__, fuelgauge->info.previous_vffullcap,
				new_vffullcap);

			new_vffullcap =
				(fuelgauge->info.previous_vffullcap * 110) /
				100;

			max77823_write_word(fuelgauge->i2c, DQACC_REG,
					    (u16)(new_vffullcap / 4));
			max77823_write_word(fuelgauge->i2c, DPACC_REG,
					    (u16)0x3200);
		} else if (new_vffullcap <
			(fuelgauge->info.previous_vffullcap * 90 / 100)) {
			pr_debug("%s: [Case 3] previous_vffullcap = 0x%04x, NewVfFullCap = 0x%04x\n",
				__func__, fuelgauge->info.previous_vffullcap,
				new_vffullcap);

			new_vffullcap =
				(fuelgauge->info.previous_vffullcap * 90) / 100;

			max77823_write_word(fuelgauge->i2c, DQACC_REG,
					    (u16)(new_vffullcap / 4));
			max77823_write_word(fuelgauge->i2c, DPACC_REG,
					    (u16)0x3200);
		} else {
			pr_debug("%s: [Case 4] previous_vffullcap = 0x%04x, NewVfFullCap = 0x%04x\n",
				__func__, fuelgauge->info.previous_vffullcap,
				new_vffullcap);
			is_vffullcap_changed = false;
		}
	}

	/* delay for register setting (dQacc, dPacc) */
	if (is_vffullcap_changed)
		msleep(300);

	fuelgauge->info.previous_vffullcap =
		max77823_read_word(fuelgauge->i2c, FULLCAP_NOM_REG);

	if (is_vffullcap_changed)
		pr_debug("%s : VfFullCap(0x%04x), dQacc(0x%04x), dPacc(0x%04x)\n",
			__func__,
			max77823_read_word(fuelgauge->i2c, FULLCAP_NOM_REG),
			max77823_read_word(fuelgauge->i2c, DQACC_REG),
			max77823_read_word(fuelgauge->i2c, DPACC_REG));

}

void fg_set_full_charged(struct max77823_fuelgauge_data *fuelgauge)
{
	pr_info("[FG_Set_Full] (B) FullCAP(%d), RemCAP(%d)\n",
		(max77823_read_word(fuelgauge->i2c, FULLCAPREP_REG)/2),
		(max77823_read_word(fuelgauge->i2c, REMCAP_REP_REG)/2));

	max77823_write_word(fuelgauge->i2c, FULLCAPREP_REG,
		(u16)max77823_read_word(fuelgauge->i2c, REMCAP_REP_REG));

	pr_info("[FG_Set_Full] (A) FullCAP(%d), RemCAP(%d)\n",
		(max77823_read_word(fuelgauge->i2c, FULLCAPREP_REG)/2),
		(max77823_read_word(fuelgauge->i2c, REMCAP_REP_REG)/2));
}

static void display_low_batt_comp_cnt(struct max77823_fuelgauge_data *fuelgauge)
{
	pr_info("Check Array(%s): [%d, %d], [%d, %d], ",
			fuelgauge->battery_data->type_str,
			fuelgauge->info.low_batt_comp_cnt[0][0],
			fuelgauge->info.low_batt_comp_cnt[0][1],
			fuelgauge->info.low_batt_comp_cnt[1][0],
			fuelgauge->info.low_batt_comp_cnt[1][1]);
	pr_info("[%d, %d], [%d, %d], [%d, %d]\n",
			fuelgauge->info.low_batt_comp_cnt[2][0],
			fuelgauge->info.low_batt_comp_cnt[2][1],
			fuelgauge->info.low_batt_comp_cnt[3][0],
			fuelgauge->info.low_batt_comp_cnt[3][1],
			fuelgauge->info.low_batt_comp_cnt[4][0],
			fuelgauge->info.low_batt_comp_cnt[4][1]);
}

static void add_low_batt_comp_cnt(struct max77823_fuelgauge_data *fuelgauge,
				int range, int level)
{
	int i;
	int j;

	/* Increase the requested count value, and reset others. */
	fuelgauge->info.low_batt_comp_cnt[range-1][level/2]++;

	for (i = 0; i < LOW_BATT_COMP_RANGE_NUM; i++) {
		for (j = 0; j < LOW_BATT_COMP_LEVEL_NUM; j++) {
			if (i == range-1 && j == level/2)
				continue;
			else
				fuelgauge->info.low_batt_comp_cnt[i][j] = 0;
		}
	}
}

void prevent_early_poweroff(struct max77823_fuelgauge_data *fuelgauge,
	int vcell, int *fg_soc)
{
	int soc = 0;
	int read_val;

	soc = fg_read_percent(fuelgauge, SOCREP_REG);

	/* No need to write REMCAP_REP in below normal cases */
	if (soc > POWER_OFF_SOC_HIGH_MARGIN || vcell > fuelgauge->battery_data->low_battery_comp_voltage)
		return;

	pr_info("%s: soc=%d, vcell=%d\n", __func__, soc, vcell);

	if (vcell > POWER_OFF_VOLTAGE_HIGH_MARGIN) {
		read_val = max77823_read_word(fuelgauge->i2c, FULLCAPREP_REG);
		/* FullCAP * 0.013 */
		max77823_write_word(fuelgauge->i2c, REMCAP_REP_REG,
		(u16)(read_val * 13 / 1000));
		msleep(200);
		*fg_soc = fg_read_percent(fuelgauge, SOCREP_REG);
		pr_info("%s: new soc=%d, vcell=%d\n", __func__, *fg_soc, vcell);
	}
}

void reset_low_batt_comp_cnt(struct max77823_fuelgauge_data *fuelgauge)
{
	memset(fuelgauge->info.low_batt_comp_cnt, 0,
		sizeof(fuelgauge->info.low_batt_comp_cnt));
}

static int check_low_batt_comp_condition(
	struct max77823_fuelgauge_data *fuelgauge,
	int *nLevel)
{
	int i;
	int j;
	int ret = 0;

	for (i = 0; i < LOW_BATT_COMP_RANGE_NUM; i++) {
		for (j = 0; j < LOW_BATT_COMP_LEVEL_NUM; j++) {
			if (fuelgauge->info.low_batt_comp_cnt[i][j] >=
				MAX_LOW_BATT_CHECK_CNT) {
				display_low_batt_comp_cnt(fuelgauge);
				ret = 1;
				*nLevel = j*2 + 1;
				break;
			}
		}
	}

	return ret;
}

static int get_low_batt_threshold(struct max77823_fuelgauge_data *fuelgauge,
				int range, int nCurrent, int level)
{
	int ret = 0;

	ret = fuelgauge->battery_data->low_battery_table[range][OFFSET] +
		(nCurrent *
		fuelgauge->battery_data->low_battery_table[range][SLOPE]);

	return ret;
}

int low_batt_compensation(struct max77823_fuelgauge_data *fuelgauge,
		int fg_soc, int fg_vcell, int fg_current)
{
	int fg_avg_current = 0;
	int fg_min_current = 0;
	int new_level = 0;
	int i, table_size;

	/* Not charging, Under low battery comp voltage */
	if (fg_vcell <= fuelgauge->battery_data->low_battery_comp_voltage) {
		fg_avg_current = fg_read_avg_current(fuelgauge) / 1000;
		fg_min_current = min(fg_avg_current, fg_current);

		table_size =
			sizeof(fuelgauge->battery_data->low_battery_table) /
			(sizeof(s16)*TABLE_MAX);

		for (i = 1; i < CURRENT_RANGE_MAX_NUM; i++) {
			if ((fg_min_current >= fuelgauge->battery_data->
				low_battery_table[i-1][RANGE]) &&
				(fg_min_current < fuelgauge->battery_data->
				low_battery_table[i][RANGE])) {
				if (fg_soc >= 10 && fg_vcell <
					get_low_batt_threshold(fuelgauge,
					i, fg_min_current, 1)) {
					add_low_batt_comp_cnt(
						fuelgauge, i, 1);
				} else {
					reset_low_batt_comp_cnt(fuelgauge);
				}
			}
		}

		if (check_low_batt_comp_condition(fuelgauge, &new_level)) {
			fg_low_batt_compensation(fuelgauge, new_level);
			reset_low_batt_comp_cnt(fuelgauge);

			/* Do not update soc right after
			 * low battery compensation
			 * to prevent from powering-off suddenly
			 */
			pr_info("%s: SOC is set to %d by low compensation!!\n",
				__func__, fg_read_percent(fuelgauge, SOCREP_REG));
		}
	}

	/* Prevent power off over 3500mV */
	prevent_early_poweroff(fuelgauge, fg_vcell, &fg_soc);

	return fg_soc;
}

static bool is_booted_in_low_battery(struct max77823_fuelgauge_data *fuelgauge)
{
	int fg_vcell = max77823_get_vcell(fuelgauge);
	int fg_current = fg_read_current(fuelgauge)/1000;
	int threshold = 0;

	threshold = 3300000 + (fg_current * 170);

	if (fg_vcell <= threshold)
		return true;
	else
		return false;
}

static bool fuelgauge_recovery_handler(struct max77823_fuelgauge_data *fuelgauge)
{
	int current_soc;
	int avsoc;
	int temperature;

	if (fuelgauge->info.soc >= LOW_BATTERY_SOC_REDUCE_UNIT) {
		pr_err("%s: Reduce the Reported SOC by 1%%\n",
			__func__);
		current_soc = fg_read_percent(fuelgauge, SOCREP_REG) / 10;

		if (current_soc) {
			pr_info("%s: Returning to Normal discharge path\n",
				__func__);
			pr_info("%s: Actual SOC(%d) non-zero\n",
				__func__, current_soc);
			fuelgauge->info.is_low_batt_alarm = false;
		} else {
			temperature = fg_read_temp(fuelgauge);
			avsoc = fg_read_percent(fuelgauge, SOCAV_REG);

			if ((fuelgauge->info.soc > avsoc) ||
				(temperature < 0)) {
				fuelgauge->info.soc -=
					LOW_BATTERY_SOC_REDUCE_UNIT;
				pr_err("%s: New Reduced RepSOC (%d)\n",
					__func__, fuelgauge->info.soc);
			} else
				pr_info("%s: Waiting for recovery (AvSOC:%d)\n",
					__func__, avsoc);
		}
	}

	return fuelgauge->info.is_low_batt_alarm;
}

static int get_fuelgauge_soc(struct max77823_fuelgauge_data *fuelgauge)
{
	union power_supply_propval value;
	int fg_soc = 0;
	int fg_vfsoc;
	int fg_vcell;
	int fg_current;
	int avg_current;
	ktime_t	current_time;
	struct timespec ts;
	int fullcap_check_interval;

	if (fuelgauge->info.is_low_batt_alarm)
		if (fuelgauge_recovery_handler(fuelgauge)) {
			fg_soc = fuelgauge->info.soc;
			goto return_soc;
		}

#if defined(ANDROID_ALARM_ACTIVATED)
	current_time = alarm_get_elapsed_realtime();
	ts = ktime_to_timespec(current_time);
#else
	current_time = ktime_get_boottime();
	ts = ktime_to_timespec(current_time);
#endif

	/* check fullcap range */
	fullcap_check_interval =
		(ts.tv_sec - fuelgauge->info.fullcap_check_interval);
	if (fullcap_check_interval >
		VFFULLCAP_CHECK_INTERVAL) {
		pr_debug("%s: check fullcap range (interval:%d)\n",
			__func__, fullcap_check_interval);
		fg_check_vf_fullcap_range(fuelgauge);
		fuelgauge->info.fullcap_check_interval = ts.tv_sec;
	}

	fg_soc = fg_read_percent(fuelgauge, SOCREP_REG);
	if (fg_soc < 0) {
		pr_info("Can't read soc!!!");
		fg_soc = fuelgauge->info.soc;
	}

	if (fuelgauge->info.low_batt_boot_flag) {
		fg_soc = 0;

		if (fuelgauge->pdata->check_cable_callback &&
		    fuelgauge->pdata->check_cable_callback() !=
			POWER_SUPPLY_TYPE_BATTERY &&
			!is_booted_in_low_battery(fuelgauge)) {
			fg_adjust_capacity(fuelgauge);
			fuelgauge->info.low_batt_boot_flag = 0;
		}

		if (fuelgauge->pdata->check_cable_callback &&
		    fuelgauge->pdata->check_cable_callback() ==
			POWER_SUPPLY_TYPE_BATTERY)
			fuelgauge->info.low_batt_boot_flag = 0;
	}

	fg_vcell = max77823_get_vcell(fuelgauge);
	fg_current = fg_read_current(fuelgauge)/1000;
	avg_current = fg_read_avg_current(fuelgauge)/1000;
	fg_vfsoc = fg_read_vfsoc(fuelgauge);

	value.intval = max77823_get_status(fuelgauge);

	/* Algorithm for reducing time to fully charged (from MAXIM) */
	if (value.intval != POWER_SUPPLY_STATUS_DISCHARGING &&
		value.intval != POWER_SUPPLY_STATUS_FULL &&
		fuelgauge->cable_type != POWER_SUPPLY_TYPE_USB &&
		/* Skip when first check after boot up */
		!fuelgauge->info.is_first_check &&
		(fg_vfsoc > VFSOC_FOR_FULLCAP_LEARNING &&
		(fg_current > LOW_CURRENT_FOR_FULLCAP_LEARNING &&
		fg_current < HIGH_CURRENT_FOR_FULLCAP_LEARNING) &&
		(avg_current > LOW_AVGCURRENT_FOR_FULLCAP_LEARNING &&
		avg_current < HIGH_AVGCURRENT_FOR_FULLCAP_LEARNING))) {

		if (fuelgauge->info.full_check_flag == 2) {
			pr_info("%s: force fully charged SOC !! (%d)",
				__func__, fuelgauge->info.full_check_flag);
			fg_set_full_charged(fuelgauge);
			fg_soc = fg_read_percent(fuelgauge, SOCREP_REG);
		} else if (fuelgauge->info.full_check_flag < 2)
			pr_info("%s: full_check_flag (%d)",
				__func__, fuelgauge->info.full_check_flag);

		/* prevent overflow */
		if (fuelgauge->info.full_check_flag++ > 10000)
			fuelgauge->info.full_check_flag = 3;
	} else
		fuelgauge->info.full_check_flag = 0;

	/*  Checks vcell level and tries to compensate SOC if needed.*/
	/*  If jig cable is connected, then skip low batt compensation check. */
	if (fuelgauge->pdata->check_jig_status &&
	    !fuelgauge->pdata->check_jig_status() &&
		value.intval == POWER_SUPPLY_STATUS_DISCHARGING)
		fg_soc = low_batt_compensation(
			fuelgauge, fg_soc, fg_vcell, fg_current);

	if (fuelgauge->info.is_first_check)
		fuelgauge->info.is_first_check = false;

	fuelgauge->info.soc = fg_soc;

return_soc:
	pr_debug("%s: soc(%d), low_batt_alarm(%d)\n",
		__func__, fuelgauge->info.soc,
		fuelgauge->info.is_low_batt_alarm);

	return fg_soc;
}

static void full_comp_work_handler(struct work_struct *work)
{
	struct max77823_fuelgauge_info *fg_info =
		container_of(work, struct max77823_fuelgauge_info, full_comp_work.work);
	struct max77823_fuelgauge_data *fuelgauge =
		container_of(fg_info, struct max77823_fuelgauge_data, info);
	int avg_current;
	union power_supply_propval value;

	avg_current = fg_read_avg_current(fuelgauge)/1000;
	psy_get_prop(fuelgauge, PS_BATT, POWER_SUPPLY_PROP_STATUS, &value);

	if (avg_current >= 25) {
		cancel_delayed_work(&fuelgauge->info.full_comp_work);
		schedule_delayed_work(&fuelgauge->info.full_comp_work, 100);
	} else {
		pr_info("%s: full charge compensation start (avg_current %d)\n",
			__func__, avg_current);
		fg_fullcharged_compensation(fuelgauge,
			(int)(value.intval ==
			POWER_SUPPLY_STATUS_FULL), false);
	}
}

static irqreturn_t max77823_jig_irq_thread(int irq, void *irq_data)
{
	struct max77823_fuelgauge_data *fuelgauge = irq_data;

	if (fuelgauge->pdata->check_jig_status &&
	    fuelgauge->pdata->check_jig_status())
		fg_reset_capacity_by_jig_connection(fuelgauge);
	else
		pr_info("%s: jig removed\n", __func__);
	return IRQ_HANDLED;
}

bool max77823_fg_init(struct max77823_fuelgauge_data *fuelgauge)
{
	ktime_t	current_time;
	struct timespec ts;
	int ret;
	int vempty;

#if defined(ANDROID_ALARM_ACTIVATED)
	current_time = alarm_get_elapsed_realtime();
	ts = ktime_to_timespec(current_time);
#else
	current_time = ktime_get_boottime();
	ts = ktime_to_timespec(current_time);
#endif

	fuelgauge->info.fullcap_check_interval = ts.tv_sec;

	fuelgauge->info.is_low_batt_alarm = false;
	fuelgauge->info.is_first_check = true;

	/* Init parameters to prevent wrong compensation. */
	fuelgauge->info.previous_fullcap =
		max77823_read_word(fuelgauge->i2c, FULLCAPREP_REG);
	fuelgauge->info.previous_vffullcap =
		max77823_read_word(fuelgauge->i2c, FULLCAP_NOM_REG);

	if (fuelgauge->pdata->check_cable_callback &&
	    (fuelgauge->pdata->check_cable_callback() !=
	     POWER_SUPPLY_TYPE_BATTERY) &&
	    is_booted_in_low_battery(fuelgauge))
		fuelgauge->info.low_batt_boot_flag = 1;

	if (fuelgauge->pdata->check_jig_status &&
	    fuelgauge->pdata->check_jig_status())
		fg_reset_capacity_by_jig_connection(fuelgauge);
	else {
		if (fuelgauge->pdata->jig_irq) {
			int ret;
			ret = request_threaded_irq(fuelgauge->pdata->jig_irq,
					NULL, max77823_jig_irq_thread,
					fuelgauge->pdata->jig_irq_attr,
					"jig-irq", fuelgauge);
			if (ret) {
				pr_info("%s: Failed to Request IRQ\n",
					__func__);
			}
		}
	}

	INIT_DELAYED_WORK(&fuelgauge->info.full_comp_work,
		full_comp_work_handler);

	max77823_write_word(fuelgauge->i2c, MAX77823_REG_CONFIG,
		(fuelgauge->pdata->thermal_source != SEC_BATTERY_THERMAL_SOURCE_FG) ? 0x2154 : 0x2254);

	max77823_write_word(fuelgauge->i2c, MAX77823_REG_TGAIN, (u16)fuelgauge->pdata->tgain);
	max77823_write_word(fuelgauge->i2c, MAX77823_REG_TOFF, (u16)fuelgauge->pdata->toff);
	max77823_write_word(fuelgauge->i2c, MAX77823_REG_TCURVE, (u16)fuelgauge->pdata->tcurve);

	ret = max77823_read_word(fuelgauge->i2c, MAX77823_REG_STATUS);
	if ((ret < 0) || !(ret & 2))
		return true;

	/* Power on reset initialization needed */
	fg_reset_capacity_by_jig_connection(fuelgauge);
	vempty = fuelgauge->pdata->empty_detect_voltage / 10;
	vempty <<= 7;
	vempty |= (fuelgauge->pdata->empty_recovery_voltage / 40) & 0x7f;
	max77823_write_word(fuelgauge->i2c, MAX77823_V_EMPTY, vempty);

	/* Clear POR condition */
	max77823_write_word(fuelgauge->i2c, MAX77823_REG_STATUS, ret & ~2);
	return true;
}

bool max77823_fg_fuelalert_init(struct max77823_fuelgauge_data *fuelgauge,
				int soc)
{
	if (fg_alert_init(fuelgauge, soc) >= 0)
		return true;
	else
		return false;
}

bool max77823_fg_is_fuelalerted(struct max77823_fuelgauge_data *fuelgauge)
{
	if (fg_check_status_reg(fuelgauge) > 0)
		return true;
	else
		return false;
}

bool max77823_fg_fuelalert_process(void *irq_data, bool is_fuel_alerted)
{
	struct max77823_fuelgauge_data *fuelgauge =
		(struct max77823_fuelgauge_data *)irq_data;
	union power_supply_propval value;
	int overcurrent_limit_in_soc;
	int current_soc = fg_read_percent(fuelgauge, SOCREP_REG);

	psy_get_prop(fuelgauge, PS_BATT, POWER_SUPPLY_PROP_STATUS, &value);
	if (value.intval == POWER_SUPPLY_STATUS_CHARGING)
		return true;

	if (fuelgauge->info.soc <= STABLE_LOW_BATTERY_DIFF)
		overcurrent_limit_in_soc = STABLE_LOW_BATTERY_DIFF_LOWBATT;
	else
		overcurrent_limit_in_soc = STABLE_LOW_BATTERY_DIFF;

	if (((int)fuelgauge->info.soc - current_soc) >
		overcurrent_limit_in_soc) {
		pr_info("%s: Abnormal Current Consumption jump by %d units\n",
			__func__, (((int)fuelgauge->info.soc - current_soc)));
		pr_info("%s: Last Reported SOC (%d).\n",
			__func__, fuelgauge->info.soc);

		fuelgauge->info.is_low_batt_alarm = true;

		if (fuelgauge->info.soc >=
			LOW_BATTERY_SOC_REDUCE_UNIT)
			return true;
	}

	if (value.intval ==
			POWER_SUPPLY_STATUS_DISCHARGING) {
		pr_err("Set battery level as 0, power off.\n");
		fuelgauge->info.soc = 0;
		value.intval = 0;
		psy_set_prop(fuelgauge, PS_BATT, POWER_SUPPLY_PROP_CAPACITY,
				&value);
	}

	return true;
}

bool max77823_fg_full_charged(struct max77823_fuelgauge_data *fuelgauge)
{
	union power_supply_propval value;

	psy_get_prop(fuelgauge, PS_BATT, POWER_SUPPLY_PROP_STATUS, &value);

	/* full charge compensation algorithm by MAXIM */
	fg_fullcharged_compensation(fuelgauge,
		(int)(value.intval == POWER_SUPPLY_STATUS_FULL), true);

	cancel_delayed_work(&fuelgauge->info.full_comp_work);
	schedule_delayed_work(&fuelgauge->info.full_comp_work, 100);

	return false;
}
#endif

bool max77823_fg_reset(struct max77823_fuelgauge_data *fuelgauge)
{
	if (!fg_reset_soc(fuelgauge))
		return true;
	else
		return false;
}

static void max77823_fg_get_scaled_capacity(
	struct max77823_fuelgauge_data *fuelgauge,
	union power_supply_propval *val)
{
	val->intval = (val->intval < fuelgauge->pdata->capacity_min) ?
		0 : ((val->intval - fuelgauge->pdata->capacity_min) * 1000 /
		     (fuelgauge->capacity_max - fuelgauge->pdata->capacity_min));

	pr_debug("%s: scaled capacity (%d.%d)\n",
		__func__, val->intval/10, val->intval%10);
}

/* capacity is integer */
static void max77823_fg_get_atomic_capacity(
	struct max77823_fuelgauge_data *fuelgauge,
	union power_supply_propval *val)
{
	if (fuelgauge->pdata->capacity_calculation_type &
		SEC_FUELGAUGE_CAPACITY_TYPE_ATOMIC) {
	if (fuelgauge->capacity_old < val->intval)
		val->intval = fuelgauge->capacity_old + 1;
	else if (fuelgauge->capacity_old > val->intval)
		val->intval = fuelgauge->capacity_old - 1;
	}

	/* keep SOC stable in abnormal status */
	if (fuelgauge->pdata->capacity_calculation_type &
		SEC_FUELGAUGE_CAPACITY_TYPE_SKIP_ABNORMAL) {
		pr_debug("%s:is_charging=%d cable_type=%d\n", __func__, fuelgauge->is_charging, fuelgauge->cable_type);
		if (!fuelgauge->is_charging &&
			fuelgauge->capacity_old < val->intval) {
			pr_err("%s: capacity (old %d : new %d)\n",
				__func__, fuelgauge->capacity_old, val->intval);
			val->intval = fuelgauge->capacity_old;
		}
	}

	/* updated old capacity */
	fuelgauge->capacity_old = val->intval;
}

static int max77823_fg_calculate_dynamic_scale(
	struct max77823_fuelgauge_data *fuelgauge)
{
	union power_supply_propval raw_soc_val;

#ifdef CONFIG_FUELGAUGE_MAX77823_VOLTAGE_TRACKING
	raw_soc_val.intval = max77823_get_soc(fuelgauge) / 10;
#else
	raw_soc_val.intval = fg_read_percent(fuelgauge, SOCREP_REG) / 10;
#endif

	if (raw_soc_val.intval <
		fuelgauge->pdata->capacity_max -
		fuelgauge->pdata->capacity_max_margin) {
		fuelgauge->capacity_max =
			fuelgauge->pdata->capacity_max -
			fuelgauge->pdata->capacity_max_margin;
		pr_debug("%s: capacity_max (%d)", __func__,
			 fuelgauge->capacity_max);
	} else {
		fuelgauge->capacity_max =
			(raw_soc_val.intval >
			fuelgauge->pdata->capacity_max +
			fuelgauge->pdata->capacity_max_margin) ?
			(fuelgauge->pdata->capacity_max +
			fuelgauge->pdata->capacity_max_margin) :
			raw_soc_val.intval;
		pr_debug("%s: raw soc (%d)", __func__,
			 fuelgauge->capacity_max);
	}

	fuelgauge->capacity_max =
		(fuelgauge->capacity_max * 99 / 100);

	/* update capacity_old for sec_fg_get_atomic_capacity algorithm */
	fuelgauge->capacity_old = 100;

	pr_info("%s: %d is used for capacity_max\n",
		__func__, fuelgauge->capacity_max);

	return fuelgauge->capacity_max;
}

static int max77823_fg_property_is_writeable(struct power_supply *psy,
                                                enum power_supply_property psp)
{
        switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
                return 1;
        default:
                break;
        }

        return 0;
}


#ifdef CONFIG_FUELGAUGE_MAX77823_VOLTAGE_TRACKING
static int max77823_fg_get_property(struct power_supply *psy,
			     enum power_supply_property psp,
			     union power_supply_propval *val)
{
	int ret;
	struct max77823_fuelgauge_data *fuelgauge =
		container_of(psy, struct max77823_fuelgauge_data, psy_fg);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
		ret = max77823_get_status(fuelgauge);
		if (ret >= 0)
			val->intval = ret;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		val->intval = 0;
		ret = max77823_read_word(fuelgauge->i2c, FULLCAP_REG);
		if (ret < 0)
			return ret;
		val->intval = ret * 1000 / 2;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = fuelgauge->cable_type;
		pr_info("%s:is_charging=%d cable_type=%d\n", __func__, fuelgauge->is_charging, fuelgauge->cable_type);
		break;
		/* Cell voltage (VCELL, mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = max77823_get_vcell(fuelgauge);
		break;
		/* Additional Voltage Information (uV) */
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		switch (val->intval) {
		case SEC_BATTEY_VOLTAGE_AVERAGE:
			val->intval = max77823_get_avgvcell(fuelgauge);
			break;
		case SEC_BATTEY_VOLTAGE_OCV:
			val->intval = max77823_get_vfocv(fuelgauge);
			break;
		}
		break;
		/* Current (mA) */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = 0;
		break;
		/* Average Current (mA) */
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		val->intval = 0;
		break;
		/* SOC (%) */
	case POWER_SUPPLY_PROP_CAPACITY:
		if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_RAW) {
			val->intval = max77823_get_soc(fuelgauge);
		} else {
			val->intval = max77823_get_soc(fuelgauge) / 10;

			if (fuelgauge->pdata->capacity_calculation_type &
			    (SEC_FUELGAUGE_CAPACITY_TYPE_SCALE |
			     SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE))
				max77823_fg_get_scaled_capacity(fuelgauge, val);

			/* capacity should be between 0% and 100%
			 * (0.1% degree)
			 */
			if (val->intval > 1000)
				val->intval = 1000;
			if (val->intval < 0)
				val->intval = 0;

			/* get only integer part */
			val->intval /= 10;

			/* check whether doing the wake_unlock */
			if ((val->intval > fuelgauge->pdata->fuel_alert_soc) &&
			    fuelgauge->is_fuel_alerted) {
				wake_unlock(&fuelgauge->fuel_alert_wake_lock);
				max77823_fg_fuelalert_init(fuelgauge,
					  fuelgauge->pdata->fuel_alert_soc);
			}

			/* (Only for atomic capacity)
			 * In initial time, capacity_old is 0.
			 * and in resume from sleep,
			 * capacity_old is too different from actual soc.
			 * should update capacity_old
			 * by val->intval in booting or resume.
			 */
			if (fuelgauge->initial_update_of_soc) {
				/* updated old capacity */
				fuelgauge->capacity_old = val->intval;
				fuelgauge->initial_update_of_soc = false;
				break;
			}

			if (fuelgauge->pdata->capacity_calculation_type &
			    (SEC_FUELGAUGE_CAPACITY_TYPE_ATOMIC |
			     SEC_FUELGAUGE_CAPACITY_TYPE_SKIP_ABNORMAL))
				max77823_fg_get_atomic_capacity(fuelgauge, val);
		}
		break;
		/* Battery Temperature */
	case POWER_SUPPLY_PROP_TEMP:
		/* Target Temperature */
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		val->intval = max77823_get_temperature(fuelgauge);
		break;
	default:
		return false;
	}
	return true;
}


static int max77823_fg_set_property(struct power_supply *psy,
			     enum power_supply_property psp,
			     const union power_supply_propval *val)
{
	struct max77823_fuelgauge_data *fuelgauge =
		container_of(psy, struct max77823_fuelgauge_data, psy_fg);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (val->intval == POWER_SUPPLY_STATUS_FULL)
			max77823_fg_full_charged(fuelgauge);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		if (val->intval == POWER_SUPPLY_TYPE_BATTERY) {
			if (fuelgauge->pdata->capacity_calculation_type &
			    SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE)
				max77823_fg_calculate_dynamic_scale(fuelgauge);
		}
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		fuelgauge->cable_type = val->intval;
		if (val->intval == POWER_SUPPLY_TYPE_BATTERY)
			fuelgauge->is_charging = false;
		else
			fuelgauge->is_charging = true;
		pr_info("%s:is_charging=%d cable_type=%d\n", __func__, fuelgauge->is_charging, fuelgauge->cable_type);
		break;
		/* Battery Temperature */
	case POWER_SUPPLY_PROP_CAPACITY:
		if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_RESET) {
			fuelgauge->initial_update_of_soc = true;
			if (!max77823_fg_reset(fuelgauge))
				return -EINVAL;
			else
				break;
		}
		/* Battery Temperature */
	case POWER_SUPPLY_PROP_TEMP:
		/* Target Temperature */
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		max77823_set_temperature(fuelgauge, val->intval);
		break;
	default:
		return false;
	}
	return true;
}
#endif

#ifdef CONFIG_FUELGAUGE_MAX77823_COULOMB_COUNTING
static int max77823_fg_get_property(struct power_supply *psy,
			     enum power_supply_property psp,
			     union power_supply_propval *val)
{
	int ret;
	struct max77823_fuelgauge_data *fuelgauge = psy->drv_data;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
		ret = max77823_get_status(fuelgauge);
		if (ret >= 0)
			val->intval = ret;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		val->intval = 0;
		ret = max77823_read_word(fuelgauge->i2c, FULLCAP_REG);
		if (ret < 0)
			return ret;
		val->intval = ret * 1000 / 2;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = fuelgauge->cable_type;
		pr_info("%s:is_charging=%d cable_type=%d\n", __func__, fuelgauge->is_charging, fuelgauge->cable_type);
		break;
		/* Cell voltage (VCELL, mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = max77823_get_vcell(fuelgauge);
		break;
		/* Additional Voltage Information (mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		switch (val->intval) {
		case SEC_BATTEY_VOLTAGE_OCV:
			val->intval = max77823_get_vfocv(fuelgauge);
			break;
		case SEC_BATTEY_VOLTAGE_AVERAGE:
		default:
			val->intval = max77823_get_avgvcell(fuelgauge);
			break;
		}
		break;
		/* Current */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = fg_read_current(fuelgauge);
		break;
		/* Average Current */
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		val->intval = fg_read_avg_current(fuelgauge);
		break;

	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
		val->intval = fg_read_power(fuelgauge, DESIGNCAP_REG);
		break;
	case POWER_SUPPLY_PROP_ENERGY_FULL:
		val->intval = fg_read_power(fuelgauge, FULLCAPREP_REG);
		break;
	case POWER_SUPPLY_PROP_ENERGY_EMPTY_DESIGN:
		val->intval = fg_read_power(fuelgauge, REMCAP_MIX_REG);	/* nothing to do with empty, stole for sec_battery */
		break;
	case POWER_SUPPLY_PROP_ENERGY_EMPTY:
		val->intval = 0;
		break;

	case POWER_SUPPLY_PROP_ENERGY_NOW:
		val->intval = fg_read_power(fuelgauge, REMCAP_REP_REG);
		break;
	case POWER_SUPPLY_PROP_ENERGY_AVG:
		val->intval = fg_read_power(fuelgauge, REMCAP_AV_REG);
		break;

		/* SOC (%) */
	case POWER_SUPPLY_PROP_CAPACITY:
		if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_RAW) {
			val->intval = fg_read_percent(fuelgauge, SOCREP_REG);
		} else {
			val->intval = get_fuelgauge_soc(fuelgauge);

			if (fuelgauge->pdata->capacity_calculation_type &
			    (SEC_FUELGAUGE_CAPACITY_TYPE_SCALE |
			     SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE))
				max77823_fg_get_scaled_capacity(fuelgauge, val);

			/* capacity should be between 0% and 100%
			 * (0.1% degree)
			 */
			if (val->intval > 1000)
				val->intval = 1000;
			if (val->intval < 0)
				val->intval = 0;

			/* get only integer part */
			val->intval /= 10;

			/* check whether doing the wake_unlock */
			if ((val->intval > fuelgauge->pdata->fuel_alert_soc) &&
			    fuelgauge->is_fuel_alerted) {
				wake_unlock(&fuelgauge->fuel_alert_wake_lock);
				max77823_fg_fuelalert_init(fuelgauge,
					  fuelgauge->pdata->fuel_alert_soc);
			}

			/* (Only for atomic capacity)
			 * In initial time, capacity_old is 0.
			 * and in resume from sleep,
			 * capacity_old is too different from actual soc.
			 * should update capacity_old
			 * by val->intval in booting or resume.
			 */
			if (fuelgauge->initial_update_of_soc) {
				/* updated old capacity */
				fuelgauge->capacity_old = val->intval;
				fuelgauge->initial_update_of_soc = false;
				break;
			}

			if (fuelgauge->pdata->capacity_calculation_type &
			    (SEC_FUELGAUGE_CAPACITY_TYPE_ATOMIC |
			     SEC_FUELGAUGE_CAPACITY_TYPE_SKIP_ABNORMAL))
				max77823_fg_get_atomic_capacity(fuelgauge, val);
		}
		break;
		/* Battery Temperature */
	case POWER_SUPPLY_PROP_TEMP:
		/* Target Temperature */
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		val->intval = fg_read_temp(fuelgauge);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int max77823_fg_set_property(struct power_supply *psy,
			     enum power_supply_property psp,
			     const union power_supply_propval *val)
{
	struct max77823_fuelgauge_data *fuelgauge = psy->drv_data;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (val->intval == POWER_SUPPLY_STATUS_FULL)
			max77823_fg_full_charged(fuelgauge);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		if (val->intval == POWER_SUPPLY_TYPE_BATTERY) {
			if (fuelgauge->pdata->capacity_calculation_type &
			    SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE)
				max77823_fg_calculate_dynamic_scale(fuelgauge);
		}
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		fuelgauge->cable_type = val->intval;
		if (val->intval == POWER_SUPPLY_TYPE_BATTERY) {
			fuelgauge->is_charging = false;
		} else {
			fuelgauge->is_charging = true;

			if (fuelgauge->info.is_low_batt_alarm) {
				pr_info("%s: Reset low_batt_alarm\n",
					 __func__);
				fuelgauge->info.is_low_batt_alarm = false;
			}

			reset_low_batt_comp_cnt(fuelgauge);
		}
		pr_info("%s:is_charging=%d cable_type=%d\n", __func__, fuelgauge->is_charging, fuelgauge->cable_type);
		break;
		/* Battery Temperature */
	case POWER_SUPPLY_PROP_CAPACITY:
		if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_RESET) {
			fuelgauge->initial_update_of_soc = true;
			if (!max77823_fg_reset(fuelgauge))
				return -EINVAL;
			else
				break;
		}
	case POWER_SUPPLY_PROP_TEMP:
		/* Target Temperature */
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		fg_write_temp(fuelgauge, val->intval);
		break;
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		fg_reset_capacity_by_jig_connection(fuelgauge);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}
#endif

static void max77823_fg_isr_work(struct work_struct *work)
{
	struct max77823_fuelgauge_data *fuelgauge =
		container_of(work, struct max77823_fuelgauge_data, isr_work.work);

	/* process for fuel gauge chip */
	max77823_fg_fuelalert_process(fuelgauge, fuelgauge->is_fuel_alerted);

	/* process for others */
	if (fuelgauge->pdata->fuelalert_process != NULL)
		fuelgauge->pdata->fuelalert_process(fuelgauge->is_fuel_alerted);
}

static irqreturn_t max77823_fg_irq_thread(int irq, void *irq_data)
{
	struct max77823_fuelgauge_data *fuelgauge = irq_data;
	bool fuel_alerted;

	if (fuelgauge->pdata->fuel_alert_soc >= 0) {
		fuel_alerted =
			max77823_fg_is_fuelalerted(fuelgauge);

		pr_info("%s: Fuel-alert %salerted!\n",
			__func__, fuel_alerted ? "" : "NOT ");

		fg_test_print(fuelgauge);

		if (fuel_alerted == fuelgauge->is_fuel_alerted) {
			if (!fuelgauge->pdata->repeated_fuelalert) {
				pr_debug("%s: Fuel-alert Repeated (%d)\n",
					__func__, fuelgauge->is_fuel_alerted);
				return IRQ_HANDLED;
			}
		}

		if (fuel_alerted)
			wake_lock(&fuelgauge->fuel_alert_wake_lock);
		else
			wake_unlock(&fuelgauge->fuel_alert_wake_lock);

		schedule_delayed_work(&fuelgauge->isr_work, 0);

		fuelgauge->is_fuel_alerted = fuel_alerted;
	}

	return IRQ_HANDLED;
}

static int max77823_fuelgauge_debugfs_show(struct seq_file *s, void *data)
{
	struct max77823_fuelgauge_data *fuelgauge = s->private;
	int reg = 0;
	u16 d[16];

	seq_printf(s, "MAX77823 FUELGAUGE IC :\n");
	seq_printf(s, "===================\n");
	while (reg < 0x100) {
		int i;
		int base = reg;
		for (i = 0; i < 0x10; i++)
			d[i] = max77823_read_word(fuelgauge->i2c, reg++);

		seq_printf(s, "%02x: %04x %04x %04x %04x %04x %04x %04x %04x  "
				    "%04x %04x %04x %04x %04x %04x %04x %04x\n",
			base, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7],
			d[8], d[9], d[10], d[11], d[12], d[13], d[14], d[15]);
		if (reg == 0x50)
			reg = 0xb0;
		else if (reg == 0xc0)
			reg = 0xd0;
		else if (reg == 0xe0)
			reg = 0xf0;
	}
	return 0;
}

static int max77823_fuelgauge_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, max77823_fuelgauge_debugfs_show, inode->i_private);
}

static const struct file_operations max77823_fuelgauge_debugfs_fops = {
	.open           = max77823_fuelgauge_debugfs_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

#ifdef CONFIG_OF
#define TFRAC_BITS 11
static int calc_temp(int ain, int toff, int tgain, int tcurve)
{
	int x1;
	int x = (toff << (TFRAC_BITS - 7)) + ((ain * tgain) >> (16 + 6 - TFRAC_BITS));
	s64 t64;
	int t;

	x1 = x - (20 << TFRAC_BITS);
	t64 = (tcurve * x1);
	t = (t64 * x1) >> (10 + TFRAC_BITS + TFRAC_BITS - TFRAC_BITS);
	if (x1 >= 0)
		t = x + t;
	else
		t = x - t;
	return t;
}

struct best_s {
	u64 sum;
	int tcurve;
	int toff;
	int tgain;
};

static u64 calc_error(int tcurve, int tgain, int toff, u32 *pairs, int length)
{
	u64 sum = 0;
	int i;

	/* calculate sum of error**2 */
	for (i = 0; i < length; i += 2) {
		int temp = pairs[i] << (TFRAC_BITS - 8);
		int ain = pairs[i + 1];
		int c = calc_temp(ain, toff, tgain, tcurve);
		int err = temp - c;
		u64 lerr;

		if (err < 0)
			err = -err;
		lerr = err;
		lerr = lerr * err;
		sum += lerr;
	}
//	pr_info("%s: tcurve=%d, tgain=%d, toff=%d, sum=%lld\n", __func__, tcurve, tgain, toff, sum);
	return sum;
}

static void get_best_toff(struct best_s *best, int tgain, u32 *pairs, int length)
{
	int toff1 = best->toff;
	int toff = toff1;
	int best_toff = best->toff;
	u64 best_sum = 0xffffffffffffffffL;
	int change = -32;

	while (1) {
		u64 sum = calc_error(best->tcurve, tgain, toff, pairs, length);

		if (best_sum >= sum) {
			best_sum = sum;
			best_toff = toff;
		} else {
			toff = best_toff;
			if (change == -1) {
				change = 16;
			} else {
				change >>= 1;
				if (!change)
					break;
			}
		}
		toff += change;
		if (change > 0) {
			if (toff >= 32768) {
				toff = 32767;
				change = toff - best_toff;
				if (!change)
					break;
			}
		} else {
			if (toff < -32768) {
				toff = -32768;
				change = toff - best_toff;
				if (!change)
					change = 16;
			}
		}
	}

	if (best->sum >= best_sum) {
		best->sum = best_sum;
		best->toff = best_toff;
		best->tgain = tgain;
		pr_info("%s: tcurve=%d, tgain=%d, toff=%d, sum=%lld\n",
			__func__, best->tcurve, tgain, best_toff, best_sum);
	}
}

static void get_best_tgain(struct best_s *best, u32 *pairs, int length)
{
	int tgain = best->tgain;
	int change = -32;

	while (1) {
		get_best_toff(best, tgain, pairs, length);

		if (tgain != best->tgain) {
			tgain = best->tgain;
			if (change == -1) {
				change = 16;
			} else {
				change >>= 1;
				if (!change)
					break;
			}
		}
		tgain += change;
		if (change > 0) {
			if (tgain >= 32768) {
				tgain = 32767;
				change = tgain - best->tgain;
				if (!change)
					break;
			}
		} else {
			if (tgain < -32768) {
				tgain = -32768;
				change = tgain - best->tgain;
				if (!change)
					change = 16;
			}
		}
	}
}

static void calibrate_temp(struct sec_battery_platform_data *pdata, u32 *pairs, int length)
{
	int i;
	struct best_s best;
	int tcurve = 0;
	int sx = 0;
	int sy = 0;
	s64 sxx = 0;
	s64 sxy = 0;
	s64 t;
	s64 s;
	int d;
	int len = length >> 1;

	for (i = 0; i < length; i += 2) {
		int y = pairs[i];
		int x = pairs[i + 1];

		y = (y << 8) / 10;
		sx += x;
		sy += y;
		sxx += x * x;
		sxy += x * y;
		pairs[i] = y;
	}
	t = sx;
	t *= sy;
	s = (sxy * len - t);		/* 24 fraction bits */
	t = sx;
	t *= sx;
	d = (sxx * len - t) >> 16;	/* 16 fraction bits */
	s = div64_s64(s, d); 		/* 8 fraction bits */
	best.tcurve = 0;
	best.tgain = s >> 2;	/* units 1/64, not 1/256*/
	/* toff is units 1/128 not 1/256 */
	best.toff = ((sy - (int)((best.tgain * (s64)sx) >> (6 + 16 - 8))) / len) >> 1;
	best.sum = calc_error(best.tcurve, best.tgain, best.toff, pairs, length);
	pr_info("%s: tcurve=%d, tgain=%d, toff=%d, sum=%lld\n",
		__func__, best.tcurve, best.tgain, best.toff, best.sum);

	for (tcurve = 0; tcurve < 256; tcurve++) {
		struct best_s b;

		b.tcurve = tcurve;
		b.tgain = best.tgain;
		b.toff = best.toff;
		b.sum = 0xffffffffffffffffL;
		get_best_tgain(&b, pairs, length);

		if (best.sum < b.sum)
			break;
		best = b;
		pr_info("%s: tcurve=%d, tgain=%d, toff=%d, sum=%lld\n",
			__func__, best.tcurve, best.tgain, best.toff, best.sum);
	}
	pdata->tcurve = best.tcurve;
	pdata->tgain = best.tgain;
	pdata->toff = best.toff;

	for (i = 0; i < length; i += 2) {
		unsigned temp = (pairs[i] * 10) >> 8;
		unsigned ain = pairs[i + 1];
		unsigned c = calc_temp(ain, best.toff, best.tgain, best.tcurve);

		pr_info("%s: %d, 0x%x, %d\n", __func__, temp, ain,
				(c * 10) >> TFRAC_BITS);
	}
}

struct dt_data {
	const char* const field;
	int	offset;
};

#define BOFFSET(field) offsetof(struct sec_battery_platform_data, field)
const struct dt_data dt_fields[] = {
	{"fuelgauge,capacity_max", BOFFSET(capacity_max) },
	{"fuelgauge,capacity_max_margin", BOFFSET(capacity_max_margin) },
	{"fuelgauge,capacity_min", BOFFSET(capacity_min) },
	{"fuelgauge,capacity_calculation_type", BOFFSET(capacity_calculation_type) },
	{"fuelgauge,fuel_alert_soc", BOFFSET(fuel_alert_soc) },
	{"empty_detect_voltage", BOFFSET(empty_detect_voltage) },
	{"empty_recovery_voltage", BOFFSET(empty_recovery_voltage) },
	{NULL, 0}
};
static int max77823_fuelgauge_parse_dt(
		struct max77823_fuelgauge_data *fuelgauge,
		struct device_node *np)
{
	struct sec_battery_platform_data *pdata = fuelgauge->pdata;
	int ret;
	const struct dt_data *dtd = dt_fields;
	struct property *prop;
	int length;
	u32 pairs[64];

	/* reset, irq gpio info */
	if (np == NULL) {
		pr_err("%s np NULL\n", __func__);
		return -1;
	}

	while (dtd->field) {
		u32 *p = (u32 *)(((void *)pdata) + dtd->offset);
		ret = of_property_read_u32(np, dtd->field, p);
		if (ret < 0)
			pr_err("%s: error reading %s(%d)\n", __func__,
					dtd->field, ret);
		else
			pr_info("%s: %s=%d\n", __func__, dtd->field, *p);
		dtd++;
	}
	pdata->repeated_fuelalert = of_property_read_bool(np,
			"fuelgauge,repeated_fuelalert");
	pr_info("%s: fg_irq: %d, repeated_fuelalert: %d\n",
			__func__, pdata->fg_irq,
			pdata->repeated_fuelalert);

	prop = of_find_property(np, "temp-disabled", &length);
	pdata->temp_disabled = prop ? 1 : 0;

	ret = of_property_read_u32_array(np, "temp-calibration", pairs, 3);
	if (ret >= 0) {
		pdata->tcurve = pairs[0];
		pdata->tgain = pairs[1];
		pdata->toff = pairs[2];
		pr_info("%s: tcurve:%d, tgain:%d toff:%d\n",
				__func__, pdata->tcurve, pdata->tgain, pdata->toff);
		return 0;
	}
	pr_err("%s: error reading temp-calibration %d\n", __func__, ret);

	prop = of_find_property(np, "temp-calibration-data", &length);
	if (!prop) {
		pr_err("%s: error reading temp-calibration-data\n", __func__);
		return 0;
	}
	length >>= 2;
	length &= ~1;
	if (length > ARRAY_SIZE(pairs))
		length = ARRAY_SIZE(pairs);
	ret = of_property_read_u32_array(np, "temp-calibration-data", pairs, length);
	if (ret < 0) {
		pr_err("%s: error reading temp-calibration-data %d\n", __func__, ret);
		return 0;
	}
	calibrate_temp(pdata, pairs, length);
	return 0;
}
#endif

const struct power_supply_desc psy_fg_desc = {
	.name = "max77823-fuelgauge",
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.properties = max77823_fuelgauge_props,
	.num_properties = ARRAY_SIZE(max77823_fuelgauge_props),
	.get_property = max77823_fg_get_property,
	.set_property = max77823_fg_set_property,
	.property_is_writeable  = max77823_fg_property_is_writeable,
};

struct power_supply_config psy_fg_config = {
};

static int max77823_fuelgauge_probe(struct platform_device *pdev)
{
	struct max77823_dev *max77823 = dev_get_drvdata(pdev->dev.parent);
	struct max77823_platform_data *pdata = dev_get_platdata(max77823->dev);
	struct max77823_fuelgauge_data *fuelgauge;
	int ret = 0;
	union power_supply_propval raw_soc_val;

	pr_info("%s: MAX77823 Fuelgauge Driver Loading\n", __func__);

	fuelgauge = kzalloc(sizeof(*fuelgauge), GFP_KERNEL);
	if (!fuelgauge)
		return -ENOMEM;

	pdata->fuelgauge_data = kzalloc(sizeof(sec_battery_platform_data_t), GFP_KERNEL);
	if (!pdata->fuelgauge_data) {
		kfree(fuelgauge);
		return -ENOMEM;
	}

	mutex_init(&fuelgauge->fg_lock);

	fuelgauge->dev = &pdev->dev;
	fuelgauge->pdata = pdata->fuelgauge_data;
	fuelgauge->i2c = max77823->fuelgauge;
	fuelgauge->max77823_pdata = pdata;

#if defined(CONFIG_OF)
	ret = max77823_fuelgauge_parse_dt(fuelgauge, pdev->dev.of_node);
	if (ret < 0) {
		pr_err("%s:dt error! ret[%d]\n", __func__, ret);
		goto err_free;
	}
#endif
	board_fuelgauge_init((void *)fuelgauge);

	platform_set_drvdata(pdev, fuelgauge);

	fuelgauge->capacity_max = fuelgauge->pdata->capacity_max;
#ifdef CONFIG_FUELGAUGE_MAX77823_VOLTAGE_TRACKING
	raw_soc_val.intval = max77823_get_soc(fuelgauge) / 10;
#else
	raw_soc_val.intval = fg_read_percent(fuelgauge, SOCREP_REG) / 10;
#endif

	if(raw_soc_val.intval > fuelgauge->pdata->capacity_max)
		max77823_fg_calculate_dynamic_scale(fuelgauge);

	(void) debugfs_create_file("max77823-fuelgauge-regs",
		S_IRUGO, NULL, (void *)fuelgauge, &max77823_fuelgauge_debugfs_fops);

	if (!max77823_fg_init(fuelgauge)) {
		pr_err("%s: Failed to Initialize Fuelgauge\n", __func__);
		goto err_free;
	}

	psy_fg_config.drv_data = fuelgauge;
	fuelgauge->psy_fg = power_supply_register(&pdev->dev, &psy_fg_desc, &psy_fg_config);
	if (IS_ERR(fuelgauge->psy_fg)) {
		pr_err("%s: Failed to Register psy_fg\n", __func__);
		ret = PTR_ERR(fuelgauge->psy_fg);
		goto err_free;
	}

	fuelgauge->fg_irq = pdata->irq_base + MAX77823_FG_IRQ_ALERT;
	pr_info("[%s]IRQ_BASE(%d) FG_IRQ(%d)\n",
		__func__, pdata->irq_base, fuelgauge->fg_irq);

	if (fuelgauge->fg_irq) {
		INIT_DELAYED_WORK(&fuelgauge->isr_work, max77823_fg_isr_work);

		ret = request_threaded_irq(fuelgauge->fg_irq,
				NULL, max77823_fg_irq_thread,
			        IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"fuelgauge-irq", fuelgauge);
		if (ret) {
			pr_err("%s: Failed to Request IRQ\n", __func__);
			goto err_supply_unreg;
		}
	}

	fuelgauge->is_fuel_alerted = false;
	if (fuelgauge->pdata->fuel_alert_soc >= 0) {
		if (max77823_fg_fuelalert_init(fuelgauge,
				       fuelgauge->pdata->fuel_alert_soc))
			wake_lock_init(&fuelgauge->fuel_alert_wake_lock,
				       WAKE_LOCK_SUSPEND, "fuel_alerted");
		else {
			pr_err("%s: Failed to Initialize Fuel-alert\n",
				__func__);
			ret = -ENODEV;
			goto err_irq;
		}
	}

	fuelgauge->initial_update_of_soc = true;

	pr_info("%s: MAX77823 Fuelgauge Driver Loaded\n", __func__);
	return 0;

err_irq:
	if (fuelgauge->fg_irq)
		free_irq(fuelgauge->fg_irq, fuelgauge);
err_supply_unreg:
	power_supply_unregister(fuelgauge->psy_fg);
err_free:
	mutex_destroy(&fuelgauge->fg_lock);
	kfree(pdata->fuelgauge_data);
	kfree(fuelgauge);

	return ret;
}

static int max77823_fuelgauge_remove(struct platform_device *pdev)
{
	struct max77823_fuelgauge_data *fuelgauge =
		platform_get_drvdata(pdev);

	if (fuelgauge->pdata->fuel_alert_soc >= 0)
		wake_lock_destroy(&fuelgauge->fuel_alert_wake_lock);

	return 0;
}

static int max77823_fuelgauge_suspend(struct device *dev)
{
	return 0;
}

static int max77823_fuelgauge_resume(struct device *dev)
{
	struct max77823_fuelgauge_data *fuelgauge = dev_get_drvdata(dev);

	fuelgauge->initial_update_of_soc = true;

	return 0;
}

static void max77823_fuelgauge_shutdown(struct device *dev)
{
}

#if defined(CONFIG_OF)
static struct of_device_id max77823_fuelgauge_dt_ids[] = {
	{ .compatible = "samsung,max77823-fuelgauge" },
	{ }
};
MODULE_DEVICE_TABLE(of, max77823_fuelgauge_dt_ids);
#endif /* CONFIG_OF */

static SIMPLE_DEV_PM_OPS(max77823_fuelgauge_pm_ops, max77823_fuelgauge_suspend,
			 max77823_fuelgauge_resume);

static struct platform_driver max77823_fuelgauge_driver = {
	.driver = {
		   .name = "max77823-fuelgauge",
		   .owner = THIS_MODULE,
#ifdef CONFIG_PM
		   .pm = &max77823_fuelgauge_pm_ops,
#endif
		.shutdown = max77823_fuelgauge_shutdown,
#if defined(CONFIG_OF)
		.of_match_table	= max77823_fuelgauge_dt_ids,
#endif /* CONFIG_OF */
		   },
	.probe	= max77823_fuelgauge_probe,
	.remove	= max77823_fuelgauge_remove,
};

static int __init max77823_fuelgauge_init(void)
{
	pr_info("%s: \n", __func__);
	return platform_driver_register(&max77823_fuelgauge_driver);
}

static void __exit max77823_fuelgauge_exit(void)
{
	platform_driver_unregister(&max77823_fuelgauge_driver);
}
module_init(max77823_fuelgauge_init);
module_exit(max77823_fuelgauge_exit);

MODULE_DESCRIPTION("Samsung MAX778023 Fuel Gauge Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:max77823-fuelgauge");
