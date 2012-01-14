/*
 * da9052-charger.c  --  Batttery Charger Driver for Dialog DA9052
 *
 * Copyright(c) 2012 Boundary Devices
 *
 * Author: Boundary Devices <eric.nelson@boundarydevices.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/uaccess.h>
#include <linux/jiffies.h>
#include <linux/power_supply.h>
#include <linux/platform_device.h>
#include <linux/freezer.h>

#include <linux/mfd/da9052/da9052.h>
#include <linux/mfd/da9052/reg.h>
#include <linux/mfd/da9052/bat.h>

#define DA9052_CHG_DEVICE_NAME	"da9052-chg"

static const char  __initdata banner[] = KERN_INFO "DA9052 CHG, (c) 2012, Boundary Devices\n";

struct charger_device_t {
	struct da9052			*da9052;
	struct da9052_bat_platform_data	*bat_pdata;
	struct delayed_work		work;
	struct power_supply		wall_psy;
	struct power_supply		usb_psy;
	struct da9052_eh_nb		dcin_det_data;
	struct da9052_eh_nb		dcin_rem_data;
	struct da9052_eh_nb		vbus_det_data;
	struct da9052_eh_nb		vbus_rem_data;
	u8				status_a ;
};

static enum power_supply_property da902_chg_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static int da9052_read(struct da9052 *da9052, u8 reg_address, u8 *reg_data)
{
	struct da9052_ssc_msg msg;
	int ret;

	msg.addr = reg_address;
	msg.data = 0;

	da9052_lock(da9052);
	ret = da9052->read(da9052, &msg);
	if (ret)
		goto ssc_comm_err;
	da9052_unlock(da9052);

	*reg_data = msg.data;
	return 0;
ssc_comm_err:
	da9052_unlock(da9052);
	return ret;
}

static void common_event_handler
	(struct charger_device_t *charger, unsigned int event)
{
	u8 reg_data ;
	int ret = da9052_read(charger->da9052, DA9052_STATUSA_REG, &reg_data);
	if (ret) {
		printk (KERN_ERR "%s: error %d reading status_a\n", __func__, ret);
	} else {
		reg_data &= DA9052_STATUSA_DCINDET|DA9052_STATUSA_VBUSDET ;
		if (charger->status_a != reg_data) {
			u8 changed = charger->status_a ^ reg_data ;
			charger->status_a = reg_data ;
			if (changed & DA9052_STATUSA_DCINDET)
                                power_supply_changed(&charger->wall_psy);
			if (changed & DA9052_STATUSA_VBUSDET)
                                power_supply_changed(&charger->usb_psy);
		}
	}
}

static void dcin_det_handler(struct da9052_eh_nb *eh_data, unsigned int event)
{
	common_event_handler (container_of(eh_data,
					   struct charger_device_t,
                                           dcin_det_data),
			      event);
}
static void dcin_rem_handler(struct da9052_eh_nb *eh_data, unsigned int event)
{
	common_event_handler (container_of(eh_data,
					   struct charger_device_t,
                                           dcin_rem_data),
			      event);
}
static void vbus_det_handler(struct da9052_eh_nb *eh_data, unsigned int event)
{
	common_event_handler (container_of(eh_data,
					   struct charger_device_t,
                                           vbus_det_data),
			      event);
}
static void vbus_rem_handler(struct da9052_eh_nb *eh_data, unsigned int event)
{
	common_event_handler (container_of(eh_data,
					   struct charger_device_t,
                                           vbus_rem_data),
			      event);
}

static int da9052_dcin_get_property
	( struct power_supply *psy,
	  enum power_supply_property psp,
          union power_supply_propval *val)
{
	s32 retval ;

	struct charger_device_t *charger =
	container_of(psy, struct charger_device_t, wall_psy);
printk (KERN_ERR "%s: prop 0x%x\n", __func__, psp);
	if (POWER_SUPPLY_PROP_ONLINE == psp) {
		val->intval = (0 != (charger->status_a & DA9052_STATUSA_DCINDET));
		retval = 0 ;
	} else
		retval = -EINVAL ;
	return retval ;
}

static int da9052_vbus_get_property
	( struct power_supply *psy,
	  enum power_supply_property psp,
          union power_supply_propval *val)
{
	s32 retval ;

	struct charger_device_t *charger =
	container_of(psy, struct charger_device_t, usb_psy);
printk (KERN_ERR "%s: prop 0x%x\n", __func__, psp);
	if (POWER_SUPPLY_PROP_ONLINE == psp) {
		val->intval = (0 != (charger->status_a & DA9052_STATUSA_VBUSDET));
		retval = 0 ;
	} else
		retval = -EINVAL ;
	return retval ;
}

static s32 __devinit da9052_chg_probe(struct platform_device *pdev)
{
	struct da9052_bat_platform_data *pdata = pdev->dev.platform_data;
	struct charger_device_t *chg_device;
	u8 reg_data;
	int ret ;

	chg_device = kzalloc(sizeof(*chg_device), GFP_KERNEL);
	if (!chg_device)
		return -ENOMEM;

	chg_device->da9052 = dev_get_drvdata(pdev->dev.parent);
	chg_device->bat_pdata = pdata;
	platform_set_drvdata(pdev, chg_device);

	ret = da9052_read(chg_device->da9052, DA9052_STATUSA_REG, &reg_data);
	if (ret)
		goto err_charger_init;
	chg_device->status_a = reg_data ;

	chg_device->dcin_det_data.eve_type = DCIN_DET_EVE;
	chg_device->dcin_det_data.call_back = dcin_det_handler ;
	ret = chg_device->da9052->register_event_notifier(chg_device->da9052, &chg_device->dcin_det_data);
	if (ret) printk (KERN_ERR "%s: Error %d registering dcin_det\n", __func__, ret);
	ret = chg_device->da9052->event_enable(chg_device->da9052, DCIN_DET_EVE);
	if (ret) printk (KERN_ERR "%s: Error %d enabling dcin_det\n", __func__, ret);

	chg_device->dcin_rem_data.eve_type = DCIN_REM_EVE;
	chg_device->dcin_rem_data.call_back = dcin_rem_handler ;
	chg_device->da9052->register_event_notifier(chg_device->da9052, &chg_device->dcin_rem_data);
	if (ret) printk (KERN_ERR "%s: Error %d registering dcin_rem\n", __func__, ret);
	ret = chg_device->da9052->event_enable(chg_device->da9052, DCIN_REM_EVE);
	if (ret) printk (KERN_ERR "%s: Error %d enabling dcin_rem\n", __func__, ret);

	chg_device->vbus_det_data.eve_type = VBUS_DET_EVE;
	chg_device->vbus_det_data.call_back = vbus_det_handler ;
	chg_device->da9052->register_event_notifier(chg_device->da9052, &chg_device->vbus_det_data);
	if (ret) printk (KERN_ERR "%s: Error %d registering vbus_det\n", __func__, ret);
	ret = chg_device->da9052->event_enable(chg_device->da9052, VBUS_DET_EVE);
	if (ret) printk (KERN_ERR "%s: Error %d enabling vbus_det\n", __func__, ret);

	chg_device->vbus_rem_data.eve_type = VBUS_REM_EVE;
	chg_device->vbus_rem_data.call_back = vbus_rem_handler ;
	chg_device->da9052->register_event_notifier(chg_device->da9052, &chg_device->vbus_rem_data);
	if (ret) printk (KERN_ERR "%s: Error %d registering vbus_rem\n", __func__, ret);
	ret = chg_device->da9052->event_enable(chg_device->da9052, VBUS_REM_EVE);
	if (ret) printk (KERN_ERR "%s: Error %d enabling vbus_rem\n", __func__, ret);

	chg_device->wall_psy.name = "wall";
	chg_device->wall_psy.use_for_apm = 1;
	chg_device->wall_psy.type = POWER_SUPPLY_TYPE_MAINS;
	chg_device->wall_psy.get_property = da9052_dcin_get_property;
	chg_device->wall_psy.properties = da902_chg_props;
	chg_device->wall_psy.num_properties = ARRAY_SIZE(da902_chg_props);
	ret = power_supply_register(&pdev->dev, &chg_device->wall_psy);
	if (ret) printk (KERN_ERR "%s: error %d registering wall power supply\n", __func__, ret);

	chg_device->usb_psy.name = "usb";
	chg_device->usb_psy.use_for_apm = 1;
	chg_device->usb_psy.type = POWER_SUPPLY_TYPE_USB;
	chg_device->usb_psy.get_property = da9052_vbus_get_property;
	chg_device->usb_psy.properties = da902_chg_props;
	chg_device->usb_psy.num_properties = ARRAY_SIZE(da902_chg_props);
	ret = power_supply_register(&pdev->dev, &chg_device->usb_psy);
	if (ret) printk (KERN_ERR "%s: error %d registering usb power supply\n", __func__, ret);

	printk(KERN_INFO "%s\n", __func__);
	return 0;

err_charger_init:
	platform_set_drvdata(pdev, NULL);
	kfree(chg_device);
	return ret;
}

static int __devexit da9052_chg_remove(struct platform_device *dev)
{
	struct charger_device_t *chg_device = platform_get_drvdata(dev);
	s32 ret;

	printk(KERN_INFO "%s\n", __func__);
	power_supply_unregister(&chg_device->usb_psy);
        power_supply_unregister(&chg_device->wall_psy);

	/* unregister the events.*/
	ret = chg_device->da9052->event_disable(chg_device->da9052, chg_device->dcin_det_data.eve_type);
	if (ret) printk (KERN_ERR "%s: error %d disabling dcin_det\n", __func__, ret);
	ret = chg_device->da9052->unregister_event_notifier(chg_device->da9052, &chg_device->dcin_det_data);
	if (ret) printk (KERN_ERR "%s: error %d unregistering dcin_det\n", __func__, ret);

	ret = chg_device->da9052->event_disable(chg_device->da9052, chg_device->dcin_rem_data.eve_type);
	if (ret) printk (KERN_ERR "%s: error %d disabling dcin_rem\n", __func__, ret);
	ret = chg_device->da9052->unregister_event_notifier(chg_device->da9052, &chg_device->dcin_rem_data);
	if (ret) printk (KERN_ERR "%s: error %d unregistering dcin_rem\n", __func__, ret);

	ret = chg_device->da9052->event_disable(chg_device->da9052, chg_device->vbus_det_data.eve_type);
	if (ret) printk (KERN_ERR "%s: error %d disabling vbus_det\n", __func__, ret);
	ret = chg_device->da9052->unregister_event_notifier(chg_device->da9052, &chg_device->vbus_det_data);
	if (ret) printk (KERN_ERR "%s: error %d unregistering vbus_det\n", __func__, ret);

	ret = chg_device->da9052->event_disable(chg_device->da9052, chg_device->vbus_rem_data.eve_type);
	if (ret) printk (KERN_ERR "%s: error %d disabling vbus_rem\n", __func__, ret);
	ret = chg_device->da9052->unregister_event_notifier(chg_device->da9052, &chg_device->vbus_rem_data);
	if (ret) printk (KERN_ERR "%s: error %d unregistering vbus_rem\n", __func__, ret);
	return 0;
}

static struct platform_driver da9052_chg_driver = {
	.probe		= da9052_chg_probe,
	.remove		= __devexit_p(da9052_chg_remove),
	.driver		= {
		.name	= DA9052_CHG_DEVICE_NAME,
		.owner	= THIS_MODULE,
	},
};

static s32 __init da9052_chg_init(void)
{
	printk(banner);
	return platform_driver_register(&da9052_chg_driver);
}
module_init(da9052_chg_init);

static void __exit da9052_chg_exit(void)
{
	printk("DA9052: Unregistering CHG device.\n");
	platform_driver_unregister(&da9052_chg_driver);
}
module_exit(da9052_chg_exit);

MODULE_AUTHOR("Boundary Devices");
MODULE_DESCRIPTION("DA9052 Charger Device Driver");
MODULE_LICENSE("GPL");
