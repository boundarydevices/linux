/*
 * drivers/amlogic/media/vout/lcd/lcd_vout.c
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
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/clk.h>
#include <linux/of_device.h>
#include <linux/amlogic/iomap.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#endif
#include <linux/reset.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#include <linux/amlogic/media/vout/lcd/lcd_unifykey.h>
#ifdef CONFIG_AMLOGIC_LCD_EXTERN
#include <linux/amlogic/media/vout/lcd/lcd_extern.h>
#endif
#include "lcd_reg.h"
#include "lcd_common.h"

#define LCD_CDEV_NAME  "lcd"

unsigned char lcd_debug_print_flag;
unsigned char lcd_resume_flag;
static struct aml_lcd_drv_s *lcd_driver;

struct mutex lcd_power_mutex;
struct mutex lcd_vout_mutex;
int lcd_vout_serve_bypass;

static char lcd_propname[20] = "null";

struct lcd_cdev_s {
	dev_t           devno;
	struct cdev     cdev;
	struct device   *dev;
};

static struct lcd_cdev_s *lcd_cdev;

/* *********************************************************
 * lcd config define
 * *********************************************************
 */
static struct ttl_config_s lcd_ttl_config = {
	.clk_pol = 0,
	.sync_valid = ((1 << 1) | (1 << 0)),
	.swap_ctrl = ((0 << 1) | (0 << 0)),
};

static struct lvds_config_s lcd_lvds_config = {
	.lvds_vswing = 1,
	.lvds_repack = 1,
	.dual_port = 0,
	.pn_swap = 0,
	.port_swap = 0,
	.lane_reverse = 0,
	.port_sel = 0,
	.phy_vswing = LVDS_PHY_VSWING_DFT,
	.phy_preem = LVDS_PHY_PREEM_DFT,
};

static struct vbyone_config_s lcd_vbyone_config = {
	.lane_count = 8,
	.region_num = 2,
	.byte_mode = 4,
	.color_fmt = 4,
	.phy_div = 1,
	.bit_rate = 0,
	.phy_vswing = VX1_PHY_VSWING_DFT,
	.phy_preem = VX1_PHY_PREEM_DFT,
	.intr_en = 1,
	.vsync_intr_en = 1,

	.ctrl_flag = 0,
	.power_on_reset_delay = VX1_PWR_ON_RESET_DLY_DFT,
	.hpd_data_delay = VX1_HPD_DATA_DELAY_DFT,
	.cdr_training_hold = VX1_CDR_TRAINING_HOLD_DFT,
};

static unsigned char dsi_init_on_table[DSI_INIT_ON_MAX] = {0xff, 0xff};
static unsigned char dsi_init_off_table[DSI_INIT_OFF_MAX] = {0xff, 0xff};

static struct dsi_config_s lcd_mipi_config = {
	.lane_num = 4,
	.bit_rate_max = 550, /* MHz */
	.factor_numerator   = 0,
	.factor_denominator = 100,
	.operation_mode_init = 1,    /* 0=video mode, 1=command mode */
	.operation_mode_display = 0, /* 0=video mode, 1=command mode */
	.video_mode_type = 2, /* 0=sync_pulse, 1=sync_event, 2=burst */
	.clk_lp_continuous = 1, /* 0=stop, 1=continue */
	.phy_stop_wait = 0,   /* 0=auto, 1=standard, 2=slow */

	.dsi_init_on  = &dsi_init_on_table[0],
	.dsi_init_off = &dsi_init_off_table[0],
	.extern_init = 0xff,
		/* ext_index if needed, must match ext_config index;
		 *    0xff for invalid
		 */
};

static struct lcd_power_ctrl_s lcd_power_config = {
	.cpu_gpio = {
		{.probe_flag = 0, .register_flag = 0,},
		{.probe_flag = 0, .register_flag = 0,},
		{.probe_flag = 0, .register_flag = 0,},
		{.probe_flag = 0, .register_flag = 0,},
		{.probe_flag = 0, .register_flag = 0,},
		{.probe_flag = 0, .register_flag = 0,},
		{.probe_flag = 0, .register_flag = 0,},
		{.probe_flag = 0, .register_flag = 0,},
		{.probe_flag = 0, .register_flag = 0,},
		{.probe_flag = 0, .register_flag = 0,},
	},
	.power_on_step = {
		{
			.type = LCD_POWER_TYPE_MAX,
		},
	},
	.power_off_step = {
		{
			.type = LCD_POWER_TYPE_MAX,
		},
	},
};

