/*
 * Microchip I2C Touchscreen Driver
 *
 * Copyright (c) 2011 Microchip Technology, Inc.
 *
 * http://www.microchip.com/mtouch
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/slab.h>

/* The private data structure that is referenced within the I2C bus driver */
struct ar1020_i2c_priv {
	struct i2c_client *client;
	struct input_dev *input;
	int irq;
	int testCount;
	int command_pending;
	int use_count;
	int button;
	struct delayed_work reenable_work;
};

/*
 * These are all the sysfs variables used to store and retrieve information
 * from a user-level application
 */
static char sendBuffer[100];
static char receiveBuffer[100];
static int commandDataPending;

/*
 * These variables allows the IRQ to be specified via a module parameter
 * or kernel parameter.  To configuration of these value, please see
 * driver documentation.
 */
static int touchIRQ;

module_param(touchIRQ, int, S_IRUGO);


/*
 * Since the reference to private data is stored within the I2C
 * bus driver, we will store another reference within this driver
 * so the sysfs related function may also access this data
 */
struct ar1020_i2c_priv *g_priv;

/*
 * Display value of "commandDataPending" variable to application that is
 * requesting it's value.
 */
static ssize_t commandDataPending_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d", commandDataPending);
}

/*
 * Save value to "commandDataPending" variable from application that is
 * requesting this.
 */
static ssize_t commandDataPending_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	sscanf(buf, "%d", &commandDataPending);
	return count;
}

static struct kobj_attribute commandDataPending_attribute =
	__ATTR(commandDataPending, 0666, commandDataPending_show,
			commandDataPending_store);


/*
 * Display value of "receiveBuffer" variable to application that is
 * requesting it's value.
 */
static ssize_t receiveBuffer_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	/* receive data is no longer pending */
	commandDataPending = 0;
	return sprintf(buf, "%s", receiveBuffer);
}

/*
 * Save value to "receiveBuffer" variable from application that is
 * requesting this.
 */
static ssize_t receiveBuffer_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	return snprintf(receiveBuffer, sizeof(receiveBuffer), "%s", buf);
}

static struct kobj_attribute receiveBuffer_attribute =
	__ATTR(receiveBuffer, 0666, receiveBuffer_show, receiveBuffer_store);

/*
 * Display value of "sendBuffer" variable to application that is
 * requesting it's value.
 */
static ssize_t sendBuffer_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%s", sendBuffer);
}

/*
 * Save value to "sendBuffer" variable from application that is
 * requesting this.
 */
static ssize_t sendBuffer_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int data[8];
	char buff[8];
	int cnt;
	int i;
	struct ar1020_i2c_priv *priv = g_priv;

	commandDataPending = 0;

	cnt = sscanf(buf, "%x %x %x %x %x %x %x %x", &data[0], &data[1],
		&data[2], &data[3], &data[4], &data[5], &data[6], &data[7]);

	pr_debug("AR1020 I2C: Processed %d bytes.\n", cnt);

	/* Verify command string to send to controller is valid */
	if (cnt < 3)
		pr_info("AR1020 I2C: Insufficient command bytes to process.\n");
	else if (data[0] != 0x0)
		pr_info("AR1020 I2C: Leading zero required when sending I2C commands.\n");
	else if (data[1] != 0x55)
		pr_info("AR1020 I2C: Invalid header byte (0x55 expected).\n");
	else if (data[2] != (cnt - 3))
		pr_info("AR1020 I2C: Number of command bytes specified not "
				"valid for current string.\n");

	strcpy(sendBuffer, "");
	pr_debug("AR1020 I2C: sending command bytes: ");
	for (i = 0; i < cnt; i++) {
		buff[i] = (char)data[i];
		pr_debug("0x%02x ", data[i]);
	}
	pr_debug("\n");

	if (priv) {
		priv->command_pending = 1;
		i2c_master_send(priv->client, buff, cnt);
	}

	return snprintf(sendBuffer, sizeof(sendBuffer), "%s", buf);
}

static struct kobj_attribute sendBuffer_attribute =
	__ATTR(sendBuffer, 0666, sendBuffer_show, sendBuffer_store);

/*
 * Create a group of calibration attributes so we may work with them
 * as a set.
 */
