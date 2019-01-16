/*
 * drivers/amlogic/wifi/wifi_dt.c
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

#include <linux/amlogic/wifi_dt.h>
#include <linux/amlogic/dhd_buf.h>

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/of_irq.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/iomap.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/pwm.h>
#include <linux/pci.h>
#include <linux/amlogic/pwm_meson.h>
#include "../../gpio/gpiolib.h"
#define OWNER_NAME "sdio_wifi"

int wifi_power_gpio;
int wifi_power_gpio2;

/*
 *there are two pwm channel outputs using one gpio
 *for gxtvbb and the follows soc
 */
struct pwm_double_data {
	struct pwm_device *pwm;
	unsigned int duty_cycle;
	unsigned int pwm_times;
};

struct pwm_double_datas {
	int num_pwm;
	struct pwm_double_data pwms[2];
};

struct pwm_single_data {
	struct pwm_device *pwm;
	unsigned int duty_cycle;
};

struct wifi_plat_info {
	int interrupt_pin;
	int irq_num;
	int irq_trigger_type;

	int power_on_pin;
	int power_on_pin_level;
	int power_on_pin_OD;
	int power_on_pin2;

	int clock_32k_pin;
	struct gpio_desc *interrupt_desc;
	struct gpio_desc *powe_desc;

	int plat_info_valid;
	struct pinctrl *p;
	struct device		*dev;
	struct pwm_double_datas ddata;
	struct pwm_single_data sdata;
};

#define WIFI_POWER_MODULE_NAME	"wifi_power"
#define WIFI_POWER_DRIVER_NAME	"wifi_power"
#define WIFI_POWER_DEVICE_NAME	"wifi_power"
#define WIFI_POWER_CLASS_NAME		"wifi_power"

#define USB_POWER_UP    _IO('m', 1)
#define USB_POWER_DOWN  _IO('m', 2)
#define WIFI_POWER_UP    _IO('m', 3)
#define WIFI_POWER_DOWN  _IO('m', 4)
#define SDIO_GET_DEV_TYPE  _IO('m', 5)
static struct wifi_plat_info wifi_info;
static dev_t wifi_power_devno;
static struct cdev *wifi_power_cdev;
static struct device *devp;
struct wifi_power_platform_data *pdata;

static int usb_power;
#define BT_BIT	0
#define WIFI_BIT	1
static DEFINE_MUTEX(wifi_bt_mutex);

