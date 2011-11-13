/*
 *  mma7660.c - Linux kernel modules for 3-Axis Orientation/Motion
 *  Detection Sensor
 *
 *  Copyright (C) 2011 Freescale Semiconductor, Inc. All Rights Reserved.
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
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/hwmon.h>
#include <linux/input-polldev.h>
#include <linux/gpio.h>

//#define TESTING
/*
 * Defines
 */
#define assert(expr)\
	if (!(expr)) {\
		printk(KERN_ERR "Assertion failed! %s,%d,%s,%s\n",\
			__FILE__, __LINE__, __func__, #expr);\
	}

#define DRV_NAME	"mma7660"
static const char *client_name = DRV_NAME;

#define INPUT_FUZZ	2
#define INPUT_FLAT	2

struct mma7660_accel
{
	struct i2c_client	*client;
	struct device		*hwmon_dev;
	struct input_dev	*idev;
	wait_queue_head_t	sample_waitq;
	struct semaphore	sem;
	struct completion	init_exit;	//thread exit notification
	struct task_struct	*rtask;
	int			use_count;
	int			bReady;
	int			interruptCnt;
	int			irq;
	unsigned		gp;
#ifdef TESTING
	struct timeval	lastInterruptTime;
#endif
};

/* register enum for mma7660 registers */
enum {
	MMA7660_XOUT,	/* Bit 0:5 - 6 bit field,  Bit 6 alert*/
	MMA7660_YOUT,
	MMA7660_ZOUT,
	MMA7660_TILT,
	MMA7660_SRST,	/* Sampling Rate Status */
	MMA7660_SPCNT,	/* Sleep Count */
	MMA7660_INTSU,	/* Interrupt setup*/
	MMA7660_MODE,
	MMA7660_SR,	/* Sample rate */
	MMA7660_PDET,	/* Tap Detection */
	MMA7660_PD,	/* Tap Debounce Count */
};

#define MODE_STANDBY 0x40
#define MODE_ACTIVE 0x41
#define MODE_TEST 0x44


static unsigned char cmd_off[] = {
		MMA7660_MODE, MODE_STANDBY,
		MMA7660_INTSU, 0x00,	/* Interrupt setup*/
		0
};
static unsigned char cmd_on[] = {
		MMA7660_MODE, MODE_STANDBY,
		MMA7660_SPCNT, 0xff,	/* Sleep Count */
		MMA7660_INTSU, 0x10,	/* Interrupt setup*/
		MMA7660_SR, 0x1,	/* Sample rate */
		MMA7660_PDET, 0x0,	/* Tap Detection */
		MMA7660_PD, 0x0,	/* Tap Debounce Count */
		MMA7660_MODE, MODE_ACTIVE,
		0
};
/*
 * Initialization function
 */
static int mma7660_send_cmds(struct i2c_client *client, unsigned char *p)
{
	int result;
	while (*p) {
		result = i2c_smbus_write_byte_data(client, p[0], p[1]);
		if (result) {
			pr_err("%s: failed(%i) %x=%x\n", __func__, result, p[0], p[1]);
			return result;
		}
		p += 2;
	}
	return result;
}

/*
 * read sensor data from mma7660
 */
static int mma7660_read_data(struct i2c_client *client, short *x, short *y, short *z)
{
	u8 tmp[4];

	do {
		if (i2c_smbus_read_i2c_block_data
				(client, MMA7660_XOUT, 3, tmp) < 3) {
			dev_err(&client->dev, "i2c block read failed\n");
			return -EIO;
		}
	} while ((tmp[0] | tmp[1] | tmp[2]) & (1 << 6));

	/* val * 0.04687g */
	*x = (short)(((int)(tmp[0] << 26)) >> 26);
	*y = (short)(((int)(tmp[1] << 26)) >> 26);
	*z = (short)(((int)(tmp[2] << 26)) >> 26);
	return 0;
}

static void report_abs(struct mma7660_accel *mma)
{
	short x, y, z;

//	printk(KERN_ERR "%s\n", __func__);
	if (mma7660_read_data(mma->client, &x, &y, &z) != 0)
		return;

	input_report_abs(mma->idev, ABS_X, x);
	input_report_abs(mma->idev, ABS_Y, y);
	input_report_abs(mma->idev, ABS_Z, z);
	input_sync(mma->idev);
//	printk(KERN_ERR "%s sync\n", __func__);
}

/*
 * This is a RT kernel thread that handles the I2c accesses
 * The I2c access functions are expected to be able to sleep.
 */
static int mma_thread(void *_mma)
{
	struct mma7660_accel *mma = _mma;
	struct task_struct *tsk = current;

	mma->rtask = tsk;

	daemonize("mma7660d");
	/* only want to receive SIGKILL */
	allow_signal(SIGKILL);

	/*
	 * We could run as a real-time thread.  However, thus far
	 * this doesn't seem to be necessary.
	 */
//	tsk->policy = SCHED_FIFO;
//	tsk->rt_priority = 1;

	complete(&mma->init_exit);

	mma->interruptCnt=0;
	mma7660_send_cmds(mma->client, cmd_on);

	do {
#ifdef TESTING
		msleep(20);
		printk(KERN_ERR "(mma7660)\n");
//		printk(KERN_ERR "%s: reading from device 0x%x\n",client_name,mma->client.addr);
		do_gettimeofday(&mma->lastInterruptTime);
#endif
		mma->bReady = 0;
		report_abs(mma);
		if (signal_pending(tsk))
			break;
		wait_event_interruptible(mma->sample_waitq, mma->bReady);
		if (signal_pending(tsk))
			break;
	} while (1);

	mma7660_send_cmds(mma->client, cmd_off);
	mma->rtask = NULL;
//	printk(KERN_ERR "%s: ts_thread exiting\n",client_name);
	complete_and_exit(&mma->init_exit, 0);
}

/*
 * We only detect samples ready with this interrupt
 * handler, and even then we just schedule our task.
 */
static irqreturn_t mma_interrupt(int irq, void *id)
{
	struct mma7660_accel *mma = id;
	int bit;
//	printk(KERN_ERR "%s\n", __func__);
	{
		unsigned gp = mma->gp;
#if 1
		bit = gpio_get_value(gp);
#else
		struct gpio_controller  *__iomem g = __gpio_to_controller(gp);
		bit = (__raw_readl(&g->in_data) >> (gp&0x1f))&1;
#endif
	}
	mma->interruptCnt++;
	if (bit==0) {
		mma->bReady=1;
		wmb();
		wake_up(&mma->sample_waitq);
	}
#ifdef TESTING
	{
		suseconds_t     tv_usec = mma->lastInterruptTime.tv_usec;
		int delta;
		do_gettimeofday(&mma->lastInterruptTime);
		delta = mma->lastInterruptTime.tv_usec - tv_usec;
		if (delta<0) delta += 1000000;
		printk(KERN_WARNING "(delta=%ius gp%i=%i)\n",delta, mma->gp, bit);
	}
#endif
	return IRQ_HANDLED;
}

static int mma_startup(struct mma7660_accel* mma)
{
	int ret = 0;
	if (!mma)
		return -EIO;

	if (down_interruptible(&mma->sem))
		return -EINTR;

	if (mma->use_count++ != 0)
		goto out;

	if (mma->rtask)
		panic("mma7660d: rtask running?");

	ret = request_irq(mma->irq, &mma_interrupt, IRQF_TRIGGER_FALLING, client_name, mma);
	if (ret) {
		printk(KERN_ERR "%s: request_irq failed, irq:%i\n", client_name,mma->irq);
		goto out;
	}

	init_completion(&mma->init_exit);
	ret = kernel_thread(mma_thread, mma, CLONE_KERNEL);
	if (ret >= 0) {
		wait_for_completion(&mma->init_exit);	//wait for thread to Start
		ret = 0;
	} else {
		free_irq(mma->irq, mma);
	}

 out:
	if (ret)
		mma->use_count--;
	up(&mma->sem);
	return ret;
}

/*
 * Release accelerometer resources.  Disable IRQs.
 */
static void mma_shutdown(struct mma7660_accel *mma)
{
	if (mma) {
		down(&mma->sem);
		if (--mma->use_count == 0) {
			if (mma->rtask) {
				send_sig(SIGKILL, mma->rtask, 1);
				wait_for_completion(&mma->init_exit);
			}
			free_irq(mma->irq, mma);
		}
		up(&mma->sem);
	}
}

static int mma_open(struct input_dev *idev)
{
	struct mma7660_accel *mma = input_get_drvdata(idev);
	return mma_startup(mma);
}

static void mma_close(struct input_dev *idev)
{
	struct mma7660_accel *mma = input_get_drvdata(idev);
	mma_shutdown(mma);
}

static int __devinit mma_init(struct mma7660_accel *mma, struct i2c_client *client)
{
	struct device *hwmon_dev;
	struct input_dev *idev;
	int result;
	/* Initialize the MMA7660 chip */
	result = mma7660_send_cmds(client, cmd_off);
	if (result) {
		dev_err(&client->dev, "init failed\n");
		return result;
	}

	hwmon_dev = hwmon_device_register(&client->dev);
	if (IS_ERR(hwmon_dev)) {
		dev_err(&client->dev, "hwmon_device_register failed\n");
		return (int)hwmon_dev;
	}
	mma->hwmon_dev = hwmon_dev;
	dev_info(&client->dev, "build time %s %s\n", __DATE__, __TIME__);

	/*input poll device register */
	idev = input_allocate_device();
	if (!idev) {
		dev_err(&client->dev, "alloc  device failed!\n");
		return -ENOMEM;
	}
	mma->idev = idev;
	idev->name = DRV_NAME;
	idev->dev.parent = &client->dev;
	idev->id.bustype = BUS_I2C;
	idev->id.product = mma->client->addr;
	idev->open = mma_open;
	idev->close = mma_close;
	idev->evbit[0] = BIT_MASK(EV_ABS);
	input_set_abs_params(idev, ABS_X, -32, 31, INPUT_FUZZ, INPUT_FLAT);
	input_set_abs_params(idev, ABS_Y, -32, 31, INPUT_FUZZ, INPUT_FLAT);
	input_set_abs_params(idev, ABS_Z, -32, 31, INPUT_FUZZ, INPUT_FLAT);
	input_set_drvdata(idev, mma);
	return input_register_device(idev);
}

static void mma_deinit(struct mma7660_accel *mma)
{
	if (mma->hwmon_dev) {
		hwmon_device_unregister(mma->hwmon_dev);
		mma->hwmon_dev = NULL;
	}
	if (mma->idev) {
		input_unregister_device(mma->idev);
		input_free_device(mma->idev);
		mma->idev = NULL;
	}
}

static ssize_t mma7660_reg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	u8 tmp[16];
	struct mma7660_accel *mma = dev_get_drvdata(dev);

	if (i2c_smbus_read_i2c_block_data(mma->client, 0, 11, tmp) < 11) {
			dev_err(dev, "i2c block read failed\n");
			return -EIO;
	}
	return sprintf(buf, "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n", tmp[0], tmp[1], tmp[2], tmp[3], tmp[4],
			tmp[5], tmp[6], tmp[7], tmp[8], tmp[9], tmp[10]);
}

