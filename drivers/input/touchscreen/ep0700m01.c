/*
 *   Boundary Devices ep0700m01 touch screen controller.
 *
 *   Copyright (c) by Troy Kisky<troy.kisky@boundarydevices.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <asm/io.h>
#include <mach/hardware.h>
#include <mach/gpio.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/input.h>

static int calibration[7] = {0};
module_param_array(calibration, int, NULL, S_IRUGO | S_IWUSR);

struct point {
	int	x;
	int	y;
};

struct ep0700m01_ts {
	struct i2c_client *client;
	struct input_dev	*idev;
	wait_queue_head_t	sample_waitq;
	struct semaphore	sem;
	struct completion	init_exit;	//thread exit notification
	struct task_struct	*rtask;
	int			use_count;
	int			bReady;
	int			interruptCnt;
	int			touch_count[2];
	struct point		save_points[2];
//#define TESTING
#ifdef TESTING
	struct timeval	lastInterruptTime;
#endif
	int irq;
	unsigned gp;
	struct proc_dir_entry *procentry;
};
static const char *client_name = "ep0700m01";

struct ep0700m01_ts* gts;

static char const procentryname[] = {
   "ep0700m01"
};

static int ts_startup(struct ep0700m01_ts* ts);
static void ts_shutdown(struct ep0700m01_ts* ts);

static int ep0700m01_proc_read
	(char *page,
	 char **start,
	 off_t off,
	 int count,
	 int *eof,
	 void *data)
{
	if (gts) {
		unsigned char regAddr[] = {0};
		unsigned char buf[32];
		struct i2c_msg readReg[2] = {
			{gts->client->addr, 0, 1, regAddr},
			{gts->client->addr, I2C_M_RD, 14, buf}
		};
		int totalWritten = 0 ;
		int rval = i2c_transfer(gts->client->adapter,
				readReg, ARRAY_SIZE(readReg));
		if (ARRAY_SIZE(readReg) == rval) {
			totalWritten = snprintf(page, count,
				"%04x %04x %04x %04x %02x %02x %02x %02x %02x %02x\n",
				(buf[0] << 8) | buf[1], (buf[2] << 8) | buf[3],
				(buf[4] << 8) | buf[5], (buf[6] << 8) | buf[7],
				buf[8], buf[9], buf[10], buf[11],
				buf[12], buf[13]);
		} else
			printk(KERN_ERR "%s: transfer error %d\n",
					__func__, rval);
		*eof = 1 ;
		return totalWritten ;
	} else {
		return -EIO ;
	}
}

static int
ep0700m01_proc_write
	(struct file *file,
	 const char __user *buffer,
	 unsigned long count,
	 void *data)
{
	printk(KERN_ERR "%s\n", __FUNCTION__);
	return count ;
}

/*-----------------------------------------------------------------------*/


//////////////////////////
static inline void ts_evt_add(struct ep0700m01_ts *ts, unsigned buttons, struct point *p)
{
	struct input_dev *idev = ts->idev;
	struct point *q = p;
	int i;
	if (buttons > 2)
		buttons = 2;
	if (calibration[6]) {
		for (i = 0; i < buttons; i++) {
			int tmpx, tmpy ;
			tmpx = calibration[0] * p->x + calibration[1] * p->y + calibration[2];
			tmpx /= calibration[6];
			if (tmpx < 0)
				tmpx = 0;
			tmpy = calibration[3] * p->x + calibration[4] * p->y + calibration[5];
			tmpy /= calibration[6];
			if (tmpy < 0)
				tmpy = 0;
			p->x = tmpx; p->y = tmpy;
			p++;
		}
	}
	if (buttons < 2)
		ts->touch_count[1] = 0;
	if (!buttons) {
		/* send release to user space. */
		input_event(idev, EV_ABS, ABS_MT_TOUCH_MAJOR, 0);
		ts->touch_count[0] = 0;
		input_mt_sync(idev);
	} else {
		for (i = 0; i < buttons; i++) {
			if (0 < ts->touch_count[i]++) {
				input_event(idev, EV_ABS, ABS_MT_POSITION_X, ts->save_points[i].x);
				input_event(idev, EV_ABS, ABS_MT_POSITION_Y, ts->save_points[i].y);
				input_event(idev, EV_ABS, ABS_MT_TOUCH_MAJOR, 1);
				input_mt_sync(idev);
			}
			ts->save_points[i] = q[i];
		}
	}
	input_sync(idev);
}

static int ts_open(struct input_dev *idev)
{
	struct ep0700m01_ts *ts = input_get_drvdata(idev);
	return ts_startup(ts);
}

static void ts_close(struct input_dev *idev)
{
	struct ep0700m01_ts *ts = input_get_drvdata(idev);
	ts_shutdown(ts);
}

