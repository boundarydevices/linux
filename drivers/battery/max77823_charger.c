/*
 *  max77823_charger.c
 *  Samsung MAX77823 Charger Driver
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
#include <linux/debugfs.h>
#include <linux/regulator/consumer.h>
#include <linux/seq_file.h>
//#include <linux/usb_notify.h>
#include <linux/battery/charger/max77823_charger.h>
#ifdef CONFIG_EXTCON_MAX77828
#include <linux/mfd/max77828-private.h>
#endif

#define ENABLE 1
#define DISABLE 0

#define SIOP_INPUT_LIMIT_CURRENT                1200
#define SIOP_CHARGING_LIMIT_CURRENT             1000
#define SIOP_WIRELESS_INPUT_LIMIT_CURRENT		660
#define SIOP_WIRELESS_CHARGING_LIMIT_CURRENT	780
#define SLOW_CHARGING_CURRENT_STANDARD          400
#define INPUT_CURRENT_1000mA                    0x1E

static enum power_supply_property max77823_charger_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
};
static enum power_supply_property max77823_otg_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static const char* const psy_names[] = {
[PS_BATT] = "battery",
[PS_PS] = "ps",
[PS_WIRELESS] = "wireless",
};

static int psy_get_prop(struct max77823_charger_data *charger, enum ps_id id, enum power_supply_property property, union power_supply_propval *value)
{
	struct power_supply *psy = charger->psy_ref[id];
	int ret = -EINVAL;

	value->intval = 0;
	if (!psy) {
		unsigned long timeout = jiffies + msecs_to_jiffies(500);
		do {
			psy = power_supply_get_by_name(psy_names[id]);
			if (psy) {
				charger->psy_ref[id] = psy;
				break;
			}
			if (time_after(jiffies, timeout)) {
				pr_err("%s: charger Failed %s\n",  __func__, psy_names[id]);
				return -ENODEV;
			}
			msleep(1);
		} while (1);
	}
	if (psy->get_property) {
		ret = psy->get_property(psy, property, value);
		if (ret < 0)
			pr_err("%s: charger Fail to get %s(%d=>%d)\n", __func__, psy_names[id], property, ret);
	}
	return ret;
}

static int psy_set_prop(struct max77823_charger_data *charger, enum ps_id id, enum power_supply_property property, union power_supply_propval *value)
{
	struct power_supply *psy = charger->psy_ref[id];
	int ret = -EINVAL;

	if (!psy) {
		unsigned long timeout = jiffies + msecs_to_jiffies(500);
		do {
			psy = power_supply_get_by_name(psy_names[id]);
			if (psy) {
				charger->psy_ref[id] = psy;
				break;
			}
			if (time_after(jiffies, timeout)) {
				pr_err("%s: charger Failed %s\n",  __func__, psy_names[id]);
				return -ENODEV;
			}
			msleep(1);
		} while (1);
	}
	if (psy->set_property) {
		ret = psy->set_property(psy, property, value);
		if (ret < 0)
			pr_err("%s: charger Fail to set %s(%d=>%d)\n", __func__, psy_names[id], property, ret);
	}
	return ret;
}

static struct sec_charging_current *get_charging_info(struct max77823_charger_data *charger, int index)
{
	struct sec_battery_platform_data *pdata = charger->pdata;
	if (index >= pdata->charging_current_entries) {
		pr_err("%s: invalid index %d\n", __func__, index);
		index = POWER_SUPPLY_TYPE_UNKNOWN;	/* 0 */
	}
	return &pdata->charging_current[index];
}

static void max77823_charger_initialize(struct max77823_charger_data *charger);
static int max77823_get_vbus_state(struct max77823_charger_data *charger);
static int max77823_get_charger_state(struct max77823_charger_data *charger);
static bool max77823_charger_unlock(struct max77823_charger_data *charger)
{
	struct i2c_client *i2c = charger->i2c;
	u8 reg_data;
	u8 chgprot;
	int retry_cnt = 0;
	bool need_init = false;

	do {
		max77823_read_reg(i2c, MAX77823_CHG_CNFG_06, &reg_data);
		chgprot = ((reg_data & 0x0C) >> 2);
		if (chgprot != 0x03) {
			pr_err("%s: unlock err, chgprot(0x%x), retry(%d)\n",
					__func__, chgprot, retry_cnt);
			max77823_write_reg(i2c, MAX77823_CHG_CNFG_06,
					   (0x03 << 2));
			need_init = true;
			msleep(20);
		} else {
			pr_debug("%s: unlock success, chgprot(0x%x)\n",
				__func__, chgprot);
			break;
		}
	} while ((chgprot != 0x03) && (++retry_cnt < 10));

	return need_init;
}

static void check_charger_unlock_state(struct max77823_charger_data *charger)
{
	bool need_reg_init;
	pr_debug("%s\n", __func__);

	need_reg_init = max77823_charger_unlock(charger);
	if (need_reg_init) {
		pr_err("%s: charger locked state, reg init\n", __func__);
		max77823_charger_initialize(charger);
	}
}

static void max77823_test_read(struct max77823_charger_data *charger)
{
	u8 data = 0;
	u32 addr = 0;
	for (addr = 0xB0; addr <= 0xC3; addr++) {
		max77823_read_reg(charger->i2c, addr, &data);
		pr_debug("MAX77823 addr : 0x%02x data : 0x%02x\n", addr, data);
	}
}

static int max77823_get_vbus_state(struct max77823_charger_data *charger)
{
	u8 reg_data;

	max77823_read_reg(charger->i2c,
			  MAX77823_CHG_DETAILS_00, &reg_data);

	if (charger->cable_type == POWER_SUPPLY_TYPE_WIRELESS)
		reg_data = ((reg_data & MAX77823_WCIN_DTLS) >>
			    MAX77823_WCIN_DTLS_SHIFT);
	else
		reg_data = ((reg_data & MAX77823_CHGIN_DTLS) >>
			    MAX77823_CHGIN_DTLS_SHIFT);

	switch (reg_data) {
	case 0x00:
		pr_debug("%s: VBUS is invalid. CHGIN < CHGIN_UVLO\n",
			__func__);
		break;
	case 0x01:
		pr_info("%s: VBUS is invalid. CHGIN < MBAT+CHGIN2SYS" \
			"and CHGIN > CHGIN_UVLO\n", __func__);
		break;
	case 0x02:
		pr_info("%s: VBUS is invalid. CHGIN > CHGIN_OVLO",
			__func__);
		break;
	case 0x03:
		pr_debug("%s: VBUS is valid. CHGIN < CHGIN_OVLO", __func__);
		break;
	default:
		break;
	}

	return reg_data;
}

static int max77823_get_charger_state(struct max77823_charger_data *charger)
{
	int status = POWER_SUPPLY_STATUS_UNKNOWN;
	u8 reg_data;

	max77823_read_reg(charger->i2c,
			  MAX77823_CHG_DETAILS_01, &reg_data);

	pr_info("%s : charger status (0x%02x)\n", __func__, reg_data);

	reg_data &= 0x0f;

	switch (reg_data)
	{
	case 0x00:
	case 0x01:
	case 0x02:
		status = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case 0x03:
	case 0x04:
		status = POWER_SUPPLY_STATUS_FULL;
		break;
	case 0x05:
	case 0x06:
	case 0x07:
		status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		break;
	case 0x08:
	case 0xA:
	case 0xB:
		status = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	default:
		status = POWER_SUPPLY_STATUS_UNKNOWN;
		break;
	}

	return (int)status;
}

static int max77823_get_charger_present(struct max77823_charger_data *charger)
{
	u8 reg_data;
	int ret;

	ret = max77823_read_reg(charger->i2c, MAX77823_CHG_INT_OK, &reg_data);
	if (ret < 0)
		return 1;
	pr_debug("%s:(0x%02x)\n", __func__, reg_data);
	return (reg_data >> MAX77823_DETBAT_SHIFT) & 1;
}

