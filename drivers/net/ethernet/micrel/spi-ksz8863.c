/**
 * Microchip KSZ8863 SPI driver
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
#define DEBUG_MSG
#endif

#ifndef CONFIG_KSZ_SWITCH_EMBEDDED
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/of.h>
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

#include <linux/spi/spi.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

#include "ksz_cfg_8863.h"
#ifndef PHY_RESET_NOT
#define PHY_RESET_NOT			PHY_RESET
#endif

#define KS8863MLI_DEV0			"ksz8863"
#define KS8863MLI_DEV2			"ksz8863_2"

#define SW_DRV_RELDATE			"Jan 30, 2020"
#define SW_DRV_VERSION			"1.2.2"

/* -------------------------------------------------------------------------- */

#define HW_R(ks, reg)		spi_rdreg8(ks, reg)
#define HW_W(ks, reg, val)	spi_wrreg8(ks, reg, val)
#define HW_R8(ks, reg)		spi_rdreg8(ks, reg)
#define HW_W8(ks, reg, val)	spi_wrreg8(ks, reg, val)
#define HW_R16(ks, reg)		spi_rdreg16(ks, reg)
#define HW_W16(ks, reg, val)	spi_wrreg16(ks, reg, val)
#define HW_R32(ks, reg)		spi_rdreg32(ks, reg)
#define HW_W32(ks, reg, val)	spi_wrreg32(ks, reg, val)

#include "ksz_spi_net.h"

/* -------------------------------------------------------------------------- */

/* SPI frame opcodes */
#define KS_SPIOP_RD			3
#define KS_SPIOP_WR			2

#define SPI_CMD_LEN			2

/*
 * SPI register read/write calls.
 *
 * All these calls issue SPI transactions to access the chip's registers. They
 * all require that the necessary lock is held to prevent accesses when the
 * chip is busy transfering packet data (RX/TX FIFO accesses).
 */

/**
 * spi_wrreg - issue write register command
 * @priv:	The switch device structure.
 * @reg:	The register address.
 * @buf:	The data buffer to write.
 * @txl:	The length of data.
 *
 * This is the low level write call that issues the necessary spi message(s)
 * to write data to the register specified in @reg.
 */
static void spi_wrreg(struct sw_priv *priv, u8 reg, void *buf, size_t txl)
{
	struct spi_hw_priv *hw_priv = priv->hw_dev;
	struct spi_device *spi = hw_priv->spidev;
	u8 *txb = (u8 *) hw_priv->txd;
	struct spi_transfer *xfer;
	struct spi_message *msg;
	int ret;

#ifdef DEBUG
	if (!mutex_is_locked(&priv->lock))
		pr_alert("W not locked: %02X\n", reg);
#endif
	txb[0] = KS_SPIOP_WR;
	txb[1] = reg;

	/* Own transmit buffer is being used. */
	if (buf == hw_priv->txd) {
		xfer = &hw_priv->spi_xfer1;
		msg = &hw_priv->spi_msg1;

		xfer->tx_buf = hw_priv->txd;
		xfer->rx_buf = NULL;
		xfer->len = txl + SPI_CMD_LEN;
	} else {
		xfer = hw_priv->spi_xfer2;
		msg = &hw_priv->spi_msg2;

		xfer->tx_buf = hw_priv->txd;
		xfer->rx_buf = NULL;
		xfer->len = SPI_CMD_LEN;

		xfer++;
		xfer->tx_buf = buf;
		xfer->rx_buf = NULL;
		xfer->len = txl;
	}

	ret = spi_sync(spi, msg);
	if (ret < 0)
		pr_alert("spi_sync() failed: %x %u\n", reg, txl);
}

static void spi_wrreg_size(struct sw_priv *priv, u8 reg, unsigned val,
			   size_t size)
{
	struct spi_hw_priv *hw_priv = priv->hw_dev;
	u8 *txb = hw_priv->txd;
	int i = SPI_CMD_LEN;
	size_t cnt = size;

	do {
		txb[i++] = (u8)(val >> (8 * (cnt - 1)));
		cnt--;
	} while (cnt);
	spi_wrreg(priv, reg, txb, size);
}

/**
 * spi_wrreg32 - write 32bit register value to chip
 * @priv:	The switch device structure.
 * @reg:	The register address.
 * @val:	The value to write.
 *
 * Issue a write to put the value @val into the register specified in @reg.
 */
static void spi_wrreg32(struct sw_priv *priv, u8 reg, unsigned val)
{
	spi_wrreg_size(priv, reg, val, 4);
}

