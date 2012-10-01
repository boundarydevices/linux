/*
 *   Boundary Devices FTx06 touch screen controller.
 *
 *   Copyright (c) by Boundary Devices <info@boundarydevices.com>
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
#include <linux/io.h>
#include <mach/hardware.h>
#include <mach/gpio.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/input.h>

#ifdef CONFIG_TOUCHSCREEN_FT5X06_SINGLE_TOUCH
#else
#define USE_ABS_MT
#endif

struct point {
	int	x;
	int	y;
};

struct ft5x06_ts {
	struct i2c_client *client;
	struct input_dev	*idev;
	wait_queue_head_t	sample_waitq;
	struct semaphore	sem;
	struct completion	init_exit;
	struct task_struct	*rtask;
	int			use_count;
	int			bReady;
	int			irq;
	unsigned		gp;
	struct proc_dir_entry  *procentry;
};
static const char *client_name = "ft5x06";

struct ft5x06_ts *gts;

static char const procentryname[] = {
   "ft5x06"
};

static int ts_startup(struct ft5x06_ts *ts);
static void ts_shutdown(struct ft5x06_ts *ts);

static int ft5x06_proc_read
	(char *page,
	 char **start,
	 off_t off,
	 int count,
	 int *eof,
	 void *data)
{
	printk(KERN_ERR "%s\n", __func__);
	return 0 ;
}

static int
ft5x06_proc_write
	(struct file *file,
	 const char __user *buffer,
	 unsigned long count,
	 void *data)
{
	printk(KERN_ERR "%s\n", __func__);
	return count ;
}

/*-----------------------------------------------------------------------*/
static inline void ts_evt_add(struct ft5x06_ts *ts,
			      unsigned buttons, struct point *p)
{
	struct input_dev *idev = ts->idev;
	int i;
	if (!buttons) {
		/* send release to user space. */
#ifdef USE_ABS_MT
		input_event(idev, EV_ABS, ABS_MT_TOUCH_MAJOR, 0);
		input_event(idev, EV_KEY, BTN_TOUCH, 0);
		input_mt_sync(idev);
#else
		input_report_abs(idev, ABS_PRESSURE, 0);
		input_report_key(idev, BTN_TOUCH, 0);
		input_sync(idev);
#endif
	} else {
		for (i = 0; i < buttons; i++) {
#ifdef USE_ABS_MT
			input_event(idev, EV_ABS, ABS_MT_POSITION_X, p[i].x);
			input_event(idev, EV_ABS, ABS_MT_POSITION_Y, p[i].y);
			input_event(idev, EV_ABS, ABS_MT_TOUCH_MAJOR, 1);
			input_mt_sync(idev);
#else
			input_report_abs(idev, ABS_X, p[i].x);
			input_report_abs(idev, ABS_Y, p[i].y);
			input_report_abs(idev, ABS_PRESSURE, 1);
			input_report_key(idev, BTN_TOUCH, 1);
			input_sync(idev);
#endif
		}
		input_event(idev, EV_KEY, BTN_TOUCH, 1);
	}
#ifdef USE_ABS_MT
	input_sync(idev);
#endif
}

static int ts_open(struct input_dev *idev)
{
	struct ft5x06_ts *ts = input_get_drvdata(idev);
	return ts_startup(ts);
}

static void ts_close(struct input_dev *idev)
{
	struct ft5x06_ts *ts = input_get_drvdata(idev);
	ts_shutdown(ts);
}