static inline int ts_register(struct ep0700m01_ts *ts)
{
	struct input_dev *idev;
	idev = input_allocate_device();
	if (idev==NULL) {
		return -ENOMEM;
	}
	ts->idev = idev;
	idev->name      = procentryname ;
	idev->id.product = ts->client->addr;
	idev->open      = ts_open;
	idev->close     = ts_close;

	__set_bit(EV_ABS, idev->evbit);
	__set_bit(EV_KEY, idev->evbit);
	__set_bit(BTN_TOUCH, idev->keybit);

	input_set_abs_params(idev, ABS_MT_POSITION_X, 0, 2047, 0, 0);
	input_set_abs_params(idev, ABS_MT_POSITION_Y, 0, 2047, 0, 0);
	input_set_abs_params(idev, ABS_MT_TOUCH_MAJOR, 0, 1, 0, 0);

	input_set_drvdata(idev, ts);
	return input_register_device(idev);
}

static inline void ts_deregister(struct ep0700m01_ts *ts)
{
	if (ts->idev) {
		input_unregister_device(ts->idev);
		input_free_device(ts->idev);
		ts->idev = NULL;
	}
}

/*-----------------------------------------------------------------------*/

/*
 * This is a RT kernel thread that handles the I2c accesses
 * The I2c access functions are expected to be able to sleep.
 */
static int ts_thread(void *_ts)
{
	int ret;
	struct point points[2];
	int buttons;
	unsigned char buf[32];
	struct ep0700m01_ts *ts = _ts;
	unsigned char sumXReg[1] = { 0 };
	struct i2c_msg readSums[2] = {
		{ts->client->addr, 0, 1, sumXReg},
		{ts->client->addr, I2C_M_RD, 10, buf}
	};

	struct task_struct *tsk = current;

	ts->rtask = tsk;

	daemonize("ep0700m01tsd");
	/* only want to receive SIGKILL */
	allow_signal(SIGKILL);

	/*
	 * We could run as a real-time thread.  However, thus far
	 * this doesn't seem to be necessary.
	 */
//	tsk->policy = SCHED_FIFO;
//	tsk->rt_priority = 1;

	complete(&ts->init_exit);

	ts->interruptCnt=0;

	do {
#ifdef TESTING
		printk(KERN_ERR "(i2cs)\n");
//		printk(KERN_ERR "%s: reading from device 0x%x\n",client_name,ts->client.addr);
		do_gettimeofday(&ts->lastInterruptTime);
#endif
		ts->bReady = 0;
		/* For the 1st access and after a release, make sure the initial register address is selected */
		ret = i2c_transfer(ts->client->adapter, readSums, 2);
		if (ret != 2) {
			printk(KERN_WARNING "%s: i2c_transfer failed\n", client_name);
			msleep(1000);
			buttons = 0;
		} else {
			int i;
			unsigned char *p = buf;
			buttons = p[9];
			if (buttons > 2)
				buttons = 2;
			for (i = 0; i < buttons; i++) {
				points[i].x = ((p[0] << 8) | p[1]) & 0x7ff;
				points[i].y = ((p[2] << 8) | p[3]) & 0x7ff;
				p += 4;
			}
		}

		if (signal_pending(tsk))
			break;
#ifdef TESTING
		printk(KERN_ERR "%s: buttons = %d, points[0].x = %d, points[0].y = %d\n", client_name, buttons, points[0].x, points[0].y);
#endif
		ts_evt_add(ts, buttons, points);
		if (buttons)
			wait_event_interruptible_timeout(ts->sample_waitq, ts->bReady, HZ/20);
		else
			wait_event_interruptible(ts->sample_waitq, ts->bReady);
		if (gpio_get_value(ts->gp)) {
			if (buttons) {
				buttons = 0;
				ts_evt_add(ts, buttons, points);
			}
			if (signal_pending(tsk))
				break;
		}
	} while (1);

	ts->rtask = NULL;
//	printk(KERN_ERR "%s: ts_thread exiting\n",client_name);
	complete_and_exit(&ts->init_exit, 0);
}

/*
 * We only detect samples ready with this interrupt
 * handler, and even then we just schedule our task.
 */
static irqreturn_t ts_interrupt(int irq, void *id)
{
	struct ep0700m01_ts *ts = id;
	int bit = gpio_get_value(ts->gp);
	ts->interruptCnt++;
	if (bit==0) {
		ts->bReady=1;
		wmb();
		wake_up(&ts->sample_waitq);
	}
#ifdef TESTING
	{
		suseconds_t     tv_usec = ts->lastInterruptTime.tv_usec;
		int delta;
		do_gettimeofday(&ts->lastInterruptTime);
		delta = ts->lastInterruptTime.tv_usec - tv_usec;
		if (delta<0) delta += 1000000;
		printk(KERN_WARNING "(delta=%ius gp%i=%i)\n",delta, ts->gp, bit);
	}
#endif
	return IRQ_HANDLED;
}

