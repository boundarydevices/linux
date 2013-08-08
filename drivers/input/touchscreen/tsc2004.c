/*
 * drivers/input/touchscreen/tsc2004.c
 *
 * Copyright (C) 2009 Texas Instruments Inc
 * Author: Vaibhav Hiremath <hvaibhav@ti.com>
 *
 * Using code from:
 *  - tsc2007.c
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of_gpio.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/i2c/tsc2007.h>

static int calibration[7];
module_param_array(calibration, int, NULL, S_IRUGO | S_IWUSR);

static void translate(u16 *px, u16 *py)
{
	int x, y, x1, y1;
	if (calibration[6]) {
		x1 = *px;
		y1 = *py;

		x = calibration[0] * x1 +
			calibration[1] * y1 +
			calibration[2];
		x /= calibration[6];
		if (x < 0)
			x = 0;
		y = calibration[3] * x1 +
			calibration[4] * y1 +
			calibration[5];
		y /= calibration[6];
		if (y < 0)
			y = 0;
		*px = x ;
		*py = y ;
	}
}

#define TS_PENUP_TIMEOUT_MS	40

/* Control byte 0 */
#define TSC2004_CMD0(addr, pnd, rw) ((addr<<3)|(pnd<<1)|rw)
/* Control byte 1 */
#define TSC2004_CMD1(cmd, mode, rst) ((1<<7)|(cmd<<4)|(mode<<2)|(rst<<1))

/* Command Bits */
#define READ_REG	1
#define WRITE_REG	0
#define SWRST_TRUE	1
#define SWRST_FALSE	0
#define PND0_TRUE	1
#define PND0_FALSE	0

/* Converter function mapping */
enum convertor_function {
	MEAS_X_Y_Z1_Z2,	/* Measure X,Y,z1 and Z2:	0x0 */
	MEAS_X_Y,	/* Measure X and Y only:	0x1 */
	MEAS_X,		/* Measure X only:		0x2 */
	MEAS_Y,		/* Measure Y only:		0x3 */
	MEAS_Z1_Z2,	/* Measure Z1 and Z2 only:	0x4 */
	MEAS_AUX,	/* Measure Auxillary input:	0x5 */
	MEAS_TEMP1,	/* Measure Temparature1:	0x6 */
	MEAS_TEMP2,	/* Measure Temparature2:	0x7 */
	MEAS_AUX_CONT,	/* Continuously measure Auxillary input: 0x8 */
	X_DRV_TEST,	/* X-Axis drivers tested 	0x9 */
	Y_DRV_TEST,	/* Y-Axis drivers tested 	0xA */
	/*Command Reserved*/
	SHORT_CKT_TST = 0xC,	/* Short circuit test:	0xC */
	XP_XN_DRV_STAT,	/* X+,Y- drivers status:	0xD */
	YP_YN_DRV_STAT,	/* X+,Y- drivers status:	0xE */
	YP_XN_DRV_STAT	/* Y+,X- drivers status:	0xF */
};

/* Register address mapping */
enum register_address {
	X_REG,		/* X register:		0x0 */
	Y_REG,		/* Y register:		0x1 */
	Z1_REG,		/* Z1 register:		0x2 */
	Z2_REG,		/* Z2 register:		0x3 */
	AUX_REG,	/* AUX register:	0x4 */
	TEMP1_REG,	/* Temp1 register:	0x5 */
	TEMP2_REG,	/* Temp2 register:	0x6 */
	STAT_REG,	/* Status Register:	0x7 */
	AUX_HGH_TH_REG,	/* AUX high threshold register:	0x8 */
	AUX_LOW_TH_REG,	/* AUX low threshold register:	0x9 */
	TMP_HGH_TH_REG,	/* Temp high threshold register:0xA */
	TMP_LOW_TH_REG,	/* Temp low threshold register:	0xB */
	CFR0_REG,	/* Configuration register 0:	0xC */
	CFR1_REG,	/* Configuration register 1:	0xD */
	CFR2_REG,	/* Configuration register 2:	0xE */
	CONV_FN_SEL_STAT	/* Convertor function select register:	0xF */
};