static DEVICE_ATTR(mma7660_reg, 0444, mma7660_reg_show, NULL);

struct plat_i2c_generic_data {
	unsigned irq;
	unsigned gp;
};

/*
 * I2C init/probing/exit functions
 */
static int __devinit mma7660_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	int result;
	struct mma7660_accel *mma;
	struct i2c_adapter *adapter;
	struct plat_i2c_generic_data *plat = client->dev.platform_data;

	if (!plat) {
		dev_err(&client->dev, "no platform data\n");
		return -EINVAL;
	}
	adapter = to_i2c_adapter(client->dev.parent);

	result = i2c_check_functionality(adapter,
					 I2C_FUNC_SMBUS_BYTE |
					 I2C_FUNC_SMBUS_BYTE_DATA);
	if (!result) {
		dev_err(&client->dev, "i2c_check_functionality failed\n");
		return -ENODEV;
	}

	mma = kzalloc(sizeof(struct mma7660_accel),GFP_KERNEL);
	if (!mma) {
		dev_err(&client->dev, "Couldn't allocate memory\n");
		return -ENOMEM;
	}
	init_waitqueue_head(&mma->sample_waitq);
	init_MUTEX(&mma->sem);
	mma->client = client;
	mma->irq = plat->irq;
	mma->gp = plat->gp;
	printk(KERN_INFO "%s: mma7660 irq=%i gp=%i\n", __func__, mma->irq, mma->gp);

	i2c_set_clientdata(client, mma);
	result = mma_init(mma, client);
	if (result) {
		mma_deinit(mma);
		kfree(mma);
	} else {
		int ret = device_create_file(&client->dev, &dev_attr_mma7660_reg);
		if (ret < 0)
			printk(KERN_WARNING "failed to add mma7660 sysfs files\n");

	}
	return result;
}

