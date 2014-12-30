/*
 * Ambient light sensor APDS-9300 from Avago Technologies
 *
 * Copyright (c) 2014 Boundary Devices
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/hwmon-sysfs.h>

#define REG_CONTROL		0x00
#define REG_TIMING		0x01
#define REG_THRESHLOWLOW	0x02
#define REG_THRESHLOWHIGH	0x03
#define REG_THRESHHIGHLOW	0x04
#define REG_THRESHHIGHHIGH	0x05
#define REG_INTERRUPT		0x06
#define REG_CRC			0x08
#define REG_ID			0x0a
#define REG_DATA0LOW		0x0c
#define REG_DATA0HIGH		0x0d
#define REG_DATA1LOW		0x0e
#define REG_DATA1HIGH		0x0f

#define APDS_COMMAND	0x80
#define APDS_WORD	0x20

static ssize_t show_reg(struct device *dev, struct device_attribute *devattr,
			   char *buf, int regnum)
{
	struct i2c_client *client = to_i2c_client(dev);
	int ret = i2c_smbus_read_byte_data(client, regnum|APDS_COMMAND);
	if (0 <= ret) {
		return sprintf(buf, "apds[%02x] == 0x%02x\n",regnum,ret);
	} else {
		dev_err(&client->dev, "%s: error reading register 0x%02x\n",
			__func__, regnum);
		return ret;
	}
}

static ssize_t set_reg(struct device *dev, struct device_attribute *devattr,
			  const char *buf, size_t count, int regnum)
{
	struct i2c_client *client = to_i2c_client(dev);
	unsigned long value;
	int ret;

	if (strict_strtoul(buf, 0, &value)
	    ||(value > 0xff))
		return -EINVAL;
	ret = i2c_smbus_write_byte_data(client, regnum|APDS_COMMAND,(u8)value);
	if (0 <= ret) {
		return count;
	} else {
		dev_err(&client->dev, "%s: error writing register 0x%02x\n",
			__func__, regnum);
		return ret;
	}
}

#define EXPOSE_REG(name,num) \
static ssize_t show_##name(struct device *dev, 			\
			   struct device_attribute *devattr,	\
			   char *buf)				\
{ 								\
	return show_reg(dev,devattr,buf,num);			\
}								\
								\
static ssize_t set_##name(struct device *dev,			\
			  struct device_attribute *devattr,	\
			  const char *buf, size_t count)	\
{								\
	return set_reg(dev,devattr,buf,count,num);		\
}								\
								\
static SENSOR_DEVICE_ATTR_2(name,				\
				S_IWUSR | S_IRUGO,		\
				show_##name,			\
				set_##name,			\
				2, 3)				\

EXPOSE_REG(control,	REG_CONTROL);
EXPOSE_REG(timing,	REG_TIMING);
EXPOSE_REG(tlowlow,	REG_THRESHLOWLOW);
EXPOSE_REG(tlowhigh,	REG_THRESHLOWHIGH);
EXPOSE_REG(thighlow,	REG_THRESHHIGHLOW);
EXPOSE_REG(thighhigh,	REG_THRESHHIGHHIGH);
EXPOSE_REG(interrupt,	REG_INTERRUPT);
EXPOSE_REG(crc,		REG_CRC);
EXPOSE_REG(id,		REG_ID);
EXPOSE_REG(d0low,	REG_DATA0LOW);
EXPOSE_REG(d0high,	REG_DATA0HIGH);
EXPOSE_REG(d1low,	REG_DATA1LOW);
EXPOSE_REG(d1high,	REG_DATA1HIGH);

static ssize_t show_d0(struct device *dev,
		       struct device_attribute *devattr,
		       char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	int ret = i2c_smbus_read_word_data(client, REG_DATA0LOW|APDS_COMMAND|APDS_WORD);
	if (0 <= ret) {
		return sprintf(buf, "0x%04x\n",ret);
	} else {
		dev_err(&client->dev, "error reading d0\n");
		return ret;
	}
}

static ssize_t show_d1(struct device *dev,
		       struct device_attribute *devattr,
		       char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	int ret = i2c_smbus_read_word_data(client, REG_DATA1LOW|APDS_COMMAND|APDS_WORD);
	if (0 <= ret) {
		return sprintf(buf, "0x%04x\n",ret);
	} else {
		dev_err(&client->dev, "error reading d1\n");
		return ret;
	}
}

static SENSOR_DEVICE_ATTR_2(d0,S_IRUGO,show_d0,0,2,3);
static SENSOR_DEVICE_ATTR_2(d1,S_IRUGO,show_d1,0,2,3);

/* Calculated values 1000 * (CH1/CH0)^1.4 for CH1/CH0 from 0 to 0.52 */
static const u16 apds9300_lux_ratio[] = {
	0, 2, 4, 7, 11, 15, 19, 24, 29, 34, 40, 45, 51, 57, 64, 70, 77, 84, 91,
	98, 105, 112, 120, 128, 136, 144, 152, 160, 168, 177, 185, 194, 203,
	212, 221, 230, 239, 249, 258, 268, 277, 287, 297, 307, 317, 327, 337,
	347, 358, 368, 379, 390, 400,
};

