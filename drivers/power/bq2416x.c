/*
 * Charger driver for bq2416x
 *
 * Copyright 2012 Boundary Devices.
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/i2c/bq2416x.h>
#include <linux/interrupt.h>
#include <linux/freezer.h>
#include <linux/gpio.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/slab.h>

#define BQ24163_STATUS_CTL	0
#define BQ24163_SUPPLY_STATUS	1
#define BQ24163_CONTROL		2
#define BQ24163_BATT_VOLTAGE	3
#define BQ24163_ID		4
#define BQ24163_BATT_CURRENT	5
#define BQ24163_DPPM		6
#define BQ24163_SAFETY_TIMER	7

#define TESTING
#define DRV_NAME	"bq2416x"
static const char *client_name = DRV_NAME;

/* decode of BQ24163_BATT_VOLTAGE[7:2] */
#define BQ_mV_TO_CHARGE_OFFSET	3500
#define BQ_mV_TO_CHARGE_STEP	20

/* decode of BQ24163_BATT_CURRENT[7:3] */
#define BQ_mA_TO_CHARGE_LIMIT_OFFSET	550		/* half this if LOW_CHARGE set */
#define BQ_mA_TO_CHARGE_LIMIT_STEP	75

/* decode of BQ24163_BATT_CURRENT[2:0] */
#define BQ_mA_TO_TERMINATE_OFFSET	50
#define BQ_mA_TO_TERMINATE_STEP		50

/* decode of BQ24163_DPPM[5:3], reduce charge current so USB does not dip below this */
#define BQ_mV_TO_USB_OFFSET	4200
#define BQ_mV_TO_USB_STEP	80

/* decode of BQ24163_CONTROL[6:4] */
unsigned short bq_usb_current_limit_mA[] = {
	100,	/* 0 - USB2.0 host */
	150,	/* 1 - USB3.0 host */
	500,	/* 2 - USB2.0 host */
	800,	/* 3 */
	900,	/* 4 - USB3.0 host */
	1500,	/* 5 */
	100,	/* 6 */
	100,	/* 7 */
};

/* decode of BQ24163_DPPM[2:0], reduce charge current so IN does not dip below this */
#define BQ_mV_TO_IN_OFFSET	4200
#define BQ_mV_TO_IN_STEP	80

/* decode of BQ24163_BATT_VOLTAGE[1] */
#define BQ_mA_TO_IN_LIMIT_OFFSET	1500
#define BQ_mA_TO_IN_LIMIT_STEP		1000

unsigned short charge_time_max[] = {
	27, 6*60, 9*60, 0xffff
};

struct bq2416x_charger_policy default_policy = {
	.charge_mV = 3600,			/* 3500mV - 4440mV, default 3600mV, charge voltage  */
	.charge_current_limit_mA = 1000,	/* 550mA - 2875mA, default 1000mA */
	.terminate_current_mA = 150,		/* 50mA - 400mA, default 150mA, Stop charging when current drops to this */
	.usb_voltage_limit_mV = 4200,		/* 4200mV - 47600mV, default 4200mV, reduce charge current to stay above this */
	.usb_current_limit_mA = 100,		/* 100mA - 1500mA, default 100mA */
	.in_voltage_limit_mV = 4200,		/* 4200mV - 47600mV, default 4200mV,  reduce charge current to stay above this */
	.in_current_limit_mA = 1500,		/* 1500mA or 2500mA, default 1500mA */
	.charge_timeout_minutes = 27,		/* 27 minute, 6 hours, 9 hours, infinite are choices, default 27 minutes */
	.otg_lock = 0,
	.prefer_usb_charging = 0,
	.disable_charging = 0,
};

struct bq2416x_priv
{
	struct i2c_client	*client;
	wait_queue_head_t	sample_waitq;
	struct task_struct	*monitoring_task;
	int			bReady;
	int			interruptCnt;
	int			irq;
	int			gp;
	int			enabled;
	int			displayoff;
	struct power_supply	battery;
	struct power_supply	usb;
	struct power_supply	ac;
	struct bq2416x_charger_policy policy;
#ifdef TESTING
	struct timeval	lastInterruptTime;
#endif
	unsigned char bq_status_ctl;
	unsigned char bq_supply_status;
	unsigned char write_cache[8];
};