/* Supported Resolution modes */
enum resolution_mode {
	MODE_10BIT,	/* 10 bit resolution */
	MODE_12BIT		/* 12 bit resolution */
};

/* Configuraton register bit fields */
/* CFR0 */
#define PEN_STS_CTRL_MODE	(1<<15)
#define ADC_STS			(1<<14)
#define RES_CTRL		(1<<13)
#define ADC_CLK_4MHZ		(0<<11)
#define ADC_CLK_2MHZ		(1<<11)
#define ADC_CLK_1MHZ		(2<<11)
#define PANEL_VLTG_STB_TIME_0US		(0<<8)
#define PANEL_VLTG_STB_TIME_100US	(1<<8)
#define PANEL_VLTG_STB_TIME_500US	(2<<8)
#define PANEL_VLTG_STB_TIME_1MS		(3<<8)
#define PANEL_VLTG_STB_TIME_5MS		(4<<8)
#define PANEL_VLTG_STB_TIME_10MS	(5<<8)
#define PANEL_VLTG_STB_TIME_50MS	(6<<8)
#define PANEL_VLTG_STB_TIME_100MS	(7<<8)

/* CFR2 */
#define PINTS1			(1<<15)
#define PINTS0			(1<<14)
#define MEDIAN_VAL_FLTR_SIZE_1	(0<<12)
#define MEDIAN_VAL_FLTR_SIZE_3	(1<<12)
#define MEDIAN_VAL_FLTR_SIZE_7	(2<<12)
#define MEDIAN_VAL_FLTR_SIZE_15	(3<<12)
#define AVRG_VAL_FLTR_SIZE_1	(0<<10)
#define AVRG_VAL_FLTR_SIZE_3_4	(1<<10)
#define AVRG_VAL_FLTR_SIZE_7_8	(2<<10)
#define AVRG_VAL_FLTR_SIZE_16	(3<<10)
#define MAV_FLTR_EN_X		(1<<4)
#define MAV_FLTR_EN_Y		(1<<3)
#define MAV_FLTR_EN_Z		(1<<2)

#define	MAX_12BIT		((1 << 12) - 1)
#define MEAS_MASK		0xFFF

struct ts_event {
	u16	x;
	u16	y;
	u16	z1, z2;
};

struct tsc2004 {
	struct input_dev	*input;
	char			phys[32];
	struct delayed_work	work;

	struct i2c_client	*client;

	u16			x_plate_ohms;

	bool			pendown;
	int			irq;

	int			reset_gpio;
	int			reset_active_low;
	int			irq_gpio;
	int			use_count;
};

static int tsc2004_read_word_data(struct tsc2004 *tsc, u8 cmd)
{
	s32 data;
	u16 val;

	data = i2c_smbus_read_word_data(tsc->client, cmd);
	if (data < 0) {
		dev_err(&tsc->client->dev, "i2c io (read) error: %d\n", data);
		return data;
	}

	/*
	 * We need to swap byte order for little-endian cpus.
	 */
	val = be16_to_cpu(data);
	dev_dbg(&tsc->client->dev, "cmd: 0x%x, val: 0x%x\n", cmd, val);
	return val;
}

static inline int tsc2004_read_xyz_data(struct tsc2004 *tsc, u8 cmd)
{
	int val = tsc2004_read_word_data(tsc, cmd);

	/*
	 * 12 bit precision, high 4 bits should be zero
	 */
	if (val >= 0)
		val &= 0xfff;
	return val;
}

static inline int tsc2004_write_word_data(struct tsc2004 *tsc, u8 cmd, u16 data)
{
	u16 val;

	val = cpu_to_be16(data);
	return i2c_smbus_write_word_data(tsc->client, cmd, val);
}

static inline int tsc2004_write_cmd(struct tsc2004 *tsc, u8 value)
{
	return i2c_smbus_write_byte(tsc->client, value);
}

