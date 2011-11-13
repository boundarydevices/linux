/*
 * Copyright 2008-2010 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

/*
 * Enables or disables a regulator on screen unblank/blank
 *
 * regulator name is specified in platform data through the
 * field
 *	mxc_lcd_platform_data:io_reg
 */

/*!
 * Include files
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/console.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fb.h>
#include <linux/fsl_devices.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mxcfb.h>
#include <linux/regulator/consumer.h>
#include <mach/hardware.h>
#include <linux/i2c.h>

static struct platform_device *plcd_dev;
static struct regulator *io_reg = 0 ;
static int lcd_on;

static void lcd_poweron(void)
{
	struct i2c_adapter *adap;
	if (lcd_on) {
		dev_dbg(&plcd_dev->dev, "%s: display is already on\n", __func__ );
		return;
	}

	dev_dbg(&plcd_dev->dev, "turning on LCD\n");

	if (io_reg) {
		regulator_enable(io_reg);
		dev_dbg(&plcd_dev->dev, "%s: regulator enabled ? %d\n", __func__,regulator_is_enabled(io_reg));
	}

	adap = i2c_get_adapter(0);
	if (adap) {
		union i2c_smbus_data data;
                int ret ;
		data.byte = 0x7b;
		ret = i2c_smbus_xfer(adap, 0x48,0,
				     I2C_SMBUS_WRITE,28,
				     I2C_SMBUS_BYTE_DATA,&data);
		if (0 == ret) {
			dev_dbg(&plcd_dev->dev, "%s: power on display\n", __func__ );
			lcd_on = 1;
		} else
			printk (KERN_ERR "%s: error enabling display\n", __func__ );
	} else
		printk (KERN_ERR "%s: error getting I2C adapter\n", __func__ );
}

static void lcd_poweroff(void)
{
	struct i2c_adapter *adap;
	if (!lcd_on) {
		dev_dbg(&plcd_dev->dev, "%s: display is already off\n", __func__ );
		return;
	}

	dev_dbg(&plcd_dev->dev, "turning off LCD\n");

	if (io_reg) {
		regulator_disable(io_reg);
		dev_dbg(&plcd_dev->dev, "%s: regulator enabled ? %d\n", __func__,regulator_is_enabled(io_reg));
	}

	adap = i2c_get_adapter(0);
	if (adap) {
		union i2c_smbus_data data;
                int ret ;
		data.byte = 0x77;
		ret = i2c_smbus_xfer(adap, 0x48,0,
				     I2C_SMBUS_WRITE,28,
				     I2C_SMBUS_BYTE_DATA,&data);
		if (0 == ret) {
			dev_dbg(&plcd_dev->dev, "%s: power off display\n", __func__ );
			lcd_on = 0;
		} else
			printk (KERN_ERR "%s: error disabling display\n", __func__ );
	} else
		printk (KERN_ERR "%s: error getting I2C adapter\n", __func__ );
}

static int lcd_fb_event(struct notifier_block *nb, unsigned long val, void *v)
{
	struct fb_event *event = v;

	dev_dbg(&plcd_dev->dev, "%s: event %lx, display %s\n", __func__, val, event->info->fix.id);
	if (strncmp(event->info->fix.id, "DISP3 BG",8)) {
		return 0;
	}

	switch (val) {
	case FB_EVENT_BLANK: {
		int blankType = *((int *)event->data);
		dev_dbg(&plcd_dev->dev, "%s: blank type 0x%x\n", __func__, blankType );
		if (blankType == FB_BLANK_UNBLANK) {
			lcd_poweron();
		} else {
			lcd_poweroff();
		}
		break;
	}
	case FB_EVENT_SUSPEND : {
		dev_dbg(&plcd_dev->dev, "%s: suspend\n", __func__ );
		lcd_poweroff();
		break;
	}
	case FB_EVENT_RESUME : {
		dev_dbg(&plcd_dev->dev, "%s: resume\n", __func__ );
		lcd_poweron();
		break;
	}

	default:
		dev_dbg(&plcd_dev->dev, "%s: unknown event %lx\n", __func__, val);
	}
	return 0;
}

static struct notifier_block nb = {
	.notifier_call = lcd_fb_event,
};

static int __devinit lcd_probe(struct platform_device *pdev)
{
	int i;
	struct mxc_lcd_platform_data *plat = pdev->dev.platform_data;

	if (plat) {
		if (plat->reset)
			plat->reset();

		io_reg = regulator_get(&pdev->dev, plat->io_reg);
		if (IS_ERR(io_reg))
			io_reg = NULL;
	}

	for (i = 0; i < num_registered_fb; i++) {
		if (strncmp(registered_fb[i]->fix.id, "DISP3 BG",8) == 0) {
			lcd_poweron();
		} else if (strcmp(registered_fb[i]->fix.id, "DISP3 FG") == 0) {
		}
	}

	fb_register_client(&nb);

	plcd_dev = pdev;

	return 0;
}

static int __devexit lcd_remove(struct platform_device *pdev)
{
	fb_unregister_client(&nb);
	lcd_poweroff();

	if (io_reg) {
		regulator_put(io_reg);
		io_reg = 0 ;
	}

	return 0;
}

#ifdef CONFIG_PM
static int lcd_suspend(struct platform_device *pdev, pm_message_t state)
{
        lcd_poweroff();
	return 0;
}

static int lcd_resume(struct platform_device *pdev)
{
	lcd_poweron();
	return 0;
}
#else
#define lcd_suspend NULL
#define lcd_resume NULL
#endif

static void lcd_shutdown(struct platform_device *pdev)
{
	printk (KERN_ERR "%s\n", __func__ );
	lcd_poweroff();
}

/*!
 * platform driver structure for CLAA WVGA
 */
static struct platform_driver lcd_driver = {
	.driver = {
		   .name = "lcd_pmic"},
	.probe = lcd_probe,
	.remove = __devexit_p(lcd_remove),
	.suspend = lcd_suspend,
	.resume = lcd_resume,
	.shutdown = lcd_shutdown,
};

static int __init claa_lcd_init(void)
{
	return platform_driver_register(&lcd_driver);
}

static void __exit claa_lcd_exit(void)
{
	platform_driver_unregister(&lcd_driver);
}

module_init(claa_lcd_init);
module_exit(claa_lcd_exit);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("CLAA WVGA LCD init driver");
MODULE_LICENSE("GPL");
