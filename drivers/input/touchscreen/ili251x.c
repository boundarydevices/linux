// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2018, emlix GmbH. All rights reserved. */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/irq.h>

#define MAX_FINGERS		10
#define REG_TOUCHDATA		0x10
#define TOUCHDATA_FINGERS	6
#define REG_TOUCHDATA2		0x14
#define TOUCHDATA2_FINGERS	4
#define REG_PANEL_INFO		0x20
#define REG_FIRMWARE_VERSION	0x40
#define REG_PROTO_VERSION	0x42
#define REG_CALIBRATE		0xcc

struct finger {
	u8 x_high:6;
	u8 dummy:1;
	u8 status:1;
	u8 x_low;
	u8 y_high;
	u8 y_low;
	u8 pressure;
} __packed;

struct touchdata {
	u8 status;
	struct finger fingers[MAX_FINGERS];
} __packed;

struct panel_info {
	u8 x_low;
	u8 x_high;
	u8 y_low;
	u8 y_high;
	u8 xchannel_num;
	u8 ychannel_num;
	u8 max_fingers;
} __packed;

struct firmware_version {
	u8 id;
	u8 major;
	u8 minor;
} __packed;

struct protocol_version {
	u8 major;
	u8 minor;
} __packed;

struct ili251x_data {
	struct i2c_client *client;
	struct input_dev *input;
	unsigned int max_fingers;
	bool use_pressure;
	struct gpio_desc *reset_gpio;
};

static int ili251x_read_reg(struct i2c_client *client, u8 reg, void *buf,
			    size_t len)
{
	struct i2c_msg msg[2] = {
		{
			.addr	= client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= &reg,
		},
		{
			.addr	= client->addr,
			.flags	= I2C_M_RD,
			.len	= len,
			.buf	= buf,
		}
	};

	if (i2c_transfer(client->adapter, msg, 2) != 2) {
		dev_err(&client->dev, "i2c transfer failed\n");
		return -EIO;
	}

	return 0;
}

static void ili251x_report_events(struct ili251x_data *data,
				  const struct touchdata *touchdata)
{
	struct input_dev *input = data->input;
	unsigned int i;
	bool touch;
	unsigned int x, y;
	const struct finger *finger;
	unsigned int reported_fingers = 0;

	/* the touch chip does not count the real fingers but switches between
	 * 0, 6 and 10 reported fingers *
	 *
	 * FIXME: With a tested ili2511 we received only garbage for fingers
	 *        6-9. As workaround we add a device tree option to limit the
	 *        handled number of fingers
	 */
	if (touchdata->status == 1)
		reported_fingers = 6;
	else if (touchdata->status == 2)
		reported_fingers = 10;

	for (i = 0; i < reported_fingers && i < data->max_fingers; i++) {
		input_mt_slot(input, i);

		finger = &touchdata->fingers[i];

		touch = finger->status;
		input_mt_report_slot_state(input, MT_TOOL_FINGER, touch);
		x = finger->x_low | (finger->x_high << 8);
		y = finger->y_low | (finger->y_high << 8);

		if (touch) {
			input_report_abs(input, ABS_MT_POSITION_X, x);
			input_report_abs(input, ABS_MT_POSITION_Y, y);
			if (data->use_pressure)
				input_report_abs(input, ABS_MT_PRESSURE,
						 finger->pressure);

		}
	}

	input_mt_report_pointer_emulation(input, false);
	input_sync(input);
}

static irqreturn_t ili251x_irq(int irq, void *irq_data)
{
	struct ili251x_data *data = irq_data;
	struct i2c_client *client = data->client;
	struct touchdata touchdata;
	int error;

	error = ili251x_read_reg(client, REG_TOUCHDATA,
				 &touchdata,
				 sizeof(touchdata) -
					sizeof(struct finger)*TOUCHDATA2_FINGERS);

	if (!error && touchdata.status == 2 && data->max_fingers > 6)
		error = ili251x_read_reg(client, REG_TOUCHDATA2,
					 &touchdata.fingers[TOUCHDATA_FINGERS],
					 sizeof(struct finger)*TOUCHDATA2_FINGERS);

	if (!error)
		ili251x_report_events(data, &touchdata);
	else
		dev_err(&client->dev,
			"Unable to get touchdata, err = %d\n", error);

	return IRQ_HANDLED;
}

static void ili251x_reset(struct ili251x_data *data)
{
	if (data->reset_gpio) {
		gpiod_set_value(data->reset_gpio, 1);
		usleep_range(50, 100);
		gpiod_set_value(data->reset_gpio, 0);
		msleep(100);
	}
}

