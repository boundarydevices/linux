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

#define GSLX680_I2C_NAME	"gslx680"

#define GSL_DATA_REG		0x80
#define GSL_STATUS_REG		0xe0
#define GSL_PAGE_REG		0xf0

#define MAX_CONTACTS		10
#define TOUCH_DATA_SIZE		(MAX_CONTACTS * 4 + 4)

#define SCREEN_MAX_X 		800
#define SCREEN_MAX_Y 		480

#define X_INDEX		4
#define Y_INDEX		6
#define ID_INDEX	7

struct gsl_ts {
	struct i2c_client *client;
	struct input_dev *input;
	struct work_struct work;
	struct workqueue_struct *wq;
	struct work_struct mt_set_nice_work;
	u8 firmware_loaded;
	bool is_suspended;
	u8 opened;
	u8 irq_enable_needed;
	unsigned down_mask;
	int irq;
	struct gpio_desc *gpiod_irq;
	struct gpio_desc *gpiod_shutdown;
	u8 touch_data[TOUCH_DATA_SIZE];
	unsigned x[MAX_CONTACTS];
	unsigned y[MAX_CONTACTS];
};

static void gslX680_shutdown(struct gsl_ts *ts, int active)
{
	pr_info("%s:active=%d\n", __func__, active);
	if (!IS_ERR(ts->gpiod_shutdown))
		gpiod_set_value(ts->gpiod_shutdown, active);
}

static int gsl_ts_write(struct i2c_client *client, u8 reg, u8 *pdata, unsigned datalen)
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
#ifdef DEBUG
	print_hex_dump_bytes("gslx tx:", DUMP_PREFIX_NONE, tmp_buf, bytelen);
#endif
	ret = i2c_master_send(client, tmp_buf, bytelen);
	if (ret < 0) {
		pr_info("%s: error (%d)\n", __func__, ret);
		mdelay(50);
		ret = i2c_master_send(client, tmp_buf, bytelen);
		if (ret < 0) {
			pr_info("%s: error2 (%d)\n", __func__, ret);
		}
	}
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
#ifdef DEBUG
	pr_info("gslx rx reg: %02x\n", reg);
	print_hex_dump_bytes("gslx rx:", DUMP_PREFIX_NONE, pdata, datalen);
#endif
	if (ret < 0) {
		pr_info("%s: error (%d)\n", __func__, ret);
	}
	return ret;
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
		pr_debug("%s: reg=0x%02x 0x%02x len=%d. \n", __func__, buf[0], buf[1], bi);
		return 0;
	}
	rc = i2c_master_send(client, buf, 1);
	if (rc < 0) {
		pr_err("%s: i2c_master_send failed %d. \n", __func__, rc);
		return rc;
	}
	bi--;
	if (bi > sizeof(tbuf))
		return -EINVAL;
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

static const char gsl_firmware[] = "gsl1680.fw";

