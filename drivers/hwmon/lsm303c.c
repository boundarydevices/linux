/*
 * ST Microelectronics LSM303DLH compass driver
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

struct lsm303_compass {
	struct i2c_client	*client;
	struct input_polled_dev	*idev;
};

static char const identbytes[] = {
	'H','4','3'
};
static unsigned const num_identbytes = ARRAY_SIZE(identbytes);

#define CRA_REG_M	0x00
#define CRB_REG_M	0x01
#define MR_REG_M	0x02
#define OUT_X_M		0x03
#define OUT_Y_M		0x05
#define OUT_Z_M		0x07
#define SR_REG_M	0x09
#define IDENTOFFS 	0x0a

#define DRVNAME		"lsm303c"
#define DRVDESC		DRVNAME ": Compass"

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

/*
 * Android wants X, Y, and Z in micro-Tesla.
 *
 * Input readings are in the range 0xF800-0x07FF (-2048-2047)
 * and represent some fraction of a gauss depending on the
 * gain setting in CRB_REG_M, bits 5-7.
 */
#define MIN_VALUE -2048
#define MAX_VALUE 2047

/*
 * The factory default seems to be 001, which means that each
 * LSB is 1/1055G for X and Y, and 1/950G for Z, which means
 * our range is roughly +- 2G.
 *
 * A micro-Tesla is 100G, yielding the following translations:
 */
#define X_TO_UTESLA(x) ((100*x)/1055)
#define Y_TO_UTESLA(y) ((100*y)/1055)
#define Z_TO_UTESLA(z) ((100*z)/950)

static void compass_poll(struct input_polled_dev *idev)
{
	struct lsm303_compass *compass = idev ? idev->private : 0 ;
	if (compass) {
		char regs[6];
		int retval = read_reg(compass->client,SR_REG_M);
		if (retval & 1) { /* conversion ready */
			int i ;
			int err = 0;
			for (i=0; i <sizeof(regs);i++) {
				int retval = read_reg(compass->client,OUT_X_M+i);
				if (0 <= retval) {
					regs[i] = (char)retval ;
				} else {
					printk (KERN_ERR "%s: error %d reading byte %u\n", __func__, retval, i);
					err += 1<<i ;
				}
			}
			if (!err) {
				s16 readings[3];
				for (i = 0 ; i < ARRAY_SIZE(readings); i++) {
					readings[i] = regs[i*2+1]+(regs[i*2]<<8);
				}
				readings[0] = X_TO_UTESLA(readings[0]);
				readings[1] = Y_TO_UTESLA(readings[1]);
				readings[2] = Z_TO_UTESLA(readings[2]);

				input_report_abs(idev->input, ABS_X, readings[0]);
				input_report_abs(idev->input, ABS_Y, readings[1]);
				input_report_abs(idev->input, ABS_Z, readings[2]);
				input_sync(idev->input);
#if 0
				printk (KERN_ERR "%s: %6d %6d %6d\n", __func__,
					readings[0],readings[1],readings[2]);
#endif
			}
		} else {
			printk (KERN_ERR "%s: not ready: %d\n", __func__, retval );
			if (retval & 2) {
				printk (KERN_ERR "%s: locked... try clearing\n", __func__ );
				retval = i2c_smbus_write_byte_data(compass->client,MR_REG_M,3); /* stop */
				if (retval) {
					printk (KERN_ERR "%s: stop err %d\n", __func__, retval );
				}
				retval = i2c_smbus_write_byte_data(compass->client,MR_REG_M,0); /* continuous conversion */
				if (retval) {
					printk (KERN_ERR "%s: start err %d\n", __func__, retval );
				}
			}
		}
	} else
		printk (KERN_ERR "%s: no compass\n", __func__ );
}

/*-----------------------------------------------------------------------*/
/* device probe and removal */

static int
lsm303_compass_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = client->adapter;
	int i, retval ;
	char idbytes[13];

	memset(idbytes,0,sizeof(idbytes));

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		printk (KERN_ERR "%s: can't read/write bytes\n", __func__ );
		return -ENODEV;
	}

	for (i=0; i <sizeof(idbytes);i++) {
		retval = read_reg(client,i);
		if (0 <= retval) {
			idbytes[i] = (char)retval ;
		} else {
			printk (KERN_ERR "%s: error %d reading byte %u\n", __func__, retval, i);
			break;
		}
	}
#if 0
	if (0 <= retval) {
		printk (KERN_ERR "%s: read %u bytes\n", __func__, i);
		for (i = 0; i <= sizeof(idbytes); i++) {
			printk (KERN_ERR "%s[%u] == 0x%x\n", __func__,i,idbytes[i]);
		}
	}
