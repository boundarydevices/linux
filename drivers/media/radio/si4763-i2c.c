/*
 * Copyright (C) 2010-2013 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/mutex.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-common.h>

#include "si4763-i2c.h"

/* module parameters */
static int debug;
module_param(debug, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, "Debug level (0 - 2)");

MODULE_DESCRIPTION("I2C driver for Si4763 FM Radio Transmitter");

/* Radio Nr */
static int radio_nr = -1;

/* radio_si4713_fops - file operations interface */
static const struct v4l2_file_operations radio_si4763_fops = {
	.owner          = THIS_MODULE,
	.ioctl          = video_ioctl2,
};

#define DEFAULT_RDS_PI			0x00
#define DEFAULT_RDS_PTY			0x00
#define DEFAULT_RDS_PS_NAME		""
#define DEFAULT_RDS_RADIO_TEXT		DEFAULT_RDS_PS_NAME
#define DEFAULT_RDS_DEVIATION		0x00C8
#define DEFAULT_RDS_PS_REPEAT_COUNT	0x0003
#define DEFAULT_LIMITER_RTIME		0x1392
#define DEFAULT_LIMITER_DEV		0x102CA
#define DEFAULT_PILOT_FREQUENCY	0x4A38
#define DEFAULT_PILOT_DEVIATION		0x1A5E
#define DEFAULT_ACOMP_ATIME		0x0000
#define DEFAULT_ACOMP_RTIME		0xF4240L
#define DEFAULT_ACOMP_GAIN		0x0F
#define DEFAULT_ACOMP_THRESHOLD	(-0x28)
#define DEFAULT_MUTE			0x01
#define DEFAULT_POWER_LEVEL		88
#define DEFAULT_FREQUENCY		8800
#define DEFAULT_PREEMPHASIS		FMPE_EU
#define DEFAULT_TUNE_RNL		0xFF

#define to_si4763_device(sd)	container_of(sd, struct si4763_device, sd)

/* frequency domain transformation (using times 10 to avoid floats) */
#define FREQ_RANGE_LOW			8750
#define FREQ_RANGE_HIGH			10800

#define MAX_ARGS 7

#define RDS_BLOCK			8
#define RDS_BLOCK_CLEAR			0x03
#define RDS_BLOCK_LOAD			0x04
#define RDS_RADIOTEXT_2A		0x20
#define RDS_RADIOTEXT_BLK_SIZE		4
#define RDS_RADIOTEXT_INDEX_MAX		0x0F
#define RDS_CARRIAGE_RETURN		0x0D

#define rds_ps_nblocks(len)	((len / RDS_BLOCK) + (len % RDS_BLOCK ? 1 : 0))

#define get_status_bit(p, b, m)	(((p) & (m)) >> (b))
#define set_bits(p, v, b, m)	(((p) & ~(m)) | ((v) << (b)))

#define ATTACK_TIME_UNIT		500

#define POWER_OFF			0x00
#define POWER_ON			0x01

#define msb(x)				((u8)((u16) x >> 8))
#define lsb(x)				((u8)((u16) x &  0x00FF))
#define compose_u16(msb, lsb)		(((u16)msb << 8) | lsb)
#define compose_u32(msb, lsb)		(((u32)msb << 16) | lsb)
#define check_command_failed(status)	(!(status & SI4713_CTS) || \
					(status & SI4713_ERR))
/* mute definition */
#define set_mute(p)			((p & 1) | ((p & 1) << 1));
#define get_mute(p)			(p & 0x01)

#ifdef DEBUG
#define DBG_BUFFER(device, message, buffer, size)			\
	{								\
		int i;							\
		char str[(size)*5];					\
		for (i = 0; i < size; i++)				\
			sprintf(str + i * 5, " 0x%02x", buffer[i]);	\
		v4l2_dbg(2, debug, device, "%s:%s\n", message, str);	\
	}
#else
#define DBG_BUFFER(device, message, buffer, size)
#endif

