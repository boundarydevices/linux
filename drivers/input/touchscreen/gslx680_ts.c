/*
 * drivers/input/touchscreen/gslX680.c
 *
 * Copyright (c) 2012 Shanghai Basewin
 *   Guan Yuwei<guanyuwei@basewin.com>
 * Copyright (c) 2013 Joe Burmeister
 *   Joe Burmeister<joe.a.burmeister@googlemail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of_gpio.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/wait.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/pm.h>
#include <linux/earlysuspend.h>
#endif

#define GSLX680_I2C_NAME	"gslx680"

#define GSL_DATA_REG		0x80
#define GSL_STATUS_REG		0xe0
#define GSL_PAGE_REG		0xf0

#define PRESS_MAX		255
#define MAX_FINGERS		10
#define MAX_CONTACTS		10
#define DMA_TRANS_LEN		0x20


#ifdef HAVE_TOUCH_KEY
static u16 key = 0;
static int key_state_flag = 0;
struct key_data {
	u16 key;
	u16 x_min;
	u16 x_max;
	u16 y_min;
	u16 y_max;
};

const u16 key_array[]={
	KEY_BACK,
	KEY_HOME,
	KEY_MENU,
	KEY_SEARCH,
};
#define MAX_KEY_NUM	 (sizeof(key_array)/sizeof(key_array[0]))

struct key_data gsl_key_data[MAX_KEY_NUM] = {
	{KEY_BACK, 2048, 2048, 2048, 2048},
	{KEY_HOME, 2048, 2048, 2048, 2048},
	{KEY_MENU, 2048, 2048, 2048, 2048},
	{KEY_SEARCH, 2048, 2048, 2048, 2048},
};
#endif

struct gsl_ts_data {
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

static struct gsl_ts_data devices[] = {
	{
		.x_index = 6,
		.y_index = 4,
		.z_index = 5,
		.id_index = 7,
		.data_reg = GSL_DATA_REG,
		.status_reg = GSL_STATUS_REG,
		.update_data = 0x4,
		.touch_bytes = 4,
		.touch_meta_data = 4,
		.finger_size = 70,
	},
};

struct gsl_ts {
	struct i2c_client *client;
	struct input_dev *input;
	struct work_struct work;
	struct workqueue_struct *wq;
	struct gsl_ts_data *dd;
	u8 *touch_data;
	u8 device_id;
	u8 prev_touches;
	u8 firmware_loaded;
	bool is_suspended;
	u8 opened;
	u8 irq_enable_needed;
	struct mutex sus_lock;
	int irq;
	struct gpio_desc *gpiod_irq;
	struct gpio_desc *gpiod_shutdown;
#if defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif
#ifdef GSL_TIMER
	struct timer_list gsl_timer;
#endif
};

static u32 id_sign[MAX_CONTACTS+1] = {0};
static u8 id_state_flag[MAX_CONTACTS+1] = {0};
static u8 id_state_old_flag[MAX_CONTACTS+1] = {0};
static u16 x_old[MAX_CONTACTS+1] = {0};
static u16 y_old[MAX_CONTACTS+1] = {0};
static u16 x_new = 0;
static u16 y_new = 0;


static int screen_max_x = 0;
static int screen_max_y = 0;
#define SCREEN_MAX_X			(screen_max_x)
#define SCREEN_MAX_Y			(screen_max_y)
static int revert_x_flag = 0;
static int revert_y_flag = 0;
static int exchange_x_y_flag = 0;

static void gslX680_shutdown(struct gsl_ts *ts, int active)
{
	pr_info("%s:active=%d\n", __func__, active);
	if (!IS_ERR(ts->gpiod_shutdown))
		gpiod_set_value(ts->gpiod_shutdown, active);
}

static inline u16 join_bytes(u8 a, u8 b)
{
	u16 ab = 0;
	ab = ab | a;
	ab = ab << 8 | b;
	return ab;
}

static int process_buffer(struct i2c_client *client, const u8* buf, int bi, int verify)
{
	int rc;
	u8 tbuf[128];

	if (!verify || (buf[0] == GSL_PAGE_REG)) {
		rc = i2c_master_send(client, buf, bi);
		if (rc < 0) {
			pr_err("%s: i2c_master_send failed %d. \n", __func__, rc);
			return rc;
		}
		pr_info("%s: reg=0x%02x 0x%02x len=%d. \n", __func__, buf[0], buf[1], bi);
		return 0;
	}
	rc = i2c_master_send(client, buf, 1);
	if (rc < 0) {
		pr_err("%s: i2c_master_send failed %d. \n", __func__, rc);
		return rc;
	}
	bi--;
	if (bi > sizeof(tbuf))
		return -1;
	rc = i2c_master_recv(client, tbuf, bi);
	if (rc < 0) {
		pr_err("%s: i2c_master_recv failed %d. \n", __func__, rc);
		return rc;
	}
	if (memcmp(&buf[1], &tbuf[0], bi)) {
		pr_err("%s: mismatch\n", __func__);
		print_hex_dump_bytes("firmware:", DUMP_PREFIX_NONE, &buf[1], bi);
		print_hex_dump_bytes("verify  :", DUMP_PREFIX_NONE, tbuf, bi);
		rc = -EINVAL;
	}
	return rc;
}

static int load_verify_firmware(struct i2c_client *client, const u8 *data, size_t size, int verify)
{
	u8 buf[128 + 4];
	int bi = 0;
	int address = 0;
	int rc = 0;
	size_t i;

	for (i = 0; i < size; i += 8) {
		const u8 *pr = &data[i];
		const u8 *pv = &pr[4];
		u32 r = pr[0] | (pr[1] << 8) | pr[2] << 16 | pr[3] << 24;

		if ((r > 0x7c) && (r != GSL_PAGE_REG)) {
			pr_err("%s: r(%x) too big\n", __func__, r);
			return -EINVAL;
		}
		if ((bi > (sizeof(buf) - 4)) || ((address != r) && bi)) {
			rc = process_buffer(client, buf, bi, verify);
			if (rc < 0)
				return rc;
			bi = 0;
		}
		if (!bi) {
			buf[bi++] = r;
			address = r;
		}
		if (r == GSL_PAGE_REG) {
			buf[bi++] = *pv++;
			address += 1;
		} else {
			buf[bi++] = *pv++;
			buf[bi++] = *pv++;
			buf[bi++] = *pv++;
			buf[bi++] = *pv++;
			address += 4;
		}
	}
	if (bi)
		rc = process_buffer(client, buf, bi, verify);
	return rc;
}

static const char gsl_firmware[] = "gsl1680_7inch_800x480.fw";

static int gsl_load_fw(struct gsl_ts *ts, struct i2c_client *client)
{
	const struct firmware *fw = NULL;
	int rc;

	pr_info("=============gsl_load_fw start==============\n");

	rc = request_firmware(&fw, gsl_firmware, &client->dev);
	if (rc) {
		dev_err(&client->dev, "Unable to open firmware %s\n", gsl_firmware);
		return rc;
	}

	rc = load_verify_firmware(client, fw->data, fw->size, 0);
	if (rc < 0)
		goto error;
#if 1
	/* Cannot read back firmware!!!  */
	rc = load_verify_firmware(client, fw->data, fw->size, 1);
	if (rc < 0) {
		pr_err("verify error\n");
		rc = 0;
	}
