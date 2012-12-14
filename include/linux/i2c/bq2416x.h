#ifndef _LINUX_I2C_BQ2416X_H
#define _LINUX_I2C_BQ2416X_H

struct bq2416x_charger_policy {
	int	charge_mV;			/* 3500mV - 4440mV, default 3600mV, charge voltage  */
	int	charge_current_limit_mA;	/* 550mA - 2875mA, default 1000mA */
						/* 225mA - 1437mA, if low_charge is set */
	int	terminate_current_mA;		/* 50mA - 400mA, default 150mA, Stop charging when current drops to this */
	int	usb_voltage_limit_mV;		/* 4200mV - 47600mV, default 4200mV, reduce charge current to stay above this */
	int	usb_current_limit_mA;		/* 100mA - 1500mA, default 100mA */
	int	in_voltage_limit_mV;		/* 4200mV - 47600mV, default 4200mV,  reduce charge current to stay above this */
	int	in_current_limit_mA;		/* 1500mA or 2500mA, default 1500mA */
	int	charge_timeout_minutes;		/* 27 minute, 6 hours, 9 hours, infinite are choices, default 27 minutes */
	int	otg_lock : 1;			/* 0 - Charging from USB allowed */
	int	prefer_usb_charging : 1;	/* 0 - Charging from IN preferred */
	int	disable_charging : 1;
	char   *battery_notify;
};

struct plat_i2c_bq2416x_data {
	unsigned gp;
	struct bq2416x_charger_policy policy;
};
#endif

