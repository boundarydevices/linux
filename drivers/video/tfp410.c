/*
 *  tfp410.c - DVI output chip
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
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/hwmon.h>
#include <linux/input-polldev.h>
#include <linux/gpio.h>
#include <linux/fb.h>
#include <linux/kthread.h>

#define TESTING

/* register definitions according to the TFP410 data sheet */
#define TFP410_VID		0x014C
#define TFP410_DID		0x0410
#define TFP410_VID_DID		(TFP410_VID | (TFP410_DID << 16))

#define TFP410_VID_LO		0x00
#define TFP410_VID_HI		0x01
#define TFP410_DID_LO		0x02
#define TFP410_DID_HI		0x03
#define TFP410_REV		0x04

#define TFP410_CTL_1		0x08
#define TFP410_CTL_1_RSVD	(1<<7)
#define TFP410_CTL_1_TDIS	(1<<6)
#define TFP410_CTL_1_VEN	(1<<5)
#define TFP410_CTL_1_HEN	(1<<4)
#define TFP410_CTL_1_DSEL	(1<<3)
#define TFP410_CTL_1_BSEL	(1<<2)
#define TFP410_CTL_1_EDGE	(1<<1)
#define TFP410_CTL_1_PD		(1<<0)

#define TFP410_CTL_2		0x09
#define TFP410_CTL_2_VLOW	(1<<7)
#define TFP410_CTL_2_MSEL_MASK	(0x7<<4)
#define TFP410_CTL_2_MSEL_INT	(1<<4)
#define TFP410_CTL_2_MSEL_RSEN	(2<<4)
#define TFP410_CTL_2_MSEL_HTPLG	(3<<4)
#define TFP410_CTL_2_TSEL_RSEN	(0<<3)
#define TFP410_CTL_2_TSEL_HTPLG	(1<<3)
#define TFP410_CTL_2_RSEN	(1<<2)
#define TFP410_CTL_2_HTPLG	(1<<1)
#define TFP410_CTL_2_MDI	(1<<0)

#define TFP410_CTL_3		0x0A
#define TFP410_CTL_3_DK_MASK 	(0x7<<5)
#define TFP410_CTL_3_DK		(1<<5)
#define TFP410_CTL_3_DKEN	(1<<4)
#define TFP410_CTL_3_CTL_MASK	(0x7<<1)
#define TFP410_CTL_3_CTL	(1<<1)

#define TFP410_USERCFG		0x0B

#define DRV_NAME	"tfp410"
static const char *client_name = DRV_NAME;


struct tfp410_priv
{
	struct i2c_client	*client;
	struct gpio_desc	*gp_i2c_sel;
	wait_queue_head_t	sample_waitq;
	struct completion	init_exit;	//thread exit notification
	struct task_struct	*rtask;
	int			bReady;
	int			interruptCnt;
	int			irq;
	unsigned		gp;
#define TFP_ENABLE_HOTPLUG 1
#define TFP_ENABLE_RSEN	2
	int			enabled;
	int			default_mode;
	int			displayoff;
	char			display_id[16];
#ifdef TESTING
	struct timeval	lastInterruptTime;
#endif
};

static unsigned char cmd_off_rsen[] = {
	TFP410_CTL_1, TFP410_CTL_1_RSVD | TFP410_CTL_1_VEN | TFP410_CTL_1_HEN |
		TFP410_CTL_1_DSEL | TFP410_CTL_1_BSEL,
	TFP410_CTL_2, TFP410_CTL_2_MDI | TFP410_CTL_2_MSEL_INT | TFP410_CTL_2_TSEL_RSEN,
		0
};

static unsigned char cmd_off_hotplug[] = {
	TFP410_CTL_1, TFP410_CTL_1_RSVD | TFP410_CTL_1_VEN | TFP410_CTL_1_HEN |
		TFP410_CTL_1_DSEL | TFP410_CTL_1_BSEL,
	TFP410_CTL_2, TFP410_CTL_2_MDI | TFP410_CTL_2_MSEL_INT | TFP410_CTL_2_TSEL_HTPLG,
		0
};

