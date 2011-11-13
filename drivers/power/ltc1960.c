/*
 * Power controller for LTC1960 from Linear Technologies
 *
 * Copyright (c) 2011, Boundary Devices
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/spi/spi.h>
#include <linux/spi/ltc1960.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>

#define POLL_INTERVAL (CONFIG_HZ/2)

#define CHARGE_BAT1	(1<<7)
#define CHARGE_BAT2	(1<<6)
#define CHARGE_CMD	0x0E

#define POWERPATH_CMD	0x0C
#define POWER_BY_BAT1	(1<<7)
#define POWER_BY_BAT2	(1<<6)
#define POWER_BY_DC	(1<<5)

#define STATUSBIT_CH	10
#define STATUSBIT_PF	11
#define STATUSBIT_DC	12
#define STATUSBIT_LP	13
#define STATUSBIT_FA	14

#define STATUSBIT_A2	0
#define STATUSBIT_A1	1
#define STATUSBIT_A0	2

#define STATUSMASK_A2	(1<<STATUSBIT_A2)
#define STATUSMASK_A1	(1<<STATUSBIT_A1)
#define STATUSMASK_A0	(1<<STATUSBIT_A0)
#define STATUSMASK_CH	(1<<STATUSBIT_CH)
#define STATUSMASK_PF	(1<<STATUSBIT_PF)
#define STATUSMASK_DC	(1<<STATUSBIT_DC)
#define STATUSMASK_LP	(1<<STATUSBIT_LP)
#define STATUSMASK_FA	(1<<STATUSBIT_FA)

#define STATUSBIT_MASK (STATUSMASK_A2 \
			|STATUSMASK_A1 \
			|STATUSMASK_A0 \
			|STATUSMASK_CH \
			|STATUSMASK_PF \
			|STATUSMASK_DC \
			|STATUSMASK_LP \
			|STATUSMASK_FA)

#define VOLTS_12_8	0xa877
#define	VOLTS_12_6	0xa887
#define CUR_1_5		0xa077
#define CUR_2_0		0xe017
#define LOW_CURRENT	0x1000

struct supported_setting_t {
	unsigned 	uva ; /* microvolts or microamps */
	unsigned short	value ;
};

static struct supported_setting_t supported_voltages[] = {
	{ 12800000, VOLTS_12_8 }
,	{ 12600000, VOLTS_12_6 }
};

static struct supported_setting_t supported_currents[] = {
	{ 2000000, CUR_2_0 }
,	{ 1500000, CUR_1_5 }
};

static int find_voltage(unsigned uv) {
	int i ;
	for (i=0 ; i < ARRAY_SIZE(supported_voltages); i++) {
		if (uv == supported_voltages[i].uva) {
			return supported_voltages[i].value ;
		}
	}
	return -1 ;
}

static int find_current(unsigned ua) {
	int i ;
	for (i=0 ; i < ARRAY_SIZE(supported_currents); i++) {
		if (ua == supported_currents[i].uva) {
			return supported_currents[i].value ;
		}
	}
	return -1 ;
}

/*
 * battery status is kept in a pair of bit fields in the
 * battery_state field:
 *
 *	bits [0..1]	first battery
 *	bits [2..3]	second battery
 * 	bit 4		DC input present
 *
 * DC input flag (1==present) is in bit 4
 *
 * the state of each battery is one of the following values
 *	0	- not present
 *	1	- empty
 *	2	- partial
 *	3	- full
 *
 */
#define BAT_NOT_PRESENT	0
#define BAT_EMPTY	1
#define BAT_PARTIAL	2
#define BAT_FULL	3
#define BAT_MASK	3

#define BAT_STATE_SHIFT 2
#define BAT_STAT(stat,which) ((stat&(BAT_MASK<<(which*BAT_STATE_SHIFT)))>>(which*BAT_STATE_SHIFT))

#define BAT_DCINPUT_SHIFT 4
#define BAT_DCINPUT_MASK (1<<BAT_DCINPUT_SHIFT)