int bq2416x_reg_read(struct bq2416x_priv *bq, unsigned char reg)
{
	int ret;
	int retry;
	if (reg >= 8)
		return -EINVAL;
	for (retry = 0; retry < 3; retry++) {
		ret = i2c_smbus_read_byte_data(bq->client, reg);
		if (ret < 0) {
			dev_err(&bq->client->dev, "%s failed(%i) reg=%x\n",
				__func__, ret, reg);
		} else {
//			dev_info(&bq->client->dev, "%s reg=%x val=%x\n",
//				__func__, reg, ret);
			break;
		}
	}
	return ret;
}

int bq2416x_reg_write(struct bq2416x_priv *bq, unsigned char reg,
		unsigned char val)
{
	int ret;
	int retry;
	if (reg >= 8)
		return -EINVAL;
	bq->write_cache[reg] = val;
	for (retry = 0; retry < 3; retry++) {
		ret = i2c_smbus_write_byte_data(bq->client, reg, val);
		if (ret < 0) {
			dev_err(&bq->client->dev, "%s failed(%i) reg=%x, val=%x\n",
				__func__, ret, reg, val);
		} else {
//			dev_info(&bq->client->dev, "%s reg=%x, val=%x\n",
//				__func__, reg, val);
			break;
		}
	}
	return ret;
}

static int convert_setting(struct bq2416x_priv *bq, int desired, int offset,
		int step, int max, const char* field)
{
	int tmp = (desired - offset) / step;
	int v;
	if (tmp < 0)
		tmp = 0;
	if (tmp > max)
		tmp = max;
	v = tmp * step + offset;
	if (desired != v) {
		dev_warn(&bq->client->dev, "%s, requested %d mV, got %d\n",
				field, desired, v);
	}
	return tmp;
}

