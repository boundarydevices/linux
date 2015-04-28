/*
 *  fxos8700.c - Linux kernel modules for FXOS8700 6-Axis Acc and Mag
 *  Combo Sensor
 *
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
#include "fxos8700.h"

#define FXOS8700_DELAY_DEFAULT		200 	/* msecs */
#define FXOS8700_POSITION_DEFAULT	1 	/* msecs */

#define FXOS8700_TYPE_ACC 	0x00
#define FXOS8700_TYPE_MAG 	0x01

#define FXOS8700_STANDBY 	0x00
#define FXOS8700_ACTIVED 	0x01

#define ABS_STATUS 			ABS_WHEEL
struct fxos8700_data_axis{
    short x;
	short y;
	short z;
};

struct fxos8700_data{
	struct i2c_client * client;
	struct miscdevice * acc_miscdev;
	struct miscdevice * mag_miscdev;
	atomic_t acc_delay;
	atomic_t mag_delay;
	atomic_t acc_active;
	atomic_t mag_active;
	atomic_t position;
};
static struct fxos8700_data * g_fxos8700_data = NULL;
static int fxos8700_position_settings[8][3][3] =
{
   {{ 0, -1,  0}, { 1,  0,	0}, {0, 0,	1}},
   {{-1,  0,  0}, { 0, -1,	0}, {0, 0,	1}},
   {{ 0,  1,  0}, {-1,  0,	0}, {0, 0,	1}},
   {{ 1,  0,  0}, { 0,  1,	0}, {0, 0,	1}},

   {{ 0, -1,  0}, {-1,  0,	0}, {0, 0,  -1}},
   {{-1,  0,  0}, { 0,  1,	0}, {0, 0,  -1}},
   {{ 0,  1,  0}, { 1,  0,	0}, {0, 0,  -1}},
   {{ 1,  0,  0}, { 0, -1,	0}, {0, 0,  -1}},
};
static int fxos8700_data_convert(struct fxos8700_data_axis *axis_data,int position)
{
   short rawdata[3],data[3];
   int i,j;
   if(position < 0 || position > 7 )
   		position = 0;
   rawdata [0] = axis_data->x;
   rawdata [1] = axis_data->y;
   rawdata [2] = axis_data->z;
   for(i = 0; i < 3 ; i++)
   {
   	data[i] = 0;
   	for(j = 0; j < 3; j++)
		data[i] += rawdata[j] * fxos8700_position_settings[position][i][j];
   }
   axis_data->x = data[0];
   axis_data->y = data[1];
   axis_data->z = data[2];
   return 0;
}
static int fxos8700_change_mode(struct i2c_client *client, int type,int active)
{
	u8 data;
	int acc_act,mag_act;
	struct fxos8700_data *pdata =  i2c_get_clientdata(client);
	acc_act = atomic_read(&pdata->acc_active);
	mag_act = atomic_read(&pdata->mag_active);
	data = i2c_smbus_read_byte_data(client, FXOS8700_CTRL_REG1);
	if(type == FXOS8700_TYPE_ACC)
		acc_act = active;
	else
		mag_act = active;
	if(acc_act ==  FXOS8700_ACTIVED || mag_act == FXOS8700_ACTIVED)
		data |= 0x01;
	else
		data &= ~0x01;
	i2c_smbus_write_byte_data(client, FXOS8700_CTRL_REG1,data);
	return 0;

}
static int fxos8700_set_odr(struct i2c_client * client,int type, int delay){
	return 0;
}

static int fxos8700_device_init(struct i2c_client *client)
{
	int result;
	struct fxos8700_data *pdata =  i2c_get_clientdata(client);

	result = i2c_smbus_write_byte_data(client, FXOS8700_CTRL_REG1, 0x00); //standby mode
	if (result < 0)
		goto out;
	result = i2c_smbus_write_byte_data(client, FXOS8700_M_CTRL_REG1, 0x1F); //
	if (result < 0)
		goto out;
	result = i2c_smbus_write_byte_data(client, FXOS8700_M_CTRL_REG2,0x5c); //hybrid mode
	if (result < 0)
		goto out;
	result = i2c_smbus_write_byte_data(client, FXOS8700_CTRL_REG1, 0x03 << 3); //odr 50hz
	if (result < 0)
		goto out;

	atomic_set(&pdata->acc_active,FXOS8700_STANDBY);
	atomic_set(&pdata->mag_active,FXOS8700_STANDBY);
    atomic_set(&pdata->position ,FXOS8700_POSITION_DEFAULT);
	return 0;
out:
	dev_err(&client->dev, "error when init fxos8700 device:(%d)", result);
	return result;
}

