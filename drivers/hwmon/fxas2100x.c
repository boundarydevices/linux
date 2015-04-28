/*
 *  fxas2100x.c - Linux kernel modules for 3-Axis Gyroscope sensor
 *  Copyright (C) 2012-2015 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/pm.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/hwmon.h>
#include <linux/input-polldev.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>

#define	SENSOR_IOCTL_BASE	'S'
#define	SENSOR_GET_MODEL_NAME		_IOR(SENSOR_IOCTL_BASE, 0, char *)
#define	SENSOR_GET_POWER_STATUS		_IOR(SENSOR_IOCTL_BASE, 2, int)
#define	SENSOR_SET_POWER_STATUS		_IOR(SENSOR_IOCTL_BASE, 3, int)
#define	SENSOR_GET_DELAY_TIME		_IOR(SENSOR_IOCTL_BASE, 4, int)
#define	SENSOR_SET_DELAY_TIME		_IOR(SENSOR_IOCTL_BASE, 5, int)
#define	SENSOR_GET_RAW_DATA			_IOR(SENSOR_IOCTL_BASE, 6, short[3])

#define FXAS2100X_I2C_ADDR         0x20

#define FXAS21000_CHIP_ID		   0xD1

#define FXAS21002_CHID_ID_1		   0xD6
#define FXAS21002_CHID_ID_2		   0xD7

#define FXAS2100X_POSITION_DEFAULT	2
#define FXAS2100X_DELAY_DEFAULT		200

#define FXAS2100X_STATUS_ZYXDR    0x08
#define FXAS2100X_BUF_SIZE    6

/* register enum for fxas2100x registers */
enum {
	FXAS2100X_STATUS = 0x00,
	FXAS2100X_OUT_X_MSB,
	FXAS2100X_OUT_X_LSB,
	FXAS2100X_OUT_Y_MSB,
	FXAS2100X_OUT_Y_LSB,
	FXAS2100X_OUT_Z_MSB,
	FXAS2100X_OUT_Z_LSB,
	FXAS2100X_DR_STATUS,
	FXAS2100X_F_STATUS,
	FXAS2100X_F_SETUP,
	FXAS2100X_F_EVENT,
	FXAS2100X_INT_SRC_FLAG,
	FXAS2100X_WHO_AM_I,
	FXAS2100X_CTRL_REG0,
	FXAS2100X_RT_CFG,
	FXAS2100X_RT_SRC,
	FXAS2100X_RT_THS,
	FXAS2100X_RT_COUNT,
	FXAS2100X_TEMP,
	FXAS2100X_CTRL_REG1,
	FXAS2100X_CTRL_REG2,
	FXAS2100X_CTRL_REG3, // fxos21002 special
	FXAS2100X_REG_END,
};

enum {
	STANDBY = 0,
	ACTIVED,
};
struct fxas2100x_data_axis {
	short x;
	short y;
	short z;
};
struct fxas2100x_data {
	struct i2c_client *client;
	atomic_t active;
	atomic_t delay;
	atomic_t position;
	u8 chip_id;
};
static struct fxas2100x_data *g_fxas2100x_data = NULL;

static int fxas2100x_position_setting[8][3][3] = {
	{ { 0,	-1, 0  }, { 1,	0,  0	   }, { 0, 0, 1	     } },
	{ { -1, 0,  0  }, { 0,	-1, 0	   }, { 0, 0, 1	     } },
	{ { 0,	1,  0  }, { -1, 0,  0	   }, { 0, 0, 1	     } },
	{ { 1,	0,  0  }, { 0,	1,  0	   }, { 0, 0, 1	     } },

	{ { 0,	-1, 0  }, { -1, 0,  0	   }, { 0, 0, -1     } },
	{ { -1, 0,  0  }, { 0,	1,  0	   }, { 0, 0, -1     } },
	{ { 0,	1,  0  }, { 1,	0,  0	   }, { 0, 0, -1     } },
	{ { 1,	0,  0  }, { 0,	-1, 0	   }, { 0, 0, -1     } },
};

static int fxas2100x_data_convert(struct fxas2100x_data *pdata,
		struct fxas2100x_data_axis *axis_data)
{
	short rawdata[3], data[3];
	int i, j;
	int position = atomic_read(&pdata->position);