/*
 * si4713_send_command - sends a command to si4713 and waits its response
 * @sdev: si4713_device structure for the device we are communicating
 * @command: command id
 * @args: command arguments we are sending (up to 7)
 * @argn: actual size of @args
 * @response: buffer to place the expected response from the device (up to 15)
 * @respn: actual size of @response
 * @usecs: amount of time to wait before reading the response (in usecs)
 */
static int si4713_send_command(struct si4763_device *sdev, const u8 command,
		const u8 args[], const int argn, u8 response[],
		const int respn, const int usecs)
{
	struct i2c_client *client = sdev->client;
	u8 data1[MAX_ARGS + 1];
	int ret;
	int i;
	if (!client->adapter)
		return -ENODEV;

	/* First send the command and its arguments */
	data1[0] = command;
	memcpy(data1 + 1, args, argn);
	DBG_BUFFER(&sdev->sd, "Parameters", data1, argn + 1);

	ret = i2c_master_send(client, data1, argn + 1);
	if (ret != argn + 1) {
		v4l2_err(&sdev->sd, "Failed to send cmd 0x%02x\n", command);
		return (ret > 0) ? -EIO : ret;
	}

	/* Then get the response */
	i = 0;
	ret = i2c_master_recv(client, response, respn);
	while (i++ < usecs && ((response[0] & 0x80) != 0x80)) {
		ret = i2c_master_recv(client, response, respn);
		udelay(1);
	}

	if (ret != respn) {
		v4l2_err(&sdev->sd, "No response for cmd 0x%02x\n", command);
		return (ret > 0) ? -EIO : ret;
	}

	DBG_BUFFER(&sdev->sd, "Response", response, respn);
	if (check_command_failed(response[0]))
		return -EBUSY;

	return 0;
}

/*
 * si4713_write_property - modifies a si4713 property
 * @sdev: si4713_device structure for the device we are communicating
 * @prop: property identification number
 * @val: new value for that property
 */
static int si4713_write_property(struct si4763_device *sdev, u16 prop, u16 val)
{
	int ret;
	u8 resp[SI4713_SET_PROP_NRESP];
	/*
	 *	.First byte = 0
	 *	.Second byte = property's MSB
	 *	.Third byte = property's LSB
	 *	.Fourth byte = value's MSB
	 *	.Fifth byte = value's LSB
	 */
	const u8 args[SI4713_SET_PROP_NARGS] = {
		0x00,
		msb(prop),
		lsb(prop),
		msb(val),
		lsb(val),
	};

	ret = si4713_send_command(sdev, SI4713_CMD_SET_PROPERTY,
			args, ARRAY_SIZE(args), resp, ARRAY_SIZE(resp),
			DEFAULT_TIMEOUT);

	if (ret < 0)
		return ret;

	v4l2_dbg(1, debug, &sdev->sd,
			"%s: property=0x%02x value=0x%02x status=0x%02x\n",
			__func__, prop, val, resp[0]);

	/*
	 * As there is no command response for SET_PROPERTY,
	 * wait Tcomp time to finish before proceed, in order
	 * to have property properly set.
	 */
	msleep(TIMEOUT_SET_PROPERTY);

	return ret;
}

static int si4713_enable_digitalout(struct si4763_device *sdev)
{
	int ret;
	u8 resp[SI4713_SET_PROP_NRESP];
	/*
	 *      .First byte = 0
	 *      .Second byte = property's MSB
	 *      .Third byte = property's LSB
	 *      .Fourth byte = value's MSB
	 *      .Fifth byte = value's LSB
	 */
	const u8 args[SI4713_EN_DIGITALOUT_NARGS] = {
		10,
		10,
		12,
		0,
	};

	ret = si4713_send_command(sdev, SI4713_CMD_EN_DIGITALOUT,
			args, ARRAY_SIZE(args), resp, ARRAY_SIZE(resp),
			DEFAULT_TIMEOUT);

	if (ret < 0)
		return ret;

	/*
	* As there is no command response for SET_PROPERTY,
	* wait Tcomp time to finish before proceed, in order
	 * to have property properly set.
	*/
	msleep(TIMEOUT_SET_PROPERTY);

	return ret;
}

