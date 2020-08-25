// SPDX-License-Identifier: GPL-2.0-only
/*
 * Driver for I2C connected EETI EXC3000 multiple touch controller
 *
 * Copyright (C) 2017 Ahmet Inan <inan@distec.de>
 *
 * minimal implementation based on egalax_ts.c and egalax_i2c.c
 */

#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/input/touchscreen.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/timer.h>
#include <asm/unaligned.h>

#define EXC3000_NUM_SLOTS		10
#define EXC3000_SLOTS_PER_FRAME		5
#define EXC3000_LEN_FRAME		66
#define EXC3000_LEN_POINT		10
#define EXC3000_MT_EVENT		6
#define EXC3000_MT_EVENT2		0x18
#define EXC3000_TIMEOUT_MS		100

struct exc3000_data {
	struct i2c_client *client;
	struct input_dev *input;
	struct touchscreen_properties prop;
	struct timer_list timer;
	struct gpio_desc	*reset_gpio;
	u32 frame_size;
	u32 slots_per_frame;
	u32 resolution;
	u32 query_resolution;
	u8 buf[2 * EXC3000_LEN_FRAME];
};

static void exc3000_report_slots(struct exc3000_data *data,
				 struct input_dev *input,
				 const u8 *buf, int num)
{
	struct touchscreen_properties *prop = &data->prop;
	u32 r = data->resolution;
	u32 q = data->query_resolution;

	for (; num--; buf += EXC3000_LEN_POINT) {
		if (buf[0] & BIT(0)) {
			unsigned i, j;
			input_mt_slot(input, buf[1]);
			input_mt_report_slot_state(input, MT_TOOL_FINGER, true);

			i = get_unaligned_le16(buf + 2);
			j = get_unaligned_le16(buf + 4);
			if (r != q) {
				if (r == (q >> 2)) {
					i <<= 2; j <<= 2;
				} else if ((r >> 2) == q) {
					i >>= 2; j >>= 2;
				} else {
					i = (i * q) / r;
					j = (j * q) / r;
				}
			}
			touchscreen_report_pos(input, prop, i, j, true);
		}
	}
}

static void exc3000_timer(struct timer_list *t)
{
	struct exc3000_data *data = from_timer(data, t, timer);

	input_mt_sync_frame(data->input);
	input_sync(data->input);
}

static unsigned char versio_10bytes[] = "D0.002";

static int exc3000_read_frame(struct exc3000_data *data, u8 *buf)
{
	struct i2c_client *client = data->client;
	unsigned char startch[] = { '\'', 0 };
	struct i2c_msg readpkt[2] = {
		{client->addr, 0, 2, startch},
		{client->addr, I2C_M_RD, data->frame_size, buf}
	};
	int ret;
	u32 resolution;
	u32 frame_size;

	ret = i2c_transfer(client->adapter, readpkt,
			   ARRAY_SIZE(readpkt));
	if (ret != ARRAY_SIZE(readpkt)) {
		dev_err(&client->dev,
			"i2c_transfer failed(%d)\n", ret);
		msleep(100);
		return (ret < 0) ? ret : -EIO;
	}

	frame_size = get_unaligned_le16(buf);
	if ((frame_size == 0x42) && (buf[2] == 0x03)) {
		if (strcmp(&buf[4], versio_10bytes) < 0) {
			data->frame_size = EXC3000_LEN_FRAME;
			data->slots_per_frame = EXC3000_SLOTS_PER_FRAME;
		} else {
			data->frame_size = 10;
			data->slots_per_frame = 1;
		}
		buf[10] = 0;
		pr_info("%s: frame_size=%d version=%s\n", __func__,
				data->frame_size, &buf[4]);
		return -EINVAL;
	}
	if (buf[2] == EXC3000_MT_EVENT) {
		resolution = 4095;
	} else if (buf[2] == EXC3000_MT_EVENT2) {
		resolution = 16383;
	} else {
		return -EINVAL;
	}
	ret = data->frame_size;
	if (data->frame_size != frame_size) {
		if (frame_size == 10) {
			ret = data->frame_size = frame_size;
			data->slots_per_frame = 1;
			pr_info("%s: frame_size=%d\n", __func__, data->frame_size);
		} else if (frame_size == EXC3000_LEN_FRAME) {
			data->frame_size = frame_size;
			data->slots_per_frame = EXC3000_SLOTS_PER_FRAME;
			pr_info("%s: frame_size=%d\n", __func__, data->frame_size);
			return -EINVAL;
		} else {
			return -EINVAL;
		}
	}
	if (data->resolution != resolution)
		data->resolution = resolution;
	return ret;
}

static int exc3000_read_data(struct exc3000_data *data,
			     u8 *buf, int *n_slots)
{
	int read_cnt;
	int n;

	read_cnt = exc3000_read_frame(data, buf);
	if (read_cnt < 0)
		return read_cnt;

	*n_slots = n = buf[3];
	if (!n || n > EXC3000_NUM_SLOTS)
		return -EINVAL;

	while (n > data->slots_per_frame) {
		buf += read_cnt;
		n -= data->slots_per_frame;
		/* Read 2nd frame to get the rest of the contacts. */
		read_cnt = exc3000_read_frame(data, buf);
		if (read_cnt)
			return read_cnt;

		/* 2nd chunk must have number of contacts set to 0. */
		if (buf[3] != 0)
			return -EINVAL;
	}

	return 0;
}