static inline int ts_register(struct ft5x06_ts *ts)
{
	struct input_dev *idev;
	idev = input_allocate_device();
	if (idev == NULL)
		return -ENOMEM;

	ts->idev = idev;
	idev->name      = procentryname ;
	idev->id.product = ts->client->addr;
	idev->open      = ts_open;
	idev->close     = ts_close;

	__set_bit(EV_ABS, idev->evbit);
	__set_bit(EV_KEY, idev->evbit);
	__set_bit(BTN_TOUCH, idev->keybit);

#ifdef USE_ABS_MT
	input_set_abs_params(idev, ABS_MT_POSITION_X, 0, 1023, 0, 0);
	input_set_abs_params(idev, ABS_MT_POSITION_Y, 0, 0x255, 0, 0);
	input_set_abs_params(idev, ABS_MT_TOUCH_MAJOR, 0, 1, 0, 0);
#else
	__set_bit(EV_SYN, idev->evbit);
	input_set_abs_params(idev, ABS_X, 0, 1023, 0, 0);
	input_set_abs_params(idev, ABS_Y, 0, 0x255, 0, 0);
	input_set_abs_params(idev, ABS_PRESSURE, 0, 1, 0, 0);
#endif

	input_set_drvdata(idev, ts);
	return input_register_device(idev);
}

static inline void ts_deregister(struct ft5x06_ts *ts)
{
	if (ts->idev) {
		input_unregister_device(ts->idev);
		input_free_device(ts->idev);
		ts->idev = NULL;
	}
}

#ifdef DEBUG
static void printHex(u8 const *buf, unsigned len)
{
	char hex[512];
	char *next = hex ;
	char *end = hex+sizeof(hex);

	while (len--) {
		next += snprintf(next, end-next, "%02x", *buf++);
		if (next >= end) {
			hex[sizeof(hex)-1] = '\0' ;
			break;
		}
	}
	printk(KERN_ERR "%s\n", hex);
}
#endif

static void write_reg(struct ft5x06_ts *ts, int regnum, int value)
{
	u8 regnval[] = {
		regnum,
		value
	};
	struct i2c_msg pkt = {
		ts->client->addr, 0, sizeof(regnval), regnval
	};
	int ret = i2c_transfer(ts->client->adapter, &pkt, 1);
	if (ret != 1)
		printk(KERN_WARNING "%s: i2c_transfer failed\n", __func__);
	else
		printk(KERN_DEBUG "%s: set register 0x%02x to 0x%02x\n",
		       __func__, regnum, value);
}

static void set_mode(struct ft5x06_ts *ts, int mode)
{
	write_reg(ts, 0, (mode&7)<<4);
	printk(KERN_DEBUG "%s: changed mode to 0x%02x\n", __func__, mode);
}

#define WORK_MODE 0
#define FACTORY_MODE 4

/*-----------------------------------------------------------------------*/

/*
 * This is a RT kernel thread that handles the I2c accesses
 * The I2c access functions are expected to be able to sleep.
 */