	if (position < 0 || position > 7)
		position = 0;
	rawdata[0] = axis_data->x;
	rawdata[1] = axis_data->y;
	rawdata[2] = axis_data->z;
	for (i = 0; i < 3; i++) {
		data[i] = 0;
		for (j = 0; j < 3; j++)
			data[i] += rawdata[j] * fxas2100x_position_setting[position][i][j];
	}
	axis_data->x = data[0];
	axis_data->y = data[1];
	axis_data->z = data[2];
	return 0;
}
static int fxas2100x_device_init(struct i2c_client *client)
{
	int result;
	u8 val;
	struct fxas2100x_data *pdata = i2c_get_clientdata(client);
	if(pdata->chip_id == FXAS21000_CHIP_ID)
		val = (0x01 << 2);            /*fxas21000 dr 200HZ */
	else
		val = (0x02 << 2);			  /*fxas21002 dr 200HZ */
	result = i2c_smbus_write_byte_data(client, FXAS2100X_CTRL_REG1,val);
	if (result < 0)
		goto out;
	atomic_set(&pdata->active,STANDBY);
	return 0;
out:
	dev_err(&client->dev, "error when init fxas2100x:(%d)", result);
	return result;
}

static int fxas2100x_change_mode(struct i2c_client *client, int mode)
{
	u8 val;
	int ret;
	if(mode == ACTIVED){
		val = i2c_smbus_read_byte_data(client, FXAS2100X_CTRL_REG1);
		val &= ~0x03;
		val |= 0x02;
		ret = i2c_smbus_write_byte_data(client,FXAS2100X_CTRL_REG1, val);  //set bit 1

	 }else{
		val = i2c_smbus_read_byte_data(client, FXAS2100X_CTRL_REG1);
		val &= (~0x03);
		ret = i2c_smbus_write_byte_data(client, FXAS2100X_CTRL_REG1,val); // clear bit 0,1
	}
	return ret;
}
static int fxas2100x_set_delay(struct i2c_client *client, int delay)
{
	return 0;
}
static int fxas2100x_device_stop(struct i2c_client *client)
{
	u8 val;
	val = i2c_smbus_read_byte_data(client, FXAS2100X_CTRL_REG1);
	val &= ~0x03;
	i2c_smbus_write_byte_data(client, FXAS2100X_CTRL_REG1, val);
	return 0;
}

static int fxas2100x_read_data(struct fxas2100x_data *pdata,
		struct fxas2100x_data_axis *data)
{
    struct i2c_client * client = pdata->client;

	int x ,y ,z;
	u8 tmp_data[FXAS2100X_BUF_SIZE];
	int ret;

	ret = i2c_smbus_read_i2c_block_data(client,
					    FXAS2100X_OUT_X_MSB,
						FXAS2100X_BUF_SIZE, tmp_data);
	if (ret < FXAS2100X_BUF_SIZE) {
		dev_err(&client->dev, "i2c block read failed\n");
		return -EIO;
	}
	data->x = ((tmp_data[0] << 8) & 0xff00) | tmp_data[1];
	data->y = ((tmp_data[2] << 8) & 0xff00) | tmp_data[3];
	data->z = ((tmp_data[4] << 8) & 0xff00) | tmp_data[5];
	if(pdata->chip_id ==  FXAS21000_CHIP_ID)
	{
		x = data->x;
		y = data->y;
		z = data->z;
		x = x * 4 / 5;
		y = y * 4 / 5;
		z = z * 4 / 5;
		data->x = x;
		data->y = y;
		data->z = z;
	}
	return 0;
}