/*
 * run/charge state (output state) is kept in a set of
 * bitfields in the field 'run_state':
 *
 *	bits [0..1]	run from field
 *			0 == first battery
 *			1 == second battery
 *			2 == DC
 *			3 == invalid
 *	bit 2		charge first
 *	bit 3		charge second
 *
 */
#define RUN_FROM_FIRST		0
#define RUN_FROM_SECOND		1
#define RUN_FROM_DC		2
#define RUN_FROM_INVALID	3
#define RUN_FROM_MASK		3

#define CHARGE_FIRST_MASK	(1<<2)
#define CHARGE_SECOND_MASK	(1<<3)
#define CHARGE_MASK		(CHARGE_FIRST_MASK|CHARGE_SECOND_MASK)

struct lt1960_data_t {
        struct spi_device 		*spi ;
        struct ltc1960_platform_data_t	*pdata ;
	struct proc_dir_entry 		*proc ;
	struct power_supply		power_supply;
	struct delayed_work 		timer_work ;
	uint16_t			last_stat ;
	unsigned			last_uv ;
	unsigned			last_ua ;
	unsigned			battery_state ;
	unsigned			run_state ;
	unsigned long			end_trickle ;
	unsigned char			charge_cmd ; // need to send this on every cycle to keep watchdog alive
};

static void print_battery_info(struct ltc1960_battery_info_t *b)
{
	printk (KERN_ERR "name:\t%s\n", b->name ? b->name : "<unspecified>");
	printk (KERN_ERR "charge_uV:\t%d\n", b->charge_uv);
	printk (KERN_ERR "charge_uA:\t%d\n", b->charge_ua);
	printk (KERN_ERR "trickle (s):\t%d\n", b->trickle_seconds);
}

static enum power_supply_property ltc1960_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CAPACITY,
};

static int ltc1960_get_property(struct power_supply *psy,
	enum power_supply_property psp,
	union power_supply_propval *val)
{
	int ret;
	struct lt1960_data_t *data = container_of(psy,
						  struct lt1960_data_t,
						  power_supply);

	switch (psp) {
		case POWER_SUPPLY_PROP_PRESENT:
			val->intval = 1 ;
			ret = 0;
			break;

		case POWER_SUPPLY_PROP_STATUS:
			if (0 != (data->run_state&CHARGE_MASK)) {
				val->intval =  POWER_SUPPLY_STATUS_CHARGING;
			} else if (data->battery_state & BAT_DCINPUT_MASK) {
				val->intval =  POWER_SUPPLY_STATUS_NOT_CHARGING;
			} else
				val->intval =  POWER_SUPPLY_STATUS_DISCHARGING;
			break;

		case POWER_SUPPLY_PROP_CAPACITY: {
			int i ;
			int div = 0 ;
			unsigned cap = 0 ;
			for (i = 0 ; i < ARRAY_SIZE(data->pdata->batteries); i++) {
				if (data->pdata->batteries[i]
				    &&
				    data->pdata->batteries[i]->name) {
					union power_supply_propval pval ;
					struct power_supply *b= power_supply_get_by_name(data->pdata->batteries[i]->name);
					if (b && b->get_property
					    &&
					    (0 == b->get_property(b,POWER_SUPPLY_PROP_CAPACITY,&pval))) {
						printk (KERN_ERR "%s: bat %d, cap %d\n", __func__, i, pval.intval);
						cap += pval.intval ;
						div++ ;
					}
				}
			}
			if (0 < div) {
				val->intval = (cap/div);
			}
			ret = 0 ;
			break;
		}

		default:
			dev_err(&data->spi->dev,
				"%s: INVALID property\n", __func__);
			return -EINVAL;
	}

#if 0
	dev_dbg(&data->spi->dev,
		"%s: property = %d, value = %d\n", __func__, psp, val->intval);
#endif
	return 0;
}

/*
 * returns byte value or error (<0)
 */
static int write1(struct spi_device *spi, uint8_t value)
{
	int retval ;
	uint8_t tx[1] = { value };
	uint8_t rx[1] = { 0x55 };
	struct spi_transfer xfer = {
		.tx_buf		= tx,
		.rx_buf		= rx,
		.len		= sizeof(tx),
	};
	struct spi_message msg;
//	printk (KERN_ERR "%s: 0x%02x\n", __func__, value );
	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);

	retval = spi_sync(spi, &msg);
	if (0 <= retval) {
//		printk (KERN_ERR "%s: spi_sync(%d): %02x\n", __func__, retval, rx[0] );
		return rx[0];
	} else {
		printk (KERN_ERR "%s: spi_sync(%d)\n", __func__, retval );
		return retval ;
	}
}

