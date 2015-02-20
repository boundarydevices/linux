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

extern void board_fuelgauge_init(void *data);

static enum power_supply_property max77823_fuelgauge_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_ENERGY_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TEMP_AMBIENT,
};

#ifdef CONFIG_FUELGAUGE_MAX77823_VOLTAGE_TRACKING
static void max77823_init_regs(struct max77823_fuelgauge_data *fuelgauge)
{
	u8 data[2];

	if (max77823_read_reg(fuelgauge->i2c, MAX77823_REG_FILTERCFG, data) < 0)
		return;

	/* Clear average vcell (12 sec) */
	data[0] &= 0x8f;

	max77823_write_reg(fuelgauge->i2c, MAX77823_REG_FILTERCFG, data);
}

static void max77823_get_version(struct max77823_fuelgauge_data *fuelgauge)
{
	u8 data[2];

	if (max77823_read_reg(fuelgauge->i2c, MAX77823_REG_VERSION, data) < 0)
		return;

	pr_debug("MAX77823 Fuel-Gauge Ver %d%d\n",
		 data[0], data[1]);
}

static void max77823_alert_init(struct max77823_fuelgauge_data *fuelgauge)
{
	u8 data[2];

	/* SALRT Threshold setting */
	data[0] = fuelgauge->pdata->fuel_alert_soc;
	data[1] = 0xff;
	max77823_write_reg(fuelgauge->i2c, MAX77823_REG_SALRT_TH, data);

	/* VALRT Threshold setting */
	data[0] = 0x00;
	data[1] = 0xff;
	max77823_write_reg(fuelgauge->i2c, MAX77823_REG_VALRT_TH, data);

	/* TALRT Threshold setting */
	data[0] = 0x80;
	data[1] = 0x7f;
	max77823_write_reg(fuelgauge->i2c, MAX77823_REG_TALRT_TH, data);
}

static bool max77823_check_status(struct max77823_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	bool ret = false;

	/* check if Smn was generated */
	if (max77823_read_reg(fuelgauge->i2c, MAX77823_REG_STATUS, data) < 0)
		return ret;

	pr_info("%s: status_reg(%02x%02x)\n",
		__func__, data[1], data[0]);

	/* minimum SOC threshold exceeded. */
	if (data[1] & (0x1 << 2))
		ret = true;

	/* clear status reg */
	if (!ret) {
		data[1] = 0;
		max77823_write_reg(fuelgauge->i2c, MAX77823_REG_STATUS, data);
		msleep(200);
	}

	return ret;
}

static int max77823_set_temperature(struct max77823_fuelgauge_data *fuelgauge,
				    int temperature)
{
	u8 data[2];

	data[0] = 0;
	data[1] = temperature;
	max77823_write_reg(fuelgauge->i2c, MAX77823_REG_TEMPERATURE, data);

	pr_debug("%s: temperature to (%d)\n", __func__, temperature);

	return temperature;
}

static int max77823_get_temperature(struct max77823_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	s32 temperature = 0;

	if (max77823_read_reg(fuelgauge->i2c,
			      MAX77823_REG_TEMPERATURE, data) < 0)
		return -ERANGE;

	/* data[] store 2's compliment format number */
	if (data[1] & (0x1 << 7)) {
		/* Negative */
		temperature = ((~(data[1])) & 0xFF) + 1;
		temperature *= (-1000);
	} else {
		temperature = data[1] & 0x7F;
		temperature *= 1000;
		temperature += data[0] * 39 / 10;
	}

	pr_debug("%s: temperature (%d)\n", __func__, temperature);

	return temperature;
}

/* soc should be 0.01% unit */
static int max77823_get_soc(struct max77823_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	int soc;

	if (max77823_read_reg(fuelgauge->i2c, MAX77823_REG_SOC_VF, data) < 0)
		return -EINVAL;

	soc = ((data[1] * 100) + (data[0] * 100 / 256));

	pr_debug("%s: raw capacity (%d)\n", __func__, soc);

	return min(soc, 10000);
}

static int max77823_get_vfocv(struct max77823_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u32 vfocv = 0;

	if (max77823_read_reg(fuelgauge->i2c, MAX77823_REG_VFOCV, data) < 0)
		return -EINVAL;

	vfocv = ((data[0] >> 3) + (data[1] << 5)) * 625 / 1000;

	pr_debug("%s: vfocv (%d)\n", __func__, vfocv);

	return vfocv;
}

static int max77823_get_vcell(struct max77823_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u32 vcell = 0;

	if (max77823_read_reg(fuelgauge->i2c, MAX77823_REG_VCELL, data) < 0)
		return -EINVAL;

	vcell = ((data[0] >> 3) + (data[1] << 5)) * 625 / 1000;

	pr_debug("%s: vcell (%d)\n", __func__, vcell);

	return vcell;
}

static int max77823_get_avgvcell(struct max77823_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u32 avgvcell = 0;

	if (max77823_read_reg(fuelgauge->i2c, MAX77823_REG_AVGVCELL, data) < 0)
		return -EINVAL;

	avgvcell = ((data[0] >> 3) + (data[1] << 5)) * 625 / 1000;

	pr_debug("%s: avgvcell (%d)\n", __func__, avgvcell);

	return avgvcell;
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
	u8 data[2];

	/* 1. Set max77823 alert configuration. */
	max77823_alert_init(fuelgauge);

	if (max77823_read_reg(fuelgauge->i2c, MAX77823_REG_CONFIG, data)
	    < 0)
		return -1;

	/*Enable Alert (Aen = 1) */
	data[0] |= (0x1 << 2);

	max77823_write_reg(fuelgauge, MAX77823_REG_CONFIG, data);

	pr_debug("%s: config_reg(%02x%02x) irq(%d)\n",
		 __func__, data[1], data[0], fuelgauge->pdata->fg_irq);

	return true;
}

