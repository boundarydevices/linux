/* drivers/input/touchscreen/ektf2k.c - ELAN EKTF2136 touchscreen driver
 *
 * Copyright (C) 2011 Elan Microelectronics Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/* #define DEBUG */

#include <linux/module.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/device.h>
#include <linux/jiffies.h>

/* for linux 2.6.36.3 */
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define FINGERPKT_SIZE		40
#define FINGERPKT_CKSUMLEN	34

#define FINGER_NUM		10
#define IDX_FINGER		0x02
#define FINGER_ID		0x08

#define PWR_STATE_DEEP_SLEEP	0
#define PWR_STATE_NORMAL	1
#define PWR_STATE_MASK		BIT(3)

#define CMD_S_PKT		0x52
#define CMD_R_PKT		0x53
#define CMD_W_PKT		0x54
#define HELLO_PKT		0x55
#define NORMAL_PKT		0x62
#define TOUCH_REPORT		0x63

#include <linux/ektf2k.h>

#define REPORTED_TOUCH_RANGE	1024

struct elan_ktf2k_ts_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	int (*power)(int on);
	struct early_suspend early_suspend;
	int intr_gpio;
	int fw_ver;
	int suspend;
	int interrupt_rtn_active;
	uint8_t read_buffer[FINGER_NUM * FINGERPKT_SIZE + 4];
};

static struct elan_ktf2k_ts_data *private_ts;

#ifdef CONFIG_HAS_EARLYSUSPEND
static void elan_ktf2k_ts_early_suspend(struct early_suspend *h);
static void elan_ktf2k_ts_late_resume(struct early_suspend *h);
#endif

static ssize_t elan_ktf2k_gpio_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;
	struct elan_ktf2k_ts_data *ts = private_ts;

	ret = gpio_get_value(ts->intr_gpio);
	sprintf(buf, "touchscreen_int(gp=%d)=%d\n", ts->intr_gpio, ret);
	ret = strlen(buf);
	return ret;
}

static DEVICE_ATTR(gpio, S_IRUGO, elan_ktf2k_gpio_show, NULL);

static ssize_t elan_ktf2k_vendor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	struct elan_ktf2k_ts_data *ts = private_ts;

	sprintf(buf, "ELAN_KTF2K 0x%x\n", ts->fw_ver);
	ret = strlen(buf);
	return ret;
}

static DEVICE_ATTR(vendor, S_IRUGO, elan_ktf2k_vendor_show, NULL);

static struct kobject *android_touch_kobj;

static int elan_ktf2k_touch_sysfs_init(void)
{
	int ret ;

	android_touch_kobj = kobject_create_and_add("android_touch", NULL);
	if (android_touch_kobj == NULL) {
		printk(KERN_ERR "[elan]%s: subsystem_register failed\n",
		       __func__);
		ret = -ENOMEM;
		return ret;
	}
	ret = sysfs_create_file(android_touch_kobj, &dev_attr_gpio.attr);
	if (ret) {
		printk(KERN_ERR "[elan]%s: sysfs_create_file failed\n",
		       __func__);
		return ret;
	}
	ret = sysfs_create_file(android_touch_kobj, &dev_attr_vendor.attr);
	if (ret) {
		printk(KERN_ERR "[elan]%s: sysfs_create_group failed\n",
		       __func__);
		return ret;
	}
	return 0 ;
}

static void elan_touch_sysfs_deinit(void)
{
	sysfs_remove_file(android_touch_kobj, &dev_attr_vendor.attr);
	sysfs_remove_file(android_touch_kobj, &dev_attr_gpio.attr);
	kobject_del(android_touch_kobj);
}