static struct attribute *attrs[] = {
	&commandDataPending_attribute.attr,
	&receiveBuffer_attribute.attr,
	&sendBuffer_attribute.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

static struct kobject *ar1020_kobj;

static void irq_reenable_work(struct work_struct *work)
{
	struct ar1020_i2c_priv *priv = container_of(work,
			struct ar1020_i2c_priv, reenable_work.work);

	enable_irq(priv->irq);
}

/*
 * When the controller interrupt is asserted, this function is scheduled
 * to be called to read the controller data.
 */
static irqreturn_t touch_irq_handler_func(int irq, void *dev_id)
{
	struct ar1020_i2c_priv *priv = (struct ar1020_i2c_priv *)dev_id;
	int i;
	int cnt;
	unsigned char packet[9];
	int x;
	int y;
	int button;

	for (i = 0; i < 5; i++)
		packet[i] = 0;

	/*
	 * We want to ensure we only read packets when we are not in the middle
	 * of command communication. Disable command mode after receiving
	 * command response to resume receiving packets.
	 */
	cnt = i2c_master_recv(priv->client, packet,
			(priv->command_pending) ? 9 : 5);

	if (cnt <= 0)
		goto error;

	if (packet[0] == 0x55) {
		unsigned char *p = receiveBuffer;
		unsigned char *pend = p + sizeof(receiveBuffer) - 2;
		/* process up to 9 bytes */
		strcpy(receiveBuffer, "");

		priv->command_pending = 0;

		if (packet[1] > 6)
			pr_info("AR1020 I2C: invalid byte count\n");
		else if (cnt > packet[1] + 2)
			cnt = packet[1] + 2;

		for (i = 0; i < cnt; i++) {
			int ret = snprintf(p, pend - p, " 0x%02x", packet[i]);
			if (ret <= 0)
				break;
			p += ret;
			if (p >= pend)
				break;
		}
		*p++ = '\n';
		*p++ = 0;
		pr_info("AR1020 I2C: command response: %s", receiveBuffer);
		commandDataPending = 1;
		return IRQ_HANDLED;
	}

	/*
	 * Decode packets of data from a device path using AR1XXX protocol.
	 *	Data format, 5 bytes: SYNC, DATA1, DATA2, DATA3, DATA4
	 * SYNC [7:0]: 1,0,0,0,0,TOUCHSTATUS[0:0]
	 * DATA1[7:0]: 0,X-LOW[6:0]
	 * DATA2[7:0]: 0,X-HIGH[4:0]
	 * DATA3[7:0]: 0,Y-LOW[6:0]
	 * DATA4[7:0]: 0,Y-HIGH[4:0]
	 *
	 * TOUCHSTATUS: 0 = Touch up, 1 = Touch down
	 */
	if ((packet[0] & 0xfe) != 0x80) {
		pr_err("AR1020 I2C: 1st Touch byte not valid.  Value: 0x%02x\n",
				packet[0]);
		goto error;	/* irq line may be shorted high, let's delay */
	}
	for (i = 1; i < 5; i++) {
		/* verify byte is valid for current index */
		if (0x80 & packet[i]) {
			pr_err("AR1020 I2C: Touch byte not valid.  Value: "
				"0x%02x Index: 0x%02x\n", packet[i], i);
			goto error;
		}
	}
	x = ((packet[2] & 0x1f) << 7) | (packet[1] & 0x7f);
	y = ((packet[4] & 0x1f) << 7) | (packet[3] & 0x7f);
	button = (packet[0] & 1);

	if (!button && !priv->button)
		return IRQ_HANDLED;

	priv->button = button;
	input_report_abs(priv->input, ABS_X, x);
	input_report_abs(priv->input, ABS_Y, y);
	input_report_key(priv->input, BTN_TOUCH, button);
	input_sync(priv->input);
	return IRQ_HANDLED;
error:
	disable_irq_nosync(priv->irq);
	schedule_delayed_work(&priv->reenable_work, 100);
	return IRQ_HANDLED;
}

static int ar1020_i2c_open(struct input_dev *idev)
{
	struct ar1020_i2c_priv *priv = input_get_drvdata(idev);

	if (priv->use_count++ == 0)
		enable_irq(priv->irq);
	return 0;
}

static void ar1020_i2c_close(struct input_dev *idev)
{
	struct ar1020_i2c_priv *priv = input_get_drvdata(idev);

	if (--priv->use_count == 0)
		disable_irq(priv->irq);
}

/*
 * After the kernel's platform specific source files have been modified to
 * reference the "ar1020_i2c" driver, this function will then be called.
 * This function needs to be called to finish registering the driver.
 */
static int __devinit ar1020_i2c_probe(struct i2c_client *client,
				      const struct i2c_device_id *id)
{
	struct ar1020_i2c_priv *priv = NULL;
	struct input_dev *input_dev = NULL;
	int err = 0;
	int irq;

	pr_info("%s: begin\n", __func__);

	if (!client) {
		pr_err("AR1020 I2C: client pointer is NULL\n");
		err = -EINVAL;
		goto error;
	}

	irq = client->irq;
	if (touchIRQ)
		irq = touchIRQ;

	if (!irq) {
		pr_err("AR1020 I2C: no IRQ set for touch controller\n");
		err = -EINVAL;
		goto error;
	}

	priv = kzalloc(sizeof(struct ar1020_i2c_priv), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!priv) {
		pr_err("AR1020 I2C: kzalloc error\n");
		err = -ENOMEM;
		goto error;
	}

	if (!input_dev) {
		pr_err("AR1020 I2C: input allocate error\n");
		err = -ENOMEM;
		goto error;
	}

	priv->client = client;
	priv->irq = irq;
	priv->input = input_dev;
	INIT_DELAYED_WORK(&priv->reenable_work, irq_reenable_work);

	input_dev->name = "AR1020 Touchscreen";
	input_dev->id.bustype = BUS_I2C;

	input_dev->open = ar1020_i2c_open;
	input_dev->close = ar1020_i2c_close;

	input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);