static unsigned char cmd_on_rsen[] = {
	TFP410_CTL_1, TFP410_CTL_1_RSVD | TFP410_CTL_1_VEN | TFP410_CTL_1_HEN |
		TFP410_CTL_1_DSEL | TFP410_CTL_1_BSEL | TFP410_CTL_1_PD,
	TFP410_CTL_2, TFP410_CTL_2_MDI | TFP410_CTL_2_MSEL_INT | TFP410_CTL_2_TSEL_RSEN,
		0
};

static unsigned char cmd_on_hotplug[] = {
	TFP410_CTL_1, TFP410_CTL_1_RSVD | TFP410_CTL_1_VEN | TFP410_CTL_1_HEN |
		TFP410_CTL_1_DSEL | TFP410_CTL_1_BSEL | TFP410_CTL_1_PD,
	TFP410_CTL_2, TFP410_CTL_2_MDI | TFP410_CTL_2_MSEL_INT | TFP410_CTL_2_TSEL_HTPLG,
		0
};
/*
 * Initialization function
 */
static int tfp410_send_cmds(struct i2c_client *client, unsigned char *p)
{
	int result = 0;
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

static int tfp410_on(struct tfp410_priv *tfp, int mode)
{
	int result;
	int hotplug = (mode == TFP_ENABLE_HOTPLUG);
	pr_info("tfp410: power up %s\n", hotplug ? "hotplug" : "rsen");
	result = tfp410_send_cmds(tfp->client, hotplug ? cmd_on_hotplug : cmd_on_rsen);
	if (!result)
		tfp->enabled = mode;
	return result;
}

static int tfp410_off(struct tfp410_priv *tfp)
{
	int result;
	int hotplug = (tfp->default_mode == TFP_ENABLE_HOTPLUG);
	pr_info("tfp410: power down %s\n", hotplug ? "hotplug" : "rsen");
	result = tfp410_send_cmds(tfp->client, hotplug ? cmd_off_hotplug : cmd_off_rsen);
	if (!result)
		tfp->enabled = 0;
	return result;
}

void tfp410_check_hotplug_rsen(struct tfp410_priv *tfp)
{
	int retry;
	int mode;
	for (retry = 0; retry < 10; retry++) {
		int ret = i2c_smbus_read_byte_data(tfp->client, TFP410_CTL_2);
		if (ret < 0) {
			struct device *dev = &tfp->client->dev;
			dev_err(dev, "i2c_smbus_read_byte_data failed(%i)\n", ret);
			mode = tfp->default_mode;
			if (retry < 8)
				continue;
		} else if (ret & TFP410_CTL_2_HTPLG) {
			mode = TFP_ENABLE_HOTPLUG;
			tfp->default_mode = mode;
		} else if (ret & TFP410_CTL_2_RSEN) {
			mode = TFP_ENABLE_RSEN;
			tfp->default_mode = mode;
		} else {
			mode = 0;
		}
		if ((mode != tfp->enabled) || !(ret & TFP410_CTL_2_MDI)) {
			if (mode)
				tfp410_on(tfp, mode);
			else
				tfp410_off(tfp);
		} else {
			break;
		}
	}
}

struct tfp410_priv *g_tfp;

static int lcd_fb_event(struct notifier_block *nb, unsigned long val, void *v)
{
	struct fb_event *event = v;
	struct tfp410_priv *tfp = g_tfp;
	struct device *dev;
	if (!tfp)
		return 0;
	dev = &tfp->client->dev;
	dev_info(dev, "%s: event %lx, display %s\n", __func__, val, event->info->fix.id);
	if (strncmp(event->info->fix.id, tfp->display_id, sizeof(tfp->display_id))) {
		return 0;
	}

	switch (val) {
	case FB_EVENT_BLANK: {
		int blankType = *((int *)event->data);
		dev_info(dev, "%s: blank type 0x%x\n", __func__, blankType );
		tfp->displayoff = (blankType == FB_BLANK_UNBLANK) ? 0 : 1;
		break;
	}
	case FB_EVENT_SUSPEND : {
		dev_info(dev, "%s: suspend\n", __func__ );
		tfp->displayoff = 1;
		break;
	}
	case FB_EVENT_RESUME : {
		dev_info(dev, "%s: resume\n", __func__ );
		tfp->displayoff = 0;
		break;
	}
	default:
		dev_info(dev, "%s: unknown event %lx\n", __func__, val);
	}
	tfp->bReady=1;
	wmb();
	wake_up(&tfp->sample_waitq);
	return 0;
}

static struct notifier_block nb = {
	.notifier_call = lcd_fb_event,
};

/*
 * This is a RT kernel thread that handles the I2c accesses
 * The I2c access functions are expected to be able to sleep.
 */
static int tfp_thread(void *_tfp)
{
	struct tfp410_priv *tfp = _tfp;
	struct task_struct *tsk = current;

	/* only want to receive SIGKILL, SIGTERM */
	allow_signal(SIGKILL);
	allow_signal(SIGTERM);

	/*
	 * We could run as a real-time thread.  However, thus far
	 * this doesn't seem to be necessary.
	 */
//	tsk->policy = SCHED_FIFO;
//	tsk->rt_priority = 1;

	complete(&tfp->init_exit);
	fb_register_client(&nb);
	g_tfp = tfp;
	tfp->interruptCnt=0;
	do {
		int bit = -1;
		do {
			unsigned gp = tfp->gp;
			int b;
			int ret;
			tfp->bReady = 0;
			b = gpio_get_value(gp);
#ifdef TESTING
			pr_info("tfp410: int bit=%d, displayoff=%d\n", b, tfp->displayoff);
#endif
			if (bit == b)
				break;
			if (signal_pending(tsk))
				goto exit1;
			/* wait 1/2 second for power/bit to stabilize */
			ret = wait_event_interruptible_timeout(tfp->sample_waitq,
					tfp->bReady, msecs_to_jiffies(500));
			bit = (!ret) ? b : -1;
		} while (1);

		if (tfp->displayoff)
			tfp410_off(tfp);
		else
			tfp410_check_hotplug_rsen(tfp);

		if (signal_pending(tsk))
			break;
		wait_event_interruptible(tfp->sample_waitq, tfp->bReady);
		if (signal_pending(tsk))
			break;
	} while (1);
exit1:
	fb_unregister_client(&nb);
	g_tfp = NULL;
	tfp410_off(tfp);
	tfp->rtask = NULL;
//	pr_err("%s: ts_thread exiting\n",client_name);
	complete_and_exit(&tfp->init_exit, 0);
}

/*
 * We only detect samples ready with this interrupt
 * handler, and even then we just schedule our task.
 */
static irqreturn_t tfp_interrupt(int irq, void *id)
{
	struct tfp410_priv *tfp = id;
//	pr_err("%s\n", __func__);
	tfp->interruptCnt++;
	tfp->bReady=1;
	wmb();
	wake_up(&tfp->sample_waitq);
#ifdef TESTING
	{
		suseconds_t     tv_usec = tfp->lastInterruptTime.tv_usec;
		int delta;
		do_gettimeofday(&tfp->lastInterruptTime);
		delta = tfp->lastInterruptTime.tv_usec - tv_usec;
		if (delta<0) delta += 1000000;
		pr_info("(delta=%ius gp%i)\n",delta, tfp->gp);
	}
#endif
	return IRQ_HANDLED;
}


/*
 * Release accelerometer resources.  Disable IRQs.
 */
static void tfp_deinit(struct tfp410_priv *tfp)
{
	if (tfp) {
		if (tfp->rtask) {
			send_sig(SIGKILL, tfp->rtask, 1);
			wait_for_completion(&tfp->init_exit);
		}
	}
}


static int tfp_init(struct tfp410_priv *tfp, struct i2c_client *client)
{
	struct task_struct *t;
	/* Initialize the tfp410 chip */
	int result = tfp410_on(tfp, tfp->default_mode);

	if (result)
		dev_err(&client->dev, "init failed\n");
	if (tfp->irq < 0)
		return 0;
	if (tfp->rtask)
		panic("tfp410d: rtask running?");

	init_completion(&tfp->init_exit);
	t = kthread_create(tfp_thread, tfp, DRV_NAME);
	if (IS_ERR(t)) {
		wait_for_completion(&tfp->init_exit);	//wait for thread to Start
		result = 0;
	}
	tfp->rtask = t;
	wake_up_process(t);
	return result;
}

static ssize_t tfp410_reg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	u8 tmp[16];
	struct tfp410_priv *tfp = dev_get_drvdata(dev);

	if (i2c_smbus_read_i2c_block_data(tfp->client, 0, 11, tmp) < 11) {
			dev_err(dev, "i2c block read failed\n");
			return -EIO;
	}
	return sprintf(buf, "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n", tmp[0], tmp[1], tmp[2], tmp[3], tmp[4],
			tmp[5], tmp[6], tmp[7], tmp[8], tmp[9], tmp[10]);
}
static ssize_t tfp410_reg_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	unsigned val;
	int result;
	struct tfp410_priv *tfp = dev_get_drvdata(dev);
	char *endp;
	unsigned reg = simple_strtol(buf, &endp, 16);
	if (reg >= 0x3e)
		return count;
	if (endp)
		if (*endp == 0x20)
			endp++;
	val = simple_strtol(endp, &endp, 16);
	if (val >= 0x100)
		return count;
	dev_err(dev, "%s:reg=0x%x, val=0x%x\n", __func__, reg, val);
	result = i2c_smbus_write_byte_data(tfp->client, reg, val);
	if (result < 0) {
		dev_err(dev, "i2c_smbus_write_byte_data failed(%i)\n", result);
		return -EIO;
	}
	return count;
}