static int gsl_load_fw(struct gsl_ts *ts, struct i2c_client *client)
{
	const struct firmware *fw = NULL;
	int rc;

	pr_info("=============gsl_load_fw request==============\n");

	rc = request_firmware(&fw, gsl_firmware, &client->dev);
	if (rc) {
		dev_err(&client->dev, "Unable to open firmware %s\n", gsl_firmware);
		return rc;
	}

	pr_info("=============gsl_load_fw start==============\n");
	rc = load_verify_firmware(client, fw->data, fw->size, 0);
	if (rc < 0)
		goto error;
#if 0
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

static int startup_chip(struct i2c_client *client)
{
	int rc;
	char buf[4];

	buf[0] = 0;
	rc = gsl_ts_write(client, GSL_STATUS_REG, buf, 1);
	if (rc < 0)
		return rc;
	msleep(10);
	return 0;
}

static int reset_chip(struct i2c_client *client)
{
	int rc;
	char buf[4];

	buf[0] = 0x88;
	rc = gsl_ts_write(client, GSL_STATUS_REG, buf, 1);
	if (rc < 0) {
		pr_info("%s: gsl_ts_write 1 fail!\n", __func__);
		return rc;
	}
	msleep(10);

	buf[0] = 0x04;
	rc = gsl_ts_write(client, 0xe4, buf, 1);
	if (rc < 0) {
		pr_info("%s: gsl_ts_write 2 fail!\n", __func__);
		return rc;
	}
	msleep(10);

	buf[0] = 0x00;
	buf[1] = 0x00;
	buf[2] = 0x00;
	buf[3] = 0x00;
	rc = gsl_ts_write(client, 0xbc, buf, 4);
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

	gslX680_shutdown(ts, 1);
	msleep(20);
	gslX680_shutdown(ts, 0);
	msleep(20);

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
	rc = startup_chip(client);
	if (rc < 0) {
		pr_info("%s: second startup_chip fail: %i\n", __func__, rc);
		return rc;
	}
	return 0;
}

static void check_mem_data(struct gsl_ts *ts, struct i2c_client *client)
{
	char read_buf[4]  = {0};

	msleep(30);
	gsl_ts_read(client, 0xb0, read_buf, sizeof(read_buf));
	if (read_buf[0] != 0x5a || read_buf[1] != 0x5a ||
	    read_buf[2] != 0x5a || read_buf[3] != 0x5a) {
		printk("#########check mem read 0xb0 = %x %x %x %x #########\n", read_buf[0], read_buf[1], read_buf[2], read_buf[3]);
		init_chip(client, 1);
	}
}

static void average_point(unsigned *px, unsigned *py,
	unsigned prev_x, unsigned prev_y, int id)
{
	unsigned x = *px;
	unsigned y = *py;
	unsigned x_err =0;
	unsigned y_err =0;

	x = (prev_x + x)/2;
	y = (prev_y + y)/2;

	x_err = (x >= prev_x) ? x - prev_x : prev_x - x;
	y_err = (y >= prev_y) ? y - prev_y : prev_y - y;

#define BIG_CHANGE	3	/* 6 */
#define SMALL_CHANGE	1	/* 2 */

	if ( (x_err > BIG_CHANGE && y_err > SMALL_CHANGE) ||
	     (x_err > SMALL_CHANGE && y_err > BIG_CHANGE) )
		return;		/* large change, don't average */

	*px = (x_err > BIG_CHANGE) ? x : prev_x;
	*py = (y_err > BIG_CHANGE) ? y : prev_y;
}

static void release_slots(struct gsl_ts *ts, unsigned mask)
{
	while (mask) {
		int slot = __ffs(mask);

		mask &= ~(1 << slot);
		input_mt_slot(ts->input, slot);
		input_mt_report_slot_state(ts->input,  MT_TOOL_FINGER, 0);
	}
}

static void report_mt_data(struct gsl_ts *ts, unsigned x, unsigned y, unsigned id)
{
	if (x >= SCREEN_MAX_X || y >= SCREEN_MAX_Y)
		return;
	pr_debug("x=%d, y=%d, id=%d\n", x, y, id);

	input_mt_slot(ts->input, id);
	input_mt_report_slot_state(ts->input,  MT_TOOL_FINGER, 1);
	input_report_abs(ts->input, ABS_MT_POSITION_X, x);
	input_report_abs(ts->input, ABS_MT_POSITION_Y, y);
}

static void report_single_data(struct gsl_ts *ts, unsigned x, unsigned y)
{
	if (x >= SCREEN_MAX_X || y >= SCREEN_MAX_Y)
		return;
	input_report_abs(ts->input, ABS_X, x);
	input_report_abs(ts->input, ABS_Y, y);
	input_report_abs(ts->input, ABS_PRESSURE, 1);
	input_report_key(ts->input, BTN_TOUCH, 1);
}

static inline unsigned join_bytes(u8 a, u8 b)
{
	return (a << 8) | b;
}

static void process_gslX680_data(struct gsl_ts *ts)
{
	unsigned touches;
	unsigned id;
	unsigned x, y;
	int i;
	unsigned down_mask = 0;
	unsigned tmp;

	touches = ts->touch_data[0];
	if (touches > MAX_CONTACTS)
		touches = MAX_CONTACTS;

	for (i = 0; i < touches; i++) {
		x = join_bytes(ts->touch_data[4 * i + X_INDEX + 1],
				ts->touch_data[4 * i + X_INDEX]);
		y = join_bytes(ts->touch_data[4 * i + Y_INDEX + 1] & 0xf,
				ts->touch_data[4 * i + Y_INDEX]);
		id = (ts->touch_data[4 * i + ID_INDEX] >> 4) - 1;
		if (id < MAX_CONTACTS) {
			down_mask |= 1 << id;
			if (ts->down_mask & (1 << id))
				average_point(&x, &y, ts->x[id], ts->y[id], id);
			ts->x[id] = x;
			ts->y[id] = y;
			report_mt_data(ts, x, y, id);
			if (!i)
				report_single_data(ts, x, y);
		}
	}
	tmp = ts->down_mask & ~down_mask;
	ts->down_mask = down_mask;
	release_slots(ts, tmp);

	if (!touches) {
		input_report_abs(ts->input, ABS_PRESSURE, 0);
		input_report_key(ts->input, BTN_TOUCH, 0);
	}
	input_sync(ts->input);
}

static void gsl_ts_worker(struct work_struct *work)
{
	int ret;
	struct gsl_ts *ts = container_of(work, struct gsl_ts,work);
	u8 read_buf[4];

	if (ts->is_suspended || !ts->input || !ts->opened) {
		dev_dbg(&ts->client->dev, "TS is waiting\n");
		ts->irq_enable_needed = true;
		return;
	}
	if (!gpiod_get_value(ts->gpiod_irq)) {
		/* No interrrupt pending */
		goto schedule1;
	}

	ret = gsl_ts_read(ts->client, GSL_DATA_REG, ts->touch_data,
			TOUCH_DATA_SIZE);
	if (ret < 0) {
		dev_err(&ts->client->dev, "read failed\n");
		goto schedule;
	}
#ifdef DEBUG
	print_hex_dump_bytes("gslx work:", DUMP_PREFIX_NONE, ts->touch_data, TOUCH_DATA_SIZE);
#endif
	if (ts->touch_data[0] == 0xff) {
		goto schedule;
	}

	ret = gsl_ts_read(ts->client, 0xbc, read_buf, sizeof(read_buf));
	if (ret < 0) {
		dev_err(&ts->client->dev, "read 0xbc failed\n");
		goto schedule;
	}

	if (read_buf[3] == 0 && read_buf[2] == 0 && read_buf[1] == 0 && read_buf[0] == 0) {
		process_gslX680_data(ts);
	} else {
		ret = reset_chip(ts->client);
		if (ret < 0) {
			dev_err(&ts->client->dev, "%s: reset_chip failed\n", __func__);
			goto schedule;
		}
		ret = startup_chip(ts->client);
		if (ret < 0) {
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

/*
 * interrupt callback
 */
static irqreturn_t gsl_ts_irq(int irq, void *dev_id)
{
	struct gsl_ts *ts = dev_id;

	disable_irq_nosync(ts->irq);
	if (!work_pending(&ts->work))
		queue_work(ts->wq, &ts->work);
	return IRQ_HANDLED;
}

static int ts_open(struct input_dev *idev)
{
	struct gsl_ts *ts = input_get_drvdata(idev);
	struct device *dev = &ts->client->dev;
	int rc;

	disable_irq(ts->irq);
	flush_workqueue(ts->wq);
	rc = init_chip(ts->client, 1);

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

static int gsl_input_register_device(struct i2c_client *client, struct gsl_ts *ts)
{
	struct input_dev *idev;
	int rc = 0;

	idev = input_allocate_device();
	if (!idev)
		return -ENOMEM;

	ts->input = idev;
	idev->name = GSLX680_I2C_NAME;
	idev->phys = GSLX680_I2C_NAME;
	idev->id.bustype = BUS_I2C;
	idev->open      = ts_open;
	idev->close     = ts_close;
	idev->dev.parent = &client->dev;
	input_set_drvdata(idev, ts);

	set_bit(EV_ABS, idev->evbit);
	set_bit(EV_KEY, idev->evbit);
	set_bit(EV_SYN, idev->evbit);

	set_bit(INPUT_PROP_DIRECT, idev->propbit);
	set_bit(BTN_TOUCH, idev->keybit);

	input_set_abs_params(idev, ABS_X, 0, SCREEN_MAX_X - 1, 0, 0);
	input_set_abs_params(idev, ABS_Y, 0, SCREEN_MAX_Y - 1, 0, 0);
	input_set_abs_params(idev, ABS_PRESSURE, 0, 1, 0 , 0);

	input_mt_init_slots(idev, MAX_CONTACTS, 0);
	input_set_abs_params(idev, ABS_MT_POSITION_X, 0, SCREEN_MAX_X - 1, 0, 0);
	input_set_abs_params(idev, ABS_MT_POSITION_Y, 0, SCREEN_MAX_Y - 1, 0, 0);
	input_set_abs_params(idev, ABS_MT_TRACKING_ID, 0, MAX_CONTACTS - 1, 0, 0);

	rc = input_register_device(idev);
	if (rc)
		input_free_device(idev);
	return rc;
}

static void gp_mt_set_nice_work(struct work_struct *work)
{
	pr_debug("[%s:%d]\n", __FUNCTION__, __LINE__);
	set_user_nice(current, -20);
}

static int gslx680_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int rc;
	struct gsl_ts *ts;
	struct device *dev = &client->dev;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(dev, "I2C functionality not supported\n");
		return -ENODEV;
	}

	ts = kzalloc(sizeof(*ts), GFP_KERNEL);
	if (!ts)
		return -ENOMEM;

	ts->client = client;
	i2c_set_clientdata(client, ts);

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
	gslX680_shutdown(ts, 1);
	if (rc < 0) {
		dev_err(dev, "reset failed\n");
		goto err_reset_chip;
	}

	ts->wq = create_singlethread_workqueue("kworkqueue_ts");
	if (!ts->wq) {
		dev_err(&client->dev, "Could not create workqueue\n");
		rc = -ENOMEM;
		goto error_wq_create;
	}
	INIT_WORK(&ts->work, gsl_ts_worker);
	INIT_WORK(&ts->mt_set_nice_work, gp_mt_set_nice_work);
	queue_work(ts->wq, &ts->mt_set_nice_work);

	rc = request_irq(client->irq, gsl_ts_irq,
			gpiod_is_active_low(ts->gpiod_irq) ? IRQF_TRIGGER_LOW :
					IRQF_TRIGGER_HIGH, "gslx680", ts);
	if (rc < 0) {
		dev_err(dev, "gslx680_ts probe: request irq failed\n");
		goto err_request_irq;
	}

	rc = gsl_input_register_device(client, ts);
	if (rc < 0) {
		dev_err(dev, "GSLX680 init failed\n");
		goto err_ts_init;
	}
	return 0;

err_ts_init:
	free_irq(ts->irq, ts);
err_request_irq:
	destroy_workqueue(ts->wq);
error_wq_create:
err_reset_chip:
err_wakeup:
	input_free_device(ts->input);
	kfree(ts);
	return rc;
}

static int gslx680_ts_remove(struct i2c_client *client)
{
	struct gsl_ts *ts = i2c_get_clientdata(client);

	disable_irq(ts->irq);
	cancel_work_sync(&ts->work);
	free_irq(ts->irq, ts);
	input_unregister_device(ts->input);
	input_free_device(ts->input);
	destroy_workqueue(ts->wq);

	gslX680_shutdown(ts, 1);
	kfree(ts);
	return 0;
}

static int gsl_ts_suspend(struct device *dev)
{
	struct gsl_ts *ts = dev_get_drvdata(dev);

	pr_info("%s: Enter\n", __func__);
	ts->is_suspended = true;

	disable_irq(ts->irq);

	reset_chip(ts->client);
	gslX680_shutdown(ts, 1);
	msleep(10);

	return 0;
}

static int gsl_ts_resume(struct device *dev)
{
	struct gsl_ts *ts = dev_get_drvdata(dev);

	pr_info("%s: Enter\n", __func__);

	gslX680_shutdown(ts, 0);
	msleep(20);
	reset_chip(ts->client);
	startup_chip(ts->client);
	check_mem_data(ts, ts->client);

	enable_irq(ts->irq);
	ts->is_suspended = false;

	return 0;
}

static const struct i2c_device_id gslx680_ts_id[] = {
	{ GSLX680_I2C_NAME, 0 },
	{}
};

MODULE_DEVICE_TABLE(i2c, gslx680_ts_id);

static const struct dev_pm_ops gslx680_pm_ops = {
#ifdef CONFIG_PM_SLEEP
	.suspend = gsl_ts_suspend,
	.resume = gsl_ts_resume,
#endif
};

static struct i2c_driver gslx680_ts_driver = {
	.class	  = I2C_CLASS_HWMON,
	.probe	  = gslx680_ts_probe,
	.remove	 = gslx680_ts_remove,
	.id_table   = gslx680_ts_id,
	.driver = {
		.name   = GSLX680_I2C_NAME,
		.owner  = THIS_MODULE,
		.pm	= &gslx680_pm_ops,
	},
};

static int gslx680_ts_init(void)
{
	return i2c_add_driver(&gslx680_ts_driver);
}

static void gslx680_ts_exit(void)
{
	i2c_del_driver(&gslx680_ts_driver);
}

late_initcall(gslx680_ts_init);
module_exit(gslx680_ts_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GSLX680 touchscreen controller driver");
MODULE_AUTHOR("Joe Burmeister, joe.a.burmeister@gmail.com");
MODULE_ALIAS("platform:gsl_ts");
