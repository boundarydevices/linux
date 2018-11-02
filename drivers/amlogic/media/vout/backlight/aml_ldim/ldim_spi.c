/*
 * drivers/amlogic/media/vout/backlight/aml_ldim/ldim_spi.c
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/amlogic/media/vout/lcd/aml_ldim.h>
#include "ldim_drv.h"
#include "ldim_dev_drv.h"

static unsigned int cs_hold_delay;
static unsigned int cs_clk_delay;

int ldim_spi_write(struct spi_device *spi, unsigned char *tbuf, int tlen)
{
	struct spi_transfer xfer;
	struct spi_message msg;
	int ret;

	if (cs_hold_delay)
		udelay(cs_hold_delay);

	spi_message_init(&msg);
	memset(&xfer, 0, sizeof(xfer));
	xfer.tx_buf = (void *)tbuf;
	xfer.rx_buf = NULL;
	xfer.len = tlen;
	spi_message_add_tail(&xfer, &msg);
	ret = spi_sync(spi, &msg);

	return ret;
}

int ldim_spi_read(struct spi_device *spi, unsigned char *tbuf, int tlen,
		unsigned char *rbuf, int rlen)
{
	struct spi_transfer xfer[2];
	struct spi_message msg;
	int ret;

	if (cs_hold_delay)
		udelay(cs_hold_delay);

	spi_message_init(&msg);
	memset(&xfer, 0, sizeof(xfer));
	xfer[0].tx_buf = (void *)tbuf;
	xfer[0].rx_buf = NULL;
	xfer[0].len = tlen;
	spi_message_add_tail(&xfer[0], &msg);
	xfer[1].tx_buf = NULL;
	xfer[1].rx_buf = (void *)rbuf;
	xfer[1].len = rlen;
	spi_message_add_tail(&xfer[1], &msg);
	ret = spi_sync(spi, &msg);

	return ret;
}

static int ldim_spi_dev_probe(struct spi_device *spi)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	int ret;

	if (ldim_debug_print)
		LDIMPR("%s\n", __func__);

	ldim_drv->spi_dev = spi;

	dev_set_drvdata(&spi->dev, ldim_drv->ldev_conf);
	spi->bits_per_word = 8;
	cs_hold_delay = ldim_drv->ldev_conf->cs_hold_delay;
	cs_clk_delay = ldim_drv->ldev_conf->cs_clk_delay;

	ret = spi_setup(spi);
	if (ret)
		LDIMERR("spi setup failed\n");

	return ret;
}

static int ldim_spi_dev_remove(struct spi_device *spi)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();

	if (ldim_debug_print)
		LDIMPR("%s\n", __func__);

	ldim_drv->spi_dev = NULL;
	return 0;
}

static struct spi_driver ldim_spi_dev_driver = {
	.probe = ldim_spi_dev_probe,
	.remove = ldim_spi_dev_remove,
	.driver = {
		.name = "ldim_dev",
		.owner = THIS_MODULE,
	},
};

int ldim_spi_driver_add(struct aml_ldim_driver_s *ldim_drv)
{
	int ret;

	if (ldim_drv->spi_info == NULL) {
		LDIMERR("%s: spi_info is null\n", __func__);
		return -1;
	}

	spi_register_board_info(ldim_drv->spi_info, 1);
	ret = spi_register_driver(&ldim_spi_dev_driver);
	if (ret) {
		LDIMERR("%s failed\n", __func__);
		return -1;
	}
	if (ldim_drv->spi_dev == NULL) {
		LDIMERR("%s failed\n", __func__);
		return -1;
	}

	LDIMPR("%s ok\n", __func__);
	return 0;
}

int ldim_spi_driver_remove(struct aml_ldim_driver_s *ldim_drv)
{
	if (ldim_drv->spi_dev)
		spi_unregister_driver(&ldim_spi_dev_driver);

	LDIMPR("%s ok\n", __func__);

	return 0;
}