static int max77823_get_charging_health(struct max77823_charger_data *charger)
{
	int state;
	int vbus_state;
	int retry_cnt;
	u8 chg_dtls_00, chg_dtls, reg_data;
	u8 chg_cnfg_00, chg_cnfg_01 ,chg_cnfg_02, chg_cnfg_04, chg_cnfg_09, chg_cnfg_12;

	max77823_read_reg(charger->i2c,
			  MAX77823_CHG_DETAILS_01, &reg_data);
	reg_data = ((reg_data & MAX77823_BAT_DTLS) >> MAX77823_BAT_DTLS_SHIFT);

	pr_debug("%s: reg_data(0x%x)\n", __func__, reg_data);
	switch (reg_data) {
	case 0x00:
		pr_info("%s: No battery and the charger is suspended\n",
			__func__);
		state = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
		break;
	case 0x01:
		pr_info("%s: battery is okay "
			"but its voltage is low(~VPQLB)\n", __func__);
		state = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case 0x02:
		pr_info("%s: battery dead\n", __func__);
		state = POWER_SUPPLY_HEALTH_DEAD;
		break;
	case 0x03:
		state = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case 0x04:
		pr_info("%s: battery is okay " \
			"but its voltage is low\n", __func__);
		state = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case 0x05:
		pr_info("%s: battery ovp\n", __func__);
		state = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		break;
	default:
		pr_info("%s: battery unknown : 0x%d\n", __func__, reg_data);
		state = POWER_SUPPLY_HEALTH_UNKNOWN;
		break;
	}

	if (state == POWER_SUPPLY_HEALTH_GOOD) {
		/* VBUS OVP state return battery OVP state */
		vbus_state = max77823_get_vbus_state(charger);
		/* read CHG_DTLS and detecting battery terminal error */
		max77823_read_reg(charger->i2c,
				  MAX77823_CHG_DETAILS_01, &chg_dtls);
		chg_dtls = ((chg_dtls & MAX77823_CHG_DTLS) >>
			    MAX77823_CHG_DTLS_SHIFT);
		max77823_read_reg(charger->i2c,
				  MAX77823_CHG_CNFG_00, &chg_cnfg_00);

		/* print the log at the abnormal case */
		if((charger->is_charging == 1) && (chg_dtls & 0x08)) {
			max77823_read_reg(charger->i2c,
				MAX77823_CHG_DETAILS_00, &chg_dtls_00);
			max77823_read_reg(charger->i2c,
				MAX77823_CHG_CNFG_01, &chg_cnfg_01);
			max77823_read_reg(charger->i2c,
				MAX77823_CHG_CNFG_02, &chg_cnfg_02);
			max77823_read_reg(charger->i2c,
				MAX77823_CHG_CNFG_04, &chg_cnfg_04);
			max77823_read_reg(charger->i2c,
					MAX77823_CHG_CNFG_09, &chg_cnfg_09);
			max77823_read_reg(charger->i2c,
					MAX77823_CHG_CNFG_12, &chg_cnfg_12);

			pr_info("%s: CHG_DTLS_00(0x%x), CHG_DTLS_01(0x%x), CHG_CNFG_00(0x%x)\n",
				__func__, chg_dtls_00, chg_dtls, chg_cnfg_00);
			pr_info("%s:  CHG_CNFG_01(0x%x), CHG_CNFG_02(0x%x), CHG_CNFG_04(0x%x)\n",
				__func__, chg_cnfg_01, chg_cnfg_02, chg_cnfg_04);
			pr_info("%s:  CHG_CNFG_09(0x%x), CHG_CNFG_12(0x%x)\n",
				__func__, chg_cnfg_09, chg_cnfg_12);
		}

		pr_debug("%s: vbus_state : 0x%x, chg_dtls : 0x%x chg_cnfg_00=0x%x\n",
				__func__, vbus_state, chg_dtls, chg_cnfg_00);
		/*  OVP is higher priority */
		if (vbus_state == 0x02) { /*  CHGIN_OVLO */
			pr_info("%s: vbus ovp\n", __func__);
			state = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
			if (charger->cable_type == POWER_SUPPLY_TYPE_WIRELESS) {
				retry_cnt = 0;
				do {
					msleep(50);
					vbus_state = max77823_get_vbus_state(charger);
				} while((retry_cnt++ < 2) && (vbus_state == 0x02));
				if (vbus_state == 0x02) {
					state = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
					pr_info("%s: wpc and over-voltage\n", __func__);
				} else
					state = POWER_SUPPLY_HEALTH_GOOD;
			}
		} else if (((vbus_state == 0x0) || (vbus_state == 0x01)) &&(chg_dtls & 0x08) &&
//				(chg_cnfg_00 & MAX77823_MODE_BUCK) &&
//				(chg_cnfg_00 & MAX77823_MODE_CHGR) &&
				(charger->cable_type != POWER_SUPPLY_TYPE_WIRELESS)) {
			pr_info("%s: vbus is under\n", __func__);
			state = POWER_SUPPLY_HEALTH_UNDERVOLTAGE;
		}
	}

	max77823_test_read(charger);

	return (int)state;
}

static u8 max77823_get_float_voltage_data(int float_voltage)
{
	u8 data;

	if (float_voltage >= 4350)
		data = (float_voltage - 3650) / 25 + 1;
	else if (float_voltage == 4340)
		data = 0x1c;
	else
		data = (float_voltage - 3650) / 25;

	return data;
}

static int max77823_get_input_current(struct max77823_charger_data *charger)
{
	u8 reg_data;
	int get_current = 0;

	if (charger->cable_type == POWER_SUPPLY_TYPE_WIRELESS) {
		max77823_read_reg(charger->i2c,
			MAX77823_CHG_CNFG_10, &reg_data);
		/* AND operation for removing the formal 2bit  */
		reg_data = reg_data & 0x3F;

		if (reg_data <= 0x3)
			get_current = 60;
		else
			get_current = reg_data * 20;
	} else {
		max77823_read_reg(charger->i2c,
			MAX77823_CHG_CNFG_09, &reg_data);
		/* AND operation for removing the formal 1bit  */
		reg_data = reg_data & 0x7F;

		if (reg_data <= 0x3)
			get_current = 100;
		else if (reg_data >= 0x78)
			get_current = 4000;
		else {
			int quotient, remainder;
			quotient = reg_data / 3;
			remainder = reg_data % 3;
			if (remainder == 0)
				get_current = quotient * 100;
			else if (remainder == 1)
				get_current = quotient * 100 + 33;
			else
				get_current = quotient * 100 + 67;
		}
	}

	return get_current;
}

static void max77823_set_buck(struct max77823_charger_data *charger,
		int enable)
{
	u8 reg_data;

	max77823_read_reg(charger->i2c          ,
		MAX77823_CHG_CNFG_00, &reg_data);

	if (enable)
		reg_data |= MAX77823_MODE_BUCK;
	else
		reg_data &= ~MAX77823_MODE_BUCK;

	pr_info("%s: CHG_CNFG_00(0x%02x)\n", __func__, reg_data);
	max77823_write_reg(charger->i2c,
			   MAX77823_CHG_CNFG_00, reg_data);
}