#endif
	pr_info("=============gsl_load_fw end==============\n");
	ts->firmware_loaded = 1;
error:
	release_firmware(fw);
	return rc;
}


static int gsl_ts_write(struct i2c_client *client, u8 reg, u8 *pdata, int datalen)
{
	int ret = 0;
	u8 tmp_buf[128];
	unsigned int bytelen = 0;
	if (datalen > 125) {
		pr_info("%s too big datalen = %d!\n", __func__, datalen);
		return -1;
	}

	tmp_buf[0] = reg;
	bytelen++;

	if (datalen != 0 && pdata != NULL) {
		memcpy(&tmp_buf[bytelen], pdata, datalen);
		bytelen += datalen;
	}
	print_hex_dump_bytes("gslx tx:", DUMP_PREFIX_NONE, tmp_buf, bytelen);

	ret = i2c_master_send(client, tmp_buf, bytelen);
	return ret;
}

static int gsl_ts_read(struct i2c_client *client, u8 reg, u8 *pdata, unsigned int datalen)
{
	int ret = 0;

	if (datalen > 126) {
		pr_info("%s too big datalen = %d!\n", __func__, datalen);
		return -1;
	}

	ret = gsl_ts_write(client, reg, NULL, 0);
	if (ret < 0) {
		pr_info("%s set data address fail!\n", __func__);
		return ret;
	}

	ret = i2c_master_recv(client, pdata, datalen);
	pr_info("gslx rx reg: %02x\n", reg);
	print_hex_dump_bytes("gslx rx:", DUMP_PREFIX_NONE, pdata, datalen);
	return ret;
}