static int fxos8700_device_stop(struct i2c_client *client)
{
	i2c_smbus_write_byte_data(client, FXOS8700_CTRL_REG1, 0x00);
	return 0;
}

static int fxos8700_read_data(struct i2c_client *client,struct fxos8700_data_axis *data, int type)
{
	u8 tmp_data[FXOS8700_DATA_BUF_SIZE];
	int ret;
	u8 reg;
	if(type == FXOS8700_TYPE_ACC)
		reg = FXOS8700_OUT_X_MSB;
	else
		reg = FXOS8700_M_OUT_X_MSB;

	ret = i2c_smbus_read_i2c_block_data(client, reg, FXOS8700_DATA_BUF_SIZE, tmp_data);
	if (ret < FXOS8700_DATA_BUF_SIZE) {
		dev_err(&client->dev, "i2c block read %s failed\n", (type == FXOS8700_TYPE_ACC ? "acc" : "mag"));
		return -EIO;
	}
	data->x = ((tmp_data[0] << 8) & 0xff00) | tmp_data[1];
	data->y = ((tmp_data[2] << 8) & 0xff00) | tmp_data[3];
	data->z = ((tmp_data[4] << 8) & 0xff00) | tmp_data[5];
	return 0;
}
static long fxos8700_acc_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct fxos8700_data *pdata = file->private_data;
	void __user *argp = (void __user *)arg;
    long ret = 0;
	short sdata[3];
	int enable;
	int delay;
	int position;
	struct fxos8700_data_axis data;
	if(!pdata){
		printk(KERN_ERR "fxos8700 struct datt point is NULL.");
		return -EFAULT;
	}
	switch (cmd) {
		case SENSOR_GET_MODEL_NAME:
			if(copy_to_user(argp,"FXOS8700 ACC",strlen("FXOS8700 ACC") +1))
			{
				printk(KERN_ERR "SENSOR_GET_MODEL_NAME copy_to_user failed.");
				ret = -EFAULT;
			}
			break;
		case SENSOR_GET_POWER_STATUS:
			enable = atomic_read(&pdata->acc_active);
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
				ret = fxos8700_change_mode(pdata->client, FXOS8700_TYPE_ACC,
										enable? FXOS8700_ACTIVED : FXOS8700_STANDBY);
				if(!ret)
					atomic_set(&pdata->acc_active,enable);
			}
			break;
		case SENSOR_GET_DELAY_TIME:
			delay = atomic_read(&pdata->acc_delay);
			if(copy_to_user(argp, &delay, sizeof(delay)))
			{
				printk(KERN_ERR "SENSOR_GET_DELAY_TIME copy_to_user failed.");
				return -EFAULT;
			}
			break;
		case SENSOR_SET_DELAY_TIME:
			if(copy_from_user(&delay,argp,sizeof(int)));
			{
				printk(KERN_ERR "SENSOR_SET_DELAY_TIME copy_to_user failed.");
				ret = -EFAULT;
			}
			if(pdata->client && delay > 0 && delay <= 500){
				ret = fxos8700_set_odr(pdata->client,FXOS8700_TYPE_ACC,delay);
				if(!ret)
					atomic_set(&pdata->acc_delay,delay);
			}
			break;
		case SENSOR_GET_RAW_DATA:
			position = atomic_read(&pdata->position);
			ret = fxos8700_read_data(pdata->client,&data,FXOS8700_TYPE_ACC);
			if(!ret){
				fxos8700_data_convert(&data,position);
				sdata[0] = data.x;
				sdata[1] = data.y;
				sdata[2] = data.z;
				if(copy_to_user(argp, sdata,sizeof(sdata)))
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

static int fxos8700_acc_open(struct inode *inode, struct file *file)
{
	file->private_data = g_fxos8700_data;
	return nonseekable_open(inode, file);
}

static int fxos8700_acc_release(struct inode *inode, struct file *file)
{
	/* note: releasing the wdt in NOWAYOUT-mode does not stop it */
	return 0;
}

static const struct file_operations fxos8700_acc_fops = {
	.owner = THIS_MODULE,
	.open = fxos8700_acc_open,
	.release = fxos8700_acc_release,
	.unlocked_ioctl = fxos8700_acc_ioctl,
};


//mag char miscdevice
static long fxos8700_mag_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct fxos8700_data *pdata = file->private_data;
	void __user *argp = (void __user *)arg;
    long ret = 0;
	short sdata[3];
	int enable;
	int delay;
	int position;

	struct fxos8700_data_axis data;
	if(!pdata){
		printk(KERN_ERR "fxos8700 struct datt point is NULL.");
		return -EFAULT;
	}
	switch (cmd) {
		case SENSOR_GET_MODEL_NAME:
			if(copy_to_user(argp,"FXOS8700 MAG",strlen("FXOS8700 MAG") +1))
			{
				printk(KERN_ERR "SENSOR_GET_MODEL_NAME copy_to_user failed.");
				ret = -EFAULT;
			}
			break;
		case SENSOR_GET_POWER_STATUS:
			enable = atomic_read(&pdata->mag_active);
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
				ret = fxos8700_change_mode(pdata->client, FXOS8700_TYPE_MAG,
										enable? FXOS8700_ACTIVED : FXOS8700_STANDBY);
				if(!ret)
					atomic_set(&pdata->mag_active,enable);
			}
			break;
		case SENSOR_GET_DELAY_TIME:
			delay = atomic_read(&pdata->mag_delay);
			if(copy_to_user(argp, &delay, sizeof(delay)))
			{
				printk(KERN_ERR "SENSOR_GET_DELAY_TIME copy_to_user failed.");
				return -EFAULT;
			}
			break;
		case SENSOR_SET_DELAY_TIME:
			if(copy_from_user(&delay,argp,sizeof(int)));
			{
				printk(KERN_ERR "SENSOR_SET_DELAY_TIME copy_to_user failed.");
				ret = -EFAULT;
			}
			if(pdata->client && delay > 0 && delay <= 500){
				ret = fxos8700_set_odr(pdata->client,FXOS8700_TYPE_MAG,delay);
				if(!ret)
					atomic_set(&pdata->mag_delay,delay);
			}
			break;
		case SENSOR_GET_RAW_DATA:
			position = atomic_read(&pdata->position);
			ret = fxos8700_read_data(pdata->client,&data,FXOS8700_TYPE_MAG);
			if(!ret){
				fxos8700_data_convert(&data,position);
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

static int fxos8700_mag_open(struct inode *inode, struct file *file)
{
	file->private_data = g_fxos8700_data;
	return nonseekable_open(inode, file);
}

static int fxos8700_mag_release(struct inode *inode, struct file *file)
{
	/* note: releasing the wdt in NOWAYOUT-mode does not stop it */
	return 0;
}

static const struct file_operations fxos8700_mag_fops = {
	.owner = THIS_MODULE,
	.open = fxos8700_mag_open,
	.release = fxos8700_mag_release,
	.unlocked_ioctl = fxos8700_mag_ioctl,
};

static struct miscdevice fxos8700_acc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "FreescaleAccelerometer",
	.fops = &fxos8700_acc_fops,
};

static struct miscdevice fxos8700_mag_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "FreescaleMagnetometer",
	.fops = &fxos8700_mag_fops,
};