static void max77823_chg_cable_work(struct work_struct *work)
{
	struct max77823_charger_data *charger =
	container_of(work, struct max77823_charger_data,chg_cable_work.work);
	int quotient, remainder;
	u8 reg_data;

	pr_info("[2][NOT HV TA]\n");

	max77823_read_reg(charger->i2c,
			  MAX77823_CHG_CNFG_09, &reg_data);

	pr_debug("[%s]VALUE(%d)DATA(0x%x)\n",
		__func__, charger->charging_current_max,
		reg_data);

	quotient = (charger->charging_current_max - 1000) / 100;
	remainder = (charger->charging_current_max - 1000) % 100;

	if (remainder >= 67)
		reg_data += ((quotient * 3) + 2);
	else if (remainder >= 33)
		reg_data += ((quotient * 3) + 1);
	else if (remainder < 33)
		reg_data += (quotient * 3);

	max77823_write_reg(charger->i2c,
			   MAX77823_CHG_CNFG_09, reg_data);

	pr_info("[%s]VALUE(%d)DATA(0x%x)\n",
		__func__, charger->charging_current_max,
		reg_data);
}

static void max77823_set_input_current(struct max77823_charger_data *charger,
				       int input_current)
{
	u8 reg_data = 0;
	int quotient, remainder;

	/* disable only buck because power onoff test issue */
	if (input_current <= 0) {
		max77823_write_reg(charger->i2c,
				   MAX77823_CHG_CNFG_09, 0x0F);
		max77823_set_buck(charger, DISABLE);
		return;
	}
	else
		max77823_set_buck(charger, ENABLE);

	if (charger->cable_type == POWER_SUPPLY_TYPE_MAINS) {
		reg_data |= INPUT_CURRENT_1000mA;
		max77823_write_reg(charger->i2c,
				   MAX77823_CHG_CNFG_09, reg_data);

		pr_info("[1][TA][0x%x]\n", reg_data);
		schedule_delayed_work(&charger->chg_cable_work, msecs_to_jiffies(2000));
	} else if (charger->cable_type == POWER_SUPPLY_TYPE_WIRELESS) {
		quotient = input_current / 20;
		reg_data |= quotient;
		max77823_write_reg(charger->i2c,
				   MAX77823_CHG_CNFG_10, reg_data);
	} else {
		/* HV TA, etc */
		cancel_delayed_work_sync(&charger->chg_cable_work);

		quotient = input_current / 100;
		remainder = input_current % 100;

		if (remainder >= 67)
			reg_data |= (quotient * 3) + 2;
		else if (remainder >= 33)
			reg_data |= (quotient * 3) + 1;
		else if (remainder < 33)
			reg_data |= quotient * 3;

		max77823_write_reg(charger->i2c,
				   MAX77823_CHG_CNFG_09, reg_data);

		pr_info("[3][HV TA, etc][0x%x]\n", reg_data);
	}
}

static void max77823_set_charge_current(struct max77823_charger_data *charger,
					int fast_charging_current)
{
	int curr_step = 50;
	u8 reg_data;

	max77823_read_reg(charger->i2c,
			  MAX77823_CHG_CNFG_02, &reg_data);
	reg_data &= ~MAX77823_CHG_CC;

	if (!fast_charging_current) {
		max77823_write_reg(charger->i2c,
				   MAX77823_CHG_CNFG_02, reg_data);
	} else {
		reg_data |= (fast_charging_current / curr_step);

		max77823_write_reg(charger->i2c,MAX77823_CHG_CNFG_02, reg_data);
	}
	pr_info("%s: reg_data(0x%02x), charging current(%d)\n",
		__func__, reg_data, fast_charging_current);

}

static void max77823_set_topoff_current(struct max77823_charger_data *charger,
					int termination_current,
					int termination_time)
{
	u8 reg_data;

	if (termination_current >= 350)
		reg_data = 0x07;
	else if (termination_current >= 300)
		reg_data = 0x06;
	else if (termination_current >= 250)
		reg_data = 0x05;
	else if (termination_current >= 200)
		reg_data = 0x04;
	else if (termination_current >= 175)
		reg_data = 0x03;
	else if (termination_current >= 150)
		reg_data = 0x02;
	else if (termination_current >= 125)
		reg_data = 0x01;
	else
		reg_data = 0x00;

	/* the unit of timeout is second*/
	termination_time = termination_time / 60;
	reg_data |= ((termination_time / 10) << 3);
	pr_info("%s: reg_data(0x%02x), topoff(%d)\n",
		__func__, reg_data, termination_current);

	max77823_write_reg(charger->i2c,
			   MAX77823_CHG_CNFG_03, reg_data);

}

static void max77823_set_charger_state(struct max77823_charger_data *charger,
	int enable)
{
	u8 reg_data;

	max77823_read_reg(charger->i2c, MAX77823_CHG_CNFG_00, &reg_data);

	if (enable)
		reg_data |= MAX77823_MODE_CHGR;
	else
		reg_data &= ~MAX77823_MODE_CHGR;

	pr_debug("%s : CHG_CNFG_00(0x%02x)\n", __func__, reg_data);

	max77823_write_reg(charger->i2c,MAX77823_CHG_CNFG_00, reg_data);
}

static void max77823_charger_function_control(
				struct max77823_charger_data *charger)
{
	const int usb_charging_current = get_charging_info(charger, POWER_SUPPLY_TYPE_USB)->fast_charging_current;
	int set_charging_current, set_charging_current_max;
	u8 chg_cnfg_00;

	pr_info("####%s####\n", __func__);

	if (charger->cable_type == POWER_SUPPLY_TYPE_BATTERY) {
		charger->is_charging = false;
		charger->aicl_on = false;
		set_charging_current = 0;
		set_charging_current_max =
			get_charging_info(charger, POWER_SUPPLY_TYPE_BATTERY)->input_current_limit;

		max77823_read_reg(charger->i2c,
				  MAX77823_CHG_CNFG_00, &chg_cnfg_00);
		chg_cnfg_00 &= ~(CHG_CNFG_00_CHG_MASK
				 | CHG_CNFG_00_OTG_MASK
				 | CHG_CNFG_00_BUCK_MASK
				 | CHG_CNFG_00_BOOST_MASK);

		set_charging_current_max =
			get_charging_info(charger, POWER_SUPPLY_TYPE_USB)->input_current_limit;
		charger->charging_current = set_charging_current;
		charger->charging_current_max = set_charging_current_max;

		max77823_write_reg(charger->i2c,
			MAX77823_CHG_CNFG_00, chg_cnfg_00);
	} else {
		charger->is_charging = true;
		charger->charging_current_max =
			get_charging_info(charger, charger->cable_type)->input_current_limit;
		charger->charging_current =
			get_charging_info(charger, charger->cable_type)->fast_charging_current;
		/* decrease the charging current according to siop level */
		set_charging_current =
			charger->charging_current * charger->siop_level / 100;
		if (set_charging_current > 0 &&
		    set_charging_current < usb_charging_current)
			set_charging_current = usb_charging_current;

		set_charging_current_max =
			charger->charging_current_max;

		if (charger->siop_level < 100) {
			if (charger->cable_type == POWER_SUPPLY_TYPE_WIRELESS) {
				if (set_charging_current_max > SIOP_WIRELESS_INPUT_LIMIT_CURRENT) {
					set_charging_current_max = SIOP_WIRELESS_INPUT_LIMIT_CURRENT;
					if (set_charging_current > SIOP_WIRELESS_CHARGING_LIMIT_CURRENT)
						set_charging_current = SIOP_WIRELESS_CHARGING_LIMIT_CURRENT;
				}
			} else {
				if (set_charging_current_max > SIOP_INPUT_LIMIT_CURRENT) {
					set_charging_current_max = SIOP_INPUT_LIMIT_CURRENT;
					if (set_charging_current > SIOP_CHARGING_LIMIT_CURRENT)
						set_charging_current = SIOP_CHARGING_LIMIT_CURRENT;
				}
			}
		}
	}

	max77823_set_charger_state(charger, charger->is_charging);

