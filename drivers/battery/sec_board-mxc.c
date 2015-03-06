/*
 *  sec_board-mxc.c
 *  Samsung Mobile Battery Driver
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/battery/sec_battery.h>

#if defined(CONFIG_FUELGAUGE_MAX17048)
#include <linux/battery/fuelgauge/max17048_fuelgauge.h>
#endif

#if defined(CONFIG_FUELGAUGE_MAX77823) || defined(CONFIG_FUELGAUGE_MAX77843)
#include <linux/battery/fuelgauge/max77823_fuelgauge.h>
#endif

//#include <linux/qpnp/pin.h>
//#include <linux/qpnp/qpnp-adc.h>
#include <linux/regulator/machine.h>
//#include <linux/regulator/krait-regulator.h>

#define SHORT_BATTERY_STANDARD      100

#if defined(CONFIG_EXTCON)
extern int get_jig_state(void);
#endif

int current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
extern unsigned int system_rev;

#if defined(CONFIG_BATTERY_SAMSUNG_DATA)
#include CONFIG_BATTERY_SAMSUNG_DATA_FILE
#else //CONFIG_BATTERY_SAMSUNG_DATA
#if defined(CONFIG_FUELGAUGE_MAX17048)
static struct max17048_fuelgauge_battery_data_t max17048_battery_data[] = {
	/* SDI battery data (High voltage 4.35V) */
	{
		.RCOMP0 = 0x5D,
		.RCOMP_charging = 0x5D,
		.temp_cohot = -175,
		.temp_cocold = -5825,
		.is_using_model_data = true,
		.type_str = "SDI",
	}
};
#endif

#if defined(CONFIG_FUELGAUGE_MAX77823) || defined(CONFIG_FUELGAUGE_MAX77843)
static struct max77823_fuelgauge_battery_data_t max77823_battery_data[] = {
	/* SDI battery data (High voltage 4.4V) */
	{
		.Capacity = 0x187E, /* TR/TB : 3220mAh */
		.low_battery_comp_voltage = 3500000,
		.low_battery_table = {
			/* range, slope, offset */
			{-5000000,	0,	0},	/* dummy for top limit */
			{-1250000, 0,	3320},
			{-750000, 97,	3451},
			{-100000, 96,	3461},
			{0, 0,	3456},
		},
		.temp_adjust_table = {
			/* range, slope, offset */
			{47000, 122,	8950},
			{60000, 200,	51000},
			{100000, 0,	0},	/* dummy for top limit */
		},
		.type_str = "SDI",
	}
};
#endif

#define CAPACITY_MAX			990
#define CAPACITY_MAX_MARGIN	50
#define CAPACITY_MIN			-7

static sec_bat_adc_table_data_t temp_table[] = {
	{26009, 900},
	{26280, 850},
	{26600, 800},
	{26950, 750},
	{27325, 700},
	{27737, 650},
	{28180, 600},
	{28699, 550},
	{29360, 500},
	{29970, 450},
	{30995, 400},
	{32046, 350},
	{32985, 300},
	{34050, 250},
	{35139, 200},
	{36179, 150},
	{37208, 100},
	{38237, 50},
	{38414, 40},
	{38598, 30},
	{38776, 20},
	{38866, 10},
	{38956, 0},
	{39102, -10},
	{39247, -20},
	{39393, -30},
	{39538, -40},
	{39684, -50},
	{40490, -100},
	{41187, -150},
	{41652, -200},
	{42030, -250},
	{42327, -300},
};

static sec_bat_adc_table_data_t chg_temp_table[] = {
	{0, 0},
};

#define TEMP_HIGH_THRESHOLD_EVENT	600
#define TEMP_HIGH_RECOVERY_EVENT		460
#define TEMP_LOW_THRESHOLD_EVENT		-50
#define TEMP_LOW_RECOVERY_EVENT		0
#define TEMP_HIGH_THRESHOLD_NORMAL	600
#define TEMP_HIGH_RECOVERY_NORMAL	460
#define TEMP_LOW_THRESHOLD_NORMAL	-50
#define TEMP_LOW_RECOVERY_NORMAL	0
#define TEMP_HIGH_THRESHOLD_LPM		600
#define TEMP_HIGH_RECOVERY_LPM		460
#define TEMP_LOW_THRESHOLD_LPM		-50
#define TEMP_LOW_RECOVERY_LPM		0
#endif

static void sec_bat_adc_ap_init(struct platform_device *pdev,
			struct sec_battery_info *battery)
{
}