/*
 * returns byte value or error (<0)
 */
static int write2(struct spi_device *spi, uint16_t value)
{
	int retval ;
	uint16_t rx = { 0x55aa };
	struct spi_transfer xfer = {
		.tx_buf		= &value,
		.rx_buf		= &rx,
		.len		= sizeof(value),
	};
	struct spi_message msg;
	printk (KERN_ERR "%s: 0x%04x\n", __func__, value );
	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);

	retval = spi_sync(spi, &msg);
	if (0 <= retval) {
		printk (KERN_ERR "%s: spi_sync(%d): %04x\n", __func__, retval, rx );
		return rx ;
	} else {
		printk (KERN_ERR "%s: spi_sync(%d)\n", __func__, retval );
		return retval ;
	}
}

static void write2_and_wait(struct spi_device *spi, uint16_t value)
{
	unsigned i = 0 ;
	int retval ;
	if (0 < (retval = write2(spi,value))) {
		while ((0 < (retval = write1(spi,0x01)))
		       &&
		       (0x80 != retval)
		       &&
		       (10 > i++)){
			if (1 < i) {
				printk (KERN_ERR "%s: waiting for ack(%d)\n", __func__, retval);
			}
			value = 0 ;
		}
		if (1 < i) {
			printk (KERN_ERR "%s: abort after %d retries\n", __func__, i);
		}
	}
}

static void set_voltage(struct spi_device *spi, unsigned uv)
{
	int value = find_voltage(uv);
	if (0 < value) {
		printk (KERN_ERR "%s: setting voltage to %u uV\n", __func__, uv);
		write2_and_wait(spi,value);
	} else if (0 != uv){
		printk (KERN_ERR "%s: unsupported voltage %u\n", __func__, uv);
	}
}

static void set_current(struct spi_device *spi, unsigned ua,int trickle)
{
	int value = find_current(ua);
	if (0 < value) {
		if (trickle)
			value |= LOW_CURRENT ;
		printk (KERN_ERR "%s: setting current to %u uA (%s)\n", __func__, ua, trickle?"trickle":"full");
		write2_and_wait(spi,value);
	} else if (0 != ua){
		printk (KERN_ERR "%s: unsupported current %u\n", __func__, ua);
	}
}

static unsigned compute_run_state(unsigned bat_state)
{
	unsigned rval = 0 ;
	if (bat_state & BAT_DCINPUT_MASK) {
		rval = RUN_FROM_DC ;
		if (BAT_FULL != BAT_STAT(bat_state,0)) {
			rval |= CHARGE_FIRST_MASK ;
		} else if (BAT_FULL != BAT_STAT(bat_state,1)) {
			rval |= CHARGE_SECOND_MASK ;
		}
	} // DC input is present, choose charge, run from DC
	else {
		rval &= ~CHARGE_MASK ;
		if (BAT_EMPTY < BAT_STAT(bat_state,1)) {
			rval |= RUN_FROM_SECOND ;
		} else
			rval |= RUN_FROM_FIRST ;
	} // No DC input... no charge, choose battery to run from

	return rval ;
}