	/* if battery full, only disable charging  */
	if ((charger->status == POWER_SUPPLY_STATUS_CHARGING) ||
	    (charger->status == POWER_SUPPLY_STATUS_FULL) ||
	    (charger->status == POWER_SUPPLY_STATUS_DISCHARGING)) {
		/* current setting */
		max77823_set_charge_current(charger,
					    set_charging_current);
		/* if battery is removed, disable input current and reenable input current
		 *  to enable buck always */
		max77823_set_input_current(charger,
					   set_charging_current_max);
		max77823_set_topoff_current(charger,
					    get_charging_info(charger, charger->cable_type)->full_check_current_1st,
					    get_charging_info(charger, charger->cable_type)->full_check_current_2nd);
	}

	pr_info("charging = %d, fc = %d, il = %d, t1 = %d, t2 = %d, cable = %d\n",
		charger->is_charging,
		charger->charging_current,
		charger->charging_current_max,
		get_charging_info(charger, charger->cable_type)->full_check_current_1st,
		get_charging_info(charger, charger->cable_type)->full_check_current_2nd,
		charger->cable_type);

	max77823_test_read(charger);

}

static void max77823_charger_initialize(struct max77823_charger_data *charger)
{
	u8 reg_data;
	pr_info("%s\n", __func__);

	/* unmasked: CHGIN_I, WCIN_I, BATP_I, BYP_I	*/
	max77823_write_reg(charger->i2c, MAX77823_CHG_INT_MASK, 0x9a);

	/* unlock charger setting protect */
	reg_data = (0x03 << 2);
	max77823_write_reg(charger->i2c, MAX77823_CHG_CNFG_06, reg_data);

	/*
	 * fast charge timer disable
	 * restart threshold disable
	 * pre-qual charge enable(default)
	 */
	reg_data = (0x08 << 0) | (0x03 << 4);
	max77823_write_reg(charger->i2c, MAX77823_CHG_CNFG_01, reg_data);

	/*
	 * charge current 466mA(default)
	 * otg current limit 900mA
	 */
	max77823_read_reg(charger->i2c, MAX77823_CHG_CNFG_02, &reg_data);
	reg_data |= (1 << 6);
	max77823_write_reg(charger->i2c, MAX77823_CHG_CNFG_02, reg_data);

	/*
	 * top off current 100mA
	 * top off timer 70min
	 */
	reg_data = 0x38;
	max77823_write_reg(charger->i2c, MAX77823_CHG_CNFG_03, reg_data);

	/*
	 * cv voltage 4.2V or 4.35V
	 * MINVSYS 3.6V(default)
	 */
	reg_data = max77823_get_float_voltage_data(charger->pdata->chg_float_voltage);
	max77823_update_reg(charger->i2c, MAX77823_CHG_CNFG_04,
			(reg_data << CHG_CNFG_04_CHG_CV_PRM_SHIFT),
			CHG_CNFG_04_CHG_CV_PRM_MASK);
	max77823_read_reg(charger->i2c, MAX77823_CHG_CNFG_04, &reg_data);
	pr_info("%s: battery cv voltage 0x%x\n", __func__, reg_data);

	max77823_test_read(charger);

}