static struct lcd_config_s lcd_config_dft = {
	.lcd_propname = lcd_propname,
	.lcd_basic = {
		.lcd_type = LCD_TYPE_MAX,
	},
	.lcd_timing = {
		.lcd_clk = 0,
		.clk_auto = 1,
		.ss_level = 0,
		.fr_adjust_type = 0,
	},
	.hdr_info = {
		.hdr_support = 0,
		.features = 0,
		.primaries_r_x = 0,
		.primaries_r_y = 0,
		.primaries_g_x = 0,
		.primaries_g_y = 0,
		.primaries_b_x = 0,
		.primaries_b_y = 0,
		.white_point_x = 0,
		.white_point_y = 0,
		.luma_max = 0,
		.luma_min = 0,
		.luma_avg = 0,
	},
	.lcd_control = {
		.ttl_config = &lcd_ttl_config,
		.lvds_config = &lcd_lvds_config,
		.vbyone_config = &lcd_vbyone_config,
		.mipi_config = &lcd_mipi_config,
	},
	.lcd_power = &lcd_power_config,
	.pinmux_flag = 0,
	.change_flag = 0,
};

static struct vinfo_s lcd_vinfo = {
	.name = "panel",
	.mode = VMODE_LCD,
	.viu_color_fmt = COLOR_FMT_RGB444,
	.vout_device = NULL,
};

struct aml_lcd_drv_s *aml_lcd_get_driver(void)
{
	return lcd_driver;
}
/* ********************************************************* */

static void lcd_power_tiny_ctrl(int status)
{
	struct lcd_power_ctrl_s *lcd_power = lcd_driver->lcd_config->lcd_power;
	struct lcd_power_step_s *power_step;
#ifdef CONFIG_AMLOGIC_LCD_EXTERN
	struct aml_lcd_extern_driver_s *ext_drv;
#endif
	int i, index;

	LCDPR("%s: %d\n", __func__, status);
	i = 0;
	while (i < LCD_PWR_STEP_MAX) {
		if (status)
			power_step = &lcd_power->power_on_step[i];
		else
			power_step = &lcd_power->power_off_step[i];

		if (power_step->type >= LCD_POWER_TYPE_MAX)
			break;
		if (lcd_debug_print_flag) {
			LCDPR("power_tiny_ctrl: %d, step %d\n", status, i);
			LCDPR("type=%d, index=%d, value=%d, delay=%d\n",
				power_step->type, power_step->index,
				power_step->value, power_step->delay);
		}
		switch (power_step->type) {
		case LCD_POWER_TYPE_CPU:
			index = power_step->index;
			lcd_cpu_gpio_set(index, power_step->value);
			break;
		case LCD_POWER_TYPE_PMU:
			LCDPR("to do\n");
			break;
		case LCD_POWER_TYPE_SIGNAL:
			if (status)
				lcd_driver->driver_tiny_enable();
			else
				lcd_driver->driver_tiny_disable();
			break;
#ifdef CONFIG_AMLOGIC_LCD_EXTERN
		case LCD_POWER_TYPE_EXTERN:
			index = power_step->index;
			ext_drv = aml_lcd_extern_get_driver(index);
			if (ext_drv) {
				if (status) {
					if (ext_drv->power_on)
						ext_drv->power_on();
					else
						LCDERR("no ext power on\n");
				} else {
					if (ext_drv->power_off)
						ext_drv->power_off();
					else
						LCDERR("no ext power off\n");
				}
			}
			break;
#endif
		default:
			break;
		}
		if (power_step->delay)
			mdelay(power_step->delay);
		i++;
	}

	if (lcd_debug_print_flag)
		LCDPR("%s: %d finished\n", __func__, status);
}