static int startup_chip(struct i2c_client *client)
{
	u8 tmp = 0x00;
	int rc = gsl_ts_write(client, 0xe0, &tmp, 1);
	msleep(10);
	return rc;
}

static int reset_chip(struct i2c_client *client)
{
	u8 buf[4] = {0x00};
	u8 tmp = 0x88;
	int rc = gsl_ts_write(client, 0xe0, &tmp, sizeof(tmp));
	if (rc < 0) {
		pr_info("%s: gsl_ts_write 1 fail!\n", __func__);
		return rc;
	}
	msleep(10);

	tmp = 0x04;
	rc = gsl_ts_write(client, 0xe4, &tmp, sizeof(tmp));
	if (rc < 0) {
		pr_info("%s: gsl_ts_write 2 fail!\n", __func__);
		return rc;
	}
	rc = gsl_ts_read(client, 0xe4, &tmp, sizeof(tmp));
	if (rc < 0) {
		pr_info("%s: gsl_ts_read  fail!\n", __func__);
		return rc;
	}
	msleep(10);

	rc = gsl_ts_write(client, 0xbc, buf, sizeof(buf));
	if (rc < 0) {
		pr_info("%s: gsl_ts_write 3 fail!\n", __func__);
		return rc;
	}
	msleep(10);

	return 0;
}

static int init_chip(struct i2c_client *client, int load)
{
	int rc;
	struct gsl_ts *ts = i2c_get_clientdata(client);

	gslX680_shutdown(ts, 0);
	msleep(30);
	rc = reset_chip(client);
	if (rc < 0) {
		pr_info("%s: reset_chip fail: %i\n", __func__, rc);
		return rc;
	}
	if (load) {
		rc = gsl_load_fw(ts, client);
		if (rc < 0) {
			pr_info("%s: gsl_load_fw fail: %i\n", __func__, rc);
			return rc;
		}
	}
	rc = startup_chip(client);
	if (rc < 0) {
		pr_info("%s: startup_chip fail: %i\n", __func__, rc);
		return rc;
	}
	rc = reset_chip(client);
	if (rc < 0) {
		pr_info("%s: second reset_chip fail: %i\n", __func__, rc);
		return rc;
	}
	gslX680_shutdown(ts, 1);
	msleep(50);
	gslX680_shutdown(ts, 0);
	msleep(30);

	gslX680_shutdown(ts, 1);
	msleep(5);
	gslX680_shutdown(ts, 0);
	msleep(20);
	rc = reset_chip(client);
	if (rc < 0) {
		pr_info("%s: third reset_chip fail: %i\n", __func__, rc);
		return rc;
	}
	rc = startup_chip(client);
	if (rc < 0) {
		pr_info("%s: second startup_chip fail: %i\n", __func__, rc);
		return rc;
	}
	return 0;
}