static int si4713_get_digitalout(struct si4763_device *sdev, u32 *response)
{
	u8 resp[5];
	u16 temp1, temp2;
	int ret;

	/*
	 *      .First byte = 0
	 *      .Second byte = property's MSB
	 *      .Third byte = property's LSB
	 *      .Fourth byte = value's MSB
	 *      .Fifth byte = value's LSB
	 */
	const u8 args[SI4713_EN_DIGITALOUT_NARGS] = {
		0,
		0,
		0,
		0,
	};

	ret = si4713_send_command(sdev, SI4713_CMD_EN_DIGITALOUT,
			args, ARRAY_SIZE(args), resp, ARRAY_SIZE(resp),
			DEFAULT_TIMEOUT);

	if (ret < 0)
		return ret;

	temp1 = compose_u16(resp[1], resp[2]);
	temp2 = compose_u16(resp[3], resp[4]);
	*response = compose_u32(temp1, temp2);
	/*
	 * As there is no command response for SET_PROPERTY,
	 * wait Tcomp time to finish before proceed, in order
	 * to have property properly set.
	 */
	msleep(TIMEOUT_SET_PROPERTY);

	return ret;
}

/*
 * si4713_powerup - Powers the device up
 * @sdev: si4713_device structure for the device we are communicating
 */
static int si4713_powerup(struct si4763_device *sdev)
{
	int ret;
	u8 resp[SI4760_PWUP_NRESP];
	const u8 args[SI4760_PWUP_NARGS] = {
		SI4760_PWUP_ARG1,
		SI4760_PWUP_ARG2,
		SI4760_PWUP_ARG3,
		SI4760_PWUP_ARG4,
		SI4760_PWUP_ARG5,
	};

	if (sdev->power_state)
		return 0;
	ret = si4713_send_command(sdev, SI4760_CMD_POWER_UP,
					args, ARRAY_SIZE(args),
					resp, ARRAY_SIZE(resp),
					TIMEOUT_POWER_UP);

	if (!ret) {
		v4l2_dbg(1, debug, &sdev->sd, "Powerup response: 0x%02x\n",
				resp[0]);
		v4l2_dbg(1, debug, &sdev->sd, "Device in power up mode\n");
		sdev->power_state = POWER_ON;

	} else {
		v4l2_err(&sdev->sd, "Failed to disable supplies: %d\n", ret);
	}

	return ret;
}

/*
 * si4713_powerdown - Powers the device down
 * @sdev: si4713_device structure for the device we are communicating
 */
static int si4713_powerdown(struct si4763_device *sdev)
{
	int ret;
	u8 resp[SI4760_PWDN_NRESP];

	ret = si4713_send_command(sdev, SI4760_CMD_POWER_DOWN,
			NULL, 0, resp, ARRAY_SIZE(resp),
			DEFAULT_TIMEOUT);

	if (!ret) {
		v4l2_dbg(1, debug, &sdev->sd, "Power down response: 0x%02x\n",
				resp[0]);
		v4l2_dbg(1, debug, &sdev->sd, "Device in reset mode\n");
		if (ret)
			v4l2_err(&sdev->sd,
				 "Failed to disable supplies: %d\n", ret);
		sdev->power_state = POWER_OFF;
	}

	return ret;
}

/*
 * si4713_checkrev - Checks if we are treating a device with the correct rev.
 * @sdev: si4713_device structure for the device we are communicating
 */
static int si4713_checkrev(struct si4763_device *sdev)
{
	struct i2c_client *client = sdev->client;
	int ret;
	u8 resp[SI4760_GETREV_NRESP];

	mutex_lock(&sdev->mutex);

	ret = si4713_send_command(sdev, SI4760_CMD_GET_REV,
			NULL, 0, resp, ARRAY_SIZE(resp),
			DEFAULT_TIMEOUT);

	if (ret < 0)
		goto unlock;

	if (resp[1] == SI4760_PRODUCT_NUMBER) {
		v4l2_info(&sdev->sd, "chip found @ 0x%02x (%s)\n",
				client->addr << 1, client->adapter->name);
	} else {
		v4l2_err(&sdev->sd, "Invalid product number\n");
		ret = -EINVAL;
	}

unlock:
	mutex_unlock(&sdev->mutex);
	return ret;
}