static int tsc2004_prepare_for_reading(struct tsc2004 *ts)
{
	int err;
	int cmd, data;
	int retries ;

	/* Reset the TSC, configure for 12 bit */
	retries = 0 ;
	do {
                /* Reset the TSC, configure for 12 bit */
                cmd = TSC2004_CMD1(MEAS_X_Y_Z1_Z2, MODE_12BIT, SWRST_TRUE);
                err = tsc2004_write_cmd(ts, cmd);
                if (err < 0)
                        printk (KERN_ERR "%s: write_cmd %d\n", __func__, err );
	} while ( (err < 0) && (3 < retries++) );

	if (err < 0)
		return err ;

	/* Enable interrupt for PENIRQ and DAV */
	cmd = TSC2004_CMD0(CFR2_REG, PND0_FALSE, WRITE_REG);
	data = PINTS1 | PINTS0 | MEDIAN_VAL_FLTR_SIZE_15 |
		AVRG_VAL_FLTR_SIZE_7_8 | MAV_FLTR_EN_X | MAV_FLTR_EN_Y |
		MAV_FLTR_EN_Z;
	err = tsc2004_write_word_data(ts, cmd, data);
	if (err < 0)
		return err;

	/* Configure the TSC in TSMode 1 */
	cmd = TSC2004_CMD0(CFR0_REG, PND0_FALSE, WRITE_REG);
	data = PEN_STS_CTRL_MODE | ADC_CLK_2MHZ | PANEL_VLTG_STB_TIME_1MS;
	err = tsc2004_write_word_data(ts, cmd, data);
	if (err < 0)
		return err;

	/* Enable x, y, z1 and z2 conversion functions */
	cmd = TSC2004_CMD1(MEAS_X_Y_Z1_Z2, MODE_12BIT, SWRST_FALSE);
	err = tsc2004_write_cmd(ts, cmd);
	if (err < 0)
		return err;

	return 0;
}

static void tsc2004_read_values(struct tsc2004 *tsc, struct ts_event *tc)
{
	int cmd;

	/* Read X Measurement */
	cmd = TSC2004_CMD0(X_REG, PND0_FALSE, READ_REG);
	tc->x = tsc2004_read_xyz_data(tsc, cmd);

	/* Read Y Measurement */
	cmd = TSC2004_CMD0(Y_REG, PND0_FALSE, READ_REG);
	tc->y = tsc2004_read_xyz_data(tsc, cmd);

	/* Read Z1 Measurement */
	cmd = TSC2004_CMD0(Z1_REG, PND0_FALSE, READ_REG);
	tc->z1 = tsc2004_read_xyz_data(tsc, cmd);

	/* Read Z2 Measurement */
	cmd = TSC2004_CMD0(Z2_REG, PND0_FALSE, READ_REG);
	tc->z2 = tsc2004_read_xyz_data(tsc, cmd);


	tc->x &= MEAS_MASK;
	tc->y &= MEAS_MASK;
	tc->z1 &= MEAS_MASK;
	tc->z2 &= MEAS_MASK;

	/* Prepare for touch readings */
	if (tsc2004_prepare_for_reading(tsc) < 0)
		dev_dbg(&tsc->client->dev, "Failed to prepare TSC for next"
				"reading\n");
}

static u32 tsc2004_calculate_pressure(struct tsc2004 *tsc, struct ts_event *tc)
{
	u32 rt = 0;

	/* range filtering */
	if (tc->x == MAX_12BIT)
		tc->x = 0;

	if (likely(tc->x && tc->z1)) {
		/* compute touch pressure resistance using equation #1 */
		rt = tc->z2 - tc->z1;
		rt *= tc->x;
		rt *= tsc->x_plate_ohms;
		rt /= tc->z1;
		rt = (rt + 2047) >> 12;
	}
	return rt;
}