static void record_point(u16 x, u16 y , u8 id)
{
	u16 x_err =0;
	u16 y_err =0;

	id_sign[id]=id_sign[id]+1;

	if (id_sign[id]==1) {
		x_old[id]=x;
		y_old[id]=y;
	}

	x = (x_old[id] + x)/2;
	y = (y_old[id] + y)/2;
   
	if (x>x_old[id]) {
		x_err=x -x_old[id];
	} else {
		x_err=x_old[id]-x;
	}

	if (y>y_old[id]) {
		y_err=y -y_old[id];
	} else {
		y_err=y_old[id]-y;
	}

	if ( (x_err > 6 && y_err > 2) || (x_err > 2 && y_err > 6) ) {
		x_new = x;	 x_old[id] = x;
		y_new = y;	 y_old[id] = y;
	} else {
		if (x_err > 6) {
			x_new = x;	 x_old[id] = x;
		} else
			x_new = x_old[id];
		if (y_err> 6) {
			y_new = y;	 y_old[id] = y;
		} else
			y_new = y_old[id];
	}

	if (id_sign[id]==1) {
		x_new= x_old[id];
		y_new= y_old[id];
	}

}

#ifdef HAVE_TOUCH_KEY
static void report_key(struct gsl_ts *ts, u16 x, u16 y)
{
	u16 i = 0;

	for(i = 0; i < MAX_KEY_NUM; i++) {
		if ((gsl_key_data[i].x_min < x) && (x < gsl_key_data[i].x_max)&&(gsl_key_data[i].y_min < y) && (y < gsl_key_data[i].y_max)) {
			key = gsl_key_data[i].key;
			input_report_key(ts->input, key, 1);
			input_sync(ts->input);
			key_state_flag = 1;
			break;
		}
	}
}
#endif

static void report_data(struct gsl_ts *ts, u16 x, u16 y, u8 pressure, u8 id)
{
	swap(x, y);

	if (x>=SCREEN_MAX_X||y>=SCREEN_MAX_Y) {
	#ifdef HAVE_TOUCH_KEY
		report_key(ts,x,y);
	#endif
		return;
	}

	if (exchange_x_y_flag)
		swap(x, y);

	if (revert_x_flag)
		x=SCREEN_MAX_X-x;

	if (revert_y_flag)
		y=SCREEN_MAX_Y-y;

	input_mt_slot(ts->input, id);
	input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, 1);
	input_report_abs(ts->input, ABS_MT_TOUCH_MAJOR, pressure);
	input_report_abs(ts->input, ABS_MT_POSITION_X, x);
	input_report_abs(ts->input, ABS_MT_POSITION_Y, y);
	input_report_abs(ts->input, ABS_MT_WIDTH_MAJOR, 1);
}

static void process_gslX680_data(struct gsl_ts *ts)
{
	u8 id, touches;
	u16 x, y;
	int i = 0;

	touches = ts->touch_data[ts->dd->touch_index];
	for(i=1;i<=MAX_CONTACTS;i++) {
		if (touches == 0)
			id_sign[i] = 0;
		id_state_flag[i] = 0;
	}

	for(i= 0;i < (touches > MAX_FINGERS ? MAX_FINGERS : touches);i ++) {
		x = join_bytes( ( ts->touch_data[ts->dd->x_index  + 4 * i + 1] & 0xf),
				ts->touch_data[ts->dd->x_index + 4 * i]);
		y = join_bytes(ts->touch_data[ts->dd->y_index + 4 * i + 1],
				ts->touch_data[ts->dd->y_index + 4 * i ]);
		id = ts->touch_data[ts->dd->id_index + 4 * i] >> 4;

		if (1 <=id && id <= MAX_CONTACTS) {
			record_point(x, y , id);
			report_data(ts, x_new, y_new, 10, id);
			id_state_flag[id] = 1;
		}
	}
	for(i=1;i<=MAX_CONTACTS;i++) {
		if ((0 != id_state_old_flag[i]) && (0 == id_state_flag[i])) {
			input_mt_slot(ts->input, i);
			input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, false);
			id_sign[i]=0;
		}
		id_state_old_flag[i] = id_state_flag[i];
	}


	if (0 == touches) {
	#ifdef HAVE_TOUCH_KEY
		if (key_state_flag) {
			input_report_key(ts->input, key, 0);
			input_sync(ts->input);
			key_state_flag = 0;
		}
	#endif
	}


	if (touches || touches != ts->prev_touches)
	{
		input_mt_report_pointer_emulation(ts->input, true);
		input_sync(ts->input);
	}

	ts->prev_touches = touches;
}


