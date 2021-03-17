/**
 * Microchip KSZ8863 I2C driver
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

#if 0
#define DEBUG
#define DBG
#endif

#ifndef CONFIG_KSZ_SWITCH_EMBEDDED
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/phy.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/cache.h>
#include <linux/crc32.h>
#include <linux/if_vlan.h>
#include <linux/ip.h>
#include <net/ip.h>
#endif

/* -------------------------------------------------------------------------- */

#include <linux/i2c.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

#include "ksz_cfg_8863.h"
#ifndef PHY_RESET_NOT
#define PHY_RESET_NOT			PHY_RESET
#endif


#define SW_DRV_RELDATE			"Jan 30, 2020"
#define SW_DRV_VERSION			"1.2.2"

/* -------------------------------------------------------------------------- */

#define HW_R(ks, reg)		i2c_rdreg8(ks, reg)
#define HW_W(ks, reg, val)	i2c_wrreg8(ks, reg, val)
#define HW_R8(ks, reg)		i2c_rdreg8(ks, reg)
#define HW_W8(ks, reg, val)	i2c_wrreg8(ks, reg, val)
#define HW_R16(ks, reg)		i2c_rdreg16(ks, reg)
#define HW_W16(ks, reg, val)	i2c_wrreg16(ks, reg, val)
#define HW_R32(ks, reg)		i2c_rdreg32(ks, reg)
#define HW_W32(ks, reg, val)	i2c_wrreg32(ks, reg, val)

#include "ksz_spi_net.h"

/* -------------------------------------------------------------------------- */

#define I2C_CMD_LEN			1

/*
 * I2C register read/write calls.
 *
 * All these calls issue I2C transactions to access the chip's registers. They
 * all require that the necessary lock is held to prevent accesses when the
 * chip is busy transfering packet data (RX/TX FIFO accesses).
 */

/**
 * i2c_wrreg - issue write register command
 * @priv:	The switch device structure.
 * @reg:	The register address.
 * @buf:	The data buffer to write.
 * @txl:	The length of data.
 *
 * This is the low level write call that issues the necessary i2c message(s)
 * to write data to the register specified in @reg.
 */
static void i2c_wrreg(struct sw_priv *priv, u8 reg, void *buf, size_t txl)
{
	struct i2c_hw_priv *hw_priv = priv->hw_dev;
	struct i2c_msg msg;
	struct i2c_client *i2c = hw_priv->i2cdev;
	struct i2c_adapter *adapter = i2c->adapter;

	hw_priv->txd[0] = reg;

	/* Own transmit buffer is not being used. */
	if (buf != hw_priv->txd)
		memcpy(&hw_priv->txd[I2C_CMD_LEN], buf, txl);

	msg.addr = i2c->addr;
	msg.flags = 0;
	msg.len = txl + I2C_CMD_LEN;
	msg.buf = hw_priv->txd;

	if (i2c_transfer(adapter, &msg, 1) != 1)
		pr_alert("i2c_transfer() failed\n");
}

static void i2c_wrreg_size(struct sw_priv *priv, u8 reg, unsigned val,
			   size_t size)
{
	struct i2c_hw_priv *hw_priv = priv->hw_dev;
	u8 *txb = hw_priv->txd;
	int i = I2C_CMD_LEN;
	size_t cnt = size;

	do {
		txb[i++] = (u8)(val >> (8 * (cnt - 1)));
		cnt--;
	} while (cnt);
	i2c_wrreg(priv, reg, txb, size);
}

/**
 * i2c_wrreg32 - write 32bit register value to chip
 * @priv:	The switch device structure.
 * @reg:	The register address.
 * @val:	The value to write.
 *
 * Issue a write to put the value @val into the register specified in @reg.
 */
static void i2c_wrreg32(struct sw_priv *priv, u8 reg, unsigned val)
{
	i2c_wrreg_size(priv, reg, val, 4);
}

/**
 * i2c_wrreg16 - write 16bit register value to chip
 * @priv:	The switch device structure.
 * @reg:	The register address.
 * @val:	The value to write.
 *
 * Issue a write to put the value @val into the register specified in @reg.
 */
