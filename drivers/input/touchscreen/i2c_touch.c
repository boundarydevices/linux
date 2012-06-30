/*
 *   Boundary Devices PIC16F616 touch screen controller.
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
/*
 * Define this if you want to talk to the input layer
 */
//#undef CONFIG_INPUT
#ifdef CONFIG_INPUT
#define USE_INPUT
#else
#undef USE_INPUT
#endif

static char const * const touch_type_names[] = {
   "Unknown"
,  "4-wire resistive"
,  "5-wire resistive"
};

static int calibration[7] = {0};
module_param_array(calibration, int, NULL, S_IRUGO | S_IWUSR);

#ifndef USE_INPUT
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>
struct ts_event {
	u16		pressure;
	u16		x;
	u16		y;
	u16		pad;
	struct timeval	stamp;
};
#define NR_EVENTS	16

#else
#include <linux/input.h>
#endif

struct pic16f616_ts {
	struct i2c_client *client;
#ifdef USE_INPUT
	struct input_dev	*idev;
#endif
#ifndef USE_INPUT
	struct fasync_struct	*fasync;
	wait_queue_head_t	read_wait;
	u8			evt_head;
	u8			evt_tail;
	struct ts_event		events[NR_EVENTS];
#endif
	wait_queue_head_t	sample_waitq;
	struct semaphore	sem;
	struct completion	init_exit;	//thread exit notification
	struct task_struct	*rtask;
	int			use_count;
	int		bReady;
	int		interruptCnt;
	int			touch_count ;
	int			save_x ;
	int			save_y ;
	int			save_pressure ;
//#define TESTING
#ifdef TESTING
	struct timeval	lastInterruptTime;
#endif
	int irq;
	unsigned gp;
	struct proc_dir_entry *procentry;
	struct proc_dir_entry *tstype_procentry;
};
static const char *client_name = "Pic16F616-ts";

static unsigned char sample_shift;

static unsigned char combine_samples = CONFIG_TOUCHSCREEN_PIC_COMBINE;
module_param(combine_samples, byte, S_IRUGO | S_IWUSR);

static unsigned char drop_samples = CONFIG_TOUCHSCREEN_PIC_DROP;
module_param(drop_samples, byte, S_IRUGO | S_IWUSR);

#define PULLUP_DELAY_DEFAULT 80
static unsigned char pullup_delay = PULLUP_DELAY_DEFAULT;
module_param(pullup_delay, byte, S_IRUGO | S_IWUSR);

#define DRIVE_DELAY_DEFAULT 40
static unsigned char drive_delay = DRIVE_DELAY_DEFAULT;		/* wait for large touchscreen area to stablize before */
module_param(drive_delay, byte, S_IRUGO | S_IWUSR);		/* starting sampling, default of 40 == 32 uS */

#define SAMPLE_DELAY_DEFAULT 10
static unsigned char sample_delay = SAMPLE_DELAY_DEFAULT;	/* delay between samples, wait at least 7.67 uS */
module_param(sample_delay, byte, S_IRUGO | S_IWUSR);

#define MIN_X		0x20
#define MAX_X		0x22
#define MIN_Y		0x24
#define MAX_Y		0x26
#define SUM_X		0x28
#define SUM_Y		0x2b
#define SAMPLE_CNT 	0x30
#define X_IGND		0x31
#define DRIVE_DELAY	0x37
#define SAMPLE_DELAY	0x38
#define PULLUP_DELAY	0x39

#define SAMPLE_MASK	0x3fffff
#define MAX_SAMPLE_VAL	1023

struct pic16f616_ts* gts;

static char const procentryname[] = {
   "pic16f616"
};

static int ts_startup(struct pic16f616_ts* ts);
static void ts_shutdown(struct pic16f616_ts* ts);

