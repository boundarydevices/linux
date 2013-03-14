/*
 * Copyright (C) 2013 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/ipu.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/mxcfb.h>
#include <linux/fsl_devices.h>
#include "mxc_dispdrv.h"

#define DISPDRV_ADV739X 	"adv739x"

#define ADV739X_MODE_NTSC		0
#define ADV739X_MODE_PAL		1

struct adv739x_data {
	struct platform_device *pdev;
	struct i2c_client *client;
	struct mxc_dispdrv_handle *disp_adv739x;
	struct fb_info *fbi;

	int ipu_id;
	int disp_id;
	int ifmt;

	int cur_mode;
	int enabled;
	struct notifier_block nb;
	unsigned reg;
};

static int adv739x_write(struct i2c_client *client, u8 reg, u8 data)
{
	int ret = 0;
	ret = i2c_smbus_write_byte_data(client, reg, data);
	pr_info("%s: reg=0x%x data=0x%x ret=%d\n", __func__, reg, data, ret);
	return ret;
}

static int adv739x_read(struct i2c_client *client, u8 reg)
{
	int data = 0;
	data = i2c_smbus_read_byte_data(client, reg);
	pr_info("%s: reg=0x%x data=0x%x %d\n", __func__, reg, data, data);
	return data;
}

static ssize_t adv739x_reg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int val;
	struct adv739x_data *adv739x = dev_get_drvdata(dev);

	val = adv739x_read(adv739x->client, adv739x->reg);
	return sprintf(buf, "%02x: %02x\n", adv739x->reg, val);
}

static ssize_t adv739x_reg_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	unsigned val;
	struct adv739x_data *adv739x = dev_get_drvdata(dev);
	char *endp;
	unsigned reg = simple_strtol(buf, &endp, 16);
	if (reg >= 0xf2)
		return count;
	adv739x->reg = reg;
	if (!endp)
		return count;
	if (*endp == 0x20)
		endp++;
	if (*endp == 0)
		return count;
	val = simple_strtol(endp, &endp, 16);
	if (val >= 0x100)
		return count;
	adv739x_write(adv739x->client, reg, val);
	return count;
}

static DEVICE_ATTR(adv739x_reg, 0644, adv739x_reg_show, adv739x_reg_store);

/*
 * left_margin: used for field0 vStart width in lines
 *
 * right_margin: used for field0 vEnd width in lines
 *
 * up_margin: used for field1 vStart width in lines
 *
 * down_margin: used for field1 vEnd width in lines
 *
 * hsync_len: EAV Code + Blanking Video + SAV Code (in pixel clock count)
 *         For BT656 NTSC, it is 4 + 67*4 + 4 = 276.
 *         For BT1120 NTSC, it is 4 + 67*2 + 4 = 142.
 *         For BT656 PAL, it is 4 + 70*4 + 4 = 288.
 *         For BT1120 PAL, it is 4 + 70*2 + 4 = 148.
 *
 * vsync_len: not used, set to 1
 */
static struct fb_videomode adv739x_modedb[] = {
	{
	 /* NTSC Interlaced output */
	 "BT656-NTSC", 60, 720, 480, 37037,
	 .left_margin = 19, .right_margin = 3,		/* interpreted as field 0 vertical top/bottom */
	 .upper_margin = 20, .lower_margin = 3,		/* interpreted as field 1 vertical top/bottom */
	 .hsync_len = 276, .vsync_len = 1,		/* htotal clocks = hsync_len + xres*2 */
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .vmode = FB_VMODE_INTERLACED,
	 .flag = FB_MODE_IS_DETAILED,},
	{
	 /* PAL Interlaced output */
	 "BT656-PAL", 50, 720, 576, 37037,
	 .left_margin = 22, .right_margin = 2,		/* interpreted as field 0 vertical top/bottom */
	 .upper_margin = 23, .lower_margin = 2,		/* interpreted as field 1 vertical top/bottom */
	 .hsync_len = 288, .vsync_len = 1,		/* htotal clocks = hsync_len + xres*2 */
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .vmode = FB_VMODE_INTERLACED,
	 .flag = FB_MODE_IS_DETAILED,},
};
static int adv739x_modedb_sz = ARRAY_SIZE(adv739x_modedb);

