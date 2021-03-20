/**
 * Microchip KSZ8863 mdio driver
 *
 * Copyright (c) 2015-2020 Microchip Technology Inc.
 * Copyright (c) 2010-2015 Micrel, Inc.
 *
 * Copyright 2009 Simtec Electronics
 *	http://www.simtec.co.uk/
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
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
 */
#define DEBUG
#include <linux/cache.h>
#include <linux/crc32.h>
#include <linux/debugfs.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/if_vlan.h>
#include <linux/interrupt.h>
#include <linux/ip.h>
#include <linux/kernel.h>
#include <linux/mdio.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/of.h>
#include <linux/phy.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <net/ip.h>

/* -------------------------------------------------------------------------- */
#include "ksz_cfg_8863.h"
#ifndef PHY_RESET_NOT
#define PHY_RESET_NOT			PHY_RESET
#endif


#define SW_DRV_RELDATE			"Mar 18, 2021"
#define SW_DRV_VERSION			"1.2.2"

/* -------------------------------------------------------------------------- */
#include "ksz_spi_net.h"

/* -------------------------------------------------------------------------- */
#define SMI_READ	BIT(4) | BIT(5)
#define SMI_WRITE	BIT(5)

/*
 *
 * All these calls issue mdio transactions to access the chip's registers. They
 * all require that the necessary lock is held to prevent accesses when the
 * chip is busy transfering packet data (RX/TX FIFO accesses).
 */

/*
 * This is the low level write call that issues the necessary mdio message(s)
 * to write data to the register specified in @reg.
 */
static void mdio_wrreg(struct ksz_sw *sw, unsigned reg, void *buf, size_t txl)
{
	struct sw_priv *priv = sw->dev;
	struct smi_hw_priv *hw_priv = priv->hw_dev;
	struct mdio_device *mdio = hw_priv->mdio;
	int ret, i;

	for (i = 0; i < txl; i++) {
		int tmp = reg + i;
		/* smi interface */
		ret = mdiobus_write(mdio->bus, ((tmp & 0xE0) >> 5) |
				SMI_WRITE, tmp & 0x1f, ((unsigned char *)buf)[i]);
		if (ret < 0)
			break;
	}
	if (ret < 0)
		pr_alert("%s: failed: %x %u\n", __func__, reg, txl);
}

/*
 * Issue a write to put the value @val into the register specified in @reg.
 */
static void mdio_wrreg32(struct ksz_sw *sw, unsigned reg, unsigned val)
{
	__be32 v_be = cpu_to_be32(val);

	mdio_wrreg(sw, reg, &v_be, 4);
}

/*
 * Issue a write to put the value @val into the register specified in @reg.
 */
static void mdio_wrreg16(struct ksz_sw *sw, unsigned reg, unsigned val)
{
	__be16 v_be = cpu_to_be16(val);

	mdio_wrreg(sw, reg, &v_be, 2);
}

/*
 * Issue a write to put the value @val into the register specified in @reg.
 */
static void mdio_wrreg8(struct ksz_sw *sw, unsigned reg, unsigned val)
{
	mdio_wrreg(sw, reg, &val, 1);
}

/*
 * mdio_rdreg - issue read register command and return the data
 * @rxb:	The RX buffer to return the result into.
 * @rxl:	The length of data expected.
 *
 * This is the low level read call that issues the necessary mdio message(s)
 * to read data from the register specified in @reg.
 */
static void mdio_rdreg(struct ksz_sw *sw, unsigned reg, void *rxb, size_t rxl)
{
	struct sw_priv *priv = sw->dev;
	struct smi_hw_priv *hw_priv = priv->hw_dev;
	struct mdio_device *mdio = hw_priv->mdio;
	int ret, i;

	for (i = 0; i < rxl; i++) {
		int tmp = reg + i;
		/* smi interface */
		ret = mdiobus_read(mdio->bus, ((tmp & 0xE0) >> 5) |
				SMI_READ, tmp & 0x1f);
		if (ret < 0)
			break;
		((unsigned char *)rxb)[i] = ret;
	}
	if (ret < 0)
		pr_alert("%s: failed: %x %u\n", __func__, reg, rxl);
}