static int ili251x_i2c_probe(struct i2c_client *client,
				       const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct ili251x_data *data;
	struct input_dev *input;
	struct panel_info panel;
	struct device_node *np = dev->of_node;
	struct firmware_version firmware;
	struct protocol_version protocol;
	int xmax, ymax;
	int error;

	dev_dbg(dev, "Probing for ili251x I2C Touschreen driver");

	if (client->irq <= 0) {
		dev_err(dev, "No IRQ!\n");
		return -EINVAL;
	}

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	input = devm_input_allocate_device(dev);
	if (!data || !input)
		return -ENOMEM;

	data->client = client;
	data->input = input;
	data->use_pressure = false;

	data->reset_gpio = devm_gpiod_get_optional(dev, "reset",
						   GPIOD_OUT_HIGH);
	if (IS_ERR(data->reset_gpio)) {
		error = PTR_ERR(data->reset_gpio);
		if (error != -EPROBE_DEFER)
			dev_err(dev,
				"Failed to get reset GPIO: %d\n", error);
		return error;
	}

	ili251x_reset(data);

	error = ili251x_read_reg(client, REG_FIRMWARE_VERSION,
				 &firmware, sizeof(firmware));
	if (error) {
		dev_err(dev, "Failed to get firmware version, err: %d\n",
			error);
		return error;
	}

	error = ili251x_read_reg(client, REG_PROTO_VERSION,
				 &protocol, sizeof(protocol));
	if (error) {
		dev_err(dev, "Failed to get protocol version, err: %d\n",
			error);
		return error;
	}
	if (protocol.major != 3) {
		dev_err(dev, "This driver expects protocol version 3.x, Chip uses: %d\n",
				protocol.major);
		return -EINVAL;
	}

	error = ili251x_read_reg(client, REG_PANEL_INFO, &panel, sizeof(panel));
	if (error) {
		dev_err(dev, "Failed to get panel information, err: %d\n",
			error);
		return error;
	}

	data->max_fingers = panel.max_fingers;
	if (np) {
		int max_fingers;

		error = of_property_read_u32(np, "max-fingers", &max_fingers);
		if (!error && max_fingers < data->max_fingers)
			data->max_fingers = max_fingers;

		if (of_property_read_bool(np, "pressure"))
			data->use_pressure = true;
	}

	xmax = panel.x_low | (panel.x_high << 8);
	ymax = panel.y_low | (panel.y_high << 8);

	/* Setup input device */
	input->name = "ili251x Touchscreen";
	input->id.bustype = BUS_I2C;
	input->dev.parent = dev;

	__set_bit(EV_SYN, input->evbit);
	__set_bit(EV_KEY, input->evbit);
	__set_bit(EV_ABS, input->evbit);
	__set_bit(BTN_TOUCH, input->keybit);

	/* Single touch */
	input_set_abs_params(input, ABS_X, 0, xmax, 0, 0);
	input_set_abs_params(input, ABS_Y, 0, ymax, 0, 0);

	/* Multi touch */
	input_mt_init_slots(input, data->max_fingers, 0);
	input_set_abs_params(input, ABS_MT_POSITION_X, 0, xmax, 0, 0);
	input_set_abs_params(input, ABS_MT_POSITION_Y, 0, ymax, 0, 0);
	if (data->use_pressure)
		input_set_abs_params(input, ABS_MT_PRESSURE, 0, U8_MAX, 0, 0);

	i2c_set_clientdata(client, data);

	error = devm_request_threaded_irq(dev, client->irq, NULL, ili251x_irq,
				 IRQF_ONESHOT, client->name, data);

	if (error) {
		dev_err(dev, "Unable to request touchscreen IRQ, err: %d\n",
			error);
		return error;
	}

	error = input_register_device(data->input);
	if (error) {
		dev_err(dev, "Cannot register input device, err: %d\n", error);
		return error;
	}

	device_init_wakeup(dev, 1);

	dev_info(dev,
		"ili251x initialized (IRQ: %d), firmware version %d.%d.%d fingers %d",
		client->irq, firmware.id, firmware.major, firmware.minor,
		data->max_fingers);

	return 0;
}

static int __maybe_unused ili251x_i2c_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	if (device_may_wakeup(&client->dev))
		enable_irq_wake(client->irq);

	return 0;
}

static int __maybe_unused ili251x_i2c_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	if (device_may_wakeup(&client->dev))
		disable_irq_wake(client->irq);

	return 0;
}

static SIMPLE_DEV_PM_OPS(ili251x_i2c_pm,
			 ili251x_i2c_suspend, ili251x_i2c_resume);

static const struct i2c_device_id ili251x_i2c_id[] = {
	{ "ili251x", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ili251x_i2c_id);

static struct i2c_driver ili251x_ts_driver = {
	.driver = {
		.name = "ili251x_i2c",
		.pm = &ili251x_i2c_pm,
	},
	.id_table = ili251x_i2c_id,
	.probe = ili251x_i2c_probe,
};

module_i2c_driver(ili251x_ts_driver);

MODULE_AUTHOR("Philipp Puschmann <pp@emlix.com>");
MODULE_DESCRIPTION("ili251x I2C Touchscreen Driver");
MODULE_LICENSE("GPL");