static void adv739x_setmode(struct adv739x_data *adv739x, int mode)
{
	struct i2c_client *client = adv739x->client;

	if(adv739x->enabled == 0)
		return;

	dev_dbg(&adv739x->client->dev, "adv739x_setmode: mode = %d.\n", mode);
	switch (mode) {
		case ADV739X_MODE_NTSC:
			// Reg 0x17: reset
			adv739x_write(client, 0x17, 0x02);
			
			mdelay(20);
			
			// Reg 0x00: DAC1~3 power on
			adv739x_write(client, 0x00, 0x1C);

			// Reg 0x01: SD input
			adv739x_write(client, 0x01, 0x00);
			
			//NTSC
			// Reg 0x80: SD, NTSC
			adv739x_write(client, 0x80, 0x10);
			
			// Reg 0x82: SD, CVBS
			adv739x_write(client, 0x82, 0xCB);
			adv739x_write(client, 0x8a, (adv739x->ifmt != IPU_PIX_FMT_BT656) ? 0x0e : 0x08);
			break;

		case ADV739X_MODE_PAL:
			// Reg 0x17: reset
			adv739x_write(client, 0x17, 0x02);
			
			mdelay(20);
			
			// Reg 0x00: DAC1~3 power on
			adv739x_write(client, 0x00, 0x1C);

			// Reg 0x01: SD input
			adv739x_write(client, 0x01, 0x00);

			// Reg 0x80: SD, PAL
			adv739x_write(client, 0x80, 0x11);
			
			// Reg 0x82: SD, CVBS
			adv739x_write(client, 0x82, 0xC3);
			adv739x_write(client, 0x8C, 0xCB);
			adv739x_write(client, 0x8D, 0x8A);
			adv739x_write(client, 0x8E, 0x09);
			adv739x_write(client, 0x8F, 0x2A);
			adv739x_write(client, 0x8a, (adv739x->ifmt != IPU_PIX_FMT_BT656) ? 0x0e : 0x08);
			break;

		default:
			dev_err(&adv739x->client->dev, "unsupported mode.\n");
			break;
	}
}

static void adv739x_poweroff(struct adv739x_data *adv739x)
{
	struct fsl_mxc_lcd_platform_data *plat = adv739x->client->dev.platform_data;

	if (adv739x->enabled != 0) {
		dev_dbg(&adv739x->client->dev, "adv739x_poweroff.\n");
		
		/* power off the adv739x */
		adv739x_write(adv739x->client, 0x00, 0x1F);

		/* Disable pins */
		if (plat->disable_pins)
			plat->disable_pins();

		adv739x->enabled = 0;
	}
}

static void adv739x_poweron(struct adv739x_data *adv739x)
{
	struct fsl_mxc_lcd_platform_data *plat = adv739x->client->dev.platform_data;

	if (adv739x->enabled == 0) {
		dev_dbg(&adv739x->client->dev, "adv739x_poweron.\n");

		/* Enable pins */
		if (plat->enable_pins)
			plat->enable_pins();

		if (plat->reset)
			plat->reset();
		
		adv739x->enabled = 1;
		adv739x_setmode(adv739x, adv739x->cur_mode);
	}
}

int adv739x_fb_event(struct notifier_block *nb, unsigned long val, void *v)
{
	struct fb_event *event = v;
	struct fb_info *fbi = event->info;
	struct adv739x_data *adv739x = container_of(nb, struct adv739x_data, nb);
	int i;

	if (strcmp(event->info->fix.id, adv739x->fbi->fix.id))
		return 0;

	switch (val) {
	case FB_EVENT_MODE_CHANGE:
		for (i = 0; i < ARRAY_SIZE(adv739x_modedb); i++) {
			if (strcmp(fbi->mode->name, adv739x_modedb[i].name) == 0) {
				adv739x->cur_mode = i;
				adv739x_setmode(adv739x, adv739x->cur_mode);
				break;
			}
		}
		break;
	case FB_EVENT_BLANK:
		if (*((int *)event->data) == FB_BLANK_UNBLANK)
			adv739x_poweron(adv739x);
		else
			adv739x_poweroff(adv739x);
		break;
	}
	return 0;
}

static int adv739x_disp_init(struct mxc_dispdrv_handle *disp,
	struct mxc_dispdrv_setting *setting)
{
	int ret = 0, i;
	struct adv739x_data *adv739x = mxc_dispdrv_getdata(disp);
	struct fsl_mxc_lcd_platform_data *plat = adv739x->client->dev.platform_data;
	struct fb_videomode *modedb = adv739x_modedb;
	int modedb_sz = adv739x_modedb_sz;
	static bool inited = false;

	if (inited)
		return -EBUSY;

	inited = true;

	setting->dev_id = plat->ipu_id;
	setting->disp_id = plat->disp_id;