static ssize_t fxos8700_enable_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct miscdevice *misc_dev = dev_get_drvdata(dev);
	struct fxos8700_data * pdata = g_fxos8700_data;
    int enable;
	enable = 0;
	if(pdata->acc_miscdev == misc_dev){
		enable = atomic_read(&pdata->acc_active);
	}
	if(pdata->mag_miscdev == misc_dev){
		enable = atomic_read(&pdata->mag_active);
	}
	return sprintf(buf, "%d\n", enable);
}


static ssize_t fxos8700_enable_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct miscdevice *misc_dev = dev_get_drvdata(dev);
	struct fxos8700_data * pdata = g_fxos8700_data;
	struct i2c_client *client = pdata->client;
	int type;
	int enable;
	int ret;
	enable = simple_strtoul(buf, NULL, 10);
	if(misc_dev == pdata->acc_miscdev)
		type = FXOS8700_TYPE_ACC;
	if(misc_dev == pdata->mag_miscdev)
		type = FXOS8700_TYPE_MAG;
	enable = (enable > 0 ? FXOS8700_ACTIVED : FXOS8700_STANDBY);
	ret = fxos8700_change_mode(client,type,enable);
	if(!ret){
		if(type == FXOS8700_TYPE_ACC)
			atomic_set(&pdata->acc_active, enable);
		else
			atomic_set(&pdata->mag_active, enable);
	}
	return count;
}