bool max77823_fg_is_fuelalerted(struct max77823_fuelgauge_data *fuelgauge)
{
	return max77823_check_status(fuelgauge);
}

bool max77823_fg_fuelalert_process(void *irq_data, bool is_fuel_alerted)
{
	struct max77823_fuelgauge_data *fuelgauge = irq_data;
	u8 data[2];

	/* update SOC */
	/* max77823_get_soc(fuelgauge); */

	if (is_fuel_alerted) {
		if (max77823_read_reg(fuelgauge->i2c,
				      MAX77823_REG_CONFIG, data) < 0)
			return false;

		data[1] |= (0x1 << 3);

		max77823_write_reg(fuelgauge->i2c,
				   MAX77823_REG_CONFIG, data);

		pr_info("%s: Fuel-alert Alerted!! (%02x%02x)\n",
			__func__, data[1], data[0]);
	} else {
		if (max77823_read_reg(fuelgauge->i2c,
				      MAX77823_REG_CONFIG, data)
		    < 0)
			return false;

		data[1] &= (~(0x1 << 3));

		max77823_write_reg(fuelgauge->i2c,
				   MAX77823_REG_CONFIG, data);

		pr_info("%s: Fuel-alert Released!! (%02x%02x)\n",
			__func__, data[1], data[0]);
	}

	max77823_read_reg(fuelgauge->i2c, MAX77823_REG_VCELL, data);
	pr_debug("%s: MAX77823_REG_VCELL(%02x%02x)\n",
		 __func__, data[1], data[0]);

	max77823_read_reg(fuelgauge->i2c, MAX77823_REG_TEMPERATURE, data);
	pr_debug("%s: MAX77823_REG_TEMPERATURE(%02x%02x)\n",
		 __func__, data[1], data[0]);

	max77823_read_reg(fuelgauge->i2c, MAX77823_REG_CONFIG, data);
	pr_debug("%s: MAX77823_REG_CONFIG(%02x%02x)\n",
		 __func__, data[1], data[0]);

	max77823_read_reg(fuelgauge->i2c, MAX77823_REG_VFOCV, data);
	pr_debug("%s: MAX77823_REG_VFOCV(%02x%02x)\n",
		 __func__, data[1], data[0]);

	max77823_read_reg(fuelgauge->i2c, MAX77823_REG_SOC_VF, data);
	pr_debug("%s: MAX77823_REG_SOC_VF(%02x%02x)\n",
		 __func__, data[1], data[0]);

	pr_debug("%s: FUEL GAUGE IRQ (%d)\n",
		 __func__,
		 gpio_get_value(fuelgauge->pdata->fg_irq));

	return true;
}

bool max77823_fg_full_charged(struct max77823_fuelgauge_data *fuelgauge)
{
	return true;
}
#endif

#ifdef CONFIG_FUELGAUGE_MAX77823_COULOMB_COUNTING
static void fg_test_print(struct max77823_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u32 average_vcell;
	u16 w_data;
	u32 temp;
	u32 temp2;
	u16 reg_data;

	if (max77823_bulk_read(fuelgauge->i2c,
				AVR_VCELL_REG, 2, data) < 0) {
		pr_err("%s: Failed to read VCELL\n", __func__);
		return;
	}

	w_data = (data[1]<<8) | data[0];

	temp = (w_data & 0xFFF) * 78125;
	average_vcell = temp / 1000000;

	temp = ((w_data & 0xF000) >> 4) * 78125;
	temp2 = temp / 1000000;
	average_vcell += (temp2 << 4);

	pr_info("%s: AVG_VCELL(%d), data(0x%04x)\n", __func__,
		average_vcell, (data[1]<<8) | data[0]);

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

	reg_data = max77823_read_word(fuelgauge->i2c, CONFIG_REG);
	pr_info("%s: CONFIG_REG(0x%02x), data(0x%04x)\n", __func__,
		CONFIG_REG, reg_data);

}

static void fg_periodic_read(struct max77823_fuelgauge_data *fuelgauge)
{
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
}

static int fg_read_vcell(struct max77823_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u32 vcell;
	u16 w_data;
	u32 temp;
	u32 temp2;

	if (max77823_bulk_read(fuelgauge->i2c, VCELL_REG, 2, data) < 0) {
		pr_err("%s: Failed to read VCELL\n", __func__);
		return -1;
	}

	w_data = (data[1]<<8) | data[0];

	temp = (w_data & 0xFFF) * 78125;
	vcell = temp / 1000000;

	temp = ((w_data & 0xF000) >> 4) * 78125;
	temp2 = temp / 1000000;
	vcell += (temp2 << 4);

	if (!(fuelgauge->info.pr_cnt % PRINT_COUNT))
		pr_info("%s: VCELL(%d), data(0x%04x)\n",
			__func__, vcell, (data[1]<<8) | data[0]);

	return vcell;
}

static int fg_read_vfocv(struct max77823_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u32 vfocv = 0;
	u16 w_data;
	u32 temp;
	u32 temp2;

	if (max77823_bulk_read(fuelgauge->i2c, VFOCV_REG, 2, data) < 0) {
		pr_err("%s: Failed to read VFOCV\n", __func__);
		return -1;
	}

	w_data = (data[1]<<8) | data[0];

	temp = (w_data & 0xFFF) * 78125;
	vfocv = temp / 1000000;

	temp = ((w_data & 0xF000) >> 4) * 78125;
	temp2 = temp / 1000000;
	vfocv += (temp2 << 4);

	return vfocv;
}