static int bq2416x_charger_config(struct bq2416x_priv *bq)
{
	struct bq2416x_charger_policy *policy = &bq->policy;
	int tmp;
	int ret;
	unsigned i;
	unsigned best;
	unsigned char settings[8];

	memset(settings, 0, sizeof(settings));
	settings[BQ24163_STATUS_CTL] = BIT(7);		/* reset watchdog */
	if (policy->prefer_usb_charging)
		settings[BQ24163_STATUS_CTL] |= BIT(3);
	if (policy->otg_lock)
		settings[BQ24163_SUPPLY_STATUS] = BIT(3);
	/* EN_STAT, enable charge current termination */
	settings[BQ24163_CONTROL] = BIT(2) | BIT(3);
	if (policy->disable_charging)
		settings[BQ24163_CONTROL] |= BIT(1);
	/* enable temperature control */
	settings[BQ24163_SAFETY_TIMER] |= BIT(3);

	/* charge_mV */
	tmp = convert_setting(bq, policy->charge_mV, BQ_mV_TO_CHARGE_OFFSET,
			BQ_mV_TO_CHARGE_STEP, 0x3f, "charge_mV");
	settings[BQ24163_BATT_VOLTAGE] |= tmp << 2;

	/* charge_current_limit_mA */
	if (policy->charge_current_limit_mA < BQ_mA_TO_CHARGE_LIMIT_OFFSET) {
		tmp = convert_setting(bq, policy->charge_current_limit_mA,
				BQ_mA_TO_CHARGE_LIMIT_OFFSET / 2,
				BQ_mA_TO_CHARGE_LIMIT_STEP / 2, 0x1f,
				"charge_current_limit_mA");
		settings[BQ24163_SAFETY_TIMER] |= (1 << 0);	/* low charge */
	} else {
		tmp = convert_setting(bq, policy->charge_current_limit_mA,
				BQ_mA_TO_CHARGE_LIMIT_OFFSET,
				BQ_mA_TO_CHARGE_LIMIT_STEP, 0x1f,
				"charge_current_limit_mA");
	}
	settings[BQ24163_BATT_CURRENT] |= tmp << 3;

	/* terminate_current_mA */
	tmp = convert_setting(bq, policy->terminate_current_mA, BQ_mA_TO_TERMINATE_OFFSET,
			BQ_mA_TO_TERMINATE_STEP, 0x07, "terminate_current_mA");
	settings[BQ24163_BATT_CURRENT] |= tmp;

	/* usb_voltage_limit_mV */
	tmp = convert_setting(bq, policy->usb_voltage_limit_mV, BQ_mV_TO_USB_OFFSET,
			BQ_mV_TO_USB_STEP, 0x07, "usb_voltage_limit_mV");
	settings[BQ24163_DPPM] |= tmp << 3;

	/* usb_current_limit_mA */
	best = 0;
	for (i = 0; i < 8; i++) {
		unsigned v = bq_usb_current_limit_mA[i];
		if (v == policy->usb_current_limit_mA) {
			best = i;
			break;
		}
		if (v > policy->usb_current_limit_mA)
			continue;
		if (v > bq_usb_current_limit_mA[best])
			best = i;
	}
	if (bq_usb_current_limit_mA[best] != policy->usb_current_limit_mA) {
		dev_warn(&bq->client->dev,
			"usb_current_limit_mA, requested %d mV, got %d\n",
			policy->usb_current_limit_mA,
			bq_usb_current_limit_mA[best]);
	}
	settings[BQ24163_CONTROL] |= best << 4;

	/* in_voltage_limit_mV */
	tmp = convert_setting(bq, policy->in_voltage_limit_mV, BQ_mV_TO_IN_OFFSET,
			BQ_mV_TO_IN_STEP, 0x07, "in_voltage_limit_mV");
	settings[BQ24163_DPPM] |= tmp;

	/* in_current_limit_mA */
	tmp = convert_setting(bq, policy->in_current_limit_mA, BQ_mA_TO_IN_LIMIT_OFFSET,
			BQ_mA_TO_IN_LIMIT_STEP, 0x01, "in_current_limit_mA");
	settings[BQ24163_BATT_VOLTAGE] |= tmp << 1;

	/* charge_timeout_minutes */
	best = 0;
	for (i = 0; i < 8; i++) {
		unsigned v = charge_time_max[i];
		if (v == policy->charge_timeout_minutes) {
			best = i;
			break;
		}
		if (v > policy->charge_timeout_minutes)
			continue;
		if (v > charge_time_max[best])
			best = i;
	}
	if (charge_time_max[best] != policy->charge_timeout_minutes) {
		dev_warn(&bq->client->dev, "charge_timeout_minutes, requested %d mV, got %d\n",
				policy->charge_timeout_minutes, charge_time_max[best]);
	}
	settings[BQ24163_SAFETY_TIMER] |= best << 5;

	/* disable charging while parameters are changed */
	ret = bq2416x_reg_write(bq, BQ24163_CONTROL, settings[BQ24163_CONTROL] | BIT(1));
	if (ret < 0)
		return ret;
	for (i = BQ24163_CONTROL + 1; i < 8; i++) {
		if (i != BQ24163_ID) {
			ret = bq2416x_reg_write(bq, i, settings[i]);
			if (ret < 0)
				return ret;
		}
	}
	/* Last write reenables charging */
	for (i = 0; i <= BQ24163_CONTROL; i++) {
		ret = bq2416x_reg_write(bq, i, settings[i]);
		if (ret < 0)
			break;
	}
	return ret;
}

/*********************************************************************
 *		AC Power
 *********************************************************************/
static int bq2416x_ac_get_prop(struct power_supply *psy,
			      enum power_supply_property psp,
			      union power_supply_propval *val)
{
	struct bq2416x_priv *bq = dev_get_drvdata(psy->dev->parent);
	int ret = -EINVAL;
	int status = bq2416x_reg_read(bq, BQ24163_SUPPLY_STATUS);
	int v;

	if (status >= 0) {
		status >>= 6;
		status &= 3;
		switch (psp) {
		case POWER_SUPPLY_PROP_ONLINE:
			val->intval = (status < 3) ? 1 : 0;
			ret = 0;
			break;
		case POWER_SUPPLY_PROP_VOLTAGE_NOW:
			switch (status) {
			case 0:
				v = bq->policy.in_voltage_limit_mV;
				break;
			case 1:
				v = 5000;
				break;
			case 2:
				v = 3800;
				break;
			case 3:
				v = 0;
				break;
			}
			val->intval = v * 1000;
			ret = 0;
			break;
		default:
			break;
		}
	}
	return ret;
}

static enum power_supply_property bq2416x_ac_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
};

/*********************************************************************
 *		USB Power
 *********************************************************************/
static int bq2416x_usb_get_prop(struct power_supply *psy,
			       enum power_supply_property psp,
			       union power_supply_propval *val)
{
	struct bq2416x_priv *bq = dev_get_drvdata(psy->dev->parent);
	int ret = -EINVAL;
	int status = bq2416x_reg_read(bq, BQ24163_SUPPLY_STATUS);
	int v;

