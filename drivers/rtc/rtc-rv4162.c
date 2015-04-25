/*
 * An rtc/i2c driver for the Micro Crystal RV-4162 Real-time clock
 *
 * Copyright 2013 Boundary Devices
 *
 * Author: Eric Nelson <eric.nelson@boundarydevices.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/bcd.h>
#include <linux/rtc.h>

/* Registers */
#define RV4162_REG_SECS		0x01
#define RV4162_SECFLAG_ST 0x80

#define RV4162_REG_FLAGS	0x0f
#define RV4162_FLAG_OF	4

static struct i2c_driver rv4162_driver;

/* block read */
static int rv4162_i2c_read_regs(struct i2c_client *client, u8 reg, u8 buf[],
		unsigned len)
{
	u8 reg_addr[1] = { reg };
	struct i2c_msg msgs[2] = {
		{
			.addr = client->addr,
			.len = sizeof(reg_addr),
			.buf = reg_addr
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = buf
		}
	};
	int ret;

	WARN_ON(reg > 0x0f);
	WARN_ON(reg + len > 0x10);

	ret = i2c_transfer(client->adapter, msgs, 2);
	if (ret > 0)
		ret = 0;
	return ret;
}

/* block write */
static int rv4162_i2c_set_regs(struct i2c_client *client, u8 reg,
		u8 const buf[], unsigned len)
{
	u8 i2c_buf[20];
	struct i2c_msg msgs[1] = {
		{
			.addr = client->addr,
			.len = len + 1,
			.buf = i2c_buf
		}
	};
	int ret;

	WARN_ON(reg > 0x0f);
	WARN_ON(reg + len > 0x10);

	i2c_buf[0] = reg;
	memcpy(&i2c_buf[1], &buf[0], len);

	ret = i2c_transfer(client->adapter, msgs, 1);
	if (ret > 0)
		ret = 0;
	return ret;
}

static int rv4162_rtc_proc(struct device *dev, struct seq_file *seq)
{
	struct i2c_client *const client = to_i2c_client(dev);
	int watchdog, freq_comp, month;

	watchdog = i2c_smbus_read_byte_data(client, 0x09);
	freq_comp = i2c_smbus_read_byte_data(client, 0x08);
	month = i2c_smbus_read_byte_data(client, 0x0a);

	seq_printf(seq, "watchdog reg\t: %x\n", watchdog);
	seq_printf(seq, "freq_comp reg\t: %x\n", freq_comp);
	seq_printf(seq, "alarm month reg\t: %x\n", month);
	return 0;
}

static int rv4162_i2c_read_time(struct i2c_client *client, struct rtc_time *tm)
{
        u8 addr = 0;
	u8 buf[8];
	int err;
	struct i2c_msg msgs[] = {
		{client->addr, 0, 1, &addr},
		{client->addr, I2C_M_RD, sizeof(buf), buf},
	};
	memset(tm,0,sizeof(*tm));
	if (2 == (err=i2c_transfer(client->adapter, &msgs[0], 2))) {
		tm->tm_sec  = bcd2bin(buf[1]&0x7f);
		tm->tm_min  = bcd2bin(buf[2]&0x7f);
		tm->tm_hour = bcd2bin(buf[3]);
		tm->tm_mon  = bcd2bin(buf[6]&0x1F)-1;
		tm->tm_mday = bcd2bin(buf[5]);
		tm->tm_wday = (buf[4] - 1) & 7;
		tm->tm_yday = -1;
		tm->tm_year = bcd2bin(buf[7])+(100*(buf[6]>>6));
		dev_dbg(&client->dev, "%s: read time: %04u-%02u-%02u %02u:%02u:%02u\n",
			__func__,
			tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
			tm->tm_hour, tm->tm_min, tm->tm_sec);

		dev_dbg(&client->dev, "%s: %02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x\n",
			__func__,
			buf[0], buf[1], buf[2], buf[3], 
			buf[4], buf[5], buf[6], buf[7]);
		return 0;
	}
	else {
		dev_err(&client->dev, "%s: error %d reading time\n", 
			__func__, err);
		return err;
	}
}

static int rv4162_i2c_set_time(struct i2c_client *client, struct rtc_time *tm)
{
	u8 buf[9];
	int err;
	struct i2c_msg msg = {
		client->addr, 0, sizeof(buf), buf
	};
	buf[0] = 0;
	buf[1] = 0;
	buf[2] = bin2bcd(tm->tm_sec);
	buf[3] = bin2bcd(tm->tm_min);
	buf[4] = bin2bcd(tm->tm_hour);
	buf[5] = tm->tm_wday+1;
	buf[6] = bin2bcd(tm->tm_mday);
	buf[7] = bin2bcd(tm->tm_mon+1) | ((tm->tm_year/100)<<6);
	buf[8] = bin2bcd(tm->tm_year%100);
	if (1 == (err=i2c_transfer(client->adapter, &msg, 1))) {
		dev_dbg(&client->dev, "%s: %04u-%02u-%02u %02u:%02u:%02u\n",
			__func__,
			tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
			tm->tm_hour, tm->tm_min, tm->tm_sec);
		dev_dbg(&client->dev, "%s: %02x:%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x\n",
			__func__,
			buf[0], buf[1], buf[2], buf[3], 
			buf[4], buf[5], buf[6], buf[7], buf[8]);
		return 0;
	}
	else {
		dev_err(&client->dev, "%s: error %d saving time\n", 
			__func__, err);
		return err;
	}
}