static void lcd_power_ctrl(int status)
{
	struct lcd_power_ctrl_s *lcd_power = lcd_driver->lcd_config->lcd_power;
	struct lcd_power_step_s *power_step;
#ifdef CONFIG_AMLOGIC_LCD_EXTERN
	struct aml_lcd_extern_driver_s *ext_drv;
#endif
	int i, index;
	int ret = 0;

	LCDPR("%s: %d\n", __func__, status);
	i = 0;
	while (i < LCD_PWR_STEP_MAX) {
		if (status)
			power_step = &lcd_power->power_on_step[i];
		else
			power_step = &lcd_power->power_off_step[i];

		if (power_step->type >= LCD_POWER_TYPE_MAX)
			break;
		if (lcd_debug_print_flag) {
			LCDPR("power_ctrl: %d, step %d\n", status, i);
			LCDPR("type=%d, index=%d, value=%d, delay=%d\n",
				power_step->type, power_step->index,
				power_step->value, power_step->delay);
		}
		switch (power_step->type) {
		case LCD_POWER_TYPE_CPU:
			index = power_step->index;
			lcd_cpu_gpio_set(index, power_step->value);
			break;
		case LCD_POWER_TYPE_PMU:
			LCDPR("to do\n");
			break;
		case LCD_POWER_TYPE_SIGNAL:
			if (status)
				ret = lcd_driver->driver_init();
			else
				lcd_driver->driver_disable();
			break;
#ifdef CONFIG_AMLOGIC_LCD_EXTERN
		case LCD_POWER_TYPE_EXTERN:
			index = power_step->index;
			ext_drv = aml_lcd_extern_get_driver(index);
			if (ext_drv) {
				if (status) {
					if (ext_drv->power_on)
						ext_drv->power_on();
					else
						LCDERR("no ext power on\n");
				} else {
					if (ext_drv->power_off)
						ext_drv->power_off();
					else
						LCDERR("no ext power off\n");
				}
			}
			break;
#endif
		default:
			break;
		}
		if (power_step->delay)
			mdelay(power_step->delay);
		i++;
	}

	if (lcd_debug_print_flag)
		LCDPR("%s: %d finished\n", __func__, status);
}

static void lcd_module_enable(void)
{
	mutex_lock(&lcd_vout_mutex);

	lcd_driver->driver_init_pre();
	lcd_driver->power_ctrl(1);
	lcd_driver->lcd_status = 1;
	lcd_driver->lcd_config->change_flag = 0;

	mutex_unlock(&lcd_vout_mutex);
}

static void lcd_module_disable(void)
{
	mutex_lock(&lcd_vout_mutex);

	lcd_driver->lcd_status = 0;
	lcd_driver->power_ctrl(0);

	mutex_unlock(&lcd_vout_mutex);
}

static void lcd_module_reset(void)
{
	mutex_lock(&lcd_vout_mutex);

	lcd_driver->lcd_status = 0;
	lcd_driver->power_ctrl(0);

	msleep(500);

	lcd_driver->driver_init_pre();
	lcd_driver->power_ctrl(1);
	lcd_driver->lcd_status = 1;
	lcd_driver->lcd_config->change_flag = 0;

	mutex_unlock(&lcd_vout_mutex);
}

static void lcd_module_tiny_reset(void)
{
	mutex_lock(&lcd_vout_mutex);

	lcd_driver->lcd_status = 0;
	lcd_power_tiny_ctrl(0);
	mdelay(500);
	lcd_power_tiny_ctrl(1);
	lcd_driver->lcd_status = 1;
	lcd_driver->lcd_config->change_flag = 0;

	mutex_unlock(&lcd_vout_mutex);
}

static void lcd_resume_work(struct work_struct *p_work)
{
	mutex_lock(&lcd_power_mutex);
	aml_lcd_notifier_call_chain(LCD_EVENT_POWER_ON, NULL);
	LCDPR("%s finished\n", __func__);
	mutex_unlock(&lcd_power_mutex);
}

/* ****************************************
 * lcd notify
 * ****************************************
 */
static int lcd_power_notifier(struct notifier_block *nb,
		unsigned long event, void *data)
{
	if (event & LCD_EVENT_LCD_ON) {
		if (lcd_debug_print_flag)
			LCDPR("%s: 0x%lx\n", __func__, event);
		lcd_module_enable();
	} else if (event & LCD_EVENT_LCD_OFF) {
		if (lcd_debug_print_flag)
			LCDPR("%s: 0x%lx\n", __func__, event);
		lcd_module_disable();
	} else {
		return NOTIFY_DONE;
	}

	return NOTIFY_OK;
}