static DEVICE_ATTR(tfp410_reg, 0644, tfp410_reg_show, tfp410_reg_store);

static ssize_t tfp410_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	u8 tmp[4];
	int result;
	struct tfp410_priv *tfp = dev_get_drvdata(dev);
	result = i2c_smbus_read_i2c_block_data(tfp->client, TFP410_CTL_1, 1, tmp);
	if (result < 1) {
		dev_err(dev, "i2c block read failed(%i)\n", result);
		return -EIO;
	}
	return sprintf(buf, "%i\n", (tmp[0] & TFP410_CTL_1_PD) ? 1 : 0);
}

static ssize_t tfp410_enable_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	int val;
	int result;
	struct tfp410_priv *tfp = dev_get_drvdata(dev);
	val = simple_strtol(buf, NULL, 10);
	if (val < 0)
		return count;
	result = i2c_smbus_read_byte_data(tfp->client, TFP410_CTL_1);
	if (result < 0) {
		dev_err(dev, "i2c_smbus_read_byte_data failed(%i)\n", result);
		return -EIO;
	}
	result &= ~TFP410_CTL_1_PD;
	if (val & 1)
		result |= TFP410_CTL_1_PD;
	result = i2c_smbus_write_byte_data(tfp->client, TFP410_CTL_1, result);
	if (result < 0) {
		dev_err(dev, "i2c_smbus_write_byte_data failed(%i)\n", result);
		return -EIO;
	}
	tfp->enabled = (val & 1) ? tfp->default_mode : 0;
	return count;
}

