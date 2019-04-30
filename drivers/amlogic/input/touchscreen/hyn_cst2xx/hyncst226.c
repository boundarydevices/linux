/*
 * drivers/input/touchscreen/hyncst226.c
 *
 * Copyright (c) 2017 ShenZheng Hynitron
 *	Ying Zhang<ying.zhang@hynitron.com.cn>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>

#if defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif
#include <linux/errno.h>
#include <linux/kernel.h>

#include <linux/platform_device.h>
#include <linux/async.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <asm/irq.h>
#include <linux/io.h>
#include <linux/gpio.h>

#include <linux/of_gpio.h>
#include <linux/input/mt.h>

#include"cst226firmware.h"

#define TP_RESET_PIN		3
#define TP_I2C_ADAPTER		(1)
#define TP_I2C_ADDR		0x1A
#define TP_NAME			"CST226"
#define TP_IRQ_PORT		100
#define TP_MAX_X		1024
#define TP_MAX_Y		600
static unsigned int gpio_reset;
static unsigned int gpio_irq;

static unsigned int gpio_irq_exist;
static unsigned int gpio_reset_exist;

#define CST226_I2C_NAME		"cst226"
#define CST226_I2C_ADDR		0x1A

#define CFG_TP_USE_CONFIG 1
#if CFG_TP_USE_CONFIG
struct tp_cfg_xml {
	unsigned int sirq;
	unsigned int i2cNum;
	unsigned int i2cAddr;
	unsigned int xMax;
	unsigned int yMax;
	unsigned int rotate;
	unsigned int xRevert;
	unsigned int yRevert;
};
static struct tp_cfg_xml cfg_xml;
#endif

#define RESUME_INIT_CHIP_WORK

#define REPORT_DATA_PROTOCOL_B
#define SLEEP_RESUME_CLEAR_POINT
#define ICS_SLOT_REPORT

#define HIGH_SPEED_IIC_TRANSFER

#define RECORD_POINT
#ifdef FILTER_POINT
#define FILTER_MAX		9
#endif

#define HYN_DATA_REG		0x80
#define HYN_STATUS_REG		0xe0
#define HYN_PAGE_REG		0xf0

#define PRESS_MAX		255
#define MAX_FINGERS		10
#define MAX_CONTACTS		10
#define DMA_TRANS_LEN		0x10

#define SCREEN_MAX_X		600
#define SCREEN_MAX_Y		1024

struct mutex mutex;

static int power_is_on;

#ifdef HYN_MONITOR
static struct delayed_work hyn_monitor_work;
static struct workqueue_struct *hyn_monitor_workqueue;
static char int_1st[4] = { 0 };
static char int_2nd[4] = { 0 };

static char bc_counter;
static char b0_counter;
static char i2c_lock_flag;
static int clk_tick_cnt = 300;
#endif
static struct i2c_client *hyn_client;

static struct hyn_ts *this_ts;

#ifdef HAVE_TOUCH_KEY
static u16 key;

static int key_state_flag;
struct key_data {
	u16 key;
	u16 x_min;
	u16 x_max;
	u16 y_min;
	u16 y_max;
};

const u16 key_array[] = {
	KEY_BACK,
	KEY_HOME,
	KEY_MENU,
	KEY_SEARCH,
};

struct key_data hyn_key_data[ARRAY_SIZE(key_array)] = {
	{KEY_BACK, 2048, 2048, 2048, 2048},
	{KEY_HOME, 2048, 2048, 2048, 2048},
	{KEY_MENU, 2048, 2048, 2048, 2048},
	{KEY_SEARCH, 2048, 2048, 2048, 2048},
};
#endif

struct hyn_ts_data {
	u8 x_index;
	u8 y_index;
	u8 z_index;
	u8 id_index;
	u8 touch_index;
	u8 data_reg;
	u8 status_reg;
	u8 data_size;
	u8 touch_bytes;
	u8 update_data;
	u8 touch_meta_data;
	u8 finger_size;
};

static struct hyn_ts_data devices[] = {
	{
	 .x_index = 6,
	 .y_index = 4,
	 .z_index = 5,
	 .id_index = 7,
	 .data_reg = HYN_DATA_REG,
	 .status_reg = HYN_STATUS_REG,
	 .update_data = 0x4,
	 .touch_bytes = 4,
	 .touch_meta_data = 4,
	 .finger_size = 70,
	 },
};

struct hyn_ts {
	struct i2c_client *client;
	struct input_dev *input;
	struct work_struct work;
	struct workqueue_struct *wq;
#ifdef RESUME_INIT_CHIP_WORK
	struct work_struct init_work;
	struct workqueue_struct *init_wq;
#endif
	struct hyn_ts_data *dd;
	u8 *touch_data;
	u8 device_id;
	int irq;
#if defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif
};

#define CTP_POWER_ID			("sensor28")
#define CTP_POWER_MIN_VOL	       (3300000)
#define CTP_POWER_MAX_VOL	       (3300000)

int current_val;

static int cst226_shutdown_low(void)
{
	gpio_direction_output(gpio_reset, 0);
	return 0;
}

static int cst226_shutdown_high(void)
{
	gpio_direction_output(gpio_reset, 1);
	return 0;
}

#ifdef HIGH_SPEED_IIC_TRANSFER

static int cst2xx_i2c_read_register(struct i2c_client *client,
				    unsigned char *buf, int len)
{
	struct i2c_msg msgs[2];
	int ret = -1;
	int retries = 0;

	msgs[0].flags = 0;
	msgs[0].addr = client->addr;
	msgs[0].len = 2;
	msgs[0].buf = buf;

	msgs[1].flags = I2C_M_RD;
	msgs[1].addr = client->addr;
	msgs[1].len = len;
	msgs[1].buf = buf;

	while (retries < 5) {
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret == 2)
			break;
		retries++;
	}

	return ret;
}

static int cst2xx_i2c_write(struct i2c_client *client, unsigned char *buf,
			    int len)
{
	struct i2c_msg msg;
	int ret = -1;
	int retries = 0;

	msg.flags = 0;
	msg.addr = client->addr;
	msg.len = len;
	msg.buf = buf;

	while (retries < 5) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret == 1)
			break;
		retries++;
	}

	return ret;
}
#else
static int cst2xx_i2c_read(struct i2c_client *client, unsigned char *buf,
			   int len)
{
	int ret = -1;
	int retries = 0;

	while (retries < 5) {
		ret = i2c_master_recv(client, buf, len);
		if (ret <= 0)
			retries++;
		else
			break;
	}

	return ret;
}

static int cst2xx_i2c_write(struct i2c_client *client, unsigned char *buf,
			    int len)
{
	int ret = -1;
	int retries = 0;

	while (retries < 5) {
		ret = i2c_master_send(client, buf, len);
		if (ret <= 0)
			retries++;
		else
			break;
	}

	return ret;
}

static int cst2xx_i2c_read_register(struct i2c_client *client,
				    unsigned char *buf, int len)
{
	int ret = -1;

	ret = cst2xx_i2c_write(client, buf, 2);

	ret = cst2xx_i2c_read(client, buf, len);

	return ret;
}
#endif
static int test_i2c(struct i2c_client *client)
{
	int retry = 0;
	int ret;
	u8 buf[4];

	buf[0] = 0xD1;
	buf[1] = 0x06;
	while (retry++ < 2) {
		ret = cst2xx_i2c_write(client, buf, 2);
		if (ret > 0)
			return ret;

		usleep_range(2000, 2010);
	}

	return ret;
}

void hard_reset_chip(int delay)
{
	cst226_shutdown_high();
	msleep(delay);
	cst226_shutdown_low();
	msleep(delay);
	cst226_shutdown_high();
}

#if CFG_TP_USE_CONFIG
static int tp_get_config(struct i2c_client *client)
{
	struct device_node *np;

	np = client->dev.of_node;
	gpio_irq = of_get_named_gpio(np, "irq-gpio", 0);
	gpio_reset = of_get_named_gpio(np, "reset-gpio", 0);

	if (!gpio_is_valid(gpio_irq) || !gpio_is_valid(gpio_reset))
		return -EINVAL;

	gpio_reset_exist = 1;
	gpio_irq_exist = 1;

	cfg_xml.xMax = TP_MAX_X;
	cfg_xml.yMax = TP_MAX_Y;
	cfg_xml.xRevert = 0;
	cfg_xml.yRevert = 0;
	cfg_xml.rotate = 1;

	if (gpio_irq_exist == 1) {
		if (gpio_request(gpio_irq, "ts_irq_gpio") != 0) {
			gpio_free(gpio_irq);
			return -EIO;
		}
		gpio_direction_input(gpio_irq);
	}
	if (gpio_reset_exist == 1) {
		if (gpio_request(gpio_reset, CST226_I2C_NAME) != 0) {
			gpio_free(gpio_reset);
			return -EIO;
		}
	}

	return 0;
}
#endif

#define  CST2XX_BASE_ADDR		(24 * 1024)
static int cst2xx_enter_download_mode(struct i2c_client *client)
{
	int ret;
	int i;
	u8 buf[2];

	hard_reset_chip(5);

	for (i = 0; i < 20; i++) {
		buf[0] = 0xD1;
		buf[1] = 0x11;
		ret = cst2xx_i2c_write(client, buf, 2);
		if (ret < 0) {
			usleep_range(1000, 1010);
			continue;
		}

		usleep_range(4000, 4010);

		buf[0] = 0xD0;
		buf[1] = 0x01;
		ret = cst2xx_i2c_read_register(client, buf, 1);
		if (ret < 0) {
			usleep_range(1000, 1010);
			continue;
		}

		if (buf[0] == 0x55)
			break;
	}

	if (buf[0] != 0x55)
		return -1;

	buf[0] = 0xD1;
	buf[1] = 0x10;
	ret = cst2xx_i2c_write(client, buf, 2);
	if (ret < 0)
		return -1;

	return 0;
}

static int cst2xx_download_program(struct i2c_client *client,
				   unsigned char *data, int len)
{
	int ret;
	int i, j;
	unsigned int wr_addr;
	unsigned char *pData;
	unsigned char *pSrc;
	unsigned char *pDst;
	unsigned char i2c_buf[8];

	pData = kmalloc(sizeof(unsigned char) * (1024 + 4), GFP_KERNEL);
	if (pData == NULL)
		return -1;

	pSrc = data;

	for (i = 0; i < (len / 1024); i++) {
		wr_addr = (i << 10) + CST2XX_BASE_ADDR;

		pData[0] = (wr_addr >> 24) & 0xFF;
		pData[1] = (wr_addr >> 16) & 0xFF;
		pData[2] = (wr_addr >> 8) & 0xFF;
		pData[3] = wr_addr & 0xFF;

		pDst = pData + 4;

		for (j = 0; j < 256; j++) {
			*pDst = *(pSrc + 3);
			*(pDst + 1) = *(pSrc + 2);
			*(pDst + 2) = *(pSrc + 1);
			*(pDst + 3) = *pSrc;

			pDst += 4;
			pSrc += 4;
		}

#ifdef HIGH_SPEED_IIC_TRANSFER
		for (j = 0; j < 256; j++) {
			i2c_buf[0] = (wr_addr >> 24) & 0xFF;
			i2c_buf[1] = (wr_addr >> 16) & 0xFF;
			i2c_buf[2] = (wr_addr >> 8) & 0xFF;
			i2c_buf[3] = wr_addr & 0xFF;

			i2c_buf[4] = pData[j * 4 + 4 + 0];
			i2c_buf[5] = pData[j * 4 + 4 + 1];
			i2c_buf[6] = pData[j * 4 + 4 + 2];
			i2c_buf[7] = pData[j * 4 + 4 + 3];

			ret = cst2xx_i2c_write(client, i2c_buf, 8);
			if (ret < 0)
				goto ERR_OUT;

			wr_addr += 4;
		}
#else
		ret = cst2xx_i2c_write(client, pData, 1024 + 4);
		if (ret < 0)
			goto ERR_OUT;
#endif
	}

	pData[3] = 0x20000FFC & 0xFF;
	pData[2] = (0x20000FFC >> 8) & 0xFF;
	pData[1] = (0x20000FFC >> 16) & 0xFF;
	pData[0] = (0x20000FFC >> 24) & 0xFF;
	pData[4] = 0x00;
	pData[5] = 0x00;
	pData[6] = 0x00;
	pData[7] = 0x00;
	ret = cst2xx_i2c_write(client, pData, 8);
	if (ret < 0)
		goto ERR_OUT;

	pData[3] = 0xD013D013 & 0xFF;
	pData[2] = (0xD013D013 >> 8) & 0xFF;
	pData[1] = (0xD013D013 >> 16) & 0xFF;
	pData[0] = (0xD013D013 >> 24) & 0xFF;
	ret = cst2xx_i2c_write(client, pData, 4);
	if (ret < 0)
		goto ERR_OUT;

	if (pData != NULL) {
		kfree(pData);
		pData = NULL;
	}
	return 0;

ERR_OUT:
	if (pData != NULL) {
		kfree(pData);
		pData = NULL;
	}
	return -1;
}

static int cst2xx_read_checksum(struct i2c_client *client)
{
	int ret;
	int i;
	unsigned int checksum;
	unsigned int bin_checksum;
	unsigned char buf[4];
	unsigned char *pData;

	for (i = 0; i < 10; i++) {
		buf[0] = 0xD0;
		buf[1] = 0x00;
		ret = cst2xx_i2c_read_register(client, buf, 1);
		if (ret < 0) {
			usleep_range(2000, 2010);
			continue;
		}

		if ((buf[0] == 0x01) || (buf[0] == 0x02))
			break;

		usleep_range(2000, 2010);
	}

	if ((buf[0] == 0x01) || (buf[0] == 0x02)) {
		buf[0] = 0xD0;
		buf[1] = 0x08;
		ret = cst2xx_i2c_read_register(client, buf, 4);

		if (ret < 0)
			return -1;

		checksum =
		    buf[0] + (buf[1] << 8) + (buf[2] << 16) + (buf[3] << 24);

		pData = fwbin + 6160;	//6*1024+16
		bin_checksum =
		    pData[0] + (pData[1] << 8) + (pData[2] << 16) +
		    (pData[3] << 24);

		buf[0] = 0xD0;
		buf[1] = 0x01;
		buf[2] = 0xA5;
		ret = cst2xx_i2c_write(client, buf, 3);

		if (ret < 0)
			return -1;
	} else {
		return -1;
	}

	return 0;
}

static int cst2xx_update_firmware(struct i2c_client *client,
				  unsigned char *pdata, int data_len)
{
	int ret;
	int retry;

	retry = 0;

start_flow:
	ret = cst2xx_enter_download_mode(client);
	if (ret < 0)
		goto fail_retry;

	ret = cst2xx_download_program(client, pdata, data_len);
	if (ret < 0)
		goto fail_retry;

	usleep_range(3000, 3010);

	ret = cst2xx_read_checksum(client);
	if (ret < 0)
		goto fail_retry;

	return 0;

fail_retry:
	if (retry < 4) {
		retry++;
		goto start_flow;
	}

	return -1;
}

static int cst2xx_boot_update_fw(struct i2c_client *client)
{
	return cst2xx_update_firmware(client, fwbin, FW_BIN_SIZE);
}

static int cst2xx_check_code(struct i2c_client *client)
{
	int retry = 0;
	int ret;
	unsigned char buf[4];

	buf[0] = 0xD0;
	buf[1] = 0x4C;
	while (retry++ < 3) {
		ret = cst2xx_i2c_read_register(client, buf, 1);
		if (ret > 0)
			break;
		usleep_range(2000, 2010);
	}

	if ((buf[0] == 226) || (buf[0] == 237) || (buf[0] == 240))
		return 0;

	ret = cst2xx_boot_update_fw(client);
	if (ret < 0)
		return -2;
	else
		return 0;
}

static void init_chip(struct i2c_client *client)
{
	int rc;

	if (gpio_reset_exist == 1) {
		cst226_shutdown_low();
		msleep(20);
		cst226_shutdown_high();
		msleep(20);
	}
	rc = test_i2c(client);
	if (rc <= 0)
		return;

	cst2xx_boot_update_fw(client);
}

static void cst2xx_touch_down(struct input_dev *input_dev, s32 id, s32 x, s32 y,
			      s32 w)
{

	s32 temp_w = (w >> 1);
	s32 temp = 0;

	temp_w += (temp & 0x0007);
	x = x * cfg_xml.xMax / 2048;
	y = y * cfg_xml.yMax / 2048;
#if CFG_TP_USE_CONFIG
	if (cfg_xml.rotate == 1) {
		int tmp;

		tmp = x;
		x = y;
		y = cfg_xml.xMax - tmp;
	} else if (cfg_xml.rotate == 2) {
		x = cfg_xml.xMax - x;
		y = cfg_xml.yMax - y;
	} else if (cfg_xml.rotate == 3) {
		int tmp;

		tmp = x;
		x = cfg_xml.yMax - y;
		y = tmp;
	}
	if (cfg_xml.xRevert == 1)
		x = cfg_xml.xMax - x;

	if (cfg_xml.yRevert == 1)
		y = cfg_xml.yMax - y;
#endif

#ifdef ICS_SLOT_REPORT
	input_mt_slot(input_dev, id);
	input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 1);
	input_report_abs(input_dev, ABS_MT_TRACKING_ID, id);
	input_report_abs(input_dev, ABS_MT_POSITION_X, x);
	input_report_abs(input_dev, ABS_MT_POSITION_Y, y);
	input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, temp_w);
	input_report_abs(input_dev, ABS_MT_WIDTH_MAJOR, temp_w);
#else
	input_report_key(input_dev, BTN_TOUCH, 1);
	input_report_abs(input_dev, ABS_MT_POSITION_X, x);
	input_report_abs(input_dev, ABS_MT_POSITION_Y, y);
	input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, temp_w);
	input_report_abs(input_dev, ABS_MT_WIDTH_MAJOR, temp_w);
	input_report_abs(input_dev, ABS_MT_TRACKING_ID, id);
	input_mt_sync(input_dev);
#endif

}

static void cst2xx_touch_up(struct input_dev *input_dev, int id)
{
#ifdef ICS_SLOT_REPORT
	input_mt_slot(input_dev, id);
	input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 0);
#else
	input_report_key(input_dev, BTN_TOUCH, 0);
	input_mt_sync(input_dev);
#endif
}

int report_flag;
static void hyn_ts_xy_worker(struct work_struct *work)
{
	u8 buf[30];
	u8 i2c_buf[8];
	u8 key_status, key_id, finger_id, sw;
	u16 input_x = 0;
	u16 input_y = 0;
	u16 input_w = 0;
	u8 cnt_up, cnt_down;
	int i, ret, idx;
	int cnt, i2c_len, len_1, len_2;

	struct hyn_ts *ts = container_of(work, struct hyn_ts, work);

	key_status = 0;
#ifdef HYN_MONITOR
	if (i2c_lock_flag == 0)
		i2c_lock_flag = 1;
	else
		return;
#endif

	buf[0] = 0xD0;
	buf[1] = 0x00;
	ret = cst2xx_i2c_read_register(ts->client, buf, 7);
	if (ret < 0)
		goto OUT_PROCESS;

	if (buf[0] == 0xAB)
		goto OUT_PROCESS;

	if (buf[6] != 0xAB)
		goto OUT_PROCESS;

	cnt = buf[5] & 0x7F;
	if (cnt > MAX_FINGERS)
		goto OUT_PROCESS;

	if (buf[5] == 0x80) {
		key_status = buf[0];
		key_id = buf[1];
		goto KEY_PROCESS;
	} else if (cnt == 0x01) {
		goto FINGER_PROCESS;
	} else {
#ifdef HIGH_SPEED_IIC_TRANSFER
		if ((buf[5] & 0x80) == 0x80) {
			i2c_len = (cnt - 1) * 5 + 3;
			len_1 = i2c_len;
			for (idx = 0; idx < i2c_len; idx += 6) {
				i2c_buf[0] = 0xD0;
				i2c_buf[1] = 0x07 + idx;

				if (len_1 >= 6) {
					len_2 = 6;
					len_1 -= 6;
				} else {
					len_2 = len_1;
					len_1 = 0;
				}

				ret =
				    cst2xx_i2c_read_register(ts->client,
							     i2c_buf, len_2);
				if (ret < 0)
					goto OUT_PROCESS;

				for (i = 0; i < len_2; i++)
					buf[5 + idx + i] = i2c_buf[i];
			}

			i2c_len += 5;
			key_status = buf[i2c_len - 3];
			key_id = buf[i2c_len - 2];
		} else {
			i2c_len = (cnt - 1) * 5 + 1;
			len_1 = i2c_len;

			for (idx = 0; idx < i2c_len; idx += 6) {
				i2c_buf[0] = 0xD0;
				i2c_buf[1] = 0x07 + idx;

				if (len_1 >= 6) {
					len_2 = 6;
					len_1 -= 6;
				} else {
					len_2 = len_1;
					len_1 = 0;
				}

				ret =
				    cst2xx_i2c_read_register(ts->client,
							     i2c_buf, len_2);
				if (ret < 0)
					goto OUT_PROCESS;

				for (i = 0; i < len_2; i++)
					buf[5 + idx + i] = i2c_buf[i];
			}
			i2c_len += 5;
		}
#else
		if ((buf[5] & 0x80) == 0x80) {
			buf[5] = 0xD0;
			buf[6] = 0x07;
			i2c_len = (cnt - 1) * 5 + 3;
			ret =
			    cst2xx_i2c_read_register(ts->client, &buf[5],
						     i2c_len);
			if (ret < 0)
				goto OUT_PROCESS;
			i2c_len += 5;
			key_status = buf[i2c_len - 3];
			key_id = buf[i2c_len - 2];
		} else {
			buf[5] = 0xD0;
			buf[6] = 0x07;
			i2c_len = (cnt - 1) * 5 + 1;
			ret =
			    cst2xx_i2c_read_register(ts->client, &buf[5],
						     i2c_len);
			if (ret < 0)
				goto OUT_PROCESS;
			i2c_len += 5;
		}
#endif

		if (buf[i2c_len - 1] != 0xAB)
			goto OUT_PROCESS;
	}

	if ((cnt > 0) && (key_status & 0x80)) {
		if (report_flag == 0xA5)
			goto KEY_PROCESS;
	}

FINGER_PROCESS:

	i2c_buf[0] = 0xD0;
	i2c_buf[1] = 0x00;
	i2c_buf[2] = 0xAB;
	ret = cst2xx_i2c_write(ts->client, i2c_buf, 3);
	if (ret < 0)
		hard_reset_chip(20);

	idx = 0;
	cnt_up = 0;
	cnt_down = 0;
	for (i = 0; i < cnt; i++) {
		input_x =
		    (unsigned int)((buf[idx + 1] << 4) |
				   ((buf[idx + 3] >> 4) & 0x0F));
		input_y =
		    (unsigned int)((buf[idx + 2] << 4) | (buf[idx + 3] & 0x0F));
		input_w = (unsigned int)(buf[idx + 4]);
		sw = (buf[idx] & 0x0F) >> 1;
		finger_id = (buf[idx] >> 4) & 0x0F;

		if (sw == 0x03) {
			cst2xx_touch_down(ts->input, finger_id, input_x,
					  input_y, input_w);
			cnt_down++;
		} else {
			cnt_up++;
			cst2xx_touch_up(ts->input, finger_id);
		}
		idx += 5;
	}

	if (cnt_down == 0)
		report_flag = 0;
	else
		report_flag = 0xCA;

	input_sync(ts->input);

	goto XY_WORK_END;

KEY_PROCESS:

	i2c_buf[0] = 0xD0;
	i2c_buf[1] = 0x00;
	i2c_buf[2] = 0xAB;
	ret = cst2xx_i2c_write(ts->client, i2c_buf, 3);
	if (ret < 0)
		hard_reset_chip(20);
#ifdef TPD_HAVE_BUTTON
	if (key_status & 0x80) {
		if ((key_status & 0x7F) == 0x03) {
			i = (key_id >> 4) - 1;
			cst2xx_touch_down(ts->input, tpd_keys_dim_local[i][0],
					  tpd_keys_dim_local[i][1], 0, 0);
			report_flag = 0xA5;
		} else {
			cst2xx_touch_up(ts->input, 0);
			report_flag = 0;
		}
	}
#endif

	input_sync(ts->input);

	goto XY_WORK_END;

OUT_PROCESS:
	buf[0] = 0xD0;
	buf[1] = 0x00;
	buf[2] = 0xAB;
	ret = cst2xx_i2c_write(ts->client, buf, 3);
	if (ret < 0)
		hard_reset_chip(20);

XY_WORK_END:
#ifdef HYN_MONITOR
	i2c_lock_flag = 0;
i2c_lock_schedule:;
#endif

	return;
}

#ifdef HYN_MONITOR
static void hyn_monitor_worker(struct work_struct *work)
{
	int retry = 0;
	int ret;
	unsigned char buf[4];

	if (i2c_lock_flag != 0)
		goto MONITOR_END;
	else
		i2c_lock_flag = 1;

	buf[0] = 0xD0;
	buf[1] = 0x4C;

	while (retry++ < 3) {
		ret = cst2xx_i2c_read_register(this_ts->client, buf, 1);
		if (ret > 0)
			break;

		usleep_range(2000, 2010);
	}

	if ((retry > 3)
	    || ((buf[0] != 226) && (buf[0] != 237) && (buf[0] != 240))) {

		hard_reset_chip(20);

		cst2xx_check_code(this_ts->client);
	}

	i2c_lock_flag = 0;

MONITOR_END:
	queue_delayed_work(hyn_monitor_workqueue, &hyn_monitor_work,
			   clk_tick_cnt);
}
#endif

#ifdef RESUME_INIT_CHIP_WORK
static void cst226_init_worker(struct work_struct *work)
{
	init_chip(this_ts->client);
	cst2xx_check_code(this_ts->client);

}
#endif

static irqreturn_t hyn_ts_irq(int irq, void *dev_id)
{
	struct hyn_ts *ts = dev_id;

	disable_irq_nosync(ts->irq);

	if (!work_pending(&ts->work))
		queue_work(ts->wq, &ts->work);

	enable_irq(ts->irq);
	return IRQ_HANDLED;
}

static int cst226_ts_init(struct i2c_client *client, struct hyn_ts *ts)
{
	struct input_dev *input_device;
	int rc = 0;

	ts->dd = &devices[ts->device_id];

	if (ts->device_id == 0) {
		ts->dd->data_size =
		    MAX_FINGERS * ts->dd->touch_bytes + ts->dd->touch_meta_data;
		ts->dd->touch_index = 0;
	}

	ts->touch_data = kzalloc(ts->dd->data_size, GFP_KERNEL);
	if (!ts->touch_data)
		return -ENOMEM;

	input_device = input_allocate_device();
	if (!input_device) {
		rc = -ENOMEM;
		goto error_alloc_dev;
	}

	ts->input = input_device;
	input_device->name = TP_NAME;
	input_device->id.bustype = BUS_I2C;
	input_device->dev.parent = &client->dev;
	input_set_drvdata(input_device, ts);

	__set_bit(EV_ABS, input_device->evbit);
	__set_bit(INPUT_PROP_DIRECT, input_device->propbit);
	input_mt_init_slots(input_device, MAX_CONTACTS, 0);
	input_set_abs_params(input_device, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(input_device, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);

#if CFG_TP_USE_CONFIG
	if (cfg_xml.rotate == 1 || cfg_xml.rotate == 3) {
		input_set_abs_params(input_device, ABS_MT_POSITION_Y, 0,
				     cfg_xml.xMax, 0, 0);
		input_set_abs_params(input_device, ABS_MT_POSITION_X, 0,
				     cfg_xml.yMax, 0, 0);
	} else {
		input_set_abs_params(input_device, ABS_MT_POSITION_X, 0,
				     cfg_xml.xMax, 0, 0);
		input_set_abs_params(input_device, ABS_MT_POSITION_Y, 0,
				     cfg_xml.yMax, 0, 0);
	}
#else
	input_set_abs_params(input_device, ABS_MT_POSITION_X, 0, TP_MAX_X, 0,
			     0);
	input_set_abs_params(input_device, ABS_MT_POSITION_Y, 0, TP_MAX_Y, 0,
			     0);
#endif

#ifdef HAVE_TOUCH_KEY
	set_bit(EV_KEY, input_device->evbit);
	input_device->evbit[0] = BIT_MASK(EV_KEY);
	for (i = 0; i < ARRAY_SIZE(key_array); i++)
		set_bit(key_array[i], input_device->keybit);
#endif

#if CFG_TP_USE_CONFIG
	if (gpio_irq_exist == 0)
		client->irq = 4;
	else if (gpio_irq_exist == 1)
		client->irq = gpio_to_irq(gpio_irq);
#else
	client->irq = TP_IRQ_PORT;
#endif
	ts->irq = client->irq;

	ts->wq = create_singlethread_workqueue("kworkqueue_ts");
	if (!ts->wq) {
		dev_err(&client->dev, "Could not create workqueue\n");
		goto error_wq_create;
	}
	flush_workqueue(ts->wq);

	INIT_WORK(&ts->work, hyn_ts_xy_worker);

#ifdef RESUME_INIT_CHIP_WORK
	ts->init_wq = create_singlethread_workqueue("ts_init_wq");
	if (!ts->init_wq) {
		dev_err(&client->dev,
			"Could not create ts_init_wq workqueue\n");
		goto error_wq_create;
	}
	flush_workqueue(ts->init_wq);
	INIT_WORK(&ts->init_work, cst226_init_worker);
#endif
	rc = input_register_device(input_device);
	if (rc)
		goto error_unreg_device;

	this_ts = ts;

	return 0;

error_unreg_device:
	destroy_workqueue(ts->wq);
error_wq_create:
	input_free_device(input_device);
error_alloc_dev:
	kfree(ts->touch_data);
	return rc;
}

static int hyn_ts_suspend(struct device *dev)
{
	flush_workqueue(this_ts->init_wq);
	if (gpio_reset_exist == 1)
		cst226_shutdown_low();
	msleep(20);

	msleep(20);

	return 0;
}

static int hyn_ts_early_resume(struct device *dev)
{
	queue_work(this_ts->init_wq, &this_ts->init_work);

	power_is_on = 1;

	return 0;
}

static int hyn_ts_resume(struct device *dev)
{
	if (power_is_on == 0) {
		queue_work(this_ts->init_wq, &this_ts->init_work);
		power_is_on = 1;
	}

	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void hyn_ts_early_suspend(struct early_suspend *h)
{
	disable_irq_nosync(this_ts->irq);

#ifdef SLEEP_RESUME_CLEAR_POINT
#ifdef REPORT_DATA_PROTOCOL_B
	int i = 0;

	for (i = 1; i <= MAX_CONTACTS; i++) {
		input_mt_slot(this_ts->input, i);
		input_mt_report_slot_state(this_ts->input, MT_TOOL_FINGER,
					   false);
	}
#endif
	input_sync(this_ts->input);
#endif

	flush_workqueue(this_ts->wq);

#ifdef HYN_MONITOR
	cancel_delayed_work_sync(&hyn_monitor_work);
#endif
}

static void hyn_ts_late_resume(struct early_suspend *h)
{

#ifdef HYN_MONITOR
	queue_delayed_work(hyn_monitor_workqueue, &hyn_monitor_work, 500);
#endif
	enable_irq(this_ts->irq);
#ifdef SLEEP_RESUME_CLEAR_POINT
#ifdef REPORT_DATA_PROTOCOL_B
	int i = 0;

	for (i = 1; i <= MAX_CONTACTS; i++) {
		input_mt_slot(this_ts->input, i);
		input_mt_report_slot_state(this_ts->input, MT_TOOL_FINGER,
					   false);
	}
#endif
	input_sync(this_ts->input);
#endif

}
#endif

static int hyn_ts_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct hyn_ts *ts;
	int rc;

#if CFG_TP_USE_CONFIG
	rc = tp_get_config(client);
	if (rc < 0)
		return -ENODEV;
#endif
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "I2C functionality not supported\n");
		return -ENODEV;
	}

	ts = kzalloc(sizeof(*ts), GFP_KERNEL);
	if (!ts)
		return -ENOMEM;
	hyn_client = client;
	ts->client = client;
	i2c_set_clientdata(client, ts);
	ts->device_id = id->driver_data;

	rc = cst226_ts_init(client, ts);
	if (rc < 0) {
		dev_err(&client->dev, "cst226 init failed\n");
		goto error_mutex_destroy;
	}

	init_chip(ts->client);
	cst2xx_check_code(ts->client);

	rc = request_irq(client->irq, hyn_ts_irq, IRQF_TRIGGER_RISING,
			 client->name, ts);
	if (rc < 0)
		goto error_req_irq_fail;

#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = hyn_ts_early_suspend;
	ts->early_suspend.resume = hyn_ts_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif
	mutex_init(&mutex);
#ifdef HYN_MONITOR
	INIT_DELAYED_WORK(&hyn_monitor_work, hyn_monitor_worker);
	hyn_monitor_workqueue =
	    create_singlethread_workqueue("hyn_monitor_workqueue");
	queue_delayed_work(hyn_monitor_workqueue, &hyn_monitor_work, 500);
#endif

	return 0;

error_req_irq_fail:
	free_irq(ts->irq, ts);

error_mutex_destroy:
	input_free_device(ts->input);
	kfree(ts);
	return rc;
}

static int hyn_ts_remove(struct i2c_client *client)
{
	struct hyn_ts *ts = i2c_get_clientdata(client);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&ts->early_suspend);
#endif

	device_init_wakeup(&client->dev, 0);
	cancel_work_sync(&ts->work);
	free_irq(ts->irq, ts);
	destroy_workqueue(ts->wq);
	cancel_work_sync(&ts->init_work);
	destroy_workqueue(ts->init_wq);
	input_unregister_device(ts->input);
#ifdef HYN_MONITOR
	cancel_delayed_work_sync(&hyn_monitor_work);
	destroy_workqueue(hyn_monitor_workqueue);
#endif

#if CFG_TP_USE_CONFIG
	gpio_free(gpio_irq);
	gpio_free(gpio_reset);
#endif
	kfree(ts->touch_data);
	kfree(ts);

	return 0;
}

static const struct of_device_id hyn_match_table[] = {
	{.compatible = "hyn, cst226",},
	{},
};

static const struct i2c_device_id hyn_ts_id[] = {
	{CST226_I2C_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, hyn_ts_id);
static unsigned short hyn_addresses[] = {
	CST226_I2C_ADDR,
	I2C_CLIENT_END,
};

static const struct dev_pm_ops tp_pm_ops = {
	.resume_early = hyn_ts_early_resume,
	.suspend = hyn_ts_suspend,
	.resume = hyn_ts_resume,
};

static struct i2c_driver hyn_ts_driver = {
	.driver = {
		   .name = CST226_I2C_NAME,
		   .owner = THIS_MODULE,
		   .pm = &tp_pm_ops,
		   .of_match_table = hyn_match_table,
		   },
	.probe = hyn_ts_probe,
	.remove = hyn_ts_remove,
	.id_table = hyn_ts_id,
	.address_list = hyn_addresses,
};

module_i2c_driver(hyn_ts_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("CST2XX touchscreen controller driver");
MODULE_AUTHOR("Ying Zhang, ying.zhang@hynitron.com.cn");
MODULE_ALIAS("platform:HYN_ts");