static int sec_bat_adc_ap_read(struct sec_battery_info *battery, int channel)
{
	int data = -1;

	switch (channel)
	{
	case SEC_BAT_ADC_CHANNEL_TEMP :
	case SEC_BAT_ADC_CHANNEL_TEMP_AMBIENT:
		data = 33000;
		break;
	default :
		break;
	}

	pr_debug("%s: data(%d)\n", __func__, data);

	return data;
}

static void sec_bat_adc_ap_exit(void)
{
}

static void sec_bat_adc_none_init(struct platform_device *pdev,
			struct sec_battery_info *battery)
{
}

static int sec_bat_adc_none_read(struct sec_battery_info *battery, int channel)
{
	return 0;
}

static void sec_bat_adc_none_exit(void)
{
}

static void sec_bat_adc_ic_init(struct platform_device *pdev,
			struct sec_battery_info *battery)
{
}

static int sec_bat_adc_ic_read(struct sec_battery_info *battery, int channel)
{
	return 0;
}

static void sec_bat_adc_ic_exit(void)
{
}
static int adc_read_type(struct sec_battery_info *battery, int channel)
{
	int adc = 0;

	switch (battery->pdata->temp_adc_type)
	{
	case SEC_BATTERY_ADC_TYPE_NONE :
		adc = sec_bat_adc_none_read(battery, channel);
		break;
	case SEC_BATTERY_ADC_TYPE_AP :
		adc = sec_bat_adc_ap_read(battery, channel);
		break;
	case SEC_BATTERY_ADC_TYPE_IC :
		adc = sec_bat_adc_ic_read(battery, channel);
		break;
	case SEC_BATTERY_ADC_TYPE_NUM :
		break;
	default :
		break;
	}
	pr_debug("[%s] ADC = %d\n", __func__, adc);
	return adc;
}

static void adc_init_type(struct platform_device *pdev,
			struct sec_battery_info *battery)
{
	switch (battery->pdata->temp_adc_type)
	{
	case SEC_BATTERY_ADC_TYPE_NONE :
		sec_bat_adc_none_init(pdev, battery);
		break;
	case SEC_BATTERY_ADC_TYPE_AP :
		sec_bat_adc_ap_init(pdev, battery);
		break;
	case SEC_BATTERY_ADC_TYPE_IC :
		sec_bat_adc_ic_init(pdev, battery);
		break;
	case SEC_BATTERY_ADC_TYPE_NUM :
		break;
	default :
		break;
	}
}

static void adc_exit_type(struct sec_battery_info *battery)
{
	switch (battery->pdata->temp_adc_type)
	{
	case SEC_BATTERY_ADC_TYPE_NONE :
		sec_bat_adc_none_exit();
		break;
	case SEC_BATTERY_ADC_TYPE_AP :
		sec_bat_adc_ap_exit();
		break;
	case SEC_BATTERY_ADC_TYPE_IC :
		sec_bat_adc_ic_exit();
		break;
	case SEC_BATTERY_ADC_TYPE_NUM :
		break;
	default :
		break;
	}
}

int adc_read(struct sec_battery_info *battery, int channel)
{
	int adc = 0;

	adc = adc_read_type(battery, channel);

	pr_debug("[%s]adc = %d\n", __func__, adc);

	return adc;
}

void adc_exit(struct sec_battery_info *battery)
{
	adc_exit_type(battery);
}

bool sec_bat_check_jig_status(void)
{
#if defined(CONFIG_SEC_LENTIS_PROJECT) || defined(CONFIG_SEC_TRLTE_PROJECT) || defined(CONFIG_SEC_TBLTE_PROJECT) || \
	defined(CONFIG_SEC_KCCAT6_PROJECT)
#if defined(CONFIG_EXTCON)
	return get_jig_state();
#else
	return false;
#endif
#else
#if 0
	if (!sec_fuelgauge) {
		pr_err("%s: sec_fuelgauge is empty\n", __func__);
		return false;
	}

	if (sec_fuelgauge->pdata->jig_irq >= 0) {
		if (gpio_get_value_cansleep(sec_fuelgauge->pdata->jig_irq))
			return true;
		else
			return false;
	} else {
		pr_err("%s: jig_irq is invalid\n", __func__);
		return false;
	}
#endif
	return false;
#endif
}

/* callback for battery check
 * return : bool
 * true - battery detected, false battery NOT detected
 */