static int fg_read_avg_vcell(struct max77823_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u32 avg_vcell = 0;
	u16 w_data;
	u32 temp;
	u32 temp2;

	if (max77823_bulk_read(fuelgauge->i2c, AVR_VCELL_REG,
			       2, data) < 0) {
		pr_err("%s: Failed to read AVG_VCELL\n", __func__);
		return -1;
	}

	w_data = (data[1]<<8) | data[0];

	temp = (w_data & 0xFFF) * 78125;
	avg_vcell = temp / 1000000;

	temp = ((w_data & 0xF000) >> 4) * 78125;
	temp2 = temp / 1000000;
	avg_vcell += (temp2 << 4);

	return avg_vcell;
}

static int fg_check_battery_present(struct max77823_fuelgauge_data *fuelgauge)
{
	u8 status_data[2];
	int ret = 1;

	/* 1. Check Bst bit */
	if (max77823_bulk_read(fuelgauge->i2c, STATUS_REG,
			       2, status_data) < 0) {
		pr_err("%s: Failed to read STATUS_REG\n", __func__);
		return 0;
	}

	if (status_data[0] & (0x1 << 3)) {
		pr_info("%s: addr(0x01), data(0x%04x)\n", __func__,
			(status_data[1]<<8) | status_data[0]);
		pr_info("%s: battery is absent!!\n", __func__);
		ret = 0;
	}

	return ret;
}

static int fg_write_temp(struct max77823_fuelgauge_data *fuelgauge,
			 int temperature)
{
	u8 data[2];

	data[0] = (temperature%10) * 1000 / 39;
	data[1] = temperature / 10;
	max77823_bulk_read(fuelgauge->i2c, TEMPERATURE_REG,
			   2, data);

	pr_debug("%s: temperature to (%d, 0x%02x%02x)\n",
		__func__, temperature, data[1], data[0]);

	return temperature;
}

static int fg_read_temp(struct max77823_fuelgauge_data *fuelgauge)
{
	u8 data[2] = {0, 0};
	int temper = 0;

	if (fg_check_battery_present(fuelgauge)) {
		if (max77823_bulk_read(fuelgauge->i2c,
				       TEMPERATURE_REG, 2, data) < 0) {
			pr_err("%s: Failed to read TEMPERATURE_REG\n",
				__func__);
			return -1;
		}

		if (data[1]&(0x1 << 7)) {
			temper = ((~(data[1]))&0xFF)+1;
			temper *= (-1000);
			temper -= ((~((int)data[0]))+1) * 39 / 10;
		} else {
			temper = data[1] & 0x7f;
			temper *= 1000;
			temper += data[0] * 39 / 10;
		}
	} else
		temper = 20000;

	if (!(fuelgauge->info.pr_cnt % PRINT_COUNT))
		pr_info("%s: TEMPERATURE(%d), data(0x%04x)\n",
			__func__, temper, (data[1]<<8) | data[0]);

	return temper/100;
}

/* soc should be 0.1% unit */
static int fg_read_vfsoc(struct max77823_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	int soc;

	if (max77823_bulk_read(fuelgauge->i2c, VFSOC_REG,
			       2, data) < 0) {
		pr_err("%s: Failed to read VFSOC\n", __func__);
		return -1;
	}

	soc = ((data[1] * 100) + (data[0] * 100 / 256)) / 10;

	return min(soc, 1000);
}

/* soc should be 0.1% unit */
static int fg_read_avsoc(struct max77823_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	int soc;

	if (max77823_bulk_read(fuelgauge->i2c, SOCAV_REG,
			       2, data) < 0) {
		pr_err("%s: Failed to read AVSOC\n", __func__);
		return -1;
	}

	soc = ((data[1] * 100) + (data[0] * 100 / 256)) / 10;

	return min(soc, 1000);
}

/* soc should be 0.1% unit */
static int fg_read_soc(struct max77823_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	int soc;

	if (max77823_bulk_read(fuelgauge->i2c, SOCREP_REG, 2, data) < 0) {
		pr_err("%s: Failed to read SOCREP\n", __func__);
		return -1;
	}

	soc = ((data[1] * 100) + (data[0] * 100 / 256)) / 10;

	pr_debug("%s: raw capacity (%d)\n", __func__, soc);

	if (!(fuelgauge->info.pr_cnt % PRINT_COUNT))
		pr_debug("%s: raw capacity (%d), data(0x%04x)\n",
			 __func__, soc, (data[1]<<8) | data[0]);

	return min(soc, 1000);
}

/* soc should be 0.01% unit */
static int fg_read_rawsoc(struct max77823_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	int soc;

	if (max77823_bulk_read(fuelgauge->i2c, SOCREP_REG,
			       2, data) < 0) {
		pr_err("%s: Failed to read SOCREP\n", __func__);
		return -1;
	}

	soc = (data[1] * 100) + (data[0] * 100 / 256);

	pr_debug("%s: raw capacity (0.01%%) (%d)\n",
		 __func__, soc);

	if (!(fuelgauge->info.pr_cnt % PRINT_COUNT))
		pr_debug("%s: raw capacity (%d), data(0x%04x)\n",
			 __func__, soc, (data[1]<<8) | data[0]);

	return min(soc, 10000);
}

static int fg_read_fullcap(struct max77823_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	int ret;

	if (max77823_bulk_read(fuelgauge->i2c, FULLCAPREP_REG,
			       2, data) < 0) {
		pr_err("%s: Failed to read FULLCAP\n", __func__);
		return -1;
	}

	ret = (data[1] << 8) + data[0];

	return ret;
}