/*
 * si4713_wait_stc - Waits STC interrupt and clears status bits. Usefull
 *		     for TX_TUNE_POWER, TX_TUNE_FREQ and TX_TUNE_MEAS
 * @sdev: si4713_device structure for the device we are communicating
 * @usecs: timeout to wait for STC interrupt signal
 */
static int si4713_wait_stc(struct si4763_device *sdev, const int usecs)
{
	int ret;
	u8 resp[SI4713_GET_STATUS_NRESP];

	/* Clear status bits */
	ret = si4713_send_command(sdev, SI4713_CMD_GET_INT_STATUS,
			NULL, 0, resp, ARRAY_SIZE(resp),
			DEFAULT_TIMEOUT);

	if (ret < 0)
		goto exit;

	v4l2_dbg(1, debug, &sdev->sd,
			"%s: status bits: 0x%02x\n", __func__, resp[0]);

	if (!(resp[0] & SI4713_STC_INT))
		ret = -EIO;

exit:
	return ret;
}

/*
 * si4713_tx_tune_freq - Sets the state of the RF carrier and sets the tuning
 *			frequency between 76 and 108 MHz in 10 kHz units and
 *			steps of 50 kHz.
 * @sdev: si4713_device structure for the device we are communicating
 * @frequency: desired frequency (76 - 108 MHz, unit 10 KHz, step 50 kHz)
 */
static int si4713_tx_tune_freq(struct si4763_device *sdev, u16 frequency)
{
	int ret;
	u8 val[SI4713_TXFREQ_NRESP];
	/*
	*	.First byte = 0
	*	.Second byte = frequency's MSB
	*	.Third byte = frequency's LSB
	 */
	const u8 args[SI4713_TXFREQ_NARGS] = {
		0x00,
		msb(frequency),
		lsb(frequency),
		0x00,
		0x00,
	};

	ret = si4713_send_command(sdev, SI4713_CMD_TX_TUNE_FREQ,
			args, ARRAY_SIZE(args), val, ARRAY_SIZE(val),
			DEFAULT_TIMEOUT);

	if (ret < 0)
		return ret;

	return 0;
}

/*
 * si4713_tx_tune_measure - Enters receive mode and measures the received noise
 *			level in units of dBuV on the selected frequency.
 *			The Frequency must be between 76 and 108 MHz in 10 kHz
 *			units and steps of 50 kHz. The command also sets the
 *			antenna	tuning capacitance. A value of 0 means
 *			autotuning, and a value of 1 to 191 indicates manual
 *			override.
 * @sdev: si4713_device structure for the device we are communicating
 * @frequency: desired frequency (76 - 108 MHz, unit 10 KHz, step 50 kHz)
 * @antcap: value of antenna tuning capacitor (0 - 191)
 */
static int si4713_tx_tune_measure(struct si4763_device *sdev, u16 frequency,
					u8 antcap)
{
	int ret;
	u8 val[SI4713_TXMEA_NRESP];
	/*
	*	.First byte = 0
	*	.Second byte = frequency's MSB
	*	.Third byte = frequency's LSB
	*	.Fourth byte = antcap
	 */
	const u8 args[SI4713_TXMEA_NARGS] = {
		0x00,
		msb(frequency),
		lsb(frequency),
		antcap,
	};

	sdev->tune_rnl = DEFAULT_TUNE_RNL;

	if (antcap > SI4713_MAX_ANTCAP)
		return -EDOM;

	ret = si4713_send_command(sdev, SI4713_CMD_TX_TUNE_MEASURE,
				  args, ARRAY_SIZE(args), val,
				  ARRAY_SIZE(val), DEFAULT_TIMEOUT);

	if (ret < 0)
		return ret;

	v4l2_dbg(1, debug, &sdev->sd,
			"%s: frequency=0x%02x antcap=0x%02x status=0x%02x\n",
			__func__, frequency, antcap, val[0]);

	return si4713_wait_stc(sdev, TIMEOUT_TX_TUNE);
}