//fxas2100x miscdevice
static long fxas2100x_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct fxas2100x_data *pdata = file->private_data;
	void __user *argp = (void __user *)arg;
    long ret = 0;
	short sdata[3];
	int enable;
	int delay;
	struct fxas2100x_data_axis data;
	if(!pdata){
		printk(KERN_ERR "FXAS2100X struct datt point is NULL.");
		return -EFAULT;
	}
	switch (cmd) {
		case SENSOR_GET_MODEL_NAME:
			if(copy_to_user(argp,"FXAS2100X GYRO",strlen("FXAS2100X GYRO") +1))
			{
				printk(KERN_ERR "SENSOR_GET_MODEL_NAME copy_to_user failed.");
				ret = -EFAULT;
			}
			break;
		case SENSOR_GET_POWER_STATUS:
			enable = atomic_read(&pdata->active);
			if(copy_to_user(argp,&enable,sizeof(int)))
			{
				printk(KERN_ERR "SENSOR_SET_POWER_STATUS copy_to_user failed.");
				ret = -EFAULT;
			}
			break;
		case SENSOR_SET_POWER_STATUS:
			if(copy_from_user(&enable,argp,sizeof(int)))
			{
				printk(KERN_ERR "SENSOR_SET_POWER_STATUS copy_to_user failed.");
				ret = -EFAULT;
			}
			if(pdata->client){
				ret = fxas2100x_change_mode(pdata->client,enable? ACTIVED : STANDBY);
				if(!ret)
					atomic_set(&pdata->active,enable);
			}
			break;
		case SENSOR_GET_DELAY_TIME:
			delay = atomic_read(&pdata->delay);
			if(copy_to_user(argp, &delay, sizeof(delay)))
			{
				printk(KERN_ERR "SENSOR_GET_DELAY_TIME copy_to_user failed.");
				return -EFAULT;
			}
			break;
		case SENSOR_SET_DELAY_TIME:
			if(copy_from_user(&delay,argp,sizeof(int)));
			{
				printk(KERN_ERR "SENSOR_GET_DELAY_TIME copy_to_user failed.");
				ret = -EFAULT;
			}
			if(pdata->client && delay > 0 && delay <= 500){
				ret = fxas2100x_set_delay(pdata->client,delay);
				if(!ret)
					atomic_set(&pdata->delay,delay);
			}
			break;
		case SENSOR_GET_RAW_DATA:
			ret = fxas2100x_read_data(pdata,&data);
			if(!ret){
				fxas2100x_data_convert(pdata,&data);
				sdata[0] = data.x;
				sdata[1] = data.y;
				sdata[2] = data.z;
				if(copy_to_user(argp,sdata,sizeof(sdata)))
				{
					printk(KERN_ERR "SENSOR_GET_RAW_DATA copy_to_user failed.");
					ret = -EFAULT;
				}
			}
			break;
		default:
			ret = -1;
	}
	return ret;
}

static int fxas2100x_open(struct inode *inode, struct file *file)
{
	file->private_data = g_fxas2100x_data;
	return nonseekable_open(inode, file);
}

static int fxas2100x_release(struct inode *inode, struct file *file)
{
	/* note: releasing the wdt in NOWAYOUT-mode does not stop it */
	return 0;
}

static const struct file_operations fxas2100x_fops = {
	.owner = THIS_MODULE,
	.open = fxas2100x_open,
	.release = fxas2100x_release,
	.unlocked_ioctl = fxas2100x_ioctl,
};

static struct miscdevice fxas2100x_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "FreescaleGyroscope",
	.fops = &fxas2100x_fops,
};

static ssize_t fxas2100x_enable_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct fxas2100x_data *pdata = g_fxas2100x_data;
	int enable = 0;
	enable = atomic_read(&pdata->active);
	return sprintf(buf, "%d\n", enable);
}

static ssize_t fxas2100x_enable_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct fxas2100x_data *pdata = g_fxas2100x_data;
	struct i2c_client *client = pdata->client;
	int ret;
	unsigned long enable;
	enable = simple_strtoul(buf, NULL, 10);
	enable = (enable > 0) ? 1 : 0;
	ret = fxas2100x_change_mode(client,(enable > 0 ? ACTIVED : STANDBY));
	if (!ret) {
		atomic_set(&pdata->active,enable);
		printk(KERN_INFO"mma enable setting active \n");
	}
	return count;
}

static ssize_t fxas2100x_poll_delay_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct fxas2100x_data *pdata = g_fxas2100x_data;
	int delay = 0;
	delay = atomic_read(&pdata->delay);
	return sprintf(buf, "%d\n", delay);
}

static ssize_t fxas2100x_poll_delay_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct fxas2100x_data *pdata = g_fxas2100x_data;
	struct i2c_client *client = pdata->client;
	int ret;
	int delay;
	delay = simple_strtoul(buf, NULL, 10);
	ret = fxas2100x_set_delay(client,delay);
	if(!ret)
		atomic_set(&pdata->delay, delay);
	return count;
}

static ssize_t fxas2100x_position_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct fxas2100x_data *pdata = g_fxas2100x_data;
	int position = 0;
	position = atomic_read(&pdata->position);
	return sprintf(buf, "%d\n", position);
}

static ssize_t fxas2100x_position_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct fxas2100x_data *pdata = g_fxas2100x_data;
	int position;
	position = simple_strtoul(buf, NULL, 10);
	atomic_set(&pdata->position,position);
	return count;
}

static DEVICE_ATTR(enable, 0666, fxas2100x_enable_show, fxas2100x_enable_store);

static DEVICE_ATTR(poll_delay, 0666,fxas2100x_poll_delay_show, fxas2100x_poll_delay_store);