static void i2c_wrreg16(struct sw_priv *priv, u8 reg, unsigned val)
{
	i2c_wrreg_size(priv, reg, val, 2);
}

/**
 * i2c_wrreg8 - write 8bit register value to chip
 * @priv:	The switch device structure.
 * @reg:	The register address.
 * @val:	The value to write.
 *
 * Issue a write to put the value @val into the register specified in @reg.
 */
static void i2c_wrreg8(struct sw_priv *priv, u8 reg, unsigned val)
{
	i2c_wrreg_size(priv, reg, val, 1);
}

/**
 * i2c_rdreg - issue read register command and return the data
 * @priv:	The switch device structure.
 * @reg:	The register address.
 * @rxb:	The RX buffer to return the result into.
 * @rxl:	The length of data expected.
 *
 * This is the low level read call that issues the necessary i2c message(s)
 * to read data from the register specified in @reg.
 */
static void i2c_rdreg(struct sw_priv *priv, u8 reg, void *rxb, unsigned rxl)
{
	struct i2c_hw_priv *hw_priv = priv->hw_dev;
	struct i2c_msg msg[2];
	struct i2c_client *i2c = hw_priv->i2cdev;
	struct i2c_adapter *adapter = i2c->adapter;

	msg[0].addr = i2c->addr;
	msg[0].flags = 0;
	msg[0].len = I2C_CMD_LEN;
	msg[0].buf = &reg;

	msg[1].addr = i2c->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = rxl;
	msg[1].buf = rxb;

	if (i2c_transfer(adapter, msg, 2) != 2)
		pr_alert("i2c_transfer() failed\n");
}

/**
 * i2c_rdreg8 - read 8 bit register from device
 * @priv:	The switch device structure.
 * @reg:	The register address.
 *
 * Read a 8bit register from the chip, returning the result.
 */
static u8 i2c_rdreg8(struct sw_priv *priv, u8 reg)
{
	u8 rxb[1];

	i2c_rdreg(priv, reg, rxb, 1);
	return rxb[0];
}

/**
 * i2c_rdreg16 - read 16 bit register from device
 * @priv:	The switch device structure.
 * @reg:	The register address.
 *
 * Read a 16bit register from the chip, returning the result.
 */
static u16 i2c_rdreg16(struct sw_priv *priv, u8 reg)
{
	__le16 rx = 0;

	i2c_rdreg(priv, reg, &rx, 2);
	return be16_to_cpu(rx);
}

/**
 * i2c_rdreg32 - read 32 bit register from device
 * @priv:	The switch device structure.
 * @reg:	The register address.
 *
 * Read a 32bit register from the chip.
 *
 * Note, this read requires the address be aligned to 4 bytes.
 */
static u32 i2c_rdreg32(struct sw_priv *priv, u8 reg)
{
	__le32 rx = 0;

	i2c_rdreg(priv, reg, &rx, 4);
	return be32_to_cpu(rx);
}

/* -------------------------------------------------------------------------- */

#define USE_SHOW_HELP
#include "ksz_common.c"

#include "ksz_req.c"

/* -------------------------------------------------------------------------- */

#define MIB_READ_INTERVAL		(HZ / 2)

static u8 sw_r8(struct ksz_sw *sw, unsigned reg)
{
	return HW_R8(sw->dev, reg);
}

static u16 sw_r16(struct ksz_sw *sw, unsigned reg)
{
	return HW_R16(sw->dev, reg);
}

static u32 sw_r32(struct ksz_sw *sw, unsigned reg)
{
	return HW_R32(sw->dev, reg);
}

static void sw_w8(struct ksz_sw *sw, unsigned reg, unsigned val)
{
	HW_W8(sw->dev, reg, val);
}

static void sw_w16(struct ksz_sw *sw, unsigned reg, unsigned val)
{
	HW_W16(sw->dev, reg, val);
}

static void sw_w32(struct ksz_sw *sw, unsigned reg, unsigned val)
{
	HW_W32(sw->dev, reg, val);
}

#include "ksz_sw.c"

static struct ksz_sw_reg_ops sw_reg_ops = {
	.r8			= sw_r8,
	.r16			= sw_r16,
	.r32			= sw_r32,
	.w8			= sw_w8,
	.w16			= sw_w16,
	.w32			= sw_w32,

