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
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
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
	if (psy->desc->get_property) {
		ret = psy->desc->get_property(psy, property, value);
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
	if (psy->desc->set_property) {
		ret = psy->desc->set_property(psy, property, value);
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
	int ret;
	u8 chgprot;
	int retry_cnt = 0;
	bool need_init = false;

	do {
		ret = max77823_read_reg(i2c, MAX77823_CHG_CNFG_06);
		if (ret < 0)
			break;
		chgprot = ((ret & 0x0C) >> 2);
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
	u32 addr = 0;

	for (addr = 0xB0; addr <= 0xC3; addr++) {
		pr_debug("MAX77823 addr : 0x%02x data : 0x%02x\n", addr,
			max77823_read_reg(charger->i2c, addr));
	}
}

static int max77823_get_vbus_state(struct max77823_charger_data *charger)
{
	int ret;

	ret = max77823_read_reg(charger->i2c, MAX77823_CHG_DETAILS_00);
	if (ret < 0)
		return ret;

	if (charger->cable_type == POWER_SUPPLY_TYPE_WIRELESS)
		ret = ((ret & MAX77823_WCIN_DTLS) >>
			    MAX77823_WCIN_DTLS_SHIFT);
	else
		ret = ((ret & MAX77823_CHGIN_DTLS) >>
			    MAX77823_CHGIN_DTLS_SHIFT);

	switch (ret) {
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

	return ret;
}

static int max77823_get_charger_state(struct max77823_charger_data *charger)
{
	int status = POWER_SUPPLY_STATUS_UNKNOWN;
	int ret;

	ret = max77823_read_reg(charger->i2c, MAX77823_CHG_DETAILS_01);

	pr_info("%s : charger status (0x%02x)\n", __func__, ret);
	if (ret < 0)
		return status;

	ret &= 0x0f;

	switch (ret)
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
	int ret;

	ret = max77823_read_reg(charger->i2c, MAX77823_CHG_INT_OK);
	if (ret < 0)
		return 1;
	pr_debug("%s:(0x%02x)\n", __func__, ret);
	return (ret >> MAX77823_DETBAT_SHIFT) & 1;
}

static int max77823_get_bat_health(struct max77823_charger_data *charger)
{
	int state;
	int ret;

	ret = max77823_read_reg(charger->i2c, MAX77823_CHG_DETAILS_01);
	if (ret < 0)
		return POWER_SUPPLY_HEALTH_UNKNOWN;
	ret = ((ret & MAX77823_BAT_DTLS) >> MAX77823_BAT_DTLS_SHIFT);

	pr_debug("%s: 0x%x\n", __func__, ret);
	switch (ret) {
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
		pr_info("%s: battery unknown : 0x%d\n", __func__, ret);
		state = POWER_SUPPLY_HEALTH_UNKNOWN;
		break;
	}
	return state;
}

static int max77823_get_charging_health(struct max77823_charger_data *charger)
{
	int state = POWER_SUPPLY_HEALTH_GOOD;
	int vbus_state;
	int retry_cnt;
	int ret;
	u8 chg_dtls;

	/* VBUS OVP state return battery OVP state */
	vbus_state = max77823_get_vbus_state(charger);
	/* read CHG_DTLS and detecting battery terminal error */
	ret = max77823_read_reg(charger->i2c, MAX77823_CHG_DETAILS_01);
	if (ret < 0)
		return POWER_SUPPLY_HEALTH_UNKNOWN;
	chg_dtls = ((ret & MAX77823_CHG_DTLS) >>
			MAX77823_CHG_DTLS_SHIFT);

	/* print the log at the abnormal case */
	if (charger->is_charging && (chg_dtls & 0x08)) {
		pr_info("%s: CHG_DTLS_00(0x%x), CHG_DTLS_01(0x%x), CHG_CNFG_00(0x%x)\n",
			__func__,
			max77823_read_reg(charger->i2c, MAX77823_CHG_DETAILS_00),
			chg_dtls,
			max77823_read_reg(charger->i2c, MAX77823_CHG_CNFG_00));
		pr_info("%s:  CHG_CNFG_01(0x%x), CHG_CNFG_02(0x%x), CHG_CNFG_04(0x%x)\n",
			__func__,
			max77823_read_reg(charger->i2c, MAX77823_CHG_CNFG_01),
			max77823_read_reg(charger->i2c, MAX77823_CHG_CNFG_02),
			max77823_read_reg(charger->i2c, MAX77823_CHG_CNFG_04));
		pr_info("%s:  CHG_CNFG_09(0x%x), CHG_CNFG_12(0x%x)\n",
			__func__,
			max77823_read_reg(charger->i2c, MAX77823_CHG_CNFG_09),
			max77823_read_reg(charger->i2c, MAX77823_CHG_CNFG_12));
	}

	pr_debug("%s: vbus_state : 0x%x, chg_dtls : 0x%x chg_cnfg_00=0x%x\n",
		__func__, vbus_state, chg_dtls,
		max77823_read_reg(charger->i2c, MAX77823_CHG_CNFG_00));
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
			(charger->cable_type != POWER_SUPPLY_TYPE_WIRELESS)) {
		pr_debug("%s: vbus is under, vbus_state=%d, chg_dtls=%d\n",
			__func__, vbus_state, chg_dtls);
		state = POWER_SUPPLY_HEALTH_UNDERVOLTAGE;
	}

	max77823_test_read(charger);

	/* If we are supplying power, mark as undervoltage */
	if ((state == POWER_SUPPLY_HEALTH_GOOD) && charger->otg_vbus_enabled)
		state = POWER_SUPPLY_HEALTH_UNDERVOLTAGE;
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
	int ret;
	int get_current = 0;

	if (charger->cable_type == POWER_SUPPLY_TYPE_WIRELESS) {
		ret = max77823_read_reg(charger->i2c, MAX77823_CHG_CNFG_10);
		/* AND operation for removing the formal 2bit  */
		if (ret >= 0)
			ret = ret & 0x3F;

		if (ret <= 0x3)
			get_current = 60;
		else
			get_current = ret * 20;
	} else {
		ret = max77823_read_reg(charger->i2c, MAX77823_CHG_CNFG_09);
		/* AND operation for removing the formal 1bit  */
		if (ret >= 0)
			ret = ret & 0x7F;

		if (ret <= 0x3)
			get_current = 100;
		else if (ret >= 0x78)
			get_current = 4000;
		else {
			int quotient, remainder;
			quotient = ret / 3;
			remainder = ret % 3;
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
	int ret;

	ret = max77823_update_reg(charger->i2c, MAX77823_CHG_CNFG_00,
		enable ? MAX77823_MODE_BUCK : 0,
		MAX77823_MODE_BUCK);

	pr_info("%s: CHG_CNFG_00(0x%02x)\n", __func__, ret);
}

static void max77823_chg_cable_work(struct work_struct *work)
{
	struct max77823_charger_data *charger =
	container_of(work, struct max77823_charger_data,chg_cable_work.work);
	int quotient, remainder;
	int ret;

	pr_debug("[%s]VALUE(%d)DATA(0x%x)\n",
		__func__, charger->charging_current_max,
		max77823_read_reg(charger->i2c, MAX77823_CHG_CNFG_09));

	quotient = charger->charging_current_max / 100;
	remainder = charger->charging_current_max % 100;

	ret = quotient * 3;
	if (remainder >= 67)
		ret += 2;
	else if (remainder >= 33)
		ret += 1;

	max77823_write_reg(charger->i2c, MAX77823_CHG_CNFG_09, ret);

	pr_info("[%s]VALUE(%d)DATA(0x%x)\n",
		__func__, charger->charging_current_max,
		ret);
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
	int ret;

	ret = max77823_read_reg(charger->i2c, MAX77823_CHG_CNFG_02);
	if (ret < 0)
		return;
	ret &= ~MAX77823_CHG_CC;

	if (!fast_charging_current) {
		max77823_write_reg(charger->i2c, MAX77823_CHG_CNFG_02, ret);
	} else {
		ret |= (fast_charging_current / curr_step);
		max77823_write_reg(charger->i2c,MAX77823_CHG_CNFG_02, ret);
	}
	pr_info("%s: 0x%02x, charging current(%d)\n",
		__func__, ret, fast_charging_current);

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
	int ret;

	ret = max77823_update_reg(charger->i2c, MAX77823_CHG_CNFG_00,
		enable ? MAX77823_MODE_CHGR : 0,
		MAX77823_MODE_CHGR);

	pr_debug("%s : CHG_CNFG_00(0x%02x)\n", __func__, ret);
}

static void max77823_charger_function_control(
				struct max77823_charger_data *charger)
{
	const int usb_charging_current = get_charging_info(charger, POWER_SUPPLY_TYPE_USB)->fast_charging_current;
	int set_charging_current, set_charging_current_max;

	pr_info("####%s#### %d\n", __func__, charger->cable_type);

	if (charger->cable_type == POWER_SUPPLY_TYPE_BATTERY) {
		int ret;

		charger->is_charging = false;
		charger->aicl_on = false;
		set_charging_current = 0;
		set_charging_current_max =
			get_charging_info(charger, POWER_SUPPLY_TYPE_BATTERY)->input_current_limit;

		set_charging_current_max =
			get_charging_info(charger, POWER_SUPPLY_TYPE_USB)->input_current_limit;
		charger->charging_current = set_charging_current;
		charger->charging_current_max = set_charging_current_max;

		ret = max77823_update_reg(charger->i2c, MAX77823_CHG_CNFG_00,
			charger->pdata->boost ? CHG_CNFG_00_BOOST_MASK : 0,
			CHG_CNFG_00_CHG_MASK | CHG_CNFG_00_BUCK_MASK |
			CHG_CNFG_00_BOOST_MASK);
		pr_debug("%s: CHG_CNFG_00(0x%02x)%d\n", __func__, ret, charger->pdata->boost);
	} else {
		int ret = max77823_read_reg(charger->i2c, MAX77823_CHG_CNFG_12);

		if ((ret >= 0) && (ret & 0x20))
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
	int ret;
	pr_info("%s\n", __func__);

	/* unmasked: CHGIN_I, WCIN_I, BATP_I, BYP_I	*/
	max77823_write_reg(charger->i2c, MAX77823_CHG_INT_MASK, 0x9a);

	/* unlock charger setting protect */
	max77823_write_reg(charger->i2c, MAX77823_CHG_CNFG_06, 0x03 << 2);

	/*
	 * fast charge timer disable
	 * restart threshold disable
	 * pre-qual charge enable(default)
	 */
	max77823_write_reg(charger->i2c, MAX77823_CHG_CNFG_01,
		(0x08 << 0) | (0x03 << 4));

	/*
	 * charge current 466mA(default)
	 * otg current limit 900mA
	 */
	max77823_update_reg(charger->i2c, MAX77823_CHG_CNFG_02, 1 << 6, 3 << 6);

	/*
	 * top off current 100mA
	 * top off timer 70min
	 */
	max77823_write_reg(charger->i2c, MAX77823_CHG_CNFG_03, 0x38);

	/*
	 * cv voltage 4.2V or 4.35V
	 * MINVSYS 3.6V(default)
	 */
	ret = max77823_get_float_voltage_data(charger->pdata->chg_float_voltage);
	ret = max77823_update_reg(charger->i2c, MAX77823_CHG_CNFG_04,
			(ret << CHG_CNFG_04_CHG_CV_PRM_SHIFT),
			CHG_CNFG_04_CHG_CV_PRM_MASK);
	pr_info("%s: battery cv voltage 0x%x\n", __func__, ret);
	max77823_test_read(charger);
}

static int max77823_chg_get_property(struct power_supply *psy,
			      enum power_supply_property psp,
			      union power_supply_propval *val)
{
	struct max77823_charger_data *charger = psy->drv_data;
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = POWER_SUPPLY_TYPE_BATTERY;
		ret = max77823_read_reg(charger->i2c, MAX77823_CHG_INT_OK);
		if (ret >= 0) {
			if (ret & MAX77823_WCIN_OK) {
				val->intval = POWER_SUPPLY_TYPE_WIRELESS;
				charger->wc_w_state = 1;
			} else if (ret & MAX77823_CHGIN_OK) {
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
		ret = max77823_read_reg(charger->i2c, MAX77823_CHG_CNFG_12);
		if (ret >= 0)
			val->intval = (ret >> 5) & 1;
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

	if (event & (REGULATOR_EVENT_DISABLE | REGULATOR_EVENT_AFT_DO_ENABLE | REGULATOR_EVENT_PRE_DO_ENABLE)) {
		charger->otg_vbus_enabled = (event & REGULATOR_EVENT_DISABLE) ? false : true;
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
	int ret;

	if (type == POWER_SUPPLY_TYPE_POWER_SHARING) {
		int enable_mask = 0;

		psy_get_prop(charger, PS_PS, POWER_SUPPLY_PROP_STATUS, &value);

		if (value.intval | charger->pdata->boost) {
			enable_mask |=  CHG_CNFG_00_BOOST_MASK;
		} else {
			ret = max77823_read_reg(charger->i2c,
				MAX77823_CHG_CNFG_00);
			if (ret < 0)
				return;
			if (ret & CHG_CNFG_00_OTG_MASK)
				enable_mask |= CHG_CNFG_00_BOOST_MASK;
		}
		max77823_update_reg(charger->i2c, MAX77823_CHG_CNFG_00,
			enable_mask, CHG_CNFG_00_BOOST_MASK);
		return;
	}
	charger->cable_type = type;
	max77823_charger_function_control(charger);
}

static int max77823_chg_set_property(struct power_supply *psy,
			  enum power_supply_property psp,
			  const union power_supply_propval *val)
{
	struct max77823_charger_data *charger = psy->drv_data;
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

static int max77823_debugfs_show(struct seq_file *s, void *data)
{
	struct max77823_charger_data *charger = s->private;
	int ret;
	u8 reg;

	seq_printf(s, "MAX77823 CHARGER IC :\n");
	seq_printf(s, "===================\n");
	for (reg = 0xB0; reg <= 0xC3; reg++) {
		ret = max77823_read_reg(charger->i2c, reg);
		if (ret >= 0)
			seq_printf(s, "0x%02x:\t0x%02x\n", reg, ret);
		else
			seq_printf(s, "0x%02x:\t----\n", reg);
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
		val.intval = max77823_get_bat_health(charger);
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
			pr_info("%s: Interrupted by OVP/UVLO, %d\n", __func__, val.intval);
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
	int wc_w_state = 0;
	int retry_cnt;
	union power_supply_propval value;
	int ret;

	pr_info("%s\n", __func__);

	/* unmask WCIN Interrupt*/
	max77823_update_reg(charger->i2c,
		MAX77823_CHG_INT_MASK, 0, 1 << 5);

	/* check and unlock */
	check_charger_unlock_state(charger);

	retry_cnt = 0;
	do {
		ret = max77823_read_reg(charger->i2c, MAX77823_CHG_INT_OK);
		if (ret >= 0) {
			wc_w_state = (ret & MAX77823_WCIN_OK)
				>> MAX77823_WCIN_OK_SHIFT;
		}
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
			ret = max77823_read_reg(charger->i2c,
					  MAX77823_CHG_DETAILS_01);
			if (ret >= 0)
				ret = ((ret & MAX77823_CHG_DTLS)
					>> MAX77823_CHG_DTLS_SHIFT);
			else
				ret = 8;
			msleep(50);
		} while((retry_cnt++ < 2) && (ret == 0x8));
		pr_info("%s: 0x%x, charging: %d\n", __func__,
			ret, charger->is_charging);
		if (!charger->is_charging)
			max77823_set_charger_state(charger, false);
		if ((ret != 0x08)
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

	max77823_update_reg(charger->i2c,
		MAX77823_CHG_INT_MASK, 1 << 5, 1 << 5);

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
	int ret;
	u8 byp_dtls;
	u8 vbus_state;

	pr_info("%s: irq(%d)\n", __func__, irq);

	/* check and unlock */
	check_charger_unlock_state(charger);

	ret = max77823_read_reg(charger->i2c, MAX77823_CHG_DETAILS_02);
	if (ret < 0)
		ret = 0;
	byp_dtls = ((ret & MAX77823_BYP_DTLS) >>
				MAX77823_BYP_DTLS_SHIFT);
	pr_info("%s: BYP_DTLS(0x%02x)\n", __func__, byp_dtls);
	vbus_state = max77823_get_vbus_state(charger);

	if (byp_dtls & 0x1) {
		pr_info("%s: bypass overcurrent limit\n", __func__);
//		send_otg_notify(n, NOTIFY_EVENT_OVERCURRENT, 0);

		/* disable the register values just related to OTG and
		   keep the values about the charging */
		max77823_update_reg(charger->i2c, MAX77823_CHG_CNFG_00,
			charger->pdata->boost ? CHG_CNFG_00_BOOST_MASK : 0,
			CHG_CNFG_00_OTG_MASK | CHG_CNFG_00_BOOST_MASK);
	}
	return IRQ_HANDLED;
}

static void max77823_chgin_isr_work(struct work_struct *work)
{
	struct max77823_charger_data *charger = container_of(work,
				struct max77823_charger_data, chgin_work);
	u8 chgin_dtls = 0;
	u8 chg_dtls = 0;
	u8 chg_cnfg_00 = 0;
	int ret;
	u8 prev_chgin_dtls = 0xff;
	int bat_health;
	union power_supply_propval value;
	int stable_count = 0;

	wake_lock(&charger->chgin_wake_lock);
	/* mask chrgin interrupt */
	max77823_update_reg(charger->i2c, MAX77823_CHG_INT_MASK,
		1 << 6, 1 << 6);

	while (1) {
		psy_get_prop(charger, PS_BATT, POWER_SUPPLY_PROP_HEALTH, &value);
		bat_health = value.intval;

		ret = max77823_read_reg(charger->i2c, MAX77823_CHG_DETAILS_00);
		if (ret >= 0) {
			chgin_dtls = ((ret & MAX77823_CHGIN_DTLS) >>
				MAX77823_CHGIN_DTLS_SHIFT);
		}
		ret = max77823_read_reg(charger->i2c, MAX77823_CHG_DETAILS_01);
		if (ret >= 0) {
			chg_dtls = ((ret & MAX77823_CHG_DTLS) >>
				MAX77823_CHG_DTLS_SHIFT);
		}
		ret = max77823_read_reg(charger->i2c, MAX77823_CHG_CNFG_00);
		if (ret >= 0)
			chg_cnfg_00 = ret;

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
		} else if (((chgin_dtls == 0x0) || (chgin_dtls == 0x01)) &&
				(chg_dtls & 0x08) && \
				(chg_cnfg_00 & MAX77823_MODE_BUCK) && \
				(chg_cnfg_00 & MAX77823_MODE_CHGR) && \
				(bat_health != POWER_SUPPLY_HEALTH_UNDERVOLTAGE) && \
				(charger->cable_type != POWER_SUPPLY_TYPE_WIRELESS)) {
			pr_info("%s, vbus_state : 0x%d, chg_state : 0x%d\n",
					__func__, chgin_dtls, chg_dtls);
			pr_info("%s: vBus is undervoltage\n", __func__);
		} else if ((bat_health == POWER_SUPPLY_HEALTH_OVERVOLTAGE) &&
				(chgin_dtls != 0x02)) {
			pr_info("%s: vbus_state : 0x%d, chg_state : 0x%d\n",
					__func__, chgin_dtls, chg_dtls);
			pr_info("%s: overvoltage->normal\n", __func__);
		} else if ((bat_health == POWER_SUPPLY_HEALTH_UNDERVOLTAGE) &&
				!((chgin_dtls == 0) || (chgin_dtls == 1))) {
			pr_info("%s: vbus_state : 0x%d, chg_state : 0x%d\n",
					__func__, chgin_dtls, chg_dtls);
			pr_info("%s: undervoltage->normal\n", __func__);
			max77823_set_input_current(charger,
					charger->charging_current_max);
		}
	}
	/* Have the battery reevaluate charging */
	psy_get_prop(charger, PS_BATT, POWER_SUPPLY_PROP_HEALTH, &value);
	/* unmask chrgin interrupt */
	max77823_update_reg(charger->i2c,
		MAX77823_CHG_INT_MASK, 0, 1 << 6);
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
	charger->irq_chgin = charger->irq_base + MAX77823_CHG_IRQ_CHGIN_I;
	ret = request_threaded_irq(charger->irq_chgin, NULL,
			max77823_chgin_irq, 0, "chgin-irq", charger);
	if (ret < 0) {
		pr_err("%s: fail to request chgin IRQ: %d: %d\n",
				__func__, charger->irq_chgin, ret);
		charger->irq_chgin = 0;
	}
	value.intval = POWER_SUPPLY_TYPE_HV_MAINS;
	psy_set_prop(charger, PS_BATT, POWER_SUPPLY_PROP_ONLINE, &value);
	/* Have the battery reevaluate charging */
	psy_get_prop(charger, PS_BATT, POWER_SUPPLY_PROP_HEALTH, &value);
}

#ifdef CONFIG_OF
static int max77823_otg_enable(struct max77823_charger_data *charger)
{
	pr_info("%s:\n", __func__);

	charger->otg_vbus_enabled = true;
	charger->is_charging = false;
	/* Disable charging from CHRG_IN when we are supplying power */
	max77823_update_reg(charger->i2c, MAX77823_CHG_CNFG_12,
			0, 0x20);

	/* disable charger interrupt: CHG_I, CHGIN_I */
	/* enable charger interrupt: BYP_I */
	max77823_update_reg(charger->i2c, MAX77823_CHG_INT_MASK,
		MAX77823_CHG_IM | MAX77823_CHGIN_IM,
		MAX77823_CHG_IM | MAX77823_CHGIN_IM | MAX77823_BYP_IM);
			/* disable charger detection */
#ifdef CONFIG_EXTCON_MAX77828
	max77828_muic_set_chgdeten(DISABLE);
#endif
	/* Update CHG_CNFG_11 to 0x54(5.1V) */
	max77823_write_reg(charger->i2c,
		MAX77823_CHG_CNFG_11, 0x54);

	/* OTG on, boost on */
	max77823_update_reg(charger->i2c, MAX77823_CHG_CNFG_00,
		CHG_CNFG_00_OTG_MASK | CHG_CNFG_00_BOOST_MASK,
		CHG_CNFG_00_OTG_MASK | CHG_CNFG_00_BOOST_MASK);

	pr_debug("%s: INT_MASK(0x%x), CHG_CNFG_00(0x%x)\n", __func__,
		max77823_read_reg(charger->i2c, MAX77823_CHG_INT_MASK),
		max77823_read_reg(charger->i2c, MAX77823_CHG_CNFG_00));
	return 0;
}

static int max77823_otg_disable(struct max77823_charger_data *charger)
{
	int enable_mask = CHG_CNFG_00_CHG_MASK | CHG_CNFG_00_BUCK_MASK;

	pr_info("%s:\n", __func__);
	charger->otg_vbus_enabled = false;
	if (charger->pdata->boost)
		enable_mask |= CHG_CNFG_00_BOOST_MASK;
	/* chrg on, OTG off, boost on/off, (buck on) */
	max77823_update_reg(charger->i2c, MAX77823_CHG_CNFG_00,
		enable_mask, CHG_CNFG_00_CHG_MASK | CHG_CNFG_00_BUCK_MASK |
		CHG_CNFG_00_OTG_MASK | CHG_CNFG_00_BOOST_MASK);

	mdelay(50);
#ifdef CONFIG_EXTCON_MAX77828
	max77828_muic_set_chgdeten(ENABLE);
#endif
	/* enable charger interrupt */
	max77823_update_reg(charger->i2c, MAX77823_CHG_INT_MASK,
		0, MAX77823_CHG_IM | MAX77823_CHGIN_IM | MAX77823_BYP_IM);

	/* Allow charging from CHRG_IN when we are not supplying power */
	max77823_update_reg(charger->i2c, MAX77823_CHG_CNFG_12,
			0x20, 0x20);

	if (charger->cable_type != POWER_SUPPLY_TYPE_BATTERY)
		charger->is_charging = true;
	pr_debug("%s: INT_MASK(0x%x), CHG_CNFG_00(0x%x)\n", __func__,
		max77823_read_reg(charger->i2c, MAX77823_CHG_INT_MASK),
		max77823_read_reg(charger->i2c, MAX77823_CHG_CNFG_00));
	return 0;
}

int max77823_regulator_enable(struct regulator_dev *rdev)
{
	int ret = 0;

	if (rdev_get_id(rdev) == REG_OTG) {
		ret = max77823_otg_enable(rdev_get_drvdata(rdev));
	}
	return (ret < 0) ? ret : 0;
}

int max77823_regulator_disable(struct regulator_dev *rdev)
{
	int ret = 0;

	if (rdev_get_id(rdev) == REG_OTG) {
		ret = max77823_otg_disable(rdev_get_drvdata(rdev));
	}
	return (ret < 0) ? ret : 0;
}

int max77823_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct max77823_charger_data *charger = rdev_get_drvdata(rdev);
	int ret;

	ret = max77823_read_reg(charger->i2c, rdev->desc->enable_reg);
	if (ret < 0)
		return ret;

	ret &= rdev->desc->enable_mask;
	return ret != 0;
}

int max77823_regulator_list_voltage_linear(struct regulator_dev *rdev,
				  unsigned int selector)
{
	if (selector >= rdev->desc->n_voltages)
		return -EINVAL;
	if (selector < rdev->desc->linear_min_sel)
		return 0;

	selector -= rdev->desc->linear_min_sel;

	return rdev->desc->min_uV + (rdev->desc->uV_step * selector);
}

static struct regulator_ops max77823_regulator_ops = {
	.enable = max77823_regulator_enable,
	.disable = max77823_regulator_disable,
	.is_enabled = max77823_regulator_is_enabled,
	.list_voltage = max77823_regulator_list_voltage_linear,
};

static struct regulator_desc regulators_ds[] = {
	{
		.name = "otg",
		.n_voltages = 1,
		.ops = &max77823_regulator_ops,
		.type = REGULATOR_VOLTAGE,
		.id = REG_OTG,
		.owner = THIS_MODULE,
		.min_uV = 5000000,
		.enable_reg = MAX77823_CHG_CNFG_00,
		.enable_mask = CHG_CNFG_00_OTG_MASK,
	},
};

static struct of_regulator_match reg_matches[] = {
	{ .name = "otg",	},
};

static int parse_regulators_dt(struct device *dev, const struct device_node *np,
		struct max77823_charger_data *charger)
{
	struct regulator_config config = { };
	struct device_node *parent;
	int ret;
	int i;

	parent = of_get_child_by_name(np, "regulators");
	if (!parent) {
		dev_err(dev, "regulators node not found\n");
		return -EINVAL;
	}

	ret = of_regulator_match(dev, parent, reg_matches,
				 ARRAY_SIZE(reg_matches));

	of_node_put(parent);
	if (ret < 0) {
		dev_err(dev, "Error parsing regulator init data: %d\n",
			ret);
		return ret;
	}

	memcpy(charger->reg_descs, regulators_ds,
		sizeof(charger->reg_descs));

	for (i = 0; i < ARRAY_SIZE(reg_matches); i++) {
		struct regulator_desc *desc = &charger->reg_descs[i];

		config.dev = dev;
		config.init_data = reg_matches[i].init_data;
		config.driver_data = charger;
		config.of_node = reg_matches[i].of_node;
		config.ena_gpio = -EINVAL;

		charger->regulators[i] =
			devm_regulator_register(dev, desc, &config);
		if (IS_ERR(charger->regulators[i])) {
			dev_err(dev, "register regulator%s failed\n",
				desc->name);
			return PTR_ERR(charger->regulators[i]);
		}
	}
	return 0;
}

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
		ret = of_property_read_u32(np, "boost",
					&pdata->boost);

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
		pr_info("%s:chg_float_voltage=%d, "
			"ovp_uvlo_check_type=%d, "
			"full_check_type=%d, "
			"boost=%d\n", __func__,
			pdata->chg_float_voltage,
			pdata->ovp_uvlo_check_type,
			pdata->full_check_type,
			pdata->boost);
	}

	return ret;
}
#endif

const struct power_supply_desc psy_chg_desc = {
	.name = "max77823-charger",
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.properties = max77823_charger_props,
	.num_properties = ARRAY_SIZE(max77823_charger_props),
	.get_property = max77823_chg_get_property,
	.set_property = max77823_chg_set_property,
	.property_is_writeable  = max77823_chg_property_is_writeable,
};

struct power_supply_config psy_chg_config = {
};

static int max77823_charger_probe(struct platform_device *pdev)
{
	struct max77823_dev *max77823 = dev_get_drvdata(pdev->dev.parent);
	struct max77823_platform_data *pdata = dev_get_platdata(max77823->dev);
	struct max77823_charger_data *charger;
	int ret = 0;
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
	charger->irq_base = pdata->irq_base;
	charger->aicl_on = false;
	charger->siop_level = 100;

#if defined(CONFIG_OF)
	ret = max77823_charger_parse_dt(charger, pdev->dev.of_node);
	if (ret < 0) {
		pr_err("%s:dt error! ret[%d]\n", __func__, ret);
		goto err_free;
	}
	ret = parse_regulators_dt(&pdev->dev, pdev->dev.of_node, charger);
	if (ret < 0) {
		pr_err("%s:reg dt error! ret[%d]\n", __func__, ret);
		goto err_free;
	}
#endif

	platform_set_drvdata(pdev, charger);

	reg_otg = devm_regulator_get_optional(&pdev->dev, "usbotg");
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
	psy_chg_config.drv_data = charger;
	charger->psy_chg = power_supply_register(&pdev->dev, &psy_chg_desc, &psy_chg_config);
	if (IS_ERR(charger->psy_chg)) {
		pr_err("%s: Failed to Register psy_chg\n", __func__);
		ret = PTR_ERR(charger->psy_chg);
		goto err_workqueue;
	}

	if (charger->pdata->chg_irq) {
		INIT_DELAYED_WORK(&charger->isr_work, max77823_chg_isr_work);

		charger->chg_irq = charger->pdata->chg_irq;
		ret = request_threaded_irq(charger->chg_irq,
				NULL, max77823_chg_irq_thread,
				charger->pdata->chg_irq_attr,
				"charger-irq", charger);
		if (ret) {
			pr_err("%s: Failed to Request IRQ\n", __func__);
			goto err_irq;
		}

		ret = enable_irq_wake(charger->chg_irq);
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

	/* Update CHG_CNFG_11 to 0x54(5.1V), boost to 5.1V */
	max77823_write_reg(charger->i2c,
		MAX77823_CHG_CNFG_11, 0x54);

	ret = max77823_read_reg(charger->i2c, MAX77823_CHG_INT_OK);
	if (ret >= 0) {
		charger->wc_w_state = (ret & MAX77823_WCIN_OK)
			>> MAX77823_WCIN_OK_SHIFT;
	}

	max77823_update_reg(charger->i2c, MAX77823_CHG_CNFG_00,
		charger->pdata->boost ? CHG_CNFG_00_BOOST_MASK : 0,
		CHG_CNFG_00_BOOST_MASK);

	/* enable chgin irq after sec_battery_probe */
	queue_delayed_work(charger->wqueue, &charger->chgin_init_work,
			msecs_to_jiffies(3000));

	charger->irq_bypass = pdata->irq_base + MAX77823_CHG_IRQ_BYP_I;
	ret = request_threaded_irq(charger->irq_bypass, NULL,
			max77823_bypass_irq, 0, "bypass-irq", charger);
	if (ret < 0) {
		pr_err("%s: fail to request bypass IRQ: %d: %d\n",
				__func__, charger->irq_bypass, ret);
		charger->irq_bypass = 0;
	}

	pr_info("%s: MAX77823 Charger Driver Loaded\n", __func__);

	return 0;

err_wc_irq:
	if (charger->chg_irq)
		free_irq(charger->chg_irq, charger);
err_irq:
	power_supply_unregister(charger->psy_chg);
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
	if (charger->wc_w_irq)
		free_irq(charger->wc_w_irq, charger);
	if (charger->chg_irq)
		free_irq(charger->chg_irq, charger);
	if (charger->irq_chgin)
		free_irq(charger->irq_chgin, charger);
	if (charger->irq_bypass)
		free_irq(charger->irq_bypass, charger);
	power_supply_unregister(charger->psy_chg);
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
/*  No need to turn off charging when device is going to poweroff */
#if 0
	struct max77823_charger_data *charger =
				dev_get_drvdata(dev);

	pr_info("%s: MAX77823 Charger driver shutdown\n", __func__);
	if (!charger->i2c) {
		pr_err("%s: no max77823 i2c client\n", __func__);
		return;
	}
	max77823_write_reg(charger->i2c, MAX77823_CHG_CNFG_00, 0x04);
	max77823_write_reg(charger->i2c, MAX77823_CHG_CNFG_09, 0x0f);
	max77823_write_reg(charger->i2c, MAX77823_CHG_CNFG_10, 0x19);
	max77823_write_reg(charger->i2c, MAX77823_CHG_CNFG_12, 0x67);
#endif
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