static void gsl_ts_xy_worker(struct work_struct *work)
{
	int rc;
	u8 read_buf[4] = {0};

	struct gsl_ts *ts = container_of(work, struct gsl_ts,work);

	if (ts->is_suspended || !ts->input || !ts->opened) {
		dev_dbg(&ts->client->dev, "TS is waiting\n");
		ts->irq_enable_needed = true;
		return;
	}
	if (!gpiod_get_value(ts->gpiod_irq)) {
		/* No interrrupt pending */
		goto schedule1;
	}

	rc = gsl_ts_read(ts->client, GSL_DATA_REG, ts->touch_data,
			ts->dd->data_size);

	if (rc < 0) {
		dev_err(&ts->client->dev, "read failed\n");
		goto schedule;
	}
	print_hex_dump_bytes("gslx work:", DUMP_PREFIX_NONE, ts->touch_data, ts->dd->data_size);

	if (ts->touch_data[ts->dd->touch_index] == 0xff) {
		goto schedule;
	}

	rc = gsl_ts_read( ts->client, 0xbc, read_buf, sizeof(read_buf));
	if (rc < 0) {
		dev_err(&ts->client->dev, "read 0xbc failed\n");
		goto schedule;
	}
   
	if (read_buf[3] == 0 && read_buf[2] == 0 && read_buf[1] == 0 && read_buf[0] == 0) {
		process_gslX680_data(ts);
	} else {
		rc = reset_chip(ts->client);
		if (rc < 0) {
			dev_err(&ts->client->dev, "%s: reset_chip failed\n", __func__);
			goto schedule;
		}
		rc = startup_chip(ts->client);
		if (rc < 0) {
			dev_err(&ts->client->dev, "%s: startup_chip failed\n", __func__);
			goto schedule;
		}
	}

schedule:
	if (gpiod_get_value(ts->gpiod_irq)) {
		/* Sleep if irq is still pending */
		msleep(1000);
	}
schedule1:
	enable_irq(ts->irq);
   
}

static irqreturn_t gsl_ts_irq(int irq, void *dev_id)
{
	struct gsl_ts *ts = dev_id;

	disable_irq_nosync(ts->irq);
	if (!work_pending(&ts->work))
	{
		queue_work(ts->wq, &ts->work);
	}
	return IRQ_HANDLED;
}


#ifdef GSL_TIMER
static void gsl_timer_handle(unsigned long data)
{
	struct gsl_ts *ts = (struct gsl_ts *)data;

#ifdef GSL_DEBUG
	pr_info("----------------gsl_timer_handle-----------------\n");
#endif

	disable_irq_nosync(ts->irq);
	check_mem_data(ts->client);
	ts->gsl_timer.expires = jiffies + 3 * HZ;
	add_timer(&ts->gsl_timer);
	enable_irq(ts->irq);
}
#endif