	if (status >= 0) {
		status >>= 4;
		status &= 3;
		switch (psp) {
		case POWER_SUPPLY_PROP_ONLINE:
			val->intval = (status < 3) ? 1 : 0;
			ret = 0;
			break;
		case POWER_SUPPLY_PROP_VOLTAGE_NOW:
			switch (status) {
			case 0:
				v = bq->policy.in_voltage_limit_mV;
				break;
			case 1:
				v = 5000;
				break;
			case 2:
				v = 3800;
				break;
			case 3:
				v = 0;
				break;
			}
			val->intval = v * 1000;
			ret = 0;
			break;
		default:
			break;
		}
	}
	return ret;
}

static enum power_supply_property bq2416x_usb_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
};

/*********************************************************************
 *		Battery properties
 *********************************************************************/
static ssize_t charger_state_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	char *charge = NULL;
	struct bq2416x_priv *bq = dev_get_drvdata(dev);
	int ctl = bq2416x_reg_read(bq, BQ24163_STATUS_CTL);

	if (ctl >= 0) {
		ctl >>= 4;
		switch (ctl & 0x7) {
		case 0:
			charge = "discharging";
			break;
		case 1:
			charge = "IN ready";
			break;
		case 2:
			charge = "USB ready";
			break;
		case 3:
			charge = "Charging from IN";
			break;
		case 4:
			charge = "Charging from USB";
			break;
		case 5:
			charge = "Charge done";
			break;
		case 7:
			charge = "Charge fault";
			break;
		}
	}
	if (charge)
		return sprintf(buf, "%s\n", charge);
	return 0;
}

static DEVICE_ATTR(charger_state, 0444, charger_state_show, NULL);

static int bq2416x_bat_get_charge_type(struct bq2416x_priv *bq)
{
	int ctl = bq2416x_reg_read(bq, BQ24163_STATUS_CTL);

	if (ctl >= 0) {
		ctl >>= 4;
		switch (ctl & 0x7) {
		case 0:
		case 1:
		case 2:
		case 5:
			return POWER_SUPPLY_CHARGE_TYPE_NONE;
		case 3:
		case 4:
			ctl = bq2416x_reg_read(bq, BQ24163_SAFETY_TIMER);
			if (ctl & 1)
				return POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
			return POWER_SUPPLY_CHARGE_TYPE_FAST;
		}
	}
	return POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
}

static int bq2416x_read_battery_uvolts(struct bq2416x_priv *bq)
{
	int ret = 0;
	int status = bq2416x_reg_read(bq, BQ24163_SUPPLY_STATUS);
	int v;

	if (status >= 0) {
		status >>= 4;
		status &= 3;
		switch (status) {
		case 0:
			v = bq->policy.charge_mV;
			break;
		case 1:
			v = 5000;
			break;
		case 2:
		case 3:
			v = 0;
			break;
		}
		ret = v * 1000;
	}
	return ret;
}

static int bq2416x_bat_get_property(struct power_supply *psy,
				   enum power_supply_property psp,
				   union power_supply_propval *val)
{
	struct bq2416x_priv *bq = dev_get_drvdata(psy->dev->parent);
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = !!(bq2416x_read_battery_uvolts(bq));
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = bq2416x_bat_get_charge_type(bq);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static enum power_supply_property bq2416x_bat_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
};

static ssize_t bq2416x_reg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	u8 tmp[8];
	struct bq2416x_priv *bq = dev_get_drvdata(dev);
	int ret;

	ret = i2c_smbus_read_i2c_block_data(bq->client, 0, sizeof(tmp), tmp);
	if (ret < sizeof(tmp)) {
		dev_err(dev, "i2c block read failed\n");
		return (ret < 0) ? ret : -EIO;
	}
	return sprintf(buf, "%02x %02x %02x %02x %02x %02x %02x %02x\n",
			tmp[0], tmp[1], tmp[2], tmp[3],
			tmp[4], tmp[5], tmp[6], tmp[7]);
}

static ssize_t bq2416x_reg_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	unsigned val;
	int ret;
	struct bq2416x_priv *bq = dev_get_drvdata(dev);
	char *endp;
	unsigned reg = simple_strtol(buf, &endp, 16);
	if (reg >= 0x8)
		return count;
	if (endp)
		if (*endp == 0x20)
			endp++;
	val = simple_strtol(endp, &endp, 16);
	if (val >= 0x100)
		return count;
	ret = bq2416x_reg_write(bq, reg, val);
	if (ret < 0)
		return ret;
	return count;
}