static int tstype_read_proc
(	char *page,
	char **start,
	off_t off,
        int count,
	int *eof,
	void *data )
{
	if (gts) {
		int tstype=0;
		unsigned char sumXReg[1] = { SUM_X };
		unsigned char regAddr[] = { X_IGND};
		unsigned char buf[32];
		struct i2c_msg readReg[] = {
			{gts->client->addr, 0, 1, regAddr},
			{gts->client->addr, I2C_M_RD, 1, buf},
			{gts->client->addr, 0, 1, sumXReg}
		};
		int totalWritten = 0 ;
		if  (0 < count) {
			int rval = i2c_transfer(gts->client->adapter,
						readReg,
						ARRAY_SIZE(readReg));
			if (ARRAY_SIZE(readReg) == rval) {
				if (buf[0]==0x80)
					tstype = 1;	/* 4 wire */
				else if (buf[0]==0x06)
					tstype = 2;	/* 5 wire */
			} else
				printk(KERN_ERR "%s: transfer error %d\n",
					__func__, rval);
			totalWritten = snprintf(page, count, "%s\n",
					touch_type_names[tstype]);
			if (totalWritten < 0) {
				totalWritten = 0;
			}
		}
		*eof = 1 ;
		return totalWritten ;
	} else {
		return -EIO ;
	}
}

static int pic16f616_proc_read
	(char *page,
	 char **start,
	 off_t off,
	 int count,
	 int *eof,
	 void *data)
{
	if (gts) {
		unsigned char regAddr[] = { MIN_X};
		unsigned char buf[32];
		struct i2c_msg readReg[2] = {
			{gts->client->addr, 0, 1, regAddr},
			{gts->client->addr, I2C_M_RD, 2, buf}
		};
		int totalWritten = 0 ;
		printk(KERN_ERR "%s\n", __FUNCTION__);
		while ((0 < count) && (PULLUP_DELAY >= regAddr[0])) {
			int rval = i2c_transfer(gts->client->adapter,
						readReg,
						ARRAY_SIZE(readReg));
			if (ARRAY_SIZE(readReg) == rval) {
				int written = snprintf(page, count,
						       "%02x: %02x %02x\n",
						       regAddr[0],
						       buf[0],
						       buf[1]);
				if (0 <= written) {
					count -= written ;
					page += written ;
					totalWritten += written ;
					regAddr[0] += 2 ;
					continue;
				}
			} else
				printk(KERN_ERR "%s: transfer error %d\n",
					__func__, rval);
			/* continue from middle */
			break;
		}
		*eof = 1 ;
		return totalWritten ;
	} else {
		return -EIO ;
	}
}

static int
pic16f616_proc_write
	(struct file *file,
	 const char __user *buffer,
	 unsigned long count,
	 void *data)
{
	printk(KERN_ERR "%s\n", __FUNCTION__);
	return count ;
}

/*-----------------------------------------------------------------------*/
#ifndef USE_INPUT

#define ts_evt_pending(ts)	((volatile u8)(ts)->evt_head != (ts)->evt_tail)
#define ts_evt_get(ts)		((ts)->events + (ts)->evt_tail)
#define ts_evt_pull(ts)		((ts)->evt_tail = ((ts)->evt_tail + 1) & (NR_EVENTS - 1))
#define ts_evt_clear(ts)	((ts)->evt_head = (ts)->evt_tail = 0)

static inline void ts_evt_add(struct pic16f616_ts *ts, u16 pressure, u16 x, u16 y)
{
	int next_head;

	do {
		rmb();
		next_head = (ts->evt_head + 1) & (NR_EVENTS - 1);
		if (next_head != ts->evt_tail) break;
		wake_up_interruptible(&ts->read_wait);
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(HZ / 20);
	} while (1);


	ts->events[ts->evt_head].pressure = pressure;
	ts->events[ts->evt_head].x = x;
	ts->events[ts->evt_head].y = y;
	do_gettimeofday(&ts->events[ts->evt_head].stamp);
	ts->evt_head = next_head;

	if (ts->fasync) kill_fasync(&ts->fasync, SIGIO, POLL_IN);
	wake_up_interruptible(&ts->read_wait);
}