static int rv4162_i2c_read_alarm(struct i2c_client *client,
		struct rtc_wkalrm *alarm)
{
	unsigned long rtc_secs, alarm_secs;
	struct rtc_time rtc_tm;
	int err;
	struct rtc_time *const tm = &alarm->time;
	u8 regs[16];

	err = rv4162_i2c_read_regs(client, 0x0a, &regs[0x0a], 5);
	if (err < 0) {
		dev_err(&client->dev, "%s: reading alarm section failed, %d\n",
			__func__, err);
		return err;
	}

	tm->tm_sec = bcd2bin(regs[0x0e] & 0x7f);
	tm->tm_min = bcd2bin(regs[0x0d] & 0x7f);
	tm->tm_hour = bcd2bin(regs[0x0c] & 0x3f);
	tm->tm_mday = bcd2bin(regs[0x0b] & 0x3f);
	tm->tm_mon = bcd2bin(regs[0x0a] & 0x1f) - 1;
	tm->tm_wday = 0;

	alarm->enabled = !!(regs[0x0a] & 0x80);

	rtc_secs = 0;
	alarm_secs = 0;
	rtc_tm.tm_year = 100;
	err = rv4162_i2c_read_time(client, &rtc_tm);
	/* The alarm doesn't store the year so get it from the rtc section */
	tm->tm_year = rtc_tm.tm_year;

	if (!err)
		rtc_tm_to_time(&rtc_tm, &rtc_secs);

	rtc_tm_to_time(tm, &alarm_secs);
	if (alarm->enabled) {
		if (alarm_secs < rtc_secs) {
			/* enabled alarm needs to move to future date */
			tm->tm_year++;
		}
	} else {
		if (alarm_secs > rtc_secs) {
			/* disabled alarm needs to move to past date */
			tm->tm_year--;
		}
	}
	return 0;
}

static int rv4162_i2c_set_alarm(struct i2c_client *client,
		struct rtc_wkalrm *alarm)
{
	struct rtc_time *alarm_tm = &alarm->time;
	u8 regs[16];
	unsigned long rtc_secs, alarm_secs;
	struct rtc_time rtc_tm;
	int err, enable;

	err = rv4162_i2c_read_time(client, &rtc_tm);
	if (err)
		return err;
	err = rtc_tm_to_time(&rtc_tm, &rtc_secs);
	if (err)
		return err;
	err = rtc_tm_to_time(alarm_tm, &alarm_secs);
	if (err)
		return err;

	/* If the alarm time is before the current time disable the alarm */
	if (!alarm->enabled || alarm_secs <= rtc_secs) {
		enable = 0x00;
		dev_info(&client->dev, "%s: alarm in the past\n", __func__);
	} else {
		enable = 0x80;
	}

	/* Program the alarm and enable it for each setting */
	regs[0x0a] = bin2bcd(alarm_tm->tm_mon + 1);
	regs[0x0b] = bin2bcd(alarm_tm->tm_mday);
	regs[0x0c] = bin2bcd(alarm_tm->tm_hour);
	regs[0x0d] = bin2bcd(alarm_tm->tm_min);
	regs[0x0e] = bin2bcd(alarm_tm->tm_sec);

	/* write ALARM registers */
	err = rv4162_i2c_set_regs(client, 0x0a, &regs[0x0a], 5);
	if (err < 0) {
		dev_err(&client->dev, "%s: writing ALARM section failed\n",
			__func__);
		return err;
	}
	regs[0x0a] |= enable;
	err = i2c_smbus_write_byte_data(client, 0x0a, regs[0x0a]);
	if (err < 0) {
		dev_err(&client->dev, "%s: writing month failed\n", __func__);
		return err;
	}
	return 0;
}

static int rv4162_rtc_toggle_alarm(struct i2c_client *client, int enable)
{
	int month = i2c_smbus_read_byte_data(client, 0x0a);

	if (month < 0) {
		dev_err(&client->dev, "%s: reading month failed\n", __func__);
		return month;
	}

	if (enable)
		month |= 0x80;
	else
		month &= ~0x80;

	month = i2c_smbus_write_byte_data(client, 0x0a, month);
	if (month < 0) {
		dev_err(&client->dev, "%s: writing month failed\n", __func__);
		return month;
	}
	return 0;
}

