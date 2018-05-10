/*
 * drivers/amlogic/media/dtv_demod/i2c_func.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/dvb/aml_demod.h>
#include "demod_func.h"

int am_demod_i2c_xfer(struct aml_demod_i2c *adap, struct i2c_msg msgs[],
		      int num)
{
	int ret = 0;

	if (adap->scl_oe) {
		/*      ret = aml_i2c_sw_bit_xfer(adap, msgs, num);*/
	} else {
		if (adap->i2c_priv)
			ret = i2c_transfer((struct i2c_adapter *)adap->i2c_priv,
					   msgs, num);
		else
			;
		/*	printk("i2c error, no valid i2c\n");*/
	}
	return ret;
}