static inline void ts_event_release(struct pic16f616_ts *ts)
{
	ts_evt_add(ts, 0, 0, 0);
}

/*
 * User space driver interface.
 */
static ssize_t picts_read(struct file *filp, char *buffer, size_t count,
		loff_t *ppos)
{
	DECLARE_WAITQUEUE(wait, current);
	struct pic16f616_ts *ts = filp->private_data;
	char *ptr = buffer;
	int err = 0;

	add_wait_queue(&ts->read_wait, &wait);
	while (count >= sizeof(struct ts_event)) {
		err = -ERESTARTSYS;
		if (signal_pending(current))
			break;

		if (ts_evt_pending(ts)) {
			struct ts_event *evt = ts_evt_get(ts);

			err = copy_to_user(ptr, evt, sizeof(struct ts_event));
			ts_evt_pull(ts);

			if (err)
				break;

			ptr += sizeof(struct ts_event);
			count -= sizeof(struct ts_event);
			continue;
		}

		__set_current_state(TASK_INTERRUPTIBLE);
		if (filp->f_flags & O_NONBLOCK) {
			err = -EAGAIN;
			break;
		}
		schedule();
	}
	__set_current_state(TASK_RUNNING);
	remove_wait_queue(&ts->read_wait, &wait);

//	printk(KERN_ERR "%s: ts_read finished\n", client_name);
	return (ptr == buffer)? err : ptr - buffer;
}

static unsigned int picts_poll(struct file *filp, poll_table *wait)
{
	struct pic16f616_ts *ts = filp->private_data;
	int ret = 0;

	poll_wait(filp, &ts->read_wait, wait);
	if (ts_evt_pending(ts))
		ret = POLLIN | POLLRDNORM;

	return ret;
}

static int picts_fasync(int fd, struct file *filp, int on)
{
	struct pic16f616_ts *ts = filp->private_data;

//	printk(KERN_ERR "%s: ts_fasync called\n", client_name);
	return fasync_helper(fd, filp, on, &ts->fasync);
}

static int picts_open(struct inode *inode, struct file *filp)
{
	struct pic16f616_ts *ts = gts;
	int ret = 0;
	ret = ts_startup(ts);
	if (ret==0) filp->private_data = ts;
	return ret;
}

/*
 * Release touchscreen resources.  Disable IRQs.
 */
static int picts_release(struct inode *inode, struct file *filp)
{
	struct pic16f616_ts *ts = filp->private_data;
	if (ts) {
		down(&ts->sem);
		picts_fasync(-1, filp, 0);
		up(&ts->sem);
		ts_shutdown(ts);
	}
//	printk(KERN_ERR "%s: ts_release finished\n", client_name);
	return 0;
}

static struct file_operations pic16f616_ts_fops = {
	.owner		= THIS_MODULE,
	.llseek		= no_llseek,
	.read		= picts_read,
	.poll		= picts_poll,
	.open		= picts_open,
	.release	= picts_release,
	.fasync		= picts_fasync,
};

/*
 * The touchscreen is a miscdevice:
 *   10 char        Non-serial mice, misc features
 *   14 = /dev/touchscreen/pic16f616_ts touchscreen
 */
static struct miscdevice pic16f616_ts_dev = {
	minor:	14,
	name:	"pic16f616_ts",
	fops:	&pic16f616_ts_fops,
};

static inline int ts_register(struct pic16f616_ts *ts)
{
	init_waitqueue_head(&ts->read_wait);
	return misc_register(&pic16f616_ts_dev);
}

static inline void ts_deregister(struct pic16f616_ts *ts)
{
	misc_deregister(&pic16f616_ts_dev);
}

/*-----------------------------------------------------------------------*/
#else
/*-----------------------------------------------------------------------*/