static int fg_read_mixcap(struct max77823_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	int ret;

	if (max77823_bulk_read(fuelgauge->i2c, REMCAP_MIX_REG,
			       2, data) < 0) {
		pr_err("%s: Failed to read REMCAP_MIX_REG\n",
		       __func__);
		return -1;
	}

	ret = (data[1] << 8) + data[0];

	return ret;
}

static int fg_read_avcap(struct max77823_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	int ret;

	if (max77823_bulk_read(fuelgauge->i2c, REMCAP_AV_REG,
			       2, data) < 0) {
		pr_err("%s: Failed to read REMCAP_AV_REG\n",
		       __func__);
		return -1;
	}

	ret = (data[1] << 8) + data[0];

	return ret;
}

static int fg_read_repcap(struct max77823_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	int ret;

	if (max77823_bulk_read(fuelgauge->i2c, REMCAP_REP_REG,
			       2, data) < 0) {
		pr_err("%s: Failed to read REMCAP_REP_REG\n",
		       __func__);
		return -1;
	}

	ret = (data[1] << 8) + data[0];

	return ret;
}

static int fg_read_current(struct max77823_fuelgauge_data *fuelgauge, int unit)
{
	u8 data1[2], data2[2];
	u32 temp, sign;
	s32 i_current;
	s32 avg_current;

	if (max77823_bulk_read(fuelgauge->i2c, CURRENT_REG,
			      2, data1) < 0) {
		pr_err("%s: Failed to read CURRENT\n", __func__);
		return -1;
	}

	if (max77823_bulk_read(fuelgauge->i2c, AVG_CURRENT_REG,
			       2, data2) < 0) {
		pr_err("%s: Failed to read AVERAGE CURRENT\n", __func__);
		return -1;
	}

	temp = ((data1[1]<<8) | data1[0]) & 0xFFFF;
	if (temp & (0x1 << 15)) {
		sign = NEGATIVE;
		temp = (~temp & 0xFFFF) + 1;
	} else
		sign = POSITIVE;

	/* 1.5625uV/0.01Ohm(Rsense) = 156.25uA */
	switch (unit) {
	case SEC_BATTEY_CURRENT_UA:
		i_current = temp * 15625 / 100;
		break;
	case SEC_BATTEY_CURRENT_MA:
	default:
		i_current = temp * 15625 / 100000;
	}

	if (sign)
		i_current *= -1;

	temp = ((data2[1]<<8) | data2[0]) & 0xFFFF;
	if (temp & (0x1 << 15)) {
		sign = NEGATIVE;
		temp = (~temp & 0xFFFF) + 1;
	} else
		sign = POSITIVE;

	/* 1.5625uV/0.01Ohm(Rsense) = 156.25uA */
	avg_current = temp * 15625 / 100000;
	if (sign)
		avg_current *= -1;

	if (!(fuelgauge->info.pr_cnt++ % PRINT_COUNT)) {
		fg_test_print(fuelgauge);
		pr_info("%s: CURRENT(%dmA), AVG_CURRENT(%dmA)\n",
			__func__, i_current, avg_current);
		fuelgauge->info.pr_cnt = 1;
		/* Read max77823's all registers every 5 minute. */
		fg_periodic_read(fuelgauge);
	}

	return i_current;
}

static int fg_read_avg_current(struct max77823_fuelgauge_data *fuelgauge, int unit)
{
	u8  data2[2];
	u32 temp, sign;
	s32 avg_current;

	if (max77823_bulk_read(fuelgauge->i2c, AVG_CURRENT_REG,
			       2, data2) < 0) {
		pr_err("%s: Failed to read AVERAGE CURRENT\n",
		       __func__);
		return -1;
	}

	temp = ((data2[1]<<8) | data2[0]) & 0xFFFF;
	if (temp & (0x1 << 15)) {
		sign = NEGATIVE;
		temp = (~temp & 0xFFFF) + 1;
	} else
		sign = POSITIVE;

	/* 1.5625uV/0.01Ohm(Rsense) = 156.25uA */
	switch (unit) {
	case SEC_BATTEY_CURRENT_UA:
		avg_current = temp * 15625 / 100;
		break;
	case SEC_BATTEY_CURRENT_MA:
	default:
		avg_current = temp * 15625 / 100000;
	}

	if (sign)
		avg_current *= -1;

	return avg_current;
}