#define WIFI_INFO(fmt, args...)	\
	dev_info(wifi_info.dev, "[%s] " fmt, __func__, ##args)

#ifdef CONFIG_OF
static const struct of_device_id wifi_match[] = {
	{
		.compatible = "amlogic, aml_wifi",
		.data		= (void *)&wifi_info
	},
	{},
};

static struct wifi_plat_info *wifi_get_driver_data
	(struct platform_device *pdev)
{
	const struct of_device_id *match;

	match = of_match_node(wifi_match, pdev->dev.of_node);
	if (!match)
		return NULL;
	return (struct wifi_plat_info *)match->data;
}
#else
#define wifi_match NULL
#endif



#define SHOW_PIN_OWN(pin_str, pin_num)	\
	WIFI_INFO("%s(%d)\n", pin_str, pin_num)

static int set_power(int value)
{
	if (!wifi_info.power_on_pin_OD) {
		if (wifi_info.power_on_pin_level)
			return gpio_direction_output(wifi_info.power_on_pin,
					!value);
		else
			return gpio_direction_output(wifi_info.power_on_pin,
					value);
	} else {
		if (wifi_info.power_on_pin_level) {
			if (value)
				gpio_direction_input(wifi_info.power_on_pin);
			else
				gpio_direction_output(wifi_info.power_on_pin,
					0);
		} else {
			if (value)
				gpio_direction_output(wifi_info.power_on_pin,
					0);
			else
				gpio_direction_input(wifi_info.power_on_pin);
		}
	}
	return 0;
}

static int set_power2(int value)
{
	if (wifi_info.power_on_pin_level)
		return gpio_direction_output(wifi_info.power_on_pin2,
				!value);
	else
		return gpio_direction_output(wifi_info.power_on_pin2,
				value);
}

static int set_wifi_power(int is_power)
{
	int ret = 0;

	if (is_power) {
		if (wifi_info.power_on_pin) {
			ret = set_power(1);
			if (ret)
				WIFI_INFO("power up failed(%d)\n", ret);
		}
		if (wifi_info.power_on_pin2) {
			ret = set_power2(1);
			if (ret)
				WIFI_INFO("power2 up failed(%d)\n", ret);
		}
	} else {
		if (wifi_info.power_on_pin) {
			ret = set_power(0);
			if (ret)
				WIFI_INFO("power down failed(%d)\n", ret);
		}
		if (wifi_info.power_on_pin2) {
			ret = set_power2(0);
			if (ret)
				WIFI_INFO("power2 down failed(%d)\n", ret);
		}
	}
	return ret;
}

static void usb_power_control(int is_power, int shift)
{
	mutex_lock(&wifi_bt_mutex);
	if (is_power) {
		if (!usb_power) {
			set_wifi_power(is_power);
			WIFI_INFO("Set %s power on !\n", (shift ? "WiFi":"BT"));
			sdio_reinit();
		}
		usb_power |= (1 << shift);
		WIFI_INFO("Set %s power on !\n", (shift ? "WiFi":"BT"));
	} else {
		usb_power &= ~(1 << shift);
		if (!usb_power) {
			set_wifi_power(is_power);
			WIFI_INFO("Set %s power down\n", (shift ? "WiFi":"BT"));
		}
	}
	mutex_unlock(&wifi_bt_mutex);
}

void set_usb_bt_power(int is_power)
{
	usb_power_control(is_power, BT_BIT);
}
EXPORT_SYMBOL(set_usb_bt_power);

void set_usb_wifi_power(int is_power)
{
	usb_power_control(is_power, WIFI_BIT);
}
EXPORT_SYMBOL(set_usb_wifi_power);
static int  wifi_power_open(struct inode *inode, struct file *file)
{
	struct cdev *cdevp = inode->i_cdev;

	file->private_data = cdevp;
	return 0;
}

static int  wifi_power_release(struct inode *inode, struct file *file)
{
	return 0;
}


void pci_reinit(void)
{
	struct pci_bus *bus = NULL;
	int cnt = 20;

	WIFI_INFO("pci wifi reinit!\n");

	pci_lock_rescan_remove();
	while ((bus = pci_find_next_bus(bus)) != NULL) {
		pci_rescan_bus(bus);
		WIFI_INFO("rescanning pci device\n");
		cnt--;
		if (cnt <= 0)
			break;
	}
	pci_unlock_rescan_remove();

}
EXPORT_SYMBOL(pci_reinit);

void pci_remove_reinit(unsigned int vid, unsigned int pid, unsigned int delBus)
{
	struct pci_bus *bus = NULL;
	struct pci_dev *devDevice, *devBus;
	int cnt = 20;

	WIFI_INFO("pci wifi remove and reinit\n");
	devDevice = pci_get_device(vid, pid, NULL);

	if (devDevice != NULL) {
		WIFI_INFO("device 0x%x:0x%x found, remove it\n", vid, pid);
		devBus = devDevice->bus->self;
		pci_stop_and_remove_bus_device_locked(devDevice);

		if ((devBus > 0) && (devBus != NULL)) {
			WIFI_INFO("remove ths bus this device on!\n");
			pci_stop_and_remove_bus_device_locked(devBus);
		}
	} else {
		WIFI_INFO("target pci device not found 0x%x:0x%x\n", vid, pid);
	}

	set_usb_wifi_power(0);
	msleep(200);
	set_usb_wifi_power(1);
	msleep(200);

	pci_lock_rescan_remove();
	while ((bus = pci_find_next_bus(bus)) != NULL) {
		pci_rescan_bus(bus);
		WIFI_INFO("rescanning pci device\n");
		cnt--;
		if (cnt <= 0)
			break;
	}
	pci_unlock_rescan_remove();

}
EXPORT_SYMBOL(pci_remove_reinit);

static long wifi_power_ioctl(struct file *filp,
	unsigned int cmd, unsigned long arg)
{
	char dev_type[10] = {'\0'};

	switch (cmd) {
	case USB_POWER_UP:
		set_usb_wifi_power(0);
		mdelay(200);
		set_usb_wifi_power(1);
		WIFI_INFO(KERN_INFO "ioctl Set usb_sdio wifi power up!\n");
		break;
	case USB_POWER_DOWN:
		set_usb_wifi_power(0);
		WIFI_INFO(KERN_INFO "ioctl Set usb_sdio wifi power down!\n");
		break;
	case WIFI_POWER_UP:
		set_usb_wifi_power(0);
		mdelay(200);
		set_usb_wifi_power(1);
		mdelay(200);
		pci_reinit();
		WIFI_INFO("Set sdio wifi power up!\n");
		break;
	case WIFI_POWER_DOWN:
		set_usb_wifi_power(0);
		WIFI_INFO("ioctl Set sdio wifi power down!\n");
		break;
	case SDIO_GET_DEV_TYPE:
		if (strlen(get_wifi_inf()) >= sizeof(dev_type))
			memcpy(dev_type, get_wifi_inf(),
				(sizeof(dev_type) - 1));
		else
			memcpy(dev_type, get_wifi_inf(),
				strlen(get_wifi_inf()));
		WIFI_INFO("wifi interface dev type: %s, length = %d\n",
				dev_type, (int)strlen(dev_type));
		if (copy_to_user((char __user *)arg,
				dev_type, strlen(dev_type)))
			return -ENOTTY;
		break;
	default:
		WIFI_INFO("usb wifi_power_ioctl: default !!!\n");
		return -EINVAL;
	}
	return 0;
}

static const struct file_operations wifi_power_fops = {
	.unlocked_ioctl = wifi_power_ioctl,
	.compat_ioctl = wifi_power_ioctl,
	.open	= wifi_power_open,
	.release	= wifi_power_release,
};

static struct class wifi_power_class = {
	.name = WIFI_POWER_CLASS_NAME,
	.owner = THIS_MODULE,
};

static int wifi_setup_dt(void)
{
	int ret;

	WIFI_INFO("wifi_setup_dt\n");
	if (!wifi_info.plat_info_valid) {
		WIFI_INFO("wifi_setup_dt : invalid device tree setting\n");
		return -1;
	}

	/* setup irq */
	if (wifi_info.interrupt_pin) {
		ret = gpio_request(wifi_info.interrupt_pin,
			OWNER_NAME);
		if (ret)
			WIFI_INFO("interrupt_pin request failed(%d)\n", ret);

		ret = gpio_direction_input(wifi_info.interrupt_pin);
		if (ret)
			WIFI_INFO("set interrupt_pin input failed(%d)\n", ret);

		wifi_info.irq_num = gpio_to_irq(wifi_info.interrupt_pin);
		if (wifi_info.irq_num)
			WIFI_INFO("irq num is:(%d)\n", wifi_info.irq_num);

		SHOW_PIN_OWN("interrupt_pin", wifi_info.interrupt_pin);
	}

	/* setup power */
	if (wifi_info.power_on_pin) {
		ret = gpio_request(wifi_info.power_on_pin, OWNER_NAME);
		if (ret)
			WIFI_INFO("power_on_pin request failed(%d)\n", ret);
		if (wifi_info.power_on_pin_level)
			ret = set_power(1);
		else
			ret = set_power(0);
		if (ret)
			WIFI_INFO("power_on_pin output failed(%d)\n", ret);
		SHOW_PIN_OWN("power_on_pin", wifi_info.power_on_pin);
	}

	if (wifi_info.power_on_pin2) {
		ret = gpio_request(wifi_info.power_on_pin2,
			OWNER_NAME);
		if (ret)
			WIFI_INFO("power_on_pin2 request failed(%d)\n", ret);
		if (wifi_info.power_on_pin_level)
			ret = set_power2(1);
		else
			ret = set_power2(0);
		if (ret)
			WIFI_INFO("power_on_pin2 output failed(%d)\n", ret);
		SHOW_PIN_OWN("power_on_pin2", wifi_info.power_on_pin2);
	}

	return 0;
}

static void wifi_teardown_dt(void)
{

	WIFI_INFO("wifi_teardown_dt\n");
	if (!wifi_info.plat_info_valid) {
		WIFI_INFO("wifi_teardown_dt : invalid device tree setting\n");
		return;
	}

	if (wifi_info.power_on_pin)
		gpio_free(wifi_info.power_on_pin);

	if (wifi_info.power_on_pin2)
		gpio_free(wifi_info.power_on_pin2);

	if (wifi_info.interrupt_pin)
		gpio_free(wifi_info.interrupt_pin);

}

/*
 * for gxb ,m8b soc
 * single pwm channel
 */
int pwm_single_channel_conf(struct wifi_plat_info *plat)
{
	struct pwm_device *pwm = plat->sdata.pwm;
	struct pwm_state pstate;
	int duty_value;
	int ret;

	/* get pwm duty_cycle property */
	ret = of_property_read_u32(plat->dev->of_node, "duty_cycle",
	&duty_value);
	if (ret) {
		pr_err("not config pwm duty_cycle");
		return ret;
	}
	/* get pwm device */
	pwm = devm_pwm_get(plat->dev, NULL);
	if (IS_ERR(pwm)) {
		ret = PTR_ERR(pwm);
		dev_err(plat->dev, "Failed to get PWM: %d\n", ret);
		return ret;
	}
	/* config pwm */
	pwm_init_state(pwm, &pstate);
	pwm_config(pwm, duty_value, pstate.period);
	pwm_enable(pwm);

	WIFI_INFO("pwm period val=%d, pwm duty val=%d\n",
	pstate.period, pstate.duty_cycle);
	WIFI_INFO("wifi pwm conf ok\n");

	return 0;
}

int pwm_double_channel_conf_dt(struct wifi_plat_info *plat)
{
	phandle pwm_phandle;
	int ret;
	struct device_node *wifinode = plat->dev->of_node;
	struct device_node *pnode = NULL;
	struct device_node *child;

	ret = of_property_read_u32(wifinode, "pwm_config", &pwm_phandle);
	if (ret) {
		pr_err("not match wifi_pwm_config node\n");
		return -1;
	} else {
		pnode = of_find_node_by_phandle(pwm_phandle);
		if (!pnode) {
			pr_err("can't find wifi_pwm_config node\n");
			return -1;
		}
	}

	/*request for pwm device */
	for_each_child_of_node(pnode, child) {
		struct pwm_double_data *pdata =
			&plat->ddata.pwms[plat->ddata.num_pwm];

		pdata->pwm = devm_of_pwm_get(plat->dev, child, NULL);
		if (IS_ERR(pdata->pwm)) {
			ret = PTR_ERR(pdata->pwm);
			dev_err(plat->dev, "unable to request PWM%d\n",
			plat->ddata.num_pwm);
			return ret;
		}
		ret = of_property_read_u32(child, "duty-cycle",
			&(pdata->duty_cycle));
		if (ret) {
			pr_err("not %d duty_cycle parameters\n",
				plat->ddata.num_pwm);
			return ret;
		}
		ret = of_property_read_u32(child, "times",
			&(pdata->pwm_times));
		if (ret) {
			pr_err("not %d pwm_times parameters\n",
				plat->ddata.num_pwm);
			return ret;
		}
		plat->ddata.num_pwm++;
	}
	WIFI_INFO("wifi pwm dt ok\n");

	return 0;
}
/*
 *configuration for double pwm
 */
int pwm_double_channel_conf(struct wifi_plat_info *plat)
{
	struct pwm_double_data pwm_data1 = plat->ddata.pwms[0];
	struct pwm_double_data pwm_data2 = plat->ddata.pwms[1];
	struct pwm_device *pwm1 = pwm_data1.pwm;
	struct pwm_device *pwm2 = pwm_data2.pwm;
	struct meson_pwm *meson1 = to_meson_pwm(pwm1->chip);
	struct meson_pwm *meson2 = to_meson_pwm(pwm2->chip);
	struct pwm_state pstate1;
	struct pwm_state pstate2;
	unsigned int pwm1_duty = pwm_data1.duty_cycle;
	unsigned int pwm1_times = pwm_data1.pwm_times;
	unsigned int pwm2_duty = pwm_data2.duty_cycle;
	unsigned int pwm2_times = pwm_data2.pwm_times;

	/*init for pwm2 device*/
	pwm_init_state(pwm1, &pstate1);
	pwm_init_state(pwm2, &pstate2);

	pwm_config(pwm1, pwm1_duty, pstate1.period);
	pwm_config(pwm2, pwm2_duty, pstate2.period);

	pwm_set_times(meson1, MESON_PWM_0, pwm1_times);
	pwm_set_times(meson2, MESON_PWM_2, pwm2_times);

	pwm_enable(pwm1);
	pwm_enable(pwm2);
	WIFI_INFO("wifi pwm conf ok\n");

	return 0;
}

static int wifi_dev_probe(struct platform_device *pdev)
{
	int ret;
	unsigned int pwm_misc;

#ifdef CONFIG_OF
	struct wifi_plat_info *plat;
	const char *value;
	struct gpio_desc *desc;
#else
	struct wifi_plat_info *plat =
	 (struct wifi_plat_info *)(pdev->dev.platform_data);
#endif

#ifdef CONFIG_OF
	if (pdev->dev.of_node) {
		plat = wifi_get_driver_data(pdev);
		plat->plat_info_valid = 0;
		plat->dev = &pdev->dev;

		ret = of_property_read_string(pdev->dev.of_node,
			"interrupt_pin", &value);
		if (ret) {
			WIFI_INFO("no interrupt pin");
			plat->interrupt_pin = 0;
		} else {
			desc = of_get_named_gpiod_flags(pdev->dev.of_node,
				"interrupt_pin", 0, NULL);
			plat->interrupt_desc = desc;
			plat->interrupt_pin = desc_to_gpio(desc);

			ret = of_property_read_string(pdev->dev.of_node,
			"irq_trigger_type", &value);
			if (ret) {
				WIFI_INFO("no irq_trigger_type");
				plat->irq_trigger_type = 0;
				return -1;
			}

			if (strcmp(value, "GPIO_IRQ_HIGH") == 0)
				plat->irq_trigger_type = GPIO_IRQ_HIGH;
			else if (strcmp(value, "GPIO_IRQ_LOW") == 0)
				plat->irq_trigger_type = GPIO_IRQ_LOW;
			else if (strcmp(value, "GPIO_IRQ_RISING") == 0)
				plat->irq_trigger_type = GPIO_IRQ_RISING;
			else if (strcmp(value, "GPIO_IRQ_FALLING") == 0)
				plat->irq_trigger_type = GPIO_IRQ_FALLING;
			else {
				WIFI_INFO("unknown irq trigger type-%s\n",
				 value);
				return -1;
			}
		}

		ret = of_property_read_string(pdev->dev.of_node,
			"power_on_pin", &value);
		if (ret) {
			WIFI_INFO("no power_on_pin");
			plat->power_on_pin = 0;
			plat->power_on_pin_OD = 0;
		} else {
			wifi_power_gpio = 1;
			desc = of_get_named_gpiod_flags(pdev->dev.of_node,
			"power_on_pin", 0, NULL);
			plat->powe_desc = desc;
			plat->power_on_pin = desc_to_gpio(desc);
		}

		ret = of_property_read_u32(pdev->dev.of_node,
		"power_on_pin_level", &plat->power_on_pin_level);

		ret = of_property_read_u32(pdev->dev.of_node,
		"power_on_pin_OD", &plat->power_on_pin_OD);
		if (ret)
			plat->power_on_pin_OD = 0;
		pr_info("wifi: power_on_pin_OD = %d;\n", plat->power_on_pin_OD);
		ret = of_property_read_string(pdev->dev.of_node,
			"power_on_pin2", &value);
		if (ret) {
			WIFI_INFO("no power_on_pin2");
			plat->power_on_pin2 = 0;
		} else {
			wifi_power_gpio2 = 1;
			desc = of_get_named_gpiod_flags(pdev->dev.of_node,
				"power_on_pin2", 0, NULL);
			plat->power_on_pin2 = desc_to_gpio(desc);
		}

		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXTVBB) {
			ret = pwm_double_channel_conf_dt(plat);
			if (ret != 0) {
				WIFI_INFO("pwm_double_channel_conf_dt error\n");
			} else {
				ret = pwm_double_channel_conf(plat);
				if (ret != 0)
					WIFI_INFO("pwm_double_channel_conf error\n");
			}
		} else if (get_cpu_type() == MESON_CPU_MAJOR_ID_GXBB) {
			ret = pwm_single_channel_conf(plat);
			if (ret)
				pr_err("pwm config err\n");
		} else if (get_cpu_type() == MESON_CPU_MAJOR_ID_M8B) {
			WIFI_INFO("set pwm as 32k output");
			aml_write_cbus(0x21b0, 0x7980799);
			pwm_misc = aml_read_cbus(0x21b2);
			pwm_misc &= ~((0x7f << 8) | (3 << 4) |
				(1 << 2) | (1 << 0));
			pwm_misc |= ((1 << 15) | (4 << 8) | (2 << 4));
			aml_write_cbus(0x21b2, pwm_misc);
			aml_write_cbus(0x21b2, (pwm_misc | (1 << 0)));
		}
		if (of_get_property(pdev->dev.of_node,
			"dhd_static_buf", NULL)) {
			WIFI_INFO("dhd_static_buf setup\n");
			bcmdhd_init_wlan_mem();
		}

		plat->plat_info_valid = 1;

		WIFI_INFO("interrupt_pin=%d\n", plat->interrupt_pin);
		WIFI_INFO("irq_num=%d, irq_trigger_type=%d\n",
			plat->irq_num, plat->irq_trigger_type);
		WIFI_INFO("power_on_pin=%d\n", plat->power_on_pin);
		WIFI_INFO("clock_32k_pin=%d\n", plat->clock_32k_pin);
	}
#endif
	ret = alloc_chrdev_region(&wifi_power_devno,
			0, 1, WIFI_POWER_DRIVER_NAME);
	if (ret < 0) {
		ret = -ENODEV;
		goto out;
	}
	ret = class_register(&wifi_power_class);
	if (ret < 0)
		goto error1;
	wifi_power_cdev = cdev_alloc();
	if (!wifi_power_cdev)
		goto error2;
	cdev_init(wifi_power_cdev, &wifi_power_fops);
	wifi_power_cdev->owner = THIS_MODULE;
	ret = cdev_add(wifi_power_cdev, wifi_power_devno, 1);
	if (ret)
		goto error3;
	devp = device_create(&wifi_power_class, NULL,
			wifi_power_devno, NULL, WIFI_POWER_DEVICE_NAME);
	if (IS_ERR(devp)) {
		ret = PTR_ERR(devp);
		goto error3;
	}
	devp->platform_data = pdata;

	wifi_setup_dt();

	return 0;
error3:
	cdev_del(wifi_power_cdev);
error2:
	class_unregister(&wifi_power_class);
error1:
	unregister_chrdev_region(wifi_power_devno, 1);
out:
	return ret;
}

