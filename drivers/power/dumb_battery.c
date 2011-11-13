/*
 * Driver for a bare battery (with no charge monitoring).
 *
 * This is used as a placeholder for a battery that can be charged
 * by the LTC1960 but can't report its status. It takes its voltage,
 * current and time-to-charge from platform data so it can be easily
 * changed.
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
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/dumb_battery.h>

static enum power_supply_property dumb_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CAPACITY,
};

struct dumb_info {
	struct platform_device *pdev ;
	struct power_supply	power_supply;
	int			cur_status ;
	int			start_level ;
	unsigned long		start_jif ;
	int			cur_level ;
};

inline struct dumb_battery_platform_t *plat( struct dumb_info *info) {
	return ((struct dumb_battery_platform_t *)info->pdev->dev.platform_data);
}

static int dumb_get_property(struct power_supply *psy,
	enum power_supply_property psp,
	union power_supply_propval *val)
{
	int ret = -EINVAL ;
	struct dumb_info *dumb_device = container_of(psy,struct dumb_info,power_supply);

	switch (psp) {
		case POWER_SUPPLY_PROP_STATUS:
                        val->intval =  dumb_device->cur_status;
			ret = 0;
			break;
		case POWER_SUPPLY_PROP_HEALTH:
                        val->intval =  POWER_SUPPLY_HEALTH_GOOD;
			ret = 0;
			break;
		case POWER_SUPPLY_PROP_PRESENT:
			val->intval = 1 ;
			ret = 0;
			break;
		case POWER_SUPPLY_PROP_CAPACITY: {
			/* update charge level if needed */
                        struct dumb_battery_platform_t *pdata = plat(dumb_device);
			if (POWER_SUPPLY_STATUS_CHARGING == dumb_device->cur_status) {
				dumb_device->cur_level = dumb_device->start_level +
							 ((jiffies-dumb_device->start_jif)*100)
				                          /(pdata->charge_sec*CONFIG_HZ);
				if (dumb_device->cur_level >= 100) {
					dumb_device->cur_status=POWER_SUPPLY_STATUS_FULL ;
					dumb_device->cur_level=100 ;
					printk (KERN_ERR "%s: battery full\n", __func__ );
				}
			} else if (POWER_SUPPLY_STATUS_DISCHARGING == dumb_device->cur_status) {
				dumb_device->cur_level = dumb_device->start_level -
							 ((jiffies-dumb_device->start_jif)*100)
				                          /(pdata->discharge_sec*CONFIG_HZ);
				if (0 < dumb_device->cur_level)
					dumb_device->cur_level=0;
			}
                        val->intval =  dumb_device->cur_level ;
			ret = 0;
			break;
		}
                default:
			printk (KERN_ERR
				"%s: INVALID property\n", __func__);
			ret = -EINVAL;
	}
	return ret ;
}

static int dumb_set_property(struct power_supply *psy,
			     enum power_supply_property psp,
			     const union power_supply_propval *val)
{
	int ret = -EINVAL ;
	struct dumb_info *dumb_device = container_of(psy,struct dumb_info,power_supply);

	printk (KERN_ERR "%s: property %d\n", __func__, psp );
	switch (psp) {
		case POWER_SUPPLY_PROP_STATUS:
			ret = 0;
			if (dumb_device->cur_status != val->intval) {
				dumb_device->cur_status = val->intval ;
				switch (val->intval) {
					case POWER_SUPPLY_STATUS_CHARGING: {
						dumb_device->start_level = dumb_device->cur_level ;
						dumb_device->start_jif = jiffies ;
						printk (KERN_ERR "%s: now charging\n", __func__ );
						break;
					}
					case POWER_SUPPLY_STATUS_DISCHARGING: {
						dumb_device->start_level = dumb_device->cur_level ;
						dumb_device->start_jif = jiffies ;
						printk (KERN_ERR "%s: now discharging\n", __func__ );
						break;
					}
					default: {
						printk (KERN_ERR "%s: neither charging nor discharging\n", __func__ );
					}
				}
			}
			break;
                default:
			printk (KERN_ERR
				"%s: INVALID property\n", __func__);
			ret = -EINVAL;
	}
	return ret ;
}

static int dumb_property_is_writeable
	( struct power_supply *psy,
          enum power_supply_property psp )
{
	int ret = (POWER_SUPPLY_PROP_STATUS == psp);
	return ret ;
}

static int dumb_probe(struct platform_device *pdev)
{
	struct dumb_info *dumb_device;
	int rc;

	if (0 != pdev->dev.platform_data) {
                struct dumb_battery_platform_t *pdata = (struct dumb_battery_platform_t *)pdev->dev.platform_data ;

		dumb_device = kzalloc(sizeof(struct dumb_info), GFP_KERNEL);
		if (!dumb_device)
			return -ENOMEM;
		dumb_device->pdev = pdev ;
		dumb_device->power_supply.name = "dumb";
		dumb_device->power_supply.type = POWER_SUPPLY_TYPE_BATTERY;
		dumb_device->power_supply.properties = dumb_properties;
		dumb_device->power_supply.num_properties =
			ARRAY_SIZE(dumb_properties);
		dumb_device->power_supply.get_property = dumb_get_property;
		dumb_device->power_supply.set_property = dumb_set_property;
		dumb_device->power_supply.property_is_writeable = dumb_property_is_writeable ;
		dumb_device->cur_status = POWER_SUPPLY_STATUS_UNKNOWN ;
		dumb_device->cur_level = pdata->init_level ;

		rc = power_supply_register(&pdev->dev, &dumb_device->power_supply);
		if (rc) {
			dev_err(&pdev->dev,
				"%s: Failed to register power supply\n", __func__);
			kfree(dumb_device);
			return rc;
		}

		dev_info(&pdev->dev,
			"%s: battery gas gauge device registered: init %d, tt chg %ds, tt dis %ds\n",
			 pdev->name, pdata->init_level, pdata->charge_sec, pdata->discharge_sec);

		return 0;
	} else {
		printk (KERN_ERR "%s: missing platform data (see linux/dumb_battery.h)", __func__ );
		return -EINVAL ;
	}
}

static int dumb_remove(struct platform_device *pdev)
{
	struct power_supply *psy = power_supply_get_by_name("dumb");
	if (psy) {
		struct dumb_info *dumb_device = container_of(psy,struct dumb_info,power_supply);
		power_supply_unregister(&dumb_device->power_supply);
		kfree(dumb_device);
	}

	return 0;
}

#if defined CONFIG_PM
static int dumb_suspend(struct platform_device *pdev,
	pm_message_t state)
{
	printk (KERN_ERR "%s\n", __func__ );
	return 0;
}

static int dumb_resume(struct platform_device *pdev)
{
	printk (KERN_ERR "%s\n", __func__ );
	return 0;
}
#else
#define dumb_suspend		NULL
#define dumb_resume		NULL
#endif

static struct platform_driver dumb_battery_driver = {
	.probe		= dumb_probe,
	.remove		= dumb_remove,
	.suspend	= dumb_suspend,
	.resume		= dumb_resume,
	.driver = {
		.name	= "dumb_battery",
	},
};

static int __init dumb_battery_init(void)
{
	printk (KERN_ERR "%s\n", __func__ );
	return platform_driver_register(&dumb_battery_driver);
}
module_init(dumb_battery_init);

static void __exit dumb_battery_exit(void)
{
	printk (KERN_ERR "%s\n", __func__ );
	platform_driver_unregister(&dumb_battery_driver);
}
module_exit(dumb_battery_exit);

MODULE_DESCRIPTION("BQ20z75 battery monitor driver");
MODULE_LICENSE("GPL");
