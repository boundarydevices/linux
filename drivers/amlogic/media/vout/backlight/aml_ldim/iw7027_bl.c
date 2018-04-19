/*
 * drivers/amlogic/media/vout/backlight/aml_ldim/iw7027_bl.c
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
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/irq.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/amlogic/media/vout/lcd/aml_ldim.h>
#include <linux/amlogic/media/vout/lcd/aml_bl.h>
#include "iw7027_bl.h"
#include "ldim_drv.h"
#include "ldim_dev_drv.h"

#define INT_VIU_VSYNC   35

#define NORMAL_MSG      (0<<7)
#define BROADCAST_MSG   (1<<7)
#define BLOCK_DATA      (0<<6)
#define SINGLE_DATA     (1<<6)
#define IW7027_DEV_ADDR 1

#define IW7027_REG_BRIGHTNESS_CHK  0x00
#define IW7027_REG_BRIGHTNESS      0x01
#define IW7027_REG_CHIPID          0x7f
#define IW7027_CHIPID              0x28

#define IW7027_POWER_ON       0
#define IW7027_POWER_RESET    1
static int iw7027_on_flag;
static int iw7027_spi_op_flag;

static DEFINE_MUTEX(iw7027_spi_mutex);

struct iw7027 {
	int test_mode;
	struct spi_device *spi;
	int cs_hold_delay;
	int cs_clk_delay;
	unsigned char cmd_size;
	unsigned char *init_data;
	struct class cls;
};
struct iw7027 *bl_iw7027;

static int test_brightness[] = {0xfff, 0xfff, 0xfff, 0xfff,
				0xfff, 0xfff, 0xfff, 0xfff, 0xfff, 0xfff
};

static int iw7027_wreg(struct spi_device *spi, u8 addr, u8 val)
{
	u8 tbuf[3];
	int ret;

	mutex_lock(&iw7027_spi_mutex);

	if (bl_iw7027->cs_hold_delay)
		udelay(bl_iw7027->cs_hold_delay);
	dirspi_start(spi);
	if (bl_iw7027->cs_clk_delay)
		udelay(bl_iw7027->cs_clk_delay);
	tbuf[0] = NORMAL_MSG | SINGLE_DATA | IW7027_DEV_ADDR;
	tbuf[1] = addr & 0x7f;
	tbuf[2] = val;
	ret = dirspi_write(spi, tbuf, 3);
	if (bl_iw7027->cs_clk_delay)
		udelay(bl_iw7027->cs_clk_delay);
	dirspi_stop(spi);
	mutex_unlock(&iw7027_spi_mutex);

	return ret;
}

static int iw7027_rreg(struct spi_device *spi, u8 addr, u8 *val)
{
	u8 tbuf[3];
	int ret;

	if (addr >= 0x80)
		iw7027_wreg(bl_iw7027->spi, 0x78, 0x80);
	else
		iw7027_wreg(bl_iw7027->spi, 0x78, 0x0);

	mutex_lock(&iw7027_spi_mutex);

	if (bl_iw7027->cs_hold_delay)
		udelay(bl_iw7027->cs_hold_delay);
	dirspi_start(spi);
	if (bl_iw7027->cs_clk_delay)
		udelay(bl_iw7027->cs_clk_delay);
	tbuf[0] = NORMAL_MSG | SINGLE_DATA | IW7027_DEV_ADDR;
	tbuf[1] = addr | 0x80;
	tbuf[2] = 0;
	ret = dirspi_write(spi, tbuf, 3);
	ret = dirspi_read(spi, val, 1);
	if (bl_iw7027->cs_clk_delay)
		udelay(bl_iw7027->cs_clk_delay);
	dirspi_stop(spi);

	mutex_unlock(&iw7027_spi_mutex);

	return ret;
}

static int iw7027_wregs(struct spi_device *spi, u8 addr, u8 *val, int len)
{
	u8 tbuf[30];
	int ret;

	mutex_lock(&iw7027_spi_mutex);

	if (bl_iw7027->cs_hold_delay)
		udelay(bl_iw7027->cs_hold_delay);
	dirspi_start(spi);
	if (bl_iw7027->cs_clk_delay)
		udelay(bl_iw7027->cs_clk_delay);
	tbuf[0] = NORMAL_MSG | BLOCK_DATA | IW7027_DEV_ADDR;
	tbuf[1] = len;
	tbuf[2] = addr & 0x7f;
	memcpy(&tbuf[3], val, len);
	ret = dirspi_write(spi, tbuf, len+3);
	if (bl_iw7027->cs_clk_delay)
		udelay(bl_iw7027->cs_clk_delay);
	dirspi_stop(spi);

	mutex_unlock(&iw7027_spi_mutex);

	return ret;
}

static int iw7027_power_on_init(int flag)
{
	unsigned char addr, val;
	int i, ret = 0;

	LDIMPR("%s: spi_op_flag=%d\n", __func__, iw7027_spi_op_flag);

	if (flag == IW7027_POWER_RESET)
		goto iw7027_power_reset_p;
	i = 1000;
	while ((iw7027_spi_op_flag) && (i > 0)) {
		i--;
		udelay(10);
	}
	if (iw7027_spi_op_flag == 1) {
		LDIMERR("%s: wait spi idle state failed\n", __func__);
		return 0;
	}

	iw7027_spi_op_flag = 1;
iw7027_power_reset_p:

	for (i = 0; i < LDIM_SPI_INIT_ON_SIZE; i += bl_iw7027->cmd_size) {
		if (bl_iw7027->init_data[i] == 0xff) {
			if (bl_iw7027->init_data[i+3] > 0)
				mdelay(bl_iw7027->init_data[i+3]);
			break;
		} else if (bl_iw7027->init_data[i] == 0x0) {
			addr = bl_iw7027->init_data[i+1];
			val = bl_iw7027->init_data[i+2];
			ret = iw7027_wreg(bl_iw7027->spi, addr, val);
			udelay(1);
		}
		if (bl_iw7027->init_data[i+3] > 0)
			mdelay(bl_iw7027->init_data[i+3]);
	}

	if (flag == IW7027_POWER_RESET)
		return ret;

	iw7027_spi_op_flag = 0;
	return ret;
}

static int iw7027_hw_init_on(void)
{
	int i, j;
	unsigned char temp, reg_chk, reg_duty_chk;

	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();

	ldim_gpio_set(ldim_drv->ldev_conf->en_gpio,
		ldim_drv->ldev_conf->en_gpio_on);
	mdelay(2);

	iw7027_wreg(bl_iw7027->spi, 0x00, 0x06);

	ldim_set_duty_pwm(&(ldim_drv->ldev_conf->pwm_config));
	ldim_drv->pinmux_ctrl(ldim_drv->ldev_conf->pinmux_name);
	mdelay(100);

	iw7027_rreg(bl_iw7027->spi, 0x00, &reg_chk);
	for (i = 1; i < 10; i++) {
		iw7027_rreg(bl_iw7027->spi, 0x00, &temp);
		if (temp != reg_chk)
			goto iw7027_smr_end;
		}

	iw7027_power_on_init(IW7027_POWER_ON);
	mdelay(500);
	iw7027_wreg(bl_iw7027->spi, 0x00, 0x07);
	iw7027_rreg(bl_iw7027->spi, 0xb3, &reg_duty_chk);
	if (reg_duty_chk == 0x05) {
		for (j = 0; j < 20; j++)
			iw7027_wreg(bl_iw7027->spi, (0x40 + j), 0x1f);
		}

iw7027_smr_end:
	iw7027_spi_op_flag = 0;

	return 0;
}

static int iw7027_hw_init_off(void)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	int i = 1000;

	while ((iw7027_spi_op_flag) && (i > 0)) {
		i--;
		udelay(10);
	}
	if (iw7027_spi_op_flag == 1)
		LDIMERR("%s: wait spi idle state failed\n", __func__);

	ldim_gpio_set(ldim_drv->ldev_conf->en_gpio,
		ldim_drv->ldev_conf->en_gpio_off);

	return 0;
}

static int iw7027_spi_dump_low(char *buf)
{
	int ret = 0;
	unsigned int i;
	unsigned char val;

	for (i = 0; i <= 0x7f; i++) {
		iw7027_rreg(bl_iw7027->spi, i, &val);
		if (buf) {
			int n = sprintf(buf+ret,
					"iw7027 reg 0x%02x=0x%02x\n", i, val);
			ret += n;
		}
		pr_info("iw7027 reg 0x%02x=0x%02x\n", i, val);
	}
	if (buf)
		ret += sprintf(buf, "\n");
	pr_info("\n");

	return ret;
}

static int iw7027_spi_dump_high(char *buf)
{
	int ret = 0;
	unsigned int i;
	unsigned char val;

	for (i = 0x80; i <= 0xff; i++) {
		iw7027_rreg(bl_iw7027->spi, i, &val);
		if (buf) {
			int n = sprintf(buf+ret,
					"iw7027 reg 0x%02x=0x%02x\n", i, val);
			ret += n;
		}
		pr_info("iw7027 reg 0x%02x=0x%02x\n", i, val);
	}
	if (buf)
		ret += sprintf(buf, "\n");
	pr_info("\n");

	return ret;
}

static unsigned int iw7027_get_value(unsigned int level)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	unsigned int val;
	unsigned int dim_max, dim_min;

	dim_max = ldim_drv->ldev_conf->dim_max;
	dim_min = ldim_drv->ldev_conf->dim_min;

	val = dim_min + ((level * (dim_max - dim_min)) / LD_DATA_MAX);

	return val;
}

static int iw7027_smr(unsigned short *buf, unsigned char len)
{
	int i, j;
	unsigned int value_flag = 0;
	unsigned char val_0[20];

	if (iw7027_on_flag == 0) {
		if (ldim_debug_print)
			LDIMPR("%s: on_flag=%d\n", __func__, iw7027_on_flag);
		return 0;
	}
	if (len != 10) {
		LDIMERR("%s: data len %d invalid\n", __func__, len);
		return -1;
	}
	if (iw7027_spi_op_flag) {
		if (ldim_debug_print) {
			LDIMPR("%s: spi_op_flag=%d\n",
				__func__, iw7027_spi_op_flag);
		}
		return 0;
	}

	iw7027_spi_op_flag = 1;

	for (i = 0; i < 10; i++)
		value_flag = value_flag || buf[i];

	if (value_flag) {
		if (bl_iw7027->test_mode) {
			val_0[0] = (test_brightness[0] & 0xf00) >> 8;
			val_0[1] = test_brightness[0] & 0xff;
			val_0[2] = (test_brightness[1] & 0xf00) >> 8;
			val_0[3] = test_brightness[1] & 0xff;
			val_0[4] = (test_brightness[2] & 0xf00) >> 8;
			val_0[5] = test_brightness[2] & 0xff;
			val_0[6] = (test_brightness[3] & 0xf00) >> 8;
			val_0[7] = test_brightness[3] & 0xff;
			val_0[8] = (test_brightness[4] & 0xf00) >> 8;
			val_0[9] = test_brightness[4] & 0xff;
			val_0[10] = (test_brightness[5] & 0xf00) >> 8;
			val_0[11] = test_brightness[5] & 0xff;
			val_0[12] = (test_brightness[6] & 0xf00) >> 8;
			val_0[13] = test_brightness[6] & 0xff;
			val_0[14] = (test_brightness[7] & 0xf00) >> 8;
			val_0[15] = test_brightness[7] & 0xff;
			val_0[16] = (test_brightness[8] & 0xf00) >> 8;
			val_0[17] = test_brightness[8] & 0xff;
			val_0[18] = (test_brightness[9] & 0xf00) >> 8;
			val_0[19] = test_brightness[9] & 0xff;

			iw7027_wregs(bl_iw7027->spi, 0x40, val_0, 20);
		} else {
			val_0[0] = ((iw7027_get_value(buf[0])
				) & 0xf00) >> 8;
			val_0[1] = (iw7027_get_value(buf[0])
				) & 0xff;
			val_0[2] = ((iw7027_get_value(buf[1])
				) & 0xf00) >> 8;
			val_0[3] = (iw7027_get_value(buf[1])
				) & 0xff;
			val_0[4] = ((iw7027_get_value(buf[2])
				) & 0xf00) >> 8;
			val_0[5] = (iw7027_get_value(buf[2])
				) & 0xff;
			val_0[6] = ((iw7027_get_value(buf[3])
				) & 0xf00) >> 8;
			val_0[7] = (iw7027_get_value(buf[3])
				) & 0xff;
			val_0[8] = ((iw7027_get_value(buf[4])
				) & 0xf00) >> 8;
			val_0[9] = (iw7027_get_value(buf[4])
				) & 0xff;
			val_0[10] = ((iw7027_get_value(buf[5])
				) & 0xf00) >> 8;
			val_0[11] = (iw7027_get_value(buf[5])
				) & 0xff;
			val_0[12] = ((iw7027_get_value(buf[6])
				) & 0xf00) >> 8;
			val_0[13] = (iw7027_get_value(buf[6])
				) & 0xff;
			val_0[14] = ((iw7027_get_value(buf[7])
				) & 0xf00) >> 8;
			val_0[15] = (iw7027_get_value(buf[7])
				) & 0xff;
			val_0[16] = ((iw7027_get_value(buf[8])
				) & 0xf00) >> 8;
			val_0[17] = (iw7027_get_value(buf[8])
				) & 0xff;
			val_0[18] = ((iw7027_get_value(buf[9])
				) & 0xf00) >> 8;
			val_0[19] = (iw7027_get_value(buf[9])
				) & 0xff;

			iw7027_wregs(bl_iw7027->spi, 0x40, val_0, 20);
		}
	} else {
		for (j = 0; j < 20; j++)
			val_0[j] = 0;
		iw7027_wregs(bl_iw7027->spi, 0x40, val_0, 20);
	}

	iw7027_spi_op_flag = 0;
	return 0;
}

static int iw7027_power_on(void)
{
	if (iw7027_on_flag) {
		LDIMPR("%s: iw7027 is already on, exit\n", __func__);
		return 0;
	}
	iw7027_hw_init_on();
	iw7027_on_flag = 1;

	LDIMPR("%s: ok\n", __func__);
	return 0;
}

static int iw7027_power_off(void)
{
	iw7027_on_flag = 0;
	iw7027_hw_init_off();

	LDIMPR("%s: ok\n", __func__);
	return 0;
}

static ssize_t iw7027_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct iw7027 *bl = container_of(class, struct iw7027, cls);
	int ret = 0;
	int i;

	if (!strcmp(attr->attr.name, "mode")) {
		ret = sprintf(buf, "0x%02x\n", bl->spi->mode);
	} else if (!strcmp(attr->attr.name, "speed")) {
		ret = sprintf(buf, "%d\n", bl->spi->max_speed_hz);
	} else if (!strcmp(attr->attr.name, "test")) {
		ret = sprintf(buf, "test mode=%d\n", bl->test_mode);
	} else if (!strcmp(attr->attr.name, "brightness")) {
		ret = sprintf(buf, "0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x\n",
				test_brightness[0],
				test_brightness[1],
				test_brightness[2],
				test_brightness[3],
				test_brightness[4],
				test_brightness[5],
				test_brightness[6],
				test_brightness[7]);
	} else if (!strcmp(attr->attr.name, "status")) {
		ret = sprintf(buf, "iw7027 status:\n"
				"dev_index      = %d\n"
				"on_flag        = %d\n"
				"en_on          = %d\n"
				"en_off         = %d\n"
				"cs_hold_delay  = %d\n"
				"cs_clk_delay   = %d\n"
				"spi_op_flag    = %d\n"
				"dim_max        = 0x%03x\n"
				"dim_min        = 0x%03x\n",
				ldim_drv->dev_index, iw7027_on_flag,
				ldim_drv->ldev_conf->en_gpio_on,
				ldim_drv->ldev_conf->en_gpio_off,
				ldim_drv->ldev_conf->cs_hold_delay,
				ldim_drv->ldev_conf->cs_clk_delay,
				iw7027_spi_op_flag,
				ldim_drv->ldev_conf->dim_max,
				ldim_drv->ldev_conf->dim_min);
	} else if (!strcmp(attr->attr.name, "dump_low")) {
		i = 1000;
		while ((iw7027_spi_op_flag) && (i > 0)) {
			i--;
			udelay(10);
		}
		if (iw7027_spi_op_flag == 0) {
			iw7027_spi_op_flag = 1;
			ret = iw7027_spi_dump_low(buf);
			iw7027_spi_op_flag = 0;
		} else {
			LDIMERR("%s: wait spi idle state failed\n", __func__);
		}
		ret += sprintf(buf, "\n");
	} else if (!strcmp(attr->attr.name, "dump_high")) {
		i = 1000;
		while ((iw7027_spi_op_flag) && (i > 0)) {
			i--;
			udelay(10);
		}
		if (iw7027_spi_op_flag == 0) {
			iw7027_spi_op_flag = 1;
			ret = iw7027_spi_dump_high(buf);
			iw7027_spi_op_flag = 0;
		} else {
			LDIMERR("%s: wait spi idle state failed\n", __func__);
		}
		ret += sprintf(buf, "\n");
	}

	return ret;
}

#define MAX_ARG_NUM 3
static ssize_t iw7027_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count)
{
	struct iw7027 *bl = container_of(class, struct iw7027, cls);
	unsigned int val, val2;
	unsigned char reg_addr, reg_val;
	int i;

	if (!strcmp(attr->attr.name, "init")) {
		iw7027_hw_init_on();
	} else if (!strcmp(attr->attr.name, "mode")) {
		i = kstrtouint(buf, 10, &val);
		if (i == 1)
			bl->spi->mode = val;
		else
			LDIMERR("%s: invalid args\n", __func__);
	} else if (!strcmp(attr->attr.name, "speed")) {
		i = kstrtouint(buf, 10, &val);
		if (i == 1)
			bl->spi->max_speed_hz = val*1000;
		else
			LDIMERR("%s: invalid args\n", __func__);
	} else if (!strcmp(attr->attr.name, "reg")) {
		if (buf[0] == 'w') {
			i = sscanf(buf, "w %x %x", &val, &val2);
			if (i == 2) {
				reg_addr = (unsigned char)val;
				reg_val = (unsigned char)val2;
				iw7027_wreg(bl->spi, reg_addr, reg_val);
			} else {
				LDIMERR("%s: invalid args\n", __func__);
			}
		} else if (buf[0] == 'r') {
			i = sscanf(buf, "r %x", &val);
			if (i == 1) {
				reg_addr = (unsigned char)val;
				iw7027_rreg(bl->spi, reg_addr, &reg_val);
				pr_info("reg 0x%02x = 0x%02x\n",
					reg_addr, reg_val);
			} else {
				LDIMERR("%s: invalid args\n", __func__);
			}
		}
	} else if (!strcmp(attr->attr.name, "test")) {
		i = kstrtouint(buf, 10, &val);
		LDIMPR("set test mode to %d\n", val);
		bl->test_mode = val;
	} else if (!strcmp(attr->attr.name, "brightness")) {
		i = sscanf(buf, "%d %d", &val, &val2);
		val &= 0xfff;
		LDIMPR("brightness=%d, index=%d\n", val, val2);
		if ((i == 2) && (val2 < ARRAY_SIZE(test_brightness)))
			test_brightness[val2] = val;
	} else
		LDIMERR("LDIM argment error!\n");
	return count;
}

static struct class_attribute iw7027_class_attrs[] = {
	__ATTR(init, 0644, iw7027_show, iw7027_store),
	__ATTR(mode, 0644, iw7027_show, iw7027_store),
	__ATTR(speed, 0644, iw7027_show, iw7027_store),
	__ATTR(reg, 0644, iw7027_show, iw7027_store),
	__ATTR(test, 0644, iw7027_show, iw7027_store),
	__ATTR(brightness, 0644, iw7027_show, iw7027_store),
	__ATTR(status, 0644, iw7027_show, NULL),
	__ATTR(dump_low, 0644, iw7027_show, NULL),
	__ATTR(dump_high, 0644, iw7027_show, NULL),
	__ATTR_NULL
};

static int iw7027_ldim_driver_update(void)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();

	ldim_drv->device_power_on = iw7027_power_on;
	ldim_drv->device_power_off = iw7027_power_off;
	ldim_drv->device_bri_update = iw7027_smr;
	return 0;
}

static int ldim_spi_dev_probe(struct spi_device *spi)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	int ret;

	ldim_drv->spi = spi;

	dev_set_drvdata(&spi->dev, ldim_drv->ldev_conf);
	spi->bits_per_word = 8;
	ret = spi_setup(spi);
	if (ret)
		LDIMERR("spi setup failed\n");

	LDIMPR("%s ok\n", __func__);
	return ret;
}

static int ldim_spi_dev_remove(struct spi_device *spi)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();

	ldim_drv->spi = NULL;
	return 0;
}

static struct spi_driver ldim_spi_dev_driver = {
	.probe = ldim_spi_dev_probe,
	.remove = ldim_spi_dev_remove,
	.driver = {
		.name = "ldim_dev",
		.owner = THIS_MODULE,
	},
};

int ldim_dev_iw7027_probe(void)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	int ret;

	iw7027_on_flag = 0;
	iw7027_spi_op_flag = 0;

	bl_iw7027 = kzalloc(sizeof(struct iw7027), GFP_KERNEL);
	if (bl_iw7027 == NULL) {
		LDIMERR("malloc bl_iw7027 failed\n");
		return -1;
	}

	spi_register_board_info(ldim_drv->spi_dev, 1);
	ret = spi_register_driver(&ldim_spi_dev_driver);
	if (ret) {
		LDIMERR("register ldim_dev spi driver failed\n");
		return -1;
	}

	bl_iw7027->test_mode = 0;
	bl_iw7027->spi = ldim_drv->spi;
	bl_iw7027->cs_hold_delay = ldim_drv->ldev_conf->cs_hold_delay;
	bl_iw7027->cs_clk_delay = ldim_drv->ldev_conf->cs_clk_delay;
	bl_iw7027->cmd_size = ldim_drv->ldev_conf->cmd_size;
	bl_iw7027->init_data = ldim_drv->ldev_conf->init_on;

	iw7027_ldim_driver_update();

	bl_iw7027->cls.name = kzalloc(10, GFP_KERNEL);
	sprintf((char *)bl_iw7027->cls.name, "iw7027");
	bl_iw7027->cls.class_attrs = iw7027_class_attrs;
	ret = class_register(&bl_iw7027->cls);
	if (ret < 0)
		pr_err("register iw7027 class failed\n");

	iw7027_on_flag = 1; /* default enable in uboot */

	LDIMPR("%s ok\n", __func__);
	return ret;
}

int ldim_dev_iw7027_remove(void)
{
	spi_unregister_driver(&ldim_spi_dev_driver);
	kfree(bl_iw7027);
	bl_iw7027 = NULL;

	return 0;
}