static int ts_startup(struct ep0700m01_ts* ts)
{
	int ret = 0;
	if (ts==NULL) return -EIO;

	if (down_interruptible(&ts->sem))
		return -EINTR;

	if (ts->use_count++ != 0)
		goto out;

	if (ts->rtask)
		panic("ep0700m01tsd: rtask running?");

	ret = request_irq(ts->irq, &ts_interrupt, IRQF_TRIGGER_FALLING, client_name, ts);
	if (ret) {
		printk(KERN_ERR "%s: request_irq failed, irq:%i\n", client_name,ts->irq);
		goto out;
	}

	init_completion(&ts->init_exit);
	ret = kernel_thread(ts_thread, ts, CLONE_KERNEL);
	if (ret >= 0) {
		wait_for_completion(&ts->init_exit);	//wait for thread to Start
		ret = 0;
	} else {
		free_irq(ts->irq, ts);
	}

 out:
	if (ret)
		ts->use_count--;
	up(&ts->sem);
	return ret;
}

/*
 * Release touchscreen resources.  Disable IRQs.
 */
static void ts_shutdown(struct ep0700m01_ts* ts)
{
	if (ts) {
		down(&ts->sem);
		if (--ts->use_count == 0) {
			if (ts->rtask) {
				send_sig(SIGKILL, ts->rtask, 1);
				wait_for_completion(&ts->init_exit);
			}
			free_irq(ts->irq, ts);
		}
		up(&ts->sem);
	}
}
/*-----------------------------------------------------------------------*/

/* Return 0 if detection is successful, -ENODEV otherwise */
static int ts_detect(struct i2c_client *client,
			  struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = client->adapter;
	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))
		return -ENODEV;
	strlcpy(info->type, "ep0700m01-ts", I2C_NAME_SIZE);
	return 0;
}
struct plat_i2c_generic_data {
	unsigned irq;
	unsigned gp;
};

static int ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int err = 0;
	struct ep0700m01_ts* ts;
	struct device *dev = &client->dev;
	struct plat_i2c_generic_data *plat = client->dev.platform_data;
	if (gts) {
		printk(KERN_ERR "%s: Error gts is already allocated\n",client_name);
		return -ENOMEM;
	}
	ts = kzalloc(sizeof(struct ep0700m01_ts),GFP_KERNEL);
	if (!ts) {
		dev_err(dev, "Couldn't allocate memory for %s\n", client_name);
		return -ENOMEM;
	}
	init_waitqueue_head(&ts->sample_waitq);
	init_MUTEX(&ts->sem);
	ts->client = client;
	ts->irq = plat->irq;
	ts->gp = plat->gp;
	printk(KERN_INFO "%s: %s touchscreen irq=%i, gp=%i\n", __func__, client_name, ts->irq, ts->gp);
	i2c_set_clientdata(client, ts);
	err = ts_register(ts);
	if (err==0) {
		gts = ts;
		ts->procentry = create_proc_entry(procentryname, 0, NULL);
		if (ts->procentry) {
			ts->procentry->read_proc = ep0700m01_proc_read ;
			ts->procentry->write_proc = ep0700m01_proc_write ;
		}
	} else {
		printk(KERN_WARNING "%s: ts_register failed\n", client_name);
		ts_deregister(ts);
		kfree(ts);
	}
	return err;
}

static int ts_remove(struct i2c_client *client)
{
	struct ep0700m01_ts* ts = i2c_get_clientdata(client);
	if (ts==gts) {
		gts = NULL;
		ts_deregister(ts);
	} else {
		printk(KERN_ERR "%s: Error ts!=gts\n",client_name);
	}
	kfree(ts);
	return 0;
}


/*-----------------------------------------------------------------------*/

static const struct i2c_device_id ts_idtable[] = {
	{ "ep0700m01-ts", 0 },
	{ }
};

static struct i2c_driver ts_driver = {
	.driver = {
		.owner		= THIS_MODULE,
		.name		= "ep0700m01-ts",
	},
	.id_table	= ts_idtable,
	.probe		= ts_probe,
	.remove		= __devexit_p(ts_remove),
	.detect		= ts_detect,
};

static int __init ts_init(void)
{
	int res;
	if ((res = i2c_add_driver(&ts_driver))) {
		printk(KERN_WARNING "%s: i2c_add_driver failed\n",client_name);
		return res;
	}
	printk("%s: version 1.0 (March. 14, 2011)\n",client_name);
	return 0;
}

static void __exit ts_exit(void)
{
	i2c_del_driver(&ts_driver);
}

MODULE_AUTHOR("Troy Kisky <troy.kisky@boundarydevices.com>");
MODULE_DESCRIPTION("I2C interface for Emerging Display Technologies ep0700m01 touch screen controller.");
MODULE_LICENSE("GPL");

module_init(ts_init)
module_exit(ts_exit)