int fg_reset_soc(struct max77823_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	int vfocv, fullcap;

	/* delay for current stablization */
	msleep(500);

	pr_info("%s: Before quick-start - VCELL(%d), VFOCV(%d), VfSOC(%d), RepSOC(%d)\n",
		__func__, fg_read_vcell(fuelgauge), fg_read_vfocv(fuelgauge),
		fg_read_vfsoc(fuelgauge), fg_read_soc(fuelgauge));
	pr_info("%s: Before quick-start - current(%d), avg current(%d)\n",
		__func__, fg_read_current(fuelgauge, SEC_BATTEY_CURRENT_MA),
		fg_read_avg_current(fuelgauge, SEC_BATTEY_CURRENT_MA));

	if (fuelgauge->pdata->check_jig_status ||
	    !fuelgauge->pdata->check_jig_status()) {
		pr_info("%s : Return by No JIG_ON signal\n", __func__);
		return 0;
	}

	max77823_write_word(fuelgauge->i2c, CYCLES_REG, 0);

	if (max77823_bulk_read(fuelgauge->i2c, MISCCFG_REG,
			       2, data) < 0) {
		pr_err("%s: Failed to read MiscCFG\n", __func__);
		return -1;
	}

	data[1] |= (0x1 << 2);
	if (max77823_bulk_write(fuelgauge->i2c, MISCCFG_REG,
				2, data) < 0) {
		pr_err("%s: Failed to write MiscCFG\n", __func__);
		return -1;
	}

	msleep(250);
	max77823_write_word(fuelgauge->i2c, FULLCAPREP_REG,
			    fuelgauge->battery_data->Capacity);
	msleep(500);

	pr_info("%s: After quick-start - VCELL(%d), VFOCV(%d), VfSOC(%d), RepSOC(%d)\n",
		__func__, fg_read_vcell(fuelgauge), fg_read_vfocv(fuelgauge),
		fg_read_vfsoc(fuelgauge), fg_read_soc(fuelgauge));
	pr_info("%s: After quick-start - current(%d), avg current(%d)\n",
		__func__, fg_read_current(fuelgauge, SEC_BATTEY_CURRENT_MA),
		fg_read_avg_current(fuelgauge, SEC_BATTEY_CURRENT_MA));

	max77823_write_word(fuelgauge->i2c, CYCLES_REG, 0x00a0);

/* P8 is not turned off by Quickstart @3.4V
 * (It's not a problem, depend on mode data)
 * Power off for factory test(File system, etc..) */
	vfocv = fg_read_vfocv(fuelgauge);
	if (vfocv < POWER_OFF_VOLTAGE_LOW_MARGIN) {
		pr_info("%s: Power off condition(%d)\n", __func__, vfocv);

		fullcap = max77823_read_word(fuelgauge->i2c, FULLCAPREP_REG);

		/* FullCAP * 0.009 */
		max77823_write_word(fuelgauge->i2c, REMCAP_REP_REG,
				    (u16)(fullcap * 9 / 1000));
		msleep(200);
		pr_info("%s: new soc=%d, vfocv=%d\n", __func__,
			fg_read_soc(fuelgauge), vfocv);
	}

	pr_info("%s: Additional step - VfOCV(%d), VfSOC(%d), RepSOC(%d)\n",
		__func__, fg_read_vfocv(fuelgauge),
		fg_read_vfsoc(fuelgauge), fg_read_soc(fuelgauge));

	return 0;
}

int fg_reset_capacity_by_jig_connection(struct max77823_fuelgauge_data *fuelgauge)
{

	pr_info("%s: DesignCap = Capacity - 1 (Jig Connection)\n", __func__);

	return max77823_write_word(fuelgauge->i2c, DESIGNCAP_REG,
				   fuelgauge->battery_data->Capacity-1);
}

int fg_adjust_capacity(struct max77823_fuelgauge_data *fuelgauge)
{
	u8 data[2];

	data[0] = 0;
	data[1] = 0;

	/* 1. Write RemCapREP(05h)=0; */
	if (max77823_bulk_write(fuelgauge->i2c, REMCAP_REP_REG,
				2, data) < 0) {
		pr_err("%s: Failed to write RemCap_REP\n", __func__);
		return -1;
	}
	msleep(200);

	pr_info("%s: After adjust - RepSOC(%d)\n", __func__,
		fg_read_soc(fuelgauge));

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
	u8 status_data[2];
	int ret = 0;

	/* 1. Check Smn was generatedread */
	if (max77823_bulk_read(fuelgauge->i2c, STATUS_REG,
			       2, status_data) < 0) {
		pr_err("%s: Failed to read STATUS_REG\n", __func__);
		return -1;
	}
	pr_info("%s: addr(0x00), data(0x%04x)\n", __func__,
		(status_data[1]<<8) | status_data[0]);

	if (status_data[1] & (0x1 << 2))
		ret = 1;

	/* 2. clear Status reg */
	status_data[1] = 0;
	if (max77823_bulk_write(fuelgauge->i2c, STATUS_REG,
				2, status_data) < 0) {
		pr_info("%s: Failed to write STATUS_REG\n", __func__);
		return -1;
	}

	return ret;
}

int get_fuelgauge_value(struct max77823_fuelgauge_data *fuelgauge, int data)
{
	int ret;

	switch (data) {
	case FG_LEVEL:
		ret = fg_read_soc(fuelgauge);
		break;

	case FG_TEMPERATURE:
		ret = fg_read_temp(fuelgauge);
		break;

	case FG_VOLTAGE:
		ret = fg_read_vcell(fuelgauge);
		break;

	case FG_CURRENT:
		ret = fg_read_current(fuelgauge, SEC_BATTEY_CURRENT_MA);
		break;

	case FG_CURRENT_AVG:
		ret = fg_read_avg_current(fuelgauge, SEC_BATTEY_CURRENT_MA);
		break;

	case FG_CHECK_STATUS:
		ret = fg_check_status_reg(fuelgauge);
		break;

	case FG_RAW_SOC:
		ret = fg_read_rawsoc(fuelgauge);
		break;

	case FG_VF_SOC:
		ret = fg_read_vfsoc(fuelgauge);
		break;

	case FG_AV_SOC:
		ret = fg_read_avsoc(fuelgauge);
		break;

	case FG_FULLCAP:
		ret = fg_read_fullcap(fuelgauge);
		break;

	case FG_MIXCAP:
		ret = fg_read_mixcap(fuelgauge);
		break;

	case FG_AVCAP:
		ret = fg_read_avcap(fuelgauge);
		break;

	case FG_REPCAP:
		ret = fg_read_repcap(fuelgauge);
		break;

	default:
		ret = -1;
		break;
	}

	return ret;
}