static void tsc2004_work(struct work_struct *work)
{
	int cmd;
	int val;
	struct tsc2004 *ts = container_of(to_delayed_work(work),
				struct tsc2004, work);

	if (!ts->pendown)
		return;

	cmd = TSC2004_CMD0(CFR0_REG, PND0_FALSE, READ_REG);
	val = tsc2004_read_word_data(ts, cmd);

	if (val & 0x8000) {
		schedule_delayed_work(&ts->work, msecs_to_jiffies(TS_PENUP_TIMEOUT_MS));
	} else {
		struct input_dev *input = ts->input;

		if (ts->pendown) {
			dev_dbg(&ts->client->dev, "UP\n");
			input_report_key(input, BTN_TOUCH, 0);
			input_report_abs(input, ABS_PRESSURE, 0);
			input_sync(input);
			ts->pendown = false;
		}
	}
}


static irqreturn_t tsc2004_irq(int irq, void *handle)
{
	struct tsc2004 *ts = handle;
	struct ts_event tc;
	u32 rt;

	/*
	 * NOTE: We can't rely on the pressure to determine the pen down
	 * state, even though this controller has a pressure sensor.
	 * The pressure value can fluctuate for quite a while after
	 * lifting the pen and in some cases may not even settle at the
	 * expected value.
	 *
	 */
	if (ts->irq_gpio >= 0)
		if (gpio_get_value(ts->irq_gpio))
			return IRQ_NONE;

	cancel_delayed_work_sync(&ts->work);
	tsc2004_read_values(ts, &tc);

	rt = tsc2004_calculate_pressure(ts, &tc);

	if ((rt > 0) && (rt <= MAX_12BIT)) {
		struct input_dev *input = ts->input;

		translate(&tc.x, &tc.y);

		if (!ts->pendown) {
			dev_dbg(&ts->client->dev, "DOWN\n");

			input_report_key(input, BTN_TOUCH, 1);
			ts->pendown = true;
		}

		input_report_abs(input, ABS_X, tc.x);
		input_report_abs(input, ABS_Y, tc.y);
		input_report_abs(input, ABS_PRESSURE, rt);

		input_sync(input);

		dev_dbg(&ts->client->dev, "point(%4d,%4d), pressure (%4u)\n",
			tc.x, tc.y, rt);
	}
	schedule_delayed_work(&ts->work, msecs_to_jiffies(TS_PENUP_TIMEOUT_MS));
	return IRQ_HANDLED;
}

static int setup_reset_gpio(struct i2c_client *client, struct tsc2004 *ts)
{
	int err = 0;
	int gpio;
	enum of_gpio_flags flags;
	struct device_node *np = client->dev.of_node;

	ts->reset_gpio = -1;
	gpio = of_get_named_gpio_flags(np, "reset-gpios", 0, &flags);
	pr_info("%s:%d\n", __func__, gpio);
	if (!gpio_is_valid(gpio))
		return 0;

	ts->reset_active_low = flags & OF_GPIO_ACTIVE_LOW;
	err = devm_gpio_request_one(&client->dev, gpio, ts->reset_active_low ?
			GPIOF_OUT_INIT_HIGH : GPIOF_OUT_INIT_LOW,
			"tsc2004_reset_gpio");
	if (err)
		dev_err(&client->dev, "can't request reset gpio %d", gpio);
	ts->reset_gpio = gpio;
	return err;
}

static int setup_irq_gpio(struct i2c_client *client, struct tsc2004 *ts)
{
	int err = 0;
	int gpio;
	struct device_node *np = client->dev.of_node;

	ts->irq_gpio = -1;
	gpio = of_get_named_gpio(np, "interrupts-extended", 0);
	pr_info("%s:%d\n", __func__, gpio);
	if (!gpio_is_valid(gpio))
		return 0;

	err = devm_gpio_request_one(&client->dev, gpio, GPIOF_DIR_IN,
			"tsc2004_irq");
	if (err) {
		dev_err(&client->dev, "can't request irq gpio %d", gpio);
		return err;
	}
	ts->irq_gpio = gpio;
	return err;
}