#define ts_evt_clear(ts)	do { } while (0)

//////////////////////////
static inline void ts_evt_add(struct pic16f616_ts *ts, u16 pressure,
		u16 x, u16 y)
{
	struct input_dev *idev = ts->idev;
	if (calibration[6]){
		int tmpx, tmpy ;
		tmpx = calibration[0]*x + calibration[1]*y + calibration[2];
		tmpx /= calibration[6];
		if (tmpx < 0)
			tmpx = 0;
		tmpy = calibration[3]*x + calibration[4]*y + calibration[5];
		tmpy /= calibration[6];
		if (tmpy < 0)
			tmpy = 0;
		x = tmpx; y = tmpy;
	}
	if (ts->touch_count++) {
		input_report_abs(idev, ABS_X, ts->save_x);
		input_report_abs(idev, ABS_Y, ts->save_y);
		input_report_abs(idev, ABS_PRESSURE, ts->save_pressure);
		input_report_key(idev, BTN_TOUCH, 1);
		input_sync(idev);
	}
	ts->save_x = x ;
	ts->save_y = y ;
	ts->save_pressure = pressure ;
}

static inline void ts_event_release(struct pic16f616_ts *ts)
{
	struct input_dev *idev = ts->idev;
	input_report_abs(idev, ABS_PRESSURE, 0);
	input_report_key(idev, BTN_TOUCH, 0);
	input_sync(idev);
	ts->touch_count = 0 ;
}
static int ts_open(struct input_dev *idev)
{
	struct pic16f616_ts *ts = input_get_drvdata(idev);
	return ts_startup(ts);
}

static void ts_close(struct input_dev *idev)
{
	struct pic16f616_ts *ts = input_get_drvdata(idev);
	ts_shutdown(ts);
}

static inline int ts_register(struct pic16f616_ts *ts)
{
	unsigned mask;
	struct input_dev *idev;
	idev = input_allocate_device();
	if (idev==NULL) {
		return -ENOMEM;
	}
	ts->idev = idev;
	idev->name      = "i2c_touch";
	idev->id.product = ts->client->addr;
	idev->open      = ts_open;
	idev->close     = ts_close;

	__set_bit(EV_ABS, idev->evbit);
	__set_bit(ABS_X, idev->absbit);
	__set_bit(ABS_Y, idev->absbit);
	__set_bit(ABS_PRESSURE, idev->absbit);

	mask = combine_samples * MAX_SAMPLE_VAL;
	mask = fls(mask);
	sample_shift = 0;
#define MAX_BITS 14	/* 16 doesn't work for android */
	if (mask > MAX_BITS) {
		sample_shift = mask - MAX_BITS;
		mask = MAX_BITS;
	}
	mask = (1 << mask) - 1;
	input_set_abs_params(idev, ABS_X, 0, mask, 0, 0);
	input_set_abs_params(idev, ABS_Y, 0, mask, 0, 0);
	input_set_abs_params(idev, ABS_PRESSURE, 0, 1, 0, 0);

	__set_bit(EV_KEY, idev->evbit);
	__set_bit(BTN_TOUCH, idev->keybit);

	input_set_drvdata(idev, ts);
	return input_register_device(idev);
}

static inline void ts_deregister(struct pic16f616_ts *ts)
{
	if (ts->idev) {
		input_unregister_device(ts->idev);
		input_free_device(ts->idev);
		ts->idev = NULL;
	}
}

#endif
/*-----------------------------------------------------------------------*/

/*
 * This is a RT kernel thread that handles the I2c accesses
 * The I2c access functions are expected to be able to sleep.
 */
