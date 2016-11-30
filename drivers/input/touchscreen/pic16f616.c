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
#include <asm/io.h>
#include <linux/module.h>

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/of_gpio.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/wait.h>

static char const * const touch_type_names[] = {
   "Unknown"
,  "4-wire resistive"
,  "5-wire resistive"
};

static int calibration[7] = {0};
module_param_array(calibration, int, NULL, S_IRUGO | S_IWUSR);


struct pic16f616_ts {
	struct i2c_client *client;
	struct input_dev	*idev;
	struct semaphore	sem;
	int			use_count;
	int			touch_count ;
	int			save_x ;
	int			save_y ;
	int			save_pressure ;
//#define TESTING
#ifdef TESTING
	struct timeval	lastInterruptTime;
#endif
	int irq;
	struct gpio_desc	*wakeup_gpio;
	struct proc_dir_entry *procentry;
	struct proc_dir_entry *tstype_procentry;

	unsigned		last_j;
	unsigned		drop;
	unsigned char		sumXReg[1];
	unsigned char		spare[3];
	struct i2c_msg		readSums[2];
	unsigned char		buf[32];
};
static const char *client_name = "Pic16F616-ts";

static unsigned char sample_shift;

static unsigned char combine_samples = CONFIG_TOUCHSCREEN_PIC16F616_COMBINE;
module_param(combine_samples, byte, S_IRUGO | S_IWUSR);

static unsigned char drop_samples = CONFIG_TOUCHSCREEN_PIC16F616_DROP;
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

static int ts_startup(struct pic16f616_ts* ts);
static void ts_shutdown(struct pic16f616_ts* ts);

static ssize_t tstype_read(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct pic16f616_ts *ts = dev_get_drvdata(dev);
	int tstype=0;
	unsigned char regAddr[] = { X_IGND};
	unsigned char read_buf[32];
	struct i2c_msg readReg[] = {
		{gts->client->addr, 0, 1, regAddr},
		{gts->client->addr, I2C_M_RD, 1, read_buf},
	};
	int totalWritten = 0 ;
	int rval;
	int count = 4096;

	if (!ts)
		return -EIO ;

	rval = i2c_transfer(ts->client->adapter, readReg, ARRAY_SIZE(readReg));
	ts->last_j |= (1 << 22);
	if (ARRAY_SIZE(readReg) == rval) {
		if (read_buf[0]==0x80)
			tstype = 1;	/* 4 wire */
		else if (read_buf[0]==0x06)
			tstype = 2;	/* 5 wire */
	} else {
		pr_err("%s: transfer error %d\n", __func__, rval);
	}
	totalWritten = snprintf(buf, count, "%s\n",
				touch_type_names[tstype]);
	if (totalWritten < 0) {
		totalWritten = 0;
	}
	return totalWritten;
}

static ssize_t pic16f616_regs_read(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct pic16f616_ts *ts = dev_get_drvdata(dev);
	int count = 4096;
	int totalWritten = 0;
	unsigned char regAddr[] = { MIN_X};
	unsigned char read_buf[32];
	struct i2c_msg readReg[2] = {
		{ts->client->addr, 0, 1, regAddr},
		{ts->client->addr, I2C_M_RD, 2, read_buf}
	};

	if (!ts)
		return -EIO ;

	pr_err("%s\n", __FUNCTION__);
	while ((0 < count) && (PULLUP_DELAY >= regAddr[0])) {
		int rval = i2c_transfer(ts->client->adapter,
					readReg,
					ARRAY_SIZE(readReg));
		ts->last_j |= (1 << 22);
		if (ARRAY_SIZE(readReg) == rval) {
			int written = snprintf(buf, count,
					       "%02x: %02x %02x\n",
					       regAddr[0],
					       buf[0],
					       buf[1]);
			if (0 <= written) {
				count -= written ;
				buf += written ;
				totalWritten += written ;
				regAddr[0] += 2 ;
				continue;
			}
		} else {
			pr_err("%s: transfer error %d\n",
				__func__, rval);
		}
		break;
	}
	return totalWritten ;
}

static ssize_t pic16f616_regs_write(struct device *dev, struct device_attribute *attr,
	 const char *buf, size_t count)
{
	struct pic16f616_ts *ts = dev_get_drvdata(dev);
	unsigned reg, value;
	int numscanned = sscanf(buf,"%x %x\n", &reg, &value);

	if (!ts)
		return -EIO ;

	if ((reg > PULLUP_DELAY) || (value > 0xff)) {
		dev_err(dev,"%s: invalid register: use form 0xREG 0xVAL\n",
			__func__);
		return count;
	}
	if (numscanned == 2) {
		int rval;
		unsigned char buf[4];

		buf[0] = reg;
	        buf[1] = value;
	        rval = i2c_master_send(ts->client, buf, 2);
		ts->last_j |= (1 << 22);
		if (rval < 0)
			dev_err(dev, "%s: error %d setting reg 0x%04x to 0x%02x\n",
				__func__, rval, reg, value);
	} else {
		dev_err(dev,"%s: invalid register: use form 0xREG [0xVAL]\n",
			__func__);
	}
	return count;
}