bool sec_bat_check_callback(struct sec_battery_info *battery)
{
	struct power_supply *psy;
	union power_supply_propval value;

	pr_info("%s:  battery->pdata->bat_irq_gpio(%d)\n",
			__func__, battery->pdata->bat_irq_gpio);
	psy = get_power_supply_by_name(battery->pdata->charger_name);
	if (!psy) {
		pr_err("%s: Failed to get psy (%s)\n",
				__func__, battery->pdata->charger_name);
		value.intval = 1;
	} else {
		int ret;
		ret = psy->get_property(psy, POWER_SUPPLY_PROP_PRESENT, &(value));
		if (ret < 0) {
			pr_err("%s: Fail to sec-charger get_property (%d=>%d)\n",
					__func__, POWER_SUPPLY_PROP_PRESENT, ret);
			value.intval = 1;
		}
	}
	return value.intval;
}

void sec_bat_check_cable_result_callback(struct device *dev,
		int cable_type)
{
	struct regulator *pma8084_lvs2;
	int rc = 0;
	current_cable_type = cable_type;

	if (current_cable_type == POWER_SUPPLY_TYPE_BATTERY)
	{
		pr_info("%s set lvs2 off\n", __func__);
		pma8084_lvs2 = regulator_get(dev, "8084_lvs2");
		if (IS_ERR(pma8084_lvs2)) {
			pr_err("%s: pma8084_lvs2 regulator_get fail\n", __func__);
			return;
		} else {
			rc = regulator_disable(pma8084_lvs2);
			if (rc) {
				pr_err("%s: error for disabling regulator VF_1P8\n", __func__);
			}
		}
	}
	else
	{
		pr_info("%s set lvs2 on\n", __func__);
		pma8084_lvs2 = regulator_get(dev, "8084_lvs2");
		if (IS_ERR(pma8084_lvs2)) {
			pr_err("%s: pma8084_lvs2 regulator_get fail\n", __func__);
			return;
		} else {
			if (!regulator_is_enabled(pma8084_lvs2)) {
				rc = regulator_enable(pma8084_lvs2);
				if (rc) {
					pr_err("%s: error for enabling regulator VF_1P8\n", __func__);
				}
			}
		}
	}
	regulator_put(pma8084_lvs2);
}

int sec_bat_check_cable_callback(struct sec_battery_info *battery)
{
	union power_supply_propval value;
	msleep(750);

	if (battery->pdata->ta_irq_gpio == 0) {
		pr_err("%s: ta_int_gpio is 0 or not assigned yet(cable_type(%d))\n",
			__func__, current_cable_type);
	} else {
		if (battery->cable_type == POWER_SUPPLY_TYPE_BATTERY &&
			!gpio_get_value_cansleep(battery->pdata->ta_irq_gpio)) {
			pr_info("%s : VBUS IN\n", __func__);

			value.intval = POWER_SUPPLY_TYPE_UARTOFF;
			psy_do_property("battery", set, POWER_SUPPLY_PROP_ONLINE, value);
			current_cable_type = POWER_SUPPLY_TYPE_UARTOFF;

			return POWER_SUPPLY_TYPE_UARTOFF;
		}

		if ((battery->cable_type == POWER_SUPPLY_TYPE_UARTOFF ||
			battery->cable_type == POWER_SUPPLY_TYPE_CARDOCK) &&
			gpio_get_value_cansleep(battery->pdata->ta_irq_gpio)) {
			pr_info("%s : VBUS OUT\n", __func__);

			value.intval = POWER_SUPPLY_TYPE_BATTERY;
			psy_do_property("battery", set, POWER_SUPPLY_PROP_ONLINE, value);
			current_cable_type = POWER_SUPPLY_TYPE_BATTERY;

			return POWER_SUPPLY_TYPE_BATTERY;
		}
	}

	return current_cable_type;
}