static DEVICE_ATTR(bq2416x_reg, 0644, bq2416x_reg_show, bq2416x_reg_store);

static ssize_t bq2416x_cache_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct bq2416x_priv *bq = dev_get_drvdata(dev);
	unsigned char *p = &bq->write_cache[0];
	return sprintf(buf, "%02x %02x %02x %02x %02x %02x %02x %02x\n",
			p[0], p[1], p[2], p[3],
			p[4], p[5], p[6], p[7]);
}

static DEVICE_ATTR(bq2416x_cache, 0444, bq2416x_cache_show, NULL);

int check_state_change(struct bq2416x_priv *bq, int v)
{
	int ret = 0;
	switch (v & 7) {
	case 0:
		break;
	case 1:
		dev_warn(&bq->client->dev, "Thermal Shutdown\n");
		break;
	case 2:
		dev_warn(&bq->client->dev, "Battery Temperature Fault\n");
		break;
	case 3:
		/* Watchdog expired, reinit charger */
		ret = bq2416x_charger_config(bq);
		dev_warn(&bq->client->dev, "Watchdog expired\n");
		break;
	case 4:
		dev_warn(&bq->client->dev, "Safety Timer Expired\n");
		break;
	case 5:
		dev_warn(&bq->client->dev, "IN Supply Fault\n");
		break;
	case 6:
		dev_warn(&bq->client->dev, "USB Supply Fault\n");
		break;
	case 7:
		dev_warn(&bq->client->dev, "Battery Fault\n");
		/*
		 * When battery fault happens, charge voltage
		 * is reset to 3.6V, change it back without
		 * restarting a charge cycle.
		 */
		ret = bq2416x_reg_write(bq, BQ24163_BATT_VOLTAGE,
				bq->write_cache[BQ24163_BATT_VOLTAGE]);
		break;
	}
	return ret;
}

int check_status_ctl_change(struct bq2416x_priv *bq, int v)
{
	int ret = 0;
	int t = v ^ bq->bq_status_ctl;
	if (t) {
		dev_info(&bq->client->dev, "reg 0 (status_ctl) now %x\n", v);
		if (t & 0x70)
			power_supply_changed(&bq->battery);
		if (t & 7) {
			ret = check_state_change(bq, v);
			if (ret < 0)
				v &= ~7;
		}
		bq->bq_status_ctl = v;
	}
	return ret;
}

int check_supply_status_change(struct bq2416x_priv *bq, int v)
{
	int t = v ^ bq->bq_supply_status;
	if (t) {
		dev_info(&bq->client->dev, "reg 1 (supply_status) now %x\n", v);
		bq->bq_supply_status = v;
		if (t & 0xc0)
			power_supply_changed(&bq->ac);
		if (t & 0x30)
			power_supply_changed(&bq->usb);
		if (t & 0x6)
			power_supply_changed(&bq->battery);
	}
	return 0;
}

static s32 monitoring_thread(void *data)
{
	struct bq2416x_priv *bq = (struct bq2416x_priv *)data;
	int ret;
	int v;
#ifdef TESTING
	int prev_val = -1;
#endif
	set_freezable();

	bq->interruptCnt=0;
	for (;;) {
		/* wait max 25 seconds, charging watchdog is 30 seconds */
		int timeout = msecs_to_jiffies(25000);
		/* Make this thread friendly to system suspend and resume */
		try_to_freeze();

		bq->bReady = 0;
#ifdef TESTING
		v = gpio_get_value(bq->gp);
		if (prev_val != v) {
			prev_val = v;
			dev_info(&bq->client->dev, "gp_val=%d\n", v);
		}
#endif
		v = bq2416x_reg_read(bq, BQ24163_STATUS_CTL);
		if (v >= 0) {
			ret = check_status_ctl_change(bq, v);
			if (ret < 0)
				timeout = msecs_to_jiffies(1000);
		} else {
			timeout = msecs_to_jiffies(1000);
		}
		v = bq2416x_reg_read(bq, BQ24163_SUPPLY_STATUS);
		if (v >= 0) {
			ret = check_supply_status_change(bq, v);
			if (ret < 0)
				timeout = msecs_to_jiffies(1000);
		} else {
			timeout = msecs_to_jiffies(1000);
		}

		/* reset watchdog */
		v = BIT(7);
		if (bq->policy.prefer_usb_charging)
			v |= BIT(3);
		ret = bq2416x_reg_write(bq, BQ24163_STATUS_CTL, v);
		if (ret < 0)
			timeout = msecs_to_jiffies(1000);

		if (bq->policy.battery_notify) {
			struct power_supply *bsupply = power_supply_get_by_name(bq->policy.battery_notify);
			if (bsupply && bsupply->external_power_changed)
				bsupply->external_power_changed(bsupply);
		}

		ret = wait_event_freezable_timeout(bq->sample_waitq,
				bq->bReady, timeout);
		if (kthread_should_stop())
			break;

	}
	return 0;
}