static int si4763_fm_rsq_status(struct si4763_device *sdev,
					u16 *frequency)
{
	int ret;
	u8 val[SI4763_FM_RSQ_STATUS_NRESP];
	/*
	*	.First byte = intack bit
	 */
	const u8 args[SI4763_FM_RSQ_STATUS_NARGS] = {
		0x00,
	};

	ret = si4713_send_command(sdev, SI4763_CMD_FM_RSQ_STATUS,
				  args, ARRAY_SIZE(args), val,
				  ARRAY_SIZE(val), DEFAULT_TIMEOUT);

	*frequency = compose_u16(val[3], val[4]);
	return ret;
}

static int si4713_set_power_state(struct si4763_device *sdev, u8 value)
{
	int ret;

	mutex_lock(&sdev->mutex);

	if (value)
		ret = si4713_powerup(sdev);
	else
		ret = si4713_powerdown(sdev);

	mutex_unlock(&sdev->mutex);
	return ret;
}

/*
 * si4713_initialize - Sets the device up with default configuration.
 * @sdev: si4713_device structure for the device we are communicating
 */
static int si4713_initialize(struct si4763_device *sdev)
{
	int ret;

	ret = si4713_set_power_state(sdev, POWER_OFF);
	if (ret < 0)
		goto exit;

	ret = si4713_set_power_state(sdev, POWER_ON);
	if (ret < 0)
		goto exit;

	ret = si4713_checkrev(sdev);
	if (ret < 0)
		goto exit;

	ret = si4713_set_power_state(sdev, POWER_OFF);
	if (ret < 0)
		goto exit;

exit:
	return ret;
}

static int si4763_setup(struct si4763_device *sdev)
{
	int ret;
	u32 temp;

	ret = si4713_write_property(sdev, 0x0202, 0xBB80);
	if (ret < 0)
		goto exit;

	ret = si4713_write_property(sdev, 0x0203, 0x0400);
	if (ret < 0)
		goto exit;
	ret = si4713_tx_tune_freq(sdev, FREQ_RANGE_LOW);
	if (ret < 0)
		goto exit;

	ret = si4713_enable_digitalout(sdev);
	if (ret < 0)
		goto exit;

	ret = si4713_get_digitalout(sdev, &temp);
	if (ret < 0)
		goto exit;

exit:
	return ret;
}

/*
 * Video4Linux Subdev Interface
 */

/* si4713_ioctl - deal with private ioctls (only rnl for now) */
long si4713_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct si4763_device *sdev = to_si4763_device(sd);
	struct si4713_rnl *rnl = arg;
	int frequency = 0;
	int ret = 0;

	if (!arg)
		return -EINVAL;

	mutex_lock(&sdev->mutex);
	switch (cmd) {
	case SI4713_IOC_MEASURE_RNL:
		if (sdev->power_state) {
			/* Set desired measurement frequency */
			ret = si4713_tx_tune_measure(sdev, frequency, 0);
			if (ret < 0)
				goto unlock;
			/* get results from tune status */
		}
		rnl->rnl = sdev->tune_rnl;
		break;

	default:
		/* nothing */
		ret = -ENOIOCTLCMD;
	}

unlock:
	mutex_unlock(&sdev->mutex);
	return ret;
}

/* si4763_g_frequency - get tuner or modulator radio frequency */
static int si4763_g_frequency(struct file *file, void *priv,
		struct v4l2_frequency *f)
{

	struct si4763_device *sdev = video_drvdata(file);
	int ret = 0;

	f->type = V4L2_TUNER_RADIO;

	mutex_lock(&sdev->mutex);

	if (sdev->power_state) {
		u16 freq;

		ret = si4763_fm_rsq_status(sdev, &freq);
		if (ret < 0)
			goto unlock;

		sdev->frequency = freq;
	}

	f->frequency = sdev->frequency;

unlock:
	mutex_unlock(&sdev->mutex);
	return ret;
}

/* si4713_s_frequency - set tuner or modulator radio frequency */
static int si4763_s_frequency(struct file *file, void *priv,
		struct v4l2_frequency *f)
{
	struct si4763_device *sdev = video_drvdata(file);
	u16 frequency = f->frequency;
	int ret = 0;

	/* Check frequency range */
	if (frequency < FREQ_RANGE_LOW || frequency > FREQ_RANGE_HIGH)
		return -EDOM;