static struct notifier_block lcd_power_nb = {
	.notifier_call = lcd_power_notifier,
	.priority = LCD_PRIORITY_POWER_LCD,
};

static int lcd_interface_notifier(struct notifier_block *nb,
		unsigned long event, void *data)
{
	if (event & LCD_EVENT_IF_ON) {
		if (lcd_debug_print_flag)
			LCDPR("%s: 0x%lx\n", __func__, event);
		lcd_driver->power_tiny_ctrl(1);
	} else if (event & LCD_EVENT_IF_OFF) {
		if (lcd_debug_print_flag)
			LCDPR("%s: 0x%lx\n", __func__, event);
		lcd_driver->power_tiny_ctrl(0);
	} else {
		return NOTIFY_DONE;
	}

	return NOTIFY_OK;
}

static struct notifier_block lcd_interface_nb = {
	.notifier_call = lcd_interface_notifier,
	.priority = LCD_PRIORITY_POWER_LCD,
};

static int lcd_bl_select_notifier(struct notifier_block *nb,
		unsigned long event, void *data)
{
	unsigned int *index;

	if ((event & LCD_EVENT_BACKLIGHT_SEL) == 0)
		return NOTIFY_DONE;
	/* LCDPR("%s: 0x%lx\n", __func__, event); */

	index = (unsigned int *)data;
	*index = lcd_driver->lcd_config->backlight_index;

	return NOTIFY_OK;
}

static struct notifier_block lcd_bl_select_nb = {
	.notifier_call = lcd_bl_select_notifier,
};
/* **************************************** */

/* ************************************************************* */
/* lcd ioctl                                                     */
/* ************************************************************* */
static int lcd_io_open(struct inode *inode, struct file *file)
{
	struct lcd_cdev_s *lcd_cdev;

	LCDPR("%s\n", __func__);
	lcd_cdev = container_of(inode->i_cdev, struct lcd_cdev_s, cdev);
	file->private_data = lcd_cdev;
	return 0;
}

static int lcd_io_release(struct inode *inode, struct file *file)
{
	LCDPR("%s\n", __func__);
	file->private_data = NULL;
	return 0;
}

static long lcd_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	void __user *argp;
	int mcd_nr;
	struct lcd_hdr_info_s *hdr_info = &lcd_driver->lcd_config->hdr_info;

	mcd_nr = _IOC_NR(cmd);
	LCDPR("%s: cmd_dir = 0x%x, cmd_nr = 0x%x\n",
		__func__, _IOC_DIR(cmd), mcd_nr);

	argp = (void __user *)arg;
	switch (mcd_nr) {
	case LCD_IOC_NR_GET_HDR_INFO:
		if (copy_to_user(argp, hdr_info, sizeof(struct lcd_hdr_info_s)))
			ret = -EFAULT;
		break;
	case LCD_IOC_NR_SET_HDR_INFO:
		if (copy_from_user(hdr_info, argp,
			sizeof(struct lcd_hdr_info_s))) {
			ret = -EFAULT;
		} else {
			lcd_hdr_vinfo_update();
			if (lcd_debug_print_flag) {
				LCDPR("set hdr_info:\n"
					"hdr_support          %d\n"
					"features             %d\n"
					"primaries_r_x        %d\n"
					"primaries_r_y        %d\n"
					"primaries_g_x        %d\n"
					"primaries_g_y        %d\n"
					"primaries_b_x        %d\n"
					"primaries_b_y        %d\n"
					"white_point_x        %d\n"
					"white_point_y        %d\n"
					"luma_max             %d\n"
					"luma_min             %d\n\n",
					hdr_info->hdr_support,
					hdr_info->features,
					hdr_info->primaries_r_x,
					hdr_info->primaries_r_y,
					hdr_info->primaries_g_x,
					hdr_info->primaries_g_y,
					hdr_info->primaries_b_x,
					hdr_info->primaries_b_y,
					hdr_info->white_point_x,
					hdr_info->white_point_y,
					hdr_info->luma_max,
					hdr_info->luma_min);
			}
		}
		break;
	default:
		LCDERR("not support ioctl cmd_nr: 0x%x\n", mcd_nr);
		ret = -EINVAL;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long lcd_compat_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	unsigned long ret;

	arg = (unsigned long)compat_ptr(arg);
	ret = lcd_ioctl(file, cmd, arg);
	return ret;
}
#endif