static int elan_ktf2k_ts_poll(struct i2c_client *client)
{
	struct elan_ktf2k_ts_data *ts = i2c_get_clientdata(client);
	int status = 0, retry = 10;

	do {
		status = gpio_get_value(ts->intr_gpio);
		dev_dbg(&client->dev, "%s: status = %d\n", __func__, status);
		retry--;
		mdelay(20);
	} while (status == 1 && retry > 0);

	if (status)
		dev_err(&client->dev, "[elan]%s: timed out\n", __func__);
	else
		dev_dbg(&client->dev, "[elan]%s: poll interrupt status %s\n",
			__func__, status == 1 ? "high" : "low");
	return (status == 0 ? 0 : -ETIMEDOUT);
}

static int elan_ktf2k_ts_get_data(struct i2c_client *client, uint8_t *cmd,
			uint8_t *buf, size_t size)
{
	int rc;

	dev_dbg(&client->dev, "[elan]%s: enter\n", __func__);

	if (buf == NULL)
		return -EINVAL;

	if ((i2c_master_send(client, cmd, 4)) != 4) {
		dev_err(&client->dev,
			"[elan]%s: i2c_master_send failed\n", __func__);
		return -EINVAL;
	}

	rc = elan_ktf2k_ts_poll(client);
	if (rc < 0) {
		dev_err(&client->dev,"%s: poll failed\n", __func__);
		return -EINVAL;
	} else {
		int sz = i2c_master_recv(client, buf, size);
		if (sz != size || buf[0] != CMD_S_PKT) {
			dev_err(&client->dev,"%s: sz=%d size=%d\n", __func__, sz, size);
			return -EINVAL;
		}
	}

	return 0;
}

static int __hello_packet_handler(struct i2c_client *client)
{
	int rc;
	uint8_t buf_recv[4] = { 0 };

	rc = elan_ktf2k_ts_poll(client);
	if (rc < 0)
		dev_err(&client->dev,
			"[elan] %s: timeout waiting for interrupt\n",
			__func__);

	rc = i2c_master_recv(client, buf_recv, 4);
	return rc;
}

static int __fw_packet_handler(struct i2c_client *client)
{
	struct elan_ktf2k_ts_data *ts = i2c_get_clientdata(client);
	int rc;
	int major, minor;
	uint8_t cmd[] = {CMD_R_PKT, 0x00, 0x00, 0x01};
	uint8_t buf_recv[4] = {0};

	rc = elan_ktf2k_ts_get_data(client, cmd, buf_recv, 4);
	if (rc < 0)
		return rc;

	major = ((buf_recv[1] & 0x0f) << 4) | ((buf_recv[2] & 0xf0) >> 4);
	minor = ((buf_recv[2] & 0x0f) << 4) | ((buf_recv[3] & 0xf0) >> 4);
	ts->fw_ver = major << 8 | minor;
	pr_info("[elan] %s: firmware version: 0x%x\n", __func__, ts->fw_ver);
	return 0;
}

static inline int elan_ktf2k_ts_parse_xy(uint8_t *data,
			uint16_t *x, uint16_t *y)
{
	*x = *y = 0;

	*x = (data[0] & 0xf0);
	*x <<= 4;
	*x |= data[1];

	*y = (data[0] & 0x0f);
	*y <<= 8;
	*y |= data[2];

	return 0;
}

static int elan_ktf2k_ts_setup(struct i2c_client *client)
{
	int rc;

	rc = __hello_packet_handler(client);
	if (rc < 0)
		goto hand_shake_failed;
	dev_dbg(&client->dev, "[elan] %s: hello packet got.\n", __func__);

	rc = __fw_packet_handler(client);
	if (rc < 0)
		goto hand_shake_failed;
	dev_dbg(&client->dev, "[elan] %s: firmware checking done.\n", __func__);

hand_shake_failed:
	return rc;
}