	.r			= sw_r8,
	.w			= sw_w8,

	.get			= sw_reg_get,
	.set			= sw_reg_set,
};

/* -------------------------------------------------------------------------- */

static int ksz8863_probe(struct i2c_client *i2c,
	const struct i2c_device_id *i2c_id)
{
	struct i2c_hw_priv *hw_priv;
	struct sw_priv *priv;

	priv = kzalloc(sizeof(struct sw_priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->hw_dev = kzalloc(sizeof(struct i2c_hw_priv), GFP_KERNEL);
	if (!priv->hw_dev) {
		kfree(priv);
		return -ENOMEM;
	}
	hw_priv = priv->hw_dev;

	hw_priv->i2cdev = i2c;

	priv->dev = &i2c->dev;

	priv->sw.reg = &sw_reg_ops;

	priv->irq = i2c->irq;

	return ksz_probe(priv);
}

static int ksz8863_remove(struct i2c_client *i2c)
{
	struct sw_priv *priv = dev_get_drvdata(&i2c->dev);

	return ksz_remove(priv);
}

#define I2C_SWITCH_NAME			"ksz8863"

#ifndef CONFIG_OF
/* Please change the I2C address if necessary. */
#define I2C_SWITCH_ADDR			0x5F

/* Please provide a system interrupt number here. */
#define I2C_SWITCH_INTR			-1

static int ksz8863_detect(struct i2c_client *i2c, struct i2c_board_info *info)
{
	strncpy(info->type, I2C_SWITCH_NAME, I2C_NAME_SIZE);
	info->irq = I2C_SWITCH_INTR;
	return 0;
}

static unsigned short i2c_address_list[] = {
	I2C_SWITCH_ADDR,

	I2C_CLIENT_END
};
#endif

static const struct i2c_device_id i2c_id[] = {
	{ I2C_SWITCH_NAME, 0 },
	{ },
};

#ifdef CONFIG_OF
static const struct of_device_id ksz8863_dt_ids[] = {
	{ .compatible = "microchip,ksz8863" },
	{ .compatible = "microchip,ksz8873" },
	{},
};
MODULE_DEVICE_TABLE(of, ksz8863_dt_ids);
#endif

static struct i2c_driver ksz8863_driver = {
	.driver = {
		.name	= I2C_SWITCH_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(ksz8863_dt_ids),
#endif
	},
	.probe		= ksz8863_probe,
	.remove		= ksz8863_remove,
	.id_table	= i2c_id,

#ifndef CONFIG_OF
	/* Big enough to be accepted in all cases. */
	.class		= 0xffff,
	.detect		= ksz8863_detect,
	.address_list	= i2c_address_list,
#endif
};

static int __init ksz8863_init(void)
{
	int ret;

	ret = i2c_add_driver(&ksz8863_driver);
#ifndef CONFIG_OF
	if (ret)
		return ret;

	/* Probe not called. */
	if (!sw_device_present) {
		struct i2c_adapter *adap;

		/* Assume I2C bus starts at 0. */
		adap = i2c_get_adapter(0);

		/* I2C master may not be created yet. */
		if (!adap) {
#if !defined(CONFIG_I2C_KSZ8863_MODULE)
			struct i2c_board_info info = {
				.type	= I2C_SWITCH_NAME,
				.addr	= I2C_SWITCH_ADDR,
				.irq	= I2C_SWITCH_INTR,
			};

			ret = i2c_register_board_info(0, &info, 1);
#else
			return -ENODEV;
#endif
		} else
			i2c_put_adapter(adap);
	}
#endif
	return ret;
}

static void __exit ksz8863_exit(void)
{
	i2c_del_driver(&ksz8863_driver);
}

#ifndef CONFIG_KSZ_SWITCH_EMBEDDED
subsys_initcall(ksz8863_init);
module_exit(ksz8863_exit);
#endif

#ifndef CONFIG_KSZ_SWITCH_EMBEDDED
MODULE_DESCRIPTION("KSZ8863 I2C switch driver");
MODULE_AUTHOR("Tristram Ha <Tristram.Ha@microchip.com>");
MODULE_LICENSE("GPL");

MODULE_ALIAS("i2c:ksz8863");
#endif