static void update_run_state(struct lt1960_data_t *data, unsigned run_state)
{
	struct power_supply *bats[2] = {0,0};
	unsigned i;
	printk (KERN_ERR "%s: run state %04x->%04x\n", __func__, data->run_state, run_state);
	for (i = 0 ; i < ARRAY_SIZE(bats); i++) {
		if (data->pdata->batteries[i]
		    &&
		    data->pdata->batteries[i]->name) {
			bats[i] = power_supply_get_by_name(data->pdata->batteries[i]->name);
		}
	}
	if ((run_state&RUN_FROM_MASK) != (data->run_state&RUN_FROM_MASK)) {
		unsigned cmd = POWERPATH_CMD|POWER_BY_DC ; /* always run from DC if present */
printk (KERN_ERR "%s: set run state to 0x%02x\n", __func__, run_state & RUN_FROM_MASK);
		switch(run_state&RUN_FROM_MASK){
			case RUN_FROM_FIRST:
				cmd |= POWER_BY_BAT2 ; 	/* battery channels are swapped from LTC1960 */
				break ;
			case RUN_FROM_SECOND:
				cmd |= POWER_BY_BAT1 ;
				break ;
		}
		printk (KERN_ERR "power cmd: %x\n",cmd);
		write1(data->spi,cmd);
	}
	if ((run_state&CHARGE_MASK) != (data->run_state&CHARGE_MASK)) {
		unsigned volts=0 ;
		unsigned amps=0;
		int trickle_sec=0 ;
		data->charge_cmd = CHARGE_CMD ;
printk (KERN_ERR "%s: set charge state to %x\n", __func__, run_state & CHARGE_MASK);
		if (CHARGE_FIRST_MASK&run_state){
			data->charge_cmd |= CHARGE_BAT2 ;
			volts = data->pdata->batteries[0]->charge_uv ;
			amps = data->pdata->batteries[0]->charge_ua ;
                        trickle_sec = data->pdata->batteries[0]->trickle_seconds ;
		} else if ((CHARGE_SECOND_MASK&run_state)
			   &&
			   data->pdata->batteries[1]) {
			data->charge_cmd |= CHARGE_BAT1 ;
			volts = data->pdata->batteries[1]->charge_uv ;
			amps = data->pdata->batteries[1]->charge_ua ;
                        trickle_sec = data->pdata->batteries[1]->trickle_seconds ;
		}
		set_voltage(data->spi,volts);
		set_current(data->spi,amps,(0 != trickle_sec));
		printk (KERN_ERR "charge cmd: %x\n",data->charge_cmd);
		write1(data->spi,data->charge_cmd);
		data->last_uv = volts ;
		data->last_ua = amps ;
		if (trickle_sec) {
			data->end_trickle = jiffies + (trickle_sec*CONFIG_HZ);
			if (unlikely(0 == data->end_trickle)) {
				data->end_trickle++ ;
			}
		}
		else
			data->end_trickle = 0 ;
	}
	for (i = 0 ; i < ARRAY_SIZE(bats); i++) {
		struct power_supply *b = bats[i];
		if (b && b->set_property) {
                        union power_supply_propval val ;
			if((CHARGE_FIRST_MASK<<i)&run_state) {
				val.intval = POWER_SUPPLY_STATUS_CHARGING;
			} /* charging this battery */
			else if (i ==(run_state&RUN_FROM_MASK)) {
				val.intval = POWER_SUPPLY_STATUS_DISCHARGING;
			} /* discharging this battery */
			else
				val.intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
                        b->set_property(b,POWER_SUPPLY_PROP_STATUS,&val);
		}
	}
	data->run_state = run_state ;
}

static void update_status(struct lt1960_data_t *data, uint16_t status,unsigned bat_state)
{
	uint16_t diffs = status^data->last_stat ;
	if (diffs&STATUSBIT_MASK) {
		printk (KERN_ERR "%s: stat 0x%04x (mask %04x), change 0x%04x\n", __func__, status, STATUSBIT_MASK, diffs);
		data->last_stat = status ;
		diffs &= STATUSBIT_MASK ;
		while (diffs) {
			int bit = ffs(diffs)-1 ;
			int mask = (1<<bit);
			int val = 0 != (status&(1<<bit));
			switch (bit) {
				case STATUSBIT_CH: printk (KERN_ERR "CHG: %d\n", val); break ;
				case STATUSBIT_PF: printk (KERN_ERR "PF: %d\n", val); break ;
				case STATUSBIT_DC: {
					printk (KERN_ERR "DC: %d\n", val);
					break ;
				}
				case STATUSBIT_LP: printk (KERN_ERR "LP: %d\n", val); break ;
				case STATUSBIT_FA: printk (KERN_ERR "FAULT: %d\n", val); break ;
			}
			diffs &= ~mask ;
		}
	} else if (0 != (status&STATUSMASK_DC)) {
		write1(data->spi,data->charge_cmd);
		if (data->end_trickle) {
			if (time_is_before_jiffies(data->end_trickle)) {
				set_current(data->spi,data->last_ua,0);
                                data->end_trickle = 0 ;
			}
		}
	}

	bat_state &= ~BAT_DCINPUT_MASK ;
	bat_state |= ((0 != (status&STATUSMASK_DC))<<BAT_DCINPUT_SHIFT);
	if (data->battery_state != bat_state) {
		unsigned run_state = compute_run_state(bat_state);
		printk (KERN_ERR "%s: battery state %04x->%04x, run state %04x\n", __func__, data->battery_state, bat_state, run_state);
                data->battery_state = bat_state;
		if (data->run_state != run_state) {
			update_run_state(data,run_state);
		}
	}
}