static irqreturn_t exc3000_interrupt(int irq, void *dev_id)
{
	struct exc3000_data *data = dev_id;
	struct input_dev *input = data->input;
	u8 *buf = data->buf;
	int slots, total_slots;
	int error;

	buf[3] = 0;
	error = exc3000_read_data(data, buf, &total_slots);
	if (!input)
		goto out;
	if (error) {
		pr_debug("%s: buf = %02x %02x %02x %02x, ret=%d\n", __func__,
			buf[0], buf[1], buf[2], buf[3], error);
		/* Schedule a timer to release "stuck" contacts */
		mod_timer(&data->timer,
			  jiffies + msecs_to_jiffies(EXC3000_TIMEOUT_MS));
		goto out;
	}

	/*
	 * We read full state successfully, no contacts will be "stuck".
	 */
	del_timer_sync(&data->timer);

	while (total_slots > 0) {
		slots = min(total_slots, (int)data->slots_per_frame);
		exc3000_report_slots(data, input, buf + 4, slots);
		total_slots -= slots;
		buf += data->frame_size;
	}

	input_mt_sync_frame(input);
	input_sync(input);

out:
	return IRQ_HANDLED;
}

static int exc3000_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct exc3000_data *data;
	struct input_dev *input;
	struct gpio_desc *gp;
	int error;
	int retry = 0;
	u32 resolution = 4095;
	unsigned char buf[0x67];

	data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	if (!device_property_read_u32(&client->dev, "resolution",
			&resolution)) {
		if (resolution < 4095) {
			dev_warn(&client->dev, "resolution of %d unsupported\n", resolution);
			resolution = 4095;
		}
	}
	data->client = client;
	data->resolution = resolution;
	data->query_resolution = resolution;
	data->slots_per_frame = EXC3000_SLOTS_PER_FRAME;
	data->frame_size = EXC3000_LEN_FRAME;

	timer_setup(&data->timer, exc3000_timer, 0);

	gp = devm_gpiod_get_optional(&client->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(gp)) {
		error = PTR_ERR(gp);
		if (error != -EPROBE_DEFER)
			dev_dbg(&client->dev, "Failed to get reset GPIO: %d\n", error);
		return error;
	}
	/* release reset */
	data->reset_gpio = gp;
	msleep(1);
	gpiod_set_value(gp, 0);

	/* probe for device */
	while (1) {
		msleep(5);
		error = i2c_master_send(client, NULL, 0);
		if (error >= 0)
			break;
		retry++;
		if (retry > 3) {
			dev_err(&client->dev, "no device\n");
			return -ENODEV;
		}
	}

	error = devm_request_threaded_irq(&client->dev, client->irq,
					  NULL, exc3000_interrupt, IRQF_ONESHOT,
					  client->name, data);
	if (error)
		return error;

	input = devm_input_allocate_device(&client->dev);
	if (!input)
		return -ENOMEM;

	input->name = "EETI EXC3000 Touch Screen";
	input->id.bustype = BUS_I2C;

	input_set_abs_params(input, ABS_MT_POSITION_X, 0, resolution, 0, 0);
	input_set_abs_params(input, ABS_MT_POSITION_Y, 0, resolution, 0, 0);
	touchscreen_parse_properties(input, true, &data->prop);

	error = input_mt_init_slots(input, EXC3000_NUM_SLOTS,
				    INPUT_MT_DIRECT | INPUT_MT_DROP_UNUSED);
	if (error)
		return error;

	error = input_register_device(input);
	if (error)
		return error;

	data->input = input;
	input_set_abs_params(input, ABS_MT_POSITION_X, 0, resolution, 0, 0);
	input_set_abs_params(input, ABS_MT_POSITION_Y, 0, resolution, 0, 0);

	memset(buf, 0, sizeof(buf));
	buf[0] = 0x67;
	buf[2] = 0x42;
	buf[4] = 0x03;
	buf[5] = 0x01;
	buf[6] = 'D';
	i2c_master_send(client, buf, sizeof(buf));
	return 0;
}

static int exc3000_remove(struct i2c_client *client)
{
	struct exc3000_data *data = i2c_get_clientdata(client);

	if (data->reset_gpio)
		gpiod_set_value(data->reset_gpio, 1);
	return 0;
}

static const struct i2c_device_id exc3000_id[] = {
	{ "exc3000", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, exc3000_id);

#ifdef CONFIG_OF
static const struct of_device_id exc3000_of_match[] = {
	{ .compatible = "eeti,exc3000" },
	{ }
};
MODULE_DEVICE_TABLE(of, exc3000_of_match);
#endif

static struct i2c_driver exc3000_driver = {
	.driver = {
		.name	= "exc3000",
		.of_match_table = of_match_ptr(exc3000_of_match),
	},
	.id_table	= exc3000_id,
	.probe		= exc3000_probe,
	.remove		= exc3000_remove,
};

module_i2c_driver(exc3000_driver);

MODULE_AUTHOR("Ahmet Inan <inan@distec.de>");
MODULE_DESCRIPTION("I2C connected EETI EXC3000 multiple touch controller driver");
MODULE_LICENSE("GPL v2");