void board_battery_init(struct platform_device *pdev, struct sec_battery_info *battery)
{
	if ((!battery->pdata->temp_adc_table) &&
		(battery->pdata->thermal_source == SEC_BATTERY_THERMAL_SOURCE_ADC)) {
		pr_info("%s : assign temp adc table\n", __func__);

		battery->pdata->temp_adc_table = temp_table;
		battery->pdata->temp_amb_adc_table = temp_table;

		battery->pdata->temp_adc_table_size = sizeof(temp_table)/sizeof(sec_bat_adc_table_data_t);
		battery->pdata->temp_amb_adc_table_size = sizeof(temp_table)/sizeof(sec_bat_adc_table_data_t);
	}

	if ((!battery->pdata->chg_temp_adc_table) &&
		(battery->pdata->chg_temp_check)) {
		pr_info("%s : assign chg temp adc table\n", __func__);
		battery->pdata->chg_temp_adc_table = chg_temp_table;
		battery->pdata->chg_temp_adc_table_size = sizeof(chg_temp_table)/sizeof(sec_bat_adc_table_data_t);
	}

	battery->pdata->event_check = true;
	battery->pdata->temp_high_threshold_event = TEMP_HIGH_THRESHOLD_EVENT;
	battery->pdata->temp_high_recovery_event = TEMP_HIGH_RECOVERY_EVENT;
	battery->pdata->temp_low_threshold_event = TEMP_LOW_THRESHOLD_EVENT;
	battery->pdata->temp_low_recovery_event = TEMP_LOW_RECOVERY_EVENT;
	battery->pdata->temp_high_threshold_normal = TEMP_HIGH_THRESHOLD_NORMAL;
	battery->pdata->temp_high_recovery_normal = TEMP_HIGH_RECOVERY_NORMAL;
	battery->pdata->temp_low_threshold_normal = TEMP_LOW_THRESHOLD_NORMAL;
	battery->pdata->temp_low_recovery_normal = TEMP_LOW_RECOVERY_NORMAL;
	battery->pdata->temp_high_threshold_lpm = TEMP_HIGH_THRESHOLD_LPM;
	battery->pdata->temp_high_recovery_lpm = TEMP_HIGH_RECOVERY_LPM;
	battery->pdata->temp_low_threshold_lpm = TEMP_LOW_THRESHOLD_LPM;
	battery->pdata->temp_low_recovery_lpm = TEMP_LOW_RECOVERY_LPM;

	adc_init_type(pdev, battery);
}

void board_fuelgauge_init(void *data)
{
#if defined(CONFIG_FUELGAUGE_MAX17048)
#if defined(CONFIG_SEC_LENTIS_PROJECT)
	if (system_rev >= 0x06) {
#endif
		struct max17048_fuelgauge_data *fuelgauge =
			(struct max17048_fuelgauge_data *)data;

		if (!fuelgauge->battery_data) {
			pr_info("%s : assign battery data\n", __func__);
			fuelgauge->battery_data = max17048_battery_data;

			pr_info("%s: RCOMP0: 0x%x, RCOMP_charging: 0x%x, "
				"temp_cohot: %d, temp_cocold: %d, "
				"is_using_model_data: %d, type_str: %s\n", __func__ ,
				fuelgauge->battery_data->RCOMP0,
				fuelgauge->battery_data->RCOMP_charging,
				fuelgauge->battery_data->temp_cohot,
				fuelgauge->battery_data->temp_cocold,
				fuelgauge->battery_data->is_using_model_data,
				fuelgauge->battery_data->type_str
				);
		}
#if defined(CONFIG_SEC_LENTIS_PROJECT)
	} else {
		struct max77823_fuelgauge_data *fuelgauge =
			(struct max77823_fuelgauge_data *)data;

		if (!fuelgauge->battery_data) {
			pr_info("%s : assign battery data\n", __func__);
			fuelgauge->battery_data = max77823_battery_data;
		}
	}
#endif
#elif defined(CONFIG_FUELGAUGE_MAX77823) || defined(CONFIG_FUELGAUGE_MAX77843)
	struct max77823_fuelgauge_data *fuelgauge =
		(struct max77823_fuelgauge_data *)data;

	if (!fuelgauge->battery_data) {
		pr_info("%s : assign battery data\n", __func__);
		fuelgauge->battery_data = max77823_battery_data;
	}
#endif
}
EXPORT_SYMBOL_GPL(board_fuelgauge_init);

void cable_initial_check(struct sec_battery_info *battery)
{
	union power_supply_propval value;

	pr_info("%s : current_cable_type : (%d)\n", __func__, current_cable_type);
	if (POWER_SUPPLY_TYPE_BATTERY != current_cable_type) {
		value.intval = current_cable_type;
		psy_do_property("battery", set,
				POWER_SUPPLY_PROP_ONLINE, value);
	} else {
		psy_do_property(battery->pdata->charger_name, get,
				POWER_SUPPLY_PROP_ONLINE, value);
		if (value.intval == POWER_SUPPLY_TYPE_WIRELESS) {
			value.intval = 1;
			psy_do_property("wireless", set,
					POWER_SUPPLY_PROP_ONLINE, value);
		}
	}
}
EXPORT_SYMBOL_GPL(cable_initial_check);