static DEVICE_ATTR(pic16f616_regs, S_IRUGO | S_IWUSR, pic16f616_regs_read, pic16f616_regs_write);
static DEVICE_ATTR(tstype, S_IRUGO, tstype_read, NULL);

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
	if (!idev)
		return -ENOMEM;
	idev->dev.parent = &ts->client->dev;
	idev->name      = "pic16f616";
	idev->id.bustype = BUS_I2C;
	idev->id.vendor = 0xbd;
	idev->id.product = ts->client->addr;
	idev->open      = ts_open;
	idev->close     = ts_close;
	ts->idev = idev;

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

/*-----------------------------------------------------------------------*/

static void ts_service(struct pic16f616_ts *ts)
{
	int ret;

	unsigned int i;
	unsigned int j;
	int select_sum = (ts->last_j & (1<<22));


#ifdef TESTING
	msleep(20);
	pr_info("%s:\n", __func__);
//	pr_info("%s: reading from device 0x%x\n",client_name,ts->client.addr);
	do_gettimeofday(&ts->lastInterruptTime);
#endif
	/*
	 * For the 1st access and after a release,
	 * make sure the initial register address is selected
	 */
	ret = select_sum ?
		(i2c_transfer(ts->client->adapter, ts->readSums, 2) + 4) :
		i2c_master_recv(ts->client, ts->buf,6);
	if (ret != 6) {
		pr_warn("%s: %s failed\n", client_name,
			select_sum ? "i2c_transfer" : "i2c_master_recv");
		i = 0;
		j = (1<<23)|(1<<22);
		memset(ts->buf, 0, 6);
	} else {
		unsigned char *buf = ts->buf;

		i = buf[0]+ (buf[1]<<8) + (buf[2]<<16);
		j = buf[3]+ (buf[4]<<8) + (buf[5]<<16);
	}

	if (j & (1<<23)) {
		/* sample is valid */
#ifdef TESTING
		pr_info("%s: i=%06x j=%06x\n",client_name,i,j);
#endif
		if (ts->drop) {
			ts->drop--;
		} else {
			if (j & (1<<22)) {
				/* this is a release notice */
				ts_event_release(ts);
				ts->drop = drop_samples;
			} else {
				/* touch is active */
				j &= SAMPLE_MASK;
				ts_evt_add(ts, 1, i >> sample_shift,
						j >> sample_shift);
			}
		}
	} else {
		pr_warn("%s: sample not valid i=%06x j=%06x %s\n", client_name,
			i, j, select_sum ? "i2c_transfer" : "i2c_master_recv");
		j = (1<<22);	/* Force register number write to help recovery */
	}

	ts->last_j = j;
}

/*
 * We only detect samples ready with this interrupt
 * handler, and even then we just schedule our task.
 */
static irqreturn_t ts_interrupt(int irq, void *id)
{
	struct pic16f616_ts *ts = id;
	int bit;

	bit = gpiod_get_value(ts->wakeup_gpio);
	if (bit) {
		ts_service(ts);
	}
#ifdef TESTING
	{
		suseconds_t     tv_usec = ts->lastInterruptTime.tv_usec;
		int delta;
		do_gettimeofday(&ts->lastInterruptTime);
		delta = ts->lastInterruptTime.tv_usec - tv_usec;
		if (delta<0) delta += 1000000;
		pr_info("(delta=%ius gp%i=%i)\n",delta, ts->gp, bit);
	}
#endif
	return IRQ_HANDLED;
}