static DEVICE_ATTR(tfp410_enable, 0644, tfp410_enable_show, tfp410_enable_store);

/*
 * I2C init/probing/exit functions
 */
static int tfp410_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	unsigned vid_did;
	int result;
	int gp;
	int ret;
	unsigned retry = 0;
	struct tfp410_priv *tfp;
	struct i2c_adapter *adapter;
        struct device_node *np = client->dev.of_node;
	struct gpio_desc *gp_i2c_sel;
	const char *display_id = NULL;

	adapter = to_i2c_adapter(client->dev.parent);

	result = i2c_check_functionality(adapter,
					 I2C_FUNC_SMBUS_BYTE |
					 I2C_FUNC_SMBUS_BYTE_DATA);
	if (!result) {
		dev_err(&client->dev, "i2c_check_functionality failed\n");
		return -ENODEV;
	}

	gp = of_get_named_gpio(np, "interrupts-extended", 0);
	if (!gpio_is_valid(gp))
		return -ENODEV;

	gp_i2c_sel = devm_gpiod_get_index(&client->dev, "i2c_sel", 0,
					  GPIOD_OUT_LOW);
	if (!IS_ERR(gp_i2c_sel)) {
		/* enable i2c mode */
		gpiod_set_value(gp_i2c_sel, 1);
	} else {
		dev_warn(&client->dev, "no i2c_sel pin available");
	}

	do {
		msleep(2);
		if (i2c_smbus_read_i2c_block_data(client, TFP410_VID_LO, 4, (unsigned char *)&vid_did) == 4)
			break;
		dev_err(&client->dev, "i2c block read failed, retry=%d\n", retry);
		if  (retry++ > 6) {
			result = -EIO;
			goto release_gpio;
		}
	} while (1);

	if (vid_did != TFP410_VID_DID) {
		dev_err(&client->dev, "id match failed %x != %x\n", vid_did, TFP410_VID_DID);
		result = -EIO;
		goto release_gpio;
	}

	tfp = kzalloc(sizeof(struct tfp410_priv),GFP_KERNEL);
	if (!tfp) {
		dev_err(&client->dev, "Couldn't allocate memory\n");
		return -ENOMEM;
	}
	init_waitqueue_head(&tfp->sample_waitq);
	tfp->client = client;
	tfp->gp_i2c_sel = gp_i2c_sel;
	tfp->irq = client->irq;
	tfp->gp = gp;
	of_property_read_string(np, "display_id", &display_id);
	if (display_id)
		strncpy(&tfp->display_id[0], display_id, sizeof(tfp->display_id));

	tfp->default_mode = TFP_ENABLE_HOTPLUG;
	pr_info("%s: tfp410 irq=%i gp=%i\n", __func__, tfp->irq, tfp->gp);
	if (tfp->irq > 0) {
		result = request_irq(tfp->irq, &tfp_interrupt, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, client_name, tfp);
		if (result) {
			pr_err("%s: request_irq failed, irq:%i\n", client_name,tfp->irq);
			goto free_tfp;
		}
	}
	i2c_set_clientdata(client, tfp);
	result = tfp_init(tfp, client);
	if (result)
		goto free_irq;
	ret = device_create_file(&client->dev, &dev_attr_tfp410_reg);
	if (ret < 0)
		pr_warn("failed to add tfp410 sysfs files\n");
	ret = device_create_file(&client->dev, &dev_attr_tfp410_enable);
	if (ret < 0)
		pr_warn("failed to add tfp410 sysfs files\n");
	return result;