static int __devexit mma7660_remove(struct i2c_client *client)
{
	int result;
	struct mma7660_accel *mma = i2c_get_clientdata(client);
	result = i2c_smbus_write_byte_data(client, MMA7660_MODE, MODE_STANDBY);
	assert(result == 0);
	device_remove_file(&client->dev, &dev_attr_mma7660_reg);
	if (mma) {
		mma_deinit(mma);
		kfree(mma);
	}
	return result;
}

static int mma7660_suspend(struct i2c_client *client, pm_message_t mesg)
{
	int result;
	result = i2c_smbus_write_byte_data(client, MMA7660_MODE, MODE_STANDBY);
	assert(result == 0);
	return result;
}

static int mma7660_resume(struct i2c_client *client)
{
	int result;
	result = i2c_smbus_write_byte_data(client, MMA7660_MODE, MODE_ACTIVE);
	assert(result == 0);
	return result;
}

static const struct i2c_device_id mma7660_id[] = {
	{DRV_NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, mma7660_id);

static struct i2c_driver mma7660_driver = {
	.driver = {
		   .name = DRV_NAME,
		   .owner = THIS_MODULE,
		   },
	.suspend = mma7660_suspend,
	.resume = mma7660_resume,
	.probe = mma7660_probe,
	.remove = __devexit_p(mma7660_remove),
	.id_table = mma7660_id,
};

static int __init mma7660_init(void)
{
	/* register driver */
	int res;

	res = i2c_add_driver(&mma7660_driver);
	if (res < 0) {
		printk(KERN_INFO "add mma7660 i2c driver failed\n");
		return -ENODEV;
	}
	printk(KERN_INFO "add mma7660 i2c driver\n");

	return res;
}

static void __exit mma7660_exit(void)
{
	printk(KERN_INFO "remove mma7660 i2c driver.\n");
	i2c_del_driver(&mma7660_driver);
}

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("MMA7660 3-Axis Orientation/Motion Detection Sensor driver");
MODULE_LICENSE("GPL");

module_init(mma7660_init);
module_exit(mma7660_exit);
