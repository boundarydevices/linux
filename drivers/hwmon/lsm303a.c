/*
 * ST Microelectronics LSM303DLH accelerometer driver
 *
 * Copyright (C) 2011, Boundary Devices (www.boundarydevices.com).
 *
 * Eric Nelson <eric.nelson@boundarydevices.com>
 *
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/input-polldev.h>

struct lsm303_accelerometer {
	struct i2c_client	*client;
	struct input_polled_dev	*idev;
};

#define DRVNAME		"lsm303a"
#define DRVDESC		DRVNAME ": Accelerometer"

#define CTRL_REG1_A		0x20
#define CTRL_REG2_A		0x21
#define CTRL_REG3_A		0x22
#define CTRL_REG4_A		0x23
#define CTRL_REG5_A		0x24
#define HP_FILTER_RESET_A	0x25
#define REFERENCE_A		0x26
#define STATUS_REG_A		0x27
#define OUT_X_L_A		0x28
#define OUT_X_H_A		0x29
#define OUT_Y_L_A		0x2A
#define OUT_Y_H_A		0x2B
#define OUT_Z_L_A		0x2C
#define OUT_Z_H_A		0x2D
#define INT1_CFG_A		0x30
#define INT1_SOURCE_A		0x31
#define INT1_THS_A		0x32
#define INT1_DURATION_A		0x33
#define INT2_CFG_A		0x34
#define INT2_SOURCE_A		0x35
#define INT2_THS_A		0x36
#define INT2_DURATION_A		0x37

/* returns 0..0xff for success, negative for failure */
static s32 read_reg(struct i2c_client *client, u8 address) {
	s32 i, rval = -EIO ;
	for (i = 0; (i < 5) && (0 > rval); i++) {
                rval = i2c_smbus_read_byte_data(client,address);
		if (0 >= rval) {
			msleep(10);
		}
	}
	return rval ;
}

static void accel_poll(struct input_polled_dev *idev)
{
	struct lsm303_accelerometer *accel = idev ? idev->private : 0 ;
	if (accel) {
		int const status_reg = read_reg(accel->client,STATUS_REG_A);
		if (0 <= status_reg) {
			int captured = 0 ;
			int i, mask ;
			for (i=0, mask=1 ; i < 3; i++, mask <<= 1) {
				if (status_reg & mask) {
					unsigned reg = OUT_X_L_A+(2*i);
					int retval = read_reg(accel->client,reg);
					if (0 <= retval) {
						s16 val = retval ;
						retval = read_reg(accel->client,++reg);
						if (0 <= retval) {
							val |= retval << 8 ;
							input_report_abs(idev->input, ABS_X+i, val);
							captured = 1 ;
//							printk (KERN_ERR "%c:%04x\n", 'X'+i, val);
							continue;
						}
					}
					/* continue from middle */
					printk (KERN_ERR "%s: error reading reg %d\n", __func__, reg);
					break;
				} // sample present
			}
			if (captured)
				input_sync(idev->input);
		} else
			printk (KERN_ERR "%s: error reading STATUS_REG_A\n", __func__ );
	}
}

/*-----------------------------------------------------------------------*/
/* device probe and removal */
#define MIN_VALUE -2048
#define MAX_VALUE 2047