static const struct i2c_device_id gslx680_ts_id[] = {
	{ GSLX680_I2C_NAME, 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, gslx680_ts_id);

static int ts_open(struct input_dev *idev)
{
	struct gsl_ts *ts = input_get_drvdata(idev);
	struct device *dev = &ts->client->dev;
	int rc;

	disable_irq(ts->irq);
	flush_workqueue(ts->wq);
	rc = init_chip(ts->client, ts->firmware_loaded ? 0 : 1);

	if (rc < 0) {
		dev_err(dev, "open failed %d\n", rc);
		return rc;
	}
	ts->opened = 1;
	if (ts->irq_enable_needed) {
		ts->irq_enable_needed = 0;
		enable_irq(ts->irq);
	}
	enable_irq(ts->irq);
	return 0;
}

static void ts_close(struct input_dev *idev)
{
	struct gsl_ts *ts = input_get_drvdata(idev);
	struct device *dev = &ts->client->dev;
	int rc;

	ts->opened = 0;
	rc = reset_chip(ts->client);
	if (rc < 0)
		dev_err(dev, "reset failed %d\n", rc);
	gslX680_shutdown(ts, 1);
}

static int gsl_ts_init_ts(struct i2c_client *client, struct gsl_ts *ts)
{
	struct input_dev *idev;
	int rc = 0;

	pr_info("[GSLX680] Enter %s\n", __func__);

	idev = input_allocate_device();
	if (!idev) {
		rc = -ENOMEM;
		goto error_alloc_dev;
	}

	ts->input = idev;
	idev->name = GSLX680_I2C_NAME;
	idev->id.bustype = BUS_I2C;
	idev->open      = ts_open;
	idev->close     = ts_close;
	idev->dev.parent = &client->dev;
	input_set_drvdata(idev, ts);


	set_bit(EV_ABS, idev->evbit);
	set_bit(EV_KEY, idev->evbit);
	set_bit(EV_SYN, idev->evbit);

	set_bit(ABS_X,		  idev->absbit);
	set_bit(ABS_Y,		  idev->absbit);
	set_bit(ABS_PRESSURE,   idev->absbit);

	set_bit(ABS_MT_POSITION_X, idev->absbit);
	set_bit(ABS_MT_POSITION_Y, idev->absbit);
	set_bit(ABS_MT_TOUCH_MAJOR, idev->absbit);
	set_bit(ABS_MT_WIDTH_MAJOR, idev->absbit);

	set_bit(BTN_TOUCH, idev->keybit);

	set_bit(BTN_TOOL_DOUBLETAP, idev->keybit);
	set_bit(BTN_TOOL_TRIPLETAP, idev->keybit);
	set_bit(BTN_TOOL_QUADTAP, idev->keybit);
	set_bit(BTN_TOOL_QUINTTAP, idev->keybit);


	input_set_abs_params(idev, ABS_X, 0, SCREEN_MAX_X, 0, 0);
	input_set_abs_params(idev, ABS_Y, 0, SCREEN_MAX_Y, 0, 0);
	input_set_abs_params(idev, ABS_PRESSURE, 0, PRESS_MAX, 0 , 0);

	input_set_abs_params(idev, ABS_MT_POSITION_X, 0, SCREEN_MAX_X, 0, 0);
	input_set_abs_params(idev, ABS_MT_POSITION_Y, 0, SCREEN_MAX_Y, 0, 0);
	input_set_abs_params(idev, ABS_MT_TOUCH_MAJOR, 0, PRESS_MAX, 0, 0);
	input_set_abs_params(idev, ABS_MT_WIDTH_MAJOR, 0, 200, 0, 0);
	input_set_abs_params(idev, ABS_MT_TRACKING_ID, 0, (MAX_CONTACTS+1), 0, 0);

	input_mt_init_slots(idev, (MAX_CONTACTS+1), 0);

#ifdef HAVE_TOUCH_KEY
	idev->evbit[0] = BIT_MASK(EV_KEY);
	for (i = 0; i < MAX_KEY_NUM; i++)
		set_bit(key_array[i], idev->keybit);
#endif

	rc = input_register_device(idev);
	if (rc)
		goto error_unreg_device;

	return 0;

error_unreg_device:
	input_free_device(idev);
error_alloc_dev:
	return rc;
}

static int gsl_ts_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct gsl_ts *ts = i2c_get_clientdata(client);

	pr_info("I'am in gsl_ts_suspend() start\n");
	ts->is_suspended = true;

#ifdef GSL_TIMER
	pr_info( "gsl_ts_suspend () : delete gsl_timer\n");

	del_timer(&ts->gsl_timer);
#endif
	disable_irq_nosync(ts->irq);

	reset_chip(ts->client);
	gslX680_shutdown(ts, 1);
	msleep(10);

	return 0;
}

