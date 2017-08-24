/*
 * drivers/amlogic/media/vin/tvin/hdmirx_ext/platform_iface.c
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

#include <linux/hrtimer.h>
#include <linux/timer.h>
#include <linux/kthread.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include "hdmirx_ext_drv.h"
#include "platform_iface.h"


/* ******************************************************************** */
/* for platform timer function */

static void __plat_timer_handler(unsigned long arg)
{
	struct hdmirx_ext_drv_s *hdrv = hdmirx_ext_get_driver();

	if (hdrv->timer_func)
		hdrv->timer_func();
	hdrv->timer.expires = jiffies + msecs_to_jiffies(hdrv->timer_cnt);
	add_timer(&hdrv->timer);
}

void __plat_timer_set(int timer_cnt)
{
	struct hdmirx_ext_drv_s *hdrv = hdmirx_ext_get_driver();

	hdrv->timer_cnt = timer_cnt;
	hdrv->timer.expires = jiffies + msecs_to_jiffies(timer_cnt);
	add_timer(&hdrv->timer);
	RXEXTPR("timer set\n");
}

void __plat_timer_init(void (*timer_func)(void))
{
	struct hdmirx_ext_drv_s *hdrv = hdmirx_ext_get_driver();

	RXEXTPR("timer init\n");

	hdrv->timer_func = timer_func;
	init_timer(&hdrv->timer);
	hdrv->timer.data = (unsigned long)hdrv;
	hdrv->timer.function = __plat_timer_handler;
}

int __plat_timer_expired(void)
{
	struct hdmirx_ext_drv_s *hdrv = hdmirx_ext_get_driver();

	del_timer_sync(&hdrv->timer);
	RXEXTPR("timer remove\n");
	return 0;
}

#if 0
/* for platform hrtimer function */
/* HRTIMER_NORESTART,  // Timer is not restarted  */
/* HRTIMER_RESTART,    // Timer must be restarted */
static enum hrtimer_restart __plat_hrtimer_handler(struct hrtimer *hr_timer)
{
	struct hdmirx_ext_drv_s *hdrv = hdmirx_ext_get_driver();

	if (hdrv->hrtimer_func)
		hdrv->hrtimer_func();

	if (hdrv->state)
		return HRTIMER_RESTART;
	else
		return HRTIMER_NORESTART;
}

void __plat_hrtimer_set(long timer_cnt)
{
	struct hdmirx_ext_drv_s *hdrv = hdmirx_ext_get_driver();

	hdrv->timer_cnt = timer_cnt;
	hrtimer_start(&hdrv->hr_timer,
		ktime_set(timer_cnt / 1000, (timer_cnt % 1000) * 1000000),
		HRTIMER_MODE_REL);
	RXEXTPR("hrtimer set\n");
}