static const struct file_operations lcd_fops = {
	.owner          = THIS_MODULE,
	.open           = lcd_io_open,
	.release        = lcd_io_release,
	.unlocked_ioctl = lcd_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = lcd_compat_ioctl,
#endif
};

static int lcd_fops_create(void)
{
	int ret = 0;

	lcd_cdev = kmalloc(sizeof(struct lcd_cdev_s), GFP_KERNEL);
	if (!lcd_cdev) {
		LCDERR("%s: failed to allocate lcd_cdev\n", __func__);
		return -1;
	}

	ret = alloc_chrdev_region(&lcd_cdev->devno, 0, 1, LCD_CDEV_NAME);
	if (ret < 0) {
		LCDERR("%s: failed to alloc devno\n", __func__);
		kfree(lcd_cdev);
		lcd_cdev = NULL;
		return -1;
	}

	cdev_init(&lcd_cdev->cdev, &lcd_fops);
	lcd_cdev->cdev.owner = THIS_MODULE;
	ret = cdev_add(&lcd_cdev->cdev, lcd_cdev->devno, 1);
	if (ret) {
		LCDERR("%s: failed to add cdev\n", __func__);
		unregister_chrdev_region(lcd_cdev->devno, 1);
		kfree(lcd_cdev);
		lcd_cdev = NULL;
		return -1;
	}

	lcd_cdev->dev = device_create(lcd_driver->lcd_debug_class, NULL,
			lcd_cdev->devno, NULL, LCD_CDEV_NAME);
	if (IS_ERR(lcd_cdev->dev)) {
		LCDERR("%s: failed to add device\n", __func__);
		ret = PTR_ERR(lcd_cdev->dev);
		cdev_del(&lcd_cdev->cdev);
		unregister_chrdev_region(lcd_cdev->devno, 1);
		kfree(lcd_cdev);
		lcd_cdev = NULL;
		return -1;
	}

	LCDPR("%s OK\n", __func__);
	return 0;
}

static void lcd_fops_remove(void)
{
	cdev_del(&lcd_cdev->cdev);
	unregister_chrdev_region(lcd_cdev->devno, 1);
	kfree(lcd_cdev);
	lcd_cdev = NULL;
}
/* ************************************************************* */

static void lcd_init_vout(void)
{
	switch (lcd_driver->lcd_mode) {
#ifdef CONFIG_AMLOGIC_LCD_TV
	case LCD_MODE_TV:
		lcd_tv_vout_server_init();
		break;
#endif
#ifdef CONFIG_AMLOGIC_LCD_TABLET
	case LCD_MODE_TABLET:
		lcd_tablet_vout_server_init();
		break;
#endif
	default:
		LCDERR("invalid lcd mode: %d\n", lcd_driver->lcd_mode);
		break;
	}
}

static int lcd_mode_probe(struct device *dev)
{
	int ret;

	switch (lcd_driver->lcd_mode) {
#ifdef CONFIG_AMLOGIC_LCD_TV
	case LCD_MODE_TV:
		lcd_tv_probe(dev);
		break;
#endif
#ifdef CONFIG_AMLOGIC_LCD_TABLET
	case LCD_MODE_TABLET:
		lcd_tablet_probe(dev);
		break;
#endif
	default:
		LCDERR("invalid lcd mode: %d\n", lcd_driver->lcd_mode);
		break;
	}

	lcd_class_creat();
	lcd_fops_create();

	ret = aml_lcd_notifier_register(&lcd_interface_nb);
	if (ret)
		LCDERR("register aml_bl_select_notifier failed\n");
	ret = aml_lcd_notifier_register(&lcd_bl_select_nb);
	if (ret)
		LCDERR("register aml_bl_select_notifier failed\n");
	ret = aml_lcd_notifier_register(&lcd_power_nb);
	if (ret)
		LCDPR("register lcd_power_notifier failed\n");

	/* add notifier for video sync_duration info refresh */
	vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE,
		&lcd_driver->lcd_info->mode);

	return 0;
}