static int gsl_ts_resume(struct i2c_client *client)
{
	struct gsl_ts *ts = i2c_get_clientdata(client);

	pr_info("I'am in gsl_ts_resume() start\n");

	gslX680_shutdown(ts, 0);
	msleep(20);
	reset_chip(ts->client);
	startup_chip(ts->client);

#ifdef GSL_TIMER
	pr_info( "gsl_ts_resume () : add gsl_timer\n");

	init_timer(&ts->gsl_timer);
	ts->gsl_timer.expires = jiffies + 3 * HZ;
	ts->gsl_timer.function = &gsl_timer_handle;
	ts->gsl_timer.data = (unsigned long)ts;
	add_timer(&ts->gsl_timer);
#endif

	enable_irq(ts->irq);
	ts->is_suspended = false;

	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void gsl_ts_early_suspend(struct early_suspend *h)
{
	struct gsl_ts *ts = container_of(h, struct gsl_ts, early_suspend);
	pr_info("[GSL1680] Enter %s\n", __func__);
	gsl_ts_suspend(&ts->client->dev);
}

static void gsl_ts_late_resume(struct early_suspend *h)
{
	struct gsl_ts *ts = container_of(h, struct gsl_ts, early_suspend);
	pr_info("[GSL1680] Enter %s\n", __func__);
	gsl_ts_resume(&ts->client->dev);
}
#endif

static int
gslx680_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct gsl_ts *ts;
	int rc;
	struct device *dev = &client->dev;

	pr_info("====%s begin=====.  \n", __func__);


	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(dev, "I2C functionality not supported\n");
		return -ENODEV;
	}

	ts = kzalloc(sizeof(*ts), GFP_KERNEL);
	if (!ts)
		return -ENOMEM;
	pr_info("==kzalloc success=\n");

	ts->client = client;
	i2c_set_clientdata(client, ts);
	ts->device_id = 0;

	ts->touch_data = NULL;
	ts->is_suspended = false;
	mutex_init(&ts->sus_lock);

	ts->dd = &devices[ts->device_id];

	if (ts->device_id == 0) {
		ts->dd->data_size = MAX_FINGERS * ts->dd->touch_bytes + ts->dd->touch_meta_data;
		ts->dd->touch_index = 0;
	}

	ts->touch_data = kzalloc(ts->dd->data_size, GFP_KERNEL);
	if (!ts->touch_data) {
		pr_err("%s: Unable to allocate memory\n", __func__);
		rc = -ENOMEM;
		goto error_mutex_destroy;
	}

	ts->prev_touches = 0;
	ts->irq = client->irq;
	ts->gpiod_shutdown = devm_gpiod_get(dev, "shutdown", GPIOD_OUT_HIGH);
	if (IS_ERR(ts->gpiod_shutdown))
		dev_warn(dev, "unable to claim shutdown gpio\n");

	ts->gpiod_irq = devm_gpiod_get(dev, "wakeup", GPIOD_IN);
	if (IS_ERR(ts->gpiod_irq)) {
		dev_err(dev, "unable to claim irq gpio\n");
		rc = PTR_ERR(ts->gpiod_irq);
		goto err_wakeup;
	}

	gslX680_shutdown(ts, 0);
	msleep(30);
	rc = reset_chip(client);
	if (rc < 0) {
		dev_err(dev, "reset failed\n");
		goto err_reset_chip;
	}

	ts->wq = create_singlethread_workqueue("kworkqueue_ts");
	if (!ts->wq) {
		dev_err(&client->dev, "Could not create workqueue\n");
		goto error_wq_create;
	}
	flush_workqueue(ts->wq);

	INIT_WORK(&ts->work, gsl_ts_xy_worker);

	rc = request_irq(client->irq, gsl_ts_irq,
			gpiod_is_active_low(ts->gpiod_irq) ? IRQF_TRIGGER_LOW :
					IRQF_TRIGGER_HIGH, "gslx680", ts);

	if (rc < 0) {
		dev_err(dev, "gslx680_ts probe: request irq failed\n");
		goto err_request_irq;
	}

	rc = gsl_ts_init_ts(client, ts);
	if (rc < 0) {
		dev_err(dev, "GSLX680 init failed\n");
		goto err_ts_init;
	}