#endif
	if (0 <= retval) {
		if (0 == memcmp(idbytes+IDENTOFFS,identbytes,sizeof(identbytes))) {
			struct lsm303_compass *compass = kzalloc(sizeof(struct lsm303_compass),GFP_KERNEL);
			compass->client = client;
			i2c_set_clientdata(client, compass);
			compass->idev = input_allocate_polled_device();
			if (compass->idev) {
				struct input_dev *input_dev;
				compass->idev->private = compass ;
				compass->idev->poll = compass_poll;
				compass->idev->poll_interval = 67; /* power-on defaults sample at 15Hz */
				compass->idev->poll_interval_min = 0;
				compass->idev->poll_interval_max = 2000;
				input_dev = compass->idev->input;

				input_dev->name       = DRVDESC;
				input_dev->id.bustype = BUS_HOST;
				input_dev->id.vendor  = 0;
				input_dev->dev.parent = &client->dev;

				set_bit(EV_ABS, input_dev->evbit);
				input_set_abs_params(input_dev, ABS_X, X_TO_UTESLA(MIN_VALUE), X_TO_UTESLA(MAX_VALUE), 0, 0);
				input_set_abs_params(input_dev, ABS_Y, Y_TO_UTESLA(MIN_VALUE), Y_TO_UTESLA(MAX_VALUE), 0, 0);
				input_set_abs_params(input_dev, ABS_Z, Z_TO_UTESLA(MIN_VALUE), Z_TO_UTESLA(MAX_VALUE), 0, 0);

				retval = input_register_polled_device(compass->idev);
				if (0 == retval) {
                                        retval = i2c_smbus_write_byte_data(client,CRB_REG_M,0x20); /* smallest range */
					if (retval) {
                                                printk (KERN_ERR "%s: error %d setting range\n", __func__,retval);
					}

                                        retval = i2c_smbus_write_byte_data(client,MR_REG_M,0); /* continuous conversion */
					printk (KERN_INFO "%s: LSM303DLH compass driver loaded\n", __func__ );
				} else {
					printk (KERN_ERR "%s: error %d registering polled device\n", __func__, retval );
					input_free_polled_device(compass->idev);
					compass->idev = NULL;
				}
			}
			else {
				printk (KERN_ERR "%s: error allocating polldev\n", __func__ );
				retval = -EINVAL ;
			}

			if (0 != retval) {
				kfree(compass);
			}
		} else {
			printk (KERN_ERR "%s: not an LSM303: %02x %02x %02x\n",
				__func__, idbytes[IDENTOFFS], idbytes[IDENTOFFS+1], idbytes[IDENTOFFS+2] );
			retval = -EINVAL ;
		}
	}
        return retval ;
}

static int lsm303_compass_remove(struct i2c_client *client)
{
	struct lsm303_compass *compass = i2c_get_clientdata(client);

	if (compass && compass->idev) {
                input_unregister_polled_device(compass->idev);
		input_free_polled_device(compass->idev);
	}
	return 0;
}

static const struct i2c_device_id lsm303_compass_ids[] = {
	{ DRVNAME, 0 },
	{ /* LIST END */ }
};
MODULE_DEVICE_TABLE(i2c, lsm303_compass_ids);

/* Return 0 if detection is successful, -ENODEV otherwise */
static int lsm303_compass_detect(struct i2c_client *client,
			struct i2c_board_info *info)
{
	printk (KERN_ERR "%s\n", __func__ );
	return 0;
}

static const unsigned short i2c_addrs[] = { 0x3c>>1, I2C_CLIENT_END };

static struct i2c_driver lsm303_compass_driver = {
	.class		= I2C_CLASS_HWMON,
	.driver = {
		.name	= DRVNAME,
	},
	.probe		= lsm303_compass_probe,
	.remove		= lsm303_compass_remove,
	.id_table	= lsm303_compass_ids,
	.detect		= lsm303_compass_detect,
	.address_list	= i2c_addrs,
};

/* module glue */

static int __init sensors_lsm303_compass_init(void)
{
	return i2c_add_driver(&lsm303_compass_driver);
}

static void __exit sensors_lsm303_compass_exit(void)
{
	i2c_del_driver(&lsm303_compass_driver);
}

MODULE_AUTHOR("Eric Nelson <eric.nelson@boundarydevices.com>");
MODULE_DESCRIPTION("LSM303 Compass driver");
MODULE_LICENSE("GPL");

module_init(sensors_lsm303_compass_init);
module_exit(sensors_lsm303_compass_exit);
