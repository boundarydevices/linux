/*
 * drivers/amlogic/media/vin/tvin/hdmirx_ext/hdmirx_ext_drv.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/slab.h>
/*#include <linux/i2c.h>*/
#include <linux/amlogic/i2c-amlogic.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/of_gpio.h>

#include "hdmirx_ext_drv.h"
#include "hdmirx_ext_hw_iface.h"
#include "hdmirx_ext_attrs.h"
#include "vdin_iface.h"

int hdmirx_ext_debug_print;

static struct hdmirx_ext_drv_s *hdmirx_ext_drv;
static struct class *hdmirx_ext_class;
static dev_t hdmirx_ext_device_id;
static struct device *hdmirx_ext_device;

struct hdmirx_ext_drv_s *hdmirx_ext_get_driver(void)
{
	if (hdmirx_ext_drv == NULL)
		RXEXTERR("%s: hdmirx_ext_drv is null\n", __func__);

	return hdmirx_ext_drv;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

static int hdmirx_ext_start(struct hdmirx_ext_drv_s *hdrv)
{
	int ret = 0;

	RXEXTDBG("%s: start!\n", __func__);
	ret = __hw_init();
	if (ret < 0) {
		RXEXTERR("%s: failed to init hw\n", __func__);
		return -2;
	}
	ret = __hw_enable();
	if (ret < 0) {
		RXEXTERR("%s: failed to enable hw\n", __func__);
		return -2;
	}

#if 0
	ret = __hw_setup_timer();
	if (ret < 0) {
		RXEXTERR("%s: failed to setup timer, err = %d!\n",
			__func__, ret);
		return -3;
	}

	ret = __hw_setup_irq();
	if (ret < 0) {
		RXEXTERR("%s: failed to setup irq, err = %d!\n",
			__func__, ret);
		return -4;
	}

	ret = __hw_setup_task();
	if (ret < 0) {
		RXEXTERR("%s: failed to setup task, err = %d!\n",
			__func__, ret);
		return -5;
	}
#endif

	return ret;
}

static void hdmirx_ext_device_probe(struct platform_device *pdev)
{
	RXEXTDBG("%s: start\n", __func__);

	if (!strncmp(hdmirx_ext_drv->name, "sii9135", 7)) {
#if defined(CONFIG_AMLOGIC_MEDIA_TVIN_HDMI_EXT_SII9135)
		hdmirx_ext_register_hw_sii9135(hdmirx_ext_drv);
#endif
	} else {
		RXEXTERR("%s: invalid device\n", __func__);
	}
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

static void hdmirx_ext_set_default_params(void)
{
	hdmirx_ext_drv->hw.i2c_addr = 0xff;
	hdmirx_ext_drv->hw.i2c_bus_index = -1;
	hdmirx_ext_drv->hw.en_gpio.gpio = NULL;
	hdmirx_ext_drv->hw.en_gpio.flag = 0;
	hdmirx_ext_drv->hw.en_gpio.val_on = -1;
	hdmirx_ext_drv->hw.en_gpio.val_off = -1;
	hdmirx_ext_drv->hw.reset_gpio.gpio = NULL;
	hdmirx_ext_drv->hw.reset_gpio.flag = 0;
	hdmirx_ext_drv->hw.reset_gpio.val_on = -1;
	hdmirx_ext_drv->hw.reset_gpio.val_off = -1;
	hdmirx_ext_drv->hw.pin = NULL;
	hdmirx_ext_drv->hw.pinmux_flag = 0;
	/* hdmirx_ext_drv->hw.irq_gpio = 0; */
	/* hdmirx_ext_drv->hw.irq_num  = -1; */
	hdmirx_ext_drv->hw.get_cable_status  = NULL;
	hdmirx_ext_drv->hw.get_signal_status = NULL;
	hdmirx_ext_drv->hw.get_input_port    = NULL;
	hdmirx_ext_drv->hw.set_input_port    = NULL;
	hdmirx_ext_drv->hw.get_video_timming = NULL;
	hdmirx_ext_drv->hw.is_hdmi_mode      = NULL;
	hdmirx_ext_drv->hw.get_audio_sample_rate = NULL;
	hdmirx_ext_drv->hw.debug            = NULL;
	hdmirx_ext_drv->hw.get_chip_version = NULL;
	hdmirx_ext_drv->hw.init             = NULL;
	hdmirx_ext_drv->hw.enable           = NULL;
	hdmirx_ext_drv->hw.disable          = NULL;

	hdmirx_ext_drv->vdin.vdin_sel = 0;
	hdmirx_ext_drv->vdin.bt656_sel = 0;
	hdmirx_ext_drv->vdin.started = 0;
	hdmirx_ext_drv->user_cmd     = 0;

	hdmirx_ext_drv->status.cable      = 0;
	hdmirx_ext_drv->status.signal     = 0;
	hdmirx_ext_drv->status.video_mode = 0xff;
	hdmirx_ext_drv->status.audio_sr   = 0;
}

#ifdef CONFIG_OF
static int hdmirx_ext_get_dts_config(struct device *dev)
{
	struct device_node *of_node = dev->of_node;
	unsigned int i2c_index;
	const char *str;
	unsigned int val, para[3];
	int ret = 0;

	/* for vdin selection */
	ret = of_property_read_u32(of_node, "vdin_sel", &val);
	if (ret) {
		RXEXTERR("%s: failed to get vdin_sel\n", __func__);
	} else {
		hdmirx_ext_drv->vdin.vdin_sel = (unsigned char)val;
		RXEXTPR("vdin_sel = %d\n", hdmirx_ext_drv->vdin.vdin_sel);
	}
	ret = of_property_read_u32(of_node, "bt656_sel", &val);
	if (ret) {
		RXEXTERR("%s: failed to get bt656_sel\n", __func__);
	} else {
		hdmirx_ext_drv->vdin.bt656_sel = (unsigned char)val;
		RXEXTPR("bt656_sel = %d\n", hdmirx_ext_drv->vdin.bt656_sel);
	}

	/* for device name */
	ret = of_property_read_string(of_node, "ext_dev_name", &str);
	if (ret) {
		RXEXTERR("failed to get ext_dev_name\n");
		str = "invalid";
	}
	strcpy(hdmirx_ext_drv->name, str);

	/* for i2c address */
	ret = of_property_read_u32(of_node, "i2c_addr", &val);
	if (ret) {
		RXEXTERR("%s: failed to get i2c_addr\n", __func__);
	} else {
		hdmirx_ext_drv->hw.i2c_addr = (unsigned char)val;
		RXEXTPR("i2c_addr = 0x%02x\n", hdmirx_ext_drv->hw.i2c_addr);
	}

	/* for i2c bus */
	ret = of_property_read_string(of_node, "i2c_bus", &str);
	if (ret) {
		RXEXTERR("%s: failed to get i2c_bus\n", __func__);
		return -1;
	}
	if (!strncmp(str, "i2c_bus_ao", 10))
		i2c_index = AML_I2C_MASTER_AO;
	else if (!strncmp(str, "i2c_bus_a", 9))
		i2c_index = AML_I2C_MASTER_A;
	else if (!strncmp(str, "i2c_bus_b", 9))
		i2c_index = AML_I2C_MASTER_B;
	else if (!strncmp(str, "i2c_bus_c", 9))
		i2c_index = AML_I2C_MASTER_C;
	else if (!strncmp(str, "i2c_bus_d", 9))
		i2c_index = AML_I2C_MASTER_D;
	else {
		RXEXTERR("%s: invalid i2c bus \"%s\" from dts\n",
			__func__, str);
		return -1;
	}
	RXEXTPR("i2c_bus = %s\n", str);
	hdmirx_ext_drv->hw.i2c_bus_index = i2c_index;

	/* for gpio_en */
	val = GPIOD_OUT_LOW;
	ret = of_property_read_u32_array(of_node, "en_gpio_val", &para[0], 2);
	if (ret) {
		RXEXTERR("%s: failed to get en_gpio_val\n", __func__);
	} else {
		hdmirx_ext_drv->hw.en_gpio.val_on = para[0];
		hdmirx_ext_drv->hw.en_gpio.val_off = para[1];
		val = hdmirx_ext_drv->hw.en_gpio.val_off ?
			GPIOD_OUT_LOW : GPIOD_OUT_HIGH;
		RXEXTPR("en_gpio val_on = %d, val_off=%d\n",
			hdmirx_ext_drv->hw.en_gpio.val_on,
			hdmirx_ext_drv->hw.en_gpio.val_off);
	}
	/* request gpio */
	hdmirx_ext_drv->hw.en_gpio.gpio = devm_gpiod_get(dev, "en", val);
	if (IS_ERR(hdmirx_ext_drv->hw.en_gpio.gpio)) {
		hdmirx_ext_drv->hw.en_gpio.flag = 0;
		RXEXTERR("register en_gpio err\n");
	} else {
		hdmirx_ext_drv->hw.en_gpio.flag = 1;
	}
	if (hdmirx_ext_drv->hw.en_gpio.flag) {
		ret = of_property_read_string(of_node, "en_gpio_name", &str);
		if (ret) {
			RXEXTPR("failed to get en_gpio_name\n");
			str = "unknown";
		}
		strcpy(hdmirx_ext_drv->hw.en_gpio.name, str);
		RXEXTPR("register gpio %s: 0x%p\n",
			hdmirx_ext_drv->hw.en_gpio.name,
			hdmirx_ext_drv->hw.en_gpio.gpio);
	}

	/* for gpio_reset */
	val = GPIOD_OUT_LOW;
	ret = of_property_read_u32_array(of_node, "reset_gpio_val",
			&para[0], 2);
	if (ret) {
		RXEXTERR("%s: failed to get reset_gpio_val\n", __func__);
	} else {
		hdmirx_ext_drv->hw.reset_gpio.val_on = para[0];
		hdmirx_ext_drv->hw.reset_gpio.val_off = para[1];
		val = hdmirx_ext_drv->hw.reset_gpio.val_on ?
			GPIOD_OUT_LOW : GPIOD_OUT_HIGH;
		RXEXTPR("reset_gpio val_on = %d, val_off=%d\n",
			hdmirx_ext_drv->hw.reset_gpio.val_on,
			hdmirx_ext_drv->hw.reset_gpio.val_off);
	}
	/* request gpio */
	hdmirx_ext_drv->hw.reset_gpio.gpio = devm_gpiod_get(dev, "reset", val);
	if (IS_ERR(hdmirx_ext_drv->hw.reset_gpio.gpio)) {
		hdmirx_ext_drv->hw.reset_gpio.flag = 0;
		RXEXTERR("register reset_gpio err\n");
	} else {
		hdmirx_ext_drv->hw.reset_gpio.flag = 1;
	}
	if (hdmirx_ext_drv->hw.reset_gpio.flag) {
		ret = of_property_read_string(of_node, "reset_gpio_name", &str);
		if (ret) {
			RXEXTPR("failed to get reset_gpio_name\n");
			str = "unknown";
		}
		strcpy(hdmirx_ext_drv->hw.reset_gpio.name, str);
		RXEXTPR("register gpio %s: 0x%p\n",
			hdmirx_ext_drv->hw.reset_gpio.name,
			hdmirx_ext_drv->hw.reset_gpio.gpio);
	}

	return 0;
}
#endif

static int hdmirx_ext_open(struct inode *node, struct file *filp)
{
	struct hdmirx_ext_drv_s *pdev;

	pdev = container_of(node->i_cdev, struct hdmirx_ext_drv_s, cdev);
	filp->private_data = pdev;

	return 0;
}

static int hdmirx_ext_release(struct inode *node, struct file *filp)
{
	return 0;
}

static const struct file_operations hdmirx_ext_fops = {
	.owner		= THIS_MODULE,
	.open		= hdmirx_ext_open,
	.release	= hdmirx_ext_release,
};

static int hdmirx_ext_probe(struct platform_device *pdev)
{
	int ret = -1;

	hdmirx_ext_debug_print = 0;
	RXEXTPR("%s: ver: %s\n", __func__, HDMIRX_EXT_DRV_VER);

	hdmirx_ext_drv = kmalloc(sizeof(struct hdmirx_ext_drv_s), GFP_KERNEL);
	if (!hdmirx_ext_drv) {
		ret = -ENOMEM;
		RXEXTERR("%s: failed to alloc hdmirx_ext_drv!\n", __func__);
		goto fail;
	}
	hdmirx_ext_drv->dev = &pdev->dev;
	hdmirx_ext_drv->vops = get_vdin_v4l2_ops();

	/* hdmirx_ext_drv default value setting */
	hdmirx_ext_set_default_params();

#ifdef CONFIG_OF
	ret = hdmirx_ext_get_dts_config(&pdev->dev);
	if (ret) {
		RXEXTERR("%s: invalid dts config\n", __func__);
		goto cleanup;
	}
#endif
	hdmirx_ext_device_probe(pdev);

	ret = alloc_chrdev_region(&hdmirx_ext_device_id, 0, 1,
			HDMIRX_EXT_DEV_NAME);
	if (ret < 0) {
		RXEXTERR("%s: failed to alloc char dev\n", __func__);
		goto cleanup;
	}

	cdev_init(&(hdmirx_ext_drv->cdev), &hdmirx_ext_fops);
	hdmirx_ext_drv->cdev.owner = THIS_MODULE;

	ret = cdev_add(&(hdmirx_ext_drv->cdev), hdmirx_ext_device_id, 1);
	if (ret) {
		RXEXTERR("%s: failed to add hdmirx_ext cdev, err = %d\n",
			__func__, ret);
		goto unregister;
	}

	hdmirx_ext_class = class_create(THIS_MODULE, HDMIRX_EXT_DRV_NAME);
	if (IS_ERR(hdmirx_ext_class)) {
		ret = PTR_ERR(hdmirx_ext_class);
		RXEXTERR("%s: failed to create hdmirx_ext class, err = %d\n",
			__func__, ret);
		goto destroy_cdev;
	}

	hdmirx_ext_device = device_create(hdmirx_ext_class, NULL,
			hdmirx_ext_device_id, NULL, "%s", hdmirx_ext_drv->name);
	if (IS_ERR(hdmirx_ext_device)) {
		ret = PTR_ERR(hdmirx_ext_device);
		RXEXTERR("%s: failed to create hdmirx_ext device, err = %d\n",
			__func__, ret);
		goto destroy_class;
	}

	ret = hdmirx_ext_create_device_attrs(hdmirx_ext_device);
	if (ret < 0) {
		RXEXTERR("%s: failed to create device attrs, err = %d\n",
			__func__, ret);
		goto destroy_device;
	}

	/* set drvdata */
	dev_set_drvdata(hdmirx_ext_device, hdmirx_ext_drv);

	ret = hdmirx_ext_start(hdmirx_ext_drv);
	if (ret) {
		RXEXTERR("%s: failed to setup resource, err = %d\n",
			__func__, ret);
		goto destroy_attrs;
	}

	RXEXTPR("driver probe ok\n");

	return 0;

destroy_attrs:
	hdmirx_ext_destroy_device_attrs(hdmirx_ext_device);

destroy_device:
	device_destroy(hdmirx_ext_class, hdmirx_ext_device_id);

destroy_class:
	class_destroy(hdmirx_ext_class);

destroy_cdev:
	cdev_del(&(hdmirx_ext_drv->cdev));

unregister:
	unregister_chrdev_region(MKDEV(MAJOR(hdmirx_ext_device_id),
		MINOR(hdmirx_ext_device_id)), 1);

cleanup:
	kfree(hdmirx_ext_drv);
	hdmirx_ext_drv = NULL;

fail:
	return ret;
}

static int hdmirx_ext_remove(struct platform_device *pdev)
{
	RXEXTPR("%s\n", __func__);

	hdmirx_ext_destroy_device_attrs(hdmirx_ext_device);

	if (hdmirx_ext_class) {
		device_destroy(hdmirx_ext_class, hdmirx_ext_device_id);
		class_destroy(hdmirx_ext_class);
	}

	if (hdmirx_ext_drv) {
		cdev_del(&(hdmirx_ext_drv->cdev));
		kfree(hdmirx_ext_drv);
		hdmirx_ext_drv = NULL;
	}

	unregister_chrdev_region(hdmirx_ext_device_id, 1);

	return 0;
}

static int hdmirx_ext_resume(struct platform_device *pdev)
{
	RXEXTPR("%s\n", __func__);
	return 0;
}

static int hdmirx_ext_suspend(struct platform_device *pdev,
		pm_message_t state)
{
	RXEXTPR("%s\n", __func__);
	if (hdmirx_ext_drv) {
		if (hdmirx_ext_drv->state) {
			hdmirx_ext_stop_vdin();
			__hw_disable();
		}
	}
	return 0;
}

static void hdmirx_ext_shutdown(struct platform_device *pdev)
{
	RXEXTPR("%s\n", __func__);
	if (hdmirx_ext_drv) {
		if (hdmirx_ext_drv->state) {
			hdmirx_ext_stop_vdin();
			__hw_disable();
		}
	}
}

#ifdef CONFIG_OF
static const struct of_device_id hdmirx_ext_dt_match[] = {
	{
		.compatible = "amlogic, hdmirx_ext",
	},
	{},
};
#else
#define hdmirx_ext_dt_match  NULL
#endif


static struct platform_driver hdmirx_ext_driver = {
	.probe  = hdmirx_ext_probe,
	.remove = hdmirx_ext_remove,
	.suspend = hdmirx_ext_suspend,
	.resume = hdmirx_ext_resume,
	.shutdown = hdmirx_ext_shutdown,
	.driver = {
		.name		= HDMIRX_EXT_DRV_NAME,
		.owner		= THIS_MODULE,
		.of_match_table = hdmirx_ext_dt_match,
	}
};
static int __init hdmirx_ext_drv_init(void)
{
	if (platform_driver_register(&hdmirx_ext_driver)) {
		RXEXTERR("%s: failed to register driver\n", __func__);
		return -ENODEV;
	}

	return 0;
}

static void __exit hdmirx_ext_drv_exit(void)
{
	RXEXT_PFUNC();
	platform_driver_unregister(&hdmirx_ext_driver);
	class_unregister(hdmirx_ext_class);
}

late_initcall(hdmirx_ext_drv_init);
module_exit(hdmirx_ext_drv_exit);

MODULE_DESCRIPTION("AML external hdmirx driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Amlogic, Inc.");