static irqreturn_t rv4162_rtc_interrupt(int irq, void *data)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(100);
	struct i2c_client *client = data;
	struct rtc_device *rtc = i2c_get_clientdata(client);
	int handled = 0;
	int err;
	int flags;

	while (1) {
		flags = i2c_smbus_read_byte_data(client, 0x0f);
		if (flags >= 0)
			break;

		if (time_after(jiffies, timeout)) {
			dev_err(&client->dev, "%s: reading flags failed\n",
				__func__);
			return IRQ_NONE;
		}
	}

	if (flags & 0x40) {
		dev_dbg(&client->dev, "alarm!\n");

		rtc_update_irq(rtc, 1, RTC_IRQF | RTC_AF);

		/* Disable the alarm */
		err = rv4162_rtc_toggle_alarm(client, 0);
		if (!err)
			handled = 1;
	}
	return handled ? IRQ_HANDLED : IRQ_NONE;
}

static int rv4162_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
        return rv4162_i2c_read_time(to_i2c_client(dev), tm);
}

static int rv4162_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	return rv4162_i2c_set_time(to_i2c_client(dev), tm);

}
static int rv4162_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alarm)
{
	return rv4162_i2c_read_alarm(to_i2c_client(dev), alarm);
}

static int rv4162_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alarm)
{
	return rv4162_i2c_set_alarm(to_i2c_client(dev), alarm);
}


static const struct rtc_class_ops rv4162_rtc_ops = {
	.proc = rv4162_rtc_proc,
	.read_time = rv4162_rtc_read_time,
	.set_time = rv4162_rtc_set_time,
	.read_alarm = rv4162_rtc_read_alarm,
	.set_alarm = rv4162_rtc_set_alarm,
};

static int rv4162_remove(struct i2c_client *client)
{
	struct rtc_device *rtc = i2c_get_clientdata(client);

	if (rtc)
		rtc_device_unregister(rtc);

	return 0;
}

static int rv4162_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct rtc_device *rtc;
	unsigned char addr = RV4162_REG_FLAGS;
	unsigned char flags;
	struct i2c_msg msgs[] = {
		{client->addr, 0, 1, &addr},
		{client->addr, I2C_M_RD, 1, &flags},
	};

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return -ENODEV;

	if ((i2c_transfer(client->adapter, &msgs[0], 2)) != 2) {
		dev_err(&client->dev, "%s: read error\n", __func__);
		return -EIO;
	}

	dev_info(&client->dev, "%s: chip found: flags 0x%02x\n", __func__, flags);
	if (flags) {
		u8 buf[2];
		struct i2c_msg clearmsg = {
			client->addr, 0, sizeof(buf), buf,
		};
		buf[0] = RV4162_REG_FLAGS;
		buf[1] = 0;
		if (1 != i2c_transfer(client->adapter, &clearmsg, 1))
			dev_err(&client->dev, "%s: error clearing flags\n",
				__func__);
	}

	if (client->irq > 0) {
		int rc = request_threaded_irq(client->irq, NULL,
					  rv4162_rtc_interrupt,
					  IRQF_SHARED | IRQF_ONESHOT,
					  rv4162_driver.driver.name, client);
		if (!rc) {
			device_init_wakeup(&client->dev, 1);
			enable_irq_wake(client->irq);
		} else {
			dev_err(&client->dev,
				"Unable to request irq %d, no alarm support\n",
				client->irq);
			client->irq = 0;
		}
	}

	rtc = rtc_device_register(rv4162_driver.driver.name, &client->dev,
				  &rv4162_rtc_ops, THIS_MODULE);

	if (IS_ERR(rtc))
		return PTR_ERR(rtc);

	i2c_set_clientdata(client, rtc);

	return 0;
}

static struct i2c_device_id rv4162_id[] = {
	{ "rv4162", 0 },
	{ }
};

static struct of_device_id rv4162_dt_ids[] = {
	{ .compatible = "mcrystal,rv4162" },
	{ /* sentinel */ }
};

static struct i2c_driver rv4162_driver = {
	.driver = {
		   .name = "rtc-rv4162",
		   .of_match_table = of_match_ptr(rv4162_dt_ids),
		   },
	.probe = &rv4162_probe,
	.remove = &rv4162_remove,
	.id_table = rv4162_id,
};

static int __init rv4162_init(void)
{
	return i2c_add_driver(&rv4162_driver);
}

static void __exit rv4162_exit(void)
{
	i2c_del_driver(&rv4162_driver);
}

MODULE_AUTHOR("Eric Nelson <eric.nelson@boundarydevices.com>");
MODULE_DESCRIPTION("Micro Crystal RV-4162 Real-time clock driver");
MODULE_LICENSE("GPL");

module_init(rv4162_init);
module_exit(rv4162_exit);