/**
 * spi_wrreg16 - write 16bit register value to chip
 * @priv:	The switch device structure.
 * @reg:	The register address.
 * @val:	The value to write.
 *
 * Issue a write to put the value @val into the register specified in @reg.
 */
static void spi_wrreg16(struct sw_priv *priv, u8 reg, unsigned val)
{
	spi_wrreg_size(priv, reg, val, 2);
}

/**
 * spi_wrreg8 - write 8bit register value to chip
 * @priv:	The switch device structure.
 * @reg:	The register address.
 * @val:	The value to write.
 *
 * Issue a write to put the value @val into the register specified in @reg.
 */
static void spi_wrreg8(struct sw_priv *priv, u8 reg, unsigned val)
{
	spi_wrreg_size(priv, reg, val, 1);
}

/**
 * ksz_rx_1msg - select whether to use one or two messages for spi read
 * @hw_priv:	The hardware device structure.
 *
 * Return whether to generate a single message with a tx and rx buffer
 * supplied to spi_sync(), or alternatively send the tx and rx buffers
 * as separate messages.
 *
 * Depending on the hardware in use, a single message may be more efficient
 * on interrupts or work done by the driver.
 *
 * This currently always returns false until we add some per-device data passed
 * from the platform code to specify which mode is better.
 */
static inline bool ksz_rx_1msg(struct spi_hw_priv *hw_priv)
{
	return hw_priv->rx_1msg;
}

/**
 * spi_rdreg - issue read register command and return the data
 * @priv:	The switch device structure.
 * @reg:	The register address.
 * @rxb:	The RX buffer to return the result into.
 * @rxl:	The length of data expected.
 *
 * This is the low level read call that issues the necessary spi message(s)
 * to read data from the register specified in @reg.
 */
static void spi_rdreg(struct sw_priv *priv, u8 reg, void *rxb, void **rx,
		      size_t rxl)
{
	struct spi_hw_priv *hw_priv = priv->hw_dev;
	struct spi_device *spi = hw_priv->spidev;
	u8 *txb = (u8 *) hw_priv->txd;
	struct spi_transfer *xfer;
	struct spi_message *msg;
	int ret;

#ifdef DEBUG
	if (!mutex_is_locked(&priv->lock))
		pr_alert("R not locked: %02X\n", reg);
#endif
	txb[0] = KS_SPIOP_RD;
	txb[1] = reg;

	if (rx)
		*rx = rxb;
	if (ksz_rx_1msg(hw_priv)) {
		msg = &hw_priv->spi_msg1;
		xfer = &hw_priv->spi_xfer1;

		xfer->tx_buf = hw_priv->txd;
		xfer->rx_buf = hw_priv->rxd;
		xfer->len = rxl + SPI_CMD_LEN;
		txb += SPI_CMD_LEN;
		memset(txb, 0, rxl);
		if (rx && rxb == hw_priv->rxd)
			*rx = (u8 *) rxb + SPI_CMD_LEN;
#if defined(CONFIG_SPI_PEGASUS) || defined(CONFIG_SPI_PEGASUS_MODULE)
		/*
		 * A hack to tell KSZ8692 SPI host controller the read command.
		 */
		memcpy(hw_priv->rxd, hw_priv->txd, SPI_CMD_LEN + 1);
		hw_priv->rxd[SPI_CMD_LEN] ^= 0xff;
#endif
	} else {
		msg = &hw_priv->spi_msg2;
		xfer = hw_priv->spi_xfer2;

		xfer->tx_buf = hw_priv->txd;
		xfer->rx_buf = NULL;
		xfer->len = SPI_CMD_LEN;

		xfer++;
		xfer->tx_buf = NULL;
		xfer->rx_buf = rxb;
		xfer->len = rxl;
	}

	ret = spi_sync(spi, msg);
	if (ret < 0)
		pr_alert("read: spi_sync() failed: %x %u\n", reg, rxl);
	else if (ksz_rx_1msg(hw_priv) && rxb != hw_priv->rxd)
		memcpy(rxb, hw_priv->rxd + SPI_CMD_LEN, rxl);
}

static void *spi_rdreg_size(struct sw_priv *priv, u8 reg, size_t size)
{
	struct spi_hw_priv *hw_priv = priv->hw_dev;
	void *r;

	spi_rdreg(priv, reg, hw_priv->rxd, &r, size);
	return r;
}