static enum power_supply_property required_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CAPACITY,
};

static unsigned update_battery_state (struct lt1960_data_t *data)
{
        unsigned bat ;
	unsigned state = 0 ;
	for (bat=0; bat<2;bat++) {
		unsigned this_bat=0 ;
		if (data->pdata->batteries[bat]
		    &&
		    data->pdata->batteries[bat]->name) {
			struct power_supply *b=power_supply_get_by_name(data->pdata->batteries[bat]->name);
			if (b && b->get_property) {
				int i ;
				int values[ARRAY_SIZE(required_properties)];
				for (i=0; i<ARRAY_SIZE(required_properties);i++) {
					union power_supply_propval pval ;
					int rv ;
					if (0 == (rv=b->get_property(b,required_properties[i],&pval))) {
						values[i]=pval.intval ;
					} else {
//						printk (KERN_DEBUG "%s: error %d reading property %d from battery %d\n", __func__, rv, i, bat);
						break;
					}
				}
				if (ARRAY_SIZE(required_properties) == i) {
					if (values[1]) {
						if (values[2]<10) {
							this_bat = BAT_EMPTY ;
						} else if (values[2]<100) {
							this_bat = BAT_PARTIAL;
						} else {
							this_bat = BAT_FULL ;
						}
					}
				}
			}
		}
		state = state | (this_bat << (bat*BAT_STATE_SHIFT));
	}
	return state ;
}

static void timeout_fn(struct work_struct *work) {
        struct lt1960_data_t *data = container_of(
						  work,
						  struct lt1960_data_t,
						  timer_work.work);
	int retval ;
	uint8_t tx[2] = { 0x04, 0x05 };
	uint16_t rx   = { 0xaa55 };
	struct spi_transfer xfer = {
		.tx_buf		= tx,
		.rx_buf		= &rx,
		.len		= sizeof(tx),
	};
	struct spi_message msg;
	schedule_delayed_work(&data->timer_work, POLL_INTERVAL);
	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);

	retval = spi_sync(data->spi, &msg);
	if (0 <= retval) {
		update_status(data,rx,update_battery_state (data));
	} else
		printk (KERN_ERR "%s: spi_sync(%d)\n", __func__, retval );
}

static int read_proc(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	struct lt1960_data_t *ltc = (struct lt1960_data_t *)data ;
	return snprintf(page,count,
			"stat: %04x\n"
			"uv: %u\n"
			"ua: %u\n"
			"bs: %04x\n"
			"rs: %04x\n"
			"cmd: %02x\n"
			"trickstart: %08lx\n",
			ltc->last_stat,ltc->last_uv,ltc->last_ua,
			ltc->battery_state, ltc->run_state, ltc->charge_cmd,
			ltc->end_trickle);
}