void __plat_hrtimer_init(void *timer_func)
{
	struct hdmirx_ext_drv_s *hdrv = hdmirx_ext_get_driver();

	RXEXTPR("hrtimer init\n");

	hdrv->hrtimer_func = timer_func;
	hrtimer_init(&hdrv->hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hdrv->hr_timer.function = __plat_hrtimer_handler;
}

int __plat_hrtimer_expired(void)
{
	struct hdmirx_ext_drv_s *hdrv = hdmirx_ext_get_driver();

	RXEXTPR("hrtimer remove\n");
	return hrtimer_cancel(&hdrv->hr_timer);
}
#endif
/* ******************************************************************** */

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* for platform i2c interface */

static int __plat_i2c_read(unsigned char dev_addr, char *buf,
		int addr_len, int data_len)
{
	struct hdmirx_ext_drv_s *hdrv = hdmirx_ext_get_driver();
	int ret;
	char reg_addr = buf[0];
	struct i2c_msg msg[] = {
		{
			.addr  = dev_addr,
			.flags = 0,
			.len   = addr_len,
			.buf   = buf,
		},
		{
			.addr  = dev_addr,
			.flags = I2C_M_RD,/* | (2<<1), */
			.len   = data_len,
			.buf   = buf,
		},
	};

	if (hdrv == NULL) {
		RXEXTERR("%s: hdmirx_ext driver is null !\n", __func__);
		return -1;
	}
	if (hdrv->hw.i2c_client == NULL) {
		RXEXTERR("%s: invalid i2c_client !\n", __func__);
		return -1;
	}

	ret = i2c_transfer(hdrv->hw.i2c_client->adapter, msg, 2);
	if (ret < 0) {
		RXEXTERR("%s: %d, dev_addr = 0x%x, reg = 0x%x\n",
			__func__, ret, dev_addr, reg_addr);
	}

	return ret;
}

static int __plat_i2c_write(unsigned char dev_addr, char *buf, int len)
{
	struct hdmirx_ext_drv_s *hdrv = hdmirx_ext_get_driver();
	int ret = -1, i;
	char reg_addr = buf[0];
	struct i2c_msg msg[] = {
		{
			.addr	= dev_addr,
			.flags	= 0,/* | (2<<1),*/
			.len	= len,
			.buf	= buf,
		},
	};

	if (hdrv == NULL) {
		RXEXTERR("%s: hdmirx_ext driver is null !\n", __func__);
		return -1;
	}
	if (hdrv->hw.i2c_client == NULL) {
		RXEXTERR("%s: invalid i2c_client !\n", __func__);
		return -1;
	}

	ret = i2c_transfer(hdrv->hw.i2c_client->adapter, msg, 1);
	if (ret < 0) {
		RXEXTERR("%s: %d, dev_addr = 0x%x, reg = 0x%x\n",
			__func__, ret, dev_addr, reg_addr);
		for (i = 1; i < len; i++)
			pr_info("value[%d] = 0x%x\n", i, buf[i]);
	}

	return ret;
}

int __plat_i2c_read_byte(unsigned char dev_addr, unsigned char offset,
		unsigned char *pvalue)
{
	int ret = 0;
	char buf[2] = {0, 0};

	buf[0] = offset;

	ret = __plat_i2c_read(dev_addr, buf, 1, 1);
	*pvalue = buf[0];

	if (ret < 0) {
		RXEXTERR("%s: dev_addr = 0x%x, reg = 0x%x, ret = %d\n",
			__func__, dev_addr, offset, ret);
	}

	return ret;
}

int __plat_i2c_write_byte(unsigned char dev_addr, unsigned char offset,
		unsigned char value)
{
	int ret = 0;
	char buf[2] = {0, 0};

	buf[0] = offset;
	buf[1] = value;

	ret = __plat_i2c_write(dev_addr, buf, 2);
	if (ret < 0) {
		RXEXTERR("%s: dev_addr = 0x%x, reg = 0x%x, ret = %d\n",
			__func__, dev_addr, offset, ret);
	}

	return ret;
}

int __plat_i2c_read_block(unsigned char dev_addr, unsigned char offset,
		char *buffer, int length)
{
	int ret = 0;

	buffer[0] = offset;

	ret = __plat_i2c_read(dev_addr, buffer, 1, length);
	if (ret < 0) {
		RXEXTERR("%s: dev_addr = 0x%x, reg = 0x%x, ret = %d\n",
			__func__, dev_addr, offset, ret);
	}

	return ret;
}

int __plat_i2c_write_block(unsigned char dev_addr, unsigned char offset,
		char *buffer, int length)
{
	int ret = 0;
	char *tmp = NULL;

	tmp = kmalloc((sizeof(char) * (length+1)), GFP_KERNEL);

	if (tmp == NULL)
		return -1;

	tmp[0] = offset;
	memcpy(&tmp[1], buffer, length);

	ret = __plat_i2c_write(dev_addr, tmp, length+1);
	if (ret < 0) {
		RXEXTERR("%s: dev_addr = 0x%x, reg = 0x%x, ret = %d\n",
			__func__, dev_addr, offset, ret);
	}

	kfree(tmp);
	return ret;
}

int __plat_i2c_init(struct hdmirx_ext_hw_s *phw, unsigned char dev_i2c_addr)
{
	struct i2c_board_info board_info;
	struct i2c_adapter *adapter;

	RXEXTDBG("%s: i2c addr = 0x%x\n", __func__, dev_i2c_addr);

	memset(&board_info, 0x0, sizeof(board_info));
	strncpy(board_info.type, HDMIRX_EXT_DEV_NAME, I2C_NAME_SIZE);
	adapter = i2c_get_adapter(phw->i2c_bus_index);
	board_info.addr = dev_i2c_addr;
	board_info.platform_data = phw;
	phw->i2c_client = i2c_new_device(adapter, &board_info);

	return 0;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* for platform gpio interface */

int __plat_gpio_init(unsigned int gpio_index)
{
	return 0;
}

void __plat_gpio_output(struct hdmirx_ext_gpio_s *gpio, unsigned int value)
{
	if (gpio->flag == 0) {
		RXEXTERR("%s: gpio is not regisetered\n", __func__);
		return;
	}
	if (IS_ERR(gpio->gpio)) {
		RXEXTERR("gpio %s: %p, err: %ld\n",
			gpio->name, gpio->gpio, PTR_ERR(gpio->gpio));
		return;
	}
	gpiod_direction_output(gpio->gpio, ((value == 0) ? 0 : 1));
	RXEXTDBG("gpio %s: %p, value: %d\n",
		gpio->name, gpio->gpio, value);
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* for platform delay interface */

void __plat_msleep(unsigned int msecs)
{
	msleep(msecs);
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* for platform video mode interface */

static char *__plat_vmode[] = {
	"480i", "480p",
	"576i", "576p",
	"720p50hz", "720p60hz",
	"1080i60hz", "1080i50hz",
	"1080p60hz", "1080p50hz",
	"invalid"
};

int __plat_get_video_mode_by_name(char *name)
{
	unsigned int mode = CEA_MAX;
	int i;

	for (i = 0; i < 10; i++) {
		if (!strcmp(name, __plat_vmode[i]))
			mode = i;
	}
	switch (mode) {
	case 0: /* 480i */
		mode = CEA_480I60;
		break;
	case 1: /* 480p */
		mode = CEA_480P60;
		break;
	case 2: /* 576i */
		mode = CEA_576I50;
		break;
	case 3: /* 576p */
		mode = CEA_576P50;
		break;
	case 4: /* 720p50 */
		mode = CEA_720P50;
		break;
	case 5: /* 720p60 */
	default:
		mode = CEA_720P60;
		break;
	case 6: /* 1080i60 */
		mode = CEA_1080I60;
		break;
	case 7: /* 1080i50 */
		mode = CEA_1080I50;
		break;
	case 8: /* 1080p60 */
		mode = CEA_1080P60;
		break;
	case 9: /* 1080p50 */
		mode = CEA_1080P50;
		break;
	}

	return mode;
}

char *__plat_get_video_mode_name(int mode)
{
	int temp = 10;

	switch (mode) {
	case CEA_480I60:
		temp = 0;
		break;
	case CEA_480P60:
		temp = 1;
		break;
	case CEA_576I50:
		temp = 2;
		break;
	case CEA_576P50:
		temp = 3;
		break;
	case CEA_720P60:
		temp = 4;
		break;
	case CEA_720P50:
		temp = 5;
		break;
	case CEA_1080I60:
		temp = 6;
		break;
	case CEA_1080I50:
		temp = 7;
		break;
	case CEA_1080P60:
		temp = 8;
		break;
	case CEA_1080P50:
		temp = 9;
		break;
	default:
		temp = 10;
		break;
	}

	return __plat_vmode[temp];
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* for platform task interface */

struct task_struct *__plat_create_thread(int (*threadfn)(void *data),
		void *data, const char namefmt[])
{
	return kthread_run(threadfn, data, namefmt);
}