static int
lsm303_accel_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int retval ;
	struct lsm303_accelerometer *accel = kzalloc(sizeof(struct lsm303_accelerometer),GFP_KERNEL);
	accel->client = client;
	i2c_set_clientdata(client, accel);
	accel->idev = input_allocate_polled_device();
	if (accel->idev) {
		struct input_dev *input_dev;
		accel->idev->private = accel ;
		accel->idev->poll = accel_poll;
		accel->idev->poll_interval = 10; /* power-on defaults sample at 50Hz */
		accel->idev->poll_interval_min = 0;
		accel->idev->poll_interval_max = 2000;
		input_dev = accel->idev->input;

		input_dev->name       = DRVDESC;
		input_dev->id.bustype = BUS_HOST;
		input_dev->id.vendor  = 0;
		input_dev->dev.parent = &client->dev;

		set_bit(EV_ABS, input_dev->evbit);
		input_set_abs_params(input_dev, ABS_X, MIN_VALUE, MAX_VALUE, 0, 0);
		input_set_abs_params(input_dev, ABS_Y, MIN_VALUE, MAX_VALUE, 0, 0);
		input_set_abs_params(input_dev, ABS_Z, MIN_VALUE, MAX_VALUE, 0, 0);

		retval = input_register_polled_device(accel->idev);
		if (0 == retval) {
			retval = i2c_smbus_write_byte_data(client,CTRL_REG1_A,0x27); /* X/Y/Z enable at 10Hz */
			if (0 == retval) {
				printk (KERN_INFO "%s: LSM303DLH accelerometer driver loaded\n", __func__ );
#if 2 == CONFIG_SENSORS_LSM303_ACCELEROMETER_RANGE
				/* factory default */
				retval = i2c_smbus_write_byte_data(client,CTRL_REG4_A,0); /* 2G full-scale */
				if (0 != retval)
					printk (KERN_ERR "%s: error %d setting full-scale range to 8G\n",
						__func__, retval);
#elif 4 == CONFIG_SENSORS_LSM303_ACCELEROMETER_RANGE
				retval = i2c_smbus_write_byte_data(client,CTRL_REG4_A,0x10); /* 8G full-scale */
				if (0 != retval)
					printk (KERN_ERR "%s: error %d setting full-scale range to 8G\n",
						__func__, retval);
#elif 8 == CONFIG_SENSORS_LSM303_ACCELEROMETER_RANGE
				retval = i2c_smbus_write_byte_data(client,CTRL_REG4_A,0x30); /* 8G full-scale */
				if (0 != retval)
					printk (KERN_ERR "%s: error %d setting full-scale range to 8G\n",
						__func__, retval);
#else
	#error only 2,4,8G full-scale ranges are supported by LSM303
#endif
			} else {
				printk (KERN_ERR "%s: error %d enabling accelerometer\n", __func__, retval );
			}
			if (0 != retval)
				input_unregister_polled_device(accel->idev);
		} else {
			printk (KERN_ERR "%s: error %d registering polled device\n", __func__, retval );
			input_free_polled_device(accel->idev);
			accel->idev = NULL;
		}
	}
	else {
		printk (KERN_ERR "%s: error allocating polldev\n", __func__ );
		retval = -EINVAL ;
	}

	if (0 != retval) {
		kfree(accel);
	}
        return retval ;
}

static int lsm303_accel_remove(struct i2c_client *client)
{
	struct lsm303_accelerometer *accel = i2c_get_clientdata(client);

	printk (KERN_ERR "%s: %p\n", __func__, accel );
	if (accel && accel->idev) {
                input_unregister_polled_device(accel->idev);
		input_free_polled_device(accel->idev);
	}
	return 0;
}

static const struct i2c_device_id lsm303_accel_ids[] = {
	{ DRVNAME, 0 },
	{ /* LIST END */ }
};
MODULE_DEVICE_TABLE(i2c, lsm303_accel_ids);

/* Return 0 if detection is successful, -ENODEV otherwise */
static int lsm303_accel_detect(struct i2c_client *client,
			struct i2c_board_info *info)
{
	printk (KERN_ERR "%s\n", __func__ );
	return 0;
}

static const unsigned short i2c_addrs[] = { 0x3c>>1, I2C_CLIENT_END };

static struct i2c_driver lsm303_accel_driver = {
	.class		= I2C_CLASS_HWMON,
	.driver = {
		.name	= DRVNAME,
	},
	.probe		= lsm303_accel_probe,
	.remove		= lsm303_accel_remove,
	.id_table	= lsm303_accel_ids,
	.detect		= lsm303_accel_detect,
	.address_list	= i2c_addrs,
};

/* module glue */

static int __init sensors_lsm303_accel_init(void)
{
	return i2c_add_driver(&lsm303_accel_driver);
}

static void __exit sensors_lsm303_accel_exit(void)
{
	i2c_del_driver(&lsm303_accel_driver);
}

MODULE_AUTHOR("Eric Nelson <eric.nelson@boundarydevices.com.com>");
MODULE_DESCRIPTION("LSM303 Accelerometer driver");
MODULE_LICENSE("GPL");

module_init(sensors_lsm303_accel_init);
module_exit(sensors_lsm303_accel_exit);