/*
 * We only detect samples ready with this interrupt
 * handler, and even then we just schedule our task.
 */
static irqreturn_t bq_interrupt(int irq, void *id)
{
	struct bq2416x_priv *bq = id;
//	printk(KERN_ERR "%s\n", __func__);
	bq->interruptCnt++;
	bq->bReady=1;
	wmb();
	wake_up(&bq->sample_waitq);
#ifdef TESTING
	{
		time_t		tv_sec = bq->lastInterruptTime.tv_sec;
		suseconds_t     tv_usec = bq->lastInterruptTime.tv_usec;
		int delta_sec, delta_usec;
		do_gettimeofday(&bq->lastInterruptTime);
		delta_usec = bq->lastInterruptTime.tv_usec - tv_usec;
		delta_sec = bq->lastInterruptTime.tv_sec - tv_sec;
		if (delta_usec < 0) {
			delta_usec += 1000000;
			delta_sec--;
		}
		pr_info("(delta=%d.%06d seconds gp%i)\n",delta_sec, delta_usec, bq->gp);
	}
#endif
	return IRQ_HANDLED;
}

/*********************************************************************
 *		Initialization
 *********************************************************************/

static int __devinit bq_init(struct bq2416x_priv *bq)
{
	int ret;
	struct power_supply *usb = &bq->usb;
	struct power_supply *battery = &bq->battery;
	struct power_supply *ac = &bq->ac;

	/* Initialize the bq2416x chip */
	ret = bq2416x_charger_config(bq);
	if (ret) {
		dev_err(&bq->client->dev, "init failed(%d)\n", ret);
		return ret;
	}

	ac->name = "bq2416x-ac";
	ac->type = POWER_SUPPLY_TYPE_MAINS;
	ac->properties = bq2416x_ac_props;
	ac->num_properties = ARRAY_SIZE(bq2416x_ac_props);
	ac->get_property = bq2416x_ac_get_prop;
	ret = power_supply_register(&bq->client->dev, ac);
	if (ret)
		return ret;

	battery->name = "bq2416x-battery";
	battery->properties = bq2416x_bat_props;
	battery->num_properties = ARRAY_SIZE(bq2416x_bat_props);
	battery->get_property = bq2416x_bat_get_property;
	battery->use_for_apm = 0;
	ret = power_supply_register(&bq->client->dev, battery);
	if (ret)
		goto battery_failed;

	usb->name = "bq2416x-usb",
	usb->type = POWER_SUPPLY_TYPE_USB;
	usb->properties = bq2416x_usb_props;
	usb->num_properties = ARRAY_SIZE(bq2416x_usb_props);
	usb->get_property = bq2416x_usb_get_prop;
	ret = power_supply_register(&bq->client->dev, usb);
	if (ret)
		goto usb_failed;

	ret = device_create_file(&bq->client->dev, &dev_attr_bq2416x_reg);
	if (ret < 0)
		dev_warn(&bq->client->dev, "failed to add bq2416x_reg sysfs: %d\n", ret);
	ret = device_create_file(&bq->client->dev, &dev_attr_bq2416x_cache);
	if (ret < 0)
		dev_warn(&bq->client->dev, "failed to add bq2416x_cache sysfs: %d\n", ret);
	ret = device_create_file(&bq->client->dev, &dev_attr_charger_state);
	if (ret < 0)
		dev_warn(&bq->client->dev, "failed to add charge sysfs: %d\n", ret);

	bq->monitoring_task = kthread_run(monitoring_thread, bq,
			"bq2416x_monitord");
	if (IS_ERR(bq->monitoring_task)) {
		ret = (int)bq->monitoring_task;
		goto thread_failed;
	}
	pr_info("Monitoring thread is successfully started\n");
	ret = 0;
	return ret;

thread_failed:
	power_supply_unregister(usb);
usb_failed:
	power_supply_unregister(battery);
battery_failed:
	power_supply_unregister(ac);
	return ret;
}