static ssize_t fxos8700_poll_delay_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct miscdevice *misc_dev = dev_get_drvdata(dev);
	struct fxos8700_data * pdata = g_fxos8700_data;
    int poll_delay = 0;
	if(pdata->acc_miscdev == misc_dev){
		poll_delay = atomic_read(&pdata->acc_delay);
	}
	if(pdata->mag_miscdev == misc_dev){
		poll_delay = atomic_read(&pdata->mag_delay);
	}
	return sprintf(buf, "%d\n", poll_delay);
}


static ssize_t fxos8700_poll_delay_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct miscdevice *misc_dev = dev_get_drvdata(dev);
	struct fxos8700_data * pdata = g_fxos8700_data;
	struct i2c_client *client = pdata->client;
	int type;
	int delay;
	int ret;
	delay = simple_strtoul(buf, NULL, 10);
	if(misc_dev == pdata->acc_miscdev)
		type = FXOS8700_TYPE_ACC;
	if(misc_dev == pdata->mag_miscdev)
		type = FXOS8700_TYPE_MAG;
	ret = fxos8700_set_odr(client,type,delay);
	if(!ret){
		if(type == FXOS8700_TYPE_ACC)
			atomic_set(&pdata->acc_delay,delay);
		else
			atomic_set(&pdata->mag_delay,delay);
	}
	return count;
}

static ssize_t fxos8700_position_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct fxos8700_data * pdata = g_fxos8700_data;
    int position = 0;
    position = atomic_read(&pdata->position) ;
	return sprintf(buf, "%d\n", position);
}

static ssize_t fxos8700_position_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	int  position;
	struct fxos8700_data * pdata = g_fxos8700_data;
	position = simple_strtoul(buf, NULL, 10);
    atomic_set(&pdata->position, position);
	return count;
}

static DEVICE_ATTR(enable, 0666,fxos8700_enable_show, fxos8700_enable_store);

static DEVICE_ATTR(poll_delay,0666,fxos8700_poll_delay_show, fxos8700_poll_delay_store);

static DEVICE_ATTR(position, 0666,fxos8700_position_show, fxos8700_position_store);

static struct attribute *fxos8700_attributes[] = {
	&dev_attr_enable.attr,
	&dev_attr_poll_delay.attr,
	&dev_attr_position.attr,
	NULL
};

static const struct attribute_group fxos8700_attr_group = {
	.attrs = fxos8700_attributes,
};

static int fxos8700_register_sysfs_device(struct fxos8700_data *pdata)
{
	struct miscdevice *misc_dev = NULL;
	int err = -1;
	/*register sysfs for acc*/
	misc_dev = pdata->acc_miscdev;
    err = sysfs_create_group(&misc_dev->this_device->kobj, &fxos8700_attr_group);
	if (err) {
		goto out;
	}

	/*register sysfs for mag*/
	misc_dev = pdata->mag_miscdev;
    err = sysfs_create_group(&misc_dev->this_device->kobj, &fxos8700_attr_group);
	if (err) {
		goto err_register_sysfs;
	}
	return 0;
err_register_sysfs:
   misc_dev = pdata->acc_miscdev;
   sysfs_remove_group(&misc_dev->this_device->kobj,&fxos8700_attr_group);  /*remove accel senosr sysfs*/
   printk("reigster mag sysfs error\n");
out:
	printk("reigster acc sysfs error\n");
	return err;
}
static int fxos8700_unregister_sysfs_device(struct fxos8700_data *pdata)
{
	struct miscdevice *misc_dev = NULL;
    misc_dev =  pdata->acc_miscdev;
    sysfs_remove_group(&misc_dev->this_device->kobj,&fxos8700_attr_group);  /*remove accel senosr sysfs*/

  	misc_dev =  pdata->mag_miscdev;
  	sysfs_remove_group(&misc_dev->this_device->kobj,&fxos8700_attr_group);  /*remove accel senosr sysfs*/
 	return 0;
}