static int lcd_mode_remove(struct device *dev)
{
	switch (lcd_driver->lcd_mode) {
#ifdef CONFIG_AMLOGIC_LCD_TV
	case LCD_MODE_TV:
		lcd_tv_remove(dev);
		break;
#endif
#ifdef CONFIG_AMLOGIC_LCD_TABLET
	case LCD_MODE_TABLET:
		lcd_tablet_remove(dev);
		break;
#endif
	default:
		LCDPR("invalid lcd mode\n");
		break;
	}
	return 0;
}

static void lcd_config_probe_delayed(struct work_struct *work)
{
	int key_init_flag = 0;
	int i = 0;
	int ret;

	if (lcd_driver->lcd_key_valid) {
		key_init_flag = key_unify_get_init_flag();
		while (key_init_flag == 0) {
			if (i++ >= LCD_UNIFYKEY_WAIT_TIMEOUT)
				break;
			msleep(20);
			key_init_flag = key_unify_get_init_flag();
		}
		LCDPR("key_init_flag=%d, i=%d\n", key_init_flag, i);
	}
	ret = lcd_mode_probe(lcd_driver->dev);
	if (ret) {
		kfree(lcd_driver);
		lcd_driver = NULL;
		LCDERR("probe exit\n");
	}
}

static void lcd_config_default(void)
{
	struct lcd_config_s *pconf;

	pconf = lcd_driver->lcd_config;
	pconf->lcd_basic.h_active = lcd_vcbus_read(ENCL_VIDEO_HAVON_END)
			- lcd_vcbus_read(ENCL_VIDEO_HAVON_BEGIN) + 1;
	pconf->lcd_basic.v_active = lcd_vcbus_read(ENCL_VIDEO_VAVON_ELINE)
			- lcd_vcbus_read(ENCL_VIDEO_VAVON_BLINE) + 1;
	if (lcd_vcbus_read(ENCL_VIDEO_EN)) {
		lcd_driver->lcd_status = 1;
		lcd_resume_flag = 1;
	} else {
		lcd_driver->lcd_status = 0;
		lcd_resume_flag = 0;
	}
	LCDPR("status: %d\n", lcd_driver->lcd_status);
}

static int lcd_config_probe(struct platform_device *pdev)
{
	const char *str;
	unsigned int val;
	int ret = 0;

	if (lcd_driver->dev->of_node == NULL) {
		LCDERR("dev of_node is null\n");
		lcd_driver->lcd_mode = LCD_MODE_MAX;
		return -1;
	}

	/* lcd driver assign */
	ret = of_property_read_string(lcd_driver->dev->of_node, "mode", &str);
	if (ret) {
		str = "none";
		LCDERR("failed to get mode\n");
		return -1;
	}
	lcd_driver->lcd_mode = lcd_mode_str_to_mode(str);
	ret = of_property_read_u32(lcd_driver->dev->of_node,
		"fr_auto_policy", &val);
	if (ret) {
		if (lcd_debug_print_flag)
			LCDPR("failed to get fr_auto_policy\n");
		lcd_driver->fr_auto_policy = 0;
	} else {
		lcd_driver->fr_auto_policy = (unsigned char)val;
	}
	ret = of_property_read_u32(lcd_driver->dev->of_node, "key_valid", &val);
	if (ret) {
		if (lcd_debug_print_flag)
			LCDPR("failed to get key_valid\n");
		lcd_driver->lcd_key_valid = 0;
	} else {
		lcd_driver->lcd_key_valid = (unsigned char)val;
	}
	LCDPR("detect mode: %s, fr_auto_policy: %d, key_valid: %d\n",
		str, lcd_driver->fr_auto_policy, lcd_driver->lcd_key_valid);

	lcd_driver->lcd_info = &lcd_vinfo;
	lcd_driver->lcd_config = &lcd_config_dft;
	lcd_driver->lcd_test_flag = 0;
	lcd_driver->lcd_resume_type = 1; /* default workqueue */
	lcd_driver->power_ctrl = lcd_power_ctrl;
	lcd_driver->module_reset = lcd_module_reset;
	lcd_driver->power_tiny_ctrl = lcd_power_tiny_ctrl;
	lcd_driver->module_tiny_reset = lcd_module_tiny_reset;
	lcd_driver->res_vsync_irq = platform_get_resource(pdev,
		IORESOURCE_IRQ, 0);
	lcd_driver->res_vx1_irq = platform_get_resource(pdev,
		IORESOURCE_IRQ, 1);
	lcd_config_default();
	lcd_init_vout();

	if (lcd_driver->lcd_key_valid) {
		if (lcd_driver->workqueue) {
			queue_delayed_work(lcd_driver->workqueue,
				&lcd_driver->lcd_probe_delayed_work,
				msecs_to_jiffies(2000));
		} else {
			LCDPR("Warning: no lcd_probe_delayed workqueue\n");
			ret = lcd_mode_probe(lcd_driver->dev);
			if (ret) {
				kfree(lcd_driver);
				lcd_driver = NULL;
				LCDERR("probe exit\n");
			}
		}
	} else {
		ret = lcd_mode_probe(lcd_driver->dev);
		if (ret) {
			kfree(lcd_driver);
			lcd_driver = NULL;
			LCDERR("probe exit\n");
		}
	}

	return 0;
}