/**
 * Read a 8bit register from the chip, returning the result.
 */
static u8 mdio_rdreg8(struct ksz_sw *sw, unsigned reg)
{
	u8 buf[4];

	mdio_rdreg(sw, reg, buf, 1);
	return buf[0];
}

/**
 * Read a 16bit register from the chip, returning the result.
 */
static u16 mdio_rdreg16(struct ksz_sw *sw, unsigned reg)
{
	__be16 val;

	mdio_rdreg(sw, reg, &val, 2);
	return be16_to_cpu(val);
}

/*
 * Read a 32bit register from the chip.
 *
 * Note, this read requires the address be aligned to 4 bytes.
 */
static u32 mdio_rdreg32(struct ksz_sw *sw, unsigned reg)
{
	__be32 val;

	mdio_rdreg(sw, reg, &val, 4);
	return be32_to_cpu(val);
}

/* -------------------------------------------------------------------------- */

#define USE_SHOW_HELP
#include "ksz_common.c"

#include "ksz_req.c"

/* -------------------------------------------------------------------------- */

#define MIB_READ_INTERVAL		(HZ / 2)

#include "ksz_sw.c"

static struct ksz_sw_reg_ops sw_reg_ops = {
	.r8			= mdio_rdreg8,
	.r16			= mdio_rdreg16,
	.r32			= mdio_rdreg32,
	.w8			= mdio_wrreg8,
	.w16			= mdio_wrreg16,
	.w32			= mdio_wrreg32,

	.r			= mdio_rdreg8,
	.w			= mdio_wrreg8,

	.get			= sw_reg_get,
	.set			= sw_reg_set,
};

/* -------------------------------------------------------------------------- */

static int ksz8863_probe(struct mdio_device *mdio)
{
	struct device *dev = &mdio->dev;
	struct device_node *np = dev->of_node;
	struct smi_hw_priv *hw_priv;
	struct sw_priv *priv;
	int ret = 0;

	pr_debug("%s:\n", __func__);
	priv = kzalloc(sizeof(struct sw_priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	hw_priv = kzalloc(sizeof(struct smi_hw_priv), GFP_KERNEL);
	if (!hw_priv) {
		ret = -ENOMEM;
		goto out1;
	}
	priv->hw_dev = hw_priv;

	hw_priv->mdio = mdio;

	priv->dev = dev;

	priv->sw.reg = &sw_reg_ops;

	ret = of_irq_get(np, 0);
	if (ret < 0) {
		if (ret == -EPROBE_DEFER)
			goto out2;
		ret = 0;
	}
	priv->irq = ret;
	return ksz_probe(priv);
out2:
	kfree(hw_priv);
out1:
	kfree(priv);
	return ret;
}

static void ksz8863_remove(struct mdio_device *mdio)
{
	struct sw_priv *priv = dev_get_drvdata(&mdio->dev);

	ksz_remove(priv);
}

static const struct of_device_id ksz8863_dt_ids[] = {
	{ .compatible = "microchip,ksz8863" },
	{ .compatible = "microchip,ksz8873" },
	{},
};
MODULE_DEVICE_TABLE(of, ksz8863_dt_ids);

static struct mdio_driver ksz8863_driver = {
	.probe = ksz8863_probe,
	.remove = ksz8863_remove,
	.mdiodrv.driver = {
		.name = "ksz8863-smi",
		.of_match_table = of_match_ptr(ksz8863_dt_ids),
	},
};

#if 1
static int __init ksz8863_init(void)
{
	return mdio_driver_register(&ksz8863_driver);
}

static void __exit ksz8863_exit(void)
{
	mdio_driver_unregister(&ksz8863_driver);
}

/* This has to be ready when FEC is probed */
subsys_initcall(ksz8863_init);
module_exit(ksz8863_exit);
#else
mdio_module_driver(ksz8863_driver);
#endif

MODULE_DESCRIPTION("KSZ8863 SMI switch driver");
MODULE_LICENSE("GPL");