static int fxos8700_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	int result, client_id;
	struct fxos8700_data * pdata;
	struct i2c_adapter *adapter;

	adapter = to_i2c_adapter(client->dev.parent);
	result = i2c_check_functionality(adapter,
					 I2C_FUNC_SMBUS_BYTE |
					 I2C_FUNC_SMBUS_BYTE_DATA);
	if (!result)
		goto err_out;

	client_id = i2c_smbus_read_byte_data(client, FXOS8700_WHO_AM_I);
	if (client_id !=  FXOS8700_DEVICE_ID && client_id != FXOS8700_PRE_DEVICE_ID) {
		dev_err(&client->dev,
			"read chip ID 0x%x is not equal to 0x%x or 0x%x\n",
			result, FXOS8700_DEVICE_ID,FXOS8700_PRE_DEVICE_ID);
		result = -EINVAL;
		goto err_out;
	}
    pdata = kzalloc(sizeof(struct fxos8700_data), GFP_KERNEL);
	if(!pdata){
		result = -ENOMEM;
		dev_err(&client->dev, "alloc data memory error!\n");
		goto err_out;
    }
	g_fxos8700_data = pdata;
	pdata->client = client;
	atomic_set(&pdata->acc_delay, FXOS8700_DELAY_DEFAULT);
	atomic_set(&pdata->mag_delay, FXOS8700_DELAY_DEFAULT);
	i2c_set_clientdata(client,pdata);

	result = misc_register(&fxos8700_acc_device);
	if (result != 0) {
		printk(KERN_ERR "register acc miscdevice error");
		goto err_regsiter_acc_misc;
	}
	pdata->acc_miscdev = &fxos8700_acc_device;

	result = misc_register(&fxos8700_mag_device);
	if (result != 0) {
		printk(KERN_ERR "register acc miscdevice error");
		goto err_regsiter_mag_misc;
	}
	pdata->mag_miscdev = &fxos8700_mag_device;

	result = fxos8700_register_sysfs_device(pdata);
	if (result) {
		dev_err(&client->dev, "create device file failed!\n");
		result = -EINVAL;
		goto err_register_sys;
	}
	fxos8700_device_init(client);
	printk("%s succ\n",__FUNCTION__);
	return 0;
err_register_sys:
	misc_deregister(&fxos8700_mag_device);
	pdata->mag_miscdev = NULL;
err_regsiter_mag_misc:
	misc_deregister(&fxos8700_acc_device);
	pdata->acc_miscdev = NULL;
err_regsiter_acc_misc:
	i2c_set_clientdata(client,NULL);
	kfree(pdata);
err_out:
	return result;
}

static int fxos8700_remove(struct i2c_client *client)
{
	struct fxos8700_data *pdata =  i2c_get_clientdata(client);
	if(!pdata)
		return 0;
    fxos8700_device_stop(client);
	fxos8700_unregister_sysfs_device(pdata);
	misc_deregister(&fxos8700_acc_device);
	misc_deregister(&fxos8700_mag_device);
    kfree(pdata);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int fxos8700_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fxos8700_data *pdata =  i2c_get_clientdata(client);
	if(atomic_read(&pdata->acc_active)|| atomic_read(&pdata->mag_active))
		fxos8700_device_stop(client);
	return 0;
}

static int fxos8700_resume(struct device *dev)
{
    int ret = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct fxos8700_data *pdata =  i2c_get_clientdata(client);
	if(atomic_read(&pdata->acc_active))
		fxos8700_change_mode(client,FXOS8700_TYPE_ACC,FXOS8700_ACTIVED);
	if(atomic_read(&pdata->mag_active))
		fxos8700_change_mode(client,FXOS8700_TYPE_MAG,FXOS8700_ACTIVED);
	return ret;

}
#endif

static const struct i2c_device_id fxos8700_id[] = {
	{"fxos8700", 0},
	{ }
};
MODULE_DEVICE_TABLE(i2c, fxos8700_id);

static SIMPLE_DEV_PM_OPS(fxos8700_pm_ops, fxos8700_suspend, fxos8700_resume);
static struct i2c_driver fxos8700_driver = {
	.driver = {
		   .name = "fxos8700",
		   .owner = THIS_MODULE,
		   .pm = &fxos8700_pm_ops,
		   },
	.probe = fxos8700_probe,
	.remove = fxos8700_remove,
	.id_table = fxos8700_id,
};

module_i2c_driver(fxos8700_driver);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("FXOS8700 6-Axis Acc and Mag Combo Sensor driver");
MODULE_LICENSE("GPL");