static int max77823_chg_get_property(struct power_supply *psy,
			      enum power_supply_property psp,
			      union power_supply_propval *val)
{
	struct max77823_charger_data *charger =
		container_of(psy, struct max77823_charger_data, psy_chg);
	u8 reg_data;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = POWER_SUPPLY_TYPE_BATTERY;
		if (max77823_read_reg(charger->i2c,
			MAX77823_CHG_INT_OK, &reg_data) == 0) {
			if (reg_data & MAX77823_WCIN_OK) {
				val->intval = POWER_SUPPLY_TYPE_WIRELESS;
				charger->wc_w_state = 1;
			} else if (reg_data & MAX77823_CHGIN_OK) {
				val->intval = POWER_SUPPLY_TYPE_MAINS;
			}
		}
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = max77823_get_charger_state(charger);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = max77823_get_charger_present(charger);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		if (!charger->is_charging)
			val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
		else if (charger->aicl_on)
		{
			val->intval = POWER_SUPPLY_CHARGE_TYPE_SLOW;
			pr_info("%s: slow-charging mode\n", __func__);
		}
		else
			val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = max77823_get_charging_health(charger);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = 0;
		if (max77823_read_reg(charger->i2c, MAX77823_CHG_CNFG_12, &reg_data) == 0)
			val->intval = (reg_data >> 5) & 1;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		val->intval = max77823_get_input_current(charger);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = max77823_get_input_current(charger);
		pr_debug("%s : set-current(%dmA), current now(%dmA)\n",
			__func__, charger->charging_current, val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int max77823_chg_property_is_writeable(struct power_supply *psy,
                                                enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
	case POWER_SUPPLY_PROP_CURRENT_MAX:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		return 1;
	default:
		break;
	}
	return 0;
}

static int max77823_otg_regulator_nb(struct notifier_block *nb, unsigned long event, void *data)
{
	struct max77823_charger_data *charger = container_of(nb, struct max77823_charger_data, otg_regulator_nb);

	if (event & (REGULATOR_EVENT_DISABLE | REGULATOR_EVENT_ENABLE | REGULATOR_EVENT_PRE_ENABLE)) {
		max77823_update_reg(charger->i2c, MAX77823_CHG_CNFG_12,
			(event & REGULATOR_EVENT_DISABLE) ? 0x20 : 0, 0x20);
		if (event & REGULATOR_EVENT_DISABLE)
			max77823_update_reg(charger->i2c, MAX77823_CHG_CNFG_00,
				CHG_CNFG_00_CHG_MASK, CHG_CNFG_00_CHG_MASK);
	}
	return 0;
}

static void max77823_set_online(struct max77823_charger_data *charger, int type)
{
	union power_supply_propval value;

	if (type == POWER_SUPPLY_TYPE_POWER_SHARING) {
		psy_get_prop(charger, PS_PS, POWER_SUPPLY_PROP_STATUS, &value);
		max77823_update_reg(charger->i2c, MAX77823_CHG_CNFG_00,
				(value.intval) ? CHG_CNFG_00_OTG_CTRL : 0, CHG_CNFG_00_OTG_CTRL);
		return;
	}
	charger->cable_type = type;
	max77823_charger_function_control(charger);
}

static int max77823_chg_set_property(struct power_supply *psy,
			  enum power_supply_property psp,
			  const union power_supply_propval *val)
{
	struct max77823_charger_data *charger =
		container_of(psy, struct max77823_charger_data, psy_chg);
	int set_charging_current_max;
	const int usb_charging_current = get_charging_info(charger, POWER_SUPPLY_TYPE_USB)->fast_charging_current;

	switch (psp) {
	/* val->intval : type */
	case POWER_SUPPLY_PROP_STATUS:
		charger->status = val->intval;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		max77823_set_online(charger, val->intval);
		break;

	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		max77823_update_reg(charger->i2c, MAX77823_CHG_CNFG_12,
			val->intval ? 0x20 : 0, 0x20);
		break;

	/* val->intval : input charging current */
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		charger->charging_current_max = val->intval;
		break;
	/*  val->intval : charging current */
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		charger->charging_current = val->intval;
		break;
	/* val->intval : charging current */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		charger->charging_current = val->intval;
		max77823_set_charge_current(charger, val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		charger->siop_level = val->intval;
		if (charger->is_charging) {
			/* decrease the charging current according to siop level */
			int current_now =
				charger->charging_current * val->intval / 100;

			/* do forced set charging current */
			if (current_now > 0 &&
					current_now < usb_charging_current)
				current_now = usb_charging_current;

			if (charger->cable_type == POWER_SUPPLY_TYPE_MAINS || \
				charger->cable_type == POWER_SUPPLY_TYPE_HV_MAINS) {
				if (charger->siop_level < 100 ) {
					set_charging_current_max = SIOP_INPUT_LIMIT_CURRENT;
				} else {
					set_charging_current_max =
						charger->charging_current_max;
				}

				if (charger->siop_level < 100 &&
				    current_now > SIOP_CHARGING_LIMIT_CURRENT)
					current_now = SIOP_CHARGING_LIMIT_CURRENT;
				max77823_set_input_current(charger,
						   set_charging_current_max);
			} else if (charger->cable_type == POWER_SUPPLY_TYPE_WIRELESS) {
				if (charger->siop_level < 100 ) {
					set_charging_current_max = SIOP_WIRELESS_INPUT_LIMIT_CURRENT;
				} else {
					set_charging_current_max =
						charger->charging_current_max;
				}

				if (charger->siop_level < 100 &&
						current_now > SIOP_WIRELESS_CHARGING_LIMIT_CURRENT)
					current_now = SIOP_WIRELESS_CHARGING_LIMIT_CURRENT;
				max77823_set_input_current(charger,
					set_charging_current_max);
			}

			max77823_set_charge_current(charger, current_now);

		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int max77823_otg_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct max77823_charger_data *charger =
		container_of(psy, struct max77823_charger_data, psy_otg);
	u8 data;

	val->intval = 0;
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		max77823_read_reg(charger->i2c, MAX77823_CHG_CNFG_00, &data);
		val->intval = (0xc400 >> (data & 0x0f)) & 1;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int max77823_otg_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct max77823_charger_data *charger =
		container_of(psy, struct max77823_charger_data, psy_otg);
	static u8 chg_int_state;
	u8 chg_cnfg_00;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		pr_info("%s: OTG %s\n", __func__, val->intval > 0 ? "on" : "off");

		if (val->intval) {
			max77823_read_reg(charger->i2c, MAX77823_CHG_INT_MASK,
				&chg_int_state);

			/* disable charger interrupt: CHG_I, CHGIN_I */
			/* enable charger interrupt: BYP_I */
			max77823_update_reg(charger->i2c, MAX77823_CHG_INT_MASK,
				MAX77823_CHG_IM | MAX77823_CHGIN_IM,
				MAX77823_CHG_IM | MAX77823_CHGIN_IM | MAX77823_BYP_IM);
			/* disable charger detection */
#ifdef CONFIG_EXTCON_MAX77828
			max77828_muic_set_chgdeten(DISABLE);
#endif
			/* OTG on, boost on */
			max77823_update_reg(charger->i2c, MAX77823_CHG_CNFG_00,
				CHG_CNFG_00_OTG_CTRL, CHG_CNFG_00_OTG_CTRL);

			/* Update CHG_CNFG_11 to 0x50(5V) */
			max77823_write_reg(charger->i2c,
				MAX77823_CHG_CNFG_11, 0x50);
		} else {
			/* OTG off, boost off, (buck on) */
			max77823_update_reg(charger->i2c, MAX77823_CHG_CNFG_00,
				CHG_CNFG_00_BUCK_MASK, CHG_CNFG_00_BUCK_MASK | CHG_CNFG_00_OTG_CTRL);

			/* Update CHG_CNFG_11 to 0x00(3V) */
			max77823_write_reg(charger->i2c,
				MAX77823_CHG_CNFG_11, 0x00);
			mdelay(50);
#ifdef CONFIG_EXTCON_MAX77828
			max77828_muic_set_chgdeten(ENABLE);
#endif
			/* enable charger interrupt */
			max77823_write_reg(charger->i2c,
				MAX77823_CHG_INT_MASK, chg_int_state);
		}
		max77823_read_reg(charger->i2c, MAX77823_CHG_INT_MASK,
			&chg_int_state);
		max77823_read_reg(charger->i2c, MAX77823_CHG_CNFG_00,
			&chg_cnfg_00);
		pr_debug("%s: INT_MASK(0x%x), CHG_CNFG_00(0x%x)\n",
			__func__, chg_int_state, chg_cnfg_00);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int max77823_debugfs_show(struct seq_file *s, void *data)
{
	struct max77823_charger_data *charger = s->private;
	u8 reg;
	u8 reg_data;

	seq_printf(s, "MAX77823 CHARGER IC :\n");
	seq_printf(s, "===================\n");
	for (reg = 0xB0; reg <= 0xC3; reg++) {
		max77823_read_reg(charger->i2c, reg, &reg_data);
		seq_printf(s, "0x%02x:\t0x%02x\n", reg, reg_data);
	}

	seq_printf(s, "\n");
	return 0;
}

static int max77823_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, max77823_debugfs_show, inode->i_private);
}

static const struct file_operations max77823_debugfs_fops = {
	.open           = max77823_debugfs_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

static void max77823_chg_isr_work(struct work_struct *work)
{
	struct max77823_charger_data *charger =
		container_of(work, struct max77823_charger_data, isr_work.work);

	union power_supply_propval val;

	if (charger->pdata->full_check_type ==
	    SEC_BATTERY_FULLCHARGED_CHGINT) {

		val.intval = max77823_get_charger_state(charger);

		switch (val.intval) {
		case POWER_SUPPLY_STATUS_DISCHARGING:
			pr_err("%s: Interrupted but Discharging\n", __func__);
			break;

		case POWER_SUPPLY_STATUS_NOT_CHARGING:
			pr_err("%s: Interrupted but NOT Charging\n", __func__);
			break;

		case POWER_SUPPLY_STATUS_FULL:
			pr_info("%s: Interrupted by Full\n", __func__);
			psy_set_prop(charger, PS_BATT, POWER_SUPPLY_PROP_STATUS, &val);
			break;

		case POWER_SUPPLY_STATUS_CHARGING:
			pr_err("%s: Interrupted but Charging\n", __func__);
			break;

		case POWER_SUPPLY_STATUS_UNKNOWN:
		default:
			pr_err("%s: Invalid Charger Status\n", __func__);
			break;
		}
	}

	if (charger->pdata->ovp_uvlo_check_type ==
		SEC_BATTERY_OVP_UVLO_CHGINT) {
		val.intval = max77823_get_charging_health(charger);
		switch (val.intval) {
		case POWER_SUPPLY_HEALTH_OVERHEAT:
		case POWER_SUPPLY_HEALTH_COLD:
			pr_err("%s: Interrupted but Hot/Cold\n", __func__);
			break;

		case POWER_SUPPLY_HEALTH_DEAD:
			pr_err("%s: Interrupted but Dead\n", __func__);
			break;

		case POWER_SUPPLY_HEALTH_OVERVOLTAGE:
		case POWER_SUPPLY_HEALTH_UNDERVOLTAGE:
			pr_info("%s: Interrupted by OVP/UVLO\n", __func__);
			psy_set_prop(charger, PS_BATT, POWER_SUPPLY_PROP_HEALTH, &val);
			break;

		case POWER_SUPPLY_HEALTH_UNSPEC_FAILURE:
			pr_err("%s: Interrupted but Unspec\n", __func__);
			break;

		case POWER_SUPPLY_HEALTH_GOOD:
			pr_err("%s: Interrupted but Good\n", __func__);
			break;

		case POWER_SUPPLY_HEALTH_UNKNOWN:
		default:
			pr_err("%s: Invalid Charger Health\n", __func__);
			break;
		}
	}
}

static irqreturn_t max77823_chg_irq_thread(int irq, void *irq_data)
{
	struct max77823_charger_data *charger = irq_data;

	pr_info("%s: Charger interrupt occured\n", __func__);

	if ((charger->pdata->full_check_type ==
	     SEC_BATTERY_FULLCHARGED_CHGINT) ||
	    (charger->pdata->ovp_uvlo_check_type ==
	     SEC_BATTERY_OVP_UVLO_CHGINT))
		schedule_delayed_work(&charger->isr_work, 0);

	return IRQ_HANDLED;
}

static void wpc_detect_work(struct work_struct *work)
{
	struct max77823_charger_data *charger = container_of(work,
						struct max77823_charger_data,
						wpc_work.work);
	int wc_w_state;
	int retry_cnt;
	union power_supply_propval value;
	u8 reg_data;

	pr_info("%s\n", __func__);

	max77823_read_reg(charger->i2c,
			  MAX77823_CHG_INT_MASK, &reg_data);
	reg_data &= ~(1 << 5);
	max77823_write_reg(charger->i2c,
		MAX77823_CHG_INT_MASK, reg_data);

	/* check and unlock */
	check_charger_unlock_state(charger);

	retry_cnt = 0;
	do {
		max77823_read_reg(charger->i2c,
				  MAX77823_CHG_INT_OK, &reg_data);
		wc_w_state = (reg_data & MAX77823_WCIN_OK)
			>> MAX77823_WCIN_OK_SHIFT;
		msleep(50);
	} while((retry_cnt++ < 2) && (wc_w_state == 0));

	if ((charger->wc_w_state == 0) && (wc_w_state == 1)) {
		value.intval = 1;
		psy_set_prop(charger, PS_WIRELESS, POWER_SUPPLY_PROP_ONLINE,
				&value);
		value.intval = POWER_SUPPLY_TYPE_WIRELESS;
		pr_info("%s: wpc activated, set V_INT as PN\n",
				__func__);
	} else if ((charger->wc_w_state == 1) && (wc_w_state == 0)) {
		if (!charger->is_charging)
			max77823_set_charger_state(charger, true);

		retry_cnt = 0;
		do {
			max77823_read_reg(charger->i2c,
					  MAX77823_CHG_DETAILS_01, &reg_data);
			reg_data = ((reg_data & MAX77823_CHG_DTLS)
				    >> MAX77823_CHG_DTLS_SHIFT);
			msleep(50);
		} while((retry_cnt++ < 2) && (reg_data == 0x8));
		pr_info("%s: reg_data: 0x%x, charging: %d\n", __func__,
			reg_data, charger->is_charging);
		if (!charger->is_charging)
			max77823_set_charger_state(charger, false);
		if ((reg_data != 0x08)
		    && (charger->cable_type == POWER_SUPPLY_TYPE_WIRELESS)) {
			pr_info("%s: wpc uvlo, but charging\n", __func__);
			queue_delayed_work(charger->wqueue, &charger->wpc_work,
					   msecs_to_jiffies(500));
			return;
		} else {
			value.intval = 0;
			psy_set_prop(charger, PS_WIRELESS,
					POWER_SUPPLY_PROP_ONLINE, &value);
			pr_info("%s: wpc deactivated, set V_INT as PD\n",
					__func__);
		}
	}
	pr_info("%s: w(%d to %d)\n", __func__,
		charger->wc_w_state, wc_w_state);

	charger->wc_w_state = wc_w_state;

	wake_unlock(&charger->wpc_wake_lock);
}

static irqreturn_t wpc_charger_irq(int irq, void *data)
{
	struct max77823_charger_data *charger = data;
	unsigned long delay;
	u8 reg_data;

	max77823_read_reg(charger->i2c,
		MAX77823_CHG_INT_MASK, &reg_data);
	reg_data |= (1 << 5);
	max77823_write_reg(charger->i2c,
		MAX77823_CHG_INT_MASK, reg_data);

	wake_lock(&charger->wpc_wake_lock);
#ifdef CONFIG_SAMSUNG_BATTERY_FACTORY
	delay = msecs_to_jiffies(0);
#else
	if (charger->wc_w_state)
		delay = msecs_to_jiffies(500);
	else
		delay = msecs_to_jiffies(0);
#endif
	queue_delayed_work(charger->wqueue, &charger->wpc_work,
			delay);
	return IRQ_HANDLED;
}

static irqreturn_t max77823_bypass_irq(int irq, void *data)
{
	struct max77823_charger_data *charger = data;
//	struct otg_notify *n = get_otg_notify();
	u8 dtls_02;
	u8 byp_dtls;
	u8 chg_cnfg_00;
	u8 vbus_state;

	pr_info("%s: irq(%d)\n", __func__, irq);

	/* check and unlock */
	check_charger_unlock_state(charger);

	max77823_read_reg(charger->i2c,
			  MAX77823_CHG_DETAILS_02,
			  &dtls_02);

	byp_dtls = ((dtls_02 & MAX77823_BYP_DTLS) >>
				MAX77823_BYP_DTLS_SHIFT);
	pr_info("%s: BYP_DTLS(0x%02x)\n", __func__, byp_dtls);
	vbus_state = max77823_get_vbus_state(charger);

	if (byp_dtls & 0x1) {
		pr_info("%s: bypass overcurrent limit\n", __func__);
//		send_otg_notify(n, NOTIFY_EVENT_OVERCURRENT, 0);

		/* disable the register values just related to OTG and
		   keep the values about the charging */
		max77823_read_reg(charger->i2c,
			MAX77823_CHG_CNFG_00, &chg_cnfg_00);
		chg_cnfg_00 &= ~(CHG_CNFG_00_OTG_MASK
				| CHG_CNFG_00_BOOST_MASK);
		max77823_write_reg(charger->i2c,
					MAX77823_CHG_CNFG_00,
					chg_cnfg_00);
	}
	return IRQ_HANDLED;
}

static void max77823_chgin_isr_work(struct work_struct *work)
{
	struct max77823_charger_data *charger = container_of(work,
				struct max77823_charger_data, chgin_work);
	u8 chgin_dtls, chg_dtls, chg_cnfg_00, reg_data;
	u8 prev_chgin_dtls = 0xff;
	int bat_health;
	union power_supply_propval value;
	int stable_count = 0;

	wake_lock(&charger->chgin_wake_lock);
	max77823_read_reg(charger->i2c,
			  MAX77823_CHG_INT_MASK, &reg_data);
	reg_data |= (1 << 6);
	max77823_write_reg(charger->i2c,
		MAX77823_CHG_INT_MASK, reg_data);

	while (1) {
		psy_get_prop(charger, PS_BATT, POWER_SUPPLY_PROP_HEALTH, &value);
		bat_health = value.intval;

		max77823_read_reg(charger->i2c, MAX77823_CHG_DETAILS_00,
				&chgin_dtls);
		chgin_dtls = ((chgin_dtls & MAX77823_CHGIN_DTLS) >>
				MAX77823_CHGIN_DTLS_SHIFT);
		max77823_read_reg(charger->i2c,
				MAX77823_CHG_DETAILS_01, &chg_dtls);
		chg_dtls = ((chg_dtls & MAX77823_CHG_DTLS) >>
				MAX77823_CHG_DTLS_SHIFT);
		max77823_read_reg(charger->i2c,
			MAX77823_CHG_CNFG_00, &chg_cnfg_00);

		if (prev_chgin_dtls == chgin_dtls) {
			stable_count++;
		} else {
			stable_count = 0;
			prev_chgin_dtls = chgin_dtls;
		}
		if (stable_count > 10)
			break;
		msleep(100);
	}
	pr_info("%s: irq(%d), chgin(0x%x), chg_dtls(0x%x) is_charging %d"
			", bat_health=0x%x\n",
			__func__, charger->irq_chgin, chgin_dtls, chg_dtls,
			charger->is_charging, bat_health);
	if (charger->is_charging) {
		if ((chgin_dtls == 0x02) && \
			(bat_health != POWER_SUPPLY_HEALTH_OVERVOLTAGE)) {
			pr_info("%s: charger is over voltage\n", __func__);
			value.intval = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
			psy_set_prop(charger, PS_BATT, POWER_SUPPLY_PROP_HEALTH,
					&value);
		} else if (((chgin_dtls == 0x0) || (chgin_dtls == 0x01)) &&
				(chg_dtls & 0x08) && \
				(chg_cnfg_00 & MAX77823_MODE_BUCK) && \
				(chg_cnfg_00 & MAX77823_MODE_CHGR) && \
				(bat_health != POWER_SUPPLY_HEALTH_UNDERVOLTAGE) && \
				(charger->cable_type != POWER_SUPPLY_TYPE_WIRELESS)) {
			pr_info("%s, vbus_state : 0x%d, chg_state : 0x%d\n",
					__func__, chgin_dtls, chg_dtls);
			pr_info("%s: vBus is undervoltage\n", __func__);
			value.intval = POWER_SUPPLY_HEALTH_UNDERVOLTAGE;
			psy_set_prop(charger, PS_BATT, POWER_SUPPLY_PROP_HEALTH,
					&value);
		} else if ((bat_health == POWER_SUPPLY_HEALTH_OVERVOLTAGE) &&
				(chgin_dtls != 0x02)) {
			pr_info("%s: vbus_state : 0x%d, chg_state : 0x%d\n",
					__func__, chgin_dtls, chg_dtls);
			pr_info("%s: overvoltage->normal\n", __func__);
			value.intval = POWER_SUPPLY_HEALTH_GOOD;
			psy_set_prop(charger, PS_BATT, POWER_SUPPLY_PROP_HEALTH,
					&value);
		} else if ((bat_health == POWER_SUPPLY_HEALTH_UNDERVOLTAGE) &&
				!((chgin_dtls == 0) || (chgin_dtls == 1))) {
			pr_info("%s: vbus_state : 0x%d, chg_state : 0x%d\n",
					__func__, chgin_dtls, chg_dtls);
			pr_info("%s: undervoltage->normal\n", __func__);
			value.intval = POWER_SUPPLY_HEALTH_GOOD;
			psy_set_prop(charger, PS_BATT, POWER_SUPPLY_PROP_HEALTH,
					&value);
			max77823_set_input_current(charger,
					charger->charging_current_max);
		}
	}
	max77823_read_reg(charger->i2c,
		MAX77823_CHG_INT_MASK, &reg_data);
	reg_data &= ~(1 << 6);
	max77823_write_reg(charger->i2c,
		MAX77823_CHG_INT_MASK, reg_data);
	wake_unlock(&charger->chgin_wake_lock);
}

static irqreturn_t max77823_chgin_irq(int irq, void *data)
{
	struct max77823_charger_data *charger = data;
	queue_work(charger->wqueue, &charger->chgin_work);

	return IRQ_HANDLED;
}

/* register chgin isr after sec_battery_probe */
static void max77823_chgin_init_work(struct work_struct *work)
{
	struct max77823_charger_data *charger = container_of(work,
						struct max77823_charger_data,
						chgin_init_work.work);
	int ret;
	union power_supply_propval value;

	pr_info("%s \n", __func__);
	queue_work(charger->wqueue, &charger->chgin_work);
	ret = request_threaded_irq(charger->irq_chgin, NULL,
			max77823_chgin_irq, 0, "chgin-irq", charger);
	if (ret < 0) {
		pr_err("%s: fail to request chgin IRQ: %d: %d\n",
				__func__, charger->irq_chgin, ret);
	}
	value.intval = POWER_SUPPLY_TYPE_HV_MAINS;
	psy_set_prop(charger, PS_BATT, POWER_SUPPLY_PROP_ONLINE, &value);
	max77823_set_online(charger, value.intval);
}

#ifdef CONFIG_OF
static int sec_charger_read_u32_index_dt(const struct device_node *np,
				       const char *propname,
				       u32 index, u32 *out_value)
{
	struct property *prop = of_find_property(np, propname, NULL);
	u32 len = (index + 1) * sizeof(*out_value);

	if (!prop)
		return (-EINVAL);
	if (!prop->value)
		return (-ENODATA);
	if (len > prop->length)
		return (-EOVERFLOW);

	*out_value = be32_to_cpup(((__be32 *)prop->value) + index);

	return 0;
}

static int max77823_charger_parse_dt(struct max77823_charger_data *charger, struct device_node *np)
{
	sec_battery_platform_data_t *pdata = charger->pdata;
	int ret = 0;

	if (np == NULL) {
		pr_err("%s np NULL\n", __func__);
		return -1;
	} else {
		int i, len;
		const u32 *p;

		ret = of_property_read_u32(np, "battery,chg_float_voltage",
					&pdata->chg_float_voltage);
		ret = of_property_read_u32(np, "battery,ovp_uvlo_check_type",
					&pdata->ovp_uvlo_check_type);
		ret = of_property_read_u32(np, "battery,full_check_type",
					&pdata->full_check_type);

		if (!pdata->charging_current) {
			p = of_get_property(np, "battery,input_current_limit", &len);
			len = len / sizeof(u32);
			pdata->charging_current = kzalloc(sizeof(sec_charging_current_t) * len,
						  GFP_KERNEL);

			pdata->charging_current_entries = len;
			for(i = 0; i < len; i++) {
				struct sec_charging_current *scc = &pdata->charging_current[i];

				ret = sec_charger_read_u32_index_dt(np,
					 "battery,input_current_limit", i,
					 &scc->input_current_limit);
				ret = sec_charger_read_u32_index_dt(np,
					 "battery,fast_charging_current", i,
					 &scc->fast_charging_current);
				ret = sec_charger_read_u32_index_dt(np,
					 "battery,full_check_current_1st", i,
					 &scc->full_check_current_1st);
				ret = sec_charger_read_u32_index_dt(np,
					 "battery,full_check_current_2nd", i,
					 &scc->full_check_current_2nd);
			}
		}
	}

	return ret;
}
#endif

static int max77823_charger_probe(struct platform_device *pdev)
{
	struct max77823_dev *max77823 = dev_get_drvdata(pdev->dev.parent);
	struct max77823_platform_data *pdata = dev_get_platdata(max77823->dev);
	struct max77823_charger_data *charger;
	int ret = 0;
	u8 reg_data;
	struct regulator *reg_otg;

	pr_info("%s: Max77823 Charger Driver Loading\n", __func__);

	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	pdata->charger_data = kzalloc(sizeof(sec_battery_platform_data_t), GFP_KERNEL);
	if (!pdata->charger_data) {
		ret = -ENOMEM;
		goto err_free;
	}

	mutex_init(&charger->charger_mutex);
	charger->dev = &pdev->dev;
	charger->i2c = max77823->charger;
	charger->pdata = pdata->charger_data;
	charger->aicl_on = false;
	charger->siop_level = 100;

#if defined(CONFIG_OF)
	ret = max77823_charger_parse_dt(charger, pdev->dev.of_node);
	if (ret < 0) {
		pr_err("%s not found charger dt! ret[%d]\n",
		       __func__, ret);
	}
#endif

	platform_set_drvdata(pdev, charger);

	reg_otg = devm_regulator_get(&pdev->dev, "usbotg");
	if (PTR_ERR(reg_otg) == -EPROBE_DEFER) {
		dev_err(&pdev->dev, "usbotg not ready, retry\n");
		ret = PTR_ERR(reg_otg);
		goto err_free;
	}
	if (!IS_ERR(reg_otg)) {
		charger->otg_regulator_nb.notifier_call =
				max77823_otg_regulator_nb;
		ret = regulator_register_notifier(reg_otg,
				&charger->otg_regulator_nb);
		if (ret != 0) {
			dev_err(&pdev->dev,
				"Failed to register regulator notifier: %d\n",
				ret);
		} else {
			dev_err(&pdev->dev, "usbotg notifier success\n");
		}
	}

	charger->psy_chg.name		= "max77823-charger";
	charger->psy_chg.type		= POWER_SUPPLY_TYPE_UNKNOWN;
	charger->psy_chg.get_property	= max77823_chg_get_property;
	charger->psy_chg.set_property	= max77823_chg_set_property;
	charger->psy_chg.properties	= max77823_charger_props;
	charger->psy_chg.num_properties	= ARRAY_SIZE(max77823_charger_props);
	charger->psy_chg.property_is_writeable  = max77823_chg_property_is_writeable;

	charger->psy_otg.name		= "otg";
	charger->psy_otg.type		= POWER_SUPPLY_TYPE_OTG;
	charger->psy_otg.get_property	= max77823_otg_get_property;
	charger->psy_otg.set_property	= max77823_otg_set_property;
	charger->psy_otg.properties		= max77823_otg_props;
	charger->psy_otg.num_properties	= ARRAY_SIZE(max77823_otg_props);

	max77823_charger_initialize(charger);

	(void) debugfs_create_file("max77823-regs",
		S_IRUGO, NULL, (void *)charger, &max77823_debugfs_fops);

	charger->wqueue =
	    create_singlethread_workqueue(dev_name(&pdev->dev));
	if (!charger->wqueue) {
		pr_err("%s: Fail to Create Workqueue\n", __func__);
		kfree(pdata->charger_data);
		ret = -ENOMEM;
		goto err_free;
	}
	wake_lock_init(&charger->chgin_wake_lock, WAKE_LOCK_SUSPEND,
			            "charger-chgin");
	INIT_WORK(&charger->chgin_work, max77823_chgin_isr_work);
	INIT_DELAYED_WORK(&charger->chgin_init_work, max77823_chgin_init_work);
	INIT_DELAYED_WORK(&charger->chg_cable_work, max77823_chg_cable_work);
	wake_lock_init(&charger->wpc_wake_lock, WAKE_LOCK_SUSPEND,
					       "charger-wpc");
	INIT_DELAYED_WORK(&charger->wpc_work, wpc_detect_work);
	ret = power_supply_register(&pdev->dev, &charger->psy_otg);
	if (ret) {
		pr_err("%s: Failed to Register psy_otg\n", __func__);
		goto err_workqueue;
	}
	ret = power_supply_register(&pdev->dev, &charger->psy_chg);
	if (ret) {
		pr_err("%s: Failed to Register psy_chg\n", __func__);
		goto err_psy_unreg_otg;
	}

	if (charger->pdata->chg_irq) {
		INIT_DELAYED_WORK(&charger->isr_work, max77823_chg_isr_work);

		ret = request_threaded_irq(charger->pdata->chg_irq,
				NULL, max77823_chg_irq_thread,
				charger->pdata->chg_irq_attr,
				"charger-irq", charger);
		if (ret) {
			pr_err("%s: Failed to Request IRQ\n", __func__);
			goto err_irq;
		}

			ret = enable_irq_wake(charger->pdata->chg_irq);
			if (ret < 0)
				pr_err("%s: Failed to Enable Wakeup Source(%d)\n",
					__func__, ret);
		}

	charger->wc_w_irq = pdata->irq_base + MAX77823_CHG_IRQ_WCIN_I;
	ret = request_threaded_irq(charger->wc_w_irq,
				   NULL, wpc_charger_irq,
				   IRQF_TRIGGER_FALLING,
				   "wpc-int", charger);
	if (ret) {
		pr_err("%s: Failed to Request IRQ\n", __func__);
		goto err_wc_irq;
	}

	max77823_read_reg(charger->i2c,
			  MAX77823_CHG_INT_OK, &reg_data);
	charger->wc_w_state = (reg_data & MAX77823_WCIN_OK)
		>> MAX77823_WCIN_OK_SHIFT;

	charger->irq_chgin = pdata->irq_base + MAX77823_CHG_IRQ_CHGIN_I;
	/* enable chgin irq after sec_battery_probe */
	queue_delayed_work(charger->wqueue, &charger->chgin_init_work,
			msecs_to_jiffies(3000));

	charger->irq_bypass = pdata->irq_base + MAX77823_CHG_IRQ_BYP_I;
	ret = request_threaded_irq(charger->irq_bypass, NULL,
			max77823_bypass_irq, 0, "bypass-irq", charger);
	if (ret < 0)
		pr_err("%s: fail to request bypass IRQ: %d: %d\n",
				__func__, charger->irq_bypass, ret);

	pr_info("%s: MAX77823 Charger Driver Loaded\n", __func__);

	return 0;

err_wc_irq:
	if (charger->pdata->chg_irq)
		free_irq(charger->pdata->chg_irq, NULL);
err_irq:
	power_supply_unregister(&charger->psy_chg);
err_psy_unreg_otg:
	power_supply_unregister(&charger->psy_otg);
err_workqueue:
	destroy_workqueue(charger->wqueue);
err_free:
	kfree(charger);

	return ret;
}

static int max77823_charger_remove(struct platform_device *pdev)
{
	struct max77823_charger_data *charger =
		platform_get_drvdata(pdev);
//	int i;

	destroy_workqueue(charger->wqueue);
	free_irq(charger->wc_w_irq, NULL);
	free_irq(charger->pdata->chg_irq, NULL);
	power_supply_unregister(&charger->psy_chg);
//	for (i = 0; i < ARRAY_SIZE(charger->psy_ref); i++)
//		power_supply_put(charger->psy_ref[i]);
	kfree(charger);

	return 0;
}

#if defined CONFIG_PM
static int max77823_charger_suspend(struct device *dev)
{
	return 0;
}

static int max77823_charger_resume(struct device *dev)
{
	return 0;
}
#else
#define max77823_charger_suspend NULL
#define max77823_charger_resume NULL
#endif

static void max77823_charger_shutdown(struct device *dev)
{
	struct max77823_charger_data *charger =
				dev_get_drvdata(dev);
	u8 reg_data;

	pr_info("%s: MAX77823 Charger driver shutdown\n", __func__);
	if (!charger->i2c) {
		pr_err("%s: no max77823 i2c client\n", __func__);
		return;
	}
	reg_data = 0x04;
	max77823_write_reg(charger->i2c,
		MAX77823_CHG_CNFG_00, reg_data);
	reg_data = 0x0F;
	max77823_write_reg(charger->i2c,
		MAX77823_CHG_CNFG_09, reg_data);
	reg_data = 0x19;
	max77823_write_reg(charger->i2c,
		MAX77823_CHG_CNFG_10, reg_data);
	reg_data = 0x67;
	max77823_write_reg(charger->i2c,
		MAX77823_CHG_CNFG_12, reg_data);
	pr_info("func:%s \n", __func__);
}

#ifdef CONFIG_OF
static struct of_device_id max77823_charger_dt_ids[] = {
	{ .compatible = "samsung,max77823-charger" },
	{ }
};
MODULE_DEVICE_TABLE(of, max77823_charger_dt_ids);
#endif

static SIMPLE_DEV_PM_OPS(max77823_charger_pm_ops, max77823_charger_suspend,
			 max77823_charger_resume);

static struct platform_driver max77823_charger_driver = {
	.driver = {
		.name = "max77823-charger",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &max77823_charger_pm_ops,
#endif
		.shutdown = max77823_charger_shutdown,
#ifdef CONFIG_OF
		.of_match_table = max77823_charger_dt_ids,
#endif
	},
	.probe = max77823_charger_probe,
	.remove = max77823_charger_remove,
};

static int __init max77823_charger_init(void)
{
	pr_info("%s : \n", __func__);
	return platform_driver_register(&max77823_charger_driver);
}

static void __exit max77823_charger_exit(void)
{
	platform_driver_unregister(&max77823_charger_driver);
}

module_init(max77823_charger_init);
module_exit(max77823_charger_exit);

MODULE_DESCRIPTION("Samsung MAX77823 Charger Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:max77823-charger");