static int __devinit ltc1960_probe(struct spi_device *spi)
{
	int i ;
	int retval = 0 ;
	printk(KERN_INFO "Device %s probed: pdata = %p\n", dev_name(&spi->dev),	spi->dev.platform_data);
	if (spi->dev.platform_data) {
                struct lt1960_data_t *data ;
		struct ltc1960_platform_data_t *pdata = spi->dev.platform_data ;
		data = kzalloc(sizeof(*data), GFP_KERNEL);
		data->spi = spi ;
		data->run_state = RUN_FROM_INVALID|CHARGE_MASK ; /* force change */
		data->pdata = pdata ;
		data->power_supply.name = "battery";
		data->power_supply.type = POWER_SUPPLY_TYPE_BATTERY;
		data->power_supply.properties = ltc1960_properties ;
		data->power_supply.num_properties =
			ARRAY_SIZE(ltc1960_properties);
		data->power_supply.get_property = ltc1960_get_property;
		spi_set_drvdata(spi,data);

		for (i = 0 ; i < ARRAY_SIZE(pdata->batteries); i++) {
			if (pdata->batteries[i]) {
				struct power_supply *bat = power_supply_get_by_name(pdata->batteries[i]->name);
				print_battery_info(pdata->batteries[i]);
				if (bat) {
					printk( KERN_ERR "%s: found %s(%p)\n", __func__, bat->name,bat);
					printk (KERN_ERR "%s: getprop %p\n", __func__, bat->get_property);
					printk (KERN_ERR "%s: setprop %p\n", __func__, bat->set_property);
					printk (KERN_ERR "%s: prop is writable %p\n", __func__, bat->property_is_writeable);
				}
			}
			else
				printk (KERN_ERR "%s: no battery %d\n", __func__, i);
		}
		printk (KERN_ERR "%s: speed %u, chip select %u, mode %u, bpw %u\n",
			__func__, spi->max_speed_hz, spi->chip_select, spi->mode, spi->bits_per_word);
		retval = power_supply_register(&spi->dev, &data->power_supply);
		if (0 == retval) {
			printk (KERN_ERR "%s: power supply registered\n", __func__ );
			INIT_DELAYED_WORK(&data->timer_work,timeout_fn);
			schedule_delayed_work(&data->timer_work, POLL_INTERVAL);
			printk (KERN_ERR "%s: volts(12.6)\n", __func__ );
			write2_and_wait(spi,VOLTS_12_6);
			printk (KERN_ERR "%s: current(1.5)\n", __func__ );
			write2_and_wait(spi,CUR_1_5);
                        data->proc = create_proc_read_entry("ltc1960",0444,0,read_proc,data);
		} else {
			dev_err(&spi->dev,
				"%s: Failed to register power supply\n", __func__);
			kfree(data);
			spi_set_drvdata(spi,0);
		}
	} else {
		printk (KERN_ERR "%s: missing platform data\n", __func__ );
		retval = -EINVAL ;
	}
	return retval ;
}

static int __devexit ltc1960_remove(struct spi_device *spi)
{
	struct lt1960_data_t *data = spi_get_drvdata(spi);
	if (data) {
		if (data->proc)
                        remove_proc_entry("ltc1960",0);
		cancel_delayed_work(&data->timer_work);
		power_supply_unregister(&data->power_supply);
		kfree(data);
	}
	printk(KERN_INFO "Device %s removed\n", dev_name(&spi->dev));
	return 0 ;
}

static int ltc1960_suspend(struct spi_device *spi, pm_message_t message)
{
	printk (KERN_ERR "%s\n", __func__ );
	return 0 ;
}

static int ltc1960_resume(struct spi_device *spi)
{
	printk (KERN_ERR "%s\n", __func__ );
	return 0 ;
}

static struct spi_driver ltc1960_driver = {
	.driver = {
		   .name = "ltc1960",
		   .bus = &spi_bus_type,
		   .owner = THIS_MODULE,
		   },
	.probe = ltc1960_probe,
	.remove = __devexit_p(ltc1960_remove),
	.suspend = ltc1960_suspend,
	.resume = ltc1960_resume,
};

/*
 * Initialization and Exit
 */
static int __init ltc1960_init(void)
{
	printk (KERN_ERR "%s\n", __func__ );
	return spi_register_driver(&ltc1960_driver);
}
module_init(ltc1960_init);

static void __exit ltc1960_exit(void)
{
	printk (KERN_ERR "%s\n", __func__ );
        spi_unregister_driver(&ltc1960_driver);
}
module_exit(ltc1960_exit);

MODULE_DESCRIPTION("LTC1960 battery charger driver");
MODULE_LICENSE("GPL");