#define DIV_ROUND_UP_ULL(ll,d) \
	({ unsigned long long _tmp = (ll)+(d)-1; do_div(_tmp, d); _tmp; })

static unsigned long apds9300_calculate_lux(u16 ch0, u16 ch1)
{
	unsigned long lux, tmp;

	/* avoid division by zero */
	if (ch0 == 0)
		return 0;

	tmp = DIV_ROUND_UP(ch1 * 100, ch0);
	if (tmp <= 52) {
		lux = 3150 * ch0 - (unsigned long)DIV_ROUND_UP_ULL(ch0
				* apds9300_lux_ratio[tmp] * 5930ull, 1000);
	} else if (tmp <= 65) {
		lux = 2290 * ch0 - 2910 * ch1;
	} else if (tmp <= 80) {
		lux = 1570 * ch0 - 1800 * ch1;
	} else if (tmp <= 130) {
		lux = 338 * ch0 - 260 * ch1;
	} else {
		lux = 0;
	}

	return lux / 100000;
}

static ssize_t show_lux(struct device *dev,
                        struct device_attribute *devattr,
                        char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	int ret = i2c_smbus_read_word_data(client, REG_DATA0LOW|APDS_COMMAND|APDS_WORD);
	if (0 <= ret) {
		u16 ch0 = (u16)ret;
                ret = i2c_smbus_read_word_data(client, REG_DATA1LOW|APDS_COMMAND|APDS_WORD);
		if (0 <= ret) {
			u16 ch1 = (u16)ret;
			return sprintf(buf, "0x%08lx\n",apds9300_calculate_lux(ch0,ch1));
		} else {
			dev_err(&client->dev, "error reading d0\n");
			return ret;
		}
	} else {
		dev_err(&client->dev, "error reading d0\n");
		return ret;
	}
}

static SENSOR_DEVICE_ATTR_2(lux,S_IRUGO,show_lux,0,2,3);

static struct sensor_device_attribute_2 * const attrs[] = {
	&sensor_dev_attr_control,
	&sensor_dev_attr_timing,
	&sensor_dev_attr_tlowlow,
	&sensor_dev_attr_tlowhigh,
	&sensor_dev_attr_thighlow,
	&sensor_dev_attr_thighhigh,
	&sensor_dev_attr_interrupt,
	&sensor_dev_attr_crc,
	&sensor_dev_attr_id,
	&sensor_dev_attr_d0low,
	&sensor_dev_attr_d0high,
	&sensor_dev_attr_d1low,
	&sensor_dev_attr_d1high,
	&sensor_dev_attr_d0,
	&sensor_dev_attr_d1,
	&sensor_dev_attr_lux,
};

static int apds9300_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	int i;
	int err;
	for (i=0; i < ARRAY_SIZE(attrs); i++) {
		err = device_create_file(&client->dev, &attrs[i]->dev_attr);
		if (err)
			break;
	}

	if (i == ARRAY_SIZE(attrs)) {
		return 0;
	} else {
		dev_err(&client->dev, "%s: Error creating attribute %d\n",
			__func__, i);
		while (i > 0) {
			device_remove_file(&client->dev, &attrs[i]->dev_attr);
			i--;
		}
		return err;
	}
}

static int apds9300_remove(struct i2c_client *client)
{
	int i;
	for (i=0; i < ARRAY_SIZE(attrs); i++)
		device_remove_file(&client->dev, &attrs[i]->dev_attr);

	return 0;
}

static const struct i2c_device_id apds9300_id[] = {
	{"apds9300", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, apds9300_id);

static struct i2c_driver apds9300_driver = {
	.driver = {
		   .name = "apds9300",
		   },
	.probe = apds9300_probe,
	.remove = apds9300_remove,
	.id_table = apds9300_id,
};

static int __init apds9300_init(void)
{
	return i2c_add_driver(&apds9300_driver);
}

static void __exit apds9300_exit(void)
{
	i2c_del_driver(&apds9300_driver);
}

MODULE_AUTHOR("Eric Nelson");
MODULE_DESCRIPTION("Ambient light sensor driver for Avago Technologies APDS9300");
MODULE_LICENSE("GPL");
module_init(apds9300_init);
module_exit(apds9300_exit);