static int ts_thread(void *_ts)
{
	int ret;
	unsigned char buf[32];
	struct pic16f616_ts *ts = _ts;
	unsigned char sumXReg[1] = { SUM_X };
	struct i2c_msg readSums[2] = {
		{ts->client->addr, 0, 1, sumXReg},
		{ts->client->addr, I2C_M_RD, 6, buf}		//read SUM_X & SUM_Y
	};

	struct task_struct *tsk = current;
	unsigned int i;
	unsigned int j = 1<<22;
	unsigned int pressure = 1;
	unsigned drop;

	ts->rtask = tsk;

	daemonize("pic16f616tsd");
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
#if 1
	/* davinci i2c has bug with very 1st write after powerup */
	buf[0] = SUM_X;
	i2c_master_send(ts->client,buf,1);
#endif

        buf[0] = PULLUP_DELAY ;
        buf[1] = pullup_delay ;		// units of time (loop counter) 0 min, 0xff max
        i2c_master_send(ts->client,buf,2);

        buf[0] = SAMPLE_CNT ;
        buf[1] = combine_samples ;		// over-sampling count
        i2c_master_send(ts->client,buf,2);

        buf[0] = SAMPLE_DELAY ;
        buf[1] = sample_delay ;
        i2c_master_send(ts->client,buf,2);

        buf[0] = DRIVE_DELAY ;
        buf[1] = drive_delay ;
        i2c_master_send(ts->client,buf,2);
	drop = drop_samples;

	do {
#ifdef TESTING
		msleep(20);
		printk(KERN_ERR "(i2cs)\n");
//		printk(KERN_ERR "%s: reading from device 0x%x\n",client_name,ts->client.addr);
		do_gettimeofday(&ts->lastInterruptTime);
#endif
		ts->bReady = 0;
		/* For the 1st access and after a release, make sure the initial register address is selected */
		ret = (j & (1<<22))?  (i2c_transfer(ts->client->adapter, readSums, 2)+4) : i2c_master_recv(ts->client,buf,6);
		if (ret != 6) {
			printk(KERN_WARNING "%s: %s failed\n",client_name,(j & (1<<22))? "i2c_transfer" : "i2c_master_recv");
			i = 0;
			j = (1<<23)|(1<<22);
		} else {
			i = buf[0]+ (buf[1]<<8) + (buf[2]<<16);
			j = buf[3]+ (buf[4]<<8) + (buf[5]<<16);
		}

		if (signal_pending(tsk))
			break;

		if (j & (1<<23)) {
			/* sample is valid */
#ifdef TESTING
			printk(KERN_ERR "%s: i=%06x j=%06x\n",client_name,i,j);
#endif
			if (drop) {
				drop--;
			} else {
				if (j & (1<<22)) {
					/* this is a release notice */
					ts_event_release(ts);
					drop = drop_samples;
				} else {
					/* touch is active */
					j &= SAMPLE_MASK;
					ts_evt_add(ts, pressure,
						i >> sample_shift,
						j >> sample_shift);
				}
			}
		} else {
			printk(KERN_WARNING "%s: sample not valid i=%06x j=%06x interruptCnt=%i\n",client_name,i,j,ts->interruptCnt);
			j = (1<<22);	/* Force register number write to help recovery */
		}
		wait_event_interruptible(ts->sample_waitq, ts->bReady);
		if (signal_pending(tsk))
			break;
	} while (1);

	ts->rtask = NULL;
	ts_evt_clear(ts);
//	printk(KERN_ERR "%s: ts_thread exiting\n",client_name);
	complete_and_exit(&ts->init_exit, 0);
}

/*
 * We only detect samples ready with this interrupt
 * handler, and even then we just schedule our task.
 */