static int ts_thread(void *_ts)
{
	int ret;
	struct point points[5];
	unsigned char buf[33];
	struct ft5x06_ts *ts = _ts;
	unsigned char startch[1] = { 0 };
	struct i2c_msg readpkt[2] = {
		{ts->client->addr, 0, 1, startch},
		{ts->client->addr, I2C_M_RD, sizeof(buf), buf}
	};

	struct task_struct *tsk = current;

	ts->rtask = tsk;

	daemonize("ft5x06tsd");
	/* only want to receive SIGKILL */
	allow_signal(SIGKILL);

	complete(&ts->init_exit);

	do {
		int buttons = 0 ;
		ts->bReady = 0;
		ret = i2c_transfer(ts->client->adapter, readpkt,
				   ARRAY_SIZE(readpkt));
		if (ret != ARRAY_SIZE(readpkt)) {
			printk(KERN_WARNING "%s: i2c_transfer failed\n",
			       client_name);
			msleep(1000);
		} else {
			int i;
			unsigned char *p = buf+3;
#ifdef DEBUG
			printHex(buf, sizeof(buf));
#endif
			buttons = buf[2];
			if (buttons > 5) {
				printk(KERN_ERR
				       "%s: invalid button count %02x\n",
				       __func__, buttons);
				buttons = 0 ;
			} else {
				for (i = 0; i < buttons; i++) {
					points[i].x = ((p[0] << 8)
						       | p[1]) & 0x7ff;
					points[i].y = ((p[2] << 8)
						       | p[3]) & 0x7ff;
					p += 6;
				}
			}
		}

		if (signal_pending(tsk))
			break;
#ifdef DEBUG
		printk(KERN_ERR "%s: buttons = %d, "
				"points[0].x = %d, "
				"points[0].y = %d\n",
		       client_name, buttons, points[0].x, points[0].y);
#endif
		ts_evt_add(ts, buttons, points);
		if (0 < buttons)
			wait_event_interruptible_timeout(ts->sample_waitq,
							 ts->bReady, HZ/20);
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
	complete_and_exit(&ts->init_exit, 0);
}

/*
 * We only detect samples ready with this interrupt
 * handler, and even then we just schedule our task.
 */
static irqreturn_t ts_interrupt(int irq, void *id)
{
	struct ft5x06_ts *ts = id;
	int bit = gpio_get_value(ts->gp);
	if (bit == 0) {
		ts->bReady = 1;
		wmb(); /* flush bReady */
		wake_up(&ts->sample_waitq);
	}
	return IRQ_HANDLED;
}

#define ID_G_THGROUP		0x80
#define ID_G_PERIODMONITOR	0x89
#define FT5X0X_REG_HEIGHT_B	0x8a
#define FT5X0X_REG_MAX_FRAME	0x8b
#define FT5X0X_REG_FEG_FRAME	0x8e
#define FT5X0X_REG_LEFT_RIGHT_OFFSET	0x92
#define FT5X0X_REG_UP_DOWN_OFFSET	0x93
#define FT5X0X_REG_DISTANCE_LEFT_RIGHT	0x94
#define FT5X0X_REG_DISTANCE_UP_DOWN	0x95
#define FT5X0X_REG_MAX_X_HIGH		0x98
#define FT5X0X_REG_MAX_X_LOW		0x99
#define FT5X0X_REG_MAX_Y_HIGH		0x9a
#define FT5X0X_REG_MAX_Y_LOW		0x9b
#define FT5X0X_REG_K_X_HIGH		0x9c
#define FT5X0X_REG_K_X_LOW		0x9d
#define FT5X0X_REG_K_Y_HIGH		0x9e
#define FT5X0X_REG_K_Y_LOW		0x9f

#define ID_G_AUTO_CLB	0xa0
#define ID_G_B_AREA_TH	0xae

#ifdef DEBUG
static void dumpRegs(struct ft5x06_ts *ts, unsigned start, unsigned end)
{
	u8 regbuf[512];
	unsigned char startch[1] = { start };
	int ret ;
	struct i2c_msg readpkt[2] = {
		{ts->client->addr, 0, 1, startch},
		{ts->client->addr, I2C_M_RD, end-start+1, regbuf}
	};
	ret = i2c_transfer(ts->client->adapter, readpkt, ARRAY_SIZE(readpkt));
	if (ret != ARRAY_SIZE(readpkt)) {
		printk(KERN_WARNING "%s: i2c_transfer failed\n", client_name);
	} else {
		printk(KERN_ERR "registers %02x..%02x\n", start, end);
		printHex(regbuf, end-start+1);
	}
}
#endif

static int ts_startup(struct ft5x06_ts *ts)
{
	int ret = 0;
	if (ts == NULL)
		return -EIO;

	if (down_interruptible(&ts->sem))
		return -EINTR;

	if (ts->use_count++ != 0)
		goto out;

	if (ts->rtask)
		panic("ft5x06tsd: rtask running?");

	ret = request_irq(ts->irq, &ts_interrupt, IRQF_TRIGGER_FALLING,
			  client_name, ts);
	if (ret) {
		printk(KERN_ERR "%s: request_irq failed, irq:%i\n",
		       client_name, ts->irq);
		goto out;
	}

#ifdef DEBUG
	set_mode(ts, FACTORY_MODE);
	dumpRegs(ts, 0x4c, 0x4C);
	write_reg(ts, 0x4C, 0x05);
	dumpRegs(ts, 0, 0x4C);
#endif
	set_mode(ts, WORK_MODE);
#ifdef DEBUG
	dumpRegs(ts, 0x3b, 0x3b);
	dumpRegs(ts, 0x6a, 0x6a);
	dumpRegs(ts, ID_G_THGROUP, ID_G_PERIODMONITOR);
	dumpRegs(ts, FT5X0X_REG_HEIGHT_B, FT5X0X_REG_K_Y_LOW);
	dumpRegs(ts, ID_G_AUTO_CLB, ID_G_B_AREA_TH);
#endif
	set_mode(ts, WORK_MODE);

	init_completion(&ts->init_exit);
	ret = kernel_thread(ts_thread, ts, CLONE_KERNEL);
	if (ret >= 0) {
		wait_for_completion(&ts->init_exit);
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
static void ts_shutdown(struct ft5x06_ts *ts)
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
static int detect_ft5x06(struct i2c_client *client)
{
	struct i2c_adapter *adapter = client->adapter;
	char buffer;
	struct i2c_msg pkt = {
		client->addr,
		I2C_M_RD,
		sizeof(buffer),
		&buffer
	};
	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))
		return -ENODEV;
	if (i2c_transfer(adapter, &pkt, 1) != 1)
		return -ENODEV;
	return 0;
}

/* Return 0 if detection is successful, -ENODEV otherwise */
static int ts_detect(struct i2c_client *client,
			  struct i2c_board_info *info)
{
	int err = detect_ft5x06(client);
	if (!err)
		strlcpy(info->type, "ft5x06-ts", I2C_NAME_SIZE);
	return err;
}

static int ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int err = 0;
	struct ft5x06_ts *ts;
	struct device *dev = &client->dev;
	if (gts) {
		printk(KERN_ERR "%s: Error gts is already allocated\n",
		       client_name);
		return -ENOMEM;
	}
	if (detect_ft5x06(client) != 0) {
		dev_err(dev, "%s: Could not detect touch screen.\n",
			client_name);
		return -ENODEV;
	}
	ts = kzalloc(sizeof(struct ft5x06_ts), GFP_KERNEL);
	if (!ts) {
		dev_err(dev, "Couldn't allocate memory for %s\n", client_name);
		return -ENOMEM;
	}
	init_waitqueue_head(&ts->sample_waitq);
	sema_init(&ts->sem, 1);
	ts->client = client;
	ts->irq = client->irq ;
	ts->gp = irq_to_gpio(client->irq);
	printk(KERN_INFO "%s: %s touchscreen irq=%i, gp=%i\n", __func__,
	       client_name, ts->irq, ts->gp);
	i2c_set_clientdata(client, ts);
	err = ts_register(ts);
	if (err == 0) {
		gts = ts;
		ts->procentry = create_proc_entry(procentryname, 0, NULL);
		if (ts->procentry) {
			ts->procentry->read_proc = ft5x06_proc_read ;
			ts->procentry->write_proc = ft5x06_proc_write ;
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
	struct ft5x06_ts *ts = i2c_get_clientdata(client);
	remove_proc_entry(procentryname, 0);
	if (ts == gts) {
		gts = NULL;
		ts_deregister(ts);
	} else {
		printk(KERN_ERR "%s: Error ts!=gts\n", client_name);
	}
	kfree(ts);
	return 0;
}


/*-----------------------------------------------------------------------*/

static const struct i2c_device_id ts_idtable[] = {
	{ "ft5x06-ts", 0 },
	{ }
};

static struct i2c_driver ts_driver = {
	.driver = {
		.owner		= THIS_MODULE,
		.name		= "ft5x06-ts",
	},
	.id_table	= ts_idtable,
	.probe		= ts_probe,
	.remove		= __devexit_p(ts_remove),
	.detect		= ts_detect,
};

static int __init ts_init(void)
{
	int res = i2c_add_driver(&ts_driver);
	if (res) {
		printk(KERN_WARNING "%s: i2c_add_driver failed\n", client_name);
		return res;
	}
	printk(KERN_INFO "%s: " __DATE__ "\n", client_name);
	return 0;
}

static void __exit ts_exit(void)
{
	i2c_del_driver(&ts_driver);
}

MODULE_AUTHOR("Boundary Devices <info@boundarydevices.com>");
MODULE_DESCRIPTION("I2C interface for FocalTech ft5x06 touch screen controller.");
MODULE_LICENSE("GPL");

module_init(ts_init)
module_exit(ts_exit)