free_irq:
	if (tfp->irq >= 0)
		free_irq(tfp->irq, tfp);
free_tfp:
	tfp_deinit(tfp);
	kfree(tfp);
release_gpio:
	if (!IS_ERR(gp_i2c_sel))
		gpiod_direction_output(gp_i2c_sel, 0);	/* disable i2c mode */
	return result;
}

static int tfp410_remove(struct i2c_client *client)
{
	struct tfp410_priv *tfp = i2c_get_clientdata(client);
	int result = tfp410_off(tfp);
	device_remove_file(&client->dev, &dev_attr_tfp410_reg);
	if (tfp) {
		if (tfp->irq >= 0)
			free_irq(tfp->irq, tfp);
		tfp_deinit(tfp);
		kfree(tfp);
	}
	return result;
}

static int tfp410_suspend(struct device *dev)
{
	struct tfp410_priv *tfp = dev_get_drvdata(dev);
	int result = tfp410_off(tfp);
	return result;
}

static int tfp410_resume(struct device *dev)
{
	struct tfp410_priv *tfp = dev_get_drvdata(dev);
	int result = tfp410_on(tfp, tfp->default_mode);
	return result;
}

static const struct i2c_device_id tfp410_id[] = {
	{DRV_NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, tfp410_id);

static const struct dev_pm_ops tfp410_pm_ops = {
#ifdef CONFIG_PM_SLEEP
	.suspend = tfp410_suspend,
	.resume = tfp410_resume,
#endif
};

static struct i2c_driver tfp410_driver = {
	.driver = {
		   .name = DRV_NAME,
		   .pm	 = &tfp410_pm_ops,
		   .owner = THIS_MODULE,
		   },
	.probe = tfp410_probe,
	.remove = tfp410_remove,
	.id_table = tfp410_id,
};

static int __init tfp410_init(void)
{
	/* register driver */
	int res = i2c_add_driver(&tfp410_driver);
	if (res < 0) {
		pr_info("add tfp410 i2c driver failed\n");
		return -ENODEV;
	}
	pr_info("add tfp410 i2c driver\n");
	return res;
}

static void __exit tfp410_exit(void)
{
	pr_info("remove tfp410 i2c driver.\n");
	i2c_del_driver(&tfp410_driver);
}

MODULE_AUTHOR("Boundary Devices, Inc.");
MODULE_DESCRIPTION("tfp410 DVI output driver");
MODULE_LICENSE("GPL");

module_init(tfp410_init);
module_exit(tfp410_exit);