#ifdef CONFIG_OF

static struct lcd_data_s lcd_data_gxtvbb = {
	.chip_type = LCD_CHIP_GXTVBB,
	.chip_name = "gxtvbb",
	.reg_map_table = &lcd_reg_gxb[0],
};

static struct lcd_data_s lcd_data_gxl = {
	.chip_type = LCD_CHIP_GXL,
	.chip_name = "gxl",
	.reg_map_table = &lcd_reg_gxb[0],
};

static struct lcd_data_s lcd_data_gxm = {
	.chip_type = LCD_CHIP_GXM,
	.chip_name = "gxm",
	.reg_map_table = &lcd_reg_gxb[0],
};

static struct lcd_data_s lcd_data_txlx = {
	.chip_type = LCD_CHIP_TXLX,
	.chip_name = "txlx",
	.reg_map_table = &lcd_reg_gxb[0],
};

static struct lcd_data_s lcd_data_axg = {
	.chip_type = LCD_CHIP_AXG,
	.chip_name = "axg",
	.reg_map_table = &lcd_reg_axg[0],
};

static const struct of_device_id lcd_dt_match_table[] = {
	{
		.compatible = "amlogic, lcd-gxtvbb",
		.data = &lcd_data_gxtvbb,
	},
	{
		.compatible = "amlogic, lcd-gxl",
		.data = &lcd_data_gxl,
	},
	{
		.compatible = "amlogic, lcd-gxm",
		.data = &lcd_data_gxm,
	},
	{
		.compatible = "amlogic, lcd-txlx",
		.data = &lcd_data_txlx,
	},
	{
		.compatible = "amlogic, lcd-axg",
		.data = &lcd_data_axg,
	},
	{},
};
#endif

static int lcd_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	int ret = 0;

#ifdef LCD_DEBUG_INFO
	lcd_debug_print_flag = 1;
#else
	lcd_debug_print_flag = 0;
#endif
	lcd_driver = kmalloc(sizeof(struct aml_lcd_drv_s), GFP_KERNEL);
	if (!lcd_driver) {
		LCDERR("%s: lcd driver no enough memory\n", __func__);
		return -ENOMEM;
	}
	lcd_driver->dev = &pdev->dev;

	match = of_match_device(lcd_dt_match_table, &pdev->dev);
	if (match == NULL) {
		LCDERR("%s: no match table\n", __func__);
		return -1;
	}
	lcd_driver->data = (struct lcd_data_s *)match->data;
	LCDPR("driver version: %s(%d-%s)\n",
		lcd_driver->version,
		lcd_driver->data->chip_type,
		lcd_driver->data->chip_name);

	mutex_init(&lcd_vout_mutex);
	mutex_init(&lcd_power_mutex);
	lcd_vout_serve_bypass = 0;

	/* init workqueue */
	INIT_DELAYED_WORK(&lcd_driver->lcd_probe_delayed_work,
		lcd_config_probe_delayed);
	lcd_driver->workqueue = create_singlethread_workqueue("lcd_work_queue");
	if (lcd_driver->workqueue == NULL)
		LCDERR("can't create lcd workqueue\n");

	INIT_WORK(&(lcd_driver->lcd_resume_work), lcd_resume_work);

	lcd_ioremap(pdev);
	lcd_clk_config_probe();
	ret = lcd_config_probe(pdev);

	LCDPR("%s %s\n", __func__, (ret ? "failed" : "ok"));
	return 0;
}

