/*
 *  reserve.c - DVI output chip
 *
 *  Copyright (C) 2011 Freescale Semiconductor, Inc. All Rights reserve.
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
#include <linux/gpio.h>
#include <linux/kthread.h>

struct reserve_priv
{
	struct i2c_client	*client;
};

/*
 * I2C init/probing/exit functions
 */
static int reserve_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct reserve_priv *rsv;

	rsv = devm_kzalloc(&client->dev, sizeof(struct reserve_priv),
				GFP_KERNEL);
	if (!rsv)
		return -ENOMEM;

	rsv->client = client;
	return 0;
}

static int reserve_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id reserve_id[] = {
	{"reserve", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, reserve_id);

static struct i2c_driver reserve_driver = {
	.driver = {
		   .name = "reserve",
		   },
	.probe = reserve_probe,
	.remove = reserve_remove,
	.id_table = reserve_id,
};

module_i2c_driver(reserve_driver);

MODULE_AUTHOR("Boundary Devices, Inc.");
MODULE_DESCRIPTION("reserve driver");
MODULE_LICENSE("GPL");