int fg_alert_init(struct max77823_fuelgauge_data *fuelgauge, int soc)
{
	u8 misccgf_data[2];
	u8 salrt_data[2];
	u8 config_data[2];
	u8 valrt_data[2];
	u8 talrt_data[2];
	u16 read_data = 0;

	/* Using RepSOC */
	if (max77823_bulk_read(fuelgauge->i2c, MISCCFG_REG, 2,
			       misccgf_data) < 0) {
		pr_err("%s: Failed to read MISCCFG_REG\n", __func__);
		return -1;
	}
	misccgf_data[0] = misccgf_data[0] & ~(0x03);

	if (max77823_bulk_write(fuelgauge->i2c, MISCCFG_REG,
				2, misccgf_data) < 0) {
		pr_info("%s: Failed to write MISCCFG_REG\n", __func__);
		return -1;
	}

	/* SALRT Threshold setting */
	salrt_data[1] = 0xff;
	salrt_data[0] = soc;
	if (max77823_bulk_write(fuelgauge->i2c, SALRT_THRESHOLD_REG,
				2, salrt_data) < 0) {
		pr_info("%s: Failed to write SALRT_THRESHOLD_REG\n", __func__);
		return -1;
	}

	/* Reset VALRT Threshold setting (disable) */
	valrt_data[1] = 0xFF;
	valrt_data[0] = 0x00;
	if (max77823_bulk_write(fuelgauge->i2c, VALRT_THRESHOLD_REG,
				2, valrt_data) < 0) {
		pr_info("%s: Failed to write VALRT_THRESHOLD_REG\n", __func__);
		return -1;
	}

	read_data = max77823_read_word(fuelgauge->i2c, (u8)VALRT_THRESHOLD_REG);
	if (read_data != 0xff00)
		pr_err("%s: VALRT_THRESHOLD_REG is not valid (0x%x)\n",
			__func__, read_data);

	/* Reset TALRT Threshold setting (disable) */
	talrt_data[1] = 0x7F;
	talrt_data[0] = 0x80;
	if (max77823_bulk_write(fuelgauge->i2c, TALRT_THRESHOLD_REG,
				2, talrt_data) < 0) {
		pr_info("%s: Failed to write TALRT_THRESHOLD_REG\n", __func__);
		return -1;
	}

	read_data = max77823_read_word(fuelgauge->i2c, (u8)TALRT_THRESHOLD_REG);
	if (read_data != 0x7f80)
		pr_err("%s: TALRT_THRESHOLD_REG is not valid (0x%x)\n",
			__func__, read_data);

	/*mdelay(100);*/

	/* Enable SOC alerts */
	if (max77823_bulk_read(fuelgauge->i2c, CONFIG_REG,
			       2, config_data) < 0) {
		pr_err("%s: Failed to read CONFIG_REG\n", __func__);
		return -1;
	}
	config_data[0] = config_data[0] | (0x1 << 2);

	if (max77823_bulk_write(fuelgauge->i2c, CONFIG_REG,
				2, config_data) < 0) {
		pr_info("%s: Failed to write CONFIG_REG\n", __func__);
		return -1;
	}

	pr_info("[%s] SALRT(0x%x%x), VALRT(0x%x%x), CONFIG(0x%x%x)\n",
		__func__,
		salrt_data[1], salrt_data[0],
		valrt_data[1], valrt_data[0],
		config_data[1], config_data[0]);

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

	pr_info("%s: (A) FullCap = 0x%04x, RemCap = 0x%04x\n", __func__,
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
		pr_info("%s: [Case 1] capacity = 0x%04x, NewVfFullCap = 0x%04x\n",
			__func__, fuelgauge->battery_data->Capacity,
			new_vffullcap);

		new_vffullcap =
			(fuelgauge->battery_data->Capacity * 110) / 100;

		max77823_write_word(fuelgauge->i2c, DQACC_REG,
				    (u16)(new_vffullcap / 4));
		max77823_write_word(fuelgauge->i2c, DPACC_REG, (u16)0x3200);
	} else if (new_vffullcap <
		(fuelgauge->battery_data->Capacity * 50 / 100)) {
		pr_info("%s: [Case 5] capacity = 0x%04x, NewVfFullCap = 0x%04x\n",
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
			pr_info("%s: [Case 2] previous_vffullcap = 0x%04x, NewVfFullCap = 0x%04x\n",
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
			pr_info("%s: [Case 3] previous_vffullcap = 0x%04x, NewVfFullCap = 0x%04x\n",
				__func__, fuelgauge->info.previous_vffullcap,
				new_vffullcap);

			new_vffullcap =
				(fuelgauge->info.previous_vffullcap * 90) / 100;

			max77823_write_word(fuelgauge->i2c, DQACC_REG,
					    (u16)(new_vffullcap / 4));
			max77823_write_word(fuelgauge->i2c, DPACC_REG,
					    (u16)0x3200);
		} else {
			pr_info("%s: [Case 4] previous_vffullcap = 0x%04x, NewVfFullCap = 0x%04x\n",
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
		pr_info("%s : VfFullCap(0x%04x), dQacc(0x%04x), dPacc(0x%04x)\n",
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

	soc = fg_read_soc(fuelgauge);

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
		*fg_soc = fg_read_soc(fuelgauge);
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
		((nCurrent *
		fuelgauge->battery_data->low_battery_table[range][SLOPE]) /
		1000);

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
		fg_avg_current = fg_read_avg_current(fuelgauge,
			SEC_BATTEY_CURRENT_MA);
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
				__func__, fg_read_soc(fuelgauge));
		}
	}

	/* Prevent power off over 3500mV */
	prevent_early_poweroff(fuelgauge, fg_vcell, &fg_soc);

	return fg_soc;
}

static bool is_booted_in_low_battery(struct max77823_fuelgauge_data *fuelgauge)
{
	int fg_vcell = get_fuelgauge_value(fuelgauge, FG_VOLTAGE);
	int fg_current = get_fuelgauge_value(fuelgauge, FG_CURRENT);
	int threshold = 0;

	threshold = 3300 + ((fg_current * 17) / 100);

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
		current_soc =
			get_fuelgauge_value(fuelgauge, FG_LEVEL) / 10;

		if (current_soc) {
			pr_info("%s: Returning to Normal discharge path\n",
				__func__);
			pr_info("%s: Actual SOC(%d) non-zero\n",
				__func__, current_soc);
			fuelgauge->info.is_low_batt_alarm = false;
		} else {
			temperature =
				get_fuelgauge_value(fuelgauge, FG_TEMPERATURE);
			avsoc =
				get_fuelgauge_value(fuelgauge, FG_AV_SOC);

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
		pr_info("%s: check fullcap range (interval:%d)\n",
			__func__, fullcap_check_interval);
		fg_check_vf_fullcap_range(fuelgauge);
		fuelgauge->info.fullcap_check_interval = ts.tv_sec;
	}

	fg_soc = get_fuelgauge_value(fuelgauge, FG_LEVEL);
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

	fg_vcell = get_fuelgauge_value(fuelgauge, FG_VOLTAGE);
	fg_current = get_fuelgauge_value(fuelgauge, FG_CURRENT);
	avg_current = get_fuelgauge_value(fuelgauge, FG_CURRENT_AVG);
	fg_vfsoc = get_fuelgauge_value(fuelgauge, FG_VF_SOC);

	psy_do_property("battery", get,
		POWER_SUPPLY_PROP_STATUS, value);

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
			fg_soc = get_fuelgauge_value(fuelgauge, FG_LEVEL);
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

	avg_current = get_fuelgauge_value(fuelgauge, FG_CURRENT_AVG);
	psy_do_property("battery", get,
		POWER_SUPPLY_PROP_STATUS, value);

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
	u8 data[2] = {0, 0};

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

	/* NOT using FG for temperature */
	if (fuelgauge->pdata->thermal_source != SEC_BATTERY_THERMAL_SOURCE_FG) {
		data[0] = 0x00;
		data[1] = 0x21;
		max77823_bulk_write(fuelgauge->i2c, CONFIG_REG,
				    2, data);
	}

	return true;
}

bool max77823_fg_fuelalert_init(struct max77823_fuelgauge_data *fuelgauge,
				int soc)
{
	if (fg_alert_init(fuelgauge, soc) > 0)
		return true;
	else
		return false;
}

bool max77823_fg_is_fuelalerted(struct max77823_fuelgauge_data *fuelgauge)
{
	if (get_fuelgauge_value(fuelgauge, FG_CHECK_STATUS) > 0)
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
	int current_soc =
		get_fuelgauge_value(fuelgauge, FG_LEVEL);

	psy_do_property("battery", get,
		POWER_SUPPLY_PROP_STATUS, value);
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
		psy_do_property("battery", set,
			POWER_SUPPLY_PROP_CAPACITY, value);
	}

	return true;
}

bool max77823_fg_full_charged(struct max77823_fuelgauge_data *fuelgauge)
{
	union power_supply_propval value;

	psy_do_property("battery", get,
		POWER_SUPPLY_PROP_STATUS, value);

	/* full charge compensation algorithm by MAXIM */
	fg_fullcharged_compensation(fuelgauge,
		(int)(value.intval == POWER_SUPPLY_STATUS_FULL), true);

	cancel_delayed_work(&fuelgauge->info.full_comp_work);
	schedule_delayed_work(&fuelgauge->info.full_comp_work, 100);

	return false;
}

bool max77823_fg_reset(struct max77823_fuelgauge_data *fuelgauge)
{
	if (!fg_reset_soc(fuelgauge))
		return true;
	else
		return false;
}
#endif

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
		pr_info("%s:is_charging=%d cable_type=%d\n", __func__, fuelgauge->is_charging, fuelgauge->cable_type);
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
	raw_soc_val.intval = get_fuelgauge_value(fuelgauge,
						 FG_RAW_SOC) / 10;
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

#ifdef CONFIG_FUELGAUGE_MAX77823_VOLTAGE_TRACKING
static int max77823_fg_get_property(strcut power_supply *psy,
			     enum power_supply_property psp,
			     union power_supply_propval *val)
{
	struct max77823_fuelgauge_data *fuelgauge =
		container_of(psy, struct max77823_fuelgauge_data, psy_fg);

	switch (psp) {
		/* Cell voltage (VCELL, mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = max77823_get_vcell(fuelgauge);
		break;
		/* Additional Voltage Information (mV) */
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
	struct max77823_fuelgauge_data *fuelgauge =
		container_of(psy, struct max77823_fuelgauge_data, psy_fg);

	switch (psp) {
		/* Cell voltage (VCELL, mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = get_fuelgauge_value(fuelgauge, FG_VOLTAGE);
		break;
		/* Additional Voltage Information (mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		switch (val->intval) {
		case SEC_BATTEY_VOLTAGE_OCV:
			val->intval = fg_read_vfocv(fuelgauge);
			break;
		case SEC_BATTEY_VOLTAGE_AVERAGE:
		default:
			val->intval = fg_read_avg_vcell(fuelgauge);
			break;
		}
		break;
		/* Current */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		switch (val->intval) {
		case SEC_BATTEY_CURRENT_UA:
			val->intval =
				fg_read_current(fuelgauge,
						SEC_BATTEY_CURRENT_UA);
			break;
		case SEC_BATTEY_CURRENT_MA:
		default:
			val->intval = get_fuelgauge_value(fuelgauge,
							  FG_CURRENT);
			break;
		}
		break;
		/* Average Current */
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		switch (val->intval) {
		case SEC_BATTEY_CURRENT_UA:
			val->intval =
				fg_read_avg_current(fuelgauge,
						    SEC_BATTEY_CURRENT_UA);
			break;
		case SEC_BATTEY_CURRENT_MA:
		default:
			val->intval =
				get_fuelgauge_value(fuelgauge,
						    FG_CURRENT_AVG);
			break;
		}
		break;
		/* Full Capacity */
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		switch (val->intval) {
		case SEC_BATTEY_CAPACITY_DESIGNED:
			val->intval = get_fuelgauge_value(fuelgauge,
							  FG_FULLCAP);
			break;
		case SEC_BATTEY_CAPACITY_ABSOLUTE:
			val->intval = get_fuelgauge_value(fuelgauge,
							  FG_MIXCAP);
			break;
		case SEC_BATTEY_CAPACITY_TEMPERARY:
			val->intval = get_fuelgauge_value(fuelgauge,
							  FG_AVCAP);
			break;
		case SEC_BATTEY_CAPACITY_CURRENT:
			val->intval = get_fuelgauge_value(fuelgauge,
							  FG_REPCAP);
			break;
		}
		break;
		/* SOC (%) */
	case POWER_SUPPLY_PROP_CAPACITY:
		if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_RAW) {
			val->intval = get_fuelgauge_value(fuelgauge,
							  FG_RAW_SOC);
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
		val->intval = get_fuelgauge_value(fuelgauge,
						  FG_TEMPERATURE);
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
	u8 reg;
	u8 reg_data;

	seq_printf(s, "MAX77823 FUELGAUGE IC :\n");
	seq_printf(s, "===================\n");
	for (reg = 0xB0; reg <= 0xC3; reg++) {
		max77823_read_reg(fuelgauge->i2c, reg, &reg_data);
		seq_printf(s, "0x%02x:\t0x%02x\n", reg, reg_data);
	}

	seq_printf(s, "\n");
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
static int max77823_fuelgauge_parse_dt(struct max77823_fuelgauge_data *fuelgauge)
{
	struct device_node *np = of_find_node_by_name(NULL, "max77823-fuelgauge");
	sec_battery_platform_data_t *pdata = fuelgauge->pdata;
	int ret;

	/* reset, irq gpio info */
	if (np == NULL) {
		pr_err("%s np NULL\n", __func__);
		return -1;
	} else {
		ret = of_property_read_u32(np, "fuelgauge,capacity_max",
				&pdata->capacity_max);
		if (ret < 0)
			pr_err("%s error reading capacity_max %d\n", __func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,capacity_max_margin",
				&pdata->capacity_max_margin);
		if (ret < 0)
			pr_err("%s error reading capacity_max_margin %d\n", __func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,capacity_min",
				&pdata->capacity_min);
		if (ret < 0)
			pr_err("%s error reading capacity_min %d\n", __func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,capacity_calculation_type",
				&pdata->capacity_calculation_type);
		if (ret < 0)
			pr_err("%s error reading capacity_calculation_type %d\n",
					__func__, ret);
		ret = of_property_read_u32(np, "fuelgauge,fuel_alert_soc",
				&pdata->fuel_alert_soc);
		if (ret < 0)
			pr_err("%s error reading pdata->fuel_alert_soc %d\n",
					__func__, ret);
		pdata->repeated_fuelalert = of_property_read_bool(np,
				"fuelgauge,repeated_fuelalert");
		pr_info("%s: fg_irq: %d, capacity_max: %d, "
				"cpacity_max_margin: %d, capacity_min: %d, "
				"calculation_type: 0x%x, fuel_alert_soc: %d, "
				"repeated_fuelalert: %d\n", __func__, pdata->fg_irq,
				pdata->capacity_max, pdata->capacity_max_margin,
				pdata->capacity_min, pdata->capacity_calculation_type,
				pdata->fuel_alert_soc, pdata->repeated_fuelalert);
	}

	return 0;
}
#endif

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
	ret = max77823_fuelgauge_parse_dt(fuelgauge);
	if (ret < 0) {
		pr_err("%s not found charger dt! ret[%d]\n",
		       __func__, ret);
	}
#endif
	board_fuelgauge_init((void *)fuelgauge);

	platform_set_drvdata(pdev, fuelgauge);

	fuelgauge->psy_fg.name		= "max77823-fuelgauge";
	fuelgauge->psy_fg.type		= POWER_SUPPLY_TYPE_UNKNOWN;
	fuelgauge->psy_fg.get_property	= max77823_fg_get_property;
	fuelgauge->psy_fg.set_property	= max77823_fg_set_property;
	fuelgauge->psy_fg.properties	= max77823_fuelgauge_props;
	fuelgauge->psy_fg.num_properties =
		ARRAY_SIZE(max77823_fuelgauge_props);
	fuelgauge->capacity_max = fuelgauge->pdata->capacity_max;
#ifdef CONFIG_FUELGAUGE_MAX77823_VOLTAGE_TRACKING
	raw_soc_val.intval = max77823_get_soc(fuelgauge) / 10;
#else
	raw_soc_val.intval = get_fuelgauge_value(fuelgauge, FG_RAW_SOC) / 10;
#endif

	if(raw_soc_val.intval > fuelgauge->pdata->capacity_max)
		max77823_fg_calculate_dynamic_scale(fuelgauge);

	(void) debugfs_create_file("max77823-fuelgauge-regs",
		S_IRUGO, NULL, (void *)fuelgauge, &max77823_fuelgauge_debugfs_fops);

	if (!max77823_fg_init(fuelgauge)) {
		pr_err("%s: Failed to Initialize Fuelgauge\n", __func__);
		goto err_free;
	}

	ret = power_supply_register(&pdev->dev, &fuelgauge->psy_fg);
	if (ret) {
		pr_err("%s: Failed to Register psy_fg\n", __func__);
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
	power_supply_unregister(&fuelgauge->psy_fg);
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