static int lcd_remove(struct platform_device *pdev)
{
	int ret;

	ret = cancel_delayed_work(&lcd_driver->lcd_probe_delayed_work);
	if (lcd_driver->workqueue)
		destroy_workqueue(lcd_driver->workqueue);

	if (lcd_driver) {
		aml_lcd_notifier_unregister(&lcd_power_nb);
		aml_lcd_notifier_unregister(&lcd_bl_select_nb);
		aml_lcd_notifier_unregister(&lcd_interface_nb);

		lcd_fops_remove();
		lcd_class_remove();
		lcd_clk_config_remove();

		lcd_mode_remove(lcd_driver->dev);
		kfree(lcd_driver);
		lcd_driver = NULL;
	}

	LCDPR("%s\n", __func__);
	return 0;
}

static int lcd_resume(struct platform_device *pdev)
{
	if (lcd_driver->lcd_resume_type) {
		lcd_resume_flag = 1;
		if (lcd_driver->workqueue) {
			queue_work(lcd_driver->workqueue,
				&(lcd_driver->lcd_resume_work));
		} else {
			LCDPR("Warning: no lcd workqueue\n");
			mutex_lock(&lcd_power_mutex);
			aml_lcd_notifier_call_chain(LCD_EVENT_POWER_ON, NULL);
			LCDPR("%s finished\n", __func__);
			mutex_unlock(&lcd_power_mutex);
		}
	} else {
		LCDPR("directly lcd resume\n");
		mutex_lock(&lcd_power_mutex);
		lcd_resume_flag = 1;
		aml_lcd_notifier_call_chain(LCD_EVENT_POWER_ON, NULL);
		LCDPR("%s finished\n", __func__);
		mutex_unlock(&lcd_power_mutex);
	}

	return 0;
}

static int lcd_suspend(struct platform_device *pdev, pm_message_t state)
{
	mutex_lock(&lcd_power_mutex);
	if (lcd_driver->lcd_status) {
		aml_lcd_notifier_call_chain(LCD_EVENT_POWER_OFF, NULL);
		lcd_resume_flag = 0;
		LCDPR("%s finished\n", __func__);
	}
	mutex_unlock(&lcd_power_mutex);
	return 0;
}

static void lcd_shutdown(struct platform_device *pdev)
{
	if (lcd_debug_print_flag)
		LCDPR("%s\n", __func__);

	if (lcd_driver->lcd_status)
		aml_lcd_notifier_call_chain(LCD_EVENT_POWER_OFF, NULL);
}

static struct platform_driver lcd_platform_driver = {
	.probe = lcd_probe,
	.remove = lcd_remove,
	.suspend = lcd_suspend,
	.resume = lcd_resume,
	.shutdown = lcd_shutdown,
	.driver = {
		.name = "mesonlcd",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(lcd_dt_match_table),
#endif
	},
};

static int __init lcd_init(void)
{
	if (platform_driver_register(&lcd_platform_driver)) {
		LCDERR("failed to register lcd driver module\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit lcd_exit(void)
{
	platform_driver_unregister(&lcd_platform_driver);
}

subsys_initcall(lcd_init);
module_exit(lcd_exit);

static int __init lcd_panel_type_para_setup(char *str)
{
	if (str != NULL)
		sprintf(lcd_propname, "%s", str);

	LCDPR("panel_type: %s\n", lcd_propname);
	return 0;
}
__setup("panel_type=", lcd_panel_type_para_setup);

MODULE_DESCRIPTION("Meson LCD Panel Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Amlogic, Inc.");