	mutex_lock(&sdev->mutex);

	if (sdev->power_state) {
		ret = si4713_tx_tune_freq(sdev, frequency);
		if (ret < 0)
			goto unlock;
		frequency = ret;
		ret = 0;
	}
	sdev->frequency = frequency;
	f->frequency = frequency;

unlock:
	mutex_unlock(&sdev->mutex);
	return ret;
}

int si4763_fops_open(struct file *file)
{
	struct si4763_device *radio = video_drvdata(file);
	int retval = 0;

	if (radio->users != 0)
		return -ENODEV;
	/* start radio */
	si4713_set_power_state(radio, POWER_ON);
	si4763_setup(radio);
	radio->users = 1;

	return retval;
}

int si4763_fops_release(struct file *file)
{
	struct si4763_device *radio = video_drvdata(file);
	int retval = 0;

	/* safety check */
	if (!radio)
		return -ENODEV;

	/* stop radio */
	si4713_set_power_state(radio, POWER_OFF);
	radio->users = 0;
	return retval;
}

/*
 * si4763_fops - file operations interface
 * video_ioctl2 is part of the v4l2 implementations. Change this pointer to the
 * ioctl function you want to implement, in case you don't want to be part of
 * v4l2.
 */
static const struct v4l2_file_operations si4763_fops = {
	.owner                  = THIS_MODULE,
	.ioctl                  = video_ioctl2,
	.open                   = si4763_fops_open,
	.release                = si4763_fops_release,
};

/* si4763_ioctl_ops - video device ioctl operations */
static const struct v4l2_ioctl_ops si4763_ioctl_ops = {
	.vidioc_g_frequency     = si4763_g_frequency,
	.vidioc_s_frequency     = si4763_s_frequency,
};

struct video_device si4763_viddev_template = {
	.fops                   = &si4763_fops,
	.name                   = "si4763_i2c",
	.release                = video_device_release,
	.ioctl_ops              = &si4763_ioctl_ops,
};

static int si4713_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct si4763_device *sdev;
	int ret;

	sdev = kzalloc(sizeof *sdev, GFP_KERNEL);
	if (!sdev) {
		dev_err(&client->dev, "Failed to alloc video device.\n");
		ret = -ENOMEM;
		goto exit;
	}
	sdev->client = client;
	sdev->users = 0;
	/* video device allocation and initialization */
	sdev->videodev = video_device_alloc();
	if (!sdev->videodev) {
		ret = -ENOMEM;
		goto free_video;
	}

	mutex_init(&sdev->mutex);
	memcpy(sdev->videodev, &si4763_viddev_template,
			sizeof(si4763_viddev_template));
	video_set_drvdata(sdev->videodev, sdev);

	ret = si4713_initialize(sdev);
	if (ret < 0) {
		v4l2_err(&sdev->sd, "Failed to probe device information.\n");
		goto free_sdev;
	}

	/* register video device */
	ret = video_register_device(sdev->videodev, VFL_TYPE_RADIO,
			radio_nr);
	if (ret) {
		dev_warn(&client->dev, "Could not register video device\n");
		goto free_video;
	}
	i2c_set_clientdata(client, sdev);
	return 0;

free_video:
	video_device_release(sdev->videodev);
free_sdev:
	kfree(sdev);
exit:
	return ret;
}

static int si4713_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct si4763_device *sdev = to_si4763_device(sd);

	if (sdev->power_state)
		si4713_set_power_state(sdev, POWER_DOWN);

	if (client->irq > 0)
		free_irq(client->irq, sdev);

	v4l2_device_unregister_subdev(sd);
	kfree(sdev);

	return 0;
}

static const struct i2c_device_id si4713_id[] = {
	{ "si4763_i2c" , 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, si4713_id);

static struct i2c_driver si4713_i2c_driver = {
	.driver	= {
		.name = "si4763_i2c",
	},
	.probe = si4713_probe,
	.remove = si4713_remove,
	.id_table = si4713_id,
};
module_i2c_driver(si4713_i2c_driver);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("I2C si4763");
MODULE_LICENSE("GPL");

