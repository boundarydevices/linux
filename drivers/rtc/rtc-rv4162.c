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

#include <linux/i2c.h>
#include <linux/rtc.h>

/* Registers */
#define RV4162_REG_SECS		0x01
#define RV4162_SECFLAG_ST 0x80

#define RV4162_REG_FLAGS	0x0f
#define RV4162_FLAG_OF	4

static struct i2c_driver rv4162_driver;

#define BCD_TO_BIN(v) (((v)&0x0f)+(((v)&0xF0)>>4)*10)

static int rv4162_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
        struct i2c_client *client = to_i2c_client(dev);
	u8 addr = 0;
	u8 buf[8];
	int err;
	struct i2c_msg msgs[] = {
		{client->addr, 0, 1, &addr},
		{client->addr, I2C_M_RD, sizeof(buf), buf},
	};
	memset(tm,0,sizeof(*tm));
	if (2 == (err=i2c_transfer(client->adapter, &msgs[0], 2))) {
		tm->tm_sec  = BCD_TO_BIN(buf[1]&0x7f);
		tm->tm_min  = BCD_TO_BIN(buf[2]&0x7f);
		tm->tm_hour = BCD_TO_BIN(buf[3]);
		tm->tm_mon  = BCD_TO_BIN(buf[6]&0x1F)-1;
		tm->tm_mday = BCD_TO_BIN(buf[5]);
		tm->tm_wday = (buf[4]&7)-1;
		tm->tm_yday = -1;
		tm->tm_year = BCD_TO_BIN(buf[7])+(100*(buf[6]>>6));
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

#define BIN_TO_BCD(v) ((((v)/10)<<4)|((v)%10))

static int rv4162_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
        struct i2c_client *client = to_i2c_client(dev);
	u8 buf[9];
	int err;
	struct i2c_msg msg = {
		client->addr, 0, sizeof(buf), buf
	};
	buf[0] = 0;
	buf[1] = 0;
	buf[2] = BIN_TO_BCD(tm->tm_sec);
	buf[3] = BIN_TO_BCD(tm->tm_min);
	buf[4] = BIN_TO_BCD(tm->tm_hour);
	buf[5] = tm->tm_wday+1;
	buf[6] = BIN_TO_BCD(tm->tm_mday);
	buf[7] = BIN_TO_BCD(tm->tm_mon+1) | ((tm->tm_year/100)<<6);
	buf[8] = BIN_TO_BCD(tm->tm_year%100);
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

static const struct rtc_class_ops rv4162_rtc_ops = {
	.read_time = rv4162_rtc_read_time,
	.set_time = rv4162_rtc_set_time,
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

static struct i2c_driver rv4162_driver = {
	.driver = {
		   .name = "rtc-rv4162",
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