/**
 * spi_rdreg8 - read 8 bit register from device
 * @priv:	The switch device structure.
 * @reg:	The register address.
 *
 * Read a 8bit register from the chip, returning the result.
 */
static u8 spi_rdreg8(struct sw_priv *priv, u8 reg)
{
	u8 *rx;

	rx = spi_rdreg_size(priv, reg, 1);
	return rx[0];
}

/**
 * spi_rdreg16 - read 16 bit register from device
 * @priv:	The switch device structure.
 * @reg:	The register address.
 *
 * Read a 16bit register from the chip, returning the result.
 */
static u16 spi_rdreg16(struct sw_priv *priv, u8 reg)
{
	u16 *rx;

	rx = spi_rdreg_size(priv, reg, 2);
	return be16_to_cpu(*rx);
}

/**
 * spi_rdreg32 - read 32 bit register from device
 * @priv:	The switch device structure.
 * @reg:	The register address.
 *
 * Read a 32bit register from the chip.
 *
 * Note, this read requires the address be aligned to 4 bytes.
 */
static u32 spi_rdreg32(struct sw_priv *priv, u8 reg)
{
	u32 *rx;

	rx = spi_rdreg_size(priv, reg, 4);
	return be32_to_cpu(*rx);
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

static int rx_1msg;
static int spi_bus;

static int ksz8863_probe(struct spi_device *spi)
{
	struct spi_hw_priv *hw_priv;
	struct sw_priv *priv;

	spi->bits_per_word = 8;

	priv = kzalloc(sizeof(struct sw_priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->hw_dev = kzalloc(sizeof(struct spi_hw_priv), GFP_KERNEL);
	if (!priv->hw_dev) {
		kfree(priv);
		return -ENOMEM;
	}
	hw_priv = priv->hw_dev;

	hw_priv->rx_1msg = rx_1msg;
	hw_priv->spidev = spi;

	priv->dev = &spi->dev;

	priv->sw.reg = &sw_reg_ops;

	/* initialise pre-made spi transfer messages */

	spi_message_init(&hw_priv->spi_msg1);
	spi_message_add_tail(&hw_priv->spi_xfer1, &hw_priv->spi_msg1);

	spi_message_init(&hw_priv->spi_msg2);
	spi_message_add_tail(&hw_priv->spi_xfer2[0], &hw_priv->spi_msg2);
	spi_message_add_tail(&hw_priv->spi_xfer2[1], &hw_priv->spi_msg2);

	priv->irq = spi->irq;

	return ksz_probe(priv);
}

static int ksz8863_remove(struct spi_device *spi)
{
	struct sw_priv *priv = dev_get_drvdata(&spi->dev);

	return ksz_remove(priv);
}

static const struct of_device_id ksz8863_dt_ids[] = {
	{ .compatible = "microchip,ksz8863" },
	{ .compatible = "microchip,ksz8873" },
	{},
};
MODULE_DEVICE_TABLE(of, ksz8863_dt_ids);

static struct spi_driver ksz8863_driver = {
	.driver = {
		.name = KS8863MLI_DEV0,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(ksz8863_dt_ids),
	},
	.probe = ksz8863_probe,
	.remove = ksz8863_remove,
};

static int __init ksz8863_init(void)
{
#ifdef DEBUG_MSG
	if (init_dbg())
		return -ENOMEM;
#endif
	return spi_register_driver(&ksz8863_driver);
}

#ifndef CONFIG_KSZ_SWITCH_EMBEDDED
static void __exit ksz8863_exit(void)
#else
static void ksz8863_exit(void)
#endif
{
	spi_unregister_driver(&ksz8863_driver);
#ifdef DEBUG_MSG
	exit_dbg();
#endif
}

#ifndef CONFIG_KSZ_SWITCH_EMBEDDED
subsys_initcall(ksz8863_init);
module_exit(ksz8863_exit);
#endif

module_param(rx_1msg, int, 0);
MODULE_PARM_DESC(rx_1msg,
	"Configure whether receive one message is used");

module_param(spi_bus, int, 0);
MODULE_PARM_DESC(spi_bus,
	"Configure which spi master to use(0=KSZ8692, 2=FTDI)");

#ifndef CONFIG_KSZ_SWITCH_EMBEDDED
MODULE_DESCRIPTION("KSZ8863 SPI switch driver");
MODULE_AUTHOR("Tristram Ha <Tristram.Ha@microchip.com>");
MODULE_LICENSE("GPL");

MODULE_ALIAS("spi:ksz8863");
#endif