static int ts2004_open(struct input_dev *idev)
{
	struct tsc2004 *ts = input_get_drvdata(idev);
	int ret;

	if (!ts)
		return -EIO;

	if (ts->use_count++)
		return 0;

	ret = request_threaded_irq(ts->irq, NULL, tsc2004_irq,
			IRQF_TRIGGER_LOW | IRQF_ONESHOT,
			ts->client->dev.driver->name, ts);
	if (ret < 0) {
		dev_err(&ts->client->dev, "irq %d busy(%d)?\n", ts->irq, ret);
		ts->use_count--;
		return ret;
	}
	return 0;
}

static void ts2004_close(struct input_dev *idev)
{
	struct tsc2004 *ts = input_get_drvdata(idev);

	if (ts) {
		if (--ts->use_count == 0) {
			free_irq(ts->irq, ts);
			cancel_delayed_work_sync(&ts->work);
		}
	}
}

static int tsc2004_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	struct tsc2004 *ts;
	struct input_dev *input_dev;
	int err;

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_READ_WORD_DATA))
		return -EIO;

	ts = kzalloc(sizeof(struct tsc2004), GFP_KERNEL);

	input_dev = input_allocate_device();
	if (!ts || !input_dev) {
		err = -ENOMEM;
		goto err_free_mem;
	}

	ts->client = client;
	ts->irq = client->irq;
	ts->input = input_dev;
	INIT_DELAYED_WORK(&ts->work, tsc2004_work);
	input_dev->open = ts2004_open;
	input_dev->close = ts2004_close;

	ts->x_plate_ohms      = 500;
	setup_reset_gpio(client, ts);
	setup_irq_gpio(client, ts);

	snprintf(ts->phys, sizeof(ts->phys),
		 "%s/input0", dev_name(&client->dev));

	input_dev->name = "tsc2004";
	input_dev->phys = ts->phys;
	input_dev->id.bustype = BUS_I2C;

	input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);

	input_set_abs_params(input_dev, ABS_X, 0, MAX_12BIT, 0, 0);
	input_set_abs_params(input_dev, ABS_Y, 0, MAX_12BIT, 0, 0);
	input_set_abs_params(input_dev, ABS_PRESSURE, 0, MAX_12BIT, 0, 0);

	/* Prepare for touch readings */
	err = tsc2004_prepare_for_reading(ts);
	if (err < 0)
		goto err_free_mem;

	input_set_drvdata(input_dev, ts);
	err = input_register_device(input_dev);
	if (err)
		goto err_free_mem;

	i2c_set_clientdata(client, ts);

	return 0;

 err_free_mem:
	input_free_device(input_dev);
	kfree(ts);
	return err;
}

static int tsc2004_remove(struct i2c_client *client)
{
	struct tsc2004	*ts = i2c_get_clientdata(client);

	input_unregister_device(ts->input);
	if (gpio_is_valid(ts->reset_gpio))
		gpio_set_value(ts->reset_gpio, ts->reset_active_low ? 0 : 1);
	kfree(ts);
	return 0;
}

static struct i2c_device_id tsc2004_idtable[] = {
	{ "tsc2004", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, tsc2004_idtable);

static const struct of_device_id tsc2004_dt_ids[] = {
       {
               .compatible = "tsc2004,tsc2004-touch",
       }, {
               /* sentinel */
       }
};
MODULE_DEVICE_TABLE(of, tsc2004_dt_ids);

static struct i2c_driver tsc2004_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "tsc2004",
		.of_match_table = tsc2004_dt_ids,
	},
	.id_table	= tsc2004_idtable,
	.probe		= tsc2004_probe,
	.remove		= tsc2004_remove,
};

static int __init tsc2004_init(void)
{
	return i2c_add_driver(&tsc2004_driver);
}

static void __exit tsc2004_exit(void)
{
	i2c_del_driver(&tsc2004_driver);
}

module_init(tsc2004_init);
module_exit(tsc2004_exit);

MODULE_AUTHOR("Vaibhav Hiremath <hvaibhav@ti.com>");
MODULE_DESCRIPTION("TSC2004 TouchScreen Driver");
MODULE_LICENSE("GPL");