	ret = fb_find_mode(&setting->fbi->var, setting->fbi, setting->dft_mode_str,
				modedb, modedb_sz, NULL, setting->default_bpp);
	if (!ret) {
		fb_videomode_to_var(&setting->fbi->var, &modedb[0]);
		setting->if_fmt = plat->default_ifmt;
		adv739x->ifmt = plat->default_ifmt;
	}

	INIT_LIST_HEAD(&setting->fbi->modelist);
	for (i = 0; i < modedb_sz; i++) {
		fb_add_videomode(&modedb[i],
				&setting->fbi->modelist);
	}

	adv739x->fbi = setting->fbi;
	adv739x->enabled = 0;
	adv739x->cur_mode = ADV739X_MODE_NTSC;  //default mode

	adv739x->pdev = platform_device_register_simple("mxc_adv739x", 0, NULL, 0);
	if (IS_ERR(adv739x->pdev)) {
		dev_err(&adv739x->client->dev,
				"Unable to register adv739x as a platform device\n");
		ret = PTR_ERR(adv739x->pdev);
		goto register_pltdev_failed;
	}

	/* Claim pins */
	if (plat->get_pins)
		if (!plat->get_pins()) {
			ret = -EACCES;
			goto get_pins_failed;
		}

	adv739x->nb.notifier_call = adv739x_fb_event;
	ret = fb_register_client(&adv739x->nb);
	if (ret < 0)
		goto reg_fbclient_failed;

	return ret;

reg_fbclient_failed:
get_pins_failed:
	platform_device_unregister(adv739x->pdev);
register_pltdev_failed:
	return ret;
}

static void adv739x_disp_deinit(struct mxc_dispdrv_handle *disp)
{
	struct adv739x_data *adv739x = mxc_dispdrv_getdata(disp);
	struct fsl_mxc_lcd_platform_data *plat = adv739x->client->dev.platform_data;

	if (adv739x->client->irq)
		free_irq(adv739x->client->irq, adv739x);

	fb_unregister_client(&adv739x->nb);

	adv739x_poweroff(adv739x);

	/* Release pins */
	if (plat->put_pins)
		plat->put_pins();

	platform_device_unregister(adv739x->pdev);
}

static struct mxc_dispdrv_driver adv739x_drv = {
	.name 	= DISPDRV_ADV739X,
	.init 	= adv739x_disp_init,
	.deinit	= adv739x_disp_deinit,
};

static int __devinit adv739x_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct fsl_mxc_lcd_platform_data *plat = client->dev.platform_data;
	struct adv739x_data *adv739x;
	int ret;

	if (!i2c_check_functionality(client->adapter,
				I2C_FUNC_SMBUS_BYTE | I2C_FUNC_SMBUS_BYTE_DATA)) {
		pr_err("%s: need smbus\n", __func__);
		return -ENODEV;
	}

	adv739x = kzalloc(sizeof(struct adv739x_data), GFP_KERNEL);
	if (!adv739x) {
		pr_err("%s: no mem\n", __func__);
		return -ENOMEM;
	}
	
	adv739x->client = client;
	adv739x->ifmt = plat->default_ifmt;

	adv739x->disp_adv739x = mxc_dispdrv_register(&adv739x_drv);
	mxc_dispdrv_setdata(adv739x->disp_adv739x, adv739x);

	i2c_set_clientdata(client, adv739x);
	ret = device_create_file(&client->dev, &dev_attr_adv739x_reg);
	if (ret < 0)
		pr_warn("failed to add adv739x sysfs files\n");
	return 0;
}

static int __devexit adv739x_remove(struct i2c_client *client)
{
	struct adv739x_data *adv739x = i2c_get_clientdata(client);

	mxc_dispdrv_puthandle(adv739x->disp_adv739x);
	mxc_dispdrv_unregister(adv739x->disp_adv739x);
	device_remove_file(&client->dev, &dev_attr_adv739x_reg);
	kfree(adv739x);
	return 0;
}

static const struct i2c_device_id adv739x_id[] = {
	{ "mxc_adv739x", 0 },
};

MODULE_DEVICE_TABLE(i2c, adv739x_id);

static struct i2c_driver adv739x_i2c_driver = {
	.driver = {
		   .name = "mxc_adv739x",
		   },
	.probe = adv739x_probe,
	.remove = adv739x_remove,
	.id_table = adv739x_id,
};

static int __init adv739x_i2c_init(void)
{
	return i2c_add_driver(&adv739x_i2c_driver);
}

static void __exit adv739x_i2c_exit(void)
{
	i2c_del_driver(&adv739x_i2c_driver);
}

module_init(adv739x_i2c_init);
module_exit(adv739x_i2c_exit);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("ADV739x TV encoder driver");
MODULE_LICENSE("GPL");