#ifdef GSL_TIMER
	pr_info( "gsl_ts_probe () : add gsl_timer\n");

	init_timer(&ts->gsl_timer);
	ts->gsl_timer.expires = jiffies + 3 * HZ;
	ts->gsl_timer.function = &gsl_timer_handle;
	ts->gsl_timer.data = (unsigned long)ts;
	add_timer(&ts->gsl_timer);
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = gsl_ts_early_suspend;
	ts->early_suspend.resume = gsl_ts_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif

	gslX680_shutdown(ts, 1);
	pr_info("==%s over =\n", __func__);
	return 0;
err_ts_init:
	free_irq(ts->irq, ts);
err_request_irq:
	destroy_workqueue(ts->wq);
error_wq_create:
err_reset_chip:
err_wakeup:
	kfree(ts->touch_data);
error_mutex_destroy:
	mutex_destroy(&ts->sus_lock);
	input_free_device(ts->input);
	kfree(ts);
	return rc;
}

static int gslx680_ts_remove(struct i2c_client *client)
{
	struct gsl_ts *ts = i2c_get_clientdata(client);
	pr_info("==gslx680_ts_remove=\n");

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&ts->early_suspend);
#endif
	device_init_wakeup(&client->dev, 0);

	disable_irq(ts->irq);
	cancel_work_sync(&ts->work);
	free_irq(ts->irq, ts);
	input_unregister_device(ts->input);
	input_free_device(ts->input);
	destroy_workqueue(ts->wq);

	mutex_destroy(&ts->sus_lock);

	if (ts->touch_data)
		kfree(ts->touch_data);
	gslX680_shutdown(ts, 1);
	kfree(ts);
	return 0;
}


static struct i2c_driver gslx680_ts_driver = {
	.class	  = I2C_CLASS_HWMON,
	.probe	  = gslx680_ts_probe,
	.remove	 = gslx680_ts_remove,
	.id_table   = gslx680_ts_id,
	.driver = {
		.name   = GSLX680_I2C_NAME,
		.owner  = THIS_MODULE,
	},
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend	= gsl_ts_suspend,
	.resume	 = gsl_ts_resume,
#endif
};


static int gslx680_ts_init(void)
{
	int ret = 0;

	pr_info("===========================%s=====================\n", __func__);

	ret = i2c_add_driver(&gslx680_ts_driver);

	return ret;
}

static void gslx680_ts_exit(void)
{
	pr_info("==gslx680_ts_exit==\n");
	i2c_del_driver(&gslx680_ts_driver);
}


late_initcall(gslx680_ts_init);
module_exit(gslx680_ts_exit);


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GSLX680 touchscreen controller driver");
MODULE_AUTHOR("Joe Burmeister, joe.a.burmeister@gmail.com");
MODULE_ALIAS("platform:gsl_ts");