static int ts_startup(struct pic16f616_ts* ts)
{
	unsigned char buf[32];
	int ret = 0;
	if (ts==NULL) return -EIO;

	if (down_interruptible(&ts->sem))
		return -EINTR;

	if (ts->use_count++ != 0)
		goto out;

	pr_info("%s: i2c_addr=0x%x, reg sum_x=0x%x\n", __func__, ts->client->addr, SUM_X);
	ts->last_j = 1 << 22;
	ts->sumXReg[0] = SUM_X;
	ts->readSums[0].addr = ts->client->addr;
	ts->readSums[0].flags = 0;
	ts->readSums[0].len = 1;
	ts->readSums[0].buf = ts->sumXReg;
	ts->readSums[1].addr = ts->client->addr;
	ts->readSums[1].flags = I2C_M_RD;
	ts->readSums[1].len = 6;
	ts->readSums[1].buf = ts->buf;		/* read SUM_X & SUM_Y */

#if 1
	/* davinci i2c has bug with very 1st write after powerup */
	buf[0] = SUM_X;
	i2c_master_send(ts->client, buf, 1);
#endif

        buf[0] = PULLUP_DELAY ;
        buf[1] = pullup_delay ;		// units of time (loop counter) 0 min, 0xff max
        i2c_master_send(ts->client, buf, 2);

        buf[0] = SAMPLE_CNT ;
        buf[1] = combine_samples ;		// over-sampling count
        i2c_master_send(ts->client, buf, 2);

        buf[0] = SAMPLE_DELAY ;
        buf[1] = sample_delay ;
        i2c_master_send(ts->client, buf, 2);

        buf[0] = DRIVE_DELAY ;
        buf[1] = drive_delay ;
        i2c_master_send(ts->client, buf, 2);
	ts->drop = drop_samples;

	ret = request_threaded_irq(ts->irq, NULL, &ts_interrupt,
			IRQF_ONESHOT, client_name, ts);
	if (ret)
		pr_err("%s: request_irq failed, irq:%i\n", client_name,ts->irq);

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

	if (gts) {
		pr_err("%s: Error gts is already allocated\n",client_name);
		return -ENOMEM;
	}
	ts = kzalloc(sizeof(struct pic16f616_ts),GFP_KERNEL);
	if (!ts) {
		dev_err(dev, "Couldn't allocate memory for %s\n", client_name);
		return -ENOMEM;
	}
	sema_init(&ts->sem, 1);
	ts->client = client;
	ts->irq = client->irq;
	ts->wakeup_gpio = devm_gpiod_get_index(dev, "wakeup", 0, GPIOD_IN);
	if (IS_ERR(ts->wakeup_gpio)) {
		err = -ENODEV;
		goto exit1;
	}
	pr_info("%s: %s touchscreen irq=%i, %i\n", __func__, client_name,
			ts->irq, gpiod_to_irq(ts->wakeup_gpio));
	i2c_set_clientdata(client, ts);
	err = ts_register(ts);
	if (err==0) {
		gts = ts;
		device_create_file(dev, &dev_attr_pic16f616_regs);
		device_create_file(dev, &dev_attr_tstype);
		return 0;
	}
	pr_warn("%s: ts_register failed\n", client_name);
	ts_deregister(ts);
exit1:
	kfree(ts);
	return err;
}

static int ts_remove(struct i2c_client *client)
{
	struct pic16f616_ts* ts = i2c_get_clientdata(client);

	device_remove_file(&client->dev, &dev_attr_pic16f616_regs);
	device_remove_file(&client->dev, &dev_attr_tstype);
	if (ts==gts) {
		gts = NULL;
		ts_deregister(ts);
	} else {
		pr_err("%s: Error ts!=gts\n",client_name);
	}
	kfree(ts);
	return 0;
}

static int pic_setup(char *options)
{
	char *this_opt;
	int rval = 0 ;
	pr_debug("%s options=%s\n", __FUNCTION__, options );

	while ((this_opt = strsep(&options, ",")) != NULL) {
		if( 0 == strncmp("pudelay:",this_opt,8) ){
			pullup_delay = simple_strtol(this_opt+8,0,0);
			if (pullup_delay==0) pullup_delay = PULLUP_DELAY_DEFAULT;
			pr_info("%s: pullup delay == %d\n", __FUNCTION__, pullup_delay );
		}
		else if( *this_opt ){
			pr_debug("%s: Unknown option %s\n", __FUNCTION__, this_opt );
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
        pr_debug("pic16F616-ts options: %s\n", args );
	return 0;
}
__setup("pic16f616=", save_options);
#endif

/*-----------------------------------------------------------------------*/

static const struct i2c_device_id ts_idtable[] = {
	{ "Pic16F616-ts", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ts_idtable);

static struct i2c_driver ts_driver = {
	.driver = {
		.owner		= THIS_MODULE,
		.name		= "pic16F616",
	},
	.id_table	= ts_idtable,
	.probe		= ts_probe,
	.remove		= ts_remove,
	.detect		= ts_detect,
};

static int __init ts_init(void)
{
	int res;
	if ((res = i2c_add_driver(&ts_driver))) {
		pr_warn("%s: i2c_add_driver failed\n",client_name);
		return res;
	}
        pic_setup(options);
	pr_info("%s: version 1.1 (Nov. 28, 2016)\n",client_name);
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
/* MODULE_ALIAS("i2c:pic16F616"); */