static int __devinit bq2416x_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter;
	unsigned retry = 0;
	struct plat_i2c_bq2416x_data *plat = client->dev.platform_data;

	struct bq2416x_priv *bq;
	int ret;

	adapter = to_i2c_adapter(client->dev.parent);

	ret = i2c_check_functionality(adapter,
			I2C_FUNC_SMBUS_BYTE | I2C_FUNC_SMBUS_BYTE_DATA);
	if (!ret) {
		dev_err(&client->dev, "i2c_check_functionality failed\n");
		return -ENODEV;
	}
	do {
		msleep(2);
		ret = i2c_smbus_read_byte_data(client, BQ24163_ID);
		if (ret >= 0)
			break;
		dev_err(&client->dev, "i2c_smbus_read_byte_data failed, retry=%d\n", retry);
		if  (retry++ > 6)
			return -EIO;
	} while (1);

	if ((ret & 0xf8) != 0x40) {
		dev_err(&client->dev, "id match failed %x != 0x40\n", ret);
		return -EIO;
	}

	bq = kzalloc(sizeof(struct bq2416x_priv),GFP_KERNEL);
	if (!bq) {
		dev_err(&client->dev, "Couldn't allocate memory\n");
		return -ENOMEM;
	}
	bq->policy = (plat) ? plat->policy : default_policy;

	init_waitqueue_head(&bq->sample_waitq);
	bq->client = client;
	bq->gp = (plat) ? plat->gp : -1;
	bq->irq = (bq->gp >= 0) ? gpio_to_irq(bq->gp) : 0;

	printk(KERN_INFO "%s: bq2416x irq=%i gp=%i\n", __func__, bq->irq, bq->gp);
	if (bq->irq > 0) {
		ret = request_irq(bq->irq, &bq_interrupt, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, client_name, bq);
		if (ret) {
			printk(KERN_ERR "%s: request_irq failed, irq:%i\n", client_name,bq->irq);
			goto free_bq;
		}
	}
	i2c_set_clientdata(client, bq);
	ret = bq_init(bq);
	if (ret)
		goto free_irq;
	return ret;

free_irq:
	if (bq->irq > 0)
		free_irq(bq->irq, bq);
free_bq:
	kfree(bq);
	return ret;
}

static int __devexit bq2416x_remove(struct i2c_client *client)
{
	struct bq2416x_priv *bq = i2c_get_clientdata(client);
	device_remove_file(&client->dev, &dev_attr_charger_state);
	if (bq) {
		if (!IS_ERR(bq->monitoring_task))
		        kthread_stop(bq->monitoring_task);
		if (bq->irq)
			free_irq(bq->irq, bq);
		power_supply_unregister(&bq->battery);
		power_supply_unregister(&bq->ac);
		power_supply_unregister(&bq->usb);
		kfree(bq);
	}
	return 0;
}

static const struct i2c_device_id bq2416x_id[] = {
	{DRV_NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, bq2416x_id);

static struct i2c_driver bq2416x_driver = {
	.driver = {
		   .name = DRV_NAME,
		   .owner = THIS_MODULE,
		   },
	.probe = bq2416x_probe,
	.remove = __devexit_p(bq2416x_remove),
	.id_table = bq2416x_id,
};

static int __init bq2416x_init(void)
{
	/* register driver */
	int res = i2c_add_driver(&bq2416x_driver);
	if (res < 0) {
		printk(KERN_INFO "add bq2416x i2c driver failed\n");
		return -ENODEV;
	}
	printk(KERN_INFO "add bq2416x i2c driver\n");
	return res;
}
module_init(bq2416x_init);

static void __exit bq2416x_exit(void)
{
	printk(KERN_INFO "remove bq2416x i2c driver.\n");
	i2c_del_driver(&bq2416x_driver);
}
module_exit(bq2416x_exit);

MODULE_AUTHOR("Boundary Devices, Inc.");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Power supply/charger driver for bq2416x");