static irqreturn_t ts_interrupt(int irq, void *id)
{
	struct pic16f616_ts *ts = id;
	int bit;
	{
		unsigned gp = ts->gp;
#if 1
		bit = gpio_get_value(gp);
#else
		struct gpio_controller  *__iomem g = __gpio_to_controller(gp);
		bit = (__raw_readl(&g->in_data) >> (gp&0x1f))&1;
#endif
	}
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

static int ts_startup(struct pic16f616_ts* ts)
{
	int ret = 0;
	if (ts==NULL) return -EIO;

	if (down_interruptible(&ts->sem))
		return -EINTR;

	if (ts->use_count++ != 0)
		goto out;

	if (ts->rtask)
		panic("pic16f616tsd: rtask running?");

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
static void ts_shutdown(struct pic16f616_ts* ts)
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
	strlcpy(info->type, "pic16f616-ts", I2C_NAME_SIZE);
	return 0;
}
struct plat_i2c_generic_data {
	unsigned irq;
	unsigned gp;
};

static int ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int err = 0;
	struct pic16f616_ts* ts;
	struct device *dev = &client->dev;
	struct plat_i2c_generic_data *plat = client->dev.platform_data;
	if (gts) {
		printk(KERN_ERR "%s: Error gts is already allocated\n",client_name);
		return -ENOMEM;
	}
	ts = kzalloc(sizeof(struct pic16f616_ts),GFP_KERNEL);
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
			ts->procentry->read_proc = pic16f616_proc_read ;
			ts->procentry->write_proc = pic16f616_proc_write ;
		}
#define TSTYPE_PROCNAME "tstype"
		ts->tstype_procentry = create_proc_entry(TSTYPE_PROCNAME, 0, 0);
		if (ts->tstype_procentry)
			ts->tstype_procentry->read_proc  = tstype_read_proc;
	} else {
		printk(KERN_WARNING "%s: ts_register failed\n", client_name);
		ts_deregister(ts);
		kfree(ts);
	}
	return err;
}

static int ts_remove(struct i2c_client *client)
{
	struct pic16f616_ts* ts = i2c_get_clientdata(client);
	if (ts==gts) {
		gts = NULL;
		ts_deregister(ts);
	} else {
		printk(KERN_ERR "%s: Error ts!=gts\n",client_name);
	}
	kfree(ts);
	return 0;
}

static int pic_setup(char *options)
{
	char *this_opt;
	int rval = 0 ;
	printk( KERN_DEBUG "%s options=%s\n", __FUNCTION__, options );

	while ((this_opt = strsep(&options, ",")) != NULL) {
		if( 0 == strncmp("pudelay:",this_opt,8) ){
			pullup_delay = simple_strtol(this_opt+8,0,0);
			if (pullup_delay==0) pullup_delay = PULLUP_DELAY_DEFAULT;
			printk( KERN_INFO "%s: pullup delay == %d\n", __FUNCTION__, pullup_delay );
		}
		else if( *this_opt ){
			printk( KERN_DEBUG "%s: Unknown option %s\n", __FUNCTION__, this_opt );
			rval = -1 ;
			break;
		}
	}

	return 0 ;
}

static char *options = "";
module_param(options, charp, S_IRUGO);

#ifndef MODULE
static int __init save_options(char *args)
{
	if (!args || !*args)
		return 0;
	options=args ;
        printk( KERN_DEBUG "pic16F616-ts options: %s\n", args );
	return 0;
}
__setup("i2c_touch=", save_options);
#endif

/*-----------------------------------------------------------------------*/

static const struct i2c_device_id ts_idtable[] = {
	{ "Pic16F616-ts", 0 },
	{ }
};

static struct i2c_driver ts_driver = {
	.driver = {
		.owner		= THIS_MODULE,
		.name		= "Pic16F616-ts",
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
        pic_setup(options);
	printk("%s: version 1.0 (Dec. 24, 2007)\n",client_name);
	return 0;
}

static void __exit ts_exit(void)
{
	i2c_del_driver(&ts_driver);
}

MODULE_AUTHOR("Troy Kisky <troy.kisky@boundarydevices.com>");
MODULE_DESCRIPTION("I2C interface for Pic16F616 touch screen controller.");
MODULE_LICENSE("GPL");

module_init(ts_init)
module_exit(ts_exit)