static int elan_ktf2k_ts_set_power_state(struct i2c_client *client, int state)
{
	uint8_t cmd[] = {CMD_W_PKT, 0x50, 0x00, 0x01};

	dev_err(&client->dev, "[elan] %s: enter\n", __func__);

	cmd[1] |= (state << 3);

	dev_dbg(&client->dev,
		"[elan] dump cmd: %02x, %02x, %02x, %02x\n",
		cmd[0], cmd[1], cmd[2], cmd[3]);

	if ((i2c_master_send(client, cmd, sizeof(cmd))) != sizeof(cmd)) {
		dev_err(&client->dev,
			"[elan] %s: i2c_master_send failed\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static int elan_ktf2k_ts_get_power_state(struct i2c_client *client)
{
	int rc = 0;
	uint8_t cmd[] = {CMD_R_PKT, 0x50, 0x00, 0x01};
	uint8_t buf[4], power_state;

	rc = elan_ktf2k_ts_get_data(client, cmd, buf, 4);
	if (rc)
		return rc;

	power_state = buf[1];
	dev_dbg(&client->dev, "[elan] dump repsponse: %0x\n", power_state);
	power_state = (power_state & PWR_STATE_MASK) >> 3;
	dev_dbg(&client->dev, "[elan] power state = %s\n",
		power_state == PWR_STATE_DEEP_SLEEP ?
		"Deep Sleep" : "Normal/Idle");

	return power_state;
}

static uint8_t checksum(uint8_t const *buf, unsigned len)
{
	uint8_t sum = 0 ;
	while (0 < len--)
		sum += *buf++ ;
	return sum ;
}

static void report_touch(struct input_dev *idev, unsigned x, unsigned y)
{
#ifndef TOUCHSCREEN_EKTF2K_SINGLE_TOUCH
	input_event(idev, EV_ABS, ABS_MT_POSITION_X, x);
	input_event(idev, EV_ABS, ABS_MT_POSITION_Y, y);
	input_event(idev, EV_ABS, ABS_MT_TOUCH_MAJOR, 1);
	input_mt_sync(idev);
#else
	input_event(idev, EV_ABS, ABS_X, x);
	input_event(idev, EV_ABS, ABS_Y, y);
	input_event(idev, EV_ABS, ABS_PRESSURE, 1);
	input_report_key(idev, BTN_TOUCH, 1);
#endif
}

static void process_fingers(struct elan_ktf2k_ts_data *ts, uint8_t *pkt)
{
	struct input_dev *idev = ts->input_dev;
	unsigned count = pkt[2]&0x0f;
	if (0 == count) {
		dev_dbg(&ts->client->dev, "[elan] release\n");
#ifndef TOUCHSCREEN_EKTF2K_SINGLE_TOUCH
		input_report_abs(idev, ABS_MT_TOUCH_MAJOR, 0);
		input_report_abs(idev, ABS_MT_WIDTH_MAJOR, 0);
		input_mt_sync(idev);
#else
		input_event(idev, EV_ABS, ABS_PRESSURE, 0);
		input_report_key(idev, BTN_TOUCH, 0);
#endif
	} else {
		unsigned const fbits = pkt[1]|((pkt[2]&0x30)<<4);
		int i ;
		unsigned mask = 1 ;
		dev_dbg(&ts->client->dev, "[elan] touch %u, bits %x\n",
			count, fbits);
		pkt += 3 ;
		for (i = 0; i < 10; i++, mask <<= 1, pkt += 3) {
			if (fbits & mask) {
				struct elan_ktf2k_i2c_platform_data *pdata
					= ts->client->dev.platform_data;
				unsigned x = pkt[1]|((pkt[0]&0xf0)<<4);
				unsigned y = pkt[2]|((pkt[0]&0x0f)<<8);
				if (x < pdata->abs_x_min)
					x = pdata->abs_x_min ;
				else if (x > pdata->abs_x_max)
					x = pdata->abs_x_max ;
				if (y < pdata->abs_y_min)
					y = pdata->abs_y_min ;
				else if (y > pdata->abs_y_max)
					y = pdata->abs_y_max ;

				x = ((x-pdata->abs_x_min)*REPORTED_TOUCH_RANGE)
				    /(pdata->abs_x_max-pdata->abs_x_min+1);
				y = ((y-pdata->abs_y_min)*REPORTED_TOUCH_RANGE)
				    /(pdata->abs_y_max-pdata->abs_y_min+1);
				report_touch(idev, x, y);
				dev_dbg(&ts->client->dev, "%u:%u\n", x, y);
			}
		}
	}
	input_sync(idev);
}

int scan_for_valid_data(struct elan_ktf2k_ts_data *ts, uint8_t **pnext, int numread)
{
	uint8_t *next = *pnext;
	int rem = sizeof(ts->read_buffer) - numread;

	next -= 4;
	numread += 4;
	for (;;) {
		int min = 5;

		if ((numread > 4) && (next[4] == NORMAL_PKT))
			min = FINGERPKT_SIZE + 4;
		while (numread < min) {
			int more_read, needed;

			if (next > ts->read_buffer) {
				memcpy(ts->read_buffer, next, numread);
				next = ts->read_buffer;
			}
			if ((rem < 0) && (min != FINGERPKT_SIZE + 4)) {
				pr_info("%s: giving up\n", __func__);
				msleep(100);
				return 0;
			}
			needed = 4 + FINGERPKT_SIZE - numread;
			more_read = i2c_master_recv(ts->client, next + numread,
					needed);
			pr_info("%s: read %d, needed=%d\n", __func__,
					more_read, needed);
			if (more_read <= 0)
				return 0;
			numread += more_read;
			rem -= more_read;
		}
		if (min == FINGERPKT_SIZE + 4) {
			if (checksum(next + 4, FINGERPKT_CKSUMLEN)
					== next[4 + FINGERPKT_CKSUMLEN]) {
				if ((next < ts->read_buffer)
						|| (next[0] != TOUCH_REPORT)) {
					next += 4;
					numread -= 4;
				}
				*pnext = next;
				return numread;
			}
		}
		next++;
		numread--;
	}
}

static irqreturn_t elan_ktf2k_ts_irq_handler(int irq, void *dev_id)
{
	struct elan_ktf2k_ts_data *ts = dev_id;
	int max = FINGERPKT_SIZE + 4;
	int loop_max = 10;

	ts->interrupt_rtn_active = 1;

	while (0 == gpio_get_value(ts->intr_gpio)) {
		int numread ;
		uint8_t *next = ts->read_buffer;

		numread = i2c_master_recv(ts->client, next, max);
		if (numread <= 0)
			continue;
#if 0
		char asciibuf[128];
		char *out = asciibuf;
		int i ;
		int sum = 0;
		for (i = 0; i < numread; i++) {
			int n = sprintf(out, "%02x ", buf[i]);
			out += n;
			sum += n;
			if (sum > (sizeof(asciibuf) - 5))
				break;
		}
		printk(KERN_ERR "[elan] gpio %d, value %d, %s\n",
				ts->intr_gpio, gpio_get_value(ts->intr_gpio),
				asciibuf);
#endif
		while (0 < numread) {
			uint8_t const cmd = *next ;
			if (NORMAL_PKT == cmd) {
				while (numread < FINGERPKT_SIZE) {
					int more_read;

					memcpy(ts->read_buffer, next, numread);
					next = ts->read_buffer;
					more_read = i2c_master_recv(ts->client, next + numread, FINGERPKT_SIZE - numread);
					if (more_read <= 0) {
						dev_err(&ts->client->dev, "short pkt %u\n", numread);
						goto give_up;
					}
					numread += more_read;
				}
				if (checksum(next, FINGERPKT_CKSUMLEN)
						== next[FINGERPKT_CKSUMLEN]) {
					process_fingers(ts, next);
				} else {
					dev_err(&ts->client->dev, "bad checksum\n");
					goto search;
				}
				next += FINGERPKT_SIZE;
				numread -= FINGERPKT_SIZE;
				max -= FINGERPKT_SIZE;
				if (max <= 0)
					max = FINGERPKT_SIZE + 4;
			} else if (TOUCH_REPORT == cmd) {
				unsigned reports = next[1];
				max = ((reports < FINGER_NUM) ? reports : FINGER_NUM) * FINGERPKT_SIZE;
				next += 4 ;
				numread -= 4 ;
			} else {
				dev_err(&ts->client->dev,
					"[elan] Unknown packet %02x,"
					" length %u\n", *next, numread);
search:
				loop_max = 1;
				if (ts->suspend)
					break;
				numread = scan_for_valid_data(ts, &next, numread);
			}
		}
give_up:
		if (--loop_max == 0)
			break;
		if (ts->suspend)
			break;
	}
	ts->interrupt_rtn_active = 0;
	return IRQ_HANDLED;
}

static int elan_ktf2k_ts_register_interrupt(struct i2c_client *client)
{
	struct elan_ktf2k_ts_data *ts = i2c_get_clientdata(client);
	int err = 0;

	err = request_threaded_irq(client->irq, NULL, elan_ktf2k_ts_irq_handler,
			IRQF_TRIGGER_LOW | IRQF_ONESHOT, client->name, ts);
	if (err)
		dev_err(&client->dev, "[elan] %s: request_irq %d failed\n",
				__func__, client->irq);

	return err;
}

static int elan_ktf2k_ts_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int err = 0;
	struct elan_ktf2k_i2c_platform_data *pdata;
	struct elan_ktf2k_ts_data *ts;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk(KERN_ERR "[elan] %s: "
			"i2c check functionality error\n", __func__);
		err = -ENODEV;
		goto err_check_functionality_failed;
	}

	ts = kzalloc(sizeof(struct elan_ktf2k_ts_data), GFP_KERNEL);
	if (ts == NULL) {
		printk(KERN_ERR "[elan] %s: "
			" allocate elan_ktf2k_ts_data failed\n", __func__);
		err = -ENOMEM;
		goto err_alloc_data_failed;
	}

	ts->client = client;
	i2c_set_clientdata(client, ts);
	pdata = client->dev.platform_data;

	if (likely(pdata != NULL)) {
		ts->power = pdata->power;
		ts->intr_gpio = pdata->intr_gpio;
	}

	if (ts->power)
		ts->power(1);

	err = elan_ktf2k_ts_setup(client);
	if (err < 0) {
		printk(KERN_INFO "No Elan chip inside\n");
		err = -ENODEV;
		goto err_detect_failed;
	}

	ts->input_dev = input_allocate_device();
	if (ts->input_dev == NULL) {
		err = -ENOMEM;
		dev_err(&client->dev,
			"[elan] Failed to allocate input device\n");
		goto err_input_dev_alloc_failed;
	}
	ts->input_dev->name = "elan-touchscreen";   /* for andorid2.2 Froyo */

	set_bit(BTN_TOUCH, ts->input_dev->keybit);

#ifdef TOUCHSCREEN_EKTF2K_SINGLE_TOUCH
	input_set_abs_params(ts->input_dev, ABS_X, 0, REPORTED_TOUCH_RANGE-1, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_Y, 0, REPORTED_TOUCH_RANGE-1, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_PRESSURE, 0, 255, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_TOOL_WIDTH, 0, 255, 0, 0);
#else
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, REPORTED_TOUCH_RANGE-1, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, REPORTED_TOUCH_RANGE-1, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0, FINGER_NUM, 0, 0);
#endif
	__set_bit(EV_ABS, ts->input_dev->evbit);
	__set_bit(EV_SYN, ts->input_dev->evbit);
	__set_bit(EV_KEY, ts->input_dev->evbit);
	__set_bit(BTN_TOUCH, ts->input_dev->keybit);

	err = input_register_device(ts->input_dev);
	if (err) {
		dev_err(&client->dev,
			"[elan]%s: unable to register %s input device\n",
			__func__, ts->input_dev->name);
		goto err_input_register_device_failed;
	}

	elan_ktf2k_ts_set_power_state(client, PWR_STATE_NORMAL);
	elan_ktf2k_ts_register_interrupt(ts->client);

	if (gpio_get_value(ts->intr_gpio) == 0) {
		printk(KERN_INFO "[elan]%s: handle missed interrupt\n",
				__func__);
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 1;
	ts->early_suspend.suspend = elan_ktf2k_ts_early_suspend;
	ts->early_suspend.resume = elan_ktf2k_ts_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif

	private_ts = ts;

	elan_ktf2k_touch_sysfs_init();

	dev_info(&client->dev,
		 "[elan] Start touchscreen %s in interrupt mode\n",
		 ts->input_dev->name);

	return 0;

err_input_register_device_failed:
	if (ts->input_dev)
		input_free_device(ts->input_dev);

err_input_dev_alloc_failed:
err_detect_failed:
	kfree(ts);

err_alloc_data_failed:
err_check_functionality_failed:

	return err;
}

static int elan_ktf2k_ts_remove(struct i2c_client *client)
{
	struct elan_ktf2k_ts_data *ts = i2c_get_clientdata(client);

	elan_touch_sysfs_deinit();

	unregister_early_suspend(&ts->early_suspend);
	free_irq(client->irq, ts);

	input_unregister_device(ts->input_dev);
	kfree(ts);

	return 0;
}

static int elan_ktf2k_ts_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct elan_ktf2k_ts_data *ts = i2c_get_clientdata(client);
	int i;

	ts->suspend = 1;
	dev_info(&client->dev, "[elan] %s: enter\n", __func__);
	disable_irq(client->irq);
	for (i = 0; i < 10 ; i++) {
		if (!ts->interrupt_rtn_active)
			break;
		msleep(1);
	}
	elan_ktf2k_ts_set_power_state(client, PWR_STATE_DEEP_SLEEP);
	return 0;
}