	input_set_abs_params(input_dev, ABS_X, 0, 4095, 0, 0);
	input_set_abs_params(input_dev, ABS_Y, 0, 4095, 0, 0);
	input_set_drvdata(input_dev, priv);
	err = input_register_device(input_dev);
	if (err) {
		pr_err("AR1020 I2C: error registering input device\n");
		goto error;
	}

	/* set type and register gpio pin as our interrupt */
	err = request_threaded_irq(priv->irq, NULL, touch_irq_handler_func,
			IRQF_TRIGGER_HIGH | IRQF_ONESHOT, "AR1020 I2C IRQ",
			priv);
	if (err < 0)
		goto error1;
	disable_irq(priv->irq);			/* wait for open */
	i2c_set_clientdata(client, priv);
	/*
	 * save pointer so sysfs helper functions may also have access
	 * to private data
	 */
	g_priv = priv;
	return 0;

error1:
	input_unregister_device(input_dev);
error:
	if (input_dev)
		input_free_device(input_dev);

	kfree(priv);
	return err;

}

/*
 * Unregister/remove the kernel driver from memory.
 */
static int ar1020_i2c_remove(struct i2c_client *client)
{
	struct ar1020_i2c_priv *priv =
			(struct ar1020_i2c_priv *)i2c_get_clientdata(client);

	free_irq(priv->irq, priv);
	input_unregister_device(priv->input);
	kfree(priv);
	g_priv = NULL;
	return 0;
}

/* This structure describe a list of supported slave chips */
static const struct i2c_device_id ar1020_i2c_id[] = {
	{ "ar1020_i2c", 0 },
	{ }
};

/*
 * This is the initial set of information the kernel has
 * before probing drivers on the system
 */
static struct i2c_driver ar1020_i2c_driver = {
	.driver = {
		.name	= "ar1020_i2c",
	},
	.probe		= ar1020_i2c_probe,
	.remove		= ar1020_i2c_remove,
	/*
	 * suspend/resume functions not needed since controller automatically
	 * put's itself to sleep mode after configurable short period of time
	 */
	.suspend	= NULL,
	.resume		= NULL,
	.id_table	= ar1020_i2c_id,
};

/*
 * This function is called during startup even if the platform specific
 * files have not been setup yet.
 */
static int __init ar1020_i2c_init(void)
{
	int retval;

	pr_debug("AR1020 I2C: ar1020_i2c_init: begin\n");
	strcpy(receiveBuffer, "");
	strcpy(sendBuffer, "");

	/*
	 * Creates a kobject "ar1020" that appears as a sub-directory
	 * under "/sys/kernel".
	 */
	ar1020_kobj = kobject_create_and_add("ar1020", kernel_kobj);
	if (!ar1020_kobj) {
		pr_err("AR1020 I2C: cannot create kobject\n");
		return -ENOMEM;
	}

	/* Create the files associated with this kobject */
	retval = sysfs_create_group(ar1020_kobj, &attr_group);
	if (retval) {
		pr_err("AR1020 I2C: error registering ar1020-i2c driver's sysfs interface\n");
		kobject_put(ar1020_kobj);
	}

	return i2c_add_driver(&ar1020_i2c_driver);
}

/*
 * This function is called after ar1020_i2c_remove() immediately before
 * being removed from the kernel.
 */
static void __exit ar1020_i2c_exit(void)
{
	pr_debug("AR1020 I2C: ar1020_i2c_exit begin\n");
	kobject_put(ar1020_kobj);
	i2c_del_driver(&ar1020_i2c_driver);
}

MODULE_AUTHOR("Steve Grahovac <steve.grahovac@microchip.com>");
MODULE_DESCRIPTION("AR1020 touchscreen I2C bus driver");
MODULE_LICENSE("GPL");

module_init(ar1020_i2c_init);
module_exit(ar1020_i2c_exit);