static int wifi_dev_remove(struct platform_device *pdev)
{
	WIFI_INFO("wifi_dev_remove\n");
	wifi_teardown_dt();
	return 0;
}

static struct platform_driver wifi_plat_driver = {
	.probe = wifi_dev_probe,
	.remove = wifi_dev_remove,
	.driver = {
	.name = "aml_wifi",
	.owner = THIS_MODULE,
	.of_match_table = wifi_match
	},
};

static int __init wifi_dt_init(void)
{
	int ret;

	ret = platform_driver_register(&wifi_plat_driver);
	return ret;
}
/* module_init(wifi_dt_init); */
fs_initcall_sync(wifi_dt_init);

static void __exit wifi_dt_exit(void)
{
	platform_driver_unregister(&wifi_plat_driver);
}
module_exit(wifi_dt_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("AMLOGIC");
MODULE_DESCRIPTION("wifi device tree driver");

/**************** wifi mac *****************/
u8 WIFI_MAC[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
static unsigned char chartonum(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'A' && c <= 'F')
		return (c - 'A') + 10;
	if (c >= 'a' && c <= 'f')
		return (c - 'a') + 10;
	return 0;
}

static int __init mac_addr_set(char *line)
{
	unsigned char mac[6];
	int i = 0;

	WIFI_INFO("try to wifi mac from emmc key!\n");
	for (i = 0; i < 6 && line[0] != '\0' && line[1] != '\0'; i++) {
		mac[i] = chartonum(line[0]) << 4 | chartonum(line[1]);
		line += 3;
	}
	memcpy(WIFI_MAC, mac, 6);
	WIFI_INFO("uboot setup mac-addr: %x:%x:%x:%x:%x:%x\n",
	WIFI_MAC[0], WIFI_MAC[1], WIFI_MAC[2], WIFI_MAC[3], WIFI_MAC[4],
	WIFI_MAC[5]);

	return 1;
}

__setup("mac_wifi=", mac_addr_set);

u8 *wifi_get_mac(void)
{
	return WIFI_MAC;
}
EXPORT_SYMBOL(wifi_get_mac);

void extern_wifi_set_enable(int is_on)
{

	if (is_on) {
		set_wifi_power(1);
		WIFI_INFO("WIFI  Enable! %d\n", wifi_info.power_on_pin);
	} else {
		set_wifi_power(0);
		WIFI_INFO("WIFI  Disable! %d\n", wifi_info.power_on_pin);

	}
}
EXPORT_SYMBOL(extern_wifi_set_enable);

int wifi_irq_num(void)
{
	return wifi_info.irq_num;
}
EXPORT_SYMBOL(wifi_irq_num);

int wifi_irq_trigger_level(void)
{
	return wifi_info.irq_trigger_type;
}
EXPORT_SYMBOL(wifi_irq_trigger_level);
MODULE_DESCRIPTION("Amlogic S912/wifi driver");
MODULE_AUTHOR("Kevin Hilman <khilman@baylibre.com>");
MODULE_LICENSE("GPL");