static DEVICE_ATTR(position, 0666,fxas2100x_position_show, fxas2100x_position_store);

static struct attribute *fxas2100x_attributes[] = {
	&dev_attr_enable.attr,
	&dev_attr_poll_delay.attr,
	&dev_attr_position.attr,
	NULL
};

static const struct attribute_group fxas2100x_attr_group = {
	.attrs	= fxas2100x_attributes,
};

static int fxas2100x_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	int result, chip_id;
	struct fxas2100x_data *pdata;
	struct i2c_adapter *adapter;

	adapter = to_i2c_adapter(client->dev.parent);
	result = i2c_check_functionality(adapter,
					 I2C_FUNC_SMBUS_BYTE |
					 I2C_FUNC_SMBUS_BYTE_DATA);
	if (!result)
		goto err_out;

	chip_id = i2c_smbus_read_byte_data(client, FXAS2100X_WHO_AM_I);

	if (chip_id != FXAS21000_CHIP_ID && chip_id != FXAS21002_CHID_ID_1 && chip_id != FXAS21002_CHID_ID_2) {
		dev_err(&client->dev,"read chip ID 0x%x is not equal to 0x%x for fxos21000) or 0x%x/0x%x,(fxos21002) !\n",
							chip_id,FXAS21000_CHIP_ID,FXAS21002_CHID_ID_1,FXAS21002_CHID_ID_2);
		result = -EINVAL;
		goto err_out;
	}
	pdata = kzalloc(sizeof(struct fxas2100x_data), GFP_KERNEL);
	if (!pdata) {
		result = -ENOMEM;
		dev_err(&client->dev, "alloc data memory error!\n");
		goto err_out;
	}
	/* Initialize the FXAS2100X chip */
	g_fxas2100x_data = pdata;
	pdata->client = client;
	pdata->chip_id = chip_id;
	atomic_set(&pdata->delay,FXAS2100X_DELAY_DEFAULT);
	atomic_set(&pdata->position,FXAS2100X_POSITION_DEFAULT);
	i2c_set_clientdata(client, pdata);
	result = misc_register(&fxas2100x_device);
	if (result != 0) {
		printk(KERN_ERR "register acc miscdevice error");
		goto err_regsiter_misc;
	}

	result = sysfs_create_group(&fxas2100x_device.this_device->kobj, &fxas2100x_attr_group);\
	if (result) {
		dev_err(&client->dev, "create device file failed!\n");
		result = -EINVAL;
		goto err_create_sysfs;
	}
	fxas2100x_device_init(client);
	printk(KERN_INFO"fxas2100x device driver probe successfully\n");
	return 0;
err_create_sysfs:
	misc_deregister(&fxas2100x_device);
err_regsiter_misc:
	kfree(pdata);
err_out:
	return result;
}
static int fxas2100x_remove(struct i2c_client *client)
{
	struct fxas2100x_data *pdata = i2c_get_clientdata(client);
	fxas2100x_device_stop(client);
	misc_deregister(&fxas2100x_device);
	if(pdata != NULL)
		kfree(pdata);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int fxas2100x_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fxas2100x_data *pdata = i2c_get_clientdata(client);

	if (atomic_read(&pdata->active))
		fxas2100x_device_stop(client);
	return 0;
}

static int fxas2100x_resume(struct device *dev)
{
	int val = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct fxas2100x_data *pdata = i2c_get_clientdata(client);

	if (atomic_read(&pdata->active)) {
		val = i2c_smbus_read_byte_data(client, FXAS2100X_CTRL_REG1);
		val &= ~0x03;
		val |= 0x02;
		i2c_smbus_write_byte_data(client, FXAS2100X_CTRL_REG1, val);
	}
	return 0;

}
#endif

static const struct i2c_device_id fxas2100x_id[] = {
	{ "fxas2100x", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, fxas2100x_id);

static SIMPLE_DEV_PM_OPS(fxas2100x_pm_ops, fxas2100x_suspend, fxas2100x_resume);
static struct i2c_driver fxas2100x_driver = {
	.class		= I2C_CLASS_HWMON,
	.driver		= {
		.name	= "fxas2100x",
		.owner	= THIS_MODULE,
		.pm	= &fxas2100x_pm_ops,
	},
	.probe		= fxas2100x_probe,
	.remove		= fxas2100x_remove,
	.id_table	= fxas2100x_id,
};

module_i2c_driver(fxas2100x_driver);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("FXAS2100X 3-Axis Gyrosope Sensor driver");
MODULE_LICENSE("GPL");