static int elan_ktf2k_ts_resume(struct i2c_client *client)
{
	struct elan_ktf2k_ts_data *ts = i2c_get_clientdata(client);
	int rc = 0, retry = 5;

	dev_info(&client->dev, "[elan] %s: enter\n", __func__);

	ts->suspend = 0;
	do {
		rc = elan_ktf2k_ts_set_power_state(client, PWR_STATE_NORMAL);
		rc = elan_ktf2k_ts_get_power_state(client);
		if (rc != PWR_STATE_NORMAL)
			printk(KERN_ERR "[elan] %s: wake up tp failed! err = %d\n",
				__func__, rc);
		else
			break;
	} while (--retry);

	enable_irq(client->irq);
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void elan_ktf2k_ts_early_suspend(struct early_suspend *h)
{
	struct elan_ktf2k_ts_data *ts;
	ts = container_of(h, struct elan_ktf2k_ts_data, early_suspend);
	elan_ktf2k_ts_suspend(ts->client, PMSG_SUSPEND);
}

static void elan_ktf2k_ts_late_resume(struct early_suspend *h)
{
	struct elan_ktf2k_ts_data *ts;
	ts = container_of(h, struct elan_ktf2k_ts_data, early_suspend);
	elan_ktf2k_ts_resume(ts->client);
}
#endif

static const struct i2c_device_id elan_ktf2k_ts_id[] = {
	{ ELAN_KTF2K_NAME, 0 },
	{ }
};

static struct i2c_driver ektf2k_ts_driver = {
	.probe		= elan_ktf2k_ts_probe,
	.remove		= elan_ktf2k_ts_remove,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend	= elan_ktf2k_ts_suspend,
	.resume		= elan_ktf2k_ts_resume,
#endif
	.id_table	= elan_ktf2k_ts_id,
	.driver		= {
		.name = ELAN_KTF2K_NAME,
	},
};

static int __devinit elan_ktf2k_ts_init(void)
{
	printk(KERN_INFO "[elan] %s\n", __func__);
	return i2c_add_driver(&ektf2k_ts_driver);
}

static void __exit elan_ktf2k_ts_exit(void)
{
	i2c_del_driver(&ektf2k_ts_driver);
	return;
}

module_init(elan_ktf2k_ts_init);
module_exit(elan_ktf2k_ts_exit);

MODULE_DESCRIPTION("ELAN EKTF2K Touchscreen Driver");
MODULE_LICENSE("GPL");
